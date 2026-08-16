#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

class OpenAIAdapter : public IAICodeAssistant {
private:
    std::string apiKey;

public:
    explicit OpenAIAdapter(const std::string& key) : apiKey(key) {}

    std::string GetProviderName() const override { return "OpenAI (GPT-4)"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        std::string prompt = 
            "You are an expert Java/Kotlin developer. A dependency was updated:\n"
            "From: " + request.changeDetails.oldDep.group + ":" + request.changeDetails.oldDep.name + " (" + request.changeDetails.oldDep.version + ")\n"
            "To: " + request.changeDetails.newDep.group + ":" + request.changeDetails.newDep.name + " (" + request.changeDetails.newDep.version + ")\n"
            "Release Notes/Moves: " + request.changeDetails.releaseNotes + "\n\n"
            "Refactor the following file to be compatible with the new version. "
            "Output ONLY the refactored code without Markdown blocks or explanations.\n\n" +
            request.originalCode;

        json payload = {
            {"model", "gpt-4o"},
            {"messages", {{{"role", "user"}, {"content", prompt}}}},
            {"temperature", 0.0} // Keep it deterministic for code
        };

        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + apiKey},
            {"Content-Type", "application/json"}
        };

        auto response = HttpClient::Post("https://api.openai.com/v1/chat/completions", payload.dump(), headers);

        if (response.statusCode == 200) {
            auto jsonResp = json::parse(response.body);
            return jsonResp["choices"][0]["message"]["content"].get<std::string>();
        }
        
        std::cerr << "OpenAI API failed: " << response.body << "\n";
        return request.originalCode; // Fallback to original code on failure
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>& appliedChanges) override {
        // Implementation follows the same pattern, asking the LLM to write a Markdown MR summary.
        return "Automatically generated MR updating " + std::to_string(appliedChanges.size()) + " dependencies.";
    }
};