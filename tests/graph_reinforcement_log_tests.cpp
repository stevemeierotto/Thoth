/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — graph reinforcement outcome log.
 * Observes the existing reinforce_plan_graph result. Does not change it.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "config.h"
#include "embedding_engine.h"
#include "executive_controller.h"
#include "index_manager.h"
#include "iplanner.h"
#include "memory.h"
#include "rag.h"
#include "tools.h"

#include <json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(const std::string& name, bool ok) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << name << "\n";
    }
}

class IdlePlanner : public IPlanner {
public:
    Plan create_plan(const std::string&) override { return Plan(); }
    Plan revise_plan(const Plan& plan, const nlohmann::json&) override { return plan; }
};

struct EnvFlag {
    const char* key;
    bool had = false;
    std::string previous;
    explicit EnvFlag(const char* name, const char* value) : key(name) {
        if (const char* current = std::getenv(name)) {
            had = true;
            previous = current;
        }
        setenv(name, value, 1);
    }
    ~EnvFlag() {
        if (had) {
            setenv(key, previous.c_str(), 1);
        } else {
            unsetenv(key);
        }
    }
};

struct Harness {
    fs::path dir;
    Config config;
    std::shared_ptr<Memory> memory;
    std::unique_ptr<EmbeddingEngine> engine;
    std::unique_ptr<IndexManager> index;
    std::shared_ptr<RAGPipeline> rag;
    std::shared_ptr<ToolRegistry> registry = std::make_shared<ToolRegistry>();
    std::shared_ptr<IdlePlanner> planner = std::make_shared<IdlePlanner>();
    std::unique_ptr<Thoth::ExecutiveController> controller;

    Harness(const fs::path& root, const std::string& name, bool with_refiner)
        : dir(root / name) {
        fs::create_directories(dir);
        config.database_path = (dir / "memory.db").string();
        memory = std::make_shared<Memory>(config);
        if (with_refiner) {
            engine = std::make_unique<EmbeddingEngine>(EmbeddingEngine::Method::TfIdf, &config);
            index = std::make_unique<IndexManager>(engine.get());
            rag = std::make_shared<RAGPipeline>(std::move(engine), index.get(), &config, memory.get());
        }
        controller = std::make_unique<Thoth::ExecutiveController>(planner, registry, rag, memory);
    }
};

std::vector<nlohmann::json> reinforcementLogs(const fs::path& workspace, const std::string& plan_id) {
    std::vector<nlohmann::json> found;
    std::ifstream in(workspace / "app_log.jsonl");
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("GRAPH_REINFORCEMENT") == std::string::npos) {
            continue;
        }
        try {
            const auto entry = nlohmann::json::parse(line);
            if (entry.value("event_name", "") != "GRAPH_REINFORCEMENT") {
                continue;
            }
            const auto& metadata = entry.at("metadata");
            if (metadata.value("plan_id", "") == plan_id) {
                found.push_back(metadata);
            }
        } catch (...) {
        }
    }
    return found;
}

} // namespace

namespace Thoth {
struct ExecutiveControllerP0TestAccess {
    static void arm(ExecutiveController& controller, const std::string& plan_id, PlanStatus status) {
        Plan plan;
        plan.plan_id = plan_id;
        plan.goal = "graph reinforcement " + plan_id;
        plan.status = status;
        plan.created_at_ms = 1;
        plan.graph_reinforced = false;
        controller.current_plan_ = std::move(plan);
        controller.state_ = ControllerState::IDLE;
        controller.active_sets_.clear();
    }

    static void pushSet(ExecutiveController& controller, const std::string& step_id, const std::string& hash) {
        ExecutiveController::ActiveStepSet set;
        set.step_id = step_id;
        set.chunk_hashes.push_back(hash);
        controller.active_sets_.push_back(std::move(set));
    }

    static bool reinforce(ExecutiveController& controller) {
        return controller.reinforce_plan_graph();
    }

    static Plan plan(const ExecutiveController& controller) {
        return controller.current_plan_;
    }

    static std::size_t setCount(const ExecutiveController& controller) {
        return controller.active_sets_.size();
    }
};
} // namespace Thoth

