#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include "AgentInterface.h"
#include "local_agent_backend.h"
#include "remote_agent_backend.h"
#include "remote_agent_http_utils.h"
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "file_handler.h"
#include "logger.h"
#include "cognitive_diagnostics_authority.h"
#include "decision_summary.h"
#include "chat_send_trace.h"
#include <json.hpp>

using json = nlohmann::json;

AgentInterface::AgentInterface() {
    try {
        // K4: only AgentInterface reads THOTH_ENGINE_URL. Backends never read env.
        const auto remote_url = ThothRemoteHttp::resolveThothEngineUrlFromEnv();
        if (remote_url.has_value()) {
            backend = std::make_unique<RemoteAgentBackend>(*remote_url);
            std::cerr << "[AgentInterface] backend=remote url=" << *remote_url << "\n";
        } else {
            backend = std::make_unique<LocalAgentBackend>();
            std::cerr << "[AgentInterface] backend=local\n";
        }

        // AgentInterface owns the event bridge; backend invokes the callback only.
        backend->setEventHandler([this](const ControllerEvent& ev) {
            if (ev.type == EventType::PLAN_REUSE_INJECTION
                || ev.type == EventType::REFLECTION_REPLAN
                || ev.type == EventType::PLAN_HISTORY_STORED) {
                std::cerr << "[AgentInterface] bridge event session=" << ev.session_id
                          << " plan=" << ev.plan_id
                          << " type=" << static_cast<int>(ev.type) << "\n";
            }
            if (this->onEvent) this->onEvent(ev);
        });

        // Start worker thread
        workerThread = std::thread(&AgentInterface::workerLoop, this);

    } catch (const std::exception& ex) {
        std::cerr << "[AgentInterface][Error] Failed to initialize agent: "
                  << ex.what() << "\n";
    } catch (...) {
        std::cerr << "[AgentInterface][Error] Unknown error during agent init.\n";
    }
}

AgentInterface::~AgentInterface() {
    shutdownWorkers();
}

void AgentInterface::workerLoop() {
    while (!shuttingDown) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(workersMutex);
            workersCv.wait(lock, [this] { return shuttingDown || !taskQueue.empty(); });

            if (shuttingDown && taskQueue.empty()) return;

            task = std::move(taskQueue.front());
            taskQueue.pop();
        }

        workerBusy.store(true);
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[AgentInterface][Worker] Task exception: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "[AgentInterface][Worker] Unknown task exception.\n";
            }
        }
        workerBusy.store(false);
    }
}

void AgentInterface::shutdownWorkers() {
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        shuttingDown = true;
    }
    workersCv.notify_all();
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void AgentInterface::setSessionId(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(workersMutex);
    activeSessionId = sessionId;

    taskQueue.push([this, sessionId]() {
        if (backend) backend->setSessionId(sessionId);
    });
    workersCv.notify_one();
}

bool AgentInterface::isRemote() const {
    return backend && backend->isRemote();
}

Thoth::BackendCapabilities AgentInterface::capabilities() const {
    if (!backend) {
        return Thoth::BackendCapabilities{};
    }
    return backend->capabilities();
}

std::string AgentInterface::backendModeLabel() const {
    return Thoth::backendModeLabel(isRemote());
}

Thoth::EventStreamSnapshot AgentInterface::eventStreamSnapshot() const {
    if (!backend) {
        return Thoth::localEventStreamSnapshot(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }
    return backend->eventStreamSnapshot();
}

nlohmann::json AgentInterface::getLatestDecisionSummary() const {
    if (!backend) {
        return Thoth::DecisionSummary::emptyV1Summary();
    }
    if (!capabilities().supportsPlanDiagnostics) {
        return Thoth::DecisionSummary::emptyV1Summary();
    }
    return backend->getLatestDecisionSummary();
}

nlohmann::json AgentInterface::listCorpusDocuments() const {
    if (!backend) {
        return Thoth::CorpusDocuments::emptyV1List();
    }
    if (!capabilities().supportsCorpusList) {
        return Thoth::CorpusDocuments::emptyV1List();
    }
    return backend->listCorpusDocuments();
}

void AgentInterface::createCorpusDocument(const std::string& sourceFilePath,
                                          const Thoth::CorpusCreateGuiOptions& options) {
    if (!backend) {
        return;
    }
    std::string sessionId;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        sessionId = activeSessionId;
        taskQueue.push([this, sourceFilePath, sessionId, options]() {
            if (!backend) {
                return;
            }
            backend->setSessionId(sessionId);
            const auto result = backend->createCorpusDocument(sourceFilePath, options);
            if (onOperationComplete) {
                onOperationComplete(result, {});
            }
        });
    }
    workersCv.notify_one();
}

