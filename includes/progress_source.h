/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — GUI Phase 5 progress provenance (pure; no wx)
 *
 * D3a: Progress that implies backend work may only originate from
 * authoritative backend signals — never from UserAction / Unknown.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_PROGRESS_SOURCE_H
#define THOTH_PROGRESS_SOURCE_H

#include <string>

namespace Thoth {

/** Internal provenance for work-implying UI mutations (not user-visible). */
enum class ProgressSource {
    EngineEvent,         // SSE / ControllerEvent from Engine
    LocalBackendEvent,   // ControllerEvent from Local plugin / IndexManager
    EngineApiResponse,   // Explicit Engine HTTP / API outcome
    UserAction,          // Suspicious if used to claim Engine/Local work
    Unknown
};

/**
 * May this source drive UI that claims backend work is in progress
 * (indexing counters, Planning…, Syncing…, activity strip work text)?
 */
inline bool mayApplyWorkProgress(ProgressSource source) {
    switch (source) {
        case ProgressSource::EngineEvent:
        case ProgressSource::LocalBackendEvent:
        case ProgressSource::EngineApiResponse:
            return true;
        case ProgressSource::UserAction:
        case ProgressSource::Unknown:
            return false;
    }
    return false;
}

/** Indexing progress specifically — events only (never drop/import UserAction). */
inline bool mayApplyIndexingProgress(ProgressSource source) {
    return source == ProgressSource::EngineEvent
        || source == ProgressSource::LocalBackendEvent;
}

/** Chrome status after Local host RAG add — does not claim indexing (events will). */
inline std::string formatFilesAddedChromeStatus(int files_added) {
    return "Added " + std::to_string(files_added) + " file(s)";
}

inline constexpr const char* kGoalSubmittedChrome = "Goal submitted";
inline constexpr const char* kMessageSubmittedChrome = "Message submitted";

/**
 * ProgressSource for a ControllerEvent delivered through the active backend.
 * is_remote (Engine HTTP / SSE) → EngineEvent; Local in-process → LocalBackendEvent.
 * Not tied to supportsIngest (R1).
 */
inline ProgressSource progressSourceForBackendEvent(bool is_remote) {
    return is_remote ? ProgressSource::EngineEvent
                     : ProgressSource::LocalBackendEvent;
}

/*
 * Grep regression checklist (Phase 5 — review before merge):
 *
 * Forbidden outside backend signal handlers / API outcome paths:
 *   - m_ragIndexingCount++ / -- without INDEXING_* handler
 *   - SetActivityMessage("Indexing…") / "Planning…" / "Syncing…" from UserAction
 *   - SetTransientStatus("…indexing…") / "Syncing RAG…" / "Planning goal…" from drop/send
 *   - m_goalPlanningPending = true from OnSend / revise / Run Goal (wait for STATE_CHANGED)
 *
 * Allowed:
 *   - INDEXING_* / STATE_CHANGED / PLAN_* / STEP_* handlers (ProgressSource::*Event)
 *   - Chrome: "Sessions saved", "Goal submitted", "Message submitted", host-only add copy
 *   - Phase 1 host-only labels (RemoteRagHonesty) — honesty, not progress fiction
 */

} // namespace Thoth

#endif // THOTH_PROGRESS_SOURCE_H
