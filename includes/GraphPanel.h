/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Graph Visualization Panel Header
 */

#pragma once

#include <wx/wx.h>
#include <json.hpp>

class GraphPanel : public wxPanel {
public:
    GraphPanel(wxWindow* parent);

    void UpdateGraphStats(const nlohmann::json& statsJson);

private:
    wxStaticText* m_nodesValue = nullptr;
    wxStaticText* m_edgesValue = nullptr;
    wxStaticText* m_avgWeightValue = nullptr;
    wxStaticText* m_successRateValue = nullptr;

    void InitializeUI();
};
