/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — Phase 10C-2 episode persistence and recovery
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "config.h"
#include "executive_controller.h"
#include "itool.h"
#include "memory.h"
#include "memory_repository.h"
#include "plan.h"
#include "tools.h"

#include <sqlite3.h>

#include <stdexcept>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

class ProbeTool : public ITool {
public:
    explicit ProbeTool(std::string name) : name_(std::move(name)) {}
    std::string name() const override { return name_; }
    std::string description() const override { return "episode recovery probe"; }
    nlohmann::json input_schema() const override { return nlohmann::json::object(); }
    bool requires_confirmation() const override { return false; }
    nlohmann::json execute(const nlohmann::json&) const override {
        return {{"status", "success"}, {"data", {{"probe", true}}}};
    }
private:
    std::string name_;
};

class ScriptPlanner : public IPlanner {
public:
    Plan plan;
    Plan create_plan(const std::string&) override { return plan; }
    Plan revise_plan(const Plan& existing, const nlohmann::json&) override { return existing; }
};

struct EnvGuard {
    const char* prev_workspace;
    const char* prev_logs;
    const char* prev_inject;
    explicit EnvGuard(const std::string& name)
        : prev_workspace(std::getenv("THOTH_WORKSPACE_PATH")),
          prev_logs(std::getenv("THOTH_LOGS_PATH")),
          prev_inject(std::getenv("THOTH_INJECT_EPISODE_CHECKPOINT_FAIL")) {
        root = "/tmp/thoth-episode-" + name;
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root + "/workspace");
        std::filesystem::create_directories(root + "/logs");
        setenv("THOTH_WORKSPACE_PATH", (root + "/workspace").c_str(), 1);
        setenv("THOTH_LOGS_PATH", (root + "/logs").c_str(), 1);
        unsetenv("THOTH_INJECT_EPISODE_CHECKPOINT_FAIL");
        unsetenv("THOTH_MOCK_LLM");
    }
    ~EnvGuard() {
        restore("THOTH_WORKSPACE_PATH", prev_workspace);
        restore("THOTH_LOGS_PATH", prev_logs);
        restore("THOTH_INJECT_EPISODE_CHECKPOINT_FAIL", prev_inject);
    }
    std::string dbPath() const { return root + "/workspace/memory.db"; }
    std::string root;
    static void restore(const char* key, const char* value) {
        if (value) setenv(key, value, 1);
        else unsetenv(key);
    }
};

Memory::EpisodeStepRecord observation(const std::string& goal, int index, const std::string& status,
                                      const std::string& summary) {
    Memory::EpisodeStepRecord step;
    step.episode_id = "traj-" + goal;
    step.goal_id = goal;
    step.step_index = index;
    step.state_summary = summary;
    step.action_taken = "probe";
    step.result_status = status;
    step.timestamp_ms = 1000 + index;
    return step;
}

void seedEpisodeRows(const std::string& path, bool conflicting) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("seed open failed");
    }
    char* err = nullptr;
    sqlite3_exec(db,
                 "CREATE TABLE episode_steps ("
                 "episode_id TEXT, goal_id TEXT, step_index INTEGER, state_summary TEXT, "
                 "action_taken TEXT, result_status TEXT, embedding_blob BLOB, timestamp_ms INTEGER);",
                 nullptr, nullptr, &err);
    sqlite3_free(err);
    const char* insert =
        "INSERT INTO episode_steps (episode_id, goal_id, step_index, state_summary, action_taken, "
        "result_status, timestamp_ms) VALUES (?, 'seed-goal', 0, 'seed summary', 'probe', ?, ?);";
    for (int copy = 0; copy < 2; ++copy) {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, insert, -1, &stmt, nullptr);
        const char* status = (conflicting && copy == 1) ? "FAILED" : "SUCCESS";
        sqlite3_bind_text(stmt, 1, "traj-seed-goal", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, status, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, 1000 + copy);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

Thoth::MemoryRepository::ActivePlanRecord activeRecord(const std::string& plan_id, const std::string& goal) {
    Thoth::MemoryRepository::ActivePlanRecord record;
    record.plan_id = plan_id;
    record.session_id = "episode-session";
    record.goal = goal;
    record.steps_json = Plan{}.to_json().dump();
    record.current_index = 0;
    record.controller_state = "IDLE";
    record.created_at_ms = 1;
    record.updated_at_ms = 1;
    return record;
}

