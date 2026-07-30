# Attachment State Analysis

## Purpose

Document the complete lifecycle of attachments between the Thoth GUI and Engine.

The goal is to identify:

- sources of truth
- ownership boundaries
- stale state
- synchronization failures
- lifecycle errors

**No implementation changes are proposed in this document.**

**Analysis date:** 2026-07-26  
**Scope:** Remote Engine mode (primary operator path) and Local backend parity where code is shared.  
**Related protocols:** [`THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md`](THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md) (TCB3/TCB4), [`GUI_integration.md`](GUI_integration.md) (Phases 8–10), [`GUI_RESTORATION_PROTOCOL.md`](GUI_RESTORATION_PROTOCOL.md) (R1/R2).

---

# 1. Attachment Objects

Every structure that participates in the attachment lifecycle, grouped by layer.

## GUI Objects

### `Thoth::ChatSession` (`includes/ChatSessionTypes.h`)

Per-chat session record persisted in `agent_workspace/chat_sessions.json`.

| Field | Role |
|-------|------|
| `id` | Session identity; becomes `session_id` on Engine ingest (TCB4) |
| `ragFilePaths` | Ordered list (max 4) of **host-side** paths shown as Local Notes slots |
| `localNoteEngine` | `map<host_path, LocalNoteEngineInfo>` — GUI-side binding after Send accept |

**Lifetime:** Created on new chat; loaded at startup; mutated on drop/delete/send/sync; saved via `MainFrame::SaveChatSessions()`.

**Owner:** GUI (`MainFrame`). Engine never reads this file.

### `Thoth::LocalNoteEngineInfo` (`includes/ChatSessionTypes.h`)

Per–Local Note Engine metadata keyed by **host path**.

| Field | Role |
|-------|------|
| `document_id` | Stable Engine document id from POST accept (`doc-<hash>`) |
| `document_name` | Engine-side filename (may differ on collision, e.g. `EGAR_1.md`) |
| `chunk_count` | Last known chunk count (-1 unknown) |
| `indexing` | True after accept until terminal corpus/indexing state |
| `failed` | True when corpus sync or INDEXING_* reports failure |

**Sent predicate (Send picker):** `document_id` non-empty ⇒ already sent (`localNoteAlreadySent` in `includes/local_note_engine_sync.h`). Indexing and failed states **still count as sent**.

### MainFrame ephemeral UI state (`src/MainFrame.cpp`, `includes/MainFrame.h`)

| State | Role |
|-------|------|
| `m_sessions` | In-memory vector of all `ChatSession` |
| `m_activeSessionIndex` | Index into `m_sessions` for active tab |
| `m_sessionId` | Copy of active session's `id`; wired to backend via `setSessionId` |
| `m_ragFileSlot1..4`, labels/tooltips | Rendered view of `ragFilePaths` + `localNoteEngine` |
| `m_corpusText`, `m_corpusStatus` | Last fetched Engine inventory display |
| `m_lastCorpusPollForIndexingMs` | Throttle for corpus poll while indexing pending |

**Lifetime:** Process lifetime; corpus panel text is a **display cache** until next `RefreshCorpusPanel()`.

## Send Picker State

Not a persistent object. Built on demand in `MainFrame::OnSendToEngine()`:

1. `LocalNoteEngineSync::collectUnsentLocalNotePaths(session)` → host paths in `ragFilePaths` where `document_id` is empty.
2. If `unsent.size() == 1` → auto-select; no dialog.
3. If `unsent.size() > 1` → `wxSingleChoiceDialog` listing **basenames only** (not full paths).

**Queries:** GUI session state only. Does **not** query Engine inventory or attachment registry.

**Caching:** None (recomputed each button click).

## Engine Attachment Registry

### `IndexManager::attachmentOwners_` (`external/basic_agent/include/index_manager.h`)

In-memory map: **normalized absolute storage path** → **`owner_context_id`** (v1 = `session_id`).

### `agent_workspace/rag_attachment_registry.json`

Durable JSON persisted by `IndexManager::saveAttachmentRegistry()`:

```json
{
  "schema_version": 1,
  "owners": {
    "/path/to/agent_workspace/rag/EGAR.md": "session-1234567890"
  }
}
```

**Owner:** Engine (`IndexManager`). Loaded at init; updated on `registerAttachmentOwner()` after successful document write + bind.

