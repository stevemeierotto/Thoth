// includes/ChatMessagePanel.h
#pragma once
#include <wx/wx.h>
#include <wx/graphics.h>
#include <json.hpp>

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

    void RenderStructuredContent(const nlohmann::json& j);

    static constexpr int BUBBLE_PADDING = 8;
    static constexpr int BUBBLE_RADIUS = 10;
    static constexpr int MARGIN = 5;
    static constexpr int MAX_BUBBLE_HEIGHT = 280;
};
