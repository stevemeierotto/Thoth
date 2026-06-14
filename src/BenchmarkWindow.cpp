/*
 * Copyright (c) 2025 Steve Meierotto
 * 
 * Thoth — Benchmark Execution & Visualization Window Implementation
 */

#include "BenchmarkWindow.h"
#include <wx/sizer.h>
#include <wx/datetime.h>
#include <wx/utils.h>
#include <wx/txtstrm.h>
#include <wx/regex.h>
#include <wx/filename.h>
#include <iostream>

wxBEGIN_EVENT_TABLE(BenchmarkWindow, wxFrame)
    EVT_TIMER(ID_TIMER, BenchmarkWindow::OnTimer)
    EVT_TIMER(ID_KILL_TIMER, BenchmarkWindow::OnKillTimer)
    EVT_BUTTON(ID_TERMINATE, BenchmarkWindow::OnTerminate)
    EVT_BUTTON(ID_OPEN_LOG, BenchmarkWindow::OnOpenLog)
    EVT_BUTTON(wxID_CLOSE, BenchmarkWindow::OnCloseClick)
    EVT_END_PROCESS(wxID_ANY, BenchmarkWindow::OnProcessEnd)
    EVT_CLOSE(BenchmarkWindow::OnCloseWindow)
wxEND_EVENT_TABLE()

BenchmarkWindow::BenchmarkWindow(wxWindow* parent, const wxString& title, 
                                 const wxString& type, const wxString& mode)
    : wxFrame(parent, wxID_ANY, title, wxDefaultPosition, wxSize(800, 600)),
      m_type(type), m_mode(mode), m_timer(this, ID_TIMER), m_killTimer(this, ID_KILL_TIMER)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Metadata Header
    m_headerText = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 80), 
                                  wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    m_headerText->SetBackgroundColour(GetBackgroundColour());
    mainSizer->Add(m_headerText, 0, wxEXPAND | wxALL, 10);

    // Terminal
    m_terminal = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL | wxTE_RICH);
    m_terminal->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    m_terminal->SetBackgroundColour(*wxBLACK);
    m_terminal->SetForegroundColour(*wxWHITE);
    mainSizer->Add(m_terminal, 1, wxEXPAND | wxALL, 5);

    // Controls
    wxBoxSizer* ctrlSizer = new wxBoxSizer(wxHORIZONTAL);
    
    m_btnTerminate = new wxButton(this, ID_TERMINATE, "Terminate (SIGTERM)");
    m_btnOpenLog = new wxButton(this, ID_OPEN_LOG, "Open Log");
    m_btnOpenLog->Enable(false);
    m_btnClose = new wxButton(this, wxID_CLOSE, "Close");
    m_btnClose->Enable(false);

    ctrlSizer->Add(m_btnTerminate, 0, wxALL, 5);
    ctrlSizer->AddStretchSpacer();
    ctrlSizer->Add(m_btnOpenLog, 0, wxALL, 5);
    ctrlSizer->Add(m_btnClose, 0, wxALL, 5);

    mainSizer->Add(ctrlSizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
    UpdateMetadataHeader();
}

BenchmarkWindow::~BenchmarkWindow() {
    if (m_timer.IsRunning()) m_timer.Stop();
    if (m_killTimer.IsRunning()) m_killTimer.Stop();
    if (m_process) {
        m_process->Detach();
        m_process = nullptr;
    }
}

void BenchmarkWindow::UpdateMetadataHeader() {
    wxString meta;
    meta << "[ Type: " << m_type << " | Mode: " << m_mode << " | PID: " 
         << (m_pid > 0 ? wxString::Format("%ld", m_pid) : wxString("Pending")) << " ]\n";
    meta << "Started: " << wxDateTime::Now().Format() << "\n";
    meta << "Command: " << m_command;
    m_headerText->SetValue(meta);
}

bool BenchmarkWindow::Run(const wxString& command, const wxString& projectRoot) {
    m_command = command;
    UpdateMetadataHeader();

    std::cerr << "[BenchmarkWindow] Run command: " << command.ToStdString() << "\n";
    std::cerr << "[BenchmarkWindow] CWD: " << projectRoot.ToStdString() << "\n";

    m_process = new wxProcess(this);
    m_process->Redirect();

    wxExecuteEnv env;
    env.cwd = projectRoot;

    m_pid = wxExecute(command, wxEXEC_ASYNC, m_process, &env);

    if (m_pid == 0) {
        AppendToTerminal("FAILED TO LAUNCH: " + command + "\n", *wxRED);
        delete m_process;
        m_process = nullptr;
        m_btnClose->Enable(true);
        m_btnTerminate->Enable(false);
        return false;
    }

    UpdateMetadataHeader();
    
    // Immediate poll to catch early output before timer kicks in
    ProcessPipes();
    
    m_timer.Start(100);
    return true;
}

void BenchmarkWindow::OnTimer(wxTimerEvent& WXUNUSED(evt)) {
    ProcessPipes();
}

void BenchmarkWindow::OnKillTimer(wxTimerEvent& WXUNUSED(evt)) {
    if (m_pid > 0) {
        KillProcess(true); // Forced escalation after 3s
    }
}