**Replace rule:** `findSessionOwnedAttachmentPath(owner_context_id, base_name)` finds an existing registry entry for the same session + basename; `createCorpusDocument` then **overwrites that file** instead of suffixing. Also attempts reclaim of `"default"`-owned paths when real session sends (TCB3 tests).

**Not tracked:** Host-side Local Note path, GUI `document_id`, or logical "same document across sessions."

## Corpus Inventory

### On-disk document files

Under Engine `rag/` (sandbox: `agent_workspace/rag/` locally, `/workspace/rag/` in Docker). Created by `IndexManager::createCorpusDocument()` via atomic write (`.tmp.<pid>` → rename).

**Collision naming:** If file exists and session-scoped replace does **not** apply → `stem_1.ext`, `stem_2.ext`, … (up to 10000 attempts). This directly explains observed duplicates (`EGAR_1.md`, `COGNATE_V2_2.md`).

### `IndexManager::chunks` + `rag_index.bin`

Vector store entries (`CodeChunk`) keyed by normalized absolute `fileName`. Persisted asynchronously via worker queue `saveIndex()`.

### `indexedFileFingerprints` / `indexingFailureReasons_`

In-memory indexing metadata. Failure reasons drive corpus `status: "failed"` in list API. **Not persisted** across Engine restart (chunks/index file may persist without failure map).

### Corpus list API model (`CorpusDocuments`, `GET /v1/rag/corpus`)

Built by `IndexManager::listCorpusDocuments()`:

- Union of: chunk file paths, fingerprint keys, **recursive filesystem scan** of `rag/` for supported extensions.
- Each entry: `id` = `stableDocumentId(storage_key)`, `name` = basename, `status` = indexed | pending | failed, optional `chunk_count`.

**Scope:** **Global / unscoped** — all documents on Engine volume, not filtered by session (R5-G4, TCB inventory hint).

## Retrieval Layer

| Object | Role |
|--------|------|
| `CodeChunk.corpus_tier` | Runtime classification: `session_attachment`, `benchmark`, `system_reference`, `legacy_orphan`, etc. |
| `CodeChunk.owner_context_id` | From attachment registry lookup on chunk path |
| `RetrievalScope` | Request-scoped filter (TCB2); active context = session id |

Retrieval reads **chunks + registry** at query time, not GUI `localNoteEngine`.

## Session Identity Objects

| Object | Location | Role |
|--------|----------|------|
| `ChatSession.id` | GUI JSON | Primary key for Local Notes map; sent as `session_id` on ingest |
| `RemoteAgentBackend::session_id_` | Remote backend | Set by `setSessionId`; included in POST body |
| `AgentInterface::activeSessionId` | Agent bridge | Copied into worker task for create |
| `IndexManager::session_id` | Engine | Attached to INDEXING_* events |
| Engine conversation session | `POST /v1/conversation/sessions` | Created when `supportsConversation`; id stored in `ChatSession.id` |

**Assumption (TCB4):** GUI tab activation synchronizes backend session before Send. **Risk:** `ChatSession.id` is authoritative for bind, but attachment registry matching depends on **consistent** `session_id` across sends.

---

# 2. Ownership Model

| Object | Owner | Authority |
|--------|-------|-----------|
| Local Notes slot list (`ragFilePaths`) | GUI | User drop/import/delete; persisted in `chat_sessions.json` |
| Local Note Engine binding (`localNoteEngine`) | GUI | Updated on POST accept, INDEXING_*, corpus sync; persisted in `chat_sessions.json` |
| Send picker candidate list | GUI (derived) | Pure function of `ragFilePaths` + `localNoteEngine`; no Engine read |
| Host Local Note file bytes | Host filesystem | Read once at Send; copied to Engine |
| Engine storage file (`rag/*.md`) | Engine | Written by `createCorpusDocument`; replace vs suffix logic |
| Attachment registry (`rag_attachment_registry.json`) | Engine | `registerAttachmentOwner` after write when `session_id` present |
| Chunk index (`chunks`, `rag_index.bin`) | Engine | `indexFile` worker; async save |
| Corpus inventory API response | Engine (derived) | Rebuilt on each `listCorpusDocuments()` call |
| Retrieval scope / tier metadata | Engine (runtime) | Classified from registry + path rules |
| Engine inventory panel display | GUI (cache) | Last successful `RefreshCorpusPanel()` result |

