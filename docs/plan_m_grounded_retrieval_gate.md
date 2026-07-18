# Plan M — Grounded Retrieval Gate (chat `grounded=true` meaning)

**Status:** ✅ **Complete** — 2026-07-16 (G4 docs closeout) · G0–G3 features shipped; G4 documentation only  
**Prerequisites:** Plan K ✅ · Plan L ✅ Complete (L3 deferred) · Hello / fake-transcript investigation (2026-07-16) · `final_score` domain confirmation (2026-07-16)  
**Roadmap:** Chat RAG correctness (post–Plan L); does not change Docker Step 10 ownership  
**Deferred (not in Plan M):** stronger meaningful-retrieval threshold; embedding / retrieval score analysis (new plan required)

---

## Purpose

Restore a trustworthy meaning for chat grounding:

- `grounded=true` / `grounding_mode=retrieved_context` ⇒ chunks were injected **after** the active gate(s) (not merely a non-empty nearest-neighbor string)  
- A **retrieval attempt** must be distinguishable from **grounding success**  
- Prompt templates must not invite fake `[User]` / `[Agent]` / `📝[RAG Context]` transcripts  

Preserve Plan K (remote transport) and Plan L (engine-owned workspace + seed). Do **not** revert remote mode or host-mirror the engine corpus.

**Phased delivery (shipped):** G1 = fail-closed score floor + `grounded` contract + attempt/success telemetry. G2 = greeting skip. G3 = cue B + anti-transcript + minimal stops + chat max tokens 512. G4 = docs closeout only. Calibrated relevance and embedding/score analysis remain **deferred** (see G4 deferrals).

---

## Locked semantic contract

| Signal | Meaning |
|--------|---------|
| Retrieval **attempted** | Chat path ran retrieve (or recorded an intentional skip) |
| Retrieval **succeeded for grounding** | ≥1 chunk passed the **active** gate(s) **and** was injected |
| `grounded=true` | Same as succeeded for grounding |
| `grounding_mode=retrieved_context` | Same as grounded |
| `grounding_mode=no_retrieval_hits` | Nothing injectable after the active gate(s), **or** intentional conversational skip (G2+). When `retrieval_ran=false`, use `retrieval_skip_reason` to disambiguate (`greeting` vs other skips). Do not treat this mode alone as “retrieval failed.” |
| `grounding_mode=no_index` | Existing empty-index path unchanged |

`grounded` and `retrieved_context` stay **aligned**.

**G1 active gate:** fail-closed floor on post-boost `final_score` (`>= kMinGroundingFinalScore`).  
**G2+:** greeting skip before retrieve.  
**Still deferred:** any stronger absolute/relative “meaningful relevance” threshold; embedding / retrieval score analysis.

---

## `final_score` domain (confirmed 2026-07-16)

Chat `CHAT_RAG_CONTEXT` / `diagnostics.breakdowns[].final_score` is the **post-pipeline ranking score**, not raw embedding similarity alone:

1. Initial recall cosine → `GragScorer` hybrid (`vector` + `keyword_weight * keyword` + `graph_weight * graph`)  
2. Then **`applyConversationalBoosts`** (e.g. filename `+0.55`, tiny-chunk ×`0.25`, …) writes the boosted value back into `final_score`

| Property | Fact |
|----------|------|
| Normalized to `[0, 1]`? | **No** — additive; good chat hits often **`> 1.0`** |
| Comparable across all paths? | **Only within** chat conversational pipeline; goal-directed path skips chat boosts / uses α blend |
| Absolute magnitude = relevance? | **No** — ranking score; healthy embeds still give Hello ~`0.5` nearest neighbors |

Therefore an absolute bar cannot honestly mean “calibrated relevance” without more evidence. **R1** keeps `0.01f` as a **floor only**.

---

## Threshold decision — R1 (locked 2026-07-16)

**Selected: R1 — Floor only for G1.**

| Decision | Lock |
|----------|------|
| `kMinGroundingFinalScore` | **`0.01f`** — applies to **post-boost** `final_score` from the existing chat retrieval pipeline |
| G1 gate role | **Fail-closed floor** only: reject `0.0` / missing / `< 0.01` scores so they cannot set `grounded=true` |
| G1 does **not** claim | Hello not-grounded; calibrated meaningful-retrieval; separation of weak real Q&A from chit-chat by score alone |
| Hello | **Deferred to G2** (exact-phrase skip policy below) |
| Stronger threshold (absolute, relative, or dual-gate) | **Deferred** until G1 telemetry (`max_score`, `candidates_*`, reasons) exists and can inform a separate plan revision |
| Rejected for G1 without new evidence | Raising the floor to ~`0.5+` (Hello overlaps real weak tops in absolute score space) |

---

## G0 locked defaults (Hello policy + score floor)

Docs-only defaults for implementers.

### Score floor (G1 consumes) — R1

| Constant (target: `chat_retrieval_config.h`) | Locked default | Rule |
|---------------------------------------------|----------------|------|
| `kMinGroundingFinalScore` | **`0.01f`** | Chunk passes **floor** iff post-boost `final_score >= 0.01f` |
| Fail-closed | — | Score `0.0` / missing score ⇒ reject; empty post-floor set ⇒ not grounded |

