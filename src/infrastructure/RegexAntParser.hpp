#pragma once

#include "../domain/Interfaces.hpp"
#include <regex>
#include <string>
#include <vector>
#include <iostream>

class RegexAntParser : public IAntParser {
private:
    std::string EscapeRegex(const std::string& str) {
        return std::regex_replace(str, std::regex(R"([-[\]{}()*+?.,\^$|#\s])"), R"(\$&)");
    }

public:
    std::vector<Dependency> ParseDependencies(const std::string& antContent) override {
        std::vector<Dependency> dependencies;

        // Matches standard Apache Ivy dependency formats used with Ant:
        // <dependency org="commons-lang" name="commons-lang" rev="2.0"/>
        std::regex depRegex(R"(<dependency\s+[^>]*?org\s*=\s*['"]([^'"]+)['"][^>]*?name\s*=\s*['"]([^'"]+)['"][^>]*?rev\s*=\s*['"]([^'"]+)['"])");
        
        auto words_begin = std::sregex_iterator(antContent.begin(), antContent.end(), depRegex);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            dependencies.push_back({
                match[1].str(), // Organization (Group)
                match[2].str(), // Name
                match[3].str()  // Revision (Version)
            });
        }
        
        return dependencies;
    }

    std::string UpdateDependencyVersion(
        const std::string& antContent, 
        const Dependency& oldDep, 
        const Dependency& newDep) override 
    {
        std::string safeOrg = EscapeRegex(oldDep.group);
        std::string safeName = EscapeRegex(oldDep.name);
        std::string safeOldVer = EscapeRegex(oldDep.version);

        // Target the specific Ivy dependency tag
        std::regex targetPattern(
            "(<dependency\\s+[^>]*?org\\s*=\\s*['\"]" + safeOrg + "['\"][^>]*?name\\s*=\\s*['\"]" + safeName + 
            "['\"][^>]*?rev\\s*=\\s*['\"])" + safeOldVer + "(['\"])"
        );

        // Replace the revision attribute value while preserving the rest of the tag
        return std::regex_replace(antContent, targetPattern, "$1" + newDep.version + "$2");
    }
};