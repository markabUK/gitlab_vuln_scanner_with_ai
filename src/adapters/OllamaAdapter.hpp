#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <map>

using json = nlohmann::json;

class OllamaAdapter : public IAICodeAssistant {
private:
    std::string model;
    std::string endpoint;

    std::string CleanMarkdown(std::string text) {
        size_t start = text.find("```");
        if (start != std::string::npos) {
            size_t nl = text.find('\n', start);
            if (nl != std::string::npos) text.erase(start, (nl - start) + 1);
        }
        size_t end = text.rfind("```");
        if (end != std::string::npos) text.erase(end, 3);
        return text;
    }

public:
    explicit OllamaAdapter(const std::string& modelName = "qwen2.5-coder:7b", 
                           const std::string& url = "http://localhost:11434/api/generate")
        : model(modelName), endpoint(url) {}

    std::string GetProviderName() const override { return "Ollama (" + model + ")"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        std::string prompt = 
            "You are an expert developer. A dependency was updated:\n" +
            request.changeDetails.releaseNotes + "\n\n"
            "Refactor this file for compatibility. Return ONLY raw code without markdown:\n\n" +
            request.originalCode;

        json payload = {
            {"model", model},
            {"prompt", prompt},
            {"stream", false},
            {"options", {{"temperature", 0.0}}}
        };

        std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};
        auto res = HttpClient::Post(endpoint, payload.dump(), headers);

        if (res.statusCode != 200) {
            std::cerr << "[AI ERROR] Ollama call failed: " << res.body << "\n";
            return request.originalCode;
        }

        try {
            auto data = json::parse(res.body);
            return CleanMarkdown(data["response"].get<std::string>());
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