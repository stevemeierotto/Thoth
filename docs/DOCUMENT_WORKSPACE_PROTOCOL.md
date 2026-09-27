# Thoth Document Workspace Protocol

**Abbreviation:** **DWP** — Document Workspace Protocol  
**Document type:** Architecture protocol (Engine Inventory · session attachments · operator workflows)  
**Status:** **DWP0** 📋 **DRAFT** — analyze + normative model; **no implementation** until plan approval and DWP lock  
**Created:** 2026-07-30  
**Prerequisite:** [`attachment_state_analysis.md`](attachment_state_analysis.md) ✅ · [`ATTACHMENT_LIFECYCLE_PROTOCOL.md`](ATTACHMENT_LIFECYCLE_PROTOCOL.md) (ALP1 🔒) · [`ATTACHMENT_LIFECYCLE_ALP1_IMPLEMENTATION_PLAN.md`](ATTACHMENT_LIFECYCLE_ALP1_IMPLEMENTATION_PLAN.md)  
**Related:** [`THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md`](THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md) (TCB2–TCB4 · TCB-ALP amendment) · [`GUI_integration.md`](GUI_integration.md) · [`GUI_RESTORATION_PROTOCOL.md`](GUI_RESTORATION_PROTOCOL.md) · [`AGENTS.md`](../AGENTS.md)

---

## Purpose

Define the **Document Workspace** architecture: a single **Engine Inventory** (master document library) and **session attachments** (chat references to inventory documents).

DWP resolves operator-visible failures such as:

- `EGAR.md` already indexed in Engine Inventory
- a new chat **Send** creates `EGAR_1.md`
- transactional indexing builds candidates but commit fails with `no_chunks` (global chunk dedup vs duplicate path)
- chats behaving as if they **own** documents instead of **referencing** them

**DWP does not introduce a parallel attachment system.** It names and operationalizes the product model that **ALP1** already specifies. Implementation reuses ALP structures (`document_registry`, session links, send policy, transactional indexing) and adds explicit **Import New** vs **Attach Existing** workflows plus revision-edit **stubs**.

Implementation requires explicit human approval per `AGENTS.md`. **No code changes are authorized by this protocol alone.**

---

## Phase DWP0 Record 📋

| Field | Value |
|-------|-------|
| Created | **2026-07-30** |
| Kind | Document Workspace — analyze, gap analysis, phased implementation plan |
| Status | **Draft** — pending review → **DWP1 Lock** |
| Input | EGAR re-send failure diagnosis (2026-07-30) · ALP1 locked protocol · ALP implement record (A–F ✅) |
| Supersedes | Ad-hoc “Send = attach” UX · implicit session-as-owner mental model in operator docs |
| Out of scope (DWP0) | Code · HTTP handlers · GUI widgets · migration execution |
| Next | Human review → **DWP1 Lock** → `AGENTS.md` implement approval → phased rollout |

---

## Relationship to ALP and TCB

| Layer | Document | Role relative to DWP |
|-------|----------|----------------------|
| **Identity & lifecycle** | [ALP1 🔒](ATTACHMENT_LIFECYCLE_PROTOCOL.md) | Normative: `document_id`, revisions, session links, transactional indexing |
| **Implementation detail** | [ALP1 Implementation Plan](ATTACHMENT_LIFECYCLE_ALP1_IMPLEMENTATION_PLAN.md) | Phases A–G; **ALP1 CERTIFIED — 2026-09-27**; G3 remains **VERIFIED**; G2b **SUPERSEDED/CLOSED** and not a pass |
| **Retrieval scope** | [TCB 🔒](THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md) | TCB2 scope; **TCB-ALP amendment** — filter by session link + `document_id` |
| **Operator product model** | **DWP (this document)** | Engine Inventory + chat attachments + dual GUI workflows + migration/rollout |

**Rule:** Where DWP and ALP1 agree, **ALP1 wins** on lifecycle invariants. DWP adds **product-facing** vocabulary (Inventory, Attach Existing, Chat Attachments) and **rollout phases** without duplicating ALP identity rules.

