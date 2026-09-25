# Chat Response Regurgitation Protocol

**Document type:** Architecture protocol (chat generation — RAG scaffold echo prevention)  
**Status:** **CSG-B** 🔒 **LOCKED** **2026-07-30** · **B.1–B.3 ✅ Implement 2026-07-30** · **B.4 ✅ Manual acceptance 2026-09-13**
**Created:** 2026-07-30  
**Related:** [`CHAT_SESSION_GOAL_PROTOCOL.md`](CHAT_SESSION_GOAL_PROTOCOL.md) (CSG-A · separate concern) · [`GUI_RESTORATION_PROTOCOL.md`](GUI_RESTORATION_PROTOCOL.md) · [`GRAG.md`](GRAG.md) · [`AGENTS.md`](../AGENTS.md)

---

## Purpose

Define how the **chat generation pipeline** prevents assistant replies from exposing internal RAG chunk injection formatting (`Document:`, `source_span=`, `---` separators) or from returning de-scaffolded pasted retrieval context in place of a natural-language answer.

**Problem:** The model sometimes echoes the internal `[RAG Context]` scaffold in user-visible replies — e.g. repeated blocks like:

```text
Document: architectural_facts.md
source_span=29-33
…
---
Document: architectural_facts.md
…
```

Retrieval and GRAG scoring may be functioning correctly; the failure is **generation / response handling**.

**This protocol locks the fix:** prompt guidance + conservative scaffold assessment + quality-gated sanitize/retry + generation diagnostics. **Retrieval injection format is unchanged.**

Implementation requires explicit human approval per `AGENTS.md`. **No code changes are authorized by the protocol alone.**

---

## CSG-B Lock Record 🔒

| Field | Value |
|-------|-------|
| Locked | **2026-07-30** |
| Kind | **Chat response regurgitation handling** — prose answers, quality-gated sanitize, one regurgitation retry, observability |
| Normative | §1–§12 · **INV-CSG-B-1–INV-CSG-B-6** |
| Input | R6 / post-CSG-A chat audit · captured `architectural_facts.md` failure · user lock review (sanitize objective, success criteria, quality-based retry) |
| Supersedes | Implicit assumption that transcript-only sanitize (Plan N) is sufficient for RAG chunk echo |
| Out of scope (CSG-B) | Retrieval ranking · retrieval diversity · GRAG scoring · grounding thresholds · CSG-A goal hydration · `formatChunkForPrompt()` / prompt injection formatting · new retrieval heuristics |
| Post-lock rule | Do not revise locked sections without **new CSG lock**; Layer 2 paste thresholds may be tuned in implementation if documented and tests updated |
| Next | **Implement B.1** → B.2 → B.3 → B.4 manual verify per §11 |

---

## §1 — Objective

Assistant chat replies must be **natural conversational prose** that answers the user's question. The user must never see internal retrieval scaffolding or a collage of pasted chunk bodies with headers stripped.

**Primary outcomes (in order):**

1. Return a valid natural-language assistant answer.
2. If not obtainable on first generation, perform **one** regurgitation retry with a plain-prose reminder.
3. If still failing, use **existing** Plan N fallback behavior (`kFallbackGreeting` / `kFallbackGeneric`).

---

## §2 — Invariants

| ID | Invariant |
|----|-----------|
| **INV-CSG-B-1** | Internal RAG injection format (`formatChunkForPrompt`, `Document:`, `source_span=`) **must not change**. |
| **INV-CSG-B-2** | Document names and source spans remain available in prompts and retrieval diagnostics for grounding — not in user-visible replies. |
| **INV-CSG-B-3** | Inline filename mentions in prose (e.g. `architectural_facts.md`) **must remain valid** and must not be classified as regurgitation. |
| **INV-CSG-B-4** | Existing Plan N transcript-loop protection (`sanitizeChatAssistantText`, `[User]`/`[Agent]`/`[RAG Context]`) **must remain intact**. |
| **INV-CSG-B-5** | At most **one additional retry** for regurgitation beyond existing Plan N empty/stop-free retry. |
| **INV-CSG-B-6** | Sanitization **must not** be used to deliver cleaned chunk bodies when the result is not a complete assistant answer (§4). |

