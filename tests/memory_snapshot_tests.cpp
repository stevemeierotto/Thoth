/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — EGAR-LX synthetic memory snapshot tests
 *
 * Each CTest invocation runs one case in a fresh process.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "../external/basic_agent/include/config.h"
#include "../external/basic_agent/include/executive_controller.h"
#include "../external/basic_agent/include/iplanner.h"
#include "../external/basic_agent/include/memory.h"
#include "../external/basic_agent/include/memory_snapshot.h"
#include "../external/basic_agent/include/plan.h"
#include "../external/basic_agent/include/tools.h"

#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

void fail(const std::string& message) {
    std::cerr << message << "\n";
}

struct FileSeal {
    bool present = false;
    std::string bytes;
};

FileSeal sealFile(const fs::path& path) {
    FileSeal seal;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return seal;
    }
    seal.present = true;
    seal.bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return seal;
}

bool sameSeal(const FileSeal& before, const FileSeal& after) {
    return before.present == after.present && before.bytes == after.bytes;
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

struct SourceTrio {
    fs::path database;
    FileSeal db;
    FileSeal wal;
    FileSeal shm;
};

SourceTrio sealSource(const fs::path& database) {
    SourceTrio trio;
    trio.database = database;
    trio.db = sealFile(database);
    trio.wal = sealFile(fs::path(database.string() + "-wal"));
    trio.shm = sealFile(fs::path(database.string() + "-shm"));
    return trio;
}

bool sourceUnchanged(const SourceTrio& before, const std::string& label) {
    const SourceTrio after = sealSource(before.database);
    if (!sameSeal(before.db, after.db) || !sameSeal(before.wal, after.wal)
        || !sameSeal(before.shm, after.shm)) {
        fail(label + ": source database changed");
        return false;
    }
    return true;
}

fs::path caseRoot(const std::string& label) {
    const fs::path root = fs::temp_directory_path() / "thoth_lx_snapshot" / label;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

bool execSql(sqlite3* db, const char* sql) {
    char* message = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &message);
    if (rc != SQLITE_OK) {
        std::cerr << "sql failed: " << (message ? message : "") << "\n";
        sqlite3_free(message);
        return false;
    }
    return true;
}

bool createWalDatabase(const fs::path& database, bool insert_fact, bool leave_uncheckpointed) {
    sqlite3* db = nullptr;
    if (sqlite3_open(database.c_str(), &db) != SQLITE_OK) {
        fail(std::string("fixture open failed: ") + (db ? sqlite3_errmsg(db) : "open"));
        if (db) {
            sqlite3_close(db);
        }
        return false;
    }
    if (leave_uncheckpointed) {
        int prior = 0;
        sqlite3_db_config(db, SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE, 1, &prior);
    }
    const bool schema_ok = execSql(db, "PRAGMA journal_mode=WAL;")
        && execSql(db,
            "CREATE TABLE trajectories (trajectory_id TEXT PRIMARY KEY);"
            "CREATE TABLE strategies (strategy_id TEXT PRIMARY KEY);"
            "CREATE TABLE past_plans (plan_id TEXT PRIMARY KEY);"
            "CREATE TABLE graph_edges (from_id TEXT, to_id TEXT, PRIMARY KEY (from_id, to_id));"
            "CREATE TABLE facts ("
            "  key TEXT PRIMARY KEY,"
            "  value TEXT NOT NULL,"
            "  confidence REAL DEFAULT 1.0,"
            "  source TEXT,"
            "  last_updated_ms INTEGER NOT NULL);");
    const bool write_ok = !insert_fact
        || execSql(db, "INSERT INTO facts (key, value, last_updated_ms) VALUES ('k', 'v', 1);");
    sqlite3_close(db);
    return schema_ok && write_ok;
}

struct EngineRun {
    int status = -1;
    bool timed_out = false;
    std::string output;
};

EngineRun runEngine(const std::string& binary, const fs::path& workspace, const fs::path& index_dir) {
    EngineRun result;
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
        setenv("THOTH_LOGS_PATH", (workspace / "logs").c_str(), 1);
        setenv("THOTH_TEST_SUITE_DEV", "1", 1);
        unsetenv("THOTH_LOG_CONFIG");
        unsetenv("THOTH_ALP_ENABLED");
        unsetenv("THOTH_ALP_TX_INDEX");
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);
        const std::string index = index_dir.string();
        char* argv[] = {
            const_cast<char*>(binary.c_str()),
            const_cast<char*>("--index-directory"),
            const_cast<char*>(index.c_str()),
            nullptr
        };
        execv(binary.c_str(), argv);
        _exit(127);
    }
    close(pipes[1]);
    const int flags = fcntl(pipes[0], F_GETFL, 0);
    fcntl(pipes[0], F_SETFL, flags | O_NONBLOCK);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    bool exited = false;
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd fd{};
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
        if (waitpid(pid, &status, WNOHANG) == pid) {
            exited = true;
            break;
        }
    }
    if (!exited) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        result.timed_out = true;
    }
    close(pipes[0]);
    result.status = status;
    return result;
}

