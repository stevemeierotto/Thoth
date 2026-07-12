/*
 * Headless driver for docs/TEST_SUITE.md (TC-01–TC-07).
 * Uses BasicAgentPlugin — no GUI.
 *
 * Tiers:
 *   --dev   Fast path: TfIdf embeddings, mock LLM, tiny corpus, cached index (no Ollama).
 *   --full  Production regression: real Ollama LLM + external embeddings (~40 min).
 */
#include "basic_agent_plugin.h"
#include "file_handler.h"
#include "inference_endpoint.h"
#include "runtime_bootstrap.h"
#include "executive_controller.h"
#include "benchmark_context.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

enum class SuiteTier { Dev, Full };

static SuiteTier parseTier(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--dev") return SuiteTier::Dev;
        if (arg == "--full") return SuiteTier::Full;
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: run_test_suite [--dev|--full]\n"
                      << "  --dev   Fast mock path (default; no Ollama)\n"
                      << "  --full  Full Ollama regression\n";
            std::exit(0);
        }
    }
    return SuiteTier::Dev;
}

static void configureDevTier(const std::string& indexPath) {
    setenv("THOTH_TEST_SUITE_DEV", "1", 1);
    setenv("THOTH_MOCK_LLM", "true", 1);
    setenv("THOTH_TEST_SUITE_INDEX", indexPath.c_str(), 1);
}

static void configureFullTier() {
    unsetenv("THOTH_TEST_SUITE_DEV");
    unsetenv("THOTH_MOCK_LLM");
    unsetenv("THOTH_TEST_SUITE_INDEX");
}

static bool ollamaReachable() {
    const auto endpoints = Thoth::resolveInferenceEndpoints();
    const std::string tagsUrl = Thoth::inferenceUrl(endpoints.base_url, "/api/tags");
    const std::string cmd = "curl -sf \"" + tagsUrl + "\" >/dev/null 2>&1";
    const int code = std::system(cmd.c_str());
    return code == 0;
}

static void truncateLog(const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
}

static std::vector<json> loadJsonl(const std::string& path) {
    std::vector<json> entries;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            entries.push_back(json::parse(line));
        } catch (...) {
        }
    }
    return entries;
}

static json benchDiagnostics(const json& entry) {
    if (entry.contains("diagnostics") && entry["diagnostics"].is_object()) {
        return entry["diagnostics"];
    }
    return entry;
}

static double jsonDouble(const json& obj, const char* key, double fallback = 0.0) {
    if (!obj.is_object() || !obj.contains(key) || obj[key].is_null()) {
        return fallback;
    }
    try {
        return obj[key].get<double>();
    } catch (...) {
        return fallback;
    }
}

static bool isGragScoringType(const std::string& scoringType) {
    return scoringType == "grag" || scoringType == "grag_hybrid" ||
           scoringType == "rag_hybrid" || scoringType == "grag_blended_hybrid";
}

static json latestGragDiagnostics(const std::vector<json>& bench, bool requireGoalPresent) {
    for (auto it = bench.rbegin(); it != bench.rend(); ++it) {
        const json d = benchDiagnostics(*it);
        if (!d.is_object()) {
            continue;
        }
        std::string st;
        if (d.contains("scoring_type") && d["scoring_type"].is_string()) {
            st = d["scoring_type"].get<std::string>();
        }
        if (!isGragScoringType(st)) {
            continue;
        }
        if (requireGoalPresent && !d.value("goal_present", false)) {
            continue;
        }
        return d;
    }
    return json();
}

static bool chatRetrievalDiagnosticsPresent(const std::vector<json>& bench) {
    for (auto it = bench.rbegin(); it != bench.rend(); ++it) {
        const json d = benchDiagnostics(*it);
        const std::string st = d.contains("scoring_type") && d["scoring_type"].is_string()
            ? d["scoring_type"].get<std::string>() : "";
        if (!isGragScoringType(st)) {
            continue;
        }
        if (d.value("alpha", 0.0) <= 0.0) {
            continue;
        }
        if (!d.contains("breakdowns") || !d["breakdowns"].is_array() || d["breakdowns"].empty()) {
            continue;
        }
        for (const auto& row : d["breakdowns"]) {
            if (row.is_object() && row.value("final_score", 0.0) > 0.0) {
                return true;
            }
        }
    }
    return false;
}

static json metadataObject(const json& event) {
    if (event.contains("metadata") && event["metadata"].is_object()) {
        return event["metadata"];
    }
    return json::object();
}

static std::vector<json> routingDecisions(const std::vector<json>& applog) {
    std::vector<json> out;
    for (const auto& e : applog) {
        if (e.value("event_name", "") == "routing_decision") {
            out.push_back(e.value("metadata", json::object()));
        }
    }
    return out;
}