Gate placement (G1): after conversational selection (`selectTopKForInjection` / equivalent), before assembling `ragContext`. Chat `processQuery` decision point only.

### Hello / chit-chat skip policy (G2 consumes — not G1)

| Rule | Locked choice |
|------|----------------|
| When | Before retrieve on the indexed chat RAG path (after empty-index / `no_index` check) |
| Match | **Exact** after normalize: trim whitespace, lowercase, strip trailing `!?.` only |
| Locked phrase set | `hello`, `hi`, `hey`, `howdy`, `yo`, `thanks`, `thank you`, `thx`, `good morning`, `good afternoon`, `good evening` |
| Non-match | Any extra token, punctuation mid-phrase, or length beyond the exact set ⇒ **not** a greeting skip |
| Primary rule | Exact-match only. Future phrase-set additions **must not** include interrogative forms. |
| Defensive (optional) | A whole-token check for `explain` / `what` / `how` / `why` **may** remain as belt-and-suspenders if the phrase list grows later; it is **not** a second matching mechanism and is not required for current locked phrases (e.g. `thanks why?` already fails exact-match). |
| On skip | Do **not** call retrieve; `retrieval_ran=false`; `retrieval_skip_reason=greeting`; `grounding_decision_reason=greeting_skip`; `candidates_found=0`; `candidates_passed_gate=0`; `grounded=false`; `grounding_mode=no_retrieval_hits`; no `[RAG Context]` / grounding rules |

`"Hello"` + seeded corpus (**T1**) is satisfied in **G2** via this skip. G1 must **not** implement greeting skip.

**Telemetry nuance (locked contract, no new enum):** `grounding_mode=no_retrieval_hits` remains the ungrounded mode for greeting skip. That string can sound like “retrieval ran and failed,” but **`retrieval_ran=false` + `retrieval_skip_reason=greeting`** disambiguate intentional conversational skips. Do **not** introduce a new `grounding_mode` value in G2; operators and future tools must consult `retrieval_skip_reason` (and `retrieval_ran`) alongside `grounding_mode`.

---

## Locked operator telemetry

Every chat turn on the RAG-capable path (including Hello skip once G2 lands) must emit telemetry (primarily `CHAT_RAG_CONTEXT`; mirror on decision-trace stage when already used) that answers:

| Operator question | Required fields (names locked; implement exactly or document rename in G1 closeout) |
|-------------------|-------------------------------------------------------------------------------------|
| Did retrieval run? | `retrieval_ran` (bool) **and** `retrieval_skip_reason` (`none` \| `greeting` \| `no_index` \| …) |
| How many candidates were found? | `candidates_found` (pre-floor count) |
| How many passed the relevance gate? | `candidates_passed_gate` (post-floor / injected count) |
| Why was grounding enabled or disabled? | `grounding_decision_reason` + consistent `grounded` / `grounding_mode` |

**Reason enum (locked set; extend only with plan revision):**

| `grounding_decision_reason` | Meaning |
|-----------------------------|---------|
| `injected_meaningful_hits` | Grounded; ≥1 passed **active** gate(s) and injected (**G1:** passed floor only — name retained for stability; not a claim of calibrated relevance) |
| `below_threshold` | Ran; candidates found; 0 passed floor |
| `greeting_skip` | Retrieval skipped (G2+) |
| `empty_index` | No index / no_index path |
| `no_candidates` | Ran; zero candidates before floor |

**`retrieval_skip_reason` values (locked):** `none` · `greeting` · `no_index` (extend only with plan revision).

**Illustrative combinations:**

- Attempt without success: `retrieval_ran=true`, `retrieval_skip_reason=none`, `candidates_found>0`, `candidates_passed_gate=0`, `grounded=false`, reason `below_threshold`  
- Skip (G2+): `retrieval_ran=false`, `retrieval_skip_reason=greeting`, reason `greeting_skip`, `grounded=false`  
- Success (G1+): `retrieval_ran=true`, `candidates_passed_gate≥1`, `grounded=true`, reason `injected_meaningful_hits`  

Also emit `max_score` / `min_injected_score` when candidates exist. Do **not** treat `retrieved_chars>0` alone as success.

Additive fields on existing `CHAT_RAG_CONTEXT` / `ChatRagContextRecord` (G1); preserve event names.

---

## G1 implementation plan (R1) — ✅ implemented 2026-07-16

**Lock date:** 2026-07-16 · **Implemented:** 2026-07-16 (verified via `thoth-core-tests`, all unit tests pass)  
**Discipline:** AGENTS.md — Plan → Review → Refine → **Lock** → Implement → Verify.

**Landed:**

