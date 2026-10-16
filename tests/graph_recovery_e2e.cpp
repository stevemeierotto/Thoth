/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — disposable GR-1..GR-5 process campaign.
 * Not part of CTest. Does not start inference servers.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "config.h"
#include "embedding_engine.h"
#include "executive_controller.h"
#include "chunkers/code_chunk.h"
#include "index_manager.h"
#include "iplanner.h"
#include "memory.h"
#include "plan.h"
#include "rag.h"
#include "sqlite_memory_repository.h"
#include "tools.h"

#include <json.hpp>
#include <sqlite3.h>

#include <sys/signalfd.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr const char* kAlpha = "alpha zebra quartz unique marker";
constexpr const char* kBeta = "beta walrus marble unique marker";
constexpr const char* kSession = "gr-session";
constexpr float kReward = 0.1f + 0.2f * (1.0f - 0.1f);
constexpr float kPenalty = 0.1f - 0.2f * 0.1f;
constexpr float kDecay = 0.995f;
constexpr int kPhaseMs = 60 * 1000;

enum class WaitKind { Stopped, Exited, TimedOut, Error };

struct ChildProcess {
    pid_t pid = -1;
    pid_t pgid = -1;
    bool group_ready = false;
};

struct DbView {
    bool opened = false;
    bool active = false;
    std::string plan_id;
    int status = -1;
    bool graph_reinforced = false;
    bool history_aging_done = false;
    bool reflection_pending = false;
    int reflection_count = 0;
    bool retrieval_sets_present = false;
    std::vector<std::vector<std::string>> retrieval_sets;
    int past_plans = 0;
    int trajectories = 0;
    int success_count = -1;
    int failure_count = -1;
    float weight = -1.f;
    std::string edge_to;
    int edge_rows = 0;
};

std::string parentSha() {
    const std::string command = std::string("git -C ") + THOTH_SOURCE_DIR + " rev-parse HEAD";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {};
    char buffer[128];
    std::string sha;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) sha += buffer;
    pclose(pipe);
    while (!sha.empty() && (sha.back() == '\n' || sha.back() == '\r')) sha.pop_back();
    return sha;
}

bool near(float actual, float expected) {
    return std::fabs(actual - expected) <= 1e-4f;
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void writeAtomic(const fs::path& path, const std::string& body) {
    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        out << body;
    }
    std::error_code error;
    fs::rename(temporary, path, error);
    if (error) fs::copy_file(temporary, path, fs::copy_options::overwrite_existing);
}

int countLines(const fs::path& path, const std::string& needle) {
    int count = 0;
    std::istringstream lines(readFile(path));
    std::string line;
    while (std::getline(lines, line)) {
        if (needle.empty() || line.find(needle) != std::string::npos) ++count;
    }
    return count;
}

bool disposablePath(const fs::path& path, std::string& error) {
    const std::string text = path.lexically_normal().string();
    if (text.rfind("/tmp/thoth-e2e-graph-", 0) != 0 || text.find("..") != std::string::npos) {
        error = "path is outside /tmp/thoth-e2e-graph-<sha>";
        return false;
    }
    if (text.find("agent_workspace") != std::string::npos) {
        error = "path names a development workspace";
        return false;
    }
    return true;
}

std::string statusText(int status) {
    if (WIFEXITED(status)) return "exit:" + std::to_string(WEXITSTATUS(status));
    if (WIFSIGNALED(status)) {
        return std::string("signal:") + strsignal(WTERMSIG(status)) + " (" +
               std::to_string(WTERMSIG(status)) + ")";
    }
    if (WIFSTOPPED(status)) {
        return std::string("stopped:") + strsignal(WSTOPSIG(status));
    }
    return "uncollected";
}

DbView readDb(const fs::path& dbPath, const std::string& fromHash) {
    DbView view;
    if (!fs::exists(dbPath)) return view;
    Thoth::SQLiteMemoryRepository repo(dbPath.string());
    view.opened = true;
    view.past_plans = static_cast<int>(repo.getAllPastPlans(2).size());
    view.trajectories = static_cast<int>(repo.getAllTrajectories().size());
    const auto edges = repo.getEdgesFrom(fromHash);
    view.edge_rows = static_cast<int>(edges.size());
    if (!edges.empty()) {
        view.edge_to = edges.front().to_id;
        view.success_count = edges.front().success_count;
        view.failure_count = edges.front().failure_count;
        view.weight = edges.front().weight;
    }
    if (const auto active = repo.getActivePlan(kSession)) {
        view.active = true;
        view.plan_id = active->plan_id;
        try {
            const auto plan = nlohmann::json::parse(active->steps_json);
            view.status = plan.value("status", -1);
            view.graph_reinforced = plan.value("graph_reinforced", false);
            view.history_aging_done = plan.value("history_aging_done", false);
            view.reflection_pending = plan.value("reflection_pending", false);
            view.reflection_count = plan.value("reflection_count", 0);
            view.retrieval_sets_present = plan.contains("retrieval_sets");
            if (plan.contains("retrieval_sets") && plan["retrieval_sets"].is_array()) {
                for (const auto& set : plan["retrieval_sets"]) {
                    std::vector<std::string> hashes;
                    for (const auto& hash : set.value("chunk_hashes", nlohmann::json::array())) {
                        hashes.push_back(hash.get<std::string>());
                    }
                    view.retrieval_sets.push_back(std::move(hashes));
                }
            }
        } catch (...) {
        }
    }
    return view;
}

nlohmann::json viewJson(const DbView& view) {
    nlohmann::json sets = nlohmann::json::array();
    for (const auto& set : view.retrieval_sets) {
        sets.push_back(set);
    }
    return {
        {"opened", view.opened},
        {"active_plan", view.active},
        {"plan_id", view.plan_id},
        {"plan_status", view.status},
        {"graph_reinforced", view.graph_reinforced},
        {"history_aging_done", view.history_aging_done},
        {"reflection_pending", view.reflection_pending},
        {"reflection_count", view.reflection_count},
        {"retrieval_sets_present", view.retrieval_sets_present},
        {"retrieval_sets", sets},
        {"past_plans", view.past_plans},
        {"trajectories", view.trajectories},
        {"edge_rows_from_alpha", view.edge_rows},
        {"edge_to", view.edge_to},
        {"success_count", view.success_count},
        {"failure_count", view.failure_count},
        {"weight", view.weight}
    };
}

class CampaignPlanner : public IPlanner {
public:
    std::string planId;
    bool failAfterFirst = false;
    bool failAfterTwo = false;
    bool llmOnly = false;
    bool penalizedRevision = false;
    bool separateReflection = false;
    fs::path logPath;
    std::atomic<int> creates{0};
    std::atomic<int> revises{0};

    explicit CampaignPlanner(std::string id, fs::path log)
        : planId(std::move(id)), logPath(std::move(log)) {}

    void note(const std::string& kind) {
        std::ofstream out(logPath, std::ios::app);
        if (out) out << kind << "\n";
    }

    PlanStep retrieval(const std::string& stepId, const char* query) {
        PlanStep step;
        step.step_id = stepId;
        step.description = "retrieve";
        step.type = StepType::RETRIEVAL;
        step.status = StepStatus::PENDING;
        step.payload = {{"query", query}, {"top_k", 1}};
        step.failure_policy.max_retries = 0;
        return step;
    }

    PlanStep llm(const std::string& stepId) {
        PlanStep step;
        step.step_id = stepId;
        step.description = "synthesize";
        step.type = StepType::LLM;
        step.status = StepStatus::PENDING;
        step.payload = {{"prompt", "test"}};
        step.failure_policy.max_retries = 0;
        return step;
    }

    PlanStep failingNode(const std::string& stepId, bool revise) {
        PlanStep step;
        step.step_id = stepId;
        step.description = "fail closed";
        step.type = StepType::NODE;
        step.status = StepStatus::PENDING;
        step.failure_policy.max_retries = 0;
        step.failure_policy.revise_plan_on_failure = revise;
        return step;
    }

