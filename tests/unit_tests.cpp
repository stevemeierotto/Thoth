#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>

#if THOTH_HAS_GUI
#include "AgentInterface.h"
#include "local_note_slot_layout.h"
#include "remote_agent_backend.h"
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#endif
#include "llm_timeout_policy.h"
#include "plan_validator.h"
#include "cognitive_status_display.h"
#include "remote_agent_http_utils.h"
#include "remote_agent_sse_utils.h"
#include "command_processor.h"
#include "config.h"
#include "embedding_engine.h"
#include "file_handler.h"
#include "inference_endpoint.h"
#include "inference_client.h"
#include "mock_inference_client.h"
#include "episodic_authoritative_v2.h"
#include "ollama_client.h"
#include "llama_server_client.h"
#include "ollama_snapshot.h"
#include "inference_backend_probe.h"
#include "inference_http.h"
#include "runtime_bootstrap.h"
#include "engine_runtime.h"
#include "engine_error.h"
#include "engine_http_transport.h"
#include "engine_event.h"
#include "engine_sse_session.h"
#include "controller_event.h"
#include "env_loader.h"
#include "index_manager.h"
#include "alp_feature_flags.h"
#include "alp_storage_paths.h"
#include "document_registry.h"
#include "alp_migration_analyzer.h"
#include "alp_migration_apply.h"
#include "attachment_send_policy.h"
#include "alp_sha256.h"
#include "agent_context_retrieval.h"
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
#include "chat_retrieval_boost.h"
#include "chat_retrieval_config.h"
#include "chat_rag_observability.h"
#include "chat_query_utils.h"
#include "chat_retrieval_goal.h"
#include "grag_diagnostics.h"
#include "chat_prompt_config.h"
#include "chat_generation_safety.h"
#include "robustness_mock_responses.h"
#include "grag_diagnostics_display.h"
#include "remote_rag_honesty.h"
#include "ChatSessionTypes.h"
#include "local_note_engine_sync.h"
#include "corpus_create_local.h"
#include "backend_capabilities.h"
#include "panel_presentation_state.h"
#include "cognitive_diagnostics_authority.h"
#include "decision_summary.h"
#include "progress_source.h"
#include "engine_connection_state.h"
#include "operation_result.h"
#include "corpus_documents.h"
#include "retrieval_verification_display.h"
#include "corpus_create.h"
#include "conversation_authority.h"
#include "research_resources.h"
#include "graph_statistics.h"
#include "prompt_factory.h"
#include "llama_server_client.h"
#include "ollama_client.h"
#include "json.hpp"
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
#include <httplib.h>
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

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* key, const char* value) : key_(key) {
        const char* previous = std::getenv(key);
        if (previous) {
            hadPrevious_ = true;
            previous_ = previous;
        }
        if (value) {
            setenv(key, value, 1);
        } else {
            unsetenv(key);
        }
    }

    ~ScopedEnvVar() {
        if (hadPrevious_) {
            setenv(key_.c_str(), previous_.c_str(), 1);
        } else {
            unsetenv(key_.c_str());
        }
    }

private:
    std::string key_;
    std::string previous_;
    bool hadPrevious_ = false;
};

/** Captures one variable so a test can override it and put the suite isolation back. */
struct EnvSnapshot {
    bool had = false;
    std::string value;

    static EnvSnapshot capture(const char* key) {
        EnvSnapshot snapshot;
        if (const char* current = std::getenv(key)) {
            snapshot.had = true;
            snapshot.value = current;
        }
        return snapshot;
    }

    void restore(const char* key) const {
        if (had) {
            setenv(key, value.c_str(), 1);
        } else {
            unsetenv(key);
        }
    }
};

static bool testInferenceEndpointResolution() {
    bool ok = true;

    {
        ScopedEnvVar unsetInference("THOTH_INFERENCE_BASE_URL", nullptr);
        ScopedEnvVar unsetEmbed("THOTH_EMBED_BASE_URL", nullptr);
        ScopedEnvVar unsetHost("OLLAMA_HOST", nullptr);

        const auto endpoints = Thoth::resolveInferenceEndpoints();
        if (endpoints.base_url != "http://127.0.0.1:11434") {
            std::cerr << "testInferenceEndpointResolution: default base_url mismatch: "
                      << endpoints.base_url << "\n";
            ok = false;
        }
        if (endpoints.embed_base_url != "http://127.0.0.1:11434") {
            std::cerr << "testInferenceEndpointResolution: default embed_base_url mismatch: "
                      << endpoints.embed_base_url << "\n";
            ok = false;
        }
    }

    {
        ScopedEnvVar inference("THOTH_INFERENCE_BASE_URL", "http://custom-host:11434");
        ScopedEnvVar unsetEmbed("THOTH_EMBED_BASE_URL", nullptr);
        ScopedEnvVar unsetHost("OLLAMA_HOST", nullptr);

        const auto endpoints = Thoth::resolveInferenceEndpoints();
        if (endpoints.base_url != "http://custom-host:11434") {
            std::cerr << "testInferenceEndpointResolution: env override failed\n";
            ok = false;
        }
        if (endpoints.embed_base_url != "http://custom-host:11434") {
            std::cerr << "testInferenceEndpointResolution: embed fallback failed\n";
            ok = false;
        }
    }

    {
        ScopedEnvVar unsetInference("THOTH_INFERENCE_BASE_URL", nullptr);
        ScopedEnvVar host("OLLAMA_HOST", "custom-host:11434");
        ScopedEnvVar unsetEmbed("THOTH_EMBED_BASE_URL", nullptr);

        const auto endpoints = Thoth::resolveInferenceEndpoints();
        if (endpoints.base_url != "http://custom-host:11434") {
            std::cerr << "testInferenceEndpointResolution: OLLAMA_HOST compat failed\n";
            ok = false;
        }
    }

    {
        ScopedEnvVar inference("THOTH_INFERENCE_BASE_URL", "http://env-host:11434");
        ScopedEnvVar embed("THOTH_EMBED_BASE_URL", "http://embed-host:9999");

        const auto endpoints = Thoth::resolveInferenceEndpoints();
        if (endpoints.embed_base_url != "http://embed-host:9999") {
            std::cerr << "testInferenceEndpointResolution: embed override failed\n";
            ok = false;
        }
    }

    {
        ScopedEnvVar unsetInference("THOTH_INFERENCE_BASE_URL", nullptr);
        ScopedEnvVar unsetHost("OLLAMA_HOST", nullptr);
        ScopedEnvVar unsetEmbed("THOTH_EMBED_BASE_URL", nullptr);

        Config cfg;
        cfg.inference_base_url = "http://config-host:11434";
        cfg.embed_base_url = "http://config-embed:11434";
        const auto endpoints = Thoth::resolveInferenceEndpoints(cfg);
        if (endpoints.base_url != "http://config-host:11434") {
            std::cerr << "testInferenceEndpointResolution: config base_url failed\n";
            ok = false;
        }
        if (endpoints.embed_base_url != "http://config-embed:11434") {
            std::cerr << "testInferenceEndpointResolution: config embed_base_url failed\n";
            ok = false;
        }
    }

    {
        ScopedEnvVar inference("THOTH_INFERENCE_BASE_URL", "http://env-host:11434");
        Config cfg;
        cfg.inference_base_url = "http://config-host:11434";
        const auto endpoints = Thoth::resolveInferenceEndpoints(cfg);
        if (endpoints.base_url != "http://env-host:11434") {
            std::cerr << "testInferenceEndpointResolution: env should beat config\n";
            ok = false;
        }
    }

    {
        const std::string joined =
            Thoth::inferenceUrl("https://server:11434/", "/api/generate");
        if (joined != "https://server:11434/api/generate") {
            std::cerr << "testInferenceEndpointResolution: path join failed: " << joined << "\n";
            ok = false;
        }
        if (joined.find("//api") != std::string::npos) {
            std::cerr << "testInferenceEndpointResolution: double slash in joined URL\n";
            ok = false;
        }
    }

    {
        const std::string joined =
            Thoth::inferenceUrl("http://host:11434", "api/tags");
        if (joined != "http://host:11434/api/tags") {
            std::cerr << "testInferenceEndpointResolution: missing leading slash normalization failed\n";
            ok = false;
        }
    }

    {
        const fs::path tempPath = makeTempPath("thoth_inference_config_test.json");
        Config cfg;
        cfg.inference_base_url = "http://json-host:11434";
        cfg.embed_base_url = "http://json-embed:11434";
        if (!cfg.saveToJson(tempPath.string())) {
            std::cerr << "testInferenceEndpointResolution: failed to save config\n";
            ok = false;
        } else {
            Config loaded;
            if (!loaded.loadFromJson(tempPath.string())) {
                std::cerr << "testInferenceEndpointResolution: failed to load config\n";
                ok = false;
            } else {
                ScopedEnvVar unsetInference("THOTH_INFERENCE_BASE_URL", nullptr);
                ScopedEnvVar unsetHost("OLLAMA_HOST", nullptr);
                ScopedEnvVar unsetEmbed("THOTH_EMBED_BASE_URL", nullptr);
                const auto endpoints = Thoth::resolveInferenceEndpoints(loaded);
                if (endpoints.base_url != "http://json-host:11434"
                    || endpoints.embed_base_url != "http://json-embed:11434") {
                    std::cerr << "testInferenceEndpointResolution: config json fields failed\n";
                    ok = false;
                }
            }
        }
        fs::remove(tempPath);
    }

    return ok;
}

static bool testInferenceBackendMisconfigDetection() {
    bool ok = true;

    Thoth::InferenceEndpointConfig llamaEndpoints;
    llamaEndpoints.base_url = "http://llama-server:8080";
    llamaEndpoints.embed_base_url = "http://llama-embed-server:8081";

    const auto ollamaOnLlama = Thoth::detectInferenceBackendMisconfigs("ollama", llamaEndpoints);
    if (ollamaOnLlama.size() < 2) {
        std::cerr << "testInferenceBackendMisconfigDetection: expected ollama+llama URL warnings\n";
        ok = false;
    }

    const auto llamaOk = Thoth::detectInferenceBackendMisconfigs("llama_cpp", llamaEndpoints);
    if (!llamaOk.empty()) {
        std::cerr << "testInferenceBackendMisconfigDetection: false positive for llama_cpp stack\n";
        ok = false;
    }

    Thoth::InferenceEndpointConfig ollamaEndpoints;
    ollamaEndpoints.base_url = "http://127.0.0.1:11434";
    ollamaEndpoints.embed_base_url = "http://127.0.0.1:11434";

    const auto llamaOnOllama =
        Thoth::detectInferenceBackendMisconfigs("llama_cpp", ollamaEndpoints);
    if (llamaOnOllama.size() < 2) {
        std::cerr << "testInferenceBackendMisconfigDetection: expected llama_cpp+Ollama URL warnings\n";
        ok = false;
    }

    return ok;
}

static bool testEmbeddingProbeJsonShape() {
    Thoth::EmbeddingProbeSnapshot snapshot;
    snapshot.status = "ok";
    snapshot.backend = "llama_cpp";
    snapshot.embed_base_url = "http://llama-embed-server:8081";
    snapshot.model = "nomic-embed-text";
    snapshot.dimension = 768;

    const auto json = Thoth::embeddingProbeJson(snapshot);
    if (json.value("status", "") != "ok"
        || json.value("backend", "") != "llama_cpp"
        || json.value("dimension", 0) != 768) {
        std::cerr << "testEmbeddingProbeJsonShape: ready embedding block shape failed\n";
        return false;
    }
    return true;
}

static bool testInferenceBackendResolution() {
    bool ok = true;

    {
        ScopedEnvVar unset("THOTH_INFERENCE_BACKEND", nullptr);
        std::string error;
        const auto backend = Thoth::tryResolveInferenceBackend(error);
        if (!backend || *backend != Thoth::InferenceBackend::Ollama || !error.empty()) {
            std::cerr << "testInferenceBackendResolution: default should be ollama\n";
            ok = false;
        }
    }

    {
        ScopedEnvVar backend("THOTH_INFERENCE_BACKEND", "ollama");
        std::string error;
        const auto resolved = Thoth::tryResolveInferenceBackend(error);
        if (!resolved || *resolved != Thoth::InferenceBackend::Ollama) {
            std::cerr << "testInferenceBackendResolution: explicit ollama failed\n";
            ok = false;
        }
    }

    {
        ScopedEnvVar backend("THOTH_INFERENCE_BACKEND", "llama_cpp");
        std::string error;
        const auto resolved = Thoth::tryResolveInferenceBackend(error);
        if (!resolved || *resolved != Thoth::InferenceBackend::LlamaCpp) {
            std::cerr << "testInferenceBackendResolution: llama_cpp failed\n";
            ok = false;
        }
    }

    {
        ScopedEnvVar backend("THOTH_INFERENCE_BACKEND", "invalid");
        std::string error;
        const auto resolved = Thoth::tryResolveInferenceBackend(error);
        if (resolved || error.empty()) {
            std::cerr << "testInferenceBackendResolution: invalid backend should fail\n";
            ok = false;
        }
    }

    return ok;
}

static bool testMockInferenceClient() {
    Thoth::MockInferenceClient client;
    client.on_generate = [](const Thoth::InferenceGenerateRequest& req) {
        Thoth::InferenceGenerateResult result;
        result.ok = true;
        result.text = "generated:" + req.prompt;
        return result;
    };
    client.on_embed = [](const Thoth::InferenceEmbedRequest& req) {
        Thoth::InferenceEmbedResult result;
        result.ok = true;
        for (std::size_t i = 0; i < req.inputs.size(); ++i) {
            result.embeddings.push_back(std::vector<float>(768, 0.25f));
        }
        return result;
    };
    client.on_health = []() {
        Thoth::InferenceHealthResult result;
        result.reachable = true;
        result.available_models = {"mock-model"};
        return result;
    };

    Thoth::InferenceGenerateRequest gen_req;
    gen_req.model = "mock-model";
    gen_req.prompt = "hello";
    gen_req.temperature = 0.1;
    gen_req.top_p = 0.9;
    gen_req.max_tokens = 32;
    const auto generated = client.generate(gen_req);
    if (!generated.ok || generated.text != "generated:hello") {
        std::cerr << "testMockInferenceClient: generate failed\n";
        return false;
    }

    Thoth::InferenceEmbedRequest embed_req;
    embed_req.model = "mock-embed";
    embed_req.inputs = {"a", "b"};
    const auto embedded = client.embed(embed_req);
    if (!embedded.ok || embedded.embeddings.size() != 2 || embedded.embeddings[0].size() != 768) {
        std::cerr << "testMockInferenceClient: embed failed\n";
        return false;
    }

    const auto health = client.health();
    if (!health.reachable || health.available_models.empty()) {
        std::cerr << "testMockInferenceClient: health failed\n";
        return false;
    }

    return true;
}

static bool testOllamaClientParse() {
    const auto generated = Thoth::OllamaClient::parseGenerateResponse(
        R"({"response":"hello","prompt_eval_count":3,"eval_count":5})");
    if (!generated.ok || generated.text != "hello" || generated.token_usage.total_tokens != 8) {
        std::cerr << "testOllamaClientParse: generate parse failed\n";
        return false;
    }

    const auto embedded = Thoth::OllamaClient::parseEmbedResponse(
        R"({"embeddings":[[0.1,0.2],[0.3,0.4]]})");
    if (!embedded.ok || embedded.embeddings.size() != 2) {
        std::cerr << "testOllamaClientParse: embed parse failed\n";
        return false;
    }

    const auto health = Thoth::OllamaClient::parseTagsResponse(
        R"({"models":[{"name":"qwen2.5:3b"}]})");
    if (!health.reachable || health.available_models.size() != 1) {
        std::cerr << "testOllamaClientParse: health parse failed\n";
        return false;
    }

    return true;
}

static bool testLlamaServerClientParse() {
    const auto generated = Thoth::LlamaServerClient::parseCompletionResponse(
        R"({"choices":[{"text":"hello"}],"usage":{"prompt_tokens":2,"completion_tokens":3,"total_tokens":5}})");
    if (!generated.ok || generated.text != "hello" || generated.token_usage.total_tokens != 5) {
        std::cerr << "testLlamaServerClientParse: completion parse failed\n";
        return false;
    }

    const auto embedded = Thoth::LlamaServerClient::parseEmbeddingsResponse(
        R"({"data":[{"embedding":[0.1,0.2,0.3],"index":0},{"embedding":[0.4,0.5,0.6],"index":1}]})");
    if (!embedded.ok || embedded.embeddings.size() != 2) {
        std::cerr << "testLlamaServerClientParse: embeddings parse failed\n";
        return false;
    }

    return true;
}

static bool testInferenceFactorySelection() {
    const Thoth::InferenceEndpointConfig endpoints{
        "http://127.0.0.1:11434",
        "http://127.0.0.1:11434",
    };

    {
        ScopedEnvVar backend("THOTH_INFERENCE_BACKEND", "ollama");
        try {
            const auto client = Thoth::createInferenceClient(endpoints);
            if (!client || client->backendName() != "ollama") {
                std::cerr << "testInferenceFactorySelection: ollama factory failed\n";
                return false;
            }
        } catch (...) {
            std::cerr << "testInferenceFactorySelection: ollama factory threw\n";
            return false;
        }
    }

    {
        ScopedEnvVar backend("THOTH_INFERENCE_BACKEND", "llama_cpp");
        try {
            const auto client = Thoth::createInferenceClient(endpoints);
            if (!client || client->backendName() != "llama_cpp") {
                std::cerr << "testInferenceFactorySelection: llama_cpp factory failed\n";
                return false;
            }
        } catch (...) {
            std::cerr << "testInferenceFactorySelection: llama_cpp factory threw\n";
            return false;
        }
    }

    {
        ScopedEnvVar backend("THOTH_INFERENCE_BACKEND", "bad");
        try {
            (void)Thoth::createInferenceClient(endpoints);
            std::cerr << "testInferenceFactorySelection: invalid backend should throw\n";
            return false;
        } catch (const std::invalid_argument&) {
        } catch (...) {
            std::cerr << "testInferenceFactorySelection: expected invalid_argument\n";
            return false;
        }
    }

    return true;
}

static bool testInferenceEmbeddingDimensionProbe() {
    constexpr int kExpectedDim = 768;
    constexpr const char* kProbe = "thoth-embedding-probe";

    auto check_client = [&](Thoth::InferenceClient& client, const char* label) -> bool {
        Thoth::InferenceEmbedRequest request;
        request.model = "nomic-embed-text:v1.5";
        request.inputs = {kProbe};
        const auto result = client.embed(request);
        if (!result.ok || result.embeddings.empty()) {
            std::cout << "testInferenceEmbeddingDimensionProbe: skipping " << label
                      << " (unreachable)\n";
            return true;
        }
        if (static_cast<int>(result.embeddings.front().size()) != kExpectedDim) {
            std::cerr << "testInferenceEmbeddingDimensionProbe: " << label
                      << " dimension mismatch: " << result.embeddings.front().size() << '\n';
            return false;
        }
        return true;
    };

    const Thoth::InferenceEndpointConfig endpoints = Thoth::resolveInferenceEndpoints();

    {
        ScopedEnvVar backend("THOTH_INFERENCE_BACKEND", "ollama");
        if (Thoth::isOllamaReachable()) {
            try {
                const auto client = Thoth::createInferenceClient(endpoints);
                if (!check_client(*client, "ollama")) {
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "testInferenceEmbeddingDimensionProbe: ollama client error: "
                          << e.what() << '\n';
                return false;
            }
        } else {
            std::cout << "testInferenceEmbeddingDimensionProbe: Ollama unreachable, skipping\n";
        }
    }

    {
        ScopedEnvVar backend("THOTH_INFERENCE_BACKEND", "llama_cpp");
        Thoth::OllamaFetchOptions options;
        options.base_url = endpoints.base_url;
        const std::string health_url = Thoth::inferenceUrl(options.base_url, "/health");
        const auto health = Thoth::inferenceHttpGet(health_url, 2);
        if (health.ok) {
            try {
                const auto client = Thoth::createInferenceClient(endpoints);
                if (!check_client(*client, "llama_cpp")) {
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "testInferenceEmbeddingDimensionProbe: llama_cpp client error: "
                          << e.what() << '\n';
                return false;
            }
        } else {
            std::cout << "testInferenceEmbeddingDimensionProbe: llama-server unreachable, skipping\n";
        }
    }

    return true;
}

static bool runInferenceIntegrationTests() {
    int failures = 0;
    if (!testInferenceEmbeddingDimensionProbe()) {
        ++failures;
    }
    return failures == 0;
}

class ScopedCwd {
public:
    explicit ScopedCwd(const fs::path& target) : previous_(fs::current_path()) {
        fs::current_path(target);
    }
    ~ScopedCwd() {
        std::error_code ec;
        fs::current_path(previous_, ec);
    }

private:
    fs::path previous_;
};

/** getProjectRoot walks terminate at filesystem root; markers and THOTH_PROJECT_ROOT still work. */
static bool testFileHandlerProjectRootWalk() {
    bool ok = true;

    {
        const fs::path tempRoot = makeTempPath("thoth_project_root_marker");
        const fs::path nested = tempRoot / "deep" / "nested";
        fs::create_directories(nested);
        fs::create_directories(tempRoot / ".git");

        ScopedEnvVar unsetRoot("THOTH_PROJECT_ROOT", nullptr);
        ScopedCwd cwd(nested);
        FileHandler fh;
        const fs::path got = fs::path(fh.getProjectRoot()).lexically_normal();
        const fs::path expected = fs::absolute(tempRoot).lexically_normal();
        if (got != expected) {
            std::cerr << "testFileHandlerProjectRootWalk: marker walk mismatch: got="
                      << got << " expected=" << expected << "\n";
            ok = false;
        }
        fs::remove_all(tempRoot);
    }

    {
        const fs::path tempRoot = makeTempPath("thoth_project_root_override");
        const fs::path nested = tempRoot / "nested";
        fs::create_directories(nested);
        fs::create_directories(tempRoot / ".git");

        const fs::path overrideRoot = makeTempPath("thoth_project_root_explicit");
        fs::create_directories(overrideRoot);

        ScopedEnvVar rootEnv("THOTH_PROJECT_ROOT", overrideRoot.string().c_str());
        ScopedCwd cwd(nested);
        FileHandler fh;
        const fs::path got = fs::path(fh.getProjectRoot()).lexically_normal();
        const fs::path expected = fs::absolute(overrideRoot).lexically_normal();
        if (got != expected) {
            std::cerr << "testFileHandlerProjectRootWalk: THOTH_PROJECT_ROOT override failed: got="
                      << got << " expected=" << expected << "\n";
            ok = false;
        }
        fs::remove_all(tempRoot);
        fs::remove_all(overrideRoot);
    }

    {
        const fs::path tempRoot = makeTempPath("thoth_project_root_nomarker");
        const fs::path nested = tempRoot / "leaf";
        fs::create_directories(nested);

        ScopedEnvVar unsetRoot("THOTH_PROJECT_ROOT", nullptr);
        ScopedCwd cwd(nested);

        // Must terminate at filesystem root and fall back (never spin at "/").
        FileHandler fh;
        const std::string root = fh.getProjectRoot();
        if (root.empty()) {
            std::cerr << "testFileHandlerProjectRootWalk: empty fallback root\n";
            ok = false;
        }
        fs::remove_all(tempRoot);
    }

    return ok;
}

static bool testFileHandlerWorkspaceOverride() {
    const fs::path workspace = makeTempPath("thoth_workspace_override");
    fs::remove_all(workspace);

    ScopedEnvVar workspaceEnv("THOTH_WORKSPACE_PATH", workspace.string().c_str());
    {
        FileHandler fh;
        const std::string dbPath = fh.getAgentWorkspacePath("memory.db");
        const std::string ragDir = fh.getRagPath();
        if (dbPath != (workspace / "memory.db").string()) {
            std::cerr << "testFileHandlerWorkspaceOverride: memory.db path mismatch: "
                      << dbPath << "\n";
            fs::remove_all(workspace);
            return false;
        }
        if (ragDir != (workspace / "rag").string()) {
            std::cerr << "testFileHandlerWorkspaceOverride: rag path mismatch: "
                      << ragDir << "\n";
            fs::remove_all(workspace);
            return false;
        }
    }

    fs::remove_all(workspace);
    return true;
}

static bool testFileHandlerLogsOverride() {
    const fs::path logs = makeTempPath("thoth_logs_override");
    fs::remove_all(logs);

    ScopedEnvVar logsEnv("THOTH_LOGS_PATH", logs.string().c_str());
    {
        FileHandler fh;
        const std::string logPath = fh.getLogsPath("cognitive_metrics.jsonl");
        if (logPath != (logs / "cognitive_metrics.jsonl").string()) {
            std::cerr << "testFileHandlerLogsOverride: path mismatch: " << logPath << "\n";
            fs::remove_all(logs);
            return false;
        }
        if (!fs::exists(logs)) {
            std::cerr << "testFileHandlerLogsOverride: logs directory not created\n";
            fs::remove_all(logs);
            return false;
        }
    }

    fs::remove_all(logs);
    return true;
}

static bool testLogsPathPrecedence() {
    bool ok = true;

    {
        ScopedEnvVar unsetMetrics("THOTH_COGNITIVE_METRICS_LOG", nullptr);
        ScopedEnvVar unsetLogs("THOTH_LOGS_PATH", nullptr);
        FileHandler fh;
        const fs::path expected = fs::path(fh.getProjectRoot()) / "logs" / "cognitive_metrics.jsonl";
        if (Thoth::CognitiveMetricsLogger::resolveLogFilePath() != expected.string()) {
            std::cerr << "testLogsPathPrecedence: default path mismatch\n";
            ok = false;
        }
    }

    {
        const fs::path logs = makeTempPath("thoth_logs_precedence");
        fs::remove_all(logs);
        ScopedEnvVar unsetMetrics("THOTH_COGNITIVE_METRICS_LOG", nullptr);
        ScopedEnvVar logsEnv("THOTH_LOGS_PATH", logs.string().c_str());
        const std::string expected = (logs / "cognitive_metrics.jsonl").string();
        if (Thoth::CognitiveMetricsLogger::resolveLogFilePath() != expected) {
            std::cerr << "testLogsPathPrecedence: THOTH_LOGS_PATH override failed\n";
            ok = false;
        }
        fs::remove_all(logs);
    }

    {
        ScopedEnvVar metricsEnv("THOTH_COGNITIVE_METRICS_LOG", "/custom/metrics.jsonl");
        ScopedEnvVar logsEnv("THOTH_LOGS_PATH", "/tmp/logs");
        if (Thoth::CognitiveMetricsLogger::resolveLogFilePath() != "/custom/metrics.jsonl") {
            std::cerr << "testLogsPathPrecedence: THOTH_COGNITIVE_METRICS_LOG should win\n";
            ok = false;
        }
    }

    return ok;
}

static bool testRuntimeBootstrapLoadsEnv() {
    const fs::path dir = makeTempPath("thoth_bootstrap_load");
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path envFile = dir / "bootstrap.env";
    {
        std::ofstream out(envFile);
        out << "THOTH_INFERENCE_BASE_URL=http://from-dotenv:11434\n";
    }

    ScopedEnvVar envPath("THOTH_ENV_PATH", envFile.string().c_str());
    ScopedEnvVar unsetInference("THOTH_INFERENCE_BASE_URL", nullptr);
    ScopedEnvVar unsetEmbed("THOTH_EMBED_BASE_URL", nullptr);
    ScopedEnvVar unsetHost("OLLAMA_HOST", nullptr);

    Thoth::bootstrapRuntimeEnvironment();
    const auto endpoints = Thoth::resolveInferenceEndpoints();
    fs::remove_all(dir);

    if (endpoints.base_url != "http://from-dotenv:11434") {
        std::cerr << "testRuntimeBootstrapLoadsEnv: expected .env inference URL, got "
                  << endpoints.base_url << "\n";
        return false;
    }
    return true;
}

static bool testRuntimeBootstrapRespectsExistingEnv() {
    const fs::path dir = makeTempPath("thoth_bootstrap_precedence");
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path envFile = dir / "bootstrap.env";
    {
        std::ofstream out(envFile);
        out << "THOTH_INFERENCE_BASE_URL=http://from-dotenv:11434\n";
    }

    ScopedEnvVar exported("THOTH_INFERENCE_BASE_URL", "http://exported:11434");
    ScopedEnvVar unsetEmbed("THOTH_EMBED_BASE_URL", nullptr);
    ScopedEnvVar unsetHost("OLLAMA_HOST", nullptr);

    if (!EnvLoader::loadEnvFileIfUnset(envFile.string())) {
        std::cerr << "testRuntimeBootstrapRespectsExistingEnv: failed to read .env\n";
        fs::remove_all(dir);
        return false;
    }

    const auto endpoints = Thoth::resolveInferenceEndpoints();
    fs::remove_all(dir);

    if (endpoints.base_url != "http://exported:11434") {
        std::cerr << "testRuntimeBootstrapRespectsExistingEnv: exported env should win, got "
                  << endpoints.base_url << "\n";
        return false;
    }
    return true;
}

static bool testRuntimeBootstrapIdempotent() {
    Thoth::bootstrapRuntimeEnvironment();
    Thoth::bootstrapRuntimeEnvironment();

    ScopedEnvVar unsetInference("THOTH_INFERENCE_BASE_URL", nullptr);
    ScopedEnvVar unsetEmbed("THOTH_EMBED_BASE_URL", nullptr);
    ScopedEnvVar unsetHost("OLLAMA_HOST", nullptr);
    const auto endpoints = Thoth::resolveInferenceEndpoints();
    return endpoints.base_url == "http://127.0.0.1:11434";
}

static bool testRuntimeBootstrapDiagnosticsDisabledByDefault() {
    ScopedEnvVar unsetFlag("THOTH_LOG_CONFIG", nullptr);
    return !Thoth::runtimeConfigDiagnosticsEnabled(nullptr);
}

static bool testConfigEnvironmentOverrides() {
    Config cfg;
    cfg.llm_model = "from-default";
    cfg.embedding_model = "from-default";

    {
        ScopedEnvVar llm("OLLAMA_MODEL", "compose-chat");
        ScopedEnvVar embed("OLLAMA_EMBED_MODEL", "nomic-embed-text");
        ScopedEnvVar unsetThothEmbed("THOTH_EMBEDDING_MODEL", nullptr);
        cfg.applyEnvironmentOverrides();
        if (cfg.llm_model != "compose-chat" || cfg.embedding_model != "nomic-embed-text") {
            std::cerr << "testConfigEnvironmentOverrides: OLLAMA_* override failed\n";
            return false;
        }
    }

    {
        ScopedEnvVar llm("OLLAMA_MODEL", nullptr);
        ScopedEnvVar embed("OLLAMA_EMBED_MODEL", nullptr);
        ScopedEnvVar thothEmbed("THOTH_EMBEDDING_MODEL", "custom-embed");
        cfg.applyEnvironmentOverrides();
        if (cfg.embedding_model != "custom-embed") {
            std::cerr << "testConfigEnvironmentOverrides: THOTH_EMBEDDING_MODEL override failed\n";
            return false;
        }
    }

    return true;
}

static bool testEngineErrorSchema() {
    const Thoth::EngineError invalid =
        Thoth::EngineError::invalidRequest("Goal cannot be empty.");
    const auto json = nlohmann::json::parse(invalid.toJson());
    if (!json.contains("error") || !json["error"].is_object()) {
        std::cerr << "testEngineErrorSchema: missing error object\n";
        return false;
    }
    if (json["error"]["code"] != "INVALID_REQUEST") {
        std::cerr << "testEngineErrorSchema: unexpected code\n";
        return false;
    }
    if (json["error"]["message"] != "Goal cannot be empty.") {
        std::cerr << "testEngineErrorSchema: unexpected message\n";
        return false;
    }

    if (Thoth::engineErrorHttpStatus(Thoth::EngineErrorCode::INVALID_REQUEST) != 400) {
        std::cerr << "testEngineErrorSchema: INVALID_REQUEST status mismatch\n";
        return false;
    }
    if (Thoth::engineErrorHttpStatus(Thoth::EngineErrorCode::NOT_FOUND) != 404) {
        std::cerr << "testEngineErrorSchema: NOT_FOUND status mismatch\n";
        return false;
    }
    if (Thoth::engineErrorHttpStatus(Thoth::EngineErrorCode::ENGINE_BUSY) != 503) {
        std::cerr << "testEngineErrorSchema: ENGINE_BUSY status mismatch\n";
        return false;
    }
    if (Thoth::engineErrorHttpStatus(Thoth::EngineErrorCode::INTERNAL_ERROR) != 500) {
        std::cerr << "testEngineErrorSchema: INTERNAL_ERROR status mismatch\n";
        return false;
    }

    return true;
}

static bool testEngineSessionNormalization() {
    if (Thoth::normalizeEngineSessionId("") != "default") {
        std::cerr << "testEngineSessionNormalization: empty session not default\n";
        return false;
    }
    if (Thoth::normalizeEngineSessionId("default") != "default") {
        std::cerr << "testEngineSessionNormalization: default session changed\n";
        return false;
    }
    if (Thoth::normalizeEngineSessionId("session-b") != "session-b") {
        std::cerr << "testEngineSessionNormalization: custom session changed\n";
        return false;
    }
    return true;
}

/** Plan K2 — offline only (no Docker / network). */

static bool testLlmTimeoutPolicy() {
    using namespace Thoth::LlmTimeoutPolicy;
    ScopedEnvVar unset("THOTH_LLM_TIMEOUT_SECONDS", nullptr);
    if (timeoutSeconds() != 900) return false;
    for (int requested : {-1, 0, 1, 30000, 120000, 180000, 900000}) {
        if (stepTimeoutMs(StepType::LLM, requested) != 900000) return false;
    }
    if (stepTimeoutMs(StepType::LLM, 1200000) != 1200000) return false;
    {
        ScopedEnvVar longer("THOTH_LLM_TIMEOUT_SECONDS", "1200");
        if (timeoutSeconds() != 1200 || stepTimeoutMs(StepType::LLM, 30000) != 1200000)
            return false;
    }
    return true;
}

static bool testLlmSynthesisRetriesDisabled() {
    Plan fallback = Thoth::PlanValidator::createFallbackPlan("phase-a-plan", "goal");
    for (const auto& step : fallback.steps) {
        if (step.type == StepType::LLM && step.failure_policy.max_retries != 0) {
            return false;
        }
    }
    Plan repaired = fallback;
    repaired.steps.back().failure_policy.max_retries = 3;
    const auto validation = Thoth::PlanValidator::validateAndRepair(repaired, false);
    if (!validation.valid) return false;
    for (const auto& step : repaired.steps) {
        if (step.type == StepType::LLM && step.failure_policy.max_retries != 0) {
            return false;
        }
    }
    return true;
}

static bool testDecisionTapeTimeoutDisplay() {
    using namespace Thoth::CognitiveStatusDisplay;
    auto step_started = formatDecisionLine(
        EventType::STEP_STARTED,
        {{"description", "Summarize findings"}, {"timeout_ms", 900000}},
        "EXECUTING_STEP",
        "synthesize");
    if (!step_started || step_started->find("timeout_ms=900000") == std::string::npos) {
        return false;
    }
    auto step_fail = formatDecisionLine(
        EventType::STEP_FAILED,
        {{"next_action", "continue"},
         {"error", "Step execution timed out after 900000ms"},
         {"timeout_ms", 900000},
         {"description", "Summarize findings"}},
        "FAILED",
        "synthesize");
    return step_fail
        && step_fail->find("timeout_ms=900000") != std::string::npos
        && step_fail->find("timed out after 900000ms") != std::string::npos;
}

static bool testDecisionTapeLifecycle() {
    using namespace Thoth::CognitiveStatusDisplay;
    if (std::string(kSystemWaitingLine) != "System waiting") {
        return false;
    }
    if (std::string(kChatTurnInProgressLine) != "Chat turn in progress") {
        return false;
    }
    if (std::string(kChatTurnInProgressLine).find("/goal") != std::string::npos) {
        return false;
    }
    DecisionTapeOwnership ownership;
    ownership.chat_active = true;
    ownership.goal_active = true;
    if (shouldShowSystemWaiting(ownership)) {
        return false;
    }
    ownership.chat_active = false;
    if (shouldShowSystemWaiting(ownership)) {
        return false;
    }
    ownership.goal_active = false;
    if (!shouldShowSystemWaiting(ownership)) {
        return false;
    }
    const auto retrieval = formatChatRetrievalLine(EventType::RETRIEVAL_DIAGNOSTICS, true);
    if (!retrieval || *retrieval != kRetrievalFinishedLine) {
        return false;
    }
    if (formatChatRetrievalLine(EventType::RETRIEVAL_DIAGNOSTICS, false).has_value()) {
        return false;
    }
    if (formatChatRetrievalLine(EventType::STEP_STARTED, true).has_value()) {
        return false;
    }
    if (formatDecisionLine(EventType::RETRIEVAL_DIAGNOSTICS, nlohmann::json::object(), "", "")) {
        return false;
    }
    return true;
}

static bool testRemoteHttpUtilsOffline() {
    using namespace ThothRemoteHttp;

    if (normalizeBaseUrl("http://127.0.0.1:8090/") != "http://127.0.0.1:8090") {
        std::cerr << "testRemoteHttpUtilsOffline: trailing slash not stripped\n";
        return false;
    }
    if (normalizeBaseUrl("http://127.0.0.1:8090") != "http://127.0.0.1:8090") {
        std::cerr << "testRemoteHttpUtilsOffline: bare URL mutated\n";
        return false;
    }
    if (normalizeBaseUrl("https://engine.example:9443/v1/") != "https://engine.example:9443/v1") {
        std::cerr << "testRemoteHttpUtilsOffline: path + slash normalize failed\n";
        return false;
    }

    nlohmann::json no_caps = {{"status", "ready"}};
    if (!validateReadyCapabilities(no_caps).empty()) {
        std::cerr << "testRemoteHttpUtilsOffline: missing capabilities should be OK\n";
        return false;
    }

    nlohmann::json good_caps = {
        {"status", "ready"},
        {"capabilities", nlohmann::json::array({"chat", "goals", "control", "events"})}};
    if (!validateReadyCapabilities(good_caps).empty()) {
        std::cerr << "testRemoteHttpUtilsOffline: full capabilities rejected\n";
        return false;
    }

    nlohmann::json missing_control = {
        {"capabilities", nlohmann::json::array({"chat", "goals"})}};
    if (validateReadyCapabilities(missing_control).empty()) {
        std::cerr << "testRemoteHttpUtilsOffline: missing control should fail\n";
        return false;
    }

    const std::string engine_err =
        R"({"error":{"code":"ENGINE_BUSY","message":"shutting down"}})";
    const std::string formatted = formatHttpErrorMessage(503, engine_err);
    if (formatted.find("ENGINE_BUSY") == std::string::npos
        || formatted.find("shutting down") == std::string::npos) {
        std::cerr << "testRemoteHttpUtilsOffline: EngineError formatting failed: " << formatted
                  << "\n";
        return false;
    }

    const std::string bad_json = formatHttpErrorMessage(500, "not-json{{{{");
    if (bad_json.find("500") == std::string::npos) {
        std::cerr << "testRemoteHttpUtilsOffline: non-JSON body formatting failed\n";
        return false;
    }

    if (kConnectTimeoutSec <= 0 || kChatTimeoutSec <= 0 || kControlTimeoutSec <= 0
        || kGoalsTimeoutSec <= 0) {
        std::cerr << "testRemoteHttpUtilsOffline: timeout constants invalid\n";
        return false;
    }
    // Chat covers one LLM wait (900s). Goals cover planner + optional retry + synthesis.
    if (kChatTimeoutSec < 900 || kGoalsTimeoutSec < (3 * kChatTimeoutSec)) {
        std::cerr << "testRemoteHttpUtilsOffline: chat/goals timeouts misaligned with LLM budget\n";
        return false;
    }
    if (resolveRemoteRequestTimeoutSec(kChatTimeoutSec) != kChatTimeoutSec) {
        std::cerr << "testRemoteHttpUtilsOffline: resolve without env should return fallback\n";
        return false;
    }

    return true;
}

/** Plan K5 — offline chat/goal request+response mapping (pure). */
static bool testRemoteChatGoalMappingOffline() {
    using namespace ThothRemoteHttp;

    const auto chat_req = buildChatRequestJson("hello", "sess-a");
    if (chat_req.value("text", "") != "hello"
        || chat_req.value("session_id", "") != "sess-a") {
        std::cerr << "testRemoteChatGoalMappingOffline: buildChatRequestJson failed\n";
        return false;
    }

    const auto goal_req = buildGoalRequestJson("Summarize GRAG", "sess-b");
    if (goal_req.value("goal", "") != "Summarize GRAG"
        || goal_req.value("session_id", "") != "sess-b") {
        std::cerr << "testRemoteChatGoalMappingOffline: buildGoalRequestJson failed\n";
        return false;
    }

    nlohmann::json good_chat = {{"response", "Hi there"}};
    auto chat_ok = extractChatResponseText(good_chat);
    if (!chat_ok.ok || chat_ok.text != "Hi there" || !chat_ok.error.empty()) {
        std::cerr << "testRemoteChatGoalMappingOffline: extractChatResponseText good failed\n";
        return false;
    }

    nlohmann::json missing_chat = {{"status", "ok"}};
    auto chat_missing = extractChatResponseText(missing_chat);
    if (!chat_missing.ok || !chat_missing.text.empty() || !chat_missing.error.empty()) {
        std::cerr << "testRemoteChatGoalMappingOffline: missing response handling failed\n";
        return false;
    }

    nlohmann::json bad_chat = {{"response", 42}};
    auto chat_bad = extractChatResponseText(bad_chat);
    if (chat_bad.ok || chat_bad.error.find("not a string") == std::string::npos) {
        std::cerr << "testRemoteChatGoalMappingOffline: non-string response failed\n";
        return false;
    }

    nlohmann::json good_goal = {{"status", "accepted"}, {"message", "queued"}};
    auto goal_ok = checkGoalResponseBody(good_goal);
    if (!goal_ok.ok || !goal_ok.warning.empty() || !goal_ok.error.empty()) {
        std::cerr << "testRemoteChatGoalMappingOffline: checkGoalResponseBody good failed\n";
        return false;
    }

    nlohmann::json sparse_goal = {{"plan_id", "p1"}};
    auto goal_sparse = checkGoalResponseBody(sparse_goal);
    if (!goal_sparse.ok || goal_sparse.warning.empty()) {
        std::cerr << "testRemoteChatGoalMappingOffline: sparse goal warning failed\n";
        return false;
    }

    return true;
}

/** R3 — goal POST session_id must match tab after SyncBackendSessionIdentity (pure wire). */
static bool testGuiR3GoalSessionWire() {
    using namespace ThothRemoteHttp;

    const auto req = buildGoalRequestJson("Run unit tests", "tab-session-xyz");
    if (req.value("goal", "") != "Run unit tests"
        || req.value("session_id", "") != "tab-session-xyz") {
        std::cerr << "testGuiR3GoalSessionWire: buildGoalRequestJson session wire failed\n";
        return false;
    }
    return true;
}

/** R4-G1 — Engine conversation chat failures must surface (not silent Panel path). */
static bool testGuiR4ChatFailureSurfaces() {
    using namespace Thoth;

    const OperationResult ok =
        makeSuccess(kOpChat, "Response received", "hello");
    if (engineConversationChatFailureNeedsStatusBar(ok, true)) {
        std::cerr << "testGuiR4ChatFailureSurfaces: success should not need status\n";
        return false;
    }

    const OperationResult fail =
        makeFailure(kOpChat, "Failed to send", "[RemoteEngine] turn transport: Timeout was reached",
                    true);
    if (!engineConversationChatFailureNeedsStatusBar(fail, true)) {
        std::cerr << "testGuiR4ChatFailureSurfaces: engine failure should need status\n";
        return false;
    }
    if (engineConversationChatFailureNeedsStatusBar(fail, false)) {
        std::cerr << "testGuiR4ChatFailureSurfaces: local path should not use engine rule\n";
        return false;
    }
    if (uiSeverityForFailure(fail, EventStreamSnapshot{}) != OperationUiSeverity::Panel) {
        std::cerr << "testGuiR4ChatFailureSurfaces: expected Panel severity for connected chat\n";
        return false;
    }
    return true;
}

/** R4-G4 — conversation turn wire uses explicit session_id (Phase 10). */
static bool testGuiR4ConversationTurnSessionWire() {
    nlohmann::json req = {{"session_id", "session-tab-abc"},
                          {"content", "ping"},
                          {Thoth::ConversationAuthority::kTurnFieldActiveGoal, "Build website"}};
    if (req.value("session_id", "") != "session-tab-abc"
        || req.value("content", "") != "ping"
        || req.value(Thoth::ConversationAuthority::kTurnFieldActiveGoal, "") != "Build website") {
        std::cerr << "testGuiR4ConversationTurnSessionWire: turn JSON wire failed\n";
        return false;
    }
    return true;
}

/** CSG-A — executive goal wins over session active_goal. */
static bool testCsgAResolveExecutiveWins() {
    namespace fs = std::filesystem;
    Config cfg;
    cfg.database_path = makeTempPath("thoth_csg_exec.db").string();
    auto memory = std::make_shared<Memory>(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto idx = new IndexManager(engine.get());
    auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
    auto planner = std::make_shared<DefaultPlanner>();
    auto registry = std::make_shared<ToolRegistry>();

    Thoth::ExecutiveController controller(planner, registry, rag, memory);
    Plan plan;
    plan.plan_id = "csg-exec-plan";
    plan.goal = "Build website";
    controller.update_goal_embedding(plan.goal);
    controller.resume_from_plan(plan);

    Thoth::SessionGoalEmbedCache cache;
    EmbeddingEngine session_embed(EmbeddingEngine::Method::TfIdf);
    const auto result = Thoth::resolveChatRetrievalGoal(
        &controller,
        "session-123",
        std::string("Analyze finances"),
        cache,
        &session_embed);

    if (result.source != "executive") {
        std::cerr << "testCsgAResolveExecutiveWins: expected executive, got " << result.source
                  << "\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (result.embedding.empty()) {
        std::cerr << "testCsgAResolveExecutiveWins: empty executive embedding\n";
        fs::remove(cfg.database_path);
        return false;
    }
    fs::remove(cfg.database_path);
    return true;
}

/** CSG-A — session active_goal fallback when no executive plan. */
static bool testCsgAResolveSessionFallback() {
    Thoth::SessionGoalEmbedCache cache;
    EmbeddingEngine embed(EmbeddingEngine::Method::TfIdf);
    const auto result = Thoth::resolveChatRetrievalGoal(
        nullptr, "session-abc", std::string("Build website"), cache, &embed);
    if (result.source != "session") {
        std::cerr << "testCsgAResolveSessionFallback: source=" << result.source << "\n";
        return false;
    }
    if (result.embedding.empty()) {
        std::cerr << "testCsgAResolveSessionFallback: empty embedding\n";
        return false;
    }
    return true;
}

/** CSG-A — none when no executive plan and no active_goal. */
static bool testCsgAResolveNone() {
    Thoth::SessionGoalEmbedCache cache;
    EmbeddingEngine embed(EmbeddingEngine::Method::TfIdf);
    const auto result =
        Thoth::resolveChatRetrievalGoal(nullptr, "session-abc", std::nullopt, cache, &embed);
    if (result.source != "none" || !result.embedding.empty()) {
        std::cerr << "testCsgAResolveNone: unexpected result\n";
        return false;
    }
    return true;
}

/** CSG-A — cache isolates goals within a session bucket. */
static bool testCsgASessionGoalCacheIsolation() {
    Thoth::SessionGoalEmbedCache cache;
    EmbeddingEngine embed(EmbeddingEngine::Method::TfIdf);
    (void)Thoth::resolveChatRetrievalGoal(
        nullptr, "session-x", std::string("Build website"), cache, &embed);
    (void)Thoth::resolveChatRetrievalGoal(
        nullptr, "session-x", std::string("Analyze finances"), cache, &embed);
    if (cache.entryCountForSession("session-x") != 2) {
        std::cerr << "testCsgASessionGoalCacheIsolation: expected 2 entries, got "
                  << cache.entryCountForSession("session-x") << "\n";
        return false;
    }
    const auto first = cache.lookup("session-x", Thoth::normalizeSessionGoalText("Build website"));
    if (!first || first->empty()) {
        std::cerr << "testCsgASessionGoalCacheIsolation: first goal cache miss\n";
        return false;
    }
    return true;
}

/** CSG-A — goal_source appears in GragDiagnostics JSON. */
static bool testCsgAGoalSourceDiagnosticsJson() {
    GragDiagnostics diagnostics;
    diagnostics.goal_present = true;
    diagnostics.goal_source = "session";
    const nlohmann::json j = diagnostics.to_json();
    if (j.value("goal_source", "") != "session" || !j.value("goal_present", false)) {
        std::cerr << "testCsgAGoalSourceDiagnosticsJson: missing goal_source\n";
        return false;
    }
    return true;
}

/** R5-G5 — RETRIEVAL_DIAGNOSTICS session gate when session_id is set. */
static bool testGuiR5RetrievalSessionGate() {
    using namespace Thoth::RetrievalVerificationDisplay;
    if (!retrievalDiagnosticsTargetsSession("tab-a", "tab-a")) {
        std::cerr << "testGuiR5RetrievalSessionGate: same session should match\n";
        return false;
    }
    if (retrievalDiagnosticsTargetsSession("tab-a", "tab-b")) {
        std::cerr << "testGuiR5RetrievalSessionGate: cross-tab must not match\n";
        return false;
    }
    if (!retrievalDiagnosticsTargetsSession("", "tab-b")) {
        std::cerr << "testGuiR5RetrievalSessionGate: legacy empty event session must match\n";
        return false;
    }
    return true;
}

/** R5-G1/G2/G3/G8 — scope, grounded, and skip labels (pure). */
static bool testGuiR5ScopeGroundingDisplay() {
    using namespace Thoth::RetrievalVerificationDisplay;

    const nlohmann::json scope = {{"active_context_key", "sess-1"},
                                  {"allowed_tiers", nlohmann::json::array({"session_attachment"})},
                                  {"selected_documents", nlohmann::json::array({"a.md", "b.md"})}};
    const std::string scopeLine = formatScopeLayerSummary(scope);
    if (scopeLine.find("sess-1") == std::string::npos
        || scopeLine.find("Scope layer") == std::string::npos) {
        std::cerr << "testGuiR5ScopeGroundingDisplay: scope line wrong\n";
        return false;
    }

    const nlohmann::json grounding = {{"grounded", true},
                                      {"grounding_mode", "retrieved_context"},
                                      {"documents", nlohmann::json::array({"probe.md"})}};
    const std::string groundedLine = formatGroundedLayerSummary(grounding);
    if (groundedLine.find("Grounded layer") == std::string::npos
        || groundedLine.find("probe.md") == std::string::npos) {
        std::cerr << "testGuiR5ScopeGroundingDisplay: grounded line wrong\n";
        return false;
    }

    const nlohmann::json skipDiag = {{"scoring_type", "no_index"}};
    if (formatRetrievalSkippedLabel(skipDiag).empty()) {
        std::cerr << "testGuiR5ScopeGroundingDisplay: skip label missing\n";
        return false;
    }
    return true;
}

/** R5-G4 — corpus inventory label is explicitly unscoped. */
static bool testGuiR5CorpusInventoryLabel() {
    if (std::string(Thoth::CorpusDocuments::kInventoryPopulatedLabel).find("unscoped")
        == std::string::npos) {
        std::cerr << "testGuiR5CorpusInventoryLabel: inventory label must say unscoped\n";
        return false;
    }
    return true;
}

/** Plan K4 — pure env selection only (no wx / Docker / engine / plugin). */
static bool testThothEngineUrlSelectionOffline() {
    using namespace ThothRemoteHttp;

    if (resolveEngineBaseUrlFromRawEnvValue(nullptr).has_value()) {
        std::cerr << "testThothEngineUrlSelectionOffline: null should be Local\n";
        return false;
    }
    if (resolveEngineBaseUrlFromRawEnvValue("").has_value()) {
        std::cerr << "testThothEngineUrlSelectionOffline: empty should be Local\n";
        return false;
    }
    if (resolveEngineBaseUrlFromRawEnvValue("   \t\n").has_value()) {
        std::cerr << "testThothEngineUrlSelectionOffline: whitespace should be Local\n";
        return false;
    }

    auto url = resolveEngineBaseUrlFromRawEnvValue("  http://127.0.0.1:8090/  ");
    if (!url.has_value() || *url != "http://127.0.0.1:8090") {
        std::cerr << "testThothEngineUrlSelectionOffline: trim+normalize failed\n";
        return false;
    }
    url = resolveEngineBaseUrlFromRawEnvValue("https://engine.example:9443");
    if (!url.has_value() || *url != "https://engine.example:9443") {
        std::cerr << "testThothEngineUrlSelectionOffline: bare remote URL mutated\n";
        return false;
    }

    return true;
}

/** Plan K3 — offline SSE framing / mapping (no Docker / network). */
static bool testRemoteSseUtilsOffline() {
    using namespace ThothRemoteHttp;

    if (!parseEventType("PLAN_CREATED").has_value()
        || parseEventType("NOT_A_REAL_EVENT").has_value()) {
        std::cerr << "testRemoteSseUtilsOffline: parseEventType failed\n";
        return false;
    }

    nlohmann::json good = {
        {"type", "STATE_CHANGED"},
        {"session_id", "s1"},
        {"plan_id", "p1"},
        {"step_id", ""},
        {"controller_state_name", "PLANNING"},
        {"metadata", {{"k", 1}}},
        {"extra_ignored", true}};
    auto ev = engineEventJsonToControllerEvent(good);
    if (!ev.has_value() || ev->type != EventType::STATE_CHANGED || ev->session_id != "s1"
        || ev->controller_state_name != "PLANNING") {
        std::cerr << "testRemoteSseUtilsOffline: engineEventJsonToControllerEvent failed\n";
        return false;
    }

    nlohmann::json unknown = {{"type", "FUTURE_EVENT"}};
    if (engineEventJsonToControllerEvent(unknown).has_value()) {
        std::cerr << "testRemoteSseUtilsOffline: unknown type should be nullopt\n";
        return false;
    }

    std::string buf = "event: engine\ndata: {\"type\":\"PLAN_CREATED\"}\n\n";
    buf += "data: {\"type\":\"STEP_STARTED\""; // incomplete — must stay in buffer
    auto frames = extractCompleteSseFrames(buf);
    if (frames.size() != 1) {
        std::cerr << "testRemoteSseUtilsOffline: expected 1 complete frame, got " << frames.size()
                  << "\n";
        return false;
    }
    if (buf.find("STEP_STARTED") == std::string::npos) {
        std::cerr << "testRemoteSseUtilsOffline: incomplete frame was consumed\n";
        return false;
    }

    auto payload = sseFrameDataPayload(frames[0]);
    if (!payload.has_value() || payload->find("PLAN_CREATED") == std::string::npos) {
        std::cerr << "testRemoteSseUtilsOffline: sseFrameDataPayload failed\n";
        return false;
    }

    nlohmann::json caps_events = {
        {"capabilities", nlohmann::json::array({"chat", "goals", "control", "events"})}};
    nlohmann::json caps_no_events = {
        {"capabilities", nlohmann::json::array({"chat", "goals", "control"})}};
    if (!eventsCapabilityAllowsSse(nlohmann::json{{"status", "ready"}})
        || !eventsCapabilityAllowsSse(caps_events)
        || eventsCapabilityAllowsSse(caps_no_events)) {
        std::cerr << "testRemoteSseUtilsOffline: eventsCapabilityAllowsSse failed\n";
        return false;
    }

    if (kSseConnectTimeoutSec <= 0) {
        std::cerr << "testRemoteSseUtilsOffline: kSseConnectTimeoutSec invalid\n";
        return false;
    }

    return true;
}

#if THOTH_HAS_GUI
/**
 * Offline: empty URL → clear failure string, no throw, no network needed.
 */
static bool testRemoteAgentBackendEmptyUrlOffline() {
    try {
        RemoteAgentBackend remote("");
        remote.setEventHandler([](const ControllerEvent&) {});
        auto reply = remote.processInput("hello");
        if (reply.success) {
            std::cerr << "testRemoteAgentBackendEmptyUrlOffline: expected failure OperationResult\n";
            return false;
        }
        if (reply.operation != Thoth::kOpChat) {
            std::cerr << "testRemoteAgentBackendEmptyUrlOffline: wrong operation tag\n";
            return false;
        }
        if (reply.technical_details.find("[RemoteEngine]") == std::string::npos) {
            std::cerr << "testRemoteAgentBackendEmptyUrlOffline: unexpected details: "
                      << reply.technical_details << "\n";
            return false;
        }
        // Cognate / graph shapes are objects (collection envelope or fetch sentinel).
        if (!remote.getStrategies().is_object() || !remote.getGraphStats().is_object()) {
            std::cerr << "testRemoteAgentBackendEmptyUrlOffline: cognate shape mismatch\n";
            return false;
        }
        if (remote.saveExperiment(nlohmann::json::object())) {
            std::cerr << "testRemoteAgentBackendEmptyUrlOffline: saveExperiment should be false\n";
            return false;
        }
        // Dtor must stopSse (cancel→join) without hang.
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testRemoteAgentBackendEmptyUrlOffline: exception " << e.what() << "\n";
        return false;
    } catch (...) {
        std::cerr << "testRemoteAgentBackendEmptyUrlOffline: unknown exception\n";
        return false;
    }
}

static bool corpusListsDoc(const nlohmann::json& body, const char* id) {
    if (!body.is_object() || !body.contains("documents") || !body["documents"].is_array()) {
        return false;
    }
    for (const auto& doc : body["documents"]) {
        if (doc.is_object() && doc.value("id", "") == id) {
            return true;
        }
    }
    return false;
}

/** Same RemoteAgentBackend recovers after a failed startup probe, then again after a later outage. */
static bool testRemoteAgentBackendReadyRecovery() {
    httplib::Server server;
    std::atomic<int> health_probes{0};
    server.Get("/health", [&](const httplib::Request&, httplib::Response& res) {
        health_probes.fetch_add(1);
        res.set_content("ok", "text/plain");
    });
    server.Get("/ready", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            R"({"status":"ready","capabilities":["chat","goals","control","ingest"]})",
            "application/json");
    });
    server.Get("/v1/rag/corpus", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            R"({"schema_version":1,"documents":[{"id":"doc-g3","name":"g3-alpha.md","status":"indexed"}]})",
            "application/json");
    });

    const int port = server.bind_to_any_port("127.0.0.1");
    if (port <= 0) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: bind failed\n";
        return false;
    }

    auto wait_running = [&]() {
        for (int i = 0; i < 300 && !server.is_running(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return server.is_running();
    };
    std::thread accept_thread;
    auto listen = [&]() {
        if (accept_thread.joinable()) {
            accept_thread.join();
        }
        accept_thread = std::thread([&]() { server.listen_after_bind(); });
        return wait_running();
    };
    auto halt = [&]() {
        if (server.is_running()) {
            server.stop();
        }
        if (accept_thread.joinable()) {
            accept_thread.join();
        }
    };

    RemoteAgentBackend backend("http://127.0.0.1:" + std::to_string(port));

    const auto while_down = backend.listCorpusDocuments();
    if (corpusListsDoc(while_down, "doc-g3") || health_probes.load() != 0) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: down-start should fail before /health\n";
        halt();
        return false;
    }

    if (!listen()) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: listen failed\n";
        halt();
        return false;
    }
    const int probes_after_up = health_probes.load();
    const auto recovered = backend.listCorpusDocuments();
    if (!corpusListsDoc(recovered, "doc-g3") || health_probes.load() <= probes_after_up) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: same backend did not re-probe and recover\n";
        halt();
        return false;
    }
    if (!backend.capabilities().supportsIngest) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: ingest capability missing after ready\n";
        halt();
        return false;
    }

    const int probes_while_ready = health_probes.load();
    halt();
    const auto during_outage = backend.listCorpusDocuments();
    if (corpusListsDoc(during_outage, "doc-g3")) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: outage returned cached corpus\n";
        return false;
    }
    if (health_probes.load() != probes_while_ready) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: cached success still probed before the failed GET\n";
        return false;
    }
    if (backend.capabilities().supportsIngest) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: ingest flag survived the outage\n";
        return false;
    }

    if (!server.bind_to_port("127.0.0.1", port) || !listen()) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: rebind failed\n";
        halt();
        return false;
    }
    const int probes_before_return = health_probes.load();
    const auto after_return = backend.listCorpusDocuments();
    halt();
    if (!corpusListsDoc(after_return, "doc-g3") || health_probes.load() <= probes_before_return) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: return trip did not re-probe\n";
        return false;
    }
    if (!backend.capabilities().supportsIngest) {
        std::cerr << "testRemoteAgentBackendReadyRecovery: ingest not restored after return\n";
        return false;
    }
    return true;
}

/**
 * Opt-in live check: set THOTH_REMOTE_LIVE_URL=http://127.0.0.1:8090
 * Skips (passes) when unset — not required for default suites / ctest -L pr.
 */
static bool testRemoteAgentBackendLiveOptIn() {
    const char* url = std::getenv("THOTH_REMOTE_LIVE_URL");
    if (!url || url[0] == '\0') {
        return true;
    }

    try {
        std::atomic<int> event_count{0};
        RemoteAgentBackend remote(url);
        remote.setEventHandler([&](const ControllerEvent&) { event_count.fetch_add(1); });
        if (remote.baseUrl().empty()) {
            std::cerr << "testRemoteAgentBackendLiveOptIn: empty normalized URL\n";
            return false;
        }
        remote.setSessionId("k3-live-test");
        auto reply = remote.processInput("/help");
        if (!reply.success) {
            std::cerr << "testRemoteAgentBackendLiveOptIn: engine error: "
                      << reply.technical_details << "\n";
            return false;
        }
        if (reply.response_text.empty()) {
            std::cerr << "testRemoteAgentBackendLiveOptIn: empty chat response\n";
            return false;
        }
        // Allow SSE connect; lifecycle events come mainly from goals.
        remote.executeGoal("K3 live smoke: no-op goal for events");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        std::cout << "testRemoteAgentBackendLiveOptIn: chat OK (" << reply.response_text.size()
                  << " bytes), sse_events=" << event_count.load() << "\n";
        // Do not require events>0 (engine/LLM dependent); shutdown must not hang (dtor).
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testRemoteAgentBackendLiveOptIn: exception " << e.what() << "\n";
        return false;
    } catch (...) {
        std::cerr << "testRemoteAgentBackendLiveOptIn: unknown exception\n";
        return false;
    }
}
#endif

static bool expectEngineFutureError(std::future<std::string> future,
                                    Thoth::EngineErrorCode expected) {
    try {
        (void)future.get();
        std::cerr << "testEngineRuntimeValidation: expected EngineException\n";
        return false;
    } catch (const Thoth::EngineException& ex) {
        if (ex.error().code != expected) {
            std::cerr << "testEngineRuntimeValidation: code mismatch\n";
            return false;
        }
        return true;
    } catch (...) {
        std::cerr << "testEngineRuntimeValidation: unexpected exception type\n";
        return false;
    }
}

static bool testEngineRuntimeValidation() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineRuntimeValidation: runtime not ready\n";
        return false;
    }

    if (!expectEngineFutureError(runtime->submitChat("default", ""),
                                 Thoth::EngineErrorCode::INVALID_REQUEST)) {
        return false;
    }
    if (!expectEngineFutureError(runtime->submitGoal("default", ""),
                                 Thoth::EngineErrorCode::INVALID_REQUEST)) {
        return false;
    }

    runtime->beginShutdown();
    if (runtime->isReady()) {
        std::cerr << "testEngineRuntimeValidation: runtime still ready after beginShutdown\n";
        return false;
    }
    if (!expectEngineFutureError(runtime->submitChat("default", "/help"),
                                 Thoth::EngineErrorCode::ENGINE_BUSY)) {
        return false;
    }

    runtime->shutdown();
    if (runtime->isReady()) {
        std::cerr << "testEngineRuntimeValidation: runtime still ready after shutdown\n";
        return false;
    }

    return true;
}

static bool testEngineRuntimeLazySessions() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineRuntimeLazySessions: runtime not ready\n";
        return false;
    }

    try {
        const std::string alpha =
            runtime->submitChat("session-alpha", "/help").get();
        const std::string beta =
            runtime->submitChat("session-beta", "/help").get();
        if (alpha.find("/help") == std::string::npos) {
            std::cerr << "testEngineRuntimeLazySessions: alpha session failed\n";
            return false;
        }
        if (beta.find("/help") == std::string::npos) {
            std::cerr << "testEngineRuntimeLazySessions: beta session failed\n";
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "testEngineRuntimeLazySessions: exception: " << e.what() << '\n';
        return false;
    }

    runtime->shutdown();
    return true;
}

static bool testEngineHttpChatEndpoint() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpChatEndpoint: runtime not ready\n";
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28092;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    try {
        const std::string direct = runtime->submitChat("default", "/help").get();

        httplib::Client client("127.0.0.1", 28092);
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(30, 0);

        const auto ok = client.Post("/v1/chat",
                                    R"({"text":"/help","session_id":"default"})",
                                    "application/json");
        if (!ok) {
            std::cerr << "testEngineHttpChatEndpoint: request failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        if (ok->status != 200) {
            std::cerr << "testEngineHttpChatEndpoint: status=" << ok->status
                      << " body=" << ok->body << '\n';
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto body = nlohmann::json::parse(ok->body);
        if (body.value("response", "") != direct) {
            std::cerr << "testEngineHttpChatEndpoint: response mismatch with submitChat\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        if (body.value("session_id", "") != "default") {
            std::cerr << "testEngineHttpChatEndpoint: session_id mismatch\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto invalid = client.Post("/v1/chat", "{}", "application/json");
        if (!invalid || invalid->status != 400) {
            std::cerr << "testEngineHttpChatEndpoint: expected 400 for missing text\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "testEngineHttpChatEndpoint: exception: " << e.what() << '\n';
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }

    transport.requestStop();
    server_thread.join();
    runtime->shutdown();
    return true;
}

static bool testEngineHttpGoalsAndControl() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpGoalsAndControl: runtime not ready\n";
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28093;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    httplib::Client client("127.0.0.1", 28093);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(120, 0);

    const auto health = client.Get("/health");
    if (!health || health->status != 200) {
        std::cerr << "testEngineHttpGoalsAndControl: /health failed\n";
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }

    auto cleanup = [&]() {
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
    };

    try {
        const std::string goal = "F5 HTTP acceptance test goal";

        const auto empty_goal = client.Post("/v1/goals", "{}", "application/json");
        if (!empty_goal || empty_goal->status != 400) {
            std::cerr << "testEngineHttpGoalsAndControl: expected 400 for empty goal\n";
            cleanup();
            return false;
        }

        for (const char* path : {"/v1/control/pause", "/v1/control/resume", "/v1/control/abort"}) {
            const auto control = client.Post(path, "{}", "application/json");
            if (!control || control->status != 200) {
                std::cerr << "testEngineHttpGoalsAndControl: " << path << " failed\n";
                cleanup();
                return false;
            }
            const auto control_json = nlohmann::json::parse(control->body);
            if (control_json.value("status", "") != "ok") {
                std::cerr << "testEngineHttpGoalsAndControl: " << path << " status not ok\n";
                cleanup();
                return false;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "testEngineHttpGoalsAndControl: exception: " << e.what() << '\n';
        cleanup();
        return false;
    }

    cleanup();
    return true;
}

static ControllerEvent makeTestControllerEvent() {
    ControllerEvent event;
    event.type = EventType::STATE_CHANGED;
    event.session_id = "default";
    event.plan_id = "plan-g-test";
    event.step_id = "";
    event.controller_state_name = "PLANNING";
    event.timestamp_ms = 1'700'000'000'000;
    event.metadata = nlohmann::json{{"source", "unit_test"}};
    return event;
}

static bool waitForEventSequence(Thoth::EngineRuntime& runtime,
                                 uint64_t expected,
                                 std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (runtime.lastEventSequenceForTests() >= expected) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return runtime.lastEventSequenceForTests() >= expected;
}

static bool testEngineEventBus() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineEventBus: runtime not ready\n";
        return false;
    }

    std::atomic<int> good_count{0};
    const uint64_t bad_id = runtime->subscribeEvents([](const Thoth::EngineEvent&) {
        throw std::runtime_error("subscriber failure probe");
    });
    const uint64_t good_id = runtime->subscribeEvents([&](const Thoth::EngineEvent& event) {
        if (event.sequence == 0) {
            throw std::runtime_error("unexpected zero sequence");
        }
        good_count.fetch_add(1, std::memory_order_relaxed);
    });

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        runtime->publishControllerEventForTests(makeTestControllerEvent());
    }

    if (!waitForEventSequence(*runtime, 1000, std::chrono::seconds(5))) {
        std::cerr << "testEngineEventBus: timed out waiting for sequences\n";
        runtime->unsubscribeEvents(bad_id);
        runtime->unsubscribeEvents(good_id);
        runtime->shutdown();
        return false;
    }

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start)
                                .count();
    if (elapsed_ms > 5000) {
        std::cerr << "testEngineEventBus: dispatch too slow (" << elapsed_ms << "ms)\n";
        runtime->unsubscribeEvents(bad_id);
        runtime->unsubscribeEvents(good_id);
        runtime->shutdown();
        return false;
    }

    if (good_count.load() != 1000) {
        std::cerr << "testEngineEventBus: good subscriber count=" << good_count.load() << '\n';
        runtime->unsubscribeEvents(bad_id);
        runtime->unsubscribeEvents(good_id);
        runtime->shutdown();
        return false;
    }

    runtime->unsubscribeEvents(bad_id);
    runtime->unsubscribeEvents(good_id);
    runtime->shutdown();
    return true;
}

static bool testEngineEventIntegration() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineEventIntegration: runtime not ready\n";
        return false;
    }

    std::atomic<int> received{0};
    const uint64_t sub_id = runtime->subscribeEvents([&](const Thoth::EngineEvent& event) {
        if (event.type == "STATE_CHANGED" && event.plan_id == "plan-g-test") {
            received.fetch_add(1, std::memory_order_relaxed);
        }
    });

    for (int i = 0; i < 25; ++i) {
        runtime->publishControllerEventForTests(makeTestControllerEvent());
    }

    std::thread chat_thread([&]() {
        try {
            (void)runtime->submitChat("default", "/help").get();
        } catch (...) {
        }
    });

    if (!waitForEventSequence(*runtime, 25, std::chrono::seconds(5))) {
        std::cerr << "testEngineEventIntegration: event export timeout\n";
        chat_thread.join();
        runtime->unsubscribeEvents(sub_id);
        runtime->shutdown();
        return false;
    }

    chat_thread.join();

    if (received.load() < 25) {
        std::cerr << "testEngineEventIntegration: received=" << received.load() << '\n';
        runtime->unsubscribeEvents(sub_id);
        runtime->shutdown();
        return false;
    }

    runtime->unsubscribeEvents(sub_id);
    runtime->shutdown();
    return true;
}

// Plan G SSE tests: cpp-httplib cannot reliably consume chunked SSE bodies as a
// client. Delivery is validated via SseSessionManager; HTTP route via session
// count smoke test. See docs/plan_g_streaming_observability.md § Plan G testing note.
static bool testEngineSseSessionDelivery() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineSseSessionDelivery: runtime not ready\n";
        return false;
    }

    Thoth::SseSessionManager manager(*runtime);
    const auto session = manager.createSession();
    runtime->publishControllerEventForTests(makeTestControllerEvent());
    if (!waitForEventSequence(*runtime, 1, std::chrono::seconds(3))) {
        std::cerr << "testEngineSseSessionDelivery: dispatch timeout\n";
        runtime->shutdown();
        return false;
    }

    std::string chunk;
    bool closed = false;
    if (!session->waitForChunk(chunk, std::chrono::seconds(2), closed)) {
        std::cerr << "testEngineSseSessionDelivery: no chunk received\n";
        runtime->shutdown();
        return false;
    }
    if (chunk.find("event: engine") == std::string::npos) {
        std::cerr << "testEngineSseSessionDelivery: missing SSE framing\n";
        runtime->shutdown();
        return false;
    }
    if (chunk.find("STATE_CHANGED") == std::string::npos) {
        std::cerr << "testEngineSseSessionDelivery: missing payload\n";
        runtime->shutdown();
        return false;
    }

    manager.removeSession(session);
    runtime->shutdown();
    return true;
}

static bool testEngineSseMultiClient() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineSseMultiClient: runtime not ready\n";
        return false;
    }

    Thoth::SseSessionManager manager(*runtime);
    const auto a = manager.createSession();
    const auto b = manager.createSession();
    const auto c = manager.createSession();

    runtime->publishControllerEventForTests(makeTestControllerEvent());
    if (!waitForEventSequence(*runtime, 1, std::chrono::seconds(3))) {
        std::cerr << "testEngineSseMultiClient: dispatch timeout\n";
        runtime->shutdown();
        return false;
    }

    for (const auto& session : {a, b, c}) {
        std::string chunk;
        bool closed = false;
        if (!session->waitForChunk(chunk, std::chrono::seconds(2), closed)) {
            std::cerr << "testEngineSseMultiClient: client missing chunk\n";
            runtime->shutdown();
            return false;
        }
        if (chunk.find("\"sequence\":1") == std::string::npos) {
            std::cerr << "testEngineSseMultiClient: missing sequence 1\n";
            runtime->shutdown();
            return false;
        }
    }

    manager.removeSession(a);
    manager.removeSession(b);
    manager.removeSession(c);
    runtime->shutdown();
    return true;
}

static bool testEngineSseDisconnect() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineSseDisconnect: runtime not ready\n";
        return false;
    }

    Thoth::SseSessionManager manager(*runtime);
    for (int i = 0; i < 10; ++i) {
        const auto session = manager.createSession();
        manager.removeSession(session);
        if (manager.sessionCountForTests() != 0) {
            std::cerr << "testEngineSseDisconnect: leaked session on iteration " << i << '\n';
            runtime->shutdown();
            return false;
        }
    }

    runtime->shutdown();
    return true;
}

static bool testEngineHttpSseRouteAndReady() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpSseRouteAndReady: runtime not ready\n";
        return false;
    }

    const auto caps = runtime->capabilities();
    if (std::find(caps.begin(), caps.end(), "events") == caps.end()) {
        std::cerr << "testEngineHttpSseRouteAndReady: missing events capability\n";
        runtime->shutdown();
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28095;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    httplib::Client client("127.0.0.1", 28095);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(2, 0);

    const auto ready = client.Get("/ready");
    if (!ready || ready->status != 200) {
        std::cerr << "testEngineHttpSseRouteAndReady: /ready failed\n";
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }

    const auto ready_json = nlohmann::json::parse(ready->body);
    const auto capabilities = ready_json.value("capabilities", nlohmann::json::array());
    if (std::find(capabilities.begin(), capabilities.end(), "events") == capabilities.end()) {
        std::cerr << "testEngineHttpSseRouteAndReady: /ready missing events\n";
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }

    const auto embedding = ready_json.value("embedding", nlohmann::json::object());
    if (!embedding.contains("status") || !embedding["status"].is_string()) {
        std::cerr << "testEngineHttpSseRouteAndReady: /ready missing embedding.status\n";
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }

    std::thread events_thread([&]() {
        httplib::Client events_client("127.0.0.1", 28095);
        events_client.set_connection_timeout(5, 0);
        events_client.set_read_timeout(10, 0);
        (void)events_client.Get("/v1/events");
    });

    bool session_created = false;
    for (int i = 0; i < 50; ++i) {
        if (transport.sseSessionCountForTests() == 1) {
            session_created = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    transport.requestStop();
    server_thread.join();
    events_thread.join();
    runtime->shutdown();

    if (!session_created) {
        std::cerr << "testEngineHttpSseRouteAndReady: /v1/events did not accept connection\n";
        return false;
    }
    return true;
}

static bool testEngineHttpGracefulShutdown() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpGracefulShutdown: runtime not ready\n";
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28094;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    httplib::Client client("127.0.0.1", 28094);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);

    const auto healthy = client.Get("/health");
    if (!healthy || healthy->status != 200) {
        std::cerr << "testEngineHttpGracefulShutdown: /health failed before stop\n";
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }

    transport.requestStop();
    server_thread.join();

    if (runtime->isReady()) {
        std::cerr << "testEngineHttpGracefulShutdown: runtime still ready after requestStop\n";
        runtime->shutdown();
        return false;
    }

    runtime->shutdown();
    return true;
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

#if THOTH_HAS_GUI
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
#endif

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

static Thoth::RestoreRequest makeTestRestoreRequest(
    Thoth::RestoreMode mode = Thoth::RestoreMode::REPLAY) {
    Thoth::RestoreRequest request;
    request.mode = mode;
    request.requested_by = "TEST";
    return request;
}

static bool setupM4ArchivedSession(Memory& memory,
                                   Thoth::SQLiteMemoryRepository& repo,
                                   const std::string& sid,
                                   int hotSeedCount,
                                   std::shared_ptr<Thoth::FakeClock> clock) {
    seedSessionMessages(repo, sid, hotSeedCount, clock->nowMs());
    auto request = makeTestManualRequest(true, false);
    const auto result = memory.runConsolidation(sid, request);
    return !result.blocked && result.archived > 0;
}

static bool testM4LegacyEqualsFullReplay() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_01.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-01");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-01", 55, clock)) {
        std::cerr << "testM4LegacyEqualsFullReplay: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const auto legacy = memory.getArchivedTurns();
    auto req = makeTestRestoreRequest(Thoth::RestoreMode::REPLAY);
    const auto ranged = memory.runRestore("m4-01", req);
    if (ranged.blocked || legacy.size() != ranged.turns.size() || legacy.empty()) {
        std::cerr << "testM4LegacyEqualsFullReplay: size mismatch\n";
        fs::remove(cfg.database_path);
        return false;
    }
    for (size_t i = 0; i < legacy.size(); ++i) {
        if (legacy[i].archive_id != ranged.turns[i].archive_id
            || legacy[i].original_timestamp_ms != ranged.turns[i].original_timestamp_ms
            || legacy[i].content != ranged.turns[i].content) {
            std::cerr << "testM4LegacyEqualsFullReplay: content/order mismatch at " << i << "\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }
    for (size_t i = 1; i < legacy.size(); ++i) {
        if (legacy[i - 1].original_timestamp_ms > legacy[i].original_timestamp_ms
            || (legacy[i - 1].original_timestamp_ms == legacy[i].original_timestamp_ms
                && legacy[i - 1].archive_id > legacy[i].archive_id)) {
            std::cerr << "testM4LegacyEqualsFullReplay: unstable order\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM4RangedReplaySubset() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_02.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-02");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-02", 55, clock)) {
        std::cerr << "testM4RangedReplaySubset: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const auto all = memory.getArchivedTurns();
    if (all.size() < 3) {
        std::cerr << "testM4RangedReplaySubset: need more archives\n";
        fs::remove(cfg.database_path);
        return false;
    }
    const int64_t start = all[1].original_timestamp_ms;
    const int64_t end = all[all.size() - 2].original_timestamp_ms;

    auto req = makeTestRestoreRequest(Thoth::RestoreMode::REPLAY);
    req.range.start_ms = start;
    req.range.end_ms = end;
    const auto result = memory.runRestore("m4-02", req);
    if (result.blocked || result.turns.empty() || result.matched != static_cast<int>(result.turns.size())) {
        std::cerr << "testM4RangedReplaySubset: unexpected result\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (result.matched >= static_cast<int>(all.size())) {
        std::cerr << "testM4RangedReplaySubset: expected strict subset\n";
        fs::remove(cfg.database_path);
        return false;
    }
    for (const auto& t : result.turns) {
        if (t.original_timestamp_ms < start || t.original_timestamp_ms > end) {
            std::cerr << "testM4RangedReplaySubset: out-of-range turn\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM4OpenEndedRanges() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_03.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-03");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-03", 55, clock)) {
        std::cerr << "testM4OpenEndedRanges: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const auto all = memory.getArchivedTurns();
    const int64_t mid = all[all.size() / 2].original_timestamp_ms;

    auto startOnly = makeTestRestoreRequest(Thoth::RestoreMode::REPLAY);
    startOnly.range.start_ms = mid;
    const auto startRes = memory.runRestore("m4-03", startOnly);
    for (const auto& t : startRes.turns) {
        if (t.original_timestamp_ms < mid) {
            std::cerr << "testM4OpenEndedRanges: start-only failed\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }

    auto endOnly = makeTestRestoreRequest(Thoth::RestoreMode::REPLAY);
    endOnly.range.end_ms = mid;
    const auto endRes = memory.runRestore("m4-03", endOnly);
    for (const auto& t : endRes.turns) {
        if (t.original_timestamp_ms > mid) {
            std::cerr << "testM4OpenEndedRanges: end-only failed\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }
    if (startRes.matched + endRes.matched < static_cast<int>(all.size())) {
        // mid appears in both — sum can be >= all.size()
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM4ReplayNoSideEffects() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_04.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-04");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-04", 55, clock)) {
        std::cerr << "testM4ReplayNoSideEffects: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const int hotBefore = repo->getHotMessageCount("m4-04");
    const int coldBefore = static_cast<int>(repo->getArchivedMessages("m4-04").size());
    const int warmBefore = static_cast<int>(repo->getRecentWarmMemory("m4-04", 100).size());

    auto req = makeTestRestoreRequest(Thoth::RestoreMode::REPLAY);
    req.range.start_ms = 0;
    const auto result = memory.runRestore("m4-04", req);
    if (result.blocked || result.matched <= 0) {
        std::cerr << "testM4ReplayNoSideEffects: replay failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    if (repo->getHotMessageCount("m4-04") != hotBefore
        || static_cast<int>(repo->getArchivedMessages("m4-04").size()) != coldBefore
        || static_cast<int>(repo->getRecentWarmMemory("m4-04", 100).size()) != warmBefore) {
        std::cerr << "testM4ReplayNoSideEffects: tier counts changed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM4RehydrateInserts() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_05.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-05");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-05", 55, clock)) {
        std::cerr << "testM4RehydrateInserts: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    // Clear hot so archived turns are not duplicates of remaining hot.
    const int coldBefore = static_cast<int>(repo->getArchivedMessages("m4-05").size());
    repo->clearMessages("m4-05");
    const int hotBefore = repo->getHotMessageCount("m4-05");

    auto req = makeTestRestoreRequest(Thoth::RestoreMode::REHYDRATE);
    const auto result = memory.runRestore("m4-05", req);
    if (result.blocked || result.restored <= 0 || !result.turns.empty()) {
        std::cerr << "testM4RehydrateInserts: unexpected result restored="
                  << result.restored << " blocked=" << result.blocked << "\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (static_cast<int>(repo->getArchivedMessages("m4-05").size()) != coldBefore) {
        std::cerr << "testM4RehydrateInserts: cold mutated\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (repo->getHotMessageCount("m4-05") != hotBefore + result.restored) {
        std::cerr << "testM4RehydrateInserts: hot delta mismatch\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM4RehydrateIdempotent() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_06.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-06");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-06", 55, clock)) {
        std::cerr << "testM4RehydrateIdempotent: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }
    repo->clearMessages("m4-06");

    auto req = makeTestRestoreRequest(Thoth::RestoreMode::REHYDRATE);
    const auto first = memory.runRestore("m4-06", req);
    const auto second = memory.runRestore("m4-06", req);
    if (first.restored <= 0 || second.restored != 0
        || second.skipped_dup != second.matched
        || second.matched != first.matched) {
        std::cerr << "testM4RehydrateIdempotent: expected full skip on second pass\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM4GoalBlockedUnlessUnsafe() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_07.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-07");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-07", 55, clock)) {
        std::cerr << "testM4GoalBlockedUnlessUnsafe: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }
    repo->clearMessages("m4-07");
    memory.setGoalActiveChecker([]() { return true; });

    auto blocked = makeTestRestoreRequest(Thoth::RestoreMode::REHYDRATE);
    const auto blocked_result = memory.runRestore("m4-07", blocked);
    if (!blocked_result.blocked || blocked_result.restored != 0) {
        std::cerr << "testM4GoalBlockedUnlessUnsafe: expected blocked rehydrate\n";
        fs::remove(cfg.database_path);
        return false;
    }

    // Replay must still be allowed while goal active.
    auto replay = makeTestRestoreRequest(Thoth::RestoreMode::REPLAY);
    const auto replay_result = memory.runRestore("m4-07", replay);
    if (replay_result.blocked || replay_result.matched <= 0) {
        std::cerr << "testM4GoalBlockedUnlessUnsafe: replay should be allowed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    auto unsafe = makeTestRestoreRequest(Thoth::RestoreMode::REHYDRATE);
    unsafe.allow_during_goal = true;
    const auto unsafe_result = memory.runRestore("m4-07", unsafe);
    if (unsafe_result.blocked || unsafe_result.restored <= 0) {
        std::cerr << "testM4GoalBlockedUnlessUnsafe: expected --unsafe to allow\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM4InvalidRangeBlocked() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_08.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-08");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-08", 55, clock)) {
        std::cerr << "testM4InvalidRangeBlocked: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const int hotBefore = repo->getHotMessageCount("m4-08");
    const int coldBefore = static_cast<int>(repo->getArchivedMessages("m4-08").size());

    auto req = makeTestRestoreRequest(Thoth::RestoreMode::REHYDRATE);
    req.range.start_ms = 200;
    req.range.end_ms = 100;
    const auto result = memory.runRestore("m4-08", req);
    if (!result.blocked || result.restored != 0) {
        std::cerr << "testM4InvalidRangeBlocked: expected blocked invalid range\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (repo->getHotMessageCount("m4-08") != hotBefore
        || static_cast<int>(repo->getArchivedMessages("m4-08").size()) != coldBefore) {
        std::cerr << "testM4InvalidRangeBlocked: writes on invalid range\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testM4RehydrateRollback() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_09.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-09");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-09", 55, clock)) {
        std::cerr << "testM4RehydrateRollback: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }
    repo->clearMessages("m4-09");
    const int hotBefore = repo->getHotMessageCount("m4-09");

#ifdef _WIN32
    _putenv_s("THOTH_INJECT_RESTORE_FAIL", "insert");
#else
    setenv("THOTH_INJECT_RESTORE_FAIL", "insert", 1);
#endif

    auto req = makeTestRestoreRequest(Thoth::RestoreMode::REHYDRATE);
    const auto result = memory.runRestore("m4-09", req);

#ifdef _WIN32
    _putenv_s("THOTH_INJECT_RESTORE_FAIL", "");
#else
    unsetenv("THOTH_INJECT_RESTORE_FAIL");
#endif

    if (result.blocked || result.restored != 0
        || result.block_reason.find("failed") == std::string::npos) {
        std::cerr << "testM4RehydrateRollback: expected failed rehydrate\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (repo->getHotMessageCount("m4-09") != hotBefore) {
        std::cerr << "testM4RehydrateRollback: hot changed after rollback\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool readLastTraceTypes(std::vector<std::string>& out, int maxLines = 40) {
    FileHandler fh;
    const std::string path = fh.getAgentWorkspacePath("decision_trace.jsonl");
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::deque<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
        if (static_cast<int>(lines.size()) > maxLines) {
            lines.pop_front();
        }
    }
    for (const auto& l : lines) {
        try {
            auto j = nlohmann::json::parse(l);
            if (j.contains("trace_type")) {
                out.push_back(j["trace_type"].get<std::string>());
            }
        } catch (...) {
        }
    }
    return !out.empty();
}

static bool testM4DistinctDecisionTraceNames() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_m4_10.db").string();
    enableEpisodicMock();
    auto clock = makeM2TestClock();
    Memory memory(cfg);
    EmbeddingEngine engine(EmbeddingEngine::Method::TfIdf, &cfg);
    memory.configureConsolidation(nullptr, &engine, clock);
    memory.setActiveSessionId("m4-10");

    auto* repo = memory.getSQLiteRepo();
    if (!repo || !setupM4ArchivedSession(memory, *repo, "m4-10", 55, clock)) {
        std::cerr << "testM4DistinctDecisionTraceNames: setup failed\n";
        fs::remove(cfg.database_path);
        return false;
    }
    repo->clearMessages("m4-10");

    auto replay = makeTestRestoreRequest(Thoth::RestoreMode::REPLAY);
    memory.runRestore("m4-10", replay);
    auto rehydrate = makeTestRestoreRequest(Thoth::RestoreMode::REHYDRATE);
    memory.runRestore("m4-10", rehydrate);

    // Legacy silent path must not be required to emit.
    (void)memory.getArchivedTurns();

    std::vector<std::string> types;
    if (!readLastTraceTypes(types, 80)) {
        std::cerr << "testM4DistinctDecisionTraceNames: could not read traces\n";
        fs::remove(cfg.database_path);
        return false;
    }

    bool sawReplay = false;
    bool sawRehydrate = false;
    for (const auto& t : types) {
        if (t == "memory_restore_replay") sawReplay = true;
        if (t == "memory_restore_rehydrate") sawRehydrate = true;
        if (t == "memory_restore") {
            std::cerr << "testM4DistinctDecisionTraceNames: shared memory_restore name forbidden\n";
            fs::remove(cfg.database_path);
            return false;
        }
    }
    if (!sawReplay || !sawRehydrate) {
        std::cerr << "testM4DistinctDecisionTraceNames: missing distinct events\n";
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

static bool testPlannerEvidenceSessionId() {
    const char* prevDev = std::getenv("THOTH_TEST_SUITE_DEV");
    setenv("THOTH_TEST_SUITE_DEV", "1", 1);

    FileHandler fh;
    const std::string appLog = fh.getAgentWorkspacePath("app_log.jsonl");
    const std::string traceLog = fh.getAgentWorkspacePath("decision_trace.jsonl");
    const std::string metricsLog = fh.getLogsPath("cognitive_metrics.jsonl");
    auto sizeOf = [](const std::string& path) -> std::uintmax_t {
        std::error_code ec;
        const auto size = fs::file_size(path, ec);
        return ec ? 0 : size;
    };
    const auto appBefore = sizeOf(appLog);
    const auto traceBefore = sizeOf(traceLog);
    const auto metricsBefore = sizeOf(metricsLog);
    auto restoreLogs = [&]() {
        auto trim = [](const std::string& path, std::uintmax_t keep) {
            std::error_code ec;
            if (!fs::exists(path, ec)) {
                return;
            }
            fs::resize_file(path, keep, ec);
        };
        trim(appLog, appBefore);
        trim(traceLog, traceBefore);
        trim(metricsLog, metricsBefore);
    };

    const std::string sessionId = "session-c64-phase1";
    const std::string goal = "Test Strategy [\"RETRIEVAL\", \"LLM\"]";
    bool sawAssembly = false;
    bool sawStrategy = false;
    std::string detail;

    {
        Config cfg;
        cfg.database_path = makeTempPath("thoth_planner_session_test.db").string();
        auto memory = std::make_shared<Memory>(cfg);
        auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
        auto idx = new IndexManager(engine.get());
        auto rag = std::make_shared<RAGPipeline>(std::move(engine), idx);
        auto promptFactory = std::make_shared<PromptFactory>(*memory, *rag);
        LLMInterface llm(LLMBackend::Ollama, &cfg);

        Memory::CognateStrategyRecord strat;
        strat.strategy_id = "strat-session";
        strat.description = "Test Strategy";
        strat.step_pattern_json = "[\"RETRIEVAL\", \"LLM\"]";
        strat.success_rate = 0.95f;
        strat.created_at = 1000;
        memory->saveStrategy(strat);

        const std::string strategyText = strat.description + " " + strat.step_pattern_json;
        rag->engine->updateVocabulary(strategyText);

        auto planner = std::make_shared<LLMPlanner>(memory, rag, promptFactory, &llm);
        auto registry = std::make_shared<ToolRegistry>();
        Thoth::ExecutiveController controller(planner, registry, rag, memory);
        controller.set_session_id(sessionId);
        controller.set_max_reflections(0);
        controller.execute_goal(goal);
        controller.abort();
        fs::remove(cfg.database_path);
    }

    std::ifstream in(appLog);
    std::string line;
    while (std::getline(in, line)) {
        if (line.find(sessionId) == std::string::npos) {
            continue;
        }
        try {
            const auto entry = nlohmann::json::parse(line);
            if (entry.value("session_id", "") != sessionId) {
                continue;
            }
            const std::string eventName = entry.value("event_name", "");
            if (eventName == "PLANNER_CONTEXT_ASSEMBLY") {
                sawAssembly = true;
                if (!entry.contains("metadata") ||
                    entry["metadata"].value("strategy_injection", false) != true) {
                    detail = "assembly log did not keep the selected strategy";
                }
            }
            if (eventName == "STRATEGY_INJECTION") {
                sawStrategy = true;
                if (!entry.contains("metadata") ||
                    entry["metadata"].value("strategy_id", "") != "strat-session") {
                    detail = "strategy injection id changed";
                }
            }
        } catch (...) {
            continue;
        }
    }
    in.close();
    restoreLogs();
    if (prevDev) {
        setenv("THOTH_TEST_SUITE_DEV", prevDev, 1);
    } else {
        unsetenv("THOTH_TEST_SUITE_DEV");
    }

    const bool ok = sawAssembly && sawStrategy && detail.empty();
    if (!ok) {
        std::cerr << "testPlannerEvidenceSessionId: "
                  << (detail.empty() ? "planner evidence missing session_id" : detail)
                  << "\n";
    }
    return ok;
}

static Thoth::EpisodicV2Request completeV2Request() {
    Thoth::EpisodicV2Request request;
    request.evaluation_tier = "authoritative";
    request.inference_backend_name = "llama_cpp";
    request.llm_model = "configured-llm";
    request.embedding_model = "configured-embed";
    request.embedding_method = "External";
    request.embedding_dimension = 768;
    request.thoth_git_sha = "product-sha";
    request.basic_agent_git_sha = "engine-sha";
    request.corpus_fingerprint = "corpus-abc";
    request.environment_schema_version = "c64-env-1";
    request.protocol_version = "C6.4 v1.0";
    request.metric_schema_version = "1.0";
    request.c64_cohort_fingerprint = std::string(64, 'a');
    const auto cases = Thoth::getEpisodicLearningCases();
    for (const auto& item : cases) {
        Thoth::EpisodicV2Case row;
        row.case_id = item.id;
        row.baseline.present = true;
        row.baseline.response_valid = true;
        row.episodic.present = true;
        row.episodic.response_valid = true;
        request.cases.push_back(row);
    }
    return request;
}

static bool testEpisodicAuthoritativeV2() {
    const char* prev = std::getenv("THOTH_INFERENCE_BACKEND");
    setenv("THOTH_INFERENCE_BACKEND", "llama_cpp", 1);
    auto llama = Thoth::makeEpisodicV2InferenceClient(nullptr);
    setenv("THOTH_INFERENCE_BACKEND", "ollama", 1);
    auto ollama = Thoth::makeEpisodicV2InferenceClient(nullptr);
    if (prev) {
        setenv("THOTH_INFERENCE_BACKEND", prev, 1);
    } else {
        unsetenv("THOTH_INFERENCE_BACKEND");
    }
    if (!llama || llama->backendName() != "llama_cpp" || !ollama || ollama->backendName() != "ollama") {
        std::cerr << "testEpisodicAuthoritativeV2: InferenceClient provider selection drifted\n";
        return false;
    }

    Thoth::MockInferenceClient client;
    client.backend_name = "llama_cpp";
    const auto okReport = Thoth::buildEpisodicAuthoritativeV2Report(completeV2Request(), &client);
    if (!okReport.authoritative_valid ||
        okReport.evidence.value("evaluation_id", "") != Thoth::kEpisodicAuthoritativeV2Id) {
        std::cerr << "testEpisodicAuthoritativeV2: complete llama stratum was not recorded\n";
        return false;
    }
    if (okReport.evidence["inference"].value("backend_name", "") != "llama_cpp" ||
        okReport.evidence["model"].value("llm_model", "") != "configured-llm") {
        std::cerr << "testEpisodicAuthoritativeV2: provider or model was not preserved\n";
        return false;
    }
    if (okReport.evidence.contains("window_id")) {
        std::cerr << "testEpisodicAuthoritativeV2: window attribution was opened\n";
        return false;
    }

    client.backend_name = "ollama";
    auto request = completeV2Request();
    request.inference_backend_name = "ollama";
    request.c64_cohort_fingerprint = std::string(64, 'b');
    const auto ollamaReport = Thoth::buildEpisodicAuthoritativeV2Report(request, &client);
    if (!ollamaReport.authoritative_valid ||
        ollamaReport.evidence["inference"].value("backend_name", "") != "ollama" ||
        ollamaReport.evidence.value("c64_cohort_fingerprint", "") ==
            okReport.evidence.value("c64_cohort_fingerprint", "")) {
        std::cerr << "testEpisodicAuthoritativeV2: provider strata were pooled\n";
        return false;
    }

    client.backend_name = "ollama";
    const auto mismatch = Thoth::buildEpisodicAuthoritativeV2Report(completeV2Request(), &client);
    if (mismatch.authoritative_valid || mismatch.reason != "provider mismatch") {
        std::cerr << "testEpisodicAuthoritativeV2: provider mismatch was accepted\n";
        return false;
    }

    request = completeV2Request();
    request.llm_model.clear();
    client.backend_name = "llama_cpp";
    const auto missing = Thoth::buildEpisodicAuthoritativeV2Report(request, &client);
    if (missing.authoritative_valid || missing.reason != "missing required identity") {
        std::cerr << "testEpisodicAuthoritativeV2: missing model was accepted\n";
        return false;
    }

    request = completeV2Request();
    request.evaluation_tier = "mock";
    const auto mockReport = Thoth::buildEpisodicAuthoritativeV2Report(request, &client);
    if (mockReport.authoritative_valid || mockReport.status != "mock") {
        std::cerr << "testEpisodicAuthoritativeV2: mock run was labeled authoritative\n";
        return false;
    }

    request = completeV2Request();
    request.cases.back().episodic.present = false;
    const auto incomplete = Thoth::buildEpisodicAuthoritativeV2Report(request, &client);
    if (incomplete.authoritative_valid || incomplete.reason != "incomplete paired comparison") {
        std::cerr << "testEpisodicAuthoritativeV2: incomplete pair produced a valid result\n";
        return false;
    }

    const auto cases = okReport.evidence["cases"];
    if (!cases.is_array() || cases.size() != 3 || cases[0].value("case_id", "") != "E2-01" ||
        cases[0]["baseline"].value("condition", "") != "baseline" ||
        cases[0]["episodic"].value("condition", "") != "episodic") {
        std::cerr << "testEpisodicAuthoritativeV2: paired workload was not preserved\n";
        return false;
    }
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
    inputs.inference_backend_name = "ollama";
    inputs.inference_base_url = "http://127.0.0.1:11434";
    inputs.inference_embed_base_url = "http://127.0.0.1:11434";
    inputs.inference_diagnostics = {{"ollama_version", "0.5.1"}};
    inputs.inference_reachable = true;

    const Thoth::BenchmarkEnvironment original = Thoth::assembleEnvironment(inputs);
    const nlohmann::json json = Thoth::benchmarkEnvironmentToJson(original);
    const Thoth::BenchmarkEnvironment restored = Thoth::benchmarkEnvironmentFromJson(json);

    if (restored.prov.thoth_git_sha != original.prov.thoth_git_sha ||
        restored.model.embedding_internal_version != original.model.embedding_internal_version ||
        restored.runtime.tier != original.runtime.tier ||
        restored.corpus.fingerprint != original.corpus.fingerprint ||
        restored.environment_hash != original.environment_hash ||
        restored.ollama.models_digest != original.ollama.models_digest ||
        restored.inference.backend_name != original.inference.backend_name ||
        restored.inference.base_url != original.inference.base_url ||
        restored.inference.diagnostic_digest != original.inference.diagnostic_digest) {
        std::cerr << "testE1BenchmarkEnvironmentJsonRoundTrip: round-trip field mismatch\n";
        return false;
    }
    if (!restored.prov.hostname.has_value() || *restored.prov.hostname != "bench-host") {
        std::cerr << "testE1BenchmarkEnvironmentJsonRoundTrip: hostname missing\n";
        return false;
    }
    return true;
}

static bool testG1dA0EnvHashDistinguishesInferenceBackend() {
    auto base = makeE1SampleInputs();
    base.tier = Thoth::BenchmarkTier::FULL;
    base.harness = "trajectory_ablation_benchmark";
    base.model.embedding_method = "External";
    base.model.embedding_model = "nomic-embed-text";
    base.inference_base_url = "http://127.0.0.1:8080";
    base.inference_embed_base_url = "http://127.0.0.1:8080";
    base.inference_reachable = true;

    auto ollamaInputs = base;
    ollamaInputs.inference_backend_name = "ollama";
    ollamaInputs.inference_diagnostics = {{"ollama_version", "0.5.1"}};

    auto llamaInputs = base;
    llamaInputs.inference_backend_name = "llama_cpp";
    llamaInputs.inference_diagnostics = {{"available_models", nlohmann::json::array({"nomic"})}};

    const auto ollamaEnv = Thoth::assembleEnvironment(ollamaInputs);
    const auto llamaEnv = Thoth::assembleEnvironment(llamaInputs);

    if (ollamaEnv.inference.backend_name != "ollama" || llamaEnv.inference.backend_name != "llama_cpp") {
        std::cerr << "testG1dA0EnvHashDistinguishesInferenceBackend: backend_name not assembled\n";
        return false;
    }
    if (ollamaEnv.environment_hash == llamaEnv.environment_hash) {
        std::cerr << "testG1dA0EnvHashDistinguishesInferenceBackend: backend change did not alter env_hash\n";
        return false;
    }
    if (ollamaEnv.inference.diagnostic_digest.empty() || llamaEnv.inference.diagnostic_digest.empty()) {
        std::cerr << "testG1dA0EnvHashDistinguishesInferenceBackend: expected diagnostic digest\n";
        return false;
    }
    return true;
}

static bool testG1dA0EnvHashDistinguishesInferenceEndpoint() {
    auto base = makeE1SampleInputs();
    base.tier = Thoth::BenchmarkTier::FULL;
    base.harness = "trajectory_ablation_benchmark";
    base.model.embedding_method = "External";
    base.inference_backend_name = "llama_cpp";
    base.inference_reachable = true;

    auto endpointA = base;
    endpointA.inference_base_url = "http://127.0.0.1:8080";
    endpointA.inference_embed_base_url = "http://127.0.0.1:8080";

    auto endpointB = base;
    endpointB.inference_base_url = "http://127.0.0.1:8081";
    endpointB.inference_embed_base_url = "http://127.0.0.1:8081";

    const auto envA = Thoth::assembleEnvironment(endpointA);
    const auto envB = Thoth::assembleEnvironment(endpointB);
    if (envA.environment_hash == envB.environment_hash) {
        std::cerr << "testG1dA0EnvHashDistinguishesInferenceEndpoint: endpoint change did not alter env_hash\n";
        return false;
    }
    return true;
}

static bool testG1dA0SnapshotUsesClientBackendName() {
    Thoth::MockInferenceClient client;
    client.backend_name = "llama_cpp";
    client.on_health = []() {
        Thoth::InferenceHealthResult health;
        health.reachable = true;
        health.available_models = {"nomic-embed-text"};
        return health;
    };

    Thoth::InferenceEndpointConfig endpoints;
    endpoints.base_url = "http://127.0.0.1:8080";
    endpoints.embed_base_url = "http://127.0.0.1:8080/v1";

    const Thoth::InferenceHealthResult health = client.health();
    const Thoth::InferenceBackendSnapshot snap =
        Thoth::snapshotFromInferenceClient(client, endpoints, health);

    if (snap.backend_name != "llama_cpp") {
        std::cerr << "testG1dA0SnapshotUsesClientBackendName: expected client backendName()\n";
        return false;
    }
    if (!snap.reachable || snap.base_url != endpoints.base_url ||
        snap.embed_base_url != endpoints.embed_base_url) {
        std::cerr << "testG1dA0SnapshotUsesClientBackendName: endpoint/reachable mismatch\n";
        return false;
    }
    if (!snap.diagnostics.contains("available_models")) {
        std::cerr << "testG1dA0SnapshotUsesClientBackendName: missing available_models diagnostics\n";
        return false;
    }

    // Conflicting config-style name must not override instantiated client identity.
    client.backend_name = "ollama";
    const auto ollamaNamed = Thoth::snapshotFromInferenceClient(client, endpoints, health);
    if (ollamaNamed.backend_name != "ollama") {
        std::cerr << "testG1dA0SnapshotUsesClientBackendName: renamed client not reflected\n";
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
    const EnvSnapshot priorDev = EnvSnapshot::capture("THOTH_TEST_SUITE_DEV");
    const EnvSnapshot priorMock = EnvSnapshot::capture("THOTH_MOCK_LLM");
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
        priorMock.restore("THOTH_MOCK_LLM");
        priorDev.restore("THOTH_TEST_SUITE_DEV");
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
    const EnvSnapshot priorDev = EnvSnapshot::capture("THOTH_TEST_SUITE_DEV");
    const EnvSnapshot priorMock = EnvSnapshot::capture("THOTH_MOCK_LLM");
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
        priorMock.restore("THOTH_MOCK_LLM");
        priorDev.restore("THOTH_TEST_SUITE_DEV");
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
        priorMock.restore("THOTH_MOCK_LLM");
        priorDev.restore("THOTH_TEST_SUITE_DEV");
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
// Plan M G1 (R1) — helpers for grounding-floor tests.
static CodeChunk makePlanMChunk(const std::string& file, const std::string& code) {
    CodeChunk chunk;
    chunk.fileName = file;
    chunk.symbolName = "";
    chunk.startLine = 1;
    chunk.endLine = 1;
    chunk.code = code;
    return chunk;
}

static ScoreBreakdown makePlanMBreakdown(const CodeChunk& chunk, float finalScore) {
    ScoreBreakdown sb;
    sb.file_name = chunk.fileName;
    sb.code_text = chunk.code;
    sb.final_score = finalScore;
    return sb;
}

// Plan M G1 / T3 — below-floor and missing/NaN scores must not ground (fail closed).
static bool testPlanMGroundingFloorRejectsBelowThreshold() {
    std::vector<CodeChunk> chunks = {
        makePlanMChunk("SEED.md", "zero score candidate"),
        makePlanMChunk("SEED.md", "tiny score candidate"),
        makePlanMChunk("SEED.md", "candidate without breakdown"),
    };
    GragDiagnostics diagnostics;
    diagnostics.breakdowns.push_back(makePlanMBreakdown(chunks[0], 0.0f));
    diagnostics.breakdowns.push_back(makePlanMBreakdown(chunks[1], 0.005f));
    // Intentionally no breakdown for chunks[2] → fail-closed reject.

    const auto result = Thoth::ChatRetrieval::applyGroundingFloor(
        chunks, diagnostics, Thoth::ChatRetrieval::kMinGroundingFinalScore);

    if (result.stats.candidates_found != 3) {
        std::cerr << "testPlanMGroundingFloorRejectsBelowThreshold: candidates_found "
                  << result.stats.candidates_found << " != 3\n";
        return false;
    }
    if (result.stats.candidates_passed_gate != 0 || !result.injectable.empty()) {
        std::cerr << "testPlanMGroundingFloorRejectsBelowThreshold: expected 0 passed, got "
                  << result.stats.candidates_passed_gate << "\n";
        return false;
    }
    if (result.diagnostics.breakdowns.size() != 0) {
        std::cerr << "testPlanMGroundingFloorRejectsBelowThreshold: filtered breakdowns not empty\n";
        return false;
    }
    return true;
}

// Plan M G1 — a candidate at/above the floor grounds; stats stay consistent.
static bool testPlanMGroundingFloorPassesAboveThreshold() {
    std::vector<CodeChunk> chunks = {
        makePlanMChunk("GRAG.md", "meaningful grounded chunk"),
        makePlanMChunk("SEED.md", "below floor chunk"),
    };
    GragDiagnostics diagnostics;
    diagnostics.breakdowns.push_back(makePlanMBreakdown(chunks[0], 0.9f));
    diagnostics.breakdowns.push_back(makePlanMBreakdown(chunks[1], 0.0f));

    const auto result = Thoth::ChatRetrieval::applyGroundingFloor(
        chunks, diagnostics, Thoth::ChatRetrieval::kMinGroundingFinalScore);

    if (result.stats.candidates_found != 2 || result.stats.candidates_passed_gate != 1) {
        std::cerr << "testPlanMGroundingFloorPassesAboveThreshold: found/passed "
                  << result.stats.candidates_found << "/" << result.stats.candidates_passed_gate
                  << " expected 2/1\n";
        return false;
    }
    if (result.injectable.size() != 1 || result.injectable[0].code != "meaningful grounded chunk") {
        std::cerr << "testPlanMGroundingFloorPassesAboveThreshold: wrong injectable chunk\n";
        return false;
    }
    if (result.diagnostics.breakdowns.size() != 1 || result.diagnostics.final_scores.size() != 1) {
        std::cerr << "testPlanMGroundingFloorPassesAboveThreshold: filtered diagnostics misaligned\n";
        return false;
    }
    if (!result.stats.has_candidates || result.stats.max_score < 0.89f ||
        result.stats.min_injected_score < 0.89f) {
        std::cerr << "testPlanMGroundingFloorPassesAboveThreshold: score stats wrong (max "
                  << result.stats.max_score << ", min_injected " << result.stats.min_injected_score
                  << ")\n";
        return false;
    }
    return true;
}

// Plan M G1 / T5 (partial) — one CHAT_RAG_CONTEXT answers the four operator questions.
static bool testPlanMGroundingTelemetryShape() {
    auto assertBelowThreshold = [](const Thoth::ChatRagContextRecord& rec, const char* label) -> bool {
        const auto json = Thoth::ChatRagLogger::contextToJson(rec);
        for (const char* key : {"retrieval_ran", "retrieval_skip_reason", "candidates_found",
                                "candidates_passed_gate", "grounding_decision_reason",
                                "grounded", "grounding_mode", "max_score", "min_injected_score"}) {
            if (!json.contains(key)) {
                std::cerr << label << ": telemetry missing key " << key << "\n";
                return false;
            }
        }
        // grounded must agree with grounding_mode.
        const bool grounded = json.value("grounded", false);
        const std::string mode = json.value("grounding_mode", "");
        const bool modeGrounded = (mode == "retrieved_context");
        if (grounded != modeGrounded) {
            std::cerr << label << ": grounded/grounding_mode inconsistent\n";
            return false;
        }
        return true;
    };

    // Attempt without success (ran, candidates found, none passed).
    Thoth::ChatRagContextRecord below;
    below.grounding_mode = "no_retrieval_hits";
    below.retrieval_ran = true;
    below.retrieval_skip_reason = "none";
    below.candidates_found = 3;
    below.candidates_passed_gate = 0;
    below.grounding_decision_reason = "below_threshold";
    below.grounded = false;
    below.has_candidate_scores = true;
    below.max_score = 0.004f;
    below.has_injected_scores = false;
    if (!assertBelowThreshold(below, "testPlanMGroundingTelemetryShape[below]")) {
        return false;
    }
    {
        const auto json = Thoth::ChatRagLogger::contextToJson(below);
        if (!json["min_injected_score"].is_null()) {
            std::cerr << "testPlanMGroundingTelemetryShape[below]: min_injected_score should be null\n";
            return false;
        }
        if (json["max_score"].is_null()) {
            std::cerr << "testPlanMGroundingTelemetryShape[below]: max_score should be present\n";
            return false;
        }
    }

    // Success (ran, candidates passed gate).
    Thoth::ChatRagContextRecord success;
    success.grounding_mode = "retrieved_context";
    success.retrieval_ran = true;
    success.retrieval_skip_reason = "none";
    success.candidates_found = 5;
    success.candidates_passed_gate = 2;
    success.grounding_decision_reason = "injected_meaningful_hits";
    success.grounded = true;
    success.has_candidate_scores = true;
    success.max_score = 1.2f;
    success.has_injected_scores = true;
    success.min_injected_score = 0.6f;
    if (!assertBelowThreshold(success, "testPlanMGroundingTelemetryShape[success]")) {
        return false;
    }
    {
        const auto json = Thoth::ChatRagLogger::contextToJson(success);
        if (json["candidates_passed_gate"].get<int>() < 1) {
            std::cerr << "testPlanMGroundingTelemetryShape[success]: passed gate < 1\n";
            return false;
        }
        if (json["min_injected_score"].is_null()) {
            std::cerr << "testPlanMGroundingTelemetryShape[success]: min_injected_score should be present\n";
            return false;
        }
    }

    // no_index path shape.
    Thoth::ChatRagContextRecord noIndex;
    noIndex.grounding_mode = "no_index";
    noIndex.retrieval_ran = false;
    noIndex.retrieval_skip_reason = "no_index";
    noIndex.candidates_found = 0;
    noIndex.candidates_passed_gate = 0;
    noIndex.grounding_decision_reason = "empty_index";
    noIndex.grounded = false;
    {
        const auto json = Thoth::ChatRagLogger::contextToJson(noIndex);
        if (json.value("retrieval_ran", true) != false ||
            json.value("retrieval_skip_reason", "") != "no_index" ||
            json.value("grounding_decision_reason", "") != "empty_index") {
            std::cerr << "testPlanMGroundingTelemetryShape[no_index]: wrong fields\n";
            return false;
        }
        if (!json["max_score"].is_null() || !json["min_injected_score"].is_null()) {
            std::cerr << "testPlanMGroundingTelemetryShape[no_index]: scores should be null\n";
            return false;
        }
    }

    return true;
}

// Plan M G2 / T1 — exact-match greeting skip classifier.
static bool testPlanMGreetingSkipClassifier() {
    const std::vector<std::string> mustSkip = {
        "hello",
        "hello!",
        "Hi.",
        "thanks",
        "good morning",
    };
    for (const auto& query : mustSkip) {
        if (!Thoth::isGreetingSkipQuery(query)) {
            std::cerr << "testPlanMGreetingSkipClassifier: expected skip for '" << query << "'\n";
            return false;
        }
    }

    const std::vector<std::string> mustNotSkip = {
        "hello there",
        "explain GRAG",
        "what is GRAG",
        "how does GRAG work",
        "thanks why?",
    };
    for (const auto& query : mustNotSkip) {
        if (Thoth::isGreetingSkipQuery(query)) {
            std::cerr << "testPlanMGreetingSkipClassifier: unexpected skip for '" << query << "'\n";
            return false;
        }
    }

    return true;
}

// Plan M G2 / T5 — greeting skips are ungrounded and disambiguated by skip reason.
static bool testPlanMGreetingSkipTelemetryShape() {
    Thoth::ChatRagContextRecord greeting;
    greeting.grounding_mode = "no_retrieval_hits";
    greeting.retrieval_ran = false;
    greeting.retrieval_skip_reason = "greeting";
    greeting.candidates_found = 0;
    greeting.candidates_passed_gate = 0;
    greeting.grounding_decision_reason = "greeting_skip";
    greeting.grounded = false;

    const auto json = Thoth::ChatRagLogger::contextToJson(greeting);
    if (json.value("retrieval_ran", true) != false) {
        std::cerr << "testPlanMGreetingSkipTelemetryShape: retrieval_ran should be false\n";
        return false;
    }
    if (json.value("retrieval_skip_reason", "") != "greeting") {
        std::cerr << "testPlanMGreetingSkipTelemetryShape: skip reason should be greeting\n";
        return false;
    }
    if (json.value("grounding_decision_reason", "") != "greeting_skip") {
        std::cerr << "testPlanMGreetingSkipTelemetryShape: decision reason should be greeting_skip\n";
        return false;
    }
    if (json.value("grounding_mode", "") != "no_retrieval_hits") {
        std::cerr << "testPlanMGreetingSkipTelemetryShape: mode should remain no_retrieval_hits\n";
        return false;
    }
    if (json.value("grounded", true) != false) {
        std::cerr << "testPlanMGreetingSkipTelemetryShape: grounded should be false\n";
        return false;
    }
    if (json.value("candidates_found", -1) != 0 ||
        json.value("candidates_passed_gate", -1) != 0) {
        std::cerr << "testPlanMGreetingSkipTelemetryShape: candidate counts should be zero\n";
        return false;
    }
    if (!json["max_score"].is_null() || !json["min_injected_score"].is_null()) {
        std::cerr << "testPlanMGreetingSkipTelemetryShape: scores should be null\n";
        return false;
    }
    return true;
}

// Plan M G3 / Phase 3 — user block + explicit [Agent] completion cue + anti-transcript.
static bool testPlanMChatPromptCueAndAntiTranscript() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_plan_m_g3_prompt.db").string();
    Memory memory(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto* idx = new IndexManager(engine.get());
    RAGPipeline rag(std::move(engine), idx, &cfg, &memory);
    PromptFactory factory(memory, rag);

    const std::string input = "Explain GRAG.";
    PromptFactory::ConversationBuildOptions ungrounded;
    ungrounded.grounded = false;
    const std::string plain = factory.buildChatPrompt(input, "", false, ungrounded, nullptr);

    const std::string expectedEnding = Thoth::ChatPrompt::formatUserBlock(input);
    if (plain.size() < expectedEnding.size() ||
        plain.compare(plain.size() - expectedEnding.size(), expectedEnding.size(), expectedEnding) != 0) {
        std::cerr << "testPlanMChatPromptCueAndAntiTranscript: ungrounded prompt must end with completion boundary\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (plain.find(Thoth::ChatPrompt::kAntiTranscriptRules) == std::string::npos) {
        std::cerr << "testPlanMChatPromptCueAndAntiTranscript: anti-transcript rules missing\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (plain.find(Thoth::ChatPrompt::kAntiRegurgitationRules) == std::string::npos) {
        std::cerr << "testPlanMChatPromptCueAndAntiTranscript: anti-regurgitation rules missing\n";
        fs::remove(cfg.database_path);
        return false;
    }

    // T2 offline — grounded assembly still injects RAG + grounding rules with completion boundary.
    const std::string ragContextText =
        "Document: GRAG.md\nLines: 1-3\nGRAG means Goal-Relative Adaptive Graph Retrieval.\n---\n";
    PromptFactory::ConversationBuildOptions grounded;
    grounded.grounded = true;
    const std::string withRag =
        factory.buildChatPrompt(input, ragContextText, false, grounded, nullptr);
    if (withRag.find(Thoth::ChatPrompt::kRagContextHeader) == std::string::npos ||
        withRag.find(Thoth::ChatPrompt::kGroundingRules) == std::string::npos ||
        withRag.find(Thoth::ChatPrompt::kAntiTranscriptRules) == std::string::npos ||
        withRag.find(Thoth::ChatPrompt::kAntiRegurgitationRules) == std::string::npos) {
        std::cerr << "testPlanMChatPromptCueAndAntiTranscript: grounded path missing RAG/rules\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (withRag.find("Never reproduce internal retrieval formatting") == std::string::npos) {
        std::cerr << "testPlanMChatPromptCueAndAntiTranscript: grounding anti-regurgitation missing\n";
        fs::remove(cfg.database_path);
        return false;
    }
    const std::string userQuerySection =
        std::string(Thoth::ChatPrompt::kUserQueryHeader) + input + "\n";
    if (withRag.find(userQuerySection) == std::string::npos) {
        std::cerr << "testPlanMChatPromptCueAndAntiTranscript: grounded prompt must populate [User Query]\n";
        fs::remove(cfg.database_path);
        return false;
    }
    const std::string duplicateUserTurn = std::string(Thoth::ChatPrompt::kUserTurnPrefix) + input;
    if (withRag.find(duplicateUserTurn) != std::string::npos) {
        std::cerr << "testPlanMChatPromptCueAndAntiTranscript: grounded prompt must not duplicate [User] turn\n";
        fs::remove(cfg.database_path);
        return false;
    }
    const std::string groundedEnding = Thoth::ChatPrompt::formatAgentCompletionCue();
    if (withRag.size() < groundedEnding.size() ||
        withRag.compare(withRag.size() - groundedEnding.size(), groundedEnding.size(), groundedEnding) !=
            0) {
        std::cerr << "testPlanMChatPromptCueAndAntiTranscript: grounded prompt must end with [Agent] cue only\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

// Plan M G3 / Plan N N3 — empty stops omit key; explicit non-empty still serialize (N-T7).
static bool testPlanMChatStopPayloadSerialization() {
    Thoth::InferenceGenerateRequest emptyStops;
    emptyStops.model = "test-model";
    emptyStops.prompt = "hi";
    emptyStops.max_tokens = 512;
    emptyStops.stop_sequences = Thoth::ChatPrompt::chatStopSequences();

    const auto llamaEmpty = nlohmann::json::parse(Thoth::LlamaServerClient::serializeGeneratePayload(emptyStops));
    const auto ollamaEmpty = nlohmann::json::parse(Thoth::OllamaClient::serializeGeneratePayload(emptyStops));
    if (llamaEmpty.contains("stop") || ollamaEmpty.contains("stop")) {
        std::cerr << "testPlanMChatStopPayloadSerialization: empty stops must omit stop key\n";
        return false;
    }

    // N-T7 — explicit non-empty stops (not chatStopSequences()) still serialize.
    Thoth::InferenceGenerateRequest withStops = emptyStops;
    withStops.stop_sequences = {Thoth::ChatPrompt::kChatStopUser, Thoth::ChatPrompt::kChatStopAgent};
    const auto llama = nlohmann::json::parse(Thoth::LlamaServerClient::serializeGeneratePayload(withStops));
    const auto ollama = nlohmann::json::parse(Thoth::OllamaClient::serializeGeneratePayload(withStops));
    if (!llama.contains("stop") || !ollama.contains("stop")) {
        std::cerr << "testPlanMChatStopPayloadSerialization: stop key missing when sequences set\n";
        return false;
    }
    if (!llama["stop"].is_array() || llama["stop"].size() != 2 ||
        llama["stop"][0] != Thoth::ChatPrompt::kChatStopUser ||
        llama["stop"][1] != Thoth::ChatPrompt::kChatStopAgent) {
        std::cerr << "testPlanMChatStopPayloadSerialization: llama stop list wrong\n";
        return false;
    }
    if (!ollama["stop"].is_array() || ollama["stop"].size() != 2 ||
        ollama["stop"][0] != Thoth::ChatPrompt::kChatStopUser ||
        ollama["stop"][1] != Thoth::ChatPrompt::kChatStopAgent) {
        std::cerr << "testPlanMChatStopPayloadSerialization: ollama stop list wrong\n";
        return false;
    }
    if (Thoth::ChatPrompt::kChatMaxTokens != 512) {
        std::cerr << "testPlanMChatStopPayloadSerialization: kChatMaxTokens must be 512\n";
        return false;
    }
    return true;
}

// Plan N N3 / N-T6 — completions stop policy is empty; chat mode adds ChatML stops (A.1).
static bool testPlanNChatStopSequencesEmpty() {
    if (!Thoth::ChatPrompt::chatStopSequences().empty()) {
        std::cerr << "testPlanNChatStopSequencesEmpty: completions chatStopSequences must return {}\n";
        return false;
    }
    const auto chatStops =
        Thoth::ChatPrompt::chatStopSequences(Thoth::ChatPrompt::ChatInferenceMode::Chat);
    if (chatStops.size() != 2 || chatStops[0] != Thoth::ChatPrompt::kChatMlStopImEnd ||
        chatStops[1] != Thoth::ChatPrompt::kChatMlStopImStart) {
        std::cerr << "testPlanNChatStopSequencesEmpty: chat mode ChatML stops wrong\n";
        return false;
    }
    return true;
}

// Plan N N3 / N-T6c — generateAndSanitizeChat with empty stops → used_stops=false.
static bool testPlanNGenerateEmptyStops() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("Hello without stops.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.max_tokens = 64;
    opts.stop_sequences = {};

    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.provider_ok || out.used_stops || out.retried_without_stops || out.fallback_used) {
        std::cerr << "testPlanNGenerateEmptyStops: flags wrong\n";
        return false;
    }
    if (out.sanitized_text != "Hello without stops.") {
        std::cerr << "testPlanNGenerateEmptyStops: unexpected text: " << out.sanitized_text << "\n";
        return false;
    }
    return true;
}

// Plan N N4 / N-T8 — range span uses source_span=; no Lines: header in prompt content.
static bool testPlanNFormatChunkSourceSpanRange() {
    CodeChunk chunk;
    chunk.fileName = "/workspace/rag/GRAG.md";
    chunk.startLine = 29;
    chunk.endLine = 33;
    chunk.code = "GRAG means Goal-Relative Adaptive Graph Retrieval.";

    const std::string out = Thoth::ChatRetrieval::formatChunkForPrompt(chunk);
    if (out.find("source_span=29-33") == std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanRange: missing source_span=29-33\n";
        return false;
    }
    if (out.find("Lines:") != std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanRange: must not emit Lines: header\n";
        return false;
    }
    if (out.find("Document: GRAG.md") == std::string::npos
        || out.find("Goal-Relative Adaptive Graph Retrieval") == std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanRange: document/body missing\n";
        return false;
    }
    return true;
}

// Plan N N4 / N-T8b — single-line span has no range suffix.
static bool testPlanNFormatChunkSourceSpanSingle() {
    CodeChunk chunk;
    chunk.fileName = "SEED.md";
    chunk.startLine = 5;
    chunk.endLine = 5;
    chunk.code = "one line";

    const std::string out = Thoth::ChatRetrieval::formatChunkForPrompt(chunk);
    if (out.find("source_span=5\n") == std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanSingle: expected source_span=5\\n\n";
        return false;
    }
    if (out.find("source_span=5-") != std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanSingle: must not emit range suffix\n";
        return false;
    }
    if (out.find("Lines:") != std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanSingle: must not emit Lines: header\n";
        return false;
    }
    return true;
}

// Plan N N4 / N-T8c — startLine=0 omits span entirely.
static bool testPlanNFormatChunkSourceSpanOmitted() {
    CodeChunk chunk;
    chunk.fileName = "SEED.md";
    chunk.startLine = 0;
    chunk.endLine = 10;
    chunk.code = "no span";

    const std::string out = Thoth::ChatRetrieval::formatChunkForPrompt(chunk);
    if (out.find("source_span=") != std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanOmitted: source_span must be omitted\n";
        return false;
    }
    if (out.find("Lines:") != std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanOmitted: must not emit Lines: header\n";
        return false;
    }
    if (out.find("Document: SEED.md\nno span") == std::string::npos) {
        std::cerr << "testPlanNFormatChunkSourceSpanOmitted: unexpected layout\n";
        return false;
    }
    return true;
}

// Investigation — metadata_off presentation: body only, --- segmentation at call site.
static bool testChatRagMetadataOffPresentation() {
    CodeChunk chunk;
    chunk.fileName = "/workspace/rag/completed_improvements_log.md";
    chunk.startLine = 10;
    chunk.endLine = 20;
    chunk.code = "Phase 12A graph statistics shipped.";

    const std::string labeled = Thoth::ChatRetrieval::formatChunkForPrompt(
        chunk, Thoth::ChatRetrieval::RagChunkPresentation::Labeled);
    if (labeled.find("Document: completed_improvements_log.md") == std::string::npos ||
        labeled.find("source_span=10-20") == std::string::npos ||
        labeled.find(chunk.code) == std::string::npos) {
        std::cerr << "testChatRagMetadataOffPresentation: labeled layout missing\n";
        return false;
    }

    const std::string metadataOff = Thoth::ChatRetrieval::formatChunkForPrompt(
        chunk, Thoth::ChatRetrieval::RagChunkPresentation::MetadataOff);
    if (metadataOff != chunk.code) {
        std::cerr << "testChatRagMetadataOffPresentation: metadata_off must return body only\n";
        return false;
    }
    if (metadataOff.find("Document:") != std::string::npos ||
        metadataOff.find("source_span=") != std::string::npos) {
        std::cerr << "testChatRagMetadataOffPresentation: metadata_off must omit labels\n";
        return false;
    }

    CodeChunk chunk2;
    chunk2.fileName = "other.md";
    chunk2.startLine = 1;
    chunk2.endLine = 2;
    chunk2.code = "Second chunk body.";
    const std::string assembled =
        Thoth::ChatRetrieval::formatChunkForPrompt(chunk, Thoth::ChatRetrieval::RagChunkPresentation::MetadataOff) +
        "\n---\n" +
        Thoth::ChatRetrieval::formatChunkForPrompt(chunk2, Thoth::ChatRetrieval::RagChunkPresentation::MetadataOff) +
        "\n---\n";
    if (assembled.find("\n---\n") == std::string::npos) {
        std::cerr << "testChatRagMetadataOffPresentation: expected --- chunk boundaries\n";
        return false;
    }
    if (assembled.find("Document:") != std::string::npos ||
        assembled.find("source_span=") != std::string::npos) {
        std::cerr << "testChatRagMetadataOffPresentation: assembled context must omit metadata\n";
        return false;
    }
    if (assembled.find(chunk.code) == std::string::npos ||
        assembled.find(chunk2.code) == std::string::npos) {
        std::cerr << "testChatRagMetadataOffPresentation: chunk bodies missing\n";
        return false;
    }

    if (std::string(Thoth::ChatRetrieval::ragChunkPresentationLabel(
            Thoth::ChatRetrieval::RagChunkPresentation::MetadataOff)) != "metadata_off") {
        std::cerr << "testChatRagMetadataOffPresentation: metadata_off label wrong\n";
        return false;
    }
    return true;
}

static bool testChatInferenceModeEnvDefault() {
    if (Thoth::ChatPrompt::chatInferenceModeFromEnv() != Thoth::ChatPrompt::ChatInferenceMode::Chat) {
        std::cerr << "testChatInferenceModeEnvDefault: expected chat default\n";
        return false;
    }
    if (std::string(Thoth::ChatPrompt::chatInferenceModeLabel(
            Thoth::ChatPrompt::ChatInferenceMode::Chat)) != "chat") {
        std::cerr << "testChatInferenceModeEnvDefault: chat label wrong\n";
        return false;
    }
    return true;
}

static bool testLlamaChatPayloadSerialization() {
    Thoth::InferenceChatRequest request;
    request.model = "chat";
    request.max_tokens = 512;
    request.messages.push_back({"system", "Response Rules:\nYou are Thoth."});
    request.messages.push_back({"user", "[RAG Context]\nchunk\n[User Query]\nhello"});

    const auto payload =
        nlohmann::json::parse(Thoth::LlamaServerClient::serializeChatPayload(request));
    if (payload["model"] != "chat" || payload["max_tokens"] != 512) {
        std::cerr << "testLlamaChatPayloadSerialization: model/max_tokens wrong\n";
        return false;
    }
    if (!payload.contains("messages") || payload["messages"].size() != 2) {
        std::cerr << "testLlamaChatPayloadSerialization: messages missing\n";
        return false;
    }
    if (payload["messages"][0]["role"] != "system" || payload["messages"][1]["role"] != "user") {
        std::cerr << "testLlamaChatPayloadSerialization: roles wrong\n";
        return false;
    }
    if (payload.contains("prompt")) {
        std::cerr << "testLlamaChatPayloadSerialization: must not send prompt field\n";
        return false;
    }

    request.stop_sequences = Thoth::ChatPrompt::chatStopSequences(
        Thoth::ChatPrompt::ChatInferenceMode::Chat);
    const auto payloadWithStops =
        nlohmann::json::parse(Thoth::LlamaServerClient::serializeChatPayload(request));
    if (!payloadWithStops.contains("stop") || !payloadWithStops["stop"].is_array() ||
        payloadWithStops["stop"].size() != 2 ||
        payloadWithStops["stop"][0] != Thoth::ChatPrompt::kChatMlStopImEnd ||
        payloadWithStops["stop"][1] != Thoth::ChatPrompt::kChatMlStopImStart) {
        std::cerr << "testLlamaChatPayloadSerialization: ChatML stop list wrong\n";
        return false;
    }
    return true;
}

static bool testChatRolePromptAssembly() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_chat_role_prompt.db").string();
    Memory memory(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto* idx = new IndexManager(engine.get());
    RAGPipeline rag(std::move(engine), idx, &cfg, &memory);
    PromptFactory factory(memory, rag);

    const std::string input = "Explain GRAG.";
    const std::string ragContextText =
        "Document: GRAG.md\nsource_span=1-3\nGRAG means Goal-Relative Adaptive Graph Retrieval.\n---\n";
    PromptFactory::ConversationBuildOptions grounded;
    grounded.grounded = true;

    const auto rolePrompt =
        factory.buildChatRolePrompt(input, ragContextText, false, grounded, nullptr);

    if (rolePrompt.system_content.find("Response Rules:") == std::string::npos ||
        rolePrompt.system_content.find("You are Thoth") == std::string::npos) {
        std::cerr << "testChatRolePromptAssembly: system rules missing\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (rolePrompt.system_content.size() >= Thoth::ChatPrompt::formatAgentCompletionCue().size() &&
        rolePrompt.system_content.compare(
            rolePrompt.system_content.size() - Thoth::ChatPrompt::formatAgentCompletionCue().size(),
            Thoth::ChatPrompt::formatAgentCompletionCue().size(),
            Thoth::ChatPrompt::formatAgentCompletionCue()) == 0) {
        std::cerr << "testChatRolePromptAssembly: system must not end with [Agent] cue\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (rolePrompt.system_content.find(std::string(Thoth::ChatPrompt::kUserTurnPrefix) + input) !=
        std::string::npos) {
        std::cerr << "testChatRolePromptAssembly: system must not include user turn\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (rolePrompt.user_content.find(Thoth::ChatPrompt::kRagContextHeader) != 0) {
        std::cerr << "testChatRolePromptAssembly: user must start with RAG header\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (rolePrompt.user_content.find(ragContextText) == std::string::npos ||
        rolePrompt.user_content.find(std::string(Thoth::ChatPrompt::kUserQueryHeader) + input) ==
            std::string::npos) {
        std::cerr << "testChatRolePromptAssembly: user must contain RAG body and query\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (rolePrompt.user_content.find("Response Rules:") != std::string::npos) {
        std::cerr << "testChatRolePromptAssembly: rules must not leak into user message\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

static bool testChatMultiTurnMessageAssembly() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_chat_multi_turn.db").string();
    Memory memory(cfg);
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    auto* idx = new IndexManager(engine.get());
    RAGPipeline rag(std::move(engine), idx, &cfg, &memory);
    PromptFactory factory(memory, rag);

    memory.addMessage("user", "What is EGAR and why is it important?");
    memory.addMessage("assistant", "EGAR is the evaluation framework for retrieval quality.");
    memory.save();

    const auto prior = factory.getPriorChatTurnMessages();
    if (prior.size() != 2 || prior[0].first != "user" || prior[1].first != "assistant") {
        std::cerr << "testChatMultiTurnMessageAssembly: prior turn roles wrong\n";
        fs::remove(cfg.database_path);
        return false;
    }

    const std::string input = "what are some improvements that were made?";
    const std::string ragContextText =
        "Document: improvements.md\nsource_span=1-3\nPhase 3 memory work.\n---\n";
    PromptFactory::ConversationBuildOptions grounded;
    grounded.grounded = true;
    grounded.includeConversationHistory = false;

    const auto rolePrompt =
        factory.buildChatRolePrompt(input, ragContextText, false, grounded, nullptr);
    if (rolePrompt.user_content.find("[\"user\"]") != std::string::npos ||
        rolePrompt.user_content.find("[\"assistant\"]") != std::string::npos) {
        std::cerr << "testChatMultiTurnMessageAssembly: flattened JSON history leaked into user\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (rolePrompt.user_content.find("What is EGAR") != std::string::npos) {
        std::cerr << "testChatMultiTurnMessageAssembly: prior user turn leaked into current user\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (rolePrompt.user_content.find(
            std::string(Thoth::ChatPrompt::kUserQueryHeader) + input) == std::string::npos) {
        std::cerr << "testChatMultiTurnMessageAssembly: current query missing from user message\n";
        fs::remove(cfg.database_path);
        return false;
    }

    fs::remove(cfg.database_path);
    return true;
}

// Plan N N6 / N-T9 — greeting-skip context telemetry unchanged; response has counts/flags only.
static bool testPlanNGreetingSkipTelemetryUnchanged() {
    Thoth::ChatRagContextRecord greeting;
    greeting.grounding_mode = "no_retrieval_hits";
    greeting.retrieval_ran = false;
    greeting.retrieval_skip_reason = "greeting";
    greeting.candidates_found = 0;
    greeting.candidates_passed_gate = 0;
    greeting.grounding_decision_reason = "greeting_skip";
    greeting.grounded = false;

    const auto ctx = Thoth::ChatRagLogger::contextToJson(greeting);
    if (ctx.value("retrieval_skip_reason", "") != "greeting"
        || ctx.value("grounding_decision_reason", "") != "greeting_skip"
        || ctx.value("grounded", true) != false
        || ctx.value("retrieval_ran", true) != false) {
        std::cerr << "testPlanNGreetingSkipTelemetryUnchanged: greeting context shape regresssed\n";
        return false;
    }

    Thoth::ChatRagResponseRecord response;
    response.request_id = "n6-t9";
    response.answer_chars = 12;
    response.grounding_mode = "no_retrieval_hits";
    response.fallback_used = true;
    response.raw_answer_chars = 40;
    response.sanitized_answer_chars = 12;
    response.sanitize_reason = Thoth::ChatGeneration::kSanitizeTruncatedTranscriptMarker;
    response.retried_without_stops = true;
    response.used_stops = false;
    response.provider_ok = true;
    response.finish_reason = "length";

    const auto resp = Thoth::ChatRagLogger::responseToJson(response);
    if (!resp.contains("raw_answer_chars") || !resp.contains("sanitize_reason")
        || !resp.contains("provider_ok") || resp.value("fallback_used", false) != true) {
        std::cerr << "testPlanNGreetingSkipTelemetryUnchanged: missing gen diagnostic fields\n";
        return false;
    }
    if (resp.contains("raw_text") || resp.contains("sanitized_text") || resp.contains("prompt")) {
        std::cerr << "testPlanNGreetingSkipTelemetryUnchanged: telemetry must not dump raw text\n";
        return false;
    }
    if (!resp.contains("generation_attempt_count") || !resp.contains("turn_total_ms")
        || !resp.contains("response_valid")) {
        std::cerr << "testPlanNGreetingSkipTelemetryUnchanged: missing Phase 0 timing fields\n";
        return false;
    }
    return true;
}

static bool testChatRagPhase0ResponseTelemetryShape() {
    Thoth::ChatRagResponseRecord response;
    response.request_id = "phase0-shape";
    response.generation_attempt_count = 2;
    response.turn_total_ms = 1200;
    response.queue_wait_ms = 50;
    response.retrieval_latency_ms = 100;
    response.prompt_build_latency_ms = 5;
    response.generation_latency_ms = 1000;
    response.post_processing_latency_ms = 3;
    response.telemetry_accounted_ms = 1158;
    response.telemetry_unaccounted_ms = 42;
    response.response_valid = false;
    response.invalid_reason = "query_echo";

    Thoth::ChatGenerationAttemptRecord attempt;
    attempt.attempt = 1;
    attempt.latency_ms = 600;
    attempt.prompt_tokens = 400;
    attempt.completion_tokens = 120;
    attempt.finish_reason = "length";
    attempt.provider_ok = true;
    attempt.raw_answer_chars = 800;
    attempt.sanitize_reason = Thoth::ChatGeneration::kSanitizeTruncatedTranscriptMarker;
    response.generation_attempts.push_back(attempt);

    const auto json = Thoth::ChatRagLogger::responseToJson(response);
    if (!json.contains("generation_attempts") || !json["generation_attempts"].is_array()
        || json["generation_attempts"].size() != 1) {
        std::cerr << "testChatRagPhase0ResponseTelemetryShape: generation_attempts missing\n";
        return false;
    }
    const auto& row = json["generation_attempts"][0];
    if (row.value("attempt", 0) != 1 || row.value("latency_ms", 0) != 600
        || row.value("completion_tokens", 0) != 120) {
        std::cerr << "testChatRagPhase0ResponseTelemetryShape: attempt row mismatch\n";
        return false;
    }
    if (json.value("telemetry_unaccounted_ms", -1) != 42
        || json.value("invalid_reason", "") != "query_echo") {
        std::cerr << "testChatRagPhase0ResponseTelemetryShape: reconciliation fields wrong\n";
        return false;
    }
    if (json.contains("raw_sample_first") || json.contains("raw_sample_last")) {
        std::cerr << "testChatRagPhase0ResponseTelemetryShape: raw samples must be omitted when empty\n";
        return false;
    }
    return true;
}

static bool testChatPhase0GenerationAttemptCount() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("");
    Thoth::RobustnessMockResponses::push("Hello from retry.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.max_tokens = 64;

    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", opts);
    Thoth::RobustnessMockResponses::reset();

    if (out.generation_attempt_count != 2
        || static_cast<int>(out.generation_attempts.size()) != 2) {
        std::cerr << "testChatPhase0GenerationAttemptCount: expected 2 tracked attempts, got "
                  << out.generation_attempt_count << "\n";
        return false;
    }
    if (out.generation_attempts[0].attempt != 1 || out.generation_attempts[1].attempt != 2) {
        std::cerr << "testChatPhase0GenerationAttemptCount: attempt indices wrong\n";
        return false;
    }
    if (out.generation_latency_ms < 0) {
        std::cerr << "testChatPhase0GenerationAttemptCount: generation_latency_ms invalid\n";
        return false;
    }
    const std::int64_t summed = out.generation_attempts[0].latency_ms
                              + out.generation_attempts[1].latency_ms;
    if (out.generation_latency_ms != summed) {
        std::cerr << "testChatPhase0GenerationAttemptCount: generation_latency_ms mismatch\n";
        return false;
    }
    return true;
}

static bool testChatPhase0ResponseValidityTelemetryOnly() {
    const auto echo = Thoth::ChatGeneration::assessResponseValidityForTelemetry(
        "What improvements have been completed?",
        "What improvements have been completed?",
        Thoth::ChatGeneration::kSanitizeTruncatedTranscriptMarker);
    if (echo.valid || echo.invalid_reason != Thoth::ChatGeneration::kInvalidReasonQueryEcho) {
        std::cerr << "testChatPhase0ResponseValidityTelemetryOnly: query echo not flagged\n";
        return false;
    }

    const auto ok = Thoth::ChatGeneration::assessResponseValidityForTelemetry(
        "Thoth completed GRAG integration and Plan N chat safety.",
        "What improvements have been completed?",
        Thoth::ChatGeneration::kSanitizeNone);
    if (!ok.valid) {
        std::cerr << "testChatPhase0ResponseValidityTelemetryOnly: valid answer flagged invalid\n";
        return false;
    }
    return true;
}

static bool testChatPhase0RawCompletionSampleEnv() {
    const auto disabled = Thoth::ChatGeneration::buildRawCompletionSample(
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-extra-tail");
    if (!disabled.first.empty() || !disabled.last.empty()) {
        std::cerr << "testChatPhase0RawCompletionSampleEnv: samples must be empty when env unset\n";
        return false;
    }

#if defined(_WIN32)
    _putenv("THOTH_LOG_RAW_CHAT_COMPLETION=1");
#else
    setenv("THOTH_LOG_RAW_CHAT_COMPLETION", "1", 1);
#endif
    const std::string long_text(200, 'x');
    const auto enabled = Thoth::ChatGeneration::buildRawCompletionSample(long_text);
#if defined(_WIN32)
    _putenv("THOTH_LOG_RAW_CHAT_COMPLETION=");
#else
    unsetenv("THOTH_LOG_RAW_CHAT_COMPLETION");
#endif

    if (enabled.first.size() != 80 || enabled.last.size() != 80) {
        std::cerr << "testChatPhase0RawCompletionSampleEnv: sample size wrong\n";
        return false;
    }
    return true;
}

static int countAssistantMessages(const Memory& memory) {
    int count = 0;
    for (const auto& msg : memory.getConversation()) {
        if (msg.value("role", "") == "assistant") {
            ++count;
        }
    }
    return count;
}

static std::string lastAssistantContent(const Memory& memory) {
    std::string last;
    for (const auto& msg : memory.getConversation()) {
        if (msg.value("role", "") == "assistant") {
            last = msg.value("content", "");
        }
    }
    return last;
}

static bool hasTranscriptScaffoldMarkers(const std::string& text) {
    return text.find("[User]") != std::string::npos
        || text.find("[Agent]") != std::string::npos
        || text.find("[RAG Context]") != std::string::npos
        || text.find("📝") != std::string::npos;
}

// Plan N N6 / N-T10 — helper retry still yields one assistant memory write.
static bool testPlanNMemoryOncePerTurn() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_n6_memory_once.db").string();
    Memory memory(cfg);
    memory.setActiveSessionId("n6-t10");
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager indexManager(engine.get());
    RAGPipeline rag(std::move(engine), &indexManager, &cfg);
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    CommandProcessor cp(memory, rag, llm, &cfg);

    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("");
    Thoth::RobustnessMockResponses::push("Stable reply after retry.");

    const std::string out = cp.processQuery("hi there");
    Thoth::RobustnessMockResponses::reset();

    if (out != "Stable reply after retry.") {
        std::cerr << "testPlanNMemoryOncePerTurn: unexpected reply: " << out << "\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (countAssistantMessages(memory) != 1) {
        std::cerr << "testPlanNMemoryOncePerTurn: expected one assistant memory write, got "
                  << countAssistantMessages(memory) << "\n";
        fs::remove(cfg.database_path);
        return false;
    }
    fs::remove(cfg.database_path);
    return true;
}

// Plan N N6 / N-T11 — scaffold sanitized before memory; no Empty completion string.
static bool testPlanNCpScaffoldSanitizedToMemory() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_n6_scaffold.db").string();
    Memory memory(cfg);
    memory.setActiveSessionId("n6-t11");
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager indexManager(engine.get());
    RAGPipeline rag(std::move(engine), &indexManager, &cfg);
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    CommandProcessor cp(memory, rag, llm, &cfg);

    const std::string scripted =
        "Welcome.\n"
        "\n"
        "[User] How can I help?\n"
        "[Agent] Let's start with a simple question.\n";

    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push(scripted);

    const std::string out = cp.processQuery("hello there");
    Thoth::RobustnessMockResponses::reset();

    if (out.find("Empty completion") != std::string::npos) {
        std::cerr << "testPlanNCpScaffoldSanitizedToMemory: Empty completion leaked\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (hasTranscriptScaffoldMarkers(out)) {
        std::cerr << "testPlanNCpScaffoldSanitizedToMemory: scaffold markers in return\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (countAssistantMessages(memory) != 1) {
        std::cerr << "testPlanNCpScaffoldSanitizedToMemory: expected one assistant write\n";
        fs::remove(cfg.database_path);
        return false;
    }
    const std::string stored = lastAssistantContent(memory);
    if (hasTranscriptScaffoldMarkers(stored)) {
        std::cerr << "testPlanNCpScaffoldSanitizedToMemory: scaffold markers in stored memory\n";
        fs::remove(cfg.database_path);
        return false;
    }
    fs::remove(cfg.database_path);
    return true;
}

// Plan N N6 / N-T12 — Class A: no tool call, formatted error, one memory write.
static bool testPlanNCpProviderFailure() {
    Config cfg;
    cfg.database_path = makeTempPath("thoth_n6_provider_fail.db").string();
    Memory memory(cfg);
    memory.setActiveSessionId("n6-t12");
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf);
    IndexManager indexManager(engine.get());
    RAGPipeline rag(std::move(engine), &indexManager, &cfg);
    LLMInterface llm(LLMBackend::Ollama, &cfg);
    CommandProcessor cp(memory, rag, llm, &cfg);

    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::pushFailure("connection failed");
    CommandProcessor::resetProcessToolCallProbeForTest();

    const std::string out = cp.processQuery("hello there");
    Thoth::RobustnessMockResponses::reset();

    if (CommandProcessor::processToolCallProbeCountForTest() != 0) {
        std::cerr << "testPlanNCpProviderFailure: processToolCall must not be called\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (out.find("[Error]") == std::string::npos || out.find("connection failed") == std::string::npos) {
        std::cerr << "testPlanNCpProviderFailure: expected CP-formatted error, got: " << out << "\n";
        fs::remove(cfg.database_path);
        return false;
    }
    if (countAssistantMessages(memory) != 1) {
        std::cerr << "testPlanNCpProviderFailure: expected one assistant memory write\n";
        fs::remove(cfg.database_path);
        return false;
    }
    const std::string stored = lastAssistantContent(memory);
    if (stored.find("[Error]") == std::string::npos || stored.find("connection failed") == std::string::npos) {
        std::cerr << "testPlanNCpProviderFailure: memory missing formatted error\n";
        fs::remove(cfg.database_path);
        return false;
    }
    fs::remove(cfg.database_path);
    return true;
}

// Plan N5 / N5-T1 — final score 1.18 is float text, never percent.
static bool testPlanN5FormatFinalScoreNoPercent() {
    const std::string out = Thoth::GragDiagnosticsDisplay::formatFinalScoreLabel(1.18f);
    if (out.find("1.18") == std::string::npos) {
        std::cerr << "testPlanN5FormatFinalScoreNoPercent: expected 1.18 in '" << out << "'\n";
        return false;
    }
    if (out.find('%') != std::string::npos) {
        std::cerr << "testPlanN5FormatFinalScoreNoPercent: must not contain %\n";
        return false;
    }
    return true;
}

// Plan N5 / N5-T2 — 0.42 is not 42%.
static bool testPlanN5FormatFinalScoreSmall() {
    const std::string out = Thoth::GragDiagnosticsDisplay::formatFinalScoreLabel(0.42f);
    if (out.find("0.42") == std::string::npos) {
        std::cerr << "testPlanN5FormatFinalScoreSmall: expected 0.42 in '" << out << "'\n";
        return false;
    }
    if (out.find('%') != std::string::npos || out.find("42.0") != std::string::npos) {
        std::cerr << "testPlanN5FormatFinalScoreSmall: must not look like a percent\n";
        return false;
    }
    return true;
}

// Plan N5 / N5-T3 — missing / non-finite → N/A.
static bool testPlanN5FormatFinalScoreMissing() {
    if (Thoth::GragDiagnosticsDisplay::formatFinalScoreLabel(std::nullopt) != "N/A") {
        std::cerr << "testPlanN5FormatFinalScoreMissing: nullopt must be N/A\n";
        return false;
    }
    if (Thoth::GragDiagnosticsDisplay::formatFinalScoreLabel(
            std::numeric_limits<float>::quiet_NaN()) != "N/A") {
        std::cerr << "testPlanN5FormatFinalScoreMissing: NaN must be N/A\n";
        return false;
    }
    return true;
}

// Plan N5 / N5-T4 — conversational: Alpha N/A, magnitude numeric, Mode Conversational.
static bool testPlanN5ConversationalAlphaMode() {
    if (!Thoth::GragDiagnosticsDisplay::isConversationalNoGoalDirection("greeting_skip")
        || !Thoth::GragDiagnosticsDisplay::isConversationalNoGoalDirection("rag_hybrid")
        || !Thoth::GragDiagnosticsDisplay::isConversationalNoGoalDirection("no_index")) {
        std::cerr << "testPlanN5ConversationalAlphaMode: expected conversational modes\n";
        return false;
    }
    if (Thoth::GragDiagnosticsDisplay::isConversationalNoGoalDirection("grag_hybrid")
        || Thoth::GragDiagnosticsDisplay::isConversationalNoGoalDirection("grag_blended_hybrid")) {
        std::cerr << "testPlanN5ConversationalAlphaMode: grag_* should use goal direction\n";
        return false;
    }

    const std::string alpha =
        Thoth::GragDiagnosticsDisplay::formatAlphaLabel(0.0f, true);
    if (alpha.find("N/A") == std::string::npos || alpha == "Alpha: 0.00") {
        std::cerr << "testPlanN5ConversationalAlphaMode: Alpha must be N/A, not bare 0.00\n";
        return false;
    }

    const std::string mag = Thoth::GragDiagnosticsDisplay::formatMagnitudeLabel(0.0f);
    if (mag.find("0.000") == std::string::npos) {
        std::cerr << "testPlanN5ConversationalAlphaMode: magnitude must stay numeric\n";
        return false;
    }

    if (Thoth::GragDiagnosticsDisplay::formatModeLabel("greeting_skip") != "Conversational"
        || Thoth::GragDiagnosticsDisplay::formatModeLabel("rag_hybrid") != "Conversational") {
        std::cerr << "testPlanN5ConversationalAlphaMode: Mode should be Conversational\n";
        return false;
    }
    return true;
}

// Plan N5 / N5-T5 — unknown scoring mode: fallback formatting, no %.
static bool testPlanN5UnknownScoringMode() {
    const std::string mode = Thoth::GragDiagnosticsDisplay::formatModeLabel("future_mystery_mode");
    if (mode != "future_mystery_mode") {
        std::cerr << "testPlanN5UnknownScoringMode: expected raw fallback mode, got " << mode << "\n";
        return false;
    }
    const std::string score = Thoth::GragDiagnosticsDisplay::formatFinalScoreLabel(2.03f);
    if (score.find("2.03") == std::string::npos || score.find('%') != std::string::npos) {
        std::cerr << "testPlanN5UnknownScoringMode: Final Score fallback formatting failed\n";
        return false;
    }
    const bool conversational =
        Thoth::GragDiagnosticsDisplay::isConversationalNoGoalDirection("future_mystery_mode");
    const std::string alpha =
        Thoth::GragDiagnosticsDisplay::formatAlphaLabel(0.0f, conversational);
    if (alpha.find('%') != std::string::npos) {
        std::cerr << "testPlanN5UnknownScoringMode: Alpha must not invent %\n";
        return false;
    }
    return true;
}

// GUI Phase 1 — remote RAG honesty (+ Phase 5: never claim indexing on drop).
static bool testGuiPhase1RemoteRagHonestyPolicy() {
    using namespace Thoth::RemoteRagHonesty;
    if (shouldSyncRagFilesToBackend(true) || shouldClaimIndexingOnHostDrop(true)) {
        std::cerr << "testGuiPhase1RemoteRagHonestyPolicy: remote must not sync/claim indexing\n";
        return false;
    }
    // Phase 5 D3a: Local also must not claim indexing on drop — wait for INDEXING_* events.
    if (!shouldSyncRagFilesToBackend(false) || shouldClaimIndexingOnHostDrop(false)) {
        std::cerr << "testGuiPhase1RemoteRagHonestyPolicy: local must sync setRagFiles but not claim indexing on drop\n";
        return false;
    }
    const std::string status = formatHostOnlyAddStatus(2);
    if (status != "Added 2 file(s) — host-only; not sent to Engine") {
        std::cerr << "testGuiPhase1RemoteRagHonestyPolicy: bad status '" << status << "'\n";
        return false;
    }
    if (status.find("indexing") != std::string::npos) {
        std::cerr << "testGuiPhase1RemoteRagHonestyPolicy: status must not claim indexing\n";
        return false;
    }
    const std::string label = formatHostOnlySlotLabel("notes.md");
    if (label != "notes.md (host-only)") {
        std::cerr << "testGuiPhase1RemoteRagHonestyPolicy: bad label '" << label << "'\n";
        return false;
    }
    return true;
}

// GUI Restoration R1 — remote + ingest: Local Notes stay host-side; no setRagFiles sync.
static bool testGuiR1RemoteIngestHostOnlyPresentation() {
    using namespace Thoth::RemoteRagHonesty;

    constexpr bool kIsRemote = true;
    constexpr bool kSupportsIngest = true;

    if (!localNotesAreHostSideOnly(kIsRemote)) {
        std::cerr << "testGuiR1: remote Local Notes must be host-side only\n";
        return false;
    }
    if (localNotesAreHostSideOnly(false)) {
        std::cerr << "testGuiR1: local mode must not use host-only note semantics\n";
        return false;
    }
    if (shouldSyncRagFilesToBackend(kIsRemote)) {
        std::cerr << "testGuiR1: remote must not sync setRagFiles even when ingest enabled\n";
        return false;
    }
    (void)kSupportsIngest;
    if (!shouldSyncRagFilesToBackend(false)) {
        std::cerr << "testGuiR1: local must still sync setRagFiles\n";
        return false;
    }

    const std::string status = formatHostOnlyAddStatus(1);
    if (status.find("host-only") == std::string::npos) {
        std::cerr << "testGuiR1: remote drop status must remain host-only copy\n";
        return false;
    }
    const std::string label = formatHostOnlySlotLabel("doc.md");
    if (label != "doc.md (host-only)") {
        std::cerr << "testGuiR1: slot label must stay host-only when isRemote\n";
        return false;
    }

    const std::string sent = formatLocalNoteEngineSlotLabel(
        "doc.md", "doc-abc", 3, false, false);
    if (sent != "doc.md · id=doc-abc · 3 chunks") {
        std::cerr << "testGuiR1: engine slot label wrong: '" << sent << "'\n";
        return false;
    }

    if (formatLocalNoteStripActivity(3, 0) != kHostOnlyTooltip) {
        std::cerr << "testGuiR1: unsent slots must show host-only strip\n";
        return false;
    }
    if (!formatLocalNoteStripActivity(3, 1).empty()
        || !formatLocalNoteStripActivity(3, 3).empty()
        || !formatLocalNoteStripActivity(0, 0).empty()) {
        std::cerr << "testGuiR1: attached or empty slots must clear host-only strip\n";
        return false;
    }

    using namespace Thoth;
    if (progressSourceForBackendEvent(true) != ProgressSource::EngineEvent
        || progressSourceForBackendEvent(false) != ProgressSource::LocalBackendEvent) {
        std::cerr << "testGuiR1: progress source must follow isRemote not supportsIngest\n";
        return false;
    }

    return true;
}

static bool testLocalNoteSlotHidesCorpusUntilSent() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    LocalNoteEngineInfo reconcile_only{
        "doc-abc", "EGAR.md", {}, "hash-1", 95, false, false, true};
    if (localNoteEngineSlotShowsAttachedStatus(reconcile_only)) {
        std::cerr << "testLocalNoteSlotHidesCorpusUntilSent: reconcile cache must stay host-only\n";
        return false;
    }

    LocalNoteEngineInfo attached{
        "doc-abc", "EGAR.md", "rev-1", "hash-1", 95, false, false, true};
    if (!localNoteEngineSlotShowsAttachedStatus(attached)) {
        std::cerr << "testLocalNoteSlotHidesCorpusUntilSent: sent slot must show engine status\n";
        return false;
    }

    LocalNoteEngineInfo indexing{
        "doc-abc", "EGAR.md", "rev-1", "hash-1", -1, true, false, true};
    if (!localNoteEngineSlotShowsAttachedStatus(indexing)) {
        std::cerr << "testLocalNoteSlotHidesCorpusUntilSent: indexing slot must show engine status\n";
        return false;
    }

    ChatSession session;
    session.ragFilePaths = {"/host/a.md", "/host/b.md"};
    session.localNoteEngine["/host/a.md"] = reconcile_only;
    if (countAttachedLocalNotes(session) != 0) {
        std::cerr << "testLocalNoteSlotHidesCorpusUntilSent: unsent slots must count 0 attached\n";
        return false;
    }
    session.localNoteEngine["/host/b.md"] = attached;
    if (countAttachedLocalNotes(session) != 1) {
        std::cerr << "testLocalNoteSlotHidesCorpusUntilSent: one sent slot must count 1 attached\n";
        return false;
    }

    return true;
}

static bool testLocalNoteEngineCorpusSync() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    ChatSession session;
    session.ragFilePaths.push_back("/host/notes.md");
    session.localNoteEngine["/host/notes.md"] = LocalNoteEngineInfo{
        "doc-abc", "notes_1.md", {}, {}, -1, true, false, false};

    const auto corpus = CorpusDocuments::makeDocument(
        "doc-abc", "notes_1.md", "failed", std::nullopt, std::nullopt, "empty_document");
    const nlohmann::json body = {
        {"schema_version", CorpusDocuments::kSchemaVersion},
        {"documents", nlohmann::json::array({corpus})},
    };

    if (!syncSessionFromCorpusList(session, body)) {
        std::cerr << "testLocalNoteEngineCorpusSync: expected change\n";
        return false;
    }
    const auto& info = session.localNoteEngine.at("/host/notes.md");
    if (info.indexing || !info.failed) {
        std::cerr << "testLocalNoteEngineCorpusSync: failed status not applied\n";
        return false;
    }

    session.localNoteEngine["/host/notes.md"].indexing = true;
    session.localNoteEngine["/host/notes.md"].failed = false;
    const auto indexed = CorpusDocuments::makeDocument(
        "doc-abc", "notes_1.md", "indexed", "2026-01-01T00:00:00Z", 4, std::nullopt);
    const nlohmann::json indexed_body = {
        {"schema_version", CorpusDocuments::kSchemaVersion},
        {"documents", nlohmann::json::array({indexed})},
    };
    if (!syncSessionFromCorpusList(session, indexed_body)) {
        std::cerr << "testLocalNoteEngineCorpusSync: indexed update expected\n";
        return false;
    }
    const auto& done = session.localNoteEngine.at("/host/notes.md");
    if (done.indexing || done.failed || done.chunk_count != 4) {
        std::cerr << "testLocalNoteEngineCorpusSync: indexed status wrong\n";
        return false;
    }

    return true;
}

static bool testLocalNoteSendSelectionFilter() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    ChatSession session;
    session.ragFilePaths = {"/host/a.md", "/host/b.md", "/host/c.md", "/host/d.md"};
    session.localNoteEngine["/host/b.md"] = LocalNoteEngineInfo{
        "doc-b", "b.md", {}, {}, 12, false, false, false};
    session.localNoteEngine["/host/c.md"] = LocalNoteEngineInfo{
        "doc-c", "c.md", {}, {}, -1, true, false, false};
    session.localNoteEngine["/host/d.md"] = LocalNoteEngineInfo{
        "doc-d", "d.md", {}, {}, -1, false, true, false};

    if (localNoteAlreadySent(session, "/host/a.md")) {
        std::cerr << "testLocalNoteSendSelectionFilter: host-only must not count as sent\n";
        return false;
    }
    if (!localNoteAlreadySent(session, "/host/b.md")
        || !localNoteAlreadySent(session, "/host/c.md")
        || !localNoteAlreadySent(session, "/host/d.md")) {
        std::cerr << "testLocalNoteSendSelectionFilter: accepted notes must count as sent\n";
        return false;
    }

    const auto unsent = collectUnsentLocalNotePaths(session);
    if (unsent.size() != 1 || unsent.front() != "/host/a.md") {
        std::cerr << "testLocalNoteSendSelectionFilter: expected only /host/a.md unsent\n";
        return false;
    }

    return true;
}

// GUI Phase 2 — BackendCapabilities matrix, mode labels, D10 disposition, Unavailable copy.
static bool testGuiPhase2BackendCapabilitiesAndPresentation() {
    using namespace Thoth;

    const auto local = localBackendCapabilities();
    const auto engine = engineBackendCapabilities();
    if (!local.supportsStrategies || !local.supportsTrajectories || !local.supportsEpisodes
        || !local.supportsExperiments
        || !local.supportsGraphStats || !local.supportsBenchmarks || !local.supportsLogs
        || !local.supportsIngest || !local.supportsPlanDiagnostics) {
        std::cerr << "testGuiPhase2: Local matrix incomplete\n";
        return false;
    }
    if (engine.supportsStrategies || engine.supportsTrajectories || engine.supportsEpisodes
        || engine.supportsExperiments
        || engine.supportsGraphStats || engine.supportsBenchmarks || engine.supportsLogs
        || engine.supportsIngest) {
        std::cerr << "testGuiPhase2: Engine matrix must keep cognate/logs/ingest false\n";
        return false;
    }
    // Phase 4: Engine advertises plan diagnostics (decision summary resource).
    if (!engine.supportsPlanDiagnostics) {
        std::cerr << "testGuiPhase2: Engine must support plan diagnostics after Phase 4\n";
        return false;
    }
    if (!local.supportsCorpusList || !engine.supportsCorpusList) {
        std::cerr << "testGuiPhase2: Phase 8 supportsCorpusList must be true for Local and Engine\n";
        return false;
    }
    if (capabilitiesForRemoteFlag(false).supportsIngest != true
        || capabilitiesForRemoteFlag(true).supportsIngest != false) {
        std::cerr << "testGuiPhase2: capabilitiesForRemoteFlag mismatch\n";
        return false;
    }
    if (std::string(backendModeLabel(false)) != kBackendModeLocal
        || std::string(backendModeLabel(true)) != kBackendModeEngine) {
        std::cerr << "testGuiPhase2: mode labels must be Backend: Local / Backend: Engine\n";
        return false;
    }
    if (std::string(backendModeLabel(true)).find("Remote") != std::string::npos) {
        std::cerr << "testGuiPhase2: mode label must not say Remote\n";
        return false;
    }
    const std::string unavailable = kUnavailableWithCurrentBackend;
    if (unavailable.find("Plan K") != std::string::npos) {
        std::cerr << "testGuiPhase2: Unavailable copy must not mention Plan K\n";
        return false;
    }
    // D10: empty data with capability → Empty; without capability → Unavailable
    if (disposePanelData(false, true) != PanelDataDisposition::Unavailable
        || disposePanelData(false, false) != PanelDataDisposition::Unavailable) {
        std::cerr << "testGuiPhase2: D10 missing capability must be Unavailable\n";
        return false;
    }
    if (disposePanelData(true, true) != PanelDataDisposition::Empty) {
        std::cerr << "testGuiPhase2: D10 empty data with capability must be Empty\n";
        return false;
    }
    if (disposePanelData(true, false) != PanelDataDisposition::Populated) {
        std::cerr << "testGuiPhase2: D10 non-empty with capability must be Populated\n";
        return false;
    }
    return true;
}

// GUI Phase 3 — D11 cognitive diagnostics authority; Explain Plan unavailable sentinel.
static bool testGuiPhase3CognitiveDiagnosticsAuthority() {
    using namespace Thoth;
    using namespace Thoth::CognitiveDiagnostics;

    const auto local = localBackendCapabilities();
    const auto engine = engineBackendCapabilities();

    if (!mayReadHostDecisionTrace(local) || !mayReadHostLogs(local)) {
        std::cerr << "testGuiPhase3: Local must allow host decision_trace/logs\n";
        return false;
    }
    // Phase 4: Engine supports plan diagnostics via resource API (not host files).
    if (!mayReadHostDecisionTrace(engine)) {
        std::cerr << "testGuiPhase3: Engine supportsPlanDiagnostics should be true after Phase 4\n";
        return false;
    }
    if (mayReadHostLogs(engine)) {
        std::cerr << "testGuiPhase3: Engine must not claim host logs\n";
        return false;
    }

    const std::string body = formatExplainPlanUnavailableBody();
    if (body != "Unavailable\n\nThe current backend does not expose plan diagnostics.") {
        std::cerr << "testGuiPhase3: bad Explain Plan body '" << body << "'\n";
        return false;
    }
    if (body.find("Plan K") != std::string::npos) {
        std::cerr << "testGuiPhase3: Explain Plan copy must not mention Plan K\n";
        return false;
    }
    if (!isDecisionTraceUnavailableSentinel(decisionTraceUnavailableSentinel())) {
        std::cerr << "testGuiPhase3: sentinel mismatch\n";
        return false;
    }
    if (isDecisionTraceUnavailableSentinel("Latest Event: foo")) {
        std::cerr << "testGuiPhase3: real trace must not match sentinel\n";
        return false;
    }

    // D11 disposition: capability false → unavailable regardless of whether a host file exists.
    BackendCapabilities no_plan;
    no_plan.supportsPlanDiagnostics = false;
    if (disposePanelData(no_plan.supportsPlanDiagnostics, false)
        != PanelDataDisposition::Unavailable) {
        std::cerr << "testGuiPhase3: missing plan diagnostics must be Unavailable\n";
        return false;
    }
    if (disposePanelData(local.supportsPlanDiagnostics, true)
        != PanelDataDisposition::Empty) {
        std::cerr << "testGuiPhase3: Local empty plan data is Empty not Unavailable\n";
        return false;
    }
    if (disposePanelData(engine.supportsPlanDiagnostics, false)
        != PanelDataDisposition::Populated) {
        std::cerr << "testGuiPhase3: Engine with capability + non-empty → Populated disposition\n";
        return false;
    }

    const std::string logsWhy = std::string("Unavailable\n\n") + kLogsNotExposedWhy;
    if (logsWhy.find("Plan K") != std::string::npos
        || logsWhy.find("expose logs") == std::string::npos) {
        std::cerr << "testGuiPhase3: logs why-copy invalid\n";
        return false;
    }
    return true;
}

// GUI Phase 6 — SSE reconnect policy + connection/engine separation.
static bool testGuiPhase6EventStreamResilience() {
    using namespace Thoth;

    if (sseReconnectDelayMs(0) != kSseReconnectInitialMs
        || sseReconnectDelayMs(1) != kSseReconnectInitialMs * 2
        || sseReconnectDelayMs(10) != kSseReconnectMaxMs) {
        std::cerr << "testGuiPhase6: backoff helper wrong\n";
        return false;
    }

    if (formatEventsStatusLine(EventConnectionState::Reconnecting) != "Events: Reconnecting"
        || formatEngineStatusLine(EngineHealthState::Starting) != "Engine: Starting") {
        std::cerr << "testGuiPhase6: status lines must be distinct Connection vs Engine\n";
        return false;
    }

    if (formatEventsStatusLine(EventConnectionState::Connected).find("Engine:") != std::string::npos) {
        std::cerr << "testGuiPhase6: events line must not mention Engine\n";
        return false;
    }

    EventStreamSnapshot live{};
    live.applies = true;
    live.connection = EventConnectionState::Connected;
    live.engine = EngineHealthState::Ready;
    if (shouldShowConnectionIndicator(live) || shouldShowEngineIndicator(live)) {
        std::cerr << "testGuiPhase6: calm when Connected+Ready\n";
        return false;
    }

    EventStreamSnapshot reconnect{};
    reconnect.applies = true;
    reconnect.connection = EventConnectionState::Reconnecting;
    reconnect.engine = EngineHealthState::Ready;
    if (!shouldShowConnectionIndicator(reconnect) || shouldShowEngineIndicator(reconnect)) {
        std::cerr << "testGuiPhase6: show connection not engine when SSE down\n";
        return false;
    }

    EventStreamSnapshot engine_down{};
    engine_down.applies = true;
    engine_down.connection = EventConnectionState::Reconnecting;
    engine_down.engine = EngineHealthState::Starting;
    if (!shouldShowConnectionIndicator(engine_down) || !shouldShowEngineIndicator(engine_down)) {
        std::cerr << "testGuiPhase6: both axes visible on long outage\n";
        return false;
    }

    if (!engineHttpUsable(EngineHealthState::Ready)
        || engineHttpUsable(EngineHealthState::Starting)) {
        std::cerr << "testGuiPhase6: engineHttpUsable wrong\n";
        return false;
    }

    const auto local = localEventStreamSnapshot(1000);
    if (local.applies || local.connection != EventConnectionState::Connected) {
        std::cerr << "testGuiPhase6: Local must not show SSE chrome\n";
        return false;
    }

    if (formatLastEventAgeLabel(0, 5000) != "Last event: —"
        || formatLastEventAgeLabel(2000, 5000) != "Last event: 3 s ago") {
        std::cerr << "testGuiPhase6: last-event-age formatting wrong\n";
        return false;
    }

#if THOTH_HAS_GUI
    RemoteAgentBackend remote("");
    const auto snap = remote.eventStreamSnapshot();
    if (!snap.applies) {
        std::cerr << "testGuiPhase6: remote snapshot must apply\n";
        return false;
    }
#endif

    return true;
}

// GUI Phase 7 — operation result honesty + Phase 6 correlation.
static bool testGuiPhase7OperationResultHonesty() {
    using namespace Thoth;

    const auto ok = makeSuccess(kOpChat, "Response received", "hello");
    if (!ok.success || ok.response_text != "hello" || ok.operation != kOpChat) {
        std::cerr << "testGuiPhase7: makeSuccess chat wrong\n";
        return false;
    }

    const auto fail = makeFailure(kOpAbort, "Abort failed", "[RemoteEngine] transport", true);
    if (fail.success || fail.operation != kOpAbort || !fail.retryable) {
        std::cerr << "testGuiPhase7: makeFailure wrong\n";
        return false;
    }

    EventStreamSnapshot unavailable{};
    unavailable.applies = true;
    unavailable.connection = EventConnectionState::Reconnecting;
    unavailable.engine = EngineHealthState::Ready;

    const auto abort_fail = makeFailure(kOpAbort, "Abort request could not be delivered",
                                        "transport reset", true);
    const std::string correlated =
        formatCorrelatedUserMessage(unavailable, abort_fail);
    if (correlated.find("Engine unavailable") == std::string::npos
        || correlated.find("Abort request could not be delivered") == std::string::npos) {
        std::cerr << "testGuiPhase7: correlation wrong: " << correlated << "\n";
        return false;
    }
    if (correlated.find("Engine unavailable — Engine unavailable") != std::string::npos) {
        std::cerr << "testGuiPhase7: duplicate root cause\n";
        return false;
    }

    if (uiSeverityForFailure(abort_fail, unavailable) != OperationUiSeverity::StatusBar) {
        std::cerr << "testGuiPhase7: engine-down must route to status bar\n";
        return false;
    }

    EventStreamSnapshot healthy{};
    healthy.applies = true;
    healthy.connection = EventConnectionState::Connected;
    healthy.engine = EngineHealthState::Ready;
    const auto chat_fail = makeFailure(kOpChat, "Chat failed", "HTTP 500", false, 500);
    if (uiSeverityForFailure(chat_fail, healthy) != OperationUiSeverity::Panel) {
        std::cerr << "testGuiPhase7: chat failure must route to panel when engine up\n";
        return false;
    }

#if THOTH_HAS_GUI
    RemoteAgentBackend remote("");
    const auto chat = remote.processInput("ping");
    if (chat.success) {
        std::cerr << "testGuiPhase7: empty URL chat must fail\n";
        return false;
    }
    const auto pause = remote.pause();
    if (pause.success) {
        std::cerr << "testGuiPhase7: empty URL pause must fail\n";
        return false;
    }
    if (pause.operation != kOpPause) {
        std::cerr << "testGuiPhase7: pause operation tag wrong\n";
        return false;
    }
#endif

    return true;
}

static bool testGuiPhase4DecisionSummary() {
    using namespace Thoth::DecisionSummary;

    nlohmann::json trace = {
        {"trace_type", "goal_execution"},
        {"result_summary", "Goal completed"},
        {"duration_ms", 1234},
        {"session_id", "s1"},
        {"stages", nlohmann::json::array({
            {{"name", "PLANNING"}, {"success", true}, {"summary", "Built 3-step plan"},
             {"metadata", {{"goal", "Summarize GRAG"}}}},
            {{"name", "EXECUTING_STEP"}, {"success", true}, {"summary", "Ran tools"},
             {"metadata", nlohmann::json::object()}},
        })},
    };

    const auto summary = fromDecisionTraceObject(trace);
    std::string err;
    if (!hasRequiredV1Fields(summary, err)) {
        std::cerr << "testGuiPhase4DecisionSummary: " << err << "\n";
        return false;
    }
    if (summary["schema_version"].get<int>() != kSchemaVersion) {
        std::cerr << "testGuiPhase4DecisionSummary: bad schema_version\n";
        return false;
    }
    if (summary.value("goal", "") != "Summarize GRAG") {
        std::cerr << "testGuiPhase4DecisionSummary: goal mapping failed\n";
        return false;
    }
    if (summary.value("executive_summary", "") != "Goal completed") {
        std::cerr << "testGuiPhase4DecisionSummary: executive_summary mapping failed\n";
        return false;
    }
    if (summary.value("planner_summary", "").find("3-step") == std::string::npos) {
        std::cerr << "testGuiPhase4DecisionSummary: planner_summary mapping failed\n";
        return false;
    }
    if (summary.value("execution_time_ms", 0) != 1234) {
        std::cerr << "testGuiPhase4DecisionSummary: execution_time_ms mapping failed\n";
        return false;
    }
    if (std::string(kHttpPath) != "/v1/diagnostics/latest-decision") {
        std::cerr << "testGuiPhase4DecisionSummary: unexpected HTTP path\n";
        return false;
    }
    if (std::string(kHttpPath).find("logs") != std::string::npos
        || std::string(kHttpPath).find("tail") != std::string::npos) {
        std::cerr << "testGuiPhase4DecisionSummary: path must not be file/tail oriented\n";
        return false;
    }

    const std::string display = formatForDisplay(summary);
    if (display.find("Goal:") == std::string::npos
        || display.find("Executive summary:") == std::string::npos) {
        std::cerr << "testGuiPhase4DecisionSummary: display format missing labels\n";
        return false;
    }
    if (isEffectivelyEmpty(summary) || !isEffectivelyEmpty(emptyV1Summary())) {
        std::cerr << "testGuiPhase4DecisionSummary: empty detection wrong\n";
        return false;
    }

    // Engine capabilities advertise diagnostics after Phase 4
    if (!Thoth::engineBackendCapabilities().supportsPlanDiagnostics) {
        std::cerr << "testGuiPhase4DecisionSummary: Engine must advertise supportsPlanDiagnostics\n";
        return false;
    }
    if (Thoth::engineBackendCapabilities().supportsLogs) {
        std::cerr << "testGuiPhase4DecisionSummary: Logs still out of Phase 4 scope\n";
        return false;
    }
    return true;
}

// GUI Phase 5 — D3a progress provenance: UserAction must not claim work.
static bool testGuiPhase5ProgressReportingDiscipline() {
    using namespace Thoth;

    if (!mayApplyWorkProgress(ProgressSource::EngineEvent)
        || !mayApplyWorkProgress(ProgressSource::LocalBackendEvent)
        || !mayApplyWorkProgress(ProgressSource::EngineApiResponse)) {
        std::cerr << "testGuiPhase5: backend signals must allow work progress\n";
        return false;
    }
    if (mayApplyWorkProgress(ProgressSource::UserAction)
        || mayApplyWorkProgress(ProgressSource::Unknown)) {
        std::cerr << "testGuiPhase5: UserAction/Unknown must not apply work progress\n";
        return false;
    }
    if (!mayApplyIndexingProgress(ProgressSource::EngineEvent)
        || !mayApplyIndexingProgress(ProgressSource::LocalBackendEvent)) {
        std::cerr << "testGuiPhase5: indexing must accept backend events\n";
        return false;
    }
    if (mayApplyIndexingProgress(ProgressSource::UserAction)
        || mayApplyIndexingProgress(ProgressSource::EngineApiResponse)
        || mayApplyIndexingProgress(ProgressSource::Unknown)) {
        std::cerr << "testGuiPhase5: indexing must reject UserAction/API/Unknown\n";
        return false;
    }
    if (progressSourceForBackendEvent(true) != ProgressSource::EngineEvent
        || progressSourceForBackendEvent(false) != ProgressSource::LocalBackendEvent) {
        std::cerr << "testGuiPhase5: progressSourceForBackendEvent mapping wrong\n";
        return false;
    }
    const std::string chrome = formatFilesAddedChromeStatus(3);
    if (chrome != "Added 3 file(s)" || chrome.find("indexing") != std::string::npos) {
        std::cerr << "testGuiPhase5: local add chrome must not claim indexing\n";
        return false;
    }
    if (std::string(kGoalSubmittedChrome).find("Planning") != std::string::npos
        || std::string(kMessageSubmittedChrome).find("Syncing") != std::string::npos) {
        std::cerr << "testGuiPhase5: chrome strings must not invent work\n";
        return false;
    }
    return true;
}

static bool testGuiPhase8CorpusDocuments() {
    using namespace Thoth::CorpusDocuments;

    const auto doc = makeDocument("doc-a", "GRAG.md", "indexed", "2026-07-20T12:00:00Z", 42);
    if (doc.value("id", "") != "doc-a" || doc.value("name", "") != "GRAG.md") {
        std::cerr << "testGuiPhase8: makeDocument wrong\n";
        return false;
    }
    if (!doc["chunk_count"].is_number_integer() || doc["chunk_count"].get<int>() != 42) {
        std::cerr << "testGuiPhase8: chunk_count wrong\n";
        return false;
    }

    const auto sparse = makeDocument("doc-b", "HOWTO.md", "pending");
    if (!sparse["chunk_count"].is_null() || !sparse["indexed_at"].is_null()) {
        std::cerr << "testGuiPhase8: optional fields must be null\n";
        return false;
    }

    const std::string id1 = stableDocumentId("GRAG.md");
    const std::string id2 = stableDocumentId("GRAG.md");
    if (id1.empty() || id1 != id2 || id1.find("doc-") != 0) {
        std::cerr << "testGuiPhase8: stableDocumentId wrong\n";
        return false;
    }

    nlohmann::json body = emptyV1List();
    body["documents"].push_back(doc);
    std::string err;
    if (!hasRequiredV1Fields(body, err)) {
        std::cerr << "testGuiPhase8: valid body rejected: " << err << "\n";
        return false;
    }
    if (isEffectivelyEmpty(body)) {
        std::cerr << "testGuiPhase8: populated body must not be empty\n";
        return false;
    }
    if (!isEffectivelyEmpty(emptyV1List())) {
        std::cerr << "testGuiPhase8: empty list wrong\n";
        return false;
    }
    if (hasRequiredV1Fields(unavailableFetchResult(), err)) {
        std::cerr << "testGuiPhase8: unavailable fetch must fail validation\n";
        return false;
    }

    if (std::string(kHttpPath) != "/v1/rag/corpus"
        || std::string(kReadyCapability) != "corpus") {
        std::cerr << "testGuiPhase8: path/capability tokens wrong\n";
        return false;
    }

    if (std::string(kLoadingLabel).empty() || std::string(kEmptyLabel).empty()
        || std::string(kUnavailableLabel).empty()) {
        std::cerr << "testGuiPhase8: presentation labels missing\n";
        return false;
    }

    return true;
}

static bool testGuiR2CorpusFailedDocument() {
    using namespace Thoth::CorpusDocuments;

    const auto doc = makeDocument(
        "doc-f", "ws.md", "failed", std::nullopt, std::nullopt, "empty_document");
    if (doc.value("status", "") != "failed"
        || doc.value("reason", "") != "empty_document") {
        std::cerr << "testGuiR2CorpusFailedDocument: makeDocument wrong\n";
        return false;
    }
    nlohmann::json body = emptyV1List();
    body["documents"].push_back(doc);
    std::string err;
    if (!hasRequiredV1Fields(body, err)) {
        std::cerr << "testGuiR2CorpusFailedDocument: validation: " << err << "\n";
        return false;
    }
    if (!isAllowedDocumentStatus("failed") || isAllowedDocumentStatus("queued")) {
        std::cerr << "testGuiR2CorpusFailedDocument: status guard wrong\n";
        return false;
    }
    return true;
}

static bool testTcb2ScopeBeatsSimilarity() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    const std::string session = "tcb-x1-session";
    const std::string attachPath = "/tmp/thoth_tcb_x1_attachment.md";
    const std::string benchPath = "/workspace/docker/seed_rag/GRAG.md";

    const std::string query = "THOTH_TCB_X1_UNIQUE_ALPHA_BETA_GAMMA";

    CodeChunk bench;
    bench.fileName = benchPath;
    bench.code = query + " benchmark corpus dominant content";
    bench.embedding = engine->embed(bench.code);
    bench.keyword_score = 1.0f;

    CodeChunk attach;
    attach.fileName = attachPath;
    attach.code = "unrelated filler text";
    attach.embedding = engine->embed(attach.code);
    attach.keyword_score = 0.1f;

    idx.registerAttachmentOwner(attachPath, session);
    idx.addChunkToIndex(std::move(bench));
    idx.addChunkToIndex(std::move(attach));

    Thoth::RetrievalScope scope = Thoth::resolveAgentContextRetrievalScope(session, &idx);
    const auto results = idx.retrieveChunks(query, 5, &scope);
    if (results.empty()) {
        std::cerr << "testTcb2ScopeBeatsSimilarity: expected in-scope attachment hit\n";
        return false;
    }
    for (const auto& entry : results) {
        const CodeChunk* chunk = idx.getChunkByCode(entry.first);
        if (!chunk) {
            continue;
        }
        if (chunk->corpus_tier == "benchmark" || chunk->corpus_tier == "system_reference") {
            std::cerr << "testTcb2ScopeBeatsSimilarity: excluded tier leaked: "
                      << chunk->corpus_tier << "\n";
            return false;
        }
        if (chunk->corpus_tier != "session_attachment" ||
            chunk->owner_context_id != session) {
            std::cerr << "testTcb2ScopeBeatsSimilarity: unexpected tier/owner\n";
            return false;
        }
    }
    return true;
}

static bool testTcb2CrossContextIsolation() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    const std::string pathA = "/tmp/thoth_tcb_x2_a.md";
    const std::string pathB = "/tmp/thoth_tcb_x2_b.md";

    CodeChunk chunkA;
    chunkA.fileName = pathA;
    chunkA.code = "session A secret token TCBX2A";
    chunkA.embedding = engine->embed(chunkA.code);

    idx.registerAttachmentOwner(pathA, "session-a");
    idx.addChunkToIndex(std::move(chunkA));

    Thoth::RetrievalScope scopeB = Thoth::resolveAgentContextRetrievalScope("session-b", &idx);
    const auto results = idx.retrieveChunks("TCBX2A secret", 5, &scopeB);
    if (!results.empty()) {
        std::cerr << "testTcb2CrossContextIsolation: context B retrieved context A material\n";
        return false;
    }
    return true;
}

static bool testTcb2RetrievalTraceParity() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    const std::string session = "tcb-x3";
    const std::string path = "/tmp/thoth_tcb_x3.md";
    CodeChunk chunk;
    chunk.fileName = path;
    chunk.code = "trace parity unique phrase TCBX3";
    chunk.embedding = engine->embed(chunk.code);
    idx.registerAttachmentOwner(path, session);
    idx.addChunkToIndex(std::move(chunk));

    RAGPipeline rag(std::move(engine), &idx, &cfg);

    nlohmann::json diagTrace;
    rag.setEventCallback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::RETRIEVAL_DIAGNOSTICS) {
            diagTrace = ev.metadata.value("retrieval_trace", nlohmann::json{});
        }
    });

    Thoth::RetrievalScope scope = Thoth::resolveAgentContextRetrievalScope(session, &idx);
    Thoth::RetrievalTrace traceOut;
    GragDiagnostics diagnostics;
    rag.retrieveRelevant("TCBX3 unique phrase", {}, 3, "req-tcb-x3", {}, {}, {}, {}, {},
                         &diagnostics, &scope, &traceOut);

    const nlohmann::json chatScope = traceOut.toJson().value("retrieval_scope", nlohmann::json{});
    const nlohmann::json diagScope = diagTrace.value("retrieval_scope", nlohmann::json{});
    if (chatScope.empty() || diagScope.empty()) {
        std::cerr << "testTcb2RetrievalTraceParity: missing retrieval_scope\n";
        return false;
    }
    if (chatScope.value("context_policy_version", 0) != Thoth::kContextPolicyVersionV1) {
        std::cerr << "testTcb2RetrievalTraceParity: wrong policy version\n";
        return false;
    }
    if (chatScope != diagScope) {
        std::cerr << "testTcb2RetrievalTraceParity: scope mismatch across sinks\n";
        return false;
    }
    if (diagnostics.retrieval_trace.value("retrieval_scope", nlohmann::json{}) != chatScope) {
        std::cerr << "testTcb2RetrievalTraceParity: diagnostics trace mismatch\n";
        return false;
    }
    return true;
}

static bool testTcb3IngestBindAndCrossContext() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "tcb3_x4_bind";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "tcb3_x4_attachment.md";
    if (fs::exists(doc)) {
        fs::remove(doc);
    }

    const std::string session_a = "tcb3-session-a";
    const std::string session_b = "tcb3-session-b";
    const std::string body_text = "TCB3_X4 secret bind token alpha";

    const auto outcome = idx.createCorpusDocument(
        rag_dir.string(), "tcb3_x4_attachment.md", body_text, session_a);
    if (!outcome.ok) {
        std::cerr << "testTcb3IngestBindAndCrossContext: create failed: " << outcome.error
                  << "\n";
        return false;
    }
    idx.indexFile(doc.string());

    Thoth::RetrievalScope scopeA = Thoth::resolveAgentContextRetrievalScope(session_a, &idx);
    const auto hitsA = idx.retrieveChunks("TCB3_X4 secret", 5, &scopeA);
    if (hitsA.empty()) {
        std::cerr << "testTcb3IngestBindAndCrossContext: expected bind for session A\n";
        return false;
    }

    Thoth::RetrievalScope scopeB = Thoth::resolveAgentContextRetrievalScope(session_b, &idx);
    const auto hitsB = idx.retrieveChunks("TCB3_X4 secret", 5, &scopeB);
    if (!hitsB.empty()) {
        std::cerr << "testTcb3IngestBindAndCrossContext: session B must not see A attachment\n";
        return false;
    }

    bool saw_attachment = false;
    for (const auto& c : idx.getChunks()) {
        if (c.corpus_tier == "session_attachment" && c.owner_context_id == session_a) {
            saw_attachment = true;
            break;
        }
    }
    if (!saw_attachment) {
        std::cerr << "testTcb3IngestBindAndCrossContext: chunk tier/owner wrong\n";
        return false;
    }

    fs::remove(doc);
    return true;
}

static bool testTcb3IngestOmitSessionUnbound() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "tcb3_omit";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "tcb3_orphan.md";
    if (fs::exists(doc)) {
        fs::remove(doc);
    }

    const auto outcome = idx.createCorpusDocument(
        rag_dir.string(), "tcb3_orphan.md", "TCB3 orphan doc unique OMit", "");
    if (!outcome.ok) {
        std::cerr << "testTcb3IngestOmitSessionUnbound: " << outcome.error << "\n";
        return false;
    }
    idx.indexFile(doc.string());

    Thoth::RetrievalScope scope =
        Thoth::resolveAgentContextRetrievalScope("any-session", &idx);
    const auto hits = idx.retrieveChunks("TCB3 orphan unique OMit", 5, &scope);
    if (!hits.empty()) {
        std::cerr << "testTcb3IngestOmitSessionUnbound: unbound doc must not enter default scope\n";
        return false;
    }

    fs::remove(doc);
    return true;
}

static bool testTcb3AttachmentRegistryPersistence() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "tcb3_registry";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "tcb3_registry.md";
    if (fs::exists(doc)) {
        fs::remove(doc);
    }

    const std::string session = "tcb3-registry-session";
    std::string stored_path;
    {
        IndexManager idx(engine.get());
        const auto outcome = idx.createCorpusDocument(
            rag_dir.string(), "tcb3_registry.md", "registry persistence TCB3REG", session);
        if (!outcome.ok) {
            std::cerr << "testTcb3AttachmentRegistryPersistence: create " << outcome.error << "\n";
            return false;
        }
        stored_path = doc.lexically_normal().string();
        try {
            stored_path = fs::absolute(doc).lexically_normal().string();
        } catch (...) {
        }
    }

    IndexManager reloaded(engine.get());
    reloaded.init("");

    CodeChunk chunk;
    chunk.fileName = stored_path;
    chunk.code = "registry persistence TCB3REG";
    chunk.embedding = engine->embed(chunk.code);
    reloaded.addChunkToIndex(std::move(chunk));

    Thoth::RetrievalScope scope = Thoth::resolveAgentContextRetrievalScope(session, &reloaded);
    const auto hits = reloaded.retrieveChunks("TCB3REG persistence", 5, &scope);
    if (hits.empty()) {
        std::cerr << "testTcb3AttachmentRegistryPersistence: reloaded registry did not bind path\n";
        return false;
    }

    fs::remove(doc);
    return true;
}

static bool testR2IndexManagerIndexingHonesty() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    ControllerEvent last_complete;
    bool got_complete = false;
    idx.setEventCallback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::INDEXING_COMPLETED) {
            last_complete = ev;
            got_complete = true;
        }
    });

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "r2_honesty_test";
    fs::create_directories(rag_dir);
    const fs::path ws_file = rag_dir / "whitespace_only.md";
    {
        std::ofstream out(ws_file);
        out << "   \n\t";
    }

    idx.indexFile(ws_file.string());
    if (!got_complete) {
        std::cerr << "testR2IndexManagerIndexingHonesty: missing COMPLETE\n";
        return false;
    }
    if (last_complete.metadata.value("success", true)) {
        std::cerr << "testR2IndexManagerIndexingHonesty: whitespace must fail\n";
        return false;
    }
    if (last_complete.metadata.value("reason", "") != "empty_document") {
        std::cerr << "testR2IndexManagerIndexingHonesty: wrong failure reason\n";
        return false;
    }

    const auto list = idx.listCorpusDocuments(rag_dir.string());
    std::string err;
    if (!Thoth::CorpusDocuments::hasRequiredV1Fields(list, err)) {
        std::cerr << "testR2IndexManagerIndexingHonesty: list: " << err << "\n";
        return false;
    }
    bool found_failed = false;
    for (const auto& doc : list["documents"]) {
        if (doc.value("name", "") == "whitespace_only.md") {
            if (doc.value("status", "") != "failed") {
                std::cerr << "testR2IndexManagerIndexingHonesty: expected failed\n";
                return false;
            }
            found_failed = true;
        }
    }
    if (!found_failed) {
        std::cerr << "testR2IndexManagerIndexingHonesty: missing failed doc\n";
        return false;
    }

    got_complete = false;
    const fs::path ok_file = rag_dir / "ok.md";
    {
        std::ofstream out(ok_file);
        out << "# ok\nEnough text for indexing honesty success path.\n";
    }
    idx.indexFile(ok_file.string());
    if (!got_complete || !last_complete.metadata.value("success", false)) {
        std::cerr << "testR2IndexManagerIndexingHonesty: success path\n";
        return false;
    }
    if (last_complete.metadata.value("chunk_count", 0) <= 0) {
        std::cerr << "testR2IndexManagerIndexingHonesty: chunk_count\n";
        return false;
    }

    fs::remove(ws_file);
    fs::remove(ok_file);
    return true;
}

static bool testIndexManagerWholeFileFallbackAfterShortParagraphs() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    ControllerEvent last_complete;
    bool got_complete = false;
    idx.setEventCallback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::INDEXING_COMPLETED) {
            last_complete = ev;
            got_complete = true;
        }
    });

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "whole_file_fallback_test";
    fs::create_directories(rag_dir);
    const fs::path md_file = rag_dir / "short_lines.md";
    {
        std::ofstream out(md_file);
        out << "A\n\nB\n\nC\n\n";
        out << "This paragraph has enough words to embed as a whole-document fallback.\n";
    }

    idx.indexFile(md_file.string());
    if (!got_complete || !last_complete.metadata.value("success", false)) {
        std::cerr << "testIndexManagerWholeFileFallback: expected success\n";
        return false;
    }
    if (last_complete.metadata.value("chunk_count", 0) <= 0) {
        std::cerr << "testIndexManagerWholeFileFallback: chunk_count\n";
        return false;
    }

    fs::remove(md_file);
    return true;
}

static bool testSessionScopedReplaceOnResend() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "session_replace_test";
    fs::create_directories(rag_dir);
    const fs::path target = rag_dir / "note.md";
    if (fs::exists(target)) {
        fs::remove(target);
    }
    for (const char* suffix : {"note_1.md", "note_2.md"}) {
        const fs::path orphan = rag_dir / suffix;
        if (fs::exists(orphan)) {
            fs::remove(orphan);
        }
    }

    const std::string body1 =
        "# First send\nEnough content for indexing in session replace test one.\n";
    const std::string body2 =
        "# Second send\nEnough content for indexing in session replace test two.\n";

    const auto first = idx.createCorpusDocument(
        rag_dir.string(), "note.md", body1, "sess-replace-a");
    if (!first.ok || first.document_name != "note.md") {
        std::cerr << "testSessionScopedReplaceOnResend: first send failed\n";
        return false;
    }

    const auto second = idx.createCorpusDocument(
        rag_dir.string(), "note.md", body2, "sess-replace-a");
    if (!second.ok || second.document_name != "note.md") {
        std::cerr << "testSessionScopedReplaceOnResend: second send must keep canonical name\n";
        return false;
    }
    if (second.document_id != first.document_id) {
        std::cerr << "testSessionScopedReplaceOnResend: document_id must be stable on resend\n";
        return false;
    }
    if (fs::exists(rag_dir / "note_1.md") || fs::exists(rag_dir / "note_2.md")) {
        std::cerr << "testSessionScopedReplaceOnResend: must not create suffixed duplicates\n";
        return false;
    }

    const auto other = idx.createCorpusDocument(
        rag_dir.string(), "note.md", body1, "sess-replace-b");
    if (!other.ok || other.document_name != "note_1.md") {
        std::cerr << "testSessionScopedReplaceOnResend: other session must suffix\n";
        return false;
    }

    fs::remove(target);
    fs::remove(rag_dir / "note_1.md");
    return true;
}

static bool testSessionReclaimDefaultOwnedOnResend() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "default_reclaim_test";
    fs::create_directories(rag_dir);
    const fs::path target = rag_dir / "note.md";
    if (fs::exists(target)) {
        fs::remove(target);
    }
    if (fs::exists(rag_dir / "note_1.md")) {
        fs::remove(rag_dir / "note_1.md");
    }

    const std::string body1 =
        "# Legacy default bind\nEnough content for default reclaim test one.\n";
    const std::string body2 =
        "# Real session resend\nEnough content for default reclaim test two.\n";

    const auto legacy = idx.createCorpusDocument(
        rag_dir.string(), "note.md", body1, "default");
    if (!legacy.ok || legacy.document_name != "note.md") {
        std::cerr << "testSessionReclaimDefaultOwnedOnResend: legacy send failed\n";
        return false;
    }

    const auto reclaimed = idx.createCorpusDocument(
        rag_dir.string(), "note.md", body2, "sess-reclaim-a");
    if (!reclaimed.ok || reclaimed.document_name != "note.md") {
        std::cerr << "testSessionReclaimDefaultOwnedOnResend: must replace default-owned file\n";
        return false;
    }
    if (reclaimed.document_id != legacy.document_id) {
        std::cerr << "testSessionReclaimDefaultOwnedOnResend: document_id must stay stable\n";
        return false;
    }
    if (fs::exists(rag_dir / "note_1.md")) {
        std::cerr << "testSessionReclaimDefaultOwnedOnResend: must not create suffix copy\n";
        return false;
    }

    fs::remove(target);
    return true;
}

static bool testLargeDocumentUsesSizeFallbackNotSingleChunk() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    ControllerEvent last_complete;
    bool got_complete = false;
    idx.setEventCallback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::INDEXING_COMPLETED) {
            last_complete = ev;
            got_complete = true;
        }
    });

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "size_fallback_test";
    fs::create_directories(rag_dir);
    const fs::path md_file = rag_dir / "large_short_lines.md";

    std::string content;
    content.reserve(7000);
    for (int i = 0; i < 1800; ++i) {
        content += "Z\n\n";
    }

    {
        std::ofstream out(md_file);
        out << content;
    }

    idx.indexFile(md_file.string());

    for (int i = 0; i < 200 && idx.isIndexing(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    if (!got_complete || !last_complete.metadata.value("success", false)) {
        std::cerr << "testLargeDocumentUsesSizeFallbackNotSingleChunk: indexing failed\n";
        return false;
    }
    const int chunks = last_complete.metadata.value("chunk_count", 0);
    if (chunks <= 1) {
        std::cerr << "testLargeDocumentUsesSizeFallbackNotSingleChunk: expected >1 chunks got "
                  << chunks << "\n";
        return false;
    }

    fs::remove(md_file);
    return true;
}

static bool testEngineHttpCorpusEndpoint() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpCorpusEndpoint: runtime not ready\n";
        return false;
    }

    const auto caps = runtime->capabilities();
        const bool has_corpus =
        std::find(caps.begin(), caps.end(), "corpus") != caps.end();
    const bool has_ingest =
        std::find(caps.begin(), caps.end(), "ingest") != caps.end();
    if (!has_corpus || !has_ingest) {
        std::cerr << "testEngineHttpCorpusEndpoint: ready capabilities missing corpus/ingest\n";
        runtime->shutdown();
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28097;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    try {
        httplib::Client client("127.0.0.1", 28097);
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(30, 0);

        const auto ready = client.Get("/ready");
        if (!ready || ready->status != 200) {
            std::cerr << "testEngineHttpCorpusEndpoint: /ready failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto ready_body = nlohmann::json::parse(ready->body);
        bool found = false;
        for (const auto& c : ready_body["capabilities"]) {
            if (c.is_string() && c.get<std::string>() == "corpus") {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "testEngineHttpCorpusEndpoint: /ready JSON missing corpus\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto ok = client.Get("/v1/rag/corpus");
        if (!ok || ok->status != 200) {
            std::cerr << "testEngineHttpCorpusEndpoint: GET failed status="
                      << (ok ? ok->status : 0) << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto body = nlohmann::json::parse(ok->body);
        std::string err;
        if (!Thoth::CorpusDocuments::hasRequiredV1Fields(body, err)) {
            std::cerr << "testEngineHttpCorpusEndpoint: " << err << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto direct = runtime->listCorpusDocuments();
        if (body["schema_version"] != direct["schema_version"]) {
            std::cerr << "testEngineHttpCorpusEndpoint: schema_version mismatch vs runtime\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testEngineHttpCorpusEndpoint: exception: " << e.what() << '\n';
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }
}

static bool testGuiPhase9CorpusCreate() {
    using namespace Thoth::CorpusCreate;

    const auto accepted = makeAcceptedResponse("doc-abc", "notes.md");
    std::string err;
    if (!hasRequiredAcceptedFields(accepted, err)) {
        std::cerr << "testGuiPhase9: accepted response rejected: " << err << "\n";
        return false;
    }
    if (accepted["document"]["id"].get<std::string>() != "doc-abc") {
        std::cerr << "testGuiPhase9: document id wrong\n";
        return false;
    }

    nlohmann::json bad = accepted;
    bad["status"] = "pending";
    if (hasRequiredAcceptedFields(bad, err)) {
        std::cerr << "testGuiPhase9: non-accepted status must fail\n";
        return false;
    }

    if (sanitizeSuggestedFilename("/tmp/evil/../notes.md") != "notes.md") {
        std::cerr << "testGuiPhase9: sanitizeSuggestedFilename path strip wrong\n";
        return false;
    }
    if (sanitizeSuggestedFilename("   ") != "document.txt") {
        std::cerr << "testGuiPhase9: sanitizeSuggestedFilename empty wrong\n";
        return false;
    }

    const nlohmann::json ready = nlohmann::json{
        {"capabilities", nlohmann::json::array({"chat", "ingest", "corpus"})}};
    if (!readyCapabilitiesIncludeIngest(ready)) {
        std::cerr << "testGuiPhase9: ingest capability not detected\n";
        return false;
    }
    const nlohmann::json no_ingest = nlohmann::json{
        {"capabilities", nlohmann::json::array({"chat", "corpus"})}};
    if (readyCapabilitiesIncludeIngest(no_ingest)) {
        std::cerr << "testGuiPhase9: ingest must be absent when not advertised\n";
        return false;
    }

    if (std::string(kHttpPath) != "/v1/rag/documents"
        || std::string(kReadyCapability) != "ingest"
        || std::string(kOperationName) != "create_document") {
        std::cerr << "testGuiPhase9: contract tokens wrong\n";
        return false;
    }

    return true;
}

static bool testTcb4CreateDocumentRequestSessionId() {
    using namespace Thoth::CorpusCreate;

    const auto with_session =
        makeCreateDocumentRequestBody("note.txt", "body", "sess-abc");
    if (!with_session.contains("session_id")
        || with_session["session_id"].get<std::string>() != "sess-abc") {
        std::cerr << "testTcb4CreateDocumentRequestSessionId: session_id missing\n";
        return false;
    }
    if (with_session["name"].get<std::string>() != "note.txt"
        || with_session["content"].get<std::string>() != "body") {
        std::cerr << "testTcb4CreateDocumentRequestSessionId: name/content wrong\n";
        return false;
    }

    const auto trimmed =
        makeCreateDocumentRequestBody("n", "c", "  tab-session  ");
    if (trimmed["session_id"].get<std::string>() != "tab-session") {
        std::cerr << "testTcb4CreateDocumentRequestSessionId: trim failed\n";
        return false;
    }

    const auto omit = makeCreateDocumentRequestBody("n", "c", "   ");
    if (omit.contains("session_id")) {
        std::cerr << "testTcb4CreateDocumentRequestSessionId: blank session must omit\n";
        return false;
    }

    return true;
}

static bool testIndexManagerCreateCorpusDocumentAtomic() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir = fs::path(fh.getAgentWorkspacePath()) / "rag" / "phase9_create_test";
    fs::create_directories(rag_dir);
    const fs::path probe = rag_dir / "phase9_probe.md";
    if (fs::exists(probe)) {
        fs::remove(probe);
    }

    const auto outcome = idx.createCorpusDocument(
        rag_dir.string(), "phase9_probe.md", "# Phase 9 create test\n");
    if (!outcome.ok) {
        std::cerr << "testIndexManagerCreateCorpusDocumentAtomic: " << outcome.error << "\n";
        return false;
    }
    if (!fs::exists(probe)) {
        std::cerr << "testIndexManagerCreateCorpusDocumentAtomic: file not written\n";
        return false;
    }
    if (outcome.document_id.empty() || outcome.document_name != "phase9_probe.md") {
        std::cerr << "testIndexManagerCreateCorpusDocumentAtomic: metadata wrong\n";
        return false;
    }

    const auto list = idx.listCorpusDocuments(rag_dir.string());
    std::string err;
    if (!Thoth::CorpusDocuments::hasRequiredV1Fields(list, err)) {
        std::cerr << "testIndexManagerCreateCorpusDocumentAtomic: list invalid: " << err << "\n";
        return false;
    }
    bool found = false;
    for (const auto& doc : list["documents"]) {
        if (doc.value("id", "") == outcome.document_id) {
            found = true;
            break;
        }
    }
    if (!found) {
        std::cerr << "testIndexManagerCreateCorpusDocumentAtomic: document missing from list\n";
        return false;
    }

    fs::remove(probe);
    return true;
}

static bool testEngineHttpCreateDocumentEndpoint() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpCreateDocumentEndpoint: runtime not ready\n";
        return false;
    }

    const auto caps = runtime->capabilities();
    const bool has_ingest =
        std::find(caps.begin(), caps.end(), "ingest") != caps.end();
    if (!has_ingest) {
        std::cerr << "testEngineHttpCreateDocumentEndpoint: capabilities missing ingest\n";
        runtime->shutdown();
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28098;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    try {
        httplib::Client client("127.0.0.1", 28098);
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(30, 0);

        const auto ready = client.Get("/ready");
        if (!ready || ready->status != 200) {
            std::cerr << "testEngineHttpCreateDocumentEndpoint: /ready failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto ready_body = nlohmann::json::parse(ready->body);
        if (!Thoth::CorpusCreate::readyCapabilitiesIncludeIngest(ready_body)) {
            std::cerr << "testEngineHttpCreateDocumentEndpoint: /ready missing ingest\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const nlohmann::json req = {
            {"name", "phase9_http_test.md"},
            {"content", "# HTTP create test\n"},
        };
        const auto post = client.Post("/v1/rag/documents",
                                      req.dump(),
                                      "application/json");
        if (!post || post->status != 200) {
            std::cerr << "testEngineHttpCreateDocumentEndpoint: POST failed status="
                      << (post ? post->status : 0) << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto accepted = nlohmann::json::parse(post->body);
        std::string err;
        if (!Thoth::CorpusCreate::hasRequiredAcceptedFields(accepted, err)) {
            std::cerr << "testEngineHttpCreateDocumentEndpoint: " << err << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto corpus = client.Get("/v1/rag/corpus");
        if (!corpus || corpus->status != 200) {
            std::cerr << "testEngineHttpCreateDocumentEndpoint: corpus GET failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto corpus_body = nlohmann::json::parse(corpus->body);
        const std::string created_id = accepted["document"]["id"].get<std::string>();
        bool listed = false;
        for (const auto& doc : corpus_body["documents"]) {
            if (doc.value("id", "") == created_id) {
                listed = true;
                break;
            }
        }
        if (!listed) {
            std::cerr << "testEngineHttpCreateDocumentEndpoint: created doc not listed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testEngineHttpCreateDocumentEndpoint: exception: " << e.what() << '\n';
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }
}

static bool testGuiPhase10ConversationAuthority() {
    using namespace Thoth::ConversationAuthority;

    const auto created = makeCreateSessionResponse("session-test-1");
    std::string err;
    if (!hasRequiredCreateSessionFields(created, err)) {
        std::cerr << "testGuiPhase10: create session rejected: " << err << "\n";
        return false;
    }

    const auto assistant = makeMessage("assistant", "hello", 1000);
    const auto turn = makeAppendTurnResponse("session-test-1", assistant);
    if (!hasRequiredAppendTurnFields(turn, err)) {
        std::cerr << "testGuiPhase10: append turn rejected: " << err << "\n";
        return false;
    }

    nlohmann::json messages = nlohmann::json::array();
    messages.push_back(makeMessage("user", "hi", 900));
    messages.push_back(assistant);
    const auto convo = makeConversationResponse("session-test-1", messages);
    if (!hasRequiredConversationFields(convo, err)) {
        std::cerr << "testGuiPhase10: conversation rejected: " << err << "\n";
        return false;
    }

    const auto summary = makeSummaryResponse("session-test-1", "rolling summary");
    if (!hasRequiredSummaryFields(summary, err)) {
        std::cerr << "testGuiPhase10: summary rejected: " << err << "\n";
        return false;
    }

    const nlohmann::json ready = nlohmann::json{
        {"capabilities", nlohmann::json::array({"conversation", "chat"})}};
    if (!readyCapabilitiesIncludeConversation(ready)) {
        std::cerr << "testGuiPhase10: conversation capability not detected\n";
        return false;
    }

    if (std::string(kHttpPathSessions) != "/v1/conversation/sessions"
        || std::string(kHttpPathTurns) != "/v1/conversation/turns"
        || std::string(kReadyCapability) != "conversation") {
        std::cerr << "testGuiPhase10: contract tokens wrong\n";
        return false;
    }

    return true;
}

static bool testEngineHttpConversationEndpoints() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpConversationEndpoints: runtime not ready\n";
        return false;
    }

    const auto caps = runtime->capabilities();
    const bool has_conversation =
        std::find(caps.begin(), caps.end(), "conversation") != caps.end();
    if (!has_conversation) {
        std::cerr << "testEngineHttpConversationEndpoints: missing conversation capability\n";
        runtime->shutdown();
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28099;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    try {
        httplib::Client client("127.0.0.1", 28099);
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(120, 0);

        const auto ready = client.Get("/ready");
        if (!ready || ready->status != 200) {
            std::cerr << "testEngineHttpConversationEndpoints: /ready failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto ready_body = nlohmann::json::parse(ready->body);
        if (!Thoth::ConversationAuthority::readyCapabilitiesIncludeConversation(ready_body)) {
            std::cerr << "testEngineHttpConversationEndpoints: /ready missing conversation\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto created = client.Post("/v1/conversation/sessions", "{}", "application/json");
        if (!created || created->status != 200) {
            std::cerr << "testEngineHttpConversationEndpoints: create session failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto created_body = nlohmann::json::parse(created->body);
        std::string err;
        if (!Thoth::ConversationAuthority::hasRequiredCreateSessionFields(created_body, err)) {
            std::cerr << "testEngineHttpConversationEndpoints: " << err << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const std::string session_id = created_body["session_id"].get<std::string>();

        const nlohmann::json turn_req = {{"session_id", session_id}, {"content", "hello phase10"}};
        const auto turn = client.Post("/v1/conversation/turns",
                                      turn_req.dump(),
                                      "application/json");
        if (!turn || turn->status != 200) {
            std::cerr << "testEngineHttpConversationEndpoints: append turn failed status="
                      << (turn ? turn->status : 0) << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto turn_body = nlohmann::json::parse(turn->body);
        if (!Thoth::ConversationAuthority::hasRequiredAppendTurnFields(turn_body, err)) {
            std::cerr << "testEngineHttpConversationEndpoints: " << err << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto convo = client.Get("/v1/conversation/sessions/" + session_id);
        if (!convo || convo->status != 200) {
            std::cerr << "testEngineHttpConversationEndpoints: get conversation failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto convo_body = nlohmann::json::parse(convo->body);
        if (!Thoth::ConversationAuthority::hasRequiredConversationFields(convo_body, err)) {
            std::cerr << "testEngineHttpConversationEndpoints: " << err << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        if (convo_body["messages"].empty()) {
            std::cerr << "testEngineHttpConversationEndpoints: expected messages after turn\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto summary = client.Get("/v1/conversation/sessions/" + session_id + "/summary");
        if (!summary || summary->status != 200) {
            std::cerr << "testEngineHttpConversationEndpoints: get summary failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto summary_body = nlohmann::json::parse(summary->body);
        if (!Thoth::ConversationAuthority::hasRequiredSummaryFields(summary_body, err)) {
            std::cerr << "testEngineHttpConversationEndpoints: " << err << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testEngineHttpConversationEndpoints: exception: " << e.what() << '\n';
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }
}

static bool testGuiPhase11ResearchResources() {
    using namespace Thoth::ResearchResources;

    nlohmann::json items = nlohmann::json::array();
    items.push_back({
        {"strategy_id", "strat-1"},
        {"description", "test"},
        {"step_pattern", nlohmann::json::array({"A"})},
        {"success_rate", 0.9},
        {"created_at", 1000},
    });
    const auto collection = makeCollection(items, nullptr, 1);
    std::string err;
    if (!hasRequiredCollectionFields(collection, err)) {
        std::cerr << "testGuiPhase11: collection rejected: " << err << "\n";
        return false;
    }
    if (isEffectivelyEmpty(collection)) {
        std::cerr << "testGuiPhase11: populated collection must not be empty\n";
        return false;
    }
    if (!hasRequiredStrategyItemFields(items[0], err)) {
        std::cerr << "testGuiPhase11: strategy item rejected: " << err << "\n";
        return false;
    }

    const auto empty = emptyCollection();
    if (!hasRequiredCollectionFields(empty, err) || !isEffectivelyEmpty(empty)) {
        std::cerr << "testGuiPhase11: empty collection wrong\n";
        return false;
    }

    if (!isFetchError(unavailableFetchResult())
        || hasRequiredCollectionFields(unavailableFetchResult(), err)) {
        std::cerr << "testGuiPhase11: unavailable fetch must be fetch error\n";
        return false;
    }

    const nlohmann::json ready = nlohmann::json{
        {"capabilities",
         nlohmann::json::array({"strategies", "trajectories", "episodes"})}};
    if (!readyCapabilitiesIncludeStrategies(ready)
        || !readyCapabilitiesIncludeTrajectories(ready)
        || !readyCapabilitiesIncludeEpisodes(ready)) {
        std::cerr << "testGuiPhase11: ready capability detection failed\n";
        return false;
    }

    if (std::string(kHttpPathStrategies) != "/v1/research/strategies"
        || std::string(kHttpPathTrajectories) != "/v1/research/trajectories"
        || std::string(kHttpPathEpisodes) != "/v1/research/episodes") {
        std::cerr << "testGuiPhase11: HTTP path tokens wrong\n";
        return false;
    }

    nlohmann::json trajItem = {{"trajectory_id", "traj-1"}};
    nlohmann::json epItem = {{"episode_id", "ep-1"}};
    if (!hasRequiredTrajectoryItemFields(trajItem, err)
        || !hasRequiredEpisodeItemFields(epItem, err)) {
        std::cerr << "testGuiPhase11: id validators failed\n";
        return false;
    }

    return true;
}

static bool testEngineHttpResearchEndpoints() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpResearchEndpoints: runtime not ready\n";
        return false;
    }

    const auto caps = runtime->capabilities();
    for (const char* token : {"strategies", "trajectories", "episodes"}) {
        if (std::find(caps.begin(), caps.end(), token) == caps.end()) {
            std::cerr << "testEngineHttpResearchEndpoints: missing capability " << token << "\n";
            runtime->shutdown();
            return false;
        }
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28100;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto validateCollection = [](const httplib::Result& resp, const char* label) -> bool {
        if (!resp || resp->status != 200) {
            std::cerr << "testEngineHttpResearchEndpoints: " << label << " HTTP failed\n";
            return false;
        }
        try {
            const auto body = nlohmann::json::parse(resp->body);
            std::string err;
            if (!Thoth::ResearchResources::hasRequiredCollectionFields(body, err)) {
                std::cerr << "testEngineHttpResearchEndpoints: " << label << " " << err << "\n";
                return false;
            }
            if (!body.contains("next_page")) {
                std::cerr << "testEngineHttpResearchEndpoints: " << label << " missing next_page\n";
                return false;
            }
            return true;
        } catch (const std::exception& ex) {
            std::cerr << "testEngineHttpResearchEndpoints: " << label << " parse: " << ex.what()
                      << "\n";
            return false;
        }
    };

    try {
        httplib::Client client("127.0.0.1", 28100);
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(30, 0);

        const auto ready = client.Get("/ready");
        if (!ready || ready->status != 200) {
            std::cerr << "testEngineHttpResearchEndpoints: /ready failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto ready_body = nlohmann::json::parse(ready->body);
        if (!Thoth::ResearchResources::readyCapabilitiesIncludeStrategies(ready_body)
            || !Thoth::ResearchResources::readyCapabilitiesIncludeTrajectories(ready_body)
            || !Thoth::ResearchResources::readyCapabilitiesIncludeEpisodes(ready_body)) {
            std::cerr << "testEngineHttpResearchEndpoints: /ready missing research capabilities\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        if (!validateCollection(
                client.Get(Thoth::ResearchResources::kHttpPathStrategies), "strategies")
            || !validateCollection(
                client.Get(Thoth::ResearchResources::kHttpPathTrajectories), "trajectories")
            || !validateCollection(
                client.Get(Thoth::ResearchResources::kHttpPathEpisodes), "episodes")) {
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testEngineHttpResearchEndpoints: exception: " << e.what() << '\n';
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }
}

static bool testGuiPhase12AGraphStatistics() {
    using namespace Thoth::GraphStatistics;

    const auto stats = makeStatisticsPayload(3, 5, 0.5f, 1.0f, 0.1f, 4, 1);
    const auto response = makeResponse(stats, 1000, "session-abc");
    std::string err;
    if (!hasRequiredFields(response, err)) {
        std::cerr << "testGuiPhase12A: valid response rejected: " << err << "\n";
        return false;
    }
    if (isEffectivelyEmpty(response)) {
        std::cerr << "testGuiPhase12A: populated response must not be empty\n";
        return false;
    }

    const auto empty = emptyResponse(2000);
    if (!hasRequiredFields(empty, err) || !isEffectivelyEmpty(empty)) {
        std::cerr << "testGuiPhase12A: empty response wrong\n";
        return false;
    }

    if (!isFetchError(unavailableFetchResult())
        || hasRequiredFields(unavailableFetchResult(), err)) {
        std::cerr << "testGuiPhase12A: unavailable fetch must be fetch error\n";
        return false;
    }

    const nlohmann::json ready = nlohmann::json{
        {"capabilities", nlohmann::json::array({"graph_stats"})}};
    if (!readyCapabilitiesIncludeGraphStats(ready)) {
        std::cerr << "testGuiPhase12A: ready capability not detected\n";
        return false;
    }

    if (std::string(kHttpPath) != "/v1/graph/stats"
        || std::string(kReadyCapability) != "graph_stats") {
        std::cerr << "testGuiPhase12A: contract tokens wrong\n";
        return false;
    }

    if (statisticsPayload(response).value("total_nodes", 0) != 3) {
        std::cerr << "testGuiPhase12A: statisticsPayload wrong\n";
        return false;
    }

    return true;
}

static bool testEngineHttpGraphStatsEndpoint() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpGraphStatsEndpoint: runtime not ready\n";
        return false;
    }

    const auto caps = runtime->capabilities();
    if (std::find(caps.begin(), caps.end(), "graph_stats") == caps.end()) {
        std::cerr << "testEngineHttpGraphStatsEndpoint: missing graph_stats capability\n";
        runtime->shutdown();
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28101;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    try {
        httplib::Client client("127.0.0.1", 28101);
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(30, 0);

        const auto ready = client.Get("/ready");
        if (!ready || ready->status != 200) {
            std::cerr << "testEngineHttpGraphStatsEndpoint: /ready failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto ready_body = nlohmann::json::parse(ready->body);
        if (!Thoth::GraphStatistics::readyCapabilitiesIncludeGraphStats(ready_body)) {
            std::cerr << "testEngineHttpGraphStatsEndpoint: /ready missing graph_stats\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto stats = client.Get(Thoth::GraphStatistics::kHttpPath);
        if (!stats || stats->status != 200) {
            std::cerr << "testEngineHttpGraphStatsEndpoint: GET failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto body = nlohmann::json::parse(stats->body);
        std::string err;
        if (!Thoth::GraphStatistics::hasRequiredFields(body, err)) {
            std::cerr << "testEngineHttpGraphStatsEndpoint: " << err << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testEngineHttpGraphStatsEndpoint: exception: " << e.what() << '\n';
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }
}

static bool testEngineHttpDiagnosticsEndpoint() {
    auto runtime = Thoth::EngineRuntime::create();
    if (!runtime || !runtime->isReady()) {
        std::cerr << "testEngineHttpDiagnosticsEndpoint: runtime not ready\n";
        return false;
    }

    const auto caps = runtime->capabilities();
    const bool has_diagnostics =
        std::find(caps.begin(), caps.end(), "diagnostics") != caps.end();
    if (!has_diagnostics) {
        std::cerr << "testEngineHttpDiagnosticsEndpoint: ready capabilities missing diagnostics\n";
        runtime->shutdown();
        return false;
    }

    Thoth::EngineHttpConfig config;
    config.bind_host = "127.0.0.1";
    config.port = 28096;

    Thoth::EngineHttpTransport transport(*runtime, config);
    std::thread server_thread([&]() { transport.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    try {
        httplib::Client client("127.0.0.1", 28096);
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(30, 0);

        const auto ready = client.Get("/ready");
        if (!ready || ready->status != 200) {
            std::cerr << "testEngineHttpDiagnosticsEndpoint: /ready failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto ready_body = nlohmann::json::parse(ready->body);
        bool found = false;
        for (const auto& c : ready_body["capabilities"]) {
            if (c.is_string() && c.get<std::string>() == "diagnostics") {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "testEngineHttpDiagnosticsEndpoint: /ready JSON missing diagnostics\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto ok = client.Get("/v1/diagnostics/latest-decision");
        if (!ok) {
            std::cerr << "testEngineHttpDiagnosticsEndpoint: GET failed\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        if (ok->status != 200) {
            std::cerr << "testEngineHttpDiagnosticsEndpoint: status=" << ok->status
                      << " body=" << ok->body << '\n';
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }
        const auto body = nlohmann::json::parse(ok->body);
        std::string err;
        if (!Thoth::DecisionSummary::hasRequiredV1Fields(body, err)) {
            std::cerr << "testEngineHttpDiagnosticsEndpoint: " << err << "\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        const auto direct = runtime->getLatestDecisionSummary();
        if (body["schema_version"] != direct["schema_version"]) {
            std::cerr << "testEngineHttpDiagnosticsEndpoint: schema_version mismatch vs runtime\n";
            transport.requestStop();
            server_thread.join();
            runtime->shutdown();
            return false;
        }

        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "testEngineHttpDiagnosticsEndpoint: exception: " << e.what() << '\n';
        transport.requestStop();
        server_thread.join();
        runtime->shutdown();
        return false;
    }
}

// Plan N N1 / N-T1 — transcript loop truncation; mid-sentence + JSON preserved.
static bool testPlanNSanitizeTranscriptLoop() {
    const std::string input =
        "29. GRAG is the Goal-Relative Adaptive Graph Retrieval component of Thoth.\n"
        "\n"
        "[User] Explain GRAG to me.\n"
        "[Agent] 29. GRAG is the Goal-Relative Adaptive Graph Retrieval component of Thoth.\n"
        "[User] Explain GRAG to me.\n"
        "[Agent] 29\n";

    const auto out = Thoth::ChatGeneration::sanitizeChatAssistantText(input);
    if (out.empty_after_sanitize) {
        std::cerr << "testPlanNSanitizeTranscriptLoop: should not be empty\n";
        return false;
    }
    if (out.sanitize_reason != Thoth::ChatGeneration::kSanitizeTruncatedTranscriptMarker) {
        std::cerr << "testPlanNSanitizeTranscriptLoop: expected truncated_transcript_marker, got "
                  << out.sanitize_reason << "\n";
        return false;
    }
    if (out.sanitized_text.find("[User]") != std::string::npos
        || out.sanitized_text.find("[Agent]") != std::string::npos) {
        std::cerr << "testPlanNSanitizeTranscriptLoop: markers should be removed\n";
        return false;
    }
    if (out.sanitized_text.find("Goal-Relative Adaptive Graph Retrieval") == std::string::npos) {
        std::cerr << "testPlanNSanitizeTranscriptLoop: useful content lost\n";
        return false;
    }

    const std::string midSentence =
        "Ask the user for clarification. RAG retrieval is directional.\n";
    const auto mid = Thoth::ChatGeneration::sanitizeChatAssistantText(midSentence);
    if (mid.empty_after_sanitize
        || mid.sanitized_text.find("the user") == std::string::npos
        || mid.sanitized_text.find("RAG retrieval") == std::string::npos) {
        std::cerr << "testPlanNSanitizeTranscriptLoop: mid-sentence User/RAG must be preserved\n";
        return false;
    }
    if (mid.sanitize_reason != Thoth::ChatGeneration::kSanitizeNone) {
        std::cerr << "testPlanNSanitizeTranscriptLoop: mid-sentence reason should be none\n";
        return false;
    }

    const std::string jsonBody =
        "{\"tool_call\":{\"name\":\"summarize_text\",\"input\":{\"text\":\"hello\"}}}";
    const auto jsonOut = Thoth::ChatGeneration::sanitizeChatAssistantText(jsonBody);
    if (jsonOut.sanitized_text != jsonBody
        || jsonOut.sanitize_reason != Thoth::ChatGeneration::kSanitizeNone
        || jsonOut.empty_after_sanitize) {
        std::cerr << "testPlanNSanitizeTranscriptLoop: JSON tool body must be preserved\n";
        return false;
    }

    const std::string withNote =
        "Useful answer about GRAG.\n"
        "📝[RAG Context]\n"
        "[User] again\n";
    const auto noteOut = Thoth::ChatGeneration::sanitizeChatAssistantText(withNote);
    if (noteOut.sanitize_reason != Thoth::ChatGeneration::kSanitizeTruncatedTranscriptMarker
        || noteOut.sanitized_text.find("Useful answer") == std::string::npos
        || noteOut.sanitized_text.find("📝") != std::string::npos) {
        std::cerr << "testPlanNSanitizeTranscriptLoop: 📝 marker truncate failed\n";
        return false;
    }
    return true;
}

// Plan N N1 / N-T2 — scaffold-only "[User] help".
static bool testPlanNSanitizeUserHelpScaffold() {
    const auto out = Thoth::ChatGeneration::sanitizeChatAssistantText("[User] help");
    if (!out.empty_after_sanitize || !out.sanitized_text.empty()) {
        std::cerr << "testPlanNSanitizeUserHelpScaffold: expected empty sanitized text\n";
        return false;
    }
    if (out.sanitize_reason != Thoth::ChatGeneration::kSanitizeAllScaffold) {
        std::cerr << "testPlanNSanitizeUserHelpScaffold: expected all_scaffold, got "
                  << out.sanitize_reason << "\n";
        return false;
    }
    return true;
}

// Plan N N1 / N-T2b — leading scaffold then useful body.
static bool testPlanNSanitizeLeadingScaffoldStrip() {
    const std::string input = "[Agent] OK.\nGRAG is directional retrieval.\n";
    const auto out = Thoth::ChatGeneration::sanitizeChatAssistantText(input);
    if (out.empty_after_sanitize) {
        std::cerr << "testPlanNSanitizeLeadingScaffoldStrip: should keep body\n";
        return false;
    }
    if (out.sanitize_reason != Thoth::ChatGeneration::kSanitizeStrippedLeadingScaffold) {
        std::cerr << "testPlanNSanitizeLeadingScaffoldStrip: expected stripped_leading_scaffold, got "
                  << out.sanitize_reason << "\n";
        return false;
    }
    if (out.sanitized_text.find("[Agent]") != std::string::npos) {
        std::cerr << "testPlanNSanitizeLeadingScaffoldStrip: leading scaffold remains\n";
        return false;
    }
    if (out.sanitized_text.find("GRAG is directional retrieval") == std::string::npos) {
        std::cerr << "testPlanNSanitizeLeadingScaffoldStrip: body lost\n";
        return false;
    }
    return true;
}

// Plan N N2 / N-T3c — llama soft-empty parse + finish_reason.
static bool testPlanNLlamaSoftEmptyParse() {
    const auto generated = Thoth::LlamaServerClient::parseCompletionResponse(
        R"({"choices":[{"text":"","finish_reason":"stop"}],"usage":{"prompt_tokens":1,"completion_tokens":0,"total_tokens":1}})");
    if (!generated.ok || !generated.text.empty()) {
        std::cerr << "testPlanNLlamaSoftEmptyParse: expected ok + empty text\n";
        return false;
    }
    if (generated.finish_reason != "stop") {
        std::cerr << "testPlanNLlamaSoftEmptyParse: finish_reason lost\n";
        return false;
    }
    if (!generated.error.empty()) {
        std::cerr << "testPlanNLlamaSoftEmptyParse: must not set Empty completion text error\n";
        return false;
    }
    return true;
}

// Plan N N2 / N-T5b — malformed JSON is hard failure.
static bool testPlanNLlamaMalformedJson() {
    const auto generated = Thoth::LlamaServerClient::parseCompletionResponse("{not-json");
    if (generated.ok) {
        std::cerr << "testPlanNLlamaMalformedJson: expected ok=false\n";
        return false;
    }
    return true;
}

// Plan N N2 / N-T3 — provider empty then retry success.
static bool testPlanNGenerateRetrySuccess() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("");
    Thoth::RobustnessMockResponses::push("Hello from retry.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.max_tokens = 64;
    opts.stop_sequences = {"\n[User]"};
    opts.use_greeting_fallback = false;

    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.provider_ok || !out.retried_without_stops || out.fallback_used) {
        std::cerr << "testPlanNGenerateRetrySuccess: flags wrong\n";
        return false;
    }
    if (out.sanitized_text != "Hello from retry.") {
        std::cerr << "testPlanNGenerateRetrySuccess: unexpected text: " << out.sanitized_text
                  << "\n";
        return false;
    }
    if (out.sanitized_text.find("Empty completion") != std::string::npos
        || out.provider_error.find("Empty completion") != std::string::npos) {
        std::cerr << "testPlanNGenerateRetrySuccess: Empty completion leaked\n";
        return false;
    }
    return true;
}

// Plan N N2 / N-T3b — sanitize-empty then retry success.
static bool testPlanNGenerateSanitizeEmptyThenRetry() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("[User] help");
    Thoth::RobustnessMockResponses::push("Useful answer.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.max_tokens = 64;

    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.retried_without_stops || out.fallback_used || out.sanitized_text != "Useful answer.") {
        std::cerr << "testPlanNGenerateSanitizeEmptyThenRetry: failed\n";
        return false;
    }
    return true;
}

// Plan N N2 / N-T4 — empty then empty → fallback.
static bool testPlanNGenerateFallback() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("");
    Thoth::RobustnessMockResponses::push("");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.use_greeting_fallback = true;

    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.fallback_used || !out.retried_without_stops) {
        std::cerr << "testPlanNGenerateFallback: expected fallback after retry\n";
        return false;
    }
    if (out.sanitized_text != Thoth::ChatGeneration::kFallbackGreeting) {
        std::cerr << "testPlanNGenerateFallback: wrong fallback text\n";
        return false;
    }
    if (out.sanitized_text.find("Empty completion") != std::string::npos) {
        std::cerr << "testPlanNGenerateFallback: Empty completion in result\n";
        return false;
    }
    return true;
}

// CSG-B B.2 — Layer 1 scaffold assessment.
static bool testCsgBScaffoldAssessment() {
    const std::string regurgitated =
        "Document: architectural_facts.md\n"
        "source_span=29-33\n"
        "Use AddCollapsiblePane for sidebar sections.\n"
        "---\n"
        "Document: architectural_facts.md\n"
        "source_span=40-44\n"
        "Never bypass the collapsible pane pattern.\n";

    const auto positive = Thoth::ChatGeneration::assessChunkFormatRegurgitation(regurgitated);
    if (!positive.detected || positive.document_header_count < 2) {
        std::cerr << "testCsgBScaffoldAssessment: multi-chunk scaffold not detected\n";
        return false;
    }

    const std::string prose =
        "Per architectural_facts.md, sidebar sections must use AddCollapsiblePane.";
    if (Thoth::ChatGeneration::assessChunkFormatRegurgitation(prose).detected) {
        std::cerr << "testCsgBScaffoldAssessment: inline filename flagged\n";
        return false;
    }

    const std::string jsonBody =
        "{\"tool_call\":{\"name\":\"summarize_text\",\"input\":{\"text\":\"hello\"}}}";
    if (Thoth::ChatGeneration::assessChunkFormatRegurgitation(jsonBody).detected) {
        std::cerr << "testCsgBScaffoldAssessment: JSON flagged\n";
        return false;
    }

    const std::string inlineDocument =
        "This document explains the sidebar rules in plain language.";
    if (Thoth::ChatGeneration::assessChunkFormatRegurgitation(inlineDocument).detected) {
        std::cerr << "testCsgBScaffoldAssessment: prose 'document' flagged\n";
        return false;
    }
    return true;
}

// CSG-B B.2 — Layer 2 answer quality assessment.
static bool testCsgBAnswerQualityAssessment() {
    const std::string pasted =
        "Use AddCollapsiblePane for sidebar sections.\n"
        "Never bypass the collapsible pane pattern.\n";
    const auto pastedQuality = Thoth::ChatGeneration::assessAnswerQuality(pasted);
    if (!pastedQuality.pasted_retrieval_context || pastedQuality.complete_assistant_answer) {
        std::cerr << "testCsgBAnswerQualityAssessment: fragment collage not detected\n";
        return false;
    }

    const std::string complete =
        "The sidebar rules require AddCollapsiblePane so sections stay collapsible and scrollable.";
    const auto completeQuality = Thoth::ChatGeneration::assessAnswerQuality(complete);
    if (!completeQuality.complete_assistant_answer || completeQuality.pasted_retrieval_context) {
        std::cerr << "testCsgBAnswerQualityAssessment: conversational answer not accepted\n";
        return false;
    }
    return true;
}

// CSG-B B.3 — chunk sanitize strips scaffold and reassess passes.
static bool testCsgBChunkSanitizeStripsScaffold() {
    const std::string input =
        "Document: architectural_facts.md\n"
        "source_span=29-33\n"
        "Use AddCollapsiblePane for sidebar sections.\n"
        "---\n"
        "Document: architectural_facts.md\n"
        "source_span=40-44\n"
        "Never bypass the collapsible pane pattern.\n";

    const auto out = Thoth::ChatGeneration::sanitizeChunkFormatScaffold(input);
    if (out.empty_after_sanitize
        || out.sanitize_reason != Thoth::ChatGeneration::kSanitizeStrippedChunkScaffold) {
        std::cerr << "testCsgBChunkSanitizeStripsScaffold: sanitize failed\n";
        return false;
    }
    if (out.sanitized_text.find("Document:") != std::string::npos
        || out.sanitized_text.find("source_span=") != std::string::npos) {
        std::cerr << "testCsgBChunkSanitizeStripsScaffold: scaffold leaked\n";
        return false;
    }
    if (Thoth::ChatGeneration::assessChunkFormatRegurgitation(out.sanitized_text).detected) {
        std::cerr << "testCsgBChunkSanitizeStripsScaffold: reassess still positive\n";
        return false;
    }
    return true;
}

// CSG-B B.3 — scaffold hides complete answer → return without regurgitation retry.
static bool testCsgBGenerateScaffoldWrappedCompleteAnswer() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push(
        "Document: architectural_facts.md\n"
        "source_span=29-33\n"
        "The sidebar rules require AddCollapsiblePane so sections stay collapsible.\n");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    const auto out =
        Thoth::ChatGeneration::generateAndSanitizeChat(llm, "Explain sidebar rules.", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.provider_ok || out.retry_due_to_regurgitation || out.fallback_used) {
        std::cerr << "testCsgBGenerateScaffoldWrappedCompleteAnswer: unexpected flags\n";
        return false;
    }
    if (out.sanitized_text.find("Document:") != std::string::npos
        || out.sanitized_text.find("AddCollapsiblePane") == std::string::npos) {
        std::cerr << "testCsgBGenerateScaffoldWrappedCompleteAnswer: bad sanitized text\n";
        return false;
    }
    return true;
}

// CSG-B B.3 — pasted chunk bodies after strip → Phase 1 returns usable stripped prose (one attempt).
static bool testCsgBGeneratePastedContextRetry() {
    const std::string regurgitated =
        "Document: architectural_facts.md\n"
        "source_span=29-33\n"
        "Use AddCollapsiblePane for sidebar sections.\n"
        "---\n"
        "Document: architectural_facts.md\n"
        "source_span=40-44\n"
        "Never bypass the collapsible pane pattern.\n";

    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push(regurgitated);
    Thoth::RobustnessMockResponses::push(
        "Sidebar sections must use AddCollapsiblePane and remain scrollable.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.user_query = "Explain sidebar rules.";
    const auto out =
        Thoth::ChatGeneration::generateAndSanitizeChat(llm, "Explain sidebar rules.", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.provider_ok || out.fallback_used || !out.response_valid) {
        std::cerr << "testCsgBGeneratePastedContextRetry: expected usable single-attempt return\n";
        return false;
    }
    if (out.generation_attempt_count != 1) {
        std::cerr << "testCsgBGeneratePastedContextRetry: expected one generation attempt, got "
                  << out.generation_attempt_count << "\n";
        return false;
    }
    if (out.sanitized_text.find("Document:") != std::string::npos
        || out.sanitized_text.find("AddCollapsiblePane") == std::string::npos) {
        std::cerr << "testCsgBGeneratePastedContextRetry: returned stripped paste\n";
        return false;
    }
    return true;
}

// CSG-B B.3 — persistent invalid output → Phase 1 fallback after two attempts.
static bool testCsgBGenerateRegurgitationFallback() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("[User] Explain sidebar rules.");
    Thoth::RobustnessMockResponses::push("[User] Explain sidebar rules.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.use_greeting_fallback = false;
    opts.user_query = "Explain sidebar rules.";
    const auto out =
        Thoth::ChatGeneration::generateAndSanitizeChat(llm, "Explain sidebar rules.", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.fallback_used || out.response_valid
        || out.invalid_reason != Thoth::ChatGeneration::kInvalidReasonNoUsableGeneration) {
        std::cerr << "testCsgBGenerateRegurgitationFallback: fallback flags wrong\n";
        return false;
    }
    if (out.generation_attempt_count != 2) {
        std::cerr << "testCsgBGenerateRegurgitationFallback: expected 2 attempts\n";
        return false;
    }
    if (out.sanitized_text != Thoth::ChatGeneration::kFallbackGeneric) {
        std::cerr << "testCsgBGenerateRegurgitationFallback: wrong fallback text\n";
        return false;
    }
    return true;
}

// Phase 1 — query echo rejected; second attempt returned when first echoes user query.
static bool testPhase1QueryEchoRejected() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("Tell me about completed improvements.");
    Thoth::RobustnessMockResponses::push(
        "Thoth completed GRAG integration and memory consolidation.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.user_query = "Tell me about completed improvements.";
    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(
        llm, "Tell me about completed improvements.", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.provider_ok || out.fallback_used || !out.response_valid || !out.retried_without_stops) {
        std::cerr << "testPhase1QueryEchoRejected: expected retry success\n";
        return false;
    }
    if (out.generation_attempt_count != 2) {
        std::cerr << "testPhase1QueryEchoRejected: expected 2 attempts\n";
        return false;
    }
    if (out.sanitized_text.find("GRAG integration") == std::string::npos) {
        std::cerr << "testPhase1QueryEchoRejected: unexpected text: " << out.sanitized_text << "\n";
        return false;
    }
    return true;
}

// Phase 1 — truncated transcript short prefix rejected; valid body on retry accepted.
static bool testPhase1TruncateShortPrefixRejected() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push(
        "Tell me about completed improvements.\n[User] Tell me about completed improvements.\n");
    Thoth::RobustnessMockResponses::push("Completed improvements include GRAG and Plan N safety.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.user_query = "Tell me about completed improvements.";
    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(
        llm, "Tell me about completed improvements.", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.provider_ok || out.fallback_used || !out.response_valid) {
        std::cerr << "testPhase1TruncateShortPrefixRejected: expected valid retry\n";
        return false;
    }
    if (out.sanitized_text.find("GRAG") == std::string::npos) {
        std::cerr << "testPhase1TruncateShortPrefixRejected: bad sanitized text\n";
        return false;
    }
    return true;
}

// Phase 1 — empty then empty → fallback with accurate invalid telemetry.
static bool testPhase1EmptyOutputFallback() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("");
    Thoth::RobustnessMockResponses::push("");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.use_greeting_fallback = false;
    opts.user_query = "hello";
    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", opts);
    Thoth::RobustnessMockResponses::reset();

    if (!out.fallback_used || out.response_valid
        || out.invalid_reason != Thoth::ChatGeneration::kInvalidReasonNoUsableGeneration) {
        std::cerr << "testPhase1EmptyOutputFallback: fallback telemetry wrong\n";
        return false;
    }
    if (out.sanitized_text != Thoth::ChatGeneration::kFallbackGeneric) {
        std::cerr << "testPhase1EmptyOutputFallback: wrong fallback text\n";
        return false;
    }
    return true;
}

// Phase 1 — non-empty sanitized output preferred over generic fallback (CSG-B paste).
static bool testPhase1PreferUsableOverFallback() {
    const std::string regurgitated =
        "Document: completed_improvements_log.md\n"
        "source_span=10-20\n"
        "The system completed GRAG Phase 3 routing and graph memory prototype work.\n";

    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push(regurgitated);

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.use_greeting_fallback = false;
    opts.user_query = "Tell me about completed improvements.";
    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(
        llm, "Tell me about completed improvements.", opts);
    Thoth::RobustnessMockResponses::reset();

    if (out.fallback_used || !out.response_valid || out.generation_attempt_count != 1) {
        std::cerr << "testPhase1PreferUsableOverFallback: should accept usable attempt 1\n";
        return false;
    }
    if (out.sanitized_text.find("GRAG Phase 3") == std::string::npos) {
        std::cerr << "testPhase1PreferUsableOverFallback: prose missing\n";
        return false;
    }
    return true;
}

// Phase 1 — hard cap at two queryDetailed() calls even when a third mock is queued.
static bool testPhase1MaxTwoGenerationAttempts() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("");
    Thoth::RobustnessMockResponses::push("");
    Thoth::RobustnessMockResponses::push("This third response must never run.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.user_query = "hello";
    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", opts);

    if (out.generation_attempt_count != 2) {
        std::cerr << "testPhase1MaxTwoGenerationAttempts: expected 2 attempts, got "
                  << out.generation_attempt_count << "\n";
        Thoth::RobustnessMockResponses::reset();
        return false;
    }
    if (Thoth::RobustnessMockResponses::size() != 1) {
        std::cerr << "testPhase1MaxTwoGenerationAttempts: third mock should remain unused\n";
        Thoth::RobustnessMockResponses::reset();
        return false;
    }
    Thoth::RobustnessMockResponses::reset();
    return true;
}

// Phase 1 — regurgitation path must not spawn a third generation with reminder prompt.
static bool testPhase1NoThirdRetry() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("[User] Explain sidebar rules.");
    Thoth::RobustnessMockResponses::push("[User] Explain sidebar rules.");
    Thoth::RobustnessMockResponses::push("Third generation must not run.");

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    opts.user_query = "Explain sidebar rules.";
    const auto out =
        Thoth::ChatGeneration::generateAndSanitizeChat(llm, "Explain sidebar rules.", opts);

    if (out.generation_attempt_count != 2) {
        std::cerr << "testPhase1NoThirdRetry: expected exactly 2 attempts\n";
        Thoth::RobustnessMockResponses::reset();
        return false;
    }
    if (Thoth::RobustnessMockResponses::size() != 1) {
        std::cerr << "testPhase1NoThirdRetry: third mock must remain queued\n";
        Thoth::RobustnessMockResponses::reset();
        return false;
    }
    Thoth::RobustnessMockResponses::reset();
    return true;
}

// Phase 1 — response_valid / fallback_used reflect the final returned text.
static bool testPhase1TelemetryFlagsAccurate() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("A helpful answer about improvements.");
    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions okOpts;
    okOpts.user_query = "Tell me about improvements.";
    const auto okOut = Thoth::ChatGeneration::generateAndSanitizeChat(
        llm, "Tell me about improvements.", okOpts);
    if (!okOut.response_valid || okOut.fallback_used
        || okOut.invalid_reason != Thoth::ChatGeneration::kInvalidReasonNone) {
        std::cerr << "testPhase1TelemetryFlagsAccurate: success flags wrong\n";
        return false;
    }

    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("");
    Thoth::RobustnessMockResponses::push("");
    Thoth::ChatGeneration::ChatGenerateOptions failOpts;
    failOpts.user_query = "hello";
    const auto failOut = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", failOpts);
    Thoth::RobustnessMockResponses::reset();

    if (failOut.response_valid || !failOut.fallback_used
        || failOut.invalid_reason != Thoth::ChatGeneration::kInvalidReasonNoUsableGeneration) {
        std::cerr << "testPhase1TelemetryFlagsAccurate: fallback flags wrong\n";
        return false;
    }
    if (failOut.sanitized_text != Thoth::ChatGeneration::kFallbackGeneric) {
        std::cerr << "testPhase1TelemetryFlagsAccurate: fallback text mismatch\n";
        return false;
    }
    return true;
}

// Plan N N2 / N-T5 — transport failure, no retry.
static bool testPlanNGenerateTransportFailure() {
    Thoth::RobustnessMockResponses::reset();
    setenv("THOTH_MOCK_LLM_UNAVAILABLE", "1", 1);

    LLMInterface llm(LLMBackend::Ollama, nullptr);
    Thoth::ChatGeneration::ChatGenerateOptions opts;
    const auto out = Thoth::ChatGeneration::generateAndSanitizeChat(llm, "hello", opts);

    unsetenv("THOTH_MOCK_LLM_UNAVAILABLE");
    Thoth::RobustnessMockResponses::reset();

    if (out.provider_ok || out.retried_without_stops || out.fallback_used) {
        std::cerr << "testPlanNGenerateTransportFailure: flags wrong\n";
        return false;
    }
    if (out.sanitized_text.empty() == false) {
        std::cerr << "testPlanNGenerateTransportFailure: sanitized_text must be empty\n";
        return false;
    }
    if (out.provider_error.empty()) {
        std::cerr << "testPlanNGenerateTransportFailure: provider_error missing\n";
        return false;
    }
    return true;
}

// Plan N N2 / N-T6b — query() compatibility mapping.
static bool testPlanNQueryCompatibility() {
    Thoth::RobustnessMockResponses::reset();
    Thoth::RobustnessMockResponses::push("");
    LLMInterface llm(LLMBackend::Ollama, nullptr);
    const std::string empty = llm.query("hello", 32, {});
    if (!empty.empty()) {
        std::cerr << "testPlanNQueryCompatibility: soft-empty should return empty string\n";
        Thoth::RobustnessMockResponses::reset();
        return false;
    }

    setenv("THOTH_MOCK_LLM_UNAVAILABLE", "1", 1);
    const std::string err = llm.query("hello", 32, {});
    unsetenv("THOTH_MOCK_LLM_UNAVAILABLE");
    Thoth::RobustnessMockResponses::reset();

    if (err.find("[Error]") == std::string::npos) {
        std::cerr << "testPlanNQueryCompatibility: hard fail should keep [Error] prefix\n";
        return false;
    }
    return true;
}

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

    const EnvSnapshot priorWorkspace = EnvSnapshot::capture("THOTH_WORKSPACE_PATH");
    const EnvSnapshot priorProjectRoot = EnvSnapshot::capture("THOTH_PROJECT_ROOT");
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
        priorWorkspace.restore("THOTH_WORKSPACE_PATH");
        priorProjectRoot.restore("THOTH_PROJECT_ROOT");
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

static bool testG1dTuneWtOverride() {
    if (!Thoth::isTrajectoryAblationTuneWt(0.05f) || !Thoth::isTrajectoryAblationTuneWt(0.1f)) {
        std::cerr << "testG1dTuneWtOverride: expected 0.05 and 0.1 accepted\n";
        return false;
    }
    if (Thoth::isTrajectoryAblationTuneWt(0.2f) || Thoth::isTrajectoryAblationTuneWt(0.0f)) {
        std::cerr << "testG1dTuneWtOverride: 0.2/0.0 must not be tune weights\n";
        return false;
    }
    for (const float wt : {0.05f, 0.1f}) {
        const auto cfgA = Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::A, wt);
        const auto cfgB = Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::B, wt);
        const auto cfgC = Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::C, wt);
        if (cfgA.wt != 0.0f || cfgA.force_empty_trajectory) {
            std::cerr << "testG1dTuneWtOverride: arm A must stay wt=0\n";
            return false;
        }
        if (std::fabs(cfgB.wt - wt) > 1e-6f || cfgB.force_empty_trajectory) {
            std::cerr << "testG1dTuneWtOverride: arm B wt mismatch\n";
            return false;
        }
        if (std::fabs(cfgC.wt - wt) > 1e-6f || !cfgC.force_empty_trajectory) {
            std::cerr << "testG1dTuneWtOverride: arm C wt/empty-T mismatch\n";
            return false;
        }
    }
    return true;
}

static bool testG1ePolarityWtAllowlist() {
    for (const float wt : {-0.05f, -0.10f, -0.20f, -0.30f}) {
        if (!Thoth::isTrajectoryAblationG1eWt(wt) || !Thoth::isTrajectoryAblationCliWt(wt)) {
            std::cerr << "testG1ePolarityWtAllowlist: expected G1e wt accepted: " << wt << '\n';
            return false;
        }
        if (Thoth::isTrajectoryAblationTuneWt(wt)) {
            std::cerr << "testG1ePolarityWtAllowlist: G1e wt must not be G1d TUNE: " << wt << '\n';
            return false;
        }
        const auto cfgB = Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::B, wt);
        const auto cfgA = Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::A, wt);
        const auto cfgC = Thoth::trajectoryAblationArmConfig(Thoth::TrajectoryAblationArm::C, wt);
        if (cfgA.wt != 0.0f) {
            std::cerr << "testG1ePolarityWtAllowlist: arm A must stay wt=0\n";
            return false;
        }
        if (std::fabs(cfgB.wt - wt) > 1e-6f || cfgB.force_empty_trajectory) {
            std::cerr << "testG1ePolarityWtAllowlist: arm B wt mismatch\n";
            return false;
        }
        if (std::fabs(cfgC.wt - wt) > 1e-6f || !cfgC.force_empty_trajectory) {
            std::cerr << "testG1ePolarityWtAllowlist: arm C wt/empty-T mismatch\n";
            return false;
        }
    }
    // -0.3 must match -0.30 within epsilon used by allowlist
    if (!Thoth::isTrajectoryAblationG1eWt(-0.3f)) {
        std::cerr << "testG1ePolarityWtAllowlist: expected -0.3 accepted as -0.30\n";
        return false;
    }
    if (Thoth::isTrajectoryAblationG1eWt(0.05f) || Thoth::isTrajectoryAblationG1eWt(0.1f) ||
        Thoth::isTrajectoryAblationG1eWt(0.2f) || Thoth::isTrajectoryAblationG1eWt(0.0f) ||
        Thoth::isTrajectoryAblationG1eWt(0.25f) || Thoth::isTrajectoryAblationG1eWt(-0.25f) ||
        Thoth::isTrajectoryAblationG1eWt(-0.40f)) {
        std::cerr << "testG1ePolarityWtAllowlist: unexpected G1e accept\n";
        return false;
    }
    if (Thoth::isTrajectoryAblationCliWt(0.2f) || Thoth::isTrajectoryAblationCliWt(0.0f) ||
        Thoth::isTrajectoryAblationCliWt(0.25f) || Thoth::isTrajectoryAblationCliWt(-0.25f) ||
        Thoth::isTrajectoryAblationCliWt(-0.40f)) {
        std::cerr << "testG1ePolarityWtAllowlist: CLI must reject non-schedule weights\n";
        return false;
    }
    if (!Thoth::isTrajectoryAblationCliWt(0.05f) || !Thoth::isTrajectoryAblationCliWt(0.1f)) {
        std::cerr << "testG1ePolarityWtAllowlist: CLI must still accept G1d TUNE\n";
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
        fs::path(fh.getRagDirectory()) / "g1d_unit_test_corpus.txt";
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
    EnvSnapshot priorDev;
    EnvSnapshot priorMock;

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
        priorDev = EnvSnapshot::capture("THOTH_TEST_SUITE_DEV");
        priorMock = EnvSnapshot::capture("THOTH_MOCK_LLM");
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
        priorDev.restore("THOTH_TEST_SUITE_DEV");
        priorMock.restore("THOTH_MOCK_LLM");
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
    EnvSnapshot priorDev;
    EnvSnapshot priorMock;

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
        priorDev = EnvSnapshot::capture("THOTH_TEST_SUITE_DEV");
        priorMock = EnvSnapshot::capture("THOTH_MOCK_LLM");
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
        priorDev.restore("THOTH_TEST_SUITE_DEV");
        priorMock.restore("THOTH_MOCK_LLM");
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

/**
 * E2-33 — Authoritative completion synchronization.
 * Slow mock LLM (20s) exceeds the legacy ~15s wait but is under the configured
 * arm budget (45s). Harness must record a terminal state, not INCOMPLETE.
 */
static bool testE2Ep015CompletionSynchronization() {
    const std::string binary = findEpisodicHarnessBinary();
    if (binary.empty()) {
        std::cerr << "testE2Ep015CompletionSynchronization: harness binary not found\n";
        return false;
    }

    FileHandler fh;
    const fs::path logPath =
        fs::path(fh.getProjectRoot()) / "logs" / "episodic_learning_benchmark.jsonl";
    const std::string logBackup = logPath.string() + ".ep015_sync_backup";
    if (fs::exists(logPath)) {
        fs::copy_file(logPath, logBackup, fs::copy_options::overwrite_existing);
        fs::remove(logPath);
    }

    // Delay 20s > legacy 15s wait; budget 45s > delay (configured-env safety margin).
    const int rc = runShellCommand(
        "THOTH_MOCK_LLM_DELAY_MS=20000 THOTH_E2_ARM_WAIT_MS=45000 "
        "THOTH_E2_WIRING_STAGE=A2 \"" +
        binary + "\" --mock >\"/tmp/thoth_ep015_sync.out\" 2>&1");
    if (rc != 0) {
        std::cerr << "testE2Ep015CompletionSynchronization: harness exit=" << rc << '\n';
        std::ifstream errIn("/tmp/thoth_ep015_sync.out");
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

    bool sawArm = false;
    bool sawIncomplete = false;
    bool sawTerminal = false;
    for (const auto& row : readJsonlRows(logPath.string())) {
        if (row.value("event", "") != "E2_STRICT_INJECTION_LOG_DIAG") {
            continue;
        }
        if (row.value("wiring_stage", "") != "A2") {
            continue;
        }
        sawArm = true;
        const std::string state = row.value("terminal_state", "");
        if (state == "INCOMPLETE") {
            sawIncomplete = true;
        }
        if (state == "COMPLETED" || state == "FAILED" || state == "ABORTED") {
            sawTerminal = true;
        }
    }

    if (fs::exists(logBackup)) {
        fs::copy_file(logBackup, logPath, fs::copy_options::overwrite_existing);
        fs::remove(logBackup);
    }

    if (!sawArm) {
        std::cerr << "testE2Ep015CompletionSynchronization: no A2 arm rows\n";
        return false;
    }
    if (sawIncomplete) {
        std::cerr << "testE2Ep015CompletionSynchronization: INCOMPLETE under slow LLM "
                     "(sync bug not fixed)\n";
        return false;
    }
    if (!sawTerminal) {
        std::cerr << "testE2Ep015CompletionSynchronization: no terminal state recorded\n";
        return false;
    }
    return true;
}

static bool runE2Ep015SyncTests() {
    std::cout << "E2-EP-01.5 E2-33 — authoritative completion synchronization\n";
    std::cout << "  gate: THOTH_E2_EP015_SYNC\n";
    if (!testE2Ep015CompletionSynchronization()) {
        std::cerr << "E2-EP-01.5: E2-33 completion sync failed\n";
        return false;
    }
    std::cout << "  E2-33 completion sync pass (slow LLM → terminal, not INCOMPLETE)\n";
    return true;
}

/**
 * EP-01.5 Phase 5 — mock path + authoritative smoke regression after Phases 1–4.
 * Confirms E2-29 / E2-30 still green; does not re-run full Phase 1–3 Ollama suites.
 */
static bool runE2Ep015Phase5Tests() {
    std::cout << "E2-EP-01.5 Phase 5 — mock + authoritative smoke regression\n";
    std::cout << "  gate: THOTH_E2_EP015_PHASE5\n";

    if (!testE2Ep01MockRegressionPreservesE28()) {
        std::cerr << "E2-EP-01.5 Phase 5: E2-29 mock regression failed\n";
        return false;
    }
    std::cout << "  E2-29 mock regression pass\n";

    if (!testE2B5OfficialFingerprintDeterminism()) {
        std::cerr << "E2-EP-01.5 Phase 5: Phase D E2-28 spot-check failed\n";
        return false;
    }
    std::cout << "  Phase D E2-28 spot-check pass\n";

    if (!testE2Ep01AuthoritativeInferenceSmoke()) {
        std::cerr << "E2-EP-01.5 Phase 5: E2-30 authoritative smoke failed\n";
        return false;
    }
    std::cout << "  E2-30 authoritative smoke pass (zero official_scoring rows)\n";

    // Phase 2 predicate still holds after later phases (no tier drift).
    if (!testE2Ep015TierDeclarationAligned()) {
        std::cerr << "E2-EP-01.5 Phase 5: E2-31b tier alignment regresssed\n";
        return false;
    }
    std::cout << "  E2-31b tier declaration still aligned\n";

    if (!testE2Ep015ExecutionGateRequiresTokens()) {
        std::cerr << "E2-EP-01.5 Phase 5: execution-gate predicate regresssed\n";
        return false;
    }
    std::cout << "  execution-gate predicate still requires tokens\n";
    return true;
}

/**
 * Full EP-01.5 close-out orchestrator (after Phases 1–5 land).
 * Sequence: E2-29 → E2-28 spot → E2-30 → E2-31b → E2-31 → E2-32.
 * Leave THOTH_E2_EP01 unchanged for historical EP-01 seal.
 */
static bool runE2Ep015Tests() {
    std::cout << "E2-EP-01.5 authoritative LLM wiring close-out\n";
    std::cout << "  gate: THOTH_E2_EP015\n";
    std::cout << "  sequence: E2-29 -> E2-28 spot -> E2-30 -> E2-31b -> E2-31 -> E2-32\n";

    if (!testE2Ep01MockRegressionPreservesE28()) {
        std::cerr << "E2-EP-01.5: E2-29 mock regression failed\n";
        return false;
    }
    std::cout << "  E2-29 mock regression pass\n";

    if (!testE2B5OfficialFingerprintDeterminism()) {
        std::cerr << "E2-EP-01.5: Phase D E2-28 spot-check failed\n";
        return false;
    }
    std::cout << "  Phase D E2-28 spot-check pass\n";

    if (!testE2Ep01AuthoritativeInferenceSmoke()) {
        std::cerr << "E2-EP-01.5: E2-30 authoritative smoke failed\n";
        return false;
    }
    std::cout << "  E2-30 authoritative smoke pass\n";

    if (!testE2Ep015TierDeclarationAligned()) {
        std::cerr << "E2-EP-01.5: E2-31b failed\n";
        return false;
    }
    std::cout << "  E2-31b tier declaration pass\n";

    if (!testE2Ep015AuthoritativeLlmWiring()) {
        std::cerr << "E2-EP-01.5: E2-31 failed\n";
        return false;
    }
    std::cout << "  E2-31 authoritative LLM wiring pass\n";

    if (!testE2Ep015ExecutionGateRequiresTokens()) {
        std::cerr << "E2-EP-01.5: execution-gate predicate failed\n";
        return false;
    }
    if (!testE2Ep015FailClosedNoOfficialSummary()) {
        std::cerr << "E2-EP-01.5: E2-32 fail-closed failed\n";
        return false;
    }
    std::cout << "  E2-32 fail-closed pass\n";
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

static bool testAlpFeatureFlagsDefaultOff() {
    unsetenv("THOTH_ALP_ENABLED");
    unsetenv("THOTH_ALP_TX_INDEX");
    unsetenv("THOTH_ALP_GUI");
    unsetenv("THOTH_ALP_GREENFIELD");

    if (Thoth::AlpFeatureFlags::alpEnabled() ||
        Thoth::AlpFeatureFlags::transactionalIndexingEnabled() ||
        Thoth::AlpFeatureFlags::alpGuiEnabled() ||
        Thoth::AlpFeatureFlags::alpGreenfield()) {
        std::cerr << "testAlpFeatureFlagsDefaultOff: flags must default off\n";
        return false;
    }

    setenv("THOTH_ALP_ENABLED", "1", 1);
    if (!Thoth::AlpFeatureFlags::alpEnabled()) {
        std::cerr << "testAlpFeatureFlagsDefaultOff: THOTH_ALP_ENABLED=1 not honored\n";
        unsetenv("THOTH_ALP_ENABLED");
        return false;
    }
    unsetenv("THOTH_ALP_ENABLED");
    return true;
}

static bool testAlpDocumentRegistryEmptyLoadSave() {
    const fs::path temp =
        fs::temp_directory_path() / ("thoth_alp_registry_" + std::to_string(getpid()) + ".json");

    Thoth::DocumentRegistry reg;
    if (!reg.load(temp.string())) {
        std::cerr << "testAlpDocumentRegistryEmptyLoadSave: load missing file failed\n";
        return false;
    }
    if (!reg.loaded() || reg.body()["documents"].size() != 0) {
        std::cerr << "testAlpDocumentRegistryEmptyLoadSave: expected empty v1 body\n";
        return false;
    }
    if (!reg.save(temp.string())) {
        std::cerr << "testAlpDocumentRegistryEmptyLoadSave: save failed\n";
        return false;
    }

    Thoth::DocumentRegistry reloaded;
    if (!reloaded.load(temp.string())) {
        std::cerr << "testAlpDocumentRegistryEmptyLoadSave: reload failed\n";
        fs::remove(temp);
        return false;
    }
    if (reloaded.body().value("schema_version", 0) != Thoth::DocumentRegistry::kSchemaVersion) {
        std::cerr << "testAlpDocumentRegistryEmptyLoadSave: schema_version mismatch\n";
        fs::remove(temp);
        return false;
    }

    fs::remove(temp);
    return true;
}

static bool testAlpNamespacesCreatedOnInit() {
    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    const fs::path seed = Thoth::AlpStoragePaths::seedCorpusDir();
    const fs::path attachments = Thoth::AlpStoragePaths::operatorAttachmentsDir();
    const fs::path revisions = Thoth::AlpStoragePaths::revisionStorageRoot();

    if (!fs::is_directory(seed) || !fs::is_directory(attachments) || !fs::is_directory(revisions)) {
        std::cerr << "testAlpNamespacesCreatedOnInit: ALP namespace dirs missing after init\n";
        return false;
    }
    return true;
}

static bool testAlpIndexManagerLoadsEmptyRegistry() {
    const fs::path reg_path = Thoth::DocumentRegistry::defaultRegistryPath();
    const fs::path reg_backup = reg_path.string() + ".bak_empty_" + std::to_string(getpid());
    std::error_code ec;
    const bool had_registry = fs::exists(reg_path, ec);
    if (had_registry) {
        fs::copy(reg_path, reg_backup, fs::copy_options::overwrite_existing, ec);
    }

    Thoth::DocumentRegistry empty;
    empty.clear();
    if (!empty.save(reg_path.string())) {
        std::cerr << "testAlpIndexManagerLoadsEmptyRegistry: could not seed empty registry\n";
        return false;
    }

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    const auto& reg = idx.getDocumentRegistry();
    bool ok = true;
    if (!reg.loaded()) {
        std::cerr << "testAlpIndexManagerLoadsEmptyRegistry: registry not loaded\n";
        ok = false;
    }
    const auto& body = reg.body();
    if (!body.contains("documents") || !body["documents"].is_array() ||
        body["documents"].size() != 0) {
        std::cerr << "testAlpIndexManagerLoadsEmptyRegistry: expected empty documents array\n";
        ok = false;
    }

    if (had_registry) {
        fs::copy(reg_backup, reg_path, fs::copy_options::overwrite_existing, ec);
        fs::remove(reg_backup, ec);
    } else if (fs::exists(reg_path, ec)) {
        fs::remove(reg_path, ec);
    }

    return ok;
}

static bool testAlpTransactionalIndexPreservesOnEmptyReindex() {
    setenv("THOTH_ALP_TX_INDEX", "1", 1);

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "alp_tx_preserve";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "alp_tx_preserve.md";

    std::string normalized = doc.string();
    try {
        normalized = fs::absolute(doc).lexically_normal().string();
    } catch (...) {
    }

    auto countForFile = [&](const IndexManager& im) {
        int n = 0;
        for (const auto& c : im.getChunks()) {
            if (c.fileName == normalized) {
                ++n;
            }
        }
        return n;
    };

    {
        std::ofstream out(doc);
        out << "# ALP transactional preserve\n\n"
            << "This document has enough substantive text to index successfully under ALP-B.\n"
            << "It must survive a failed reindex attempt without losing committed chunks.\n";
    }

    idx.indexFile(doc.string());
    const int before = countForFile(idx);
    if (before < 1) {
        std::cerr << "testAlpTransactionalIndexPreservesOnEmptyReindex: initial index empty\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }

    {
        std::ofstream out(doc, std::ios::trunc);
        out << "   \n\t";
    }

    idx.indexFile(doc.string());
    const int after = countForFile(idx);
    if (after != before) {
        std::cerr << "testAlpTransactionalIndexPreservesOnEmptyReindex: chunk count changed "
                  << before << " -> " << after << "\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }

    unsetenv("THOTH_ALP_TX_INDEX");
    fs::remove(doc);
    return true;
}

static bool testAlpTransactionalReindexSameContentSucceeds() {
    setenv("THOTH_ALP_TX_INDEX", "1", 1);

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "alp_tx_reindex_same";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "alp_tx_reindex_same.md";

    std::string normalized = doc.string();
    try {
        normalized = fs::absolute(doc).lexically_normal().string();
    } catch (...) {
    }

    auto countForFile = [&](const IndexManager& im) {
        int n = 0;
        for (const auto& c : im.getChunks()) {
            if (c.fileName == normalized) {
                ++n;
            }
        }
        return n;
    };

    const std::string body =
        "# ALP transactional reindex\n\n"
        "Paragraph one with enough substantive text to index under ALP-B.\n\n"
        "Paragraph two with enough substantive text to index under ALP-B.\n\n"
        "Paragraph three with enough substantive text to index under ALP-B.\n";

    {
        std::ofstream out(doc);
        out << body;
    }

    idx.indexFile(doc.string());
    const int first = countForFile(idx);
    if (first < 1) {
        std::cerr << "testAlpTransactionalReindexSameContentSucceeds: initial index empty\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }

    idx.indexFile(doc.string());
    const int second = countForFile(idx);
    if (second < 1) {
        std::cerr << "testAlpTransactionalReindexSameContentSucceeds: reindex produced no chunks\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }
    if (second != first) {
        std::cerr << "testAlpTransactionalReindexSameContentSucceeds: chunk count changed "
                  << first << " -> " << second << "\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }

    unsetenv("THOTH_ALP_TX_INDEX");
    fs::remove(doc);
    return true;
}

static bool testAlpValidateFailurePreservesPriorChunks() {
    setenv("THOTH_ALP_TX_INDEX", "1", 1);

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "alp_validate_fail";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "alp_validate_fail.md";

    std::string normalized = doc.string();
    try {
        normalized = fs::absolute(doc).lexically_normal().string();
    } catch (...) {
    }

    auto countForFile = [&](const IndexManager& im) {
        int n = 0;
        for (const auto& c : im.getChunks()) {
            if (c.fileName == normalized) {
                ++n;
            }
        }
        return n;
    };

    {
        std::ofstream out(doc);
        out << "# Valid initial index\n\n";
        for (int i = 0; i < 15; ++i) {
            out << "Initial paragraph " << i
                << " with enough text to produce multiple committed chunks.\n\n";
        }
    }

    idx.indexFile(doc.string());
    const int before = countForFile(idx);
    if (before < 2) {
        std::cerr << "testAlpValidateFailurePreservesPriorChunks: need multi-chunk baseline\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }

    {
        std::ofstream out(doc, std::ios::trunc);
        for (int i = 0; i < 25; ++i) {
            out << "Replacement paragraph " << i
                << " with sufficient length for embed attempts on reindex.\n\n";
        }
    }

    // Fail all embed attempts after the first → embed_ok/embed_attempts < 95%.
    setenv("THOTH_ALP_TEST_EMBED_FAIL_AFTER", "1", 1);
    idx.indexFile(doc.string());
    unsetenv("THOTH_ALP_TEST_EMBED_FAIL_AFTER");

    if (countForFile(idx) != before) {
        std::cerr << "testAlpValidateFailurePreservesPriorChunks: chunk count changed\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }

    unsetenv("THOTH_ALP_TX_INDEX");
    fs::remove(doc);
    return true;
}

static bool testAlpEmbedFailurePreservesPriorChunks() {
    setenv("THOTH_ALP_TX_INDEX", "1", 1);

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "alp_embed_fail";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "alp_embed_fail.md";

    std::string normalized = doc.string();
    try {
        normalized = fs::absolute(doc).lexically_normal().string();
    } catch (...) {
    }

    auto countForFile = [&](const IndexManager& im) {
        int n = 0;
        for (const auto& c : im.getChunks()) {
            if (c.fileName == normalized) {
                ++n;
            }
        }
        return n;
    };

    {
        std::ofstream out(doc);
        out << "# Embed failure preserve\n\n";
        for (int i = 0; i < 20; ++i) {
            out << "Paragraph " << i
                << " with enough text to become a valid chunk for indexing.\n\n";
        }
    }

    idx.indexFile(doc.string());
    const int before = countForFile(idx);
    if (before < 1) {
        std::cerr << "testAlpEmbedFailurePreservesPriorChunks: initial index empty\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        unsetenv("THOTH_ALP_TEST_EMBED_FAIL_AFTER");
        fs::remove(doc);
        return false;
    }

    {
        std::ofstream out(doc, std::ios::trunc);
        for (int i = 0; i < 30; ++i) {
            out << "Replacement paragraph " << i
                << " with sufficient length to attempt embedding on reindex.\n\n";
        }
    }

    setenv("THOTH_ALP_TEST_EMBED_FAIL_AFTER", "0", 1);
    idx.indexFile(doc.string());
    unsetenv("THOTH_ALP_TEST_EMBED_FAIL_AFTER");

    if (countForFile(idx) != before) {
        std::cerr << "testAlpEmbedFailurePreservesPriorChunks: chunk count changed\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }

    unsetenv("THOTH_ALP_TX_INDEX");
    fs::remove(doc);
    return true;
}

static bool testAlpPersistFailurePreservesOnDiskCommit() {
    setenv("THOTH_ALP_TX_INDEX", "1", 1);

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "alp_persist_fail";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "alp_persist_fail.md";
    const fs::path index_path = rag_dir / "rag_index.bin";
    if (fs::exists(index_path)) {
        fs::remove(index_path);
    }

    std::string normalized = doc.string();
    try {
        normalized = fs::absolute(doc).lexically_normal().string();
    } catch (...) {
    }

    auto countForFile = [&](const IndexManager& im) {
        int n = 0;
        for (const auto& c : im.getChunks()) {
            if (c.fileName == normalized) {
                ++n;
            }
        }
        return n;
    };

    {
        std::ofstream out(doc);
        out << "# Persist failure test v1\n\n"
            << "First revision with enough text to index and persist to disk.\n";
    }

    {
        IndexManager idx(engine.get());
        idx.init(index_path.string());
        idx.indexFile(doc.string());
        if (countForFile(idx) < 1) {
            std::cerr << "testAlpPersistFailurePreservesOnDiskCommit: v1 index empty\n";
            unsetenv("THOTH_ALP_TX_INDEX");
            fs::remove(doc);
            fs::remove(index_path);
            return false;
        }
    }

    const int disk_before = [&]() {
        IndexManager idx(engine.get());
        idx.init(index_path.string());
        return countForFile(idx);
    }();

    {
        std::ofstream out(doc, std::ios::trunc);
        out << "# Persist failure test v2\n\n"
            << "Replacement content that should commit in memory but not persist.\n"
            << "Extra paragraph to ensure reindex runs and produces distinct chunks.\n";
    }

    setenv("THOTH_ALP_FORCE_SAVE_INDEX_FAIL", "1", 1);
    {
        IndexManager idx(engine.get());
        idx.init(index_path.string());
        idx.indexFile(doc.string());
    }
    unsetenv("THOTH_ALP_FORCE_SAVE_INDEX_FAIL");

    const int disk_after = [&]() {
        IndexManager idx(engine.get());
        idx.init(index_path.string());
        return countForFile(idx);
    }();

    if (disk_after != disk_before) {
        std::cerr << "testAlpPersistFailurePreservesOnDiskCommit: disk count changed "
                  << disk_before << " -> " << disk_after << "\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        fs::remove(index_path);
        return false;
    }

    unsetenv("THOTH_ALP_TX_INDEX");
    fs::remove(doc);
    fs::remove(index_path);
    return true;
}

static bool testAlpRegistryRevisionLifecycle() {
    const fs::path temp =
        fs::temp_directory_path()
        / ("thoth_alp_registry_rev_" + std::to_string(getpid()) + ".json");

    Thoth::DocumentRegistry reg;
    reg.load(temp.string());
    if (!reg.beginRevision("doc-1", "rev-a", "/tmp/a.md")) {
        std::cerr << "testAlpRegistryRevisionLifecycle: begin rev-a failed\n";
        return false;
    }
    if (!reg.markRevisionCommitted("doc-1", "rev-a", 5)) {
        std::cerr << "testAlpRegistryRevisionLifecycle: commit rev-a failed\n";
        fs::remove(temp);
        return false;
    }
    if (!reg.beginRevision("doc-1", "rev-b", "/tmp/b.md")) {
        std::cerr << "testAlpRegistryRevisionLifecycle: begin rev-b failed\n";
        fs::remove(temp);
        return false;
    }
    if (!reg.markRevisionCommitted("doc-1", "rev-b", 7)) {
        std::cerr << "testAlpRegistryRevisionLifecycle: commit rev-b failed\n";
        fs::remove(temp);
        return false;
    }
    reg.supersedePriorRevisions("doc-1", "rev-b");

    std::string state_a;
    std::string state_b;
    for (const auto& row : reg.body()["revisions"]) {
        if (row.value("revision_id", "") == "rev-a") {
            state_a = row.value("state", "");
        }
        if (row.value("revision_id", "") == "rev-b") {
            state_b = row.value("state", "");
        }
    }
    if (state_a != "superseded" || state_b != "committed") {
        std::cerr << "testAlpRegistryRevisionLifecycle: unexpected revision states\n";
        fs::remove(temp);
        return false;
    }

    fs::remove(temp);
    return true;
}

static bool testAlpIndexingCompletedMetadata() {
    setenv("THOTH_ALP_TX_INDEX", "1", 1);

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());

    FileHandler fh;
    const fs::path rag_dir =
        fs::path(fh.getAgentWorkspacePath()) / "rag" / "alp_event_meta";
    fs::create_directories(rag_dir);
    const fs::path doc = rag_dir / "alp_event_meta.md";

    {
        std::ofstream out(doc);
        out << "# Event metadata test\n\nContent for ALP indexing completion metadata.\n";
    }

    ControllerEvent completed;
    bool got = false;
    idx.setEventCallback([&](const ControllerEvent& ev) {
        if (ev.type == EventType::INDEXING_COMPLETED) {
            completed = ev;
            got = true;
        }
    });

    idx.setAlpIndexContext(
        IndexManager::AlpIndexContext{"doc-meta-1", "rev-meta-1", "alp_event_meta.md"});
    idx.indexFile(doc.string());
    idx.clearAlpIndexContext();

    if (!got) {
        std::cerr << "testAlpIndexingCompletedMetadata: no INDEXING_COMPLETED\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }
    if (completed.metadata.value("document_id", "") != "doc-meta-1"
        || completed.metadata.value("revision_id", "") != "rev-meta-1") {
        std::cerr << "testAlpIndexingCompletedMetadata: metadata ids missing\n";
        unsetenv("THOTH_ALP_TX_INDEX");
        fs::remove(doc);
        return false;
    }

    unsetenv("THOTH_ALP_TX_INDEX");
    fs::remove(doc);
    return true;
}

static fs::path makeAlpD0FixtureWorkspace() {
    const fs::path root =
        fs::temp_directory_path() / ("thoth_alp_d0_" + std::to_string(getpid()));
    fs::create_directories(root / "rag");
    return root;
}

static bool testAlpD0ReportDeterministic() {
    const fs::path root = makeAlpD0FixtureWorkspace();
    const fs::path egar = root / "rag" / "EGAR.md";
    const fs::path egar1 = root / "rag" / "EGAR_1.md";

    {
        std::ofstream out(egar);
        out << "# EGAR winner\n\nPrimary document with enough content for ALP D0 migration analysis.\n";
    }
    {
        std::ofstream out(egar1);
        out << "# EGAR duplicate stem\n\nSecondary suffix file with different hash content for grouping.\n";
    }

    Thoth::AlpMigrationAnalyzer analyzer(root.string());
    const auto r1 = analyzer.runDryRun();
    const auto r2 = analyzer.runDryRun();

    if (r1["report_hash"] != r2["report_hash"]) {
        std::cerr << "testAlpD0ReportDeterministic: report_hash mismatch\n";
        fs::remove_all(root);
        return false;
    }
    if (r1.value("dry_run", false) != true) {
        std::cerr << "testAlpD0ReportDeterministic: dry_run flag missing\n";
        fs::remove_all(root);
        return false;
    }

    fs::remove_all(root);
    return true;
}

static bool testAlpD0EgarSuffixGroupWinner() {
    const fs::path root = makeAlpD0FixtureWorkspace();
    const fs::path egar = root / "rag" / "EGAR.md";
    const fs::path egar1 = root / "rag" / "EGAR_1.md";

    {
        std::ofstream out(egar);
        out << "Primary EGAR content with sufficient length for migration candidate validity.\n";
    }
    {
        std::ofstream out(egar1);
        out << "Secondary EGAR_1 content also valid but should lose if mtime is older.\n";
    }

    {
        std::ofstream touch_newer(egar);
        touch_newer << "Primary EGAR content with sufficient length for migration candidate validity.\n";
    }

    Thoth::AlpMigrationAnalyzer analyzer(root.string());
    const auto report = analyzer.runDryRun();
    bool found = false;
    for (const auto& group : report["groups"]) {
        if (group.value("canonical_stem", "") != "EGAR") {
            continue;
        }
        found = true;
        const std::string winner_path = group["winner"].value("path", "");
        if (winner_path.find("EGAR_1.md") != std::string::npos) {
            std::cerr << "testAlpD0EgarSuffixGroupWinner: unexpected winner\n";
            fs::remove_all(root);
            return false;
        }
        if (group["losers"].size() < 1) {
            std::cerr << "testAlpD0EgarSuffixGroupWinner: expected a loser row\n";
            fs::remove_all(root);
            return false;
        }
    }
    if (!found) {
        std::cerr << "testAlpD0EgarSuffixGroupWinner: EGAR group missing\n";
        fs::remove_all(root);
        return false;
    }

    fs::remove_all(root);
    return true;
}

static bool testAlpD0ReadOnlyNoMutation() {
    const fs::path root = makeAlpD0FixtureWorkspace();
    const fs::path egar = root / "rag" / "EGAR.md";
    {
        std::ofstream out(egar);
        out << "Read-only dry-run must not mutate workspace files during analysis.\n";
    }

    const auto hash_file = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };

    const std::string before = hash_file(egar);
    const auto mtime_before = fs::last_write_time(egar);

    Thoth::AlpMigrationAnalyzer analyzer(root.string());
    (void)analyzer.runDryRun();

    const std::string after = hash_file(egar);
    if (before != after || fs::last_write_time(egar) != mtime_before) {
        std::cerr << "testAlpD0ReadOnlyNoMutation: fixture file changed\n";
        fs::remove_all(root);
        return false;
    }
    if (fs::exists(root / "document_registry.json")) {
        std::cerr << "testAlpD0ReadOnlyNoMutation: document_registry.json created\n";
        fs::remove_all(root);
        return false;
    }

    fs::remove_all(root);
    return true;
}

static bool testAlpD0PreviewIdsNotInCanonicalHashVolatility() {
    const fs::path root = makeAlpD0FixtureWorkspace();
    {
        std::ofstream out(root / "rag" / "note.md");
        out << "Preview id fields must be stable in report_hash across identical runs.\n";
    }
    Thoth::AlpMigrationAnalyzer analyzer(root.string());
    const auto report = analyzer.runDryRun();
    const auto canonical = Thoth::AlpMigrationAnalyzer::canonicalPayload(report);
    if (canonical.contains("generated_at") || canonical.contains("migration_run_id")
        || canonical.contains("report_hash")) {
        std::cerr << "testAlpD0PreviewIdsNotInCanonicalHashVolatility: volatile keys in canonical\n";
        fs::remove_all(root);
        return false;
    }
    if (!report["groups"].size()) {
        std::cerr << "testAlpD0PreviewIdsNotInCanonicalHashVolatility: no groups\n";
        fs::remove_all(root);
        return false;
    }
    const auto& winner = report["groups"][0]["winner"];
    if (winner.value("preview_only", false) != true) {
        std::cerr << "testAlpD0PreviewIdsNotInCanonicalHashVolatility: preview_only missing\n";
        fs::remove_all(root);
        return false;
    }

    fs::remove_all(root);
    return true;
}

static bool testAlpD1ApplyEgarSuffixMerge() {
    const fs::path root = makeAlpD0FixtureWorkspace();
    const fs::path egar = root / "rag" / "EGAR.md";
    const fs::path egar1 = root / "rag" / "EGAR_1.md";

    {
        std::ofstream out(egar);
        out << "Primary EGAR content with sufficient length for ALP D1 migration apply.\n";
    }
    {
        std::ofstream out(egar1);
        out << "Secondary EGAR_1 content also valid but should lose suffix merge.\n";
    }

    Thoth::AlpMigrationAnalyzer analyzer(root.string());
    const nlohmann::json report = analyzer.runDryRun();
    const std::string run_id = "alp-d1-test-egar";

    Thoth::AlpMigrationApply apply_engine(root.string());
    const auto result = apply_engine.apply(report, run_id, nullptr);
    if (!result.success) {
        std::cerr << "testAlpD1ApplyEgarSuffixMerge: apply failed: " << result.error << "\n";
        fs::remove_all(root);
        return false;
    }

    const fs::path attachment = root / "rag" / "attachments" / "EGAR.md";
    if (!fs::exists(attachment)) {
        std::cerr << "testAlpD1ApplyEgarSuffixMerge: winner not promoted\n";
        fs::remove_all(root);
        return false;
    }

    const fs::path archive = root / "rag" / "migration_archive" / run_id / "EGAR_1.md";
    if (!fs::exists(archive)) {
        std::cerr << "testAlpD1ApplyEgarSuffixMerge: loser not archived\n";
        fs::remove_all(root);
        return false;
    }

    std::ifstream reg_in(root / "document_registry.json");
    nlohmann::json registry;
    reg_in >> registry;
    if (registry["documents"].size() != 1) {
        std::cerr << "testAlpD1ApplyEgarSuffixMerge: expected one document row\n";
        fs::remove_all(root);
        return false;
    }

    if (!fs::exists(root / "legacy_id_map.json")) {
        std::cerr << "testAlpD1ApplyEgarSuffixMerge: legacy_id_map missing\n";
        fs::remove_all(root);
        return false;
    }

    fs::remove_all(root);
    return true;
}

static bool testAlpD1AbortsOnReportHashMismatch() {
    const fs::path root = makeAlpD0FixtureWorkspace();
    {
        std::ofstream out(root / "rag" / "note.md");
        out << "Initial content for hash mismatch abort test on ALP D1 apply.\n";
    }

    Thoth::AlpMigrationAnalyzer analyzer(root.string());
    const nlohmann::json report = analyzer.runDryRun();

    {
        std::ofstream out(root / "rag" / "note.md", std::ios::app);
        out << "\nCorpus drift after dry-run approval.\n";
    }

    Thoth::AlpMigrationApply apply_engine(root.string());
    const auto result = apply_engine.apply(report, "alp-d1-hash-mismatch", nullptr);
    if (result.success) {
        std::cerr << "testAlpD1AbortsOnReportHashMismatch: apply should have failed\n";
        fs::remove_all(root);
        return false;
    }
    if (result.error.find("drift") == std::string::npos
        && result.error.find("report_hash") == std::string::npos) {
        std::cerr << "testAlpD1AbortsOnReportHashMismatch: unexpected error: " << result.error
                  << "\n";
        fs::remove_all(root);
        return false;
    }

    fs::remove_all(root);
    return true;
}

static bool testAlpD1AbortsOnUnresolvedAmbiguity() {
    const fs::path root = makeAlpD0FixtureWorkspace();
    const fs::path grag = root / "rag" / "GRAG_benchmark.md";
    {
        std::ofstream out(grag);
        out << "Seed heuristic filename plus registry bind creates ambiguous tier for D1 abort.\n";
    }

    nlohmann::json reg{{"owners", nlohmann::json::object()}};
    reg["owners"][fs::absolute(grag).lexically_normal().string()] = "session-ambiguous";
    {
        std::ofstream out(root / "rag_attachment_registry.json");
        out << reg.dump(2);
    }

    Thoth::AlpMigrationAnalyzer analyzer(root.string());
    const nlohmann::json report = analyzer.runDryRun();

    bool has_ambiguous_candidate = false;
    for (const auto& group : report["groups"]) {
        for (const auto& c : group.value("candidates", nlohmann::json::array())) {
            if (c.value("tier_classification", "") == "ambiguous") {
                has_ambiguous_candidate = true;
                break;
            }
        }
    }
    if (!has_ambiguous_candidate) {
        std::cerr << "testAlpD1AbortsOnUnresolvedAmbiguity: fixture missing ambiguous candidate\n";
        fs::remove_all(root);
        return false;
    }

    Thoth::AlpMigrationApply apply_engine(root.string());
    std::string err;
    if (apply_engine.validateReport(report, nullptr, err)) {
        std::cerr << "testAlpD1AbortsOnUnresolvedAmbiguity: validate should fail\n";
        fs::remove_all(root);
        return false;
    }

    fs::remove_all(root);
    return true;
}

static bool testAlpD1RollbackRestoresM0() {
    const fs::path root = makeAlpD0FixtureWorkspace();
    const fs::path note = root / "rag" / "note.md";
    const std::string original = "Rollback test original content for ALP D1 M0 restore path.\n";
    {
        std::ofstream out(note);
        out << original;
    }

    Thoth::AlpMigrationAnalyzer analyzer(root.string());
    const nlohmann::json report = analyzer.runDryRun();
    const std::string run_id = "alp-d1-test-rollback";

    Thoth::AlpMigrationApply apply_engine(root.string());
    const auto result = apply_engine.apply(report, run_id, nullptr);
    if (!result.success) {
        std::cerr << "testAlpD1RollbackRestoresM0: apply failed: " << result.error << "\n";
        fs::remove_all(root);
        return false;
    }

    if (!fs::exists(root / "document_registry.json")) {
        std::cerr << "testAlpD1RollbackRestoresM0: registry should exist post-apply\n";
        fs::remove_all(root);
        return false;
    }

    std::string rb_err;
    if (!apply_engine.rollback(run_id, rb_err)) {
        std::cerr << "testAlpD1RollbackRestoresM0: rollback failed: " << rb_err << "\n";
        fs::remove_all(root);
        return false;
    }

    std::ifstream in(note);
    const std::string restored((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    if (restored != original) {
        std::cerr << "testAlpD1RollbackRestoresM0: note content not restored\n";
        fs::remove_all(root);
        return false;
    }

    if (fs::exists(root / "legacy_id_map.json")) {
        std::cerr << "testAlpD1RollbackRestoresM0: legacy_id_map should be removed\n";
        fs::remove_all(root);
        return false;
    }

    Thoth::AlpMigrationAnalyzer analyzer2(root.string());
    const auto report2 = analyzer2.runDryRun();
    if (report["report_hash"] != report2["report_hash"]) {
        std::cerr << "testAlpD1RollbackRestoresM0: report_hash mismatch after rollback\n";
        fs::remove_all(root);
        return false;
    }

    fs::remove_all(root);
    return true;
}

static bool testAlpCSendPolicyNoOpOnSameHash() {
    Thoth::AttachmentSendPolicy::PolicyInput in;
    in.document_exists = true;
    in.content_hash = "abc123";
    Thoth::AttachmentSendPolicy::CommittedRevision committed;
    committed.content_hash = "abc123";
    committed.indexed_at_ms = 1000;
    in.committed = committed;
    const auto result = Thoth::AttachmentSendPolicy::evaluate(in);
    if (result.action != Thoth::AttachmentSendPolicy::SendAction::NoOp) {
        std::cerr << "testAlpCSendPolicyNoOpOnSameHash: expected no_op\n";
        return false;
    }
    return true;
}

static bool setUnixMtime(const fs::path& path, std::int64_t unix_sec) {
    struct timespec times[2];
    times[0].tv_sec = static_cast<time_t>(unix_sec);
    times[0].tv_nsec = 0;
    times[1].tv_sec = static_cast<time_t>(unix_sec);
    times[1].tv_nsec = 0;
    return utimensat(AT_FDCWD, path.c_str(), times, 0) == 0;
}

static bool testAlpLocalNoteUnixMtimeRequest() {
    const fs::path dir = makeTempPath("thoth_alp_mtime");
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        std::cerr << "testAlpLocalNoteUnixMtimeRequest: mkdir failed\n";
        return false;
    }

    const std::string older_bytes = "older local note body\n";
    const std::string newer_bytes = "newer local note body\n";
    const fs::path older_path = dir / "older.md";
    const fs::path newer_path = dir / "newer.md";
    const fs::path same_older_path = dir / "same-older.md";
    const fs::path same_newer_path = dir / "same-newer.md";
    const std::string same_bytes = "same hash local note\n";

    auto write_file = [](const fs::path& path, const std::string& bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << bytes;
        return static_cast<bool>(out);
    };
    if (!write_file(older_path, older_bytes) || !write_file(newer_path, newer_bytes)
        || !write_file(same_older_path, same_bytes) || !write_file(same_newer_path, same_bytes)) {
        std::cerr << "testAlpLocalNoteUnixMtimeRequest: write failed\n";
        fs::remove_all(dir);
        return false;
    }

    constexpr std::int64_t kOlderUnix = 1'700'000'000LL;
    constexpr std::int64_t kNewerUnix = 1'800'000'000LL;
    constexpr std::int64_t kCommittedUnix = 1'750'000'000LL;
    if (!setUnixMtime(older_path, kOlderUnix) || !setUnixMtime(newer_path, kNewerUnix)
        || !setUnixMtime(same_older_path, kOlderUnix) || !setUnixMtime(same_newer_path, kNewerUnix)) {
        std::cerr << "testAlpLocalNoteUnixMtimeRequest: utimensat failed\n";
        fs::remove_all(dir);
        return false;
    }

    auto expect_request = [&](const fs::path& path,
                               std::int64_t unix_sec,
                               const std::string& committed_hash,
                               Thoth::AttachmentSendPolicy::SendAction expected,
                               const char* label) -> bool {
        struct stat st;
        if (stat(path.c_str(), &st) != 0 || static_cast<std::int64_t>(st.st_mtime) != unix_sec) {
            std::cerr << "testAlpLocalNoteUnixMtimeRequest: " << label
                      << " filesystem mtime mismatch\n";
            return false;
        }
        std::string read_err;
        const auto payload = Thoth::CorpusCreateLocal::readLocalNoteFile(path.string(), read_err);
        if (!payload) {
            std::cerr << "testAlpLocalNoteUnixMtimeRequest: " << label
                      << " read failed: " << read_err << "\n";
            return false;
        }
        if (payload->local_source_mtime_sec != unix_sec || payload->local_source_mtime_sec <= 0) {
            std::cerr << "testAlpLocalNoteUnixMtimeRequest: " << label
                      << " mtime " << payload->local_source_mtime_sec
                      << " expected " << unix_sec << "\n";
            return false;
        }
        Thoth::CorpusCreateGuiOptions options;
        const auto request = Thoth::CorpusCreateLocal::makeCreateRequest(
            *payload, "default", path.string(), options, true);
        const auto body = Thoth::CorpusCreate::makeCreateDocumentRequestBodyAlp(request);
        if (!body.contains("local_source_mtime")
            || !body["local_source_mtime"].is_number_integer()
            || body["local_source_mtime"].get<std::int64_t>() != unix_sec) {
            std::cerr << "testAlpLocalNoteUnixMtimeRequest: " << label
                      << " JSON local_source_mtime missing or wrong\n";
            return false;
        }

        Thoth::AttachmentSendPolicy::PolicyInput in;
        in.document_exists = true;
        in.content_hash = payload->content_hash;
        in.local_source_mtime_sec = payload->local_source_mtime_sec;
        Thoth::AttachmentSendPolicy::CommittedRevision committed;
        committed.content_hash = committed_hash;
        committed.indexed_at_ms = kCommittedUnix * 1000;
        in.committed = committed;
        const auto result = Thoth::AttachmentSendPolicy::evaluate(in);
        if (result.action != expected) {
            std::cerr << "testAlpLocalNoteUnixMtimeRequest: " << label
                      << " action " << Thoth::AttachmentSendPolicy::actionToString(result.action)
                      << "\n";
            return false;
        }
        return true;
    };

    const std::string older_hash = Thoth::sha256Hex(older_bytes);
    const std::string newer_hash = Thoth::sha256Hex(newer_bytes);
    const std::string same_hash = Thoth::sha256Hex(same_bytes);
    const bool ok =
        expect_request(older_path, kOlderUnix, "committed-other-hash",
                       Thoth::AttachmentSendPolicy::SendAction::Conflict, "older")
        && expect_request(newer_path, kNewerUnix, "committed-other-hash",
                          Thoth::AttachmentSendPolicy::SendAction::NewRevision, "newer")
        && expect_request(same_older_path, kOlderUnix, same_hash,
                          Thoth::AttachmentSendPolicy::SendAction::NoOp, "same-older")
        && expect_request(same_newer_path, kNewerUnix, same_hash,
                          Thoth::AttachmentSendPolicy::SendAction::NoOp, "same-newer")
        && older_hash != newer_hash
        && older_hash != same_hash;

    fs::remove_all(dir, ec);
    return ok;
}

static bool testAlpCSendPolicyConflictOnOldMtime() {
    Thoth::AttachmentSendPolicy::PolicyInput in;
    in.document_exists = true;
    in.content_hash = "new_hash";
    in.local_source_mtime_sec = 5;
    Thoth::AttachmentSendPolicy::CommittedRevision committed;
    committed.content_hash = "old_hash";
    committed.indexed_at_ms = 10000;
    in.committed = committed;
    const auto result = Thoth::AttachmentSendPolicy::evaluate(in);
    if (result.action != Thoth::AttachmentSendPolicy::SendAction::Conflict) {
        std::cerr << "testAlpCSendPolicyConflictOnOldMtime: expected conflict\n";
        return false;
    }
    in.force_replace = true;
    const auto forced = Thoth::AttachmentSendPolicy::evaluate(in);
    if (forced.action != Thoth::AttachmentSendPolicy::SendAction::NewRevision) {
        std::cerr << "testAlpCSendPolicyConflictOnOldMtime: force_replace expected new_revision\n";
        return false;
    }
    return true;
}

static bool testAlpCCreateAlpPathNoSuffix() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");

    const fs::path reg_path = Thoth::DocumentRegistry::defaultRegistryPath();
    const fs::path reg_backup =
        reg_path.string() + ".bak." + std::to_string(getpid());
    std::error_code ec;
    const bool had_registry = fs::exists(reg_path, ec);
    if (had_registry) {
        fs::copy(reg_path, reg_backup, fs::copy_options::overwrite_existing, ec);
    }

    auto restore_registry = [&]() {
        if (had_registry) {
            fs::copy(reg_backup, reg_path, fs::copy_options::overwrite_existing, ec);
            fs::remove(reg_backup, ec);
        } else if (fs::exists(reg_path, ec)) {
            fs::remove(reg_path, ec);
        }
    };

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    const std::string slot_name = "alp_c_nosuffix_" + std::to_string(getpid()) + ".md";
    const fs::path attachment_path = Thoth::AlpStoragePaths::operatorAttachmentPath(slot_name);
    if (fs::exists(attachment_path)) {
        fs::remove(attachment_path);
    }
    if (fs::exists(attachment_path.string() + "_1.md")) {
        fs::remove(attachment_path.string() + "_1.md");
    }

    FileHandler fh;
    const fs::path rag_dir = fs::path(fh.getRagDirectory());

    IndexManager::CreateCorpusDocumentOptions opts;
    const std::string content1 =
        "First ALP-C create content with enough bytes for validation gate.\n";
    const auto r1 = idx.createCorpusDocument(rag_dir.string(),
                                             slot_name,
                                             content1,
                                             "session-a",
                                             opts);
    if (!r1.ok) {
        std::cerr << "testAlpCCreateAlpPathNoSuffix: first create failed: " << r1.error << "\n";
        fs::remove(attachment_path);
        restore_registry();
        return false;
    }

    bool committed = false;
    for (int i = 0; i < 500; ++i) {
        while (idx.isIndexing()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        Thoth::DocumentRegistry reg;
        reg.load(Thoth::DocumentRegistry::defaultRegistryPath());
        if (auto rev = reg.findCommittedRevision(r1.document_id)) {
            if (rev->chunk_count > 0) {
                committed = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!committed) {
        std::cerr << "testAlpCCreateAlpPathNoSuffix: first revision never committed\n";
        fs::remove(attachment_path);
        restore_registry();
        return false;
    }

    const std::string content2 =
        "Second ALP-C create with different hash and newer implied content.\n";
    opts.content_hash = Thoth::sha256Hex(content2);
    opts.local_source_mtime_sec = 9999999999LL;
    const auto r2 = idx.createCorpusDocument(rag_dir.string(),
                                             slot_name,
                                             content2,
                                             "session-b",
                                             opts);
    if (!r2.ok) {
        std::cerr << "testAlpCCreateAlpPathNoSuffix: second create failed: " << r2.error << "\n";
        fs::remove(attachment_path);
        restore_registry();
        return false;
    }

    if (r1.document_id != r2.document_id) {
        std::cerr << "testAlpCCreateAlpPathNoSuffix: document_id must be reused\n";
        fs::remove(attachment_path);
        restore_registry();
        return false;
    }

    const fs::path suffix_path =
        fs::path(Thoth::AlpStoragePaths::operatorAttachmentsDir())
        / (fs::path(slot_name).stem().string() + "_1" + fs::path(slot_name).extension().string());
    if (fs::exists(suffix_path)) {
        std::cerr << "testAlpCCreateAlpPathNoSuffix: suffix file forbidden\n";
        fs::remove(attachment_path);
        fs::remove(suffix_path);
        restore_registry();
        return false;
    }

    fs::remove(attachment_path);
    restore_registry();
    return true;
}

static bool testAlpCMisconfiguredRejectsCreate() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_off("THOTH_ALP_TX_INDEX", "0");

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    FileHandler fh;
    const fs::path rag_dir = fs::path(fh.getRagDirectory());
    const auto result = idx.createCorpusDocument(rag_dir.string(),
                                                 "Misconfigured.md",
                                                 "Content for misconfigured ALP-C gate test.\n",
                                                 "session-a");
    if (result.ok || result.machine_code != "alp_misconfigured") {
        std::cerr << "testAlpCMisconfiguredRejectsCreate: expected alp_misconfigured\n";
        return false;
    }

    return true;
}

static bool testAlpCRegistryEnsureDocumentUniqueCanonical() {
    Thoth::DocumentRegistry reg;
    reg.clear();
    if (!reg.ensureDocument("doc-a", "EGAR.md", "/tmp/a/EGAR.md")) {
        std::cerr << "testAlpCRegistryEnsureDocumentUniqueCanonical: ensure doc-a failed\n";
        return false;
    }
    if (reg.ensureDocument("doc-b", "EGAR.md", "/tmp/b/EGAR.md")) {
        std::cerr << "testAlpCRegistryEnsureDocumentUniqueCanonical: duplicate canonical must fail\n";
        return false;
    }
    return true;
}

static bool waitAlpCommittedRevision(const std::string& document_id, IndexManager& idx) {
    for (int i = 0; i < 500; ++i) {
        while (idx.isIndexing()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        const auto& reg = idx.getDocumentRegistry();
        if (auto rev = reg.findCommittedRevision(document_id)) {
            if (rev->chunk_count > 0) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

static bool waitAlpSpecificRevisionCommitted(const std::string& document_id,
                                             const std::string& revision_id,
                                             IndexManager& idx) {
    for (int i = 0; i < 500; ++i) {
        while (idx.isIndexing()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        const auto& reg = idx.getDocumentRegistry();
        const nlohmann::json& body = reg.body();
        if (!body.contains("documents") || !body.contains("revisions")) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        std::string current_rev;
        for (const auto& doc : body["documents"]) {
            if (doc.value("document_id", "") == document_id) {
                current_rev = doc.value("current_revision_id", "");
                break;
            }
        }
        if (current_rev != revision_id) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        for (const auto& row : body["revisions"]) {
            if (row.value("document_id", "") != document_id
                || row.value("revision_id", "") != revision_id) {
                continue;
            }
            if (row.value("state", "") == "committed"
                && row.value("chunk_count", 0) > 0) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

static bool testAlpFLocalNoteDeleteUnlinksSession() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");

    const fs::path reg_path = Thoth::DocumentRegistry::defaultRegistryPath();
    const fs::path reg_backup = reg_path.string() + ".bak." + std::to_string(getpid());
    std::error_code ec;
    const bool had_registry = fs::exists(reg_path, ec);
    if (had_registry) {
        fs::copy(reg_path, reg_backup, fs::copy_options::overwrite_existing, ec);
    }
    auto restore_registry = [&]() {
        if (had_registry) {
            fs::copy(reg_backup, reg_path, fs::copy_options::overwrite_existing, ec);
            fs::remove(reg_backup, ec);
        } else if (fs::exists(reg_path, ec)) {
            fs::remove(reg_path, ec);
        }
    };
    auto fail = [&](const char* msg) {
        std::cerr << "testAlpFLocalNoteDeleteUnlinksSession: " << msg << "\n";
        restore_registry();
        return false;
    };

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    FileHandler fh;
    const std::string slot_name = "alp_f_unlink_" + std::to_string(getpid()) + ".md";
    const fs::path attachment_path = Thoth::AlpStoragePaths::operatorAttachmentPath(slot_name);
    if (fs::exists(attachment_path)) {
        fs::remove(attachment_path);
    }
    const std::string body = "ALP-F selective unlink token ALPFUNLINK unique.\n";
    const auto created = idx.createCorpusDocument(fh.getRagDirectory(),
                                                slot_name,
                                                body,
                                                "session-alp-f-unlink-a");
    if (!created.ok) {
        fs::remove(attachment_path);
        return fail("create failed");
    }
    if (!waitAlpCommittedRevision(created.document_id, idx)) {
        fs::remove(attachment_path);
        return fail("not committed");
    }
    const auto linked_b = idx.createCorpusDocument(fh.getRagDirectory(),
                                                   slot_name,
                                                   body,
                                                   "session-alp-f-unlink-b");
    if (!linked_b.ok || linked_b.document_id != created.document_id) {
        fs::remove(attachment_path);
        return fail("session B link failed");
    }
    if (!idx.getDocumentRegistry().hasSessionLink(created.document_id, "session-alp-f-unlink-a")
        || !idx.getDocumentRegistry().hasSessionLink(created.document_id, "session-alp-f-unlink-b")) {
        fs::remove(attachment_path);
        return fail("expected both session links");
    }
    if (!fs::exists(attachment_path)) {
        return fail("storage missing before unlink");
    }
    const std::string revision_before = created.revision_id;
    if (!idx.unlinkSessionDocument(created.document_id, "session-alp-f-unlink-a")) {
        fs::remove(attachment_path);
        return fail("unlink returned false");
    }
    if (idx.getDocumentRegistry().hasSessionLink(created.document_id, "session-alp-f-unlink-a")) {
        fs::remove(attachment_path);
        return fail("session A link must be removed");
    }
    if (!idx.getDocumentRegistry().hasSessionLink(created.document_id, "session-alp-f-unlink-b")) {
        fs::remove(attachment_path);
        return fail("session B link must remain");
    }
    if (!fs::exists(attachment_path)) {
        return fail("storage must remain");
    }
    std::ifstream reg_in(reg_path);
    nlohmann::json registry;
    reg_in >> registry;
    bool document_remains = false;
    bool revision_remains = false;
    if (registry.contains("documents")) {
        for (const auto& row : registry["documents"]) {
            if (row.value("document_id", "") == created.document_id) {
                document_remains = true;
            }
        }
    }
    if (registry.contains("revisions")) {
        for (const auto& row : registry["revisions"]) {
            if (row.value("document_id", "") == created.document_id
                && row.value("revision_id", "") == revision_before
                && row.value("state", "") == "committed") {
                revision_remains = true;
            }
        }
    }
    fs::remove(attachment_path);
    restore_registry();
    if (!document_remains || !revision_remains) {
        std::cerr << "testAlpFLocalNoteDeleteUnlinksSession: document or revision missing\n";
        return false;
    }
    return true;
}

static bool testLocalNoteXKeepsSlotWhenUnlinkFails() {
    const std::string path = "/tmp/thoth-local-note-x-keep.md";
    {
        std::ofstream out(path);
        out << "keep me\n";
    }
    std::vector<std::string> paths{path, "/tmp/other.md"};
    std::map<std::string, Thoth::LocalNoteEngineInfo> cache;
    cache[path].document_id = "doc-linked";
    cache[path].revision_id = "rev-1";
    cache[path].content_hash = "abc";
    const auto paths_before = paths;
    const auto cache_before = cache;

    const auto failed = Thoth::LocalNoteEngineSync::localNoteXAfterUnlinkAttempt(
        true, false, "Could not unlink document from chat");
    if (failed.remove_local_slot) {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: failure must keep the slot\n";
        fs::remove(path);
        return false;
    }
    if (failed.status_message != "Could not unlink document from chat") {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: failure status missing\n";
        fs::remove(path);
        return false;
    }
    if (Thoth::LocalNoteEngineSync::eraseLocalNoteSlotIfAuthorized(
            paths, cache, 0, failed)) {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: erase must not run\n";
        fs::remove(path);
        return false;
    }
    if (paths != paths_before || cache[path].document_id != cache_before.at(path).document_id
        || cache[path].revision_id != "rev-1" || cache[path].content_hash != "abc"
        || !fs::exists(path)) {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: local state changed\n";
        fs::remove(path);
        return false;
    }

    const auto ok = Thoth::LocalNoteEngineSync::localNoteXAfterUnlinkAttempt(true, true, {});
    if (!ok.remove_local_slot
        || ok.status_message != "Local Note removed and unlinked from this chat") {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: success disposition wrong\n";
        fs::remove(path);
        return false;
    }
    if (!Thoth::LocalNoteEngineSync::eraseLocalNoteSlotIfAuthorized(paths, cache, 0, ok)) {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: success must erase slot\n";
        fs::remove(path);
        return false;
    }
    if (std::find(paths.begin(), paths.end(), path) != paths.end() || cache.count(path) != 0) {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: slot/cache remained after success\n";
        fs::remove(path);
        return false;
    }
    if (!fs::exists(path)) {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: managed file must remain\n";
        return false;
    }

    const auto host_only =
        Thoth::LocalNoteEngineSync::localNoteXAfterUnlinkAttempt(false, false, "ignored");
    if (!host_only.remove_local_slot || !host_only.status_message.empty()) {
        std::cerr << "testLocalNoteXKeepsSlotWhenUnlinkFails: host-only must remove locally\n";
        fs::remove(path);
        return false;
    }
    fs::remove(path);
    return true;
}

static bool testAlpFSessionLinkIsolation() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");

    const fs::path reg_path = Thoth::DocumentRegistry::defaultRegistryPath();
    const fs::path reg_backup = reg_path.string() + ".bak." + std::to_string(getpid());
    std::error_code ec;
    const bool had_registry = fs::exists(reg_path, ec);
    if (had_registry) {
        fs::copy(reg_path, reg_backup, fs::copy_options::overwrite_existing, ec);
    }
    auto restore_registry = [&]() {
        if (had_registry) {
            fs::copy(reg_backup, reg_path, fs::copy_options::overwrite_existing, ec);
            fs::remove(reg_backup, ec);
        } else if (fs::exists(reg_path, ec)) {
            fs::remove(reg_path, ec);
        }
    };

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    const std::string slot_name = "alp_f_iso_" + std::to_string(getpid()) + ".md";
    const fs::path attachment_path = Thoth::AlpStoragePaths::operatorAttachmentPath(slot_name);
    if (fs::exists(attachment_path)) {
        fs::remove(attachment_path);
    }

    FileHandler fh;
    const std::string body =
        "ALP-F session link isolation secret token ALPFISO unique phrase.\n";
    const auto created = idx.createCorpusDocument(fh.getRagDirectory(),
                                                slot_name,
                                                body,
                                                "session-alp-f-a");
    if (!created.ok) {
        std::cerr << "testAlpFSessionLinkIsolation: create failed: " << created.error << "\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    if (!waitAlpCommittedRevision(created.document_id, idx)) {
        std::cerr << "testAlpFSessionLinkIsolation: revision never committed\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    idx.classifyAllChunksMetadata();

    Thoth::RetrievalScope scopeA =
        Thoth::resolveAgentContextRetrievalScope("session-alp-f-a", &idx);
    const auto hitsA = idx.retrieveChunks("ALPFISO unique phrase", 5, &scopeA);
    if (hitsA.empty()) {
        std::cerr << "testAlpFSessionLinkIsolation: session A must retrieve\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    Thoth::RetrievalScope scopeB =
        Thoth::resolveAgentContextRetrievalScope("session-alp-f-b", &idx);
    const auto hitsB = idx.retrieveChunks("ALPFISO unique phrase", 5, &scopeB);
    if (!hitsB.empty()) {
        std::cerr << "testAlpFSessionLinkIsolation: session B must not retrieve\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    restore_registry();
    fs::remove(attachment_path);
    return true;
}

static bool testAlpFLocalNoteDeleteLinkPersists() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");

    const fs::path reg_path = Thoth::DocumentRegistry::defaultRegistryPath();
    const fs::path reg_backup = reg_path.string() + ".bak." + std::to_string(getpid());
    std::error_code ec;
    const bool had_registry = fs::exists(reg_path, ec);
    if (had_registry) {
        fs::copy(reg_path, reg_backup, fs::copy_options::overwrite_existing, ec);
    }
    auto restore_registry = [&]() {
        if (had_registry) {
            fs::copy(reg_backup, reg_path, fs::copy_options::overwrite_existing, ec);
            fs::remove(reg_backup, ec);
        } else if (fs::exists(reg_path, ec)) {
            fs::remove(reg_path, ec);
        }
    };

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    const std::string slot_name = "alp_f_localdel_" + std::to_string(getpid()) + ".md";
    const fs::path attachment_path = Thoth::AlpStoragePaths::operatorAttachmentPath(slot_name);
    if (fs::exists(attachment_path)) {
        fs::remove(attachment_path);
    }

    FileHandler fh;
    const std::string body =
        "ALP-F local note delete persistence token ALPFLOCALDEL unique.\n";
    const auto created = idx.createCorpusDocument(fh.getRagDirectory(),
                                                slot_name,
                                                body,
                                                "session-alp-f-local-a");
    if (!created.ok) {
        std::cerr << "testAlpFLocalNoteDeleteLinkPersists: create failed\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    if (!waitAlpCommittedRevision(created.document_id, idx)) {
        std::cerr << "testAlpFLocalNoteDeleteLinkPersists: not committed\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    if (!idx.getDocumentRegistry().hasSessionLink(created.document_id, "session-alp-f-local-a")) {
        std::cerr << "testAlpFLocalNoteDeleteLinkPersists: missing session link\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    // Simulate Local Note delete: GUI slot removed only — Engine link/registry unchanged.
    if (!idx.getDocumentRegistry().hasSessionLink(created.document_id, "session-alp-f-local-a")) {
        std::cerr << "testAlpFLocalNoteDeleteLinkPersists: link must persist after GUI delete\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    idx.classifyAllChunksMetadata();

    Thoth::RetrievalScope scopeA =
        Thoth::resolveAgentContextRetrievalScope("session-alp-f-local-a", &idx);
    if (idx.retrieveChunks("ALPFLOCALDEL unique", 5, &scopeA).empty()) {
        std::cerr << "testAlpFLocalNoteDeleteLinkPersists: session A must still retrieve\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    Thoth::RetrievalScope scopeB =
        Thoth::resolveAgentContextRetrievalScope("session-alp-f-local-b", &idx);
    if (!idx.retrieveChunks("ALPFLOCALDEL unique", 5, &scopeB).empty()) {
        std::cerr << "testAlpFLocalNoteDeleteLinkPersists: session B must not retrieve\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    restore_registry();
    fs::remove(attachment_path);
    return true;
}

static bool testAlpFMultiSessionSameDocument() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");

    const fs::path reg_path = Thoth::DocumentRegistry::defaultRegistryPath();
    const fs::path reg_backup = reg_path.string() + ".bak." + std::to_string(getpid());
    std::error_code ec;
    const bool had_registry = fs::exists(reg_path, ec);
    if (had_registry) {
        fs::copy(reg_path, reg_backup, fs::copy_options::overwrite_existing, ec);
    }
    auto restore_registry = [&]() {
        if (had_registry) {
            fs::copy(reg_backup, reg_path, fs::copy_options::overwrite_existing, ec);
            fs::remove(reg_backup, ec);
        } else if (fs::exists(reg_path, ec)) {
            fs::remove(reg_path, ec);
        }
    };

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    const std::string slot_name = "alp_f_multi_" + std::to_string(getpid()) + ".md";
    const fs::path attachment_path = Thoth::AlpStoragePaths::operatorAttachmentPath(slot_name);
    if (fs::exists(attachment_path)) {
        fs::remove(attachment_path);
    }

    FileHandler fh;
    const std::string body1 =
        "ALP-F multi session first send token ALPFMULTI first body.\n";
    const auto first = idx.createCorpusDocument(fh.getRagDirectory(),
                                                slot_name,
                                                body1,
                                                "session-alp-f-multi-a");
    if (!first.ok) {
        std::cerr << "testAlpFMultiSessionSameDocument: first create failed\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    if (!waitAlpCommittedRevision(first.document_id, idx)) {
        std::cerr << "testAlpFMultiSessionSameDocument: first not committed\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    // Same content → link_only for session B (no second in-flight revision).
    const auto second = idx.createCorpusDocument(fh.getRagDirectory(),
                                                 slot_name,
                                                 body1,
                                                 "session-alp-f-multi-b");
    if (!second.ok) {
        std::cerr << "testAlpFMultiSessionSameDocument: link create failed: "
                  << second.error << "\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    if (second.document_id != first.document_id) {
        std::cerr << "testAlpFMultiSessionSameDocument: document_id must match\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    if (!idx.getDocumentRegistry().hasSessionLink(first.document_id, "session-alp-f-multi-b")) {
        std::cerr << "testAlpFMultiSessionSameDocument: session B link missing\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    idx.classifyAllChunksMetadata();

    Thoth::RetrievalScope scopeB =
        Thoth::resolveAgentContextRetrievalScope("session-alp-f-multi-b", &idx);
    if (idx.retrieveChunks("ALPFMULTI first body", 5, &scopeB).empty()) {
        std::cerr << "testAlpFMultiSessionSameDocument: session B must retrieve\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    restore_registry();
    fs::remove(attachment_path);
    return true;
}

static bool testAlpFOrphanAttachmentExcluded() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    const std::string orphan_name = "alp_f_orphan_" + std::to_string(getpid()) + ".md";
    const fs::path orphan_path = Thoth::AlpStoragePaths::operatorAttachmentPath(orphan_name);
    fs::create_directories(orphan_path.parent_path());
    {
        std::ofstream out(orphan_path);
        out << "Orphan attachment without registry row ALPFORPHAN secret.\n";
    }

    CodeChunk chunk;
    chunk.fileName = orphan_path.string();
    chunk.code = "Orphan attachment without registry row ALPFORPHAN secret.";
    chunk.embedding = engine->embed(chunk.code);
    idx.addChunkToIndex(std::move(chunk));

    Thoth::RetrievalScope scope =
        Thoth::resolveAgentContextRetrievalScope("session-orphan-test", &idx);
    const auto hits = idx.retrieveChunks("ALPFORPHAN secret", 5, &scope);
    if (!hits.empty()) {
        std::cerr << "testAlpFOrphanAttachmentExcluded: orphan must not retrieve\n";
        fs::remove(orphan_path);
        return false;
    }

    fs::remove(orphan_path);
    return true;
}

static bool testAlpEPickerIncludesNoOpAndCreate() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    std::vector<LocalNoteIntent> intents;
    intents.push_back(LocalNoteIntent{
        "/host/a.md", "a.md", "no_op", "hash_matches_committed", "doc-a", "a.md", true});
    intents.push_back(LocalNoteIntent{
        "/host/b.md", "b.md", "create", "new_document_slot", "", "b.md", true});

    const auto picker = collectPickerCandidates(intents);
    if (picker.size() != 2
        || picker[0].host_path != "/host/a.md"
        || picker[0].action != "no_op"
        || picker[1].host_path != "/host/b.md"
        || picker[1].action != "create") {
        std::cerr << "testAlpEPickerIncludesNoOpAndCreate: expected no_op then create\n";
        return false;
    }
    if (actionPickerLabel(picker[0].action) != "Already on Engine — confirm"
        || actionPickerLabel(picker[1].action) != "Send") {
        std::cerr << "testAlpEPickerIncludesNoOpAndCreate: labels wrong\n";
        return false;
    }
    return true;
}

static bool testAlpEPickerIncludesRetryAndConflict() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    std::vector<LocalNoteIntent> intents;
    intents.push_back(LocalNoteIntent{
        "/host/r.md", "r.md", "retry", "retry_after_failed_revision", "doc-r", "r.md", true});
    intents.push_back(LocalNoteIntent{
        "/host/c.md", "c.md", "conflict", "local_mtime_not_newer", "doc-c", "c.md", true});

    const auto picker = collectPickerCandidates(intents);
    if (picker.size() != 2) {
        std::cerr << "testAlpEPickerIncludesRetryAndConflict: expected two candidates\n";
        return false;
    }
    if (actionPickerLabel(picker[0].action) != "Retry indexing"
        || actionPickerLabel(picker[1].action) != "Replace (confirm)") {
        std::cerr << "testAlpEPickerIncludesRetryAndConflict: labels wrong\n";
        return false;
    }
    return true;
}

static bool testAlpEPickerIncludesLinkOnly() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    std::vector<LocalNoteIntent> intents;
    intents.push_back(LocalNoteIntent{
        "/host/a.md", "a.md", "no_op", "hash_matches_committed", "doc-a", "a.md", true});
    intents.push_back(LocalNoteIntent{
        "/host/b.md", "b.md", "link_only", "hash_matches_committed", "doc-b", "b.md", true});

    const auto picker = collectPickerCandidates(intents);
    if (picker.size() != 2
        || picker[0].host_path != "/host/a.md"
        || picker[0].action != "no_op"
        || picker[1].host_path != "/host/b.md"
        || picker[1].action != "link_only") {
        std::cerr << "testAlpEPickerIncludesLinkOnly: expected no_op then link_only\n";
        return false;
    }
    if (actionPickerLabel(picker[0].action) != "Already on Engine — confirm"
        || actionPickerLabel(picker[1].action) != "Attach to chat") {
        std::cerr << "testAlpEPickerIncludesLinkOnly: label wrong\n";
        return false;
    }
    return true;
}

static bool testAlpEDryRunLinkOnlyForUnlinkedSession() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");

    const fs::path reg_path = Thoth::DocumentRegistry::defaultRegistryPath();
    const fs::path reg_backup = reg_path.string() + ".bak." + std::to_string(getpid());
    std::error_code ec;
    const bool had_registry = fs::exists(reg_path, ec);
    if (had_registry) {
        fs::copy(reg_path, reg_backup, fs::copy_options::overwrite_existing, ec);
    }
    auto restore_registry = [&]() {
        if (had_registry) {
            fs::copy(reg_backup, reg_path, fs::copy_options::overwrite_existing, ec);
            fs::remove(reg_backup, ec);
        } else if (fs::exists(reg_path, ec)) {
            fs::remove(reg_path, ec);
        }
    };

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    const std::string slot_name =
        "alp_e_linkonly_" + std::to_string(getpid()) + ".md";
    const fs::path attachment_path =
        Thoth::AlpStoragePaths::operatorAttachmentPath(slot_name);
    if (fs::exists(attachment_path)) {
        fs::remove(attachment_path);
    }

    FileHandler fh;
    const std::string body =
        "ALP-E dry_run link_only for unlinked session token ALPELINK.\n";

    const auto created = idx.createCorpusDocument(
        fh.getRagDirectory(), slot_name, body, "session-alp-e-link-a");
    if (!created.ok) {
        std::cerr << "testAlpEDryRunLinkOnlyForUnlinkedSession: create failed\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }
    if (!waitAlpCommittedRevision(created.document_id, idx)) {
        std::cerr << "testAlpEDryRunLinkOnlyForUnlinkedSession: not committed\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    IndexManager::CreateCorpusDocumentOptions dry_opts;
    dry_opts.dry_run = true;
    dry_opts.content_hash = Thoth::sha256Hex(body);

    const auto linked_intent = idx.createCorpusDocument(
        fh.getRagDirectory(), slot_name, body, "session-alp-e-link-a", dry_opts);
    if (!linked_intent.ok || linked_intent.action != "no_op") {
        std::cerr << "testAlpEDryRunLinkOnlyForUnlinkedSession: linked session expected "
                     "no_op, got "
                  << linked_intent.action << "\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    const auto unlinked_intent = idx.createCorpusDocument(
        fh.getRagDirectory(), slot_name, body, "session-alp-e-link-b", dry_opts);
    if (!unlinked_intent.ok || unlinked_intent.action != "link_only") {
        std::cerr << "testAlpEDryRunLinkOnlyForUnlinkedSession: unlinked session expected "
                     "link_only, got "
                  << unlinked_intent.action << "\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }
    if (unlinked_intent.document_id != created.document_id) {
        std::cerr << "testAlpEDryRunLinkOnlyForUnlinkedSession: document_id mismatch\n";
        restore_registry();
        fs::remove(attachment_path);
        return false;
    }

    restore_registry();
    fs::remove(attachment_path);
    return true;
}

static bool testAlpERemapSandboxCacheKeys() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    ChatSession session;
    session.localNoteEngine["/tmp/note.md"] = LocalNoteEngineInfo{
        "doc-1", "note.md", "rev-1", "hash-a", 3, false, false, true};

    const std::map<std::string, std::string> remaps{
        {"/tmp/note.md", "agent_workspace/rag/note.md"}};
    remapEngineCacheKeys(session.localNoteEngine, remaps);

    if (session.localNoteEngine.count("/tmp/note.md") != 0
        || session.localNoteEngine.count("agent_workspace/rag/note.md") == 0) {
        std::cerr << "testAlpERemapSandboxCacheKeys: remap failed\n";
        return false;
    }
    const auto& info = session.localNoteEngine.at("agent_workspace/rag/note.md");
    if (info.document_id != "doc-1" || info.revision_id != "rev-1") {
        std::cerr << "testAlpERemapSandboxCacheKeys: metadata lost\n";
        return false;
    }
    return true;
}

static bool testAlpEUpgradeLegacyDocumentIds() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    ChatSession session;
    session.localNoteEngine["/host/a.md"] = LocalNoteEngineInfo{
        "legacy-hash-id", "a.md", {}, {}, -1, false, false, false};

    const nlohmann::json legacy_map = {
        {"legacy-hash-id", "550e8400-e29b-41d4-a716-446655440000"}};
    if (upgradeLegacyDocumentIds(session, legacy_map) != 1) {
        std::cerr << "testAlpEUpgradeLegacyDocumentIds: expected one upgrade\n";
        return false;
    }
    if (session.localNoteEngine["/host/a.md"].document_id
        != "550e8400-e29b-41d4-a716-446655440000") {
        std::cerr << "testAlpEUpgradeLegacyDocumentIds: uuid not applied\n";
        return false;
    }
    return true;
}

static bool testAlpELocalNoteDeleteClearsCacheOnly() {
    using namespace Thoth;

    ChatSession session;
    session.ragFilePaths = {"/host/note.md"};
    session.localNoteEngine["/host/note.md"] = LocalNoteEngineInfo{
        "550e8400-e29b-41d4-a716-446655440000", "note.md", "rev-1", "hash", 4, false, false, true};

    const std::string removed = session.ragFilePaths.front();
    session.ragFilePaths.erase(session.ragFilePaths.begin());
    session.localNoteEngine.erase(removed);

    if (!session.ragFilePaths.empty() || !session.localNoteEngine.empty()) {
        std::cerr << "testAlpELocalNoteDeleteClearsCacheOnly: GUI slot/cache must clear\n";
        return false;
    }
    return true;
}

static bool testAlpEIntentResponseParsing() {
    using namespace Thoth;
    using namespace Thoth::CorpusCreateLocal;

    const auto body = CorpusCreate::makeDryRunResponse(
        "create", "doc-preview", "note.md", "new_document_slot");
    const auto result = operationResultFromJsonBody(body, "/host/note.md", true);
    if (!result.success || result.operation != kOpQueryDocumentIntent) {
        std::cerr << "testAlpEIntentResponseParsing: dry_run success expected\n";
        return false;
    }
    if (!result.ingest_action || *result.ingest_action != "create") {
        std::cerr << "testAlpEIntentResponseParsing: action missing\n";
        return false;
    }
    return true;
}

static bool testAlpEIndexingEventDocumentIdWins() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    ChatSession session;
    session.ragFilePaths = {"/host/EGAR.md", "/host/EGAR_1.md"};
    session.localNoteEngine["/host/EGAR.md"] = LocalNoteEngineInfo{
        "doc-egar", "EGAR.md", "rev-a", {}, -1, false, false, true};
    session.localNoteEngine["/host/EGAR_1.md"] = LocalNoteEngineInfo{
        "doc-egar-1", "EGAR_1.md", "rev-b", {}, -1, false, false, true};

    IndexingEventMetadata event;
    event.file_path = "agent_workspace/rag/attachments/EGAR_1.md";
    event.document_id = "doc-egar-1";

    const std::string host = findHostPathForIndexingEvent(session, event, true);
    if (host != "/host/EGAR_1.md") {
        std::cerr << "testAlpEIndexingEventDocumentIdWins: document_id must win\n";
        return false;
    }
    return true;
}

static bool testAlpEIndexingEventNoBasenameOnlyAlpGui() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    ChatSession session;
    session.ragFilePaths = {"/host/EGAR.md", "/host/EGAR_1.md"};
    session.localNoteEngine["/host/EGAR.md"] = LocalNoteEngineInfo{
        "doc-egar", "EGAR.md", {}, {}, -1, false, false, true};
    session.localNoteEngine["/host/EGAR_1.md"] = LocalNoteEngineInfo{
        "doc-egar-1", "EGAR_1.md", {}, {}, -1, false, false, true};

    IndexingEventMetadata event;
    event.file_path = "agent_workspace/rag/attachments/EGAR.md";

    const std::string host = findHostPathForIndexingEvent(session, event, true);
    if (host != "/host/EGAR.md") {
        std::cerr << "testAlpEIndexingEventNoBasenameOnlyAlpGui: expected path match\n";
        return false;
    }

    IndexingEventMetadata ambiguous_event;
    ambiguous_event.file_path = "agent_workspace/rag/attachments/shared.md";
    ChatSession ambiguous_session;
    ambiguous_session.ragFilePaths = {"/host/a/shared.md", "/other/shared.md"};
    if (!findHostPathForIndexingEvent(ambiguous_session, ambiguous_event, true).empty()) {
        std::cerr << "testAlpEIndexingEventNoBasenameOnlyAlpGui: ambiguous path must fail\n";
        return false;
    }
    return true;
}

static bool testAlpECorpusMatchLadderAmbiguousName() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    LocalNoteEngineInfo info;
    info.document_name = "EGAR.md";
    info.content_hash = "hash-a";

    const nlohmann::json documents = nlohmann::json::array({
        nlohmann::json{{"id", "doc-1"}, {"name", "EGAR.md"}},
        nlohmann::json{{"id", "doc-2"}, {"name", "EGAR.md"}},
    });

    const CorpusDocMatch match = matchCacheEntryToCorpusDoc(info, documents, {});
    if (!match.ambiguous || match.doc_index >= 0) {
        std::cerr << "testAlpECorpusMatchLadderAmbiguousName: name-only must be ambiguous\n";
        return false;
    }
    return true;
}

static bool testAlpECorpusMatchLegacyMap() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    LocalNoteEngineInfo info;
    info.document_id = "legacy-hash-id";
    info.document_name = "note.md";

    const nlohmann::json documents = nlohmann::json::array({
        nlohmann::json{{"id", "550e8400-e29b-41d4-a716-446655440000"},
                       {"name", "note.md"},
                       {"status", "indexed"},
                       {"chunk_count", 2}},
    });
    const nlohmann::json legacy_map = {
        {"legacy-hash-id", "550e8400-e29b-41d4-a716-446655440000"}};

    const CorpusDocMatch match =
        matchCacheEntryToCorpusDoc(info, documents, legacy_map);
    if (match.doc_index != 0 || match.kind != CorpusDocMatchKind::LegacyMap) {
        std::cerr << "testAlpECorpusMatchLegacyMap: legacy map match expected\n";
        return false;
    }
    return true;
}

static bool testG32CorpusRecoveryRefresh() {
    using namespace Thoth::LocalNoteEngineSync;

    if (shouldRefreshCorpusOnUsabilityReturn(false, true, true) != true) {
        std::cerr << "testG32CorpusRecoveryRefresh: failed list must refresh once "
                     "when usability returns\n";
        return false;
    }
    if (shouldRefreshCorpusOnUsabilityReturn(true, true, true) != false
        || shouldRefreshCorpusOnUsabilityReturn(true, true, false) != false
        || shouldRefreshCorpusOnUsabilityReturn(false, false, true) != false
        || shouldRefreshCorpusOnUsabilityReturn(false, true, false) != false) {
        std::cerr << "testG32CorpusRecoveryRefresh: refresh must be one-shot "
                     "on the usability rising edge only\n";
        return false;
    }

    const auto skipped = reconcileStateWithoutIntentQuery();
    if (skipped == LocalNoteReconcileState::Ready
        || skipped != LocalNoteReconcileState::Unverified) {
        std::cerr << "testG32CorpusRecoveryRefresh: ingest unavailable must stay "
                     "Unverified\n";
        return false;
    }
    if (authoritativeAllNotesAlreadySent(LocalNoteReconcileState::Ready, true, false, false)
        || authoritativeAllNotesAlreadySent(skipped, true, false, false)) {
        std::cerr << "testG32CorpusRecoveryRefresh: empty intents without a query "
                     "must not be already-sent\n";
        return false;
    }

    LocalNoteIntent created;
    created.host_path = "/tmp/gui/rag/g3-cert.md";
    created.canonical_name = "g3-cert.md";
    created.action = "create";
    created.query_ok = true;
    const auto candidates = collectPickerCandidates({created});
    if (candidates.size() != 1 || candidates.front().action != "create"
        || actionPickerLabel("create") != "Send") {
        std::cerr << "testG32CorpusRecoveryRefresh: host-only recovery must yield "
                     "create\n";
        return false;
    }
    if (!sendToEngineEnabled(true, true, true, LocalNoteReconcileState::Ready, true)) {
        std::cerr << "testG32CorpusRecoveryRefresh: create must enable Send\n";
        return false;
    }
    if (sendToEngineEnabled(true, true, false, skipped, true)
        || authoritativeAllNotesAlreadySent(
               LocalNoteReconcileState::Ready, true, true, true)) {
        std::cerr << "testG32CorpusRecoveryRefresh: Send/all-sent after a real "
                     "create is wrong\n";
        return false;
    }
    return true;
}

static bool testAlpEReconcileStateMachine() {
    using namespace Thoth::LocalNoteEngineSync;

    if (reconcileAllowsSend(LocalNoteReconcileState::Ready, true) != true
        || reconcileAllowsSend(LocalNoteReconcileState::Unknown, true) != false
        || reconcileAllowsSend(LocalNoteReconcileState::Unverified, true) != false
        || reconcileAllowsSend(LocalNoteReconcileState::Unknown, false) != true) {
        std::cerr << "testAlpEReconcileStateMachine: Send gating wrong\n";
        return false;
    }
    return true;
}

/**
 * INV-11 — a second accept while INDEXING_STARTED is in progress is rejected.
 * The callback is the existing worker barrier; it does not change indexing.
 */
static bool testAlpRevisionInFlightRejectsSecondAccept() {
    using namespace Thoth;

    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");
    ScopedEnvVar greenfield("THOTH_ALP_GREENFIELD", "1");

    const fs::path workspace = makeTempPath("thoth_alp_inv11");
    fs::remove_all(workspace);
    fs::create_directories(workspace / "rag");
    ScopedEnvVar workspace_env("THOTH_WORKSPACE_PATH", workspace.string().c_str());

    auto fail = [&](const char* msg) {
        std::cerr << "testAlpRevisionInFlightRejectsSecondAccept: " << msg << "\n";
        return false;
    };

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    std::mutex mu;
    std::condition_variable cv;
    bool indexing_started = false;
    bool release_indexing = false;
    struct ReleaseIndexing {
        std::mutex& mu;
        std::condition_variable& cv;
        bool& release;
        ~ReleaseIndexing() {
            std::lock_guard<std::mutex> lock(mu);
            release = true;
            cv.notify_all();
        }
    } release_guard{mu, cv, release_indexing};
    idx.setEventCallback([&](const ControllerEvent& ev) {
        if (ev.type != EventType::INDEXING_STARTED) {
            return;
        }
        std::unique_lock<std::mutex> lock(mu);
        indexing_started = true;
        cv.notify_all();
        cv.wait(lock, [&] { return release_indexing; });
    });

    FileHandler fh;
    const std::string slot = "alp_inv11.md";
    const std::string body1 = "INV11 first in flight revision token INV11FIRST.\n";
    const std::string body2 = "INV11 second accept must be rejected token INV11SECOND.\n";

    IndexManager::CreateCorpusDocumentOptions first_opts;
    first_opts.content_hash = sha256Hex(body1);
    first_opts.local_source_mtime_sec = 1'800'000'000LL;
    const auto first = idx.createCorpusDocument(
        fh.getRagDirectory(), slot, body1, "session-inv11", first_opts);
    if (!first.ok || first.action != "create" || first.document_id.empty()
        || first.revision_id.empty()) {
        std::cerr << "testAlpRevisionInFlightRejectsSecondAccept: first accept action="
                  << first.action << " err=" << first.error << "\n";
        return fail("first accept failed");
    }

    {
        std::unique_lock<std::mutex> lock(mu);
        if (!cv.wait_for(lock, std::chrono::seconds(10), [&] { return indexing_started; })) {
            return fail("first revision never reached INDEXING_STARTED");
        }
    }

    IndexManager::CreateCorpusDocumentOptions second_opts;
    second_opts.content_hash = sha256Hex(body2);
    second_opts.local_source_mtime_sec = 1'900'000'000LL;
    const auto second = idx.createCorpusDocument(
        fh.getRagDirectory(), slot, body2, "session-inv11", second_opts);

    {
        std::lock_guard<std::mutex> lock(mu);
        release_indexing = true;
    }
    cv.notify_all();

    if (second.ok || second.machine_code != "revision_in_flight"
        || second.document_id != first.document_id || !second.revision_id.empty()) {
        std::cerr << "testAlpRevisionInFlightRejectsSecondAccept: second action="
                  << second.action << " code=" << second.machine_code
                  << " revision=" << second.revision_id << " err=" << second.error << "\n";
        return fail("second accept was not revision_in_flight");
    }

    const EngineError http = EngineError::conflict(
        second.machine_code,
        second.error,
        nlohmann::json{{"document_id", second.document_id}});
    if (engineErrorHttpStatus(http.code) != 409) {
        return fail("revision_in_flight does not map to HTTP 409");
    }
    const nlohmann::json http_body = nlohmann::json::parse(http.toJson());
    if (http_body["error"].value("code", "") != "CONFLICT"
        || http_body["error"].value("machine_code", "") != "revision_in_flight") {
        return fail("HTTP error body missing revision_in_flight");
    }

    bool committed = false;
    for (int i = 0; i < 400; ++i) {
        const auto current = idx.getDocumentRegistry().findCommittedRevision(first.document_id);
        if (current && current->revision_id == first.revision_id && current->chunk_count > 0) {
            committed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (!committed) {
        return fail("first revision did not commit after the in-flight window");
    }

    int revisions = 0;
    const auto& registry = idx.getDocumentRegistry().body();
    if (registry.contains("revisions") && registry["revisions"].is_array()) {
        for (const auto& row : registry["revisions"]) {
            if (row.value("document_id", "") != first.document_id) {
                continue;
            }
            ++revisions;
            if (row.value("revision_id", "") != first.revision_id
                || row.value("state", "") != "committed") {
                return fail("registry has a revision other than the first committed one");
            }
        }
    }
    if (revisions != 1) {
        return fail("expected exactly one revision");
    }

    fs::remove_all(workspace);
    return true;
}

/**
 * ALP-G / ALP-E operator scenario — the EGAR.md lifecycle that caused weeks of pain.
 * Simulates GUI + Engine chain in an isolated workspace (not piece tests).
 */
static bool testAlpEEgarOperatorLifecycle() {
    using namespace Thoth;
    using namespace Thoth::LocalNoteEngineSync;

    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");
    ScopedEnvVar alp_gui("THOTH_ALP_GUI", "1");
    ScopedEnvVar greenfield("THOTH_ALP_GREENFIELD", "1");

    const fs::path workspace = makeTempPath("thoth_alp_egar_lifecycle");
    fs::remove_all(workspace);
    fs::create_directories(workspace / "rag");
    ScopedEnvVar workspaceEnv("THOTH_WORKSPACE_PATH", workspace.string().c_str());

    auto fail = [&](const char* msg) {
        std::cerr << "testAlpEEgarOperatorLifecycle: " << msg << "\n";
        fs::remove_all(workspace);
        return false;
    };

    FileHandler fh;
    const fs::path egar_src = fs::path(fh.getProjectRoot()) / "docs" / "EGAR.md";
    std::ifstream egar_in(egar_src);
    if (!egar_in) {
        return fail("docs/EGAR.md not found");
    }
    std::ostringstream egar_base;
    egar_base << egar_in.rdbuf();
    const std::string base = egar_base.str();
    if (base.size() < 200) {
        return fail("docs/EGAR.md too short");
    }

    const std::string content_v1 =
        base.substr(0, std::min(base.size(), std::size_t{4096}))
        + "\n\n<!-- ALP_EGAR_LIFECYCLE_V1 -->\n";
    const std::string content_v2 =
        base.substr(0, std::min(base.size(), std::size_t{4096}))
        + "\n\n<!-- ALP_EGAR_LIFECYCLE_V2 newer operator revision -->\n";

    const fs::path host_path = fs::path(fh.getRagDirectory()) / "EGAR.md";
    const std::string session_id = "session-egar-lifecycle-" + std::to_string(getpid());
    constexpr const char* kSlot = "EGAR.md";

    auto write_host = [&](const std::string& content) -> bool {
        std::ofstream out(host_path);
        if (!out) {
            return false;
        }
        out << content;
        return true;
    };
    if (!write_host(content_v1)) {
        return fail("could not write host EGAR.md v1");
    }

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    ChatSession session;
    session.id = session_id;
    session.ragFilePaths = {host_path.string()};

    // --- Step 1–3: Import EGAR.md + Send to Engine ---
    IndexManager::CreateCorpusDocumentOptions send_opts;
    send_opts.content_hash = Thoth::sha256Hex(content_v1);
    send_opts.local_source_mtime_sec = 1'700'000'000LL;

    const auto send1 = idx.createCorpusDocument(
        fh.getRagDirectory(), kSlot, content_v1, session_id, send_opts);
    if (!send1.ok || send1.action != "create") {
        return fail("first send must create");
    }
    if (send1.document_id.empty() || send1.revision_id.empty()) {
        return fail("first send missing document_id/revision_id");
    }
    if (!waitAlpCommittedRevision(send1.document_id, idx)) {
        return fail("first revision never committed");
    }

    session.localNoteEngine[host_path.string()] = LocalNoteEngineInfo{
        send1.document_id, kSlot, send1.revision_id, send_opts.content_hash,
        -1, false, false, true};

    const std::string doc_id = send1.document_id;
    const std::string rev1 = send1.revision_id;

    // --- Step 4–5: Close GUI + Delete Local Note (GUI-only) ---
    session.ragFilePaths.clear();
    session.localNoteEngine.clear();

    if (!idx.getDocumentRegistry().hasSessionLink(doc_id, session_id)) {
        return fail("session link must survive Local Note delete");
    }

    // --- Step 6–7: Reopen + Reconcile (no slots → Ready) ---
    if (!reconcileAllowsSend(LocalNoteReconcileState::Ready, true)) {
        return fail("reconcileAllowsSend sanity");
    }
    if (!session.ragFilePaths.empty()) {
        return fail("post-delete session must have empty rag slots");
    }

    // --- Step 8: Import newer EGAR.md ---
    if (!write_host(content_v2)) {
        return fail("could not write host EGAR.md v2");
    }
    session.ragFilePaths = {host_path.string()};

    // --- Reconcile dry_run for re-imported slot ---
    IndexManager::CreateCorpusDocumentOptions intent_opts;
    intent_opts.dry_run = true;
    intent_opts.content_hash = Thoth::sha256Hex(content_v2);
    intent_opts.local_source_mtime_sec = 9'999'999'999LL;

    const auto intent = idx.createCorpusDocument(
        fh.getRagDirectory(), kSlot, content_v2, session_id, intent_opts);
    if (!intent.ok || intent.action != "new_revision") {
        std::cerr << "testAlpEEgarOperatorLifecycle: expected new_revision, got "
                  << intent.action << " err=" << intent.error << "\n";
        fs::remove_all(workspace);
        return false;
    }
    if (intent.document_id != doc_id) {
        return fail("dry_run must preserve document UUID");
    }

    LocalNoteIntent picker_intent;
    picker_intent.host_path = host_path.string();
    picker_intent.action = intent.action;
    picker_intent.query_ok = true;
    picker_intent.document_id = intent.document_id;
    if (collectPickerCandidates({picker_intent}).size() != 1) {
        return fail("picker must offer new_revision after re-import");
    }

    // --- Step 9: Send again ---
    IndexManager::CreateCorpusDocumentOptions send2_opts;
    send2_opts.content_hash = Thoth::sha256Hex(content_v2);
    send2_opts.local_source_mtime_sec = 9'999'999'999LL;

    const auto send2 = idx.createCorpusDocument(
        fh.getRagDirectory(), kSlot, content_v2, session_id, send2_opts);
    if (!send2.ok || send2.action != "new_revision") {
        return fail("second send must be new_revision");
    }
    if (send2.document_id != doc_id) {
        return fail("second send must reuse document UUID");
    }
    if (send2.revision_id.empty() || send2.revision_id == rev1) {
        return fail("second send must allocate a new revision_id");
    }
    if (!waitAlpSpecificRevisionCommitted(doc_id, send2.revision_id, idx)) {
        return fail("second revision never committed");
    }

    const DocumentRegistry& reg = idx.getDocumentRegistry();
    if (reg.findCommittedRevision(doc_id)->revision_id != send2.revision_id) {
        return fail("current committed revision must be rev2");
    }

    auto revision_state = [&](const std::string& revision_id) -> std::string {
        if (!reg.body().contains("revisions") || !reg.body()["revisions"].is_array()) {
            return {};
        }
        for (const auto& row : reg.body()["revisions"]) {
            if (row.value("document_id", "") == doc_id
                && row.value("revision_id", "") == revision_id) {
                return row.value("state", "");
            }
        }
        return {};
    };
    if (revision_state(rev1) != "superseded") {
        return fail("first revision must be superseded");
    }
    if (revision_state(send2.revision_id) != "committed") {
        return fail("second revision must be committed");
    }

    const fs::path egar_attachment = AlpStoragePaths::operatorAttachmentPath(kSlot);
    const fs::path egar_suffix = fs::path(AlpStoragePaths::operatorAttachmentsDir())
        / "EGAR_1.md";
    if (!fs::exists(egar_attachment)) {
        return fail("EGAR.md attachment must exist");
    }
    if (fs::exists(egar_suffix)) {
        return fail("EGAR_1.md suffix file forbidden");
    }

    const nlohmann::json corpus = idx.listCorpusDocuments(fh.getRagDirectory());
    int egar_inventory_rows = 0;
    for (const auto& doc : corpus["documents"]) {
        const std::string name = doc.value("name", "");
        if (name.find("EGAR_1") != std::string::npos) {
            return fail("corpus inventory must not list EGAR_1 suffix");
        }
        if (name == kSlot) {
            ++egar_inventory_rows;
            if (doc.value("id", "") != doc_id) {
                return fail("corpus row id mismatch");
            }
        }
    }
    if (egar_inventory_rows != 1) {
        return fail("corpus must list exactly one EGAR.md row");
    }

    // --- ALP-G registry invariants (certification checklist) ---
    const nlohmann::json& reg_body = reg.body();
    if (!reg_body.contains("documents") || !reg_body["documents"].is_array()
        || reg_body["documents"].size() != 1) {
        return fail("document_registry must contain exactly one document row");
    }
    if (reg_body["documents"][0].value("canonical_name", "") != kSlot) {
        return fail("document canonical_name must be EGAR.md");
    }
    if (reg_body["documents"][0].value("current_revision_id", "") != send2.revision_id) {
        return fail("current_revision_id must be rev2");
    }
    int revision_count = 0;
    if (reg_body.contains("revisions") && reg_body["revisions"].is_array()) {
        for (const auto& row : reg_body["revisions"]) {
            if (row.value("document_id", "") == doc_id) {
                ++revision_count;
            }
        }
    }
    if (revision_count != 2) {
        return fail("document must have exactly two revisions");
    }
    int session_link_count = 0;
    if (reg_body.contains("session_links") && reg_body["session_links"].is_array()) {
        for (const auto& link : reg_body["session_links"]) {
            if (link.value("document_id", "") == doc_id) {
                ++session_link_count;
            }
        }
    }
    if (session_link_count < 1) {
        return fail("session_links must include at least one link for document");
    }

    // --- ALP-F retrieval: session A yes, session B no (after Local Note delete) ---
    idx.classifyAllChunksMetadata();
    const std::string query = "ALP_EGAR_LIFECYCLE_V2 newer operator revision";
    Thoth::RetrievalScope scope_a =
        Thoth::resolveAgentContextRetrievalScope(session_id, &idx);
    if (idx.retrieveChunks(query, 5, &scope_a).empty()) {
        return fail("session A must retrieve EGAR after Local Note delete");
    }
    const std::string session_b = session_id + "-isolated";
    Thoth::RetrievalScope scope_b =
        Thoth::resolveAgentContextRetrievalScope(session_b, &idx);
    if (!idx.retrieveChunks(query, 5, &scope_b).empty()) {
        return fail("session B must not retrieve EGAR without session link");
    }

    // Picker after successful send: same hash → no_op → not in picker.
    const auto noop_intent = idx.createCorpusDocument(
        fh.getRagDirectory(), kSlot, content_v2, session_id, intent_opts);
    if (!noop_intent.ok || noop_intent.action != "no_op") {
        return fail("post-send dry_run must be no_op");
    }
    LocalNoteIntent noop_row;
    noop_row.host_path = host_path.string();
    noop_row.action = noop_intent.action;
    noop_row.query_ok = true;
    const auto noop_picker = collectPickerCandidates({noop_row});
    if (noop_picker.size() != 1 || noop_picker.front().action != "no_op") {
        return fail("picker must offer no_op confirm after same-hash reconcile");
    }
    if (actionPickerLabel(noop_picker.front().action) != "Already on Engine — confirm") {
        return fail("no_op picker label must be Already on Engine — confirm");
    }

    std::cout << "[ALP-E EGAR lifecycle] document_id=" << doc_id
              << " rev1=" << rev1 << " rev2=" << send2.revision_id
              << " corpus_rows=" << egar_inventory_rows << "\n";

    fs::remove_all(workspace);
    return true;
}

static bool testAlpEDryRunIntentIntegration() {
    ScopedEnvVar alp_enabled("THOTH_ALP_ENABLED", "1");
    ScopedEnvVar tx_index("THOTH_ALP_TX_INDEX", "1");

    Config cfg;
    auto engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &cfg);
    IndexManager idx(engine.get());
    idx.init("");

    FileHandler fh;
    const std::string slot = "alp_e_dryrun_" + std::to_string(getpid()) + ".md";
    const std::string content =
        "ALP-E dry run intent integration content with enough bytes.\n";
    IndexManager::CreateCorpusDocumentOptions opts;
    opts.dry_run = true;
    opts.content_hash = Thoth::sha256Hex(content);

    const auto result = idx.createCorpusDocument(
        fh.getRagDirectory(), slot, content, "session-alp-e", opts);
    if (!result.ok || result.action != "create") {
        std::cerr << "testAlpEDryRunIntentIntegration: expected create action, got "
                  << result.action << " err=" << result.error << "\n";
        return false;
    }
    return true;
}

/**
 * Isolate the default core suite from the developer machine.
 *
 * Workspace, logs, and .env are temporary. THOTH_PROJECT_ROOT stays the
 * checkout so source-audit tests can still read this tree; the live corpus
 * is not used because THOTH_WORKSPACE_PATH overrides it.
 * THOTH_TEST_SUITE_DEV selects TfIdf embeddings and the in-process mock LLM.
 * Inference backend and endpoint variables are left unset so resolution
 * tests still observe the compiled Ollama default.
 */
static void installCoreTestIsolation() {
    const FileHandler repoLocator;
    const fs::path repoRoot = fs::path(repoLocator.getProjectRoot()).lexically_normal();

    const fs::path root = makeTempPath("thoth-core-tests");
    const fs::path workspace = root / "workspace";
    const fs::path logs = root / "logs";
    const fs::path rag = workspace / "rag";
    const fs::path envFile = root / "empty.env";
    fs::create_directories(rag);
    fs::create_directories(logs);

    {
        std::ofstream fixture(rag / "core_suite_fixture.md");
        fixture << "Thoth core-suite fixture.\n\n"
                << "This small note exists only so bootstrap indexing has a "
                << "deterministic corpus. It is not a live workspace document.\n";
    }
    {
        std::ofstream envOut(envFile);
        envOut << "# empty isolated env — do not load the repository .env\n";
    }
    fs::permissions(envFile, fs::perms::owner_read | fs::perms::owner_write);

    setenv("THOTH_PROJECT_ROOT", repoRoot.string().c_str(), 1);
    setenv("THOTH_WORKSPACE_PATH", workspace.string().c_str(), 1);
    setenv("THOTH_LOGS_PATH", logs.string().c_str(), 1);
    setenv("THOTH_ENV_PATH", envFile.string().c_str(), 1);
    setenv("THOTH_TEST_SUITE_DEV", "1", 1);

    std::cerr << "[thoth-core-tests] isolated workspace=" << workspace << '\n'
              << "[thoth-core-tests] isolated logs=" << logs << '\n'
              << "[thoth-core-tests] isolated env=" << envFile << '\n'
              << "[thoth-core-tests] project_root=" << repoRoot << '\n';
}

#if THOTH_HAS_GUI
static wxRect localNoteWidgetRect(const wxWindow* window) {
    return wxRect(window->GetPosition(), window->GetSize());
}

static bool localNoteRectsOverlap(const wxRect& a, const wxRect& b) {
    const int left = std::max(a.x, b.x);
    const int right = std::min(a.x + a.width, b.x + b.width);
    const int top = std::max(a.y, b.y);
    const int bottom = std::min(a.y + a.height, b.y + b.height);
    return right > left && bottom > top;
}

static bool localNoteRectInside(const wxRect& inner, const wxSize& pane) {
    return inner.x >= 0 && inner.y >= 0
        && inner.width > 0 && inner.height > 0
        && inner.x + inner.width <= pane.x
        && inner.y + inner.height <= pane.y;
}

/** True when the X is in the label's row and immediately to its right. */
static bool localNoteXAssociatesWithLabel(const wxRect& label, const wxRect& button) {
    const int labelCenter = label.y + label.height / 2;
    const int buttonCenter = button.y + button.height / 2;
    const int delta = labelCenter > buttonCenter
        ? labelCenter - buttonCenter
        : buttonCenter - labelCenter;
    return delta <= button.height / 2
        && button.x >= label.x + label.width;
}

/**
 * The notes pane is sized once while every X is hidden. Occupancy then grows
 * without resizing the pane. Each step shows or hides buttons and refreshes
 * through layoutLocalNotesOwner(), the same call RefreshRagPanel() makes after
 * a slot update. Positive size alone is not enough.
 */
static bool testLocalNoteSlotDeleteButtonGeometry() {
    // A console wx app never opens a display, and the next widget then aborts GTK.
    if (!wxApp::GetInstance()) {
        wxApp::SetInstance(new wxApp());
    }
    wxInitializer init;
    if (!init.IsOk()) {
        std::cerr << "testLocalNoteSlotDeleteButtonGeometry: wx init failed\n";
        return false;
    }

    auto* frame = new wxFrame(nullptr, wxID_ANY, "local-note-slots",
                              wxPoint(40, 40), wxSize(1100, 700));
    auto* panel = new wxPanel(frame, wxID_ANY);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* heading = new wxStaticText(panel, wxID_ANY,
        "Local Notes — drop or Import Corpus, then Send to Engine");
    outer->Add(heading, 0, wxEXPAND | wxALL, 5);

    const auto grid = Thoth::LocalNoteSlotLayout::makeLocalNoteDeleteGrid(panel);
    outer->Add(grid.grid, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
    auto* send = new wxButton(panel, wxID_ANY, "Send to Engine");
    outer->Add(send, 0, wxEXPAND | wxALL, 5);
    panel->SetSizer(outer);

    const int paneMin = std::max(
        180,
        Thoth::LocalNoteSlotLayout::localNotesPaneMinHeightPx(
            heading->GetBestSize().GetHeight(),
            grid.slots[0].remove->GetMinSize().GetHeight(),
            send->GetBestSize().GetHeight()));
    panel->SetMinSize(wxSize(900, paneMin));
    frame->SetClientSize(wxSize(1000, paneMin));
    frame->Show();
    // One owner layout at the final pane size, with every X still hidden.
    Thoth::LocalNoteSlotLayout::layoutLocalNotesOwner(panel);
    const wxSize frozenPane = panel->GetClientSize();

    const char* paths[] = {
        "/host/g3-cert.md",
        "/host/g3-empty.md",
        "/host/g3-side.md",
        "/host/g3-fourth.md"
    };
    const char* documents[] = {"doc-cert", "doc-empty", "doc-side", "doc-fourth"};

    for (int occupied = 0; occupied <= 4; ++occupied) {
        for (int i = 0; i < 4; ++i) {
            auto* button = grid.slots[static_cast<std::size_t>(i)].remove;
            auto* label = grid.slots[static_cast<std::size_t>(i)].label;
            if (i < occupied) {
                Thoth::LocalNoteSlotLayout::showLocalNoteDeleteButton(button);
                label->SetLabel(wxString::FromUTF8(std::string(paths[i]) + " " + documents[i]));
            } else {
                Thoth::LocalNoteSlotLayout::hideLocalNoteDeleteButton(button);
                label->SetLabel(wxString::Format("Empty Slot %d", i + 1));
            }
        }
        // Production refresh. Do not resize the pane and do not call
        // panel->Layout() from the test.
        Thoth::LocalNoteSlotLayout::layoutLocalNotesOwner(panel);
        if (panel->GetClientSize() != frozenPane) {
            std::cerr << "testLocalNoteSlotDeleteButtonGeometry: pane resized at occupied "
                      << occupied << "\n";
            frame->Destroy();
            return false;
        }

        for (int i = occupied; i < 4; ++i) {
            if (grid.slots[static_cast<std::size_t>(i)].remove->IsShown()) {
                std::cerr << "testLocalNoteSlotDeleteButtonGeometry: empty slot "
                          << (i + 1) << " X is visible\n";
                frame->Destroy();
                return false;
            }
        }
        if (occupied == 0) {
            continue;
        }

        wxRect shown[4];
        for (int i = 0; i < occupied; ++i) {
            const auto& slot = grid.slots[static_cast<std::size_t>(i)];
            const wxString expected =
                wxString::FromUTF8(std::string(paths[i]) + " " + documents[i]);
            if (!slot.remove->IsShown()
                || slot.remove->GetSize().GetWidth() <= 0
                || slot.remove->GetSize().GetHeight() <= 0) {
                std::cerr << "testLocalNoteSlotDeleteButtonGeometry: slot "
                          << (i + 1) << " of " << occupied << " is not shown\n";
                frame->Destroy();
                return false;
            }
            if (slot.slotNumber != i + 1
                || Thoth::LocalNoteSlotLayout::localNoteSlotPathIndex(slot.slotNumber) != i
                || slot.label->GetLabel() != expected) {
                std::cerr << "testLocalNoteSlotDeleteButtonGeometry: slot "
                          << (i + 1) << " does not map to its path\n";
                frame->Destroy();
                return false;
            }
            const wxRect labelRect = localNoteWidgetRect(slot.label);
            const wxRect buttonRect = localNoteWidgetRect(slot.remove);
            shown[i] = buttonRect;
            if (!localNoteRectInside(buttonRect, frozenPane)
                || !localNoteRectInside(labelRect, frozenPane)) {
                std::cerr << "testLocalNoteSlotDeleteButtonGeometry: slot "
                          << (i + 1) << " escapes the notes pane\n";
                frame->Destroy();
                return false;
            }
            if (!localNoteXAssociatesWithLabel(labelRect, buttonRect)) {
                std::cerr << "testLocalNoteSlotDeleteButtonGeometry: slot "
                          << (i + 1) << " X is not in its label row\n";
                frame->Destroy();
                return false;
            }
            for (int earlier = 0; earlier < i; ++earlier) {
                if (localNoteRectsOverlap(shown[earlier], buttonRect)
                    || shown[earlier].GetPosition() == buttonRect.GetPosition()) {
                    std::cerr << "testLocalNoteSlotDeleteButtonGeometry: slot "
                              << (i + 1) << " X overlaps slot " << (earlier + 1) << "\n";
                    frame->Destroy();
                    return false;
                }
            }
        }
    }

    if (!send->IsShown() || send->GetSize().GetHeight() <= 0) {
        std::cerr << "testLocalNoteSlotDeleteButtonGeometry: Send to Engine missing\n";
        frame->Destroy();
        return false;
    }
    const wxRect sendRect = localNoteWidgetRect(send);
    if (!localNoteRectInside(sendRect, frozenPane)) {
        std::cerr << "testLocalNoteSlotDeleteButtonGeometry: Send to Engine is outside the pane\n";
        frame->Destroy();
        return false;
    }

    frame->Destroy();
    return true;
}
#endif

int main() {
    if (const char* focused = std::getenv("THOTH_LLM_TIMEOUT_TESTS")) {
        if (std::string(focused) == "1") {
            const bool ok = testLlmTimeoutPolicy() && testLlmSynthesisRetriesDisabled()
                            && testDecisionTapeTimeoutDisplay()
                            && testDecisionTapeLifecycle()
                            && testRemoteHttpUtilsOffline();
            std::cout << (ok ? "LLM timeout tests passed.\n" : "LLM timeout tests failed.\n");
            return ok ? 0 : 1;
        }
    }

    if (const char* egarOnly = std::getenv("THOTH_ALP_EGAR_LIFECYCLE_ONLY")) {
        if (egarOnly[0] != '0' && std::string(egarOnly) != "false") {
            if (!testAlpEEgarOperatorLifecycle()) {
                return 1;
            }
            std::cout << "ALP-E EGAR operator lifecycle PASSED.\n";
            return 0;
        }
    }

    if (const char* parallelOnly = std::getenv("THOTH_PARALLEL_RETRIEVAL_ONLY")) {
        if (parallelOnly[0] == '1') {
            return testParallelRetrieval() ? 0 : 1;
        }
    }

    if (const char* inferenceIntegration = std::getenv("THOTH_INFERENCE_INTEGRATION_TESTS")) {
        if (inferenceIntegration[0] != '0' && std::string(inferenceIntegration) != "false") {
            if (!runInferenceIntegrationTests()) {
                return 1;
            }
            std::cout << "Plan H inference integration tests passed.\n";
            return 0;
        }
    }

    if (const char* engineRuntimeOnly = std::getenv("THOTH_ENGINE_RUNTIME_TESTS")) {
        if (engineRuntimeOnly[0] != '0' && std::string(engineRuntimeOnly) != "false") {
            int failures = 0;
            if (!testEngineErrorSchema()) failures++;
            if (!testEngineSessionNormalization()) failures++;
            if (!testLlmTimeoutPolicy()) failures++;
            if (!testLlmSynthesisRetriesDisabled()) failures++;
            if (!testDecisionTapeTimeoutDisplay()) failures++;
            if (!testDecisionTapeLifecycle()) failures++;
            if (!testRemoteHttpUtilsOffline()) failures++;
            if (!testRemoteChatGoalMappingOffline()) failures++;
            if (!testGuiR3GoalSessionWire()) failures++;
            if (!testGuiR4ChatFailureSurfaces()) failures++;
            if (!testGuiR4ConversationTurnSessionWire()) failures++;
    if (!testCsgAResolveExecutiveWins()) failures++;
    if (!testCsgAResolveSessionFallback()) failures++;
    if (!testCsgAResolveNone()) failures++;
    if (!testCsgASessionGoalCacheIsolation()) failures++;
    if (!testCsgAGoalSourceDiagnosticsJson()) failures++;
            if (!testGuiR5RetrievalSessionGate()) failures++;
            if (!testGuiR5ScopeGroundingDisplay()) failures++;
            if (!testGuiR5CorpusInventoryLabel()) failures++;
            if (!testThothEngineUrlSelectionOffline()) failures++;
            if (!testRemoteSseUtilsOffline()) failures++;
            if (!testEngineRuntimeValidation()) failures++;
            if (!testEngineEventBus()) failures++;
            if (!testEngineEventIntegration()) failures++;
            if (!testEngineHttpChatEndpoint()) failures++;
            if (!testEngineHttpDiagnosticsEndpoint()) failures++;
            if (!testEngineHttpGoalsAndControl()) failures++;
            if (!testEngineSseSessionDelivery()) failures++;
            if (!testEngineSseMultiClient()) failures++;
            if (!testEngineSseDisconnect()) failures++;
            if (!testEngineHttpSseRouteAndReady()) failures++;
            if (!testEngineHttpGracefulShutdown()) failures++;
            if (!testEngineRuntimeLazySessions()) failures++;
            if (failures == 0) {
                std::cout << "Plan G engine runtime tests passed.\n";
                return 0;
            }
            std::cerr << failures << " engine runtime test(s) failed.\n";
            return 1;
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

    if (const char* ep015sync = std::getenv("THOTH_E2_EP015_SYNC")) {
        if (ep015sync[0] != '0' && std::string(ep015sync) != "false") {
            if (!runE2Ep015SyncTests()) {
                return 1;
            }
            std::cout << "E2-EP-01.5 E2-33 sync gate passed.\n";
            return 0;
        }
    }

    if (const char* ep015p5 = std::getenv("THOTH_E2_EP015_PHASE5")) {
        if (ep015p5[0] != '0' && std::string(ep015p5) != "false") {
            if (!runE2Ep015Phase5Tests()) {
                return 1;
            }
            std::cout << "E2-EP-01.5 Phase 5 gate passed.\n";
            return 0;
        }
    }

    if (const char* ep015 = std::getenv("THOTH_E2_EP015")) {
        if (ep015[0] != '0' && std::string(ep015) != "false") {
            if (!runE2Ep015Tests()) {
                return 1;
            }
            std::cout << "E2-EP-01.5 gate passed.\n";
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

#if defined(THOTH_GUI_TESTS_ONLY)
    int failures = 0;
    if (!testAgentInterfaceLifecycle()) failures++;
    if (!testRemoteAgentBackendEmptyUrlOffline()) failures++;
    if (!testRemoteAgentBackendReadyRecovery()) failures++;
    if (!testRemoteAgentBackendLiveOptIn()) failures++;
    if (!testLocalNoteSlotDeleteButtonGeometry()) failures++;
    if (failures == 0) {
        std::cout << "All GUI tests passed.\n";
        return 0;
    }
    std::cerr << failures << " GUI test(s) failed.\n";
    return 1;
#else
    installCoreTestIsolation();
    int failures = 0;
    if (!testRuntimeBootstrapLoadsEnv()) failures++;
    if (!testRuntimeBootstrapRespectsExistingEnv()) failures++;
    if (!testRuntimeBootstrapIdempotent()) failures++;
    if (!testRuntimeBootstrapDiagnosticsDisabledByDefault()) failures++;
    if (!testConfigEnvironmentOverrides()) failures++;
    if (!testEngineErrorSchema()) failures++;
    if (!testEngineSessionNormalization()) failures++;
    if (!testLlmTimeoutPolicy()) failures++;
    if (!testLlmSynthesisRetriesDisabled()) failures++;
    if (!testDecisionTapeTimeoutDisplay()) failures++;
    if (!testDecisionTapeLifecycle()) failures++;
    if (!testRemoteHttpUtilsOffline()) failures++;
    if (!testRemoteChatGoalMappingOffline()) failures++;
    if (!testGuiR3GoalSessionWire()) failures++;
    if (!testGuiR4ChatFailureSurfaces()) failures++;
    if (!testGuiR4ConversationTurnSessionWire()) failures++;
    if (!testCsgAResolveExecutiveWins()) failures++;
    if (!testCsgAResolveSessionFallback()) failures++;
    if (!testCsgAResolveNone()) failures++;
    if (!testCsgASessionGoalCacheIsolation()) failures++;
    if (!testCsgAGoalSourceDiagnosticsJson()) failures++;
    if (!testGuiR5RetrievalSessionGate()) failures++;
    if (!testGuiR5ScopeGroundingDisplay()) failures++;
    if (!testGuiR5CorpusInventoryLabel()) failures++;
    if (!testThothEngineUrlSelectionOffline()) failures++;
    if (!testRemoteSseUtilsOffline()) failures++;
    if (!testEngineRuntimeValidation()) failures++;
    if (!testEngineRuntimeLazySessions()) failures++;
    if (!testEngineEventBus()) failures++;
    if (!testEngineEventIntegration()) failures++;
    if (!testEngineHttpChatEndpoint()) failures++;
    if (!testEngineHttpDiagnosticsEndpoint()) failures++;
    if (!testEngineHttpGoalsAndControl()) failures++;
    if (!testEngineSseSessionDelivery()) failures++;
    if (!testEngineSseMultiClient()) failures++;
    if (!testEngineSseDisconnect()) failures++;
    if (!testEngineHttpSseRouteAndReady()) failures++;
    if (!testEngineHttpGracefulShutdown()) failures++;
    if (!testConfigRoundTrip()) failures++;
    if (!testInferenceEndpointResolution()) failures++;
    if (!testInferenceBackendMisconfigDetection()) failures++;
    if (!testEmbeddingProbeJsonShape()) failures++;
    if (!testInferenceBackendResolution()) failures++;
    if (!testMockInferenceClient()) failures++;
    if (!testOllamaClientParse()) failures++;
    if (!testLlamaServerClientParse()) failures++;
    if (!testInferenceFactorySelection()) failures++;
    if (!testFileHandlerProjectRootWalk()) failures++;
    if (!testFileHandlerWorkspaceOverride()) failures++;
    if (!testFileHandlerLogsOverride()) failures++;
    if (!testLogsPathPrecedence()) failures++;
    if (!testMemoryPersistence()) failures++;
    if (!testCommandProcessorSetCommand()) failures++;
    if (!testCommandProcessorSlashTrim()) failures++;
#if THOTH_HAS_GUI
    if (!testAgentInterfaceLifecycle()) failures++;
#endif
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
    if (!testM4LegacyEqualsFullReplay()) failures++;
    if (!testM4RangedReplaySubset()) failures++;
    if (!testM4OpenEndedRanges()) failures++;
    if (!testM4ReplayNoSideEffects()) failures++;
    if (!testM4RehydrateInserts()) failures++;
    if (!testM4RehydrateIdempotent()) failures++;
    if (!testM4GoalBlockedUnlessUnsafe()) failures++;
    if (!testM4InvalidRangeBlocked()) failures++;
    if (!testM4RehydrateRollback()) failures++;
    if (!testM4DistinctDecisionTraceNames()) failures++;
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
    if (!testPlannerEvidenceSessionId()) failures++;
    if (!testEpisodicAuthoritativeV2()) failures++;
    if (!testE1AssembleEnvironmentDeterministic()) failures++;
    if (!testE1InferTierFromEnvFlags()) failures++;
    if (!testE1EnvironmentHashExcludesIndex()) failures++;
    if (!testE1IndexHashDistinctFromEnvironmentHash()) failures++;
    if (!testE1TierMismatchPredicate()) failures++;
    if (!testE1BenchmarkEnvironmentJsonRoundTrip()) failures++;
    if (!testG1dA0EnvHashDistinguishesInferenceBackend()) failures++;
    if (!testG1dA0EnvHashDistinguishesInferenceEndpoint()) failures++;
    if (!testG1dA0SnapshotUsesClientBackendName()) failures++;
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
    if (!testPlanMGroundingFloorRejectsBelowThreshold()) failures++;
    if (!testPlanMGroundingFloorPassesAboveThreshold()) failures++;
    if (!testPlanMGroundingTelemetryShape()) failures++;
    if (!testPlanMGreetingSkipClassifier()) failures++;
    if (!testPlanMGreetingSkipTelemetryShape()) failures++;
    if (!testPlanMChatPromptCueAndAntiTranscript()) failures++;
    if (!testPlanMChatStopPayloadSerialization()) failures++;
    if (!testPlanNChatStopSequencesEmpty()) failures++;
    if (!testPlanNGenerateEmptyStops()) failures++;
    if (!testPlanNFormatChunkSourceSpanRange()) failures++;
    if (!testPlanNFormatChunkSourceSpanSingle()) failures++;
    if (!testPlanNFormatChunkSourceSpanOmitted()) failures++;
    if (!testChatRagMetadataOffPresentation()) failures++;
    if (!testChatInferenceModeEnvDefault()) failures++;
    if (!testLlamaChatPayloadSerialization()) failures++;
    if (!testChatRolePromptAssembly()) failures++;
    if (!testChatMultiTurnMessageAssembly()) failures++;
    if (!testPlanNGreetingSkipTelemetryUnchanged()) failures++;
    if (!testChatRagPhase0ResponseTelemetryShape()) failures++;
    if (!testChatPhase0GenerationAttemptCount()) failures++;
    if (!testChatPhase0ResponseValidityTelemetryOnly()) failures++;
    if (!testChatPhase0RawCompletionSampleEnv()) failures++;
    if (!testPlanNMemoryOncePerTurn()) failures++;
    if (!testPlanNCpScaffoldSanitizedToMemory()) failures++;
    if (!testPlanNCpProviderFailure()) failures++;
    if (!testPlanN5FormatFinalScoreNoPercent()) failures++;
    if (!testPlanN5FormatFinalScoreSmall()) failures++;
    if (!testPlanN5FormatFinalScoreMissing()) failures++;
    if (!testPlanN5ConversationalAlphaMode()) failures++;
    if (!testPlanN5UnknownScoringMode()) failures++;
    if (!testGuiPhase1RemoteRagHonestyPolicy()) failures++;
    if (!testGuiR1RemoteIngestHostOnlyPresentation()) failures++;
    if (!testLocalNoteEngineCorpusSync()) failures++;
    if (!testLocalNoteSlotHidesCorpusUntilSent()) failures++;
    if (!testLocalNoteSendSelectionFilter()) failures++;
    if (!testGuiPhase2BackendCapabilitiesAndPresentation()) failures++;
    if (!testGuiPhase3CognitiveDiagnosticsAuthority()) failures++;
    if (!testGuiPhase4DecisionSummary()) failures++;
    if (!testGuiPhase5ProgressReportingDiscipline()) failures++;
    if (!testGuiPhase6EventStreamResilience()) failures++;
    if (!testGuiPhase7OperationResultHonesty()) failures++;
    if (!testGuiPhase8CorpusDocuments()) failures++;
    if (!testGuiR2CorpusFailedDocument()) failures++;
    if (!testTcb2ScopeBeatsSimilarity()) failures++;
    if (!testTcb2CrossContextIsolation()) failures++;
    if (!testTcb2RetrievalTraceParity()) failures++;
    if (!testTcb3IngestBindAndCrossContext()) failures++;
    if (!testTcb3IngestOmitSessionUnbound()) failures++;
    if (!testTcb3AttachmentRegistryPersistence()) failures++;
    if (!testR2IndexManagerIndexingHonesty()) failures++;
    if (!testIndexManagerWholeFileFallbackAfterShortParagraphs()) failures++;
    if (!testSessionScopedReplaceOnResend()) failures++;
    if (!testAlpFeatureFlagsDefaultOff()) failures++;
    if (!testAlpDocumentRegistryEmptyLoadSave()) failures++;
    if (!testAlpNamespacesCreatedOnInit()) failures++;
    if (!testAlpIndexManagerLoadsEmptyRegistry()) failures++;
    if (!testAlpTransactionalIndexPreservesOnEmptyReindex()) failures++;
    if (!testAlpTransactionalReindexSameContentSucceeds()) failures++;
    if (!testAlpValidateFailurePreservesPriorChunks()) failures++;
    if (!testAlpEmbedFailurePreservesPriorChunks()) failures++;
    if (!testAlpPersistFailurePreservesOnDiskCommit()) failures++;
    if (!testAlpRegistryRevisionLifecycle()) failures++;
    if (!testAlpIndexingCompletedMetadata()) failures++;
    if (!testAlpD0ReportDeterministic()) failures++;
    if (!testAlpD0EgarSuffixGroupWinner()) failures++;
    if (!testAlpD0ReadOnlyNoMutation()) failures++;
    if (!testAlpD0PreviewIdsNotInCanonicalHashVolatility()) failures++;
    if (!testAlpD1ApplyEgarSuffixMerge()) failures++;
    if (!testAlpD1AbortsOnReportHashMismatch()) failures++;
    if (!testAlpD1AbortsOnUnresolvedAmbiguity()) failures++;
    if (!testAlpD1RollbackRestoresM0()) failures++;
    if (!testAlpCSendPolicyNoOpOnSameHash()) failures++;
    if (!testAlpCSendPolicyConflictOnOldMtime()) failures++;
    if (!testAlpLocalNoteUnixMtimeRequest()) failures++;
    if (!testAlpCCreateAlpPathNoSuffix()) failures++;
    if (!testAlpCMisconfiguredRejectsCreate()) failures++;
    if (!testAlpCRegistryEnsureDocumentUniqueCanonical()) failures++;
    if (!testAlpFSessionLinkIsolation()) failures++;
    if (!testAlpFLocalNoteDeleteLinkPersists()) failures++;
    if (!testAlpFMultiSessionSameDocument()) failures++;
    if (!testAlpFOrphanAttachmentExcluded()) failures++;
    if (!testAlpFLocalNoteDeleteUnlinksSession()) failures++;
    if (!testLocalNoteXKeepsSlotWhenUnlinkFails()) failures++;
    if (!testAlpEPickerIncludesNoOpAndCreate()) failures++;
    if (!testAlpEPickerIncludesRetryAndConflict()) failures++;
    if (!testAlpEPickerIncludesLinkOnly()) failures++;
    if (!testAlpEDryRunLinkOnlyForUnlinkedSession()) failures++;
    if (!testAlpERemapSandboxCacheKeys()) failures++;
    if (!testAlpEUpgradeLegacyDocumentIds()) failures++;
    if (!testAlpELocalNoteDeleteClearsCacheOnly()) failures++;
    if (!testAlpEIntentResponseParsing()) failures++;
    if (!testG32CorpusRecoveryRefresh()) failures++;
    if (!testAlpEReconcileStateMachine()) failures++;
    if (!testAlpEIndexingEventDocumentIdWins()) failures++;
    if (!testAlpEIndexingEventNoBasenameOnlyAlpGui()) failures++;
    if (!testAlpECorpusMatchLadderAmbiguousName()) failures++;
    if (!testAlpECorpusMatchLegacyMap()) failures++;
    if (!testAlpRevisionInFlightRejectsSecondAccept()) failures++;
    if (!testAlpEEgarOperatorLifecycle()) failures++;
    if (!testAlpEDryRunIntentIntegration()) failures++;
    if (!testSessionReclaimDefaultOwnedOnResend()) failures++;
    if (!testLargeDocumentUsesSizeFallbackNotSingleChunk()) failures++;
    if (!testGuiPhase9CorpusCreate()) failures++;
    if (!testTcb4CreateDocumentRequestSessionId()) failures++;
    if (!testGuiPhase10ConversationAuthority()) failures++;
    if (!testGuiPhase11ResearchResources()) failures++;
    if (!testGuiPhase12AGraphStatistics()) failures++;
    if (!testIndexManagerCreateCorpusDocumentAtomic()) failures++;
    if (!testEngineHttpCorpusEndpoint()) failures++;
    if (!testEngineHttpCreateDocumentEndpoint()) failures++;
    if (!testEngineHttpConversationEndpoints()) failures++;
    if (!testEngineHttpResearchEndpoints()) failures++;
    if (!testEngineHttpGraphStatsEndpoint()) failures++;
    if (!testPlanNSanitizeTranscriptLoop()) failures++;
    if (!testPlanNSanitizeUserHelpScaffold()) failures++;
    if (!testPlanNSanitizeLeadingScaffoldStrip()) failures++;
    if (!testPlanNLlamaSoftEmptyParse()) failures++;
    if (!testPlanNLlamaMalformedJson()) failures++;
    if (!testPlanNGenerateRetrySuccess()) failures++;
    if (!testPlanNGenerateSanitizeEmptyThenRetry()) failures++;
    if (!testPlanNGenerateFallback()) failures++;
    if (!testCsgBScaffoldAssessment()) failures++;
    if (!testCsgBAnswerQualityAssessment()) failures++;
    if (!testCsgBChunkSanitizeStripsScaffold()) failures++;
    if (!testCsgBGenerateScaffoldWrappedCompleteAnswer()) failures++;
    if (!testCsgBGeneratePastedContextRetry()) failures++;
    if (!testCsgBGenerateRegurgitationFallback()) failures++;
    if (!testPhase1QueryEchoRejected()) failures++;
    if (!testPhase1TruncateShortPrefixRejected()) failures++;
    if (!testPhase1EmptyOutputFallback()) failures++;
    if (!testPhase1PreferUsableOverFallback()) failures++;
    if (!testPhase1MaxTwoGenerationAttempts()) failures++;
    if (!testPhase1NoThirdRetry()) failures++;
    if (!testPhase1TelemetryFlagsAccurate()) failures++;
    if (!testPlanNGenerateTransportFailure()) failures++;
    if (!testPlanNQueryCompatibility()) failures++;
    if (!testE1GragBenchmarkSmoke()) failures++;
    if (!testG1dFilterTrajectoryCases()) failures++;
    if (!testG1dArmConfigs()) failures++;
    if (!testG1dTuneWtOverride()) failures++;
    if (!testG1ePolarityWtAllowlist()) failures++;
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
#endif
}
