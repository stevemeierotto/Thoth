#pragma once

#include <cstdint>
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

    struct ChatSession {
        std::string id;
        std::string title;
        std::int64_t createdAtMs = 0;
        std::int64_t updatedAtMs = 0;
        std::vector<ChatMessage> messages;
        std::vector<std::string> ragFilePaths;
        std::string activeGoal;
    };
} // namespace Thoth
