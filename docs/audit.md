# Thoth Codebase Audit: Current State

**Last Updated:** 2026-03-10  
**Audit Date:** 2026-03-10  
**Status:** ✅ Current — Verified against codebase

---

## 1. Sandbox & File Access Boundaries

### code_modify Write Paths

- **Allowlist Logic:** The `CodeModifyTool::isPathSafe()` method enforces that all file operations (read, apply_diff, revert) must be within the project root. It rejects absolute paths and path traversal (`..`).
- **Project Root:** Determined by `FileHandler::getProjectRoot()`, which defaults to `/home/steve/Thoth` (the directory containing the project root marker).
- **Reality:** `code_modify` can theoretically write to any file in the live codebase, though the `apply_diff` operation is currently a **stub** that returns an error: `"Unified diff application not fully implemented in v1.0 prototype. Harness is ready."`
- **Confirmation Required:** ✅ `code_modify` requires confirmation (`requires_confirmation()` returns `true`)

### RAG Bootstrap Indexer Scan Paths

- **✅ STRICT SANDBOX ENFORCEMENT:** `IndexManager::indexFile()` and `IndexManager::indexProject()` now **hard-reject** any paths outside `/home/steve/Thoth/agent_workspace/` with security logging.
- **Bootstrap Logic:** In `CommandProcessor::ensureInitialized()`, if the index is empty, it calls `indexProject(fh.getAgentWorkspacePath("rag/"))` — **sandboxed to agent_workspace/rag/** only.
- **Internal Exclusions:** `IndexManager::indexProject()` recursively scans but explicitly skips: `rag_index.bin`, `/build/`, `/.git/`, and `/docs/`.
- **Reality:** The RAG indexer **only** scans the sandboxed `agent_workspace/rag/` directory. No access to live codebase via RAG indexing.

### Boundary Comparison

- **Separation:** `code_modify` is anchored to project root (`/home/steve/Thoth`), while RAG indexing is **strictly limited** to `agent_workspace/`.
- **Live Codebase Access:** 
  - ✅ **Read access:** Via `code_modify` read operation (requires confirmation)
  - ⚠️ **Write access:** `code_modify` has write-intent but `apply_diff` is a stub
  - ✅ **RAG access:** **Restricted to sandbox only** — no live codebase indexing

---

## 2. Registered Tools

The `ToolRegistry` contains **9 tools** (all registered):

| Tool Name | Purpose | Access/Permissions | Requires Confirmation | Status |
|-----------|---------|-------------------|----------------------|--------|
| `summarize_text` | Summarizes input text via LLM | Internal (LLM) | ❌ No | ✅ Active |
| `gmail_read_labels` | Lists Gmail labels | Network (Google API) | ✅ **Yes** | ✅ Active |
| `project_analyze` | Lists files and directories | Filesystem (Read-only, Project Root) | ❌ No | ✅ Active |
| `run_tests` | Executes unit tests | Shell (Executes `/build/debug/tests/thoth-unit-tests`) | ✅ **Yes** | ✅ Active |
| `code_modify` | Read/Modify/Build codebase | Filesystem (Read/Write, Project Root), Shell (Build) | ✅ **Yes** | ⚠️ `apply_diff` is stub |
| `web_scrape` | Scrapes text from a URL | Network (libcurl) | ❌ No | ✅ Active |
| `gmail_read_messages` | Reads Gmail messages | Network (Google API) | ✅ **Yes** | ✅ Active |
| `store_fact` | Stores facts in FactStore | SQLite (memory.db) | ❌ No | ✅ Active |
| `self_correct` | Verifies results against expectations | Internal (LLM) | ❌ No | ✅ Active |

### Tool Confirmation System

**✅ IMPLEMENTED:** The `requires_confirmation()` system is **fully functional**:
- `ITool` interface includes `virtual bool requires_confirmation() const = 0;`
- `ToolRegistry::executeTool()` enforces confirmation gate:
  ```cpp
  if (tool->requires_confirmation()) {
      if (!input.contains("confirmed") || !input["confirmed"].get<bool>()) {
          return error with "requires_confirmation": true
      }
  }
  ```
- **Tools requiring confirmation:** `gmail_read_labels`, `run_tests`, `code_modify`, `gmail_read_messages`

### Config Defaults

- `enable_tools`: `true` (tools enabled by default)
- `allow_network`: `true` by default
- `allow_web`: `true` by default
- `allow_file_io`: `true` by default
- `allow_shell_exec`: `false` by default, but `run_tests` and `code_modify build` use `popen` directly regardless of this flag (⚠️ **Known gap**)

---

## 3. Security Enforcement

### ConstraintChecker Integration

**✅ FULLY INTEGRATED:** `ConstraintChecker` is now integrated into **both** execution paths:

1. **Standard Chat Loop:** `CommandProcessor::processToolCall()` calls `constraint_checker_.check_action()` before executing any tool (line 346 in `command_processor.cpp`)
2. **Goal Execution:** `ExecutiveController` also uses `ConstraintChecker` for plan execution

**Action Type Mapping:**
- `code_modify` with `operation: "read"` → `file_read`
- `code_modify` with `operation: "apply_diff"` → `file_modify`
- `web_scrape` → `network_request`
- Gmail tools → `network_request`

**Blocking Behavior:** If `check_action()` returns `allowed: false`, the tool execution is blocked and an error is returned with the security policy reason.

### Sandbox Boundaries

**✅ STRICTLY ENFORCED:**
- `IndexManager::indexFile()`: Hard-rejects paths outside `/home/steve/Thoth/agent_workspace/` (line 187-190)
- `IndexManager::indexProject()`: Hard-rejects root paths outside sandbox (line 285-288)
- Security violations are logged: `[SECURITY] REJECTED path outside sandbox: <path>`
- Bootstrap RAG indexing **only** uses `agent_workspace/rag/` directory

---

## 4. agent_workspace Contents

### Files/Directories

- `app_log.jsonl`: General application logs (structured JSONL)
- `chat_sessions.json`: Persistent chat history
- `decision_trace.jsonl`: Detailed event logs (PLAN_CREATED, STEP_STARTED, etc.) — managed by `DecisionTraceLogger`
- `grag_benchmark.jsonl`: Raw benchmark execution data (managed by `RAGPipeline`)
- `memory.db`: SQLite database for:
  - Episodic memory (full execution trajectories)
  - Plan history
  - Step metrics
  - Graph memory (nodes/edges)
- `prompt_templates/`: System and tool prompts
- `rag/`: Sandboxed RAG corpus (bootstrap source)
- `rag_index.bin`: Binary RAG index (vector embeddings)
- `retrieval_config.json`: Optimized GRAG weights ($w_q=0.4, w_d=0.4, w_k=0.3, w_g=0.3$)

### RAG Index Status

- **Bootstrap Source:** `agent_workspace/rag/` (sandboxed)
- **Automatic Bootstrap:** On first run if index is empty
- **Typical Size:** 200-500 chunks for research paper corpus
- **Hard Boundary:** Indexing **only** accepts paths within `agent_workspace/`

---

## 5. Documentation vs Reality

### ✅ Accurate Claims

- **Tool Confirmation System:** ✅ Fully implemented and enforced
- **ConstraintChecker:** ✅ Integrated into both standard chat and goal execution
- **Sandbox Boundaries:** ✅ Strictly enforced in `IndexManager`
- **9 Tools Registered:** ✅ All tools present and accounted for

### ⚠️ Known Gaps / Inaccuracies

1. **code_modify apply_diff is Stub:**
   - `completed_improvements_log.md` claims "Implemented Self-Building Capability... giving the agent tools to... modify its own codebase."
   - **Reality:** `apply_diff` returns error: `"Unified diff application not fully implemented in v1.0 prototype. Harness is ready."`
   - **Status:** Stub — infrastructure exists but diff application not implemented

2. **allow_shell_exec Flag Ignored:**
   - `Config` has `allow_shell_exec` flag (defaults to `false`)
   - `RunTestsTool` and `CodeModifyTool` use `popen` directly without checking this flag
   - **Risk:** Shell execution happens regardless of config setting

3. **RAG Bootstrap Changed:**
   - Old behavior: Indexed entire project root
   - **Current behavior:** Only indexes `agent_workspace/rag/` (sandboxed)
   - This is a **security improvement**, not a gap

---

## 6. What the Agent Can Do Autonomously Right Now

### ✅ Capabilities

- Read source files in project (via `code_modify` read, requires confirmation)
- Analyze project directory structure (via `project_analyze`)
- Execute unit test suite (via `run_tests`, requires confirmation)
- Scrape public websites (via `web_scrape`)
- Read Gmail labels and messages (via Gmail tools, requires confirmation)
- Formulate and execute multi-step plans (via `ExecutiveController`)
- Store structured facts (via `store_fact`)
- Self-correct reasoning (via `self_correct`)
- Trigger CMake builds (via `code_modify build`, requires confirmation)

### ⚠️ Limitations

1. **Code Modification:** Cannot actually apply diffs — `apply_diff` is a stub that returns an error
2. **Shell Execution Config:** `allow_shell_exec` flag is ignored by `run_tests` and `code_modify`
3. **Sandbox Scope:** RAG indexing is **strictly limited** to `agent_workspace/rag/` — no live codebase indexing

### 🔒 Security Posture

**✅ Strengths:**
- Tool confirmation system enforced
- ConstraintChecker active in both execution paths
- Strict sandbox boundaries for RAG indexing
- Path validation in `code_modify`

**⚠️ Risks:**
1. **Shell Execution Bypass:** `allow_shell_exec` flag is not checked by tools that use `popen`
2. **Code Modification Stub:** Agent will fail goals requiring actual code changes (infinite revision loop possible)
3. **Project Root Access:** `code_modify` can read any file in project root (though requires confirmation)

---

## 7. Verification Checklist

Run this audit verification:

```bash
# 1. Verify tool confirmation enforcement
grep -r "requires_confirmation" external/basic_agent/src/tools.cpp
# Should show: ToolRegistry checks confirmation before execution

# 2. Verify ConstraintChecker integration
grep -r "constraint_checker_.check_action" external/basic_agent/src/command_processor.cpp
# Should show: ConstraintChecker called in processToolCall

# 3. Verify sandbox boundaries
grep -r "REJECTED path outside sandbox" external/basic_agent/src/index_manager.cpp
# Should show: Hard-reject logic for paths outside agent_workspace/

# 4. Verify code_modify stub
grep -r "not fully implemented" external/basic_agent/src/code_modify_tool.cpp
# Should show: apply_diff stub message

# 5. Count registered tools
grep -r "registerTool" external/basic_agent/src/tools.cpp | wc -l
# Should show: 9 tools (7 in constructor + 2 in initialize)
```

---

## 8. Recommendations

### High Priority

1. **Implement `code_modify apply_diff`:** The stub should be replaced with actual unified diff application
2. **Enforce `allow_shell_exec` flag:** `RunTestsTool` and `CodeModifyTool` should check this flag before using `popen`

### Medium Priority

3. **Document sandbox boundary change:** Update docs to reflect that RAG indexing is now sandboxed (this is a security improvement)
4. **Add confirmation to `web_scrape`:** Consider requiring confirmation for network requests

### Low Priority

5. **Audit tool confirmation requirements:** Review which tools should require confirmation (currently: Gmail tools, `run_tests`, `code_modify`)

---

**Next Audit:** Run when major security changes are made or before release.
