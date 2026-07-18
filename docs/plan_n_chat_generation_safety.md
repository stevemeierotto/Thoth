# Plan N — Chat Generation Safety

**Status:** 🔒 **Locked** — 2026-07-17 (N0) · N1–N4 ✅ · **N6 ✅ Implemented** 2026-07-17 · Plan N chat path complete · **N5** → [`plan_n5_grag_diagnostics_display.md`](plan_n5_grag_diagnostics_display.md) ✅ Implemented 2026-07-18  
**Prerequisites:** Plan M ✅ Complete (G1–G3 shipped) · Post-rebuild live failures (`[User] help`, empty completion, transcript loops)  
**Roadmap:** Conversational chat reliability (post–Plan M); does not change GRAG scoring, Cognate, or Plan K/L transport  
**N5 (separate plan):** `GragDiagnosticsPanel` display honesty — see [`plan_n5_grag_diagnostics_display.md`](plan_n5_grag_diagnostics_display.md)

---

## Purpose

Create **one authoritative conversational generation boundary** so chat safety policy cannot drift across the three `CommandProcessor` paths (`no_index`, `greeting_skip`, indexed).

Plan N is **not** only an empty-completion fix. It establishes:

```text
CommandProcessor
        │
        ▼
generateAndSanitizeChat()
        │
        ├── inference generation
        ├── sanitize output
        ├── classify empty / failure
        ├── optional single retry without stops
        └── fallback if required
        │
        ▼
processToolCall()
        │
        ▼
memory (once) + HTTP/GUI response
```

Preserve Plan M grounding (G1 floor, G2 greeting skip, G3 cue B + anti-transcript + `kChatMaxTokens=512`).  
**Revise Plan M G3 for conversational chat only:** transcript stop sequences are **removed** from the chat path (Option A). The general `stop_sequences` API remains for other callers.

---

## Locked decisions (N0)

| ID | Decision |
|----|----------|
| **L1** | `provider_ok` ≠ useful text. HTTP 200 + empty `choices[0].text` ⇒ `provider_ok=true`, `text=""` (not transport failure). |
| **L2** | `LLMInterface` gains a structured generate entry (e.g. `queryDetailed`) used by the chat helper; string `query(...)` remains for non-chat. Mock / unavailable / test-suite paths must share this entry. |
| **L3** | `generateAndSanitizeChat` is mandatory. After N6: **no** conversational `llm.query(` in `command_processor.cpp`. |
| **L4** | Helper owns: generate, sanitize, classify, one retry, fallback, generation diagnostic fields. CommandProcessor owns: orchestration, `processToolCall`, memory once, return/HTTP. |
| **L5** | Retry: max one per turn; same prompt; same `max_tokens`; empty stops; no recursive `processQuery`; no duplicate memory. |
| **L6** | Conversational chat sends **empty** stop list. `chatStopSequences()` returns `{}`. Serializers still emit `stop` when non-empty (non-chat). |
| **L7** | N4 LLM-facing chunk header: **`source_span=29-33`** (not `Lines: 29-33`). Line numbers remain in chat_rag document metrics. |
| **L8** | Fallback copy: greeting → `"I'm here — how can I help?"`; other chat → `"I couldn't generate a reply."` |
| **L9** | `stop_triggered = (finish_reason == "stop")` when known; else `false` (unknown is not a positive claim). |
| **L10** | Telemetry: char counts, reasons, flags — **no** full prompt dumps by default. |
| **L11** | N5 diagnostics UI **out**. N1b anti-transcript wording soften **optional post-smoke**. History scrub of old sessions **deferred**. |

---

## Generation contract

### `InferenceGenerateResult` (extended)

| Field | Meaning |
|-------|---------|
| `ok` / **`provider_ok`** | Transport + parse succeeded. **Does not** mean useful text. |
| `text` | May legally be empty when the provider stopped before tokens. |
| `error` | Set when `!provider_ok`. |
| `finish_reason` | From provider JSON when present; else `""`. |
| `raw_json` | Unchanged (in-memory; not logged wholesale). |

**Parse lock (llama + Ollama clients):** HTTP 200 + empty completion text ⇒ `ok=true`, `text=""`. Connection / HTTP failure / malformed JSON / explicit error object ⇒ `ok=false`.

### `ChatGenerationResult` (chat helper output)

