/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: RemoteAgentBackend implementation (HTTP + SSE)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "remote_agent_backend.h"
#include "decision_summary.h"
#include "conversation_authority.h"
#include "corpus_create.h"
#include "corpus_create_local.h"
#include "corpus_documents.h"
#include "research_resources.h"
#include "graph_statistics.h"
#include "engine_connection_state.h"
#include "operation_result.h"

#include <curl/curl.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

using json = nlohmann::json;
using ThothRemoteHttp::buildChatRequestJson;
using ThothRemoteHttp::buildGoalRequestJson;
using ThothRemoteHttp::checkGoalResponseBody;
using ThothRemoteHttp::engineEventJsonToControllerEvent;
using ThothRemoteHttp::eventsCapabilityAllowsSse;
using ThothRemoteHttp::extractChatResponseText;
using ThothRemoteHttp::extractCompleteSseFrames;
using ThothRemoteHttp::formatHttpErrorMessage;
using ThothRemoteHttp::kChatTimeoutSec;
using ThothRemoteHttp::kConnectTimeoutSec;
using ThothRemoteHttp::kControlTimeoutSec;
using ThothRemoteHttp::kGoalsTimeoutSec;
using ThothRemoteHttp::kHealthReadyTimeoutSec;
using ThothRemoteHttp::kSseConnectTimeoutSec;
using ThothRemoteHttp::normalizeBaseUrl;
using ThothRemoteHttp::resolveRemoteRequestTimeoutSec;
using ThothRemoteHttp::sseFrameDataPayload;
using ThothRemoteHttp::validateReadyCapabilities;

namespace {

size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

struct SseWriteContext {
    RemoteAgentBackend* backend{nullptr};
    std::string* buffer{nullptr};
    std::atomic<bool>* cancel{nullptr};
};

void logRemoteError(const char* operation, const std::string& detail) {
    std::cerr << "[RemoteAgentBackend] " << operation << " failed: " << detail << "\n";
}

void logRemoteWarning(const char* operation, const std::string& detail) {
    std::cerr << "[RemoteAgentBackend] " << operation << " warning: " << detail << "\n";
}

int sseProgressCallback(void* clientp,
                        curl_off_t /*dltotal*/,
                        curl_off_t /*dlnow*/,
                        curl_off_t /*ultotal*/,
                        curl_off_t /*ulnow*/) {
    auto* cancel = static_cast<std::atomic<bool>*>(clientp);
    if (cancel && cancel->load()) {
        return 1; // abort
    }
    return 0;
}

size_t sseWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<SseWriteContext*>(userdata);
    if (!ctx || !ctx->cancel || ctx->cancel->load()) {
        return 0;
    }
    const size_t total = size * nmemb;
    ctx->buffer->append(ptr, total);
    ctx->backend->dispatchSseFrames(*ctx->buffer);
    return total;
}

} // namespace

RemoteAgentBackend::RemoteAgentBackend(std::string base_url)
    : base_url_(normalizeBaseUrl(std::move(base_url))) {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    snapshot_.applies = true;
    snapshot_.sse_enabled = true;
    snapshot_.connection = Thoth::EventConnectionState::Disconnected;
    snapshot_.engine = Thoth::EngineHealthState::Unknown;
    snapshot_.snapshot_ms = nowMs();
}

RemoteAgentBackend::~RemoteAgentBackend() {
    stopSse();
}

int64_t RemoteAgentBackend::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void RemoteAgentBackend::updateConnectionState(Thoth::EventConnectionState state) {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    snapshot_.connection = state;
    publishSnapshotLocked();
}

void RemoteAgentBackend::publishSnapshotLocked() {
    snapshot_.last_event_ms = last_event_ms_.load();
    snapshot_.snapshot_ms = nowMs();
}

Thoth::EventStreamSnapshot RemoteAgentBackend::eventStreamSnapshot() const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    Thoth::EventStreamSnapshot copy = snapshot_;
    copy.last_event_ms = last_event_ms_.load();
    copy.snapshot_ms = nowMs();
    return copy;
}

void RemoteAgentBackend::sleepInterruptibleMs(int64_t delay_ms) {
    constexpr int64_t kSliceMs = 100;
    int64_t remaining = delay_ms;
    while (remaining > 0 && !sse_cancel_.load()) {
        const int64_t slice = std::min(remaining, kSliceMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(slice));
        remaining -= slice;
    }
}

bool RemoteAgentBackend::probeEngineHealth(Thoth::EngineHealthState& out) {
    out = Thoth::EngineHealthState::Unavailable;
    if (base_url_.empty()) {
        return false;
    }

    const HttpResult health = httpGet("/health", kHealthReadyTimeoutSec);
    if (!health.transport_ok || health.status < 200 || health.status >= 300) {
        out = Thoth::EngineHealthState::Unavailable;
        return false;
    }

    const HttpResult ready = httpGet("/ready", kHealthReadyTimeoutSec);
    if (!ready.transport_ok) {
        out = Thoth::EngineHealthState::Starting;
        return false;
    }
    if (ready.status >= 200 && ready.status < 300) {
        out = Thoth::EngineHealthState::Ready;
        return true;
    }
    out = Thoth::EngineHealthState::Starting;
    return false;
}

