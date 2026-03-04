#include "AgentInterface.h"
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "file_handler.h"
#include "logger.h"
#include <json.hpp>

using json = nlohmann::json;

AgentInterface::AgentInterface() {
    try {
        plugin = std::make_unique<BasicAgentPlugin>();
        std::cerr << "[AgentInterface] BasicAgentPlugin initialized successfully.\n";
        StructuredLogger::instance().log(
            LogLevel::Info,
            "agent_interface",
            "agent_initialized",
            "BasicAgentPlugin initialized successfully");
    } catch (const std::exception& ex) {
        std::cerr << "[AgentInterface][Error] Failed to initialize agent: " 
                  << ex.what() << "\n";
        StructuredLogger::instance().log(
            LogLevel::Error,
            "agent_interface",
            "agent_init_failed",
            "Failed to initialize BasicAgentPlugin",
            {{"error", ex.what()}});
    } catch (...) {
        std::cerr << "[AgentInterface][Error] Unknown error during agent init.\n";
        StructuredLogger::instance().log(
            LogLevel::Error,
            "agent_interface",
            "agent_init_failed",
            "Unknown error during agent initialization");
    }
}

AgentInterface::~AgentInterface() {
    shutdownWorkers();
}

void AgentInterface::shutdownWorkers() {
    shuttingDown.store(true);

    std::vector<std::thread> activeWorkers;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        activeWorkers.swap(workers);
    }

    for (auto& worker : activeWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void AgentInterface::setSessionId(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(workersMutex);
    activeSessionId = sessionId;
}

bool AgentInterface::loadConversationMemory(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const std::string& summary) {
    if (!plugin) {
        return false;
    }

    try {
        plugin->setConversationMemory(messages, summary);
        return true;
    } catch (...) {
        return false;
    }
}

void AgentInterface::setRagFiles(const std::vector<std::string>& filePaths) {
    if (plugin) {
        plugin->setRagFiles(filePaths);
    }
}

std::string AgentInterface::processUserInput(const std::string& input, const std::string& requestId) {
    std::string resolvedRequestId = requestId;
    if (resolvedRequestId.empty()) {
        resolvedRequestId = "req-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    std::string sessionIdCopy;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        sessionIdCopy = activeSessionId;
    }

    if (shuttingDown.load()) {
        StructuredLogger::instance().log(
            LogLevel::Warn,
            "agent_interface",
            "input_rejected",
            "Input rejected: agent shutting down",
            {{"input_length", input.size()}},
            resolvedRequestId,
            sessionIdCopy);
        return "[Error: Agent is shutting down]";
    }

    if (!plugin) {
        StructuredLogger::instance().log(
            LogLevel::Error,
            "agent_interface",
            "input_rejected",
            "Input rejected: plugin not initialized",
            {{"input_length", input.size()}},
            resolvedRequestId,
            sessionIdCopy);
        return "[Error: BasicAgentPlugin not initialized]";
    }

    std::cerr << "[AgentInterface] Processing user input.\n";
    StructuredLogger::instance().log(
        LogLevel::Info,
        "agent_interface",
        "input_received",
        "Processing user input",
        {{"input_length", input.size()}},
        resolvedRequestId,
        sessionIdCopy);

    std::lock_guard<std::mutex> lock(workersMutex);
    workers.emplace_back([this, input, resolvedRequestId, sessionIdCopy]() {
        StructuredLogger::instance().setContext(resolvedRequestId, sessionIdCopy);
        try {
            std::string result = plugin->processInput(input);
            std::cerr << "[AgentInterface] Agent response ready.\n";
            StructuredLogger::instance().log(
                LogLevel::Info,
                "agent_interface",
                "response_ready",
                "Agent response generated",
                {{"response_length", result.size()}},
                resolvedRequestId,
                sessionIdCopy);

            if (!shuttingDown.load() && onResponse) {
                onResponse(result, resolvedRequestId);
            }

        } catch (const std::exception& e) {
            std::cerr << "[AgentInterface] Exception: " << e.what() << "\n";
            StructuredLogger::instance().log(
                LogLevel::Error,
                "agent_interface",
                "response_failed",
                "Exception while generating agent response",
                {{"error", e.what()}},
                resolvedRequestId,
                sessionIdCopy);
            if (!shuttingDown.load() && onResponse) {
                std::string err = "[Agent Error] " + std::string(e.what());
                onResponse(err, resolvedRequestId);
            }
        } catch (...) {
            std::cerr << "[AgentInterface] Unknown exception\n";
            StructuredLogger::instance().log(
                LogLevel::Error,
                "agent_interface",
                "response_failed",
                "Unknown exception while generating agent response",
                nlohmann::json::object(),
                resolvedRequestId,
                sessionIdCopy);
            if (!shuttingDown.load() && onResponse) {
                onResponse("[Agent Error] Unknown exception", resolvedRequestId);
            }
        }

        StructuredLogger::instance().clearContext();
    });

    return "[Processing...]";
}

std::string AgentInterface::getLatestDecisionTraceSummary() const {
    FileHandler fileHandler;
    const std::string tracePath = fileHandler.getAgentWorkspacePath("decision_trace.jsonl");

    std::ifstream in(tracePath);
    if (!in.is_open()) {
        return "No decision trace found yet.";
    }

    json lastTrace;
    bool found = false;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            json parsed = json::parse(line);
            if (!parsed.is_object()) continue;
            lastTrace = std::move(parsed);
            found = true;
        } catch (...) {
            continue;
        }
    }

    if (!found) {
        return "No valid decision trace entries available.";
    }

    std::ostringstream out;
    out << "Request: " << lastTrace.value("request_id", "unknown") << "\n";
    out << "Type: " << lastTrace.value("trace_type", "unknown") << "\n";
    out << "Result: " << lastTrace.value("result_summary", "n/a") << "\n";
    out << "Success: " << (lastTrace.value("success", false) ? "yes" : "no") << "\n";

    if (lastTrace.contains("duration_ms") && lastTrace["duration_ms"].is_number()) {
        out << "Duration: " << lastTrace["duration_ms"].get<long long>() << " ms\n";
    }

    out << "\nStages:\n";

    if (lastTrace.contains("stages") && lastTrace["stages"].is_array()) {
        for (const auto& stage : lastTrace["stages"]) {
            const std::string name = stage.value("name", "unnamed");
            const bool success = stage.value("success", false);
            const std::string summary = stage.value("summary", "");
            out << "- " << name << " [" << (success ? "ok" : "failed") << "]";
            if (!summary.empty()) {
                out << ": " << summary;
            }
            out << "\n";
        }
    } else {
        out << "- No stages recorded\n";
    }

    return out.str();
}

