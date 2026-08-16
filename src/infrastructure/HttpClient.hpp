#pragma once

#include <string>
#include <map>
#include <cpr/cpr.h>

class HttpClient {
public:
    struct Response {
        int statusCode;
        std::string body;
    };

    static Response Get(const std::string& url, const std::map<std::string, std::string>& headers = {}) {
        cpr::Header cprHeaders;
        for (const auto& [key, value] : headers) {
            cprHeaders.insert({key, value});
        }
        
        cpr::Response r = cpr::Get(cpr::Url{url}, cprHeaders);
        return {static_cast<int>(r.status_code), r.text};
    }

    static Response Post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers = {}) {
        cpr::Header cprHeaders;
        for (const auto& [key, value] : headers) {
            cprHeaders.insert({key, value});
        }
        
        cpr::Response r = cpr::Post(cpr::Url{url}, cprHeaders, cpr::Body{body});
        return {static_cast<int>(r.status_code), r.text};
    }
};