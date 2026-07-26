# Thoth Agent Context Boundary Restoration Protocol

**Document type:** Architecture restoration protocol (agent context & retrieval boundaries)  
**Status:** **TCB0** 🔒 · **TCB1** 🔒 · **TCB1.5** 🔒 **§3.0** · **TCB2** 🔒 **TCB2.1** · **Verify** ✅ · **TCB3** 🔒 **TCB3.1** · **Verify** ✅ · **TCB4** 🔒 **Implement** ✅ · **Verify (E.4)** ✅ **2026-07-23** — **mandatory TCB complete**
**Created:** 2026-07-22  
**TCB0 locked:** 2026-07-22  
**TCB1 locked:** 2026-07-22  
**TCB2 locked:** 2026-07-22  
**TCB2 verified:** 2026-07-22  
**TCB3 locked:** 2026-07-22 (Appendix D) · **TCB3.1** 🔒 **2026-07-23** · **Human Lock TCB3 (final)** **2026-07-23**  
**TCB4 locked:** **2026-07-23** (Appendix E)
**Former filename:** `AGENT_MEMORY_BOUNDARY_PROTOCOL.md` (superseded by this name — same analyze content)  
**Prerequisite for:** [`GUI_RESTORATION_PROTOCOL.md`](GUI_RESTORATION_PROTOCOL.md) **R3+** — **R-series resumes after Appendix E.4 (TCB4 Verify)** (not before); must not redefine Agent Context outside this protocol  
**Last mandatory TCB gate 🔒:** **Appendix E.4** (TCB4 Verify — cross-context negative + positive). **TCB5** / **TCB6** are **not** R-series blockers (optional hardening / docs closeout).  
**Related:** [`AGENTS.md`](../AGENTS.md) · [`GUI_integration.md`](GUI_integration.md) Phase 0 Appendix D · [`GRAG.md`](GRAG.md) · [`plan_l_workspace_corpus.md`](plan_l_workspace_corpus.md) · [`plan_k_gui_api_client.md`](plan_k_gui_api_client.md) · [`GUI_RESTORATION_PROTOCOL.md`](GUI_RESTORATION_PROTOCOL.md)

---

## Purpose

During GUI/client–server restoration (R-series), operators observed that **Engine-mode chat and goals retrieve against global seeded corpus content** rather than the **active Agent Context** (v1: the session active on the Engine worker), while the original Thoth product assumption was:

> **Each agent has its own cognitive context** (bounded retrieval union — not “the whole Engine index”).

Dockerization (Plan L) and Engine ownership (Phases 8–10) correctly moved **persistence and corpus storage** to the Engine. That work did **not** lock a normative rule for **what each agent may retrieve** versus **what lives on the shared Engine volume**. The R-series must **not** patch symptoms session-by-session; this protocol defines the **Agent Context Boundary** — invariants and phased recovery for attachments, conversation memory, episodic memory, and retrieval scope together.

**Planning vocabulary:** **Session attachments** are one input to **Agent Context**, not a synonym for it. Locking invariants in attachment-only terms would force a protocol rewrite when conversation, episodic, approved long-term, and explicitly allowed shared knowledge join the default retrieval union.

**TCB0 🔒** locks analyze record (§1–§2, §4–§5, §7–§9, Appendix A). **TCB1 🔒** locks §3 (**§3.0** identity, §3.1–§3.7), §3.5–§3.6, §6, Appendix B **P1–P7**, and phase pipeline §8. **TCB1.5 🔒** centralizes **§3.0**. **TCB2 🔒** locks Appendix C. **TCB3 🔒** locks Appendix D. **TCB4 🔒** locks Appendix E. **Code:** per-phase **`AGENTS.md` implement approval**.

---

## Phase TCB0 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-22** |
| Kind | **Overall protocol** — Agent Context Boundary analyze + restoration phase structure |
| Frozen | Problem statement · current architecture (§1) · Agent Context model (§2) · failure path (§4) · recovery **options** (§5, not yet chosen) · migration risks (§7) · verification strategy (§9) · TCB0–TCB6 phase map (§8) · Appendix A code anchors |
| **Not frozen at TCB0** | §3 candidate invariants · §6 recommended architecture · any implementation |
| Post-lock rule | Do not rewrite TCB0 analyze sections without a **new lock**; refine via **TCB1** (Lock invariants + recovery) then TCB2+ Implement |
| Out of scope (TCB0) | Code · HTTP contract changes · choosing final recovery option |
| Next | **TCB1** — Analyze → Refine → **plan** → Lock (freeze §3 + §6 recovery choice) |

---

## Phase TCB1 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-22** |
| Kind | **Agent Context Boundary** — normative invariants, recovery architecture, observability contract, v1 identity rules |
| Normative | **§3.0** · **TCB-R1–R4**, **TCB-I1–I3**, **TCB-L1–L3**, **TCB-B1–B3**, **TCB-O1–O6** (§3.5–3.6) · **TCB-X1–X4** (§3.7) · **TCB-P1**, **TCB-P2** (Appendix B.4) |
| Product choices 🔒 | **P1–P7** (Appendix B.3) — incl. **P3** temporary warm-memory (future TCB phase); **P7** scope diagnostics |
| Identity | **§3.0 Context identity** (single normative rule; later phases inherit — do not restate v1 mapping) |
| Recovery 🔒 | **§5.4** request-scoped **`RetrievalScope`**; deliver via **§5.1** tier + **`owner_context_id`** + **§5.3** attachment registry |
| v1 retrieval union | **`session_attachment`** matching active context + **exclude** `benchmark` / **`system_reference`**; no episodic/warm vector union in TCB2 |
| Rejected | §5.2 / §5.5 / §5.6 default paths; remote host-path `setRagFiles`; TCB1 code |
| Pipeline 🔒 | **TCB1 Lock → TCB2 retrieval scope → TCB3 session ingest identity → TCB4 GUI wiring → Appendix E.4 (last TCB gate) → R-series resumes** (TCB5–TCB6 optional — see §8) |
| Post-lock rule | Do not expand TCB1 or revise **§3.0**, §3.1–§3.7, §6, or P1–P7 without **new lock** |
| TCB1.1 refine | **2026-07-22** — terminology only: normative **`active_context_key`** (v1 **=** **`session_id`**); supersedes draft **`active_session_id`** wording; semantics unchanged |
| TCB1.2 refine | **2026-07-22** — observability: **TCB-O5** **`context_policy_version`** on **`RetrievalScope`** (Appendix C §C.2); semantics of R/L/B invariants unchanged |
| TCB1.3 refine | **2026-07-22** — tier enum: **`system_seed`** → **`system_reference`** (reserve **seed** for Plan L **`benchmark`** corpus only) |
| TCB1.4 refine | **2026-07-22** — observability: one Engine-built **`RetrievalTrace`** per retrieval; **`CHAT_RAG_CONTEXT`** + **`RETRIEVAL_DIAGNOSTICS`** embed it (§3.6) — no duplicate scope payloads |
| TCB1.5 refine 🔒 | **2026-07-22** — **§3.0 Context identity** centralized; redundant v1 mapping text elsewhere → pointers to **§3.0** (semantics unchanged) |
| Next | **TCB3 Implement** — `AGENTS.md` approval (Appendix D) |

---

## Phase TCB1.5 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-22** |
| Kind | **Context identity centralization** — single normative rule **§3.0** |
| Normative | **§3.0** · **TCB-ID1** (summary invariant) |
| Semantics | Unchanged from TCB1 + TCB1.1–TCB1.4; **no** new product choices |
| Supersedes | Duplicate identity / v1-mapping prose in §2.1, Lock Record Identity row, phase lock Identity rows (replaced by **§3.0** pointers) |
| Post-lock rule | Do not revise **§3.0** without **new lock** |
| Next | **TCB3 Implement** — `AGENTS.md` approval (TCB3 🔒 **2026-07-23**) |

---

## Phase TCB2 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-22** |
| Kind | **Engine retrieval boundary** — `RetrievalScope`, runtime tier classification, request-scoped filter, **`RetrievalTrace`**, TCB2 tests |
| Normative plan | **Appendix C** (incorporated at lock; post-lock edits require **new TCB2 lock**) |
| Identity 🔒 | Inherits **§3.0** (no separate identity rule in this phase) |
| Scope snapshot 🔒 | **`RetrievalScope`** includes **`context_policy_version`** (**TCB2 ships `1`**) |
| Observability 🔒 | **`RetrievalTrace`** — single builder; **`CHAT_RAG_CONTEXT`** + **`RETRIEVAL_DIAGNOSTICS`** embed **`retrieval_trace`** (**TCB-O6**) |
| Index persistence 🔒 | **Runtime classification first** — load existing index → classify **`corpus_tier`** / **`owner_context_id`** at runtime → apply **`RetrievalScope`** filter. **No persistent index format migration in TCB2** unless strictly required for correctness. Optional on-disk v2 metadata fields deferred. |
| Permanent test 🔒 | **TCB-X1–X3** (§3.7) — **TCB-X1** seed exclusion by **scope**, not similarity |
| Post-lock revisit | **2026-07-23** — Appendix C + **§3.7 TCB-X2** → **§3.0** pointers (**TCB2.1**); **2026-07-22** partial pass superseded for Appendix C |
| TCB2.1 refine 🔒 | **2026-07-23** — Appendix C + **§3.7 TCB-X2** inherit **§3.0** (terminology only) |
| **TCB2.1 purpose** | Align TCB2 terminology with the normative Context Identity Rule in **§3.0**. No architectural or behavioral changes. |
| Out of scope (TCB2) | TCB3 ingest registry · TCB4 GUI · index format migration · episodic/warm union (P3) · GRAG weight changes |
| Pipeline 🔒 | **TCB2 Implement** → **TCB3** ingest identity → **TCB4** GUI wiring → **Appendix E.4 (last TCB gate)** → **R-series resumes** |
| Design alignment | Restores product goal: **each agent has its own cognitive context** (bounded retrieval union), after Docker/Engine migration separated storage from scope |
| Next | **TCB3 Implement** — `AGENTS.md` approval (after **TCB3 Lock** 🔒) |

---

## Phase TCB2 Verify Record ✅

