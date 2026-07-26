# Client/Server Completion Roadmap — GUI as Presentation Client

**Document type:** Master planning roadmap (phased locks)  
**Status:** Phase 0–12A 🔒 locked; Phase 1–11 ✅ · Phase 12A ✅ implemented 2026-07-21; Phase 12B placeholder; later phases remain Draft  
**Created:** 2026-07-19  
**Refined:** 2026-07-21 (Phase 12 split → 12A 🔒 / 12B placeholder)
**Phase 0 locked:** 2026-07-19  
**Phase 1 locked:** 2026-07-19  
**Phase 1 implemented:** 2026-07-19  
**Phase 2 locked:** 2026-07-19  
**Phase 2 implemented:** 2026-07-19  
**Phase 3 locked:** 2026-07-19  
**Phase 3 implemented:** 2026-07-19  
**Phase 4 locked:** 2026-07-19  
**Phase 4 implemented:** 2026-07-19  
**Phase 5 locked:** 2026-07-19  
**Phase 5 implemented:** 2026-07-19  
**Phase 6 locked:** 2026-07-20  
**Phase 6 implemented:** 2026-07-20  
**Phase 7 locked:** 2026-07-20  
**Phase 7 implemented:** 2026-07-20  
**Phase 8 locked:** 2026-07-20  
**Phase 8 implemented:** 2026-07-20  
**Phase 9 implemented:** 2026-07-20  
**Phase 10 locked:** 2026-07-20  
**Phase 10 implemented:** 2026-07-20  
**Phase 11 locked:** 2026-07-21  
**Phase 11 implemented:** 2026-07-21  
**Phase 12A locked:** 2026-07-21  
**Phase 12A implemented:** 2026-07-21  
**Related:** [plan_k_gui_api_client.md](plan_k_gui_api_client.md) ✅ · [plan_l_workspace_corpus.md](plan_l_workspace_corpus.md) ✅ (L3 deferred) · [docker_roadmap.md](docker_roadmap.md) · [GETTING_STARTED.md](GETTING_STARTED.md) · [ENGINE_EVENTS.md](ENGINE_EVENTS.md) · [AGENTS.md](../AGENTS.md)

> **Filename note:** This file remains `docs/GUI_integration.md` for stable links. Its mission is **client/server architecture completion**, not “add APIs for their own sake.”

---

## Phase 0 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-19 |
| Kind | Documentation / ownership freeze only — **no code** |
| Normative sections | Purpose · GUI may/may-not · Design Principles (D0–D9) · Ownership Matrix · Appendix A · Appendix D · Unavailable vs Empty |
| Post-lock rule | Later phase PRs must update Appendix A / D rows they touch; do not silently revise Phase 0 principles without a new lock |
| Next | Phase 1 locked — implement only after explicit “Implement” / “Proceed” |

**Phase 0 freezes the vocabulary for “Who owns this state?”** All subsequent phases reference this lock the way harness work references protocol documents.

---

## Phase 1 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-19 |
| Kind | Implementation plan lock — **Option A**; ✅ implemented 2026-07-19 |
| Product choice | **Option A** — keep RAG slots as host-only notes; never claim Engine indexing; skip remote `setRagFiles` |
| Rejected | Option B (disable add path entirely) |
| Normative call sites | `HandleFileDrop` · Import Corpus (via drop) · RAG slot delete · `RefreshRagPanel` remote labels |
| Locked status strings | See Phase 1 section below |
| Out of scope | Ingest API, corpus list API, seed changes, Phase 2+ |
| Post-lock rule | Do not expand to Phase 2 without a new lock |
| Implement artifacts | `includes/remote_rag_honesty.h` · `src/MainFrame.cpp` · `tests/unit_tests.cpp` · `docker/README.md` |

---

## Phase 2 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-19 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-19 |
| Product choices | Mode labels `Backend: Engine` / `Backend: Local`; Unavailable copy without roadmap codenames; Benchmarks menu disabled + in-menu Local availability status; `SetPresentationState` five-state API; `BackendCapabilities` v1 matrix; D10 explicit capability gating |
| Normative | Mode banner · capability-driven UI · panel presentation states · no Plan K / “Remote” as mode name in user-visible strings · D10 |
| Out of scope | Cognate/graph/log HTTP APIs; Phase 3 cognitive-diagnostics authority (beyond `supportsLogs=false`); Preferences URL UI; inventing Loading without real waits |
| Post-lock rule | Do not expand to Phase 3 or reopen product choices without a new lock |
| Implement artifacts | `backend_capabilities.h` · `panel_presentation_state.h` · `i_agent_backend.h` · Local/Remote backends · `AgentInterface.*` · Strategy / Trajectory / Experiment / Graph panels · `MainFrame.*` · `unit_tests.cpp` · `docker/README.md` |

---

## Phase 3 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-19 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-19 |
| Normative | **D11** — GUI may only present cognitive information obtained from the active backend |
| Product choices | Phase name = Backend Authority for Cognitive Diagnostics; Explain Plan Unavailable **with why**; expanded cognitive-diagnostics audit list; Phase 4 stays separate (honesty before Engine read APIs); `GetDecisionTrace()` unification is future direction only |
| Locked Explain Plan copy | Title `Explain Plan` · `Unavailable` · `The current backend does not expose plan diagnostics.` |
| Out of scope | Engine decision-summary resource (Phase 4); Logs/activity; unifying Local JSONL vs Engine HTTP was deferred to Phase 4; changing Local storage |
| Post-lock rule | Do not expand to Phase 4 or reopen D11 / product choices without a new lock |
| Implement artifacts | `cognitive_diagnostics_authority.h` · `backend_capabilities.h` (`supportsPlanDiagnostics`) · `AgentInterface.cpp` · `MainFrame.cpp` · `unit_tests.cpp` · `docker/README.md` |

---

## Phase 4 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-19 |
| Kind | Implementation plan lock — **Explain-only**; ✅ implemented 2026-07-19 |
| Normative | **D11** · **D12** · **D13** — Engine owns diagnostics computation and presentation model; GUI renders only |
| Product choices | Explain-only (no Logs/activity); `getLatestDecisionSummary()`; resource-oriented HTTP (not file/tail); structured JSON with mandatory `schema_version`; Local+Remote same interface; Engine advertises `supportsPlanDiagnostics` when live |
| Locked method | `IAgentBackend::getLatestDecisionSummary()` → structured JSON |
| Locked HTTP | `GET /v1/diagnostics/latest-decision` |
| Locked v1 fields | `schema_version`, `session_id`, `goal`, `executive_summary`, `planner_summary`, `retrieved_chunks`, `selected_strategy`, `execution_time_ms` |
| Out of scope | Logs / recent activity / session timeline / event history; multi-session path impl; cognate/graph HTTP; auth; GUI reconstruction from SSE |
| Post-lock rule | Do not expand to Logs/activity or reopen Explain-only / D12–D13 without a new lock |
| Implement artifacts | `decision_summary.h` · `EngineRuntime::getLatestDecisionSummary` · `engine_http_transport.cpp` · Local/Remote backends · `AgentInterface` · `MainFrame` · `backend_capabilities.h` · tests · `docker/README.md` |

---

## Phase 5 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-19 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-19 |
| Normative | **D3** / **D3a** — GUI reflects backend-reported work; never predicts / simulates / infers progress |
| Product choices | Progress from authoritative backend signals only; Engine → SSE + API responses; Local → Local backend events; audit all work-implying UI; internal `ProgressSource` preferred; last-event-age deferred to Phase 6; documented grep regression guard |
| Out of scope | New event types; SSE reconnect / connection health (Phase 6); activity history; last-event-age UX |
| Post-lock rule | Do not expand to Phase 6 or reopen D3a / product choices without a new lock; implement only after explicit “Implement” / “Proceed” / “Approved” |
| Implement artifacts | `progress_source.h` · MainFrame progress audit/fixes · event→UI map below · `testGuiPhase5ProgressReportingDiscipline` · grep checklist in header · docker smoke item 11 |

---

## Phase 6 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-20 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-20 |
| Normative | Connection state machine · connection health ≠ engine health · Phase 5 + 6 together: truthful + resilient event view · **stale-live invariant** (dead stream ⇒ connection state visible) |
| Product choices | States: Connected / Disconnected / Reconnecting / Failed · exponential backoff + max interval · retry indefinitely until user leaves Engine mode / intentional stop · **Failed** only when reconnect intentionally stopped (not auto on timeout) · new events only (no replay/cursors/dedup) · last-event-age diagnostics-only · long-outage UX: freeze progress, show Reconnecting, Engine axis from `/ready`, honest controls · Local: no SSE reconnect chrome |
| `/ready` during reconnect | Probe on backoff ticks; Engine axis from `/ready`; Connection axis from SSE open/read outcomes |
| Out of scope | Replay / Last-Event-ID / cursors / client dedup · general HTTP retry for chat/goals · new event types · activity history |
| Post-lock rule | Do not expand to Phase 7 or reopen product choices without a new lock |
| Implement artifacts | `engine_connection_state.h` · RemoteAgentBackend reconnect loop · AgentInterface snapshot · MainFrame 3-field status + poll timer · GRAG last-event-age · `testGuiPhase6EventStreamResilience` · docker smoke item 12 |

---

## Phase 7 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-20 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-20 |
| Title | **Operation Result Honesty** (transport-agnostic — not HTTP-only) |
| Normative | **One-outcome rule** — every user-initiated Engine op → exactly one visible success or failure; never neither, never both · **No success before backend confirmation** |
| Product choices | Structured internal **`OperationResult`** (success, operation, user_message, technical_details, retryable; optional http_status for HTTP v1) · UI severity: status bar (transient/network) · panel (operation-specific) · modal (decision/unrecoverable only) · **Phase 6 correlation** — root cause from connection/engine once; operation line adds detail only · Local + Remote same result path |
| Out of scope | Auth · automatic retries · changing transport wire (HTTP remains v1 impl; type stays agnostic) |
| Post-lock rule | Do not expand to Phase 8 or reopen product choices without a new lock; implement only after explicit “Implement” / “Proceed” / “Approved” |
| Implement artifacts (planned) | `operation_result.h` · IAgentBackend result returns · Remote/Local mapping · MainFrame render + correlation · chat async failure path · unit/policy tests · docker smoke |
| Implement artifacts | `operation_result.h` · `IAgentBackend`/`Local`/`Remote` `OperationResult` returns · `AgentInterface::onOperationComplete` · MainFrame `HandleOperationComplete` + Phase 6 correlation · `testGuiPhase7OperationResultHonesty` · docker smoke item 13 |

---

## Phase 8 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-20 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-20 |
| Title | **Engine Corpus Listing** (corpus document model — not filesystem listing) |
| Normative | **Engine owns corpus model; GUI presents corpus model; GUI never derives corpus from local filesystem state** · resource-oriented (Phase 4 pattern) · files → **corpus documents** |
| Product choices | `GET /v1/rag/corpus` (illustrative) → document list · fields: **`id`**, **`name`**, **`indexed_at`**, **`status`**, optional **`chunk_count`** (`null` OK) · forbidden: mtime, size, paths · **`supportsCorpusList`** capability · remote RAG tab: **Engine Corpus** + collapsed **▼ Local Notes** · three distinct states: Loading… / Corpus is empty. / Corpus listing unavailable. · dual equal lists forbidden |
| Out of scope | Ingest/upload (Phase 9) · delete/replace · filesystem metadata in contract · host path as Engine truth |
| Post-lock rule | Do not expand to Phase 9 or reopen product choices without a new lock; implement only after explicit “Implement” / “Proceed” / “Approved” |
| Implement artifacts (planned) | Engine corpus HTTP handler · `listCorpusDocuments()` on `IAgentBackend` · `supportsCorpusList` · MainFrame Engine Corpus + Local Notes · Loading/Empty/Unavailable presentation · unit/HTTP tests · docker smoke item 14 |
| Implement artifacts | `corpus_documents.h` · `GET /v1/rag/corpus` · `IndexManager::listCorpusDocuments` · `IAgentBackend`/`Local`/`Remote`/`AgentInterface` · `supportsCorpusList` · MainFrame Engine Corpus + Local Notes · `testGuiPhase8CorpusDocuments` · `testEngineHttpCorpusEndpoint` · docker smoke item 14 |

---

## Phase 9 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-20 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-20 |
| Title | **Corpus Document Creation** (create document — not upload file / not multipart-as-contract) |
| Normative | **GUI initiates create corpus document; Engine owns id, filename, storage, chunking, embedding, replacement, atomicity** · GUI owns initiation + presentation only · acceptance (**OperationResult**, Phase 7) ≠ indexing progress (**SSE `INDEXING_*`**, Phase 5) |
| Product choices | **Create corpus document** resource (illustrative `POST /v1/rag/documents`) · **add only** — no delete · **explicit [Send to Engine]** — no auto-ingest on remote drop · Local Note → create → Accepted → Indexing → Indexed/Failed → Available · atomic: exists or doesn't — never half-indexed · **`supportsIngest`** via `/ready` · wire format (multipart/JSON) is implementation detail |
| Document lifecycle | Local Note → [Send to Engine] → Accepted (OperationResult) → Indexing (SSE) → Indexed/Failed → Available (Phase 8 list) |
| Out of scope | Delete/replace · GUI choosing storage/filename/chunk params · locking multipart in protocol · mirroring host workspace · auto-ingest on drop |
| Post-lock rule | Do not expand to Phase 10 or reopen product choices without a new lock; implement only after explicit “Implement” / “Proceed” / “Approved” |
| Protocol note | Dedicated **create-document protocol** doc recommended at implement time (separate from this GUI phase lock) |
| Implement artifacts (planned) | Create-document HTTP handler · atomic ingest pipeline · `createCorpusDocument()` → `OperationResult` · `supportsIngest` on `/ready` · MainFrame **[Send to Engine]** · Phase 7 + Phase 5 separation · corpus list refresh · unit/integration tests · docker smoke item 15 |
| Implement artifacts | `corpus_create.h` · `POST /v1/rag/documents` · `IndexManager::createCorpusDocument` · `supportsIngest` via `/ready` · `createCorpusDocument()` on backends + `AgentInterface` · MainFrame **[Send to Engine]** · `testGuiPhase9CorpusCreate` · `testEngineHttpCreateDocumentEndpoint` · docker smoke item 15 |

---

## Phase 10 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-20 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-20 |
| Title | **Conversation / Session Authority** (Engine owns conversation — not GUI sync / not replace) |
| Normative | **The Engine is the authoritative owner of conversation state. The GUI displays and navigates conversation state but never reconstructs or replaces it.** · Phase 9 = knowledge ownership · Phase 10 = conversation ownership |
| Architectural model | GUI sends user turn → Engine **appends** user turn → Engine generates reply → Engine **appends assistant turn (internal)** → GUI displays reply · **Not** “GUI syncs history after each reply” |
| API surface (illustrative) | **Create session** · **Append user turn** · **Append assistant turn** (Engine-internal only) · **Get conversation** · **Get summary** · **Forbidden:** `replace-conversation`, bulk overwrite, GUI transcript as source of truth |
| Session ownership | **Engine:** conversation, context, memory, summarization, pruning, session id · **GUI:** scroll, selected tab, rendering, layout, UI chrome |
| Host JSON | `chat_sessions.json` → **cache** (titles, selection) — **not** authoritative conversation truth in Engine mode |
| Failure model | Append HTTP fail / Engine reject → user turn **never committed** → Phase 7 honest failure (“Failed to send”) — no phantom bubbles |
| Restart model | Restart GUI → connect → **Get conversation** → display — Engine restores, GUI does not replay disk as truth |
| Capability | **`supportsConversation`** (or equivalent) via `/ready` |
| Out of scope | Replace/overwrite APIs · dual-write Engine + host JSON as co-equal truths · cognate/trajectory sync (Phase 11) · GUI summarization/pruning policy |
| Post-lock rule | Do not expand to Phase 11 or reopen product choices without a new lock; implement only after explicit “Implement” / “Proceed” / “Approved” |
| Protocol note | Dedicated **conversation/session protocol** doc recommended at implement time (separate from this GUI phase lock) |
| Implement artifacts (planned) | Session store + create/append/get HTTP handlers · internal assistant append · `supportsConversation` on `/ready` · Remote backend session APIs · MainFrame remote send/load · demote `chat_sessions.json` to cache · Phase 7 uncommitted failure · unit/integration tests · docker smoke item 16 |
| Implement artifacts | `conversation_authority.h` · `POST /v1/conversation/sessions` · `POST /v1/conversation/turns` · `GET /v1/conversation/sessions/{id}` · `GET .../summary` · `Memory::getTimedMessages`/`getSummaryForSession` · `supportsConversation` via `/ready` · `createConversationSession`/`appendConversationTurn`/`getConversation`/`getConversationSummary` on Local/Remote/`AgentInterface` · MainFrame Engine-mode send (no optimistic user bubble) · `RefreshSessionConversationFromEngine` · cache-only `chat_sessions.json` · `testGuiPhase10ConversationAuthority` · `testEngineHttpConversationEndpoints` · docker smoke item 16 |

