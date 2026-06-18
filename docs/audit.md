# Thoth Codebase Audit: Current State

**Last Updated:** 2026-06-18  
**Audit Date:** 2026-06-18  
**Status:** ✅ Current — Verified against codebase (automated checklist + `ctest`)

**Verification run (2026-06-18):**
- `ctest --output-on-failure`: **100% pass** (~100s)
- Tool confirmation, `ConstraintChecker`, sandbox reject, and `apply_diff` stub: confirmed via grep (see §7)
- **Manual** `TEST_SUITE.md` TC-01–TC-07: not re-run this session (requires GUI + live LLM); last historical pass 2026-03-12 in `VERIFIED_BASELINE.md`

---

## 1. Sandbox & File Access Boundaries

### code_modify Write Paths

- **Allowlist Logic:** `CodeModifyTool::isPathSafe()` enforces that read / apply_diff / revert operations stay within the project root. Absolute paths and `..` traversal are rejected.
- **Project Root:** `FileHandler::getProjectRoot()` — walks up from CWD for `GEMINI.md` or `.git`, else derives from executable path. Override: `THOTH_PROJECT_ROOT` env var.
- **Reality:** `code_modify` can read any file under project root (with confirmation). `apply_diff` is a **stub** returning: `"Unified diff application not fully implemented in v1.0 prototype. Harness is ready."`
- **Confirmation Required:** ✅ `code_modify` requires confirmation

### RAG Bootstrap Indexer Scan Paths

- **✅ STRICT SANDBOX ENFORCEMENT:** `IndexManager::indexFile()` and `IndexManager::indexProject()` **hard-reject** paths outside `FileHandler::getAgentWorkspacePath()` (default: `<project_root>/agent_workspace/`). Override: `THOTH_WORKSPACE_PATH` env var.
- **Bootstrap Logic:** `CommandProcessor::ensureInitialized()` indexes `agent_workspace/rag/` only when the index is empty.
- **Internal Exclusions:** `indexProject()` skips `rag_index.bin`, `/build/`, `/.git/`, and `/docs/`.
- **Reality:** RAG indexing does **not** scan the live codebase tree.

### Boundary Comparison

