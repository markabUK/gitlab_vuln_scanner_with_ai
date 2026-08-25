#pragma once

#include <string>
#include <vector>
#include <fstream>
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

struct CodeReplacement {
    std::string search;
    std::string replace;
};

struct DependencyMigration {
    std::string oldGroup;
    std::string oldName;
    std::string newGroup;
    std::string newName;
    std::vector<CodeReplacement> replacements;
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
    std::string botEmail;           // NEW: Identifies commits made by the bot
    
    std::string aiProvider;
    std::string geminiApiKey;
    std::string openAiApiKey;
    std::string ollamaEndpoint;
    std::string ollamaModel;
    
    std::string googleChatWebhook;  // NEW: Webhook URL for Google Chat space
    
    TargetConfig target;
    std::vector<RegistryConfig> registries;
    std::vector<DependencyMigration> migrations;

    static AppSettings Load(const std::string& configPath) {
        if (!std::filesystem::exists(configPath)) {
            throw std::runtime_error("Configuration file not found: " + configPath);
        }

        std::ifstream file(configPath);
        json j;
        file >> j;

        AppSettings settings;

        settings.gitlabHost = j.value("GitLab", json::object()).value("Host", "https://gitlab.com");
        settings.gitlabToken = j.value("GitLab", json::object()).value("Token", "");
        settings.botEmail = j.value("GitLab", json::object()).value("BotEmail", "bot@dependencyupdater.local");

        settings.aiProvider = j.value("AI", json::object()).value("Provider", "GEMINI");
        settings.geminiApiKey = j.value("AI", json::object()).value("GeminiApiKey", "");
        settings.openAiApiKey = j.value("AI", json::object()).value("OpenAIApiKey", "");
        settings.ollamaEndpoint = j.value("AI", json::object()).value("OllamaEndpoint", "http://localhost:11434/api/generate");
        settings.ollamaModel = j.value("AI", json::object()).value("OllamaModel", "qwen2.5-coder:7b");

        settings.googleChatWebhook = j.value("Notifications", json::object()).value("GoogleChatWebhook", "");

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

        for (const auto& regJson : j["Registries"]) {
            RegistryConfig reg;
            reg.type = regJson.value("Type", "MavenCentral");
            reg.url = regJson.value("Url", "");
            reg.token = regJson.value("Token", "");
            for (const auto& prefix : regJson["GroupPrefixes"]) {
                reg.groupPrefixes.push_back(prefix.get<std::string>());
            }
            settings.registries.push_back(reg);
        }

        if (j.contains("Migrations")) {
            for (const auto& mJson : j["Migrations"]) {
                DependencyMigration dm;
                dm.oldGroup = mJson.value("OldGroup", "");
                dm.oldName = mJson.value("OldName", "");
                dm.newGroup = mJson.value("NewGroup", "");
                dm.newName = mJson.value("NewName", "");
                
                if (mJson.contains("Replacements")) {
                    for (const auto& repJson : mJson["Replacements"]) {
                        CodeReplacement cr;
                        cr.search = repJson.value("Search", "");
                        cr.replace = repJson.value("Replace", "");
                        dm.replacements.push_back(cr);
                    }
                }
                settings.migrations.push_back(dm);
            }
        }

        return settings;
    }
};