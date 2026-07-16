/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: RemoteAgentBackend (libcurl → thoth-engine HTTP + SSE)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 *
 * May link into production binaries but must not be constructed by
 * AgentInterface until K4. Tests / manual harnesses only.
 */
#pragma once

#include "i_agent_backend.h"
#include "remote_agent_http_utils.h"
#include "remote_agent_sse_utils.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

class RemoteAgentBackend : public IAgentBackend {
public:
    explicit RemoteAgentBackend(std::string base_url);
    ~RemoteAgentBackend() override;

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

    bool isRemote() const override { return true; }

    /** Exposed for tests — normalized base URL after construction. */
    const std::string& baseUrl() const { return base_url_; }

    /** Invoked from SSE libcurl write callback (must be accessible). */
    void dispatchSseFrames(std::string& buffer);

private:
    struct HttpResult {
        bool transport_ok{false};
        long status{0};
        std::string body;
        std::string transport_error;
    };

    HttpResult httpGet(const std::string& path, long timeout_sec) const;
    HttpResult httpPostJson(const std::string& path,
                            const std::string& json_body,
                            long timeout_sec) const;

    /** First use: /health + /ready; caches result. No retries. */
    bool ensureReady(std::string& error_out);

    void controlPost(const char* path_suffix, const char* op_name);

    void startSseIfNeeded();
    void stopSse();
    void sseLoop();

    std::string base_url_;
    std::mutex session_mutex_;
    std::string session_id_{"default"};

    std::mutex handler_mutex_;
    std::function<void(const ControllerEvent&)> event_handler_;

    std::mutex ready_mutex_;
    bool ready_checked_{false};
    bool ready_ok_{false};
    bool events_sse_allowed_{false};
    std::string ready_error_;

    std::mutex sse_start_mutex_;
    bool sse_started_{false};
    std::thread sse_thread_;
    std::atomic<bool> sse_cancel_{false};
};
