#include "generation_budget.h"
#include "generation_call.h"
#include "generation_call_log.h"
#include "generation_progress_observer.h"
#include "llama_server_client.h"
#include "llm_timeout_policy.h"
#include "llm_interface.h"
#include "llm_planner.h"
#include "mock_inference_client.h"
#include "robustness_mock_responses.h"
#include "plan.h"
#include "rag.h"
#include "embedding_engine.h"
#include "index_manager.h"
#include "memory.h"
#include "alp_sha256.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>

namespace {

class ScopeRestoreEnv {
public:
    ScopeRestoreEnv(const char* name, const char* value) : name_(name) {
        const char* existing = std::getenv(name);
        if (existing) {
            had_ = true;
            previous_ = existing;
        }
        setenv(name, value, 1);
    }
    ~ScopeRestoreEnv() {
        if (had_) {
            setenv(name_, previous_.c_str(), 1);
        } else {
            unsetenv(name_);
        }
    }
private:
    const char* name_;
    bool had_ = false;
    std::string previous_;
};

bool expectParse(const std::string& text, bool ok) {
    int out = 0;
    const auto error = Thoth::GenerationBudget::parseCanonicalInteger(text, out);
    if (ok == !error.has_value()) {
        return true;
    }
    std::cerr << "mtcp parse " << text << " unexpected\n";
    return false;
}

} // namespace

static bool testMtcpGenerationBudgetAndSeed() {
    if (!expectParse("", false) || !expectParse("0", false) || !expectParse("-1", false)
        || !expectParse("+512", false) || !expectParse("0512", false) || !expectParse("512.0", false)
        || !expectParse("2147483648", false) || !expectParse("512", true)
        || !expectParse("1024", true) || !expectParse("2048", true) || !expectParse("17001", true)) {
        return false;
    }

    Thoth::GenerationBudget::resetForTests();
    if (Thoth::GenerationBudget::hasCeiling() || Thoth::GenerationBudget::resolvedOr(512) != 512
        || Thoth::GenerationBudget::hasSeed()) {
        std::cerr << "mtcp unset ceiling did not preserve 512 fallback\n";
        return false;
    }

    Thoth::GenerationBudget::setForTests(1024, 17001);
    if (Thoth::GenerationBudget::resolvedOr(512) != 1024 || Thoth::GenerationBudget::seed() != 17001) {
        std::cerr << "mtcp set ceiling/seed not applied\n";
        return false;
    }

    Thoth::InferenceGenerateRequest plain;
    plain.model = "chat";
    plain.prompt = "hi";
    plain.max_tokens = 512;
    const std::string without = Thoth::LlamaServerClient::serializeGeneratePayload(plain);
    if (without.find("\"seed\"") != std::string::npos) {
        std::cerr << "mtcp unset seed was serialized\n";
        return false;
    }

    plain.seed = 17001;
    plain.max_tokens = 1024;
    const auto payload = nlohmann::json::parse(Thoth::LlamaServerClient::serializeGeneratePayload(plain));
    if (payload["seed"] != 17001 || payload["max_tokens"] != 1024) {
        std::cerr << "mtcp completions seed/ceiling payload wrong\n";
        return false;
    }

    Thoth::InferenceChatRequest chat;
    chat.model = "chat";
    chat.messages.push_back({"user", "hi"});
    chat.max_tokens = 2048;
    chat.seed = 17001;
    const auto chat_payload = nlohmann::json::parse(Thoth::LlamaServerClient::serializeChatPayload(chat));
    if (chat_payload["seed"] != 17001 || chat_payload["max_tokens"] != 2048) {
        std::cerr << "mtcp chat seed/ceiling payload wrong\n";
        return false;
    }

    Thoth::InferenceEmbedRequest embed;
    embed.model = "nomic";
    embed.inputs = {"a"};
    const std::string embed_payload = Thoth::LlamaServerClient::serializeEmbedPayload(embed);
    if (embed_payload.find("seed") != std::string::npos) {
        std::cerr << "mtcp embed payload included seed\n";
        return false;
    }

    Thoth::GenerationBudget::resetForTests();
    return true;
}

