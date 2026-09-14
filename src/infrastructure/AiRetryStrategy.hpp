#pragma once

#include "StringUtils.hpp"
#include <string>
#include <functional>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>

class AiRetryStrategy {
public:
    static std::string Execute(
        int maxRetries,
        int initialBackoffMs,
        const std::string& originalCode,
        std::function<std::string(int attempt, bool isRetry)> apiCall) 
    {
        int backoff = initialBackoffMs;
        bool isRetry = false;

        for (int attempt = 1; attempt <= maxRetries; ++attempt) {
            try {
                // The lambda should return an empty string to signal a network/HTTP failure
                std::string rawOutput = apiCall(attempt, isRetry);

                if (rawOutput.empty()) {
                    if (attempt < maxRetries) {
                        std::cerr << "  [AI WARNING] API failure on attempt " << attempt 
                                  << ". Retrying in " << (backoff / 1000.0) << "s...\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                        backoff *= 2;
                    }
                    continue;
                }

                std::string cleanedCode = StringUtils::CleanAIOutput(rawOutput, originalCode);

                // Success check: did it actually change anything?
                if (cleanedCode != originalCode && !cleanedCode.empty()) {
                    return cleanedCode; 
                }

                std::cout << "  [AI Retry] Model returned unchanged code. Forcing a retry (" << attempt << "/" << maxRetries << ")...\n";
                isRetry = true;
                
            } catch (const std::exception& e) {
                std::cerr << "  [AI ERROR] Attempt " << attempt << " threw exception: " << e.what() << "\n";
                if (attempt < maxRetries) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                    backoff *= 2;
                }
            }
        }

        std::cout << "  [AI WARNING] Max retries reached or model refused to modify code. Leaving file unchanged.\n";
        return originalCode;
    }
};