bool prepareEngineDatabase(const std::string& binary, const fs::path& workspace, std::string& error) {
    fs::create_directories(workspace / "evidence");
    const EngineRun run = runEngine(binary, workspace, workspace / "evidence");
    if (run.timed_out || !WIFEXITED(run.status) || WEXITSTATUS(run.status) != 0) {
        error = "engine exit failed: " + run.output;
        return false;
    }
    if (!fs::is_regular_file(workspace / "memory.db")
        || !fs::is_regular_file(fs::path((workspace / "memory.db").string() + "-wal"))) {
        error = "engine did not leave a database and WAL";
        return false;
    }
    return true;
}

PlanStep toolStep() {
    PlanStep step;
    step.step_id = "goal-tool";
    step.description = "synthetic tool step";
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
        plan.plan_id = "snapshot-goal-plan";
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;
        plan.steps.push_back(toolStep());
        return plan;
    }
    Plan revise_plan(const Plan& existing_plan, const nlohmann::json&) override {
        return existing_plan;
    }
};

bool waitForTerminal(Thoth::ExecutiveController& controller) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto state = controller.get_state();
        if (state == Thoth::ControllerState::COMPLETED || state == Thoth::ControllerState::FAILED
            || state == Thoth::ControllerState::ABORTED) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

int runSyntheticGoal(const fs::path& workspace) {
    setenv("THOTH_WORKSPACE_PATH", workspace.c_str(), 1);
    setenv("THOTH_LOGS_PATH", (workspace / "logs").c_str(), 1);
    setenv("THOTH_TEST_SUITE_DEV", "1", 1);
    const fs::path evidence = workspace / "evidence";
    fs::create_directories(evidence);
    Config config;
    config.database_path = (workspace / "memory.db").string();
    auto memory = std::make_shared<Memory>(config);
    auto registry = std::make_shared<ToolRegistry>();
    auto planner = std::make_shared<ToolGoalPlanner>();
    Thoth::ExecutiveController controller(planner, registry, nullptr, memory);
    controller.set_max_reflections(0);
    const std::string accepted = controller.execute_fixed_evidence_goal(
        "diagnose the historical case", evidence.string());
    const bool finished = waitForTerminal(controller);
    if (accepted.rfind("GOAL ACCEPTED:", 0) != 0 || !finished
        || controller.get_state() != Thoth::ControllerState::FAILED) {
        return 2;
    }
    return 0;
}

bool snapshotFreshEngine(const std::string& binary, const fs::path& root,
                         Thoth::MemorySnapshot& snapshot, SourceTrio& source) {
    const fs::path workspace = root / "engine";
    std::string error;
    if (!prepareEngineDatabase(binary, workspace, error)) {
        fail(error);
        return false;
    }
    source = sealSource(workspace / "memory.db");
    const Thoth::ExperienceTableCounts empty;
    if (!Thoth::createConsistentMemorySnapshot(
            source.database.string(), (root / "frozen").string(), &empty, snapshot, error)) {
        fail("snapshot failed: " + error);
        return false;
    }
    if (!sourceUnchanged(source, "snapshot")) {
        return false;
    }
    if (snapshot.sha256.size() != 64 || snapshot.counts != empty
        || !fs::is_regular_file(snapshot.database_path)) {
        fail("snapshot artifact is incomplete: " + snapshot.sha256);
        return false;
    }
    if (fs::exists(fs::path(snapshot.database_path + "-wal"))
        || fs::exists(fs::path(snapshot.database_path + "-shm"))) {
        fail("canonical snapshot left a WAL sidecar");
        return false;
    }
    std::cout << "SNAPSHOT_SHA256 " << snapshot.sha256 << "\n";
    return true;
}

