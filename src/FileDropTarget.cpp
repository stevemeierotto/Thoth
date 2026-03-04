#include "FileDropTarget.h"
#include "MainFrame.h" // For MainFrame::HandleFileDrop

// --- File Drop Target for RAG ---

FileDropTarget::FileDropTarget(MainFrame* owner) : m_owner(owner) {}

bool FileDropTarget::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) {
    if (!m_owner) {
        return false;
    }
    // x and y are unused in this implementation, suppress warnings
    (void)x;
    (void)y;
    return m_owner->HandleFileDrop(filenames);
}
