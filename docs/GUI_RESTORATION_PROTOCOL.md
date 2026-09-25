# GUI Functional Restoration Protocol

**Document type:** Focused restoration protocol (workflows first)  
**Status:** R1 🔒 · R1.5 🔒 ✅ · **R2 🔒 Verify ✅ 2026-07-22** · **R3 🔒 Implement ✅** (Verify 🔶) · **R4 🔒 Implement ✅** (Verify 🔶 2026-07-24) · **R5 🔒 Implement ✅ 2026-07-23** (Verify pending)  
**Created:** 2026-07-21  
**Refined:** 2026-07-21 (R1.5 complete; **R2 Engine Indexing Honesty** refined — ready for Lock R2)  
**Phase R1 locked:** 2026-07-21  
**Phase R1.5:** 🔒 Locked 2026-07-21 — Analyze ✅ · Implement ✅ · **Verify ✅ 2026-07-21**  
**Phase R2 (Engine indexing honesty):** 🔒 · Implemented ✅ · **Verify ✅ 2026-07-22**  
**Related:** [GUI_integration.md](GUI_integration.md) (architecture foundation — **paused** until restoration completes) · [AGENTS.md](../AGENTS.md) · [plan_l_workspace_corpus.md](plan_l_workspace_corpus.md) · [docker/README.md](../docker/README.md)

---

## Purpose

The [GUI Integration roadmap](GUI_integration.md) established Engine ownership, capabilities, and presentation honesty (Phases 0–12A). That work is the **foundation**, not the current priority.

**Current priority:** **R2 🔒 locked** — Engine indexing honesty. Implement only after explicit `AGENTS.md` approval.

### Final phase structure

| Phase | Goal | Touches | Complete when |
|-------|------|---------|----------------|
| **R1.5** ✅ | GUI works | GUI + existing APIs | User can ingest and see results |
| **R2** | Engine indexing **truth** | Engine event metadata + minimal GUI display | Success/failure no longer ambiguous |
| **R3+** | Goals, chat, audit, … | TBD per phase | Per phase lock |

---

## Scope correction (2026-07-21)

Two problems were incorrectly merged:

| | Problem A (active) | Problem B (deferred) |
|---|-------------------|----------------------|
| **Summary** | GUI remote RAG workflow broken or misleading | Engine does not expose enough indexing truth |
| **Phase** | **R1.5** Remote RAG GUI Stabilization | **R2** Engine Indexing Honesty (future) |

The **2026-07-21 R2 lock** (metadata + corpus `failed`) is **superseded** for implementation ordering until R1.5 Verify completes and R2 is **re-analyzed and re-locked**. Appendix B analyze findings are **input to future R2**, not authorization to implement R2 now.

---

## Relationship to GUI Integration

| Topic | Where it lives |
|-------|----------------|
| Who owns corpus, conversation, research state | `GUI_integration.md` Phase 0, Appendix D |
| Corpus list + create-document contracts | Phases 8–9 (implemented) |
| Progress from INDEXING_* only | Phase 5 (D3a) |
| OperationResult on create/send failures | Phase 7 |
| Remaining API phases (12B+, benchmarks policy, etc.) | **Paused** until this protocol completes |

Restoration work **must not** violate locked integration principles. It **may** fix wiring bugs, environment gaps, and UX that blocks end-to-end workflows.

---

## Mandatory workflow (every restoration phase)

```
Analyze → Refine → Lock → Implement → Verify → Human approval → Next phase
```

| Step | Rule |
|------|------|
| **Analyze** | Inspect repo + live symptoms; trace workflows; **no fixes** |
| **Refine** | Narrow scope; resolve open questions; update phase text |
| **Lock** | Human approves locked phase (explicit “Lock R*n*” or equivalent) |
| **Implement** | Exact locked scope only (`AGENTS.md` gate: “Implement” / “Proceed” / “Approved”) |
| **Verify** | Tests + manual checklist + docker smoke where applicable |
| **STOP** | No auto-start of the next phase |

**Do not assume** broken workflows share one root cause. Each phase starts with diagnosis.

---

## Restoration principles

1. **One workflow per phase** — ship a restorable end-to-end path before starting the next.
2. **Diagnosis before implementation** — document observations and hypotheses before proposing fixes.
3. **Engine truth in Engine mode** — GUI displays Engine corpus, indexing events, and operation outcomes; no fabricated progress.
4. **Separate infrastructure from wiring** — embedding/Ollama/seed failures are valid findings; label them explicitly in analyze output.
5. **Human approval** — each phase requires explicit lock and explicit implement approval per `AGENTS.md`.

### Restoration sequence (corrected 2026-07-21)

| Phase | Question |
|-------|----------|
| **R1** | GUI ownership semantics correct? (🔒 implemented) |
| **R1.5** | Does the GUI **drive** drop → send → progress → corpus correctly and **display Engine data faithfully** (not perfect indexing)? |
| **R2** | Does the Engine report **whether indexing finished successfully** (not retrieval quality)? |
| **R3+** | Goals, chat, retrieval, audit |

---

## Phase R1.5 — Remote RAG GUI Stabilization (Draft)

**Status:** 🔒 Locked 2026-07-21 · Analyze ✅ 2026-07-21 — **Implement pending**

### Purpose

Restore a **working GUI workflow** using **existing Engine behavior** (no new APIs, no new indexing status architecture).

**Primary question:** Does the GUI correctly execute the Phase 9 remote ingest workflow and show what the Engine actually returns?

### Success criteria (normative for R1.5 Verify)

R1.5 **passes** when:

1. The GUI **correctly drives** the existing workflow (DROP → Local Note → Send to Engine → POST → listen for INDEXING_* → refresh corpus list).
2. The GUI **accurately displays** the information **currently available** from the Engine (`/ready` capabilities, POST acceptance/errors, SSE events, `GET /v1/rag/corpus` fields) — without inventing state, hiding failures the API already reports, or implying success the corpus does not show.

R1.5 **does not** require perfect or truthful indexing semantics (e.g. COMPLETE ⇒ indexed, zero-chunk outcomes, embed degradation). Those gaps are **R2**; document them in the Analyze Report but **do not** fail R1.5 Verify solely because Engine outcomes are misleading.

### In scope

1. **Environment verification** — rebuilt engine image; `/ready`; `GET /v1/rag/corpus`; `POST /v1/rag/documents` (confirms the GUI has a real client target).
2. **End-to-end workflow trace** — DROP → Local Note → Send → POST → SSE START/COMPLETE → corpus refresh/display.
3. **GUI-only fixes** — remote/local assumptions, host-only labeling, corpus refresh, SSE handling, stale UI, invalid remote ops (e.g. `setRagFiles`), wrong endpoint assumptions, layout crashes, copy that claims more than Engine data supports.
4. **Verify against success criteria above** — workflow drivable; display faithful to current API/SSE/corpus payloads.

