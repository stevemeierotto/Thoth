/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — chat-turn UI status helpers (pure; no wx)
 *
 * Engine success ≠ transcript visibility. Chrome stays until the current turn's
 * assistant content is present in the originating session, or failure is shown.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_CHAT_TURN_UI_STATUS_H
#define THOTH_CHAT_TURN_UI_STATUS_H

#include "ChatSessionTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Thoth {
namespace ChatTurnUi {

enum class Phase {
    Idle = 0,
    Accepted,
    WaitingEngine,
    Refreshing,
    Completed,
    Failed
};

inline bool isTerminal(Phase p) {
    return p == Phase::Completed || p == Phase::Failed;
}

inline bool isActivePhase(Phase p) {
    return p == Phase::Accepted || p == Phase::WaitingEngine || p == Phase::Refreshing;
}

/** Narrow chat-turn ownership — not a general GUI state framework. */
struct ChatTurnUiState {
    Phase phase = Phase::Idle;
    std::string request_id;
    std::string session_id;
    std::string user_content;
    /** Set on Engine chat success from OperationResult.response_text. */
    std::string expected_assistant_content;
    std::int64_t started_at_ms = 0;
    bool terminal = false;
    /** 0 = not yet refreshed after success; increments per GET attempt. */
    int refresh_attempt = 0;
    /** Watchdog unlocked Send while Engine request may still complete. */
    bool send_unlocked_early = false;
};

inline bool identitiesMatch(const ChatTurnUiState& turn,
                            const std::string& request_id,
                            const std::string& session_id) {
    return !turn.request_id.empty() && !turn.session_id.empty()
        && turn.request_id == request_id && turn.session_id == session_id;
}

/**
 * Stale-callback invariant: only the active non-terminal turn with matching
 * request_id + session_id may mutate chat-turn UI / that turn's phase.
 */
inline bool mayMutateTurn(const std::optional<ChatTurnUiState>& active,
                          const std::string& request_id,
                          const std::string& session_id) {
    if (!active.has_value() || active->terminal || isTerminal(active->phase)) {
        return false;
    }
    return identitiesMatch(*active, request_id, session_id);
}

/**
 * Prove the refreshed transcript contains this turn's assistant.
 * Message-count increase alone is insufficient.
 * Uses exact user_content + expected_assistant_content pair (last match wins).
 */
inline bool transcriptContainsCurrentTurnAssistant(
    const std::vector<ChatMessage>& messages,
    const std::string& user_content,
    const std::string& expected_assistant_content) {
    if (expected_assistant_content.empty() || user_content.empty()) {
        return false;
    }
    int last_match_assistant_index = -1;
    for (std::size_t i = 0; i < messages.size(); ++i) {
        if (messages[i].role != "user" || messages[i].content != user_content) {
            continue;
        }
        for (std::size_t j = i + 1; j < messages.size(); ++j) {
            if (messages[j].role == "user") {
                break;
            }
            if (messages[j].role == "assistant"
                && messages[j].content == expected_assistant_content) {
                last_match_assistant_index = static_cast<int>(j);
                break;
            }
        }
    }
    return last_match_assistant_index >= 0;
}

inline const char* phaseLabel(Phase p) {
    switch (p) {
        case Phase::Accepted:
            return "Sending…";
        case Phase::WaitingEngine:
            return "Waiting for Engine…";
        case Phase::Refreshing:
            return "Loading reply…";
        case Phase::Completed:
            return "";
        case Phase::Failed:
            return "";
        case Phase::Idle:
        default:
            return "";
    }
}

/** Elapsed suffix for waiting_engine chrome, e.g. " (12s)" or " (2m 5s)". */
inline std::string formatElapsedSuffix(std::int64_t started_at_ms, std::int64_t now_ms) {
    if (started_at_ms <= 0 || now_ms < started_at_ms) {
        return {};
    }
    const std::int64_t elapsed_s = (now_ms - started_at_ms) / 1000;
    if (elapsed_s < 60) {
        return " (" + std::to_string(elapsed_s) + "s)";
    }
    const std::int64_t minutes = elapsed_s / 60;
    const std::int64_t seconds = elapsed_s % 60;
    return " (" + std::to_string(minutes) + "m " + std::to_string(seconds) + "s)";
}

inline std::string statusTextForTurn(const ChatTurnUiState& turn, std::int64_t now_ms) {
    if (turn.phase == Phase::WaitingEngine && turn.send_unlocked_early) {
        std::string out = "Still waiting for Engine…";
        out += formatElapsedSuffix(turn.started_at_ms, now_ms);
        return out;
    }
    const char* base = phaseLabel(turn.phase);
    if (base == nullptr || base[0] == '\0') {
        return {};
    }
    std::string out(base);
    if (turn.phase == Phase::WaitingEngine) {
        out += formatElapsedSuffix(turn.started_at_ms, now_ms);
    }
    return out;
}

inline constexpr const char* kAnotherReplyInProgressStatus =
    "Waiting — another reply is in progress.";

inline constexpr const char* kReplyNotInTranscriptStatus =
    "Reply received from Engine but it isn’t in the chat yet.";

/** GET attempts after Engine success: initial + retries (second check required). */
inline constexpr int kMaxRefreshAttempts = 2;
/** Delay between transcript GET verify attempts. */
inline constexpr int kRefreshRetryDelayMs = 500;

} // namespace ChatTurnUi
} // namespace Thoth

#endif // THOTH_CHAT_TURN_UI_STATUS_H