    Plan create_plan(const std::string& goal) override {
        ++creates;
        note("create");
        Plan plan;
        plan.plan_id = planId;
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;
        if (separateReflection && creates.load() > 1) {
            plan.plan_id = planId + "-replacement";
            auto first = retrieval(plan.plan_id + "-alpha", kAlpha);
            auto second = retrieval(plan.plan_id + "-beta", kBeta);
            second.depends_on = {first.step_id};
            auto synth = llm(plan.plan_id + "-llm");
            synth.depends_on = {second.step_id};
            plan.steps = {first, second, synth};
            return plan;
        }
        if (llmOnly) {
            plan.steps = {llm(planId + "-llm")};
            return plan;
        }
        if (penalizedRevision || separateReflection) {
            auto first = retrieval(planId + "-alpha", kAlpha);
            auto fail = failingNode(planId + "-fail", penalizedRevision);
            fail.depends_on = {first.step_id};
            plan.steps = {first, fail};
            return plan;
        }
        if (failAfterFirst) {
            auto first = retrieval(planId + "-alpha", kAlpha);
            auto fail = failingNode(planId + "-fail", true);
            fail.depends_on = {first.step_id};
            plan.steps = {first, fail};
            return plan;
        }
        auto first = retrieval(planId + "-alpha", kAlpha);
        auto second = retrieval(planId + "-beta", kBeta);
        second.depends_on = {first.step_id};
        if (failAfterTwo) {
            auto fail = failingNode(planId + "-fail", false);
            fail.depends_on = {second.step_id};
            plan.steps = {first, second, fail};
            return plan;
        }
        auto synth = llm(planId + "-llm");
        synth.depends_on = {second.step_id};
        plan.steps = {first, second, synth};
        return plan;
    }

    Plan revise_plan(const Plan& existing, const nlohmann::json&) override {
        ++revises;
        note("revise");
        Plan revised = existing;
        revised.plan_id = existing.plan_id;
        revised.status = PlanStatus::ACTIVE;
        revised.steps.clear();
        for (const auto& step : existing.steps) {
            if (step.status == StepStatus::SUCCESS) revised.steps.push_back(step);
        }
        if (penalizedRevision && revises.load() == 1) {
            auto second = retrieval(planId + "-beta", kBeta);
            if (!revised.steps.empty()) second.depends_on = {revised.steps.back().step_id};
            auto fail = failingNode(planId + "-fail-2", true);
            fail.depends_on = {second.step_id};
            revised.steps.push_back(second);
            revised.steps.push_back(fail);
            return revised;
        }
        if (penalizedRevision) {
            auto synth = llm(planId + "-llm");
            if (!revised.steps.empty()) synth.depends_on = {revised.steps.back().step_id};
            revised.steps.push_back(synth);
            return revised;
        }
        auto second = retrieval(planId + "-beta", kBeta);
        if (!revised.steps.empty()) second.depends_on = {revised.steps.back().step_id};
        auto synth = llm(planId + "-llm");
        synth.depends_on = {second.step_id};
        revised.steps.push_back(second);
        revised.steps.push_back(synth);
        return revised;
    }
};

void seedChunks(RAGPipeline& rag, IndexManager& index, const fs::path& workspace) {
    const char* texts[] = {kAlpha, kBeta};
    const char* files[] = {"alpha.txt", "beta.txt"};
    const char* symbols[] = {"alpha", "beta"};
    for (const char* text : texts) rag.engine->updateVocabulary(text);
    index.setSessionId(kSession);
    for (int i = 0; i < 2; ++i) {
        const fs::path path = fs::absolute(workspace / files[i]).lexically_normal();
        index.registerAttachmentOwner(path.string(), kSession);
        CodeChunk chunk;
        chunk.fileName = path.string();
        chunk.symbolName = symbols[i];
        chunk.code = texts[i];
        chunk.embedding = rag.engine->embed(texts[i]);
        index.addChunkToIndex(std::move(chunk));
    }
}

bool waitTerminal(Thoth::ExecutiveController& controller) {
    for (int spin = 0; spin < kPhaseMs / 50; ++spin) {
        const auto state = controller.get_state();
        if (state == Thoth::ControllerState::COMPLETED ||
            state == Thoth::ControllerState::FAILED ||
            state == Thoth::ControllerState::ABORTED) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

void bindWorkspace(const fs::path& workspace) {
    setenv("THOTH_WORKSPACE_PATH", workspace.c_str(), 1);
    setenv("THOTH_LOGS_PATH", workspace.c_str(), 1);
    setenv("THOTH_MOCK_LLM", "1", 1);
    unsetenv("THOTH_HALT_AFTER_GRAPH_COMMIT");
    unsetenv("THOTH_HALT_AFTER_AGING_COMMIT");
    unsetenv("THOTH_HALT_AFTER_TRAJECTORY_COMMIT");
    unsetenv("THOTH_HALT_BEFORE_HISTORY_WRITE");
    unsetenv("THOTH_HALT_AFTER_PAST_PLAN");
    unsetenv("THOTH_GRAPH_HALT_DIR");
}

int runChild(const std::string& caseName, const fs::path& workspace) {
    std::string error;
    if (!disposablePath(workspace, error)) {
        std::cerr << error << "\n";
        return 2;
    }
    fs::create_directories(workspace);
    bindWorkspace(workspace);
    if (caseName == "gr-3" || caseName == "gr-4") {
        setenv("THOTH_HALT_AFTER_GRAPH_COMMIT", "1", 1);
        setenv("THOTH_GRAPH_HALT_DIR", workspace.c_str(), 1);
    } else if (caseName == "gr-5") {
        setenv("THOTH_HALT_AFTER_AGING_COMMIT", "1", 1);
        setenv("THOTH_GRAPH_HALT_DIR", workspace.c_str(), 1);
    } else if (caseName == "gr-6" || caseName == "gr-7" || caseName == "gr-8") {
        setenv("THOTH_HALT_AFTER_TRAJECTORY_COMMIT", "1", 1);
        setenv("THOTH_GRAPH_HALT_DIR", workspace.c_str(), 1);
    } else if (caseName == "gr-11") {
        setenv("THOTH_HALT_BEFORE_HISTORY_WRITE", "1", 1);
        setenv("THOTH_GRAPH_HALT_DIR", workspace.c_str(), 1);
    } else if (caseName == "gr-12") {
        setenv("THOTH_HALT_AFTER_PAST_PLAN", "1", 1);
        setenv("THOTH_GRAPH_HALT_DIR", workspace.c_str(), 1);
    }

    auto planner = std::make_shared<CampaignPlanner>(caseName + "-plan", workspace / "planner_log.txt");
    planner->failAfterFirst = caseName == "gr-2";
    planner->failAfterTwo = caseName == "gr-4";
    planner->penalizedRevision = caseName == "gr-7" || caseName == "gr-11" || caseName == "gr-12";
    planner->separateReflection = caseName == "gr-9";
    Config config;
    config.enable_retrieval_prefetch = false;
    config.max_parallel_retrieval = 1;
    config.database_path = (workspace / "memory.db").string();
    auto memory = std::make_shared<Memory>(config);
    memory->setActiveSessionId(kSession);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto index = std::make_unique<IndexManager>(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get());
    seedChunks(*rag, *index, workspace);
    auto tools = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, tools, rag, memory);
    controller.set_config(&config);
    controller.set_session_id(kSession);
    controller.set_max_reflections(caseName == "gr-4" || caseName == "gr-9" ? 1 : 0);
    if (caseName == "gr-2" || caseName == "gr-13") {
        if (auto* repo = memory->getSQLiteRepo()) {
            const fs::path marker = workspace / "graph_halt.json";
            repo->setActivePlanCommitObserverForTests([marker](const auto& plan) {
                try {
                    const auto stored = nlohmann::json::parse(plan.steps_json);
                    if (!stored.contains("retrieval_sets") || !stored["retrieval_sets"].is_array() ||
                        stored["retrieval_sets"].size() != 1) {
                        return;
                    }
                } catch (...) {
                    return;
                }
                nlohmann::json body = {{"boundary", "after_first_retrieval_set"},
                                       {"pid", static_cast<int>(::getpid())},
                                       {"plan_id", plan.plan_id}};
                writeAtomic(marker, body.dump(2));
                ::raise(SIGSTOP);
            });
        }
    }
    controller.execute_goal("summarize the retrieved notes");
    const bool halt = caseName == "gr-2" || caseName == "gr-3" || caseName == "gr-4" ||
                      caseName == "gr-5" || caseName == "gr-6" || caseName == "gr-7" ||
                      caseName == "gr-8" || caseName == "gr-11" || caseName == "gr-12" ||
                      caseName == "gr-13";
    if (halt) {
        for (;;) ::pause();
    }
    if (!waitTerminal(controller)) return 3;
    nlohmann::json result = {
        {"state", static_cast<int>(controller.get_state())},
        {"plan_id", controller.get_current_plan().plan_id},
        {"creates", planner->creates.load()},
        {"revises", planner->revises.load()}
    };
    writeAtomic(workspace / "child_result.json", result.dump(2));
    return controller.get_state() == Thoth::ControllerState::COMPLETED ? 0 : 4;
}

int resumeChild(const std::string& caseName, const fs::path& workspace) {
    std::string error;
    if (!disposablePath(workspace, error)) {
        std::cerr << error << "\n";
        return 2;
    }
    bindWorkspace(workspace);
    auto planner = std::make_shared<CampaignPlanner>(caseName + "-plan", workspace / "planner_log.txt");
    planner->failAfterFirst = caseName == "gr-2";
    planner->llmOnly = caseName == "gr-4";
    planner->penalizedRevision = caseName == "gr-7" || caseName == "gr-11" || caseName == "gr-12";
    planner->separateReflection = caseName == "gr-9";
    Config config;
    config.enable_retrieval_prefetch = false;
    config.max_parallel_retrieval = 1;
    auto memory = std::make_shared<Memory>(config);
    memory->setActiveSessionId(kSession);
    auto loaded = memory->getActivePlan(kSession);
    if (!loaded) {
        std::cerr << "resume found no active plan\n";
        return 5;
    }
    Plan plan = Plan::from_json(nlohmann::json::parse(loaded->steps_json));
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto index = std::make_unique<IndexManager>(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get());
    seedChunks(*rag, *index, workspace);
    auto tools = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, tools, rag, memory);
    controller.set_config(&config);
    controller.set_session_id(kSession);
    controller.set_max_reflections(caseName == "gr-4" || caseName == "gr-9" ? 1 : 0);
    std::vector<std::string> startedSteps;
    std::mutex startedMu;
    controller.set_event_callback([&](const ControllerEvent& event) {
        if (event.type != EventType::STEP_STARTED) return;
        std::lock_guard<std::mutex> lock(startedMu);
        startedSteps.push_back(event.step_id);
    });
    controller.resume_from_plan(plan);
    if (!waitTerminal(controller) &&
        controller.get_state() != Thoth::ControllerState::COMPLETED &&
        controller.get_state() != Thoth::ControllerState::FAILED) {
        return 3;
    }
    nlohmann::json result = {
        {"state", static_cast<int>(controller.get_state())},
        {"plan_id", controller.get_current_plan().plan_id},
        {"creates", planner->creates.load()},
        {"revises", planner->revises.load()},
        {"reflection_count", controller.get_reflection_count()},
        {"step_starts", startedSteps}
    };
    writeAtomic(workspace / "resume_result.json", result.dump(2));
    return controller.get_state() == Thoth::ControllerState::COMPLETED ? 0 : 4;
}

