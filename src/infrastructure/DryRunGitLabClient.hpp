#pragma once

#include "../domain/Interfaces.hpp"
#include <iostream>
#include <memory>

class DryRunGitLabClient : public IGitLabClient {
private:
    std::shared_ptr<IGitLabClient> realClient;

public:
    explicit DryRunGitLabClient(std::shared_ptr<IGitLabClient> client) 
        : realClient(std::move(client)) {}

    std::vector<ProjectContext> GetProjectsInGroup(const std::string& groupId) override {
        return realClient->GetProjectsInGroup(groupId);
    }

    std::string FetchFileContent(const std::string& projectId, const std::string& filePath, const std::string& ref) override {
        return realClient->FetchFileContent(projectId, filePath, ref);
    }

    std::vector<std::string> GetSourceFiles(const std::string& projectId, const std::string& ref) override {
        return realClient->GetSourceFiles(projectId, ref);
    }

    void CreateBranch(const std::string& projectId, const std::string& newBranch, const std::string& refBranch) override {
        std::cout << "👉 [DRY RUN] Would create branch: '" << newBranch 
                  << "' from '" << refBranch << "' in project " << projectId << "\n";
    }

    void CommitFile(const std::string& projectId, const std::string& branch, const std::string& filePath,
                    const std::string& content, const std::string& commitMessage) override {
        std::cout << "\n=================================================================\n";
        std::cout << "👉 [DRY RUN] Would commit file: '" << filePath << "' to branch '" << branch << "'\n";
        std::cout << "   [Commit Message]: " << commitMessage << "\n";
        std::cout << "   [FULL REFACTORED CODE BY AI]:\n";
        std::cout << "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n";
        
        // Print the complete refactored code from the AI
        std::cout << content << "\n";
        
        std::cout << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
        std::cout << "=================================================================\n\n";
    }

    std::string CreateMergeRequest(const std::string& projectId, const std::string& sourceBranch,
                                   const std::string& targetBranch, const std::string& title,
                                   const std::string& description) override {
        std::cout << "👉 [DRY RUN] Would create Merge Request in project " << projectId << "\n";
        std::cout << "   [Source]: " << sourceBranch << " -> [Target]: " << targetBranch << "\n";
        std::cout << "   [Title]: " << title << "\n";
        return "http://dry-run.local/mr/dummy"; 
    }

    std::vector<MergeRequest> GetOpenMergeRequests(const std::string& projectId) override {
        return realClient->GetOpenMergeRequests(projectId); // Safe to read
    }

    void CloseMergeRequest(const std::string& projectId, const std::string& mrIid) override {
        std::cout << "👉 [DRY RUN] Would CLOSE stale Merge Request IID: '" << mrIid << "' in project " << projectId << "\n";
    }

    void DeleteBranch(const std::string& projectId, const std::string& branchName) override {
        std::cout << "👉 [DRY RUN] Would DELETE stale branch: '" << branchName << "' in project " << projectId << "\n";
    }
};