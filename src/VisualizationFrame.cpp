// src/VisualizationFrame.cpp
//
// Implements the placeholder visualization frame.

#include "VisualizationFrame.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

VisualizationFrame::VisualizationFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Visualization (placeholder)", wxDefaultPosition, wxSize(600, 400))
{
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* label = new wxStaticText(
        panel, wxID_ANY,
        "Visualization placeholder.\nWhen ready, replace this frame's contents with an SDL2 window or embed.",
        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE
    );

    sizer->Add(label, 1, wxALIGN_CENTER | wxALL, 20);
    panel->SetSizer(sizer);
}

