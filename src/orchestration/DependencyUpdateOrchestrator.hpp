#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/AppSettings.hpp"
#include <memory>
#include <spdlog/spdlog.h>
#include <chrono>
#include <vector>
#include <string>

class DependencyUpdateOrchestrator {
private:
    std::shared_ptr<IGitLabClient> gitlab;
    std::vector<std::shared_ptr<IEcosystemHandler>> handlers;
    TargetConfig targetConfig;
    
    std::shared_ptr<INotificationClient> notifier;
    bool debugMode;

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
        for (const auto& ep : targetConfig.excludeProjects) {
            if (project.projectId == ep || project.projectName == ep) {
                return true;
            }
        }
        
        for (const auto& eg : targetConfig.excludeGroups) {
            if (project.projectName.find(eg + "/") == 0) {
                return true;
            }
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
            if (mr.sourceBranch.find("chore/deps-update-") == 0) {
                ourMrs.push_back(mr);
            }
        }
        return ourMrs;
    }

    bool ShouldSkipDueToActiveOrHumanMRs(const ProjectContext& project, const std::vector<MergeRequest>& ourMrs) {
        for (const auto& mr : ourMrs) {
            auto commits = gitlab->GetMergeRequestCommits(project.projectId, mr.iid);
            bool hasHumanCommits = false;
            std::string humanName, humanEmail;

            // Check commit patterns instead of author emails
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
                spdlog::warn("       Preserving their work and skipping project.");
                notifier->NotifyUserOfSkippedMR(project.projectName, humanName, humanEmail, mr.webUrl);
                return true; // Skip project
            }

            if (!IsOlderThanOneMonth(mr.createdAt)) {
                spdlog::info("  Found recent active MR with no manual commits. Skipping project.");
                return true; // Skip project
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

    void DispatchToEcosystemHandlers(const ProjectContext& project) {
        for (const auto& handler : handlers) {
            std::vector<std::string> targetExtensions = handler->GetTargetExtensions();
            auto relevantFiles = gitlab->GetSourceFiles(project.projectId, project.defaultBranch, targetExtensions);
            
            if (!relevantFiles.empty()) {
                spdlog::info("Executing {} workflow...", handler->GetEcosystemName());
                handler->Process(project, relevantFiles);
            }
        }
    }

    void ProcessProject(const ProjectContext& project) {
        spdlog::info("--------------------------------------------------");
        spdlog::info("Processing Project: {}", project.projectName);
        
        std::vector<MergeRequest> botMrs = GetBotMergeRequests(project.projectId);

        if (ShouldSkipDueToActiveOrHumanMRs(project, botMrs)) {
            return;
        }

        CleanupStaleMergeRequests(project.projectId, botMrs);
        
        DispatchToEcosystemHandlers(project);
    }

public:
    DependencyUpdateOrchestrator(
        std::shared_ptr<IGitLabClient> glClient,
        std::vector<std::shared_ptr<IEcosystemHandler>> ecosystemHandlers,
        const TargetConfig& target,
        std::shared_ptr<INotificationClient> notifClient,
        bool isDebug = false)
        : gitlab(glClient), handlers(std::move(ecosystemHandlers)), 
          targetConfig(target), notifier(notifClient), debugMode(isDebug) {}

    void RunWorkflow() {
        std::vector<ProjectContext> projects;

        if (targetConfig.type == "Project") {
            spdlog::info("Starting Unified Workflow for Single Project ID: {}...", targetConfig.id);
            auto projOpt = gitlab->GetProject(targetConfig.id);
            if (projOpt) {
                projects.push_back(*projOpt);
            }
        } else {
            spdlog::info("Starting Unified Workflow for Group ID: {}...", targetConfig.id);
            projects = gitlab->GetProjectsInGroup(targetConfig.id);
        }

        spdlog::info("Found {} initial project(s) before filtering.", projects.size());
        
        for (const auto& project : projects) {
            if (IsExcluded(project)) {
                spdlog::info("[SKIP] Project excluded by configuration: {}", project.projectName);
                continue;
            }
            ProcessProject(project);
        }
    }
};