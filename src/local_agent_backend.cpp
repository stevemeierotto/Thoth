/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: LocalAgentBackend implementation
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "local_agent_backend.h"
#include "basic_agent_plugin.h"
#include "decision_summary.h"
#include "corpus_documents.h"
#include "conversation_authority.h"
#include "corpus_create.h"
#include "corpus_create_local.h"
#include "engine_error.h"
#include "alp_sha256.h"
#include "research_resources.h"
#include "graph_statistics.h"
#include "engine_error.h"
#include "engine_connection_state.h"
#include "operation_result.h"
#include "file_handler.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

namespace {

void logBackendError(const char* operation, const std::exception& ex) {
    std::cerr << "[LocalAgentBackend] " << operation << " failed: " << ex.what() << "\n";
}

void logBackendError(const char* operation) {
    std::cerr << "[LocalAgentBackend] " << operation << " failed: unknown error\n";
}

} // namespace

LocalAgentBackend::LocalAgentBackend()
    : plugin_(std::make_unique<BasicAgentPlugin>()) {}

LocalAgentBackend::~LocalAgentBackend() = default;

void LocalAgentBackend::setEventHandler(std::function<void(const ControllerEvent&)> handler) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setEventHandler failed: plugin not initialized\n";
            return;
        }
        plugin_->onEvent = std::move(handler);
    } catch (const std::exception& ex) {
        logBackendError("setEventHandler", ex);
    } catch (...) {
        logBackendError("setEventHandler");
    }
}

void LocalAgentBackend::setSessionId(const std::string& sessionId) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setSessionId failed: plugin not initialized\n";
            return;
        }
        plugin_->setSessionId(sessionId);
    } catch (const std::exception& ex) {
        logBackendError("setSessionId", ex);
    } catch (...) {
        logBackendError("setSessionId");
    }
}

Thoth::OperationResult LocalAgentBackend::processInput(const std::string& input) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] processInput failed: plugin not initialized\n";
            return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", "plugin not initialized");
        }
        const auto reply = plugin_->processInput(input);
        if (reply.empty()) {
            return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", "no response from agent");
        }
        return Thoth::makeSuccess(Thoth::kOpChat, "Response received", reply);
    } catch (const std::exception& ex) {
        logBackendError("processInput", ex);
        return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", ex.what());
    } catch (...) {
        logBackendError("processInput");
        return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", "unknown error");
    }
}

Thoth::OperationResult LocalAgentBackend::executeGoal(const std::string& goal) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] executeGoal failed: plugin not initialized\n";
            return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", "plugin not initialized");
        }
        plugin_->executeGoal(goal);
        return Thoth::makeSuccess(Thoth::kOpGoal, "Goal accepted");
    } catch (const std::exception& ex) {
        logBackendError("executeGoal", ex);
        return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", ex.what());
    } catch (...) {
        logBackendError("executeGoal");
        return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", "unknown error");
    }
}

Thoth::OperationResult LocalAgentBackend::pause() {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] pause failed: plugin not initialized\n";
            return Thoth::makeFailure(Thoth::kOpPause, "Pause failed", "plugin not initialized");
        }
        plugin_->pause();
        return Thoth::makeSuccess(Thoth::kOpPause, "Agent execution paused");
    } catch (const std::exception& ex) {
        logBackendError("pause", ex);
        return Thoth::makeFailure(Thoth::kOpPause, "Pause failed", ex.what());
    } catch (...) {
        logBackendError("pause");
        return Thoth::makeFailure(Thoth::kOpPause, "Pause failed", "unknown error");
    }
}

Thoth::OperationResult LocalAgentBackend::resume() {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] resume failed: plugin not initialized\n";
            return Thoth::makeFailure(Thoth::kOpResume, "Resume failed", "plugin not initialized");
        }
        plugin_->resume();
        return Thoth::makeSuccess(Thoth::kOpResume, "Agent execution resumed");
    } catch (const std::exception& ex) {
        logBackendError("resume", ex);
        return Thoth::makeFailure(Thoth::kOpResume, "Resume failed", ex.what());
    } catch (...) {
        logBackendError("resume");
        return Thoth::makeFailure(Thoth::kOpResume, "Resume failed", "unknown error");
    }
}

