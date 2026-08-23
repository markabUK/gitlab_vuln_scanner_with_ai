#pragma once
#include "../domain/Interfaces.hpp"
#include "HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <algorithm>
#include <cctype>

class NuGetV3Registry : public INuGetRegistry {
public:
    std::optional<std::string> GetLatestVersion(const Dependency& oldDep) override {
        // The V3 API requires the package ID to be lowercased
        std::string lowerId = oldDep.name;
        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
        
        // Query the V3 Flat Container endpoint
        std::string url = "https://api.nuget.org/v3-flatcontainer/" + lowerId + "/index.json";
        auto response = HttpClient::Get(url);
        
        if (response.statusCode != 200) {
            return std::nullopt;
        }

        try {
            auto jsonResp = nlohmann::json::parse(response.body);
            // The response contains a JSON array of version strings
            if (jsonResp.contains("versions") && jsonResp["versions"].is_array()) {
                auto versions = jsonResp["versions"];
                if (!versions.empty()) {
                    // Versions are typically returned chronologically
                    return versions.back().get<std::string>();
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse NuGet V3 response: " << e.what() << "\n";
        }
        return std::nullopt;
    }

    DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) override {
        DependencyChange change;
        change.oldDep = oldDep;
        change.newDep = newDep;
        change.hasPackageMove = false;
        change.releaseNotes = "NuGet package updated to " + newDep.version + ". Check for breaking API changes.";
        change.skipAI = false; 
        return change;
    }
};