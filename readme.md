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


Example appsettings.json
``` JSON

{
  "GitLab": {
    "Host": "[https://gitlab.com](https://gitlab.com)",
    "Token": "glpat-YOUR_TOKEN"
  },
  "AI": {
    "Provider": "GEMINI",
    "GeminiApiKey": "AIzaSy...",
    "OpenAIApiKey": "",
    "OllamaEndpoint": "http://localhost:11434/api/generate",
    "OllamaModel": "qwen2.5-coder:7b"
  },
  "Registries": [
    {
      "Type": "GitLab",
      "Url": "[https://gitlab.com/api/v4/projects/44020136/packages/maven](https://gitlab.com/api/v4/projects/44020136/packages/maven)",
      "Token": "glpat-REGISTRY_READ_TOKEN",
      "GroupPrefixes": ["uk.co.tpplc"]
    },
    {
      "Type": "MavenCentral",
      "Url": "[https://repo1.maven.org/maven2](https://repo1.maven.org/maven2)",
      "Token": "",
      "GroupPrefixes": ["*"] 
    }
  ],
  "Migrations": [
    {
      "OldGroup": "junit",
      "OldName": "junit",
      "NewGroup": "org.junit.jupiter",
      "NewName": "junit-jupiter-api",
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
      "OldGroup": "org.mockito",
      "OldName": "mockito-all",
      "NewGroup": "org.mockito",
      "NewName": "mockito-core",
      "Replacements": []
    },
    {
      "OldGroup": "javax.jms",
      "OldName": "javax.jms-api",
      "NewGroup": "jakarta.jms",
      "NewName": "jakarta.jms-api",
      "Replacements": [
        { "Search": "import javax.jms", "Replace": "import jakarta.jms" }
      ]
    },
    {
      "OldGroup": "javax.servlet",
      "OldName": "javax.servlet-api",
      "NewGroup": "jakarta.servlet",
      "NewName": "jakarta.servlet-api",
      "Replacements": [
        { "Search": "import javax.servlet", "Replace": "import jakarta.servlet" }
      ]
    },
    {
      "OldGroup": "javax.annotation",
      "OldName": "javax.annotation-api",
      "NewGroup": "jakarta.annotation",
      "NewName": "jakarta.annotation-api",
      "Replacements": [
        { "Search": "import javax.annotation", "Replace": "import jakarta.annotation" }
      ]
    },
    {
      "OldGroup": "javax.xml.bind",
      "OldName": "jaxb-api",
      "NewGroup": "jakarta.xml.bind",
      "NewName": "jakarta.xml.bind-api",
      "Replacements": [
        { "Search": "import javax.xml.bind", "Replace": "import jakarta.xml.bind" }
      ]
    },
    {
      "OldGroup": "javax.validation",
      "OldName": "validation-api",
      "NewGroup": "jakarta.validation",
      "NewName": "jakarta.validation-api",
      "Replacements": [
        { "Search": "import javax.validation", "Replace": "import jakarta.validation" }
      ]
    },
    {
      "OldGroup": "org.assertj",
      "OldName": "",
      "NewGroup": "org.assertj",
      "NewName": "",
      "Replacements": [
        { "Search": "org.assertj.core.api.Java6Assertions", "Replace": "org.assertj.core.api.Assertions" }
      ]
    }
  ]
}
```

📦 Multi-Registry & Internal Libraries

The tool utilizes a Composite Registry Router.
If a dependency in build.gradle matches a GroupPrefix (e.g., uk.co.tpplc), the tool queries the private GitLab registry.

Smart AI Bypassing: When an internal library is bumped, the build.gradle is updated, but the AI refactoring engine is explicitly skipped for that library. This prevents the AI from halluincating APIs for proprietary code it has no knowledge of.

## 🤝 Contributing

When adding new AI providers (e.g., Anthropic Claude, Groq):

1. Create a new adapter class in `src/adapters/` that inherits from `IAICodeAssistant`.
2. Implement the `RefactorCode` and `GenerateMergeRequestDescription` methods.
3. Add your provider string to the environment variable parser in `src/main.cpp`.

---

## 📄 License

MIT License

