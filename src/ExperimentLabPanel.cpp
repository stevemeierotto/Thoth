/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Experiment Lab Panel Implementation
 */

#include "ExperimentLabPanel.h"
#include <wx/sizer.h>
#include <iomanip>
#include <sstream>

ExperimentLabPanel::ExperimentLabPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    InitializeUI();
}

void ExperimentLabPanel::InitializeUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Scientific Experiments:"), 0, wxALL, 5);
    
    m_experimentList = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 300));
    m_experimentList->AppendTextColumn("ID", wxDATAVIEW_CELL_INERT, 100);
    m_experimentList->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 200);
    m_experimentList->AppendTextColumn("Hypothesis", wxDATAVIEW_CELL_INERT, 300);
    m_experimentList->AppendTextColumn("Status", wxDATAVIEW_CELL_INERT, 100);
    
    mainSizer->Add(m_experimentList, 1, wxEXPAND | wxALL, 5);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(this, wxID_ANY, "New Experiment"), 0, wxALL, 5);
    btnSizer->Add(new wxButton(this, wxID_ANY, "Run Benchmark"), 0, wxALL, 5);
    mainSizer->Add(btnSizer, 0, wxALIGN_RIGHT);

    SetSizer(mainSizer);
}

void ExperimentLabPanel::UpdateExperiments(const nlohmann::json& experimentsJson) {
    m_experimentList->DeleteAllItems();

    if (!experimentsJson.is_array()) return;

    for (const auto& exp : experimentsJson) {
        wxVector<wxVariant> data;
        data.push_back(wxVariant(wxString::FromUTF8(exp.value("experiment_id", "unknown"))));
        data.push_back(wxVariant(wxString::FromUTF8(exp.value("name", "n/a"))));
        data.push_back(wxVariant(wxString::FromUTF8(exp.value("hypothesis", ""))));
        data.push_back(wxVariant(wxString::FromUTF8(exp.value("status", "pending"))));
        
        m_experimentList->AppendItem(data);
    }
    
    Layout();
}
