# Getting Started with Thoth

**Last Updated:** 2026-07-12  
**Purpose:** Complete setup guide for building and running Thoth

---

## Prerequisites

### Required System Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    libwxgtk3.2-dev \
    pkg-config
```

**macOS (Homebrew):**
```bash
brew install cmake curl sqlite wxwidgets pkg-config
```

**Fedora/RHEL:**
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    libcurl-devel \
    sqlite-devel \
    wxGTK3-devel \
    pkgconfig
```

### Compiler Requirements

- **C++20** compiler required
- **GCC 10+** or **Clang 12+** (Linux/macOS)
- **MSVC 2019+** (Windows, not fully tested)

### CMake Version

- **CMake 3.16+** required (3.20+ recommended)

---

## External Services

### Ollama (Required for Local LLM)

Thoth uses Ollama for local language model inference and embeddings.

**Installation:**
```bash
# Linux/macOS
curl -fsSL https://ollama.com/install.sh | sh

# Or download from: https://ollama.com/download
```

**Required Models:**
```bash
# Pull the language model (adjust model name as needed)
ollama pull qwen2.5:3b  # or your preferred model

# Pull the embedding model (required for GRAG)
ollama pull nomic-embed-text
```

**Verify Installation:**
```bash
ollama list  # Should show your models
ollama run qwen2.5:3b "Hello"  # Test inference
```

### Optional: OpenAI API

If you want to use OpenAI instead of Ollama:

1. Get an API key from https://platform.openai.com/
2. Set environment variable: `export OPENAI_API_KEY="sk-..."`
3. Configure in Thoth settings (GUI) or `config.json`

---

## Building Thoth

### Step 1: Clone the Repository

```bash
git clone <repository-url>
cd Thoth
```

### Step 2: Configure with CMake Presets

Thoth uses CMake presets for consistent builds:

```bash
# Debug build (default, recommended for development)
cmake --preset debug

# Release build (optimized)
cmake --preset release

# Debug with AddressSanitizer (for memory debugging)
cmake --preset asan

# Debug with ThreadSanitizer (for concurrency debugging)
cmake --preset tsan

# Headless engine only (no wxWidgets required)
cmake --preset engine-only
```

**Build Directory:** Presets create builds in `build/debug`, `build/release`, `build/engine-only`, etc.

### Step 3: Build

```bash
# Using preset (recommended)
cmake --build --preset build-debug

# Or manually
cd build/debug
make -j$(nproc)  # Linux
# or
make -j$(sysctl -n hw.ncpu)  # macOS
```

**Output:**
- GUI executable: `build/debug/thoth-control-panel`
- Headless engine: `build/debug/external/basic_agent/thoth-engine`
- Core library: `build/debug/libbasic_agent.so` (Linux) or `.dylib` (macOS)
- Unit tests (core): `build/debug/tests/thoth-core-tests`
- Unit tests (GUI): `build/debug/tests/thoth-gui-tests` (when GUI is built)

### Step 4: Verify Build

```bash
# Check executable exists
ls -lh build/debug/thoth-control-panel

# Run unit tests
# Core suite (no wxWidgets):
ctest --test-dir build/debug -R thoth-core-tests --output-on-failure

# GUI lifecycle tests (wxWidgets required):
ctest --test-dir build/debug -L gui --output-on-failure

# Quick dev loop (~70s, no Ollama): dev TEST_SUITE + reflection A/B + robustness suite
ctest --test-dir build/debug -L fast --output-on-failure

# Individual cognitive harnesses (optional):
# ./build/debug/tests/run_test_suite --dev
# ./build/debug/external/basic_agent/run_reflection_ab_benchmark
# ./build/debug/external/basic_agent/run_robustness_suite

# PR-equivalent (core unit + cognitive; no wxWidgets on engine-only):
ctest --test-dir build/debug -L pr --output-on-failure

# Full local coverage (core + GUI):
ctest --test-dir build/debug -L "pr|gui" --output-on-failure

# Full Ollama regression (~40 min; nightly / manual only)
cmake --preset debug -DTHOTH_TEST_SUITE_FULL=ON
cmake --build --preset build-debug
ctest --test-dir build/debug -R test-suite-full --output-on-failure --timeout 3600
```

### Headless engine (`thoth-engine`)

The headless binary runs the full cognitive stack (`BasicAgentPlugin`) without wxWidgets. Use the `engine-only` preset on machines without GUI dependencies, or the default `debug` preset when both GUI and engine are needed.

