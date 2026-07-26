/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — GUI Phase 6 SSE connection / engine health (pure; no wx)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_ENGINE_CONNECTION_STATE_H
#define THOTH_ENGINE_CONNECTION_STATE_H

#include <algorithm>
#include <cstdint>
#include <string>

namespace Thoth {

/** Phase 6 — SSE event stream connection state machine. */
enum class EventConnectionState {
    Connected,
    Disconnected,
    Reconnecting,
    Failed
};

/** Phase 6 — Engine process readiness (distinct from SSE connection). */
enum class EngineHealthState {
    Unknown,
    Ready,
    Starting,
    Unavailable
};

/** Snapshot for GUI polling — thread-safe copy from RemoteAgentBackend. */
struct EventStreamSnapshot {
    EventConnectionState connection{EventConnectionState::Disconnected};
    EngineHealthState engine{EngineHealthState::Unknown};
    /** Wall-clock ms of last received SSE event; 0 if none yet. */
    int64_t last_event_ms{0};
    int64_t snapshot_ms{0};
    bool sse_enabled{false};
    bool applies{false}; // false in Local mode (no SSE chrome)
};

inline constexpr int64_t kSseReconnectInitialMs = 1000;
inline constexpr int64_t kSseReconnectMaxMs = 30000;

/** Exponential backoff with cap — attempt 0 → initial, then ×2 per step. */
inline int64_t sseReconnectDelayMs(int attempt) {
    if (attempt < 0) {
        attempt = 0;
    }
    int64_t delay = kSseReconnectInitialMs;
    for (int i = 0; i < attempt && delay < kSseReconnectMaxMs; ++i) {
        delay = std::min(delay * 2, kSseReconnectMaxMs);
    }
    return delay;
}

inline const char* eventConnectionStateLabel(EventConnectionState state) {
    switch (state) {
        case EventConnectionState::Connected: return "Connected";
        case EventConnectionState::Disconnected: return "Disconnected";
        case EventConnectionState::Reconnecting: return "Reconnecting";
        case EventConnectionState::Failed: return "Failed";
    }
    return "Unknown";
}

inline const char* engineHealthStateLabel(EngineHealthState state) {
    switch (state) {
        case EngineHealthState::Unknown: return "Unknown";
        case EngineHealthState::Ready: return "Ready";
        case EngineHealthState::Starting: return "Starting";
        case EngineHealthState::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

inline std::string formatEventsStatusLine(EventConnectionState state) {
    return std::string("Events: ") + eventConnectionStateLabel(state);
}

inline std::string formatEngineStatusLine(EngineHealthState state) {
    return std::string("Engine: ") + engineHealthStateLabel(state);
}

/** Diagnostics-only last-event-age (Phase 6). */
inline std::string formatLastEventAgeLabel(int64_t last_event_ms, int64_t now_ms) {
    if (last_event_ms <= 0) {
        return "Last event: —";
    }
    if (now_ms <= last_event_ms) {
        return "Last event: 0 s ago";
    }
    const int64_t age_sec = (now_ms - last_event_ms) / 1000;
    return "Last event: " + std::to_string(age_sec) + " s ago";
}

inline bool engineHttpUsable(EngineHealthState engine) {
    return engine == EngineHealthState::Ready;
}

/** Stale-live invariant helper — show connection chrome whenever not Connected. */
inline bool shouldShowConnectionIndicator(const EventStreamSnapshot& snap) {
    return snap.applies && snap.connection != EventConnectionState::Connected;
}

inline bool shouldShowEngineIndicator(const EventStreamSnapshot& snap) {
    return snap.applies && snap.engine != EngineHealthState::Ready;
}

/** Local mode — in-process events; no SSE reconnect UI. */
inline EventStreamSnapshot localEventStreamSnapshot(int64_t now_ms) {
    EventStreamSnapshot s;
    s.connection = EventConnectionState::Connected;
    s.engine = EngineHealthState::Ready;
    s.snapshot_ms = now_ms;
    s.sse_enabled = false;
    s.applies = false;
    return s;
}

} // namespace Thoth

#endif // THOTH_ENGINE_CONNECTION_STATE_H
