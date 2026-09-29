# Thoth Max-Token Characterization Protocol

| Field | Value |
| --- | --- |
| Document title | Thoth Max-Token Characterization Protocol |
| Identifier | MTCP v1.1 |
| Lifecycle | **SEALED** |
| Seal date | 2026-09-29 |
| Parent | MTCP v1.0, sealed at `docs/MAX_TOKEN_CHARACTERIZATION_PROTOCOL.md`, seal commit `a3690f7fc50da920992829bcbc39e19aa983894f` |
| Minimum implementation baseline | Product `fa8667e482221cfd2ca79fa4991482f2870dbe97`. Engine `4a2d8a8f37114a2509632f7998eb3b72382e20a3`. |
| Path | `docs/MAX_TOKEN_CHARACTERIZATION_PROTOCOL_v1.1.md` |

MTCP v1.1 is **SEALED** as of 2026-09-29. This seal is prospective. It fixes the scientific specification before progress-observer implementation, TOKEN-CHAR image creation for v1.1, the v1.1 seed gate, and any v1.1 characterization generation. The seal does not itself perform those steps.

v1.0 remains sealed. This document does not edit that file, its appendix, or the historical Stage A evidence.

## 1. Purpose

MTCP v1.1 defines how Thoth will characterize one deployment setting: the maximum number of tokens a wired text generation is allowed to produce.

The characterization exists to choose the smallest tested ceiling that provides adequate generation headroom, before that choice is considered for a later C6 or EGAR inference freeze. It is a configuration characterization. It is not the official C6.4 longitudinal experiment, not the EGAR learning experiment, not a comparison of Thoth with another system, and not a claim that longer answers are better.

v1.1 keeps the v1.0 scientific question. It changes the observation limit and the usage record so that the text-generation timeout, and a missing provider usage object, do not enter the ceiling comparison as if they were generated-token measurements.

Every condition that is compared with another condition must use the same Product SHA and the same Engine SHA. A SHA change between Stage A and Stage B makes the comparison invalid. The minimum baseline above is the identity-propagation floor. The execution SHAs are the commits that contain the accepted progress telemetry after that telemetry has been implemented and verified. Those execution SHAs are written in the condition manifest. They are not invented by this sealed text.

## 2. Research question

Holding the inference model and its bytes, the llama.cpp runtime, context capacity, sampling configuration, retrieval configuration, task definitions, starting cognitive state, and the compared software versions constant, does increasing the shared maximum generated-token ceiling above 512 materially reduce output truncation or length-associated generation failure, and what is the smallest tested ceiling that is adequate under the rule in this protocol?

The decision favors the smallest adequate ceiling. It does not favor the longest output.

The text-generation timeout is an observation limit. It is not this question’s independent variable.

## 3. Historical boundary

MTCP v1.0 Stage A is **INVALID**. It selected no ceiling. It contributes no scored observations to v1.1.

No v1.0 characterization call may be reused, rescored, substituted, or pooled with a v1.1 trial. v1.0 elapsed times may appear in this sealed text only as the locked inputs of the observation-limit formula in §5. Those inputs are not v1.1 evidence about a ceiling.

v1.1 scored work begins from a fresh S0-CHAR for every scored task, under §8. The v1.0 seed-gate record is not a v1.1 seed gate. v1.1 runs its own gate only after the execution build that contains the verified progress observer exists.

## 4. Independent variable

The only intended independent variable is the **shared maximum generated-token ceiling**.

Tested values:

| Condition | Ceiling |
| --- | --- |
| A0 | 512 |
| A1 | 1024 |
| B0, only if triggered | 2048 |

The ceiling is one value applied to every wired text-generation path under comparison: ordinary chat, plan generation, plan retry, plan revision, revision retry, reflection planning if it occurs, synthesis, and, if a defined task happens to call them, self-correction and episodic batch summary.

Context window, prompt length, the model’s native training context, and the text-generation timeout are not this variable. They are controls or observations.

## 5. Observation limit

The common text-generation timeout is **4356 seconds**.

It is defined prospectively as:

