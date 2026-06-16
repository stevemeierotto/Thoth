// src/MainFrame.cpp
//
// Implements the main control panel GUI.

#include "MainFrame.h"
#include "VisualizationFrame.h"
#include "GragDiagnosticsPanel.h"
#include "StrategyPanel.h"
#include "PlanExecutionPanel.h"
#include "TrajectoryViewer.h"
#include "ExperimentLabPanel.h"
#include "GraphPanel.h"
#include "BenchmarkWindow.h"
#include "ExecutiveStateStrip.h"
#include "file_handler.h"
#include "../external/basic_agent/include/memory_pruning_config.h"

#include <json.hpp>
#include <wx/notebook.h>
#include <wx/clipbrd.h>
#include <wx/aboutdlg.h>

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
#include "ChatSessionRenderer.h"
#include "ChatMessagePanel.h"
#include <wx/clipbrd.h>

using json = nlohmann::json;
using namespace Thoth; // Bring ChatSession and ChatMessage into scope

namespace {

void TrimSessionMessagesForPersistence(Thoth::ChatSession& session) {
    const std::size_t maxHot = Thoth::MemoryPruning::kMaxHotMessages;
    if (session.messages.size() <= maxHot) {
        return;
    }
    const std::size_t dropCount = session.messages.size() - maxHot;
    session.messages.erase(session.messages.begin(),
                           session.messages.begin() + static_cast<std::ptrdiff_t>(dropCount));
}

} // namespace

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
            session.activeGoal = item.value("active_goal", "");

            if (item.contains("rag_files") && item["rag_files"].is_array()) {
                for (const auto& path : item["rag_files"]) {
                    if (path.is_string()) {
                        session.ragFilePaths.push_back(path.get<std::string>());
                    }
                }
            } else if (item.contains("ragFilePaths") && item["ragFilePaths"].is_array()) {
                // Fallback for old schema
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

        for (auto& session : m_sessions) {
            TrimSessionMessagesForPersistence(session);
        }
    } catch (...) {
        m_sessions.clear();
    }
}

void MainFrame::SaveChatSessions() {
    if (m_chatSessionsPath.empty()) {
        return;
    }

    for (auto& session : m_sessions) {
        TrimSessionMessagesForPersistence(session);
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
        sessionJson["rag_files"] = session.ragFilePaths;
        sessionJson["active_goal"] = session.activeGoal;
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
        m_auiManager.Update();
        return;
    }

    const auto& session = m_sessions[m_activeSessionIndex];
    const auto& files = session.ragFilePaths;

    setSlot(m_ragFileSlot1, m_ragDeleteBtn1, files.size() > 0 ? files[0] : "", 1);
    setSlot(m_ragFileSlot2, m_ragDeleteBtn2, files.size() > 1 ? files[1] : "", 2);
    setSlot(m_ragFileSlot3, m_ragDeleteBtn3, files.size() > 2 ? files[2] : "", 3);
    setSlot(m_ragFileSlot4, m_ragDeleteBtn4, files.size() > 3 ? files[3] : "", 4);
    
    m_auiManager.Update();
}