---

## Phase 11 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-21 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-21 |
| Title | **Research Resource Read APIs** (Strategies, Trajectories, Episodes) |
| Normative principle | **The Engine owns research knowledge. The GUI presents research knowledge through stable, versioned resources and never infers research state from missing or local data.** |
| Cache rule | **The GUI never caches research resources as an authoritative source. Refreshes always originate from the Engine.** |
| Resource model | **Research resources** — not debugging endpoints. Part of the research model; may later feed papers, exports, and evaluation tooling. |
| Storage abstraction | GUI requests research resources only (`getStrategies`, `getTrajectories`, `getEpisodes`). No SQLite, table names, or storage layout in the contract. Engine may source from SQLite, memory, or another backend — contract stays stable. |
| Collection envelope | Mandatory on all three list endpoints: **`schema_version`**, **`items`**, **`next_page`**, **`total_items`**. **`schema_version`** versions the **JSON representation presented to clients** and is **independent of internal Engine storage schemas**. **`next_page`** is an **opaque continuation token forever** — the GUI must treat it as opaque and **must not derive meaning from its contents**. Pagination is **mandatory in the API shape** even if v1 returns one page. |
| Stable identifiers | Every item carries an **immutable Engine-assigned identifier**: `strategy_id`, `trajectory_id`, `episode_id` — opaque strings; enables future Open / Compare / View / Export without API redesign. |
| Naming | **`getEpisodeSteps()` → `getEpisodes()`** (Option A). Engine may think in steps internally; GUI resource is Episodes. |
| Capability flags | Granular via `/ready`: **`supportsStrategies`**, **`supportsTrajectories`**, **`supportsEpisodes`** — no generic `supportsCognate`. Engine may enable resources gradually. When a flag is false → **Unavailable**; when true after implement → four collection states only (see D14). |
| Roadmap rule D14 | **Any Engine-backed collection resource must present exactly one of: Loading, Populated, Empty, Error.** Failed fetch → **Error**, never silent empty success. Unavailable applies only when capability is absent. |
| API surface (illustrative) | `GET /v1/research/strategies` · `GET /v1/research/trajectories` · `GET /v1/research/episodes` — all return the same collection envelope |
| Dependencies | Phase 2 (presentation states) · Phase 7 (operation/result patterns if reused) · capability discovery via `/ready`. **Not** Phase 10 (conversation and research are orthogonal). |
| Out of scope | Experiment Lab write/run · graph stats (Phase 12A) · cognate write/sync · E2 STRICT coupling · Export/Compare/Open actions (ids are prerequisite only) |
| Post-lock rule | Do not expand to Phase 12A or reopen product choices without a new lock; implement only after explicit “Implement” / “Proceed” / “Approved” |
| Implement artifacts (planned) | `research_resources.h` (collection envelope + validation) · Engine HTTP GET handlers · `/ready` capability tokens · `supportsEpisodes` on `BackendCapabilities` · Remote real fetches (errors → Error, not `[]`) · rename `getEpisodeSteps` → `getEpisodes` · panel four-state rule · no authoritative GUI research cache · unit/HTTP tests · docker smoke item 17 |
| Implement artifacts | `research_resources.h` · `GET /v1/research/strategies` · `/trajectories` · `/episodes` · `BasicAgentPlugin::listStrategies/listTrajectories/listEpisodes` · `/ready` tokens · `supportsEpisodes` · Remote `fetchResearchCollection` · `getEpisodes()` rename · MainFrame D14 refresh · `testGuiPhase11ResearchResources` · `testEngineHttpResearchEndpoints` · docker smoke item 17 |

---

## Phase 12A Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-21 |
| Kind | Implementation plan lock — ✅ implemented 2026-07-21 |
| Title | **Graph Statistics Resource API** (singleton — not a collection) |
| Normative principle | **The Engine owns graph state and graph statistics. The GUI presents graph resources through stable APIs and never derives graph state from local storage or missing data.** |
| Cache rule | **The GUI never caches graph statistics as an authoritative source. Refreshes always originate from the Engine.** |
| Resource model | **Singleton** Engine-owned graph statistics resource — no collection envelope, no pagination. GUI calls `getGraphStats()`. Storage (SQLite, in-memory, etc.) is Engine-internal. |
| Illustrative fields | `schema_version` (client JSON representation, independent of storage schemas) · `generated_at` (**Engine-assigned** — when the snapshot was produced, not when the GUI received it) · optional advisory `session_id` (informational only when stats are session-specific; contract remains valid for global metrics) · Engine-authored statistics fields — never table/column names |
| Empty (singleton) | **Empty** indicates the Engine successfully returned a valid graph statistics resource that contains **no meaningful graph data** for the active context. Distinct from **Error** (fetch failed) and **Unavailable** (capability absent). An invalid or failed fetch must not be treated as Empty. |
| Capability | **`supportsGraphStats`** via `/ready` — false → Unavailable; true → Loading / Populated / Empty / Error only |
| Presentation | D14 extended to singleton resources — four states when capability live; never silent `{}` on fetch failure |
| API surface (illustrative) | `GET /v1/graph/stats` |
| Dependencies | Phase 2 ✅ · Phase 7 ✅ (if reused) · Phase 11 ✅ (capability, honesty, no cache pattern) · `/ready`. **Not** Phase 12B. |
| Out of scope | Experiment Lab (Phase 12B) · graph mutation · collection envelope · Phase 13 benchmarks · E2 coupling · exact stat field lock unless required at implement |
| Phase 12B independence | **No architectural assumptions made in Phase 12A shall constrain the future design of Phase 12B.** |
| Post-lock rule | Do not expand to Phase 12B detail or reopen product choices without a new lock; implement only after explicit “Implement” / “Proceed” / “Approved” |
| Implement artifacts (planned) | `graph_statistics.h` (singleton contract + validation) · Engine HTTP GET · `/ready` token · Remote fetch · GraphPanel D14 refresh · no authoritative cache · unit/HTTP tests · docker smoke item 18 |
| Implement artifacts | `graph_statistics.h` · `GET /v1/graph/stats` · `getGraphStatisticsResource` on plugin/runtime · `graph_stats` on `/ready` · Remote `fetchGraphStatisticsResource` · MainFrame/GraphPanel D14 refresh · `testGuiPhase12AGraphStatistics` · `testEngineHttpGraphStatsEndpoint` · docker smoke item 18 |

---

## Purpose

This roadmap **completes the transition from a monolithic application to a client/server architecture** in which the **Engine is the authoritative source of cognitive state** and the **GUI is a presentation client**.

The work is about **finishing the separation of responsibilities** — not about accumulating endpoints. New APIs appear only where the thin client must **request** Engine work or **display** Engine truth. Honesty phases that remove fabricated GUI behavior are as important as capability phases that add transport.

It defines **what** must change, **in what order**, and **how each phase is validated** — without implementing code until a phase is explicitly locked and approved.

**Primary question for every future feature:**

> Who owns this state?

If the answer is cognitive, the owner is the Engine. If the answer is window chrome, the owner may be the GUI.

---

## GUI Responsibilities (precise)

### The GUI may

| Responsibility | Examples |
|----------------|----------|
| **Display** | Plan strip, GRAG diagnostics, cognate panels, chat bubbles, status, Engine error messages |
| **Request** | Chat, goals, control (pause/resume/abort), corpus list, ingest upload, log/trace fetch |
| **Cache transient UI state** | Window layout, splitter positions, selected tab, collapsed panes, scroll position, which session row is highlighted in the list |

### The GUI may not

| Forbidden | Why |
|-----------|-----|
| **Chunk** | Engine owns document segmentation |
| **Retrieve** | Engine owns GRAG / RAG retrieval |
| **Ground** | Engine owns grounding decisions and floors |
| **Store memory** | Engine owns episodic / conversation / cognate stores |
| **Simulate engine state** | No fake “running,” “indexed,” or “retrieved” without Engine report |
| **Invent progress** | No optimistic indexing counters or status claims |
| **Invent diagnostics** | No recomputed scores, fabricated breakdowns, or host traces presented as Engine truth |

**UI state vs cognitive state:** Layout and selection are UI state (GUI may own). Plans, embeddings, chunks, retrieval scores, memory rows, and execution status are cognitive state (Engine owns; GUI only displays or requests).

---

## Unavailable vs Empty (Phase 0 normative; Phase 2 extends Loading)

| Condition | Meaning | Required UX intent |
|-----------|---------|-------------------|
| **Empty** | The backend answered successfully with no records | Show empty/zero as a real result |
| **Loading** | Waiting on the backend | Show loading; do not flash Empty/Unavailable |
| **Unavailable** | This capability is not exposed by the current backend | State clearly Unavailable — **not** Empty |
| **Host-only** | Surface exists only on the host GUI; does not affect Engine cognition | Label host-only; never claim Engine indexing/retrieval |
| **Error** | Backend query/request failed | Show error; do not silently Empty |
| **Populated** | Backend returned one or more records | Show data |

Do not present Unavailable as Empty. Do not infer Unavailable from an empty payload (see D10 / Phase 2).

**Engine-backed collections (D14, Phase 11+):** When a collection capability is supported, the panel uses exactly **Loading · Populated · Empty · Error** — never silent empty on fetch failure. **Unavailable** applies only while the capability is absent.

---
## Current Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ Host GUI (wxWidgets)                                            │
│  MainFrame · panels · chat_sessions.json · host agent_workspace │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                     AgentInterface (selection once at startup)
                    ┌───────────┴───────────┐
                    │                       │
         THOTH_ENGINE_URL unset    THOTH_ENGINE_URL set
                    │                       │
                    ▼                       ▼
         LocalAgentBackend          RemoteAgentBackend
         BasicAgentPlugin           libcurl HTTP + SSE
         (in-process cognition)     (transport only)
                    │                       │
                    ▼                       ▼
         Host agent_workspace/      Compose thoth-engine
         RAG · memory.db · index    /workspace · /logs
