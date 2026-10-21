#include "PlanExecutionPanel.h"
#include "Cm1FeedbackDialog.h"
#include "plan_execution_display.h"
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

    m_feedbackButton = new wxButton(this, wxID_ANY, "Feedback");
    m_feedbackButton->Hide();
    m_feedbackButton->Bind(wxEVT_BUTTON, &PlanExecutionPanel::OnFeedback, this);
    mainSizer->Add(m_feedbackButton, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

    m_stepList = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_stepList->SetMinSize(wxSize(-1, 60));
    m_stepList->AppendTextColumn("Step", wxDATAVIEW_CELL_INERT, 200);
    m_stepList->AppendTextColumn("Status", wxDATAVIEW_CELL_INERT, 80);
    
    mainSizer->Add(m_stepList, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
}

void PlanExecutionPanel::ResetPlan(const nlohmann::json& planJson) {
    ClearSynthesisOffer();
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

void PlanExecutionPanel::SetFeedbackHandlers(
    std::function<Thoth::OperationResult(const Thoth::Cm1FeedbackSubmission&)> submit,
    std::function<std::optional<Thoth::Cm1UserFeedback>(const std::string&)> latest) {
    m_submitFeedback = std::move(submit);
    m_latestFeedback = std::move(latest);
}

void PlanExecutionPanel::ClearSynthesisOffer() {
    m_planId.clear();
    m_stepId.clear();
    m_generationId.clear();
    m_answerText.clear();
    if (m_feedbackButton) {
        m_feedbackButton->Hide();
    }
}

void PlanExecutionPanel::OfferSynthesisAnswer(const std::string& plan_id,
                                              const std::string& step_id,
                                              const std::string& generation_id,
                                              const std::string& answer_text) {
    if (plan_id.empty() || step_id.empty() || generation_id.empty() || answer_text.empty()) {
        return;
    }
    m_planId = plan_id;
    m_stepId = step_id;
    m_generationId = generation_id;
    m_answerText = answer_text;
    if (m_feedbackButton) {
        m_feedbackButton->SetLabel("Feedback");
        m_feedbackButton->Show();
        Layout();
    }
}

void PlanExecutionPanel::OnFeedback(wxCommandEvent&) {
    if (!m_submitFeedback || m_answerText.empty()) {
        return;
    }
    const std::string answer_id = Thoth::makeCm1AnswerId(m_planId, m_stepId, m_generationId);
    std::optional<Thoth::Cm1UserFeedback> existing;
    if (m_latestFeedback) {
        existing = m_latestFeedback(answer_id);
    }
    const auto submitted = ShowCm1FeedbackDialog(
        this, m_answerText, existing ? &*existing : nullptr);
    if (!submitted) {
        return;
    }
    Thoth::Cm1FeedbackSubmission request = *submitted;
    request.answer_id = answer_id;
    request.answer_text = m_answerText;
    const Thoth::OperationResult result = m_submitFeedback(request);
    const int style = result.success ? (wxOK | wxICON_INFORMATION) : (wxOK | wxICON_WARNING);
    wxMessageBox(wxString::FromUTF8(result.user_message), "Answer feedback", style, this);
}

void PlanExecutionPanel::ClearForSessionSwitch() {
    ClearSynthesisOffer();
    if (m_stepList) {
        m_stepList->DeleteAllItems();
    }
    if (m_goalLabel) {
        m_goalLabel->SetLabel(
            wxString::FromUTF8(Thoth::PlanExecutionDisplay::formatActiveGoalLabel("")));
    }
    if (m_stateLabel) {
        m_stateLabel->SetLabel(wxString::FromUTF8(Thoth::PlanExecutionDisplay::kIdleState));
    }
    Layout();
}

void PlanExecutionPanel::SetSessionGoalDisplay(const std::string& goal) {
    if (!m_goalLabel || !m_stateLabel) {
        return;
    }
    // Do not clobber an in-flight executive plan view on the *current* session.
    // ActivateSession must ClearForSessionSwitch() first so prior-chat steps
    // cannot pin a stale Observability goal.
    const std::size_t stepCount =
        m_stepList ? static_cast<std::size_t>(m_stepList->GetItemCount()) : 0;
    if (!Thoth::PlanExecutionDisplay::canApplySessionGoalDisplay(stepCount)) {
        return;
    }
    m_goalLabel->SetLabel(
        wxString::FromUTF8(Thoth::PlanExecutionDisplay::formatActiveGoalLabel(goal)));
    if (goal.empty()) {
        m_stateLabel->SetLabel(wxString::FromUTF8(Thoth::PlanExecutionDisplay::kIdleState));
    } else {
        m_stateLabel->SetLabel(
            wxString::FromUTF8(Thoth::PlanExecutionDisplay::kSessionChatRetrievalState));
    }
    Layout();
}
