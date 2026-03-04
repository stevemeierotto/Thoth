#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

int main() {
    int failures = 0;

    if (!testConfigRoundTrip()) failures++;
    if (!testMemoryPersistence()) failures++;
    if (!testCommandProcessorSetCommand()) failures++;
    if (!testAgentInterfaceLifecycle()) failures++;
    if (!testStructuredLoggerRedaction()) failures++;

    if (failures == 0) {
        std::cout << "All unit tests passed.\n";
        return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