- `chat_retrieval_config.h` — `kMinGroundingFinalScore = 0.01f` (fail-closed floor on post-boost `final_score`).
- `chat_retrieval_boost.{h,cpp}` — `applyGroundingFloor(...)` pure helper + `GroundingFloorResult` / `GroundingFloorStats` (fail-closed on missing/NaN; returns injectable chunks + aligned filtered diagnostics + stats).
- `chat_rag_observability.h` + `chat_rag_logger.cpp` — additive `CHAT_RAG_CONTEXT` fields: `retrieval_ran`, `retrieval_skip_reason`, `candidates_found`, `candidates_passed_gate`, `grounding_decision_reason`, `grounded`, `max_score`/`min_injected_score` (null when absent).
- `command_processor.cpp` — chat path applies the floor before building `ragContext`; `grounded` / `grounding_mode` derive from post-floor injection; reason mapping (`no_candidates` / `below_threshold` / `injected_meaningful_hits`); `no_index` path emits `empty_index` telemetry.
- `tests/unit_tests.cpp` — `testPlanMGroundingFloorRejectsBelowThreshold` (T3), `testPlanMGroundingFloorPassesAboveThreshold`, `testPlanMGroundingTelemetryShape` (T5 partial).

**Deferred as planned:** greeting skip (G2 / T1), prompt+stops (G3 / T2, T4), stronger meaningful threshold (post-telemetry).

**Scope:** Fail-closed score floor + wire `grounded` / `grounding_mode` to post-floor injection + additive telemetry.  
**Out of scope:** Greeting skip (G2); prompt/stops (G3); raising/`dual` meaningful threshold; Docker / Plan K/L changes.

### Behavior

```text
chunks, diagnostics ← retrieveRelevant(...)   # already post boost + selectTopK
candidates_found ← chunks.size()
injectable ← filter where post-boost final_score >= 0.01f   # fail-closed if missing
candidates_passed_gate ← injectable.size()
ragContext ← format(injectable)
options.grounded ← !injectable.empty()
grounding_mode ← injectable.empty() ? no_retrieval_hits : retrieved_context
# fill CHAT_RAG_CONTEXT telemetry (attempt vs success)
```

### Files (expected)

| File | Change |
|------|--------|
| `chat_retrieval_config.h` | `kMinGroundingFinalScore = 0.01f` |
| `chat_retrieval_boost.{h,cpp}` | Pure floor filter helper |
| `chat_rag_observability.h` + `chat_rag_logger.cpp` | Additive telemetry fields |
| `command_processor.cpp` | Floor + grounded wiring + telemetry (`no_index` + indexed chat path) |
| `tests/unit_tests.cpp` | T3 + telemetry shape; **not** T1 |

### G1 verify

| ID | Case | Expect |
|----|------|--------|
| **T3** | Candidates with score `0` / `< 0.01` | `candidates_found>0`, `passed_gate=0`, not grounded, reason `below_threshold` |
| **Pass unit** | One chunk `final_score >= 0.01` | Passes floor; grounded path possible |
| **Telemetry** | Serialize context record | Four operator questions answerable; `max_score` present when candidates exist |
| **Explicit non-goal** | Hello / greeting skip | **Not implemented / not required** to pass G1 |

### G1 success criteria

1. All-zero / below-floor candidates never set `retrieved_context` / `grounded=true`.  
2. `grounded` ↔ non-empty **post-floor** injection (contract fix vs today’s `!ragContext.empty()` on ungated text).  
3. Telemetry distinguishes attempt vs success.  
4. No greeting-skip code; no threshold raise beyond `0.01f`.

Stop after G1 verify — do not start G2.

---

## G2 implementation plan — ✅ implemented 2026-07-16

**Lock date:** 2026-07-16 · **Implemented:** 2026-07-16 (verified via `thoth-core-tests`, all unit tests pass)  
**Discipline:** AGENTS.md — Plan → Review → Refine → **Lock** → Implement → Verify.

**Depends on:** G1 ✅  
**Layer:** Query intent **before** retrieve (core chat path). Does not touch Plan K, Plan L, Docker, RAG ownership, or transport.

**Landed:**

- `chat_query_utils.h` / `.cpp` — `isGreetingSkipQuery` (exact-match primary after normalize; optional defensive interrogative-token check).
- `command_processor.cpp` — greeting skip branch after empty-index / before retrieve; ungrounded prompt; skip telemetry (`retrieval_ran=false`, `retrieval_skip_reason=greeting`, `grounding_decision_reason=greeting_skip`, `grounding_mode=no_retrieval_hits`).
- `tests/unit_tests.cpp` — `testPlanMGreetingSkipClassifier` (T1 must-skip / must-not-skip), `testPlanMGreetingSkipTelemetryShape` (T5 skip path).

### Scope

Greeting / chit-chat skip so locked exact phrases never retrieve or ground. Completes **T1** and the greeting-skip arm of **T5**.

### Out of scope

- Prompt anti-transcript / stops / `max_tokens` → **G3** (keep completely separate)
- Stronger score threshold / dual gate
- New `grounding_mode` enum values
- Plan K/L / Docker / remote client changes

### Classifier (primary rule)

1. **Exact-match only** after normalize (trim, lowercase, strip trailing `!?.`).
2. Membership in the locked phrase set (see Hello policy above).
3. Future phrase additions **must not** include interrogative forms.
4. Optional defensive interrogative-token check (`explain` / `what` / `how` / `why`) may remain for future list growth; it is **not** a second matching mechanism. Current locked phrases already fail exact-match for cases like `thanks why?`.

### Wire in `CommandProcessor::processQuery`

```text
if (index empty) → existing no_index path
else if (isGreetingSkipQuery(input)) {
  empty ragContext; options.grounded=false
  emit CHAT_RAG_CONTEXT (greeting skip telemetry)
  → LLM (ungrounded) → return
}
else → retrieve → applyGroundingFloor (G1) → …
```