struct UnsetDev {
    std::string saved;
    UnsetDev() {
        const char* dev = std::getenv("THOTH_TEST_SUITE_DEV");
        if (dev) {
            saved = dev;
        }
        unsetenv("THOTH_TEST_SUITE_DEV");
    }
    ~UnsetDev() {
        if (!saved.empty()) {
            setenv("THOTH_TEST_SUITE_DEV", saved.c_str(), 1);
        }
    }
};

static bool testMtcpCallScopedMetadata() {
    Thoth::RobustnessMockResponses::reset();
    UnsetDev unset_dev;
    const std::string log_path = "/tmp/thoth-mtcp-generation-calls.jsonl";
    std::filesystem::remove(log_path);
    ScopeRestoreEnv log_env("THOTH_GENERATION_CALLS_LOG", log_path.c_str());
    Thoth::GenerationBudget::resetForTests();

    auto make_llm = []() {
        auto llm = std::make_unique<LLMInterface>(LLMBackend::Ollama, nullptr);
        auto client = std::make_unique<Thoth::MockInferenceClient>();
        client->on_generate = [](const Thoth::InferenceGenerateRequest& req) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            Thoth::InferenceGenerateResult result;
            result.ok = true;
            result.text = "body-" + std::to_string(req.max_tokens);
            result.finish_reason = req.max_tokens >= 1024 ? "length" : "stop";
            result.token_usage.prompt_tokens = 10;
            result.token_usage.completion_tokens = req.max_tokens >= 1024 ? req.max_tokens : 4;
            result.token_usage.total_tokens = result.token_usage.prompt_tokens + result.token_usage.completion_tokens;
            result.elapsed_ms = 0;
            return result;
        };
        llm->setInferenceClientForTests(std::move(client));
        llm->useModel("chat");
        return llm;
    };

    auto llm_a = make_llm();
    auto llm_b = make_llm();
    Thoth::GenerationOutcome first;
    Thoth::GenerationOutcome second;
    std::thread thread_a([&]() {
        Thoth::GenerationCallContext ctx;
        ctx.task_id = "C1";
        ctx.call_type = "plan";
        first = llm_a->generateCall("prompt-a", 512, {}, ctx);
    });
    std::thread thread_b([&]() {
        Thoth::GenerationCallContext ctx;
        ctx.task_id = "C2";
        ctx.call_type = "synthesis";
        second = llm_b->generateCall("prompt-b", 1024, {}, ctx);
    });
    thread_a.join();
    thread_b.join();

    if (first.context.task_id != "C1" || second.context.task_id != "C2") {
        std::cerr << "mtcp concurrent task ids crossed\n";
        return false;
    }
    if (first.finish_reason != "stop" || second.finish_reason != "length") {
        std::cerr << "mtcp finish reason lost first=" << first.finish_reason
                  << " second=" << second.finish_reason
                  << " ok=" << first.ok << "/" << second.ok
                  << " text=" << first.text << "/" << second.text
                  << " err=" << first.error << "\n";
        return false;
    }
    if (first.prompt_tokens != 10 || first.completion_tokens != 4 || second.completion_tokens != 1024) {
        std::cerr << "mtcp token counts lost\n";
        return false;
    }
    if (first.elapsed_ms < 0 || second.elapsed_ms < 0) {
        std::cerr << "mtcp elapsed time negative\n";
        return false;
    }
    if (first.text.find("prompt-a") != std::string::npos) {
        std::cerr << "mtcp outcome leaked prompt into text identity check unexpectedly skipped\n";
    }

    Thoth::GenerationRecordFields fields;
    fields.associated_generation_id = first.generation_id;
    fields.has_validation_ok = true;
    fields.validation_ok = false;
    fields.fallback_used = true;
    if (!Thoth::GenerationCallLog::append(first, fields)) {
        std::cerr << "mtcp generation log write failed\n";
        return false;
    }
    Thoth::GenerationOutcome chat_like = first;
    chat_like.context.call_type = "chat";
    if (Thoth::GenerationCallLog::append(chat_like, fields)) {
        std::cerr << "mtcp chat record was written to generation_calls.jsonl\n";
        return false;
    }

    const std::string logged = [&]() {
        std::ifstream in(log_path);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }();
    if (logged.find("prompt-a") != std::string::npos || logged.find("\"text\"") != std::string::npos
        || logged.find("body-") != std::string::npos) {
        std::cerr << "mtcp log contained prompt or completion text\n";
        return false;
    }
    if (logged.find("\"fallback_used\":true") == std::string::npos
        || logged.find(first.generation_id) == std::string::npos
        || logged.find("\"task_id\":\"C1\"") == std::string::npos) {
        std::cerr << "mtcp fallback association missing\n";
        return false;
    }

    std::filesystem::remove(log_path);
    Thoth::GenerationBudget::resetForTests();
    return true;
}

