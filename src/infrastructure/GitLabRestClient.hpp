#pragma once

#include "../domain/Interfaces.hpp"
#include "HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

class GitLabRestClient : public IGitLabClient {
private:
    std::string baseUrl;
    std::string apiToken;

    std::map<std::string, std::string> GetAuthHeaders() {
        return {
            {"PRIVATE-TOKEN", apiToken},
            {"Content-Type", "application/json"}
        };
    }

public:
    GitLabRestClient(const std::string& url, const std::string& token)
        : baseUrl(url), apiToken(token) {}

    std::vector<ProjectContext> GetProjectsInGroup(const std::string& groupId) override {
        std::vector<ProjectContext> projects;
        int page = 1;
        bool hasMore = true;

        std::cout << "Fetching all projects in group " << groupId << " (including subgroups)...\n";

        while (hasMore) {
            std::string url = baseUrl + "/api/v4/groups/" + groupId +
                              "/projects?include_subgroups=true&per_page=100&page=" + std::to_string(page);

            auto response = HttpClient::Get(url, GetAuthHeaders());
            if (response.statusCode != 200) {
                std::cerr << "Failed to fetch projects. GitLab API returned: " << response.statusCode << "\n";
                break;
            }

            auto jsonArray = json::parse(response.body);
            if (jsonArray.empty()) { hasMore = false; break; }

            for (const auto& item : jsonArray) {
                ProjectContext ctx;
                ctx.projectId = std::to_string(item["id"].get<int>());
                ctx.projectName = item["path_with_namespace"].get<std::string>();
                ctx.defaultBranch = item.value("default_branch", "main");
                projects.push_back(ctx);
            }
            page++;
        }

        return projects;
    }

    std::string FetchFileContent(const std::string& projectId, const std::string& filePath, const std::string& ref) override {
        std::string encodedPath = filePath;
        size_t pos = 0;
        while ((pos = encodedPath.find('/', pos)) != std::string::npos) {
            encodedPath.replace(pos, 1, "%2F");
            pos += 3;
        }

        std::string url = baseUrl + "/api/v4/projects/" + projectId +
                          "/repository/files/" + encodedPath + "/raw?ref=" + ref;

        auto response = HttpClient::Get(url, GetAuthHeaders());
        if (response.statusCode != 200) {
            std::cerr << "Failed to fetch file: " << filePath << " (Status " << response.statusCode << ")\n";
            return "";
        }
        return response.body;
    }

    void CreateBranch(const std::string& projectId, const std::string& newBranch, const std::string& refBranch) override {
        std::string url = baseUrl + "/api/v4/projects/" + projectId + "/repository/branches";
        json payload = {{"branch", newBranch}, {"ref", refBranch}};
        auto response = HttpClient::Post(url, payload.dump(), GetAuthHeaders());
        if (response.statusCode != 200 && response.statusCode != 201)
            std::cerr << "[ERROR] Failed to create branch '" << newBranch
                      << "' (Status " << response.statusCode << "): " << response.body << "\n";
    }

    void CommitFile(const std::string& projectId, const std::string& branch, const std::string& filePath,
                    const std::string& content, const std::string& commitMessage) override {
        std::string url = baseUrl + "/api/v4/projects/" + projectId + "/repository/commits";
        json payload = {
            {"branch", branch},
            {"commit_message", commitMessage},
            {"actions", {{
                {"action", "update"},
                {"file_path", filePath},
                {"content", content}
            }}}
        };
        auto response = HttpClient::Post(url, payload.dump(), GetAuthHeaders());
        if (response.statusCode != 200 && response.statusCode != 201)
            std::cerr << "[ERROR] Failed to commit file '" << filePath
                      << "' (Status " << response.statusCode << "): " << response.body << "\n";
    }

    std::vector<std::string> GetSourceFiles(const std::string& projectId, const std::string& ref) override {
        std::vector<std::string> sourceFiles;
        int page = 1;
        bool hasMore = true;

        while (hasMore) {
            std::string url = baseUrl + "/api/v4/projects/" + projectId +
                              "/repository/tree?recursive=true&per_page=100&ref=" + ref +
                              "&page=" + std::to_string(page);

            auto response = HttpClient::Get(url, GetAuthHeaders());
            if (response.statusCode != 200) break;

            auto jsonArray = json::parse(response.body);
            if (jsonArray.empty()) { hasMore = false; break; }

            for (const auto& item : jsonArray) {
                if (item["type"].get<std::string>() == "blob") {
                    std::string path = item["path"].get<std::string>();
                    if (path.ends_with(".java") || path.ends_with(".kt")   || path.ends_with(".kts") ||
                        path.ends_with(".xml")  || path.ends_with(".yaml") || path.ends_with(".yml")) {
                        sourceFiles.push_back(path);
                    }
                }
            }
            page++;
        }
        return sourceFiles;
    }

    std::string CreateMergeRequest(const std::string& projectId, const std::string& sourceBranch,
                                   const std::string& targetBranch, const std::string& title,
                                   const std::string& description) override {
        std::string url = baseUrl + "/api/v4/projects/" + projectId + "/merge_requests";
        json payload = {
            {"source_branch", sourceBranch},
            {"target_branch", targetBranch},
            {"title", title},
            {"description", description}
        };
        auto response = HttpClient::Post(url, payload.dump(), GetAuthHeaders());
        if (response.statusCode == 201) {
            auto jsonResp = json::parse(response.body);
            return jsonResp["web_url"].get<std::string>();
        }
        return "";
    }
};