std::string exePath() {
    char buffer[4096];
    const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n <= 0) return {};
    buffer[n] = '\0';
    return buffer;
}

class SigchldBlock {
public:
    SigchldBlock() {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        sigprocmask(SIG_BLOCK, &set, &previous_);
    }
    ~SigchldBlock() { sigprocmask(SIG_SETMASK, &previous_, nullptr); }

private:
    sigset_t previous_{};
};

ChildProcess spawnGroup(const std::vector<std::string>& args) {
    ChildProcess child;
    const pid_t pid = fork();
    if (pid < 0) return child;
    if (pid == 0) {
        if (setpgid(0, 0) != 0) _exit(127);
        std::vector<char*> argv;
        for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        execv(argv[0], argv.data());
        _exit(127);
    }
    if (setpgid(pid, pid) != 0 && getpgid(pid) != pid) {
        child.pid = pid;
        child.pgid = getpgid(pid);
        return child;
    }
    child.pid = pid;
    child.pgid = getpgid(pid);
    child.group_ready = child.pgid == pid && child.pgid > 1 && child.pgid != getpgrp();
    return child;
}

WaitKind waitChild(pid_t pid, int timeout_ms, int& status) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    const int fd = signalfd(-1, &set, SFD_CLOEXEC);
    if (fd < 0) return WaitKind::Error;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
                                deadline - std::chrono::steady_clock::now())
                                .count();
        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        const int polled = poll(&pfd, 1, static_cast<int>(std::max<int64_t>(remain, 0)));
        if (polled < 0) {
            close(fd);
            return WaitKind::Error;
        }
        if (polled > 0) {
            signalfd_siginfo info{};
            const ssize_t got = read(fd, &info, sizeof(info));
            if (got < 0) {
                close(fd);
                return WaitKind::Error;
            }
        }
        const pid_t reaped = waitpid(pid, &status, WUNTRACED | WNOHANG);
        if (reaped == pid && WIFSTOPPED(status)) {
            close(fd);
            return WaitKind::Stopped;
        }
        if (reaped == pid && (WIFEXITED(status) || WIFSIGNALED(status))) {
            close(fd);
            return WaitKind::Exited;
        }
        if (polled == 0) {
            close(fd);
            return WaitKind::TimedOut;
        }
    }
    close(fd);
    return WaitKind::TimedOut;
}

bool killGroup(const ChildProcess& child, int& status, std::string& error) {
    if (!child.group_ready) {
        error = "process group was not established";
        return false;
    }
    if (kill(-child.pgid, SIGKILL) != 0) {
        error = std::string("SIGKILL failed: ") + std::strerror(errno);
        return false;
    }
    const pid_t reaped = waitpid(child.pid, &status, 0);
    if (reaped != child.pid) {
        error = "failed to reap killed child";
        return false;
    }
    return true;
}

nlohmann::json readHistory(const fs::path& dbPath) {
    nlohmann::json history = {
        {"trajectories", nlohmann::json::array()},
        {"past_plans", nlohmann::json::array()},
        {"cognate_plans", nlohmann::json::array()},
        {"active_steps", nlohmann::json::array()}
    };
    if (!fs::exists(dbPath)) return history;
    Thoth::SQLiteMemoryRepository repo(dbPath.string());
    for (const auto& traj : repo.getAllTrajectories()) {
        nlohmann::json steps = nlohmann::json::array();
        try {
            const auto body = nlohmann::json::parse(traj.trajectory_json);
            if (body.contains("steps") && body["steps"].is_array()) steps = body["steps"];
        } catch (...) {
        }
        history["trajectories"].push_back({
            {"trajectory_id", traj.trajectory_id},
            {"success_score", traj.success_score},
            {"usage_count", traj.usage_count},
            {"tier", traj.tier},
            {"step_count", steps.size()},
            {"steps", steps}
        });
    }
    for (const auto& plan : repo.getAllPastPlans(2)) {
        history["past_plans"].push_back({
            {"plan_id", plan.plan_id},
            {"success_score", plan.success_score}
        });
    }
    for (const auto& plan : repo.getAllCognatePlans()) {
        history["cognate_plans"].push_back({
            {"plan_id", plan.plan_id},
            {"success_score", plan.success_score},
            {"status", plan.status}
        });
    }
    if (const auto active = repo.getActivePlan(kSession)) {
        try {
            const auto plan = nlohmann::json::parse(active->steps_json);
            if (plan.contains("steps") && plan["steps"].is_array()) {
                for (const auto& step : plan["steps"]) {
                    history["active_steps"].push_back({
                        {"step_id", step.value("step_id", "")},
                        {"status", step.value("status", -1)}
                    });
                }
            }
        } catch (...) {
        }
    }
    return history;
}

