#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include "../infrastructure/AiRetryStrategy.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

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
        std::string systemInstructions = 
            "You are an automated code migration tool. "
            "You must rewrite the code replacing old library calls with new ones. "
            "Never return the code unchanged. Output ONLY raw source code.";

        std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};

        return AiRetryStrategy::Execute(3, 1000, request.originalCode, [&](int attempt, bool isRetry) {
            std::string userPrompt = "RELEASE NOTES:\n" + request.changeDetails.releaseNotes + "\n\n";
            if (!request.customPromptContext.empty()) userPrompt += "CHEAT SHEET:\n" + request.customPromptContext + "\n\n";

            if (isRetry) {
                userPrompt += "CRITICAL SYSTEM WARNING: In your previous attempt, you returned the exact original code. "
                              "You MUST find the outdated APIs and rewrite them. DO NOT return the original code unchanged.\n\n";
            }
            userPrompt += "ORIGINAL CODE:\n" + request.originalCode;

            json payload = {
                {"model", model},
                {"messages", json::array({
                    {{"role", "system"}, {"content", systemInstructions}},
                    {{"role", "user"}, {"content", userPrompt}}
                })},
                {"stream", false},
                {"options", { {"temperature", 0.0}, {"num_ctx", 8192} }}
            };

            auto res = HttpClient::Post(endpoint, payload.dump(), headers);

            if (res.statusCode == 200) {
                auto data = json::parse(res.body);
                return data["message"]["content"].get<std::string>();
            }
            return std::string(""); // Trigger retry on HTTP failure
        });
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>&) override {
        return "Automated dependency updates via Local Ollama.";
    }
};