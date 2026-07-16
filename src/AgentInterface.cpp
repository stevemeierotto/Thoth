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

        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[AgentInterface][Worker] Task exception: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "[AgentInterface][Worker] Unknown task exception.\n";
            }
        }
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
            if (backend) backend->pause();
        });
    }
    workersCv.notify_one();
}

void AgentInterface::resume() {
    if (!backend) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (backend) backend->resume();
        });
    }
    workersCv.notify_one();
}

void AgentInterface::abort() {
    if (!backend) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (backend) backend->abort();
        });
    }
    workersCv.notify_one();
}

void AgentInterface::executeGoal(const std::string& goal) {
    if (!backend) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, goal]() {
            if (backend) backend->executeGoal(goal);
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
            auto reply = backend->processInput(input);
            if (!reply.has_value()) return;
            if (onResponse) onResponse(*reply, resolvedRequestId);
        });
    }
    workersCv.notify_one();
}

std::string AgentInterface::getLatestDecisionTraceSummary() const {
    FileHandler fh;
    std::string path = fh.getAgentWorkspacePath("decision_trace.jsonl");
    if (!std::filesystem::exists(path)) return "No trace available.";

    std::ifstream in(path);
    std::string line, lastLine;
    while (std::getline(in, line)) {
        if (!line.empty()) lastLine = line;
    }

    if (lastLine.empty()) return "Trace empty.";

    try {
        auto j = json::parse(lastLine);
        std::ostringstream out;
        out << "Latest Event: " << j.value("trace_type", "unknown") << "\n";
        if (j.contains("stages") && j["stages"].is_array() && !j["stages"].empty()) {
            auto lastStage = j["stages"].back();
            out << "Status: " << lastStage.value("name", "none")
                << " (" << (lastStage.value("success", false) ? "OK" : "Failed") << ")\n";
            out << "Summary: " << lastStage.value("summary", "none");
        }
        return out.str();
    } catch (...) {
        return "Failed to parse latest trace.";
    }
}

nlohmann::json AgentInterface::getStrategies() const {
    if (!backend) return json::array();
    return backend->getStrategies();
}

nlohmann::json AgentInterface::getTrajectories() const {
    if (!backend) return json::array();
    return backend->getTrajectories();
}

nlohmann::json AgentInterface::getEpisodeSteps() const {
    if (!backend) return json::array();
    return backend->getEpisodeSteps();
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
