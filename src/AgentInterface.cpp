#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include "AgentInterface.h"
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
        plugin = std::make_unique<BasicAgentPlugin>();
        
        plugin->onEvent = [this](const ControllerEvent& ev) {
            if (ev.type == EventType::PLAN_REUSE_INJECTION
                || ev.type == EventType::REFLECTION_REPLAN
                || ev.type == EventType::PLAN_HISTORY_STORED) {
                std::cerr << "[AgentInterface] bridge event session=" << ev.session_id
                          << " plan=" << ev.plan_id
                          << " type=" << static_cast<int>(ev.type) << "\n";
            }
            if (this->onEvent) this->onEvent(ev);
        };

        std::cerr << "[AgentInterface] BasicAgentPlugin initialized successfully.\n";
        
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
        if (plugin) plugin->setSessionId(sessionId);
    });
    workersCv.notify_one();
}

bool AgentInterface::loadConversationMemory(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const std::string& summary) {
    if (!plugin) return false;

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, messages, summary]() {
            if (plugin) plugin->setConversationMemory(messages, summary);
        });
    }
    workersCv.notify_one();
    return true;
}

void AgentInterface::setRagFiles(const std::vector<std::string>& filePaths) {
    if (!plugin) return;

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, filePaths]() {
            if (plugin) plugin->setRagFiles(filePaths);
        });
    }
    workersCv.notify_one();
}

void AgentInterface::checkResumablePlan() {
    if (!plugin) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (plugin) plugin->checkResumablePlan();
        });
    }
    workersCv.notify_one();
}

void AgentInterface::pause() {
    if (!plugin) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (plugin) plugin->pause();
        });
    }
    workersCv.notify_one();
}

void AgentInterface::resume() {
    if (!plugin) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (plugin) plugin->resume();
        });
    }
    workersCv.notify_one();
}

void AgentInterface::abort() {
    if (!plugin) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this]() {
            if (plugin) plugin->abort();
        });
    }
    workersCv.notify_one();
}

void AgentInterface::executeGoal(const std::string& goal) {
    if (!plugin) return;
    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, goal]() {
            if (plugin) plugin->executeGoal(goal);
        });
    }
    workersCv.notify_one();
}

void AgentInterface::processUserInput(const std::string& input, const std::string& requestId) {
    if (!plugin) return;
    
    std::string resolvedRequestId = requestId;
    if (resolvedRequestId.empty()) {
        resolvedRequestId = "req-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        taskQueue.push([this, input, resolvedRequestId]() {
            if (plugin) {
                std::string reply = plugin->processInput(input);
                if (onResponse) onResponse(reply, resolvedRequestId);
            }
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
    if (!plugin) return json::array();
    try {
        auto strats = plugin->getAllStrategies();
        json result = json::array();
        for (const auto& s : strats) {
            json stepPattern = json::array();
            try { stepPattern = json::parse(s.step_pattern_json); } catch(...) {}
            result.push_back({
                {"strategy_id", s.strategy_id},
                {"description", s.description},
                {"step_pattern", stepPattern},
                {"success_rate", s.success_rate},
                {"created_at", s.created_at}
            });
        }
        return result;
    } catch (...) { return json::array(); }
}

nlohmann::json AgentInterface::getTrajectories() const {
    if (!plugin) return json::array();
    try {
        auto trajs = plugin->getAllTrajectories();
        std::sort(trajs.begin(), trajs.end(), [](const auto& a, const auto& b) {
            return a.created_at > b.created_at;
        });
        if (trajs.size() > 20) trajs.resize(20);

        json result = json::array();
        for (const auto& t : trajs) {
            json trajectory = json::object();
            try { trajectory = json::parse(t.trajectory_json); } catch(...) {}
            result.push_back({
                {"trajectory_id", t.trajectory_id},
                {"goal", t.goal},
                {"trajectory", trajectory},
                {"success_score", t.success_score},
                {"created_at", t.created_at},
                {"usage_count", t.usage_count},
                {"tier", t.tier}
            });
        }
        return result;
    } catch (...) { return json::array(); }
}

nlohmann::json AgentInterface::getEpisodeSteps() const {
    if (!plugin) return json::array();
    try {
        auto steps = plugin->getAllEpisodeSteps();
        json result = json::array();
        for (const auto& s : steps) {
            result.push_back({
                {"episode_id", s.episode_id},
                {"goal_id", s.goal_id},
                {"step_index", s.step_index},
                {"state_summary", s.state_summary},
                {"action_taken", s.action_taken},
                {"result_status", s.result_status},
                {"timestamp_ms", s.timestamp_ms}
            });
        }
        return result;
    } catch (...) { return json::array(); }
}

nlohmann::json AgentInterface::getExperiments() const {
    if (!plugin) return json::array();
    try {
        auto exprs = plugin->getAllExperiments();
        json result = json::array();
        for (const auto& e : exprs) {
            json config = json::object();
            json results = json::object();
            try { config = json::parse(e.configuration_json); } catch(...) {}
            try { results = json::parse(e.results_json); } catch(...) {}
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
    } catch (...) { return json::array(); }
}

bool AgentInterface::saveExperiment(const nlohmann::json& experimentJson) {
    if (!plugin) return false;
    try {
        Memory::CognateExperimentRecord rec;
        rec.experiment_id = experimentJson.value("experiment_id", "");
        rec.name = experimentJson.value("name", "Unnamed Experiment");
        rec.hypothesis = experimentJson.value("hypothesis", "");
        rec.configuration_json = experimentJson.value("configuration", json::object()).dump();
        rec.results_json = experimentJson.value("results", json::object()).dump();
        rec.created_at = experimentJson.value("created_at", 0LL);
        rec.status = experimentJson.value("status", "pending");
        return plugin->saveExperiment(rec);
    } catch (...) { return false; }
}

nlohmann::json AgentInterface::getGraphStats() const {
    if (!plugin) return json::object();
    try {
        auto stats = plugin->getGraphStatistics();
        return {
            {"total_nodes", stats.total_nodes},
            {"total_edges", stats.total_edges},
            {"avg_edge_weight", stats.avg_edge_weight},
            {"max_edge_weight", stats.max_edge_weight},
            {"min_edge_weight", stats.min_edge_weight},
            {"total_success_count", stats.total_success_count},
            {"total_failure_count", stats.total_failure_count}
        };
    } catch (...) { return json::object(); }
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
