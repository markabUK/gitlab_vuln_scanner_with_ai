#pragma once
#include "BaseEcosystemHandler.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <regex>

class DotNetHandler : public BaseEcosystemHandler {
private:
    std::shared_ptr<IDotNetParser> parser;
    std::shared_ptr<INuGetRegistry> registry;

    bool FileImportsDependency(const std::string& code, const DependencyChange& change) const {
        if (change.oldDep.name == "TargetFramework") return true;
        if (change.skipAI) return false;

        if (!change.oldDep.name.empty()) {
            std::string wordPattern = "\\b" + change.oldDep.name + "\\b";
            try {
                if (std::regex_search(code, std::regex(wordPattern, std::regex_constants::icase))) return true;
            } catch (...) {}
        }

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

    void ProcessBuildFiles(const ProjectContext& project, const std::string& localRepoPath, const std::string& branchName,
                           std::vector<DependencyChange>& masterChanges, std::vector<std::string>& modifiedBuildFiles) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(localRepoPath)) {
            if (!entry.is_regular_file()) continue;
            std::string pathStr = entry.path().string();
            std::string ext = entry.path().extension().string();

            if (pathStr.find("/bin/") != std::string::npos || pathStr.find("/obj/") != std::string::npos) continue;

            if (ext == ".csproj" || ext == ".fsproj") {
                std::ifstream in(pathStr);
                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

                auto dependencies = parser->ParseDependencies(content);
                std::string updatedContent = content;
                bool fileChanged = false;

                for (const auto& dep : dependencies) {
                    if (dep.name == "TargetFramework") {
                        if (dep.version.starts_with("net") && dep.version != "net10.0") {
                            Dependency newDep = {"Microsoft.NETCore.App", "TargetFramework", "net10.0"};
                            updatedContent = parser->UpdateDependencyVersion(updatedContent, dep, newDep);
                            masterChanges.push_back({dep, newDep, false, "", "", "Upgraded to .NET 10 LTS.", false});
                            fileChanged = true;
                            spdlog::info("[.NET] Framework Update found in {}: {} -> net10.0", entry.path().filename().string(), dep.version);
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
                        
                        std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();
                        spdlog::info("[.NET] NuGet Update found in {}: {} ({} -> {})", relPath, dep.name, dep.version, diff.newDep.version);
                    }
                }
                if (fileChanged && updatedContent != content) {
                    std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();
                    RecordChange(project, branchName, entry.path(), relPath, updatedContent, "chore: Update .NET dependencies in " + relPath);
                    modifiedBuildFiles.push_back(relPath);
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

            if (pathStr.find("/bin/") != std::string::npos || pathStr.find("/obj/") != std::string::npos) continue;

            if (ext == ".cs" || ext == ".fs") {
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
                combinedChange.oldDep = {"Multiple", "NuGet Packages", "Various"};
                combinedChange.newDep.version = "Various";
                combinedChange.releaseNotes = BuildStrictPromptInstructions("C#/F#", relevantChanges);

                spdlog::info("  -> AI analyzing .NET file {}...", relPath);
                RefactorRequest req = {relPath, baseCode, combinedChange, BuildCombinedContext(relevantChanges)};
                std::string rawAiCode = ai->RefactorCode(req);
                
                std::string workingCode = StringUtils::CleanAIOutput(rawAiCode, baseCode);
                workingCode = PostProcessCode(workingCode, relevantChanges);

                if (workingCode != baseCode && workingCode != "NO_CHANGES_NEEDED") {
                    RecordChange(project, branchName, entry.path(), relPath, workingCode, "refactor: AI updates for .NET upgrades");
                    modifiedSourceFiles.push_back(relPath);
                }
            }
        }
    }

public:
    DotNetHandler(
        std::shared_ptr<IGitLabClient> glClient,
        std::shared_ptr<IAICodeAssistant> aiAssistant,
        std::shared_ptr<IDotNetParser> dotnetParser,
        std::shared_ptr<INuGetRegistry> nugetRegistry,
        const std::vector<DependencyMigration>& migrations,
        bool dryRun = false)
        : BaseEcosystemHandler(glClient, aiAssistant, migrations, dryRun),
          parser(dotnetParser), registry(nugetRegistry) {}

    std::string GetEcosystemName() const override { return ".NET (C#/F#)"; }
    std::vector<std::string> GetTargetExtensions() const override { return {}; }

    std::string Process(const ProjectContext& project, const std::string& localRepoPath, const std::string& branchName) override {
        std::vector<DependencyChange> masterChanges;
        std::vector<std::string> modifiedBuildFiles;
        
        ProcessBuildFiles(project, localRepoPath, branchName, masterChanges, modifiedBuildFiles);
        if (masterChanges.empty()) return "";
        DeduplicateChanges(masterChanges);
        
        std::vector<std::string> modifiedSourceFiles;
        RefactorSourceFiles(project, localRepoPath, branchName, masterChanges, modifiedSourceFiles);

        return BuildMergeRequestDescription(masterChanges, modifiedSourceFiles, modifiedBuildFiles);
    }
};