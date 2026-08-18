#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include "../infrastructure/StringUtils.hpp" // <-- Include shared utility
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
        
        // Ensure at least 4500ms (4.5s) between requests -> max ~13 requests/min
        if (elapsed < 4500) {
            std::this_thread::sleep_for(std::chrono::milliseconds(4500 - elapsed));
        }
        lastRequestTime = std::chrono::steady_clock::now();
    }

public:
    explicit GeminiAdapter(const std::string& key) : apiKey(key) {
        // Initialize with a past time so the very first request fires immediately
        lastRequestTime = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    }

    std::string GetProviderName() const override { return "Google Gemini (gemini-3.7-flash)"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        std::string prompt = 
            "You are an expert developer. A dependency was updated:\n"
            "From: " + request.changeDetails.oldDep.group + ":" + request.changeDetails.oldDep.name + 
            " (" + request.changeDetails.oldDep.version + ")\n"
            "To: " + request.changeDetails.newDep.version + "\n"
            "Notes: " + request.changeDetails.releaseNotes + "\n\n"
            "Refactor the following file to be compatible with the new version. "
            "CRITICAL INSTRUCTION: Output ONLY the raw refactored code. Do not include markdown formatting, "
            "explanations, or placeholders. Return the entire, fully-functional file.\n\n"
            "--- ORIGINAL CODE ---\n" +
            request.originalCode;

        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.7-flash:generateContent";

        json payload = {
            {"contents", {{
                {"role", "user"},
                {"parts", {{ {"text", prompt} }}}
            }}},
            {"generationConfig", {
                {"temperature", 0.0}
            }}
        };

        std::map<std::string, std::string> headers = {
            {"x-goog-api-key", apiKey},
            {"Content-Type", "application/json"}
        };

        int maxRetries = 5;
        int backoffMs = 2000;

        for (int attempt = 1; attempt <= maxRetries; ++attempt) {
            EnforceRateLimit(); // Block here until 4.5s have passed since the last request
            
            auto response = HttpClient::Post(url, payload.dump(), headers);

            if (response.statusCode == 200) {
                try {
                    auto jsonResp = json::parse(response.body);
                    std::string aiCode = jsonResp["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
                    
                    // <-- Use centralized cleaner, passing base code for safety checks
                    return StringUtils::CleanAIOutput(aiCode, request.originalCode); 
                } catch (const std::exception& e) {
                    std::cerr << "[AI ERROR] Failed to parse Gemini response: " << e.what() << "\n"
                              << "Full response: " << response.body << "\n";
                    return request.originalCode;
                }
            }

            if (response.statusCode == 503 || response.statusCode == 429) {
                std::cerr << "[AI WARNING] Gemini API returned " << response.statusCode 
                          << " (Attempt " << attempt << " of " << maxRetries << "). Retrying in " 
                          << (backoffMs / 1000.0) << " seconds...\n";
                
                if (attempt == maxRetries) {
                    std::cerr << "[AI ERROR] Max retries reached. Server is too busy. Skipping file.\n";
                    return request.originalCode;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
                backoffMs *= 2;
            } else {
                std::cerr << "[AI ERROR] Gemini API failed with unrecoverable status " << response.statusCode << "\n"
                          << "Response: " << response.body << "\n";
                return request.originalCode;
            }
        }

        return request.originalCode;
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>& changes) override {
        return "Automated dependency updates via Google Gemini API.";
    }
};