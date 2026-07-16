/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: remote HTTP client helpers (offline-testable, no curl)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#pragma once

#include "json.hpp"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace ThothRemoteHttp {

/**
 * Named timeouts for RemoteAgentBackend (libcurl).
 *
 * Chat/goals defaults align with engine→llama LLM budget (default 600s via
 * THOTH_LLM_TIMEOUT_SECONDS). Goals allow two planner LLM attempts + margin.
 * Override both request timeouts with THOTH_REMOTE_HTTP_TIMEOUT_SECONDS.
 */
inline constexpr long kConnectTimeoutSec = 10L;
inline constexpr long kControlTimeoutSec = 30L;
inline constexpr long kChatTimeoutSec = 600L;
inline constexpr long kGoalsTimeoutSec = 1260L; // 2 * 600 + 60 planner-retry margin
inline constexpr long kHealthReadyTimeoutSec = 10L;

/**
 * Resolve libcurl CURLOPT_TIMEOUT for /v1/chat or /v1/goals.
 * If THOTH_REMOTE_HTTP_TIMEOUT_SECONDS is set to a positive integer, that value
 * wins; otherwise `fallback` (typically kChatTimeoutSec / kGoalsTimeoutSec).
 */
inline long resolveRemoteRequestTimeoutSec(long fallback) {
    if (const char* env = std::getenv("THOTH_REMOTE_HTTP_TIMEOUT_SECONDS")) {
        try {
            const long parsed = std::stol(std::string(env));
            if (parsed > 0) {
                return parsed;
            }
        } catch (...) {
            // ignore invalid env; use fallback
        }
    }
    return fallback > 0 ? fallback : kChatTimeoutSec;
}

inline std::string trimWhitespace(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\n'
                          || s.front() == '\r')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n'
                          || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

/** Strip a single trailing '/' — preserve scheme/host/port. */
inline std::string normalizeBaseUrl(std::string url) {
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    return url;
}

/**
 * Plan K4 selection (pure): raw env value → optional Remote base URL.
 * nullopt ⇒ Local; engaged string ⇒ Remote (trailing slash normalized).
 * Does not connect; does not read getenv itself when given an explicit value.
 */
inline std::optional<std::string> resolveEngineBaseUrlFromRawEnvValue(const char* raw_or_null) {
    if (!raw_or_null) {
        return std::nullopt;
    }
    std::string trimmed = trimWhitespace(raw_or_null);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return normalizeBaseUrl(std::move(trimmed));
}

/** AgentInterface-only: read THOTH_ENGINE_URL. Backends must not call this. */
inline std::optional<std::string> resolveThothEngineUrlFromEnv() {
    return resolveEngineBaseUrlFromRawEnvValue(std::getenv("THOTH_ENGINE_URL"));
}

/**
 * If body contains a capabilities array, require chat/goals/control.
 * Missing capabilities key → accept (K0 §5 / K2 §ready).
 * Returns empty string on OK, otherwise failure reason.
 */
inline std::string validateReadyCapabilities(const nlohmann::json& body) {
    if (!body.contains("capabilities")) {
        return {};
    }
    if (!body["capabilities"].is_array()) {
        return "capabilities field is not an array";
    }

    bool has_chat = false;
    bool has_goals = false;
    bool has_control = false;
    for (const auto& cap : body["capabilities"]) {
        if (!cap.is_string()) {
            continue;
        }
        const std::string& s = cap.get_ref<const std::string&>();
        if (s == "chat") has_chat = true;
        else if (s == "goals") has_goals = true;
        else if (s == "control") has_control = true;
    }

    if (!has_chat || !has_goals || !has_control) {
        return "ready capabilities missing required chat/goals/control";
    }
    return {};
}

/** Plan F /v1/chat request body (pure). */
inline nlohmann::json buildChatRequestJson(const std::string& text,
                                           const std::string& session_id) {
    return {{"text", text}, {"session_id", session_id}};
}

/** Plan F /v1/goals request body (pure). */
inline nlohmann::json buildGoalRequestJson(const std::string& goal,
                                           const std::string& session_id) {
    return {{"goal", goal}, {"session_id", session_id}};
}

/**
 * Extract chat response text from /v1/chat JSON body.
 * ok=true + text set on success; ok=false + error set on hard failure;
 * ok=true + empty text when "response" missing (caller may warn).
 */
struct ChatResponseExtract {
    bool ok{false};
    std::string text;
    std::string error;
};

inline ChatResponseExtract extractChatResponseText(const nlohmann::json& body) {
    ChatResponseExtract out;
    if (!body.contains("response")) {
        out.ok = true;
        return out;
    }
    if (!body["response"].is_string()) {
        out.error = "[RemoteEngine] chat response field is not a string";
        return out;
    }
    out.ok = true;
    out.text = body["response"].get<std::string>();
    return out;
}

/**
 * Validate /v1/goals response JSON (tolerates missing status/message).
 * ok=true on acceptable body; warning non-empty when semantic fields absent.
 */
struct GoalResponseCheck {
    bool ok{false};
    std::string warning;
    std::string error;
};

inline GoalResponseCheck checkGoalResponseBody(const nlohmann::json& body) {
    GoalResponseCheck out;
    out.ok = true;
    if (!body.contains("status") && !body.contains("message")) {
        out.warning = "missing semantic fields status/message (tolerated)";
    }
    return out;
}

/** Prefer EngineError JSON: { "error": { "code", "message" } }. */
inline std::string formatHttpErrorMessage(long http_status, const std::string& body) {
    if (!body.empty()) {
        try {
            const auto j = nlohmann::json::parse(body);
            if (j.contains("error") && j["error"].is_object()) {
                const auto& err = j["error"];
                const std::string code = err.value("code", "");
                const std::string message = err.value("message", "");
                if (!code.empty() || !message.empty()) {
                    std::string out = "[RemoteEngine] HTTP " + std::to_string(http_status);
                    if (!code.empty()) out += " " + code;
                    if (!message.empty()) out += ": " + message;
                    return out;
                }
            }
            if (j.contains("message") && j["message"].is_string()) {
                return "[RemoteEngine] HTTP " + std::to_string(http_status) + ": "
                    + j["message"].get<std::string>();
            }
        } catch (...) {
            // fall through to generic
        }
    }
    return "[RemoteEngine] HTTP " + std::to_string(http_status)
        + (body.empty() ? "" : (": " + body.substr(0, 200)));
}

} // namespace ThothRemoteHttp
