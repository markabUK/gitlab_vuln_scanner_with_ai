#pragma once

#include "../domain/Interfaces.hpp"
#include <memory>
#include <iostream>
#include <chrono>

class DependencyUpdateOrchestrator {
private:
    std::shared_ptr<IGitLabClient> gitlab;
    std::shared_ptr<IMavenRegistry> maven;
    std::shared_ptr<IAICodeAssistant> ai;
    std::shared_ptr<IGradleParser> parser;

public:
    DependencyUpdateOrchestrator(
        std::shared_ptr<IGitLabClient> glClient,
        std::shared_ptr<IMavenRegistry> mvnRegistry,
        std::shared_ptr<IAICodeAssistant> aiAssistant,
        std::shared_ptr<IGradleParser> gradleParser)
        : gitlab(glClient), maven(mvnRegistry), ai(aiAssistant), parser(gradleParser) {}

    void RunWorkflow(const std::string& gitlabGroupId) {
        std::cout << "Starting Dependency Update Workflow using AI Provider: "
                  << ai->GetProviderName() << "\n";

        auto projects = gitlab->GetProjectsInGroup(gitlabGroupId);
        std::cout << "Found " << projects.size() << " projects in group " << gitlabGroupId << ".\n";

        for (const auto& project : projects) {
            ProcessProject(project);
        }
    }

private:
    void ProcessProject(const ProjectContext& project) {
        std::cout << "--------------------------------------------------\n";
        std::cout << "Processing Project: " << project.projectName << "\n";

        // Try build.gradle first, then build.gradle.kts for Kotlin DSL projects
        std::string buildFilePath = "build.gradle";
        std::string buildGradle = gitlab->FetchFileContent(project.projectId, buildFilePath, project.defaultBranch);
        if (buildGradle.empty()) {
            buildFilePath = "build.gradle.kts";
            buildGradle = gitlab->FetchFileContent(project.projectId, buildFilePath, project.defaultBranch);
        }
        if (buildGradle.empty()) {
            std::cout << "No build.gradle / build.gradle.kts found on default branch. Skipping.\n";
            return;
        }

        auto dependencies = parser->ParseDependencies(buildGradle);
        std::vector<DependencyChange> appliedChanges;
        std::string updatedGradle = buildGradle;

        for (const auto& dep : dependencies) {
            auto latestOpt = maven->GetLatestVersion(dep);
            if (latestOpt && *latestOpt != dep.version) {
                Dependency newDep = {dep.group, dep.name, *latestOpt};
                DependencyChange diff = maven->InspectVersionDiff(dep, newDep);
                updatedGradle = parser->UpdateDependencyVersion(updatedGradle, dep, diff.newDep);
                appliedChanges.push_back(diff);
                std::cout << "Update found: " << dep.name << " (" << dep.version << " -> " << diff.newDep.version << ")\n";
            }
        }

        if (appliedChanges.empty()) {
            std::cout << "All dependencies are up to date.\n";
            return;
        }

        CreateRefactoringMergeRequest(project, buildFilePath, updatedGradle, appliedChanges);
    }

    void CreateRefactoringMergeRequest(const ProjectContext& project, const std::string& buildFilePath,
                                       const std::string& newGradle, const std::vector<DependencyChange>& changes) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        std::string branchName = "chore/deps-update-" + std::to_string(now);

        std::cout << "Creating branch: " << branchName << "\n";
        gitlab->CreateBranch(project.projectId, branchName, project.defaultBranch);
        gitlab->CommitFile(project.projectId, branchName, buildFilePath, newGradle,
                           "chore: Update dependencies in " + buildFilePath);

        std::cout << "Fetching source files for AI analysis...\n";
        auto sourceFiles = gitlab->GetSourceFiles(project.projectId, project.defaultBranch);

        if (sourceFiles.empty()) {
            std::cout << "No source files found in repository.\n";
        } else {
            std::cout << "Found " << sourceFiles.size() << " source files. Beginning AI refactoring phase...\n";
        }

        for (const auto& filePath : sourceFiles) {
            std::string workingCode = gitlab->FetchFileContent(project.projectId, filePath, project.defaultBranch);
            if (workingCode.empty()) continue;

            std::string baseCode = workingCode;
            for (const auto& change : changes) {
                RefactorRequest req = {filePath, workingCode, change, ""};
                std::string refactoredCode = ai->RefactorCode(req);
                if (refactoredCode != workingCode && !refactoredCode.empty())
                    workingCode = refactoredCode;
            }

            if (workingCode != baseCode) {
                std::cout << "AI refactored file: " << filePath << "\n";
                gitlab->CommitFile(project.projectId, branchName, filePath, workingCode,
                                   "refactor: AI updates for dependency upgrades");
            }
        }

        std::string mrDescription = ai->GenerateMergeRequestDescription(changes);
        std::string mrUrl = gitlab->CreateMergeRequest(
            project.projectId, branchName, project.defaultBranch,
            "chore: Automated Dependency & Code Update", mrDescription
        );

        std::cout << "Successfully created Merge Request: " << mrUrl << "\n";
    }
};