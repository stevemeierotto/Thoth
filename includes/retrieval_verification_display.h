/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Phase R5 retrieval verification display (pure; no wx)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_RETRIEVAL_VERIFICATION_DISPLAY_H
#define THOTH_RETRIEVAL_VERIFICATION_DISPLAY_H

#include "json.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace Thoth {
namespace RetrievalVerificationDisplay {

/** R5-G5 — strict tab match when Engine sets session_id; legacy empty id is permissive. */
inline bool retrievalDiagnosticsTargetsSession(const std::string& event_session_id,
                                               const std::string& active_tab_session_id) {
    if (event_session_id.empty()) {
        return true;
    }
    return event_session_id == active_tab_session_id;
}

inline nlohmann::json extractDiagnosticsPayload(const nlohmann::json& metadata) {
    try {
        if (!metadata.is_object()) {
            return metadata;
        }
        if (metadata.contains("diagnostics") && metadata["diagnostics"].is_object()) {
            return metadata["diagnostics"];
        }
        if (metadata.contains("result") && metadata["result"].is_object()) {
            const auto& result = metadata["result"];
            if (result.contains("diagnostics") && result["diagnostics"].is_object()) {
                return result["diagnostics"];
            }
        }
    } catch (...) {
    }
    return metadata;
}

inline nlohmann::json embeddedRetrievalTrace(const nlohmann::json& diagnostics) {
    if (!diagnostics.is_object()) {
        return nlohmann::json::object();
    }
    if (diagnostics.contains("retrieval_trace") && diagnostics["retrieval_trace"].is_object()) {
        return diagnostics["retrieval_trace"];
    }
    return nlohmann::json::object();
}

inline nlohmann::json scopeFromDiagnostics(const nlohmann::json& diagnostics) {
    const nlohmann::json trace = embeddedRetrievalTrace(diagnostics);
    if (trace.contains("retrieval_scope") && trace["retrieval_scope"].is_object()) {
        return trace["retrieval_scope"];
    }
    return nlohmann::json::object();
}

inline std::string formatStringList(const nlohmann::json& arr, std::size_t max_items = 4) {
    if (!arr.is_array() || arr.empty()) {
        return "none";
    }
    std::ostringstream out;
    std::size_t n = 0;
    for (const auto& item : arr) {
        if (!item.is_string()) {
            continue;
        }
        if (n > 0) {
            out << ", ";
        }
        out << item.get<std::string>();
        ++n;
        if (n >= max_items) {
            if (arr.size() > max_items) {
                out << ", …";
            }
            break;
        }
    }
    const std::string s = out.str();
    return s.empty() ? "none" : s;
}

/** R5-G1 — Scope layer summary from retrieval_trace.retrieval_scope only. */
inline std::string formatScopeLayerSummary(const nlohmann::json& retrieval_scope) {
    if (!retrieval_scope.is_object() || retrieval_scope.empty()) {
        return "Scope: retrieval skipped (no retrieval_trace scope)";
    }
    std::ostringstream out;
    out << "Scope layer — active_context_key="
        << retrieval_scope.value("active_context_key", "?");
    out << " · tiers=" << formatStringList(retrieval_scope.value("allowed_tiers", nlohmann::json::array()));
    out << " · selected_docs="
        << formatStringList(retrieval_scope.value("selected_documents", nlohmann::json::array()));
    return out.str();
}

/** R5-G3 — Grounded layer from trace.grounding / CHAT_RAG_CONTEXT-shaped fields only. */
inline std::string formatGroundedLayerSummary(const nlohmann::json& grounding) {
    if (!grounding.is_object() || grounding.empty()) {
        return "Grounded layer — pending (authoritative after injection gate; not from candidates)";
    }
    const bool grounded = grounding.value("grounded", false);
    const std::string mode = grounding.value("grounding_mode", "unknown");
    std::ostringstream out;
    out << "Grounded layer — grounded=" << (grounded ? "true" : "false") << " · mode=" << mode;
    if (grounding.contains("documents") && grounding["documents"].is_array()) {
        out << " · docs=" << formatStringList(grounding["documents"]);
    } else if (grounding.contains("document_files") && grounding["document_files"].is_array()) {
        out << " · docs=" << formatStringList(grounding["document_files"]);
    }
    return out.str();
}

inline std::string formatRequestIdLine(const nlohmann::json& metadata,
                                       const nlohmann::json& diagnostics) {
    if (metadata.is_object() && metadata.contains("request_id")
        && metadata["request_id"].is_string()) {
        return std::string("request_id: ") + metadata["request_id"].get<std::string>();
    }
    const nlohmann::json trace = embeddedRetrievalTrace(diagnostics);
    if (trace.contains("request_id") && trace["request_id"].is_string()) {
        return std::string("request_id: ") + trace["request_id"].get<std::string>();
    }
    return "request_id: —";
}

inline bool isRetrievalSkippedPath(const nlohmann::json& diagnostics) {
    if (!diagnostics.is_object()) {
        return true;
    }
    const std::string scoring = diagnostics.value("scoring_type", "");
    if (scoring == "no_index" || scoring == "greeting_skip") {
        return true;
    }
    const nlohmann::json trace = embeddedRetrievalTrace(diagnostics);
    return trace.empty() || !trace.contains("retrieval_scope");
}

inline std::string formatRetrievalSkippedLabel(const nlohmann::json& diagnostics) {
    const std::string scoring = diagnostics.value("scoring_type", "");
    if (scoring == "no_index") {
        return "Retrieval skipped — empty index (no retrieval_trace scope)";
    }
    if (scoring == "greeting_skip") {
        return "Retrieval skipped — greeting path (no retrieval_trace scope)";
    }
    if (isRetrievalSkippedPath(diagnostics)) {
        return "Retrieval skipped — no retrieval_trace scope";
    }
    return {};
}

inline bool breakdownsIncludeWarmMemory(const nlohmann::json& diagnostics) {
    if (!diagnostics.contains("breakdowns") || !diagnostics["breakdowns"].is_array()) {
        return false;
    }
    for (const auto& row : diagnostics["breakdowns"]) {
        if (!row.is_object()) {
            continue;
        }
        const std::string file = row.value("file_name", row.value("file", ""));
        if (file.rfind("warm_memory:", 0) == 0) {
            return true;
        }
    }
    return false;
}

inline constexpr const char* kWarmMemoryFootnote =
    "Note: warm_memory:* rows are prompt-tier candidates, not scope-selected attachments.";

inline constexpr const char* kCandidateLayerTitle =
    "Candidate layer (pre-grounding scores — not injected):";

inline constexpr const char* kInventoryLayerHint =
    "Inventory layer — unscoped Engine document list (not active Agent Context).";

} // namespace RetrievalVerificationDisplay
} // namespace Thoth

#endif // THOTH_RETRIEVAL_VERIFICATION_DISPLAY_H