| Field | Value |
|-------|-------|
| Verified | **2026-07-22** (automated gate) |
| Build | `cmake --build … --target thoth-core-tests` — **PASS** |
| Unit suite | `/home/steve/Thoth/build/tests/thoth-core-tests` — **All unit tests passed** (exit 0) |
| **TCB-X1** | `testTcb2ScopeBeatsSimilarity` — scope excludes higher-scoring **benchmark** tier |
| **TCB-X2** | `testTcb2CrossContextIsolation` — context A not visible in context B default scope |
| **TCB-X3** | `testTcb2RetrievalTraceParity` — identical **`retrieval_scope`** on diagnostics trace vs **`RetrievalTrace`** |
| **TCB-B*** (harness) | `testE1ChatRagBenchmarkSmoke`, `testE1GragBenchmarkSmoke` — **PASS** (in same suite; harness path unchanged) |
| **TCB-O6** | Covered by **TCB-X3** (trace parity) |
| Not in this gate | Docker Plan L L2 manual probe; operator GUI chat — **TCB4** / ops smoke (Engine + GUI wiring) |
| Handoff | **TCB3 Implement** when human approves per `AGENTS.md` (after **TCB3 Lock** 🔒) |

---

## Phase TCB3 Verify Record ✅

| Field | Value |
|-------|-------|
| Verified | **2026-07-23** (automated gate) |
| Build | `cmake --build … --target thoth-core-tests` — **PASS** |
| Unit suite | `thoth-core-tests` — **All unit tests passed** (exit 0) |
| **TCB-X4** | `testTcb3IngestBindAndCrossContext` — bind + cross-context exclusion on create path |
| Unbound ingest | `testTcb3IngestOmitSessionUnbound` — no default Agent Context without **`session_id`** |
| Registry | `testTcb3AttachmentRegistryPersistence` — **`rag_attachment_registry.json`** reload |
| **TCB-X1–X3** | No regression (full suite) |
| HTTP | `testEngineHttpCreateDocumentEndpoint` — acceptance unchanged (POST without **`session_id`**) |
| Not in this gate | GUI Send with **`session_id`** — **TCB4** |
| Handoff | **TCB4 Implement** when human approves per `AGENTS.md` |

---

## Phase TCB3 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-22** (Appendix D) · **TCB3.1** 🔒 **2026-07-23** · **Human Lock TCB3 (final)** **2026-07-23** |
| Kind | **Session ingest identity** — HTTP bind, **§5.3** attachment registry, persistence, **TCB-X4** |
| Normative plan | **Appendix D** (incorporated at lock, **TCB3.1** terminology — **§3.0** inherit); post-lock edits require **new TCB3 lock** |
| Identity 🔒 | Inherits **§3.0** only — **TCB3 implements** ingest bind (**P4**); no alternate identity rule |
| HTTP 🔒 | **`POST /v1/rag/documents`**: optional **`session_id`** string; **Phase 9 acceptance JSON unchanged** (still `accepted` + `document` id/name); omit or empty → ingest succeeds but **no** Agent Context bind (**P4**, **§3.0** ingest-without-binding) |
| Registry 🔒 | **§5.3** — Engine persists **storage path → `owner_context_id`** (in-memory **`attachmentOwners_`** today); **TCB3** adds durable registry under agent workspace, reload on startup, **`registerAttachmentOwner`** on bind after successful write |
| Classification 🔒 | After bind, runtime **`corpus_tier` = `session_attachment`** for that path’s chunks (**§5.1**); unbound ingests remain **`legacy_orphan`** until rebound |
| Permanent test 🔒 | **TCB-X4** (§3.7) — ingest bind + cross-context exclusion via HTTP/create path |
| Out of scope (TCB3) | TCB4 GUI **`session_id`** on Send · extended corpus list / session filter API · **`legacy_orphan` re-bind admin API** · index on-disk chunk metadata migration · GRAG weight changes |
| Pipeline 🔒 | **TCB3 Implement** → **TCB3 Verify** → **TCB4** GUI wiring → **Appendix E.4 (last TCB gate)** → **R-series resumes** |
| Post-lock revisit | **2026-07-23** — Appendix D → **§3.0** pointers (**TCB3.1**); Appendix C covered by **TCB2.1** |
| TCB3.1 refine 🔒 | **2026-07-23** — Appendix D inherit **§3.0** (terminology only) |
| **TCB3.1 purpose** | Align Appendix D terminology with **§3.0** Context Identity Rule. No architectural changes. No behavioral changes. No implementation changes. |
| Post-lock rule | Do not revise Appendix D HTTP/registry/classification contract or expand TCB3 scope without **new TCB3 lock**; **§3.0** unchanged |
| Next | **TCB4 Implement** — `AGENTS.md` approval (Appendix E) |

---

## Phase TCB4 Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-23** |
| Kind | **GUI wiring** — remote ingest **`session_id`**, session sync, honest UX, cross-context verify |
| Normative plan | **Appendix E** (incorporated at lock; post-lock edits require **new TCB4 lock**) |
| Identity 🔒 | Inherits **§3.0** only — wire **`session_id`** on create uses **backend session identity** when available; **GUI tab activation** synchronizes backend session before operations (chat, goals, Send); backend identity is **authoritative**, not a per-operation override from Send |
| Ingest 🔒 | **`POST /v1/rag/documents`**: include **`session_id`** from backend session identity when available (**TCB3** bind); omit when unavailable (**P4**) |
| R1 🔒 | **No** remote host-path **`setRagFiles`**; **no** auto-ingest on drop (preserve **GUI_RESTORATION** / R1) |
| Cross-context verify 🔒 | **Required** manual negative: session A Send doc A → session B asks about A → **no** retrieval hit for A; **no** seed/benchmark fallback grounding (**TCB-I1**, **TCB-R3**) — original defect = **one context must not retrieve another** |
| Positive verify 🔒 | Session A Send → chat in A may ground on attachment (same **`session_id`** / **`active_context_key`** per **§3.0**) |
| Out of scope (TCB4) | Phase 8 corpus **`session_id`** / tier filter API · auto-ingest on drop · Engine changes beyond TCB3 POST · GRAG weight changes |
| Pipeline 🔒 | **TCB4 Implement** → **Appendix E.4 Verify (last mandatory TCB gate)** → **R-series resumes** (**R3+ Implement** only after **Phase TCB4 Verify Record** ✅) |
| Last TCB gate 🔒 | **Appendix E.4** only — no TCB5/TCB6 approval required for **R3+ Implement** |
| Post-lock rule | Do not revise Appendix E without **new TCB4 lock**; **§3.0** unchanged |
| Next | **R3+** per [`GUI_RESTORATION_PROTOCOL.md`](GUI_RESTORATION_PROTOCOL.md) (**mandatory TCB sequence complete**) |

---

## Phase TCB4 Implement Record ✅

| Field | Value |
|-------|-------|
| Implemented | **2026-07-23** |
| Build | `cmake --build … --target thoth-core-tests` — **PASS** |
| Unit suite | `thoth-core-tests` — **All unit tests passed** (exit 0) |
| **Remote POST** | `CorpusCreate::makeCreateDocumentRequestBody` + `RemoteAgentBackend::createCorpusDocument` sends **`session_id`** from **`session_id_`** (trim; omit if blank) |
| **Session sync** | `MainFrame::ActivateSession` → **`setSessionId`** (existing); idempotent **`setSessionId`** before **Send to Engine** |
| **Local parity** | Unchanged — `LocalAgentBackend` uses **`getActiveSessionId()`** (**TCB3**) |
| **R1** | No remote **`setRagFiles`**; no auto-ingest on drop |
| **UX** | Send tooltip: session bind + full inventory vs retrieval scope |
| **TCB4 test** | `testTcb4CreateDocumentRequestSessionId` — POST body **`session_id`** / omit rules |
| Not in this gate | Appendix E.4 manual positive + cross-context negative (Engine + GUI operator) |
| Handoff | **Appendix E.4** (last mandatory TCB gate) — **Phase TCB4 Verify Record** when manual gate passes |

---

## Phase TCB4 Verify Record ✅

| Field | Value |
|-------|-------|
| Verified | **2026-07-23** — **Appendix E.4** (**last mandatory TCB gate**) |
| Stack | Compose **`thoth-engine:local`** @ `http://127.0.0.1:8090` · **`GET /version`**: `engine` **0.2**, `protocol` **v1**, `git` **unknown** |
| Procedure | HTTP parity with GUI Phase 10 + Phase 9: **`POST /v1/conversation/sessions`**, **`POST /v1/rag/documents`** with **`session_id`**, **`POST /v1/conversation/turns`** |
| **Ingest bind** | **`POST /v1/rag/documents`** `session_id` = **A** → **`/workspace/rag_attachment_registry.json`** maps `tcb4-e4-probe.md` → **A** |
| **Manual — positive** ✅ | Session **A**: distinctive doc ingested + indexed → chat in **A** → **`chat_rag_context`**: `grounding_mode` **`retrieved_context`**, `grounded` **true**, injected **`tcb4-e4-probe.md`**, `retrieval_scope.selected_documents` **`['tcb4-e4-probe.md']`**, `active_context_key` **A** (`request_id` **req-1784828336058-2**) |
| **Manual — cross-context negative** ✅ | Session **B**: same unique token query → **`chat_rag_context`**: `grounding_mode` **`no_retrieval_hits`**, `grounded` **false**, **`documents` []**, **`candidates_found` 0**, no **`tcb4-e4-probe.md`**, no **`GRAG.md`** in inject/breakdowns, `active_context_key` **B**, `allowed_tiers` **`['session_attachment']`**, `selected_documents` **[]** (`request_id` **req-1784829326809-4**) |
| **TCB-R3** | No seed/benchmark fallback on **B** (empty inject, not default corpus grounding) |
| **Automated** | **Phase TCB4 Implement Record** ✅ · **`thoth-core-tests`** green (prior gate) |
| **Not in this gate** | Docker L2 full operator regression; GUI click-path replay (same Engine contracts) |
| **R3+ handoff** | **Mandatory TCB complete** — **R3+ Implement** may proceed per §8 (**TCB5/TCB6 optional**) |

---

## Phase TCB1 — Lock invariants & recovery

**Status:** 🔒 **Locked 2026-07-22** · Analyze ✅ · Appendix B incorporated

**Objective (complete):** Recovery path chosen; §3 + §3.5 + §6 frozen; handoff to **TCB2** defined.

---

## Appendix B — TCB1 Analyze & Lock Plan (2026-07-22)

### B.1 Analyze scope

