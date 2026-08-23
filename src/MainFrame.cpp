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
#include "remote_rag_honesty.h"
#include "backend_capabilities.h"
#include "panel_presentation_state.h"
#include "cognitive_diagnostics_authority.h"
#include "progress_source.h"
#include "engine_connection_state.h"
#include "operation_result.h"
#include "corpus_documents.h"
#include "retrieval_verification_display.h"
#include "corpus_create.h"
#include "local_note_engine_sync.h"
#include "alp_feature_flags.h"
#include "corpus_create_local.h"
#include "conversation_authority.h"
#include "research_resources.h"
#include "graph_statistics.h"
#include "../external/basic_agent/include/memory_pruning_config.h"
#include "../external/basic_agent/include/decision_summary.h"

#include <json.hpp>
#include <wx/notebook.h>
#include <wx/clipbrd.h>
#include <wx/aboutdlg.h>
#include <wx/timer.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h> // For wxStyledTextCtrl
#include <wx/filename.h> // For wxFileName
#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/splitter.h>
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

std::string TrimGoalForDisplay(const std::string& goal) {
    if (goal.empty()) {
        return goal;
    }

    std::string trimmed = goal;

    static const char* kInjectionMarkers[] = {
        "\n\n[RELEVANT PAST APPROACHES",
        "\n[RELEVANT PAST APPROACHES",
        "[RELEVANT PAST APPROACHES",
        "\n\n(Reflection:",
        "\n(Reflection:",
        " (Reflection:",
    };
    for (const char* marker : kInjectionMarkers) {
        const std::size_t pos = trimmed.find(marker);
        if (pos != std::string::npos) {
            trimmed = trimmed.substr(0, pos);
        }
    }

    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r' || trimmed.back() == ' ')) {
        trimmed.pop_back();
    }
    const auto first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    trimmed = trimmed.substr(first);
    const auto last = trimmed.find_last_not_of(" \t\r\n");
    if (last != std::string::npos) {
        trimmed = trimmed.substr(0, last + 1);
    }

    if (trimmed.size() >= 6 && trimmed.compare(0, 6, "/goal ") == 0) {
        trimmed = trimmed.substr(6);
    } else if (trimmed.size() >= 5 && trimmed.compare(0, 5, "Goal:") == 0) {
        trimmed = trimmed.substr(5);
        const auto lead = trimmed.find_first_not_of(" \t");
        if (lead != std::string::npos) {
            trimmed = trimmed.substr(lead);
        }
    }

    constexpr std::size_t kMaxDisplayLen = 120;
    if (trimmed.size() > kMaxDisplayLen) {
        trimmed = trimmed.substr(0, kMaxDisplayLen - 3) + "...";
    }

    return trimmed;
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
            session.activeGoal = TrimGoalForDisplay(item.value("active_goal", ""));

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

            if (item.contains("local_note_engine") && item["local_note_engine"].is_object()) {
                for (auto it = item["local_note_engine"].begin();
                     it != item["local_note_engine"].end();
                     ++it) {
                    if (!it.value().is_object()) {
                        continue;
                    }
                    Thoth::LocalNoteEngineInfo info;
                    info.document_id = it.value().value("document_id", "");
                    info.document_name = it.value().value("document_name", "");
                    info.revision_id = it.value().value("revision_id", "");
                    info.content_hash = it.value().value("content_hash", "");
                    info.chunk_count = it.value().value("chunk_count", -1);
                    info.indexing = it.value().value("indexing", false);
                    info.failed = it.value().value("failed", false);
                    info.reconcile_verified = it.value().value("reconcile_verified", false);
                    session.localNoteEngine[it.key()] = std::move(info);
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

void MainFrame::SaveChatSessions() {
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

    const bool cacheOnly = agent && agent->capabilities().supportsConversation;

    for (const auto& session : m_sessions) {
        json sessionJson;
        sessionJson["id"] = session.id;
        sessionJson["title"] = session.title;
        sessionJson["created_at_ms"] = session.createdAtMs;
        sessionJson["updated_at_ms"] = session.updatedAtMs;
        sessionJson["rag_files"] = session.ragFilePaths;
        sessionJson["active_goal"] = session.activeGoal;
        if (!session.localNoteEngine.empty()) {
            json engineJson = json::object();
            for (const auto& entry : session.localNoteEngine) {
                engineJson[entry.first] = {
                    {"document_id", entry.second.document_id},
                    {"document_name", entry.second.document_name},
                    {"revision_id", entry.second.revision_id},
                    {"content_hash", entry.second.content_hash},
                    {"chunk_count", entry.second.chunk_count},
                    {"indexing", entry.second.indexing},
                    {"failed", entry.second.failed},
                    {"reconcile_verified", entry.second.reconcile_verified},
                };
            }
            sessionJson["local_note_engine"] = std::move(engineJson);
        }
        if (!cacheOnly) {
            sessionJson["messages"] = json::array();
            for (const auto& message : session.messages) {
                sessionJson["messages"].push_back({
                    {"role", message.role},
                    {"content", message.content},
                    {"timestamp_ms", message.timestampMs}
                });
            }
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
    if (agent && agent->capabilities().supportsConversation) {
        const nlohmann::json created = agent->createConversationSession();
        session.id = created.value("session_id", "session-" + std::to_string(nowMs));
    } else {
        session.id = "session-" + std::to_string(nowMs);
    }
    session.title = title;
    session.createdAtMs = nowMs;
    session.updatedAtMs = nowMs;
    m_sessions.push_back(std::move(session));
}

void MainFrame::RefreshSessionConversationFromEngine(const std::string& sessionId) {
    if (!agent || !agent->capabilities().supportsConversation || sessionId.empty()) {
        return;
    }

    const nlohmann::json body = agent->getConversation(sessionId);
    std::string err;
    if (!Thoth::ConversationAuthority::hasRequiredConversationFields(body, err)) {
        return;
    }

    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
                           [&sessionId](const Thoth::ChatSession& session) {
                               return session.id == sessionId;
                           });
    if (it == m_sessions.end()) {
        return;
    }

    it->messages.clear();
    for (const auto& msg : body["messages"]) {
        Thoth::ChatMessage message;
        message.role = msg.value("role", "assistant");
        message.content = msg.value("content", "");
        message.timestampMs = msg.value("timestamp_ms", 0LL);
        it->messages.push_back(std::move(message));
    }
    it->updatedAtMs = NowMs();

    if (m_sessionId == sessionId && m_activeSessionIndex >= 0) {
        RenderSession(static_cast<std::size_t>(m_activeSessionIndex));
    }
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

void MainFrame::RefreshRagTabLayout() {
    if (m_ragTab) {
        m_ragTab->Layout();
    }
    if (m_bottomNotebook) {
        m_bottomNotebook->Layout();
    }
    m_auiManager.Update();
}

void MainFrame::RefreshRagPanel() {
    const bool hostOnlyNotes =
        agent && Thoth::RemoteRagHonesty::localNotesAreHostSideOnly(agent->isRemote());
    const bool alp_gui = UseAlpGuiPicker();

    auto setSlot = [this, hostOnlyNotes, alp_gui](wxStaticText* slot, wxButton* btn, const std::string& path, int index) {
        if (!slot || !btn) return;
        if (path.empty()) {
            slot->SetLabel(wxString::Format("Empty Slot %d", index));
            slot->UnsetToolTip();
            btn->Hide();
        } else {
            wxFileName fn(wxString::FromUTF8(path));
            const std::string base = fn.GetFullName().ToStdString();
            if (hostOnlyNotes) {
                const Thoth::LocalNoteEngineInfo* engineInfo = nullptr;
                if (m_activeSessionIndex >= 0
                    && m_activeSessionIndex < static_cast<int>(m_sessions.size())) {
                    const auto& session =
                        m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
                    const auto it = session.localNoteEngine.find(path);
                    if (it != session.localNoteEngine.end()) {
                        engineInfo = &it->second;
                    }
                }
                if (engineInfo
                    && Thoth::LocalNoteEngineSync::localNoteEngineSlotShowsAttachedStatus(
                           *engineInfo)) {
                    std::string label = Thoth::RemoteRagHonesty::formatLocalNoteEngineSlotLabel(
                            base,
                            engineInfo->document_id,
                            engineInfo->chunk_count,
                            engineInfo->indexing,
                            engineInfo->failed,
                            alp_gui);
                    if (alp_gui && !engineInfo->reconcile_verified && !path.empty()) {
                        label += " · cache not verified";
                    }
                    slot->SetLabel(wxString::FromUTF8(label));
                    slot->SetToolTip(wxString::FromUTF8(
                        Thoth::RemoteRagHonesty::formatLocalNoteEngineTooltip(
                            engineInfo->document_id,
                            engineInfo->chunk_count,
                            engineInfo->indexing,
                            engineInfo->failed)));
                } else {
                    slot->SetLabel(wxString::FromUTF8(
                        Thoth::RemoteRagHonesty::formatHostOnlySlotLabel(base)));
                    slot->SetToolTip(wxString::FromUTF8(Thoth::RemoteRagHonesty::kHostOnlyTooltip));
                }
            } else {
                slot->SetLabel(fn.GetFullName());
                slot->UnsetToolTip();
            }
            btn->Show();
        }
    };

    if (m_activeSessionIndex < 0 || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        setSlot(m_ragFileSlot1, m_ragDeleteBtn1, "", 1);
        setSlot(m_ragFileSlot2, m_ragDeleteBtn2, "", 2);
        setSlot(m_ragFileSlot3, m_ragDeleteBtn3, "", 3);
        setSlot(m_ragFileSlot4, m_ragDeleteBtn4, "", 4);
        ApplyIngestControls(agent ? agent->eventStreamSnapshot()
                                  : Thoth::localEventStreamSnapshot(NowMs()));
        RefreshRagTabLayout();
        return;
    }

    const auto& session = m_sessions[m_activeSessionIndex];
    const auto& files = session.ragFilePaths;

    setSlot(m_ragFileSlot1, m_ragDeleteBtn1, files.size() > 0 ? files[0] : "", 1);
    setSlot(m_ragFileSlot2, m_ragDeleteBtn2, files.size() > 1 ? files[1] : "", 2);
    setSlot(m_ragFileSlot3, m_ragDeleteBtn3, files.size() > 2 ? files[2] : "", 3);
    setSlot(m_ragFileSlot4, m_ragDeleteBtn4, files.size() > 3 ? files[3] : "", 4);

    ApplyIngestControls(agent ? agent->eventStreamSnapshot()
                              : Thoth::localEventStreamSnapshot(NowMs()));
    if (m_stateStrip && m_ragIndexingCount == 0 && !m_goalPlanningPending) {
        if (hostOnlyNotes) {
            const int attached =
                Thoth::LocalNoteEngineSync::countAttachedLocalNotes(session);
            const std::string strip =
                Thoth::RemoteRagHonesty::formatLocalNoteStripActivity(
                    static_cast<int>(files.size()), attached);
            if (strip.empty()) {
                m_stateStrip->ClearActivityMessage();
            } else {
                m_stateStrip->SetActivityMessage(wxString::FromUTF8(strip));
            }
        } else {
            m_stateStrip->ClearActivityMessage();
        }
    }
    RefreshRagTabLayout();
}

void MainFrame::RefreshCorpusPanel() {
    if (!m_corpusStatus || !m_corpusText || !agent) {
        return;
    }

    const auto caps = agent->capabilities();
    m_corpusText->Clear();

    if (!caps.supportsCorpusList) {
        m_corpusStatus->SetLabel(wxString::FromUTF8(Thoth::CorpusDocuments::kUnavailableLabel));
        return;
    }

    m_corpusStatus->SetLabel(wxString::FromUTF8(Thoth::CorpusDocuments::kLoadingLabel));
    m_corpusText->Clear();

    const nlohmann::json body = agent->listCorpusDocuments();
    std::string err;
    if (!Thoth::CorpusDocuments::hasRequiredV1Fields(body, err)) {
        m_corpusStatus->SetLabel(wxString::FromUTF8(Thoth::CorpusDocuments::kUnavailableLabel));
        if (UseAlpGuiPicker()) {
            ReconcileLocalNotesEngine(Thoth::CorpusDocuments::emptyV1List());
        }
        return;
    }

    const auto disposition = Thoth::disposePanelData(
        true, Thoth::CorpusDocuments::isEffectivelyEmpty(body));
    if (disposition == Thoth::PanelDataDisposition::Empty) {
        m_corpusStatus->SetLabel(wxString::FromUTF8(Thoth::CorpusDocuments::kEmptyLabel));
        SyncLocalNotesFromCorpus(body);
        ReconcileLocalNotesEngine(body);
        return;
    }

    m_corpusStatus->SetLabel(
        wxString::FromUTF8(Thoth::CorpusDocuments::kInventoryPopulatedLabel));
    m_corpusStatus->SetToolTip(wxString::FromUTF8(
        Thoth::RetrievalVerificationDisplay::kInventoryLayerHint));

    wxString inventory;
    const bool alp_gui = UseAlpGuiPicker();
    for (const auto& doc : body["documents"]) {
        if (!doc.is_object()) {
            continue;
        }
        wxString line = wxString::FromUTF8(doc.value("name", ""));
        const std::string docId = doc.value("id", "");
        if (!docId.empty()) {
            const std::string id_display = alp_gui
                ? Thoth::LocalNoteEngineSync::formatUuidShort(docId)
                : docId;
            line += wxString::FromUTF8(" · id=" + id_display);
        }
        const std::string status = doc.value("status", "");
        if (!status.empty()) {
            line += wxString::FromUTF8(" · " + status);
        }
        if (status == "failed" && doc.contains("reason") && doc["reason"].is_string()) {
            const std::string reason = doc["reason"].get<std::string>();
            if (!reason.empty()) {
                line += wxString::FromUTF8(" \u00b7 " + reason);
            }
        }
        if (doc.contains("chunk_count") && !doc["chunk_count"].is_null()) {
            if (doc["chunk_count"].is_number_integer()) {
                line += wxString::FromUTF8(" \u00b7 ");
                line += wxString::Format("%d chunks", doc["chunk_count"].get<int>());
            }
        }
        if (!inventory.empty()) {
            inventory += "\n";
        }
        inventory += line;
    }
    m_corpusText->ChangeValue(inventory);

    SyncLocalNotesFromCorpus(body);
    ReconcileLocalNotesEngine(body);

    m_auiManager.Update();
}

void MainFrame::ApplyIngestControls(const Thoth::EventStreamSnapshot& snap) {
    if (!m_sendToEngineBtn) {
        return;
    }
    const bool engine_usable = !snap.applies || Thoth::engineHttpUsable(snap.engine);
    const bool canIngest = agent && agent->capabilities().supportsIngest;
    const bool alp_gui = UseAlpGuiPicker();

    bool hasNote = false;
    bool allNotesSent = false;
    if (m_activeSessionIndex >= 0
        && m_activeSessionIndex < static_cast<int>(m_sessions.size())) {
        const auto& session =
            m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
        if (alp_gui) {
            const auto candidates =
                Thoth::LocalNoteEngineSync::collectPickerCandidates(m_localNoteIntents);
            hasNote = !candidates.empty();
            allNotesSent = !session.ragFilePaths.empty() && candidates.empty()
                           && Thoth::LocalNoteEngineSync::reconcileAllowsSend(
                               m_localNoteReconcileState, alp_gui);
        } else {
            const auto unsent =
                Thoth::LocalNoteEngineSync::collectUnsentLocalNotePaths(session);
            hasNote = !unsent.empty();
            allNotesSent = !session.ragFilePaths.empty() && unsent.empty();
        }
    }

    const bool reconcile_ready =
        Thoth::LocalNoteEngineSync::reconcileAllowsSend(m_localNoteReconcileState, alp_gui);
    m_sendToEngineBtn->Show(canIngest || hasNote);
    m_sendToEngineBtn->Enable(canIngest && hasNote && engine_usable && reconcile_ready);
    if (!canIngest) {
        m_sendToEngineBtn->SetToolTip(
            wxString::FromUTF8("Document ingest unavailable with the current backend"));
    } else if (!engine_usable) {
        m_sendToEngineBtn->SetToolTip(
            wxString::FromUTF8("Engine is not ready — try again when Engine: Ready"));
    } else if (alp_gui && !reconcile_ready) {
        m_sendToEngineBtn->SetToolTip(
            wxString::FromUTF8("Checking send eligibility with Engine…"));
    } else if (!hasNote) {
        m_sendToEngineBtn->SetToolTip(
            wxString::FromUTF8(allNotesSent
                                   ? "All Local Notes in this session have already been sent"
                                   : "Add a Local Note (drop or Import Corpus) first"));
    } else {
        m_sendToEngineBtn->SetToolTip(
            wxString::FromUTF8(
                "Send the Local Note to Engine corpus for this chat session "
                "(Engine list shows full inventory; retrieval uses session scope)"));
    }
}

void MainFrame::OnSendToEngine(wxCommandEvent& WXUNUSED(evt)) {
    if (!agent || !agent->capabilities().supportsIngest) {
        SetTransientStatus(wxString::FromUTF8(
            "Document ingest unavailable with the current backend"));
        return;
    }
    if (m_activeSessionIndex < 0
        || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        return;
    }
    if (m_sessionId.empty()) {
        SetTransientStatus(wxString::FromUTF8("Select or create a chat session first"));
        return;
    }

    const auto& session =
        m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
    const bool alp_gui = UseAlpGuiPicker();

    std::vector<std::string> picker_paths;
    std::vector<Thoth::LocalNoteEngineSync::LocalNoteIntent> picker_intents;
    if (alp_gui) {
        picker_intents = Thoth::LocalNoteEngineSync::collectPickerCandidates(m_localNoteIntents);
        picker_paths.reserve(picker_intents.size());
        for (const auto& intent : picker_intents) {
            picker_paths.push_back(intent.host_path);
        }
    } else {
        picker_paths = Thoth::LocalNoteEngineSync::collectUnsentLocalNotePaths(session);
    }

    if (picker_paths.empty()) {
        if (session.ragFilePaths.empty()) {
            SetTransientStatus(wxString::FromUTF8("Add a Local Note first"));
        } else if (alp_gui
                   && m_localNoteReconcileState
                          == Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Unknown) {
            SetTransientStatus(wxString::FromUTF8(
                "Send eligibility not verified yet — wait for Engine reconcile"));
        } else if (alp_gui
                   && m_localNoteReconcileState
                          == Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Unverified) {
            SetTransientStatus(wxString::FromUTF8(
                "Engine not ready — Send disabled until reconcile succeeds"));
        } else {
            SetTransientStatus(wxString::FromUTF8(
                "All Local Notes in this session have already been sent to Engine"));
        }
        return;
    }

    std::string selected_path;
    const Thoth::LocalNoteEngineSync::LocalNoteIntent* selected_intent = nullptr;
    if (picker_paths.size() == 1) {
        selected_path = picker_paths.front();
        if (alp_gui && !picker_intents.empty()) {
            selected_intent = &picker_intents.front();
        }
    } else {
        wxArrayString choices;
        for (std::size_t i = 0; i < picker_paths.size(); ++i) {
            wxString line = wxString::FromUTF8(
                std::filesystem::path(picker_paths[i]).filename().string());
            if (alp_gui && i < picker_intents.size()) {
                line += wxString::FromUTF8(
                    " · " + Thoth::LocalNoteEngineSync::actionPickerLabel(
                                picker_intents[i].action));
            }
            choices.Add(line);
        }
        wxSingleChoiceDialog dialog(this,
                                    wxString::FromUTF8("Choose a Local Note to send:"),
                                    wxString::FromUTF8("Send to Engine"),
                                    choices);
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }
        const int index = dialog.GetSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= picker_paths.size()) {
            return;
        }
        selected_path = picker_paths[static_cast<std::size_t>(index)];
        if (alp_gui && static_cast<std::size_t>(index) < picker_intents.size()) {
            selected_intent = &picker_intents[static_cast<std::size_t>(index)];
        }
    }

    bool force_replace = false;
    if (alp_gui && selected_intent && selected_intent->action == "conflict") {
        if (!ConfirmForceReplace(selected_path, selected_intent->reason)) {
            return;
        }
        force_replace = true;
    }

    SendLocalNoteToEngine(selected_path, force_replace);
}

void MainFrame::SendLocalNoteToEngine(const std::string& host_path, bool force_replace) {
    if (!agent || host_path.empty()) {
        return;
    }
    agent->setSessionId(m_sessionId);
    SetTransientStatus(wxString::FromUTF8("Sending document to Engine…"));
    Thoth::CorpusCreateGuiOptions options;
    options.force_replace = force_replace;
    agent->createCorpusDocument(host_path, options);
}

void MainFrame::RenderSession(std::size_t sessionIndex) {
    if (!m_chatContainer || !m_chatInnerPanel || !m_chatSizer || sessionIndex >= m_sessions.size()) {
        return;
    }

    const Thoth::ChatSession& session = m_sessions[sessionIndex];

    m_chatSizer->Clear(true);

    for (const auto& message : session.messages) {
        bool isUser = (message.role == "user");
        std::string safeContent = message.content;
        if (safeContent.size() > 50000) {
            safeContent = safeContent.substr(0, 50000) + "\n... [TRUNCATED] ...";
        }
        ChatMessagePanel* bubble = new ChatMessagePanel(
            m_chatInnerPanel, wxString::FromUTF8(safeContent), isUser);
        m_chatSizer->Add(bubble, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
    }

    ScrollChatToBottom();
}

void MainFrame::ScrollChatToBottom() {
    if (!m_chatContainer || !m_chatInnerPanel || !m_chatSizer) {
        return;
    }

    const int width = std::max(m_chatContainer->GetClientSize().GetWidth(), 1);
    m_chatInnerPanel->SetMinSize(wxSize(width, -1));

    m_chatInnerPanel->Layout();
    m_chatSizer->Layout();
    m_chatContainer->FitInside();

    const wxSize client = m_chatContainer->GetClientSize();
    const wxSize virt = m_chatContainer->GetVirtualSize();
    const int maxScrollY = std::max(0, virt.GetHeight() - client.GetHeight());
    m_chatContainer->Scroll(0, maxScrollY);
}

void MainFrame::OnChatContainerSize(wxSizeEvent& evt) {
    if (m_chatContainer && m_chatInnerPanel) {
        const int width = std::max(m_chatContainer->GetClientSize().GetWidth(), 1);
        m_chatInnerPanel->SetMinSize(wxSize(width, -1));
        m_chatInnerPanel->Layout();
        m_chatContainer->FitInside();
    }
    evt.Skip();
}

bool MainFrame::SyncAgentMemoryFromActiveSession(bool includeRagFiles) {
    if (!agent || m_activeSessionIndex < 0 || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        return false;
    }

    const Thoth::ChatSession& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];

    // Phase 10: Engine owns conversation — no host memory sync.
    if (agent->capabilities().supportsConversation) {
        if (!session.activeGoal.empty()) {
            RefreshGoalBanner();
        }
        return false;
    }

    // Remote (Plan K): conversation/RAG/resume sync have no HTTP APIs — skip no-ops.
    if (agent->isRemote()) {
        if (!session.activeGoal.empty()) {
            RefreshGoalBanner();
        }
        return false;
    }

    auto sessionCopy = session;
    MigrateFilesToSandbox(sessionCopy.ragFilePaths);
    
    std::vector<Memory::TimedMessage> memoryMessages;
    memoryMessages.reserve(sessionCopy.messages.size());
    for (const auto& message : sessionCopy.messages) {
        Memory::TimedMessage timed;
        timed.role = message.role;
        timed.content = message.content;
        timed.timestamp_ms = message.timestampMs;
        memoryMessages.push_back(std::move(timed));
    }

    if (includeRagFiles) {
        agent->setRagFiles(sessionCopy.ragFilePaths);
    }
    bool ok = agent->loadConversationMemorySync(memoryMessages, BuildMemorySummary(sessionCopy));
    if (ok) {
        agent->checkResumablePlan();
        if (m_activeSessionIndex >= 0
            && m_activeSessionIndex < static_cast<int>(m_sessions.size())) {
            TrimSessionMessagesForPersistence(m_sessions[static_cast<std::size_t>(m_activeSessionIndex)]);
        }
    }
    
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
    m_localNoteIntents.clear();
    m_localNoteReconcileState = UseAlpGuiPicker()
        ? Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Unknown
        : Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Ready;

    if (agent) {
        agent->setSessionId(m_sessionId);
    }

    RefreshSessionConversationFromEngine(m_sessionId);

    RenderSession(sessionIndex);
    RefreshChatList();
    RefreshRagPanel(); // New: Update the RAG file panel
    const bool memoryLoaded = SyncAgentMemoryFromActiveSession();
    UpdateBackendModeBanner();
    if (agent && agent->isRemote()) {
        SetTransientStatus("Host memory/RAG not synced with Engine");
    } else {
        SetTransientStatus(memoryLoaded ? "Loaded selected chat into memory"
                                   : "Selected chat (memory sync unavailable)");
    }
    RefreshGoalBanner();
    RefreshAllPanels();
    UpdateChatSendChrome();
}

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Thoth Control Panel", wxDefaultPosition, wxSize(1000, 700))
{
    SetMinSize(wxSize(800, 600));
    FileHandler fileHandler;
    m_chatSessionsPath = fileHandler.getAgentWorkspacePath("chat_sessions.json");

    agent = std::make_unique<AgentInterface>();

    agent->onOperationComplete = [this](const Thoth::OperationResult& result,
                                        const std::string& requestId) {
        HandleOperationComplete(result, requestId);
    };

    // --- Wire Observability events (Phase 2) ---
    agent->onEvent = [this](const ControllerEvent& ev) {
        // Explicitly capture metadata by value to avoid lifetime issues
        nlohmann::json metadata = ev.metadata;
        EventType type = ev.type;
        std::string stepId = ev.step_id;
        std::string eventSessionId = ev.session_id;
        std::string controllerState = ev.controller_state_name;

        wxTheApp->CallAfter([this, type, metadata, stepId, eventSessionId, controllerState]() {
            // Safety: Check if this window still exists
            if (!wxPendingDelete.Member(this) && wxWindow::FindWindowById(GetId())) {
                bool isActiveSession = (eventSessionId.empty() || eventSessionId == this->m_sessionId);
                const bool retrievalForTab =
                    Thoth::RetrievalVerificationDisplay::retrievalDiagnosticsTargetsSession(
                        eventSessionId, this->m_sessionId);
                
                std::cerr << "[MainFrame] onEvent: type=" << (int)type 
                          << ", evSid=" << eventSessionId 
                          << ", curSid=" << this->m_sessionId 
                          << ", active=" << (isActiveSession ? "YES" : "NO") << "\n";

                if (type == EventType::RETRIEVAL_DIAGNOSTICS) {
                    std::cerr << "[MainFrame] Received RETRIEVAL_DIAGNOSTICS event.\n";
                    if (retrievalForTab) {
                        if (this->m_gragPanel) this->m_gragPanel->UpdateDiagnostics(metadata);
                        if (this->m_graphPanel) this->m_graphPanel->UpdateControllerState("EXECUTING_RETRIEVAL");
                    }
                } else if (type == EventType::MODE_SWITCHED) {
                    if (isActiveSession && this->m_graphPanel) {
                        this->m_graphPanel->UpdateControllerState("SCIENTIFIC_MODE");
                    }
                } else if (type == EventType::STATE_CHANGED) {
                    if (isActiveSession) {
                        const auto progressSrc = this->ActiveBackendProgressSource();
                        if (controllerState == "PLANNING") {
                            m_goalPlanningPending = true;
                            this->ApplyWorkActivity("Planning…", progressSrc);
                        } else if (controllerState == "REVISING_PLAN") {
                            this->ApplyWorkActivity("Revising plan…", progressSrc);
                        }
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
                            m_goalPlanningPending = false;
                            const auto& plan = metadata["plan"];
                            const bool hasSteps = plan.contains("steps") && plan["steps"].is_array()
                                && !plan["steps"].empty();
                            if (this->m_stateStrip) {
                                if (hasSteps) {
                                    this->m_stateStrip->ResetPlan(plan);
                                } else {
                                    this->ApplyWorkActivity(
                                        "Plan generation failed",
                                        this->ActiveBackendProgressSource());
                                }
                            }
                            if (this->m_planPanel) {
                                this->m_planPanel->ResetPlan(plan);
                                this->m_planPanel->SetExecutionState(hasSteps ? "Running" : "Failed");
                            }
                        }
                        if (metadata["plan"].contains("goal") && metadata["plan"]["goal"].is_string()) {
                            const std::string targetSid =
                                this->ResolveGoalEventSessionId(eventSessionId);
                            if (!targetSid.empty()) {
                                const bool knownSession = std::any_of(
                                    this->m_sessions.begin(),
                                    this->m_sessions.end(),
                                    [&targetSid](const Thoth::ChatSession& session) {
                                        return session.id == targetSid;
                                    });
                                if (knownSession) {
                                    SetSessionGoal(targetSid,
                                                   metadata["plan"]["goal"].get<std::string>());
                                }
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
                    Thoth::LocalNoteEngineSync::IndexingEventMetadata indexing_meta;
                    indexing_meta.file_path = metadata.value("file_path", "");
                    indexing_meta.document_id = metadata.value("document_id", "");
                    indexing_meta.revision_id = metadata.value("revision_id", "");
                    if (metadata.contains("document_name")
                        && metadata["document_name"].is_string()) {
                        indexing_meta.document_name =
                            metadata["document_name"].get<std::string>();
                    }
                    const auto progressSrc = this->ActiveBackendProgressSource();
                    if (isActiveSession && Thoth::mayApplyIndexingProgress(progressSrc)) {
                        ++m_ragIndexingCount;
                        RefreshExecutiveStripActivity();
                    }
                    wxFileName fn(wxString::FromUTF8(
                        Thoth::LocalNoteEngineSync::ragEventBasename(indexing_meta.file_path)));
                    this->ApplyWorkStatus("Indexing: " + fn.GetFullName(), progressSrc);
                    if (Thoth::mayApplyIndexingProgress(progressSrc)) {
                        ApplyLocalNoteIndexingStarted(indexing_meta);
                    }
                } else if (type == EventType::INDEXING_COMPLETED) {
                    Thoth::LocalNoteEngineSync::IndexingEventMetadata indexing_meta;
                    indexing_meta.file_path = metadata.value("file_path", "");
                    indexing_meta.document_id = metadata.value("document_id", "");
                    indexing_meta.revision_id = metadata.value("revision_id", "");
                    if (metadata.contains("document_name")
                        && metadata["document_name"].is_string()) {
                        indexing_meta.document_name =
                            metadata["document_name"].get<std::string>();
                    }
                    const auto progressSrc = this->ActiveBackendProgressSource();
                    if (isActiveSession && Thoth::mayApplyIndexingProgress(progressSrc)) {
                        m_ragIndexingCount = std::max(0, m_ragIndexingCount - 1);
                        RefreshExecutiveStripActivity();
                    }
                    wxFileName fn(wxString::FromUTF8(
                        Thoth::LocalNoteEngineSync::ragEventBasename(indexing_meta.file_path)));
                    if (metadata.contains("success")) {
                        const bool ok = metadata.value("success", false);
                        if (ok) {
                            this->ApplyWorkStatus(
                                "Indexing succeeded: " + fn.GetFullName(), progressSrc);
                        } else {
                            std::string reason = metadata.value("reason", "");
                            wxString msg = wxString::FromUTF8("Indexing failed: ")
                                + fn.GetFullName();
                            if (!reason.empty()) {
                                msg += wxString::FromUTF8(" (" + reason + ")");
                            }
                            this->ApplyWorkStatus(msg, progressSrc);
                        }
                        if (Thoth::mayApplyIndexingProgress(progressSrc)) {
                            int chunk_count = -1;
                            if (metadata.contains("chunk_count")
                                && metadata["chunk_count"].is_number_integer()) {
                                chunk_count = metadata["chunk_count"].get<int>();
                            }
                            ApplyLocalNoteIndexingCompleted(
                                indexing_meta, metadata.value("success", false), chunk_count);
                        }
                    } else {
                        this->ApplyWorkStatus(
                            "Indexing finished: " + fn.GetFullName(), progressSrc);
                        if (Thoth::mayApplyIndexingProgress(progressSrc)) {
                            ApplyLocalNoteIndexingCompleted(indexing_meta, false, -1);
                        }
                    }
                    RefreshRagPanel();
                    if (agent && agent->capabilities().supportsCorpusList) {
                        RefreshCorpusPanel();
                    }
                } else if (type == EventType::PLAN_COMPLETED) {
                    m_goalPlanningPending = false;
                    this->ApplyWorkStatus(
                        "Goal completed successfully",
                        this->ActiveBackendProgressSource());
                    ClearSessionGoal(this->ResolveGoalEventSessionId(eventSessionId));
                    if (isActiveSession) {
                        if (this->m_typingIndicator) {
                            this->m_typingIndicator->Hide();
                        }
                        if (this->m_planPanel) {
                            this->m_planPanel->SetExecutionState("Completed");
                        }
                        if (this->m_inputCtrl) {
                            this->m_inputCtrl->SetFocus();
                        }
                    }
                    this->RefreshAllPanels();
                } else if (type == EventType::PLAN_FAILED) {
                    ClearSessionGoal(this->ResolveGoalEventSessionId(eventSessionId));
                    if (isActiveSession) {
                        m_goalPlanningPending = false;
                        RefreshExecutiveStripActivity();
                        if (this->m_typingIndicator) {
                            this->m_typingIndicator->Hide();
                        }
                        if (this->m_planPanel) this->m_planPanel->SetExecutionState("Failed");
                    }
                } else if (type == EventType::PLAN_ABORTED) {
                    ClearSessionGoal(this->ResolveGoalEventSessionId(eventSessionId));
                    if (isActiveSession) {
                        m_goalPlanningPending = false;
                        RefreshExecutiveStripActivity();
                        if (this->m_typingIndicator) {
                            this->m_typingIndicator->Hide();
                        }
                        if (this->m_planPanel) this->m_planPanel->SetExecutionState("Aborted");
                    }
                } else if (type == EventType::PLAN_REUSE_INJECTION) {
                    std::cerr << "[MainFrame] PLAN_REUSE_INJECTION source="
                              << metadata.value("source", "unknown")
                              << " count=" << metadata.value("plan_count", 0) << "\n";
                    if (isActiveSession) {
                        this->ApplyWorkStatus(wxString::Format(
                            "Plan reuse: %d similar past plan(s) from %s",
                            metadata.value("plan_count", 0),
                            wxString::FromUTF8(metadata.value("source", "unknown"))),
                            this->ActiveBackendProgressSource());
                    }
                } else if (type == EventType::REFLECTION_REPLAN) {
                    std::cerr << "[MainFrame] REFLECTION_REPLAN score="
                              << metadata.value("trajectory_score", 0.0f)
                              << " cycle=" << metadata.value("reflection_cycle", 0) << "\n";
                    if (isActiveSession) {
                        this->ApplyWorkStatus(wxString::Format(
                            "Reflection replan (score %.2f, cycle %d)",
                            metadata.value("trajectory_score", 0.0f),
                            metadata.value("reflection_cycle", 0)),
                            this->ActiveBackendProgressSource());
                    }
                } else if (type == EventType::PLAN_HISTORY_STORED) {
                    std::cerr << "[MainFrame] PLAN_HISTORY_STORED plan_id="
                              << metadata.value("plan_id", "")
                              << " score=" << metadata.value("success_score", 0.0f) << "\n";
                    if (isActiveSession) {
                        this->ApplyWorkStatus(
                            "Plan history saved to past_plans + cognate_plans",
                            this->ActiveBackendProgressSource());
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
    m_leftSidebar->SetDropTarget(new FileDropTarget(this));

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
    m_chatContainer->SetMinSize(wxSize(80, 80));

    m_chatInnerPanel = new wxPanel(m_chatContainer, wxID_ANY);
    m_chatInnerPanel->SetBackgroundColour(*wxWHITE);
    m_chatSizer = new wxBoxSizer(wxVERTICAL);
    m_chatInnerPanel->SetSizer(m_chatSizer);

    wxBoxSizer* chatScrollSizer = new wxBoxSizer(wxVERTICAL);
    chatScrollSizer->Add(m_chatInnerPanel, 0, wxEXPAND);
    m_chatContainer->SetSizer(chatScrollSizer);
    m_chatContainer->Bind(wxEVT_SIZE, &MainFrame::OnChatContainerSize, this);

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

    // Keep the prompt anchored: chat scrolls internally instead of pushing input off-screen.
    centerSizer->SetSizeHints(centerPanel);
    
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
    
    // 1. RAG Files Tab — splitter keeps Engine inventory + Local Notes both visible
    m_ragTab = new wxPanel(m_bottomNotebook, wxID_ANY);
    wxBoxSizer* ragTabOuter = new wxBoxSizer(wxVERTICAL);
    wxSplitterWindow* ragSplit =
        new wxSplitterWindow(m_ragTab, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxSP_LIVE_UPDATE | wxSP_3D);
    ragSplit->SetMinimumPaneSize(80);

    wxPanel* corpusPanel = new wxPanel(ragSplit, wxID_ANY);
    wxBoxSizer* corpusSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* corpusHeader = new wxStaticText(
        corpusPanel, wxID_ANY,
        wxString::FromUTF8("Engine inventory (read-only — not session attachments)"));
    corpusHeader->Wrap(400);
    m_corpusStatus = new wxStaticText(corpusPanel, wxID_ANY,
        wxString::FromUTF8(Thoth::CorpusDocuments::kLoadingLabel));
    m_corpusText = new wxTextCtrl(
        corpusPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 80),
        wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxHSCROLL);
    m_corpusText->SetToolTip(wxString::FromUTF8("Select text and Ctrl+C to copy"));
    corpusSizer->Add(corpusHeader, 0, wxEXPAND | wxALL, 5);
    corpusSizer->Add(m_corpusStatus, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    corpusSizer->Add(m_corpusText, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    corpusPanel->SetSizer(corpusSizer);

    wxPanel* localNotesPanel = new wxPanel(ragSplit, wxID_ANY);
    wxBoxSizer* localNotesOuter = new wxBoxSizer(wxVERTICAL);
    wxStaticText* localNotesHeader = new wxStaticText(
        localNotesPanel, wxID_ANY,
        wxString::FromUTF8("Local Notes — drop or Import Corpus, then Send to Engine"));
    localNotesHeader->Wrap(400);
    localNotesOuter->Add(localNotesHeader, 0, wxEXPAND | wxALL, 5);

    wxFlexGridSizer* ragSizer = new wxFlexGridSizer(2, 2, 5, 5);
    ragSizer->AddGrowableCol(0, 1);
    ragSizer->AddGrowableCol(1, 1);

    auto createSlotSizer = [this, localNotesPanel](wxStaticText*& slot, wxButton*& btn, int index) {
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        slot = new wxStaticText(localNotesPanel, wxID_ANY, wxString::Format("Empty Slot %d", index),
                                wxDefaultPosition, wxDefaultSize,
                                wxST_ELLIPSIZE_END);
        btn = new wxButton(localNotesPanel, wxID_ANY, "X", wxDefaultPosition, wxSize(28, 28));
        btn->SetToolTip("Remove file");
        slot->SetMinSize(wxSize(80, 22));

        sizer->Add(slot, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);
        sizer->Add(btn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        sizer->SetMinSize(wxSize(-1, 32));

        btn->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) {
            if (m_activeSessionIndex < 0 || static_cast<size_t>(m_activeSessionIndex) >= m_sessions.size()) return;
            auto& session = m_sessions[static_cast<size_t>(m_activeSessionIndex)];
            if (static_cast<size_t>(index - 1) < session.ragFilePaths.size()) {
                const std::string removed = session.ragFilePaths[static_cast<std::size_t>(index - 1)];
                session.ragFilePaths.erase(session.ragFilePaths.begin() + (index - 1));
                session.localNoteEngine.erase(removed);
                SaveChatSessions();
                RefreshRagPanel();
                if (agent
                    && Thoth::RemoteRagHonesty::shouldSyncRagFilesToBackend(agent->isRemote())) {
                    agent->setRagFiles(session.ragFilePaths);
                }
            }
        });

        return sizer;
    };

    ragSizer->Add(createSlotSizer(m_ragFileSlot1, m_ragDeleteBtn1, 1), 1, wxEXPAND);
    ragSizer->Add(createSlotSizer(m_ragFileSlot2, m_ragDeleteBtn2, 2), 1, wxEXPAND);
    ragSizer->Add(createSlotSizer(m_ragFileSlot3, m_ragDeleteBtn3, 3), 1, wxEXPAND);
    ragSizer->Add(createSlotSizer(m_ragFileSlot4, m_ragDeleteBtn4, 4), 1, wxEXPAND);

    localNotesOuter->Add(ragSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

    m_sendToEngineBtn = new wxButton(localNotesPanel,
                                      wxID_ANY,
                                      wxString::FromUTF8("Send to Engine"));
    m_sendToEngineBtn->SetToolTip(
        wxString::FromUTF8("Create an Engine corpus document from a Local Note"));
    localNotesOuter->Add(m_sendToEngineBtn, 0, wxEXPAND | wxALL, 5);
    m_sendToEngineBtn->Bind(wxEVT_BUTTON, &MainFrame::OnSendToEngine, this);

    localNotesPanel->SetSizer(localNotesOuter);
    localNotesPanel->SetMinSize(wxSize(240, 140));

    ragSplit->SplitHorizontally(corpusPanel, localNotesPanel);
    ragSplit->SetSashPosition(180);

    ragTabOuter->Add(ragSplit, 1, wxEXPAND);
    m_ragTab->SetSizer(ragTabOuter);
    m_ragTab->SetDropTarget(new FileDropTarget(this));

    wxPanel* ragTab = m_ragTab;

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
        .MinSize(-1, 280)
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
        if (agent && agent->isRemote()) {
            SetTransientStatus(
                wxString::FromUTF8("Goal cleared for this chat. Use Agent → Abort if a plan is "
                                   "still running on the Engine."));
        }
        ClearActiveGoal();
    });
    
    m_reviseGoalBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_activeSessionIndex < 0 || static_cast<size_t>(m_activeSessionIndex) >= m_sessions.size()) return;
        auto& session = m_sessions[static_cast<size_t>(m_activeSessionIndex)];
        wxString newGoal = wxGetTextFromUser("Revise active goal:", "Revise Goal", wxString::FromUTF8(session.activeGoal), this);
        if (!newGoal.IsEmpty()) {
            SetTransientStatus(wxString::FromUTF8(Thoth::kGoalSubmittedChrome));
            const std::string goalStd = newGoal.ToStdString();
            SetSessionGoal(m_sessionId, goalStd);
            SyncBackendSessionIdentity();
            if (agent) {
                agent->executeGoal(goalStd);
            }
        }
    });

    CreateStatusBar(3);
    int widths[3] = {-2, -3, -1};
    SetStatusWidths(3, widths);
    UpdateBackendModeBanner();

    SetupMenuBar();
    ApplyBenchmarksMenuCapabilities();
    ApplyEngineDegradedControls(agent ? agent->eventStreamSnapshot()
                                      : Thoth::localEventStreamSnapshot(NowMs()));

    m_connectionPollTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &MainFrame::OnConnectionPollTimer, this, m_connectionPollTimer.GetId());
    m_connectionPollTimer.Start(1000);

    LoadChatSessions();
    if (m_sessions.empty()) {
        CreateNewSession("New Chat");
        SaveChatSessions();
    } else {
        SaveChatSessions();
    }
    ActivateSession(0); // Activate the first session (most recent)

    UpdateBackendModeBanner();
    SetTransientStatus("Ready");
}

MainFrame::~MainFrame() {
    m_connectionPollTimer.Stop();
    m_auiManager.UnInit();
    if (agent) {
        agent->onOperationComplete = nullptr;
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
        SetTransientStatus("Full chat copied to clipboard.");
    }
}

void MainFrame::OnSend(wxCommandEvent& WXUNUSED(evt)) {
    wxString input = m_inputCtrl->GetValue();
    input.Trim(true).Trim(false);
    if (input.IsEmpty()) return;
    m_inputCtrl->Clear();

    // Ensure we have a valid session and it's active
    if (m_activeSessionIndex < 0 || m_activeSessionIndex >= static_cast<int>(m_sessions.size()) || m_sessionId.empty()) {
        CreateNewSession("New Chat");
        ActivateSession(m_sessions.size() - 1);
    }

    const std::string activeId = m_sessionId;
    
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
        [&activeId](const Thoth::ChatSession& s) { return s.id == activeId; });
    
    if (it == m_sessions.end()) {
        SetTransientStatus("Error: Active session lost");
        return;
    }

    Thoth::ChatSession& session = *it;
    const bool isGoal = InputStartsGoal(input);
    const wxString goalText = isGoal ? ExtractGoalText(input) : wxString();
    const bool engineConversation =
        agent && agent->capabilities().supportsConversation && !isGoal;

    if (session.messages.empty()) {
        session.title = BuildSessionTitle(input);
    }

    if (!engineConversation) {
        session.messages.push_back({"user", input.ToStdString(), NowMs()});
    }
    session.updatedAtMs = NowMs();
    SaveChatSessions();

    // Sync conversation memory after the new message is stored. Skip RAG re-index for
    // goals so /goal is not blocked behind bulk indexing on the worker thread.
    SyncAgentMemoryFromActiveSession(!isGoal && !engineConversation);
    
    // Refresh UI
    RefreshChatList();
    if (!engineConversation) {
        RenderSession(static_cast<std::size_t>(m_activeSessionIndex));
    }

    if (m_graphPanel) {
        m_graphPanel->ResetNodes();
    }
    m_auiManager.Update();

    if (isGoal) {
        SetTransientStatus(wxString::FromUTF8(Thoth::kGoalSubmittedChrome));
    } else {
        SetTransientStatus(wxString::FromUTF8(Thoth::kMessageSubmittedChrome));
    }

    if (agent) {
        if (isGoal) {
            if (m_typingIndicator) {
                m_typingIndicator->Hide();
            }
            const std::string goalStd = goalText.ToStdString();
            if (goalStd.empty()) {
                SetTransientStatus("Goal text empty — use \"goal: …\" or \"/goal …\"");
            } else {
                std::cerr << "[MainFrame] executeGoal for session " << activeId << "\n";
                SetSessionGoal(activeId, goalStd);
                SyncBackendSessionIdentity();
                agent->executeGoal(goalStd);
                if (!agent->isRemote() && !session.ragFilePaths.empty()) {
                    auto sessionCopy = session;
                    MigrateFilesToSandbox(sessionCopy.ragFilePaths);
                    agent->setRagFiles(sessionCopy.ragFilePaths);
                }
            }
        } else if (engineConversation) {
            SyncBackendSessionIdentity();
            if (agent->workerHasContentionBeforeEnqueue()) {
                SetTransientStatus(
                    wxString::FromUTF8("Waiting for Engine… (message queued behind prior work)"));
            }
            m_typingIndicator->Show();
            ++m_requestCounter;
            const std::string requestId = activeId + "-" + std::to_string(m_requestCounter);
            m_requestToSession[requestId] = activeId;
            RegisterPendingChatRequest(requestId, activeId);
            std::optional<std::string> activeGoal;
            if (!session.activeGoal.empty()) {
                activeGoal = session.activeGoal;
            }
            agent->appendConversationTurn(activeId, input.ToStdString(), requestId, activeGoal);
        } else {
            SyncBackendSessionIdentity();
            m_typingIndicator->Show();
            ++m_requestCounter;
            const std::string requestId = activeId + "-" + std::to_string(m_requestCounter);
            m_requestToSession[requestId] = activeId;
            RegisterPendingChatRequest(requestId, activeId);
            std::cerr << "[MainFrame] Sending request " << requestId << " for session "
                      << activeId << "\n";
            agent->processUserInput(input.ToStdString(), requestId);
        }
    } else {
        wxMessageBox("Agent not initialized.", "Error", wxOK | wxICON_ERROR, this);
    }
    if (m_inputCtrl) {
        m_inputCtrl->SetFocus();
    }
    RefreshAllPanels();
}

void MainFrame::OnShowDecisionTrace(wxCommandEvent& WXUNUSED(evt)) {
    if (!agent) {
        wxMessageBox("Agent is not initialized.", "Explain Plan", wxOK | wxICON_WARNING, this);
        return;
    }

    // Phase 3/4 D11: Unavailable with why when backend lacks plan diagnostics.
    if (!agent->capabilities().supportsPlanDiagnostics) {
        wxMessageBox(
            wxString::FromUTF8(Thoth::CognitiveDiagnostics::formatExplainPlanUnavailableBody()),
            wxString::FromUTF8(Thoth::CognitiveDiagnostics::kExplainPlanDialogTitle),
            wxOK | wxICON_INFORMATION,
            this);
        return;
    }

    // Phase 4: render Engine-authored structured decision summary (D12/D13).
    const nlohmann::json summary = agent->getLatestDecisionSummary();
    std::string body;
    if (Thoth::DecisionSummary::isEffectivelyEmpty(summary)) {
        body = "No decision summary available.";
    } else {
        body = Thoth::DecisionSummary::formatForDisplay(summary);
    }
    wxMessageBox(
        wxString::FromUTF8(body),
        wxString::FromUTF8(Thoth::CognitiveDiagnostics::kExplainPlanDialogTitle),
        wxOK | wxICON_INFORMATION,
        this);
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
    SetTransientStatus("New chat created");
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
        SetTransientStatus("Delete canceled");
        return;
    }

    const std::string deletedSessionId = m_sessions[static_cast<size_t>(sessionIndexToDelete)].id;
    std::erase_if(m_requestToSession, [&deletedSessionId](const auto& entry) {
        return entry.second == deletedSessionId;
    });
    m_inFlightChatBySession.erase(deletedSessionId);

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
    SetTransientStatus("Chat deleted");
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
        
        // Check for duplicates (exact path or same basename slot)
        bool isDuplicate = false;
        const std::string incoming_path = filename.ToStdString();
        const std::string incoming_base =
            std::filesystem::path(incoming_path).filename().string();
        for (const auto& existingPath : session.ragFilePaths) {
            if (existingPath == incoming_path) {
                isDuplicate = true;
                break;
            }
            if (!incoming_base.empty()
                && std::filesystem::path(existingPath).filename().string() == incoming_base) {
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
        const bool hostOnlyNotes =
            agent && Thoth::RemoteRagHonesty::localNotesAreHostSideOnly(agent->isRemote());
        // Phase 5: never invent indexing from the drop itself.
        if (hostOnlyNotes) {
            SetTransientStatus(wxString::FromUTF8(
                Thoth::RemoteRagHonesty::formatHostOnlyAddStatus(filesAddedCount)));
        } else {
            SetTransientStatus(wxString::FromUTF8(
                Thoth::formatFilesAddedChromeStatus(filesAddedCount)));
        }
        if (agent
            && Thoth::RemoteRagHonesty::shouldSyncRagFilesToBackend(agent->isRemote())) {
            agent->setRagFiles(session.ragFilePaths);
        }
        if (hostOnlyNotes && agent && agent->capabilities().supportsIngest) {
            RefreshCorpusPanel();
        }
        return true;
    }

    if (!filenames.IsEmpty()) {
        SetTransientStatus(wxString::FromUTF8("File(s) already in this session"));
    }
    return false;
}

namespace {

Thoth::PanelPresentationState ResearchCollectionDisposition(const nlohmann::json& body) {
    using namespace Thoth;
    if (ResearchResources::isFetchError(body)) {
        return PanelPresentationState::Error;
    }
    std::string err;
    if (!ResearchResources::hasRequiredCollectionFields(body, err)) {
        return PanelPresentationState::Error;
    }
    if (ResearchResources::isEffectivelyEmpty(body)) {
        return PanelPresentationState::Empty;
    }
    return PanelPresentationState::Populated;
}

Thoth::PanelPresentationState GraphStatisticsDisposition(const nlohmann::json& body) {
    using namespace Thoth;
    if (GraphStatistics::isFetchError(body)) {
        return PanelPresentationState::Error;
    }
    std::string err;
    if (!GraphStatistics::hasRequiredFields(body, err)) {
        return PanelPresentationState::Error;
    }
    if (GraphStatistics::isEffectivelyEmpty(body)) {
        return PanelPresentationState::Empty;
    }
    return PanelPresentationState::Populated;
}

} // namespace

void MainFrame::RefreshAllPanels() {
    if (!agent) return;

    const auto caps = agent->capabilities();
    const wxString unavailable =
        wxString::FromUTF8(Thoth::kUnavailableWithCurrentBackend);

    if (m_strategyPanel) {
        if (!caps.supportsStrategies) {
            m_strategyPanel->SetPresentationState(
                Thoth::PanelPresentationState::Unavailable, unavailable);
        } else {
            m_strategyPanel->SetPresentationState(Thoth::PanelPresentationState::Loading);
            const nlohmann::json body = agent->getStrategies();
            const auto disposition = ResearchCollectionDisposition(body);
            if (disposition == Thoth::PanelPresentationState::Error) {
                m_strategyPanel->SetPresentationState(
                    Thoth::PanelPresentationState::Error, "Error loading strategies.");
            } else if (disposition == Thoth::PanelPresentationState::Empty) {
                m_strategyPanel->SetPresentationState(Thoth::PanelPresentationState::Empty);
            } else {
                m_strategyPanel->UpdateStrategies(Thoth::ResearchResources::itemsArray(body));
            }
        }
    }
    if (m_trajectoryViewer) {
        if (!caps.supportsTrajectories) {
            m_trajectoryViewer->SetPresentationState(
                Thoth::PanelPresentationState::Unavailable, unavailable);
        } else {
            m_trajectoryViewer->SetPresentationState(Thoth::PanelPresentationState::Loading);
            const nlohmann::json trajBody = agent->getTrajectories();
            const auto trajDisposition = ResearchCollectionDisposition(trajBody);
            if (trajDisposition == Thoth::PanelPresentationState::Error) {
                m_trajectoryViewer->SetPresentationState(
                    Thoth::PanelPresentationState::Error, "Error loading trajectories.");
            } else {
                nlohmann::json episodeItems = nlohmann::json::array();
                bool episodeError = false;
                if (caps.supportsEpisodes) {
                    const nlohmann::json epBody = agent->getEpisodes();
                    const auto epDisposition = ResearchCollectionDisposition(epBody);
                    if (epDisposition == Thoth::PanelPresentationState::Error) {
                        episodeError = true;
                    } else {
                        episodeItems = Thoth::ResearchResources::itemsArray(epBody);
                    }
                }
                if (episodeError) {
                    m_trajectoryViewer->SetPresentationState(
                        Thoth::PanelPresentationState::Error, "Error loading episodes.");
                } else {
                    const bool trajEmpty =
                        trajDisposition == Thoth::PanelPresentationState::Empty;
                    const bool epEmpty = !caps.supportsEpisodes || episodeItems.empty();
                    if (trajEmpty && epEmpty) {
                        m_trajectoryViewer->SetPresentationState(
                            Thoth::PanelPresentationState::Empty);
                    } else {
                        m_trajectoryViewer->UpdateTrajectories(
                            Thoth::ResearchResources::itemsArray(trajBody), episodeItems);
                    }
                }
            }
        }
    }
    if (m_experimentLab) {
        if (!caps.supportsExperiments) {
            m_experimentLab->SetPresentationState(
                Thoth::PanelPresentationState::Unavailable, unavailable);
        } else {
            m_experimentLab->UpdateExperiments(agent->getExperiments());
        }
    }
    if (m_graphPanel) {
        if (!caps.supportsGraphStats) {
            m_graphPanel->SetPresentationState(
                Thoth::PanelPresentationState::Unavailable, unavailable);
        } else {
            m_graphPanel->SetPresentationState(Thoth::PanelPresentationState::Loading);
            const nlohmann::json body = agent->getGraphStats();
            const auto disposition = GraphStatisticsDisposition(body);
            if (disposition == Thoth::PanelPresentationState::Error) {
                m_graphPanel->SetPresentationState(
                    Thoth::PanelPresentationState::Error, "Error loading graph stats.");
            } else if (disposition == Thoth::PanelPresentationState::Empty) {
                m_graphPanel->SetPresentationState(Thoth::PanelPresentationState::Empty);
            } else {
                m_graphPanel->UpdateGraphStats(Thoth::GraphStatistics::statisticsPayload(body));
            }
        }
    }

    RefreshCorpusPanel();

    if (m_logText) {
        if (!caps.supportsLogs) {
            // Phase 3: capability-specific why (not bare Unavailable alone).
            m_logText->SetValue(wxString::FromUTF8(
                std::string("Unavailable\n\n")
                + Thoth::CognitiveDiagnostics::kLogsNotExposedWhy));
        } else {
            FileHandler fileHandler;
            std::string tracePath = fileHandler.getAgentWorkspacePath("decision_trace.jsonl");
            if (std::filesystem::exists(tracePath)) {
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
                        while (start < content.size()
                               && (static_cast<unsigned char>(content[start]) & 0xC0) == 0x80) {
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
    }
}

void MainFrame::UpdateBackendModeBanner() {
    if (!GetStatusBar()) {
        return;
    }
    if (!agent) {
        SetStatusText("Backend: —", 0);
        return;
    }
    SetStatusText(wxString::FromUTF8(agent->backendModeLabel()), 0);
    RefreshEventStreamIndicators();
}

void MainFrame::RefreshEventStreamIndicators() {
    if (!GetStatusBar() || GetStatusBar()->GetFieldsCount() < 2 || !agent) {
        return;
    }

    const auto snap = agent->eventStreamSnapshot();
    ApplyEngineDegradedControls(snap);

    if (!snap.applies) {
        SetStatusText("", 1);
        if (m_gragPanel) {
            m_gragPanel->UpdateLastEventAgeLabel("");
        }
        return;
    }

    wxString secondary;
    if (Thoth::shouldShowConnectionIndicator(snap)) {
        secondary = wxString::FromUTF8(Thoth::formatEventsStatusLine(snap.connection));
    }
    if (Thoth::shouldShowEngineIndicator(snap)) {
        if (!secondary.IsEmpty()) {
            secondary << " · ";
        }
        secondary << wxString::FromUTF8(Thoth::formatEngineStatusLine(snap.engine));
    }
    SetStatusText(secondary, 1);

    if (m_gragPanel) {
        m_gragPanel->UpdateLastEventAgeLabel(wxString::FromUTF8(
            Thoth::formatLastEventAgeLabel(snap.last_event_ms, snap.snapshot_ms)));
    }
}

void MainFrame::ApplyEngineDegradedControls(const Thoth::EventStreamSnapshot& snap) {
    const bool engine_usable = !snap.applies || Thoth::engineHttpUsable(snap.engine);
    if (m_sendButton) {
        m_sendButton->Enable(engine_usable);
        m_sendButton->SetToolTip(engine_usable
            ? wxString()
            : wxString::FromUTF8("Engine is not ready — try again when Engine: Ready"));
    }
    if (m_reviseGoalBtn) {
        m_reviseGoalBtn->Enable(engine_usable);
    }
    if (wxMenuBar* bar = GetMenuBar()) {
        bar->Enable(ID_MENU_AGENT_RUN_GOAL, engine_usable);
        bar->Enable(ID_MENU_AGENT_PAUSE, engine_usable);
        bar->Enable(ID_MENU_AGENT_RESUME, engine_usable);
        bar->Enable(ID_MENU_AGENT_ABORT, engine_usable);
    }
    ApplyIngestControls(snap);
}

void MainFrame::OnConnectionPollTimer(wxTimerEvent& WXUNUSED(evt)) {
    RefreshEventStreamIndicators();
    if (!agent || !agent->capabilities().supportsCorpusList || !HasPendingLocalNoteIndexing()) {
        return;
    }
    const std::int64_t now = NowMs();
    if (now - m_lastCorpusPollForIndexingMs < 5000) {
        return;
    }
    m_lastCorpusPollForIndexingMs = now;
    RefreshCorpusPanel();
}

void MainFrame::SetTransientStatus(const wxString& text) {
    const int field = (GetStatusBar() && GetStatusBar()->GetFieldsCount() > 2) ? 2 : 0;
    if (GetStatusBar() && GetStatusBar()->GetFieldsCount() > 1) {
        SetStatusText(text, field);
    } else {
        SetStatusText(text);
    }
}

void MainFrame::ApplyBenchmarksMenuCapabilities() {
    wxMenuBar* bar = GetMenuBar();
    if (!bar) return;
    const bool enabled = !agent || agent->capabilities().supportsBenchmarks;
    bar->Enable(ID_MENU_BENCH_RUN_GRAG, enabled);
    bar->Enable(ID_MENU_BENCH_RETRIEVAL_COMPARISON, enabled);
    bar->Enable(ID_MENU_BENCH_STRATEGY_LEARNING, enabled);
    bar->Enable(ID_MENU_BENCH_FULL_SYSTEM, enabled);
    bar->Enable(ID_MENU_BENCH_EXPORT_COGNITIVE_METRICS, enabled);
    if (wxMenuItem* status = bar->FindItem(ID_MENU_BENCH_STATUS)) {
        status->SetItemLabel(enabled
            ? "Status: Available"
            : wxString::FromUTF8(Thoth::kBenchmarksAvailableInLocal));
        status->Enable(false);
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
    SetTransientStatus("Sessions saved.");
}

void MainFrame::OnMenuFileExportSession(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("File → Export Session", "Exporting sessions is not yet implemented.");
}

void MainFrame::OnMenuFileImportCorpus(wxCommandEvent& WXUNUSED(evt)) {
    wxFileDialog dialog(this, "Import Corpus Files", "", "",
        "All files (*.*)|*.*|Text files (*.txt)|*.txt|Markdown (*.md)|*.md",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    wxArrayString paths;
    dialog.GetPaths(paths);
    if (!paths.IsEmpty()) {
        HandleFileDrop(paths);
    }
}

void MainFrame::OnMenuFileExit(wxCommandEvent& WXUNUSED(evt)) {
    Close(true);
}

void MainFrame::OnMenuAgentRunGoal(wxCommandEvent& WXUNUSED(evt)) {
    wxString goal = wxGetTextFromUser("Enter autonomous goal:", "Run Goal", "", this);
    if (goal.IsEmpty()) {
        return;
    }
    if (m_sessionId.empty()) {
        CreateNewSession("New Chat");
        ActivateSession(m_sessions.size() - 1);
    }
    SetTransientStatus(wxString::FromUTF8(Thoth::kGoalSubmittedChrome));
    const std::string goalStd = goal.ToStdString();
    SetSessionGoal(m_sessionId, goalStd);
    SyncBackendSessionIdentity();
    if (agent) {
        agent->executeGoal(goalStd);
    }
}

void MainFrame::OnMenuAgentPause(wxCommandEvent& WXUNUSED(evt)) {
    if (agent) {
        agent->pause();
    }
}

void MainFrame::OnMenuAgentResume(wxCommandEvent& WXUNUSED(evt)) {
    if (agent) {
        agent->resume();
    }
}

void MainFrame::OnMenuAgentAbort(wxCommandEvent& WXUNUSED(evt)) {
    const int confirm = wxMessageBox("Abort active goal?", "Confirm Abort", wxYES_NO | wxICON_WARNING, this);
    if (confirm == wxYES && agent) {
        agent->abort();
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
    benchMenu->AppendSeparator();
    benchMenu->Append(ID_MENU_BENCH_EXPORT_COGNITIVE_METRICS, "Export &Cognitive Metrics...");
    benchMenu->AppendSeparator();
    {
        wxMenuItem* statusItem = benchMenu->Append(
            ID_MENU_BENCH_STATUS,
            wxString::FromUTF8(Thoth::kBenchmarksAvailableInLocal));
        statusItem->Enable(false);
    }

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
    Bind(wxEVT_MENU, &MainFrame::OnMenuBenchExportCognitiveMetrics, this, ID_MENU_BENCH_EXPORT_COGNITIVE_METRICS);

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
    if (agent && !agent->capabilities().supportsBenchmarks) {
        return;
    }
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

void MainFrame::OnMenuBenchExportCognitiveMetrics(wxCommandEvent& WXUNUSED(evt)) {
    FileHandler fh;
    const std::filesystem::path projectRoot = fh.getProjectRoot();
    const std::filesystem::path sourcePath = projectRoot / "logs" / "cognitive_metrics.jsonl";

    if (!std::filesystem::exists(sourcePath)) {
        wxMessageBox("No cognitive metrics log found at logs/cognitive_metrics.jsonl.\nRun a goal first.",
                     "Export Cognitive Metrics", wxOK | wxICON_INFORMATION, this);
        return;
    }

    wxFileDialog dialog(this,
                        "Export Cognitive Metrics",
                        wxString::FromUTF8((projectRoot / "logs").string()),
                        "cognitive_metrics.jsonl",
                        "JSON Lines (*.jsonl)|*.jsonl|CSV (*.csv)|*.csv",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString destPath = dialog.GetPath();
    const bool exportCsv = destPath.Lower().EndsWith(".csv");

    std::ifstream in(sourcePath);
    if (!in.is_open()) {
        wxMessageBox("Could not read logs/cognitive_metrics.jsonl.", "Export Cognitive Metrics",
                     wxOK | wxICON_ERROR, this);
        return;
    }

    std::vector<nlohmann::json> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            auto row = nlohmann::json::parse(line);
            if (row.value("event", "") == "GOAL_COGNITIVE_METRICS") {
                rows.push_back(std::move(row));
            }
        } catch (...) {
        }
    }

    if (rows.empty()) {
        wxMessageBox("The cognitive metrics log contains no GOAL_COGNITIVE_METRICS rows.",
                     "Export Cognitive Metrics", wxOK | wxICON_INFORMATION, this);
        return;
    }

    std::ofstream out(destPath.ToStdString());
    if (!out.is_open()) {
        wxMessageBox("Could not write export file.", "Export Cognitive Metrics", wxOK | wxICON_ERROR, this);
        return;
    }

    static const char* kColumns[] = {
        "emitted_at_ms", "plan_id", "session_id", "goal", "outcome", "total_wall_clock_ms",
        "planning_time_ms", "retrieval_time_ms", "llm_synthesis_time_ms", "step_count",
        "retrieved_chunk_count", "grag_alpha", "grag_routing_mode", "trajectory_score",
        "final_success_score", "reflection_count", "revisions_count", "max_reflections",
        "reflection_skip_reason", "plan_reused", "total_tokens", "prompt_tokens",
        "completion_tokens", "planning_tokens", "synthesis_tokens", "synthesis_prompt_chars",
        "synthesis_context_truncated"
    };

    auto csvEscape = [](const std::string& value) {
        const bool needsQuotes = value.find_first_of(",\"\n\r") != std::string::npos;
        std::string escaped;
        escaped.reserve(value.size() + 2);
        if (needsQuotes) {
            escaped.push_back('"');
        }
        for (char ch : value) {
            if (ch == '"') {
                escaped += "\"\"";
            } else {
                escaped.push_back(ch);
            }
        }
        if (needsQuotes) {
            escaped.push_back('"');
        }
        return escaped;
    };

    auto fieldValue = [](const nlohmann::json& row, const char* key) -> std::string {
        if (!row.contains(key)) {
            return "";
        }
        const auto& value = row.at(key);
        if (value.is_string()) {
            return value.get<std::string>();
        }
        if (value.is_boolean()) {
            return value.get<bool>() ? "true" : "false";
        }
        if (value.is_number()) {
            return value.dump();
        }
        return value.dump();
    };

    if (exportCsv) {
        for (size_t i = 0; i < sizeof(kColumns) / sizeof(kColumns[0]); ++i) {
            if (i > 0) {
                out << ',';
            }
            out << kColumns[i];
        }
        out << '\n';
        for (const auto& row : rows) {
            for (size_t i = 0; i < sizeof(kColumns) / sizeof(kColumns[0]); ++i) {
                if (i > 0) {
                    out << ',';
                }
                out << csvEscape(fieldValue(row, kColumns[i]));
            }
            out << '\n';
        }
    } else {
        for (const auto& row : rows) {
            out << row.dump() << '\n';
        }
    }

    SetTransientStatus(wxString::Format("Exported %zu cognitive metric rows.", rows.size()));
}

void MainFrame::OnMenuBenchRetrievalComparison(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("Benchmarks → Retrieval Comparison", "Comparison tool is forthcoming.");
}

void MainFrame::OnMenuBenchStrategyLearning(wxCommandEvent& WXUNUSED(evt)) {
    ShowMenuStatus("Benchmarks → Strategy Learning", "Learning analysis is forthcoming.");
}

void MainFrame::OnMenuBenchFullSystem(wxCommandEvent& WXUNUSED(evt)) {
    if (agent && !agent->capabilities().supportsBenchmarks) {
        return;
    }
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

    const std::string& rawGoal = m_sessions[static_cast<size_t>(m_activeSessionIndex)].activeGoal;
    const std::string displayGoal = TrimGoalForDisplay(rawGoal);
    if (displayGoal.empty()) {
        if (m_goalBanner) m_goalBanner->Hide();
    } else {
        if (m_goalText) {
            wxString label = wxString::FromUTF8("Current Goal: " + displayGoal);
            m_goalText->SetLabel(label);
            const int wrapWidth = std::max(m_goalBanner->GetClientSize().GetWidth() - 140, 200);
            m_goalText->Wrap(wrapWidth);
        }
        if (m_goalBanner) m_goalBanner->Show();
    }
    if (m_planPanel) {
        m_planPanel->SetSessionGoalDisplay(displayGoal);
    }
    m_auiManager.Update();
}

void MainFrame::ClearActiveGoal() {
    if (m_sessionId.empty()) {
        return;
    }
    ClearSessionGoal(m_sessionId);
}

void MainFrame::ClearSessionGoal(const std::string& sessionId) {
    if (sessionId.empty()) {
        return;
    }
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
                           [&sessionId](const Thoth::ChatSession& session) {
                               return session.id == sessionId;
                           });
    if (it == m_sessions.end()) {
        return;
    }
    if (it->activeGoal.empty()) {
        return;
    }
    it->activeGoal.clear();
    SaveChatSessions();
    if (m_sessionId == sessionId) {
        RefreshGoalBanner();
    }
}

void MainFrame::SyncBackendSessionIdentity() {
    if (agent && !m_sessionId.empty()) {
        agent->setSessionId(m_sessionId);
    }
}

std::string MainFrame::ResolveGoalEventSessionId(const std::string& eventSessionId) const {
    if (!eventSessionId.empty()) {
        return eventSessionId;
    }
    return m_sessionId;
}

void MainFrame::RegisterPendingChatRequest(const std::string& requestId,
                                           const std::string& sessionId) {
    if (requestId.empty() || sessionId.empty()) {
        return;
    }
    ++m_inFlightChatBySession[sessionId];
    UpdateChatSendChrome();
}

void MainFrame::ClearPendingChatRequest(const std::string& /*requestId*/,
                                          const std::string& sessionId) {
    if (sessionId.empty()) {
        return;
    }
    auto it = m_inFlightChatBySession.find(sessionId);
    if (it == m_inFlightChatBySession.end()) {
        UpdateChatSendChrome();
        return;
    }
    it->second = std::max(0, it->second - 1);
    if (it->second == 0) {
        m_inFlightChatBySession.erase(it);
    }
    UpdateChatSendChrome();
}

void MainFrame::UpdateChatSendChrome() {
    if (!m_sendButton) {
        return;
    }
    const bool engineConversation =
        agent && agent->capabilities().supportsConversation;
    if (!engineConversation || m_sessionId.empty()) {
        m_sendButton->Enable(true);
        return;
    }
    const auto it = m_inFlightChatBySession.find(m_sessionId);
    const int inFlight = (it != m_inFlightChatBySession.end()) ? it->second : 0;
    m_sendButton->Enable(inFlight == 0);
}

void MainFrame::SetSessionGoal(const std::string& sessionId, const std::string& goal) {
    const std::string displayGoal = TrimGoalForDisplay(goal);
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(), [&sessionId](const Thoth::ChatSession& session){
        return session.id == sessionId;
    });
    if (it == m_sessions.end()) {
        return;
    }

    it->activeGoal = displayGoal;
    SaveChatSessions();

    if (m_sessionId == sessionId) {
        RefreshGoalBanner();
    }
}

bool MainFrame::InputStartsGoal(const wxString& input) {
    wxString trimmed = input;
    trimmed.Trim(true).Trim(false);
    const wxString lower = trimmed.Lower();
    return lower.StartsWith("/goal") || lower.StartsWith("goal:");
}

wxString MainFrame::ExtractGoalText(const wxString& input) {
    wxString trimmed = input;
    trimmed.Trim(true).Trim(false);
    const wxString lower = trimmed.Lower();
    if (lower.StartsWith("goal:")) {
        wxString rest = trimmed.Mid(5);
        rest.Trim(true).Trim(false);
        return rest;
    }
    if (lower.StartsWith("/goal")) {
        wxString rest = trimmed.Mid(5);
        rest.Trim(true).Trim(false);
        return rest;
    }
    return wxString();
}

void MainFrame::RefreshExecutiveStripActivity() {
    if (!m_stateStrip) {
        return;
    }
    if (m_ragIndexingCount > 0) {
        m_stateStrip->SetActivityMessage(
            wxString::Format("Indexing RAG files (%d in progress)", m_ragIndexingCount));
    } else if (m_goalPlanningPending) {
        m_stateStrip->SetActivityMessage("Planning…");
    } else {
        m_stateStrip->ClearActivityMessage();
    }
}

Thoth::ProgressSource MainFrame::ActiveBackendProgressSource() const {
    const bool is_remote = agent && agent->isRemote();
    return Thoth::progressSourceForBackendEvent(is_remote);
}

void MainFrame::ApplyWorkActivity(const wxString& message, Thoth::ProgressSource source) {
    if (!m_stateStrip || !Thoth::mayApplyWorkProgress(source)) {
        return;
    }
    m_stateStrip->SetActivityMessage(message);
}

void MainFrame::ApplyWorkStatus(const wxString& text, Thoth::ProgressSource source) {
    if (!Thoth::mayApplyWorkProgress(source)) {
        return;
    }
    SetTransientStatus(text);
}

void MainFrame::HandleOperationComplete(const Thoth::OperationResult& result,
                                        const std::string& requestId) {
    wxTheApp->CallAfter([this, result, requestId]() {
        if (wxPendingDelete.Member(this) || !wxWindow::FindWindowById(GetId())) {
            return;
        }

        const Thoth::EventStreamSnapshot snap =
            agent ? agent->eventStreamSnapshot() : Thoth::EventStreamSnapshot{};
        const std::string correlated =
            Thoth::formatCorrelatedUserMessage(snap, result);
        const wxString message = wxString::FromUTF8(correlated);
        const Thoth::OperationUiSeverity severity =
            Thoth::uiSeverityForFailure(result, snap);

        const bool isChat = result.operation == Thoth::kOpChat && !requestId.empty();
        std::string targetSessionId;
        if (isChat) {
            const auto requestIt = m_requestToSession.find(requestId);
            if (requestIt == m_requestToSession.end()) {
                return;
            }
            targetSessionId = requestIt->second;
            m_requestToSession.erase(requestIt);
            ClearPendingChatRequest(requestId, targetSessionId);

            auto sessionIt = std::find_if(m_sessions.begin(), m_sessions.end(),
                [&targetSessionId](const Thoth::ChatSession& session) {
                    return session.id == targetSessionId;
                });
            if (sessionIt == m_sessions.end()) {
                return;
            }

            const bool engineConversation =
                agent && agent->capabilities().supportsConversation;

            if (result.success) {
                if (engineConversation) {
                    RefreshSessionConversationFromEngine(targetSessionId);
                } else {
                    sessionIt->messages.push_back({"assistant", result.response_text, NowMs()});
                }
            } else if (severity == Thoth::OperationUiSeverity::Panel && !engineConversation) {
                sessionIt->messages.push_back({"assistant", correlated, NowMs()});
            }

            if (result.success || (severity == Thoth::OperationUiSeverity::Panel && !engineConversation)) {
                sessionIt->updatedAtMs = NowMs();
                SaveChatSessions();
            }

            if (m_typingIndicator) {
                m_typingIndicator->Hide();
            }
            if (m_graphPanel) {
                m_graphPanel->UpdateControllerState("IDLE");
            }
            m_auiManager.Update();
            RefreshChatList();

            if (m_sessionId == targetSessionId) {
                RenderSession(static_cast<std::size_t>(m_activeSessionIndex));
                if (m_inputCtrl) {
                    m_inputCtrl->SetFocus();
                }
            }
            RefreshAllPanels();
        }

        if (!result.success && isChat && agent && agent->capabilities().supportsConversation
            && Thoth::engineConversationChatFailureNeedsStatusBar(result, true)) {
            SetTransientStatus(message);
        }

        if (severity == Thoth::OperationUiSeverity::Modal) {
            wxMessageBox(message, wxString::FromUTF8("Operation failed"), wxOK | wxICON_ERROR,
                         this);
            if (!result.success && result.operation == Thoth::kOpGoal) {
                m_goalPlanningPending = false;
                RefreshExecutiveStripActivity();
                ClearSessionGoal(m_sessionId);
            }
            return;
        }

        if (!result.success && result.operation == Thoth::kOpGoal) {
            m_goalPlanningPending = false;
            RefreshExecutiveStripActivity();
            ClearSessionGoal(m_sessionId);
        }

        if (result.success) {
            SetTransientStatus(message);
            if (result.operation == Thoth::CorpusCreate::kOperationName) {
                if (result.ingest_host_path && result.ingest_document_id
                    && m_activeSessionIndex >= 0
                    && m_activeSessionIndex < static_cast<int>(m_sessions.size())) {
                    std::string content_hash;
                    if (m_activeSessionIndex >= 0) {
                        const auto& session =
                            m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
                        const auto it =
                            session.localNoteEngine.find(*result.ingest_host_path);
                        if (it != session.localNoteEngine.end()) {
                            content_hash = it->second.content_hash;
                        }
                    }
                    RecordLocalNoteIngestAccept(
                        *result.ingest_host_path,
                        *result.ingest_document_id,
                        result.ingest_document_name.value_or(""),
                        result.ingest_revision_id.value_or(""),
                        content_hash);
                }
                RefreshCorpusPanel();
            }
            return;
        }

        if (result.operation == Thoth::CorpusCreate::kOperationName
            && result.ingest_content_conflict && result.ingest_host_path
            && UseAlpGuiPicker()) {
            if (ConfirmForceReplace(*result.ingest_host_path,
                                    result.ingest_reason.value_or(
                                        result.technical_details))) {
                SendLocalNoteToEngine(*result.ingest_host_path, true);
            } else {
                SetTransientStatus(wxString::FromUTF8("Send cancelled — conflict not confirmed"));
            }
            return;
        }

        if (severity == Thoth::OperationUiSeverity::StatusBar
            || (severity == Thoth::OperationUiSeverity::Panel && !isChat)) {
            SetTransientStatus(message);
        }
    });
}

void MainFrame::RecordLocalNoteIngestAccept(const std::string& host_path,
                                            const std::string& document_id,
                                            const std::string& document_name,
                                            const std::string& revision_id,
                                            const std::string& content_hash) {
    if (m_activeSessionIndex < 0
        || m_activeSessionIndex >= static_cast<int>(m_sessions.size())
        || host_path.empty() || document_id.empty()) {
        return;
    }
    auto& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
    auto& info = session.localNoteEngine[host_path];
    info.document_id = document_id;
    info.document_name = document_name;
    if (!revision_id.empty()) {
        info.revision_id = revision_id;
    }
    if (!content_hash.empty()) {
        info.content_hash = content_hash;
    } else {
        std::string read_err;
        if (const auto payload = Thoth::CorpusCreateLocal::readLocalNoteFile(host_path, read_err)) {
            info.content_hash = payload->content_hash;
        }
    }
    info.indexing = true;
    info.chunk_count = -1;
    info.failed = false;
    info.reconcile_verified = UseAlpGuiPicker();
    session.updatedAtMs = NowMs();
    SaveChatSessions();
    RefreshRagPanel();
}

void MainFrame::SyncLocalNotesFromCorpus(const nlohmann::json& corpus_body) {
    if (m_activeSessionIndex < 0
        || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        return;
    }
    auto& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
    const nlohmann::json legacy_map = LoadLegacyIdMap();
    if (!Thoth::LocalNoteEngineSync::syncSessionFromCorpusList(
            session, corpus_body, legacy_map)) {
        return;
    }
    session.updatedAtMs = NowMs();
    SaveChatSessions();
    RefreshRagPanel();
}

bool MainFrame::HasPendingLocalNoteIndexing() const {
    if (m_activeSessionIndex < 0
        || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        return false;
    }
    for (const auto& entry :
         m_sessions[static_cast<std::size_t>(m_activeSessionIndex)].localNoteEngine) {
        if (entry.second.indexing) {
            return true;
        }
    }
    return false;
}

void MainFrame::ApplyLocalNoteIndexingStarted(
    const Thoth::LocalNoteEngineSync::IndexingEventMetadata& event) {
    if (m_activeSessionIndex < 0
        || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        return;
    }
    auto& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
    const std::string host_path = Thoth::LocalNoteEngineSync::findHostPathForIndexingEvent(
        session, event, Thoth::AlpFeatureFlags::alpGuiEnabled());
    if (host_path.empty()) {
        return;
    }
    auto& info = session.localNoteEngine[host_path];
    info.indexing = true;
    info.failed = false;
    SaveChatSessions();
    RefreshRagPanel();
}

void MainFrame::ApplyLocalNoteIndexingCompleted(
    const Thoth::LocalNoteEngineSync::IndexingEventMetadata& event,
    bool success,
    int chunk_count) {
    if (m_activeSessionIndex < 0
        || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        return;
    }
    auto& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
    const std::string host_path = Thoth::LocalNoteEngineSync::findHostPathForIndexingEvent(
        session, event, Thoth::AlpFeatureFlags::alpGuiEnabled());
    if (host_path.empty()) {
        return;
    }
    auto& info = session.localNoteEngine[host_path];
    info.indexing = false;
    info.failed = !success;
    if (success && chunk_count >= 0) {
        info.chunk_count = chunk_count;
    }
    SaveChatSessions();
    RefreshRagPanel();
}

void MainFrame::MigrateFilesToSandbox(std::vector<std::string>& paths) {
    FileHandler fileHandler;
    const std::filesystem::path destDir = fileHandler.getRagDirectory();
    bool changed = false;
    std::map<std::string, std::string> remaps;
    for (auto& path : paths) {
        if (path.find("agent_workspace/rag/") == std::string::npos) {
            try {
                const std::string old_path = path;
                std::filesystem::path src(path);
                if (std::filesystem::exists(src)) {
                    if (!std::filesystem::exists(destDir)) {
                        std::filesystem::create_directories(destDir);
                    }
                    std::filesystem::path dest = destDir / src.filename();
                    if (!std::filesystem::exists(dest)) {
                        std::filesystem::copy_file(src, dest);
                    }
                    path = dest.string();
                    if (path != old_path) {
                        remaps[old_path] = path;
                    }
                    changed = true;
                }
            } catch (...) {}
        }
    }
    if (!remaps.empty() && m_activeSessionIndex >= 0
        && m_activeSessionIndex < static_cast<int>(m_sessions.size())) {
        auto& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
        Thoth::LocalNoteEngineSync::remapEngineCacheKeys(session.localNoteEngine, remaps);
        changed = true;
    }
    if (changed) {
        SaveChatSessions();
    }
}

bool MainFrame::UseAlpGuiPicker() const {
    return Thoth::AlpFeatureFlags::alpGuiEnabled();
}

nlohmann::json MainFrame::LoadLegacyIdMap() const {
    FileHandler fileHandler;
    const std::string map_path = fileHandler.getAgentWorkspacePath("legacy_id_map.json");
    std::ifstream in(map_path);
    if (!in) {
        return nlohmann::json::object();
    }
    try {
        nlohmann::json map;
        in >> map;
        return map.is_object() ? map : nlohmann::json::object();
    } catch (...) {
        return nlohmann::json::object();
    }
}

bool MainFrame::ConfirmForceReplace(const std::string& host_path,
                                    const std::string& reason) const {
    wxFileName fn(wxString::FromUTF8(host_path));
    wxString message = wxString::FromUTF8(
        "Local file may be older than the committed Engine revision for "
        + fn.GetFullName().ToStdString()
        + ".\n\nForce replace will create a new revision if you confirm.");
    if (!reason.empty()) {
        message += wxString::FromUTF8("\n\nEngine: " + reason);
    }
    return wxMessageBox(message,
                        wxString::FromUTF8("Confirm force replace"),
                        wxYES_NO | wxICON_WARNING,
                        const_cast<MainFrame*>(this)) == wxYES;
}

void MainFrame::ReconcileLocalNotesEngine(const nlohmann::json& corpus_body) {
    m_localNoteIntents.clear();
    if (!UseAlpGuiPicker() || !agent || !agent->capabilities().supportsIngest) {
        m_localNoteReconcileState =
            Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Ready;
        ApplyIngestControls(agent ? agent->eventStreamSnapshot()
                                  : Thoth::localEventStreamSnapshot(NowMs()));
        return;
    }

    const auto snap = agent->eventStreamSnapshot();
    const bool engine_usable = !snap.applies || Thoth::engineHttpUsable(snap.engine);
    if (!engine_usable) {
        m_localNoteReconcileState =
            Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Unverified;
        ApplyIngestControls(snap);
        return;
    }

    if (m_activeSessionIndex < 0
        || m_activeSessionIndex >= static_cast<int>(m_sessions.size())) {
        m_localNoteReconcileState =
            Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Ready;
        ApplyIngestControls(snap);
        return;
    }

    auto& session = m_sessions[static_cast<std::size_t>(m_activeSessionIndex)];
    const nlohmann::json legacy_map = LoadLegacyIdMap();
    if (!legacy_map.empty()) {
        if (Thoth::LocalNoteEngineSync::upgradeLegacyDocumentIds(session, legacy_map) > 0) {
            session.updatedAtMs = NowMs();
            SaveChatSessions();
        }
    }

    const std::int64_t reconcile_started_ms = NowMs();
    for (const auto& host_path : session.ragFilePaths) {
        if (NowMs() - reconcile_started_ms
            > Thoth::LocalNoteEngineSync::kReconcileTotalBudgetMs) {
            Thoth::LocalNoteEngineSync::LocalNoteIntent intent;
            intent.host_path = host_path;
            intent.canonical_name = std::filesystem::path(host_path).filename().string();
            intent.query_ok = false;
            intent.reason = "reconcile total timeout";
            m_localNoteIntents.push_back(std::move(intent));
            continue;
        }

        Thoth::LocalNoteEngineSync::LocalNoteIntent intent;
        intent.host_path = host_path;
        intent.canonical_name = std::filesystem::path(host_path).filename().string();

        std::string read_err;
        const auto payload =
            Thoth::CorpusCreateLocal::readLocalNoteFile(host_path, read_err);
        if (!payload) {
            intent.query_ok = false;
            intent.reason = read_err;
            m_localNoteIntents.push_back(std::move(intent));
            continue;
        }

        const auto query = agent->queryCorpusDocumentIntent(host_path);
        if (!query.success || !query.ingest_action) {
            intent.query_ok = false;
            intent.reason = query.technical_details.empty() ? query.user_message
                                                            : query.technical_details;
            m_localNoteIntents.push_back(std::move(intent));
            continue;
        }

        intent.query_ok = true;
        intent.action = *query.ingest_action;
        if (query.ingest_reason) {
            intent.reason = *query.ingest_reason;
        }
        if (query.ingest_document_id) {
            intent.document_id = *query.ingest_document_id;
        }
        if (query.ingest_document_name) {
            intent.document_name = *query.ingest_document_name;
        }
        m_localNoteIntents.push_back(intent);

        auto& info = session.localNoteEngine[host_path];
        Thoth::LocalNoteEngineSync::applyIntentToCache(
            info, m_localNoteIntents.back(), payload->content_hash);
    }

    if (!session.localNoteEngine.empty()) {
        Thoth::LocalNoteEngineSync::syncSessionFromCorpusList(
            session, corpus_body, legacy_map);
    }

    m_localNoteReconcileState =
        Thoth::LocalNoteEngineSync::LocalNoteReconcileState::Ready;
    session.updatedAtMs = NowMs();
    SaveChatSessions();
    ApplyIngestControls(snap);
    RefreshRagPanel();
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