int countEpisodeSteps(const fs::path& dbPath) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return -1;
    }
    sqlite3_stmt* stmt = nullptr;
    int count = -1;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM episode_steps;", -1, &stmt, nullptr) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

nlohmann::json readWorkingSnapshot(const fs::path& dbPath) {
    nlohmann::json snapshot = {{"present", false}, {"revisions_count", 0}, {"step_count", 0},
                               {"error_count", 0}, {"initial_step_count", 0}, {"trajectory_id", ""}};
    if (!fs::exists(dbPath)) return snapshot;
    Thoth::SQLiteMemoryRepository repo(dbPath.string());
    const auto active = repo.getActivePlan(kSession);
    if (!active) return snapshot;
    try {
        const auto plan = nlohmann::json::parse(active->steps_json);
        if (!plan.contains("working_trajectory") || !plan["working_trajectory"].is_object()) {
            return snapshot;
        }
        const auto& body = plan["working_trajectory"];
        int errors = 0;
        int steps = 0;
        if (body.contains("steps") && body["steps"].is_array()) {
            steps = static_cast<int>(body["steps"].size());
            for (const auto& step : body["steps"]) {
                if (!step.value("error", "").empty()) ++errors;
            }
        }
        int initialSteps = 0;
        if (body.contains("plan_initial") && body["plan_initial"].is_object() &&
            body["plan_initial"].contains("steps") && body["plan_initial"]["steps"].is_array()) {
            initialSteps = static_cast<int>(body["plan_initial"]["steps"].size());
        }
        snapshot = {{"present", true},
                    {"revisions_count", plan.value("working_revisions_count", 0)},
                    {"step_count", steps},
                    {"error_count", errors},
                    {"initial_step_count", initialSteps},
                    {"trajectory_id", body.value("trajectory_id", "")},
                    {"goal", body.value("goal", "")}};
    } catch (...) {
    }
    return snapshot;
}

bool setTrajectoryMetadata(const fs::path& dbPath, const std::string& trajectoryId, int usage,
                           int tier, int64_t createdAt, std::string& error) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        error = "could not open the trajectory database";
        if (db) sqlite3_close(db);
        return false;
    }
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE trajectories SET usage_count = ?, tier = ?, created_at = ? "
                      "WHERE trajectory_id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_int(stmt, 1, usage);
    sqlite3_bind_int(stmt, 2, tier);
    sqlite3_bind_int64(stmt, 3, createdAt);
    sqlite3_bind_text(stmt, 4, trajectoryId.c_str(), -1, SQLITE_TRANSIENT);
    const int stepped = sqlite3_step(stmt);
    const int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (stepped != SQLITE_DONE || changed != 1) {
        error = "trajectory metadata update did not change one row";
        return false;
    }
    return true;
}

int inspectChild(const fs::path& db, const fs::path& out) {
    const std::string from = Memory::calculateContentHash(kAlpha);
    nlohmann::json body = viewJson(readDb(db, from));
    body["history"] = readHistory(db);
    writeAtomic(out, body.dump(2));
    return 0;
}

bool spawnInspect(const fs::path& db, const fs::path& out, std::string& error) {
    SigchldBlock block;
    const ChildProcess child = spawnGroup({exePath(), "inspect", "--db", db.string(), "--out", out.string()});
    if (!child.group_ready) {
        error = "inspect process group was not established";
        return false;
    }
    int status = 0;
    const WaitKind kind = waitChild(child.pid, 15000, status);
    if (kind != WaitKind::Exited || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error = "inspect process did not exit 0 (" + statusText(status) + ")";
        if (kind != WaitKind::Exited) killGroup(child, status, error);
        return false;
    }
    return fs::exists(out);
}

std::string haltBoundary(const fs::path& workspace) {
    try {
        return nlohmann::json::parse(readFile(workspace / "graph_halt.json")).value("boundary", "");
    } catch (...) {
        return {};
    }
}

bool edgeRewardedOnce(const DbView& view, const std::string& beta, float weight) {
    return view.edge_rows == 1 && view.edge_to == beta && view.success_count == 1 &&
           view.failure_count == 0 && near(view.weight, weight);
}

struct CaseReport {
    bool passed = false;
    std::string detail;
    nlohmann::json evidence;
};

CaseReport runGr1(const fs::path& workspace, const std::string& exe, const std::string& beta) {
    CaseReport report;
    SigchldBlock block;
    const ChildProcess child = spawnGroup({exe, "run", "--case", "gr-1", "--workspace", workspace.string()});
    if (!child.group_ready) {
        report.detail = "process group was not established";
        return report;
    }
    int status = 0;
    const WaitKind kind = waitChild(child.pid, kPhaseMs, status);
    report.evidence["child_wait"] = kind == WaitKind::Exited ? "exited" : "not-exited";
    report.evidence["child_status"] = statusText(status);
    report.evidence["termination"] = "normal-process-exit";
    if (kind != WaitKind::Exited || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        report.detail = "GR-1 child did not exit 0";
        return report;
    }
    const fs::path snap = workspace / "after_exit_inspect.json";
    std::string error;
    if (!spawnInspect(workspace / "memory.db", snap, error)) {
        report.detail = error;
        return report;
    }
    const DbView view = readDb(workspace / "memory.db", Memory::calculateContentHash(kAlpha));
    report.evidence["after_exit"] = viewJson(view);
    if (view.active || view.past_plans != 1 || view.trajectories != 1 ||
        !edgeRewardedOnce(view, beta, kReward * kDecay)) {
        report.detail = "GR-1 persisted state missed one aged reward, one history row, or one trajectory";
        return report;
    }
    report.passed = true;
    report.detail = "clean completion, separate inspect process";
    return report;
}

