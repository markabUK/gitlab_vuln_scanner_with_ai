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
};

struct RefactorRequest {
    std::string filePath;
    std::string originalCode;
    DependencyChange changeDetails;
    std::string customPromptContext; // Optional user prompt extensions
};

struct ProjectContext {
    std::string projectId;
    std::string projectName;
    std::string defaultBranch;
    std::string rawBuildGradle;
    std::vector<std::string> sourceFiles; // Paths to Java/Kotlin source files
};