**Critical boundary:** GUI owns **intent to attach** (host path + session id at send time). Engine owns **physical document + index + registry bind**. There is **no single shared attachment record** spanning both sides.

---

# 3. Creation Lifecycle

Complete transition graph (happy path):

```
Local file on host filesystem
    ↓  (user drop / Import Corpus)
HandleFileDrop → push path into ChatSession.ragFilePaths
    ↓  (optional MigrateFilesToSandbox — may rewrite path to agent_workspace/rag/)
SaveChatSessions + RefreshRagPanel
    ↓  (user clicks Send to Engine)
OnSendToEngine → collectUnsentLocalNotePaths
    ↓  (picker or auto-select host path)
agent->setSessionId(m_sessionId)
AgentInterface::createCorpusDocument (worker thread)
    ↓
RemoteAgentBackend: read host file → POST /v1/rag/documents
    { name, content, session_id? }
    ↓
Engine: IndexManager::createCorpusDocument
    - resolve path (replace if session-owned basename exists, else suffix if collision)
    - atomic write to rag/
    - registerAttachmentOwner if session_id non-empty
    - indexFileAsync(stored_path)
    - queue saveIndex()
    ↓  (HTTP 200 accept — immediate)
OperationResult → HandleOperationComplete
    → RecordLocalNoteIngestAccept(host_path, document_id, document_name)
    → RefreshCorpusPanel()
    ↓  (async worker)
indexFile: INDEXING_STARTED event
    → ApplyLocalNoteIndexingStarted (GUI)
    ↓
indexFile: chunk + embed + saveIndex
    → INDEXING_COMPLETED event
    → ApplyLocalNoteIndexingCompleted (GUI)
    → RefreshCorpusPanel()
    ↓
Retrieval: chunks classified session_attachment if registry bind exists
    ↓
GUI: RefreshRagPanel / SyncLocalNotesFromCorpus on corpus fetch
```

**Accept vs index:** POST accept returns before indexing completes (Phase 9 contract). GUI sets `indexing=true` at accept, not at INDEXING_STARTED.

---

# 4. Mutation Lifecycle

### Rename

- **Engine `document_name`:** May differ from host basename after collision (`notes_1.md`). Stored in `localNoteEngine.document_name`.
- **Host path:** If user removes and re-adds a file, path string may change; old `localNoteEngine` key orphaned unless explicitly erased.

### Delete (Local Notes X button)

- Removes path from `ragFilePaths` and erases `localNoteEngine[path]`.
- Does **not** delete Engine file, registry entry, or chunks.
- Does **not** remove from corpus inventory.
- Remote mode: does **not** call `setRagFiles` (R1).

**Observed Case A linkage:** After delete, `ragFilePaths` is empty → Send button disabled / no picker. User must re-import via drop or Import Corpus. Deleting alone is not a "share again" action.

### Resend (same session, same basename)

- Engine: `findSessionOwnedAttachmentPath` → overwrite same storage path → stable `document_id` (unit: `testSessionScopedReplaceOnResend`).
- GUI: Blocked if `document_id` already set — **Send picker excludes the path**. No current GUI "resend/replace" path unless `localNoteEngine` entry cleared.

### Resend (different session or missing registry bind)

- Engine creates `basename_1.ext`, new registry entry, new `document_id`.
- Prior file + chunks remain → **inventory duplicates**.

### Failed ingest

- **HTTP accept failure:** `RecordLocalNoteIngestAccept` not called; path remains unsent in picker.
- **Accept success + indexing failure:** `document_id` already set → **picker treats as sent**; user cannot retry via Send without manual state repair. Corpus may show `failed`; GUI sync sets `failed=true` via `syncSessionFromCorpusList`.

### Session change

- `ActivateSession` → new `m_sessionId`, `setSessionId`, refresh Local Notes from that session's maps.
- Engine registry unchanged; prior session attachments remain on volume.

### Sandbox migration (`MigrateFilesToSandbox`)

- Rewrites entries in `ragFilePaths` to `agent_workspace/rag/<basename>`.
- **Does not remapping `localNoteEngine` keys** — potential key drift if migration runs after bindings exist.

---

# 5. Modification Authority

