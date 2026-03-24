/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Trajectory Viewer Header
 */

#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <json.hpp>

class TrajectoryViewer : public wxPanel {
public:
    TrajectoryViewer(wxWindow* parent);

    void UpdateTrajectories(const nlohmann::json& trajectoriesJson);

private:
    wxDataViewListCtrl* m_trajectoryList = nullptr;
    void InitializeUI();
};
