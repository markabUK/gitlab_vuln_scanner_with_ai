#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include "../infrastructure/AiRetryStrategy.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

using json = nlohmann::json;

class OllamaAdapter : public IAICodeAssistant {
private:
    std::string model;
    std::string endpoint;
    int contextLength;
    int maxTokens;

public:
    explicit OllamaAdapter(const std::string& modelName = "qwen2.5-coder:7b", 
                           const std::string& url = "http://localhost:11434/api/chat",
                           int ctxLength = 32768,
                           int outputLength = 16384)
        : model(modelName), endpoint(url), contextLength(ctxLength), maxTokens(outputLength) {}

    std::string GetProviderName() const override { return "Ollama (" + model + ")"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        std::string systemInstructions = 
           "You are an automated code migration tool. "
           "CRITICAL: You must output the ENTIRE source file from start to finish. "
           "Do not summarize, truncate, or omit any methods, even if they are repetitive stubs. "
           "If no changes are needed, output EXACTLY: NO_CHANGES_NEEDED";

        std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};

        // Passed lambda no longer takes `isRetry`
        return AiRetryStrategy::Execute(3, 1000, request.originalCode, [&](int attempt) {
            std::string userPrompt = "RELEASE NOTES:\n" + request.changeDetails.releaseNotes + "\n\n";
            if (!request.customPromptContext.empty()) {
                userPrompt += "CHEAT SHEET:\n" + request.customPromptContext + "\n\n";
            }

            userPrompt += "ORIGINAL CODE:\n" + request.originalCode;

            json payload = {
                {"model", model},
                {"messages", json::array({
                    {{"role", "system"}, {"content", systemInstructions}},
                    {{"role", "user"}, {"content", userPrompt}}
                })},
                {"stream", false},
                {"options", { 
                    {"temperature", 0.0}, 
                    {"num_ctx", contextLength},
                    {"num_predict", maxTokens}
                }
                }
            };

            spdlog::debug("[Ollama] Sending payload to model: {} (Attempt {})", model, attempt);
            auto res = HttpClient::Post(endpoint, payload.dump(), headers);

            if (res.statusCode == 200) {
                auto data = json::parse(res.body);
                return data["message"]["content"].get<std::string>();
            }
            
            spdlog::error("[Ollama] API failure. Status: {} - Body: {}", res.statusCode, res.body);
            return std::string(""); // Trigger retry on HTTP failure
        });
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>&) override {
        return "Automated dependency updates via Local Ollama.";
    }
};