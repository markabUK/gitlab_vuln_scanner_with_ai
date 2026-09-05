#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <map>
#include <vector>

using json = nlohmann::json;

class OllamaAdapter : public IAICodeAssistant {
private:
    std::string model;
    std::string endpoint;

public:
    explicit OllamaAdapter(const std::string& modelName = "qwen2.5-coder:7b", 
                           const std::string& url = "http://localhost:11434/api/chat")
        : model(modelName), endpoint(url) {}

    std::string GetProviderName() const override { return "Ollama (" + model + ")"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        int maxRetries = 3;
        int attempt = 1;
        bool isRetry = false;

        std::string systemInstructions = 
            "You are an automated, stateless code migration tool. "
            "Your ONLY task is to refactor the provided code to match the dependency update rules. "
            "You must rewrite the code completely, replacing all old library calls with the new ones. "
            "Never return the code unchanged. Do not include markdown code blocks or conversational explanations. "
            "Output ONLY the raw source code.";

        std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};

        while (attempt <= maxRetries) {
            std::string userPrompt = 
                "DEPENDENCY UPDATE RELEASE NOTES & MIGRATION RULES:\n" + request.changeDetails.releaseNotes + "\n\n";

                if (!request.customPromptContext.empty()) {
                userPrompt += "API MIGRATION CHEAT SHEET:\n" + request.customPromptContext + "\n\n";
            }

            if (isRetry) {
                userPrompt += "CRITICAL SYSTEM WARNING: In your previous attempt, you returned the exact original code without applying the migration. "
                              "You MUST find the outdated dependencies/APIs and rewrite them according to the rules above. DO NOT return the original code unchanged.\n\n";
            }

            userPrompt += "ORIGINAL CODE TO REFACTOR:\n" + request.originalCode;

            json payload = {
                {"model", model},
                {"messages", json::array({
                    {{"role", "system"}, {"content", systemInstructions}},
                    {{"role", "user"}, {"content", userPrompt}}
                })},
                {"stream", false},
                {"options", {
                    {"temperature", 0.0},     
                    {"num_ctx", 8192}        
                }}
            };

            auto res = HttpClient::Post(endpoint, payload.dump(), headers);

            if (res.statusCode != 200) {
                std::cerr << "[AI ERROR] Ollama call failed (Status " << res.statusCode << "): " << res.body << "\n";
                return request.originalCode;
            }

            try {
                auto data = json::parse(res.body);
                std::string rawOutput = data["message"]["content"].get<std::string>();

                std::string cleanedCode = rawOutput;
                if (cleanedCode.substr(0, 3) == "```") {
                    size_t start = cleanedCode.find('\n');
                    size_t end = cleanedCode.rfind("```");
                    if (start != std::string::npos && end != std::string::npos && end > start) {
                        cleanedCode = cleanedCode.substr(start + 1, end - start - 1);
                    }
                }

                if (cleanedCode != request.originalCode && !cleanedCode.empty()) {
                    return cleanedCode; // Success!
                }

                std::cout << "  [Ollama Retry] Model returned unchanged code. Forcing a retry (" << attempt << "/" << maxRetries << ")...\n";
                isRetry = true;
                attempt++;

            } catch (const std::exception& e) {
                std::cerr << "[AI ERROR] Failed to parse Ollama response: " << e.what() << "\n"
                          << "Full response: " << res.body << "\n";
                return request.originalCode;
            }
        }

        std::cout << "  [Ollama Warning] Model failed to modify the file after " << maxRetries << " attempts. Leaving file unchanged.\n";
        return request.originalCode;
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>&) override {
        return "Automated dependency updates via Local Ollama.";
    }
};