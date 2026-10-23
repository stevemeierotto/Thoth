/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — CM-1 live controller end-to-end check.
 * Single-passage baseline: one retrieval step, one live synthesis step.
 * Two-passage scenario: two retrieval steps, then one live synthesis step.
 * Each scenario uses separate temporary workspaces. Not registered with CTest.
 * Does not open agent_workspace databases or project cognitive logs.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "alp_sha256.h"
#include "cm1_telemetry.h"
#include "config.h"
#include "embedding_engine.h"
#include "executive_controller.h"
#include "file_handler.h"
#include "graph_refiner.h"
#include "index_manager.h"
#include "inference_client.h"
#include "inference_endpoint.h"
#include "llm_interface.h"
#include "llm_timeout_policy.h"
#include "memory.h"
#include "plan.h"
#include "rag.h"
#include "sqlite_memory_repository.h"
#include "tools.h"

#include <json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

const char* kOwnerName = "OWNER";
const char* kRootPrefix = "/tmp/thoth-cm1-controller-";
const char* kSessionId = "cm1-live-controller";
const char* kRetrieveStep = "retrieve-note";
const char* kSynthesizeStep = "synthesize";
const char* kToken = "cm1livepassagealpha";
const char* kNoteLine =
    "cm1livepassagealpha The relay cache stores one blue token for this isolated controller check.";
const char* kGoal =
    "State what the relay cache stores using the note that contains cm1livepassagealpha.";
const char* kQuery = "cm1livepassagealpha";
const char* kSinglePrompt =
    "Using only the retrieved note, answer in one sentence what the relay cache stores.";
const char* kBetaLine =
    "cm1livepassagebeta The relay cache stores one green token for this isolated controller check.";
const char* kTwoGoal =
    "State what the relay cache stores using the notes that contain cm1livepassagealpha and cm1livepassagebeta.";
const char* kTwoPrompt =
    "Using only the retrieved notes, answer in one sentence what the relay cache stores.";

struct PassageNote {
    const char* step_id = "";
    const char* file_name = "";
    const char* token = "";
    const char* line = "";
    const char* query = "";
};

struct Scenario {
    const char* name = "";
    const char* goal = "";
    const char* synthesis_prompt = "";
    std::vector<PassageNote> notes;
};

std::string noteBody(const PassageNote& note) {
    return std::string(note.line) + "\n";
}

Scenario singleScenario() {
    Scenario scenario;
    scenario.name = "single";
    scenario.goal = kGoal;
    scenario.synthesis_prompt = kSinglePrompt;
    scenario.notes.push_back(PassageNote{kRetrieveStep, "note.md", kToken, kNoteLine, kQuery});
    return scenario;
}

Scenario twoScenario() {
    Scenario scenario;
    scenario.name = "two";
    scenario.goal = kTwoGoal;
    scenario.synthesis_prompt = kTwoPrompt;
    scenario.notes.push_back(
        PassageNote{"retrieve-alpha", "alpha.md", kToken, kNoteLine, kQuery});
    scenario.notes.push_back(PassageNote{
        "retrieve-beta", "beta.md", "cm1livepassagebeta", kBetaLine, "cm1livepassagebeta"});
    return scenario;
}

std::string g_fail;
bool g_preserve_trace = false;

void fail(const std::string& why) {
    if (g_fail.empty()) {
        g_fail = why;
    }
    std::cerr << "cm1-live-controller FAIL: " << why << "\n";
}

bool nearlyEqual(float left, float right) {
    return std::fabs(left - right) < 1e-5f;
}

std::string passageRegion(const std::string& prompt) {
    const std::string start = "EVIDENCE PASSAGE START\n";
    const std::string end = "EVIDENCE PASSAGE END\n";
    const auto at = prompt.find(start);
    const auto stop = prompt.find(end);
    if (at == std::string::npos || stop == std::string::npos || stop < at) {
        return {};
    }
    return prompt.substr(at + start.size(), stop - (at + start.size()));
}

int passageMarkerCount(const std::string& region) {
    int count = 0;
    for (std::size_t at = 0; (at = region.find("[passage ", at)) != std::string::npos; at += 9) {
        ++count;
    }
    return count;
}

bool isolatedPath(const std::string& path) {
    const fs::path normal = fs::path(path).lexically_normal();
    const std::string text = normal.string();
    if (text.find("..") != std::string::npos) {
        return false;
    }
    if (text.rfind(kRootPrefix, 0) != 0) {
        return false;
    }
    if (text.find("agent_workspace") != std::string::npos) {
        return false;
    }
    return true;
}

// Same per-edge factor as Memory::applyAgingLocked. A newly reinforced edge
// is not old enough for the additional 30-day factor.
float weightAfterHistoryAging(float reinforcedWeight) {
    return reinforcedWeight * 0.995f;
}

enum class DiagnosticMatch { Ok, Missing, Text, Score };

DiagnosticMatch matchSelectedDiagnostic(const nlohmann::json& result,
                                        std::size_t index,
                                        const std::string& content,
                                        float& scoreOut) {
    if (!result.is_object() || !result.contains("diagnostics") || !result["diagnostics"].is_object()) {
        return DiagnosticMatch::Missing;
    }
    const auto& diagnostics = result["diagnostics"];
    if (!diagnostics.contains("breakdowns") || !diagnostics["breakdowns"].is_array()
        || !diagnostics.contains("final_scores") || !diagnostics["final_scores"].is_array()) {
        return DiagnosticMatch::Missing;
    }
    const auto& breakdowns = diagnostics["breakdowns"];
    const auto& scores = diagnostics["final_scores"];
    if (index >= breakdowns.size() || index >= scores.size() || !breakdowns[index].is_object()
        || !scores[index].is_number() || !breakdowns[index].contains("final_score")
        || !breakdowns[index]["final_score"].is_number()) {
        return DiagnosticMatch::Missing;
    }
    if (breakdowns[index].value("code_text", "") != content) {
        return DiagnosticMatch::Text;
    }
    const float score = scores[index].get<float>();
    if (!nearlyEqual(score, breakdowns[index]["final_score"].get<float>())) {
        return DiagnosticMatch::Score;
    }
    scoreOut = score;
    return DiagnosticMatch::Ok;
}

bool selfCheckCompletionTrace();

