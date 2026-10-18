/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — EGAR-LX Package 1 containment tests
 *
 * Each CTest invocation runs one case in a fresh process.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "../external/basic_agent/include/fixed_evidence_containment.h"
#include "../external/basic_agent/include/tools.h"
#include "../external/basic_agent/include/workflow_engine.h"
#include "../external/basic_agent/include/command_processor.h"
#include "../external/basic_agent/include/executive_controller.h"
#include "../external/basic_agent/include/run_tests_tool.h"
#include "../external/basic_agent/include/step_metrics_repository.h"
#include "../external/basic_agent/include/embedding_engine.h"
#include "../external/basic_agent/include/index_manager.h"
#include "../external/basic_agent/include/rag.h"
#include "../external/basic_agent/include/memory.h"
#include "../external/basic_agent/include/config.h"
#include "../external/basic_agent/include/plan.h"
#include "../external/basic_agent/include/iplanner.h"
#include "../external/basic_agent/include/chunkers/code_chunk.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

std::atomic<int> g_probe_calls{0};

class ProbeTool : public ITool {
public:
    ProbeTool(std::string name, bool needs_confirmation)
        : name_(std::move(name)), needs_confirmation_(needs_confirmation) {}

    std::string name() const override { return name_; }
    std::string description() const override { return "containment probe"; }
    nlohmann::json input_schema() const override {
        return {{"type", "object"}, {"additionalProperties", true}};
    }
    bool requires_confirmation() const override { return needs_confirmation_; }
    nlohmann::json execute(const nlohmann::json&) const override {
        g_probe_calls.fetch_add(1, std::memory_order_relaxed);
        return {{"status", "success"}, {"data", nlohmann::json::object()}, {"error_message", ""}};
    }

private:
    std::string name_;
    bool needs_confirmation_;
};

class StubPlanner : public IPlanner {
public:
    Plan create_plan(const std::string&) override { return Plan(); }
    Plan revise_plan(const Plan& existing_plan, const nlohmann::json&) override {
        return existing_plan;
    }
};

bool containmentError(const nlohmann::json& body) {
    return body.value("error_message", "") == "fixed_evidence_containment";
}

void fail(const std::string& message) {
    std::cerr << message << "\n";
}

struct WorkflowHarness {
    Config config;
    std::shared_ptr<ToolRegistry> registry = std::make_shared<ToolRegistry>();
    std::shared_ptr<Thoth::StepMetricsRepository> metrics;
    std::unique_ptr<EmbeddingEngine> engine;
    std::unique_ptr<IndexManager> index;
    std::unique_ptr<RAGPipeline> rag;
    std::unique_ptr<Thoth::WorkflowEngine> workflow;
    std::filesystem::path metrics_path;

    explicit WorkflowHarness(const std::filesystem::path& dir) {
        std::filesystem::create_directories(dir);
        metrics_path = dir / "step_metrics.db";
        metrics = std::make_shared<Thoth::StepMetricsRepository>(metrics_path.string());
        engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &config);
        index = std::make_unique<IndexManager>(engine.get());
        rag = std::make_unique<RAGPipeline>(nullptr, index.get(), &config, nullptr);
        workflow = std::make_unique<Thoth::WorkflowEngine>(registry, nullptr, nullptr, metrics);
    }
};

PlanStep toolStep(const std::string& id, const std::string& tool) {
    PlanStep step;
    step.step_id = id;
    step.description = "tool step";
    step.type = StepType::TOOL;
    step.payload = {{"tool", tool}};
    step.failure_policy.max_retries = 1;
    step.failure_policy.revise_plan_on_failure = false;
    return step;
}

bool expectDeniedStep(const Thoth::StepResult& result, const std::string& label) {
    if (result.success) {
        fail(label + ": step succeeded");
        return false;
    }
    if (result.error_message != "fixed_evidence_containment") {
        fail(label + ": error_message=" + result.error_message);
        return false;
    }
    if (!containmentError(result.data)) {
        fail(label + ": data is not the containment contract");
        return false;
    }
    if (g_probe_calls.load() != 0) {
        fail(label + ": handler invocations=" + std::to_string(g_probe_calls.load()));
        return false;
    }
    return true;
}