CaseReport runKilled(const std::string& caseName, const fs::path& workspace, const std::string& exe,
                     const std::string& expectedBoundary, const std::string& beta) {
    CaseReport report;
    SigchldBlock block;
    const ChildProcess child = spawnGroup({exe, "run", "--case", caseName, "--workspace", workspace.string()});
    if (!child.group_ready) {
        report.detail = "process group was not established";
        return report;
    }
    int stopped = 0;
    const WaitKind kind = waitChild(child.pid, kPhaseMs, stopped);
    report.evidence["child_wait_before_kill"] = kind == WaitKind::Stopped ? "stopped" : "not-stopped";
    report.evidence["stop_status"] = statusText(stopped);
    if (kind != WaitKind::Stopped) {
        int ignored = 0;
        std::string ignoredError;
        killGroup(child, ignored, ignoredError);
        report.detail = caseName + " did not stop at the halt";
        report.evidence["child_status"] = statusText(ignored);
        return report;
    }
    const std::string boundary = haltBoundary(workspace);
    report.evidence["halt_boundary"] = boundary;
    if (boundary != expectedBoundary) {
        int ignored = 0;
        std::string ignoredError;
        killGroup(child, ignored, ignoredError);
        report.detail = caseName + " stopped at " + boundary;
        return report;
    }
    const DbView before = readDb(workspace / "memory.db", Memory::calculateContentHash(kAlpha));
    nlohmann::json beforeBody = viewJson(before);
    if (caseName == "gr-2" || caseName == "gr-3" || caseName == "gr-4" || caseName == "gr-5" ||
        caseName == "gr-6" || caseName == "gr-7" || caseName == "gr-8" ||
        caseName == "gr-11" || caseName == "gr-12" || caseName == "gr-13") {
        beforeBody["history"] = readHistory(workspace / "memory.db");
        beforeBody["working_snapshot"] = readWorkingSnapshot(workspace / "memory.db");
        beforeBody["episode_step_count"] = countEpisodeSteps(workspace / "memory.db");
    }
    if (caseName == "gr-8") {
        const int64_t agedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count() -
                               (2LL * 24 * 60 * 60 * 1000);
        std::string metaError;
        if (!setTrajectoryMetadata(workspace / "memory.db", "traj-gr-8-plan", 4, 1, agedAt, metaError)) {
            int ignored = 0;
            std::string ignoredError;
            killGroup(child, ignored, ignoredError);
            report.detail = metaError;
            return report;
        }
        beforeBody["history_after_metadata"] = readHistory(workspace / "memory.db");
    }
    writeAtomic(workspace / "before_kill_parent.json", beforeBody.dump(2));
    report.evidence["before_kill"] = beforeBody;
    report.evidence["planner_creates_before_kill"] = countLines(workspace / "planner_log.txt", "create");
    report.evidence["planner_revises_before_kill"] = countLines(workspace / "planner_log.txt", "revise");
    int killed = 0;
    std::string error;
    if (!killGroup(child, killed, error)) {
        report.detail = error;
        return report;
    }
    report.evidence["child_status"] = statusText(killed);
    report.evidence["termination"] = "process-group-SIGKILL";
    if (!WIFSIGNALED(killed) || WTERMSIG(killed) != SIGKILL) {
        report.detail = caseName + " was not reaped as SIGKILL";
        return report;
    }
    if (!spawnInspect(workspace / "memory.db", workspace / "after_kill_inspect.json", error)) {
        report.detail = error;
        return report;
    }
    const DbView afterKill = readDb(workspace / "memory.db", Memory::calculateContentHash(kAlpha));
    report.evidence["after_kill"] = viewJson(afterKill);
    if (afterKill.success_count != before.success_count || afterKill.failure_count != before.failure_count ||
        !near(afterKill.weight, before.weight) || afterKill.graph_reinforced != before.graph_reinforced ||
        afterKill.history_aging_done != before.history_aging_done) {
        report.detail = caseName + " changed committed state across SIGKILL";
        return report;
    }

    auto acceptBefore = [&](std::string& why) {
        if (caseName == "gr-2") {
            if (!before.active || before.plan_id != "gr-2-plan" || before.retrieval_sets.size() != 1 ||
                before.retrieval_sets[0].size() != 1 ||
                before.retrieval_sets[0][0] != Memory::calculateContentHash(kAlpha) ||
                before.edge_rows != 0) {
                why = "first retrieval set was not the only durable set";
                return false;
            }
            return true;
        }
        if (caseName == "gr-3") {
            if (!before.active || !before.graph_reinforced || before.history_aging_done ||
                !edgeRewardedOnce(before, beta, kReward) || before.past_plans != 1 ||
                before.trajectories != 1) {
                why = "graph commit was not an unaged single reward";
                return false;
            }
            return true;
        }
        if (caseName == "gr-4") {
            if (!before.active || !before.graph_reinforced || before.history_aging_done ||
                !before.reflection_pending || before.reflection_count != 1 ||
                before.status != static_cast<int>(PlanStatus::ACTIVE) ||
                before.edge_rows != 1 || before.edge_to != beta || before.success_count != 0 ||
                before.failure_count != 1 || !near(before.weight, kPenalty) ||
                countLines(workspace / "planner_log.txt", "create") != 1) {
                why = "reflection graph commit was not one unaged penalty";
                return false;
            }
            return true;
        }
        if (caseName == "gr-11" || caseName == "gr-12") {
            const auto snapshot = report.evidence["before_kill"].value("working_snapshot", nlohmann::json::object());
            const auto history = report.evidence["before_kill"].value("history", nlohmann::json::object());
            const bool ready = before.active && !before.graph_reinforced && before.edge_rows == 0 &&
                               before.trajectories == 0 && snapshot.value("present", false) &&
                               snapshot.value("trajectory_id", "") == "traj-" + caseName + "-plan" &&
                               snapshot.value("error_count", 0) == 2 &&
                               snapshot.value("revisions_count", 0) >= 1 &&
                               snapshot.value("initial_step_count", 0) >= 2;
            if (!ready) {
                why = "working snapshot was not durable before history completion";
                return false;
            }
            if (caseName == "gr-12") {
                const auto past = history.value("past_plans", nlohmann::json::array());
                const float pastScore = past.empty() ? -1.f : past[0].value("success_score", -1.f);
                if (past.size() != 1 || !near(pastScore, 0.9f)) {
                    why = "past-plan score was not committed before the trajectory write";
                    return false;
                }
            }
            return true;
        }
        if (caseName == "gr-13") {
            const auto snapshot = report.evidence["before_kill"].value("working_snapshot", nlohmann::json::object());
            if (!before.active || before.trajectories != 0 || before.edge_rows != 0 ||
                !snapshot.value("present", false) || snapshot.value("step_count", 0) != 1 ||
                snapshot.value("error_count", 0) != 0) {
                why = "mid-execution snapshot was missing or a finalized trajectory existed";
                return false;
            }
            return true;
        }
        if (caseName == "gr-6" || caseName == "gr-7" || caseName == "gr-8") {
            const auto history = report.evidence["before_kill"].value("history", nlohmann::json::object());
            const auto trajectories = history.value("trajectories", nlohmann::json::array());
            const std::string expectedId = "traj-" + caseName + "-plan";
            const bool oneTraj = trajectories.size() == 1 &&
                                 trajectories[0].value("trajectory_id", "") == expectedId &&
                                 trajectories[0].value("step_count", 0) >= 3;
            if (caseName == "gr-7") {
                int errors = 0;
                for (const auto& step : trajectories.empty()
                                             ? nlohmann::json::array()
                                             : trajectories[0].value("steps", nlohmann::json::array())) {
                    if (!step.value("error", "").empty()) ++errors;
                }
                const float score = trajectories.empty() ? -1.f
                                                         : trajectories[0].value("success_score", -1.f);
                const auto past = history.value("past_plans", nlohmann::json::array());
                const float pastScore = past.empty() ? -1.f : past[0].value("success_score", -1.f);
                if (errors != 2 || !near(score, 0.9f) || !near(pastScore, 0.9f)) {
                    why = "recorded step errors did not produce the 0.9 score";
                    return false;
                }
            }
            if (!before.active || before.graph_reinforced || before.edge_rows != 0 ||
                before.past_plans != 1 || before.trajectories != 1 || !oneTraj) {
                why = "trajectory commit was not durable before graph reinforcement";
                return false;
            }
            return true;
        }
        if (!before.active || !before.graph_reinforced || !before.history_aging_done ||
            !edgeRewardedOnce(before, beta, kReward * kDecay)) {
            why = "aging commit was not a single decay with the active plan still present";
            return false;
        }
        return true;
    };
    std::string why;
    if (!acceptBefore(why)) {
        report.detail = why;
        return report;
    }
    if (caseName == "gr-13") {
        report.passed = true;
        report.detail = "working snapshot present and no finalized trajectory row";
        report.evidence["egar_visible_trajectories"] = 0;
        return report;
    }

    const int createsBefore = countLines(workspace / "planner_log.txt", "create");
    const ChildProcess resume = spawnGroup({exe, "resume", "--case", caseName, "--workspace", workspace.string()});
    if (!resume.group_ready) {
        report.detail = "resume process group was not established";
        return report;
    }
    int resumeStatus = 0;
    const WaitKind resumeKind = waitChild(resume.pid, kPhaseMs, resumeStatus);
    report.evidence["resume_wait"] = resumeKind == WaitKind::Exited ? "exited" : "not-exited";
    report.evidence["resume_status"] = statusText(resumeStatus);
    report.evidence["resume_termination"] = "normal-process-exit";
    if (resumeKind != WaitKind::Exited || !WIFEXITED(resumeStatus) || WEXITSTATUS(resumeStatus) != 0) {
        report.detail = caseName + " resume did not exit 0";
        return report;
    }
    if (!spawnInspect(workspace / "memory.db", workspace / "after_recovery_inspect.json", error)) {
        report.detail = error;
        return report;
    }
    const DbView recovered = readDb(workspace / "memory.db", Memory::calculateContentHash(kAlpha));
    report.evidence["after_recovery"] = viewJson(recovered);
    report.evidence["resume_result"] = readFile(workspace / "resume_result.json");
    const int createsAfter = countLines(workspace / "planner_log.txt", "create");
    const int revisesAfter = countLines(workspace / "planner_log.txt", "revise");
    report.evidence["planner_creates_during_resume"] = createsAfter - createsBefore;
    report.evidence["planner_revises_total"] = revisesAfter;
    if (caseName == "gr-2" || caseName == "gr-4" || caseName == "gr-5") {
        report.evidence["episode_steps_before_kill"] = report.evidence["before_kill"].value("episode_step_count", -1);
        report.evidence["episode_steps_after_recovery"] = countEpisodeSteps(workspace / "memory.db");
        report.evidence["working_snapshot_before_kill"] =
            report.evidence["before_kill"].value("working_snapshot", nlohmann::json::object());
    }

    if (caseName == "gr-2") {
        if (recovered.active || recovered.plan_id != "" || revisesAfter != 1 ||
            createsAfter != createsBefore || !edgeRewardedOnce(recovered, beta, kReward * kDecay) ||
            recovered.past_plans != 1) {
            report.detail = "revision did not keep one reward and the original plan id";
            return report;
        }
        try {
            const auto resumed = nlohmann::json::parse(readFile(workspace / "resume_result.json"));
            if (resumed.value("plan_id", "") != "gr-2-plan") {
                report.detail = "resume plan id changed";
                return report;
            }
        } catch (...) {
            report.detail = "resume result missing";
            return report;
        }
    } else if (caseName == "gr-3") {
        nlohmann::json afterHistory = nlohmann::json::object();
        try {
            afterHistory = nlohmann::json::parse(readFile(workspace / "after_recovery_inspect.json"))
                               .value("history", nlohmann::json::object());
        } catch (...) {
        }
        const auto beforeHistory = report.evidence["before_kill"].value("history", nlohmann::json::object());
        report.evidence["history_before_kill"] = beforeHistory;
        report.evidence["history_after_recovery"] = afterHistory;
        auto cognateScore = [](const nlohmann::json& history, const std::string& planId) {
            for (const auto& row : history.value("cognate_plans", nlohmann::json::array())) {
                if (row.value("plan_id", "") == planId) return row.value("success_score", -1.f);
            }
            return -1.f;
        };
        const float beforeScore = cognateScore(beforeHistory, "gr-3-plan");
        const float afterScore = cognateScore(afterHistory, "gr-3-plan");
        if (recovered.active || !edgeRewardedOnce(recovered, beta, kReward * kDecay) ||
            createsAfter != createsBefore || recovered.past_plans != 1 ||
            recovered.trajectories != 1 || !near(beforeScore, 1.0f) || !near(afterScore, beforeScore)) {
            report.detail = "resume aged more than once, reinforced again, or changed the Cognate score";
            return report;
        }
    } else if (caseName == "gr-4") {
        const float twice = kPenalty * kDecay * kDecay;
        if (recovered.failure_count != 1 || recovered.success_count != 0 || !near(recovered.weight, twice) ||
            createsAfter - createsBefore != 1 || revisesAfter != 0) {
            report.detail = "reflection resume repeated the penalty or called the planner early";
            return report;
        }
    } else if (caseName == "gr-11" || caseName == "gr-12") {
        nlohmann::json afterHistory = nlohmann::json::object();
        try {
            afterHistory = nlohmann::json::parse(readFile(workspace / "after_recovery_inspect.json"))
                               .value("history", nlohmann::json::object());
        } catch (...) {
        }
        report.evidence["history_after_recovery"] = afterHistory;
        const auto beforeHistory = report.evidence["before_kill"].value("history", nlohmann::json::object());
        const auto snapshot = report.evidence["before_kill"].value("working_snapshot", nlohmann::json::object());
        const auto rows = afterHistory.value("trajectories", nlohmann::json::array());
        const auto past = afterHistory.value("past_plans", nlohmann::json::array());
        const auto cognate = afterHistory.value("cognate_plans", nlohmann::json::array());
        int errors = 0;
        float score = -1.f;
        if (!rows.empty()) {
            score = rows[0].value("success_score", -1.f);
            for (const auto& step : rows[0].value("steps", nlohmann::json::array())) {
                if (!step.value("error", "").empty()) ++errors;
            }
        }
        int initialSteps = 0;
        try {
            const auto loaded = Thoth::SQLiteMemoryRepository((workspace / "memory.db").string())
                                    .loadTrajectory("traj-" + caseName + "-plan");
            if (loaded) {
                const auto body = nlohmann::json::parse(loaded->trajectory_json);
                if (body.contains("plan_initial") && body["plan_initial"].contains("steps") &&
                    body["plan_initial"]["steps"].is_array()) {
                    initialSteps = static_cast<int>(body["plan_initial"]["steps"].size());
                }
            }
        } catch (...) {
        }
        nlohmann::json resumeResult = nlohmann::json::object();
        try {
            resumeResult = nlohmann::json::parse(readFile(workspace / "resume_result.json"));
        } catch (...) {
        }
        const float pastScore = past.empty() ? -1.f : past[0].value("success_score", -1.f);
        const float cognateScore = cognate.empty() ? -1.f : cognate[0].value("success_score", -1.f);
        const auto beforePast = beforeHistory.value("past_plans", nlohmann::json::array());
        const float beforePastScore = beforePast.empty() ? -1.f : beforePast[0].value("success_score", -1.f);
        const bool matched = rows.size() == 1 &&
                             rows[0].value("trajectory_id", "") == "traj-" + caseName + "-plan" &&
                             errors == 2 && initialSteps >= 2 && near(score, 0.9f) &&
                             near(pastScore, 0.9f) && near(cognateScore, 0.9f) &&
                             (caseName != "gr-12" || near(beforePastScore, 0.9f)) &&
                             recovered.success_count == 1 && recovered.failure_count == 0 &&
                             !recovered.active &&
                             resumeResult.value("step_starts", nlohmann::json::array()).empty() &&
                             snapshot.value("revisions_count", 0) >= 1;
        report.evidence["egar_visible_before_kill"] = before.trajectories;
        if (!matched) {
            report.detail = "restored snapshot did not preserve the 0.9 score and recorded errors";
            return report;
        }
    } else if (caseName == "gr-6" || caseName == "gr-7" || caseName == "gr-8") {
        const auto beforeHistory = report.evidence["before_kill"].value("history", nlohmann::json::object());
        nlohmann::json afterKillHistory = nlohmann::json::object();
        nlohmann::json afterRecoveryHistory = nlohmann::json::object();
        try {
            afterKillHistory = nlohmann::json::parse(readFile(workspace / "after_kill_inspect.json"))
                                   .value("history", nlohmann::json::object());
            afterRecoveryHistory = nlohmann::json::parse(readFile(workspace / "after_recovery_inspect.json"))
                                       .value("history", nlohmann::json::object());
        } catch (...) {
        }
        report.evidence["history_before_kill"] = beforeHistory;
        report.evidence["history_after_kill"] = afterKillHistory;
        report.evidence["history_after_recovery"] = afterRecoveryHistory;
        nlohmann::json resumeResult = nlohmann::json::object();
        try {
            resumeResult = nlohmann::json::parse(readFile(workspace / "resume_result.json"));
        } catch (...) {
        }
        const auto resumedSteps = resumeResult.value("step_starts", nlohmann::json::array());
        const std::string expectedId = "traj-" + caseName + "-plan";
        auto findTraj = [&](const nlohmann::json& history, const std::string& id) -> const nlohmann::json* {
            if (!history.contains("trajectories")) return nullptr;
            for (const auto& traj : history["trajectories"]) {
                if (traj.value("trajectory_id", "") == id) return &traj;
            }
            return nullptr;
        };
        const nlohmann::json* original = findTraj(beforeHistory, expectedId);
        const nlohmann::json* recoveredTraj = findTraj(afterRecoveryHistory, expectedId);
        const auto recoveredRows = afterRecoveryHistory.value("trajectories", nlohmann::json::array());
        bool emptyId = false;
        for (const auto& traj : recoveredRows) {
            if (traj.value("trajectory_id", "") .empty()) emptyId = true;
        }
        std::vector<std::string> failed;
        if (recoveredRows.size() != 1 || !recoveredTraj) {
            failed.push_back("trajectory row count is not exactly one " + expectedId);
        }
        if (!original || !recoveredTraj ||
            original->value("steps", nlohmann::json::array()) != recoveredTraj->value("steps", nlohmann::json::array()) ||
            !near(original->value("success_score", -1.f), recoveredTraj->value("success_score", -2.f))) {
            failed.push_back("original trajectory steps or success score were not preserved");
        }
        if (afterRecoveryHistory.value("past_plans", nlohmann::json::array()).size() != 1 ||
            afterRecoveryHistory.value("cognate_plans", nlohmann::json::array()).size() != 1) {
            failed.push_back("past-plan or Cognate-plan row count is not one");
        }
        if (emptyId) failed.push_back("an empty-ID trajectory was inserted");
        if (recovered.success_count != 1 || recovered.failure_count != 0) {
            failed.push_back("graph reinforcement count is not one");
        }
        if (!resumedSteps.empty()) failed.push_back("completed steps were started again");
        if (caseName == "gr-6" || caseName == "gr-7") {
            auto cognateScore = [](const nlohmann::json& history, const std::string& planId) {
                for (const auto& row : history.value("cognate_plans", nlohmann::json::array())) {
                    if (row.value("plan_id", "") == planId) return row.value("success_score", -1.f);
                }
                return -1.f;
            };
            const float expected = caseName == "gr-7" ? 0.9f : 1.0f;
            const std::string planId = caseName + "-plan";
            if (!near(cognateScore(beforeHistory, planId), expected) ||
                !near(cognateScore(afterKillHistory, planId), expected) ||
                !near(cognateScore(afterRecoveryHistory, planId), expected)) {
                failed.push_back("Cognate score was not preserved");
            }
        }
        if (caseName == "gr-7") {
            const auto beforePast = beforeHistory.value("past_plans", nlohmann::json::array());
            const auto afterPast = afterRecoveryHistory.value("past_plans", nlohmann::json::array());
            const float beforeScore = beforePast.empty() ? -1.f : beforePast[0].value("success_score", -1.f);
            const float afterScore = afterPast.empty() ? -1.f : afterPast[0].value("success_score", -1.f);
            if (!near(beforeScore, 0.9f) || !near(afterScore, beforeScore)) {
                failed.push_back("past-plan error penalty was not preserved");
            }
        }
        if (caseName == "gr-8") {
            const auto stamped = report.evidence["before_kill"].value(
                "history_after_metadata", nlohmann::json::object());
            const nlohmann::json* stampedTraj = findTraj(stamped, expectedId);
            if (!stampedTraj || !recoveredTraj ||
                stampedTraj->value("usage_count", -1) != 4 ||
                recoveredTraj->value("usage_count", -1) != 4 ||
                stampedTraj->value("tier", -1) != 1 ||
                recoveredTraj->value("tier", -1) != 1) {
                failed.push_back("usage_count or tier was reset");
            }
        }
        report.evidence["acceptance_failures"] = failed;
        if (!failed.empty()) {
            report.detail = "confirmed recovery defect";
            report.passed = false;
            return report;
        }
    } else if (recovered.active || !edgeRewardedOnce(recovered, beta, kReward * kDecay) ||
               createsAfter != createsBefore || revisesAfter != 0) {
        report.detail = "post-aging resume decayed or reinforced again";
        return report;
    }
    report.passed = true;
    report.detail = "SIGKILL at " + expectedBoundary + ", recovery matched";
    return report;
}

