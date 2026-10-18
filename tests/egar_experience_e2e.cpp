/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — deterministic EGAR experience-path fixture.
 * Five controller goals, TF-IDF, and a prompt-conditional fake chat client.
 * Not part of CTest. Does not start inference servers.
 * Store A and Store B are temporary databases, not EGAR experiment arms.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "config.h"
#include "embedding_engine.h"
#include "executive_controller.h"
#include "file_handler.h"
#include "grag_diagnostics.h"
#include "grag_scorer.h"
#include "index_manager.h"
#include "inference_client.h"
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

#include <chrono>
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

const char* kMarkerStrategy = "[RELEVANT STRATEGY";
const char* kMarkerTrajectory = "[PAST EXPERIENCE";
const char* kMarkerPastPlan = "[RELEVANT PAST APPROACHES";
const char* kTruncation = "...(truncated)...";
const char* kPattern = "RETRIEVAL->RETRIEVAL->LLM";
const char* kAcquireLlm = "acquire-llm";
const char* kConditionedLlm = "experience-conditioned-llm";
const char* kOwnerName = "OWNER";

std::string failReason;

void fail(const std::string& why) {
    if (failReason.empty()) {
        failReason = why;
    }
    std::cerr << "egar-experience FAIL: " << why << "\n";
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

std::string planJson(const char* llmStepId) {
    std::ostringstream out;
    out << "{\"plan\":["
        << "{\"step_id\":\"acquire-alpha\",\"step_type\":\"RETRIEVAL\","
        << "\"description\":\"Retrieve the alpha note\","
        << "\"payload\":{\"query\":\"uniquealphatoken\",\"top_k\":5}},"
        << "{\"step_id\":\"acquire-beta\",\"step_type\":\"RETRIEVAL\","
        << "\"description\":\"Retrieve the beta note\","
        << "\"depends_on\":[\"acquire-alpha\"],"
        << "\"payload\":{\"query\":\"uniquebetatoken\",\"top_k\":5}},"
        << "{\"step_id\":\"" << llmStepId << "\",\"step_type\":\"LLM\","
        << "\"description\":\"Summarize the two notes\","
        << "\"depends_on\":[\"acquire-beta\"],\"payload\":{}}]}";
    return out.str();
}

bool hasAllThreeBlocks(const std::string& prompt) {
    return prompt.find(kMarkerStrategy) != std::string::npos
        && prompt.find(kMarkerTrajectory) != std::string::npos
        && prompt.find("RELEVANT TRAJECTORY") != std::string::npos
        && prompt.find(kMarkerPastPlan) != std::string::npos;
}

class ExperienceFakeClient : public Thoth::InferenceClient {
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
        const bool planPrompt = request.prompt.find("\nSchema:\n") != std::string::npos;
        if (planPrompt) {
            const char* stepId = hasAllThreeBlocks(request.prompt) ? kConditionedLlm : kAcquireLlm;
            result.text = planJson(stepId);
            result.token_usage.completion_tokens = 64;
        } else {
            result.text = "experience synthesis ok";
            result.token_usage.completion_tokens = 3;
        }
        result.token_usage.total_tokens =
            result.token_usage.prompt_tokens + result.token_usage.completion_tokens;
        return result;
    }

    Thoth::InferenceGenerateResult generateChat(const Thoth::InferenceChatRequest&) override {
        Thoth::InferenceGenerateResult result;
        result.ok = false;
        result.error = "ExperienceFakeClient does not serve chat";
        return result;
    }

    Thoth::InferenceEmbedResult embed(const Thoth::InferenceEmbedRequest&) override {
        Thoth::InferenceEmbedResult result;
        result.ok = false;
        result.error = "ExperienceFakeClient does not embed";
        return result;
    }

    Thoth::InferenceHealthResult health() override {
        Thoth::InferenceHealthResult result;
        result.reachable = true;
        return result;
    }

    std::string backendName() const override { return "egar-experience-fake"; }

    std::string lastPlanPrompt() const {
        for (auto it = prompts.rbegin(); it != prompts.rend(); ++it) {
            if (it->find("\nSchema:\n") != std::string::npos) {
                return *it;
            }
        }
        return {};
    }
};

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

