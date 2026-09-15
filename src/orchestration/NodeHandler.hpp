#pragma once
#include "BaseEcosystemHandler.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <regex>
#include <spdlog/spdlog.h>

class NodeHandler : public BaseEcosystemHandler {
private:
    std::shared_ptr<INpmParser> parser;
    std::shared_ptr<INpmRegistry> registry;

    bool FileImportsDependency(const std::string& code, const DependencyChange& change) const {
        if (change.oldDep.name.empty() || change.oldDep.name == "version" || change.skipAI) return false;
        
        std::regex reqRegex("require\\s*\\(\\s*['\"]" + change.oldDep.name + "['\"]\\s*\\)");
        if (std::regex_search(code, reqRegex)) return true;

        std::regex impRegex("from\\s+['\"]" + change.oldDep.name + "['\"]");
        if (std::regex_search(code, impRegex)) return true;

        std::string wordPattern = "\\b" + change.oldDep.name + "\\b";
        try {
            if (std::regex_search(code, std::regex(wordPattern))) return true;
        } catch (...) {}

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

    void ProcessPackageFiles(const ProjectContext& project, const std::string& localRepoPath, const std::string& branchName,
                             std::vector<DependencyChange>& masterChanges, std::vector<std::string>& modifiedBuildFiles) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(localRepoPath)) {
            if (!entry.is_regular_file()) continue;
            std::string pathStr = entry.path().string();
            
            if (pathStr.find("node_modules") != std::string::npos) continue;

            if (entry.path().filename() == "package.json") {
                std::ifstream in(pathStr);
                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

                auto dependencies = parser->ParseDependencies(content);
                std::string updatedContent = content;
                bool fileChanged = false;

                for (const auto& dep : dependencies) {
                    if (dep.name == "version") continue; 

                    auto latestOpt = registry->GetLatestVersion(dep);
                    if (latestOpt && *latestOpt != dep.version) {
                        Dependency newDep = {dep.group, dep.name, *latestOpt};
                        DependencyChange diff = registry->InspectVersionDiff(dep, newDep);
                        updatedContent = parser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                        masterChanges.push_back(diff);
                        fileChanged = true;
                        
                        std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();
                        spdlog::info("[Node] Update found in {}: {} ({} -> {})", relPath, dep.name, dep.version, diff.newDep.version);
                    }
                }
                if (fileChanged && updatedContent != content) {
                    std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();
                    RecordChange(project, branchName, entry.path(), relPath, updatedContent, "chore: Update Node dependencies in " + relPath);
                    modifiedBuildFiles.push_back(relPath);
                    
                    if (!isDryRun) {
                        spdlog::info("  [Node] Generating package-lock.json locally in {}...", entry.path().parent_path().string());
                        std::string cmd = "cd " + entry.path().parent_path().string() + " && npm install --package-lock-only --ignore-scripts > /dev/null 2>&1";
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

            if (pathStr.find("node_modules") != std::string::npos || pathStr.find("/dist/") != std::string::npos || pathStr.find("/build/") != std::string::npos) continue;
            if (pathStr.find(".min.js") != std::string::npos || pathStr.find(".bundle.js") != std::string::npos) continue;

            if (ext == ".js" || ext == ".ts" || ext == ".jsx" || ext == ".tsx" || ext == ".mjs") {
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
                combinedChange.oldDep = {"Multiple", "NPM Packages", "Various"};
                combinedChange.newDep.version = "Various";
                combinedChange.releaseNotes = BuildStrictPromptInstructions("JavaScript/TypeScript", relevantChanges);

                spdlog::info("  -> AI analyzing Node file {}...", relPath);
                RefactorRequest req = {relPath, baseCode, combinedChange, BuildCombinedContext(relevantChanges)};
                std::string rawAiCode = ai->RefactorCode(req);
                
                std::string workingCode = StringUtils::CleanAIOutput(rawAiCode, baseCode);
                workingCode = PostProcessCode(workingCode, relevantChanges);

                if (workingCode != baseCode && workingCode != "NO_CHANGES_NEEDED") {
                    RecordChange(project, branchName, entry.path(), relPath, workingCode, "refactor: AI updates for Node dependency upgrades");
                    modifiedSourceFiles.push_back(relPath);
                }
            }
        }
    }

public:
    NodeHandler(
        std::shared_ptr<IGitLabClient> glClient,
        std::shared_ptr<IAICodeAssistant> aiAssistant,
        std::shared_ptr<INpmParser> npmParser,
        std::shared_ptr<INpmRegistry> npmRegistry,
        const std::vector<DependencyMigration>& migrations,
        bool dryRun = false)
        : BaseEcosystemHandler(glClient, aiAssistant, migrations, dryRun),
          parser(npmParser), registry(npmRegistry) {}

    std::string GetEcosystemName() const override { return "Node.js (JS/TS)"; }
    std::vector<std::string> GetTargetExtensions() const override { return {}; }

    std::string Process(const ProjectContext& project, const std::string& localRepoPath, const std::string& branchName) override {
        std::vector<DependencyChange> masterChanges;
        std::vector<std::string> modifiedBuildFiles;
        
        ProcessPackageFiles(project, localRepoPath, branchName, masterChanges, modifiedBuildFiles);
        if (masterChanges.empty()) return "";
        DeduplicateChanges(masterChanges);
        
        std::vector<std::string> modifiedSourceFiles;
        RefactorSourceFiles(project, localRepoPath, branchName, masterChanges, modifiedSourceFiles);

        return BuildMergeRequestDescription(masterChanges, modifiedSourceFiles, modifiedBuildFiles);
    }
};