- **Separation:** `code_modify` is anchored to **project root**; RAG indexing is limited to **agent_workspace/**.
- **Live Codebase Access:**
  - ✅ **Read:** `code_modify` read (confirmation required)
  - ⚠️ **Write:** `apply_diff` stub only
  - ✅ **RAG:** sandbox only

---

## 2. Registered Tools

The `ToolRegistry` contains **9 tools** (all registered):

| Tool Name | Purpose | Access/Permissions | Requires Confirmation | Status |
|-----------|---------|-------------------|----------------------|--------|
| `summarize_text` | Summarizes input text via LLM | Internal (LLM) | ❌ No | ✅ Active |
| `gmail_read_labels` | Lists Gmail labels | Network (Google API) | ✅ **Yes** | ✅ Active |
| `project_analyze` | Lists files and directories | Filesystem (Read-only, Project Root) | ❌ No | ✅ Active |
| `run_tests` | Executes unit test suite | Shell (`popen` when enabled) | ✅ **Yes** | ✅ Active |
| `code_modify` | Read/Modify/Build codebase | Filesystem + Shell (build) | ✅ **Yes** | ⚠️ `apply_diff` stub |
| `web_scrape` | Scrapes text from a URL | Network (libcurl) | ❌ No | ✅ Active |
| `gmail_read_messages` | Reads Gmail messages | Network (Google API) | ✅ **Yes** | ✅ Active |
| `store_fact` | Stores facts in FactStore | SQLite (memory.db) | ❌ No | ✅ Active |
| `self_correct` | Verifies results against expectations | Internal (LLM) | ❌ No | ✅ Active |

### Tool Confirmation System

**✅ IMPLEMENTED:** `ToolRegistry::executeTool()` checks `requires_confirmation()` before execution; unconfirmed calls return `"requires_confirmation": true`.

**Tools requiring confirmation:** `gmail_read_labels`, `run_tests`, `code_modify`, `gmail_read_messages`

### Config Defaults

- `enable_tools`: `true`
- `allow_network`: `true`
- `allow_web`: `true`
- `allow_file_io`: `true`
- `allow_shell_exec`: `false` (default)

### Shell execution gating (P1.5 — fixed 2026-06-15)

**✅ ENFORCED:** `RunTestsTool` and `CodeModifyTool` (build operation) check `Config::allow_shell_exec` via `ToolRegistry::setConfig()` before `popen`. When false, tools return an error referencing `allow_shell_exec`. Unit test: `testAllowShellExecGate`.

---

## 3. Security Enforcement

### ConstraintChecker Integration

**✅ FULLY INTEGRATED** in standard chat (`CommandProcessor::processToolCall`) and goal execution (`ExecutiveController`).

**Action type mapping (examples):**
- `code_modify` read → `file_read`
- `code_modify` apply_diff → `file_modify`
- `web_scrape` / Gmail → `network_request`

Blocked actions return the policy reason from `check_action()`.

### Sandbox Boundaries

**✅ STRICTLY ENFORCED** in `IndexManager` — paths outside `agent_workspace/` log `[SECURITY] REJECTED path outside sandbox: …` and are rejected.

---

## 4. agent_workspace Contents

Managed by the memory system — **do not edit manually.**

| Path | Role |
|------|------|
| `memory.db` | Episodic, plan history, graph, step metrics |
| `chat_sessions.json` | Conversation history (GUI hot-tier trim active) |
| `rag/` | Sandboxed RAG corpus (bootstrap source) |
| `rag_index.bin` | Vector index |
| `retrieval_config.json` | GRAG weights (runtime; gitignored in dev) |
| `decision_trace.jsonl` | Pipeline trace (also under `logs/` in some setups) |
| `grag_benchmark.jsonl` | Retrieval math diagnostics |

**RAG index:** bootstrap from `agent_workspace/rag/` only; typical research corpus ~200–500 chunks.

---

## 5. Documentation vs Reality

### ✅ Accurate Claims

- Tool confirmation system
- `ConstraintChecker` on both execution paths
- Sandbox boundaries in `IndexManager`
- Nine registered tools
- `allow_shell_exec` gating on shell-backed tools (P1.5)
- Plan history reuse, memory hot-tier pruning (2026-06-16)

### ⚠️ Known Gaps (honest)

1. **`code_modify apply_diff` stub** — Self-building / diff apply is **optional future expansion**, not active work. Harness tools (`project_analyze`, `run_tests`, read) work.
2. **Trace replay** — Observability only; authoritative resume is SQLite (`resume_from_plan()`).
3. **Trajectory $w_t$ tuning** — Weight active locally; mixed lift on `TRAJECTORY_DISAMBIGUATES` benchmark cases (`plan_reuse_tuning.md`).
4. **Hierarchical subgoals** — Not implemented (single root goal embedding for full plan).

### ✅ Security improvements (not gaps)

- RAG bootstrap scoped to `agent_workspace/rag/` (was broader in early prototypes)

---

## 6. What the Agent Can Do Autonomously Right Now

### ✅ Capabilities

- Multi-step plans via `ExecutiveController` (RETRIEVAL + LLM, tools, scientific mode)
- GRAG directional retrieval with graph learning
- Plan history reuse and strategy promotion
- Read project files (`code_modify` read, confirmation)
- Run unit tests (`run_tests`, confirmation + `allow_shell_exec`)
- Scrape web, Gmail (with confirmation), store facts, self-correct reasoning
- Hot-tier memory pruning and session scoping

### ⚠️ Limitations

1. Cannot apply unified diffs (`apply_diff` stub)
2. RAG corpus limited to sandbox — not live codebase indexing
3. NODE execution harness — spec only (`NODE.md`)

### 🔒 Security Posture

**Strengths:** confirmation gates, `ConstraintChecker`, sandbox indexing, path validation, shell flag gating.

**Residual risks:** `code_modify` read spans project root (confirmation required); goals requiring real code edits may loop until `apply_diff` exists.

---

## 7. Verification Checklist

Run after security-sensitive changes:

```bash
# Build + unit tests
cmake --build --preset build-debug
cd build/debug && ctest --output-on-failure

# Tool confirmation
rg "requires_confirmation" external/basic_agent/src/tools.cpp

# ConstraintChecker
rg "constraint_checker_.check_action" external/basic_agent/src/command_processor.cpp

# Sandbox
rg "REJECTED path outside sandbox" external/basic_agent/src/index_manager.cpp

# Shell gating
rg "allow_shell_exec" external/basic_agent/src/run_tests_tool.cpp external/basic_agent/src/code_modify_tool.cpp

# apply_diff stub (expected until Phase 5 resumes)
rg "not fully implemented" external/basic_agent/src/code_modify_tool.cpp
```

**Manual pipeline regression:** `docs/TEST_SUITE.md` TC-01–TC-07 (GUI + Ollama). Record results in `VERIFIED_BASELINE.md` §5.

---

## 8. Recommendations

### High Priority

1. **Re-run manual TEST_SUITE** before major merges (TC-01–TC-07).
2. **Finish Phase 3 memory** — warm-tier summarize, restore path (`cursor_list.md` M1–M4).

### Medium Priority

3. **Trajectory / benchmark tuning** — improve `TRAJECTORY_DISAMBIGUATES` scores (`new_corpus_tests.md`, `plan_reuse_tuning.md`).
4. **Automate critical TC signals** — integration tests for `routing_mode`, alpha, RETRIEVAL-before-LLM.

### Low Priority / Future

5. **`apply_diff` implementation** — only after internals stable (Phase 5 optional expansion).
6. **Review confirmation on `web_scrape`** — optional hardening.

---

**Next audit:** After manual TEST_SUITE pass or major security / tool changes.