bool ownedRunDirectory(const fs::path& root, const std::string& token) {
    const fs::path normal = root.lexically_normal();
    const std::string text = normal.string();
    if (text.find("..") != std::string::npos) {
        return false;
    }
    if (text.rfind("/tmp/thoth-egar-experience-", 0) != 0) {
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
        std::cerr << "egar-experience: refused cleanup of " << root << "\n";
        return;
    }
    std::error_code error;
    fs::remove_all(root, error);
    if (error) {
        std::cerr << "egar-experience: cleanup failed: " << error.message() << "\n";
    }
}

struct EdgeSnap {
    std::string from_id;
    std::string to_id;
    int success_count = 0;
    int failure_count = 0;
};

std::vector<EdgeSnap> edgesOf(Memory& memory) {
    std::vector<EdgeSnap> edges;
    auto* repo = memory.getSQLiteRepo();
    if (!repo) {
        return edges;
    }
    for (const auto& edge : repo->getAllEdges()) {
        edges.push_back({edge.from_id, edge.to_id, edge.success_count, edge.failure_count});
    }
    return edges;
}

struct GraphProbe {
    float graph_avg_weight = 0.0f;
    int graph_activations = 0;
    float top_final_score = 0.0f;
    int breakdowns = 0;
};

GraphProbe probeGraph(RAGPipeline& rag) {
    GragDiagnostics diagnostics;
    rag.retrieveRelevant("uniquealphatoken", {}, 5, "egar-experience-probe", "", "",
                         {}, {}, {}, &diagnostics);
    GraphProbe probe;
    probe.graph_avg_weight = diagnostics.graph_avg_weight;
    probe.graph_activations = diagnostics.graph_activations;
    probe.breakdowns = static_cast<int>(diagnostics.breakdowns.size());
    if (!diagnostics.breakdowns.empty()) {
        probe.top_final_score = diagnostics.breakdowns.front().final_score;
    }
    return probe;
}

bool stepSucceeded(const Plan& plan, const std::string& stepId) {
    for (const auto& step : plan.steps) {
        if (step.step_id == stepId && step.status == StepStatus::SUCCESS) {
            return true;
        }
    }
    return false;
}

bool planContains(const Plan& plan, const std::string& stepId) {
    for (const auto& step : plan.steps) {
        if (step.step_id == stepId) {
            return true;
        }
    }
    return false;
}

std::string goalText(int index) {
    return "successful pattern detected retrieval llm widget case " + std::to_string(index);
}

struct StoreCounts {
    int trajectories = 0;
    int past_plans = 0;
    int strategies = 0;
    std::vector<EdgeSnap> edges;
};

StoreCounts countsOf(Memory& memory) {
    StoreCounts counts;
    counts.trajectories = static_cast<int>(memory.getAllTrajectories().size());
    counts.past_plans = static_cast<int>(memory.getAllPastPlans().size());
    counts.strategies = static_cast<int>(memory.getAllStrategies().size());
    counts.edges = edgesOf(memory);
    return counts;
}

