/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Strategy Panel Header
 */

#pragma once

#include "panel_presentation_state.h"
#include <wx/wx.h>
#include <wx/dataview.h>
#include <json.hpp>

class StrategyPanel : public wxPanel {
public:
    StrategyPanel(wxWindow* parent);

    void SetPresentationState(Thoth::PanelPresentationState state,
                              const wxString& message = wxEmptyString);
    void UpdateStrategies(const nlohmann::json& strategiesJson);

private:
    wxStaticText* m_statusLabel = nullptr;
    wxDataViewListCtrl* m_strategyList = nullptr;
    void InitializeUI();
};
