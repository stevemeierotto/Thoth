/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — opt-in E2E reliability harness.
 * Not part of default CTest. Live cases are not started by self-check.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "basic_agent_plugin.h"
#include "chunkers/code_chunk.h"
#include "config.h"
#include "corpus_create.h"
#include "embedding_engine.h"
#include "engine_event.h"
#include "executive_controller.h"
#include "index_manager.h"
#include "inference_client.h"
#include "inference_endpoint.h"
#include "iplanner.h"
#include "itool.h"
#include "memory.h"
#include "plan.h"
#include "rag.h"
#include "sqlite_memory_repository.h"
#include "tools.h"

#include <json.hpp>

#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kRl01Sec = 180;
constexpr int kRl02RunSec = 8 * 60;
constexpr int kRl02ResumeSec = 8 * 60;
constexpr int kRl04Sec = 60;
constexpr int kRl03RunSec = 8 * 60;
constexpr int kRl03CharSec = 60;
constexpr int kRl03ResumeBackstopSec = 90;
constexpr int kOverheadSec = 2 * 60;
constexpr int kTotalCapSec = 31 * 60;

static_assert(kRl01Sec + kRl02RunSec + kRl02ResumeSec + kRl04Sec + kRl03RunSec +
                      kRl03CharSec + kOverheadSec ==
                  kTotalCapSec,
              "reliability case limits must sum to the 31-minute cap");

constexpr const char* kGoal =
    "Using only the isolated disposable note, report the marker RELIABILITYQX7 in one sentence.";

struct Options {
    std::string workspace;
    std::string logs;
    std::string session;
    std::string chat_url;
    std::string embed_url;
    std::string boundary = "none";
    std::string expect = "terminal";
    std::string case_name;
    std::string live_chat_url;
    int max_wall_ms = 0;
    bool sequence = false;
};

enum class WaitKind { Stopped, Exited, TimedOut, Error };

struct Verdict {
    bool accepted = false;
    std::string classification;
    std::string detail;
};

std::string parentSha() {
    const std::string command = std::string("git -C ") + THOTH_SOURCE_DIR + " rev-parse HEAD";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {};
    char buffer[128];
    std::string sha;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        sha += buffer;
    }
    pclose(pipe);
    while (!sha.empty() && (sha.back() == '\n' || sha.back() == '\r')) sha.pop_back();
    return sha;
}

std::string disposableRoot() {
    const std::string sha = parentSha();
    if (sha.size() < 7) return {};
    return std::string("/tmp/thoth-e2e-reliability-") + sha;
}

uint64_t fnv1a(const std::string& text) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool forbiddenEnvSet(std::string& name) {
    const char* keys[] = {"THOTH_C64_WINDOW_FILE", "THOTH_C64_CURRENT_FINGERPRINT",
                          "THOTH_ALP_ENABLED", "THOTH_TEST_SUITE_DEV", "THOTH_MOCK_LLM"};
    for (const char* key : keys) {
        const char* value = std::getenv(key);
        if (value && *value) {
            name = key;
            return true;
        }
    }
    return false;
}

bool symlinkInsideRoot(const fs::path& path, const fs::path& root) {
    const fs::path normal = path.lexically_normal();
    const fs::path rootNormal = root.lexically_normal();
    fs::path current;
    bool inside = false;
    auto rootIt = rootNormal.begin();
    for (const auto& part : normal) {
        current /= part;
        if (!inside) {
            if (rootIt != rootNormal.end() && *rootIt == part) {
                ++rootIt;
                if (rootIt == rootNormal.end()) inside = true;
            }
            continue;
        }
        std::error_code ec;
        const auto status = fs::symlink_status(current, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory) return false;
            return true;
        }
        if (fs::is_symlink(status)) return true;
    }
    return false;
}

bool isLexicallyInside(const fs::path& path, const fs::path& root) {
    auto pathIt = path.begin();
    for (const auto& part : root) {
        if (pathIt == path.end() || *pathIt != part) return false;
        ++pathIt;
    }
    return true;
}

bool validateDisposablePath(const fs::path& raw, const fs::path& root, std::string& error) {
    if (raw.empty() || !raw.is_absolute()) {
        error = "path must be absolute";
        return false;
    }
    const fs::path normal = raw.lexically_normal();
    if (!isLexicallyInside(normal, root.lexically_normal())) {
        error = "path is outside the disposable root";
        return false;
    }
    for (const auto& part : normal) {
        if (part == "..") {
            error = "path contains ..";
            return false;
        }
    }
    if (symlinkInsideRoot(normal, root)) {
        error = "path contains a symlink";
        return false;
    }
    const fs::path devWorkspace = fs::path(THOTH_SOURCE_DIR) / "agent_workspace";
    const fs::path devLogs = fs::path(THOTH_SOURCE_DIR) / "logs";
    if (isLexicallyInside(normal, devWorkspace.lexically_normal()) ||
        isLexicallyInside(normal, devLogs.lexically_normal()) ||
        normal == devWorkspace.lexically_normal() || normal == devLogs.lexically_normal()) {
        error = "path resolves into DEV workspace or logs";
        return false;
    }
    return true;
}

bool ensureDisposableDir(const fs::path& raw, const fs::path& root, std::string& error) {
    if (!validateDisposablePath(raw, root, error)) return false;
    std::error_code ec;
    fs::create_directories(raw, ec);
    if (ec) {
        error = "cannot create directory";
        return false;
    }
    std::error_code canonicalEc;
    const fs::path canonical = fs::weakly_canonical(raw, canonicalEc);
    std::error_code rootEc;
    const fs::path canonicalRoot = fs::weakly_canonical(root, rootEc);
    if (canonicalEc || rootEc || symlinkInsideRoot(canonical, canonicalRoot) ||
        !isLexicallyInside(canonical, canonicalRoot)) {
        error = "created path escapes the disposable root";
        return false;
    }
    return true;
}

bool removeOwnedTree(const fs::path& raw, const fs::path& root, std::string& error) {
    if (!validateDisposablePath(raw, root, error)) return false;
    const fs::path normal = raw.lexically_normal();
    const fs::path rootNormal = root.lexically_normal();
    if (normal == rootNormal) {
        error = "refusing to delete the disposable root itself";
        return false;
    }
    std::error_code ec;
    fs::remove_all(normal, ec);
    if (ec) {
        error = "remove_all failed";
        return false;
    }
    return true;
}

bool trajectoryIsProductionDefault(double weight) {
    return weight > -0.050000001 && weight < -0.049999999;
}

bool ensureRetrievalConfig(const fs::path& workspace, std::string& error) {
    const fs::path path = workspace / "retrieval_config.json";
    std::error_code ec;
    if (fs::exists(path, ec)) {
        try {
            const auto config = nlohmann::json::parse(readFile(path));
            if (!config.contains("retrieval_weights") ||
                !config["retrieval_weights"].contains("trajectory") ||
                !config["retrieval_weights"]["trajectory"].is_number()) {
                error = "existing retrieval_config.json has no numeric trajectory weight";
                return false;
            }
            const double weight = config["retrieval_weights"]["trajectory"].get<double>();
            if (!trajectoryIsProductionDefault(weight)) {
                error = "existing retrieval_config.json trajectory weight is not -0.05";
                return false;
            }
        } catch (...) {
            error = "existing retrieval_config.json is not valid JSON";
            return false;
        }
        return true;
    }
    nlohmann::json config = {{"retrieval_weights",
                              {{"query", 0.4},
                               {"direction", 0.4},
                               {"trajectory", -0.05},
                               {"keyword", 0.3}}}};
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        error = "cannot write retrieval_config.json";
        return false;
    }
    out << config.dump(4) << "\n";
    return static_cast<bool>(out);
}

nlohmann::json parsePlanJson(const std::string& steps_json) {
    try {
        return nlohmann::json::parse(steps_json);
    } catch (...) {
        return nlohmann::json();
    }
}

bool matchesRl02(const std::string& steps_json) {
    const auto plan = parsePlanJson(steps_json);
    if (!plan.contains("steps") || !plan["steps"].is_array()) return false;
    int retrieval_success = 0;
    int llm_pending = 0;
    for (const auto& step : plan["steps"]) {
        const int type = step.value("type", -1);
        const int status = step.value("status", -1);
        if (status == static_cast<int>(StepStatus::RUNNING)) return false;
        if (status == static_cast<int>(StepStatus::FAILED)) return false;
        if (type == static_cast<int>(StepType::RETRIEVAL) &&
            status == static_cast<int>(StepStatus::SUCCESS)) {
            ++retrieval_success;
            continue;
        }
        if (type == static_cast<int>(StepType::LLM) &&
            status == static_cast<int>(StepStatus::PENDING)) {
            ++llm_pending;
            continue;
        }
        return false;
    }
    return retrieval_success == 1 && llm_pending == 1;
}

bool matchesRl03(const std::string& steps_json) {
    const auto plan = parsePlanJson(steps_json);
    if (!plan.contains("steps") || !plan["steps"].is_array()) return false;
    for (const auto& step : plan["steps"]) {
        if (step.value("status", -1) == static_cast<int>(StepStatus::RUNNING)) return true;
    }
    return false;
}

bool boundaryMatches(const std::string& boundary, const std::string& steps_json) {
    if (boundary == "rl02") return matchesRl02(steps_json);
    if (boundary == "rl03") return matchesRl03(steps_json);
    return false;
}

bool writeAtomic(const fs::path& path, const std::string& body) {
    const fs::path tmp = path.string() + ".tmp";
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) return false;
    out << body;
    out.flush();
    if (!out) return false;
    out.close();
    std::error_code ec;
    fs::rename(tmp, path, ec);
    return !ec;
}

Verdict judgeRl01(const std::string& terminal, int reflections, const std::string& skip_reason,
                  const std::vector<std::string>& errors, bool active_plan, int past_plans) {
    Verdict verdict;
    verdict.classification = "fail";
    if (terminal != "PLAN_FAILED") {
        verdict.detail = "terminal event was not PLAN_FAILED";
        return verdict;
    }
    if (reflections > 2) {
        verdict.detail = "reflection count exceeded 2";
        return verdict;
    }
    if (skip_reason != "max_reflections_exhausted") {
        verdict.detail = "reflection_skip_reason was not max_reflections_exhausted";
        return verdict;
    }
    for (const auto& error : errors) {
        std::string lower = error;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.find("timeout") != std::string::npos) {
            verdict.detail = "failure was classified as a timeout";
            return verdict;
        }
    }
    if (active_plan) {
        verdict.detail = "active plan remained";
        return verdict;
    }
    if (past_plans < 1) {
        verdict.detail = "past plans were not retained";
        return verdict;
    }
    verdict.accepted = true;
    verdict.classification = "controlled_failure_handling";
    verdict.detail = "PLAN_FAILED contained the chat refusal. Service was not restored.";
    return verdict;
}

