# Thoth Max-Token Characterization Protocol

| Field | Value |
| --- | --- |
| Document title | Thoth Max-Token Characterization Protocol |
| Identifier | MTCP v1.0 |
| Lifecycle | **SEALED** |
| Draft baseline (verification only) | Product `963e7c421c6ce789c45d8bef0cefb33d962b5711` on `env/isolation-infra`. Engine `86dcdd80d0c606ad1f27754aa4b67cc27d3fd546` on `c6/phase-7`. |
| Path | `docs/MAX_TOKEN_CHARACTERIZATION_PROTOCOL.md` |

MTCP v1.0 is **SEALED**. This seal is prospective. It fixes the scientific specification before instrumentation, TOKEN-CHAR creation, the seed reproducibility gate, and any 512, 1024, or 2048 characterization generation or result. The seal does not itself perform those steps.

## 1. Purpose

MTCP v1.0 defines how Thoth will characterize one deployment setting: the maximum number of tokens a wired text generation is allowed to produce.

The characterization exists to choose the smallest tested ceiling that provides adequate generation headroom, before that choice is considered for a later C6 or EGAR inference freeze. It is a configuration characterization. It is not the official C6.4 longitudinal experiment, not the EGAR learning experiment, not a comparison of Thoth with another system, and not a claim that longer answers are better.

The draft-baseline commits above are the repository state when this protocol was drafted. Execution SHAs will differ once approved instrumentation exists. Every condition that is compared with another condition must use the same Product SHA and the same Engine SHA. A SHA change between Stage A and Stage B makes the comparison invalid.

## 2. Research question

Holding the inference model and its bytes, the llama.cpp runtime, context capacity, sampling configuration, retrieval configuration, task definitions, starting cognitive state, and the compared software versions constant, does increasing the shared maximum generated-token ceiling above 512 materially reduce output truncation or length-associated generation failure, and what is the smallest tested ceiling that is adequate under the rule in this protocol?

The decision favors the smallest adequate ceiling. It does not favor the longest output.

## 3. Scope and non-claims

This protocol may produce evidence about:

- ceiling pressure
- generation truncation
- whether a plan validates on the first generation and after a validation retry
- programmatic plan fallback when that fallback is length-associated
- synthesis completion
- generation latency
- generated-token cost
- objective task completion already recorded by the system

This protocol must not be reported as evidence of:

- general intelligence or general response quality
- learning, memory improvement, or experience reuse
- EGAR effectiveness
- C6 longitudinal improvement
- superiority of one model over another
- effects of context size, temperature, samplers, retrieval weights, or any other parameter that this protocol holds constant

Null results are results. A finding that 512 is adequate is as reportable as a finding that it is not.

## 4. Independent variable

The only intended independent variable is the **shared maximum generated-token ceiling**.

Tested values:

| Condition | Ceiling |
| --- | --- |
| A0 | 512 |
| A1 | 1024 |
| B0, only if triggered | 2048 |

The ceiling is one value applied to every wired text-generation path under comparison: ordinary chat, plan generation, plan retry, plan revision, revision retry, reflection planning if it occurs, synthesis, and, if a defined task happens to call them, self-correction and episodic batch summary.

Context window, prompt length, and the model’s native training context are not this variable. They are controls or observations.

## 5. Controlled variables

The following must be the same across every condition that enters one decision. Where a setting comes from the llama.cpp process rather than from Thoth, it is still controlled and must be recorded from that process.

- Product SHA and Engine SHA, equal across conditions
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
- text-generation timeout and embedding timeout
- embedding strict/fallback policy
- retrieval-configuration hash
- characterization corpus hash
- sealed task prompts, task order, and the revision fixture
- S0-CHAR procedure and logical starting state
- reflection maximum, equal to the default compiled into the Engine SHA under comparison
- workspace isolation from C6-LIVE, EGAR-LAB, and operational DEV state

Sampler values that Thoth does not send are recorded at the start of each condition. At the draft baseline, the running DEV server reported `top_k = 40` and `min_p = 0.05`, with the random seed sentinel `4294967295`. Those observations illustrate why server-side defaults must be copied into the manifest. The characterization itself must read them again. It must not assume the draft-time numbers are still in force.

## 6. Context requirement

Every condition uses **`n_ctx = 8192`**.

This is a methodological setting for MTCP v1.0. It is not a permanent C6 or EGAR context decision.

8192 is required because:

- the operational DEV runtime has already run the candidate model at 8192
- a candidate output ceiling has to leave room for a realistic prompt
- a 2048-token output ceiling cannot be compared fairly inside a 2048-token total context, because the prompt would have to be discarded or the call rejected
- a different context in any condition would confound the ceiling comparison

Before any scored generation in a condition, read `n_ctx` from the running llama.cpp server. If it is not 8192, that condition is invalid and must be aborted. Do not score any of its calls.

## 7. Environment and isolation

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

## 8. S0-CHAR

**S0-CHAR** is the logical starting state of every scored task.