static bool testMtcpRevisionWrapperStable() {
    Thoth::RobustnessMockResponses::reset();
    const std::string log_path = "/tmp/thoth-mtcp-revision.jsonl";
    std::filesystem::remove(log_path);
    ScopeRestoreEnv log_env("THOTH_GENERATION_CALLS_LOG", log_path.c_str());
    const fs::path tempDir = makeTempPath("thoth_mtcp_revise");
    fs::create_directories(tempDir);
    Config cfg;
    cfg.database_path = (tempDir / "memory.db").string();
    Memory memory(cfg);
    auto embedding = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    auto* index = new IndexManager(embedding.get());
    RAGPipeline rag(std::move(embedding), index, &cfg, &memory);
    auto prompts = std::make_shared<PromptFactory>(memory, rag);
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    auto client = std::make_unique<Thoth::MockInferenceClient>();
    client->on_generate = [](const Thoth::InferenceGenerateRequest&) {
        Thoth::InferenceGenerateResult result;
        result.ok = true;
        result.text = "not-a-plan";
        result.finish_reason = "stop";
        result.token_usage.prompt_tokens = 3;
        result.token_usage.completion_tokens = 2;
        result.token_usage.total_tokens = 5;
        return result;
    };
    llm.setInferenceClientForTests(std::move(client));
    llm.useModel("chat");
    auto memory_ptr = std::shared_ptr<Memory>(&memory, [](Memory*) {});
    auto rag_ptr = std::shared_ptr<RAGPipeline>(&rag, [](RAGPipeline*) {});
    LLMPlanner planner(memory_ptr, rag_ptr, prompts, &llm);

    const auto plan_json = nlohmann::json::parse(R"({
        "plan_id":"mtcp-r1-plan",
        "goal":"Report the failing process and the last log line for Northline Badge Relay incident NL-BR-4417 using the characterization notes.",
        "current_index":0,
        "status":0,
        "created_at_ms":1710000000000,
        "updated_at_ms":1710000000000,
        "steps":[{
            "step_id":"mtcp-r1-retrieve",
            "description":"Retrieve notes for query MTCP-DOC-404",
            "type":1,
            "tool":{},
            "payload":{"query":"MTCP-DOC-404","top_k":5},
            "status":3,
            "retry_count":0,
            "failure_policy":{"max_retries":1,"abort_on_failure":false,"revise_plan_on_failure":true},
            "result":{"status":"error"},
            "outcome":{"run_block_reason":"NONE"},
            "reasoning":"",
            "started_at_ms":1710000001000,
            "completed_at_ms":1710000001000,
            "depends_on":[]
        }]
    })");
    Plan plan = Plan::from_json(plan_json);
    const auto failed = nlohmann::json::parse(
        R"({"data":{"query":"MTCP-DOC-404","retrieved_chunk_count":0},"error_message":"Retrieval returned no documents for query MTCP-DOC-404.","status":"error"})");

    Thoth::GenerationBudget::setForTests(512, std::nullopt);
    const std::string hash_512 = planner.revisionWrapperSha256(plan, failed);
    Thoth::GenerationBudget::setForTests(2048, std::nullopt);
    const std::string hash_2048 = planner.revisionWrapperSha256(plan, failed);
    const Plan revised = planner.revise_plan(plan, failed);
    (void)revised;
    Thoth::GenerationBudget::resetForTests();
    fs::remove_all(tempDir);
    std::filesystem::remove(log_path);

    if (plan.created_at_ms != 1710000000000 || plan.updated_at_ms != 1710000000000) {
        std::cerr << "mtcp revise_plan changed frozen input timestamps\n";
        return false;
    }
    if (hash_512.empty() || hash_512 != hash_2048 || hash_512 != Thoth::sha256Hex(
            prompts->buildRevisionPrompt(plan.goal, plan.to_json().dump(), failed.dump(), nullptr))) {
        std::cerr << "mtcp wrapper hash not stable across ceilings\n";
        return false;
    }
    return true;
}

