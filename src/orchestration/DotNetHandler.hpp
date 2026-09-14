#pragma once

#include "BaseEcosystemHandler.hpp"

class DotNetHandler : public BaseEcosystemHandler {
private:
    std::shared_ptr<IDotNetParser> parser;
    std::shared_ptr<INuGetRegistry> registry;

    bool FileImportsDependency(const std::string& code, const DependencyChange& change) const {
        if (change.oldDep.name == "TargetFramework") return true;
        if (code.find(change.oldDep.name) != std::string::npos) return true;
        return false;
    }

    void EnsureBranchExists(const std::string& projectId, const std::string& branchName, const std::string& defaultBranch, bool& branchCreated) const {
        if (!branchCreated) {
            gitlab->CreateBranch(projectId, branchName, defaultBranch);
            branchCreated = true;
        }
    }

    std::vector<std::string> GetTargetProjects(const ProjectContext& project, const std::vector<std::string>& repoFiles, std::vector<std::string>& sourceFiles, std::string& slnxFilePath) const {
        std::vector<std::string> dotnetProjectFiles;

        for (const auto& file : repoFiles) {
            if (file.ends_with(".slnx")) {
                slnxFilePath = file;
            } else if (file.ends_with(".csproj") || file.ends_with(".fsproj")) {
                dotnetProjectFiles.push_back(file);
            } else if (file.ends_with(".cs") || file.ends_with(".fs")) {
                sourceFiles.push_back(file);
            }
        }

        if (!slnxFilePath.empty()) {
            std::cout << "[.NET] Detected .slnx solution: " << slnxFilePath << "\n";
            std::string slnxContent = gitlab->FetchFileContent(project.projectId, slnxFilePath, project.defaultBranch);
            return parser->ParseSlnxProjects(slnxContent);
        } else if (!dotnetProjectFiles.empty()) {
            std::cout << "[.NET] No .slnx found. Managing discovered .csproj files independently.\n";
            return dotnetProjectFiles;
        } 
        
        return {};
    }

    void ProcessBuildFiles(const ProjectContext& project, const std::string& branchName, const std::vector<std::string>& targetProjects, std::vector<DependencyChange>& masterChanges, std::vector<std::string>& modifiedBuildFiles, bool& branchCreated) {
        for (const auto& projPath : targetProjects) {
            std::string content = gitlab->FetchFileContent(project.projectId, projPath, project.defaultBranch);
            if (content.empty()) continue;

            auto dependencies = parser->ParseDependencies(content);
            std::string updatedContent = content;
            bool fileChanged = false;

            for (const auto& dep : dependencies) {
                if (dep.name == "TargetFramework") {
                    if (dep.version.starts_with("net") && dep.version != "net10.0") {
                        Dependency newDep = {"Microsoft.NETCore.App", "TargetFramework", "net10.0"};
                        updatedContent = parser->UpdateDependencyVersion(updatedContent, dep, newDep);
                        
                        DependencyChange diff = {dep, newDep, false, "", "", "Upgraded to .NET 10 LTS.", false};
                        masterChanges.push_back(diff);
                        fileChanged = true;
                        std::cout << "[.NET] Framework Update found in " << projPath << ": " << dep.version << " -> net10.0\n";
                    }
                    continue;
                }

                auto latestOpt = registry->GetLatestVersion(dep);
                if (latestOpt && *latestOpt != dep.version) {
                    Dependency newDep = {dep.group, dep.name, *latestOpt};
                    DependencyChange diff = registry->InspectVersionDiff(dep, newDep);
                    
                    updatedContent = parser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                    masterChanges.push_back(diff);
                    fileChanged = true;
                    std::cout << "[.NET] NuGet Update found in " << projPath << ": " << dep.name << " (" << dep.version << " -> " << diff.newDep.version << ")\n";
                }
            }

            if (fileChanged && updatedContent != content) {
                EnsureBranchExists(project.projectId, branchName, project.defaultBranch, branchCreated);
                gitlab->CommitFile(project.projectId, branchName, projPath, updatedContent, "chore: Update .NET dependencies in " + projPath);
                modifiedBuildFiles.push_back(projPath);
            }
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
            combinedChange.oldDep = {"Multiple", "NuGet Packages", "Various"};
            combinedChange.newDep.version = "Various";
            
            std::string combinedNotes = "TASK: Refactor C#/F# code to be compatible with updated dependencies:\n";
            for (const auto& c : relevantChanges) {
                if (c.oldDep.name == "TargetFramework") {
                    combinedNotes += "- TargetFramework upgraded to .NET 10. Refactor syntax to utilize modern C# language features if applicable.\n";
                } else {
                    combinedNotes += "- " + c.oldDep.name + " updated to " + c.newDep.version + "\n";
                }
            }
            
            combinedNotes += "\nOUTPUT FORMAT: Return ONLY the raw updated source code. DO NOT wrap in markdown blocks.";
            combinedChange.releaseNotes = combinedNotes;

            std::cout << " -> AI analyzing .NET file " << filePath << "...\n";
            RefactorRequest req = {filePath, baseCode, combinedChange, BuildCombinedContext(relevantChanges)};
            std::string rawAiCode = ai->RefactorCode(req);
            
            std::string workingCode = StringUtils::CleanAIOutput(rawAiCode, baseCode);
            workingCode = PostProcessCode(workingCode, relevantChanges);

            if (workingCode != baseCode) {
                gitlab->CommitFile(project.projectId, branchName, filePath, workingCode, "refactor: AI updates for .NET upgrades");
                modifiedSourceFiles.push_back(filePath);
            }
        }
    }

public:
    DotNetHandler(
        std::shared_ptr<IGitLabClient> glClient,
        std::shared_ptr<IAICodeAssistant> aiAssistant,
        std::shared_ptr<IDotNetParser> dotnetParser,
        std::shared_ptr<INuGetRegistry> nugetRegistry,
        const std::vector<DependencyMigration>& migrations)
        : BaseEcosystemHandler(glClient, aiAssistant, migrations), 
          parser(dotnetParser), registry(nugetRegistry) {}

    std::string GetEcosystemName() const override { return ".NET (C#/F#)"; }

    std::vector<std::string> GetTargetExtensions() const override {
        return {".slnx", ".csproj", ".fsproj", ".cs", ".fs"};
    }

    void Process(const ProjectContext& project, const std::vector<std::string>& repoFiles) override {
        std::string slnxFilePath = "";
        std::vector<std::string> sourceFiles;
        std::vector<std::string> targetProjects = GetTargetProjects(project, repoFiles, sourceFiles, slnxFilePath);

        if (targetProjects.empty()) return;

        std::vector<DependencyChange> masterChanges;
        std::vector<std::string> modifiedBuildFiles;
        std::string branchName = GenerateBranchName("dotnet");
        bool branchCreated = false;

        ProcessBuildFiles(project, branchName, targetProjects, masterChanges, modifiedBuildFiles, branchCreated);

        if (masterChanges.empty() || !branchCreated) return;
        DeduplicateChanges(masterChanges);
        
        std::vector<std::string> modifiedSourceFiles;
        RefactorSourceFiles(project, branchName, sourceFiles, masterChanges, modifiedSourceFiles);

        if (!slnxFilePath.empty()) modifiedBuildFiles.push_back(slnxFilePath + " (Scanned via Solution)");
        
        std::string mrDescription = BuildMergeRequestDescription(masterChanges, modifiedSourceFiles, modifiedBuildFiles);
        gitlab->CreateMergeRequest(project.projectId, branchName, project.defaultBranch, "chore: Automated .NET Dependency Update", mrDescription);
    }
};