bool testInterruptedCheckpointRollsBack() {
    EnvGuard env("interrupt");
    auto memory = std::make_shared<Memory>(Config{});
    memory->setActiveSessionId("episode-session");
    if (!memory->storeActivePlan(activeRecord("goal-plan", "old goal"))) return false;

    setenv("THOTH_INJECT_EPISODE_CHECKPOINT_FAIL", "1", 1);
    const bool failed = memory->commitStepCheckpoint(
        activeRecord("goal-plan", "new goal"),
        std::nullopt,
        observation("goal-plan", 0, "SUCCESS", "committed step"));
    unsetenv("THOTH_INJECT_EPISODE_CHECKPOINT_FAIL");
    if (failed) return false;
    const auto still = memory->getActivePlan("episode-session");
    if (!still || still->plan_id != "goal-plan" || still->goal != "old goal") return false;
    if (!memory->getAllEpisodeSteps().empty()) return false;

    if (!memory->commitStepCheckpoint(activeRecord("goal-plan", "new goal"), std::nullopt,
                                      observation("goal-plan", 0, "SUCCESS", "committed step"))) {
        return false;
    }
    if (!memory->commitStepCheckpoint(activeRecord("goal-plan", "new goal"), std::nullopt,
                                      observation("goal-plan", 0, "SUCCESS", "committed step"))) {
        return false;
    }
    const auto rows = memory->getAllEpisodeSteps();
    const auto saved = memory->getActivePlan("episode-session");
    return rows.size() == 1 && rows[0].goal_id == "goal-plan" && rows[0].result_status == "SUCCESS" &&
           saved && saved->goal == "new goal";
}

Plan durablePlan() {
    Plan plan;
    plan.plan_id = "backfill-plan";
    plan.goal = "backfill";
    plan.plan_origin = "fallback";
    plan.status = PlanStatus::COMPLETED;
    PlanStep done;
    done.step_id = "done";
    done.description = "durable success";
    done.type = StepType::TOOL;
    done.tool = {{"name", "ep_ok"}};
    done.status = StepStatus::SUCCESS;
    done.completed_at_ms = 42;
    PlanStep failed;
    failed.step_id = "failed";
    failed.description = "durable failure";
    failed.type = StepType::TOOL;
    failed.tool = {{"name", "ep_bad"}};
    failed.status = StepStatus::FAILED;
    failed.completed_at_ms = 43;
    plan.steps = {done, failed};
    return plan;
}

bool testMissingRowBackfillOmitsUnfinished() {
    EnvGuard env("backfill");
    {
        Config config;
        auto memory = std::make_shared<Memory>(config);
        auto tools = std::make_shared<ToolRegistry>();
        auto planner = std::make_shared<ScriptPlanner>();
        Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
        controller.set_config(&config);
        controller.set_session_id("backfill-session");
        controller.set_max_reflections(0);
        memory->setActiveSessionId("backfill-session");
        controller.resume_from_plan(durablePlan());
        const auto rows = memory->getAllEpisodeSteps();
        if (rows.size() != 2) return false;
        bool saw_success = false;
        bool saw_failure = false;
        for (const auto& row : rows) {
            if (row.step_index == 0 && row.result_status == "SUCCESS" && row.state_summary == "durable success" &&
                row.timestamp_ms == 42) {
                saw_success = true;
            }
            if (row.step_index == 1 && row.result_status == "FAILED" && row.state_summary == "durable failure") {
                saw_failure = true;
            }
        }
        if (!saw_success || !saw_failure) return false;
        controller.resume_from_plan(durablePlan());
        if (memory->getAllEpisodeSteps().size() != 2) return false;
    }
    EnvGuard running_env("running");
    {
        Config config;
        auto memory = std::make_shared<Memory>(config);
        auto tools = std::make_shared<ToolRegistry>();
        auto planner = std::make_shared<ScriptPlanner>();
        Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
        controller.set_config(&config);
        controller.set_session_id("running-session");
        controller.set_max_reflections(0);
        memory->setActiveSessionId("running-session");
        Plan plan = durablePlan();
        plan.plan_id = "running-plan";
        plan.status = PlanStatus::ACTIVE;
        plan.steps.resize(1);
        PlanStep running;
        running.step_id = "running";
        running.description = "not finished";
        running.type = StepType::TOOL;
        running.tool = {{"name", "ep_ok"}};
        running.status = StepStatus::RUNNING;
        plan.steps.push_back(running);
        controller.resume_from_plan(plan);
        if (controller.get_state() != Thoth::ControllerState::FAILED) return false;
        const auto rows = memory->getAllEpisodeSteps();
        if (rows.size() != 1 || rows[0].step_index != 0 || rows[0].result_status != "SUCCESS") return false;
    }
    return true;
}

bool statusesPreserved(const std::vector<Memory::EpisodeStepRecord>& rows) {
    bool success = false;
    bool failed = false;
    for (const auto& row : rows) {
        if (row.goal_id == "seed-goal" && row.step_index == 0 && row.result_status == "SUCCESS" &&
            row.state_summary == "seed summary") {
            success = true;
        }
        if (row.goal_id == "seed-goal" && row.step_index == 0 && row.result_status == "FAILED" &&
            row.state_summary == "seed summary") {
            failed = true;
        }
    }
    return success && failed;
}

