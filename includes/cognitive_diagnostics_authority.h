/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — GUI Phase 3 D11 cognitive diagnostics authority (pure; no wx)
 *
 * Normative: The GUI may only present cognitive information obtained from
 * the active backend. Host artifacts are never a substitute when Engine
 * is authoritative.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_COGNITIVE_DIAGNOSTICS_AUTHORITY_H
#define THOTH_COGNITIVE_DIAGNOSTICS_AUTHORITY_H

#include "backend_capabilities.h"

#include <string>

namespace Thoth {
namespace CognitiveDiagnostics {

/**
 * D11 — may the GUI present host-local cognitive artifact bytes as diagnostics?
 * True only when the active backend owns those artifacts (Local).
 */
inline bool mayPresentHostCognitiveArtifact(const BackendCapabilities& caps) {
    // Plan diagnostics and logs share the host decision_trace / log family today.
    return caps.supportsPlanDiagnostics || caps.supportsLogs;
}

inline bool mayReadHostDecisionTrace(const BackendCapabilities& caps) {
    return caps.supportsPlanDiagnostics;
}

inline bool mayReadHostLogs(const BackendCapabilities& caps) {
    return caps.supportsLogs;
}

/** Locked Explain Plan dialog title (Phase 3). */
inline constexpr const char* kExplainPlanDialogTitle = "Explain Plan";

/** Locked one-line why — no roadmap codenames. */
inline constexpr const char* kPlanDiagnosticsNotExposedWhy =
    "The current backend does not expose plan diagnostics.";

inline constexpr const char* kLogsNotExposedWhy =
    "The current backend does not expose logs.";

/**
 * Locked Explain Plan body when capability missing:
 *   Unavailable
 *
 *   The current backend does not expose plan diagnostics.
 */
inline std::string formatExplainPlanUnavailableBody() {
    return std::string("Unavailable\n\n") + kPlanDiagnosticsNotExposedWhy;
}

/** Sentinel returned by getLatestDecisionTraceSummary when gated (no host bytes). */
inline std::string decisionTraceUnavailableSentinel() {
    return formatExplainPlanUnavailableBody();
}

/** True if text looks like the locked unavailable body (unit tests / guards). */
inline bool isDecisionTraceUnavailableSentinel(const std::string& text) {
    return text.find("Unavailable") != std::string::npos
        && text.find(kPlanDiagnosticsNotExposedWhy) != std::string::npos
        && text.find("Plan K") == std::string::npos;
}

/**
 * Phase 3 audit checklist (document outcomes in completion log):
 * - decision_trace.jsonl / Explain Plan — GATE
 * - Logs tab / decision_trace tail — GATE (Phase 2 supportsLogs; Phase 3 why-copy)
 * - app_log — no GUI cognitive view found presenting it as Engine truth
 * - Executive state snapshots — SSE/event strip; not host-file fallback
 * - Planner outputs — PlanExecutionPanel from events; not host file
 * - Retrieval traces / GRAG diagnostics — event metadata; no host-file fallback
 * - Benchmark summaries — supportsBenchmarks (Phase 2)
 * - Cached explanations — no separate cache path found
 * - chat_sessions.json — UI chrome only (not Engine memory claim)
 */

} // namespace CognitiveDiagnostics
} // namespace Thoth

#endif // THOTH_COGNITIVE_DIAGNOSTICS_AUTHORITY_H
