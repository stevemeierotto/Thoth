/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: LocalAgentBackend implementation
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "local_agent_backend.h"
#include "basic_agent_plugin.h"

#include <algorithm>
#include <iostream>

using json = nlohmann::json;

namespace {

void logBackendError(const char* operation, const std::exception& ex) {
    std::cerr << "[LocalAgentBackend] " << operation << " failed: " << ex.what() << "\n";
}

void logBackendError(const char* operation) {
    std::cerr << "[LocalAgentBackend] " << operation << " failed: unknown error\n";
}

} // namespace

LocalAgentBackend::LocalAgentBackend()
    : plugin_(std::make_unique<BasicAgentPlugin>()) {}

LocalAgentBackend::~LocalAgentBackend() = default;

void LocalAgentBackend::setEventHandler(std::function<void(const ControllerEvent&)> handler) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setEventHandler failed: plugin not initialized\n";
            return;
        }
        plugin_->onEvent = std::move(handler);
    } catch (const std::exception& ex) {
        logBackendError("setEventHandler", ex);
    } catch (...) {
        logBackendError("setEventHandler");
    }
}

void LocalAgentBackend::setSessionId(const std::string& sessionId) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setSessionId failed: plugin not initialized\n";
            return;
        }
        plugin_->setSessionId(sessionId);
    } catch (const std::exception& ex) {
        logBackendError("setSessionId", ex);
    } catch (...) {
        logBackendError("setSessionId");
    }
}

std::optional<std::string> LocalAgentBackend::processInput(const std::string& input) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] processInput failed: plugin not initialized\n";
            return std::nullopt;
        }
        return plugin_->processInput(input);
    } catch (const std::exception& ex) {
        logBackendError("processInput", ex);
        return std::nullopt;
    } catch (...) {
        logBackendError("processInput");
        return std::nullopt;
    }
}

void LocalAgentBackend::executeGoal(const std::string& goal) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] executeGoal failed: plugin not initialized\n";
            return;
        }
        plugin_->executeGoal(goal);
    } catch (const std::exception& ex) {
        logBackendError("executeGoal", ex);
    } catch (...) {
        logBackendError("executeGoal");
    }
}

void LocalAgentBackend::pause() {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] pause failed: plugin not initialized\n";
            return;
        }
        plugin_->pause();
    } catch (const std::exception& ex) {
        logBackendError("pause", ex);
    } catch (...) {
        logBackendError("pause");
    }
}

void LocalAgentBackend::resume() {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] resume failed: plugin not initialized\n";
            return;
        }
        plugin_->resume();
    } catch (const std::exception& ex) {
        logBackendError("resume", ex);
    } catch (...) {
        logBackendError("resume");
    }
}

void LocalAgentBackend::abort() {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] abort failed: plugin not initialized\n";
            return;
        }
        plugin_->abort();
    } catch (const std::exception& ex) {
        logBackendError("abort", ex);
    } catch (...) {
        logBackendError("abort");
    }
}

void LocalAgentBackend::setConversationMemory(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const std::string& summary) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setConversationMemory failed: plugin not initialized\n";
            return;
        }
        plugin_->setConversationMemory(messages, summary);
    } catch (const std::exception& ex) {
        logBackendError("setConversationMemory", ex);
    } catch (...) {
        logBackendError("setConversationMemory");
    }
}

void LocalAgentBackend::setConversationMemory(
    const std::vector<Memory::TimedMessage>& messages,
    const std::string& summary) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setConversationMemory(timed) failed: plugin not initialized\n";
            return;
        }
        plugin_->setConversationMemory(messages, summary);
    } catch (const std::exception& ex) {
        logBackendError("setConversationMemory(timed)", ex);
    } catch (...) {
        logBackendError("setConversationMemory(timed)");
    }
}

void LocalAgentBackend::setRagFiles(const std::vector<std::string>& filePaths) {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] setRagFiles failed: plugin not initialized\n";
            return;
        }
        plugin_->setRagFiles(filePaths);
    } catch (const std::exception& ex) {
        logBackendError("setRagFiles", ex);
    } catch (...) {
        logBackendError("setRagFiles");
    }
}

void LocalAgentBackend::checkResumablePlan() {
    try {
        if (!plugin_) {
            std::cerr << "[LocalAgentBackend] checkResumablePlan failed: plugin not initialized\n";
            return;
        }
        plugin_->checkResumablePlan();
    } catch (const std::exception& ex) {
        logBackendError("checkResumablePlan", ex);
    } catch (...) {
        logBackendError("checkResumablePlan");
    }
}

