/*
 * Copyright (c) 2026 Steve Meierotto
 *
 * Thoth — compact CM-1 feedback dialog for one goal-synthesis answer
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#include "Cm1FeedbackDialog.h"

#include <wx/slider.h>
#include <wx/statline.h>

namespace {

class Cm1FeedbackDialog : public wxDialog {
public:
    Cm1FeedbackDialog(wxWindow* parent,
                      const std::string& answer_preview,
                      const Thoth::Cm1UserFeedback* existing)
        : wxDialog(parent, wxID_ANY, "Answer feedback", wxDefaultPosition, wxSize(420, 360)) {
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* preview = new wxStaticText(
            this, wxID_ANY, wxString::FromUTF8(answer_preview));
        preview->Wrap(380);
        root->Add(preview, 0, wxALL, 8);
        root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

        wxArrayString thumbs;
        thumbs.Add("None");
        thumbs.Add("Thumbs up");
        thumbs.Add("Thumbs down");
        m_thumbs = new wxRadioBox(this, wxID_ANY, "Thumbs", wxDefaultPosition, wxDefaultSize,
                                  thumbs, 1, wxRA_SPECIFY_ROWS);
        m_thumbs->SetSelection(0);
        root->Add(m_thumbs, 0, wxEXPAND | wxALL, 8);

        m_includeRating = new wxCheckBox(this, wxID_ANY, "Include quality rating (1-10)");
        root->Add(m_includeRating, 0, wxLEFT | wxRIGHT, 8);
        m_slider = new wxSlider(this, wxID_ANY, 5, 1, 10, wxDefaultPosition, wxDefaultSize,
                                wxSL_HORIZONTAL | wxSL_LABELS | wxSL_AUTOTICKS);
        m_slider->Enable(false);
        root->Add(m_slider, 0, wxEXPAND | wxALL, 8);

        root->Add(new wxStaticText(this, wxID_ANY, "Explanation (optional)"), 0, wxLEFT | wxRIGHT, 8);
        m_explanation = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 80),
                                       wxTE_MULTILINE);
        root->Add(m_explanation, 1, wxEXPAND | wxALL, 8);

        auto* buttons = new wxBoxSizer(wxHORIZONTAL);
        buttons->AddStretchSpacer(1);
        auto* cancel = new wxButton(this, wxID_CANCEL, "Cancel");
        auto* submit = new wxButton(this, wxID_OK, "Submit");
        cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });
        submit->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_OK); });
        buttons->Add(cancel, 0, wxRIGHT, 6);
        buttons->Add(submit, 0);
        root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

        if (existing) {
            if (existing->thumbs_status == "up") m_thumbs->SetSelection(1);
            else if (existing->thumbs_status == "down") m_thumbs->SetSelection(2);
            if (existing->quality_rating_status == "present") {
                m_includeRating->SetValue(true);
                m_slider->Enable(true);
                m_slider->SetValue(existing->quality_rating);
            }
            m_explanation->SetValue(wxString::FromUTF8(existing->explanation));
        }

        m_includeRating->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
            m_slider->Enable(m_includeRating->GetValue());
        });
        SetSizerAndFit(root);
        CentreOnParent();
    }

    Thoth::Cm1FeedbackSubmission submission() const {
        Thoth::Cm1FeedbackSubmission out;
        const int choice = m_thumbs->GetSelection();
        if (choice == 1) out.thumbs_status = "up";
        else if (choice == 2) out.thumbs_status = "down";
        else out.thumbs_status = "absent";
        out.include_quality_rating = m_includeRating->GetValue();
        out.quality_rating = m_slider->GetValue();
        out.explanation = m_explanation->GetValue().ToStdString();
        return out;
    }

private:
    wxRadioBox* m_thumbs = nullptr;
    wxCheckBox* m_includeRating = nullptr;
    wxSlider* m_slider = nullptr;
    wxTextCtrl* m_explanation = nullptr;
};

std::string previewOf(const std::string& text) {
    constexpr std::size_t kMax = 180;
    if (text.size() <= kMax) return text;
    return text.substr(0, kMax) + "...";
}

} // namespace

std::optional<Thoth::Cm1FeedbackSubmission> ShowCm1FeedbackDialog(
    wxWindow* parent,
    const std::string& answer_preview,
    const Thoth::Cm1UserFeedback* existing) {
    Cm1FeedbackDialog dialog(parent, previewOf(answer_preview), existing);
    if (dialog.ShowModal() != wxID_OK) {
        return std::nullopt;
    }
    return dialog.submission();
}