bool selfCheck() {
    Thoth::Cm1EvidenceChunk chunk;
    chunk.content = std::string(kNoteLine) + "\n";
    chunk.content_sha256 = Thoth::sha256Hex(chunk.content);
    const std::string prompt = Thoth::buildCm1EvidencePrompt(
        "The relay cache stores one blue token.", {chunk});
    const std::string region = passageRegion(prompt);
    const std::string expected = std::string("[passage ") + chunk.content_sha256 + "]\n"
        + chunk.content + "\n\n";
    if (region != expected || passageMarkerCount(region) != 1) {
        fail("harness: passage region does not match the production evidence prompt");
        return false;
    }

    Thoth::Cm1EvidenceChunk other;
    other.content = "A second passage must not compare equal.";
    other.content_sha256 = Thoth::sha256Hex(other.content);
    const std::string extra = passageRegion(Thoth::buildCm1EvidencePrompt("claim", {chunk, other}));
    if (extra.empty() || extra == expected || passageMarkerCount(extra) != 2) {
        fail("harness: an extra evidence passage was not detected");
        return false;
    }

    if (!isolatedPath("/tmp/thoth-cm1-controller-abc/off/memory.db")) {
        fail("harness: owned temporary database was rejected");
        return false;
    }
    if (isolatedPath("/home/steve/Thoth/agent_workspace/memory.db")
        || isolatedPath("/tmp/thoth-cm1-controller-abc/../agent_workspace/memory.db")
        || isolatedPath("/tmp/thoth-cm1-controller-abc/agent_workspace/memory.db")
        || isolatedPath("/tmp/other/memory.db")) {
        fail("harness: a non-owned path was accepted");
        return false;
    }

    Thoth::Cm1EvidenceChunk second;
    second.content = std::string(kBetaLine) + "\n";
    second.content_sha256 = Thoth::sha256Hex(second.content);
    const std::string ordered = passageRegion(
        Thoth::buildCm1EvidencePrompt("claim", {chunk, second}));
    const std::string expected_ordered = std::string("[passage ") + chunk.content_sha256 + "]\n"
        + chunk.content + "\n\n[passage " + second.content_sha256 + "]\n" + second.content + "\n\n";
    if (ordered != expected_ordered || passageMarkerCount(ordered) != 2
        || ordered.find(chunk.content_sha256) > ordered.find(second.content_sha256)) {
        fail("harness: two included passages are not kept in order");
        return false;
    }

    Memory::Edge seed;
    seed.from_id = "alpha-hash";
    seed.to_id = "beta-hash";
    seed.weight = 0.1f;
    seed.success_count = 0;
    seed.failure_count = 0;
    const auto reinforced = Thoth::GraphRefiner(std::shared_ptr<Memory>{}).computeReinforcedEdges({seed}, 1.0f);
    if (reinforced.size() != 1 || reinforced.front().success_count != 1
        || reinforced.front().failure_count != 0 || reinforced.front().weight <= seed.weight) {
        fail("harness: a new successful retrieval-set edge did not reinforce");
        return false;
    }
    const float persistedWeight = weightAfterHistoryAging(reinforced.front().weight);
    if (!(persistedWeight < reinforced.front().weight)
        || !nearlyEqual(persistedWeight, 0.2786000072956085f)) {
        fail("harness: history aging did not produce the persisted retrieval-edge weight");
        return false;
    }

    const nlohmann::json oneCandidate = {
        {"diagnostics", {
            {"breakdowns", nlohmann::json::array({{{"code_text", chunk.content}, {"final_score", 0.25}}})},
            {"final_scores", nlohmann::json::array({0.25})},
        }},
    };
    const nlohmann::json twoCandidates = {
        {"diagnostics", {
            {"breakdowns", nlohmann::json::array({
                {{"code_text", chunk.content}, {"final_score", 0.4}},
                {{"code_text", second.content}, {"final_score", 0.0}},
            })},
            {"final_scores", nlohmann::json::array({0.4, 0.0})},
        }},
    };
    float selectedScore = 0.0f;
    if (matchSelectedDiagnostic(oneCandidate, 0, chunk.content, selectedScore) != DiagnosticMatch::Ok
        || !nearlyEqual(selectedScore, 0.25f)
        || oneCandidate["diagnostics"]["breakdowns"].size() != 1
        || oneCandidate["diagnostics"]["final_scores"].size() != 1) {
        fail("harness: one scored candidate did not match its selected passage");
        return false;
    }
    if (matchSelectedDiagnostic(twoCandidates, 0, chunk.content, selectedScore) != DiagnosticMatch::Ok
        || !nearlyEqual(selectedScore, 0.4f)
        || twoCandidates["diagnostics"]["breakdowns"].size() < 2
        || matchSelectedDiagnostic(twoCandidates, 0, second.content, selectedScore) != DiagnosticMatch::Text) {
        fail("harness: extra scored candidates hid the selected passage");
        return false;
    }
    nlohmann::json mismatchedScore = twoCandidates;
    mismatchedScore["diagnostics"]["final_scores"][0] = 0.1;
    if (matchSelectedDiagnostic(mismatchedScore, 0, chunk.content, selectedScore) != DiagnosticMatch::Score) {
        fail("harness: a selected passage accepted a different diagnostic score");
        return false;
    }
    if (!selfCheckCompletionTrace()) {
        return false;
    }

    std::cout << "cm1-live-controller self-check PASS\n";
    return true;
}

struct EnvSlot {
    const char* name = "";
    bool had = false;
    std::string value;
};

class EnvGuard {
public:
    explicit EnvGuard(std::initializer_list<const char*> names) {
        for (const char* name : names) {
            EnvSlot slot;
            slot.name = name;
            if (const char* current = std::getenv(name)) {
                slot.had = true;
                slot.value = current;
            }
            slots_.push_back(std::move(slot));
        }
    }

    ~EnvGuard() {
        for (const auto& slot : slots_) {
            if (slot.had) {
                setenv(slot.name, slot.value.c_str(), 1);
            } else {
                unsetenv(slot.name);
            }
        }
    }

    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;

private:
    std::vector<EnvSlot> slots_;
};

bool envTruthy(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return false;
    }
    const std::string text(value);
    return text == "1" || text == "true" || text == "TRUE" || text == "yes" || text == "YES";
}

const char* stateName(Thoth::ControllerState state) {
    switch (state) {
        case Thoth::ControllerState::IDLE: return "IDLE";
        case Thoth::ControllerState::PLANNING: return "PLANNING";
        case Thoth::ControllerState::EXECUTING_STEP: return "EXECUTING_STEP";
        case Thoth::ControllerState::OBSERVING_RESULT: return "OBSERVING_RESULT";
        case Thoth::ControllerState::REVISING_PLAN: return "REVISING_PLAN";
        case Thoth::ControllerState::SCIENTIFIC_MODE: return "SCIENTIFIC_MODE";
        case Thoth::ControllerState::COMPLETED: return "COMPLETED";
        case Thoth::ControllerState::ABORTED: return "ABORTED";
        case Thoth::ControllerState::FAILED: return "FAILED";
    }
    return "UNKNOWN";
}

struct RecordedCall {
    std::string prompt;
    std::string text;
    std::string finish_reason;
    std::string error;
    bool ok = false;
    std::int64_t elapsed_ms = 0;
};

class RecordingClient : public Thoth::InferenceClient {
public:
    explicit RecordingClient(std::unique_ptr<Thoth::InferenceClient> inner)
        : inner_(std::move(inner)) {}

    std::vector<std::string> prompts() const {
        std::lock_guard<std::mutex> lock(mu_);
        return prompts_;
    }

    std::vector<RecordedCall> evidenceCalls() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<RecordedCall> evidence;
        for (const auto& call : calls_) {
            if (call.prompt.find("EVIDENCE PASSAGE START\n") != std::string::npos) {
                evidence.push_back(call);
            }
        }
        return evidence;
    }

    Thoth::InferenceGenerateResult generate(const Thoth::InferenceGenerateRequest& request) override {
        const Thoth::InferenceGenerateResult result = inner_->generate(request);
        RecordedCall call;
        call.prompt = request.prompt;
        call.text = result.text;
        call.finish_reason = result.finish_reason;
        call.error = result.error;
        call.ok = result.ok;
        call.elapsed_ms = result.elapsed_ms;
        std::lock_guard<std::mutex> lock(mu_);
        prompts_.push_back(request.prompt);
        calls_.push_back(std::move(call));
        return result;
    }

    Thoth::InferenceGenerateResult generateChat(const Thoth::InferenceChatRequest& request) override {
        return inner_->generateChat(request);
    }

    Thoth::InferenceEmbedResult embed(const Thoth::InferenceEmbedRequest& request) override {
        return inner_->embed(request);
    }

    Thoth::InferenceHealthResult health() override { return inner_->health(); }

    std::string backendName() const override { return inner_->backendName(); }