### Engine change rule (normative)

During R1.5 **do not modify**:

- HTTP response **contracts** (paths, status codes, JSON shapes the GUI already depends on)
- **SSE metadata** schemas
- Corpus **`status` enums** or list field semantics
- **`IndexManager`** behavior (indexing worker, chunking, persistence)

**Exception (narrow):** An Engine-side change is allowed **only** if Analyze proves the GUI call path is **broken** because the Engine violates an **already-shipped, GUI-integration contract** (e.g. route missing on current image, 404 on documented endpoint) — and the fix is the **minimal** restoration of that existing contract, not R2 honesty or new semantics. If the GUI works against the contract but outcomes are misleading, **document for R2**; do not change Engine in R1.5.

Default implementation surface: **GUI + bridge** (`MainFrame`, `AgentInterface`, `RemoteAgentBackend`, headers in `includes/`).

### Explicit non-goals

New indexing states, failure architecture, IndexManager/chunking/embed changes, GRAG/retrieval, new APIs, job tracking, observability/metrics, **and any Engine contract/indexing change outside the narrow exception above**. **Engine semantic gaps → document for R2; do not expand R1.5.**

### Deliverable

**R1.5 Analyze Report** (template in protocol handoff) → Lock R1.5 → Implement → Verify checklist.

### Dependencies

R1 code merged. Fresh `thoth-engine:local` + `THOTH_ENGINE_URL`.

### STOP gate

R2 **must not** start until R1.5 Verify approved.

### Human approval

~~Lock R1.5~~ ✅ 2026-07-21 · Analyze ✅ · **Implement** ✅ 2026-07-21

---

## Phase R1.5 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-21 |
| Kind | Remote RAG **GUI stabilization** — drive existing workflow; display **current** Engine data |
| Success criteria | GUI drives DROP→SEND→POST→INDEXING_*→corpus refresh; UI faithful to `/ready`, POST, SSE, corpus JSON — **not** perfect indexing semantics |
| Engine rule | **No** API/SSE enum/IndexManager changes unless **narrow exception** (broken shipped call path) |
| Analyze | ✅ 2026-07-21 — **Appendix C** |
| Implement | ✅ 2026-07-21 — neutral COMPLETE copy · basename slot match · Verify (automated + manual checklist) |
| Out of scope | R2 honesty · GRAG · new APIs · observability |
| Next | R2 re-lock when ready (Engine honesty); optional operator spot-check GUI drop→send |

---

## Phase R1 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | 2026-07-21 |
| Kind | GUI-only restoration — Remote RAG **truth** (Phase 9 presentation + wiring) |
| Normative | **R1 invariant** (DROP / SEND / INDEX / CORPUS) · `isRemote()` vs `supportsIngest` separation · mandatory `AgentInterface::setRagFiles` remote guard · `INDEXING_COMPLETED` → `RefreshCorpusPanel()` (immediate) |
| Helpers | `localNotesAreHostSideOnly(is_remote)` · `shouldSyncRagFilesToBackend(is_remote)` at call sites — **no** new drop-status helper |
| Primary test | `testGuiR1RemoteIngestHostOnlyPresentation` |
| Out of scope | Engine indexing honesty (R2) · multipart/delete/GRAG/embed · async corpus list · new RAG copy unless Verify requires |
| Post-lock rule | Do not expand R1 scope or revise invariant without a new lock; implement only after explicit “Implement” / “Proceed” / “Approved” |
| Implemented | 2026-07-21 |
| Next | R1.5 ✅ — see Phase R2 |

---

## Phase R2 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-21** (refined normative text; supersedes deferred 2026-07-21 lock) |
| Status | 🔒 Locked · Implemented ✅ · **Verify ✅ 2026-07-22** |
| Verify | A/C/SSE ✅ live Compose · `thoth-core-tests` ✅ · **D** not run (manual) |
| Next | **R3** Analyze → Lock (R2 STOP gate cleared for phase start) |
| Prerequisite | R1.5 Verify ✅ |
| Kind | **Engine Indexing Honesty** — not GUI ownership; **not retrieval quality** |
| Question | **Did indexing finish successfully?** (not: is retrieval good?) |
| Analyze | ✅ 2026-07-21 — **Appendix B**; Scenario **D** manual before R2 Verify |
| Ingestion API | **`POST /v1/rag/documents` unchanged** — **acceptance only**; no synchronous wait for indexing; no POST contract change |
| SSE | `INDEXING_STARTED` → `INDEXING_COMPLETED` only; **no** `INDEXING_FAILED` type |
| COMPLETE metadata (when armed) | `file_path`, `success` (bool), `chunk_count` (int), `reason` (string if `success=false`) |
| Success (Verify) | COMPLETE **`success: true`** and **existing corpus representation** confirms the document is **available** (today: terminal **`indexed`** with list fields already exposed — not a new retrieval-quality bar) |
| Failure (Verify) | COMPLETE **`success: false`** and corpus **`failed`** with **`reason`** when known |
| Corpus `status` | **Existing:** `indexed`, `pending` · **R2 adds only:** **`failed`** (minimum terminal failure for honesty). **No** new lifecycle states (`queued`, `processing`, `embedding`, `chunking`, `saving`, `retrying`, `indexing`, …) |
| Worker outcome rules (Engine internal) | 0 chunks stored → FAILED `no_chunks` · whitespace accepted then empty → FAILED `empty_document` · empty POST body → **400** (Phase 9) · TfIdf/embed fallback with chunks stored → assign **success: true** (no degraded tier) |
| Persistence | **Option A** — derive `failed` at worker completion; **no new DB**; **no job IDs** |
| Legacy COMPLETE (no `success`) | GUI: refresh corpus; neutral copy only (R1.5) |
| GUI scope | **Minimal — not a redesign:** on COMPLETE, **read metadata → display actual result** (do not assume success). Show corpus **`failed`** when present. **No** new ingest UX |
| Event timestamps | **Out of scope** unless timestamps **break the GUI** — if wrong, **document** (Appendix B); no timestamp/schema/observability expansion |
| Out of scope | Job system, polling POST, metrics dashboards, GRAG, retrieval scoring, embed model selection, TfIdf redesign, upload redesign, llama **501** fix (ops) |
| Primary tests | IndexManager outcome · corpus HTTP `failed` · COMPLETE metadata JSON |
| Post-lock rule | Do not expand R2 without new lock |
| Implemented | 2026-07-21 — IndexManager outcomes · COMPLETE metadata · corpus `failed` · GUI display |

---

## Phase R2 — Engine Indexing Honesty

**Status:** 🔒 **Locked 2026-07-21** · **Implemented ✅** · **Verify ✅ 2026-07-22**

### Objective

