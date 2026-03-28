/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Trajectory Viewer Header
 */

#pragma once

#include <wx/wx.h>
#include <wx/treelist.h>
#include <json.hpp>

class TrajectoryViewer : public wxPanel {
public:
    TrajectoryViewer(wxWindow* parent);

    void UpdateTrajectories(const nlohmann::json& trajectoriesJson, const nlohmann::json& episodeStepsJson = nlohmann::json::array());

private:
    // wxTreeListCtrl provides BOTH columns (labels) and a tree structure (expandable)
    wxTreeListCtrl* m_treeList = nullptr;
    void InitializeUI();
};
