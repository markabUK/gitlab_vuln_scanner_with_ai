#pragma once

#include "../domain/Interfaces.hpp"
#include "HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <algorithm> // for std::replace
#include <regex>     // for regex matching

using json = nlohmann::json;

class MavenCentralRegistry : public IMavenRegistry {
public:
    // 1. Finds the newest STABLE version number using the Maven Search API
    std::optional<std::string> GetLatestVersion(const Dependency& oldDep) override {
        // Bumped to rows=50 to ensure we find the matching classifier even in noisy releases
        std::string url = "https://search.maven.org/solrsearch/select?q=g:" + oldDep.group + 
                          "+AND+a:" + oldDep.name + "&core=gav&rows=50&wt=json&sort=timestamp+desc";
        
        auto response = HttpClient::Get(url);
        if (response.statusCode != 200) return std::nullopt;

        try {
            auto jsonResponse = json::parse(response.body);
            auto docs = jsonResponse["response"]["docs"];
            std::regex unstablePattern(R"((alpha|beta|rc|cr|preview|draft|-m\d+|\.m\d+))", std::regex_constants::icase);

            // 1. FOOLPROOF SUFFIX EXTRACTION (No regex used here)
            std::string requiredSuffix = "";
            std::string lowerOldVer = oldDep.version;
            std::transform(lowerOldVer.begin(), lowerOldVer.end(), lowerOldVer.begin(), ::tolower);
            
            // Explicitly look for known classifiers
            std::vector<std::string> classifiers = {"jre11", "jre17", "jre21", "jre8", "android"};
            for (const auto& cls : classifiers) {
                if (lowerOldVer.find(cls) != std::string::npos) {
                    requiredSuffix = cls;
                    break;
                }
            }

            // 2. FIND THE LATEST STABLE MATCHING SUFFIX
            for (const auto& doc : docs) {
                std::string version = doc["v"].get<std::string>();
                
                if (!std::regex_search(version, unstablePattern)) {
                    
                    // Enforce the suffix rule
                    if (!requiredSuffix.empty()) {
                        std::string lowerNewVer = version;
                        std::transform(lowerNewVer.begin(), lowerNewVer.end(), lowerNewVer.begin(), ::tolower);
                        
                        // If the old version was jre11, the new one MUST contain jre11
                        if (lowerNewVer.find(requiredSuffix) == std::string::npos) {
                            continue; // Skip this version, keep looping!
                        }
                    }
                    
                    return version;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "JSON Parsing error for Maven response: " << e.what() << "\n";
        }
        return std::nullopt;
    }
    
    // 2. Checks if the library was relocated by reading the raw pom.xml
    DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) override {
        DependencyChange change;
        change.oldDep = oldDep;
        change.newDep = newDep;
        
        // Construct the URL to the raw POM file on Maven Central
        // Format: https://repo1.maven.org/maven2/group/path/name/version/name-version.pom
        std::string groupPath = oldDep.group;
        std::replace(groupPath.begin(), groupPath.end(), '.', '/');
        
        std::string pomUrl = "https://repo1.maven.org/maven2/" + 
                             groupPath + "/" + oldDep.name + "/" + oldDep.version + "/" + 
                             oldDep.name + "-" + oldDep.version + ".pom";
                             
        // Fetch the POM file content
        auto response = HttpClient::Get(pomUrl);
        
        // Look for the <relocation> tag block
        if (response.statusCode == 200 && response.body.find("<relocation>") != std::string::npos) {
            change.hasPackageMove = true;
            
            // Extract the new coordinates from the XML
            std::string newGroupId = ExtractXmlTag(response.body, "groupId");
            std::string newArtifactId = ExtractXmlTag(response.body, "artifactId");
            
            // If the tag exists, update the dependency. Otherwise, keep the old one.
            change.newDep.group = newGroupId.empty() ? oldDep.group : newGroupId;
            change.newDep.name = newArtifactId.empty() ? oldDep.name : newArtifactId;
            
            change.releaseNotes = "Maven POM Relocation detected. Moved to " + change.newDep.group + ":" + change.newDep.name;
        } else {
            change.releaseNotes = "Version updated to " + newDep.version + ". Ensure API compatibility.";
        }

        return change;
    }

private:
    // Helper function to extract basic XML tags without needing a heavy XML library
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