---

## §3 — Success criteria

A **successful** user-visible response satisfies **all** of:

1. **Answers the user's question** (topical relevance to the query).
2. **Reads as normal conversational prose** (synthesis, not a documentation excerpt collage).
3. **May naturally mention filenames** when helpful (inline, not `Document:` header format).
4. **Contains no retrieval scaffold** (`Document:`, `source_span=`, chunk `---` separators).
5. **Does not primarily consist of copied retrieval context** (chunk bodies pasted with or without headers).

These criteria guide **both** sanitization acceptance and retry decisions (§8).

**Verification note:** Success is **not** measured by absence of `Document:` headers alone. A de-scaffolded chunk dump is still a failure.

---

## §4 — Sanitization objective

Sanitization exists to **reveal** an otherwise valid assistant answer hidden under retrieval scaffold — **not** to expose cleaned chunk bodies to the user.

| Situation | Action |
|-----------|--------|
| Scaffold removed → remaining text is a **complete assistant answer** (§6 Layer 2 pass) | **Return** sanitized text |
| Scaffold removed → remaining text is **largely pasted retrieval context** (§6 Layer 2 fail) | **Do not return** — treat as regurgitation → one retry (§8) |
| Scaffold cannot be removed safely / reassess still detects scaffold | One retry (§8) |
| No scaffold ever present | Existing Plan N path; Layer 2 optional only when paste signals are extreme (implementation conservatively defaults to pass) |

**Forbidden:** Returning stripped chunk text that reads as indexed-document paste rather than an assistant reply.

---

## §5 — Assessment Layer 1 (scaffold only)

**Scope:** Identify **retrieval injection structure** only. Conservative, line-anchored, structure-aware.

### Positive signals (line start after horizontal whitespace trim)

| Signal | Pattern |
|--------|---------|
| Document header | `Document:` |
| Source span | `source_span=` |
| Chunk separator | Whole line equals `---` |

### Explicit non-signals (must not alone trigger regurgitation)

- Inline filename references (`architectural_facts.md`, `See GRAG.md`)
- Quoted markdown within prose
- JSON or structured tool output
- Ordinary uses of the word "document" / "Document" **within** prose (not line-anchored header)

### API (normative)

```cpp
struct RegurgitationAssessment {
    bool detected = false;
    float score = 0.0f;           // 0–1 scaffold dominance
    int document_header_count = 0;
    int source_span_count = 0;
    int scaffold_separator_count = 0;
};

RegurgitationAssessment assessChunkFormatRegurgitation(std::string_view text);
```

Prefer extensible structure-aware rules over brittle global substring matching.

---

## §6 — Assessment Layer 2 (answer quality, post-scaffold)

Runs **after** scaffold strip when Layer 1 was triggered on the transcript-sanitized text.

**Purpose:** Distinguish:

- **Complete assistant answer** → accept (§4 return)
- **Pasted retrieval context** → regurgitation → retry (§8)

### Paste signals (conservative — require multiple indicators)

Implementation must document which combination triggers fail. Minimum normative set:

- **Doc-excerpt shape:** ≥2 ATX markdown headers (`#`, `##`) at line start with no conversational framing, **or**
- **Enumerated excerpt:** ≥3 lines matching `^\d+\.\s` list-item pattern typical of indexed docs, **or**
- **Fragment collage:** ≥2 consecutive short declarative lines without connective prose between blocks **and** no direct answer framing

### Complete-answer signals (any sufficient set)

- Multi-sentence explanatory prose addressing the query topic
- Conversational framing (e.g. "The sidebar rules require…", "In Thoth…")
- Connected synthesis across facts (not bullet collage)

### Ambiguity rule

When Layer 1 fired and Layer 2 is ambiguous → **retry** (do not return stripped bodies). When Layer 1 never fired → **do not retry** for Layer 2 alone unless paste signals are overwhelming (implementation default: pass).

**No retrieval comparison:** Layer 2 must not call GRAG, read chunk store, or compare against injected context. Generation-pipeline heuristics only.

