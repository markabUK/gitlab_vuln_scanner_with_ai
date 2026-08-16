#include <iostream>
#include <cstdlib>
#include <memory>

#include "infrastructure/AdvancedGradleParser.hpp"
#include "infrastructure/MavenCentralRegistry.hpp"
#include "infrastructure/GitLabRestClient.hpp"
#include "adapters/GitLabDuoAdapter.hpp"
#include "adapters/OpenAIAdapter.hpp"
#include "orchestration/DependencyUpdateOrchestrator.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <GitLab-Group-ID>\n";
        return 1;
    }
    std::string groupId = argv[1];

    // Read environment variables
    const char* glTokenEnv = std::getenv("GITLAB_PRIVATE_TOKEN");
    const char* glHostEnv = std::getenv("GITLAB_HOST");
    const char* aiProviderEnv = std::getenv("AI_PROVIDER"); // "DUO" or "OPENAI"
    const char* openAiTokenEnv = std::getenv("OPENAI_API_KEY");

    if (!glTokenEnv) {
        std::cerr << "Error: GITLAB_PRIVATE_TOKEN environment variable is missing.\n";
        return 1;
    }

    std::string gitlabToken = glTokenEnv;
    std::string gitlabHost = glHostEnv ? glHostEnv : "https://gitlab.com";
    std::string aiProvider = aiProviderEnv ? aiProviderEnv : "DUO";

    // 1. Instantiate the Parser and Registry
    auto parser = std::make_shared<AdvancedGradleParser>();
    auto maven = std::make_shared<MavenCentralRegistry>();

    // 2. Instantiate the GitLab Client
    auto gitlabClient = std::make_shared<GitLabRestClient>(gitlabHost, gitlabToken);

    // 3. Instantiate the selected AI Adapter (Strategy Pattern)
    std::shared_ptr<IAICodeAssistant> aiAssistant;
    
    if (aiProvider == "OPENAI") {
        if (!openAiTokenEnv) {
            std::cerr << "Error: OPENAI_API_KEY is missing but provider is set to OPENAI.\n";
            return 1;
        }
        aiAssistant = std::make_shared<OpenAIAdapter>(openAiTokenEnv);
    } else {
        // Default to GitLab Duo
        aiAssistant = std::make_shared<GitLabDuoAdapter>(gitlabHost, gitlabToken);
    }

    // 4. Wire everything into the Orchestrator (Dependency Injection)
    DependencyUpdateOrchestrator orchestrator(gitlabClient, maven, aiAssistant, parser);

    // 5. Execute
    try {
        orchestrator.RunWorkflow(groupId);
    } catch (const std::exception& e) {
        std::cerr << "Workflow failed with exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}