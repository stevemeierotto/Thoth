// src/main.cpp
//
// Entry point for Thoth Control Panel.
// Keeps this file clean — only app bootstrap code.

#include <wx/wx.h>
#include "MainFrame.h"

class ThothApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit())
            return false;
        MainFrame* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(ThothApp);

