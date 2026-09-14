#pragma once

#include "../domain/Interfaces.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <unordered_set>

class DryRunGitLabClient : public IGitLabClient {
private:
    std::shared_ptr<IGitLabClient> realClient;

public:
    explicit DryRunGitLabClient(std::shared_ptr<IGitLabClient> client)
        : realClient(std::move(client)) {}

    std::optional<ProjectContext> GetProject(const std::string& projectId) override {
        return realClient->GetProject(projectId);
    }

    std::vector<ProjectContext> GetProjectsInGroup(const std::string& groupId) override {
        return realClient->GetProjectsInGroup(groupId);
    }

    std::string FetchFileContent(const std::string& projectId, const std::string& filePath, const std::string& ref) override {
        return realClient->FetchFileContent(projectId, filePath, ref);
    }

    std::vector<std::string> GetSourceFiles(const std::string& projectId, const std::string& ref, const std::vector<std::string>& extensions) override {
        return realClient->GetSourceFiles(projectId, ref, extensions);
    }

    void CreateBranch(const std::string& projectId, const std::string& newBranch, const std::string& refBranch) override {
        spdlog::info("  [DRY RUN] Would create branch: '{}' from '{}' in project {}", newBranch, refBranch, projectId);
    }

    void CommitFile(const std::string& projectId, const std::string& branch, const std::string& filePath,
                    const std::string& content, const std::string& commitMessage) override {
        spdlog::info("=================================================================");
        spdlog::info("  [DRY RUN] Would commit file: '{}' to branch '{}'", filePath, branch);
        spdlog::info("   [Commit Message]: {}", commitMessage);
        spdlog::info("   [FULL REFACTORED CODE BY AI]:");
        spdlog::info("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv");
        
        spdlog::info("{}", content);
        
        spdlog::info("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
        spdlog::info("=================================================================");
    }

    std::string CreateMergeRequest(const std::string& projectId, const std::string& sourceBranch,
                                   const std::string& targetBranch, const std::string& title,
                                   const std::string& description) override {
        spdlog::info("  [DRY RUN] Would create Merge Request in project {}", projectId);
        spdlog::info("   [Source]: {} -> [Target]: {}", sourceBranch, targetBranch);
        spdlog::info("   [Title]: {}", title);
        return "http://dry-run.local/mr/dummy";
    }

    std::vector<MergeRequest> GetOpenMergeRequests(const std::string& projectId) override {
        return realClient->GetOpenMergeRequests(projectId); // Safe to read
    }

    std::vector<Commit> GetMergeRequestCommits(const std::string& projectId, const std::string& mrIid) override {
        return realClient->GetMergeRequestCommits(projectId, mrIid); // Safe to read
    }

    void CloseMergeRequest(const std::string& projectId, const std::string& mrIid) override {
        spdlog::info("  [DRY RUN] Would CLOSE stale Merge Request IID: '{}' in project {}", mrIid, projectId);
    }

    void DeleteBranch(const std::string& projectId, const std::string& branchName) override {
        spdlog::info("  [DRY RUN] Would DELETE stale branch: '{}' in project {}", branchName, projectId);
    }
};