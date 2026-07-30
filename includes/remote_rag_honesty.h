/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — GUI Phase 1 remote RAG honesty helpers (pure; no wx)
 *
 * Locked strings for Option A: host-only session notes must never claim
 * Engine indexing when THOTH_ENGINE_URL remote mode is active.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_REMOTE_RAG_HONESTY_H
#define THOTH_REMOTE_RAG_HONESTY_H

#include <string>

namespace Thoth {
namespace RemoteRagHonesty {

/** R1 — Local Notes on host are host-side only when using Engine (remote) transport. */
inline bool localNotesAreHostSideOnly(bool is_remote) {
    return is_remote;
}

/** Phase 1 / R1 — sync session RAG paths to backend only in Local mode (host workspace). */
inline bool shouldSyncRagFilesToBackend(bool is_remote) {
    return !is_remote;
}

/**
 * Phase 5 (D3a): never claim indexing on host drop — wait for INDEXING_* events.
 * Supersedes Phase 1 local-true; Engine mode still never claims (same result).
 */
inline bool shouldClaimIndexingOnHostDrop(bool /*is_remote*/) {
    return false;
}

/** Locked status bar string after remote host-only add. */
inline std::string formatHostOnlyAddStatus(int files_added) {
    return "Added " + std::to_string(files_added)
        + " file(s) — host-only; not sent to Engine";
}

/** Locked slot label suffix for remote non-empty slots. */
inline std::string formatHostOnlySlotLabel(const std::string& basename) {
    if (basename.empty()) {
        return "(host-only)";
    }
    return basename + " (host-only)";
}

inline constexpr const char* kHostOnlyTooltip = "Host-only (not sent to Engine)";

/**
 * Local Note slot label after Send to Engine (drops host-only suffix).
 */
inline std::string formatLocalNoteEngineSlotLabel(const std::string& basename,
                                                  const std::string& document_id,
                                                  int chunk_count,
                                                  bool indexing,
                                                  bool failed,
                                                  bool use_short_uuid = false) {
    std::string label = basename.empty() ? "document" : basename;
    if (!document_id.empty()) {
        const std::string id_display =
            use_short_uuid && document_id.size() > 8 ? document_id.substr(0, 8)
                                                   : document_id;
        label += " · id=" + id_display;
    }
    if (indexing) {
        label += " · indexing…";
    } else if (failed) {
        label += " · indexing failed";
    } else if (chunk_count >= 0) {
        label += " · " + std::to_string(chunk_count)
            + (chunk_count == 1 ? " chunk" : " chunks");
    }
    return label;
}

inline std::string formatLocalNoteEngineTooltip(const std::string& document_id,
                                                int chunk_count,
                                                bool indexing,
                                                bool failed) {
    if (document_id.empty() && !indexing && !failed && chunk_count < 0) {
        return kHostOnlyTooltip;
    }
    std::string tip = "Sent to Engine";
    if (!document_id.empty()) {
        tip += " (id=" + document_id + ")";
    }
    if (indexing) {
        tip += " — indexing…";
    } else if (failed) {
        tip += " — indexing failed";
    } else if (chunk_count >= 0) {
        tip += " — " + std::to_string(chunk_count)
            + (chunk_count == 1 ? " chunk" : " chunks");
    }
    return tip;
}

/** Optional status footnote (seed path) — appendable; not required on every add. */
inline constexpr const char* kSeedWorkspaceHint =
    "Seed engine corpus: ./docker/seed-workspace.sh";

} // namespace RemoteRagHonesty
} // namespace Thoth

#endif // THOTH_REMOTE_RAG_HONESTY_H
