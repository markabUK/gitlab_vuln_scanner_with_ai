#pragma once

#include "../domain/Interfaces.hpp"
#include <regex>
#include <string>
#include <vector>
#include <iostream>

class RegexNpmParser : public INpmParser {
private:
    std::string EscapeRegex(const std::string& str) {
        return std::regex_replace(str, std::regex(R"([-[\]{}()*+?.,\^$|#\s])"), R"(\$&)");
    }

public:
    std::vector<Dependency> ParseDependencies(const std::string& packageJsonContent) override {
        std::vector<Dependency> dependencies;

        // Matches JSON key-value pairs that look like NPM dependencies.
        // Group 1: Package name (supports scoped packages like @types/node)
        // Group 2: Version string (supports semver prefixes like ^, ~, >=)
        std::regex depRegex(R"(\"(@?[a-zA-Z0-9.\-_/]+)\"\s*:\s*\"([~^\>=<]*\d[^"]*)\")");
        
        auto words_begin = std::sregex_iterator(packageJsonContent.begin(), packageJsonContent.end(), depRegex);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            
            // Extract the actual raw version without the semver prefix (^, ~) for registry lookups
            std::string fullVersion = match[2].str();
            std::string cleanVersion = std::regex_replace(fullVersion, std::regex(R"([~^\>=<]+)"), "");

            dependencies.push_back({
                "",             // Group is empty for NPM
                match[1].str(), // Package Name
                cleanVersion    // Cleaned Version for registry comparison
            });
        }
        
        return dependencies;
    }

    std::string UpdateDependencyVersion(
        const std::string& packageJsonContent, 
        const Dependency& oldDep, 
        const Dependency& newDep) override 
    {
        std::string safeName = EscapeRegex(oldDep.name);
        
        // We match the package name, the colon, and the opening quote of the version.
        // We capture any existing semver prefix (^, ~) in Group 2 to preserve it!
        std::regex targetPattern(R"((\")" + safeName + R"(\"\s*:\s*\")([~^\>=<]*)[^\"]+(\"))");
        
        // Replace with: "package-name": " + prefix + newVersion + "
        return std::regex_replace(packageJsonContent, targetPattern, "$1$2" + newDep.version + "$3");
    }
};