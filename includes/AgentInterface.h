#pragma once
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <utility>
#include "basic_agent_plugin.h"  // your plugin wrapper

class AgentInterface {
public:
    AgentInterface();
    ~AgentInterface();

    // Processes user input and returns agent response
    std::string processUserInput(const std::string& input, const std::string& requestId = "");
    void setSessionId(const std::string& sessionId);
    std::string getLatestDecisionTraceSummary() const;
    bool loadConversationMemory(
        const std::vector<std::pair<std::string, std::string>>& messages,
        const std::string& summary = "");
    void setRagFiles(const std::vector<std::string>& filePaths);

    std::function<void(const std::string&, const std::string&)> onResponse;
private:
    void shutdownWorkers();

    std::unique_ptr<BasicAgentPlugin> plugin;
    std::mutex workersMutex;
    std::vector<std::thread> workers;
    std::atomic<bool> shuttingDown{false};
    std::string activeSessionId;
};