---

## §7 — Generation pipeline (normative flow)

Per generation attempt:

```
generate (LLM)
  → transcript sanitize (Plan N — existing)
  → Layer 1 scaffold assess
  → if scaffold present: strip scaffold lines only
  → Layer 1 reassess
  → if scaffold remains: fail → retry path (§8)
  → Layer 2 answer-quality assess (when Layer 1 was triggered)
  → if complete answer: success → return
  → if pasted context: fail → retry path (§8)
  → if no Layer 1 trigger: success → return
```

**Existing Plan N behavior preserved:**

- Empty after transcript sanitize → one stop-free retry (unchanged).
- Regurgitation retry (§8) is **separate** and counts toward INV-CSG-B-5 maximum of one.

---

## §8 — Retry policy

Retry is based on **response quality**, not merely visible scaffold.

| After full per-attempt pipeline | Action |
|--------------------------------|--------|
| Complete assistant answer (§3, §6 pass) | **Return** |
| Scaffold removed but largely pasted context | **One retry** with plain-prose reminder |
| Scaffold remaining after strip | **One retry** |
| Retry still regurgitating (scaffold or paste) | **Existing fallback** (Plan N L8) |

**Retry prompt:** Append `kRegurgitationRetryReminder` (never truncated constant in `chat_prompt_config.h`) to the assembled prompt — one time only.

**Must not:** Increase total regurgitation retries beyond one. Must not add retrieval-side diversity or re-ranking to compensate.

---

## §9 — Prompt refinement (B.1)

**Files:** `external/basic_agent/include/chat_prompt_config.h`, `external/basic_agent/src/prompt_factory.cpp`

### `kGroundingRules` (refine)

- Answer in normal prose; use `[RAG Context]` as internal reference only.
- Never reproduce `Document:`, `source_span=`, or `---` scaffold.
- Mention filenames naturally when useful (not header format).
- Preserve insufficient-context behavior ("not in indexed documents — do not guess").

Remove phrasing that primes mimicry (e.g. "name the Document when helpful").

### `kAntiRegurgitationRules` (new, never truncated)

Explicit complement to `kAntiTranscriptRules` — state that internal labels are for grounding only and must not appear in replies.

### Assembly

Wire `kAntiRegurgitationRules` into `assembleConversationSections()` with same never-truncated status as anti-transcript and grounding rules. Update prompt budget accounting.

---

## §10 — Observability (generation only)

Extend `ChatRagResponseRecord` / `ChatGenerationResult` and `ChatRagLogger::responseToJson` — **no changes to retrieval / GRAG context diagnostics**.

| Field | Type | Purpose |
|-------|------|---------|
| `regurgitation_detected` | bool | Scaffold and/or paste-quality failure on final attempt |
| `regurgitation_score` | float | Layer 1 dominance (optionally reflect Layer 2 in implementation notes) |
| `retry_due_to_regurgitation` | bool | This turn performed the CSG-B retry |
| `regurgitation_retry_reason` | string | **Why** retry occurred — see enum below |
| `sanitize_reason` | string | Existing Plan N values + `stripped_chunk_scaffold`, `all_chunk_scaffold` |

### `regurgitation_retry_reason` (normative values)

| Value | Meaning |
|-------|---------|
| `none` | No regurgitation retry |
| `scaffold_remaining` | Layer 1 reassess still detected scaffold after strip |
| `pasted_context_after_strip` | Layer 2 failed — stripped bodies not acceptable |
| `scaffold_and_paste` | Both signals contributed |

**Invariant:** Operators must be able to explain why a regurgitation retry occurred from `chat_rag.jsonl` alone.

Wire through `CommandProcessor::applyGenerationDiagnostics`.

---

## §11 — Out of scope (hard stop)

Do **not** modify during CSG-B implementation:

- Retrieval ranking or diversity
- GRAG scoring formula or weights
- Grounding thresholds / grounding floor
- CSG-A goal hydration (`chat_retrieval_goal`, `active_goal`, `goal_source`)
- `formatChunkForPrompt()` or `[RAG Context]` injection layout
- `chat_retrieval_boost`, `grag_scorer`, RAG pipeline ranking

