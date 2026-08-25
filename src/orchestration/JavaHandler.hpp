#pragma once

#include "BaseEcosystemHandler.hpp"

class JavaHandler : public BaseEcosystemHandler {
private:
    std::shared_ptr<IGradleParser> gradleParser;
    std::shared_ptr<IPomParser> pomParser;
    std::shared_ptr<IAntParser> antParser;
    std::shared_ptr<IMavenRegistry> registry;

    bool FileImportsDependency(const std::string& code, const DependencyChange& change) const {
        if (code.find(change.oldDep.group) != std::string::npos) return true;
        if (code.find(change.oldDep.name) != std::string::npos) return true;
        if (change.oldDep.group == "junit" && code.find("org.junit") != std::string::npos) return true;
        if (change.oldDep.name == "guava" && code.find("com.google.common") != std::string::npos) return true;
        return false;
    }

public:
    JavaHandler(
        std::shared_ptr<IGitLabClient> glClient,
        std::shared_ptr<IAICodeAssistant> aiAssistant,
        std::shared_ptr<IGradleParser> gParser,
        std::shared_ptr<IPomParser> pParser,
        std::shared_ptr<IAntParser> aParser,
        std::shared_ptr<IMavenRegistry> mavenRegistry,
        const std::vector<DependencyMigration>& migrations)
        : BaseEcosystemHandler(glClient, aiAssistant, migrations), 
          gradleParser(gParser), pomParser(pParser), antParser(aParser), registry(mavenRegistry) {}

    std::string GetEcosystemName() const override {
        return "Java/Kotlin (Gradle, Maven, Ant)";
    }

    std::vector<std::string> GetTargetExtensions() const override {
        return {".java", ".kt", ".kts", "build.gradle", "gradle.properties", "pom.xml", "build.xml", "ivy.xml"};
    }

    void Process(const ProjectContext& project, const std::vector<std::string>& repoFiles) override {
        std::vector<std::string> buildFiles, sourceFiles;
        
        for (const auto& file : repoFiles) {
            if (file.ends_with(".java") || file.ends_with(".kt")) {
                sourceFiles.push_back(file);
            } else {
                buildFiles.push_back(file); // Everything else is a targeted build/config file
            }
        }

        if (buildFiles.empty()) return;

        std::vector<DependencyChange> masterChanges;
        std::vector<std::string> modifiedBuildFiles;
        
        std::string branchName = GenerateBranchName("java");
        bool branchCreated = false;

        for (const auto& buildFilePath : buildFiles) {
            std::string buildContent = gitlab->FetchFileContent(project.projectId, buildFilePath, project.defaultBranch);
            if (buildContent.empty()) continue;

            std::vector<Dependency> dependencies;
            if (buildFilePath.ends_with("pom.xml")) {
                dependencies = pomParser->ParseDependencies(buildContent);
            } else if (buildFilePath.ends_with("ivy.xml") || buildFilePath.ends_with("build.xml")) {
                dependencies = antParser->ParseDependencies(buildContent);
            } else {
                dependencies = gradleParser->ParseDependencies(buildContent);
            }

            std::string updatedContent = buildContent;
            bool fileChanged = false;

            for (const auto& dep : dependencies) {
                auto latestOpt = registry->GetLatestVersion(dep);
                if (latestOpt && *latestOpt != dep.version) {
                    Dependency newDep = {dep.group, dep.name, *latestOpt};
                    DependencyChange diff = registry->InspectVersionDiff(dep, newDep);
                    
                    if (buildFilePath.ends_with("pom.xml")) {
                        updatedContent = pomParser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                    } else if (buildFilePath.ends_with("ivy.xml") || buildFilePath.ends_with("build.xml")) {
                        updatedContent = antParser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                    } else {
                        updatedContent = gradleParser->UpdateDependencyVersion(updatedContent, dep, diff.newDep);
                    }

                    masterChanges.push_back(diff);
                    fileChanged = true;
                    std::cout << "[Java] Update found in " << buildFilePath << ": " << dep.name << " (" << dep.version << " -> " << diff.newDep.version << ")\n";
                }
            }

            if (fileChanged && updatedContent != buildContent) {
                if (!branchCreated) {
                    try { gitlab->CreateBranch(project.projectId, branchName, project.defaultBranch); branchCreated = true; } 
                    catch (...) { return; }
                }
                gitlab->CommitFile(project.projectId, branchName, buildFilePath, updatedContent, "chore: Update Java dependencies in " + buildFilePath);
                modifiedBuildFiles.push_back(buildFilePath);
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
            combinedChange.oldDep = {"Multiple", "Dependencies", "Various"};
            combinedChange.newDep.version = "Various";
            
            std::string combinedNotes = "TASK: Refactor Java/Kotlin code to be compatible with updated dependencies:\n";
            for (const auto& c : relevantChanges) {
                combinedNotes += "- " + c.oldDep.group + ":" + c.oldDep.name + " updated to " + c.newDep.version + "\n";
            }
            
            combinedNotes += "\nOUTPUT FORMAT: Return ONLY the raw updated source code. DO NOT wrap in markdown blocks.";
            combinedChange.releaseNotes = combinedNotes;

            std::cout << " -> AI analyzing Java file " << filePath << "...\n";
            RefactorRequest req = {filePath, baseCode, combinedChange, ""};
            std::string rawAiCode = ai->RefactorCode(req);
            
            std::string workingCode = StringUtils::CleanAIOutput(rawAiCode, baseCode);
            workingCode = PostProcessCode(workingCode, relevantChanges);

            if (workingCode != baseCode) {
                gitlab->CommitFile(project.projectId, branchName, filePath, workingCode, "refactor: AI updates for Java dependency upgrades");
                modifiedSourceFiles.push_back(filePath);
            }
        }

        std::string mrDescription = BuildMergeRequestDescription(masterChanges, modifiedSourceFiles, modifiedBuildFiles);
        gitlab->CreateMergeRequest(project.projectId, branchName, project.defaultBranch, "chore: Automated Java Dependency Update", mrDescription);
    }
};