static std::vector<json> traceEvents(const std::vector<json>& trace) {
    std::vector<json> events;
    for (const auto& entry : trace) {
        if (entry.contains("event_type")) {
            events.push_back({
                {"name", entry.value("event_type", "")},
                {"metadata", entry.value("metadata", json::object())}
            });
        }
        if (entry.contains("stages") && entry["stages"].is_array()) {
            for (const auto& stage : entry["stages"]) {
                events.push_back({
                    {"name", stage.value("name", "")},
                    {"metadata", stage.value("metadata", json::object())}
                });
            }
        }
    }
    return events;
}

struct RunFlags {
    std::atomic<bool> plan_completed{false};
    std::atomic<bool> executing{false};
    std::atomic<bool> retrieval_diagnostics{false};
};

static bool waitMs(int ms, const std::function<bool()>& done) {
    const int step = 100;
    for (int elapsed = 0; elapsed < ms; elapsed += step) {
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
    }
    return done();
}

static bool report(const char* tc, const char* name, bool pass, const char* reason = "") {
    std::cout << (pass ? "  PASS  " : "  FAIL  ") << tc << " | " << name;
    if (!pass && reason && *reason) {
        std::cout << "\n         -> " << reason;
    }
    std::cout << "\n";
    return pass;
}

/** RAII: emit TEST_SUITE_COMPLETE on normal exit, TEST_SUITE_ABORTED if scope exits early. */
class TestSuiteRunRecorder {
public:
    TestSuiteRunRecorder(Thoth::BenchmarkRun& run, bool devTier) : run_(run), devTier_(devTier) {}

    ~TestSuiteRunRecorder() {
        if (!finished_) {
            run_.emit("TEST_SUITE_ABORTED", payload());
        }
    }

    void complete(int failures) {
        failures_ = failures;
        run_.emit("TEST_SUITE_COMPLETE", payload());
        finished_ = true;
    }

private:
    nlohmann::json payload() const {
        return {{"failed", failures_}, {"tier", devTier_ ? "dev" : "full"}};
    }

    Thoth::BenchmarkRun& run_;
    bool devTier_;
    int failures_ = 0;
    bool finished_ = false;
};

static void writeTestCorpus() {
    FileHandler fh;
    const fs::path corpus = fs::path(fh.getAgentWorkspacePath("rag/test_suite_corpus"));
    fs::create_directories(corpus);
    auto write = [&](const char* name, const char* body) {
        std::ofstream out(corpus / name);
        out << body;
    };
    write("grag_overview.md",
          "GRAG (Goal-Relative Adaptive Graph Retrieval) uses goal embedding G and current state C. "
          "Direction D = G - C steers retrieval. Indexes include codebase_index and conversations_index. "
          "Adaptive alpha blends query similarity with directional scoring.\n");
    write("executive_controller.md",
          "ExecutiveController state machine: IDLE, PLANNING, EXECUTING_STEP, OBSERVING_RESULT, "
          "REVISING_PLAN, COMPLETED. Plans include RETRIEVAL steps before LLM steps.\n");
    write("indexes.md",
          "GRAG multi-index routing uses PLAN_AWARE, GOAL_ONLY, and CONVERSATIONAL modes. "
          "PLAN_AWARE scans codebase_index when a goal is active.\n");
}

static void resetPlanTemplatesForTestRun() {
    FileHandler fh;
    const fs::path dir = fs::path(fh.getAgentWorkspacePath("prompt_templates"));
    for (const char* name : {"plan_generation.tmpl", "plan_revision.tmpl"}) {
        std::error_code ec;
        fs::remove(dir / name, ec);
    }
}

