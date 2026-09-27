/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Local Note ↔ Engine corpus status sync (pure; no wx)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_LOCAL_NOTE_ENGINE_SYNC_H
#define THOTH_LOCAL_NOTE_ENGINE_SYNC_H

#include "ChatSessionTypes.h"
#include "corpus_documents.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Thoth {
namespace LocalNoteEngineSync {

/** ALP-E — Send gating state machine (non-authoritative GUI mirror). */
enum class LocalNoteReconcileState {
    /** ALP GUI off, or reconcile pass finished (may include per-slot failures). */
    Ready = 0,
    /** ALP GUI on; first reconcile not yet finished. */
    Unknown,
    /** Engine unavailable — Send disabled; cache display-only. */
    Unverified,
};

inline bool reconcileAllowsSend(LocalNoteReconcileState state, bool alp_gui) {
    if (!alp_gui) {
        return true;
    }
    return state == LocalNoteReconcileState::Ready;
}

/**
 * One corpus-authority refresh when HTTP usability returns after an unavailable
 * listing. False on later polls while usability stays true, so recovery is not
 * a 1-second poll.
 */
inline bool shouldRefreshCorpusOnUsabilityReturn(bool engine_was_usable,
                                                  bool engine_now_usable,
                                                  bool corpus_authority_unavailable) {
    return corpus_authority_unavailable && engine_now_usable && !engine_was_usable;
}

/** No Engine intent query ran (ingest unavailable). Reconcile is not complete. */
inline LocalNoteReconcileState reconcileStateWithoutIntentQuery() {
    return LocalNoteReconcileState::Unverified;
}

inline bool sendToEngineEnabled(bool can_ingest,
                                bool engine_usable,
                                bool has_picker_candidate,
                                LocalNoteReconcileState state,
                                bool alp_gui) {
    return can_ingest && engine_usable && has_picker_candidate
           && reconcileAllowsSend(state, alp_gui);
}

/**
 * "All notes already sent" only after a reconcile pass that actually queried
 * Engine intent. An empty intent list from a skipped query is not that result.
 */
inline bool authoritativeAllNotesAlreadySent(LocalNoteReconcileState state,
                                              bool has_rag_files,
                                              bool has_picker_candidates,
                                              bool authoritative_intent_query) {
    if (!authoritative_intent_query) {
        return false;
    }
    return has_rag_files && !has_picker_candidates && reconcileAllowsSend(state, true);
}

/** Per-intent HTTP budget aligns with ThothRemoteHttp::kControlTimeoutSec (30s). */
inline constexpr int kReconcilePerIntentTimeoutMs = 30000;
/** Total startup reconcile budget — partial failures must not block GUI forever. */
inline constexpr int kReconcileTotalBudgetMs = 90000;

/** ALP-E — INDEXING_* event fields for host-path resolution (priority ladder). */
struct IndexingEventMetadata {
    std::string file_path;
    std::string document_id;
    std::string revision_id;
    std::string document_name;
};

/** R1.5 — SSE file_path may be engine-absolute; slots use basenames for display. */
inline std::string ragEventBasename(const std::string& file_path) {
    if (file_path.empty()) {
        return {};
    }
    return std::filesystem::path(file_path).filename().string();
}

/** ALP-E — first 8 hex chars of UUID for display. */
inline std::string formatUuidShort(const std::string& document_id) {
    if (document_id.size() <= 8) {
        return document_id;
    }
    return document_id.substr(0, 8);
}

/** ALP-E — Engine dry_run intent for one Local Note slot. */
struct LocalNoteIntent {
    std::string host_path;
    std::string canonical_name;
    std::string action;
    std::string reason;
    std::string document_id;
    std::string document_name;
    bool query_ok = false;
};

inline bool isPickerEligibleAction(const std::string& action) {
    return action == "create" || action == "new_revision" || action == "retry"
           || action == "conflict" || action == "link_only" || action == "no_op";
}

inline std::string actionPickerLabel(const std::string& action) {
    if (action == "create") {
        return "Send";
    }
    if (action == "new_revision") {
        return "Update available";
    }
    if (action == "retry") {
        return "Retry indexing";
    }
    if (action == "conflict") {
        return "Replace (confirm)";
    }
    if (action == "link_only") {
        return "Attach to chat";
    }
    if (action == "no_op") {
        return "Already on Engine — confirm";
    }
    return action;
}

/** True after Send to Engine accept (POST), not dry_run reconcile cache alone. */
inline bool localNoteEngineSlotShowsAttachedStatus(const LocalNoteEngineInfo& info) {
    return info.indexing || info.failed || !info.revision_id.empty();
}

inline int countAttachedLocalNotes(const ChatSession& session) {
    int attached = 0;
    for (const auto& path : session.ragFilePaths) {
        const auto it = session.localNoteEngine.find(path);
        if (it != session.localNoteEngine.end()
            && localNoteEngineSlotShowsAttachedStatus(it->second)) {
            ++attached;
        }
    }
    return attached;
}

/** True after Send to Engine accept (document id recorded for this host path). */
inline bool localNoteAlreadySent(const ChatSession& session,
                                 const std::string& host_path) {
    const auto it = session.localNoteEngine.find(host_path);
    return it != session.localNoteEngine.end() && !it->second.document_id.empty();
}

/** Host paths not yet sent to Engine — legacy picker (pre-ALP-E). */
inline std::vector<std::string> collectUnsentLocalNotePaths(
    const ChatSession& session) {
    std::vector<std::string> unsent;
    unsent.reserve(session.ragFilePaths.size());
    for (const auto& path : session.ragFilePaths) {
        if (!localNoteAlreadySent(session, path)) {
            unsent.push_back(path);
        }
    }
    return unsent;
}

/** ALP-E — picker candidates from Engine intent snapshot. */
inline std::vector<LocalNoteIntent> collectPickerCandidates(
    const std::vector<LocalNoteIntent>& intents) {
    std::vector<LocalNoteIntent> out;
    out.reserve(intents.size());
    for (const auto& intent : intents) {
        if (intent.query_ok && isPickerEligibleAction(intent.action)) {
            out.push_back(intent);
        }
    }
    return out;
}

inline void remapEngineCacheKeys(std::map<std::string, LocalNoteEngineInfo>& cache,
                                 const std::map<std::string, std::string>& old_to_new) {
    if (old_to_new.empty()) {
        return;
    }
    std::map<std::string, LocalNoteEngineInfo> rebuilt;
    for (const auto& entry : cache) {
        const auto it = old_to_new.find(entry.first);
        const std::string& key = it != old_to_new.end() ? it->second : entry.first;
        rebuilt[key] = entry.second;
    }
    cache = std::move(rebuilt);
}

/** ALP-E — upgrade Phase-8 hash ids in cache using D1 legacy_id_map.json. */
inline int upgradeLegacyDocumentIds(
    ChatSession& session,
    const nlohmann::json& legacy_id_map) {
    if (!legacy_id_map.is_object() || session.localNoteEngine.empty()) {
        return 0;
    }
    int upgraded = 0;
    for (auto& entry : session.localNoteEngine) {
        if (entry.second.document_id.empty()) {
            continue;
        }
        const auto it = legacy_id_map.find(entry.second.document_id);
        if (it != legacy_id_map.end() && it->is_string()) {
            const std::string uuid = it->get<std::string>();
            if (!uuid.empty() && uuid != entry.second.document_id) {
                entry.second.document_id = uuid;
                ++upgraded;
            }
        }
    }
    return upgraded;
}

inline void applyIntentToCache(LocalNoteEngineInfo& info,
                               const LocalNoteIntent& intent,
                               const std::string& content_hash) {
    if (!intent.query_ok) {
        info.reconcile_verified = false;
        return;
    }
    info.reconcile_verified = true;
    if (!intent.document_id.empty() && intent.document_id != "dry-run-preview") {
        info.document_id = intent.document_id;
    }
    if (!intent.document_name.empty()) {
        info.document_name = intent.document_name;
    }
    if (!content_hash.empty()) {
        info.content_hash = content_hash;
    }
    if (intent.action == "retry") {
        info.failed = true;
        info.indexing = false;
    } else if (intent.action == "create" || intent.action == "new_revision"
               || intent.action == "conflict") {
        if (info.document_id.empty()) {
            info.indexing = false;
            info.failed = false;
        }
    } else if (intent.action == "no_op" || intent.action == "link_only") {
        info.indexing = false;
        info.failed = false;
    }
}

/** Apply Engine corpus document status to a tracked Local Note binding. */
inline bool applyCorpusDocumentStatus(LocalNoteEngineInfo& info,
                                      const std::string& status,
                                      int chunk_count) {
    if (info.document_id.empty()) {
        return false;
    }
    if (status == "pending" || status == "indexing") {
        info.indexing = true;
        info.failed = false;
        return true;
    }
    if (status == "indexed") {
        info.indexing = false;
        info.failed = false;
        if (chunk_count >= 0) {
            info.chunk_count = chunk_count;
        }
        return true;
    }
    if (status == "failed") {
        info.indexing = false;
        info.failed = true;
        info.chunk_count = -1;
        return true;
    }
    return false;
}

enum class CorpusDocMatchKind {
    None,
    DocumentId,
    LegacyMap,
    NameAndHash,
    NameOnly,
};

struct CorpusDocMatch {
    int doc_index = -1;
    CorpusDocMatchKind kind = CorpusDocMatchKind::None;
    bool ambiguous = false;
};

/** Collect document_id + legacy_id_map aliases for corpus row lookup. */
inline std::vector<std::string> documentIdCandidates(
    const std::string& document_id,
    const nlohmann::json& legacy_id_map) {
    std::vector<std::string> ids;
    if (document_id.empty()) {
        return ids;
    }
    auto append_unique = [&ids](const std::string& id) {
        if (id.empty()) {
            return;
        }
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    };
    append_unique(document_id);
    if (legacy_id_map.is_object()) {
        if (const auto it = legacy_id_map.find(document_id);
            it != legacy_id_map.end() && it->is_string()) {
            append_unique(it->get<std::string>());
        }
        for (const auto& entry : legacy_id_map.items()) {
            if (entry.value().is_string()
                && entry.value().get<std::string>() == document_id) {
                append_unique(entry.key());
            }
        }
    }
    return ids;
}

/**
 * ALP-E locked corpus↔cache match ladder (never basename-only):
 * 1. document_id exact (+ legacy_id_map aliases)
 * 2. canonical_name + content_hash (when corpus exposes hash)
 * 3. canonical_name only — ambiguous when multiple rows share name
 */
inline CorpusDocMatch matchCacheEntryToCorpusDoc(
    const LocalNoteEngineInfo& info,
    const nlohmann::json& documents,
    const nlohmann::json& legacy_id_map) {
    if (!documents.is_array()) {
        return {};
    }

    const auto id_candidates = documentIdCandidates(info.document_id, legacy_id_map);
    if (!id_candidates.empty()) {
        int found = -1;
        bool used_legacy = false;
        for (std::size_t i = 0; i < documents.size(); ++i) {
            if (!documents[i].is_object()) {
                continue;
            }
            const std::string doc_id = documents[i].value("id", "");
            for (const auto& candidate : id_candidates) {
                if (doc_id != candidate) {
                    continue;
                }
                if (found >= 0 && static_cast<std::size_t>(found) != i) {
                    return { -1, CorpusDocMatchKind::None, true };
                }
                found = static_cast<int>(i);
                if (candidate != info.document_id) {
                    used_legacy = true;
                }
            }
        }
        if (found >= 0) {
            return { found,
                     used_legacy ? CorpusDocMatchKind::LegacyMap
                                 : CorpusDocMatchKind::DocumentId,
                     false };
        }
    }

    if (!info.document_name.empty() && !info.content_hash.empty()) {
        int found = -1;
        for (std::size_t i = 0; i < documents.size(); ++i) {
            if (!documents[i].is_object()) {
                continue;
            }
            const auto& doc = documents[i];
            if (doc.value("name", "") != info.document_name) {
                continue;
            }
            const std::string doc_hash = doc.value("content_hash", "");
            if (doc_hash.empty() || doc_hash != info.content_hash) {
                continue;
            }
            if (found >= 0) {
                return { -1, CorpusDocMatchKind::None, true };
            }
            found = static_cast<int>(i);
        }
        if (found >= 0) {
            return { found, CorpusDocMatchKind::NameAndHash, false };
        }
    }

    if (!info.document_name.empty()) {
        int found = -1;
        for (std::size_t i = 0; i < documents.size(); ++i) {
            if (!documents[i].is_object()) {
                continue;
            }
            if (documents[i].value("name", "") != info.document_name) {
                continue;
            }
            if (found >= 0) {
                return { -1, CorpusDocMatchKind::NameOnly, true };
            }
            found = static_cast<int>(i);
        }
        if (found >= 0) {
            return { found, CorpusDocMatchKind::NameOnly, false };
        }
    }

    return {};
}

/**
 * ALP-E locked INDEXING_* host-path ladder:
 * 1. document_id  2. revision_id  3. canonical_name
 * 4. path  5. basename (legacy / !alp_gui only)
 */
inline std::string findHostPathForIndexingEvent(const ChatSession& session,
                                                const IndexingEventMetadata& event,
                                                bool alp_gui) {
    if (!event.document_id.empty()) {
        for (const auto& entry : session.localNoteEngine) {
            if (entry.second.document_id == event.document_id) {
                return entry.first;
            }
        }
    }

    if (!event.revision_id.empty()) {
        for (const auto& entry : session.localNoteEngine) {
            if (entry.second.revision_id == event.revision_id) {
                return entry.first;
            }
        }
    }

    const std::string canonical = !event.document_name.empty()
        ? event.document_name
        : ragEventBasename(event.file_path);
    if (!canonical.empty()) {
        std::string match;
        for (const auto& entry : session.localNoteEngine) {
            const std::string slot_name = !entry.second.document_name.empty()
                ? entry.second.document_name
                : ragEventBasename(entry.first);
            if (slot_name != canonical) {
                continue;
            }
            if (!match.empty()) {
                return {};
            }
            match = entry.first;
        }
        if (match.empty()) {
            for (const auto& path : session.ragFilePaths) {
                if (ragEventBasename(path) != canonical) {
                    continue;
                }
                if (!match.empty()) {
                    return {};
                }
                match = path;
            }
        }
        if (!match.empty()) {
            return match;
        }
    }

    if (!event.file_path.empty()) {
        const std::string event_base = ragEventBasename(event.file_path);
        std::string match;
        for (const auto& path : session.ragFilePaths) {
            if (path != event.file_path && ragEventBasename(path) != event_base) {
                continue;
            }
            if (!match.empty() && match != path) {
                return {};
            }
            match = path;
        }
        if (!match.empty()) {
            return match;
        }
    }

    if (!alp_gui) {
        const std::string event_base = ragEventBasename(event.file_path);
        if (!event_base.empty()) {
            std::string match;
            for (const auto& path : session.ragFilePaths) {
                if (ragEventBasename(path) != event_base) {
                    continue;
                }
                if (!match.empty()) {
                    return {};
                }
                match = path;
            }
            if (!match.empty()) {
                return match;
            }
        }
        std::string lone;
        for (const auto& entry : session.localNoteEngine) {
            if (!entry.second.indexing) {
                continue;
            }
            if (!lone.empty()) {
                return {};
            }
            lone = entry.first;
        }
        return lone;
    }

    return {};
}

/** Match corpus list entries to session Local Note bindings (locked ladder). */
inline bool syncSessionFromCorpusList(ChatSession& session,
                                      const nlohmann::json& corpus_body,
                                      const nlohmann::json& legacy_id_map = {}) {
    if (session.localNoteEngine.empty()) {
        return false;
    }
    std::string err;
    if (!CorpusDocuments::hasRequiredV1Fields(corpus_body, err)
        || !corpus_body.contains("documents")
        || !corpus_body["documents"].is_array()) {
        return false;
    }

    const auto& documents = corpus_body["documents"];
    bool changed = false;
    for (auto& entry : session.localNoteEngine) {
        const CorpusDocMatch match =
            matchCacheEntryToCorpusDoc(entry.second, documents, legacy_id_map);
        if (match.doc_index < 0 || match.ambiguous) {
            continue;
        }
        const auto& doc = documents[match.doc_index];
        int chunk_count = -1;
        if (doc.contains("chunk_count") && doc["chunk_count"].is_number_integer()) {
            chunk_count = doc["chunk_count"].get<int>();
        }
        const LocalNoteEngineInfo before = entry.second;
        if (applyCorpusDocumentStatus(
                entry.second, doc.value("status", ""), chunk_count)
            && (entry.second.indexing != before.indexing
                || entry.second.failed != before.failed
                || entry.second.chunk_count != before.chunk_count)) {
            changed = true;
        }
    }
    return changed;
}

/**
 * Local Note X against Engine session-link authority.
 * A linked note is removed from the GUI only after unlink succeeds.
 * A host-only note (no document id) is removed locally.
 */
struct LocalNoteXDisposition {
    bool remove_local_slot = false;
    std::string status_message;
};

inline LocalNoteXDisposition localNoteXAfterUnlinkAttempt(
    bool has_engine_document,
    bool unlink_succeeded,
    const std::string& failure_user_message) {
    if (!has_engine_document) {
        return {true, {}};
    }
    if (!unlink_succeeded) {
        std::string message = failure_user_message;
        if (message.empty()) {
            message = "Could not unlink document from chat";
        }
        return {false, std::move(message)};
    }
    return {true, "Local Note removed and unlinked from this chat"};
}

/** Erase one slot and its cache only when disposition allows it. */
inline bool eraseLocalNoteSlotIfAuthorized(
    std::vector<std::string>& paths,
    std::map<std::string, LocalNoteEngineInfo>& cache,
    std::size_t index,
    const LocalNoteXDisposition& disposition) {
    if (!disposition.remove_local_slot || index >= paths.size()) {
        return false;
    }
    const std::string removed = paths[index];
    paths.erase(paths.begin() + static_cast<std::ptrdiff_t>(index));
    cache.erase(removed);
    return true;
}

inline std::string formatEngineSlotDocumentId(const std::string& document_id,
                                              bool use_short_uuid) {
    if (document_id.empty()) {
        return {};
    }
    if (use_short_uuid) {
        return formatUuidShort(document_id);
    }
    return document_id;
}

} // namespace LocalNoteEngineSync
} // namespace Thoth

#endif // THOTH_LOCAL_NOTE_ENGINE_SYNC_H
