Here is a comprehensive, production-ready `README.md` that documents the architecture, setup, environment variables, and execution modes.

---

# GitLab Gradle Dependency & AI Refactoring Updater

An automated C++ CLI orchestration tool that scans GitLab groups for Gradle projects, checks Maven Central for dependency upgrades, updates build files, and leverages LLMs to automatically refactor source code to match breaking API changes before opening a Merge Request.

---

## 🌟 Key Features

* **Recursive GitLab Group Scanning:** Discovers all repositories containing `build.gradle` or `build.gradle.kts`.
* **Automated Maven Central Registry Checks:** Compares declared dependencies against the latest stable releases on Maven Central.
* **Smart Source Filtering:** Detects explicit package imports in source files (`.java`, etc.) to minimize unnecessary AI prompts and save token usage.
* **Multi-Provider AI Refactoring Engine:** Plug-and-play AI backends using the Strategy Pattern:
* **Google Gemini (Default):** Cloud LLM with automatic 4.5s request pacing and exponential backoff retry logic.
* **Ollama:** 100% free, private, and unlimited local refactoring using models like `qwen2.5-coder:7b`.
* **OpenAI:** GPT-4o integration.
* **GitLab Duo:** Enterprise AI completions.


* **Non-Destructive Dry-Run Modes:** Simulates workflows and prints exact AI code diffs to the console without modifying remote repositories.

---

## 🏗️ Architecture & Design Patterns

The project is structured with clean architecture and Object-Oriented Design Principles:

* **Strategy Pattern:** `IAICodeAssistant` allows seamless switching between Gemini, Ollama, OpenAI, and GitLab Duo.
* **Decorator Pattern:** `DryRunGitLabClient` wraps the real `IGitLabClient` to intercept and log mutating actions (branching, commits, MR creation) while preserving read access.
* **Dependency Injection:** The `DependencyUpdateOrchestrator` receives abstract interfaces for parsing, HTTP client calls, registries, and AI assistants.

---

## 📋 Prerequisites

* **C++ Compiler:** Supporting C++17 or C++20 (`g++`, `clang++`)
* **Build System:** `CMake` (v3.15+)
* **Dependencies:**
* `libcurl` (HTTP requests)
* `nlohmann_json` (JSON parsing)
* `pthread` / POSIX sockets (Linux/macOS)



---

## ⚙️ Environment Variables

Configure your environment before running the updater:

| Variable | Required | Default | Description |
| --- | --- | --- | --- |
| `GITLAB_PRIVATE_TOKEN` | **Yes** | — | Personal/Project access token with `api` read/write permissions. |
| `GITLAB_HOST` | No | `[https://gitlab.com](https://gitlab.com)` | Custom/self-managed GitLab instance URL. |
| `AI_PROVIDER` | No | `GEMINI` | Chosen AI adapter: `GEMINI`, `OLLAMA`, `OPENAI`, or `DUO`. |
| `GEMINI_API_KEY` | If using `GEMINI` | — | Google AI Studio API key. |
| `OPENAI_API_KEY` | If using `OPENAI` | — | OpenAI API Key. |

---

## 🛠️ Build Instructions

```bash
# Clean and configure build directory
rm -rf build
cmake -S . -B build  

# Compile the binary
cmake --build build 

```

The resulting executable will be located at `./build/GradleDependencyUpdater`.

---

## 🚀 Usage

### Command Syntax

```bash
./build/GradleDependencyUpdater <GitLab-Group-ID> [OPTIONS]

```

### Modes of Execution

#### 1. Live Production Run (Mutates GitLab)

Runs the end-to-end workflow, commits updated Gradle files, refactors code using the configured AI provider, and submits an MR.

```bash
export GITLAB_PRIVATE_TOKEN="glpat-xxxxxxxxxxxx"
export GEMINI_API_KEY="AIzaSyxxxxxxxxxxxx"

./build/GradleDependencyUpdater 122013261

```

#### 2. Live AI Dry-Run (`--dry-run`)

Executes live AI refactoring and **prints the full refactored code to the terminal**, but suppresses all git commits and MR creations.

```bash
./build/GradleDependencyUpdater 122013261 --dry-run

```

#### 3. Offline Dry-Run (`--dry-run-offline`)