Thoth::OperationResult AgentInterface::queryCorpusDocumentIntent(
    const std::string& sourceFilePath) {
    if (!backend) {
        return Thoth::makeFailure(Thoth::kOpQueryDocumentIntent,
                                  "Could not query send intent",
                                  "backend not initialized");
    }
    if (!capabilities().supportsIngest) {
        return Thoth::makeFailure(Thoth::kOpQueryDocumentIntent,
                                  "Could not query send intent",
                                  "ingest unavailable");
    }
    std::string sessionId;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        sessionId = activeSessionId;
    }
    if (!sessionId.empty()) {
        backend->setSessionId(sessionId);
    }
    return backend->queryCorpusDocumentIntent(sourceFilePath);
}

Thoth::OperationResult AgentInterface::unlinkSessionDocument(
    const std::string& document_id,
    const std::string& session_id) {
    if (!backend) {
        return Thoth::makeFailure(Thoth::kOpUnlinkSessionDocument,
                                  "Could not unlink document from chat",
                                  "backend unavailable");
    }
    return backend->unlinkSessionDocument(document_id, session_id);
}

nlohmann::json AgentInterface::createConversationSession() const {
    if (!backend) {
        return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
    }
    if (!capabilities().supportsConversation) {
        return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
    }
    return backend->createConversationSession();
}

void AgentInterface::appendConversationTurn(const std::string& sessionId,
                                            const std::string& content,
                                            const std::string& requestId,
                                            const std::optional<std::string>& active_goal) {
    Thoth::ChatSendTrace::logPayload("payload_b9_AgentInterface_entry_content", content);
    Thoth::ChatSendTrace::log(
        11, "appendConversationTurn_entry",
        "request=" + requestId + " session=" + sessionId
            + " content_len=" + std::to_string(content.size()));
    if (!backend) {
        Thoth::ChatSendTrace::log(
            11, "http_dispatch",
            "NOT_DISPATCHED reason=backend_null request=" + requestId
                + " session=" + sessionId);
        return;
    }
    std::string resolvedRequestId = requestId;
    if (resolvedRequestId.empty()) {
        resolvedRequestId = "req-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        const size_t qsize = taskQueue.size();
        // Lambda captures `content` by value (copy of the by-ref parameter at enqueue time).
        Thoth::ChatSendTrace::log(
            11, "http_dispatch_enqueued",
            "request=" + resolvedRequestId + " session=" + sessionId
                + " queue_size_before_push=" + std::to_string(qsize)
                + " worker_busy=" + std::string(workerBusy.load() ? "yes" : "no")
                + " captured_content_len=" + std::to_string(content.size()));
        taskQueue.push([this, sessionId, content, active_goal, resolvedRequestId]() {
            Thoth::ChatSendTrace::logPayload(
                "payload_b10_worker_captured_content", content);
            Thoth::ChatSendTrace::log(
                11, "http_dispatch_worker_start",
                "request=" + resolvedRequestId + " session=" + sessionId
                    + " content_len=" + std::to_string(content.size()));
            if (!backend) {
                Thoth::ChatSendTrace::log(
                    11, "http_dispatch",
                    "NOT_DISPATCHED reason=backend_null_in_worker request="
                        + resolvedRequestId);
                if (onOperationComplete) {
                    onOperationComplete(
                        Thoth::makeFailure(Thoth::kOpChat,
                                           "Failed to send",
                                           "backend unavailable"),
                        resolvedRequestId);
                }
                return;
            }
            const auto result = backend->appendConversationTurn(sessionId, content, active_goal);
            Thoth::ChatSendTrace::log(
                12, "engine_response_at_worker",
                "request=" + resolvedRequestId + " success="
                    + std::string(result.success ? "yes" : "no")
                    + " detail=" + result.technical_details);
            if (onOperationComplete) {
                onOperationComplete(result, resolvedRequestId);
            } else {
                Thoth::ChatSendTrace::log(
                    13, "completion_path",
                    "GUI_CLEANUP_MISSING reason=onOperationComplete_null request="
                        + resolvedRequestId);
            }
        });
    }
    workersCv.notify_one();
}