---

# 1. Architecture Review

## 1.1 Target model

The **Engine Inventory** is the authoritative master library. Each logical document exists **exactly once**.

```
Engine Inventory (authoritative)
├── doc-123  EGAR.md      [revision chain → indexed chunks]
├── doc-456  ALP.md
└── doc-789  Docker.md

Chat A — session_links → { doc-123, doc-456 }
Chat B — session_links → { doc-123, doc-789 }
```

| Property | Owner | Rule |
|----------|-------|------|
| Document identity | Engine | Stable UUID `document_id` per canonical slot |
| Canonical name | Engine | One `canonical_name` per operator document (e.g. `EGAR.md`) |
| Revision history | Engine | `revision_id` per ingest; committed revision serves retrieval |
| Content hash | Engine | Per revision (SHA-256) |
| Indexed chunks | Engine | Committed revision only in live index |
| Chat association | Engine | `session_links(session_id, document_id)` — **many chats → one document** |

**Chats do not own documents.** Chats **reference** inventory documents for retrieval context.

## 1.2 ALP alignment verdict

| DWP concept | ALP1 equivalent | Code status (2026-07-30) |
|-------------|-------------------|-------------------------|
| Engine Inventory | `document_registry` + registry-derived `GET /v1/rag/corpus` | ✅ Implemented; **not enabled in production** |
| Attach without re-index | Send policy `no_op` + `addSessionLink` | ✅ ALP-C |
| No suffix duplicates | `rag/attachments/{canonical_name}` | ✅ ALP-C (when `THOTH_ALP_ENABLED=1`) |
| Session-scoped retrieval | ALP-F `linked_document_ids` filter | ✅ Implemented |
| Transactional re-index | ALP-B (`THOTH_ALP_TX_INDEX=1`) | ✅ Enabled partially in prod |

**Verdict:** Engine Inventory **already represents** the target model **when ALP is fully enabled and migration has run**. Legacy + partial-TX production **does not**.

---

# 2. Current-State Analysis

## 2.1 Engine Inventory (legacy production path)

When `THOTH_ALP_ENABLED=0` (current default except `THOTH_ALP_TX_INDEX=1`):

| Aspect | Current behavior |
|--------|------------------|
| Identity | Path-derived `stableDocumentId(storage_key)` |
| Storage | Flat `rag/{name}.md`; collision → `EGAR_1.md`, `EGAR_2.md`, … |
| Ownership | `rag_attachment_registry.json`: path → `session_id` |
| Inventory API | Filesystem scan + chunk paths + fingerprints — **global, unscoped** |
| Cross-chat re-send | New session → suffix file → full re-index attempt |

### Observed failure chain (EGAR, 2026-07-30)

1. Chat A indexed `EGAR.md` successfully (95 chunks).
2. Chat B **Send** same bytes → legacy ingest allocates `EGAR_1.md`.
3. `THOTH_ALP_TX_INDEX=1` builds 95 transactional candidates (`stored_after_primary=95`).
4. `commitCandidateChunksForFile` → `addChunk` deduplicates by **`chunk.code` globally**.
5. All chunks identical to live `EGAR.md` chunks → **0 chunks under new path** → `reason=no_chunks`.
6. GUI shows indexing **failed**; inventory lists junk rows `EGAR_1.md`, `EGAR_2.md`.

## 2.2 Chat attachment tracking (GUI)

[`ChatSession`](../../includes/ChatSessionTypes.h) today:

| Field | Role |
|-------|------|
| `ragFilePaths` | Host paths (max 4) — Local Notes slots |
| `localNoteEngine` | `map<host_path, LocalNoteEngineInfo>` — **non-authoritative cache** |

Legacy send gating: `document_id` non-empty on host path ⇒ “already sent.” **No** persisted `session_attachments[]` by `document_id`.

## 2.3 GUI workflow today

Single operator path for both intents:

```
Drop / Import → Local Notes → Send to Engine → POST /v1/rag/documents (full content)
```