bool chatDenied(const std::string& tool, const nlohmann::json& input, const std::string& label) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "thoth_lx_containment" / label;
    std::filesystem::create_directories(dir);
    Config config;
    config.database_path = (dir / "memory.db").string();
    config.allow_shell_exec = false;
    Memory memory(config);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &config);
    IndexManager index(engine.get());
    RAGPipeline rag(std::move(engine), &index, &config, &memory);
    LLMInterface llm(LLMBackend::Ollama, &config);
    CommandProcessor processor(memory, rag, llm, &config);
    DecisionTrace trace;

    g_probe_calls.store(0);
    ToolRegistry::instance().registerTool(std::make_unique<ProbeTool>(tool, false));
    Thoth::FixedEvidenceContainment::enable();

    const nlohmann::json call = {{"tool_call", {{"name", tool}, {"input", input}}}};
    const std::string response = processor.processToolCall(call.dump(), trace);
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(response);
    } catch (...) {
        fail(label + ": response was not JSON");
        std::filesystem::remove_all(dir);
        return false;
    }
    const bool ok = containmentError(body) && g_probe_calls.load() == 0 &&
                    body.value("tool", "") == tool;
    if (!ok) {
        fail(label + ": " + response + " calls=" + std::to_string(g_probe_calls.load()));
    }
    std::filesystem::remove_all(dir);
    return ok;
}

bool testChatWebScrape() {
    return chatDenied("web_scrape", {{"url", "https://example.invalid/case"}}, "chat_web_scrape");
}

bool testChatCodeModify() {
    const bool read_ok = chatDenied(
        "code_modify",
        {{"operation", "read"}, {"file_path", "case_notes.txt"}},
        "chat_code_modify_read");
    const bool apply_ok = chatDenied(
        "code_modify",
        {{"operation", "apply_diff"}, {"file_path", "case_notes.txt"}, {"diff", "---"}},
        "chat_code_modify_apply");
    return read_ok && apply_ok;
}

bool testWorkflowToolRetry() {
    const auto dir = std::filesystem::temp_directory_path() / "thoth_lx_containment" / "workflow_tool";
    WorkflowHarness harness(dir);
    g_probe_calls.store(0);
    harness.registry->registerTool(std::make_unique<ProbeTool>("web_scrape", false));
    Thoth::FixedEvidenceContainment::enable();
    PlanStep step = toolStep("tool-1", "web_scrape");
    const Thoth::StepResult result = harness.workflow->executeStep(step, "plan-1");
    const bool ok = expectDeniedStep(result, "workflow_tool_retry") && result.final_retry_count == 1;
    if (result.final_retry_count != 1) {
        fail("workflow_tool_retry: final_retry_count=" + std::to_string(result.final_retry_count));
    }
    std::filesystem::remove_all(dir);
    return ok;
}

bool retrievalDenied(const PlanStep& step, const std::string& label) {
    const auto dir = std::filesystem::temp_directory_path() / "thoth_lx_containment" / label;
    WorkflowHarness harness(dir);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &harness.config);
    IndexManager index(engine.get());
    RAGPipeline rag(std::move(engine), &index, &harness.config, nullptr);
    auto workflow = std::make_unique<Thoth::WorkflowEngine>(
        harness.registry, std::shared_ptr<RAGPipeline>(&rag, [](RAGPipeline*) {}), nullptr, harness.metrics);
    g_probe_calls.store(0);
    harness.registry->registerTool(std::make_unique<ProbeTool>("web_scrape", false));
    Thoth::FixedEvidenceContainment::enable();
    const Thoth::StepResult result = workflow->executeStep(step, "plan-r");
    const bool fell_through = result.success ||
                              (result.data.is_object() && result.data.value("status", "") == "success");
    const bool ok = expectDeniedStep(result, label) && !fell_through;
    if (fell_through) {
        fail(label + ": retrieval produced a success result");
    }
    std::filesystem::remove_all(dir);
    return ok;
}

PlanStep retrievalStep() {
    PlanStep step;
    step.step_id = "ret-1";
    step.description = "retrieve the fixed bundle";
    step.type = StepType::RETRIEVAL;
    step.payload = {{"query", "alpha token diagnostic bundle"}, {"top_k", 3}};
    return step;
}

bool testRetrievalPayloadTool() {
    PlanStep step = retrievalStep();
    step.payload["tool"] = "web_scrape";
    return retrievalDenied(step, "retrieval_payload_tool");
}

bool testRetrievalPayloadToolName() {
    PlanStep step = retrievalStep();
    step.payload["tool_name"] = "web_scrape";
    return retrievalDenied(step, "retrieval_payload_tool_name");
}

bool testRetrievalStepTool() {
    PlanStep step = retrievalStep();
    step.tool = {{"tool", "web_scrape"}};
    return retrievalDenied(step, "retrieval_step_tool");
}

bool testEmptyAndNonString() {
    PlanStep empty = retrievalStep();
    empty.payload["tool"] = "";
    PlanStep empty_step_tool = retrievalStep();
    empty_step_tool.tool = "";
    PlanStep non_string = retrievalStep();
    non_string.payload["tool"] = 1;
    PlanStep non_string_step_tool = retrievalStep();
    non_string_step_tool.tool = 1;
    return retrievalDenied(empty, "retrieval_empty_marker") &&
           retrievalDenied(empty_step_tool, "retrieval_empty_step_tool") &&
           retrievalDenied(non_string, "retrieval_nonstr_marker") &&
           retrievalDenied(non_string_step_tool, "retrieval_nonstr_step_tool");
}