CaseReport runLegacy(const fs::path& workspace, const std::string& exe) {
    CaseReport report;
    Plan plan;
    plan.plan_id = "gr-14-plan";
    plan.goal = "legacy active plan without a working snapshot";
    plan.status = PlanStatus::ACTIVE;
    plan.created_at_ms = 1;
    plan.updated_at_ms = 1;
    for (const char* id : {"a", "b", "c"}) {
        PlanStep step;
        step.step_id = std::string("legacy-") + id;
        step.description = "already finished";
        step.type = StepType::LLM;
        step.status = StepStatus::SUCCESS;
        plan.steps.push_back(step);
    }
    const std::string stored = plan.to_json().dump();
    if (stored.find("working_trajectory") != std::string::npos) {
        report.detail = "legacy seed contained a working snapshot";
        return report;
    }
    {
        Thoth::SQLiteMemoryRepository repo((workspace / "memory.db").string());
        Thoth::MemoryRepository::ActivePlanRecord rec;
        rec.plan_id = plan.plan_id;
        rec.session_id = kSession;
        rec.goal = plan.goal;
        rec.steps_json = stored;
        rec.controller_state = "IDLE";
        rec.created_at_ms = 1;
        rec.updated_at_ms = 1;
        if (!repo.storeActivePlan(rec)) {
            report.detail = "legacy active plan was not stored";
            return report;
        }
    }
    SigchldBlock block;
    const ChildProcess child = spawnGroup({exe, "resume", "--case", "gr-14", "--workspace", workspace.string()});
    if (!child.group_ready) {
        report.detail = "process group was not established";
        return report;
    }
    int status = 0;
    const WaitKind kind = waitChild(child.pid, kPhaseMs, status);
    report.evidence["resume_status"] = statusText(status);
    report.evidence["termination"] = "resume-without-snapshot";
    if (kind != WaitKind::Exited || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        report.detail = "legacy resume did not exit 0";
        return report;
    }
    const auto history = readHistory(workspace / "memory.db");
    report.evidence["history"] = history;
    const auto rows = history.value("trajectories", nlohmann::json::array());
    const auto cognate = history.value("cognate_plans", nlohmann::json::array());
    const int steps = rows.empty() ? -1 : rows[0].value("step_count", -1);
    const float score = rows.empty() ? -1.f : rows[0].value("success_score", -1.f);
    const float cognateScore = cognate.empty() ? -1.f : cognate[0].value("success_score", -1.f);
    if (rows.size() != 1 || steps != 0 || !near(score, 1.0f) || !near(cognateScore, 1.0f)) {
        report.detail = "legacy resume fabricated trajectory steps or lost the completion";
        return report;
    }
    report.passed = true;
    report.detail = "no snapshot: trajectory steps were not fabricated; recorded errors and revision count are unrecoverable";
    return report;
}