- Engine Inventory panel is **read-only** (no Attach action).
- ALP-E dry_run picker exists in code; requires `THOTH_ALP_GUI=1`.

## 2.4 Retrieval today

| Mode | Filter |
|------|--------|
| Legacy (TCB3) | `chunk.owner_context_id == active_context_key` |
| ALP (TCB-ALP / ALP-F) | `chunk.document_id ∈ scope.linked_document_ids` |

GRAG scoring unchanged; **scope** bounds the candidate set.

## 2.5 Partial ALP in production (critical)

| Flag | Production (2026-07-30) | Effect |
|------|-------------------------|--------|
| `THOTH_ALP_TX_INDEX=1` | ✅ | Transactional commit path active |
| `THOTH_ALP_ENABLED=1` | ❌ | Legacy create, suffix naming, path registry |
| `THOTH_ALP_GUI=1` | ❌ | Legacy unsent picker |

**This combination is forbidden by ALP-C misconfig gate in code and MUST NOT be used in production.**

---

# 3. Gap Analysis

| ID | Gap | Severity |
|----|-----|----------|
| **G1** | Partial ALP (TX without ENABLED) | Critical |
| **G2** | Legacy ingest default (suffix duplicates) | Critical |
| **G3** | ALP-D1 migration not applied on operator workspace | High |
| **G4** | Global chunk dedup by text breaks same-content re-index to new path | Critical |
| **G5** | No **Attach Existing** GUI workflow | High |
| **G6** | Chat persistence lacks `document_id` attachment refs | Medium |
| **G7** | No dedicated link-only HTTP API (attach without upload) | Medium |
| **G8** | Revision editing not stubbed for future work | Low (this phase) |
| **G9** | Failed suffix orphans in inventory (`EGAR_1`, `EGAR_2`) | Medium |
| **G10** | Synchronous indexing blocks Engine HTTP (SSE / health) | High |
| **G11** | ALP-G G2b manual certification was never signed (owner deferral 2026-09-13). Closed 2026-09-27 as **SUPERSEDED/CLOSED**, not as a pass. G3 sealed 2026-09-27. ALP1 was certified later on 2026-09-27 and that seal did not mark G2b PASS. | Closed 2026-09-27 |

---

# 4. Recommended Architecture (DWP Normative)

## 4.1 Authority model

| Concern | Authoritative owner |
|---------|---------------------|
| Document registry (identity, revisions, session links) | **Engine** — `document_registry.json` |
| Chunk index | **Engine** — `rag_index.bin` + in-memory chunks |
| Corpus inventory API | **Engine** — derived read model |
| Retrieval scope | **Engine** — `RetrievalScope.linked_document_ids` |
| Local Notes slots (Import staging) | **GUI** — `ragFilePaths` |
| Chat attachment display cache | **GUI** — reconciled from Engine |
| Session attachment refs (persistence) | **GUI** — `session_attachments[]`; Engine `session_links` wins on conflict |

**Rule:** If GUI cache disagrees with Engine registry, **Engine wins** after reconciliation (ALP §8).

## 4.2 Operator workflows 🔒 (DWP product model)

### Workflow A — Import New Document

```
Choose file (host)
    ↓
Upload content + hash + mtime
    ↓
POST /v1/rag/documents  →  create | new_revision | retry
    ↓
Index (async) → committed revision
    ↓
Document appears in Engine Inventory
    ↓
Session link created for current chat
    ↓
Chat Attachments list updated
```

### Workflow B — Attach Existing Document

```
Open Engine Inventory
    ↓
Choose document (by canonical_name / document_id)
    ↓
Attach to current chat  (no upload, no indexing)
    ↓
POST session attachment API  →  addSessionLink only
    ↓
Chat Attachments list updated
    ↓
Retrieval includes document immediately (committed revision)
```

**Invariant:** Attach Existing MUST NOT create a new storage file, revision, or suffix name.

### Interim equivalence (until Attach API ships)

`POST /v1/rag/documents` with matching `content_hash` → action `no_op` + session link. Acceptable for automation only; **not** acceptable as primary operator UX (requires reading full file bytes).

