#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <csignal>
#include <spdlog/spdlog.h>

// Core Infrastructure
#include "infrastructure/LoggerSetup.hpp"
#include "infrastructure/AppSettings.hpp"
#include "infrastructure/GitLabRestClient.hpp"
#include "infrastructure/DryRunGitLabClient.hpp"
#include "infrastructure/CompositeRegistry.hpp"
#include "infrastructure/GoogleChatNotifier.hpp"
#include "infrastructure/GitManager.hpp" // Included for signal cleanup

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

// ============================================================================
// SIGNAL HANDLER FOR GRACEFUL SHUTDOWN
// ============================================================================
void HandleSignal(int signal) {
    spdlog::warn("\n[!] Interrupt signal ({}) received. Aborting process...", signal);
    GitManager::CleanupAllWorkspaces();
    spdlog::info("Shutdown complete.");
    std::exit(signal);
}

struct CliArgs {
    std::string overrideTargetId;
    std::string configPath;
    bool isDryRun = false;
    bool isOffline = false;
    bool isDebug = false;
};

CliArgs ParseCommandLine(int argc, char* argv[]) {
    CliArgs args;
    std::filesystem::path exePath = std::filesystem::absolute(std::filesystem::path(argv[0]));
    args.configPath = (exePath.parent_path() / "appsettings.json").string();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dry-run") args.isDryRun = true;
        else if (arg == "--dry-run-offline") { args.isDryRun = true; args.isOffline = true; }
        else if (arg == "--debug") args.isDebug = true;
        else if (arg.find("--config=") == 0) args.configPath = arg.substr(9);
        else if (args.overrideTargetId.empty() && arg[0] != '-') args.overrideTargetId = arg;
    }
    return args;
}

std::shared_ptr<CompositeRegistry> BuildMavenRegistry(const AppSettings& settings) {
    auto router = std::make_shared<CompositeRegistry>();
    for (const auto& regConfig : settings.registries) {
        if (regConfig.type == "GitLab") {
            router->AddRegistry(std::make_shared<GitLabMavenRegistry>(regConfig.url, regConfig.token), regConfig.groupPrefixes);
        } else {
            router->AddRegistry(std::make_shared<MavenCentralRegistry>(settings.GetMigrations("Java")), regConfig.groupPrefixes);
        }
    }
    return router;
}

std::shared_ptr<IAICodeAssistant> BuildAiAssistant(const AppSettings& settings, bool isDryRunOffline) {
    if (isDryRunOffline) return std::make_shared<DryRunAICodeAssistant>();
    
    if (settings.aiProvider == "OPENAI") return std::make_shared<OpenAIAdapter>(settings.openAiApiKey);
    if (settings.aiProvider == "DUO") return std::make_shared<GitLabDuoAdapter>(settings.gitlabHost, settings.gitlabToken);
    if (settings.aiProvider == "OLLAMA") return std::make_shared<OllamaAdapter>(settings.ollamaModel, settings.ollamaEndpoint, settings.ollamaContextLength, settings.ollamaOutputLength);
    
    return std::make_shared<GeminiAdapter>(settings.geminiApiKey);
}

std::vector<std::shared_ptr<IEcosystemHandler>> BuildHandlers(
    std::shared_ptr<IGitLabClient> gitlabClient, 
    std::shared_ptr<IAICodeAssistant> aiAssistant, 
    const AppSettings& settings,
    bool isDryRun) 
{
    std::vector<std::shared_ptr<IEcosystemHandler>> handlers;
    
    handlers.push_back(std::make_shared<JavaHandler>(
        gitlabClient, 
        aiAssistant, 
        std::make_shared<AdvancedGradleParser>(), 
        std::make_shared<RegexPomParser>(), 
        std::make_shared<RegexAntParser>(),  
        BuildMavenRegistry(settings),        
        settings.GetMigrations("Java"), 
        isDryRun));
        
    handlers.push_back(std::make_shared<DotNetHandler>(
        gitlabClient, 
        aiAssistant, 
        std::make_shared<RegexDotNetParser>(), 
        std::make_shared<NuGetV3Registry>(), 
        settings.GetMigrations("DotNet"), 
        isDryRun));
        
    handlers.push_back(std::make_shared<GoHandler>(
        gitlabClient, 
        aiAssistant, 
        std::make_shared<RegexGoParser>(), 
        std::make_shared<GoModulesRegistry>(), 
        settings.GetMigrations("Go"), 
        isDryRun));
        
    handlers.push_back(std::make_shared<NodeHandler>(
        gitlabClient, 
        aiAssistant, 
        std::make_shared<RegexNpmParser>(), 
        std::make_shared<NpmRegistry>(), 
        settings.GetMigrations("Node"), 
        isDryRun));

    return handlers;
}

int main(int argc, char* argv[]) {
    // Bind OS Signals
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    LoggerSetup::Initialize("logs");
    CliArgs args = ParseCommandLine(argc, argv);

    AppSettings settings;
    try {
        settings = AppSettings::Load(args.configPath);
    } catch (const std::exception& e) {
        spdlog::critical("Config Error: {}", e.what());
        return 1;
    }

    if (settings.gitlabToken.empty()) {
        spdlog::critical("GitLab Token is missing in {}", args.configPath);
        return 1;
    }

    if (!args.overrideTargetId.empty()) settings.target.id = args.overrideTargetId;

    if (settings.target.id.empty()) {
        spdlog::critical("Usage Error: Target ID must be specified either in the config file (Target.Id) or as a command line argument.");
        spdlog::info("Command: {} [Target-ID] [--dry-run] [--dry-run-offline] [--debug] [--config=/path/to/appsettings.json]", argv[0]);
        return 1;
    }

    std::shared_ptr<IGitLabClient> gitlabClient = std::make_shared<GitLabRestClient>(settings.gitlabHost, settings.gitlabToken);
    std::shared_ptr<INotificationClient> chatNotifier = std::make_shared<GoogleChatNotifier>(settings.googleChatWebhook);
    
    if (args.isDryRun) {
        spdlog::info("============================================");
        spdlog::info("          DRY RUN MODE ACTIVATED            ");
        spdlog::info("============================================");
        gitlabClient = std::make_shared<DryRunGitLabClient>(gitlabClient);
    }

    auto aiAssistant = BuildAiAssistant(settings, args.isOffline);
    auto handlers = BuildHandlers(gitlabClient, aiAssistant, settings, args.isDryRun);

    DependencyUpdateOrchestrator orchestrator(gitlabClient, handlers, settings, chatNotifier, args.isDryRun);

    try {
        spdlog::info("Starting Dependency Updater...");
        orchestrator.RunWorkflow();
        spdlog::info("Workflow completed successfully.");
    } catch (const std::exception& e) {
        spdlog::critical("Workflow failed with exception: {}", e.what());
        GitManager::CleanupAllWorkspaces(); // Catch-all safety net
        return 1;
    }

    LoggerSetup::Shutdown();
    return 0;
}