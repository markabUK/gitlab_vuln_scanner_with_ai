#pragma once

#include "BaseEcosystemHandler.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <map>
#include <spdlog/spdlog.h>

class GoHandler : public BaseEcosystemHandler {
private:
    std::shared_ptr<IGoParser> parser;
    std::shared_ptr<IGoRegistry> registry;

    bool FileImportsDependency(const std::string& code, const DependencyChange& change) const {
        return code.find(change.oldDep.name) != std::string::npos;
    }

    void EnsureBranchExists(const std::string& projectId, const std::string& branchName, const std::string& defaultBranch, bool& branchCreated) const {
        if (!branchCreated) {
            gitlab->CreateBranch(projectId, branchName, defaultBranch);
            branchCreated = true;
        }
    }

    void CategorizeFiles(const std::vector<std::string>& repoFiles, std::vector<std::string>& modFiles, std::vector<std::string>& sourceFiles) const {
        for (const auto& file : repoFiles) {
            if (file.ends_with("go.mod")) {
                modFiles.push_back(file);
            } else if (file.ends_with(".go")) {
                sourceFiles.push_back(file);
            }
        }
    }

    void ProcessModFiles(const ProjectContext& project, const std::string& branchName, const std::vector<std::string>& modFiles, std::vector<DependencyChange>& masterChanges, std::vector<std::string>& modifiedBuildFiles, std::map<std::string, std::string>& pendingLockfileTargets, bool& branchCreated) {
        for (const auto& modFilePath : modFiles) {
            std::string content = gitlab->FetchFileContent(project.projectId, modFilePath, project.defaultBranch);
            if (content.empty()) continue;

            auto dependencies = parser->ParseDependencies(content);
            std::string updatedContent = content;
            bool fileChanged = false;

            for (const auto& dep : dependencies) {
                auto latestOpt = registry->GetLatestVersion(dep);
                if (latestOpt && *latestOpt != dep.version) {
                    Dependency newDep = {dep.group, dep.name, *latestOpt};
                    DependencyChange diff = registry->InspectVersionDiff(dep, newDep);
                    
                    updatedContent = parser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                    masterChanges.push_back(diff);
                    fileChanged = true;
                    
                    spdlog::info("[Go] Update found in {}: {} ({} -> {})", modFilePath, dep.name, dep.version, diff.newDep.version);
                }
            }

            if (fileChanged && updatedContent != content) {
                EnsureBranchExists(project.projectId, branchName, project.defaultBranch, branchCreated);
                gitlab->CommitFile(project.projectId, branchName, modFilePath, updatedContent, "chore: Update Go dependencies in " + modFilePath);
                modifiedBuildFiles.push_back(modFilePath);
                pendingLockfileTargets[modFilePath] = updatedContent;
            }
        }
    }

    void SyncLockfiles(const std::string& projectId, const std::string& branchName, const std::map<std::string, std::string>& pendingLockfileTargets, std::vector<std::string>& modifiedBuildFiles) {
        auto generatedLockfiles = GenerateLockfiles(pendingLockfileTargets);
        for (const auto& [lockPath, lockContent] : generatedLockfiles) {
            gitlab->CommitFile(projectId, branchName, lockPath, lockContent, "chore: Sync go.sum");
            modifiedBuildFiles.push_back(lockPath);
        }
    }