After **Send to Engine**, the operator can answer: **did indexing finish successfully?** — from **SSE COMPLETE metadata** and **`GET /v1/rag/corpus`**, without treating `INDEXING_COMPLETED` alone as success.

R2 does **not** judge retrieval quality, chunking strategy, or embed health beyond assigning success/failure at worker completion.

### R2 invariant (🔒 normative)

**`INDEXING_COMPLETED`:** indexing **worker finished** — not “document is usable.”

**Success:** COMPLETE metadata **`success: true`** and the **existing corpus representation** confirms the document is **available** (Phase 8 list model; today that means terminal **`indexed`** as already shown in the GUI — not a new “chunk_count policy” success gate for operators).

**Failure:** COMPLETE metadata **`success: false`** and corpus **`failed`** with **`reason`** when known.

**Ingestion API guard:** **`POST /v1/rag/documents` remains acceptance-only** (Phase 9). R2 must **not** make POST block until indexing completes or change acceptance JSON semantics.

### Corpus status (minimal addition)

| Status | Role |
|--------|------|
| `indexed` | **Existing** — document available per current list contract |
| `pending` | **Existing** — not yet terminal (R2 should eliminate “stuck pending” after worker ends by moving failures to `failed`) |
| `failed` | **R2 adds** — minimum terminal failure state for honesty |

No new ingestion lifecycle state machine.

### Locked `reason` vocabulary (closed set v1)

`no_chunks` · `empty_document` · `read_failed` · `sandbox_rejected` · `engine_unavailable`

(Extend only via new lock.)

### GUI boundary (minimal)

| Before R2 | After R2 |
|-----------|----------|
| `INDEXING_COMPLETED` → assume success | `INDEXING_COMPLETED` → **read metadata** → **display actual result** |
| Corpus refresh only | Also honor **`failed`** / `success` when present |

R1.5 already uses neutral **“Indexing finished:”** on COMPLETE; R2 adds outcome-aware display when metadata exists.

### Analyze summary (2026-07-21)

| Scenario | Result |
|----------|--------|
| **A** Normal md | POST accepted → START/COMPLETE (**metadata: `file_path` only today**) → corpus **indexed** when text sufficient |
| **B** llama stopped | POST accepted → corpus **indexed** + chunks via **TfIdf fallback**; embed errors in logs only |
| **C** Empty / ws | Empty → **400**; whitespace → **accepted** → COMPLETE → corpus **`pending` forever** |
| **D** Interrupt | **Not run** — required manual step before R2 Verify closeout |
| **Env** | Fresh `thoth-engine:local`; **200** on `/v1/rag/corpus` and POST documents |

Full detail: **Appendix B**.

### Implementation scope (high level — 🔒 locked)

**Engine:** `IndexManager` worker outcome; COMPLETE metadata; map terminal failures to corpus **`failed`**; optional structured log per file (not a metrics platform).

**Contracts:** `ENGINE_EVENTS.md` · `corpus_documents.h` validation for **`failed`**. **No** COMPLETE timestamp cleanup unless GUI is broken — otherwise document only.

**GUI (minimal):** `MainFrame` INDEXING handler + corpus lines — metadata + `failed` status.

**Tests:** IndexManager · corpus HTTP · event JSON; Phase 8/9 regressions.

### Out of scope (R2)

Synchronous POST, multipart/streaming redesign, delete/replace corpus, retry engine, GRAG, retrieval scoring, embedding model selection, TfIdf redesign, dashboards, goal lifecycle (R3), extra corpus statuses, SUCCESS_DEGRADED, event timestamp/schema/observability programs.

### Validation (R2 Verify)

- A–C on Compose with truthful SSE + corpus  
- Manual **D** (Appendix B)  
- Seeded corpus smoke unchanged  
- `thoth-core-tests` / docker smoke 14/15 green  
- **Success criterion:** operator can distinguish success vs failure without inferring from COMPLETE alone  

### Dependencies

R1 + R1.5 complete. Engine image with `corpus` + `ingest`.

### STOP gate

Do not start **R3** until R2 Verify approved.

### Human approval

1. ~~Lock R2~~ ✅ 2026-07-21 (refined)  
2. ~~Implement~~ ✅ 2026-07-21  
3. ~~R2 Verify~~ ✅ 2026-07-22 (A/C/SSE live; **D** not run — document before production hardening if desired)  

---

## Phase R1 — Remote RAG Truth Restoration (GUI)

**Status:** 🔒 Locked 2026-07-21 · ✅ Implemented 2026-07-21 — Verify pending

### Objective

Make the GUI **accurately represent** Phase 9 ingest architecture (DROP → Local Note → Send to Engine → Engine Corpus → indexing events). See **Phase R1 Lock Record** for normative scope.

### R1 invariant (🔒)

When **`isRemote()`** and **`supportsIngest`**: DROP = host Local Note only; SEND = content to Engine; INDEX = Engine only; CORPUS = read from Engine only.

Full locked plan: Phase R1 Lock Record above (implemented 2026-07-21).

---

## Phase R3 — Goal Lifecycle

**Status:** 🔒 **Locked 2026-07-23** · **Implemented ✅ 2026-07-23** · **Verify 🔶 partial 2026-07-23** (Engine `:8090` + unit tests; GUI checklist operator)

### Objective

The GUI always reflects the Engine’s **active goal**: set, display, completion, clearing, and session association.

### Phase R3 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-23** |
| Normative | **R3-G1** banner/cache vs Engine · **R3-G2** terminal clears banner · **R3-G3** `setSessionId` before goal POST · **R3-G4** Clear = host only + honest remote copy · **R3-G5** HTTP goal failure clears optimistic goal · **R3-G6** `ResolveGoalEventSessionId` (empty SSE `session_id` → active tab; documented legacy) · **R3-G7** local parity for sync + terminal clear |
| In scope | `MainFrame` goal submit/revise/menu/clear · terminal SSE · `HandleOperationComplete` `kOpGoal` |
| Out of scope | Phase 14 resume · scientific mode · new goal GET API · RetrievalScope / TCB |
| Primary test | `testGuiR3GoalSessionWire` |
| Verify | Manual checklist §R3.4 below |
| Post-lock rule | Do not expand R3 without new lock |

### R3-G1–G7 (normative)

| ID | Decision |
|----|----------|
| R3-G1 | Engine mode: goal banner from `SetSessionGoal` on submit and `PLAN_CREATED`/`REVISED` when `plan.goal` present; host `active_goal` is presentation cache |
| R3-G2 | On `PLAN_COMPLETED` / `PLAN_FAILED` / `PLAN_ABORTED` for resolved session → `ClearSessionGoal` (hide banner); status/plan panel retain terminal copy |
| R3-G3 | Before `executeGoal`, `SyncBackendSessionIdentity()` (`setSessionId(m_sessionId)`) |
| R3-G4 | Clear button: clear host goal; remote status cites Abort if Engine may still run; no false “Engine cleared” |
| R3-G5 | Failed `kOpGoal` in `HandleOperationComplete` → clear optimistic goal for active session |
| R3-G6 | Goal-bearing SSE: prefer `session_id`; `ResolveGoalEventSessionId` maps empty → active tab (legacy Engine exception) |
| R3-G7 | Local: same sync + terminal clear; preserve RAG-on-goal behavior |