| Object | GUI | Engine worker | Engine HTTP | Background timer |
|--------|-----|---------------|-------------|------------------|
| `ragFilePaths` | ✅ drop/delete/import | — | — | — |
| `localNoteEngine` | ✅ accept handler, INDEXING_*, corpus sync | — | — | — |
| `rag/*.md` files | — | ✅ createCorpusDocument | ✅ POST /v1/rag/documents | — |
| Attachment registry | — | ✅ registerAttachmentOwner | (via create) | — |
| Chunks / index | — | ✅ indexFile, saveIndex | — | — |
| Corpus list response | — | ✅ listCorpusDocuments | ✅ GET /v1/rag/corpus | — |
| Inventory panel text | ✅ RefreshCorpusPanel | — | — | ✅ 5s poll if indexing pending |

---

# 6. Read Paths

| Component | Reads |
|-----------|--------|
| `RefreshRagPanel` | `ragFilePaths`, `localNoteEngine`, remote/local mode |
| `ApplyIngestControls` | `collectUnsentLocalNotePaths`, capabilities, event stream |
| `OnSendToEngine` | unsent paths, `m_sessionId` |
| `RecordLocalNoteIngestAccept` | active session, OperationResult ingest fields |
| `SyncLocalNotesFromCorpus` | corpus JSON `documents[]`, matches on `document_id` |
| `ApplyLocalNoteIndexingStarted/Completed` | INDEXING_* metadata; resolves host path via basename / `document_name` / lone-indexing heuristic |
| `RefreshCorpusPanel` | `GET /v1/rag/corpus` via backend |
| `findSessionRagPathForIndexingEvent` | basename match, `document_name`, single `indexing` entry |
| `IndexManager::createCorpusDocument` | attachment registry, filesystem existence |
| `IndexManager::listCorpusDocuments` | chunks, fingerprints, rag dir scan, failure map |
| GRAG / retrieval | chunks, attachment registry, RetrievalScope |
| `localNoteAlreadySent` | `localNoteEngine[host_path].document_id` |

---

# 7. Send Picker Logic

**Input:** Active `ChatSession` (`ragFilePaths`, `localNoteEngine`).

**Output:** Single host path to send, or cancel.

**Filtering (`LocalNoteEngineSync`):**

| Rule | Behavior |
|------|----------|
| Candidate set | Every path in `session.ragFilePaths` (order preserved) |
| Exclude if | `localNoteEngine[path].document_id` non-empty |
| Include if | No map entry, or empty `document_id` |
| Does **not** check | Engine inventory, registry, indexing/failed status, other sessions |
| Does **not** allow resend | Once accepted (`document_id` set), path never reappears |

**Additional gates (`ApplyIngestControls` / `OnSendToEngine`):**

- `supportsIngest` from `/ready`
- Engine usable (event stream)
- Non-empty `m_sessionId`
- At least one unsent path

**Stale/conflict scenarios for reported Case B (sent files appear in picker):**

- `document_id` empty despite prior successful accept (handler skipped, wrong session index, operation lost).
- Path key mismatch: `ragFilePaths` uses migrated sandbox path; `localNoteEngine` keyed under pre-migration path (or vice versa).
- User interprets basename collision in multi-choice dialog incorrectly (picker shows basenames only).

**Inverse (Case A — picker does not appear):**

- No paths in `ragFilePaths` after Local Notes delete.
- All paths have non-empty `document_id` (including failed/indexing).
- Ingest disabled or engine not ready (button hidden/disabled, not an empty picker).

---

# 8. Local Notes Logic

**Display rules (`RefreshRagPanel`):**

- Up to 4 slots from `session.ragFilePaths[0..3]`.
- Remote mode (`localNotesAreHostSideOnly`): slots show `(host-only)` until Engine metadata present.
- After send: label via `formatLocalNoteEngineSlotLabel` — basename, id, chunk count, indexing/failed suffix.
- Local (non-remote) mode: basename only; no Engine sync labels.

**Deletion:** Removes slot from session vectors only (see §4). Engine artifacts untouched.

**Refresh triggers:**

- Session activate, drop/import, delete, send accept, INDEXING_*, corpus sync, `RefreshAllPanels`.

**Sync from Engine (`SyncLocalNotesFromCorpus`):**

- Only updates entries with matching `document_id` already in `localNoteEngine`.
- Does **not** discover new attachments or fix missing bindings.
- Called from `RefreshCorpusPanel` after inventory fetch.

