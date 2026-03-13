// src/MainFrame.cpp
//
// Implements the main control panel GUI.

#include "MainFrame.h"
#include "VisualizationFrame.h"
#include "GragDiagnosticsPanel.h"
#include "ExecutiveStateStrip.h"
#include "file_handler.h"

#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h> // For wxStyledTextCtrl
#include <wx/filename.h> // For wxFileName
#include <thread>
#include <chrono>

#include "FileDropTarget.h"
#include "ChatSessionDataViewModel.h"
#include "ChatMessagePanel.h"

#include <wx/clipbrd.h>

using json = nlohmann::json;
using namespace Thoth; // Bring ChatSession and ChatMessage into scope

std::int64_t MainFrame::NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string MainFrame::BuildSessionTitle(const wxString& firstUserMessage) const {
    wxString compact = firstUserMessage;
    compact.Replace("\n", " ");
    compact.Trim(true);
    compact.Trim(false);

    if (compact.IsEmpty()) {
        return "New Chat";
    }

    constexpr std::size_t maxLen = 42;
    const std::string title = compact.ToStdString();
    if (title.size() <= maxLen) {
        return title;
    }
    return title.substr(0, maxLen - 3) + "...";
}

std::string MainFrame::BuildMemorySummary(const Thoth::ChatSession& session) const {
    std::ostringstream out;
    out << "Session: " << session.title << "\n";
    out << "Messages: " << session.messages.size() << "\n";
    if (!session.messages.empty()) {
        const auto& last = session.messages.back();
        out << "Last role: " << last.role << "\n";
        const std::string preview = last.content.size() > 200
            ? last.content.substr(0, 197) + "..."
            : last.content;
        out << "Last message: " << preview;
    }
    return out.str();
}

void MainFrame::LoadChatSessions() {
    m_sessions.clear();

    if (m_chatSessionsPath.empty()) {
        return;
    }

    std::ifstream in(m_chatSessionsPath);
    if (!in.is_open()) {
        return;
    }

    try {
        json root;
        in >> root;
        if (!root.is_object() || !root.contains("sessions") || !root["sessions"].is_array()) {
            return;
        }

        for (const auto& item : root["sessions"]) {
            if (!item.is_object()) {
                continue;
            }

            Thoth::ChatSession session;
            session.id = item.value("id", "");
            session.title = item.value("title", "New Chat");
            session.createdAtMs = item.value("created_at_ms", 0LL);
            session.updatedAtMs = item.value("updated_at_ms", session.createdAtMs);

            if (item.contains("ragFilePaths") && item["ragFilePaths"].is_array()) {
                for (const auto& path : item["ragFilePaths"]) {
                    if (path.is_string()) {
                        session.ragFilePaths.push_back(path.get<std::string>());
                    }
                }
            }

            if (session.id.empty()) {
                continue;
            }

            if (item.contains("messages") && item["messages"].is_array()) {
                for (const auto& messageItem : item["messages"]) {
                    if (!messageItem.is_object()) {
                        continue;
                    }

                    Thoth::ChatMessage message;
                    message.role = messageItem.value("role", "assistant");
                    message.content = messageItem.value("content", "");
                    message.timestampMs = messageItem.value("timestamp_ms", 0LL);
                    session.messages.push_back(std::move(message));
                }
            }

            if (session.updatedAtMs == 0) {
                session.updatedAtMs = NowMs();
            }

            m_sessions.push_back(std::move(session));
        }
    } catch (...) {
        m_sessions.clear();
    }
}

void MainFrame::SaveChatSessions() const {
    if (m_chatSessionsPath.empty()) {
        return;
    }

    const std::filesystem::path sessionsPath(m_chatSessionsPath);
    if (!sessionsPath.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(sessionsPath.parent_path(), ec);
    }

    json root;
    root["version"] = 1;
    root["sessions"] = json::array();

    for (const auto& session : m_sessions) {
        json sessionJson;
        sessionJson["id"] = session.id;
        sessionJson["title"] = session.title;
        sessionJson["created_at_ms"] = session.createdAtMs;
        sessionJson["updated_at_ms"] = session.updatedAtMs;
        sessionJson["ragFilePaths"] = session.ragFilePaths;
        sessionJson["messages"] = json::array();

        for (const auto& message : session.messages) {
            sessionJson["messages"].push_back({
                {"role", message.role},
                {"content", message.content},
                {"timestamp_ms", message.timestampMs}
            });
        }

        root["sessions"].push_back(std::move(sessionJson));
    }

    std::ofstream out(m_chatSessionsPath);
    if (out.is_open()) {
        out << root.dump(2);
    }
}