| Input | Result |
|-------|--------|
| TCB0 §1–§2, §4 | Root cause confirmed: **unscoped `retrieveChunks`** on Engine when `activeCorpusFiles_` empty; session id does not materialize **Agent Context** for RAG |
| Local parity | `setRagFiles` ≈ attachment-only filter — **semantic target** for Engine, not “bring back remote setRagFiles with host paths” |
| Phase 9 POST | Body today: `{ name, content }` only — **no `session_id`**; ingest cannot register attachment ownership |
| Phase 8 list | No tier / owner fields — inventory ≠ Agent Context (TCB4 presentation issue; policy is Engine-internal first) |
| R2 guard | POST remains **acceptance-only**; TCB1 may add **optional** JSON fields; must not block on indexing completion |
| GRAG math | Unchanged at TCB1 — scope/filter only |
| Concurrent sessions | **5.3 alone insufficient** (global `activeCorpusFiles_`) — TCB1 must lock **5.4 request-scoped** as normative |

### B.2 Recovery decision (recommended for Lock)

| Choice | TCB1 recommendation |
|--------|---------------------|
| Primary | **§5.4** — per-request **`RetrievalScope`** (active context key + resolved context materialization) |
| Delivery | **§5.4** request-scoped **`RetrievalScope`**; deliver via **§5.1** tier + **§3.0** / **§5.3** attachment registry |
| Rejected | §5.2 separate indexes (defer); §5.5 GUI filter; §5.6 multi-engine |
| Physical index | **Single index** (TCB-B3) with deterministic tier filter |
| **Identity (TCB1)** | See **§3.0** (do not restate here). |

**Locked v1 Agent Context materialization (retrieval union):**

1. All **`session_attachment`** tier chunks whose **`owner_context_id` matches `active_context_key`** per **§3.0**.  
2. **Exclude** `benchmark` / **`system_reference`** tiers from default union. (**Plan L** continues to use **“seed”** only for the **`benchmark`** harness corpus — not as a generic `corpus_tier` name.)  
3. **Do not yet union** episodic/warm/conversation into vector retrieval in TCB2 — hooks only; preserves TCB-R1 wording for future slices without implementing them in TCB2 scope.

### B.3 Product choices to lock at TCB1 (resolve open items)

| ID | Question | Recommended lock |
|----|----------|------------------|
| **P1** | **TCB-R3** — no attachments indexed | **No seed/benchmark grounding** — chat may run conversational path with `no_retrieval_hits` / empty inject (align Plan M fail-closed spirit); goals use in-context attachments only when present |
| **P2** | Legacy co-mingled docs on volume | Classify Plan L whitelist basenames + sentinel paths as **`benchmark`** tier; unknown ingests without **`owner_context_id`** → **`legacy_orphan`** excluded from default Agent Context until rebound per **§3.0** / **P4** |
| **P3** | **TCB-I3** cross-session warm memory in goal-directed RAG | **Preserve existing warm-memory behavior temporarily.** TCB1 does **not** modify episodic/warm memory isolation. **Future TCB phase required** before treating warm all-session goal behavior as permanent. **TCB2 scope:** v1 Agent Context filter applies to **index chunks only** (attachments + tier exclusion). |
| **P4** | POST `/v1/rag/documents` | Optional **`session_id`** (v1 wire) — binds attachment per **§3.0**; omit → ingested but **not** in default Agent Context |
| **P5** | Local mode | **Same Agent Context policy** inside plugin; retrieval filter follows **`active_context_key`** per **§3.0**, not accidental global state across tabs |
| **P6** | Harness / smoke | Explicit **`RetrievalScopeOverride`** or env flag (e.g. benchmark tier only) — **TCB-R4**; docker L2 probe unchanged in intent |
| **P7** | Retrieval traceability | Every retrieval request MUST be traceable to a **resolved `RetrievalScope`** via a single Engine-built **`RetrievalTrace`** (§3.6). Scope fields (**scope type**, **`context_policy_version`**, **allowed tiers**, **selected / excluded documents**) MUST NOT be duplicated or re-derived per sink. See **TCB-O1–O6** (§3.5). |

### B.4 Normative §3 at Lock (changes from draft)

- **Promote** TCB-R1–R4, TCB-I1–I3, TCB-L1–L3, TCB-B1–B3, **TCB-O1–O6** **verbatim** except:  
- **TCB-R3** replace “product choice at Lock” with **P1** locked text (no silent seed fallback).  
- **Add TCB-P1 (policy hook):** Engine exposes **`resolveAgentContextRetrievalScope(active_context_key)`** — input **`active_context_key`** per **§3.0** — used by all chat/goal retrieval entry points (implementation TCB2).  
- **Add TCB-P2 (trace hook):** After each `retrieveRelevant` / executive retrieval, Engine builds **one** **`RetrievalTrace`** (§3.6) and attaches it to every observability sink (`CHAT_RAG_CONTEXT`, `RETRIEVAL_DIAGNOSTICS`, decision trace, harness logs). **Contract locked at TCB1**; implementation **TCB2**.

### B.5 TCB1 explicit non-goals

- TCB2–TCB6 implementation · GUI corpus UX (TCB4) · R3 goal lifecycle · GRAG diagnostics wiring (R5) · auto-ingest on drop · HTTP breaking changes · new GRAG weights · deleting seed files from volume · **episodic/warm memory isolation changes (deferred — P3; future TCB phase)**

### B.6 Phase handoff after Lock TCB1

| Phase | Locked scope (from TCB1) |
|-------|----------------------------|
| **TCB2** | `RetrievalScope` resolution + tier metadata; per-request filter; **`RetrievalTrace`** (§3.6) on every retrieval; unit tests TCB-R/L/B/O; local+engine plugin paths |
| **TCB3** | Optional POST `session_id` (v1 context binding); **`owner_context_id`** registry; integration tests cross-context |
| **TCB4** | GUI: pass `session_id` on create; refresh attachment view; copy — **no** remote `setRagFiles`; **Appendix E.4** = **last mandatory TCB gate** |
| **TCB5** | Benchmark/smoke non-regression with explicit scope — **optional** (not an R3+ blocker) |
| **TCB6** | Docs closeout + cross-links — **optional** (may parallel R-series) |

### B.7 TCB1 Verify (2026-07-22)

- [x] Human confirms **P1–P7**  
- [x] Human confirms **5.4 + 5.1 + 5.3** bundle  
- [x] Lock Record appended; §3 + §3.5 + §6 marked 🔒  
- [x] **STOP** — no TCB2 code until separate implement approval  

### B.8 Risks accepted at Lock (unchanged from §7)

Legacy orphan docs; Phase 8 list still shows full inventory until TCB4; concurrent session correctness depends on **request-scoped** filter (not session-switch `setActiveCorpusFiles` alone).

---

## Mandatory workflow (every TCB phase)

Same discipline as GUI restoration:

```
Analyze → Refine → Lock → Implement → Verify → Human approval → Next phase
```

| Step | Rule |
|------|------|
| **Analyze** | Trace code paths + operator symptoms; **no fixes** |
| **Refine** | Narrow scope; resolve product/architecture choices |
| **Lock** | Human approves locked phase text |
| **Implement** | Exact locked scope only |
| **Verify** | Tests + manual + benchmark non-regression |
| **STOP** | No auto-start of next phase |

---

## Relationship to GUI Restoration (R-series)

| R phase | Dependency on this protocol |
|---------|------------------------------|
| **R1–R2** ✅ | Ingest **honesty** and corpus **listing** — **complete**; they do **not** define agent context / retrieval scope |
| **R3** Goal lifecycle | Must not assume shared Engine RAG index = per-context Agent Context |
| **R4** Chat reliability | Transport/timeouts separate from **Agent Context Boundary** |
| **R5** Retrieval verification | Verification procedure must declare **agent context slice** and **corpus tier** under test |
| **R6** Audit | Includes boundary checklist items from **TCB Verify** |

**Rule:** **TCB1 🔒** — R3+ must cite **TCB §3 / §3.5**. **R3+ Implement** only after **Appendix E.4** is approved and **Phase TCB4 Verify Record** ✅ (see [`THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md`](THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md) §8 — **last mandatory TCB gate**).

---

## 1. Current architecture

### 1.1 Design intent (historical)

| Layer | Original assumption |
|-------|---------------------|
| **GUI session** | One chat/agent tab ≈ one cognitive context: messages, optional dropped files, active goal |
| **Local mode** | Host `agent_workspace/` + in-process plugin; session switch reloads conversation and **RAG file set** into the plugin |
| **Retrieval** | GRAG scores chunks from an index; **which files enter the index for a request** was implicitly “what this session attached” in local workflows |

### 1.2 Deployment modes today

```
┌─────────────────────────────────────────────────────────────────┐
│ LOCAL (in-process BasicAgentPlugin)                              │
│  Host agent_workspace/ · memory.db · rag/ · chat_sessions.json   │
│  MainFrame::SyncAgentMemoryFromActiveSession → setRagFiles(paths)│
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ ENGINE (Compose / RemoteAgentBackend)                            │
│  Single EngineRuntime · one worker · one BasicAgentPlugin        │
│  Volume thoth-workspace → /workspace (memory.db + rag/ + index)  │
│  Seed: docker/seed_rag/ whitelist (Plan L)                       │
│  Ingest: POST /v1/rag/documents (Phase 9) → same /workspace/rag  │
│  Chat/goals: POST with session_id → ensureSessionOnWorker(id)    │
└─────────────────────────────────────────────────────────────────┘
```

**Critical structural fact:** The remote Engine runs **one plugin instance** shared by all HTTP `session_id` values. Session switching on the worker updates **conversation/episodic session identity** but does **not** today re-bind **retrieval corpus scope** per session.

### 1.3 How memory works (by category)

| Mechanism | Owner | Session awareness | Remote Engine behavior |
|-----------|--------|-------------------|-------------------------|
| **Conversation turns** | Engine (Phase 10) | `session_id` on messages (SQLite) | Authoritative per `session_id` |
| **Summaries / prune / restore** | Engine (`Memory`, M4) | `activeSessionId` + SQL filters | Per-session when APIs invoked with id |
| **Episodic / plans / trajectories** | Engine SQLite | `session_id` on plans and related rows | Goal/plan state keyed by session |
| **Warm / consolidated memory** | Engine | `searchWarmMemory` uses **active** session; goal path may use **all sessions** in `RAGPipeline` | Tied to whichever session last ran on worker |
| **Host `chat_sessions.json`** | GUI cache | Per-session UI chrome | **Not** Engine truth in remote mode |
| **GUI `ragFilePaths` in session JSON** | GUI | Per-session host paths | **Not** sent via `setRagFiles` when remote (R1 guard) |

