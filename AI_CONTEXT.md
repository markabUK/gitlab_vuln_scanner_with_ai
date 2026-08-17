#### Update to `ai_context.md`
If you are passing project context to LLMs, update your context file:

```markdown
# AI Context: GitLab Gradle Dependency & Refactoring Orchestrator

**Project Description:**
A C++17 CLI tool that automates Gradle dependency updates across GitLab repositories, using AI (Gemini/OpenAI/Ollama) to refactor source code for breaking changes.

**Core Architecture:**
1. **Dependency Injection:** Logic is decoupled using interfaces (`IGitLabClient`, `IAICodeAssistant`, `IMavenRegistry`).
2. **Strategy Pattern:** Multiple AI adapters switch behaviors cleanly.
3. **Decorator Pattern:** `DryRunGitLabClient` allows safe dry-runs without `if(dryRun)` cluttering business logic.
4. **Composite Pattern:** `CompositeRegistry` routes dependency lookup checks to internal `GitLabMavenRegistry` or public `MavenCentralRegistry` based on prefix mapping in `appsettings.json`.

**Key Business Rules:**
- The CLI parses `appsettings.json` to configure GitLab, AI keys, and Registry routing.
- If a dependency belongs to an internal registry (e.g., prefix `uk.co.tpplc`), the version is bumped but the `skipAI` flag is set. The AI is **never** asked to refactor code for internal dependencies.
- Requests to Cloud AI APIs are batched per-file and rate-limited internally (e.g., 4.5s delays for Gemini Free Tier).
- `nlohmann_json` is used for all JSON parsing.
- `libcurl` is used for all HTTP requests via a static `HttpClient` wrapper.