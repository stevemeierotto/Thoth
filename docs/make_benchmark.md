# Plan: Production-Grade Benchmark Execution & Visualization (FINAL)

This plan defines the implementation of a robust, thread-safe, and observable benchmark execution system for Thoth. It addresses critical threading, process lifecycle, and concurrency issues identified during architectural review.

## Goal
Enable real-time execution and visualization of agent benchmarks (`run_grag_benchmark`, `run_cognate_benchmark`) directly from the Thoth UI, ensuring strict separation of concerns, thread-safe pipe handling, and protection against SQLite database contention.

---

## Phase 1: Robust Path Resolution & Environment
**Objective**: Locate benchmark binaries and enforce a stable execution environment.

1.  **Binary Discovery Logic**:
    - Implement `GetBenchmarkBinaryPath(const wxString& binaryName)` in `AgentInterface`.
    - Search sequence:
        1.  `wxStandardPaths::Get().GetExecutablePath()` directory.
        2.  Sibling `build/` directories (accounting for `Debug`/`Release` subfolders).
        3.  Fallback to checking `PATH`.
    - Verification: Perform `wxFileName::FileExists` and `wxFileName::IsFileExecutable` checks before attempting launch.
2.  **Working Directory & Environment Enforcement**:
    - **MANDATORY**: Use `wxExecuteEnv` to explicitly set the `cwd` to the project root.
    - Example:
      ```cpp
      wxExecuteEnv env;
      env.cwd = projectRoot; // Resolved via wxStandardPaths
      wxExecute(command, wxEXEC_ASYNC, process, &env);
      ```

---

## Phase 2: `BenchmarkWindow` Architecture & Lifecycle
**Objective**: Create a "Research Instrument" window that handles process lifecycle and dual-stream output.

1.  **UI Construction**:
    - **Metadata Header**: `[Type | Mode | PID]`, `Started: Timestamp`, `Command: String`.
    - **Terminal**: `wxTextCtrl` (Multi-line, Monospace, Read-only).
    - **Controls**: `btnTerminate` (SIGTERM -> Escalation), `btnClose` (Enabled on completion), "Open Log".
2.  **Thread-Safe Dual-Stream Pipe Reading**:
    - **Mechanism**: Use a `wxTimer` (100ms) to poll `IsInputAvailable()` and `IsErrorAvailable()`.
    - **Safe Buffer Capture (CRITICAL)**: Always copy data into a `wxString` before calling `CallAfter` to prevent buffer corruption on slow hardware.
3.  **Line-Buffered Regex Parsing**:
    - **MANDATORY**: Accumulate incoming chunks into a `std::string` or `wxString` line buffer.
    - Only trigger regex parsing (`nDCG@5:\s*([0-9.]+)`) when a `\n` is encountered to avoid partial-chunk failures.
4.  **Non-Blocking Termination Escalation**:
    - **MANDATORY**: The 3-second timeout between `SIGTERM` and `SIGKILL` MUST use a `wxTimer`.
    - Do NOT use `wxSleep` or `std::this_thread::sleep_for`, as this will freeze the UI thread.
5.  **Final Pipe Drain**: Explicitly drain both `GetInputStream()` and `GetErrorStream()` in the `wxEVT_END_PROCESS` handler to capture buffered "tail" data.

---

## Phase 3: `MainFrame` Integration & Concurrency Guards
**Objective**: Centralize benchmark selection and protect shared resources (SQLite).

1.  **SQLite Contention Guard**:
    - **MANDATORY**: Only allow ONE benchmark to run at a time via a `m_isBenchmarkRunning` flag.
    - Block concurrent runs with a user-facing message to prevent SQLite deadlocks.
2.  **Application Shutdown Protection**:
    - **MANDATORY**: Implement a `MainFrame` close handler (`wxEVT_CLOSE_WINDOW`).
    - If a benchmark is running, the app must either:
        - Send `SIGTERM` to the benchmark and wait/block until it exits.
        - Prompt the user to terminate the benchmark before closing.
    - **Reason**: Prevents zombie processes from running with no attached UI.

---

## Phase 4: Observability & Result Extraction
**Objective**: Connect the raw output to the Thoth research data.

1.  **Metric Extraction**: Parse `nDCG@5` and `Success Rate` from the line buffer.
2.  **Post-Process Actions**: Provide a button to open `docs/benchmark_results.md` and update the `ExperimentLabPanel`.

---

## Phase 5: Verification & Hardening
**Objective**: Ensure system stability and correctness.

1.  **Recompile & Link**: `cmake --build --preset debug`.
2.  **Validation Steps**:
    - **Zombie Check**: Verify no processes remain after app close.
    - **Threading Stress**: Ensure smooth UI during heavy output bursts.
    - **CWD Check**: Confirm logs appear in the project root `logs/` directory.

---
**CRITICAL: Recompile after every step. Capture by value. Use line buffers for regex. Enforce singleton execution.**
