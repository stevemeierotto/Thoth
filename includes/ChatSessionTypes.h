#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Forward declaration if needed, but not for these simple structs
// class MainFrame;

namespace Thoth { // Using a namespace to avoid global name collisions
    struct ChatMessage {
        std::string role;
        std::string content;
        std::int64_t timestampMs = 0;
    };

    /** Engine ingest outcome for a host-side Local Note path (Option A + send feedback). */
    struct LocalNoteEngineInfo {
        std::string document_id;
        /** Engine-side filename at accept (may differ from host basename on collision). */
        std::string document_name;
        int chunk_count = -1;
        bool indexing = false;
        bool failed = false;
    };

    struct ChatSession {
        std::string id;
        std::string title;
        std::int64_t createdAtMs = 0;
        std::int64_t updatedAtMs = 0;
        std::vector<ChatMessage> messages;
        std::vector<std::string> ragFilePaths;
        /** Host path → Engine document metadata after Send to Engine. */
        std::map<std::string, LocalNoteEngineInfo> localNoteEngine;
        std::string activeGoal;
    };
} // namespace Thoth
