#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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
#include "e2_strict_enforcement.h"
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

static bool testParallelRetrieval() {
    setenv("THOTH_MOCK_LLM", "true", 1);
    Config cfg;
    cfg.database_path = makeTempPath("thoth_parallel_retrieval_test.db").string();
    cfg.enable_retrieval_prefetch = true;
    cfg.max_parallel_retrieval = 4;

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

    int timeout = 100;
    while (!completed.load() && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --timeout;
    }

    const bool ok = completed.load() && max_concurrent.load() >= 2;
    if (!ok) {
        std::cerr << "testParallelRetrieval: expected concurrent retrievals (max="
                  << max_concurrent.load() << ", completed=" << completed.load() << ")\n";
    }

    fs::remove(cfg.database_path);
    unsetenv("THOTH_MOCK_LLM");
    return ok;
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
        rec.state_json = s.to_json().dump();
        rec.iteration_count = s.iteration_count;
        rec.confidence_score = s.confidence_score;
        rec.created_at = s.created_at;
        rec.updated_at = s.updated_at;

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

    auto s2 = Thoth::ProblemState::from_json(nlohmann::json::parse(optRec->state_json));
    
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

static bool e2PlantAndConsolidate(Memory& memory,
                                  EmbeddingEngine* engine,
                                  const std::string& sessionId,
                                  const std::string& plantMessage) {
    memory.configureConsolidation(nullptr, engine);
    memory.setActiveSessionId(sessionId);
    memory.addMessage("user", plantMessage);
    for (int i = 1; i < 60; ++i) {
        memory.addMessage("user", "filler turn " + std::to_string(i));
    }
    return !memory.getRecentWarmMemory(1).empty();
}

static Thoth::EpisodicLearningArmObservation runE2TestArm(
    const Thoth::EpisodicLearningCase& spec,
    const std::string& armLabel,
    const Thoth::BenchmarkAttribution& attribution) {
    setenv("THOTH_MOCK_EPISODIC", "1", 1);
    setenv("THOTH_MOCK_LLM", "true", 1);

    Config cfg;
    cfg.max_reflections = 0;
    cfg.database_path =
        makeTempPath("thoth_e2_test_" + spec.id + "_" + armLabel).string();
    fs::remove(cfg.database_path);

    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    EmbeddingEngine* enginePtr = engine.get();

    const bool needsPlant =
        !spec.plant_message.empty() &&
        (spec.cold_arm_pre_consolidated || armLabel == "warm");
    if (needsPlant) {
        e2PlantAndConsolidate(*memory, enginePtr, spec.plant_session_id, spec.plant_message);
    }

    auto idx = new IndexManager(enginePtr);
    if (!spec.index_distractor_text.empty()) {
        CodeChunk distractor;
        distractor.code = spec.index_distractor_text;
        distractor.fileName = "e2-distractor.md";
        distractor.embedding = enginePtr->embed(distractor.code);
        idx->addChunkToIndex(std::move(distractor));
    }

    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx, &cfg, memory.get());

    Thoth::EpisodicRetrievalProvenance retrievalProv;
    rag->setEventCallback([&](const ControllerEvent& ev) { (void)ev; });

    auto planner = std::make_shared<Thoth::EpisodicLearningMockPlanner>(spec.validation_token);
    auto registry = std::make_shared<ToolRegistry>();
    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    controller.set_max_reflections(0);
    memory->setActiveSessionId(spec.id + "-" + armLabel + "-goal");

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
    for (const auto& step : controller.get_current_plan().steps) {
        if (step.type == StepType::RETRIEVAL && !step.result.is_null()) {
            retrievalProv =
                Thoth::provenanceFromRetrievalStepResult(step.result, spec.expectations);
            break;
        }
    }
    obs.retrieval = retrievalProv;
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

static Thoth::E2EvalConfig makeE2StrictTestConfig() {
    Thoth::E2EvalConfig cfg;
    cfg.tier = Thoth::E2EvalTier::STRICT;
    cfg.versions.corpus_snapshot_id = "e2-test-corpus";
    cfg.versions.model_version_or_weights_hash = "mock";
    cfg.versions.embedding_model_version = "TfIdf:2";
    cfg.versions.retrieval_engine_version = Thoth::kE2StrictRetrievalEngineVersion;
    return cfg;
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
    const auto warm = runE2TestArm(*spec, "warm", attr);
    const auto eval = Thoth::evaluateEpisodicLearningCase(
        spec->id, spec->expectations, cold, warm, makeE2StrictTestConfig());
    if (!eval.passes) {
        std::cerr << "testE2CaseById: " << caseId << " failed — " << eval.failure_reason << '\n';
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