Plan K explicitly documents: remote backend receives `session_id` on requests but **does not** manage an independent memory store beyond Engine APIs.

### 1.4 How retrieval works (GRAG + index)

Authoritative scoring and routing: [`GRAG.md`](GRAG.md) (`RAGPipeline::retrieveRelevant` → `IndexManager::retrieveChunks` → `GragScorer::rescore`).

**Index storage:** Single vector index + chunk table under Engine workspace (`rag_index.bin`, chunks keyed by file path / storage key).

**Corpus filtering (existing, local-oriented):**

- `BasicAgentPlugin::setRagFiles` indexes paths and calls `IndexManager::setActiveCorpusFiles(filePaths)`.
- `IndexManager::retrieveChunks`: if `activeCorpusFiles_` / `activeCorpusRoots_` are **non-empty**, results are **filtered** to those paths; if **empty**, **no filter** — **all indexed chunks** are eligible.

**Benchmark harnesses** (not GUI) already call `setActiveCorpusFiles` with explicit benchmark paths (`run_chat_rag_benchmark`, `run_grag_benchmark`, E2 strict paths).

**Chat path:** `CommandProcessor` calls `rag.retrieveRelevant(...)` for conversational turns; executive **RETRIEVAL** steps use the same pipeline in `WorkflowEngine`.

**Ingest path (Phase 9):** `createCorpusDocument` writes under Engine `rag/`, async-indexes, exposes document via `GET /v1/rag/corpus`. It does **not**:

- attach `session_id` to corpus metadata (Phase 8 v1 fields: `id`, `name`, `status`, `indexed_at`, `chunk_count`, `reason`),
- call `setActiveCorpusFiles` for the ingesting session.

### 1.5 Where session identity exists or is lost

| Location | Session id present? | Affects retrieval scope? |
|----------|---------------------|---------------------------|
| HTTP `/v1/chat`, `/v1/goals`, conversation APIs | Yes | **No** (only memory/conversation routing) |
| `EngineRuntime::ensureSessionOnWorker` → `plugin->setSessionId` | Yes | Updates Memory + Executive + IndexManager **event** session id — **not** active corpus filter |
| GUI `ActivateSession` → `agent->setSessionId` | Yes | Remote: **skips** RAG sync (`SyncAgentMemoryFromActiveSession` early return when `supportsConversation`) |
| `RemoteAgentBackend::setRagFiles` | N/A | **No-op** (by design, Plan K / R1) |
| GUI session `ragFilePaths` | Per GUI session | Used for Local Note + **Send to Engine** only; **not** a retrieval contract on Engine |
| `IndexManager::activeCorpusFiles_` | No | **Global** to the single plugin instance |
| Corpus list JSON | No `session_id` field | Cannot derive “this session’s documents” from API alone |

**Boundary loss summary:** Engine ownership of **disk + index** was conflated with “one shared retrieval universe for all agents.” Session identity survived for **conversation and plans**, not for **request-scoped Agent Context** (including which indexed documents may ground a turn).

---

## 2. Memory categories and agent context

### 2.1 Target model — Agent Context (normative direction)

**Agent Context** is the bounded retrieval/memory union for a single **active cognitive scope** — what the Engine may use for **default conversational and goal-directed retrieval** on that scope’s behalf. Sources are composed by **policy**, not by “everything on disk.”

**Identity hierarchy (direction — not fully implemented):**

```
Agent (long-lived)
 ├── persistent memory
 └── conversations
      └── sessions  ← active Agent Context binds here (**§3.0** for v1 key source)
            └── active Agent Context  ← retrieval scope binds HERE
```

Normative identity and **TCB v1 wiring** are defined once in **§3.0**. §2.1 describes the Agent Context composition model only.

```
Agent Context (bounded by active_context_key — see §3.0)
├── Current attachments      ← indexed documents registered to this context
├── Conversation memory      ← turns + summaries for this context
├── Episodic memory          ← plans, trajectories, warm tier (session-keyed today)
├── Approved long-term memory ← future: operator- or policy-approved retention
└── Shared knowledge         ← only when explicitly allowed (not silent seed tier)
```

**Out of default Agent Context (unless explicit opt-in):**

- Plan L **benchmark / seed tier** (docker smoke, harness corpora).
- Other agents’ attachments and conversation.
- Unscoped “full workspace RAG index” as a implicit default.

### 2.2 Storage categories (orthogonal to GUI “Engine Corpus” display)

| Category | Role in Agent Context | Typical content |
|----------|----------------------|-------------------|
| **Session attachments** | **Member** when ingested and registered to `session_id` | Local Note → Send to Engine → chunks |
| **Conversation memory** | **Member** (prompt + future retrieval-augmented recall) | SQLite `messages`, summaries |
| **Episodic memory** | **Member** under session policy | Plans, step metrics, warm rows |
| **Approved long-term memory** | **Member** when approved (future) | Consolidated / promoted facts |
| **Shared knowledge** | **Member** only if policy allows | Explicit shared corpus — not Plan L seed by default |
| **Benchmark / test corpus** | **Excluded** from default Agent Context | `docker/seed_rag/`, hardened suites, golden markdown |
| **Global system knowledge** | **Not** synonymous with RAG seed | Config, graph DB, facts — separate policy surface |

**Distinction (locked for planning):**

- **Engine owns storage** (Phase 8–10) ≠ **all stored bytes participate in every agent’s default retrieval**.
- **Benchmark corpus** must remain **addressable in isolation** for reproducible metrics (harness explicit scope — today `setActiveCorpusFiles`, env, or successor API).
- **Near-term restoration (TCB2–4)** focuses on **attachments + excluding silent seed tier**; the **Agent Context** frame is the long-lived invariant so later memory classes plug in without rewriting §3.

---

## 3. Required invariants (🔒 normative — TCB1 Lock 2026-07-22)

Post-lock changes require a **new lock** (including **§3.0** — see **TCB1.5 Lock Record**). Implementation phases must not expand scope without re-lock.

### 3.0 Context identity (🔒 normative — TCB1.5 Lock 2026-07-22)

**Purpose:** One rule for protocol identity vs **TCB v1** wire mapping. **TCB2**, **TCB3**, **TCB4**, and §5–§6 **inherit** this section — they MUST NOT restate the v1 mapping.

**TCB-ID1 — Context identity**

1. **Architecture:** Do **not** lock **`agent == session`**. Long-term model: Agent → persistent memory / conversations → sessions; **active Agent Context** is the bounded retrieval scope for one active cognitive scope (see §2.1 hierarchy).
2. **Protocol fields:** **`active_context_key`** — key for whose Agent Context is active on retrieval and scope resolution. **`owner_context_id`** — ownership on **`session_attachment`** material (chunks/documents); not a synonym for the agent entity.
3. **Default attachment filter (stable):** A **`session_attachment`** chunk/document is in the default Agent Context union iff **`owner_context_id == active_context_key`** (tier exclusions per §5.1 / **TCB-R2**).
4. **TCB v1 wiring (implementation mapping only):**

| Protocol field | v1 source | Typical surfaces |
|----------------|-----------|------------------|
| **`active_context_key`** | Request / worker **`session_id`** | `/v1/chat`, `/v1/goals`, `ensureSessionOnWorker`, conversation store, plan rows, **`resolveAgentContextRetrievalScope(...)`** input |
| **`owner_context_id`** (when bound) | Same string as registering **`session_id`** on ingest/register | `POST /v1/rag/documents` optional **`session_id`** (TCB3+), §5.3 attachment registry |

5. **Wire vs protocol:** HTTP/JSON and worker code may keep the field name **`session_id`**. Observability (**`RetrievalTrace`**, diagnostics) and protocol text use **`active_context_key`** / **`owner_context_id`** where the rule applies.
6. **Evolution:** A future **`context_id`** (or agent-scoped key) may replace the v1 mapping **without** rewriting **TCB-R1** tier/filter semantics — only resolver + ingest binding tables change.
7. **Historical:** TCB1.1 superseded draft **`active_session_id`** wording; meaning unchanged.

**Ingest without binding:** If ingest omits **`session_id`** (v1 wire), material is **not** registered to an Agent Context (**P4**); tier/classification may be **`legacy_orphan`** until rebound per **P2**.

### 3.1 Retrieval

| ID | Invariant |
|----|-----------|
| **TCB-R1** | **Default conversational and goal-directed retrieval** MUST be limited to the **active Agent Context** (see §2.1) — not the entire Engine workspace index. **Session attachments** are one source within that context once indexed and registered per **§3.0**; conversation, episodic, approved long-term, and explicitly allowed shared knowledge are additional sources per policy (some not yet implemented). Scope resolves **`active_context_key`** per **§3.0**. |
| **TCB-R2** | **Benchmark / seed tier** MUST NOT enter default Agent Context unless an explicit **operator, policy, or harness mode** opts in (debug/benchmark menu — out of scope until locked). |
| **TCB-R3** | When the **attachment slice** of the active Agent Context is empty, retrieval MUST still respect Agent Context rules: **no silent fallback** to benchmark/seed tier for grounding. Chat MAY use conversational path with **`no_retrieval_hits`** / empty inject (Plan M fail-closed spirit). Goals use in-context attachments only when present. |
| **TCB-R4** | Harness and CLI benchmark entry points MUST continue to set retrieval scope **explicitly** (today: `setActiveCorpusFiles`, dedicated workspace, or successor API) without requiring GUI agents or default Agent Context. |

### 3.2 Isolation

| ID | Invariant |
|----|-----------|
| **TCB-I1** | **Active Agent Context A** MUST NOT be used to ground retrieval for **active Agent Context B** on the same Engine process (attachments today; same rule for other context slices as they ship). Compare **`active_context_key`** per **§3.0**. |
| **TCB-I2** | Conversation content MUST remain isolated per `session_id` (already Phase 10 — preserve). |
| **TCB-I3** | Episodic/plan state MUST remain keyed by `session_id` (preserve). **Temporary (P3):** existing cross-session warm search in goal mode may remain until a **future TCB phase** addresses episodic/warm isolation — TCB1 does not modify it; document in TCB2 if behavior is unchanged. |

### 3.3 Leakage (must never happen)

