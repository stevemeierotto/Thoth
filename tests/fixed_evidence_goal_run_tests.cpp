/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — one-shot EGAR-LX goal file tests
 *
 * Each CTest invocation runs one case in a fresh process.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "../external/basic_agent/include/config.h"
#include "../external/basic_agent/include/executive_controller.h"
#include "../external/basic_agent/include/fixed_evidence_containment.h"
#include "../external/basic_agent/include/fixed_evidence_goal_run.h"
#include "../external/basic_agent/include/iplanner.h"
#include "../external/basic_agent/include/memory.h"
#include "../external/basic_agent/include/plan.h"
#include "../external/basic_agent/include/tools.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

std::atomic<int> g_probe_calls{0};
std::atomic<int> g_plans_created{0};

class ProbeTool : public ITool {
public:
    ProbeTool(std::string name, bool confirm)
        : name_(std::move(name)), confirm_(confirm) {}
    std::string name() const override { return name_; }
    std::string description() const override { return "probe"; }
    nlohmann::json input_schema() const override {
        return {{"type", "object"}, {"additionalProperties", true}};
    }
    bool requires_confirmation() const override { return confirm_; }
    nlohmann::json execute(const nlohmann::json&) const override {
        g_probe_calls.fetch_add(1);
        return {{"status", "success"}};
    }
private:
    std::string name_;
    bool confirm_;
};

PlanStep toolStep() {
    PlanStep step;
    step.step_id = "goal-tool";
    step.description = "tool step";
    step.type = StepType::TOOL;
    step.payload = {{"tool", "web_scrape"}};
    step.failure_policy.max_retries = 0;
    step.failure_policy.revise_plan_on_failure = false;
    return step;
}

class ToolGoalPlanner : public IPlanner {
public:
    Plan create_plan(const std::string& goal) override {
        Plan plan;
        plan.plan_id = "goal-path-plan";
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;
        plan.steps.push_back(toolStep());
        return plan;
    }
    Plan revise_plan(const Plan& existing_plan, const nlohmann::json&) override {
        return existing_plan;
    }
};

void fail(const std::string& message) {
    std::cerr << message << "\n";
}

struct Harness {
    std::filesystem::path dir;
    Config config;
    std::shared_ptr<Memory> memory;
    std::shared_ptr<ToolRegistry> registry;
    std::shared_ptr<ToolGoalPlanner> planner;
    std::unique_ptr<Thoth::ExecutiveController> controller;

    explicit Harness(const std::string& label) {
        dir = std::filesystem::temp_directory_path() / "thoth_lx_goal_file" / label;
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        setenv("THOTH_WORKSPACE_PATH", dir.c_str(), 1);
        setenv("THOTH_LOGS_PATH", (dir / "logs").c_str(), 1);
        config.database_path = (dir / "memory.db").string();
        memory = std::make_shared<Memory>(config);
        registry = std::make_shared<ToolRegistry>();
        registry->registerTool(std::make_unique<ProbeTool>("web_scrape", false));
        planner = std::make_shared<ToolGoalPlanner>();
        controller = std::make_unique<Thoth::ExecutiveController>(planner, registry, nullptr, memory);
        controller->set_max_reflections(0);
        controller->set_event_callback([](const ControllerEvent& event) {
            if (event.type == EventType::PLAN_CREATED) {
                g_plans_created.fetch_add(1);
            }
        });
    }

    ~Harness() {
        controller.reset();
        std::filesystem::remove_all(dir);
    }
};

bool writeFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << text;
    return out.good();
}

bool traceHas(const std::filesystem::path& dir, const std::string& needle) {
    std::ifstream in(dir / "decision_trace.jsonl");
    if (!in) {
        return false;
    }
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return body.find(needle) != std::string::npos;
}

bool testContained() {
    if (Thoth::FixedEvidenceContainment::enabled()) {
        fail("contained: flag started enabled");
        return false;
    }
    Harness harness("contained");
    g_probe_calls.store(0);
    const auto goal_path = harness.dir / "goal.txt";
    const std::string marker = "EGAR-LX-ONE-SHOT-MARKER";
    if (!writeFile(goal_path, marker + "\nProblem:\ncase text\n")) {
        fail("contained: could not write goal file");
        return false;
    }
    std::string error;
    std::string state;
    const bool ok = Thoth::runFixedEvidenceGoalFile(
        *harness.controller, goal_path.string(), harness.dir.string(), error, state);
    const bool recorded = traceHas(harness.dir, "STEP_FAILED")
        && traceHas(harness.dir, "fixed_evidence_containment")
        && traceHas(harness.dir, marker);
    if (!ok || state != "FAILED" || !Thoth::FixedEvidenceContainment::enabled()
        || g_probe_calls.load() != 0 || !recorded) {
        fail("contained: ok=" + std::to_string(ok) + " state=" + state + " error=" + error
             + " calls=" + std::to_string(g_probe_calls.load())
             + " trace=" + std::to_string(recorded));
        return false;
    }
    return true;
}

