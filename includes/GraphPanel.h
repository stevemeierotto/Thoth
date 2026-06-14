/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Graph Visualization Panel Header
 */

#pragma once

#include <wx/wx.h>
#include <json.hpp>
#include <map>

class GraphPanel : public wxPanel {
public:
    GraphPanel(wxWindow* parent);

    void UpdateGraphStats(const nlohmann::json& statsJson);
    void UpdateControllerState(const std::string& state);
    void ResetNodes();

private:
    wxStaticText* m_nodesValue = nullptr;
    wxStaticText* m_edgesValue = nullptr;
    wxStaticText* m_avgWeightValue = nullptr;
    wxStaticText* m_successRateValue = nullptr;

    struct GraphNode {
        std::string label;
        wxRect rect;
        bool active = false;  // Has been touched in current request
        bool current = false; // Is currently performing work (pulses)
    };
    std::map<std::string, GraphNode> m_cognitiveNodes;
    std::string m_currentState;

    void InitializeUI();
    void OnPaint(wxPaintEvent& event);
    void OnTimer(wxTimerEvent& event);
    void DrawArrow(wxGraphicsContext* gc, const wxPoint& start, const wxPoint& end);

    wxTimer m_timer;
    float m_pulseValue = 0.0f;
    bool m_pulseIncreasing = true;
};
