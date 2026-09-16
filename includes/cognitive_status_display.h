/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Cognitive status panel labels (pure; no wx)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_COGNITIVE_STATUS_DISPLAY_H
#define THOTH_COGNITIVE_STATUS_DISPLAY_H

#include "controller_event.h"

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace Thoth {
namespace CognitiveStatusDisplay {

/** Start/finish pairs need headroom; Phase 2b will fill the tape live. */
inline constexpr std::size_t kMaxTimelineLines = 24;

/** Phase 1 optimistic line (GUI, before Engine events). */
inline constexpr const char* kOptimisticPlanningStartedLine = "Planning started";

/** COGNITION_STAGE metadata.stage values (Phase 2a contract). */
inline constexpr const char* kStagePlanning = "planning";
inline constexpr const char* kStagePlanReuse = "plan_reuse";
inline constexpr const char* kStageTrajectory = "trajectory";
inline constexpr const char* kStageStrategy = "strategy";
inline constexpr const char* kStageLlmPlan = "llm_plan";

inline constexpr const char* kPhaseStarted = "started";
inline constexpr const char* kPhaseFinished = "finished";

/** Session-scoped: empty event session matches any; otherwise must equal active. */
inline bool mayApplyEvent(const std::string& event_session_id,
                          const std::string& active_session_id) {
    if (event_session_id.empty()) {
        return true;
    }
    if (active_session_id.empty()) {
        return false;
    }
    return event_session_id == active_session_id;
}

inline std::string formatPhaseFromState(const std::string& controller_state_name) {
    if (controller_state_name.empty() || controller_state_name == "IDLE") {
        return "Idle";
    }
    if (controller_state_name == "PLANNING") {
        return "Planning";
    }
    if (controller_state_name == "EXECUTING_STEP") {
        return "Executing step";
    }
    if (controller_state_name == "OBSERVING_RESULT") {
        return "Observing result";
    }
    if (controller_state_name == "REVISING_PLAN") {
        return "Revising plan";
    }
    if (controller_state_name == "SCIENTIFIC_MODE") {
        return "Scientific mode";
    }
    if (controller_state_name == "COMPLETED") {
        return "Completed";
    }
    if (controller_state_name == "ABORTED") {
        return "Aborted";
    }
    if (controller_state_name == "FAILED") {
        return "Failed";
    }
    return controller_state_name;
}

inline std::string stepDisplayName(const std::string& step_id,
                                   const nlohmann::json& metadata) {
    if (metadata.contains("description") && metadata["description"].is_string()) {
        const std::string desc = metadata["description"].get<std::string>();
        if (!desc.empty()) {
            return desc;
        }
    }
    return step_id;
}

/**
 * Format COGNITION_STAGE metadata into an operator-facing line.
 * Contract: metadata.stage + metadata.phase (started|finished).
 */
inline std::optional<std::string> formatCognitionStageLine(const nlohmann::json& metadata) {
    const std::string stage = metadata.value("stage", "");
    const std::string phase = metadata.value("phase", "");
    if (stage.empty() || phase.empty()) {
        return std::nullopt;
    }

    const bool started = (phase == kPhaseStarted);
    const bool finished = (phase == kPhaseFinished);
    if (!started && !finished) {
        return std::nullopt;
    }

    if (stage == kStagePlanning) {
        return std::string(started ? "Planning started" : "Planning finished");
    }
    if (stage == kStagePlanReuse) {
        if (started) {
            return std::string("Plan reuse search started");
        }
        const int count = metadata.value("plan_count", 0);
        if (count <= 0) {
            return std::string("Plan reuse search finished: not found");
        }
        std::ostringstream oss;
        oss << "Plan reuse search finished: found " << count << " similar past plan(s)";
        return oss.str();
    }
    if (stage == kStageTrajectory) {
        if (started) {
            return std::string("Trajectory search started");
        }
        const bool injected = metadata.value("injected", false);
        const int count = metadata.value("trajectory_count", 0);
        if (!injected && count <= 0) {
            return std::string("Trajectory search finished: not found");
        }
        std::ostringstream oss;
        oss << "Trajectory search finished: found " << (count > 0 ? count : 1)
            << " (injecting)";
        return oss.str();
    }
    if (stage == kStageStrategy) {
        if (started) {
            return std::string("Strategy lookup started");
        }
        const bool injected = metadata.value("injected", false);
        const float min_sim = metadata.value("min_similarity", 0.40f);
        if (!injected) {
            std::ostringstream oss;
            oss << "Strategy lookup finished: no match ≥ " << std::fixed;
            oss.precision(2);
            oss << min_sim;
            return oss.str();
        }
        std::ostringstream oss;
        oss << "Strategy lookup finished: matched (sim " << std::fixed;
        oss.precision(2);
        oss << metadata.value("similarity", 0.0f) << " ≥ " << min_sim
            << ") — injecting into prompt";
        return oss.str();
    }
    if (stage == kStageLlmPlan) {
        return std::string(started ? "LLM plan generation started"
                                   : "LLM plan generation finished");
    }

    std::ostringstream oss;
    oss << stage << " " << phase;
    return oss.str();
}

inline std::string formatPhaseForCognitionStage(const nlohmann::json& metadata) {
    const std::string stage = metadata.value("stage", "");
    if (stage == kStagePlanning || stage == kStagePlanReuse || stage == kStageTrajectory
        || stage == kStageStrategy || stage == kStageLlmPlan) {
        return "Planning";
    }
    return "Planning";
}

inline std::optional<std::string> formatDecisionLine(EventType type,
                                                     const nlohmann::json& metadata,
                                                     const std::string& controller_state_name,
                                                     const std::string& step_id) {
    switch (type) {
    case EventType::COGNITION_STAGE:
        return formatCognitionStageLine(metadata);

    case EventType::STATE_CHANGED:
        if (controller_state_name == "PLANNING") {
            // Align with optimistic / COGNITION_STAGE planning started.
            return std::string(kOptimisticPlanningStartedLine);
        }
        if (controller_state_name == "REVISING_PLAN") {
            return std::string("Next: revising plan");
        }
        return std::string("State → ") + formatPhaseFromState(controller_state_name);

    case EventType::PLAN_REUSE_INJECTION: {
        const int count = metadata.value("plan_count", 0);
        if (count <= 0) {
            return std::string("Plan reuse search finished: not found");
        }
        std::ostringstream oss;
        oss << "Plan reuse search finished: found " << count << " similar past plan(s)";
        return oss.str();
    }

    case EventType::TRAJECTORY_INJECTION: {
        const bool injected = metadata.value("injected", false);
        if (!injected && metadata.value("trajectory_count", 0) <= 0) {
            return std::string("Trajectory search finished: not found");
        }
        std::ostringstream oss;
        oss << "Trajectory search finished: found "
            << metadata.value("trajectory_count", 1) << " (injecting)";
        return oss.str();
    }

    case EventType::STRATEGY_INJECTION: {
        const bool injected = metadata.value("injected", false);
        const float sim = metadata.value("similarity", 0.0f);
        const float min_sim = metadata.value("min_similarity", 0.40f);
        if (!injected) {
            std::ostringstream oss;
            oss << "Strategy lookup finished: no match ≥ " << std::fixed;
            oss.precision(2);
            oss << min_sim;
            return oss.str();
        }
        std::ostringstream oss;
        oss << "Strategy lookup finished: matched (sim " << std::fixed;
        oss.precision(2);
        oss << sim << " ≥ " << min_sim << ") — injecting into prompt";
        return oss.str();
    }

    case EventType::PLAN_CREATED: {
        const bool reuse = metadata.value("plan_reuse_in_goal", false)
            || metadata.value("plan_reused", false);
        const bool strategy = metadata.value("strategy_injection", false);
        if (reuse && strategy) {
            return std::string("Plan created — with plan reuse + strategy context");
        }
        if (reuse) {
            return std::string("Plan created — with plan-reuse context");
        }
        if (strategy) {
            return std::string("Plan created — with strategy context");
        }
        return std::string("Plan created — from scratch (or context already logged)");
    }

    case EventType::STEP_STARTED: {
        std::ostringstream oss;
        oss << "Step started";
        const std::string name = stepDisplayName(step_id, metadata);
        if (!name.empty()) {
            oss << ": " << name;
        }
        if (metadata.contains("timeout_ms") && metadata["timeout_ms"].is_number_integer()) {
            oss << " [timeout_ms=" << metadata["timeout_ms"].get<int>() << "]";
        }
        return oss.str();
    }

    case EventType::STEP_COMPLETED: {
        std::ostringstream oss;
        oss << "Step finished";
        const std::string name = stepDisplayName(step_id, metadata);
        if (!name.empty()) {
            oss << ": " << name;
        }
        return oss.str();
    }

    case EventType::STEP_FAILED: {
        std::ostringstream oss;
        oss << "Step failed";
        const std::string name = stepDisplayName(step_id, metadata);
        if (!name.empty()) {
            oss << ": " << name;
        }
        if (metadata.contains("timeout_ms") && metadata["timeout_ms"].is_number_integer()) {
            oss << " [timeout_ms=" << metadata["timeout_ms"].get<int>() << "]";
        }
        const std::string next = metadata.value("next_action", "");
        if (next == "revise") {
            oss << " → REVISING_PLAN";
        } else if (next == "abort") {
            oss << " → FAILED";
        } else if (next == "continue") {
            oss << " → continue";
        }
        if (metadata.contains("error") && metadata["error"].is_string()) {
            const std::string err = metadata["error"].get<std::string>();
            if (!err.empty()) {
                oss << " — " << err;
            }
        }
        return oss.str();
    }

    case EventType::PLAN_REVISED:
        return std::string("Plan revised after step failure");

    case EventType::REFLECTION_REPLAN: {
        std::ostringstream oss;
        oss << "Reflection triggered (score " << std::fixed;
        oss.precision(2);
        oss << metadata.value("trajectory_score", 0.0f) << " < "
            << metadata.value("reflection_threshold", 0.6f) << ") → PLANNING";
        return oss.str();
    }

    case EventType::PLAN_COMPLETED: {
        std::ostringstream oss;
        oss << "Plan finished — trajectory score ";
        if (metadata.contains("trajectory_score")) {
            oss << std::fixed;
            oss.precision(2);
            oss << metadata.value("trajectory_score", 0.0f);
        } else {
            oss << "(n/a)";
        }
        return oss.str();
    }

    case EventType::PLAN_FAILED: {
        std::ostringstream oss;
        oss << "Plan failed";
        if (metadata.contains("trajectory_score")) {
            oss << " — score " << std::fixed;
            oss.precision(2);
            oss << metadata.value("trajectory_score", 0.0f);
        }
        if (metadata.contains("reflection_skip_reason")
            && metadata["reflection_skip_reason"].is_string()
            && !metadata["reflection_skip_reason"].get<std::string>().empty()) {
            oss << " (reflection skipped: "
                << metadata["reflection_skip_reason"].get<std::string>() << ")";
        }
        return oss.str();
    }

    case EventType::PLAN_ABORTED:
        return std::string("Plan aborted");

    case EventType::PLAN_HISTORY_STORED: {
        std::ostringstream oss;
        oss << "Trajectory score stored";
        if (metadata.contains("success_score")) {
            oss << " (" << std::fixed;
            oss.precision(2);
            oss << metadata.value("success_score", 0.0f) << ")";
        }
        return oss.str();
    }

    default:
        return std::nullopt;
    }
}

/** Prefer explicit phase from event type when richer than raw state. */
inline std::string formatPhaseForEvent(EventType type,
                                       const std::string& controller_state_name,
                                       const nlohmann::json& metadata = nlohmann::json::object()) {
    switch (type) {
    case EventType::COGNITION_STAGE:
        return formatPhaseForCognitionStage(metadata);
    case EventType::REFLECTION_REPLAN:
        return "Reflection → Planning";
    case EventType::PLAN_REUSE_INJECTION:
    case EventType::STRATEGY_INJECTION:
    case EventType::TRAJECTORY_INJECTION:
        return "Planning";
    case EventType::PLAN_CREATED:
        return "Plan ready";
    case EventType::STEP_STARTED:
        return "Executing step";
    case EventType::STEP_COMPLETED:
        return "Observing result";
    case EventType::STEP_FAILED:
        return "Step failed";
    case EventType::PLAN_REVISED:
        return "Revising plan";
    case EventType::PLAN_COMPLETED:
        return "Completed";
    case EventType::PLAN_FAILED:
        return "Failed";
    case EventType::PLAN_ABORTED:
        return "Aborted";
    case EventType::STATE_CHANGED:
        if (controller_state_name == "PLANNING") {
            return "Planning";
        }
        return formatPhaseFromState(controller_state_name);
    default:
        return formatPhaseFromState(controller_state_name);
    }
}

} // namespace CognitiveStatusDisplay
} // namespace Thoth

#endif // THOTH_COGNITIVE_STATUS_DISPLAY_H
