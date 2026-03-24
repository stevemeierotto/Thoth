#pragma once
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <utility>
#include <condition_variable>
#include <queue>
#include "basic_agent_plugin.h"

class AgentInterface {
public:
    AgentInterface();
    ~AgentInterface();

    // Processes user input asynchronously
    void processUserInput(const std::string& input, const std::string& requestId = "");
    void setSessionId(const std::string& sessionId);
    std::string getLatestDecisionTraceSummary() const;
    bool loadConversationMemory(
        const std::vector<std::pair<std::string, std::string>>& messages,
        const std::string& summary = "");
    void setRagFiles(const std::vector<std::string>& filePaths);
    void checkResumablePlan();

    // Executive Control
    void pause();
    void resume();
    void abort();
    void executeGoal(const std::string& goal);

    // Cognate Memory Access (for UI panels)
    nlohmann::json getStrategies() const;
    nlohmann::json getTrajectories() const;
    nlohmann::json getExperiments() const;
    nlohmann::json getGraphStats() const;

    bool saveExperiment(const nlohmann::json& experimentJson);

    std::function<void(const std::string&, const std::string&)> onResponse;
    std::function<void(const ControllerEvent&)> onEvent;
private:
    void workerLoop();
    void shutdownWorkers();

    std::unique_ptr<BasicAgentPlugin> plugin;
    std::mutex workersMutex;
    std::condition_variable workersCv;
    std::queue<std::function<void()>> taskQueue;
    std::thread workerThread;
    
    std::atomic<bool> shuttingDown{false};
    std::string activeSessionId;
};