bool testSnapshot(const std::string& binary) {
    const fs::path root = caseRoot("snapshot");
    Thoth::MemorySnapshot snapshot;
    SourceTrio source;
    if (!snapshotFreshEngine(binary, root, snapshot, source)) {
        return false;
    }
    const fs::path restored = root / "verify" / "memory.db";
    std::string error;
    if (!Thoth::restoreMemorySnapshot(snapshot.database_path, restored.string(), error)) {
        fail("restore failed: " + error);
        return false;
    }
    if (readFile(snapshot.database_path) != readFile(restored)) {
        fail("restored copy does not match the frozen snapshot");
        return false;
    }
    Thoth::ExperienceTableCounts counts;
    if (!Thoth::readExperienceTableCounts(restored.string(), counts, error)
        || counts != Thoth::ExperienceTableCounts{}) {
        fail("restored experience counts: " + error);
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(restored.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        fail("integrity open failed");
        if (db) {
            sqlite3_close(db);
        }
        return false;
    }
    sqlite3_stmt* stmt = nullptr;
    const bool integrity = sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &stmt, nullptr) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW
        && std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) == "ok";
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    fs::remove_all(root);
    if (!integrity) {
        fail("restored integrity_check failed");
        return false;
    }
    return true;
}

bool testGoal(const std::string& binary) {
    const fs::path root = caseRoot("goal");
    Thoth::MemorySnapshot snapshot;
    SourceTrio source;
    if (!snapshotFreshEngine(binary, root, snapshot, source)) {
        return false;
    }
    const fs::path eval_a = root / "eval_a";
    const fs::path eval_b = root / "eval_b";
    std::string error;
    if (!Thoth::restoreMemorySnapshot(snapshot.database_path, (eval_a / "memory.db").string(), error)
        || !Thoth::restoreMemorySnapshot(snapshot.database_path, (eval_b / "memory.db").string(), error)) {
        fail("evaluation restore failed: " + error);
        return false;
    }
    const std::string frozen_before = readFile(snapshot.database_path);
    const std::string idle_before = readFile(eval_b / "memory.db");
    const pid_t pid = fork();
    if (pid < 0) {
        fail("goal fork failed");
        return false;
    }
    if (pid == 0) {
        const int code = runSyntheticGoal(eval_a);
        _exit(code);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    int status = 0;
    bool exited = false;
    while (std::chrono::steady_clock::now() < deadline) {
        if (waitpid(pid, &status, WNOHANG) == pid) {
            exited = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!exited) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        fail("synthetic goal timed out");
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fail("synthetic goal failed status=" + std::to_string(status));
        return false;
    }
    const std::string frozen_after = readFile(snapshot.database_path);
    const std::string idle_after = readFile(eval_b / "memory.db");
    Thoth::ExperienceTableCounts counts;
    const bool counted = Thoth::readExperienceTableCounts(
        (eval_a / "memory.db").string(), counts, error);
    const bool experience_written = counted
        && (counts.past_plans > 0 || counts.trajectories > 0);
    const bool frozen_identical = !frozen_before.empty() && frozen_before == frozen_after
        && idle_before == frozen_before && idle_after == frozen_before
        && !fs::exists(fs::path((eval_b / "memory.db").string() + "-wal"));
    std::cout << "GOAL_FROZEN_SHA256 " << snapshot.sha256 << "\n";
    std::cout << "GOAL_EVAL_PAST_PLANS " << counts.past_plans
              << " TRAJECTORIES " << counts.trajectories << "\n";
    const bool source_same = sourceUnchanged(source, "goal");
    fs::remove_all(root);
    if (!source_same || !frozen_identical || !experience_written) {
        fail("goal: frozen_identical=" + std::to_string(frozen_identical)
             + " counted=" + std::to_string(counted)
             + " error=" + error
             + " past_plans=" + std::to_string(counts.past_plans)
             + " trajectories=" + std::to_string(counts.trajectories));
        return false;
    }
    return true;
}

bool expectSnapshotFailure(const fs::path& database, const fs::path& snapshot_dir,
                           const Thoth::ExperienceTableCounts* required,
                           const std::string& needle, const std::string& label) {
    const SourceTrio before = sealSource(database);
    Thoth::MemorySnapshot snapshot;
    std::string error;
    const bool created = Thoth::createConsistentMemorySnapshot(
        database.string(), snapshot_dir.string(), required, snapshot, error);
    if (created || error.find(needle) == std::string::npos || !snapshot.database_path.empty()
        || fs::exists(snapshot_dir / "memory.db")) {
        fail(label + ": created=" + std::to_string(created) + " error=" + error);
        return false;
    }
    return sourceUnchanged(before, label);
}

bool testMissing() {
    const fs::path root = caseRoot("missing");
    const bool ok = expectSnapshotFailure(
        root / "absent" / "memory.db",
        root / "frozen",
        nullptr,
        "memory database is missing",
        "missing");
    fs::remove_all(root);
    return ok;
}

bool testIncomplete() {
    const fs::path root = caseRoot("incomplete");
    const fs::path database = root / "source" / "memory.db";
    fs::create_directories(database.parent_path());
    if (!createWalDatabase(database, false, true)) {
        fail("incomplete: fixture failed");
        return false;
    }
    fs::remove(fs::path(database.string() + "-wal"));
    const bool ok = expectSnapshotFailure(
        database, root / "frozen", nullptr,
        "memory database is incomplete without its WAL", "incomplete");
    fs::remove_all(root);
    return ok;
}

bool testInvalidWal() {
    const fs::path root = caseRoot("invalid_wal");
    const fs::path database = root / "source" / "memory.db";
    fs::create_directories(database.parent_path());
    if (!createWalDatabase(database, false, true)) {
        fail("invalid_wal: fixture failed");
        return false;
    }
    const fs::path wal = fs::path(database.string() + "-wal");
    {
        std::ofstream out(wal, std::ios::binary | std::ios::trunc);
        out << "this is not a sqlite wal";
    }
    const bool ok = expectSnapshotFailure(
        database, root / "frozen", nullptr, "memory snapshot WAL is invalid", "invalid_wal");
    fs::remove_all(root);
    return ok;
}

bool testCountMismatch() {
    const fs::path root = caseRoot("count_mismatch");
    const fs::path database = root / "source" / "memory.db";
    fs::create_directories(database.parent_path());
    if (!createWalDatabase(database, true, false)) {
        fail("count_mismatch: fixture failed");
        return false;
    }
    const Thoth::ExperienceTableCounts required;
    const bool ok = expectSnapshotFailure(
        database, root / "frozen", &required,
        "memory snapshot experience counts mismatch", "count_mismatch");
    fs::remove_all(root);
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        fail("usage: memory_snapshot_tests <case> [thoth-engine]");
        return 2;
    }
    const std::string case_name = argv[1];
    bool ok = false;
    if (case_name == "missing") {
        ok = testMissing();
    } else if (case_name == "incomplete") {
        ok = testIncomplete();
    } else if (case_name == "invalid_wal") {
        ok = testInvalidWal();
    } else if (case_name == "count_mismatch") {
        ok = testCountMismatch();
    } else if (case_name == "snapshot" || case_name == "goal") {
        if (argc < 3) {
            fail(case_name + " requires the thoth-engine path");
            return 2;
        }
        ok = case_name == "snapshot" ? testSnapshot(argv[2]) : testGoal(argv[2]);
    } else {
        fail("unknown case: " + case_name);
        return 2;
    }
    return ok ? 0 : 1;
}
