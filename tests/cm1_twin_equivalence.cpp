/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — CM-1 Phase 1.1 twin equivalence harness.
 * Disposable TF-IDF workspaces. Fake chat client. Not an EGAR experiment arm.
 * Not part of default CTest labels that gate experiments.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "alp_sha256.h"
#include "cm1_telemetry.h"
#include "config.h"
#include "embedding_engine.h"
#include "executive_controller.h"
#include "experience_provenance.h"
#include "file_handler.h"
#include "index_manager.h"
#include "inference_client.h"
#include "llm_interface.h"
#include "llm_planner.h"
#include "memory.h"
#include "plan.h"
#include "prompt_factory.h"
#include "rag.h"
#include "sqlite_memory_repository.h"
#include "tools.h"

#include <json.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {

const char* kOwnerName = "OWNER";
const char* kAlpha =
    "uniquealphatoken alpha widget relay-cache note for cm1 twin verification\n";
const char* kBeta =
    "uniquebetatoken beta widget relay-cache note for cm1 twin verification\n";

std::string failReason;

void fail(const std::string& why) {
    if (failReason.empty()) {
        failReason = why;
    }
    std::cerr << "cm1-twin FAIL: " << why << "\n";
}

struct EnvSlot {
    bool had = false;
    std::string value;
};

EnvSlot capture(const char* name) {
    EnvSlot slot;
    if (const char* current = std::getenv(name)) {
        slot.had = true;
        slot.value = current;
    }
    return slot;
}

void restore(const char* name, const EnvSlot& slot) {
    if (slot.had) {
        setenv(name, slot.value.c_str(), 1);
    } else {
        unsetenv(name);
    }
}

