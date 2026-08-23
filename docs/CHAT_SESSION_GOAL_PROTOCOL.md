# Chat Session Goal Retrieval Protocol

**Document type:** Architecture protocol (session goal → Engine chat retrieval)  
**Status:** **CSG-A** 🔒 **LOCKED** **2026-07-30** · **A.1–A.3 ✅ Implement 2026-07-30** · A.4 manual verify pending  
**Created:** 2026-07-30  
**Related:** [`GUI_RESTORATION_PROTOCOL.md`](GUI_RESTORATION_PROTOCOL.md) (R3-G1 banner/cache vs Engine) · [`THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md`](THOTH_AGENT_CONTEXT_BOUNDARY_PROTOCOL.md) · [`GUI_integration.md`](GUI_integration.md) · [`GRAG.md`](GRAG.md) · [`AGENTS.md`](../AGENTS.md)

---

## Purpose

Define how the **Engine** resolves goal context for **chat-turn retrieval** when the GUI banner shows an active session goal but the ExecutiveController has no live plan (typical after GUI restart).

**Problem:** Host `active_goal` in `chat_sessions.json` restores the banner (R3-G1 presentation cache). Chat requests today send only `{ session_id, content }`. Engine syncs goal embeddings only when ExecutiveController has a non-empty `plan_id`. After restart, retrieval runs in conversational mode (`scoring_type=rag_hybrid`, direction magnitude 0, UI "N/A (chat)") even though the user sees a goal.

**This protocol locks the fix:** optional `active_goal` on chat turns + Engine-side goal resolver with explicit `goal_source` diagnostics.

Implementation requires explicit human approval per `AGENTS.md`. **No code changes are authorized by the protocol alone.**

---

## CSG-A Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-30** |
| Kind | **Session goal on chat turns** — resolver precedence, session-scoped embed cache, diagnostic truth, phased implementation |
| Normative | §1–§8 · **INV-CSG-1–INV-CSG-6** |
| Input | R6 audit chat/goal analysis · R3-G1 banner semantics · user lock review (cache scope + `goal_source`) |
| Supersedes | Implicit assumption that banner goal implies Engine directional GRAG after restart |
| Out of scope (CSG-A) | Response regurgitation (Phase B) · GRAG scoring formula changes · Engine persistence of session goals · `/sessions/{id}/goal` hydrate endpoint |
| Post-lock rule | Do not revise locked sections without **new CSG lock**; numeric cache thresholds may be tuned in implementation if documented |
| Next | **Implement A.1** → A.2 → A.3 → A.4 per §6 |

---

## §1 — Objective

When the GUI banner shows `activeGoal`, chat retrieval **must** use directional GRAG after GUI restart **without** re-running `executeGoal`.

**Success (A.4 manual verify):**

| Field | Expected |
|-------|----------|
| `goal_source` | `"session"` |
| `goal_present` | `true` |
| `scoring_type` | `grag_hybrid` or `grag_blended_hybrid` |
| `direction_magnitude` | `> 0` |
| GRAG panel Alpha | Numeric, not "N/A (chat)" |

---

## §2 — Goal resolution precedence

| Priority | Source | `goal_source` |
|----------|--------|---------------|
| 1 | ExecutiveController (`plan_id` non-empty **and** non-empty `goal_embedding_`) | `"executive"` |
| 2 | Request `active_goal` (trimmed, non-empty) | `"session"` |
| 3 | Neither | `"none"` |

**Rule:** Executive wins when both executive embeddings and request `active_goal` are present.

**Resolver API (normative):**

```cpp
struct ChatRetrievalGoal {
    std::vector<float> embedding;  // empty when unresolved
    std::string source;            // "executive" | "session" | "none"
    std::string error;             // non-empty when session goal embed fails
};

ChatRetrievalGoal resolveChatRetrievalGoal(
    ExecutiveController* controller,
    const std::string& session_id,
    const std::optional<std::string>& active_goal,
    SessionGoalEmbedCache& cache,
    EmbeddingEngine& embed);
```

---

## §3 — Diagnostic truth (required)

Retrieval diagnostics **must** expose `goal_source` on every chat retrieval that emits `RETRIEVAL_DIAGNOSTICS`.

**Example:**

```json
{
  "goal_present": true,
  "goal_source": "session",
  "direction_magnitude": 0.42,
  "scoring_type": "grag_hybrid"
}
```

**Implementation requirements:**

1. Add `goal_source` to `GragDiagnostics` and `to_json()`.
2. Set from `ChatRetrievalGoal.source` in `resolveChatRetrievalGoal()`.
3. Propagate through `processQuery` → `RETRIEVAL_DIAGNOSTICS` event → GUI panel / logs.
4. When `goal_source=session` but embed fails: `goal_present=false`, include error reason in diagnostics/trace — **no silent fallback** without trace.

**Invariant INV-CSG-1:** Operators must be able to answer "where did this goal come from?" from diagnostics alone, without inferring from side effects.

---

## §4 — SessionGoalEmbedCache

**Scope:** session lifetime, **not** global Engine lifetime.

```
SessionGoalEmbedCache
  key:   (session_id, normalized_goal_hash)
  value: { embedding: vector<float>, cached_at_ms: int64 }
  scope: entries belong to one session_id bucket
```

### Normalization (before hash)

1. Trim leading/trailing whitespace.
2. Collapse internal whitespace (align with GUI `TrimGoalForDisplay` where possible).
3. Hash UTF-8 bytes of normalized text (e.g. SHA-256 truncated to 64-bit hex — same pattern as other Thoth hashes).