Thoth::OperationResult LocalAgentBackend::abort() {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] abort failed: plugin not initialized\n";
            return Thoth::makeFailure(Thoth::kOpAbort, "Abort failed", "plugin not initialized");
        }
        plugin_->abort();
        return Thoth::makeSuccess(Thoth::kOpAbort, "Agent execution aborted");
    } catch (const std::exception& ex) {
        logBackendError("abort", ex);
        return Thoth::makeFailure(Thoth::kOpAbort, "Abort failed", ex.what());
    } catch (...) {
        logBackendError("abort");
        return Thoth::makeFailure(Thoth::kOpAbort, "Abort failed", "unknown error");
    }
}

void LocalAgentBackend::setConversationMemory(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const std::string& summary) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setConversationMemory failed: plugin not initialized\n";
            return;
        }
        plugin_->setConversationMemory(messages, summary);
    } catch (const std::exception& ex) {
        logBackendError("setConversationMemory", ex);
    } catch (...) {
        logBackendError("setConversationMemory");
    }
}

void LocalAgentBackend::setConversationMemory(
    const std::vector<Memory::TimedMessage>& messages,
    const std::string& summary) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setConversationMemory(timed) failed: plugin not initialized\n";
            return;
        }
        plugin_->setConversationMemory(messages, summary);
    } catch (const std::exception& ex) {
        logBackendError("setConversationMemory(timed)", ex);
    } catch (...) {
        logBackendError("setConversationMemory(timed)");
    }
}

void LocalAgentBackend::setRagFiles(const std::vector<std::string>& filePaths) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setRagFiles failed: plugin not initialized\n";
            return;
        }
        plugin_->setRagFiles(filePaths);
    } catch (const std::exception& ex) {
        logBackendError("setRagFiles", ex);
    } catch (...) {
        logBackendError("setRagFiles");
    }
}

void LocalAgentBackend::checkResumablePlan() {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] checkResumablePlan failed: plugin not initialized\n";
            return;
        }
        plugin_->checkResumablePlan();
    } catch (const std::exception& ex) {
        logBackendError("checkResumablePlan", ex);
    } catch (...) {
        logBackendError("checkResumablePlan");
    }
}

nlohmann::json LocalAgentBackend::getStrategies() const {
    try {
        if (!plugin_) {
            return Thoth::ResearchResources::unavailableFetchResult();
        }
        return plugin_->listStrategies();
    } catch (const std::exception& ex) {
        logBackendError("getStrategies", ex);
        return Thoth::ResearchResources::unavailableFetchResult();
    } catch (...) {
        logBackendError("getStrategies");
        return Thoth::ResearchResources::unavailableFetchResult();
    }
}

nlohmann::json LocalAgentBackend::getTrajectories() const {
    try {
        if (!plugin_) {
            return Thoth::ResearchResources::unavailableFetchResult();
        }
        return plugin_->listTrajectories();
    } catch (const std::exception& ex) {
        logBackendError("getTrajectories", ex);
        return Thoth::ResearchResources::unavailableFetchResult();
    } catch (...) {
        logBackendError("getTrajectories");
        return Thoth::ResearchResources::unavailableFetchResult();
    }
}

nlohmann::json LocalAgentBackend::getEpisodes() const {
    try {
        if (!plugin_) {
            return Thoth::ResearchResources::unavailableFetchResult();
        }
        return plugin_->listEpisodes();
    } catch (const std::exception& ex) {
        logBackendError("getEpisodes", ex);
        return Thoth::ResearchResources::unavailableFetchResult();
    } catch (...) {
        logBackendError("getEpisodes");
        return Thoth::ResearchResources::unavailableFetchResult();
    }
}

nlohmann::json LocalAgentBackend::getExperiments() const {
    if (!plugin_) {
        std::cerr << "[LocalAgentBackend] getExperiments failed: plugin not initialized\n";
        return json::array();
    }
    try {
        auto exprs = plugin_->getAllExperiments();
        json result = json::array();
        for (const auto& e : exprs) {
            json config = json::object();
            json results = json::object();
            try {
                config = json::parse(e.configuration_json);
            } catch (...) {
            }
            try {
                results = json::parse(e.results_json);
            } catch (...) {
            }
            result.push_back({
                {"experiment_id", e.experiment_id},
                {"name", e.name},
                {"hypothesis", e.hypothesis},
                {"configuration", config},
                {"results", results},
                {"created_at", e.created_at},
                {"status", e.status}
            });
        }
        return result;
    } catch (const std::exception& ex) {
        logBackendError("getExperiments", ex);
        return json::array();
    } catch (...) {
        logBackendError("getExperiments");
        return json::array();
    }
}