| ID | Rule |
|----|------|
| **TCB-L1** | GUI MUST NOT imply that host-only Local Notes are indexed on Engine without successful ingest (R1 — preserve). |
| **TCB-L2** | A document registered as a **session attachment** for **context S** MUST NOT appear in **context T**’s default Agent Context unless a future **shared attachment / shared knowledge** feature is explicitly locked (default: **forbidden**). **S** / **T** are **`active_context_key`** values per **§3.0**. |
| **TCB-L3** | Benchmark seed content (e.g. `GRAG.md`, sentinel probes) MUST NOT dominate operator chat when the **active Agent Context** should prioritize session material (observed failure mode: unscoped index search). |

### 3.4 Benchmark reproducibility

| ID | Rule |
|----|------|
| **TCB-B1** | Plan L seed set and docker smoke probes remain valid on a **known corpus tier**, independent of per-context Agent Context policy. |
| **TCB-B2** | Published GRAG benchmark numbers remain tied to **documented corpus identifiers** ([`benchmark_results.md`](benchmark_results.md), env capture) — default Agent Context policy must not alter harness corpora. |
| **TCB-B3** | Any shared physical index is acceptable **only if** logical tier + filter semantics are deterministic and test-covered. |

### 3.5 Retrieval scope observability (🔒 normative — TCB1 Lock 2026-07-22)

Every retrieval request MUST be traceable to exactly one **resolved `RetrievalScope`** (policy output of **TCB-P1**, not implicit global index state).

| ID | Requirement |
|----|-------------|
| **TCB-O1** | **Traceability:** **`RetrievalTrace`** (§3.6) MUST include stable **`retrieval_scope_id`** and **`context_policy_version`** (see **TCB-O5**). Example: `retrieval_scope_id`, `context_policy_version`: **1**. |
| **TCB-O2** | **Scope type:** **`RetrievalTrace.retrieval_scope.scope_type`** (or equivalent nested path) — e.g. `default_agent_context` (v1 GUI path), `harness_override`, `benchmark_explicit` (TCB-R4). |
| **TCB-O3** | **Allowed corpus tier(s):** **`allowed_tiers`** on the scope slice of **`RetrievalTrace`** — e.g. `session_attachment` only; harness may add `benchmark`. |
| **TCB-O4** | **Documents:** **`selected_documents`** on the scope slice. **`excluded_documents`** MAY appear **only when debug is enabled** (config/env — operator opt-in); production default MUST NOT require full corpuses in events. |
| **TCB-O5** | **Policy version:** Scope slice of **`RetrievalTrace`** MUST include integer **`context_policy_version`**. **TCB2 ships version `1`.** Required for scientific reproducibility (Appendix C §C.2). |
| **TCB-O6** | **Single trace object:** Engine MUST construct **one** **`RetrievalTrace`** per retrieval invocation. **`CHAT_RAG_CONTEXT`**, **`RETRIEVAL_DIAGNOSTICS`**, decision-trace stages, and benchmark/harness logs MUST embed the **same** object under a common key (**`retrieval_trace`**, recommended). Sinks MUST NOT assemble separate scope payloads; GUI, chat logs, and GRAG panel MUST read scope from **`RetrievalTrace`**, not parallel fields. |

**GUI (R5 / TCB4 adjacency):** `GragDiagnosticsPanel` consumes **`RETRIEVAL_DIAGNOSTICS`**; chat tooling consumes **`CHAT_RAG_CONTEXT`**. Both MUST use the embedded **`retrieval_trace`** for scope (TCB-O6). GUI MUST NOT invent scope (D11). Surfacing excluded docs follows **TCB-O4** debug rule.

### 3.6 Unified retrieval trace (🔒 normative — TCB1.4 refine 2026-07-22)

**Problem:** Separate scope assembly for chat telemetry vs GRAG diagnostics vs benchmarks drifts over time (chat says one scope, panel another, harness a third).

**Contract:** **`RetrievalTrace`** is the **single canonical object** built once after **`RetrievalScope`** is resolved and retrieval/scoring completes (or skips). All transports are **views** that embed it.

| Piece | Role |
|-------|------|
| **`retrieval_scope`** | Resolved **`RetrievalScope`** snapshot — **TCB-O1–O5** fields |
| **`grag`** | Ranked candidates, score breakdowns, filter stats (existing GRAG diagnostics shape) |
| **`grounding`** (chat path) | Plan M attempt/success fields when applicable — **must not** duplicate scope |
| **`request_id`** | Correlates `CHAT_RAG_CONTEXT` / `CHAT_RAG_RESPONSE` / SSE / decision trace |

**Sinks (non-exhaustive):** `CHAT_RAG_CONTEXT`, `RETRIEVAL_DIAGNOSTICS`, decision-trace retrieval stages, `grag_benchmark.jsonl` where scoped — each carries **`retrieval_trace`** copied from the same builder.

**Implementation rule (TCB2):** One function (or equivalent) **`buildRetrievalTrace(...)`**; sinks add event-specific envelopes only (event name, timestamps, chat byte ratios, etc.).

### 3.7 Test invariants (🔒 normative — TCB2 Lock 2026-07-22; **TCB-X4** TCB3 Lock 2026-07-22)

Automated tests for TCB2 MUST include these as **permanent** regressions (not one-off fixtures).

| ID | Invariant |
|----|-----------|
| **TCB-X1** | **Scope beats similarity:** Under default Agent Context (`scope_type` = `default_agent_context`, **`context_policy_version` = 1**), an in-scope **`session_attachment`** MUST be retrievable while **`benchmark`** and **`system_reference`** chunks MUST NOT appear in filtered candidates **even when** their GRAG / similarity scores would outrank the attachment. **The security boundary is `RetrievalScope`, not the scorer.** |
| **TCB-X2** | **Cross-context isolation:** Attachment registered to context A MUST NOT appear in default retrieval for context B (**TCB-I1**). A and B are distinct **`active_context_key`** values per **§3.0**. |
| **TCB-X3** | **Trace parity:** Same **`request_id`** → identical **`retrieval_trace.retrieval_scope`** on **`CHAT_RAG_CONTEXT`** and **`RETRIEVAL_DIAGNOSTICS`** (**TCB-O6**). |
| **TCB-X4** | **Ingest bind (TCB3 🔒):** Create/bind a document with wire **`session_id` = S** per **§3.0** → chunks are **`session_attachment`** with **`owner_context_id` = S**; default retrieval for **`active_context_key` = S** includes them; for **`active_context_key` = T ≠ S** they MUST NOT appear (**TCB-I1**). Unbound ingest MUST NOT enter default Agent Context (**P4**). |

---

## 4. Current failure path (observed mechanism)

End-to-end path explaining **“I attached a file to my session, but answers cite global/seed knowledge.”**

### 4.1 Remote (Engine) path

1. Operator drops file → GUI stores **host path** in session `ragFilePaths` (Local Note). R1: **no** `setRagFiles` to Engine.
2. Optional **Send to Engine** → `createCorpusDocument(hostPath)` → POST body → file stored under **`/workspace/rag/`** alongside Plan L seed files → async index → chunks merged into **shared** index.
3. Operator sends chat with `session_id = S` → Engine worker `ensureSessionOnWorker(S)` → `setSessionId(S)` (memory/plan context only).
4. `CommandProcessor` / executive retrieval → `retrieveRelevant` → `retrieveChunks`.
5. Because **`activeCorpusFiles_` is empty** on Engine (remote never calls `setRagFiles` / no per-session corpus binding), **`filterCorpus == false`** → search runs over **entire index** including seed markdown.
6. GRAG ranks chunks; seeded docs (GRAG.md, AGENTS.md, HOWTO.md, cognate.md) often **outscore** a small or newly ingested attachment → model grounds on **global tier**.
7. GUI shows full **Engine Corpus** list (Phase 8) — correctly lists seed + ingested docs — reinforcing operator belief that “everything in the list is my session context,” which is **false** under current retrieval semantics.

### 4.2 Local path (partial parity)

1. `SyncAgentMemoryFromActiveSession(true)` → `setRagFiles(session.ragFilePaths)` → **active corpus filter applied**.
2. Session switch reloads paths for **local** mode; retrieval scoped to attachment paths **when sync runs**.

Local mode can still see **host sandbox** files outside session intent if paths are wrong or migrate copies overlap — lower priority than Engine regression.

### 4.3 “Uses global memory” (terminology)

Operators may mean any of:

| Interpretation | Current behavior |
|----------------|------------------|
| **RAG seed corpus** | Yes — unfiltered index search on Engine |
| **Episodic DB from other sessions** | Partially mitigated by session id on plans; warm-memory goal path may widen scope |
| **Host `chat_sessions.json` messages** | Remote: Engine conversation wins on refresh; not primary leak for RAG |

TCB Analyze treats **RAG tier leakage** as the **primary** Agent Context Boundary defect; episodic/warm cross-session reads are **secondary** — **unchanged by TCB1–TCB2 v1 index filter**; **future TCB phase required** (P3).

### 4.4 GRAG diagnostics panel (adjacent symptom)

`GragDiagnosticsPanel` updates on **`RETRIEVAL_DIAGNOSTICS`** events; chat observability uses **`CHAT_RAG_CONTEXT`**. Today payloads may omit unified scope (**TCB-O6** — locked at TCB1.4, implemented TCB2). Until then, the panel may show GRAG scoring while scope is unknown — an observability gap, not a substitute for boundary fix. **TCB2+** must emit one **`RetrievalTrace`** embedded in both events so panel and chat logs agree on **`retrieval_scope`**.

---

## 5. Recovery options (architecture alternatives)

Evaluate at **Refine/Lock**; no default implementation chosen until human Lock.

### 5.1 Metadata filtering (single index, logical tiers)

- Tag each chunk / corpus document with **`corpus_tier`** and **`owner_context_id`** (nullable for benchmark / system reference / shared policy). Attachment **`owner_context_id`** and filter semantics: **§3.0**.

**`corpus_tier` values (TCB2+, Engine-internal):**

| Tier | Meaning |
|------|---------|
| **`session_attachment`** | Operator material bound to **`owner_context_id`** / **`active_context_key`** |
| **`benchmark`** | Plan L / harness / smoke corpus (`docker/seed_rag/`, golden probes) — **“seed” in Plan L docs refers here only** |
| **`system_reference`** | System reference markdown co-located in Engine RAG workspace (e.g. shipped docs); **not** default Agent Context |
| **`legacy_orphan`** | Pre-TCB3 ingests without **`owner_context_id`** — excluded from default Agent Context until re-bound |
| **`shared_knowledge`** | Future: explicit shared corpus when locked (not Plan L default) |