class MtcpSynthesisProvenancePlanner : public IPlanner {
public:
    Plan create_plan(const std::string& goal) override {
        Plan plan;
        plan.plan_id = "mtcp-synth-plan";
        plan.goal = goal;
        plan.status = PlanStatus::ACTIVE;
        PlanStep step;
        step.step_id = "mtcp-synth-step";
        step.description = "Synthesize the characterization answer";
        step.type = StepType::LLM;
        step.payload = {{"prompt", "MTCP_SYNTH_PROMPT_MARKER"}};
        plan.steps.push_back(step);
        return plan;
    }
    Plan revise_plan(const Plan& plan, const nlohmann::json&) override { return plan; }
};

static bool testMtcpSynthesisTaskIdPropagation() {
    Thoth::RobustnessMockResponses::reset();
    UnsetDev unset_dev;
    const char* saved_mock = std::getenv("THOTH_MOCK_LLM");
    const std::string saved_mock_value = saved_mock ? saved_mock : "";
    unsetenv("THOTH_MOCK_LLM");
    Thoth::GenerationBudget::resetForTests();

    const fs::path db_path = makeTempPath("thoth_mtcp_synthesis_task.db");
    const std::string log_path = "/tmp/thoth-mtcp-synthesis-task.jsonl";
    std::filesystem::remove(log_path);
    ScopeRestoreEnv log_env("THOTH_GENERATION_CALLS_LOG", log_path.c_str());

    Config cfg;
    cfg.database_path = db_path.string();
    auto memory = std::make_shared<Memory>(cfg);
    auto embedding = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto index = new IndexManager(embedding.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(embedding), index, &cfg, memory.get());
    auto planner = std::make_shared<MtcpSynthesisProvenancePlanner>();
    auto registry = std::make_shared<ToolRegistry>();

    auto llm = std::make_unique<LLMInterface>(LLMBackend::Ollama, &cfg);
    auto client = std::make_unique<Thoth::MockInferenceClient>();
    client->on_generate = [](const Thoth::InferenceGenerateRequest&) {
        Thoth::InferenceGenerateResult result;
        result.ok = true;
        result.text = "MTCP_SYNTH_COMPLETION_MARKER";
        result.finish_reason = "stop";
        result.token_usage.prompt_tokens = 11;
        result.token_usage.completion_tokens = 4;
        result.token_usage.total_tokens = 15;
        return result;
    };
    llm->setInferenceClientForTests(std::move(client));
    llm->useModel("chat");

    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    controller.set_config(&cfg);
    controller.set_llm_interface(llm.get());
    controller.set_max_reflections(0);
    controller.set_session_id("mtcp-synth-session");
    controller.execute_goal("mtcp synthesis provenance goal", {}, "G2");
    const bool finished = waitForPlanTerminal(controller, 15000);

    bool ok = finished;
    std::string logged;
    if (ok) {
        std::ifstream in(log_path);
        std::stringstream buffer;
        buffer << in.rdbuf();
        logged = buffer.str();
        if (logged.find("MTCP_SYNTH_PROMPT_MARKER") != std::string::npos
            || logged.find("MTCP_SYNTH_COMPLETION_MARKER") != std::string::npos) {
            std::cerr << "mtcp synthesis telemetry logged prompt or completion text\n";
            ok = false;
        }
    } else {
        std::cerr << "mtcp synthesis provenance goal did not finish\n";
    }

    bool saw_synthesis = false;
    if (ok) {
        std::stringstream lines(logged);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.empty()) {
                continue;
            }
            const auto row = nlohmann::json::parse(line);
            if (row.value("call_type", "") != "synthesis") {
                continue;
            }
            saw_synthesis = true;
            if (row.value("task_id", "") != "G2"
                || row.value("session_id", "") != "mtcp-synth-session"
                || row.value("plan_id", "") != "mtcp-synth-plan"
                || row.contains("prompt") || row.contains("completion") || row.contains("text")
                || row.contains("raw_completion") || row.contains("chain_of_thought")) {
                std::cerr << "mtcp synthesis provenance fields missing or text fields present\n";
                ok = false;
            }
        }
        if (!saw_synthesis) {
            std::cerr << "mtcp synthesis generation record absent\n";
            ok = false;
        }
    }

    std::filesystem::remove(log_path);
    fs::remove(db_path);
    Thoth::GenerationBudget::resetForTests();
    if (!saved_mock_value.empty()) {
        setenv("THOTH_MOCK_LLM", saved_mock_value.c_str(), 1);
    }
    return ok;
}

