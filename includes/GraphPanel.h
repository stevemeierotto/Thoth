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

private:
    wxStaticText* m_nodesValue = nullptr;
    wxStaticText* m_edgesValue = nullptr;
    wxStaticText* m_avgWeightValue = nullptr;
    wxStaticText* m_successRateValue = nullptr;

    struct GraphNode {
        std::string label;
        wxRect rect;
        bool active = false;
    };
    std::map<std::string, GraphNode> m_cognitiveNodes;
    std::string m_currentState;

    void InitializeUI();
    void OnPaint(wxPaintEvent& event);
    void DrawArrow(wxGraphicsContext* gc, const wxPoint& start, const wxPoint& end);
};