`ceil(2 × R × C_max)`

where:

- `R = 544447 / 512` seconds per completion token, the slowest successful long-generation wall-clock rate in the preserved v1.0 timing record (512 R1 `revision_retry`: 544447 ms, 512 completion tokens, `provider_ok` true)
- `C_max = 2048`, because 2048 remains conditionally scoreable under Stage B

`ceil(2 × (544447 / 512) × 2048) = 4356`.

This timeout is the same for ceiling 512, ceiling 1024, and, if Stage B is triggered, ceiling 2048. It is also the text-generation timeout of the v1.1 seed gate, so the gate and the scored conditions use one observation limit.

The value is justified for the sealed MTCP workload on the experimental hardware that produced the v1.0 timing record. It is not a general Thoth timeout and not an Apollo deployment recommendation.

The embedding timeout stays **300 seconds**.

A generation that reaches this observation limit is a provider or transport failure under §19. It is not a ceiling-pressure event and it is not scored by substituting in-flight progress counts.

## 6. Controlled variables

The following must be the same across every condition that enters one decision. Where a setting comes from the llama.cpp process rather than from Thoth, it is still controlled and must be recorded from that process.

- Product SHA and Engine SHA, equal across conditions, each at or after the §1 minimum baseline, and each equal to the manifest’s execution SHA
- Engine image identity
- chat model path and chat model SHA-256
- embedding model path and embedding model SHA-256
- llama.cpp image digest and build identifier
- inference backend
- chat inference mode
- chat context `n_ctx`, required value 8192
- embedding context
- temperature, top_p, and the effective values of top_k, min_p, repeat penalty, presence penalty, frequency penalty, and mirostat parameters exposed by the runtime
- characterization seed, in deterministic paired mode
- text-generation timeout, required value 4356 seconds, and embedding timeout, required value 300 seconds
- embedding strict/fallback policy
- retrieval-configuration hash
- characterization corpus hash
- task prompts, task order, and the revision fixture, which are the sealed v1.0 Appendix A assets named in §26
- S0-CHAR procedure and logical starting state
- reflection maximum, equal to the default compiled into the Engine SHA under comparison
- workspace isolation from C6-LIVE, EGAR-LAB, and operational DEV state

Sampler values that Thoth does not send are recorded at the start of each condition from the running server. The characterization must not assume an earlier server’s sampler report is still in force.

## 7. Context requirement

Every condition uses **`n_ctx = 8192`**.

This is a methodological setting for MTCP. It is not a permanent C6 or EGAR context decision.

8192 is required because:

- the operational DEV runtime has already run the candidate model at 8192
- a candidate output ceiling has to leave room for a realistic prompt
- a 2048-token output ceiling cannot be compared fairly inside a 2048-token total context, because the prompt would have to be discarded or the call rejected
- a different context in any condition would confound the ceiling comparison

Before any scored generation in a condition, read `n_ctx` from the running llama.cpp server. If it is not 8192, that condition is invalid and must be aborted. Do not score any of its calls.

## 8. Environment and isolation

The characterization environment is **TOKEN-CHAR**.

| Property | Value |
| --- | --- |
| Compose project | `thoth-char` |
| Engine port | 8093 |
| Workspace volume | dedicated to `thoth-char` |
| Log volume | dedicated to `thoth-char` |
| C6 window | none |
| C6 corpus | none |
| EGAR corpus | none |
| C6-LIVE state | not mounted and not written |
| EGAR-LAB state | not mounted and not written |
| Operational DEV workspace | not used as the starting state |

TOKEN-CHAR may mount the existing model volume read-only when the manifest records the SHA-256 of the chat GGUF and the embedding GGUF. Model bytes are an immutable input, not a place to store experimental state.

C6-LIVE and EGAR-LAB are out of scope. Creating, starting, stopping, ingesting into, or reading those environments for this protocol is a protocol violation. Opening a C6 window is a protocol violation.

TOKEN-CHAR is disposable. After the decision is recorded, its workspace must not be reused as S0 for C6 or EGAR.

## 9. S0-CHAR

