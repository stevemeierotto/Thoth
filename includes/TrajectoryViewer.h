/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Trajectory Viewer Header
 */

#pragma once

#include "panel_presentation_state.h"
#include <wx/wx.h>
#include <wx/treelist.h>
#include <json.hpp>

class TrajectoryViewer : public wxPanel {
public:
    TrajectoryViewer(wxWindow* parent);

    void SetPresentationState(Thoth::PanelPresentationState state,
                              const wxString& message = wxEmptyString);
    void UpdateTrajectories(const nlohmann::json& trajectoriesJson,
                            const nlohmann::json& episodesJson = nlohmann::json::array());

private:
    wxStaticText* m_statusLabel = nullptr;
    wxTreeListCtrl* m_treeList = nullptr;
    void InitializeUI();
};
