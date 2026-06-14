// includes/MainFrame.h
//
// Declares the main wxWidgets frame for Thoth Control Panel.
// Handles sidebar, chat display, input box, and buttons.

#pragma once
#include "AgentInterface.h"
#include <wx/aui/aui.h>
#include <wx/collpane.h>
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

class GragDiagnosticsPanel;
class StrategyPanel;
class PlanExecutionPanel;
class TrajectoryViewer;
class ExperimentLabPanel;
class GraphPanel;
class BenchmarkWindow;
namespace Thoth { class ExecutiveStateStrip; }

class MainFrame : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;

    // Public method for FileDropTarget to call
    bool HandleFileDrop(const wxArrayString& filenames);

private:
    // AUI Manager
    wxAuiManager m_auiManager;
    
    // Benchmark state
    bool m_isBenchmarkRunning = false;
    BenchmarkWindow* m_activeBenchmarkWindow = nullptr;

    enum MenuID : int {
        ID_MENU_FILE_NEW_CHAT = wxID_HIGHEST + 1,
        ID_MENU_FILE_OPEN_SESSION,
        ID_MENU_FILE_SAVE_SESSION,
        ID_MENU_FILE_EXPORT_SESSION,
        ID_MENU_FILE_IMPORT_CORPUS,
        ID_MENU_FILE_EXIT,
        ID_MENU_AGENT_RUN_GOAL,
        ID_MENU_AGENT_PAUSE,
        ID_MENU_AGENT_RESUME,
        ID_MENU_AGENT_ABORT,
        ID_MENU_AGENT_SHOW_PLAN,
        ID_MENU_AGENT_SHOW_TRAJECTORY,
        ID_MENU_TOOLS_STRATEGY_VIEWER,
        ID_MENU_TOOLS_TRAJECTORY_BROWSER,
        ID_MENU_TOOLS_TOOL_REGISTRY,
        ID_MENU_TOOLS_PROMPT_TEMPLATES,
        ID_MENU_BENCH_RUN_GRAG,
        ID_MENU_BENCH_RETRIEVAL_COMPARISON,
        ID_MENU_BENCH_STRATEGY_LEARNING,
        ID_MENU_BENCH_FULL_SYSTEM,
        ID_MENU_VIEW_SHOW_SESSIONS,
        ID_MENU_VIEW_SHOW_PLAN,
        ID_MENU_VIEW_SHOW_GRAG,
        ID_MENU_VIEW_SHOW_STRATEGY,
        ID_MENU_VIEW_SHOW_RETRIEVAL_GRAPH,
        ID_MENU_VIEW_SHOW_PLAN_TREE,
        ID_MENU_VIEW_TOGGLE_DARK,
        ID_MENU_HELP_DOCUMENTATION,
        ID_MENU_HELP_ARCH_OVERVIEW,
        ID_MENU_HELP_ABOUT
    };

    // UI elements
    wxDataViewCtrl* m_chatList = nullptr;
    wxButton* m_newChatButton = nullptr;
    wxButton* m_deleteChatButton = nullptr;
    wxButton* m_copyChatButton = nullptr;
    wxScrolledWindow* m_chatContainer = nullptr;
    wxBoxSizer* m_chatSizer = nullptr;
    GragDiagnosticsPanel* m_gragPanel = nullptr;
    StrategyPanel* m_strategyPanel = nullptr;
    PlanExecutionPanel* m_planPanel = nullptr;

    wxNotebook* m_bottomNotebook = nullptr;
    TrajectoryViewer* m_trajectoryViewer = nullptr;
    ExperimentLabPanel* m_experimentLab = nullptr;
    GraphPanel* m_graphPanel = nullptr;
    wxPanel* m_logPanel = nullptr;
    wxTextCtrl* m_logText = nullptr;

    Thoth::ExecutiveStateStrip* m_stateStrip = nullptr;
    
    wxPanel*      m_goalBanner = nullptr;
    wxStaticText* m_goalText = nullptr;
    wxButton*     m_clearGoalBtn = nullptr;
    wxButton*     m_reviseGoalBtn = nullptr;

    wxTextCtrl* m_inputCtrl = nullptr;
    wxButton* m_sendButton = nullptr;
    wxButton* m_retrievalExplainBtn = nullptr;
    wxButton* m_planExplainBtn = nullptr;

    wxStaticText* m_typingIndicator = nullptr;

    // Sidebar Containers
    wxScrolledWindow* m_leftSidebar = nullptr;
    wxScrolledWindow* m_rightSidebar = nullptr;

    // Helper for collapsible sections
    wxCollapsiblePane* AddCollapsiblePane(wxScrolledWindow* parent, const wxString& label, wxWindow* content, bool expanded = true);

    wxStaticText* m_ragFileSlot1 = nullptr;
    wxStaticText* m_ragFileSlot2 = nullptr;
    wxStaticText* m_ragFileSlot3 = nullptr;
    wxStaticText* m_ragFileSlot4 = nullptr;

    wxButton* m_ragDeleteBtn1 = nullptr;
    wxButton* m_ragDeleteBtn2 = nullptr;
    wxButton* m_ragDeleteBtn3 = nullptr;
    wxButton* m_ragDeleteBtn4 = nullptr;

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
    void UpdateRagSlotLabel(const std::string& path, const std::string& label);
    void MigrateFilesToSandbox(std::vector<std::string>& paths);
    void RenderSession(std::size_t sessionIndex);
    void ActivateSession(std::size_t sessionIndex);
    bool SyncAgentMemoryFromActiveSession();
    void RefreshAllPanels();

    // Event handlers
    void OnSend(wxCommandEvent& evt);
    void OnShowDecisionTrace(wxCommandEvent& evt);
    void OnChatSelected(wxDataViewEvent& evt);
    void OnNewChat(wxCommandEvent& evt);
    void OnDeleteChat(wxCommandEvent& evt);
    void OnCopyChat(wxCommandEvent& evt);
    void OnMenuFileNewChat(wxCommandEvent& evt);
    void OnMenuFileOpenSession(wxCommandEvent& evt);
    void OnMenuFileSaveSession(wxCommandEvent& evt);
    void OnMenuFileExportSession(wxCommandEvent& evt);
    void OnMenuFileImportCorpus(wxCommandEvent& evt);
    void OnMenuFileExit(wxCommandEvent& evt);
    void OnMenuAgentRunGoal(wxCommandEvent& evt);
    void OnMenuAgentPause(wxCommandEvent& evt);
    void OnMenuAgentResume(wxCommandEvent& evt);
    void OnMenuAgentAbort(wxCommandEvent& evt);
    void OnMenuAgentShowPlan(wxCommandEvent& evt);
    void OnMenuAgentShowTrajectory(wxCommandEvent& evt);
    void OnMenuToolsStrategyViewer(wxCommandEvent& evt);
    void OnMenuToolsTrajectoryBrowser(wxCommandEvent& evt);
    void OnMenuToolsToolRegistry(wxCommandEvent& evt);
    void OnMenuToolsPromptTemplates(wxCommandEvent& evt);
    void OnMenuBenchRunGrag(wxCommandEvent& evt);
    void OnMenuBenchRetrievalComparison(wxCommandEvent& evt);
    void OnMenuBenchStrategyLearning(wxCommandEvent& evt);
    void OnMenuBenchFullSystem(wxCommandEvent& evt);
    void OnMenuViewShowGrag(wxCommandEvent& evt);
    void OnMenuViewShowStrategy(wxCommandEvent& evt);
    void OnMenuViewShowRetrievalGraph(wxCommandEvent& evt);
    void OnMenuViewShowPlanTree(wxCommandEvent& evt);
    void OnMenuViewToggleDark(wxCommandEvent& evt);
    void OnMenuHelpDocumentation(wxCommandEvent& evt);
    void OnMenuHelpArchitecture(wxCommandEvent& evt);
    void OnMenuHelpAbout(wxCommandEvent& evt);
    void OnClose(wxCloseEvent& evt);

    void RefreshGoalBanner();
    void ClearActiveGoal();
    void SetSessionGoal(const std::string& sessionId, const std::string& goal);

    void SetupMenuBar();
    void ShowMenuStatus(const wxString& title, const wxString& message);
};