void BenchmarkWindow::ProcessPipes(bool finalDrain) {
    if (!m_process) return;

    bool hasData = true;
    while (hasData) {
        hasData = false;
        
        // Read Stdout
        if (m_process->IsInputAvailable()) {
            wxInputStream* in = m_process->GetInputStream();
            char buffer[4096];
            in->Read(buffer, sizeof(buffer) - 1);
            size_t count = in->LastRead();
            if (count > 0) {
                buffer[count] = '\0';
                wxString chunk = wxString::FromUTF8(buffer);
                wxTheApp->CallAfter([this, chunk]() {
                    AppendToTerminal(chunk, *wxWHITE);
                });
                hasData = true;
            }
        }

        // Read Stderr
        if (m_process->IsErrorAvailable()) {
            wxInputStream* err = m_process->GetErrorStream();
            char buffer[4096];
            err->Read(buffer, sizeof(buffer) - 1);
            size_t count = err->LastRead();
            if (count > 0) {
                buffer[count] = '\0';
                wxString chunk = wxString::FromUTF8(buffer);
                wxTheApp->CallAfter([this, chunk]() {
                    AppendToTerminal(chunk, *wxRED);
                });
                hasData = true;
            }
        }
        
        if (!finalDrain && !hasData) break;
        if (finalDrain && !hasData) break;
    }
}

void BenchmarkWindow::AppendToTerminal(const wxString& text, const wxColour& colour) {
    m_terminal->SetDefaultStyle(wxTextAttr(colour));
    m_terminal->AppendText(text);
    m_terminal->Update();
    
    // Accumulate for line parsing
    m_lineBuffer += text;
    size_t pos;
    while ((pos = m_lineBuffer.find('\n')) != wxString::npos) {
        wxString line = m_lineBuffer.substr(0, pos).Trim();
        m_lineBuffer = m_lineBuffer.substr(pos + 1);
        
        // Regex parsing for metrics
        // nDCG@5: 0.647
        // Success Rate: 80.0%
        wxRegEx reNdcg("nDCG@5:\\s*([0-9.]+)");
        wxRegEx reSuccess("Success Rate:\\s*([0-9.]+)");

        if (reNdcg.Matches(line)) {
            wxString val = reNdcg.GetMatch(line, 1);
            AppendToTerminal("\n[METRIC DETECTED: nDCG@5 = " + val + "]\n", *wxGREEN);
        }
        if (reSuccess.Matches(line)) {
            wxString val = reSuccess.GetMatch(line, 1);
            AppendToTerminal("\n[METRIC DETECTED: Success Rate = " + val + "%]\n", *wxGREEN);
        }
    }
}

void BenchmarkWindow::OnOpenLog(wxCommandEvent& WXUNUSED(evt)) {
    // Open the benchmark results file
    wxFileName results("docs", "benchmark_results.md");
    if (results.FileExists()) {
        wxLaunchDefaultApplication(results.GetFullPath());
    } else {
        // Try absolute path if relative fails
        wxString path = wxGetCwd() + "/docs/benchmark_results.md";
        if (wxFileName::Exists(path)) {
            wxLaunchDefaultApplication(path);
        } else {
             wxMessageBox("Could not find docs/benchmark_results.md", "Error", wxOK | wxICON_ERROR);
        }
    }
}

void BenchmarkWindow::OnTerminate(wxCommandEvent& WXUNUSED(evt)) {
    if (m_pid > 0) {
        if (m_btnTerminate->GetLabel() == "Force Kill (SIGKILL)") {
            KillProcess(true);
        } else {
            KillProcess(false); // First attempt: SIGTERM
            m_btnTerminate->SetLabel("Force Kill (SIGKILL)");
            m_killTimer.StartOnce(3000);
        }
    }
}

void BenchmarkWindow::KillProcess(bool force) {
    if (m_pid > 0) {
        wxProcess::Kill(m_pid, force ? wxSIGKILL : wxSIGTERM);
        if (force) AppendToTerminal("\n[PROCESS KILLED MANUALLY (SIGKILL)]\n", *wxRED);
        else AppendToTerminal("\n[TERMINATION REQUESTED (SIGTERM)]\n", *wxRED);
    }
}

void BenchmarkWindow::OnProcessEnd(wxProcessEvent& evt) {
    m_timer.Stop();
    if (m_killTimer.IsRunning()) m_killTimer.Stop();
    
    m_pid = 0;
    ProcessPipes(true); // Final drain
    
    AppendToTerminal(wxString::Format("\n[PROCESS EXITED WITH CODE %d]\n", evt.GetExitCode()), *wxCYAN);
    
    FinalizeExecution();
}

void BenchmarkWindow::FinalizeExecution() {
    m_btnClose->Enable(true);
    m_btnTerminate->Enable(false);
    m_btnOpenLog->Enable(true);
    
    if (m_process) {
        delete m_process;
        m_process = nullptr;
    }
}

void BenchmarkWindow::OnCloseClick(wxCommandEvent& WXUNUSED(evt)) {
    Close();
}

void BenchmarkWindow::OnCloseWindow(wxCloseEvent& evt) {
    if (m_pid > 0) {
        int res = wxMessageBox("Benchmark is still running. Terminate and exit?", "Confirm Exit", 
                               wxYES_NO | wxICON_QUESTION);
        if (res == wxYES) {
            KillProcess(true);
            evt.Skip();
        } else {
            evt.Veto();
        }
    } else {
        evt.Skip();
    }
}
