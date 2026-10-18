/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — live EGAR lifecycle fixture.
 * Real llama.cpp chat and external 768-dimensional embeddings.
 * Not part of CTest. Not an experiment arm.
 * Store A, Store B, and the graph subcase are temporary databases.
 *
 * The chat and embed base URLs must be set explicitly. This fixture does not
 * open unpublished container ports 8080 or 8081 on its own.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "alp_feature_flags.h"
#include "config.h"
#include "embedding_engine.h"
#include "executive_controller.h"
#include "file_handler.h"
#include "generation_call_log.h"
#include "goal_text_utils.h"
#include "iplanner.h"
#include "index_manager.h"
#include "inference_client.h"
#include "llama_server_client.h"
#include "llm_interface.h"
#include "llm_planner.h"
#include "memory.h"
#include "plan.h"
#include "plan_reuse_config.h"
#include "planner_injection_config.h"
#include "prompt_factory.h"
#include "rag.h"
#include "sqlite_memory_repository.h"
#include "tools.h"

#include <json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
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
const char* kAlphaNote =
    "uniquealphatoken alpha widget relay-cache note for live egar verification\n";
const char* kBetaNote =
    "uniquebetatoken beta widget relay-cache note for live egar verification\n";
const char* kMarkerStrategy = "[RELEVANT STRATEGY";
const char* kMarkerTrajectory = "[PAST EXPERIENCE";
const char* kMarkerPastPlan = "[RELEVANT PAST APPROACHES";
const int kExpectedDimension = 768;
const int kGoalWaitSeconds = 900;

enum class Outcome { Pass, Fail, Inconclusive };

struct EnvSlot {
    bool had = false;
    std::string value;
};

struct GenerateCall {
    bool plan_prompt = false;
    bool ok = false;
    std::string error;
    std::string finish_reason;
    std::size_t prompt_bytes = 0;
    std::string prompt;
};

struct PatternStats {
    std::string key;
    int count = 0;
    float success_sum = 0.0f;
    int evidenced = 0;
};

class RecordingLlamaClient : public Thoth::InferenceClient {
public:
    explicit RecordingLlamaClient(std::unique_ptr<Thoth::LlamaServerClient> inner)
        : inner_(std::move(inner)) {}

    std::vector<GenerateCall> calls;

    Thoth::InferenceGenerateResult generate(const Thoth::InferenceGenerateRequest& request) override {
        GenerateCall call;
        call.plan_prompt = request.prompt.find("\nSchema:\n") != std::string::npos;
        call.prompt_bytes = request.prompt.size();
        call.prompt = request.prompt;
        Thoth::InferenceGenerateResult result = inner_->generate(request);
        call.ok = result.ok;
        call.error = result.error;
        call.finish_reason = result.finish_reason;
        calls.push_back(std::move(call));
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
    std::unique_ptr<Thoth::LlamaServerClient> inner_;
};

class TwoRetrievalPlanner : public IPlanner {
public:
    Plan create_plan(const std::string& goal) override {
        Plan plan;
        plan.plan_id = "live-graph-subcase";
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;

        PlanStep alpha;
        alpha.step_id = "graph-alpha";
        alpha.description = "Retrieve the alpha note";
        alpha.type = StepType::RETRIEVAL;
        alpha.payload = {{"query", "uniquealphatoken"}, {"top_k", 5}};

        PlanStep beta;
        beta.step_id = "graph-beta";
        beta.description = "Retrieve the beta note";
        beta.type = StepType::RETRIEVAL;
        beta.depends_on = {"graph-alpha"};
        beta.payload = {{"query", "uniquebetatoken"}, {"top_k", 5}};

        plan.steps = {alpha, beta};
        return plan;
    }