| Field | Meaning |
|-------|---------|
| `raw_text` | Provider text before sanitize |
| `sanitized_text` | After sanitize (memory/HTTP input to tools) |
| `finish_reason` | Copied from provider when known |
| `stop_triggered` | Per L9 |
| `sanitize_reason` | `none` \| `truncated_transcript_marker` \| `stripped_leading_scaffold` \| `all_scaffold` |
| `empty_after_sanitize` | Sanitized empty/whitespace |
| `used_stops` | This attempt sent non-empty stops (chat: always false after N3) |
| `retried_without_stops` | The one allowed retry ran |
| `fallback_used` | Canned fallback applied |
| `provider_ok` | Last attempt’s provider success |
| `provider_error` | When `!provider_ok` |

---

## Empty / failure taxonomy

| Class | Detection | Action |
|-------|-----------|--------|
| **A Transport / hard provider failure** | `!provider_ok` | Existing error path; no retry; no success fallback |
| **B Provider-valid empty** | `provider_ok && raw_text.empty()` | One retry without stops |
| **C Sanitize-empty** | `provider_ok && !raw.empty() && empty_after_sanitize` | One retry if not yet retried; else fallback (L8) |
| **D Success** | Non-empty `sanitized_text` | `processToolCall` → memory once → return |

---

## Sanitization (N1) — 🔒 Locked 2026-07-17

**When (full Plan N):** After provider returns; **before** `processToolCall`, `memory.addMessage`, HTTP/GUI.  
**N1 itself:** pure helper only — **no** live chat wiring (no runtime behavior change).

### N1 deliverables (locked scope)

| In | Out |
|----|-----|
| `chat_generation_safety.h` / `.cpp` | `generateAndSanitizeChat` implementation |
| `SanitizeOutcome` + `sanitizeChatAssistantText()` | `command_processor.cpp` |
| `ChatGenerationResult` type with N2 fields **defaulted only** | `LLMInterface` / inference clients |
| CMake: add `src/chat_generation_safety.cpp` to `BASIC_AGENT_SOURCES` | Retry / fallback / stops / telemetry emit |
| Unit tests N-T1, N-T2, **N-T2b** (N-T2c optional) | N4 headers; N5 UI; N1b wording |

### API (locked)

```text
SanitizeOutcome {
  sanitized_text,
  sanitize_reason,      // none | truncated_transcript_marker
                        //   | stripped_leading_scaffold | all_scaffold
  empty_after_sanitize
}

sanitizeChatAssistantText(raw) → SanitizeOutcome   // pure, deterministic, no I/O
```

`ChatGenerationResult` lives in the same module (final boundary type). N1 does **not** fill provider/retry/fallback fields beyond defaults. N1 does **not** ship a stub `generateAndSanitizeChat` that touches inference.

### Sanitizer rules (locked)

1. **Pure / side-effect free** — no JSON parse, no memory/HTTP/inference, no semantic interpretation beyond line markers.
2. **Line boundary** = first line, or after `\n` (also tolerate `\r\n` by treating line starts after `\n`).
3. After optional leading spaces/tabs on that line, recognize markers:
   - `[User]` · `[Agent]` · `[RAG Context]` · line starting with `📝`
4. **Truncate** at earliest mid-body marker → `truncated_transcript_marker`.
5. **Leading scaffold strip** when marker line(s) removed and remainder non-empty → `stripped_leading_scaffold`.
6. **Scaffold-only** (e.g. `[User] help`) → `sanitized_text=""`, `empty_after_sanitize=true`, `all_scaffold`.
7. **Whitespace-only / empty raw** → `sanitized_text=""`, `empty_after_sanitize=true`, `sanitize_reason=none` (not `all_scaffold`).
8. **Preserve** mid-sentence “User” / “RAG”; normal prose; JSON tool bodies without markers (byte-identical).

### N1 tests (locked)

| ID | Case | Expect |
|----|------|--------|
| **N-T1** | Loop with `[User]` / `[Agent]` / `📝`; mid-sentence User/RAG; JSON tool body | Truncate; correct reason; mid-sentence + JSON preserved |
| **N-T2** | `[User] help` | Empty; `empty_after_sanitize`; `all_scaffold` |
| **N-T2b** | Leading scaffold then useful body | Body kept; `stripped_leading_scaffold` |
| **N-T2c** (optional) | Whitespace-only | Empty; `none` |

