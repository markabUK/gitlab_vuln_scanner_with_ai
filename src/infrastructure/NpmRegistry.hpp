#pragma once

#include "../domain/Interfaces.hpp"
#include "HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

class NpmRegistry : public INpmRegistry {
public:
    std::optional<std::string> GetLatestVersion(const Dependency& oldDep) override {
        // NPM registry API endpoint for package metadata
        std::string url = "https://registry.npmjs.org/" + oldDep.name;
        auto response = HttpClient::Get(url);
        
        if (response.statusCode != 200) {
            return std::nullopt;
        }

        try {
            auto jsonResp = nlohmann::json::parse(response.body);
            // NPM stores the active LTS or stable release in dist-tags.latest
            if (jsonResp.contains("dist-tags") && jsonResp["dist-tags"].contains("latest")) {
                return jsonResp["dist-tags"]["latest"].get<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse NPM response for " << oldDep.name << ": " << e.what() << "\n";
        }
        return std::nullopt;
    }

    DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) override {
        DependencyChange change;
        change.oldDep = oldDep;
        change.newDep = newDep;
        change.hasPackageMove = false;
        change.releaseNotes = "NPM package updated to " + newDep.version + ". Run `npm install` to update package-lock.json.";
        change.skipAI = false; 
        return change;
    }
};