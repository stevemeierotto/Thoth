// includes/chat_send_trace.h
//
// Diagnostic-only ordered tracing for one Engine chat-send pipeline pass.
// Enable with THOTH_CHAT_SEND_TRACE=1 (or any non-empty value).
// Optional one-shot synthetic send: THOTH_CHAT_SEND_TRACE_ONCE=<message>
//
#pragma once

#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace Thoth {
namespace ChatSendTrace {

inline bool enabled() {
    const char* v = std::getenv("THOTH_CHAT_SEND_TRACE");
    return v != nullptr && v[0] != '\0';
}

inline void log(int step, const char* name, const std::string& detail) {
    if (!enabled()) {
        return;
    }
    std::cerr << "[CHAT_SEND_TRACE] step=" << step << " " << name;
    if (!detail.empty()) {
        std::cerr << " | " << detail;
    }
    std::cerr << std::endl;
}

inline void logAlways(int step, const char* name, const std::string& detail) {
    // Always emit when tracing is on; alias for call sites that must not be optimized away.
    log(step, name, detail);
}

inline const char* onceMessage() {
    const char* v = std::getenv("THOTH_CHAT_SEND_TRACE_ONCE");
    if (v == nullptr || v[0] == '\0') {
        return nullptr;
    }
    return v;
}

/** Hex + printable preview for proving where bytes disappear (cap 64 bytes). */
inline std::string bytesPreview(const std::string& s, std::size_t cap = 64) {
    std::ostringstream oss;
    oss << "bytes=" << s.size() << " hex=";
    const std::size_t n = s.size() < cap ? s.size() : cap;
    for (std::size_t i = 0; i < n; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << (static_cast<unsigned>(static_cast<unsigned char>(s[i])));
        if (i + 1 < n) {
            oss << ' ';
        }
    }
    if (s.size() > cap) {
        oss << " ...";
    }
    oss << std::dec << " text=\"";
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isprint(c) && c != '"' && c != '\\') {
            oss << static_cast<char>(c);
        } else {
            oss << '.';
        }
    }
    if (s.size() > cap) {
        oss << "...";
    }
    oss << '"';
    return oss.str();
}

inline void logPayload(const char* boundary, const std::string& payload) {
    if (!enabled()) {
        return;
    }
    std::cerr << "[CHAT_SEND_TRACE] payload_boundary=" << boundary
              << " | " << bytesPreview(payload) << std::endl;
}

} // namespace ChatSendTrace
} // namespace Thoth
