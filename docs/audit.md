 Thoth Codebase Audit: Current State (2026-03-10)

  1. Sandbox & File Access Boundaries


   * code_modify Write Paths:
       * Allowlist Logic: The CodeModifyTool::isPathSafe method enforces that
         all file operations (read, apply_diff, revert) must be within the
         project root. It rejects absolute paths and path traversal (..).
       * Project Root: Determined by FileHandler::getProjectRoot(), which
         defaults to /home/steve/Thoth (the directory containing GEMINI.md).
       * Reality: code_modify can theoretically write to any file in the live
         codebase, though the apply_diff operation is currently a stub that
         returns an error.


   * RAG Bootstrap Indexer Scan Paths:
       * Root Path Logic: In CommandProcessor::ensureInitialized(), if the index
         is empty, it calls indexProject(fh.getProjectRoot()).
       * Internal Exclusions: IndexManager::indexProject recursively scans the
         root but explicitly skips: rag_index.bin, /build/, /.git/, and /docs/.
       * Reality: The RAG indexer scans the entire live codebase except for the
         specified exclusion folders.


   * Boundary Comparison:
       * Overlap: Both boundaries are anchored to the same project root
         (/home/steve/Thoth).
       * Difference: code_modify is conceptually permitted to access anything in
         the root (including docs), while the RAG indexer explicitly ignores
         docs, build, and .git.
       * Live Codebase Access: The agent has full read access to the live
         codebase via the RAG index and code_modify read. It has "write-intent"
         access via code_modify, although the implementation is currently
         blocked by a stub.

  2. Registered Tools

  The ToolRegistry contains the following tools:



  ┌──────────────┬─────────────┬──────────────────────────┬────────────────┐
  │ Tool Name    │ Purpose     │ Access/Permissions       │ requires_confi │
  │              │             │                          │ rmation        │
  ├──────────────┼─────────────┼──────────────────────────┼────────────────┤
  │ summarize_te │ Summarizes  │ Internal (LLM).          │ No             │
  │ xt           │ input text  │                          │                │
  │              │ via LLM.    │                          │                │
  │ gmail_read_l │ Lists Gmail │ Network (Google API).    │ No             │
  │ abels        │ labels.     │                          │                │
  │ project_anal │ Lists files │ Filesystem (Read-only,   │ No             │
  │ yze          │ and         │ Project Root).           │                │
  │              │ directories │                          │                │
  │              │ .           │                          │                │
  │ run_tests    │ Executes    │ Shell (Executes          │ No             │
  │              │ unit tests. │ /build/debug/tests/thoth │                │
  │              │             │ -unit-tests).            │                │
  │ code_modify  │ Read/Modify │ Filesystem (Read/Write,  │ No             │
  │              │ /Build      │ Project Root), Shell     │                │
  │              │ codebase.   │ (Build).                 │                │
  │ web_scrape   │ Scrapes     │ Network (libcurl).       │ No             │
  │              │ text from a │                          │                │
  │              │ URL.        │                          │                │
  │ gmail_read_m │ Reads Gmail │ Network (Google API).    │ No             │
  │ essages      │ messages.   │                          │                │
  │ store_fact   │ Stores      │ SQLite (memory.db).      │ No             │
  │              │ facts in    │                          │                │
  │              │ FactStore.  │                          │                │
  │ self_correct │ Verifies    │ Internal (LLM).          │ No             │
  │              │ results     │                          │                │
  │              │ against     │                          │                │
  │              │ expectation │                          │                │
  │              │ s.          │                          │                │
  └──────────────┴─────────────┴──────────────────────────┴────────────────┘
   * Config Defaults: Tools are enabled by default (enable_tools: true).
     allow_network, allow_web, and allow_file_io are all true by default.
     allow_shell_exec is false by default, but several tools (run_tests,
     code_modify build) use popen directly regardless of this flag.


  3. agent_workspace Contents


   * Files/Directories:
       * app_log.jsonl: General application logs.
       * chat_sessions.json: Persistent chat history.
       * decision_trace.jsonl: Detailed event logs (PLAN_CREATED, STEP_STARTED,
         etc.).
       * grag_benchmark.jsonl: Raw benchmark execution data.
       * grag_metrics.jsonl: High-level success/failure metrics for goals.
       * memory.db: SQLite database for episodic memory, past plans, and
         trajectories.
       * prompt_templates/: System and tool prompts.
       * rag/rag_index.bin: The binary RAG index.
       * retrieval_config.json: Optimized GRAG weights ($w_q, w_d, w_k$).


   * RAG Index Status:
       * The index points to /home/steve/Thoth.
       * It is automatically bootstrapped on the first run if empty.
       * It typically contains 200-500 chunks for a project of this size
         (filtered by supported extensions and exclusions).

  4. Docs vs Reality Gaps


   * code_modify Stub: completed_improvements_log.md claims "Implemented
     Self-Building Capability... giving the agent tools to... modify its own
     codebase." This is false. The code reveals applyDiff is a stub that
     explicitly returns an error: "Unified diff application not fully
     implemented in v1.0 prototype."
   * ConstraintChecker Bypass: completed_improvements_log.md claims a "global
     ConstraintChecker for security policy." While it exists, it is only invoked
     by ExecutiveController. Direct tool calls from
     CommandProcessor::processToolCall (standard chat loop) bypass all
     constraints.
   * requires_confirmation: Mentioned in some architectural docs, but the ITool
     interface and ToolRegistry have no implementation for this. Tools execute
     immediately upon request.
   * allow_shell_exec Flag: The Config has an allow_shell_exec flag set to
     false, but the codebase ignores it in RunTestsTool and CodeModifyTool.


  5. What the Agent Can Do Autonomously Right Now


   * Capabilities:
       * Read every source file in the project.
       * Analyze the project directory structure.
       * Execute the existing test suite and see failures.
       * Scrape any public website.
       * Read the user's Gmail.
       * Formulate multi-step plans and execute them (Planning -> Tool Use ->
         Reflection).
       * Trigger a cmake build of the project.


   * Surprises/Risks:
       1. Code Modification is Broken: The agent will fail any goal that
          requires it to actually write code changes, leading to an infinite
          "Refining Plan" loop or FAILED status.
       2. Constraint Bypass: In a standard chat session, the agent can be
          tricked into reading restricted paths (like /etc/passwd) or hitting
          restricted domains, as ConstraintChecker is only active during "Goal
          Execution" (ExecutiveController).
       3. No Confirmation: The agent can read your emails or scrape URLs without
          any "Are you sure?" prompt.
       4. Live Execution: The run_tests tool executes a binary on your system;
          while hardcoded to the thoth binary, any vulnerability in that binary
          could be exploited by an agent-generated input.

