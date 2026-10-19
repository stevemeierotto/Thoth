/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — Phase 10F-2 durable cognitive metrics outbox
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "cognitive_metrics.h"
#include "config.h"
#include "executive_controller.h"
#include "itool.h"
#include "memory.h"
#include "plan.h"
#include "tools.h"

#include <sqlite3.h>

#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

class ProbeTool : public ITool {
public:
    explicit ProbeTool(std::string name, std::string status)
        : name_(std::move(name)), status_(std::move(status)) {}
    std::string name() const override { return name_; }
    std::string description() const override { return "metrics outbox probe"; }
    nlohmann::json input_schema() const override { return nlohmann::json::object(); }
    bool requires_confirmation() const override { return false; }
    nlohmann::json execute(const nlohmann::json&) const override {
        if (status_ == "block") {
            while (!released()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return {{"status", "success"}};
        }
        if (status_ == "error") {
            return {{"status", "error"}, {"error_message", "probe failure"}};
        }
        return {{"status", "success"}};
    }
    static std::atomic<bool>& released() {
        static std::atomic<bool> gate{false};
        return gate;
    }
private:
    std::string name_;
    std::string status_;
};

class ScriptPlanner : public IPlanner {
public:
    Plan plan;
    Plan create_plan(const std::string&) override { return plan; }
    Plan revise_plan(const Plan& existing, const nlohmann::json&) override { return existing; }
};

class QueuePlanner : public IPlanner {
public:
    std::vector<Plan> plans;
    size_t index = 0;
    Plan create_plan(const std::string&) override {
        if (plans.empty()) return Plan();
        if (index >= plans.size()) return plans.back();
        return plans[index++];
    }
    Plan revise_plan(const Plan& existing, const nlohmann::json&) override { return existing; }
};

struct EnvGuard {
    std::string root;
    std::string workspace;
    std::string metrics;
    const char* prev_workspace;
    const char* prev_logs;
    const char* prev_metrics;
    const char* prev_flush;
    const char* prev_aging;
    explicit EnvGuard(const std::string& name)
        : root("/tmp/thoth-metrics-outbox-" + name),
          workspace(root + "/workspace"),
          metrics(root + "/cognitive_metrics.jsonl"),
          prev_workspace(std::getenv("THOTH_WORKSPACE_PATH")),
          prev_logs(std::getenv("THOTH_LOGS_PATH")),
          prev_metrics(std::getenv("THOTH_COGNITIVE_METRICS_LOG")),
          prev_flush(std::getenv("THOTH_INJECT_METRICS_FLUSH")),
          prev_aging(std::getenv("THOTH_INJECT_AGING_COMMIT_FAIL")) {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(workspace);
        setenv("THOTH_WORKSPACE_PATH", workspace.c_str(), 1);
        setenv("THOTH_LOGS_PATH", (root + "/logs").c_str(), 1);
        setenv("THOTH_COGNITIVE_METRICS_LOG", metrics.c_str(), 1);
        unsetenv("THOTH_INJECT_METRICS_FLUSH");
        unsetenv("THOTH_INJECT_AGING_COMMIT_FAIL");
        unsetenv("THOTH_C64_WINDOW_FILE");
        unsetenv("THOTH_C64_CURRENT_FINGERPRINT");
        unsetenv("THOTH_TEST_SUITE_DEV");
        unsetenv("THOTH_MOCK_LLM");
    }
    ~EnvGuard() {
        restore("THOTH_WORKSPACE_PATH", prev_workspace);
        restore("THOTH_LOGS_PATH", prev_logs);
        restore("THOTH_COGNITIVE_METRICS_LOG", prev_metrics);
        restore("THOTH_INJECT_METRICS_FLUSH", prev_flush);
        restore("THOTH_INJECT_AGING_COMMIT_FAIL", prev_aging);
    }
    static void restore(const char* key, const char* value) {
        if (value) setenv(key, value, 1);
        else unsetenv(key);
    }
    std::string db() const { return workspace + "/memory.db"; }
};

struct Harness {
    Config config;
    std::shared_ptr<Memory> memory;
    std::shared_ptr<ToolRegistry> tools;
    std::shared_ptr<IPlanner> planner;
    Thoth::ExecutiveController controller;
    Harness(const std::string& session, std::shared_ptr<IPlanner> planner_in)
        : memory(std::make_shared<Memory>(config)),
          tools(std::make_shared<ToolRegistry>()),
          planner(std::move(planner_in)),
          controller(planner, tools, nullptr, memory) {
        tools->registerTool(std::make_unique<ProbeTool>("outbox_ok", "success"));
        tools->registerTool(std::make_unique<ProbeTool>("outbox_fail", "error"));
        tools->registerTool(std::make_unique<ProbeTool>("outbox_block", "block"));
        controller.set_config(&config);
        controller.set_session_id(session);
        controller.set_max_reflections(0);
        memory->setActiveSessionId(session);
    }
};

PlanStep toolStep(const std::string& id, const std::string& tool) {
    PlanStep step;
    step.step_id = id;
    step.description = id;
    step.type = StepType::TOOL;
    step.payload = {{"tool", tool}};
    step.status = StepStatus::PENDING;
    step.failure_policy.max_retries = 0;
    return step;
}

bool waitState(Thoth::ExecutiveController& controller, Thoth::ControllerState wanted, int ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (controller.get_state() == wanted) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

std::vector<nlohmann::json> validLines(const std::string& path) {
    std::vector<nlohmann::json> rows;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            auto parsed = nlohmann::json::parse(line);
            if (parsed.is_object()) rows.push_back(std::move(parsed));
        } catch (...) {}
    }
    return rows;
}