**Follow-on (locked separately):** [`CHAT_SESSION_GOAL_PROTOCOL.md`](CHAT_SESSION_GOAL_PROTOCOL.md) **CSG-A 🔒 2026-07-30** — pass host `active_goal` on chat turns so Engine directional GRAG survives GUI restart; extends R3-G1 without changing banner ownership. [`CHAT_RESPONSE_REGURGITATION_PROTOCOL.md`](CHAT_RESPONSE_REGURGITATION_PROTOCOL.md) **CSG-B 🔒 2026-07-30** — chat reply prose vs RAG scaffold regurgitation (generation pipeline only).

### Phase R3 Implement Record

| Field | Value |
|-------|-------|
| Implemented | **2026-07-23** |
| Files | `MainFrame.cpp` / `MainFrame.h` · `tests/unit_tests.cpp` (`testGuiR3GoalSessionWire`) |
| Behavior | Session sync on goal paths · terminal banner clear · HTTP goal failure revert · remote Clear copy |

### Phase R3 Verify Record 🔶 2026-07-23

| Field | Value |
|-------|-------|
| Target | Compose Engine `http://127.0.0.1:8090` (`engine` 0.2, `/ready` includes `goals`, `events`, `control`) |
| Automated | `testGuiR3GoalSessionWire` + full `thoth-core-tests` ✅ |
| Engine probe | Sessions `r3-verify-a-1784831253`, `r3-verify-b-1784831253` (artifact dir `/tmp/r3-verify-65414/`) |

| Checklist item | Result | Evidence |
|----------------|--------|----------|
| Goal POST + SSE progress (strip/plan substrate) | ✅ | A: HTTP 200 `accepted`; SSE `PLAN_CREATED` + `STEP_*` with `session_id` = A; `plan.goal` = submitted text |
| Session B while A running (Engine correlation) | ✅ | SSE `STATE_CHANGED` / `PLANNING` for B at `18:39:20Z` while A on `synthesize` |
| POST `/v1/goals` rejection (G5 substrate) | ✅ | `{}` → HTTP 400 `INVALID_REQUEST`; empty `goal` → HTTP 400 |
| `PLAN_COMPLETED` → banner clear (G2) | ⏳ | **Not observed** in probe window: A stuck `STEP_STARTED` `synthesize` from `18:38:58Z` (inference latency/hang); operator GUI pass needed |
| Tab banner isolation (G1/G3) | ⏳ | **GUI only** — not driven in this session |
| Clear goal honest copy (G4) | ⏳ | **GUI only** |
| Local smoke (G7) | ⏳ | **GUI only** (`THOTH_ENGINE_URL` unset) |
| `POST /v1/control/abort` | ✅ | HTTP 200 `status: ok` (no `PLAN_ABORTED` in SSE log before probe ended) |

**Operator follow-up (≈5 min):** `export THOTH_ENGINE_URL=http://127.0.0.1:8090`, restart Control Panel, run §R3.4 banner/tab/Clear/local items; confirm one goal reaches **Completed** and banner clears.

### R3.4 Verify checklist (human)

- [x] Engine: goal POST + SSE `session_id` / plan lifecycle (partial — terminal pending)
- [x] Engine: POST `/v1/goals` rejection → HTTP 400
- [ ] Engine mode, session A: `goal: …` → banner; SSE strip/plan; `PLAN_COMPLETED` → banner cleared
- [ ] Session B tab during A run: banner shows B only
- [ ] Clear goal: honest copy; Abort when needed
- [ ] Local smoke: one goal still works

### Current observed symptoms (reported)

- Active Goal display out of sync with Engine.
- Goal completion / clearing unreliable in Engine mode.

### Expected end state

Set Goal / Run Goal / executive strip / plan panel show the same goal state the Engine reports via HTTP/SSE; completion clears or updates UI honestly.

### Scope

Diagnosis and repair of goal submission, session goal fields, SSE plan events, and banner/strip refresh — **Engine mode only** unless Local regression requires parity.

### Out of scope

- Scientific execution mode redesign.
- Resume-from-crash HTTP (GUI Integration Phase 14).

### Validation

Manual goal run: submit → executing → completed/failed/aborted; UI matches SSE and control API state.

### Completion criteria

Human-signed checklist for goal sync in Engine mode.

### Dependencies

R1 recommended; R1 goal-path `setRagFiles` confusion removed in R1.

### STOP gate

Do not start **R4** until R3 Verify is approved.

### Human approval requirement

Lock R3 → then Implement approval.

---

## Phase R4 — Remote Chat Reliability

**Status:** 🔒 **Locked 2026-07-23** · **Implemented ✅ 2026-07-23** · **Verify 🔶 2026-07-24**

### Phase R4 Verify Record 🔶

| Field | Value |
|-------|-------|
| Verified | **2026-07-24** (partial — Engine worker saturated during probe) |
| Automated | **`thoth-core-tests` ✅** (`testGuiR4ChatFailureSurfaces`, `testGuiR4ConversationTurnSessionWire`, `testEngineHttpConversationEndpoints`, Phase 10 helpers) |
| Engine probe | **`/ready` ✅** · **`conversation` capability ✅** · **R4-V7 partial ✅** — missing `session_id` → HTTP **400** |
| Blocked | **R4-V1–V6 live turns** — `POST /v1/conversation/turns` timed out (**180s+**) while worker held prior **~604s** turn (`execution_time_ms=604485` on `/v1/diagnostics/latest-decision`); queued verify curls exacerbated single-worker backlog |
| GUI pending | **R4-V1** restart/history · **R4-V2** timeout status bar · **R4-V3** processing→completed chrome · **R4-V4** goal+chat waiting copy · **R4-V5** tab mid-flight · **R4-V6** E1/E5 bubble parity · **R4-V8** local GUI smoke |
| Note | Empty `session_id` turn returned HTTP **200** on one probe — Engine accepts empty wire id; GUI always sends tab id via **SyncBackendSessionIdentity** (R4-G4) |
| Evidence | `./scripts/r4_engine_verify.sh` · `/tmp/r4-verify-*` probe dirs · abort/restart Engine before manual GUI pass |

**Automated Engine probes (optional, before GUI R4-V1–V8):**