CaseReport runUninterrupted(const fs::path& workspace, const std::string& exe, const std::string& beta) {
    CaseReport report;
    SigchldBlock block;
    const ChildProcess child = spawnGroup({exe, "run", "--case", "gr-10", "--workspace", workspace.string()});
    if (!child.group_ready) {
        report.detail = "process group was not established";
        return report;
    }
    int status = 0;
    const WaitKind kind = waitChild(child.pid, kPhaseMs, status);
    report.evidence["child_wait"] = kind == WaitKind::Exited ? "exited" : "not-exited";
    report.evidence["child_status"] = statusText(status);
    report.evidence["termination"] = "normal-process-exit";
    if (kind != WaitKind::Exited || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        report.detail = "uninterrupted child did not exit 0";
        return report;
    }
    std::string error;
    if (!spawnInspect(workspace / "memory.db", workspace / "after_exit_inspect.json", error)) {
        report.detail = error;
        return report;
    }
    const auto history = readHistory(workspace / "memory.db");
    report.evidence["history"] = history;
    const DbView view = readDb(workspace / "memory.db", Memory::calculateContentHash(kAlpha));
    report.evidence["after_exit"] = viewJson(view);
    const auto trajectories = history.value("trajectories", nlohmann::json::array());
    const auto cognate = history.value("cognate_plans", nlohmann::json::array());
    const float trajectoryScore = trajectories.empty() ? -1.f : trajectories[0].value("success_score", -1.f);
    const float cognateScore = cognate.empty() ? -1.f : cognate[0].value("success_score", -1.f);
    if (view.active || trajectories.size() != 1 || cognate.size() != 1 ||
        trajectories[0].value("trajectory_id", "") != "traj-gr-10-plan" ||
        !near(trajectoryScore, 1.0f) || !near(cognateScore, trajectoryScore) ||
        !edgeRewardedOnce(view, beta, kReward * kDecay)) {
        report.detail = "uninterrupted Cognate score did not match the trajectory score";
        return report;
    }
    report.passed = true;
    report.detail = "Cognate score matched the computed trajectory score";
    return report;
}