---

# 9. Engine Inventory Logic

**API:** `GET /v1/rag/corpus` → `CorpusDocuments` v1 JSON.

**Implementation (`IndexManager::listCorpusDocuments`):**

1. Aggregate chunk counts per storage path.
2. Include fingerprint-only paths (indexed metadata without chunks).
3. Scan `rag/` tree for supported extensions — **includes files with no registry bind and no chunks** (status `pending` or `failed`).
4. Emit one row per storage path; `id` from hash of relative storage key.

**Persistence:** Document files and `rag_index.bin` persist on volume; registry in separate JSON file.

**Duplicate handling:** **None at inventory level** — each distinct storage file (`EGAR.md`, `EGAR_1.md`) is a separate row. Replace logic only prevents duplicates **within same session + basename** when registry bind is correct.

**GUI presentation:** Read-only text area; header explicitly **"Engine inventory (read-only — not session attachments)"** / **"Engine inventory (unscoped)"**. Not filtered by active session.

---

# 10. Cache Inventory

| Cache | Location | Lifetime | Invalidation |
|-------|----------|----------|--------------|
| `m_sessions` + `localNoteEngine` | MainFrame RAM | Until save/load | SaveChatSessions on mutations |
| `chat_sessions.json` | Host disk | Persistent | GUI writes |
| Corpus panel text (`m_corpusText`) | MainFrame RAM | Until next refresh | `RefreshCorpusPanel` |
| `m_lastCorpusPollForIndexingMs` | MainFrame RAM | Process | Timer reset every 5s when indexing pending |
| `RemoteAgentBackend` ready/capabilities | Backend RAM | Until `/ready` refresh | ensureReady |
| `attachmentOwners_` | IndexManager RAM | Until reload | loadAttachmentRegistry at init; save on register |
| `chunks` / vector store | IndexManager RAM | Until reload | loadIndex at init; index mutations |
| `indexedFileFingerprints` | IndexManager RAM | Process | Updated on successful index |
| `indexingFailureReasons_` | IndexManager RAM | Process | **Lost on restart** |
| `rag_index.bin` | Engine disk | Persistent | saveIndex after queued tasks |
| `rag_attachment_registry.json` | Engine disk | Persistent | saveAttachmentRegistry |
| `shouldReindexFile` skip | Per-path fingerprint | Until file mtime/size change | — |

**No cache** for Send picker (always derived fresh).

---

# 11. Synchronization Events

| Event | Who updates what |
|-------|------------------|
| **Drop / Import** | GUI: `ragFilePaths`; optional sandbox migrate; Local: `setRagFiles`. No Engine document. |
| **Send started** | GUI: status text; backend: POST in flight. No state commit yet. |
| **Ingest accepted (HTTP 200)** | GUI: `localNoteEngine` id/name, `indexing=true`; corpus panel refresh. Engine: file on disk, registry bind, async index queued. |
| **INDEXING_STARTED** | GUI: `indexing=true` (again); activity strip. Engine: event only. |
| **INDEXING_COMPLETED success** | GUI: chunk count, `indexing=false`; corpus refresh. Engine: chunks in memory, saveIndex queued, failure map cleared. |
| **INDEXING_COMPLETED failure** | GUI: `failed=true`; corpus refresh. Engine: may have **zero or partial chunks** after prior remove; failure reason in memory. |
| **Corpus poll (5s timer)** | GUI: RefreshCorpusPanel → SyncLocalNotesFromCorpus when indexing pending. |
| **Local Notes delete** | GUI: remove path + binding. Engine: unchanged. |
| **Session switch** | GUI: reload slots for new session. Backend: `setSessionId`. Engine: unchanged. |
| **Engine restart** | Registry + index reload from disk; failure map empty; GUI JSON unchanged — **desync risk**. |
| **Container rebuild without volume** | Engine state fresh; GUI still has old `document_id`s — **desync risk**. |

---

# 12. Complete State Machine

## Primary flow (ASCII)

