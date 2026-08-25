#pragma once

#include <string>
#include <vector>
#include <optional>

struct Dependency {
    std::string group;
    std::string name;
    std::string version;
};

struct DependencyChange {
    Dependency oldDep;
    Dependency newDep;
    bool hasPackageMove{false};
    std::string oldPackageName;
    std::string newPackageName;
    std::string releaseNotes;
    bool skipAI{false};
};

struct RefactorRequest {
    std::string filePath;
    std::string originalCode;
    DependencyChange changeDetails;
    std::string customPromptContext;
};

struct ProjectContext {
    std::string projectId;
    std::string projectName;
    std::string defaultBranch;
    std::string rawBuildGradle;
    std::vector<std::string> sourceFiles; 
};

// NEW: Commit model to track author contributions
struct Commit {
    std::string id;
    std::string authorName;
    std::string authorEmail;
    std::string title;
};

struct MergeRequest {
    std::string iid;
    std::string title;
    std::string sourceBranch;
    std::string createdAt; 
    std::string webUrl;      // NEW: For chat links
    std::string authorEmail; // NEW: To identify if the bot owns it
};