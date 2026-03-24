/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Graph Visualization Panel Implementation
 */

#include "GraphPanel.h"
#include <wx/sizer.h>
#include <iomanip>
#include <sstream>

GraphPanel::GraphPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    InitializeUI();
}

void GraphPanel::InitializeUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Cognate Knowledge Graph:"), 0, wxALL, 5);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 10);
    grid->Add(new wxStaticText(this, wxID_ANY, "Total Nodes:"));
    m_nodesValue = new wxStaticText(this, wxID_ANY, "0");
    grid->Add(m_nodesValue);

    grid->Add(new wxStaticText(this, wxID_ANY, "Total Edges:"));
    m_edgesValue = new wxStaticText(this, wxID_ANY, "0");
    grid->Add(m_edgesValue);

    grid->Add(new wxStaticText(this, wxID_ANY, "Avg Edge Weight:"));
    m_avgWeightValue = new wxStaticText(this, wxID_ANY, "0.00");
    grid->Add(m_avgWeightValue);

    grid->Add(new wxStaticText(this, wxID_ANY, "Global Success:"));
    m_successRateValue = new wxStaticText(this, wxID_ANY, "0%");
    grid->Add(m_successRateValue);

    mainSizer->Add(grid, 0, wxALL, 10);

    // Placeholder for actual graph visualization
    mainSizer->Add(new wxStaticText(this, wxID_ANY, "[Visual Workflow Graph Prototype - Stub]"), 1, wxALIGN_CENTER | wxALL, 20);

    SetSizer(mainSizer);
}

void GraphPanel::UpdateGraphStats(const nlohmann::json& statsJson) {
    if (statsJson.is_null()) return;

    m_nodesValue->SetLabel(std::to_string(statsJson.value("total_nodes", 0)));
    m_edgesValue->SetLabel(std::to_string(statsJson.value("total_edges", 0)));
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << statsJson.value("avg_edge_weight", 0.0f);
    m_avgWeightValue->SetLabel(ss.str());

    int success = statsJson.value("total_success_count", 0);
    int failure = statsJson.value("total_failure_count", 0);
    int total = success + failure;
    float rate = (total > 0) ? (float)success / total : 0.0f;
    
    std::stringstream ss2;
    ss2 << std::fixed << std::setprecision(1) << (rate * 100.0f) << "%";
    m_successRateValue->SetLabel(ss2.str());

    Layout();
}