Empty-index check **before** greeting skip so empty corpus stays `no_index` / `empty_index`.

### Telemetry (no new grounding_mode)

| Field | Greeting skip |
|-------|----------------|
| `retrieval_ran` | `false` |
| `retrieval_skip_reason` | `greeting` |
| `candidates_found` / `candidates_passed_gate` | `0` |
| `grounding_decision_reason` | `greeting_skip` |
| `grounded` | `false` |
| `grounding_mode` | `no_retrieval_hits` (includes intentional conversational skips; **`retrieval_skip_reason` disambiguates**) |
| `max_score` / `min_injected_score` | null |

### Files (expected)

| File | Change |
|------|--------|
| `chat_query_utils.h` / `.cpp` | `isGreetingSkipQuery` (exact-match primary) |
| `command_processor.cpp` | Greeting branch before retrieve |
| `tests/unit_tests.cpp` | Must-skip / must-not-skip + T5 skip telemetry |
| Plan M + log | G2 ✅ after verify |

### Tests

| Group | Cases | Expect |
|-------|-------|--------|
| **Must skip** | `hello`, `hello!`, `Hi.`, `thanks`, `good morning` | classifier true; skip telemetry |
| **Must not skip** | `hello there`, `explain GRAG`, `what is GRAG`, `how does GRAG work` | classifier false (protects against over-aggressive “friendly chat” heuristics) |
| **T5** | Greeting-skip context record JSON | Four operator questions; `retrieval_ran=false` + `skip_reason=greeting` |
| **Regression** | G1 floor units | Still pass |

### Success criteria (G2 only)

1. Locked greetings → no retrieve, not grounded, telemetry = greeting skip.  
2. Must-not-skip set still takes G1 retrieve → floor path.  
3. No G3 prompt/stop edits; no new `grounding_mode` values.

---

## G3 implementation plan — ✅ implemented 2026-07-16

**Lock date:** 2026-07-16 · **Implemented:** 2026-07-16 (verified via `thoth-core-tests`, all unit tests pass)  
**Discipline:** AGENTS.md — Plan → Review → Refine → **Lock** → Implement → Verify.

**Depends on:** G1 ✅ · G2 ✅  
**Layer:** Chat **prompt contract** + **generation bounds** + optional **stop sequences**. Does not touch retrieval, greeting, Plan K/L, Docker, or Cognate.

**Closeout — locked choices delivered:**

| Item | Delivered |
|------|-----------|
| Cue **B** | User block `"[User] " + user_input + "\n"` via `ChatPrompt::formatUserBlock`; no open `[Agent]` slot |
| Anti-transcript | `kAntiTranscriptRules` in never-truncated core (always on for chat assembly) |
| Stops | `"\n[User]"`, `"\n[Agent]"` only; chat path only |
| Chat max tokens | `kChatMaxTokens = 512` via `llm.query(..., 512, stops)` on chat paths |
| Inference | Additive `InferenceGenerateRequest::stop_sequences`; omit JSON `stop` when empty |

**Landed:**

- `chat_prompt_config.h` — anti-transcript, cue helpers, `kChatMaxTokens`, stop constants
- `prompt_factory.cpp` — cue B + anti-transcript in core budget/assembly
- `inference_types.h` — optional `stop_sequences`
- `llama_server_client` / `ollama_client` — `serializeGeneratePayload` + stop when non-empty
- `llm_interface` — `query`/`askOllama` overloads with stops
- `command_processor.cpp` — chat `query` uses 512 + chat stops (no_index, greeting skip, indexed)
- `tests/unit_tests.cpp` — T4/T2 prompt units + stop payload serialization

### Scope (boundary lock)

| In G3 | Out of G3 |
|-------|-----------|
| Anti-transcript prompt rules (never-truncated core) | Retrieval floor / threshold |
| Completion-cue change (see lock choice) | Greeting skip |
| Minimal stop sequences on chat generate | Plan K remote client |
| Explicit chat `max_tokens` bound (no quality cut below current default) | Plan L / Docker / seed |
| Offline T2 / T4 / stop-payload tests | Live GGUF / Compose smoke as PR gate |
| Additive optional `stop_sequences` on Plan H request type | Inference capability discovery / provider refactor |

---

### 1. Completion cue — analysis (do not assume A)

**Current template** (both `assembleConversationSections` and `buildChatPrompt` budget math):

```text
… history …
[User] <input>
[Agent] 
```

History uses `[role]` labels from memory (`user` / `assistant`), while the open turn uses `[User]` / `[Agent]` — a second bracket dialect. Grounded prompts also prepend `[RAG Context]` / `[User Query]`.

#### Does keeping `\n[Agent] ` still bias small models?

**Yes, strongly.** The final tokens are an open dialogue slot in a transcript grammar. Autoregressive models continue that grammar: finish “Agent” text, then often invent `[User]` / another `[Agent]` / even `[RAG Context]` blocks. This matches the observed Hello / weak-model failure mode.

#### Do anti-transcript instructions reliably override that pattern?

