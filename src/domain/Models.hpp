#pragma once

#include <string>
#include <vector>
#include <optional>

struct Dependency {
    std::string group;
    std::string name;
    std::string version;
};

struct CodeReplacement {
    std::string search;
    std::string replace;
};

struct DependencyMigration {
    std::string oldGroup;
    std::string oldName;
    std::string newGroup;
    std::string newName;
    
    std::string migrationDocPath;     // NEW: Path to the markdown cheat-sheet
    std::string migrationDocContent;  // NEW: The actual content loaded at startup
    
    std::vector<CodeReplacement> replacements;
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
    std::string customPromptContext; // We will inject the markdown content here
};

struct ProjectContext {
    std::string projectId;
    std::string projectName;
    std::string defaultBranch;
    std::string rawBuildGradle;
    std::vector<std::string> sourceFiles; 
};

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
    std::string webUrl;      
    std::string authorEmail; 
};