| Command | curl timeout | Purpose |
|---------|--------------|---------|
| `./scripts/r4_engine_verify.sh sanity` | **240s** (`R4_VERIFY_SANITY_TIMEOUT`) | Single `hello` turn — fast sanity on slow hardware |
| `./scripts/r4_engine_verify.sh full` | **600s** per turn (`R4_VERIFY_TURN_TIMEOUT`; matches chat HTTP default) | 3-turn session + goal/chat contention (E2/E3/E7 substrate) |

Set `THOTH_ENGINE_URL` if Engine is not on `http://127.0.0.1:8090`.

**Manual sign-off (after Engine idle):** run R4-V1–V8 in GUI with `THOTH_ENGINE_URL=http://127.0.0.1:8090`; capture E1–E7 for V6; then flip this record to **Verify ✅**.

### Phase R4 Implement Record

| Field | Value |
|-------|-------|
| Implemented | **2026-07-23** |
| Files | `MainFrame.cpp` / `MainFrame.h` · `AgentInterface.cpp` / `.h` · `operation_result.h` · `tests/unit_tests.cpp` |
| Behavior | Engine chat failure status (G1) · `SyncBackendSessionIdentity` on send (G4) · in-flight Send disable + queue hint (G5/G6) · orphan SSE goal skip · `workerHasContentionBeforeEnqueue` |

### Objective

Engine-mode chat **reliability and GUI honesty**: send → Engine-owned turn → refresh on success or **clear failure**; no silent failures; documented timeout behavior; **Phase 10** conversation authority preserved.

### Phase R4 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-23** |
| Analyze | ✅ 2026-07-23 (revised plan — single phase; R4-G1–G8 + R4-V1–V8) |
| Prerequisite | **R3 Verify ✅** before R4 **Implement** (R3 may remain 🔶 until signed) |
| Normative | **R4-G1–R8** (decisions + acceptance criteria — not separate sub-plans) |
| In scope | `MainFrame` send/complete · `RemoteAgentBackend` chat/turn errors · optional minimal `AgentInterface` in-flight/queue chrome · docs/timeouts |
| Out of scope | Retrieval / GRAG / TCB · prompt/model changes · streaming redesign · R3 goal lifecycle · GUI framework redesign · **separate Engine workers or chat/goal queues** (unless future lock) |
| Primary tests | `testGuiR4ChatFailureSurfaces` · `testGuiR4ConversationTurnSessionWire` (names at implement) |
| Verify | **R4-V1–V8** + diagnostic evidence **E1–E7** |
| Post-lock rule | Do not expand R4 without new lock; implement only after explicit `AGENTS.md` approval |

### R4-G1–G8 (normative)

| ID | Decision |
|----|----------|
| **R4-G1** | **Failure visibility (centerpiece):** Engine-mode chat must **never** fail silently. On failed `kOpChat` (Phase 10 path): typing cleared + **user-visible** status (modal only per existing Phase 7). Covers **transport**, **timeout**, **HTTP 4xx/5xx**, Engine busy / invalid response. |
| **R4-G2** | **Phase 10 on failure:** No fake user/assistant bubbles on failure. Post-failure `GET conversation` only if Lock/verify proves partial commit (default: refresh on **success** only). |
| **R4-G3** | **Four states in copy:** User can distinguish **still processing**, **queued behind another Engine operation**, **failed**, **completed**. Document chat timeout default **600s** and **`THOTH_REMOTE_HTTP_TIMEOUT_SECONDS`**. |
| **R4-G4** | **Session consistency:** Before conversation send, **`SyncBackendSessionIdentity()`** — same rules as Phase 10, TCB4 Send, R3 goals; no parallel sync paths. |
| **R4-G5** | **Queue (minimal):** No new workers/queues. If turn waits on `AgentInterface` worker, show **waiting** chrome — not freeze, not silent drop. |
| **R4-G6** | **In-flight UX:** Track pending `kOpChat` per tab (`requestId` / `m_requestToSession`); disable Send or in-flight indicator; tab switch keeps correlation. |
| **R4-G7** | **Noise separation:** Non-chat failures (e.g. graph stats **10s** GET) must not read as chat failure. |
| **R4-G8** | **Local mode:** No regression; Engine branches gated on `supportsConversation` / `engineConversation`. |

**Single-reply integrity (acceptance):** One Engine assistant response → **exactly one** GUI assistant message — no duplicate assistant rows, stale replay, or buffer leakage in the UI (**R4-V6**). Root cause **not** assumed at lock; layer evidence **E1–E7** required.

### Diagnostic evidence (E1–E7)

| # | Layer | Evidence |
|---|--------|----------|
| E1 | GUI request | Send path, `requestId`, session id |
| E2 | HTTP turn | `POST /v1/conversation/turns` status + `assistant` in body |
| E3 | Engine store | `GET /v1/conversation/sessions/{id}` message list (counts, text) |
| E4 | Engine generation | `GET /v1/diagnostics/latest-decision` and/or `decision_trace.jsonl` for that turn |
| E5 | GUI display | Bubble count vs E3 after `RefreshSessionConversationFromEngine` |
| E6 | Host cache | `chat_sessions.json` — must not be truth in Engine mode |
| E7 | Queue/timing | Send → complete latency; goal/worker contention |

**Classify:** Duplicates in E3 only → Engine store. E3 once, GUI twice → refresh/render/double completion. One row, leaked text in content → generation (out of R4 fix unless GUI double-append proven).

### R4 implementation scope (Engine mode first)

| Item | Maps to |
|------|---------|
| `HandleOperationComplete` — Engine chat failure → status + typing cleanup | G1, G3 |
| Timeout/transport user copy · `retryable` | G1, G3 |
| `SyncBackendSessionIdentity()` on conversation send | G4 |
| Worker-wait / in-flight chrome (minimal) | G5, G6 |
| Audit success path: refresh-only vs double-apply | V6 |
| Protocol/README timeout docs | G3 |

### R4 verification checklist (R4-V1–V8)

| ID | Check |
|----|--------|
| **R4-V1** | Multi-turn same session (3+); restart GUI → history from Engine **GET** |
| **R4-V2** | Induced timeout (`THOTH_REMOTE_HTTP_TIMEOUT_SECONDS` low) → **failed**, typing cleared, clear message |
| **R4-V3** | Slow success within default timeout → **processing** then **completed** |
| **R4-V4** | Goal running + chat send → **queued/waiting** (G5), then completes or documented busy |
| **R4-V5** | Tab switch mid-flight → reply on originating session |
| **R4-V6** | **One query → one assistant row** in E3 and GUI; E1–E7 captured; classify duplicates vs content leakage |
| **R4-V7** | Transport / timeout / HTTP failure classes → G1 messaging each |
| **R4-V8** | Local smoke (`THOTH_ENGINE_URL` unset) — no regression |

