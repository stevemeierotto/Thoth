/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Cognitive decision-state panel (goal / executive spine)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#pragma once

#include <deque>
#include <string>

#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "controller_event.h"
#include "json.hpp"

/**
 * Live cognitive decision tape for goal execution.
 * Distinct from chat-turn waiting chrome (Engine conversation HTTP).
 */
class CognitiveStatusPanel : public wxPanel {
public:
    explicit CognitiveStatusPanel(wxWindow* parent);

    /**
     * Apply a ControllerEvent if it belongs to the active session.
     * chat_turn_active allows the existing retrieval-diagnostics event onto the tape.
     * Events that do not produce a tape line do not change the phase.
     */
    void ApplyEvent(const ControllerEvent& event,
                    const std::string& active_session_id,
                    bool chat_turn_active);

    /**
     * Phase 1 — paint immediately on goal submit (before Engine events).
     * Clears prior goal tape so this run starts with "Planning started".
     */
    void BeginGoalPlanningOptimistic();

    /** Chat submit line. Preserves earlier tape history. */
    void NoteChatTurnWaiting();

    /** Idle line. Consecutive duplicates are suppressed. */
    void NoteSystemWaiting();

    void ClearForSessionSwitch();
    void SetPhase(const std::string& phase_label);

private:
    void InitializeUI();
    void AppendLine(const std::string& line);
    void RefreshTimeline();

    wxStaticText* m_phaseLabel = nullptr;
    wxTextCtrl* m_timeline = nullptr;
    std::deque<std::string> m_lines;
};