static bool testMtcpV11UsageAndObserver() {
    const auto measured = Thoth::LlamaServerClient::parseCompletionResponse(
        R"({"choices":[{"text":"","finish_reason":"stop"}],"usage":{"prompt_tokens":4,"completion_tokens":0,"total_tokens":4}})");
    if (!measured.ok || !measured.provider_usage_reported || measured.token_usage.completion_tokens != 0
        || measured.token_usage.prompt_tokens != 4) {
        std::cerr << "mtcp measured zero was not preserved\n";
        return false;
    }
    const auto missing = Thoth::LlamaServerClient::parseCompletionResponse(
        R"({"choices":[{"text":"hello","finish_reason":"stop"}]})");
    if (!missing.ok || missing.provider_usage_reported) {
        std::cerr << "mtcp missing usage object was treated as reported\n";
        return false;
    }

    {
        ScopeRestoreEnv timeout_env("THOTH_LLM_TIMEOUT_SECONDS", "4356");
        if (Thoth::LlmTimeoutPolicy::timeoutSeconds() != 4356) {
            std::cerr << "mtcp 4356 timeout was not accepted\n";
            return false;
        }
    }
    if (std::getenv("THOTH_LLM_TIMEOUT_SECONDS") == nullptr
        && Thoth::LlmTimeoutPolicy::timeoutSeconds() != 900) {
        std::cerr << "mtcp default timeout is no longer 900\n";
        return false;
    }

    const std::string call_log = "/tmp/thoth-mtcp-v11-usage.jsonl";
    const std::string progress_log = "/tmp/thoth-mtcp-v11-progress.jsonl";
    std::filesystem::remove(call_log);
    std::filesystem::remove(progress_log);
    ScopeRestoreEnv calls_env("THOTH_GENERATION_CALLS_LOG", call_log.c_str());
    ScopeRestoreEnv progress_env("THOTH_GENERATION_PROGRESS_LOG", progress_log.c_str());

    Thoth::GenerationRecordFields fields;
    Thoth::GenerationOutcome timed_out;
    timed_out.generation_id = "timeout-1";
    timed_out.ok = false;
    timed_out.error = "Timeout was reached";
    timed_out.context.call_type = "plan_retry";
    timed_out.context.task_id = "G2";
    timed_out.requested_max_tokens = 1024;
    timed_out.prompt_tokens = 50;
    timed_out.completion_tokens = 50;
    timed_out.provider_usage_reported = false;
    if (!Thoth::GenerationCallLog::append(timed_out, fields)) {
        std::cerr << "mtcp unavailable usage log failed\n";
        return false;
    }
    Thoth::GenerationOutcome measured_zero;
    measured_zero.generation_id = "zero-1";
    measured_zero.ok = true;
    measured_zero.provider_usage_reported = true;
    measured_zero.prompt_tokens = 4;
    measured_zero.completion_tokens = 0;
    measured_zero.has_total_tokens = true;
    measured_zero.total_tokens = 4;
    measured_zero.finish_reason = "length";
    measured_zero.context.call_type = "synthesis";
    measured_zero.context.task_id = "G2";
    measured_zero.requested_max_tokens = 512;
    if (!Thoth::GenerationCallLog::append(measured_zero, fields)) {
        std::cerr << "mtcp measured zero log failed\n";
        return false;
    }
    const std::string logged = std::string(std::istreambuf_iterator<char>(std::ifstream(call_log).rdbuf()),
                                           std::istreambuf_iterator<char>());
    const auto timeout_line = logged.substr(0, logged.find('\n'));
    if (timeout_line.find("\"provider_usage\":\"unavailable\"") == std::string::npos
        || timeout_line.find("prompt_tokens") != std::string::npos
        || timeout_line.find("completion_tokens") != std::string::npos
        || timeout_line.find("synthesis_observed_empty") != std::string::npos) {
        std::cerr << "mtcp timeout row stored numeric usage\n";
        return false;
    }
    if (logged.find("\"provider_usage\":\"reported\"") == std::string::npos
        || logged.find("\"completion_tokens\":0") == std::string::npos) {
        std::cerr << "mtcp measured zero was not recorded\n";
        return false;
    }

    Thoth::InferenceGenerateRequest request;
    request.model = "chat";
    request.prompt = "unchanged";
    request.max_tokens = 512;
    const std::string payload = Thoth::LlamaServerClient::serializeGeneratePayload(request);
    if (payload.find("fail_on_no_slot") != std::string::npos || payload.find("/slots") != std::string::npos) {
        std::cerr << "mtcp completion payload was changed\n";
        return false;
    }

    std::atomic<int> fetches{0};
    std::atomic<bool> release{false};
    std::mutex block_mutex;
    std::condition_variable block_cv;
    Thoth::setProgressObserverTestHooks({
        [](std::chrono::milliseconds delay) {
            if (delay != std::chrono::seconds(60)) {
                std::cerr << "mtcp observer interval was not 60 seconds\n";
            }
        },
        [&]() {
            const int which = ++fetches;
            Thoth::InferenceHttpResponse http;
            http.ok = true;
            if (which == 1) {
                std::unique_lock<std::mutex> lock(block_mutex);
                block_cv.wait(lock, [&] { return release.load(); });
                http.body = R"([{"is_processing":true,"prompt":"secret text","next_token":{"n_decoded":1}}])";
            } else {
                http.body = R"([{"is_processing":true,"n_prompt_tokens":8,"next_token":{"n_decoded":3,"n_remain":4}}])";
            }
            return http;
        },
    });
    {
        Thoth::GenerationProgressSession first("gen-a", "http://llama-server:8080/slots");
        for (int i = 0; i < 200 && fetches.load() < 1; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        {
            Thoth::GenerationProgressSession second("gen-b", "http://llama-server:8080/slots?fail_on_no_slot=1");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (fetches.load() != 1) {
                std::cerr << "mtcp observer allowed more than one request in flight\n";
                release = true;
                block_cv.notify_all();
                Thoth::clearProgressObserverTestHooks();
                return false;
            }
        }
        release = true;
        block_cv.notify_all();
    }
    {
        Thoth::GenerationProgressSession clean("gen-c", "http://llama-server:8080/slots");
        for (int i = 0; i < 200 && fetches.load() < 2; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    Thoth::clearProgressObserverTestHooks();
    const std::string progress = std::string(std::istreambuf_iterator<char>(std::ifstream(progress_log).rdbuf()),
                                             std::istreambuf_iterator<char>());
    if (progress.find("secret text") != std::string::npos
        || progress.find("\"prompt\"") != std::string::npos
        || progress.find("discarded_text") == std::string::npos
        || progress.find("in_flight") == std::string::npos
        || progress.find("\"n_decoded\":3") == std::string::npos) {
        std::cerr << "mtcp progress log did not keep the observer contract\n";
        return false;
    }
    std::filesystem::remove(call_log);
    std::filesystem::remove(progress_log);
    return true;
}
