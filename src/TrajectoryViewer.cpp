/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Trajectory Viewer Implementation
 */

#include "TrajectoryViewer.h"
#include <wx/sizer.h>
#include <iomanip>
#include <sstream>

TrajectoryViewer::TrajectoryViewer(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    InitializeUI();
}

void TrajectoryViewer::InitializeUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Execution Trajectories:"), 0, wxALL, 5);
    
    m_trajectoryList = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 300));
    m_trajectoryList->AppendTextColumn("ID", wxDATAVIEW_CELL_INERT, 100);
    m_trajectoryList->AppendTextColumn("Goal", wxDATAVIEW_CELL_INERT, 300);
    m_trajectoryList->AppendTextColumn("Score", wxDATAVIEW_CELL_INERT, 80);
    m_trajectoryList->AppendTextColumn("Created", wxDATAVIEW_CELL_INERT, 150);
    
    mainSizer->Add(m_trajectoryList, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
}

void TrajectoryViewer::UpdateTrajectories(const nlohmann::json& trajectoriesJson) {
    m_trajectoryList->DeleteAllItems();

    if (!trajectoriesJson.is_array()) return;

    for (const auto& traj : trajectoriesJson) {
        wxVector<wxVariant> data;
        data.push_back(wxVariant(wxString::FromUTF8(traj.value("trajectory_id", "unknown"))));
        data.push_back(wxVariant(wxString::FromUTF8(traj.value("goal", "n/a"))));

        float score = traj.value("success_score", 0.0f);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << score;
        data.push_back(wxVariant(wxString::FromUTF8(ss.str())));

        long long ts = traj.value("created_at", 0LL);
        wxDateTime dt(static_cast<time_t>(ts / 1000));
        data.push_back(wxVariant(dt.Format("%Y-%m-%d %H:%M:%S")));
        
        m_trajectoryList->AppendItem(data);
    }
    
    Layout();
}
