/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Experiment Lab Panel Header
 */

#pragma once

#include "panel_presentation_state.h"
#include <wx/wx.h>
#include <wx/dataview.h>
#include <json.hpp>

class ExperimentLabPanel : public wxPanel {
public:
    ExperimentLabPanel(wxWindow* parent);

    void SetPresentationState(Thoth::PanelPresentationState state,
                              const wxString& message = wxEmptyString);
    void UpdateExperiments(const nlohmann::json& experimentsJson);

private:
    wxStaticText* m_statusLabel = nullptr;
    wxDataViewListCtrl* m_experimentList = nullptr;
    void InitializeUI();
};