**Chat-turn waiting chrome (2026-09-11):** Engine chat no longer uses a bare “Agent thinking…” hide-on-HTTP-success path. While a turn is in flight the center chrome shows **Waiting for Engine…** with elapsed time; after Engine success it shows **Loading reply…** until that turn’s assistant text is present in the originating session (exact user/assistant content pair), with one bounded refresh retry. Chrome clears only on verified transcript visibility or an explicit failure. Goal/executive events must not clear an owned chat turn. See `completed_improvements_log.md` (2026-09-11).

**Cognitive State panel (2026-09-12):** Goal / executive decision points surface in Observability tab **Cognitive State** (decision tape) — separate from chat-turn waiting. Do not drive typing-indicator ownership from cognition events. Right column is a tabbed notebook (like System State); see `architectural_facts.md` §8 and `completed_improvements_log.md` (2026-09-12).

Record results in **Phase R4 Verify Record** (append after manual pass).

### Current observed symptoms (reported)

- Query timeouts; end-to-end chat reliability issues in Engine mode.
- **Duplicate assistant entries / apparent history leakage in displayed replies** (cause TBD — verify via V6 + E1–E7).

### Expected end state

Send → append turn → assistant reply via **GET refresh** or **clear failure**; typing and status match Phase 10; timeouts documented.

### Scope

HTTP client timeouts, worker queue **presentation**, conversation append + refresh path, UI correlation — **not** retrieval quality.

### Out of scope

- Model quality / prompt changes.
- Streaming UX redesign.
- Retrieval, TCB, R3 goals, GUI framework redesign.
- Separate chat/goal Engine workers or queues.

### Dependencies

Phase 10 conversation path operational ✅.

### STOP gate

Do not start **R5** until **R4 Verify** is approved.

### Human approval requirement

**Lock ✅ 2026-07-23** → then **`AGENTS.md` Implement** approval.

---

## Phase R5 — Retrieval Verification & Honest Diagnostics

**Status:** 🔒 **Locked 2026-07-23** · **Implemented ✅ 2026-07-23** · Verify pending

### Phase R5 Implement Record

| Field | Value |
|-------|-------|
| Implemented | **2026-07-23** |
| Files | `retrieval_verification_display.h` · `GragDiagnosticsPanel.*` · `MainFrame.cpp` · `corpus_documents.h` · `rag.cpp` · `command_processor.cpp` · `tests/unit_tests.cpp` |
| Behavior | Four-layer labels (Scope / Candidate / Grounded / Inventory) · scope from `retrieval_trace` only · grounded from trace `grounding` (CHAT_RAG_CONTEXT-shaped) · `session_id` on retrieval diagnostics · post-grounding enriched SSE · corpus `id=` for duplicate names · R5 unit helpers |

### Objective

Expose and verify **retrieval truth** in the GUI and operator workflow **without changing retrieval behavior**.

**R5 changes how retrieval evidence is displayed and correlated, not how retrieval decisions are made.** Presentation, labeling, and correlation only. The Engine retrieval decision path remains authoritative and unchanged.

### Phase R5 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-23** |
| Analyze | ✅ 2026-07-23 (scope review + refinement) |
| Prerequisite | **R4 Verify ✅** before R5 **Implement** (protocol STOP gate); **TCB1–TCB4** ✅ |
| Normative | **R5-G1–G9** |
| In scope | GRAG panel + corpus labeling + diagnostics session correlation + operator Tests A/B/C/E + pure tests |
| Out of scope | GRAG scoring/weights/filters · TCB2/3 identity · ingest · benchmarks behavior · auto ingest · remote `setRagFiles` · prompt/model · warm-memory **behavior** change |
| Deferred (default) | `excluded_documents` debug · corpus API tier/owner columns · mandatory Test D · GRAG debug console |
| Verify | **R5-V1–V8** |
| Post-lock rule | Do not expand R5 without new lock; implement only after explicit `AGENTS.md` approval |

### Four-layer truth model (normative)

| Layer | Source | GUI must label |
|-------|--------|----------------|
| **Inventory** | `GET /v1/rag/corpus` | Unscoped engine document list |
| **Scope** | `retrieval_trace.retrieval_scope` | Policy for this request (`active_context_key`, tiers, selected docs) |
| **Candidate** | GRAG breakdowns / `candidates_found` | Pre-grounding scores — **not injected** |
| **Grounded** | `CHAT_RAG_CONTEXT` (`documents`, `grounded`, `grounding_mode`) | **Authoritative** injected context |

Footnotes (not extra layers): prompt **`[Memory Context]`** / `warm_memory:*` candidates — document in UI/playbook; skip paths without trace — **“retrieval skipped”**.

### R5-G1–G9 (normative)

| ID | Decision |
|----|----------|
| **R5-G1** | Scope UI **only** from embedded `retrieval_trace.retrieval_scope` (TCB-O6); GUI must not invent or re-derive scope. |
| **R5-G2** | Every retrieval-related GUI surface must **explicitly identify** which layer it represents: **Inventory**, **Scope**, **Candidate**, or **Grounded**. **No GUI label may imply grounding when only candidate evidence exists.** |
| **R5-G3** | **Grounded** authority = **`CHAT_RAG_CONTEXT`**; GUI may summarize, not recompute injection. |
| **R5-G4** | Corpus panel = **Inventory (unscoped)** — must not be read as active Agent Context or grounded set. |
| **R5-G5** | **`RETRIEVAL_DIAGNOSTICS`** carries `session_id` = `active_context_key` (observability-only); GUI strict tab match when set. |
| **R5-G6** | **No** changes to GRAG scoring, ranking, weights, filters, TCB2/3 identity, ingest, benchmarks, automatic ingestion, remote `setRagFiles`, prompt construction, model behavior, or warm-memory retrieval paths. |
| **R5-G7** | Warm-memory tier bypass: **document + footnote** in Scope/Candidate UI; behavior unchanged. |
| **R5-G8** | Retrieval skip paths without `retrieval_trace`: show **“retrieval skipped”**, not fabricated scope. |
| **R5-G9** | Duplicate filenames: distinguish via **existing** metadata in display/playbook; no indexing/identity changes unless a future lock adds read-only observability fields (default **no**). |

### R5 implementation order (one phase — internal steps)

1. **Trace-first GUI** — scope strip, candidate relabel, `request_id`, grounded summary (from `CHAT_RAG_CONTEXT` / trace, not breakdowns).  
2. **Inventory honesty** — corpus panel unscoped disclaimer.  
3. **SSE correlation** — Engine sets `session_id` on retrieval diagnostics; GUI gate.  
4. **Playbook + tests** — Tests A/B/C/E; optional D; unit helpers.  
5. **Verify** — R5 Verify Record.

### R5 verification checklist (R5-V1–V8)

