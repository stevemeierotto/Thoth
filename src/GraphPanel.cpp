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
    : wxPanel(parent, wxID_ANY), m_timer(this)
{
    InitializeUI();
    Bind(wxEVT_PAINT, &GraphPanel::OnPaint, this);
    Bind(wxEVT_TIMER, &GraphPanel::OnTimer, this, m_timer.GetId());
    m_timer.Start(50); // 20 FPS for smooth pulsing
}

void GraphPanel::OnTimer(wxTimerEvent& event) {
    if (m_pulseIncreasing) {
        m_pulseValue += 0.05f;
        if (m_pulseValue >= 1.0f) {
            m_pulseValue = 1.0f;
            m_pulseIncreasing = false;
        }
    } else {
        m_pulseValue -= 0.05f;
        if (m_pulseValue <= 0.0f) {
            m_pulseValue = 0.0f;
            m_pulseIncreasing = true;
        }
    }

    // Only refresh if something is currently performing work
    bool anyCurrent = false;
    for (auto const& [key, node] : m_cognitiveNodes) {
        if (node.current) {
            anyCurrent = true;
            break;
        }
    }

    if (anyCurrent) {
        Refresh();
    }
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
    
    // Clear current flag for all nodes
    for (auto& [key, node] : m_cognitiveNodes) {
        node.current = false;
    }

    auto activate = [&](const std::string& key) {
        if (m_cognitiveNodes.count(key)) {
            m_cognitiveNodes[key].active = true;
            m_cognitiveNodes[key].current = true;
        }
    };

    // Map state to nodes
    if (state == "PLANNING" || state == "CONVERSATIONAL") {
        if (state == "PLANNING") activate("GOAL");
        activate("PLANNER");
    } else if (state == "EXECUTING_STEP" || state == "OBSERVING_RESULT") {
        activate("EXECUTOR");
    } else if (state == "EXECUTING_RETRIEVAL") {
        activate("EXECUTOR");
        activate("MEMORY");
    } else if (state == "EXECUTING_LLM") {
        activate("EXECUTOR");
        activate("PLANNER");
    } else if (state == "EXECUTING_TOOL") {
        activate("EXECUTOR");
        activate("LEARNING");
    } else if (state == "SCIENTIFIC_MODE") {
        activate("SCIENTIFIC");
    } else if (state == "REVISING_PLAN") {
        activate("PLANNER");
        activate("EXECUTOR");
    } else if (state == "COMPLETED") {
        activate("LEARNING");
        activate("MEMORY");
    }

    Refresh();
}

void GraphPanel::ResetNodes() {
    for (auto& [key, node] : m_cognitiveNodes) {
        node.active = false;
        node.current = false;
    }
    Refresh();
}

void GraphPanel::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    
    wxSize size = GetClientSize();
    if (size.x <= 0 || size.y <= 0) return;

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
        if (node.current) {
            // Pulse between bright green and a softer green
            int r = 100 + (int)(50 * m_pulseValue);
            int g = 200 + (int)(55 * m_pulseValue);
            int b = 100 + (int)(50 * m_pulseValue);
            gc->SetBrush(wxBrush(wxColour(r, g, b))); 
            gc->SetPen(wxPen(wxColour(50, 150, 50), 2 + (int)(2 * m_pulseValue)));
        } else if (node.active) {
            gc->SetBrush(wxBrush(wxColour(120, 220, 120))); // Solid green for previously active
            gc->SetPen(wxPen(wxColour(80, 180, 80), 2));
        } else {
            gc->SetBrush(wxBrush(wxColour(240, 240, 240))); // Grey for inactive
            gc->SetPen(wxPen(wxColour(200, 200, 200), 1));
        }

        gc->DrawRoundedRectangle(node.rect.x, node.rect.y, node.rect.width, node.rect.height, 5);
        
        double tw, th;
        gc->GetTextExtent(node.label, &tw, &th);
        
        if (node.current || node.active) {
            gc->SetFont(font.Bold(), *wxBLACK);
        } else {
            gc->SetFont(font, *wxBLACK);
        }
        
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
