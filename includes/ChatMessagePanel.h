// includes/ChatMessagePanel.h
#pragma once
#include <wx/wx.h>
#include <wx/graphics.h>

class ChatMessagePanel : public wxPanel {
public:
    ChatMessagePanel(wxWindow* parent, const wxString& message, bool isUser);

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnCopy(wxCommandEvent& event);

    wxString m_message;
    bool m_isUser;
    wxTextCtrl* m_textCtrl;

    static constexpr int BUBBLE_PADDING = 12;
    static constexpr int BUBBLE_RADIUS = 12;
    static constexpr int MARGIN = 10;
};
