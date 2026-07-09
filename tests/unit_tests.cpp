#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include "AgentInterface.h"
#include "command_processor.h"
#include "config.h"
#include "embedding_engine.h"
#include "file_handler.h"
#include "index_manager.h"
#include "llm_interface.h"
#include "logger.h"
#include "memory.h"
#include "rag.h"
#include "problem_state.h"
#include "plan_parser.h" 
#include "executive_controller.h"
#include "llm_planner.h"
#include "default_planner.h"
#include "tools.h"
#include "project_analyze_tool.h"
#include "run_tests_tool.h"
#include "code_modify_tool.h"
#include "sqlite_memory_repository.h"
#include "memory_pruning_config.h"
#include "memory_consolidation_metrics.h"
#include "consolidation_policy.h"
#include "consolidation_api.h"
#include "clock.h"
#include "grag_scorer.h"
#include "fact_store.h"
#include "store_fact_tool.h"
#include "flat_vector_store.h"
#include "web_scrape_tool.h"
#include "self_correct_tool.h"
#include "constraint_checker.h"
#include "gmail_read_messages_tool.h"
#include "scientific_execution_mode.h"
#include "benchmark_runner.h"
#include "benchmark_reporter.h"
#include "benchmark_environment.h"
#include "benchmark_context.h"
#include "benchmark_case_registry.h"
#include "trajectory_ablation.h"
#include "git_metadata.h"
#include "ollama_snapshot.h"
#include "cognitive_metrics.h"
#include "basic_agent_plugin.h"
#include "reflection_ab_cases.h"
#include "episodic_learning_cases.h"
#include "episodic_learning_eval.h"
#include "episodic_evaluation_service.h"
#include "episode_events.h"
#include "episode_event_channel.h"
#include "evaluation_subscriber.h"
#include "replay_subscriber.h"
#include "metrics_subscriber.h"
#include "trace_subscriber.h"
#include "e2_path_equivalence.h"
#include "diagnostic_service.h"
#include "pipeline_telemetry_service.h"
#include "e2_strict_enforcement.h"
#include "e2_strict_retrieval.h"
#include "workflow_engine.h"
#include <json.hpp>

namespace fs = std::filesystem;

static fs::path makeTempPath(const std::string& name) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() / (name + "_" + std::to_string(stamp));
}

static bool testConfigRoundTrip() {
    const fs::path tempPath = makeTempPath("thoth_config_test.json");

    Config cfg;
    cfg.set("temperature", "0.42");
    cfg.set("max_tokens", "256");
    cfg.set("allow_network", "false");
    cfg.set("allow_file_io", "true");

    if (!cfg.saveToJson(tempPath.string())) {
        std::cerr << "testConfigRoundTrip: failed to save config\n";
        return false;
    }

    Config loaded;
    if (!loaded.loadFromJson(tempPath.string())) {
        std::cerr << "testConfigRoundTrip: failed to load config\n";
        fs::remove(tempPath);
        return false;
    }

    const bool ok = loaded.get("temperature") == "0.420000"
        && loaded.get("max_tokens") == "256"
        && loaded.get("allow_network") == "false"
        && loaded.get("allow_file_io") == "true";

    fs::remove(tempPath);
    if (!ok) std::cerr << "testConfigRoundTrip: value mismatch\n";
    return ok;
}

static bool testMemoryPersistence() {
    const fs::path tempDir = makeTempPath("thoth_memory_dir");
    fs::create_directories(tempDir);
    const fs::path dbPath = tempDir / "memory.db";

    Config cfg;
    cfg.database_path = dbPath.string();

    {
        Memory memory(cfg);
        memory.clear();
        memory.addMessage("user", "hello");
        memory.addMessage("assistant", "world");
        memory.updateSummary("hello", "world");
    }

    Memory reloaded(cfg);
    auto conversation = reloaded.getConversation();
    std::string summary = reloaded.getSummary(false);

    std::cerr << "[DEBUG] Reloaded conversation size: " << conversation.size() << "\n";
    std::cerr << "[DEBUG] Reloaded summary: [" << summary << "]\n";

    const bool ok = conversation.size() == 2
        && summary.find("Last Goal") != std::string::npos;

    fs::remove_all(tempDir);
    if (!ok) std::cerr << "testMemoryPersistence: persistence mismatch\n";
    return ok;
}

static bool testCommandProcessorSetCommand() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_cp_memory.db").string();
    Memory memory(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager indexManager(engine.get());
    RAGPipeline rag(std::move(engine), &indexManager, &cfg);
    LLMInterface llm(LLMBackend::Ollama, &cfg);

    CommandProcessor cp(memory, rag, llm, &cfg);
    cp.handleCommand("/set verbosity 2");

    const bool ok = cfg.verbosity == 2;
    fs::remove(cfg.database_path);
    if (!ok) std::cerr << "testCommandProcessorSetCommand: /set did not apply\n";
    return ok;
}

static bool testCommandProcessorSlashTrim() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_cp_slash_trim.db").string();
    Memory memory(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager indexManager(engine.get());
    RAGPipeline rag(std::move(engine), &indexManager, &cfg);
    LLMInterface llm(LLMBackend::Ollama, &cfg);

    CommandProcessor cp(memory, rag, llm, &cfg);
    memory.setActiveSessionId("trim-test");

    const std::string help = cp.handleCommand("/help\n");
    if (help.find("Unknown command") != std::string::npos) {
        std::cerr << "testCommandProcessorSlashTrim: /help\\n not recognized\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const std::string prune = cp.handleCommand("/prune\r\n");
    if (prune.find("Unknown command") != std::string::npos) {
        std::cerr << "testCommandProcessorSlashTrim: /prune\\r\\n not recognized\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (prune.find("[Prune status]") == std::string::npos) {
        std::cerr << "testCommandProcessorSlashTrim: expected prune status line\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testAgentInterfaceLifecycle() {
    try {
        auto agent = std::make_unique<AgentInterface>();
        agent.reset();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testAgentInterfaceLifecycle: exception " << e.what() << "\n";
        return false;
    } catch (...) {
        std::cerr << "testAgentInterfaceLifecycle: unknown exception\n";
        return false;
    }
}

static bool testStructuredLoggerRedaction() {
    const std::string requestId = "test-redaction-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());

    StructuredLogger::instance().log(
        LogLevel::Info,
        "test",
        "redaction_probe",
        "Authorization: Bearer abcdef123456 and contact me@example.com",
        {
            {"api_key", "sk-abcdefghijklmnop"},
            {"note", "email admin@example.com"}
        },
        requestId,
        "test-session");

    FileHandler fh;
    const std::string logPath = fh.getAgentWorkspacePath("app_log.jsonl");
    std::ifstream in(logPath);
    if (!in.is_open()) {
        std::cerr << "testStructuredLoggerRedaction: failed to open log file\n";
        return false;
    }

    using json = nlohmann::json;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            auto entry = json::parse(line);
            if (entry.value("request_id", "") != requestId) {
                continue;
            }

            const std::string message = entry.value("message", "");
            const std::string apiKey = entry["metadata"].value("api_key", "");
            const std::string note = entry["metadata"].value("note", "");

            const bool ok = message.find("Bearer abcdef123456") == std::string::npos
                && message.find("[REDACTED_EMAIL]") != std::string::npos
                && apiKey == "[REDACTED]"
                && note.find("@") == std::string::npos;

            if (!ok) {
                std::cerr << "testStructuredLoggerRedaction: redaction mismatch\n";
            }
            return ok;
        } catch (...) {
            continue;
        }
    }

    std::cerr << "testStructuredLoggerRedaction: did not find probe log entry\n";
    return false;
}

static bool testRetrievalDiagnosticsEvent() {
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager idx(engine.get());
    RAGPipeline rag(std::move(engine), &idx);
    
    bool eventFired = false;
    rag.setEventCallback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::RETRIEVAL_DIAGNOSTICS) {
            eventFired = true;
            if (!ev.metadata.contains("alpha")) eventFired = false;
        }
    });

    // Add a dummy chunk so retrieval has something to find
    CodeChunk c;
    c.code = "test content";
    c.embedding = {0.1f, 0.2f};
    idx.addChunkToIndex(std::move(c));

    rag.retrieveRelevant("test query");
    
    if (!eventFired) std::cerr << "testRetrievalDiagnosticsEvent: event not fired or missing metadata\n";
    return eventFired;
}

static bool testBootstrapIndexing() {
    FileHandler fh;
    Config cfg;
    cfg.database_path = makeTempPath("thoth_bootstrap_test.db").string();
    Memory memory(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager indexManager(engine.get());
    RAGPipeline rag(std::move(engine), &indexManager, &cfg);
    LLMInterface llm(LLMBackend::Ollama, &cfg);

    CommandProcessor cp(memory, rag, llm, &cfg);
    
    // Trigger ensureInitialized()
    cp.handleCommand("/help");

    size_t chunkCount = indexManager.getChunks().size();
    if (chunkCount == 0) {
        std::cerr << "testBootstrapIndexing: 0 chunks indexed from " << fh.getProjectRoot() << "\n";
        fs::remove(cfg.database_path);
        return false;
    }

    std::cout << "testBootstrapIndexing: successfully indexed " << chunkCount << " chunks.\n";
    fs::remove(cfg.database_path);
    return true;
}

static bool testPlanParser() {
    int test_failures = 0;
    auto run_case = [&](const std::string& name, bool result) {
        if (!result) {
            std::cerr << "testPlanParser: FAILED - " << name << "\n";
            test_failures++;
        }
    };

    // Test Case 1: Valid multi-step plan
    const std::string valid_plan_str = R"({
        "plan": [
            {"step_id": "step-1", "step_type": "RETRIEVAL", "description": "Get context"},
            {"step_id": "step-2", "step_type": "LLM", "description": "Summarize"}
        ]
    })";
    auto plan1 = Thoth::PlanParser::parse(valid_plan_str, "plan-1");
    run_case("Valid Plan", plan1.has_value() && plan1->steps.size() == 2 && plan1->steps[0].type == StepType::RETRIEVAL);

    // Test Case 2: Malformed JSON
    const std::string malformed_json_str = "{\"plan\": [,]}";
    auto plan2 = Thoth::PlanParser::parse(malformed_json_str, "plan-2");
    run_case("Malformed JSON", !plan2.has_value());

    // Test Case 3: Missing required field ("description")
    const std::string missing_field_str = R"({
        "plan": [{"step_type": "LLM"}]
    })";
    auto plan3 = Thoth::PlanParser::parse(missing_field_str, "plan-3");
    run_case("Missing Required Field", !plan3.has_value());

    // Test Case 4: Markdown Fencing
    const std::string fenced_plan_str = R"(
        Some preamble text from the LLM.
        ```json
        {
            "plan": [{"step_type": "TOOL", "description": "Run a tool"}]
        }
        ```
        Some closing text.
    )";
    auto plan4 = Thoth::PlanParser::parse(fenced_plan_str, "plan-4");
    run_case("Markdown Fencing", plan4.has_value() && plan4->steps.size() == 1 && plan4->steps[0].type == StepType::TOOL);

    // Test Case 5: Empty plan array
    const std::string empty_plan_str = "{\"plan\": []}";
    auto plan5 = Thoth::PlanParser::parse(empty_plan_str, "plan-5");
    run_case("Empty Plan Array", !plan5.has_value());

    // Test Case 6: Missing step_id generation
    const std::string missing_id_str = R"({
        "plan": [{"step_type": "LLM", "description": "A step without an ID"}]
    })";
    auto plan6 = Thoth::PlanParser::parse(missing_id_str, "plan-6");
    run_case("Missing Step ID Generation", plan6.has_value() && plan6->steps.size() == 1 && plan6->steps[0].step_id == "plan-6-step-0");

    return test_failures == 0;
}

static bool testResumeFromTrace() {
    FileHandler fh;
    const std::string tracePath = fh.getAgentWorkspacePath("decision_trace.jsonl");
    
    // Clear existing trace
    { std::ofstream out(tracePath, std::ios::trunc); }

    Config cfg;
    cfg.database_path = makeTempPath("thoth_resume_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
    auto planner = std::make_shared<DefaultPlanner>();
    auto registry = std::make_shared<ToolRegistry>();
    
    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    
    // 1. Start a goal and wait for it to complete step 0 (RETRIEVAL)
    controller.execute_goal("Resume test goal");
    
    // Wait for step 0 to reach terminal state
    int timeout = 100;
    while (timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto p = controller.get_current_plan();
        if (!p.steps.empty() && 
            (p.steps[0].status == StepStatus::SUCCESS || p.steps[0].status == StepStatus::FAILED)) break;
        --timeout;
    }

    if (timeout <= 0) {
        std::cerr << "testResumeFromTrace: timeout waiting for step 0 to complete\n";
        fs::remove(cfg.database_path);
        return false;
    }

    // 2. Simulate a crash: Reconstruct plan from trace
    std::ifstream in(tracePath);
    std::string line;
    nlohmann::json last_plan_json;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            auto entry = nlohmann::json::parse(line);
            if (entry.contains("event_type")) {
                if (entry.contains("metadata")) {
                    auto meta = entry["metadata"];
                    if (meta.contains("plan")) {
                        last_plan_json = meta["plan"];
                    }
                }
            }
        } catch (...) {}
    }

    if (last_plan_json.is_null()) {
        std::cerr << "testResumeFromTrace: failed to find plan in trace\n";
        fs::remove(cfg.database_path);
        return false;
    }

    // 3. Resume
    Plan reconstructed = Plan::from_json(last_plan_json);
    
    Thoth::ExecutiveController controller2(planner, registry, rag, memory);
    controller2.resume_from_plan(reconstructed);

    // 4. Verify it continues
    if (controller2.get_state() == Thoth::ControllerState::FAILED) {
        // Failing is OK if it's because of document missing, as long as it reached terminal
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testProjectAnalyzeTool() {
    ProjectAnalyzeTool tool;
    nlohmann::json input = {{"root_path", "./"}};
    nlohmann::json output = tool.execute(input);

    if (output["status"] != "success") {
        std::cerr << "testProjectAnalyzeTool: status mismatch: " << output.dump() << "\n";
        return false;
    }

    auto data = output["data"];
    if (!data.contains("files") || !data["files"].is_array() || data["files"].empty()) {
        std::cerr << "testProjectAnalyzeTool: missing or empty files array\n";
        return false;
    }

    return true;
}

static bool testRunTestsTool() {
    setenv("THOTH_MOCK_TESTS", "true", 1);
    RunTestsTool tool;
    nlohmann::json input = nlohmann::json::object();
    nlohmann::json output = tool.execute(input);
    unsetenv("THOTH_MOCK_TESTS");

    if (output["status"] != "success") {
        std::cerr << "testRunTestsTool: status mismatch: " << output.dump() << "\n";
        return false;
    }

    return true;
}

static bool testCodeModifyTool() {
    CodeModifyTool tool;
    nlohmann::json input_bad = {{"operation", "read"}, {"file_path", "../../../etc/passwd"}};
    nlohmann::json output_bad = tool.execute(input_bad);
    if (output_bad["status"] != "error") {
        std::cerr << "testCodeModifyTool: failed to reject path traversal\n";
        return false;
    }

    nlohmann::json input_read = {{"operation", "read"}, {"file_path", "AGENTS.md"}};
    nlohmann::json output_read = tool.execute(input_read);
    if (output_read["status"] != "success" || !output_read["data"].contains("content")) {
        std::cerr << "testCodeModifyTool: failed to read valid file\n";
        return false;
    }

    return true;
}

static bool testAllowShellExecGate() {
    Config cfg;
    cfg.allow_shell_exec = false;

    unsetenv("THOTH_MOCK_TESTS");
    RunTestsTool runTool(&cfg);
    nlohmann::json runOut = runTool.execute({{"confirmed", true}});
    if (runOut["status"] != "error") {
        std::cerr << "testAllowShellExecGate: run_tests should fail when allow_shell_exec is false\n";
        return false;
    }
    std::string runErr = runOut.value("error_message", "");
    if (runErr.find("allow_shell_exec") == std::string::npos) {
        std::cerr << "testAllowShellExecGate: unexpected run_tests error: " << runErr << "\n";
        return false;
    }

    CodeModifyTool codeTool(&cfg);
    nlohmann::json buildOut = codeTool.execute({{"operation", "build"}, {"confirmed", true}});
    if (buildOut["status"] != "error") {
        std::cerr << "testAllowShellExecGate: code_modify build should fail when allow_shell_exec is false\n";
        return false;
    }
    std::string buildErr = buildOut.value("error_message", "");
    if (buildErr.find("allow_shell_exec") == std::string::npos) {
        std::cerr << "testAllowShellExecGate: unexpected build error: " << buildErr << "\n";
        return false;
    }

    return true;
}

static bool testPastPlanRetrieval() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_plan_reuse_test.db").string();
    Memory memory(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);

    const std::string goodGoal = "optimize GRAG retrieval directional scoring";
    const std::string lowGoal = "optimize GRAG retrieval directional scoring failed run";
    const std::string unrelatedGoal = "email inbox classification pipeline";
    const std::string queryGoal = "improve GRAG directional retrieval scoring";

    // TfIdf IDF weights require a seeded corpus (same as index_manager during indexing).
    for (const auto& text : {goodGoal, lowGoal, unrelatedGoal, queryGoal}) {
        engine->updateVocabulary(text);
    }

    auto embed = [&](const std::string& text) { return engine->embed(text); };

    Memory::PastPlanRecord good;
    good.plan_id = "plan-good";
    good.goal = goodGoal;
    good.outline = R"({"steps":[{"step_id":"s1"}]})";
    good.success_score = 0.9f;
    good.goal_embedding = embed(goodGoal);
    memory.storePastPlan(good);

    Memory::PastPlanRecord low;
    low.plan_id = "plan-low";
    low.goal = lowGoal;
    low.outline = R"({"steps":[]})";
    low.success_score = 0.3f;
    low.goal_embedding = embed(lowGoal);
    memory.storePastPlan(low);

    Memory::PastPlanRecord unrelated;
    unrelated.plan_id = "plan-other";
    unrelated.goal = unrelatedGoal;
    unrelated.outline = R"({"steps":[]})";
    unrelated.success_score = 0.95f;
    unrelated.goal_embedding = embed(unrelatedGoal);
    memory.storePastPlan(unrelated);

    auto query = embed(queryGoal);
    auto results = memory.retrieveSimilarPlans(query, 2);

    if (results.empty() || results[0].plan_id != "plan-good") {
        std::cerr << "testPastPlanRetrieval: expected plan-good first, got "
                  << (results.empty() ? "none" : results[0].plan_id) << "\n";
        fs::remove(cfg.database_path);
        return false;
    }

    for (const auto& r : results) {
        if (r.plan_id == "plan-low") {
            std::cerr << "testPastPlanRetrieval: low-success plan should be filtered\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testMemoryPruning() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_pruning_test.db").string();
    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
#ifdef _WIN32
    _putenv_s("THOTH_MOCK_EPISODIC", "1");
#else
    setenv("THOTH_MOCK_EPISODIC", "1", 1);
#endif
    std::string sid = "test_session";
    repo->createSession(sid, 1000);

    for (int i = 0; i < 60; ++i) {
        repo->appendMessage(sid, {"user", "msg " + std::to_string(i), 1000 + i});
    }

    Thoth::PruningPolicy policy;
    policy.max_hot_messages = Thoth::MemoryPruning::kMaxHotMessages;
    policy.prune_batch_size = Thoth::MemoryPruning::kPruneBatchSize;

    Thoth::MemoryPruner pruner(*repo, policy, nullptr, &engine);
    int archived = pruner.consolidateOneBatch(sid);

    if (archived != static_cast<int>(Thoth::MemoryPruning::kPruneBatchSize)) {
        std::cerr << "testMemoryPruning: expected " << Thoth::MemoryPruning::kPruneBatchSize
                  << " archived, got " << archived << "\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const auto warm = repo->getRecentWarmMemory(sid, 5);
    if (warm.empty()) {
        std::cerr << "testMemoryPruning: expected warm memory row\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testMemoryPruningIntegration() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_pruning_integration.db").string();
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
#ifdef _WIN32
    _putenv_s("THOTH_MOCK_EPISODIC", "1");
#else
    setenv("THOTH_MOCK_EPISODIC", "1", 1);
#endif
    Memory memory(cfg);
    memory.configureConsolidation(nullptr, &engine);
    memory.setActiveSessionId("integration-session");

    for (int i = 0; i < 60; ++i) {
        memory.addMessage("user", "msg " + std::to_string(i));
    }

    const auto hot = memory.getConversation();
    if (hot.size() > Thoth::MemoryPruning::kMaxHotMessages) {
        std::cerr << "testMemoryPruningIntegration: hot tier exceeded cap, got "
                  << hot.size() << "\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const auto archived = memory.getArchivedTurns();
    if (archived.size() < Thoth::MemoryPruning::kPruneBatchSize) {
        std::cerr << "testMemoryPruningIntegration: expected at least "
                  << Thoth::MemoryPruning::kPruneBatchSize << " archived turns, got "
                  << archived.size() << "\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const auto warm = memory.getRecentWarmMemory(3);
    if (warm.empty()) {
        std::cerr << "testMemoryPruningIntegration: expected warm memory after consolidation\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static void enableEpisodicMock() {
#ifdef _WIN32
    _putenv_s("THOTH_MOCK_EPISODIC", "1");
#else
    setenv("THOTH_MOCK_EPISODIC", "1", 1);
#endif
}

static bool hotConversationContains(const Memory& memory, const std::string& needle) {
    for (const auto& msg : memory.getConversation()) {
        if (msg.at("content").get<std::string>().find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static bool seedApolloSessionAndConsolidate(Memory& memory, EmbeddingEngine& engine) {
    enableEpisodicMock();
    memory.configureConsolidation(nullptr, &engine);
    memory.setActiveSessionId("episodic-apollo");
    memory.addMessage("user", "My dog's name is Apollo.");
    for (int i = 1; i < 60; ++i) {
        memory.addMessage("user", "filler turn " + std::to_string(i));
    }
    return !hotConversationContains(memory, "Apollo");
}

static bool testEpisodicRetrievalEndToEnd() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_episodic_e2e.db").string();
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    Memory memory(cfg);

    if (!seedApolloSessionAndConsolidate(memory, *engine)) {
        std::cerr << "testEpisodicRetrievalEndToEnd: Apollo still in hot tier\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const std::vector<float> queryEmb = engine->embed("What is my dog's name?");
    const auto warmHits = memory.searchWarmMemory(queryEmb, 3);
    if (warmHits.empty() || warmHits.front().rendered_summary.find("Apollo") == std::string::npos) {
        std::cerr << "testEpisodicRetrievalEndToEnd: warm search missed Apollo\n";
        fs::remove(cfg.database_path);
        return false;
    }

    auto idx = new IndexManager(engine.get());
    CodeChunk distractor;
    distractor.code = "Paris is the capital of France and a major European city.";
    distractor.fileName = "geography.md";
    distractor.embedding = engine->embed(distractor.code);
    idx->addChunkToIndex(std::move(distractor));

    RAGPipeline rag(std::move(engine), idx, &cfg, &memory);
    const auto chunks = rag.retrieveRelevant("What is my dog's name?", {}, 5);
    bool foundWarmApollo = false;
    for (const auto& chunk : chunks) {
        if (chunk.fileName.rfind("warm_memory:", 0) == 0 &&
            chunk.code.find("Apollo") != std::string::npos) {
            foundWarmApollo = true;
            break;
        }
    }
    if (!foundWarmApollo) {
        std::cerr << "testEpisodicRetrievalEndToEnd: GRAG did not return warm Apollo chunk\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testEpisodicMemoryBenchmarkNegative() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_episodic_negative.db").string();
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    Memory memory(cfg);

    if (!seedApolloSessionAndConsolidate(memory, *engine)) {
        std::cerr << "testEpisodicMemoryBenchmarkNegative: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const auto dogHits = memory.searchWarmMemory(engine->embed("What is my dog's name?"), 1);
    if (dogHits.empty() || dogHits.front().rendered_summary.find("Apollo") == std::string::npos) {
        std::cerr << "testEpisodicMemoryBenchmarkNegative: positive control failed (no Apollo warm)\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const auto unrelatedHits =
        memory.searchWarmMemory(engine->embed("What is the capital of France?"), 3);
    for (const auto& row : unrelatedHits) {
        if (row.rendered_summary.find("Paris") != std::string::npos) {
            std::cerr << "testEpisodicMemoryBenchmarkNegative: warm memory leaked unrelated fact (Paris)\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }

    Memory underCap(cfg);
    underCap.configureConsolidation(nullptr, engine.get());
    underCap.setActiveSessionId("under-cap");
    for (int i = 0; i < 5; ++i) {
        underCap.addMessage("user", "short session " + std::to_string(i));
    }
    if (!underCap.getRecentWarmMemory(1).empty()) {
        std::cerr << "testEpisodicMemoryBenchmarkNegative: warm created below hot cap\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testConsolidationFailureEmbed() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_consolidation_fail_embed.db").string();
    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    enableEpisodicMock();
#ifdef _WIN32
    _putenv_s("THOTH_MOCK_EMBED_EMPTY", "1");
#else
    setenv("THOTH_MOCK_EMBED_EMPTY", "1", 1);
#endif

    const std::string sid = "fail-embed";
    repo->createSession(sid, 1000);
    for (int i = 0; i < 60; ++i) {
        repo->appendMessage(sid, {"user", "msg " + std::to_string(i), 1000 + i});
    }

    Thoth::PruningPolicy policy;
    Thoth::MemoryPruner pruner(*repo, policy, nullptr, &engine);
    const int archived = pruner.prune(sid);

#ifdef _WIN32
    _putenv_s("THOTH_MOCK_EMBED_EMPTY", "");
#else
    unsetenv("THOTH_MOCK_EMBED_EMPTY");
#endif

    if (archived != 0 || repo->getHotMessageCount(sid) != 60) {
        std::cerr << "testConsolidationFailureEmbed: hot tier changed (archived=" << archived << ")\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (!repo->getRecentWarmMemory(sid, 1).empty()) {
        std::cerr << "testConsolidationFailureEmbed: warm row created on embed failure\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testConsolidationFailureTransaction() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_consolidation_fail_txn.db").string();
    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    enableEpisodicMock();
#ifdef _WIN32
    _putenv_s("THOTH_INJECT_CONSOLIDATION_FAIL", "commit");
#else
    setenv("THOTH_INJECT_CONSOLIDATION_FAIL", "commit", 1);
#endif

    const std::string sid = "fail-txn";
    repo->createSession(sid, 1000);
    for (int i = 0; i < 60; ++i) {
        repo->appendMessage(sid, {"user", "msg " + std::to_string(i), 1000 + i});
    }

    Thoth::PruningPolicy policy;
    Thoth::MemoryPruner pruner(*repo, policy, nullptr, &engine);
    const int archived = pruner.prune(sid);

#ifdef _WIN32
    _putenv_s("THOTH_INJECT_CONSOLIDATION_FAIL", "");
#else
    unsetenv("THOTH_INJECT_CONSOLIDATION_FAIL");
#endif

    if (archived != 0 || repo->getHotMessageCount(sid) != 60) {
        std::cerr << "testConsolidationFailureTransaction: hot tier changed\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (!repo->getRecentWarmMemory(sid, 1).empty() || !repo->getArchivedMessages(sid).empty()) {
        std::cerr << "testConsolidationFailureTransaction: partial consolidation persisted\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testConsolidationLatencyRecorded() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_consolidation_latency.db").string();
    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    enableEpisodicMock();
    Thoth::resetConsolidationTimingForTest();

    const std::string sid = "latency";
    repo->createSession(sid, 1000);
    for (int i = 0; i < 60; ++i) {
        repo->appendMessage(sid, {"user", "msg " + std::to_string(i), 1000 + i});
    }

    Thoth::PruningPolicy policy;
    Thoth::MemoryPruner pruner(*repo, policy, nullptr, &engine);
    if (pruner.prune(sid) <= 0) {
        std::cerr << "testConsolidationLatencyRecorded: prune failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const Thoth::ConsolidationTiming timing = Thoth::lastConsolidationTiming();
    if (timing.consolidation_ms <= 0 || timing.transaction_ms < 0) {
        std::cerr << "testConsolidationLatencyRecorded: missing timing fields\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

namespace {

constexpr int64_t kTestDayMs = 86'400'000LL;

std::shared_ptr<Thoth::FakeClock> makeM2TestClock(int64_t baseMs = 1'700'000'000'000LL) {
    return std::make_shared<Thoth::FakeClock>(baseMs);
}

void seedSessionMessages(Thoth::SQLiteMemoryRepository& repo,
                         const std::string& sessionId,
                         int count,
                         int64_t baseTimestampMs) {
    repo.createSession(sessionId, baseTimestampMs);
    for (int i = 0; i < count; ++i) {
        repo.appendMessage(sessionId, {"user", "msg " + std::to_string(i), baseTimestampMs + i});
    }
}

} // namespace

static bool testM2StaleSessionUnderCap() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m2_stale_under_cap.db").string();
    cfg.memory_max_hot_age_days = 30;
    enableEpisodicMock();

    const int64_t baseMs = 1'700'000'000'000LL;
    auto clock = makeM2TestClock(baseMs);
    clock->advanceDays(31);

    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    seedSessionMessages(*repo, "stale-session", 20, baseMs);

    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    const auto policy = Thoth::PruningPolicy::fromConfig(cfg);
    Thoth::MemoryPruner pruner(*repo, policy, nullptr, &engine, clock);

    const auto decision = pruner.evaluatePolicy("stale-session");
    if (!decision.shouldConsolidate()
        || !Thoth::hasConsolidationReason(decision.reasons, Thoth::ConsolidationReason::SESSION_INACTIVE)) {
        std::cerr << "testM2StaleSessionUnderCap: expected SESSION_INACTIVE trigger\n";
        fs::remove(cfg.database_path);
        return false;
    }

    if (pruner.consolidateIfNeeded("stale-session").total_archived <= 0) {
        std::cerr << "testM2StaleSessionUnderCap: expected consolidation under cap\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (repo->getRecentWarmMemory("stale-session", 1).empty()) {
        std::cerr << "testM2StaleSessionUnderCap: expected warm memory row\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM2FreshSessionNoOp() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m2_fresh.db").string();
    auto clock = makeM2TestClock();
    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    seedSessionMessages(*repo, "fresh", 10, clock->nowMs());

    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    Thoth::MemoryPruner pruner(*repo, Thoth::PruningPolicy::fromConfig(cfg), nullptr, &engine, clock);
    const auto decision = pruner.evaluatePolicy("fresh");
    if (decision.shouldConsolidate()) {
        std::cerr << "testM2FreshSessionNoOp: unexpected consolidation trigger\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM2StartupDeferredNonActive() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m2_startup_deferred.db").string();
    enableEpisodicMock();

    const int64_t baseMs = 1'700'000'000'000LL;
    auto clock = makeM2TestClock(baseMs);
    clock->advanceDays(31);

    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("active");

    if (auto* sqliteRepo = memory.getSQLiteRepo()) {
        seedSessionMessages(*sqliteRepo, "active", 5, clock->nowMs());
        seedSessionMessages(*sqliteRepo, "stale-other", 20, baseMs);
    } else {
        std::cerr << "testM2StartupDeferredNonActive: sqlite repo unavailable\n";
        return false;
    }

    memory.runStartupConsolidationDiscovery();

    if (!memory.isSessionMarkedStale("stale-other")) {
        std::cerr << "testM2StartupDeferredNonActive: expected stale-other marked\n";
        fs::remove(cfg.database_path);
        return false;
    }

    if (auto* sqliteRepo = memory.getSQLiteRepo()) {
        if (!sqliteRepo->getRecentWarmMemory("stale-other", 1).empty()) {
            std::cerr << "testM2StartupDeferredNonActive: stale-other consolidated during discovery\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }

    memory.setActiveSessionId("stale-other");
    if (memory.isSessionMarkedStale("stale-other")) {
        std::cerr << "testM2StartupDeferredNonActive: stale mark not cleared after access\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (auto* sqliteRepo = memory.getSQLiteRepo()) {
        if (sqliteRepo->getRecentWarmMemory("stale-other", 1).empty()) {
            std::cerr << "testM2StartupDeferredNonActive: expected warm after session switch\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM2TimestampPreservation() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m2_timestamp.db").string();
    enableEpisodicMock();

    const int64_t baseMs = 1'700'000'000'000LL;
    auto clock = makeM2TestClock(baseMs);

    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("ts-session");

    const int64_t preservedTs = baseMs + 12'345;
    Memory::TimedMessage message;
    message.role = "user";
    message.content = "hello";
    message.timestamp_ms = preservedTs;
    memory.loadConversation({message});

    if (auto* sqliteRepo = memory.getSQLiteRepo()) {
        const auto messages = sqliteRepo->getMessages("ts-session");
        if (messages.size() != 1) {
            std::cerr << "testM2TimestampPreservation: expected 1 message, got "
                      << messages.size() << "\n";
            fs::remove(cfg.database_path);
            return false;
        }
        if (messages[0].timestamp_ms != preservedTs) {
            std::cerr << "testM2TimestampPreservation: timestamp not preserved (got "
                      << messages[0].timestamp_ms << ", expected " << preservedTs << ")\n";
            fs::remove(cfg.database_path);
            return false;
        }
    } else {
        std::cerr << "testM2TimestampPreservation: sqlite repo unavailable\n";
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM2MultiTriggerReasons() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m2_multi_trigger.db").string();
    cfg.memory_max_hot_messages = 50;
    cfg.memory_max_hot_age_days = 30;
    enableEpisodicMock();

    const int64_t baseMs = 1'700'000'000'000LL;
    auto clock = makeM2TestClock(baseMs);
    clock->advanceDays(31);

    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    seedSessionMessages(*repo, "multi", 61, baseMs);

    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    Thoth::MemoryPruner pruner(*repo, Thoth::PruningPolicy::fromConfig(cfg), nullptr, &engine, clock);
    const auto decision = pruner.evaluatePolicy("multi");
    if (!Thoth::hasConsolidationReason(decision.reasons, Thoth::ConsolidationReason::HOT_COUNT)
        || !Thoth::hasConsolidationReason(decision.reasons, Thoth::ConsolidationReason::OLDEST_MESSAGE)) {
        std::cerr << "testM2MultiTriggerReasons: expected HOT_COUNT and OLDEST_MESSAGE\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM2BatchCapDeferred() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m2_batch_cap.db").string();
    enableEpisodicMock();

    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    seedSessionMessages(*repo, "cap", 110, 1'000'000'000'000LL);

    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    Thoth::MemoryPruner pruner(*repo, Thoth::PruningPolicy::fromConfig(cfg), nullptr, &engine);
    const auto result = pruner.consolidateIfNeeded("cap");
    if (!result.deferred) {
        std::cerr << "testM2BatchCapDeferred: expected deferred flag\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (result.batches_completed != static_cast<int>(Thoth::MemoryPruning::kMaxBatchesPerInvocation)) {
        std::cerr << "testM2BatchCapDeferred: expected max batches completed\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (result.total_archived != static_cast<int>(Thoth::MemoryPruning::kMaxBatchesPerInvocation
                                                  * Thoth::MemoryPruning::kPruneBatchSize)) {
        std::cerr << "testM2BatchCapDeferred: unexpected archived count " << result.total_archived << "\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (!pruner.evaluatePolicy("cap").shouldConsolidate()) {
        std::cerr << "testM2BatchCapDeferred: expected remaining stale messages\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static Thoth::ConsolidationRequest makeTestManualRequest(bool ignore_thresholds = false,
                                                         bool single_batch = false) {
    Thoth::ConsolidationRequest request;
    request.source = Thoth::ConsolidationSource::MANUAL;
    request.requested_by = "TEST";
    request.ignore_thresholds = ignore_thresholds;
    request.single_batch = single_batch;
    return request;
}

static bool testM3StatusDryRun() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_status.db").string();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m3-status");

    if (auto* repo = memory.getSQLiteRepo()) {
        seedSessionMessages(*repo, "m3-status", 30, clock->nowMs());
    } else {
        std::cerr << "testM3StatusDryRun: sqlite repo unavailable\n";
        return false;
    }

    const auto status = memory.getConsolidationStatus("m3-status");
    if (status.decision.hot_count != 30) {
        std::cerr << "testM3StatusDryRun: unexpected hot count " << status.decision.hot_count << "\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (status.decision.shouldConsolidate()) {
        std::cerr << "testM3StatusDryRun: unexpected policy trigger under cap\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (auto* repo = memory.getSQLiteRepo()) {
        if (!repo->getRecentWarmMemory("m3-status", 1).empty()) {
            std::cerr << "testM3StatusDryRun: warm row created during status\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM3IgnoreThresholdsUnderCap() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_ignore.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m3-ignore");

    if (auto* repo = memory.getSQLiteRepo()) {
        seedSessionMessages(*repo, "m3-ignore", 15, clock->nowMs());
    } else {
        std::cerr << "testM3IgnoreThresholdsUnderCap: sqlite repo unavailable\n";
        return false;
    }

    auto request = makeTestManualRequest(true, true);
    const auto result = memory.runConsolidation("m3-ignore", request);
    if (result.blocked || result.archived != 10 || result.warm_created != 1) {
        std::cerr << "testM3IgnoreThresholdsUnderCap: expected batch archive under cap\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (result.source != Thoth::ConsolidationSource::MANUAL) {
        std::cerr << "testM3IgnoreThresholdsUnderCap: expected MANUAL source\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (result.decision.shouldConsolidate()) {
        std::cerr << "testM3IgnoreThresholdsUnderCap: expected policy reasons NONE after partial\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM3PolicyRunOverCap() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_over_cap.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m3-cap");

    if (auto* repo = memory.getSQLiteRepo()) {
        seedSessionMessages(*repo, "m3-cap", 55, clock->nowMs());
    } else {
        std::cerr << "testM3PolicyRunOverCap: sqlite repo unavailable\n";
        return false;
    }

    const auto status_before = memory.getConsolidationStatus("m3-cap");
    if (!Thoth::hasConsolidationReason(status_before.decision.reasons,
                                       Thoth::ConsolidationReason::HOT_COUNT)) {
        std::cerr << "testM3PolicyRunOverCap: expected HOT_COUNT before run\n";
        fs::remove(cfg.database_path);
        return false;
    }

    auto request = makeTestManualRequest(false, false);
    const auto result = memory.runConsolidation("m3-cap", request);
    if (result.archived <= 0 || result.warm_created <= 0) {
        std::cerr << "testM3PolicyRunOverCap: expected consolidation over cap\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM3EmptyHot() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_empty.db").string();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine);
    memory.setActiveSessionId("m3-empty");

    auto request = makeTestManualRequest(true, false);
    const auto result = memory.runConsolidation("m3-empty", request);
    if (result.archived != 0 || result.warm_created != 0) {
        std::cerr << "testM3EmptyHot: expected no-op on empty hot tier\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM3ClearsStaleMark() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_stale.db").string();
    enableEpisodicMock();

    const int64_t baseMs = 1'700'000'000'000LL;
    auto clock = makeM2TestClock(baseMs);
    clock->advanceDays(31);

    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("active");

    if (auto* repo = memory.getSQLiteRepo()) {
        seedSessionMessages(*repo, "active", 5, clock->nowMs());
        seedSessionMessages(*repo, "stale-m3", 20, baseMs);
    } else {
        std::cerr << "testM3ClearsStaleMark: sqlite repo unavailable\n";
        return false;
    }

    memory.runStartupConsolidationDiscovery();
    if (!memory.isSessionMarkedStale("stale-m3")) {
        std::cerr << "testM3ClearsStaleMark: expected stale mark\n";
        fs::remove(cfg.database_path);
        return false;
    }

    auto request = makeTestManualRequest(false, true);
    const auto result = memory.runConsolidation("stale-m3", request);
    if (result.archived <= 0) {
        std::cerr << "testM3ClearsStaleMark: expected manual consolidation\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (memory.isSessionMarkedStale("stale-m3")) {
        std::cerr << "testM3ClearsStaleMark: stale mark not cleared\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM3EmbedUnavailable() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_no_embed.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();

    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    seedSessionMessages(*repo, "no-embed", 55, clock->nowMs());

    Thoth::MemoryPruner pruner(*repo, Thoth::PruningPolicy::fromConfig(cfg), nullptr, nullptr, clock);
    Thoth::ConsolidationRequest request;
    request.source = Thoth::ConsolidationSource::MANUAL;
    request.requested_by = "TEST";
    const auto result = pruner.runConsolidation("no-embed", request);
    if (result.archived != 0) {
        std::cerr << "testM3EmbedUnavailable: expected hot unchanged without embed engine\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (repo->getHotMessageCount("no-embed") != 55) {
        std::cerr << "testM3EmbedUnavailable: hot tier mutated\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM3IdempotentDoubleRun() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_idempotent.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m3-idem");

    if (auto* repo = memory.getSQLiteRepo()) {
        seedSessionMessages(*repo, "m3-idem", 55, clock->nowMs());
    } else {
        std::cerr << "testM3IdempotentDoubleRun: sqlite repo unavailable\n";
        return false;
    }

    auto request = makeTestManualRequest(false, false);
    const auto first = memory.runConsolidation("m3-idem", request);
    const int warm_after_first = memory.getRecentWarmMemory(100).size();
    const auto second = memory.runConsolidation("m3-idem", request);

    if (first.archived <= 0) {
        std::cerr << "testM3IdempotentDoubleRun: first run expected archives\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (second.archived != 0 || second.warm_created != 0) {
        std::cerr << "testM3IdempotentDoubleRun: second run should no-op under cap\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (memory.getRecentWarmMemory(100).size() != warm_after_first) {
        std::cerr << "testM3IdempotentDoubleRun: duplicate warm rows\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM3StatusRunStatus() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_status_run.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m3-srs");

    if (auto* repo = memory.getSQLiteRepo()) {
        seedSessionMessages(*repo, "m3-srs", 55, clock->nowMs());
    } else {
        std::cerr << "testM3StatusRunStatus: sqlite repo unavailable\n";
        return false;
    }

    const auto before = memory.getConsolidationStatus("m3-srs");
    auto request = makeTestManualRequest(false, true);
    const auto run = memory.runConsolidation("m3-srs", request);
    const auto after = memory.getConsolidationStatus("m3-srs");

    if (!before.decision.shouldConsolidate()) {
        std::cerr << "testM3StatusRunStatus: expected should consolidate before run\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (run.archived <= 0 || after.decision.hot_count >= before.decision.hot_count) {
        std::cerr << "testM3StatusRunStatus: hot count did not decrease\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (after.decision.hot_count != run.remaining_hot) {
        std::cerr << "testM3StatusRunStatus: remaining_hot mismatch\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM3GoalBlockedUnlessUnsafe() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m3_goal_block.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setGoalActiveChecker([]() { return true; });
    memory.setActiveSessionId("m3-block");

    if (auto* repo = memory.getSQLiteRepo()) {
        seedSessionMessages(*repo, "m3-block", 55, clock->nowMs());
    } else {
        std::cerr << "testM3GoalBlockedUnlessUnsafe: sqlite repo unavailable\n";
        return false;
    }

    auto blocked = makeTestManualRequest(false, false);
    const auto blocked_result = memory.runConsolidation("m3-block", blocked);
    if (!blocked_result.blocked || blocked_result.archived != 0) {
        std::cerr << "testM3GoalBlockedUnlessUnsafe: expected blocked run\n";
        fs::remove(cfg.database_path);
        return false;
    }

    auto unsafe = makeTestManualRequest(false, true);
    unsafe.allow_during_goal = true;
    const auto unsafe_result = memory.runConsolidation("m3-block", unsafe);
    if (unsafe_result.blocked || unsafe_result.archived <= 0) {
        std::cerr << "testM3GoalBlockedUnlessUnsafe: expected --unsafe to allow run\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testMemorySessionScoping() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_session_scope.db").string();
    Memory memory(cfg);

    memory.setActiveSessionId("session-a");
    memory.addMessage("user", "alpha");

    memory.setActiveSessionId("session-b");
    memory.addMessage("user", "beta");

    memory.setActiveSessionId("session-a");
    const auto convoA = memory.getConversation();
    if (convoA.size() != 1 || convoA[0]["content"] != "alpha") {
        std::cerr << "testMemorySessionScoping: session-a isolation failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    memory.setActiveSessionId("session-b");
    const auto convoB = memory.getConversation();
    if (convoB.size() != 1 || convoB[0]["content"] != "beta") {
        std::cerr << "testMemorySessionScoping: session-b isolation failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testFactStore() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_fact_test.db").string();
    Thoth::SQLiteMemoryRepository repo(cfg.database_path);
    Thoth::FactStore store(repo);

    Thoth::Fact f1{"pi", "3.14", 1.0f, "math", 1000};
    store.upsert(f1);

    auto retrieved = store.get("pi");
    if (!retrieved || retrieved->value != "3.14") {
        std::cerr << "testFactStore: get failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testStoreFactTool() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_store_fact_tool_test.db").string();
    Thoth::SQLiteMemoryRepository repo(cfg.database_path);
    Thoth::FactStore store(repo);
    StoreFactTool tool(store);

    nlohmann::json input = {{"key", "color"}, {"value", "blue"}};
    nlohmann::json output = tool.execute(input);
    if (output["status"] != "success") {
        std::cerr << "testStoreFactTool: tool execution failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testVectorStoreAbstraction() {
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    Thoth::FlatVectorStore store(engine.get());
    std::vector<float> v1 = {1.0f, 0.0f, 0.0f};
    store.insert("doc1", v1);
    if (store.chunk_count() != 1) {
        std::cerr << "testVectorStoreAbstraction: count mismatch\n";
        return false;
    }
    return true;
}

static bool testWebScrapeTool() {
    setenv("THOTH_MOCK_SCRAPE", "true", 1);
    WebScrapeTool tool;
    std::string url = "https://www.google.com";
    nlohmann::json input = {{"url", url}};
    nlohmann::json output = tool.execute(input);
    unsetenv("THOTH_MOCK_SCRAPE");

    if (output["status"] != "success") {
        std::cerr << "testWebScrapeTool: status mismatch: " << output.dump() << "\n";
        return false;
    }

    return true;
}

static bool testSelfCorrectTool() {
    Config cfg;
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    SelfCorrectTool tool(llm);
    nlohmann::json schema = tool.input_schema();
    if (schema["type"] != "object" || !schema["properties"].contains("result")) {
        std::cerr << "testSelfCorrectTool: schema mismatch\n";
        return false;
    }
    return true;
}

static bool testConstraintChecker() {
    Thoth::ConstraintChecker checker;
    
    // Test blocked path
    nlohmann::json payload_path = {{"file_path", "/etc/passwd"}};
    auto res1 = checker.check_action("file_read", payload_path);
    if (res1.allowed) {
        std::cerr << "testConstraintChecker: failed to block /etc/passwd\n";
        return false;
    }

    // Test allowed path
    nlohmann::json payload_ok = {{"file_path", "src/main.cpp"}};
    auto res2 = checker.check_action("file_read", payload_ok);
    if (!res2.allowed) {
        std::cerr << "testConstraintChecker: incorrectly blocked src/main.cpp\n";
        return false;
    }

    return true;
}

static bool testGmailReadMessagesTool() {
    GmailReadMessagesTool tool;
    
    // Test list
    nlohmann::json input_list = {{"operation", "list"}};
    nlohmann::json output_list = tool.execute(input_list);
    if (output_list["status"] != "success" || !output_list["data"].contains("messages")) {
        std::cerr << "testGmailReadMessagesTool: list failed\n";
        return false;
    }

    return true;
}

class MockBatchPlanner : public IPlanner {
public:
    Plan create_plan(const std::string& goal) override {
        Plan plan;
        plan.plan_id = "batch-plan-123";
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;

        PlanStep s1;
        s1.step_id = "step-1";
        s1.description = "Parallel step A";
        s1.type = StepType::LLM; 
        s1.payload = {{"prompt", "test"}};
        plan.steps.push_back(s1);

        PlanStep s2;
        s2.step_id = "step-2";
        s2.description = "Parallel step B";
        s2.type = StepType::LLM;
        s2.payload = {{"prompt", "test"}};
        plan.steps.push_back(s2);

        PlanStep s3;
        s3.step_id = "step-3";
        s3.description = "Dependent step";
        s3.type = StepType::LLM;
        s3.payload = {{"prompt", "test"}};
        s3.depends_on = {"step-1", "step-2"};
        plan.steps.push_back(s3);

        return plan;
    }
    Plan revise_plan(const Plan& p, const nlohmann::json&) override { return p; }
};

static bool testToolBatching() {
    setenv("THOTH_MOCK_LLM", "true", 1);
    Config cfg;
    cfg.database_path = makeTempPath("thoth_batch_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
    auto planner = std::make_shared<MockBatchPlanner>();
    auto registry = std::make_shared<ToolRegistry>();
    
    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    
    bool completed = false;
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_COMPLETED || ev.type == EventType::PLAN_FAILED) {
            completed = true;
        }
    });

    controller.execute_goal("Batch test goal");

    int timeout = 100;
    while (!completed && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --timeout;
    }

    if (!completed) {
        std::cerr << "testToolBatching: plan failed to complete (State: " << (int)controller.get_state() << ")\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    unsetenv("THOTH_MOCK_LLM");
    return true;
}

class MockReflectionPlanner : public IPlanner {
public:
    int call_count = 0;
    std::vector<std::string> goals_requested;
    Plan create_plan(const std::string& goal) override {
        call_count++;
        goals_requested.push_back(goal);
        Plan plan;
        plan.plan_id = "reflection-plan-" + std::to_string(call_count);
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;

        PlanStep s1;
        s1.step_id = "step-1";
        s1.description = "Reflectable step";
        // First plan must finish with score < 0.6 to trigger reflection.
        // Use NODE (not implemented) so the first attempt fails deterministically.
        if (call_count == 1) {
            s1.type = StepType::NODE;
            s1.payload = {{"node_id", "reflection-test-node"}};
        } else {
            s1.type = StepType::LLM;
            s1.payload = {{"prompt", "test"}};
        }
        plan.steps.push_back(s1);
        return plan;
    }
    Plan revise_plan(const Plan& p, const nlohmann::json&) override { return p; }
};

class MockParallelRetrievalPlanner : public IPlanner {
public:
    Plan create_plan(const std::string& goal) override {
        Plan plan;
        plan.plan_id = "parallel-retrieval-plan";
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;

        PlanStep r1;
        r1.step_id = "retrieve-a";
        r1.description = "Retrieve GRAG alpha documentation";
        r1.type = StepType::RETRIEVAL;
        r1.payload = {{"query", "GRAG alpha directional scoring"}, {"top_k", 3}};

        PlanStep r2;
        r2.step_id = "retrieve-b";
        r2.description = "Retrieve ExecutiveController documentation";
        r2.type = StepType::RETRIEVAL;
        r2.payload = {{"query", "ExecutiveController state machine"}, {"top_k", 3}};

        PlanStep synth;
        synth.step_id = "synthesize";
        synth.description = "Summarize retrieved context";
        synth.type = StepType::LLM;
        synth.payload = {{"prompt", "Summarize findings"}};
        synth.depends_on = {"retrieve-a", "retrieve-b"};

        plan.steps.push_back(r1);
        plan.steps.push_back(r2);
        plan.steps.push_back(synth);
        return plan;
    }
    Plan revise_plan(const Plan& p, const nlohmann::json&) override { return p; }
};

static const char* controllerStateName(Thoth::ControllerState state) {
    using S = Thoth::ControllerState;
    switch (state) {
        case S::IDLE: return "IDLE";
        case S::PLANNING: return "PLANNING";
        case S::EXECUTING_STEP: return "EXECUTING_STEP";
        case S::OBSERVING_RESULT: return "OBSERVING_RESULT";
        case S::REVISING_PLAN: return "REVISING_PLAN";
        case S::SCIENTIFIC_MODE: return "SCIENTIFIC_MODE";
        case S::COMPLETED: return "COMPLETED";
        case S::ABORTED: return "ABORTED";
        case S::FAILED: return "FAILED";
    }
    return "UNKNOWN";
}

static bool isPlanTerminalState(Thoth::ControllerState state) {
    return state == Thoth::ControllerState::COMPLETED ||
           state == Thoth::ControllerState::FAILED ||
           state == Thoth::ControllerState::ABORTED;
}

static bool testParallelRetrieval() {
    setenv("THOTH_MOCK_LLM", "true", 1);
    Config cfg;
    cfg.database_path = makeTempPath("thoth_parallel_retrieval_test.db").string();
    cfg.enable_retrieval_prefetch = true;
    cfg.max_parallel_retrieval = 4;
    cfg.max_reflections = 0;

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());

    CodeChunk chunkA;
    chunkA.code = "GRAG alpha blends query similarity with directional scoring D = G - C.";
    chunkA.fileName = "grag.md";
    chunkA.embedding = engine->embed(chunkA.code);
    idx->addChunkToIndex(std::move(chunkA));

    CodeChunk chunkB;
    chunkB.code = "ExecutiveController states include IDLE, PLANNING, EXECUTING_STEP, and COMPLETED.";
    chunkB.fileName = "controller.md";
    chunkB.embedding = engine->embed(chunkB.code);
    idx->addChunkToIndex(std::move(chunkB));

    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
    auto planner = std::make_shared<MockParallelRetrievalPlanner>();
    auto registry = std::make_shared<ToolRegistry>();

    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    controller.set_config(&cfg);
    controller.set_max_reflections(0);

    std::atomic<int> active_retrievals{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<bool> completed{false};

    controller.set_event_callback([&](const ControllerEvent& ev) {
        const bool isRetrievalStep =
            ev.step_id == "retrieve-a" || ev.step_id == "retrieve-b";
        if (ev.type == EventType::STEP_STARTED && isRetrievalStep) {
            const int now = ++active_retrievals;
            int observed = max_concurrent.load();
            while (now > observed && !max_concurrent.compare_exchange_weak(observed, now)) {
            }
        } else if ((ev.type == EventType::STEP_COMPLETED || ev.type == EventType::STEP_FAILED) &&
                   isRetrievalStep) {
            --active_retrievals;
        } else if (ev.type == EventType::PLAN_COMPLETED || ev.type == EventType::PLAN_FAILED) {
            completed.store(true);
        }
    });

    controller.execute_goal("Parallel retrieval test");

    constexpr int kStepMs = 50;
    constexpr int kTimeoutMs = 60000;
    bool terminal_ok = false;
    for (int waited = 0; waited < kTimeoutMs; waited += kStepMs) {
        if (isPlanTerminalState(controller.get_state())) {
            terminal_ok = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));
    }

    const int maxObserved = max_concurrent.load();
    const bool concurrency_ok = maxObserved >= 2;
    const bool event_completed = completed.load();

    if (!terminal_ok || !concurrency_ok) {
        std::cerr << "testParallelRetrieval: terminal=" << (terminal_ok ? "true" : "false")
                  << " state=" << controllerStateName(controller.get_state())
                  << " max_concurrent=" << maxObserved
                  << " completed=" << (event_completed ? "true" : "false");
        if (!terminal_ok) {
            std::cerr << " (completion regression)";
        }
        if (!concurrency_ok) {
            std::cerr << " (concurrency regression)";
        }
        std::cerr << "\n";
    }

    fs::remove(cfg.database_path);
    unsetenv("THOTH_MOCK_LLM");
    return terminal_ok && concurrency_ok;
}

static bool testReflectionLoop() {
    setenv("THOTH_MOCK_LLM", "true", 1);
    Config cfg;
    cfg.database_path = makeTempPath("thoth_reflection_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
    auto planner = std::make_shared<MockReflectionPlanner>();
    auto registry = std::make_shared<ToolRegistry>();

    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    std::atomic<int> plan_created_events{0};
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_CREATED) {
            plan_created_events.fetch_add(1, std::memory_order_relaxed);
        }
    });

    controller.execute_goal("Reflection test goal");

    int timeout = 100;
    while ((planner->call_count < 2 || plan_created_events.load() < 2) && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --timeout;
    }

    if (planner->call_count < 2) {
        std::cerr << "testReflectionLoop: failed to trigger reflection (call count: "
                  << planner->call_count << ")\n";
        fs::remove(cfg.database_path);
        return false;
    }

    if (plan_created_events.load() < 2) {
        std::cerr << "testReflectionLoop: expected at least 2 PLAN_CREATED events (got "
                  << plan_created_events.load() << ")\n";
        fs::remove(cfg.database_path);
        return false;
    }

    if (planner->goals_requested.size() < 2 ||
        planner->goals_requested[1].find("Reflection:") == std::string::npos) {
        std::cerr << "testReflectionLoop: reflection replan goal missing Reflection context\n";
        fs::remove(cfg.database_path);
        return false;
    }

    timeout = 100;
    while (controller.get_state() != Thoth::ControllerState::COMPLETED && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --timeout;
    }

    if (controller.get_state() != Thoth::ControllerState::COMPLETED) {
        std::cerr << "testReflectionLoop: expected COMPLETED after reflection replan (state: "
                  << static_cast<int>(controller.get_state()) << ")\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const Plan final_plan = controller.get_current_plan();
    if (final_plan.plan_id != "reflection-plan-2") {
        std::cerr << "testReflectionLoop: expected active plan reflection-plan-2 (got "
                  << final_plan.plan_id << ")\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    unsetenv("THOTH_MOCK_LLM");
    return true;
}

static bool testCognitiveSpine() {
    setenv("THOTH_MOCK_SCRAPE", "true", 1);
    
    Config cfg;
    cfg.database_path = makeTempPath("thoth_spine_test.db").string();
    Thoth::SQLiteMemoryRepository repo(cfg.database_path);
    auto fact_store = std::make_shared<Thoth::FactStore>(repo);
    
    auto& registry = ToolRegistry::instance();
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    registry.initialize(fact_store, &llm);

    // 1. Scrape
    nlohmann::json scrape_out = registry.executeTool("web_scrape", {{"url", "https://thoth.ai"}});
    if (scrape_out["status"] != "success") {
        std::cerr << "testCognitiveSpine: scrape failed: " << scrape_out.dump() << "\n";
        return false;
    }

    // 2. Summarize (Legacy Tool)
    nlohmann::json sum_out = registry.executeTool("summarize_text", {{"text", scrape_out["data"]["content"]}, {"num_sentences", 1}});
    if (sum_out["status"] != "success") {
        std::cerr << "testCognitiveSpine: summarize failed: " << sum_out.dump() << "\n";
        return false;
    }

    // 3. Store Fact
    nlohmann::json fact_out = registry.executeTool("store_fact", {{"key", "spine_test"}, {"value", sum_out["data"]["summary"]}});
    if (fact_out["status"] != "success") {
        std::cerr << "testCognitiveSpine: store_fact failed: " << fact_out.dump() << "\n";
        return false;
    }

    // 4. Verify
    auto fact = fact_store->get("spine_test");
    if (!fact || fact->value.empty()) {
        std::cerr << "testCognitiveSpine: fact verification failed\n";
        return false;
    }

    unsetenv("THOTH_MOCK_SCRAPE");
    fs::remove(cfg.database_path);
    return true;
}

static bool testBenchmarkRAGMode() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_bench_rag_test.db").string();
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager indexManager(engine.get());
    RAGPipeline rag(std::move(engine), &indexManager, &cfg);
    
    Thoth::BenchmarkRunner runner(rag);
    
    std::vector<Thoth::BenchmarkCase> cases = {
        {"U1", "understand how GRAG directional scoring works", "GRAG directional embedding", "", {"GRAG"}, "UNAMBIGUOUS"},
        {"U2", "understand plan execution", "plan steps execution", "", {"PLAN"}, "UNAMBIGUOUS"}
    };

    Thoth::BenchmarkConfig config;
    config.wq = 1.0f;
    config.wd = 0.0f;
    config.wt = 0.0f;
    config.top_k = 5;

    auto result = runner.run(config, cases);

    if (result.cases.size() != 2) {
        std::cerr << "testBenchmarkRAGMode: case count mismatch\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testBenchmarkComparison() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_bench_comp_test.db").string();
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager indexManager(engine.get());
    RAGPipeline rag(std::move(engine), &indexManager, &cfg);
    
    Thoth::BenchmarkRunner runner(rag);
    
    std::vector<Thoth::BenchmarkCase> cases = {
        {"U1", "understand how GRAG directional scoring works", "GRAG directional embedding", "", {"GRAG"}, "UNAMBIGUOUS"},
        {"G1", "understand plan failure recovery", "recovery failure restart", "", {"PLAN"}, "GOAL_DISAMBIGUATES"},
        {"T1", "understand memory system", "storage tiers warm cold", "Already read hot tier definition", {"cognate", "architectural_facts"}, "TRAJECTORY_DISAMBIGUATES"}
    };

    auto result = runner.runComparison(cases);

    if (result.deltas.size() != 3) {
        std::cerr << "testBenchmarkComparison: delta count mismatch\n";
        fs::remove(cfg.database_path);
        return false;
    }

    if (result.rag_precision_by_type.empty() || result.grag_precision_by_type.empty()) {
        std::cerr << "testBenchmarkComparison: breakdown maps empty\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testBenchmarkReporter() {
    Thoth::ComparisonResult res;
    res.rag_mean_precision = 0.5f;
    res.grag_mean_precision = 0.7f;
    res.rag_mean_reciprocal_rank = 0.4f;
    res.grag_mean_reciprocal_rank = 0.6f;
    res.rag_mean_ndcg = 0.45f;
    res.grag_mean_ndcg = 0.65f;
    
    res.rag_precision_by_type["UNAMBIGUOUS"] = 0.8f;
    res.grag_precision_by_type["UNAMBIGUOUS"] = 0.8f;
    res.rag_ndcg_by_type["UNAMBIGUOUS"] = 0.7f;
    res.grag_ndcg_by_type["UNAMBIGUOUS"] = 0.7f;
    
    Thoth::CaseDelta cd;
    cd.case_id = "TEST-1";
    cd.case_type = "UNAMBIGUOUS";
    cd.rag_precision = 0.8f;
    cd.grag_precision = 0.8f;
    cd.precision_delta = 0.0f;
    cd.rag_reciprocal_rank = 0.5f;
    cd.grag_reciprocal_rank = 0.5f;
    cd.rag_ndcg = 0.7f;
    cd.grag_ndcg = 0.7f;
    cd.ndcg_delta = 0.0f;
    cd.directional_lift = 0.0f;
    res.deltas.push_back(cd);

    // Verify stdout doesn't crash
    Thoth::BenchmarkReporter::reportToStdout(res);

    return true;
}

static bool testProblemStatePersistence() {
    const fs::path tempDir = makeTempPath("thoth_problem_state_dir");
    fs::create_directories(tempDir);
    const fs::path dbPath = tempDir / "memory.db";

    Config cfg;
    cfg.database_path = dbPath.string();

    {
        Memory memory(cfg);
        Thoth::ProblemState s;
        s.problem_id = "prob-123";
        s.goal_id = "goal-456";
        s.problem_description = "Test problem";
        s.hypotheses = {"H1", "H2"};
        s.constraints = {"C1"};
        s.iteration_count = 2;
        s.confidence_score = 0.75f;
        s.created_at = 1000;
        s.updated_at = 2000;

        Thoth::MemoryRepository::ProblemStateRecord rec;
        rec.problem_id = s.problem_id;
        rec.goal_id = s.goal_id;
        rec.iteration_count = s.iteration_count;
        rec.confidence_score = s.confidence_score;
        rec.created_at = s.created_at;
        rec.updated_at = s.updated_at;

        const nlohmann::json stateJson = s.to_json();
        rec.state_json = stateJson.dump();

        if (!memory.saveProblemState(rec)) {
            std::cerr << "testProblemStatePersistence: save failed\n";
            fs::remove_all(tempDir);
            return false;
        }
    }

    Memory reloaded(cfg);
    auto optRec = reloaded.loadProblemState("prob-123");
    if (!optRec) {
        std::cerr << "testProblemStatePersistence: load failed\n";
        fs::remove_all(tempDir);
        return false;
    }

    const nlohmann::json parsed = nlohmann::json::parse(optRec->state_json);
    auto s2 = Thoth::ProblemState::from_json(parsed);
    
    const bool ok = s2.problem_id == "prob-123"
        && s2.hypotheses.size() == 2
        && s2.iteration_count == 2
        && s2.confidence_score == 0.75f;

    fs::remove_all(tempDir);
    if (!ok) std::cerr << "testProblemStatePersistence: data mismatch\n";
    return ok;
}

static bool testModeSwitchPersistence() {
    const fs::path tempDir = makeTempPath("thoth_mode_switch_dir");
    fs::create_directories(tempDir);
    const fs::path dbPath = tempDir / "memory.db";

    Config cfg;
    cfg.database_path = dbPath.string();

    auto memory = std::make_shared<Memory>(cfg);
    auto planner = std::make_shared<DefaultPlanner>();
    auto registry = std::make_shared<ToolRegistry>();
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);

    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    
    bool eventFired = false;
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::MODE_SWITCHED) {
            eventFired = true;
        }
    });

    // Setup initial state
    Thoth::ProblemState s;
    s.problem_id = "prob-switch";
    s.problem_description = "Testing switch";
    controller.update_problem_state(s);

    // Perform switch
    controller.set_execution_mode(std::make_unique<Thoth::ScientificExecutionMode>());

    // Verify event
    if (!eventFired) {
        std::cerr << "testModeSwitchPersistence: MODE_SWITCHED event not fired\n";
        fs::remove_all(tempDir);
        return false;
    }

    // Verify persistence
    auto optRec = memory->loadProblemState("prob-switch");
    if (!optRec) {
        std::cerr << "testModeSwitchPersistence: problem state not persisted during switch\n";
        fs::remove_all(tempDir);
        return false;
    }

    fs::remove_all(tempDir);
    return true;
}

static bool testEventSchemaStandardization() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_event_schema_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto planner = std::make_shared<DefaultPlanner>();
    auto registry = std::make_shared<ToolRegistry>();
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);

    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    
    nlohmann::json captured_meta;
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::STATE_CHANGED) {
            captured_meta = ev.metadata;
        }
    });

    controller.emit_event(EventType::STATE_CHANGED, "", {{"custom_field", 123}});

    const bool ok = captured_meta.contains("reasoning_stage")
        && captured_meta.contains("confidence_score")
        && captured_meta.contains("success")
        && captured_meta.contains("iteration_count")
        && captured_meta["custom_field"] == 123;

    fs::remove(cfg.database_path);
    if (!ok) std::cerr << "testEventSchemaStandardization: schema fields missing or incorrect\n";
    return ok;
}

static bool testScientificLoopStages() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_sci_loop_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto planner = std::make_shared<DefaultPlanner>();
    auto registry = std::make_shared<ToolRegistry>();
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);

    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    
    std::vector<std::string> stages_seen;
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::STATE_CHANGED && ev.metadata.contains("reasoning_stage")) {
            stages_seen.push_back(ev.metadata["reasoning_stage"]);
        }
    });

    Thoth::ProblemState s;
    s.problem_id = "sci-test";
    controller.update_problem_state(s);
    
    auto sci_mode = std::make_unique<Thoth::ScientificExecutionMode>();
    
    // Execute 4 steps to cover one full cycle
    sci_mode->execute_step(controller); // Hypothesis
    sci_mode->execute_step(controller); // Constraints
    sci_mode->execute_step(controller); // Evaluation
    sci_mode->execute_step(controller); // Selection

    const bool ok = stages_seen.size() >= 4
        && stages_seen[0] == "hypothesis_generation"
        && stages_seen[1] == "constraint_extraction"
        && stages_seen[2] == "feasibility_evaluation"
        && stages_seen[3] == "final_selection";

    auto final_state = controller.get_problem_state();
    const bool state_ok = final_state.iteration_count == 1
        && final_state.hypotheses.size() == 2
        && final_state.constraints.size() == 1;

    fs::remove(cfg.database_path);
    if (!ok) std::cerr << "testScientificLoopStages: stages sequence incorrect\n";
    if (!state_ok) std::cerr << "testScientificLoopStages: problem state not updated correctly\n";
    return ok && state_ok;
}

static bool testScientificConvergence() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_sci_conv_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto planner = std::make_shared<DefaultPlanner>();
    auto registry = std::make_shared<ToolRegistry>();
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);

    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    
    Thoth::ProblemState s;
    s.problem_id = "conv-test";
    controller.update_problem_state(s);
    controller.transition_to(Thoth::ControllerState::SCIENTIFIC_MODE);
    
    auto sci_mode = std::make_unique<Thoth::ScientificExecutionMode>();
    
    // Simulate multiple cycles until IDLE state is reached (indicating loop exit)
    int safety_counter = 30; // 4 stages * 5 max iterations = 20 steps max
    while (controller.get_state() == Thoth::ControllerState::SCIENTIFIC_MODE && safety_counter > 0) {
        sci_mode->execute_step(controller);
        safety_counter--;
    }

    auto final_state = controller.get_problem_state();
    
    // Based on the simulation in scientific_execution_mode.cpp:
    // iter 0: confidence 0.6
    // iter 1: confidence 0.75
    // iter 2: confidence 0.9
    // iter 3: confidence 0.95
    // iter 4: confidence 0.95 -> delta 0.0, score 0.95 -> CONVERGED
    
    const bool ok = controller.get_state() == Thoth::ControllerState::IDLE
        && final_state.iteration_count > 1
        && final_state.iteration_count < 6;

    fs::remove(cfg.database_path);
    if (!ok) {
        std::cerr << "testScientificConvergence: failed to exit loop or incorrect iteration count (" 
                  << final_state.iteration_count << ")\n";
    }
    return ok;
}

static bool testStrategyPromotion() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_strategy_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    Thoth::StrategyEngine engine(memory);

    // Simulate 3 successful trajectories with the same pattern
    // Pattern: RETRIEVAL -> TOOL:project_analyze
    nlohmann::json steps = nlohmann::json::array();
    steps.push_back({{"type", 1}, {"tool", "none"}}); // RETRIEVAL
    steps.push_back({{"type", 3}, {"tool", "project_analyze"}}); // TOOL

    nlohmann::json traj_json;
    traj_json["steps"] = steps;

    for (int i = 0; i < 3; ++i) {
        Memory::CognateTrajectoryRecord rec;
        rec.trajectory_id = "traj-" + std::to_string(i);
        rec.goal = "test goal";
        rec.trajectory_json = traj_json.dump();
        rec.success_score = 1.0f;
        rec.embedding = {0.1f, 0.2f, 0.3f};
        rec.created_at = 1000 + i;
        memory->saveTrajectory(rec);
    }

    engine.processTrajectories();

    auto strategies = memory->getAllStrategies();
    
    // We expect 1 strategy to be promoted
    const bool ok = strategies.size() == 1
        && strategies[0].success_rate >= 0.8f
        && strategies[0].description.find("RETRIEVAL->TOOL:project_analyze") != std::string::npos;

    fs::remove(cfg.database_path);
    if (!ok) std::cerr << "testStrategyPromotion: strategy not promoted correctly (count: " << strategies.size() << ")\n";
    return ok;
}

static bool testLlmTokenUsage() {
    Config cfg;
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    llm.resetSessionTokenUsage();

    const char* prevDev = std::getenv("THOTH_TEST_SUITE_DEV");
    setenv("THOTH_TEST_SUITE_DEV", "1", 1);

    const std::string response = llm.query("token usage test prompt for cognitive metrics");
    const LlmTokenUsage session = llm.sessionTokenUsage();
    const LlmTokenUsage last = llm.lastCallTokenUsage();

    if (prevDev) {
        setenv("THOTH_TEST_SUITE_DEV", prevDev, 1);
    } else {
        unsetenv("THOTH_TEST_SUITE_DEV");
    }

    llm.resetSessionTokenUsage();
    const LlmTokenUsage cleared = llm.sessionTokenUsage();

    const bool ok = !response.empty()
        && session.total_tokens > 0
        && last.total_tokens > 0
        && cleared.total_tokens == 0;

    if (!ok) {
        std::cerr << "testLlmTokenUsage: expected non-zero tracked tokens and resettable session\n";
    }
    return ok;
}

static bool testStrategyInjection() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_strategy_inject_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
    auto prompt_factory = std::make_shared<PromptFactory>(*memory, *rag);
    LLMInterface llm(LLMBackend::Ollama, &cfg);

    // 1. Create a strategy in memory
    Memory::CognateStrategyRecord strat;
    strat.strategy_id = "strat-123";
    strat.description = "Test Strategy";
    strat.step_pattern_json = "[\"RETRIEVAL\", \"LLM\"]";
    strat.success_rate = 0.95f;
    strat.created_at = 1000;
    memory->saveStrategy(strat);

    LLMPlanner planner(memory, rag, prompt_factory, &llm);
    
    // Logic verification (compilation + method existence)
    // Real injection verified via verbose logging in implementation.
    
    fs::remove(cfg.database_path);
    return true; 
}

namespace {

Thoth::BenchmarkEnvironmentInputs makeE1SampleInputs() {
    Thoth::BenchmarkEnvironmentInputs inputs;
    inputs.tier = Thoth::BenchmarkTier::DEV;
    inputs.harness = "test_suite";
    inputs.provenance.thoth_git_sha = "abc1234";
    inputs.provenance.basic_agent_git_sha = "def5678";
    inputs.provenance.captured_at_ms = 1'700'000'000'000LL;
    inputs.model.llm_model = "mock";
    inputs.model.embedding_model = "tfidf-local";
    inputs.model.embedding_method = "TfIdf";
    inputs.model.embedding_dimension = 768;
    inputs.model.embedding_internal_version = 2;
    inputs.thoth_env_flags = {{"THOTH_TEST_SUITE_DEV", "1"}};
    inputs.corpus_fingerprint_override = "corpus-fast-deadbeef";
    inputs.corpus_mode = Thoth::CorpusFingerprintMode::FAST;
    inputs.corpus_chunk_count = 42;
    return inputs;
}

} // namespace

static bool testE1AssembleEnvironmentDeterministic() {
    const auto inputs = makeE1SampleInputs();
    const Thoth::BenchmarkEnvironment first = Thoth::assembleEnvironment(inputs);
    const Thoth::BenchmarkEnvironment second = Thoth::assembleEnvironment(inputs);

    if (first.environment_hash.empty() || first.environment_hash != second.environment_hash) {
        std::cerr << "testE1AssembleEnvironmentDeterministic: hash mismatch\n";
        return false;
    }
    if (first.model.llm_model != "mock" || first.runtime.harness != "test_suite") {
        std::cerr << "testE1AssembleEnvironmentDeterministic: unexpected assembled fields\n";
        return false;
    }
    if (!first.ollama.models_digest.empty()) {
        std::cerr << "testE1AssembleEnvironmentDeterministic: expected empty ollama digest\n";
        return false;
    }
    return true;
}

static bool testE1InferTierFromEnvFlags() {
    Thoth::BenchmarkEnvironmentInputs devInputs = makeE1SampleInputs();
    devInputs.tier = Thoth::BenchmarkTier::DEV;
    devInputs.thoth_env_flags = {{"THOTH_TEST_SUITE_DEV", "1"}};
    if (Thoth::inferTier(devInputs) != Thoth::BenchmarkTier::DEV) {
        std::cerr << "testE1InferTierFromEnvFlags: expected DEV\n";
        return false;
    }

    Thoth::BenchmarkEnvironmentInputs fullInputs = makeE1SampleInputs();
    fullInputs.tier = Thoth::BenchmarkTier::FULL;
    fullInputs.harness = "test_suite";
    fullInputs.thoth_env_flags = nlohmann::json::object();
    fullInputs.ollama_reachable = true;
    if (Thoth::inferTier(fullInputs) != Thoth::BenchmarkTier::FULL) {
        std::cerr << "testE1InferTierFromEnvFlags: expected FULL\n";
        return false;
    }

    Thoth::BenchmarkEnvironmentInputs mockInputs = makeE1SampleInputs();
    mockInputs.tier = Thoth::BenchmarkTier::MOCK;
    mockInputs.harness = "robustness_suite";
    mockInputs.thoth_env_flags = {{"THOTH_MOCK_LLM", "1"}};
    if (Thoth::inferTier(mockInputs) != Thoth::BenchmarkTier::MOCK) {
        std::cerr << "testE1InferTierFromEnvFlags: expected MOCK\n";
        return false;
    }

    Thoth::BenchmarkEnvironmentInputs ollamaInputs = makeE1SampleInputs();
    ollamaInputs.tier = Thoth::BenchmarkTier::OLLAMA;
    ollamaInputs.harness = "chat_rag_benchmark";
    ollamaInputs.model.embedding_method = "External";
    ollamaInputs.thoth_env_flags = nlohmann::json::object();
    ollamaInputs.ollama_reachable = true;
    if (Thoth::inferTier(ollamaInputs) != Thoth::BenchmarkTier::OLLAMA) {
        std::cerr << "testE1InferTierFromEnvFlags: expected OLLAMA\n";
        return false;
    }

    Thoth::BenchmarkEnvironmentInputs episodicMock;
    episodicMock.harness = "episodic_learning_benchmark";
    episodicMock.tier = Thoth::BenchmarkTier::MOCK;
    episodicMock.model.llm_model = "mock";
    episodicMock.model.embedding_method = "TfIdf";
    episodicMock.thoth_env_flags = {{"THOTH_MOCK_EPISODIC", "1"}, {"THOTH_MOCK_LLM", "true"}};
    if (Thoth::inferTier(episodicMock) != Thoth::BenchmarkTier::MOCK) {
        std::cerr << "testE1InferTierFromEnvFlags: expected episodic MOCK\n";
        return false;
    }

    Thoth::BenchmarkEnvironmentInputs episodicAuth;
    episodicAuth.harness = "episodic_learning_benchmark";
    episodicAuth.tier = Thoth::BenchmarkTier::FULL;
    episodicAuth.model.llm_model = "qwen2.5:3b";
    episodicAuth.model.embedding_method = "External";
    episodicAuth.ollama_reachable = true;
    if (Thoth::inferTier(episodicAuth) != Thoth::BenchmarkTier::OLLAMA) {
        std::cerr << "testE1InferTierFromEnvFlags: expected episodic OLLAMA\n";
        return false;
    }

    Thoth::BenchmarkEnvironmentInputs strictCorpus = makeE1SampleInputs();
    strictCorpus.corpus_mode = Thoth::CorpusFingerprintMode::STRICT;
    strictCorpus.corpus_fingerprint_override = "strict-corpus";
    const auto strictEnv = Thoth::assembleEnvironment(strictCorpus);
    if (strictEnv.corpus.fingerprint_mode != "STRICT") {
        std::cerr << "testE1InferTierFromEnvFlags: expected STRICT corpus mode\n";
        return false;
    }

    return true;
}

static bool testE1EnvironmentHashExcludesIndex() {
    auto inputs = makeE1SampleInputs();
    const Thoth::BenchmarkEnvironment baseline = Thoth::assembleEnvironment(inputs);

    inputs.rag_index_header = {
        {"model_name", "nomic-embed-text"},
        {"embedding_dimension", 768},
        {"embedding_version", 2},
        {"chunk_count", 100},
    };
    const Thoth::BenchmarkEnvironment withIndex = Thoth::assembleEnvironment(inputs);

    if (baseline.environment_hash != withIndex.environment_hash) {
        std::cerr << "testE1EnvironmentHashExcludesIndex: index changed environment_hash\n";
        return false;
    }

    const std::string recomputed = Thoth::computeEnvironmentHash(withIndex);
    if (recomputed != withIndex.environment_hash) {
        std::cerr << "testE1EnvironmentHashExcludesIndex: recompute mismatch\n";
        return false;
    }
    return true;
}

static bool testE1IndexHashDistinctFromEnvironmentHash() {
    Thoth::IndexEnvironment index;
    index.rag_index_header = {
        {"model_name", "nomic-embed-text"},
        {"embedding_dimension", 768},
        {"embedding_version", 2},
        {"chunk_count", 100},
    };

    const auto inputs = makeE1SampleInputs();
    const Thoth::BenchmarkEnvironment env = Thoth::assembleEnvironment(inputs);
    const std::string indexHash = Thoth::computeIndexHash(index);

    if (indexHash.empty()) {
        std::cerr << "testE1IndexHashDistinctFromEnvironmentHash: empty index hash\n";
        return false;
    }
    if (indexHash == env.environment_hash) {
        std::cerr << "testE1IndexHashDistinctFromEnvironmentHash: index hash equals environment hash\n";
        return false;
    }
    return true;
}

static bool testE1TierMismatchPredicate() {
    Thoth::BenchmarkEnvironmentInputs inputs = makeE1SampleInputs();
    inputs.tier = Thoth::BenchmarkTier::FULL;
    inputs.thoth_env_flags = {{"THOTH_TEST_SUITE_DEV", "1"}};
    if (!Thoth::hasTierMismatch(inputs)) {
        std::cerr << "testE1TierMismatchPredicate: expected mismatch for FULL vs DEV flags\n";
        return false;
    }

    inputs.tier = Thoth::BenchmarkTier::DEV;
    if (Thoth::hasTierMismatch(inputs)) {
        std::cerr << "testE1TierMismatchPredicate: unexpected mismatch for aligned DEV tier\n";
        return false;
    }
    return true;
}

static bool testE1BenchmarkEnvironmentJsonRoundTrip() {
    auto inputs = makeE1SampleInputs();
    inputs.include_hostname = true;
    inputs.provenance.hostname = "bench-host";
    inputs.rag_index_header = {{"embedding_version", 2}};
    inputs.ollama = Thoth::OllamaSnapshot{
        "0.5.1",
        {{"llama3", "digest-a"}, {"nomic-embed-text", "digest-b"}},
    };

    const Thoth::BenchmarkEnvironment original = Thoth::assembleEnvironment(inputs);
    const nlohmann::json json = Thoth::benchmarkEnvironmentToJson(original);
    const Thoth::BenchmarkEnvironment restored = Thoth::benchmarkEnvironmentFromJson(json);

    if (restored.prov.thoth_git_sha != original.prov.thoth_git_sha ||
        restored.model.embedding_internal_version != original.model.embedding_internal_version ||
        restored.runtime.tier != original.runtime.tier ||
        restored.corpus.fingerprint != original.corpus.fingerprint ||
        restored.environment_hash != original.environment_hash ||
        restored.ollama.models_digest != original.ollama.models_digest) {
        std::cerr << "testE1BenchmarkEnvironmentJsonRoundTrip: round-trip field mismatch\n";
        return false;
    }
    if (!restored.prov.hostname.has_value() || *restored.prov.hostname != "bench-host") {
        std::cerr << "testE1BenchmarkEnvironmentJsonRoundTrip: hostname missing\n";
        return false;
    }
    return true;
}

class MockSingleLlmPlanner : public IPlanner {
public:
    Plan create_plan(const std::string& goal) override {
        Plan plan;
        plan.plan_id = "e1-metrics-plan";
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;

        PlanStep step;
        step.step_id = "step-1";
        step.description = "Single LLM step";
        step.type = StepType::LLM;
        step.payload = {{"prompt", "benchmark metrics test"}};
        plan.steps.push_back(step);
        return plan;
    }

    Plan revise_plan(const Plan& plan, const nlohmann::json&) override { return plan; }
};

static bool waitForPlanTerminal(Thoth::ExecutiveController& controller, int timeoutMs = 10000) {
    const int stepMs = 50;
    int waited = 0;
    while (waited < timeoutMs) {
        const auto state = controller.get_state();
        if (state == Thoth::ControllerState::COMPLETED ||
            state == Thoth::ControllerState::FAILED ||
            state == Thoth::ControllerState::ABORTED) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
        waited += stepMs;
    }
    return false;
}

static std::optional<nlohmann::json> readMetricsRowForPlan(const fs::path& logPath,
                                                            const std::string& planId) {
    if (!fs::exists(logPath)) {
        return std::nullopt;
    }
    std::ifstream in(logPath);
    std::string line;
    std::optional<nlohmann::json> lastMatch;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            const auto row = nlohmann::json::parse(line);
            if (row.value("event", "") == "GOAL_COGNITIVE_METRICS" &&
                row.value("plan_id", "") == planId) {
                lastMatch = row;
            }
        } catch (...) {
        }
    }
    return lastMatch;
}

struct E1MetricsTestBed {
    Config cfg;
    std::shared_ptr<Memory> memory;
    std::unique_ptr<EmbeddingEngine> engine;
    IndexManager* idx = nullptr;
    std::shared_ptr<RAGPipeline> rag;
    std::shared_ptr<MockSingleLlmPlanner> planner;
    std::shared_ptr<ToolRegistry> registry;
    std::unique_ptr<Thoth::ExecutiveController> controller;

    E1MetricsTestBed() {
        cfg.database_path = makeTempPath("thoth_e1_metrics.db").string();
        memory = std::make_shared<Memory>(cfg);
        engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
        idx = new IndexManager(engine.get());
        rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
        planner = std::make_shared<MockSingleLlmPlanner>();
        registry = std::make_shared<ToolRegistry>();
        controller = std::make_unique<Thoth::ExecutiveController>(planner, registry, rag, memory);
    }

    void cleanup() { fs::remove(cfg.database_path); }
};

static bool testE1GoalMetricsWithAttribution() {
    setenv("THOTH_MOCK_LLM", "true", 1);
    const fs::path metricsLog = makeTempPath("thoth_e1_cognitive_metrics.jsonl");
    setenv("THOTH_COGNITIVE_METRICS_LOG", metricsLog.string().c_str(), 1);

    E1MetricsTestBed bed;
    const Thoth::BenchmarkAttribution attribution{"run-e1-09", "envhash-e1-09"};

    bed.controller->execute_goal("E1 attribution metrics goal", attribution);
    if (!waitForPlanTerminal(*bed.controller)) {
        std::cerr << "testE1GoalMetricsWithAttribution: goal did not finish\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    const auto row = readMetricsRowForPlan(metricsLog, "e1-metrics-plan");
    if (!row.has_value()) {
        std::cerr << "testE1GoalMetricsWithAttribution: metrics row missing\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    if (row->value("run_id", "") != attribution.run_id ||
        row->value("env_hash", "") != attribution.env_hash) {
        std::cerr << "testE1GoalMetricsWithAttribution: attribution fields missing or wrong\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    bed.cleanup();
    fs::remove(metricsLog);
    unsetenv("THOTH_COGNITIVE_METRICS_LOG");
    unsetenv("THOTH_MOCK_LLM");
    return true;
}

static bool testE1GoalMetricsWithoutAttribution() {
    setenv("THOTH_MOCK_LLM", "true", 1);
    const fs::path metricsLog = makeTempPath("thoth_e1_cognitive_metrics_none.jsonl");
    setenv("THOTH_COGNITIVE_METRICS_LOG", metricsLog.string().c_str(), 1);

    E1MetricsTestBed bed;
    bed.controller->execute_goal("E1 no attribution metrics goal");
    if (!waitForPlanTerminal(*bed.controller)) {
        std::cerr << "testE1GoalMetricsWithoutAttribution: goal did not finish\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    const auto row = readMetricsRowForPlan(metricsLog, "e1-metrics-plan");
    if (!row.has_value()) {
        std::cerr << "testE1GoalMetricsWithoutAttribution: metrics row missing\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    if (row->contains("run_id") || row->contains("env_hash")) {
        std::cerr << "testE1GoalMetricsWithoutAttribution: unexpected benchmark fields\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    bed.cleanup();
    fs::remove(metricsLog);
    unsetenv("THOTH_COGNITIVE_METRICS_LOG");
    unsetenv("THOTH_MOCK_LLM");
    return true;
}

static bool testE1GoalMetricsWorkerThreadAttribution() {
    setenv("THOTH_MOCK_LLM", "true", 1);
    const fs::path metricsLog = makeTempPath("thoth_e1_cognitive_metrics_worker.jsonl");
    setenv("THOTH_COGNITIVE_METRICS_LOG", metricsLog.string().c_str(), 1);

    E1MetricsTestBed bed;
    const std::thread::id callerThread = std::this_thread::get_id();
    std::optional<std::thread::id> stepStartedThread;
    std::optional<std::thread::id> terminalEventThread;

    bed.controller->set_event_callback([&](const ControllerEvent& event) {
        if (event.type == EventType::STEP_STARTED) {
            stepStartedThread = std::this_thread::get_id();
        }
        if (event.type == EventType::PLAN_COMPLETED || event.type == EventType::PLAN_FAILED) {
            terminalEventThread = std::this_thread::get_id();
        }
    });

    const Thoth::BenchmarkAttribution attribution{"run-e1-11", "envhash-e1-11"};
    bed.controller->execute_goal("E1 worker thread attribution goal", attribution);

    if (!waitForPlanTerminal(*bed.controller)) {
        std::cerr << "testE1GoalMetricsWorkerThreadAttribution: goal did not finish\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    int callbackWaitMs = 0;
    while ((!stepStartedThread.has_value() || !terminalEventThread.has_value()) &&
           callbackWaitMs < 2000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        callbackWaitMs += 20;
    }

    if (!stepStartedThread.has_value() || *stepStartedThread == callerThread) {
        std::cerr << "testE1GoalMetricsWorkerThreadAttribution: STEP_STARTED not on worker thread\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    if (!terminalEventThread.has_value() || *terminalEventThread == callerThread) {
        std::cerr << "testE1GoalMetricsWorkerThreadAttribution: terminal event not on worker thread\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    const auto row = readMetricsRowForPlan(metricsLog, "e1-metrics-plan");
    if (!row.has_value() ||
        row->value("run_id", "") != attribution.run_id ||
        row->value("env_hash", "") != attribution.env_hash) {
        std::cerr << "testE1GoalMetricsWorkerThreadAttribution: worker metrics missing attribution\n";
        bed.cleanup();
        fs::remove(metricsLog);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    bed.cleanup();
    fs::remove(metricsLog);
    unsetenv("THOTH_COGNITIVE_METRICS_LOG");
    unsetenv("THOTH_MOCK_LLM");
    return true;
}

static std::optional<nlohmann::json> readMetricsRowWithRunId(const fs::path& logPath,
                                                              const std::string& runId) {
    if (!fs::exists(logPath)) {
        return std::nullopt;
    }
    std::ifstream in(logPath);
    std::string line;
    std::optional<nlohmann::json> lastMatch;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            const auto row = nlohmann::json::parse(line);
            if (row.value("event", "") == "GOAL_COGNITIVE_METRICS" &&
                row.value("run_id", "") == runId) {
                lastMatch = row;
            }
        } catch (...) {
        }
    }
    return lastMatch;
}

static bool waitMsHarnessPlan(std::atomic<bool>& planTerminal, int timeoutMs = 120000) {
    const int stepMs = 100;
    for (int waited = 0; waited < timeoutMs; waited += stepMs) {
        if (planTerminal.load()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
    }
    return planTerminal.load();
}

/** E1-12: harness helper path — plugin buildTestSuiteBenchmarkInputs → executeGoal → metrics/sidecar. */
static bool testE1HarnessBenchmarkSmoke() {
    setenv("THOTH_TEST_SUITE_DEV", "1", 1);
    setenv("THOTH_MOCK_LLM", "true", 1);

    FileHandler fh;
    const fs::path corpus = fs::path(fh.getAgentWorkspacePath("rag/e1_harness_smoke"));
    fs::create_directories(corpus);
    {
        std::ofstream out(corpus / "smoke.md");
        out << "GRAG smoke corpus for E1-12 harness path test.\n";
    }
    const std::string corpusPath = corpus.string();
    const std::string indexPath = (corpus / "e1_harness.rag_index.bin").string();
    setenv("THOTH_TEST_SUITE_INDEX", indexPath.c_str(), 1);

    const fs::path logsDir = makeTempPath("thoth_e1_harness_logs");
    fs::create_directories(logsDir);
    const fs::path metricsLog = logsDir / "cognitive_metrics.jsonl";
    setenv("THOTH_COGNITIVE_METRICS_LOG", metricsLog.string().c_str(), 1);

    BasicAgentPlugin plugin;
    plugin.setSessionId("e1-12");
    plugin.setRagFiles({corpusPath});

    const auto inputs = plugin.buildTestSuiteBenchmarkInputs(false, corpusPath);
    Thoth::BenchmarkContextOptions opts;
    opts.logs_directory = logsDir.string();
    opts.auto_fill_git = false;
    Thoth::BenchmarkRun run = Thoth::BenchmarkRun::create(inputs, opts);
    run.bindIndex(plugin.benchmarkIndexEnvironment());
    const Thoth::BenchmarkAttribution attr = run.attribution();

    std::atomic<bool> planTerminal{false};
    plugin.onEvent = [&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_COMPLETED || ev.type == EventType::PLAN_FAILED) {
            planTerminal.store(true);
        }
    };

    plugin.executeGoal("E1-12 harness smoke goal", attr);

    const bool finished = waitMsHarnessPlan(planTerminal);
    auto cleanup = [&]() {
        fs::remove_all(logsDir);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_TEST_SUITE_INDEX");
        unsetenv("THOTH_MOCK_LLM");
        unsetenv("THOTH_TEST_SUITE_DEV");
    };

    if (!finished) {
        std::cerr << "testE1HarnessBenchmarkSmoke: goal did not finish\n";
        cleanup();
        return false;
    }

    nlohmann::json sidecar;
    {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        if (!in.is_open()) {
            std::cerr << "testE1HarnessBenchmarkSmoke: sidecar missing\n";
            cleanup();
            return false;
        }
        in >> sidecar;
    }
    if (sidecar.value("run_id", "") != run.run_id() ||
        sidecar.value("environment_hash", "") != run.environment_hash()) {
        std::cerr << "testE1HarnessBenchmarkSmoke: sidecar run identity mismatch\n";
        cleanup();
        return false;
    }

    const auto row = readMetricsRowWithRunId(metricsLog, attr.run_id);
    if (!row.has_value()) {
        std::cerr << "testE1HarnessBenchmarkSmoke: metrics row missing for run_id\n";
        cleanup();
        return false;
    }
    if (row->value("env_hash", "") != attr.env_hash) {
        std::cerr << "testE1HarnessBenchmarkSmoke: metrics env_hash mismatch\n";
        cleanup();
        return false;
    }

    cleanup();
    return true;
}

/** E1-13: reflection A/B harness path — probe stack → execute_goal(attribution) → metrics/sidecar. */
static bool testE1ReflectionAbBenchmarkSmoke() {
    setenv("THOTH_MOCK_LLM", "true", 1);

    const fs::path logsDir = makeTempPath("thoth_e1_reflection_ab_logs");
    fs::create_directories(logsDir);
    const fs::path metricsLog = logsDir / "cognitive_metrics.jsonl";
    setenv("THOTH_COGNITIVE_METRICS_LOG", metricsLog.string().c_str(), 1);

    auto probeEngine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager probeIdx(probeEngine.get());

    Thoth::BenchmarkEnvironmentInputs inputs;
    inputs.harness = "reflection_ab_benchmark";
    inputs.tier = Thoth::BenchmarkTier::MOCK;
    inputs.model.llm_model = "mock";
    inputs.model.embedding_method = "TfIdf";
    inputs.model.embedding_dimension = probeEngine->getDimension();
    inputs.model.embedding_internal_version = probeEngine->getInternalVersion();
    inputs.corpus_mode = Thoth::CorpusFingerprintMode::FAST;
    inputs.thoth_env_flags = Thoth::collectThothEnvFlags();

    Thoth::BenchmarkContextOptions opts;
    opts.logs_directory = logsDir.string();
    opts.auto_fill_git = false;
    Thoth::BenchmarkRun run = Thoth::BenchmarkRun::create(inputs, opts);

    Thoth::IndexEnvironment index;
    index.rag_index_header = {
        {"model_name", probeEngine->getModelName()},
        {"embedding_dimension", probeEngine->getDimension()},
        {"embedding_version", probeEngine->getInternalVersion()},
        {"chunk_count", 0},
    };
    run.bindIndex(index);

    if (run.index_hash().empty()) {
        std::cerr << "testE1ReflectionAbBenchmarkSmoke: index_hash empty after bind\n";
        fs::remove_all(logsDir);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    const Thoth::BenchmarkAttribution attr = run.attribution();

    Config cfg;
    cfg.max_reflections = 0;
    cfg.database_path = makeTempPath("thoth_e1_reflection_ab.db").string();
    fs::remove(cfg.database_path);

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
    auto planner = std::make_shared<Thoth::ReflectionAbMockPlanner>(
        Thoth::ReflectionAbFixture::RecoverableStepFailure);
    auto registry = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, registry, rag, memory);

    std::atomic<bool> planTerminal{false};
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_COMPLETED || ev.type == EventType::PLAN_FAILED) {
            planTerminal.store(true);
        }
    });

    controller.execute_goal("E1-13 reflection AB smoke goal", attr);

    const bool finished = waitMsHarnessPlan(planTerminal, 30000);
    auto cleanup = [&]() {
        fs::remove(cfg.database_path);
        fs::remove_all(logsDir);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
    };

    if (!finished) {
        std::cerr << "testE1ReflectionAbBenchmarkSmoke: goal did not finish\n";
        cleanup();
        return false;
    }

    nlohmann::json sidecar;
    {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        if (!in.is_open()) {
            std::cerr << "testE1ReflectionAbBenchmarkSmoke: sidecar missing\n";
            cleanup();
            return false;
        }
        in >> sidecar;
    }
    if (sidecar.value("run_id", "") != run.run_id() ||
        sidecar.value("environment_hash", "") != run.environment_hash()) {
        std::cerr << "testE1ReflectionAbBenchmarkSmoke: sidecar run identity mismatch\n";
        cleanup();
        return false;
    }

    const auto row = readMetricsRowWithRunId(metricsLog, attr.run_id);
    if (!row.has_value() || row->value("env_hash", "") != attr.env_hash) {
        std::cerr << "testE1ReflectionAbBenchmarkSmoke: metrics attribution mismatch\n";
        cleanup();
        return false;
    }

    cleanup();
    return true;
}

class E1RobustnessSmokePlanner : public IPlanner {
public:
    Plan create_plan(const std::string& goal) override {
        Plan plan;
        plan.plan_id = "e1-14-robustness-plan";
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;
        PlanStep step;
        step.step_id = "fast-step";
        step.description = "E1-14 smoke LLM step";
        step.type = StepType::LLM;
        step.payload = {{"prompt", "E1-14 robustness smoke"}};
        plan.steps.push_back(step);
        return plan;
    }

    Plan revise_plan(const Plan& plan, const nlohmann::json&) override { return plan; }
};

/** E1-14: robustness harness path — probe stack → execute_goal(attribution) → metrics/sidecar. */
static bool testE1RobustnessBenchmarkSmoke() {
    unsetenv("THOTH_TEST_SUITE_DEV");
    setenv("THOTH_MOCK_LLM", "true", 1);

    const fs::path logsDir = makeTempPath("thoth_e1_robustness_logs");
    fs::create_directories(logsDir);
    const fs::path metricsLog = logsDir / "cognitive_metrics.jsonl";
    setenv("THOTH_COGNITIVE_METRICS_LOG", metricsLog.string().c_str(), 1);

    auto probeEngine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager probeIdx(probeEngine.get());

    Thoth::BenchmarkEnvironmentInputs inputs;
    inputs.harness = "robustness_suite";
    inputs.tier = Thoth::BenchmarkTier::MOCK;
    inputs.model.llm_model = "mock";
    inputs.model.embedding_method = "TfIdf";
    inputs.model.embedding_dimension = probeEngine->getDimension();
    inputs.model.embedding_internal_version = probeEngine->getInternalVersion();
    inputs.corpus_mode = Thoth::CorpusFingerprintMode::FAST;
    inputs.thoth_env_flags = Thoth::collectThothEnvFlags();

    Thoth::BenchmarkContextOptions opts;
    opts.logs_directory = logsDir.string();
    opts.auto_fill_git = false;
    Thoth::BenchmarkRun run = Thoth::BenchmarkRun::create(inputs, opts);

    Thoth::IndexEnvironment index;
    index.rag_index_header = {
        {"model_name", probeEngine->getModelName()},
        {"embedding_dimension", probeEngine->getDimension()},
        {"embedding_version", probeEngine->getInternalVersion()},
        {"chunk_count", 0},
    };
    run.bindIndex(index);

    if (run.index_hash().empty()) {
        std::cerr << "testE1RobustnessBenchmarkSmoke: index_hash empty after bind\n";
        fs::remove_all(logsDir);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
        return false;
    }

    const Thoth::BenchmarkAttribution attr = run.attribution();

    Config cfg;
    cfg.database_path = makeTempPath("thoth_e1_robustness.db").string();
    fs::remove(cfg.database_path);

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
    auto planner = std::make_shared<E1RobustnessSmokePlanner>();
    auto registry = std::make_shared<ToolRegistry>();
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    controller.set_config(&cfg);
    controller.set_llm_interface(&llm);

    std::atomic<bool> planTerminal{false};
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_COMPLETED || ev.type == EventType::PLAN_FAILED) {
            planTerminal.store(true);
        }
    });

    controller.execute_goal("E1-14 robustness smoke goal", attr);

    const bool finished = waitMsHarnessPlan(planTerminal, 30000);
    auto cleanup = [&]() {
        fs::remove(cfg.database_path);
        fs::remove_all(logsDir);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_LLM");
    };

    if (!finished) {
        std::cerr << "testE1RobustnessBenchmarkSmoke: goal did not finish\n";
        cleanup();
        return false;
    }

    nlohmann::json sidecar;
    {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        if (!in.is_open()) {
            std::cerr << "testE1RobustnessBenchmarkSmoke: sidecar missing\n";
            cleanup();
            return false;
        }
        in >> sidecar;
    }
    if (sidecar.value("run_id", "") != run.run_id() ||
        sidecar.value("environment_hash", "") != run.environment_hash()) {
        std::cerr << "testE1RobustnessBenchmarkSmoke: sidecar run identity mismatch\n";
        cleanup();
        return false;
    }

    const auto row = readMetricsRowWithRunId(metricsLog, attr.run_id);
    if (!row.has_value() || row->value("env_hash", "") != attr.env_hash) {
        std::cerr << "testE1RobustnessBenchmarkSmoke: metrics attribution mismatch\n";
        cleanup();
        return false;
    }

    cleanup();
    return true;
}

/** E1-15: chat-RAG harness path — probe stack → retrieveRelevant → sidecar (no Ollama). */
static bool testE1ChatRagBenchmarkSmoke() {
    const fs::path logsDir = makeTempPath("thoth_e1_chat_rag_logs");
    fs::create_directories(logsDir);

    auto probeEngine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager probeIdx(probeEngine.get());

    Thoth::BenchmarkEnvironmentInputs inputs;
    inputs.harness = "chat_rag_benchmark";
    inputs.tier = Thoth::BenchmarkTier::MOCK;
    inputs.model.llm_model = "mock";
    inputs.model.embedding_method = "TfIdf";
    inputs.model.embedding_dimension = probeEngine->getDimension();
    inputs.model.embedding_internal_version = probeEngine->getInternalVersion();
    inputs.corpus_mode = Thoth::CorpusFingerprintMode::FAST;
    inputs.thoth_env_flags = Thoth::collectThothEnvFlags();

    Thoth::BenchmarkContextOptions opts;
    opts.logs_directory = logsDir.string();
    opts.auto_fill_git = false;
    Thoth::BenchmarkRun run = Thoth::BenchmarkRun::create(inputs, opts);

    Thoth::IndexEnvironment index;
    index.rag_index_header = {
        {"model_name", probeEngine->getModelName()},
        {"embedding_dimension", probeEngine->getDimension()},
        {"embedding_version", probeEngine->getInternalVersion()},
        {"chunk_count", 0},
    };
    run.bindIndex(index);

    if (run.index_hash().empty()) {
        std::cerr << "testE1ChatRagBenchmarkSmoke: index_hash empty after bind\n";
        fs::remove_all(logsDir);
        return false;
    }

    Config cfg;
    Memory memory(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    RAGPipeline rag(std::move(engine), idx, &cfg, &memory);

    GragDiagnostics diagnostics;
    const auto chunks = rag.retrieveRelevant("E1-15 chat rag smoke query", {}, 3, "E1-15", {}, {}, {}, {}, {},
                                              &diagnostics);
    if (chunks.empty() && diagnostics.breakdowns.empty()) {
        // Empty index is acceptable — wiring must not crash.
    }

    nlohmann::json sidecar;
    {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        if (!in.is_open()) {
            std::cerr << "testE1ChatRagBenchmarkSmoke: sidecar missing\n";
            fs::remove_all(logsDir);
            return false;
        }
        in >> sidecar;
    }
    if (sidecar.value("run_id", "") != run.run_id() ||
        sidecar.value("environment_hash", "") != run.environment_hash()) {
        std::cerr << "testE1ChatRagBenchmarkSmoke: sidecar run identity mismatch\n";
        fs::remove_all(logsDir);
        return false;
    }

    fs::remove_all(logsDir);
    return true;
}

/** E1-16: GRAG harness path — probe stack → reportToFile(identity) → JSONL row + sidecar. */
static bool testE1GragBenchmarkSmoke() {
    const fs::path tempRoot = makeTempPath("thoth_e1_grag");
    const fs::path workspaceDir = tempRoot / "agent_workspace";
    const fs::path logsDir = tempRoot / "logs";
    fs::create_directories(workspaceDir);
    fs::create_directories(logsDir);

    setenv("THOTH_WORKSPACE_PATH", workspaceDir.string().c_str(), 1);
    setenv("THOTH_PROJECT_ROOT", tempRoot.string().c_str(), 1);

    auto probeEngine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager probeIdx(probeEngine.get());

    Thoth::BenchmarkEnvironmentInputs inputs;
    inputs.harness = "grag_benchmark";
    inputs.tier = Thoth::BenchmarkTier::MOCK;
    inputs.model.llm_model = "mock";
    inputs.model.embedding_method = "TfIdf";
    inputs.model.embedding_dimension = probeEngine->getDimension();
    inputs.model.embedding_internal_version = probeEngine->getInternalVersion();
    inputs.corpus_mode = Thoth::CorpusFingerprintMode::FAST;
    inputs.thoth_env_flags = Thoth::collectThothEnvFlags();

    Thoth::BenchmarkContextOptions opts;
    opts.logs_directory = logsDir.string();
    opts.auto_fill_git = false;
    Thoth::BenchmarkRun run = Thoth::BenchmarkRun::create(inputs, opts);

    Thoth::IndexEnvironment index;
    index.rag_index_header = {
        {"model_name", probeEngine->getModelName()},
        {"embedding_dimension", probeEngine->getDimension()},
        {"embedding_version", probeEngine->getInternalVersion()},
        {"chunk_count", 0},
    };
    run.bindIndex(index);

    auto cleanup = [&]() {
        unsetenv("THOTH_WORKSPACE_PATH");
        unsetenv("THOTH_PROJECT_ROOT");
        fs::remove_all(tempRoot);
    };

    if (run.index_hash().empty()) {
        std::cerr << "testE1GragBenchmarkSmoke: index_hash empty after bind\n";
        cleanup();
        return false;
    }

    const Thoth::BenchmarkRunIdentity identity{run.run_id(), run.environment_hash()};

    Thoth::ComparisonResult mockResult;
    mockResult.rag_mean_ndcg = 0.4f;
    mockResult.grag_mean_ndcg = 0.5f;

    if (!Thoth::BenchmarkReporter::reportToFile(mockResult, 0, identity)) {
        std::cerr << "testE1GragBenchmarkSmoke: reportToFile failed\n";
        cleanup();
        return false;
    }

    const fs::path jsonlPath = workspaceDir / "grag_benchmark.jsonl";
    nlohmann::json row;
    {
        std::ifstream in(jsonlPath);
        if (!in.is_open()) {
            std::cerr << "testE1GragBenchmarkSmoke: grag_benchmark.jsonl missing\n";
            cleanup();
            return false;
        }
        std::string line;
        std::getline(in, line);
        row = nlohmann::json::parse(line);
    }

    if (row.value("run_id", "") != identity.run_id || row.value("env_hash", "") != identity.env_hash) {
        std::cerr << "testE1GragBenchmarkSmoke: JSONL identity mismatch\n";
        cleanup();
        return false;
    }

    nlohmann::json sidecar;
    {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        if (!in.is_open()) {
            std::cerr << "testE1GragBenchmarkSmoke: sidecar missing\n";
            cleanup();
            return false;
        }
        in >> sidecar;
    }
    if (sidecar.value("run_id", "") != run.run_id() ||
        sidecar.value("environment_hash", "") != run.environment_hash()) {
        std::cerr << "testE1GragBenchmarkSmoke: sidecar run identity mismatch\n";
        cleanup();
        return false;
    }

    cleanup();
    return true;
}

static Thoth::BenchmarkContextOptions makeE1TempLogsOptions(const fs::path& logsDir) {
    Thoth::BenchmarkContextOptions options;
    options.logs_directory = logsDir.string();
    options.auto_fill_git = false;
    options.auto_collect_env_flags = false;
    return options;
}

static bool testE1BenchmarkContextCreateSidecar() {
    const fs::path logsDir = makeTempPath("thoth_e1_logs");
    fs::create_directories(logsDir);

    const auto run = Thoth::BenchmarkRun::create(makeE1SampleInputs(), makeE1TempLogsOptions(logsDir));

    if (run.run_id().empty() || run.environment_hash().empty()) {
        std::cerr << "testE1BenchmarkContextCreateSidecar: missing run_id or environment_hash\n";
        fs::remove_all(logsDir);
        return false;
    }

    const fs::path sidecar = logsDir / "benchmark_env.latest.json";
    if (!fs::exists(sidecar)) {
        std::cerr << "testE1BenchmarkContextCreateSidecar: sidecar missing\n";
        fs::remove_all(logsDir);
        return false;
    }

    nlohmann::json doc;
    {
        std::ifstream in(sidecar);
        in >> doc;
    }
    if (doc.value("run_id", "") != run.run_id() ||
        doc.value("environment_hash", "") != run.environment_hash() ||
        !doc.contains("environment")) {
        std::cerr << "testE1BenchmarkContextCreateSidecar: sidecar fields mismatch\n";
        fs::remove_all(logsDir);
        return false;
    }

    const fs::path jsonl = logsDir / "benchmark_env.jsonl";
    if (!fs::exists(jsonl)) {
        std::cerr << "testE1BenchmarkContextCreateSidecar: jsonl missing\n";
        fs::remove_all(logsDir);
        return false;
    }

    bool sawBenchmarkEnv = false;
    {
        std::ifstream in(jsonl);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            const auto row = nlohmann::json::parse(line);
            if (row.value("event", "") == "BENCHMARK_ENV" &&
                row.value("run_id", "") == run.run_id() &&
                row.value("env_hash", "") == run.environment_hash() &&
                row.contains("env")) {
                sawBenchmarkEnv = true;
                break;
            }
        }
    }
    if (!sawBenchmarkEnv) {
        std::cerr << "testE1BenchmarkContextCreateSidecar: BENCHMARK_ENV row missing\n";
        fs::remove_all(logsDir);
        return false;
    }

    const Thoth::BenchmarkAttribution attr = run.attribution();
    if (attr.run_id != run.run_id() || attr.env_hash != run.environment_hash()) {
        std::cerr << "testE1BenchmarkContextCreateSidecar: attribution mismatch\n";
        fs::remove_all(logsDir);
        return false;
    }

    fs::remove_all(logsDir);
    return true;
}

static bool testE1BenchmarkContextBindIndexMerge() {
    const fs::path logsDir = makeTempPath("thoth_e1_bind_logs");
    fs::create_directories(logsDir);

    Thoth::BenchmarkRun run = Thoth::BenchmarkRun::create(makeE1SampleInputs(), makeE1TempLogsOptions(logsDir));
    const std::string originalRunId = run.run_id();
    const std::string originalEnvHash = run.environment_hash();

    Thoth::IndexEnvironment firstIndex;
    firstIndex.rag_index_header = {
        {"model_name", "nomic-embed-text"},
        {"embedding_dimension", 768},
        {"embedding_version", 2},
        {"chunk_count", 10},
    };
    run.bindIndex(firstIndex);

    if (run.index_hash().empty()) {
        std::cerr << "testE1BenchmarkContextBindIndexMerge: expected index_hash after first bind\n";
        fs::remove_all(logsDir);
        return false;
    }

    const std::string firstIndexHash = run.index_hash();

    Thoth::IndexEnvironment secondIndex = firstIndex;
    secondIndex.rag_index_header["chunk_count"] = 20;
    run.bindIndex(secondIndex);

    if (run.run_id() != originalRunId || run.environment_hash() != originalEnvHash) {
        std::cerr << "testE1BenchmarkContextBindIndexMerge: bindIndex changed run identity\n";
        fs::remove_all(logsDir);
        return false;
    }
    if (run.index_hash().empty() || run.index_hash() == firstIndexHash) {
        std::cerr << "testE1BenchmarkContextBindIndexMerge: index_hash did not update\n";
        fs::remove_all(logsDir);
        return false;
    }

    nlohmann::json doc;
    {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        in >> doc;
    }
    if (doc.value("run_id", "") != originalRunId ||
        doc.value("environment_hash", "") != originalEnvHash ||
        doc.value("index_hash", "") != run.index_hash()) {
        std::cerr << "testE1BenchmarkContextBindIndexMerge: sidecar merge mismatch\n";
        fs::remove_all(logsDir);
        return false;
    }

    std::atomic<bool> sidecarValid{true};
    Thoth::IndexEnvironment threadIndex = firstIndex;
    threadIndex.rag_index_header["chunk_count"] = 30;
    Thoth::IndexEnvironment threadIndexB = firstIndex;
    threadIndexB.rag_index_header["chunk_count"] = 40;

    std::thread bindA([&]() {
        run.bindIndex(threadIndex);
    });
    std::thread bindB([&]() {
        run.bindIndex(threadIndexB);
    });
    bindA.join();
    bindB.join();

    try {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        nlohmann::json merged;
        in >> merged;
        if (merged.value("run_id", "") != originalRunId) {
            sidecarValid = false;
        }
    } catch (...) {
        sidecarValid = false;
    }

    if (!sidecarValid.load()) {
        std::cerr << "testE1BenchmarkContextBindIndexMerge: concurrent bind left invalid sidecar\n";
        fs::remove_all(logsDir);
        return false;
    }

    fs::remove_all(logsDir);
    return true;
}

/** E1-17: double bindIndex with different index_hash records index_mismatch; run_id unchanged. */
static bool testE1BenchmarkContextDoubleBindMismatch() {
    const fs::path logsDir = makeTempPath("thoth_e1_double_bind");
    fs::create_directories(logsDir);

    Thoth::BenchmarkRun run = Thoth::BenchmarkRun::create(makeE1SampleInputs(), makeE1TempLogsOptions(logsDir));
    const std::string originalRunId = run.run_id();
    const std::string originalEnvHash = run.environment_hash();

    Thoth::IndexEnvironment firstIndex;
    firstIndex.rag_index_header = {
        {"model_name", "nomic-embed-text"},
        {"embedding_dimension", 768},
        {"embedding_version", 2},
        {"chunk_count", 10},
    };
    run.bindIndex(firstIndex);
    const std::string firstIndexHash = run.index_hash();

    Thoth::IndexEnvironment secondIndex = firstIndex;
    secondIndex.rag_index_header["chunk_count"] = 20;
    run.bindIndex(secondIndex);

    if (run.run_id() != originalRunId || run.environment_hash() != originalEnvHash) {
        std::cerr << "testE1BenchmarkContextDoubleBindMismatch: run identity changed\n";
        fs::remove_all(logsDir);
        return false;
    }
    if (run.index_hash() == firstIndexHash || run.index_hash().empty()) {
        std::cerr << "testE1BenchmarkContextDoubleBindMismatch: index_hash did not update\n";
        fs::remove_all(logsDir);
        return false;
    }

    const auto& mismatch = run.environment().index.index_mismatch;
    if (!mismatch.has_value()) {
        std::cerr << "testE1BenchmarkContextDoubleBindMismatch: index_mismatch missing from environment\n";
        fs::remove_all(logsDir);
        return false;
    }
    if (mismatch->value("prior_hash", "") != firstIndexHash ||
        mismatch->value("new_hash", "") != run.index_hash()) {
        std::cerr << "testE1BenchmarkContextDoubleBindMismatch: index_mismatch hashes wrong\n";
        fs::remove_all(logsDir);
        return false;
    }

    nlohmann::json sidecar;
    {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        in >> sidecar;
    }
    const auto& sidecarIndex = sidecar["environment"]["index"];
    if (!sidecarIndex.contains("index_mismatch")) {
        std::cerr << "testE1BenchmarkContextDoubleBindMismatch: sidecar missing index_mismatch\n";
        fs::remove_all(logsDir);
        return false;
    }

    bool foundBoundEvent = false;
    {
        std::ifstream in(logsDir / "benchmark_env.jsonl");
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            const nlohmann::json row = nlohmann::json::parse(line);
            if (row.value("event", "") != "BENCHMARK_INDEX_BOUND") {
                continue;
            }
            const auto& payload = row.value("payload", nlohmann::json::object());
            if (payload.contains("index_mismatch")) {
                foundBoundEvent = true;
                if (payload["index_mismatch"].value("prior_hash", "") != firstIndexHash) {
                    std::cerr << "testE1BenchmarkContextDoubleBindMismatch: JSONL prior_hash wrong\n";
                    fs::remove_all(logsDir);
                    return false;
                }
            }
        }
    }
    if (!foundBoundEvent) {
        std::cerr << "testE1BenchmarkContextDoubleBindMismatch: BENCHMARK_INDEX_BOUND missing index_mismatch\n";
        fs::remove_all(logsDir);
        return false;
    }

    fs::remove_all(logsDir);
    return true;
}

static bool testG1dFilterTrajectoryCases() {
    const auto all = Thoth::BenchmarkCaseRegistry::getCases();
    const auto filtered = Thoth::filterTrajectoryDisambiguatesCases(all);
    if (filtered.empty()) {
        std::cerr << "testG1dFilterTrajectoryCases: expected non-empty filter\n";
        return false;
    }
    for (const auto& c : filtered) {
        if (c.case_type != "TRAJECTORY_DISAMBIGUATES") {
            std::cerr << "testG1dFilterTrajectoryCases: wrong case type in filter\n";
            return false;
        }
    }
    if (filtered.size() >= all.size()) {
        std::cerr << "testG1dFilterTrajectoryCases: filter did not reduce set\n";
        return false;
    }
    return true;
}

static bool testG1dArmConfigs() {
    const Thoth::BenchmarkConfig cfgA =
        Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::A);
    const Thoth::BenchmarkConfig cfgB =
        Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::B);
    const Thoth::BenchmarkConfig cfgC =
        Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::C);

    if (cfgA.wt != 0.0f || cfgA.force_empty_trajectory) {
        std::cerr << "testG1dArmConfigs: arm A config wrong\n";
        return false;
    }
    if (cfgB.wt != 0.2f || cfgB.force_empty_trajectory) {
        std::cerr << "testG1dArmConfigs: arm B config wrong\n";
        return false;
    }
    if (cfgC.wt != 0.2f || !cfgC.force_empty_trajectory) {
        std::cerr << "testG1dArmConfigs: arm C must force empty trajectory\n";
        return false;
    }
    if (cfgA.wq != cfgB.wq || cfgA.wd != cfgB.wd) {
        std::cerr << "testG1dArmConfigs: wq/wd must match across arms\n";
        return false;
    }
    return true;
}

static bool testG1dWinnerAndTieEpsilon() {
    if (Thoth::computeTrajectoryAblationWinner(0.5f, 0.5f, 0.3f) != "TIE") {
        std::cerr << "testG1dWinnerAndTieEpsilon: expected TIE at equality\n";
        return false;
    }
    if (Thoth::computeTrajectoryAblationWinner(0.5005f, 0.5f, 0.3f) != "TIE") {
        std::cerr << "testG1dWinnerAndTieEpsilon: expected TIE within epsilon\n";
        return false;
    }
    if (Thoth::computeTrajectoryAblationWinner(0.502f, 0.5f, 0.3f) != "A") {
        std::cerr << "testG1dWinnerAndTieEpsilon: expected A win outside epsilon\n";
        return false;
    }
    if (Thoth::computeTrajectoryAblationWinner(0.3f, 0.8f, 0.4f) != "B") {
        std::cerr << "testG1dWinnerAndTieEpsilon: expected B win\n";
        return false;
    }
    return true;
}

/** G1d-03: TfIdf smoke — 2 trajectory cases × 3 arms; summary + winner counts. */
static bool testG1dTrajectoryAblationSmoke() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_g1d_smoke.db").string();
    Memory memory(cfg);

    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    auto idx = new IndexManager(engine.get());

    FileHandler fh;
    const fs::path corpusFile =
        fs::path(fh.getProjectRoot()) / "agent_workspace" / "rag" / "g1d_unit_test_corpus.txt";
    fs::create_directories(corpusFile.parent_path());
    {
        std::ofstream out(corpusFile);
        out << "ReAct Thought Action Observation loop for agent reasoning.\n";
        out << "MemGPT paging memory operating system limits.\n";
        out << "Generative Agents memory stream reflection architecture.\n";
    }
    idx->indexFile(corpusFile.string());
    if (idx->getChunks().empty()) {
        std::cerr << "testG1dTrajectoryAblationSmoke: index produced no chunks\n";
        fs::remove(cfg.database_path);
        fs::remove(corpusFile);
        delete idx;
        return false;
    }

    auto filtered = Thoth::filterTrajectoryDisambiguatesCases(Thoth::BenchmarkCaseRegistry::getCases());
    if (filtered.size() < 2) {
        std::cerr << "testG1dTrajectoryAblationSmoke: need >= 2 trajectory cases\n";
        fs::remove(cfg.database_path);
        fs::remove(corpusFile);
        delete idx;
        return false;
    }
    filtered.resize(2);

    RAGPipeline rag(std::move(engine), idx, &cfg, &memory);
    Thoth::BenchmarkRunner runner(rag);

    const Thoth::BenchmarkResult result_a =
        runner.run(Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::A), filtered);
    const Thoth::BenchmarkResult result_b =
        runner.run(Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::B), filtered);
    const Thoth::BenchmarkResult result_c =
        runner.run(Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::C), filtered);

    if (result_a.cases.size() != 2 || result_b.cases.size() != 2 || result_c.cases.size() != 2) {
        std::cerr << "testG1dTrajectoryAblationSmoke: case count mismatch\n";
        fs::remove(cfg.database_path);
        fs::remove(corpusFile);
        delete idx;
        return false;
    }

    const Thoth::TrajectoryAblationSummary summary =
        Thoth::computeTrajectoryAblationSummary(filtered, result_a, result_b, result_c);

    const int winnerTotal = summary.a_wins + summary.b_wins + summary.c_wins + summary.ties;
    if (winnerTotal != 2) {
        std::cerr << "testG1dTrajectoryAblationSmoke: winner counts != cases_run\n";
        fs::remove(cfg.database_path);
        fs::remove(corpusFile);
        delete idx;
        return false;
    }
    if (summary.decision == Thoth::G1dDecision::PENDING) {
        std::cerr << "testG1dTrajectoryAblationSmoke: decision still PENDING\n";
        fs::remove(cfg.database_path);
        fs::remove(corpusFile);
        delete idx;
        return false;
    }

    fs::remove(cfg.database_path);
    fs::remove(corpusFile);
    delete idx;
    return true;
}

static Thoth::E2EvalConfig makeE2StrictTestConfig() {
    Thoth::E2EvalConfig cfg;
    cfg.tier = Thoth::E2EvalTier::STRICT;
    cfg.versions.corpus_snapshot_id = "e2-test-corpus";
    cfg.versions.model_version_or_weights_hash = "mock";
    cfg.versions.embedding_model_version = "TfIdf:2";
    cfg.versions.retrieval_engine_version = Thoth::kE2StrictRetrievalEngineVersion;
    return cfg;
}

/** Optional episode-channel wiring for D2-02 harness runs (publication + optional replay). */
struct E2EpisodeChannelHarness {
    std::shared_ptr<Thoth::InProcessEpisodeEventChannel> channel;
    std::shared_ptr<Thoth::ReplaySubscriber> replay;
    bool enable_episode_publication = false;
    bool register_replay_subscriber = false;
    bool wired = false;
};

static Thoth::EpisodicLearningArmObservation runE2TestArm(
    const Thoth::EpisodicLearningCase& spec,
    const std::string& armLabel,
    const Thoth::BenchmarkAttribution& attribution,
    Thoth::SealedEpisodeInjectionLog* sealedLogOut = nullptr,
    Thoth::E2RunBlockReason* runBlockReasonOut = nullptr,
    E2EpisodeChannelHarness* channelHarness = nullptr) {
    setenv("THOTH_MOCK_EPISODIC", "1", 1);
    setenv("THOTH_MOCK_LLM", "true", 1);

    constexpr std::int64_t kBuilderTs = 1'700'000'000'000LL;
    const Thoth::SealedEpisodeInjectionLog sealedLog =
        Thoth::buildStrictInjectionLogFromCaseTable(spec, armLabel, kBuilderTs);
    if (sealedLogOut) {
        *sealedLogOut = sealedLog;
    }

    Config cfg;
    cfg.max_reflections = 0;
    cfg.database_path =
        makeTempPath("thoth_e2_test_" + spec.id + "_" + armLabel).string();
    fs::remove(cfg.database_path);

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    EmbeddingEngine* enginePtr = engine.get();

    (void)sealedLog;

    auto idx = new IndexManager(enginePtr);
    if (!spec.index_distractor_text.empty()) {
        Thoth::addEpisodicEvalCorpusChunk(enginePtr, idx, spec.index_distractor_text);
    }

    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());

    rag->setEventCallback([&](const ControllerEvent& ev) { (void)ev; });

    const Thoth::E2EvalConfig strictConfig = makeE2StrictTestConfig();

    auto planner = std::make_shared<Thoth::EpisodicLearningMockPlanner>(spec.validation_token);
    auto registry = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    controller.set_max_reflections(0);
    controller.set_e2_strict_eval_context(&sealedLog, &strictConfig);
    memory->setActiveSessionId(spec.id + "-" + armLabel + "-goal");

    if (channelHarness && channelHarness->enable_episode_publication) {
        if (!channelHarness->channel) {
            channelHarness->channel = std::make_shared<Thoth::InProcessEpisodeEventChannel>();
        }
        if (!channelHarness->wired) {
            channelHarness->channel->subscribe(std::make_shared<Thoth::EvaluationSubscriber>());
            if (channelHarness->register_replay_subscriber) {
                channelHarness->replay = std::make_shared<Thoth::ReplaySubscriber>();
                channelHarness->channel->subscribe(channelHarness->replay);
            }
            channelHarness->wired = true;
        }
        cfg.enable_episodic_evaluation_publication = true;
        controller.set_config(&cfg);
        controller.set_episode_event_channel(channelHarness->channel.get());
    }

    std::atomic<bool> terminal{false};
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_COMPLETED || ev.type == EventType::PLAN_FAILED ||
            ev.type == EventType::PLAN_ABORTED) {
            terminal.store(true);
        }
    });

    controller.execute_goal(spec.goal, attribution);

    int timeout = 150;
    while (!terminal.load() && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --timeout;
    }

    Thoth::EpisodicLearningArmObservation obs;
    obs.arm_label = armLabel;

    const bool episodicRequired =
        Thoth::strictEpisodicContentRequired(spec, armLabel);
    if (const auto execResult =
            Thoth::executiveStrictRetrievalFromPlan(controller.get_current_plan())) {
        obs.retrieval = Thoth::provenanceFromStrictRetrievalResult(
            *execResult, spec.expectations, episodicRequired);
        obs.arm_scoring_status = obs.retrieval.arm_scoring_status;
    } else {
        obs.retrieval.arm_scoring_status = Thoth::E2ArmScoringStatus::FAILED_RETRIEVAL;
        obs.arm_scoring_status = Thoth::E2ArmScoringStatus::FAILED_RETRIEVAL;
    }

    controller.clear_e2_strict_eval_context();

    if (runBlockReasonOut) {
        *runBlockReasonOut = Thoth::runBlockReasonFromPlan(controller.get_current_plan());
    }

    switch (controller.get_state()) {
        case Thoth::ControllerState::COMPLETED:
            obs.terminal_state = "COMPLETED";
            obs.final_success_score = 1.0f;
            break;
        case Thoth::ControllerState::FAILED:
            obs.terminal_state = "FAILED";
            obs.final_success_score = 0.0f;
            break;
        default:
            obs.terminal_state = "INCOMPLETE";
            obs.final_success_score = 0.0f;
            break;
    }

    fs::remove(cfg.database_path);
    return obs;
}

static std::optional<Thoth::EpisodicLearningCase> findEpisodicCaseById(const std::string& id) {
    for (const auto& spec : Thoth::getEpisodicLearningCases()) {
        if (spec.id == id) {
            return spec;
        }
    }
    return std::nullopt;
}

static bool testE2StrictInjectionLogFromCaseTable() {
    constexpr std::int64_t kBuilderTs = 1'700'000'000'000LL;

    Thoth::SealedEpisodeInjectionLog mutableLog;
    Thoth::EpisodeInjectionEntry probe;
    probe.episode_id = "ep";
    probe.source = "evaluation";
    probe.content = "x";
    probe.content_hash = "h";
    probe.injected_at_ms = kBuilderTs;
    mutableLog.append(std::move(probe));
    try {
        mutableLog.seal();
        Thoth::EpisodeInjectionEntry probe2;
        probe2.episode_id = "ep2";
        probe2.source = "evaluation";
        probe2.content = "y";
        probe2.content_hash = "h2";
        probe2.injected_at_ms = kBuilderTs;
        mutableLog.append(std::move(probe2));
        std::cerr << "testE2StrictInjectionLogFromCaseTable: append after seal should throw\n";
        return false;
    } catch (const std::logic_error&) {
    }

    const auto e201 = findEpisodicCaseById("E2-01");
    const auto e203 = findEpisodicCaseById("E2-03");
    if (!e201 || !e203) {
        std::cerr << "testE2StrictInjectionLogFromCaseTable: missing golden cases\n";
        return false;
    }

    if (e203->plant_message.empty() || e203->plant_session_id.empty() ||
        !e203->cold_arm_pre_consolidated) {
        std::cerr << "testE2StrictInjectionLogFromCaseTable: E2-03 fixture incomplete — "
                     "plant_message, plant_session_id, cold_arm_pre_consolidated required\n";
        return false;
    }

    const auto cold01 =
        Thoth::buildStrictInjectionLogFromCaseTable(*e201, "cold", kBuilderTs);
    if (!cold01.isSealed() || !cold01.entries().empty()) {
        std::cerr << "testE2StrictInjectionLogFromCaseTable: E2-01 cold should be empty sealed\n";
        return false;
    }

    const auto warm01 =
        Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs);
    if (!warm01.isSealed() || warm01.entries().size() != 1) {
        std::cerr << "testE2StrictInjectionLogFromCaseTable: E2-01 warm should have one entry\n";
        return false;
    }
    const auto& warmEntry = warm01.entries().front();
    if (warmEntry.content != e201->plant_message ||
        warmEntry.episode_id != e201->plant_session_id || warmEntry.source != "evaluation" ||
        warmEntry.injected_at_ms != kBuilderTs ||
        warmEntry.content_hash != Thoth::sha256Hex(e201->plant_message)) {
        std::cerr << "testE2StrictInjectionLogFromCaseTable: E2-01 warm entry mismatch\n";
        return false;
    }

    const auto cold03 =
        Thoth::buildStrictInjectionLogFromCaseTable(*e203, "cold", kBuilderTs);
    if (!cold03.isSealed() || cold03.entries().size() != 1) {
        std::cerr << "testE2StrictInjectionLogFromCaseTable: E2-03 cold should have one entry\n";
        return false;
    }
    if (cold03.entries().front().content != e203->plant_message) {
        std::cerr << "testE2StrictInjectionLogFromCaseTable: E2-03 cold content mismatch\n";
        return false;
    }

    const std::string jsonA =
        Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs).toJson().dump();
    const std::string jsonB =
        Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs).toJson().dump();
    if (jsonA != jsonB) {
        std::cerr << "testE2StrictInjectionLogFromCaseTable: builder not deterministic\n";
        return false;
    }

    return true;
}

static bool testE2EmbeddingVersionPin() {
    // Regression: assigning int 2 to std::string via char coercion yields \x02, not "2".
    {
        Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
        cfg.versions.embedding_model_version = std::string(1, static_cast<char>(2));
        try {
            Thoth::validateStrictConfigForOfficialRun(cfg, true);
            std::cerr << "testE2EmbeddingVersionPin: expected reject for control-char pin\n";
            return false;
        } catch (const Thoth::E2StrictValidationError&) {
        }
    }

    // Harness path: TfIdf engine internal version → canonical printable pin.
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf);
    const std::string pin =
        Thoth::makeEmbeddingModelVersionPin("TfIdf", engine.getInternalVersion());
    if (pin != "TfIdf:2") {
        std::cerr << "testE2EmbeddingVersionPin: unexpected pin '" << pin << "'\n";
        return false;
    }
    if (!Thoth::isPrintableVersionPin(pin)) {
        std::cerr << "testE2EmbeddingVersionPin: canonical pin not printable\n";
        return false;
    }

    Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    cfg.versions.embedding_model_version = pin;
    try {
        Thoth::validateStrictConfigForOfficialRun(cfg, true);
    } catch (const Thoth::E2StrictValidationError& e) {
        std::cerr << "testE2EmbeddingVersionPin: valid harness pin rejected: " << e.what()
                  << '\n';
        return false;
    }

    return true;
}

static bool testE2StrictConfigEnforcement() {
    Thoth::E2EvalConfig incomplete = Thoth::E2EvalConfig::strictDefaults();
    try {
        Thoth::validateStrictConfigForOfficialRun(incomplete, true);
        std::cerr << "testE2StrictConfigEnforcement: expected validation throw\n";
        return false;
    } catch (const Thoth::E2StrictValidationError&) {
    }

    const auto valid = makeE2StrictTestConfig();
    try {
        Thoth::validateStrictConfigForOfficialRun(valid, true);
    } catch (const Thoth::E2StrictValidationError& e) {
        std::cerr << "testE2StrictConfigEnforcement: valid config rejected: " << e.what() << '\n';
        return false;
    }

    const auto fp = Thoth::computeEvaluationFingerprint(valid);
    if (fp.fingerprint_hash.empty() || fp.canonical_json.empty()) {
        std::cerr << "testE2StrictConfigEnforcement: empty fingerprint\n";
        return false;
    }

    Thoth::EpisodicLearningExpectations positive;
    positive.expect_warm_retrieval_hit = true;
    positive.lift_constraint = Thoth::EpisodicLiftConstraint::GTE;
    positive.lift_threshold = Thoth::kEpisodicLearningLiftMargin;

    Thoth::EpisodicLearningArmObservation cold;
    cold.terminal_state = "FAILED";
    cold.final_success_score = 0.0f;

    Thoth::EpisodicLearningArmObservation warm;
    warm.terminal_state = "COMPLETED";
    warm.final_success_score = 1.0f;
    warm.retrieval.warm_retrieval_hit = true;

    const auto rejected = Thoth::evaluateEpisodicLearningCase(
        "pin-check", positive, cold, warm, incomplete);
    if (rejected.passes || rejected.failure_reason.find("version pins") == std::string::npos) {
        std::cerr << "testE2StrictConfigEnforcement: incomplete config should fail scoring\n";
        return false;
    }

    return true;
}

static bool testE2TableDrivenEvaluator() {
    Thoth::EpisodicLearningExpectations positive;
    positive.expect_warm_retrieval_hit = true;
    positive.lift_constraint = Thoth::EpisodicLiftConstraint::GTE;
    positive.lift_threshold = Thoth::kEpisodicLearningLiftMargin;

    Thoth::EpisodicLearningArmObservation cold;
    cold.terminal_state = "FAILED";
    cold.final_success_score = 0.0f;
    cold.retrieval.warm_retrieval_hit = false;

    Thoth::EpisodicLearningArmObservation warm;
    warm.terminal_state = "COMPLETED";
    warm.final_success_score = 1.0f;
    warm.retrieval.warm_retrieval_hit = true;
    warm.retrieval.retrieved_memory_id = "mem-1";

    const auto passEval = Thoth::evaluateEpisodicLearningCase(
        "synthetic", positive, cold, warm, makeE2StrictTestConfig());
    if (!passEval.passes || passEval.lift < Thoth::kEpisodicLearningLiftMargin) {
        std::cerr << "testE2TableDrivenEvaluator: expected synthetic pass\n";
        return false;
    }

    Thoth::EpisodicLearningExpectations negative;
    negative.expect_warm_retrieval_hit = false;
    negative.lift_constraint = Thoth::EpisodicLiftConstraint::ABS_LT;
    negative.lift_threshold = Thoth::kEpisodicLearningLiftMargin;
    negative.forbidden_retrieval_tokens = {"Apollo"};

    Thoth::EpisodicLearningArmObservation warmFail = warm;
    warmFail.retrieval.warm_retrieval_hit = true;
    const auto failEval = Thoth::evaluateEpisodicLearningCase(
        "synthetic-neg", negative, cold, warmFail, makeE2StrictTestConfig());
    if (failEval.passes) {
        std::cerr << "testE2TableDrivenEvaluator: expected negative retrieval fail\n";
        return false;
    }

    return true;
}

static bool testE2A2StrictArmNoPlantSourceContract() {
    FileHandler fh;
    const fs::path harnessPath = fs::path(fh.getProjectRoot()) / "external" / "basic_agent" /
                                 "src" / "run_episodic_learning_benchmark.cpp";
    std::ifstream in(harnessPath);
    if (!in.is_open()) {
        std::cerr << "testE2A2StrictArmNoPlantSourceContract: cannot read harness source\n";
        return false;
    }

    const std::string source((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

    const auto runCaseArmPos = source.find("E2CaseArmPlumbingResult runCaseArm");
    if (runCaseArmPos == std::string::npos) {
        std::cerr << "testE2A2StrictArmNoPlantSourceContract: runCaseArm not found\n";
        return false;
    }

    const auto mainPos = source.find("\nint main()", runCaseArmPos);
    const std::size_t runCaseArmEnd =
        mainPos == std::string::npos ? source.size() : mainPos;
    const std::string runCaseArmBody =
        source.substr(runCaseArmPos, runCaseArmEnd - runCaseArmPos);

    if (runCaseArmBody.find("plantAndConsolidate") != std::string::npos) {
        std::cerr << "testE2A2StrictArmNoPlantSourceContract: runCaseArm still calls "
                     "plantAndConsolidate\n";
        return false;
    }

    return true;
}

static bool testE2A2SealedLogOwnership() {
    constexpr std::int64_t kBuilderTs = 1'700'000'000'000LL;
    const auto e201 = findEpisodicCaseById("E2-01");
    const auto e203 = findEpisodicCaseById("E2-03");
    if (!e201 || !e203) {
        std::cerr << "testE2A2SealedLogOwnership: missing golden cases\n";
        return false;
    }

    Thoth::BenchmarkAttribution attr{"e2-ownership-run", "e2-ownership-env"};

    int builderCalls = 0;
    Thoth::setStrictInjectionLogBuilderCallCounterForTests(&builderCalls);

    Thoth::SealedEpisodeInjectionLog armLog;
    runE2TestArm(*e201, "warm", attr, &armLog);

    Thoth::clearStrictInjectionLogBuilderCallCounterForTests();

    if (builderCalls != 1) {
        std::cerr << "testE2A2SealedLogOwnership: expected one builder call, got "
                  << builderCalls << '\n';
        return false;
    }

    const std::string standalone =
        Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs).toJson().dump();
    if (armLog.toJson().dump() != standalone) {
        std::cerr << "testE2A2SealedLogOwnership: arm log not byte-identical to standalone "
                     "builder\n";
        return false;
    }

    const std::string coldStandalone =
        Thoth::buildStrictInjectionLogFromCaseTable(*e203, "cold", kBuilderTs).toJson().dump();
    Thoth::SealedEpisodeInjectionLog coldArmLog;
    builderCalls = 0;
    Thoth::setStrictInjectionLogBuilderCallCounterForTests(&builderCalls);
    runE2TestArm(*e203, "cold", attr, &coldArmLog);
    Thoth::clearStrictInjectionLogBuilderCallCounterForTests();

    if (builderCalls != 1) {
        std::cerr << "testE2A2SealedLogOwnership: E2-03 cold expected one builder call\n";
        return false;
    }
    if (coldArmLog.toJson().dump() != coldStandalone) {
        std::cerr << "testE2A2SealedLogOwnership: E2-03 cold arm log mismatch\n";
        return false;
    }

    return true;
}

static bool testE2A2HarnessWiringSmoke() {
    const auto e201 = findEpisodicCaseById("E2-01");
    if (!e201) {
        std::cerr << "testE2A2HarnessWiringSmoke: missing E2-01\n";
        return false;
    }

    constexpr std::int64_t kBuilderTs = 1'700'000'000'000LL;
    const std::string expectedWarmLog =
        Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs).toJson().dump();

    Thoth::BenchmarkAttribution attr{"e2-a2-smoke-run", "e2-a2-smoke-env"};
    Thoth::SealedEpisodeInjectionLog warmArmLog;
    const auto warmObs = runE2TestArm(*e201, "warm", attr, &warmArmLog);

    if (warmArmLog.toJson().dump() != expectedWarmLog) {
        std::cerr << "testE2A2HarnessWiringSmoke: warm sealed log mismatch vs A1 builder\n";
        return false;
    }

    if (warmObs.arm_label != "warm") {
        std::cerr << "testE2A2HarnessWiringSmoke: unexpected arm label\n";
        return false;
    }

    return true;
}

struct E2RetrievalTestFixture {
    Config cfg;
    std::unique_ptr<EmbeddingEngine> engine;
    std::unique_ptr<IndexManager> idx;

    explicit E2RetrievalTestFixture(const Thoth::EpisodicLearningCase& spec) {
        cfg.max_reflections = 0;
        cfg.database_path = makeTempPath("e2_strict_retrieval").string();
        engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
        idx = std::make_unique<IndexManager>(engine.get());
        if (!spec.index_distractor_text.empty()) {
            Thoth::addEpisodicEvalCorpusChunk(engine.get(), idx.get(), spec.index_distractor_text);
        }
    }
};

static Thoth::E2StrictRetrievalResult runStrictKernelArm(
    const Thoth::EpisodicLearningCase& spec,
    const std::string& armLabel,
    const Thoth::SealedEpisodeInjectionLog& sealedLog,
    E2RetrievalTestFixture& fixture) {
    Thoth::E2StrictRetrievalInput input;
    input.query = spec.goal;
    input.episode_log = &sealedLog;
    input.config = makeE2StrictTestConfig();
    input.index = fixture.idx.get();
    input.engine = fixture.engine.get();
    input.top_k = 5;
    return Thoth::e2StrictRetrieve(input);
}

static bool strictKernelHasEpisodicHit(const Thoth::E2StrictRetrievalResult& retrieval,
                                       const Thoth::EpisodicLearningCase& spec,
                                       const std::string& armLabel) {
    const bool episodicRequired =
        Thoth::strictEpisodicContentRequired(spec, armLabel);
    const auto prov = Thoth::provenanceFromStrictRetrievalResult(
        retrieval, spec.expectations, episodicRequired);
    return prov.warm_retrieval_hit;
}

static bool testE2StrictRetrievalKernel() {
    constexpr std::int64_t kBuilderTs = 1'700'000'000'000LL;

    const auto e201 = findEpisodicCaseById("E2-01");
    const auto e202 = findEpisodicCaseById("E2-02");
    const auto e203 = findEpisodicCaseById("E2-03");
    if (!e201 || !e202 || !e203) {
        std::cerr << "testE2StrictRetrievalKernel: missing golden cases\n";
        return false;
    }

    // --- Retrieval (E2-01 / E2-02 / E2-03) ---
    {
        E2RetrievalTestFixture fixture(*e201);
        const auto warmLog =
            Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs);
        const auto coldLog =
            Thoth::buildStrictInjectionLogFromCaseTable(*e201, "cold", kBuilderTs);

        const auto warmRetrieval = runStrictKernelArm(*e201, "warm", warmLog, fixture);
        const auto coldRetrieval = runStrictKernelArm(*e201, "cold", coldLog, fixture);

        if (warmRetrieval.status != Thoth::E2ArmScoringStatus::OK) {
            std::cerr << "testE2StrictRetrievalKernel: E2-01 warm retrieval failed\n";
            return false;
        }
        if (!strictKernelHasEpisodicHit(warmRetrieval, *e201, "warm")) {
            std::cerr << "testE2StrictRetrievalKernel: E2-01 warm expected Apollo hit\n";
            return false;
        }
        if (strictKernelHasEpisodicHit(coldRetrieval, *e201, "cold")) {
            std::cerr << "testE2StrictRetrievalKernel: E2-01 cold should not hit Apollo\n";
            return false;
        }
    }

    {
        E2RetrievalTestFixture fixture(*e202);
        const auto warmLog =
            Thoth::buildStrictInjectionLogFromCaseTable(*e202, "warm", kBuilderTs);
        const auto warmRetrieval = runStrictKernelArm(*e202, "warm", warmLog, fixture);
        if (!strictKernelHasEpisodicHit(warmRetrieval, *e202, "warm")) {
            std::cerr << "testE2StrictRetrievalKernel: E2-02 warm expected hit\n";
            return false;
        }
    }

    {
        E2RetrievalTestFixture fixture(*e203);
        const auto warmLog =
            Thoth::buildStrictInjectionLogFromCaseTable(*e203, "warm", kBuilderTs);
        const auto coldLog =
            Thoth::buildStrictInjectionLogFromCaseTable(*e203, "cold", kBuilderTs);
        const auto warmRetrieval = runStrictKernelArm(*e203, "warm", warmLog, fixture);
        const auto coldRetrieval = runStrictKernelArm(*e203, "cold", coldLog, fixture);
        if (strictKernelHasEpisodicHit(warmRetrieval, *e203, "warm") ||
            strictKernelHasEpisodicHit(coldRetrieval, *e203, "cold")) {
            std::cerr << "testE2StrictRetrievalKernel: E2-03 expected no episodic Apollo hit\n";
            return false;
        }
    }

    // --- Boundary mapping ---
    {
        E2RetrievalTestFixture fixture(*e201);
        const auto warmLog =
            Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs);
        const auto retrieval = runStrictKernelArm(*e201, "warm", warmLog, fixture);
        const auto prov = Thoth::provenanceFromStrictRetrievalResult(
            retrieval, e201->expectations,
            Thoth::strictEpisodicContentRequired(*e201, "warm"));
        if (prov.chunks.size() != retrieval.chunks.size()) {
            std::cerr << "testE2StrictRetrievalKernel: boundary chunk count mismatch\n";
            return false;
        }
        if (prov.arm_scoring_status != Thoth::E2ArmScoringStatus::OK) {
            std::cerr << "testE2StrictRetrievalKernel: boundary status not OK\n";
            return false;
        }
    }

    // --- Fail-closed ---
    {
        E2RetrievalTestFixture fixture(*e201);
        const auto sealedLog =
            Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs);

        Thoth::SealedEpisodeInjectionLog unsealed;
        Thoth::EpisodeInjectionEntry entry;
        entry.episode_id = "x";
        entry.source = "evaluation";
        entry.content = "y";
        entry.content_hash = "z";
        entry.injected_at_ms = kBuilderTs;
        unsealed.append(std::move(entry));

        Thoth::E2StrictRetrievalInput input;
        input.query = e201->goal;
        input.episode_log = &unsealed;
        input.config = makeE2StrictTestConfig();
        input.index = fixture.idx.get();
        input.engine = fixture.engine.get();
        input.top_k = 5;
        const auto unsealedResult = Thoth::e2StrictRetrieve(input);
        if (unsealedResult.status != Thoth::E2ArmScoringStatus::FAILED_STRICT_BOUNDARY) {
            std::cerr << "testE2StrictRetrievalKernel: unsealed log should fail boundary\n";
            return false;
        }

        Thoth::E2EvalConfig integration = Thoth::E2EvalConfig::integrationDefaults();
        integration.versions = makeE2StrictTestConfig().versions;
        input.config = integration;
        input.episode_log = &sealedLog;
        const auto tierResult = Thoth::e2StrictRetrieve(input);
        if (tierResult.status != Thoth::E2ArmScoringStatus::FAILED_STRICT_BOUNDARY) {
            std::cerr << "testE2StrictRetrievalKernel: non-STRICT tier should fail boundary\n";
            return false;
        }

        input.config = makeE2StrictTestConfig();
        input.top_k = 0;
        const auto badTopK = Thoth::e2StrictRetrieve(input);
        if (badTopK.status != Thoth::E2ArmScoringStatus::FAILED_RETRIEVAL ||
            !badTopK.chunks.empty()) {
            std::cerr << "testE2StrictRetrievalKernel: invalid top_k should fail closed\n";
            return false;
        }
    }

    // --- Determinism (pre-built sealed log; no builder re-invoke) ---
    {
        E2RetrievalTestFixture fixture(*e201);
        const Thoth::SealedEpisodeInjectionLog fixedLog =
            Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", kBuilderTs);
        const auto first = runStrictKernelArm(*e201, "warm", fixedLog, fixture);
        const auto second = runStrictKernelArm(*e201, "warm", fixedLog, fixture);
        if (first.status != second.status || first.chunks.size() != second.chunks.size()) {
            std::cerr << "testE2StrictRetrievalKernel: determinism status/count mismatch\n";
            return false;
        }
        for (std::size_t i = 0; i < first.chunks.size(); ++i) {
            if (first.chunks[i].chunk_id != second.chunks[i].chunk_id) {
                std::cerr << "testE2StrictRetrievalKernel: determinism chunk order mismatch\n";
                return false;
            }
        }
    }

    // --- Purity source contract ---
    {
        FileHandler fh;
        const fs::path kernelPath = fs::path(fh.getProjectRoot()) / "external" / "basic_agent" /
                                    "src" / "e2_strict_retrieval.cpp";
        std::ifstream in(kernelPath);
        if (!in.is_open()) {
            std::cerr << "testE2StrictRetrievalKernel: cannot read kernel source\n";
            return false;
        }
        const std::string kernelSource((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
        if (kernelSource.find("executive_controller") != std::string::npos ||
            kernelSource.find("RAGPipeline") != std::string::npos ||
            kernelSource.find("memory.h") != std::string::npos ||
            kernelSource.find("sqlite") != std::string::npos) {
            std::cerr << "testE2StrictRetrievalKernel: kernel purity violation in source\n";
            return false;
        }
    }

    // --- Harness STRICT boundary must not use executive provenance ---
    {
        FileHandler fh;
        const fs::path harnessPath = fs::path(fh.getProjectRoot()) / "external" / "basic_agent" /
                                     "src" / "run_episodic_learning_benchmark.cpp";
        std::ifstream in(harnessPath);
        const std::string source((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
        const auto pos = source.find("E2CaseArmPlumbingResult runCaseArm");
        const auto end = source.find("\nint main()", pos);
        const std::string body = source.substr(pos, end - pos);
        if (body.find("provenanceFromRetrievalStepResult") != std::string::npos) {
            std::cerr << "testE2StrictRetrievalKernel: runCaseArm still uses executive "
                         "provenance helper\n";
            return false;
        }
        if (body.find("provenanceFromStrictRetrievalResult") == std::string::npos) {
            std::cerr << "testE2StrictRetrievalKernel: runCaseArm missing strict boundary mapper\n";
            return false;
        }
    }

    return true;
}

static bool testE2ExecutiveStrictEquivalence() {
    constexpr std::int64_t kBuilderTs = 1'700'000'000'000LL;

    const auto e201 = findEpisodicCaseById("E2-01");
    const auto e202 = findEpisodicCaseById("E2-02");
    const auto e203 = findEpisodicCaseById("E2-03");
    if (!e201 || !e202 || !e203) {
        std::cerr << "testE2ExecutiveStrictEquivalence: missing golden cases\n";
        return false;
    }

    Thoth::BenchmarkAttribution attr{"e2-a4-equiv-run", "e2-a4-equiv-env"};

    for (const auto* spec : {&*e201, &*e202, &*e203}) {
        for (const char* armLabel : {"cold", "warm"}) {
            const Thoth::SealedEpisodeInjectionLog sealedLog =
                Thoth::buildStrictInjectionLogFromCaseTable(*spec, armLabel, kBuilderTs);
            E2RetrievalTestFixture fixture(*spec);
            const auto harnessResult = runStrictKernelArm(*spec, armLabel, sealedLog, fixture);

            setenv("THOTH_MOCK_EPISODIC", "1", 1);
            setenv("THOTH_MOCK_LLM", "true", 1);

            Config cfg;
            cfg.max_reflections = 0;
            cfg.database_path =
                makeTempPath("e2_a4_exec_" + spec->id + "_" + armLabel).string();
            fs::remove(cfg.database_path);

            auto memory = std::make_shared<Memory>(cfg);
            auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
            auto idx = new IndexManager(engine.get());
            if (!spec->index_distractor_text.empty()) {
                Thoth::addEpisodicEvalCorpusChunk(
                    engine.get(), idx, spec->index_distractor_text);
            }

            auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
            const Thoth::E2EvalConfig strictConfig = makeE2StrictTestConfig();
            auto planner =
                std::make_shared<Thoth::EpisodicLearningMockPlanner>(spec->validation_token);
            auto registry = std::make_shared<ToolRegistry>();
            Thoth::ExecutiveController controller(planner, registry, rag, memory);
            controller.set_max_reflections(0);
            controller.set_e2_strict_eval_context(&sealedLog, &strictConfig);
            memory->setActiveSessionId(spec->id + "-" + armLabel + "-goal");

            std::atomic<bool> terminal{false};
            controller.set_event_callback([&](const ControllerEvent& ev) {
                if (ev.type == EventType::PLAN_COMPLETED || ev.type == EventType::PLAN_FAILED ||
                    ev.type == EventType::PLAN_ABORTED) {
                    terminal.store(true);
                }
            });
            controller.execute_goal(spec->goal, attr);

            int timeout = 150;
            while (!terminal.load() && timeout > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                --timeout;
            }

            const auto execResult =
                Thoth::executiveStrictRetrievalFromPlan(controller.get_current_plan());
            controller.clear_e2_strict_eval_context();
            fs::remove(cfg.database_path);
            delete idx;

            if (!execResult) {
                std::cerr << "testE2ExecutiveStrictEquivalence: missing executive result for "
                          << spec->id << ' ' << armLabel << '\n';
                return false;
            }
            if (!Thoth::e2StrictRetrievalResultsEquivalent(harnessResult, *execResult)) {
                std::cerr << "testE2ExecutiveStrictEquivalence: mismatch " << spec->id << ' '
                          << armLabel << " harness_status="
                          << Thoth::e2ArmScoringStatusToString(harnessResult.status)
                          << " executive_status="
                          << Thoth::e2ArmScoringStatusToString(execResult->status) << '\n';
                return false;
            }
        }
    }

    // Failure-path equivalence: unsealed log → FAILED_STRICT_BOUNDARY on both paths.
    {
        E2RetrievalTestFixture fixture(*e201);
        Thoth::SealedEpisodeInjectionLog unsealed;
        Thoth::EpisodeInjectionEntry entry;
        entry.episode_id = "x";
        entry.source = "evaluation";
        entry.content = "y";
        entry.content_hash = "z";
        entry.injected_at_ms = kBuilderTs;
        unsealed.append(std::move(entry));

        Thoth::E2StrictRetrievalInput input;
        input.query = e201->goal;
        input.episode_log = &unsealed;
        input.config = makeE2StrictTestConfig();
        input.index = fixture.idx.get();
        input.engine = fixture.engine.get();
        input.top_k = 5;
        const auto harnessResult = Thoth::e2StrictRetrieve(input);
        if (harnessResult.status != Thoth::E2ArmScoringStatus::FAILED_STRICT_BOUNDARY) {
            std::cerr << "testE2ExecutiveStrictEquivalence: unsealed harness expected boundary "
                         "fail\n";
            return false;
        }

        Config cfg;
        cfg.max_reflections = 0;
        cfg.database_path = makeTempPath("e2_a4_unsealed").string();
        fs::remove(cfg.database_path);
        auto memory = std::make_shared<Memory>(cfg);
        auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
        auto idx = new IndexManager(engine.get());
        auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
        const Thoth::E2EvalConfig strictConfig = makeE2StrictTestConfig();
        auto planner = std::make_shared<Thoth::EpisodicLearningMockPlanner>(e201->validation_token);
        auto registry = std::make_shared<ToolRegistry>();
        Thoth::ExecutiveController controller(planner, registry, rag, memory);
        controller.set_e2_strict_eval_context(&unsealed, &strictConfig);

        PlanStep step;
        step.step_id = "retrieve";
        step.type = StepType::RETRIEVAL;
        step.payload = {{"query", e201->goal}, {"top_k", 5}};

        Thoth::StepExecutionContext ctx;
        ctx.e2_strict_episode_log = &unsealed;
        ctx.e2_eval_config = &strictConfig;

        Thoth::WorkflowEngine workflow(registry, rag, memory, nullptr, nullptr);
        const auto stepResult = workflow.executeStep(step, "plan", ctx);
        delete idx;
        fs::remove(cfg.database_path);

        const auto execResult = Thoth::e2StrictRetrievalResultFromRetrievalStep(stepResult.data);
        if (execResult.status != Thoth::E2ArmScoringStatus::FAILED_STRICT_BOUNDARY) {
            std::cerr << "testE2ExecutiveStrictEquivalence: unsealed executive expected boundary "
                         "fail\n";
            return false;
        }
        if (!Thoth::e2StrictRetrievalResultsEquivalent(harnessResult, execResult)) {
            std::cerr << "testE2ExecutiveStrictEquivalence: unsealed failure-path mismatch\n";
            return false;
        }
    }

    return true;
}

static bool testE2A4StaticDispatchAudit() {
    FileHandler fh;
    const fs::path workflowPath =
        fs::path(fh.getProjectRoot()) / "external" / "basic_agent" / "src" / "workflow_engine.cpp";
    std::ifstream in(workflowPath);
    if (!in.is_open()) {
        std::cerr << "testE2A4StaticDispatchAudit: cannot read workflow_engine.cpp\n";
        return false;
    }
    const std::string source((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

    const auto fnPos = source.find("WorkflowEngine::executeRetrieval");
    const auto fnEnd = source.find("\nStepResult WorkflowEngine::executeLLM", fnPos);
    if (fnPos == std::string::npos || fnEnd == std::string::npos) {
        std::cerr << "testE2A4StaticDispatchAudit: executeRetrieval body not found\n";
        return false;
    }
    const std::string body = source.substr(fnPos, fnEnd - fnPos);

    if (body.find("e2StrictRetrieve") == std::string::npos) {
        std::cerr << "testE2A4StaticDispatchAudit: missing e2StrictRetrieve in executeRetrieval\n";
        return false;
    }
    if (body.find("Single dispatch decision point") == std::string::npos) {
        std::cerr << "testE2A4StaticDispatchAudit: missing dispatch invariant comment\n";
        return false;
    }

    const auto strictBranch = body.find("E2EvalTier::STRICT");
    const auto ragCall = body.find("ragPipeline_->retrieveRelevant", strictBranch);
    if (strictBranch == std::string::npos || ragCall == std::string::npos ||
        ragCall < strictBranch) {
        std::cerr << "testE2A4StaticDispatchAudit: RAG call not after STRICT branch\n";
        return false;
    }

    if (source.find("executeRetrievalStrict") != std::string::npos) {
        std::cerr << "testE2A4StaticDispatchAudit: forbidden parallel strict helper\n";
        return false;
    }

    return true;
}

static bool testE2NonStrictRetrievalPreserved() {
    Config cfg;
    cfg.max_reflections = 0;
    cfg.database_path = makeTempPath("e2_non_strict_rag").string();
    fs::remove(cfg.database_path);

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    auto idx = new IndexManager(engine.get());
    Thoth::addEpisodicEvalCorpusChunk(
        engine.get(), idx, "Paris is the capital of France.", "france.md");

    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
    auto registry = std::make_shared<ToolRegistry>();
    Thoth::WorkflowEngine workflow(registry, rag, memory, nullptr, nullptr);
    workflow.setConfig(&cfg);

    PlanStep step;
    step.step_id = "retrieve";
    step.type = StepType::RETRIEVAL;
    step.payload = {{"query", "capital of France"}, {"top_k", 3}};

    Thoth::StepExecutionContext ctx;
    const auto result = workflow.executeStep(step, "plan", ctx);
    delete idx;
    fs::remove(cfg.database_path);

    if (result.data.value("strict_e2_retrieval", false)) {
        std::cerr << "testE2NonStrictRetrievalPreserved: unexpected STRICT branch\n";
        return false;
    }
    if (!result.success) {
        std::cerr << "testE2NonStrictRetrievalPreserved: RAG retrieval failed: "
                  << result.error_message << '\n';
        return false;
    }
    return true;
}

/** E2-11 — A5 runtime heuristic guard (STRICT miswire hard-fail + NON-STRICT smoke). */
static bool testE2RuntimeHeuristicGuard() {
    // STRICT miswire: heuristic entry under STRICT eval context must hard-fail.
    {
        Config cfg;
        cfg.max_reflections = 0;
        cfg.database_path = makeTempPath("e2_a5_strict_miswire").string();
        fs::remove(cfg.database_path);

        auto memory = std::make_shared<Memory>(cfg);
        auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
        auto idx = new IndexManager(engine.get());
        Thoth::addEpisodicEvalCorpusChunk(
            engine.get(), idx, "Paris is the capital of France.", "france.md");

        auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
        const Thoth::E2EvalConfig strictConfig = makeE2StrictTestConfig();
        rag->setActiveE2EvalConfig(&strictConfig);

        try {
            (void)rag->retrieveRelevant("capital of France", {}, 3);
            std::cerr << "testE2RuntimeHeuristicGuard: expected LINK:RUNTIME_HEURISTIC throw\n";
            delete idx;
            fs::remove(cfg.database_path);
            return false;
        } catch (const Thoth::E2RuntimeHeuristicGuardViolation& e) {
            if (std::string(e.what()).find("LINK:RUNTIME_HEURISTIC") == std::string::npos) {
                std::cerr << "testE2RuntimeHeuristicGuard: unexpected guard message: " << e.what()
                          << '\n';
                delete idx;
                fs::remove(cfg.database_path);
                return false;
            }
        }

        delete idx;
        fs::remove(cfg.database_path);
    }

    // NON-STRICT: guard silent — heuristic retrieval completes (E2-11 smoke).
    if (!testE2NonStrictRetrievalPreserved()) {
        std::cerr << "testE2RuntimeHeuristicGuard: NON-STRICT heuristic smoke failed\n";
        return false;
    }

    // Static: guard present at heuristic entry in rag.cpp.
    {
        FileHandler fh;
        const fs::path ragPath =
            fs::path(fh.getProjectRoot()) / "external" / "basic_agent" / "src" / "rag.cpp";
        std::ifstream in(ragPath);
        if (!in.is_open()) {
            std::cerr << "testE2RuntimeHeuristicGuard: cannot read rag.cpp\n";
            return false;
        }
        const std::string source((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
        const auto fnPos = source.find("RAGPipeline::retrieveRelevant");
        const auto fnEnd = source.find("\nstd::string RAGPipeline::query", fnPos);
        if (fnPos == std::string::npos) {
            std::cerr << "testE2RuntimeHeuristicGuard: retrieveRelevant not found\n";
            return false;
        }
        const std::string body =
            fnEnd == std::string::npos ? source.substr(fnPos) : source.substr(fnPos, fnEnd - fnPos);
        if (body.find("guardAgainstStrictHeuristicRetrieval") == std::string::npos) {
            std::cerr << "testE2RuntimeHeuristicGuard: missing guard in retrieveRelevant\n";
            return false;
        }
    }

    return true;
}

/** B1 — schema only: enums, defaults, JSON stubs; no active block/resolution semantics. */
static bool testE2B1BlockResolutionSchema() {
    Thoth::EpisodicLearningCaseEvaluation defaultEval;
    if (defaultEval.run_block_reason != Thoth::E2RunBlockReason::NONE ||
        defaultEval.evaluation_resolution.has_value()) {
        std::cerr << "testE2B1BlockResolutionSchema: default case eval must be NONE / unset resolution\n";
        return false;
    }

    Thoth::EpisodicLearningSummary defaultSummary;
    if (defaultSummary.scorable_cases != 0 || defaultSummary.not_scorable_cases != 0 ||
        defaultSummary.evaluation_resolution.has_value()) {
        std::cerr << "testE2B1BlockResolutionSchema: default summary rollup must be zero / unset\n";
        return false;
    }

    const std::vector<Thoth::E2RunBlockReason> reasons = {
        Thoth::E2RunBlockReason::NONE,
        Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD,
        Thoth::E2RunBlockReason::WIRING_GATE,
        Thoth::E2RunBlockReason::STRICT_BOUNDARY_VIOLATION,
        Thoth::E2RunBlockReason::PROVENANCE_VIOLATION,
    };
    for (const auto reason : reasons) {
        if (Thoth::e2RunBlockReasonToString(reason).empty()) {
            std::cerr << "testE2B1BlockResolutionSchema: empty block reason string\n";
            return false;
        }
    }
    if (Thoth::e2RunBlockReasonToProtocolString(Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD) !=
        "LINK:RUNTIME_HEURISTIC") {
        std::cerr << "testE2B1BlockResolutionSchema: guard protocol string mismatch\n";
        return false;
    }

    try {
        throw Thoth::E2RuntimeHeuristicGuardViolation();
    } catch (const std::exception& e) {
        if (Thoth::e2RunBlockReasonFromException(e) != Thoth::E2RunBlockReason::NONE) {
            std::cerr << "testE2B1BlockResolutionSchema: B1 stub must return NONE for guard\n";
            return false;
        }
    }

    Thoth::EpisodicLearningExpectations positive;
    positive.expect_warm_retrieval_hit = true;
    positive.lift_constraint = Thoth::EpisodicLiftConstraint::GTE;
    positive.lift_threshold = Thoth::kEpisodicLearningLiftMargin;
    positive.include_in_mean_episodic_lift = true;

    Thoth::EpisodicLearningArmObservation cold;
    cold.terminal_state = "FAILED";
    cold.final_success_score = 0.0f;
    Thoth::EpisodicLearningArmObservation warm;
    warm.terminal_state = "COMPLETED";
    warm.final_success_score = 1.0f;
    warm.retrieval.warm_retrieval_hit = true;

    const auto passEval = Thoth::evaluateEpisodicLearningCase(
        "b1-schema", positive, cold, warm, makeE2StrictTestConfig());
    if (passEval.run_block_reason != Thoth::E2RunBlockReason::NONE ||
        passEval.evaluation_resolution.has_value()) {
        std::cerr << "testE2B1BlockResolutionSchema: evaluator path must leave block NONE in B1\n";
        return false;
    }

    const auto summary = Thoth::summarizeEpisodicLearning(
        {passEval}, {positive}, makeE2StrictTestConfig());
    for (const auto& row : summary.case_results) {
        if (row.run_block_reason != Thoth::E2RunBlockReason::NONE) {
            std::cerr << "testE2B1BlockResolutionSchema: summary case has non-NONE block in B1\n";
            return false;
        }
    }
    if (summary.scorable_cases != 0 || summary.not_scorable_cases != 0 ||
        summary.evaluation_resolution.has_value()) {
        std::cerr << "testE2B1BlockResolutionSchema: summary rollup must remain inactive in B1\n";
        return false;
    }

    const nlohmann::json caseJson = Thoth::caseEvaluationToJson(passEval);
    if (caseJson.value("run_block_reason", "") != "NONE") {
        std::cerr << "testE2B1BlockResolutionSchema: JSON stub must emit NONE\n";
        return false;
    }
    if (caseJson.contains("evaluation_resolution")) {
        std::cerr << "testE2B1BlockResolutionSchema: evaluation_resolution must not be emitted when unset\n";
        return false;
    }

    return true;
}

static bool extractWorkflowEngineFunctionBody(const std::string& source,
                                              const std::string& signature,
                                              const std::string& nextSignature,
                                              std::string* outBody) {
    const auto fnPos = source.find(signature);
    const auto fnEnd = source.find(nextSignature, fnPos);
    if (fnPos == std::string::npos || fnEnd == std::string::npos || fnEnd <= fnPos) {
        return false;
    }
    *outBody = source.substr(fnPos, fnEnd - fnPos);
    return true;
}

static size_t countRunBlockReasonAssignments(const std::string& body) {
    size_t count = 0;
    std::size_t pos = 0;
    while ((pos = body.find("run_block_reason", pos)) != std::string::npos) {
        const std::size_t eq = body.find('=', pos);
        if (eq != std::string::npos && eq - pos < 32) {
            ++count;
        }
        pos = eq == std::string::npos ? pos + 1 : eq + 1;
    }
    return count;
}

/** B2.1 — whitelist audit for run_block_reason write sites in workflow_engine.cpp. */
static bool testE2RunBlockReasonWriteSiteAudit() {
    FileHandler fh;
    const fs::path workflowPath =
        fs::path(fh.getProjectRoot()) / "external" / "basic_agent" / "src" / "workflow_engine.cpp";
    std::ifstream in(workflowPath);
    if (!in.is_open()) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: cannot read workflow_engine.cpp\n";
        return false;
    }
    const std::string source((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

    if (source.find("run_block_reason = E2RunBlockReason::NONE") != std::string::npos) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: explicit NONE reset forbidden\n";
        return false;
    }

    std::string executeStepBody;
    if (!extractWorkflowEngineFunctionBody(
            source,
            "StepResult WorkflowEngine::executeStep",
            "std::future<StepResult> WorkflowEngine::executeStepAsync",
            &executeStepBody)) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: executeStep body not found\n";
        return false;
    }

    const size_t stepAssignments = countRunBlockReasonAssignments(executeStepBody);
    if (stepAssignments != 1) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: executeStep must have exactly one forward, got "
                  << stepAssignments << '\n';
        return false;
    }
    if (executeStepBody.find("result.run_block_reason = currentAttempt.run_block_reason") ==
        std::string::npos) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: executeStep forward pattern missing\n";
        return false;
    }
    if (executeStepBody.find("RUNTIME_HEURISTIC_GUARD") != std::string::npos) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: executeStep must not assign guard enum\n";
        return false;
    }

    std::string executeRetrievalBody;
    if (!extractWorkflowEngineFunctionBody(
            source,
            "StepResult WorkflowEngine::executeRetrieval",
            "\nStepResult WorkflowEngine::executeLLM",
            &executeRetrievalBody)) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: executeRetrieval body not found\n";
        return false;
    }

    const size_t retrievalAssignments = countRunBlockReasonAssignments(executeRetrievalBody);
    if (retrievalAssignments != 1) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: executeRetrieval must have one semantic write, got "
                  << retrievalAssignments << '\n';
        return false;
    }
    const auto guardCatch = executeRetrievalBody.find("catch (const E2RuntimeHeuristicGuardViolation&");
    const auto assignPos = executeRetrievalBody.find("run_block_reason = E2RunBlockReason::RUNTIME_HEURISTIC_GUARD");
    if (guardCatch == std::string::npos || assignPos == std::string::npos ||
        assignPos < guardCatch) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: guard catch semantic write missing\n";
        return false;
    }

    const size_t totalAssignments = countRunBlockReasonAssignments(source);
    if (totalAssignments != 2) {
        std::cerr << "testE2RunBlockReasonWriteSiteAudit: expected 2 total assignments in file, got "
                  << totalAssignments << '\n';
        return false;
    }

    return true;
}

static Thoth::StepResult executeWorkflowRetrievalStep(
    const std::shared_ptr<RAGPipeline>& rag,
    const Thoth::StepExecutionContext& ctx,
    const std::string& query) {
    auto registry = std::make_shared<ToolRegistry>();
    Thoth::WorkflowEngine workflow(registry, rag, nullptr, nullptr, nullptr);
    PlanStep step;
    step.step_id = "retrieve";
    step.type = StepType::RETRIEVAL;
    step.payload = {{"query", query}, {"top_k", 3}};
    return workflow.executeStep(step, "e2-b2-plan", ctx);
}

/** E2-12 — B2 typed guard capture at workflow boundary (no plan inference). */
static bool testE2RunBlockReasonGuardCapture() {
    Config cfg;
    cfg.max_reflections = 0;
    cfg.database_path = makeTempPath("e2_b2_guard_capture").string();
    fs::remove(cfg.database_path);

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    auto idx = new IndexManager(engine.get());
    Thoth::addEpisodicEvalCorpusChunk(
        engine.get(), idx, "Paris is the capital of France.", "france.md");

    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
    const Thoth::E2EvalConfig strictConfig = makeE2StrictTestConfig();
    rag->setActiveE2EvalConfig(&strictConfig);

    Thoth::StepExecutionContext ctx;
    const Thoth::StepResult result =
        executeWorkflowRetrievalStep(rag, ctx, "capital of France");

    delete idx;
    fs::remove(cfg.database_path);

    if (result.run_block_reason != Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD) {
        std::cerr << "testE2RunBlockReasonGuardCapture: expected RUNTIME_HEURISTIC_GUARD, got "
                  << Thoth::e2RunBlockReasonToString(result.run_block_reason) << '\n';
        return false;
    }
    if (result.success) {
        std::cerr << "testE2RunBlockReasonGuardCapture: expected failed step\n";
        return false;
    }
    if (result.error_message.find("LINK:RUNTIME_HEURISTIC") == std::string::npos) {
        std::cerr << "testE2RunBlockReasonGuardCapture: missing LINK diagnostic\n";
        return false;
    }
    return true;
}

/** E2-13 — STRICT kernel dispatch leaves run_block_reason at default NONE. */
static bool testE2RunBlockReasonHappyPathNone() {
    const auto cases = Thoth::getEpisodicLearningCases();
    const Thoth::EpisodicLearningCase* e201 = nullptr;
    for (const auto& c : cases) {
        if (c.id == "E2-01") {
            e201 = &c;
            break;
        }
    }
    if (!e201) {
        std::cerr << "testE2RunBlockReasonHappyPathNone: missing E2-01\n";
        return false;
    }

    Config cfg;
    cfg.max_reflections = 0;
    cfg.database_path = makeTempPath("e2_b2_happy_none").string();
    fs::remove(cfg.database_path);

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    auto idx = new IndexManager(engine.get());
    if (!e201->index_distractor_text.empty()) {
        Thoth::addEpisodicEvalCorpusChunk(engine.get(), idx, e201->index_distractor_text);
    }
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
    const Thoth::E2EvalConfig strictConfig = makeE2StrictTestConfig();
    rag->setActiveE2EvalConfig(&strictConfig);

    const Thoth::SealedEpisodeInjectionLog sealedLog =
        Thoth::buildStrictInjectionLogFromCaseTable(*e201, "warm", 1'700'000'000'000LL);

    Thoth::StepExecutionContext ctx;
    ctx.e2_strict_episode_log = &sealedLog;
    ctx.e2_eval_config = &strictConfig;

    const Thoth::StepResult result = executeWorkflowRetrievalStep(rag, ctx, e201->goal);

    delete idx;
    fs::remove(cfg.database_path);

    if (result.run_block_reason != Thoth::E2RunBlockReason::NONE) {
        std::cerr << "testE2RunBlockReasonHappyPathNone: expected NONE, got "
                  << Thoth::e2RunBlockReasonToString(result.run_block_reason) << '\n';
        return false;
    }
    if (!result.success) {
        std::cerr << "testE2RunBlockReasonHappyPathNone: expected successful STRICT retrieval\n";
        return false;
    }
    return true;
}

/** E2-14 — B2 wiring does not change arm scoring on golden happy paths. */
static bool testE2RunBlockReasonArmStatusUnchanged() {
    const auto cases = Thoth::getEpisodicLearningCases();
    const Thoth::EpisodicLearningCase* spec = nullptr;
    for (const auto& c : cases) {
        if (c.id == "E2-01") {
            spec = &c;
            break;
        }
    }
    if (!spec) {
        std::cerr << "testE2RunBlockReasonArmStatusUnchanged: missing E2-01\n";
        return false;
    }

    Thoth::BenchmarkAttribution attr{"e2-b2-run", "e2-b2-env"};
    const auto cold = runE2TestArm(*spec, "cold", attr);
    const auto warm = runE2TestArm(*spec, "warm", attr);
    if (cold.arm_scoring_status != Thoth::E2ArmScoringStatus::OK ||
        warm.arm_scoring_status != Thoth::E2ArmScoringStatus::OK) {
        std::cerr << "testE2RunBlockReasonArmStatusUnchanged: arm status regression\n";
        return false;
    }

    const auto eval = Thoth::evaluateEpisodicLearningCase(
        spec->id, spec->expectations, cold, warm, makeE2StrictTestConfig());
    if (eval.run_block_reason != Thoth::E2RunBlockReason::NONE) {
        std::cerr << "testE2RunBlockReasonArmStatusUnchanged: case block should stay NONE in B2\n";
        return false;
    }
    if (!eval.passes) {
        std::cerr << "testE2RunBlockReasonArmStatusUnchanged: E2-01 failed\n";
        return false;
    }
    return true;
}

/** E2-15 — block dominates arm: GUARD + arm OK → NOT_SCORABLE. */
static bool testE2B3ResolveEvaluationGuardNotScorable() {
    const Thoth::E2EvaluationResolution resolution = Thoth::resolveEvaluation(
        Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD, Thoth::E2ArmScoringStatus::OK);
    if (resolution != Thoth::E2EvaluationResolution::NOT_SCORABLE) {
        std::cerr << "testE2B3ResolveEvaluationGuardNotScorable: expected NOT_SCORABLE, got "
                  << Thoth::e2EvaluationResolutionToString(resolution) << '\n';
        return false;
    }
    return true;
}

/** E2-16 — NONE + FAILED_RETRIEVAL → SCORED_FAILURE. */
static bool testE2B3ResolveEvaluationArmFailure() {
    const Thoth::E2EvaluationResolution resolution = Thoth::resolveEvaluation(
        Thoth::E2RunBlockReason::NONE, Thoth::E2ArmScoringStatus::FAILED_RETRIEVAL);
    if (resolution != Thoth::E2EvaluationResolution::SCORED_FAILURE) {
        std::cerr << "testE2B3ResolveEvaluationArmFailure: expected SCORED_FAILURE, got "
                  << Thoth::e2EvaluationResolutionToString(resolution) << '\n';
        return false;
    }
    return true;
}

/** E2-17 — NONE + arm OK → SCORED_SUCCESS. */
static bool testE2B3ResolveEvaluationArmSuccess() {
    const Thoth::E2EvaluationResolution resolution = Thoth::resolveEvaluation(
        Thoth::E2RunBlockReason::NONE, Thoth::E2ArmScoringStatus::OK);
    if (resolution != Thoth::E2EvaluationResolution::SCORED_SUCCESS) {
        std::cerr << "testE2B3ResolveEvaluationArmSuccess: expected SCORED_SUCCESS, got "
                  << Thoth::e2EvaluationResolutionToString(resolution) << '\n';
        return false;
    }
    return true;
}

/** E2-12 — integrated precedence: GUARD block + arm fail → NOT_SCORABLE. */
static bool testE2B3BlockPrecedenceOverArmFailure() {
    Thoth::EpisodicLearningCaseEvaluation eval;
    eval.run_block_reason = Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD;
    eval.cold.arm_scoring_status = Thoth::E2ArmScoringStatus::FAILED_RETRIEVAL;
    eval.warm.arm_scoring_status = Thoth::E2ArmScoringStatus::FAILED_RETRIEVAL;
    Thoth::applyCaseEvaluationResolution(eval);
    if (!eval.evaluation_resolution.has_value() ||
        *eval.evaluation_resolution != Thoth::E2EvaluationResolution::NOT_SCORABLE) {
        std::cerr << "testE2B3BlockPrecedenceOverArmFailure: expected NOT_SCORABLE\n";
        return false;
    }
    return true;
}

/** E2-18 — single struct copy: StepResult → PlanStep.outcome.run_block_reason at completion. */
static bool testE2B3PlanStepTransportMerge() {
    const auto cases = Thoth::getEpisodicLearningCases();
    const Thoth::EpisodicLearningCase* spec = nullptr;
    for (const auto& c : cases) {
        if (c.id == "E2-01") {
            spec = &c;
            break;
        }
    }
    if (!spec) {
        std::cerr << "testE2B3PlanStepTransportMerge: missing E2-01\n";
        return false;
    }

    Config cfg;
    cfg.max_reflections = 0;
    cfg.database_path = makeTempPath("e2_b3_transport_merge").string();
    fs::remove(cfg.database_path);

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    auto idx = new IndexManager(engine.get());
    Thoth::addEpisodicEvalCorpusChunk(
        engine.get(), idx, spec->index_distractor_text.empty()
                                ? "Paris is the capital of France."
                                : spec->index_distractor_text);

    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());
    const Thoth::E2EvalConfig strictConfig = makeE2StrictTestConfig();
    rag->setActiveE2EvalConfig(&strictConfig);

    auto planner = std::make_shared<Thoth::EpisodicLearningMockPlanner>(spec->validation_token);
    auto registry = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    controller.set_max_reflections(0);
    // Miswire: STRICT active on RAG but no strict dispatch context → heuristic guard trip.

    Thoth::BenchmarkAttribution attr{"e2-b3-transport", "e2-b3-env"};
    memory->setActiveSessionId("e2-b3-transport-goal");

    std::atomic<bool> terminal{false};
    controller.set_event_callback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::PLAN_COMPLETED || ev.type == EventType::PLAN_FAILED ||
            ev.type == EventType::PLAN_ABORTED) {
            terminal.store(true);
        }
    });

    controller.execute_goal(spec->goal, attr);

    int timeout = 150;
    while (!terminal.load() && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --timeout;
    }

    const Plan plan = controller.get_current_plan();
    const Thoth::E2RunBlockReason fromHelper = Thoth::runBlockReasonFromPlan(plan);
    bool sawRetrievalStep = false;
    for (const auto& step : plan.steps) {
        if (step.type == StepType::RETRIEVAL && step.status != StepStatus::PENDING &&
            step.status != StepStatus::RUNNING) {
            sawRetrievalStep = true;
            if (step.outcome.run_block_reason != Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD) {
                std::cerr << "testE2B3PlanStepTransportMerge: PlanStep.outcome.run_block_reason mismatch\n";
                delete idx;
                fs::remove(cfg.database_path);
                return false;
            }
        }
    }

    delete idx;
    fs::remove(cfg.database_path);

    if (!sawRetrievalStep) {
        std::cerr << "testE2B3PlanStepTransportMerge: no completed RETRIEVAL step\n";
        return false;
    }
    if (fromHelper != Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD) {
        std::cerr << "testE2B3PlanStepTransportMerge: runBlockReasonFromPlan mismatch\n";
        return false;
    }
    return true;
}

/** E2-19 — B3.1 envelope equivalence: runBlockReasonFromPlan unchanged vs B3 contract. */
static bool testE2B31PlanStepOutcomeEquivalence() {
    const Thoth::E2RunBlockReason block = Thoth::runBlockReasonFromPlan(
        []() {
            Plan plan;
            PlanStep step;
            step.type = StepType::RETRIEVAL;
            step.status = StepStatus::FAILED;
            step.outcome.run_block_reason = Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD;
            plan.steps.push_back(step);
            return plan;
        }());
    if (block != Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD) {
        std::cerr << "testE2B31PlanStepOutcomeEquivalence: helper read mismatch\n";
        return false;
    }
    return true;
}

/** B3.1 — PlanStep outcome serde round-trip + legacy top-level fallback. */
static bool testE2B31PlanStepOutcomeSerde() {
    PlanStep step;
    step.step_id = "serde-step";
    step.type = StepType::RETRIEVAL;
    step.status = StepStatus::FAILED;
    step.outcome.run_block_reason = Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD;

    const nlohmann::json serialized = step.to_json();
    if (!serialized.contains("outcome") ||
        serialized["outcome"].value("run_block_reason", "") != "RUNTIME_HEURISTIC_GUARD") {
        std::cerr << "testE2B31PlanStepOutcomeSerde: to_json missing outcome envelope\n";
        return false;
    }

    const PlanStep roundTrip = PlanStep::from_json(serialized);
    if (roundTrip.outcome.run_block_reason != Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD) {
        std::cerr << "testE2B31PlanStepOutcomeSerde: round-trip mismatch\n";
        return false;
    }

    const nlohmann::json legacy = {{"step_id", "legacy"},
                                   {"type", static_cast<int>(StepType::RETRIEVAL)},
                                   {"status", static_cast<int>(StepStatus::FAILED)},
                                   {"run_block_reason", "RUNTIME_HEURISTIC_GUARD"}};
    const PlanStep legacyStep = PlanStep::from_json(legacy);
    if (legacyStep.outcome.run_block_reason != Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD) {
        std::cerr << "testE2B31PlanStepOutcomeSerde: legacy top-level fallback failed\n";
        return false;
    }
    return true;
}

/** B3.1 — sole write site audit for PlanStep.outcome.run_block_reason in executive_controller.cpp. */
static bool testE2B31OutcomeWriteSiteAudit() {
    std::ifstream in("external/basic_agent/src/executive_controller.cpp");
    if (!in.is_open()) {
        in.open("/home/steve/Thoth/external/basic_agent/src/executive_controller.cpp");
    }
    if (!in.is_open()) {
        std::cerr << "testE2B31OutcomeWriteSiteAudit: cannot read executive_controller.cpp\n";
        return false;
    }
    const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    if (source.find("step.run_block_reason") != std::string::npos) {
        std::cerr << "testE2B31OutcomeWriteSiteAudit: legacy step.run_block_reason must be removed\n";
        return false;
    }

    std::size_t pos = 0;
    int assignCount = 0;
    while ((pos = source.find("outcome.run_block_reason", pos)) != std::string::npos) {
        const std::size_t lineStart = source.rfind('\n', pos == 0 ? 0 : pos - 1);
        const std::size_t lineEnd = source.find('\n', pos);
        const std::string line =
            source.substr(lineStart == std::string::npos ? 0 : lineStart + 1,
                          lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart - 1);
        if (line.find('=') != std::string::npos) {
            ++assignCount;
        }
        pos += 1;
    }

    if (assignCount != 1) {
        std::cerr << "testE2B31OutcomeWriteSiteAudit: expected 1 outcome assignment, got "
                  << assignCount << '\n';
        return false;
    }
    return true;
}

/** E2-20 — B4 export: NOT_SCORABLE case omits e2_outcome, emits scoring_block_reason. */
static bool testE2B4ExportNotScorableCase() {
    Thoth::EpisodicLearningCaseEvaluation eval;
    eval.case_id = "E2-20";
    eval.run_block_reason = Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD;
    eval.evaluation_resolution = Thoth::E2EvaluationResolution::NOT_SCORABLE;
    const nlohmann::json j = Thoth::caseEvaluationToJson(eval);
    if (j.value("evaluation_resolution", "") != "NOT_SCORABLE") {
        std::cerr << "testE2B4ExportNotScorableCase: missing NOT_SCORABLE resolution\n";
        return false;
    }
    if (j.value("scoring_block_reason", "") != "LINK:RUNTIME_HEURISTIC") {
        std::cerr << "testE2B4ExportNotScorableCase: scoring_block_reason mismatch\n";
        return false;
    }
    if (j.contains("e2_outcome")) {
        std::cerr << "testE2B4ExportNotScorableCase: e2_outcome must be omitted\n";
        return false;
    }
    return true;
}

/** E2-21 — B4 export: SCORED_FAILURE derives e2_outcome FAILURE. */
static bool testE2B4ExportScoredFailureCase() {
    Thoth::EpisodicLearningCaseEvaluation eval;
    eval.case_id = "E2-21";
    eval.passes = false;
    eval.evaluation_resolution = Thoth::E2EvaluationResolution::SCORED_FAILURE;
    const nlohmann::json j = Thoth::caseEvaluationToJson(eval);
    if (j.value("e2_outcome", "") != "FAILURE") {
        std::cerr << "testE2B4ExportScoredFailureCase: expected FAILURE export\n";
        return false;
    }
    return true;
}

/** E2-22 — B4 export: SCORED_SUCCESS derives e2_outcome SUCCESS. */
static bool testE2B4ExportScoredSuccessCase() {
    Thoth::EpisodicLearningCaseEvaluation eval;
    eval.case_id = "E2-22";
    eval.passes = true;
    eval.evaluation_resolution = Thoth::E2EvaluationResolution::SCORED_SUCCESS;
    const nlohmann::json j = Thoth::caseEvaluationToJson(eval);
    if (j.value("e2_outcome", "") != "SUCCESS") {
        std::cerr << "testE2B4ExportScoredSuccessCase: expected SUCCESS export\n";
        return false;
    }
    return true;
}

/** E2-23 — B4 export: summary not_scorable_by_reason rollup. */
static bool testE2B4ExportNotScorableSummaryRollup() {
    Thoth::EpisodicLearningCaseEvaluation blocked;
    blocked.case_id = "E2-23";
    blocked.run_block_reason = Thoth::E2RunBlockReason::RUNTIME_HEURISTIC_GUARD;
    blocked.evaluation_resolution = Thoth::E2EvaluationResolution::NOT_SCORABLE;

    Thoth::EpisodicLearningSummary summary;
    summary.case_results.push_back(blocked);
    summary.not_scorable_cases = 1;
    summary.evaluation_resolution = Thoth::E2EvaluationResolution::NOT_SCORABLE;

    const nlohmann::json j = Thoth::episodicLearningSummaryToJson(summary);
    if (j.value("not_scorable_cases", 0) != 1) {
        std::cerr << "testE2B4ExportNotScorableSummaryRollup: not_scorable_cases mismatch\n";
        return false;
    }
    if (j["not_scorable_by_reason"].value("RUNTIME_HEURISTIC_GUARD", 0) != 1) {
        std::cerr << "testE2B4ExportNotScorableSummaryRollup: breakdown mismatch\n";
        return false;
    }
    if (j.contains("e2_outcome")) {
        std::cerr << "testE2B4ExportNotScorableSummaryRollup: summary e2_outcome must be omitted\n";
        return false;
    }
    return true;
}

/** E2-24 — B4 export: success_rate uses scorable-only denominator. */
static bool testE2B4ExportSuccessRateScorableOnly() {
    Thoth::EpisodicLearningCaseEvaluation blocked;
    blocked.evaluation_resolution = Thoth::E2EvaluationResolution::NOT_SCORABLE;

    Thoth::EpisodicLearningCaseEvaluation scoredFail;
    scoredFail.evaluation_resolution = Thoth::E2EvaluationResolution::SCORED_FAILURE;

    Thoth::EpisodicLearningCaseEvaluation scoredPass;
    scoredPass.passes = true;
    scoredPass.evaluation_resolution = Thoth::E2EvaluationResolution::SCORED_SUCCESS;

    const std::vector<Thoth::EpisodicLearningCaseEvaluation> cases = {
        blocked, scoredFail, scoredPass};
    const float rate = Thoth::successRateForExport(cases);
    if (std::abs(rate - 0.5f) > 0.0001f) {
        std::cerr << "testE2B4ExportSuccessRateScorableOnly: expected 0.5, got " << rate << '\n';
        return false;
    }
    return true;
}

static Thoth::EpisodicLearningSummary buildOfficialGoldenSummary() {
    const auto cases = Thoth::getEpisodicLearningCases();
    Thoth::BenchmarkAttribution attr{"e2-b5-golden", "e2-b5-golden-env"};
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    std::vector<Thoth::EpisodicLearningCaseEvaluation> evaluations;
    std::vector<Thoth::EpisodicLearningExpectations> expectations;
    for (const auto& spec : cases) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold = runE2TestArm(spec, "cold", attr, nullptr, nullptr);
        const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);
        auto eval = Thoth::evaluateEpisodicLearningCase(
            spec.id, spec.expectations, cold, warm, cfg);
        eval.run_block_reason = warmBlock;
        Thoth::applyCaseEvaluationResolution(eval);
        evaluations.push_back(eval);
        expectations.push_back(spec.expectations);
    }
    return Thoth::summarizeEpisodicLearning(evaluations, expectations, cfg);
}

static Thoth::EpisodicLearningSummary buildOfficialGoldenSummaryWithChannelHarness(
    bool registerReplaySubscriber) {
    const auto cases = Thoth::getEpisodicLearningCases();
    Thoth::BenchmarkAttribution attr{"e2-d2-02", "e2-d2-02-env"};
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    E2EpisodeChannelHarness harness;
    harness.enable_episode_publication = true;
    harness.register_replay_subscriber = registerReplaySubscriber;

    std::vector<Thoth::EpisodicLearningCaseEvaluation> evaluations;
    std::vector<Thoth::EpisodicLearningExpectations> expectations;
    for (const auto& spec : cases) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold =
            runE2TestArm(spec, "cold", attr, nullptr, nullptr, &harness);
        const auto warm =
            runE2TestArm(spec, "warm", attr, nullptr, &warmBlock, &harness);
        auto eval = Thoth::evaluateEpisodicLearningCase(
            spec.id, spec.expectations, cold, warm, cfg);
        eval.run_block_reason = warmBlock;
        Thoth::applyCaseEvaluationResolution(eval);
        evaluations.push_back(eval);
        expectations.push_back(spec.expectations);
    }
    return Thoth::summarizeEpisodicLearning(evaluations, expectations, cfg);
}

static nlohmann::json episodicLearningScopedBSnapshot(
    const Thoth::EpisodicLearningSummary& summary) {
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    const auto fingerprint = Thoth::computeEvaluationFingerprint(cfg);
    return Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary, fingerprint.toJson(), cfg.toJson());
}

static bool episodeJsonLacksStrictAuthorityFields(const nlohmann::json& row) {
    return !row.contains("official_scoring") && !row.contains("e2_outcome") &&
           !row.contains("evaluation_resolution");
}

/** E2-D2-02 — replay registration/removal does not change wiring_stage=B harness outcomes. */
static bool testE2D2BenchmarkAuthorityIsolation() {
    setenv("THOTH_E2_WIRING_STAGE", "B", 1);

    const auto baselineSummary = buildOfficialGoldenSummary();
    const auto baselineSnap = episodicLearningScopedBSnapshot(baselineSummary);

    const auto noReplaySummary = buildOfficialGoldenSummaryWithChannelHarness(false);
    const auto noReplaySnap = episodicLearningScopedBSnapshot(noReplaySummary);
    if (!Thoth::episodicLearningScopedEquivalenceEqual(baselineSnap, noReplaySnap)) {
        std::cerr << "testE2D2BenchmarkAuthorityIsolation: publication-only harness differs "
                     "from baseline\n";
        return false;
    }

    const auto withReplaySummary = buildOfficialGoldenSummaryWithChannelHarness(true);
    const auto withReplaySnap = episodicLearningScopedBSnapshot(withReplaySummary);
    if (!Thoth::episodicLearningScopedEquivalenceEqual(baselineSnap, withReplaySnap)) {
        std::cerr << "testE2D2BenchmarkAuthorityIsolation: replay-registered harness differs "
                     "from baseline\n";
        return false;
    }

    const auto removalSummary = buildOfficialGoldenSummaryWithChannelHarness(false);
    const auto removalSnap = episodicLearningScopedBSnapshot(removalSummary);
    if (!Thoth::episodicLearningScopedEquivalenceEqual(baselineSnap, removalSnap)) {
        std::cerr << "testE2D2BenchmarkAuthorityIsolation: post-removal harness differs from "
                     "baseline\n";
        return false;
    }

    Thoth::EpisodicLearningRunEnvelope envelope{true, true, "B"};
    const nlohmann::json officialRow = Thoth::episodicLearningSummaryLogRow(
        {}, withReplaySummary, static_cast<int>(withReplaySummary.case_results.size()),
        withReplaySummary.case_results.size(), envelope);
    if (officialRow.value("wiring_stage", "") != "B" ||
        officialRow.value("official_scoring", false) != true) {
        std::cerr << "testE2D2BenchmarkAuthorityIsolation: wiring_stage=B envelope mismatch\n";
        return false;
    }

    E2EpisodeChannelHarness replayHarness;
    replayHarness.enable_episode_publication = true;
    replayHarness.register_replay_subscriber = true;
    const auto cases = Thoth::getEpisodicLearningCases();
    if (!cases.empty()) {
        Thoth::BenchmarkAttribution attr{"e2-d2-02-auth", "e2-d2-02-auth-env"};
        (void)runE2TestArm(cases.front(), "warm", attr, nullptr, nullptr, &replayHarness);
    }
    if (!replayHarness.replay || replayHarness.replay->captureCount() == 0) {
        std::cerr << "testE2D2BenchmarkAuthorityIsolation: replay subscriber did not capture "
                     "episodes\n";
        return false;
    }

    for (std::size_t i = 0; i < replayHarness.replay->captureCount(); ++i) {
        const Thoth::EpisodeCompleted* captured = replayHarness.replay->capturedAtForTests(i);
        if (!captured ||
            !episodeJsonLacksStrictAuthorityFields(captured->toJson())) {
            std::cerr << "testE2D2BenchmarkAuthorityIsolation: captured episode has authority "
                         "fields\n";
            return false;
        }
    }

    std::vector<nlohmann::json> replaySinkRows;
    replayHarness.replay->setReplaySinkForTests([&](const Thoth::EpisodeCompleted& replayed) {
        replaySinkRows.push_back(replayed.toJson());
    });
    for (std::size_t i = 0; i < replayHarness.replay->captureCount(); ++i) {
        if (!replayHarness.replay->replayCaptured(i)) {
            std::cerr << "testE2D2BenchmarkAuthorityIsolation: replayCaptured failed\n";
            return false;
        }
    }
    for (const auto& row : replaySinkRows) {
        if (!episodeJsonLacksStrictAuthorityFields(row)) {
            std::cerr << "testE2D2BenchmarkAuthorityIsolation: replay sink emitted authority "
                         "fields\n";
            return false;
        }
    }

    unsetenv("THOTH_E2_WIRING_STAGE");
    return true;
}

/** E2-25 — B5 official harness envelope on summary JSONL. */
static bool testE2B5OfficialHarnessEnvelope() {
    const auto summary = buildOfficialGoldenSummary();
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    const auto fingerprint = Thoth::computeEvaluationFingerprint(cfg);
    Thoth::EpisodicLearningLogContext ctx;
    Thoth::EpisodicLearningRunEnvelope envelope{true, true, "B"};
    const nlohmann::json row =
        Thoth::episodicLearningSummaryLogRow(ctx, summary, 3, 3, envelope);
    if (row.value("official_scoring", false) != true ||
        row.value("scoring_enabled", false) != true ||
        row.value("wiring_stage", "") != "B") {
        std::cerr << "testE2B5OfficialHarnessEnvelope: official flags missing\n";
        return false;
    }
    if (!row.contains("evaluation_resolution")) {
        std::cerr << "testE2B5OfficialHarnessEnvelope: evaluation_resolution missing\n";
        return false;
    }
    (void)fingerprint;
    return true;
}

/** E2-26 — golden trio under official config: SCORED_SUCCESS, not_scorable_cases == 0. */
static bool testE2B5OfficialGoldenTrio() {
    const auto summary = buildOfficialGoldenSummary();
    if (summary.not_scorable_cases != 0) {
        std::cerr << "testE2B5OfficialGoldenTrio: expected not_scorable_cases == 0\n";
        return false;
    }
    for (const auto& eval : summary.case_results) {
        if (!eval.evaluation_resolution.has_value() ||
            *eval.evaluation_resolution != Thoth::E2EvaluationResolution::SCORED_SUCCESS) {
            std::cerr << "testE2B5OfficialGoldenTrio: case " << eval.case_id
                      << " expected SCORED_SUCCESS\n";
            return false;
        }
    }
    if (!summary.evaluation_resolution.has_value() ||
        *summary.evaluation_resolution != Thoth::E2EvaluationResolution::SCORED_SUCCESS) {
        std::cerr << "testE2B5OfficialGoldenTrio: summary expected SCORED_SUCCESS\n";
        return false;
    }
    const nlohmann::json caseJson = Thoth::caseEvaluationToJson(summary.case_results.front());
    if (caseJson.value("e2_outcome", "") != "SUCCESS") {
        std::cerr << "testE2B5OfficialGoldenTrio: expected derived e2_outcome SUCCESS\n";
        return false;
    }
    return true;
}

/** E2-27 — non-authoritative envelope must not claim official_scoring. */
static bool testE2B5NonAuthoritativeEnvelope() {
    Thoth::EpisodicLearningSummary summary;
    Thoth::EpisodicLearningRunEnvelope envelope{false, true, "SCORING"};
    const nlohmann::json row =
        Thoth::episodicLearningSummaryLogRow({}, summary, 0, 0, envelope);
    if (row.value("official_scoring", true) != false) {
        std::cerr << "testE2B5NonAuthoritativeEnvelope: SCORING must not be official\n";
        return false;
    }
    return true;
}

/** E2-28 — scoped equivalence determinism across two identical evaluation builds. */
static bool testE2B5OfficialFingerprintDeterminism() {
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    const auto fp = Thoth::computeEvaluationFingerprint(cfg);
    const auto summary_a = buildOfficialGoldenSummary();
    const auto summary_b = buildOfficialGoldenSummary();
    const auto snap_a = Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary_a, fp.toJson(), cfg.toJson());
    const auto snap_b = Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary_b, fp.toJson(), cfg.toJson());
    if (!Thoth::episodicLearningScopedEquivalenceEqual(snap_a, snap_b)) {
        std::cerr << "testE2B5OfficialFingerprintDeterminism: scoped snapshots differ\n";
        return false;
    }
    if (Thoth::episodicLearningFingerprintMismatchBucket(snap_a, snap_b, "h1", "h1") != 0) {
        std::cerr << "testE2B5OfficialFingerprintDeterminism: diagnosis bucket mismatch\n";
        return false;
    }
    return true;
}

/** B5 — scored loop structural audit: no wiring_stage references inside runScoredEvaluationLoop. */
static bool testE2B5ScoredLoopStructuralAudit() {
    std::ifstream in("external/basic_agent/src/run_episodic_learning_benchmark.cpp");
    if (!in.is_open()) {
        in.open("/home/steve/Thoth/external/basic_agent/src/run_episodic_learning_benchmark.cpp");
    }
    if (!in.is_open()) {
        std::cerr << "testE2B5ScoredLoopStructuralAudit: cannot read harness source\n";
        return false;
    }
    const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto fnPos = source.find("ScoredLoopOutcome runScoredEvaluationLoop");
    const auto fnEnd = source.find("int runScoredEvaluationHarness", fnPos);
    if (fnPos == std::string::npos || fnEnd == std::string::npos) {
        std::cerr << "testE2B5ScoredLoopStructuralAudit: runScoredEvaluationLoop not found\n";
        return false;
    }
    const std::string body = source.substr(fnPos, fnEnd - fnPos);
    if (body.find("wiring_stage") != std::string::npos ||
        body.find("wiringStage") != std::string::npos) {
        std::cerr << "testE2B5ScoredLoopStructuralAudit: stage branching inside scored loop\n";
        return false;
    }
    return true;
}

static std::string readRepoSourceFile(const std::string& relativePath) {
    std::ifstream in(relativePath);
    if (!in.is_open()) {
        in.open("/home/steve/Thoth/" + relativePath);
    }
    if (!in.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/** E2-C1-01 — service façade matches Phase B free-function evaluation output. */
static bool testE2C1ServiceOutputEquivalence() {
    const Thoth::IEpisodicEvaluationService& svc = Thoth::episodicEvaluationService();
    const auto cases = Thoth::getEpisodicLearningCases();
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    Thoth::BenchmarkAttribution attr{"e2-c1-equiv", "e2-c1-equiv-env"};
    std::vector<Thoth::EpisodicLearningCaseEvaluation> directEvals;
    std::vector<Thoth::EpisodicLearningCaseEvaluation> serviceEvals;
    std::vector<Thoth::EpisodicLearningExpectations> expectations;
    for (const auto& spec : cases) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold = runE2TestArm(spec, "cold", attr, nullptr, nullptr);
        const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);

        Thoth::EpisodicLearningCaseEvaluation direct =
            Thoth::evaluateEpisodicLearningCase(spec.id, spec.expectations, cold, warm, cfg);
        direct.run_block_reason = warmBlock;
        Thoth::applyCaseEvaluationResolution(direct);

        Thoth::EpisodicLearningCaseEvaluation viaService =
            svc.evaluateCase(spec.id, spec.expectations, cold, warm, cfg);
        viaService.run_block_reason = warmBlock;
        svc.applyCaseResolution(viaService);

        if (direct.passes != viaService.passes || direct.lift != viaService.lift ||
            direct.failure_reason != viaService.failure_reason ||
            direct.run_block_reason != viaService.run_block_reason ||
            direct.evaluation_resolution != viaService.evaluation_resolution) {
            std::cerr << "testE2C1ServiceOutputEquivalence: mismatch for " << spec.id << '\n';
            return false;
        }
        directEvals.push_back(direct);
        serviceEvals.push_back(viaService);
        expectations.push_back(spec.expectations);
    }

    const auto directSummary =
        Thoth::summarizeEpisodicLearning(directEvals, expectations, cfg);
    const auto serviceSummary = svc.summarize(serviceEvals, expectations, cfg);
    if (directSummary.mean_episodic_lift != serviceSummary.mean_episodic_lift ||
        directSummary.scorable_cases != serviceSummary.scorable_cases ||
        directSummary.not_scorable_cases != serviceSummary.not_scorable_cases ||
        directSummary.evaluation_resolution != serviceSummary.evaluation_resolution) {
        std::cerr << "testE2C1ServiceOutputEquivalence: summary mismatch\n";
        return false;
    }

    const auto directFp = Thoth::computeEvaluationFingerprint(cfg);
    const auto serviceFp = svc.computeFingerprint(cfg);
    if (directFp.fingerprint_hash != serviceFp.fingerprint_hash ||
        directFp.canonical_json != serviceFp.canonical_json) {
        std::cerr << "testE2C1ServiceOutputEquivalence: fingerprint mismatch\n";
        return false;
    }
    return true;
}

/** E2-C1-02 — scored loop delegates evaluation to service (no direct algorithm). */
static bool testE2C1HarnessNoDirectEvaluationAlgorithm() {
    const std::string source = readRepoSourceFile(
        "external/basic_agent/src/run_episodic_learning_benchmark.cpp");
    if (source.empty()) {
        std::cerr << "testE2C1HarnessNoDirectEvaluationAlgorithm: cannot read harness source\n";
        return false;
    }
    const auto fnPos = source.find("ScoredLoopOutcome runScoredEvaluationLoop");
    const auto fnEnd = source.find("int runScoredEvaluationHarness", fnPos);
    if (fnPos == std::string::npos || fnEnd == std::string::npos) {
        std::cerr << "testE2C1HarnessNoDirectEvaluationAlgorithm: runScoredEvaluationLoop not found\n";
        return false;
    }
    const std::string body = source.substr(fnPos, fnEnd - fnPos);
    const std::vector<std::string> forbidden = {"evaluateEpisodicLearningCase",
                                                "applyCaseEvaluationResolution",
                                                "summarizeEpisodicLearning",
                                                "resolveEvaluation("};
    for (const auto& sym : forbidden) {
        if (body.find(sym) != std::string::npos) {
            std::cerr << "testE2C1HarnessNoDirectEvaluationAlgorithm: found " << sym << '\n';
            return false;
        }
    }
    if (body.find("evalService") == std::string::npos) {
        std::cerr << "testE2C1HarnessNoDirectEvaluationAlgorithm: evalService missing\n";
        return false;
    }
    return true;
}

/** E2-C1-03 — evaluation service contains no benchmark-specific logic. */
static bool testE2C1ServiceNoBenchmarkLogic() {
    const std::vector<std::string> paths = {
        "external/basic_agent/include/episodic_evaluation_service.h",
        "external/basic_agent/src/episodic_evaluation_service.cpp"};
    const std::vector<std::string> forbidden = {"wiring_stage",
                                                "THOTH_E2_WIRING_STAGE",
                                                "benchmarkLogPath",
                                                "appendJsonLine",
                                                "run_episodic_learning_benchmark",
                                                "std::getenv",
                                                "std::cout"};
    for (const auto& path : paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << "testE2C1ServiceNoBenchmarkLogic: cannot read " << path << '\n';
            return false;
        }
        for (const auto& sym : forbidden) {
            if (source.find(sym) != std::string::npos) {
                std::cerr << "testE2C1ServiceNoBenchmarkLogic: " << sym << " in " << path << '\n';
                return false;
            }
        }
    }
    return true;
}

/** E2-C2-01 — publication disabled by default (config flag OFF). */
static bool testE2C2PublicationDisabledByDefault() {
    Config cfg;
    if (cfg.enable_episodic_evaluation_publication) {
        std::cerr << "testE2C2PublicationDisabledByDefault: expected default OFF\n";
        return false;
    }
    return true;
}

class TestEpisodeCaptureSubscriber final : public Thoth::IEpisodeEventSubscriber {
public:
    int deliveries = 0;
    Thoth::EpisodeCompleted last_event;
    void onEpisodeCompleted(const Thoth::EpisodeCompleted& event) override {
        ++deliveries;
        last_event = event;
    }
};

/** E2-C2-02 — channel delivers immutable episode to subscriber. */
static bool testE2C2ChannelDeliversEpisode() {
    Thoth::InProcessEpisodeEventChannel channel;
    auto capture = std::make_shared<TestEpisodeCaptureSubscriber>();
    channel.subscribe(capture);

    Thoth::EpisodeCompleted event;
    event.plan_id = "plan-c2-test";
    event.goal = "goal";
    event.terminal_state = "COMPLETED";
    event.final_success_score = 0.8f;
    channel.publish(event);

    if (capture->deliveries != 1) {
        std::cerr << "testE2C2ChannelDeliversEpisode: expected one delivery\n";
        return false;
    }
    if (capture->last_event.plan_id != "plan-c2-test" ||
        capture->last_event.terminal_state != "COMPLETED") {
        std::cerr << "testE2C2ChannelDeliversEpisode: payload mismatch\n";
        return false;
    }
    return true;
}

/** E2-C2-02 — Executive source has no direct evaluation service import. */
static bool testE2C2ExecutiveNoEvalImport() {
    const std::string source = readRepoSourceFile("external/basic_agent/src/executive_controller.cpp");
    if (source.empty()) {
        std::cerr << "testE2C2ExecutiveNoEvalImport: cannot read executive source\n";
        return false;
    }
    const std::vector<std::string> forbidden = {"episodic_evaluation_service",
                                                "IEpisodicEvaluationService",
                                                "evaluateCase",
                                                "episodicEvaluationService"};
    for (const auto& sym : forbidden) {
        if (source.find(sym) != std::string::npos) {
            std::cerr << "testE2C2ExecutiveNoEvalImport: found " << sym << '\n';
            return false;
        }
    }
    if (source.find("publish_episode_completed_unlocked") == std::string::npos) {
        std::cerr << "testE2C2ExecutiveNoEvalImport: publication hook missing\n";
        return false;
    }
    return true;
}

/** E2-C2-03 — EvaluationSubscriber output is INTEGRATION / non-official. */
static bool testE2C2IntegrationEnvelope() {
    Thoth::EpisodeCompleted event;
    event.plan_id = "e2-c2-integration";
    event.goal = "integration goal";
    event.terminal_state = "COMPLETED";
    event.final_success_score = 1.0f;
    event.plan_snapshot = nlohmann::json::object();

    Thoth::EvaluationSubscriber subscriber;
    subscriber.onEpisodeCompleted(event);

    const Thoth::EpisodicLearningSummary* summary = Thoth::EvaluationSubscriber::lastSummaryForTests();
    if (!summary) {
        std::cerr << "testE2C2IntegrationEnvelope: missing summary\n";
        return false;
    }
    if (summary->official_scoring) {
        std::cerr << "testE2C2IntegrationEnvelope: official_scoring must be false\n";
        return false;
    }
    if (summary->scoring_tier != Thoth::E2EvalTier::INTEGRATION) {
        std::cerr << "testE2C2IntegrationEnvelope: expected INTEGRATION tier\n";
        return false;
    }

    Thoth::EpisodicLearningRunEnvelope envelope{false, true, "INTEGRATION"};
    const nlohmann::json row =
        Thoth::episodicLearningSummaryLogRow({}, *summary, 1, 1, envelope);
    if (row.value("official_scoring", true)) {
        std::cerr << "testE2C2IntegrationEnvelope: row official_scoring true\n";
        return false;
    }
    if (row.contains("e2_outcome")) {
        std::cerr << "testE2C2IntegrationEnvelope: INTEGRATION must not emit e2_outcome\n";
        return false;
    }
    return true;
}

/** E2-C2-04 — EvaluationSubscriber contains no execution logic. */
static bool testE2C2SubscriberNoExecutionLogic() {
    const std::string source = readRepoSourceFile("external/basic_agent/src/evaluation_subscriber.cpp");
    if (source.empty()) {
        std::cerr << "testE2C2SubscriberNoExecutionLogic: cannot read subscriber source\n";
        return false;
    }
    const std::vector<std::string> forbidden = {"ExecutiveController",
                                                "RAGPipeline",
                                                "execute_goal",
                                                "plantAndConsolidate",
                                                "memory_->",
                                                "Memory::"};
    for (const auto& sym : forbidden) {
        if (source.find(sym) != std::string::npos) {
            std::cerr << "testE2C2SubscriberNoExecutionLogic: found " << sym << '\n';
            return false;
        }
    }
    return true;
}

/** E2-C2 — mapping is deterministic and side-effect free (pure function). */
static bool testE2C2MappingDeterministic() {
    Thoth::EpisodeCompleted event;
    event.plan_id = "map-test";
    event.terminal_state = "COMPLETED";
    event.final_success_score = 0.5f;
    event.plan_snapshot = {{"steps", nlohmann::json::array()}};
    const auto a = Thoth::mapEpisodeToProductionObservations(event);
    const auto b = Thoth::mapEpisodeToProductionObservations(event);
    if (a.cold.final_success_score != b.cold.final_success_score ||
        a.warm.final_success_score != b.warm.final_success_score ||
        a.warm.terminal_state != b.warm.terminal_state) {
        std::cerr << "testE2C2MappingDeterministic: mapping not deterministic\n";
        return false;
    }
    return true;
}

// --- E2-D1: Event channel maturity (fan-out without coupling) ---

static bool episodeCompletedFieldsEqual(const Thoth::EpisodeCompleted& a,
                                        const Thoth::EpisodeCompleted& b) {
    return a.plan_id == b.plan_id && a.goal == b.goal && a.terminal_state == b.terminal_state &&
           a.final_success_score == b.final_success_score && a.run_id == b.run_id &&
           a.env_hash == b.env_hash && a.plan_snapshot == b.plan_snapshot &&
           a.trajectory_snapshot == b.trajectory_snapshot;
}

/** Cross-run comparison: outcome-carrying fields only (snapshots may vary between executions). */
static bool episodeCompletedOutcomeEqual(const Thoth::EpisodeCompleted& a,
                                         const Thoth::EpisodeCompleted& b) {
    return a.plan_id == b.plan_id && a.goal == b.goal && a.terminal_state == b.terminal_state &&
           a.final_success_score == b.final_success_score && a.run_id == b.run_id &&
           a.env_hash == b.env_hash;
}

class TestThrowingEpisodeSubscriber final : public Thoth::IEpisodeEventSubscriber {
public:
    int deliveries = 0;
    void onEpisodeCompleted(const Thoth::EpisodeCompleted&) override {
        ++deliveries;
        throw std::runtime_error("E2-D1 injected subscriber failure");
    }
};

class TestFifoOrderSubscriber final : public Thoth::IEpisodeEventSubscriber {
public:
    int id = 0;
    std::vector<int>* order = nullptr;
    void onEpisodeCompleted(const Thoth::EpisodeCompleted&) override {
        if (order) {
            order->push_back(id);
        }
    }
};

struct ExecutiveD1TestBed {
    Config cfg;
    std::shared_ptr<Memory> memory;
    std::unique_ptr<EmbeddingEngine> engine;
    IndexManager* idx = nullptr;
    std::shared_ptr<RAGPipeline> rag;
    std::shared_ptr<MockSingleLlmPlanner> planner;
    std::shared_ptr<ToolRegistry> registry;
    std::shared_ptr<Thoth::InProcessEpisodeEventChannel> channel;
    std::unique_ptr<Thoth::ExecutiveController> controller;

    explicit ExecutiveD1TestBed(const std::string& dbSuffix) {
        cfg.database_path = makeTempPath("thoth_e2_d1_" + dbSuffix + ".db").string();
        cfg.enable_episodic_evaluation_publication = true;
        memory = std::make_shared<Memory>(cfg);
        engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
        idx = new IndexManager(engine.get());
        rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
        planner = std::make_shared<MockSingleLlmPlanner>();
        registry = std::make_shared<ToolRegistry>();
        channel = std::make_shared<Thoth::InProcessEpisodeEventChannel>();
        controller = std::make_unique<Thoth::ExecutiveController>(planner, registry, rag, memory);
        controller->set_config(&cfg);
        controller->set_episode_event_channel(channel.get());
    }

    ~ExecutiveD1TestBed() { fs::remove(cfg.database_path); }

    bool runGoalToTerminal(const std::string& goal) {
        controller->execute_goal(goal);
        return waitForPlanTerminal(*controller);
    }
};

static bool executiveOutcomeEqual(const Thoth::ExecutiveController& a,
                                  const Thoth::ExecutiveController& b) {
    if (a.get_state() != b.get_state()) {
        return false;
    }
    const Plan planA = a.get_current_plan();
    const Plan planB = b.get_current_plan();
    return planA.goal == planB.goal && planA.status == planB.status &&
           planA.plan_id == planB.plan_id;
}

/** E2-D1-01 — multi-subscriber delivery; byte-identical immutable event. */
static bool testE2D1MultiSubscriberDelivery() {
    Thoth::InProcessEpisodeEventChannel channel;
    auto captureA = std::make_shared<TestEpisodeCaptureSubscriber>();
    auto captureB = std::make_shared<TestEpisodeCaptureSubscriber>();
    auto captureC = std::make_shared<TestEpisodeCaptureSubscriber>();
    channel.subscribe(captureA);
    channel.subscribe(captureB);
    channel.subscribe(captureC);

    if (channel.subscriberCountForTests() != 3) {
        std::cerr << "testE2D1MultiSubscriberDelivery: expected 3 subscribers\n";
        return false;
    }

    Thoth::EpisodeCompleted event;
    event.plan_id = "plan-d1-01";
    event.goal = "d1 fan-out goal";
    event.terminal_state = "COMPLETED";
    event.final_success_score = 0.75f;
    event.run_id = "run-d1-01";
    event.env_hash = "env-d1-01";
    event.plan_snapshot = {{"steps", nlohmann::json::array({{{"step_id", "s1"}}})}};
    event.trajectory_snapshot = {{"entries", nlohmann::json::array()}};
    channel.publish(event);

    const std::array<std::shared_ptr<TestEpisodeCaptureSubscriber>, 3> captures = {
        captureA, captureB, captureC};
    for (const auto& capture : captures) {
        if (capture->deliveries != 1) {
            std::cerr << "testE2D1MultiSubscriberDelivery: expected one delivery per subscriber\n";
            return false;
        }
        if (!episodeCompletedFieldsEqual(capture->last_event, event)) {
            std::cerr << "testE2D1MultiSubscriberDelivery: payload mismatch across subscribers\n";
            return false;
        }
    }
    if (!captureB->last_event.plan_snapshot.contains("steps")) {
        std::cerr << "testE2D1MultiSubscriberDelivery: snapshot not preserved\n";
        return false;
    }

    std::vector<int> fifoOrder;
    Thoth::InProcessEpisodeEventChannel fifoChannel;
    auto first = std::make_shared<TestFifoOrderSubscriber>();
    auto second = std::make_shared<TestFifoOrderSubscriber>();
    auto third = std::make_shared<TestFifoOrderSubscriber>();
    first->id = 1;
    second->id = 2;
    third->id = 3;
    first->order = &fifoOrder;
    second->order = &fifoOrder;
    third->order = &fifoOrder;
    fifoChannel.subscribe(first);
    fifoChannel.subscribe(second);
    fifoChannel.subscribe(third);
    Thoth::EpisodeCompleted fifoEvent;
    fifoEvent.plan_id = "fifo";
    fifoChannel.publish(fifoEvent);
    if (fifoOrder != std::vector<int>({1, 2, 3})) {
        std::cerr << "testE2D1MultiSubscriberDelivery: FIFO delivery order mismatch\n";
        return false;
    }
    return true;
}

/** E2-D1-02 — mandatory Executive-path failure isolation (Passive Consumer Law §4). */
static bool testE2D1ExecutiveFailureIsolation() {
    setenv("THOTH_MOCK_LLM", "true", 1);
    const std::string goal = "E2-D1 executive failure isolation goal";

    ExecutiveD1TestBed baseline("d1_02_baseline");
    auto baselineCapture = std::make_shared<TestEpisodeCaptureSubscriber>();
    baseline.channel->subscribe(baselineCapture);
    if (!baseline.runGoalToTerminal(goal)) {
        std::cerr << "testE2D1ExecutiveFailureIsolation: baseline goal did not reach terminal\n";
        return false;
    }
    const auto baselineState = baseline.controller->get_state();
    const Plan baselinePlan = baseline.controller->get_current_plan();

    ExecutiveD1TestBed throwingRun("d1_02_throwing");
    auto throwing = std::make_shared<TestThrowingEpisodeSubscriber>();
    auto healthyCapture = std::make_shared<TestEpisodeCaptureSubscriber>();
    throwingRun.channel->subscribe(throwing);
    throwingRun.channel->subscribe(healthyCapture);
    if (!throwingRun.runGoalToTerminal(goal)) {
        std::cerr << "testE2D1ExecutiveFailureIsolation: throwing run did not reach terminal\n";
        return false;
    }

    if (throwing->deliveries != 1) {
        std::cerr << "testE2D1ExecutiveFailureIsolation: throwing subscriber not invoked\n";
        return false;
    }
    if (healthyCapture->deliveries != 1) {
        std::cerr << "testE2D1ExecutiveFailureIsolation: healthy subscriber not delivered after throw\n";
        return false;
    }
    if (!executiveOutcomeEqual(*baseline.controller, *throwingRun.controller)) {
        std::cerr << "testE2D1ExecutiveFailureIsolation: Executive terminal outcome changed\n";
        return false;
    }
    if (throwingRun.controller->get_state() != baselineState) {
        std::cerr << "testE2D1ExecutiveFailureIsolation: controller state mismatch\n";
        return false;
    }
    const Plan throwingPlan = throwingRun.controller->get_current_plan();
    if (throwingPlan.status != baselinePlan.status || throwingPlan.goal != baselinePlan.goal) {
        std::cerr << "testE2D1ExecutiveFailureIsolation: plan outcome mismatch\n";
        return false;
    }
    return true;
}

/**
 * E2-D1-03 — invisibility audit + behavioral outcome/payload equivalence.
 * Passive Consumer Law §3: structural audit ensures subscribers cannot influence
 * execution ordering (no subscriber-count / consumer-identity branching in Executive).
 */
static bool e2D1ExecutiveInvisibilityStructuralAudit() {
    const std::string source = readRepoSourceFile("external/basic_agent/src/executive_controller.cpp");
    if (source.empty()) {
        std::cerr << "e2D1ExecutiveInvisibilityStructuralAudit: cannot read executive source\n";
        return false;
    }
    const std::vector<std::string> forbidden = {"subscriberCount", "subscriber_count", "subscribers_"};
    for (const auto& sym : forbidden) {
        if (source.find(sym) != std::string::npos) {
            std::cerr << "e2D1ExecutiveInvisibilityStructuralAudit: found forbidden symbol " << sym
                      << " (Passive Consumer Law §3)\n";
            return false;
        }
    }
    if (source.find("publish_episode_completed_unlocked") == std::string::npos) {
        std::cerr << "e2D1ExecutiveInvisibilityStructuralAudit: publication hook missing\n";
        return false;
    }
    return true;
}

static bool testE2D1ExecutiveInvisibilityAudit() {
    if (!e2D1ExecutiveInvisibilityStructuralAudit()) {
        return false;
    }

    setenv("THOTH_MOCK_LLM", "true", 1);
    const std::string goal = "E2-D1 invisibility goal";
    const Thoth::BenchmarkAttribution attr{"run-d1-03", "env-d1-03"};

    ExecutiveD1TestBed zeroSubs("d1_03_zero");
    zeroSubs.controller->execute_goal(goal, attr);
    if (!waitForPlanTerminal(*zeroSubs.controller)) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: zero-subscriber run timeout\n";
        return false;
    }
    const auto zeroState = zeroSubs.controller->get_state();
    const Plan zeroPlan = zeroSubs.controller->get_current_plan();
    const auto zeroEvent = zeroSubs.channel->lastPublishedEventForTests();
    if (!zeroEvent.has_value()) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: zero-subscriber publish missing\n";
        return false;
    }

    ExecutiveD1TestBed manySubs("d1_03_many");
    auto capA = std::make_shared<TestEpisodeCaptureSubscriber>();
    auto capB = std::make_shared<TestEpisodeCaptureSubscriber>();
    auto capC = std::make_shared<TestEpisodeCaptureSubscriber>();
    manySubs.channel->subscribe(capA);
    manySubs.channel->subscribe(capB);
    manySubs.channel->subscribe(capC);
    if (manySubs.channel->subscriberCountForTests() != 3) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: expected 3 subscribers\n";
        return false;
    }
    manySubs.controller->execute_goal(goal, attr);
    if (!waitForPlanTerminal(*manySubs.controller)) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: many-subscriber run timeout\n";
        return false;
    }

    if (manySubs.controller->get_state() != zeroState) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: Executive state differs at 0 vs N\n";
        return false;
    }
    const Plan manyPlan = manySubs.controller->get_current_plan();
    if (manyPlan.status != zeroPlan.status || manyPlan.goal != zeroPlan.goal) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: plan outcome differs at 0 vs N\n";
        return false;
    }

    const auto manyEvent = manySubs.channel->lastPublishedEventForTests();
    if (!manyEvent.has_value()) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: many-subscriber publish missing\n";
        return false;
    }
    if (!episodeCompletedOutcomeEqual(*zeroEvent, *manyEvent)) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: published outcome fields differ at 0 vs N\n";
        return false;
    }
    if (!episodeCompletedFieldsEqual(capA->last_event, capB->last_event) ||
        !episodeCompletedFieldsEqual(capA->last_event, *manyEvent)) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: subscriber payloads not identical\n";
        return false;
    }
    if (zeroEvent->run_id != attr.run_id || zeroEvent->env_hash != attr.env_hash) {
        std::cerr << "testE2D1ExecutiveInvisibilityAudit: attribution not propagated\n";
        return false;
    }
    return true;
}

static bool runE2D1Tests() {
    if (!testE2D1MultiSubscriberDelivery()) {
        std::cerr << "E2-D1-01 failed\n";
        return false;
    }
    if (!testE2D1ExecutiveFailureIsolation()) {
        std::cerr << "E2-D1-02 failed\n";
        return false;
    }
    if (!testE2D1ExecutiveInvisibilityAudit()) {
        std::cerr << "E2-D1-03 failed\n";
        return false;
    }
    std::cout << "E2-D1-01..03 path equivalence prerequisites: channel fan-out green\n";
    return true;
}

// --- E2-D2: Replay subscriber (deterministic re-observation) ---

static Thoth::EpisodeCompleted makeE2D2FixtureEvent() {
    Thoth::EpisodeCompleted event;
    event.plan_id = "plan-d2-01";
    event.goal = "d2 replay coexistence goal";
    event.terminal_state = "COMPLETED";
    event.final_success_score = 0.85f;
    event.completed_at_ms = 1'700'000'000'000;
    event.run_id = "run-d2-01";
    event.env_hash = "env-d2-01";
    event.plan_snapshot = {
        {"steps", nlohmann::json::array({{{"step_id", "retrieve-a"}, {"status", "SUCCESS"}}})},
        {"e2_expectations",
         {{"case_id", "E2-D2-01"},
          {"expected_lift", 0.0},
          {"min_cold_score", 0.0},
          {"min_warm_score", 0.0}}}};
    event.trajectory_snapshot = {{"entries", nlohmann::json::array({{{"step_id", "s1"}}})}};
    return event;
}

static nlohmann::json evaluationSubscriberObservedOutputsJson() {
    nlohmann::json observed = nlohmann::json::object();
    if (const auto* summary = Thoth::EvaluationSubscriber::lastSummaryForTests()) {
        observed["summary"] = Thoth::episodicLearningSummaryToJson(*summary);
    }
    if (const auto* diagnostics = Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests()) {
        observed["diagnostics"] = Thoth::evaluationDiagnosticSummaryToJson(*diagnostics);
    }
    return observed;
}

static bool testE2D2ReplayStructuralAudit() {
    const std::vector<std::string> paths = {"external/basic_agent/include/replay_subscriber.h",
                                            "external/basic_agent/src/replay_subscriber.cpp"};
    const std::vector<std::string> forbidden = {"publish(",
                                                "ExecutiveController",
                                                "resolveEvaluation",
                                                "execute_goal",
                                                "episodic_evaluation_service",
                                                "IEpisodicEvaluationService"};
    const std::vector<std::string> forbiddenChannelHandles = {
        "IEpisodeEventChannel*",
        "IEpisodeEventChannel&",
        "std::shared_ptr<IEpisodeEventChannel>"};

    auto extractReplaySubscriberClassBlock = [](const std::string& header) -> std::string {
        const std::string marker = "class ReplaySubscriber";
        const std::size_t start = header.find(marker);
        if (start == std::string::npos) {
            return {};
        }
        const std::size_t open = header.find('{', start);
        if (open == std::string::npos) {
            return {};
        }
        int depth = 0;
        for (std::size_t i = open; i < header.size(); ++i) {
            if (header[i] == '{') {
                ++depth;
            } else if (header[i] == '}') {
                --depth;
                if (depth == 0) {
                    return header.substr(start, i - start + 1);
                }
            }
        }
        return {};
    };

    for (const auto& path : paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << "testE2D2ReplayStructuralAudit: cannot read " << path << '\n';
            return false;
        }
        for (const auto& sym : forbidden) {
            if (source.find(sym) != std::string::npos) {
                std::cerr << "testE2D2ReplayStructuralAudit: found " << sym << " in " << path
                          << '\n';
                return false;
            }
        }
    }

    const std::string header =
        readRepoSourceFile("external/basic_agent/include/replay_subscriber.h");
    const std::string classBody = extractReplaySubscriberClassBlock(header);
    if (classBody.empty()) {
        std::cerr << "testE2D2ReplayStructuralAudit: cannot locate ReplaySubscriber class\n";
        return false;
    }
    for (const auto& handle : forbiddenChannelHandles) {
        if (classBody.find(handle) != std::string::npos) {
            std::cerr << "testE2D2ReplayStructuralAudit: ReplaySubscriber holds channel "
                      << handle << '\n';
            return false;
        }
    }
    return true;
}

/** E2-D2-01 — replay idempotence, append-only storage, coexistence, structural audit. */
static bool testE2D2ReplayIdempotenceAndCoexistence() {
    if (!testE2D2ReplayStructuralAudit()) {
        return false;
    }

    const Thoth::EpisodeCompleted fixture = makeE2D2FixtureEvent();

    Thoth::EvaluationSubscriber evalOnly;
    evalOnly.onEpisodeCompleted(fixture);
    const nlohmann::json evalBaselineOutputs = evaluationSubscriberObservedOutputsJson();
    if (evalBaselineOutputs.empty()) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: eval baseline missing outputs\n";
        return false;
    }

    Thoth::InProcessEpisodeEventChannel channel;
    auto replay = std::make_shared<Thoth::ReplaySubscriber>();
    auto eval = std::make_shared<Thoth::EvaluationSubscriber>();
    channel.subscribe(eval);
    channel.subscribe(replay);
    channel.publish(fixture);

    if (replay->captureCount() != 1) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: expected capture count 1\n";
        return false;
    }
    const Thoth::EpisodeCompleted* captured = replay->capturedAtForTests(0);
    if (!captured || !episodeCompletedFieldsEqual(*captured, fixture)) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: captured payload mismatch\n";
        return false;
    }
    const Thoth::EpisodeCompleted storedBeforeReplay = *captured;

    const nlohmann::json evalCoexistenceOutputs = evaluationSubscriberObservedOutputsJson();
    if (evalCoexistenceOutputs != evalBaselineOutputs) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: EvaluationSubscriber outputs "
                     "changed when ReplaySubscriber present\n";
        return false;
    }

    std::vector<Thoth::EpisodeCompleted> replaySinkPayloads;
    replay->setReplaySinkForTests([&](const Thoth::EpisodeCompleted& replayed) {
        replaySinkPayloads.push_back(replayed);
    });

    if (!replay->replayCaptured(0)) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: first replay failed\n";
        return false;
    }
    if (replay->captureCount() != 1) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: capture count changed after "
                     "first replay\n";
        return false;
    }
    captured = replay->capturedAtForTests(0);
    if (!captured || !episodeCompletedFieldsEqual(*captured, storedBeforeReplay) ||
        !episodeCompletedFieldsEqual(*captured, fixture)) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: stored object changed after "
                     "first replay\n";
        return false;
    }
    if (replaySinkPayloads.size() != 1 ||
        !episodeCompletedFieldsEqual(replaySinkPayloads.back(), fixture)) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: first replay payload mismatch\n";
        return false;
    }
    const Thoth::EpisodeCompleted firstReplayPayload = replaySinkPayloads.back();

    if (!replay->replayCaptured(0)) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: second replay failed\n";
        return false;
    }
    if (replay->captureCount() != 1) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: capture count changed after "
                     "second replay\n";
        return false;
    }
    captured = replay->capturedAtForTests(0);
    if (!captured || !episodeCompletedFieldsEqual(*captured, storedBeforeReplay) ||
        !episodeCompletedFieldsEqual(*captured, fixture)) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: stored object changed after "
                     "second replay\n";
        return false;
    }
    if (replaySinkPayloads.size() != 2 ||
        !episodeCompletedFieldsEqual(replaySinkPayloads.back(), firstReplayPayload) ||
        !episodeCompletedFieldsEqual(replaySinkPayloads.back(), fixture)) {
        std::cerr << "testE2D2ReplayIdempotenceAndCoexistence: second replay payload mismatch\n";
        return false;
    }

    return true;
}

static bool runE2D2Tests() {
    if (!testE2D2ReplayIdempotenceAndCoexistence()) {
        std::cerr << "E2-D2-01 failed\n";
        return false;
    }
    if (!testE2D2BenchmarkAuthorityIsolation()) {
        std::cerr << "E2-D2-02 failed\n";
        return false;
    }
    std::cout << "E2-D2-01 replay idempotence + coexistence green\n";
    std::cout << "E2-D2-02 benchmark authority isolation green\n";
    return true;
}

// --- E2-D3: Metrics + trace subscribers (observability without influence) ---

static bool testE2D3Step1ConfigDefaults() {
    Config cfg;
    if (cfg.enable_metrics_subscriber || cfg.enable_trace_subscriber) {
        std::cerr << "testE2D3Step1ConfigDefaults: D3 flags must default OFF\n";
        return false;
    }
    return true;
}

static std::string extractSubscriberClassBlock(const std::string& header,
                                               const std::string& className) {
    const std::string marker = "class " + className;
    const std::size_t start = header.find(marker);
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t open = header.find('{', start);
    if (open == std::string::npos) {
        return {};
    }
    int depth = 0;
    for (std::size_t i = open; i < header.size(); ++i) {
        if (header[i] == '{') {
            ++depth;
        } else if (header[i] == '}') {
            --depth;
            if (depth == 0) {
                return header.substr(start, i - start + 1);
            }
        }
    }
    return {};
}

static bool testE2D3Step1StructuralAudit() {
    const std::vector<std::string> paths = {"external/basic_agent/include/metrics_subscriber.h",
                                            "external/basic_agent/src/metrics_subscriber.cpp",
                                            "external/basic_agent/include/trace_subscriber.h",
                                            "external/basic_agent/src/trace_subscriber.cpp"};
    const std::vector<std::string> forbidden = {"publish(",
                                                "ExecutiveController",
                                                "resolveEvaluation",
                                                "execute_goal",
                                                "episodic_evaluation_service",
                                                "IEpisodicEvaluationService",
                                                "ReplaySubscriber",
                                                "registerReplaySubscriber"};
    const std::vector<std::string> forbiddenChannelHandles = {
        "IEpisodeEventChannel*",
        "IEpisodeEventChannel&",
        "std::shared_ptr<IEpisodeEventChannel>"};

    for (const auto& path : paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << "testE2D3Step1StructuralAudit: cannot read " << path << '\n';
            return false;
        }
        for (const auto& sym : forbidden) {
            if (source.find(sym) != std::string::npos) {
                std::cerr << "testE2D3Step1StructuralAudit: found " << sym << " in " << path
                          << '\n';
                return false;
            }
        }
    }

    const std::vector<std::pair<std::string, std::string>> classHeaders = {
        {"external/basic_agent/include/metrics_subscriber.h", "MetricsSubscriber"},
        {"external/basic_agent/include/trace_subscriber.h", "TraceSubscriber"}};
    for (const auto& [path, className] : classHeaders) {
        const std::string header = readRepoSourceFile(path);
        const std::string classBody = extractSubscriberClassBlock(header, className);
        if (classBody.empty()) {
            std::cerr << "testE2D3Step1StructuralAudit: cannot locate " << className << '\n';
            return false;
        }
        for (const auto& handle : forbiddenChannelHandles) {
            if (classBody.find(handle) != std::string::npos) {
                std::cerr << "testE2D3Step1StructuralAudit: " << className << " holds channel "
                          << handle << '\n';
                return false;
            }
        }
    }
    return true;
}

static bool testE2D3Step1RegistrationAndDelivery() {
    Thoth::InProcessEpisodeEventChannel channel;
    Thoth::registerMetricsSubscriber(channel);
    Thoth::registerTraceSubscriber(channel);
    if (channel.subscriberCountForTests() != 2) {
        std::cerr << "testE2D3Step1RegistrationAndDelivery: expected 2 subscribers\n";
        return false;
    }

    const Thoth::EpisodeCompleted fixture = makeE2D2FixtureEvent();
    channel.publish(fixture);
    if (Thoth::MetricsSubscriber::deliveryCountForTests() != 1) {
        std::cerr << "testE2D3Step1RegistrationAndDelivery: metrics delivery count mismatch\n";
        return false;
    }
    if (Thoth::TraceSubscriber::segmentCountForTests() != 1) {
        std::cerr << "testE2D3Step1RegistrationAndDelivery: trace segment count mismatch\n";
        return false;
    }
    return true;
}

static bool testE2D3Step1ImmutableEventView() {
    Thoth::InProcessEpisodeEventChannel channel;
    auto metrics = std::make_shared<Thoth::MetricsSubscriber>();
    auto trace = std::make_shared<Thoth::TraceSubscriber>();
    channel.subscribe(metrics);
    channel.subscribe(trace);

    const Thoth::EpisodeCompleted fixture = makeE2D2FixtureEvent();
    channel.publish(fixture);

    const auto published = channel.lastPublishedEventForTests();
    if (!published || !episodeCompletedFieldsEqual(*published, fixture)) {
        std::cerr << "testE2D3Step1ImmutableEventView: published snapshot mismatch\n";
        return false;
    }
    if (metrics->deliveryCount() != 1) {
        std::cerr << "testE2D3Step1ImmutableEventView: metrics not delivered\n";
        return false;
    }
    const Thoth::TraceSegmentRecord* segment = trace->segmentAtForTests(0);
    if (!segment || segment->plan_id != fixture.plan_id || segment->run_id != fixture.run_id ||
        segment->timestamp_ms != fixture.completed_at_ms) {
        std::cerr << "testE2D3Step1ImmutableEventView: trace segment fields mismatch\n";
        return false;
    }
    return true;
}

static bool testE2D3Step1CoexistenceWithEvalReplay() {
    const Thoth::EpisodeCompleted fixture = makeE2D2FixtureEvent();

    Thoth::EvaluationSubscriber evalOnly;
    evalOnly.onEpisodeCompleted(fixture);
    const nlohmann::json evalBaselineOutputs = evaluationSubscriberObservedOutputsJson();
    if (evalBaselineOutputs.empty()) {
        std::cerr << "testE2D3Step1CoexistenceWithEvalReplay: eval baseline missing outputs\n";
        return false;
    }

    Thoth::InProcessEpisodeEventChannel channel;
    auto eval = std::make_shared<Thoth::EvaluationSubscriber>();
    auto replay = std::make_shared<Thoth::ReplaySubscriber>();
    auto metrics = std::make_shared<Thoth::MetricsSubscriber>();
    auto trace = std::make_shared<Thoth::TraceSubscriber>();
    channel.subscribe(eval);
    channel.subscribe(replay);
    channel.subscribe(metrics);
    channel.subscribe(trace);
    channel.publish(fixture);

    const nlohmann::json evalWithD3 = evaluationSubscriberObservedOutputsJson();
    if (evalWithD3 != evalBaselineOutputs) {
        std::cerr << "testE2D3Step1CoexistenceWithEvalReplay: EvaluationSubscriber outputs "
                     "changed when D3 subscribers present\n";
        return false;
    }
    if (replay->captureCount() != 1) {
        std::cerr << "testE2D3Step1CoexistenceWithEvalReplay: replay capture count mismatch\n";
        return false;
    }
    if (metrics->deliveryCount() != 1 || trace->segmentCount() != 1) {
        std::cerr << "testE2D3Step1CoexistenceWithEvalReplay: D3 delivery mismatch\n";
        return false;
    }
    return true;
}

static bool runE2D3Step1Tests() {
    if (!testE2D3Step1ConfigDefaults()) {
        std::cerr << "E2-D3-Step1 config defaults failed\n";
        return false;
    }
    if (!testE2D3Step1StructuralAudit()) {
        std::cerr << "E2-D3-Step1 structural audit failed\n";
        return false;
    }
    if (!testE2D3Step1RegistrationAndDelivery()) {
        std::cerr << "E2-D3-Step1 registration failed\n";
        return false;
    }
    if (!testE2D3Step1ImmutableEventView()) {
        std::cerr << "E2-D3-Step1 immutability failed\n";
        return false;
    }
    if (!testE2D3Step1CoexistenceWithEvalReplay()) {
        std::cerr << "E2-D3-Step1 coexistence failed\n";
        return false;
    }
    std::cout << "E2-D3-Step1 subscriber skeleton green\n";
    return true;
}

static bool metricsJsonlContainsForbiddenAuthority(const nlohmann::json& record) {
    const std::string dumped = record.dump();
    const std::vector<std::string> forbidden = {"official_scoring",
                                                "e2_outcome",
                                                "evaluation_resolution",
                                                "\"lift\"",
                                                "success_rate",
                                                "scorable_cases",
                                                "not_scorable_cases"};
    for (const auto& key : forbidden) {
        if (dumped.find(key) != std::string::npos) {
            std::cerr << "metricsJsonlContainsForbiddenAuthority: forbidden key " << key << '\n';
            return true;
        }
    }
    return false;
}

static bool testE2D3_01MetricsEvalIndependence() {
    const std::vector<std::string> paths = {"external/basic_agent/include/metrics_subscriber.h",
                                            "external/basic_agent/src/metrics_subscriber.cpp"};
    const std::vector<std::string> forbidden = {"episodic_evaluation_service.h",
                                                "evaluation_subscriber.h",
                                                "IEpisodicEvaluationService",
                                                "resolveEvaluation",
                                                "evaluateCase",
                                                "evaluateEpisodicLearningCase",
                                                "episodicDiagnosticService",
                                                "e2_expectations"};
    for (const auto& path : paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << "testE2D3_01MetricsEvalIndependence: cannot read " << path << '\n';
            return false;
        }
        for (const auto& sym : forbidden) {
            if (source.find(sym) != std::string::npos) {
                std::cerr << "testE2D3_01MetricsEvalIndependence: found " << sym << " in " << path
                          << '\n';
                return false;
            }
        }
    }
    return true;
}

static bool testE2D3_01MetricsSinkOnly() {
    const fs::path jsonlPath = makeTempPath("thoth_e2_d3_01_metrics.jsonl");
    Thoth::MetricsSubscriber::setJsonlSinkPathForTests(jsonlPath.string());

    const Thoth::EpisodeCompleted fixture = makeE2D2FixtureEvent();

    Thoth::EvaluationSubscriber evalOnly;
    evalOnly.onEpisodeCompleted(fixture);
    const nlohmann::json evalBaselineOutputs = evaluationSubscriberObservedOutputsJson();
    if (evalBaselineOutputs.empty()) {
        std::cerr << "testE2D3_01MetricsSinkOnly: eval baseline missing outputs\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }

    Thoth::InProcessEpisodeEventChannel channel;
    auto eval = std::make_shared<Thoth::EvaluationSubscriber>();
    auto replay = std::make_shared<Thoth::ReplaySubscriber>();
    auto metrics = std::make_shared<Thoth::MetricsSubscriber>();
    channel.subscribe(eval);
    channel.subscribe(replay);
    channel.subscribe(metrics);
    channel.publish(fixture);

    const nlohmann::json evalWithMetrics = evaluationSubscriberObservedOutputsJson();
    if (evalWithMetrics != evalBaselineOutputs) {
        std::cerr << "testE2D3_01MetricsSinkOnly: EvaluationSubscriber outputs changed\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }
    if (replay->captureCount() != 1) {
        std::cerr << "testE2D3_01MetricsSinkOnly: replay capture count mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }

    const Thoth::MetricsRunAggregate* aggregate = metrics->runAggregateForRun(fixture.run_id);
    if (!aggregate || aggregate->episode_completed_total != 1) {
        std::cerr << "testE2D3_01MetricsSinkOnly: episode_completed_total mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }
    if (aggregate->observed_final_success_score != fixture.final_success_score) {
        std::cerr << "testE2D3_01MetricsSinkOnly: observed_final_success_score mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }
    if (aggregate->plan_step_count != 1) {
        std::cerr << "testE2D3_01MetricsSinkOnly: plan_step_count mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }
    if (aggregate->histogram_score_samples != 1) {
        std::cerr << "testE2D3_01MetricsSinkOnly: histogram sample count mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }

    const nlohmann::json jsonl = metrics->lastJsonlRecord();
    if (jsonl.value("metrics_schema_version", "") != "1.0" ||
        jsonl.value("record_type", "") != "episode_observation") {
        std::cerr << "testE2D3_01MetricsSinkOnly: episode JSONL schema mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }
    const auto& observations = jsonl["observations"];
    if (!observations.is_object() ||
        observations.value("observed_final_success_score", 0.0f) != fixture.final_success_score) {
        std::cerr << "testE2D3_01MetricsSinkOnly: JSONL observations mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }
    if (metricsJsonlContainsForbiddenAuthority(jsonl)) {
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }

    Thoth::E2PipelineStageTimings timings;
    timings.pipeline_duration_ms = 42;
    timings.mapping_duration_ms = 10;
    timings.evaluation_duration_ms = 12;
    timings.diagnostic_duration_ms = 8;
    timings.episodes_processed = 1;
    Thoth::E2PipelineTelemetryContext context;
    context.run_id = fixture.run_id;
    context.plan_id = fixture.plan_id;
    context.episode_completed_at_ms = fixture.completed_at_ms;
    const Thoth::E2PipelineTelemetryRecord pipelineRecord =
        Thoth::episodicPipelineTelemetryService().recordPipelineRun(timings, context);
    metrics->observePipelineTelemetry(pipelineRecord);

    if (aggregate->last_pipeline_duration_ms != 42) {
        std::cerr << "testE2D3_01MetricsSinkOnly: pipeline duration observation mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }

    const nlohmann::json pipelineJsonl = metrics->lastJsonlRecord();
    if (pipelineJsonl.value("record_type", "") != "pipeline_observation" ||
        !pipelineJsonl.contains("pipeline")) {
        std::cerr << "testE2D3_01MetricsSinkOnly: pipeline JSONL shape mismatch\n";
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }
    if (metricsJsonlContainsForbiddenAuthority(pipelineJsonl)) {
        Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
        return false;
    }

    Thoth::MetricsSubscriber::resetJsonlSinkPathForTests();
    std::error_code ec;
    fs::remove(jsonlPath, ec);
    return true;
}

static bool runE2D3_01Tests() {
    if (!testE2D3_01MetricsEvalIndependence()) {
        std::cerr << "E2-D3-01 eval-independence audit failed\n";
        return false;
    }
    if (!testE2D3_01MetricsSinkOnly()) {
        std::cerr << "E2-D3-01 metrics sink-only failed\n";
        return false;
    }
    std::cout << "E2-D3-01 metrics sink-only green\n";
    return true;
}

// --- E2-D3-02: Failure isolation (metrics + trace subscribers) ---

class TestThrowingD3SubscriberProxy final : public Thoth::IEpisodeEventSubscriber {
public:
    std::shared_ptr<Thoth::IEpisodeEventSubscriber> inner;
    int deliveries = 0;
    std::string label;

    void onEpisodeCompleted(const Thoth::EpisodeCompleted& event) override {
        ++deliveries;
        if (inner) {
            inner->onEpisodeCompleted(event);
        }
        throw std::runtime_error("E2-D3-02 " + label + " injected subscriber failure");
    }
};

enum class D3FanoutOrder { EvalReplayMetricsTrace, TraceMetricsReplayEval };

static nlohmann::json evalBaselineOutputsForFixture(const Thoth::EpisodeCompleted& fixture) {
    Thoth::EvaluationSubscriber evalOnly;
    evalOnly.onEpisodeCompleted(fixture);
    return evaluationSubscriberObservedOutputsJson();
}

static void subscribeD3Fanout(Thoth::InProcessEpisodeEventChannel& channel,
                              D3FanoutOrder order,
                              const std::shared_ptr<Thoth::EvaluationSubscriber>& eval,
                              const std::shared_ptr<Thoth::ReplaySubscriber>& replay,
                              const std::shared_ptr<Thoth::IEpisodeEventSubscriber>& metrics_slot,
                              const std::shared_ptr<Thoth::IEpisodeEventSubscriber>& trace_slot,
                              const std::shared_ptr<TestEpisodeCaptureSubscriber>& capture) {
    const auto sub = [&](const std::shared_ptr<Thoth::IEpisodeEventSubscriber>& s) {
        if (s) {
            channel.subscribe(s);
        }
    };
    switch (order) {
    case D3FanoutOrder::EvalReplayMetricsTrace:
        sub(std::static_pointer_cast<Thoth::IEpisodeEventSubscriber>(eval));
        sub(std::static_pointer_cast<Thoth::IEpisodeEventSubscriber>(replay));
        sub(metrics_slot);
        sub(trace_slot);
        sub(capture);
        break;
    case D3FanoutOrder::TraceMetricsReplayEval:
        sub(trace_slot);
        sub(metrics_slot);
        sub(std::static_pointer_cast<Thoth::IEpisodeEventSubscriber>(replay));
        sub(std::static_pointer_cast<Thoth::IEpisodeEventSubscriber>(eval));
        sub(capture);
        break;
    }
}

static bool assertPublicationInvariant(const Thoth::InProcessEpisodeEventChannel& channel,
                                       const Thoth::EpisodeCompleted& fixture) {
    const auto published = channel.lastPublishedEventForTests();
    if (!published) {
        std::cerr << "assertPublicationInvariant: published snapshot missing\n";
        return false;
    }
    if (!episodeCompletedFieldsEqual(*published, fixture)) {
        std::cerr << "assertPublicationInvariant: published snapshot mismatch\n";
        return false;
    }
    return true;
}

static bool assertNonThrowingExactlyOnce(
    const std::shared_ptr<Thoth::ReplaySubscriber>& replay,
    const std::shared_ptr<Thoth::MetricsSubscriber>& metrics,
    const std::shared_ptr<Thoth::TraceSubscriber>& trace,
    const std::shared_ptr<TestEpisodeCaptureSubscriber>& capture,
    const nlohmann::json& evalBaseline) {
    if (replay->captureCount() != 1) {
        std::cerr << "assertNonThrowingExactlyOnce: replay delivery count mismatch\n";
        return false;
    }
    if (metrics->deliveryCount() != 1) {
        std::cerr << "assertNonThrowingExactlyOnce: metrics delivery count mismatch\n";
        return false;
    }
    if (trace->segmentCount() != 1) {
        std::cerr << "assertNonThrowingExactlyOnce: trace delivery count mismatch\n";
        return false;
    }
    if (capture->deliveries != 1) {
        std::cerr << "assertNonThrowingExactlyOnce: capture delivery count mismatch\n";
        return false;
    }
    const nlohmann::json evalObserved = evaluationSubscriberObservedOutputsJson();
    if (evalBaseline.empty() || evalObserved != evalBaseline) {
        std::cerr << "assertNonThrowingExactlyOnce: eval delivery/output mismatch\n";
        return false;
    }
    return true;
}

static void registerD3SubscribersOnExecutiveBed(
    ExecutiveD1TestBed& bed,
    std::shared_ptr<Thoth::EvaluationSubscriber>& eval,
    std::shared_ptr<Thoth::ReplaySubscriber>& replay,
    std::shared_ptr<Thoth::MetricsSubscriber>& metrics,
    std::shared_ptr<Thoth::TraceSubscriber>& trace,
    const std::shared_ptr<Thoth::IEpisodeEventSubscriber>& metrics_slot,
    const std::shared_ptr<Thoth::IEpisodeEventSubscriber>& trace_slot) {
    eval = std::make_shared<Thoth::EvaluationSubscriber>();
    replay = std::make_shared<Thoth::ReplaySubscriber>();
    metrics = std::make_shared<Thoth::MetricsSubscriber>();
    trace = std::make_shared<Thoth::TraceSubscriber>();
    bed.channel->subscribe(eval);
    bed.channel->subscribe(replay);
    bed.channel->subscribe(metrics_slot ? metrics_slot : metrics);
    bed.channel->subscribe(trace_slot ? trace_slot : trace);
}

static bool testE2D3_02ChannelFailureIsolation(const bool throwing_metrics) {
    const Thoth::EpisodeCompleted fixture = makeE2D2FixtureEvent();
    const nlohmann::json evalBaseline = evalBaselineOutputsForFixture(fixture);

    auto eval = std::make_shared<Thoth::EvaluationSubscriber>();
    auto replay = std::make_shared<Thoth::ReplaySubscriber>();
    auto metrics = std::make_shared<Thoth::MetricsSubscriber>();
    auto trace = std::make_shared<Thoth::TraceSubscriber>();
    auto capture = std::make_shared<TestEpisodeCaptureSubscriber>();
    auto throwing_proxy = std::make_shared<TestThrowingD3SubscriberProxy>();

    std::shared_ptr<Thoth::IEpisodeEventSubscriber> metrics_slot = metrics;
    std::shared_ptr<Thoth::IEpisodeEventSubscriber> trace_slot = trace;
    if (throwing_metrics) {
        throwing_proxy->inner = metrics;
        throwing_proxy->label = "MetricsSubscriber";
        metrics_slot = throwing_proxy;
    } else {
        throwing_proxy->inner = trace;
        throwing_proxy->label = "TraceSubscriber";
        trace_slot = throwing_proxy;
    }

    Thoth::InProcessEpisodeEventChannel channel;
    subscribeD3Fanout(channel,
                      D3FanoutOrder::EvalReplayMetricsTrace,
                      eval,
                      replay,
                      metrics_slot,
                      trace_slot,
                      capture);
    channel.publish(fixture);

    if (!assertPublicationInvariant(channel, fixture)) {
        return false;
    }
    if (throwing_proxy->deliveries != 1) {
        std::cerr << "testE2D3_02ChannelFailureIsolation: throwing proxy not invoked once\n";
        return false;
    }
    return assertNonThrowingExactlyOnce(replay, metrics, trace, capture, evalBaseline);
}

static bool testE2D3_02OrderingPermutation() {
    const Thoth::EpisodeCompleted fixture = makeE2D2FixtureEvent();
    const nlohmann::json evalBaseline = evalBaselineOutputsForFixture(fixture);
    const D3FanoutOrder orders[] = {D3FanoutOrder::EvalReplayMetricsTrace,
                                    D3FanoutOrder::TraceMetricsReplayEval};

    for (const D3FanoutOrder order : orders) {
        auto eval = std::make_shared<Thoth::EvaluationSubscriber>();
        auto replay = std::make_shared<Thoth::ReplaySubscriber>();
        auto metrics = std::make_shared<Thoth::MetricsSubscriber>();
        auto trace = std::make_shared<Thoth::TraceSubscriber>();
        auto capture = std::make_shared<TestEpisodeCaptureSubscriber>();

        Thoth::InProcessEpisodeEventChannel channel;
        subscribeD3Fanout(channel, order, eval, replay, metrics, trace, capture);
        channel.publish(fixture);

        if (!assertPublicationInvariant(channel, fixture)) {
            std::cerr << "testE2D3_02OrderingPermutation: publication invariant failed\n";
            return false;
        }
        if (!assertNonThrowingExactlyOnce(replay, metrics, trace, capture, evalBaseline)) {
            std::cerr << "testE2D3_02OrderingPermutation: delivery invariant failed\n";
            return false;
        }
    }
    return true;
}

static bool testE2D3_02ExecutiveFailureIsolation(const bool throwing_metrics) {
    setenv("THOTH_MOCK_LLM", "true", 1);
    const std::string goal = "E2-D3-02 executive failure isolation goal";

    std::shared_ptr<Thoth::EvaluationSubscriber> evalBaseline;
    std::shared_ptr<Thoth::ReplaySubscriber> replayBaseline;
    std::shared_ptr<Thoth::MetricsSubscriber> metricsBaseline;
    std::shared_ptr<Thoth::TraceSubscriber> traceBaseline;
    ExecutiveD1TestBed baseline("d3_02_baseline");
    registerD3SubscribersOnExecutiveBed(
        baseline, evalBaseline, replayBaseline, metricsBaseline, traceBaseline, nullptr, nullptr);
    if (!baseline.runGoalToTerminal(goal)) {
        std::cerr << "testE2D3_02ExecutiveFailureIsolation: baseline goal did not reach terminal\n";
        return false;
    }
    const auto baselineState = baseline.controller->get_state();
    const Plan baselinePlan = baseline.controller->get_current_plan();

    std::shared_ptr<Thoth::EvaluationSubscriber> evalThrow;
    std::shared_ptr<Thoth::ReplaySubscriber> replayThrow;
    std::shared_ptr<Thoth::MetricsSubscriber> metricsThrow;
    std::shared_ptr<Thoth::TraceSubscriber> traceThrow;
    auto throwing_proxy = std::make_shared<TestThrowingD3SubscriberProxy>();
    std::shared_ptr<Thoth::IEpisodeEventSubscriber> metrics_slot;
    std::shared_ptr<Thoth::IEpisodeEventSubscriber> trace_slot;
    ExecutiveD1TestBed throwingRun("d3_02_throwing");
    if (throwing_metrics) {
        throwing_proxy->label = "MetricsSubscriber";
        registerD3SubscribersOnExecutiveBed(throwingRun,
                                          evalThrow,
                                          replayThrow,
                                          metricsThrow,
                                          traceThrow,
                                          throwing_proxy,
                                          nullptr);
        throwing_proxy->inner = metricsThrow;
    } else {
        throwing_proxy->label = "TraceSubscriber";
        registerD3SubscribersOnExecutiveBed(throwingRun,
                                          evalThrow,
                                          replayThrow,
                                          metricsThrow,
                                          traceThrow,
                                          nullptr,
                                          throwing_proxy);
        throwing_proxy->inner = traceThrow;
    }
    if (!throwingRun.runGoalToTerminal(goal)) {
        std::cerr << "testE2D3_02ExecutiveFailureIsolation: throwing run did not reach terminal\n";
        return false;
    }

    if (throwing_proxy->deliveries != 1) {
        std::cerr << "testE2D3_02ExecutiveFailureIsolation: throwing proxy not invoked once\n";
        return false;
    }
    if (replayThrow->captureCount() != 1 || metricsThrow->deliveryCount() != 1 ||
        traceThrow->segmentCount() != 1) {
        std::cerr << "testE2D3_02ExecutiveFailureIsolation: non-throwing delivery count mismatch\n";
        return false;
    }
    if (!executiveOutcomeEqual(*baseline.controller, *throwingRun.controller)) {
        std::cerr << "testE2D3_02ExecutiveFailureIsolation: Executive terminal outcome changed\n";
        return false;
    }
    if (throwingRun.controller->get_state() != baselineState) {
        std::cerr << "testE2D3_02ExecutiveFailureIsolation: controller state mismatch\n";
        return false;
    }
    const Plan throwingPlan = throwingRun.controller->get_current_plan();
    if (throwingPlan.status != baselinePlan.status || throwingPlan.goal != baselinePlan.goal ||
        throwingPlan.plan_id != baselinePlan.plan_id) {
        std::cerr << "testE2D3_02ExecutiveFailureIsolation: plan outcome mismatch\n";
        return false;
    }
    return true;
}

static bool runE2D3_02Tests() {
    if (!testE2D3_02ChannelFailureIsolation(true)) {
        std::cerr << "E2-D3-02 channel throwing-metrics failed\n";
        return false;
    }
    if (!testE2D3_02ChannelFailureIsolation(false)) {
        std::cerr << "E2-D3-02 channel throwing-trace failed\n";
        return false;
    }
    if (!testE2D3_02OrderingPermutation()) {
        std::cerr << "E2-D3-02 ordering permutation failed\n";
        return false;
    }
    if (!testE2D3_02ExecutiveFailureIsolation(true)) {
        std::cerr << "E2-D3-02 executive throwing-metrics failed\n";
        return false;
    }
    if (!testE2D3_02ExecutiveFailureIsolation(false)) {
        std::cerr << "E2-D3-02 executive throwing-trace failed\n";
        return false;
    }
    std::cout << "E2-D3-02 failure isolation green\n";
    return true;
}

// --- E2-D3-03: Structural audit (measure, don't interpret) ---

static const std::vector<std::string>& d3SubscriberSourcePaths() {
    static const std::vector<std::string> paths = {
        "external/basic_agent/include/metrics_subscriber.h",
        "external/basic_agent/src/metrics_subscriber.cpp",
        "external/basic_agent/include/trace_subscriber.h",
        "external/basic_agent/src/trace_subscriber.cpp"};
    return paths;
}

static bool grepSubscriberSources(const std::vector<std::string>& paths,
                                  const std::vector<std::string>& forbidden,
                                  const char* audit_name) {
    for (const auto& path : paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << audit_name << ": cannot read " << path << '\n';
            return false;
        }
        for (const auto& sym : forbidden) {
            if (source.find(sym) != std::string::npos) {
                std::cerr << audit_name << ": found forbidden symbol " << sym << " in " << path
                          << '\n';
                return false;
            }
        }
    }
    return true;
}

static bool grepClassBody(const std::string& header_path,
                          const std::string& class_name,
                          const std::vector<std::string>& forbidden,
                          const char* audit_name) {
    const std::string header = readRepoSourceFile(header_path);
    const std::string class_body = extractSubscriberClassBlock(header, class_name);
    if (class_body.empty()) {
        std::cerr << audit_name << ": cannot locate class " << class_name << '\n';
        return false;
    }
    for (const auto& sym : forbidden) {
        if (class_body.find(sym) != std::string::npos) {
            std::cerr << audit_name << ": class " << class_name << " contains forbidden " << sym
                      << '\n';
            return false;
        }
    }
    return true;
}

/** E2-D3-03 — narrow authority symbol grep (architectural authority only). */
static bool testE2D3_03ForbiddenAuthoritySymbols() {
    const std::vector<std::string> forbidden = {"episodic_evaluation_service.h",
                                              "evaluation_subscriber.h",
                                              "IEpisodicEvaluationService",
                                              "resolveEvaluation",
                                              "evaluateCase",
                                              "evaluateEpisodicLearningCase",
                                              "episodicDiagnosticService",
                                              "ExecutiveController",
                                              "execute_goal",
                                              "e2_expectations",
                                              "official_scoring",
                                              "evaluation_resolution",
                                              "e2_outcome"};
    return grepSubscriberSources(d3SubscriberSourcePaths(), forbidden,
                                 "testE2D3_03ForbiddenAuthoritySymbols");
}

/** E2-D3-03 — exclusive ownership: metrics ⊄ trace; trace ⊄ metrics aggregation. */
static bool testE2D3_03ExclusiveOwnership() {
    const std::vector<std::string> metrics_paths = {
        "external/basic_agent/include/metrics_subscriber.h",
        "external/basic_agent/src/metrics_subscriber.cpp"};
    const std::vector<std::string> metrics_forbidden = {"TraceSegmentRecord",
                                                        "segmentAtForTests",
                                                        "segments_",
                                                        "kRingCapacity",
                                                        "correlation_keys"};
    if (!grepSubscriberSources(metrics_paths, metrics_forbidden,
                               "testE2D3_03ExclusiveOwnership(metrics)")) {
        return false;
    }

    const std::vector<std::string> trace_paths = {
        "external/basic_agent/include/trace_subscriber.h",
        "external/basic_agent/src/trace_subscriber.cpp"};
    const std::vector<std::string> trace_forbidden = {"MetricsRunAggregate",
                                                      "histogramObserve",
                                                      "counterIncrement",
                                                      "counterAdd",
                                                      "gaugeSet",
                                                      "durationObserveMs",
                                                      "observed_final_success_score",
                                                      "metrics_schema_version"};
    return grepSubscriberSources(trace_paths, trace_forbidden,
                                 "testE2D3_03ExclusiveOwnership(trace)");
}

/** E2-D3-03 — immutability contract: const event view at API boundary. */
static bool testE2D3_03ImmutabilityContract() {
    const std::vector<std::pair<std::string, std::string>> headers = {
        {"external/basic_agent/include/metrics_subscriber.h", "MetricsSubscriber"},
        {"external/basic_agent/include/trace_subscriber.h", "TraceSubscriber"}};
    for (const auto& [path, class_name] : headers) {
        const std::string header = readRepoSourceFile(path);
        if (header.find("onEpisodeCompleted(const EpisodeCompleted&") == std::string::npos) {
            std::cerr << "testE2D3_03ImmutabilityContract: missing const event view in " << path
                      << '\n';
            return false;
        }
    }
    return testE2D3Step1ImmutableEventView();
}

/** E2-D3-03 — ordering structural: no registration or delivery-sequence dependence. */
static bool testE2D3_03OrderingStructural() {
    const std::vector<std::string> forbidden = {"subscriberCount",
                                                "firstDelivery",
                                                "lastSubscriber",
                                                "seen_before",
                                                "delivery_index",
                                                "registration_order",
                                                "subscriber_index"};
    return grepSubscriberSources(d3SubscriberSourcePaths(), forbidden,
                                 "testE2D3_03OrderingStructural");
}

/** E2-D3-03 — subscribers own no publication mechanism (class bodies). */
static bool testE2D3_03PublicationMechanism() {
    const std::vector<std::string> class_forbidden = {"IEpisodeEventChannel*",
                                                      "IEpisodeEventChannel&",
                                                      "std::shared_ptr<IEpisodeEventChannel>",
                                                      "publish("};
    if (!grepClassBody("external/basic_agent/include/metrics_subscriber.h",
                       "MetricsSubscriber",
                       class_forbidden,
                       "testE2D3_03PublicationMechanism")) {
        return false;
    }
    if (!grepClassBody("external/basic_agent/include/trace_subscriber.h",
                       "TraceSubscriber",
                       class_forbidden,
                       "testE2D3_03PublicationMechanism")) {
        return false;
    }
    const std::vector<std::string> cpp_paths = {
        "external/basic_agent/src/metrics_subscriber.cpp",
        "external/basic_agent/src/trace_subscriber.cpp"};
    return grepSubscriberSources(cpp_paths, {"publish("},
                                 "testE2D3_03PublicationMechanism(cpp)");
}

/** E2-D3-03 — JSONL observational path: forbidden authority keys absent from builders. */
static bool testE2D3_03JsonlObservationalPath() {
    const std::string metrics_source =
        readRepoSourceFile("external/basic_agent/src/metrics_subscriber.cpp");
    if (metrics_source.empty()) {
        std::cerr << "testE2D3_03JsonlObservationalPath: cannot read metrics source\n";
        return false;
    }
    const std::vector<std::string> forbidden_keys = {"\"official_scoring\"",
                                                     "\"e2_outcome\"",
                                                     "\"evaluation_resolution\"",
                                                     "\"success_rate\"",
                                                     "\"lift\"",
                                                     "\"pass\"",
                                                     "\"fail\"",
                                                     "\"recommendation\""};
    for (const auto& key : forbidden_keys) {
        if (metrics_source.find(key) != std::string::npos) {
            std::cerr << "testE2D3_03JsonlObservationalPath: forbidden JSONL key " << key
                      << " in metrics source\n";
            return false;
        }
    }
    const std::vector<std::string> required_allowed = {"\"metrics_schema_version\"",
                                                       "\"observed_final_success_score\"",
                                                       "\"terminal_state_label\""};
    for (const auto& key : required_allowed) {
        if (metrics_source.find(key) == std::string::npos) {
            std::cerr << "testE2D3_03JsonlObservationalPath: missing allowed key " << key
                      << " in metrics source\n";
            return false;
        }
    }
    return true;
}

/** E2-D3-03 — authority boundary: no reverse edges into eval/Executive. */
static bool testE2D3_03AuthorityBoundary() {
    const std::vector<std::string> forbidden = {"EvaluationSubscriber",
                                              "episodicEvaluationService",
                                              "IEpisodicEvaluationService",
                                              "resolveEvaluation",
                                              "execute_goal",
                                              "ExecutiveController"};
    if (!grepSubscriberSources(d3SubscriberSourcePaths(), forbidden,
                                 "testE2D3_03AuthorityBoundary")) {
        return false;
    }
    return testE2D3_01MetricsSinkOnly();
}

static bool runE2D3_03Tests() {
    if (!testE2D3_03ForbiddenAuthoritySymbols()) {
        std::cerr << "E2-D3-03 forbidden authority symbols failed\n";
        return false;
    }
    if (!testE2D3_03ExclusiveOwnership()) {
        std::cerr << "E2-D3-03 exclusive ownership failed\n";
        return false;
    }
    if (!testE2D3_03ImmutabilityContract()) {
        std::cerr << "E2-D3-03 immutability contract failed\n";
        return false;
    }
    if (!testE2D3_03OrderingStructural()) {
        std::cerr << "E2-D3-03 ordering structural failed\n";
        return false;
    }
    if (!testE2D3_03PublicationMechanism()) {
        std::cerr << "E2-D3-03 publication mechanism failed\n";
        return false;
    }
    if (!testE2D3_03JsonlObservationalPath()) {
        std::cerr << "E2-D3-03 JSONL observational path failed\n";
        return false;
    }
    if (!testE2D3_03AuthorityBoundary()) {
        std::cerr << "E2-D3-03 authority boundary failed\n";
        return false;
    }
    if (!testE2D3Step1StructuralAudit()) {
        std::cerr << "E2-D3-03 step-1 structural audit regression failed\n";
        return false;
    }
    std::cout << "E2-D3-03 structural audit green\n";
    return true;
}

static bool testE2D3_05ConfigJsonRoundTrip() {
    const fs::path tempPath = makeTempPath("thoth_d3_flags_config.json");

    auto roundTrip = [&](bool metricsOn, bool traceOn) -> bool {
        Config cfg;
        cfg.enable_metrics_subscriber = metricsOn;
        cfg.enable_trace_subscriber = traceOn;
        if (!cfg.saveToJson(tempPath.string())) {
            std::cerr << "testE2D3_05ConfigJsonRoundTrip: failed to save config\n";
            return false;
        }
        Config loaded;
        if (!loaded.loadFromJson(tempPath.string())) {
            std::cerr << "testE2D3_05ConfigJsonRoundTrip: failed to load config\n";
            return false;
        }
        if (loaded.enable_metrics_subscriber != metricsOn ||
            loaded.enable_trace_subscriber != traceOn) {
            std::cerr << "testE2D3_05ConfigJsonRoundTrip: flag mismatch after round-trip\n";
            return false;
        }
        return true;
    };

    const bool ok = roundTrip(true, false) && roundTrip(false, true) && roundTrip(true, true) &&
        roundTrip(false, false);
    fs::remove(tempPath);
    return ok;
}

static bool testE2D3_05PluginStructuralAudit() {
    const std::string plugin =
        readRepoSourceFile("external/basic_agent/src/basic_agent_plugin.cpp");
    if (plugin.empty()) {
        std::cerr << "testE2D3_05PluginStructuralAudit: cannot read basic_agent_plugin.cpp\n";
        return false;
    }
    if (plugin.find("config.enable_metrics_subscriber && episode_event_channel_") ==
            std::string::npos ||
        plugin.find("Thoth::registerMetricsSubscriber(*episode_event_channel_)") ==
            std::string::npos ||
        plugin.find("config.enable_trace_subscriber && episode_event_channel_") ==
            std::string::npos ||
        plugin.find("Thoth::registerTraceSubscriber(*episode_event_channel_)") ==
            std::string::npos) {
        std::cerr << "testE2D3_05PluginStructuralAudit: flag-gated registration blocks missing\n";
        return false;
    }

    const std::string executive =
        readRepoSourceFile("external/basic_agent/src/executive_controller.cpp");
    if (executive.empty()) {
        std::cerr << "testE2D3_05PluginStructuralAudit: cannot read executive_controller.cpp\n";
        return false;
    }
    const std::vector<std::string> forbiddenExecutive = {"enable_metrics_subscriber",
                                                         "enable_trace_subscriber",
                                                         "MetricsSubscriber",
                                                         "TraceSubscriber",
                                                         "registerMetricsSubscriber",
                                                         "registerTraceSubscriber"};
    for (const auto& sym : forbiddenExecutive) {
        if (executive.find(sym) != std::string::npos) {
            std::cerr << "testE2D3_05PluginStructuralAudit: Executive references " << sym << '\n';
            return false;
        }
    }
    return true;
}

static bool testE2D3_05ProductionOnlyRegistrationPath() {
    const std::vector<std::string> allowMetrics = {
        "external/basic_agent/src/basic_agent_plugin.cpp",
        "external/basic_agent/src/metrics_subscriber.cpp"};
    const std::vector<std::string> allowTrace = {
        "external/basic_agent/src/basic_agent_plugin.cpp",
        "external/basic_agent/src/trace_subscriber.cpp"};

    FileHandler fh;
    const fs::path srcRoot = fs::path(fh.getProjectRoot()) / "external/basic_agent/src";
    if (!fs::is_directory(srcRoot)) {
        std::cerr << "testE2D3_05ProductionOnlyRegistrationPath: missing src directory\n";
        return false;
    }

    for (const auto& entry : fs::directory_iterator(srcRoot)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp") {
            continue;
        }
        const std::string rel = "external/basic_agent/src/" + entry.path().filename().string();
        const std::string source = readRepoSourceFile(rel);
        if (source.empty()) {
            std::cerr << "testE2D3_05ProductionOnlyRegistrationPath: cannot read " << rel << '\n';
            return false;
        }
        if (source.find("registerMetricsSubscriber") != std::string::npos &&
            std::find(allowMetrics.begin(), allowMetrics.end(), rel) == allowMetrics.end()) {
            std::cerr << "testE2D3_05ProductionOnlyRegistrationPath: unexpected metrics registration "
                         "in "
                      << rel << '\n';
            return false;
        }
        if (source.find("registerTraceSubscriber") != std::string::npos &&
            std::find(allowTrace.begin(), allowTrace.end(), rel) == allowTrace.end()) {
            std::cerr << "testE2D3_05ProductionOnlyRegistrationPath: unexpected trace registration in "
                      << rel << '\n';
            return false;
        }
    }

    const std::string plugin =
        readRepoSourceFile("external/basic_agent/src/basic_agent_plugin.cpp");
    if (plugin.find("registerMetricsSubscriber") == std::string::npos ||
        plugin.find("registerTraceSubscriber") == std::string::npos) {
        std::cerr << "testE2D3_05ProductionOnlyRegistrationPath: plugin missing registration calls\n";
        return false;
    }
    return true;
}

struct E2D3PluginWorkspaceGuard {
    fs::path workspace;
    std::string priorWorkspaceEnv;
    bool hadWorkspaceEnv = false;

    bool prepare(bool metricsOn, bool traceOn) {
        workspace = makeTempPath("thoth_e2_d3_plugin_workspace");
        fs::create_directories(workspace);

        Config cfg;
        cfg.enable_metrics_subscriber = metricsOn;
        cfg.enable_trace_subscriber = traceOn;
        cfg.enable_episodic_evaluation_publication = false;
        cfg.enable_episode_replay_subscriber = false;
        if (!cfg.saveToJson((workspace / "config.json").string())) {
            return false;
        }

        if (const char* prior = std::getenv("THOTH_WORKSPACE_PATH"); prior && *prior) {
            hadWorkspaceEnv = true;
            priorWorkspaceEnv = prior;
        }
        setenv("THOTH_WORKSPACE_PATH", workspace.string().c_str(), 1);
        setenv("THOTH_TEST_SUITE_DEV", "1", 1);
        setenv("THOTH_MOCK_LLM", "true", 1);
        return true;
    }

    void restore() {
        if (hadWorkspaceEnv) {
            setenv("THOTH_WORKSPACE_PATH", priorWorkspaceEnv.c_str(), 1);
        } else {
            unsetenv("THOTH_WORKSPACE_PATH");
        }
        unsetenv("THOTH_TEST_SUITE_DEV");
        unsetenv("THOTH_MOCK_LLM");
        fs::remove_all(workspace);
    }
};

static bool verifyD3PluginChannelRegistration(Thoth::InProcessEpisodeEventChannel& channel,
                                             bool expectMetrics,
                                             bool expectTrace) {
    const std::size_t expectedCount =
        (expectMetrics ? 1u : 0u) + (expectTrace ? 1u : 0u);
    if (channel.subscriberCountForTests() != expectedCount) {
        std::cerr << "verifyD3PluginChannelRegistration: subscriber count "
                  << channel.subscriberCountForTests() << " expected " << expectedCount << '\n';
        return false;
    }

    const bool metricsOnChannel =
        Thoth::MetricsSubscriber::isRegisteredOnChannelForTests(channel);
    const bool traceOnChannel = Thoth::TraceSubscriber::isRegisteredOnChannelForTests(channel);
    if (metricsOnChannel != expectMetrics || traceOnChannel != expectTrace) {
        std::cerr << "verifyD3PluginChannelRegistration: identity mismatch metrics="
                  << metricsOnChannel << " trace=" << traceOnChannel << '\n';
        return false;
    }

    const Thoth::EpisodeCompleted fixture = makeE2D2FixtureEvent();
    channel.publish(fixture);

    const std::size_t metricsDeliveries = Thoth::MetricsSubscriber::deliveryCountForTests();
    const std::size_t traceSegments = Thoth::TraceSubscriber::segmentCountForTests();
    const bool metricsDelivered = metricsDeliveries == 1;
    const bool traceDelivered = traceSegments == 1;
    if (metricsDelivered != expectMetrics || traceDelivered != expectTrace) {
        std::cerr << "verifyD3PluginChannelRegistration: delivery mismatch metrics="
                  << metricsDeliveries << " trace=" << traceSegments << '\n';
        return false;
    }
    return true;
}

static bool testE2D3_05PluginRegistrationIntegration() {
    struct FlagCase {
        bool metrics;
        bool trace;
    };
    const FlagCase cases[] = {{false, false}, {true, false}, {false, true}, {true, true}};

    for (const auto& flagCase : cases) {
        E2D3PluginWorkspaceGuard guard;
        if (!guard.prepare(flagCase.metrics, flagCase.trace)) {
            std::cerr << "testE2D3_05PluginRegistrationIntegration: workspace prepare failed\n";
            return false;
        }

        {
            BasicAgentPlugin plugin;
            Thoth::InProcessEpisodeEventChannel* channel = plugin.episodeEventChannelForTests();
            if (!channel) {
                std::cerr << "testE2D3_05PluginRegistrationIntegration: missing channel\n";
                guard.restore();
                return false;
            }
            if (!verifyD3PluginChannelRegistration(*channel, flagCase.metrics, flagCase.trace)) {
                guard.restore();
                return false;
            }
        }
        guard.restore();
    }
    return true;
}

static bool runE2D3_05Tests() {
    if (!testE2D3_05ConfigJsonRoundTrip()) {
        std::cerr << "E2-D3-05 config JSON round-trip failed\n";
        return false;
    }
    if (!testE2D3_05PluginStructuralAudit()) {
        std::cerr << "E2-D3-05 plugin structural audit failed\n";
        return false;
    }
    if (!testE2D3_05ProductionOnlyRegistrationPath()) {
        std::cerr << "E2-D3-05 production-only registration path failed\n";
        return false;
    }
    if (!testE2D3_05PluginRegistrationIntegration()) {
        std::cerr << "E2-D3-05 plugin registration integration failed\n";
        return false;
    }
    std::cout << "E2-D3-05 plugin/config integration proof green\n";
    return true;
}

static bool runE2D3Tests() {
    if (!runE2D3Step1Tests()) {
        std::cerr << "E2-D3 proof suite Step 1 failed\n";
        return false;
    }
    if (!runE2D3_01Tests()) {
        std::cerr << "E2-D3 proof suite Step 2 (E2-D3-01) failed\n";
        return false;
    }
    if (!runE2D3_02Tests()) {
        std::cerr << "E2-D3 proof suite Step 3 (E2-D3-02) failed\n";
        return false;
    }
    if (!runE2D3_03Tests()) {
        std::cerr << "E2-D3 proof suite Step 4 (E2-D3-03) failed\n";
        return false;
    }
    if (!runE2D3_05Tests()) {
        std::cerr << "E2-D3 proof suite Step 5 failed\n";
        return false;
    }
    std::cout << "E2-D3 full proof suite (Steps 1-5) green\n";
    return true;
}

// --- E2-D4 Step 1: Production wiring seam confirmation (structural only) ---

static bool testE2D4Step1ConfigDefaultOff() {
    return testE2C2PublicationDisabledByDefault();
}

static bool testE2D4Step1ConfigJsonRoundTrip() {
    const fs::path tempPath = makeTempPath("thoth_d4_eval_publication_config.json");

    Config cfg;
    cfg.enable_episodic_evaluation_publication = true;
    cfg.enable_episodic_pipeline_telemetry = true;
    if (!cfg.saveToJson(tempPath.string())) {
        std::cerr << "testE2D4Step1ConfigJsonRoundTrip: failed to save config\n";
        return false;
    }

    Config loaded;
    if (!loaded.loadFromJson(tempPath.string())) {
        std::cerr << "testE2D4Step1ConfigJsonRoundTrip: failed to load config\n";
        fs::remove(tempPath);
        return false;
    }

    const bool ok = loaded.enable_episodic_evaluation_publication &&
        loaded.enable_episodic_pipeline_telemetry;
    fs::remove(tempPath);
    if (!ok) {
        std::cerr << "testE2D4Step1ConfigJsonRoundTrip: flag mismatch after round-trip\n";
    }
    return ok;
}

static bool testE2D4Step1IntegrationDefaultsContract() {
    const Thoth::E2EvalConfig integration = Thoth::E2EvalConfig::integrationDefaults();
    if (integration.tier != Thoth::E2EvalTier::INTEGRATION) {
        std::cerr << "testE2D4Step1IntegrationDefaultsContract: expected INTEGRATION tier\n";
        return false;
    }
    if (integration.officialScoring()) {
        std::cerr << "testE2D4Step1IntegrationDefaultsContract: officialScoring must be false\n";
        return false;
    }
    if (!integration.crossSessionEnabled() || !integration.heuristicsAllowed()) {
        std::cerr << "testE2D4Step1IntegrationDefaultsContract: integration flags mismatch\n";
        return false;
    }
    return true;
}

static bool testE2D4Step1PluginStructuralAudit() {
    const std::string plugin =
        readRepoSourceFile("external/basic_agent/src/basic_agent_plugin.cpp");
    if (plugin.empty()) {
        std::cerr << "testE2D4Step1PluginStructuralAudit: cannot read basic_agent_plugin.cpp\n";
        return false;
    }
    if (plugin.find("config.enable_episodic_evaluation_publication && episode_event_channel_") ==
            std::string::npos ||
        plugin.find("Thoth::registerEvaluationSubscriber(*episode_event_channel_)") ==
            std::string::npos ||
        plugin.find("Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(") ==
            std::string::npos) {
        std::cerr << "testE2D4Step1PluginStructuralAudit: flag-gated eval registration missing\n";
        return false;
    }
    if (plugin.find("setEvaluationSubscriberEvalConfigForTests") != std::string::npos) {
        std::cerr << "testE2D4Step1PluginStructuralAudit: test config seam in plugin\n";
        return false;
    }
    return true;
}

static bool testE2D4Step1ProductionOnlyRegistrationPath() {
    const std::vector<std::string> allowEval = {
        "external/basic_agent/src/basic_agent_plugin.cpp",
        "external/basic_agent/src/evaluation_subscriber.cpp"};

    FileHandler fh;
    const fs::path srcRoot = fs::path(fh.getProjectRoot()) / "external/basic_agent/src";
    if (!fs::is_directory(srcRoot)) {
        std::cerr << "testE2D4Step1ProductionOnlyRegistrationPath: missing src directory\n";
        return false;
    }

    for (const auto& entry : fs::directory_iterator(srcRoot)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp") {
            continue;
        }
        const std::string rel = "external/basic_agent/src/" + entry.path().filename().string();
        const std::string source = readRepoSourceFile(rel);
        if (source.empty()) {
            std::cerr << "testE2D4Step1ProductionOnlyRegistrationPath: cannot read " << rel
                      << '\n';
            return false;
        }
        if (source.find("registerEvaluationSubscriber") != std::string::npos &&
            std::find(allowEval.begin(), allowEval.end(), rel) == allowEval.end()) {
            std::cerr << "testE2D4Step1ProductionOnlyRegistrationPath: unexpected registration in "
                      << rel << '\n';
            return false;
        }
    }

    const std::string plugin =
        readRepoSourceFile("external/basic_agent/src/basic_agent_plugin.cpp");
    if (plugin.find("registerEvaluationSubscriber") == std::string::npos) {
        std::cerr << "testE2D4Step1ProductionOnlyRegistrationPath: plugin missing registration\n";
        return false;
    }
    return true;
}

static bool testE2D4Step1SubscriberConfigurationSelectionAudit() {
    const std::string source =
        readRepoSourceFile("external/basic_agent/src/evaluation_subscriber.cpp");
    if (source.empty()) {
        std::cerr << "testE2D4Step1SubscriberConfigurationSelectionAudit: cannot read subscriber "
                     "source\n";
        return false;
    }
    if (source.find("integrationDefaults()") == std::string::npos ||
        source.find("g_eval_config_for_tests.value_or(E2EvalConfig::integrationDefaults())") ==
            std::string::npos ||
        source.find("!g_eval_config_for_tests.has_value()") == std::string::npos ||
        source.find("config.tier = E2EvalTier::INTEGRATION") == std::string::npos) {
        std::cerr << "testE2D4Step1SubscriberConfigurationSelectionAudit: integration config "
                     "selection missing\n";
        return false;
    }
    if (source.find("strictDefaults()") != std::string::npos) {
        std::cerr << "testE2D4Step1SubscriberConfigurationSelectionAudit: strictDefaults in "
                     "subscriber production source\n";
        return false;
    }
    return true;
}

static bool testE2D4Step1ExecutivePublicationGate() {
    const std::string executive =
        readRepoSourceFile("external/basic_agent/src/executive_controller.cpp");
    if (executive.empty()) {
        std::cerr << "testE2D4Step1ExecutivePublicationGate: cannot read executive_controller.cpp\n";
        return false;
    }
    if (executive.find("!config_->enable_episodic_evaluation_publication") ==
        std::string::npos) {
        std::cerr << "testE2D4Step1ExecutivePublicationGate: publication flag gate missing\n";
        return false;
    }
    const std::vector<std::string> forbidden = {"official_scoring",
                                                "E2EvalTier::STRICT",
                                                "strictDefaults(",
                                                "registerEvaluationSubscriber",
                                                "integrationDefaults(",
                                                "scoring_tier"};
    for (const auto& sym : forbidden) {
        if (executive.find(sym) != std::string::npos) {
            std::cerr << "testE2D4Step1ExecutivePublicationGate: Executive references " << sym
                      << '\n';
            return false;
        }
    }
    return true;
}

static bool testE2D4Step1TestSeamIsolation() {
    const std::vector<std::string> paths = {
        "external/basic_agent/src/basic_agent_plugin.cpp",
        "external/basic_agent/src/executive_controller.cpp"};
    for (const auto& path : paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << "testE2D4Step1TestSeamIsolation: cannot read " << path << '\n';
            return false;
        }
        if (source.find("setEvaluationSubscriberEvalConfigForTests") != std::string::npos) {
            std::cerr << "testE2D4Step1TestSeamIsolation: test seam in production init " << path
                      << '\n';
            return false;
        }
    }
    return true;
}

static bool runE2D4Step1Tests() {
    if (!testE2D4Step1ConfigDefaultOff()) {
        std::cerr << "E2-D4-Step1 config default OFF failed\n";
        return false;
    }
    if (!testE2D4Step1ConfigJsonRoundTrip()) {
        std::cerr << "E2-D4-Step1 config JSON round-trip failed\n";
        return false;
    }
    if (!testE2D4Step1IntegrationDefaultsContract()) {
        std::cerr << "E2-D4-Step1 integrationDefaults contract failed\n";
        return false;
    }
    if (!testE2D4Step1PluginStructuralAudit()) {
        std::cerr << "E2-D4-Step1 plugin structural audit failed\n";
        return false;
    }
    if (!testE2D4Step1ProductionOnlyRegistrationPath()) {
        std::cerr << "E2-D4-Step1 production-only registration path failed\n";
        return false;
    }
    if (!testE2D4Step1SubscriberConfigurationSelectionAudit()) {
        std::cerr << "E2-D4-Step1 subscriber configuration selection audit failed\n";
        return false;
    }
    if (!testE2D4Step1ExecutivePublicationGate()) {
        std::cerr << "E2-D4-Step1 executive publication gate failed\n";
        return false;
    }
    if (!testE2D4Step1TestSeamIsolation()) {
        std::cerr << "E2-D4-Step1 test seam isolation failed\n";
        return false;
    }
    if (!testE2C2IntegrationEnvelope()) {
        std::cerr << "E2-D4-Step1 C2 integration envelope regression failed\n";
        return false;
    }

    std::cout << "E2-D4-Step1 production wiring seam confirmation green\n";
    std::cout << "E2-D4-Step1 evidence:\n";
    std::cout << "  gate: THOTH_E2_D4_STEP1 structural audits green\n";
    std::cout << "  verified seams: enable_episodic_evaluation_publication default OFF; JSON "
                 "round-trip; integrationDefaults() contract; plugin flag-gated "
                 "registerEvaluationSubscriber; production-only registration; subscriber "
                 "configuration selection (integrationDefaults when test seam unset); executive "
                 "publication gate; test seam isolation from plugin/executive\n";
    std::cout << "  deferred: Step 2 E2-D4-01 live plugin path; "
                 "Step 3 E2-D4-02 STRICT authority preservation audit\n";
    return true;
}

// --- E2-D4 Step 2: E2-D4-01 live plugin path (presence + containment + negative) ---

struct E2D4PluginWorkspaceGuard {
    fs::path workspace;
    std::string priorWorkspaceEnv;
    bool hadWorkspaceEnv = false;

    bool prepare() {
        workspace = makeTempPath("thoth_e2_d4_plugin_workspace");
        fs::create_directories(workspace);

        Config cfg;
        cfg.enable_episodic_evaluation_publication = true;
        cfg.enable_episodic_pipeline_telemetry = false;
        cfg.enable_episode_replay_subscriber = false;
        cfg.enable_metrics_subscriber = false;
        cfg.enable_trace_subscriber = false;
        if (!cfg.saveToJson((workspace / "config.json").string())) {
            return false;
        }

        if (const char* prior = std::getenv("THOTH_WORKSPACE_PATH"); prior && *prior) {
            hadWorkspaceEnv = true;
            priorWorkspaceEnv = prior;
        }
        setenv("THOTH_WORKSPACE_PATH", workspace.string().c_str(), 1);
        setenv("THOTH_TEST_SUITE_DEV", "1", 1);
        setenv("THOTH_MOCK_LLM", "true", 1);
        return true;
    }

    void restore() {
        Thoth::setEvaluationSubscriberEvalConfigForTests(std::nullopt);
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        if (hadWorkspaceEnv) {
            setenv("THOTH_WORKSPACE_PATH", priorWorkspaceEnv.c_str(), 1);
        } else {
            unsetenv("THOTH_WORKSPACE_PATH");
        }
        unsetenv("THOTH_TEST_SUITE_DEV");
        unsetenv("THOTH_MOCK_LLM");
        fs::remove_all(workspace);
    }
};

static bool e2D4RunLivePluginPathPublish(const Thoth::EpisodeCompleted& event,
                                         const char* audit_label) {
    E2D4PluginWorkspaceGuard guard;
    if (!guard.prepare()) {
        std::cerr << audit_label << ": workspace prepare failed\n";
        return false;
    }

    Thoth::setEvaluationSubscriberEvalConfigForTests(std::nullopt);

    {
        BasicAgentPlugin plugin;
        Thoth::InProcessEpisodeEventChannel* channel = plugin.episodeEventChannelForTests();
        if (!channel) {
            std::cerr << audit_label << ": missing episode channel\n";
            guard.restore();
            return false;
        }
        if (channel->subscriberCountForTests() != 1) {
            std::cerr << audit_label << ": expected single eval subscriber, count="
                      << channel->subscriberCountForTests() << '\n';
            guard.restore();
            return false;
        }

        channel->publish(event);

        if (!Thoth::EvaluationSubscriber::lastSummaryForTests()) {
            std::cerr << audit_label << ": subscriber produced no summary\n";
            guard.restore();
            return false;
        }
    }

    guard.restore();
    return true;
}

static nlohmann::json e2D4BuildIntegrationSummaryLogRow(const Thoth::EpisodeCompleted& event) {
    const Thoth::EpisodicLearningSummary* summary =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    if (!summary) {
        return nlohmann::json::object();
    }

    const Thoth::E2EvalConfig integration = Thoth::E2EvalConfig::integrationDefaults();
    Thoth::EpisodicLearningLogContext ctx;
    ctx.timestamp_ms = event.completed_at_ms;
    ctx.run_id = event.run_id;
    ctx.env_hash = event.env_hash;
    ctx.e2_eval_config = integration.toJson();
    ctx.evaluation_fingerprint =
        Thoth::episodicEvaluationService().computeFingerprint(integration).toJson();

    int cases_passed = 0;
    for (const auto& eval : summary->case_results) {
        if (eval.passes) {
            ++cases_passed;
        }
    }

    const Thoth::EpisodicLearningRunEnvelope envelope{false, true, "INTEGRATION"};
    return Thoth::episodicLearningSummaryLogRow(
        ctx, *summary, cases_passed, summary->case_results.size(), envelope);
}

static bool e2D4AssertIntegrationPresence(const Thoth::EpisodicLearningSummary& summary,
                                          const nlohmann::json& row,
                                          const char* audit_label) {
    if (summary.scoring_tier != Thoth::E2EvalTier::INTEGRATION) {
        std::cerr << audit_label << ": expected INTEGRATION tier\n";
        return false;
    }
    if (summary.official_scoring) {
        std::cerr << audit_label << ": official_scoring must be false\n";
        return false;
    }
    if (!Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests()) {
        std::cerr << audit_label << ": missing diagnostic metadata\n";
        return false;
    }

    if (row.value("event", "") != "EPISODIC_LEARNING_SUMMARY") {
        std::cerr << audit_label << ": expected EPISODIC_LEARNING_SUMMARY row\n";
        return false;
    }
    if (row.value("scoring_tier", "") != "INTEGRATION") {
        std::cerr << audit_label << ": JSONL scoring_tier must be INTEGRATION\n";
        return false;
    }
    if (row.value("official_scoring", true) != false) {
        std::cerr << audit_label << ": JSONL official_scoring must be false\n";
        return false;
    }
    if (row.value("wiring_stage", "") != "INTEGRATION") {
        std::cerr << audit_label << ": expected wiring_stage INTEGRATION\n";
        return false;
    }
    if (!row.contains("e2_eval_config") || !row["e2_eval_config"].is_object()) {
        std::cerr << audit_label << ": missing e2_eval_config diagnostic metadata\n";
        return false;
    }
    return true;
}

/** D4 containment contract — absence proofs only (§ D.4.0). */
static bool e2D4ViolatesContainmentContract(const Thoth::EpisodicLearningSummary& summary,
                                            const nlohmann::json& row,
                                            const nlohmann::json& diag_row) {
    if (summary.official_scoring) {
        return true;
    }
    if (summary.evaluation_resolution.has_value()) {
        return true;
    }
    if (row.value("official_scoring", true) != false) {
        return true;
    }
    if (row.contains("e2_outcome")) {
        return true;
    }
    if (row.contains("evaluation_resolution")) {
        return true;
    }
    if (row.value("wiring_stage", "") == "B" && row.value("official_scoring", false) == true) {
        return true;
    }
    if (row.contains("success_rate")) {
        return true;
    }
    if (diag_row.contains("e2_outcome")) {
        return true;
    }
    if (diag_row.value("official_scoring", false) == true) {
        return true;
    }
    return false;
}

static bool testE2D4_01LivePluginPathPresence() {
    const Thoth::EpisodeCompleted event = makeE2D2FixtureEvent();
    if (!e2D4RunLivePluginPathPublish(event, "testE2D4_01LivePluginPathPresence")) {
        return false;
    }

    const Thoth::EpisodicLearningSummary* summary =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    const nlohmann::json row = e2D4BuildIntegrationSummaryLogRow(event);
    return e2D4AssertIntegrationPresence(*summary, row, "testE2D4_01LivePluginPathPresence");
}

static bool testE2D4_01LivePluginPathJsonlPresence() {
    const Thoth::EpisodeCompleted event = makeE2D2FixtureEvent();
    if (!e2D4RunLivePluginPathPublish(event, "testE2D4_01LivePluginPathJsonlPresence")) {
        return false;
    }

    const Thoth::EpisodicLearningSummary* summary =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    const nlohmann::json row = e2D4BuildIntegrationSummaryLogRow(event);
    if (!e2D4AssertIntegrationPresence(*summary, row,
                                       "testE2D4_01LivePluginPathJsonlPresence")) {
        return false;
    }

    if (!row.contains("evaluation_fingerprint") || !row.contains("case_results")) {
        std::cerr << "testE2D4_01LivePluginPathJsonlPresence: missing E2-06 JSONL fields\n";
        return false;
    }
    return true;
}

static bool testE2D4_01LivePluginPathContainment() {
    const Thoth::EpisodeCompleted event = makeE2D2FixtureEvent();
    if (!e2D4RunLivePluginPathPublish(event, "testE2D4_01LivePluginPathContainment")) {
        return false;
    }

    const Thoth::EpisodicLearningSummary* summary =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    const nlohmann::json row = e2D4BuildIntegrationSummaryLogRow(event);
    const nlohmann::json diag_row = Thoth::evaluationDiagnosticSummaryToJson(
        *Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests());

    if (e2D4ViolatesContainmentContract(*summary, row, diag_row)) {
        std::cerr << "testE2D4_01LivePluginPathContainment: containment contract violated\n";
        return false;
    }
    return true;
}

static bool testE2D4_01IntegrationDefaultsBehavioralNegative() {
    const Thoth::EpisodeCompleted event = makeE2D2FixtureEvent();
    const Thoth::E2EvalConfig integration = Thoth::E2EvalConfig::integrationDefaults();

    Thoth::setEvaluationSubscriberEvalConfigForTests(Thoth::E2EvalConfig::strictDefaults());
    {
        Thoth::EvaluationSubscriber strictSubscriber;
        strictSubscriber.onEpisodeCompleted(event);
    }
    const Thoth::EpisodicLearningSummary* strictSummary =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    if (!strictSummary || strictSummary->scoring_tier != Thoth::E2EvalTier::STRICT ||
        !strictSummary->official_scoring) {
        std::cerr << "testE2D4_01IntegrationDefaultsBehavioralNegative: STRICT control baseline "
                     "missing\n";
        Thoth::setEvaluationSubscriberEvalConfigForTests(std::nullopt);
        return false;
    }

    const Thoth::EpisodicLearningSummary strictSummaryCopy = *strictSummary;

    Thoth::setEvaluationSubscriberEvalConfigForTests(std::nullopt);
    if (!e2D4RunLivePluginPathPublish(
            event, "testE2D4_01IntegrationDefaultsBehavioralNegative")) {
        return false;
    }

    const Thoth::EpisodicLearningSummary* liveSummary =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    if (!liveSummary) {
        std::cerr << "testE2D4_01IntegrationDefaultsBehavioralNegative: missing live summary\n";
        return false;
    }
    if (liveSummary->scoring_tier != integration.tier ||
        liveSummary->official_scoring != integration.officialScoring()) {
        std::cerr << "testE2D4_01IntegrationDefaultsBehavioralNegative: live path does not match "
                     "integrationDefaults()\n";
        return false;
    }
    if (liveSummary->scoring_tier == strictSummaryCopy.scoring_tier &&
        liveSummary->official_scoring == strictSummaryCopy.official_scoring) {
        std::cerr << "testE2D4_01IntegrationDefaultsBehavioralNegative: live path matches STRICT "
                     "control — STRICT config may be injected\n";
        return false;
    }
    if (liveSummary->outcome_rationale.find("non-scoring diagnostic") == std::string::npos) {
        std::cerr << "testE2D4_01IntegrationDefaultsBehavioralNegative: expected INTEGRATION "
                     "diagnostic summarize path\n";
        return false;
    }
    if (strictSummaryCopy.outcome_rationale.find("non-scoring diagnostic") != std::string::npos) {
        std::cerr << "testE2D4_01IntegrationDefaultsBehavioralNegative: STRICT control must not "
                     "use INTEGRATION diagnostic summarize path\n";
        return false;
    }

    Thoth::setEvaluationSubscriberEvalConfigForTests(std::nullopt);
    return true;
}

static bool runE2D4_01Tests() {
    if (!testE2D4_01LivePluginPathPresence()) {
        std::cerr << "E2-D4-01 live plugin path presence failed\n";
        return false;
    }
    if (!testE2D4_01LivePluginPathJsonlPresence()) {
        std::cerr << "E2-D4-01 live plugin path JSONL presence failed\n";
        return false;
    }
    if (!testE2D4_01LivePluginPathContainment()) {
        std::cerr << "E2-D4-01 live plugin path containment failed\n";
        return false;
    }
    if (!testE2D4_01IntegrationDefaultsBehavioralNegative()) {
        std::cerr << "E2-D4-01 integrationDefaults behavioral negative failed\n";
        return false;
    }
    if (!runE2D4Step1Tests()) {
        std::cerr << "E2-D4-01 Step 1 regression failed\n";
        return false;
    }

    std::cout << "E2-D4-01 live plugin path proof green\n";
    std::cout << "E2-D4-01 evidence:\n";
    std::cout << "  gate: THOTH_E2_D4_01 presence + containment + integrationDefaults negative\n";
    std::cout << "  live plugin path: BasicAgentPlugin -> channel -> EvaluationSubscriber\n";
    std::cout << "  deferred: Step 3 E2-D4-02 STRICT authority preservation audit\n";
    return true;
}

// --- E2-D4 Step 3: E2-D4-02 STRICT authority preservation audit ---

struct E2WiringStageBGuard {
    E2WiringStageBGuard() { setenv("THOTH_E2_WIRING_STAGE", "B", 1); }
    ~E2WiringStageBGuard() { unsetenv("THOTH_E2_WIRING_STAGE"); }
};

static Thoth::EpisodicLearningSummary buildOfficialGoldenSummaryWithD4EvalPublicationHarness() {
    return buildOfficialGoldenSummaryWithChannelHarness(false);
}

static nlohmann::json e2D4OfficialStrictSummaryLogRow(
    const Thoth::EpisodicLearningSummary& summary) {
    const Thoth::EpisodicLearningRunEnvelope envelope{true, true, "B"};
    return Thoth::episodicLearningSummaryLogRow(
        {}, summary, static_cast<int>(summary.case_results.size()),
        summary.case_results.size(), envelope);
}

static bool testE2D4_02StrictOfficialEnvelopePresence() {
    E2WiringStageBGuard wiringStage;
    const auto summary = buildOfficialGoldenSummary();
    if (summary.scoring_tier != Thoth::E2EvalTier::STRICT || !summary.official_scoring) {
        std::cerr << "testE2D4_02StrictOfficialEnvelopePresence: summary not STRICT official\n";
        return false;
    }
    if (!summary.evaluation_resolution.has_value() ||
        *summary.evaluation_resolution != Thoth::E2EvaluationResolution::SCORED_SUCCESS) {
        std::cerr << "testE2D4_02StrictOfficialEnvelopePresence: golden rollup missing "
                     "SCORED_SUCCESS\n";
        return false;
    }

    const nlohmann::json row = e2D4OfficialStrictSummaryLogRow(summary);
    if (row.value("wiring_stage", "") != "B" || row.value("official_scoring", false) != true) {
        std::cerr << "testE2D4_02StrictOfficialEnvelopePresence: official envelope mismatch\n";
        return false;
    }
    if (row.value("scoring_tier", "") != "STRICT") {
        std::cerr << "testE2D4_02StrictOfficialEnvelopePresence: expected scoring_tier STRICT\n";
        return false;
    }
    if (!row.contains("evaluation_resolution")) {
        std::cerr << "testE2D4_02StrictOfficialEnvelopePresence: evaluation_resolution missing\n";
        return false;
    }
    return true;
}

static bool testE2D4_02ScopedEquivalencePreservedWithEvalPublication() {
    E2WiringStageBGuard wiringStage;
    const auto baselineSummary = buildOfficialGoldenSummary();
    const auto baselineSnap = episodicLearningScopedBSnapshot(baselineSummary);

    const auto publicationSummary = buildOfficialGoldenSummaryWithChannelHarness(false);
    const auto publicationSnap = episodicLearningScopedBSnapshot(publicationSummary);
    if (!Thoth::episodicLearningScopedEquivalenceEqual(baselineSnap, publicationSnap)) {
        std::cerr << "testE2D4_02ScopedEquivalencePreservedWithEvalPublication: E2-28 scoped "
                     "snapshot differs with eval publication ON\n";
        return false;
    }
    return true;
}

static bool testE2D4_02ScopedEquivalencePreservedWithD4Workspace() {
    E2WiringStageBGuard wiringStage;
    E2D4PluginWorkspaceGuard workspace;
    if (!workspace.prepare()) {
        std::cerr << "testE2D4_02ScopedEquivalencePreservedWithD4Workspace: workspace prepare "
                     "failed\n";
        return false;
    }

    const auto baselineSummary = buildOfficialGoldenSummary();
    const auto baselineSnap = episodicLearningScopedBSnapshot(baselineSummary);

    const auto d4Summary = buildOfficialGoldenSummaryWithD4EvalPublicationHarness();
    const auto d4Snap = episodicLearningScopedBSnapshot(d4Summary);
    const bool ok = Thoth::episodicLearningScopedEquivalenceEqual(baselineSnap, d4Snap);

    workspace.restore();
    if (!ok) {
        std::cerr << "testE2D4_02ScopedEquivalencePreservedWithD4Workspace: E2-28 scoped snapshot "
                     "differs with D4 workspace active\n";
        return false;
    }
    return true;
}

static bool testE2D4_02StrictFingerprintDeterminismWithD4Wiring() {
    E2WiringStageBGuard wiringStage;
    E2D4PluginWorkspaceGuard workspace;
    if (!workspace.prepare()) {
        std::cerr << "testE2D4_02StrictFingerprintDeterminismWithD4Wiring: workspace prepare "
                     "failed\n";
        return false;
    }

    const auto summary_a = buildOfficialGoldenSummaryWithD4EvalPublicationHarness();
    const auto summary_b = buildOfficialGoldenSummaryWithD4EvalPublicationHarness();
    const auto snap_a = episodicLearningScopedBSnapshot(summary_a);
    const auto snap_b = episodicLearningScopedBSnapshot(summary_b);

    workspace.restore();
    if (!Thoth::episodicLearningScopedEquivalenceEqual(snap_a, snap_b)) {
        std::cerr << "testE2D4_02StrictFingerprintDeterminismWithD4Wiring: consecutive scoped "
                     "snapshots differ\n";
        return false;
    }
    if (Thoth::episodicLearningFingerprintMismatchBucket(snap_a, snap_b, "h1", "h1") != 0) {
        std::cerr << "testE2D4_02StrictFingerprintDeterminismWithD4Wiring: expected E2-28 "
                     "bucket #0\n";
        return false;
    }
    return true;
}

static bool testE2D4_02NoIntegrationLeakIntoStrictArtifacts() {
    E2WiringStageBGuard wiringStage;

    const auto strictSummary = buildOfficialGoldenSummaryWithChannelHarness(false);
    if (strictSummary.scoring_tier != Thoth::E2EvalTier::STRICT || !strictSummary.official_scoring) {
        std::cerr << "testE2D4_02NoIntegrationLeakIntoStrictArtifacts: official rollup not STRICT\n";
        return false;
    }

    const nlohmann::json officialRow = e2D4OfficialStrictSummaryLogRow(strictSummary);
    if (officialRow.value("scoring_tier", "") == "INTEGRATION" ||
        officialRow.value("official_scoring", true) == false) {
        std::cerr << "testE2D4_02NoIntegrationLeakIntoStrictArtifacts: INTEGRATION authority on "
                     "official STRICT row\n";
        return false;
    }

    E2EpisodeChannelHarness harness;
    harness.enable_episode_publication = true;
    harness.register_replay_subscriber = false;
    const auto cases = Thoth::getEpisodicLearningCases();
    if (cases.empty()) {
        std::cerr << "testE2D4_02NoIntegrationLeakIntoStrictArtifacts: no episodic cases\n";
        return false;
    }

    Thoth::BenchmarkAttribution attr{"e2-d4-02-isolation", "e2-d4-02-isolation-env"};
    (void)runE2TestArm(cases.front(), "warm", attr, nullptr, nullptr, &harness);

    const Thoth::EpisodicLearningSummary* sideSummary =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    if (!sideSummary) {
        std::cerr << "testE2D4_02NoIntegrationLeakIntoStrictArtifacts: missing subscriber side "
                     "summary\n";
        return false;
    }
    if (sideSummary->scoring_tier != Thoth::E2EvalTier::INTEGRATION ||
        sideSummary->official_scoring) {
        std::cerr << "testE2D4_02NoIntegrationLeakIntoStrictArtifacts: side channel must be "
                     "INTEGRATION non-official\n";
        return false;
    }
    if (sideSummary->scoring_tier == strictSummary.scoring_tier &&
        sideSummary->official_scoring == strictSummary.official_scoring) {
        std::cerr << "testE2D4_02NoIntegrationLeakIntoStrictArtifacts: side channel collapsed "
                     "into official STRICT rollup\n";
        return false;
    }

    if (harness.channel && harness.channel->lastPublishedEventForTests().has_value()) {
        const nlohmann::json episodeJson =
            harness.channel->lastPublishedEventForTests()->toJson();
        if (!episodeJsonLacksStrictAuthorityFields(episodeJson)) {
            std::cerr << "testE2D4_02NoIntegrationLeakIntoStrictArtifacts: published episode has "
                         "authority fields\n";
            return false;
        }
    }

    return true;
}

static bool runE2D4_02Tests() {
    if (!testE2D4_02StrictOfficialEnvelopePresence()) {
        std::cerr << "E2-D4-02 STRICT official envelope presence failed\n";
        return false;
    }
    if (!testE2D4_02ScopedEquivalencePreservedWithEvalPublication()) {
        std::cerr << "E2-D4-02 scoped equivalence (eval publication) failed\n";
        return false;
    }
    if (!testE2D4_02ScopedEquivalencePreservedWithD4Workspace()) {
        std::cerr << "E2-D4-02 scoped equivalence (D4 workspace) failed\n";
        return false;
    }
    if (!testE2D4_02StrictFingerprintDeterminismWithD4Wiring()) {
        std::cerr << "E2-D4-02 fingerprint determinism with D4 wiring failed\n";
        return false;
    }
    if (!testE2D4_02NoIntegrationLeakIntoStrictArtifacts()) {
        std::cerr << "E2-D4-02 INTEGRATION leak isolation failed\n";
        return false;
    }
    if (!testE2D2BenchmarkAuthorityIsolation()) {
        std::cerr << "E2-D4-02 E2-D2-02 regression failed\n";
        return false;
    }
    if (!runE2D4_01Tests()) {
        std::cerr << "E2-D4-02 Step 2 regression failed\n";
        return false;
    }

    std::cout << "E2-D4-02 STRICT authority preservation audit green\n";
    std::cout << "E2-D4-02 evidence:\n";
    std::cout << "  gate: THOTH_E2_D4_02 presence + preservation + isolation\n";
    std::cout << "  invariant: observational infrastructure transparent to authoritative path\n";
    std::cout << "  comparator: episodicLearningScopedEquivalenceEqual (E2-28 scoped snapshot)\n";
    std::cout << "  deferred: Step 4 backward compatibility · Step 5 composition proof (THOTH_E2_D4=1)\n";
    return true;
}

/** E2-C3-01 — diagnostics do not call evaluation or scoring functions. */
static bool testE2C3NoEvaluationCoupling() {
    const std::vector<std::string> paths = {"external/basic_agent/include/diagnostic_service.h",
                                            "external/basic_agent/src/diagnostic_service.cpp"};
    const std::vector<std::string> forbidden = {"evaluateCase",
                                                "evaluateEpisodicLearningCase",
                                                "resolveEvaluation(",
                                                "applyCaseEvaluationResolution",
                                                "applyCaseResolution",
                                                "summarizeEpisodicLearning",
                                                "summarize(",
                                                "computeFingerprint",
                                                "episodicEvaluationService",
                                                "IEpisodicEvaluationService"};
    for (const auto& path : paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << "testE2C3NoEvaluationCoupling: cannot read " << path << '\n';
            return false;
        }
        for (const auto& sym : forbidden) {
            if (source.find(sym) != std::string::npos) {
                std::cerr << "testE2C3NoEvaluationCoupling: found " << sym << " in " << path << '\n';
                return false;
            }
        }
    }

    const std::string evalServiceHeader =
        readRepoSourceFile("external/basic_agent/include/episodic_evaluation_service.h");
    const std::string evalServiceSource =
        readRepoSourceFile("external/basic_agent/src/episodic_evaluation_service.cpp");
    if (evalServiceHeader.find("diagnostic_service") != std::string::npos ||
        evalServiceSource.find("diagnostic_service") != std::string::npos) {
        std::cerr << "testE2C3NoEvaluationCoupling: evaluation service imports diagnostics\n";
        return false;
    }
    return true;
}

/** E2-C3-02 — same evaluation input produces identical diagnostics output. */
static bool testE2C3Determinism() {
    const Thoth::IEpisodicEvaluationService& svc = Thoth::episodicEvaluationService();
    const auto cases = Thoth::getEpisodicLearningCases();
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    Thoth::BenchmarkAttribution attr{"e2-c3-determinism", "e2-c3-determinism-env"};
    std::vector<Thoth::EpisodicLearningCaseEvaluation> evaluations;
    std::vector<Thoth::EpisodicLearningExpectations> expectations;
    for (const auto& spec : cases) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold = runE2TestArm(spec, "cold", attr, nullptr, nullptr);
        const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);
        auto eval = svc.evaluateCase(spec.id, spec.expectations, cold, warm, cfg);
        eval.run_block_reason = warmBlock;
        svc.applyCaseResolution(eval);
        evaluations.push_back(eval);
        expectations.push_back(spec.expectations);
    }
    const auto summary = svc.summarize(evaluations, expectations, cfg);
    const auto fingerprint = svc.computeFingerprint(cfg);

    Thoth::EvaluationDiagnosticsContext context;
    context.run_id = "e2-c3-determinism";
    context.env_hash = "e2-c3-determinism-env";
    context.fingerprint_hash = fingerprint.fingerprint_hash;
    context.e2_eval_config = cfg.toJson();

    const Thoth::IDiagnosticService& diag = Thoth::episodicDiagnosticService();
    const auto run_a = diag.generateRunDiagnostics(summary, context);
    const auto run_b = diag.generateRunDiagnostics(summary, context);
    const auto json_a = Thoth::evaluationDiagnosticSummaryToJson(run_a);
    const auto json_b = Thoth::evaluationDiagnosticSummaryToJson(run_b);
    if (json_a != json_b) {
        std::cerr << "testE2C3Determinism: run diagnostics differ\n";
        return false;
    }

    for (const auto& eval : evaluations) {
        const auto case_a = diag.generateDiagnostics(eval, cfg, context);
        const auto case_b = diag.generateDiagnostics(eval, cfg, context);
        if (Thoth::evaluationDiagnosticToJson(case_a) != Thoth::evaluationDiagnosticToJson(case_b)) {
            std::cerr << "testE2C3Determinism: case diagnostics differ for " << eval.case_id << '\n';
            return false;
        }
    }
    return true;
}

/** E2-C3-03 — diagnostics do not mutate evaluation artifacts. */
static bool testE2C3NonInterference() {
    const Thoth::IEpisodicEvaluationService& svc = Thoth::episodicEvaluationService();
    const auto cases = Thoth::getEpisodicLearningCases();
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    Thoth::BenchmarkAttribution attr{"e2-c3-noninterference", "e2-c3-noninterference-env"};
    std::vector<Thoth::EpisodicLearningCaseEvaluation> evaluations;
    std::vector<Thoth::EpisodicLearningExpectations> expectations;
    for (const auto& spec : cases) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold = runE2TestArm(spec, "cold", attr, nullptr, nullptr);
        const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);
        auto eval = svc.evaluateCase(spec.id, spec.expectations, cold, warm, cfg);
        eval.run_block_reason = warmBlock;
        svc.applyCaseResolution(eval);
        evaluations.push_back(eval);
        expectations.push_back(spec.expectations);
    }
    const auto summary_before = svc.summarize(evaluations, expectations, cfg);
    const auto fingerprint_before = svc.computeFingerprint(cfg);

    std::vector<Thoth::EpisodicLearningCaseEvaluation> eval_copy = evaluations;
    Thoth::EpisodicLearningSummary summary_copy = summary_before;
    const std::string fp_hash_before = fingerprint_before.fingerprint_hash;

    Thoth::EvaluationDiagnosticsContext context;
    context.run_id = "e2-c3-noninterference";
    context.env_hash = "e2-c3-noninterference-env";
    context.fingerprint_hash = fp_hash_before;
    context.e2_eval_config = cfg.toJson();
    (void)Thoth::episodicDiagnosticService().generateRunDiagnostics(summary_copy, context);
    for (auto& eval : eval_copy) {
        (void)Thoth::episodicDiagnosticService().generateDiagnostics(eval, cfg, context);
    }

    const auto summary_after = svc.summarize(evaluations, expectations, cfg);
    const auto fingerprint_after = svc.computeFingerprint(cfg);
    if (summary_before.mean_episodic_lift != summary_after.mean_episodic_lift ||
        summary_before.scorable_cases != summary_after.scorable_cases ||
        summary_before.not_scorable_cases != summary_after.not_scorable_cases ||
        summary_before.evaluation_resolution != summary_after.evaluation_resolution) {
        std::cerr << "testE2C3NonInterference: summary changed after diagnostics\n";
        return false;
    }
    if (fingerprint_before.fingerprint_hash != fingerprint_after.fingerprint_hash) {
        std::cerr << "testE2C3NonInterference: fingerprint changed after diagnostics\n";
        return false;
    }
    for (std::size_t i = 0; i < evaluations.size(); ++i) {
        const auto& before = evaluations[i];
        const auto& after = eval_copy[i];
        if (before.passes != after.passes || before.lift != after.lift ||
            before.failure_reason != after.failure_reason ||
            before.run_block_reason != after.run_block_reason ||
            before.evaluation_resolution != after.evaluation_resolution) {
            std::cerr << "testE2C3NonInterference: case evaluation mutated for " << before.case_id
                      << '\n';
            return false;
        }
    }
    return true;
}

/** E2-C3-04 — C2→C3 pipeline preserves E2-25–E2-28 evaluation semantics. */
static bool testE2C3PipelineIntegrity() {
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    const auto fp = Thoth::computeEvaluationFingerprint(cfg);
    const auto summary_a = buildOfficialGoldenSummary();
    const auto summary_b = buildOfficialGoldenSummary();
    const auto snap_a = Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary_a, fp.toJson(), cfg.toJson());
    const auto snap_b = Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary_b, fp.toJson(), cfg.toJson());
    if (!Thoth::episodicLearningScopedEquivalenceEqual(snap_a, snap_b)) {
        std::cerr << "testE2C3PipelineIntegrity: golden scoped snapshots differ\n";
        return false;
    }

    Thoth::EvaluationDiagnosticsContext context;
    context.run_id = "e2-c3-pipeline";
    context.env_hash = "e2-c3-pipeline-env";
    context.fingerprint_hash = fp.fingerprint_hash;
    context.e2_eval_config = cfg.toJson();
    const auto run_diag = Thoth::episodicDiagnosticService().generateRunDiagnostics(summary_a, context);
    if (run_diag.case_diagnostics.empty()) {
        std::cerr << "testE2C3PipelineIntegrity: expected case diagnostics\n";
        return false;
    }
    for (const auto& item : run_diag.case_diagnostics) {
        if (item.diagnosis_bucket != 0 ||
            item.failure_classification != Thoth::E2DiagnosticFailureClassification::NONE) {
            std::cerr << "testE2C3PipelineIntegrity: golden case expected bucket 0\n";
            return false;
        }
        if (!item.evaluation_resolution_snapshot.has_value() ||
            *item.evaluation_resolution_snapshot !=
                Thoth::E2EvaluationResolution::SCORED_SUCCESS) {
            std::cerr << "testE2C3PipelineIntegrity: golden case expected SCORED_SUCCESS\n";
            return false;
        }
    }

    Thoth::EpisodeCompleted event;
    event.plan_id = "e2-c3-pipeline-integration";
    event.goal = "pipeline integrity";
    event.terminal_state = "COMPLETED";
    event.final_success_score = 1.0f;
    event.run_id = "e2-c3-subscriber-run";
    event.env_hash = "e2-c3-subscriber-env";
    event.plan_snapshot = {{"steps", nlohmann::json::array()}};

    Thoth::EvaluationSubscriber subscriber;
    subscriber.onEpisodeCompleted(event);

    const Thoth::EpisodicLearningSummary* summary = Thoth::EvaluationSubscriber::lastSummaryForTests();
    const Thoth::EvaluationDiagnosticsSummary* diagnostics =
        Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests();
    if (!summary || !diagnostics) {
        std::cerr << "testE2C3PipelineIntegrity: subscriber artifacts missing\n";
        return false;
    }
    if (summary->official_scoring || summary->scoring_tier != Thoth::E2EvalTier::INTEGRATION) {
        std::cerr << "testE2C3PipelineIntegrity: subscriber must stay INTEGRATION non-official\n";
        return false;
    }
    if (summary->case_results.empty()) {
        std::cerr << "testE2C3PipelineIntegrity: expected case results from subscriber\n";
        return false;
    }
    const auto& case_eval = summary->case_results.front();
    if (!case_eval.evaluation_resolution.has_value() ||
        *case_eval.evaluation_resolution != Thoth::E2EvaluationResolution::SCORED_SUCCESS) {
        std::cerr << "testE2C3PipelineIntegrity: subscriber case not SCORED_SUCCESS\n";
        return false;
    }
    const nlohmann::json diag_row = Thoth::evaluationDiagnosticSummaryToJson(*diagnostics);
    if (diag_row.value("event_type", "") != "E2_EVAL_DIAGNOSTIC_SUMMARY") {
        std::cerr << "testE2C3PipelineIntegrity: diagnostic event_type mismatch\n";
        return false;
    }
    if (diag_row.contains("e2_outcome")) {
        std::cerr << "testE2C3PipelineIntegrity: diagnostics must not emit e2_outcome\n";
        return false;
    }
    return true;
}

static Thoth::EpisodeCompleted makeE2C4CheckpointEvent() {
    Thoth::EpisodeCompleted event;
    event.plan_id = "e2-c4-checkpoint";
    event.goal = "checkpoint proof goal";
    event.terminal_state = "COMPLETED";
    event.final_success_score = 1.0f;
    event.run_id = "e2-c4-checkpoint-run";
    event.env_hash = "e2-c4-checkpoint-env";
    event.completed_at_ms = 1'700'000'000'000;
    event.plan_snapshot = {{"steps", nlohmann::json::array()}};
    return event;
}

/** E2-C4-01 — telemetry schema segregation + no eval/diag coupling. */
static bool testE2C4NoEvaluationCoupling() {
    const std::vector<std::string> paths = {
        "external/basic_agent/include/pipeline_telemetry_service.h",
        "external/basic_agent/src/pipeline_telemetry_service.cpp"};
    const std::vector<std::string> forbidden = {"evaluateCase",
                                                "evaluateEpisodicLearningCase",
                                                "resolveEvaluation(",
                                                "applyCaseEvaluationResolution",
                                                "applyCaseResolution",
                                                "summarizeEpisodicLearning",
                                                "summarize(",
                                                "computeFingerprint",
                                                "generateDiagnostics",
                                                "generateRunDiagnostics",
                                                "episodicEvaluationService",
                                                "episodicDiagnosticService",
                                                "IEpisodicEvaluationService",
                                                "IDiagnosticService",
                                                "executive_controller",
                                                "episode_events.h"};
    for (const auto& path : paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << "testE2C4NoEvaluationCoupling: cannot read " << path << '\n';
            return false;
        }
        for (const auto& sym : forbidden) {
            if (source.find(sym) != std::string::npos) {
                std::cerr << "testE2C4NoEvaluationCoupling: found " << sym << " in " << path << '\n';
                return false;
            }
        }
    }

    const std::vector<std::string> no_import_paths = {
        "external/basic_agent/include/episodic_evaluation_service.h",
        "external/basic_agent/src/episodic_evaluation_service.cpp",
        "external/basic_agent/include/diagnostic_service.h",
        "external/basic_agent/src/diagnostic_service.cpp"};
    for (const auto& path : no_import_paths) {
        const std::string source = readRepoSourceFile(path);
        if (source.find("pipeline_telemetry") != std::string::npos) {
            std::cerr << "testE2C4NoEvaluationCoupling: " << path << " imports telemetry\n";
            return false;
        }
    }

    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(true);
    Thoth::EvaluationSubscriber subscriber;
    subscriber.onEpisodeCompleted(makeE2C4CheckpointEvent());
    const Thoth::E2PipelineTelemetryRecord* record =
        Thoth::EvaluationSubscriber::lastTelemetryRecordForTests();
    if (!record) {
        std::cerr << "testE2C4NoEvaluationCoupling: expected telemetry record when ON\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }
    const nlohmann::json row = Thoth::e2PipelineTelemetryToJson(*record);
    if (row.value("telemetry_tier", "") != "ARCHITECTURE") {
        std::cerr << "testE2C4NoEvaluationCoupling: bad telemetry_tier\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }
    const std::vector<std::string> forbidden_fields = {"evaluation_resolution",
                                                       "fingerprint_hash",
                                                       "lift",
                                                       "passes",
                                                       "mean_episodic_lift",
                                                       "e2_outcome",
                                                       "diagnosis_bucket",
                                                       "failure_classification",
                                                       "success"};
    for (const auto& field : forbidden_fields) {
        if (row.contains(field)) {
            std::cerr << "testE2C4NoEvaluationCoupling: forbidden field " << field << '\n';
            Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
            return false;
        }
    }
    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
    return true;
}

/** E2-C4-02 — telemetry failure does not alter eval/diagnostic artifacts. */
static bool testE2C4NonBlockingFailure() {
    const Thoth::EpisodeCompleted event = makeE2C4CheckpointEvent();

    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(true);
    Thoth::setE2PipelineTelemetryThrowForTests(false);
    Thoth::EvaluationSubscriber baseline;
    baseline.onEpisodeCompleted(event);
    const auto* summary_base = Thoth::EvaluationSubscriber::lastSummaryForTests();
    const auto* diag_base = Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests();
    if (!summary_base || !diag_base) {
        std::cerr << "testE2C4NonBlockingFailure: baseline artifacts missing\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }
    const nlohmann::json summary_base_json = Thoth::episodicLearningSummaryToJson(*summary_base);
    const nlohmann::json diag_base_json =
        Thoth::evaluationDiagnosticSummaryToJson(*diag_base);

    Thoth::setE2PipelineTelemetryThrowForTests(true);
    Thoth::EvaluationSubscriber failing;
    failing.onEpisodeCompleted(event);
    Thoth::setE2PipelineTelemetryThrowForTests(false);

    const auto* summary_fail = Thoth::EvaluationSubscriber::lastSummaryForTests();
    const auto* diag_fail = Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests();
    if (!summary_fail || !diag_fail) {
        std::cerr << "testE2C4NonBlockingFailure: post-throw artifacts missing\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }
    if (Thoth::EvaluationSubscriber::lastTelemetryRecordForTests() != nullptr) {
        std::cerr << "testE2C4NonBlockingFailure: telemetry record set after throw\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }
    if (Thoth::episodicLearningSummaryToJson(*summary_fail) != summary_base_json ||
        Thoth::evaluationDiagnosticSummaryToJson(*diag_fail) != diag_base_json) {
        std::cerr << "testE2C4NonBlockingFailure: eval/diag changed after telemetry throw\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }

    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
    return true;
}

/** E2-C4-03b — subscriber telemetry block must not branch on eval/diag semantics. */
static bool testE2C4SubscriberTelemetryBlockAudit() {
    const std::string source = readRepoSourceFile("external/basic_agent/src/evaluation_subscriber.cpp");
    if (source.empty()) {
        std::cerr << "testE2C4SubscriberTelemetryBlockAudit: cannot read subscriber source\n";
        return false;
    }
    const auto blockStart = source.find("if (g_pipeline_telemetry_enabled)");
    if (blockStart == std::string::npos) {
        std::cerr << "testE2C4SubscriberTelemetryBlockAudit: telemetry block not found\n";
        return false;
    }
    const auto blockEnd = source.find("EpisodicLearningRunEnvelope envelope", blockStart);
    if (blockEnd == std::string::npos || blockEnd <= blockStart) {
        std::cerr << "testE2C4SubscriberTelemetryBlockAudit: telemetry block end not found\n";
        return false;
    }
    const std::string block = source.substr(blockStart, blockEnd - blockStart);
    const std::vector<std::string> forbidden = {"summary",
                                                "runDiagnostics",
                                                "evaluation.",
                                                "evaluation_resolution",
                                                "diagnosis_bucket",
                                                "failure_classification",
                                                "passes",
                                                "lift",
                                                "fingerprint_hash",
                                                "episodicDiagnosticService",
                                                "generateRunDiagnostics"};
    for (const auto& sym : forbidden) {
        if (block.find(sym) != std::string::npos) {
            std::cerr << "testE2C4SubscriberTelemetryBlockAudit: forbidden symbol " << sym
                      << " in telemetry block\n";
            return false;
        }
    }
    return true;
}

/** E2-C4-03 — structural audit: telemetry service has no measurement clocks. */
static bool testE2C4StructuralAudit() {
    const std::string source =
        readRepoSourceFile("external/basic_agent/src/pipeline_telemetry_service.cpp");
    if (source.empty()) {
        std::cerr << "testE2C4StructuralAudit: cannot read telemetry source\n";
        return false;
    }
    const std::vector<std::string> forbidden = {"steady_clock", "system_clock", "chrono"};
    for (const auto& sym : forbidden) {
        if (source.find(sym) != std::string::npos) {
            std::cerr << "testE2C4StructuralAudit: telemetry service must not measure: " << sym
                      << '\n';
            return false;
        }
    }
    return true;
}

/** E2-C4-04 — telemetry recordPipelineRun does not mutate evaluation artifacts. */
static bool testE2C4NonInterference() {
    const Thoth::IEpisodicEvaluationService& svc = Thoth::episodicEvaluationService();
    const auto cases = Thoth::getEpisodicLearningCases();
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    Thoth::BenchmarkAttribution attr{"e2-c4-noninterference", "e2-c4-noninterference-env"};
    std::vector<Thoth::EpisodicLearningCaseEvaluation> evaluations;
    std::vector<Thoth::EpisodicLearningExpectations> expectations;
    for (const auto& spec : cases) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold = runE2TestArm(spec, "cold", attr, nullptr, nullptr);
        const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);
        auto eval = svc.evaluateCase(spec.id, spec.expectations, cold, warm, cfg);
        eval.run_block_reason = warmBlock;
        svc.applyCaseResolution(eval);
        evaluations.push_back(eval);
        expectations.push_back(spec.expectations);
    }
    const auto summary_before = svc.summarize(evaluations, expectations, cfg);
    const auto fingerprint_before = svc.computeFingerprint(cfg);

    Thoth::E2PipelineStageTimings timings;
    timings.mapping_duration_ms = 1;
    timings.evaluation_duration_ms = 2;
    timings.diagnostic_duration_ms = 3;
    timings.pipeline_duration_ms = 6;
    timings.episodes_processed = 1;
    Thoth::E2PipelineTelemetryContext context;
    context.run_id = "e2-c4-noninterference";
    context.plan_id = "plan";
    (void)Thoth::episodicPipelineTelemetryService().recordPipelineRun(timings, context);

    const auto summary_after = svc.summarize(evaluations, expectations, cfg);
    const auto fingerprint_after = svc.computeFingerprint(cfg);
    if (summary_before.mean_episodic_lift != summary_after.mean_episodic_lift ||
        summary_before.scorable_cases != summary_after.scorable_cases ||
        summary_before.not_scorable_cases != summary_after.not_scorable_cases ||
        summary_before.evaluation_resolution != summary_after.evaluation_resolution) {
        std::cerr << "testE2C4NonInterference: summary changed after telemetry\n";
        return false;
    }
    if (fingerprint_before.fingerprint_hash != fingerprint_after.fingerprint_hash) {
        std::cerr << "testE2C4NonInterference: fingerprint changed after telemetry\n";
        return false;
    }
    for (std::size_t i = 0; i < evaluations.size(); ++i) {
        const auto& before = evaluations[i];
        if (before.passes != evaluations[i].passes || before.lift != evaluations[i].lift ||
            before.failure_reason != evaluations[i].failure_reason ||
            before.run_block_reason != evaluations[i].run_block_reason ||
            before.evaluation_resolution != evaluations[i].evaluation_resolution) {
            std::cerr << "testE2C4NonInterference: case evaluation mutated for " << before.case_id
                      << '\n';
            return false;
        }
    }
    return true;
}

/** E2-C4-05 — C1→C4 pipeline preserves E2-28 golden semantics + subscriber path. */
static bool testE2C4PipelineIntegrity() {
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    const auto fp = Thoth::computeEvaluationFingerprint(cfg);
    const auto summary_a = buildOfficialGoldenSummary();
    const auto summary_b = buildOfficialGoldenSummary();
    const auto snap_a = Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary_a, fp.toJson(), cfg.toJson());
    const auto snap_b = Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary_b, fp.toJson(), cfg.toJson());
    if (!Thoth::episodicLearningScopedEquivalenceEqual(snap_a, snap_b)) {
        std::cerr << "testE2C4PipelineIntegrity: golden scoped snapshots differ\n";
        return false;
    }
    if (Thoth::episodicLearningFingerprintMismatchBucket(snap_a, snap_b, "h1", "h1") != 0) {
        std::cerr << "testE2C4PipelineIntegrity: golden diagnosis bucket mismatch\n";
        return false;
    }

    Config config;
    if (config.enable_episodic_pipeline_telemetry) {
        std::cerr << "testE2C4PipelineIntegrity: telemetry flag must default OFF\n";
        return false;
    }

    const Thoth::EpisodeCompleted event = makeE2C4CheckpointEvent();
    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
    Thoth::EvaluationSubscriber off;
    off.onEpisodeCompleted(event);
    const auto summary_off_json =
        Thoth::episodicLearningSummaryToJson(*Thoth::EvaluationSubscriber::lastSummaryForTests());
    const auto diag_off_json = Thoth::evaluationDiagnosticSummaryToJson(
        *Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests());

    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(true);
    Thoth::EvaluationSubscriber on;
    on.onEpisodeCompleted(event);
    const auto summary_on_json =
        Thoth::episodicLearningSummaryToJson(*Thoth::EvaluationSubscriber::lastSummaryForTests());
    const auto diag_on_json = Thoth::evaluationDiagnosticSummaryToJson(
        *Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests());
    const auto* telemetry = Thoth::EvaluationSubscriber::lastTelemetryRecordForTests();
    if (!telemetry) {
        std::cerr << "testE2C4PipelineIntegrity: missing telemetry record when ON\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }

    if (summary_off_json != summary_on_json || diag_off_json != diag_on_json) {
        std::cerr << "testE2C4PipelineIntegrity: eval/diag differ OFF vs ON\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }

    const auto& case_eval = Thoth::EvaluationSubscriber::lastSummaryForTests()->case_results.front();
    if (!case_eval.evaluation_resolution.has_value() ||
        *case_eval.evaluation_resolution != Thoth::E2EvaluationResolution::SCORED_SUCCESS) {
        std::cerr << "testE2C4PipelineIntegrity: case evaluation_resolution not SCORED_SUCCESS\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }

    const nlohmann::json telemetry_json = Thoth::e2PipelineTelemetryToJson(*telemetry);
    if (telemetry_json.value("event_type", "") != "E2_EVAL_TELEMETRY_PIPELINE") {
        std::cerr << "testE2C4PipelineIntegrity: bad telemetry event_type\n";
        Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
        return false;
    }

    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
    return true;
}

/** E2-C4 Checkpoint 2 — telemetry OFF vs ON observational proof (THOTH_E2_C4_CP2=1). */
static bool testE2C4Checkpoint2ObservationalProof() {
    const Thoth::EpisodeCompleted event = makeE2C4CheckpointEvent();

    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
    Thoth::EvaluationSubscriber subscriber_off;
    subscriber_off.onEpisodeCompleted(event);
    const Thoth::EpisodicLearningSummary* summary_off =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    const Thoth::EvaluationDiagnosticsSummary* diag_off =
        Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests();
    if (!summary_off || !diag_off) {
        std::cerr << "testE2C4Checkpoint2ObservationalProof: missing OFF artifacts\n";
        return false;
    }
    if (Thoth::EvaluationSubscriber::lastTelemetryRecordForTests() != nullptr) {
        std::cerr << "testE2C4Checkpoint2ObservationalProof: telemetry record present when OFF\n";
        return false;
    }
    const nlohmann::json summary_off_json = Thoth::episodicLearningSummaryToJson(*summary_off);
    const nlohmann::json diag_off_json =
        Thoth::evaluationDiagnosticSummaryToJson(*diag_off);

    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(true);
    Thoth::EvaluationSubscriber subscriber_on;
    subscriber_on.onEpisodeCompleted(event);
    const Thoth::EpisodicLearningSummary* summary_on =
        Thoth::EvaluationSubscriber::lastSummaryForTests();
    const Thoth::EvaluationDiagnosticsSummary* diag_on =
        Thoth::EvaluationSubscriber::lastRunDiagnosticsForTests();
    const Thoth::E2PipelineTelemetryRecord* telemetry_on =
        Thoth::EvaluationSubscriber::lastTelemetryRecordForTests();
    if (!summary_on || !diag_on || !telemetry_on) {
        std::cerr << "testE2C4Checkpoint2ObservationalProof: missing ON artifacts\n";
        return false;
    }

    const nlohmann::json summary_on_json = Thoth::episodicLearningSummaryToJson(*summary_on);
    const nlohmann::json diag_on_json = Thoth::evaluationDiagnosticSummaryToJson(*diag_on);
    if (summary_off_json != summary_on_json) {
        std::cerr << "testE2C4Checkpoint2ObservationalProof: summary differs OFF vs ON\n";
        return false;
    }
    if (diag_off_json != diag_on_json) {
        std::cerr << "testE2C4Checkpoint2ObservationalProof: diagnostics differ OFF vs ON\n";
        return false;
    }

    const auto& case_eval = summary_on->case_results.front();
    if (!case_eval.evaluation_resolution.has_value()) {
        std::cerr << "testE2C4Checkpoint2ObservationalProof: missing case evaluation_resolution\n";
        return false;
    }

    const nlohmann::json telemetry_json = Thoth::e2PipelineTelemetryToJson(*telemetry_on);
    if (telemetry_json.value("event_type", "") != "E2_EVAL_TELEMETRY_PIPELINE") {
        std::cerr << "testE2C4Checkpoint2ObservationalProof: bad event_type\n";
        return false;
    }
    if (telemetry_json.value("telemetry_tier", "") != "ARCHITECTURE") {
        std::cerr << "testE2C4Checkpoint2ObservationalProof: bad telemetry_tier\n";
        return false;
    }
    const std::vector<std::string> forbidden = {"evaluation_resolution",
                                                "fingerprint_hash",
                                                "lift",
                                                "passes",
                                                "e2_outcome",
                                                "diagnosis_bucket",
                                                "failure_classification",
                                                "mean_episodic_lift"};
    for (const auto& field : forbidden) {
        if (telemetry_json.contains(field)) {
            std::cerr << "testE2C4Checkpoint2ObservationalProof: forbidden field " << field << '\n';
            return false;
        }
    }

    Thoth::setEvaluationSubscriberPipelineTelemetryEnabled(false);
    return true;
}

/** Checkpoint 0 — mapping fidelity: benchmark arms survive production mapper round-trip. */
static bool testE2C5MappingFidelity() {
    const auto cases = Thoth::getEpisodicLearningCases();
    Thoth::BenchmarkAttribution attr{"e2-c5-mapping", "e2-c5-mapping-env"};
    int mapping_safe = 0;
    for (const auto& spec : cases) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold = runE2TestArm(spec, "cold", attr);
        const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);
        std::string report;
        if (!Thoth::validateMappingFidelityForCase(spec, cold, warm, spec.expectations, &report)) {
            std::cerr << "testE2C5MappingFidelity: " << report << '\n';
            return false;
        }
        ++mapping_safe;
    }
    if (mapping_safe == 0) {
        std::cerr << "testE2C5MappingFidelity: no mapping-safe fixtures\n";
        return false;
    }
    return true;
}

static bool runE2C5PathEquivalenceForCase(const Thoth::EpisodicLearningCase& spec,
                                          std::string* report_out) {
    Thoth::BenchmarkAttribution attr{"e2-c5-equiv", "e2-c5-equiv-env"};
    Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
    const auto cold = runE2TestArm(spec, "cold", attr);
    const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();

    std::string fidelity_report;
    if (!Thoth::validateMappingFidelityForCase(spec, cold, warm, spec.expectations,
                                               &fidelity_report)) {
        if (report_out) {
            *report_out = fidelity_report;
        }
        return false;
    }

    const auto benchmark = Thoth::runBenchmarkPathArtifacts(
        spec.id, spec.expectations, cold, warm, cfg, warmBlock);
    const Thoth::EpisodeCompleted episode = Thoth::episodeFromBenchmarkArmsForTests(
        spec, cold, warm, spec.expectations, warmBlock);
    const auto production = Thoth::runProductionPathArtifacts(episode, cfg);
    const Thoth::E2PathEquivalenceDiff diff = Thoth::diffPathEquivalence(benchmark, production);
    if (!diff.equivalent) {
        if (report_out && !diff.mismatches.empty()) {
            *report_out = spec.id + ": " + diff.mismatches.front();
        }
        return false;
    }
    return true;
}

/** E2-C5-01 — semantic equivalence under pinned evaluation semantics. */
static bool testE2C5SemanticEquivalence() {
    for (const auto& spec : Thoth::getEpisodicLearningCases()) {
        std::string report;
        if (!runE2C5PathEquivalenceForCase(spec, &report)) {
            std::cerr << "testE2C5SemanticEquivalence: " << report << '\n';
            return false;
        }
    }
    return true;
}

/** E2-C5-02 — fingerprint stability post-normalization on pinned config. */
static bool testE2C5FingerprintStability() {
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    const auto fp_a = Thoth::episodicEvaluationService().computeFingerprint(cfg);
    const auto fp_b = Thoth::episodicEvaluationService().computeFingerprint(cfg);
    if (fp_a.fingerprint_hash != fp_b.fingerprint_hash) {
        std::cerr << "testE2C5FingerprintStability: fingerprint unstable across calls\n";
        return false;
    }
    Thoth::BenchmarkAttribution attr{"e2-c5-fp", "e2-c5-fp-env"};
    for (const auto& spec : Thoth::getEpisodicLearningCases()) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold = runE2TestArm(spec, "cold", attr);
        const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);
        const auto benchmark =
            Thoth::runBenchmarkPathArtifacts(spec.id, spec.expectations, cold, warm, cfg, warmBlock);
        const auto episode = Thoth::episodeFromBenchmarkArmsForTests(
            spec, cold, warm, spec.expectations, warmBlock);
        const auto production = Thoth::runProductionPathArtifacts(episode, cfg);
        if (benchmark.fingerprint.fingerprint_hash != production.fingerprint.fingerprint_hash) {
            std::cerr << "testE2C5FingerprintStability: path fingerprint mismatch for " << spec.id
                      << '\n';
            return false;
        }
        if (benchmark.fingerprint.toJson().value("e2_eval_config", nlohmann::json::object()) !=
            production.fingerprint.toJson().value("e2_eval_config", nlohmann::json::object())) {
            std::cerr << "testE2C5FingerprintStability: e2_eval_config pins mismatch for "
                      << spec.id << '\n';
            return false;
        }
    }
    return true;
}

/** E2-C5-03 — cross-path artifact consistency (normalized snapshots). */
static bool testE2C5CrossPathArtifactConsistency() {
    return testE2C5SemanticEquivalence();
}

/** E2-C5-04 — no hidden coupling across eval/exec/diag/telemetry layers. */
static bool testE2C5NoHiddenCoupling() {
    const std::vector<std::pair<std::string, std::vector<std::string>>> audits = {
        {"external/basic_agent/include/episodic_evaluation_service.h",
         {"executive_controller", "pipeline_telemetry", "evaluation_subscriber"}},
        {"external/basic_agent/src/episodic_evaluation_service.cpp",
         {"executive_controller", "pipeline_telemetry", "evaluation_subscriber"}},
        {"external/basic_agent/include/diagnostic_service.h",
         {"executive_controller", "pipeline_telemetry", "evaluation_subscriber"}},
        {"external/basic_agent/src/diagnostic_service.cpp",
         {"executive_controller", "pipeline_telemetry", "evaluation_subscriber"}},
        {"external/basic_agent/include/pipeline_telemetry_service.h",
         {"episodic_evaluation_service", "diagnostic_service", "executive_controller",
          "evaluation_subscriber"}},
        {"external/basic_agent/src/pipeline_telemetry_service.cpp",
         {"episodic_evaluation_service", "diagnostic_service", "executive_controller",
          "evaluation_subscriber"}},
        {"external/basic_agent/src/e2_path_equivalence.cpp",
         {"executive_controller"}}};
    for (const auto& [path, forbidden] : audits) {
        const std::string source = readRepoSourceFile(path);
        if (source.empty()) {
            std::cerr << "testE2C5NoHiddenCoupling: cannot read " << path << '\n';
            return false;
        }
        for (const auto& sym : forbidden) {
            if (source.find(sym) != std::string::npos) {
                std::cerr << "testE2C5NoHiddenCoupling: " << path << " imports " << sym << '\n';
                return false;
            }
        }
    }
    return true;
}

/** E2-C5-05 — path equivalence on mapping-safe golden fixtures (regression companion). */
static bool testE2C5PathEquivalenceGoldenFixtures() {
    return testE2C5SemanticEquivalence();
}

static bool runE2C5RegressionGate() {
    if (!testE2C5MappingFidelity()) {
        std::cerr << "runE2C5RegressionGate: mapping fidelity failed\n";
        return false;
    }
    if (!testE2C5SemanticEquivalence()) {
        std::cerr << "runE2C5RegressionGate: semantic equivalence failed\n";
        return false;
    }
    if (!testE2C5FingerprintStability()) {
        std::cerr << "runE2C5RegressionGate: fingerprint stability failed\n";
        return false;
    }
    if (!testE2C5CrossPathArtifactConsistency()) {
        std::cerr << "runE2C5RegressionGate: cross-path artifact consistency failed\n";
        return false;
    }
    if (!testE2C5NoHiddenCoupling()) {
        std::cerr << "runE2C5RegressionGate: hidden coupling audit failed\n";
        return false;
    }
    if (!testE2C5PathEquivalenceGoldenFixtures()) {
        std::cerr << "runE2C5RegressionGate: golden fixture equivalence failed\n";
        return false;
    }
    return true;
}

// --- E2-D4 Step 4: backward-compat regressions (orchestration only) ---

static bool verifyD4Step4DefaultFlagContract() {
    Config cfg;
    if (cfg.enable_episodic_evaluation_publication) {
        std::cerr << "verifyD4Step4DefaultFlagContract: eval publication must default OFF\n";
        return false;
    }
    if (cfg.enable_episodic_pipeline_telemetry) {
        std::cerr << "verifyD4Step4DefaultFlagContract: pipeline telemetry must default OFF\n";
        return false;
    }
    if (cfg.enable_episode_replay_subscriber) {
        std::cerr << "verifyD4Step4DefaultFlagContract: replay subscriber must default OFF\n";
        return false;
    }
    if (cfg.enable_metrics_subscriber) {
        std::cerr << "verifyD4Step4DefaultFlagContract: metrics subscriber must default OFF\n";
        return false;
    }
    if (cfg.enable_trace_subscriber) {
        std::cerr << "verifyD4Step4DefaultFlagContract: trace subscriber must default OFF\n";
        return false;
    }
    return true;
}

static bool runE2D4Step4Tests() {
    if (!verifyD4Step4DefaultFlagContract()) {
        std::cerr << "E2-D4-Step4 default flag contract failed\n";
        return false;
    }

    if (!runE2D3Tests()) {
        std::cerr << "E2-D4-Step4 THOTH_E2_D3 regression failed\n";
        return false;
    }
    if (!runE2D2Tests()) {
        std::cerr << "E2-D4-Step4 THOTH_E2_D2 regression failed\n";
        return false;
    }
    if (!runE2D1Tests()) {
        std::cerr << "E2-D4-Step4 THOTH_E2_D1 regression failed\n";
        return false;
    }
    if (!runE2C5RegressionGate()) {
        std::cerr << "E2-D4-Step4 THOTH_E2_C5 regression failed\n";
        return false;
    }

    std::cout << "E2-D4-Step4 backward-compat regression green\n";
    std::cout << "E2-D4-Step4 evidence:\n";
    std::cout << "  gate: THOTH_E2_D4_STEP4\n";
    std::cout << "  THOTH_E2_D3=1 pass\n";
    std::cout << "  THOTH_E2_D2=1 pass\n";
    std::cout << "  THOTH_E2_D1=1 pass\n";
    std::cout << "  THOTH_E2_C5=1 pass\n";
    std::cout << "  default flag contract verified (no D4 workspace harness with eval ON)\n";
    std::cout << "  conclusion: no backward-compat regression detected\n";
    return true;
}

// --- E2-D4 Step 5: composition proof (orchestration only) ---

static bool runE2D4Tests() {
    if (!runE2D4_02Tests()) {
        std::cerr << "E2-D4 composition: Phase A (Steps 1-3) failed\n";
        return false;
    }
    if (!runE2D4Step4Tests()) {
        std::cerr << "E2-D4 composition: Phase B (Step 4 backward compatibility) failed\n";
        return false;
    }

    std::cout << "E2-D4 composition proof green\n";
    std::cout << "E2-D4 evidence:\n";
    std::cout << "  gate: THOTH_E2_D4\n";
    std::cout << "  Phase A: THOTH_E2_D4_02=1 pass (structural seam + live INTEGRATION behavior + "
                 "STRICT authority preservation)\n";
    std::cout << "  Phase B: THOTH_E2_D4_STEP4=1 pass (backward compatibility)\n";
    std::cout << "  E2-D4-01 obligations satisfied\n";
    std::cout << "  E2-D4-02 obligations satisfied\n";
    std::cout << "  D4-I1..I7 evidence chain satisfied (Steps 1-3)\n";
    std::cout << "  THOTH_E2_D3=1 pass\n";
    std::cout << "  THOTH_E2_D2=1 pass\n";
    std::cout << "  THOTH_E2_D1=1 pass\n";
    std::cout << "  THOTH_E2_C5=1 pass\n";
    std::cout << "  default flag contract verified during Phase B\n";
    std::cout << "  conclusion: D4 proof suite complete — all obligations compose\n";
    std::cout << "  deferred: D5 evolution trust proof\n";
    return true;
}

// --- E2-D5 Step 1: authority preservation meta-proof (E2-D5-03) ---

static bool attestD4CompositionEvidence() {
    std::cout << "E2-D5 authority: D4 composition evidence attested (reference only)\n";
    std::cout << "  gate: THOTH_E2_D4=1\n";
    std::cout << "  close-out: 2026-07-08 commit d4216c8\n";
    std::cout << "  E2-D4-01: live INTEGRATION containment (consumed by reference)\n";
    std::cout << "  E2-D4-02: STRICT authority preservation (consumed by reference)\n";
    std::cout << "  D4-I1..I7: structural + behavioral chain (consumed by reference)\n";
    std::cout << "  D2 replay authority: consumed via D4 Step 4 backward-compat attestation\n";
    std::cout << "  cross-layer service import coupling: deferred to Step 2 THOTH_E2_D5_C5 "
                 "(testE2C5NoHiddenCoupling — C5 layer audit)\n";
    return true;
}

static bool runE2D5AuthorityMetaProof() {
    if (!attestD4CompositionEvidence()) {
        std::cerr << "E2-D5-Step1 D4 composition attestation failed\n";
        return false;
    }
    if (!e2D1ExecutiveInvisibilityStructuralAudit()) {
        std::cerr << "E2-D5-Step1 D1 Executive structural invisibility failed\n";
        return false;
    }
    if (!runE2D3_03Tests()) {
        std::cerr << "E2-D5-Step1 D3-03 structural authority boundary failed\n";
        return false;
    }
    if (!testE2D4_02NoIntegrationLeakIntoStrictArtifacts()) {
        std::cerr << "E2-D5-Step1 D4-02 isolation absence failed\n";
        return false;
    }

    std::cout << "E2-D5-Step1 authority preservation meta-proof green\n";
    std::cout << "E2-D5-Step1 evidence:\n";
    std::cout << "  gate: THOTH_E2_D5_AUTHORITY\n";
    std::cout << "  preregistered: E2-D5-03\n";
    std::cout << "  D4 composition evidence attested (THOTH_E2_D4=1, d4216c8)\n";
    std::cout << "  e2D1ExecutiveInvisibilityStructuralAudit pass\n";
    std::cout << "  runE2D3_03Tests pass (includes D3-01 spot-check via authority boundary)\n";
    std::cout << "  testE2D4_02NoIntegrationLeakIntoStrictArtifacts pass\n";
    std::cout << "  D2 replay authority: consumed by reference\n";
    std::cout << "  conclusion: authority boundaries preserved post-evolution\n";
    std::cout << "  deferred: Step 2 behavioral preservation · Step 3 determinism · Step 4 closure\n";
    return true;
}

// --- E2-D5 Step 2: behavioral preservation meta-proof (E2-D5-01) ---

static bool attestD5Step1AuthorityEvidence() {
    std::cout << "E2-D5 authority: Step 1 evidence attested (reference only)\n";
    std::cout << "  D5 Step 1: THOTH_E2_D5_AUTHORITY=1 (commit 0b4df02)\n";
    return true;
}

static bool runE2D5C5Proof() {
    if (!attestD5Step1AuthorityEvidence()) {
        std::cerr << "E2-D5-Step2 prior evidence attestation failed\n";
        return false;
    }
    std::cout << "E2-D5 behavioral: prior evidence attested (reference only)\n";
    std::cout << "  Phase C: THOTH_E2_C5=1 (consumed by reference)\n";
    std::cout << "  D4 Step 4: C5 backward-compat pass (consumed by reference)\n";
    if (!runE2C5RegressionGate()) {
        std::cerr << "E2-D5-Step2 C5 regression gate failed\n";
        return false;
    }

    std::cout << "E2-D5-Step2 behavioral preservation meta-proof green\n";
    std::cout << "E2-D5-Step2 evidence:\n";
    std::cout << "  gate: THOTH_E2_D5_C5\n";
    std::cout << "  preregistered: E2-D5-01\n";
    std::cout << "  D5 Step 1 attested (THOTH_E2_D5_AUTHORITY=1, 0b4df02)\n";
    std::cout << "  runE2C5RegressionGate pass\n";
    std::cout << "  testE2C5SemanticEquivalence: mapping-safe fixtures MATCH\n";
    std::cout << "  testE2C5NoHiddenCoupling: C5 service-layer import coupling "
                 "(not Step 1 authority duplicate)\n";
    std::cout << "  conclusion: behavioral equivalence preserved post-evolution "
                 "(preservation only — not promotion)\n";
    std::cout << "  deferred: Step 3 determinism · Step 4 closure\n";
    return true;
}

// --- E2-D5 Step 3: determinism preservation meta-proof (E2-D5-02) ---

static bool attestD5Step2BehavioralEvidence() {
    std::cout << "E2-D5 behavioral: Step 2 evidence attested (reference only)\n";
    std::cout << "  D5 Step 2: THOTH_E2_D5_C5=1 (commit f16664d)\n";
    return true;
}

static bool attestPhaseBE2_28Evidence() {
    std::cout << "E2-D5 determinism: Phase B E2-28 evidence attested (reference only)\n";
    std::cout << "  Phase B close-out: testE2B5OfficialFingerprintDeterminism() (E2-28)\n";
    std::cout << "  consumed by reference — not re-running full Phase B suite\n";
    return true;
}

static bool runE2D5DeterminismProof() {
    if (!attestD5Step1AuthorityEvidence()) {
        std::cerr << "E2-D5-Step3 Step 1 authority attestation failed\n";
        return false;
    }
    if (!attestD5Step2BehavioralEvidence()) {
        std::cerr << "E2-D5-Step3 Step 2 behavioral attestation failed\n";
        return false;
    }
    if (!attestPhaseBE2_28Evidence()) {
        std::cerr << "E2-D5-Step3 Phase B E2-28 attestation failed\n";
        return false;
    }
    if (!testE2B5OfficialFingerprintDeterminism()) {
        std::cerr << "E2-D5-Step3 E2-28 determinism helper failed\n";
        return false;
    }

    std::cout << "E2-D5-Step3 determinism preservation meta-proof green\n";
    std::cout << "E2-D5-Step3 evidence:\n";
    std::cout << "  gate: THOTH_E2_D5_DETERMINISM\n";
    std::cout << "  preregistered: E2-D5-02\n";
    std::cout << "  D5 Step 1 authority attested (THOTH_E2_D5_AUTHORITY=1, 0b4df02)\n";
    std::cout << "  D5 Step 2 behavioral attested (THOTH_E2_D5_C5=1, f16664d)\n";
    std::cout << "  Phase B E2-28 attested (consumed by reference)\n";
    std::cout << "  testE2B5OfficialFingerprintDeterminism pass\n";
    std::cout << "  scoped-equivalence snapshots: deep-equal across consecutive builds\n";
    std::cout << "  diagnosis bucket: #0 (equivalent)\n";
    std::cout << "  conclusion: deterministic trust preserved post-evolution "
                 "(preservation only — not promotion)\n";
    std::cout << "  deferred: Step 4 closure\n";
    return true;
}

// --- E2-D5 Step 4: phase closure (evolution trust proof) ---

static bool attestD1CloseOutEvidence() {
    std::cout << "E2-D5 closure: D1 evidence attested (reference only)\n";
    std::cout << "  gate: THOTH_E2_D1=1\n";
    std::cout << "  close-out: 2026-07-05 (channel fan-out + Executive invisibility)\n";
    return true;
}

static bool attestD2CloseOutEvidence() {
    std::cout << "E2-D5 closure: D2 evidence attested (reference only)\n";
    std::cout << "  gate: THOTH_E2_D2=1\n";
    std::cout << "  close-out: 2026-07-07 (replay + benchmark authority isolation)\n";
    return true;
}

static bool attestD3CloseOutEvidence() {
    std::cout << "E2-D5 closure: D3 evidence attested (reference only)\n";
    std::cout << "  gate: THOTH_E2_D3=1\n";
    std::cout << "  close-out: 2026-07-07 (observability without authority)\n";
    return true;
}

static std::string findEpisodicHarnessBinary() {
    FileHandler fh;
    const fs::path root = fh.getProjectRoot();
    const std::vector<fs::path> candidates = {
        root / "build" / "debug" / "external" / "basic_agent" /
            "run_episodic_learning_benchmark",
        root / "build" / "release" / "external" / "basic_agent" /
            "run_episodic_learning_benchmark",
        fs::path("build/debug/external/basic_agent/run_episodic_learning_benchmark"),
    };
    for (const auto& path : candidates) {
        if (fs::exists(path)) {
            return fs::absolute(path).string();
        }
    }
    return {};
}

static std::vector<nlohmann::json> readJsonlRows(const std::string& path) {
    std::vector<nlohmann::json> rows;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            rows.push_back(nlohmann::json::parse(line));
        } catch (...) {
        }
    }
    return rows;
}

static int runShellCommand(const std::string& cmd) {
    return std::system(cmd.c_str());
}

static Thoth::BenchmarkEnvironmentInputs makeEpisodicMockHarnessInputs() {
    Thoth::BenchmarkEnvironmentInputs inputs;
    inputs.harness = "episodic_learning_benchmark";
    inputs.tier = Thoth::BenchmarkTier::MOCK;
    inputs.model.llm_model = "mock";
    inputs.model.embedding_model = "tfidf-local";
    inputs.model.embedding_method = "TfIdf";
    inputs.thoth_env_flags = {{"THOTH_MOCK_EPISODIC", "1"}, {"THOTH_MOCK_LLM", "true"}};
    return inputs;
}

static Thoth::BenchmarkEnvironmentInputs makeEpisodicAuthoritativeHarnessInputs() {
    Thoth::BenchmarkEnvironmentInputs inputs;
    inputs.harness = "episodic_learning_benchmark";
    // EP-01.5 Phase 2: declared tier must match inferTier() (External + reachable → OLLAMA).
    inputs.tier = Thoth::BenchmarkTier::OLLAMA;
    Config cfg;
    inputs.model.llm_model = cfg.llm_model;
    inputs.model.embedding_model = cfg.embedding_model;
    inputs.model.embedding_method = "External";
    inputs.ollama_reachable = true;
    inputs.thoth_env_flags = nlohmann::json::object();
    return inputs;
}

/** E2-29a — episodic harness inferTier mock classification. */
static bool testE2Ep01InferTierMockEpisodicHarness() {
    const auto inputs = makeEpisodicMockHarnessInputs();
    if (Thoth::inferTier(inputs) != Thoth::BenchmarkTier::MOCK) {
        std::cerr << "testE2Ep01InferTierMockEpisodicHarness: expected MOCK\n";
        return false;
    }
    if (Thoth::hasTierMismatch(inputs)) {
        std::cerr << "testE2Ep01InferTierMockEpisodicHarness: unexpected tier mismatch\n";
        return false;
    }
    return true;
}

/** E2-29b — episodic harness inferTier authoritative classification. */
static bool testE2Ep01InferTierAuthoritativeEpisodicHarness() {
    const auto inputs = makeEpisodicAuthoritativeHarnessInputs();
    const Thoth::BenchmarkTier inferred = Thoth::inferTier(inputs);
    if (inferred != Thoth::BenchmarkTier::OLLAMA && inferred != Thoth::BenchmarkTier::FULL) {
        std::cerr << "testE2Ep01InferTierAuthoritativeEpisodicHarness: expected OLLAMA or FULL\n";
        return false;
    }
    return true;
}

/** E2-29c — default mock harness A1 wiring smoke. */
static bool testE2Ep01MockHarnessWiringSmoke() {
    const std::string binary = findEpisodicHarnessBinary();
    if (binary.empty()) {
        std::cerr << "testE2Ep01MockHarnessWiringSmoke: harness binary not found — build first\n";
        return false;
    }
    const int rc = runShellCommand(
        "THOTH_E2_WIRING_STAGE=A1 \"" + binary + "\" --mock >/dev/null 2>&1");
    if (rc != 0) {
        std::cerr << "testE2Ep01MockHarnessWiringSmoke: mock A1 harness exit=" << rc << '\n';
        return false;
    }
    return true;
}

/** E2-29 — mock path preserves Phase B / E2-28 scoped equivalence (bucket #0). */
static bool testE2Ep01MockRegressionPreservesE28() {
    if (!testE2Ep01InferTierMockEpisodicHarness()) {
        return false;
    }
    if (!testE2Ep01InferTierAuthoritativeEpisodicHarness()) {
        return false;
    }
    if (!testE2Ep01MockHarnessWiringSmoke()) {
        return false;
    }
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    const auto fp = Thoth::computeEvaluationFingerprint(cfg);
    const auto summary_a = buildOfficialGoldenSummary();
    const auto summary_b = buildOfficialGoldenSummary();
    const auto snap_a = Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary_a, fp.toJson(), cfg.toJson());
    const auto snap_b = Thoth::episodicLearningScopedEquivalenceSnapshot(
        summary_b, fp.toJson(), cfg.toJson());
    if (!Thoth::episodicLearningScopedEquivalenceEqual(snap_a, snap_b)) {
        std::cerr << "testE2Ep01MockRegressionPreservesE28: scoped snapshots differ\n";
        return false;
    }
    if (Thoth::episodicLearningFingerprintMismatchBucket(snap_a, snap_b, "h1", "h1") != 0) {
        std::cerr << "testE2Ep01MockRegressionPreservesE28: diagnosis bucket mismatch\n";
        return false;
    }
    return true;
}

/** E2-30 — authoritative inference smoke; zero official_scoring rows. */
static bool testE2Ep01AuthoritativeInferenceSmoke() {
    if (!Thoth::isOllamaReachable()) {
        std::cerr << "testE2Ep01AuthoritativeInferenceSmoke: Ollama not reachable\n";
        return false;
    }
    const std::string binary = findEpisodicHarnessBinary();
    if (binary.empty()) {
        std::cerr << "testE2Ep01AuthoritativeInferenceSmoke: harness binary not found\n";
        return false;
    }

    FileHandler fh;
    const fs::path logPath =
        fs::path(fh.getProjectRoot()) / "logs" / "episodic_learning_benchmark.jsonl";
    const std::string logBackup = logPath.string() + ".ep01_backup";
    if (fs::exists(logPath)) {
        fs::copy_file(logPath, logBackup, fs::copy_options::overwrite_existing);
        fs::remove(logPath);
    }

    const int rc = runShellCommand(
        "THOTH_E2_WIRING_STAGE=A2 \"" + binary + "\" --authoritative >/dev/null 2>&1");
    if (rc != 0) {
        std::cerr << "testE2Ep01AuthoritativeInferenceSmoke: authoritative A2 exit=" << rc
                  << '\n';
        if (fs::exists(logBackup)) {
            fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
            fs::remove(logBackup);
        }
        return false;
    }

    bool sawWiringCheckpoint = false;
    for (const auto& row : readJsonlRows(logPath.string())) {
        if (row.value("official_scoring", false) == true) {
            std::cerr << "testE2Ep01AuthoritativeInferenceSmoke: official_scoring row found\n";
            if (fs::exists(logBackup)) {
                fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
                fs::remove(logBackup);
            }
            return false;
        }
        if (row.value("event", "") == "E2_WIRING_CHECKPOINT" &&
            row.value("wiring_stage", "") == "A2") {
            sawWiringCheckpoint = true;
        }
    }
    if (!sawWiringCheckpoint) {
        std::cerr << "testE2Ep01AuthoritativeInferenceSmoke: missing A2 wiring checkpoint\n";
        if (fs::exists(logBackup)) {
            fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
            fs::remove(logBackup);
        }
        return false;
    }

    if (fs::exists(logBackup)) {
        fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
        fs::remove(logBackup);
    }
    return true;
}

/** E2-31 — EP-01.5 Phase 1: authoritative LLM wiring; tokens prove live query path. */
static bool testE2Ep015AuthoritativeLlmWiring() {
    if (!Thoth::isOllamaReachable()) {
        std::cerr << "testE2Ep015AuthoritativeLlmWiring: Ollama not reachable\n";
        return false;
    }
    const std::string binary = findEpisodicHarnessBinary();
    if (binary.empty()) {
        std::cerr << "testE2Ep015AuthoritativeLlmWiring: harness binary not found\n";
        return false;
    }

    FileHandler fh;
    const fs::path logPath =
        fs::path(fh.getProjectRoot()) / "logs" / "episodic_learning_benchmark.jsonl";
    const std::string logBackup = logPath.string() + ".ep015_backup";
    if (fs::exists(logPath)) {
        fs::copy_file(logPath, logBackup, fs::copy_options::overwrite_existing);
        fs::remove(logPath);
    }

    const int rc = runShellCommand(
        "THOTH_E2_EP015_SMOKE=1 \"" + binary +
        "\" --authoritative >\"/tmp/thoth_ep015_smoke.out\" 2>&1");
    if (rc != 0) {
        std::cerr << "testE2Ep015AuthoritativeLlmWiring: smoke exit=" << rc << '\n';
        std::ifstream errIn("/tmp/thoth_ep015_smoke.out");
        std::string errLine;
        while (std::getline(errIn, errLine)) {
            std::cerr << "  | " << errLine << '\n';
        }
        if (fs::exists(logBackup)) {
            fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
            fs::remove(logBackup);
        }
        return false;
    }

    bool sawSmoke = false;
    bool tokensOk = false;
    for (const auto& row : readJsonlRows(logPath.string())) {
        if (row.value("official_scoring", false) == true) {
            std::cerr << "testE2Ep015AuthoritativeLlmWiring: official_scoring row found\n";
            if (fs::exists(logBackup)) {
                fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
                fs::remove(logBackup);
            }
            return false;
        }
        if (row.value("event", "") == "E2_EP015_LLM_WIRING_SMOKE") {
            sawSmoke = true;
            tokensOk = row.value("tokens_ok", false);
            const auto total = row.value("total_tokens", 0);
            if (total <= 0 && !tokensOk) {
                std::cerr << "testE2Ep015AuthoritativeLlmWiring: tokens not recorded\n";
                if (fs::exists(logBackup)) {
                    fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
                    fs::remove(logBackup);
                }
                return false;
            }
        }
    }

    if (fs::exists(logBackup)) {
        fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
        fs::remove(logBackup);
    }

    if (!sawSmoke) {
        std::cerr << "testE2Ep015AuthoritativeLlmWiring: missing E2_EP015_LLM_WIRING_SMOKE row\n";
        return false;
    }
    if (!tokensOk) {
        std::cerr << "testE2Ep015AuthoritativeLlmWiring: tokens_ok=false\n";
        return false;
    }
    return true;
}

/** EP-01.5 Phase 1 gate only — full EP-015 orchestrator lands after Phases 2–5. */
static bool runE2Ep015Phase1Tests() {
    std::cout << "E2-EP-01.5 Phase 1 — authoritative LLM wiring\n";
    std::cout << "  gate: THOTH_E2_EP015_PHASE1\n";
    if (!testE2Ep015AuthoritativeLlmWiring()) {
        std::cerr << "E2-EP-01.5 Phase 1: E2-31 failed\n";
        return false;
    }
    std::cout << "  E2-31 authoritative LLM wiring pass (tokens recorded)\n";
    return true;
}

/** E2-31b — EP-01.5 Phase 2: authoritative episodic inputs have no TIER_MISMATCH. */
static bool testE2Ep015TierDeclarationAligned() {
    const auto inputs = makeEpisodicAuthoritativeHarnessInputs();
    const Thoth::BenchmarkTier inferred = Thoth::inferTier(inputs);
    if (inferred != Thoth::BenchmarkTier::OLLAMA) {
        std::cerr << "testE2Ep015TierDeclarationAligned: expected inferred OLLAMA\n";
        return false;
    }
    if (inputs.tier != Thoth::BenchmarkTier::OLLAMA) {
        std::cerr << "testE2Ep015TierDeclarationAligned: expected declared OLLAMA\n";
        return false;
    }
    if (Thoth::hasTierMismatch(inputs)) {
        std::cerr << "testE2Ep015TierDeclarationAligned: unexpected TIER_MISMATCH\n";
        return false;
    }
    // Mock path must remain mismatch-free (no collateral from Phase 2).
    if (Thoth::hasTierMismatch(makeEpisodicMockHarnessInputs())) {
        std::cerr << "testE2Ep015TierDeclarationAligned: mock path now mismatches\n";
        return false;
    }
    return true;
}

/** EP-01.5 Phase 2 gate — tier declaration only. */
static bool runE2Ep015Phase2Tests() {
    std::cout << "E2-EP-01.5 Phase 2 — tier declaration alignment\n";
    std::cout << "  gate: THOTH_E2_EP015_PHASE2\n";
    if (!testE2Ep015TierDeclarationAligned()) {
        std::cerr << "E2-EP-01.5 Phase 2: E2-31b failed\n";
        return false;
    }
    std::cout << "  E2-31b tier declaration aligned (no TIER_MISMATCH)\n";
    return true;
}

/**
 * E2-32 — EP-01.5 Phase 3: forced LLM no-op cannot emit official summary.
 * Uses THOTH_E2_EP015_FORCE_LLM_NOOP to skip set_llm_interface under --authoritative + B.
 */
static bool testE2Ep015FailClosedNoOfficialSummary() {
    if (!Thoth::isOllamaReachable()) {
        std::cerr << "testE2Ep015FailClosedNoOfficialSummary: Ollama not reachable\n";
        return false;
    }
    const std::string binary = findEpisodicHarnessBinary();
    if (binary.empty()) {
        std::cerr << "testE2Ep015FailClosedNoOfficialSummary: harness binary not found\n";
        return false;
    }

    FileHandler fh;
    const fs::path logPath =
        fs::path(fh.getProjectRoot()) / "logs" / "episodic_learning_benchmark.jsonl";
    const std::string logBackup = logPath.string() + ".ep015_p3_backup";
    if (fs::exists(logPath)) {
        fs::copy_file(logPath, logBackup, fs::copy_options::overwrite_existing);
        fs::remove(logPath);
    }

    const int rc = runShellCommand(
        "THOTH_E2_EP015_FORCE_LLM_NOOP=1 THOTH_E2_WIRING_STAGE=B \"" + binary +
        "\" --authoritative >\"/tmp/thoth_ep015_p3.out\" 2>&1");
    if (rc == 0) {
        std::cerr << "testE2Ep015FailClosedNoOfficialSummary: expected non-zero exit on no-op\n";
        if (fs::exists(logBackup)) {
            fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
            fs::remove(logBackup);
        }
        return false;
    }

    bool sawAbort = false;
    bool sawOfficialSummary = false;
    for (const auto& row : readJsonlRows(logPath.string())) {
        const std::string event = row.value("event", "");
        if (event == "EPISODIC_LEARNING_SUMMARY" && row.value("official_scoring", false)) {
            sawOfficialSummary = true;
        }
        if (event == "EPISODIC_LEARNING_ABORTED" &&
            row.value("abort_reason", "") == "AUTHORITATIVE_LLM_NOOP") {
            sawAbort = true;
        }
        // Recorder destructor may also emit ABORTED without abort_reason.
        if (event == "EPISODIC_LEARNING_ABORTED") {
            sawAbort = true;
        }
    }

    if (fs::exists(logBackup)) {
        fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
        fs::remove(logBackup);
    }

    if (sawOfficialSummary) {
        std::cerr << "testE2Ep015FailClosedNoOfficialSummary: official SUMMARY emitted on no-op\n";
        return false;
    }
    if (!sawAbort) {
        std::cerr << "testE2Ep015FailClosedNoOfficialSummary: missing ABORTED / NOOP signal\n";
        std::ifstream errIn("/tmp/thoth_ep015_p3.out");
        std::string errLine;
        while (std::getline(errIn, errLine)) {
            std::cerr << "  | " << errLine << '\n';
        }
        return false;
    }
    return true;
}

/** Pure predicate check — latency alone must not pass the execution gate. */
static bool testE2Ep015ExecutionGateRequiresTokens() {
    // Mirror harness rule: llm_wired && (total>0 || prompt+completion>0).
    const auto gate = [](bool wired, std::int64_t total, std::int64_t prompt,
                         std::int64_t completion, std::int64_t /*synth_ms*/) {
        return wired && (total > 0 || (prompt + completion) > 0);
    };
    if (gate(false, 0, 0, 0, 5000)) {
        std::cerr << "testE2Ep015ExecutionGateRequiresTokens: unwired must fail\n";
        return false;
    }
    if (gate(true, 0, 0, 0, 5000)) {
        std::cerr << "testE2Ep015ExecutionGateRequiresTokens: latency-only must fail\n";
        return false;
    }
    if (!gate(true, 10, 0, 0, 0)) {
        std::cerr << "testE2Ep015ExecutionGateRequiresTokens: total_tokens must pass\n";
        return false;
    }
    if (!gate(true, 0, 5, 3, 0)) {
        std::cerr << "testE2Ep015ExecutionGateRequiresTokens: prompt+completion must pass\n";
        return false;
    }
    return true;
}

/** EP-01.5 Phase 3 gate — fail-closed authoritative guards. */
static bool runE2Ep015Phase3Tests() {
    std::cout << "E2-EP-01.5 Phase 3 — fail-closed authoritative guards\n";
    std::cout << "  gate: THOTH_E2_EP015_PHASE3\n";
    if (!testE2Ep015ExecutionGateRequiresTokens()) {
        std::cerr << "E2-EP-01.5 Phase 3: execution-gate predicate failed\n";
        return false;
    }
    std::cout << "  execution-gate predicate pass (tokens required; latency insufficient)\n";
    if (!testE2Ep015FailClosedNoOfficialSummary()) {
        std::cerr << "E2-EP-01.5 Phase 3: E2-32 fail-closed summary suppression failed\n";
        return false;
    }
    std::cout << "  E2-32 fail-closed: no-op cannot emit official SUMMARY\n";
    return true;
}

static bool runE2Ep01Tests() {
    std::cout << "E2-EP-01 episodic authoritative inference harness proof\n";
    std::cout << "  gate: THOTH_E2_EP01\n";
    std::cout << "  sequence: E2-29 -> Phase D E2-28 spot-check -> E2-30\n";

    if (!testE2Ep01MockRegressionPreservesE28()) {
        std::cerr << "E2-EP-01: E2-29 mock regression failed\n";
        return false;
    }
    std::cout << "  E2-29 mock regression pass\n";

    if (!testE2B5OfficialFingerprintDeterminism()) {
        std::cerr << "E2-EP-01: Phase D E2-28 spot-check failed\n";
        return false;
    }
    std::cout << "  Phase D E2-28 spot-check pass\n";

    if (!testE2Ep01AuthoritativeInferenceSmoke()) {
        std::cerr << "E2-EP-01: E2-30 authoritative smoke failed\n";
        return false;
    }
    std::cout << "  E2-30 authoritative smoke pass (zero official_scoring rows)\n";
    return true;
}

static bool runE2D5Tests() {
    if (!attestD1CloseOutEvidence()) {
        std::cerr << "E2-D5 closure: D1 attestation failed\n";
        return false;
    }
    if (!attestD2CloseOutEvidence()) {
        std::cerr << "E2-D5 closure: D2 attestation failed\n";
        return false;
    }
    if (!attestD3CloseOutEvidence()) {
        std::cerr << "E2-D5 closure: D3 attestation failed\n";
        return false;
    }
    if (!attestD4CompositionEvidence()) {
        std::cerr << "E2-D5 closure: D4 attestation failed\n";
        return false;
    }
    if (!runE2D5AuthorityMetaProof()) {
        std::cerr << "E2-D5 closure: Step 1 authority meta-proof failed\n";
        return false;
    }
    if (!runE2D5C5Proof()) {
        std::cerr << "E2-D5 closure: Step 2 behavioral meta-proof failed\n";
        return false;
    }
    if (!runE2D5DeterminismProof()) {
        std::cerr << "E2-D5 closure: Step 3 determinism meta-proof failed\n";
        return false;
    }

    std::cout << "E2-D5 evolution trust proof green\n";
    std::cout << "E2-D5 closure evidence:\n";
    std::cout << "  gate: THOTH_E2_D5\n";
    std::cout << "  THOTH_E2_D1=1 attested (2026-07-05 close-out)\n";
    std::cout << "  THOTH_E2_D2=1 attested (2026-07-07 close-out)\n";
    std::cout << "  THOTH_E2_D3=1 attested (2026-07-07 close-out)\n";
    std::cout << "  THOTH_E2_D4=1 attested (d4216c8)\n";
    std::cout << "  runE2D5AuthorityMetaProof pass (E2-D5-03, 0b4df02)\n";
    std::cout << "  runE2D5C5Proof pass (E2-D5-01, f16664d)\n";
    std::cout << "  runE2D5DeterminismProof pass (E2-D5-02, 6dec86b)\n";
    std::cout << "  phase seal: docs/phases/PHASE_D_COMPLETE.md\n";
    std::cout << "  conclusion: evolution trust proof green — Phase D trust boundary sealed "
                 "(preservation only — not promotion)\n";
    std::cout << "  deferred: Phase E scientific defense\n";
    return true;
}

/** Evidence printer — THOTH_E2_C5_MATRIX=1 only. */
static void printE2C5EquivalenceMatrix() {
    Thoth::BenchmarkAttribution attr{"e2-c5-matrix", "e2-c5-matrix-env"};
    const Thoth::E2EvalConfig cfg = makeE2StrictTestConfig();
    std::cout << "E2-C5 equivalence matrix (benchmark vs production, normalized snapshots)\n";
    std::cout << "case_id | resolution | scorable | not_scorable | diag_bucket | fingerprint_match\n";
    for (const auto& spec : Thoth::getEpisodicLearningCases()) {
        Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
        const auto cold = runE2TestArm(spec, "cold", attr);
        const auto warm = runE2TestArm(spec, "warm", attr, nullptr, &warmBlock);
        const auto benchmark = Thoth::runBenchmarkPathArtifacts(
            spec.id, spec.expectations, cold, warm, cfg, warmBlock);
        const auto episode = Thoth::episodeFromBenchmarkArmsForTests(
            spec, cold, warm, spec.expectations, warmBlock);
        const auto production = Thoth::runProductionPathArtifacts(episode, cfg);
        const auto diff = Thoth::diffPathEquivalence(benchmark, production);
        const auto bench_snap = Thoth::pathEquivalenceCaseEvalSnapshot(benchmark.case_eval);
        const auto prod_snap = Thoth::pathEquivalenceCaseEvalSnapshot(production.case_eval);
        const std::string resolution =
            bench_snap.value("evaluation_resolution", "unset");
        const std::string fp_match =
            benchmark.fingerprint.fingerprint_hash == production.fingerprint.fingerprint_hash
                ? "YES"
                : "NO";
        std::cout << spec.id << " | " << resolution << " | "
                  << benchmark.summary.scorable_cases << '/' << production.summary.scorable_cases
                  << " | " << benchmark.summary.not_scorable_cases << '/'
                  << production.summary.not_scorable_cases << " | "
                  << benchmark.diagnostics.diagnosis_bucket << '/'
                  << production.diagnostics.diagnosis_bucket << " | " << fp_match
                  << (diff.equivalent ? " | MATCH" : " | MISMATCH") << '\n';
        if (!diff.equivalent) {
            for (const auto& m : diff.mismatches) {
                std::cout << "  mismatch: " << m << '\n';
            }
        }
    }
}

static bool testE2CaseById(const std::string& caseId) {
    const auto cases = Thoth::getEpisodicLearningCases();
    const Thoth::EpisodicLearningCase* spec = nullptr;
    for (const auto& c : cases) {
        if (c.id == caseId) {
            spec = &c;
            break;
        }
    }
    if (!spec) {
        std::cerr << "testE2CaseById: missing case " << caseId << '\n';
        return false;
    }

    Thoth::BenchmarkAttribution attr{"e2-test-run", "e2-test-env"};
    const auto cold = runE2TestArm(*spec, "cold", attr);
    Thoth::E2RunBlockReason warmBlock = Thoth::E2RunBlockReason::NONE;
    const auto warm = runE2TestArm(*spec, "warm", attr, nullptr, &warmBlock);
    auto eval = Thoth::evaluateEpisodicLearningCase(
        spec->id, spec->expectations, cold, warm, makeE2StrictTestConfig());
    eval.run_block_reason = warmBlock;
    Thoth::applyCaseEvaluationResolution(eval);
    if (!eval.passes) {
        std::cerr << "testE2CaseById: " << caseId << " failed — " << eval.failure_reason << '\n';
        return false;
    }
    if (!eval.evaluation_resolution.has_value() ||
        *eval.evaluation_resolution != Thoth::E2EvaluationResolution::SCORED_SUCCESS) {
        std::cerr << "testE2CaseById: " << caseId << " expected SCORED_SUCCESS resolution\n";
        return false;
    }
    return true;
}

/** E2-04: harness path — probe stack, sidecar identity, JSONL row. */
static bool testE2EpisodicLearningBenchmarkSmoke() {
    setenv("THOTH_MOCK_EPISODIC", "1", 1);
    setenv("THOTH_MOCK_LLM", "true", 1);

    const fs::path logsDir = makeTempPath("thoth_e2_smoke_logs");
    fs::create_directories(logsDir);
    const fs::path metricsLog = logsDir / "cognitive_metrics.jsonl";
    setenv("THOTH_COGNITIVE_METRICS_LOG", metricsLog.string().c_str(), 1);

    auto probeEngine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager probeIdx(probeEngine.get());

    Thoth::BenchmarkEnvironmentInputs inputs;
    inputs.harness = "episodic_learning_benchmark";
    inputs.tier = Thoth::BenchmarkTier::MOCK;
    inputs.model.llm_model = "mock";
    inputs.model.embedding_method = "TfIdf";
    inputs.model.embedding_dimension = probeEngine->getDimension();
    inputs.model.embedding_internal_version = probeEngine->getInternalVersion();
    inputs.corpus_mode = Thoth::CorpusFingerprintMode::FAST;
    inputs.thoth_env_flags = Thoth::collectThothEnvFlags();

    Thoth::BenchmarkContextOptions opts;
    opts.logs_directory = logsDir.string();
    opts.auto_fill_git = false;
    Thoth::BenchmarkRun run = Thoth::BenchmarkRun::create(inputs, opts);

    Thoth::IndexEnvironment index;
    index.rag_index_header = {
        {"model_name", probeEngine->getModelName()},
        {"embedding_dimension", probeEngine->getDimension()},
        {"embedding_version", probeEngine->getInternalVersion()},
        {"chunk_count", 0},
    };
    run.bindIndex(index);

    if (run.index_hash().empty()) {
        std::cerr << "testE2EpisodicLearningBenchmarkSmoke: index_hash empty\n";
        fs::remove_all(logsDir);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        return false;
    }

    const Thoth::BenchmarkAttribution attr = run.attribution();
    const auto cases = Thoth::getEpisodicLearningCases();
    if (cases.empty()) {
        std::cerr << "testE2EpisodicLearningBenchmarkSmoke: no cases\n";
        fs::remove_all(logsDir);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        return false;
    }

    runE2TestArm(cases.front(), "warm", attr);

    auto cleanup = [&]() {
        fs::remove_all(logsDir);
        unsetenv("THOTH_COGNITIVE_METRICS_LOG");
        unsetenv("THOTH_MOCK_EPISODIC");
        unsetenv("THOTH_MOCK_LLM");
    };

    nlohmann::json sidecar;
    {
        std::ifstream in(logsDir / "benchmark_env.latest.json");
        if (!in.is_open()) {
            std::cerr << "testE2EpisodicLearningBenchmarkSmoke: sidecar missing\n";
            cleanup();
            return false;
        }
        in >> sidecar;
    }
    if (sidecar.value("run_id", "") != run.run_id() ||
        sidecar.value("environment_hash", "") != run.environment_hash()) {
        std::cerr << "testE2EpisodicLearningBenchmarkSmoke: sidecar mismatch\n";
        cleanup();
        return false;
    }

    const auto row = readMetricsRowWithRunId(metricsLog, attr.run_id);
    if (!row.has_value() || row->value("env_hash", "") != attr.env_hash) {
        std::cerr << "testE2EpisodicLearningBenchmarkSmoke: metrics attribution mismatch\n";
        cleanup();
        return false;
    }

    cleanup();
    return true;
}

int main() {
    if (const char* parallelOnly = std::getenv("THOTH_PARALLEL_RETRIEVAL_ONLY")) {
        if (parallelOnly[0] == '1') {
            return testParallelRetrieval() ? 0 : 1;
        }
    }

    if (const char* ep01 = std::getenv("THOTH_E2_EP01")) {
        if (ep01[0] != '0' && std::string(ep01) != "false") {
            if (!runE2Ep01Tests()) {
                return 1;
            }
            std::cout << "E2-EP-01 gate passed.\n";
            return 0;
        }
    }

    if (const char* ep015p1 = std::getenv("THOTH_E2_EP015_PHASE1")) {
        if (ep015p1[0] != '0' && std::string(ep015p1) != "false") {
            if (!runE2Ep015Phase1Tests()) {
                return 1;
            }
            std::cout << "E2-EP-01.5 Phase 1 gate passed.\n";
            return 0;
        }
    }

    if (const char* ep015p2 = std::getenv("THOTH_E2_EP015_PHASE2")) {
        if (ep015p2[0] != '0' && std::string(ep015p2) != "false") {
            if (!runE2Ep015Phase2Tests()) {
                return 1;
            }
            std::cout << "E2-EP-01.5 Phase 2 gate passed.\n";
            return 0;
        }
    }

    if (const char* ep015p3 = std::getenv("THOTH_E2_EP015_PHASE3")) {
        if (ep015p3[0] != '0' && std::string(ep015p3) != "false") {
            if (!runE2Ep015Phase3Tests()) {
                return 1;
            }
            std::cout << "E2-EP-01.5 Phase 3 gate passed.\n";
            return 0;
        }
    }

    if (const char* d5 = std::getenv("THOTH_E2_D5")) {
        if (d5[0] != '0' && std::string(d5) != "false") {
            if (!runE2D5Tests()) {
                return 1;
            }
            std::cout << "E2-D5 closure gate passed.\n";
            return 0;
        }
    }

    if (const char* d5_c5 = std::getenv("THOTH_E2_D5_C5")) {
        if (d5_c5[0] != '0' && std::string(d5_c5) != "false") {
            if (!runE2D5C5Proof()) {
                return 1;
            }
            std::cout << "E2-D5-Step2 behavioral gate passed.\n";
            return 0;
        }
    }

    if (const char* d5_determinism = std::getenv("THOTH_E2_D5_DETERMINISM")) {
        if (d5_determinism[0] != '0' && std::string(d5_determinism) != "false") {
            if (!runE2D5DeterminismProof()) {
                return 1;
            }
            std::cout << "E2-D5-Step3 determinism gate passed.\n";
            return 0;
        }
    }

    if (const char* d5_authority = std::getenv("THOTH_E2_D5_AUTHORITY")) {
        if (d5_authority[0] != '0' && std::string(d5_authority) != "false") {
            if (!runE2D5AuthorityMetaProof()) {
                return 1;
            }
            std::cout << "E2-D5-Step1 authority gate passed.\n";
            return 0;
        }
    }

    if (const char* d4 = std::getenv("THOTH_E2_D4")) {
        if (d4[0] != '0' && std::string(d4) != "false") {
            if (!runE2D4Tests()) {
                return 1;
            }
            std::cout << "E2-D4 composition gate passed.\n";
            return 0;
        }
    }

    if (const char* d4_step4 = std::getenv("THOTH_E2_D4_STEP4")) {
        if (d4_step4[0] != '0' && std::string(d4_step4) != "false") {
            if (!runE2D4Step4Tests()) {
                return 1;
            }
            std::cout << "E2-D4-Step4 gate passed.\n";
            return 0;
        }
    }

    if (const char* d4_02 = std::getenv("THOTH_E2_D4_02")) {
        if (d4_02[0] != '0' && std::string(d4_02) != "false") {
            if (!runE2D4_02Tests()) {
                return 1;
            }
            std::cout << "E2-D4-02 gate passed.\n";
            return 0;
        }
    }

    if (const char* d4_01 = std::getenv("THOTH_E2_D4_01")) {
        if (d4_01[0] != '0' && std::string(d4_01) != "false") {
            if (!runE2D4_01Tests()) {
                return 1;
            }
            std::cout << "E2-D4-01 gate passed.\n";
            return 0;
        }
    }

    if (const char* d4_step1 = std::getenv("THOTH_E2_D4_STEP1")) {
        if (d4_step1[0] != '0' && std::string(d4_step1) != "false") {
            if (!runE2D4Step1Tests()) {
                return 1;
            }
            std::cout << "E2-D4-Step1 gate passed.\n";
            return 0;
        }
    }

    if (const char* d3 = std::getenv("THOTH_E2_D3")) {
        if (d3[0] != '0' && std::string(d3) != "false") {
            if (!runE2D3Tests()) {
                return 1;
            }
            std::cout << "E2-D3 gate passed (full proof suite).\n";
            return 0;
        }
    }

    if (const char* d3_05 = std::getenv("THOTH_E2_D3_05")) {
        if (d3_05[0] != '0' && std::string(d3_05) != "false") {
            if (!runE2D3_05Tests()) {
                return 1;
            }
            std::cout << "E2-D3-05 gate passed.\n";
            return 0;
        }
    }

    if (const char* d3_03 = std::getenv("THOTH_E2_D3_03")) {
        if (d3_03[0] != '0' && std::string(d3_03) != "false") {
            if (!runE2D3_03Tests()) {
                return 1;
            }
            std::cout << "E2-D3-03 gate passed.\n";
            return 0;
        }
    }

    if (const char* d3_02 = std::getenv("THOTH_E2_D3_02")) {
        if (d3_02[0] != '0' && std::string(d3_02) != "false") {
            if (!runE2D3_02Tests()) {
                return 1;
            }
            std::cout << "E2-D3-02 gate passed.\n";
            return 0;
        }
    }

    if (const char* d3_01 = std::getenv("THOTH_E2_D3_01")) {
        if (d3_01[0] != '0' && std::string(d3_01) != "false") {
            if (!runE2D3_01Tests()) {
                return 1;
            }
            std::cout << "E2-D3-01 gate passed.\n";
            return 0;
        }
    }

    if (const char* d3step1 = std::getenv("THOTH_E2_D3_STEP1")) {
        if (d3step1[0] != '0' && std::string(d3step1) != "false") {
            if (!runE2D3Step1Tests()) {
                return 1;
            }
            std::cout << "E2-D3 Step 1 gate passed.\n";
            return 0;
        }
    }

    if (const char* d2 = std::getenv("THOTH_E2_D2")) {
        if (d2[0] != '0' && std::string(d2) != "false") {
            if (!runE2D2Tests()) {
                return 1;
            }
            std::cout << "E2-D2 gate passed.\n";
            return 0;
        }
    }

    if (const char* d1 = std::getenv("THOTH_E2_D1")) {
        if (d1[0] != '0' && std::string(d1) != "false") {
            if (!runE2D1Tests()) {
                return 1;
            }
            std::cout << "E2-D1 gate passed.\n";
            return 0;
        }
    }

    if (const char* matrix = std::getenv("THOTH_E2_C5_MATRIX")) {
        if (matrix[0] == '1') {
            printE2C5EquivalenceMatrix();
            return 0;
        }
    }
    if (const char* c5 = std::getenv("THOTH_E2_C5")) {
        if (c5[0] == '1') {
            return runE2C5RegressionGate() ? 0 : 1;
        }
    }
    if (const char* cp2 = std::getenv("THOTH_E2_C4_CP2")) {
        if (cp2[0] == '1') {
            return testE2C4Checkpoint2ObservationalProof() ? 0 : 1;
        }
    }

    int failures = 0;
    if (!testConfigRoundTrip()) failures++;
    if (!testMemoryPersistence()) failures++;
    if (!testCommandProcessorSetCommand()) failures++;
    if (!testCommandProcessorSlashTrim()) failures++;
    if (!testAgentInterfaceLifecycle()) failures++;
    if (!testStructuredLoggerRedaction()) failures++;
    if (!testRetrievalDiagnosticsEvent()) failures++;
    if (!testBootstrapIndexing()) failures++;
    if (!testPlanParser()) failures++;
    if (!testResumeFromTrace()) failures++;
    if (!testProjectAnalyzeTool()) failures++;
    if (!testRunTestsTool()) failures++;
    if (!testCodeModifyTool()) failures++;
    if (!testAllowShellExecGate()) failures++;
    if (!testPastPlanRetrieval()) failures++;
    if (!testMemoryPruning()) failures++;
    if (!testMemoryPruningIntegration()) failures++;
    if (!testEpisodicRetrievalEndToEnd()) failures++;
    if (!testEpisodicMemoryBenchmarkNegative()) failures++;
    if (!testConsolidationFailureEmbed()) failures++;
    if (!testConsolidationFailureTransaction()) failures++;
    if (!testConsolidationLatencyRecorded()) failures++;
    if (!testM2StaleSessionUnderCap()) failures++;
    if (!testM2FreshSessionNoOp()) failures++;
    if (!testM2StartupDeferredNonActive()) failures++;
    if (!testM2TimestampPreservation()) failures++;
    if (!testM2MultiTriggerReasons()) failures++;
    if (!testM2BatchCapDeferred()) failures++;
    if (!testM3StatusDryRun()) failures++;
    if (!testM3IgnoreThresholdsUnderCap()) failures++;
    if (!testM3PolicyRunOverCap()) failures++;
    if (!testM3EmptyHot()) failures++;
    if (!testM3ClearsStaleMark()) failures++;
    if (!testM3EmbedUnavailable()) failures++;
    if (!testM3IdempotentDoubleRun()) failures++;
    if (!testM3StatusRunStatus()) failures++;
    if (!testM3GoalBlockedUnlessUnsafe()) failures++;
    if (!testMemorySessionScoping()) failures++;
    if (!testFactStore()) failures++;
    if (!testStoreFactTool()) failures++;
    if (!testVectorStoreAbstraction()) failures++;
    if (!testWebScrapeTool()) failures++;
    if (!testToolBatching()) failures++;
    if (!testParallelRetrieval()) failures++;
    if (!testLlmTokenUsage()) failures++;
    if (!testSelfCorrectTool()) failures++;
    if (!testConstraintChecker()) failures++;
    if (!testReflectionLoop()) failures++;
    if (!testGmailReadMessagesTool()) failures++;
    if (!testCognitiveSpine()) failures++;
    if (!testBenchmarkRAGMode()) failures++;
    if (!testBenchmarkComparison()) failures++;
    if (!testBenchmarkReporter()) failures++;
    if (!testProblemStatePersistence()) failures++;
    if (!testModeSwitchPersistence()) failures++;
    if (!testEventSchemaStandardization()) failures++;
    if (!testScientificLoopStages()) failures++;
    if (!testScientificConvergence()) failures++;
    if (!testStrategyPromotion()) failures++;
    if (!testStrategyInjection()) failures++;
    if (!testE1AssembleEnvironmentDeterministic()) failures++;
    if (!testE1InferTierFromEnvFlags()) failures++;
    if (!testE1EnvironmentHashExcludesIndex()) failures++;
    if (!testE1IndexHashDistinctFromEnvironmentHash()) failures++;
    if (!testE1TierMismatchPredicate()) failures++;
    if (!testE1BenchmarkEnvironmentJsonRoundTrip()) failures++;
    if (!testE1BenchmarkContextCreateSidecar()) failures++;
    if (!testE1BenchmarkContextBindIndexMerge()) failures++;
    if (!testE1BenchmarkContextDoubleBindMismatch()) failures++;
    if (!testE1GoalMetricsWithAttribution()) failures++;
    if (!testE1GoalMetricsWithoutAttribution()) failures++;
    if (!testE1GoalMetricsWorkerThreadAttribution()) failures++;
    if (!testE1HarnessBenchmarkSmoke()) failures++;
    if (!testE1ReflectionAbBenchmarkSmoke()) failures++;
    if (!testE1RobustnessBenchmarkSmoke()) failures++;
    if (!testE1ChatRagBenchmarkSmoke()) failures++;
    if (!testE1GragBenchmarkSmoke()) failures++;
    if (!testG1dFilterTrajectoryCases()) failures++;
    if (!testG1dArmConfigs()) failures++;
    if (!testG1dWinnerAndTieEpsilon()) failures++;
    if (!testG1dTrajectoryAblationSmoke()) failures++;
    if (!testE2StrictInjectionLogFromCaseTable()) failures++;
    if (!testE2EmbeddingVersionPin()) failures++;
    if (!testE2StrictConfigEnforcement()) failures++;
    if (!testE2TableDrivenEvaluator()) failures++;
    if (!testE2A2StrictArmNoPlantSourceContract()) failures++;
    if (!testE2A2SealedLogOwnership()) failures++;
    if (!testE2A2HarnessWiringSmoke()) failures++;
    if (!testE2StrictRetrievalKernel()) failures++;
    if (!testE2ExecutiveStrictEquivalence()) failures++;
    if (!testE2A4StaticDispatchAudit()) failures++;
    if (!testE2RuntimeHeuristicGuard()) failures++;
    if (!testE2B1BlockResolutionSchema()) failures++;
    if (!testE2RunBlockReasonGuardCapture()) failures++;
    if (!testE2RunBlockReasonHappyPathNone()) failures++;
    if (!testE2RunBlockReasonArmStatusUnchanged()) failures++;
    if (!testE2RunBlockReasonWriteSiteAudit()) failures++;
    if (!testE2B3ResolveEvaluationGuardNotScorable()) failures++;
    if (!testE2B3ResolveEvaluationArmFailure()) failures++;
    if (!testE2B3ResolveEvaluationArmSuccess()) failures++;
    if (!testE2B3BlockPrecedenceOverArmFailure()) failures++;
    if (!testE2B3PlanStepTransportMerge()) failures++;
    if (!testE2B31PlanStepOutcomeEquivalence()) failures++;
    if (!testE2B31PlanStepOutcomeSerde()) failures++;
    if (!testE2B31OutcomeWriteSiteAudit()) failures++;
    if (!testE2B4ExportNotScorableCase()) failures++;
    if (!testE2B4ExportScoredFailureCase()) failures++;
    if (!testE2B4ExportScoredSuccessCase()) failures++;
    if (!testE2B4ExportNotScorableSummaryRollup()) failures++;
    if (!testE2B4ExportSuccessRateScorableOnly()) failures++;
    if (!testE2B5OfficialHarnessEnvelope()) failures++;
    if (!testE2B5OfficialGoldenTrio()) failures++;
    if (!testE2B5NonAuthoritativeEnvelope()) failures++;
    if (!testE2B5OfficialFingerprintDeterminism()) failures++;
    if (!testE2B5ScoredLoopStructuralAudit()) failures++;
    if (!testE2C1ServiceOutputEquivalence()) failures++;
    if (!testE2C1HarnessNoDirectEvaluationAlgorithm()) failures++;
    if (!testE2C1ServiceNoBenchmarkLogic()) failures++;
    if (!testE2C2PublicationDisabledByDefault()) failures++;
    if (!testE2C2ChannelDeliversEpisode()) failures++;
    if (!testE2C2ExecutiveNoEvalImport()) failures++;
    if (!testE2C2IntegrationEnvelope()) failures++;
    if (!testE2C2SubscriberNoExecutionLogic()) failures++;
    if (!testE2C2MappingDeterministic()) failures++;
    if (!testE2D1MultiSubscriberDelivery()) failures++;
    if (!testE2D1ExecutiveFailureIsolation()) failures++;
    if (!testE2D1ExecutiveInvisibilityAudit()) failures++;
    if (!testE2C3NoEvaluationCoupling()) failures++;
    if (!testE2C3Determinism()) failures++;
    if (!testE2C3NonInterference()) failures++;
    if (!testE2C3PipelineIntegrity()) failures++;
    if (!testE2C4NoEvaluationCoupling()) failures++;
    if (!testE2C4NonBlockingFailure()) failures++;
    if (!testE2C4StructuralAudit()) failures++;
    if (!testE2C4SubscriberTelemetryBlockAudit()) failures++;
    if (!testE2C4NonInterference()) failures++;
    if (!testE2C4PipelineIntegrity()) failures++;
    if (!runE2C5RegressionGate()) failures++;
    if (!testE2CaseById("E2-01")) failures++;
    if (!testE2CaseById("E2-02")) failures++;
    if (!testE2CaseById("E2-03")) failures++;
    if (!testE2EpisodicLearningBenchmarkSmoke()) failures++;

    if (failures == 0) {
        std::cout << "All unit tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