    void RefactorSourceFiles(const ProjectContext& project, const std::string& branchName, const std::vector<std::string>& sourceFiles, const std::vector<DependencyChange>& masterChanges, std::vector<std::string>& modifiedSourceFiles) {
        for (const auto& filePath : sourceFiles) {
            std::string baseCode = gitlab->FetchFileContent(project.projectId, filePath, project.defaultBranch);
            if (baseCode.empty()) continue;

            std::vector<DependencyChange> relevantChanges;
            for (const auto& change : masterChanges) {
                if (FileImportsDependency(baseCode, change) && !change.skipAI) {
                    relevantChanges.push_back(change);
                }
            }
            if (relevantChanges.empty()) continue;

            DependencyChange combinedChange;
            combinedChange.oldDep = {"Multiple", "Go Modules", "Various"};
            combinedChange.newDep.version = "Various";
            
            std::string combinedNotes = "TASK: Refactor Go code to be compatible with updated dependencies:\n";
            for (const auto& c : relevantChanges) {
                combinedNotes += "- " + c.oldDep.name + " updated to " + c.newDep.version + "\n";
            }
            
            combinedNotes += "\nOUTPUT FORMAT: Return ONLY the raw updated source code. DO NOT wrap in markdown blocks.";
            combinedChange.releaseNotes = combinedNotes;

            spdlog::info(" -> AI analyzing Go file {}...", filePath);
            RefactorRequest req = {filePath, baseCode, combinedChange, BuildCombinedContext(relevantChanges)};
            std::string rawAiCode = ai->RefactorCode(req);
            
            std::string workingCode = StringUtils::CleanAIOutput(rawAiCode, baseCode);
            workingCode = PostProcessCode(workingCode, relevantChanges);

            if (workingCode != baseCode) {
                gitlab->CommitFile(project.projectId, branchName, filePath, workingCode, "refactor: AI updates for Go module upgrades");
                modifiedSourceFiles.push_back(filePath);
            }
        }
    }

public:
    GoHandler(
        std::shared_ptr<IGitLabClient> glClient,
        std::shared_ptr<IAICodeAssistant> aiAssistant,
        std::shared_ptr<IGoParser> goParser,
        std::shared_ptr<IGoRegistry> goRegistry,
        const std::vector<DependencyMigration>& migrations)
        : BaseEcosystemHandler(glClient, aiAssistant, migrations), 
          parser(goParser), registry(goRegistry) {}

    std::string GetEcosystemName() const override { return "Go Modules"; }

    std::vector<std::string> GetTargetExtensions() const override {
        return {"go.mod", ".go"};
    }

    std::map<std::string, std::string> GenerateLockfiles(const std::map<std::string, std::string>& modifiedBuildFiles) const override {
        std::map<std::string, std::string> lockfiles;
        
        for (const auto& [filePath, newContent] : modifiedBuildFiles) {
            if (filePath.find("go.mod") != std::string::npos) {
                std::string tmpDir = "/tmp/deps_bot_go_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                std::filesystem::create_directories(tmpDir);

                std::ofstream outMod(tmpDir + "/go.mod");
                outMod << newContent;
                outMod.close();

                spdlog::info("  [Go] Generating go.sum locally...");
                std::string cmd = "cd " + tmpDir + " && go mod tidy > /dev/null 2>&1";
                int result = std::system(cmd.c_str());

                if (result == 0 && std::filesystem::exists(tmpDir + "/go.sum")) {
                    std::ifstream lockFile(tmpDir + "/go.sum");
                    std::stringstream buffer;
                    buffer << lockFile.rdbuf();
                    
                    std::string lockfilePath = filePath;
                    size_t pos = lockfilePath.rfind("go.mod");
                    if (pos != std::string::npos) {
                        lockfilePath.replace(pos, 6, "go.sum");
                    } else {
                        lockfilePath = "go.sum";
                    }
                    lockfiles[lockfilePath] = buffer.str();
                }

                std::filesystem::remove_all(tmpDir);
            }
        }
        return lockfiles;
    }

    void Process(const ProjectContext& project, const std::vector<std::string>& repoFiles) override {
        std::vector<std::string> modFiles, sourceFiles;
        CategorizeFiles(repoFiles, modFiles, sourceFiles);

        if (modFiles.empty()) return;

        std::vector<DependencyChange> masterChanges;
        std::vector<std::string> modifiedBuildFiles;
        std::map<std::string, std::string> pendingLockfileTargets;
        std::string branchName = GenerateBranchName("go");
        bool branchCreated = false;

        ProcessModFiles(project, branchName, modFiles, masterChanges, modifiedBuildFiles, pendingLockfileTargets, branchCreated);

        if (masterChanges.empty() || !branchCreated) return;
        DeduplicateChanges(masterChanges);
        
        SyncLockfiles(project.projectId, branchName, pendingLockfileTargets, modifiedBuildFiles);

        std::vector<std::string> modifiedSourceFiles;
        RefactorSourceFiles(project, branchName, sourceFiles, masterChanges, modifiedSourceFiles);

        std::string mrDescription = BuildMergeRequestDescription(masterChanges, modifiedSourceFiles, modifiedBuildFiles);
        mrDescription += "\n\n> **Note:** Please verify `go test ./...` locally.";
        gitlab->CreateMergeRequest(project.projectId, branchName, project.defaultBranch, "chore: Automated Go Modules Update", mrDescription);
    }
};