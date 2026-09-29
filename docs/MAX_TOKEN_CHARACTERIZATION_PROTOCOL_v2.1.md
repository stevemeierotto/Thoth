# Thoth Max-Token Characterization Protocol

| Field | Value |
| --- | --- |
| Document title | Thoth Max-Token Characterization Protocol |
| Identifier | MTCP v2.1 — Intermediate Generation Ceiling Characterization |
| Lifecycle | **SEALED** |
| Seal date | 2026-09-29 |
| Parent evidence | Sealed MTCP v1.1, `docs/MAX_TOKEN_CHARACTERIZATION_PROTOCOL_v1.1.md`, seal `bf6602ca26c580e75ea61b07f09896ee788dee94` |
| Parent result | `docs/MTCP_V1_1_RESULT.md` |
| Path | `docs/MAX_TOKEN_CHARACTERIZATION_PROTOCOL_v2.1.md` |

MTCP v2.1 is **SEALED** as of 2026-09-29. This seal is prospective. It fixes the scientific specification before any 768 generation. It is a new protocol. It does not amend sealed MTCP v1.0 or sealed MTCP v1.1.

The seal does not authorize a 768 run, a seed gate, a 512 rerun, a 1024 rerun, or a 2048 run.

## 1. Purpose

MTCP v1.1 validly compared the shared generation ceilings 512 and 1024. 1024 was not a material improvement over 512. The selected ceiling was 512. Adequacy was not met. Stage B was not triggered. 2048 was not run.

The locations of the three pressure slots changed between those ceilings. v1.1 was not designed to ask whether an intermediate ceiling changes that burden. v2.1 asks that question with one new condition.

## 2. Research questions

Primary: holding the experimental subject and the frozen controls in §5 identical to the valid MTCP v1.1 run, except for this protocol’s identity and the shared generation ceiling, does a shared generation ceiling of 768 materially reduce ceiling pressure and length-associated structured failure relative to the preserved 512 condition, without increasing programmatic fallbacks?

Secondary, descriptive only: within the tested set 512, 768, and 1024, does the pattern of those same counts support a statement of non-monotonic behavior?

Neither question assumes that 768 is better. A null result is a valid result.

“Preferred under this protocol” and “adequate” are different claims. v1.1 selected 512 while finding that 512 was not adequate. v2.1 keeps that distinction.

## 3. Historical boundary

MTCP v1.1 remains complete and unchanged. Its 512 and 1024 observations are historical comparator evidence. They may be used in the v2.1 decision only after the comparability proof in §5 passes.

Do not rescore the v1.1 calls under a new definition. Where v2.1 uses a v1.1 metric or threshold, use the original definition in sealed v1.1 §§14–17 and the implementation clarification that a zero completion count with `finish_reason` other than `stop` is not, by itself, an empty-synthesis structured failure. Empty synthesis remains a completed provider response whose text was empty.

Do not pool MTCP v1.0 evidence. MTCP v1.0 Stage A remains invalid.

Do not rerun 512. Do not rerun 1024. Do not run 2048. v2.1 does not reopen the v1.1 Stage B predicate.

## 4. Independent variable

The only new experimental value is the shared generation ceiling **768**.

`THOTH_GENERATION_MAX_TOKENS=768` for every scored call in the new condition. It is one value on every wired text-generation path that the v1.1 condition used: chat, plan, plan retry, revision, revision retry, reflection planning if it occurs, and synthesis.

768 is not selected because it lies between 512 and 1024. The comparison does not require one of the three ceilings to be named a winner.

## 5. Comparator admissibility

The preserved 512 and 1024 conditions are admissible comparators only when the 768 run uses the same experimental subject and the same controls as the v1.1 result, except this protocol’s identity and the ceiling 768.

Before any 768 generation, record the live values and compare them with `docs/MTCP_V1_1_RESULT.md`. The comparison is inadmissible if any of the following differ:

