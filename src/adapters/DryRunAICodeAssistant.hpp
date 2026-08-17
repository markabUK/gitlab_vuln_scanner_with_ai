#pragma once

#include "../domain/Interfaces.hpp"
#include <iostream>
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
            std::cout << "[DRY RUN] Would call AI to refactor: " << request.filePath << "\n";
            loggedFiles.insert(request.filePath);
        }
        
        // Return original code so the workflow doesn't crash on empty data
        return request.originalCode; 
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>& appliedChanges) override {
        return "Offline Dry Run MR Description";
    }
};