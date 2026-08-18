#pragma once

#include <string>

class StringUtils {
public:
    // Replaces all occurrences of a substring with a new string
    static std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
        if (from.empty()) return str;
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); 
        }
        return str;
    }

    // Strips conversational text and markdown blocks (```java ... ```) from AI output
    static std::string CleanAIOutput(std::string code, const std::string& baseCode = "") {
        size_t startTick = code.find("```");
        if (startTick != std::string::npos) {
            size_t startLineEnd = code.find('\n', startTick);
            size_t endTick = code.rfind("```");
            
            if (endTick != std::string::npos && endTick > startLineEnd) {
                code = code.substr(startLineEnd + 1, endTick - startLineEnd - 1);
            }
        }
        
        // Safety constraint: If the AI truncated the file heavily, reject it and return the original
        if (!baseCode.empty() && code.length() < (baseCode.length() * 0.5)) {
            return baseCode; 
        }
        
        return code;
    }
};