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
    // Update default URL to use the structured /api/chat endpoint
    explicit OllamaAdapter(const std::string& modelName = "qwen2.5-coder:7b", 
                           const std::string& url = "http://localhost:11434/api/chat")
        : model(modelName), endpoint(url) {}

    std::string GetProviderName() const override { return "Ollama (" + model + ")"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        // System rules lock the model into "Code Refactoring" mode
        std::string systemInstructions = 
            "You are an automated, stateless code migration tool. "
            "Your ONLY task is to refactor the provided code to match the dependency update rules. "
            "You must rewrite the code completely, replacing all old library calls with the new ones. "
            "Never return the code unchanged. Do not include markdown code blocks or conversational explanations. "
            "Output ONLY the raw source code.";

        std::string userPrompt = 
            "DEPENDENCY UPDATE RELEASE NOTES & MIGRATION RULES:\n" + request.changeDetails.releaseNotes + "\n\n"
            "ORIGINAL CODE TO REFACTOR:\n" + request.originalCode;

        // Structured chat payload
        json payload = {
            {"model", model},
            {"messages", json::array({
                {{"role", "system"}, {"content", systemInstructions}},
                {{"role", "user"}, {"content", userPrompt}}
            })},
            {"stream", false},
            {"options", {
                {"temperature", 0.0},     // 0.0 forces deterministic, non-creative coding
                {"num_ctx", 8192}         // VITAL: Allocates enough RAM for large Java files
            }}
        };

        std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};
        auto res = HttpClient::Post(endpoint, payload.dump(), headers);

        if (res.statusCode != 200) {
            std::cerr << "[AI ERROR] Ollama call failed (Status " << res.statusCode << "): " << res.body << "\n";
            return request.originalCode;
        }

        try {
            auto data = json::parse(res.body);
            // /api/chat returns the text inside a nested message object
            return data["message"]["content"].get<std::string>();
        } catch (const std::exception& e) {
            std::cerr << "[AI ERROR] Failed to parse Ollama response: " << e.what() << "\n"
                      << "Full response: " << res.body << "\n";
            return request.originalCode;
        }
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>&) override {
        return "Automated dependency updates via Local Ollama.";
    }
};