bool testOrdinaryRetrieval() {
    const auto dir = std::filesystem::temp_directory_path() / "thoth_lx_containment" / "ordinary_retrieval";
    std::filesystem::create_directories(dir);
    Config config;
    auto registry = std::make_shared<ToolRegistry>();
    auto metrics = std::make_shared<Thoth::StepMetricsRepository>((dir / "metrics.db").string());
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &config);
    IndexManager index(engine.get());
    CodeChunk chunk;
    chunk.fileName = "bundle.txt";
    chunk.symbolName = "note";
    chunk.code = "alpha token diagnostic bundle";
    chunk.keyword_score = 1.0f;
    chunk.embedding = engine->embed(chunk.code);
    index.addChunkToIndex(std::move(chunk));
    RAGPipeline rag(nullptr, &index, &config, nullptr);
    auto workflow = std::make_unique<Thoth::WorkflowEngine>(
        registry, std::shared_ptr<RAGPipeline>(&rag, [](RAGPipeline*) {}), nullptr, metrics);
    g_probe_calls.store(0);
    registry->registerTool(std::make_unique<ProbeTool>("web_scrape", false));
    Thoth::FixedEvidenceContainment::enable();
    const Thoth::StepResult result = workflow->executeStep(retrievalStep(), "plan-ordinary");
    const bool contained = result.error_message == "fixed_evidence_containment" ||
                           containmentError(result.data);
    const bool ok = !contained && g_probe_calls.load() == 0 &&
                    (result.success || result.data.value("retrieved_chunk_count", -1) >= 0);
    if (!ok) {
        fail("ordinary_retrieval: success=" + std::string(result.success ? "true" : "false") +
             " error=" + result.error_message + " data=" + result.data.dump());
    }
    std::filesystem::remove_all(dir);
    return ok;
}

bool testResumedToolStep() {
    const auto dir = std::filesystem::temp_directory_path() / "thoth_lx_containment" / "resumed";
    std::filesystem::create_directories(dir);
    Config config;
    config.database_path = (dir / "memory.db").string();
    auto memory = std::make_shared<Memory>(config);
    auto registry = std::make_shared<ToolRegistry>();
    g_probe_calls.store(0);
    registry->registerTool(std::make_unique<ProbeTool>("web_scrape", false));
    auto planner = std::make_shared<StubPlanner>();
    Thoth::ExecutiveController controller(planner, registry, nullptr, memory);
    controller.set_max_reflections(0);
    std::atomic<int> failed_events{0};
    std::string failed_error;
    controller.set_event_callback([&](const ControllerEvent& event) {
        if (event.type == EventType::STEP_FAILED) {
            failed_events.fetch_add(1);
            failed_error = event.metadata.value("error", "");
        }
    });
    Thoth::FixedEvidenceContainment::enable();
    Plan plan;
    plan.plan_id = "resumed-plan";
    plan.goal = "diagnose the historical case";
    plan.status = PlanStatus::ACTIVE;
    plan.steps.push_back(toolStep("resumed-tool", "web_scrape"));
    controller.resume_from_plan(plan);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline &&
           controller.get_state() != Thoth::ControllerState::FAILED &&
           controller.get_state() != Thoth::ControllerState::COMPLETED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    const bool ok = g_probe_calls.load() == 0 && failed_events.load() >= 1 &&
                    failed_error == "fixed_evidence_containment";
    if (!ok) {
        fail("resumed_tool_step: calls=" + std::to_string(g_probe_calls.load()) +
             " events=" + std::to_string(failed_events.load()) + " error=" + failed_error);
    }
    std::filesystem::remove_all(dir);
    return ok;
}

bool testConfirmStatus() {
    const Thoth::ContainmentConfirmation before =
        Thoth::FixedEvidenceContainment::confirmContainmentEnabled();
    if (before.containment_enabled || before.experimental_environment_ready ||
        std::string(before.detail) != "containment_disabled") {
        fail("confirm_status: before enable");
        return false;
    }
    Thoth::FixedEvidenceContainment::enable();
    Thoth::FixedEvidenceContainment::enable();
    const Thoth::ContainmentConfirmation after =
        Thoth::FixedEvidenceContainment::confirmContainmentEnabled();
    if (!after.containment_enabled || after.experimental_environment_ready ||
        std::string(after.detail) != "containment_only_environment_not_checked") {
        fail("confirm_status: after enable");
        return false;
    }
    if (!Thoth::FixedEvidenceContainment::enabled()) {
        fail("confirm_status: enabled() is false");
        return false;
    }
    return true;
}

