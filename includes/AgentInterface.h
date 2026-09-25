#pragma once
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <utility>
#include <queue>
#include <optional>
#include <condition_variable>
#include <future>
#include <wx/stdpaths.h>
#include <wx/filename.h>

#include "backend_capabilities.h"
#include "controller_event.h"
#include "engine_connection_state.h"
#include "memory.h"
#include "operation_result.h"
#include "corpus_documents.h"
#include "corpus_create.h"
#include "conversation_authority.h"
#include "json.hpp"

class IAgentBackend;

class AgentInterface {
public:
    AgentInterface();
    ~AgentInterface();

    // Helper to locate benchmark binaries without hardcoding paths.
    // Searches executable dir, sibling build/ dirs, and PATH.
    static wxString GetBenchmarkBinaryPath(const wxString& binaryName);

    // Processes user input asynchronously
    void processUserInput(const std::string& input, const std::string& requestId = "");
    void setSessionId(const std::string& sessionId);
    std::string getLatestDecisionTraceSummary() const;
    bool loadConversationMemory(
        const std::vector<std::pair<std::string, std::string>>& messages,
        const std::string& summary = "");
    bool loadConversationMemorySync(
        const std::vector<Memory::TimedMessage>& messages,
        const std::string& summary = "");
    /** Legacy API without timestamps (uses current time for each message). */
    bool loadConversationMemorySync(
        const std::vector<std::pair<std::string, std::string>>& messages,
        const std::string& summary = "");
    void setRagFiles(const std::vector<std::string>& filePaths);
    void checkResumablePlan();

    // Executive Control
    void pause();
    void resume();
    void abort();
    void executeGoal(const std::string& goal);

    /** True when THOTH_ENGINE_URL selected RemoteAgentBackend at construction. */
    bool isRemote() const;

    /** Explicit feature surface for capability-driven UI (Phase 2). */
    Thoth::BackendCapabilities capabilities() const;

    /** User-facing mode banner: "Backend: Engine" or "Backend: Local". */
    std::string backendModeLabel() const;

    /** Phase 6 — SSE connection + engine health (Local: applies=false). */
    Thoth::EventStreamSnapshot eventStreamSnapshot() const;

    /**
     * Phase 4 — structured decision summary for Explain Plan (D12/D13).
     * Empty object fields when none available; check capabilities() first.
     */
    nlohmann::json getLatestDecisionSummary() const;

    /** Phase 8 — Engine-owned corpus document list. */
    nlohmann::json listCorpusDocuments() const;

    /** Phase 9 — initiate create corpus document (acceptance OperationResult). */
    void createCorpusDocument(const std::string& sourceFilePath,
                              const Thoth::CorpusCreateGuiOptions& options = {});

    /** ALP-E — synchronous dry_run intent query for Send picker / reconcile. */
    Thoth::OperationResult queryCorpusDocumentIntent(const std::string& sourceFilePath);

    /** ALP amend — unlink session↔document after Local Note X. */
    Thoth::OperationResult unlinkSessionDocument(const std::string& document_id,
                                                 const std::string& session_id);

    /** Phase 10 — Engine conversation authority. */
    nlohmann::json createConversationSession() const;
    void appendConversationTurn(const std::string& sessionId,
                                const std::string& content,
                                const std::string& requestId = "",
                                const std::optional<std::string>& active_goal = std::nullopt);
    nlohmann::json getConversation(const std::string& sessionId) const;
    nlohmann::json getConversationSummary(const std::string& sessionId) const;

    /** R4-G5 — true if a worker task is running or others are queued (turn may wait). */
    bool workerHasContentionBeforeEnqueue();

    // Cognate Memory Access (for UI panels)
    nlohmann::json getStrategies() const;
    nlohmann::json getTrajectories() const;
    nlohmann::json getEpisodes() const;
    nlohmann::json getExperiments() const;
    nlohmann::json getGraphStats() const;

    bool saveExperiment(const nlohmann::json& experimentJson);

    std::function<void(const Thoth::OperationResult&, const std::string& requestId)>
        onOperationComplete;
    std::function<void(const ControllerEvent&)> onEvent;
private:
    void workerLoop();
    void shutdownWorkers();

    std::unique_ptr<IAgentBackend> backend;
    std::mutex workersMutex;
    std::condition_variable workersCv;
    std::queue<std::function<void()>> taskQueue;
    std::thread workerThread;

    std::atomic<bool> shuttingDown{false};
    std::atomic<bool> workerBusy{false};
    std::string activeSessionId;
};
