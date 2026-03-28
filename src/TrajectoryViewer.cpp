/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Trajectory Viewer Implementation
 */

#include "TrajectoryViewer.h"
#include <wx/sizer.h>
#include <iomanip>
#include <sstream>
#include <map>
#include <vector>

TrajectoryViewer::TrajectoryViewer(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    InitializeUI();
}

void TrajectoryViewer::InitializeUI() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Using wxTreeListCtrl for robust hierarchical data with columns (Labels)
    m_treeList = new wxTreeListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTL_SINGLE);
    m_treeList->SetMinSize(wxSize(-1, 60));
    
    // Define columns (The "Labels" requested)
    m_treeList->AppendColumn("Process (Goal / Step)", 400);
    m_treeList->AppendColumn("ID / Index", 120);
    m_treeList->AppendColumn("Score / Status", 100);
    m_treeList->AppendColumn("Date / Time", 180);
    
    mainSizer->Add(m_treeList, 1, wxEXPAND | wxALL, 0);

    SetSizer(mainSizer);
}

void TrajectoryViewer::UpdateTrajectories(const nlohmann::json& trajectoriesJson, const nlohmann::json& episodeStepsJson) {
    if (!wxIsMainThread()) {
        wxTheApp->CallAfter([this, trajectoriesJson, episodeStepsJson]() {
            UpdateTrajectories(trajectoriesJson, episodeStepsJson);
        });
        return;
    }

    m_treeList->DeleteAllItems();
    wxTreeListItem root = m_treeList->GetRootItem();

    // 1. Map trajectories for easy lookup
    std::map<std::string, const nlohmann::json*> trajMap;
    if (trajectoriesJson.is_array()) {
        for (const auto& t : trajectoriesJson) {
            trajMap[t.value("trajectory_id", "")] = &t;
        }
    }

    // 2. Group episode steps by episode_id
    std::map<std::string, std::vector<const nlohmann::json*>> stepsByEpisode;
    if (episodeStepsJson.is_array()) {
        for (const auto& s : episodeStepsJson) {
            stepsByEpisode[s.value("episode_id", "unknown")].push_back(&s);
        }
    }

    // 3. Populate Tree
    // First, consolidated trajectories
    for (auto const& [tid, trajPtr] : trajMap) {
        const auto& traj = *trajPtr;
        std::string goal = traj.value("goal", "n/a");
        float score = traj.value("success_score", 0.0f);
        long long ts = traj.value("created_at", 0LL);
        wxDateTime dt(static_cast<time_t>(ts / 1000));

        std::stringstream ssScore;
        ssScore << std::fixed << std::setprecision(2) << score;

        wxTreeListItem parent = m_treeList->AppendItem(root, wxString::FromUTF8(goal));
        m_treeList->SetItemText(parent, 1, wxString::FromUTF8(tid));
        m_treeList->SetItemText(parent, 2, wxString::FromUTF8(ssScore.str()));
        m_treeList->SetItemText(parent, 3, dt.Format("%Y-%m-%d %H:%M:%S"));

        // 3a. Initial Plan Node (The "Plan" in Plan vs Reality)
        if (traj.contains("trajectory") && traj["trajectory"].contains("plan_initial")) {
            auto const& plan = traj["trajectory"]["plan_initial"];
            if (plan.contains("steps") && plan["steps"].is_array()) {
                wxTreeListItem planRoot = m_treeList->AppendItem(parent, "Initial Plan");
                m_treeList->SetItemText(planRoot, 2, "Planned");
                for (auto const& ps : plan["steps"]) {
                    wxTreeListItem pStep = m_treeList->AppendItem(planRoot, wxString::FromUTF8(ps.value("description", "")));
                    m_treeList->SetItemText(pStep, 1, wxString::FromUTF8(ps.value("step_id", "")));
                }
            }
        }

        // 3b. Reality Node (Actual Steps Taken - The "Reality")
        if (stepsByEpisode.count(tid)) {
            auto steps = stepsByEpisode[tid];
            std::sort(steps.begin(), steps.end(), [](const auto* a, const auto* b) {
                return a->value("step_index", 0) < b->value("step_index", 0);
            });

            wxTreeListItem realityRoot = m_treeList->AppendItem(parent, "Reality (Steps Taken)");
            m_treeList->SetItemText(realityRoot, 2, "Executed");

            for (const auto* stepPtr : steps) {
                const auto& s = *stepPtr;
                wxTreeListItem child = m_treeList->AppendItem(realityRoot, wxString::FromUTF8(s.value("state_summary", "")));
                m_treeList->SetItemText(child, 1, wxString::Format("%d", s.value("step_index", 0)));
                m_treeList->SetItemText(child, 2, wxString::FromUTF8(s.value("result_status", "")));
            }
        }
    }

    // 4. In-progress/Orphan episodes
    for (auto const& [eid, steps] : stepsByEpisode) {
        if (trajMap.find(eid) == trajMap.end()) {
            const auto* firstStep = steps[0];
            long long ts = firstStep->value("timestamp_ms", 0LL);
            wxDateTime dt(static_cast<time_t>(ts / 1000));

            wxTreeListItem parent = m_treeList->AppendItem(root, 
                wxString::FromUTF8(firstStep->value("state_summary", "Execution in progress...")));
            
            m_treeList->SetItemText(parent, 1, wxString::FromUTF8(eid));
            m_treeList->SetItemText(parent, 2, "In-Prog");
            m_treeList->SetItemText(parent, 3, dt.Format("%Y-%m-%d %H:%M:%S"));

            for (const auto* stepPtr : steps) {
                const auto& s = *stepPtr;
                wxTreeListItem child = m_treeList->AppendItem(parent, wxString::FromUTF8(s.value("state_summary", "")));
                m_treeList->SetItemText(child, 1, wxString::Format("%d", s.value("step_index", 0)));
                m_treeList->SetItemText(child, 2, wxString::FromUTF8(s.value("result_status", "")));
            }
        }
    }
    
    Layout();
}