void RemoteAgentBackend::setEventHandler(std::function<void(const ControllerEvent&)> handler) {
    try {
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            event_handler_ = std::move(handler);
        }
        // Atomic replace only — do not restart SSE (K3 §6). May start if not yet started.
        startSseIfNeeded();
    } catch (const std::exception& ex) {
        logRemoteError("setEventHandler", ex.what());
    } catch (...) {
        logRemoteError("setEventHandler", "unknown error");
    }
}

void RemoteAgentBackend::setSessionId(const std::string& sessionId) {
    try {
        std::lock_guard<std::mutex> lock(session_mutex_);
        session_id_ = sessionId;
    } catch (const std::exception& ex) {
        logRemoteError("setSessionId", ex.what());
    } catch (...) {
        logRemoteError("setSessionId", "unknown error");
    }
}

RemoteAgentBackend::HttpResult RemoteAgentBackend::httpGet(const std::string& path,
                                                           long timeout_sec) const {
    HttpResult result;
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.transport_error = "curl_easy_init failed";
        return result;
    }

    const std::string url = base_url_ + path;
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        result.transport_error = curl_easy_strerror(code);
        curl_easy_cleanup(curl);
        return result;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    curl_easy_cleanup(curl);
    result.transport_ok = true;
    result.body = std::move(body);
    return result;
}

RemoteAgentBackend::HttpResult RemoteAgentBackend::httpPostJson(const std::string& path,
                                                                const std::string& json_body,
                                                                long timeout_sec) const {
    HttpResult result;
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.transport_error = "curl_easy_init failed";
        return result;
    }

    const std::string url = base_url_ + path;
    std::string body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        result.transport_error = curl_easy_strerror(code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return result;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    result.transport_ok = true;
    result.body = std::move(body);
    return result;
}

bool RemoteAgentBackend::ensureReady(std::string& error_out) {
    bool just_became_ready = false;
    try {
        {
            std::lock_guard<std::mutex> lock(ready_mutex_);
            if (ready_checked_) {
                if (!ready_ok_) {
                    error_out = ready_error_;
                }
                return ready_ok_;
            }

            ready_checked_ = true;
            ready_ok_ = false;
            events_sse_allowed_ = false;
            ingest_allowed_ = false;
            conversation_allowed_ = false;
            strategies_allowed_ = false;
            trajectories_allowed_ = false;
            episodes_allowed_ = false;
            graph_stats_allowed_ = false;

            if (base_url_.empty()) {
                ready_error_ = "[RemoteEngine] empty base URL";
                error_out = ready_error_;
                return false;
            }

            const HttpResult health = httpGet("/health", kHealthReadyTimeoutSec);
            if (!health.transport_ok) {
                ready_error_ = "[RemoteEngine] /health transport: " + health.transport_error;
                error_out = ready_error_;
                return false;
            }
            if (health.status < 200 || health.status >= 300) {
                ready_error_ = formatHttpErrorMessage(health.status, health.body);
                error_out = ready_error_;
                return false;
            }

            const HttpResult ready = httpGet("/ready", kHealthReadyTimeoutSec);
            if (!ready.transport_ok) {
                ready_error_ = "[RemoteEngine] /ready transport: " + ready.transport_error;
                error_out = ready_error_;
                return false;
            }
            if (ready.status < 200 || ready.status >= 300) {
                ready_error_ = formatHttpErrorMessage(ready.status, ready.body);
                error_out = ready_error_;
                return false;
            }

            if (!ready.body.empty()) {
                try {
                    const json body = json::parse(ready.body);
                    const std::string cap_err = validateReadyCapabilities(body);
                    if (!cap_err.empty()) {
                        ready_error_ = "[RemoteEngine] " + cap_err;
                        error_out = ready_error_;
                        return false;
                    }
                    events_sse_allowed_ = eventsCapabilityAllowsSse(body);
                    ingest_allowed_ = Thoth::CorpusCreate::readyCapabilitiesIncludeIngest(body);
                    conversation_allowed_ =
                        Thoth::ConversationAuthority::readyCapabilitiesIncludeConversation(body);
                    strategies_allowed_ =
                        Thoth::ResearchResources::readyCapabilitiesIncludeStrategies(body);
                    trajectories_allowed_ =
                        Thoth::ResearchResources::readyCapabilitiesIncludeTrajectories(body);
                    episodes_allowed_ =
                        Thoth::ResearchResources::readyCapabilitiesIncludeEpisodes(body);
                    graph_stats_allowed_ =
                        Thoth::GraphStatistics::readyCapabilitiesIncludeGraphStats(body);
                    if (!events_sse_allowed_) {
                        logRemoteWarning("ensureReady",
                                         "capabilities present without \"events\"; SSE disabled");
                    }
                } catch (const std::exception& ex) {
                    ready_error_ =
                        std::string("[RemoteEngine] /ready JSON parse failed: ") + ex.what();
                    error_out = ready_error_;
                    return false;
                } catch (...) {
                    ready_error_ = "[RemoteEngine] /ready JSON parse failed";
                    error_out = ready_error_;
                    return false;
                }
            } else {
                events_sse_allowed_ = true;
                ingest_allowed_ = false;
                conversation_allowed_ = false;
                strategies_allowed_ = false;
                trajectories_allowed_ = false;
                episodes_allowed_ = false;
            graph_stats_allowed_ = false;
            }

            ready_ok_ = true;
            ready_error_.clear();
            just_became_ready = true;
        }

        if (just_became_ready) {
            startSseIfNeeded();
        }
        return true;
    } catch (const std::exception& ex) {
        logRemoteError("ensureReady", ex.what());
        error_out = std::string("[RemoteEngine] ensureReady: ") + ex.what();
        return false;
    } catch (...) {
        logRemoteError("ensureReady", "unknown error");
        error_out = "[RemoteEngine] ensureReady: unknown error";
        return false;
    }
}