It contains:

- an initialized memory schema
- the sealed retrieval configuration
- the sealed synthetic corpus, indexed
- no characterization goals
- no trajectories, strategies, or episodic memories produced by another task or another ceiling
- no chat messages or plans from another task or another ceiling
- no C6 window file and no C6 cohort fingerprint
- no EGAR S0 and no EGAR corpus

**Trial rule.** Each task identity is one trial. Each trial starts from S0-CHAR. Completing C1 does not leave memory in place for C2. Completing 512 does not leave memory in place for 1024. A later ceiling, and a later task at the same ceiling, must not inherit plans, chat, trajectories, or strategies from an earlier trial.

This per-task reset is required because a larger ceiling changes what would be stored, and a later task would then see a different prompt. That would confound the ceiling with prompt content.

Logical equivalence is verified before the first generation of each trial. Required checks:

- retrieval-configuration hash equals the sealed hash
- corpus hash equals the sealed hash
- the index source is exactly the sealed corpus
- characterization goals, plans, trajectories, strategy records, episodic memories, and chat messages are absent
- context, model hashes, sampler record, ceiling, and seed mode match the condition manifest

Database bytes need not match across trials. Initialization timestamps may differ. Equivalence is the checklist above. If a trial fails the checklist, that trial is invalid.

## 9. Corpus rules

The corpus is a small synthetic set created for MTCP. The following sources are forbidden:

- the future C6 research corpus
- the sealed EGAR Linux troubleshooting corpus
- the operational DEV workspace, its chat logs, and its memory
- TOKEN-CHAR leftovers from an earlier ceiling or an earlier task

Purpose: give retrieval and synthesis a fixed, known target so those paths run. Retrieval difficulty is not the variable under study.

Required properties are instantiated by Appendix A:

- three short original documents
- one distinct retrievable fact class in each document: a service-failure fact, a configuration fact, and a recovery-order fact
- no document restates another document’s fact
- each document is short enough that a normal retrieval injection plus the largest tested output ceiling still satisfies the overflow rule at `n_ctx = 8192`
- the set is frozen, byte-hashed, and identical in every trial

No characterization trial may run before this protocol, including Appendix A, is sealed.

## 10. Workload

Six task identities are sufficient. They cover chat, planning, synthesis, and revision. Additional tasks are out of scope for v1.0.

| Id | Class | Role in the decision |
| --- | --- | --- |
| C1 | Short chat | Low-pressure control. A ceiling hit here is evidence that the run is not behaving like a short generation. |
| C2 | Retrieval-grounded chat | Short answer that must use the synthetic corpus. |
| C3 | Structured chat | Prompt written to ask for a long structured answer, so ceiling pressure has a chance to appear. |
| G1 | Goal | Simple retrieval, planning, and synthesis of one corpus fact. |
| G2 | Goal | Longer synthesis that compares all three documents. |
| R1 | Revision | One controlled call to the production plan-revision path, using a sealed fixture plan and a sealed failing step result. |

R1 is a fixture so revision does not depend on whether a previous goal happened to fail a step. The fixture contains no experience from C1–G2.

Prompt text, the seed-gate prompt, and the R1 fixture are Appendix A. They are frozen before any result exists. Changing them after a scored trial is a new protocol version.

**Call types that count when they occur inside a defined task:** `chat`, `plan`, `plan_retry`, `revision`, `revision_retry`, `synthesis`. Reflection planning, if the production reflection maximum causes it during G1 or G2, is recorded as `plan` for that task and is scored. The reflection maximum itself stays at the Engine default (2 at the draft-baseline Engine SHA) so the policy matches the deployment subject.

Self-correction and episodic batch summary are not separate tasks and are not intentionally invoked. If a defined task emits them, they are scored as slots of that task. They are not required for a valid condition.

Sealed run order, each from a new S0-CHAR: C1, C2, C3, G1, G2, R1. The seed gate is not part of this order. It uses a disposable state that is destroyed before Stage A.

## 11. Stage structure

### Stage A

Compare ceiling 512 with ceiling 1024 on task identities C1, C2, C3, G1, G2, and R1. In deterministic mode, one trial of each task at each ceiling. In fallback mode, the reduced set and the repetition count in §13.

### Stage B

2048 is not part of the default workload. It is started only when the Stage A predicate in §17 is true. Curiosity, inspection of answer text, or a preference for longer answers does not start Stage B.

Stage B repeats only the Stage B task set: every Stage A task identity that, at 1024, still has at least one ceiling-pressure event or at least one length-associated structured failure. Each of those trials starts again from S0-CHAR. Tasks that were clean at 1024 are not re-run. No new task identity may be added.

If the Stage A predicate is false, a 2048 run is a protocol violation and its generations are not evidence.

## 12. Seed reproducibility gate

Production Thoth does not send a seed. MTCP’s preferred analysis is a deterministic paired comparison, for this characterization only. Passing the gate does not set C6 seed policy, EGAR seed policy, or production seed policy.