If investigation indicates those areas require changes → **document evidence, stop, request new protocol** — do not expand CSG-B.

**B.4 retrieval diversity:** Deferred unless B.1–B.3 fail manual verification (§12). Document evidence only; no implementation.

---

## §12 — Verification

### Automated (B.3 + tests)

- [ ] Prompt tests: grounded path includes anti-regurgitation rules; grounding text updated
- [ ] Layer 1: positive multi-chunk block; negative inline filename, JSON, prose "document"
- [ ] Sanitize: strips scaffold; reassess clean
- [ ] Strip + pasted bodies → **retry**, does **not** return stripped text
- [ ] Strip + valid prose → return without retry
- [ ] Retry success → conversational mock response
- [ ] Persistent regurgitation → Plan N fallback
- [ ] `testPlanNSanitizeTranscriptLoop` and Plan N generate tests unchanged

### Manual — captured `architectural_facts.md` failure

Reproduce the query that produced chunk-format echo. **Success example:**

> Concise conversational explanation of sidebar / `AddCollapsiblePane` rules — answers the question, no internal scaffold, no pasted doc collage.

**Failure examples:**

- Any `Document:` / `source_span=` / chunk `---` in reply
- De-scaffolded but still reads as pasted indexed-doc excerpt
- Verbatim bullet list from source without synthesis

**Acceptance record (2026-09-13):** Operator verified coherent, corpus-grounded
answers with no observed retrieval-scaffold echo and almost no hallucination. This
closes B.4 for the captured failure class; it does not change the locked
generation or retrieval contracts.

---

## §13 — Implementation phases

| Phase | Scope | Verify |
|-------|-------|--------|
| **B.1** | Prompt constants + `prompt_factory` wiring + prompt tests | Grounded prompt includes new rules |
| **B.2** | Layer 1 + Layer 2 assess APIs + unit tests | Positive/negative cases |
| **B.3** | `sanitizeChunkFormatScaffold`, pipeline integration, retry, diagnostics | Generation safety tests + build |
| **B.4** | Manual verify against captured failure | §12 manual criteria |

**Order:** B.1 → B.2 → B.3 → B.4. No parallel retrieval work.

---

## §14 — Files by sub-phase

| Phase | Files |
|-------|-------|
| B.1 | `chat_prompt_config.h`, `prompt_factory.cpp`, `tests/unit_tests.cpp` (prompt tests) |
| B.2 | `chat_generation_safety.h`, `chat_generation_safety.cpp`, `tests/unit_tests.cpp` |
| B.3 | `chat_generation_safety.{h,cpp}`, `chat_rag_observability.h`, `chat_rag_logger.cpp`, `command_processor.cpp`, `tests/unit_tests.cpp` |

**Not in scope:** `chat_retrieval_boost.cpp`, `grag_scorer.cpp`, `command_processor` retrieval path (except `applyGenerationDiagnostics`).

---

## Relationship to CSG-A

| CSG-A | CSG-B |
|-------|-------|
| Session goal → directional retrieval after GUI restart | Generation quality — prose answers vs scaffold echo |
| `goal_source`, embed cache | `regurgitation_retry_reason`, sanitize/retry |
| Locked separately 2026-07-30 | Locked separately 2026-07-30 |

CSG-A and CSG-B are independent. CSG-B may be implemented without CSG-A A.4 manual sign-off, but both may be verified together in chat manual tests.

---

## Verify checklist (post-implementation)

- [ ] B.1 prompt tests pass
- [ ] B.2 assessment tests pass (conservative negatives)
- [ ] B.3 pipeline tests pass; Plan N transcript tests pass
- [ ] `chat_rag.jsonl` includes regurgitation fields with meaningful `regurgitation_retry_reason`
- [x] Manual: `architectural_facts.md` query → §12 success example (operator acceptance, 2026-09-13)
- [ ] Retrieval diagnostics unchanged vs pre-CSG-B baseline
- [ ] No edits to `formatChunkForPrompt` or GRAG scoring paths