void RemoteAgentBackend::dispatchSseFrames(std::string& buffer) {
    const std::vector<std::string> frames = extractCompleteSseFrames(buffer);
    for (const std::string& frame : frames) {
        if (sse_cancel_.load()) {
            return;
        }
        auto payload = sseFrameDataPayload(frame);
        if (!payload.has_value()) {
            continue; // keepalive / comment-only
        }
        try {
            const json body = json::parse(*payload);
            auto ev = engineEventJsonToControllerEvent(body);
            if (!ev.has_value()) {
                logRemoteWarning("sse", "skipped event (unknown type or missing fields)");
                continue;
            }
            std::function<void(const ControllerEvent&)> handler;
            {
                std::lock_guard<std::mutex> lock(handler_mutex_);
                handler = event_handler_;
            }
            if (handler) {
                handler(*ev); // serial, receive order
            }
            last_event_ms_.store(nowMs());
            {
                std::lock_guard<std::mutex> lock(snapshot_mutex_);
                publishSnapshotLocked();
            }
        } catch (const std::exception& ex) {
            logRemoteWarning("sse", std::string("parse failure (continue): ") + ex.what());
        } catch (...) {
            logRemoteWarning("sse", "parse failure (continue): unknown");
        }
    }
}

void RemoteAgentBackend::startSseIfNeeded() {
    try {
        std::lock_guard<std::mutex> lock(sse_start_mutex_);
        if (sse_started_) {
            return;
        }

        bool ready = false;
        bool events_ok = false;
        {
            std::lock_guard<std::mutex> rlock(ready_mutex_);
            ready = ready_ok_;
            events_ok = events_sse_allowed_;
        }
        if (!ready || !events_ok) {
            if (ready && !events_ok) {
                updateConnectionState(Thoth::EventConnectionState::Failed);
                {
                    std::lock_guard<std::mutex> lock(snapshot_mutex_);
                    snapshot_.sse_enabled = false;
                    publishSnapshotLocked();
                }
            }
            return;
        }

        {
            std::lock_guard<std::mutex> hlock(handler_mutex_);
            if (!event_handler_) {
                return;
            }
        }

        sse_cancel_.store(false);
        sse_started_ = true;
        sse_thread_ = std::thread(&RemoteAgentBackend::sseLoop, this);
    } catch (const std::exception& ex) {
        logRemoteError("startSseIfNeeded", ex.what());
        sse_started_ = false;
    } catch (...) {
        logRemoteError("startSseIfNeeded", "unknown error");
        sse_started_ = false;
    }
}

void RemoteAgentBackend::stopSse() {
    sse_cancel_.store(true);
    updateConnectionState(Thoth::EventConnectionState::Failed);
    if (sse_thread_.joinable()) {
        sse_thread_.join();
    }
    sse_started_ = false;
}

void RemoteAgentBackend::sseLoop() {
    reconnect_attempt_ = 0;
    sseReconnectLoop();
}

void RemoteAgentBackend::sseReconnectLoop() {
    while (!sse_cancel_.load()) {
        Thoth::EngineHealthState engine_state = Thoth::EngineHealthState::Unknown;
        (void)probeEngineHealth(engine_state);
        {
            std::lock_guard<std::mutex> lock(snapshot_mutex_);
            snapshot_.engine = engine_state;
            publishSnapshotLocked();
        }

        if (engine_state != Thoth::EngineHealthState::Ready) {
            updateConnectionState(Thoth::EventConnectionState::Reconnecting);
            const int64_t delay = Thoth::sseReconnectDelayMs(reconnect_attempt_++);
            sleepInterruptibleMs(delay);
            continue;
        }

        if (reconnect_attempt_ > 0) {
            updateConnectionState(Thoth::EventConnectionState::Reconnecting);
        } else {
            updateConnectionState(Thoth::EventConnectionState::Disconnected);
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            logRemoteError("sse", "curl_easy_init failed (transport)");
            updateConnectionState(Thoth::EventConnectionState::Reconnecting);
            sleepInterruptibleMs(Thoth::sseReconnectDelayMs(reconnect_attempt_++));
            continue;
        }

        std::string buffer;
        const std::string url = base_url_ + "/v1/events";
        SseWriteContext wctx{this, &buffer, &sse_cancel_};

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Accept: text/event-stream");
        headers = curl_slist_append(headers, "Cache-Control: no-cache");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sseWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wctx);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kSseConnectTimeoutSec);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, sseProgressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &sse_cancel_);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        updateConnectionState(Thoth::EventConnectionState::Connected);
        reconnect_attempt_ = 0;

        const CURLcode code = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        if (sse_cancel_.load()) {
            curl_easy_cleanup(curl);
            break;
        }

        updateConnectionState(Thoth::EventConnectionState::Disconnected);

        if (code != CURLE_OK && code != CURLE_ABORTED_BY_CALLBACK && code != CURLE_WRITE_ERROR) {
            logRemoteError("sse", std::string("transport: ") + curl_easy_strerror(code));
        } else if (code == CURLE_OK) {
            long status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
            if (status != 0 && (status < 200 || status >= 300)) {
                logRemoteError("sse", "transport: HTTP " + std::to_string(status));
            }
        }

        curl_easy_cleanup(curl);

        updateConnectionState(Thoth::EventConnectionState::Reconnecting);
        sleepInterruptibleMs(Thoth::sseReconnectDelayMs(reconnect_attempt_++));
    }
}