```bash
# Build engine only (no wxWidgets)
cmake --preset engine-only
cmake --build --preset build-engine-only

# One-shot prompt (scripts / CI smoke)
export THOTH_WORKSPACE_PATH=/tmp/thoth-ws
export THOTH_INFERENCE_BASE_URL=http://127.0.0.1:11434
mkdir -p "$THOTH_WORKSPACE_PATH"
./build/engine-only/external/basic_agent/thoth-engine --execute "/prune status"

# Interactive REPL (stdin)
./build/engine-only/external/basic_agent/thoth-engine
```

**Modes:**
- `--help` / `--version` — usage and version
- `--execute "<prompt>"` — single `processInput()` call, then exit
- `--serve [--bind HOST] [--port PORT]` — HTTP API server (Plan F)
- (default) — read prompts from stdin until EOF or Ctrl+C

**HTTP server (`--serve`):**

Configuration precedence: CLI flags → environment variables → defaults.

| Setting | CLI | Environment | Default |
|---------|-----|-------------|---------|
| Bind address | `--bind` | `THOTH_ENGINE_BIND` | `127.0.0.1` |
| Port | `--port` | `THOTH_ENGINE_PORT` | `8090` |

```bash
./build/engine-only/external/basic_agent/thoth-engine --serve --port 8090

# Ops
curl -s http://127.0.0.1:8090/health
curl -s http://127.0.0.1:8090/ready
curl -s http://127.0.0.1:8090/version

# API v1
curl -s -X POST http://127.0.0.1:8090/v1/chat \
  -H 'Content-Type: application/json' \
  -d '{"text":"/help","session_id":"default"}'

curl -s -X POST http://127.0.0.1:8090/v1/goals \
  -H 'Content-Type: application/json' \
  -d '{"goal":"...","session_id":"default"}'

curl -s -X POST http://127.0.0.1:8090/v1/control/pause
curl -s -X POST http://127.0.0.1:8090/v1/control/resume
curl -s -X POST http://127.0.0.1:8090/v1/control/abort

# SSE observability (Plan G)
curl -N -H 'Accept: text/event-stream' http://127.0.0.1:8090/v1/events
```

`GET /ready` `capabilities` describe **engine features** (`EngineRuntime`), not which HTTP routes are wired in a given build checkpoint. When SSE is live, capabilities include `"events"`.

`GET /v1/events` streams live `EngineEvent` JSON over SSE. See [`ENGINE_EVENTS.md`](ENGINE_EVENTS.md) for guarantees, event types, and reconnect guidance.

`POST /v1/goals` returns after the engine accepts the goal and completes **initial planning**; step execution continues asynchronously on the executive loop.

Graceful shutdown: `SIGINT` / `SIGTERM` stop accepting new HTTP connections (including SSE), return `503 ENGINE_BUSY` to new API requests, drain the command queue (bounded, default 5 s), flush open SSE clients, then tear down the plugin. Suitable for container stop signals.

Specs: [`plan_f_engine_runtime_http.md`](plan_f_engine_runtime_http.md) · [`plan_g_streaming_observability.md`](plan_g_streaming_observability.md)

### Docker Compose (Plan I)

Packaging-only stack: `thoth-engine` + `llama-server` with named volumes. Plans F–H runtime code is unchanged inside the container.

```bash
# From repo root — see docker/README.md for model setup
docker compose up -d --build
curl -s http://127.0.0.1:8090/health
./docker/smoke.sh   # full 8-step verification (models required for /v1/goals)
```

Operator guide: [`docker/README.md`](../docker/README.md) · Spec: [`plan_i_docker_compose_v1.md`](plan_i_docker_compose_v1.md)

Host-native `cmake --preset engine-only` remains the primary developer workflow; Docker is additive.

### Hybrid development (roadmap Step 6 + Plan K)

Run the engine in Compose; drive it from the host with `curl` (or scripts) on `http://127.0.0.1:8090`. Optionally point the host GUI at that engine with **`THOTH_ENGINE_URL`**.

```bash
docker compose up -d --build
curl -sf http://127.0.0.1:8090/ready | jq .
curl -s -X POST http://127.0.0.1:8090/v1/chat \
  -H 'Content-Type: application/json' \
  -d '{"text":"/help","session_id":"hybrid"}'
curl -N -H 'Accept: text/event-stream' http://127.0.0.1:8090/v1/events

# Optional — remote GUI (restart to switch; unset → in-process Local)
export THOTH_ENGINE_URL=http://127.0.0.1:8090
./build/debug/thoth-control-panel
```

