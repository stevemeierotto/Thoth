/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — GRAG Diagnostics Panel Implementation Phase 2.2 / Plan N5
 */

#include "GragDiagnosticsPanel.h"
#include "grag_diagnostics_display.h"
#include <wx/sizer.h>
#include <wx/statline.h>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <optional>
#include <iostream>

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
    m_alphaLabel = new wxStaticText(this, wxID_ANY, "N/A (chat)");
    mainSizer->Add(m_alphaLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

    // Magnitude and Mode (Plan N5 — Mode carries interpretation; magnitude stays numeric)
    wxFlexGridSizer* infoSizer = new wxFlexGridSizer(2, 5, 10);
    infoSizer->Add(new wxStaticText(this, wxID_ANY, "Magnitude ||G-C||:"), 0, wxALIGN_CENTER_VERTICAL);
    m_magnitudeValue = new wxStaticText(this, wxID_ANY, "0.000");
    infoSizer->Add(m_magnitudeValue, 0, wxALIGN_CENTER_VERTICAL);

    infoSizer->Add(new wxStaticText(this, wxID_ANY, "Mode:"), 0, wxALIGN_CENTER_VERTICAL);
    m_scoringTypeValue = new wxStaticText(this, wxID_ANY, "Conversational");
    infoSizer->Add(m_scoringTypeValue, 0, wxALIGN_CENTER_VERTICAL);
    mainSizer->Add(infoSizer, 0, wxALL, 10);

    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

    // Chunks List
    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Retrieved Chunks:"), 0, wxALL, 5);
    m_chunksList = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_chunksList->SetMinSize(wxSize(-1, 60));
    m_chunksList->AppendTextColumn("Final Score", wxDATAVIEW_CELL_INERT, 100);
    m_chunksList->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 150);
    m_chunksList->AppendTextColumn("Symbol", wxDATAVIEW_CELL_INERT, 150);
    mainSizer->Add(m_chunksList, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
}

static std::optional<float> ExtractFinalScore(const nlohmann::json& chunk) {
    try {
        if (chunk.is_object()) {
            if (chunk.contains("final_score") && chunk["final_score"].is_number()) {
                return chunk["final_score"].get<float>();
            }
            if (chunk.contains("score") && chunk["score"].is_number()) {
                return chunk["score"].get<float>();
            }
        }
    } catch (...) {}
    return std::nullopt;
}

static nlohmann::json ExtractDiagnosticsPayload(const nlohmann::json& metadata) {
    try {
        if (!metadata.is_object()) return metadata;

        if (metadata.contains("diagnostics") && metadata["diagnostics"].is_object()) {
            return metadata["diagnostics"];
        }

        if (metadata.contains("result") && metadata["result"].is_object()) {
            const auto& result = metadata["result"];
            if (result.contains("diagnostics") && result["diagnostics"].is_object()) {
                return result["diagnostics"];
            }
        }
    } catch (...) {}

    return metadata;
}

void GragDiagnosticsPanel::UpdateDiagnostics(const nlohmann::json& metadata) {
    if (!m_chunksList || !m_alphaGauge || !m_alphaLabel || !m_magnitudeValue || !m_scoringTypeValue) {
        return;
    }

    try {
        const nlohmann::json diagnostics = ExtractDiagnosticsPayload(metadata);
        
        if (diagnostics.is_object()) {
            std::string scoringType;
            if (diagnostics.contains("scoring_type") && diagnostics["scoring_type"].is_string()) {
                scoringType = diagnostics["scoring_type"].get<std::string>();
            }

            const bool conversational =
                Thoth::GragDiagnosticsDisplay::isConversationalNoGoalDirection(scoringType);

            float alpha = 0.0f;
            if (diagnostics.contains("alpha") && diagnostics["alpha"].is_number()) {
                alpha = diagnostics["alpha"].get<float>();
            }

            m_alphaLabel->SetLabel(
                Thoth::GragDiagnosticsDisplay::formatAlphaLabel(alpha, conversational));
            if (conversational) {
                m_alphaGauge->SetValue(0);
            } else {
                m_alphaGauge->SetValue(std::clamp(static_cast<int>(alpha * 100), 0, 100));
            }

            if (diagnostics.contains("direction_magnitude") && diagnostics["direction_magnitude"].is_number()) {
                const float mag = diagnostics["direction_magnitude"].get<float>();
                m_magnitudeValue->SetLabel(
                    Thoth::GragDiagnosticsDisplay::formatMagnitudeLabel(mag));
            }

            m_scoringTypeValue->SetLabel(
                Thoth::GragDiagnosticsDisplay::formatModeLabel(scoringType));

            if (diagnostics.contains("breakdowns") && diagnostics["breakdowns"].is_array()) {
                m_chunksList->DeleteAllItems();
                const auto& breakdowns = diagnostics["breakdowns"];
                size_t count = std::min(breakdowns.size(), static_cast<size_t>(10));
                
                for (size_t i = 0; i < count; ++i) {
                    const auto& chunk = breakdowns[i];
                    if (!chunk.is_object()) continue;

                    wxVector<wxVariant> data;
                    std::string scoreLabel = Thoth::GragDiagnosticsDisplay::formatFinalScoreLabel(
                        ExtractFinalScore(chunk));
                    data.push_back(wxVariant(wxString::FromUTF8(scoreLabel)));

                    std::string fileName = "unknown";
                    if (chunk.contains("file_name") && chunk["file_name"].is_string()) {
                        fileName = chunk["file_name"].get<std::string>();
                    } else if (chunk.contains("file") && chunk["file"].is_string()) {
                        fileName = chunk["file"].get<std::string>();
                    }
                    if (fileName.size() > 512) fileName = fileName.substr(0, 509) + "...";
                    data.push_back(wxVariant(wxString::FromUTF8(fileName)));

                    std::string symbol = "";
                    if (chunk.contains("symbol") && chunk["symbol"].is_string()) {
                        symbol = chunk["symbol"].get<std::string>();
                    }
                    if (symbol.size() > 256) symbol = symbol.substr(0, 253) + "...";
                    data.push_back(wxVariant(wxString::FromUTF8(symbol)));
                    
                    m_chunksList->AppendItem(data);
                }
            } else if (diagnostics.contains("chunks_retrieved") && diagnostics["chunks_retrieved"].is_array()) {
                m_chunksList->DeleteAllItems();
                const auto& chunks = diagnostics["chunks_retrieved"];
                size_t count = std::min(chunks.size(), static_cast<size_t>(10));
                for (size_t i = 0; i < count; ++i) {
                    const auto& chunk = chunks[i];
                    if (!chunk.is_object()) continue;

                    wxVector<wxVariant> data;
                    std::string scoreLabel = Thoth::GragDiagnosticsDisplay::formatFinalScoreLabel(
                        ExtractFinalScore(chunk));
                    data.push_back(wxVariant(wxString::FromUTF8(scoreLabel)));
                    
                    std::string f = chunk.value("file", "unknown");
                    if (f.size() > 512) f = f.substr(0, 509) + "...";
                    data.push_back(wxVariant(wxString::FromUTF8(f)));
                    
                    std::string s = chunk.value("symbol", "");
                    if (s.size() > 256) s = s.substr(0, 253) + "...";
                    data.push_back(wxVariant(wxString::FromUTF8(s)));
                    
                    m_chunksList->AppendItem(data);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[GragDiagnosticsPanel] Exception in UpdateDiagnostics: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "[GragDiagnosticsPanel] Unknown exception in UpdateDiagnostics\n";
    }

    Layout();
}
