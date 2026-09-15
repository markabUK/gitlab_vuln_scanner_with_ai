#pragma once
#include "../domain/Interfaces.hpp"
#include "../infrastructure/AppSettings.hpp"
#include "../infrastructure/GitManager.hpp"
#include <memory>
#include <spdlog/spdlog.h>
#include <chrono>
#include <vector>
#include <string>

class DependencyUpdateOrchestrator {
private:
    std::shared_ptr<IGitLabClient> gitlab;
    std::vector<std::shared_ptr<IEcosystemHandler>> handlers;
    AppSettings appSettings; 
    std::shared_ptr<INotificationClient> notifier;
    bool isDryRun;

    bool IsOlderThanOneMonth(const std::string& isoDate) {
        if (isoDate.size() < 10) return false;
        try {
            int year = std::stoi(isoDate.substr(0, 4));
            int month = std::stoi(isoDate.substr(5, 2));
            int day = std::stoi(isoDate.substr(8, 2));
            auto now = std::chrono::system_clock::now();
            time_t tt = std::chrono::system_clock::to_time_t(now);
            tm local_tm = *localtime(&tt);
            int currentYear = local_tm.tm_year + 1900;
            int currentMonth = local_tm.tm_mon + 1;
            int currentDay = local_tm.tm_mday;
            int totalMonthsDiff = (currentYear - year) * 12 + (currentMonth - month);
            if (totalMonthsDiff > 1) return true;
            if (totalMonthsDiff == 1) return currentDay >= day;
            return false;
        } catch (...) { return false; }
    }

    bool IsExcluded(const ProjectContext& project) {
        for (const auto& ep : appSettings.target.excludeProjects) {
            if (project.projectId == ep || project.projectName == ep) return true;
        }
        for (const auto& eg : appSettings.target.excludeGroups) {
            if (project.projectName.find(eg + "/") == 0) return true;
        }
        return false;
    }

    bool IsAutomatedCommit(const std::string& commitTitle) {
        return commitTitle.find("chore: Update ") == 0 || 
               commitTitle.find("refactor: AI updates for ") == 0 ||
               commitTitle.find("chore: Automated ") == 0;
    }

    std::vector<MergeRequest> GetBotMergeRequests(const std::string& projectId) {
        auto openMrs = gitlab->GetOpenMergeRequests(projectId);
        std::vector<MergeRequest> ourMrs;
        for (const auto& mr : openMrs) {
            if (mr.sourceBranch.find("chore/deps-update-") == 0) ourMrs.push_back(mr);
        }
        return ourMrs;
    }

    bool ShouldSkipDueToActiveOrHumanMRs(const ProjectContext& project, const std::vector<MergeRequest>& ourMrs) {
        for (const auto& mr : ourMrs) {
            auto commits = gitlab->GetMergeRequestCommits(project.projectId, mr.iid);
            bool hasHumanCommits = false;
            std::string humanName, humanEmail;
            for (const auto& commit : commits) {
                if (!IsAutomatedCommit(commit.title)) {
                    hasHumanCommits = true;
                    humanName = commit.authorName;
                    humanEmail = commit.authorEmail;
                    break;
                }
            }
            if (hasHumanCommits) {
                spdlog::warn("[SKIP] MR !{} contains manual human commits from {} ({}).", mr.iid, humanName, humanEmail);
                notifier->NotifyUserOfSkippedMR(project.projectName, humanName, humanEmail, mr.webUrl);
                return true; 
            }
            if (!IsOlderThanOneMonth(mr.createdAt)) {
                spdlog::info("  Found recent active MR with no manual commits. Skipping project.");
                return true; 
            }
        }
        return false;
    }

    void CleanupStaleMergeRequests(const std::string& projectId, const std::vector<MergeRequest>& ourMrs) {
        for (const auto& mr : ourMrs) {
            spdlog::info("  Closing stale, untouched MR ({})...", mr.sourceBranch);
            gitlab->CloseMergeRequest(projectId, mr.iid);
            gitlab->DeleteBranch(projectId, mr.sourceBranch);
        }
    }

    void ProcessProject(const ProjectContext& project) {
        spdlog::info("--------------------------------------------------");
        spdlog::info("Processing Project: {}", project.projectName);
        
        std::vector<MergeRequest> botMrs = GetBotMergeRequests(project.projectId);
        if (ShouldSkipDueToActiveOrHumanMRs(project, botMrs)) return;
        CleanupStaleMergeRequests(project.projectId, botMrs);
        
        // Clones locally into /tmp (automatically deleted upon function exit via RAII)
        GitManager git(appSettings.gitlabToken);

        std::string strippedHost = appSettings.gitlabHost;
        if (strippedHost.find("https://") == 0) strippedHost = strippedHost.substr(8);
        else if (strippedHost.find("http://") == 0) strippedHost = strippedHost.substr(7);

        std::string cloneUrl = "https://oauth2:" + appSettings.gitlabToken + "@" + strippedHost + "/" + project.projectName + ".git";
        if (!git.Clone(cloneUrl, project.projectId)) {
            spdlog::error("Failed to clone {}. Skipping.", project.projectName);
            return;
        }

        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        std::string branchName = "chore/deps-update-" + std::to_string(now);

        if (!isDryRun) {
            git.CreateBranch(branchName);
        }

        std::string combinedMrDescription = "";

        // Execute handlers using local files and collect MR descriptions
        for (const auto& handler : handlers) {
            std::string desc = handler->Process(project, git.GetLocalPath(), branchName);
            if (!desc.empty()) {
                combinedMrDescription += desc + "\n\n---\n\n";
            }
        }

        // --- ATOMIC COMMIT & PUSH ---
        if (git.HasChanges()) {
            if (isDryRun) {
                spdlog::info("=================================================================");
                spdlog::info("[DRY RUN] Local changes staged. Skipping push to remote GitLab.");
                spdlog::info("[DRY RUN] Workspace will now be securely destroyed.");
                spdlog::info("=================================================================");
                
                // Triggers the DryRunGitLabClient to log the dummy MR intent
                gitlab->CreateMergeRequest(project.projectId, branchName, project.defaultBranch, 
                    "chore: Automated Dependency Upgrades", combinedMrDescription);
            } else {
                if (git.CommitAll("chore: Automated dependency updates and AI compatibility refactoring")) {
                    if (git.Push(branchName)) {
                        gitlab->CreateMergeRequest(project.projectId, branchName, project.defaultBranch, 
                            "chore: Automated Dependency Upgrades", combinedMrDescription);
                    }
                }
            }
        } else {
            spdlog::info("Repository clean. No files were modified for {}.", project.projectName);
        }
    }

public:
    DependencyUpdateOrchestrator(
        std::shared_ptr<IGitLabClient> glClient,
        std::vector<std::shared_ptr<IEcosystemHandler>> ecosystemHandlers,
        const AppSettings& settings,
        std::shared_ptr<INotificationClient> notifClient,
        bool dryRun = false)
        : gitlab(glClient), handlers(std::move(ecosystemHandlers)), 
          appSettings(settings), notifier(notifClient), isDryRun(dryRun) {}

    void RunWorkflow() {
        std::vector<ProjectContext> projects;
        if (appSettings.target.type == "Project") {
            auto projOpt = gitlab->GetProject(appSettings.target.id);
            if (projOpt) projects.push_back(*projOpt);
        } else {
            projects = gitlab->GetProjectsInGroup(appSettings.target.id);
        }
        
        for (const auto& project : projects) {
            if (IsExcluded(project)) continue;
            ProcessProject(project);
        }
    }
};