bool testOneGoal() {
    if (Thoth::FixedEvidenceContainment::enabled()) {
        fail("one_goal: flag started enabled");
        return false;
    }
    Harness harness("one_goal");
    g_probe_calls.store(0);
    g_plans_created.store(0);
    std::string error;
    std::string state;
    if (Thoth::runFixedEvidenceGoal(*harness.controller, " \n\t", harness.dir.string(), error, state)
        || error != "goal text is empty"
        || Thoth::FixedEvidenceContainment::enabled()
        || g_plans_created.load() != 0) {
        fail("one_goal: empty text consumed the slot: " + error);
        return false;
    }

    const auto goal_path = harness.dir / "goal.txt";
    if (!writeFile(goal_path, "one goal per process\n")) {
        fail("one_goal: could not write goal file");
        return false;
    }
    if (!Thoth::runFixedEvidenceGoalFile(
            *harness.controller, goal_path.string(), harness.dir.string(), error, state)
        || state != "FAILED"
        || g_plans_created.load() != 1
        || g_probe_calls.load() != 0) {
        fail("one_goal: first run state=" + state + " error=" + error
             + " plans=" + std::to_string(g_plans_created.load())
             + " calls=" + std::to_string(g_probe_calls.load()));
        return false;
    }

    const int plans_after_first = g_plans_created.load();
    std::string second_error;
    std::string second_state;
    const bool second = Thoth::runFixedEvidenceGoalFile(
        *harness.controller, goal_path.string(), harness.dir.string(), second_error, second_state);
    if (second || second_error != "one fixed-evidence goal per process"
        || g_plans_created.load() != plans_after_first
        || g_probe_calls.load() != 0) {
        fail("one_goal: second run started another goal: " + second_error
             + " plans=" + std::to_string(g_plans_created.load()));
        return false;
    }
    return true;
}

struct ChildResult {
    int exit_code = -1;
    std::string output;
    bool timed_out = false;
};

ChildResult runEngine(const std::string& binary,
                      const std::vector<std::string>& args,
                      const std::filesystem::path& workspace) {
    ChildResult result;
    int pipes[2];
    if (pipe(pipes) != 0) {
        result.output = "pipe failed";
        return result;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipes[0]);
        close(pipes[1]);
        result.output = "fork failed";
        return result;
    }
    if (pid == 0) {
        setenv("THOTH_WORKSPACE_PATH", workspace.c_str(), 1);
        setenv("THOTH_LOGS_PATH", (workspace / "logs").string().c_str(), 1);
        unsetenv("THOTH_LOG_CONFIG");
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(binary.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execv(binary.c_str(), argv.data());
        _exit(127);
    }

    close(pipes[1]);
    const int flags = fcntl(pipes[0], F_GETFL, 0);
    fcntl(pipes[0], F_SETFL, flags | O_NONBLOCK);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    bool exited = false;
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd fd {};
        fd.fd = pipes[0];
        fd.events = POLLIN;
        poll(&fd, 1, 100);
        char buf[1024];
        while (true) {
            const ssize_t n = read(pipes[0], buf, sizeof(buf));
            if (n > 0) {
                result.output.append(buf, static_cast<std::size_t>(n));
            } else {
                break;
            }
        }
        const pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) {
            exited = true;
            break;
        }
    }
    if (!exited) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        result.timed_out = true;
    }
    char buf[1024];
    while (true) {
        const ssize_t n = read(pipes[0], buf, sizeof(buf));
        if (n > 0) {
            result.output.append(buf, static_cast<std::size_t>(n));
        } else {
            break;
        }
    }
    close(pipes[0]);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }
    return result;
}

bool expectReject(const std::string& binary,
                  const std::vector<std::string>& args,
                  const std::filesystem::path& workspace,
                  const std::string& needle,
                  const std::string& label) {
    const ChildResult child = runEngine(binary, args, workspace);
    const bool started = child.output.find("BasicAgentPlugin") != std::string::npos
        || child.output.find("GOAL ACCEPTED") != std::string::npos
        || child.output.find("FIXED_EVIDENCE_GOAL") != std::string::npos;
    if (child.timed_out || child.exit_code == 0 || started
        || child.output.find(needle) == std::string::npos) {
        fail(label + ": exit=" + std::to_string(child.exit_code)
             + " timeout=" + std::to_string(child.timed_out) + "\n" + child.output);
        return false;
    }
    return true;
}