Verdict judgeRl02(const std::string& original_plan, const std::string& resumed_plan,
                  int retrieval_starts, int llm_starts, const std::string& terminal) {
    Verdict verdict;
    verdict.classification = "fail";
    if (original_plan.empty() || original_plan != resumed_plan) {
        verdict.detail = "resume did not keep the same plan_id";
        return verdict;
    }
    if (retrieval_starts != 0) {
        verdict.detail = "completed retrieval step was dispatched again";
        return verdict;
    }
    if (llm_starts != 1) {
        verdict.detail = "pending LLM step was not dispatched once";
        return verdict;
    }
    if (terminal == "PLAN_FAILED") {
        verdict.classification = "controlled_terminal_outcome";
        verdict.detail = "PLAN_FAILED is not a successful resume";
        return verdict;
    }
    if (terminal != "PLAN_COMPLETED") {
        verdict.detail = "resume did not reach PLAN_COMPLETED";
        return verdict;
    }
    verdict.accepted = true;
    verdict.classification = "successful_resume";
    verdict.detail = "same plan completed without a second retrieval dispatch";
    return verdict;
}

Verdict judgeRl04(bool plan_created, int step_starts, bool active_plan) {
    Verdict verdict;
    verdict.classification = "fail";
    if (plan_created || step_starts != 0 || active_plan) {
        verdict.detail = "completed session started or resumed a plan";
        return verdict;
    }
    verdict.accepted = true;
    verdict.classification = "completed_session_stayed_terminal";
    verdict.detail = "checkResumablePlan did not start a plan";
    return verdict;
}

struct Rl03Evidence {
    bool still_running = false;
    bool boundary_step_still_running = false;
    std::string boundary_plan_id;
    std::string resume_plan_id;
    std::string terminal;
    int retrieval_starts = 0;
    int llm_starts = 0;
    int reflections = 0;
    int success_step_starts = 0;
    bool prior_running_recorded = false;
    bool requeued_pending_persisted = false;
    bool live_synthesis = false;
};

Verdict judgeRl03(const Rl03Evidence& evidence) {
    Verdict verdict;
    verdict.accepted = false;
    const bool stalled = evidence.still_running && evidence.boundary_step_still_running
        && evidence.retrieval_starts == 0 && evidence.llm_starts == 0 && evidence.terminal.empty();
    if (stalled) {
        verdict.classification = "defect_reproduced";
        verdict.detail = "RUNNING step remained undispatched after resume. This is a reliability defect.";
        return verdict;
    }
    if (evidence.boundary_plan_id.empty() || evidence.resume_plan_id != evidence.boundary_plan_id) {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "resume did not keep the interrupted plan_id";
        return verdict;
    }
    if (!evidence.prior_running_recorded) {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "original RUNNING status was not recorded";
        return verdict;
    }
    if (!evidence.requeued_pending_persisted) {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "retrieval was not persisted as PENDING before redispatch";
        return verdict;
    }
    if (evidence.retrieval_starts != 1) {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "interrupted retrieval was not dispatched exactly once";
        return verdict;
    }
    if (evidence.success_step_starts != 0) {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "a previously successful step was dispatched again";
        return verdict;
    }
    if (evidence.llm_starts != 1 || !evidence.live_synthesis) {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "pending synthesis did not execute once on llama.cpp";
        return verdict;
    }
    if (evidence.reflections != 0) {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "resume created a reflection or replacement plan";
        return verdict;
    }
    if (evidence.still_running || evidence.boundary_step_still_running) {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "a RUNNING step was still outstanding after resume";
        return verdict;
    }
    if (evidence.terminal != "PLAN_COMPLETED") {
        verdict.classification = "recovery_not_verified";
        verdict.detail = "resume did not reach PLAN_COMPLETED";
        return verdict;
    }
    verdict.accepted = true;
    verdict.classification = "recovery_verified";
    verdict.detail = "interrupted retrieval was requeued once and the same plan completed";
    return verdict;
}

struct ChildProcess {
    pid_t pid = -1;
    pid_t pgid = -1;
    bool group_ready = false;
};

bool isOurChild(pid_t pid) {
    if (pid <= 1) return false;
    std::ifstream in("/proc/" + std::to_string(pid) + "/status");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("PPid:", 0) != 0) continue;
        const int ppid = std::stoi(line.substr(5));
        return ppid == getpid();
    }
    return false;
}

void logProcess(const ChildProcess& child) {
    std::cerr << "RELIABILITY_PROCESS pid=" << child.pid << " pgid=" << child.pgid
              << " supervisor_pgid=" << getpgrp()
              << " group_ready=" << (child.group_ready ? 1 : 0) << "\n";
}

bool ownershipOk(const ChildProcess& child) {
    if (child.pid <= 1 || child.pgid <= 1 || child.pgid != child.pid) return false;
    if (child.pid == getpid() || child.pgid == getpgrp() || child.pid == getpgrp()) return false;
    if (!isOurChild(child.pid)) return false;
    const pid_t actual = getpgid(child.pid);
    if (actual != child.pid || actual == getpgrp()) return false;
    return true;
}

bool reapBounded(pid_t pid, int timeout_ms, int& status, std::string& error) {
    if (pid <= 1) {
        error = "refusing to wait on an invalid pid";
        return false;
    }
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    const int fd = signalfd(-1, &set, SFD_CLOEXEC);
    if (fd >= 0) {
        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        const int polled = poll(&pfd, 1, timeout_ms);
        if (polled > 0) {
            signalfd_siginfo info{};
            const ssize_t got = read(fd, &info, sizeof(info));
            (void)got;
        }
        close(fd);
    }
    const pid_t reaped = waitpid(pid, &status, WNOHANG | WUNTRACED);
    if (reaped == pid && (WIFEXITED(status) || WIFSIGNALED(status))) return true;
    if (reaped < 0 && errno == ECHILD) return true;
    error = "bounded reap did not collect pid " + std::to_string(pid);
    return false;
}

bool terminateChild(const ChildProcess& child, int& status, std::string& error) {
    if (child.pid <= 1 || child.pid == getpid()) {
        error = "invalid child pid";
        return false;
    }
    int probe = 0;
    const pid_t got = waitpid(child.pid, &probe, WNOHANG | WUNTRACED);
    if (got == child.pid && (WIFEXITED(probe) || WIFSIGNALED(probe))) {
        status = probe;
        std::cerr << "RELIABILITY_REAPED pid=" << child.pid << " status=" << status << "\n";
        return true;
    }
    if (got < 0 && errno == ECHILD) {
        status = 0;
        std::cerr << "RELIABILITY_REAPED pid=" << child.pid << " already collected\n";
        return true;
    }
    bool sent = false;
    if (ownershipOk(child)) {
        std::cerr << "RELIABILITY_SIGNAL group pid=" << child.pid << " pgid=" << child.pgid
                  << " supervisor_pgid=" << getpgrp() << "\n";
        sent = kill(-child.pgid, SIGKILL) == 0;
    } else if (isOurChild(child.pid)) {
        std::cerr << "RELIABILITY_SIGNAL direct pid=" << child.pid << " pgid=" << getpgid(child.pid)
                  << " supervisor_pgid=" << getpgrp() << "\n";
        sent = kill(child.pid, SIGKILL) == 0;
    } else {
        error = "ownership check failed and pid is not our child";
        std::cerr << "RELIABILITY_CLEANUP_FAILED pid=" << child.pid << " " << error << "\n";
        return false;
    }
    if (!sent) {
        error = std::string("signal failed: ") + std::strerror(errno);
        std::cerr << "RELIABILITY_CLEANUP_FAILED pid=" << child.pid << " " << error << "\n";
    }
    if (!reapBounded(child.pid, 2000, status, error)) {
        std::cerr << "RELIABILITY_CLEANUP_FAILED pid=" << child.pid << " " << error << "\n";
        return false;
    }
    std::cerr << "RELIABILITY_REAPED pid=" << child.pid << " status=" << status << "\n";
    return true;
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

WaitKind waitChild(pid_t pid, int timeout_ms, int& status) {
    if (pid <= 1) return WaitKind::Error;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    const int fd = signalfd(-1, &set, SFD_CLOEXEC);
    if (fd < 0) return WaitKind::Error;
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    const int polled = poll(&pfd, 1, timeout_ms);
    if (polled == 0) {
        close(fd);
        status = 0;
        return WaitKind::TimedOut;
    }
    if (polled < 0) {
        close(fd);
        return WaitKind::Error;
    }
    signalfd_siginfo info{};
    const ssize_t got = read(fd, &info, sizeof(info));
    close(fd);
    if (got < 0) return WaitKind::Error;
    const pid_t reaped = waitpid(pid, &status, WUNTRACED | WNOHANG);
    if (reaped == pid && WIFSTOPPED(status)) return WaitKind::Stopped;
    if (reaped == pid && (WIFEXITED(status) || WIFSIGNALED(status))) return WaitKind::Exited;
    return WaitKind::Error;
}

ChildProcess spawnGroup(const std::vector<std::string>& args) {
    ChildProcess child;
    const pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "RELIABILITY_PROCESS fork failed: " << std::strerror(errno) << "\n";
        return child;
    }
    if (pid == 0) {
        if (setpgid(0, 0) != 0) {
            std::cerr << "child setpgid failed: " << std::strerror(errno) << "\n";
            _exit(127);
        }
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        execv(argv[0], argv.data());
        _exit(127);
    }
    if (setpgid(pid, pid) != 0) {
        const pid_t actual = getpgid(pid);
        child.pid = pid;
        child.pgid = actual;
        child.group_ready = actual == pid && actual > 1 && actual != getpgrp();
        std::cerr << "supervisor setpgid failed errno=" << errno << " ";
        logProcess(child);
        return child;
    }
    child.pid = pid;
    child.pgid = getpgid(pid);
    child.group_ready = child.pgid == pid && child.pgid > 1 && child.pgid != getpgrp();
    logProcess(child);
    return child;
}