## 4.3 GUI surface areas

| Region | Purpose |
|--------|---------|
| **Engine Inventory** | Browse all workspace documents; **Import** is not implicit — explicit actions only |
| **Chat Attachments** | Documents linked to **this session** (`document_id`, name, status) |
| **Local Notes** (optional) | Staging for **Import New** only (max 4 slots) |

## 4.4 Chat persistence model (recommended)

Extend `chat_sessions.json`:

```json
{
  "session_attachments": [
    {
      "document_id": "550e8400-e29b-41d4-a716-446655440000",
      "canonical_name": "EGAR.md",
      "linked_at_ms": 1785431913670,
      "local_host_path": null
    }
  ],
  "rag_files": [ "/path/to/staging/EGAR.md" ]
}
```

| Field | Semantics |
|-------|-----------|
| `session_attachments` | Retrieval context for this chat (mirrors Engine session links) |
| `rag_files` | Host staging paths for Import New workflow only |

Remove Local Note (**X**): GUI slot only — ALP1 P0 (session links persist unless explicitly detached).

---

# 5. Retrieval (DWP + TCB-ALP)

## 5.1 Scope resolution

At query time ([`agent_context_retrieval.h`](../../external/basic_agent/include/agent_context_retrieval.h)):

1. `active_context_key` = session id (TCB §3.0).
2. `linked_document_ids` = `DocumentRegistry.listLinkedDocumentIds(session_id)`.
3. Include chunk iff:
   - `corpus_tier == session_attachment`, **and**
   - `chunk.document_id ∈ linked_document_ids`, **and**
   - chunk belongs to **committed** revision storage path (ALP-F map).

## 5.2 Cross-chat behavior 🔒

| Scenario | Retrieval |
|----------|-----------|
| Chat A linked `doc-123` (EGAR) | EGAR chunks visible in A |
| Chat B **not** linked to `doc-123` | EGAR **excluded** (TCB-X4 re-baseline) |
| Chat B **Attach Existing** `doc-123` | EGAR visible in B — **no re-index** |
| Inventory lists EGAR | **Does not** auto-attach to all chats |

**Clarification:** “One canonical EGAR in the workspace” applies to **storage and inventory**, not automatic retrieval in every tab.

## 5.3 GRAG

No scoring formula changes. DWP only defines **which chunks enter** the scoped candidate pool.

---

# 6. API Requirements

## 6.1 Existing (ALP-C — enable with `THOTH_ALP_ENABLED=1`)

| Method | Path | Role |
|--------|------|------|
| POST | `/v1/rag/documents` | Import New; dry_run intent; actions: `create`, `new_revision`, `no_op`, `retry`, `conflict` |
| GET | `/v1/rag/corpus` | Engine Inventory (registry-derived; one row per `document_id`) |

## 6.2 New (DWP — session attachments)

| Method | Path | Body | Effect |
|--------|------|------|--------|
| POST | `/v1/conversation/sessions/{session_id}/attachments` | `{ "document_id": "uuid" }` | `addSessionLink`; idempotent |
| GET | `/v1/conversation/sessions/{session_id}/attachments` | — | List linked documents |
| DELETE | `/v1/conversation/sessions/{session_id}/attachments/{document_id}` | — | Remove link; **does not** delete inventory document |

## 6.3 Reconciliation (ALP §8.3 — DWP adopts)

| Method | Path | Role |
|--------|------|------|
| POST | `/v1/rag/documents/reconcile` | Batch Local Note slot ↔ registry intent |

## 6.4 Future revision stubs (DWP §9 — not implemented)

| Method | Path | Phase-1 behavior |
|--------|------|------------------|
| GET | `/v1/rag/documents/{document_id}/revisions` | `{ revisions: [] }` schema stable |
| POST | `/v1/rag/documents/{document_id}/revisions` | `501` / `not_implemented` |
| GET | `/v1/rag/documents/{document_id}/content` | `501` (future editor load) |

---

# 7. Engine Requirements

