/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — compact CM-1 feedback dialog for one goal-synthesis answer
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#pragma once

#include "cm1_telemetry.h"

#include <optional>
#include <string>
#include <wx/wx.h>

/** Cancel or close returns nullopt and writes nothing. */
std::optional<Thoth::Cm1FeedbackSubmission> ShowCm1FeedbackDialog(
    wxWindow* parent,
    const std::string& answer_preview,
    const Thoth::Cm1UserFeedback* existing);