Fast simulation mode. Checks dependencies and repository files, but makes **zero AI API calls** and **no git mutations** (ideal for testing parser/logic without burning API limits).

```bash
./build/GradleDependencyUpdater 122013261 --dry-run-offline

```

---

## 🤖 Configuring AI Providers

### Google Gemini (Default)

Uses Google's Gemini Flash model with built-in rate-limit protection:

```bash
export AI_PROVIDER="GEMINI"
export GEMINI_API_KEY="your-gemini-key"
./build/GradleDependencyUpdater 122013261 --dry-run

```

### Local Ollama (Free & Unlimited)

Ensure Ollama is running locally with a coding model:

```bash
ollama run qwen2.5-coder:7b

```

Run the tool:

```bash
export AI_PROVIDER="OLLAMA"
./build/GradleDependencyUpdater 122013261 --dry-run

```

### OpenAI

```bash
export AI_PROVIDER="OPENAI"
export OPENAI_API_KEY="sk-proj-xxxxxxxx"
./build/GradleDependencyUpdater 122013261 --dry-run

```

---

## 📁 Project Structure

```text
├── CMakeLists.txt
├── src/
│   ├── main.cpp                              # CLI entrypoint & Dependency Injection setup
│   ├── domain/
│   │   └── Interfaces.hpp                    # Core abstractions (IGitLabClient, IAICodeAssistant, etc.)
│   ├── infrastructure/
│   │   ├── HttpClient.hpp                    # libcurl REST wrapper
│   │   ├── AdvancedGradleParser.hpp          # Regex-based Gradle dependency parser & updater
│   │   ├── MavenCentralRegistry.hpp          # Maven Central search API integration
│   │   ├── GitLabRestClient.hpp              # GitLab REST API client
│   │   └── DryRunGitLabClient.hpp            # Non-mutating Decorator for safe execution
│   ├── adapters/
│   │   ├── GeminiAdapter.hpp                 # Google Gemini API integration with rate-pacing
│   │   ├── OllamaAdapter.hpp                 # Local Ollama LLM integration
│   │   ├── OpenAIAdapter.hpp                 # OpenAI chat completions integration
│   │   ├── GitLabDuoAdapter.hpp              # GitLab Duo assistant integration
│   │   └── DryRunAICodeAssistant.hpp         # Offline mock assistant
│   └── orchestration/
│       └── DependencyUpdateOrchestrator.hpp  # Main pipeline coordinator

```

Here is the rest of the README, rounding out the documentation with dependency installation, troubleshooting, and rate-limiting details that are crucial for anyone running this tool!

```markdown
---

## 📦 Installing C++ Dependencies

Before running CMake, ensure you have the required C++ libraries installed on your system.

**Ubuntu / Debian:**
```bash
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev nlohmann-json3-dev cmake build-essential

```

**macOS (Homebrew):**

```bash
brew install curl nlohmann-json cmake

```

---

## 🚦 Rate Limits & API Pacing

### Gemini Free Tier (15 RPM Limit)

If using the default `GEMINI` provider on a Free Tier API key, you are limited to 15 Requests Per Minute.

* **The Solution:** The `GeminiAdapter` has a built-in strict rate-pacer. It tracks the time of your last request and will automatically pause the application thread for up to 4.5 seconds to guarantee you never exceed ~13 requests per minute.
* **Retries:** If Google's servers are overloaded (returning a `503 Service Unavailable`), the adapter uses an exponential backoff strategy (2s, 4s, 8s...) up to 5 times before gracefully skipping the file.

### Overcoming Rate Limits

If you have a large monorepo and cannot wait for the 4.5-second pacing:

1. **Switch to Ollama:** Run `export AI_PROVIDER="OLLAMA"` to use your local GPU/CPU for unlimited, zero-delay refactoring.
2. **Upgrade Gemini:** Attach a billing account in Google Cloud to your API key project to enter the Pay-As-You-Go tier, which massively increases your RPM limits.

---

## 🐛 Troubleshooting

| Error / Issue | Root Cause & Solution |
| --- | --- |
| **`[AI ERROR] Gemini API failed with status 404`** | You are targeting a deprecated model. The tool defaults to `gemini-3.7-flash`. Ensure your API URL matches the supported versions in your Google AI Studio dashboard. |
| **`[AI ERROR] ... status 429`** | You hit a rate limit. Ensure the 4.5-second `EnforceRateLimit()` function is active in your adapter, or switch to an offline/unlimited provider like Ollama. |
| **`Failed to fetch project files (401 Unauthorized)`** | Your `GITLAB_PRIVATE_TOKEN` is invalid, expired, or missing the `api`, `read_repository`, and `write_repository` scopes. |
| **No MRs are being created** | Ensure you are running **without** the `--dry-run` or `--dry-run-offline` flags if you want real mutations to occur. |

---

## ⚙️ Configuration (`appsettings.json`)

The application replaces environment variables with a clean JSON configuration file. By default, it looks for `appsettings.json` in the exact directory as the executable.

You can override the config location via CLI:
```bash
./build/GradleDependencyUpdater 122013261 --config="/tmp/custom-settings.json"