**S0-CHAR** is the logical starting state of every scored task. v1.1 uses the same procedure and the same certified archive as the v1.0 preparation. It does not create a new snapshot for this seal.

Certified archive, outside the repository:

| Artifact | SHA-256 |
| --- | --- |
| `s0-char.tar` | `76153c66d1fa10bf0c05d7a568eff5d4b0b83c8332988de62f193985a2b7d9c3` |
| `s0-char.json` | `3afdb4d20aa2acb6ea1e711523be0ac9ae57e54750adb53103a69b605bdfd528` |

That state contains:

- an initialized memory schema
- the sealed retrieval configuration
- the sealed synthetic corpus, indexed
- no characterization goals
- no trajectories, strategies, or episodic memories produced by another task or another ceiling
- no chat messages or plans from another task or another ceiling
- no C6 window file and no C6 cohort fingerprint
- no EGAR S0 and no EGAR corpus

**Trial rule.** Each task identity is one trial. Each trial starts from S0-CHAR. Completing C1 does not leave memory in place for C2. Completing 512 does not leave memory in place for 1024. A later ceiling, and a later task at the same ceiling, must not inherit plans, chat, trajectories, or strategies from an earlier trial. A v1.0 trial is not a starting state for a v1.1 trial.

Logical equivalence is verified before the first generation of each trial. Required checks:

- retrieval-configuration hash equals the sealed hash
- corpus hash equals the sealed hash
- the index source is exactly the sealed corpus
- characterization goals, plans, trajectories, strategy records, episodic memories, and chat messages are absent
- context, model hashes, sampler record, ceiling, seed mode, and the 4356-second text timeout match the condition manifest

Database bytes need not match across trials. Initialization timestamps may differ. Equivalence is the checklist above. If a trial fails the checklist, that trial is invalid.

## 10. Corpus and workload

The corpus, prompts, goals, diagnostic prompt, seed, and R1 fixture are the sealed MTCP v1.0 Appendix A assets. Their identities are §26. v1.1 does not restate those bytes.

Six task identities cover chat, planning, synthesis, and revision.

| Id | Class | Role in the decision |
| --- | --- | --- |
| C1 | Short chat | Low-pressure control. A ceiling hit here is evidence that the run is not behaving like a short generation. |
| C2 | Retrieval-grounded chat | Short answer that must use the synthetic corpus. |
| C3 | Structured chat | Prompt written to ask for a long structured answer, so ceiling pressure has a chance to appear. |
| G1 | Goal | Simple retrieval, planning, and synthesis of one corpus fact. |
| G2 | Goal | Longer synthesis that compares all three documents. |
| R1 | Revision | One controlled call to the production plan-revision path, using the sealed fixture plan and the sealed failing step result. |

R1 is a fixture so revision does not depend on whether a previous goal happened to fail a step. The fixture contains no experience from C1–G2.

**Call types that count when they occur inside a defined task:** `chat`, `plan`, `plan_retry`, `revision`, `revision_retry`, `synthesis`. Reflection planning, if the production reflection maximum causes it during G1 or G2, is recorded as `plan` for that task and is scored. The reflection maximum itself stays at the Engine default compiled into the execution SHA.

Self-correction and episodic batch summary are not separate tasks and are not intentionally invoked. If a defined task emits them, they are scored as slots of that task. They are not required for a valid condition.

Deterministic run order, each from a new S0-CHAR:

1. At ceiling 512: C1, C2, C3, G1, G2, R1
2. At ceiling 1024: C1, C2, C3, G1, G2, R1

The seed gate is not part of this order. It uses a disposable state that is destroyed before Stage A.

No characterization trial may run before this protocol is sealed.

## 11. Stage structure

### Stage A

Compare ceiling 512 with ceiling 1024 on task identities C1, C2, C3, G1, G2, and R1. In deterministic mode, one trial of each task at each ceiling. In fallback mode, the reduced set and the repetition count in §13.

### Stage B

2048 is not part of the default workload. It is started only when the Stage A predicate in §17 is true. Curiosity, inspection of answer text, or a preference for longer answers does not start Stage B.