bool waitForTerminal(Thoth::ExecutiveController& controller) {
    for (int i = 0; i < 200; ++i) {
        const auto state = controller.get_state();
        if (state == Thoth::ControllerState::COMPLETED || state == Thoth::ControllerState::FAILED
            || state == Thoth::ControllerState::ABORTED) {
            return state == Thoth::ControllerState::COMPLETED;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

std::string planJson() {
    return std::string("{\"plan\":[")
        + "{\"step_id\":\"r-alpha\",\"step_type\":\"RETRIEVAL\","
          "\"description\":\"Retrieve the alpha note\","
          "\"payload\":{\"query\":\"uniquealphatoken\",\"top_k\":5}},"
        + "{\"step_id\":\"r-beta\",\"step_type\":\"RETRIEVAL\","
          "\"description\":\"Retrieve the beta note\","
          "\"depends_on\":[\"r-alpha\"],"
          "\"payload\":{\"query\":\"uniquebetatoken\",\"top_k\":5}},"
        + "{\"step_id\":\"synth\",\"step_type\":\"LLM\","
          "\"description\":\"Summarize the two notes\","
          "\"depends_on\":[\"r-beta\"],\"payload\":{}}]}";
}

class TwinFakeClient : public Thoth::InferenceClient {
public:
    std::vector<std::string> prompts;

    Thoth::InferenceGenerateResult generate(const Thoth::InferenceGenerateRequest& request) override {
        prompts.push_back(request.prompt);
        Thoth::InferenceGenerateResult result;
        result.ok = true;
        result.finish_reason = "stop";
        result.provider_usage_reported = true;
        result.token_usage.prompt_tokens = 32;
        result.elapsed_ms = 1;
        if (request.prompt.find("\nSchema:\n") != std::string::npos) {
            result.text = planJson();
            result.token_usage.completion_tokens = 64;
        } else {
            result.text = "cm1 twin synthesis ok";
            result.token_usage.completion_tokens = 4;
        }
        result.token_usage.total_tokens =
            result.token_usage.prompt_tokens + result.token_usage.completion_tokens;
        return result;
    }

    Thoth::InferenceGenerateResult generateChat(const Thoth::InferenceChatRequest&) override {
        Thoth::InferenceGenerateResult result;
        result.ok = false;
        result.error = "TwinFakeClient does not serve chat";
        return result;
    }

    Thoth::InferenceEmbedResult embed(const Thoth::InferenceEmbedRequest&) override {
        Thoth::InferenceEmbedResult result;
        result.ok = false;
        result.error = "TwinFakeClient does not embed";
        return result;
    }

    Thoth::InferenceHealthResult health() override {
        Thoth::InferenceHealthResult result;
        result.reachable = true;
        return result;
    }

    std::string backendName() const override { return "cm1-twin-fake"; }
};

bool ownedRunDirectory(const fs::path& root, const std::string& token) {
    const fs::path normal = root.lexically_normal();
    const std::string text = normal.string();
    if (text.find("..") != std::string::npos) {
        return false;
    }
    if (text.rfind("/tmp/thoth-cm1-twin-", 0) != 0) {
        return false;
    }
    std::ifstream in(normal / kOwnerName);
    if (!in.is_open()) {
        return false;
    }
    std::string got;
    std::getline(in, got);
    return got == token;
}

void removeOwnedRunDirectory(const fs::path& root, const std::string& token) {
    if (!ownedRunDirectory(root, token)) {
        std::cerr << "cm1-twin: refused cleanup of " << root << "\n";
        return;
    }
    std::error_code error;
    fs::remove_all(root, error);
}

std::string basenameOf(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string normalizePrompt(const std::string& prompt) {
    // Drop absolute temp paths so twins with different directories still compare.
    std::string out = prompt;
    const std::string marker = "/tmp/thoth-cm1-twin-";
    std::size_t pos = 0;
    while ((pos = out.find(marker, pos)) != std::string::npos) {
        const std::size_t end = out.find_first_of(" \n\r\t\"'", pos);
        const std::size_t stop = end == std::string::npos ? out.size() : end;
        const std::string path = out.substr(pos, stop - pos);
        out.replace(pos, stop - pos, basenameOf(path));
        pos += basenameOf(path).size();
    }
    return out;
}

struct EdgeSnap {
    std::string from_id;
    std::string to_id;
    float weight = 0.0f;
    int success_count = 0;
    int failure_count = 0;
};

struct CognitiveFingerprint {
    std::string outcome;
    float trajectory_score = 0.0f;
    std::string plan_origin;
    bool learning_eligible = false;
    std::vector<std::string> step_ids;
    std::vector<std::string> step_types;
    std::vector<std::string> step_statuses;
    std::vector<std::string> retrieval_chunk_hashes; // ordered selected contents
    std::vector<std::string> retrieval_chunk_bases;  // ordered selected basenames
    std::vector<float> retrieval_final_scores;       // ordered from last diagnostics present
    std::vector<std::string> prompts_normalized;
    std::vector<EdgeSnap> edges;
    int past_plans = 0;
    int trajectories = 0;
    int strategies = 0;
    int cm1_snapshots = 0;
    int cm1_answers = 0;
};

bool nearlyEqual(float a, float b) {
    return std::fabs(a - b) < 1e-5f;
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
    return edges;
}

CognitiveFingerprint fingerprint(Thoth::ExecutiveController& controller,
                                 Memory& memory,
                                 TwinFakeClient& fake) {
    CognitiveFingerprint fp;
    const Plan plan = controller.get_current_plan();
    fp.outcome = controller.get_state() == Thoth::ControllerState::COMPLETED ? "completed" : "other";
    fp.plan_origin = Thoth::normalizePlanOrigin(plan.plan_origin);
    fp.learning_eligible = Thoth::learningEligibleOrigin(fp.plan_origin);

    for (const auto& traj : memory.getAllTrajectories()) {
        fp.trajectory_score = traj.success_score;
    }
    fp.past_plans = static_cast<int>(memory.getAllPastPlans().size());
    fp.trajectories = static_cast<int>(memory.getAllTrajectories().size());
    fp.strategies = static_cast<int>(memory.getAllStrategies().size());
    fp.edges = edgesOf(memory);
    fp.cm1_snapshots = static_cast<int>(memory.listCm1SnapshotsForPlan(plan.plan_id).size());
    for (const auto& snap : memory.listCm1SnapshotsForPlan(plan.plan_id)) {
        (void)snap;
    }
    // Count answer versions by probing known synth step if present.
    for (const auto& step : plan.steps) {
        fp.step_ids.push_back(step.step_id);
        fp.step_types.push_back(std::to_string(static_cast<int>(step.type)));
        fp.step_statuses.push_back(std::to_string(static_cast<int>(step.status)));
        if (step.type == StepType::RETRIEVAL && step.result.is_object()) {
            const nlohmann::json* data = nullptr;
            if (step.result.contains("data") && step.result["data"].is_object()) {
                if (step.result["data"].contains("chunks")) {
                    data = &step.result["data"];
                } else if (step.result["data"].contains("data")
                           && step.result["data"]["data"].is_object()) {
                    data = &step.result["data"]["data"];
                }
            }
            if (data && data->contains("chunks") && (*data)["chunks"].is_array()) {
                for (const auto& chunk : (*data)["chunks"]) {
                    if (!chunk.is_object()) continue;
                    const std::string file = chunk.value("file", "");
                    const std::string content = chunk.value("content", "");
                    fp.retrieval_chunk_bases.push_back(basenameOf(file));
                    fp.retrieval_chunk_hashes.push_back(Thoth::sha256Hex(content));
                }
            }
            if (step.result.contains("diagnostics") && step.result["diagnostics"].is_object()) {
                const auto& d = step.result["diagnostics"];
                if (d.contains("final_scores") && d["final_scores"].is_array()) {
                    for (const auto& s : d["final_scores"]) {
                        if (s.is_number()) {
                            fp.retrieval_final_scores.push_back(s.get<float>());
                        }
                    }
                }
            }
        }
        if (step.type == StepType::LLM && step.result.is_object()
            && step.result.contains("generation_id") && step.result["generation_id"].is_string()) {
            const auto ans = memory.loadCm1AnswerVersion(Thoth::makeCm1AnswerId(
                plan.plan_id, step.step_id, step.result["generation_id"].get<std::string>()));
            if (ans) ++fp.cm1_answers;
        }
    }
    for (const auto& prompt : fake.prompts) {
        fp.prompts_normalized.push_back(normalizePrompt(prompt));
    }
    return fp;
}

bool sameEdges(const std::vector<EdgeSnap>& a, const std::vector<EdgeSnap>& b) {
    if (a.size() != b.size()) return false;
    for (const auto& edge : a) {
        bool found = false;
        for (const auto& other : b) {
            if (edge.from_id == other.from_id && edge.to_id == other.to_id
                && edge.success_count == other.success_count
                && edge.failure_count == other.failure_count
                && nearlyEqual(edge.weight, other.weight)) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

bool compareCognitive(const CognitiveFingerprint& off, const CognitiveFingerprint& on) {
    auto check = [&](bool ok, const std::string& label) {
        if (!ok) {
            fail("cognitive mismatch: " + label);
        }
        return ok;
    };
    bool ok = true;
    ok = check(off.outcome == on.outcome, "outcome") && ok;
    ok = check(nearlyEqual(off.trajectory_score, on.trajectory_score), "trajectory_score") && ok;
    ok = check(off.plan_origin == on.plan_origin, "plan_origin") && ok;
    ok = check(off.learning_eligible == on.learning_eligible, "learning_eligible") && ok;
    ok = check(off.step_ids == on.step_ids, "step_ids") && ok;
    ok = check(off.step_types == on.step_types, "step_types") && ok;
    ok = check(off.step_statuses == on.step_statuses, "step_statuses") && ok;
    ok = check(off.retrieval_chunk_hashes == on.retrieval_chunk_hashes, "retrieval chunk hashes")
         && ok;
    ok = check(off.retrieval_chunk_bases == on.retrieval_chunk_bases, "retrieval chunk basenames")
         && ok;
    ok = check(off.retrieval_final_scores.size() == on.retrieval_final_scores.size(),
               "final_scores size")
         && ok;
    if (off.retrieval_final_scores.size() == on.retrieval_final_scores.size()) {
        for (std::size_t i = 0; i < off.retrieval_final_scores.size(); ++i) {
            ok = check(nearlyEqual(off.retrieval_final_scores[i], on.retrieval_final_scores[i]),
                       "final_score[" + std::to_string(i) + "]")
                 && ok;
        }
    }
    ok = check(off.prompts_normalized == on.prompts_normalized, "prompts") && ok;
    ok = check(sameEdges(off.edges, on.edges), "graph edges") && ok;
    ok = check(off.past_plans == on.past_plans, "past_plans") && ok;
    ok = check(off.trajectories == on.trajectories, "trajectories") && ok;
    ok = check(off.strategies == on.strategies, "strategies") && ok;
    return ok;
}

struct ArmResult {
    CognitiveFingerprint fp;
    std::string plan_id;
    std::string db_path;
    fs::path store_dir;
};

void seedCorpusInWorkspace(const fs::path& storeDir) {
    const fs::path corpus = storeDir / "corpus";
    fs::create_directories(corpus);
    std::ofstream(corpus / "alpha.md") << kAlpha;
    std::ofstream(corpus / "beta.md") << kBeta;
}

bool runArm(const fs::path& storeDir,
            const char* cm1Flag,
            ArmResult& out,
            const char* breadthFlag = nullptr) {
    setenv("THOTH_WORKSPACE_PATH", storeDir.string().c_str(), 1);
    setenv("THOTH_CM1_TELEMETRY", cm1Flag, 1);
    if (breadthFlag && breadthFlag[0] != '\0' && std::string(breadthFlag) != "0") {
        setenv("THOTH_CM1_BREADTH", breadthFlag, 1);
    } else {
        unsetenv("THOTH_CM1_BREADTH");
    }
    unsetenv("THOTH_INJECT_CM1_WRITE_FAIL");
    unsetenv("THOTH_MOCK_LLM");
    seedCorpusInWorkspace(storeDir);

    Config config;
    config.database_path = (storeDir / "memory.db").string();
    config.llm_model = "deterministic-fake";
    auto memory = std::make_shared<Memory>(config);
    FileHandler pathCheck;
    const std::string openedDb = (fs::path(pathCheck.getAgentWorkspacePath()) / "memory.db").string();
    if (openedDb.find("/tmp/thoth-cm1-twin-") != 0 || openedDb.find("agent_workspace") != std::string::npos) {
        fail(std::string("database not in owned temp workspace: ") + openedDb);
        return false;
    }
    out.db_path = openedDb;
    out.store_dir = storeDir;

    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &config);
    EmbeddingEngine* enginePtr = engine.get();
    enginePtr->updateVocabulary(kAlpha);
    enginePtr->updateVocabulary(kBeta);
    enginePtr->updateVocabulary("cm1 twin goal retrieve alpha beta");

    auto index = std::make_unique<IndexManager>(enginePtr);
    memory->setActiveSessionId("cm1-twin");
    const fs::path corpus = storeDir / "corpus";
    const std::string alphaPath = fs::absolute(corpus / "alpha.md").lexically_normal().string();
    const std::string betaPath = fs::absolute(corpus / "beta.md").lexically_normal().string();
    index->registerAttachmentOwner(alphaPath, "cm1-twin");
    index->registerAttachmentOwner(betaPath, "cm1-twin");
    index->indexFile(alphaPath);
    index->indexFile(betaPath);

    auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get(), &config, memory.get());
    auto prompts = std::make_shared<PromptFactory>(*memory, *rag);
    LLMInterface llm(LLMBackend::Ollama, &config);
    auto fakeHolder = std::make_unique<TwinFakeClient>();
    TwinFakeClient* fake = fakeHolder.get();
    llm.setInferenceClientForTests(std::move(fakeHolder));
    auto planner = std::make_shared<LLMPlanner>(memory, rag, prompts, &llm);
    auto tools = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, tools, rag, memory);
    controller.set_llm_interface(&llm);
    controller.set_session_id("cm1-twin");
    controller.set_max_reflections(0);

    controller.execute_goal("cm1 twin goal retrieve alpha beta");
    if (!waitForTerminal(controller)) {
        fail(std::string("goal did not complete with CM1=") + cm1Flag);
        return false;
    }
    out.plan_id = controller.get_current_plan().plan_id;
    out.fp = fingerprint(controller, *memory, *fake);

    // Re-open the database to verify CM-1 durability across process-local reopen.
    {
        Config reopenCfg;
        reopenCfg.database_path = config.database_path;
        Memory reopen(reopenCfg);
        const auto snaps = reopen.listCm1SnapshotsForPlan(out.plan_id);
        if (std::string(cm1Flag) == "0") {
            if (!snaps.empty()) {
                fail("CM1=0 arm persisted snapshots after reopen");
                return false;
            }
        } else {
            if (snaps.empty()) {
                fail("CM1=1 arm lost snapshots after reopen");
                return false;
            }
            out.fp.cm1_snapshots = static_cast<int>(snaps.size());
        }
    }
    return true;
}

bool testWriteFailureIsolation(const fs::path& storeDir) {
    setenv("THOTH_WORKSPACE_PATH", storeDir.string().c_str(), 1);
    setenv("THOTH_CM1_TELEMETRY", "1", 1);
    unsetenv("THOTH_CM1_BREADTH");
    setenv("THOTH_INJECT_CM1_WRITE_FAIL", "1", 1);
    unsetenv("THOTH_MOCK_LLM");
    seedCorpusInWorkspace(storeDir);

    Config config;
    config.database_path = (storeDir / "memory.db").string();
    config.llm_model = "deterministic-fake";
    auto memory = std::make_shared<Memory>(config);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &config);
    EmbeddingEngine* enginePtr = engine.get();
    enginePtr->updateVocabulary(kAlpha);
    enginePtr->updateVocabulary(kBeta);
    enginePtr->updateVocabulary("cm1 twin goal retrieve alpha beta");
    auto index = std::make_unique<IndexManager>(enginePtr);
    memory->setActiveSessionId("cm1-twin-fail");
    const fs::path corpus = storeDir / "corpus";
    const std::string alphaPath = fs::absolute(corpus / "alpha.md").lexically_normal().string();
    const std::string betaPath = fs::absolute(corpus / "beta.md").lexically_normal().string();
    index->registerAttachmentOwner(alphaPath, "cm1-twin-fail");
    index->registerAttachmentOwner(betaPath, "cm1-twin-fail");
    index->indexFile(alphaPath);
    index->indexFile(betaPath);
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get(), &config, memory.get());
    auto prompts = std::make_shared<PromptFactory>(*memory, *rag);
    LLMInterface llm(LLMBackend::Ollama, &config);
    auto fakeHolder = std::make_unique<TwinFakeClient>();
    llm.setInferenceClientForTests(std::move(fakeHolder));
    auto planner = std::make_shared<LLMPlanner>(memory, rag, prompts, &llm);
    auto tools = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, tools, rag, memory);
    controller.set_llm_interface(&llm);
    controller.set_session_id("cm1-twin-fail");
    controller.set_max_reflections(0);
    controller.execute_goal("cm1 twin goal retrieve alpha beta");
    if (!waitForTerminal(controller)) {
        fail("write-failure arm did not complete");
        unsetenv("THOTH_INJECT_CM1_WRITE_FAIL");
        return false;
    }
    const Plan plan = controller.get_current_plan();
    const auto snaps = memory->listCm1SnapshotsForPlan(plan.plan_id);
    const auto trajs = memory->getAllTrajectories();
    unsetenv("THOTH_INJECT_CM1_WRITE_FAIL");
    if (!snaps.empty()) {
        fail("write-failure arm fabricated CM-1 snapshots");
        return false;
    }
    if (trajs.empty()) {
        fail("write-failure arm lost trajectory persistence");
        return false;
    }
    if (controller.get_state() != Thoth::ControllerState::COMPLETED) {
        fail("write-failure arm changed goal outcome");
        return false;
    }
    return true;
}

} // namespace