The gate runs before Stage A, on a disposable workspace that is destroyed afterward. It uses the sealed diagnostic prompt in Appendix A, which must be written to finish well below 512 tokens. Both generations use ceiling 512, context 8192, the characterization seed, and the same model, image, samplers, backend, and chat mode as Stage A. The diagnostic prompt is not one of C1–R1.

The compared string is the raw provider completion text. Sanitized chat text, logs, and timestamps are not the comparison string.

**PASS** only if the two raw completion texts are byte-for-byte identical.

Also record finish reason and completion-token count for both generations.

- If the texts match and finish reason or completion tokens differ, the gate is **inconclusive**. Deterministic mode must not start, and fallback mode must not start. The mismatch is a telemetry defect. Repair the instrumentation and repeat the gate. Do not weaken the text comparison.
- If the texts do not match, deterministic paired mode **fails**. Switch to §13. Do not relax the gate after seeing the mismatch.
- The seed is the integer fixed in Appendix A. The random sentinel is not a characterization seed.

## 13. Repeated-trial fallback

If the gate fails on text mismatch, a single run per ceiling is not paired evidence.

Fallback workload:

- tasks C2, C3, G1, and G2
- three trials of each task at each ceiling that the stage rule requires
- each trial from S0-CHAR
- seed omitted, so sampling stays nondeterministic as in production
- C1 and R1 are not fallback decision tasks

Consequences that are accepted in advance: revision is not part of fallback evidence. Three trials support a majority per slot. They do not support a precise rate.

A pooled “two fewer events” rule would be too weak once three repetitions multiply the number of calls. The fallback statistic is therefore the majority rule in §17, not a pooled copy of the paired-mode count. Three is the smallest odd repetition count that defines a majority, and it is the repetition count locked for v1.0. Five would cost substantially more generator time at about two tokens per second and is not adopted after results exist.

Compare fallback conditions as aggregated majorities. Do not quote one repetition’s wording as the outcome.

## 14. Telemetry requirements

A scored call is invalid without these fields:

- `call_type`
- `requested_max_tokens`
- `prompt_tokens`
- `completion_tokens`
- `total_tokens` when the provider supplies it
- `finish_reason`
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

The characterization must not log extra prompt text, completion text, or chain-of-thought in order to measure tokens. Appendix A already holds the prompts. Provider text from the seed gate may be retained only for the gate comparison, in the gate record, and is not a license to log Stage A or Stage B text.

## 15. Measurements

### Ceiling pressure

A generation is a **ceiling-pressure event** when `provider_ok` is true and either:

- `finish_reason` is `length`, or
- `completion_tokens / requested_max_tokens >= 0.95`

Report count, fraction, call type, and task identity. The fraction’s denominator is the number of valid scored calls in that report, and the numerator is always shown beside it.

### Planner and revision

For each goal or revision trial, record: first generation valid or not, whether a validation retry ran, the retry’s validity, and whether the final plan is a programmatic fallback.

### Synthesis

For each synthesis call, record success or failure, finish reason, completion tokens, `elapsed_ms`, and the existing indicator that retrieved context was truncated before the call.

### Operational cost

Record total completion tokens and every call’s `elapsed_ms`. Report the median when a series has at least 3 values. Report the 95th percentile only when that series has at least 8 values. Smaller series publish the raw list and no percentile.

### Task outcome

Use only outcomes the system already records: chat validity and invalidity reason, goal outcome `completed`, `failed`, or `aborted`, and step success or failure. No subjective quality score may be added after the runs.

## 16. Structured failure

A **structured failure** is any of:

- a plan or revision generation that fails validation
- a final plan produced by programmatic fallback
- a synthesis generation that is empty, or whose step fails because the generation is empty or unusable

A retrieval miss, a tool error, or a confirmation denial is not a structured failure.

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

`min(2, N)` is required so that a single residual pressured slot, which cannot fall by two, still counts as material when 2048 clears it. When `N >= 2`, the Stage A threshold of two still applies.

- If 2048 is a material improvement and is adequate, select **2048**.
- If 2048 is a material improvement and is not adequate, select **2048** and record `2048 selected; adequacy not met`. v1.0 does not test a ceiling above 2048.
- If 2048 is not a material improvement, select **1024** and record whether 1024 met adequacy.

No tested ceiling above 512 is selected unless it is a material improvement over the next smaller tested ceiling. Residual pressure at the selected ceiling stays in the report.

### Why the preliminary formula was changed

The preliminary formula used raw call totals and treated any provider error as a failed improvement. Unequal retries and spontaneous reflection would have changed the total without a matched slot. A single leftover pressured call at Stage B could never have satisfied “two fewer,” so 2048 would have been rejected for clearing the only residual that triggered it. Truncation could have improved while fallbacks got worse, and the OR would still have accepted the higher ceiling. Those cases are handled by slot identity, the no-worsening clause, and `min(2, N)` on Stage B. Provider errors are validity failures under §18, not a third scientific endpoint.

