#pragma once

#include "BaseEcosystemHandler.hpp"

class GoHandler : public BaseEcosystemHandler {
private:
    std::shared_ptr<IGoParser> parser;
    std::shared_ptr<IGoRegistry> registry;

    bool FileImportsDependency(const std::string& code, const DependencyChange& change) const {
        // Go import paths are directly mapped to the dependency name
        return code.find(change.oldDep.name) != std::string::npos;
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

    std::string GetEcosystemName() const override {
        return "Go";
    }

    std::vector<std::string> GetTargetExtensions() const override {
        return {"go.mod", ".go"};
    }

    void Process(const ProjectContext& project, const std::vector<std::string>& repoFiles) override {
        std::vector<std::string> modFiles, sourceFiles;
        
        for (const auto& file : repoFiles) {
            if (file.ends_with("go.mod")) {
                modFiles.push_back(file);
            } else if (file.ends_with(".go")) {
                sourceFiles.push_back(file);
            }
        }

        if (modFiles.empty()) return;

        std::vector<DependencyChange> masterChanges;
        std::vector<std::string> modifiedBuildFiles;
        
        std::string branchName = GenerateBranchName("go");
        bool branchCreated = false;

        for (const auto& modFilePath : modFiles) {
            std::string modContent = gitlab->FetchFileContent(project.projectId, modFilePath, project.defaultBranch);
            if (modContent.empty()) continue;

            auto dependencies = parser->ParseDependencies(modContent);
            std::string updatedContent = modContent;
            bool fileChanged = false;

            for (const auto& dep : dependencies) {
                auto latestOpt = registry->GetLatestVersion(dep);
                if (latestOpt && *latestOpt != dep.version) {
                    Dependency newDep = {dep.group, dep.name, *latestOpt};
                    DependencyChange diff = registry->InspectVersionDiff(dep, newDep);
                    
                    updatedContent = parser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                    masterChanges.push_back(diff);
                    fileChanged = true;
                    std::cout << "[Go] Update found in " << modFilePath << ": " << dep.name << " (" << dep.version << " -> " << diff.newDep.version << ")\n";
                }
            }

            if (fileChanged && updatedContent != modContent) {
                if (!branchCreated) {
                    try { gitlab->CreateBranch(project.projectId, branchName, project.defaultBranch); branchCreated = true; } 
                    catch (...) { return; }
                }
                gitlab->CommitFile(project.projectId, branchName, modFilePath, updatedContent, "chore: Update Go modules in " + modFilePath);
                modifiedBuildFiles.push_back(modFilePath);
            }
        }

        if (masterChanges.empty() || !branchCreated) return;
        DeduplicateChanges(masterChanges);
        
        std::vector<std::string> modifiedSourceFiles;

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

            std::cout << " -> AI analyzing Go file " << filePath << "...\n";
            RefactorRequest req = {filePath, baseCode, combinedChange, ""};
            std::string rawAiCode = ai->RefactorCode(req);
            
            std::string workingCode = StringUtils::CleanAIOutput(rawAiCode, baseCode);
            workingCode = PostProcessCode(workingCode, relevantChanges);

            if (workingCode != baseCode) {
                gitlab->CommitFile(project.projectId, branchName, filePath, workingCode, "refactor: AI updates for Go module upgrades");
                modifiedSourceFiles.push_back(filePath);
            }
        }

        std::string mrDescription = BuildMergeRequestDescription(masterChanges, modifiedSourceFiles, modifiedBuildFiles);
        mrDescription += "\n\n> **Note:** Please run `go mod tidy` and `go test ./...` locally before merging to ensure `go.sum` is updated and tests pass.";
        
        gitlab->CreateMergeRequest(project.projectId, branchName, project.defaultBranch, "chore: Automated Go Dependency Update", mrDescription);
    }
};