int countExactId(const std::vector<nlohmann::json>& rows, const std::string& id) {
    int count = 0;
    for (const auto& row : rows) {
        if (row.contains("metrics_id") && row["metrics_id"].is_string()
            && row["metrics_id"].get<std::string>() == id) {
            ++count;
        }
    }
    return count;
}

int scalar(const std::string& db, const std::string& sql) {
    sqlite3* handle = nullptr;
    if (sqlite3_open_v2(db.c_str(), &handle, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (handle) sqlite3_close(handle);
        return -1;
    }
    sqlite3_stmt* stmt = nullptr;
    int value = -1;
    if (sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW) {
        value = sqlite3_column_int(stmt, 0);
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_close(handle);
    return value;
}

std::string textColumn(const std::string& db, const std::string& sql) {
    sqlite3* handle = nullptr;
    if (sqlite3_open_v2(db.c_str(), &handle, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (handle) sqlite3_close(handle);
        return {};
    }
    sqlite3_stmt* stmt = nullptr;
    std::string value;
    if (sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_text(stmt, 0)) {
        value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_close(handle);
    return value;
}

bool learningTablesEmpty(const std::string& db) {
    return scalar(db, "SELECT COUNT(*) FROM graph_edges") == 0
        && scalar(db, "SELECT COUNT(*) FROM trajectories") == 0
        && scalar(db, "SELECT COUNT(*) FROM past_plans") == 0
        && scalar(db, "SELECT COUNT(*) FROM strategies") == 0;
}

bool testAgingRollbackLeavesNoOutbox() {
    EnvGuard env("aging-rollback");
    setenv("THOTH_INJECT_AGING_COMMIT_FAIL", "1", 1);
    auto planner = std::make_shared<ScriptPlanner>();
    Plan plan;
    plan.plan_id = "aging-plan";
    plan.goal = "aging rollback";
    plan.steps = {toolStep("aging-step", "outbox_ok")};
    planner->plan = plan;
    Harness harness("aging", planner);
    harness.controller.execute_goal(plan.goal);
    if (!waitState(harness.controller, Thoth::ControllerState::EXECUTING_STEP, 3000)
        && harness.controller.get_state() != Thoth::ControllerState::COMPLETED
        && harness.controller.get_state() != Thoth::ControllerState::FAILED
        && harness.controller.get_state() != Thoth::ControllerState::IDLE) {
        std::cerr << "aging rollback: goal did not run\n";
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (scalar(env.db(), "SELECT COUNT(*) FROM cognitive_metrics_outbox") != 0) {
        std::cerr << "aging rollback: outbox row survived the rolled-back aging transaction\n";
        return false;
    }
    if (countExactId(validLines(env.metrics), "aging-plan") != 0) {
        std::cerr << "aging rollback: metrics line written without a committed outbox row\n";
        return false;
    }
    return true;
}

bool testFlushInterruptionsAndRestart() {
    EnvGuard env("flush");
    Plan finished;
    {
        setenv("THOTH_INJECT_METRICS_FLUSH", "before_append", 1);
        auto planner = std::make_shared<ScriptPlanner>();
        Plan plan;
        plan.plan_id = "flush-plan";
        plan.goal = "flush interruptions";
        plan.steps = {toolStep("flush-step", "outbox_ok")};
        planner->plan = plan;
        Harness harness("flush", planner);
        harness.controller.execute_goal(plan.goal);
        if (!waitState(harness.controller, Thoth::ControllerState::COMPLETED, 5000)) {
            std::cerr << "flush: completion missing, state " << static_cast<int>(harness.controller.get_state()) << "\n";
            return false;
        }
        finished = harness.controller.get_current_plan();
        if (countExactId(validLines(env.metrics), "flush-plan") != 0) {
            std::cerr << "flush: line appended before the append point\n";
            return false;
        }
        if (scalar(env.db(), "SELECT flushed FROM cognitive_metrics_outbox WHERE metrics_id='flush-plan'") != 0) {
            std::cerr << "flush: row marked flushed before append\n";
            return false;
        }
        const std::string original = textColumn(
            env.db(), "SELECT payload_json FROM cognitive_metrics_outbox WHERE metrics_id='flush-plan'");
        auto original_json = nlohmann::json::parse(original);
        if (original_json.value("metrics_id", "") != "flush-plan"
            || original_json.value("metrics_partial", true)
            || !original_json.contains("total_tokens")) {
            std::cerr << "flush: stored payload is not the exact terminal record\n";
            return false;
        }
        harness.memory->commitCognitiveMetrics("flush-plan", "{\"metrics_id\":\"flush-plan\",\"outcome\":\"tampered\"}");
        const std::string kept = textColumn(
            env.db(), "SELECT payload_json FROM cognitive_metrics_outbox WHERE metrics_id='flush-plan'");
        if (kept != original) {
            std::cerr << "flush: a later enqueue replaced the immutable payload\n";
            return false;
        }
    }

    unsetenv("THOTH_INJECT_METRICS_FLUSH");
    {
        auto planner = std::make_shared<ScriptPlanner>();
        Harness harness("flush", planner);
        finished.history_aging_done = true;
        finished.graph_reinforced = true;
        finished.status = PlanStatus::COMPLETED;
        harness.controller.resume_from_plan(finished);
        if (countExactId(validLines(env.metrics), "flush-plan") != 1) {
            std::cerr << "flush: restart did not append the pending record once\n";
            return false;
        }
        if (scalar(env.db(), "SELECT flushed FROM cognitive_metrics_outbox WHERE metrics_id='flush-plan'") != 1) {
            std::cerr << "flush: restart did not mark the delivered record\n";
            return false;
        }
    }
    {
        auto planner = std::make_shared<ScriptPlanner>();
        Harness harness("flush", planner);
        harness.controller.resume_from_plan(finished);
        if (countExactId(validLines(env.metrics), "flush-plan") != 1) {
            std::cerr << "flush: repeated restart appended another record\n";
            return false;
        }
    }

    {
        setenv("THOTH_INJECT_METRICS_FLUSH", "after_append", 1);
        auto planner = std::make_shared<ScriptPlanner>();
        Plan plan;
        plan.plan_id = "after-plan";
        plan.goal = "after append";
        plan.steps = {toolStep("after-step", "outbox_ok")};
        planner->plan = plan;
        Harness harness("flush-after", planner);
        harness.controller.execute_goal(plan.goal);
        if (!waitState(harness.controller, Thoth::ControllerState::COMPLETED, 5000)) {
            std::cerr << "after append: completion missing\n";
            return false;
        }
        if (countExactId(validLines(env.metrics), "after-plan") != 1
            || scalar(env.db(), "SELECT flushed FROM cognitive_metrics_outbox WHERE metrics_id='after-plan'") != 0) {
            std::cerr << "after append: expected one line and an unflushed row\n";
            return false;
        }
        finished = harness.controller.get_current_plan();
    }
    unsetenv("THOTH_INJECT_METRICS_FLUSH");
    finished.history_aging_done = true;
    finished.graph_reinforced = true;
    finished.status = PlanStatus::COMPLETED;
    for (int restart = 0; restart < 2; ++restart) {
        auto planner = std::make_shared<ScriptPlanner>();
        Harness harness("flush-after", planner);
        harness.controller.resume_from_plan(finished);
        if (countExactId(validLines(env.metrics), "after-plan") != 1) {
            std::cerr << "after append: restart " << restart << " changed the line count\n";
            return false;
        }
    }
    if (scalar(env.db(), "SELECT flushed FROM cognitive_metrics_outbox WHERE metrics_id='after-plan'") != 1) {
        std::cerr << "after append: delivered row stayed unflushed\n";
        return false;
    }
    return true;
}

bool testTruncatedTailAndExactId() {
    EnvGuard env("tail");
    const std::string original =
        "{\"metrics_id\":\"other\",\"goal\":\"mentions flush-plan\"}\n{\"metrics_id\":\"tail-plan\"";
    {
        std::ofstream out(env.metrics, std::ios::binary);
        out << original;
    }
    if (Thoth::CognitiveMetricsLogger::instance().metricsIdDelivered("tail-plan")) {
        std::cerr << "tail: truncated line counted as delivered\n";
        return false;
    }
    if (Thoth::CognitiveMetricsLogger::instance().metricsIdDelivered("flush-plan")) {
        std::cerr << "tail: substring inside another record counted as delivered\n";
        return false;
    }
    if (!Thoth::CognitiveMetricsLogger::instance().metricsIdDelivered("other")) {
        std::cerr << "tail: exact metrics_id was not recognized\n";
        return false;
    }

    nlohmann::json payload = {{"metrics_id", "tail-plan"}, {"outcome", "completed"}, {"metrics_partial", false}};
    auto planner = std::make_shared<ScriptPlanner>();
    Harness harness("tail", planner);
    if (!harness.memory->commitCognitiveMetrics("tail-plan", payload.dump())) {
        std::cerr << "tail: enqueue failed\n";
        return false;
    }
    Plan plan;
    plan.plan_id = "legacy-terminal";
    plan.goal = "legacy";
    plan.status = PlanStatus::COMPLETED;
    plan.history_aging_done = true;
    plan.graph_reinforced = true;
    harness.controller.resume_from_plan(plan);
    std::ifstream in(env.metrics, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.compare(0, original.size(), original) != 0) {
        std::cerr << "tail: original log bytes were not preserved\n";
        return false;
    }
    if (countExactId(validLines(env.metrics), "tail-plan") != 1
        || countExactId(validLines(env.metrics), "legacy-terminal") != 0) {
        std::cerr << "tail: flush fabricated or duplicated a record\n";
        return false;
    }
    harness.controller.resume_from_plan(plan);
    if (countExactId(validLines(env.metrics), "tail-plan") != 1) {
        std::cerr << "tail: repeated restart appended again\n";
        return false;
    }
    return true;
}

bool testLegacyTerminalIsNotFabricated() {
    EnvGuard env("legacy");
    auto planner = std::make_shared<ScriptPlanner>();
    Harness harness("legacy", planner);
    Plan plan;
    plan.plan_id = "legacy-plan";
    plan.goal = "already finished";
    plan.status = PlanStatus::COMPLETED;
    plan.history_aging_done = true;
    plan.graph_reinforced = true;
    harness.controller.resume_from_plan(plan);
    harness.controller.resume_from_plan(plan);
    if (std::filesystem::exists(env.metrics) || scalar(env.db(), "SELECT COUNT(*) FROM cognitive_metrics_outbox") != 0) {
        std::cerr << "legacy: a terminal plan without an outbox row gained a metrics record\n";
        return false;
    }
    return true;
}

bool testFailClosedAndAbortDoNotLearn() {
    EnvGuard env("closed");
    {
        auto planner = std::make_shared<ScriptPlanner>();
        Harness harness("closed", planner);
        Plan plan;
        plan.plan_id = "closed-plan";
        plan.goal = "fail closed";
        plan.plan_origin = "unknown";
        plan.status = PlanStatus::ACTIVE;
        plan.steps = {toolStep("closed-step", "outbox_ok")};
        plan.steps[0].status = StepStatus::RUNNING;
        plan.goal_metrics.present = true;
        plan.goal_metrics.window_assigned = true;
        plan.goal_metrics.window_id = "original-window";
        plan.goal_metrics.protocol_version = "C6.4 v1.0";
        plan.goal_metrics.metric_schema_version = "1.0";
        plan.goal_metrics.environment_schema_version = "c64-env-1";
        plan.goal_metrics.cohort_fingerprint = std::string(64, 'a');
        plan.goal_metrics.goal_started_at_ms = 1700000000000LL;
        plan.goal_metrics.total_tokens = 80;
        plan.goal_metrics.planning_tokens = 10;
        harness.controller.resume_from_plan(plan);
        const auto rows = validLines(env.metrics);
        if (countExactId(rows, "closed-plan") != 1) {
            std::cerr << "fail-closed: expected one metrics record\n";
            return false;
        }
        const auto& row = rows[0];
        if (row.value("outcome", "") != "failed" || !row.value("metrics_partial", false)
            || row.value("metrics_partial_reason", "") != "checkpoint_gap"
            || row.value("window_id", "") != "original-window"
            || row.value("learning_eligible", true)
            || row.contains("total_tokens") || row.contains("total_wall_clock_ms")
            || !row.contains("available_counters")
            || row["available_counters"].value("counters_are_exact_totals", true)
            || row["available_counters"].value("total_tokens", 0) != 80) {
            std::cerr << "fail-closed: partial metrics or window were not preserved\n";
            return false;
        }
        if (!learningTablesEmpty(env.db())) {
            std::cerr << "fail-closed: learning or graph rows were written\n";
            return false;
        }
    }
    {
        ProbeTool::released() = false;
        auto planner = std::make_shared<ScriptPlanner>();
        Plan plan;
        plan.plan_id = "abort-plan";
        plan.goal = "abort metrics";
        plan.steps = {toolStep("abort-step", "outbox_block")};
        planner->plan = plan;
        Harness harness("abort", planner);
        harness.controller.execute_goal(plan.goal);
        if (!waitState(harness.controller, Thoth::ControllerState::EXECUTING_STEP, 3000)) {
            ProbeTool::released() = true;
            std::cerr << "abort: step did not start\n";
            return false;
        }
        harness.controller.abort();
        if (!waitState(harness.controller, Thoth::ControllerState::ABORTED, 3000)) {
            ProbeTool::released() = true;
            std::cerr << "abort: state did not become aborted\n";
            return false;
        }
        ProbeTool::released() = true;
        const auto rows = validLines(env.metrics);
        if (countExactId(rows, "abort-plan") != 1) {
            std::cerr << "abort: expected one aborted metrics record\n";
            return false;
        }
        bool saw = false;
        for (const auto& row : rows) {
            if (row.value("metrics_id", "") == "abort-plan") {
                saw = row.value("outcome", "") == "aborted";
            }
        }
        if (!saw || !learningTablesEmpty(env.db())) {
            std::cerr << "abort: outcome or learning tables were wrong\n";
            return false;
        }
    }
    return true;
}

bool testReflectionAgingDoesNotEnqueue() {
    EnvGuard env("reflect");
    auto planner = std::make_shared<QueuePlanner>();
    Plan first;
    first.plan_id = "reflect-first";
    first.goal = "needs reflection";
    first.steps = {toolStep("reflect-bad", "outbox_fail")};
    Plan second;
    second.plan_id = "reflect-next";
    second.goal = "replacement";
    second.steps = {toolStep("reflect-good", "outbox_ok")};
    planner->plans = {first, second};
    Harness harness("reflect", planner);
    harness.controller.set_max_reflections(1);
    harness.controller.execute_goal(first.goal);
    if (!waitState(harness.controller, Thoth::ControllerState::COMPLETED, 8000)) {
        std::cerr << "reflection: replacement did not complete, state "
                  << static_cast<int>(harness.controller.get_state()) << "\n";
        return false;
    }
    if (scalar(env.db(), "SELECT COUNT(*) FROM cognitive_metrics_outbox WHERE metrics_id='reflect-first'") != 0
        || countExactId(validLines(env.metrics), "reflect-first") != 0
        || countExactId(validLines(env.metrics), "reflect-next") != 1) {
        std::cerr << "reflection: intermediate aging enqueued or the replacement record is missing\n";
        return false;
    }
    return true;
}

int resumePendingFlush() {
    Config config;
    auto memory = std::make_shared<Memory>(config);
    auto planner = std::make_shared<ScriptPlanner>();
    auto tools = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
    controller.set_config(&config);
    controller.set_session_id("outbox-race");
    controller.set_max_reflections(0);
    Plan plan;
    plan.plan_id = "carrier-plan";
    plan.goal = "carrier";
    plan.status = PlanStatus::COMPLETED;
    plan.history_aging_done = true;
    plan.graph_reinforced = true;
    controller.resume_from_plan(plan);
    return 0;
}

pid_t forkFlush(const char* inject) {
    const pid_t pid = fork();
    if (pid == 0) {
        if (inject) setenv("THOTH_INJECT_METRICS_FLUSH", inject, 1);
        else unsetenv("THOTH_INJECT_METRICS_FLUSH");
        const int rc = resumePendingFlush();
        _exit(rc);
    }
    return pid;
}

int waitChild(pid_t pid) {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return 99;
    return status;
}

bool testConcurrentWritersOneLine() {
    EnvGuard env("concurrent");
    {
        Config config;
        Memory memory(config);
        if (!memory.commitCognitiveMetrics(
                "race-plan",
                "{\"metrics_id\":\"race-plan\",\"outcome\":\"completed\",\"metrics_partial\":false}")) {
            std::cerr << "concurrent: enqueue failed\n";
            return false;
        }
    }
    const pid_t holder = forkFlush("hold_after_scan");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < deadline &&
           !std::filesystem::exists(env.metrics + ".hold")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!std::filesystem::exists(env.metrics + ".hold")) {
        kill(holder, SIGKILL);
        waitChild(holder);
        std::cerr << "concurrent: holder did not reach the locked scan\n";
        return false;
    }
    const pid_t competitor = forkFlush(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    if (countExactId(validLines(env.metrics), "race-plan") != 0) {
        std::ofstream release(env.metrics + ".release");
        release << "1\n";
        kill(holder, SIGKILL);
        kill(competitor, SIGKILL);
        waitChild(holder);
        waitChild(competitor);
        std::cerr << "concurrent: second writer appended while the lock was held\n";
        return false;
    }
    {
        std::ofstream release(env.metrics + ".release");
        release << "1\n";
    }
    const int holderStatus = waitChild(holder);
    const int competitorStatus = waitChild(competitor);
    if (!WIFEXITED(holderStatus) || WEXITSTATUS(holderStatus) != 0 ||
        !WIFEXITED(competitorStatus) || WEXITSTATUS(competitorStatus) != 0) {
        std::cerr << "concurrent: writer process failed\n";
        return false;
    }
    if (countExactId(validLines(env.metrics), "race-plan") != 1 ||
        scalar(env.db(), "SELECT flushed FROM cognitive_metrics_outbox WHERE metrics_id='race-plan'") != 1) {
        std::cerr << "concurrent: expected one delivered line\n";
        return false;
    }
    return true;
}

bool testCrashAfterAppendRecoversOneLine() {
    EnvGuard env("crash-append");
    {
        Config config;
        Memory memory(config);
        if (!memory.commitCognitiveMetrics(
                "crash-plan",
                "{\"metrics_id\":\"crash-plan\",\"outcome\":\"completed\",\"metrics_partial\":false}")) {
            std::cerr << "crash: enqueue failed\n";
            return false;
        }
    }
    const pid_t child = forkFlush("kill_after_append");
    const int status = waitChild(child);
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL) {
        std::cerr << "crash: child was not killed after append\n";
        return false;
    }
    if (countExactId(validLines(env.metrics), "crash-plan") != 1 ||
        scalar(env.db(), "SELECT flushed FROM cognitive_metrics_outbox WHERE metrics_id='crash-plan'") != 0) {
        std::cerr << "crash: expected one line and an unflushed row\n";
        return false;
    }
    const pid_t recovery = forkFlush(nullptr);
    const int recoveryStatus = waitChild(recovery);
    if (!WIFEXITED(recoveryStatus) || WEXITSTATUS(recoveryStatus) != 0 ||
        countExactId(validLines(env.metrics), "crash-plan") != 1 ||
        scalar(env.db(), "SELECT flushed FROM cognitive_metrics_outbox WHERE metrics_id='crash-plan'") != 1) {
        std::cerr << "crash: recovery appended another line or left the row pending\n";
        return false;
    }
    return true;
}

bool testCrossDatabaseSameLog() {
    EnvGuard env("cross-db");
    const std::string workspaceB = env.root + "/workspace-b";
    std::filesystem::create_directories(workspaceB);
    {
        Config config;
        Memory first(config);
        if (!first.commitCognitiveMetrics(
                "db-a",
                "{\"metrics_id\":\"db-a\",\"outcome\":\"completed\",\"metrics_partial\":false}")) {
            return false;
        }
    }
    setenv("THOTH_WORKSPACE_PATH", workspaceB.c_str(), 1);
    {
        Config config;
        Memory second(config);
        if (!second.commitCognitiveMetrics(
                "db-b",
                "{\"metrics_id\":\"db-b\",\"outcome\":\"failed\",\"metrics_partial\":true}")) {
            return false;
        }
    }
    setenv("THOTH_WORKSPACE_PATH", env.workspace.c_str(), 1);
    const pid_t holder = fork();
    if (holder == 0) {
        setenv("THOTH_WORKSPACE_PATH", env.workspace.c_str(), 1);
        setenv("THOTH_INJECT_METRICS_FLUSH", "hold_after_scan", 1);
        _exit(resumePendingFlush());
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < deadline &&
           !std::filesystem::exists(env.metrics + ".hold")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!std::filesystem::exists(env.metrics + ".hold")) {
        kill(holder, SIGKILL);
        waitChild(holder);
        std::cerr << "cross-db: holder did not lock the shared log\n";
        return false;
    }
    const pid_t other = fork();
    if (other == 0) {
        setenv("THOTH_WORKSPACE_PATH", workspaceB.c_str(), 1);
        unsetenv("THOTH_INJECT_METRICS_FLUSH");
        _exit(resumePendingFlush());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    if (countExactId(validLines(env.metrics), "db-b") != 0) {
        std::ofstream release(env.metrics + ".release");
        release << "1\n";
        kill(holder, SIGKILL);
        kill(other, SIGKILL);
        waitChild(holder);
        waitChild(other);
        std::cerr << "cross-db: the second database appended during the lock\n";
        return false;
    }
    {
        std::ofstream release(env.metrics + ".release");
        release << "1\n";
    }
    waitChild(holder);
    waitChild(other);
    const auto rows = validLines(env.metrics);
    if (countExactId(rows, "db-a") != 1 || countExactId(rows, "db-b") != 1) {
        std::cerr << "cross-db: shared log does not contain exactly one line per database\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    const bool ok = testAgingRollbackLeavesNoOutbox()
        && testFlushInterruptionsAndRestart()
        && testTruncatedTailAndExactId()
        && testLegacyTerminalIsNotFabricated()
        && testFailClosedAndAbortDoNotLearn()
        && testReflectionAgingDoesNotEnqueue()
        && testConcurrentWritersOneLine()
        && testCrashAfterAppendRecoversOneLine()
        && testCrossDatabaseSameLog();
    if (!ok) return 1;
    std::cout << "cognitive metrics outbox tests passed\n";
    return 0;
}