int main() {
    const EnvSlot savedWorkspace = capture("THOTH_WORKSPACE_PATH");
    const EnvSlot savedLogs = capture("THOTH_LOGS_PATH");
    const EnvSlot savedCm1 = capture("THOTH_CM1_TELEMETRY");
    const EnvSlot savedBreadth = capture("THOTH_CM1_BREADTH");
    const EnvSlot savedInject = capture("THOTH_INJECT_CM1_WRITE_FAIL");
    const EnvSlot savedMock = capture("THOTH_MOCK_LLM");
    const EnvSlot savedReflections = capture("THOTH_MAX_REFLECTIONS");

    std::string templatePath = "/tmp/thoth-cm1-twin-XXXXXX";
    std::vector<char> tmpl(templatePath.begin(), templatePath.end());
    tmpl.push_back('\0');
    if (mkdtemp(tmpl.data()) == nullptr) {
        std::cerr << "cm1-twin FAIL: could not create run directory\n";
        return 1;
    }
    const fs::path root(tmpl.data());
    const std::string token = std::to_string(static_cast<long long>(getpid())) + "-cm1-twin";
    {
        std::ofstream owner(root / kOwnerName);
        owner << token;
    }
    const fs::path offDir = root / "off";
    const fs::path onDir = root / "on";
    const fs::path breadthDir = root / "breadth";
    const fs::path failDir = root / "fail";
    fs::create_directories(offDir);
    fs::create_directories(onDir);
    fs::create_directories(breadthDir);
    fs::create_directories(failDir);
    fs::create_directories(root / "logs");

    auto finish = [&](int code) {
        restore("THOTH_WORKSPACE_PATH", savedWorkspace);
        restore("THOTH_LOGS_PATH", savedLogs);
        restore("THOTH_CM1_TELEMETRY", savedCm1);
        restore("THOTH_CM1_BREADTH", savedBreadth);
        restore("THOTH_INJECT_CM1_WRITE_FAIL", savedInject);
        restore("THOTH_MOCK_LLM", savedMock);
        restore("THOTH_MAX_REFLECTIONS", savedReflections);
        if (code == 0) {
            removeOwnedRunDirectory(root, token);
        } else {
            std::cerr << "cm1-twin evidence left at " << root << "\n";
            if (!failReason.empty()) {
                std::cerr << "cm1-twin reason: " << failReason << "\n";
            }
        }
        return code;
    };

    setenv("THOTH_LOGS_PATH", (root / "logs").string().c_str(), 1);
    setenv("THOTH_MAX_REFLECTIONS", "0", 1);

    ArmResult off;
    ArmResult on;
    if (!runArm(offDir, "0", off)) {
        return finish(1);
    }
    if (!runArm(onDir, "1", on)) {
        return finish(1);
    }

    if (off.fp.cm1_snapshots != 0 || off.fp.cm1_answers != 0) {
        fail("CM1=0 arm recorded telemetry");
        return finish(1);
    }
    if (on.fp.cm1_snapshots < 2) {
        fail("CM1=1 arm expected retrieval+synthesis snapshots, got "
             + std::to_string(on.fp.cm1_snapshots));
        return finish(1);
    }
    // Fake client provides generation_id via generateCall; answer row expected.
    if (on.fp.cm1_answers < 1) {
        fail("CM1=1 arm expected at least one answer version");
        return finish(1);
    }

    if (!compareCognitive(off.fp, on.fp)) {
        return finish(1);
    }

    ArmResult breadth;
    if (!runArm(breadthDir, "1", breadth, "1")) {
        return finish(1);
    }
    if (!compareCognitive(on.fp, breadth.fp)) {
        return finish(1);
    }
    {
        Config onCfg;
        onCfg.database_path = on.db_path;
        Memory onMemory(onCfg);
        for (const auto& snap : onMemory.listCm1SnapshotsForPlan(on.plan_id)) {
            if (onMemory.loadCm1BreadthEstimate("cm1b2:" + snap.snapshot_id)) {
                fail("telemetry-only arm wrote a breadth estimate");
                return finish(1);
            }
        }
        Config breadthCfg;
        breadthCfg.database_path = breadth.db_path;
        Memory breadthMemory(breadthCfg);
        const auto snaps = breadthMemory.listCm1SnapshotsForPlan(breadth.plan_id);
        if (snaps.size() < 2) {
            fail("breadth arm lost raw snapshots");
            return finish(1);
        }
        int estimates = 0;
        for (const auto& snap : snaps) {
            if (snap.estimator_status != Thoth::kCm1EstimatorStatus) {
                fail("breadth arm changed raw estimator_status");
                return finish(1);
            }
            const auto features = nlohmann::json::parse(snap.features_json);
            if (features.contains("goal_confidence") || features.contains("evidence_breadth_retrieved")) {
                fail("breadth arm wrote the estimate into the raw snapshot");
                return finish(1);
            }
            const auto estimate = breadthMemory.loadCm1BreadthEstimate("cm1b2:" + snap.snapshot_id);
            if (!estimate) {
                fail("breadth arm missing derived estimate");
                return finish(1);
            }
            const auto payload = nlohmann::json::parse(estimate->payload_json);
            if (payload["agreement_status"] != "not_evaluated"
                || payload.contains("goal_confidence")
                || payload.contains("predicted_correctness")
                || payload["estimator_version"] != Thoth::kCm1BreadthVersion) {
                fail("breadth payload violated the phase-2 contract");
                return finish(1);
            }
            if (snap.snapshot_type == "retrieval_observed"
                && snap.measurement_status != "empty_retrieval"
                && payload["evidence_breadth_retrieved_status"] != "available") {
                fail("retrieval snapshot did not carry per-source query_sim into breadth");
                return finish(1);
            }
            ++estimates;
        }
        if (estimates < 2) {
            fail("breadth arm expected an estimate per snapshot");
            return finish(1);
        }
    }

    if (!testWriteFailureIsolation(failDir)) {
        return finish(1);
    }

    std::cout << "cm1-twin PASS"
              << " off_db=\"" << off.db_path << "\""
              << " on_db=\"" << on.db_path << "\""
              << " cm1_snapshots=" << on.fp.cm1_snapshots
              << " cm1_answers=" << on.fp.cm1_answers
              << " trajectory_score=" << on.fp.trajectory_score
              << " edges=" << on.fp.edges.size()
              << " prompts=" << on.fp.prompts_normalized.size()
              << "\n";
    return finish(0);
}
