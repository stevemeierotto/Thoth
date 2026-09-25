/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — chat Send pending / unlock / watchdog helpers (pure; no wx)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_CHAT_SEND_CHROME_H
#define THOTH_CHAT_SEND_CHROME_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Thoth {
namespace ChatSendChrome {

/** Default GUI pending-chat watchdog (ms). Unlock Send only — must not be much
 *  shorter than remote chat HTTP (kChatTimeoutSec 900) or late success is dropped. */
inline constexpr std::int64_t kPendingWatchdogMs = 900000; // 15 minutes

inline constexpr const char* kWaitingForEngineStatus =
    "Waiting for Engine… (Send locked — Esc in chat, or Agent → Unlock Send)";
inline constexpr const char* kSendLockedStatus =
    "Send locked: chat still in flight. Esc in chat, or Agent → Unlock Send.";
inline constexpr const char* kUnlockedSendStatus =
    "Send unlocked for this chat. Prior Engine work may still finish in the background.";
inline constexpr const char* kWatchdogStatus =
    "Still waiting on Engine — Send re-enabled. Reply will appear when ready.";

inline int pendingCountForSession(
    const std::unordered_map<std::string, int>& in_flight_by_session,
    const std::string& session_id) {
    if (session_id.empty()) {
        return 0;
    }
    const auto it = in_flight_by_session.find(session_id);
    return it == in_flight_by_session.end() ? 0 : std::max(0, it->second);
}

inline bool sendEnabledForSession(
    const std::unordered_map<std::string, int>& in_flight_by_session,
    const std::string& session_id) {
    return pendingCountForSession(in_flight_by_session, session_id) == 0;
}

inline void registerPending(std::unordered_map<std::string, int>& in_flight_by_session,
                            const std::string& session_id) {
    if (session_id.empty()) {
        return;
    }
    ++in_flight_by_session[session_id];
}

/** Decrement one pending turn; erase key at zero. */
inline void clearOnePending(std::unordered_map<std::string, int>& in_flight_by_session,
                            const std::string& session_id) {
    if (session_id.empty()) {
        return;
    }
    auto it = in_flight_by_session.find(session_id);
    if (it == in_flight_by_session.end()) {
        return;
    }
    it->second = std::max(0, it->second - 1);
    if (it->second == 0) {
        in_flight_by_session.erase(it);
    }
}

/** Force unlock Send for a session (cancel / orphan completion / watchdog). */
inline void forceClearSessionPending(
    std::unordered_map<std::string, int>& in_flight_by_session,
    const std::string& session_id) {
    if (session_id.empty()) {
        return;
    }
    in_flight_by_session.erase(session_id);
}

inline bool watchdogShouldFire(std::int64_t pending_started_at_ms,
                               std::int64_t now_ms,
                               std::int64_t timeout_ms = kPendingWatchdogMs) {
    if (pending_started_at_ms <= 0 || now_ms < pending_started_at_ms || timeout_ms <= 0) {
        return false;
    }
    return (now_ms - pending_started_at_ms) >= timeout_ms;
}

} // namespace ChatSendChrome
} // namespace Thoth

#endif // THOTH_CHAT_SEND_CHROME_H