nlohmann::json LocalAgentBackend::getStrategies() const {
    if (!plugin_) {
        std::cerr << "[LocalAgentBackend] getStrategies failed: plugin not initialized\n";
        return json::array();
    }
    try {
        auto strats = plugin_->getAllStrategies();
        json result = json::array();
        for (const auto& s : strats) {
            json stepPattern = json::array();
            try {
                stepPattern = json::parse(s.step_pattern_json);
            } catch (...) {
            }
            result.push_back({
                {"strategy_id", s.strategy_id},
                {"description", s.description},
                {"step_pattern", stepPattern},
                {"success_rate", s.success_rate},
                {"created_at", s.created_at}
            });
        }
        return result;
    } catch (const std::exception& ex) {
        logBackendError("getStrategies", ex);
        return json::array();
    } catch (...) {
        logBackendError("getStrategies");
        return json::array();
    }
}

nlohmann::json LocalAgentBackend::getTrajectories() const {
    if (!plugin_) {
        std::cerr << "[LocalAgentBackend] getTrajectories failed: plugin not initialized\n";
        return json::array();
    }
    try {
        auto trajs = plugin_->getAllTrajectories();
        std::sort(trajs.begin(), trajs.end(), [](const auto& a, const auto& b) {
            return a.created_at > b.created_at;
        });
        if (trajs.size() > 20) {
            trajs.resize(20);
        }

        json result = json::array();
        for (const auto& t : trajs) {
            json trajectory = json::object();
            try {
                trajectory = json::parse(t.trajectory_json);
            } catch (...) {
            }
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
    } catch (const std::exception& ex) {
        logBackendError("getTrajectories", ex);
        return json::array();
    } catch (...) {
        logBackendError("getTrajectories");
        return json::array();
    }
}

nlohmann::json LocalAgentBackend::getEpisodeSteps() const {
    if (!plugin_) {
        std::cerr << "[LocalAgentBackend] getEpisodeSteps failed: plugin not initialized\n";
        return json::array();
    }
    try {
        auto steps = plugin_->getAllEpisodeSteps();
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
    } catch (const std::exception& ex) {
        logBackendError("getEpisodeSteps", ex);
        return json::array();
    } catch (...) {
        logBackendError("getEpisodeSteps");
        return json::array();
    }
}

nlohmann::json LocalAgentBackend::getExperiments() const {
    if (!plugin_) {
        std::cerr << "[LocalAgentBackend] getExperiments failed: plugin not initialized\n";
        return json::array();
    }
    try {
        auto exprs = plugin_->getAllExperiments();
        json result = json::array();
        for (const auto& e : exprs) {
            json config = json::object();
            json results = json::object();
            try {
                config = json::parse(e.configuration_json);
            } catch (...) {
            }
            try {
                results = json::parse(e.results_json);
            } catch (...) {
            }
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
    } catch (const std::exception& ex) {
        logBackendError("getExperiments", ex);
        return json::array();
    } catch (...) {
        logBackendError("getExperiments");
        return json::array();
    }
}

nlohmann::json LocalAgentBackend::getGraphStats() const {
    if (!plugin_) {
        std::cerr << "[LocalAgentBackend] getGraphStats failed: plugin not initialized\n";
        return json::object();
    }
    try {
        auto stats = plugin_->getGraphStatistics();
        return {
            {"total_nodes", stats.total_nodes},
            {"total_edges", stats.total_edges},
            {"avg_edge_weight", stats.avg_edge_weight},
            {"max_edge_weight", stats.max_edge_weight},
            {"min_edge_weight", stats.min_edge_weight},
            {"total_success_count", stats.total_success_count},
            {"total_failure_count", stats.total_failure_count}
        };
    } catch (const std::exception& ex) {
        logBackendError("getGraphStats", ex);
        return json::object();
    } catch (...) {
        logBackendError("getGraphStats");
        return json::object();
    }
}

bool LocalAgentBackend::saveExperiment(const nlohmann::json& experimentJson) {
    if (!plugin_) {
        std::cerr << "[LocalAgentBackend] saveExperiment failed: plugin not initialized\n";
        return false;
    }
    try {
        Memory::CognateExperimentRecord rec;
        rec.experiment_id = experimentJson.value("experiment_id", "");
        rec.name = experimentJson.value("name", "Unnamed Experiment");
        rec.hypothesis = experimentJson.value("hypothesis", "");
        rec.configuration_json = experimentJson.value("configuration", json::object()).dump();
        rec.results_json = experimentJson.value("results", json::object()).dump();
        rec.created_at = experimentJson.value("created_at", 0LL);
        rec.status = experimentJson.value("status", "pending");
        return plugin_->saveExperiment(rec);
    } catch (const std::exception& ex) {
        logBackendError("saveExperiment", ex);
        return false;
    } catch (...) {
        logBackendError("saveExperiment");
        return false;
    }
}