private:
    std::unique_ptr<Thoth::InferenceClient> inner_;
    mutable std::mutex mu_;
    std::vector<std::string> prompts_;
    std::vector<RecordedCall> calls_;
};

class ScriptedPassagePlanner : public IPlanner {
public:
    ScriptedPassagePlanner(std::string plan_id, Scenario scenario)
        : plan_id_(std::move(plan_id)), scenario_(std::move(scenario)) {}

    Plan create_plan(const std::string& goal) override {
        Plan plan;
        plan.plan_id = plan_id_;
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;

        std::string previous;
        for (const auto& note : scenario_.notes) {
            PlanStep retrieval;
            retrieval.step_id = note.step_id;
            retrieval.description = scenario_.notes.size() == 1
                ? "Retrieve the unique note"
                : std::string("Retrieve ") + note.file_name;
            retrieval.type = StepType::RETRIEVAL;
            retrieval.payload = {{"query", note.query}, {"top_k", 1}};
            if (!previous.empty()) {
                retrieval.depends_on = {previous};
            }
            plan.steps.push_back(retrieval);
            previous = note.step_id;
        }

        PlanStep synthesis;
        synthesis.step_id = kSynthesizeStep;
        synthesis.description = scenario_.notes.size() == 1
            ? "Answer from the retrieved note in one sentence"
            : "Answer from the retrieved notes in one sentence";
        synthesis.type = StepType::LLM;
        synthesis.depends_on = {previous};
        synthesis.payload = {{"prompt", scenario_.synthesis_prompt}};
        plan.steps.push_back(synthesis);
        return plan;
    }

    Plan revise_plan(const Plan& existing_plan, const nlohmann::json&) override {
        return existing_plan;
    }

private:
    std::string plan_id_;
    Scenario scenario_;
};

bool ownedDirectory(const fs::path& root, const std::string& token) {
    if (!isolatedPath(root.string())) {
        return false;
    }
    std::ifstream in(root / kOwnerName);
    if (!in.is_open()) {
        return false;
    }
    std::string got;
    std::getline(in, got);
    return got == token;
}

bool retainWorkspace(int code, bool unavailable_trace) {
    return code == 1 || unavailable_trace;
}

void removeOwnedDirectory(const fs::path& root, const std::string& token) {
    if (!ownedDirectory(root, token)) {
        std::cerr << "cm1-live-controller: refused cleanup of " << root << "\n";
        return;
    }
    std::error_code error;
    fs::remove_all(root, error);
}

bool restrictDirectory(const fs::path& path) {
    std::error_code error;
    fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace, error);
    if (error) {
        fail(std::string("harness: could not restrict ") + path.string() + ": " + error.message());
        return false;
    }
    return true;
}

struct EdgeSnap {
    std::string from_id;
    std::string to_id;
    float weight = 0.0f;
    int success_count = 0;
    int failure_count = 0;
};

struct Fingerprint {
    std::vector<std::string> step_ids;
    std::vector<int> step_statuses;
    std::vector<std::string> retrieval_hashes;
    std::vector<float> final_scores;
    std::vector<EdgeSnap> edges;
    int trajectories = 0;
    int past_plans = 0;
    int strategies = 0;
};

struct Measurements {
    std::int64_t prompt_tokens = -1;
    std::int64_t completion_tokens = -1;
    std::int64_t total_tokens = -1;
    std::int64_t past_plan_duration_ms = -1;
    std::int64_t synthesis_latency_ms = -1;
    std::int64_t goal_wall_clock_ms = -1;
};

struct ArmRecord {
    std::string name;
    bool evidence = false;
    std::string plan_id;
    std::string db_path;
    std::string logs_dir;
    Fingerprint fp;
    Measurements measured;
    std::string judgment_summary;
    int evidence_calls = 0;
    int claims_total = 0;
};

bool waitForCompleted(Thoth::ExecutiveController& controller, int wait_seconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(wait_seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto state = controller.get_state();
        const bool terminal = state == Thoth::ControllerState::COMPLETED
            || state == Thoth::ControllerState::FAILED
            || state == Thoth::ControllerState::ABORTED;
        if (terminal && !controller.is_running()) {
            return state == Thoth::ControllerState::COMPLETED;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false;
}

const PlanStep* findStep(const Plan& plan, const std::string& step_id) {
    for (const auto& step : plan.steps) {
        if (step.step_id == step_id) {
            return &step;
        }
    }
    return nullptr;
}

std::vector<std::pair<std::string, std::string>> retrievalChunks(const PlanStep& step) {
    std::vector<std::pair<std::string, std::string>> chunks;
    if (!step.result.is_object() || !step.result.contains("data") || !step.result["data"].is_object()) {
        return chunks;
    }
    const auto& data = step.result["data"];
    if (!data.contains("chunks") || !data["chunks"].is_array()) {
        return chunks;
    }
    for (const auto& chunk : data["chunks"]) {
        if (!chunk.is_object()) {
            continue;
        }
        chunks.emplace_back(chunk.value("file", ""), chunk.value("content", ""));
    }
    return chunks;
}

std::vector<std::string> inclusionHashes(const PlanStep& step) {
    std::vector<std::string> hashes;
    if (!step.result.is_object() || !step.result.contains("cm1_included_content_sha256")
        || !step.result["cm1_included_content_sha256"].is_array()) {
        return hashes;
    }
    for (const auto& hash : step.result["cm1_included_content_sha256"]) {
        if (hash.is_string()) {
            hashes.push_back(hash.get<std::string>());
        }
    }
    return hashes;
}

std::vector<float> finalScores(const PlanStep& step) {
    std::vector<float> scores;
    if (!step.result.is_object() || !step.result.contains("diagnostics")
        || !step.result["diagnostics"].is_object()) {
        return scores;
    }
    const auto& diagnostics = step.result["diagnostics"];
    if (!diagnostics.contains("final_scores") || !diagnostics["final_scores"].is_array()) {
        return scores;
    }
    for (const auto& score : diagnostics["final_scores"]) {
        if (score.is_number()) {
            scores.push_back(score.get<float>());
        }
    }
    return scores;
}

int countCallType(const fs::path& log_path, const std::string& plan_id, const std::string& call_type) {
    std::ifstream in(log_path);
    if (!in.is_open()) {
        return 0;
    }
    int count = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            const auto row = nlohmann::json::parse(line);
            if (row.value("plan_id", "") == plan_id && row.value("call_type", "") == call_type) {
                ++count;
            }
        } catch (...) {
        }
    }
    return count;
}

bool readMeasurements(const fs::path& metrics_path,
                      const std::string& plan_id,
                      const Memory& memory,
                      Measurements& out) {
    std::ifstream in(metrics_path);
    if (!in.is_open()) {
        fail(std::string("harness: cognitive metrics log was not written at ") + metrics_path.string());
        return false;
    }
    nlohmann::json matched;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            auto row = nlohmann::json::parse(line);
            if (row.value("event", "") == "GOAL_COGNITIVE_METRICS" && row.value("plan_id", "") == plan_id
                && row.value("outcome", "") == "completed") {
                matched = std::move(row);
            }
        } catch (...) {
        }
    }
    if (!matched.is_object() || !matched.contains("total_tokens") || !matched.contains("prompt_tokens")
        || !matched.contains("completion_tokens") || !matched.contains("llm_synthesis_time_ms")
        || !matched.contains("total_wall_clock_ms")) {
        fail("production: completed goal metrics omitted token totals, synthesis latency, or wall clock");
        return false;
    }
    out.prompt_tokens = matched["prompt_tokens"].get<std::int64_t>();
    out.completion_tokens = matched["completion_tokens"].get<std::int64_t>();
    out.total_tokens = matched["total_tokens"].get<std::int64_t>();
    out.synthesis_latency_ms = matched["llm_synthesis_time_ms"].get<std::int64_t>();
    out.goal_wall_clock_ms = matched["total_wall_clock_ms"].get<std::int64_t>();
    const auto plans = memory.getAllPastPlans();
    if (plans.size() == 1) {
        out.past_plan_duration_ms = plans.front().duration_ms;
    }
    return true;
}