| Control | Required identity |
| --- | --- |
| Product execution commit | `adec59e4d9981a2abb46a5eea350ebfc65053a61` |
| Engine execution commit | `a31215db040eba5fa3b2fbd87fb9c96728c38efa` |
| Engine image | `sha256:a272bd9f2a4d6867a6bf12d702ef6ce879d1e788af9ca54fab8acb8c64e639c3` |
| llama.cpp image | `sha256:823b6f019cafbee8878dfdd0d4750eae4f81dfafb60dc1fbefb66794a59903c8` |
| llama.cpp build | `b9994-14d3ba45f` |
| Chat model path and SHA-256 | `/models/Qwen2.5-7B-Instruct-Q4_K_M.gguf`, `65b8fcd92af6b4fefa935c625d1ac27ea29dcb6ee14589c55a8f115ceaaa1423` |
| Embedding model path and SHA-256 | `/models/nomic-embed-text.gguf`, `f7af6f66802f4df86eda10fe9bbcfc75c39562bed48ef6ace719a251cf1c2fdb` |
| Backend and inference mode | `llama_cpp`, `chat` |
| Chat context | `n_ctx` 8192 |
| Embedding context | 512 |
| Seed | 17001, sent on the generation request |
| Text observation limit | 4356 seconds |
| Embedding timeout | 300 seconds |
| Sampling | the effective generation values in §5.1 |
| Corpus fingerprint | `155f9f0be3a5210730fd5042bcaf228ea1d4421d21b411ea1890e769d58a8921` |
| Retrieval configuration | `b4eab95e104f84de05ee7c1ac26e1117447104c983ce924942e987db073df396` |
| Task definitions | sealed v1.0 Appendix A prompts and goals, cited by v1.1, task-set `a41031ff84308edbaf40804f4e96aae8b85395a0ff9e00fffc7afe402002ec90` |
| R1 fixture | the frozen plan and failed-step result; wrapper SHA-256 must equal `a3ec75149480a7574cbf36358e05a3154fe4f2dbdf9ea64537efed40795fd144` |
| S0-CHAR | archive `76153c66d1fa10bf0c05d7a568eff5d4b0b83c8332988de62f193985a2b7d9c3`, restored per the v1.1 procedure to `memory.db` `a7bc9da2abbe814805965109ad016b0606da7786ebb3d4574f069bd1ae15bb52` and index `aaf5056496dc943081b745675b794b58131dca8015160b805dad5d775bc9bf3b` before every task |
| Task order | C1, C2, C3, G1, G2, R1, one trial each |
| Isolation | TOKEN-CHAR only; C6-LIVE, EGAR-LAB, and the operational DEV workspace are not the starting state |
| Provider-usage semantics | reported integers versus explicit unavailable; unavailable is not numeric zero |
| Progress observer | fixed 60-second `/slots` poll, at most one request in flight, no `fail_on_no_slot`, `LLAMA_SERVER_SLOTS_DEBUG` unset; gaps stay diagnostic |
| Validation and fallback | the production planner and revision behavior used in v1.1, not a new retry policy |

If any row differs, stop before the first 768 generation. Do not treat the preserved 512 or 1024 counts as directly comparable. Do not adjust a prompt, a hash, a sampler, a timeout, or the decision threshold to repair the mismatch inside this protocol.

### 5.1 Effective sampler identity

Verified against the frozen v1.1 subject, Engine `a31215d` and llama.cpp build `b9994-14d3ba45f`. These are the values that must be in force for the 768 comparison. They are not inferred from the image digest alone.

| Control | Effective value | Where it was verified |
| --- | --- | --- |
| temperature | 0.7 | `Config` default, sent on every completion and chat request. The v1.1 TOKEN-CHAR workspace had no `config.json`, so the compiled default was the value used. |
| top_p | 1.0 | Same request path. |
| top_k | 40 | llama.cpp `/props` `params.top_k`. The request does not send `top_k`. |
| min_p | 0.05 | llama.cpp `/props` `params.min_p`. The request does not send `min_p`. |
| repeat_penalty | 1.0 | llama.cpp `/props` `params.repeat_penalty`. The request does not send it. |
| presence_penalty | 0 | llama.cpp `/props` `params.presence_penalty`. The request does not send it. |
| frequency_penalty | 0 | llama.cpp `/props` `params.frequency_penalty`. The request does not send it. |
| mirostat | 0 | llama.cpp `/props` `params.mirostat`. The request does not send it. |
| inference seed | 17001 | `THOTH_INFERENCE_SEED` on the v1.1 subject, written into the request by `applyConfiguredSeed`. |