bool testInvalid(const std::string& binary) {
    if (binary.empty() || !std::filesystem::is_regular_file(binary)) {
        fail("invalid: engine binary missing");
        return false;
    }
    const auto dir = std::filesystem::temp_directory_path() / "thoth_lx_goal_file" / "invalid";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    std::string goal;
    std::string error;
    const auto missing = (dir / "missing.txt").string();
    if (Thoth::readFixedEvidenceGoalFile(missing, goal, error)
        || error.find("goal file not found") == std::string::npos) {
        fail("invalid: missing file: " + error);
        std::filesystem::remove_all(dir);
        return false;
    }
    const auto empty_path = dir / "empty.txt";
    writeFile(empty_path, "");
    if (Thoth::readFixedEvidenceGoalFile(empty_path.string(), goal, error)
        || error.find("goal file is empty") == std::string::npos) {
        fail("invalid: empty file: " + error);
        std::filesystem::remove_all(dir);
        return false;
    }
    writeFile(dir / "blank.txt", " \n\t\r");
    if (Thoth::readFixedEvidenceGoalFile((dir / "blank.txt").string(), goal, error)
        || error.find("goal file is empty") == std::string::npos) {
        fail("invalid: blank file: " + error);
        std::filesystem::remove_all(dir);
        return false;
    }
    std::string nul_text = "goal";
    nul_text.push_back('\0');
    writeFile(dir / "nul.txt", nul_text);
    if (Thoth::readFixedEvidenceGoalFile((dir / "nul.txt").string(), goal, error)
        || error.find("not valid text") == std::string::npos) {
        fail("invalid: nul file: " + error);
        std::filesystem::remove_all(dir);
        return false;
    }
    if (Thoth::readFixedEvidenceGoalFile(dir.string(), goal, error)
        || error.find("not a file") == std::string::npos) {
        fail("invalid: directory: " + error);
        std::filesystem::remove_all(dir);
        return false;
    }
    if (Thoth::FixedEvidenceContainment::enabled()) {
        fail("invalid: containment enabled without a goal");
        std::filesystem::remove_all(dir);
        return false;
    }

    const std::string goal_file = empty_path.string();
    const bool cli_ok =
        expectReject(binary, {"--fixed-evidence-goal", missing}, dir, "goal file not found", "cli-missing")
        && expectReject(binary, {"--fixed-evidence-goal", goal_file}, dir, "goal file is empty", "cli-empty")
        && expectReject(binary, {"--fixed-evidence-goal", dir.string()}, dir, "not a file", "cli-dir")
        && expectReject(binary, {"--fixed-evidence-goal"}, dir, "requires a file path", "cli-no-path")
        && expectReject(binary, {"--serve", "--fixed-evidence-goal", goal_file}, dir,
                        "cannot be combined", "cli-serve")
        && expectReject(binary, {"--execute", "hello", "--fixed-evidence-goal", goal_file}, dir,
                        "cannot be combined", "cli-execute")
        && expectReject(binary, {"--fixed-evidence-goal", goal_file, "--fixed-evidence-goal", goal_file}, dir,
                        "one goal file per process", "cli-two")
        && expectReject(binary, {"--execute"}, dir, "requires a prompt argument", "cli-execute-usage");
    writeFile(dir / "ready.txt", "diagnose the case\n");
    const std::string ready = (dir / "ready.txt").string();
    const bool evidence_ok =
        expectReject(binary, {"--fixed-evidence-goal", ready}, dir,
                     "requires --evidence-directory", "cli-no-evidence")
        && expectReject(binary, {"--fixed-evidence-goal", ready, "--evidence-directory",
                                 (dir / "missing-evidence").string()},
                        dir, "evidence directory not found", "cli-missing-evidence")
        && expectReject(binary, {"--fixed-evidence-goal", ready, "--evidence-directory", ready},
                        dir, "not a directory", "cli-evidence-file")
        && expectReject(binary, {"--evidence-directory", dir.string()}, dir,
                        "requires --fixed-evidence-goal", "cli-evidence-only");
    const ChildResult help = runEngine(binary, {"--help"}, dir);
    const bool help_ok = !help.timed_out && help.exit_code == 0
        && help.output.find("--fixed-evidence-goal") != std::string::npos
        && help.output.find("--evidence-directory") != std::string::npos
        && help.output.find("BasicAgentPlugin") == std::string::npos;
    if (!help_ok) {
        fail("invalid: help exit=" + std::to_string(help.exit_code) + "\n" + help.output);
    }
    std::filesystem::remove_all(dir);
    return cli_ok && evidence_ok && help_ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: thoth-fixed-evidence-goal-tests <contained|invalid|one_goal> [engine]\n";
        return 2;
    }
    const std::string id = argv[1];
    bool ok = false;
    if (id == "contained") ok = testContained();
    else if (id == "one_goal") ok = testOneGoal();
    else if (id == "invalid") ok = testInvalid(argc >= 3 ? argv[2] : "");
    else {
        std::cerr << "unknown case: " << id << "\n";
        return 2;
    }
    return ok ? 0 : 1;
}