std::vector<EdgeSnap> edgesOf(Memory& memory) {
    std::vector<EdgeSnap> edges;
    auto* repo = memory.getSQLiteRepo();
    if (!repo) {
        return edges;
    }
    for (const auto& edge : repo->getAllEdges()) {
        edges.push_back({edge.from_id, edge.to_id, edge.weight, edge.success_count, edge.failure_count});
    }
    std::sort(edges.begin(), edges.end(), [](const EdgeSnap& left, const EdgeSnap& right) {
        if (left.from_id != right.from_id) {
            return left.from_id < right.from_id;
        }
        return left.to_id < right.to_id;
    });
    return edges;
}

bool sameEdges(const std::vector<EdgeSnap>& left, const std::vector<EdgeSnap>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].from_id != right[i].from_id || left[i].to_id != right[i].to_id
            || left[i].success_count != right[i].success_count
            || left[i].failure_count != right[i].failure_count
            || !nearlyEqual(left[i].weight, right[i].weight)) {
            return false;
        }
    }
    return true;
}

std::string judgmentSummary(const nlohmann::json& payload) {
    std::ostringstream out;
    out << "claims_total=" << payload.value("claims_total", -1);
    if (!payload.contains("claims") || !payload["claims"].is_array()) {
        return out.str();
    }
    for (const auto& claim : payload["claims"]) {
        if (!claim.is_object()) {
            continue;
        }
        out << " [" << claim.value("claim_index", -1) << " ";
        if (claim.value("evaluation_status", "") == "available") {
            out << claim.value("label", "");
        } else {
            out << "unavailable:" << claim.value("unavailable_reason", "");
        }
        out << "]";
    }
    return out.str();
}

bool claimCalledModel(const nlohmann::json& claim) {
    if (claim.value("evaluation_status", "") == "available") {
        return true;
    }
    const std::string reason = claim.value("unavailable_reason", "");
    return reason != "mock_synthesis" && reason != "inclusion_unavailable"
        && reason != "evidence_text_missing";
}

struct CompletionTrace {
    bool needed = false;
    std::string text;
    std::string generation_id;
    nlohmann::json record;
};

std::vector<CompletionTrace> unavailableCompletionTraces(
    const std::string& plan_id,
    const std::string& judgment_id,
    const nlohmann::json& payload,
    const std::vector<RecordedCall>& evidence_calls) {
    std::vector<CompletionTrace> traces;
    if (!payload.contains("claims") || !payload["claims"].is_array()) {
        return traces;
    }
    std::size_t call_index = 0;
    for (const auto& claim : payload["claims"]) {
        if (!claim.is_object() || !claimCalledModel(claim)) {
            continue;
        }
        const bool unavailable = claim.value("evaluation_status", "") != "available";
        if (call_index >= evidence_calls.size()) {
            if (unavailable) {
                CompletionTrace missing;
                missing.needed = true;
                missing.record = {
                    {"plan_id", plan_id},
                    {"judgment_id", judgment_id},
                    {"claim_index", claim.value("claim_index", -1)},
                    {"unavailable_reason", claim.value("unavailable_reason", "")},
                    {"generation_id", claim.value("generation_id", "")},
                    {"error", "completion_not_recorded"},
                };
                traces.push_back(std::move(missing));
            }
            continue;
        }
        const RecordedCall& call = evidence_calls[call_index++];
        if (!unavailable) {
            continue;
        }
        CompletionTrace trace;
        trace.needed = true;
        trace.text = call.text;
        trace.generation_id = claim.value("generation_id", "");
        trace.record = {
            {"plan_id", plan_id},
            {"judgment_id", judgment_id},
            {"answer_id", payload.value("answer_id", "")},
            {"claim_index", claim.value("claim_index", -1)},
            {"unavailable_reason", claim.value("unavailable_reason", "")},
            {"generation_id", trace.generation_id},
            {"finish_reason", call.finish_reason},
            {"ok", call.ok},
            {"error", call.error},
            {"elapsed_ms", call.elapsed_ms},
            {"text", call.text},
        };
        traces.push_back(std::move(trace));
    }
    return traces;
}

bool writeCompletionTrace(const fs::path& logs_dir, const CompletionTrace& trace) {
    const std::string stem_name = trace.generation_id.empty()
        ? "claim-" + std::to_string(trace.record.value("claim_index", -1))
        : trace.generation_id;
    const fs::path stem = logs_dir / (stem_name + "-completion");
    std::ofstream raw(stem.string() + ".txt");
    std::ofstream record(stem.string() + ".json");
    if (!raw.is_open() || !record.is_open()) {
        return false;
    }
    raw << trace.text;
    record << trace.record.dump(2) << "\n";
    if (!raw || !record) {
        return false;
    }
    std::cout << "diagnostic=" << stem.string() << ".txt\n";
    return true;
}

bool selfCheckCompletionTrace() {
    const nlohmann::json available = {
        {"answer_id", "answer-1"},
        {"claims", nlohmann::json::array({
            {{"claim_index", 0}, {"evaluation_status", "available"}, {"label", "supported"},
             {"generation_id", "gen-available"}},
        })},
    };
    RecordedCall kept;
    kept.prompt = "EVIDENCE PASSAGE START\nkept";
    kept.text = "{\"label\":\"supported\",\"evidence_quote\":\"kept\"}";
    kept.finish_reason = "stop";
    kept.ok = true;
    if (!unavailableCompletionTraces("plan", "cm1e1:answer-1", available, {kept}).empty()) {
        fail("harness: an available judgment requested a completion trace");
        return false;
    }

    RecordedCall synthesis;
    synthesis.prompt = "Using only the retrieved notes";
    synthesis.text = "The relay cache stores both tokens.";
    synthesis.finish_reason = "stop";
    RecordedCall parsed;
    parsed.prompt = "EVIDENCE PASSAGE START\npassage";
    parsed.text = "not a json object";
    parsed.finish_reason = "stop";
    parsed.ok = true;
    parsed.elapsed_ms = 12;
    const nlohmann::json unavailable = {
        {"answer_id", "answer-2"},
        {"claims", nlohmann::json::array({
            {{"claim_index", 0}, {"evaluation_status", "unavailable"},
             {"unavailable_reason", "parse"}, {"generation_id", "gen-parse"}},
        })},
    };
    const auto traces = unavailableCompletionTraces(
        "cm1-ctrl-two-on", "cm1e1:answer-2", unavailable, {parsed});
    if (traces.size() != 1 || traces.front().text != parsed.text
        || traces.front().text == synthesis.text
        || traces.front().record.value("finish_reason", "") != "stop"
        || traces.front().record.value("generation_id", "") != "gen-parse"
        || traces.front().record.value("judgment_id", "") != "cm1e1:answer-2"
        || traces.front().record.value("unavailable_reason", "") != "parse") {
        fail("harness: an unavailable judgment did not keep its model completion");
        return false;
    }

    std::string pattern = std::string(kRootPrefix) + "traceXXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (mkdtemp(buffer.data()) == nullptr) {
        fail("harness: could not create a completion-trace directory");
        return false;
    }
    const fs::path dir(buffer.data());
    const bool wrote = writeCompletionTrace(dir, traces.front());
    std::string saved;
    if (wrote) {
        std::ifstream in((dir / "gen-parse-completion.txt").string());
        std::getline(in, saved);
    }
    std::error_code error;
    fs::remove_all(dir, error);
    if (!wrote || saved != parsed.text || error) {
        fail("harness: the completion trace file did not keep the model text");
        return false;
    }
    if (retainWorkspace(0, false) || !retainWorkspace(0, true)
        || !retainWorkspace(1, false) || !retainWorkspace(1, true)
        || retainWorkspace(2, false) || !retainWorkspace(2, true)) {
        fail("harness: an unavailable judgment trace would be deleted");
        return false;
    }
    return true;
}

