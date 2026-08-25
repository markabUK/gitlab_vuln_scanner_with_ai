#pragma once

#include "../domain/Interfaces.hpp"
#include <regex>
#include <string>
#include <vector>
#include <iostream>

class RegexPomParser : public IPomParser {
private:
    std::string EscapeRegex(const std::string& str) {
        return std::regex_replace(str, std::regex(R"([-[\]{}()*+?.,\^$|#\s])"), R"(\$&)");
    }

public:
    std::vector<Dependency> ParseDependencies(const std::string& pomContent) override {
        std::vector<Dependency> dependencies;

        // Matches a standard Maven dependency block:
        // <dependency>
        //   <groupId>org.example</groupId>
        //   <artifactId>my-lib</artifactId>
        //   <version>1.0.0</version>
        // </dependency>
        std::regex depRegex(R"(<dependency>\s*<groupId>([^<]+)</groupId>\s*<artifactId>([^<]+)</artifactId>\s*<version>([^<]+)</version>\s*</dependency>)");
        
        auto words_begin = std::sregex_iterator(pomContent.begin(), pomContent.end(), depRegex);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            dependencies.push_back({
                match[1].str(), // Group ID
                match[2].str(), // Artifact ID
                match[3].str()  // Version
            });
        }
        
        return dependencies;
    }

    std::string UpdateDependencyVersion(
        const std::string& pomContent, 
        const Dependency& oldDep, 
        const Dependency& newDep) override 
    {
        std::string safeGroup = EscapeRegex(oldDep.group);
        std::string safeArtifact = EscapeRegex(oldDep.name);
        std::string safeOldVer = EscapeRegex(oldDep.version);

        // Target the specific block to avoid modifying sibling dependencies that might share the same version variable
        std::regex targetPattern(
            "(<dependency>\\s*<groupId>\\s*" + safeGroup + "\\s*</groupId>\\s*" +
            "<artifactId>\\s*" + safeArtifact + "\\s*</artifactId>\\s*" +
            "<version>\\s*)" + safeOldVer + "(\\s*</version>\\s*</dependency>)"
        );

        // Replace only the version portion ($1 captures everything before the version, $2 everything after)
        return std::regex_replace(pomContent, targetPattern, "$1" + newDep.version + "$2");
    }
};