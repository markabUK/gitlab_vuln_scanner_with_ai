#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include "../infrastructure/AiRetryStrategy.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

using json = nlohmann::json;

class GeminiAdapter : public IAICodeAssistant {
private:
    std::string apiKey;
    std::chrono::steady_clock::time_point lastRequestTime;

    void EnforceRateLimit() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRequestTime).count();
        if (elapsed < 4500) {
            std::this_thread::sleep_for(std::chrono::milliseconds(4500 - elapsed));
        }
        lastRequestTime = std::chrono::steady_clock::now();
    }

public:
    explicit GeminiAdapter(const std::string& key) : apiKey(key) {
        lastRequestTime = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    }

    std::string GetProviderName() const override { return "Google Gemini (gemini-3.7-flash)"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        std::string basePrompt = 
            "You are an expert developer. A dependency was updated:\n"
            "From: " + request.changeDetails.oldDep.group + ":" + request.changeDetails.oldDep.name + 
            " (" + request.changeDetails.oldDep.version + ")\n"
            "To: " + request.changeDetails.newDep.version + "\n"
            "Notes: " + request.changeDetails.releaseNotes + "\n\n"
            "Refactor the following file to be compatible with the new version. "
            "CRITICAL INSTRUCTION: Output ONLY the raw refactored code.\n\n"
            "--- ORIGINAL CODE ---\n" + request.originalCode;

        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.7-flash:generateContent";
        std::map<std::string, std::string> headers = {
            {"x-goog-api-key", apiKey},
            {"Content-Type", "application/json"}
        };

        return AiRetryStrategy::Execute(5, 2000, request.originalCode, [&](int attempt, bool isRetry) {
            EnforceRateLimit();

            std::string prompt = basePrompt;
            if (isRetry) {
                prompt += "\n\nCRITICAL WARNING: You previously returned the code unchanged. You MUST apply the updates.";
            }

            json payload = {
                {"contents", {{ {"role", "user"}, {"parts", {{ {"text", prompt} }}} }}},
                {"generationConfig", { {"temperature", 0.0} }}
            };

            auto response = HttpClient::Post(url, payload.dump(), headers);

            if (response.statusCode == 200) {
                auto jsonResp = json::parse(response.body);
                // Return raw string; strategy handles cleaning
                return jsonResp["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
            }
            
            if (response.statusCode == 503 || response.statusCode == 429) {
                return std::string(""); // Trigger backoff logic in strategy
            }
            
            throw std::runtime_error("Gemini API unrecoverable status " + std::to_string(response.statusCode));
        });
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>&) override {
        return "Automated dependency updates via Google Gemini API.";
    }
};