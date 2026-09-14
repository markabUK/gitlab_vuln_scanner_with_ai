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
        std::function<std::string(int attempt)> apiCall) 
    {
        int backoff = initialBackoffMs;

        for (int attempt = 1; attempt <= maxRetries; ++attempt) {
            try {
                std::string rawOutput = apiCall(attempt);

                if (rawOutput.empty()) {
                    if (attempt < maxRetries) {
                        std::cerr << "  [AI WARNING] API failure on attempt " << attempt 
                                  << ". Retrying in " << (backoff / 1000.0) << "s...\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                        backoff *= 2;
                    }
                    continue;
                }

                if (rawOutput.find("NO_CHANGES_NEEDED") != std::string::npos) {
                    std::cout << "  [AI] Code is already compatible. No changes applied.\n";
                    return originalCode;
                }

                std::string cleanedCode = StringUtils::CleanAIOutput(rawOutput, originalCode);

                if (cleanedCode == originalCode) {
                    std::cout << "  [AI] Model returned unchanged code. Assuming compatibility.\n";
                }

                return cleanedCode; 
                
            } catch (const std::exception& e) {
                std::cerr << "  [AI ERROR] Attempt " << attempt << " threw exception: " << e.what() << "\n";
                if (attempt < maxRetries) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                    backoff *= 2;
                }
            }
        }

        std::cout << "  [AI WARNING] Max retries reached. Leaving file unchanged.\n";
        return originalCode;
    }
};