ChildProcess spawnWithoutGroup() {
    ChildProcess child;
    const pid_t pid = fork();
    if (pid < 0) return child;
    if (pid == 0) {
        for (;;) pause();
    }
    child.pid = pid;
    child.pgid = getpgid(pid);
    child.group_ready = false;
    logProcess(child);
    return child;
}

std::string exePath() {
    char buffer[4096];
    const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n <= 0) return {};
    buffer[n] = '\0';
    return buffer;
}

bool connectionRefused(const std::string& url, std::string& error) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos || url.compare(0, scheme, "http") != 0) {
        error = "chat url must be http";
        return false;
    }
    const std::string rest = url.substr(scheme + 3);
    const auto slash = rest.find('/');
    const std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    const auto colon = hostport.rfind(':');
    if (colon == std::string::npos) {
        error = "chat url needs a port";
        return false;
    }
    const std::string host = hostport.substr(0, colon);
    int port = 0;
    try {
        port = std::stoi(hostport.substr(colon + 1));
    } catch (...) {
        error = "chat url port is invalid";
        return false;
    }
    if (host != "127.0.0.1") {
        error = "refusal probe only accepts 127.0.0.1";
        return false;
    }
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = "socket failed";
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    const int rc = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    const int err = errno;
    close(fd);
    if (rc == 0) {
        error = "port accepted a connection";
        return false;
    }
    if (err != ECONNREFUSED) {
        error = std::string("connect errno ") + std::to_string(err);
        return false;
    }
    return true;
}

struct EventLog {
    std::mutex mu;
    std::condition_variable cv;
    std::vector<nlohmann::json> events;
    bool terminal = false;
    std::string terminal_type;
    std::string plan_id;
    bool plan_created = false;
    int retrieval_starts = 0;
    int llm_starts = 0;
    int reflections = 0;
    int success_step_starts = 0;
    int requeued_persisted_status = -1;
    bool prior_running_recorded = false;
    std::vector<std::string> errors;
    fs::path path;

    void record(const ControllerEvent& event) {
        nlohmann::json row = {{"type", Thoth::engineEventTypeName(event.type)},
                              {"plan_id", event.plan_id},
                              {"step_id", event.step_id},
                              {"metadata", event.metadata}};
        {
            std::lock_guard<std::mutex> lock(mu);
            events.push_back(row);
            if (!event.plan_id.empty()) plan_id = event.plan_id;
            if (event.type == EventType::PLAN_CREATED) plan_created = true;
            if (event.type == EventType::REFLECTION_REPLAN) ++reflections;
            if (event.type == EventType::STEP_STARTED) {
                const int step_type = event.metadata.value("step_type", -1);
                if (step_type == static_cast<int>(StepType::RETRIEVAL)) ++retrieval_starts;
                if (step_type == static_cast<int>(StepType::LLM)) ++llm_starts;
            }
            if (event.type == EventType::STEP_FAILED) {
                errors.push_back(event.metadata.value("error", ""));
            }
            if (event.type == EventType::PLAN_COMPLETED || event.type == EventType::PLAN_FAILED ||
                event.type == EventType::PLAN_ABORTED) {
                terminal = true;
                terminal_type = Thoth::engineEventTypeName(event.type);
            }
        }
        if (!path.empty()) {
            std::ofstream out(path, std::ios::app);
            if (out) out << row.dump() << "\n";
        }
        cv.notify_all();
    }
};

bool pinLlamaCppBackend(const std::string& chatUrl, const std::string& embedUrl, std::string& error);

bool prepareWorkspace(const Options& options, std::string& error) {
    std::string forbidden;
    if (forbiddenEnvSet(forbidden)) {
        error = "refusing to start because " + forbidden + " is set";
        return false;
    }
    const std::string rootText = disposableRoot();
    if (rootText.empty()) {
        error = "cannot read parent git SHA";
        return false;
    }
    const fs::path root(rootText);
    if (!ensureDisposableDir(root, root, error)) return false;
    if (!ensureDisposableDir(options.workspace, root, error)) return false;
    if (!ensureDisposableDir(options.logs, root, error)) return false;
    if (!isLexicallyInside(fs::path(options.logs).lexically_normal(),
                           fs::path(options.workspace).lexically_normal())) {
        error = "logs must be inside the workspace";
        return false;
    }
    if (!ensureRetrievalConfig(options.workspace, error)) return false;
    setenv("THOTH_WORKSPACE_PATH", options.workspace.c_str(), 1);
    setenv("THOTH_LOGS_PATH", options.logs.c_str(), 1);
    if (!pinLlamaCppBackend(options.chat_url, options.embed_url, error)) return false;
    return true;
}

bool pinLlamaCppBackend(const std::string& chatUrl, const std::string& embedUrl, std::string& error) {
    if (chatUrl.empty() || embedUrl.empty()) {
        error = "chat and embed URLs are required; inherited inference URLs are not used";
        return false;
    }
    if (setenv("THOTH_INFERENCE_BACKEND", "llama_cpp", 1) != 0 ||
        setenv("THOTH_EMBED_STRICT", "1", 1) != 0 ||
        setenv("THOTH_INFERENCE_BASE_URL", chatUrl.c_str(), 1) != 0 ||
        setenv("THOTH_EMBED_BASE_URL", embedUrl.c_str(), 1) != 0) {
        error = "could not set the llama.cpp inference environment";
        return false;
    }
    std::string resolveError;
    const auto backend = Thoth::tryResolveInferenceBackend(resolveError);
    const char* selected = std::getenv("THOTH_INFERENCE_BACKEND");
    const char* strict = std::getenv("THOTH_EMBED_STRICT");
    const char* chat = std::getenv("THOTH_INFERENCE_BASE_URL");
    const char* embed = std::getenv("THOTH_EMBED_BASE_URL");
    if (!backend || *backend != Thoth::InferenceBackend::LlamaCpp || !selected ||
        std::string(selected) != "llama_cpp" || !strict || std::string(strict) != "1" || !chat ||
        chatUrl != chat || !embed || embedUrl != embed) {
        error = resolveError.empty()
                    ? "incompatible inference backend; refusing to select Ollama"
                    : resolveError;
        return false;
    }
    try {
        Thoth::InferenceEndpointConfig endpoints;
        endpoints.base_url = chatUrl;
        endpoints.embed_base_url = embedUrl;
        const std::unique_ptr<Thoth::InferenceClient> client =
            Thoth::createInferenceClient(endpoints, nullptr);
        if (!client || client->backendName() != "llama_cpp") {
            error = "inference client is not llama.cpp";
            return false;
        }
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
    std::cerr << "RELIABILITY_BACKEND backend=llama_cpp embed_strict=1"
              << " chat=" << Thoth::inferenceUrl(chatUrl, "/v1/completions")
              << " chat_api=" << Thoth::inferenceUrl(chatUrl, "/v1/chat/completions")
              << " embed=" << Thoth::inferenceUrl(embedUrl, "/v1/embeddings") << "\n";
    return true;
}

std::string lastSkipReason(const fs::path& logs) {
    const std::string body = readFile(logs / "cognitive_metrics.jsonl");
    std::string last;
    std::istringstream lines(body);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty()) last = line;
    }
    if (last.empty()) return {};
    try {
        return nlohmann::json::parse(last).value("reflection_skip_reason", "");
    } catch (...) {
        return {};
    }
}

void installBoundary(BasicAgentPlugin& plugin, const Options& options) {
    if (options.boundary != "rl02" && options.boundary != "rl03") return;
    const fs::path boundary_path = fs::path(options.workspace) / "reliability_boundary.json";
    const std::string boundary = options.boundary;
    plugin.setActivePlanCommitObserverForTests([boundary, boundary_path](const auto& plan) {
        if (!boundaryMatches(boundary, plan.steps_json)) return;
        nlohmann::json marker = {{"boundary", boundary},
                                 {"plan_id", plan.plan_id},
                                 {"session_id", plan.session_id},
                                 {"steps_json", plan.steps_json}};
        writeAtomic(boundary_path, marker.dump(2));
        raise(SIGSTOP);
    });
}

int runGoal(const Options& options) {
    const auto started = std::chrono::steady_clock::now();
    const int wall_ms = options.max_wall_ms > 0 ? options.max_wall_ms : kRl01Sec * 1000;
    auto remainingMs = [&]() {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
        return wall_ms - static_cast<int>(elapsed);
    };
    std::string error;
    if (!prepareWorkspace(options, error)) {
        std::cerr << "reliability run: " << error << "\n";
        return 2;
    }
    EventLog log;
    log.path = fs::path(options.workspace) / "reliability_events.jsonl";
    BasicAgentPlugin plugin;
    installBoundary(plugin, options);
    plugin.onEvent = [&](const ControllerEvent& event) { log.record(event); };
    plugin.setSessionId(options.session);
    const std::string note = "Isolated reliability note. Marker RELIABILITYQX7 appears only here.\n";
    const auto accepted = plugin.createCorpusDocument("reliability_note.md", note, options.session);
    if (!Thoth::CorpusCreate::hasRequiredAcceptedFields(accepted, error)) {
        std::cerr << "reliability run: corpus rejected: " << error << "\n";
        return 2;
    }
    bool indexed = false;
    {
        const int index_budget = remainingMs();
        if (index_budget <= 0) {
            std::cerr << "reliability run: wall clock exhausted before indexing\n";
            return 4;
        }
        std::unique_lock<std::mutex> lock(log.mu);
        indexed = log.cv.wait_for(lock, std::chrono::milliseconds(index_budget), [&] {
            for (const auto& event : log.events) {
                if (event.value("type", "") == "INDEXING_COMPLETED") return true;
            }
            return false;
        });
    }
    if (!indexed) {
        const auto corpus = plugin.listCorpusDocuments();
        if (corpus.contains("documents")) {
            for (const auto& document : corpus["documents"]) {
                if (document.value("status", "") == "indexed") indexed = true;
            }
        }
    }
    if (!indexed) {
        std::cerr << "reliability run: fixture was not indexed\n";
        return 2;
    }
    plugin.executeGoal(kGoal);
    {
        const int goal_budget = remainingMs();
        if (goal_budget <= 0) {
            std::cerr << "reliability run: wall clock exhausted before the goal finished\n";
            return 4;
        }
        std::unique_lock<std::mutex> lock(log.mu);
        log.cv.wait_for(lock, std::chrono::milliseconds(goal_budget), [&] { return log.terminal; });
    }
    nlohmann::json result = {{"command", "run"},
                             {"terminal", log.terminal_type},
                             {"plan_id", log.plan_id},
                             {"reflections", log.reflections},
                             {"retrieval_starts", log.retrieval_starts},
                             {"llm_starts", log.llm_starts},
                             {"errors", log.errors},
                             {"reflection_skip_reason", lastSkipReason(options.logs)}};
    writeAtomic(fs::path(options.workspace) / "reliability_result.json", result.dump(2));
    return log.terminal ? 0 : 4;
}