### Fallback-mode statistic

A slot is **majority-pressured** when at least 2 of its 3 trials are ceiling-pressure events. A fallback is a **majority fallback** when at least 2 of 3 trials end in programmatic fallback and at least one of those fallbacks is length-associated.

Material improvement uses the same two clauses, with majority-pressured slots in place of pressure slots and majority fallbacks in place of fallbacks. Adequacy uses the majority: a fallback condition is adequate when no decision slot is majority-pressured and no length-associated structured failure occurs in 2 or more trials of a task. Stage B uses the same substitution and the same `min(2, N)` on majority-pressured slots.

## 18. Context-overflow invalidation

Every scored call must satisfy:

`prompt_tokens + requested_max_tokens <= n_ctx`

If a call violates that inequality, do not use it as evidence about the ceiling. Record the call, the token counts, `n_ctx`, the task identity, and the ceiling.

One violating call invalidates that task. Any invalid decision task invalidates the **whole condition**. Do not drop the task and score the rest. A partial set would no longer be the sealed workload. The condition may be rerun once the cause is fixed, under the same protocol, only if the sealed prompts and context still satisfy the inequality. If a sealed prompt cannot satisfy it at 8192 for a tested ceiling, stop. That is a protocol defect and requires a new version.

## 19. Condition validity

| Class | Meaning | Effect |
| --- | --- | --- |
| Invalid call | Missing required telemetry, wrong `requested_max_tokens` for the condition, `provider_ok` false, or context overflow | The task is invalid |
| Invalid task | Any invalid scored call, S0-CHAR check failed, or prompt hash differs from Appendix A | The condition is invalid |
| Invalid condition | Wrong SHA, image, model hash, build, context, retrieval hash, corpus hash, sampler record, seed mode, or reflection maximum; missing telemetry sink; decision task invalid | Do not enter the decision. Rerun only after the cause is fixed, without changing the rule |
| Protocol violation | C6 or EGAR state used, window opened, forbidden corpus used, Stage B run without the predicate, threshold changed after data, prompts logged to study quality | Stop. Those generations are not MTCP evidence |

A poor scientific result is not invalid. High truncation at 512 is a valid outcome.

The same condition invalidated twice by a defect that the sealed protocol cannot fix is a stop. Do not edit v1.0 in place.

## 20. Abort rules

Abort a condition, and do not keep generating, when any of these is known:

- `n_ctx` is not 8192
- chat or embedding SHA-256 differs from the manifest
- llama.cpp image digest or build differs from the manifest
- S0-CHAR verification fails
- the first scored call does not carry the required telemetry
- `requested_max_tokens` differs from the condition ceiling
- deterministic mode sends no seed or sends a different seed
- fallback mode sends a seed
- three provider failures occur in a row
- the workspace or model mount is a C6-LIVE or EGAR-LAB volume

Do not abort because the ceiling is hit often, because plans fall back, or because a goal outcome is `failed`. Those are admissible results.

## 21. Analysis rules

The report must:

- include every valid trial, including null effects
- list invalid calls, invalid conditions, aborts, and protocol violations separately from scored evidence
- keep non-length-associated failures distinct from length-associated failures
- state whether the run was deterministic paired mode or fallback mode
- state the Stage A predicate outcome before presenting any 2048 number
- apply §17 without a revised threshold

No metric is added or removed after the first scored trial. If execution shows that a rule in v1.0 cannot be applied, stop and write a new version. Typographical repairs that do not change a threshold, a task identity, a validity class, or a claim may be noted as errata. Anything that changes scientific meaning is a new version.

## 22. Provenance manifest

Each condition has a manifest. The manifest is not the C6.4 cohort fingerprint, and this protocol does not change that fingerprint.

Required fields:

- protocol identifier and lifecycle state at run time (sealed before a valid run)
- Product SHA and Engine SHA
- Engine image identity
- llama.cpp image digest and build
- chat model path and SHA-256
- embedding model path and SHA-256
- chat `n_ctx` and embedding context
- generation ceiling
- temperature, top_p, effective top_k, effective min_p, and the other effective sampler controls the runtime exposes
- seed, or an explicit statement that fallback mode omitted the seed
- inference backend and chat inference mode
- text timeout and embedding timeout
- embedding strict/fallback setting
- reflection maximum
- retrieval-configuration SHA-256
- corpus hash
- Appendix A asset hash, covering prompts, diagnostic prompt, and the R1 fixture
- S0-CHAR checklist result for each trial
- task-set identity
- stage and, for Stage B, the predicate result that triggered it
- start and end timestamps

## 23. Relationship to a future C6 or EGAR freeze

The MTCP result may inform the shared `max_tokens` value later copied into a frozen inference deployment.

MTCP does not itself freeze C6 context, C6 seed policy, EGAR seed policy, either corpus, C6 S0, or EGAR S0. Context 8192 inside TOKEN-CHAR is not a deployment decision. The characterization seed is not a deployment seed.

