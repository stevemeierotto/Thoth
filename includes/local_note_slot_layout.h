/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — Local Note slot X layout (GUI)
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_LOCAL_NOTE_SLOT_LAYOUT_H
#define THOTH_LOCAL_NOTE_SLOT_LAYOUT_H

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <algorithm>
#include <array>

namespace Thoth {
namespace LocalNoteSlotLayout {

/** Floor so a hidden-then-shown X still has a positive sizer request. */
inline constexpr int kDeleteButtonMinWidth = 48;
inline constexpr int kDeleteButtonMinHeight = 36;

/** Slot number is 1-based. Path index is 0-based into ragFilePaths. */
inline int localNoteSlotPathIndex(int slotNumber) {
    return slotNumber - 1;
}

inline wxSize stableDeleteButtonMinSize(const wxSize& best) {
    wxSize size = best;
    if (size.x < kDeleteButtonMinWidth) {
        size.x = kDeleteButtonMinWidth;
    }
    if (size.y < kDeleteButtonMinHeight) {
        size.y = kDeleteButtonMinHeight;
    }
    return size;
}

inline void applyStableDeleteButtonSize(wxButton* button) {
    if (!button) {
        return;
    }
    button->SetMinSize(stableDeleteButtonMinSize(button->GetBestSize()));
    button->InvalidateBestSize();
}

/** Show a previously hidden X and force the next layout to honor its min size. */
inline void showLocalNoteDeleteButton(wxButton* button) {
    if (!button) {
        return;
    }
    if (button->GetMinSize().x < kDeleteButtonMinWidth
        || button->GetMinSize().y < kDeleteButtonMinHeight) {
        applyStableDeleteButtonSize(button);
    }
    button->Show();
    button->InvalidateBestSize();
}

/** Hide without clearing the min size that the next Show() must reuse. */
inline void hideLocalNoteDeleteButton(wxButton* button) {
    if (!button) {
        return;
    }
    if (button->GetMinSize().x < kDeleteButtonMinWidth
        || button->GetMinSize().y < kDeleteButtonMinHeight) {
        applyStableDeleteButtonSize(button);
    }
    button->Hide();
}

/**
 * Heading + two button-height rows + the inter-row gap + Send + the borders
 * the notes sizer actually applies. 180px was too short once both rows carry
 * a real X, which clipped Send to Engine.
 */
inline int localNotesPaneMinHeightPx(int headingHeight, int buttonHeight, int sendHeight) {
    if (headingHeight < 16) {
        headingHeight = 16;
    }
    if (buttonHeight < kDeleteButtonMinHeight) {
        buttonHeight = kDeleteButtonMinHeight;
    }
    if (sendHeight < 34) {
        sendHeight = 34;
    }
    const int row = buttonHeight + 10;
    const int grid = row + 5 + row;
    return 5 + headingHeight + 5 + grid + 5 + sendHeight + 5 + 8;
}

struct LocalNoteSlotWidgets {
    wxStaticText* label = nullptr;
    wxButton* remove = nullptr;
    int slotNumber = 0;
};

struct LocalNoteDeleteGrid {
    wxFlexGridSizer* grid = nullptr;
    std::array<LocalNoteSlotWidgets, 4> slots{};
};

/** Four slot rows/columns. Every X starts hidden with a stable min size. */
inline LocalNoteDeleteGrid makeLocalNoteDeleteGrid(wxWindow* parent) {
    LocalNoteDeleteGrid built;
    built.grid = new wxFlexGridSizer(2, 2, 5, 5);
    built.grid->AddGrowableCol(0, 1);
    built.grid->AddGrowableCol(1, 1);
    for (int i = 0; i < 4; ++i) {
        auto* sizer = new wxBoxSizer(wxHORIZONTAL);
        auto* label = new wxStaticText(parent, wxID_ANY,
                                        wxString::Format("Empty Slot %d", i + 1),
                                        wxDefaultPosition, wxDefaultSize,
                                        wxST_ELLIPSIZE_END);
        auto* button = new wxButton(parent, wxID_ANY, "X");
        button->SetToolTip("Remove file");
        label->SetMinSize(wxSize(80, 22));
        applyStableDeleteButtonSize(button);
        hideLocalNoteDeleteButton(button);
        sizer->Add(label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);
        sizer->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        built.grid->Add(sizer, 1, wxEXPAND);
        built.slots[static_cast<std::size_t>(i)] =
            LocalNoteSlotWidgets{label, button, i + 1};
    }
    return built;
}

} // namespace LocalNoteSlotLayout
} // namespace Thoth

#endif