bool AgentInterface::workerHasContentionBeforeEnqueue() {
    std::lock_guard<std::mutex> lock(workersMutex);
    return workerBusy.load() || !taskQueue.empty();
}

nlohmann::json AgentInterface::getConversation(const std::string& sessionId) const {
    if (!backend || !capabilities().supportsConversation) {
        return Thoth::ConversationAuthority::emptyConversation(sessionId);
    }
    return backend->getConversation(sessionId);
}

nlohmann::json AgentInterface::getConversationSummary(const std::string& sessionId) const {
    if (!backend || !capabilities().supportsConversation) {
        return Thoth::ConversationAuthority::makeSummaryResponse(sessionId, "");
    }
    return backend->getConversationSummary(sessionId);
}

std::string AgentInterface::getLatestDecisionTraceSummary() const {
    // Phase 4: format Engine-authored structured summary (no host-file inventing in GUI).
    if (!capabilities().supportsPlanDiagnostics) {
        return Thoth::CognitiveDiagnostics::decisionTraceUnavailableSentinel();
    }
    const auto summary = getLatestDecisionSummary();
    if (Thoth::DecisionSummary::isEffectivelyEmpty(summary)) {
        return "No decision summary available.";
    }
    return Thoth::DecisionSummary::formatForDisplay(summary);
}

nlohmann::json AgentInterface::getStrategies() const {
    if (!backend) return json::array();
    return backend->getStrategies();
}

bool AgentInterface::loadConversationMemory(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const std::string& summary) {
    if (!backend) return false;
    // Plan K: no conversation-sync HTTP — do not claim success in remote mode.
    if (backend->isRemote()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, messages, summary]() {
            if (backend) backend->setConversationMemory(messages, summary);
        });
    }
    workersCv.notify_one();
    return true;
}

bool AgentInterface::loadConversationMemorySync(
    const std::vector<Memory::TimedMessage>& messages,
    const std::string& summary) {
    if (!backend) {
        return false;
    }
    // Plan K: no conversation-sync HTTP — do not claim success in remote mode.
    if (backend->isRemote()) {
        return false;
    }

    auto done = std::make_shared<std::promise<void>>();
    std::future<void> finished = done->get_future();
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, messages, summary, done]() {
            if (backend) {
                backend->setConversationMemory(messages, summary);
            }
            done->set_value();
        });
    }
    workersCv.notify_one();
    finished.wait();
    return true;
}

bool AgentInterface::loadConversationMemorySync(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const std::string& summary) {
    std::vector<Memory::TimedMessage> timed;
    timed.reserve(messages.size());
    for (const auto& msg : messages) {
        timed.push_back({msg.first, msg.second, 0});
    }
    return loadConversationMemorySync(timed, summary);
}

void AgentInterface::setRagFiles(const std::vector<std::string>& filePaths) {
    if (!backend) return;
    if (isRemote()) {
        std::cerr << "[AgentInterface] setRagFiles ignored (remote backend; use Send to Engine)\n";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, filePaths]() {
            if (backend) backend->setRagFiles(filePaths);
        });
    }
    workersCv.notify_one();
}

void AgentInterface::checkResumablePlan() {
    if (!backend) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (backend) backend->checkResumablePlan();
        });
    }
    workersCv.notify_one();
}

void AgentInterface::pause() {
    if (!backend) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (!backend) return;
            const auto result = backend->pause();
            if (onOperationComplete) {
                onOperationComplete(result, {});
            }
        });
    }
    workersCv.notify_one();
}

void AgentInterface::resume() {
    if (!backend) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (!backend) return;
            const auto result = backend->resume();
            if (onOperationComplete) {
                onOperationComplete(result, {});
            }
        });
    }
    workersCv.notify_one();
}

void AgentInterface::abort() {
    if (!backend) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (!backend) return;
            const auto result = backend->abort();
            if (onOperationComplete) {
                onOperationComplete(result, {});
            }
        });
    }
    workersCv.notify_one();
}

void AgentInterface::executeGoal(const std::string& goal) {
    if (!backend) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, goal]() {
            if (!backend) return;
            const auto result = backend->executeGoal(goal);
            if (onOperationComplete) {
                onOperationComplete(result, {});
            }
        });
    }
    workersCv.notify_one();
}

