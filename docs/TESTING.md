# Testing Guide

**Last Updated:** 2026-06-17  
**Purpose:** Complete guide to Thoth's test structure and how to run tests

---

## Test Structure

### Test Organization

Thoth uses a single unified test file with individual test functions:

```
tests/
├── CMakeLists.txt        # Test build configuration
└── unit_tests.cpp        # All unit tests (single file)
```

**Test Pattern:**
- Each test is a `static bool testFunctionName()` function
- Returns `true` on success, `false` on failure
- Uses temporary directories/files for isolation
- Tests are registered in `main()` function

### Test Categories

#### 1. Configuration Tests
- `testConfigRoundTrip()` - Config save/load persistence
- Configuration value validation

#### 2. Memory System Tests
- `testMemoryPersistence()` - SQLite memory persistence
- `testMemoryRepository()` - Memory repository operations
- Conversation history and summary storage

#### 3. RAG/GRAG Tests
- `testBootstrapIndexing()` - Initial corpus indexing
- `testRetrievalDiagnosticsEvent()` - GRAG event emission
- Vector store operations
- Embedding generation

#### 4. Tool System Tests
- Tool registration and execution
- Tool confirmation system
- Tool input/output validation

#### 5. Planning & Execution Tests
- `testPlanParser()` - Plan JSON parsing
- `testResumeFromTrace()` - Crash recovery
- ExecutiveController state transitions
- Plan revision logic
- `testReflectionLoop()` - Reflection replan after low trajectory score (see below)

##### Reflection loop testing (`testReflectionLoop`)

The ExecutiveController reflection path runs when a plan finishes with **`calculate_trajectory_score() < 0.6`** (see `completed_improvements_log.md`, 2026-03-29). Up to **`MAX_REFLECTIONS` (2)** replans call `IPlanner::create_plan()` again with a goal suffix containing **`Reflection:`**.

**Important for test authors:** `WorkflowEngine::executeLLM()` is a stub that **always succeeds**, which yields a trajectory score of **1.0** and **does not** trigger reflection. To test reflection in unit tests you must arrange a first plan that completes with a **low score**, for example:

- A **`StepType::NODE`** step (not implemented — fails deterministically), or
- A **`StepType::TOOL`** step that resolves to a missing tool, or
- An empty plan (score 0.0 by definition)

`testReflectionLoop` uses the NODE pattern on the first plan and an LLM step on the reflection replan. It asserts:

1. `create_plan` is called at least twice
2. The second goal string contains `Reflection:`
3. At least two `PLAN_CREATED` events
4. The controller reaches `COMPLETED` on the second plan
5. The active plan id is `reflection-plan-2`

Because `execute_goal()` returns before the background `run_loop` finishes, assertions must **poll** until both `create_plan` call count and `PLAN_CREATED` events reach 2 (use `std::atomic` for event counts).

Do not use an all-LLM mock plan to test reflection unless you also mock failure at the workflow layer.

#### 6. Integration Tests
- `testAgentInterfaceLifecycle()` - Full agent initialization
- `testCommandProcessorSetCommand()` - Command processing
- End-to-end goal execution

#### 7. Security & Logging Tests
- `testStructuredLoggerRedaction()` - Sensitive data redaction
- Constraint checking
- Sandbox boundary enforcement

---

## Running Tests

### Prerequisites

1. **Build the project first:**
   ```bash
   cmake --preset debug
   cmake --build --preset build-debug
   ```

2. **Ensure Ollama is running** (some tests require it):
   ```bash
   ollama list  # Verify Ollama is accessible
   ```

3. **Workspace directory must be writable:**
   ```bash
   chmod -R u+w agent_workspace/
   ```

### Running All Tests

**Using CTest (Recommended):**
```bash
cd build/debug
ctest --output-on-failure
```

**Direct Execution:**
```bash
cd build/debug
./tests/thoth-unit-tests
```

**With Verbose Output:**
```bash
./tests/thoth-unit-tests --verbose  # If supported
```

### Running Specific Tests

The test suite doesn't currently support filtering individual tests. To test specific functionality:

1. **Modify `unit_tests.cpp`** temporarily to comment out other tests
2. **Use a debugger** to set breakpoints in specific test functions
3. **Run the full suite** and check output for specific test names

### Test Output

**Success Output:**
```
Running tests...
testConfigRoundTrip: OK
testMemoryPersistence: OK
...
All tests passed!
```

**Failure Output:**
```
testConfigRoundTrip: FAILED - value mismatch
testMemoryPersistence: OK
...
Some tests failed!
```

Each test function prints error messages to `stderr` on failure:
```cpp
if (!ok) std::cerr << "testName: failure reason\n";
```

