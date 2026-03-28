#include "PlanExecutionPanel.h"
#include <wx/sizer.h>
#include <wx/statline.h>

PlanExecutionPanel::PlanExecutionPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    InitializeUI();
}

void PlanExecutionPanel::InitializeUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    m_goalLabel = new wxStaticText(this, wxID_ANY, "Active Goal: None");
    m_goalLabel->SetFont(m_goalLabel->GetFont().Bold());
    mainSizer->Add(m_goalLabel, 0, wxALL, 5);

    m_stateLabel = new wxStaticText(this, wxID_ANY, "State: Idle");
    mainSizer->Add(m_stateLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

    m_stepList = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_stepList->SetMinSize(wxSize(-1, 60));
    m_stepList->AppendTextColumn("Step", wxDATAVIEW_CELL_INERT, 200);
    m_stepList->AppendTextColumn("Status", wxDATAVIEW_CELL_INERT, 80);
    
    mainSizer->Add(m_stepList, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
}

void PlanExecutionPanel::ResetPlan(const nlohmann::json& planJson) {
    m_stepList->DeleteAllItems();
    if (planJson.contains("goal")) {
        m_goalLabel->SetLabel("Goal: " + wxString::FromUTF8(planJson["goal"].get<std::string>()));
    }

    if (planJson.contains("steps") && planJson["steps"].is_array()) {
        for (const auto& step : planJson["steps"]) {
            wxVector<wxVariant> data;
            data.push_back(wxVariant(wxString::FromUTF8(step.value("description", "Untitled Step"))));
            data.push_back(wxVariant("Pending"));
            m_stepList->AppendItem(data);
        }
    }
    Layout();
}

void PlanExecutionPanel::UpdateStepStatus(const std::string& stepId, const std::string& status) {
    // For now, we simple-match by description or just update the current active one
    // In a real implementation, we'd store step IDs in the list
    // This is a simplified version
    for (unsigned int i = 0; i < m_stepList->GetItemCount(); ++i) {
        // Just a placeholder logic: update the first 'Pending' or 'Running' to the new status
        wxVariant val;
        m_stepList->GetValue(val, i, 1);
        if (val.GetString() == "Pending" || val.GetString() == "Running" || val.GetString() == "Active") {
            m_stepList->SetValue(wxVariant(status), i, 1);
            break;
        }
    }
}

void PlanExecutionPanel::SetExecutionState(const std::string& state) {
    m_stateLabel->SetLabel("State: " + state);
}
