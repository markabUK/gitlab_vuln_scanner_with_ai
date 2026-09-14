#pragma once

#include "../domain/Interfaces.hpp"
#include <regex>
#include <map>
#include <iostream>
#include <algorithm>

class AdvancedGradleParser : public IGradleParser {
private:
    std::map<std::string, std::string> variables;

    // --- Helper Methods ---

    std::string CleanVariableName(std::string varStr) const {
        varStr.erase(std::remove(varStr.begin(), varStr.end(), '$'), varStr.end());
        varStr.erase(std::remove(varStr.begin(), varStr.end(), '{'), varStr.end());
        varStr.erase(std::remove(varStr.begin(), varStr.end(), '}'), varStr.end());
        return varStr;
    }

    std::string EscapeRegex(const std::string& s) const {
        return std::regex_replace(s, std::regex(R"([-[\]{}()*+?.,\^$|#\s])"), R"(\$&)");
    }

    std::string SafeReplace(const std::string& content, const std::regex& pattern, const std::string& oldSub, const std::string& newSub) const {
        std::string result = content;
        std::smatch match;
        size_t searchOffset = 0;
        
        while (true) {
            std::string::const_iterator searchStart = result.cbegin() + searchOffset;
            if (!std::regex_search(searchStart, result.cend(), match, pattern)) {
                break;
            }
            
            std::string fullMatch = match[0].str();
            size_t matchPos = searchOffset + match.position();
            
            size_t verPos = fullMatch.rfind(oldSub); 
            if (verPos != std::string::npos) {
                std::string newMatch = fullMatch.substr(0, verPos) + newSub + fullMatch.substr(verPos + oldSub.length());
                result.replace(matchPos, fullMatch.length(), newMatch);
                searchOffset = matchPos + newMatch.length();
            } else {
                searchOffset = matchPos + fullMatch.length();
            }
        }
        return result;
    }

    // --- Parsing Subroutines ---

    void ExtractVariables(const std::string& content) {
        variables.clear();
        std::regex varRegex(R"((?:def|val|var|ext\.)?\s*([a-zA-Z0-9_.]+)\s*=\s*['"]([^'"]+)['"])");
        
        auto begin = std::sregex_iterator(content.begin(), content.end(), varRegex);
        auto end = std::sregex_iterator();

        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::smatch match = *i;
            variables[match[1].str()] = match[2].str();
        }
    }

    std::string ResolveVersion(std::string rawVersion) const {
        if (rawVersion.find('$') != std::string::npos) {
            std::string varName = CleanVariableName(rawVersion);
            auto it = variables.find(varName);
            if (it != variables.end()) {
                return it->second;
            }
        }
        return rawVersion;
    }

    void ParseStringNotation(const std::string& content, std::vector<Dependency>& dependencies) const {
        std::regex stringNotRegex(R"((implementation|api|compileOnly|testImplementation)\s*[\(]?\s*(['"])([^'"]+):([^'"]+):([^'"]+)\2\s*[\)]?)");
        auto beginStr = std::sregex_iterator(content.begin(), content.end(), stringNotRegex);
        for (std::sregex_iterator i = beginStr; i != std::sregex_iterator(); ++i) {
            std::smatch match = *i;
            dependencies.push_back({match[3].str(), match[4].str(), ResolveVersion(match[5].str())});
        }
    }

    void ParseMapNotation(const std::string& content, std::vector<Dependency>& dependencies) const {
        std::regex mapNotRegex(R"(group\s*[:=]\s*['"]([^'"]+)['"]\s*,\s*name\s*[:=]\s*['"]([^'"]+)['"]\s*,\s*version\s*[:=]\s*['"]([^'"]+)['"])");
        auto beginMap = std::sregex_iterator(content.begin(), content.end(), mapNotRegex);
        for (std::sregex_iterator i = beginMap; i != std::sregex_iterator(); ++i) {
            std::smatch match = *i;
            dependencies.push_back({match[1].str(), match[2].str(), ResolveVersion(match[3].str())});
        }
    }

    // --- Update Subroutines ---

    std::string FindVariableForVersion(const std::string& version) const {
        for (const auto& [varName, varValue] : variables) {
            if (varValue == version) {
                return varName;
            }
        }
        return "";
    }

    std::string UpdateVariableNotation(const std::string& content, const std::string& varName, const std::string& oldVersion, const std::string& newVersion) const {
        std::string safeOldVer = EscapeRegex(oldVersion);
        std::regex safeVarAssignRegex(EscapeRegex(varName) + "\\s*=\\s*['\"]" + safeOldVer + "['\"]");
        return SafeReplace(content, safeVarAssignRegex, oldVersion, newVersion);
    }

    std::string UpdateInlineNotation(const std::string& content, const Dependency& oldDep, const std::string& newVersion) const {
        std::string updatedContent = content;
        std::string safeName = EscapeRegex(oldDep.name);
        std::string safeOldVer = EscapeRegex(oldDep.version);

        // Update String Notation (e.g., 'org.apache.logging.log4j:log4j-api:2.25.1')
        std::regex strRegex("['\"]" + EscapeRegex(oldDep.group) + ":" + safeName + ":" + safeOldVer + "['\"]");
        updatedContent = SafeReplace(updatedContent, strRegex, oldDep.version, newVersion);

        // Update Map Notation (e.g., name: 'log4j-api', version: '2.25.1')
        std::regex mapRegex("name\\s*[:=]\\s*['\"]" + safeName + "['\"]\\s*,\\s*version\\s*[:=]\\s*['\"]" + safeOldVer + "['\"]");
        updatedContent = SafeReplace(updatedContent, mapRegex, oldDep.version, newVersion);

        return updatedContent;
    }

public:
    std::vector<Dependency> ParseDependencies(const std::string& gradleContent) override {
        std::vector<Dependency> dependencies;
        
        ExtractVariables(gradleContent);
        ParseStringNotation(gradleContent, dependencies);
        ParseMapNotation(gradleContent, dependencies);

        return dependencies;
    }

    std::string UpdateDependencyVersion(
        const std::string& gradleContent, 
        const Dependency& oldDep, 
        const Dependency& newDep) override 
    {
        std::string targetVarName = FindVariableForVersion(oldDep.version);

        if (!targetVarName.empty()) {
            std::string updatedContent = UpdateVariableNotation(gradleContent, targetVarName, oldDep.version, newDep.version);
            variables[targetVarName] = newDep.version; // Keep internal state synced
            return updatedContent;
        } else {
            return UpdateInlineNotation(gradleContent, oldDep, newDep.version);
        }
    }
};