Before the first 768 generation, read `/props` again. The unsent fields above must still match. The request payload must still send temperature 0.7, top_p 1.0, and seed 17001.

An idle `/props` reading, taken when no request is in flight, reports temperature 0.8, top_p 0.95, and seed `4294967295`. Those are the server defaults Thoth replaces on each scored call. They are not the effective generation values. A 768 run whose request omits the override, or whose `/props` unsent fields differ from this table, fails §5.

The ceiling itself must be 768 on every scored call. A call whose `requested_max_tokens` is not 768 invalidates the 768 condition.

## 6. Reproducibility mode

The v1.1 seed gate passed in deterministic paired mode on this subject. Both raw completions were byte-identical, SHA-256 `0061a7bd6fd6e588ffd74537644f0953f13f76d1ebd3c145a7e23607aca20683`, finish reason `stop`, and reported usage 235 prompt tokens and 52 completion tokens. The gate prompt was written to finish well below 512 tokens. Changing only the ceiling from 512 or 1024 to 768 does not change the reproducibility question that gate answered.

If §5 passes, v2.1 adopts that PASS. The 768 condition is deterministic paired mode with seed 17001. No new seed gate is run.

If §5 fails, do not start a substitute gate and do not switch to fallback mode. Stop. A new gate would be a different protocol.

## 7. The 768 condition

Execute exactly one condition:

768: C1, C2, C3, G1, G2, R1.

Each task starts from a restored certified S0-CHAR. There is no adaptive rerun and no prompt change after a result is seen. Truncation, validation failure, fallback, and a poor cognitive outcome are experimental evidence. They are not, by themselves, reasons to repeat the task.

Chat calls may again write no progress file. That is the v1.1 chat-path behavior. Record it. It does not by itself invalidate a call that otherwise has provider-reported usage and the required generation-call fields.

## 8. Measurements

Use the v1.1 definitions without new thresholds:

- slot `(task_id, call_type, ordinal)`
- provider validity and reported versus unavailable usage
- prompt tokens, completion tokens, and requested ceiling
- finish reason
- ceiling pressure
- validation, retry, and programmatic fallback
- length-associated structured failure
- synthesis outcome, including `synthesis_observed_empty` only for a completed empty provider text
- elapsed time
- context overflow under `prompt_tokens + requested_max_tokens <= 8192` when prompt usage was reported
- diagnostic `/slots` progress and gaps

Progress samples do not replace provider usage and do not enter §9.

The 512 and 1024 counts used in §9 are the counts already recorded in `docs/MTCP_V1_1_RESULT.md`: 3 pressure slots, 2 programmatic fallbacks, and 3 length-associated structured failures at each ceiling. They are not recomputed under a new rule.

## 9. Decision rule

Apply this rule only if the 768 condition is valid under §10 and the comparators are admissible under §5. Do not change the thresholds after seeing the 768 calls.

### Material improvement of 768 over 512

Use the sealed v1.1 material-improvement clauses. 768 is a **material improvement** over the preserved 512 condition when either clause is true:

1. 768 has at least two fewer ceiling-pressure slots than 512, the number of length-associated structured failures does not increase, and the number of programmatic fallbacks does not increase.
2. 768 has at least one fewer programmatic fallback than 512, at least one removed fallback is length-associated at 512, and the number of ceiling-pressure slots does not increase.

Equal aggregate counts are not a material improvement, including when the pressured slots are different tasks or call types. A pressure slot that is new at 768 counts in the 768 total.

An increase in pressure slots, in length-associated structured failures, or in programmatic fallbacks blocks the clause that would otherwise pass.

### Material worsening

768 is **materially worse** than a comparator (512 or 1024) only when either clause is true:

1. 768 has at least two more ceiling-pressure slots than the comparator, while the number of length-associated structured failures does not decrease and the number of programmatic fallbacks does not decrease.
2. 768 has at least one more programmatic fallback than the comparator, at least one added fallback is length-associated at 768, and the number of ceiling-pressure slots does not decrease.