---

## Test Implementation Details

### Test Isolation

Tests use temporary directories to avoid conflicts:

```cpp
static fs::path makeTempPath(const std::string& name) {
    const auto stamp = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    return fs::temp_directory_path() / (name + "_" + std::to_string(stamp));
}
```

**Cleanup:**
- Tests create temporary files/directories
- Cleanup happens in test function (or relies on OS cleanup)
- No shared state between tests

### Test Data

**Temporary Databases:**
- Each memory test creates its own `memory.db`
- Tests clean up after themselves
- No interference with production `agent_workspace/`

**Mock Services:**
- Some tests require Ollama (embedding tests)
- Tests that don't need LLM use mocks or skip LLM calls
- No external API calls in unit tests (except Ollama local)

### Assertion Pattern

Tests use simple boolean assertions:

```cpp
static bool testExample() {
    // Setup
    Config cfg;
    cfg.database_path = makeTempPath("test.db").string();
    
    // Execute
    Memory memory(cfg);
    memory.addMessage("user", "test");
    
    // Assert
    const bool ok = memory.getConversation().size() == 1;
    
    // Cleanup
    fs::remove(cfg.database_path);
    
    if (!ok) std::cerr << "testExample: assertion failed\n";
    return ok;
}
```

---

## Adding New Tests

### Test Function Template

```cpp
static bool testNewFeature() {
    // 1. Setup - Create temporary resources
    const fs::path tempPath = makeTempPath("test_resource");
    
    // 2. Execute - Test the feature
    // ... your test code ...
    
    // 3. Assert - Check results
    const bool ok = /* your assertion */;
    
    // 4. Cleanup - Remove temporary resources
    fs::remove(tempPath);
    
    // 5. Error reporting
    if (!ok) std::cerr << "testNewFeature: failure reason\n";
    return ok;
}
```

### Registering Tests

Add your test to `main()` function in `unit_tests.cpp`:

```cpp
int main() {
    int failures = 0;
    
    // ... existing tests ...
    
    if (!testNewFeature()) failures++;
    
    // ... rest of main ...
}
```

### Test Naming Convention

- Function name: `testFeatureName()` or `testComponentFeature()`
- Use descriptive names that indicate what's being tested
- Follow existing patterns (e.g., `testMemoryPersistence`, `testPlanParser`)

---

## Test Coverage

### Currently Tested Components

✅ **Core Systems:**
- Configuration management
- Memory persistence (SQLite)
- RAG indexing and retrieval
- Plan parsing and validation
- Tool registration

✅ **Integration Points:**
- AgentInterface lifecycle
- CommandProcessor commands
- ExecutiveController state machine
- GRAG event emission
- Reflection replan loop (`testReflectionLoop`)

✅ **Security:**
- Log redaction (API keys, emails)
- Constraint checking
- Sandbox boundaries

### Areas Needing More Tests

📋 **Planned Test Expansion:**
- ExecutiveController full lifecycle (beyond reflection replan)
- Plan revision scenarios (`revise_plan` mid-execution, distinct from reflection)
- Strategy engine pattern detection (partial coverage exists)
- Scientific execution mode (partial coverage exists)
- Graph memory operations
- Multi-threaded scenarios
- Automated equivalents of `TEST_SUITE.md` TC-01–TC-07

---

## Continuous Integration

### CI Test Execution

Tests run automatically in CI/CD pipelines:

```yaml
# Example CI step
- name: Run Tests
  run: |
    cmake --preset debug
    cmake --build --preset build-debug
    cd build/debug
    ctest --output-on-failure
```

### Test Requirements for CI

1. **No external dependencies** (except Ollama, which CI must provide)
2. **Deterministic** - Tests must produce same results every run
3. **Fast** - Complete test suite should run in < 5 minutes
4. **Isolated** - No shared state between test runs

---

## Debugging Tests

### Using GDB

```bash
gdb ./build/debug/tests/thoth-unit-tests
(gdb) break testFunctionName
(gdb) run
(gdb) # Debug as normal
```

### Using IDE Debuggers

**VS Code:**
1. Set breakpoint in `unit_tests.cpp`
2. Create `.vscode/launch.json`:
   ```json
   {
     "type": "cppdbg",
     "request": "launch",
     "program": "${workspaceFolder}/build/debug/tests/thoth-unit-tests",
     "args": [],
     "stopAtEntry": false
   }
   ```

**CLion:**
- Right-click test function → "Debug 'testFunctionName'"
- Or run entire test suite with debugger attached

### Common Test Issues