| ID | Mandatory | Check |
|----|-----------|--------|
| **R5-V1** | Yes | Test **A** + evidence bundle; scope/trace/grounded align |
| **R5-V2** | Yes | Test **B** cross-session; diagnostics not on wrong tab |
| **R5-V3** | Yes | Test **C** — inventory visible ≠ grounded inject |
| **R5-V4** | Yes | Test **E** — duplicate names distinguishable via existing metadata |
| **R5-V5** | Yes | Human sign-off: all retrieval GUI labels satisfy **R5-G2** |
| **R5-V6** | Yes | `thoth-core-tests` R5 helpers + TCB2 trace parity regressions |
| **R5-V7** | Optional | Test **D** benchmark explicit scope (TCB-R4 harness) |
| **R5-V8** | Yes | Verify Record: `/version`, image id, `session_id`, `request_id` |

**Acceptance:** Operator can classify **Inventory / Scope / Candidate / Grounded** for one turn without contradicting Engine logs.

### Current observed symptoms (reported)

- Global corpus / multiple `GRAG.md` / full inventory creates uncertainty about scoped retrieval.
- GRAG panel reads like grounded context; difficulty verifying ingest → retrieval → inject chain.

### Expected end state

Reproducible operator procedure (Tests A/B/C/E); GUI layers labeled per **R5-G2**; `CHAT_RAG_CONTEXT` remains authoritative for grounded docs.

### Scope

Presentation, labeling, correlation, minimal observability on SSE (`session_id`); **not** retrieval quality tuning.

### Out of scope

- GRAG scoring, weights, ranking, filters (TCB2 behavior).
- TCB2/3 identity, ingest, benchmark behavior changes.
- Automatic ingestion, remote `setRagFiles` restoration.
- Prompt / model changes.
- Corpus management UI, filtering, new ingest controls.

### Dependencies

**R1** + **R2** ✅ recommended · **TCB1–TCB4** ✅ · Phase 10 ✅ · **R4 Verify ✅** before R5 Implement.

### STOP gate

Do not start **R6** until **R5 Verify** is approved.

### Human approval requirement

**Lock ✅ 2026-07-23** → then **`AGENTS.md` Implement** approval.

---

## Phase R6 — End-to-End Functional Audit

**Status:** Phase 0 Evidence ✅ 2026-07-24 · Implement pending

### Phase R6 Phase 0 Evidence Record ✅

| Field | Value |
|-------|-------|
| Captured | **2026-07-24** |
| Script | `scripts/r6_evidence_probe.sh` |
| Report | **`docs/r6_evidence_record.md`** |
| 0A Ingest | **PASS** — POST 200, session bind, indexed, retrieval scoped |
| 0B Turns | **PASS** (fresh worker) — T1 487s / T2 251s / T3 54s; prompt 365→710 chars |
| 0C Worker | No deadlock; diagnostics align with turn latencies |
| R6-02 update | Engine/HTTP ingest **confirmed OK**; GUI E1/E8 **not yet captured** |
| R6-08 update | Turn 3 **not** slowest; backlog + generation length dominate |
| Workdirs | `/tmp/r6-evidence-1784923121` (ingest), `/tmp/r6-evidence-1784923480` (turns) |

**Next human action:** Approve **Phase 1 — Fix Send to Engine (GUI path)** scope per evidence record.

### Objective

Verify major Engine-backed GUI workflows **together**: ingest, chat, goal, corpus list, connection/reconnect, honest failures.

### Current observed symptoms

Integration gaps discovered in R1–R5.

### Expected end state

Single audit checklist; residual backlog recorded; decision to resume or defer `GUI_integration.md` later phases.

### Scope

Checklist execution only; fix only **blockers** explicitly scoped into R5 lock.

### Out of scope

New features from paused integration roadmap.

### Validation

Full manual audit + `ctest -L pr` + docker smoke subset.

### Completion criteria

Human approves restoration closeout; backlog appended to `improvements.md` or restoration addendum.

### Dependencies

R1–R5 complete or explicitly waived with human approval.

### STOP gate

Restoration protocol complete — return to integration roadmap planning.

### Human approval requirement

Lock R6 → Implement (if any) → closeout approval.

---

## Document control

| Field | Value |
|-------|-------|
| Owner | Human architect + implementing agent per phase |
| Update rule | Append status markers; do not rewrite completed analyze records without new lock |
| Code changes | **Forbidden** until phase locked and `AGENTS.md` implement approval |
| Normative companion | `GUI_integration.md` (architecture); this doc (function) |

**Next human action:** **R5 Verify** (R5-V1–V8) · optional **R4 Verify ✅** · optional **R3 Verify ✅** sign-off.

---

## Appendix A — Phase R1 Analyze Record (2026-07-21)

**Scope:** Static repository trace + contract review. **No live Docker/Ollama run** in this analyze pass — environment-specific breaks must be confirmed during Refine/Verify.

### A.1 Intended chain (Engine mode)

| Step | Component | Mechanism |
|------|-----------|-----------|
| 1 | `FileDropTarget` → `MainFrame::HandleFileDrop` | Host path → session `ragFilePaths`; optional `MigrateFilesToSandbox` (host `agent_workspace/rag/`) |
| 2 | Local Notes UI | **[Send to Engine]** → `AgentInterface::createCorpusDocument` (worker thread) |
| 3 | `RemoteAgentBackend::createCorpusDocument` | Read host file → JSON `POST /v1/rag/documents` (`kControlTimeoutSec` = 30s) |
| 4 | `engine_http_transport` | `EngineRuntime::createCorpusDocument` → `BasicAgentPlugin` → `IndexManager::createCorpusDocument` |
| 5 | Storage | Atomic write under Engine workspace `rag/` (container volume when using Compose) |
| 6 | Indexing | `indexFileAsync` → chunk → `EmbeddingEngine::embed` / batch → `saveIndex` |
| 7 | Events | `INDEXING_*` → EngineRuntime SSE → `RemoteAgentBackend` → `MainFrame` handlers |
| 8 | Corpus UI | `GET /v1/rag/corpus` via `listCorpusDocuments` → **Engine Corpus** list |

Phase 8–9 contracts: [GUI_integration.md](GUI_integration.md) lock records; helpers in `corpus_create.h`, `corpus_documents.h`.

### A.2 Analyze status

| Field | Value |
|-------|-------|
| Analyze completed | 2026-07-21 |
| Locked | ✅ 2026-07-21 (Phase R1) |
| Live repro on Compose | Pending human Refine |

### A.3 Findings summary

Analyze + refine captured in § Phase R1 (locked implementation plan). Engine-side indexing honesty deferred to **Phase R2**.

---

## Appendix B — Phase R2 Analyze Record (2026-07-21)

**Environment:** `thoth-engine:local` container ~45 min old at analyze; `/ready` 200 with `corpus` + `ingest`; `GET /v1/rag/corpus` **200**; `POST /v1/rag/documents` **200**. Historical GUI **404** on corpus = **stale engine image (H5)**.

