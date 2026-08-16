#pragma once

#include "../domain/Interfaces.hpp"
#include "../infrastructure/HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

class GitLabDuoAdapter : public IAICodeAssistant {
private:
    std::string gitlabHost;
    std::string gitlabToken;
    std::string projectPath; // optional, e.g. "group/project"

    // Safety net: strip markdown fences if the model includes them despite instructions.
    std::string CleanMarkdownCodeBlocks(std::string text) {
        size_t startTick = text.find("```");
        if (startTick != std::string::npos) {
            size_t endOfLine = text.find('\n', startTick);
            if (endOfLine != std::string::npos)
                text.erase(startTick, (endOfLine - startTick) + 1);
        }
        size_t endTick = text.rfind("```");
        if (endTick != std::string::npos)
            text.erase(endTick, 3);
        return text;
    }

public:
    GitLabDuoAdapter(const std::string& host, const std::string& token, const std::string& project = "")
        : gitlabHost(host), gitlabToken(token), projectPath(project) {}

    std::string GetProviderName() const override { return "GitLab Duo"; }

    std::string RefactorCode(const RefactorRequest& request) override {
        // intent:"completion" uses a synchronous text model (e.g. Codestral) that populates
        // choices[0].text. intent:"generation" invokes the async Code Generations Agent which
        // returns choices:[] empty. Embed the instruction as a comment above the code so the
        // completion model sees it as context — user_instruction is not supported here.
        std::string contentAbove =
            "/* REFACTOR TASK\n"
            " * Dependency updated:\n"
            " *   From: " + request.changeDetails.oldDep.group + ":" + request.changeDetails.oldDep.name +
            " (" + request.changeDetails.oldDep.version + ")\n"
            " *   To:   " + request.changeDetails.newDep.group + ":" + request.changeDetails.newDep.name +
            " (" + request.changeDetails.newDep.version + ")\n"
            " *   Notes: " + request.changeDetails.releaseNotes + "\n"
            " * Output ONLY the complete refactored file. No explanations, no markdown.\n"
            " */\n\n"
            + request.originalCode;

        json payload = {
            {"current_file", {
                {"file_name", request.filePath},
                {"content_above_cursor", contentAbove},
                {"content_below_cursor", ""}
            }},
            {"intent", "completion"},
            {"stream", false}
        };
        if (!projectPath.empty())
            payload["project_path"] = projectPath;

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
                // Empty choices means the model found nothing to change — this is normal
                // for minor version bumps with no breaking API changes.
                std::cout << "  [DUO] No code changes needed for " << request.filePath << "\n";
                return request.originalCode;
            }

            std::string aiCode = jsonResp["choices"][0]["text"].get<std::string>();
            return CleanMarkdownCodeBlocks(aiCode);
        } catch (const std::exception& e) {
            std::cerr << "[AI ERROR] Failed to parse GitLab Duo response: " << e.what()
                      << "\nFull response: " << response.body << "\n";
            return request.originalCode;
        }
    }

    std::string GenerateMergeRequestDescription(const std::vector<DependencyChange>& changes) override {
        std::string desc = "## Dependency Updates\n\nAutomated dependency updates with "
                           "AI-assisted code refactoring via GitLab Duo.\n\n### Changes\n";
        for (const auto& change : changes)
            desc += "- **" + change.oldDep.group + ":" + change.oldDep.name + "** `" +
                    change.oldDep.version + "` → `" + change.newDep.version + "`\n";
        return desc;
    }
};