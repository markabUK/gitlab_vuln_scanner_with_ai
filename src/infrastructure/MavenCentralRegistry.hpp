#pragma once

#include "../domain/Interfaces.hpp"
#include "HttpClient.hpp"
#include "AppSettings.hpp"
#include <iostream>
#include <algorithm>
#include <regex>
#include <cctype>
#include <vector>

class MavenCentralRegistry : public IMavenRegistry {
private:
    std::vector<DependencyMigration> migrations;

public:
    explicit MavenCentralRegistry(const std::vector<DependencyMigration>& configMigrations = {}) 
        : migrations(configMigrations) {}

    std::optional<std::string> GetLatestVersion(const Dependency& oldDep) override {
        Dependency searchDep = ApplyKnownEcosystemMigrations(oldDep);

        std::string groupPath = searchDep.group;
        std::replace(groupPath.begin(), groupPath.end(), '.', '/');
        
        std::string url = "https://repo1.maven.org/maven2/" + groupPath + "/" + searchDep.name + "/maven-metadata.xml";
        
        auto response = HttpClient::Get(url);
        if (response.statusCode != 200) return std::nullopt;

        try {
            std::regex unstablePattern(R"((alpha|beta|rc|cr|preview|draft|-m\d+|\.m\d+|-b\d+|-ea))", std::regex_constants::icase);
            std::regex versionRegex(R"(<version>([^<]+)</version>)");

            std::string requiredSuffix = "";
            std::string lowerOldVer = oldDep.version;
            std::transform(lowerOldVer.begin(), lowerOldVer.end(), lowerOldVer.begin(), ::tolower);
            
            bool wasAndroid = (lowerOldVer.find("android") != std::string::npos);
            
            std::vector<std::string> classifiers = {"jre11", "jre17", "jre21", "jre8"};
            for (const auto& cls : classifiers) {
                if (lowerOldVer.find(cls) != std::string::npos) {
                    requiredSuffix = cls;
                    break;
                }
            }

            bool isEcosystemShift = (searchDep.group != oldDep.group || searchDep.name != oldDep.name);
            std::string bestVersion = isEcosystemShift ? "0.0.0" : oldDep.version;
            bool foundNewer = false;

            auto words_begin = std::sregex_iterator(response.body.begin(), response.body.end(), versionRegex);
            auto words_end = std::sregex_iterator();

            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                std::string version = (*i)[1].str();
                
                if (!std::regex_search(version, unstablePattern)) {
                    std::string lowerNewVer = version;
                    std::transform(lowerNewVer.begin(), lowerNewVer.end(), lowerNewVer.begin(), ::tolower);
                    
                    if (!wasAndroid && lowerNewVer.find("android") != std::string::npos) continue;
                    if (!requiredSuffix.empty() && lowerNewVer.find(requiredSuffix) == std::string::npos) continue;
                    
                    if (IsVersionGreater(version, bestVersion)) {
                        bestVersion = version;
                        foundNewer = true;
                    }
                }
            }

            if (foundNewer) return bestVersion;

        } catch (const std::exception& e) {
            std::cerr << "XML Parsing error for Maven metadata: " << e.what() << "\n";
        }
        return std::nullopt;
    }
    
    DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) override {
        Dependency targetDep = ApplyKnownEcosystemMigrations(oldDep);
        
        DependencyChange change;
        change.oldDep = oldDep;
        change.skipAI = false; 
        
        if (targetDep.group != oldDep.group || targetDep.name != oldDep.name) {
            change.newDep = {targetDep.group, targetDep.name, newDep.version};
            change.hasPackageMove = true;
            change.releaseNotes = "Major Ecosystem Migration: Modernized to " + change.newDep.group + ":" + change.newDep.name;
            return change;
        }

        change.newDep = newDep;
        change.hasPackageMove = false;
        
        std::string groupPath = oldDep.group;
        std::replace(groupPath.begin(), groupPath.end(), '.', '/');
        
        // FIX: Fetch the NEW version's POM to see if it was relocated at this version!
        std::string pomUrl = "https://repo1.maven.org/maven2/" + 
                             groupPath + "/" + oldDep.name + "/" + newDep.version + "/" + 
                             oldDep.name + "-" + newDep.version + ".pom";
                             
        auto response = HttpClient::Get(pomUrl);
        
        if (response.statusCode == 200) {
            // FIX: Extract the specific relocation block so we don't grab parent POM coordinates
            std::string relocationBlock = ExtractXmlTag(response.body, "relocation");
            if (!relocationBlock.empty()) {
                change.hasPackageMove = true;
                std::string newGroupId = ExtractXmlTag(relocationBlock, "groupId");
                std::string newArtifactId = ExtractXmlTag(relocationBlock, "artifactId");
                
                change.newDep.group = newGroupId.empty() ? oldDep.group : newGroupId;
                change.newDep.name = newArtifactId.empty() ? oldDep.name : newArtifactId;
                change.releaseNotes = "Maven POM Relocation detected. Moved to " + change.newDep.group + ":" + change.newDep.name;
            } else {
                change.releaseNotes = "Version updated to " + newDep.version + ". Ensure API compatibility.";
            }
        } else {
            change.releaseNotes = "Version updated to " + newDep.version + ". Ensure API compatibility.";
        }

        return change;
    }

private:
    Dependency ApplyKnownEcosystemMigrations(const Dependency& dep) {
        for (const auto& migration : migrations) {
            if (dep.group == migration.oldGroup && dep.name == migration.oldName) {
                return {migration.newGroup, migration.newName, dep.version};
            }
        }
        return dep;
    }

    bool IsVersionGreater(const std::string& v1, const std::string& v2) {
        int i = 0, j = 0;
        int n1 = v1.length(), n2 = v2.length();
        
        while (i < n1 || j < n2) {
            long long num1 = 0, num2 = 0;
            bool hasNum1 = false, hasNum2 = false;
            
            while (i < n1 && !isdigit(v1[i])) i++;
            while (i < n1 && isdigit(v1[i])) {
                num1 = num1 * 10 + (v1[i] - '0');
                hasNum1 = true;
                i++;
            }
            
            while (j < n2 && !isdigit(v2[j])) j++;
            while (j < n2 && isdigit(v2[j])) {
                num2 = num2 * 10 + (v2[j] - '0');
                hasNum2 = true;
                j++;
            }
            
            if (hasNum1 && hasNum2) {
                if (num1 > num2) return true;
                if (num1 < num2) return false;
            } else if (hasNum1 && num1 > 0) {
                return true; 
            } else if (hasNum2 && num2 > 0) {
                return false; 
            }
        }
        return false;
    }

    std::string ExtractXmlTag(const std::string& xml, const std::string& tag) {
        std::string openTag = "<" + tag + ">";
        std::string closeTag = "</" + tag + ">";
        size_t start = xml.find(openTag);
        if (start == std::string::npos) return "";
        start += openTag.length();
        size_t end = xml.find(closeTag, start);
        if (end == std::string::npos) return "";
        return xml.substr(start, end - start);
    }
};