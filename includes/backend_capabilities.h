/*
 * Copyright (c) 2025 Steve Meierotto
 *
 * Thoth — GUI Phase 2 backend capability model (pure; no wx)
 *
 * The GUI renders from capabilities, not scattered isRemote() checks.
 * isRemote() may still derive this matrix at construction.
 *
 * Licensed under the MIT License (see LICENSE in project root)
 */
#ifndef THOTH_BACKEND_CAPABILITIES_H
#define THOTH_BACKEND_CAPABILITIES_H

#include <string>

namespace Thoth {

struct BackendCapabilities {
    bool supportsStrategies = false;
    bool supportsTrajectories = false;
    /** Phase 11 — Engine-owned episode research resource collection. */
    bool supportsEpisodes = false;
    bool supportsExperiments = false;
    bool supportsGraphStats = false;
    bool supportsBenchmarks = false;
    bool supportsLogs = false;
    bool supportsIngest = false;
    /** Phase 3 — Explain Plan / decision_trace summaries from active backend. */
    bool supportsPlanDiagnostics = false;
    /** Phase 8 — Engine corpus document list (not filesystem paths). */
    bool supportsCorpusList = false;
    /** Phase 10 — Engine-owned conversation authority. */
    bool supportsConversation = false;
};

/** Phase 2/3 locked matrix — LocalAgentBackend. */
inline BackendCapabilities localBackendCapabilities() {
    BackendCapabilities c;
    c.supportsStrategies = true;
    c.supportsTrajectories = true;
    c.supportsEpisodes = true;
    c.supportsExperiments = true;
    c.supportsGraphStats = true;
    c.supportsBenchmarks = true;
    c.supportsLogs = true;
    c.supportsIngest = true;
    c.supportsPlanDiagnostics = true;
    c.supportsCorpusList = true;
    return c;
}

/** Phase 2/3/4 locked matrix — Engine HTTP client (RemoteAgentBackend). */
inline BackendCapabilities engineBackendCapabilities() {
    BackendCapabilities c;
    // Phase 4: decision summary resource is live on Engine.
    c.supportsPlanDiagnostics = true;
    // Phase 8: corpus document list resource is live on Engine.
    c.supportsCorpusList = true;
    return c;
}

/** Derive v1 matrix from transport ownership flag. */
inline BackendCapabilities capabilitiesForRemoteFlag(bool is_remote) {
    return is_remote ? engineBackendCapabilities() : localBackendCapabilities();
}

/** User-facing mode banner — never “Remote”. */
inline constexpr const char* kBackendModeEngine = "Backend: Engine";
inline constexpr const char* kBackendModeLocal = "Backend: Local";

inline const char* backendModeLabel(bool is_remote) {
    return is_remote ? kBackendModeEngine : kBackendModeLocal;
}

/**
 * Remote banner includes the Engine base URL selected at startup.
 * The URL is the runtime identity (port 8090 / 8091 / 8092). Names are not
 * hard-coded here; they are Compose labels on the deployment that owns that URL.
 */
inline std::string backendModeLabelWithEndpoint(bool is_remote, const std::string& endpoint) {
    std::string label = backendModeLabel(is_remote);
    if (is_remote && !endpoint.empty()) {
        label.push_back(' ');
        label += endpoint;
    }
    return label;
}

inline std::string backendModeLabel(const BackendCapabilities& caps) {
    // Engine mode is the sparse matrix (no cognate/benchmark surfaces yet).
    const bool looks_local = caps.supportsStrategies && caps.supportsBenchmarks
        && caps.supportsIngest;
    return looks_local ? kBackendModeLocal : kBackendModeEngine;
}

/** Locked Unavailable copy — no roadmap codenames. */
inline constexpr const char* kUnavailableWithCurrentBackend =
    "Unavailable with the current backend.";

inline constexpr const char* kFeatureUnavailableCurrentBackend =
    "This feature is not available from the current backend.";

inline constexpr const char* kBenchmarksAvailableInLocal =
    "Status: Available in Local backend";

/**
 * D10 — disposition for panel data. Never treat empty payload as Unavailable.
 */
enum class PanelDataDisposition {
    Unavailable,
    Empty,
    Populated
};

inline PanelDataDisposition disposePanelData(bool supports_capability, bool data_empty) {
    if (!supports_capability) {
        return PanelDataDisposition::Unavailable;
    }
    if (data_empty) {
        return PanelDataDisposition::Empty;
    }
    return PanelDataDisposition::Populated;
}

} // namespace Thoth

#endif // THOTH_BACKEND_CAPABILITIES_H