Thoth::OperationResult RemoteAgentBackend::processInput(const std::string& input) {
    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            logRemoteError("processInput", ready_err);
            return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", ready_err, true);
        }

        std::string sid;
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            sid = session_id_;
        }

        const json req = buildChatRequestJson(input, sid);
        const long chat_timeout = resolveRemoteRequestTimeoutSec(kChatTimeoutSec);
        const HttpResult http = httpPostJson("/v1/chat", req.dump(), chat_timeout);
        if (!http.transport_ok) {
            const std::string msg = "[RemoteEngine] /v1/chat transport: " + http.transport_error;
            logRemoteError("processInput", msg);
            return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", msg, true);
        }
        if (http.status < 200 || http.status >= 300) {
            const std::string msg = formatHttpErrorMessage(http.status, http.body);
            logRemoteError("processInput", msg);
            return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", msg, false, http.status);
        }

        try {
            const json body = json::parse(http.body.empty() ? "{}" : http.body);
            const auto extracted = extractChatResponseText(body);
            if (!extracted.ok) {
                logRemoteError("processInput", extracted.error);
                return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", extracted.error);
            }
            if (extracted.text.empty() && !body.contains("response")) {
                logRemoteWarning("processInput", "missing semantic field \"response\"");
            }
            return Thoth::makeSuccess(Thoth::kOpChat, "Response received", extracted.text);
        } catch (const std::exception& ex) {
            const std::string msg =
                std::string("[RemoteEngine] chat response JSON parse failed: ") + ex.what();
            logRemoteError("processInput", msg);
            return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", msg);
        }
    } catch (const std::exception& ex) {
        logRemoteError("processInput", ex.what());
        const std::string msg = std::string("[RemoteEngine] processInput: ") + ex.what();
        return Thoth::makeFailure(Thoth::kOpChat, "Chat failed", msg);
    } catch (...) {
        logRemoteError("processInput", "unknown error");
        return Thoth::makeFailure(Thoth::kOpChat, "Chat failed",
                                  "[RemoteEngine] processInput: unknown error");
    }
}

Thoth::OperationResult RemoteAgentBackend::executeGoal(const std::string& goal) {
    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            logRemoteError("executeGoal", ready_err);
            return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", ready_err, true);
        }

        std::string sid;
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            sid = session_id_;
        }

        const json req = buildGoalRequestJson(goal, sid);
        const long goals_timeout = resolveRemoteRequestTimeoutSec(kGoalsTimeoutSec);
        const HttpResult http = httpPostJson("/v1/goals", req.dump(), goals_timeout);
        if (!http.transport_ok) {
            const std::string msg =
                "[RemoteEngine] /v1/goals transport: " + http.transport_error;
            logRemoteError("executeGoal", msg);
            return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", msg, true);
        }
        if (http.status < 200 || http.status >= 300) {
            const std::string msg = formatHttpErrorMessage(http.status, http.body);
            logRemoteError("executeGoal", msg);
            return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", msg, false, http.status);
        }

        if (!http.body.empty()) {
            try {
                const json body = json::parse(http.body);
                const auto check = checkGoalResponseBody(body);
                if (!check.ok) {
                    logRemoteError("executeGoal", check.error);
                    return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", check.error);
                }
                if (!check.warning.empty()) {
                    logRemoteWarning("executeGoal", check.warning);
                }
            } catch (const std::exception& ex) {
                const std::string msg =
                    std::string("goals response JSON parse failed: ") + ex.what();
                logRemoteError("executeGoal", msg);
                return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", msg);
            } catch (...) {
                logRemoteError("executeGoal", "goals response JSON parse failed");
                return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed",
                                          "goals response JSON parse failed");
            }
        }
        return Thoth::makeSuccess(Thoth::kOpGoal, "Goal accepted");
    } catch (const std::exception& ex) {
        logRemoteError("executeGoal", ex.what());
        return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", ex.what());
    } catch (...) {
        logRemoteError("executeGoal", "unknown error");
        return Thoth::makeFailure(Thoth::kOpGoal, "Goal failed", "unknown error");
    }
}