int resumeGoal(const Options& options) {
    std::string error;
    if (!prepareWorkspace(options, error)) {
        std::cerr << "reliability resume: " << error << "\n";
        return 2;
    }
    EventLog log;
    log.path = fs::path(options.workspace) / "reliability_resume_events.jsonl";
    std::unordered_set<std::string> successStepIds;
    {
        Thoth::SQLiteMemoryRepository prior((fs::path(options.workspace) / "memory.db").string());
        if (const auto loaded = prior.getActivePlan(options.session)) {
            const auto plan = parsePlanJson(loaded->steps_json);
            if (plan.contains("steps") && plan["steps"].is_array()) {
                for (const auto& step : plan["steps"]) {
                    const std::string id = step.value("step_id", "");
                    if (!id.empty() && step.value("status", -1) == static_cast<int>(StepStatus::SUCCESS)) {
                        successStepIds.insert(id);
                    }
                }
            }
        }
    }
    BasicAgentPlugin plugin;
    plugin.onEvent = [&](const ControllerEvent& event) {
        if (event.metadata.value("resume_reconciliation", "") == "retrieval_requeued"
            && event.metadata.value("prior_status", "") == "RUNNING") {
            int persisted = -1;
            Thoth::SQLiteMemoryRepository reader((fs::path(options.workspace) / "memory.db").string());
            if (const auto loaded = reader.getActivePlan(options.session)) {
                const auto plan = parsePlanJson(loaded->steps_json);
                if (plan.contains("steps") && plan["steps"].is_array()) {
                    for (const auto& step : plan["steps"]) {
                        if (step.value("step_id", "") == event.step_id) persisted = step.value("status", -1);
                    }
                }
            }
            std::lock_guard<std::mutex> lock(log.mu);
            log.prior_running_recorded = true;
            log.requeued_persisted_status = persisted;
        }
        if (event.type == EventType::STEP_STARTED && successStepIds.count(event.step_id) != 0) {
            std::lock_guard<std::mutex> lock(log.mu);
            ++log.success_step_starts;
        }
        log.record(event);
    };
    plugin.setSessionId(options.session);
    plugin.checkResumablePlan();
    if (options.expect == "idle") {
        nlohmann::json result = {{"command", "resume"},
                                 {"expect", "idle"},
                                 {"plan_created", log.plan_created},
                                 {"step_starts", log.retrieval_starts + log.llm_starts},
                                 {"plan_id", log.plan_id}};
        writeAtomic(fs::path(options.workspace) / "reliability_resume_result.json", result.dump(2));
        return log.plan_created ? 2 : 0;
    }
    const int wall_ms = options.max_wall_ms > 0 ? options.max_wall_ms : kRl02ResumeSec * 1000;
    {
        std::unique_lock<std::mutex> lock(log.mu);
        log.cv.wait_for(lock, std::chrono::milliseconds(wall_ms), [&] { return log.terminal; });
    }
    nlohmann::json result = {{"command", "resume"},
                             {"terminal", log.terminal_type},
                             {"plan_id", log.plan_id},
                             {"retrieval_starts", log.retrieval_starts},
                             {"llm_starts", log.llm_starts},
                             {"reflections", log.reflections},
                             {"success_step_starts", log.success_step_starts},
                             {"prior_running_recorded", log.prior_running_recorded},
                             {"requeued_persisted_status", log.requeued_persisted_status},
                             {"plan_created", log.plan_created}};
    writeAtomic(fs::path(options.workspace) / "reliability_resume_result.json", result.dump(2));
    return log.terminal ? 0 : 4;
}

int selfCheckBoundary(const std::string& db_path, const std::string& session) {
    std::string error;
    const fs::path db(db_path);
    const std::string rootText = disposableRoot();
    if (rootText.empty() || !validateDisposablePath(db.parent_path(), rootText, error)) {
        std::cerr << "self-check-boundary: " << error << "\n";
        return 2;
    }
    Thoth::SQLiteMemoryRepository repo(db_path);
    repo.setActivePlanCommitObserverForTests([&](const auto& plan) {
        if (!matchesRl02(plan.steps_json)) return;
        raise(SIGSTOP);
    });
    Thoth::MemoryRepository::ActivePlanRecord pending;
    pending.plan_id = "boundary-pending";
    pending.session_id = session;
    pending.goal = "pending";
    pending.steps_json = R"({"steps":[{"type":1,"status":0},{"type":2,"status":0}]})";
    pending.controller_state = "IDLE";
    pending.updated_at_ms = 1;
    if (!repo.storeActivePlan(pending)) return 2;
    Thoth::MemoryRepository::ActivePlanRecord ready;
    ready.plan_id = "boundary-ready";
    ready.session_id = session;
    ready.goal = "ready";
    ready.steps_json =
        R"({"steps":[{"type":1,"status":2},{"type":2,"status":0}]})";
    ready.controller_state = "IDLE";
    ready.updated_at_ms = 2;
    repo.storeActivePlan(ready);
    return 3;
}

bool observerSelfCheck(const fs::path& dir, std::string& error) {
    const fs::path db = dir / "observer.db";
    Thoth::SQLiteMemoryRepository repo(db.string());
    int calls = 0;
    Thoth::MemoryRepository::ActivePlanRecord rec;
    rec.plan_id = "observer-plan";
    rec.session_id = "observer-session";
    rec.goal = "observer";
    rec.steps_json = R"({"steps":[]})";
    rec.controller_state = "IDLE";
    if (!repo.storeActivePlan(rec) || calls != 0) {
        error = "observer ran while unset";
        return false;
    }
    bool visible_during_callback = false;
    repo.setActivePlanCommitObserverForTests([&](const auto& plan) {
        ++calls;
        Thoth::SQLiteMemoryRepository reader(db.string());
        const auto loaded = reader.getActivePlan(plan.session_id);
        visible_during_callback = loaded && loaded->plan_id == plan.plan_id &&
                                  loaded->steps_json == plan.steps_json;
    });
    rec.goal = "observer-visible";
    if (!repo.storeActivePlan(rec) || calls != 1 || !visible_during_callback) {
        error = "committed row was not visible to a second connection inside the observer";
        return false;
    }
    repo.setActivePlanCommitObserverForTests({});
    if (!repo.storeActivePlan(rec) || calls != 1) {
        error = "observer stayed active after removal";
        return false;
    }
    repo.setActivePlanCommitObserverForTests([&](const auto&) { throw std::runtime_error("observer boom"); });
    rec.goal = "observer-throw";
    if (!repo.storeActivePlan(rec)) {
        error = "observer exception changed the commit result";
        return false;
    }
    Thoth::SQLiteMemoryRepository reader(db.string());
    const auto loaded = reader.getActivePlan(rec.session_id);
    if (!loaded || loaded->goal != "observer-throw") {
        error = "throwing observer lost the committed row";
        return false;
    }
    return true;
}

bool criteriaSelfCheck(std::string& error) {
    const auto rl01 = judgeRl01("PLAN_FAILED", 2, "max_reflections_exhausted",
                                {"Connection refused"}, false, 1);
    if (!rl01.accepted || rl01.classification != "controlled_failure_handling") {
        error = "RL-01 controlled-failure criterion failed";
        return false;
    }
    const auto recovered = judgeRl01("PLAN_COMPLETED", 0, "", {}, false, 1);
    if (recovered.accepted) {
        error = "RL-01 accepted a completed goal";
        return false;
    }
    const auto rl02 = judgeRl02("plan-a", "plan-a", 0, 1, "PLAN_COMPLETED");
    if (!rl02.accepted || rl02.classification != "successful_resume") {
        error = "RL-02 success criterion failed";
        return false;
    }
    const auto failed_resume = judgeRl02("plan-a", "plan-a", 0, 1, "PLAN_FAILED");
    if (failed_resume.accepted || failed_resume.classification != "controlled_terminal_outcome") {
        error = "RL-02 treated PLAN_FAILED as success";
        return false;
    }
    const auto rl04 = judgeRl04(false, 0, false);
    if (!rl04.accepted) {
        error = "RL-04 criterion failed";
        return false;
    }
    const auto restarted = judgeRl04(true, 0, false);
    if (restarted.accepted) {
        error = "RL-04 accepted a new plan";
        return false;
    }
    Rl03Evidence stall;
    stall.still_running = true;
    stall.boundary_step_still_running = true;
    const auto defect = judgeRl03(stall);
    if (defect.accepted || defect.classification != "defect_reproduced") {
        error = "RL-03 stall was classified as a pass";
        return false;
    }
    Rl03Evidence recovery;
    recovery.boundary_plan_id = "plan-rl03";
    recovery.resume_plan_id = "plan-rl03";
    recovery.terminal = "PLAN_COMPLETED";
    recovery.retrieval_starts = 1;
    recovery.llm_starts = 1;
    recovery.prior_running_recorded = true;
    recovery.requeued_pending_persisted = true;
    recovery.live_synthesis = true;
    const auto verified = judgeRl03(recovery);
    if (!verified.accepted || verified.classification != "recovery_verified") {
        error = "RL-03 recovery criterion failed";
        return false;
    }
    recovery.requeued_pending_persisted = false;
    const auto incomplete = judgeRl03(recovery);
    if (incomplete.accepted || incomplete.classification == "recovery_verified") {
        error = "RL-03 recovery accepted a resume that was not persisted as PENDING";
        return false;
    }
    if (matchesRl02(R"({"steps":[{"type":1,"status":2},{"type":2,"status":0}]})") != true) {
        error = "RL-02 predicate missed the committed boundary";
        return false;
    }
    if (matchesRl02(R"({"steps":[{"type":1,"status":2},{"type":2,"status":1}]})") != false) {
        error = "RL-02 predicate accepted a RUNNING step";
        return false;
    }
    if (matchesRl03(R"({"steps":[{"type":1,"status":1}]})") != true) {
        error = "RL-03 predicate missed RUNNING";
        return false;
    }
    return true;
}