A smaller increase in any of the three counts is reported with its sign. It is not material worsening. Equal counts, including when the pressured slots move, are neither material improvement nor material worsening.

### Adequacy at 768

768 is **adequate** when its valid decision set has:

- zero ceiling-pressure events, and
- zero length-associated structured failures

Residual pressure is reported even if another sentence names 768 under this protocol. Adequacy is not implied by selection.

### Selection under this protocol

768 is preferred under v2.1 only when it is a material improvement over 512. Record whether 768 met adequacy.

If 768 is not a material improvement, do not prefer 768. The historical v1.1 selection of 512 stands, including the v1.1 statement that adequacy was not met. v2.1 does not award 512 a second time and does not change that adequacy finding.

Do not prefer 1024. Do not prefer 768 merely because its counts are closer to zero than 1024’s counts. Do not prefer 2048. There is no requirement that 512, 768, or 1024 be named the winner.

### Preserved 1024 comparator

Apply the same two clauses with 1024 as the reference and 768 as the candidate. This comparison is descriptive.

- If 768 is a material improvement over 1024, say so.
- If 768 is materially worse than 1024, say so.
- If the three counts are equal, say the aggregate burden is unchanged and record the slot identities.
- Smaller increases, decreases short of material improvement, and mixed directions are reported and are not material worsening.

The 1024 comparison does not override the selection rule against 512. A 768 condition that improves on 1024 and does not improve on 512 is not preferred under this protocol. 1024 is not eligible for reselection. 2048 is not opened.

### Non-monotonic pattern

Two descriptive statements are defined. Neither is required for a result, and neither prefers a ceiling.

The ceilings are consistent with a **material intermediate minimum** only when 768 is a material improvement over preserved 512 and a material improvement over preserved 1024.

The ceilings are consistent with a **material intermediate maximum** only when 768 is materially worse than preserved 512 and materially worse than preserved 1024.

The following are insufficient to characterize a material non-monotonic relationship:

- equal aggregate counts, with or without a change in which slots are pressured
- pressure-slot movement alone
- a material change against only one of the two preserved ceilings
- mixed directions, including one count falling while another rises
- an increase too small to meet the material-worsening clauses
- inspection of answer text, latency, or progress samples

Report the three count triples either way. These statements are separate from which ceiling, if any, is preferred under the 768-versus-512 rule.

## 10. Validity and stopping

Carry forward sealed v1.1 §§18–20. In particular:

- `provider_ok` false is an invalid call
- unavailable provider usage is an invalid call and is not numeric zero
- context overflow on reported prompt tokens invalidates the call
- a wrong model, image, hash, ceiling, seed, S0 restore, or task attribution invalidates the affected trial or the 768 condition as v1.1 specifies for the analogous defect
- one invalid scored call invalidates its task and the 768 condition; do not score a partial task list as the condition
- stop generation if three provider failures occur in a row, if `n_ctx` is not 8192, if the text timeout is not 4356, or if the workspace or model mount is C6-LIVE or EGAR-LAB

A poor answer, a validation failure, a fallback, or a ceiling hit is evidence. It is not an infrastructure abort.

Progress gaps do not change `provider_ok` and do not repair an invalid call.

## 11. Outcomes the analysis must be able to state

After a valid 768 condition and an admissible comparison, the report states, without forcing one of them:

- 768 is a material improvement over 512, and whether 768 is adequate
- 768 is not a material improvement over 512
- 768 is materially worse than 512, or a smaller change is descriptive only
- 768 is mixed relative to 512, with each count’s direction
- the same statements relative to the preserved 1024 counts, without reselecting 1024
- the 512/768/1024 pattern is consistent with a material intermediate minimum, consistent with a material intermediate maximum, or insufficient to characterize a material non-monotonic relationship

If the comparability proof fails, the only permitted conclusion is that 768 was not compared. No ceiling is preferred by v2.1 in that case.

## 12. What this seal does not do

It does not edit v1.0, v1.1, the v1.1 result, the runner, or the analyzer. It does not change S0-CHAR, C6-LIVE, or EGAR-LAB. A later implementation may apply §9 to the new 768 records and the frozen v1.1 counts. That implementation is not part of this sealed text.
