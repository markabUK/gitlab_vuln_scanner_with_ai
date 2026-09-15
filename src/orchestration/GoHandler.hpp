#pragma once
#include "BaseEcosystemHandler.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <spdlog/spdlog.h>

class GoHandler : public BaseEcosystemHandler {
private:
    std::shared_ptr<IGoParser> parser;
    std::shared_ptr<IGoRegistry> registry;

    bool FileImportsDependency(const std::string& code, const DependencyChange& change) const {
        if (change.oldDep.name.empty() || change.skipAI) return false;
        if (code.find(change.oldDep.name) != std::string::npos) return true;

        for (const auto& m : migrations) {
            if (IsMigrationApplicable(m, change)) {
                if (!m.newGroup.empty() && code.find(m.newGroup) != std::string::npos) return true;
                if (!m.newName.empty() && code.find(m.newName) != std::string::npos) return true;
                for (const auto& rep : m.replacements) {
                    if (!rep.search.empty() && code.find(rep.search) != std::string::npos) return true;
                }
            }
        }
        return false;
    }

    void ProcessModFiles(const ProjectContext& project, const std::string& localRepoPath, const std::string& branchName,
                         std::vector<DependencyChange>& masterChanges, std::vector<std::string>& modifiedBuildFiles) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(localRepoPath)) {
            if (!entry.is_regular_file()) continue;
            std::string pathStr = entry.path().string();
            
            if (entry.path().filename() == "go.mod") {
                std::ifstream in(pathStr);
                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

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

                        std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();
                        spdlog::info("[Go] Update found in {}: {} ({} -> {})", relPath, dep.name, dep.version, diff.newDep.version);
                    }
                }
                if (fileChanged && updatedContent != content) {
                    std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();
                    RecordChange(project, branchName, entry.path(), relPath, updatedContent, "chore: Update Go dependencies in " + relPath);
                    modifiedBuildFiles.push_back(relPath);
                    
                    if (!isDryRun) {
                        spdlog::info("  [Go] Generating go.sum locally in {}...", entry.path().parent_path().string());
                        std::string cmd = "cd " + entry.path().parent_path().string() + " && go mod tidy > /dev/null 2>&1";
                        std::system(cmd.c_str());
                    }
                }
            }
        }
    }

    void RefactorSourceFiles(const ProjectContext& project, const std::string& localRepoPath, const std::string& branchName,
                             const std::vector<DependencyChange>& masterChanges, std::vector<std::string>& modifiedSourceFiles) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(localRepoPath)) {
            if (!entry.is_regular_file()) continue;
            std::string pathStr = entry.path().string();
            std::string ext = entry.path().extension().string();

            if (ext == ".go") {
                std::ifstream in(pathStr);
                std::string baseCode((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

                std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();

                std::vector<DependencyChange> relevantChanges;
                for (const auto& change : masterChanges) {
                    if (FileImportsDependency(baseCode, change)) {
                        relevantChanges.push_back(change);
                    }
                }

                if (relevantChanges.empty()) continue;

                DependencyChange combinedChange;
                combinedChange.oldDep = {"Multiple", "Go Modules", "Various"};
                combinedChange.newDep.version = "Various";
                combinedChange.releaseNotes = BuildStrictPromptInstructions("Go", relevantChanges);

                spdlog::info("  -> AI analyzing Go file {}...", relPath);
                RefactorRequest req = {relPath, baseCode, combinedChange, BuildCombinedContext(relevantChanges)};
                std::string rawAiCode = ai->RefactorCode(req);
                
                std::string workingCode = StringUtils::CleanAIOutput(rawAiCode, baseCode);
                workingCode = PostProcessCode(workingCode, relevantChanges);

                if (workingCode != baseCode && workingCode != "NO_CHANGES_NEEDED") {
                    RecordChange(project, branchName, entry.path(), relPath, workingCode, "refactor: AI updates for Go module upgrades");
                    modifiedSourceFiles.push_back(relPath);
                }
            }
        }
    }

public:
    GoHandler(
        std::shared_ptr<IGitLabClient> glClient,
        std::shared_ptr<IAICodeAssistant> aiAssistant,
        std::shared_ptr<IGoParser> goParser,
        std::shared_ptr<IGoRegistry> goRegistry,
        const std::vector<DependencyMigration>& migrations,
        bool dryRun = false)
        : BaseEcosystemHandler(glClient, aiAssistant, migrations, dryRun),
          parser(goParser), registry(goRegistry) {}

    std::string GetEcosystemName() const override { return "Go Modules"; }
    std::vector<std::string> GetTargetExtensions() const override { return {}; }

    std::string Process(const ProjectContext& project, const std::string& localRepoPath, const std::string& branchName) override {
        std::vector<DependencyChange> masterChanges;
        std::vector<std::string> modifiedBuildFiles;
        
        ProcessModFiles(project, localRepoPath, branchName, masterChanges, modifiedBuildFiles);
        if (masterChanges.empty()) return "";
        DeduplicateChanges(masterChanges);
        
        std::vector<std::string> modifiedSourceFiles;
        RefactorSourceFiles(project, localRepoPath, branchName, masterChanges, modifiedSourceFiles);

        return BuildMergeRequestDescription(masterChanges, modifiedSourceFiles, modifiedBuildFiles);
    }
};