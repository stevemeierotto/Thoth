// src/ChatMessagePanel.cpp
#include "ChatMessagePanel.h"
#include <memory>
#include <wx/graphics.h>
#include <wx/stattext.h>
#include <wx/sizer.h>

ChatMessagePanel::ChatMessagePanel(wxWindow* parent, const wxString& message, bool isUser)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTRANSPARENT_WINDOW),
      m_message(message), m_isUser(isUser)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Content with wrapping
    wxStaticText* content = new wxStaticText(this, wxID_ANY, m_message);
    content->SetForegroundColour(m_isUser ? *wxWHITE : *wxBLACK);
    content->Wrap(500); // Fixed max width for now, will improve later
    
    // Position within the panel
    sizer->Add(content, 0, (m_isUser ? wxALIGN_RIGHT : wxALIGN_LEFT) | wxALL, BUBBLE_PADDING + MARGIN);
    
    SetSizer(sizer);
    Layout();
    
    // Final size including bubble padding
    SetMinSize(wxSize(-1, GetBestSize().y + MARGIN));
    
    Bind(wxEVT_PAINT, &ChatMessagePanel::OnPaint, this);
    Bind(wxEVT_SIZE, &ChatMessagePanel::OnSize, this);
}

void ChatMessagePanel::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc) return;

    wxSize size = GetSize();
    wxWindowList& children = GetChildren();
    if (children.empty()) return;
    
    wxWindow* content = children.front();
    wxRect contentRect = content->GetRect();
    
    // The bubble rect
    wxRect bubbleRect = contentRect;
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