- `retrieveChunks` applies filter derived from **Agent Context policy** for **`active_context_key`** (**§3.0** attachment match; exclude **`benchmark`**, **`system_reference`**, **`legacy_orphan`** from default union; later: union of §2.1 slices) ∪ optional harness override.
- **Pros:** One index; Plan L seed unchanged on disk; aligns with Phase 8 document ids.
- **Cons:** Requires ingest + seed loader changes; migration for existing chunks; strict tests for filter correctness.

### 5.2 Separate collections / indexes

- Physical split: e.g. `rag_index_session.bin` vs `rag_index_system.bin` or collection id in vector store.
- **Pros:** Hard isolation; benchmark index immutable during GUI use.
- **Cons:** Higher operational cost; merge routing in GRAG; more Engine complexity.

### 5.3 Session-scoped retrieval via active corpus list (extend today’s mechanism)

- Persist per-session list of **Engine storage keys** (from ingest `document_id`) in Engine session store.
- On `ensureSessionOnWorker`, map keys to absolute paths and call **`setActiveCorpusFiles`** (or successor) for that session only.
- **Pros:** Reuses proven `activeCorpusFiles_` filter; benchmarks already use same API.
- **Cons:** Single global `activeCorpusFiles_` on one plugin → **race** between concurrent sessions unless worker serializes **all** retrieval with session-scoped filter applied **per request** (not only at session switch); path alignment for ingested docs must be exact.

**TCB1 delivery note:** v1 Agent Context uses **§5.4** per-request filter, not global active corpus mutation. **§5.3** names the **attachment registry** (storage key / normalized path → **`owner_context_id`**) implemented in **TCB3** (Appendix D); identity binding per **§3.0**.
### 5.4 Request-scoped filter (no global active corpus mutation)

- Pass **`RetrievalScope`** into `retrieveRelevant` / `retrieveChunks` **per chat/goal request** — no shared mutable filter state. Minimum resolved fields: **`active_context_key`**, **`context_policy_version`**, **`retrieval_scope_id`**, **`scope_type`**, **`allowed_tiers`**, materialized **`selected_documents`** (see Appendix C §C.2). **`active_context_key`** per **§3.0**; **`context_policy_version` = 1**.
- **Pros:** Correct for multi-session Engine; clearest semantics.
- **Cons:** API surface change across CommandProcessor, Executive, HTTP layer; larger refactor.

### 5.5 GUI-side-only filtering

- Filter displayed chunks or prompt context in GUI — **rejected** per D11/D12 (Engine owns grounding); would violate integration principles.

### 5.6 Separate Engine instances per session

- **Rejected** for default product (ops cost); may remain dev-only.

---

## 6. Locked recovery architecture (🔒 TCB1 — 2026-07-22)

**Normative:** **§5.4** request-scoped **`RetrievalScope`**; implement via **§5.1 + §5.3** in **TCB2–TCB3**; observability via **`RetrievalTrace`** (§3.6, **TCB-O***) in **TCB2**. See **Phase TCB1 Lock Record**.

| Piece | Recommendation |
|-------|----------------|
| **Document model** | Engine-internal metadata: `corpus_tier` + **`owner_context_id`** per **§3.0**; Plan L loader marks whitelist files **`benchmark`**; workspace reference docs **`system_reference`**. |
| **Ingest** | `POST /v1/rag/documents` optional **`session_id`** (v1 wire) — binds attachment per **§3.0** (**TCB3** registry). |
| **Retrieval** | Every chat/goal call resolves **`active_context_key`** per **§3.0** via Engine policy; GRAG scoring unchanged. Every request emits one **RetrievalTrace** (§3.6, TCB-O*). |
| **Benchmarks** | Harness sets scope to benchmark tier or explicit path list (preserve TCB-B*). |
| **GUI** | On session activate + after ingest complete: refresh attachment registry from Engine (new read API or extended corpus list with tier filter); **do not** reintroduce remote `setRagFiles` with host paths. |
| **Single worker** | Session switch only updates conversation/plan context; retrieval scope comes from **request**, avoiding cross-session filter bleed. |

**Explicit non-goal for v1 boundary restore:** Multi-tenant security ACLs, cross-session document sharing, or deleting seed documents from the Engine volume.

---

## 7. Migration risks

| Risk | Impact | Mitigation (planning) |
|------|--------|------------------------|
| Existing Engine volumes with co-mingled seed + user docs | Cannot infer **`owner_context_id`** for old ingests | **TCB2 🔒:** runtime tier classification on load (Plan L whitelist → **`benchmark`**, reference basenames → **`system_reference`**, unknown → **`legacy_orphan`**); **no index file format migration** in TCB2 unless required. Re-ingest with **`session_id`** (TCB3) for authoritative bind. |
| Phase 8 corpus list contract | GUI shows all documents | Add optional filter query or separate “Session attachments” list in later locked API phase |
| Concurrent sessions on one Engine | Wrong filter if global `activeCorpusFiles_` | Request-scoped filter (recommended) |
| Path/string mismatch host vs Engine absolute paths | Filter misses ingested chunks | Canonical **storage_key** / `document_id` only — never host path on Engine |
| Benchmark/smoke regression | L2 sentinel expects seed retrieval | Harness uses explicit tier=benchmark scope, not GUI default |
| Local mode regression | Already uses `setRagFiles` | Parity tests Local vs Engine **semantics**, not necessarily same code path |
| R5 verification scripts | Assumed single corpus | Update procedure to state tier + session |
| Locked Phase 9 “no auto-ingest on drop” | Attachments still require Send (or future lock) | Boundary protocol independent of ingest UX; ingest must **register** session ownership when it occurs |

---

## 8. Implementation phases (🔒 pipeline — TCB1 Lock 2026-07-22)

**Last mandatory TCB gate:** **Appendix E.4** (TCB4 Verify). Completing **Phase TCB4 Verify Record** closes the TCB sequence for **R3+ Implement**. **TCB5** / **TCB6** do **not** gate R-series.

**Mandatory sequence for GUI restoration unblock:**

```
TCB1 Lock  ✅ 2026-07-22
    │
    ▼
TCB2 Lock  ✅ 2026-07-22
    │
    ▼
TCB2 Implement ✅ 2026-07-22
    │
    ▼
TCB2 Verify ✅ 2026-07-22
    │
    ▼
TCB3 Lock ✅ 2026-07-22 · **TCB3.1** ✅ **2026-07-23** · **Human Lock TCB3 (final)** ✅ **2026-07-23**
    │
    ▼
TCB3 Implement ✅ · TCB3 Verify ✅ 2026-07-23
    │
    ▼
TCB4 Lock ✅ 2026-07-23
    │
    ▼
TCB4 Implement ✅ 2026-07-23
    │
    ▼
TCB4 Verify ✅ 2026-07-23 — **Appendix E.4 (last mandatory TCB gate)**
    │
    ▼
R-series resumes (R3 Analyze → Lock → Implement per GUI_RESTORATION_PROTOCOL)

    (Optional — not R3+ blockers: TCB5 benchmark/smoke · TCB6 docs closeout)
```

| Phase | Goal | STOP gate |
|-------|------|-----------|
| **TCB0** | Overall protocol analyze | ✅ Locked 2026-07-22 |
| **TCB1** | Lock §3 + §6 + P1–P7 | ✅ Locked 2026-07-22 |
| **TCB2** | **Retrieval scope** — Engine core | ✅ **Locked** · ✅ **Implemented** · ✅ **Verified 2026-07-22** |
| **TCB3** | **Session ingest identity** — HTTP + registry | ✅ **Locked** **2026-07-23** · ✅ **Implemented** · ✅ **Verified** |
| **TCB4** | **GUI wiring** | ✅ **Locked** · ✅ **Implement 2026-07-23** · ✅ **Verify (E.4) 2026-07-23** — **last mandatory TCB gate** |
| **TCB5** | Benchmark / smoke non-regression (TCB-B*) | **Optional** — recommended before production; **not** an R3+ blocker |
| **TCB6** | Docs closeout (`completed_improvements_log`, cross-links) | **Optional** — may run in parallel with R-series |

**R-series rule:** Do **not** start **R3+ Implement** until **Appendix E.4** is satisfied and **Phase TCB4 Verify Record** ✅. That completes the **mandatory TCB sequence**. R3 **Analyze/Lock** may proceed in parallel with earlier TCB work if citations stay within TCB1 norms.

| Phase | Touches | Complete when |
|-------|---------|----------------|
| **TCB2** | `IndexManager`, `RAGPipeline`, `CommandProcessor`, Executive retrieval, runtime tier classify, **`RetrievalTrace`** | **TCB-X1–X3** green; seed excluded by scope; TCB-O6 parity |
| **TCB3** | `POST /v1/rag/documents`, **`attachmentOwners_` persistence**, `registerAttachmentOwner` on bind | **TCB-X4** green; registry survives reload; unbound ingest excluded from default scope |
| **TCB4** | `RemoteAgentBackend` POST, `MainFrame` session sync, copy/tooltips | **Appendix E.4** manual + **Phase TCB4 Verify Record** ✅ → **R3+** unblocked (**last TCB gate**) |
| **TCB5** | Harness, docker smoke, `run_*_benchmark` | TCB-B* checklist (**optional**) |
| **TCB6** | Docs | Human closeout (**optional**) |

**Out of scope (TCB unless new lock):** Auto-ingest on drop; GRAG weight tuning; episodic/warm isolation (**P3** — future TCB phase); multi-Engine sharding.

---

## 9. Verification strategy (cross-cutting)

| Check | Method |
|-------|--------|
| Session isolation | Two session ids, two ingested docs, cross-query |
| Seed exclusion (**TCB-X1**) | Controlled fixture: **`benchmark`** / **`system_reference`** chunks outscore attachment on similarity; default **`RetrievalScope`** still returns only in-scope attachment — **scope beats similarity** |
| Seed exclusion (operator) | Chat with only attachment — seed sentinel absent from grounded chunks |
| Benchmark path | `docker/README` L2 probe still passes under **benchmark scope** |
| Local parity | Local session A/B file sets retrieve independently |
| R-series gate | R5 script declares **harness scope** (benchmark tier), not default Agent Context |
| Scope trace | Each chat/goal retrieval builds one **`RetrievalTrace`**; **`CHAT_RAG_CONTEXT`** and **`RETRIEVAL_DIAGNOSTICS`** carry identical **`retrieval_trace`** (scope + **`context_policy_version`**) per TCB-O* |