void MainFrame::CreateNewSession(const std::string& title) {
    const std::int64_t nowMs = NowMs();
    Thoth::ChatSession session;
    session.id = "session-" + std::to_string(nowMs);
    session.title = title;
    session.createdAtMs = nowMs;
    session.updatedAtMs = nowMs;
    m_sessions.push_back(std::move(session));
}

void MainFrame::RefreshChatList() {
    if (!m_chatList || !m_chatListModel) {
        return;
    }

    // Sort the underlying data first
    std::sort(m_sessions.begin(), m_sessions.end(), [](const Thoth::ChatSession& a, const Thoth::ChatSession& b) {
        return a.updatedAtMs > b.updatedAtMs;
    });

    // Notify the model that it needs to be re-read from the source
    m_chatListModel->Cleared();

    // Find the new index of the active session
    if (m_activeSessionIndex != -1) {
        auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
            [this](const Thoth::ChatSession& s) {
                return s.id == m_sessionId;
            });
        if (it != m_sessions.end()) {
            m_activeSessionIndex = static_cast<int>(std::distance(m_sessions.begin(), it));
        } else {
            m_activeSessionIndex = -1;
        }
    }

    // Select the active session in the view
    if (m_activeSessionIndex != -1) {
        wxDataViewItem activeItem = m_chatListModel->GetItemFromIndex(m_activeSessionIndex);
        if (activeItem.IsOk()) {
            m_chatList->EnsureVisible(activeItem);
            m_chatList->Select(activeItem);
        }
    }
}

void MainFrame::RefreshRagPanel() {
    auto setSlot = [this](wxStaticText* slot, wxButton* btn, const std::string& path, int index) {
        if (!slot || !btn) return;
        if (path.empty()) {
            slot->SetLabel(wxString::Format("Empty Slot %d", index));
            btn->Hide();
        } else {
            wxFileName fn(path);
            slot->SetLabel(fn.GetFullName());
            btn->Show();
        }
    };

    if (m_activeSessionIndex < 0 || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        setSlot(m_ragFileSlot1, m_ragDeleteBtn1, "", 1);
        setSlot(m_ragFileSlot2, m_ragDeleteBtn2, "", 2);
        setSlot(m_ragFileSlot3, m_ragDeleteBtn3, "", 3);
        setSlot(m_ragFileSlot4, m_ragDeleteBtn4, "", 4);
        Layout();
        return;
    }

    const auto& session = m_sessions[m_activeSessionIndex];
    const auto& files = session.ragFilePaths;

    setSlot(m_ragFileSlot1, m_ragDeleteBtn1, files.size() > 0 ? files[0] : "", 1);
    setSlot(m_ragFileSlot2, m_ragDeleteBtn2, files.size() > 1 ? files[1] : "", 2);
    setSlot(m_ragFileSlot3, m_ragDeleteBtn3, files.size() > 2 ? files[2] : "", 3);
    setSlot(m_ragFileSlot4, m_ragDeleteBtn4, files.size() > 3 ? files[3] : "", 4);
    
    Layout();
}

