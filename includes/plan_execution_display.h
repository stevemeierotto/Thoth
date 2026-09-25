/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan Execution panel display helpers (pure; no wx)
 * Phase A: session-scoped observability goal on chat switch
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_PLAN_EXECUTION_DISPLAY_H
#define THOTH_PLAN_EXECUTION_DISPLAY_H

#include <cstddef>
#include <string>

namespace Thoth {
namespace PlanExecutionDisplay {

/**
 * Session-banner goal may update Observability labels only when no executive
 * plan steps are listed. Callers that switch chats MUST clear steps first so
 * a prior plan cannot pin a stale goal (Phase A).
 */
inline bool canApplySessionGoalDisplay(std::size_t executive_step_count) {
    return executive_step_count == 0;
}

inline std::string formatActiveGoalLabel(const std::string& goal) {
    if (goal.empty()) {
        return "Active Goal: None";
    }
    return std::string("Active Goal: ") + goal;
}

inline constexpr const char* kSessionChatRetrievalState = "State: Session (chat retrieval)";
inline constexpr const char* kIdleState = "State: Idle";

} // namespace PlanExecutionDisplay
} // namespace Thoth

#endif // THOTH_PLAN_EXECUTION_DISPLAY_H
