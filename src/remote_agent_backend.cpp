/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan K: RemoteAgentBackend implementation (HTTP + SSE)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "remote_agent_backend.h"

#include <curl/curl.h>

#include <iostream>

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
    : base_url_(normalizeBaseUrl(std::move(base_url))) {}

RemoteAgentBackend::~RemoteAgentBackend() {
    stopSse();
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
        session_id_ = sessionId.empty() ? "default" : sessionId;
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
    // 1. cancel → 2. abort curl (via progress/write) → 3. exit loop → 4. join → 5. destroy
    sse_cancel_.store(true);
    if (sse_thread_.joinable()) {
        sse_thread_.join();
    }
}

void RemoteAgentBackend::sseLoop() {
    CURL* curl = nullptr;
    std::string buffer;
    try {
        curl = curl_easy_init();
        if (!curl) {
            logRemoteError("sse", "curl_easy_init failed (transport)");
            return;
        }

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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // until cancel
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, sseProgressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &sse_cancel_);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        const CURLcode code = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        if (sse_cancel_.load()) {
            // clean shutdown
        } else if (code != CURLE_OK && code != CURLE_ABORTED_BY_CALLBACK
                   && code != CURLE_WRITE_ERROR) {
            logRemoteError("sse", std::string("transport: ") + curl_easy_strerror(code));
        } else if (code == CURLE_OK) {
            long status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
            if (status != 0 && (status < 200 || status >= 300)) {
                logRemoteError("sse", "transport: HTTP " + std::to_string(status));
            }
        }
    } catch (const std::exception& ex) {
        logRemoteError("sse", std::string("transport/loop: ") + ex.what());
    } catch (...) {
        logRemoteError("sse", "transport/loop: unknown");
    }

    if (curl) {
        curl_easy_cleanup(curl);
    }
}

std::optional<std::string> RemoteAgentBackend::processInput(const std::string& input) {
    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            logRemoteError("processInput", ready_err);
            return ready_err;
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
            return msg;
        }
        if (http.status < 200 || http.status >= 300) {
            const std::string msg = formatHttpErrorMessage(http.status, http.body);
            logRemoteError("processInput", msg);
            return msg;
        }

        try {
            const json body = json::parse(http.body.empty() ? "{}" : http.body);
            const auto extracted = extractChatResponseText(body);
            if (!extracted.ok) {
                logRemoteError("processInput", extracted.error);
                return extracted.error;
            }
            if (extracted.text.empty() && !body.contains("response")) {
                logRemoteWarning("processInput", "missing semantic field \"response\"");
            }
            return extracted.text;
        } catch (const std::exception& ex) {
            const std::string msg =
                std::string("[RemoteEngine] chat response JSON parse failed: ") + ex.what();
            logRemoteError("processInput", msg);
            return msg;
        }
    } catch (const std::exception& ex) {
        logRemoteError("processInput", ex.what());
        return std::string("[RemoteEngine] processInput: ") + ex.what();
    } catch (...) {
        logRemoteError("processInput", "unknown error");
        return std::string("[RemoteEngine] processInput: unknown error");
    }
}

void RemoteAgentBackend::executeGoal(const std::string& goal) {
    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            logRemoteError("executeGoal", ready_err);
            return;
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
            logRemoteError("executeGoal",
                           "[RemoteEngine] /v1/goals transport: " + http.transport_error);
            return;
        }
        if (http.status < 200 || http.status >= 300) {
            logRemoteError("executeGoal", formatHttpErrorMessage(http.status, http.body));
            return;
        }

        if (!http.body.empty()) {
            try {
                const json body = json::parse(http.body);
                const auto check = checkGoalResponseBody(body);
                if (!check.ok) {
                    logRemoteError("executeGoal", check.error);
                } else if (!check.warning.empty()) {
                    logRemoteWarning("executeGoal", check.warning);
                }
            } catch (const std::exception& ex) {
                logRemoteError("executeGoal",
                               std::string("goals response JSON parse failed: ") + ex.what());
            } catch (...) {
                logRemoteError("executeGoal", "goals response JSON parse failed");
            }
        }
    } catch (const std::exception& ex) {
        logRemoteError("executeGoal", ex.what());
    } catch (...) {
        logRemoteError("executeGoal", "unknown error");
    }
}

void RemoteAgentBackend::controlPost(const char* path_suffix, const char* op_name) {
    try {
        std::string ready_err;
        if (!ensureReady(ready_err)) {
            logRemoteError(op_name, ready_err);
            return;
        }

        const std::string path = std::string("/v1/control/") + path_suffix;
        const HttpResult http = httpPostJson(path, "{}", kControlTimeoutSec);
        if (!http.transport_ok) {
            logRemoteError(op_name, std::string("[RemoteEngine] ") + path + " transport: "
                                        + http.transport_error);
            return;
        }
        if (http.status < 200 || http.status >= 300) {
            logRemoteError(op_name, formatHttpErrorMessage(http.status, http.body));
            return;
        }
    } catch (const std::exception& ex) {
        logRemoteError(op_name, ex.what());
    } catch (...) {
        logRemoteError(op_name, "unknown error");
    }
}

void RemoteAgentBackend::pause() { controlPost("pause", "pause"); }
void RemoteAgentBackend::resume() { controlPost("resume", "resume"); }
void RemoteAgentBackend::abort() { controlPost("abort", "abort"); }

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

nlohmann::json RemoteAgentBackend::getStrategies() const { return json::array(); }
nlohmann::json RemoteAgentBackend::getTrajectories() const { return json::array(); }
nlohmann::json RemoteAgentBackend::getEpisodeSteps() const { return json::array(); }
nlohmann::json RemoteAgentBackend::getExperiments() const { return json::array(); }
nlohmann::json RemoteAgentBackend::getGraphStats() const { return json::object(); }

bool RemoteAgentBackend::saveExperiment(const nlohmann::json&) {
    logRemoteWarning("saveExperiment", "unavailable in remote mode");
    return false;
}