A later freeze still has to record the ceiling together with model hashes, llama.cpp build, context, samplers, timeouts, and seed policy. Selecting a ceiling here is not that freeze.

## 24. Versioning and seal

| State | Meaning |
| --- | --- |
| DRAFT | Open for owner review. Superseded for this version by owner acceptance. |
| ACCEPTED | Owner has accepted the scientific rules, including Appendix A. This state preceded the seal. No characterization data. |
| SEALED | Accepted rules plus this appendix, committed before implementation results and before any scored generation. This is the current state. |
| SUPERSEDED | A later version replaced it. The superseded text stays in history. |

Current lifecycle state: **SEALED**.

The task-set digest and the Appendix A asset digest are recorded in the seal record. They are not inside the hashed JSON. The Git commit that introduces this sealed text is the MTCP v1.0 seal identity. It exists before instrumentation results and before any scored generation.

After seal, implementation has to be checked against the sealed text before TOKEN-CHAR is used. If a sealed rule and an implementation disagree, the run waits. The implementation does not redefine the rule.

Material changes after seal produce MTCP v1.1 or later. v1.0 is not rewritten to match results.

## 25. Scientific choices

No scientific threshold is left for the operator to choose during analysis. Appendix A freezes the corpus, prompts, goals, revision fixture, seed, and orders. Characterization generations do not exist in this document.

# Appendix A: Frozen Characterization Assets

| Field | Value |
| --- | --- |
| Parent | Thoth Max-Token Characterization Protocol, MTCP v1.0 |
| Appendix status | **SEALED** |
| Effect on the body | None. Thresholds, stages, the seed-gate rule, fallback, validity, abort, and analysis stay as accepted. |

File bytes below are UTF-8, LF line endings, no BOM, and exactly one newline after the last line. Repository copies of those bytes are `docs/mtcp/mtcp_service_failure.md`, `docs/mtcp/mtcp_configuration.md`, and `docs/mtcp/mtcp_recovery_order.md`. Identity filenames in the corpus-set fingerprint are the basenames. Prompts and goals have no leading or trailing whitespace. Where a prompt is shown on more than one line, the line break is a single LF.

## A1. Synthetic corpus

Three original fixture notes. Join key `NL-BR-4417` only identifies the same fictional incident. It is not a target fact, and no note repeats another note's target fact.

`mtcp_service_failure.md`  
SHA-256 `522f62f3eb652d2023e0764ef92d56209a813502829254e0f60535eb770ac9b0` (282 bytes)

```
# Northline Badge Relay incident

Incident `NL-BR-4417` is the only incident in this note.

At 09:40 UTC on 2026-03-14, the Northline Badge Relay stopped accepting badges.

The process that failed is `badge-cache`.

The last log line from that process was `queue=amber-7 rejected`.
```

`mtcp_configuration.md`  
SHA-256 `4f5fdb9bb63268e8c5df5fcc483992af55d255745a64c4e4294b6ed3f387c722` (200 bytes)

```
# Northline Badge Relay setting

This note applies to incident `NL-BR-4417`.

The relay reads `badge.queue.limit` from `/etc/northline/relay.toml`.

The in-force value of `badge.queue.limit` is `24`.
```

`mtcp_recovery_order.md`  
SHA-256 `fc838d2681f114347de8e5de2cccd92d7fb79b30baa837d57343a628000c4000` (296 bytes)

```
# Northline Badge Relay recovery order

This note applies to incident `NL-BR-4417`.

Perform the recovery in this order and do not reorder it.

1. Start unit `relay-cache.service`.
2. Run `northline-relay check --profile kiln`.
3. Open gate `east-gate` only after that check prints `kiln-ready`.
```

Corpus-set fingerprint SHA-256: `155f9f0be3a5210730fd5042bcaf228ea1d4421d21b411ea1890e769d58a8921`

The fingerprint is the SHA-256 of the following UTF-8 document, filenames sorted, one LF after each line:

```
mtcp_configuration.md 4f5fdb9bb63268e8c5df5fcc483992af55d255745a64c4e4294b6ed3f387c722
mtcp_recovery_order.md fc838d2681f114347de8e5de2cccd92d7fb79b30baa837d57343a628000c4000
mtcp_service_failure.md 522f62f3eb652d2023e0764ef92d56209a813502829254e0f60535eb770ac9b0
```

Retrieval configuration input, not part of the corpus: `docker/experiment/retrieval_config.json`, SHA-256 `b4eab95e104f84de05ee7c1ac26e1117447104c983ce924942e987db073df396`.

## A2. Ground-truth table

| Document | Target fact | Exact supported answer | Tasks |
| --- | --- | --- | --- |
| `mtcp_service_failure.md` | Failing process and its last log line | Process `badge-cache`. Last log line `queue=amber-7 rejected`. | G1, G2 |
| `mtcp_configuration.md` | In-force queue limit | `badge.queue.limit` is `24`. | C2, G2 |
| `mtcp_recovery_order.md` | Recovery order | Start `relay-cache.service`, then run `northline-relay check --profile kiln`, then open `east-gate` only after that check prints `kiln-ready`. | G2 |