    Plan revise_plan(const Plan& existing_plan, const nlohmann::json&) override {
        return existing_plan;
    }
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

bool truthy(const char* value) {
    return Thoth::AlpFeatureFlags::truthyEnv(value);
}

std::string lowerCopy(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

bool contextError(const std::string& error) {
    const std::string text = lowerCopy(error);
    return text.find("context") != std::string::npos
        || text.find("n_ctx") != std::string::npos
        || text.find("exceeds the available context") != std::string::npos;
}

std::string goalText(int index) {
    return "Find the widget relay-cache note and summarize the useful evidence. Case "
        + std::to_string(index);
}

bool restrictDirectory(const fs::path& path) {
    std::error_code error;
    fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace, error);
    if (error) {
        std::cerr << "egar-live FAIL: could not set 0700 on " << path
                  << ": " << error.message() << "\n";
        return false;
    }
    return true;
}

bool restrictFile(const fs::path& path) {
    std::error_code error;
    fs::permissions(path,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace,
                    error);
    if (error) {
        std::cerr << "egar-live FAIL: could not set 0600 on " << path
                  << ": " << error.message() << "\n";
        return false;
    }
    return true;
}

bool waitForTerminal(Thoth::ExecutiveController& controller) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kGoalWaitSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto state = controller.get_state();
        if (state == Thoth::ControllerState::COMPLETED
            || state == Thoth::ControllerState::FAILED
            || state == Thoth::ControllerState::ABORTED) {
            return state == Thoth::ControllerState::COMPLETED;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false;
}

void writeNotes(const fs::path& corpus) {
    fs::create_directories(corpus);
    std::ofstream alpha(corpus / "alpha.md");
    alpha << kAlphaNote;
    std::ofstream beta(corpus / "beta.md");
    beta << kBetaNote;
}

bool indexSessionNotes(IndexManager& index, const fs::path& corpus, const std::string& session) {
    const std::string alpha = fs::absolute(corpus / "alpha.md").lexically_normal().string();
    const std::string beta = fs::absolute(corpus / "beta.md").lexically_normal().string();
    index.registerAttachmentOwner(alpha, session);
    index.registerAttachmentOwner(beta, session);
    index.indexFile(alpha);
    index.indexFile(beta);
    if (index.getChunks().size() != 2) {
        return false;
    }
    for (const auto& chunk : index.getChunks()) {
        if (static_cast<int>(chunk.embedding.size()) != kExpectedDimension
            || chunk.corpus_tier != "session_attachment"
            || chunk.owner_context_id != session) {
            return false;
        }
    }
    return true;
}

bool stepHasUsefulEvidence(const PlanStep& step) {
    if (step.type != StepType::RETRIEVAL || !step.result.is_object()) {
        return false;
    }
    if (step.result.value("retrieved_chunk_count", 0) <= 0
        || step.result.value("retrieval_empty", false)) {
        return false;
    }
    const auto diagnostics = step.result.value("diagnostics", nlohmann::json::object());
    const auto breakdowns = diagnostics.value("breakdowns", nlohmann::json::array());
    for (const auto& breakdown : breakdowns) {
        if (breakdown.is_object() && breakdown.value("final_score", 0.0f) > 0.0f
            && !breakdown.value("code_text", std::string()).empty()) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> patternOf(const nlohmann::json& trajectory) {
    std::vector<std::string> steps;
    if (!trajectory.is_object() || !trajectory.contains("steps") || !trajectory["steps"].is_array()) {
        return steps;
    }
    for (const auto& step : trajectory["steps"]) {
        if (!step.is_object()) {
            continue;
        }
        std::string label;
        if (step.contains("tool") && step["tool"].is_string()) {
            const std::string tool = step["tool"].get<std::string>();
            if (!tool.empty() && tool != "none") {
                label = "TOOL:" + tool;
            }
        }
        if (label.empty() && step.contains("type") && step["type"].is_number_integer()) {
            switch (step["type"].get<int>()) {
                case 1: label = "RETRIEVAL"; break;
                case 2: label = "LLM"; break;
                default: label = "STEP_" + std::to_string(step["type"].get<int>()); break;
            }
        }
        if (!label.empty()) {
            steps.push_back(label);
        }
    }
    return steps;
}

std::string patternKey(const std::vector<std::string>& steps) {
    std::string key;
    for (std::size_t i = 0; i < steps.size(); ++i) {
        if (i != 0) {
            key += "->";
        }
        key += steps[i];
    }
    return key;
}

bool trajectoryEvidenced(const nlohmann::json& trajectory) {
    if (!trajectory.is_object() || !trajectory.contains("steps")) {
        return false;
    }
    bool sawRetrieval = false;
    for (const auto& step : trajectory["steps"]) {
        if (!step.is_object() || step.value("type", -1) != 1) {
            continue;
        }
        sawRetrieval = true;
        const auto result = step.value("result", nlohmann::json::object());
        if (result.value("retrieved_chunk_count", 0) <= 0 || result.value("retrieval_empty", false)) {
            return false;
        }
        bool scored = false;
        const auto breakdowns = result.value("diagnostics", nlohmann::json::object())
                                    .value("breakdowns", nlohmann::json::array());
        for (const auto& breakdown : breakdowns) {
            if (breakdown.is_object() && breakdown.value("final_score", 0.0f) > 0.0f) {
                scored = true;
            }
        }
        if (!scored) {
            return false;
        }
    }
    return sawRetrieval;
}

std::vector<std::string> hashesOf(const Plan& plan) {
    std::vector<std::string> hashes;
    for (const auto& set : plan.retrieval_sets) {
        hashes.insert(hashes.end(), set.chunk_hashes.begin(), set.chunk_hashes.end());
    }
    if (!hashes.empty()) {
        return hashes;
    }
    for (const auto& step : plan.steps) {
        if (step.type != StepType::RETRIEVAL || !step.result.is_object()) {
            continue;
        }
        const auto breakdowns = step.result.value("diagnostics", nlohmann::json::object())
                                    .value("breakdowns", nlohmann::json::array());
        for (const auto& breakdown : breakdowns) {
            if (!breakdown.is_object() || breakdown.value("final_score", 0.0f) <= 0.0f) {
                continue;
            }
            const std::string text = breakdown.value("code_text", "");
            if (!text.empty()) {
                hashes.push_back(Memory::calculateContentHash(text));
            }
        }
    }
    return hashes;
}

struct DirectedEdge {
    std::string from;
    std::string to;
};

std::vector<DirectedEdge> requiredCrossSetEdges(const Plan& plan) {
    std::vector<DirectedEdge> edges;
    const auto& sets = plan.retrieval_sets;
    for (std::size_t i = 0; i + 1 < sets.size(); ++i) {
        for (const auto& from : sets[i].chunk_hashes) {
            for (const auto& to : sets[i + 1].chunk_hashes) {
                if (from.empty() || to.empty() || from == to) {
                    continue;
                }
                edges.push_back(DirectedEdge{from, to});
            }
        }
    }
    return edges;
}

enum class FallbackMark { No, Yes, Unknown };

FallbackMark fallbackForPlan(const std::string& planId) {
    if (planId.empty()) {
        return FallbackMark::Unknown;
    }
    std::ifstream in(Thoth::GenerationCallLog::logFilePath());
    if (!in) {
        return FallbackMark::Unknown;
    }
    bool sawPlannerRecord = false;
    bool used = false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        nlohmann::json row;
        try {
            row = nlohmann::json::parse(line);
        } catch (...) {
            return FallbackMark::Unknown;
        }
        if (!row.is_object() || row.value("plan_id", std::string()) != planId) {
            continue;
        }
        const std::string callType = row.value("call_type", std::string());
        if (callType != "plan" && callType != "plan_retry") {
            continue;
        }
        if (!row.contains("fallback_used") || !row["fallback_used"].is_boolean()) {
            return FallbackMark::Unknown;
        }
        sawPlannerRecord = true;
        used = row["fallback_used"].get<bool>();
    }
    if (!sawPlannerRecord) {
        return FallbackMark::Unknown;
    }
    return used ? FallbackMark::Yes : FallbackMark::No;
}

struct Stack {
    std::shared_ptr<Memory> memory;
    std::unique_ptr<IndexManager> index;
    std::shared_ptr<RAGPipeline> rag;
    std::shared_ptr<PromptFactory> prompts;
    std::unique_ptr<LLMInterface> llm;
    RecordingLlamaClient* recorder = nullptr;
    std::shared_ptr<IPlanner> planner;
    std::shared_ptr<ToolRegistry> tools;
    std::unique_ptr<Thoth::ExecutiveController> controller;
};

struct LiveGoalResult {
    bool completed = false;
    bool inference_failed = false;
    bool context_failed = false;
    bool fallback = false;
    std::string inference_error;
    std::string plan_prompt_path;
};

bool writePrompt(const fs::path& path, const std::string& prompt) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) {
        std::cerr << "egar-live FAIL: could not create " << path.parent_path()
                  << ": " << error.message() << "\n";
        return false;
    }
    if (!restrictDirectory(path.parent_path())) {
        return false;
    }
    {
        std::ofstream out(path);
        if (!out) {
            std::cerr << "egar-live FAIL: could not write " << path << "\n";
            return false;
        }
        out << prompt;
        if (!out) {
            std::cerr << "egar-live FAIL: could not write " << path << "\n";
            return false;
        }
    }
    return restrictFile(path);
}

}  // namespace

