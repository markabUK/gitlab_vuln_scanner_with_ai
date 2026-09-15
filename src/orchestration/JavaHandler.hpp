#pragma once
#include "BaseEcosystemHandler.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <regex>

class JavaHandler : public BaseEcosystemHandler {
private:
    std::shared_ptr<IGradleParser> gradleParser;
    std::shared_ptr<IPomParser> pomParser;
    std::shared_ptr<IAntParser> antParser;
    std::shared_ptr<IMavenRegistry> registry;

    bool FileImportsDependency(const std::string& code, const DependencyChange& change) const {
        if (change.skipAI) return false;

        if (!change.oldDep.name.empty()) {
            std::string wordPattern = "\\b" + change.oldDep.name + "\\b";
            try {
                if (std::regex_search(code, std::regex(wordPattern, std::regex_constants::icase))) return true;
            } catch (...) {}
        }

        if (!change.oldDep.group.empty() && code.find(change.oldDep.group) != std::string::npos) {
            return true;
        }

        for (const auto& m : migrations) {
            if (IsMigrationApplicable(m, change)) {
                if (!m.newGroup.empty() && code.find(m.newGroup) != std::string::npos) return true;
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
            std::string filename = entry.path().filename().string();
            std::string ext = entry.path().extension().string();

            if (pathStr.find("/build/") != std::string::npos || pathStr.find("/.gradle/") != std::string::npos || pathStr.find("/target/") != std::string::npos) continue;

            if (ext == ".kts" || filename == "build.gradle" || filename == "pom.xml" || filename == "build.xml" || filename == "ivy.xml") {
                std::ifstream in(pathStr);
                std::string buildContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

                std::vector<Dependency> dependencies;
                if (filename == "pom.xml") dependencies = pomParser->ParseDependencies(buildContent);
                else if (filename == "build.xml" || filename == "ivy.xml") dependencies = antParser->ParseDependencies(buildContent);
                else dependencies = gradleParser->ParseDependencies(buildContent);

                std::string updatedContent = buildContent;
                bool fileChanged = false;

                for (const auto& dep : dependencies) {
                    auto latestOpt = registry->GetLatestVersion(dep);
                    if (latestOpt && *latestOpt != dep.version) {
                        Dependency newDep = {dep.group, dep.name, *latestOpt};
                        DependencyChange diff = registry->InspectVersionDiff(dep, newDep);
                        
                        if (filename == "pom.xml") updatedContent = pomParser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                        else if (filename == "build.xml" || filename == "ivy.xml") updatedContent = antParser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                        else updatedContent = gradleParser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                        
                        masterChanges.push_back(diff);
                        fileChanged = true;
                        std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();
                        spdlog::info("[Java] Update found in {}: {} ({} -> {})", relPath, dep.name, dep.version, diff.newDep.version);
                    }
                }
                if (fileChanged && updatedContent != buildContent) {
                    std::string relPath = std::filesystem::relative(entry.path(), localRepoPath).string();
                    RecordChange(project, branchName, entry.path(), relPath, updatedContent, "chore: Update Java dependencies in " + relPath);
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

            if (pathStr.find("/build/") != std::string::npos || pathStr.find("/.gradle/") != std::string::npos || pathStr.find("/target/") != std::string::npos) continue;

            if (ext == ".java" || ext == ".kt") {
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
                combinedChange.oldDep = {"Multiple", "Dependencies", "Various"};
                combinedChange.newDep.version = "Various";
                combinedChange.releaseNotes = BuildStrictPromptInstructions("Java/Kotlin", relevantChanges);

                spdlog::info("  -> AI analyzing Java file {}...", relPath);
                RefactorRequest req = {relPath, baseCode, combinedChange, BuildCombinedContext(relevantChanges)};
                std::string rawAiCode = ai->RefactorCode(req);
                
                std::string workingCode = StringUtils::CleanAIOutput(rawAiCode, baseCode);
                workingCode = PostProcessCode(workingCode, relevantChanges);

                if (workingCode != baseCode && workingCode != "NO_CHANGES_NEEDED") {
                    RecordChange(project, branchName, entry.path(), relPath, workingCode, "refactor: AI updates for Java dependency upgrades");
                    modifiedSourceFiles.push_back(relPath);
                }
            }
        }
    }

public:
    JavaHandler(
        std::shared_ptr<IGitLabClient> glClient,
        std::shared_ptr<IAICodeAssistant> aiAssistant,
        std::shared_ptr<IGradleParser> gParser,
        std::shared_ptr<IPomParser> pParser,
        std::shared_ptr<IAntParser> aParser,
        std::shared_ptr<IMavenRegistry> mavenRegistry,
        const std::vector<DependencyMigration>& migrations,
        bool dryRun = false)
        : BaseEcosystemHandler(glClient, aiAssistant, migrations, dryRun),
          gradleParser(gParser), pomParser(pParser), antParser(aParser), registry(mavenRegistry) {}

    std::string GetEcosystemName() const override { return "Java/Kotlin (Gradle, Maven, Ant)"; }
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