| Scenario | Key observation |
|----------|-----------------|
| **A** Normal | `r2-analyze-a.md` → accepted → START/COMPLETE (**metadata: `file_path` only**) → corpus **indexed**, 3 chunks; embed **501** + TfIdf fallback in logs |
| **B** llama stopped | Still **indexed** + chunks via TfIdf; embed errors in logs only |
| **C** Empty / ws | Empty → **400**; whitespace → accepted → COMPLETE → corpus **`pending`** indefinitely |
| **D** Interrupt | **Not executed** — required manually before R2 Verify |
| **Short doc** | `probe.md` → **pending** despite indexing attempt (0 storable chunks) |

**Classification:** H3 (reporting) primary; H4 (embed 501) operational; H5 for past 404. (H1 GUI “Indexed” on COMPLETE addressed in **R1.5**.)

**R2 target (🔒 locked):** COMPLETE metadata `success` / `reason` / `chunk_count`; corpus **`failed`** only as new terminal status; POST **acceptance-only** unchanged; **no** timestamp/schema cleanup unless GUI broken — document anomalies instead.

---

## Appendix C — Phase R1.5 Analyze Report (2026-07-21)

### Environment

| Item | Result |
|------|--------|
| Image | `thoth-engine:local` `15f1fdde0444` built **2026-07-21 09:38 AKDT** |
| Container | `thoth-thoth-engine-1` **Up (healthy)** · `8090→8090` |
| `/ready` | **200** — capabilities include **`corpus`**, **`ingest`**, **`events`** |
| `GET /v1/rag/corpus` | **200** — 11 documents (mix **`indexed`** / **`pending`**) |
| `POST /v1/rag/documents` | **200** — probe `r15-analyze-probe.md` accepted → corpus **`indexed`**, 1 chunk (~2s) |
| SSE `/v1/events` | Stream reachable (keepalive); indexing events not captured in this curl window (GUI uses same endpoint) |

### Workflow test

| Step | Result |
|------|--------|
| **DROP** | **Code review ✅** — host paths in session; `localNotesAreHostSideOnly`; no remote `setRagFiles` on drop; host-only status copy. **Manual GUI not re-run this session.** |
| **SEND** | **Code review ✅** — `OnSendToEngine` → `AgentInterface::createCorpusDocument` → `RemoteAgentBackend` JSON POST `/v1/rag/documents`; `OperationResult` → `HandleOperationComplete` refreshes corpus on accept. **Live POST ✅** (above). |
| **SSE** | **Code review ✅** — remote SSE loop `/v1/events`; `INDEXING_*` → progress + strip; `ActiveBackendProgressSource()` remote-only. **Live event trace during GUI send: not run** (requires control panel). |
| **CORPUS** | **Live GET ✅** — list matches Engine; **GUI ✅** — `RefreshCorpusPanel` renders `name · status · N chunks` from JSON (UTF-8 bullet fix in place). Refresh on accept + on `INDEXING_COMPLETED`. |
| **GUI** | **Partial** — wiring aligns with Phase 9 + R1; **fidelity gap:** `INDEXING_COMPLETED` sets status **“Indexed: …”** though SSE metadata is **`file_path` only** and corpus may still show **`pending`** (Appendix B **C**, **probe.md**). Corpus line is truthful; work-status line over-claims. |

### Root cause classification

| ID | Class | Notes |
|----|-------|-------|
| Primary | **A — GUI bug (display fidelity)** | “Indexed:” on COMPLETE without success signal in available metadata; fix in R1.5 Implement (copy + optional defer success wording to corpus refresh). |
| Secondary | **B — Engine semantics (deferred R2)** | `pending` forever on whitespace/zero-chunk; COMPLETE without outcome fields — **do not fix in R1.5**. |
| Not observed | **C — Docker/runtime mismatch** | Current image exposes Phase 8/9 routes; past 404 = stale image (H5). |
| Residual | **D — Unknown until manual GUI** | Full drop→send→SSE UX with `THOTH_ENGINE_URL` not exercised in this analyze run. |

### Recommendation

**Minimal GUI fixes (Implement phase):**

1. On `INDEXING_COMPLETED`, replace **“Indexed:”** with neutral copy aligned to available SSE data (e.g. **“Indexing finished:”**) and rely on **Engine Corpus** lines for `status` / `chunk_count` after refresh.
2. Optionally tighten Local Note slot label after COMPLETE (filename match on basename if `file_path` is engine-absolute).
3. Run **R1.5 Verify** manual checklist (remote control panel + smoke 14/15).

**Deferred for R2 (no Engine work in R1.5):**

- COMPLETE metadata `success` / `reason` / `chunk_count`; corpus **`failed`**; whitespace/zero-chunk outcomes (Appendix B).

**Engine change rule:** No Engine changes required for call-path restoration; POST/corpus/SSE routes operational on current image.

### R1.5 Implement (2026-07-21)

- `MainFrame`: `INDEXING_COMPLETED` → **“Indexing finished:”**; SSE status lines use event path **basename**.
- `UpdateRagSlotLabel`: match slots by **basename** (engine-absolute `file_path` safe).
- Verify: API smoke 14/15 **pass**; full wx GUI path **manual** (operator + `THOTH_ENGINE_URL`).

### R1.5 Verify (2026-07-21)

| Check | Result |
|-------|--------|
| `/ready` 200 + `corpus`, `ingest`, `events` | ✅ |
| `GET /v1/rag/corpus` (smoke 14) | ✅ — seeded + live docs |
| `POST /v1/rag/documents` → corpus `indexed` (smoke 15) | ✅ — `r15-verify-final.md` |
| `thoth-core-tests` (R1, Phase 8/9, engine HTTP) | ✅ all passed |
| Control panel + `THOTH_ENGINE_URL` | ✅ starts `backend=remote url=8090` |
| Operator drop → Send → SSE → corpus UI | Spot-check recommended (not CI-automated) |

---

## Appendix D — Phase R2 Verify (2026-07-22)

**Environment:** `docker compose up -d --build`; `thoth-engine:local` image **2026-07-22**; `/ready` **200** (`corpus`, `ingest`, `events`).

| Scenario | Result |
|----------|--------|
| **C empty POST** | **400** ✅ |
| **C whitespace** | POST **200** → corpus **`failed`** + **`reason: empty_document`** ✅ |
| **A normal md** | POST **200** → corpus **`indexed`**, `chunk_count` ≥ 1 ✅ |
| **SSE metadata** | `INDEXING_COMPLETED` includes **`success`**, **`chunk_count`** ✅ |
| **B llama/TfIdf** | Not isolated this run (stack healthy; A succeeded) |
| **D interrupt** | **Not run** — optional manual |
| **Tests** | `thoth-core-tests` **all passed** (incl. `testR2IndexManagerIndexingHonesty`) |

**Outcome:** R2 Verify **approved** for phase closeout; Scenario **D** remains optional manual hardening.
