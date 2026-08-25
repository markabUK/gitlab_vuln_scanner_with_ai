#pragma once

#include "../domain/Interfaces.hpp"
#include <regex>
#include <string>
#include <vector>
#include <iostream>

class RegexGoParser : public IGoParser {
private:
    std::string EscapeRegex(const std::string& str) {
        return std::regex_replace(str, std::regex(R"([-[\]{}()*+?.,\^$|#\s])"), R"(\$&)");
    }

public:
    std::vector<Dependency> ParseDependencies(const std::string& goModContent) override {
        std::vector<Dependency> dependencies;

        // Matches module paths and versions in go.mod.
        // E.g., `github.com/stretchr/testify v1.8.4` or `golang.org/x/crypto v0.14.0 // indirect`
        // We use multiline mode implicitly by scanning line by line or using wide matchers.
        std::regex requireRegex(R"(([a-zA-Z0-9.\-_/]+)\s+(v[0-9a-zA-Z.\-_+]+)(?:\s+//.*)?)");
        
        auto words_begin = std::sregex_iterator(goModContent.begin(), goModContent.end(), requireRegex);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string modPath = match[1].str();
            
            // Skip the "module" declaration itself or "go" version declaration
            if (modPath == "module" || modPath == "go") {
                continue;
            }
            
            dependencies.push_back({
                "",             // Group is kept empty; Go modules use the full path as the unique identifier
                modPath,        // Module Path (e.g., github.com/foo/bar)
                match[2].str()  // Version (e.g., v1.2.3)
            });
        }

        return dependencies;
    }

    std::string UpdateDependencyVersion(
        const std::string& goModContent, 
        const Dependency& oldDep, 
        const Dependency& newDep) override 
    {
        std::string safeName = EscapeRegex(oldDep.name);
        std::string safeOldVer = EscapeRegex(oldDep.version);

        // Look for the exact module path followed by whitespace and the old version
        // $1 captures the path and whitespace, $2 captures any trailing comments (e.g., "// indirect")
        std::regex targetPattern("(" + safeName + "\\s+)" + safeOldVer + "(\\s*(?://.*)?|$)");
        
        return std::regex_replace(goModContent, targetPattern, "$1" + newDep.version + "$2");
    }
};