| Requirement | Rationale | DWP phase |
|-------------|-----------|-----------|
| **`THOTH_ALP_ENABLED=1`** after migration | Registry model, no suffix ingest | P1 |
| **Forbid `TX_INDEX=1` without `ENABLED=1`** | Prevent EGAR_1 / `no_chunks` hybrid | P0 |
| **Scope chunk dedup** by `(document_id, fileName, code)` — not global text alone | Same-content commit to new path must not silently drop all chunks | P0/P1 |
| Run **ALP-D1 migration** | Collapse legacy flat `rag/` into attachments namespace | P1 |
| Session attachment HTTP handlers | Attach without upload | P2 |
| Registry-only corpus list | No suffix orphan rows in operator inventory | P1 |
| Non-blocking index worker | `/health`, `/ready`, `/v1/events` stay responsive during ingest | P4 |
| Admin orphan cleanup | Remove failed `EGAR_1`-style rows post-migration | P1 |
| Revision stub handlers | Stable schema; no editing behavior | P5 |

---

# 8. GUI Requirements

## 8.1 Actions

| Operator action | GUI | Backend |
|-----------------|-----|---------|
| Import New | Drop / Import Corpus → Send | `POST /v1/rag/documents` |
| Attach Existing | Inventory → Attach to Chat | `POST .../attachments` |
| Detach from chat | Chat Attachments → Remove | `DELETE .../attachments/{id}` |
| Remove Local Note | Slot **X** | GUI only (ALP1 P0) |

## 8.2 Status labels

Distinguish:

| Label | Meaning |
|-------|---------|
| **Attached** | Session link exists; committed revision |
| **Accepted** | HTTP accept; indexing in progress |
| **Failed** | Revision failed; prior committed unchanged |
| **Local only** | Host slot; not linked |

## 8.3 ALP-E integration

When `THOTH_ALP_GUI=1`:

- Send picker driven by Engine **dry_run** intent (not `localNoteAlreadySent` alone).
- `no_op` → link + mark attached; do not show as “unsent.”
- Reconcile on startup and corpus refresh.

## 8.4 Revision stub (GUI)

- Menu placeholder: **Edit Document…** → not implemented toast.
- **View revision history** → empty dialog from stub GET.
- **No editor widget** in DWP rollout phases.

---

# 9. Future Revision Workflow (Stub Only)

**Not implemented in DWP phases P0–P4.** Plumbing only — avoid painting into a corner.

## 9.1 Intended future flow

```
Open document → Modify → Save Revision → Re-index → Replace chunks → Linked chats see new revision
```

## 9.2 Future data flow

1. POST revision body → `beginRevision` → stage under `rag/revisions/{document_id}/{revision_id}/`.
2. Worker: ALP-B candidate → validate → commit → supersede prior → advance `current_revision_id`.
3. **Session links unchanged** — same `document_id`; retrieval picks up new committed chunks.
4. SSE: `INDEXING_*` events carry `revision_id`.

## 9.3 Stub interfaces (Engine — illustrative)

```cpp
struct SaveRevisionRequest {
    std::string document_id;
    std::string content;
    std::string content_hash;
};

struct SaveRevisionResult {
    std::string revision_id;
    std::string status;  // "accepted" | "not_implemented"
};

SaveRevisionResult saveDocumentRevisionStub(const SaveRevisionRequest&);
```

## 9.4 Invariants (future)

- `force_replace` and conflict rules from ALP1 §4 still apply.
- Failed revision does not advance `current_revision_id`.
- All linked sessions see the new revision after **COMMITTED** without re-attach.

---

# 10. Data Model

## 10.1 Engine — `document_registry.json` (authoritative)

Per ALP-A/C:

```json
{
  "schema_version": 1,
  "documents": [
    {
      "document_id": "uuid",
      "canonical_name": "EGAR.md",
      "storage_path": "rag/attachments/EGAR.md",
      "current_revision_id": "uuid"
    }
  ],
  "revisions": [ "..." ],
  "session_links": [
    { "session_id": "...", "document_id": "...", "linked_at_ms": 0 }
  ]
}
```

