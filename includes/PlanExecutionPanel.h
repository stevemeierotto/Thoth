#pragma once
#include <wx/wx.h>
#include <wx/dataview.h>
#include <json.hpp>
#include "ChatSessionTypes.h" // For StepStatus if needed, or define it locally

class PlanExecutionPanel : public wxPanel {
public:
    explicit PlanExecutionPanel(wxWindow* parent);

    void ResetPlan(const nlohmann::json& planJson);
    void UpdateStepStatus(const std::string& stepId, const std::string& status);
    void SetExecutionState(const std::string& state);

private:
    void InitializeUI();

    wxStaticText* m_goalLabel = nullptr;
    wxStaticText* m_stateLabel = nullptr;
    wxDataViewListCtrl* m_stepList = nullptr;
};