```

**What already works remotely (Plan K):**

| Capability | Path |
|------------|------|
| Chat | `POST /v1/chat` |
| Goals | `POST /v1/goals` |
| Pause / Resume / Abort | `POST /v1/control/*` |
| Live events | `GET /v1/events` (SSE → `ControllerEvent`) |
| Health gate | `GET /health`, `GET /ready` |

**What still behaves as if the GUI owns cognition (remote mode):**

| Area | Current behavior | Problem |
|------|------------------|---------|
| RAG Files tab / drag-drop | Host paths + `setRagFiles` no-op | Claims “indexing…” while Engine never indexes host files |
| Conversation memory sync | Skipped when remote | Host chat history ≠ engine memory |
| Crash resume | `checkResumablePlan` no-op | Resume UI inert |
| Strategies / Trajectories / Experiments / Graph stats | Empty JSON | Panels look empty, not “unavailable remotely” |
| Explain Plan / Logs tab | Read **host** `decision_trace.jsonl` | Host traces ≠ container cognition |
| Benchmarks menu | Spawns **host** binaries | Runs local cognition, not Compose engine |
| Session store | Host `chat_sessions.json` | Fine as UI chrome; must not be mistaken for engine memory |
| Executive strip indexing counter | Can increment from drop path without Engine events | Fabricates progress |

**Engine ownership today (Plan L):**

- Named volume → `/workspace` (`memory.db`, `rag/`, `rag_index.bin`)
- Seed corpus via `docker/seed_rag/` + `./docker/seed-workspace.sh` (whitelist only)
- Host mirroring of `agent_workspace/` **forbidden**
- Phase 3 ingest (`POST /v1/rag/ingest`) **explicitly out of Plan L**

---

## Target Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ Host GUI — presentation client (Display · Request · UI cache only) │
│  • Display Engine events & query responses                         │
│  • Request user intent (chat, goal, control, ingest)               │
│  • Cache transient UI state only (layout, tabs, selection)         │
│  • Never invent progress, diagnostics, or cognitive state          │
└───────────────────────────────┬────────────────────────────────────┘
                                │ HTTP + SSE (authoritative)
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ Thoth Engine — sole cognitive authority                         │
│  RAG · chunking · embeddings · indexing · document management   │
│  Memory · retrieval · GRAG · planner · executive · tools        │
│  Cognate stores · traces · logs · cognitive state               │
└─────────────────────────────────────────────────────────────────┘
```

**Long-term completion criteria for this roadmap:**

1. Client/server separation is complete: Engine authoritative; GUI presentation client.
2. Every cognitive fact shown in the GUI originates from the Engine (HTTP, SSE, or Engine query API).
3. GUI never fabricates state, invents progress/diagnostics, or reimplements Engine computations (D5).
4. GUI never claims work is occurring unless the Engine reports that it is occurring.
5. Transient UI state (layout, tabs) remains the only class of state the GUI owns.
6. Local in-process mode remains available for development until explicitly retired by a later locked plan — but remote mode must already obey the thin-client rules.

---

## Design Principles

| # | Principle |
|---|-----------|
| D0 | **Single Source of Truth.** Every piece of cognitive state has exactly one owner. The GUI may cache transient UI state but may never become the authoritative owner of cognitive data. |
| D1 | **Engine owns cognition.** GUI never indexes, embeds, retrieves, plans, grounds, or mutates cognitive stores. |
| D2 | **Honesty over convenience.** Prefer disabled/unavailable UI over silent no-ops that look successful. **Honesty phases come before capability phases** so operators debug real Engine behavior, not GUI fiction. |
| D3 | **Events are truth for progress.** Indexing, steps, retrieval diagnostics, and plan state update only from Engine-reported events (or explicit query APIs). |
| D3a | **Progress reflects backend-reported work only.** The GUI never predicts, simulates, or infers progress. Authoritative signals are: Engine SSE events, explicit Engine API responses, and Local backend events when Local owns cognition. User actions alone must not invent “working” / indexing / syncing state. |
| D4 | **No host/container path confusion.** Host `agent_workspace/` must never be presented as Engine state when remote. |
| D5 | **No Duplicate Logic.** If a computation exists in the Engine, the GUI must consume its result rather than reimplementing the computation (scores, grounding, routing mode, chunk counts, plan status, etc.). Divergence is inevitable once two implementations exist. |
| D6 | **Transport stays thin.** New Engine APIs are additive, versioned, and documented before GUI consumption — and only when Display/Request cannot be satisfied by existing contracts. |
| D7 | **Small lockable phases.** Each phase ends in a stable working system; no automatic continuation. |
| D8 | **Local mode is not a license to lie in remote mode.** Dual-mode is allowed; remote honesty is mandatory. |
| D9 | **Plan K/L locks stand until superseded.** This roadmap proposes future work; it does not silently reopen closed plans. New ingest/cognate APIs require their own lock cycle. |
| D10 | **No capability inference from missing data.** The GUI must not treat an empty payload as “capability missing.” Capability is an explicit backend property; empty data is a successful Empty answer. (`if (!supportsX) Unavailable; else if (data.empty()) Empty;`) |
| D11 | **Backend authority for cognitive diagnostics.** The GUI may only present cognitive information obtained from the active backend. Host filesystem artifacts (`decision_trace.jsonl`, local snapshots, cached explanations, etc.) are never a substitute for Engine cognition when Engine is authoritative. Unavailable must state that the capability is not exposed — not merely hide a control. |
| D12 | **Engine produces diagnostics; GUI renders them.** The GUI may never reconstruct, re-summarize, or invent cognitive-diagnostic content from raw traces or events. Resource APIs return Engine-authored structured payloads; the GUI presents fields only. |
| D13 | **Engine owns the presentation model for cognitive diagnostics.** Which fields belong in a decision/plan summary is an Engine contract. The GUI does not choose, invent, or reorder the diagnostic model — it renders what the Engine provides (and handles Unavailable / schema_version). |
| D14 | **Engine-backed collection presentation (Phase 11+).** Any Engine-backed collection resource must present exactly one of: **Loading, Populated, Empty, Error**. Failed fetch → **Error** — never silent empty success. **Unavailable** applies only when the capability is not exposed. The GUI never caches collection items as authoritative research truth; refreshes originate from the Engine. |

---

## Assumptions

1. Compose Engine (`thoth-engine` + inference) remains the production remote target.
2. `THOTH_ENGINE_URL` remains the mode selector until a later Preferences phase (if any).
3. Plan K transport contracts (`/v1/chat`, `/v1/goals`, `/v1/control/*`, `/v1/events`) remain the baseline.
4. Plan L engine-owned corpus model remains the baseline for remote RAG files on disk.
5. Human approval gate (`AGENTS.md`) applies to every phase: Plan → Review → Refine → Lock → Implement → Verify.
6. Scientific / eval harnesses (E2, benchmarks) may stay host-driven unless a phase explicitly moves them behind Engine APIs.
7. Session chrome (`chat_sessions.json` titles, message bubbles) may remain host-local as **UI transcript**, provided it is never labeled as Engine episodic/conversation memory without sync.

---

## Deferred Items

These are acknowledged and **not** scheduled inside early phases:

| Item | Why deferred |
|------|----------------|
| Containerized wxWidgets GUI | Explicitly out of docker roadmap v1 |
| API authentication | Out of docker roadmap v1 |
| Automatic host↔volume mirroring of `agent_workspace/` | Forbidden by Plan L |
| Baking corpus into engine image | Forbidden by Plan L |
| Full Cognate write APIs + Experiment Lab remote runs | Requires dedicated cognate HTTP plan |
| Nightly containerized `test-suite-full` (docker Step 8) | Separate ops track |
| Retiring LocalAgentBackend entirely | Only after remote parity + explicit lock |
| Preferences UI for engine URL | Optional later; env selection works today |

---

## Risks

| Risk | Impact | Mitigation direction |
|------|--------|----------------------|
| Misleading “indexing…” / empty cognate panels | Users believe RAG/cognition is broken | Early honesty phases before new APIs |
| Host decision_trace / diagnostics shown while Engine authoritative | False debugging conclusions | Phase 3 — Backend authority for cognitive diagnostics |
| Large “ingest + cognate + sync” mega-phase | Hard to review/lock; regresses easily | Many small phases |
| SSE disconnect without reconnect | Stale UI mid-goal | Dedicated reconnect phase |
| Dual-mode drift | Local and remote UX diverge silently | Capability matrix + mode banner |
| Reopening Plan L ingest without lock | Protocol discipline break | Separate ingest plan document + lock |
| GUI still calling local plugin helpers “just for display” | Hidden cognition | Audit checklist per phase |

---

## Ownership Matrix (current → target)

| Domain | Current owner (remote) | Target owner | GUI role | Engine role | API today |
|--------|------------------------|--------------|----------|-------------|-----------|
| Chat generation | Engine | Engine | Send text; show reply | Generate | `/v1/chat` |
| Goal execution | Engine | Engine | Start/control; show SSE | Plan/execute | `/v1/goals`, control, events |
| RAG document set | Host UI slots (ignored) | Engine | Request ingest/list; show Engine **corpus documents** | Store, chunk, index | **None** (seed script only; Phase 8 list API) |
| Chunking / indexing progress | GUI fabricates on drop | Engine events | Display INDEXING_* | Emit events | Events exist; drop path wrong |
| Retrieval / GRAG | Engine | Engine | Show diagnostics from events | Score/retrieve | SSE `RETRIEVAL_DIAGNOSTICS` |
| Conversation memory | Host-only (unsynced) | Engine | Optional sync UI | Persist | **None** |
| Episodic / cognate | Empty panels | Engine | Present records | Persist/query | **None** |
| Decision traces / logs | Host files | Engine | Fetch/display | Write `/logs`, workspace traces | **None** |
| Benchmarks | Host binaries | Host tooling *or* Engine job API | Label mode; never pretend remote | Optional later | Host only |
| Session list UI | Host JSON | Host UI chrome OK | Manage UI sessions | Ignore unless sync API | N/A |

---

## Workflow (every phase)

```
Analyze → Create/Refine Plan → STOP (human review)
    → Plan Lock (explicit)
    → Implement (exact locked scope)
    → Testing
    → Human Approval
    → Next phase (never automatic)
```

No phase implements the next phase’s work. No phase locks itself.

---

# Implementation Phases

---

### Phase 0 — Inventory Freeze & Capability Contract

**Status:** 🔒 **Locked** (2026-07-19) — docs only; ownership vocabulary frozen

**Objective**  
Freeze a written capability / ownership matrix for Local vs Remote backends and map every MainFrame surface to “Engine-backed / Host-only / Unavailable remotely,” so later phases share one stable reference — analogous to protocol documents for harness work.

**Current State (at lock)**  
Plan K transport client ✅; Plan L engine-owned corpus ✅; MainFrame has partial remote awareness; fabricated RAG indexing and host-trace/cognate gaps remain for later phases.

**Desired End State**  
✅ Achieved: Phase 0 lock record; D0–D9; GUI may/may-not; Unavailable vs Empty; Ownership Matrix; Appendix A + D normative. **No code.**

**Scope (completed by this lock)**  
- Enumerated UI surfaces in Appendix A.  
- Unavailable vs Empty conventions.  
- D0, may/may-not, D5 normative.  
- Appendix D quick-scan ownership table.  
- Phase ownership of gaps identified in Appendix A.

**Out of Scope**  
Code; locking Phase 1+; API design beyond naming in later phases.

**Completion Criteria**  
- ✅ Ownership principles approved via Phase 0 lock.  
- ✅ Matrix + Appendix D frozen.  
- ✅ Phase order retained (honesty first).  
- ✅ No code merged under Phase 0.

**Dependencies**  
Plan K, Plan L.

**Expected User Experience**  
Unchanged by Phase 0.

**STOP** — Phase 0 locked. Next: lock Phase 1 (Option A) before any implementation.

---

### Phase 1 — Remote Honesty: Stop Fabricating Indexing

**Status:** 🔒 Locked (2026-07-19) · ✅ **Implemented** 2026-07-19 (Option A)

**Objective**  
Eliminate the highest-impact lie: host RAG drop/delete paths that claim indexing while `RemoteAgentBackend::setRagFiles` is a no-op.

**Current State**  
`HandleFileDrop` sets “Indexing dropped files…” / “indexing…” and calls `setRagFiles`. Delete path calls `setRagFiles`. Engine never receives host files (Plan K/L).

**Desired End State**  
In remote mode, GUI never claims indexing from host file drops. Status text states that host RAG slots do not sync to the Engine. Activity strip does not show fabricated indexing. Host slots remain as **UI notes** so Phase 9 ingest can pick them up without a “where did my drops go?” transition bug.

**Product choice (locked):** **Option A — Keep slots as host-only notes**  
- Allow drop/import into session JSON and sandbox migrate as today.  
- Label slots **Host-only (not sent to Engine)**.  
- **Never** claim indexing; **skip** `setRagFiles` when remote.  
- Rejected alternative: Option B (disable add path entirely).

**Scope (locked)**  
1. `HandleFileDrop` / Import Corpus: if remote → no `setRagFiles`; no strip “Indexing dropped files…”; locked status string below; still save/migrate/refresh (Option A).  
2. RAG slot delete: if remote → session JSON + refresh only; no `setRagFiles`.  
3. `RefreshRagPanel`: remote non-empty slots show host-only label or tooltip.  
4. Docs: one remote smoke bullet in `docker/README.md`; Phase 1 ✅ marker in this file + `completed_improvements_log.md` after implement.  
5. Optional pure helper for status strings (unit-testable without wx).

**Locked status / label strings**

| Situation | Text |
|-----------|------|
| Remote add (status bar) | `Added N file(s) — host-only; not sent to Engine` |
| Remote slot label (or tooltip) | `filename (host-only)` / tooltip `Host-only (not sent to Engine)` |
| Remote strip on add | Do **not** set “Indexing dropped files…” (clear or leave prior Engine activity) |
| Optional seed hint (status or dialog footnote) | May append: `Seed engine corpus: ./docker/seed-workspace.sh` |

**Out of Scope**  
Engine ingest, corpus list API, seed script changes, cognate APIs, mode banner (Phase 2), host trace ban (Phase 3), changing `RemoteAgentBackend::setRagFiles` implementation (GUI simply stops calling it when remote). Local-mode behavior changes beyond not breaking them. Option B.

**Implementation Considerations**  
Preserve host paths for Phase 9 ingest transition. INDEXING_* SSE handlers remain for **real** Engine events. Grep Phase 1 PR for stray remote `setRagFiles` from MainFrame RAG paths.

**Potential Risks**  
Users may still think host slots feed retrieval — mitigate with host-only labeling. Phase 9 maps notes → **explicit [Send to Engine]** create-document request without reintroducing fake indexing.

**Validation Strategy**  
- Remote: drop/import → no indexing claims; `docker exec … ls /workspace/rag` unchanged; slots labeled host-only.  
- Local: drop → real INDEXING_* / indexing status as today.

**Required Tests**  
- Unit test for remote status/policy helper if extracted.  
- Manual remote + local checklist.  
- Smoke line in `docker/README.md`.

**Completion Criteria (post-implement)**  
- Remote add/delete never calls `setRagFiles`.  
- No fabricated indexing activity/status on remote add.  
- Remote slot labeling honest.  
- Local indexing path unchanged.  
- Human signs off on live UX.

**Dependencies**  
Phase 0 🔒 (RAG Files row).

**Expected User Experience**  
Remote: host-only notes, honest status. Local: previous indexing feedback.

**Files (implement)**  
`src/MainFrame.cpp` · optional `includes/` helper · `docker/README.md` · docs status markers.

**STOP** — Phase 1 implemented. Do not start Phase 2 until Phase 2 is locked and approved.

---

### Phase 2 — Mode Banner, Capabilities & Panel Presentation States

**Status:** 🔒 Locked + ✅ implemented 2026-07-19

**Objective**  
Make the active cognitive backend continuously visible, drive research surfaces from an explicit **capability model** (not sprinkled `isRemote()` heuristics), and present diagnostic panels with a consistent state machine so Unavailable is never confused with Empty.

**Current State**  
Brief status on session activate; cognate panels refresh to `[]` and look like “no data.” Benchmarks run host binaries while the UI may be Engine-backed. Mode string historically said “Remote.”

**Desired End State**  
- Persistent mode: **`Backend: Engine`** or **`Backend: Local`** (user-facing; implementation may still use `isRemote()` to *derive* capabilities).  
- Panels support presentation states: **Loading · Empty · Populated · Unavailable · Error**.  
- Strategies / Trajectories / Experiments / Graph-stats use **Unavailable** when the capability is false — never because the array is empty.  
- Benchmarks menu items **disabled** when `supportsBenchmarks` is false; menu/status communicates “Available in Local backend” without click-then-dialog.  
- UI copy never mentions Plan K / roadmap codenames.

**Product choices (🔒 locked)**  

| Choice | Decision |
|--------|----------|
| Mode labels | `Backend: Engine` / `Backend: Local` (not “Remote”) — user cares who owns cognition, not Docker vs cluster |
| Unavailable copy | `Unavailable with the current backend.` or `This feature is not available from the current backend.` — **no Plan K / roadmap codenames in UI** |
| Benchmarks | Menu items **disabled** when unsupported; availability communicated in-menu (no click-then-fail dialog) |
| Panel API | Standardize `SetPresentationState` (Loading · Empty · Populated · Unavailable · Error) on every diagnostic panel |
| Capability gate | `BackendCapabilities` struct; GUI branches on capabilities — `isRemote()` only inside capability derivation |
| D10 (normative) | Never `if (data.empty()) showUnavailable()` — always `if (!supportsX) Unavailable; else if (data.empty()) Empty` |

**Benchmarks menu UX (Engine mode example)**  

```
Benchmarks
------------
Run GRAG...              (disabled)
Run Retrieval...         (disabled)
Run Strategy Learning... (disabled)
Run Full System...       (disabled)
------------
Status: Available in Local backend
```

Disabled controls + in-menu status beat initiating actions that cannot succeed.

**`BackendCapabilities` (Phase 2 v1 — 🔒 locked)**  

| Flag | Local | Engine (HTTP client) |
|------|-------|----------------------|
| `supportsStrategies` | true | false |
| `supportsTrajectories` | true | false |
| `supportsExperiments` | true | false |
| `supportsGraphStats` | true | false |
| `supportsBenchmarks` | true | false |
| `supportsLogs` | true | false (Phase 3 will keep Unavailable until Engine log API) |
| `supportsIngest` | true (via setRagFiles) | false (host-only notes; Phase 9 **`supportsIngest`** via `/ready`) |
| `supportsPlanDiagnostics` | true | false (Phase 3 D11; Phase 4 Engine trace API) |

Derive once from backend type at startup (and on mode change if ever supported). Prefer `agent->capabilities()` over repeating `isRemote()` in panels.

**Scope**  
1. Status-bar (or equivalent persistent one-line) mode indicator: `Backend: Engine` / `Backend: Local` (replace transient “Remote engine mode — …” strings where they act as mode banner).  
2. `BackendCapabilities` on `IAgentBackend` / `AgentInterface::capabilities()` (+ unit tests for Local vs Engine v1 matrix).  
3. `PanelPresentationState` enum + `SetPresentationState` on Strategy, Trajectory, Experiment Lab, Graph-stats panels (Loading reserved; use only where a real wait exists).  
4. `RefreshAllPanels` policy (D10):

```cpp
const auto caps = agent->capabilities();
if (!caps.supportsStrategies)
    m_strategyPanel->SetPresentationState(Unavailable, kUnavailableBackendCopy);
else
    m_strategyPanel->UpdateStrategies(agent->getStrategies()); // → Empty or Populated
```

5. Benchmarks menu: `Enable(false)` on host-only items when `!supportsBenchmarks`; append disabled “Status: Available in Local backend” line (wxMenu item or status-bar companion — no modal on click).  
6. Grep guard: no user-visible “Remote” as mode name, no “Plan K” in GUI strings.  
7. `docker/README.md` smoke checklist lines for Engine vs Local panel/benchmark behavior.

**Likely touch points (implementation preview)**  

| Area | Files |
|------|-------|
| Capabilities | `includes/backend_capabilities.h`, `i_agent_backend.h`, `local_agent_backend.*`, `remote_agent_backend.*`, `AgentInterface.*` |
| Panel states | `includes/panel_presentation_state.h`, `StrategyPanel.*`, `TrajectoryViewer.*`, `ExperimentLabPanel.*`, `GraphPanel.*` |
| Wiring | `MainFrame.cpp` (`RefreshAllPanels`, menu init, status bar) |
| Tests | `tests/unit_tests.cpp` (capability matrix, D10 policy helper, mode label strings) |

**Out of Scope**  
Cognate/graph/log HTTP APIs; changing benchmark binaries; Phase 3 D11 cognitive-diagnostics authority (may set `supportsLogs=false` for Engine now as honesty, without implementing Engine log fetch); Preferences for backend URL; inventing Loading for every panel if no async fetch yet (Loading reserved in the API for when fetches exist).

**Implementation Considerations**  
- Avoid new sidebar collapsible chrome (`architectural_facts.md` §8).  
- Graph SSE node highlight remains event-driven; stats block follows `supportsGraphStats`.  
- Phase 1 RAG host-only path can later consult `supportsIngest` (optional small consistency pass — do not reopen Phase 1 scope unless needed).  
- Future backends (Docker Engine, remote server, cluster) only change the capability matrix + mode label policy — GUI stays capability-driven.

**Potential Risks**  
Over-building Loading before async panel fetches exist — ship the enum/API, use Loading only where a wait is real. Capability struct drift — keep v1 matrix in one function next to backend selection.

**Validation Strategy**  
- Engine mode: `Backend: Engine`; Strategy/etc. Unavailable (not empty tables); Benchmarks disabled with Local availability hint.  
- Local mode: `Backend: Local`; empty cognate after real empty query shows Empty; Benchmarks enabled.  
- Grep UI strings: no “Plan K”, no user-facing “Remote” as mode name.

**Required Tests**  
- Unit: mode labels; Unavailable copy; capability matrices Local vs Engine; “empty data ≠ Unavailable” policy helper.  
- Manual: dual-mode smoke checklist.

**Completion Criteria**  
- Mode always Engine or Local.  
- No cognate panel implies Empty when capability is missing.  
- Capability known explicitly (D10).  
- Benchmarks disabled when unsupported without click-to-fail.  
- Local unchanged for supported surfaces.

**Dependencies**  
Phase 0 🔒 · Phase 1 ✅.

**Expected User Experience**  
Clear who owns cognition; research panels honest; menu state self-explanatory.

**STOP** — Phase 2 implemented. Phase 3 ✅ — see Phase 3 section.

---

### Phase 3 — Backend Authority for Cognitive Diagnostics

**Status:** 🔒 Locked + ✅ implemented 2026-07-19

**Objective**  
Establish the architectural rule: **the GUI may only present cognitive information obtained from the active backend.** When Engine is authoritative, every cognitive diagnostic view must originate from that backend — or the UI must clearly indicate the capability is unavailable. Host file reads are one violation of this rule, not the definition of the phase.

**Normative principle (D11)**  
> The GUI may only present cognitive information obtained from the active backend.

Host `agent_workspace/` JSONL, local snapshots, and cached explanations are never a legitimate substitute for Engine cognition in Engine mode.

**Current State**  
- Phase 2: Logs tab and cognate panels already Unavailable under Engine via `supportsLogs` / capability matrix.  
- `AgentInterface::getLatestDecisionTraceSummary` and **Explain Plan** still read host `decision_trace.jsonl` even when cognition ran in the container.  
- Other GUI surfaces may still derive cognitive meaning from local artifacts — audit required.

**Desired End State**  
- Engine mode: every cognitive diagnostic either comes from the active backend API/events, or shows an explicit Unavailable explanation (not a silent empty / wrong host file).  
- **Explain Plan** expands copy beyond a bare Unavailable, e.g.:

```
Explain Plan
Unavailable

The current backend does not expose plan diagnostics.
```

- Local mode unchanged (Local backend may still read host workspace — that *is* the active backend’s store).  
- Phase 4 remains the place that adds Engine APIs so Unavailable can become real data.

**Product choices (🔒 locked)**  

| Choice | Decision |
|--------|----------|
| Phase name / framing | Architectural principle (D11), not “ban file reads” |
| Explain Plan UX | Title + Unavailable + one-line **why** (capability not exposed) |
| Unavailable copy family | Align with Phase 2; prefer capability-specific why for Explain Plan / Logs |
| Audit breadth | All GUI features that derive cognitive info from local artifacts (see list below) |
| Phase 4 split | Honesty (Phase 3) before Engine read APIs (Phase 4) — keep separate |
| Future abstraction | GUI asks `GetDecisionTrace()` (or equivalent); backend chooses JSONL / SQLite / HTTP — not Phase 3 scope |

**Cognitive-diagnostics audit list (Phase 3)**  

Gate or document each surface against D11 when Engine is authoritative:

| Surface / artifact | Typical path today | Phase 3 expectation |
|--------------------|--------------------|---------------------|
| `decision_trace.jsonl` | Explain Plan, Logs | Unavailable + why (no host content) |
| `app_log` / related logs | Logs / debug UI | Unavailable + why if presented as Engine truth |
| Executive state snapshots | Any host-derived executive summary | Backend or Unavailable |
| Planner outputs | Host-cached plan text presented as live | Backend or Unavailable |
| Retrieval traces | Host-only retrieval dumps | Backend/SSE or Unavailable |
| GRAG diagnostics | Prefer Engine events; no host-file fallback as Engine truth | Audit; keep event-driven if already Engine-sourced |
| Benchmark summaries | Host harness output | Already `supportsBenchmarks=false`; confirm no Engine-labeled host summaries |
| Cached explanations | Stale host “explain” blobs | Do not show as current Engine diagnostics |

Even surfaces that are already honest under Phase 2 must appear on the audit checklist so the rule is applied consistently.

**Scope**  
1. Codify D11 in this roadmap + gate Explain Plan / `getLatestDecisionTraceSummary` (and any other remaining host cognitive readers found by audit).  
2. Prefer `capabilities()` / `supportsLogs` (and a plan-diagnostics flag if needed) over sprinkled `isRemote()` — same Phase 2 pattern.  
3. Explain Plan dialog: Unavailable **with reason** (“does not expose plan diagnostics”).  
4. Produce a short audit checklist result in completion notes (which surfaces gated vs already-compliant vs deferred).  
5. Unit tests for unavailable sentinels; no Plan K / roadmap names in user-visible strings.  
6. Docker smoke: remote goal → Explain Plan must not show host-only traces.

**Out of Scope**  
- Building Engine log/trace HTTP APIs (**Phase 4**).  
- Unifying Local filesystem vs Engine HTTP behind a single `GetDecisionTrace()` abstraction (future cleanup after Phase 4 — noted as direction, not this phase).  
- Changing how Local backend stores traces.  
- Reopening GRAG scoring / event schemas.

**Implementation Considerations**  
- Phase 2 already set `supportsLogs=false` for Engine and Unavailable on the Logs tab — Phase 3 **closes remaining gaps** (Explain Plan foremost) and documents the broader invariant.  
- Local filesystem reads remain valid when Local is the active backend.  
- Audit may find GRAG diagnostics already SSE/event-driven — mark compliant; do not regress.  
- Future: GUI should not care whether the backend got data from JSONL, SQLite, REST, gRPC, or Docker — only `backend->getDecisionTrace()` (Phase 4+ evolution).

**Potential Risks**  
Temporary loss of remote debug convenience — acceptable for honesty. Over-gating event-driven panels that already obey D11 — mitigate with explicit audit outcomes.

**Validation Strategy**  
- Engine mode after a remote goal: Explain Plan shows Unavailable + why; never host `decision_trace` from an unrelated local run.  
- Logs remain Unavailable (Phase 2) or same copy family.  
- Local mode: Explain Plan / Logs still work from host workspace.  
- Grep / unit guards: no Engine-mode path that presents host cognitive file bytes as diagnostics.

**Required Tests**  
- Unit: Engine/remote path returns unavailable sentinel (no host file content).  
- Unit: Explain Plan copy includes capability-why language (no Plan K).  
- Manual: remote goal + Explain Plan; Local regression.

**Completion Criteria**  
- **No GUI cognitive view may derive its content from local artifacts while the Engine backend is authoritative.**  
- **Every cognitive diagnostic displayed in Engine mode must originate from the active backend, or the UI must clearly indicate that the capability is unavailable** (with a short why where the surface is user-initiated, e.g. Explain Plan).  
- Audit checklist recorded for the surfaces above.  
- Phase 4 still required before remote Explain Plan / Logs show real Engine data.

**Dependencies**  
Phase 0–2 🔒 · Phase 2 ✅ (capability model + Unavailable patterns).

**Expected User Experience**  
Operators see an honest gap with a clear reason, not a convincing-but-wrong host trace.

**STOP** — Phase 3 implemented. Phase 4 ✅ — see Phase 4 section.

---

### Phase 4 — Engine Decision Summary Resource (Explain Plan)

**Status:** 🔒 Locked + ✅ implemented 2026-07-19 — Explain-only

**Objective**  
Restore **Explain Plan** via a single Engine-owned, resource-oriented **decision summary** — structured JSON over a shared `IAgentBackend` method. No logs, no activity feed, no file/tail APIs in this phase.

**Normative principles**  
- **D11** — cognitive info only from the active backend.  
- **D12** — Engine produces diagnostics; GUI only renders them.  
- **D13** — Engine owns which fields belong in the diagnostic presentation model.

**Framing (cohesive sequence)**  

| Phase | Theme |
|------:|-------|
| 1 | Honest work reporting |
| 2 | Honest capability reporting |
| 3 | Honest diagnostics ownership (Unavailable) |
| **4** | **Restore Explain Plan via Engine-owned resource** |
| Later | Recent activity · Logs · Session timeline · Event history |

**Current State**  
- Engine: Explain Plan Unavailable with why (Phase 3).  
- Local: string summary from host `decision_trace.jsonl` inside `AgentInterface`.  
- File/tail-oriented HTTP drafts — **rejected**.

**Desired End State**  

```
GUI (presentation only)
        │
AgentInterface
        │
IAgentBackend::capabilities()
IAgentBackend::getLatestDecisionSummary()  → structured JSON
        │
   ┌────┴────┐
LocalBackend   RemoteBackend
(JSONL/SQLite   (HTTP resource)
 internal)      
        │
     Engine
```

- GUI does not know HTTP, JSONL, SQLite, Docker, or files.  
- Logs tab remains Phase 3 Unavailable under Engine until a **future phase**.  
- Capability `supportsPlanDiagnostics` becomes `true` on Engine when the resource is live (backend-advertised).

**Product choices (🔒 locked)**  

| Choice | Decision |
|--------|----------|
| Scope | **Explain-only** — not Logs / activity / timeline |
| Backend method | `getLatestDecisionSummary()` (not “explanation” — avoids LLM-prose implication) |
| HTTP resource (illustrative) | `GET /v1/diagnostics/latest-decision` (or equivalent; not `/v1/logs/…`) |
| Forbidden | `tail`, path-to-JSONL, arbitrary file browse, GUI log parsing |
| Payload | Engine-authored structured JSON; **`schema_version` required** |
| Local/Remote | Same interface **required** |
| Field ownership | Engine (D13); GUI renders provided fields |
| Session evolution | Design allows later `GET /v1/session/{id}/diagnostics/latest-decision`; v1 = active-session latest |
| Capability | Flip `supportsPlanDiagnostics` via `capabilities()` when live |

**Locked method naming rationale**  
Prefer `getLatestDecisionSummary()` over `getLatestExplanation()` — signals Engine cognitive-pipeline summary, not an LLM narrative. Alternatives considered and rejected for Phase 4 primary name: `getLatestPlanExplanation`, `getLatestExecutionSummary` (may revisit if schema grows).

**Minimal v1 payload (schema_version mandatory)**  

```json
{
  "schema_version": 1,
  "session_id": "...",
  "goal": "...",
  "executive_summary": "...",
  "planner_summary": "...",
  "retrieved_chunks": [],
  "selected_strategy": null,
  "execution_time_ms": 0
}
```

- Clients **MUST** read `schema_version`.  
- Additive fields allowed in later schema versions; removing/renaming requires a version bump.  
- GUI renders fields present in the payload; it does not invent missing ones.

**Scope (Phase 4 only)**  
1. Engine HTTP handler for the decision-summary resource + contract docs.  
2. `IAgentBackend::getLatestDecisionSummary()` → `nlohmann::json` on Local + Remote.  
3. `AgentInterface` exposes the same; replace Explain Plan’s host-string path.  
4. MainFrame Explain Plan: Loading → render structured fields (or Unavailable if `!supportsPlanDiagnostics`).  
5. Remote `capabilities()` advertises `supportsPlanDiagnostics=true` when endpoint is live.  
6. Unit/integration tests + dual-mode smoke.  
7. Docker checklist: remote goal → Explain Plan shows Engine decision summary.

**Out of Scope (explicit future phases)**  
- Recent activity / Logs tab / session timeline / event history.  
- `getRecentActivity()` or any tail-like API.  
- Multi-session path implementation (reserve shape only).  
- Cognate / strategies / graph HTTP.  
- Auth, full log shipping, file browser.  
- GUI reconstruction of summaries from SSE.

**Implementation Considerations**  
- Size limits + redaction on Engine before response.  
- Do not block wx UI thread — async + Phase 2 Loading state if needed.  
- LocalBackend may assemble the structured object from workspace artifacts **internally**.  
- Keep `supportsLogs=false` on Engine until a logs/activity phase.  
- Prefer one capability flag (`supportsPlanDiagnostics`) — do not add a redundant `supportsExplain`.

**Potential Risks**  
- Minimal schema too thin for useful Explain Plan — mitigate with executive + planner summaries required in v1.  
- Local assembly diverging from Engine assembly — share field contract tests; D5/D12/D13 apply to both backends’ *outputs*, not storage.

**Validation Strategy**  
- `curl` resource → JSON with `schema_version` (not raw JSONL).  
- Remote Explain Plan matches Engine payload for a fresh goal.  
- Local Explain Plan uses identical GUI code path.  
- Grep: no Explain Plan path reads `decision_trace.jsonl` in GUI after this phase.  
- Logs still Unavailable under Engine (regression check).

**Required Tests**  
- Engine HTTP: `schema_version` + required v1 fields.  
- Remote/Local backend mapping offline.  
- Capability matrix: Engine `supportsPlanDiagnostics=true` after ship.  
- Manual: Engine Explain Plan; Local regression; Logs still Unavailable remotely.

**Completion Criteria**  
- Explain Plan restored under Engine via `getLatestDecisionSummary()`.  
- Local and Engine share the same method; GUI is storage-agnostic.  
- No file/tail public API; no GUI reconstruction of diagnostics.  
- Logs/activity **not** claimed done.

**Dependencies**  
Phase 0–3 🔒 · Phase 3 ✅. Additive Engine API (does not reopen Plan K).

**Expected User Experience**  
Explain Plan works again remotely with real Engine decision summaries; same button path locally.

**STOP** — Phase 4 implemented (Explain-only). Phase 5 locked separately — implement Phase 5 only on explicit approval.

---

### Phase 5 — Progress Reporting Discipline

**Status:** 🔒 Locked + ✅ implemented 2026-07-19

**Objective**  
Make UI progress trustworthy: the GUI **reflects backend-reported work** and never predicts, simulates, or infers it. After this phase, a lively UI must mean the cognitive owner actually reported work — not a fictional local story.

**Normative principle (D3 / D3a)**  
> Progress state must originate from authoritative backend signals.

| Mode | Allowed progress signals |
|------|--------------------------|
| Engine | Engine SSE events · explicit Engine API responses |
| Local | Local backend events (plugin / in-process owner of the work) |

Both are valid because both originate from the **actual owner of the work**. “Events only” means **backend-reported signals only** — not “SSE forever to the exclusion of Local events or future Engine APIs.”

**Implemented (2026-07-19)**  
- `includes/progress_source.h` — `ProgressSource`, `mayApplyWorkProgress` / `mayApplyIndexingProgress`, chrome strings, grep checklist  
- MainFrame: removed optimistic Planning… / Syncing… / drop indexing claims; `ApplyWorkActivity` / `ApplyWorkStatus` gated by provenance  
- Host drop never claims indexing (Phase 1 helper always false); Local chrome “Added N file(s)” until INDEXING_*  
- IndexManager: `INDEXING_COMPLETED` paired on all exits after STARTED (counter integrity)  
- Unit test `testGuiPhase5ProgressReportingDiscipline`

#### Event → UI map (progress surfaces)

| Backend signal | UI update |
|----------------|-----------|
| `INDEXING_STARTED` | `m_ragIndexingCount++`; strip “Indexing RAG files…”; status “Indexing: *file*”; slot “(Indexing…)” |
| `INDEXING_COMPLETED` | counter−−; status “Indexed: *file*”; RefreshRagPanel |
| `STATE_CHANGED` / PLANNING | `m_goalPlanningPending`; strip “Planning…” |
| `STATE_CHANGED` / REVISING_PLAN | strip “Revising plan…” |
| `PLAN_CREATED` / `PLAN_REVISED` | clear planning pending; ResetPlan / failure activity |
| `STEP_*` | strip + plan panel step status; graph node from step_type |
| `PLAN_COMPLETED` / `FAILED` / `ABORTED` | clear pending; status / execution state |
| `PLAN_REUSE_INJECTION` / `REFLECTION_REPLAN` / `PLAN_HISTORY_STORED` | status from event metadata |
| Pause / Resume / Abort API | status after control call (ApiResponse / LocalBackendEvent) |
| User drop / send / revise | **Chrome only** (“Added N…”, “Goal submitted”, “Message submitted”) — never invent work |

**Out of Scope (unchanged)**  
- New Engine event types; SSE reconnect / last-event-age (**Phase 6**); activity history.

**STOP** — Phase 5 implemented. Do not auto-start Phase 6.

---

### Phase 6 — SSE Resilience (Reconnect)

**Status:** 🔒 Locked + ✅ implemented 2026-07-20

**Objective**  
Replace Plan K’s “SSE dies until process restart” with a **bounded, state-machined reconnect policy** so mid-goal UI stays truthful and live. Builds on Phase 5: progress only comes from authoritative events; Phase 6 keeps those events resilient to disconnects.

**Current State**  
No retry/reconnect (Plan K lock). Disconnect ends observability. Progress strip can look “live” with no connection indicator.

**Desired End State**  
- Explicit **connection state machine** drives all connection UI (no ad hoc reconnect strings).  
- **Connection health** and **engine health** are separate surfaces.  
- Simple v1 reconnect: exponential backoff, max interval, retry until user disconnects / mode change; **new events only** (no replay/cursors/dedup).  
- Optional diagnostics **last-event-age** indicator.  
- Long outages: frozen progress + visible reconnecting + clear control affordances — user never wonders if the GUI hung.

---

#### Connection state machine (normative)

```
Connected
    │
    ▼
Disconnected
    │
    ▼
Reconnecting
    │
 ┌──┴──┐
 │     │
 ▼     ▼
Connected   Failed
```

| State | Meaning | Typical UI |
|-------|---------|------------|
| **Connected** | SSE stream healthy; events may arrive | Quiet / “Events: Connected” |
| **Disconnected** | Stream lost (or never opened); not yet retrying / brief gap | “Events: Disconnected” |
| **Reconnecting** | Backoff policy actively retrying | “Events: Reconnecting…” (+ optional next-attempt hint) |
| **Failed** | Terminal — reconnect intentionally stopped (user left Engine mode, backend teardown) | “Events: Failed” + why |

UI **always** reflects exactly one of these states for the event stream. Status copy is derived from the state enum — not scattered free-form messages.

**v1 Failed policy (🔒 locked):** Stay in **Reconnecting** indefinitely (until Local mode / process exit / intentional stop). Do **not** auto-escalate to Failed on timeout or attempt count. Use **Failed** only when reconnect is intentionally stopped (user disconnect, backend teardown).

---

#### Connection health vs engine health (normative)

These are **different**. Do not collapse them into one banner.

| Signal class | Examples | User-facing axis |
|--------------|----------|------------------|
| **Connection** | TCP lost; SSE stream closed; client read error | `Connection: Lost` / state machine above |
| **Engine** | Process stopped; `/ready` unhealthy / starting; capability not ready | `Engine: Starting` / `Engine: Unavailable` / ready |

| What user sees | Implies |
|----------------|---------|
| Connection lost / Reconnecting | Check network / wait for SSE; Engine may still be fine |
| Engine starting / unhealthy | Wait for Engine; SSE may reconnect once `/ready` is good |

**`/ready` during reconnect (🔒 locked):** Probe `/ready` on backoff ticks; surface **Engine** axis from `/ready`; keep **Connection** axis from SSE open/read outcomes. Do not collapse the two into one banner.

---

#### Reconnect policy v1 (🔒 locked)

| Choice | Decision |
|--------|----------|
| Backoff | Exponential backoff |
| Ceiling | Maximum retry interval (cap) |
| Duration | Retry indefinitely until user leaves Engine mode / intentional disconnect, with clear state |
| Failed | Only on intentional stop — never auto on timeout/attempt count |
| On reconnect | Subscribe to **new events only** |
| Replay | **Out** — no Last-Event-ID replay, no cursors, no client deduplication |
| Exceptions | No exceptions across GUI/backend boundary |

Rationale: avoid duplicate STEP storms until a proper resume cursor exists. Quiet progress during gap is correct under Phase 5 (no invented events).

---

#### Last event age

Moved here from Phase 5 deferral.

- Track wall-clock age of last successfully received event while **Connected** (and optionally while Reconnecting — “last seen before disconnect”).  
- Prefer **diagnostics-only** (e.g. GRAG/diagnostics footer or Explain-adjacent), not a front-and-center banner: e.g. `Last event: 12 s ago`.  
- Not a substitute for the connection state machine.

---

#### UX: long Engine unavailability (e.g. five minutes)

Answer locked into the phase:

1. **Progress indicators freeze** — no new events → no invented progress (Phase 5).  
2. **Connection state = Reconnecting** (or Disconnected briefly between attempts).  
3. **Engine axis** shows Starting/Unavailable when `/ready` says so.  
4. **Controls** that require the Engine are disabled **or** clearly indicate they will not work (prefer disable + short why).  
5. User must **never** wonder whether the GUI itself is hung — connection (and engine) state is always visible when not Connected.

---

**Scope**  
1. Connection state enum + single owner updating it from SSE loop outcomes.  
2. `RemoteAgentBackend` SSE reconnect policy (backoff helper).  
3. GUI surfaces: connection state banner/indicator; separate engine-health indicator when probeable.  
4. Last-event-age in diagnostics.  
5. Degraded-mode control messaging / disable policy while not Connected (or while Engine unhealthy).  
6. Documented reconnect contract in this roadmap at lock.

**Out of Scope**  
- Event replay, resume cursors, client-side deduplication.  
- General HTTP retry for chat/goals (Phase 7 honesty may surface failures; retries are separate).  
- New Engine event types.  
- Activity history / Logs timeline.

**Implementation Considerations**  
- Must not violate “no exceptions across boundary.”  
- Prefer client-only; no Engine changes for v1.  
- Local mode: connection state machine N/A or always Connected (in-process events) — do not show Engine SSE reconnect chrome when Local.  
- Align degraded controls with Phase 2 capabilities (don’t invent Unavailable from empty data — D10).

**Potential Risks**  
- Duplicate events if a future Engine replay is enabled accidentally — keep replay off.  
- Banner noise — connection state should be calm when Connected; prominent when not.  
- Confusing Connection vs Engine — copy must stay distinct.

**Validation Strategy**  
- Kill/restart SSE path (network pause / container restart) with GUI open → state transitions Connected → Disconnected → Reconnecting → Connected; no silent live strip.  
- Stop Engine five minutes: progress frozen; Reconnecting; Engine unhealthy; controls honest.  
- Chat/goals without events: degraded messaging (Phase 7 may deepen error honesty).

**Required Tests**  
- Unit tests: backoff helper; connection state transitions.  
- Policy tests: Connection vs Engine copy distinct; last-event-age formatting.  
- Integration/manual docker bounce.  
- Regression: Local mode unchanged; chat/goals still callable with clear degraded UX when events dead.

**Completion Criteria**  
- Written reconnect contract **locked** and implemented.  
- Connection state machine is the sole source of connection status UI.  
- Connection health ≠ engine health in the UI.  
- **Architectural invariant:** Loss of the event stream must never leave the GUI displaying stale “live” status without also displaying the connection state.  
- Last-event-age available in diagnostics.  
- Five-minute outage UX checklist satisfied.

**Dependencies**  
Phase 5 ✅ (progress from events only).

**Implemented (2026-07-20)**  
- `includes/engine_connection_state.h` — state machine, backoff, labels, last-event-age, policy helpers  
- `RemoteAgentBackend::sseReconnectLoop` — exponential backoff, `/ready` probe on retry ticks, new-events-only SSE, snapshot API  
- `AgentInterface::eventStreamSnapshot()` · Local returns `applies=false`  
- MainFrame — 3-field status bar (Backend | Events/Engine | transient), 1s poll timer, degraded agent controls when Engine unhealthy  
- GRAG diagnostics footer — `Last event: N s ago`  
- Unit test `testGuiPhase6EventStreamResilience` · docker smoke item 12  

**Expected User Experience**  
Transient blips reconnect visibly; long outages look like a waiting client, not a hung desktop app. Truthful progress (Phase 5) + robust event delivery (Phase 6).

**STOP** — Phase 6 implemented. Do not auto-start Phase 7.

---


### Phase 7 — Operation Result Honesty

**Status:** 🔒 Locked 2026-07-20 — ✅ implemented 2026-07-20

**Objective**  
Every user-initiated Engine operation produces exactly one visible outcome: **success or failure** — never neither, never both. Surface failures from the active backend without inventing success. Transport-agnostic: today HTTP; tomorrow WebSocket/gRPC/Unix socket — the GUI cares whether the operation succeeded, not which wire carried it.

**Current State**  
Failures often stderr-only; status may claim paused/resumed/aborted/sent before the backend confirms. Phase 6 disables controls when Engine is unhealthy, but duplicate error noise can still appear when connection is already lost.

**Desired End State**  
- Structured internal **`OperationResult`** on every control path (pause, resume, abort, goal, chat).  
- UI policy by severity — status bar / panel / modal — not blanket dialogs.  
- Correlation with Phase 6 connection/engine state — one root cause, one user-facing explanation.  
- **Architectural rule:** no success indication before confirmation from the active backend.

---

#### OperationResult (internal — 🔒 locked)

Conceptual shape (transport-agnostic):

```
OperationResult
---------------
success: bool
operation: string          // e.g. "pause", "abort", "chat", "goal"
user_message: string       // human-readable, for UI
technical_details: string  // optional; logs / expandable detail
retryable: bool            // hint for UX (not auto-retry in v1)
http_status: optional      // when transport is HTTP today
```

Every user-initiated Engine operation flows through the same result type — Local and Remote backends populate it; MainFrame renders from it. No scattered `"Failed to pause"` strings as the sole contract.

---

#### UI presentation policy (🔒 locked)

| Severity | Surface | When |
|----------|---------|------|
| **Status bar** | Transient / network / expected failures | Engine unavailable, connection lost, timeout |
| **Panel notification** | Operation-specific failure | Abort failed, goal rejected, chat error — user initiated the action |
| **Modal dialog** | Only when user must decide or error is unrecoverable | Rare; not default for transport blips |

Most failures must **not** interrupt the user with a modal.

---

#### Correlation with Phase 6 connection state

Avoid duplicate messages when connection/engine health already explains the failure.

| Bad (duplicate) | Better (correlated) |
|-----------------|---------------------|
| Connection lost. · Abort failed. · Goal failed. · Pause failed. | **Engine unavailable.** · Abort request could not be delivered. |

Rules:
- If Phase 6 snapshot shows Engine not Ready or Events not Connected, **root cause** comes from connection/engine indicators (field 1).  
- Operation failure adds **one** operation-specific line — not a second copy of “connection lost.”  
- Prefer: `Engine unavailable — abort request could not be delivered` over stacking four independent errors.

---

#### Architectural rule (normative)

> **Every user-initiated Engine operation produces exactly one visible outcome: success or failure. Never neither. Never both.**

> **No success indication may be displayed before confirmation from the active backend.**

Applies to: pause, resume, abort, goal submission, chat send, and any future Engine-facing control.

---

**Scope**  
1. `OperationResult` (or equivalent) on `IAgentBackend` control/chat/goal paths — Local + Remote.  
2. RemoteAgentBackend: map transport failures (HTTP today) into `OperationResult`; stderr remains for operators.  
3. MainFrame: render policy (status / panel / modal); correlate with Phase 6 snapshot.  
4. Remove optimistic success status on UserAction before backend confirms (extends Phase 5 discipline to outcomes).  
5. Document operation→UI map at lock.

**Out of Scope**  
- Auth.  
- Automatic retries (Phase 6 reconnect is separate).  
- Changing transport (still HTTP in v1 implementation; type stays wire-agnostic).

**Implementation Considerations**  
- No exceptions across GUI/backend boundary (Plan K spirit).  
- Local backend returns `OperationResult` from plugin outcomes where available.  
- Chat async path: failure must reach `onResponse` or explicit failure channel — not silent drop.  
- Phase 6 degraded controls remain; Phase 7 adds honest **result** when user acts anyway or engine becomes ready mid-session.

**Potential Risks**  
- Over-notification if correlation rules are skipped — enforce in review/tests.  
- Async chat/goal race: success chrome vs late failure — tie to requestId correlation.

**Validation Strategy**  
- Stop engine → Abort → one correlated failure (not success + stderr only).  
- Engine up → Pause succeeds → one success indication after backend confirms.  
- Mock HTTP 500/timeout in unit tests → structured failure, no false success.

**Required Tests**  
- Policy/unit: `OperationResult` mapping; correlation helpers; no success-before-confirm.  
- Backend tests with mocked transport failures.  
- Manual docker stop during control op.

**Completion Criteria**  
- Every Engine operation results in an **explicit success or failure** indication.  
- No success indication before confirmation from the active backend.  
- Structured internal results — not ad hoc error strings as the only contract.  
- UI policy (status / panel / modal) documented and applied.  
- Failures correlated with Phase 6 state — no duplicate “connection lost” spam.

**Dependencies**  
Phase 6 ✅ (connection/engine snapshot for correlation). Phase 5 ✅ (no invented progress — extends to outcomes).

**Expected User Experience**  
Controls feel trustworthy: one clear outcome per action; root cause shown once; calm UI unless the user must decide.

**STOP** — Phase 7 implemented. Do not auto-start Phase 8 without implement approval.

---

### Phase 8 — Engine Corpus Listing API

**Status:** 🔒 Locked 2026-07-20 — ✅ implemented 2026-07-20

**Objective**  
Let the GUI display the Engine’s **knowledge corpus** — the set of documents the Engine can retrieve against — instead of treating host session slots as authority. Same architectural move as Phase 4: **resource-oriented, not filesystem-oriented.**

**Normative principle (🔒 locked)**  

> **The Engine owns the corpus model. The GUI presents the corpus model. The GUI never derives the corpus from local filesystem state.**

**Architectural shift**  

| Before Phase 8 | After Phase 8 |
|----------------|---------------|
| GUI thinks in **files** (host paths, slots, mtime/size) | GUI thinks in **corpus documents** (Engine-owned metadata) |
| Remote slots are host-only notes with no Engine truth | Engine corpus is authoritative; host notes are demoted UI chrome |

The Engine may eventually store files, database records, generated documents, or remote sources — the GUI must not need to know.

**Current State**  
Operators use `docker exec` / seed script to see what the Engine has; GUI RAG tab shows **host session slots** labeled `(host-only)` (Phase 1 honesty). No read API.

**Desired End State**  

```
GUI (presentation only)
        │
AgentInterface
        │
IAgentBackend::capabilities()          → supportsCorpusList
IAgentBackend::listCorpusDocuments()   → structured JSON (corpus model)
        │
   ┌────┴────┐
LocalBackend   RemoteBackend
(host rag      (HTTP corpus resource)
 via existing
  index view)
        │
     Engine
```

- GUI does not know `/workspace/rag`, paths, mtimes, or directory listing semantics.  
- Endpoint represents a **knowledge corpus**, not “list files in a folder.”  
- Storage layout may change without breaking the contract.

**Product choices (🔒 locked 2026-07-20)**

| Choice | Decision |
|--------|----------|
| Abstraction | **Corpus documents** — not filesystem listing; no `/workspace/rag` in GUI or public contract prose |
| HTTP resource (illustrative) | `GET /v1/rag/corpus` → **corpus document list** (name is illustrative; payload is document-centric) |
| Document fields (v1) | **`id`** (stable identifier) · **`name`** (display) · **`indexed_at`** (Engine timestamp) · **`status`** (Engine indexing state) · **`chunk_count`** (optional — `null` if expensive/unavailable) |
| Forbidden in GUI contract | `mtime`, raw `size`, host paths, directory paths, “file browser” semantics |
| Capability | New **`supportsCorpusList`** — GUI discovers feature via `capabilities()`; never infer from mode alone |
| Remote RAG tab layout | **Engine Corpus** (primary list) · **▼ Local Notes** (collapsed pane — host slots preserved, not equal peer) |
| Empty / loading / unavailable | **Three distinct remote states** — never merge: **Loading…** · **Corpus is empty.** · **Corpus listing unavailable.** |
| Local mode | May populate corpus list from Local backend’s view of indexed documents (same interface; implementation may differ) |
| Dual lists | **Forbidden** as equal peers — host notes demoted, not shown as Engine retrieval corpus |

**Minimal v1 payload (conceptual — `schema_version` required)**

```json
{
  "schema_version": 1,
  "documents": [
    {
      "id": "doc-grag-md",
      "name": "GRAG.md",
      "indexed_at": "2026-07-20T12:00:00Z",
      "status": "indexed",
      "chunk_count": 42
    },
    {
      "id": "doc-howto-md",
      "name": "HOWTO.md",
      "indexed_at": null,
      "status": "pending",
      "chunk_count": null
    }
  ]
}
```

- Clients **MUST** read `schema_version`.  
- **`id`** is stable for Phase 9 create-document correlation, future delete/replace (deferred), and document-detail views — do not identify documents by filename alone.  
- **`chunk_count: null`** is valid; GUI omits the value when null.  
- Additive fields allowed in later schema versions.

**Remote RAG tab UX (🔒 locked)**

```
Engine Corpus
-------------
GRAG.md          indexed · 42 chunks
HOWTO.md         indexed
AGENTS.md        indexed

▼ Local Notes (collapsed)
completed_improvements_log.md   (host-only)
```

- **Engine Corpus** — from `listCorpusDocuments()` when `supportsCorpusList`.  
- **Local Notes** — existing host slot paths; collapsed by default; never implies Engine retrieval.  
- **Loading…** — fetch in flight.  
- **Corpus is empty.** — successful list, zero documents (distinct from unavailable).  
- **Corpus listing unavailable.** — `!supportsCorpusList`, HTTP/capability failure, or Engine not ready (use Phase 6/7 honesty — do not invent documents).

**Scope**  
1. Engine HTTP handler for corpus document list + contract docs (sandboxed server-side; GUI never sees paths).  
2. `IAgentBackend::listCorpusDocuments()` → structured JSON on Local + Remote.  
3. `BackendCapabilities::supportsCorpusList` + Remote advertises when live.  
4. MainFrame RAG tab: Engine Corpus section + collapsed Local Notes in remote mode.  
5. Presentation states: Loading / Empty / Unavailable / Populated (same discipline as Phase 2 panels).  
6. Unit/integration tests + docker smoke.

**Out of Scope**  
- Upload / ingest (Phase 9).  
- Delete / replace documents.  
- Exposing filesystem metadata (mtime, size, paths) in the API or GUI.  
- Mirroring host `agent_workspace/rag` as Engine truth.

**Implementation Considerations**  
- Align with Phase 4 pattern: GUI renders Engine-provided document model only.  
- Local backend may map existing index/RAG state into the same document shape — no requirement that Local use HTTP.  
- Phase 1 host-only honesty remains for Local Notes; Phase 8 adds Engine truth above it.  
- `chunk_count` computation must not block shipping the list endpoint.

**Potential Risks**  
- Users confuse Local Notes with Engine corpus — mitigate with collapsed demotion + labels.  
- Expensive chunk enumeration — mitigated by optional/null `chunk_count`.

**Validation Strategy**  
- Seed workspace → GUI Engine Corpus matches seeded document **names** (by `id`/`name`, not path).  
- Empty Engine volume → **Corpus is empty.** (not unavailable).  
- Engine down / no capability → **Corpus listing unavailable.**  
- Local Notes remain visible when collapsed; do not disappear.

**Required Tests**  
- HTTP contract tests (schema_version, document shape, optional chunk_count).  
- Capability matrix: `supportsCorpusList` Local vs Engine.  
- Presentation policy: Loading vs Empty vs Unavailable never conflated.  
- Docker verification after seed.  
- Manual GUI remote tab layout.

**Completion Criteria**  
- Remote **Engine Corpus** reflects Engine documents, not host drops.  
- GUI never derives corpus from local filesystem state.  
- Host notes preserved in collapsed **Local Notes** — not equal list.  
- Three remote empty/loading/unavailable states are distinct and tested.

**Dependencies**  
Phases 1–2 ✅ · Phase 7 ✅ · Plan L listing-only (ingest remains Phase 9 / separate lock).

**Expected User Experience**  
“These are the documents the Engine can retrieve.” Host notes remain as personal annotations, clearly separate.

**STOP** — Phase 8 implemented. Do not auto-start Phase 9 without implement approval.

---

### Phase 9 — Corpus Document Creation (Engine-Owned)

**Status:** 🔒 Locked 2026-07-20 · ✅ Implemented 2026-07-20

**Objective**  
Let the GUI **initiate** creation of Engine **corpus documents** and **observe** indexing — without host path sync, without choosing storage, and without merging acceptance with progress. Today a document may originate from a host file; tomorrow it may come from clipboard, URL, generated text, DB export, or another agent — the API is **create corpus document**, not upload multipart.

**Normative principle (🔒 locked)**  

> **The GUI says “create corpus document.” The Engine owns document id, filename, storage, chunking, embedding, replacement policy, and atomicity. The GUI owns initiation and presentation only.**

**Architectural shift**

| Wrong framing | Locked framing |
|---------------|----------------|
| Upload file / multipart form | **Create corpus document** (resource-oriented) |
| Drop → auto-ingest in remote mode | Drop → **Local Note** → explicit **[Send to Engine]** |
| Acceptance + indexing in one UX | **OperationResult** for acceptance · **SSE** for indexing progress |
| Half-indexed visible state | **Atomic:** document exists or it doesn't — Engine owns transaction |

**Document lifecycle (🔒 locked — GUI understands states; transport is implementation detail)**

```
Local Note (host UI chrome)
        │
        ▼  [Send to Engine]  — user-initiated only (no auto-ingest on drop)
Create Corpus Document (request)
        │
        ▼
Accepted          ← structured OperationResult (Phase 7) — one outcome
        │
        ▼
Indexing          ← INDEXING_* SSE only (Phase 5) — not merged with acceptance
        │
   ┌────┴────┐
   ▼         ▼
Indexed    Failed
   │
   ▼
Available (retrievable — reflects in Phase 8 corpus list)
```

**Current State**  
Plan L deferred ingest; seed script is the only supported corpus path. Phase 1 keeps host drops as **Local Notes**; Phase 8 lists Engine corpus documents; remote `setRagFiles` remains a no-op; `supportsIngest` is false in Engine mode.

**Desired End State**  

```
GUI (initiation + presentation only)
        │
AgentInterface::createCorpusDocument(...)  → OperationResult (acceptance)
        │
IAgentBackend::capabilities()  → supportsIngest (from /ready)
        │
   ┌────┴────┐
LocalBackend   RemoteBackend
(setRagFiles     (HTTP create-document resource)
 legacy path)
        │
     Engine  — owns id, filename, storage, chunking, embedding, atomicity
        │
     SSE INDEXING_*  → GUI progress (Phase 5)
        │
     GET /v1/rag/corpus  → lifecycle status visible (Phase 8)
```

- GUI does **not** choose filename, document id, storage path, chunking, embedding, or replacement policy.  
- Wire format (multipart vs JSON+bytes) is **implementation** — lock describes the **operation**, not multipart.  
- **`supportsIngest`** advertised via `/ready`; GUI enables **[Send to Engine]** when true (alongside existing `supportsCorpusList`).

**Product choices (🔒 locked 2026-07-20)**

| Choice | Decision |
|--------|----------|
| Operation | **Create corpus document** — not “upload file” / not multipart-as-contract |
| HTTP resource (illustrative) | `POST /v1/rag/documents` (or equivalent **document creation** resource — not `/upload`) |
| Add only | **Yes** — **no delete** in Phase 9 (delete deferred: confirmation, active retrieval, rollback, permissions, races) |
| Remote drop behavior | **No auto-ingest** — drop/import → Local Note only; user clicks **[Send to Engine]** |
| Acceptance vs progress | **OperationResult** (Phase 7) for create acceptance · **SSE `INDEXING_*`** (Phase 5) for indexing — **never merge** |
| Atomicity | Engine guarantees: document **exists** or **doesn't** — never half-indexed in corpus model |
| Capability | **`supportsIngest`** via `/ready` — GUI enables Send when true; never infer from mode alone |
| Phase 8 correlation | Engine assigns stable **`id`**; GUI refreshes corpus list after acceptance; filename is display-only |
| Local mode | May retain `setRagFiles` path until unified create API on Local (implementation detail at lock) |
| Protocol doc | **Dedicated ingest/create-document protocol lock** recommended (separate from GUI phase lock) |

**Remote RAG tab UX (🔒 locked — builds on Phase 8 layout)**

```
Engine Corpus
--------------
GRAG.md          indexed · 42 chunks
...

▼ Local Notes
--------------
completed_improvements_log.md   (host-only)
[Send to Engine]                ← enabled when supportsIngest
```

- **[Send to Engine]** initiates **create corpus document** from the selected Local Note.  
- Status after click: Phase 7 one-outcome (accepted / failed) — not “indexing…” until SSE.  
- Progress strip / counters: **INDEXING_* events only** after acceptance.

**Scope**  
1. Dedicated **create-document protocol** plan + lock (prerequisite).  
2. Engine: create-document handler, atomic ingest pipeline, quotas/allowlists, `supportsIngest` on `/ready`.  
3. `IAgentBackend::createCorpusDocument(...)` → `OperationResult` (Local + Remote).  
4. MainFrame: **[Send to Engine]** on Local Notes; no remote auto-ingest on drop; refresh Phase 8 corpus on acceptance.  
5. Phase 7 + Phase 5 wiring: acceptance ≠ indexing progress.  
6. Unit/integration tests + docker smoke.

**Out of Scope (Phase 9)**  
- **Delete / replace** documents (future phase).  
- Mirroring entire host `agent_workspace/`; baking corpus into image.  
- Locking multipart or wire-specific upload mechanics in the protocol (implementation choice).  
- GUI choosing storage paths, filenames, or chunk parameters.

**Implementation Considerations**  
- Supersedes Plan L “no ingest” only via explicit protocol + phase lock. Keep **O1 engine ownership**.  
- Phase 1 Local Notes preserved — explicit Send avoids “where did my drops go?” and fake indexing.  
- Future sources (clipboard, URL, …) reuse the same **create corpus document** operation with different payload shapes — GUI still doesn't own storage.

**Potential Risks**  
- Users expect drop = ingest — mitigate with explicit button + honest status (Phase 7 acceptance, Phase 5 SSE).  
- Large payloads / embedding latency — quotas; atomic rollback on failure.  
- Race: accept then SSE gap — quiet progress during gap is correct (Phase 5 + Phase 6).

**Validation Strategy**  
- Local Note → **[Send to Engine]** → OperationResult success → SSE `INDEXING_*` → document appears in Engine Corpus with `indexed` status → retrieval query grounded.  
- Failed create → one OperationResult failure; no fabricated indexing.  
- Seed script path still works (regression).  
- No half-indexed document in corpus list after failed pipeline.

**Required Tests**  
- Protocol: create-document schema; atomicity; `supportsIngest` on `/ready`.  
- OperationResult acceptance path; SSE indexing path (separate tests).  
- GUI: Send enabled/disabled from capability; no auto-ingest on remote drop.  
- Integration: create → list (Phase 8) → retrieve.  
- Docker disk/permission checks.

**Completion Criteria**  
- Remote GUI can **explicitly** create an Engine corpus document from a Local Note.  
- **No** host-path `setRagFiles` in remote mode for new adds.  
- Acceptance (OperationResult) and indexing (SSE) are separate, tested, and honest.  
- Engine corpus reflects **Indexed** or **Failed** — never half-indexed.  
- **`supportsIngest`** drives GUI affordance.

**Dependencies**  
Phases 1 ✅ · 5 ✅ · 7 ✅ · 8 ✅ · Plan L (supersede via explicit lock).

**Expected User Experience**  
“I added a note locally, sent it to the Engine, got a clear accept/reject, then watched real indexing events — and the document showed up in Engine Corpus when ready.”

**STOP** — Phase 10 implemented. Do not auto-start Phase 11 without lock + implement approval.

---

### Phase 10 — Conversation / Session Authority API

**Status:** 🔒 Locked + ✅ implemented 2026-07-20

**Objective**  
Make the **Engine the authoritative owner of conversation state** so multi-turn remote chat is Engine-authoritative — not “GUI syncs history to Engine,” but **Engine maintains history; GUI displays and navigates it.**

Phase 9 established Engine ownership of **knowledge** (corpus). Phase 10 establishes Engine ownership of **conversation** — the second pillar of client/server truth.

**Normative principle (🔒 locked)**  

> **The Engine is the authoritative owner of conversation state. The GUI displays and navigates conversation state but never reconstructs or replaces it.**

**Architectural model (🔒 locked — Engine append, not GUI sync)**  

This is **not** “sync turns after each reply.” The GUI does not synchronize history. The Engine appends and owns it.

```
GUI
  ↓  send user turn (HTTP)
Engine
  ↓  append user turn to conversation store
  ↓  generate reply (internal)
  ↓  append assistant turn (internal — not a GUI-facing “replace”)
  ↓  return reply (+ optional session metadata)
GUI
  ↓  display reply (and refresh from Engine when needed)
```

**Session ownership (🔒 locked)**  

| Owner | Owns |
|-------|------|
| **Engine** | Conversation, context, memory, summarization, pruning, session identity |
| **GUI** | Scroll position, selected session tab, rendering, window layout, local UI chrome |

The GUI **never** replaces the Engine’s conversation. No `replace-conversation` API — that path is **forbidden** (dangerous: races, dual truth, accidental wipe).

**Current State**  
`RemoteAgentBackend::setConversationMemory` is a remote no-op. Host `chat_sessions.json` is treated as UI persistence but effectively acts as a second source of truth. Restart GUI → history may appear “lost” even when Engine could hold continuity. Failed sends may leave optimistic bubbles inconsistent with Engine acceptance (Phase 7 partially addresses operation honesty; conversation commit model not yet locked).

**Desired End State**  

```
GUI (display + navigation only)
        │
        │  create session / append user turn / get conversation / get summary
        ▼
   RemoteBackend  ──HTTP──▶  Engine session store
        │                         │
        │                         ├─ append user turn
        │                         ├─ append assistant turn (internal)
        │                         ├─ summarize / prune (Engine policy)
        │                         └─ authoritative history
        │
        ▼
   Display bubbles from Engine-fetched state (or from append response + refresh)
```

**API surface (illustrative — resource-oriented, lock describes operations not wire details)**  

| Operation | Role | GUI? |
|-----------|------|------|
| **Create session** | Engine assigns `session_id` | Yes — new chat |
| **Append user turn** | Engine accepts and stores user message; triggers reply pipeline | Yes — send message |
| **Append assistant turn** | Engine stores model reply after generation | **Internal** (Engine only) |
| **Get conversation** | Recent messages for display (pagination TBD) | Yes — load / refresh / restart |
| **Get summary** | Engine-maintained rolling summary for long context | Yes — optional display / debug |

**Explicitly out of API contract:** `replace-conversation`, bulk overwrite, GUI-supplied full transcript as source of truth.

**Restart scenario (🔒 locked UX)**  

Today: restart GUI → “lost history?”  

Locked model:

```
Restart GUI
  ↓
Connect (session_id from UI chrome or user selection)
  ↓
Get conversation (+ summary if needed)
  ↓
Display
```

The GUI does **not** restore conversation truth from disk. The **Engine** does. `chat_sessions.json` eventually becomes a **cache** (titles, last-selected id, optional offline chrome) — **not** authoritative conversation truth in Engine mode.

**Failure model (🔒 locked — aligns with Phase 7)**  

```
Send message
  ↓
HTTP fails (or Engine rejects)
  ↓
User message NEVER appears as committed
  ↓
UI: "Failed to send." (OperationResult / Phase 7 one-outcome)
```

No optimistic assistant bubble. No “sync failed silently.” Engine never accepted the turn → GUI must not show it as part of authoritative history.

**Product choices (🔒 locked 2026-07-20)**  

| Choice | Decision |
|--------|----------|
| Authority | **Engine owns conversation** — GUI display/navigation only |
| Model | **Append-only** user turns via Engine; assistant turns Engine-internal |
| Forbidden | **`replace-conversation`** and any GUI-driven full transcript overwrite |
| Session id | **Engine-created** on create-session; GUI stores id as chrome |
| Host JSON | `chat_sessions.json` → **cache**, not truth (Engine mode) |
| Send failure | **Uncommitted** user turn — honest failure, no phantom bubbles |
| Restart | **Get conversation** from Engine — not replay from host file |
| Local mode | May retain in-process path until unified session API (implementation detail at lock) |
| Capability | **`supportsConversation`** (or equivalent) via `/ready` — GUI gates remote session APIs |

**Scope**  
1. Dedicated **conversation/session protocol** plan + lock (prerequisite — separate doc recommended).  
2. Engine: session store, create/append/get endpoints, internal assistant append, summarization hooks.  
3. `RemoteAgentBackend`: create session, append turn, get conversation/summary; **`supportsConversation`** from `/ready`.  
4. MainFrame: remote send path uses append API; load session from Engine on activate/restart; demote `chat_sessions.json` to cache in Engine mode.  
5. Phase 7 wiring: failed append → OperationResult failure, no committed bubble.  
6. Unit/integration tests + docker smoke item 16.

**Out of Scope (Phase 10)**  
- Full cognate / trajectory sync (Phase 11).  
- GUI-side summarization or pruning policy.  
- Dual-write Engine + host JSON as co-equal truths.  
- Replace/overwrite conversation APIs.

**Implementation Considerations**  
- Idempotency keys for append (retry-safe sends).  
- Max history / pagination for `get conversation`.  
- PII and retention — Engine policy, not GUI.  
- Local backend may keep `setConversationMemory` until unified API — do not break Local regression.  
- Phase 9 pattern: capability via `/ready`, OperationResult for acceptance, SSE/events for long-running work if any.

**Potential Risks**  
- Large transcripts on every open — mitigate with pagination + summary endpoint.  
- Session id collision / stale cache — Engine is source; GUI refreshes on mismatch.  
- Users expect host JSON to “be the chat” — mitigate with honest Engine mode labeling during transition.

**Validation Strategy**  
- Multi-turn remote chat → Engine store contains all turns.  
- Restart GUI → same `session_id` → **Get conversation** restores bubbles.  
- Failed send → no committed user bubble; Phase 7 failure copy.  
- Local mode regression unchanged.  
- `chat_sessions.json` in Engine mode: cache-only behavior verified.

**Required Tests**  
- Protocol: create session, append user turn, internal assistant append, get conversation, get summary; no replace endpoint.  
- OperationResult on append failure; no optimistic commit.  
- Restart/load integration test with live Engine.  
- Capability matrix: `supportsConversation` from `/ready`.  
- Local regression suite.

**Completion Criteria**  
- Documented session authority model; Engine owns conversation in remote mode.  
- GUI never reconstructs or replaces Engine conversation.  
- Restart displays Engine history via get APIs.  
- Failed sends stay uncommitted with honest UX.  
- `chat_sessions.json` documented and implemented as cache (Engine mode).

**Dependencies**  
Phase 7 ✅ · Phase 9 ✅ (ownership pattern precedent).

**Expected User Experience**  
“I restarted the GUI, opened my chat, and the Engine still had the full conversation. When a message failed to send, it didn’t pretend it went through.”

**STOP** — Phase 10 implemented. Do not auto-start Phase 11 implementation without explicit approval.

---

### Phase 11 — Research Resource Read APIs (Strategies, Trajectories, Episodes)

**Status:** 🔒 Locked + ✅ implemented 2026-07-21

**Objective**  
Expose Engine **research resources** over HTTP so research panels display real Engine learning state — not debugging endpoints, not SQLite concepts, not silent empty stubs.

**Normative principle (🔒 locked)**  

> **The Engine owns research knowledge. The GUI presents research knowledge through stable, versioned resources and never infers research state from missing or local data.**

**Cache rule (🔒 locked)**  

> **The GUI never caches research resources as an authoritative source. Refreshes always originate from the Engine.**

**Current State**  
`RemoteAgentBackend::getStrategies()`, `getTrajectories()`, `getEpisodeSteps()` return empty arrays (Plan K intentional). Panels show Empty-like UX when the issue is missing API or failed fetch.

**Desired End State**  

```
GUI (display only — no authoritative research cache)
        │
        │  getStrategies / getTrajectories / getEpisodes
        ▼
   RemoteBackend  ──HTTP──▶  Engine research resource store
        │                         │
        │                         ├─ collection envelope (schema_version, items, next_page, total_items)
        │                         ├─ immutable strategy_id / trajectory_id / episode_id
        │                         └─ storage implementation hidden (SQLite, memory, etc.)
        ▼
   StrategyPanel · TrajectoryViewer · Episodes
        │
        ▼
   Loading | Populated | Empty | Error  (D14 — when capability supported)
```

**Collection envelope (🔒 locked — mandatory, all three endpoints)**  

| Field | Rule |
|-------|------|
| `schema_version` | Versions **client JSON representation** — independent of internal storage schemas |
| `items` | Required array; empty on success → **Empty** |
| `next_page` | Opaque continuation token forever; GUI must not derive meaning from contents; `null` when exhausted |
| `total_items` | Required when cheap; `null` OK if expensive |

Illustrative HTTP: `GET /v1/research/strategies`, `/trajectories`, `/episodes` with shared paging query params.

**Immutable identifiers (🔒 locked)**  
`strategy_id`, `trajectory_id`, `episode_id` on every item — Engine-assigned, opaque, immutable for URLs, exports, bookmarks, comparisons, and future drill-down.

**Naming (🔒 locked)**  
Rename **`getEpisodeSteps()` → `getEpisodes()`** across backend surface. Engine may use steps internally; GUI resource is Episodes.

**Capability flags (🔒 locked)**  
**`supportsStrategies`**, **`supportsTrajectories`**, **`supportsEpisodes`** from `/ready` — no `supportsCognate`. Partial enablement allowed. When false → **Unavailable**; when true → D14 four states only.

**Presentation (🔒 locked — D14)**  

| State | When |
|-------|------|
| **Loading** | Fetch in flight |
| **Populated** | Success, items non-empty |
| **Empty** | Success, items empty — real zero |
| **Error** | Fetch failed — never masquerade as Empty |

**Scope**  
1. `research_resources.h` — collection envelope + validation helpers.  
2. Engine HTTP — three read-only collection endpoints + `/ready` tokens.  
3. Engine runtime/plugin — assemble from internal stores (hidden).  
4. `RemoteAgentBackend` — real HTTP; map failures to Error (not `[]`).  
5. Rename `getEpisodeSteps` → `getEpisodes`; wire panels to four-state rule.  
6. No authoritative GUI cache of research items.  
7. Tests + docker smoke item 17.

**Out of Scope (Phase 11)**  
Experiment Lab write/run · graph stats (Phase 12A) · cognate write/sync · E2 coupling · Export/Compare/Open (ids are prerequisite).

**Implementation Considerations**  
Pagination mandatory in contract shape; do not couple to E2 STRICT scoring; item fields beyond ids are versioned under `schema_version`.

**Potential Risks**  
Large payloads (mitigated by paging shape); schema drift between client representation and storage (mitigated by separating `schema_version` from storage migrations).

**Validation Strategy**  
Goals that store research rows → Remote **Populated** · success + empty items → **Empty** · transport/HTTP/schema failure → **Error** · partial `/ready` → enabled panels D14; disabled **Unavailable** · refresh re-fetches from Engine.

**Required Tests**  
Collection envelope validation · HTTP integration · panel disposition (no silent empty) · capability matrix from `/ready` · Local regression · eval harness unaffected.

**Completion Criteria**  
Research panels show Engine data or explicit **Error** — never silent empty success when fetch failed. GUI does not cache research as truth. `getEpisodes()` naming live.

**Dependencies**  
Phase 2 ✅ · Phase 7 ✅ (if reused) · `/ready` capability discovery. **Not** Phase 10.

**Expected User Experience**  
"I ran goals on the Engine; Strategies showed real research data — or a clear error. Empty meant the Engine truly had nothing. Refresh always came from the Engine."

**STOP** — Phase 11 implemented. Do not auto-start Phase 12A implementation without explicit approval.

---

### Phase 12A — Graph Statistics Resource API

**Status:** 🔒 Locked + ✅ implemented 2026-07-21

**Objective**  
Expose Engine-owned **graph statistics** as a stable, versioned **singleton resource** so the Graph panel displays real Engine graph state in remote mode — not silent `{}`, not local SQLite inference, not host storage as truth.

**Normative principle (🔒 locked)**  

> **The Engine owns graph state and graph statistics. The GUI presents graph resources through stable APIs and never derives graph state from local storage or missing data.**

**Cache rule (🔒 locked)**  

> **The GUI never caches graph statistics as an authoritative source. Refreshes always originate from the Engine.**

**Resource model (🔒 locked)**  
Graph statistics are an **Engine-owned singleton resource** — **not** a collection. Unlike Phase 11:

| Phase 11 (collections) | Phase 12A (singleton) |
|--------------------------|------------------------|
| Collection envelope (`items`, `next_page`, …) | Single resource object |
| Pagination mandatory | No pagination |
| Multiple immutable ids per item | One stats snapshot per fetch |

The GUI calls **`getGraphStats()`**. The Engine decides whether stats come from SQLite graph memory, in-memory graph state, or another backend — **storage is never part of the contract**.

Illustrative response shape (resource-oriented — exact stat fields lockable at implement):

| Field (illustrative) | Role |
|----------------------|------|
| `schema_version` | Versions **client JSON representation** — independent of internal storage schemas |
| `generated_at` | **Engine-assigned** timestamp representing when the statistics snapshot was **produced** — not when the GUI received it |
| `session_id` (optional, advisory) | May be included when graph statistics are session-specific. The GUI treats it as **informational only** — contract remains valid for global graph metrics |
| Statistics fields | Engine-defined counts / density / relational metrics — **not** table or column names |

Do **not** expose SQLite tables, node/edge storage layout, or filesystem paths in the API.

**Current State**  
`RemoteAgentBackend::getGraphStats()` returns `{}` (Plan K intentional stub). `GraphPanel` receives an empty object in Engine mode and may present **Empty-like** UX when the issue is missing API or failed fetch. Local mode works via `BasicAgentPlugin::getGraphStatistics()` → `Memory::GraphStatistics`.

**Desired End State**  

```
GUI (GraphPanel — display only, no authoritative cache)
        │
        │  getGraphStats()
        ▼
   RemoteBackend  ──HTTP──▶  Engine graph statistics resource
        │                         │
        │                         ├─ singleton JSON (schema_version, generated_at, …)
        │                         └─ storage implementation hidden
        ▼
   Loading | Populated | Empty | Error  (when capability supported)
```

Illustrative HTTP: `GET /v1/graph/stats`.

**Capability (🔒 locked)**  
**`supportsGraphStats`** from `/ready`. When false → **Unavailable**. When true after implement → four presentation states only.

**Presentation (🔒 locked — D14 singleton extension)**  
When **`supportsGraphStats`** is true, `GraphPanel` uses exactly:

| State | When |
|-------|------|
| **Loading** | Fetch in flight |
| **Populated** | Success; valid resource with meaningful graph data |
| **Empty** | Success; valid resource with **no meaningful graph data** for the active context (see Empty definition below) |
| **Error** | Fetch failed or schema invalid — never masquerade as Empty |

**Empty (singleton — 🔒 locked):** **Empty** indicates the Engine successfully returned a valid graph statistics resource that contains **no meaningful graph data** for the active context. It is distinct from **Error** (fetch failed) and **Unavailable** (capability absent). A bare `{}`, transport failure, or invalid payload must **not** be treated as Empty.

**Unavailable** applies only while the capability is absent.

**Scope**  
1. Graph statistics contract helpers (pure header).  
2. Engine HTTP **GET** singleton handler + `/ready` capability token.  
3. Engine runtime/plugin — assemble from internal graph store (hidden).  
4. `RemoteAgentBackend` — real HTTP fetch; failures → fetch-error sentinel (not `{}`).  
5. `MainFrame` / `GraphPanel` — four-state refresh; no authoritative GUI cache.  
6. Tests + docker smoke item **18**.

**Out of Scope (Phase 12A)**  
- Experiment Lab ( **Phase 12B** ).  
- Graph **mutation** from GUI.  
- Collection envelope / pagination.  
- Full benchmark orchestration (Phase 13).  
- E2 / eval authority changes.

**Implementation Considerations**  
- `generated_at` is Engine-authoritative for staleness display.  
- Optional `session_id` must not drive GUI reconstruction logic.  
- Local backend returns the same resource shape as Remote for regression parity.

**Potential Risks**  
- Empty vs Error ambiguity — mitigated by locked Empty definition above.  
- Schema drift — mitigated by `schema_version` independent of storage migrations.

**Validation Strategy**  
- Graph-updating goal → Remote **Populated**.  
- Valid resource, no meaningful data → **Empty**.  
- Fetch/schema failure → **Error**.  
- No capability → **Unavailable**.  
- Refresh re-fetches from Engine.

**Required Tests**  
- Singleton contract + fetch-error sentinel.  
- HTTP + `/ready` capability.  
- GUI disposition (Empty ≠ Error ≠ silent `{}`).  
- Local regression · eval harness unaffected.

**Completion Criteria (🔒 locked)**  
- The Graph panel displays **only Engine-authored graph statistics** or an explicit presentation state (**Loading**, **Populated**, **Empty**, **Error**, or **Unavailable**). It **never derives or reconstructs graph statistics locally**.  
- GUI does not cache graph stats as authoritative truth.  
- Storage implementation not exposed in contract.  
- `supportsGraphStats` gates Remote path correctly.

**Dependencies**  
Phase 2 ✅ · Phase 7 ✅ (if reused) · Phase 11 ✅ · `/ready`. **Not** Phase 12B.

**Expected User Experience**  
"I ran a goal that used graph memory; the Graph tab showed Engine statistics — or a clear error. Empty meant the Engine truly had no meaningful graph data, not that the API failed."

**STOP** — Phase 12A implemented. Do not auto-start Phase 12B without its own Analyze → Refine → Lock cycle.

---

### Phase 12B — Experiment Lab Strategy

**Status:** Placeholder — **not planned in detail**

**Reserved scope:** Experiment Lab surfaces (experiments list, save/run policy, host-lab vs Engine-backed honesty, benchmark button labeling). Split from the former combined Phase 12 so graph statistics can ship independently.

**Workflow:** Phase 12B will receive its own **Analyze → Refine → Lock → Implement** cycle **after Phase 12A is completed**. No APIs, product decisions, or implementation details are defined here.

**Independence (🔒 locked):** **No architectural assumptions made in Phase 12A shall constrain the future design of Phase 12B.**

**STOP** — Do not implement or lock Phase 12B until Phase 12A is complete and 12B analysis begins.

---

### Phase 13 — Benchmarks Mode Policy

**Status:** Draft  

**Objective**  
Define and implement a clear policy: host benchmarks remain host tooling, or Engine gains a job API — but GUI must not imply remote Engine execution when running host binaries.

**Current State**  
Benchmarks menu spawns host processes against host tree.

**Desired End State**  
Locked policy + UI labels. If host-only: menu section “Host benchmarks (local process).” If Engine jobs: new API + progress via events.

**Scope**  
- Policy lock.  
- UI labeling/disable rules.  
- Optional Engine job API (only if policy chooses it).

**Out of Scope**  
Changing scientific claim rules for papers/evals.

**Implementation Considerations**  
Eval integrity: do not silently mix host and container environments in one “Run” button.

**Potential Risks**  
Confusing CI vs GUI paths — document env fingerprint display.

**Validation Strategy**  
Manual menu inspection remote vs local; optional docker job test if API exists.

**Required Tests**  
- Policy doc + GUI checklist.  
- If API: integration tests.

**Completion Criteria**  
- User cannot mistake a host binary run for Compose Engine cognition.

**Dependencies**  
Phase 2.

**Expected User Experience**  
Clear separation of measurement tooling vs live Engine.

**STOP**

---

### Phase 14 — Resume / Crash Recovery over HTTP

**Status:** Draft  

**Objective**  
Restore resumability for remote goals via Engine APIs.

**Current State**  
`checkResumablePlan` no-op remotely; Engine may still persist plans in `/workspace`.

**Desired End State**  
`GET /v1/goals/resumable` + `POST /v1/goals/resume` (names TBD). GUI offers resume when Engine reports a resumable plan.

**Scope**  
- Engine endpoints wrapping existing resume primitives.  
- GUI Agent menu wiring for remote.

**Out of Scope**  
Changing executive persistence format.

**Implementation Considerations**  
Session_id correlation; safety confirmation on resume.

**Potential Risks**  
Resuming wrong session — require explicit plan_id.

**Validation Strategy**  
Start goal, kill GUI, reopen, resume from Engine state.

**Required Tests**  
- Engine tests for resumable detection.  
- Manual remote crash/resume.  
- Local resume regression.

**Completion Criteria**  
- Remote resume works end-to-end without host memory.db.

**Dependencies**  
Phases 7, 10 helpful.

**Expected User Experience**  
Crash-resilient remote research sessions.

**STOP**

---

### Phase 15 — GRAG Diagnostics Completeness

**Status:** Draft  

**Objective**  
Ensure retrieval diagnostics in remote mode always reflect Engine retrieval outcomes, including grounded vs below_threshold vs greeting_skip vs no_index — without GUI inference.

**Current State**  
SSE `RETRIEVAL_DIAGNOSTICS` works when delivered; grounding floor can empty injectable context while candidates existed; UI may look like “not chunking.”

**Desired End State**  
Diagnostics panel shows Engine fields: `scoring_type`, alpha/magnitude honesty (Plan N5), candidate counts, grounding reason, corpus identity. Optional “Explain retrieval” pulls last Engine diagnostic snapshot via API if SSE missed.

**Scope**  
- Confirm SSE payload completeness.  
- Optional `GET /v1/retrieval/last_diagnostics`.  
- GUI labels for grounding_decision_reason.

**Out of Scope**  
Changing GRAG math weights.

**Implementation Considerations**  
Preserve Plan N5 display helpers; extend rather than invent percent scores.

**Potential Risks**  
Panel clutter — keep advanced fields collapsible.

**Validation Strategy**  
Queries: greeting skip; seeded GRAG question; below_threshold query; post-ingest hit.

**Required Tests**  
- Existing Plan N5 unit tests remain green.  
- Manual remote matrix.  
- Optional protocol note if scientific retrieval claims involved ( ordinarily not).

**Completion Criteria**  
- Users can distinguish “no index,” “skipped,” “retrieved but rejected,” and “injected.”

**Dependencies**  
Phases 5–6, 8–9 for full corpus stories.

**Expected User Experience**  
Diagnostics explain RAG behavior instead of implying UI failure.

**STOP**

---

### Phase 16 — Thin-Client Hardening Audit

**Status:** Draft  

**Objective**  
Repo-wide audit that remote GUI paths cannot touch host cognitive stores or local plugin cognition.

**Current State**  
Incremental fixes across earlier phases.

**Desired End State**  
Checklist + automated guards (tests or lint-style ripgrep CI) preventing regressions: no remote `setRagFiles` success path; no host decision_trace reads; no local plugin calls from RemoteAgentBackend.

**Scope**  
- Audit document update in this file.  
- CI check scripts or unit tests.  
- Fix any stragglers found.

**Out of Scope**  
Removing LocalAgentBackend.

**Implementation Considerations**  
Keep local mode fully functional.

**Potential Risks**  
False-positive CI on legitimate host UI transcript files (`chat_sessions.json` OK).

**Validation Strategy**  
CI job on PR; manual remote soak.

**Required Tests**  
- Automated ownership guards.  
- Full `ctest -L pr` + compose smoke.

**Completion Criteria**  
- Audit signed off; CI guard merged.

**Dependencies**  
Phases 1–15 as applicable.

**Expected User Experience**  
Stable thin client.

**STOP**

---

### Phase 17 — Local Mode Policy (Retain, Demote, or Retire)

**Status:** Draft  

**Objective**  
Decide the long-term fate of in-process LocalAgentBackend now that remote thin-client is complete.

**Current State**  
Local is default and fully cognitive; remote is optional.

**Desired End State**  
Locked policy:

- **Retain** local for dev/tests, or  
- **Demote** local to explicit `THOTH_LOCAL=1`, or  
- **Retire** local from GUI (engine-only).

Document developer workflows accordingly.

**Scope**  
- Policy + docs (`GETTING_STARTED`, AGENTS).  
- Selection logic changes if demote/retire.

**Out of Scope**  
Deleting core library; headless engine remains.

**Implementation Considerations**  
Test suite may still need in-process plugin without GUI.

**Potential Risks**  
Breaking developer inner loop — require owner approval.

**Validation Strategy**  
Per chosen policy: smoke local and/or remote.

**Required Tests**  
- Selection unit tests.  
- Docs review.

**Completion Criteria**  
- Written policy locked and reflected in startup logs/docs.

**Dependencies**  
Phase 16.

**Expected User Experience**  
Clear default path for research console users.

**STOP**

---

### Phase 18 — Roadmap Closeout

**Status:** Draft  

**Objective**  
Declare GUI thin-client transition complete against the long-term goals; update `docker_roadmap.md` / `completed_improvements_log.md` pointers; list residual known gaps (if any) as new backlog items — not silent debt.

**Current State**  
This document is the living roadmap.

**Desired End State**  
Closeout note: GUI presentation-only in remote mode; Engine sole cognitive authority; residual items explicitly deferred or filed.

**Scope**  
- Documentation closeout only (plus tiny doc links).  
- Verification matrix run once end-to-end.

**Out of Scope**  
New features.

**Implementation Considerations**  
Do not claim paper/eval metrics from GUI integration work.

**Potential Risks**  
Premature closeout — require human sign-off against checklist below.

**Validation Strategy**  
End-to-end remote checklist:

1. Mode banner remote.  
2. Corpus list = Engine.  
3. Ingest (if Phase 9 shipped) indexes via events.  
4. Chat grounded on Engine docs.  
5. Goal SSE drives plan strip.  
6. GRAG diagnostics from Engine.  
7. Traces/logs from Engine.  
8. Cognate panels from Engine or labeled N/A.  
9. D11: no local-artifact cognitive diagnostics while Engine authoritative.  
10. No fabricated indexing.

**Required Tests**  
- Full manual checklist.  
- Compose smoke + `ctest -L pr`.  
- Optional scientific note: GUI integration is not a retrieval quality claim.

**Completion Criteria**  
- Human approves closeout.  
- Roadmap marked Complete with date.  
- Residual backlog recorded.

**Dependencies**  
All prior accepted phases for the chosen scope subset.

**Expected User Experience**  
Desktop GUI is a true thin client over the containerized Engine.

**STOP**

---

## Suggested Sequencing (summary)

| Order | Phase | Theme |
|------:|-------|--------|
| 0 | Inventory freeze | Docs — 🔒 2026-07-19 |
| 1 | Stop fake indexing | Honesty — 🔒 + ✅ 2026-07-19 |
| 2 | Mode banner / capabilities / panel states | Honesty — 🔒 + ✅ 2026-07-19 |
| 3 | Backend authority for cognitive diagnostics (D11) | Honesty — 🔒 + ✅ 2026-07-19 |
| 4 | Decision summary resource (Explain Plan only) | Engine read — 🔒 + ✅ 2026-07-19 |
| 5 | Progress reporting discipline (backend signals only) | Honesty — 🔒 2026-07-19 |
| 6 | SSE reconnect | Reliability |
| 7 | Control error honesty | Honesty |
| 8 | Corpus listing | Engine read |
| 9 | Ingest + indexing | Engine write |
| 10 | Conversation sync | Engine memory |
| 11 | Cognate reads | Engine read |
| 12 | Graph / experiments | Engine read |
| 13 | Benchmarks policy | Clarity |
| 14 | Resume HTTP | Continuity |
| 15 | GRAG diagnostics UX | Clarity |
| 16 | Hardening audit | Guardrails |
| 17 | Local mode policy | Product |
| 18 | Closeout | Docs |

Phases **1–3, 5, 7** are **honesty-first**: they fix the user’s understanding of the system before adding capabilities, which makes debugging Engine behavior tractable.  
Phases **4, 6, 8–15** require additive Engine contracts and individual locks.  
Phase **9** specifically requires a new ingest protocol that supersedes Plan L’s deferral **only when explicitly locked**.

**Lock order:** Phase **0** first (ownership freeze), then honesty phases, then API phases.

---

## Appendix A — Capability Matrix (Phase 0 frozen)

| UI surface | Local | Remote today | Remote honesty class | Remote target | Phase |
|------------|-------|--------------|----------------------|---------------|-------|
| Chat | Plugin | `/v1/chat` | Engine-backed | `/v1/chat` | — |
| Goals / control | Plugin | HTTP | Engine-backed | HTTP | — / 7 |
| Executive state strip (plan/steps) | Plugin events | SSE | Engine-backed (when SSE live) | SSE + reconnect | 5–6 |
| GRAG Diagnostics | Plugin events | SSE | Engine-backed (when SSE live) | SSE + optional poll | 5–6, 15 |
| RAG Files tab | Host index | Host slots + fake indexing | **Host-only** (misleading) | Engine corpus + ingest; slots notes until then | **1**, 8, 9 |
| Explain Plan | Host `decision_trace` → Phase 3 Unavailable | Host files | Wrong source | Structured explain resource (4) | 3–4 |
| Logs tab | Host trace tail → Phase 3 Unavailable | Host files | Wrong source | Activity resource or later | 3–4 |
| Strategies panel | DB | Empty `[]` | **Unavailable** (looks Empty) | Cognate HTTP | 2, 11 |
| Trajectories tab | DB | Empty `[]` | **Unavailable** | Cognate HTTP | 2, 11 |
| Experiments tab | Partial | Inert / empty | **Unavailable** / host-lab | Policy + optional API | 2, 12 |
| Graph tab (stats) | Plugin | Empty `{}` | **Unavailable** | Graph API | 2, 12 |
| Graph tab (SSE highlight) | Events | SSE | Engine-backed | SSE | 5–6 |
| Benchmarks menu | Host binaries | Host binaries | **Host-only** (must label) | Labeled / Engine jobs | 2, 13 |
| Resume / crash recovery | Plugin | No-op | **Unavailable** | Resume HTTP | 14 |
| Memory sync | Plugin | Skip | **Unavailable** | Sync API | 10 |
| Mode / backend indicator | Implicit | Brief status only | Incomplete | Persistent banner | 2 |
| Window layout / tabs | GUI | GUI | UI state (GUI owns) | GUI owns | — |
| Chat session list / bubbles | Host JSON | Host JSON | UI chrome (not Engine memory) | UI chrome ± sync | 10 |

---

## Appendix B — Non-Goals (reminders)

- GUI must not become a second indexer.  
- GUI must not bind-mount host `memory.db` into Compose by default.  
- GUI must not treat empty cognate JSON as scientific evidence of “no learning.”  
- GUI must not reimplement Engine computations (D5).  
- This roadmap does not itself change GRAG weights, E2 STRICT, or paper claims.

---

## Appendix C — Document Control

| Field | Value |
|-------|-------|
| Phase 0 | 🔒 Locked 2026-07-19 |
| Phase 1 | 🔒 Locked + ✅ implemented 2026-07-19 (Option A) |
| Phase 2 | 🔒 Locked + ✅ implemented 2026-07-19 |
| Phase 3 | 🔒 Locked + ✅ implemented 2026-07-19 (D11) |
| Phase 4 | 🔒 Locked + ✅ implemented 2026-07-19 (Explain-only) |
| Phase 5 | 🔒 Locked + ✅ implemented 2026-07-19 (D3a) |
| Phase 6 | 🔒 Locked + ✅ implemented 2026-07-20 (SSE reconnect) |
| Phase 7 | 🔒 Locked + ✅ implemented 2026-07-20 (operation result honesty) |
| Phase 8 | 🔒 Locked + ✅ implemented 2026-07-20 (Engine corpus listing) |
| Phase 9 | 🔒 Locked + ✅ implemented 2026-07-20 (corpus document creation) |
| Phase 10 | 🔒 Locked + ✅ implemented 2026-07-20 (conversation/session authority) |
| Phase 11 | 🔒 Locked + ✅ implemented 2026-07-21 (research resource read APIs) |
| Phase 12A | 🔒 Locked + ✅ implemented 2026-07-21 (graph statistics singleton resource) |
| Phase 12B | Placeholder — experiment lab strategy (lock after 12A complete) |
| Phase 13+ | Draft until individually locked (numbers unchanged) |
| Later phases | Draft until individually locked |
| Implementation | **Forbidden** until the specific phase is locked and approved |
| Owners | Human architect + implementing agent per phase |
| Update rule | Append phase status markers when phases lock/complete; do not rewrite Phase 0 history |
| Normative rules | D0 Single Source of Truth · GUI may/may-not · D5 No Duplicate Logic · D10 No capability inference · D11 Backend authority for cognitive diagnostics · D12 Engine produces diagnostics · D13 Engine owns diagnostic presentation model · **D14 Engine-backed collection presentation (Loading/Populated/Empty/Error)** · D3/D3a Progress from backend signals only · Phase 6 connection state machine + connection≠engine health + stale-live invariant · Phase 7 one-outcome rule + OperationResult + UI severity + Phase 6 correlation · Unavailable vs Empty/Loading · Capability-driven UI (Phase 2) |

---

## Appendix D — Quick Ownership Table (Phase 0 frozen)

Use this table when answering **“Who owns this state?”** before writing code. Keep scannable; expand only via explicit lock revision.

| Component | Owner | GUI Role |
|-----------|-------|----------|
| Executive | Engine | Display |
| Planner | Engine | Display / Request (start, revise via Engine APIs) |
| Memory (episodic / conversation / cognate) | Engine | Display / Request (research resources when API exists) |
| Research resources (strategies / trajectories / episodes) | Engine | Display / Request — never cache as authoritative truth (D14) |
| Graph statistics | Engine | Display / Request — singleton resource; never cache as authoritative truth |
| GRAG / retrieval | Engine | Display |
| Grounding decisions | Engine | Display |
| Chunker | Engine | None |
| Embeddings | Engine | None |
| Indexing | Engine | Display (progress from Engine events only) |
| Document corpus on disk | Engine | Display / Request (list, ingest) |
| Diagnostics (scores, breakdowns, routing) | Engine | Display (consume Engine results; never recompute) |
| Events / observability stream | Engine | Subscribe |
| File upload / ingest | GUI → Engine | Request only |
| Session cognitive state | Engine | Display |
| UI session chrome (titles, selected tab, layout) | GUI | Own |
| UI transcript cache (`chat_sessions.json` in Engine mode) | GUI (cache) | Own chrome; **Engine owns conversation truth** — refresh via get APIs |
| Conversation / session history | Engine | Display / Request (append turn, get conversation) — **never replace** |
| Window layout / splitters / tabs | GUI | Own |
| Benchmarks (until policy says otherwise) | Host tooling or Engine jobs per Phase 13 | Display / Request; never mislabel |

---

**Next human action:** Begin Phase 12B Analyze when ready. Do not auto-start Phase 12B lock or implement until Phase 12A is verified in production use.
