/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: internal agent backend interface (GUI layer)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#pragma once

#include "backend_capabilities.h"
#include "controller_event.h"
#include "engine_connection_state.h"
#include "operation_result.h"
#include "memory.h"
#include "json.hpp"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * Internal backend behind AgentInterface. Not part of the GUI contract —
 * MainFrame must not include or use this type.
 */
class IAgentBackend {
public:
    virtual ~IAgentBackend() = default;

    virtual void setEventHandler(std::function<void(const ControllerEvent&)> handler) = 0;

    virtual void setSessionId(const std::string& sessionId) = 0;
    virtual Thoth::OperationResult processInput(const std::string& input) = 0;
    virtual Thoth::OperationResult executeGoal(const std::string& goal) = 0;
    virtual Thoth::OperationResult pause() = 0;
    virtual Thoth::OperationResult resume() = 0;
    virtual Thoth::OperationResult abort() = 0;

    virtual void setConversationMemory(
        const std::vector<std::pair<std::string, std::string>>& messages,
        const std::string& summary) = 0;
    virtual void setConversationMemory(
        const std::vector<Memory::TimedMessage>& messages,
        const std::string& summary) = 0;
    virtual void setRagFiles(const std::vector<std::string>& filePaths) = 0;
    virtual void checkResumablePlan() = 0;

    virtual nlohmann::json getStrategies() const = 0;
    virtual nlohmann::json getTrajectories() const = 0;
    virtual nlohmann::json getEpisodes() const = 0;
    virtual nlohmann::json getExperiments() const = 0;
    virtual nlohmann::json getGraphStats() const = 0;
    virtual bool saveExperiment(const nlohmann::json& experimentJson) = 0;

    /**
     * Phase 4 — Engine-authored structured decision summary (Explain Plan).
     * Local may assemble from workspace; Remote fetches HTTP resource.
     */
    virtual nlohmann::json getLatestDecisionSummary() const = 0;

    /** Phase 8 — Engine-owned corpus document list. */
    virtual nlohmann::json listCorpusDocuments() const = 0;

    /** Phase 9 — create corpus document from a host Local Note path. */
    virtual Thoth::OperationResult createCorpusDocument(const std::string& sourceFilePath) = 0;

    /** Phase 10 — Engine-owned conversation authority. */
    virtual nlohmann::json createConversationSession() = 0;
    virtual Thoth::OperationResult appendConversationTurn(const std::string& session_id,
                                                          const std::string& content) = 0;
    virtual nlohmann::json getConversation(const std::string& session_id) const = 0;
    virtual nlohmann::json getConversationSummary(const std::string& session_id) const = 0;

    /** Identification / logging — prefer capabilities() for UI gating (Phase 2). */
    virtual bool isRemote() const = 0;

    /** Explicit feature surface — GUI must not infer from empty payloads (D10). */
    virtual Thoth::BackendCapabilities capabilities() const = 0;

    /** Phase 6 — SSE connection + engine health snapshot (Local: applies=false). */
    virtual Thoth::EventStreamSnapshot eventStreamSnapshot() const = 0;
};
