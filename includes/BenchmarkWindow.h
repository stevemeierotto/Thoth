/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Benchmark Execution & Visualization Window
 */

#pragma once

#include <wx/wx.h>
#include <wx/process.h>
#include <wx/timer.h>
#include <wx/textctrl.h>
#include <wx/gauge.h>

class BenchmarkWindow : public wxFrame {
public:
    BenchmarkWindow(wxWindow* parent, const wxString& title, 
                    const wxString& type, const wxString& mode);
    virtual ~BenchmarkWindow();

    // Starts the benchmark process with the given command and project root.
    bool Run(const wxString& command, const wxString& projectRoot);

private:
    void OnTimer(wxTimerEvent& evt);
    void OnKillTimer(wxTimerEvent& evt);
    void OnTerminate(wxCommandEvent& evt);
    void OnOpenLog(wxCommandEvent& evt);
    void OnCloseClick(wxCommandEvent& evt);
    void OnProcessEnd(wxProcessEvent& evt);
    void OnCloseWindow(wxCloseEvent& evt);

    void AppendToTerminal(const wxString& text, const wxColour& colour = *wxBLACK);
    void ProcessPipes(bool finalDrain = false);
    void UpdateMetadataHeader();
    void FinalizeExecution();
    void KillProcess(bool force = false);

    wxString m_command;
    wxString m_type;
    wxString m_mode;
    long m_pid = 0;
    wxProcess* m_process = nullptr;
    wxTimer m_timer;
    wxTimer m_killTimer; // For SIGTERM -> SIGKILL escalation
    
    wxTextCtrl* m_headerText = nullptr;
    wxTextCtrl* m_terminal = nullptr;
    wxButton* m_btnTerminate = nullptr;
    wxButton* m_btnClose = nullptr;
    wxButton* m_btnOpenLog = nullptr;

    wxString m_lineBuffer; // For complete line parsing

    enum {
        ID_TIMER = 1000,
        ID_KILL_TIMER,
        ID_TERMINATE,
        ID_OPEN_LOG
    };

    wxDECLARE_EVENT_TABLE();
};
