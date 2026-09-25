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
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "FileDropTarget.h"
#include "ChatSessionDataViewModel.h"
#include "ChatSessionTypes.h" // Contains ChatMessage and ChatSession structs
#include "chat_turn_ui_status.h"
#include "local_note_engine_sync.h"
#include "json.hpp"
#include "progress_source.h"

class GragDiagnosticsPanel;
class StrategyPanel;
class PlanExecutionPanel;
class CognitiveStatusPanel;
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
        ID_MENU_AGENT_UNLOCK_SEND,
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
        ID_MENU_BENCH_EXPORT_COGNITIVE_METRICS,
        ID_MENU_BENCH_STATUS,
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
    wxPanel* m_chatInnerPanel = nullptr;
    wxBoxSizer* m_chatSizer = nullptr;
    GragDiagnosticsPanel* m_gragPanel = nullptr;
    StrategyPanel* m_strategyPanel = nullptr;
    PlanExecutionPanel* m_planPanel = nullptr;
    CognitiveStatusPanel* m_cognitivePanel = nullptr;

    wxNotebook* m_bottomNotebook = nullptr;
    wxNotebook* m_observabilityNotebook = nullptr;
    TrajectoryViewer* m_trajectoryViewer = nullptr;
    ExperimentLabPanel* m_experimentLab = nullptr;
    GraphPanel* m_graphPanel = nullptr;
    wxPanel* m_logPanel = nullptr;
    wxTextCtrl* m_logText = nullptr;

    Thoth::ExecutiveStateStrip* m_stateStrip = nullptr;
    
    wxPanel*      m_goalBanner = nullptr;
    wxStaticText* m_goalText = nullptr;
    wxButton*     m_runGoalBtn = nullptr;
    wxButton*     m_clearGoalBtn = nullptr;
    wxButton*     m_reviseGoalBtn = nullptr;

    wxTextCtrl* m_inputCtrl = nullptr;
    wxButton* m_sendButton = nullptr;
    wxButton* m_retrievalExplainBtn = nullptr;
    wxButton* m_planExplainBtn = nullptr;

    wxStaticText* m_typingIndicator = nullptr;
    /** Active Engine chat turn chrome owner (narrow; not ExecutiveStateStrip). */
    std::optional<Thoth::ChatTurnUi::ChatTurnUiState> m_activeChatTurn;
    /** Guards RefreshChatTurnChromeText Layout against wxSizeEvent re-entrancy. */
    bool m_inChatTurnChromeRefresh = false;

    // Sidebar Containers
    wxScrolledWindow* m_leftSidebar = nullptr;

    // Helper for collapsible sections (left Knowledge Base only)
    wxCollapsiblePane* AddCollapsiblePane(wxScrolledWindow* parent, const wxString& label, wxWindow* content, bool expanded = true);

    wxStaticText* m_ragFileSlot1 = nullptr;
    wxStaticText* m_ragFileSlot2 = nullptr;
    wxStaticText* m_ragFileSlot3 = nullptr;
    wxStaticText* m_ragFileSlot4 = nullptr;

    wxButton* m_ragDeleteBtn1 = nullptr;
    wxButton* m_ragDeleteBtn2 = nullptr;
    wxButton* m_ragDeleteBtn3 = nullptr;
    wxButton* m_ragDeleteBtn4 = nullptr;

    wxStaticText* m_corpusStatus = nullptr;
    wxTextCtrl* m_corpusText = nullptr;
    wxPanel* m_ragTab = nullptr;
    wxButton* m_sendToEngineBtn = nullptr;

    // Data model for the chat list
    wxObjectDataPtr<ChatSessionDataViewModel> m_chatListModel;

    wxString m_currentChatTitle;
    std::string m_sessionId;
    std::uint64_t m_requestCounter = 0;
    std::string m_chatSessionsPath;
    std::vector<Thoth::ChatSession> m_sessions;
    int m_activeSessionIndex = -1;
    std::unordered_map<std::string, std::string> m_requestToSession;
    std::unordered_map<std::string, int> m_inFlightChatBySession;
    /** Wall-clock start of current pending streak per session (for watchdog). */
    std::unordered_map<std::string, std::int64_t> m_chatPendingStartedAtMsBySession;

    std::unique_ptr<AgentInterface> agent;

    /** ALP-E — Engine intent snapshot for active session (non-authoritative). */
    std::vector<Thoth::LocalNoteEngineSync::LocalNoteIntent> m_localNoteIntents;
    Thoth::LocalNoteEngineSync::LocalNoteReconcileState m_localNoteReconcileState =
        Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Ready;

    static std::int64_t NowMs();
    std::string BuildSessionTitle(const wxString& firstUserMessage) const;
    std::string BuildMemorySummary(const Thoth::ChatSession& session) const;
    void LoadChatSessions();
    void SaveChatSessions();
    void CreateNewSession(const std::string& title = "New Chat");
    void RefreshChatList();
    void RefreshRagPanel();
    void RefreshRagTabLayout();
    void RefreshCorpusPanel();
    void RefreshSessionConversationFromEngine(const std::string& sessionId);
    void ApplyIngestControls(const Thoth::EventStreamSnapshot& snap);
    void ApplyLocalNoteIndexingStarted(
        const Thoth::LocalNoteEngineSync::IndexingEventMetadata& event);
    void ApplyLocalNoteIndexingCompleted(
        const Thoth::LocalNoteEngineSync::IndexingEventMetadata& event,
        bool success,
        int chunk_count);
    void RecordLocalNoteIngestAccept(const std::string& host_path,
                                     const std::string& document_id,
                                     const std::string& document_name,
                                     const std::string& revision_id = {},
                                     const std::string& content_hash = {});
    void ReconcileLocalNotesEngine(const nlohmann::json& corpus_body);
    bool UseAlpGuiPicker() const;
    void SendLocalNoteToEngine(const std::string& host_path, bool force_replace);
    bool ConfirmForceReplace(const std::string& host_path,
                             const std::string& reason) const;
    nlohmann::json LoadLegacyIdMap() const;
    void SyncLocalNotesFromCorpus(const nlohmann::json& corpus_body);
    bool HasPendingLocalNoteIndexing() const;
    void MigrateFilesToSandbox(std::vector<std::string>& paths);
    void RenderSession(std::size_t sessionIndex);
    void ScrollChatToBottom();
    void ActivateSession(std::size_t sessionIndex);
    bool SyncAgentMemoryFromActiveSession(bool includeRagFiles = true);
    void RefreshAllPanels();
    void UpdateBackendModeBanner();
    void RefreshEventStreamIndicators();
    void ApplyEngineDegradedControls(const Thoth::EventStreamSnapshot& snap);
    void OnConnectionPollTimer(wxTimerEvent& evt);
    void ApplyBenchmarksMenuCapabilities();
    void SetTransientStatus(const wxString& text);
    /** Strip leading "goal:" / "/goal" for executeGoal routing. Empty if not a goal. */
    static wxString ExtractGoalText(const wxString& input);

    // Event handlers
    void OnSend(wxCommandEvent& evt);
    void OnSendToEngine(wxCommandEvent& evt);
    void OnChatContainerSize(wxSizeEvent& evt);
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
    void OnMenuBenchExportCognitiveMetrics(wxCommandEvent& evt);
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
    void ClearSessionGoal(const std::string& sessionId);
    void SetSessionGoal(const std::string& sessionId, const std::string& goal);
    /** Start or restart the active banner goal without opening the Revise dialog. */
    void RunActiveBannerGoal();
    /** Phase 1 — Cognitive State tape: Planning started (before Engine events). */
    void BeginCognitiveGoalPlanningOptimistic();
    /** R3 — align backend session identity with active tab before goal POST. */
    void SyncBackendSessionIdentity();
    /** R3-G6 — resolve session for goal lifecycle events (never broadcast on empty id). */
    std::string ResolveGoalEventSessionId(const std::string& eventSessionId) const;
    void RegisterPendingChatRequest(const std::string& requestId, const std::string& sessionId);
    void ClearPendingChatRequest(const std::string& requestId, const std::string& sessionId);
    /** Unlock Send for a session without waiting for Engine (S2 cancel / S1 orphan). */
    void ForceClearSessionChatPending(const std::string& sessionId);
    /** Unlock Send without abandoning in-flight request_id / turn (watchdog). */
    void UnlockSendKeepInFlightTurn(const std::string& sessionId);
    void UnlockActiveSessionChatSend();
    /**
     * Apply Engine chat success to the originating session transcript even if the
     * turn UI was unlocked early. Returns true when verify succeeded.
     */
    bool ApplyEngineChatSuccessTranscript(const std::string& requestId,
                                          const std::string& sessionId,
                                          const std::string& userContent,
                                          const std::string& expectedAssistant);
    void UpdateChatSendChrome();
    void OnChatPendingWatchdogTimer(wxTimerEvent& evt);
    void OnChatTurnElapsedTimer(wxTimerEvent& evt);
    void OnChatTurnRefreshRetryTimer(wxTimerEvent& evt);
    void OnUnlockChatSend(wxCommandEvent& evt);
    void OnChatInputKeyDown(wxKeyEvent& evt);
    void RefreshExecutiveStripActivity();
    static bool InputStartsGoal(const wxString& input);

    /** Begin owned chat-turn chrome (Engine conversation path). */
    void BeginChatTurnUi(const std::string& requestId,
                         const std::string& sessionId,
                         const std::string& userContent);
    void SetChatTurnPhase(Thoth::ChatTurnUi::Phase phase);
    void RefreshChatTurnChromeText();
    /** Hide typing chrome only if this request still owns it (or force clear owner). */
    void ClearChatTurnChromeIfOwner(const std::string& requestId);
    void ClearChatTurnChromeForSession(const std::string& sessionId,
                                       Thoth::ChatTurnUi::Phase terminalPhase);
    bool ChatTurnOwnsTypingIndicator() const;
    /** Goal/executive events must not hide typing while a chat turn owns it. */
    void HideTypingIndicatorIfChatTurnIdle();
    /**
     * Refresh originating session and prove assistant for this turn.
     * Returns true when transcriptContainsCurrentTurnAssistant succeeds.
     */
    bool RefreshAndVerifyChatTurnAssistant(const std::string& sessionId,
                                           const std::string& userContent,
                                           const std::string& expectedAssistant);
    void FinishChatTurnSuccess(const std::string& requestId,
                               const std::string& sessionId,
                               const std::string& expectedAssistant);
    void FinishChatTurnFailure(const std::string& requestId,
                               const std::string& sessionId);

    /** Phase 5 — work-implying strip text; no-op if source is UserAction/Unknown. */
    void ApplyWorkActivity(const wxString& message, Thoth::ProgressSource source);
    /** Phase 5 — work-implying status; chrome may use SetTransientStatus directly. */
    void ApplyWorkStatus(const wxString& text, Thoth::ProgressSource source);
    /** Phase 7 — one visible outcome per user-initiated Engine operation. */
    void HandleOperationComplete(const Thoth::OperationResult& result,
                                 const std::string& requestId);
    Thoth::ProgressSource ActiveBackendProgressSource() const;

    int m_ragIndexingCount = 0;
    bool m_goalPlanningPending = false;

    wxTimer m_connectionPollTimer;
    wxTimer m_chatPendingWatchdogTimer;
    wxTimer m_chatTurnElapsedTimer;
    wxTimer m_chatTurnRefreshRetryTimer;
    std::int64_t m_lastCorpusPollForIndexingMs = 0;

    void SetupMenuBar();
    void ShowMenuStatus(const wxString& title, const wxString& message);
};