```
LOCAL FILE (host filesystem)
        |
        v
LOCAL NOTES (ChatSession.ragFilePaths — GUI owned, max 4)
        |
        v
SEND PICKER (unsent paths: no document_id in localNoteEngine)
        |
        v
ENGINE INGEST POST (acceptance only)
        |
        +--> HTTP fail -----------------> GUI: no binding; picker still shows path
        |
        v
ACCEPT OK
        |
        +--> GUI: localNoteEngine[host_path] = { id, name, indexing }
        +--> Engine: write rag file; registry bind; queue indexFileAsync
        |
        v
ATTACHMENT REGISTRY (path -> session_id)     CORPUS FILE ON DISK
        |                                              |
        +----------------------+-----------------------+
                               v
                    INDEX WORKER (indexFile)
                               |
                    removeChunksForFile (OLD CHUNKS DELETED FIRST)
                               |
                    chunk + embed (+ fallback paths)
                               |
                +--------------+--------------+
                |                             |
           success                         failure
                |                             |
                v                             v
        saveIndex (persist)          partial/zero chunks persisted
                |                             |
                v                             v
        INDEXING_COMPLETED            INDEXING_COMPLETED (success=false)
                |                             |
                v                             v
CORPUS INVENTORY (unscoped list)     status failed / low chunk_count
                |
                v
RETRIEVAL (RetrievalScope filters session_attachment)
                |
                v
GUI REFRESH (RefreshCorpusPanel, SyncLocalNotesFromCorpus, RefreshRagPanel)
```

## Failure / edge paths

### Duplicate (`EGAR.md` + `EGAR_1.md`)

```
Send with session_id S1 → EGAR.md (registry: EGAR.md -> S1)
Send with session_id S2 → collision, no S2-owned basename → EGAR_1.md
OR
Send with S1 but registry missing / wrong session → suffix duplicate
OR
First send without session_id → unbound file; second send with session → suffix
```

### Resend blocked in GUI

```
Accept sets document_id → localNoteAlreadySent true → excluded from picker
(Even if indexing failed or document degraded)
```

### Embedding failure / re-index degradation

```
indexFile:
  removeChunksForFile  ──>  ~95 chunks removed from memory
  embedBatch partial fail / fallback chunk_by_size
  finalize with stored > 0 but << original  ──>  saveIndex persists degraded state
```

Observed EGAR ~95 → ~5 chunks matches: **delete-then-rebuild** indexing with embedding errors triggering fallback chunking (`chunk_by_size` / `whole_small`), not transactional rollback.

### Partial indexing / persist timing

```
createCorpusDocument queues: [ indexFile task, saveIndex task ]  (FIFO worker)
saveIndex writes entire chunks vector — includes partial post-failure state
No rollback to previous rag_index.bin on failure
```

### Delete / re-add

```
Delete Local Note → ragFilePaths empty, binding erased, Engine doc remains
Re-add same file → new ragFilePaths entry, empty document_id → unsent (if re-imported)
Send without re-add → no picker (Case A)
```

### Session switch

```
Session A attachments in registry under A
Switch to Session B → Local Notes show B's slots; inventory still lists A's files
Retrieval in B excludes A's chunks (TCB2) if scope correct
GUI may still show A's document_ids in session A when switching back
```

## Duplicated sources of truth

| Concern | Source A | Source B | Source C |
|---------|----------|----------|----------|
| "Was this sent?" | GUI `document_id` | Engine file existence | Registry owner |
| Document identity | GUI `document_id` | `stableDocumentId(storage_key)` | On-disk filename |
| Display name | Host basename | Engine `document.name` | Chunk path basename |
| Indexing state | GUI `indexing`/`failed` | Corpus `status` | INDEXING_* events |
| Chunk count | GUI `chunk_count` | Corpus `chunk_count` | Live chunk query |
| Session binding | GUI `session.id` at send | Registry value | Chunk `owner_context_id` |

## Unclear ownership boundaries

1. **Who owns "the attachment"?** GUI treats host path + `document_id` as the record; Engine treats storage path + registry + chunks as the record — **no shared primary key** except convention on `document_id` hash.
2. **Who owns lifecycle after Local Notes delete?** Engine retains orphaned documents with no GUI visibility except unscoped inventory.
3. **Who owns resend/replace policy?** Engine supports same-session file replace; GUI **never sends again** once accepted.
4. **Who owns indexing transactionality?** Engine removes old chunks before new index succeeds; no snapshot/rollback — **destructive re-index**.
5. **Session identity:** Conversation session id, GUI tab id, and registry `owner_context_id` are intended to align (TCB4) but are stored in separate stores with no reconciliation loop.

