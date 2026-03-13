/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Executive State Visualizer Phase 2.3
 */

#pragma once

#include <wx/wx.h>
#include <wx/graphics.h>
#include <vector>
#include <string>
#include "plan.h"
#include "controller_event.h"

namespace Thoth {

/**
 * @brief A custom-drawn horizontal strip that visualizes the progress of a Plan.
 */
class ExecutiveStateStrip : public wxPanel {
public:
    ExecutiveStateStrip(wxWindow* parent);

    void ResetPlan(const nlohmann::json& planJson);
    void UpdateStepStatus(const std::string& stepId, StepStatus status);

private:
    struct VisualStep {
        std::string id;
        std::string description;
        StepStatus status;
    };

    std::vector<VisualStep> m_steps;
    
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    
    wxColour GetStatusColour(StepStatus status);
    
    static constexpr int STEP_WIDTH = 120;
    static constexpr int STEP_HEIGHT = 40;
    static constexpr int SPACING = 15;
    static constexpr int MARGIN = 10;
};

} // namespace Thoth
