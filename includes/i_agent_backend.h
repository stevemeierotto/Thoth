/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: internal agent backend interface (GUI layer)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#pragma once

#include "controller_event.h"
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
    virtual std::optional<std::string> processInput(const std::string& input) = 0;
    virtual void executeGoal(const std::string& goal) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void abort() = 0;

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
    virtual nlohmann::json getEpisodeSteps() const = 0;
    virtual nlohmann::json getExperiments() const = 0;
    virtual nlohmann::json getGraphStats() const = 0;
    virtual bool saveExperiment(const nlohmann::json& experimentJson) = 0;

    /** Identification / logging only — not used for backend selection in K1. */
    virtual bool isRemote() const = 0;
};