## Possible race conditions

| Race | Effect |
|------|--------|
| Accept → RefreshCorpusPanel before index completes | Corpus shows `pending`; GUI `indexing=true` |
| INDEXING_* vs corpus poll ordering | Transient inconsistent chunk_count |
| `createCorpusDocument` worker vs `setSessionId` | Mitigated by explicit setSessionId before Send; AgentInterface also sets on worker |
| Concurrent sends same basename same session | Worker queue serializes; second should replace if first completed registry bind |
| GUI `m_activeSessionIndex` changes before `RecordLocalNoteIngestAccept` | Binding written to wrong session |
| `MigrateFilesToSandbox` vs existing `localNoteEngine` keys | Orphaned or mismatched bindings |
| Engine restart during GUI `indexing=true` | GUI stuck "indexing" until corpus sync or manual edit |
| `findSessionRagPathForIndexingEvent` basename heuristic | Wrong slot updated if multiple notes share basename |

## Current behavior vs design assumptions

| Assumption | Actual behavior |
|------------|-----------------|
| Same-session resend replaces duplicate inventory entries | Engine: yes (if registry hit). GUI: **blocks resend** after accept. |
| Send picker reflects Engine truth | Picker uses **GUI binding only**. |
| Local Notes delete removes attachment | **Host slot only**; Engine doc remains. |
| Inventory reflects session attachments | **Unscoped** full volume listing. |
| Indexing is atomic | **Remove-then-write**; failures degrade or zero out prior index. |
| `document_id` identifies document across resend | Stable only if storage path unchanged (replace path). Suffix creates new id. |
| Rebuild container fixes state | GUI JSON and Engine volume can disagree; duplicates persist on volume. |

---

# Conclusion

## 1. Current source(s) of truth

There is **no single source of truth**. In practice operators must consult:

- **Send eligibility:** GUI `chat_sessions.json` → `localNoteEngine[].document_id`
- **Physical documents:** Engine `rag/` filesystem + `GET /v1/rag/corpus`
- **Retrieval scope:** Engine `rag_attachment_registry.json` + chunk classification
- **Index content:** `rag_index.bin` + in-memory chunks

## 2. Actual source of truth required (product-level)

A unified attachment lifecycle needs one **Engine-authoritative attachment record** (stable id, owner session, storage key, generation/version, indexing state) with the GUI holding **references only** (id + host path for display/send), plus explicit operations: create, replace, delete, reindex, query — with consistent id across replace.

## 3. Conflicts identified

- GUI "sent" flag (accept-time `document_id`) vs operator expectation of retry after indexing failure.
- Engine duplicate files from suffix collision vs GUI single host basename.
- Unscoped inventory vs session-scoped retrieval vs session-scoped Local Notes.
- Destructive re-index vs operator expectation of safe re-ingest.
- Host path keys vs sandbox-migrated paths in `localNoteEngine`.
- Persistent GUI state vs rebuilt Engine container.

## 4. Missing lifecycle guarantees

- No transactional indexing (rollback on embed failure).
- No GUI-initiated Engine delete or replace.
- No reconciliation between GUI bindings and Engine registry after restart.
- No resend path after failed or degraded indexing.
- No deduplication in corpus inventory API.
- No validation that `session_id` on send matches registry owner before suffixing.
- Failure map not durable — corpus status after restart may show `pending` for failed docs.

## 5. Required protocol decisions (next phase — not decided here)

These are **decision points** for the formal lifecycle protocol; no recommendation is made in this document:

1. **Attachment primary key:** Engine document id vs host path vs pair?
2. **Resend semantics:** Replace in place vs new version vs blocked?
3. **Delete semantics:** Local Notes only vs Engine evict vs both?
4. **Indexing transaction:** Stage-and-swap index vs copy-on-write vs explicit two-phase?
5. **Picker authority:** GUI-only filter vs Engine "pending send" API?
6. **Inventory scope:** Global list vs session-filtered view vs both?
7. **Failure recovery:** Auto-clear `document_id` on terminal failure vs explicit "Retry indexing"?
8. **Session/registry reconciliation:** Startup sync job vs on-demand?

---

**STATUS:** Analysis complete. No code changes. Awaiting protocol design and implementation approval per `AGENTS.md`.
