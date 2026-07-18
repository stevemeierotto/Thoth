/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Plan N5 GRAG diagnostics display helpers (pure; no wx)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_GRAG_DIAGNOSTICS_DISPLAY_H
#define THOTH_GRAG_DIAGNOSTICS_DISPLAY_H

#include <cmath>
#include <optional>
#include <sstream>
#include <string>

namespace Thoth {
namespace GragDiagnosticsDisplay {

/** True when retrieval is not using a goal embedding / directional blend. */
inline bool isConversationalNoGoalDirection(const std::string& scoring_type) {
    if (scoring_type == "no_index" || scoring_type == "greeting_skip"
        || scoring_type == "rag_hybrid") {
        return true;
    }
    // Unknown / empty: treat as non-goal unless clearly a grag_* mode.
    if (scoring_type.rfind("grag_", 0) == 0) {
        return false;
    }
    // Empty or unrecognized — no goal direction assumed for Alpha N/A safety.
    return scoring_type.empty() || scoring_type.find("grag") == std::string::npos;
}

/** Plan N5 D2 — plain float final_score; never percent. */
inline std::string formatFinalScoreLabel(std::optional<float> score) {
    if (!score.has_value() || !std::isfinite(score.value())) {
        return "N/A";
    }
    std::ostringstream ss;
    ss << std::fixed;
    ss.precision(2);
    ss << score.value();
    return ss.str();
}

/**
 * Plan N5 D3 — Alpha label.
 * Conversational / no goal embedding → "N/A (chat)"; else "Alpha: X.XX".
 */
inline std::string formatAlphaLabel(float alpha, bool conversational_no_goal) {
    if (conversational_no_goal) {
        return "N/A (chat)";
    }
    std::ostringstream ss;
    ss << "Alpha: " << std::fixed;
    ss.precision(2);
    ss << alpha;
    return ss.str();
}

/** Plan N5 D4 — Mode label for interpretation (magnitude stays numeric elsewhere). */
inline std::string formatModeLabel(const std::string& scoring_type) {
    if (scoring_type == "no_index" || scoring_type == "greeting_skip"
        || scoring_type == "rag_hybrid") {
        return "Conversational";
    }
    if (scoring_type == "grag_hybrid") {
        return "GRAG";
    }
    if (scoring_type == "grag_blended_hybrid") {
        return "GRAG blended";
    }
    if (scoring_type.empty()) {
        return "Unknown";
    }
    // N5-T5 — unknown mode: surface raw type as fallback, never invent %.
    return scoring_type;
}

inline std::string formatMagnitudeLabel(float magnitude) {
    std::ostringstream ss;
    ss << std::fixed;
    ss.precision(3);
    ss << magnitude;
    return ss.str();
}

} // namespace GragDiagnosticsDisplay
} // namespace Thoth

#endif // THOTH_GRAG_DIAGNOSTICS_DISPLAY_H