**Not reliably on small local models.** Instructions sit earlier in the prompt; the completion prior at the tip is format continuation. Natural-language “do not invent turns” loses to an open `[Agent]` cue often enough that G3 should not treat instructions alone as sufficient if the cue stays.

#### Is changing only the final cue lower-risk than multiple inference changes?

**Yes for blast radius.** A cue edit is prompt-factory / `chat_prompt_config` only. Adding `stop_sequences` touches Plan H types + Ollama + llama-server serializers. Cue change addresses the **root bias**; stops are a **safety net**, not a substitute for leaving the bias in place.

#### Tradeoff table

| Option | Definition | Fixes root bias? | Infra surface | Quality / regression risk |
|--------|------------|------------------|---------------|---------------------------|
| **A** | Keep `[User]…\n[Agent] ` + anti-transcript + stops | No — fights the tip prior with stops/instructions | Medium (stops) | Style stable; higher chance stops fire mid-script; bug may persist if stops miss variants |
| **B** | **Replace cue** + anti-transcript + **minimal stops** | Yes | Medium (prompt + stops) | Slight answer-shape change; lowest *combined* risk of (unfixed bug + truncation from fighting cue) |
| **C** | Replace cue + anti-transcript **only** (no stop field) | Yes | **Low** (prompt only) | Least infra risk; weaker hard stop if model still emits `[User]` |

**Recommended lock: B** — replace the open `[Agent]` completion cue, add short anti-transcript rules, and keep a **minimal** stop set as defense in depth.

**Rejected as default: A** — keeping `[Agent] ` preserves the primary transcript prior; anti-transcript + stops become compensatory. That is a deliberate last resort only if cue replacement regresses answer quality in review.

**C** is acceptable if lock prefers zero Plan H touch in G3; then stops defer to a follow-up. Prefer **B** unless that constraint is stated.

#### Locked cue replacement (under B) — exact text

**Old user block (remove):**

```text
[User] <user_input>\n[Agent] 
```

i.e. C++ today: `"[User] " + user_input + "\n[Agent] "`

**New user block (lock exact):**

```text
[User] <user_input>\n
```

i.e. C++: `"[User] " + user_input + "\n"`

| Rule | Lock |
|------|------|
| Constant / assembly | Single shared user-block construction in `assembleConversationSections` (and any duplicate budget math in `buildChatPrompt`) must use this exact string |
| After `<user_input>` | Exactly one `\n`, then **end of prompt** |
| Must **not** append | `[Agent]`, `[Agent] ` (with space), `Agent:`, `Assistant:`, or any other open role/completion slot |
| History labels | Unchanged (`[user]` / `[assistant]` from memory). G3 does not rewrite history format |
| Grounded headers | Unchanged: still may prepend `[RAG Context]` … `[User Query]` before the conversation sections |

**Resulting final prompt ending (exact)**

The prompt always ends with the current user turn and a newline — never an open agent turn:

```text
… (optional history lines) …
[User] <verbatim user_input>
⏎
```

(`⏎` = the single trailing newline that terminates the prompt; generation starts immediately after that newline.)

**Ungrounded example ending**

```text
You are Thoth, a helpful assistant. …
[user] prior …
[assistant] prior …
[User] Hello
```

**Grounded example ending** (RAG + rules earlier in the same string; ending is identical in shape)

```text
[RAG Context]
Document: GRAG.md
…
[User Query]
Grounding Rules:
…
<anti-transcript rules>
You are Thoth, a helpful assistant. …
[User] Explain GRAG.
```

**T4b assertion (exact):** `buildChatPrompt(...)` result must:

1. Contain `"[User] " + user_input + "\n"` as a suffix of the conversation section / full prompt (allowing only that trailing newline after the user line).  
2. **Not** contain `"\n[Agent]"` anywhere as an open completion cue (in particular must not end with `"\n[Agent] "` or `"\n[Agent]"`).

This is the architectural cause fix: the model is no longer primed mid-transcript as Agent.

---

### 2. Stop sequences — refined

#### Proposed earlier (too broad)

`\n[User]` · `\n[Agent]` · `\n[RAG Context]` (+ emoji variants)

#### Analysis

| Candidate | Keep? | Why |
|-----------|-------|-----|
| `\n[User]` | **Yes** | Continuation of fake user turns; unlikely mid-sentence in a normal answer |
| `\n[Agent]` | **Yes** | Continuation of fake agent turns; protects against old template even after cue change |
| `\n[RAG Context]` | **No** | Answers may *mention* RAG / quote the header when explaining Thoth; stop would truncate legitimate grounded explanations |
| `📝[RAG Context]` / emoji variants | **No** | Hallucination marker; handle via anti-transcript text, not stop vocabulary expansion |

Leading `\n` keeps stops aimed at **new transcript lines**, not mid-word brackets.

#### Recommended lock — smallest safe stop set

```text
"\n[User]"
"\n[Agent]"
```

Only these two. Apply on **chat** generate requests only (empty default elsewhere).

---

### 3. Chat max tokens — refined

| Candidate | Assessment |
|-----------|------------|
| `256` | Too aggressive for long grounded GRAG explanations; quality cut to suppress transcripts |
| `384` | Covers most local log answers; still clips outliers |
| **`512`** | Matches current `Config::max_tokens` default; explicit chat bound without reducing below today’s ceiling |

