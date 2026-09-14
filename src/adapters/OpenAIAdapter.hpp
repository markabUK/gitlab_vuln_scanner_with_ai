#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include "../infrastructure/AiRetryStrategy.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

using json = nlohmann::json;

class OpenAIAdapter : public IAICodeAssistant {
private:
    std::string apiKey;

public:
    explicit OpenAIAdapter(const std::string& key) : apiKey(key) {}

    std::string GetProviderName() const override { return "OpenAI (GPT-4)"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        std::string prompt = 
            "You are an expert developer. A dependency was updated:\n"
            "From: " + request.changeDetails.oldDep.group + ":" + request.changeDetails.oldDep.name + " (" + request.changeDetails.oldDep.version + ")\n"
            "To: " + request.changeDetails.newDep.group + ":" + request.changeDetails.newDep.name + " (" + request.changeDetails.newDep.version + ")\n"
            "Release Notes/Moves: " + request.changeDetails.releaseNotes + "\n\n"
            "Refactor the following file.\n"
            "CRITICAL INSTRUCTION: If the code requires updates, output ONLY the raw refactored code without Markdown blocks.\n"
            "If the code is already fully compatible and requires NO changes, output exactly this text and nothing else: NO_CHANGES_NEEDED\n\n" +
            request.originalCode;

        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + apiKey},
            {"Content-Type", "application/json"}
        };

        // Lambda now only takes 'attempt'
        return AiRetryStrategy::Execute(3, 2000, request.originalCode, [&](int attempt) {
            json payload = {
                {"model", "gpt-4o"},
                {"messages", {{{"role", "user"}, {"content", prompt}}}},
                {"temperature", 0.0} 
            };

            spdlog::debug("[OpenAI] Sending completion request (Attempt {})", attempt);
            auto response = HttpClient::Post("https://api.openai.com/v1/chat/completions", payload.dump(), headers);

            if (response.statusCode == 200) {
                auto jsonResp = json::parse(response.body);
                return jsonResp["choices"][0]["message"]["content"].get<std::string>();
            }
            
            spdlog::error("[OpenAI] API failure. Status: {} - Body: {}", response.statusCode, response.body);
            return std::string(""); // Trigger retry on HTTP failure
        });
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>&) override {
        return "Automated dependency updates via OpenAI API.";
    }
};