Stage B repeats only the Stage B task set: every Stage A task identity that, at 1024, still has at least one ceiling-pressure event or at least one length-associated structured failure. Each of those trials starts again from S0-CHAR. Tasks that were clean at 1024 are not re-run. No new task identity may be added.

If the Stage A predicate is false, a 2048 run is a protocol violation and its generations are not evidence.

## 12. Seed reproducibility gate

Production Thoth does not send a seed. MTCP’s preferred analysis is a deterministic paired comparison, for this characterization only. Passing the gate does not set C6 seed policy, EGAR seed policy, or production seed policy.

The gate runs before Stage A, on a disposable workspace that is destroyed afterward. It uses the sealed diagnostic prompt in v1.0 Appendix A. Both generations use ceiling 512, context 8192, seed `17001`, the 4356-second text timeout, and the same model, image, samplers, backend, and chat mode as Stage A. The diagnostic prompt is not one of C1–R1.

The compared string is the raw provider completion text. Sanitized chat text, logs, and timestamps are not the comparison string.

**PASS** only if the two raw completion texts are byte-for-byte identical.

Also record finish reason and completion-token count for both generations. Those counts follow §14. A missing usage object is unavailable, not zero. Seed-gate timing is diagnostic. It is not §17 scored evidence.

- If the texts match and finish reason or completion tokens differ, the gate is **inconclusive**. Deterministic mode must not start, and fallback mode must not start. The mismatch is a telemetry defect. Repair the instrumentation and repeat the gate. Do not weaken the text comparison.
- If the texts do not match, deterministic paired mode **fails**. Switch to §13. Do not relax the gate after seeing the mismatch.
- The seed is `17001`. The random sentinel is not a characterization seed.

## 13. Repeated-trial fallback

If the gate fails on text mismatch, a single run per ceiling is not paired evidence.

Fallback workload:

- tasks C2, C3, G1, and G2
- three trials of each task at each ceiling that the stage rule requires
- each trial from S0-CHAR
- seed omitted, so sampling stays nondeterministic as in production
- C1 and R1 are not fallback decision tasks

Consequences that are accepted in advance: revision is not part of fallback evidence. Three trials support a majority per slot. They do not support a precise rate.

The fallback statistic is the majority rule in §17. Three is the repetition count for v1.1. Compare fallback conditions as aggregated majorities. Do not quote one repetition’s wording as the outcome.

Fallback order, blocked by task, at each ceiling that the stage requires: C2, C2, C2, then C3, C3, C3, then G1, G1, G1, then G2, G2, G2. Ceiling order remains 512, then 1024. If Stage B is triggered, three blocked repetitions of each eligible fallback task at 2048, in identity order C2, C3, G1, G2.

## 14. Telemetry requirements

### 14.1 Provider usage

A completed provider response that reports a usage integer records that integer. Zero in that object is **measured zero**.

A failed transfer with no parsed usage records `prompt_tokens`, `completion_tokens`, and `total_tokens` as **unavailable**. Unavailable is not zero. The record carries an explicit marker that usage was not supplied. It does not store a numeric zero for those fields.

In-flight progress counts, including `next_token.n_decoded` and prompt-token processing counters, are not copied into `prompt_tokens`, `completion_tokens`, or `total_tokens`.

### 14.2 Required fields

A scored call is invalid without these fields:

- `call_type`
- `requested_max_tokens`
- provider usage for `prompt_tokens` and `completion_tokens`, either a reported integer or an explicit unavailable marker
- `total_tokens` when the provider supplies it; unavailable when the provider does not
- `finish_reason` when the provider supplies it; empty or unavailable when the transfer fails before a finish reason exists
- `elapsed_ms`
- `attempt`
- `provider_ok`
- `task_id`

When the caller has them:

- `plan_id`
- `session_id`
- `parse_ok`
- `validation_ok`
- `fallback_used`

Chat may use the existing authoritative chat generation record when that record contains these facts and can be joined to `task_id`. Non-chat generations need a structured metadata record that carries the same facts.