Local `chat_rag.jsonl` response sizes (n≈366): median answer short; max ≈1847 chars (~460 tokens). Cutting to 256 would risk real technical answers; **512 does not**.

**Recommended lock:** `kChatMaxTokens = 512` as an **explicit chat-path ceiling** (via existing `num_predict_override`), not a reduction campaign. If chat already uses `config->max_tokens == 512`, the constant documents intent and prevents accidental huge overrides on the chat path only. Planner/executive remain on their existing knobs.

**Not a substitute for cue fix** — token caps truncate symptoms late; cue change prevents scripts early.

---

### 4. Inference request changes — refined

`InferenceGenerateRequest` today: `model`, `prompt`, `temperature`, `top_p`, `max_tokens` — **no stops**.

| Question | Answer for G3 |
|----------|----------------|
| Clean additive fit? | **Yes** — optional `std::vector<std::string> stop_sequences` (default empty) |
| Both providers? | **Yes if B** — serialize when non-empty in `LlamaServerClient` and `OllamaClient` only |
| Capability-aware? | **No in G3** — out of scope; empty = omit field (current behavior) |
| Fail safely? | Omit `stop` key when empty; if a provider ignores unknown/empty stops, behavior = today. Do not add discovery APIs |
| Mock client? | No behavior change required when empty; payload unit tests target real serializers or shared JSON builder helpers |

**Scope discipline:** additive field + two serializers + chat call site. No Plan H checkpoint reopen, no embedding changes, no new client class.

If lock chooses **C** (no stops), delete this file set from G3 scope entirely.

---

### 5. Files in / out of scope (revised)

**In scope (under recommended B):**

| File | Role |
|------|------|
| `chat_prompt_config.h` | `kAntiTranscriptRules`; cue constant; `kChatMaxTokens = 512` |
| `prompt_factory.cpp` | Apply new cue; inject anti-transcript into never-truncated core |
| `inference_types.h` | Optional `stop_sequences` |
| `llama_server_client.cpp` / `ollama_client.cpp` | Emit stops when non-empty |
| `llm_interface.cpp` and/or `command_processor.cpp` | Chat path: stops + `num_predict_override = 512` |
| `tests/unit_tests.cpp` | T4, T2 offline, stop payload, G1/G2 regression |
| Plan M + `completed_improvements_log.md` | Closeout records cue **B**, stop list, `512` |

**Removed from earlier G3 draft scope:**

- Stop on `\n[RAG Context]` or emoji markers  
- `kChatMaxTokens` 256/384 as quality-reducing defaults  
- Provider capability negotiation  
- Assumption that cue choice **A** is preferred  

**Still out:** retrieval, greeting, Plan K/L, Docker, Cognate, history role-label rename.

---

### 6. Tests (offline only — no GGUF)

| ID | Case | Expect |
|----|------|--------|
| **T4a** | `buildChatPrompt` grounded + ungrounded | Contains anti-transcript rules; does **not** end with `\n[Agent] `; does not instruct multi-turn script continuation |
| **T4b** | Cue snapshot | Prompt ends with `"[User] " + input + "\n"`; does **not** end with / contain open `"\n[Agent]"` completion cue |
| **T2** | Grounded assembly with non-empty RAG string + `grounded=true` | Still includes `[RAG Context]` header + grounding rules; anti-transcript present; cue still non-open-role |
| **T2b** | G1 floor regression | `applyGroundingFloor` pass/fail units unchanged |
| **Stops** | Llama + Ollama generate payload with chat stops | JSON contains exactly `\n[User]` and `\n[Agent]` when set; **omits** stop key when empty |
| **G2 regression** | Greeting classifier + skip telemetry | Still pass |

No live model call required for PR gate.

---

### Recommended lock choices (summary)

| Item | Lock |
|------|------|
| **Cue** | **B** — exact user block `"[User] " + user_input + "\n"`; prompt ends there (no `[Agent]` slot) |
| **Anti-transcript** | Yes — short never-truncated rules (incl. no invented `[RAG Context]` / `📝[…]` scripts) |
| **Stop sequences** | Exact list: `"\n[User]"`, `"\n[Agent]"` only |
| **Chat max tokens** | **`512`** (explicit ceiling; not a quality cut) |
| **Inference** | Additive optional `stop_sequences`; both providers; omit when empty; no capability API |

---

### G3 success criteria

1. Prompt no longer opens an `[Agent]` transcript slot.  
2. Anti-transcript rules present in core assembly.  
3. Chat generate uses the two-stop set + explicit 512 bound.  
4. Offline T2/T4/stops green; G1/G2 regressions green.  
5. No retrieval / greeting / Plan K/L / Docker changes.

---

## G4 closeout — ✅ implemented 2026-07-16

**Lock date:** 2026-07-16 · **Implemented:** 2026-07-16 (docs/status only)  
**Discipline:** AGENTS.md — Plan → Review → Refine → **Lock** → Implement → Verify.

**Depends on:** G0–G3 ✅  
**Nature:** **Documentation / status closeout only.** G4 did **not** change chat retrieval, grounding, prompts, stops, tokens, Plan K, Plan L, Docker Compose, or Cognate.

### What G4 is / is not