### Rules

| Rule | Detail |
|------|--------|
| **INV-CSG-2** | No cross-session reuse — same `session_id` string with different goal text → different hash → different entry |
| **INV-CSG-3** | Cache is optimization only; `active_goal` on each request remains authoritative |
| **INV-CSG-4** | Per-session bucket eviction when bucket size > N (default **4**, oldest first) |
| **INV-CSG-5** | Optional TTL may be added in implementation if trivial; document if used |

**Rationale:** Multiple GUI clients may share Engine with overlapping session id strings but different goals. Global or cross-session cache would cause accidental embedding reuse.

**Files:**

- `external/basic_agent/include/chat_retrieval_goal.h`
- `external/basic_agent/src/chat_retrieval_goal.cpp`

---

## §5 — API contract

### Request field (optional)

Thread optional `active_goal` through:

| Endpoint | Field |
|----------|-------|
| `POST /v1/conversation/turns` | `active_goal` (string, optional) |
| `POST /v1/chat` | `active_goal` (string, optional) — parity |

Document in `conversation_authority.h`.

### Pass-through chain

```
HTTP → EngineRuntime → BasicAgentPlugin → processQuery(..., active_goal)
```

### GUI (A.3 only, after A.2 verified)

`MainFrame` chat send includes `session.activeGoal` when non-empty → `AgentInterface` → local/remote backends → HTTP JSON.

**Invariant INV-CSG-6:** Engine does **not** persist session goals; host `chat_sessions.json` remains source of truth for banner/cache (R3-G1).

---

## §6 — Implementation order (locked)

```
A.1 → A.2 → A.3 → A.4
```

Each sub-phase requires `AGENTS.md` gate approval before starting. Do not skip or reorder.

### A.1 — Engine resolver only (no GUI)

**Deliver:**

- `chat_retrieval_goal.{h,cpp}` including `SessionGoalEmbedCache`
- Wire into `command_processor.cpp` (resolver + `goal_source` on diagnostics)
- Add `chat_retrieval_goal.cpp` to `external/basic_agent/CMakeLists.txt`

**Unit tests (`tests/unit_tests.cpp`):**

| Test | Assert |
|------|--------|
| Executive wins | Executive embedding used; `goal_source=executive` |
| Session fallback | No executive plan; `active_goal` set → embed + `goal_source=session` |
| None | No plan, no `active_goal` → `goal_source=none`, empty embedding |
| Cache isolation | Same `session_id`, two goals → two cache entries |
| Diagnostic JSON | `goal_source` present in diagnostics serialization |

**Gate:** `thoth-core-tests` green before any HTTP/GUI work.

### A.2 — Thread `active_goal` through Engine API

- Optional `active_goal` on `/v1/conversation/turns` and `/v1/chat`
- Pass through runtime/plugin/command processor

**Verify:** curl/manual HTTP with `active_goal` → 200; logs show `goal_source=session`, `goal_present=true`.

### A.3 — GUI sends banner goal

- Only after A.2 verified
- `MainFrame.cpp`, `AgentInterface.{h,cpp}`, `i_agent_backend.h`, `local_agent_backend.cpp`, `remote_agent_backend.cpp`

### A.4 — Full restart test (manual)

1. Start Engine  
2. Open GUI → create goal → confirm banner  
3. **Restart GUI** (Engine may keep running)  
4. Send question (no re-execute goal)  
5. Confirm §1 success criteria

---

## §7 — Files by sub-phase

| Phase | Files |
|-------|-------|
| A.1 | `chat_retrieval_goal.{h,cpp}`, `grag_diagnostics.h` (or equivalent), `command_processor.cpp`, `CMakeLists.txt`, `tests/unit_tests.cpp` |
| A.2 | `engine_http_transport.cpp`, `engine_runtime.{h,cpp}`, `basic_agent_plugin.{h,cpp}`, `command_processor.h`, `conversation_authority.h` |
| A.3 | `MainFrame.cpp`, `AgentInterface.{h,cpp}`, `i_agent_backend.h`, `local/remote_agent_backend.{h,cpp}` |

---

## §8 — Phase B (separate — see CSG-B)

**Response regurgitation** (model echoing `Document:` / `source_span=` injection format) is **out of scope** for CSG-A.

**Locked separately:** [`CHAT_RESPONSE_REGURGITATION_PROTOCOL.md`](CHAT_RESPONSE_REGURGITATION_PROTOCOL.md) **CSG-B 🔒 2026-07-30** — generation pipeline only (prompt + assess + quality-gated sanitize/retry). No implementation until explicit approval.

---

## Relationship to GUI Restoration R3-G1

| R3-G1 | CSG-A |
|-------|-------|
| Host `active_goal` is **presentation cache** for banner | Host `active_goal` is **also** passed on chat turns for retrieval |
| Engine owns cognition | Engine resolves goal via precedence §2; does not mirror host file |

CSG-A **extends** R3-G1 without changing banner ownership.

---

## Verify checklist (post-implementation)

- [ ] A.1 unit tests pass
- [ ] A.2 curl: `active_goal` → `goal_source=session`
- [ ] A.3 GUI sends goal on chat
- [ ] A.4 restart sequence: §1 success criteria
- [ ] `RETRIEVAL_DIAGNOSTICS` events include `goal_source` in `decision_trace.jsonl`
- [ ] Embed failure path logged (no silent conversational fallback)
