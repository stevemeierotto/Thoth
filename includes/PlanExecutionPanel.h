#pragma once
#include <wx/wx.h>
#include <wx/dataview.h>
#include <functional>
#include <json.hpp>
#include <optional>
#include <string>
#include "ChatSessionTypes.h" // For StepStatus if needed, or define it locally
#include "cm1_telemetry.h"
#include "operation_result.h"

class PlanExecutionPanel : public wxPanel {
public:
    explicit PlanExecutionPanel(wxWindow* parent);

    void ResetPlan(const nlohmann::json& planJson);
    void UpdateStepStatus(const std::string& stepId, const std::string& status);
    void SetExecutionState(const std::string& state);
    /**
     * Phase A — drop executive steps/labels from a prior chat so Observability
     * can bind to the newly activated session.
     */
    void ClearForSessionSwitch();
    /** CSG-A — host session goal when no executive plan steps are shown. */
    void SetSessionGoalDisplay(const std::string& goal);
    void SetFeedbackHandlers(
        std::function<Thoth::OperationResult(const Thoth::Cm1FeedbackSubmission&)> submit,
        std::function<std::optional<Thoth::Cm1UserFeedback>(const std::string&)> latest);
    /** Latest goal-synthesis answer that can receive feedback. */
    void OfferSynthesisAnswer(const std::string& plan_id,
                              const std::string& step_id,
                              const std::string& generation_id,
                              const std::string& answer_text);

private:
    void InitializeUI();
    void ClearSynthesisOffer();
    void OnFeedback(wxCommandEvent& event);

    wxStaticText* m_goalLabel = nullptr;
    wxStaticText* m_stateLabel = nullptr;
    wxButton* m_feedbackButton = nullptr;
    wxDataViewListCtrl* m_stepList = nullptr;
    std::function<Thoth::OperationResult(const Thoth::Cm1FeedbackSubmission&)> m_submitFeedback;
    std::function<std::optional<Thoth::Cm1UserFeedback>(const std::string&)> m_latestFeedback;
    std::string m_planId;
    std::string m_stepId;
    std::string m_generationId;
    std::string m_answerText;
};