bool judgmentOutcomeAccepted(const nlohmann::json& payload, std::string& why) {
    if (payload.value("claims_total", 0) < 1) {
        why = "production: judgment extracted no claims";
        return false;
    }
    if (!payload.contains("claims") || !payload["claims"].is_array() || payload["claims"].empty()) {
        why = "production: judgment persisted no claim rows";
        return false;
    }
    for (const auto& claim : payload["claims"]) {
        if (!claim.is_object()) {
            why = "production: judgment claim row is not an object";
            return false;
        }
        const std::string status = claim.value("evaluation_status", "");
        if (status == "available") {
            const std::string label = claim.value("label", "");
            if (label != "supported" && label != "contradicted" && label != "insufficient_evidence") {
                why = "production: judgment label is not a known evidence label";
                return false;
            }
            continue;
        }
        if (status != "unavailable") {
            why = "production: judgment claim has no label or unavailable outcome";
            return false;
        }
        const std::string reason = claim.value("unavailable_reason", "");
        if (reason.empty()) {
            why = "production: unavailable judgment omitted its reason";
            return false;
        }
        if (reason == "mock_synthesis" || reason == "inclusion_unavailable"
            || reason == "evidence_text_missing") {
            why = "production: judgment skipped the model (" + reason + ")";
            return false;
        }
    }
    return true;
}

bool hasSynthesisSnapshot(Memory& memory, const std::string& plan_id) {
    for (const auto& snap : memory.listCm1SnapshotsForPlan(plan_id)) {
        if (snap.step_id == kSynthesizeStep && snap.snapshot_type == "synthesis_observed") {
            return true;
        }
    }
    return false;
}

bool breadthAbsent(Memory& memory, const std::string& plan_id) {
    for (const auto& snap : memory.listCm1SnapshotsForPlan(plan_id)) {
        if (memory.loadCm1BreadthEstimate("cm1b2:" + snap.snapshot_id)) {
            return false;
        }
    }
    return true;
}

void printMeasurements(const ArmRecord& arm) {
    std::cout << "arm=" << arm.name
              << " evidence=" << (arm.evidence ? "on" : "off")
              << " plan_id=" << arm.plan_id
              << " prompt_tokens=" << arm.measured.prompt_tokens
              << " completion_tokens=" << arm.measured.completion_tokens
              << " total_tokens=" << arm.measured.total_tokens
              << " past_plan_duration_ms=" << arm.measured.past_plan_duration_ms
              << " synthesis_latency_ms=" << arm.measured.synthesis_latency_ms
              << " goal_wall_clock_ms=" << arm.measured.goal_wall_clock_ms
              << " evidence_calls=" << arm.evidence_calls
              << " " << arm.judgment_summary
              << "\n";
}

bool compareFingerprints(const Fingerprint& off, const Fingerprint& on) {
    auto check = [&](bool ok, const std::string& label) {
        if (!ok) {
            fail("production: deterministic mismatch: " + label);
        }
        return ok;
    };
    bool ok = true;
    ok = check(off.step_ids == on.step_ids, "step ids") && ok;
    ok = check(off.step_statuses == on.step_statuses, "step statuses") && ok;
    ok = check(off.retrieval_hashes == on.retrieval_hashes, "retrieval content hashes") && ok;
    ok = check(off.final_scores.size() == on.final_scores.size(), "GRAG final score count") && ok;
    if (off.final_scores.size() == on.final_scores.size()) {
        for (std::size_t i = 0; i < off.final_scores.size(); ++i) {
            ok = check(nearlyEqual(off.final_scores[i], on.final_scores[i]),
                       "GRAG final score " + std::to_string(i))
                && ok;
        }
    }
    ok = check(sameEdges(off.edges, on.edges), "graph edges") && ok;
    ok = check(off.trajectories == on.trajectories, "trajectory count") && ok;
    ok = check(off.past_plans == on.past_plans, "past-plan count") && ok;
    ok = check(off.strategies == on.strategies, "strategy count") && ok;
    return ok;
}

bool reopenAgrees(const ArmRecord& arm,
                   const std::string& answer_text,
                   const std::string& generation_id,
                   const std::string& judgment_payload);

bool productionEdge(const std::vector<EdgeSnap>& edges, const std::string& from, const std::string& to) {
    if (edges.size() != 1 || edges.front().from_id != from || edges.front().to_id != to) {
        return false;
    }
    Memory::Edge seed;
    seed.from_id = from;
    seed.to_id = to;
    seed.weight = 0.1f;
    seed.success_count = 0;
    seed.failure_count = 0;
    const auto reinforced =
        Thoth::GraphRefiner(std::shared_ptr<Memory>{}).computeReinforcedEdges({seed}, 1.0f);
    if (reinforced.size() != 1) {
        return false;
    }
    return edges.front().success_count == reinforced.front().success_count
        && edges.front().failure_count == reinforced.front().failure_count
        && nearlyEqual(edges.front().weight, weightAfterHistoryAging(reinforced.front().weight));
}

