#pragma once

#include "../domain/Interfaces.hpp"
#include <regex>
#include <map>
#include <iostream>
#include <algorithm>

class AdvancedGradleParser : public IGradleParser {
private:
    std::map<std::string, std::string> variables;

    // Helper to strip $, { and } from variables like ${libVersion}
    std::string CleanVariableName(std::string varStr) {
        varStr.erase(std::remove(varStr.begin(), varStr.end(), '$'), varStr.end());
        varStr.erase(std::remove(varStr.begin(), varStr.end(), '{'), varStr.end());
        varStr.erase(std::remove(varStr.begin(), varStr.end(), '}'), varStr.end());
        return varStr;
    }

    // Helper to escape special regex characters (like dots in version numbers)
    std::string EscapeRegex(const std::string& s) {
        return std::regex_replace(s, std::regex(R"([-[\]{}()*+?.,\^$|#\s])"), R"(\$&)");
    }

    // Safely replaces text without using $1 formatting (prevents the $12 capture group bug)
    std::string SafeReplace(const std::string& content, const std::regex& pattern, const std::string& oldSub, const std::string& newSub) {
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
            
            // Find where the old version string is inside the match
            size_t verPos = fullMatch.rfind(oldSub); 
            if (verPos != std::string::npos) {
                // Stitch together the match with the new version safely
                std::string newMatch = fullMatch.substr(0, verPos) + newSub + fullMatch.substr(verPos + oldSub.length());
                
                result.replace(matchPos, fullMatch.length(), newMatch);
                searchOffset = matchPos + newMatch.length();
            } else {
                searchOffset = matchPos + fullMatch.length();
            }
        }
        return result;
    }

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

public:
    std::vector<Dependency> ParseDependencies(const std::string& gradleContent) override {
        std::vector<Dependency> dependencies;
        
        ExtractVariables(gradleContent);

        // Pass 1: Parse String Notation
        std::regex stringNotRegex(R"((implementation|api|compileOnly|testImplementation)\s*[\(]?\s*(['"])([^'"]+):([^'"]+):([^'"]+)\2\s*[\)]?)");
        auto beginStr = std::sregex_iterator(gradleContent.begin(), gradleContent.end(), stringNotRegex);
        for (std::sregex_iterator i = beginStr; i != std::sregex_iterator(); ++i) {
            std::smatch match = *i;
            std::string rawVersion = match[5].str();
            
            if (rawVersion.find('$') != std::string::npos) {
                std::string varName = CleanVariableName(rawVersion);
                if (variables.count(varName)) {
                    rawVersion = variables[varName];
                }
            }
            dependencies.push_back({match[3].str(), match[4].str(), rawVersion});
        }

        // Pass 2: Parse Map Notation
        std::regex mapNotRegex(R"(group\s*[:=]\s*['"]([^'"]+)['"]\s*,\s*name\s*[:=]\s*['"]([^'"]+)['"]\s*,\s*version\s*[:=]\s*['"]([^'"]+)['"])");
        auto beginMap = std::sregex_iterator(gradleContent.begin(), gradleContent.end(), mapNotRegex);
        for (std::sregex_iterator i = beginMap; i != std::sregex_iterator(); ++i) {
            std::smatch match = *i;
            std::string rawVersion = match[3].str();
            
            if (rawVersion.find('$') != std::string::npos) {
                std::string varName = CleanVariableName(rawVersion);
                if (variables.count(varName)) {
                    rawVersion = variables[varName];
                }
            }
            dependencies.push_back({match[1].str(), match[2].str(), rawVersion});
        }

        return dependencies;
    }

    std::string UpdateDependencyVersion(
        const std::string& gradleContent, 
        const Dependency& oldDep, 
        const Dependency& newDep) override 
    {
        std::string updatedContent = gradleContent;
        std::string safeName = EscapeRegex(oldDep.name);
        std::string safeOldVer = EscapeRegex(oldDep.version);

        std::string targetVarName = "";
        for (const auto& [varName, varValue] : variables) {
            if (varValue == oldDep.version) {
                targetVarName = varName;
                break;
            }
        }

        if (!targetVarName.empty()) {
            // Update the variable assignment (e.g., ext.log4jVersion = '2.25.1')
            std::regex safeVarAssignRegex(EscapeRegex(targetVarName) + "\\s*=\\s*['\"]" + safeOldVer + "['\"]");
            updatedContent = SafeReplace(updatedContent, safeVarAssignRegex, oldDep.version, newDep.version);
            variables[targetVarName] = newDep.version;
        } else {
            // Update String Notation (e.g., 'org.apache.logging.log4j:log4j-api:2.25.1')
            std::regex strRegex("['\"]" + EscapeRegex(oldDep.group) + ":" + safeName + ":" + safeOldVer + "['\"]");
            updatedContent = SafeReplace(updatedContent, strRegex, oldDep.version, newDep.version);

            // Update Map Notation (e.g., name: 'log4j-api', version: '2.25.1')
            std::regex mapRegex("name\\s*[:=]\\s*['\"]" + safeName + "['\"]\\s*,\\s*version\\s*[:=]\\s*['\"]" + safeOldVer + "['\"]");
            updatedContent = SafeReplace(updatedContent, mapRegex, oldDep.version, newDep.version);
        }

        return updatedContent;
    }
};