#include "ChatSessionRenderer.h"
#include <wx/dc.h>
#include <wx/settings.h>
#include <ctime>

ChatSessionRenderer::ChatSessionRenderer()
    : wxDataViewCustomRenderer("void*", wxDATAVIEW_CELL_INERT) {}

bool ChatSessionRenderer::Render(wxRect rect, wxDC* dc, int state) {
    if (!m_session) return true;

    // Background and selection
    if (state & wxDATAVIEW_CELL_SELECTED) {
        dc->SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
        dc->SetPen(*wxTRANSPARENT_PEN);
        dc->DrawRectangle(rect);
        dc->SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
    } else {
        dc->SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }

    // Font setup
    wxFont baseFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    
    wxFont titleFont = baseFont;
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    
    wxFont subFont = baseFont;
    int subSize = std::max(7, baseFont.GetPointSize() - 1);
    subFont.SetPointSize(subSize);

    // 1. Draw Title
    dc->SetFont(titleFont);
    wxString title = wxString::FromUTF8(m_session->title);
    dc->DrawText(title, rect.x + 5, rect.y + 3);

    // 2. Draw Date (smaller, lighter)
    dc->SetFont(subFont);
    std::time_t t = m_session->updatedAtMs / 1000;
    std::tm* tm = std::localtime(&t);
    char dateBuf[64];
    std::strftime(dateBuf, sizeof(dateBuf), "%m/%d %H:%M", tm);
    wxString dateStr = wxString::FromUTF8(dateBuf);
    
    wxSize dateSize = dc->GetTextExtent(dateStr);
    if (!(state & wxDATAVIEW_CELL_SELECTED)) {
        dc->SetTextForeground(wxColour(140, 140, 140));
    }
    dc->DrawText(dateStr, rect.x + rect.width - dateSize.x - 5, rect.y + 5);

    // 3. Draw Preview (last message)
    dc->SetFont(subFont);
    wxString preview = "No messages";
    if (!m_session->messages.empty()) {
        preview = wxString::FromUTF8(m_session->messages.back().content);
        preview.Replace("\n", " ");
        if (preview.Length() > 60) {
            preview = preview.Left(57) + "...";
        }
    }
    
    if (!(state & wxDATAVIEW_CELL_SELECTED)) {
        dc->SetTextForeground(wxColour(110, 110, 110));
    }
    dc->DrawText(preview, rect.x + 5, rect.y + 21);

    return true;
}

wxSize ChatSessionRenderer::GetSize() const {
    return wxSize(-1, 44); 
}

bool ChatSessionRenderer::SetValue(const wxVariant& value) {
    m_session = static_cast<Thoth::ChatSession*>(value.GetVoidPtr());
    return true;
}

bool ChatSessionRenderer::GetValue(wxVariant& value) const {
    value = static_cast<void*>(m_session);
    return true;
}