The characterization must not log prompt text, completion text, or chain-of-thought in order to measure tokens. v1.0 Appendix A already holds the prompts. Provider text from the v1.1 seed gate may be retained only for the gate comparison, in the gate record, and is not a license to log Stage A or Stage B text.

### 14.3 In-flight progress

While a text generation is in flight, a side observer may `GET /slots` on the llama.cpp server that is already serving the request.

The observer does not modify `POST /v1/completions`. `stream` stays false. Seed, samplers, prompt, ceiling, and provider behavior stay as the request was built.

Progress samples are taken by `GET /slots` at a fixed interval of 60 seconds, measured from the end of one attempt to the start of the next. At most one such request is in flight. The request does not include `fail_on_no_slot`. `LLAMA_SERVER_SLOTS_DEBUG` is unset. If an attempt times out, returns an HTTP error, cannot be parsed, or contains a prompt or generated-text field, discard the body and record a progress gap with the sample time and the reason. The next attempt waits 60 seconds. A progress gap does not change `provider_ok`, finish reason, provider usage, ceiling pressure, or condition validity, and it does not cancel or retry the generation.

Each retained progress sample may contain only:

- sample timestamp
- processing state (`is_processing`)
- `n_prompt_tokens`, `n_prompt_tokens_processed`, and `n_prompt_tokens_cache`, when the server exposes them
- `next_token.n_decoded`
- `next_token.n_remain`

Progress samples are diagnostic evidence. They do not replace provider usage. They do not enter ceiling pressure, adequacy, material improvement, fallback counts, or the overflow inequality.

## 15. Measurements

### Ceiling pressure

A generation is a **ceiling-pressure event** when `provider_ok` is true, completion-token usage is a reported integer, and either:

- `finish_reason` is `length`, or
- `completion_tokens / requested_max_tokens >= 0.95`

Unavailable completion usage cannot form this ratio. A progress `n_decoded` value cannot form this ratio.

Report count, fraction, call type, and task identity. The fraction’s denominator is the number of valid scored calls in that report, and the numerator is always shown beside it.

### Planner and revision

For each goal or revision trial, record: first generation valid or not, whether a validation retry ran, the retry’s validity, and whether the final plan is a programmatic fallback.

### Synthesis

For each synthesis call, record success or failure, finish reason, provider completion tokens or an unavailable marker, `elapsed_ms`, and the existing indicator that retrieved context was truncated before the call.

A synthesis generation is empty, for §16, only when the provider reports a measured completion of zero or the returned text is empty after a completed response. Unavailable usage is not an empty generation.

### Operational cost

Record provider-reported completion tokens where they exist, and every call’s `elapsed_ms`. Report the median when a series has at least 3 values. Report the 95th percentile only when that series has at least 8 values. Smaller series publish the raw list and no percentile. Unavailable usage is omitted from token sums.

### Task outcome

Use only outcomes the system already records: chat validity and invalidity reason, goal outcome `completed`, `failed`, or `aborted`, and step success or failure. No subjective quality score may be added after the runs.

## 16. Structured failure

A **structured failure** is any of:

- a plan or revision generation that fails validation
- a final plan produced by programmatic fallback
- a synthesis generation that is empty under §15, or whose step fails because the generation is empty or unusable

A retrieval miss, a tool error, or a confirmation denial is not a structured failure. A provider timeout is a validity failure under §19, not by itself a structured failure.

A structured failure is **length-associated** only when the generation that failed is itself a ceiling-pressure event. For fallback, that generation is the validation-failed attempt immediately before the fallback. An earlier length hit does not make the fallback length-associated if the attempt that directly preceded fallback was not a ceiling-pressure event.

Structured failures that are not length-associated are reported. They do not by themselves trigger Stage B. They do count in the “must not get worse” clause of the decision rule.

## 17. Decision rule

Apply this rule only to valid conditions. Do not change thresholds after seeing results. The selected ceiling is the smallest tested value that the rule names. A larger ceiling is never selected because its answers look preferable.

### Slots

A **call slot** is `(task_id, call_type, ordinal)`. Ordinal numbers distinct calls of the same type inside one trial, starting at 1. `plan_retry` and `revision_retry` are their own call types.