| G4 **is** | G4 is **not** |
|-----------|----------------|
| Marking Plan M **✅ Complete** after G0–G3 | A feature checkpoint (no new gates, cues, stops, or telemetry fields) |
| Writing the final architecture + operator telemetry guide | Re-tuning `kMinGroundingFinalScore` or adding dual/relative score gates |
| Cross-linking operator docs (Plan L Evidence C, GETTING_STARTED) | Embedding-model alignment / score-distribution analysis work |
| Recording **deferred** future work honestly | Live GGUF/Compose mandatory re-verification |

If a change would alter runtime behavior, it belongs in a **new plan revision**, not G4.

---

### Operator telemetry states (clarify for closeout docs)

Document these combinations so operators (and tools that only glance at `grounding_mode`) do not misread logs.

| Situation | `retrieval_ran` | `retrieval_skip_reason` | `candidates_found` | `candidates_passed_gate` | `grounding_decision_reason` | `grounded` | `grounding_mode` |
|-----------|-----------------|-------------------------|--------------------|----------------------------|-----------------------------|------------|------------------|
| Empty index | `false` | `no_index` | `0` | `0` | `empty_index` | `false` | `no_index` |
| Greeting skip (G2) | `false` | `greeting` | `0` | `0` | `greeting_skip` | `false` | `no_retrieval_hits` |
| Ran; zero candidates | `true` | `none` | `0` | `0` | `no_candidates` | `false` | `no_retrieval_hits` |
| Ran; all below floor (R1) | `true` | `none` | `>0` | `0` | `below_threshold` | `false` | `no_retrieval_hits` |
| Ran; ≥1 passed floor | `true` | `none` | `≥1` | `≥1` | `injected_meaningful_hits` | `true` | `retrieved_context` |

**Reading rules (must appear in closeout text):**

1. **`grounding_mode=no_retrieval_hits` is ambiguous alone.** It covers “ran but nothing injectable,” **and** intentional greeting skip. Always pair with `retrieval_ran` + `retrieval_skip_reason`.  
2. **`grounded=true` / `retrieved_context` means post-floor injection succeeded** — not “nearest neighbors existed.”  
3. **`injected_meaningful_hits` (reason name) under R1 means “passed the fail-closed floor,”** not a calibrated relevance claim.  
4. Prefer `CHAT_RAG_CONTEXT` for attempt vs success; `CHAT_RAG_RESPONSE` mirrors `grounding_mode` for the answer turn.

---

### Deferred future work (record in closeout — not G4 scope)

These remain **out of Plan M** and require a **new approved plan** before any code:

| Deferred item | Why deferred | Notes |
|---------------|--------------|-------|
| **Stronger meaningful-retrieval threshold** (absolute, relative, or dual-gate beyond `0.01f`) | Needs post-G1 telemetry evidence; Hello overlaps weak real tops in absolute score space | Do not raise the floor in G4 |
| **Embedding / retrieval score analysis** | Investigate score distributions, embed-model alignment (host vs Compose / llama embed vs chat), zero-score clusters, and whether a calibrated gate is justified | Analysis-first; may produce a Plan N (or Plan M revision) — **not** G4 |
| Live Compose + GGUF “Explain GRAG” / Hello smoke | Optional operator check; not a PR gate for G0–G3 | May use Plan L L2 checklist + Plan M telemetry fields |

G4’s job is to **name** these deferrals clearly so “Complete” does not imply calibrated relevance or embedding health.

---

### Deliverables (docs/status only)

| File | Change |
|------|--------|
| `docs/plan_m_grounded_retrieval_gate.md` | Closeout section + final architecture + telemetry state table; overall **✅ Complete**; G4 ✅; current gate closed |
| `docs/completed_improvements_log.md` | G4 entry: G0–G3 shipped; telemetry reading rules; deferred threshold + **embedding/score analysis** |
| `docs/docker_roadmap.md` | Plan M → ✅ Complete (chat RAG; no Docker step ownership change) |
| `docker/README.md` | Short Plan M operator note on Evidence C: new fields; `no_retrieval_hits` disambiguation; Hello → expect `greeting_skip` |
| `docs/GETTING_STARTED.md` | Brief pointer to Plan M for remote/hybrid chat grounding semantics (2–4 lines) |

**Forbidden in G4 diff:** any `external/basic_agent/**`, `src/**`, `tests/**`, Compose, seed scripts, or CMake changes.

---

### Plan M closeout — Final Architecture State

**Plan M is ✅ Complete (2026-07-16).** Features G0–G3 are shipped; G4 is documentation only.

**Chat path (Local and Remote share core):**

```text
empty index → no_index
else greeting skip → ungrounded + greeting_skip (no retrieve)
else retrieve → applyGroundingFloor(0.01) → grounded iff injectable
prompt: anti-transcript + cue B ("[User] " + input + "\n") + stops {\n[User], \n[Agent]} + max_tokens 512
```

**Operator truth:** use the telemetry state table above. Do not treat `grounding_mode` alone as attempt vs success.

**Preserved:** Plan K HTTP/SSE GUI; Plan L O1 engine-owned corpus + `docker/seed_rag/` seed workflow.

**Deferred (new plan required before code):**