bool testDuplicateAfterInsertPreservesConflicts() {
    EnvGuard identical_env("identical");
    seedEpisodeRows(identical_env.dbPath(), false);
    {
        auto memory = std::make_shared<Memory>(Config{});
        const auto before = memory->inspectEpisodeIdentity();
        if (before.row_count != 2 || before.identical_duplicate_groups != 1 ||
            before.conflicting_goal_step_groups != 0 || before.observation_unique_index) {
            return false;
        }
        memory->storeEpisodeStep(observation("seed-goal", 0, "SUCCESS", "seed summary"));
        const auto after = memory->inspectEpisodeIdentity();
        if (after.row_count != 2 || after.identical_duplicate_groups != 1) return false;
    }

    EnvGuard conflict_env("conflict");
    seedEpisodeRows(conflict_env.dbPath(), true);
    auto memory = std::make_shared<Memory>(Config{});
    const auto opened = memory->inspectEpisodeIdentity();
    if (opened.row_count != 2 || opened.conflicting_goal_step_groups != 1 ||
        opened.identical_duplicate_groups != 0 || !opened.observation_unique_index) {
        return false;
    }
    memory->storeEpisodeStep(observation("seed-goal", 0, "SUCCESS", "seed summary"));
    memory->storeEpisodeStep(observation("seed-goal", 0, "FAILED", "seed summary"));
    if (memory->inspectEpisodeIdentity().row_count != 2 || !statusesPreserved(memory->getAllEpisodeSteps())) {
        return false;
    }
    memory->storeEpisodeStep(observation("seed-goal", 0, "SUCCESS", "different summary"));
    const auto added = memory->getAllEpisodeSteps();
    int success_original = 0;
    int failed_original = 0;
    int different = 0;
    for (const auto& row : added) {
        if (row.result_status == "SUCCESS" && row.state_summary == "seed summary") success_original += 1;
        if (row.result_status == "FAILED" && row.state_summary == "seed summary") failed_original += 1;
        if (row.state_summary == "different summary") different += 1;
    }
    return success_original == 1 && failed_original == 1 && different == 1 && added.size() == 3;
}

bool waitState(Thoth::ExecutiveController& controller, Thoth::ControllerState wanted) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (controller.get_state() == wanted) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

bool testNormalRestartDoesNotDuplicate() {
    EnvGuard env("restart");
    Plan finished;
    {
        Config config;
        auto memory = std::make_shared<Memory>(config);
        auto tools = std::make_shared<ToolRegistry>();
        tools->registerTool(std::make_unique<ProbeTool>("ep_ok"));
        auto planner = std::make_shared<ScriptPlanner>();
        Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
        controller.set_config(&config);
        controller.set_session_id("restart-session");
        controller.set_max_reflections(0);
        memory->setActiveSessionId("restart-session");
        Plan plan;
        plan.plan_id = "restart-plan";
        plan.goal = "restart";
        plan.plan_origin = "model";
        PlanStep first;
        first.step_id = "restart-1";
        first.description = "first";
        first.type = StepType::TOOL;
        first.payload = {{"tool", "ep_ok"}};
        first.failure_policy.max_retries = 0;
        PlanStep second = first;
        second.step_id = "restart-2";
        second.description = "second";
        second.depends_on = {"restart-1"};
        plan.steps = {first, second};
        planner->plan = plan;
        controller.execute_goal(plan.goal);
        if (!waitState(controller, Thoth::ControllerState::COMPLETED)) return false;
        const auto rows = memory->getAllEpisodeSteps();
        if (rows.size() != 2) return false;
        finished = controller.get_current_plan();
    }
    Config config;
    auto memory = std::make_shared<Memory>(config);
    auto tools = std::make_shared<ToolRegistry>();
    auto planner = std::make_shared<ScriptPlanner>();
    Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
    controller.set_config(&config);
    controller.set_session_id("restart-session");
    controller.set_max_reflections(0);
    memory->setActiveSessionId("restart-session");
    controller.resume_from_plan(finished);
    const auto rows = memory->getAllEpisodeSteps();
    if (rows.size() != 2) return false;
    bool first = false;
    bool second = false;
    for (const auto& row : rows) {
        if (row.goal_id == "restart-plan" && row.step_index == 0 && row.result_status == "SUCCESS") first = true;
        if (row.goal_id == "restart-plan" && row.step_index == 1 && row.result_status == "SUCCESS") second = true;
    }
    const auto identity = memory->inspectEpisodeIdentity();
    return first && second && identity.duplicate_goal_step_groups == 0 && identity.observation_unique_index;
}

} // namespace

int main() {
    if (!testInterruptedCheckpointRollsBack()) {
        std::cerr << "interrupted checkpoint test failed\n";
        return 1;
    }
    if (!testMissingRowBackfillOmitsUnfinished()) {
        std::cerr << "missing-row backfill test failed\n";
        return 1;
    }
    if (!testDuplicateAfterInsertPreservesConflicts()) {
        std::cerr << "duplicate-after-insert test failed\n";
        return 1;
    }
    if (!testNormalRestartDoesNotDuplicate()) {
        std::cerr << "normal restart test failed\n";
        return 1;
    }
    std::cout << "episode recovery tests passed\n";
    return 0;
}