In deterministic mode the pressure count of a condition is the number of union slots, across the two ceilings, that are ceiling-pressure events in that condition. A retry that exists only at the lower ceiling counts only for the lower ceiling. A slot that is absent is not a pressure event.

### Material improvement

In deterministic mode, higher ceiling H is a **material improvement** over lower ceiling L when either clause is true:

1. H has at least two fewer ceiling-pressure slots than L, the number of length-associated structured failures does not increase, and the number of programmatic fallbacks does not increase.
2. H has at least one fewer programmatic fallback than L, at least one removed fallback is length-associated at L, and the number of ceiling-pressure slots does not increase.

Clause 1 and clause 2 are alternatives. A worsening of the other endpoint blocks the clause that would otherwise pass. New pressure on a slot that was clean at L counts in H’s pressure total, so it works against both clauses.

### Adequacy

A ceiling is **adequate** when its valid decision set has:

- zero ceiling-pressure events, and
- zero length-associated structured failures

Residual pressure is always reported, including when a non-adequate ceiling is the one the rule selects.

### Stage A outcomes

Compare 1024 with 512.

- If 1024 is not a material improvement, select **512**. Do not run Stage B. If 512 is not adequate, the recorded result is `512 selected; adequacy not met`.
- If 1024 is a material improvement and 1024 is adequate, select **1024**. Do not run Stage B.
- If 1024 is a material improvement and 1024 is not adequate, Stage B is **triggered**.

### Stage B outcomes

Compare 2048 with 1024 on the Stage B task set only.

Let `N` be the number of ceiling-pressure slots at 1024 inside that set. 2048 is a material improvement over 1024 when either:

1. the pressure-slot count falls by at least `min(2, N)`, with `N >= 1`, and length-associated structured failures do not increase, and programmatic fallbacks do not increase, or
2. clause 2 of the paired rule holds on this set.

- If 2048 is a material improvement and is adequate, select **2048**.
- If 2048 is a material improvement and is not adequate, select **2048** and record `2048 selected; adequacy not met`. v1.1 does not test a ceiling above 2048.
- If 2048 is not a material improvement, select **1024** and record whether 1024 met adequacy.

No tested ceiling above 512 is selected unless it is a material improvement over the next smaller tested ceiling. Residual pressure at the selected ceiling stays in the report.

### Fallback-mode statistic

A slot is **majority-pressured** when at least 2 of its 3 trials are ceiling-pressure events. A fallback is a **majority fallback** when at least 2 of 3 trials end in programmatic fallback and at least one of those fallbacks is length-associated.

Material improvement uses the same two clauses, with majority-pressured slots in place of pressure slots and majority fallbacks in place of fallbacks. Adequacy uses the majority: a fallback condition is adequate when no decision slot is majority-pressured and no length-associated structured failure occurs in 2 or more trials of a task. Stage B uses the same substitution and the same `min(2, N)` on majority-pressured slots.

## 18. Context-overflow invalidation

Every scored call whose provider reported `prompt_tokens` must satisfy:

`prompt_tokens + requested_max_tokens <= n_ctx`

The integer in that inequality is the provider-reported prompt-token count. Measured zero may be used. Unavailable usage is not entered as zero, and a progress counter is not entered as `prompt_tokens`.

If a call with reported `prompt_tokens` violates that inequality, do not use it as evidence about the ceiling. Record the call, the token counts, `n_ctx`, the task identity, and the ceiling.

One violating call invalidates that task. Any invalid decision task invalidates the **whole condition**. Do not drop the task and score the rest. A partial set would no longer be the sealed workload. The condition may be rerun once the cause is fixed, under the same protocol, only if the sealed prompts and context still satisfy the inequality. If a sealed prompt cannot satisfy it at 8192 for a tested ceiling, stop. That is a protocol defect and requires a new version.

A call with unavailable prompt usage does not pass this inequality by omission. It is invalid under §19.

## 19. Condition validity

