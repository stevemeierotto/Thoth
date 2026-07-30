/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — GUI Phase 7 operation result honesty (pure; no wx)
 *
 * Transport-agnostic outcome for user-initiated Engine operations.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_OPERATION_RESULT_H
#define THOTH_OPERATION_RESULT_H

#include "engine_connection_state.h"

#include <optional>
#include <string>

namespace Thoth {

/** ALP-E — GUI options for POST /v1/rag/documents (consumes frozen ALP-C contract). */
struct CorpusCreateGuiOptions {
    bool force_replace = false;
};

/** Where the GUI should surface a failed operation (Phase 7 policy). */
enum class OperationUiSeverity {
    StatusBar,
    Panel,
    Modal
};

/** Structured backend outcome — not wire-specific. */
struct OperationResult {
    bool success = false;
    std::string operation;
    std::string user_message;
    std::string technical_details;
    bool retryable = false;
    std::optional<long> http_status;
    /** Chat success payload; empty for control/goal ops. */
    std::string response_text;
    /** Set on successful create_document — host Local Note path sent. */
    std::optional<std::string> ingest_host_path;
    /** Set on successful create_document — Engine-assigned document id. */
    std::optional<std::string> ingest_document_id;
    /** Set on successful create_document — Engine-side document name. */
    std::optional<std::string> ingest_document_name;
    /** ALP-E — accepted revision id (schema_version >= 2). */
    std::optional<std::string> ingest_revision_id;
    /** ALP-E — dry_run or accept action string from Engine. */
    std::optional<std::string> ingest_action;
    /** ALP-E — policy reason from dry_run or error path. */
    std::optional<std::string> ingest_reason;
    /** ALP-E — Engine error machine_code when present (e.g. content_conflict). */
    std::optional<std::string> ingest_machine_code;
    /** ALP-E — true when Engine returned 409 content_conflict. */
    bool ingest_content_conflict = false;
};

inline OperationResult makeSuccess(const std::string& operation,
                                   const std::string& user_message,
                                   const std::string& response_text = "") {
    OperationResult r;
    r.success = true;
    r.operation = operation;
    r.user_message = user_message;
    r.response_text = response_text;
    return r;
}

inline OperationResult makeFailure(const std::string& operation,
                                   const std::string& user_message,
                                   const std::string& technical_details = {},
                                   bool retryable = false,
                                   std::optional<long> http_status = std::nullopt) {
    OperationResult r;
    r.success = false;
    r.operation = operation;
    r.user_message = user_message;
    r.technical_details = technical_details;
    r.retryable = retryable;
    r.http_status = http_status;
    return r;
}

inline std::string operationFailureDetail(const std::string& operation) {
    if (operation == "abort") {
        return "Abort request could not be delivered";
    }
    if (operation == "pause") {
        return "Pause request could not be delivered";
    }
    if (operation == "resume") {
        return "Resume request could not be delivered";
    }
    if (operation == "goal") {
        return "Goal request could not be delivered";
    }
    if (operation == "chat") {
        return "Message could not be delivered";
    }
    if (operation == "create_document") {
        return "Document could not be sent to Engine";
    }
    return operation + " request could not be delivered";
}

inline bool engineUnavailableForOperations(const EventStreamSnapshot& snap) {
    if (!snap.applies) {
        return false;
    }
    if (!engineHttpUsable(snap.engine)) {
        return true;
    }
    return snap.connection != EventConnectionState::Connected;
}

inline std::string connectionRootCauseLabel(const EventStreamSnapshot& snap) {
    if (!snap.applies) {
        return {};
    }
    if (!engineHttpUsable(snap.engine)) {
        if (snap.engine == EngineHealthState::Starting) {
            return "Engine starting";
        }
        return "Engine unavailable";
    }
    if (snap.connection == EventConnectionState::Reconnecting) {
        return "Engine unavailable";
    }
    if (snap.connection == EventConnectionState::Disconnected
        || snap.connection == EventConnectionState::Failed) {
        return "Engine unavailable";
    }
    return {};
}

/** Phase 6 correlation — root cause once; operation adds detail only. */
inline std::string formatCorrelatedUserMessage(const EventStreamSnapshot& snap,
                                               const OperationResult& result) {
    if (result.success) {
        return result.user_message;
    }
    const std::string root = connectionRootCauseLabel(snap);
    std::string detail = result.user_message;
    if (detail.empty()) {
        detail = operationFailureDetail(result.operation);
    }
    if (root.empty()) {
        return detail;
    }
    if (detail.find(root) != std::string::npos) {
        return detail;
    }
    return root + " — " + detail;
}

inline OperationUiSeverity uiSeverityForFailure(const OperationResult& result,
                                                const EventStreamSnapshot& snap) {
    if (result.success) {
        return OperationUiSeverity::StatusBar;
    }
    if (engineUnavailableForOperations(snap)) {
        return OperationUiSeverity::StatusBar;
    }
    if (result.operation == "chat" || result.operation == "goal") {
        return OperationUiSeverity::Panel;
    }
    return OperationUiSeverity::StatusBar;
}

inline constexpr const char* kOpChat = "chat";
inline constexpr const char* kOpGoal = "goal";
inline constexpr const char* kOpPause = "pause";
inline constexpr const char* kOpResume = "resume";
inline constexpr const char* kOpAbort = "abort";
inline constexpr const char* kOpCreateDocument = "create_document";
inline constexpr const char* kOpQueryDocumentIntent = "query_document_intent";

/** R4-G1 — Engine-mode Phase 10 chat failures must not use the silent Panel+isChat path. */
inline bool engineConversationChatFailureNeedsStatusBar(const OperationResult& result,
                                                          bool supportsConversation) {
    return supportsConversation && !result.success && result.operation == kOpChat;
}

} // namespace Thoth

#endif // THOTH_OPERATION_RESULT_H
