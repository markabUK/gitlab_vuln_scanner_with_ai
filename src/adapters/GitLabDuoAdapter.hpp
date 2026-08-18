#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include "../infrastructure/StringUtils.hpp" // <-- Include shared utility
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using json = nlohmann::json;

class GitLabDuoAdapter : public IAICodeAssistant {
private:
    std::string gitlabHost;
    std::string gitlabToken;
    std::string projectPath;

    struct CommentSyntax {
        std::string prefix;
        std::string suffix;
    };

    // Dynamically apply the correct comment syntax to prevent the AI 
    // from hallucinating the wrong programming language.
    CommentSyntax GetCommentSyntax(const std::string& filePath) {
        if (filePath.ends_with(".xml") || filePath.ends_with(".html")) {
            return {"<!-- ", " -->"};
        }
        if (filePath.ends_with(".yml") || filePath.ends_with(".yaml") || 
            filePath.ends_with(".properties") || filePath.ends_with(".sh") || 
            filePath.ends_with(".py")) {
            return {"# ", ""};
        }
        // Default for Java, Kotlin, C++, Gradle, etc.
        return {"// ", ""};
    }

public:
    GitLabDuoAdapter(const std::string& host, const std::string& token, const std::string& project = "")
        : gitlabHost(host), gitlabToken(token), projectPath(project) {}

    std::string GetProviderName() const override { return "GitLab Duo"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        CommentSyntax cs = GetCommentSyntax(request.filePath);

        // Build a prompt that uses the correct comment syntax for the file type
        std::string contentAbove =
            cs.prefix + "=====================================================================" + cs.suffix + "\n" +
            cs.prefix + "TASK: REFACTOR ENTIRE FILE FOR DEPENDENCY UPDATE" + cs.suffix + "\n" +
            cs.prefix + "Dependency updated:" + cs.suffix + "\n" +
            cs.prefix + "From: " + request.changeDetails.oldDep.group + ":" + request.changeDetails.oldDep.name +
            " (" + request.changeDetails.oldDep.version + ")" + cs.suffix + "\n" +
            cs.prefix + "To: " + request.changeDetails.newDep.group + ":" + request.changeDetails.newDep.name +
            " (" + request.changeDetails.newDep.version + ")" + cs.suffix + "\n" +
            cs.prefix + "Notes: " + request.changeDetails.releaseNotes + cs.suffix + "\n" +
            cs.prefix + "CRITICAL INSTRUCTIONS:" + cs.suffix + "\n" +
            cs.prefix + "1. Output the ENTIRE completely refactored file. Do not stop early." + cs.suffix + "\n" +
            cs.prefix + "2. DO NOT use placeholders. Output raw code only." + cs.suffix + "\n" +
            cs.prefix + "=====================================================================" + cs.suffix + "\n" +
            cs.prefix + "--- ORIGINAL CODE START ---" + cs.suffix + "\n\n" +
            request.originalCode + "\n\n" +
            cs.prefix + "--- ORIGINAL CODE END ---" + cs.suffix + "\n" +
            cs.prefix + "--- REFACTORED CODE START ---" + cs.suffix + "\n";

        json payload = {
            {"current_file", {
                {"file_name", request.filePath},
                {"content_above_cursor", contentAbove},
                {"content_below_cursor", ""}
            }},
            {"intent", "completion"},
            {"stream", false},
            {"max_new_tokens", 4096} // Attempt to force maximum output length to prevent truncation
        };
        
        if (!projectPath.empty()) {
            payload["project_path"] = projectPath;
        }

        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + gitlabToken},
            {"Content-Type", "application/json"}
        };

        auto response = HttpClient::Post(gitlabHost + "/api/v4/code_suggestions/completions",
            payload.dump(), headers);

        if (response.statusCode != 200) {
            std::cerr << "[AI ERROR] GitLab Duo API failed with status " << response.statusCode
                      << "\nResponse: " << response.body << "\n";
            return request.originalCode;
        }

        try {
            auto jsonResp = json::parse(response.body);

            if (jsonResp["choices"].empty()) {
                std::cout << " [DUO] No code changes needed for " << request.filePath << "\n";
                return request.originalCode;
            }

            std::string aiCode = jsonResp["choices"][0]["text"].get<std::string>();
            
            // <-- Use centralized cleaner, passing base code for safety checks
            return StringUtils::CleanAIOutput(aiCode, request.originalCode); 
        } catch (const std::exception& e) {
            std::cerr << "[AI ERROR] Failed to parse GitLab Duo response: " << e.what()
                      << "\nFull response: " << response.body << "\n";
            return request.originalCode;
        }
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>& changes) override {
        std::string desc = "## Dependency Updates\n\nAutomated dependency updates with "
                           "AI-assisted code refactoring via GitLab Duo.\n\n### Changes\n";
        for (const auto& change : changes) {
            desc += "- **" + change.oldDep.group + ":" + change.oldDep.name + "** `" +
                    change.oldDep.version + "` → `" + change.newDep.version + "`\n";
        }
        return desc;
    }
};