1. Stronger meaningful-retrieval threshold beyond R1 floor `0.01f`  
2. Embedding / retrieval score analysis (distributions, embed alignment, zero-score clusters)  
3. Optional live Compose/GGUF smoke (not a Plan M PR gate)

### Plan N revision (conversational stops) — 2026-07-17

**Locked supersession for conversational chat only:** Plan M G3’s transcript stop list (`\n[User]`, `\n[Agent]`) is **withdrawn from the chat path** by [Plan N — Chat Generation Safety](plan_n_chat_generation_safety.md) (Option A). Live Compose/GGUF evidence showed those stops can produce empty completions and still miss no-newline scaffolding (e.g. `[User] help`).

**Unchanged from Plan M G3:** cue B (no open `[Agent]`), anti-transcript rules, `kChatMaxTokens = 512`, grounding G1/G2.

**Preserved:** `InferenceGenerateRequest::stop_sequences` and provider serializers for **non-chat** callers. See Plan N L6.

---

## Priority work (historical — delivered)

### P1 — Gate before grounded mode

1. **G1:** Fail-closed floor (`0.01f` on post-boost `final_score`) + grounded wiring + telemetry (R1).  
2. **G2:** Greeting / chit-chat skip (Hello / T1).  
3. **Later (plan revision):** Stronger meaningful-retrieval threshold only after telemetry evidence.  
4. Single decision point in `CommandProcessor::processQuery` for injectable → `grounded` ↔ `grounding_mode`.

### P2 — Stop prompt-induced fake transcripts

1. Anti-transcript instructions in never-truncated core (`chat_prompt_config.h` / assembly).  
2. **Replace** open `[Agent]` completion cue (recommended **B**; see G3 section — do not assume keep-cue).  
3. Minimal stop sequences: `\n[User]`, `\n[Agent]` only (not `[RAG Context]`).  
4. Explicit chat `max_tokens` ceiling **512** (not a cut below current default).

**At G3 lock:** confirm cue **B** (or C if no Plan H touch); record stop list + `512` in closeout.

### P3 — Regression tests (mandatory across plan)

| ID | Case | Expect | Checkpoint |
|----|------|--------|------------|
| **T1** | Must-skip greetings vs must-not-skip corpus/friendly phrases | Skip only exact locked set; telemetry `greeting_skip` | **G2** |
| **T2** | Known GRAG / corpus query | Grounded; `candidates_passed_gate≥1` | **G3** (smoke) / post-G1 may already pass floor |
| **T3** | Below-floor / zero-score candidates | `found>0`, `passed=0`, not grounded | **G1** |
| **T4** | Prompt assembly | No fake transcript invitation | **G3** |
| **T5** | Telemetry shape | Four operator questions from one `CHAT_RAG_CONTEXT` | **G1** partial; **G2** completes skip path |

Offline / `ctest -L pr`. No Docker/GGUF required for PR.

---

## Architecture preservation (locked)

| Preserve | Forbidden |
|----------|-----------|
| Plan K remote HTTP/SSE | Silent fallback to local |
| Plan L O1 engine-owned `/workspace` + `docker/seed_rag/` | Host `agent_workspace` mirror / memory bind |
| Same core path for Local and Remote backends | Remote-only cognition forks |
| Existing `no_index` | Redefining `retrieved_context` without the active gate(s) |

No Cognate APIs. No Phase 3 ingest in this plan.

---

## Checkpoint sequence

| ID | Work | Status |
|----|------|--------|
| **M0 / G0** | Contract + telemetry + Hello policy + score-floor defaults | ✅ Complete 2026-07-16 |
| **G1** | R1: fail-closed floor + grounded contract + telemetry (not Hello; not stronger threshold) | ✅ Implemented 2026-07-16 (verified) |
| **G2** | Greeting skip + T1 / T5 skip path | ✅ Implemented 2026-07-16 (verified) |
| **G3** | Prompt cue + anti-transcript + minimal stops + chat token ceiling | ✅ Implemented 2026-07-16 (verified) |
| **G4** | Docs/status closeout only (telemetry guide + deferrals; no features) | ✅ Implemented 2026-07-16 |

Each: **Plan → Review → Approve → Implement → Verify → STOP** unless a batch Implement is explicitly authorized.

---

## Success criteria (full Plan M)

1. Hello + seeded engine → **not** grounded; telemetry explains why (**G2**).  
2. GRAG/corpus query → grounded; telemetry shows ran + passed ≥1.  
3. Operators distinguish **attempt** vs **successful grounding** from telemetry alone (**G1+**).  
4. Fake `[User]`/`[Agent]`/`📝[RAG Context]` scripts suppressed by prompt/stops (**G3**).  
   - **Post-ship note:** conversational transcript **stops** superseded by Plan N (sanitize + empty-stop chat policy); cue B + anti-transcript remain.  
5. Plan K/L unchanged.

---

## Current gate

**Plan M ✅ Complete (G4 docs closeout 2026-07-16).**  
**Follow-on:** [Plan N](plan_n_chat_generation_safety.md) 🔒 Locked 2026-07-17 (chat generation safety).

No further Plan M checkpoints. Runtime changes (stronger threshold, embedding/score analysis, diagnostics UI, etc.) require a **new** plan and explicit approval.

STATUS: PLAN M COMPLETE