---

## 10. Document control

| Field | Value |
|-------|-------|
| Owner | Human architect + implementing agent per TCB phase |
| Code changes | **Forbidden** until TCB phase locked + `AGENTS.md` implement approval |
| GUI R-series | **R3+ Implement** only after **Appendix E.4** + **Phase TCB4 Verify Record** ✅ (**last mandatory TCB gate**); R3 Analyze/Lock may parallel earlier TCB (R1–R2 unchanged) |
| Normative companions | `GUI_integration.md` (ownership), `plan_l_workspace_corpus.md` (seed tier), `GRAG.md` (scoring unchanged) |

---

## Appendix A — Code anchors (2026-07-22 analyze)

| Concern | Location |
|---------|----------|
| Active corpus filter | `IndexManager::setActiveCorpusFiles`, `retrieveChunks`, `chunkInActiveCorpus` |
| Local session RAG bind | `BasicAgentPlugin::setRagFiles` → `setActiveCorpusFiles` |
| Remote RAG no-op | `RemoteAgentBackend::setRagFiles`, `AgentInterface::setRagFiles` guard |
| GUI skip remote memory/RAG sync | `MainFrame::SyncAgentMemoryFromActiveSession` |
| Engine session switch | `EngineRuntime::ensureSessionOnWorker` → `BasicAgentPlugin::setSessionId` |
| Ingest without session bind | `IndexManager::createCorpusDocument` |
| Chat retrieval | `CommandProcessor` → `rag.retrieveRelevant` |
| Plan L seed tier | `docker/seed_rag/` whitelist, `plan_l_workspace_corpus.md` |

---

## Appendix C — TCB2 Implementation Plan (🔒 Locked 2026-07-22 · **TCB2.1** 2026-07-23)

**Status:** 🔒 **Locked 2026-07-22** · **TCB2.1** doc refine **2026-07-23** (inherit **§3.0** — see **Phase TCB2 Lock Record**)

**Context identity:** **§3.0** only (**TCB-ID1**). This appendix specifies **TCB2 retrieval architecture** — **`RetrievalScope`**, runtime classification, per-request filter, **`RetrievalTrace`**, tests. It does **not** define **`active_context_key`**, **`owner_context_id`**, or v1 **`session_id`** mapping.

**Locks (unchanged at TCB2.1):** **`context_policy_version`**, **`RetrievalTrace`**, runtime index classification, **TCB-X1–X3**. **Code:** **`AGENTS.md` Implement** approval required (TCB2 ✅).

### C.0 Index persistence (🔒 open decision resolved at lock)

Index migrations are high-risk and **not required** to restore Agent Context behavior. **TCB2** uses:

```
Load existing index (unchanged on-disk format)
        │
        ▼
Classify metadata at runtime (corpus_tier, owner_context_id where inferrable)
        │
        ▼
Apply per-request RetrievalScope filter
```

| Rule | Detail |
|------|--------|
| **Do** | Reclassify on load + at retrieval using Plan L whitelist, path/registry hints, TCB3 registry when present |
| **Do not** | Force a persistent index format migration in TCB2 |
| **Defer** | Optional v2 on-disk metadata fields on **`saveIndex`** — only when a later locked phase requires it |
| **Exception** | On-disk change **only if** strictly required for correctness (must be called out in implement plan + human approval) |

### C.1 Objective

Implement per-request **`RetrievalScope`** (§5.4) so default chat/goal retrieval applies:

- The **§3.0** default attachment filter for the resolved **`active_context_key`**;
- **§5.1** tier exclusions for default Agent Context (**TCB-R2**, **P1**): no **`benchmark`** / **`system_reference`** / unbound **`legacy_orphan`** in the default union.

**Scope resolution (TCB-P1):** Chat/goal entry points supply **`active_context_key`** per **§3.0** to **`resolveAgentContextRetrievalScope(...)`** — no separate TCB2 identity rule.

### C.2 RetrievalScope (implementation contract)

Per-request **`RetrievalScope`** is the policy input to retrieval: output of **`resolveAgentContextRetrievalScope(active_context_key)`** (TCB-P1), input to **`retrieveChunks`** / **`retrieveRelevant`**.

Observability MUST NOT re-serialize scope separately per sink — see **§3.6 `RetrievalTrace`** (nested **`retrieval_scope`**).

**Required fields (`retrieval_scope` snapshot):**

| Field | Role |
|-------|------|
| **`retrieval_scope_id`** | Stable id for this resolved scope instance (trace / replay) |
| **`context_policy_version`** | Integer rule-set id — **which policy union produced this scope** |
| **`active_context_key`** | Required scope snapshot field; semantics **§3.0** |
| **`scope_type`** | e.g. `default_agent_context`, `harness_override`, `benchmark_explicit` (TCB-O2) |
| **`allowed_tiers`** | Corpus tiers eligible for filter (TCB-O3) |
| **`selected_documents`** | Document ids/names materialized into scope (TCB-O4) |

**Example (`retrieval_scope` inside `RetrievalTrace`):**

```json
{
  "retrieval_trace": {
    "request_id": "req_abc",
    "retrieval_scope": {
      "retrieval_scope_id": "rs_20260722_abc123",
      "context_policy_version": 1,
      "active_context_key": "<active_context_key>",
      "scope_type": "default_agent_context",
      "allowed_tiers": ["session_attachment"],
      "selected_documents": ["doc_attachment_1"]
    },
    "grag": { "breakdowns": [] }
  }
}
```

Example placeholder **`<active_context_key>`**: v1 value per **§3.0** (wire may still use **`session_id`** on HTTP/worker).

Event envelopes (`CHAT_RAG_CONTEXT`, `RETRIEVAL_DIAGNOSTICS`) wrap the same **`retrieval_trace`** object; only event-specific fields differ outside it.

**Why `context_policy_version`:** Agent Context will grow beyond attachments-only. Logs and benchmarks must record **which rule set** was active, not only which documents were selected.

| **`context_policy_version`** | Policy union (direction) |
|------------------------------|---------------------------|
| **1** (TCB2) | Policy union **1**: **§3.0** attachment slice; exclude **§5.1** non-default tiers (**`benchmark`**, **`system_reference`**, **`legacy_orphan`**) from default union |
| **2** (future) | Version **1** + **approved long-term / approved memory** slices per policy |
| **3** (future) | Version **2** + **agent learned memory** (promoted / consolidated retrieval) |

Resolver **`resolveAgentContextRetrievalScope`** MUST set **`context_policy_version`** from a single Engine constant/registry for the active rule set. Raising the version is a **locked protocol / phase** change, not a silent behavior drift.

### C.2.1 RetrievalTrace (observability contract)

| Item | Detail |
|------|--------|
| **Builder** | One **`buildRetrievalTrace(...)`** (name illustrative) per retrieval call |
| **Embed key** | **`retrieval_trace`** on **`CHAT_RAG_CONTEXT`**, **`RETRIEVAL_DIAGNOSTICS`**, and aligned decision-trace / benchmark rows |
| **TCB-O6 test** | Byte-identical **`retrieval_scope`** (or full **`retrieval_trace`**) across sinks for the same **`request_id`** |
| **GUI** | `GragDiagnosticsPanel` reads scope from **`retrieval_trace.retrieval_scope`**; no parallel scope fields on the event root |

### C.3 In scope (TCB2)

Identity inputs and filter semantics inherit **§3.0** and **§5.1**; rows below are **TCB2 delivery** only.

| Item | Detail |
|------|--------|
| **TCB-P1** | `resolveAgentContextRetrievalScope(...)` → **`RetrievalScope`**; caller supplies **`active_context_key`** per **§3.0** |
| **Filter** | Request-scoped filter in `retrieveChunks` / `retrieveRelevant` — **not** global `activeCorpusFiles_` alone |
| **Tiers** | v1 policy **1**: allowed tiers per **§5.1** + **§3.0** default union; deny excluded tiers in default Agent Context |
| **TCB-P2 / TCB-O*** | **`buildRetrievalTrace`** → embed on all sinks; scope fields only inside **`retrieval_trace.retrieval_scope`** |
| **P5** | Local plugin: same request-scoped policy as Engine (**§3.0** at entry) |
| **P6** | Harness explicit **`benchmark`** / override scope unchanged (TCB-R4) |
| **Tests** | Permanent: **TCB-X1** (scope beats similarity), **TCB-X2**, **TCB-X3**; isolation; harness **TCB-B*** |

### C.4 Out of scope (defer)

| Item | Phase |
|------|-------|
| POST ingest **`session_id`** / registry bind | **TCB3** |
| GUI Send wiring, corpus labels | **TCB4** |
| Warm episodic vector union (P3) | Future TCB phase |
| GRAG weight changes | — |

### C.5 Verify gate (before TCB3)

- **TCB-X1** passes with benchmark chunks scored higher than attachment but excluded by filter.
- Two **`active_context_key`** values per **§3.0**: attachment in A not retrieved for B’s default scope (**TCB-X2**).
- Chat with attachment-only context: seed sentinel absent from **`selected_documents`**.
- **TCB-X3:** **`retrieval_trace`** parity across chat + GRAG sinks.
- Benchmark smoke still passes under **harness / benchmark** scope (TCB-B*).

### C.6 Post-lock

- [x] Human Lock TCB2 **2026-07-22**
- [x] **TCB2 Implement** — **2026-07-22** (Engine: `agent_context_retrieval`, scope filter, `RetrievalTrace`, TCB-X*)
- [x] **TCB2 Verify** — **2026-07-22** (see **Phase TCB2 Verify Record**)
- [x] **TCB2.1** — **2026-07-23** (Appendix C inherit **§3.0**; see **Phase TCB2 Lock Record**)
- [ ] **TCB3 Implement** — `AGENTS.md` approval (Appendix D)

---

## Appendix D — TCB3 Implementation Plan (🔒 Locked 2026-07-23)

**Status:** 🔒 **Locked 2026-07-23** (Appendix D **2026-07-22** + **TCB3.1** **2026-07-23** — see **Phase TCB3 Lock Record**)

**Context identity:** **§3.0** only (**TCB-ID1**). This appendix specifies **TCB3 ingest delivery** — HTTP optional **`session_id`**, **§5.3** registry persistence, classification, **TCB-X4**. It does **not** redefine **`active_context_key`**, **`owner_context_id`**, or v1 wire mapping (see **§3.0** §4 table and ingest-without-binding).

**Code:** **`AGENTS.md` Implement** approval required.