C1, C3, the seed-gate prompt, and R1 have no corpus answer.

## A3. Seed reproducibility prompt

This prompt is not a scored task. It does not use the corpus. Both diagnostic generations use ceiling 512, context 8192, and seed `17001`.

```
Write one paragraph that repeats this inventory in the given order. For each item, give the code, a colon, then the label. Separate items with semicolons. Do not add an item. Do not omit an item. Do not offer a second list.

KITE-14: alpha; LAMP-27: bravo; MOTH-33: charlie; NEST-46: delta; OAK-58: echo; PEAR-62: foxtrot
```

Pass remains byte-for-byte identity of the raw provider completion. Finish reason and completion-token count are recorded and do not replace that test. The comparison string is the raw provider text, not a sanitized chat string.

## A4. Characterization seed

The characterization seed is **17001**.

It is inside 1 through 2147483647. It is not 0, -1, or the llama.cpp random sentinel. It was not chosen from model output.

## A5. C1 exact prompt

```
What is 17 plus 25? Reply with one short sentence that states the sum.
```

## A6. C2 exact prompt

Targets the configuration fact only: `badge.queue.limit` is `24`.

```
Using the characterization notes for incident NL-BR-4417, what is the in-force integer value of badge.queue.limit? Reply with the integer only.
```

## A7. C3 exact prompt

```
Write a handover procedure for fictional drill DRILL-KILN of the Northline Badge Relay.

Use only these names: badge-cache, relay-cache.service, east-gate, and the command northline-relay check --profile kiln.

Produce exactly 12 numbered steps. Each step is two sentences: the action, then the reason. Every step names at least one of those names. The 12 steps together must cover starting the cache, running the check, and opening the gate.

After the steps, write one JSON object and no other JSON. Use exactly these keys, each with a short string value: drill, cache_process, service_unit, check_command, gate, ready_marker, operator_note, rollback_note.
```

## A8. G1 exact goal

Targets the service-failure fact only.

```
Report the failing process and the last log line for Northline Badge Relay incident NL-BR-4417 using the characterization notes.
```

Submitted as the goal text on the normal goal path. It does not name the process or the log line.

## A9. G2 exact goal

Targets all three facts.

```
Using the characterization notes for incident NL-BR-4417, write an incident brief with three sections titled Failure, Configuration, and Recovery. Each section is one paragraph and must state only what those notes support for that section. After the sections, add one JSON object and no other JSON. The object has key failure_process, key queue_limit, and key recovery_steps. The first two values are strings. recovery_steps is an array of three strings in the order given by the notes.
```

## A10. R1 revision fixture

R1 calls production `LLMPlanner::revise_plan(existing_plan, failed_step_result)` with the objects below. It does not wait for a live step to fail inside `ExecutiveController`. That controller writes the current time into `updated_at_ms` before revision, which would make the prompt differ across trials. The accepted protocol requires the production wrapper to stay identical. A direct `revise_plan` call with frozen timestamps is that path.

### Frozen fixture input

**Goal**

```
Report the failing process and the last log line for Northline Badge Relay incident NL-BR-4417 using the characterization notes.
```

**Plan identity**

| Field | Value |
| --- | --- |
| `plan_id` | `mtcp-r1-plan` |
| `created_at_ms` | `1710000000000` |
| `updated_at_ms` | `1710000000000` |
| `current_index` | `0` |
| `status` | `0` (ACTIVE) |

Integer encoding used by production JSON: step type TOOL `0`, RETRIEVAL `1`, LLM `2`, NODE `3`; step status PENDING `0`, RUNNING `1`, SUCCESS `2`, FAILED `3`, SKIPPED `4`.

**Steps**

| `step_id` | Type | Status | What it is |
| --- | --- | --- | --- |
| `mtcp-r1-retrieve` | RETRIEVAL (`1`) | FAILED (`3`) | Query `MTCP-DOC-404`, which is not a corpus document. `depends_on` is empty. `revise_plan_on_failure` is true. |
| `mtcp-r1-synthesize` | LLM (`2`) | PENDING (`0`) | Depends on `mtcp-r1-retrieve`. Empty payload. `revise_plan_on_failure` is false. |

The retrieval step description is `Retrieve notes for query MTCP-DOC-404`. The synthesis step description is `Report the failing process and last log line from retrieved notes`. Retrieval payload is `{"query":"MTCP-DOC-404","top_k":5}`. Retrieval `started_at_ms` and `completed_at_ms` are both `1710000001000`. The synthesis step times are `0`. Both steps have empty `tool`, empty `reasoning`, `retry_count` `0`, `max_retries` `1`, `abort_on_failure` false, and outcome `run_block_reason` `NONE`. The synthesis `result` is null. The retrieval `result` is the failure object below.

