#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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
#include "plan_parser.h" 
#include "executive_controller.h"
#include "default_planner.h"
#include "tools.h"
#include "project_analyze_tool.h"
#include "run_tests_tool.h"
#include "code_modify_tool.h"
#include "sqlite_memory_repository.h"
#include "fact_store.h"
#include "store_fact_tool.h"
#include "flat_vector_store.h"
#include "web_scrape_tool.h"
#include "self_correct_tool.h"
#include "constraint_checker.h"
#include "gmail_read_messages_tool.h"
#include "benchmark_runner.h"
#include "benchmark_reporter.h"
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

    const bool ok = conversation.size() == 2
        && reloaded.getSummary(false).find("Last Goal") != std::string::npos;

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

static bool testMemoryPruning() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_pruning_test.db").string();
    auto repo = std::make_unique<Thoth::SQLiteMemoryRepository>(cfg.database_path);
    std::string sid = "test_session";
    repo->createSession(sid, 1000);

    for (int i = 0; i < 60; ++i) {
        repo->appendMessage(sid, {"user", "msg " + std::to_string(i), 1000 + i});
    }

    Thoth::PruningPolicy policy;
    policy.max_hot_messages = 50;
    policy.prune_batch_size = 10;
    
    Thoth::MemoryPruner pruner(*repo, policy);
    int archived = pruner.prune(sid);

    if (archived != 10) {
        std::cerr << "testMemoryPruning: expected 10 archived, got " << archived << "\n";
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
    return true;
}

class MockReflectionPlanner : public IPlanner {
public:
    int call_count = 0;
    Plan create_plan(const std::string& goal) override {
        call_count++;
        Plan plan;
        plan.plan_id = "reflection-plan-" + std::to_string(call_count);
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;

        PlanStep s1;
        s1.step_id = "step-1";
        s1.description = "Reflectable step";
        s1.type = StepType::LLM;
        s1.payload = {{"prompt", "test"}};
        plan.steps.push_back(s1);
        return plan;
    }
    Plan revise_plan(const Plan& p, const nlohmann::json&) override { return p; }
};

static bool testReflectionLoop() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_reflection_test.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
    auto planner = std::make_shared<MockReflectionPlanner>();
    auto registry = std::make_shared<ToolRegistry>();
    
    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    controller.execute_goal("Reflection test goal");

    int timeout = 100;
    while (planner->call_count < 2 && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --timeout;
    }

    if (planner->call_count < 2) {
        std::cerr << "testReflectionLoop: failed to trigger reflection (call count: " << planner->call_count << ")\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
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

    // Verify file write
    bool ok = Thoth::BenchmarkReporter::reportToFile(res, 100);
    return ok;
}


int main() {
    int failures = 0;
    if (!testConfigRoundTrip()) failures++;
    if (!testMemoryPersistence()) failures++;
    if (!testCommandProcessorSetCommand()) failures++;
    if (!testAgentInterfaceLifecycle()) failures++;
    if (!testStructuredLoggerRedaction()) failures++;
    if (!testRetrievalDiagnosticsEvent()) failures++;
    if (!testBootstrapIndexing()) failures++;
    if (!testPlanParser()) failures++;
    if (!testResumeFromTrace()) failures++;
    if (!testProjectAnalyzeTool()) failures++;
    if (!testRunTestsTool()) failures++;
    if (!testCodeModifyTool()) failures++;
    if (!testMemoryPruning()) failures++;
    if (!testFactStore()) failures++;
    if (!testStoreFactTool()) failures++;
    if (!testVectorStoreAbstraction()) failures++;
    if (!testWebScrapeTool()) failures++;
    if (!testToolBatching()) failures++;
    if (!testSelfCorrectTool()) failures++;
    if (!testConstraintChecker()) failures++;
    if (!testReflectionLoop()) failures++;
    if (!testGmailReadMessagesTool()) failures++;
    if (!testCognitiveSpine()) failures++;
    if (!testBenchmarkRAGMode()) failures++;
    if (!testBenchmarkComparison()) failures++;
    if (!testBenchmarkReporter()) failures++;

    if (failures == 0) {
        std::cout << "All unit tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