### N1 verify

- Library builds with new source registered in CMake.  
- N-T1 / N-T2 / N-T2b pass.  
- Grep: no N1 changes under `command_processor`, `llm_interface`, llama/ollama clients.

---

## Structured generation + empty handling (N2) — 🔒 Locked 2026-07-17

**Architectural goal:** Separate inference success from chat policy.

```text
InferenceClient::generate
    → InferenceGenerateResult          // provider_ok ≠ useful text
LLMInterface::queryDetailed
    → InferenceGenerateResult          // mocks / unavailable / testSuiteDev preserved
generateAndSanitizeChat
    → ChatGenerationResult             // sanitize, one retry, fallback
CommandProcessor (N6 only)
    → processToolCall / memory once / HTTP
```

### N2 deliverables (locked scope)

| In | Out |
|----|-----|
| `InferenceGenerateResult.finish_reason` | `command_processor.cpp` migration |
| Llama soft-empty parse (`ok=true` + empty text) | N3 stop policy / `chatStopSequences` body |
| Ollama `done_reason` → `finish_reason` when present | N4 `formatChunkForPrompt` |
| `LLMInterface::queryDetailed(...)` | Telemetry emission (N6) |
| `query()` via `queryDetailed` + compatibility mapping | Diagnostics UI (N5) |
| `ChatGenerateOptions` + `generateAndSanitizeChat` | OpenAI full chat parity (structured unsupported ok) |
| Tests N-T3, N-T3b, N-T3c, N-T4, N-T5, N-T5b, **N-T6b** | |

### Provider contract (`InferenceGenerateResult`)

| Field | Meaning |
|-------|---------|
| `ok` | Transport + parse succeeded — **not** “useful answer” |
| `text` | May be empty on provider-valid empty completion |
| `error` | Set when `!ok` |
| `finish_reason` | Llama: `choices[0].finish_reason`; Ollama: `done_reason` if present; else `""` |
| `raw_json` / `token_usage` | Unchanged |

**Llama soft-empty (L1):** HTTP 200 + choices present + empty text ⇒ `ok=true`, `text=""`, **do not** set `error="Empty completion text"`. Malformed / missing choices / error object / HTTP fail ⇒ `ok=false`.

### `queryDetailed` / `query` (L2)

- Add `queryDetailed(prompt, num_predict_override, stop_sequences) → InferenceGenerateResult`.
- Must share mock / `THOTH_MOCK_LLM_UNAVAILABLE` / test-suite gates with `query`.
- Prefer: string `query()` **calls** `queryDetailed`, then:
  - `ok && !text.empty()` → return `text`
  - `ok && text.empty()` → return `""` (not Empty completion error)
  - `!ok` → existing `formatInferenceGenerateError(...)`
- OpenAI backend on `queryDetailed`: return `ok=false` with clear error (unsupported for structured chat path) unless trivially wrapped — **locked: unsupported structured error**.

### Error ownership (locked)

`ChatGenerationResult` describes facts — **never** stores `"Assistant: [Error] ..."` UI strings.

| Class A failure | `provider_ok=false`, `provider_error` set, `sanitized_text=""` |
| N6 | Formats user-facing errors via existing helpers |

### `generateAndSanitizeChat` (locked)

```text
ChatGenerateOptions {
  max_tokens                 // default kChatMaxTokens
  stop_sequences             // attempt 0 only; N3 will pass {}
  use_greeting_fallback      // L8 string selection
}

generateAndSanitizeChat(llm, prompt, opts) → ChatGenerationResult
```

**Lifecycle:**

1. Attempt 0: `queryDetailed(prompt, max_tokens, opts.stop_sequences)`  
2. If `!ok` → class A; return (no retry, no fallback)  
3. `raw_text` → `sanitizeChatAssistantText` → sanitize fields  
4. Non-empty sanitized → class D; return  
5. Empty (B provider-empty or C sanitize-empty) and not retried → attempt 1: `queryDetailed(..., {})`; set `retried_without_stops`; sanitize again  
6. Still empty → L8 into `sanitized_text`; `fallback_used=true`  
7. `stop_triggered = (finish_reason == "stop")` on last successful provider attempt; else `false`  
8. `used_stops` reflects whether that attempt sent a non-empty stop list  

