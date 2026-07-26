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
    /** Phase 6 — diagnostics-only last SSE event age (Engine mode). */
    void UpdateLastEventAgeLabel(const wxString& label);

private:
    wxStaticText* m_layerHintLabel = nullptr;
    wxStaticText* m_scopeLabel = nullptr;
    wxStaticText* m_requestIdLabel = nullptr;
    wxStaticText* m_groundedLabel = nullptr;
    wxStaticText* m_warmMemoryNote = nullptr;
    wxStaticText* m_alphaLabel;
    wxGauge*      m_alphaGauge;
    wxStaticText* m_magnitudeValue;
    wxStaticText* m_scoringTypeValue;
    wxStaticText* m_lastEventLabel = nullptr;
    wxDataViewListCtrl* m_chunksList;

    void InitializeUI();
};