**Failing-step result** (also the `failed_step_result` argument)

```
{"data":{"query":"MTCP-DOC-404","retrieved_chunk_count":0},"error_message":"Retrieval returned no documents for query MTCP-DOC-404.","status":"error"}
```

The stimulus is a missing fixture id. It contains no finish reason and no generation text. `MTCP-DOC-404` does not appear in the corpus, so the failure is not a truncated generation.

Load this plan with `Plan::from_json` and pass that value to `revise_plan` without modifying timestamps.

### Production-generated prompt wrapper

`revise_plan` builds the model prompt from the Engine revision template, the goal, `existing_plan.to_json().dump()`, and `failed_step_result.dump()`. Strategy text and past-experience text are empty on this path. Appendix A does not freeze that rendered string. The later implementation must show that the rendered wrapper hash is the same at every ceiling. S0-CHAR must leave `plan_revision.tmpl` either absent or byte-identical to the Engine built-in default. A workspace template that differs from that default invalidates R1.

## A11. Task and repetition order

The seed gate runs first, on a disposable workspace that is destroyed before Stage A.

**Deterministic mode.** At each ceiling, one trial of each task, in this order, each from a new S0-CHAR:

1. C1
2. C2
3. C3
4. G1
5. G2
6. R1

**Fallback mode**, if the seed gate fails on text mismatch. Decision tasks only, three trials each, blocked by task:

1. C2, C2, C2
2. C3, C3, C3
3. G1, G1, G1
4. G2, G2, G2

C1 and R1 are not run for the fallback decision. The report states that revision evidence is absent in fallback mode.

Blocked repetitions are frozen because every trial already returns to S0-CHAR, so round interleaving adds no pairing, and adjacent repetitions of one task make a missed reset visible in the log. This order is not available to change after results.

## A12. Ceiling-condition order

**Deterministic Stage A:** the full task list at ceiling **512**, then the full task list at ceiling **1024**.

**Stage B,** only after the accepted Stage A predicate is true: ceiling **2048**, one fresh S0-CHAR trial of each eligible task, keeping the A11 identity order and skipping tasks that are not eligible.

**Fallback:** the same ceiling order. All three repetitions of the fallback list at 512, then all three at 1024. If Stage B is triggered, three blocked repetitions of each eligible fallback task at 2048, in identity order C2, C3, G1, G2.

512-then-1024 is the fixed order because the reset already removes cognitive carryover, and a single documented order is enough.

## A13. Mechanical pre-run validation

Before any scored generation of a trial, all of the following must hold. A failure makes that trial invalid under the accepted validity rules.

- The three corpus files exist and each SHA-256 matches A1.
- The corpus-set fingerprint matches A1.
- Retrieval-configuration SHA-256 matches A1.
- The task strings and the R1 JSON match this appendix.
- Product SHA and Engine SHA match the condition manifest, and match each other across conditions.
- Chat-model SHA-256, embedding-model SHA-256, and llama.cpp image digest and build match the manifest.
- `n_ctx` is 8192.
- Effective sampler values match the manifest.
- The requested generation ceiling is the condition ceiling.
- Deterministic mode sends seed `17001`. Fallback mode sends no seed.
- The S0-CHAR checklist in the protocol passes, including an absent or default `plan_revision.tmpl`.
- The seed-gate workspace has been destroyed before the first Stage A trial.

When a provider returns `prompt_tokens`, the accepted overflow rule is applied. No expected prompt-token or completion-token count is set in advance.

For R1, after the calls exist, the rendered revision-wrapper hashes at the compared ceilings must be equal. Inequality invalidates R1 rather than becoming a ceiling result.

## A14. Hashing and identity rules

SHA-256 over raw bytes. Timestamps and filenames alone are not identities.

| Identity | Canonical bytes |
| --- | --- |
| Corpus file | The file bytes in A1. |
| Corpus set | The sorted `name digest` document in A1, each line ended by one LF. |
| Task set | UTF-8 JSON, object keys in this order: `c1`, `c2`, `c3`, `g1`, `g2`, `r1_goal`, `r1_failed_step_result`, `r1_plan`, `deterministic_task_order`, `fallback_task_order`, `fallback_repetition`. String values are the exact prompts, goal, and orders. `r1_plan` is the frozen plan object. No insignificant whitespace. This hash excludes the seed, the diagnostic prompt, and the ground-truth table. |
| Appendix asset hash | UTF-8 JSON, keys in this order: `corpus_files` (array of `{name, sha256}` sorted by name), `corpus_set_sha256`, `retrieval_config_sha256`, `diagnostic_prompt`, `seed`, `task_set_sha256`, `ceiling_order`, `stage_b_order`. The digest is not placed inside the hashed JSON. |

`deterministic_task_order` is `["C1","C2","C3","G1","G2","R1"]`. `fallback_task_order` is `["C2","C3","G1","G2"]`. `fallback_repetition` is `"blocked-3"`. `ceiling_order` is `["512","1024"]` and `stage_b_order` is `"2048-after-predicate"`.