int main(int argc, char** argv) {
    const SuiteTier tier = parseTier(argc, argv);
    const bool devTier = tier == SuiteTier::Dev;

    writeTestCorpus();
    resetPlanTemplatesForTestRun();

    FileHandler fh;
    const std::string corpusPath = fh.getAgentWorkspacePath("rag/test_suite_corpus");
    const std::string indexPath = (fs::path(corpusPath) / "test_suite.rag_index.bin").string();

    if (devTier) {
        configureDevTier(indexPath);
        std::cout << "TEST_SUITE tier: dev (mock LLM, TfIdf, cached index)\n";
    } else {
        configureFullTier();
        if (!ollamaReachable()) {
            const auto endpoints = Thoth::resolveInferenceEndpoints();
            std::cerr << "TEST_SUITE: Ollama not reachable at "
                      << endpoints.base_url << " — start Ollama first.\n";
            return 2;
        }
        std::cout << "TEST_SUITE tier: full (Ollama required)\n";
    }

    const std::string tracePath = fh.getAgentWorkspacePath("decision_trace.jsonl");
    const std::string benchPath = fh.getAgentWorkspacePath("grag_benchmark.jsonl");
    const std::string appPath = fh.getAgentWorkspacePath("app_log.jsonl");

    truncateLog(tracePath);
    truncateLog(benchPath);
    // Keep app_log history minimal — truncate for clean routing_decision reads
    truncateLog(appPath);

    const int goalWaitMs = devTier ? 120000 : 600000;
    const int chatWaitMs = devTier ? 30000 : 120000;
    const int execWaitMs = devTier ? 15000 : 60000;

    Thoth::bootstrapRuntimeEnvironment();

    RunFlags flags;
    BasicAgentPlugin plugin;
    plugin.setSessionId("test_suite_headless");
    plugin.setRagFiles({corpusPath});

    Thoth::BenchmarkRun benchmarkRun = Thoth::BenchmarkRun::create(
        plugin.buildTestSuiteBenchmarkInputs(!devTier, corpusPath));
    benchmarkRun.bindIndex(plugin.benchmarkIndexEnvironment());
    const Thoth::BenchmarkAttribution suiteAttribution = benchmarkRun.attribution();

    std::cout << "BENCHMARK_ENV run_id=" << benchmarkRun.run_id()
              << " env_hash=" << benchmarkRun.environment_hash()
              << " index_hash=" << benchmarkRun.index_hash()
              << " tier=" << (devTier ? "dev" : "full") << "\n";

    TestSuiteRunRecorder suiteRecorder(benchmarkRun, devTier);

    if (const char* abortSmoke = std::getenv("THOTH_TEST_SUITE_BENCHMARK_ABORT_SMOKE");
        abortSmoke && (std::string(abortSmoke) == "1" || std::string(abortSmoke) == "true")) {
        std::cerr << "TEST_SUITE: benchmark abort smoke — exiting before complete()\n";
        return 2;
    }

    plugin.onEvent = [&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_COMPLETED) flags.plan_completed.store(true);
        if (ev.type == EventType::STEP_STARTED) flags.executing.store(true);
        if (ev.type == EventType::RETRIEVAL_DIAGNOSTICS) flags.retrieval_diagnostics.store(true);
    };

    int failures = 0;
    auto fail = [&](bool ok) { if (!ok) ++failures; };

    // ── TC-01: plain chat, no goal ─────────────────────────────────────────
    std::cout << "\nRunning TC-01 …\n";
    plugin.processInput("What is GRAG?");
    {
        auto routing = routingDecisions(loadJsonl(appPath));
        const json* last = routing.empty() ? nullptr : &routing.back();
        const bool ok = last && (*last).value("goal_active", true) == false
            && (*last).value("routing_mode", "") == "CONVERSATIONAL";
        fail(report("TC-01", "CONVERSATIONAL routing, goal_active=false", ok,
                    "Expected CONVERSATIONAL with no active goal"));
    }

    // ── TC-02 / TC-03: goal execution + GRAG math ──────────────────────────
    std::cout << "Running TC-02 / TC-03 …\n";
    flags.plan_completed.store(false);
    flags.executing.store(false);
    plugin.executeGoal("Analyze the ExecutiveController and summarize its state machine",
                       suiteAttribution);

    const bool goalFinished = waitMs(goalWaitMs, [&] {
        return flags.plan_completed.load();
    });

    const auto trace = loadJsonl(tracePath);
    const auto events = traceEvents(trace);
    std::vector<json> planCreates;
    for (const auto& e : events) {
        if (e.value("name", "") == "PLAN_CREATED") planCreates.push_back(e);
    }
    json chosenPlan;
    for (auto it = planCreates.rbegin(); it != planCreates.rend(); ++it) {
        const json meta = metadataObject(*it);
        if (!meta.contains("plan") || !meta["plan"].is_object()) {
            continue;
        }
        const json& planObj = meta["plan"];
        if (!planObj.contains("steps") || !planObj["steps"].is_array()) {
            continue;
        }
        if (planObj["steps"].size() >= 2) {
            chosenPlan = planObj;
            break;
        }
    }
    fail(report("TC-02", "goal run finished (PLAN_COMPLETED)", goalFinished,
                "Timed out waiting for plan completion (600s)"));
    fail(report("TC-02", "plan has >= 2 steps", chosenPlan.contains("steps") && chosenPlan["steps"].size() >= 2,
                "No PLAN_CREATED with >= 2 steps in trace"));

    int retStart = -1;
    int llmStart = -1;
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].value("name", "") != "STEP_STARTED") continue;
        const json meta = metadataObject(events[i]);
        const int st = meta.value("step_type", -1);
        if (st == 1 && retStart < 0) retStart = static_cast<int>(i);
        if (st == 2) llmStart = static_cast<int>(i);
    }
    fail(report("TC-02", "RETRIEVAL STEP_STARTED before LLM", retStart >= 0 && llmStart >= 0 && retStart < llmStart,
                "RETRIEVAL/LLM step order wrong in trace"));

    const auto bench = loadJsonl(benchPath);
    const json latestGrag = latestGragDiagnostics(bench, true);
    const json latestGragAny = latestGrag.is_object() ? latestGrag : latestGragDiagnostics(bench, false);
    const json& gragRow = latestGrag.is_object() ? latestGrag : latestGragAny;
    const double alpha = jsonDouble(gragRow, "alpha");
    const double magnitude = jsonDouble(gragRow, "direction_magnitude");
    const bool goalGrag = gragRow.value("goal_present", false);
    fail(report("TC-03", "grag scoring entry present", gragRow.is_object(),
                "No grag/rag_hybrid entry in grag_benchmark.jsonl"));
    if (devTier) {
        fail(report("TC-03", "goal-scoped retrieval logged (goal_present)", goalGrag,
                    "No goal-scoped GRAG row after executeGoal"));
    } else {
        fail(report("TC-03", "alpha > 0", alpha > 0.0, "Direction blend collapsed"));
        fail(report("TC-03", "direction_magnitude > 0", magnitude > 0.0,
                    "Goal and state embeddings may be identical"));
    }

    // ── TC-04 / TC-05: chat during active goal ─────────────────────────────
    std::cout << "Running TC-04 / TC-05 …\n";
    flags.plan_completed.store(false);
    flags.executing.store(false);
    flags.retrieval_diagnostics.store(false);
    plugin.executeGoal("Review the GRAG retrieval implementation", suiteAttribution);
    waitMs(execWaitMs, [&] { return flags.executing.load(); });
    plugin.processInput("What indexes does GRAG use?");
    waitMs(chatWaitMs, [] { return true; }); // allow retrieval + LLM to finish

    {
        auto routing = routingDecisions(loadJsonl(appPath));
        bool found = false;
        for (auto it = routing.rbegin(); it != routing.rend(); ++it) {
            if ((*it).value("routing_mode", "") == "PLAN_AWARE"
                && (*it).value("goal_active", false) == true) {
                found = true;
                break;
            }
        }
        fail(report("TC-04", "PLAN_AWARE while goal active", found,
                    "No PLAN_AWARE routing_decision with goal_active=true"));
    }
    fail(report("TC-05", "retrieval diagnostics signal present",
                flags.retrieval_diagnostics.load() || chatRetrievalDiagnosticsPresent(loadJsonl(benchPath)),
                "No RETRIEVAL_DIAGNOSTICS callback and no scored GRAG rows in grag_benchmark.jsonl"));

    // ── TC-06: no tool hallucination ───────────────────────────────────────
    std::cout << "Running TC-06 …\n";
    const char* questions[] = {
        "Will the scientific reasoning mode work?",
        "What is the current state of the controller?",
        "How does GRAG calculate alpha?"
    };
    for (const char* q : questions) {
        plugin.processInput(q);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    {
        std::ifstream in(tracePath);
        std::string content((std::istreambuf_iterator<char>(in)), {});
        const bool bad = content.find("gmail_read_labels") != std::string::npos
            || content.find("stock_quote") != std::string::npos;
        fail(report("TC-06", "no unexpected tool names in trace", !bad,
                    "Found suspicious tool invocation in decision_trace.jsonl"));
    }

    // ── TC-07: goal persists after completion ──────────────────────────────
    std::cout << "Running TC-07 …\n";
    flags.plan_completed.store(false);
    plugin.executeGoal("Summarize GRAG directional scoring in one paragraph", suiteAttribution);
    waitMs(goalWaitMs, [&] { return flags.plan_completed.load(); });
    plugin.processInput("What did you find?");
    {
        auto routing = routingDecisions(loadJsonl(appPath));
        bool found = false;
        for (auto it = routing.rbegin(); it != routing.rend(); ++it) {
            if ((*it).value("routing_mode", "") == "PLAN_AWARE"
                && (*it).value("goal_active", false) == true) {
                found = true;
                break;
            }
        }
        fail(report("TC-07", "PLAN_AWARE after PLAN_COMPLETED", found,
                    "goal_active cleared too early after plan completion"));
    }

    std::cout << "\n══════════════════════════════════════════════\n";
    if (failures == 0) {
        std::cout << "  TEST_SUITE: ALL 7 CASES PASSED";
        if (devTier) {
            std::cout << " (dev tier)";
        }
        std::cout << "\n";
    } else {
        std::cout << "  TEST_SUITE: " << failures << " CHECK(S) FAILED\n";
        std::cout << "  Logs: " << tracePath << "\n";
        std::cout << "        " << benchPath << "\n";
        std::cout << "        " << appPath << "\n";
    }
    std::cout << "══════════════════════════════════════════════\n\n";

    suiteRecorder.complete(failures);
    return failures == 0 ? 0 : 1;
}
