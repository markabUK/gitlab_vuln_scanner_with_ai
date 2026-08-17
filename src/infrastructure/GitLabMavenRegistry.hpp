#pragma once

#include "../domain/Interfaces.hpp"
#include "HttpClient.hpp"
#include <string>
#include <regex>
#include <iostream>

class GitLabMavenRegistry : public IMavenRegistry {
private:
    std::string baseUrl;
    std::string token;

    std::string ReplaceDotsWithSlashes(std::string str) {
        for (char& c : str) {
            if (c == '.') c = '/';
        }
        return str;
    }

public:
    GitLabMavenRegistry(const std::string& url, const std::string& pat) 
        : baseUrl(url), token(pat) {}

    std::optional<std::string> GetLatestVersion(const Dependency& dep) override {
        // Maven metadata path: /group/id/artifact/maven-metadata.xml
        std::string url = baseUrl + "/" + ReplaceDotsWithSlashes(dep.group) + "/" + dep.name + "/maven-metadata.xml";
        
        std::map<std::string, std::string> headers = {
            {"Private-Token", token}
        };

        auto response = HttpClient::Get(url, headers);
        
        if (response.statusCode != 200) {
            return std::nullopt;
        }

        // Parse XML for <latest> tag
        std::regex latestRegex(R"(<latest>([^<]+)</latest>)");
        std::smatch match;
        if (std::regex_search(response.body, match, latestRegex)) {
            return match[1].str();
        }

        return std::nullopt;
    }

    DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) override {
        DependencyChange diff;
        diff.oldDep = oldDep;
        diff.newDep = newDep;
        diff.hasPackageMove = false;
        diff.releaseNotes = "Internal GitLab Registry update. No public release notes available.";
        diff.skipAI = true; // <-- CRITICAL: Skip AI processing for internal proprietary code
        
        return diff;
    }
};