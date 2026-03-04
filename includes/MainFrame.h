// includes/MainFrame.h
//
// Declares the main wxWidgets frame for Thoth Control Panel.
// Handles sidebar, chat display, input box, and buttons.

#pragma once
#include "AgentInterface.h"
#include <wx/aui/aui.h>
#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/scrolwin.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "FileDropTarget.h"
#include "ChatSessionDataViewModel.h"
#include "ChatSessionTypes.h" // Contains ChatMessage and ChatSession structs

class MainFrame : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;

    // Public method for FileDropTarget to call
    bool HandleFileDrop(const wxArrayString& filenames);

private:
    // AUI Manager
    wxAuiManager m_auiManager;

    // UI elements
    wxDataViewCtrl* m_chatList = nullptr;
    wxButton* m_newChatButton = nullptr;
    wxButton* m_deleteChatButton = nullptr;
    wxScrolledWindow* m_chatContainer = nullptr;
    wxBoxSizer* m_chatSizer = nullptr;
    wxTextCtrl* m_inputCtrl = nullptr;
    wxButton* m_sendButton = nullptr;
    wxButton* m_traceButton = nullptr;

    wxStaticText* m_typingIndicator = nullptr;

    wxStaticText* m_ragFileSlot1 = nullptr;
    wxStaticText* m_ragFileSlot2 = nullptr;
    wxStaticText* m_ragFileSlot3 = nullptr;
    wxStaticText* m_ragFileSlot4 = nullptr;

    // Data model for the chat list
    wxObjectDataPtr<ChatSessionDataViewModel> m_chatListModel;

    wxString m_currentChatTitle;
    std::string m_sessionId;
    std::uint64_t m_requestCounter = 0;
    std::string m_chatSessionsPath;
    std::vector<Thoth::ChatSession> m_sessions;
    int m_activeSessionIndex = -1;
    std::unordered_map<std::string, std::string> m_requestToSession;

    std::unique_ptr<AgentInterface> agent;

    static std::int64_t NowMs();
    std::string BuildSessionTitle(const wxString& firstUserMessage) const;
    std::string BuildMemorySummary(const Thoth::ChatSession& session) const;
    void LoadChatSessions();
    void SaveChatSessions() const;
    void CreateNewSession(const std::string& title = "New Chat");
    void RefreshChatList();
    void RefreshRagPanel();
    void RenderSession(std::size_t sessionIndex);
    void ActivateSession(std::size_t sessionIndex);
    bool SyncAgentMemoryFromActiveSession();

    // Event handlers
    void OnSend(wxCommandEvent& evt);
    void OnShowDecisionTrace(wxCommandEvent& evt);
    void OnChatSelected(wxDataViewEvent& evt);
    void OnNewChat(wxCommandEvent& evt);
    void OnDeleteChat(wxCommandEvent& evt);
};