class ResumeProbeTool : public ITool {
public:
    mutable std::atomic<int> executions{0};

    std::string name() const override { return "resume_reconcile_probe"; }
    std::string description() const override { return "Counts executions during resume reconciliation tests"; }
    nlohmann::json input_schema() const override { return nlohmann::json::object(); }
    bool requires_confirmation() const override { return false; }
    nlohmann::json execute(const nlohmann::json&) const override {
        executions.fetch_add(1);
        return {{"status", "success"}};
    }
};

class SilentPlanner : public IPlanner {
public:
    std::atomic<int> calls{0};
    Plan create_plan(const std::string&) override {
        calls.fetch_add(1);
        return {};
    }
    Plan revise_plan(const Plan&, const nlohmann::json&) override {
        calls.fetch_add(1);
        return {};
    }
};

struct ScopedEnv {
    std::string key;
    std::string previous;
    bool had = false;
    ScopedEnv(const char* name, const std::string& value) : key(name) {
        if (const char* current = std::getenv(name)) {
            had = true;
            previous = current;
        }
        setenv(name, value.c_str(), 1);
    }
    ~ScopedEnv() {
        if (had) setenv(key.c_str(), previous.c_str(), 1);
        else unsetenv(key.c_str());
    }
};

PlanStep makeStep(const std::string& id, StepType type, StepStatus status, const nlohmann::json& payload) {
    PlanStep step;
    step.step_id = id;
    step.description = id;
    step.type = type;
    step.status = status;
    step.payload = payload;
    step.tool = nlohmann::json::object();
    if (status == StepStatus::RUNNING) step.started_at_ms = 10;
    if (status == StepStatus::SUCCESS) {
        step.started_at_ms = 1;
        step.completed_at_ms = 2;
        step.result = {{"status", "success"}};
    }
    return step;
}

bool resumeReconcileSelfCheck(const fs::path& root, std::string& error) {
    const fs::path base = root / "resume-reconcile";
    std::error_code ec;
    fs::create_directories(base, ec);
    ScopedEnv workspace("THOTH_WORKSPACE_PATH", base.string());
    ScopedEnv logs("THOTH_LOGS_PATH", (base / "logs").string());
    fs::create_directories(base / "logs", ec);

    auto fail = [&](const std::string& why) {
        error = why;
        return false;
    };

    auto runBlocked = [&](const std::string& label, PlanStep orphan) {
        const fs::path dir = base / label;
        fs::create_directories(dir, ec);
        ScopedEnv caseWorkspace("THOTH_WORKSPACE_PATH", dir.string());
        Config config;
        config.database_path = (dir / "memory.db").string();
        auto memory = std::make_shared<Memory>(config);
        auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
        auto index = std::make_unique<IndexManager>(engine.get());
        index->init((dir / "rag_index.bin").string());
        auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get());
        auto tools = std::make_shared<ToolRegistry>();
        auto probeOwner = std::make_unique<ResumeProbeTool>();
        ResumeProbeTool* probe = probeOwner.get();
        tools->registerTool(std::move(probeOwner));
        auto planner = std::make_shared<SilentPlanner>();
        Thoth::ExecutiveController controller(planner, tools, rag, memory);
        controller.set_max_reflections(0);
        controller.set_session_id(label);
        std::mutex mu;
        std::vector<EventType> events;
        int planFailed = 0;
        int reflections = 0;
        int starts = 0;
        controller.set_event_callback([&](const ControllerEvent& event) {
            std::lock_guard<std::mutex> lock(mu);
            events.push_back(event.type);
            if (event.type == EventType::PLAN_FAILED) ++planFailed;
            if (event.type == EventType::REFLECTION_REPLAN) ++reflections;
            if (event.type == EventType::STEP_STARTED) ++starts;
        });
        Plan plan;
        plan.plan_id = "plan-" + label;
        plan.goal = "blocked resume " + label;
        plan.status = PlanStatus::ACTIVE;
        plan.steps.push_back(makeStep("already-done", StepType::RETRIEVAL, StepStatus::SUCCESS,
                                      {{"query", "done"}, {"top_k", 1}}));
        plan.steps.push_back(std::move(orphan));
        controller.resume_from_plan(plan);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const Plan current = controller.get_current_plan();
        if (controller.get_state() != Thoth::ControllerState::FAILED) return fail(label + " stayed runnable");
        if (current.plan_id != plan.plan_id) return fail(label + " changed plan id");
        if (probe->executions.load() != 0) return fail(label + " executed the probe tool");
        if (planner->calls.load() != 0 || reflections != 0) return fail(label + " planned or reflected");
        if (planFailed != 1 || starts != 0) return fail(label + " did not fail closed once");
        bool sawPrior = false;
        bool sawBlock = false;
        for (const auto& step : current.steps) {
            if (step.step_id == "already-done") {
                if (step.status != StepStatus::SUCCESS) return fail(label + " changed the successful step");
            }
            if (step.reasoning.find("resume_prior_status=RUNNING") != std::string::npos) sawPrior = true;
            if (step.result.is_object() && step.result.value("error", "") == "interrupted_step_not_replayed"
                && step.result.value("prior_status", "") == "RUNNING") {
                sawBlock = true;
                if (step.status != StepStatus::FAILED) return fail(label + " did not fail the orphan");
            }
        }
        if (!sawPrior || !sawBlock) return fail(label + " lost the original RUNNING evidence");
        controller.resume_from_plan(current);
        if (planFailed != 1 || planner->calls.load() != 0) return fail(label + " resumed a terminal plan again");
        return true;
    };

    {
        const fs::path dir = base / "retrieval";
        fs::create_directories(dir, ec);
        ScopedEnv caseWorkspace("THOTH_WORKSPACE_PATH", dir.string());
        Config config;
        config.database_path = (dir / "memory.db").string();
        auto memory = std::make_shared<Memory>(config);
        auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
        auto index = std::make_unique<IndexManager>(engine.get());
        index->init((dir / "rag_index.bin").string());
        CodeChunk chunk;
        chunk.fileName = "reliability_note.md";
        chunk.code = "Isolated reliability note. Marker RELIABILITYQX7 appears only here.";
        chunk.keyword_score = 1.0f;
        chunk.embedding = {1.0f};
        index->addChunkToIndex(std::move(chunk));
        auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get());
        auto tools = std::make_shared<ToolRegistry>();
        auto planner = std::make_shared<SilentPlanner>();
        Thoth::ExecutiveController controller(planner, tools, rag, memory);
        controller.set_max_reflections(0);
        controller.set_session_id("retrieval-resume");
        std::mutex mu;
        std::condition_variable cv;
        int retrievalStarts = 0;
        int successStarts = 0;
        int reflections = 0;
        bool sawRequeue = false;
        bool terminal = false;
        std::string firstPersistedStatus;
        if (auto* repo = memory->getSQLiteRepo()) {
            repo->setActivePlanCommitObserverForTests([&](const Thoth::SQLiteMemoryRepository::ActivePlanRecord& record) {
                if (!firstPersistedStatus.empty()) return;
                try {
                    const auto stored = nlohmann::json::parse(record.steps_json);
                    for (const auto& step : stored["steps"]) {
                        if (step.value("step_id", "") == "orphan-retrieval") {
                            firstPersistedStatus = std::to_string(step.value("status", -1));
                        }
                    }
                } catch (...) {
                }
            });
        }
        controller.set_event_callback([&](const ControllerEvent& event) {
            std::lock_guard<std::mutex> lock(mu);
            if (event.type == EventType::STEP_STARTED && event.step_id == "orphan-retrieval") ++retrievalStarts;
            if (event.type == EventType::STEP_STARTED && event.step_id == "already-done") ++successStarts;
            if (event.type == EventType::REFLECTION_REPLAN) ++reflections;
            if (event.type == EventType::STATE_CHANGED
                && event.metadata.value("resume_reconciliation", "") == "retrieval_requeued"
                && event.metadata.value("prior_status", "") == "RUNNING") {
                sawRequeue = true;
            }
            if (event.type == EventType::PLAN_COMPLETED || event.type == EventType::PLAN_FAILED) terminal = true;
            cv.notify_all();
        });
        Plan plan;
        plan.plan_id = "plan-retrieval-resume";
        plan.goal = "replay the interrupted retrieval";
        plan.status = PlanStatus::ACTIVE;
        plan.steps.push_back(makeStep("already-done", StepType::RETRIEVAL, StepStatus::SUCCESS,
                                      {{"query", "done"}, {"top_k", 1}}));
        plan.steps.push_back(makeStep("orphan-retrieval", StepType::RETRIEVAL, StepStatus::RUNNING,
                                      {{"query", "RELIABILITYQX7"}, {"top_k", 1}}));
        controller.resume_from_plan(plan);
        {
            std::unique_lock<std::mutex> lock(mu);
            cv.wait_for(lock, std::chrono::seconds(5), [&] { return terminal; });
        }
        const Plan current = controller.get_current_plan();
        if (current.plan_id != plan.plan_id) return fail("retrieval resume changed plan id");
        if (firstPersistedStatus != "0") return fail("retrieval was not persisted as PENDING before dispatch");
        if (retrievalStarts != 1) return fail("orphaned retrieval did not execute once");
        if (successStarts != 0) return fail("successful retrieval was dispatched again");
        if (reflections != 0 || planner->calls.load() != 0) return fail("retrieval resume reflected or replanned");
        if (controller.get_state() == Thoth::ControllerState::IDLE) return fail("retrieval resume remained IDLE");
        bool prior = false;
        for (const auto& step : current.steps) {
            if (step.step_id == "orphan-retrieval"
                && step.reasoning.find("resume_prior_status=RUNNING") != std::string::npos) {
                prior = true;
            }
        }
        if (!sawRequeue || !prior) return fail("retrieval resume lost the original RUNNING evidence");
        if (!terminal || controller.get_state() != Thoth::ControllerState::COMPLETED) {
            std::string detail = "retrieval resume did not complete state="
                + std::to_string(static_cast<int>(controller.get_state()));
            for (const auto& step : current.steps) {
                detail += " " + step.step_id + "=" + std::to_string(static_cast<int>(step.status));
                if (step.result.is_object()) detail += step.result.dump();
            }
            return fail(detail);
        }
    }

    PlanStep tool = makeStep("orphan-tool", StepType::TOOL, StepStatus::RUNNING,
                             {{"tool", "resume_reconcile_probe"}});
    if (!runBlocked("tool", std::move(tool))) return false;
    PlanStep toolRetrieval = makeStep("orphan-tool-retrieval", StepType::RETRIEVAL, StepStatus::RUNNING,
                                      {{"tool", "resume_reconcile_probe"}, {"query", "RELIABILITYQX7"}});
    if (!runBlocked("tool-retrieval", std::move(toolRetrieval))) return false;
    PlanStep llm = makeStep("orphan-llm", StepType::LLM, StepStatus::RUNNING, nlohmann::json::object());
    if (!runBlocked("llm", std::move(llm))) return false;
    PlanStep unknown = makeStep("orphan-unknown", static_cast<StepType>(99), StepStatus::RUNNING,
                                nlohmann::json::object());
    if (!runBlocked("unknown", std::move(unknown))) return false;
    PlanStep node = makeStep("orphan-node", StepType::NODE, StepStatus::RUNNING, nlohmann::json::object());
    if (!runBlocked("node", std::move(node))) return false;

    auto runTerminal = [&](const std::string& label, PlanStatus status, Thoth::ControllerState expected) {
        const fs::path dir = base / ("terminal-" + label);
        fs::create_directories(dir, ec);
        ScopedEnv caseWorkspace("THOTH_WORKSPACE_PATH", dir.string());
        Config config;
        config.database_path = (dir / "memory.db").string();
        auto memory = std::make_shared<Memory>(config);
        auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
        auto index = std::make_unique<IndexManager>(engine.get());
        index->init((dir / "rag_index.bin").string());
        auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get());
        auto tools = std::make_shared<ToolRegistry>();
        auto probeOwner = std::make_unique<ResumeProbeTool>();
        ResumeProbeTool* probe = probeOwner.get();
        tools->registerTool(std::move(probeOwner));
        auto planner = std::make_shared<SilentPlanner>();
        Thoth::ExecutiveController controller(planner, tools, rag, memory);
        controller.set_max_reflections(0);
        controller.set_session_id("terminal-" + label);
        std::mutex mu;
        int requeues = 0;
        int starts = 0;
        int reflections = 0;
        int planEvents = 0;
        controller.set_event_callback([&](const ControllerEvent& event) {
            std::lock_guard<std::mutex> lock(mu);
            if (event.type == EventType::STATE_CHANGED
                && event.metadata.value("resume_reconciliation", "") == "retrieval_requeued") {
                ++requeues;
            }
            if (event.type == EventType::STEP_STARTED) ++starts;
            if (event.type == EventType::REFLECTION_REPLAN) ++reflections;
            if (event.type == EventType::PLAN_CREATED || event.type == EventType::PLAN_COMPLETED
                || event.type == EventType::PLAN_FAILED || event.type == EventType::PLAN_REVISED) {
                ++planEvents;
            }
        });
        Plan plan;
        plan.plan_id = "plan-terminal-" + label;
        plan.goal = "terminal resume " + label;
        plan.status = status;
        plan.steps.push_back(makeStep("orphan-retrieval", StepType::RETRIEVAL, StepStatus::RUNNING,
                                      {{"query", "RELIABILITYQX7"}, {"top_k", 1}}));
        const std::string before = plan.steps.front().to_json().dump();
        controller.resume_from_plan(plan);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const Plan once = controller.get_current_plan();
        if (once.plan_id != plan.plan_id || once.status != status) return fail(label + " changed the terminal plan");
        if (once.steps.size() != 1 || once.steps.front().to_json().dump() != before) {
            return fail(label + " changed the orphaned retrieval");
        }
        if (controller.get_state() != expected) return fail(label + " did not stay terminal");
        if (requeues != 0 || starts != 0 || reflections != 0 || planEvents != 0) {
            return fail(label + " emitted reconciliation, dispatch, reflection, or a replacement plan");
        }
        if (probe->executions.load() != 0 || planner->calls.load() != 0) {
            return fail(label + " dispatched work");
        }
        if (auto active = memory->getActivePlan("terminal-" + label)) {
            const auto stored = nlohmann::json::parse(active->steps_json);
            if (stored.value("plan_id", "") != plan.plan_id
                || stored.value("status", -1) != static_cast<int>(status)
                || stored["steps"].size() != 1
                || stored["steps"][0].value("status", -1) != static_cast<int>(StepStatus::RUNNING)) {
                return fail(label + " persisted a different terminal plan");
            }
        } else {
            return fail(label + " did not keep the active plan");
        }
        controller.resume_from_plan(once);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const Plan twice = controller.get_current_plan();
        if (twice.plan_id != plan.plan_id || twice.status != status
            || twice.steps.front().to_json().dump() != before) {
            return fail(label + " second resume was not idempotent");
        }
        if (controller.get_state() != expected || requeues != 0 || starts != 0
            || reflections != 0 || planEvents != 0 || planner->calls.load() != 0) {
            return fail(label + " second resume restarted the plan");
        }
        return true;
    };

    if (!runTerminal("completed", PlanStatus::COMPLETED, Thoth::ControllerState::COMPLETED)) return false;
    if (!runTerminal("failed", PlanStatus::FAILED, Thoth::ControllerState::FAILED)) return false;
    if (!runTerminal("aborted", PlanStatus::ABORTED, Thoth::ControllerState::ABORTED)) return false;
    return true;
}