int main() {
    if (truthy(std::getenv("THOTH_TEST_SUITE_DEV"))) {
        std::cerr << "egar-live FAIL: THOTH_TEST_SUITE_DEV would select TF-IDF\n";
        return 1;
    }
    if (Thoth::AlpFeatureFlags::alpEnabled()) {
        std::cerr << "egar-live FAIL: THOTH_ALP_ENABLED is set; legacy attachment ownership is required\n";
        return 1;
    }
    const char* backend = std::getenv("THOTH_INFERENCE_BACKEND");
    if (!backend || std::string(backend) != "llama_cpp") {
        std::cerr << "egar-live FAIL: THOTH_INFERENCE_BACKEND must be llama_cpp\n";
        return 1;
    }
    const char* chatUrl = std::getenv("THOTH_INFERENCE_BASE_URL");
    const char* embedUrl = std::getenv("THOTH_EMBED_BASE_URL");
    if (!chatUrl || !*chatUrl || !embedUrl || !*embedUrl) {
        std::cerr << "egar-live FAIL: THOTH_INFERENCE_BASE_URL and THOTH_EMBED_BASE_URL are required\n";
        std::cerr << "egar-live: unpublished container ports 8080 and 8081 are not assumed\n";
        return 1;
    }

    const EnvSlot savedStrict = capture("THOTH_EMBED_STRICT");
    const EnvSlot savedReflections = capture("THOTH_MAX_REFLECTIONS");
    const EnvSlot savedWorkspace = capture("THOTH_WORKSPACE_PATH");
    const EnvSlot savedLogs = capture("THOTH_LOGS_PATH");
    setenv("THOTH_EMBED_STRICT", "1", 1);
    setenv("THOTH_MAX_REFLECTIONS", "0", 1);

    char directoryTemplate[] = "/tmp/thoth-v1-live-XXXXXX";
    char* created = mkdtemp(directoryTemplate);
    if (!created) {
        std::cerr << "egar-live FAIL: could not create the run directory\n";
        restore("THOTH_EMBED_STRICT", savedStrict);
        restore("THOTH_MAX_REFLECTIONS", savedReflections);
        return 1;
    }
    const fs::path root = created;
    const std::string token = std::to_string(getpid()) + "-egar-live";
    if (!restrictDirectory(root)) {
        std::cerr << "egar-live evidence left at " << root.string() << "\n";
        restore("THOTH_EMBED_STRICT", savedStrict);
        restore("THOTH_MAX_REFLECTIONS", savedReflections);
        return 1;
    }
    {
        std::ofstream owner(root / kOwnerName);
        owner << token << "\n";
    }
    const fs::path evidence = root / "evidence";
    std::error_code evidenceError;
    fs::create_directories(evidence, evidenceError);
    if (evidenceError || !restrictDirectory(evidence)) {
        if (evidenceError) {
            std::cerr << "egar-live FAIL: could not create " << evidence
                      << ": " << evidenceError.message() << "\n";
        }
        std::cerr << "egar-live evidence left at " << root.string() << "\n";
        restore("THOTH_EMBED_STRICT", savedStrict);
        restore("THOTH_MAX_REFLECTIONS", savedReflections);
        return 1;
    }
    setenv("THOTH_LOGS_PATH", (root / "logs").string().c_str(), 1);
    fs::create_directories(root / "logs");

    Outcome outcome = Outcome::Fail;
    std::string outcomeDetail;
    std::string controlledGraph = "FAIL";
    std::string liveNodes = "NOT_TESTED";
    std::string liveEdges = "NOT_TESTED";
    int acquisitionGoals = 0;
    nlohmann::json fallbackGoals = 0;
    bool fallbackKnown = true;
    std::string qualifyingPattern;
    std::string strategyId;

    auto finish = [&](Outcome finalOutcome) {
        outcome = finalOutcome;
        const Thoth::LibraryProvenance library = Thoth::loadedLibraryProvenance();
        nlohmann::json report = {
            {"outcome", finalOutcome == Outcome::Pass ? "PASS"
                         : finalOutcome == Outcome::Inconclusive ? "INCONCLUSIVE" : "FAIL"},
            {"detail", outcomeDetail},
            {"controlled_graph", controlledGraph},
            {"live_planner_nodes", liveNodes},
            {"live_planner_edges", liveEdges},
            {"acquisition_goals", acquisitionGoals},
            {"fallback_goals", fallbackGoals},
            {"qualifying_pattern", qualifyingPattern},
            {"strategy_id", strategyId},
            {"chat_url", chatUrl},
            {"embed_url", embedUrl},
            {"embed_strict", "1"},
            {"containment_enabled", false},
            {"library_path", library.path},
            {"library_sha256", library.sha256},
            {"library_build_id", library.build_id},
        };
        std::ofstream out(root / "live_report.json");
        out << report.dump(2) << "\n";
        const char* label = finalOutcome == Outcome::Pass ? "PASS"
            : finalOutcome == Outcome::Inconclusive ? "INCONCLUSIVE" : "FAIL";
        std::cout << "egar-live " << label
                  << " controlled_graph=" << controlledGraph
                  << " live_nodes=" << liveNodes
                  << " live_edges=" << liveEdges
                  << " detail=" << outcomeDetail << "\n";
        restore("THOTH_WORKSPACE_PATH", savedWorkspace);
        restore("THOTH_LOGS_PATH", savedLogs);
        restore("THOTH_EMBED_STRICT", savedStrict);
        restore("THOTH_MAX_REFLECTIONS", savedReflections);
        std::cerr << "egar-live evidence left at " << root.string() << "\n";
        return finalOutcome == Outcome::Pass ? 0 : finalOutcome == Outcome::Inconclusive ? 2 : 1;
    };

    Config config;
    config.llm_model = std::getenv("OLLAMA_MODEL") ? std::getenv("OLLAMA_MODEL") : "chat";
    if (const char* embedModel = std::getenv("THOTH_EMBEDDING_MODEL")) {
        config.embedding_model = embedModel;
    } else if (const char* embedModel = std::getenv("OLLAMA_EMBED_MODEL")) {
        config.embedding_model = embedModel;
    }

    {
        Thoth::LlamaServerClient probe(chatUrl, embedUrl);
        const auto health = probe.health();
        if (!health.reachable) {
            outcomeDetail = "chat health failed: " + health.error;
            return finish(Outcome::Fail);
        }
        Thoth::InferenceEmbedRequest request;
        request.model = config.embedding_model;
        request.inputs = {"thoth-live-embed-probe"};
        const auto embedded = probe.embed(request);
        if (!embedded.ok || embedded.embeddings.size() != 1
            || static_cast<int>(embedded.embeddings.front().size()) != kExpectedDimension) {
            outcomeDetail = "embed probe failed: " + embedded.error;
            return finish(Outcome::Fail);
        }
    }

    auto runControlledGraph = [&]() -> bool {
        const fs::path dir = root / "store_graph";
        const fs::path corpus = dir / "corpus";
        writeNotes(corpus);
        setenv("THOTH_WORKSPACE_PATH", dir.string().c_str(), 1);
        auto memory = std::make_shared<Memory>(config);
        memory->setActiveSessionId("egar-live-graph");
        auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::External, &config);
        if (engine->getMethod() != EmbeddingEngine::Method::External
            || engine->getDimension() != kExpectedDimension) {
            outcomeDetail = "graph subcase engine is not external 768";
            return false;
        }
        auto index = std::make_unique<IndexManager>(engine.get());
        if (!indexSessionNotes(*index, corpus, "egar-live-graph")) {
            outcomeDetail = "graph subcase notes were not stored as 768-dimensional session attachments";
            return false;
        }
        auto rag = std::make_shared<RAGPipeline>(std::move(engine), index.get(), &config, memory.get());
        auto planner = std::make_shared<TwoRetrievalPlanner>();
        auto tools = std::make_shared<ToolRegistry>();
        Thoth::ExecutiveController controller(planner, tools, rag, memory);
        controller.set_session_id("egar-live-graph");
        controller.set_max_reflections(0);
        controller.execute_goal("Retrieve the alpha note and the beta note");
        if (!waitForTerminal(controller)) {
            outcomeDetail = "graph subcase did not complete";
            return false;
        }
        const Plan plan = controller.get_current_plan();
        if (plan.steps.size() != 2 || !stepHasUsefulEvidence(plan.steps[0])
            || !stepHasUsefulEvidence(plan.steps[1])) {
            outcomeDetail = "graph subcase retrieval did not return two evidenced chunks";
            return false;
        }
        std::vector<std::string> hashes = hashesOf(plan);
        if (hashes.size() < 2 || hashes[0] == hashes[1]) {
            outcomeDetail = "graph subcase did not keep two distinct chunk hashes";
            return false;
        }
        bool edge = false;
        if (auto* repo = memory->getSQLiteRepo()) {
            for (const auto& item : repo->getAllEdges()) {
                if (item.from_id == hashes[0] && item.to_id == hashes[1] && item.from_id != item.to_id) {
                    edge = true;
                }
            }
        }
        if (!edge) {
            outcomeDetail = "graph subcase did not write a directed edge";
            return false;
        }
        controlledGraph = "PASS";
        return true;
    };

    if (!runControlledGraph()) {
        return finish(Outcome::Fail);
    }

    const fs::path storeA = root / "store_a";
    const fs::path corpusA = storeA / "corpus";
    writeNotes(corpusA);
    setenv("THOTH_WORKSPACE_PATH", storeA.string().c_str(), 1);
    auto memoryA = std::make_shared<Memory>(config);
    memoryA->setActiveSessionId("egar-live-store-a");
    auto engineA = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::External, &config);
    auto indexA = std::make_unique<IndexManager>(engineA.get());
    if (!indexSessionNotes(*indexA, corpusA, "egar-live-store-a")) {
        outcomeDetail = "store A notes were not stored as 768-dimensional session attachments";
        return finish(Outcome::Fail);
    }
    auto ragA = std::make_shared<RAGPipeline>(std::move(engineA), indexA.get(), &config, memoryA.get());
    auto promptsA = std::make_shared<PromptFactory>(*memoryA, *ragA);
    auto llmA = std::make_unique<LLMInterface>(LLMBackend::Ollama, &config);
    auto innerA = std::make_unique<Thoth::LlamaServerClient>(chatUrl, embedUrl);
    auto recorderHolder = std::make_unique<RecordingLlamaClient>(std::move(innerA));
    RecordingLlamaClient* recorder = recorderHolder.get();
    llmA->setInferenceClientForTests(std::move(recorderHolder));
    auto plannerA = std::make_shared<LLMPlanner>(memoryA, ragA, promptsA, llmA.get());
    auto toolsA = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controllerA(plannerA, toolsA, ragA, memoryA);
    controllerA.set_llm_interface(llmA.get());
    controllerA.set_session_id("egar-live-store-a");
    controllerA.set_max_reflections(0);

    auto consumeCalls = [&](int from, int goalIndex, LiveGoalResult& result) {
        for (int i = from; i < static_cast<int>(recorder->calls.size()); ++i) {
            const GenerateCall& call = recorder->calls[static_cast<std::size_t>(i)];
            if (!call.ok) {
                result.inference_failed = true;
                result.context_failed = contextError(call.error);
                result.inference_error = call.error.empty() ? "generate returned not ok" : call.error;
            }
            if (call.plan_prompt) {
                const fs::path path = evidence / ("store_a_goal_" + std::to_string(goalIndex) + "_planner_prompt.txt");
                if (!writePrompt(path, call.prompt)) {
                    outcomeDetail = "could not preserve planner prompt for goal "
                        + std::to_string(goalIndex);
                    return false;
                }
                result.plan_prompt_path = path.string();
            }
        }
        return true;
    };

    bool liveEdgeCheck = false;
    std::vector<DirectedEdge> requiredEdges;
    for (int goal = 1; goal <= 3; ++goal) {
        const int callFrom = static_cast<int>(recorder->calls.size());
        controllerA.execute_goal(goalText(goal));
        LiveGoalResult result;
        result.completed = waitForTerminal(controllerA);
        if (!consumeCalls(callFrom, goal, result)) {
            return finish(Outcome::Fail);
        }
        if (result.inference_failed) {
            outcomeDetail = result.context_failed
                ? "context overflow: " + result.inference_error
                : "inference request failed: " + result.inference_error;
            return finish(Outcome::Fail);
        }
        if (!result.completed) {
            outcomeDetail = "acquisition goal " + std::to_string(goal) + " did not complete";
            return finish(Outcome::Inconclusive);
        }
        const Plan plan = controllerA.get_current_plan();
        const FallbackMark mark = fallbackForPlan(plan.plan_id);
        if (mark == FallbackMark::Unknown) {
            fallbackKnown = false;
        } else if (fallbackKnown && mark == FallbackMark::Yes) {
            fallbackGoals = fallbackGoals.get<int>() + 1;
        }
        std::vector<std::string> goalHashes = hashesOf(plan);
        if (!goalHashes.empty()) {
            liveNodes = "PASS";
        }
        const auto required = requiredCrossSetEdges(plan);
        if (!required.empty()) {
            liveEdgeCheck = true;
            requiredEdges.insert(requiredEdges.end(), required.begin(), required.end());
        }
        ++acquisitionGoals;
    }
    if (!fallbackKnown) {
        fallbackGoals = "unknown";
    }

    if (liveEdgeCheck) {
        bool allPresent = true;
        auto* repo = memoryA->getSQLiteRepo();
        const auto stored = repo ? repo->getAllEdges() : std::vector<Memory::Edge>{};
        for (const auto& required : requiredEdges) {
            bool found = false;
            for (const auto& item : stored) {
                if (item.from_id == required.from && item.to_id == required.to) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                allPresent = false;
            }
        }
        liveEdges = allPresent ? "PASS" : "FAIL";
        if (!allPresent) {
            outcomeDetail = "a live plan had consecutive retrieval sets but a directed edge is missing";
            return finish(Outcome::Fail);
        }
    }

    std::vector<PatternStats> patterns;
    bool embeddingsAre768 = true;
    for (const auto& past : memoryA->getAllPastPlans()) {
        if (static_cast<int>(past.goal_embedding.size()) != kExpectedDimension) {
            embeddingsAre768 = false;
        }
    }
    for (const auto& trajectory : memoryA->getAllTrajectories()) {
        if (static_cast<int>(trajectory.embedding.size()) != kExpectedDimension) {
            embeddingsAre768 = false;
        }
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(trajectory.trajectory_json);
        } catch (...) {
            continue;
        }
        const auto steps = patternOf(body);
        if (steps.size() < 2) {
            continue;
        }
        const std::string key = patternKey(steps);
        auto found = std::find_if(patterns.begin(), patterns.end(),
                                   [&](const PatternStats& item) { return item.key == key; });
        if (found == patterns.end()) {
            patterns.push_back(PatternStats{key, 0, 0.0f, 0});
            found = std::prev(patterns.end());
        }
        found->count += 1;
        found->success_sum += trajectory.success_score;
        if (trajectory.success_score >= 0.8f && trajectoryEvidenced(body)) {
            found->evidenced += 1;
        }
    }
    if (!embeddingsAre768) {
        outcomeDetail = "stored experience embeddings are not 768-dimensional";
        return finish(Outcome::Fail);
    }

    const PatternStats* qualified = nullptr;
    for (const auto& pattern : patterns) {
        const float mean = pattern.count == 0 ? 0.0f : pattern.success_sum / static_cast<float>(pattern.count);
        if (pattern.evidenced >= 3 && mean >= 0.8f) {
            qualified = &pattern;
            qualifyingPattern = pattern.key;
        }
    }
    if (!qualified) {
        outcomeDetail = "real model did not establish three evidenced trajectories of one pattern";
        return finish(Outcome::Inconclusive);
    }

    const auto strategies = memoryA->getAllStrategies();
    const Memory::CognateStrategyRecord* strategy = nullptr;
    for (const auto& item : strategies) {
        if (item.description.find(qualifyingPattern) != std::string::npos && item.success_rate >= 0.8f) {
            strategy = &item;
            strategyId = item.strategy_id;
        }
    }
    if (!strategy) {
        outcomeDetail = "acquisition conditions existed but no strategy was promoted";
        return finish(Outcome::Fail);
    }

    const std::string goalFour = goalText(4);
    nlohmann::json envelope;
    envelope["schema_version"] = 2;
    envelope["type"] = "goal";
    envelope["content"] = goalFour;
    const auto goalVector = ragA->engine->embed(goalFour);
    const auto pastVector = ragA->engine->embed(envelope.dump());
    if (static_cast<int>(goalVector.size()) != kExpectedDimension
        || static_cast<int>(pastVector.size()) != kExpectedDimension) {
        outcomeDetail = "goal embedding for injection was not 768-dimensional";
        return finish(Outcome::Fail);
    }
    const auto eligiblePlans =
        memoryA->retrieveSimilarPlans(pastVector, Thoth::PlanReuse::kDefaultRetrieveLimit);
    const auto eligibleTrajectories =
        memoryA->retrieveSimilarTrajectories(goalVector, Thoth::PlannerInjection::kMaxTrajectoryInject);
    if (eligiblePlans.size() != 1 || eligibleTrajectories.size() != 1) {
        outcomeDetail = "injection goal did not select one past plan and one trajectory";
        return finish(Outcome::Fail);
    }

    const int callFrom = static_cast<int>(recorder->calls.size());
    controllerA.execute_goal(goalFour);
    LiveGoalResult injection;
    injection.completed = waitForTerminal(controllerA);
    if (!consumeCalls(callFrom, 4, injection)) {
        return finish(Outcome::Fail);
    }
    if (injection.inference_failed) {
        outcomeDetail = injection.context_failed
            ? "injection context overflow: " + injection.inference_error
            : "injection inference request failed: " + injection.inference_error;
        return finish(Outcome::Fail);
    }
    if (injection.plan_prompt_path.empty()) {
        outcomeDetail = "injection goal did not capture a planner prompt";
        return finish(Outcome::Fail);
    }
    std::ifstream promptFile(injection.plan_prompt_path);
    std::stringstream buffer;
    buffer << promptFile.rdbuf();
    const std::string prompt = buffer.str();
    const std::string outline = Thoth::sanitizePlanOutline(eligiblePlans.front().outline);
    const bool injected = prompt.find(strategyId) != std::string::npos
        && prompt.find(strategy->step_pattern_json) != std::string::npos
        && prompt.find(eligibleTrajectories.front().goal) != std::string::npos
        && prompt.find(eligiblePlans.front().plan_id) != std::string::npos
        && prompt.find(outline) != std::string::npos
        && prompt.find("...(truncated)...") == std::string::npos;
    if (!injected) {
        outcomeDetail = "captured planner prompt is missing strategy, trajectory, or past-plan text";
        return finish(Outcome::Fail);
    }

    const auto frozenTrajectories = memoryA->getAllTrajectories().size();
    const auto frozenPlans = memoryA->getAllPastPlans().size();
    const auto frozenStrategies = memoryA->getAllStrategies().size();
    std::vector<std::pair<std::string, std::string>> frozenEdges;
    if (auto* repo = memoryA->getSQLiteRepo()) {
        for (const auto& edge : repo->getAllEdges()) {
            frozenEdges.push_back({edge.from_id, edge.to_id});
        }
    }

    const fs::path storeB = root / "store_b";
    writeNotes(storeB / "corpus");
    setenv("THOTH_WORKSPACE_PATH", storeB.string().c_str(), 1);
    auto memoryB = std::make_shared<Memory>(config);
    if (!memoryB->getAllTrajectories().empty() || !memoryB->getAllPastPlans().empty()
        || !memoryB->getAllStrategies().empty()) {
        outcomeDetail = "store B was not empty";
        return finish(Outcome::Fail);
    }
    memoryB->setActiveSessionId("egar-live-store-b");
    auto engineB = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::External, &config);
    auto indexB = std::make_unique<IndexManager>(engineB.get());
    if (!indexSessionNotes(*indexB, storeB / "corpus", "egar-live-store-b")) {
        outcomeDetail = "store B notes were not stored as 768-dimensional session attachments";
        return finish(Outcome::Fail);
    }
    auto ragB = std::make_shared<RAGPipeline>(std::move(engineB), indexB.get(), &config, memoryB.get());
    auto promptsB = std::make_shared<PromptFactory>(*memoryB, *ragB);
    auto llmB = std::make_unique<LLMInterface>(LLMBackend::Ollama, &config);
    auto recorderBHolder = std::make_unique<RecordingLlamaClient>(
        std::make_unique<Thoth::LlamaServerClient>(chatUrl, embedUrl));
    RecordingLlamaClient* recorderB = recorderBHolder.get();
    llmB->setInferenceClientForTests(std::move(recorderBHolder));
    auto plannerB = std::make_shared<LLMPlanner>(memoryB, ragB, promptsB, llmB.get());
    auto toolsB = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controllerB(plannerB, toolsB, ragB, memoryB);
    controllerB.set_llm_interface(llmB.get());
    controllerB.set_session_id("egar-live-store-b");
    controllerB.set_max_reflections(0);
    controllerB.execute_goal(goalFour);
    if (!waitForTerminal(controllerB)) {
        outcomeDetail = "store B goal did not complete";
        return finish(Outcome::Fail);
    }
    bool storeBPlanPrompt = false;
    for (const auto& call : recorderB->calls) {
        if (!call.ok) {
            outcomeDetail = "store B inference request failed: " + call.error;
            return finish(Outcome::Fail);
        }
        if (!call.plan_prompt) {
            continue;
        }
        storeBPlanPrompt = true;
        if (!writePrompt(evidence / "store_b_planner_prompt.txt", call.prompt)) {
            outcomeDetail = "could not preserve store B planner prompt";
            return finish(Outcome::Fail);
        }
        if (call.prompt.find(strategyId) != std::string::npos
            || call.prompt.find(kMarkerStrategy) != std::string::npos
            || call.prompt.find(kMarkerTrajectory) != std::string::npos
            || call.prompt.find(kMarkerPastPlan) != std::string::npos
            || call.prompt.find("RELEVANT TRAJECTORY") != std::string::npos) {
            outcomeDetail = "store B planner prompt contained store A experience";
            return finish(Outcome::Fail);
        }
    }
    if (!storeBPlanPrompt) {
        outcomeDetail = "store B did not capture a planner prompt";
        return finish(Outcome::Fail);
    }
    if (memoryA->getAllTrajectories().size() != frozenTrajectories
        || memoryA->getAllPastPlans().size() != frozenPlans
        || memoryA->getAllStrategies().size() != frozenStrategies) {
        outcomeDetail = "store B changed store A experience rows";
        return finish(Outcome::Fail);
    }
    if (auto* repo = memoryA->getSQLiteRepo()) {
        std::vector<std::pair<std::string, std::string>> after;
        for (const auto& edge : repo->getAllEdges()) {
            after.push_back({edge.from_id, edge.to_id});
        }
        if (after != frozenEdges) {
            outcomeDetail = "store B changed store A graph edges";
            return finish(Outcome::Fail);
        }
    }

    outcomeDetail = "strategy, trajectory, and past plan were present in the captured planner prompt";
    return finish(Outcome::Pass);
}