void AgentInterface::processUserInput(const std::string& input, const std::string& requestId) {
    if (!backend) return;

    std::string resolvedRequestId = requestId;
    if (resolvedRequestId.empty()) {
        resolvedRequestId = "req-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, input, resolvedRequestId]() {
            if (!backend) return;
            const auto result = backend->processInput(input);
            if (onOperationComplete) {
                onOperationComplete(result, resolvedRequestId);
            }
        });
    }
    workersCv.notify_one();
}

nlohmann::json AgentInterface::getTrajectories() const {
    if (!backend) return json::array();
    return backend->getTrajectories();
}

nlohmann::json AgentInterface::getEpisodes() const {
    if (!backend) return json::object();
    return backend->getEpisodes();
}

nlohmann::json AgentInterface::getExperiments() const {
    if (!backend) return json::array();
    return backend->getExperiments();
}

bool AgentInterface::saveExperiment(const nlohmann::json& experimentJson) {
    if (!backend) return false;
    return backend->saveExperiment(experimentJson);
}

nlohmann::json AgentInterface::getGraphStats() const {
    if (!backend) return json::object();
    return backend->getGraphStats();
}

wxString AgentInterface::GetBenchmarkBinaryPath(const wxString& binaryName) {
    // 1. Get the directory of the currently running executable
    wxString exePath = wxStandardPaths::Get().GetExecutablePath();
    wxFileName exeFileName(exePath);
    wxString exeDir = exeFileName.GetPath();

    std::cerr << "[AgentInterface] exeDir: " << exeDir.ToStdString() << "\n";

    // 2. Check the same directory as the executable
    wxFileName target(exeDir, binaryName);
    if (target.FileExists() && target.IsFileExecutable()) {
        std::cerr << "[AgentInterface] Found binary in exeDir: " << target.GetFullPath().ToStdString() << "\n";
        return target.GetFullPath();
    }

    // 3. Search upwards for the project root and then look into known build structures
    wxString current = exeDir;
    while (!current.IsEmpty()) {
        // Option A: current contains agent_workspace (the project root)
        wxFileName rootCheck(current, "");
        rootCheck.AppendDir("agent_workspace");

        // Option B: current IS "build"
        wxFileName buildCheck(current, "");
        bool isBuildDir = (buildCheck.GetDirs().Last() == "build");

        if (rootCheck.DirExists() || isBuildDir) {
            wxString buildRoot = isBuildDir ? current : (current + "/build");
            std::cerr << "[AgentInterface] Searching build root: " << buildRoot.ToStdString() << "\n";

            wxArrayString subPaths;
            subPaths.Add("debug/external/basic_agent");
            subPaths.Add("release/external/basic_agent");
            subPaths.Add("external/basic_agent");

            for (const auto& sub : subPaths) {
                wxFileName candidate(buildRoot, "");
                candidate.AppendDir(sub);
                wxFileName finalPath(candidate.GetPath(), binaryName);

                std::cerr << "[AgentInterface] Checking: " << finalPath.GetFullPath().ToStdString() << "\n";
                if (finalPath.FileExists() && finalPath.IsFileExecutable()) {
                    std::cerr << "[AgentInterface] Found binary: " << finalPath.GetFullPath().ToStdString() << "\n";
                    return finalPath.GetFullPath();
                }
            }
        }

        // Go up one level
        wxString parent = wxFileName(current, "").GetPath();
        if (parent == current || parent.IsEmpty()) break;
        current = parent;
    }

    // 4. Fallback: Try hardcoded paths relative to project root (if we can find it)
    // Assume we are in /home/steve/Thoth (project root)
    {
        wxArrayString hardcoded;
        hardcoded.Add("/home/steve/Thoth/build/debug/external/basic_agent");
        hardcoded.Add("/home/steve/Thoth/build/external/basic_agent");

        for (const auto& p : hardcoded) {
            wxFileName finalPath(p, binaryName);
            if (finalPath.FileExists() && finalPath.IsFileExecutable()) {
                std::cerr << "[AgentInterface] Found via hardcoded path: " << finalPath.GetFullPath().ToStdString() << "\n";
                return finalPath.GetFullPath();
            }
        }
    }

    // Final Fallback: Return just the name and hope it's in the PATH
    std::cerr << "[AgentInterface] WARNING: Binary not found. Falling back to PATH: " << binaryName.ToStdString() << "\n";
    return binaryName;
}
