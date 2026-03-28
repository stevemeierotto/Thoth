/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Strategy Panel Implementation
 */

#include "StrategyPanel.h"
#include <wx/sizer.h>
#include <iomanip>
#include <sstream>

StrategyPanel::StrategyPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    InitializeUI();
}

void StrategyPanel::InitializeUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Learned Strategies:"), 0, wxALL, 5);
    
    m_strategyList = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_strategyList->SetMinSize(wxSize(-1, 60));
    m_strategyList->AppendTextColumn("ID", wxDATAVIEW_CELL_INERT, 80);
    m_strategyList->AppendTextColumn("Pattern", wxDATAVIEW_CELL_INERT, 150);
    m_strategyList->AppendTextColumn("Success", wxDATAVIEW_CELL_INERT, 70);
    
    mainSizer->Add(m_strategyList, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
}

void StrategyPanel::UpdateStrategies(const nlohmann::json& strategiesJson) {
    m_strategyList->DeleteAllItems();

    if (!strategiesJson.is_array()) return;

    for (const auto& strat : strategiesJson) {
        wxVector<wxVariant> data;
        data.push_back(wxVariant(wxString::FromUTF8(strat.value("strategy_id", "unknown"))));
        
        std::string pattern = "";
        if (strat.contains("step_pattern") && strat["step_pattern"].is_array()) {
            for (size_t i = 0; i < strat["step_pattern"].size(); ++i) {
                pattern += strat["step_pattern"][i].get<std::string>();
                if (i < strat["step_pattern"].size() - 1) pattern += "->";
            }
        }
        data.push_back(wxVariant(wxString::FromUTF8(pattern)));

        float rate = strat.value("success_rate", 0.0f);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (rate * 100.0f) << "%";
        data.push_back(wxVariant(wxString::FromUTF8(ss.str())));
        
        m_strategyList->AppendItem(data);
    }
    
    Layout();
}