**Limitations:** cognate side panels stay empty in remote mode (no cognate HTTP APIs yet). The remote GUI does **not** own or synchronize the engine workspace — Compose volumes are separate from host `agent_workspace/`. Engine may be offline at GUI launch; failures appear when chat/goals/SSE are used. For a usable Compose RAG corpus, use the engine-owned workflow: `./docker/seed-workspace.sh` then the Plan L L2 checklist in [`docker/README.md`](../docker/README.md#plan-l-l2--seed--restart--retrieval-evidence-docs-checklist) ([Plan L](plan_l_workspace_corpus.md) ✅ Complete (L3 deferred)). Chat grounding semantics (`grounded`, greeting skip, attempt vs success telemetry) are defined in [Plan M](plan_m_grounded_retrieval_gate.md) ✅ Complete — read `retrieval_ran` / `retrieval_skip_reason` alongside `grounding_mode`.

**Local regression:** unset `THOTH_ENGINE_URL` → `[AgentInterface] backend=local` at startup; in-process chat/goals unchanged from pre–Plan K. Remote is additive only.

Full hybrid notes: [`docker/README.md` § Hybrid development](../docker/README.md#hybrid-development) · smoke checklist: [`docker/README.md` § GUI remote smoke checklist](../docker/README.md#gui-remote-smoke-checklist-plan-k5) · Plan K: [`plan_k_gui_api_client.md`](plan_k_gui_api_client.md)

**Portable paths** (see Environment Variables):
- `THOTH_WORKSPACE_PATH` — workspace (`memory.db`, `rag/`, config)
- `THOTH_LOGS_PATH` — benchmark and metrics JSONL logs
- `THOTH_INFERENCE_BASE_URL` — inference service origin (Ollama default `http://127.0.0.1:11434`; llama-server e.g. `http://127.0.0.1:8080`)
- `THOTH_INFERENCE_BACKEND` — `ollama` (default) or `llama_cpp`
- `THOTH_EMBED_BASE_URL` — optional separate embed endpoint (defaults to inference base)

**Tests on engine-only builds:** `ctest -L pr` runs the full PR suite (core unit tests + cognitive/Python tests) without wxWidgets.

**Test targets:** `thoth-core-tests` (engine/core), `thoth-gui-tests` (GUI lifecycle). `cmake --build --target thoth-unit-tests` is a build alias for both binaries; use `ctest -R thoth-core-tests` instead of the old `ctest -R thoth-unit-tests` name.

### Longitudinal analysis (C6 Phase 3)

After production logs have accumulated, run from the repository root:

```bash
python3 scripts/analyze_cognitive_longitudinal.py
```

Operator procedures, expected outputs, and troubleshooting: **[cognitive_longitudinal_ops.md](cognitive_longitudinal_ops.md)**. Normative semantics: `C6_phase3_analyzer_contract.md` and `C6_phase3_reporting_contract.md`.

---

## First Run

### Initialize Workspace

On first run, Thoth will create the `agent_workspace/` directory structure:

```
agent_workspace/
├── memory.db              # SQLite database (created automatically)
├── chat_sessions.json     # Conversation history
├── rag/                   # Sandboxed RAG corpus
├── rag_index.bin          # Vector embeddings index
├── retrieval_config.json  # GRAG weight configuration
└── decision_trace.jsonl   # Execution trace log
```

### Launch the GUI

```bash
./build/debug/thoth-control-panel
```

**First Launch Checklist:**
1. ✅ GUI window opens
2. ✅ Check status bar for "Ollama connection" status
3. ✅ Verify `agent_workspace/` directory was created
4. ✅ Try a simple query: "What is Thoth?"

### Verify Ollama Connection

The GUI should show Ollama connection status. If it fails:

1. **Check Ollama is running:**
   ```bash
   curl http://localhost:11434/api/tags
   ```

2. **Check required models are available:**
   ```bash
   ollama list
   # Should show: qwen2.5:3b (or your model) and nomic-embed-text
   ```

3. **Test embedding endpoint:**
   ```bash
   curl http://localhost:11434/api/embed -d '{"model":"nomic-embed-text","prompt":"test"}'
   ```

---

## Configuration

### Environment Variables

Create a `.env` file in the project root (optional). Entry points (`thoth-engine`, GUI app, `run_test_suite`) load it automatically at startup via `bootstrapRuntimeEnvironment()`.

**Precedence:** exported shell environment > `.env` > `config.json` > defaults.

Point to a custom file with `THOTH_ENV_PATH=/path/to/.env`.

Print resolved paths at startup with `THOTH_LOG_CONFIG=1`:

```bash
THOTH_LOG_CONFIG=1 ./build/debug/external/basic_agent/thoth-engine --version
```

```bash
# .env file (DO NOT COMMIT)
THOTH_WORKSPACE_PATH=          # optional: override agent_workspace location
THOTH_LOGS_PATH=               # optional: override logs/ directory
THOTH_INFERENCE_BASE_URL=http://127.0.0.1:11434
THOTH_INFERENCE_BACKEND=ollama   # ollama | llama_cpp
THOTH_LOG_CONFIG=0             # set to 1 to print resolved startup paths
OLLAMA_HOST=localhost:11434    # compat alias for inference host
OLLAMA_MODEL=qwen2.5:3b
OLLAMA_EMBED_MODEL=nomic-embed-text

# Optional: OpenAI (if not using Ollama)
# OPENAI_API_KEY=sk-...
```

### Runtime Configuration

Configuration is stored in `agent_workspace/` and managed via:
- GUI settings panel
- `config.json` (if using CLI mode)
- Environment variables (override)

**Key Settings:**
- `enable_tools`: Enable/disable tool system (default: true)
- `temperature`: LLM temperature (default: 0.7)
- `max_tokens`: Maximum response tokens
- `verbosity`: Logging verbosity level

---

## Troubleshooting

### Build Issues

**"wxWidgets not found":**
```bash
# Ubuntu/Debian
sudo apt-get install libwxgtk3.2-dev

# macOS
brew install wxwidgets
```

**"CURL not found":**
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev

# macOS
brew install curl
```

**"SQLite3 not found":**
```bash
# Ubuntu/Debian
sudo apt-get install libsqlite3-dev

# macOS (usually pre-installed)
```

**CMake version too old:**
```bash
# Install latest CMake
# Ubuntu: Download from cmake.org or use snap
sudo snap install cmake --classic

# macOS
brew install cmake
```

### Runtime Issues

**"Ollama connection failed":**
- Verify Ollama is running: `ollama list`
- Check firewall/network settings
- Verify `OLLAMA_HOST` environment variable

**"Model not found":**
- Pull required models: `ollama pull qwen2.5:3b`
- Check model name matches configuration

**"Permission denied" on workspace:**
```bash
chmod -R u+w agent_workspace/
```

**GUI won't start:**
- Check wxWidgets installation
- Verify display server (X11/Wayland) is running
- Check logs in `agent_workspace/app_log.jsonl`

### Test Failures

**Unit tests fail:**
- Ensure workspace directory is writable
- Check Ollama is running (some tests require it)
- Run core unit tests: `./build/debug/tests/thoth-core-tests`

**Integration tests fail:**
- Verify `agent_workspace/rag/` has bootstrap corpus
- Check SQLite database permissions
- Review `logs/decision_trace.jsonl` for errors

---

## Development Setup

### Recommended IDE Setup

**VS Code:**
- Install C/C++ extension
- Install CMake Tools extension
- Configure `settings.json`:
  ```json
  {
    "cmake.configureSettings": {
      "CMAKE_BUILD_TYPE": "Debug"
    }
  }
  ```

**CLion:**
- Open project root
- CMake will auto-configure
- Use built-in debugger

### Debugging

**Using GDB:**
```bash
gdb ./build/debug/thoth-control-panel
(gdb) run
```

**Using AddressSanitizer:**
```bash
cmake --preset asan
cmake --build --preset build-asan
./build/asan/thoth-control-panel
# ASan will report memory errors automatically
```

**Using ThreadSanitizer:**
```bash
cmake --preset tsan
cmake --build --preset build-tsan
./build/tsan/thoth-control-panel
# TSan will report race conditions automatically
```

### Code Formatting

Thoth follows standard C++ conventions:
- 4 spaces indentation
- `snake_case` for files/functions
- `PascalCase` for classes
- See `AGENTS.md` for full conventions

---

## Next Steps

1. **Read the Architecture Guide:** [AGENTS.md](../AGENTS.md)
2. **Explore the System:** [README.md](README.md)
3. **Run Tests:** [TESTING.md](TESTING.md)
4. **Check Active Roadmap:** [improvements.md](improvements.md)

---

## Quick Reference

### Build Commands
```bash
# Configure
cmake --preset debug

# Build
cmake --build --preset build-debug

# Headless engine only
cmake --preset engine-only
cmake --build --preset build-engine-only

# Test
cd build/debug && ctest

# Run GUI
./build/debug/thoth-control-panel

# Run headless engine
./build/debug/external/basic_agent/thoth-engine --help
```

### Directory Structure
```
Thoth/
├── build/              # Build outputs (gitignored)
├── src/                # GUI source code
├── external/basic_agent/  # Core agent library
├── agent_workspace/    # Runtime state (created on first run)
├── logs/              # Runtime logs
├── tests/             # Unit tests
└── docs/              # Documentation
```

### Important Files
- `CMakeLists.txt` - Main build configuration
- `CMakePresets.json` - Build presets
- `AGENTS.md` - Architecture guide
- `agent_workspace/retrieval_config.json` - GRAG weights (locked)

---

**Need Help?** Check [INDEX.md](INDEX.md) for documentation navigation.
