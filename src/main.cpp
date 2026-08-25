#include <iostream>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>

// Core Infrastructure
#include "infrastructure/AppSettings.hpp"
#include "infrastructure/GitLabRestClient.hpp"
#include "infrastructure/DryRunGitLabClient.hpp"
#include "infrastructure/CompositeRegistry.hpp"
#include "infrastructure/GoogleChatNotifier.hpp"

// Parsers & Registries
#include "infrastructure/AdvancedGradleParser.hpp"
#include "infrastructure/RegexPomParser.hpp"
#include "infrastructure/RegexAntParser.hpp"
#include "infrastructure/MavenCentralRegistry.hpp"
#include "infrastructure/GitLabMavenRegistry.hpp"
#include "infrastructure/RegexDotNetParser.hpp"
#include "infrastructure/NuGetV3Registry.hpp"
#include "infrastructure/RegexGoParser.hpp"
#include "infrastructure/GoModulesRegistry.hpp"
#include "infrastructure/RegexNpmParser.hpp"
#include "infrastructure/NpmRegistry.hpp"

// AI Adapters
#include "adapters/GitLabDuoAdapter.hpp"
#include "adapters/OpenAIAdapter.hpp"
#include "adapters/GeminiAdapter.hpp"
#include "adapters/OllamaAdapter.hpp"
#include "adapters/DryRunAICodeAssistant.hpp"

// Ecosystem Handlers
#include "orchestration/JavaHandler.hpp"
#include "orchestration/DotNetHandler.hpp"
#include "orchestration/GoHandler.hpp"
#include "orchestration/NodeHandler.hpp"
#include "orchestration/DependencyUpdateOrchestrator.hpp"

int main(int argc, char* argv[]) {
    std::string cliOverrideTargetId = "";
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
        } else if (cliOverrideTargetId.empty() && arg[0] != '-') {
            cliOverrideTargetId = arg;
        }
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

    if (!cliOverrideTargetId.empty()) {
        settings.target.id = cliOverrideTargetId;
    }

    if (settings.target.id.empty()) {
        std::cerr << "Usage Error: Target ID must be specified either in the config file (Target.Id) or as a command line argument.\n";
        std::cerr << "Command: " << argv[0] << " [Target-ID] [--dry-run] [--dry-run-offline] [--debug] [--config=/path/to/appsettings.json]\n";
        return 1;
    }

    // --- 1. Instantiate Parsers ---
    auto gradleParser = std::make_shared<AdvancedGradleParser>();
    auto pomParser = std::make_shared<RegexPomParser>();
    auto antParser = std::make_shared<RegexAntParser>();
    auto dotnetParser = std::make_shared<RegexDotNetParser>();
    auto goParser = std::make_shared<RegexGoParser>();
    auto npmParser = std::make_shared<RegexNpmParser>();

    // --- 2. Instantiate Registries ---
    auto mavenRegistryRouter = std::make_shared<CompositeRegistry>();
    for (const auto& regConfig : settings.registries) {
        if (regConfig.type == "GitLab") {
            mavenRegistryRouter->AddRegistry(
                std::make_shared<GitLabMavenRegistry>(regConfig.url, regConfig.token),
                regConfig.groupPrefixes
            );
        } else {
            mavenRegistryRouter->AddRegistry(
                std::make_shared<MavenCentralRegistry>(settings.migrations),
                regConfig.groupPrefixes
            );
        }
    }
    
    auto nugetRegistry = std::make_shared<NuGetV3Registry>();
    auto goRegistry = std::make_shared<GoModulesRegistry>();
    auto npmRegistry = std::make_shared<NpmRegistry>();

    // --- 3. Instantiate Clients ---
    std::shared_ptr<IGitLabClient> gitlabClient = std::make_shared<GitLabRestClient>(settings.gitlabHost, settings.gitlabToken);
    std::shared_ptr<INotificationClient> chatNotifier = std::make_shared<GoogleChatNotifier>(settings.googleChatWebhook);
    
    // --- 4. Instantiate AI Assistant ---
    std::shared_ptr<IAICodeAssistant> aiAssistant;
    if (settings.aiProvider == "OPENAI") {
        aiAssistant = std::make_shared<OpenAIAdapter>(settings.openAiApiKey);
    } else if (settings.aiProvider == "DUO") {
        aiAssistant = std::make_shared<GitLabDuoAdapter>(settings.gitlabHost, settings.gitlabToken);
    } else if (settings.aiProvider == "OLLAMA") {
        aiAssistant = std::make_shared<OllamaAdapter>(settings.ollamaModel, settings.ollamaEndpoint);
    } else {
        aiAssistant = std::make_shared<GeminiAdapter>(settings.geminiApiKey);
    }

    // --- 5. Dry Run Mode Interception ---
    if (isDryRun) {
        std::cout << "\n============================================\n";
        std::cout << "    DRY RUN MODE ACTIVATED  \n";
        gitlabClient = std::make_shared<DryRunGitLabClient>(gitlabClient);
        
        if (isOffline) {
            aiAssistant = std::make_shared<DryRunAICodeAssistant>(); 
        }
        std::cout << "============================================\n\n";
    }

    bool enableDebugOutput = isDryRun || isDebug;

    // --- 6. Wire up Ecosystem Handlers ---
    std::vector<std::shared_ptr<IEcosystemHandler>> handlers;
    
    handlers.push_back(std::make_shared<JavaHandler>(
        gitlabClient, aiAssistant, gradleParser, pomParser, antParser, mavenRegistryRouter, settings.migrations));
        
    handlers.push_back(std::make_shared<DotNetHandler>(
        gitlabClient, aiAssistant, dotnetParser, nugetRegistry, settings.migrations));
        
    handlers.push_back(std::make_shared<GoHandler>(
        gitlabClient, aiAssistant, goParser, goRegistry, settings.migrations));
        
    handlers.push_back(std::make_shared<NodeHandler>(
        gitlabClient, aiAssistant, npmParser, npmRegistry, settings.migrations));

    // --- 7. Run Orchestrator ---
    // UPDATED: Removed botEmail from injection
    DependencyUpdateOrchestrator orchestrator(
        gitlabClient, 
        handlers, 
        settings.target, 
        chatNotifier, 
        enableDebugOutput
    );

    try {
        orchestrator.RunWorkflow();
    } catch (const std::exception& e) {
        std::cerr << "Workflow failed with exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}