## 10.2 Engine — chunks

- `CodeChunk.document_id` — ALP-F retrieval binding.
- `CodeChunk.fileName` — committed storage path under `rag/attachments/`.

## 10.3 GUI — `chat_sessions.json`

Add `session_attachments[]` (§4.4). Retain `local_note_engine` as reconcile cache until DWP GUI phase complete.

## 10.4 Retired after migration

- Path-key `rag_attachment_registry.json` as write path.
- Flat `rag/{stem}_N.md` suffix files as active inventory rows.

---

# 11. Migration Strategy

## 11.1 Preconditions

1. Snapshot **M0**: `rag/`, `rag_index.bin`, `rag_attachment_registry.json`, `chat_sessions.json`, `document_registry.json`.
2. Stop Engine.
3. `thoth-migrate-alp --dry-run` → human review (ALP-D0).
4. Explicit approval per `AGENTS.md`.

## 11.2 Apply (ALP-D1)

1. `thoth-migrate-alp --apply` with documented rollback.
2. Winner rules for duplicate stems (`EGAR.md` vs `EGAR_1.md` vs `EGAR_2.md`).
3. Emit `legacy_id_map.json` (Phase-8 hash ids → UUID).
4. Committed bytes under `rag/attachments/{canonical_name}`.
5. Losers → `rag/migration_archive/{run_id}/`.

## 11.3 GUI session upgrade

For each `localNoteEngine` entry: map ids via `legacy_id_map.json`; populate `session_attachments[]`.

## 11.4 Flag rollout (target production)

```bash
THOTH_ALP_ENABLED=1
THOTH_ALP_TX_INDEX=1
THOTH_ALP_GUI=1
```

## 11.5 Rollback

- `THOTH_ALP_ENABLED=0` restores legacy create path (M0 restore if registry corrupted).
- DWP does not mandate destructive deletion during migration.

---

# 12. Risks

| Risk | Severity | Mitigation |
|------|----------|------------|
| Partial ALP (TX only) | Critical | P0 ops guard; startup misconfig fatal/warn |
| Global chunk dedup | Critical | P0/P1 dedup scope fix |
| Migration data loss | High | M0 + rollback CLI |
| Retrieval regression | High | TCB-X4 re-baseline; ALP-F tests; EGAR lifecycle harness |
| Duplicate `session_id` in GUI (`default`) | High | TCB4 — Engine-issued conversation session ids |
| Engine HTTP blocked during index | High | P4 worker isolation |
| GUI/cache drift | Medium | ALP-E reconcile |
| Embed server 500 under batch | Medium | Separate embed hardening track |
| Breaking API clients expecting suffix names | Low | Document canonical-name-only inventory |

---

# 13. Phased Implementation Plan

Phases are **DWP rollout labels**. Engine/GUI work **reuses ALP-A–G** where noted.

## P0 — Stop the bleeding (1–2 days)

| Task | Owner |
|------|-------|
| Ops: disable `THOTH_ALP_TX_INDEX` **or** enable full ALP only after migration | Ops |
| Engine: enforce TX-without-ENABLED misconfig gate at startup | Engine |
| Engine: fix chunk dedup scope (§7) | Engine |
| Optional: admin cleanup of failed suffix rows | Ops |

**Exit:** Re-send EGAR from new chat does not create suffix failure mode.

## P1 — Enable Inventory model (3–5 days)

| Task | Owner |
|------|-------|
| ALP-D0 dry-run + human approval | Ops |
| ALP-D1 apply + rollback verification | Ops |
| Enable `THOTH_ALP_ENABLED=1` | Ops |
| Registry-only corpus; one row per document | Engine |
| GUI `legacy_id_map` upgrade | GUI |
| `./scripts/alp_g_verify.sh gate` | QA |

**Exit:** Operator inventory matches single-document-per-name model.

## P2 — Session attachment API (3–4 days)

