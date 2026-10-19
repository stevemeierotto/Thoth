/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — Phase 10C-3 reflection recovery
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "config.h"
#include "executive_controller.h"
#include "itool.h"
#include "memory.h"
#include "plan.h"
#include "tools.h"
#include "trajectory.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace {

class StatusTool : public ITool {
public:
    StatusTool(std::string name, bool ok) : name_(std::move(name)), ok_(ok) {}
    std::string name() const override { return name_; }
    std::string description() const override { return "reflection recovery probe"; }
    nlohmann::json input_schema() const override { return nlohmann::json::object(); }
    bool requires_confirmation() const override { return false; }
    nlohmann::json execute(const nlohmann::json&) const override {
        if (ok_) return {{"status", "success"}, {"data", {{"probe", true}}}};
        return {{"status", "error"}, {"error_message", "phase10c reflection fail"}};
    }
private:
    std::string name_;
    bool ok_;
};

class ScriptPlanner : public IPlanner {
public:
    Plan plan;
    int calls = 0;
    std::string seen_goal;
    std::mutex mu;
    Plan create_plan(const std::string& goal) override {
        std::lock_guard<std::mutex> lock(mu);
        ++calls;
        seen_goal = goal;
        return plan;
    }
    Plan revise_plan(const Plan& existing, const nlohmann::json&) override { return existing; }
    std::string goal() {
        std::lock_guard<std::mutex> lock(mu);
        return seen_goal;
    }
    int callCount() {
        std::lock_guard<std::mutex> lock(mu);
        return calls;
    }
};

struct EnvGuard {
    const char* prev_workspace;
    const char* prev_logs;
    const char* prev_inject;
    std::string root;
    explicit EnvGuard(const std::string& name)
        : prev_workspace(std::getenv("THOTH_WORKSPACE_PATH")),
          prev_logs(std::getenv("THOTH_LOGS_PATH")),
          prev_inject(std::getenv("THOTH_INJECT_AGING_COMMIT_FAIL")) {
        root = "/tmp/thoth-reflection-" + name;
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root + "/workspace");
        std::filesystem::create_directories(root + "/logs");
        setenv("THOTH_WORKSPACE_PATH", (root + "/workspace").c_str(), 1);
        setenv("THOTH_LOGS_PATH", (root + "/logs").c_str(), 1);
        unsetenv("THOTH_INJECT_AGING_COMMIT_FAIL");
        unsetenv("THOTH_MOCK_LLM");
    }
    ~EnvGuard() {
        restore("THOTH_WORKSPACE_PATH", prev_workspace);
        restore("THOTH_LOGS_PATH", prev_logs);
        restore("THOTH_INJECT_AGING_COMMIT_FAIL", prev_inject);
    }
    static void restore(const char* key, const char* value) {
        if (value) setenv(key, value, 1);
        else unsetenv(key);
    }
};

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

