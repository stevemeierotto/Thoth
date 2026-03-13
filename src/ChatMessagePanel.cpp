// src/ChatMessagePanel.cpp
#include "ChatMessagePanel.h"
#include <memory>
#include <algorithm>
#include <wx/graphics.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/clipbrd.h>

ChatMessagePanel::ChatMessagePanel(wxWindow* parent, const wxString& message, bool isUser)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTRANSPARENT_WINDOW),
      m_message(message), m_isUser(isUser)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Use a read-only text control to allow selection and copying
    // Added wxTE_NO_VSCROLL to avoid nested scrollbars which often cause GTK issues
    m_textCtrl = new wxTextCtrl(this, wxID_ANY, m_message, wxDefaultPosition, wxDefaultSize, 
                                 wxTE_MULTILINE | wxTE_READONLY | wxNO_BORDER | wxTE_RICH2 | wxTE_NO_VSCROLL);
    m_textCtrl->SetBackgroundColour(m_isUser ? wxColour(0, 120, 215) : wxColour(240, 240, 240));
    m_textCtrl->SetForegroundColour(m_isUser ? *wxWHITE : *wxBLACK);
    
    // Estimate height more safely. 
    // On Linux, GetNumberOfLines() might be 1 before first paint.
    std::string stdMsg = m_message.ToStdString();
    int lineCount = std::count(stdMsg.begin(), stdMsg.end(), '\n') + 1;
    
    // If text is long and doesn't have newlines, it will wrap. 
    if (m_message.Length() > 60 && lineCount == 1) {
        lineCount = (m_message.Length() / 50) + 1;
    }

    int height = m_textCtrl->GetCharHeight() * (lineCount + 1) + 20;
    m_textCtrl->SetMinSize(wxSize(350, height)); // Narrower min width
    
    // Position within the panel - allow to expand.
    // Removed alignment flags because wxEXPAND overrides them in a box sizer, 
    // which was causing a wxWidgets assertion failure.
    sizer->Add(m_textCtrl, 0, wxALL | wxEXPAND, BUBBLE_PADDING + MARGIN);

    // Optional: Add a small copy button
    wxButton* copyBtn = new wxButton(this, wxID_ANY, "Copy", wxDefaultPosition, wxSize(50, 20));
    copyBtn->SetWindowStyle(wxBORDER_NONE);
    sizer->Add(copyBtn, 0, (m_isUser ? wxALIGN_RIGHT : wxALIGN_LEFT) | wxLEFT | wxRIGHT | wxBOTTOM, MARGIN);

    Bind(wxEVT_BUTTON, &ChatMessagePanel::OnCopy, this, copyBtn->GetId());
    
    SetSizer(sizer);
    Layout();
    
    // Final size including bubble padding
    SetMinSize(wxSize(-1, GetBestSize().y + MARGIN));
    
    Bind(wxEVT_PAINT, &ChatMessagePanel::OnPaint, this);
    Bind(wxEVT_SIZE, &ChatMessagePanel::OnSize, this);
}

void ChatMessagePanel::OnCopy(wxCommandEvent& WXUNUSED(event)) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_message));
        wxTheClipboard->Close();
        wxLogStatus("Message copied to clipboard.");
    }
}

void ChatMessagePanel::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc) return;

    wxSize size = GetSize();
    
    // Find the text control among children
    wxRect bubbleRect = m_textCtrl->GetRect();
    bubbleRect.Inflate(BUBBLE_PADDING, BUBBLE_PADDING);

    // Modern flat colors
    wxColour bubbleColor = m_isUser ? wxColour(0, 120, 215) : wxColour(240, 240, 240);
    gc->SetBrush(wxBrush(bubbleColor));
    gc->SetPen(*wxTRANSPARENT_PEN);
    
    gc->DrawRoundedRectangle(bubbleRect.x, bubbleRect.y, bubbleRect.width, bubbleRect.height, BUBBLE_RADIUS);
}

void ChatMessagePanel::OnSize(wxSizeEvent& event) {
    Refresh();
    event.Skip();
}