Thoth::OperationResult RemoteAgentBackend::controlPost(const char* path_suffix,
                                                     const char* op_name,
                                                     const char* operation) {
    const auto failure = [&](const std::string& detail, bool retryable = false,
                             std::optional<long> http_status = std::nullopt) {
        return Thoth::makeFailure(operation, Thoth::operationFailureDetail(operation), detail,
                                  retryable, http_status);
    };

    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            logRemoteError(op_name, ready_err);
            return failure(ready_err, true);
        }

        const std::string path = std::string("/v1/control/") + path_suffix;
        const HttpResult http = httpPostJson(path, "{}", kControlTimeoutSec);
        if (!http.transport_ok) {
            const std::string msg =
                std::string("[RemoteEngine] ") + path + " transport: " + http.transport_error;
            logRemoteError(op_name, msg);
            return failure(msg, true);
        }
        if (http.status < 200 || http.status >= 300) {
            const std::string msg = formatHttpErrorMessage(http.status, http.body);
            logRemoteError(op_name, msg);
            return failure(msg, false, http.status);
        }

        if (operation == Thoth::kOpPause) {
            return Thoth::makeSuccess(Thoth::kOpPause, "Agent execution paused");
        }
        if (operation == Thoth::kOpResume) {
            return Thoth::makeSuccess(Thoth::kOpResume, "Agent execution resumed");
        }
        if (operation == Thoth::kOpAbort) {
            return Thoth::makeSuccess(Thoth::kOpAbort, "Agent execution aborted");
        }
        return Thoth::makeSuccess(operation, "Operation succeeded");
    } catch (const std::exception& ex) {
        logRemoteError(op_name, ex.what());
        return failure(ex.what());
    } catch (...) {
        logRemoteError(op_name, "unknown error");
        return failure("unknown error");
    }
}

Thoth::OperationResult RemoteAgentBackend::pause() {
    return controlPost("pause", "pause", Thoth::kOpPause);
}
Thoth::OperationResult RemoteAgentBackend::resume() {
    return controlPost("resume", "resume", Thoth::kOpResume);
}
Thoth::OperationResult RemoteAgentBackend::abort() {
    return controlPost("abort", "abort", Thoth::kOpAbort);
}

void RemoteAgentBackend::setConversationMemory(
    const std::vector<std::pair<std::string, std::string>>&,
    const std::string&) {
    logRemoteWarning("setConversationMemory", "unavailable in remote mode (no cognate/sync API)");
}

void RemoteAgentBackend::setConversationMemory(
    const std::vector<Memory::TimedMessage>&,
    const std::string&) {
    logRemoteWarning("setConversationMemory(timed)",
                     "unavailable in remote mode (no cognate/sync API)");
}

void RemoteAgentBackend::setRagFiles(const std::vector<std::string>&) {
    logRemoteWarning("setRagFiles", "unavailable in remote mode (host paths ≠ engine workspace)");
}

void RemoteAgentBackend::checkResumablePlan() {
    logRemoteWarning("checkResumablePlan", "unavailable in remote mode");
}

nlohmann::json RemoteAgentBackend::fetchResearchCollection(const char* path) const {
    using namespace Thoth;
    try {
        std::string ready_err;
        if (!const_cast<RemoteAgentBackend*>(this)->ensureReady(ready_err)) {
            logRemoteError("fetchResearchCollection", ready_err);
            return ResearchResources::unavailableFetchResult();
        }
        const HttpResult http = httpGet(path, ThothRemoteHttp::kHealthReadyTimeoutSec);
        if (!http.transport_ok) {
            logRemoteError("fetchResearchCollection",
                           std::string("[RemoteEngine] transport: ") + http.transport_error);
            return ResearchResources::unavailableFetchResult();
        }
        if (http.status < 200 || http.status >= 300) {
            logRemoteError("fetchResearchCollection", formatHttpErrorMessage(http.status, http.body));
            return ResearchResources::unavailableFetchResult();
        }
        try {
            const json body = json::parse(http.body);
            std::string err;
            if (!ResearchResources::hasRequiredCollectionFields(body, err)) {
                logRemoteError("fetchResearchCollection", err);
                return ResearchResources::unavailableFetchResult();
            }
            return body;
        } catch (const std::exception& ex) {
            logRemoteError("fetchResearchCollection",
                           std::string("JSON parse failed: ") + ex.what());
            return ResearchResources::unavailableFetchResult();
        }
    } catch (const std::exception& ex) {
        logRemoteError("fetchResearchCollection", ex.what());
        return ResearchResources::unavailableFetchResult();
    } catch (...) {
        logRemoteError("fetchResearchCollection", "unknown error");
        return ResearchResources::unavailableFetchResult();
    }
}

nlohmann::json RemoteAgentBackend::fetchGraphStatisticsResource() const {
    using namespace Thoth;
    try {
        std::string ready_err;
        if (!const_cast<RemoteAgentBackend*>(this)->ensureReady(ready_err)) {
            logRemoteError("fetchGraphStatisticsResource", ready_err);
            return GraphStatistics::unavailableFetchResult();
        }
        const HttpResult http =
            httpGet(GraphStatistics::kHttpPath, ThothRemoteHttp::kHealthReadyTimeoutSec);
        if (!http.transport_ok) {
            logRemoteError("fetchGraphStatisticsResource",
                           std::string("[RemoteEngine] transport: ") + http.transport_error);
            return GraphStatistics::unavailableFetchResult();
        }
        if (http.status < 200 || http.status >= 300) {
            logRemoteError("fetchGraphStatisticsResource",
                           formatHttpErrorMessage(http.status, http.body));
            return GraphStatistics::unavailableFetchResult();
        }
        try {
            const json body = json::parse(http.body);
            std::string err;
            if (!GraphStatistics::hasRequiredFields(body, err)) {
                logRemoteError("fetchGraphStatisticsResource", err);
                return GraphStatistics::unavailableFetchResult();
            }
            return body;
        } catch (const std::exception& ex) {
            logRemoteError("fetchGraphStatisticsResource",
                           std::string("JSON parse failed: ") + ex.what());
            return GraphStatistics::unavailableFetchResult();
        }
    } catch (const std::exception& ex) {
        logRemoteError("fetchGraphStatisticsResource", ex.what());
        return GraphStatistics::unavailableFetchResult();
    } catch (...) {
        logRemoteError("fetchGraphStatisticsResource", "unknown error");
        return GraphStatistics::unavailableFetchResult();
    }
}