================================================================================
Example appsettings.json
================================================================================

``` JSON
{
  "GitLab": {
    "Host": "https://gitlab.example.com",
    "Token": "glpat-xxxxxxxxxxxxxxxxxxxx",
    "BotEmail": "dependency-bot@yourcompany.com"
  },
  "Target": {
    "Type": "Group",
    "Id": "12345",
    "ExcludeGroups": [
      "legacy-archive",
      "sandbox/experimental"
    ],
    "ExcludeProjects": [
      "9999",
      "core-team/frozen-service"
    ]
  },
  "Notifications": {
    "GoogleChatWebhook": "https://chat.googleapis.com/v1/spaces/AAAAxxxxxx/messages?key=AIzaxxxxx&token=xxxxxx"
  },
  "AI": {
    "Provider": "OLLAMA",
    "GeminiApiKey": "",
    "OpenAIApiKey": "",
    "OllamaEndpoint": "http://localhost:11434/api/chat",
    "OllamaModel": "qwen2.5-coder:7b"
  },
  "Registries": [
    {
      "Type": "GitLab",
      "Url": "https://gitlab.example.com/api/v4/groups/12345/-/packages/maven",
      "Token": "glpat-xxxxxxxxxxxxxxxxxxxx",
      "GroupPrefixes": [
        "com.yourcompany.internal",
        "com.yourcompany.shared"
      ]
    },
    {
      "Type": "MavenCentral",
      "Url": "",
      "Token": "",
      "GroupPrefixes": [
        "*"
      ]
    }
  ],
  "Migrations": {
    "Java": [
      {
        "OldGroup": "junit",
        "OldName": "junit",
        "NewGroup": "org.junit.jupiter",
        "NewName": "junit-jupiter-api",
        "MaxOldVersion": "4.99.99",
        "MinNewVersion": "5.0.0",
        "MigrationDocPath": "./migrations/junit4-to-junit5.md",
        "Replacements": [
          { "Search": "import org.junit.Test;", "Replace": "import org.junit.jupiter.api.Test;" },
          { "Search": "import org.junit.Before;", "Replace": "import org.junit.jupiter.api.BeforeEach;" },
          { "Search": "import org.junit.After;", "Replace": "import org.junit.jupiter.api.AfterEach;" },
          { "Search": "import org.junit.BeforeClass;", "Replace": "import org.junit.jupiter.api.BeforeAll;" },
          { "Search": "import org.junit.AfterClass;", "Replace": "import org.junit.jupiter.api.AfterAll;" },
          { "Search": "import org.junit.Ignore;", "Replace": "import org.junit.jupiter.api.Disabled;" },
          { "Search": "import org.junit.Assert.", "Replace": "import org.junit.jupiter.api.Assertions." },
          { "Search": "import static org.junit.Assert.", "Replace": "import static org.junit.jupiter.api.Assertions." }
        ]
      },
      {
        "OldGroup": "javax.servlet",
        "OldName": "javax.servlet-api",
        "NewGroup": "jakarta.servlet",
        "NewName": "jakarta.servlet-api",
        "MigrationDocPath": "./migrations/javax-to-jakarta.md",
        "Replacements": [
          { "Search": "import javax.servlet.", "Replace": "import jakarta.servlet." }
        ]
      },
      {
        "OldGroup": "io.micronaut",
        "OldName": "micronaut-core",
        "MaxOldVersion": "4.99.99",
        "MinNewVersion": "5.0.0",
        "MigrationDocPath": "./migrations/micronaut4-to-micronaut5.md",
        "Replacements": [
          { "Search": "import io.micronaut.core.annotation.Nullable;", "Replace": "import org.jspecify.annotations.Nullable;" },
          { "Search": "import io.micronaut.core.annotation.NonNull;", "Replace": "import org.jspecify.annotations.NonNull;" }
        ]
      }
    ],
    "DotNet": [
      {
        "OldGroup": "",
        "OldName": "Newtonsoft.Json",
        "NewGroup": "",
        "NewName": "System.Text.Json",
        "MigrationDocPath": "./migrations/newtonsoft-to-system-text.md",
        "Replacements": [
          { "Search": "using Newtonsoft.Json;", "Replace": "using System.Text.Json;" },
          { "Search": "using Newtonsoft.Json.Serialization;", "Replace": "using System.Text.Json.Serialization;" }
        ]
      }
    ],
    "Go": [],
    "Node": []
  }
}
```