int selfCheck() {
    const std::string rootText = disposableRoot();
    if (rootText.empty()) {
        std::cerr << "self-check: cannot read parent SHA\n";
        return 2;
    }
    const fs::path root(rootText);
    std::string error;
    if (!ensureDisposableDir(root, root, error)) {
        std::cerr << "self-check: " << error << "\n";
        return 2;
    }
    const fs::path work = root / ("self-check-" + std::to_string(getpid()));
    const fs::path logs = work / "logs";
    if (!ensureDisposableDir(work, root, error) || !ensureDisposableDir(logs, root, error)) {
        std::cerr << "self-check: " << error << "\n";
        return 2;
    }

    const fs::path devConfig = fs::path(THOTH_SOURCE_DIR) / "agent_workspace" / "retrieval_config.json";
    const std::string devBefore = readFile(devConfig);
    const uint64_t devHash = fnv1a(devBefore);

    auto fail = [&](const std::string& why) {
        std::cerr << "self-check: " << why << "\n";
        std::string ignored;
        removeOwnedTree(work, root, ignored);
        return 2;
    };

    std::string rejected;
    if (validateDisposablePath("relative/workspace", root, rejected)) return fail("accepted a relative path");
    if (validateDisposablePath(fs::path(THOTH_SOURCE_DIR) / "agent_workspace", root, rejected)) {
        return fail("accepted the DEV workspace");
    }
    if (validateDisposablePath("/tmp/not-the-reliability-root", root, rejected)) {
        return fail("accepted a path outside the disposable root");
    }
    const fs::path link = work / "escape-link";
    std::error_code ec;
    fs::create_symlink(fs::path(THOTH_SOURCE_DIR) / "agent_workspace", link, ec);
    if (!ec && validateDisposablePath(link, root, rejected)) return fail("accepted a symlink");

    if (!ensureRetrievalConfig(work, error)) return fail(error);
    const fs::path configPath = work / "retrieval_config.json";
    const std::string freshBytes = readFile(configPath);
    const auto freshWritten = nlohmann::json::parse(freshBytes);
    if (!trajectoryIsProductionDefault(freshWritten["retrieval_weights"].value("trajectory", 0.0))) {
        return fail("fresh trajectory weight was not -0.05");
    }
    const auto freshMtime = fs::last_write_time(configPath);
    if (!ensureRetrievalConfig(work, error)) return fail(error);
    if (readFile(configPath) != freshBytes || fs::last_write_time(configPath) != freshMtime) {
        return fail("valid retrieval config was rewritten");
    }
    const std::string preservedBytes =
        "{\n  \"note\": \"preserve-me\",\n  \"retrieval_weights\": {\"trajectory\": -0.05}\n}\n";
    {
        std::ofstream out(configPath, std::ios::trunc);
        out << preservedBytes;
    }
    const auto preservedMtime = fs::last_write_time(configPath);
    if (!ensureRetrievalConfig(work, error)) return fail("valid existing retrieval config was rejected");
    if (readFile(configPath) != preservedBytes || fs::last_write_time(configPath) != preservedMtime) {
        return fail("resume initialization changed an existing retrieval config");
    }
    const std::string invalidBytes = "{\"retrieval_weights\":{\"trajectory\":0.2}}\n";
    {
        std::ofstream out(configPath, std::ios::trunc);
        out << invalidBytes;
    }
    if (ensureRetrievalConfig(work, error) || readFile(configPath) != invalidBytes) {
        return fail("invalid retrieval config was accepted or rewritten");
    }
    fs::remove(configPath, ec);
    if (fnv1a(readFile(devConfig)) != devHash) return fail("DEV retrieval config changed");
    if (!resumeReconcileSelfCheck(work, error)) return fail(error);

    setenv("THOTH_INFERENCE_BACKEND", "ollama", 1);
    unsetenv("THOTH_EMBED_STRICT");
    setenv("THOTH_INFERENCE_BASE_URL", "http://inherited.invalid", 1);
    setenv("THOTH_EMBED_BASE_URL", "http://inherited-embed.invalid", 1);
    if (pinLlamaCppBackend("", "http://127.0.0.1:9", error)) {
        return fail("missing chat URL was accepted");
    }
    if (!std::getenv("THOTH_INFERENCE_BACKEND") ||
        std::string(std::getenv("THOTH_INFERENCE_BACKEND")) != "ollama") {
        return fail("rejected backend setup changed the inherited backend");
    }
    const char* labels[] = {"run", "resume"};
    std::string firstBackend;
    std::string firstStrict;
    std::string firstChat;
    std::string firstEmbed;
    for (const char* label : labels) {
        Options probe;
        probe.workspace = (work / label).string();
        probe.logs = (work / label / "logs").string();
        probe.chat_url = "http://172.18.0.3:8080";
        probe.embed_url = "http://172.18.0.2:8081";
        setenv("THOTH_INFERENCE_BACKEND", "ollama", 1);
        unsetenv("THOTH_EMBED_STRICT");
        if (!prepareWorkspace(probe, error)) return fail(error);
        const char* backend = std::getenv("THOTH_INFERENCE_BACKEND");
        const char* strict = std::getenv("THOTH_EMBED_STRICT");
        const char* chat = std::getenv("THOTH_INFERENCE_BASE_URL");
        const char* embed = std::getenv("THOTH_EMBED_BASE_URL");
        if (!backend || !strict || !chat || !embed) return fail("backend environment was not set");
        if (firstBackend.empty()) {
            firstBackend = backend;
            firstStrict = strict;
            firstChat = chat;
            firstEmbed = embed;
        } else if (firstBackend != backend || firstStrict != strict || firstChat != chat ||
                   firstEmbed != embed) {
            return fail("run and resume backend settings differ");
        }
        const std::string retrieval = readFile(fs::path(probe.workspace) / "retrieval_config.json");
        if (!trajectoryIsProductionDefault(
                nlohmann::json::parse(retrieval)["retrieval_weights"].value("trajectory", 0.0))) {
            return fail("backend probe changed the trajectory weight");
        }
    }
    if (firstBackend != "llama_cpp" || firstStrict != "1" || firstChat != "http://172.18.0.3:8080" ||
        firstEmbed != "http://172.18.0.2:8081") {
        return fail("backend probe did not select llama.cpp with strict embeddings");
    }

    if (!observerSelfCheck(work, error)) return fail(error);
    if (!criteriaSelfCheck(error)) return fail(error);
    ChildProcess forbiddenGroup;
    forbiddenGroup.pid = 1;
    forbiddenGroup.pgid = 1;
    if (ownershipOk(forbiddenGroup)) return fail("process-group guard accepted pid 1");
    forbiddenGroup.pid = getpgrp();
    forbiddenGroup.pgid = getpgrp();
    if (ownershipOk(forbiddenGroup)) return fail("process-group guard accepted the supervisor PGID");

    std::string refused_error;
    if (!connectionRefused("http://127.0.0.1:1", refused_error)) {
        return fail("local refusal probe failed: " + refused_error);
    }

    SigchldBlock block;
    const ChildProcess ungrouped = spawnWithoutGroup();
    if (ungrouped.pid <= 1 || ownershipOk(ungrouped)) {
        int ignored = 0;
        std::string cleanupError;
        terminateChild(ungrouped, ignored, cleanupError);
        return fail("ungrouped child was accepted as an owned process group");
    }
    {
        int status = 0;
        std::string cleanupError;
        if (!terminateChild(ungrouped, status, cleanupError)) {
            return fail("direct-child cleanup failed: " + cleanupError);
        }
    }
    if (isOurChild(ungrouped.pid)) return fail("ungrouped child was not reaped");

    ChildProcess innocent;
    innocent.pid = fork();
    if (innocent.pid < 0) return fail("cannot fork innocent process");
    if (innocent.pid == 0) {
        if (setpgid(0, 0) != 0) _exit(127);
        for (;;) pause();
    }
    if (setpgid(innocent.pid, innocent.pid) != 0 && getpgid(innocent.pid) != innocent.pid) {
        int ignored = 0;
        std::string cleanupError;
        terminateChild(innocent, ignored, cleanupError);
        return fail("innocent setpgid failed");
    }
    innocent.pgid = getpgid(innocent.pid);
    innocent.group_ready = innocent.pgid == innocent.pid && innocent.pgid != getpgrp();
    logProcess(innocent);

    const std::string db = (work / "boundary.db").string();
    const std::vector<std::string> args = {exePath(), "self-check-boundary", "--db", db, "--session",
                                           "boundary-session"};
    const ChildProcess victim = spawnGroup(args);
    auto releaseBoth = [&]() {
        int ignored = 0;
        std::string cleanupError;
        terminateChild(victim, ignored, cleanupError);
        terminateChild(innocent, ignored, cleanupError);
    };
    if (!victim.group_ready) {
        releaseBoth();
        return fail("cannot establish boundary child process group");
    }
    int status = 0;
    const WaitKind kind = waitChild(victim.pid, 10000, status);
    if (kind != WaitKind::Stopped) {
        releaseBoth();
        return fail("boundary child did not stop on the committed predicate");
    }
    {
        Thoth::SQLiteMemoryRepository reader(db);
        const auto loaded = reader.getActivePlan("boundary-session");
        if (!loaded || !matchesRl02(loaded->steps_json)) {
            releaseBoth();
            return fail("second connection did not see the RL-02 boundary");
        }
    }
    if (!isOurChild(innocent.pid)) {
        releaseBoth();
        return fail("innocent process died before the group signal");
    }
    {
        std::string cleanupError;
        if (!ownershipOk(victim) || !terminateChild(victim, status, cleanupError)) {
            terminateChild(victim, status, cleanupError);
            terminateChild(innocent, status, cleanupError);
            return fail("could not signal the test process group");
        }
    }
    if (!isOurChild(innocent.pid)) return fail("group signal reached the innocent process");
    {
        std::string cleanupError;
        if (!terminateChild(innocent, status, cleanupError)) return fail(cleanupError);
    }

    const fs::path sentinel = fs::path("/tmp") / ("thoth-reliability-sentinel-" + std::to_string(getpid()));
    {
        std::ofstream out(sentinel);
        out << "keep\n";
    }
    if (!removeOwnedTree(work, root, error)) return fail(error);
    if (!fs::exists(sentinel)) return fail("cleanup removed a file outside the disposable tree");
    fs::remove(sentinel, ec);
    if (removeOwnedTree(fs::path(THOTH_SOURCE_DIR) / "agent_workspace", root, error)) {
        return fail("cleanup accepted the DEV workspace");
    }
    if (fnv1a(readFile(devConfig)) != devHash) return fail("DEV retrieval config changed during self-check");
    fs::remove(root, ec);

    std::cout << "SELF_CHECK_OK root=" << root << " total_cap_sec=" << kTotalCapSec << "\n";
    return 0;
}

