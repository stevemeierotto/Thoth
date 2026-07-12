// src/main.cpp
//
// Entry point for Thoth Control Panel.
// Keeps this file clean — only app bootstrap code.

#include <wx/wx.h>
#include <curl/curl.h>
#include "MainFrame.h"
#include "runtime_bootstrap.h"

class ThothApp : public wxApp {
public:
    bool OnInit() override {
        Thoth::bootstrapRuntimeEnvironment();

        // Initialize libcurl globally
        curl_global_init(CURL_GLOBAL_ALL);

        if (!wxApp::OnInit())
            return false;
        MainFrame* frame = new MainFrame();
        frame->Show(true);
        return true;
    }

    int OnExit() override {
        // Cleanup libcurl globally
        curl_global_cleanup();
        return wxApp::OnExit();
    }
};

wxIMPLEMENT_APP(ThothApp);