CaseReport runReflection(const fs::path& workspace, const std::string& exe) {
    CaseReport report;
    SigchldBlock block;
    const ChildProcess child = spawnGroup({exe, "run", "--case", "gr-9", "--workspace", workspace.string()});
    if (!child.group_ready) {
        report.detail = "process group was not established";
        return report;
    }
    int status = 0;
    const WaitKind kind = waitChild(child.pid, kPhaseMs, status);
    report.evidence["child_wait"] = kind == WaitKind::Exited ? "exited" : "not-exited";
    report.evidence["child_status"] = statusText(status);
    report.evidence["termination"] = "normal-process-exit";
    if (kind != WaitKind::Exited || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        report.detail = "reflection child did not exit 0";
        return report;
    }
    std::string error;
    if (!spawnInspect(workspace / "memory.db", workspace / "after_exit_inspect.json", error)) {
        report.detail = error;
        return report;
    }
    const auto history = readHistory(workspace / "memory.db");
    report.evidence["history"] = history;
    const auto trajectories = history.value("trajectories", nlohmann::json::array());
    const auto past = history.value("past_plans", nlohmann::json::array());
    const auto cognate = history.value("cognate_plans", nlohmann::json::array());
    auto scoreFor = [](const nlohmann::json& rows, const char* key, const std::string& id) {
        for (const auto& row : rows) {
            if (row.value(key, "") == id) return row.value("success_score", -1.f);
        }
        return -1.f;
    };
    const bool separate =
        trajectories.size() == 2 && past.size() == 2 && cognate.size() == 2 &&
        near(scoreFor(trajectories, "trajectory_id", "traj-gr-9-plan"), 0.0f) &&
        near(scoreFor(trajectories, "trajectory_id", "traj-gr-9-plan-replacement"), 1.0f) &&
        near(scoreFor(past, "plan_id", "gr-9-plan"), 0.0f) &&
        near(scoreFor(past, "plan_id", "gr-9-plan-replacement"), 1.0f) &&
        near(scoreFor(cognate, "plan_id", "gr-9-plan"), 0.0f) &&
        near(scoreFor(cognate, "plan_id", "gr-9-plan-replacement"), 1.0f);
    if (!separate) {
        report.detail = "failed and replacement plans did not keep separate history scores";
        return report;
    }
    report.passed = true;
    report.detail = "separate plan and trajectory ids";
    return report;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
    auto option = [&](const std::string& name) {
        for (size_t i = 0; i + 1 < args.size(); ++i) {
            if (args[i] == name) return args[i + 1];
        }
        return std::string();
    };
    if (!args.empty() && args[0] == "inspect") {
        return inspectChild(option("--db"), option("--out"));
    }
    if (!args.empty() && args[0] == "run") {
        return runChild(option("--case"), option("--workspace"));
    }
    if (!args.empty() && args[0] == "resume") {
        return resumeChild(option("--case"), option("--workspace"));
    }

    const std::string sha = parentSha();
    if (sha.size() < 7) {
        std::cerr << "could not read the parent HEAD\n";
        return 2;
    }
    const fs::path root = fs::path("/tmp/thoth-e2e-graph-" + sha);
    std::string error;
    if (!disposablePath(root, error)) {
        std::cerr << error << "\n";
        return 2;
    }
    const char* forbidden[] = {"THOTH_C64_WINDOW_FILE", "THOTH_C64_CURRENT_FINGERPRINT",
                               "THOTH_ALP_ENABLED", "THOTH_TEST_SUITE_DEV"};
    for (const char* key : forbidden) {
        const char* value = std::getenv(key);
        if (value && *value) {
            std::cerr << "refusing to run with " << key << " set\n";
            return 2;
        }
    }
    fs::create_directories(root);
    const std::string exe = exePath();
    const std::string beta = Memory::calculateContentHash(kBeta);
    const auto started = std::chrono::steady_clock::now();
    nlohmann::json summary = {{"root", root.string()}, {"parent_sha", sha}};
    struct Item {
        const char* name;
        const char* boundary;
    };
    std::vector<Item> items = {
        {"gr-1", ""},
        {"gr-2", "after_first_retrieval_set"},
        {"gr-3", "after_graph_commit"},
        {"gr-4", "after_graph_commit"},
        {"gr-5", "after_aging_commit"}
    };
    bool repair = false;
    if (!args.empty() && args[0] == "gr-6") {
        items = {{"gr-6", "after_trajectory_commit"}};
    } else if (!args.empty() && args[0] == "gr-checkpoint") {
        repair = true;
        items = {
            {"gr-11", "before_history_write"},
            {"gr-12", "after_past_plan"},
            {"gr-13", "after_first_retrieval_set"},
            {"gr-14", ""},
            {"gr-6", "after_trajectory_commit"},
            {"gr-7", "after_trajectory_commit"},
            {"gr-3", "after_graph_commit"},
            {"gr-9", ""},
            {"gr-10", ""}
        };
    } else if (!args.empty() && args[0] == "gr-cognate") {
        repair = true;
        items = {
            {"gr-6", "after_trajectory_commit"},
            {"gr-7", "after_trajectory_commit"},
            {"gr-3", "after_graph_commit"},
            {"gr-9", ""},
            {"gr-10", ""}
        };
    } else if (!args.empty() && args[0] == "gr-confirm") {
        items = {
            {"gr-2", "after_first_retrieval_set"},
            {"gr-4", "after_graph_commit"},
            {"gr-5", "after_aging_commit"}
        };
    } else if (!args.empty() && args[0] == "gr-repair") {
        repair = true;
        items = {
            {"gr-6", "after_trajectory_commit"},
            {"gr-7", "after_trajectory_commit"},
            {"gr-8", "after_trajectory_commit"},
            {"gr-3", "after_graph_commit"},
            {"gr-9", ""}
        };
    }
    bool all = true;
    for (const auto& item : items) {
        const fs::path workspace = root / item.name;
        std::string workspaceError;
        if (!disposablePath(workspace, workspaceError)) {
            std::cerr << workspaceError << "\n";
            return 2;
        }
        std::error_code removeError;
        fs::remove_all(workspace, removeError);
        fs::create_directories(workspace);
        const auto caseStarted = std::chrono::steady_clock::now();
        CaseReport report = item.name == std::string("gr-1")
                                ? runGr1(workspace, exe, beta)
                                : item.name == std::string("gr-9")
                                      ? runReflection(workspace, exe)
                                      : item.name == std::string("gr-10")
                                            ? runUninterrupted(workspace, exe, beta)
                                            : item.name == std::string("gr-14")
                                                  ? runLegacy(workspace, exe)
                                                  : runKilled(item.name, workspace, exe, item.boundary, beta);
        const auto caseMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - caseStarted)
                                .count();
        report.evidence["runtime_ms"] = caseMs;
        report.evidence["workspace"] = workspace.string();
        summary[item.name] = {{"passed", report.passed},
                              {"detail", report.detail},
                              {"evidence", report.evidence}};
        std::cout << item.name << (report.passed ? " PASS " : " FAIL ") << report.detail
                  << " (" << caseMs << " ms)\n";
        if (!report.passed) {
            all = false;
            if (!repair) break;
        }
    }
    const auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();
    summary["passed"] = all;
    summary["runtime_ms"] = totalMs;
    writeAtomic(root / "campaign_report.json", summary.dump(2));
    std::cout << "campaign " << (all ? "PASS" : "FAIL") << " root=" << root
              << " runtime_ms=" << totalMs << "\n";
    return all ? 0 : 1;
}
