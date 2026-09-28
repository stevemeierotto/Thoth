/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — Trajectories panel score column.
 * Joins episode ids to the research trajectory payload. A missing terminal
 * row is labeled unterminated. It is not given a success or failure score.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_TRAJECTORY_ROW_STATUS_H
#define THOTH_TRAJECTORY_ROW_STATUS_H

#include <iomanip>
#include <sstream>
#include <string>

#include <json.hpp>

namespace Thoth {

/** Episode group with steps and no terminal trajectory row. Not an outcome. */
inline constexpr const char* kUnterminatedTrajectoryLabel = "Unterminated";

inline std::string formatTrajectorySuccessScore(double score) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << score;
    return ss.str();
}

/**
 * Score column for one episode/trajectory id.
 * A persisted trajectory row shows its success_score, including 0.00.
 * Absence from the trajectory payload shows kUnterminatedTrajectoryLabel.
 */
inline std::string trajectoryParentStatusColumn(const std::string& episodeId,
                                                const nlohmann::json& trajectoryItems) {
    if (trajectoryItems.is_array()) {
        for (const auto& item : trajectoryItems) {
            if (!item.is_object()) {
                continue;
            }
            if (item.value("trajectory_id", std::string()) != episodeId) {
                continue;
            }
            return formatTrajectorySuccessScore(item.value("success_score", 0.0));
        }
    }
    return kUnterminatedTrajectoryLabel;
}

} // namespace Thoth

#endif // THOTH_TRAJECTORY_ROW_STATUS_H