nlohmann::json RemoteAgentBackend::getStrategies() const {
    return fetchResearchCollection(Thoth::ResearchResources::kHttpPathStrategies);
}

nlohmann::json RemoteAgentBackend::getTrajectories() const {
    return fetchResearchCollection(Thoth::ResearchResources::kHttpPathTrajectories);
}

nlohmann::json RemoteAgentBackend::getEpisodes() const {
    return fetchResearchCollection(Thoth::ResearchResources::kHttpPathEpisodes);
}
nlohmann::json RemoteAgentBackend::getExperiments() const { return json::array(); }
nlohmann::json RemoteAgentBackend::getGraphStats() const {
    return fetchGraphStatisticsResource();
}

bool RemoteAgentBackend::saveExperiment(const nlohmann::json&) {
    logRemoteWarning("saveExperiment", "unavailable in remote mode");
    return false;
}

nlohmann::json RemoteAgentBackend::getLatestDecisionSummary() const {
    try {
        std::string ready_err;
        if (!const_cast<RemoteAgentBackend*>(this)->ensureReady(ready_err)) {
            logRemoteError("getLatestDecisionSummary", ready_err);
            return Thoth::DecisionSummary::emptyV1Summary();
        }
        const HttpResult http = httpGet(Thoth::DecisionSummary::kHttpPath,
                                        ThothRemoteHttp::kHealthReadyTimeoutSec);
        if (!http.transport_ok) {
            logRemoteError("getLatestDecisionSummary",
                           "[RemoteEngine] diagnostics transport: " + http.transport_error);
            return Thoth::DecisionSummary::emptyV1Summary();
        }
        if (http.status < 200 || http.status >= 300) {
            logRemoteError("getLatestDecisionSummary",
                           formatHttpErrorMessage(http.status, http.body));
            return Thoth::DecisionSummary::emptyV1Summary();
        }
        try {
            auto body = json::parse(http.body);
            std::string err;
            if (!Thoth::DecisionSummary::hasRequiredV1Fields(body, err)) {
                logRemoteError("getLatestDecisionSummary", err);
                return Thoth::DecisionSummary::emptyV1Summary();
            }
            return body;
        } catch (const std::exception& ex) {
            logRemoteError("getLatestDecisionSummary",
                           std::string("JSON parse failed: ") + ex.what());
            return Thoth::DecisionSummary::emptyV1Summary();
        }
    } catch (const std::exception& ex) {
        logRemoteError("getLatestDecisionSummary", ex.what());
        return Thoth::DecisionSummary::emptyV1Summary();
    } catch (...) {
        logRemoteError("getLatestDecisionSummary", "unknown error");
        return Thoth::DecisionSummary::emptyV1Summary();
    }
}

nlohmann::json RemoteAgentBackend::listCorpusDocuments() const {
    try {
        std::string ready_err;
        if (!const_cast<RemoteAgentBackend*>(this)->ensureReady(ready_err)) {
            logRemoteError("listCorpusDocuments", ready_err);
            return Thoth::CorpusDocuments::unavailableFetchResult();
        }
        const HttpResult http = httpGet(Thoth::CorpusDocuments::kHttpPath,
                                        ThothRemoteHttp::kHealthReadyTimeoutSec);
        if (!http.transport_ok) {
            logRemoteError("listCorpusDocuments",
                           "[RemoteEngine] corpus transport: " + http.transport_error);
            return Thoth::CorpusDocuments::unavailableFetchResult();
        }
        if (http.status < 200 || http.status >= 300) {
            logRemoteError("listCorpusDocuments",
                           formatHttpErrorMessage(http.status, http.body));
            return Thoth::CorpusDocuments::unavailableFetchResult();
        }
        try {
            auto body = json::parse(http.body);
            std::string err;
            if (!Thoth::CorpusDocuments::hasRequiredV1Fields(body, err)) {
                logRemoteError("listCorpusDocuments", err);
                return Thoth::CorpusDocuments::unavailableFetchResult();
            }
            return body;
        } catch (const std::exception& ex) {
            logRemoteError("listCorpusDocuments",
                           std::string("JSON parse failed: ") + ex.what());
            return Thoth::CorpusDocuments::unavailableFetchResult();
        }
    } catch (const std::exception& ex) {
        logRemoteError("listCorpusDocuments", ex.what());
        return Thoth::CorpusDocuments::unavailableFetchResult();
    } catch (...) {
        logRemoteError("listCorpusDocuments", "unknown error");
        return Thoth::CorpusDocuments::unavailableFetchResult();
    }
}