**Retry ownership:** helper only — not CommandProcessor, not inference clients. Max one retry; same prompt; same `max_tokens`; retry stops `{}`; no recursive `processQuery`; no memory writes in N2.

### Empty taxonomy (locked)

| Class | Detection | Action |
|-------|-----------|--------|
| **A** Transport/hard | `!provider_ok` | No retry; no success fallback |
| **B** Provider-empty | `ok && raw_text.empty()` | One retry without stops |
| **C** Sanitize-empty | `ok && !raw.empty() && empty_after_sanitize` | One retry if needed; else L8 |
| **D** Success | non-empty `sanitized_text` | Ready for N6 |

### N2 tests (locked)

| ID | Case | Expect |
|----|------|--------|
| **N-T3** | Empty+ok then non-empty | One retry; `retried_without_stops`; final text |
| **N-T3b** | `[User] help` then retry success | Class C then D |
| **N-T3c** | Llama parse empty + finish_reason | `ok=true`; text empty; reason preserved |
| **N-T4** | Empty then empty | `fallback_used`; no Empty completion in helper result |
| **N-T5** | Transport `ok=false` | No retry; no fallback; `sanitized_text=""` |
| **N-T5b** | Malformed JSON | `ok=false` |
| **N-T6b** | `query()` soft-empty → `""`; hard fail → `[Error]…` | Compatibility |

### N2 verify

- N-T3–T5 / T3b / T3c / T5b / T6b pass.  
- Grep: no N2 edits to `command_processor.cpp`, chat stop constants, `formatChunkForPrompt`.

**Live-path verification (explicit — not a failure):**

| Layer | Status after N2 |
|-------|-----------------|
| Soft-empty parse + `queryDetailed` + `generateAndSanitizeChat` | ✅ Exists (unit-tested) |
| `CommandProcessor` conversational arms | Still `llm.query(...)` string path — **not migrated** |
| Full sanitize / retry / fallback on GUI `hello` | **Requires N6** |

If interactive chat still shows transcript scaffolding, empty replies, or incomplete greeting behavior after N2, that does **not** mean N2 failed. The chat pipeline migration simply has not happened yet. Interim only: string `query()` maps provider-valid empty text to `""` instead of `llama_cpp: Empty completion text`.

---

## Conversational stop policy (N3) — ✅ Implemented 2026-07-17

**Goal (L6):** Conversational chat sends an **empty** transcript stop list. `chatStopSequences()` returns `{}`. General `stop_sequences` API and serializers remain for non-chat / explicit callers.

### N3 deliverables (locked scope)

| In | Out |
|----|-----|
| `chatStopSequences()` → `return {}` + Plan N comment | CommandProcessor migration to `generateAndSanitizeChat` (**N6**) |
| Revise Plan M G3 stop unit test only (plus N3 tests below) | Removing `kChatStopUser` / `kChatStopAgent` unless audit shows zero refs |
| Docs: Plan M supersession already noted; N3 status | N4 headers; telemetry emit |

### Focus rules (locked)

1. **All three conversational arms keep calling `chatStopSequences()`** (`no_index`, `greeting_skip`, indexed). Do **not** hardcode `{}` at each CP site in N3 — change the policy function so those calls become empty stops.
2. **Planner / non-chat paths unaffected** — only change `chatStopSequences()` body; leave `InferenceGenerateRequest::stop_sequences` and serializers alone. N-T7 uses an **explicit** non-empty vector (not `chatStopSequences()`).
3. **Plan M G3 tests only** for stop expectations — update `testPlanMChatStopPayloadSerialization` (or split): empty chat policy omits `stop`; manual non-empty stops still serialize. Cue B / anti-transcript tests unchanged.
4. **`generateAndSanitizeChat` already accepts empty `opts.stop_sequences`** — no helper logic change required; **N-T6c is required** to prove it.
5. **No CommandProcessor migration** — no swap from `llm.query` to `generateAndSanitizeChat`; no memory/tool changes.

### `kChatStopUser` / `kChatStopAgent` lifecycle (refinement)

- **Do not remove** these constants in N3 unless an audit confirms **no remaining references** (code, tests, comments that still need the literal strings for serializer N-T7, etc.).
- Prefer keep constants + empty `chatStopSequences()`; N-T7 may use the constants or literals for explicit non-empty payloads.

### Live-path note after N3