**Test fails intermittently:**
- Check for race conditions (use ThreadSanitizer)
- Verify temporary file cleanup
- Check for shared state

**Test passes locally but fails in CI:**
- Check for hardcoded paths (use relative paths)
- Verify environment variables
- Check Ollama availability in CI

**Test hangs:**
- Check for infinite loops
- Verify network timeouts
- Check for deadlocks (use ThreadSanitizer)

---

## Performance Testing

### Benchmark Tests

GRAG retrieval benchmarks are separate from unit tests:

```bash
# Run GRAG benchmarks
./build/debug/external/basic_agent/run_grag_benchmark
```

**Benchmark Output:**
- Results written to `logs/grag_benchmark.jsonl`
- Summary auto-archived to `docs/benchmark_results.md`

### Profiling Tests

**Using Valgrind:**
```bash
valgrind --leak-check=full ./build/debug/tests/thoth-unit-tests
```

**Using perf (Linux):**
```bash
perf record ./build/debug/tests/thoth-unit-tests
perf report
```

---

## Manual Pipeline Tests (GUI + logs)

Repeatable goal/chat pipeline checks with expected log signals: **[TEST_SUITE.md](TEST_SUITE.md)** (TC-01–TC-07). Use with [VERIFIED_BASELINE.md](VERIFIED_BASELINE.md) for regression contract.

**GRAG diagnostics show zero?** See [ui_improvements.md](ui_improvements.md) §10.

---

## Manual Integration Tests (Concurrency)

Post–thread-safety hardening checks for `ExecutiveController` mutex integration. Run with `decision_trace.jsonl` and `grag_benchmark.jsonl` open.

### Test 1 — Basic Goal Activation

1. Start the application.
2. Run: `/goal improve recommendation system relevance`
3. While the plan is active, send: `What techniques exist for collaborative filtering?`

**Verify in logs:**

- `grag_benchmark.jsonl`: `goal_present: true`, `scoring_type: grag`
- `decision_trace.jsonl`: `PLAN_CREATED`, `STEP_STARTED`, `STEP_COMPLETED` (if applicable), plan still active or `PLAN_COMPLETED`

Confirms embedding lifecycle survived mutex integration.

### Test 2 — Concurrent Interrupt

Stress thread restart integrity:

1. Issue a long-running goal (or simulate planner delay).
2. Immediately issue: `/goal build fraud detection pipeline`

**Expected:**

- First controller thread stops cleanly; embeddings cleared; new `PLAN_CREATED`; no crash; no corrupted SQLite plan; no mixed GRAG state.
3. Send a normal query and confirm GRAG scoring reflects the **new** goal.

### Test 3 — Pause / Resume Visibility

1. Start a multi-step goal.
2. Call pause (Agent menu or equivalent).
3. Confirm loop halts and state-change events appear in the trace.
4. Call resume; confirm execution continues without duplicated steps.

### What these verify

- No deadlock under rapid goal switching
- No lost embedding state
- No partial plan mutation races
- No stale callback invocation
- No double `PLAN_COMPLETED` events
- Consistent `current_index` in logs

If all three pass, the controller is structurally stable for concurrent GUI use.

---

### ✅ Do

- Use temporary directories for all test data
- Clean up after tests (or document why not)
- Test both success and failure paths
- Test edge cases (empty inputs, null pointers, etc.)
- Use descriptive test names
- Print clear error messages on failure

### ❌ Don't

- Don't modify production `agent_workspace/` in tests
- Don't hardcode absolute paths
- Don't rely on external services (except Ollama)
- Don't create tests that depend on execution order
- Don't skip cleanup (memory leaks, file pollution)

---

## Quick Reference

### Run Tests
```bash
cd build/debug
ctest --output-on-failure
```

### Add Test
1. Add `static bool testName()` function
2. Register in `main()`: `if (!testName()) failures++;`

### Debug Test
```bash
gdb ./build/debug/tests/thoth-unit-tests
(gdb) break testName
(gdb) run
```

### Check Coverage
```bash
# Install gcov/lcov first
cmake --preset debug -DCMAKE_BUILD_TYPE=Coverage
cmake --build --preset build-debug
./build/debug/tests/thoth-unit-tests
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage/
```

---

**See Also:**
- [GETTING_STARTED.md](GETTING_STARTED.md) - Build setup
- [AGENTS.md](../AGENTS.md) - Architecture guide
- [TEST_SUITE.md](TEST_SUITE.md) - Manual pipeline tests TC-01–TC-07
- [ui_improvements.md](ui_improvements.md) - Research console UI status and troubleshooting
- [HOWTO.md](HOWTO.md) - Using `/goal` and goal mode vs chat
