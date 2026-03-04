#pragma once

#include <wx/dnd.h>
#include <wx/string.h> // For wxArrayString

// Forward declarations
class MainFrame; // MainFrame needs to be visible to FileDropTarget

class FileDropTarget final : public wxFileDropTarget {
public:
    explicit FileDropTarget(MainFrame* owner);

    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override;

private:
    MainFrame* m_owner;
};