N3 changes **stop policy on the live string `query` path** (fewer stop-induced empty completions). Full sanitize / retry / fallback still requires **N6**. Unchanged transcript scaffolding after N3 alone is not necessarily an N3 failure.

### N3 tests (locked)

| ID | Case | Expect |
|----|------|--------|
| **N-T6** | `chatStopSequences().empty()` | True |
| **N-T6c** | `generateAndSanitizeChat` with `opts.stop_sequences={}` | Completes; `used_stops==false` on success path without needing retry-for-stops |
| **N-T7** | Serialize request with **explicit** non-empty stops | `stop` key present; values correct |

Optional: N-T6d — document/grep that CP still references `chatStopSequences` three times.

### N3 verify

- N-T6, **N-T6c**, N-T7 pass; Plan M cue tests still pass.  
- Grep: `command_processor` still calls `chatStopSequences` (three arms); no `generateAndSanitizeChat` in CP.  
- Constants retained unless audit clears removal.

---

## Prompt-control header (N4) — ✅ Implemented 2026-07-17

**Goal (L7):** LLM-facing chunk headers use `source_span=…` instead of `Lines: …`, so the model is less likely to treat a line range as a numbered-list cue (e.g. answering `29.`).

### N4 deliverables (locked scope)

| In | Out |
|----|-----|
| `formatChunkForPrompt` → emit `source_span=` | CommandProcessor / `generateAndSanitizeChat` (**N6**) |
| Required tests **N-T8**, **N-T8b**, **N-T8c** | Changing Plan M cue/anti-transcript fixture `Lines: 1-3` |
| Docs status + completed log on implement | RAG retrieval, ranking, grounding floor, citations, telemetry `start_line`, N5 UI |

### Focus rules (locked)

1. **Single function change** — only `formatChunkForPrompt` in `chat_retrieval_boost.cpp` (optional one-line L7 note on the header declaration).
2. **Span emission rules** (preserve today’s range logic):
   - `startLine <= 0` → omit span line entirely.
   - `startLine > 0` and `endLine > startLine` → `source_span={start}-{end}` (e.g. `source_span=29-33`).
   - `startLine > 0` and `endLine <= startLine` → `source_span={start}` only (e.g. `source_span=5`).
3. **`Document:` + chunk body + caller `---` separators unchanged.**
4. **Do not update** Plan M `testPlanMChatPromptCueAndAntiTranscript` fixture — that string tests anti-transcript assembly, not the formatter. Mixing concepts is out of scope.
5. **No CP / stop / sanitize / retrieval changes.**

### Explicit non-goals (locked)

- **N4 does not guarantee** the model will cite `source_span` correctly. It only prevents the header from resembling a numbered-list instruction.
- **N4 does not alter** RAG retrieval, ranking, grounding, or citations.
- Chat RAG observability / GUI metrics may still expose real line numbers (`start_line`, etc.).

### N4 tests (locked — all required)

| ID | Case | Expect |
|----|------|--------|
| **N-T8** | `startLine=29`, `endLine=33` | Output contains `source_span=29-33`; must **not** emit `Lines:` as an LLM-facing header |
| **N-T8b** | `startLine=5`, `endLine=5` | `source_span=5` present; no `-5` range suffix; no `Lines:` header |
| **N-T8c** | `startLine=0` (any end) | No `source_span=` line and no `Lines:` header |

### N4 verify

- N-T8, **N-T8b**, **N-T8c** pass.  
- **Verify `formatChunkForPrompt` no longer emits `Lines:` as prompt content** (assert on function output / active emit path — not a blanket ban on the substring elsewhere in `chat_retrieval_boost.cpp`, e.g. comments or unrelated logs).  
- Grep: no N4 edits to `command_processor.cpp`, stops, sanitize, Plan M cue fixture.

### Live-path note after N4

N4 only affects turns that inject RAG via `formatChunkForPrompt`. Greeting-skip / empty-injection paths are unchanged. Transcript-loop replies after N4 alone still require **N6**.

---

## Wire chat generation safety (N6) — ✅ Implemented 2026-07-17

**Goal (L3/L4):** All three conversational arms use one CommandProcessor wrapper that calls `generateAndSanitizeChat`, then tools/memory/HTTP. After N6: **no** conversational `llm.query(` in `command_processor.cpp`.

### N6 deliverables (locked scope)

