#pragma once

#include "../domain/Interfaces.hpp"
#include <regex>
#include <sstream>

class RegexGradleParser : public IGradleParser {
public:
    std::vector<Dependency> ParseDependencies(const std::string& gradleContent) override {
        std::vector<Dependency> dependencies;
        
        // Matches: implementation 'group:name:version' OR api("group:name:version")
        // Group 1: Configuration (implementation, api, testCompile, etc.)
        // Group 2: Group ID
        // Group 3: Artifact ID
        // Group 4: Version
        std::regex depRegex(R"((implementation|api|compileOnly|testImplementation)\s*[\(]?['"]([^'"]+):([^'"]+):([^'"]+)['"][\)]?)");
        
        auto words_begin = std::sregex_iterator(gradleContent.begin(), gradleContent.end(), depRegex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            dependencies.push_back({
                match[2].str(), // Group
                match[3].str(), // Name
                match[4].str()  // Version
            });
        }

        return dependencies;
    }

    std::string UpdateDependencyVersion(
        const std::string& gradleContent, 
        const Dependency& oldDep, 
        const Dependency& newDep) override 
    {
        // Construct a safe regex to find the exact dependency line
        // E.g., looking for exactly: group:name:oldVersion
        std::string targetPattern = oldDep.group + ":" + oldDep.name + ":" + oldDep.version;
        std::string replacement = newDep.group + ":" + newDep.name + ":" + newDep.version;
        
        // Escape periods for regex matching
        std::string safePattern = std::regex_replace(targetPattern, std::regex(R"(\.)"), R"(\.)");
        std::regex replaceRegex(safePattern);

        return std::regex_replace(gradleContent, replaceRegex, replacement);
    }
};