nlohmann::json LocalAgentBackend::getGraphStats() const {
    try {
        if (!plugin_) {
            return Thoth::GraphStatistics::unavailableFetchResult();
        }
        return plugin_->getGraphStatisticsResource();
    } catch (const std::exception& ex) {
        logBackendError("getGraphStats", ex);
        return Thoth::GraphStatistics::unavailableFetchResult();
    } catch (...) {
        logBackendError("getGraphStats");
        return Thoth::GraphStatistics::unavailableFetchResult();
    }
}

bool LocalAgentBackend::saveExperiment(const nlohmann::json& experimentJson) {
    if (!plugin_) {
        std::cerr << "[LocalAgentBackend] saveExperiment failed: plugin not initialized\n";
        return false;
    }
    try {
        Memory::CognateExperimentRecord rec;
        rec.experiment_id = experimentJson.value("experiment_id", "");
        rec.name = experimentJson.value("name", "Unnamed Experiment");
        rec.hypothesis = experimentJson.value("hypothesis", "");
        rec.configuration_json = experimentJson.value("configuration", json::object()).dump();
        rec.results_json = experimentJson.value("results", json::object()).dump();
        rec.created_at = experimentJson.value("created_at", 0LL);
        rec.status = experimentJson.value("status", "pending");
        return plugin_->saveExperiment(rec);
    } catch (const std::exception& ex) {
        logBackendError("saveExperiment", ex);
        return false;
    } catch (...) {
        logBackendError("saveExperiment");
        return false;
    }
}

nlohmann::json LocalAgentBackend::getLatestDecisionSummary() const {
    try {
        FileHandler fh;
        const std::string path = fh.getAgentWorkspacePath("decision_trace.jsonl");
        return Thoth::DecisionSummary::loadLatestFromDecisionTraceFile(path);
    } catch (const std::exception& ex) {
        logBackendError("getLatestDecisionSummary", ex);
        return Thoth::DecisionSummary::emptyV1Summary();
    } catch (...) {
        logBackendError("getLatestDecisionSummary");
        return Thoth::DecisionSummary::emptyV1Summary();
    }
}

Thoth::EventStreamSnapshot LocalAgentBackend::eventStreamSnapshot() const {
    const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    return Thoth::localEventStreamSnapshot(now_ms);
}

nlohmann::json LocalAgentBackend::listCorpusDocuments() const {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] listCorpusDocuments failed: plugin not initialized\n";
            return Thoth::CorpusDocuments::emptyV1List();
        }
        return plugin_->listCorpusDocuments();
    } catch (const std::exception& ex) {
        logBackendError("listCorpusDocuments", ex);
        return Thoth::CorpusDocuments::emptyV1List();
    } catch (...) {
        logBackendError("listCorpusDocuments");
        return Thoth::CorpusDocuments::emptyV1List();
    }
}

Thoth::OperationResult LocalAgentBackend::queryCorpusDocumentIntent(
    const std::string& sourceFilePath) {
    using namespace Thoth;
    using namespace Thoth::CorpusCreateLocal;
    try {
        if (!plugin_) {
            return makeFailure(kOpQueryDocumentIntent,
                               "Could not query send intent",
                               "plugin not initialized");
        }
        std::string read_err;
        const auto payload = readLocalNoteFile(sourceFilePath, read_err);
        if (!payload) {
            return makeFailure(kOpQueryDocumentIntent,
                               "Could not query send intent",
                               read_err.empty() ? sourceFilePath : read_err);
        }
        const auto create_req = makeCreateRequest(
            *payload, plugin_->getActiveSessionId(), sourceFilePath, {}, true);
        const nlohmann::json body = plugin_->createCorpusDocument(create_req);
        return operationResultFromJsonBody(body, sourceFilePath, true);
    } catch (const Thoth::EngineException& ex) {
        auto result = operationResultFromEngineException(ex, sourceFilePath);
        result.operation = kOpQueryDocumentIntent;
        result.user_message = "Could not query send intent";
        return result;
    } catch (const std::exception& ex) {
        return makeFailure(kOpQueryDocumentIntent,
                           "Could not query send intent",
                           ex.what());
    } catch (...) {
        return makeFailure(kOpQueryDocumentIntent, "Could not query send intent");
    }
}