| In | Out |
|----|-----|
| **Required** private CP conversational generation wrapper; all three arms call it | Reopening N2 / changing `chat_generation_safety.*` |
| Class A → CP formats error via `LLMInterface::formatProviderError` (thin wrap of existing helper); **no** `processToolCall` | N5 diagnostics UI; history scrub (L11) |
| Class B/C/D → `processToolCall(sanitized)` → memory once | Stops / `source_span=` / grounding / greeting classifier changes |
| Telemetry on `CHAT_RAG_RESPONSE` + generation stage | Non-chat / planner `llm.query` callers |
| Tests **N-T9**, **N-T10**, **N-T11**, **N-T12** | |

### Focus rules (locked)

1. **One required private wrapper** in `CommandProcessor` (e.g. `runConversationalGenerate`). All three arms (`no_index`, `greeting_skip`, indexed) call it — no triplicated generate/tool/memory policy.
2. **Options:** `max_tokens = kChatMaxTokens`; `stop_sequences = chatStopSequences()` (`{}`); `use_greeting_fallback = true` **only** on the greeting arm.
3. **Class A (`!provider_ok`):** format user-facing error in CP; skip `processToolCall`; memory write **once** with that error string.
4. **Success / fallback path:** `processToolCall(gen.sanitized_text)` then existing memory block once (user + assistant).
5. **Do not modify** `chat_generation_safety.h` / `.cpp` in N6 (helper already complete). Do not reopen N2 contracts.
6. **Expose** `LLMInterface::formatProviderError(detail)` (or equivalent) wrapping the existing static formatter — CP must not invent new error copy.

### Telemetry (locked)

Extend `ChatRagResponseRecord` / `responseToJson` and the `generation` decision-trace stage with **flags, reasons, and character counts only**:

| Field | Notes |
|-------|--------|
| `fallback_used` | From `gen` (stop hardcoding `false`) |
| `raw_answer_chars` / `sanitized_answer_chars` | Sizes only |
| `sanitize_reason` | Enum/string reason |
| `retried_without_stops` / `used_stops` / `provider_ok` | Flags |
| `finish_reason` | When known; else empty |

**Hard rule:** **No raw generation text in telemetry.** No full prompts, no `raw_text` / `sanitized_text` dumps (L10).

Greeting-skip **context** telemetry shape must not regress (N-T9).

### Explicit non-goals (locked)

- N6 does not promise strong model quality — only one safe generation boundary.
- N6 does not alter RAG retrieval, ranking, grounding, or `source_span=`.
- N6 does not scrub historical poisoned sessions.

### N6 tests (locked)

| ID | Case | Expect |
|----|------|--------|
| **N-T9** | Greeting-skip telemetry | Plan M greeting context fields unchanged |
| **N-T10** | Memory once | One assistant memory write per turn even if helper retried internally |
| **N-T11** | CP + mock scaffold | Sanitized or L8 fallback returned; no `Empty completion text`; one memory write; **stored assistant memory must not contain transcript scaffolding markers** (`[User]`, `[Agent]`, `[RAG Context]`, or leading `📝` marker lines) |
| **N-T12** | CP provider failure | Mock `provider_ok=false`, `provider_error="connection failed"` → `processToolCall` not called; CP-formatted error returned; memory write once |

### N6 verify

- N-T9–**N-T12** pass.  
- Grep: three arms → wrapper → `generateAndSanitizeChat`; **no** conversational `llm.query(` in `command_processor.cpp`.  
- `chat_generation_safety.*` untouched.  
- Telemetry JSON has counts/flags/reasons only — no raw generation body fields.  
- Docker smoke (post-rebuild): `hello` is truncated/fallback, not a multi-turn transcript script.

---

## Checkpoint sequence

| ID | Work | Status |
|----|------|--------|
| **N0** | Spec lock + Plan M G3 revision note | 🔒 Locked 2026-07-17 |
| **N1** | Pure sanitize foundation (`SanitizeOutcome` + types + tests) | ✅ Implemented 2026-07-17 |
| **N2** | `queryDetailed` + soft-empty + `generateAndSanitizeChat` + taxonomy | ✅ Implemented 2026-07-17 |
| **N3** | Chat empty stops via `chatStopSequences()`; revise Plan M stop tests | ✅ Implemented 2026-07-17 |
| **N4** | `source_span=` prompt-control header | ✅ Implemented 2026-07-17 |
| **N6** | Wire all three CP arms via one wrapper; telemetry; N-T9–T12; grep gate | ✅ Implemented 2026-07-17 |

