// includes/VisualizationFrame.h
//
// Declares a placeholder frame for visualization.
// Later we will embed or connect SDL2 rendering here.

#pragma once
#include <wx/wx.h>

class VisualizationFrame : public wxFrame {
public:
    explicit VisualizationFrame(wxWindow* parent);
};