Thoth::OperationResult LocalAgentBackend::createCorpusDocument(
    const std::string& sourceFilePath,
    const Thoth::CorpusCreateGuiOptions& options) {
    using namespace Thoth;
    using namespace Thoth::CorpusCreateLocal;
    try {
        if (!plugin_) {
            return makeFailure(CorpusCreate::kOperationName,
                               "Document could not be sent to Engine",
                               "plugin not initialized");
        }
        std::string read_err;
        const auto payload = readLocalNoteFile(sourceFilePath, read_err);
        if (!payload) {
            return makeFailure(CorpusCreate::kOperationName,
                               read_err == "Local Note file not found"
                                   ? "Local Note file not found"
                                   : (read_err == "No Local Note selected"
                                          ? "No Local Note selected"
                                          : "Document could not be sent to Engine"),
                               read_err.empty() ? sourceFilePath : read_err);
        }
        const auto create_req = makeCreateRequest(
            *payload, plugin_->getActiveSessionId(), sourceFilePath, options, false);
        const nlohmann::json body = plugin_->createCorpusDocument(create_req);
        return operationResultFromJsonBody(body, sourceFilePath, false);
    } catch (const Thoth::EngineException& ex) {
        return operationResultFromEngineException(ex, sourceFilePath);
    } catch (const std::exception& ex) {
        logBackendError("createCorpusDocument", ex);
        return makeFailure(CorpusCreate::kOperationName,
                           "Document could not be sent to Engine",
                           ex.what());
    } catch (...) {
        logBackendError("createCorpusDocument");
        return makeFailure(CorpusCreate::kOperationName,
                           "Document could not be sent to Engine");
    }
}

nlohmann::json LocalAgentBackend::createConversationSession() {
    try {
        if (!plugin_) {
            return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
        }
        return plugin_->createConversationSession();
    } catch (const Thoth::EngineException& ex) {
        logBackendError("createConversationSession", ex);
        return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
    } catch (...) {
        logBackendError("createConversationSession");
        return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
    }
}

Thoth::OperationResult LocalAgentBackend::appendConversationTurn(const std::string& session_id,
                                                                 const std::string& content) {
    using namespace Thoth;
    try {
        if (!plugin_) {
            return makeFailure(kOpChat, "Failed to send", "plugin not initialized");
        }
        const nlohmann::json body = plugin_->appendUserTurn(session_id, content);
        std::string err;
        if (!ConversationAuthority::hasRequiredAppendTurnFields(body, err)) {
            return makeFailure(kOpChat, "Failed to send", err);
        }
        const std::string assistant = body["assistant"]["content"].get<std::string>();
        return makeSuccess(kOpChat, "Response received", assistant);
    } catch (const Thoth::EngineException& ex) {
        return makeFailure(kOpChat, "Failed to send", ex.error().message, true);
    } catch (const std::exception& ex) {
        logBackendError("appendConversationTurn", ex);
        return makeFailure(kOpChat, "Failed to send", ex.what());
    } catch (...) {
        logBackendError("appendConversationTurn");
        return makeFailure(kOpChat, "Failed to send");
    }
}

nlohmann::json LocalAgentBackend::getConversation(const std::string& session_id) const {
    try {
        if (!plugin_) {
            return Thoth::ConversationAuthority::emptyConversation(session_id);
        }
        return plugin_->getConversationForSession(session_id);
    } catch (...) {
        return Thoth::ConversationAuthority::emptyConversation(session_id);
    }
}

nlohmann::json LocalAgentBackend::getConversationSummary(const std::string& session_id) const {
    try {
        if (!plugin_) {
            return Thoth::ConversationAuthority::makeSummaryResponse(session_id, "");
        }
        return plugin_->getConversationSummaryForSession(session_id);
    } catch (...) {
        return Thoth::ConversationAuthority::makeSummaryResponse(session_id, "");
    }
}
