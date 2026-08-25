#pragma once

#include "../domain/Interfaces.hpp"
#include <memory>
#include <iostream>
#include <chrono>
#include <vector>
#include <string>

class DependencyUpdateOrchestrator {
private:
    std::shared_ptr<IGitLabClient> gitlab;
    std::vector<std::shared_ptr<IEcosystemHandler>> handlers;
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

public:
    DependencyUpdateOrchestrator(
        std::shared_ptr<IGitLabClient> glClient,
        std::vector<std::shared_ptr<IEcosystemHandler>> ecosystemHandlers,
        bool isDebug = false)
        : gitlab(glClient), handlers(std::move(ecosystemHandlers)), debugMode(isDebug) {}

    void RunWorkflow(const std::string& gitlabGroupId) {
        std::cout << "Starting Unified Dependency Update Workflow...\n";
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
        
        // --- 1. MR Housekeeping ---
        auto openMrs = gitlab->GetOpenMergeRequests(project.projectId);
        std::vector<MergeRequest> ourMrs;
        for (const auto& mr : openMrs) {
            if (mr.sourceBranch.find("chore/deps-update-") == 0) ourMrs.push_back(mr);
        }

        for (const auto& mr : ourMrs) {
            if (!IsOlderThanOneMonth(mr.createdAt)) {
                std::cout << "   Found recent active MR. Skipping project.\n";
                return; 
            }
        }

        for (const auto& mr : ourMrs) {
            std::cout << "  Closing stale MR (" << mr.sourceBranch << ")...\n";
            gitlab->CloseMergeRequest(project.projectId, mr.iid);
            gitlab->DeleteBranch(project.projectId, mr.sourceBranch);
        }

        // --- 2. Dispatch to Handlers ---
        for (const auto& handler : handlers) {
            std::vector<std::string> targetExtensions = handler->GetTargetExtensions();
            
            // Only fetch the specific files this handler cares about to save bandwidth
            auto relevantFiles = gitlab->GetSourceFiles(project.projectId, project.defaultBranch, targetExtensions);
            
            if (!relevantFiles.empty()) {
                std::cout << "Executing " << handler->GetEcosystemName() << " workflow...\n";
                handler->Process(project, relevantFiles);
            }
        }
    }
};