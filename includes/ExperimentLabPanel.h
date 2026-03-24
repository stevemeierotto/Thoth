/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Experiment Lab Panel Header
 */

#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <json.hpp>

class ExperimentLabPanel : public wxPanel {
public:
    ExperimentLabPanel(wxWindow* parent);

    void UpdateExperiments(const nlohmann::json& experimentsJson);

private:
    wxDataViewListCtrl* m_experimentList = nullptr;
    void InitializeUI();
};