### D.0 Relationship to TCB2

**TCB2** ships request-scoped **`RetrievalScope`**, runtime tier classification, and in-memory **`IndexManager::registerAttachmentOwner`**. **TCB3** makes **§3.0** ingest bind **authoritative and durable**: optional **`session_id`** on **`POST /v1/rag/documents`**, persist **§5.3** registry, prove **TCB-X4**. Retrieval filter semantics are **unchanged** (**§5.4** + **§3.0** at query time).

### D.1 Objective

Ingest via **`POST /v1/rag/documents`** (Phase 9 path unchanged):

| Condition | **TCB3 behavior (explicit)** |
|-----------|------------------------------|
| **Optional `session_id` = S** (non-empty string on request body) | After successful document finalize: apply **§3.0** ingest bind for **S** → **`registerAttachmentOwner(path, …)`** per **§5.3**; persist registry; classify path **`session_attachment`** (**§5.1**). Default retrieval may include chunks for that bind only per **§3.0** (with **TCB2** **`RetrievalScope`**). |
| **Omit `session_id` or empty string** | Ingest still **`accepted`** (Phase 9 JSON unchanged); **no** registry bind; material **`legacy_orphan`** for default Agent Context until re-ingest with bind (**P4**, **§3.0** ingest-without-binding). |
| **Storage / index** | Atomic write + async index as today; **no** `rag_index.bin` format change. |

### D.2 HTTP contract (🔒)

| Item | Rule |
|------|------|
| Path | **`POST /v1/rag/documents`** (unchanged) |
| Required body | **`content`** (string); **`name`** optional (unchanged) |
| New optional field | **`session_id`** — optional non-empty string on request body (v1 wire name unchanged). When present: trigger **§3.0** ingest bind + **D.1** / **D.3** registry. Identity mapping: **§3.0** only. |
| Acceptance response | **Unchanged** Phase 9 JSON (`schema_version`, `status: accepted`, `document.id`, `document.name`) — **no** blocking on indexing completion |
| Validation | Reject malformed **`session_id`** type (non-string). Empty string treated as **omit** (no bind). **Do not** require session to exist in conversation store for bind (opaque context keys allowed for tests; GUI TCB4 will send active session id). |
| Errors | Invalid body → existing **`invalidRequest`** paths; bind failure after successful write MUST NOT leave orphan file without defined behavior — implement plan MUST either roll back file or register failure outcome (prefer: bind immediately after successful rename, before async index). |

### D.3 Attachment registry (§5.3 delivery — 🔒)

**Registry behavior (explicit, unchanged at TCB3.1):**

| Item | Rule |
|------|------|
| In-memory | Continue **`attachmentOwners_`**: normalized absolute path → **`owner_context_id`** (value from **§3.0** bind when **`session_id`** supplied on POST) |
| API | **`registerAttachmentOwner(path, owner_context_id)`** after successful document finalize |
| Persistence | JSON registry under agent workspace (e.g. `agent_workspace/rag_attachment_registry.json`) — load on **`IndexManager`** init, save after register; **no** `rag_index.bin` format change |
| Keys | Normalized absolute path of stored file under RAG root (same key used by classification on load) |
| Reload | On startup: load registry → **`classifyAllChunksMetadata`** so **`legacy_orphan`** rebound paths become **`session_attachment`** |

### D.4 Code touch list (implement plan input)

| Layer | Change |
|-------|--------|
| `engine_http_transport.cpp` | Parse optional **`session_id`**; pass to runtime |
| `EngineRuntime` / `BasicAgentPlugin::createCorpusDocument` | Optional bind parameter |
| `IndexManager::createCorpusDocument` | Call **`registerAttachmentOwner`** + persist when bind present |
| Local ingest parity | Same optional bind on local **`createCorpusDocument`** path (**P5**) when caller supplies context key |
| Tests | **`testTcb3IngestBind`** / **`testTcb3IngestOmitSession`** / registry reload; extend **`testEngineHttpCreateDocumentEndpoint`**; permanent **TCB-X4** |

### D.5 Out of scope (TCB3)

| Item | Phase |
|------|-------|
| GUI **`session_id`** on Send | **TCB4** |
| Corpus list filtered by session / new read API | Later locked API (§6 GUI row) |
| Admin re-bind for **`legacy_orphan`** | Future phase (re-ingest with **`session_id`** is v1 mitigation) |
| Chunk metadata in **`rag_index.bin`** | Deferred (runtime + registry sufficient for v1) |

### D.6 Verify gate (before TCB4)

- **TCB-X4** green (bind + cross-context exclusion on create path).
- **TCB-X1–X3** remain green (no regression).
- Unbound ingest excluded from default **`RetrievalScope`** per **§3.0** / **TCB2** (any **`active_context_key`**).
- Registry survives Engine restart (reload test).
- **`testEngineHttpCreateDocumentEndpoint`** + docker smoke item 15 still pass (acceptance semantics unchanged).
- Harness / **TCB-B*** unchanged under explicit benchmark scope.

### D.7 Post-lock

- [x] Human Lock TCB3 **2026-07-22**
- [x] **TCB3.1** — **2026-07-23** (Appendix D inherit **§3.0**)
- [x] **Human Lock TCB3 (final)** — **2026-07-23**
- [x] **TCB3 Implement** — **2026-07-23**
- [x] **TCB3 Verify** — **2026-07-23** (see **Phase TCB3 Verify Record**)

---

**Next human action:** **Appendix E.4** (last mandatory TCB gate) — **Phase TCB4 Verify Record** → **R-series resumes** (**R3+ Implement**).

---

## Appendix E — TCB4 Implementation Plan (🔒 Locked 2026-07-23)

**Status:** 🔒 **Locked 2026-07-23** (see **Phase TCB4 Lock Record**)

**Context identity:** **§3.0** only (**TCB-ID1**). This appendix specifies **GUI delivery** — wiring **Send to Engine** to **TCB3** ingest bind. It does **not** redefine **`active_context_key`**, **`owner_context_id`**, or v1 mapping.

**Code:** **`AGENTS.md` Implement** approval required.

### E.0 Relationship to TCB3

**TCB3** accepts optional **`session_id`** on **`POST /v1/rag/documents`** and persists **§5.3** registry bind. **TCB4** closes the **remote GUI gap**: **`RemoteAgentBackend::createCorpusDocument`** must include **`session_id`** from **backend session identity** when available. **Local** path already binds via active session on plugin when **`setSessionId`** has run (**P5** parity check in verify).

### E.1 Session identity on ingest (🔒)

| Rule | Detail |
|------|--------|
| **Authoritative** | **Backend session identity** (`IAgentBackend` / **`RemoteAgentBackend::session_id_`** after **`setSessionId`**) is the **only** source for POST **`session_id`**. |
| **GUI role** | **Tab activation** (`ActivateSession` → **`AgentInterface::setSessionId`**) **synchronizes** backend session identity **before** chat, goals, and **Send to Engine**. Send **does not** invent a parallel session key. |
| **POST body** | When backend session identity is **available** (non-empty after trim), JSON includes **`session_id`**; otherwise omit (**TCB3** / **P4**). |
| **Alignment** | Same id as **`/v1/chat`** / **`/v1/goals`** for that tab per **§3.0** v1 wiring. |

### E.2 In scope (TCB4 delivery)

| Item | Detail |
|------|--------|
| **Remote POST** | Add **`session_id`** from backend session identity to **`POST /v1/rag/documents`** |
| **Session sync** | Ensure backend session is synced on tab activate (existing); idempotent sync before Send allowed |
| **Local parity** | Confirm local **`createCorpusDocument`** still binds via active session (**TCB3** path) |
| **R1 preserve** | No remote **`setRagFiles`**; no auto-ingest on drop |
| **UX (minimal)** | Copy/tooltip: Send binds to **current session**; Engine Corpus list = **full inventory**, not default retrieval scope |
| **Tests (implement)** | Backend unit: POST JSON includes **`session_id`** when backend session set; optional stack test |

### E.3 Out of scope (TCB4)

| Item | Phase |
|------|-------|
| Corpus list session / tier filter API | Later locked API (§6 GUI row) |
| **`GragDiagnosticsPanel`** scope UI beyond **`retrieval_trace`** | R5 adjacency |
| Engine / TCB3 contract changes | — |
| Auto-ingest on drop | R1 / Phase 9 |

### E.4 Verify gate — **last mandatory TCB gate** (unblocks R3+ Implement)

**Normative:** Human approval of this gate and a **Phase TCB4 Verify Record** in this protocol **completes the mandatory TCB restoration sequence**. **TCB5** (benchmark/smoke) and **TCB6** (docs closeout) are **not** required before **R3+ Implement**; they may run afterward or in parallel with R-series.

**Automated (implement plan):**

- Remote **`createCorpusDocument`** includes **`session_id`** when backend session identity is set.
- **`thoth-core-tests`** green (**TCB-X1–X4**, Phase 9 / HTTP smoke).
- Optional: **`testTcb4RemotePostSessionId`** (name illustrative).

**Manual — positive (required):**

1. Session **A**: **Send** distinctive document **A**; wait indexed.
2. Session **A**: chat query answerable only from **A** → grounds on attachment (not silent seed-only default).

**Manual — cross-context negative (required):**

1. Session **A**: **Send** document **A** (unique content token).
2. Session **B**: activate tab (backend session **B**); **do not** Send **A**.
3. Session **B**: ask about **A**’s unique content.

**Expected:**

- **No retrieval hit** for **A**’s attachment under **B**’s **`active_context_key`** (**TCB-I1**).
- **No seed/benchmark fallback** pretending to answer (**TCB-R3**, **TCB-R2**) — e.g. **`no_retrieval_hits`** / empty inject, not default corpus grounding.

**Rationale (locked):** Original defect = **one context accidentally retrieving another** (shared index), not merely “file failed to index.”

**Not in this gate:** Docker L2 full operator regression (ops smoke); optional duplicate of R2 Scenario D.

### E.5 Post-lock

- [x] Human Lock TCB4 **2026-07-23**
- [x] **TCB4 Implement** — **2026-07-23** (see **Phase TCB4 Implement Record**)
- [x] **TCB4 Verify (Appendix E.4 — last mandatory TCB gate)** — **2026-07-23** (see **Phase TCB4 Verify Record**)

---

STATUS: **TCB mandatory sequence complete** ✅ 2026-07-23 — **R3+ Implement** unblocked per §8 (**TCB5/TCB6 optional**)
