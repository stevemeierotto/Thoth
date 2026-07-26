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

#include <string>
#include <vector>

namespace Thoth {
namespace LocalNoteEngineSync {

/** True after Send to Engine accept (document id recorded for this host path). */
inline bool localNoteAlreadySent(const ChatSession& session,
                                 const std::string& host_path) {
    const auto it = session.localNoteEngine.find(host_path);
    return it != session.localNoteEngine.end() && !it->second.document_id.empty();
}

/** Host paths not yet sent to Engine — used by the Send to Engine picker. */
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

/** Apply Engine corpus document status to a tracked Local Note binding. */
inline bool applyCorpusDocumentStatus(LocalNoteEngineInfo& info,
                                      const std::string& status,
                                      int chunk_count) {
    if (info.document_id.empty()) {
        return false;
    }
    if (status == "pending") {
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

/** Match corpus list entries to session Local Note bindings by document id. */
inline bool syncSessionFromCorpusList(ChatSession& session,
                                      const nlohmann::json& corpus_body) {
    if (session.localNoteEngine.empty()) {
        return false;
    }
    std::string err;
    if (!CorpusDocuments::hasRequiredV1Fields(corpus_body, err)
        || !corpus_body.contains("documents")
        || !corpus_body["documents"].is_array()) {
        return false;
    }

    bool changed = false;
    for (const auto& doc : corpus_body["documents"]) {
        if (!doc.is_object()) {
            continue;
        }
        const std::string doc_id = doc.value("id", "");
        if (doc_id.empty()) {
            continue;
        }
        for (auto& entry : session.localNoteEngine) {
            if (entry.second.document_id != doc_id) {
                continue;
            }
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
    }
    return changed;
}

} // namespace LocalNoteEngineSync
} // namespace Thoth

#endif // THOTH_LOCAL_NOTE_ENGINE_SYNC_H
