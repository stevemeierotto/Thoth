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
    std::optional<std::string> processInput(const std::string& input) override;
    void executeGoal(const std::string& goal) override;
    void pause() override;
    void resume() override;
    void abort() override;

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
    nlohmann::json getEpisodeSteps() const override;
    nlohmann::json getExperiments() const override;
    nlohmann::json getGraphStats() const override;
    bool saveExperiment(const nlohmann::json& experimentJson) override;

    bool isRemote() const override { return false; }

private:
    std::unique_ptr<BasicAgentPlugin> plugin_;
};
