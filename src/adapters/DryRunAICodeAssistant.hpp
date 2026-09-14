#pragma once

#include "../domain/Interfaces.hpp"
#include <spdlog/spdlog.h>
#include <unordered_set>

class DryRunAICodeAssistant : public IAICodeAssistant {
private:
    std::unordered_set<std::string> loggedFiles;

public:
    std::string GetProviderName() const override { 
        return "Dry-Run Simulator (Offline)"; 
    }

    std::string RefactorCode(const RefactorRequest& request) override {
        // Only print the log once per file to avoid terminal spam
        if (loggedFiles.find(request.filePath) == loggedFiles.end()) {
            spdlog::info("[DRY RUN] Would call AI to refactor: {}", request.filePath);
            loggedFiles.insert(request.filePath);
        }
        
        // Return original code so the workflow doesn't crash on empty data
        return request.originalCode; 
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>& appliedChanges) override {
        return "Offline Dry Run MR Description";
    }
};