# Getting Started with Thoth

**Last Updated:** 2026-03-10  
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
```

**Build Directory:** Presets create builds in `build/debug`, `build/release`, etc.

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
- Main executable: `build/debug/thoth-control-panel`
- Core library: `build/debug/libbasic_agent.so` (Linux) or `.dylib` (macOS)
- Unit tests: `build/debug/tests/thoth-unit-tests`

### Step 4: Verify Build

```bash
# Check executable exists
ls -lh build/debug/thoth-control-panel

# Run unit tests
# Quick dev loop (~70s, no Ollama): dev TEST_SUITE + reflection A/B + robustness suite
ctest --test-dir build/debug -L fast --output-on-failure

# Individual cognitive harnesses (optional):
# ./build/debug/tests/run_test_suite --dev
# ./build/debug/external/basic_agent/run_reflection_ab_benchmark
# ./build/debug/external/basic_agent/run_robustness_suite

# PR-equivalent (unit + cognitive):
ctest --test-dir build/debug -L pr --output-on-failure

# Full Ollama regression (~40 min; nightly / manual only)
cmake --preset debug -DTHOTH_TEST_SUITE_FULL=ON
cmake --build --preset build-debug
ctest --test-dir build/debug -R test-suite-full --output-on-failure --timeout 3600
```

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

Create a `.env` file in the project root (optional):

```bash
# .env file (DO NOT COMMIT)
OLLAMA_HOST=localhost:11434
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
- Run with verbose output: `./tests/thoth-unit-tests --verbose`

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

# Test
cd build/debug && ctest

# Run
./build/debug/thoth-control-panel
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
