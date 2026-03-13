/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Executive State Visualizer Implementation Phase 2.3
 */

#include "ExecutiveStateStrip.h"
#include <wx/dcbuffer.h>

namespace Thoth {

ExecutiveStateStrip::ExecutiveStateStrip(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, STEP_HEIGHT + 2 * MARGIN))
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(245, 245, 245));
    
    Bind(wxEVT_PAINT, &ExecutiveStateStrip::OnPaint, this);
    Bind(wxEVT_SIZE, &ExecutiveStateStrip::OnSize, this);
}

void ExecutiveStateStrip::ResetPlan(const nlohmann::json& planJson) {
    m_steps.clear();
    try {
        if (planJson.contains("steps") && planJson["steps"].is_array()) {
            for (const auto& s : planJson["steps"]) {
                VisualStep vs;
                vs.id = s.value("step_id", "");
                vs.description = s.value("description", "");
                vs.status = StepStatus::PENDING; // Initial state
                m_steps.push_back(vs);
            }
        }
    } catch (...) {}
    
    Refresh();
}

void ExecutiveStateStrip::UpdateStepStatus(const std::string& stepId, StepStatus status) {
    for (auto& s : m_steps) {
        if (s.id == stepId) {
            s.status = status;
            break;
        }
    }
    Refresh();
}

void ExecutiveStateStrip::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();
    
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc) return;

    wxFont font = GetFont();
    font.SetPointSize(8);
    gc->SetFont(font, *wxBLACK);

    int x = MARGIN;
    int y = MARGIN;

    for (size_t i = 0; i < m_steps.size(); ++i) {
        const auto& step = m_steps[i];
        
        // Draw connecting line to next step
        if (i < m_steps.size() - 1) {
            gc->SetPen(wxPen(wxColour(200, 200, 200), 2));
            gc->StrokeLine(x + STEP_WIDTH, y + STEP_HEIGHT / 2, x + STEP_WIDTH + SPACING, y + STEP_HEIGHT / 2);
        }

        // Draw Step Box
        wxColour col = GetStatusColour(step.status);
        gc->SetBrush(wxBrush(col));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRoundedRectangle(x, y, STEP_WIDTH, STEP_HEIGHT, 5);

        // Draw Abbreviated ID or Label
        wxString label = wxString::FromUTF8(step.description);
        if (label.Length() > 15) label = label.Left(12) + "...";
        
        double textW, textH;
        gc->GetTextExtent(label, &textW, &textH);
        gc->SetFont(font, (step.status == StepStatus::RUNNING || step.status == StepStatus::PENDING) ? *wxBLACK : *wxWHITE);
        gc->DrawText(label, x + (STEP_WIDTH - textW) / 2, y + (STEP_HEIGHT - textH) / 2);

        x += STEP_WIDTH + SPACING;
    }
}

void ExecutiveStateStrip::OnSize(wxSizeEvent& event) {
    Refresh();
    event.Skip();
}

wxColour ExecutiveStateStrip::GetStatusColour(StepStatus status) {
    switch (status) {
        case StepStatus::PENDING: return wxColour(220, 220, 220);
        case StepStatus::RUNNING: return wxColour(255, 235, 59);  // Yellow
        case StepStatus::SUCCESS: return wxColour(76, 175, 80);   // Green
        case StepStatus::FAILED:  return wxColour(244, 67, 54);   // Red
        case StepStatus::SKIPPED: return wxColour(180, 180, 180); 
    }
    return *wxWHITE;
}

} // namespace Thoth