bool runArm(const fs::path& store_dir,
            const std::string& plan_id,
            bool evidence,
            int wait_seconds,
            const Scenario& scenario,
            ArmRecord& out) {
    out.name = std::string(scenario.name) == "single"
        ? (evidence ? "on" : "off")
        : std::string(scenario.name) + (evidence ? "-on" : "-off");
    out.evidence = evidence;
    out.plan_id = plan_id;
    fs::create_directories(store_dir / "logs");
    fs::create_directories(store_dir / "corpus");
    if (!restrictDirectory(store_dir) || !restrictDirectory(store_dir / "logs")
        || !restrictDirectory(store_dir / "corpus")) {
        return false;
    }

    setenv("THOTH_WORKSPACE_PATH", store_dir.string().c_str(), 1);
    setenv("THOTH_LOGS_PATH", (store_dir / "logs").string().c_str(), 1);
    const fs::path generation_log = store_dir / "logs" / "generation_calls.jsonl";
    const fs::path metrics_log = store_dir / "logs" / "cognitive_metrics.jsonl";
    setenv("THOTH_GENERATION_CALLS_LOG", generation_log.string().c_str(), 1);
    setenv("THOTH_COGNITIVE_METRICS_LOG", metrics_log.string().c_str(), 1);
    setenv("THOTH_CM1_TELEMETRY", "1", 1);
    unsetenv("THOTH_CM1_BREADTH");
    unsetenv("THOTH_MOCK_LLM");
    unsetenv("THOTH_INJECT_CM1_WRITE_FAIL");
    setenv("THOTH_MAX_REFLECTIONS", "0", 1);
    if (evidence) {
        setenv("THOTH_CM1_EVIDENCE", "1", 1);
    } else {
        unsetenv("THOTH_CM1_EVIDENCE");
    }
    out.logs_dir = (store_dir / "logs").string();

    FileHandler path_check;
    const std::string workspace = path_check.getAgentWorkspacePath();
    const std::string database = (fs::path(workspace) / "memory.db").string();
    if (!isolatedPath(workspace) || !isolatedPath(database) || !isolatedPath(generation_log.string())
        || !isolatedPath(metrics_log.string())) {
        fail(std::string("harness: arm path escaped the temporary root: ") + workspace);
        return false;
    }
    out.db_path = database;

    for (const auto& note : scenario.notes) {
        std::ofstream out_note(store_dir / "corpus" / note.file_name);
        out_note << note.line << '\n';
    }

    std::string saved_answer;
    std::string saved_generation;
    std::string saved_payload;
    {
    Config config;
    config.database_path = database;
    auto memory = std::make_shared<Memory>(config);
    memory->setActiveSessionId(kSessionId);

    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &config);
    EmbeddingEngine* engine_ptr = engine.get();
    for (const auto& note : scenario.notes) {
        engine_ptr->updateVocabulary(noteBody(note));
    }
    engine_ptr->updateVocabulary(scenario.goal);
    auto index = std::make_unique<IndexManager>(engine_ptr);
    for (const auto& note : scenario.notes) {
        const std::string note_path =
            fs::absolute(store_dir / "corpus" / note.file_name).lexically_normal().string();
        if (!isolatedPath(note_path)) {
            fail(std::string("harness: note path escaped the temporary root: ") + note_path);
            return false;
        }
        index->registerAttachmentOwner(note_path, kSessionId);
        index->indexFile(note_path);
    }
    if (index->getChunks().size() != scenario.notes.size()) {
        fail(scenario.notes.size() == 1
                 ? "harness: the temporary note did not index as one unchanged passage"
                 : "harness: the temporary notes did not index as one passage each");
        return false;
    }
    for (const auto& note : scenario.notes) {
        const std::string body = noteBody(note);
        bool found = false;
        for (const auto& chunk : index->getChunks()) {
            const std::string file_name = note.file_name;
            if (chunk.code == body && chunk.code.find(note.token) != std::string::npos
                && chunk.fileName.size() >= file_name.size()
                && chunk.fileName.compare(chunk.fileName.size() - file_name.size(),
                                          file_name.size(), file_name)
                    == 0) {
                found = true;
            }
        }
        if (!found) {
            fail(scenario.notes.size() == 1
                     ? "harness: the temporary note did not index as one unchanged passage"
                     : "harness: a temporary note was not indexed unchanged");
            return false;
        }
    }

    auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get(), &config, memory.get());
    LLMInterface llm(LLMBackend::Ollama, &config);
    const auto endpoints = Thoth::resolveInferenceEndpoints(config);
    std::unique_ptr<Thoth::InferenceClient> inner;
    try {
        inner = Thoth::createInferenceClient(endpoints, &config);
    } catch (const std::exception& ex) {
        fail(std::string("harness: inference client was not created: ") + ex.what());
        return false;
    }
    auto recorder_holder = std::make_unique<RecordingClient>(std::move(inner));
    RecordingClient* recorder = recorder_holder.get();
    llm.setInferenceClientForTests(std::move(recorder_holder));

    auto planner = std::make_shared<ScriptedPassagePlanner>(plan_id, scenario);
    auto tools = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, tools, rag, memory);
    controller.set_llm_interface(&llm);
    controller.set_config(&config);
    controller.set_session_id(kSessionId);
    controller.set_max_reflections(0);
    controller.execute_goal(scenario.goal);

    const bool completed = waitForCompleted(controller, wait_seconds);
    const Plan plan = controller.get_current_plan();
    const auto prompts = recorder->prompts();
    const Thoth::ControllerState state = controller.get_state();
    if (!completed || state != Thoth::ControllerState::COMPLETED) {
        std::string detail = std::string("production: controller finished ") + stateName(state);
        const PlanStep* synthesis = findStep(plan, kSynthesizeStep);
        if (synthesis && synthesis->result.is_object()) {
            detail += " synthesis_error=" + synthesis->result.value("error_message", "");
        }
        fail(detail);
        return false;
    }
    if (plan.plan_id != plan_id) {
        fail("harness: controller replaced the scripted plan id");
        return false;
    }

    const PlanStep* synthesis = findStep(plan, kSynthesizeStep);
    if (!synthesis || synthesis->status != StepStatus::SUCCESS) {
        fail("production: retrieval or synthesis did not succeed");
        return false;
    }
    std::vector<std::string> expected_hashes;
    std::vector<std::string> graph_hashes;
    std::vector<std::int64_t> completed_at;
    out.fp.step_ids.clear();
    out.fp.step_statuses.clear();
    out.fp.final_scores.clear();
    for (const auto& note : scenario.notes) {
        const PlanStep* retrieval = findStep(plan, note.step_id);
        const std::string body = noteBody(note);
        if (!retrieval || retrieval->status != StepStatus::SUCCESS) {
            fail("production: retrieval or synthesis did not succeed");
            return false;
        }
        const auto chunks = retrievalChunks(*retrieval);
        if (chunks.size() != 1 || chunks.front().second != body) {
            fail(scenario.notes.size() == 1
                     ? "production: retrieval did not return the one indexed passage"
                     : "production: retrieval did not return each indexed passage");
            return false;
        }
        const std::string selected = chunks.front().second;
        const bool singleCandidate = scenario.notes.size() == 1;
        const auto& diagnostics = retrieval->result.contains("diagnostics")
            ? retrieval->result["diagnostics"]
            : nlohmann::json();
        const int breakdownCount = diagnostics.is_object() && diagnostics.contains("breakdowns")
                && diagnostics["breakdowns"].is_array()
            ? static_cast<int>(diagnostics["breakdowns"].size())
            : 0;
        const auto scores = finalScores(*retrieval);
        if (singleCandidate && (breakdownCount != 1 || scores.size() != 1)) {
            fail(breakdownCount != 1
                     ? "production: retrieval diagnostics did not keep the included passage text"
                     : "production: retrieval diagnostics did not record one GRAG final score");
            return false;
        }
        float selectedScore = 0.0f;
        const DiagnosticMatch match = matchSelectedDiagnostic(retrieval->result, 0, selected, selectedScore);
        if (match != DiagnosticMatch::Ok) {
            fail(singleCandidate
                     ? (match == DiagnosticMatch::Score
                            ? "production: retrieval diagnostics did not record one GRAG final score"
                            : "production: retrieval diagnostics did not keep the included passage text")
                     : (match == DiagnosticMatch::Score
                            ? "production: the selected passage score does not match its diagnostic entry"
                            : "production: the selected passage does not match the top diagnostic candidate"));
            return false;
        }
        expected_hashes.push_back(Thoth::sha256Hex(body));
        graph_hashes.push_back(Memory::calculateContentHash(body));
        completed_at.push_back(retrieval->completed_at_ms);
        out.fp.step_ids.push_back(retrieval->step_id);
        out.fp.step_statuses.push_back(static_cast<int>(retrieval->status));
        out.fp.final_scores.push_back(selectedScore);
    }
    if (!synthesis->result.is_object() || !synthesis->result.value("cm1_included_in_synthesis_available", false)
        || synthesis->result.value("synthesis_context_truncated", true)) {
        fail("production: synthesis did not record an untruncated inclusion list");
        return false;
    }
    const auto hashes = inclusionHashes(*synthesis);
    if (hashes != expected_hashes) {
        fail(scenario.notes.size() == 1
                 ? "production: synthesis inclusion hash is not the indexed passage"
                 : "production: synthesis did not include both passages in retrieval order");
        return false;
    }
    std::string answer_text;
    if (synthesis->result.contains("data") && synthesis->result["data"].is_object()) {
        answer_text = synthesis->result["data"].value("response", "");
    }
    const std::string generation_id = synthesis->result.value("generation_id", "");
    if (answer_text.empty() || generation_id.empty()) {
        fail("production: synthesis completed without an answer and generation id");
        return false;
    }

    std::vector<std::string> synthesis_prompts;
    std::vector<std::string> evidence_prompts;
    for (const auto& prompt : prompts) {
        if (prompt.find("EVIDENCE PASSAGE START\n") != std::string::npos) {
            evidence_prompts.push_back(prompt);
        } else {
            synthesis_prompts.push_back(prompt);
        }
    }
    if (synthesis_prompts.size() != 1) {
        fail("production: synthesis prompt did not contain the included passage");
        return false;
    }
    for (const auto& note : scenario.notes) {
        if (synthesis_prompts.front().find(noteBody(note)) == std::string::npos) {
            fail("production: synthesis prompt did not contain the included passage");
            return false;
        }
    }

    std::vector<Thoth::Cm1EvidenceChunk> included_chunks;
    for (std::size_t i = 0; i < hashes.size(); ++i) {
        Thoth::Cm1EvidenceChunk included;
        included.content = noteBody(scenario.notes[i]);
        included.content_sha256 = hashes[i];
        included_chunks.push_back(std::move(included));
    }
    const std::string expected_region = passageRegion(
        Thoth::buildCm1EvidencePrompt("claim", included_chunks));
    const int logged_evidence = countCallType(generation_log, plan_id, "evidence_judgment");
    const int logged_synthesis = countCallType(generation_log, plan_id, "synthesis");
    if (logged_synthesis != 1) {
        fail("production: generation log does not contain one synthesis call for this plan");
        return false;
    }

    const std::string answer_id = Thoth::makeCm1AnswerId(plan_id, kSynthesizeStep, generation_id);
    const std::string answer_hash = Thoth::sha256Hex(answer_text);
    if (evidence) {
        if (evidence_prompts.empty() || logged_evidence < 1
            || static_cast<int>(evidence_prompts.size()) != logged_evidence) {
            fail("production: evidence arm did not make a recorded evidence_judgment call");
            return false;
        }
        for (const auto& prompt : evidence_prompts) {
            const std::string region = passageRegion(prompt);
            if (region != expected_region
                || passageMarkerCount(region) != static_cast<int>(included_chunks.size())) {
                fail("production: judgment received a different evidence passage than synthesis included");
                return false;
            }
        }
    } else if (!evidence_prompts.empty() || logged_evidence != 0) {
        fail("production: evidence-off arm made an evidence_judgment call");
        return false;
    }

    out.fp.step_ids.push_back(synthesis->step_id);
    out.fp.step_statuses.push_back(static_cast<int>(synthesis->status));
    out.fp.retrieval_hashes = expected_hashes;
    out.fp.edges = edgesOf(*memory);
    if (scenario.notes.size() >= 2) {
        // One retrieval set has no pair to connect, so the single-passage plan
        // records no edge. Two completed sets reinforce one edge in completion order.
        if (completed_at.size() != 2 || graph_hashes.size() != 2 || graph_hashes[0] == graph_hashes[1]) {
            fail("harness: two passages did not produce two distinct graph identities");
            return false;
        }
        const bool first_finished_earlier = completed_at[0] < completed_at[1];
        const bool second_finished_earlier = completed_at[1] < completed_at[0];
        const bool matched = first_finished_earlier
            ? productionEdge(out.fp.edges, graph_hashes[0], graph_hashes[1])
            : second_finished_earlier
                ? productionEdge(out.fp.edges, graph_hashes[1], graph_hashes[0])
                : productionEdge(out.fp.edges, graph_hashes[0], graph_hashes[1])
                    || productionEdge(out.fp.edges, graph_hashes[1], graph_hashes[0]);
        if (!matched) {
            fail("production: two retrieval sets did not reinforce one graph edge");
            return false;
        }
    }
    out.fp.trajectories = static_cast<int>(memory->getAllTrajectories().size());
    out.fp.past_plans = static_cast<int>(memory->getAllPastPlans().size());
    out.fp.strategies = static_cast<int>(memory->getAllStrategies().size());
    if (!readMeasurements(metrics_log, plan_id, *memory, out.measured)) {
        return false;
    }
    out.evidence_calls = logged_evidence;

    if (!hasSynthesisSnapshot(*memory, plan_id)) {
        fail("production: synthesis snapshot was not persisted");
        return false;
    }
    const auto answer = memory->loadCm1AnswerVersion(answer_id);
    if (!answer || answer->generation_id != generation_id || answer->content_sha256 != answer_hash
        || answer->answer_id != answer_id) {
        fail("production: answer version does not match the synthesis generation");
        return false;
    }
    if (!breadthAbsent(*memory, plan_id)) {
        fail("production: breadth estimate was written while breadth was disabled");
        return false;
    }

    const std::string judgment_id = std::string("cm1e1:") + answer_id;
    if (!evidence) {
        if (memory->loadCm1EvidenceJudgment(judgment_id)) {
            fail("production: evidence-off arm persisted a judgment");
            return false;
        }
        out.judgment_summary = "judgment=absent";
    } else {
        const auto judgment = memory->loadCm1EvidenceJudgment(judgment_id);
        if (!judgment || judgment->answer_id != answer_id || judgment->content_sha256 != answer_hash
            || judgment->estimator_version != Thoth::kCm1EvidenceVersion) {
            fail("production: judgment is not tied to the synthesis answer");
            return false;
        }
        nlohmann::json payload;
        try {
            payload = nlohmann::json::parse(judgment->payload_json);
        } catch (...) {
            fail("production: judgment payload is not JSON");
            return false;
        }
        if (payload.value("answer_id", "") != answer_id
            || payload.value("content_sha256", "") != answer_hash) {
            fail("production: judgment payload does not repeat the answer id and content hash");
            return false;
        }
        std::string why;
        if (!judgmentOutcomeAccepted(payload, why)) {
            fail(why);
            return false;
        }
        out.claims_total = payload.value("claims_total", 0);
        out.judgment_summary = judgmentSummary(payload);
        saved_payload = judgment->payload_json;
        const auto traces = unavailableCompletionTraces(
            plan_id, judgment_id, payload, recorder->evidenceCalls());
        for (const auto& trace : traces) {
            if (trace.record.value("error", "") == "completion_not_recorded"
                || !writeCompletionTrace(store_dir / "logs", trace)) {
                fail("harness: unavailable judgment completion was not saved");
                return false;
            }
            g_preserve_trace = true;
        }
    }
    saved_answer = answer_text;
    saved_generation = generation_id;
    }
    return reopenAgrees(out, saved_answer, saved_generation, saved_payload);
}

