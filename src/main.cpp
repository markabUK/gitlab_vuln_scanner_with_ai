#include <iostream>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>

#include "infrastructure/AdvancedGradleParser.hpp"
#include "infrastructure/MavenCentralRegistry.hpp"
#include "infrastructure/GitLabMavenRegistry.hpp"
#include "infrastructure/CompositeRegistry.hpp"
#include "infrastructure/AppSettings.hpp"
#include "infrastructure/GitLabRestClient.hpp"
#include "infrastructure/DryRunGitLabClient.hpp"
#include "adapters/GitLabDuoAdapter.hpp"
#include "adapters/OpenAIAdapter.hpp"
#include "adapters/GeminiAdapter.hpp"
#include "adapters/OllamaAdapter.hpp"
#include "adapters/DryRunAICodeAssistant.hpp"
#include "orchestration/DependencyUpdateOrchestrator.hpp"

int main(int argc, char* argv[]) {
    std::string groupId = "";
    bool isDryRun = false;
    bool isOffline = false;
    bool isDebug = false;
    
    std::filesystem::path exePath = std::filesystem::absolute(std::filesystem::path(argv[0]));
    std::string configPath = (exePath.parent_path() / "appsettings.json").string();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dry-run") {
            isDryRun = true;
        } else if (arg == "--dry-run-offline") {
            isDryRun = true;
            isOffline = true;
        } else if (arg == "--debug") {
            isDebug = true;
        } else if (arg.find("--config=") == 0) {
            configPath = arg.substr(9);
        } else if (groupId.empty()) {
            groupId = arg;
        }
    }

    if (groupId.empty()) {
        std::cerr << "Usage: " << argv[0] << " <GitLab-Group-ID> [--dry-run] [--dry-run-offline] [--debug] [--config=/path/to/appsettings.json]\n";
        return 1;
    }

    AppSettings settings;
    try {
        settings = AppSettings::Load(configPath);
    } catch (const std::exception& e) {
        std::cerr << "Config Error: " << e.what() << "\n";
        return 1;
    }

    if (settings.gitlabToken.empty()) {
        std::cerr << "Error: GitLab Token is missing in " << configPath << "\n";
        return 1;
    }

    auto parser = std::make_shared<AdvancedGradleParser>();
    
    auto registryRouter = std::make_shared<CompositeRegistry>();
    for (const auto& regConfig : settings.registries) {
        if (regConfig.type == "GitLab") {
            registryRouter->AddRegistry(
                std::make_shared<GitLabMavenRegistry>(regConfig.url, regConfig.token),
                regConfig.groupPrefixes
            );
        } else {
            registryRouter->AddRegistry(
                std::make_shared<MavenCentralRegistry>(settings.migrations),
                regConfig.groupPrefixes
            );
        }
    }

    std::shared_ptr<IGitLabClient> gitlabClient = std::make_shared<GitLabRestClient>(settings.gitlabHost, settings.gitlabToken);
    std::shared_ptr<IAICodeAssistant> aiAssistant;
    
    if (settings.aiProvider == "OPENAI") {
        aiAssistant = std::make_shared<OpenAIAdapter>(settings.openAiApiKey);
    } 
    else if (settings.aiProvider == "DUO") {
        aiAssistant = std::make_shared<GitLabDuoAdapter>(settings.gitlabHost, settings.gitlabToken);
    } 
    else if (settings.aiProvider == "OLLAMA") {
        aiAssistant = std::make_shared<OllamaAdapter>(settings.ollamaModel, settings.ollamaEndpoint);
    }
    else {
        aiAssistant = std::make_shared<GeminiAdapter>(settings.geminiApiKey);
    }
    
    if (isDryRun) {
        std::cout << "\n============================================\n";
        std::cout << " ⚠️  DRY RUN MODE ACTIVATED ⚠️\n";
        gitlabClient = std::make_shared<DryRunGitLabClient>(gitlabClient);
        
        if (isOffline) {
            aiAssistant = std::make_shared<DryRunAICodeAssistant>(); 
        }
        std::cout << "============================================\n\n";
    }
    
    bool enableDebugOutput = isDryRun || isDebug;

    // PASS CONFIG MIGRATIONS TO THE ORCHESTRATOR
    DependencyUpdateOrchestrator orchestrator(gitlabClient, registryRouter, aiAssistant, parser, settings.migrations, enableDebugOutput);

    try {
        orchestrator.RunWorkflow(groupId);
    } catch (const std::exception& e) {
        std::cerr << "Workflow failed with exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}