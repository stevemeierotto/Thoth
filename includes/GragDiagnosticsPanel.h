/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — GRAG Diagnostics Panel Phase 2.2
 */

#pragma once

#include <wx/wx.h>
#include <wx/gauge.h>
#include <wx/dataview.h>
#include "controller_event.h"

class GragDiagnosticsPanel : public wxPanel {
public:
    GragDiagnosticsPanel(wxWindow* parent);

    void UpdateDiagnostics(const nlohmann::json& metadata);

private:
    wxStaticText* m_alphaLabel;
    wxGauge*      m_alphaGauge;
    wxStaticText* m_magnitudeValue;
    wxStaticText* m_scoringTypeValue;
    wxDataViewListCtrl* m_chunksList;

    void InitializeUI();
};