**N5** — separate plan: [`plan_n5_grag_diagnostics_display.md`](plan_n5_grag_diagnostics_display.md) ✅ Implemented 2026-07-18.

Each checkpoint: **Plan → Review → Refine → Lock → Implement → Verify → STOP** (AGENTS.md) unless a batch Implement is explicitly authorized.

---

## Exact files (full plan)

| File | Role |
|------|------|
| `docs/plan_n_chat_generation_safety.md` | This lock |
| `docs/plan_m_grounded_retrieval_gate.md` | G3 stop revision pointer |
| `external/basic_agent/include/chat_generation_safety.h` (+ `.cpp`) | Helper + sanitize |
| `external/basic_agent/include/inference_types.h` | `finish_reason` |
| `external/basic_agent/src/llama_server_client.cpp` / `ollama_client.cpp` | Soft-empty + `finish_reason` |
| `external/basic_agent/include/llm_interface.h` (+ `.cpp`) | Structured query for chat |
| `external/basic_agent/include/chat_prompt_config.h` | Empty `chatStopSequences()` |
| `external/basic_agent/src/chat_retrieval_boost.cpp` | N4 `source_span=` |
| `external/basic_agent/src/command_processor.cpp` | N6 wire helper |
| `external/basic_agent/include/chat_rag_observability.h` / `chat_rag_logger.cpp` | Gen diagnostic fields |
| `tests/unit_tests.cpp` | N-T1–T11 |
| `docs/completed_improvements_log.md` / `improvements.md` / `docker/README.md` | Closeout |

**Not in Plan N:** `src/GragDiagnosticsPanel.cpp`

---

## Test plan (full)

| ID | Case | Checkpoint |
|----|------|------------|
| **N-T1** | Transcript loop sanitize; mid-sentence preserved; JSON tool body preserved | **N1** |
| **N-T2** | `[User] help` → `empty_after_sanitize` + `all_scaffold` | **N1** |
| **N-T2b** | Leading scaffold + useful body → `stripped_leading_scaffold` | **N1** |
| **N-T3** | Provider empty → retry → success | **N2** |
| **N-T3b** | Sanitize-empty then retry success | **N2** |
| **N-T3c** | Llama soft-empty parse + finish_reason | **N2** |
| **N-T4** | Empty → retry empty → fallback | **N2** |
| **N-T5** | Transport failure → error, no retry | **N2** |
| **N-T5b** | Malformed JSON → ok=false | **N2** |
| **N-T6b** | `query()` compatibility (soft-empty → `""`; hard fail → `[Error]`) | **N2** |
| **N-T6** | `chatStopSequences()` returns empty | **N3** |
| **N-T6c** | `generateAndSanitizeChat` with empty stops → `used_stops==false` | **N3** |
| **N-T7** | Non-empty **explicit** stop serialization still valid | **N3** |
| **N-T8** | `formatChunkForPrompt` range → `source_span=29-33`; no `Lines:` header | **N4** |
| **N-T8b** | Single-line span → `source_span=5` | **N4** |
| **N-T8c** | `startLine=0` → no span / no `Lines:` header | **N4** |
| **N-T9** | Greeting skip telemetry unchanged | **N6** |
| **N-T10** | Memory once per turn | **N6** |
| **N-T11** | CP + mock scaffold → sanitized/fallback; one memory write; **no scaffold markers in stored assistant memory** | **N6** |
| **N-T12** | CP Class A: no tool call; formatted error; one memory write | **N6** |

---

## Success criteria

1. `hello` does not return `[User] help` or `llama_cpp: Empty completion text`.  
2. Transcript loops truncated before memory/HTTP.  
3. One generation boundary; no conversational `llm.query` left in CP.  
4. Chat omits transcript stops; stop API remains for others.  
5. Chunk prompts use `source_span=`, not `Lines:`.  
6. Telemetry retains raw vs sanitized evidence without full prompt dumps.

---

## Protocol lock

This document is **locked**. Do not silently revise contracts during implementation. If implementation forces a contract change, stop and request approval (AGENTS.md Protocol Lock Rule).

STATUS: PLAN N COMPLETE (N1–N6 + N5)
