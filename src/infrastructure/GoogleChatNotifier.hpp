#pragma once

#include "../domain/Interfaces.hpp"
#include "HttpClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using json = nlohmann::json;

class GoogleChatNotifier : public INotificationClient {
private:
    std::string webhookUrl;

public:
    explicit GoogleChatNotifier(const std::string& webhook) : webhookUrl(webhook) {}

    void NotifyUserOfSkippedMR(
        const std::string& projectName, 
        const std::string& humanAuthorName, 
        const std::string& humanAuthorEmail, 
        const std::string& mrUrl) override 
    {
        if (webhookUrl.empty()) {
            std::cout << "  [Notify] Google Chat webhook not configured. Skipping notification.\n";
            return;
        }

        // Google Chat supports `<URL|Display Text>` for standard links in text messages.
        std::string messageText = 
            "⚠️ *Dependency Update Skipped*\n\n"
            "Project: *" + projectName + "*\n"
            "I detected human commits from *" + humanAuthorName + "* (" + humanAuthorEmail + "). "
            "To prevent overwriting your work, I have paused automated updates for this branch.\n\n"
            "Please review the MR: <" + mrUrl + "|View Merge Request>";

        json payload = {
            {"text", messageText}
        };

        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"}
        };

        auto response = HttpClient::Post(webhookUrl, payload.dump(), headers);

        if (response.statusCode != 200) {
            std::cerr << "  [Notify] Failed to send Google Chat message. Status " << response.statusCode << "\n"
                      << "  Response: " << response.body << "\n";
        } else {
            std::cout << "  [Notify] Successfully pinged Google Chat regarding " << projectName << ".\n";
        }
    }
};