bool sameEdges(const std::vector<EdgeSnap>& left, const std::vector<EdgeSnap>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (const auto& edge : left) {
        bool found = false;
        for (const auto& other : right) {
            if (edge.from_id == other.from_id && edge.to_id == other.to_id
                && edge.success_count == other.success_count
                && edge.failure_count == other.failure_count) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool assemblyKeptExperience(const fs::path& workspace) {
    std::ifstream in(workspace / "app_log.jsonl");
    bool saw = false;
    bool dropped = true;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("PLANNER_CONTEXT_ASSEMBLY") == std::string::npos) {
            continue;
        }
        try {
            const auto entry = nlohmann::json::parse(line);
            if (entry.value("event_name", "") != "PLANNER_CONTEXT_ASSEMBLY") {
                continue;
            }
            const auto metadata = entry.value("metadata", nlohmann::json::object());
            if (!metadata.value("strategy_injection", false) || !metadata.value("trajectory_injection", false)
                || !metadata.value("plan_reuse_in_goal", false)) {
                continue;
            }
            saw = true;
            dropped = metadata.value("experience_dropped", true);
        } catch (...) {
        }
    }
    if (!saw) {
        fail("goal 4 did not log an assembly containing strategy, trajectory, and past-plan injection");
        return false;
    }
    if (dropped) {
        fail("goal 4 logged experience_dropped");
        return false;
    }
    return true;
}

}  // namespace

