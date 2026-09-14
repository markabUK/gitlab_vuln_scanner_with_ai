#pragma once

#include "../domain/Models.hpp"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <filesystem>

using json = nlohmann::json;

struct RegistryConfig {
    std::string type;
    std::string url;
    std::string token;
    std::vector<std::string> groupPrefixes;
};

struct TargetConfig {
    std::string type; 
    std::string id;   
    std::vector<std::string> excludeGroups;
    std::vector<std::string> excludeProjects;
};

class AppSettings {
public:
    std::string gitlabHost;
    std::string gitlabToken;
    std::string botEmail;
    
    std::string aiProvider;
    std::string geminiApiKey;
    std::string openAiApiKey;
    std::string ollamaEndpoint;
    std::string ollamaModel;
    int ollamaContextLength = 32768;
    int ollamaOutputLength = 16384;
    
    std::string googleChatWebhook;
    
    TargetConfig target;
    std::vector<RegistryConfig> registries;
    
    std::map<std::string, std::vector<DependencyMigration>> migrations;

    static AppSettings Load(const std::string& configPath) {
        if (!std::filesystem::exists(configPath)) {
            throw std::runtime_error("Configuration file not found: " + configPath);
        }

        std::ifstream file(configPath);
        json j;
        file >> j;

        AppSettings settings;

        ParseGitLab(j, settings);
        ParseAI(j, settings);
        ParseNotifications(j, settings);
        ParseTarget(j, settings);
        ParseRegistries(j, settings);
        ParseMigrations(j, settings);

        return settings;
    }

private:
    static void ParseGitLab(const json& j, AppSettings& settings) {
        json glJson = j.value("GitLab", json::object());
        settings.gitlabHost = glJson.value("Host", "https://gitlab.com");
        settings.gitlabToken = glJson.value("Token", "");
        settings.botEmail = glJson.value("BotEmail", "bot@dependencyupdater.local");
    }

    static void ParseAI(const json& j, AppSettings& settings) {
        json aiJson = j.value("AI", json::object());
        settings.aiProvider = aiJson.value("Provider", "GEMINI");
        settings.geminiApiKey = aiJson.value("GeminiApiKey", "");
        settings.openAiApiKey = aiJson.value("OpenAIApiKey", "");
        settings.ollamaEndpoint = aiJson.value("OllamaEndpoint", "http://localhost:11434/api/generate");
        settings.ollamaModel = aiJson.value("OllamaModel", "qwen2.5-coder:7b");
        settings.ollamaContextLength = aiJson.value("OllamaContextLength", 32768);
    }

    static void ParseNotifications(const json& j, AppSettings& settings) {
        json notifJson = j.value("Notifications", json::object());
        settings.googleChatWebhook = notifJson.value("GoogleChatWebhook", "");
    }

    static void ParseTarget(const json& j, AppSettings& settings) {
        json targetJson = j.value("Target", json::object());
        settings.target.type = targetJson.value("Type", "Group");
        settings.target.id = targetJson.value("Id", "");
        
        if (targetJson.contains("ExcludeGroups")) {
            for (const auto& eg : targetJson["ExcludeGroups"]) {
                settings.target.excludeGroups.push_back(eg.get<std::string>());
            }
        }
        
        if (targetJson.contains("ExcludeProjects")) {
            for (const auto& ep : targetJson["ExcludeProjects"]) {
                settings.target.excludeProjects.push_back(ep.get<std::string>());
            }
        }
    }

    static void ParseRegistries(const json& j, AppSettings& settings) {
        if (!j.contains("Registries")) return;

        for (const auto& regJson : j["Registries"]) {
            RegistryConfig reg;
            reg.type = regJson.value("Type", "MavenCentral");
            reg.url = regJson.value("Url", "");
            reg.token = regJson.value("Token", "");
            if (regJson.contains("GroupPrefixes")) {
                for (const auto& prefix : regJson["GroupPrefixes"]) {
                    reg.groupPrefixes.push_back(prefix.get<std::string>());
                }
            }
            settings.registries.push_back(reg);
        }
    }

    static void ParseMigrations(const json& j, AppSettings& settings) {
        if (!j.contains("Migrations")) return;

        for (auto it = j["Migrations"].begin(); it != j["Migrations"].end(); ++it) {
            std::string ecosystem = it.key();
            
            auto& migrationList = settings.migrations[ecosystem];

            for (const auto& mJson : it.value()) {
                DependencyMigration dm;
                dm.oldGroup = mJson.value("OldGroup", "");
                dm.oldName = mJson.value("OldName", "");
                dm.newGroup = mJson.value("NewGroup", "");
                dm.newName = mJson.value("NewName", "");
                
                dm.maxOldVersion = mJson.value("MaxOldVersion", "");
                dm.minNewVersion = mJson.value("MinNewVersion", "");
                
                dm.migrationDocPath = mJson.value("MigrationDocPath", "");
                if (!dm.migrationDocPath.empty()) {
                    if (std::filesystem::exists(dm.migrationDocPath)) {
                        std::ifstream docFile(dm.migrationDocPath);
                        std::stringstream buffer;
                        buffer << docFile.rdbuf();
                        dm.migrationDocContent = buffer.str();
                    } else {
                        spdlog::warn("MigrationDocPath not found: {}", dm.migrationDocPath);
                    }
                }
                
                if (mJson.contains("Replacements")) {
                    for (const auto& repJson : mJson["Replacements"]) {
                        CodeReplacement cr;
                        cr.search = repJson.value("Search", "");
                        cr.replace = repJson.value("Replace", "");
                        dm.replacements.push_back(cr);
                    }
                }
                migrationList.push_back(dm);
            }
        }
    }
};