Nested JSON encoding used for those two digests, without changing any accepted value:

- no space after `:` or `,`
- `r1_failed_step_result` and the retrieval payload use the key order written in A10
- `r1_plan` and each step use production `Plan::to_json` and `PlanStep::to_json` insertion order
- the synthesis step `result` is JSON null and its `revise_plan_on_failure` is false
- `seed` is the JSON number `17001`
- multi-line prompts use LF where this appendix shows a paragraph break, and they have no leading or trailing whitespace

## A15. Ground-truth boundaries

The A2 answers may be used to check that the fixtures say what this appendix claims, to name which note a task is aimed at, and to describe a run after the decision rule has already been applied.

Answer correctness is not a ceiling-selection endpoint. The accepted rule uses ceiling pressure, length-associated structured failure, fallback, and existing objective outcomes only. This appendix does not add correctness to that rule.

## A16. Prompt-pressure review

| Asset | Path | Pressure | Why it is not a forced ceiling |
| --- | --- | --- | --- |
| Diagnostic | Chat, no retrieval | Low. A short copy of six codes. | It exists to test seed identity, and it is expected to finish well under 512. |
| C1 | Chat | Low. | One arithmetic sentence. A control that generation returns at all. |
| C2 | Retrieval-grounded chat | Low. | One integer from one note. The integer is not in the prompt. |
| C3 | Chat | Higher structured load. | A 12-step handover with a fixed JSON object is a normal procedure request. It does not mention token limits, and it does not say to continue until cut off. Two sentences per step can land near 512 or finish under it. Either outcome is valid. |
| G1 | Plan, retrieval, synthesis | Low for the final answer. Planning still runs. | One process name and one log line. No length instruction. |
| G2 | Plan, retrieval, synthesis across three notes | Moderate. The informative cognitive case. | Three short sections plus one JSON object. No request to be lengthy, and no token count. All three notes are required because each holds a different fact. |
| R1 | `revise_plan` only | Revision validation, not answer length. | The failure is a missing document id, not a truncated generation. |

C3 is the chat probe most likely to meet the 512 ceiling. That comes from a fixed 12-step structure, which is the workload the accepted protocol asked for. It does not instruct the model to lose. G2 asks for the three facts once each. A long essay is not requested.

## A17. Completeness assessment

Another researcher who has this protocol and the same model and runtime can reproduce the experimental inputs without asking what the corpus, prompts, goals, revision fixture, seed, task order, or ceiling order were.

The rendered revision prompt is intentionally not copied here, because production code builds it. Its inputs are frozen, and cross-ceiling wrapper identity is a required check. The task-set digest and the appendix-asset digest are recorded in the seal-readiness section and are not fields of the hashed JSON.

No experimental input is left to choose after results exist.

# Seal-readiness record

This section records digests. It is not part of the hashed JSON. It does not change a threshold, a task, a prompt, or a validity rule. Current lifecycle state is **SEALED**. No characterization generation is recorded here.

## Prospective seal record

MTCP v1.0 was sealed before all of the following:

- instrumentation implementation
- TOKEN-CHAR creation
- seed reproducibility testing
- any 512, 1024, or 2048 characterization generation
- any characterization result

The scientific rules, prompts, fixtures, thresholds, orders, corpus bytes, and the digests below are the sealed specification. A later scientific change requires a new protocol version. The Git commit that contains this document and the three corpus fixtures is the seal identity.

| Identity | SHA-256 |
| --- | --- |
| `docs/mtcp/mtcp_service_failure.md` | `522f62f3eb652d2023e0764ef92d56209a813502829254e0f60535eb770ac9b0` |
| `docs/mtcp/mtcp_configuration.md` | `4f5fdb9bb63268e8c5df5fcc483992af55d255745a64c4e4294b6ed3f387c722` |
| `docs/mtcp/mtcp_recovery_order.md` | `fc838d2681f114347de8e5de2cccd92d7fb79b30baa837d57343a628000c4000` |
| Corpus-set fingerprint | `155f9f0be3a5210730fd5042bcaf228ea1d4421d21b411ea1890e769d58a8921` |
| `docker/experiment/retrieval_config.json` | `b4eab95e104f84de05ee7c1ac26e1117447104c983ce924942e987db073df396` |
| Task-set | `a41031ff84308edbaf40804f4e96aae8b85395a0ff9e00fffc7afe402002ec90` |
| Appendix A asset | `7d8a3b4bd07663737c50d3a56304dedaca37e2b6db9cddea8c6e0140a353ec39` |

The Appendix A asset digest covers the canonical JSON whose fields are `corpus_files`, `corpus_set_sha256`, `retrieval_config_sha256`, `diagnostic_prompt`, `seed`, `task_set_sha256`, `ceiling_order`, and `stage_b_order`. That JSON contains the task-set digest and does not contain the Appendix A asset digest.