namespace {

bool oneOutcome(const std::vector<nlohmann::json>& logs,
                const char* outcome,
                const char* reason,
                int edge_count,
                bool expect_score,
                float score) {
    if (logs.size() != 1) {
        return false;
    }
    const auto& metadata = logs.front();
    if (metadata.value("outcome", "") != outcome || metadata.value("reason", "") != reason
        || metadata.value("edge_count", -1) != edge_count) {
        return false;
    }
    if (!expect_score) {
        return !metadata.contains("success_score");
    }
    return metadata.contains("success_score") && metadata["success_score"].get<float>() == score;
}

void testReward(const fs::path& root) {
    Harness harness(root, "reward", true);
    Thoth::ExecutiveControllerP0TestAccess::arm(
        *harness.controller, "plan-reward", PlanStatus::COMPLETED);
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s1", "hash-reward-from");
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s2", "hash-reward-to");
    const bool committed = Thoth::ExecutiveControllerP0TestAccess::reinforce(*harness.controller);
    const auto edges = harness.memory->getEdgesFrom("hash-reward-from");
    const auto again = Thoth::ExecutiveControllerP0TestAccess::reinforce(*harness.controller);
    const auto logs = reinforcementLogs(root, "plan-reward");
    check("reward commits one strengthened edge",
          committed && again
              && Thoth::ExecutiveControllerP0TestAccess::plan(*harness.controller).graph_reinforced
              && edges.size() == 1 && edges.front().to_id == "hash-reward-to"
              && edges.front().success_count == 1 && edges.front().failure_count == 0
              && edges.front().weight > 0.1f);
    check("reward logs one outcome and a repeat is not a second attempt",
          oneOutcome(logs, "reward", "reward", 1, true, 1.0f));
}

void testPenalty(const fs::path& root) {
    Harness harness(root, "penalty", true);
    Thoth::ExecutiveControllerP0TestAccess::arm(
        *harness.controller, "plan-penalty", PlanStatus::FAILED);
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s1", "hash-penalty-from");
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s2", "hash-penalty-to");
    const bool committed = Thoth::ExecutiveControllerP0TestAccess::reinforce(*harness.controller);
    const auto edges = harness.memory->getEdgesFrom("hash-penalty-from");
    const auto logs = reinforcementLogs(root, "plan-penalty");
    check("penalty commits one weakened edge",
          committed && edges.size() == 1 && edges.front().success_count == 0
              && edges.front().failure_count == 1 && edges.front().weight < 0.1f);
    check("penalty logs one outcome", oneOutcome(logs, "penalty", "penalty", 1, true, 0.0f));
}

void testNoEdge(const fs::path& root) {
    Harness harness(root, "no-edge", true);
    Thoth::ExecutiveControllerP0TestAccess::arm(
        *harness.controller, "plan-no-edge", PlanStatus::COMPLETED);
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "only", "hash-single");
    const bool committed = Thoth::ExecutiveControllerP0TestAccess::reinforce(*harness.controller);
    const auto logs = reinforcementLogs(root, "plan-no-edge");
    check("one retrieval set commits no edge",
          committed
              && Thoth::ExecutiveControllerP0TestAccess::plan(*harness.controller).graph_reinforced
              && Thoth::ExecutiveControllerP0TestAccess::setCount(*harness.controller) == 0
              && harness.memory->getEdgesFrom("hash-single").empty());
    check("one retrieval set logs no_edge",
          oneOutcome(logs, "no_edge", "no_eligible_edges", 0, false, 0.0f));
}

void testMissingRefiner(const fs::path& root) {
    Harness harness(root, "missing-refiner", false);
    Thoth::ExecutiveControllerP0TestAccess::arm(
        *harness.controller, "plan-missing-refiner", PlanStatus::COMPLETED);
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s1", "hash-missing-from");
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s2", "hash-missing-to");
    const bool committed = Thoth::ExecutiveControllerP0TestAccess::reinforce(*harness.controller);
    const auto logs = reinforcementLogs(root, "plan-missing-refiner");
    check("missing refiner still commits without edges",
          committed
              && Thoth::ExecutiveControllerP0TestAccess::plan(*harness.controller).graph_reinforced
              && harness.memory->getEdgesFrom("hash-missing-from").empty());
    check("missing refiner is indeterminate",
          oneOutcome(logs, "indeterminate", "missing_refiner", 0, false, 0.0f));
}

void testFailedCommit(const fs::path& root) {
    Harness harness(root, "failed-commit", true);
    Thoth::ExecutiveControllerP0TestAccess::arm(
        *harness.controller, "plan-failed-commit", PlanStatus::COMPLETED);
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s1", "hash-fail-commit-from");
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s2", "hash-fail-commit-to");
    bool committed = true;
    {
        EnvFlag inject("THOTH_INJECT_GRAPH_COMMIT_FAIL", "1");
        committed = Thoth::ExecutiveControllerP0TestAccess::reinforce(*harness.controller);
    }
    const auto logs = reinforcementLogs(root, "plan-failed-commit");
    check("failed commit leaves the graph and marker unchanged",
          !committed
              && !Thoth::ExecutiveControllerP0TestAccess::plan(*harness.controller).graph_reinforced
              && harness.memory->getEdgesFrom("hash-fail-commit-from").empty());
    check("failed commit is indeterminate",
          oneOutcome(logs, "indeterminate", "failed_commit", 1, true, 1.0f));
}

void testUnavailable(const fs::path& root) {
    Harness harness(root, "unavailable", true);
    Thoth::ExecutiveControllerP0TestAccess::arm(*harness.controller, "", PlanStatus::COMPLETED);
    Thoth::ExecutiveControllerP0TestAccess::pushSet(*harness.controller, "s1", "hash-unavailable");
    const bool committed = Thoth::ExecutiveControllerP0TestAccess::reinforce(*harness.controller);
    const auto logs = reinforcementLogs(root, "");
    check("empty plan is not reinforced",
          !committed && Thoth::ExecutiveControllerP0TestAccess::setCount(*harness.controller) == 0
              && harness.memory->getEdgesFrom("hash-unavailable").empty());
    check("empty plan is indeterminate",
          oneOutcome(logs, "indeterminate", "unavailable", 0, false, 0.0f));
}

} // namespace

int main() {
    char directoryTemplate[] = "/tmp/thoth-graph-log-XXXXXX";
    char* created = mkdtemp(directoryTemplate);
    if (!created) {
        std::cerr << "could not create the log workspace\n";
        return 1;
    }
    const fs::path root = created;
    setenv("THOTH_WORKSPACE_PATH", root.string().c_str(), 1);
    setenv("THOTH_LOGS_PATH", (root / "logs").string().c_str(), 1);
    unsetenv("THOTH_INJECT_GRAPH_COMMIT_FAIL");

    testReward(root);
    testPenalty(root);
    testNoEdge(root);
    testMissingRefiner(root);
    testFailedCommit(root);
    testUnavailable(root);

    if (failures != 0) {
        std::cerr << failures << " graph reinforcement log checks failed\n";
        return 1;
    }
    std::cout << "graph reinforcement log checks passed\n";
    return 0;
}
