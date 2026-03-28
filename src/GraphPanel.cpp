/*
 * Copyright (c) 2026 Steve Meierotto
 * 
 * Thoth — Graph Visualization Panel (Cognative Loop)
 */

#include "GraphPanel.h"
#include <wx/sizer.h>
#include <wx/graphics.h>
#include <wx/dcclient.h>
#include <iomanip>
#include <sstream>
#include <cmath>

GraphPanel::GraphPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    InitializeUI();
    Bind(wxEVT_PAINT, &GraphPanel::OnPaint, this);
}

void GraphPanel::InitializeUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Cognate Architecture Status:"), 0, wxALL, 5);

    wxFlexGridSizer* grid = new wxFlexGridSizer(4, 5, 10);
    grid->Add(new wxStaticText(this, wxID_ANY, "Nodes:"));
    m_nodesValue = new wxStaticText(this, wxID_ANY, "0");
    grid->Add(m_nodesValue);

    grid->Add(new wxStaticText(this, wxID_ANY, "Edges:"));
    m_edgesValue = new wxStaticText(this, wxID_ANY, "0");
    grid->Add(m_edgesValue);

    grid->Add(new wxStaticText(this, wxID_ANY, "Weight:"));
    m_avgWeightValue = new wxStaticText(this, wxID_ANY, "0.00");
    grid->Add(m_avgWeightValue);

    grid->Add(new wxStaticText(this, wxID_ANY, "Success:"));
    m_successRateValue = new wxStaticText(this, wxID_ANY, "0%");
    grid->Add(m_successRateValue);

    mainSizer->Add(grid, 0, wxALL, 10);

    // Setup Cognitive Loop Nodes positions (relative)
    m_cognitiveNodes["GOAL"]       = {"Goal Ingestion", wxRect(20, 100, 120, 40)};
    m_cognitiveNodes["PLANNER"]    = {"LLM Planner",    wxRect(180, 100, 120, 40)};
    m_cognitiveNodes["EXECUTOR"]   = {"Executive Ctrl", wxRect(340, 100, 120, 40)};
    m_cognitiveNodes["SCIENTIFIC"] = {"Scientific Mode",wxRect(340, 40, 120, 40)};
    m_cognitiveNodes["MEMORY"]     = {"Hybrid Memory",  wxRect(500, 100, 120, 40)};
    m_cognitiveNodes["LEARNING"]   = {"Strategy Engine",wxRect(340, 160, 120, 40)};

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

void GraphPanel::UpdateControllerState(const std::string& state) {
    m_currentState = state;
    
    // Reset all nodes
    for (auto& [key, node] : m_cognitiveNodes) {
        node.active = false;
    }

    // Map state to nodes
    if (state == "PLANNING") {
        m_cognitiveNodes["GOAL"].active = true;
        m_cognitiveNodes["PLANNER"].active = true;
    } else if (state == "EXECUTING_STEP" || state == "OBSERVING_RESULT") {
        m_cognitiveNodes["EXECUTOR"].active = true;
    } else if (state == "SCIENTIFIC_MODE") {
        m_cognitiveNodes["SCIENTIFIC"].active = true;
    } else if (state == "REVISING_PLAN") {
        m_cognitiveNodes["PLANNER"].active = true;
        m_cognitiveNodes["EXECUTOR"].active = true;
    } else if (state == "COMPLETED") {
        m_cognitiveNodes["LEARNING"].active = true;
        m_cognitiveNodes["MEMORY"].active = true;
    } else if (state == "IDLE") {
        // All nodes inactive - already handled by reset loop above
    }

    Refresh();
}

void GraphPanel::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (!gc) return;

    // Use a clean font
    wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    gc->SetFont(font, *wxBLACK);

    // Draw Edges (Arrows)
    gc->SetPen(wxPen(wxColour(180, 180, 180), 2));
    
    // Goal -> Planner
    DrawArrow(gc, wxPoint(140, 120), wxPoint(180, 120));
    // Planner -> Executor
    DrawArrow(gc, wxPoint(300, 120), wxPoint(340, 120));
    // Executor <-> Scientific
    DrawArrow(gc, wxPoint(400, 100), wxPoint(400, 80));
    DrawArrow(gc, wxPoint(400, 80), wxPoint(400, 100));
    // Executor -> Memory
    DrawArrow(gc, wxPoint(460, 120), wxPoint(500, 120));
    // Executor -> Learning
    DrawArrow(gc, wxPoint(400, 140), wxPoint(400, 160));
    // Learning -> Memory
    DrawArrow(gc, wxPoint(460, 180), wxPoint(500, 140));

    // Draw Nodes
    for (auto const& [key, node] : m_cognitiveNodes) {
        if (node.active) {
            gc->SetBrush(wxBrush(wxColour(100, 200, 100))); // Green for active
            gc->SetPen(wxPen(wxColour(50, 150, 50), 2));
        } else {
            gc->SetBrush(wxBrush(wxColour(240, 240, 240))); // Grey for inactive
            gc->SetPen(wxPen(wxColour(200, 200, 200), 1));
        }

        gc->DrawRoundedRectangle(node.rect.x, node.rect.y, node.rect.width, node.rect.height, 5);
        
        double tw, th;
        gc->GetTextExtent(node.label, &tw, &th);
        gc->DrawText(node.label, node.rect.x + (node.rect.width - tw)/2, node.rect.y + (node.rect.height - th)/2);
    }

    delete gc;
}

void GraphPanel::DrawArrow(wxGraphicsContext* gc, const wxPoint& start, const wxPoint& end) {
    gc->StrokeLine(start.x, start.y, end.x, end.y);
    
    // Simple arrow head
    double angle = atan2(end.y - start.y, end.x - start.x);
    double headLen = 10;
    gc->StrokeLine(end.x, end.y, end.x - headLen * cos(angle - M_PI/6), end.y - headLen * sin(angle - M_PI/6));
    gc->StrokeLine(end.x, end.y, end.x - headLen * cos(angle + M_PI/6), end.y - headLen * sin(angle + M_PI/6));
}