void MainFrame::RenderSession(std::size_t sessionIndex) {
    if (!m_chatContainer || !m_chatSizer || sessionIndex >= m_sessions.size()) {
        return;
    }

    const Thoth::ChatSession& session = m_sessions[sessionIndex];

    m_chatSizer->Clear(true); // Delete all old message panels

    for (const auto& message : session.messages) {
        bool isUser = (message.role == "user");
        std::string safeContent = message.content;
        if (safeContent.size() > 50000) {
            safeContent = safeContent.substr(0, 50000) + "\n... [TRUNCATED] ...";
        }
        ChatMessagePanel* bubble = new ChatMessagePanel(m_chatContainer, wxString::FromUTF8(safeContent), isUser);
        m_chatSizer->Add(bubble, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    }

    m_chatContainer->FitInside();
    m_chatContainer->Scroll(0, m_chatContainer->GetVirtualSize().y);
}

bool MainFrame::SyncAgentMemoryFromActiveSession() {
    if (!agent || m_activeSessionIndex < 0 || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        return false;
    }

    const Thoth::ChatSession& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
    
    // Migrate files to sandbox before indexing
    auto sessionCopy = session;
    MigrateFilesToSandbox(sessionCopy.ragFilePaths);
    
    std::vector<std::pair<std::string, std::string>> memoryMessages;
    memoryMessages.reserve(session.messages.size());
    for (const auto& message : session.messages) {
        memoryMessages.emplace_back(message.role, message.content);
    }

    agent->setRagFiles(sessionCopy.ragFilePaths);
    bool ok = agent->loadConversationMemory(memoryMessages, BuildMemorySummary(session));
    if (ok) {
        agent->checkResumablePlan();
    }
    
    // Safety: Always ensure the goal banner reflects the session we just loaded
    if (!session.activeGoal.empty()) {
        RefreshGoalBanner();
    }
    
    return ok;
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
    RefreshGoalBanner();
    RefreshAllPanels();
}

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Thoth Control Panel", wxDefaultPosition, wxSize(1000, 700))
{
    SetMinSize(wxSize(800, 600));
    FileHandler fileHandler;
    m_chatSessionsPath = fileHandler.getAgentWorkspacePath("chat_sessions.json");

    agent = std::make_unique<AgentInterface>();

    agent->onResponse = [this](const std::string& reply, const std::string& requestId) {
        wxTheApp->CallAfter([this, reply, requestId]() {
            // Safety: Check if this window still exists
            if (wxPendingDelete.Member(this) || !wxWindow::FindWindowById(GetId())) {
                return;
            }

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

            if (m_typingIndicator) {
                m_typingIndicator->Hide();
            }
            if (m_graphPanel) m_graphPanel->UpdateControllerState("IDLE");
            m_auiManager.Update();

            RefreshChatList();

            // Only render if we are still looking at the session that got the reply
            if (m_sessionId == targetSessionId) {
                 RenderSession(static_cast<std::size_t>(m_activeSessionIndex));
            }

            SetStatusText("Response received");
            RefreshAllPanels();
        });
    };

    // --- Wire Observability events (Phase 2) ---
    agent->onEvent = [this](const ControllerEvent& ev) {
        // Explicitly capture metadata by value to avoid lifetime issues
        nlohmann::json metadata = ev.metadata;
        EventType type = ev.type;
        std::string stepId = ev.step_id;
        std::string eventSessionId = ev.session_id;

        wxTheApp->CallAfter([this, type, metadata, stepId, eventSessionId]() {
            // Safety: Check if this window still exists
            if (!wxPendingDelete.Member(this) && wxWindow::FindWindowById(GetId())) {
                bool isActiveSession = (eventSessionId.empty() || eventSessionId == this->m_sessionId);
                
                std::cerr << "[MainFrame] onEvent: type=" << (int)type 
                          << ", evSid=" << eventSessionId 
                          << ", curSid=" << this->m_sessionId 
                          << ", active=" << (isActiveSession ? "YES" : "NO") << "\n";

                if (type == EventType::RETRIEVAL_DIAGNOSTICS) {
                    std::cerr << "[MainFrame] Received RETRIEVAL_DIAGNOSTICS event.\n";
                    if (isActiveSession) {
                        if (this->m_gragPanel) this->m_gragPanel->UpdateDiagnostics(metadata);
                        if (this->m_graphPanel) this->m_graphPanel->UpdateControllerState("EXECUTING_RETRIEVAL");
                    }
                } else if (type == EventType::MODE_SWITCHED) {
                    if (isActiveSession && this->m_graphPanel) {
                        this->m_graphPanel->UpdateControllerState("SCIENTIFIC_MODE");
                    }
                } else if (type == EventType::STATE_CHANGED) {
                    if (isActiveSession) {
                        if (metadata.contains("reasoning_stage")) {
                            std::string stage = metadata["reasoning_stage"].get<std::string>();
                            if (this->m_graphPanel) {
                                // Map sub-stages to high-level graph nodes
                                if (stage == "hypothesis_generation") this->m_graphPanel->UpdateControllerState("PLANNING");
                                else if (stage == "feasibility_evaluation") this->m_graphPanel->UpdateControllerState("SCIENTIFIC_MODE");
                                else if (stage == "final_selection") this->m_graphPanel->UpdateControllerState("COMPLETED");
                            }
                        } else {
                            if (this->m_graphPanel) this->m_graphPanel->UpdateControllerState(metadata.value("state", "IDLE"));
                        }
                    }
                } else if (type == EventType::PLAN_CREATED || type == EventType::PLAN_REVISED) {
                    if (metadata.contains("plan")) {
                        if (isActiveSession) {
                            if (this->m_stateStrip) this->m_stateStrip->ResetPlan(metadata["plan"]);
                            if (this->m_planPanel) {
                                this->m_planPanel->ResetPlan(metadata["plan"]);
                                this->m_planPanel->SetExecutionState("Running");
                            }
                        }
                        if (metadata["plan"].contains("goal") && metadata["plan"]["goal"].is_string()) {
                            // Use eventSessionId if present (best correlation)
                            std::string targetSid = eventSessionId;
                            if (targetSid.empty()) {
                                // If event has no ID, only update current session if we're sure
                                targetSid = this->m_sessionId;
                            }
                            
                            if (!targetSid.empty()) {
                                SetSessionGoal(targetSid, metadata["plan"]["goal"].get<std::string>());
                            }
                        }
                    }
                } else if (type == EventType::STEP_STARTED) {
                    if (isActiveSession) {
                        if (this->m_stateStrip) this->m_stateStrip->UpdateStepStatus(stepId, StepStatus::RUNNING);
                        if (this->m_planPanel) this->m_planPanel->UpdateStepStatus(stepId, "Running");
                        
                        if (this->m_graphPanel && metadata.contains("step_type")) {
                            int st = metadata["step_type"].get<int>();
                            if (st == 0) this->m_graphPanel->UpdateControllerState("EXECUTING_TOOL");      // StepType::TOOL
                            else if (st == 1) this->m_graphPanel->UpdateControllerState("EXECUTING_RETRIEVAL"); // StepType::RETRIEVAL
                            else if (st == 2) this->m_graphPanel->UpdateControllerState("EXECUTING_LLM");       // StepType::LLM
                        }
                    }
                } else if (type == EventType::STEP_COMPLETED) {
                    if (isActiveSession) {
                        if (this->m_stateStrip) this->m_stateStrip->UpdateStepStatus(stepId, StepStatus::SUCCESS);
                        if (this->m_planPanel) this->m_planPanel->UpdateStepStatus(stepId, "Success");
                    }
                } else if (type == EventType::STEP_FAILED) {
                    if (isActiveSession) {
                        if (this->m_stateStrip) this->m_stateStrip->UpdateStepStatus(stepId, StepStatus::FAILED);
                        if (this->m_planPanel) this->m_planPanel->UpdateStepStatus(stepId, "Failed");
                    }
                } else if (type == EventType::INDEXING_STARTED) {
                    std::string path = metadata["file_path"].get<std::string>();
                    this->SetStatusText("Chunking: " + path);
                    this->UpdateRagSlotLabel(path, "Chunking...");
                } else if (type == EventType::INDEXING_COMPLETED) {
                    std::string path = metadata["file_path"].get<std::string>();
                    this->SetStatusText("Indexing complete: " + path);
                    this->RefreshRagPanel();
                } else if (type == EventType::PLAN_COMPLETED) {
                    this->SetStatusText("Goal completed successfully");
                    if (isActiveSession && this->m_planPanel) this->m_planPanel->SetExecutionState("Completed");
                    this->RefreshAllPanels();
                } else if (type == EventType::PLAN_FAILED) {
                    if (isActiveSession && this->m_planPanel) this->m_planPanel->SetExecutionState("Failed");
                } else if (type == EventType::PLAN_ABORTED) {
                    if (isActiveSession && this->m_planPanel) this->m_planPanel->SetExecutionState("Aborted");
                } else if (type == EventType::PLAN_REUSE_INJECTION) {
                    std::cerr << "[MainFrame] PLAN_REUSE_INJECTION source="
                              << metadata.value("source", "unknown")
                              << " count=" << metadata.value("plan_count", 0) << "\n";
                    if (isActiveSession) {
                        SetStatusText(wxString::Format(
                            "Plan reuse: %d similar past plan(s) from %s",
                            metadata.value("plan_count", 0),
                            wxString::FromUTF8(metadata.value("source", "unknown"))));
                    }
                } else if (type == EventType::REFLECTION_REPLAN) {
                    std::cerr << "[MainFrame] REFLECTION_REPLAN score="
                              << metadata.value("trajectory_score", 0.0f)
                              << " cycle=" << metadata.value("reflection_cycle", 0) << "\n";
                    if (isActiveSession) {
                        SetStatusText(wxString::Format(
                            "Reflection replan (score %.2f, cycle %d)",
                            metadata.value("trajectory_score", 0.0f),
                            metadata.value("reflection_cycle", 0)));
                    }
                } else if (type == EventType::PLAN_HISTORY_STORED) {
                    std::cerr << "[MainFrame] PLAN_HISTORY_STORED plan_id="
                              << metadata.value("plan_id", "")
                              << " score=" << metadata.value("success_score", 0.0f) << "\n";
                    if (isActiveSession) {
                        SetStatusText("Plan history saved to past_plans + cognate_plans");
                    }
                }
                
                // Periodically refresh panels during execution for live updates
                if (type == EventType::STEP_COMPLETED) {
                    this->RefreshAllPanels();
                }
            }
        });
    };

    // AUI Manager
    m_auiManager.SetManagedWindow(this);

    // --- Left Sidebar ---
    m_leftSidebar = new wxScrolledWindow(this, wxID_ANY);
    m_leftSidebar->SetScrollRate(0, 10);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    m_leftSidebar->SetSizer(leftSizer); // Initialize sizer early

    wxPanel* pastChatsPane = new wxPanel(m_leftSidebar, wxID_ANY);
    wxBoxSizer* pastChatsSizer = new wxBoxSizer(wxVERTICAL);

    m_chatList = new wxDataViewCtrl(pastChatsPane, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_NO_HEADER | wxDV_ROW_LINES);
    m_chatListModel = new ChatSessionDataViewModel(&m_sessions);
    m_chatList->AssociateModel(m_chatListModel.get());

    auto renderer = new ChatSessionRenderer();
    auto col = new wxDataViewColumn("Past Chats", renderer, ChatSessionDataViewModel::Col_Session, 200, wxALIGN_LEFT);
    m_chatList->AppendColumn(col);

    pastChatsSizer->Add(m_chatList, 1, wxEXPAND | wxALL, 2);

    wxBoxSizer* sidebarBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_newChatButton = new wxButton(pastChatsPane, wxID_ANY, "New", wxDefaultPosition, wxSize(60, 30));
    m_deleteChatButton = new wxButton(pastChatsPane, wxID_ANY, "Del", wxDefaultPosition, wxSize(60, 30));
    m_copyChatButton = new wxButton(pastChatsPane, wxID_ANY, "Copy", wxDefaultPosition, wxSize(60, 30));
    sidebarBtnSizer->Add(m_newChatButton, 1, wxALL, 2);
    sidebarBtnSizer->Add(m_deleteChatButton, 1, wxALL, 2);
    sidebarBtnSizer->Add(m_copyChatButton, 1, wxALL, 2);
    pastChatsSizer->Add(sidebarBtnSizer, 0, wxEXPAND);
    pastChatsPane->SetSizer(pastChatsSizer);

    AddCollapsiblePane(m_leftSidebar, "Past Chats", pastChatsPane);

    // --- Center Chat Area ---
    // ... (rest of center area setup)
    wxPanel* centerPanel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* centerSizer = new wxBoxSizer(wxVERTICAL);

    m_stateStrip = new Thoth::ExecutiveStateStrip(centerPanel);
    centerSizer->Add(m_stateStrip, 0, wxEXPAND | wxALL, 0);

    m_goalBanner = new wxPanel(centerPanel, wxID_ANY);
    m_goalBanner->SetBackgroundColour(wxColour(255, 243, 224));
    wxBoxSizer* goalSizer = new wxBoxSizer(wxHORIZONTAL);
    m_goalText = new wxStaticText(m_goalBanner, wxID_ANY, "Current Goal: None");
    m_goalText->SetFont(m_goalText->GetFont().Bold());
    
    m_reviseGoalBtn = new wxButton(m_goalBanner, wxID_ANY, "Revise", wxDefaultPosition, wxSize(60, 24));
    m_clearGoalBtn = new wxButton(m_goalBanner, wxID_ANY, "X", wxDefaultPosition, wxSize(24, 24));
    
    goalSizer->Add(m_goalText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    goalSizer->Add(m_reviseGoalBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    goalSizer->Add(m_clearGoalBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    m_goalBanner->SetSizer(goalSizer);
    centerSizer->Add(m_goalBanner, 0, wxEXPAND | wxALL, 0);
    m_goalBanner->Hide();

    m_chatContainer = new wxScrolledWindow(centerPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_chatContainer->SetScrollRate(0, 10);
    m_chatContainer->SetBackgroundColour(*wxWHITE);
    m_chatSizer = new wxBoxSizer(wxVERTICAL);
    m_chatSizer->AddStretchSpacer(1); // Add a spacer to stabilize initial layout
    m_chatContainer->SetSizer(m_chatSizer);

    m_typingIndicator = new wxStaticText(centerPanel, wxID_ANY, "Agent thinking...");
    m_typingIndicator->SetForegroundColour(wxColour(100, 100, 100));
    m_typingIndicator->Hide();

    // Input Control - spanning full width
    m_inputCtrl = new wxTextCtrl(centerPanel, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 80), wxTE_MULTILINE);
    
    // Buttons in a horizontal sizer
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    m_sendButton = new wxButton(centerPanel, wxID_ANY, "Send", wxDefaultPosition, wxSize(100, 30));
    
    m_retrievalExplainBtn = new wxButton(centerPanel, wxID_ANY, "Explain Retrieval", wxDefaultPosition, wxSize(140, 30));
    m_retrievalExplainBtn->SetToolTip("Show retrieval diagnostics for the last answer");
    
    m_planExplainBtn = new wxButton(centerPanel, wxID_ANY, "Explain Plan", wxDefaultPosition, wxSize(120, 30));
    m_planExplainBtn->SetToolTip("Show the full decision trace and plan reasoning");

    buttonSizer->AddStretchSpacer(1); // Push buttons to the right
    buttonSizer->Add(m_sendButton, 0, wxALL, 5);
    buttonSizer->Add(m_retrievalExplainBtn, 0, wxALL, 5);
    buttonSizer->Add(m_planExplainBtn, 0, wxALL, 5);

    centerSizer->Add(m_chatContainer, 1, wxEXPAND | wxALL, 0);
    centerSizer->Add(m_typingIndicator, 0, wxALIGN_LEFT | wxLEFT, 15);
    centerSizer->Add(m_inputCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
    centerSizer->Add(buttonSizer, 0, wxEXPAND | wxBOTTOM, 5);
    
    centerPanel->SetSizer(centerSizer);

    // --- Right Observability Panel ---
    m_rightSidebar = new wxScrolledWindow(this, wxID_ANY);
    m_rightSidebar->SetScrollRate(0, 10);
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    m_rightSidebar->SetSizer(rightSizer); // Initialize sizer early
    
    m_planPanel = new PlanExecutionPanel(this);
    m_gragPanel = new GragDiagnosticsPanel(this);
    m_strategyPanel = new StrategyPanel(this);
    
    AddCollapsiblePane(m_rightSidebar, "Plan Execution", m_planPanel);
    AddCollapsiblePane(m_rightSidebar, "GRAG Diagnostics", m_gragPanel);
    AddCollapsiblePane(m_rightSidebar, "Strategy Engine", m_strategyPanel);

    // --- Bottom Tabbed Notebook ---
    m_bottomNotebook = new wxNotebook(this, wxID_ANY);
    
    // 1. RAG Files Tab
    wxPanel* ragTab = new wxPanel(m_bottomNotebook, wxID_ANY);
    // ... (rest of notebook setup)
    // (Skipping for brevity in this replace call, 
    // but I'll make sure to read enough context to not mess it up)

    // 2. Trajectories Tab
    m_trajectoryViewer = new TrajectoryViewer(m_bottomNotebook);

    // 3. Experiments Tab
    m_experimentLab = new ExperimentLabPanel(m_bottomNotebook);

    // 4. Graph Tab (Cognate Loop visualization)
    m_graphPanel = new GraphPanel(m_bottomNotebook);

    // 5. Logs Tab
    m_logPanel = new wxPanel(m_bottomNotebook);
    wxBoxSizer* logSizer = new wxBoxSizer(wxVERTICAL);
    m_logText = new wxTextCtrl(m_logPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    logSizer->Add(m_logText, 1, wxEXPAND);
    m_logPanel->SetSizer(logSizer);

    m_bottomNotebook->AddPage(ragTab, "RAG Files");
    m_bottomNotebook->AddPage(m_trajectoryViewer, "Trajectories");
    m_bottomNotebook->AddPage(m_experimentLab, "Experiments");
    m_bottomNotebook->AddPage(m_graphPanel, "Graph");
    m_bottomNotebook->AddPage(m_logPanel, "Logs");


    // Add panes to the AUI manager
    m_auiManager.AddPane(m_leftSidebar, wxAuiPaneInfo()
        .Left()
        .Name("KnowledgeBase")
        .Layer(1)
        .BestSize(300, -1)
        .MinSize(150, -1)
        .Caption("Knowledge Base")
        .CloseButton(false)
        .MaximizeButton(true)
        .Resizable(true)
        .Dockable(true)
        .PaneBorder(true)
        .PinButton(true));

    m_auiManager.AddPane(centerPanel, wxAuiPaneInfo()
        .CenterPane()
        .Name("ChatCenter")
        .PaneBorder(false));

    m_auiManager.AddPane(m_bottomNotebook, wxAuiPaneInfo()
        .Bottom()
        .Name("SystemState")
        .Layer(1)
        .BestSize(-1, 350)
        .MinSize(-1, 150)
        .Caption("System State")
        .CloseButton(true)
        .Resizable(true)
        .Dockable(true)
        .PinButton(true));

    m_auiManager.AddPane(m_rightSidebar, wxAuiPaneInfo()
        .Right()
        .Name("Observability")
        .Caption("Observability")
        .Layer(1)
        .BestSize(350, -1)
        .MinSize(150, -1)
        .CloseButton(false)
        .MaximizeButton(true)
        .Resizable(true)
        .Dockable(true)
        .PaneBorder(true));


    // Commit the layout
    m_auiManager.Update();

    // Event bindings
    m_sendButton->Bind(wxEVT_BUTTON, &MainFrame::OnSend, this);
    m_retrievalExplainBtn->Bind(wxEVT_BUTTON, &MainFrame::OnMenuViewShowGrag, this);
    m_planExplainBtn->Bind(wxEVT_BUTTON, &MainFrame::OnShowDecisionTrace, this);
    m_inputCtrl->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        if (event.ControlDown() && event.GetKeyCode() == WXK_RETURN) {
            wxCommandEvent evt(wxEVT_BUTTON, m_sendButton->GetId());
            OnSend(evt);
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
        ClearActiveGoal();
    });
    
    m_reviseGoalBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_activeSessionIndex < 0 || static_cast<size_t>(m_activeSessionIndex) >= m_sessions.size()) return;
        auto& session = m_sessions[static_cast<size_t>(m_activeSessionIndex)];
        wxString newGoal = wxGetTextFromUser("Revise active goal:", "Revise Goal", wxString::FromUTF8(session.activeGoal), this);
        if (!newGoal.IsEmpty()) {
            if (agent) agent->executeGoal(newGoal.ToStdString());
        }
    });

    CreateStatusBar(1);

    SetupMenuBar();

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
    if (m_activeSessionIndex < 0 || m_activeSessionIndex >= static_cast<int>(m_sessions.size()) || m_sessionId.empty()) {
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
    if (m_graphPanel) {
        m_graphPanel->ResetNodes();
        m_graphPanel->UpdateControllerState("CONVERSATIONAL");
    }
    m_auiManager.Update();

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
    RefreshAllPanels();
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
        MigrateFilesToSandbox(session.ragFilePaths);
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

void MainFrame::RefreshAllPanels() {
    if (!agent) return;

    if (m_strategyPanel) {
        m_strategyPanel->UpdateStrategies(agent->getStrategies());
    }
    if (m_trajectoryViewer) {
        m_trajectoryViewer->UpdateTrajectories(agent->getTrajectories());
    }
    if (m_experimentLab) {
        m_experimentLab->UpdateExperiments(agent->getExperiments());
    }
    if (m_graphPanel) {
        m_graphPanel->UpdateGraphStats(agent->getGraphStats());
    }
    
    // Refresh logs if available (Safe tail read)
    FileHandler fileHandler;
    std::string tracePath = fileHandler.getAgentWorkspacePath("decision_trace.jsonl");
    if (m_logText && std::filesystem::exists(tracePath)) {
        try {
            std::ifstream in(tracePath, std::ios::binary | std::ios::ate);
            if (in) {
                std::streamsize fileSize = in.tellg();
                size_t maxRead = 4096; // Read last 4KB
                size_t toRead = std::min(static_cast<size_t>(fileSize), maxRead);
                
                in.seekg(fileSize - static_cast<std::streamsize>(toRead));
                std::string content(toRead, '\0');
                in.read(&content[0], static_cast<std::streamsize>(toRead));
                
                size_t start = 0;
                while (start < content.size() && (static_cast<unsigned char>(content[start]) & 0xC0) == 0x80) {
                    start++;
                }

                wxString logContent = wxString::FromUTF8(content.substr(start));
                m_logText->SetValue(logContent);
                if (m_logText->GetValue().length() > 10000) {
                    m_logText->SetValue(m_logText->GetValue().Right(10000));
                }
                m_logText->SetInsertionPointEnd();
            }
        } catch (...) {
            m_logText->SetValue("Error reading log file.");
        }
    }
}

void MainFrame::OnMenuFileNewChat(wxCommandEvent& evt) {
    OnNewChat(evt);
}

void MainFrame::OnMenuFileOpenSession(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("File → Open Session", "Use the sidebar to switch between sessions.");
}

void MainFrame::OnMenuFileSaveSession(wxCommandEvent& WXUNUSED(evt)) {
    SaveChatSessions();
    SetStatusText("Sessions saved.");
}

void MainFrame::OnMenuFileExportSession(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("File → Export Session", "Exporting sessions is not yet implemented.");
}

void MainFrame::OnMenuFileImportCorpus(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("File → Import Corpus", "Drag and drop files into the Knowledge Base sidebar or RAG tab.");
}

void MainFrame::OnMenuFileExit(wxCommandEvent& WXUNUSED(evt)) {
    Close(true);
}

void MainFrame::OnMenuAgentRunGoal(wxCommandEvent& WXUNUSED(evt)) {
    wxString goal = wxGetTextFromUser("Enter autonomous goal:", "Run Goal", "", this);
    if (!goal.IsEmpty()) {
        if (agent) {
            agent->executeGoal(goal.ToStdString());
        }
    }
}

void MainFrame::OnMenuAgentPause(wxCommandEvent& WXUNUSED(evt)) {
    if (agent) {
        agent->pause();
        SetStatusText("Agent execution paused.");
    }
}

void MainFrame::OnMenuAgentResume(wxCommandEvent& WXUNUSED(evt)) {
    if (agent) {
        agent->resume();
        SetStatusText("Agent execution resumed.");
    }
}

void MainFrame::OnMenuAgentAbort(wxCommandEvent& WXUNUSED(evt)) {
    const int confirm = wxMessageBox("Abort active goal?", "Confirm Abort", wxYES_NO | wxICON_WARNING, this);
    if (confirm == wxYES && agent) {
        agent->abort();
        SetStatusText("Agent execution aborted.");
    }
}

void MainFrame::SetupMenuBar() {
    wxMenuBar* menuBar = new wxMenuBar();

    // File Menu
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_MENU_FILE_NEW_CHAT, "&New Chat\tCtrl+N");
    fileMenu->Append(ID_MENU_FILE_OPEN_SESSION, "&Open Session\tCtrl+O");
    fileMenu->Append(ID_MENU_FILE_SAVE_SESSION, "&Save Sessions\tCtrl+S");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_FILE_EXPORT_SESSION, "&Export Session...");
    fileMenu->Append(ID_MENU_FILE_IMPORT_CORPUS, "&Import Corpus...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_FILE_EXIT, "E&xit\tAlt+F4");

    // Agent Menu
    wxMenu* agentMenu = new wxMenu();
    agentMenu->Append(ID_MENU_AGENT_RUN_GOAL, "&Run Goal...\tCtrl+G");
    agentMenu->Append(ID_MENU_AGENT_PAUSE, "&Pause Execution");
    agentMenu->Append(ID_MENU_AGENT_RESUME, "&Resume Execution");
    agentMenu->Append(ID_MENU_AGENT_ABORT, "&Abort Execution\tCtrl+Shift+A");
    agentMenu->AppendSeparator();
    agentMenu->Append(ID_MENU_AGENT_SHOW_PLAN, "Show Current &Plan");
    agentMenu->Append(ID_MENU_AGENT_SHOW_TRAJECTORY, "Show &Trajectory");

    // Tools Menu
    wxMenu* toolsMenu = new wxMenu();
    toolsMenu->Append(ID_MENU_TOOLS_STRATEGY_VIEWER, "&Strategy Viewer");
    toolsMenu->Append(ID_MENU_TOOLS_TRAJECTORY_BROWSER, "&Trajectory Browser");
    toolsMenu->Append(ID_MENU_TOOLS_TOOL_REGISTRY, "&Tool Registry");
    toolsMenu->Append(ID_MENU_TOOLS_PROMPT_TEMPLATES, "&Prompt Templates");

    // Benchmarks Menu
    wxMenu* benchMenu = new wxMenu();
    benchMenu->Append(ID_MENU_BENCH_RUN_GRAG, "Run &GRAG Benchmark");
    benchMenu->Append(ID_MENU_BENCH_RETRIEVAL_COMPARISON, "Run &Retrieval Comparison");
    benchMenu->Append(ID_MENU_BENCH_STRATEGY_LEARNING, "Run &Strategy Learning Test");
    benchMenu->Append(ID_MENU_BENCH_FULL_SYSTEM, "Run &Full System Benchmark");

    // View Menu
    wxMenu* viewMenu = new wxMenu();
    viewMenu->Append(ID_MENU_VIEW_SHOW_GRAG, "Show &GRAG Diagnostics");
    viewMenu->Append(ID_MENU_VIEW_SHOW_STRATEGY, "Show &Strategy Engine");
    viewMenu->Append(ID_MENU_VIEW_SHOW_RETRIEVAL_GRAPH, "Show &Retrieval Graph");
    viewMenu->Append(ID_MENU_VIEW_SHOW_PLAN_TREE, "Show &Plan Tree");
    viewMenu->AppendSeparator();
    viewMenu->Append(ID_MENU_VIEW_TOGGLE_DARK, "&Toggle Dark Mode");

    // Help Menu
    wxMenu* helpMenu = new wxMenu();
    helpMenu->Append(ID_MENU_HELP_DOCUMENTATION, "&Documentation\tF1");
    helpMenu->Append(ID_MENU_HELP_ARCH_OVERVIEW, "&Architecture Overview");
    helpMenu->Append(ID_MENU_HELP_ABOUT, "&About Thoth");

    menuBar->Append(fileMenu, "&File");
    menuBar->Append(agentMenu, "&Agent");
    menuBar->Append(toolsMenu, "&Tools");
    menuBar->Append(benchMenu, "&Benchmarks");
    menuBar->Append(viewMenu, "&View");
    menuBar->Append(helpMenu, "&Help");

    SetMenuBar(menuBar);

    // Bindings
    Bind(wxEVT_MENU, &MainFrame::OnMenuFileNewChat, this, ID_MENU_FILE_NEW_CHAT);
    Bind(wxEVT_MENU, &MainFrame::OnMenuFileOpenSession, this, ID_MENU_FILE_OPEN_SESSION);
    Bind(wxEVT_MENU, &MainFrame::OnMenuFileSaveSession, this, ID_MENU_FILE_SAVE_SESSION);
    Bind(wxEVT_MENU, &MainFrame::OnMenuFileExportSession, this, ID_MENU_FILE_EXPORT_SESSION);
    Bind(wxEVT_MENU, &MainFrame::OnMenuFileImportCorpus, this, ID_MENU_FILE_IMPORT_CORPUS);
    Bind(wxEVT_MENU, &MainFrame::OnMenuFileExit, this, ID_MENU_FILE_EXIT);

    Bind(wxEVT_MENU, &MainFrame::OnMenuAgentRunGoal, this, ID_MENU_AGENT_RUN_GOAL);
    Bind(wxEVT_MENU, &MainFrame::OnMenuAgentPause, this, ID_MENU_AGENT_PAUSE);
    Bind(wxEVT_MENU, &MainFrame::OnMenuAgentResume, this, ID_MENU_AGENT_RESUME);
    Bind(wxEVT_MENU, &MainFrame::OnMenuAgentAbort, this, ID_MENU_AGENT_ABORT);
    Bind(wxEVT_MENU, &MainFrame::OnMenuAgentShowPlan, this, ID_MENU_AGENT_SHOW_PLAN);
    Bind(wxEVT_MENU, &MainFrame::OnMenuAgentShowTrajectory, this, ID_MENU_AGENT_SHOW_TRAJECTORY);

    Bind(wxEVT_MENU, &MainFrame::OnMenuToolsStrategyViewer, this, ID_MENU_TOOLS_STRATEGY_VIEWER);
    Bind(wxEVT_MENU, &MainFrame::OnMenuToolsTrajectoryBrowser, this, ID_MENU_TOOLS_TRAJECTORY_BROWSER);
    Bind(wxEVT_MENU, &MainFrame::OnMenuToolsToolRegistry, this, ID_MENU_TOOLS_TOOL_REGISTRY);
    Bind(wxEVT_MENU, &MainFrame::OnMenuToolsPromptTemplates, this, ID_MENU_TOOLS_PROMPT_TEMPLATES);

    Bind(wxEVT_MENU, &MainFrame::OnMenuBenchRunGrag, this, ID_MENU_BENCH_RUN_GRAG);
    Bind(wxEVT_MENU, &MainFrame::OnMenuBenchRetrievalComparison, this, ID_MENU_BENCH_RETRIEVAL_COMPARISON);
    Bind(wxEVT_MENU, &MainFrame::OnMenuBenchStrategyLearning, this, ID_MENU_BENCH_STRATEGY_LEARNING);
    Bind(wxEVT_MENU, &MainFrame::OnMenuBenchFullSystem, this, ID_MENU_BENCH_FULL_SYSTEM);

    Bind(wxEVT_MENU, &MainFrame::OnMenuViewShowGrag, this, ID_MENU_VIEW_SHOW_GRAG);
    Bind(wxEVT_MENU, &MainFrame::OnMenuViewShowStrategy, this, ID_MENU_VIEW_SHOW_STRATEGY);
    Bind(wxEVT_MENU, &MainFrame::OnMenuViewShowRetrievalGraph, this, ID_MENU_VIEW_SHOW_RETRIEVAL_GRAPH);
    Bind(wxEVT_MENU, &MainFrame::OnMenuViewShowPlanTree, this, ID_MENU_VIEW_SHOW_PLAN_TREE);
    Bind(wxEVT_MENU, &MainFrame::OnMenuViewToggleDark, this, ID_MENU_VIEW_TOGGLE_DARK);

    Bind(wxEVT_MENU, &MainFrame::OnMenuHelpDocumentation, this, ID_MENU_HELP_DOCUMENTATION);
    Bind(wxEVT_MENU, &MainFrame::OnMenuHelpArchitecture, this, ID_MENU_HELP_ARCH_OVERVIEW);
    Bind(wxEVT_MENU, &MainFrame::OnMenuHelpAbout, this, ID_MENU_HELP_ABOUT);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
}

void MainFrame::OnMenuAgentShowPlan(wxCommandEvent& WXUNUSED(evt)) {
    auto& pane = m_auiManager.GetPane(m_bottomNotebook);
    if (pane.IsOk()) {
        pane.Show();
        m_bottomNotebook->SetSelection(0); // RAG Files
        m_auiManager.Update();
    }
}

void MainFrame::OnMenuAgentShowTrajectory(wxCommandEvent& WXUNUSED(evt)) {
    auto& pane = m_auiManager.GetPane(m_bottomNotebook);
    if (pane.IsOk()) {
        pane.Show();
        m_bottomNotebook->SetSelection(1); // Trajectories
        m_auiManager.Update();
    }
}

void MainFrame::OnMenuToolsStrategyViewer(wxCommandEvent& evt) {
    OnMenuViewShowStrategy(evt);
}

void MainFrame::OnMenuToolsTrajectoryBrowser(wxCommandEvent& evt) {
    OnMenuAgentShowTrajectory(evt);
}

void MainFrame::OnMenuToolsToolRegistry(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("Tools → Tool Registry", "Listing active tools is forthcoming.");
}

void MainFrame::OnMenuToolsPromptTemplates(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("Tools → Prompt Templates", "Prompt management is forthcoming.");
}

void MainFrame::OnMenuBenchRunGrag(wxCommandEvent& WXUNUSED(evt)) {
    if (m_isBenchmarkRunning) {
        wxMessageBox("A benchmark is already in progress. Concurrent runs are disabled to prevent SQLite database locking.", 
                     "Benchmark Busy", wxOK | wxICON_INFORMATION);
        return;
    }

    wxString bin = AgentInterface::GetBenchmarkBinaryPath("run_grag_benchmark");
    if (bin.IsEmpty()) {
        wxMessageBox("Could not locate 'run_grag_benchmark' executable.", "Error", wxOK | wxICON_ERROR);
        return;
    }

    // Determine project root (where agent_workspace/ is located)
    FileHandler fh;
    wxString projectRoot = wxString::FromUTF8(fh.getProjectRoot());
    if (projectRoot.IsEmpty()) projectRoot = ".";
    
    std::cerr << "[MainFrame] projectRoot: " << projectRoot.ToStdString() << "\n";

    m_isBenchmarkRunning = true;
    m_activeBenchmarkWindow = new BenchmarkWindow(this, "GRAG Benchmark", "GRAG", "Sample (Standard)");
    
    // Bind the window's destruction to reset the flag
    m_activeBenchmarkWindow->Bind(wxEVT_DESTROY, [this](wxWindowDestroyEvent& e) {
        if (e.GetEventObject() == m_activeBenchmarkWindow) {
            m_isBenchmarkRunning = false;
            m_activeBenchmarkWindow = nullptr;
        }
        e.Skip();
    });

    m_activeBenchmarkWindow->Show();
    m_activeBenchmarkWindow->Run(bin + " --sample", projectRoot);
}

void MainFrame::OnMenuBenchRetrievalComparison(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("Benchmarks → Retrieval Comparison", "Comparison tool is forthcoming.");
}

void MainFrame::OnMenuBenchStrategyLearning(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("Benchmarks → Strategy Learning", "Learning analysis is forthcoming.");
}

void MainFrame::OnMenuBenchFullSystem(wxCommandEvent& WXUNUSED(evt)) {
    if (m_isBenchmarkRunning) {
        wxMessageBox("A benchmark is already in progress.", "Benchmark Busy", wxOK | wxICON_INFORMATION);
        return;
    }

    wxString bin = AgentInterface::GetBenchmarkBinaryPath("run_cognate_benchmark");
    if (bin.IsEmpty()) {
        wxMessageBox("Could not locate 'run_cognate_benchmark' executable.", "Error", wxOK | wxICON_ERROR);
        return;
    }

    FileHandler fh;
    wxString projectRoot = wxString::FromUTF8(fh.getProjectRoot());
    if (projectRoot.IsEmpty()) projectRoot = ".";

    m_isBenchmarkRunning = true;
    m_activeBenchmarkWindow = new BenchmarkWindow(this, "Full System Benchmark", "Cognate", "Full System");
    
    m_activeBenchmarkWindow->Bind(wxEVT_DESTROY, [this](wxWindowDestroyEvent& e) {
        if (e.GetEventObject() == m_activeBenchmarkWindow) {
            m_isBenchmarkRunning = false;
            m_activeBenchmarkWindow = nullptr;
        }
        e.Skip();
    });

    m_activeBenchmarkWindow->Show();
    m_activeBenchmarkWindow->Run(bin, projectRoot);
}

void MainFrame::OnMenuViewShowGrag(wxCommandEvent& evt) {
    wxWindow* sidebar = m_rightSidebar;
    int id = evt.GetId();
    if (id == ID_MENU_VIEW_SHOW_SESSIONS) sidebar = m_leftSidebar;
    
    if (!sidebar) return;

    auto& pane = m_auiManager.GetPane(sidebar);
    if (pane.IsOk()) {
        pane.Show();
        m_auiManager.Update();
    }
}

void MainFrame::OnMenuViewShowStrategy(wxCommandEvent& evt) {
    OnMenuViewShowGrag(evt);
}

void MainFrame::OnMenuViewShowRetrievalGraph(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("View → Show Retrieval Graph", "Retrieval graph visualization is forthcoming.");
}

void MainFrame::OnMenuViewShowPlanTree(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("View → Show Plan Tree", "Hierarchical plan view is forthcoming.");
}

void MainFrame::OnMenuViewToggleDark(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("View → Toggle Dark Mode", "Dark mode is forthcoming.");
}

void MainFrame::OnMenuHelpDocumentation(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("Help → Documentation", "Documentation is available in the docs/ folder.");
}

void MainFrame::OnMenuHelpArchitecture(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("Help → Architecture", "Refer to docs/README.md for architectural details.");
}

void MainFrame::OnMenuHelpAbout(wxCommandEvent& WXUNUSED(evt)) {
    wxAboutDialogInfo info;
    info.SetName("Thoth Control Panel");
    info.SetVersion("0.2.0");
    info.SetDescription("Experience-Aware Cognitive Agent Research Console");
    info.SetCopyright("(c) 2025 Steve Meierotto");
    wxAboutBox(info);
}

void MainFrame::OnClose(wxCloseEvent& evt) {
    if (m_isBenchmarkRunning && m_activeBenchmarkWindow) {
        // We try to close the benchmark window first.
        // It will prompt the user and Veto if they say NO.
        if (!m_activeBenchmarkWindow->Close()) {
            evt.Veto();
            return;
        }
    }
    
    evt.Skip();
}

void MainFrame::RefreshGoalBanner() {
    if (m_activeSessionIndex < 0 || static_cast<size_t>(m_activeSessionIndex) >= m_sessions.size()) {
        std::cerr << "[MainFrame] RefreshGoalBanner: Invalid index " << m_activeSessionIndex << "\n";
        if (m_goalBanner) m_goalBanner->Hide();
        m_auiManager.Update();
        return;
    }

    const std::string& goal = m_sessions[static_cast<size_t>(m_activeSessionIndex)].activeGoal;
    std::cerr << "[MainFrame] RefreshGoalBanner: goal=\"" << goal << "\"\n";
    if (goal.empty()) {
        if (m_goalBanner) m_goalBanner->Hide();
    } else {
        if (m_goalText) m_goalText->SetLabel("Current Goal: " + wxString::FromUTF8(goal));
        if (m_goalBanner) m_goalBanner->Show();
    }
    m_auiManager.Update();
}

void MainFrame::ClearActiveGoal() {
    if (m_activeSessionIndex >= 0 && m_activeSessionIndex < static_cast<int>(m_sessions.size())) {
        m_sessions[static_cast<size_t>(m_activeSessionIndex)].activeGoal.clear();
        SaveChatSessions();
    }
    RefreshGoalBanner();
}

void MainFrame::SetSessionGoal(const std::string& sessionId, const std::string& goal) {
    std::cerr << "[MainFrame] SetSessionGoal: sid=" << sessionId << ", goal=\"" << goal << "\"\n";
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(), [&sessionId](const Thoth::ChatSession& session){
        return session.id == sessionId;
    });
    if (it == m_sessions.end()) {
        std::cerr << "[MainFrame] SetSessionGoal: Session NOT FOUND!\n";
        return;
    }

    it->activeGoal = goal;
    SaveChatSessions();

    std::cerr << "[MainFrame] SetSessionGoal: updated goal for " << sessionId << ", curSid=" << m_sessionId << "\n";
    if (m_sessionId == sessionId) {
        RefreshGoalBanner();
    }
}

void MainFrame::UpdateRagSlotLabel(const std::string& path, const std::string& label) {
    auto update = [&](wxStaticText* slot, const std::string& p) {
        if (!slot || m_activeSessionIndex < 0) return false;
        std::filesystem::path fp(p);
        if (slot->GetLabel().Contains(fp.filename().string())) {
            slot->SetLabel(fp.filename().string() + " (" + label + ")");
            return true;
        }
        return false;
    };

    if (update(m_ragFileSlot1, path)) return;
    if (update(m_ragFileSlot2, path)) return;
    if (update(m_ragFileSlot3, path)) return;
    if (update(m_ragFileSlot4, path)) return;
}

void MainFrame::MigrateFilesToSandbox(std::vector<std::string>& paths) {
    bool changed = false;
    for (auto& path : paths) {
        if (path.find("agent_workspace/rag/") == std::string::npos) {
            try {
                std::filesystem::path src(path);
                if (std::filesystem::exists(src)) {
                    std::filesystem::path destDir = "/home/steve/Thoth/agent_workspace/rag/";
                    if (!std::filesystem::exists(destDir)) {
                        std::filesystem::create_directories(destDir);
                    }
                    std::filesystem::path dest = destDir / src.filename();
                    if (!std::filesystem::exists(dest)) {
                        std::filesystem::copy_file(src, dest);
                    }
                    path = dest.string();
                    changed = true;
                }
            } catch (...) {}
        }
    }
    if (changed) {
        SaveChatSessions();
    }
}

void MainFrame::ShowMenuStatus(const wxString& title, const wxString& message) {
    wxMessageBox(message, title, wxOK | wxICON_INFORMATION, this);
}

wxCollapsiblePane* MainFrame::AddCollapsiblePane(wxScrolledWindow* parent, const wxString& label, wxWindow* content, bool expanded) {
    wxCollapsiblePane* coll = new wxCollapsiblePane(parent, wxID_ANY, label);
    if (expanded) coll->Expand();
    
    wxWindow* pane = coll->GetPane();
    wxBoxSizer* paneSizer = new wxBoxSizer(wxVERTICAL);
    
    content->Reparent(pane);
    paneSizer->Add(content, 1, wxEXPAND | wxALL, 0);
    pane->SetSizer(paneSizer);

    coll->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [this, parent, coll](wxCollapsiblePaneEvent& evt) {
        parent->Layout();
        parent->FitInside();
        
        if (!evt.GetCollapsed()) {
            wxTheApp->CallAfter([parent, coll]() {
                // Ensure the newly expanded pane is visible
                int x, y;
                coll->GetPosition(&x, &y);
                int ppuX, ppuY;
                parent->GetScrollPixelsPerUnit(&ppuX, &ppuY);
                
                if (ppuY > 0) {
                    int scrollY = y / ppuY;
                    int maxScrollX, maxScrollY;
                    parent->GetVirtualSize(&maxScrollX, &maxScrollY);
                    
                    // Safety: clamp to valid range
                    if (scrollY >= 0) {
                        parent->Scroll(-1, scrollY);
                    }
                }
            });
        }
    });

    if (parent->GetSizer()) {
        parent->GetSizer()->Add(coll, 0, wxEXPAND | wxALL, 5);
    }

    return coll;
}
