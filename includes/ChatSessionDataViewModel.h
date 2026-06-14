#pragma once

#include "ChatSessionTypes.h" // For Thoth::ChatSession
#include <wx/dataview.h>
#include <vector>

class MainFrame; // Still need MainFrame forward declaration for explicit constructor argument

class ChatSessionDataViewModel final : public wxDataViewModel {
public:
    enum {
        Col_Session,
        Col_Max
    };

    explicit ChatSessionDataViewModel(std::vector<Thoth::ChatSession>* sessions);

    // Required overrides from wxDataViewModel
    unsigned int GetColumnCount() const override;
    wxString GetColumnType(unsigned int col) const override;
    void GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const override;
    bool SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col) override;
    wxDataViewItem GetParent(const wxDataViewItem& item) const override;
    bool IsContainer(const wxDataViewItem& item) const override;
    unsigned int GetChildren(const wxDataViewItem& item, wxDataViewItemArray& children) const override;

    // Custom helper methods
    int GetSessionIndex(const wxDataViewItem& item) const;
    wxDataViewItem GetItemFromIndex(int sessionIndex) const;

private:
    std::vector<Thoth::ChatSession>* m_sessions;
};
