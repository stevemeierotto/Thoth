/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — Phase 10C-1 C6 cognitive metrics recovery
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "config.h"
#include "executive_controller.h"
#include "itool.h"
#include "llm_interface.h"
#include "memory.h"
#include "plan.h"
#include "sqlite_memory_repository.h"
#include "tools.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

namespace {

std::atomic<bool> g_release_gate{false};

class ProbeTool : public ITool {
public:
    explicit ProbeTool(std::string name, bool gate) : name_(std::move(name)), gate_(gate) {}
    std::string name() const override { return name_; }
    std::string description() const override { return "c6 metrics probe"; }
    nlohmann::json input_schema() const override { return nlohmann::json::object(); }
    bool requires_confirmation() const override { return false; }
    nlohmann::json execute(const nlohmann::json&) const override {
        if (gate_) {
            while (!g_release_gate.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return {{"status", "success"}, {"data", {{"probe", true}}}};
    }
private:
    std::string name_;
    bool gate_;
};

class ScriptPlanner : public IPlanner {
public:
    Plan plan;
    Plan create_plan(const std::string&) override { return plan; }
    Plan revise_plan(const Plan& existing, const nlohmann::json&) override { return existing; }
};

struct EnvGuard {
    std::string workspace;
    std::string metrics;
    const char* prev_workspace;
    const char* prev_logs;
    const char* prev_metrics;
    const char* prev_window;
    const char* prev_fp;
    const char* prev_dev;
    explicit EnvGuard(const std::string& name)
        : workspace("/tmp/thoth-c6-metrics-" + name + "/workspace"),
          metrics("/tmp/thoth-c6-metrics-" + name + "/cognitive_metrics.jsonl"),
          prev_workspace(std::getenv("THOTH_WORKSPACE_PATH")),
          prev_logs(std::getenv("THOTH_LOGS_PATH")),
          prev_metrics(std::getenv("THOTH_COGNITIVE_METRICS_LOG")),
          prev_window(std::getenv("THOTH_C64_WINDOW_FILE")),
          prev_fp(std::getenv("THOTH_C64_CURRENT_FINGERPRINT")),
          prev_dev(std::getenv("THOTH_TEST_SUITE_DEV")) {
        std::filesystem::remove_all("/tmp/thoth-c6-metrics-" + name);
        std::filesystem::create_directories(workspace);
        setenv("THOTH_WORKSPACE_PATH", workspace.c_str(), 1);
        setenv("THOTH_LOGS_PATH", ("/tmp/thoth-c6-metrics-" + name + "/logs").c_str(), 1);
        setenv("THOTH_COGNITIVE_METRICS_LOG", metrics.c_str(), 1);
        unsetenv("THOTH_MOCK_LLM");
    }
    ~EnvGuard() {
        restore("THOTH_WORKSPACE_PATH", prev_workspace);
        restore("THOTH_LOGS_PATH", prev_logs);
        restore("THOTH_COGNITIVE_METRICS_LOG", prev_metrics);
        restore("THOTH_C64_WINDOW_FILE", prev_window);
        restore("THOTH_C64_CURRENT_FINGERPRINT", prev_fp);
        restore("THOTH_TEST_SUITE_DEV", prev_dev);
    }
    static void restore(const char* key, const char* value) {
        if (value) setenv(key, value, 1);
        else unsetenv(key);
    }
};

void writeWindow(const std::string& path, const std::string& window_id) {
    const std::string fp(64, 'a');
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    const long long start = now - 60000;
    const long long end = start + 28LL * 86400000LL;
    std::ofstream out(path);
    out << "{\"status\":\"open\",\"window_id\":\"" << window_id
        << "\",\"protocol_version\":\"C6.4 v1.0\",\"metric_schema_version\":\"1.0\","
           "\"environment_schema_version\":\"c64-env-1\",\"evaluation_tier\":\"authoritative\","
           "\"window_start_ms\":" << start << ",\"window_end_ms\":" << end
        << ",\"c64_cohort_fingerprint\":\"" << fp << "\"}";
    setenv("THOTH_C64_WINDOW_FILE", path.c_str(), 1);
    setenv("THOTH_C64_CURRENT_FINGERPRINT", fp.c_str(), 1);
}

PlanStep toolStep(const std::string& id, const std::string& tool, StepStatus status) {
    PlanStep step;
    step.step_id = id;
    step.description = id;
    step.type = StepType::TOOL;
    step.payload = {{"tool", tool}};
    step.status = status;
    step.failure_policy.max_retries = 0;
    return step;
}

class Harness {
public:
    Config config;
    std::shared_ptr<Memory> memory;
    std::shared_ptr<ToolRegistry> tools;
    std::shared_ptr<ScriptPlanner> planner;
    LLMInterface llm;
    Thoth::ExecutiveController controller;
    Harness(const std::string& session)
        : memory(std::make_shared<Memory>(config)),
          tools(std::make_shared<ToolRegistry>()),
          planner(std::make_shared<ScriptPlanner>()),
          llm(LLMBackend::Ollama, &config),
          controller(planner, tools, nullptr, memory) {
        tools->registerTool(std::make_unique<ProbeTool>("c6_ok", false));
        tools->registerTool(std::make_unique<ProbeTool>("c6_gate", true));
        controller.set_config(&config);
        controller.set_llm_interface(&llm);
        controller.set_session_id(session);
        controller.set_max_reflections(0);
        memory->setActiveSessionId(session);
    }
};

bool waitState(Thoth::ExecutiveController& controller, Thoth::ControllerState wanted, int ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (controller.get_state() == wanted) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

std::vector<nlohmann::json> readMetrics(const std::string& path) {
    std::vector<nlohmann::json> rows;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) rows.push_back(nlohmann::json::parse(line));
    }
    return rows;
}

bool exactKeysAbsent(const nlohmann::json& row) {
    for (const char* key : {"total_tokens", "prompt_tokens", "completion_tokens", "planning_tokens",
                            "synthesis_tokens", "planning_time_ms", "retrieval_time_ms",
                            "llm_synthesis_time_ms", "total_wall_clock_ms"}) {
        if (row.contains(key)) return false;
    }
    return true;
}

GoalMetricsCheckpoint sampleMetrics() {
    GoalMetricsCheckpoint metrics;
    metrics.present = true;
    metrics.window_assigned = true;
    metrics.window_id = "original-window";
    metrics.protocol_version = "C6.4 v1.0";
    metrics.metric_schema_version = "1.0";
    metrics.environment_schema_version = "c64-env-1";
    metrics.cohort_fingerprint = std::string(64, 'a');
    metrics.goal_started_at_ms = 1700000000000LL;
    metrics.total_tokens = 80;
    metrics.prompt_tokens = 50;
    metrics.completion_tokens = 30;
    metrics.planning_tokens = 10;
    metrics.planning_time_ms = 25;
    return metrics;
}

bool testExactCompletionAndDurableCheckpoint() {
    EnvGuard env("exact");
    writeWindow("/tmp/thoth-c6-metrics-exact/window.json", "fixture-window");
    setenv("THOTH_TEST_SUITE_DEV", "1", 1);
    g_release_gate = false;
    Harness harness("c6-exact");
    Plan plan;
    plan.plan_id = "exact-plan";
    plan.goal = "exact metrics";
    plan.plan_origin = "model";
    PlanStep synthesis;
    synthesis.step_id = "exact-1";
    synthesis.description = "phase10c synthesis";
    synthesis.type = StepType::LLM;
    synthesis.payload = {{"prompt", "Say phase10c."}};
    synthesis.failure_policy.max_retries = 0;
    plan.steps = {synthesis, toolStep("exact-2", "c6_gate", StepStatus::PENDING)};
    plan.steps[1].depends_on = {"exact-1"};
    harness.planner->plan = plan;
    harness.controller.execute_goal(plan.goal);

    bool saw_checkpoint = false;
    for (int i = 0; i < 200 && !saw_checkpoint; ++i) {
        const Plan current = harness.controller.get_current_plan();
        if (current.goal_metrics.present && current.goal_metrics.window_id == "fixture-window" &&
            current.steps.size() == 2 && current.steps[0].status == StepStatus::SUCCESS &&
            current.steps[1].status != StepStatus::SUCCESS) {
            saw_checkpoint = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    g_release_gate = true;
    if (!saw_checkpoint || !waitState(harness.controller, Thoth::ControllerState::COMPLETED, 5000)) {
        std::cerr << "exact: checkpoint or completion missing\n";
        return false;
    }
    const auto rows = readMetrics(env.metrics);
    if (rows.size() != 1 || rows[0].value("metrics_partial", true) ||
        rows[0].value("window_id", "") != "fixture-window" ||
        !rows[0].contains("total_tokens") ||
        rows[0].value("total_tokens", static_cast<std::int64_t>(0)) <= 0 ||
        rows[0].contains("available_counters")) {
        std::cerr << "exact: " << (rows.empty() ? "no row" : rows[0].dump()) << "\n";
        return false;
    }
    return true;
}

bool testRestartPreservesWindowAndMarksGap() {
    EnvGuard env("restart");
    writeWindow("/tmp/thoth-c6-metrics-restart/window.json", "other-window");
    Harness harness("c6-restart");
    Plan plan;
    plan.plan_id = "restart-plan";
    plan.goal = "restart metrics";
    plan.plan_origin = "model";
    plan.status = PlanStatus::ACTIVE;
    plan.goal_metrics = sampleMetrics();
    plan.steps = {toolStep("restart-done", "c6_ok", StepStatus::SUCCESS),
                  toolStep("restart-next", "c6_ok", StepStatus::PENDING)};
    plan.steps[1].depends_on = {"restart-done"};
    harness.controller.resume_from_plan(plan);
    if (!waitState(harness.controller, Thoth::ControllerState::COMPLETED, 5000)) {
        std::cerr << "restart: did not complete\n";
        return false;
    }
    const auto rows = readMetrics(env.metrics);
    if (rows.size() != 1) {
        std::cerr << "restart: row count " << rows.size() << "\n";
        return false;
    }
    const auto& row = rows[0];
    if (!row.value("metrics_partial", false) ||
        row.value("metrics_partial_reason", "") != "checkpoint_gap" ||
        row.value("window_id", "") != "original-window" ||
        row.value("goal_started_at_ms", static_cast<std::int64_t>(0)) != 1700000000000LL ||
        !exactKeysAbsent(row) || !row.contains("available_counters") ||
        row["available_counters"].value("counters_are_exact_totals", true) ||
        row["available_counters"].value("total_tokens", static_cast<std::int64_t>(-1)) != 80) {
        std::cerr << "restart: " << row.dump() << "\n";
        return false;
    }
    return true;
}

bool testLegacyOmitsCounters() {
    EnvGuard env("legacy");
    writeWindow("/tmp/thoth-c6-metrics-legacy/window.json", "should-not-attach");
    Harness harness("c6-legacy");
    Plan plan;
    plan.plan_id = "legacy-plan";
    plan.goal = "legacy metrics";
    plan.plan_origin = "fallback";
    plan.status = PlanStatus::ACTIVE;
    plan.steps = {toolStep("legacy-done", "c6_ok", StepStatus::SUCCESS),
                  toolStep("legacy-next", "c6_ok", StepStatus::PENDING)};
    plan.steps[1].depends_on = {"legacy-done"};
    harness.controller.resume_from_plan(plan);
    if (!waitState(harness.controller, Thoth::ControllerState::COMPLETED, 5000)) {
        std::cerr << "legacy: did not complete\n";
        return false;
    }
    const auto rows = readMetrics(env.metrics);
    if (rows.size() != 1) return false;
    const auto& row = rows[0];
    if (!row.value("metrics_partial", false) ||
        row.value("metrics_partial_reason", "") != "not_checkpointed" ||
        row.contains("window_id") || row.contains("goal_started_at_ms") ||
        row.contains("available_counters") || !exactKeysAbsent(row) ||
        row.value("learning_eligible", true)) {
        std::cerr << "legacy: " << row.dump() << "\n";
        return false;
    }
    return true;
}

bool testFailClosedEmitsOnePartialFailure() {
    EnvGuard env("failclosed");
    writeWindow("/tmp/thoth-c6-metrics-failclosed/window.json", "other-window");
    Harness harness("c6-failclosed");
    Plan plan;
    plan.plan_id = "failclosed-plan";
    plan.goal = "fail closed metrics";
    plan.plan_origin = "model";
    plan.status = PlanStatus::ACTIVE;
    plan.goal_metrics = sampleMetrics();
    PlanStep running = toolStep("failclosed-run", "c6_ok", StepStatus::RUNNING);
    running.started_at_ms = 5;
    plan.steps = {running};
    harness.controller.resume_from_plan(plan);
    if (!waitState(harness.controller, Thoth::ControllerState::FAILED, 5000)) {
        std::cerr << "failclosed: state " << static_cast<int>(harness.controller.get_state()) << "\n";
        return false;
    }
    const auto rows = readMetrics(env.metrics);
    if (rows.size() != 1) {
        std::cerr << "failclosed: rows " << rows.size() << "\n";
        return false;
    }
    const auto& row = rows[0];
    if (row.value("outcome", "") != "failed" ||
        row.value("reflection_skip_reason", "") != "interrupted_step_not_replayed" ||
        row.value("metrics_partial_reason", "") != "checkpoint_gap" ||
        row.value("window_id", "") != "original-window" ||
        !exactKeysAbsent(row) ||
        row["available_counters"].value("total_tokens", static_cast<std::int64_t>(-1)) != 80) {
        std::cerr << "failclosed: " << row.dump() << "\n";
        return false;
    }
    if (!harness.memory->getAllTrajectories().empty() ||
        !harness.memory->getAllPastPlans().empty() ||
        !harness.memory->getAllStrategies().empty() ||
        !harness.memory->getSQLiteRepo()->getAllEdges().empty()) {
        std::cerr << "failclosed: learning or graph rows were written\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    int failed = 0;
    if (!testExactCompletionAndDurableCheckpoint()) ++failed;
    if (!testRestartPreservesWindowAndMarksGap()) ++failed;
    if (!testLegacyOmitsCounters()) ++failed;
    if (!testFailClosedEmitsOnePartialFailure()) ++failed;
    if (failed == 0) {
        std::cout << "c6 metrics recovery tests passed\n";
        return 0;
    }
    std::cerr << failed << " c6 metrics recovery test(s) failed\n";
    return 1;
}
