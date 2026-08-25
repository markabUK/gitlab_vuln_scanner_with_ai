#pragma once

#include "../domain/Interfaces.hpp"
#include "HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <algorithm>
#include <cctype>

class GoModulesRegistry : public IGoRegistry {
public:
    std::optional<std::string> GetLatestVersion(const Dependency& oldDep) override {
        // Go module names (the ID) are typically lowercase for the proxy
        std::string moduleId = oldDep.name;
        std::transform(moduleId.begin(), moduleId.end(), moduleId.begin(), ::tolower);
        
        // Query the Go proxy for the latest version info
        std::string url = "https://proxy.golang.org/" + moduleId + "/@latest";
        auto response = HttpClient::Get(url);
        
        if (response.statusCode != 200) {
            return std::nullopt;
        }

        try {
            auto jsonResp = nlohmann::json::parse(response.body);
            if (jsonResp.contains("Version") && jsonResp["Version"].is_string()) {
                return jsonResp["Version"].get<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse Go Proxy response for " << oldDep.name << ": " << e.what() << "\n";
        }
        return std::nullopt;
    }

    DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) override {
        DependencyChange change;
        change.oldDep = oldDep;
        change.newDep = newDep;
        change.hasPackageMove = false;
        change.releaseNotes = "Go module updated to " + newDep.version + ". Run `go mod tidy` after this merge.";
        change.skipAI = false; 
        return change;
    }
};