bool readResult(const fs::path& path, nlohmann::json& out) {
    try {
        out = nlohmann::json::parse(readFile(path));
        return !out.is_null();
    } catch (...) {
        return false;
    }
}

bool releaseChild(const ChildProcess& child) {
    if (child.pid <= 0) return true;
    int status = 0;
    std::string cleanupError;
    return terminateChild(child, status, cleanupError);
}

int superviseCase(const Options& options) {
    std::string error;
    const std::string root = disposableRoot();
    if (root.empty() || !ensureDisposableDir(options.workspace, root, error) ||
        !ensureDisposableDir(options.logs, root, error)) {
        std::cerr << "supervise: " << error << "\n";
        return 2;
    }
    if (options.case_name == "rl-01") {
        if (!connectionRefused(options.chat_url, error)) {
            std::cerr << "supervise rl-01: " << error << "\n";
            return 2;
        }
    }
    const std::string exe = exePath();
    SigchldBlock block;
    if (options.case_name == "rl-01") {
        Options run = options;
        run.boundary = "none";
        run.max_wall_ms = kRl01Sec * 1000;
        const ChildProcess child = spawnGroup({exe, "run", "--workspace", run.workspace, "--logs", run.logs,
                                                "--session", run.session, "--chat-url", run.chat_url, "--embed-url",
                                                run.embed_url, "--boundary", "none", "--max-wall-ms",
                                                std::to_string(run.max_wall_ms)});
        if (!child.group_ready) {
            releaseChild(child);
            std::cerr << "supervise rl-01: process group was not established\n";
            return 2;
        }
        int status = 0;
        const WaitKind kind = waitChild(child.pid, (kRl01Sec + 15) * 1000, status);
        if (kind == WaitKind::TimedOut && !releaseChild(child)) return 2;
        if (kind != WaitKind::TimedOut && !releaseChild(child)) return 2;
        nlohmann::json result;
        if (!readResult(fs::path(run.workspace) / "reliability_result.json", result)) return 2;
        Thoth::SQLiteMemoryRepository reader((fs::path(run.workspace) / "memory.db").string());
        const bool active = reader.getActivePlan(run.session).has_value();
        const int past = static_cast<int>(reader.getAllPastPlans(2).size());
        std::vector<std::string> errors;
        for (const auto& item : result.value("errors", nlohmann::json::array())) {
            errors.push_back(item.get<std::string>());
        }
        const Verdict verdict = judgeRl01(result.value("terminal", ""), result.value("reflections", 0),
                                          result.value("reflection_skip_reason", ""), errors, active, past);
        std::cout << verdict.classification << " " << verdict.detail << "\n";
        return verdict.accepted ? 0 : 1;
    }
    if (options.case_name == "rl-02") {
        const ChildProcess child = spawnGroup({exe, "run", "--workspace", options.workspace, "--logs", options.logs,
                                                "--session", options.session, "--chat-url", options.chat_url,
                                                "--embed-url", options.embed_url, "--boundary", "rl02",
                                                "--max-wall-ms", std::to_string(kRl02RunSec * 1000)});
        if (!child.group_ready) {
            releaseChild(child);
            std::cerr << "supervise rl-02: process group was not established\n";
            return 2;
        }
        int status = 0;
        const WaitKind kind = waitChild(child.pid, kRl02RunSec * 1000, status);
        if (kind != WaitKind::Stopped) {
            releaseChild(child);
            std::cerr << "supervise rl-02: committed boundary was not observed\n";
            return 2;
        }
        Thoth::SQLiteMemoryRepository reader((fs::path(options.workspace) / "memory.db").string());
        const auto loaded = reader.getActivePlan(options.session);
        if (!loaded || !matchesRl02(loaded->steps_json)) {
            releaseChild(child);
            std::cerr << "supervise rl-02: second connection rejected the boundary\n";
            return 2;
        }
        const std::string plan_id = loaded->plan_id;
        if (!releaseChild(child)) return 2;
        const ChildProcess resume = spawnGroup({exe, "resume", "--workspace", options.workspace, "--logs",
                                                 options.logs, "--session", options.session, "--chat-url",
                                                 options.chat_url, "--embed-url", options.embed_url, "--expect",
                                                 "terminal", "--max-wall-ms", std::to_string(kRl02ResumeSec * 1000)});
        if (!resume.group_ready) {
            releaseChild(resume);
            std::cerr << "supervise rl-02: resume process group was not established\n";
            return 2;
        }
        int resume_status = 0;
        const WaitKind resume_kind = waitChild(resume.pid, kRl02ResumeSec * 1000, resume_status);
        (void)resume_kind;
        if (!releaseChild(resume)) return 2;
        nlohmann::json result;
        if (!readResult(fs::path(options.workspace) / "reliability_resume_result.json", result)) return 2;
        const Verdict verdict = judgeRl02(plan_id, result.value("plan_id", ""),
                                          result.value("retrieval_starts", -1), result.value("llm_starts", -1),
                                          result.value("terminal", ""));
        std::cout << verdict.classification << " " << verdict.detail << "\n";
        return verdict.accepted ? 0 : 1;
    }
    if (options.case_name == "rl-04") {
        const ChildProcess child = spawnGroup({exe, "resume", "--workspace", options.workspace, "--logs",
                                                options.logs, "--session", options.session, "--chat-url",
                                                options.chat_url, "--embed-url", options.embed_url, "--expect",
                                                "idle", "--max-wall-ms", std::to_string(kRl04Sec * 1000)});
        if (!child.group_ready) {
            releaseChild(child);
            std::cerr << "supervise rl-04: process group was not established\n";
            return 2;
        }
        int status = 0;
        const WaitKind kind = waitChild(child.pid, kRl04Sec * 1000, status);
        if (kind == WaitKind::TimedOut && !releaseChild(child)) return 2;
        if (kind != WaitKind::TimedOut && !releaseChild(child)) return 2;
        nlohmann::json result;
        if (!readResult(fs::path(options.workspace) / "reliability_resume_result.json", result)) return 2;
        Thoth::SQLiteMemoryRepository reader((fs::path(options.workspace) / "memory.db").string());
        const Verdict verdict = judgeRl04(result.value("plan_created", true), result.value("step_starts", 1),
                                          reader.getActivePlan(options.session).has_value());
        std::cout << verdict.classification << " " << verdict.detail << "\n";
        return verdict.accepted ? 0 : 1;
    }
    if (options.case_name == "rl-03") {
        const ChildProcess child = spawnGroup({exe, "run", "--workspace", options.workspace, "--logs", options.logs,
                                                "--session", options.session, "--chat-url", options.chat_url,
                                                "--embed-url", options.embed_url, "--boundary", "rl03",
                                                "--max-wall-ms", std::to_string(kRl03RunSec * 1000)});
        if (!child.group_ready) {
            releaseChild(child);
            std::cerr << "supervise rl-03: process group was not established\n";
            return 2;
        }
        int status = 0;
        const WaitKind kind = waitChild(child.pid, kRl03RunSec * 1000, status);
        if (kind != WaitKind::Stopped) {
            releaseChild(child);
            std::cerr << "supervise rl-03: RUNNING commit was not observed\n";
            return 2;
        }
        {
            Thoth::SQLiteMemoryRepository reader((fs::path(options.workspace) / "memory.db").string());
            const auto loaded = reader.getActivePlan(options.session);
            if (!loaded || !matchesRl03(loaded->steps_json)) {
                releaseChild(child);
                std::cerr << "supervise rl-03: second connection rejected the RUNNING commit\n";
                return 2;
            }
        }
        if (!releaseChild(child)) return 2;
        const ChildProcess resume = spawnGroup(
            {exe, "resume", "--workspace", options.workspace, "--logs", options.logs, "--session", options.session,
             "--chat-url", options.chat_url, "--embed-url", options.embed_url, "--expect", "terminal",
             "--max-wall-ms", std::to_string(kRl03ResumeBackstopSec * 1000)});
        if (!resume.group_ready) {
            releaseChild(resume);
            std::cerr << "supervise rl-03: resume process group was not established\n";
            return 2;
        }
        int resume_status = 0;
        const WaitKind resume_kind = waitChild(resume.pid, kRl03CharSec * 1000, resume_status);
        const bool still_running = resume_kind == WaitKind::TimedOut && isOurChild(resume.pid);
        bool step_running = false;
        {
            Thoth::SQLiteMemoryRepository reader((fs::path(options.workspace) / "memory.db").string());
            const auto loaded = reader.getActivePlan(options.session);
            step_running = loaded && matchesRl03(loaded->steps_json);
        }
        nlohmann::json result;
        const bool have_result =
            readResult(fs::path(options.workspace) / "reliability_resume_result.json", result);
        nlohmann::json boundary;
        readResult(fs::path(options.workspace) / "reliability_boundary.json", boundary);
        bool live_synthesis = false;
        {
            std::istringstream lines(readFile(fs::path(options.logs) / "generation_calls.jsonl"));
            std::string line;
            while (std::getline(lines, line)) {
                try {
                    const auto call = nlohmann::json::parse(line);
                    if (call.value("call_type", "") == "synthesis" && call.value("provider_ok", false)
                        && call.value("elapsed_ms", 0) > 0
                        && call.value("plan_id", "") == boundary.value("plan_id", "")) {
                        live_synthesis = true;
                    }
                } catch (...) {
                }
            }
        }
        if (!releaseChild(resume)) return 2;
        Rl03Evidence evidence;
        evidence.still_running = still_running;
        evidence.boundary_step_still_running = step_running;
        evidence.boundary_plan_id = boundary.value("plan_id", "");
        evidence.resume_plan_id = have_result ? result.value("plan_id", "") : "";
        evidence.terminal = have_result ? result.value("terminal", "") : "";
        evidence.retrieval_starts = have_result ? result.value("retrieval_starts", 0) : 0;
        evidence.llm_starts = have_result ? result.value("llm_starts", 0) : 0;
        evidence.reflections = have_result ? result.value("reflections", 0) : 0;
        evidence.success_step_starts = have_result ? result.value("success_step_starts", 0) : 0;
        evidence.prior_running_recorded = have_result && result.value("prior_running_recorded", false);
        evidence.requeued_pending_persisted =
            have_result && result.value("requeued_persisted_status", -1) == static_cast<int>(StepStatus::PENDING);
        evidence.live_synthesis = live_synthesis;
        const Verdict verdict = judgeRl03(evidence);
        std::cout << verdict.classification << " " << verdict.detail << "\n";
        if (verdict.classification == "recovery_verified") return 0;
        if (verdict.classification == "defect_reproduced") return 3;
        return 1;
    }
    std::cerr << "supervise: unknown case\n";
    return 2;
}

