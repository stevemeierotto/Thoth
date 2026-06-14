#pragma once

#include <wx/dataview.h>
#include "ChatSessionTypes.h"

class ChatSessionRenderer : public wxDataViewCustomRenderer {
public:
    ChatSessionRenderer();

    // Required overrides
    bool Render(wxRect rect, wxDC* dc, int state) override;
    wxSize GetSize() const override;
    bool SetValue(const wxVariant& value) override;
    bool GetValue(wxVariant& value) const override;

private:
    Thoth::ChatSession* m_session = nullptr;
};