bool reopenAgrees(const ArmRecord& arm,
                   const std::string& answer_text,
                   const std::string& generation_id,
                   const std::string& judgment_payload) {
    Config config;
    config.database_path = arm.db_path;
    Memory reopen(config);
    const std::string answer_id = Thoth::makeCm1AnswerId(arm.plan_id, kSynthesizeStep, generation_id);
    const auto answer = reopen.loadCm1AnswerVersion(answer_id);
    if (!answer || answer->content_sha256 != Thoth::sha256Hex(answer_text)
        || answer->generation_id != generation_id || answer->answer_id != answer_id) {
        fail("production: reopened database lost the answer version");
        return false;
    }
    if (!hasSynthesisSnapshot(reopen, arm.plan_id)) {
        fail("production: reopened database lost the synthesis snapshot");
        return false;
    }
    const auto judgment = reopen.loadCm1EvidenceJudgment("cm1e1:" + answer_id);
    if (!arm.evidence) {
        if (judgment) {
            fail("production: reopened evidence-off database contains a judgment");
            return false;
        }
        return true;
    }
    if (!judgment || judgment->answer_id != answer->answer_id
        || judgment->content_sha256 != answer->content_sha256
        || judgment->payload_json != judgment_payload) {
        fail("production: reopened database lost the judgment association");
        return false;
    }
    return true;
}