================================================================================
README.md SECTION
================================================================================

## Configuration (`appsettings.json`)

All runtime options, targeting criteria, registry endpoints, AI configurations, and migration rules are controlled via `appsettings.json`. The application looks for this file in the same directory as the executable by default, or via the `--config=/path/to/appsettings.json` CLI argument.

---

### 1. GitLab (`GitLab`)
Configures repository connectivity and bot identity.

- Host: string (default: "https://gitlab.com")
  The base URL of your GitLab instance (cloud or self-hosted).
- Token: string (default: "")
  GitLab Personal or Project Access Token. Requires api, read_repository, and write_repository scopes.
- BotEmail: string (default: "bot@...")
  Email address attributed to automated actions if custom metadata is required.

---

### 2. Target Scope & Filtering (`Target`)
Controls which repositories the tool scans and refactors.

- Type: string (default: "Group")
  Can be "Group" (scans all projects under the group ID) or "Project" (executes against a single repository).
- Id: string (default: "")
  The numeric ID or URL-encoded path of the target group or project. Can be overridden via CLI argument.
- ExcludeGroups: array of strings (default: [])
  List of group names or path prefixes to skip. Any project under these paths will be ignored.
- ExcludeProjects: array of strings (default: [])
  List of project IDs or project path names to exclude from processing.

---

### 3. Notifications (`Notifications`)
Configures proactive webhook alerts when automated runs encounter human interventions.

- GoogleChatWebhook: string
  Incoming Webhook URL for Google Chat. If an open bot MR contains manual commits from human developers, the bot leaves the MR untouched, skips the project, and fires an alert with MR details to this webhook. Leave empty ("") to disable.

---

### 4. AI Provider Settings (`AI`)
Configures the code refactoring engine.

- Provider: string (default: "GEMINI")
  AI backend to use. Options: "OLLAMA", "GEMINI", "OPENAI", or "DUO".
- GeminiApiKey: string (default: "")
  API key when Provider is set to "GEMINI".
- OpenAIApiKey: string (default: "")
  API key when Provider is set to "OPENAI".
- OllamaEndpoint: string (default: "http://localhost:11434/api/chat")
  Full HTTP URL to your Ollama chat endpoint.
- OllamaModel: string (default: "qwen2.5-coder:7b")
  Model tag to load inside Ollama (e.g., qwen2.5-coder:7b, deepseek-coder-v2).

* Self-Correction & No-Op Detection:
  When using "OLLAMA", the adapter monitors for lazy responses. If the model echoes back the unchanged code, the tool triggers a self-correction loop (up to 3 attempts) with targeted warning prompts to enforce code modifications.

---

### 5. Package Registries (`Registries`)
Controls where the tool checks for newer versions of dependencies.

Each item in the array supports:
- Type: The registry protocol ("MavenCentral" or "GitLab").
- Url: Base URL for the registry (e.g., GitLab Package Registry endpoint).
- Token: Auth token required for private registries.
- GroupPrefixes: List of package prefixes directed to this registry (e.g., ["com.mycompany.*"]). Use ["*"] for public catch-alls.

---

### 6. Per-Language Migrations (`Migrations`)
Defines automated upgrade rules split across language ecosystems ("Java", "DotNet", "Go", "Node").

Each entry represents a migration rule:

- OldGroup / OldName:
  Identifier of the existing dependency. For .NET/Node/Go where groups do not exist, leave OldGroup blank ("") and set OldName to the package ID (e.g., "Newtonsoft.Json").
- NewGroup / NewName (Optional):
  Set these only when an artifact is relocated or renamed (e.g., junit:junit -> org.junit.jupiter:junit-jupiter-api). For standard in-place version bumps, omit or leave blank.
- MaxOldVersion / MinNewVersion (Optional):
  Semantic version boundaries.
  - MaxOldVersion: The upper limit of the current project version to qualify for this rule.
  - MinNewVersion: The minimum target version required to trigger this rule.
  - Omit both to run the rule unconditionally on any version change.
- MigrationDocPath (Optional):
  Path to a curated local Markdown file containing API changes and before/after code examples. Loaded at startup and injected into the AI prompt to bypass LLM knowledge cutoffs.
- Replacements (Optional):
  Array of direct text substitutions (Search and Replace). Executed across source files for fast, deterministic namespace and import updates without consuming AI compute.

#### Migration Stacking:
If a project updates across multiple major versions (e.g., from v3 to v5), defining separate rules with respective version boundaries causes the tool to automatically stack all qualifying migration documents and execute their string replacements sequentially in a single Merge Request.

📦 Multi-Registry & Internal Libraries

The tool utilizes a Composite Registry Router.
If a dependency in build.gradle matches a GroupPrefix (e.g., uk.co.tpplc), the tool queries the private GitLab registry.

Smart AI Bypassing: When an internal library is bumped, the build.gradle is updated, but the AI refactoring engine is explicitly skipped for that library. This prevents the AI from halluincating APIs for proprietary code it has no knowledge of.


## Example migration document:

``` text/plain
# Micronaut 4 to Micronaut 5 Migration Guide

You are migrating Java/Kotlin code from Micronaut 4 to Micronaut 5.1.x.

### Critical API Changes:
1. Nullability Annotations: Micronaut 5 formally adopts JSpecify. Replace `javax.annotation.Nullable`, `jakarta.annotation.Nullable`, or `io.micronaut.core.annotation.Nullable` with `org.jspecify.annotations.Nullable` (and similarly for `@NonNull`).
2. Reactive Streams: RxJava 2 is no longer supported. You must migrate RxJava 2 imports (`io.reactivex.*`) to RxJava 3 (`io.reactivex.rxjava3.core.*`) or Project Reactor (`reactor.core.publisher.*`). Replace `Single<T>` with `Mono<T>` or `io.reactivex.rxjava3.core.Single<T>`.
3. Micronaut Views: The `@TurboView` annotation has been renamed to `@TurboStreamView`.
4. Embedded Data: If using embedded fields in Micronaut Data, the embedded naming strategy changed. Ensure fields inside `@Embeddable` classes are correctly mapped if they previously relied on legacy implicit naming.

### Example BEFORE:
    import io.micronaut.core.annotation.Nullable;
    import io.micronaut.views.turbo.TurboView;
    import io.reactivex.Single;
    import io.micronaut.http.annotation.Get;
    import io.micronaut.http.annotation.Controller;

    @Controller("/api")
    public class LegacyController {

        @Get("/view")
        @TurboView("my-view")
        public Single<String> renderView(@Nullable String name) {
            return Single.just(name == null ? "Default" : name);
        }
    }

### Example AFTER:
    import org.jspecify.annotations.Nullable;
    import io.micronaut.views.turbo.TurboStreamView;
    import reactor.core.publisher.Mono;
    import io.micronaut.http.annotation.Get;
    import io.micronaut.http.annotation.Controller;

    @Controller("/api")
    public class LegacyController {

        @Get("/view")
        @TurboStreamView("my-view")
        public Mono<String> renderView(@Nullable String name) {
            return Mono.just(name == null ? "Default" : name);
        }
    }

```

## 🤝 Contributing

When adding new AI providers (e.g., Anthropic Claude, Groq):

1. Create a new adapter class in `src/adapters/` that inherits from `IAICodeAssistant`.
2. Implement the `RefactorCode` and `GenerateMergeRequestDescription` methods.
3. Add your provider string to the environment variable parser in `src/main.cpp`.

---

## 📄 License

MIT License

