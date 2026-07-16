/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: pure SSE framing + EngineEvent → ControllerEvent mapping
 * (no networking, no callbacks)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#pragma once

#include "controller_event.h"
#include "json.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ThothRemoteHttp {

/** Connect timeout for opening the SSE stream (stream itself uses TIMEOUT=0). */
inline constexpr long kSseConnectTimeoutSec = 10L;

/** True if capabilities absent, or array contains "events". */
inline bool eventsCapabilityAllowsSse(const nlohmann::json& ready_body) {
    if (!ready_body.contains("capabilities")) {
        return true;
    }
    if (!ready_body["capabilities"].is_array()) {
        return false;
    }
    for (const auto& cap : ready_body["capabilities"]) {
        if (cap.is_string() && cap.get_ref<const std::string&>() == "events") {
            return true;
        }
    }
    return false;
}

inline std::optional<EventType> parseEventType(const std::string& name) {
    if (name == "PLAN_CREATED") return EventType::PLAN_CREATED;
    if (name == "STEP_STARTED") return EventType::STEP_STARTED;
    if (name == "STEP_COMPLETED") return EventType::STEP_COMPLETED;
    if (name == "STEP_FAILED") return EventType::STEP_FAILED;
    if (name == "STEP_RETRYING") return EventType::STEP_RETRYING;
    if (name == "PLAN_REVISED") return EventType::PLAN_REVISED;
    if (name == "PLAN_COMPLETED") return EventType::PLAN_COMPLETED;
    if (name == "PLAN_ABORTED") return EventType::PLAN_ABORTED;
    if (name == "PLAN_FAILED") return EventType::PLAN_FAILED;
    if (name == "STATE_CHANGED") return EventType::STATE_CHANGED;
    if (name == "MODE_SWITCHED") return EventType::MODE_SWITCHED;
    if (name == "EMBEDDING_FAILED") return EventType::EMBEDDING_FAILED;
    if (name == "RETRIEVAL_DIAGNOSTICS") return EventType::RETRIEVAL_DIAGNOSTICS;
    if (name == "INDEXING_STARTED") return EventType::INDEXING_STARTED;
    if (name == "INDEXING_COMPLETED") return EventType::INDEXING_COMPLETED;
    if (name == "PLAN_REUSE_INJECTION") return EventType::PLAN_REUSE_INJECTION;
    if (name == "REFLECTION_REPLAN") return EventType::REFLECTION_REPLAN;
    if (name == "PLAN_HISTORY_STORED") return EventType::PLAN_HISTORY_STORED;
    return std::nullopt;
}

/**
 * Map EngineEvent JSON to ControllerEvent.
 * Missing optional fields tolerated; unknown/missing type → nullopt.
 */
inline std::optional<ControllerEvent> engineEventJsonToControllerEvent(
    const nlohmann::json& body) {
    if (!body.contains("type") || !body["type"].is_string()) {
        return std::nullopt;
    }
    const auto type = parseEventType(body["type"].get<std::string>());
    if (!type.has_value()) {
        return std::nullopt;
    }

    ControllerEvent ev;
    ev.type = *type;
    ev.session_id = body.value("session_id", "");
    ev.plan_id = body.value("plan_id", "");
    ev.step_id = body.value("step_id", "");
    ev.controller_state_name = body.value("controller_state_name", "");
    ev.timestamp_ms = 0;
    if (body.contains("metadata") && body["metadata"].is_object()) {
        ev.metadata = body["metadata"];
    } else {
        ev.metadata = nlohmann::json::object();
    }
    return ev;
}

/**
 * Append bytes to buffer; extract complete SSE frames ending at blank line.
 * Returns ordered complete frame strings (without the trailing blank line).
 * Leaves incomplete tail in buffer. Never parses JSON.
 */
inline std::vector<std::string> extractCompleteSseFrames(std::string& buffer) {
    std::vector<std::string> frames;
    for (;;) {
        const std::size_t pos_nn = buffer.find("\n\n");
        const std::size_t pos_rr = buffer.find("\r\n\r\n");
        std::size_t pos = std::string::npos;
        std::size_t delim_len = 0;
        if (pos_nn != std::string::npos && (pos_rr == std::string::npos || pos_nn <= pos_rr)) {
            pos = pos_nn;
            delim_len = 2;
        } else if (pos_rr != std::string::npos) {
            pos = pos_rr;
            delim_len = 4;
        }
        if (pos == std::string::npos) {
            break;
        }
        frames.push_back(buffer.substr(0, pos));
        buffer.erase(0, pos + delim_len);
    }
    return frames;
}

/**
 * Parse one SSE frame into data payload JSON text (concatenated data: lines).
 * Returns nullopt if no data lines / empty. Does not parse JSON.
 */
inline std::optional<std::string> sseFrameDataPayload(const std::string& frame) {
    std::string data;
    bool have_data = false;
    std::size_t start = 0;
    while (start <= frame.size()) {
        std::size_t end = frame.find('\n', start);
        if (end == std::string::npos) {
            end = frame.size();
        }
        std::string line = frame.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("data:", 0) == 0) {
            std::string value = line.substr(5);
            if (!value.empty() && value[0] == ' ') {
                value.erase(0, 1);
            }
            if (have_data) {
                data.push_back('\n');
            }
            data += value;
            have_data = true;
        }
        if (end == frame.size()) {
            break;
        }
        start = end + 1;
    }
    if (!have_data) {
        return std::nullopt;
    }
    return data;
}

} // namespace ThothRemoteHttp