| Task | Owner |
|------|-------|
| Implement §6.2 HTTP handlers | Engine |
| `IAgentBackend` / Remote bridge | GUI |
| Persist `session_attachments[]` | GUI |

**Exit:** Attach Existing without POST body bytes.

## P3 — GUI dual workflow (4–6 days)

| Task | Owner |
|------|-------|
| Enable `THOTH_ALP_GUI=1` | Ops |
| Engine Inventory + **Attach to Chat** | GUI |
| Chat Attachments panel | GUI |
| ALP-E reconcile | GUI |
| ALP-G G2b operator sign-off | **SUPERSEDED/CLOSED** 2026-09-27. Not an open sign-off. See ALP protocol § ALP-G G2b Superseded Closeout. |

**Exit:** Import vs Attach clearly separated in UI.

## P4 — Engine robustness (2–3 days, may parallelize)

| Task | Owner |
|------|-------|
| Index worker off HTTP thread | Engine |
| Health/SSE independent of ingest | Engine |

**Exit:** GUI connection stable during large ingests.

## P5 — Revision stubs (1–2 days)

| Task | Owner |
|------|-------|
| Stub HTTP §6.4 | Engine |
| GUI placeholder menus §8.4 | GUI |

**Exit:** Stable future API; no editing.

## P6 — Legacy retirement (post ALP-G certification)

| Task | Owner |
|------|-------|
| Remove legacy create path (ALP-G) | Engine |
| Retire suffix collision + path registry writes | Engine |

---

# 14. DWP Invariants (draft — lock at DWP1)

| ID | Invariant |
|----|-----------|
| **DWP-INV-1** | Exactly one `document_id` per operator `canonical_name`. |
| **DWP-INV-2** | Engine Inventory lists **one row per `document_id`**, not per suffix file. |
| **DWP-INV-3** | Attach Existing creates **session link only** — no upload, no index worker. |
| **DWP-INV-4** | Import New is the **only** operator path that enqueues indexing. |
| **DWP-INV-5** | Retrieval uses **session-linked** `document_id` set, not global inventory. |
| **DWP-INV-6** | Same `content_hash` as committed → **`no_op` + link**, never suffix file. |
| **DWP-INV-7** | `THOTH_ALP_TX_INDEX=1` requires `THOTH_ALP_ENABLED=1`. |
| **DWP-INV-8** | Failed revision does not remove prior committed chunks from retrieval. |
| **DWP-INV-9** | Detach from chat removes **link only**, not inventory document. |
| **DWP-INV-10** | Revision edit endpoints may exist as stubs; **no** edit behavior until a future protocol lock. |

---

# 15. Verification (DWP acceptance)

| Check | Method |
|-------|--------|
| Cross-chat attach | Session B attaches EGAR by id → retrieval hit without re-index |
| Cross-chat isolation | Session C without link → zero EGAR chunks |
| Import new doc | New canonical name → one inventory row → link created |
| Re-import same hash | `no_op`; no `EGAR_1` suffix |
| Inventory cardinality | `GET /v1/rag/corpus` — no failed suffix rows after migration |
| ALP-G EGAR lifecycle | `./scripts/alp_g_verify.sh` (G2a) plus sealed G3. The manual G2b script is superseded and must not be executed. |
| Engine liveness during index | `/ready` + SSE connected during 95-chunk ingest |

---

## Document history

| Version | Date | Change |
|---------|------|--------|
| DWP0 draft | 2026-07-30 | Initial protocol from analyze session (EGAR failure, inventory/attachment model) |
| Status | 2026-09-27 | G11 / P3 / §15: G2b recorded **SUPERSEDED/CLOSED**. ALP1 **CERTIFIED — 2026-09-27**. G3 seal unchanged. G2b was not marked PASS. |

---

**Post-DWP0 rule:** Do not implement until **DWP1 Lock** + explicit `AGENTS.md` approval. ALP1 locked sections remain authoritative for lifecycle detail; amend ALP only via ALP lock process, not via DWP edits.

**STATUS:** DWP0 DRAFT — WAITING FOR REVIEW AND DWP1 LOCK APPROVAL
