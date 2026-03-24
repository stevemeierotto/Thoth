/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Strategy Panel Header
 */

#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <json.hpp>

class StrategyPanel : public wxPanel {
public:
    StrategyPanel(wxWindow* parent);

    void UpdateStrategies(const nlohmann::json& strategiesJson);

private:
    wxDataViewListCtrl* m_strategyList = nullptr;
    void InitializeUI();
};