int main() {
    const EnvSlot savedWorkspace = capture("THOTH_WORKSPACE_PATH");
    const EnvSlot savedLogs = capture("THOTH_LOGS_PATH");
    const EnvSlot savedMock = capture("THOTH_MOCK_LLM");
    const EnvSlot savedEmbed = capture("THOTH_EMBED_BASE_URL");
    const EnvSlot savedStrict = capture("THOTH_EMBED_STRICT");
    const EnvSlot savedBackend = capture("THOTH_INFERENCE_BACKEND");
    const EnvSlot savedReflections = capture("THOTH_MAX_REFLECTIONS");
    unsetenv("THOTH_MOCK_LLM");
    unsetenv("THOTH_EMBED_BASE_URL");
    unsetenv("THOTH_EMBED_STRICT");
    unsetenv("THOTH_INFERENCE_BACKEND");
    unsetenv("THOTH_MAX_REFLECTIONS");

    std::string templatePath = "/tmp/thoth-egar-experience-XXXXXX";
    std::vector<char> scratch(templatePath.begin(), templatePath.end());
    scratch.push_back('\0');
    if (!mkdtemp(scratch.data())) {
        std::cerr << "egar-experience FAIL: could not create a run directory\n";
        return 1;
    }
    const fs::path root(scratch.data());
    const std::string token = std::to_string(static_cast<long long>(getpid())) + "-egar-experience";
    {
        std::ofstream owner(root / kOwnerName);
        owner << token << "\n";
    }
    if (!ownedRunDirectory(root, token)) {
        std::cerr << "egar-experience FAIL: ownership check rejected the new directory\n";
        return 1;
    }

    const fs::path storeADir = root / "store_a";
    const fs::path storeBDir = root / "store_b";
    const fs::path corpusA = storeADir / "corpus";
    const fs::path corpusB = storeBDir / "corpus";
    fs::create_directories(corpusA);
    fs::create_directories(corpusB);
    const char* alphaNote = "uniquealphatoken successful pattern detected retrieval llm alpha widget note\n";
    const char* betaNote = "uniquebetatoken successful pattern detected retrieval llm beta widget note\n";
    {
        std::ofstream alpha(corpusA / "alpha.md");
        alpha << alphaNote;
        std::ofstream beta(corpusA / "beta.md");
        beta << betaNote;
        std::ofstream alphaB(corpusB / "alpha.md");
        alphaB << alphaNote;
        std::ofstream betaB(corpusB / "beta.md");
        betaB << betaNote;
    }

    auto finish = [&](int code) {
        restore("THOTH_WORKSPACE_PATH", savedWorkspace);
        restore("THOTH_LOGS_PATH", savedLogs);
        restore("THOTH_MOCK_LLM", savedMock);
        restore("THOTH_EMBED_BASE_URL", savedEmbed);
        restore("THOTH_EMBED_STRICT", savedStrict);
        restore("THOTH_INFERENCE_BACKEND", savedBackend);
        restore("THOTH_MAX_REFLECTIONS", savedReflections);
        if (code == 0) {
            removeOwnedRunDirectory(root, token);
        } else {
            std::cerr << "egar-experience evidence left at " << root << "\n";
        }
        return code;
    };

    setenv("THOTH_LOGS_PATH", (root / "logs").string().c_str(), 1);
    fs::create_directories(root / "logs");
    setenv("THOTH_WORKSPACE_PATH", storeADir.string().c_str(), 1);

    Config configA;
    configA.database_path = (storeADir / "memory.db").string();
    configA.llm_model = "deterministic-fake";
    auto memoryA = std::make_shared<Memory>(configA);
    FileHandler pathCheck;
    const std::string openedDb = (fs::path(pathCheck.getAgentWorkspacePath()) / "memory.db").string();
    if (openedDb.find("/tmp/thoth-egar-experience-") != 0 || openedDb.find("agent_workspace") != std::string::npos) {
        fail("store A database is not inside the owned temp workspace: " + openedDb);
        return finish(1);
    }

    auto initial = countsOf(*memoryA);
    if (initial.trajectories != 0 || initial.past_plans != 0 || initial.strategies != 0 || !initial.edges.empty()) {
        fail("store A was not empty");
        return finish(1);
    }

    const std::string predictedStrategy =
        std::string("Successful pattern detected: ") + kPattern + " [\"RETRIEVAL\",\"RETRIEVAL\",\"LLM\"]";
    auto seedPrimaryVocabulary = [&](EmbeddingEngine& engine) {
        engine.updateVocabulary(alphaNote);
        engine.updateVocabulary(betaNote);
        engine.updateVocabulary(predictedStrategy);
        for (int i = 1; i <= 4; ++i) {
            engine.updateVocabulary(goalText(i));
        }
    };

    auto engineA = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &configA);
    EmbeddingEngine* enginePtr = engineA.get();
    auto indexA = std::make_unique<IndexManager>(enginePtr);
    memoryA->setActiveSessionId("egar-experience-store-a");
    seedPrimaryVocabulary(*enginePtr);
    const std::string alphaPathA =
        fs::absolute(corpusA / "alpha.md").lexically_normal().string();
    const std::string betaPathA =
        fs::absolute(corpusA / "beta.md").lexically_normal().string();
    indexA->registerAttachmentOwner(alphaPathA, "egar-experience-store-a");
    indexA->registerAttachmentOwner(betaPathA, "egar-experience-store-a");
    indexA->indexFile(alphaPathA);
    indexA->indexFile(betaPathA);

    auto ragA = std::make_shared<RAGPipeline>(std::move(engineA), indexA.get(), &configA, memoryA.get());
    auto promptsA = std::make_shared<PromptFactory>(*memoryA, *ragA);
    LLMInterface llmA(LLMBackend::Ollama, &configA);
    auto fakeAHolder = std::make_unique<ExperienceFakeClient>();
    ExperienceFakeClient* fakeA = fakeAHolder.get();
    llmA.setInferenceClientForTests(std::move(fakeAHolder));
    auto plannerA = std::make_shared<LLMPlanner>(memoryA, ragA, promptsA, &llmA);
    auto toolsA = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controllerA(plannerA, toolsA, ragA, memoryA);
    controllerA.set_llm_interface(&llmA);
    controllerA.set_session_id("egar-experience-store-a");
    controllerA.set_max_reflections(0);

    const GraphProbe beforeEdges = probeGraph(*ragA);

    for (int goal = 1; goal <= 3; ++goal) {
        const int promptsBefore = static_cast<int>(fakeA->prompts.size());
        controllerA.execute_goal(goalText(goal));
        if (!waitForTerminal(controllerA)) {
            fail("acquisition goal " + std::to_string(goal) + " did not complete");
            return finish(1);
        }
        const std::string planPrompt = [&]() {
            for (int i = promptsBefore; i < static_cast<int>(fakeA->prompts.size()); ++i) {
                if (fakeA->prompts[static_cast<std::size_t>(i)].find("\nSchema:\n") != std::string::npos) {
                    return fakeA->prompts[static_cast<std::size_t>(i)];
                }
            }
            return std::string();
        }();
        if (hasAllThreeBlocks(planPrompt)) {
            fail("acquisition goal " + std::to_string(goal) + " saw all three experience blocks");
            return finish(1);
        }
        const Plan plan = controllerA.get_current_plan();
        if (!stepSucceeded(plan, kAcquireLlm) || planContains(plan, kConditionedLlm)) {
            fail("acquisition goal " + std::to_string(goal) + " did not execute the baseline step");
            return finish(1);
        }
        const auto counts = countsOf(*memoryA);
        if (counts.trajectories != goal || counts.past_plans != goal) {
            fail("acquisition goal " + std::to_string(goal) + " did not persist one trajectory and past plan");
            return finish(1);
        }
        for (const auto& traj : memoryA->getAllTrajectories()) {
            if (traj.success_score < 0.8f) {
                fail("acquisition trajectory scored below 0.8");
                return finish(1);
            }
        }
        for (const auto& past : memoryA->getAllPastPlans()) {
            if (past.success_score < Thoth::PlanReuse::kMinSuccessScore) {
                fail("acquisition past plan is below the reuse floor");
                return finish(1);
            }
        }
        const int strategies = counts.strategies;
        if (goal < 3 && strategies != 0) {
            fail("a strategy was promoted before the third trajectory");
            return finish(1);
        }
        if (goal == 3 && strategies != 1) {
            fail("the third completed goal did not promote exactly one strategy");
            return finish(1);
        }
    }

    const auto strategies = memoryA->getAllStrategies();
    if (strategies.size() != 1 || strategies.front().description.find(kPattern) == std::string::npos
        || strategies.front().success_rate < 0.8f) {
        fail("promoted strategy does not carry the retrieval pattern at rate >= 0.8");
        return finish(1);
    }
    const std::string strategyId = strategies.front().strategy_id;
    const auto edgesAfterThree = edgesOf(*memoryA);
    if (edgesAfterThree.empty()) {
        fail("three completed two-retrieval goals did not write a graph edge");
        return finish(1);
    }
    const GraphProbe afterEdges = probeGraph(*ragA);

    const std::string goalFour = goalText(4);
    const auto goalVector = ragA->engine->embed(goalFour);
    const std::string strategyEmbedText =
        strategies.front().description + " " + strategies.front().step_pattern_json;
    const float strategySimilarity = GragScorer::cosine_similarity(goalVector, ragA->engine->embed(strategyEmbedText));
    if (strategySimilarity < Thoth::PlannerInjection::kMinStrategySimilarity) {
        fail("TF-IDF strategy similarity " + std::to_string(strategySimilarity) + " is below "
             + std::to_string(Thoth::PlannerInjection::kMinStrategySimilarity));
        return finish(1);
    }
    nlohmann::json structuredGoal;
    structuredGoal["schema_version"] = 2;
    structuredGoal["type"] = "goal";
    structuredGoal["content"] = goalFour;
    const auto pastPlanVector = ragA->engine->embed(structuredGoal.dump());
    const auto eligiblePlans =
        memoryA->retrieveSimilarPlans(pastPlanVector, Thoth::PlanReuse::kDefaultRetrieveLimit);
    const auto eligibleTrajectories =
        memoryA->retrieveSimilarTrajectories(goalVector, Thoth::PlannerInjection::kMaxTrajectoryInject);
    if (eligiblePlans.size() != 1 || eligibleTrajectories.size() != 1) {
        fail("goal 4 retrieval did not return one past plan and one trajectory");
        return finish(1);
    }

    const int promptsBeforeFour = static_cast<int>(fakeA->prompts.size());
    controllerA.execute_goal(goalFour);
    if (!waitForTerminal(controllerA)) {
        fail("goal 4 did not complete");
        return finish(1);
    }
    std::string goalFourPrompt;
    for (int i = promptsBeforeFour; i < static_cast<int>(fakeA->prompts.size()); ++i) {
        if (fakeA->prompts[static_cast<std::size_t>(i)].find("\nSchema:\n") != std::string::npos) {
            goalFourPrompt = fakeA->prompts[static_cast<std::size_t>(i)];
        }
    }
    if (!hasAllThreeBlocks(goalFourPrompt) || goalFourPrompt.find(kTruncation) != std::string::npos) {
        fail("goal 4 prompt is missing an experience block or is truncated");
        return finish(1);
    }
    if (goalFourPrompt.find(strategyId) == std::string::npos
        || goalFourPrompt.find(strategies.front().step_pattern_json) == std::string::npos
        || goalFourPrompt.find(eligibleTrajectories.front().goal) == std::string::npos
        || goalFourPrompt.find(eligiblePlans.front().plan_id) == std::string::npos
        || goalFourPrompt.find("RETRIEVAL(acquire-alpha)") == std::string::npos
        || goalFourPrompt.find("LLM(acquire-llm)") == std::string::npos) {
        fail("goal 4 prompt does not contain the selected strategy, trajectory, and past-plan outline");
        return finish(1);
    }
    if (!assemblyKeptExperience(storeADir)) {
        return finish(1);
    }
    const Plan conditioned = controllerA.get_current_plan();
    if (!stepSucceeded(conditioned, kConditionedLlm) || planContains(conditioned, kAcquireLlm)) {
        fail("goal 4 did not execute the prompt-conditioned step");
        return finish(1);
    }
    const auto afterFour = countsOf(*memoryA);
    if (afterFour.trajectories != 4 || afterFour.past_plans != 4 || afterFour.strategies != 1) {
        fail("goal 4 did not persist a fourth trajectory while keeping one strategy");
        return finish(1);
    }
    bool conditionedStored = false;
    for (const auto& traj : memoryA->getAllTrajectories()) {
        if (traj.trajectory_json.find(kConditionedLlm) != std::string::npos) {
            conditionedStored = true;
        }
    }
    if (!conditionedStored) {
        fail("the fourth trajectory does not contain the conditioned step");
        return finish(1);
    }
    bool edgeUpdated = false;
    for (const auto& before : edgesAfterThree) {
        for (const auto& after : afterFour.edges) {
            if (before.from_id == after.from_id && before.to_id == after.to_id
                && after.success_count == before.success_count + 1) {
                edgeUpdated = true;
            }
        }
    }
    if (!edgeUpdated) {
        fail("goal 4 did not increment an existing graph edge success count");
        return finish(1);
    }

    const StoreCounts storeAFrozen = countsOf(*memoryA);

    setenv("THOTH_WORKSPACE_PATH", storeBDir.string().c_str(), 1);
    Config configB;
    configB.database_path = (storeBDir / "memory.db").string();
    configB.llm_model = "deterministic-fake";
    auto memoryB = std::make_shared<Memory>(configB);
    FileHandler pathCheckB;
    const std::string openedB = (fs::path(pathCheckB.getAgentWorkspacePath()) / "memory.db").string();
    if (openedB.find("/tmp/thoth-egar-experience-") != 0 || openedB == openedDb
        || openedB.find("agent_workspace") != std::string::npos) {
        fail("store B is not a distinct owned database");
        return finish(1);
    }
    const auto initialB = countsOf(*memoryB);
    if (initialB.trajectories != 0 || initialB.past_plans != 0 || initialB.strategies != 0 || !initialB.edges.empty()) {
        fail("store B was not empty");
        return finish(1);
    }

    auto engineB = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &configB);
    auto indexB = std::make_unique<IndexManager>(engineB.get());
    memoryB->setActiveSessionId("egar-experience-store-b");
    seedPrimaryVocabulary(*engineB);
    const std::string alphaPathB =
        fs::absolute(corpusB / "alpha.md").lexically_normal().string();
    const std::string betaPathB =
        fs::absolute(corpusB / "beta.md").lexically_normal().string();
    indexB->registerAttachmentOwner(alphaPathB, "egar-experience-store-b");
    indexB->registerAttachmentOwner(betaPathB, "egar-experience-store-b");
    indexB->indexFile(alphaPathB);
    indexB->indexFile(betaPathB);
    auto ragB = std::make_shared<RAGPipeline>(std::move(engineB), indexB.get(), &configB, memoryB.get());
    auto promptsB = std::make_shared<PromptFactory>(*memoryB, *ragB);
    LLMInterface llmB(LLMBackend::Ollama, &configB);
    auto fakeBHolder = std::make_unique<ExperienceFakeClient>();
    ExperienceFakeClient* fakeB = fakeBHolder.get();
    llmB.setInferenceClientForTests(std::move(fakeBHolder));
    auto plannerB = std::make_shared<LLMPlanner>(memoryB, ragB, promptsB, &llmB);
    auto toolsB = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controllerB(plannerB, toolsB, ragB, memoryB);
    controllerB.set_llm_interface(&llmB);
    controllerB.set_session_id("egar-experience-store-b");
    controllerB.set_max_reflections(0);
    controllerB.execute_goal(goalFour);
    if (!waitForTerminal(controllerB)) {
        fail("store B goal did not complete");
        return finish(1);
    }
    const std::string storeBPrompt = fakeB->lastPlanPrompt();
    if (hasAllThreeBlocks(storeBPrompt) || storeBPrompt.find(strategyId) != std::string::npos
        || storeBPrompt.find(kMarkerStrategy) != std::string::npos
        || storeBPrompt.find(kMarkerPastPlan) != std::string::npos
        || storeBPrompt.find("RELEVANT TRAJECTORY") != std::string::npos) {
        fail("store B prompt contained experience from store A");
        return finish(1);
    }
    const Plan baseline = controllerB.get_current_plan();
    if (!stepSucceeded(baseline, kAcquireLlm) || planContains(baseline, kConditionedLlm)) {
        fail("store B did not execute the baseline step");
        return finish(1);
    }

    const StoreCounts storeAAfterB = countsOf(*memoryA);
    if (storeAAfterB.trajectories != storeAFrozen.trajectories
        || storeAAfterB.past_plans != storeAFrozen.past_plans
        || storeAAfterB.strategies != storeAFrozen.strategies
        || !sameEdges(storeAAfterB.edges, storeAFrozen.edges)) {
        fail("store B changed store A");
        return finish(1);
    }

    nlohmann::json report = {
        {"store_a_db", openedDb},
        {"store_b_db", openedB},
        {"strategy_id", strategyId},
        {"strategy_similarity", strategySimilarity},
        {"graph_probe_before",
         {{"graph_avg_weight", beforeEdges.graph_avg_weight},
          {"graph_activations", beforeEdges.graph_activations},
          {"top_final_score", beforeEdges.top_final_score},
          {"breakdowns", beforeEdges.breakdowns}}},
        {"graph_probe_after_acquisition",
         {{"graph_avg_weight", afterEdges.graph_avg_weight},
          {"graph_activations", afterEdges.graph_activations},
          {"top_final_score", afterEdges.top_final_score},
          {"breakdowns", afterEdges.breakdowns},
          {"required_difference", false}}},
        {"conditioned_step", kConditionedLlm},
        {"baseline_step", kAcquireLlm},
    };
    {
        std::ofstream out(root / "experience_report.json");
        out << report.dump(2) << "\n";
    }

    std::cout << "egar-experience PASS store_a=\"" << openedDb << "\" strategy=" << strategyId
              << " similarity=" << strategySimilarity
              << " graph_before=" << beforeEdges.graph_avg_weight
              << " graph_after=" << afterEdges.graph_avg_weight << "\n";
    return finish(0);
}