void MainFrame::RenderSession(std::size_t sessionIndex) {
    if (!m_chatContainer || !m_chatSizer || sessionIndex >= m_sessions.size()) {
        return;
    }

    const Thoth::ChatSession& session = m_sessions[sessionIndex];

    m_chatSizer->Clear(true); // Delete all old message panels

    for (const auto& message : session.messages) {
        bool isUser = (message.role == "user");
        ChatMessagePanel* bubble = new ChatMessagePanel(m_chatContainer, wxString::FromUTF8(message.content), isUser);
        m_chatSizer->Add(bubble, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    }

    m_chatContainer->Layout();
    m_chatContainer->FitInside();
    m_chatContainer->Scroll(0, m_chatContainer->GetVirtualSize().y);
}

bool MainFrame::SyncAgentMemoryFromActiveSession() {
    if (!agent || m_activeSessionIndex < 0 || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        return false;
    }

    const Thoth::ChatSession& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
    std::vector<std::pair<std::string, std::string>> memoryMessages;
    memoryMessages.reserve(session.messages.size());
    for (const auto& message : session.messages) {
        memoryMessages.emplace_back(message.role, message.content);
    }

    agent->setRagFiles(session.ragFilePaths);
    return agent->loadConversationMemory(memoryMessages, BuildMemorySummary(session));
}

void MainFrame::ActivateSession(std::size_t sessionIndex) {
    if (sessionIndex >= m_sessions.size()) {
        if (!m_sessions.empty()) {
            sessionIndex = 0; // Fallback to the first session
        } else {
             return; // Nothing to activate
        }
    }

    m_activeSessionIndex = static_cast<int>(sessionIndex);
    m_sessionId = m_sessions[sessionIndex].id;
    m_currentChatTitle = wxString::FromUTF8(m_sessions[sessionIndex].title);

    if (agent) {
        agent->setSessionId(m_sessionId);
    }

    RenderSession(sessionIndex);
    RefreshChatList();
    RefreshRagPanel(); // New: Update the RAG file panel
    const bool memoryLoaded = SyncAgentMemoryFromActiveSession();
    SetStatusText(memoryLoaded ? "Loaded selected chat into memory" : "Selected chat (memory sync unavailable)");
}

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Thoth Control Panel", wxDefaultPosition, wxSize(1000, 700))
{
    FileHandler fileHandler;
    m_chatSessionsPath = fileHandler.getAgentWorkspacePath("chat_sessions.json");

    agent = std::make_unique<AgentInterface>();

    agent->onResponse = [this](const std::string& reply, const std::string& requestId) {
        wxTheApp->CallAfter([this, reply, requestId]() {
            const auto requestIt = m_requestToSession.find(requestId);
            if (requestIt == m_requestToSession.end()) {
                return;
            }

            const std::string targetSessionId = requestIt->second;
            m_requestToSession.erase(requestIt);

            auto sessionIt = std::find_if(m_sessions.begin(), m_sessions.end(),
                [&targetSessionId](const Thoth::ChatSession& session) {
                    return session.id == targetSessionId;
                });
            if (sessionIt == m_sessions.end()) {
                return;
            }

            sessionIt->messages.push_back({"assistant", reply, NowMs()});
            sessionIt->updatedAtMs = NowMs();
            SaveChatSessions();

            m_typingIndicator->Hide();
            this->Layout();

            RefreshChatList();

            // Only render if we are still looking at the session that got the reply
            if (m_sessionId == targetSessionId) {
                 RenderSession(static_cast<std::size_t>(m_activeSessionIndex));
            }

            SetStatusText("Response received");
        });
    };

    // --- Wire Observability events (Phase 2) ---
    agent->onEvent = [this](const ControllerEvent& ev) {
        wxTheApp->CallAfter([this, event = ev]() {
            if (event.type == EventType::RETRIEVAL_DIAGNOSTICS) {
                if (this->m_gragPanel) {
                    this->m_gragPanel->UpdateDiagnostics(event.metadata);
                }
            } else if (event.type == EventType::PLAN_CREATED || event.type == EventType::PLAN_REVISED) {
                if (event.metadata.contains("plan")) {
                    if (this->m_stateStrip) this->m_stateStrip->ResetPlan(event.metadata["plan"]);
                    if (this->m_goalText && event.metadata["plan"].contains("goal")) {
                        this->m_goalText->SetLabel(wxString::FromUTF8(event.metadata["plan"]["goal"]));
                        this->m_goalBanner->Show();
                        this->Layout();
                    }
                }
            } else if (event.type == EventType::STEP_STARTED) {
                if (this->m_stateStrip) {
                    this->m_stateStrip->UpdateStepStatus(event.step_id, StepStatus::RUNNING);
                }
            } else if (event.type == EventType::STEP_COMPLETED) {
                if (this->m_stateStrip) {
                    this->m_stateStrip->UpdateStepStatus(event.step_id, StepStatus::SUCCESS);
                }
            } else if (event.type == EventType::STEP_FAILED) {
                if (this->m_stateStrip) {
                    this->m_stateStrip->UpdateStepStatus(event.step_id, StepStatus::FAILED);
                }
            } else if (event.type == EventType::PLAN_COMPLETED || event.type == EventType::PLAN_FAILED || event.type == EventType::PLAN_ABORTED) {
                // Keep banner visible but maybe update status
            }
        });
    };

    // AUI Manager
    m_auiManager.SetManagedWindow(this);

    // --- Left Sidebar ---
    wxPanel* leftPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* sidebarLabel = new wxStaticText(leftPanel, wxID_ANY, "Past Chats");
    sidebarLabel->SetFont(sidebarLabel->GetFont().Bold());

    m_chatList = new wxDataViewCtrl(leftPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES | wxDV_VERT_RULES);
    m_chatListModel = new ChatSessionDataViewModel(&m_sessions);
    m_chatList->AssociateModel(m_chatListModel.get());

    // Setup column
    auto* title_col = new wxDataViewTextRenderer();
    wxDataViewColumn* column = new wxDataViewColumn("Session", title_col, ChatSessionDataViewModel::Col_Title, wxDVC_DEFAULT_WIDTH, wxALIGN_LEFT);
    m_chatList->AppendColumn(column);


    m_newChatButton = new wxButton(leftPanel, wxID_ANY, "New Chat");
    m_deleteChatButton = new wxButton(leftPanel, wxID_ANY, "Delete Chat");
    m_copyChatButton = new wxButton(leftPanel, wxID_ANY, "Copy Chat");

    leftSizer->Add(sidebarLabel, 0, wxALL, 8);
    leftSizer->Add(m_chatList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    leftSizer->Add(m_newChatButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    leftSizer->Add(m_deleteChatButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    leftSizer->Add(m_copyChatButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    leftPanel->SetSizer(leftSizer);

    // --- Center Main Chat Area ---
    wxPanel* centerPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBoxSizer* centerSizer = new wxBoxSizer(wxVERTICAL);

    // Active Goal Banner (Step 2.4)
    m_goalBanner = new wxPanel(centerPanel, wxID_ANY);
    m_goalBanner->SetBackgroundColour(wxColour(232, 245, 233)); // Light green
    wxBoxSizer* bannerSizer = new wxBoxSizer(wxHORIZONTAL);
    
    m_goalText = new wxStaticText(m_goalBanner, wxID_ANY, "No Active Goal");
    m_goalText->SetFont(m_goalText->GetFont().Bold());
    
    m_clearGoalBtn = new wxButton(m_goalBanner, wxID_ANY, "Clear", wxDefaultPosition, wxSize(60, 24));
    m_reviseGoalBtn = new wxButton(m_goalBanner, wxID_ANY, "Revise", wxDefaultPosition, wxSize(60, 24));
    
    bannerSizer->Add(new wxStaticText(m_goalBanner, wxID_ANY, "ACTIVE GOAL: "), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
    bannerSizer->Add(m_goalText, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 10);
    bannerSizer->Add(m_reviseGoalBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    bannerSizer->Add(m_clearGoalBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    m_goalBanner->SetSizer(bannerSizer);
    m_goalBanner->Hide(); // Hidden by default

    centerSizer->Add(m_goalBanner, 0, wxEXPAND | wxALL, 5);

    // Executive State Visualizer (Step 2.3)
    m_stateStrip = new Thoth::ExecutiveStateStrip(centerPanel);
    centerSizer->Add(m_stateStrip, 0, wxEXPAND | wxALL, 5);

    m_chatContainer = new wxScrolledWindow(centerPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_chatContainer->SetScrollRate(0, 20);
    m_chatContainer->SetBackgroundColour(*wxWHITE);
    
    m_chatSizer = new wxBoxSizer(wxVERTICAL);
    m_chatContainer->SetSizer(m_chatSizer);

    wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);
    m_inputCtrl = new wxTextCtrl(
        centerPanel,
        wxID_ANY,
        "",
        wxDefaultPosition,
        wxSize(-1, 60), // taller box for multiline input
        wxTE_MULTILINE | wxTE_RICH2 | wxTE_WORDWRAP
    );


    m_sendButton = new wxButton(centerPanel, wxID_ANY, "Send");
    m_traceButton = new wxButton(centerPanel, wxID_ANY, "Why this answer?");

    m_typingIndicator = new wxStaticText(centerPanel, wxID_ANY, "Agent is thinking...");
    m_typingIndicator->SetFont(m_typingIndicator->GetFont().Italic());
    m_typingIndicator->SetForegroundColour(wxColour(120, 120, 120));
    m_typingIndicator->Hide();

    inputSizer->Add(m_inputCtrl, 1, wxEXPAND | wxALL, 6);
    inputSizer->Add(m_sendButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
    inputSizer->Add(m_traceButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);

    centerSizer->Add(m_chatContainer, 1, wxEXPAND | wxALL, 0);
    centerSizer->Add(m_typingIndicator, 0, wxALIGN_LEFT | wxLEFT, 15);
    centerSizer->Add(inputSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);
    centerPanel->SetSizer(centerSizer);

    // --- RAG File Drop Area ---
    wxPanel* bottomPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxFlexGridSizer* bottomSizer = new wxFlexGridSizer(2, 2, 5, 5);
    bottomSizer->AddGrowableCol(0, 1);
    bottomSizer->AddGrowableCol(1, 1);
    bottomSizer->AddGrowableRow(0, 1);
    bottomSizer->AddGrowableRow(1, 1);

    auto createSlotSizer = [this, bottomPanel](wxStaticText*& slot, wxButton*& btn, int index) {
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        slot = new wxStaticText(bottomPanel, wxID_ANY, wxString::Format("Empty Slot %d", index));
        btn = new wxButton(bottomPanel, wxID_ANY, "X", wxDefaultPosition, wxSize(32, 32));
        btn->SetToolTip("Remove file");
        
        wxFont font = slot->GetFont();
        font.MakeSmaller();
        slot->SetFont(font);
        
        sizer->Add(slot, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);
        sizer->Add(btn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        
        btn->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) {
            if (m_activeSessionIndex < 0 || static_cast<size_t>(m_activeSessionIndex) >= m_sessions.size()) return;
            auto& session = m_sessions[static_cast<size_t>(m_activeSessionIndex)];
            if (static_cast<size_t>(index - 1) < session.ragFilePaths.size()) {
                session.ragFilePaths.erase(session.ragFilePaths.begin() + (index - 1));
                SaveChatSessions();
                RefreshRagPanel();
                if (agent) {
                    agent->setRagFiles(session.ragFilePaths);
                }
            }
        });
        
        return sizer;
    };

    bottomSizer->Add(createSlotSizer(m_ragFileSlot1, m_ragDeleteBtn1, 1), 1, wxEXPAND);
    bottomSizer->Add(createSlotSizer(m_ragFileSlot2, m_ragDeleteBtn2, 2), 1, wxEXPAND);
    bottomSizer->Add(createSlotSizer(m_ragFileSlot3, m_ragDeleteBtn3, 3), 1, wxEXPAND);
    bottomSizer->Add(createSlotSizer(m_ragFileSlot4, m_ragDeleteBtn4, 4), 1, wxEXPAND);

    bottomPanel->SetSizer(bottomSizer);

    // --- Right Observability Panel (Step 2.2) ---
    m_gragPanel = new GragDiagnosticsPanel(this);

    // Add panes to the AUI manager
    m_auiManager.AddPane(leftPanel, wxAuiPaneInfo()
        .Left()
        .Layer(1)
        .BestSize(250, -1)
        .MinSize(200, -1)
        .Caption("Chat Sessions")
        .CloseButton(false)
        .PinButton(true));

    m_auiManager.AddPane(centerPanel, wxAuiPaneInfo()
        .CenterPane()
        .PaneBorder(false));

    m_auiManager.AddPane(bottomPanel, wxAuiPaneInfo()
        .Bottom()
        .Layer(1)
        .BestSize(-1, 200)
        .MinSize(-1, 150)
        .Caption("RAG Files")
        .CloseButton(true)
        .PinButton(true));

    m_auiManager.AddPane(m_gragPanel, wxAuiPaneInfo()
        .Right()
        .Layer(1)
        .Position(0)
        .MinSize(250, -1)
        .BestSize(300, -1)
        .Caption("GRAG Diagnostics")
        .CloseButton(true)
        .MaximizeButton(true));

    bottomPanel->SetDropTarget(new FileDropTarget(this));

    // Commit the layout
    m_auiManager.Update();

    // Event bindings
    m_sendButton->Bind(wxEVT_BUTTON, &MainFrame::OnSend, this);
    m_traceButton->Bind(wxEVT_BUTTON, &MainFrame::OnShowDecisionTrace, this);
    m_inputCtrl->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
    if (event.ControlDown() && event.GetKeyCode() == WXK_RETURN) {
        wxCommandEvent dummy;
        OnSend(dummy);
    } else {
        event.Skip();
    }
});
    m_chatList->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &MainFrame::OnChatSelected, this);
    m_newChatButton->Bind(wxEVT_BUTTON, &MainFrame::OnNewChat, this);
    m_deleteChatButton->Bind(wxEVT_BUTTON, &MainFrame::OnDeleteChat, this);
    m_copyChatButton->Bind(wxEVT_BUTTON, &MainFrame::OnCopyChat, this);

    // Goal Banner bindings
    m_clearGoalBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        m_goalBanner->Hide();
        m_goalText->SetLabel("No Active Goal");
        this->Layout();
    });

    CreateStatusBar(1);

    LoadChatSessions();
    if (m_sessions.empty()) {
        CreateNewSession("New Chat");
        SaveChatSessions();
    }
    ActivateSession(0); // Activate the first session (most recent)

    SetStatusText("Ready");
}

MainFrame::~MainFrame() {
    m_auiManager.UnInit();
    if (agent) {
        agent->onResponse = nullptr;
        agent->onEvent = nullptr;
    }
}

void MainFrame::OnCopyChat(wxCommandEvent& WXUNUSED(evt)) {
    if (m_activeSessionIndex < 0 || static_cast<size_t>(m_activeSessionIndex) >= m_sessions.size()) {
        return;
    }

    const Thoth::ChatSession& session = m_sessions[static_cast<size_t>(m_activeSessionIndex)];
    wxString fullChat;
    for (const auto& msg : session.messages) {
        fullChat << (msg.role == "user" ? "USER: " : "AGENT: ") 
                 << wxString::FromUTF8(msg.content) << "\n\n";
    }

    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(fullChat));
        wxTheClipboard->Close();
        SetStatusText("Full chat copied to clipboard.");
    }
}

void MainFrame::OnSend(wxCommandEvent& WXUNUSED(evt)) {
    wxString input = m_inputCtrl->GetValue();
    if (input.IsEmpty()) return;
    m_inputCtrl->Clear();

    // Ensure we have a valid session and it's active
    if (m_activeSessionIndex < 0 || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        CreateNewSession("New Chat");
        ActivateSession(m_sessions.size() - 1);
    }

    SyncAgentMemoryFromActiveSession();

    // Capture the current session ID to ensure we stay in sync
    const std::string activeId = m_sessionId;
    
    // Find the session object by ID (safer than index if something sorted)
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
        [&activeId](const Thoth::ChatSession& s) { return s.id == activeId; });
    
    if (it == m_sessions.end()) {
        SetStatusText("Error: Active session lost");
        return;
    }

    Thoth::ChatSession& session = *it;
    if (session.messages.empty()) {
        session.title = BuildSessionTitle(input);
    }
    
    session.messages.push_back({"user", input.ToStdString(), NowMs()});
    session.updatedAtMs = NowMs();
    SaveChatSessions();
    
    // Refresh UI
    RefreshChatList(); // Re-sorts, updates m_activeSessionIndex to match m_sessionId
    RenderSession(static_cast<std::size_t>(m_activeSessionIndex));

    m_typingIndicator->Show();
    this->Layout();

    SetStatusText("Message sent, awaiting response...");

    if (agent) {
        ++m_requestCounter;
        const std::string requestId = activeId + "-" + std::to_string(m_requestCounter);
        
        // Track which session this request belongs to
        m_requestToSession[requestId] = activeId;
        
        std::cerr << "[MainFrame] Sending request " << requestId << " for session " << activeId << "\n";
        agent->processUserInput(input.ToStdString(), requestId);
    } else {
        wxMessageBox("Agent not initialized.", "Error", wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::OnShowDecisionTrace(wxCommandEvent& WXUNUSED(evt)) {
    if (!agent) {
        wxMessageBox("Agent is not initialized.", "Decision Trace", wxOK | wxICON_WARNING, this);
        return;
    }

    const std::string summary = agent->getLatestDecisionTraceSummary();
    wxMessageBox(wxString::FromUTF8(summary), "Why this answer?", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnChatSelected(wxDataViewEvent& evt) {
    if (!m_chatListModel) return;

    wxDataViewItem selectedItem = evt.GetItem();
    if (!selectedItem.IsOk()) return;
    
    int sessionIndex = m_chatListModel->GetSessionIndex(selectedItem);
    if (sessionIndex != -1) {
        ActivateSession(static_cast<std::size_t>(sessionIndex));
    }
}

void MainFrame::OnNewChat(wxCommandEvent& WXUNUSED(evt)) {
    CreateNewSession("New Chat");
    SaveChatSessions();
    // The new session is added at the end, let's find it after sorting
    RefreshChatList();
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(), [](const Thoth::ChatSession& s){
        return s.messages.empty();
    });
    size_t newIndex = 0;
    if (it != m_sessions.end()) {
        newIndex = std::distance(m_sessions.begin(), it);
    }
    ActivateSession(newIndex);
    m_inputCtrl->SetFocus();
    SetStatusText("New chat created");
}

void MainFrame::OnDeleteChat(wxCommandEvent& WXUNUSED(evt)) {
    if (m_sessions.empty() || !m_chatListModel) {
        return;
    }

    wxDataViewItem selectedItem = m_chatList->GetCurrentItem();
    if (!selectedItem.IsOk()) {
        wxMessageBox("Please select a chat to delete.", "Delete Chat", wxOK | wxICON_INFORMATION, this);
        return;
    }

    int sessionIndexToDelete = m_chatListModel->GetSessionIndex(selectedItem);
     if (sessionIndexToDelete == -1) return;

    const wxString sessionTitle = wxString::FromUTF8(m_sessions[static_cast<size_t>(sessionIndexToDelete)].title);
    const int confirm = wxMessageBox(
        "Delete chat: \"" + sessionTitle + "\"?",
        "Confirm Delete",
        wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
        this);
    if (confirm != wxYES) {
        SetStatusText("Delete canceled");
        return;
    }

    const std::string deletedSessionId = m_sessions[static_cast<size_t>(sessionIndexToDelete)].id;
    std::erase_if(m_requestToSession, [&deletedSessionId](const auto& entry) {
        return entry.second == deletedSessionId;
    });

    m_sessions.erase(m_sessions.begin() + sessionIndexToDelete);

    if (m_sessions.empty()) {
        CreateNewSession("New Chat");
    }

    SaveChatSessions();
    RefreshChatList(); // Let refresh handle finding the next valid selection

    // Activate the next logical session
    int nextIndex = std::min(sessionIndexToDelete, static_cast<int>(m_sessions.size() - 1));
    ActivateSession(static_cast<size_t>(nextIndex));

    m_inputCtrl->SetFocus();
    SetStatusText("Chat deleted");
}

bool MainFrame::HandleFileDrop(const wxArrayString& filenames) {
    if (m_activeSessionIndex < 0 || static_cast<size_t>(m_activeSessionIndex) >= m_sessions.size()) {
        wxMessageBox("No active chat session to add files to.", "Drag and Drop Error", wxOK | wxICON_EXCLAMATION, this);
        return false;
    }

    auto& session = m_sessions[static_cast<size_t>(m_activeSessionIndex)];
    int filesAddedCount = 0;
    const size_t maxFiles = 4;

    for (const wxString& filename : filenames) {
        if (session.ragFilePaths.size() >= maxFiles) {
            wxMessageBox("Cannot add more than " + std::to_string(maxFiles) + " RAG files per session.", "File Limit Exceeded", wxOK | wxICON_EXCLAMATION, this);
            break;
        }
        
        // Check for duplicates
        bool isDuplicate = false;
        for (const auto& existingPath : session.ragFilePaths) {
            if (existingPath == filename.ToStdString()) {
                isDuplicate = true;
                break;
            }
        }

        if (!isDuplicate) {
            session.ragFilePaths.push_back(filename.ToStdString());
            filesAddedCount++;
        }
    }

    if (filesAddedCount > 0) {
        SaveChatSessions();
        RefreshRagPanel();
        if (agent) {
            agent->setRagFiles(session.ragFilePaths);
        }
        SetStatusText("Added " + std::to_string(filesAddedCount) + " file(s) to RAG context.");
        return true;
    }

    return false;
}