bool runScenario(const fs::path& root, const Scenario& scenario, int wait_seconds) {
    ArmRecord off;
    ArmRecord on;
    const bool single = std::string(scenario.name) == "single";
    const fs::path dir = single ? root : root / scenario.name;
    const std::string off_id = single ? "cm1-ctrl-off" : std::string("cm1-ctrl-") + scenario.name + "-off";
    const std::string on_id = single ? "cm1-ctrl-on" : std::string("cm1-ctrl-") + scenario.name + "-on";
    if (!runArm(dir / "off", off_id, false, wait_seconds, scenario, off)) {
        if (g_fail.empty()) {
            std::cerr << "cm1-live-controller INCONCLUSIVE: " << scenario.name
                      << " evidence-off arm stopped before the goal\n";
        }
        return false;
    }
    printMeasurements(off);
    if (!runArm(dir / "on", on_id, true, wait_seconds, scenario, on)) {
        printMeasurements(off);
        if (g_fail.empty()) {
            std::cerr << "cm1-live-controller INCONCLUSIVE: " << scenario.name
                      << " evidence-on arm stopped before the goal\n";
        }
        return false;
    }
    printMeasurements(on);

    const std::size_t passages = scenario.notes.size();
    if (off.fp.retrieval_hashes.size() != passages || on.fp.retrieval_hashes.size() != passages
        || off.fp.final_scores.size() != passages || on.fp.final_scores.size() != passages
        || off.fp.past_plans != 1 || on.fp.past_plans != 1
        || off.fp.trajectories != 1 || on.fp.trajectories != 1) {
        fail(passages == 1
                 ? "production: a completed arm did not store one passage, one score, one past plan, and one trajectory"
                 : "production: a completed arm did not store both passages, both scores, one past plan, and one trajectory");
        return false;
    }
    if (passages >= 2 && (off.fp.edges.size() != 1 || on.fp.edges.size() != 1)) {
        fail("production: two retrieval sets did not reinforce one graph edge");
        return false;
    }
    if (on.evidence_calls < 1 || on.claims_total < 1) {
        fail("production: evidence arm completed without a judged claim");
        return false;
    }
    if (!compareFingerprints(off.fp, on.fp)) {
        return false;
    }

    std::cout << "cm1-live-controller PASS";
    if (!single) {
        std::cout << " scenario=" << scenario.name;
    }
    std::cout << " edges=" << on.fp.edges.size()
              << " evidence_calls=" << on.evidence_calls
              << " " << on.judgment_summary
              << "\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--self-check") {
        return selfCheck() ? 0 : 1;
    }
    if (argc != 1) {
        std::cerr << "usage: thoth-cm1-live-controller [--self-check]\n";
        return 2;
    }

    if (envTruthy("THOTH_MOCK_LLM") || envTruthy("THOTH_MOCK_LLM_UNAVAILABLE")
        || envTruthy("THOTH_TEST_SUITE_DEV")) {
        std::cerr << "cm1-live-controller INCONCLUSIVE: mock or test-suite generation is enabled\n";
        return 2;
    }

    EnvGuard env({
        "THOTH_WORKSPACE_PATH",
        "THOTH_LOGS_PATH",
        "THOTH_GENERATION_CALLS_LOG",
        "THOTH_COGNITIVE_METRICS_LOG",
        "THOTH_CM1_TELEMETRY",
        "THOTH_CM1_EVIDENCE",
        "THOTH_CM1_BREADTH",
        "THOTH_MOCK_LLM",
        "THOTH_INJECT_CM1_WRITE_FAIL",
        "THOTH_MAX_REFLECTIONS",
    });

    std::string template_path = std::string(kRootPrefix) + "XXXXXX";
    std::vector<char> buffer(template_path.begin(), template_path.end());
    buffer.push_back('\0');
    if (mkdtemp(buffer.data()) == nullptr) {
        std::cerr << "cm1-live-controller INCONCLUSIVE: could not create a temporary workspace\n";
        return 2;
    }
    const fs::path root(buffer.data());
    const std::string token = std::to_string(static_cast<long long>(getpid())) + "-cm1-live-controller";
    {
        std::ofstream owner(root / kOwnerName);
        owner << token;
    }

    auto finish = [&](int code) {
        if (code == 1) {
            std::cerr << "cm1-live-controller evidence left at " << root << "\n";
            if (!g_fail.empty()) {
                std::cerr << "cm1-live-controller reason: " << g_fail << "\n";
            }
        } else if (retainWorkspace(code, g_preserve_trace)) {
            std::cerr << "cm1-live-controller unavailable judgment trace left at " << root << "\n";
        } else {
            removeOwnedDirectory(root, token);
        }
        return code;
    };
    if (!restrictDirectory(root)) {
        return finish(1);
    }

    setenv("THOTH_WORKSPACE_PATH", root.string().c_str(), 1);
    setenv("THOTH_LOGS_PATH", (root / "logs").string().c_str(), 1);
    fs::create_directories(root / "logs");

    Config probe_config;
    std::string backend_error;
    const auto backend = Thoth::tryResolveInferenceBackend(backend_error);
    if (!backend) {
        std::cerr << "cm1-live-controller INCONCLUSIVE: " << backend_error << "\n";
        return finish(2);
    }
    const auto endpoints = Thoth::resolveInferenceEndpoints(probe_config);
    try {
        auto probe = Thoth::createInferenceClient(endpoints, &probe_config);
        const auto health = probe->health();
        if (!health.reachable) {
            std::cerr << "cm1-live-controller INCONCLUSIVE: inference server unavailable at "
                      << endpoints.base_url;
            if (!health.error.empty()) {
                std::cerr << ": " << health.error;
            }
            std::cerr << "\n";
            return finish(2);
        }
    } catch (const std::exception& ex) {
        std::cerr << "cm1-live-controller INCONCLUSIVE: " << ex.what() << "\n";
        return finish(2);
    }

    const int timeout_seconds = static_cast<int>(Thoth::LlmTimeoutPolicy::timeoutSeconds());
    const int wait_seconds = timeout_seconds * 2 + 120;
    std::cout << "cm1-live-controller start"
              << " backend=" << Thoth::inferenceBackendName(*backend)
              << " endpoint=" << endpoints.base_url
              << " timeout_s=" << timeout_seconds
              << " wait_s=" << wait_seconds
              << " workspace=" << root
              << "\n";

    if (!runScenario(root, singleScenario(), wait_seconds)
        || !runScenario(root, twoScenario(), wait_seconds)) {
        return finish(g_fail.empty() ? 2 : 1);
    }
    return finish(0);
}