| Class | Meaning | Effect |
| --- | --- | --- |
| Invalid call | Missing required telemetry, wrong `requested_max_tokens` for the condition, `provider_ok` false, unavailable provider usage, or context overflow on reported `prompt_tokens` | The task is invalid |
| Invalid task | Any invalid scored call, S0-CHAR check failed, or prompt hash differs from v1.0 Appendix A | The condition is invalid |
| Invalid condition | Wrong SHA, image, model hash, build, context, retrieval hash, corpus hash, sampler record, seed mode, text timeout other than 4356 seconds, or reflection maximum; missing telemetry sink; decision task invalid | Do not enter the decision. Rerun only after the cause is fixed, without changing the rule |
| Protocol violation | C6 or EGAR state used, window opened, forbidden corpus used, Stage B run without the predicate, threshold changed after data, prompts or generated text logged to study quality, v1.0 scored calls pooled into v1.1 | Stop. Those generations are not MTCP v1.1 evidence |

A poor scientific result is not invalid. High truncation at 512 is a valid outcome.

Unavailable usage on a failed transfer remains an invalid call. Recording it as null does not make the call scoreable. In-flight progress that shows decoding during that transfer does not repair `provider_ok` false.

The same condition invalidated twice by a defect that this protocol cannot fix is a stop. Do not edit a sealed v1.1 in place. v1.0 is not edited to absorb that defect.

## 20. Abort rules

Abort a condition, and do not keep generating, when any of these is known:

- `n_ctx` is not 8192
- the text-generation timeout is not 4356 seconds
- chat or embedding SHA-256 differs from the manifest
- llama.cpp image digest or build differs from the manifest
- S0-CHAR verification fails
- the first scored call does not carry the required telemetry, including an explicit usage integer or an explicit unavailable marker
- `requested_max_tokens` differs from the condition ceiling
- deterministic mode sends no seed or sends a different seed
- fallback mode sends a seed
- three provider failures occur in a row
- the workspace or model mount is a C6-LIVE or EGAR-LAB volume

Do not abort because the ceiling is hit often, because plans fall back, or because a goal outcome is `failed`. Those are admissible results. A single provider timeout still invalidates its task and therefore its condition under §19. The three-in-a-row line is the point at which generation stops early. It does not license scoring the calls that already failed.

## 21. Analysis rules

The report must:

- include every valid trial, including null effects
- list invalid calls, invalid conditions, aborts, and protocol violations separately from scored evidence
- keep non-length-associated failures distinct from length-associated failures
- keep unavailable usage distinct from measured zero
- state whether the run was deterministic paired mode or fallback mode
- state the Stage A predicate outcome before presenting any 2048 number
- apply §17 without a revised threshold

No metric is added or removed after the first scored v1.1 trial. If execution shows that a rule in v1.1 cannot be applied, stop and write a new version. Typographical repairs that do not change a threshold, a task identity, a validity class, or a claim may be noted as errata. Anything that changes scientific meaning is a new version.

## 22. Provenance manifest

Each condition has a manifest. The manifest is not the C6.4 cohort fingerprint, and this protocol does not change that fingerprint.

Required fields:

- protocol identifier `MTCP v1.1` and lifecycle state at run time (sealed before a valid run)
- Product SHA and Engine SHA actually used after progress-telemetry implementation and verification, each at or after the §1 minimum baseline, and equal across compared conditions
- Engine image identity
- llama.cpp image digest and build
- chat model path and SHA-256
- embedding model path and SHA-256
- chat `n_ctx` and embedding context
- generation ceiling
- temperature, top_p, effective top_k, effective min_p, and the other effective sampler controls the runtime exposes
- seed, or an explicit statement that fallback mode omitted the seed
- inference backend and chat inference mode
- text timeout `4356` and the formula `ceil(2 × (544447 / 512) × 2048)`, and embedding timeout `300`
- embedding strict/fallback setting
- reflection maximum
- retrieval-configuration SHA-256
- corpus hash
- v1.0 Appendix A asset hash, covering prompts, diagnostic prompt, and the R1 fixture
- S0-CHAR checklist result for each trial, including the archive hashes in §9
- task-set identity
- stage and, for Stage B, the predicate result that triggered it
- confirmation that no v1.0 scored call was entered
- start and end timestamps