Thoth::BackendCapabilities RemoteAgentBackend::capabilities() const {
    std::string err;
    const_cast<RemoteAgentBackend*>(this)->ensureReady(err);
    Thoth::BackendCapabilities caps = Thoth::engineBackendCapabilities();
    std::lock_guard<std::mutex> lock(ready_mutex_);
    caps.supportsIngest = ingest_allowed_;
    caps.supportsConversation = conversation_allowed_;
    caps.supportsStrategies = strategies_allowed_;
    caps.supportsTrajectories = trajectories_allowed_;
    caps.supportsEpisodes = episodes_allowed_;
    caps.supportsGraphStats = graph_stats_allowed_;
    return caps;
}

Thoth::OperationResult RemoteAgentBackend::queryCorpusDocumentIntent(
    const std::string& sourceFilePath) {
    return postCorpusDocumentRequest(sourceFilePath, {}, true);
}

Thoth::OperationResult RemoteAgentBackend::createCorpusDocument(
    const std::string& sourceFilePath,
    const Thoth::CorpusCreateGuiOptions& options) {
    return postCorpusDocumentRequest(sourceFilePath, options, false);
}

Thoth::OperationResult RemoteAgentBackend::postCorpusDocumentRequest(
    const std::string& sourceFilePath,
    const Thoth::CorpusCreateGuiOptions& options,
    bool dry_run) {
    using namespace Thoth;
    using namespace Thoth::CorpusCreateLocal;
    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            return makeFailure(dry_run ? kOpQueryDocumentIntent : CorpusCreate::kOperationName,
                               dry_run ? "Could not query send intent"
                                       : "Document could not be sent to Engine",
                               ready_err,
                               true);
        }
        {
            std::lock_guard<std::mutex> lock(ready_mutex_);
            if (!ingest_allowed_) {
                return makeFailure(dry_run ? kOpQueryDocumentIntent : CorpusCreate::kOperationName,
                                   dry_run ? "Could not query send intent"
                                           : "Document ingest unavailable with the current Engine",
                                   "ingest capability missing from /ready");
            }
        }
        std::string read_err;
        const auto payload = readLocalNoteFile(sourceFilePath, read_err);
        if (!payload) {
            return makeFailure(dry_run ? kOpQueryDocumentIntent : CorpusCreate::kOperationName,
                               dry_run ? "Could not query send intent"
                                       : (read_err == "Local Note file not found"
                                              ? "Local Note file not found"
                                              : "Document could not be sent to Engine"),
                               read_err.empty() ? sourceFilePath : read_err);
        }
        std::string backend_session_id;
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            backend_session_id = session_id_;
        }
        const json req = CorpusCreate::makeCreateDocumentRequestBodyAlp(
            makeCreateRequest(*payload, backend_session_id, sourceFilePath, options, dry_run));
        const HttpResult http = httpPostJson(CorpusCreate::kHttpPath,
                                             req.dump(),
                                             ThothRemoteHttp::kControlTimeoutSec);
        if (!http.transport_ok) {
            return makeFailure(dry_run ? kOpQueryDocumentIntent : CorpusCreate::kOperationName,
                               dry_run ? "Could not query send intent"
                                       : "Document could not be sent to Engine",
                               "[RemoteEngine] create transport: " + http.transport_error,
                               true);
        }
        if (http.status < 200 || http.status >= 300) {
            const std::string machine_code = parseHttpErrorMachineCode(http.body);
            auto result = makeFailure(
                dry_run ? kOpQueryDocumentIntent : CorpusCreate::kOperationName,
                dry_run ? "Could not query send intent" : "Document could not be sent to Engine",
                formatHttpErrorMessage(http.status, http.body),
                http.status >= 500,
                http.status);
            result.ingest_host_path = sourceFilePath;
            if (!machine_code.empty()) {
                result.ingest_machine_code = machine_code;
                if (machine_code == "content_conflict") {
                    result.ingest_content_conflict = true;
                }
            }
            return result;
        }
        json body;
        try {
            body = json::parse(http.body);
        } catch (const std::exception& ex) {
            return makeFailure(dry_run ? kOpQueryDocumentIntent : CorpusCreate::kOperationName,
                               dry_run ? "Could not query send intent"
                                       : "Document acceptance response invalid",
                               ex.what());
        }
        return operationResultFromJsonBody(body, sourceFilePath, dry_run);
    } catch (const std::exception& ex) {
        logRemoteError(dry_run ? "queryCorpusDocumentIntent" : "createCorpusDocument", ex.what());
        return makeFailure(dry_run ? kOpQueryDocumentIntent : CorpusCreate::kOperationName,
                           dry_run ? "Could not query send intent"
                                   : "Document could not be sent to Engine",
                           ex.what());
    } catch (...) {
        logRemoteError(dry_run ? "queryCorpusDocumentIntent" : "createCorpusDocument",
                       "unknown error");
        return makeFailure(dry_run ? kOpQueryDocumentIntent : CorpusCreate::kOperationName,
                           dry_run ? "Could not query send intent"
                                   : "Document could not be sent to Engine");
    }
}