bool testNoConfirmationAndConfirmed() {
    g_probe_calls.store(0);
    ToolRegistry registry;
    registry.registerTool(std::make_unique<ProbeTool>("summarize_text", false));
    Thoth::FixedEvidenceContainment::enable();
    const nlohmann::json without = registry.executeTool("summarize_text", {{"text", "bundle"}});
    const nlohmann::json with = registry.executeTool(
        "summarize_text", {{"text", "bundle"}, {"confirmed", true}});
    const bool ok = containmentError(without) && containmentError(with) &&
                    without.value("tool", "") == "summarize_text" &&
                    with.value("tool", "") == "summarize_text" &&
                    g_probe_calls.load() == 0;
    if (!ok) {
        fail("no_confirmation: calls=" + std::to_string(g_probe_calls.load()));
    }
    return ok;
}

bool testDisabledBehavior() {
    if (Thoth::FixedEvidenceContainment::enabled()) {
        fail("disabled_behavior: flag started enabled");
        return false;
    }
    const Thoth::ContainmentConfirmation status =
        Thoth::FixedEvidenceContainment::confirmContainmentEnabled();
    Config config;
    config.allow_shell_exec = false;
    ToolRegistry registry;
    registry.setConfig(&config);
    g_probe_calls.store(0);
    registry.registerTool(std::make_unique<ProbeTool>("summarize_text", false));
    const nlohmann::json probe = registry.executeTool("summarize_text", {{"text", "bundle"}});
    const nlohmann::json shell = registry.executeTool("run_tests", {{"confirmed", true}});
    const std::string shell_error = shell.value("error_message", "");
    const bool ok = !status.containment_enabled &&
                    g_probe_calls.load() == 1 &&
                    probe.value("status", "") == "success" &&
                    shell_error.find("Shell execution is disabled") != std::string::npos &&
                    shell_error != "fixed_evidence_containment";
    if (!ok) {
        fail("disabled_behavior: calls=" + std::to_string(g_probe_calls.load()) +
             " shell=" + shell_error);
    }
    return ok;
}

bool testStepFailedMetadata() {
    const auto dir = std::filesystem::temp_directory_path() / "thoth_lx_containment" / "step_failed";
    WorkflowHarness harness(dir);
    g_probe_calls.store(0);
    harness.registry->registerTool(std::make_unique<ProbeTool>("code_modify", true));
    Thoth::FixedEvidenceContainment::enable();
    PlanStep step = toolStep("denied-step", "code_modify");
    const Thoth::StepResult result = harness.workflow->executeStep(step, "plan-meta");
    if (!expectDeniedStep(result, "step_failed_metadata")) {
        std::filesystem::remove_all(dir);
        return false;
    }

    auto planner = std::make_shared<StubPlanner>();
    auto memory = std::shared_ptr<Memory>{};
    Thoth::ExecutiveController controller(planner, harness.registry, nullptr, memory);
    controller.set_max_reflections(0);
    Plan plan;
    plan.plan_id = "meta-plan";
    plan.status = PlanStatus::COMPLETED;
    plan.steps.push_back(step);
    controller.resume_from_plan(plan);
    std::string failed_error;
    controller.set_event_callback([&](const ControllerEvent& event) {
        if (event.type == EventType::STEP_FAILED) {
            failed_error = event.metadata.value("error", "");
        }
    });
    controller.handle_step_completion(result);
    const bool ok = failed_error == "fixed_evidence_containment" && g_probe_calls.load() == 0;
    if (!ok) {
        fail("step_failed_metadata: event error=" + failed_error);
    }
    std::filesystem::remove_all(dir);
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: thoth-containment-tests <case>\n";
        return 2;
    }
    const std::string id = argv[1];
    bool ok = false;
    if (id == "chat_web_scrape") ok = testChatWebScrape();
    else if (id == "chat_code_modify") ok = testChatCodeModify();
    else if (id == "workflow_tool_retry") ok = testWorkflowToolRetry();
    else if (id == "retrieval_payload_tool") ok = testRetrievalPayloadTool();
    else if (id == "retrieval_payload_tool_name") ok = testRetrievalPayloadToolName();
    else if (id == "retrieval_step_tool") ok = testRetrievalStepTool();
    else if (id == "retrieval_empty_and_nonstr") ok = testEmptyAndNonString();
    else if (id == "ordinary_retrieval") ok = testOrdinaryRetrieval();
    else if (id == "resumed_tool_step") ok = testResumedToolStep();
    else if (id == "confirm_status") ok = testConfirmStatus();
    else if (id == "no_confirmation") ok = testNoConfirmationAndConfirmed();
    else if (id == "confirmed_true") ok = testNoConfirmationAndConfirmed();
    else if (id == "disabled_behavior") ok = testDisabledBehavior();
    else if (id == "step_failed_metadata") ok = testStepFailedMetadata();
    else {
        std::cerr << "unknown case: " << id << "\n";
        return 2;
    }
    return ok ? 0 : 1;
}
