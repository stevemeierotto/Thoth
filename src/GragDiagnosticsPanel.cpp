/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — GRAG Diagnostics Panel Implementation Phase 2.2
 */

#include "GragDiagnosticsPanel.h"
#include <wx/sizer.h>
#include <wx/statline.h>
#include <iomanip>
#include <sstream>

GragDiagnosticsPanel::GragDiagnosticsPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    InitializeUI();
}

void GragDiagnosticsPanel::InitializeUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Alpha (Directional Strength)
    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Directional Strength (Alpha):"), 0, wxALL, 5);
    m_alphaGauge = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL);
    mainSizer->Add(m_alphaGauge, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    m_alphaLabel = new wxStaticText(this, wxID_ANY, "Alpha: 0.00");
    mainSizer->Add(m_alphaLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

    // Magnitude and Scoring Type
    wxFlexGridSizer* infoSizer = new wxFlexGridSizer(2, 5, 10);
    infoSizer->Add(new wxStaticText(this, wxID_ANY, "Magnitude ||G-C||:"), 0, wxALIGN_CENTER_VERTICAL);
    m_magnitudeValue = new wxStaticText(this, wxID_ANY, "0.000");
    infoSizer->Add(m_magnitudeValue, 0, wxALIGN_CENTER_VERTICAL);

    infoSizer->Add(new wxStaticText(this, wxID_ANY, "Scoring Type:"), 0, wxALIGN_CENTER_VERTICAL);
    m_scoringTypeValue = new wxStaticText(this, wxID_ANY, "N/A");
    infoSizer->Add(m_scoringTypeValue, 0, wxALIGN_CENTER_VERTICAL);
    mainSizer->Add(infoSizer, 0, wxALL, 10);

    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

    // Chunks List
    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Retrieved Chunks:"), 0, wxALL, 5);
    m_chunksList = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 200));
    m_chunksList->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 150);
    m_chunksList->AppendTextColumn("Symbol", wxDATAVIEW_CELL_INERT, 150);
    mainSizer->Add(m_chunksList, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
}

void GragDiagnosticsPanel::UpdateDiagnostics(const nlohmann::json& metadata) {
    // Note: This must be called via wxTheApp->CallAfter or inside UI thread
    
    if (metadata.contains("alpha") && metadata["alpha"].is_number()) {
        float alpha = metadata["alpha"].get<float>();
        m_alphaGauge->SetValue(static_cast<int>(alpha * 100));
        std::stringstream ss;
        ss << "Alpha: " << std::fixed << std::setprecision(2) << alpha;
        m_alphaLabel->SetLabel(ss.str());
    }

    if (metadata.contains("direction_magnitude") && metadata["direction_magnitude"].is_number()) {
        float mag = metadata["direction_magnitude"].get<float>();
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << mag;
        m_magnitudeValue->SetLabel(ss.str());
    }

    if (metadata.contains("scoring_type") && metadata["scoring_type"].is_string()) {
        m_scoringTypeValue->SetLabel(metadata["scoring_type"].get<std::string>());
    }

    if (metadata.contains("chunks_retrieved") && metadata["chunks_retrieved"].is_array()) {
        m_chunksList->DeleteAllItems();
        for (const auto& chunk : metadata["chunks_retrieved"]) {
            wxVector<wxVariant> data;
            data.push_back(wxVariant(wxString::FromUTF8(chunk.value("file", "unknown"))));
            data.push_back(wxVariant(wxString::FromUTF8(chunk.value("symbol", ""))));
            m_chunksList->AppendItem(data);
        }
    }

    Layout();
}
