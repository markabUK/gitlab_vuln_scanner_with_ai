#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include "../infrastructure/AiRetryStrategy.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
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

    CommentSyntax GetCommentSyntax(const std::string& filePath) {
        if (filePath.ends_with(".xml") || filePath.ends_with(".html")) return {"<!-- ", " -->"};
        if (filePath.ends_with(".yml") || filePath.ends_with(".yaml") || 
            filePath.ends_with(".properties") || filePath.ends_with(".sh") || 
            filePath.ends_with(".py")) return {"# ", ""};
        return {"// ", ""};
    }

public:
    GitLabDuoAdapter(const std::string& host, const std::string& token, const std::string& project = "")
        : gitlabHost(host), gitlabToken(token), projectPath(project) {}

    std::string GetProviderName() const override { return "GitLab Duo"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        CommentSyntax cs = GetCommentSyntax(request.filePath);
        std::string contentAbove =
            cs.prefix + "TASK: REFACTOR ENTIRE FILE FOR DEPENDENCY UPDATE" + cs.suffix + "\n" +
            cs.prefix + "From: " + request.changeDetails.oldDep.group + ":" + request.changeDetails.oldDep.name +
            " (" + request.changeDetails.oldDep.version + ")" + cs.suffix + "\n" +
            cs.prefix + "To: " + request.changeDetails.newDep.group + ":" + request.changeDetails.newDep.name +
            " (" + request.changeDetails.newDep.version + ")" + cs.suffix + "\n" +
            cs.prefix + "Notes: " + request.changeDetails.releaseNotes + cs.suffix + "\n" +
            cs.prefix + "If the code is already fully compatible and requires NO changes, output exactly: NO_CHANGES_NEEDED" + cs.suffix + "\n" +
            cs.prefix + "--- ORIGINAL CODE START ---" + cs.suffix + "\n\n" +
            request.originalCode + "\n\n" +
            cs.prefix + "--- REFACTORED CODE START ---" + cs.suffix + "\n";

        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + gitlabToken},
            {"Content-Type", "application/json"}
        };

        // Lambda now only takes 'attempt'
        return AiRetryStrategy::Execute(3, 2000, request.originalCode, [&](int attempt) {
            json payload = {
                {"current_file", {
                    {"file_name", request.filePath},
                    {"content_above_cursor", contentAbove},
                    {"content_below_cursor", ""}
                }},
                {"intent", "completion"},
                {"stream", false},
                {"max_new_tokens", 4096}
            };
            if (!projectPath.empty()) payload["project_path"] = projectPath;

            spdlog::debug("[GitLab Duo] Requesting completion for {} (Attempt {})", request.filePath, attempt);
            auto response = HttpClient::Post(gitlabHost + "/api/v4/code_suggestions/completions", payload.dump(), headers);

            if (response.statusCode == 200) {
                auto jsonResp = json::parse(response.body);
                if (jsonResp["choices"].empty()) {
                    spdlog::debug("[GitLab Duo] No choices returned by model.");
                    return request.originalCode;
                }
                return jsonResp["choices"][0]["text"].get<std::string>();
            }
            
            spdlog::error("[GitLab Duo] API failure. Status: {} - Body: {}", response.statusCode, response.body);
            return std::string(""); // Trigger retry on HTTP failure
        });
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>&) override {
        return "Automated dependency updates via GitLab Duo.";
    }
};