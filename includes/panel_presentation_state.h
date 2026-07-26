/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — GUI Phase 2 diagnostic panel presentation states
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_PANEL_PRESENTATION_STATE_H
#define THOTH_PANEL_PRESENTATION_STATE_H

namespace Thoth {

/**
 * Consistent presentation model for diagnostic panels.
 * Loading is reserved for real waits; Empty ≠ Unavailable (see D10).
 */
enum class PanelPresentationState {
    Loading,
    Empty,
    Populated,
    Unavailable,
    Error
};

} // namespace Thoth

#endif // THOTH_PANEL_PRESENTATION_STATE_H