int superviseSequence(const Options& base) {
    if (base.workspace.empty() || base.chat_url.empty() || base.live_chat_url.empty() ||
        base.embed_url.empty()) {
        std::cerr << "supervise --sequence needs --workspace, --chat-url (refused), "
                     "--live-chat-url, and --embed-url\n";
        return 2;
    }
    const fs::path root(base.workspace);
    struct Step {
        const char* name;
        const char* session;
        const char* dir;
        bool refused_chat;
    };
    const Step steps[] = {
        {"rl-01", "reliability-rl01", "rl-01", true},
        {"rl-02", "reliability-rl02", "rl-02", false},
        {"rl-04", "reliability-rl02", "rl-02", false},
        {"rl-03", "reliability-rl03", "rl-03", false},
    };
    for (const Step& step : steps) {
        Options next = base;
        next.sequence = false;
        next.case_name = step.name;
        next.session = step.session;
        next.workspace = (root / step.dir).string();
        next.logs = (root / step.dir / "logs").string();
        next.chat_url = step.refused_chat ? base.chat_url : base.live_chat_url;
        std::cout << "RELIABILITY_CASE_BEGIN " << step.name << "\n";
        const int rc = superviseCase(next);
        std::cout << "RELIABILITY_CASE_END " << step.name << " exit=" << rc << "\n";
        if (step.name == std::string("rl-02") && rc != 0) {
            std::cerr << "supervise: RL-04 and RL-03 stay unstarted after RL-02 did not complete\n";
            return rc;
        }
        if (step.name == std::string("rl-04") && rc != 0) {
            std::cerr << "supervise: RL-03 stays unstarted after RL-04 did not stay terminal\n";
            return rc;
        }
        if (rc != 0 && std::string(step.name) != "rl-03") return rc;
        if (std::string(step.name) == "rl-03") return rc;
    }
    return 2;
}

Options parse(int argc, char** argv) {
    Options options;
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        auto need = [&](std::string& dest) {
            if (i + 1 >= argc) return;
            dest = argv[++i];
        };
        if (key == "--workspace") need(options.workspace);
        else if (key == "--logs") need(options.logs);
        else if (key == "--session") need(options.session);
        else if (key == "--chat-url") need(options.chat_url);
        else if (key == "--live-chat-url") need(options.live_chat_url);
        else if (key == "--embed-url") need(options.embed_url);
        else if (key == "--boundary") need(options.boundary);
        else if (key == "--expect") need(options.expect);
        else if (key == "--case") need(options.case_name);
        else if (key == "--db") need(options.workspace);
        else if (key == "--max-wall-ms") {
            if (i + 1 < argc) options.max_wall_ms = std::stoi(argv[++i]);
        } else if (key == "--sequence") options.sequence = true;
    }
    return options;
}

void usage() {
    std::cerr << "Usage:\n"
              << "  thoth-reliability-harness self-check\n"
              << "  thoth-reliability-harness run --workspace PATH --logs PATH --session ID "
                 "--chat-url URL --embed-url URL [--boundary none|rl02|rl03]\n"
              << "  thoth-reliability-harness resume --workspace PATH --logs PATH --session ID "
                 "--chat-url URL --embed-url URL [--expect terminal|idle]\n"
              << "  thoth-reliability-harness supervise --case rl-01|rl-02|rl-04|rl-03 "
                 "--workspace PATH --logs PATH --session ID --chat-url URL --embed-url URL\n"
              << "  thoth-reliability-harness supervise --sequence --workspace ROOT "
                 "--chat-url http://127.0.0.1/<refused> --live-chat-url URL --embed-url URL\n"
              << "Live cases are opt-in. self-check does not execute them.\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::string command = argv[1];
    if (command == "self-check") return selfCheck();
    const Options options = parse(argc, argv);
    if (command == "self-check-boundary") return selfCheckBoundary(options.workspace, options.session);
    if (command == "run") return runGoal(options);
    if (command == "resume") return resumeGoal(options);
    if (command == "supervise") {
        if (options.sequence) return superviseSequence(options);
        return superviseCase(options);
    }
    usage();
    return 2;
}