bool waitFor(const std::function<bool()>& ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ready()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

std::optional<Plan> loadActive(Memory& memory, const std::string& session) {
    const auto row = memory.getActivePlan(session);
    if (!row) return std::nullopt;
    return Plan::from_json(nlohmann::json::parse(row->steps_json));
}

bool trajectoryHasError(const nlohmann::json& trajectory) {
    if (!trajectory.contains("steps") || !trajectory["steps"].is_array()) return false;
    for (const auto& step : trajectory["steps"]) {
        if (step.is_object() && step.value("error", "") == "phase10c reflection fail") return true;
    }
    return false;
}

bool testAfterGraphCommitKeepsTrajectoryAndScore() {
    EnvGuard env("graph-commit");
    setenv("THOTH_INJECT_AGING_COMMIT_FAIL", "1", 1);
    Config config;
    auto memory = std::make_shared<Memory>(config);
    auto tools = std::make_shared<ToolRegistry>();
    tools->registerTool(std::make_unique<StatusTool>("reflect_fail", false));
    auto planner = std::make_shared<ScriptPlanner>();
    Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
    controller.set_config(&config);
    controller.set_session_id("reflect-session");
    controller.set_max_reflections(1);
    memory->setActiveSessionId("reflect-session");

    Plan plan;
    plan.plan_id = "reflect-plan";
    plan.goal = "phase10c reflection";
    plan.plan_origin = "model";
    plan.steps = {toolStep("reflect-1", "reflect_fail", StepStatus::PENDING)};
    planner->plan = plan;
    controller.execute_goal(plan.goal);

    const bool durable = waitFor([&]() {
        const auto stored = loadActive(*memory, "reflect-session");
        return stored && stored->reflection_pending && stored->graph_reinforced &&
               !stored->history_aging_done && stored->recorded_trajectory_score_present &&
               stored->working_trajectory_present;
    });
    if (!durable || planner->callCount() != 1) return false;
    const auto stored = loadActive(*memory, "reflect-session");
    const auto trajectory = memory->loadTrajectory("traj-reflect-plan");
    if (!stored || !trajectory) return false;
    return stored->recorded_trajectory_score == 0.0f &&
           trajectoryHasError(stored->working_trajectory) &&
           trajectory->success_score == 0.0f &&
           stored->status == PlanStatus::ACTIVE;
}

bool testResumeRestoresTrajectoryAndUsesScore() {
    EnvGuard env("resume");
    Config config;
    auto memory = std::make_shared<Memory>(config);
    memory->setActiveSessionId("reflect-session");
    Thoth::Trajectory trajectory;
    trajectory.trajectory_id = "traj-reflect-plan";
    trajectory.goal = "phase10c reflection";
    trajectory.success_score = 0.0f;
    Thoth::RecordedStep recorded;
    recorded.step_id = "reflect-1";
    recorded.description = "reflect-1";
    recorded.error = "phase10c reflection fail";
    trajectory.steps.push_back(recorded);
    Memory::CognateTrajectoryRecord row;
    row.trajectory_id = trajectory.trajectory_id;
    row.goal = trajectory.goal;
    row.trajectory_json = trajectory.to_json().dump();
    row.success_score = 0.0f;
    row.plan_origin = "model";
    row.created_at = 50;
    if (!memory->saveTrajectory(row)) return false;

    Plan plan;
    plan.plan_id = "reflect-plan";
    plan.goal = "phase10c reflection";
    plan.plan_origin = "model";
    plan.status = PlanStatus::ACTIVE;
    plan.graph_reinforced = true;
    plan.reflection_pending = true;
    plan.history_aging_done = false;
    plan.recorded_trajectory_score_present = true;
    plan.recorded_trajectory_score = 0.0f;
    plan.working_trajectory_present = true;
    plan.working_trajectory = trajectory.to_json();
    plan.steps = {toolStep("reflect-1", "reflect_ok", StepStatus::FAILED)};

    auto tools = std::make_shared<ToolRegistry>();
    tools->registerTool(std::make_unique<StatusTool>("reflect_ok", true));
    auto planner = std::make_shared<ScriptPlanner>();
    Plan replacement;
    replacement.plan_id = "replacement-plan";
    replacement.goal = "ignored";
    replacement.plan_origin = "model";
    replacement.steps = {toolStep("next", "reflect_ok", StepStatus::PENDING)};
    planner->plan = replacement;
    Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
    controller.set_config(&config);
    controller.set_session_id("reflect-session");
    controller.set_max_reflections(0);
    controller.resume_from_plan(plan);

    if (!waitFor([&]() { return planner->goal().find("0.000000") != std::string::npos; })) return false;
    const auto kept = memory->loadTrajectory("traj-reflect-plan");
    const std::string goal = planner->goal();
    return kept && kept->success_score == 0.0f &&
           goal.find("phase10c reflection (Reflection: previous attempt had low success score 0.000000)") !=
               std::string::npos;
}

class AgingPlanner : public IPlanner {
public:
    Memory* memory = nullptr;
    std::string session;
    Plan replacement;
    std::string seen;
    Plan snapshot;
    bool saw = false;
    std::mutex mu;
    Plan create_plan(const std::string& goal) override {
        std::lock_guard<std::mutex> lock(mu);
        seen = goal;
        if (memory) {
            const auto row = memory->getActivePlan(session);
            if (row) {
                snapshot = Plan::from_json(nlohmann::json::parse(row->steps_json));
                saw = snapshot.history_aging_done && snapshot.working_trajectory_present &&
                      snapshot.recorded_trajectory_score_present;
            }
        }
        return replacement;
    }
    Plan revise_plan(const Plan& existing, const nlohmann::json&) override { return existing; }
};

bool testRecordedScoreSurvivesAging() {
    EnvGuard env("score");
    Config config;
    auto memory = std::make_shared<Memory>(config);
    memory->setActiveSessionId("score-session");
    Thoth::Trajectory trajectory;
    trajectory.trajectory_id = "traj-score-plan";
    trajectory.goal = "score goal";
    Thoth::RecordedStep recorded;
    recorded.step_id = "done";
    recorded.error = "recorded mismatch";
    trajectory.steps.push_back(recorded);

    Plan plan;
    plan.plan_id = "score-plan";
    plan.goal = "score goal";
    plan.plan_origin = "model";
    plan.status = PlanStatus::ACTIVE;
    plan.graph_reinforced = true;
    plan.reflection_pending = true;
    plan.history_aging_done = false;
    plan.recorded_trajectory_score_present = true;
    plan.recorded_trajectory_score = 0.25f;
    plan.working_trajectory_present = true;
    plan.working_trajectory = trajectory.to_json();
    plan.working_revisions_count = 2;
    plan.steps = {toolStep("done", "reflect_ok", StepStatus::SUCCESS)};

    auto tools = std::make_shared<ToolRegistry>();
    tools->registerTool(std::make_unique<StatusTool>("reflect_ok", true));
    auto planner = std::make_shared<AgingPlanner>();
    planner->memory = memory.get();
    planner->session = "score-session";
    planner->replacement.plan_id = "after-score";
    planner->replacement.plan_origin = "model";
    planner->replacement.steps = {toolStep("next", "reflect_ok", StepStatus::PENDING)};
    Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
    controller.set_config(&config);
    controller.set_session_id("score-session");
    controller.set_max_reflections(0);
    controller.resume_from_plan(plan);
    if (!waitFor([&]() {
            std::lock_guard<std::mutex> lock(planner->mu);
            return planner->saw;
        })) {
        return false;
    }
    std::lock_guard<std::mutex> lock(planner->mu);
    const std::string needle = std::to_string(0.25f);
    return planner->seen.find(needle) != std::string::npos &&
           planner->seen.find(std::to_string(0.9f)) == std::string::npos &&
           planner->snapshot.recorded_trajectory_score == 0.25f &&
           planner->snapshot.working_revisions_count == 2 &&
           planner->snapshot.working_trajectory["steps"].size() == 1;
}

bool testLegacyRecalculatesMissingScore() {
    EnvGuard env("legacy");
    Config config;
    auto memory = std::make_shared<Memory>(config);
    memory->setActiveSessionId("legacy-session");
    Plan plan;
    plan.plan_id = "legacy-plan";
    plan.goal = "legacy goal";
    plan.plan_origin = "fallback";
    plan.status = PlanStatus::ACTIVE;
    plan.graph_reinforced = true;
    plan.reflection_pending = true;
    plan.history_aging_done = true;
    plan.steps = {toolStep("done", "reflect_ok", StepStatus::SUCCESS)};
    auto tools = std::make_shared<ToolRegistry>();
    tools->registerTool(std::make_unique<StatusTool>("reflect_ok", true));
    auto planner = std::make_shared<ScriptPlanner>();
    planner->plan.plan_id = "legacy-next";
    planner->plan.plan_origin = "model";
    planner->plan.steps = {toolStep("next", "reflect_ok", StepStatus::PENDING)};
    Thoth::ExecutiveController controller(planner, tools, nullptr, memory);
    controller.set_config(&config);
    controller.set_session_id("legacy-session");
    controller.set_max_reflections(0);
    controller.resume_from_plan(plan);
    return waitFor([&]() {
        return planner->goal().find(
                   "legacy goal (Reflection: previous attempt had low success score 1.000000)") !=
               std::string::npos;
    });
}

} // namespace

int main() {
    if (!testAfterGraphCommitKeepsTrajectoryAndScore()) {
        std::cerr << "after_graph_commit test failed\n";
        return 1;
    }
    if (!testResumeRestoresTrajectoryAndUsesScore()) {
        std::cerr << "trajectory restoration test failed\n";
        return 1;
    }
    if (!testRecordedScoreSurvivesAging()) {
        std::cerr << "score consistency test failed\n";
        return 1;
    }
    if (!testLegacyRecalculatesMissingScore()) {
        std::cerr << "legacy reflection test failed\n";
        return 1;
    }
    std::cout << "reflection recovery tests passed\n";
    return 0;
}
