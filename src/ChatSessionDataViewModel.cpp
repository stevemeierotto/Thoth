 #include "ChatSessionDataViewModel.h"
 #include "ChatSessionTypes.h" // For Thoth::ChatSession
 #include "MainFrame.h" // Needed for MainFrame* for explicit conversion/casting

 // --- Data Model for Chat Sessions ---

 ChatSessionDataViewModel::ChatSessionDataViewModel(std::vector<Thoth::ChatSession>* sessions)
     : m_sessions(sessions) {}
      unsigned int ChatSessionDataViewModel::GetColumnCount() const {
      return Col_Max;
 }

 wxString ChatSessionDataViewModel::GetColumnType(unsigned int col) const {
     if (col == Col_Title) {
         return "string";
     }
     return "string";
}

 void ChatSessionDataViewModel::GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const {
    wxASSERT(item.IsOk());
     auto* session = static_cast<Thoth::ChatSession*>(item.GetID());
     if (!session || !m_sessions) {
         return;
     }

     if (col == Col_Title) {
         variant = wxString::FromUTF8(session->title);
     }
 }

bool ChatSessionDataViewModel::SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col) {
     // Read-only model
     return false;
 }

wxDataViewItem ChatSessionDataViewModel::GetParent(const wxDataViewItem& item) const {
    // This is a flat list, no parents
     return wxDataViewItem(nullptr);
 }

bool ChatSessionDataViewModel::IsContainer(const wxDataViewItem& item) const  {
     // A wxDataViewItem is a container if it has children
     if (!item.IsOk()) {
         return true; // Root is a container
    }
     return false; // Sessions are not containers
 }

 unsigned int ChatSessionDataViewModel::GetChildren(const wxDataViewItem& item, wxDataViewItemArray& children) const  {
     if (!m_sessions) {
         return 0;
     }

     if (item.IsOk()) {
         return 0; // Sessions don't have children
     }

     // The children of the invisible root are the sessions
     for (auto& session : *m_sessions) {
         children.Add(wxDataViewItem(static_cast<void*>(&session)));
     }
     return m_sessions->size();
 }

 // Helper to get the session index from an item
 int ChatSessionDataViewModel::GetSessionIndex(const wxDataViewItem& item) const {
     if (!item.IsOk() || !m_sessions) {
         return -1;
     }
     auto* target = static_cast<Thoth::ChatSession*>(item.GetID());
     for (size_t i = 0; i < m_sessions->size(); ++i) {
         if (&(*m_sessions)[i] == target) {
             return static_cast<int>(i);
         }
     }
     return -1;
 }

 // Helper to get the item from a session index
 wxDataViewItem ChatSessionDataViewModel::GetItemFromIndex(int sessionIndex) const {
     if (sessionIndex < 0 || !m_sessions || sessionIndex >= static_cast<int>(m_sessions->size())) {
         return wxDataViewItem(nullptr);
     }
    return wxDataViewItem(static_cast<void*>(&(*m_sessions)[static_cast<size_t>(sessionIndex)]));
}

 // Private members are declared in the header file.

