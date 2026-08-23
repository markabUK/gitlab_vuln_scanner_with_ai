#pragma once

#include "Models.hpp"
#include <memory>
#include <optional>
#include <vector>

// ==========================================
// 1. Gradle Parser Interface
// ==========================================
class IGradleParser {
public:
    virtual ~IGradleParser() = default;
    virtual std::vector<Dependency> ParseDependencies(const std::string& gradleContent) = 0;
    virtual std::string UpdateDependencyVersion(
        const std::string& gradleContent, 
        const Dependency& oldDep, 
        const Dependency& newDep
    ) = 0;
};

// ==========================================
// 2. Maven Registry Interface (Central / GitLab Private)
// ==========================================
class IMavenRegistry {
public:
    virtual ~IMavenRegistry() = default;
    virtual std::optional<std::string> GetLatestVersion(const Dependency& oldDep) = 0;
    virtual DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) = 0;
};

// ==========================================
// 3. Pluggable AI Assistant Interface (Vendor-Agnostic)
// ==========================================
class IAICodeAssistant {
public:
    virtual ~IAICodeAssistant() = default;

    virtual std::string GetProviderName() const = 0;
    virtual std::string RefactorCode(const RefactorRequest& request) = 0;
    virtual std::string GenerateMergeRequestDescription(
        const std::vector<DependencyChange>& appliedChanges
    ) = 0;
};

// ==========================================
// 4. GitLab Client Interfaces (Repository & MR Management)
// ==========================================
class IGitLabClient {
public:
    virtual ~IGitLabClient() = default;
    virtual std::vector<ProjectContext> GetProjectsInGroup(const std::string& groupId) = 0;
    virtual std::string FetchFileContent(const std::string& projectId, const std::string& filePath, const std::string& ref) = 0;
    virtual std::vector<std::string> GetSourceFiles(const std::string& projectId, const std::string& ref) = 0;
    virtual void CreateBranch(const std::string& projectId, const std::string& newBranch, const std::string& refBranch) = 0;
    virtual std::vector<MergeRequest> GetOpenMergeRequests(const std::string& projectId) = 0;
    virtual void CloseMergeRequest(const std::string& projectId, const std::string& mrIid) = 0;
    virtual void DeleteBranch(const std::string& projectId, const std::string& branchName) = 0;
    virtual void CommitFile(
        const std::string& projectId, 
        const std::string& branch, 
        const std::string& filePath, 
        const std::string& content, 
        const std::string& commitMessage
    ) = 0;
    virtual std::string CreateMergeRequest(
        const std::string& projectId, 
        const std::string& sourceBranch, 
        const std::string& targetBranch, 
        const std::string& title, 
        const std::string& description
    ) = 0;
};

// ==========================================
// 5. .NET Parser Interface
// ==========================================
class IDotNetParser {
public:
    virtual ~IDotNetParser() = default;
    
    // Parses .csproj, .fsproj, Directory.Build.props
    virtual std::vector<Dependency> ParseDependencies(const std::string& projectContent) = 0;
    
    // Updates PackageReference elements
    virtual std::string UpdateDependencyVersion(
        const std::string& projectContent, 
        const Dependency& oldDep, 
        const Dependency& newDep
    ) = 0;

    // Parses the new XML-based .slnx format to extract project paths
    virtual std::vector<std::string> ParseSlnxProjects(const std::string& slnxContent) = 0;
};

// ==========================================
// 6. NuGet Registry Interface
// ==========================================
class INuGetRegistry {
public:
    virtual ~INuGetRegistry() = default;
    virtual std::optional<std::string> GetLatestVersion(const Dependency& oldDep) = 0;
    virtual DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) = 0;
};