nlohmann::json RemoteAgentBackend::createConversationSession() {
    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            logRemoteError("createConversationSession", ready_err);
            return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
        }
        {
            std::lock_guard<std::mutex> lock(ready_mutex_);
            if (!conversation_allowed_) {
                logRemoteError("createConversationSession",
                               "conversation capability missing from /ready");
                return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
            }
        }
        const HttpResult http = httpPostJson(Thoth::ConversationAuthority::kHttpPathSessions,
                                             "{}",
                                             ThothRemoteHttp::kControlTimeoutSec);
        if (!http.transport_ok || http.status < 200 || http.status >= 300) {
            logRemoteError("createConversationSession",
                           http.transport_ok ? formatHttpErrorMessage(http.status, http.body)
                                             : http.transport_error);
            return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
        }
        const json body = json::parse(http.body);
        std::string err;
        if (!Thoth::ConversationAuthority::hasRequiredCreateSessionFields(body, err)) {
            logRemoteError("createConversationSession", err);
            return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
        }
        return body;
    } catch (const std::exception& ex) {
        logRemoteError("createConversationSession", ex.what());
        return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
    } catch (...) {
        logRemoteError("createConversationSession", "unknown error");
        return Thoth::ConversationAuthority::makeCreateSessionResponse("default");
    }
}

Thoth::OperationResult RemoteAgentBackend::appendConversationTurn(const std::string& session_id,
                                                                  const std::string& content) {
    using namespace Thoth;
    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            return makeFailure(kOpChat, "Failed to send", ready_err, true);
        }
        {
            std::lock_guard<std::mutex> lock(ready_mutex_);
            if (!conversation_allowed_) {
                return makeFailure(kOpChat,
                                   "Failed to send",
                                   "conversation capability missing from /ready");
            }
        }
        if (session_id.empty() || content.empty()) {
            return makeFailure(kOpChat, "Failed to send", "session_id and content required");
        }
        const json req = {{"session_id", session_id}, {"content", content}};
        const long chat_timeout = resolveRemoteRequestTimeoutSec(kChatTimeoutSec);
        const HttpResult http = httpPostJson(ConversationAuthority::kHttpPathTurns,
                                             req.dump(),
                                             chat_timeout);
        if (!http.transport_ok) {
            return makeFailure(kOpChat,
                               "Failed to send",
                               "[RemoteEngine] turn transport: " + http.transport_error,
                               true);
        }
        if (http.status < 200 || http.status >= 300) {
            return makeFailure(kOpChat,
                               "Failed to send",
                               formatHttpErrorMessage(http.status, http.body),
                               http.status >= 500,
                               http.status);
        }
        json body;
        try {
            body = json::parse(http.body);
        } catch (const std::exception& ex) {
            return makeFailure(kOpChat, "Failed to send", ex.what());
        }
        std::string err;
        if (!ConversationAuthority::hasRequiredAppendTurnFields(body, err)) {
            return makeFailure(kOpChat, "Failed to send", err);
        }
        const std::string assistant = body["assistant"]["content"].get<std::string>();
        return makeSuccess(kOpChat, "Response received", assistant);
    } catch (const std::exception& ex) {
        logRemoteError("appendConversationTurn", ex.what());
        return makeFailure(kOpChat, "Failed to send", ex.what());
    } catch (...) {
        logRemoteError("appendConversationTurn", "unknown error");
        return makeFailure(kOpChat, "Failed to send");
    }
}

nlohmann::json RemoteAgentBackend::getConversation(const std::string& session_id) const {
    try {
        std::string ready_err;
        if (!const_cast<RemoteAgentBackend*>(this)->ensureReady(ready_err)) {
            return Thoth::ConversationAuthority::emptyConversation(session_id);
        }
        const std::string path =
            std::string(Thoth::ConversationAuthority::kHttpPathSessions) + "/" + session_id;
        const HttpResult http = httpGet(path, ThothRemoteHttp::kHealthReadyTimeoutSec);
        if (!http.transport_ok || http.status < 200 || http.status >= 300) {
            return Thoth::ConversationAuthority::emptyConversation(session_id);
        }
        json body = json::parse(http.body);
        std::string err;
        if (!Thoth::ConversationAuthority::hasRequiredConversationFields(body, err)) {
            logRemoteError("getConversation", err);
            return Thoth::ConversationAuthority::emptyConversation(session_id);
        }
        return body;
    } catch (...) {
        return Thoth::ConversationAuthority::emptyConversation(session_id);
    }
}

nlohmann::json RemoteAgentBackend::getConversationSummary(const std::string& session_id) const {
    try {
        std::string ready_err;
        if (!const_cast<RemoteAgentBackend*>(this)->ensureReady(ready_err)) {
            return Thoth::ConversationAuthority::makeSummaryResponse(session_id, "");
        }
        const std::string path = std::string(Thoth::ConversationAuthority::kHttpPathSessions) + "/"
                                 + session_id + "/summary";
        const HttpResult http = httpGet(path, ThothRemoteHttp::kHealthReadyTimeoutSec);
        if (!http.transport_ok || http.status < 200 || http.status >= 300) {
            return Thoth::ConversationAuthority::makeSummaryResponse(session_id, "");
        }
        json body = json::parse(http.body);
        std::string err;
        if (!Thoth::ConversationAuthority::hasRequiredSummaryFields(body, err)) {
            logRemoteError("getConversationSummary", err);
            return Thoth::ConversationAuthority::makeSummaryResponse(session_id, "");
        }
        return body;
    } catch (...) {
        return Thoth::ConversationAuthority::makeSummaryResponse(session_id, "");
    }
}