## 23. Relationship to a future C6 or EGAR freeze

The MTCP result may inform the shared `max_tokens` value later copied into a frozen inference deployment.

MTCP does not itself freeze C6 context, C6 seed policy, EGAR seed policy, either corpus, C6 S0, or EGAR S0. Context 8192 inside TOKEN-CHAR is not a deployment decision. The characterization seed is not a deployment seed. The 4356-second observation limit is not a deployment timeout.

A later freeze still has to record the ceiling together with model hashes, llama.cpp build, context, samplers, timeouts, and seed policy. Selecting a ceiling here is not that freeze.

## 24. Versioning

| State | Meaning |
| --- | --- |
| DRAFT | The reviewed text before this seal. Superseded for this version by the 2026-09-29 seal. |
| SEALED | This document, committed before progress-observer implementation results and before any v1.1 scored generation. This is the current state. |
| SUPERSEDED | A later version replaced it. The superseded text stays in history. |

Current lifecycle state: **SEALED** on 2026-09-29.

v1.0 stays **SEALED**. Material changes after this seal produce MTCP v1.2 or later. v1.1 is not rewritten to match results.

## 25. Scientific choices carried forward

The following are unchanged from sealed v1.0:

- the research question in §2
- ceilings 512, 1024, and conditional 2048
- decision thresholds in §17, including two fewer pressure slots, one length-associated fallback removal, adequacy at zero pressure and zero length-associated structured failures, and Stage B `min(2, N)`
- deterministic task order and fallback repetition
- seed `17001`
- `n_ctx` 8192
- corpus, prompts, diagnostic prompt, retrieval configuration, and the R1 fixture
- per-task S0-CHAR restore
- the provider-failure rule that `provider_ok` false invalidates the call, the task, and the condition
- isolation from C6-LIVE, EGAR-LAB, and operational DEV

The changes sealed in v1.1 are the 4356-second observation limit, the distinction between measured zero and unavailable usage, diagnostic `/slots` progress samples, the identity-propagation baseline, and the exclusion of v1.0 Stage A from the v1.1 decision.

## 26. Incorporated v1.0 assets

v1.0 Appendix A remains the byte source for fixtures. v1.1 does not copy those bytes into this file. The identities below are the v1.0 seal record.

| Identity | SHA-256 |
| --- | --- |
| `docs/mtcp/mtcp_service_failure.md` | `522f62f3eb652d2023e0764ef92d56209a813502829254e0f60535eb770ac9b0` |
| `docs/mtcp/mtcp_configuration.md` | `4f5fdb9bb63268e8c5df5fcc483992af55d255745a64c4e4294b6ed3f387c722` |
| `docs/mtcp/mtcp_recovery_order.md` | `fc838d2681f114347de8e5de2cccd92d7fb79b30baa837d57343a628000c4000` |
| Corpus-set fingerprint | `155f9f0be3a5210730fd5042bcaf228ea1d4421d21b411ea1890e769d58a8921` |
| `docker/experiment/retrieval_config.json` | `b4eab95e104f84de05ee7c1ac26e1117447104c983ce924942e987db073df396` |
| Task-set | `a41031ff84308edbaf40804f4e96aae8b85395a0ff9e00fffc7afe402002ec90` |
| Appendix A asset | `7d8a3b4bd07663737c50d3a56304dedaca37e2b6db9cddea8c6e0140a353ec39` |

The characterization seed remains `17001`. `ceiling_order` remains `["512","1024"]`. `stage_b_order` remains `2048-after-predicate`. `deterministic_task_order` remains `["C1","C2","C3","G1","G2","R1"]`. `fallback_task_order` remains `["C2","C3","G1","G2"]`. `fallback_repetition` remains `blocked-3`.

R1 still requires the rendered revision-wrapper hash to be identical at every compared ceiling. Inequality invalidates R1. S0-CHAR must leave `plan_revision.tmpl` either absent or byte-identical to the Engine built-in default.
