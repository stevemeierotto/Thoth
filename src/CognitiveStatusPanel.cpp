/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Cognitive decision-state panel
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */

#include "CognitiveStatusPanel.h"
#include "cognitive_status_display.h"

#include <wx/sizer.h>
#include <wx/statline.h>

CognitiveStatusPanel::CognitiveStatusPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    InitializeUI();
}

void CognitiveStatusPanel::InitializeUI() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    m_phaseLabel = new wxStaticText(this, wxID_ANY, "Phase: Idle");
    m_phaseLabel->SetFont(m_phaseLabel->GetFont().Bold());
    mainSizer->Add(m_phaseLabel, 0, wxALL, 5);

    mainSizer->Add(
        new wxStaticText(this, wxID_ANY, "Decision tape (goal / executive)"),
        0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Read-only multiline tape — no wxLB_HSCROLL / nested list scrollbar.
    m_timeline = new wxTextCtrl(
        this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY);
    m_timeline->SetMinSize(wxSize(-1, 120));
    mainSizer->Add(m_timeline, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
}

void CognitiveStatusPanel::SetPhase(const std::string& phase_label) {
    if (!m_phaseLabel) {
        return;
    }
    m_phaseLabel->SetLabel(wxString::FromUTF8("Phase: " + phase_label));
}

void CognitiveStatusPanel::AppendLine(const std::string& line) {
    if (line.empty()) {
        return;
    }
    // Skip consecutive duplicates (optimistic "Planning started" + Engine echo).
    if (!m_lines.empty() && m_lines.back() == line) {
        return;
    }
    m_lines.push_back(line);
    while (m_lines.size() > Thoth::CognitiveStatusDisplay::kMaxTimelineLines) {
        m_lines.pop_front();
    }
    RefreshTimeline();
}

void CognitiveStatusPanel::BeginGoalPlanningOptimistic() {
    m_lines.clear();
    if (m_timeline) {
        m_timeline->Clear();
    }
    SetPhase("Planning");
    AppendLine(Thoth::CognitiveStatusDisplay::kOptimisticPlanningStartedLine);
    // Force a paint before any long GUI-thread work (RefreshAllPanels / sync).
    Layout();
    if (m_timeline) {
        m_timeline->Refresh();
        m_timeline->Update();
    }
    Refresh();
    Update();
}

void CognitiveStatusPanel::NoteChatTurnWaiting() {
    SetPhase("Chat (not /goal)");
    AppendLine("Chat turn in progress — use /goal … for executive decision tape");
    Layout();
    Refresh();
    Update();
}

void CognitiveStatusPanel::RefreshTimeline() {
    if (!m_timeline) {
        return;
    }
    wxString body;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (i > 0) {
            body << '\n';
        }
        body << wxString::FromUTF8(m_lines[i]);
    }
    m_timeline->ChangeValue(body);
    if (!m_lines.empty()) {
        m_timeline->ShowPosition(m_timeline->GetLastPosition());
    }
}

void CognitiveStatusPanel::ClearForSessionSwitch() {
    m_lines.clear();
    if (m_timeline) {
        m_timeline->Clear();
    }
    SetPhase("Idle");
}

void CognitiveStatusPanel::ApplyEvent(const ControllerEvent& event,
                                      const std::string& active_session_id) {
    if (!Thoth::CognitiveStatusDisplay::mayApplyEvent(event.session_id, active_session_id)) {
        return;
    }

    const auto line = Thoth::CognitiveStatusDisplay::formatDecisionLine(
        event.type, event.metadata, event.controller_state_name, event.step_id);
    if (line.has_value()) {
        AppendLine(*line);
    }

    SetPhase(Thoth::CognitiveStatusDisplay::formatPhaseForEvent(
        event.type, event.controller_state_name, event.metadata));
}
