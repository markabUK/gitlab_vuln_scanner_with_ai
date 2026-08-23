#pragma once

#include "../domain/Interfaces.hpp"
#include <regex>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

class RegexDotNetParser : public IDotNetParser {
private:
    // Helper to escape special regex characters (such as dots in version strings)
    std::string EscapeRegex(const std::string& str) {
        return std::regex_replace(str, std::regex(R"([-[\]{}()*+?.,\^$|#\s])"), R"(\$&)");
    }

    // Normalizes backslashes to forward slashes for cross-platform Git repository paths
    std::string NormalizePath(std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        // Strip leading slash or ./ if present
        if (path.rfind("./", 0) == 0) {
            path = path.substr(2);
        }
        while (!path.empty() && path.front() == '/') {
            path = path.substr(1);
        }
        return path;
    }

public:
    std::vector<Dependency> ParseDependencies(const std::string& projectContent) override {
        std::vector<Dependency> dependencies;

        // 1. Match standard attribute-style PackageReference:
        //    <PackageReference Include="Newtonsoft.Json" Version="13.0.3" />
        //    <PackageReference Version="13.0.3" Include="Newtonsoft.Json" />
        std::regex attrRegex1(R"(<PackageReference\s+[^>]*?Include\s*=\s*['"]([^'"]+)['"][^>]*?Version\s*=\s*['"]([^'"]+)['"][^>]*?>)");
        auto b1 = std::sregex_iterator(projectContent.begin(), projectContent.end(), attrRegex1);
        auto e1 = std::sregex_iterator();
        for (std::sregex_iterator i = b1; i != e1; ++i) {
            std::smatch match = *i;
            dependencies.push_back({
                "",             // NuGet package ID serves as the primary name
                match[1].str(), // Package ID
                match[2].str()  // Version
            });
        }

        std::regex attrRegex2(R"(<PackageReference\s+[^>]*?Version\s*=\s*['"]([^'"]+)['"][^>]*?Include\s*=\s*['"]([^'"]+)['"][^>]*?>)");
        auto b2 = std::sregex_iterator(projectContent.begin(), projectContent.end(), attrRegex2);
        auto e2 = std::sregex_iterator();
        for (std::sregex_iterator i = b2; i != e2; ++i) {
            std::smatch match = *i;
            dependencies.push_back({
                "",             
                match[2].str(), // Package ID
                match[1].str()  // Version
            });
        }

        // 2. Match element-style PackageReference:
        //    <PackageReference Include="Newtonsoft.Json">
        //        <Version>13.0.3</Version>
        //    </PackageReference>
        std::regex elemRegex(R"(<PackageReference\s+[^>]*?Include\s*=\s*['"]([^'"]+)['"][^>]*?>[\s\S]*?<Version>\s*([^<]+)\s*</Version>[\s\S]*?</PackageReference>)");
        auto b3 = std::sregex_iterator(projectContent.begin(), projectContent.end(), elemRegex);
        auto e3 = std::sregex_iterator();
        for (std::sregex_iterator i = b3; i != e3; ++i) {
            std::smatch match = *i;
            dependencies.push_back({
                "",
                match[1].str(),
                match[2].str()
            });
        }

        // 3. Match <TargetFramework> for Target Framework LTS management
        std::regex tfmRegex(R"(<TargetFramework>\s*([^<]+)\s*</TargetFramework>)");
        std::smatch tfmMatch;
        if (std::regex_search(projectContent, tfmMatch, tfmRegex)) {
            dependencies.push_back({
                "Microsoft.NETCore.App", 
                "TargetFramework", 
                tfmMatch[1].str()
            });
        }

        return dependencies;
    }

    std::string UpdateDependencyVersion(
        const std::string& projectContent, 
        const Dependency& oldDep, 
        const Dependency& newDep) override 
    {
        std::string updated = projectContent;

        // Special handling for TargetFramework upgrade
        if (oldDep.name == "TargetFramework") {
            std::regex tfmRegex(R"(<TargetFramework>\s*)" + EscapeRegex(oldDep.version) + R"(\s*</TargetFramework>)");
            return std::regex_replace(updated, tfmRegex, "<TargetFramework>" + newDep.version + "</TargetFramework>");
        }

        std::string safeName = EscapeRegex(oldDep.name);
        std::string safeOldVer = EscapeRegex(oldDep.version);

        // 1. Replace attribute-style: Include="Name" Version="OldVer"
        std::regex attrPattern1(R"((<PackageReference\s+[^>]*?Include\s*=\s*['"])" + safeName + R"(['"][^>]*?Version\s*=\s*['"])" + safeOldVer + R"(['"]))");
        std::smatch match;
        if (std::regex_search(updated, match, attrPattern1)) {
            std::regex repRegex(R"(Version\s*=\s*['"])" + safeOldVer + R"(['"])");
            std::string matchedSegment = match[0].str();
            std::string replacedSegment = std::regex_replace(matchedSegment, repRegex, "Version=\"" + newDep.version + "\"");
            updated.replace(match.position(), match.length(), replacedSegment);
            return updated;
        }

        // 2. Replace element-style: <Version>OldVer</Version>
        std::regex elemPattern(R"((<PackageReference\s+[^>]*?Include\s*=\s*['"])" + safeName + R"(['"][^>]*?>[\s\S]*?<Version>\s*))" + safeOldVer + R"((\s*</Version>[\s\S]*?</PackageReference>))");
        updated = std::regex_replace(updated, elemPattern, "$1" + newDep.version + "$2");

        return updated;
    }

    std::vector<std::string> ParseSlnxProjects(const std::string& slnxContent) override {
        std::vector<std::string> projects;

        // Matches projects in .slnx files:
        // <Project Path="src/MyApi/MyApi.csproj" />
        // <Project Path="tests\MyApi.Tests\MyApi.Tests.csproj" Type="..." />
        std::regex slnxProjRegex(R"(<Project\s+[^>]*?Path\s*=\s*['"]([^'"]+)['"][^>]*?>)");

        auto begin = std::sregex_iterator(slnxContent.begin(), slnxContent.end(), slnxProjRegex);
        auto end = std::sregex_iterator();

        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::smatch match = *i;
            std::string rawPath = match[1].str();
            std::string normalized = NormalizePath(rawPath);
            
            // Only include actual project build files (.csproj, .fsproj, .vbproj)
            if (normalized.ends_with(".csproj") || 
                normalized.ends_with(".fsproj") || 
                normalized.ends_with(".vbproj")) {
                projects.push_back(normalized);
            }
        }

        return projects;
    }
};