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
#include "engine_connection_state.h"
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

    Thoth::OperationResult createCorpusDocument(
        const std::string& sourceFilePath,
        const Thoth::CorpusCreateGuiOptions& options = {}) override;

    Thoth::OperationResult queryCorpusDocumentIntent(
        const std::string& sourceFilePath) override;

    Thoth::OperationResult unlinkSessionDocument(
        const std::string& document_id,
        const std::string& session_id) override;

    nlohmann::json createConversationSession() override;
    Thoth::OperationResult appendConversationTurn(
        const std::string& session_id,
        const std::string& content,
        const std::optional<std::string>& active_goal = std::nullopt) override;
    nlohmann::json getConversation(const std::string& session_id) const override;
    nlohmann::json getConversationSummary(const std::string& session_id) const override;

    bool isRemote() const override { return true; }
    Thoth::BackendCapabilities capabilities() const override;

    Thoth::EventStreamSnapshot eventStreamSnapshot() const override;

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

    nlohmann::json fetchResearchCollection(const char* path) const;
    nlohmann::json fetchGraphStatisticsResource() const;

    /**
     * Probe /health + /ready unless a successful result is already cached.
     * A failed probe is not latched. A later request-path transport failure
     * clears a cached success so the next operation probes again.
     */
    bool ensureReady(std::string& error_out);

    /** Drop cached readiness and capability flags. Does not retry the caller. */
    void invalidateTransientReady();

    Thoth::OperationResult controlPost(const char* path_suffix, const char* op_name,
                                       const char* operation);

    Thoth::OperationResult postCorpusDocumentRequest(
        const std::string& sourceFilePath,
        const Thoth::CorpusCreateGuiOptions& options,
        bool dry_run);

    void startSseIfNeeded();
    void stopSse();
    void sseLoop();
    void sseReconnectLoop();
    bool probeEngineHealth(Thoth::EngineHealthState& out);
    void updateConnectionState(Thoth::EventConnectionState state);
    void publishSnapshotLocked();
    void sleepInterruptibleMs(int64_t delay_ms);
    static int64_t nowMs();

    std::string base_url_;
    std::mutex session_mutex_;
    std::string session_id_;

    std::mutex handler_mutex_;
    std::function<void(const ControllerEvent&)> event_handler_;

    mutable std::mutex ready_mutex_;
    bool ready_checked_{false};
    bool ready_ok_{false};
    bool events_sse_allowed_{false};
    bool ingest_allowed_{false};
    bool conversation_allowed_{false};
    bool strategies_allowed_{false};
    bool trajectories_allowed_{false};
    bool episodes_allowed_{false};
    bool graph_stats_allowed_{false};
    std::string ready_error_;

    std::mutex sse_start_mutex_;
    bool sse_started_{false};
    std::thread sse_thread_;
    std::atomic<bool> sse_cancel_{false};

    mutable std::mutex snapshot_mutex_;
    Thoth::EventStreamSnapshot snapshot_;
    std::atomic<int64_t> last_event_ms_{0};
    int reconnect_attempt_{0};
};
