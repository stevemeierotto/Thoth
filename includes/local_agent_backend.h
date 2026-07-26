/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: in-process LocalAgentBackend (exclusive BasicAgentPlugin owner)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#pragma once

#include "i_agent_backend.h"

#include <memory>

class BasicAgentPlugin;

class LocalAgentBackend : public IAgentBackend {
public:
    LocalAgentBackend();
    ~LocalAgentBackend() override;

    void setEventHandler(std::function<void(const ControllerEvent&)> handler) override;

    void setSessionId(const std::string& sessionId) override;
    Thoth::OperationResult processInput(const std::string& input) override;
    Thoth::OperationResult executeGoal(const std::string& goal) override;
    Thoth::OperationResult pause() override;
    Thoth::OperationResult resume() override;
    Thoth::OperationResult abort() override;

    void setConversationMemory(
        const std::vector<std::pair<std::string, std::string>>& messages,
        const std::string& summary) override;
    void setConversationMemory(
        const std::vector<Memory::TimedMessage>& messages,
        const std::string& summary) override;
    void setRagFiles(const std::vector<std::string>& filePaths) override;
    void checkResumablePlan() override;

    nlohmann::json getStrategies() const override;
    nlohmann::json getTrajectories() const override;
    nlohmann::json getEpisodes() const override;
    nlohmann::json getExperiments() const override;
    nlohmann::json getGraphStats() const override;
    bool saveExperiment(const nlohmann::json& experimentJson) override;

    nlohmann::json getLatestDecisionSummary() const override;

    nlohmann::json listCorpusDocuments() const override;

    Thoth::OperationResult createCorpusDocument(const std::string& sourceFilePath) override;

    nlohmann::json createConversationSession() override;
    Thoth::OperationResult appendConversationTurn(const std::string& session_id,
                                                  const std::string& content) override;
    nlohmann::json getConversation(const std::string& session_id) const override;
    nlohmann::json getConversationSummary(const std::string& session_id) const override;

    bool isRemote() const override { return false; }
    Thoth::BackendCapabilities capabilities() const override {
        return Thoth::localBackendCapabilities();
    }

    Thoth::EventStreamSnapshot eventStreamSnapshot() const override;

private:
    std::unique_ptr<BasicAgentPlugin> plugin_;
};
