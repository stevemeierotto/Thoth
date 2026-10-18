# EGAR Linux Diagnostic A/B Experimental Protocol

**Protocol version:** EGAR-LX v0.1  
**Status:** EGAR-LX v0.1 — DRAFT, not frozen, not authorized for experimental execution. Sections 1–12 are approved for protocol design through 2026-10-15. No baseline is frozen. No sample size or acquisition count is selected. No corpus has been collected. No engineering pilot, acquisition, or evaluation is authorized. No C6.4 window is opened. Approval of the twelve design sections is not permission to implement or run the experiment.  
**Date:** 2026-10-14. Design amendments through 2026-10-15.  
**Does not modify:** `C6_phase4_protocol.md`, `C6_phase3_protocol.md`, `E2_PROTOCOL.md`, `E_PHASE_PROTOCOL.md`, `B1_PROTOCOL.md`, `EGAR.md`, `MAX_TOKEN_CHARACTERIZATION_PROTOCOL.md`, engine code, Docker compose, or any experience store.

This draft proposes a future controlled comparison. It does not claim that a Linux diagnostic corpus, an EGAR A/B harness, or a sealed EGAR Linux troubleshooting corpus already exists. The phrase “sealed EGAR Linux troubleshooting corpus” in MTCP v1.0 names a forbidden input for that characterization. It does not define a dataset.

If this draft conflicts with a locked protocol, the locked protocol governs its own evidence. This draft does not revise those protocols.

Claims in §11 are marked **VERIFIED** for current source behavior, **HISTORICAL** for a retained 2026-10-14 observation, **APPROVED DESIGN** for a protocol decision, **UNIMPLEMENTED** for a prerequisite that is not present, or **UNRESOLVED** when the fact or the value is not established.

---

## Document structure

| Class | Sections | Binding? |
|-------|----------|----------|
| **Design rules** | §§1–12 | Yes, for protocol design. Not a baseline freeze and not an execution authorization. §10’s open rows stay open |
| **Evidence labels** | §11 | Approved as an evidence register for protocol design, 2026-10-15. Not a baseline freeze and not an execution authorization |
| **Future research** | Future Research and Follow-Up Experiments, after §12 | No. Not part of EGAR-LX v0.1. Not approved for execution |

---

## 1. Purpose and research questions

### Purpose

**Approved for protocol design, 2026-10-15. Not authorized for execution.** Measure whether experience that Thoth itself produced on earlier Linux diagnostic problems changes performance on later, related problems that Thoth has not seen.

This experiment is separate from C6.4. C6.4 observes ordinary operational goals over two prospective 28-day windows. It is not this comparison, and this comparison is not an official C6.4 window.

### Primary question

Does access to experience accumulated from previous Thoth problem-solving attempts improve Thoth’s performance on new, related Linux diagnostic problems?

### Secondary questions

**Approved for protocol design, 2026-10-15. Not authorized for execution.** Report these separately. Do not fold them into the primary outcome after results are seen.

| Id | Question |
|----|----------|
| S1 | Is the stated root cause more often correct? |
| S2 | Are proposed remedies feasible given the evidence available at the cutoff, without requiring the historical patch text? |
| S3 | Does retrieved documentation support the claims the answer makes? |
| S4 | Does the answer say when the cutoff evidence is insufficient? |
| S5 | After a failed step or a rejected hypothesis, does a later attempt in the same goal recover? |
| S6 | Do latency, token counts, and failed-step counts differ? |
| S7 | Was a stored trajectory, promoted strategy, or past plan actually injected on the EGAR arm? |

S7 is a process check. It is not task success.

---

## 2. Hypothesis and experimental arms

**Section status:** Approved as revised, 2026-10-14. This approval does not freeze the protocol, preregister the hypothesis, or authorize a pilot.

### Hypothesis

**PROPOSED, not preregistered.** On held-out cases, Arm B’s primary score is higher than Arm A’s because Arm B starts from Thoth’s own earlier experience. The direction is not assumed. A null or negative result is a valid result.

### Decision D2

**APPROVED — Option 1.** The experiment tests EGAR as currently implemented. It does not enhance injection, retrieval thresholds, strategy promotion, or the planner, and it does not add an EGAR enable or disable switch. The current payload and its limits stay in force.

**D2 interpretation approved 2026-10-15.** Option 1 includes the corrected P0–R2 graph pathway. This is not a new option. A mechanically successful plan can reward graph edges when that attempt has at least two valid GRAG retrieval sets. A mechanically failed plan can penalize eligible edges. One retrieval set does not create an edge. Edges that are written are part of the frozen acquisition snapshot used by Arm B. Arm A starts without acquired graph edges. Acquisition cases are not required to force multiple retrievals. A one-retrieval case is graph-inert, and that absence is reported. This interpretation does not change EGAR injection, similarity thresholds, strategy promotion, or mechanical scoring.

### Arms

Each evaluation case uses its own disposable database. Nothing written during that case is copied into the next case, into the acquisition snapshot, or into a development, C6, or sealed database.

| Arm | Database at the start of the case | What stays shared |
|-----|------------------------------------|-------------------|
| **A — Baseline** | A new database with no trajectories, promoted strategies, past plans, or graph edges | Same build, model, embedding, sampler record, context, timeout, tools, GRAG code, retrieval-weight file, and pre-cutoff corpus |
| **B — EGAR** | A new copy of one hashed acquisition snapshot | Same as Arm A |

The snapshot is taken after acquisition finishes. It keeps the graph edges and scores the product wrote. Evaluation does not open that snapshot for writing. Arm B’s case database is a copy of it. Deleting or wiping a development, C6, or sealed database is forbidden.

**VERIFIED.** There is no EGAR off switch. Strategy, trajectory, and plan-reuse text are added only when stored rows qualify (`llm_planner.cpp`, `memory.cpp`, `executive_controller.cpp`). An empty database is the baseline. `Memory::clear()` only clears messages for the active session and is not the baseline procedure.

**UNRESOLVED.** Before any pilot, inspect one newly initialized database and record row counts for trajectories, strategies, past plans, graph edges, and facts. This approval does not record that inspection. If initialization inserts rows that the planner or GRAG can read, the Arm A definition must be amended.

### What the current experience path can change

**VERIFIED.** A non-empty store can change the first planner prompt through:

| Mechanism | What is injected | Gate |
|-----------|------------------|------|
| Promoted strategy | Description, step-pattern JSON, and historical success rate. Capped at 512 characters | At least 3 trajectories; pattern count ≥ 3 and mean success ≥ 0.8; cosine ≥ 0.40; at most one strategy |
| Similar trajectory | Prior goal text and `success_score` only. The step list, diagnosis, and tool results stay in `trajectory_json` and are not copied into this snippet | Cosine floor 0.55 after a +0.1 boost when `success_score` ≥ 0.8; at most one trajectory. The combined past-experience block is capped at 512 characters |
| Past plan | Step-type summary (`TYPE(step_id)`), not the raw plan JSON | Success ≥ 0.6; cosine floor 0.55 after the same +0.1 boost at 0.8; at most one plan |

Strategy text is a step-type or tool-name pattern, not a written root cause. If the core prompt uses the entire budget, the experience block is removed and `experience_dropped` is true.

### What must stay identical, and what may still diverge

**APPROVED.**

- Product git SHA and engine git SHA.
- Inference backend, chat model bytes, embedding model bytes, context length, and the recorded sampler settings.
- `THOTH_LLM_TIMEOUT_SECONDS` if set; otherwise the code default of 900 seconds.
- Retrieval-weight file, including trajectory weight.
- The pre-cutoff document corpus and its fingerprint.
- Tool registry and confirmation policy.
- Reflection ceiling (code default: 2 cycles when the trajectory score is below 0.6).
- The evaluation prompt, cutoff, and scoring rubric for that case.

GRAG uses the same code, corpus, and retrieval configuration on both arms. Rankings may still differ. Acquisition success can leave graph edges, and GRAG adds those edge weights to chunk scores. The current episode’s trajectory embedding is built from the steps of the goal now running. With fewer than 3 steps, trajectory weight is forced to 0. After the plans diverge, that embedding can diverge too.

Cognate’s planner and reflection code are the same on both arms. The first planner call is where stored experience can enter. `revise_plan` builds a failure-only prompt and does not receive the EGAR block.

### Other memory state

**VERIFIED.**

| State | Current behavior |
|-------|------------------|
| Warm, hot, and cold trajectory tiers | Aging rewrites the tier label. Trajectory retrieval does not read it. A warm row is not preferred, and a cold row is not excluded |
| Facts | `store_fact` can write the facts table. No production caller reads those rows into the planner or GRAG |
| Chat summaries | Session summaries are available on the chat path. `create_plan` does not load them |

### Injection record

**APPROVED.** For every evaluation case, record whether these existing events appeared: `STRATEGY_INJECTION`, `TRAJECTORY_INJECTION`, `PLAN_REUSE_INJECTION`, and `experience_dropped`. Arm A must show none of the three injection events. Arm B may show none. Missing injection is a valid observation. It is not a failed run and not a reason to change a threshold. `experience_dropped: true` means an experience block was selected and then omitted from the prompt. Count that separately from a case that selected nothing.

### Interpretation

**APPROVED.** The comparison measures the combined effect of the existing experience mechanisms, including planner injection, retained graph edges, and scores stored by the product. It does not isolate planner-prompt injection by itself. Reflection replans do not receive the EGAR block.

### Still open

How an external diagnosis grade is handled is decided: it is not written into `success_score`. Mechanical execution, graph reinforcement, experience retrieval, and external diagnostic correctness stay separate. The fresh-schema row count above is still required before a pilot.

---

## 3. Corpus design

**Section status:** Approved for the decisions in this section, 2026-10-14. No case is selected. Nothing is downloaded. This approval does not authorize corpus collection.

### Role

**APPROVED.** The corpus is the controlled input for this A/B experiment. It is not the C6.4 corpus. C6.4’s fingerprint must not be pointed at these files, and these runs must not receive a C6.4 `window_id`.

### Scope

**APPROVED.** Cases may be Linux kernel problems or Linux userspace problems. Userspace includes services, applications, networking tools, and configuration failures.

Each case is a historical problem with a documented resolution. The record must contain enough evidence to score a diagnosis and a proposed remedy independently. Problems that are still unresolved are out of this initial experiment.

### Admission and rejection

**APPROVED.** A case is admissible only when all of the following hold:

- A documented historical cutoff separates information available before resolution from findings that came later.
- Every visible document is dated at or before that cutoff. Every sealed document is after it.
- The source is traceable: the record names where each document came from and when it was retrieved.
- The historical outcome is reliable enough to score. The basis for that reliability is part of the case record.
- The pre-resolution evidence can be separated from the answer.

Reject a case when the pre-resolution evidence cannot be separated from the answer, or when the outcome is too ambiguous to score fairly.

### Case record

**APPROVED.** The two bundles must not be mixed.

**Visible bundle** (may be indexed or placed in the goal prompt):

- Original report and symptoms, truncated at the cutoff.
- Software name, version, and environment stated in that report.
- Logs, configuration, and discussion that existed at the cutoff.
- Manuals or other documents Thoth is allowed to retrieve, also dated at or before the cutoff.

**Sealed bundle** (answer key; not indexed; not in the prompt; not in the experience store):

- Later diagnosis and resolution.
- Patch identifiers, test results, reversions, and follow-up fixes.
- Reviewer notes that cite the outcome.
- The rubric evidence an adjudicator may use.

Historical resolutions, later patches, and evaluation answer keys must not be exposed to Thoth during diagnosis.

### Public sources to consider later

**PROPOSED options, not a selection.** Naming a source here does not admit any case from it.

| Source | What it can supply | Leakage risk |
|--------|--------------------|--------------|
| [Kernel Bugzilla](https://bugzilla.kernel.org/) | Reports, comments, closures | Later comments often contain the fix |
| [lore.kernel.org](https://lore.kernel.org/) | Patch discussions and `Fixes:` / `Reverts:` mail | The patch series is the outcome |
| Stable-tree commit messages | Symptom text plus a later commit | The commit is after the cutoff by definition |
| Distribution trackers (Debian BTS, Launchpad, and similar) | User reports and maintainer closures | Closure text is sealed-bundle material |
| Published CVE timelines | A dated disclosure and a later advisory | Advisory text is often the resolution |

### Families

**PROPOSED.** Group candidates into families such as filesystem, networking, memory management, drivers, services, applications, and configuration. Families are for matching and for leakage review. They are not strata with approved sample sizes.

### What this corpus is not

- Not the five-paper GRAG research corpus.
- Not the E2 three-case episodic set.
- Not the C6 synthetic `plan-ol` fixtures.
- Not the MTCP fictional incident notes.
- Not an import of historical answers into the strategy table.
- Not a set of currently unresolved problems.

---

## 4. Experience acquisition and transfer

**Section status:** Approved for the decisions in this section, 2026-10-14. This approval does not authorize acquisition, corpus collection, or a change to controller scoring.

### Scoring

**APPROVED.** Keep the controller’s current `success_score` behavior. Do not write an external diagnosis grade or remedy grade back into EGAR, and do not edit those rows by hand. Diagnostic accuracy and proposed remedies are judged afterward, outside the experience store.

Incorrect diagnoses, unsuccessful trajectories, and misleading internal success scores stay in the snapshot. They are not removed to improve later results. Existing promotion thresholds stay as they are. If too few trajectories qualify and no strategy is injected, that outcome is reported. It is not a reason to lower a threshold.

### Genuine experience

**APPROVED.** Arm B’s trajectories come from actual Thoth goal executions on acquisition cases. Historical developer solutions are not imported. EGAR rows are not written by hand.

### Two partitions

**APPROVED.** Acquisition cases and held-out evaluation cases are distinct. An evaluation case may share a family with an acquisition case. It must not be the same issue, a near-duplicate, or a carrier of the evaluation solution.

| Partition | Who runs it | Document index | Experience store | Answer key |
|-----------|-------------|----------------|------------------|------------|
| **Acquisition** | Thoth, before evaluation | Visible bundle only | Whatever the controller writes during those runs, including unsuccessful rows | No |
| **Evaluation** | Thoth, Arm A and Arm B | Visible bundle only | Arm B reads a fresh copy of the frozen snapshot. The case database is not copied forward | Yes, sealed, and not visible during the run |

### Freeze

**APPROVED.** After acquisition, hash the database and keep that file as the snapshot. Each Arm B evaluation case gets a new disposable copy of that snapshot. Each Arm A case gets a new empty database. Evaluation writes stay in the case database. They do not re-enter the snapshot and do not enter the next case.

### Answer separation

**APPROVED.** Historical resolutions and sealed evaluation keys stay inaccessible during diagnostic execution. They are not indexed, not placed in the goal, and not stored as a goal, trajectory, or strategy. Acquisition goals must not quote an evaluation case’s sealed diagnosis. Logs that contain sealed-bundle text stay outside the retriever.

### Static acquisition investigation

**Closed for protocol design, 2026-10-15.** The read-only experience-pathway investigation is sufficient for protocol design. It is recorded in `docs/EGAR_LX_EXPERIENCE_PATHWAY_INVESTIGATION.md`. Closing it does not mean the engineering pilot, acquisition, or evaluation has been run, and it does not authorize those activities.

The investigation described the pre-correction graph path, in which a normal finish did not set `PlanStatus::COMPLETED` before reinforcement. P0–R2 changed that path. Terminal plan status is now recorded before reinforcement and history storage. The investigation’s findings on mechanical scoring, the compressed reuse payload, and promotion thresholds still describe the current implementation.

These limitations stay in force:

- Internal trajectory `success_score` measures mechanical execution, not diagnostic correctness.
- The reused experience payload remains compressed. It does not ordinarily inject the previous diagnosis.
- A diagnostically incorrect answer may still receive a high internal score.
- The corrected graph pathway has passed reported implementation tests. Those tests have not shown that the pathway improves diagnostic performance.

### Still open

**UNRESOLVED.** Pretraining may already contain famous bugs. The cutoff does not remove that threat. Whether a future acquisition run will qualify for strategy promotion, or for trajectory and plan-reuse injection, is not known until those runs exist. No such run is authorized here.

---

## 5. Evaluation methodology

**Section status:** Scoring approved 2026-10-14. Justified-uncertainty amendment approved 2026-10-15. Rubric clarifications approved 2026-10-15. Independent reviewer procedure approved 2026-10-15. Secondary measurements and leakage integrity approved 2026-10-15. Standardized diagnostic goal approved 2026-10-15. Case-eligibility requirements approved 2026-10-15. Diagnosis-scoring boundary approved 2026-10-15. Contradictory causal claims approved 2026-10-15. Mixed causal errors approved 2026-10-15. Remedy verification approved 2026-10-15. Measurement instrumentation approved 2026-10-15. Leakage-audit procedure approved 2026-10-15. Pretraining-contamination policy approved 2026-10-15. Fixed historical evidence approach approved 2026-10-15. Acquisition/evaluation fixed-evidence parity approved 2026-10-15. This approval does not authorize scoring software, case construction, tool-restriction enforcement, or the experiment.

### Task

**APPROVED 2026-10-15.** Use this instruction for every acquisition case and every evaluation case, and for both arms. Only the fixed pre-cutoff case fields below may change from case to case. The instruction is not rewritten because a case is assigned to Arm A or Arm B.

> Investigate the following Linux problem using the supplied historical case evidence, your general Linux knowledge, and permitted diagnostic tools.
>
> Identify the most likely underlying cause and explain which evidence supports your conclusion.
>
> Propose a technically justified remedy. If the available evidence is insufficient to establish the cause, identify the remaining possibilities, explain what evidence is missing, and propose the next useful diagnostic step.
>
> Distinguish observed facts from hypotheses. Do not invent logs, test results, or successful repairs.
>
> Problem:
> [CASE-SPECIFIC PRE-CUTOFF EVIDENCE]

**Fixed historical evidence.** **APPROVED 2026-10-15 — Approach 1.** **Acquisition/evaluation parity approved 2026-10-15.** Acquisition and evaluation both use a predefined, immutable historical evidence bundle that contains only permitted pre-solution information. Thoth analyzes that bundle with general Linux knowledge and the approved internal capabilities. It must not obtain additional case-specific evidence during the run. The comparison measures diagnostic reasoning and the effect of experience acquired on that same kind of task. It does not reconstruct a live Linux environment or retrieve further historical evidence interactively. Controlled historical retrieval and live diagnostic execution are possible later experiments. EGAR-LX v0.1 does not add an evidence-provider, a simulated diagnostic tool, live failure reproduction, a virtualized historical environment, or another experimental arm. Those designs need a separate review.

The quoted instruction is unchanged. In both acquisition and evaluation, “permitted diagnostic tools” in that instruction does not authorize external diagnostic execution. Thoth may analyze the supplied evidence, apply general Linux knowledge, use its existing Cognate reasoning, use the approved GRAG configuration, and retrieve and use EGAR experience under the stage and arm configuration. Internal retrieval stays inside the approved corpus boundaries, historical cutoffs, experience isolation, and leakage protections. Thoth may form and revise hypotheses from the supplied evidence, identify missing information, recommend diagnostic commands or investigative steps in the written answer, and propose a technically defensible remedy. Do not disable internal Cognate, GRAG, or EGAR operations, and do not disable acquisition learning, because external diagnostic tools are restricted.

Throughout EGAR-LX v0.1, Thoth must not execute live Linux diagnostic commands, inspect the host workstation as a substitute for the historical system, launch a container or virtual machine to recreate the failure, browse for more case-specific information, retrieve historical solutions, later patches, or post-cutoff findings, request more case artifacts from an interactive evidence provider, read sealed resolutions or reviewer materials, or modify or supplement the fixed bundle. This is a diagnostic-access boundary. It does not prohibit the approved internal Cognate, GRAG, or EGAR operations. Enforcing it requires a separately approved implementation plan. This amendment does not change runtime permissions, the acquisition algorithm, EGAR learning behavior, injection, retrieval, scoring, promotion, the snapshot procedure, or controller behavior.

During acquisition, the existing experience-generation and storage mechanisms stay in force. Approved experience writes required by that process are allowed. The experience database is not seeded with manually authored solutions or external evaluation grades. After acquisition, the approved snapshot is frozen and hashed. Acquisition cases and evaluation cases stay independent under the approved eligibility rules: they do not share the same underlying bug or a near-duplicate.

During evaluation, Arm A starts each case with a fresh empty experience database. Arm B starts each case with a fresh disposable copy of the frozen snapshot. Writes from one evaluation case do not transfer to another case, and evaluation does not modify the frozen snapshot. Both arms use the same model, code, diagnostic instruction, case evidence, external-tool restrictions, and other approved configuration. The intended difference remains the approved experience-bearing state.

Recommended commands are not executed, and their results are not invented. Reviewers judge a recommendation under the approved diagnosis and remedy rubrics. A useful diagnostic recommendation may receive remedy or next-action credit. A proposed genuine correction may receive full remedy credit when it meets the approved verification standard. The lack of live execution does not by itself lower the remedy score. Historical resolutions, later patches, sealed answer keys, and post-resolution findings stay inaccessible during diagnosis. They are not placed in the goal text, the visible corpus, the retrieval context, or the experience store.

The task requires observed evidence to be distinguished from hypotheses, evidence-supported reasoning, and a technically defensible remedy. Justified uncertainty is allowed when the pre-cutoff evidence cannot establish a definitive cause. In that situation the answer must identify the missing evidence and propose a useful next diagnostic step. Fabricated logs, unsupported claims, and invented repair outcomes are not rewarded.

The visible case input has these conceptual fields. They are not a software schema, and this amendment does not collect cases. A later implementation plan may define the machine-readable form, subject to review.

- **Case identifier.** A stable neutral identifier. It must not reveal the documented resolution, a vulnerability identifier, or the underlying cause.
- **Linux environment.** Distribution and version, kernel version, relevant services, software, dependencies, and configuration, and hardware or environmental details, when known and available before the cutoff.
- **Observed problem.** Historical symptoms, failure behavior, and user or operator observations available before the cutoff.
- **Pre-cutoff diagnostic evidence.** Available logs, error messages, configuration excerpts, recorded command outputs, historical traces or measurements, and other relevant evidence documented before the solution cutoff.
- **Evidence provenance and cutoff.** Provenance and cutoff information required by the case-eligibility procedure. These fields do not expose the sealed resolution. Only provenance appropriate to Thoth’s visible input is included. Restricted audit metadata stays separate.
- **Unavailable information.** Unavailable evidence is distinguished from evidence that an event did not occur. Missing logs, measurements, diagnostic results, and environment details are not invented.

For each evaluation case, Arm A and Arm B receive the same case-specific evidence, the same diagnostic goal wording, the same case-input formatting, and the same permitted and prohibited diagnostic capabilities. The case evidence is not changed because of an arm’s behavior, and one arm is not given extra evidence because it asked for something the other arm did not ask for. The approved isolation rules stay in force. The intended difference remains Arm A’s empty experience database and Arm B’s copy of the frozen acquisition snapshot. Both arms also receive the same retrieval corpus and model configuration. This amendment does not select cases or authorize a harness, acquisition, or a pilot.

### Primary outcome

**APPROVED 2026-10-14.** The case score is the sum of a diagnosis score and a remedy score. Each part is an integer from 0 to 2. The combined score is an integer from 0 to 4. There is no bonus point outside those scales.

Report the diagnosis score, the remedy score, and the combined score for every case. External scores do not replace, modify, or get written into the controller’s `success_score`.

### Diagnosis

**APPROVED.** Scores use the 2026-10-15 boundary, the contradictory-claims clarification, and the mixed-causal-errors clarification. There is no bonus point outside this scale. The same rules apply to both arms. Reviewers judge the technical meaning of the whole explanation. They do not count mistakes or subtract a point per mistake.

| Score | Meaning |
|-------|---------|
| 2 | Identifies all essential causal elements supported by the historical evidence, contains no materially incorrect causal claim, and is technically equivalent to the documented underlying cause. Exact historical wording is not required. Incidental details are not required |
| 1 | Any one of the situations below |
| 0 | The overall causal explanation is fundamentally incorrect, even if it names a correct subsystem or uses some correct terminology; or an incorrect causal claim undermines an otherwise partially correct explanation; or the diagnosis is unsupported; or it is an unjustified confident guess; or it is vague uncertainty without useful diagnostic reasoning |

Diagnosis score 1 has these approved situations. They are not extra points.

- **Partial cause.** Identifies a meaningful and technically correct portion of the actual causal chain, but omits an essential element.
- **Correct cause plus a material error.** Identifies the essential underlying cause and also adds a materially incorrect causal explanation. Naming every essential element does not by itself earn score 2 when that error is present.
- **Partial cause plus an error that does not undermine it.** Gives a meaningful partial causal explanation and also an incorrect claim that does not undermine the technically correct portion. This is the mixed omission-and-error case that remains score 1.
- **Evidence insufficient.** When the pre-cutoff evidence cannot establish a definitive cause, correctly narrows the plausible causes or the relevant subsystem, explicitly identifies the missing evidence, and explains why that evidence matters. A generic statement such as “more information is needed” does not by itself earn this score.

A materially incorrect causal claim changes, contradicts, or substantially misrepresents why the documented failure occurred. An unrelated minor factual error does not by itself reduce the diagnosis score. Naming the correct subsystem does not by itself earn a point when the accompanying explanation fundamentally misidentifies the failure. When a material causal error affects the score, the justification names that claim and explains its effect on the overall diagnosis.

Unsupported confidence is not rewarded. Well-supported uncertainty is not penalized merely because the answer declines to assert a definitive cause. Cautious wording alone is score 0.

**Essential causal elements.** Before evaluation, each eligible case has a sealed record of the essential causal elements reviewers use for the diagnosis score. The elements are grounded in the documented resolution and its supporting evidence, and they are distinguished from incidental details. Reviewers use that record and the supporting historical evidence. They allow a technically equivalent explanation. They do not require a literal match to the historical patch description. They distinguish meaningful partial understanding from incidental correct terminology. The sealed elements are never exposed to Thoth during diagnostic execution. No such record is created by this amendment. Independent blinded review, disagreement resolution, and the scoring audit trail are unchanged.

### Remedy or next action

**APPROVED 2026-10-15.** Diagnosis and remedy are scored independently. A remedy score of 2 does not require a diagnosis score of 2. The same standard applies to both arms. Thoth is not required to run the proposed fix, and the original machine does not have to be recreated. Full credit requires a genuine correction of the documented failure. An apparent improvement, symptom suppression, or resemblance to the historical patch is not enough.

| Score | Meaning |
|-------|---------|
| 2 | A technically feasible, supported remedy that addresses the actual underlying failure mechanism established by the historical evidence. A technically equivalent or alternative action qualifies when it genuinely addresses that cause. The exact historical patch is not required |
| 1 | A safe, relevant, and useful diagnostic, verification, or temporary recovery step that does not establish a complete correction. This includes a step that would help resolve remaining uncertainty, a useful diagnostic or verification step after the cause is already established, and a temporary recovery action that is appropriate and clearly identified as temporary |
| 0 | An infeasible, unsupported, harmful, or irrelevant action. This includes a symptom-suppression action presented as a complete correction when it does not meaningfully address the failure |

Score 2 also requires a defensible technical basis and a feasible action for the documented environment, including the relevant operating-system versions, services, dependencies, configuration, hardware, and other stated constraints. An action that could not reasonably work in that environment does not receive score 2. Reviewers use the sealed historical resolution, its supporting evidence, and this rubric. Historical evidence or other technically defensible evidence may support the remedy. A matching patch description alone does not establish correctness. When a reviewer awards score 2, the justification records the technical basis and distinguishes genuine causal correction from a coincidental or temporary improvement.

Do not award score 2 only because the action resembles the historical patch, temporarily suppresses symptoms, produces an unrelated improvement, or appears successful without addressing the documented failure. A service restart or workaround is not automatically a successful remedy and is not automatically worthless. Reviewers judge its relevance, safety, and stated purpose under this rubric. Independent blinded review, disagreement resolution, and the scoring audit trail are unchanged.

### Case eligibility

**APPROVED 2026-10-15.** This requirement replaces the earlier exclusion sentence. A historical Linux issue enters the corpus only when all of the following hold. No numerical threshold for document length, log count, or evidence volume is approved. This amendment does not select or download any case.

- **Resolution.** Documented evidence shows the issue was actually resolved. A closed issue or a merged pull request is not, by itself, that evidence. The resolution and its support stay in the sealed bundle. Unresolved cases are excluded from this initial experiment.
- **Cutoff.** A documented cutoff falls before the eventual solution became known. The record identifies the evidence available before that cutoff. Later diagnoses, patches, test results, and resolution details stay outside materials Thoth can read.
- **Diagnostic evidence.** Pre-cutoff evidence is sufficient for meaningful diagnostic reasoning. A case whose evidence cannot support a fair evaluation is rejected.
- **Environment.** The record includes enough information to understand the reported failure. That may include kernel or distribution versions, services, configuration, dependencies, hardware, or software versions. Reproducing the original machine is not required.
- **Review.** Sealed evidence is sufficient for independent reviewers to score both the diagnosis and the proposed remedy under the approved 0–4 rubric and reviewer procedure. A technically defensible alternative remedy is allowed. Reproducing the exact historical patch is not required.
- **Separation.** Pre-resolution evidence can be separated reliably from the eventual answer. Resolutions, sealed keys, and post-cutoff findings are not exposed through the visible bundle, the goal text, the retrieval context, or the EGAR experience store. Suspected leakage and confirmed leakage stay distinct. Confirmed exposure of sealed answer material remains an experimental integrity failure.
- **Independence.** An acquisition case and a held-out evaluation case do not share the same underlying bug, the same issue, or a near-duplicate. Related problem families are allowed. The record states why the cases are sufficiently distinct.

Every candidate, accepted or rejected, has a written eligibility record: case identifier and source provenance; cutoff and its justification; evidence that the issue was resolved; pre-cutoff diagnostic evidence; relevant environment information; visible and sealed separation; the independence assessment; the decision and its rationale; and any unresolved concerns. Rejected-candidate records are kept for later methodological review.

**Pretraining contamination.** **APPROVED 2026-10-15.** A familiarity flag means possible prior exposure through model pretraining. It is not confirmed memorization and it is not confirmed experimental leakage. It does not add or subtract diagnosis or remedy points. Confirmed experimental leakage stays under the leakage-audit procedure. This amendment does not collect or classify cases.

During future corpus preparation, flag a candidate when pretraining familiarity is plausible. Indicators include a widely documented historical Linux bug, a famous or extensively discussed CVE, a highly publicized kernel or userspace failure, or a distinctive identifier, error message, or patch reference that may be recognizable from public documentation. Do not label a case as memorized without evidence for that stronger conclusion. The cutoff does not eliminate knowledge acquired during model pretraining.

The eligibility record for each flagged candidate states why familiarity is suspected. That basis may include public visibility, publication history, a recognizable bug or vulnerability identifier, distinctive technical details, and the provenance of the flag.

Do not reject a case only because the model might recognize it. The eligibility requirements above still apply. An otherwise eligible flagged case stays in the predefined primary A/B population. Do not remove a case because of Thoth’s later diagnosis or remedy score. Other approved eligibility and integrity rules still apply.

Both arms use the same underlying model, the same case set, and the same scoring requirements. Do not change the primary population after results are seen merely because a case appears familiar. Familiarity may affect both arms. Using the same model does not establish that its effect on the measured EGAR difference is zero.

When enough cases exist, also report a sensitivity comparison that excludes cases prospectively flagged for possible pretraining familiarity. That comparison is secondary. It does not replace the primary comparison. Report the primary comparison on the predefined eligible population, the sensitivity comparison on the eligible unflagged cases, how many flagged cases were left out of the sensitivity subset, and whether the estimated EGAR effect changes materially. If the unflagged subset is too small for a meaningful reading, say so. This amendment sets no corpus size, statistical threshold, or exclusion count. Sensitivity results are not used to redefine the primary analysis afterward.

Freeze familiarity classifications and their supporting criteria before evaluation begins, and before Arm A or Arm B results exist. Keep the original classification records. Do not change a flag afterward to explain an unexpected score. A concern found later is recorded separately, with provenance and timing. Do not silently change the predefined primary population or the sensitivity subset. A future implementation plan must say how classifications and their freeze state are recorded and verified. This amendment does not create those records. A flag does not establish that the model encountered the case in training, memorized the resolution, used memorized information in the answer, or received prohibited experimental information.

### Review

**APPROVED 2026-10-15.** This procedure replaces the 2026-10-14 statement that two blinded reviewers score each case. It does not change the 0–4 scale, the component definitions, or the separation from `success_score`.

Two reviewers score each case. Each has enough Linux troubleshooting knowledge to judge the historical case, its underlying cause, and proposed remedies, and each can apply the diagnosis and remedy rubric in this section.

For evaluation only, each reviewer may examine the visible evidence bundle, the sealed historical resolution and its supporting evidence, the sealed essential-causal-element record, Thoth’s diagnostic response and proposed actions, and this rubric. Both reviewers, and any third reviewer, use that same sealed record and this rubric. Sealed materials are never exposed to Thoth during diagnostic execution.

Reviewers do not see which arm produced the response, the EGAR experience database or accumulated experience, or Thoth’s internal `success_score`. The same restrictions apply to a third reviewer if one is used.

Each reviewer independently assigns an integer diagnosis score from 0 to 2 and an integer remedy score from 0 to 2, with a brief evidence-based justification for each. The two initial reviews are finished before the reviewers discuss them.

If the reviewers disagree on a component:

1. Keep both original reviews.
2. Let the reviewers compare their evidence and reasoning.
3. Try to reach an evidence-based consensus on the disputed component, using this rubric.
4. If disagreement remains, refer only the disputed component or components to a third reviewer who meets the same qualification standard as the first two. That reviewer uses the same rubric, the same historical evidence, and the same sealed essential-causal-element record, and stays blinded as above.

The case then has one final diagnosis score and one final remedy score. Their sum is the 0–4 primary outcome. The third reviewer decides only components that are still disputed. Component scores are not averaged, and fractional points are not used.

Keep both initial reviews, their scores and justifications, any disagreements, consensus decisions and their reasons, any third-reviewer adjudication, and the final component scores and combined score. The record must be detailed enough for a later check of scoring consistency. Reviewer agreement may be analyzed as a secondary methodological measure. It does not change the primary score.

### Secondary measurements and leakage integrity

**APPROVED 2026-10-15.** Record the five categories below. They do not add points and do not change the diagnosis rubric, the remedy rubric, or the 0–4 primary outcome. Leakage classifications do not add or subtract diagnosis or remedy points.

**Evidence use.** Record whether the diagnosis is supported by relevant evidence, whether that evidence was available before the historical cutoff or is an unsupported assertion, and whether cited or retrieved evidence actually supports the proposed diagnosis.

**Recovery behavior.** Record whether unsuccessful diagnostic steps are recognized and whether the approach changes after a failure or contradictory evidence. Keep the failed attempts and the recovery behavior, not only the final answer. This record does not change EGAR’s existing recovery or reflection behavior.

**Execution cost.** Where available, record wall-clock elapsed time, token usage, the number of tool calls, failed diagnostic or execution steps, and timeouts and stop conditions. Use the same definitions and time boundaries on both arms. A measurement that was not produced is unavailable, not zero. Do not infer a token count, a tool count, or an elapsed time from an unsupported assumption, and do not change a runtime limit without a separate approval.

**EGAR activation.** Where observable, record whether experience retrieval was attempted, whether it succeeded, whether retrieved experience was injected, whether a strategy, trajectory, or plan was reused, whether graph-based experience evidence was used, whether retrieved material was omitted because of a context-budget limit, and whether relevant EGAR activity occurred without injection. Keep the available evidence of `STRATEGY_INJECTION`, `TRAJECTORY_INJECTION`, and plan reuse. An Arm B case is not dropped only because no activation was observed. Existing injection thresholds, retrieval behavior, and promotion rules stay as they are. A negative primary result with no observed activation is reported separately from a negative primary result in which experience was available and was used.

**Answer leakage.** Audit whether prohibited case-specific information was delivered to Thoth, not merely whether it exists somewhere in the evaluation environment. Pathways are the visible bundle, the diagnostic prompt and case-specific fields, retrieval results and retrieved content, tool outputs, EGAR experience records and the frozen Arm B snapshot, execution logs and context-injection records, and any other observable path that could expose sealed case-specific material.

**Confirmed leakage** requires verifiable evidence that the prohibited information reached Thoth. That evidence may be a recorded prompt, a retrieval or tool-output record, an execution trace into the working context, or another reproducible exposure path. A correct diagnosis, an exact patch recommendation, or an unusually detailed answer does not by itself confirm leakage. Answer similarity is not that evidence. Confirmed leakage is an integrity failure. Quarantine the affected result from ordinary valid-case analysis. Keep the evidence, record the pathway, and check whether other cases or the frozen Arm B snapshot were affected. Keep a traceable record of the failure and its disposition. Do not silently drop or replace the case, and do not rebuild a snapshot, rerun the evaluation, or change the analysis population under this amendment.

**Suspected leakage** is a credible concern that exposure has not been verified. Flag the case, keep the original prompts, outputs, logs, and execution evidence, record why it is suspected, and record what further evidence would confirm or resolve it. An unresolved suspicion is not confirmed leakage, and the case is not silently removed.

**No detected leakage** means the available audit evidence does not identify prohibited exposure. It does not prove that undetectable exposure was impossible. A material gap in what the audit can observe is recorded. Missing audit evidence is not proof that no exposure occurred.

Model familiarity from pretraining is not experimental leakage. A pretraining flag stays a flag under the pretraining-contamination policy above. It is not, by itself, confirmed exposure, and it does not by itself exclude an otherwise eligible case from the primary population.

The audit does not show arm assignment, EGAR database contents, or `success_score` to the blinded diagnosis and remedy reviewers. Investigation material that would reveal the arm stays separate from reviewer-facing scoring material. Original reviewer scores and explanations are kept. The link between a score record and an integrity investigation remains auditable without revealing the arm early. A leakage failure is not evidence of successful EGAR reasoning, and it does not change the 0–4 score.

**Instrumentation.** Collect these observations from existing logs and externally observable execution events. Do not change Cognate, GRAG, or EGAR decision logic, retrieval rankings, experience selection, injection, or the internal `success_score`. Do not add diagnostic information to Thoth’s context in order to measure it. If existing logs cannot support a required measurement, record an instrumentation gap. Do not treat that gap as proof that the event did not occur.

Each EGAR-activity item is one of three states. **Observed activity** means the event was positively recorded. **Observed absence** means the available instrumentation supports that the event did not occur. **Unavailable** means the instrumentation cannot establish whether it occurred. Unavailable is not inactivity.

Both arms use the same logging configuration, timing boundaries, event definitions, collection procedure, availability rules, and timeout and failure recording. A difference that cannot be avoided is written down and investigated before results are interpreted.

Each measurement record names the stable case identifier, an arm-independent run identifier, the relevant configuration and artifact hashes, timestamps, and whether the measurement was available. Reviewer-facing records do not reveal arm assignment, EGAR database contents, or the internal `success_score`. Sealed resolutions and scoring records stay inaccessible to Thoth. Instrumentation does not change prompts, goals, permitted tools, or tool outputs; does not expose sealed answers, resolutions, or the rubric; does not move evaluation memory between cases; does not change the frozen Arm B snapshot; and does not put arm-specific information into the reasoning context.

Logging can consume time and resources. Both arms carry the same instrumentation. A meaningful difference in that overhead is a validity concern. This amendment does not assume the overhead is zero, and it does not authorize a pilot to measure it.

**Implementation details deferred.** A later implementation plan must specify the machine-readable case-input representation; enforcement of the external diagnostic restrictions during both acquisition and evaluation; preservation of internal Cognate, GRAG, and EGAR operations; verification that both stages use this fixed-evidence approach; verification of Arm A and Arm B configuration and evidence parity; case-identifier and provenance leakage protections; how frozen familiarity classifications are recorded and verified; and the mapping from required measurements to existing audit-log events. That plan is not authorized by this amendment.

---

## 6. Experimental design and statistical analysis

**Section status:** Primary estimand approved 2026-10-15. Analysis population principles approved 2026-10-15. Missingness and incomplete-pair handling approved 2026-10-15 — Option B. Single generation per arm and no generation seed approved 2026-10-15. Confirmed-leakage numerical disposition approved 2026-10-15 — Option A. Primary uncertainty method approved 2026-10-15 — 95% paired bootstrap confidence interval. Family-dependence treatment approved 2026-10-15 — Option A: account for Linux problem-family clustering in the final evaluation's 95% paired bootstrap confidence interval. Engineering pilot design approved 2026-10-15 — Option A: four acquisition cases, four paired evaluation cases, at least two Linux problem families. Arm-order randomization approved 2026-10-15 — Option A: reproducible, approximately balanced randomization of A→B versus B→A execution order. EGAR non-injection reporting approved 2026-10-15 — Option A: report confirmed no-injection frequency and observability gaps for Arm B as secondary descriptive observations. Minimum worthwhile improvement approved 2026-10-15 — +0.5 points on the combined 0–4 scale, for final sample-size planning only. Target precision approved 2026-10-15 — Option B: approximate ±0.5-point half-width for the final 95% family-cluster-aware paired bootstrap confidence interval, as a planning objective. Final acquisition and evaluation sample sizes are not approved. Sample-size planning approach approved 2026-10-15 — Option A: scenario-based planning. Paired-score variability scenarios approved 2026-10-15 — hypothetical standard deviations 0.75, 1.25, and 2.00 points. Within-family dependence scenarios approved 2026-10-15 — hypothetical ICC 0.05, 0.15, and 0.30. Family-count configurations approved 2026-10-15 — Option A: hypothetical Small 4×5=20, Medium 8×5=40, and Large 12×5=60 paired evaluation cases. Combined pair-loss scenarios approved 2026-10-15 — Option A: hypothetical 0%, 10%, and 20%. Pair-loss distribution models approved 2026-10-15 — Option A: spread losses and concentrated losses. Hypothetical pair-loss allocation method approved 2026-10-15 — Option A: deterministic. Hypothetical combined pair-loss rounding approved 2026-10-15 — Option A: nearest whole pair, exact halves upward, L = floor(N × p + 0.5). Hypothetical pair-loss family ordering approved 2026-10-15 — Option A: fixed neutral family IDs in ascending order. Family-ID assignment convention approved 2026-10-15 — Option A: alphabetical canonical names, then F01, F02, F03, and so on. Hypothetical pair-loss within-family case ordering approved 2026-10-15 — Option A: stable unique case IDs in ascending order. Case-ID assignment convention approved 2026-10-15 — Option B: sequential C001, C002, C003, and so on, in documented formal registration order. Hypothetical spread-loss allocation approved 2026-10-15 — Option A: deterministic round-robin. Hypothetical concentrated-loss allocation approved 2026-10-15 — Option A: fill one family at a time. Family-level cluster bootstrap approved 2026-10-15 — Option B. Final bootstrap specifications approved 2026-10-15: 10,000 replicates, a percentile 95% interval, an eight-family reporting minimum, and a 5,000-versus-10,000 stability check. The numerical bootstrap seed is not yet selected. Experimental Parameter Register approved 2026-10-15. This approval does not authorize executing the pilot or the experiment.

### Design

**Approved for protocol design, 2026-10-15. Not authorized for execution.**

- Unit of analysis: one evaluation case.
- Each case runs on Arm A and Arm B (paired).
- Prompt, corpus, and model pin match within the pair.
- Workspaces do not match. They are separate directories or volumes.
- Order: within each pair, Arm A then Arm B, or Arm B then Arm A, under the approved rule below. The order seed is not a llama.cpp generation seed. No generation seed is added.
- Acquisition runs finish, and the store is frozen, before the first evaluation pair.

### Primary estimand

**Primary estimand approved 2026-10-15.** The primary case score is the externally adjudicated combined score approved in Section 5. Diagnosis contributes an integer from 0 to 2. Remedy or next action contributes an integer from 0 to 2. The combined score is their sum, an integer from 0 to 4.

For each eligible paired evaluation case, the case difference is the Arm B combined score minus the Arm A combined score. The primary estimand is the mean of those paired differences across complete scorable pairs in the analysis population defined below, excluding confirmed-leakage pairs under Option A.

Report the mean paired difference of the diagnosis scores and the mean paired difference of the remedy scores alongside the combined result. External reviewer grades are never written into EGAR’s internal `success_score`. That internal score is not the primary experimental outcome.

### Analysis population

**Analysis population principles approved 2026-10-15.** The primary population is the set of evaluation cases that meet the approved Section 5 eligibility requirements and are designated for the primary comparison before any evaluation result is seen. That set is defined and frozen before the final evaluation. Case-eligibility, acquisition and evaluation independence, and the Section 5 leakage audit stay in force.

Do not add, remove, replace, or reclassify a case because one arm scored higher, because experience was or was not retrieved or injected, because a diagnosis or remedy was correct or incorrect, because the result supports or fails the hypothesis, because the case later looks easy or difficult, or because the answer resembles a historically published solution.

An otherwise eligible case stays in that population when both arms produce a valid reviewable result, when Arm B does not retrieve experience, when Arm B retrieves experience but does not inject it, when no EGAR activation is observed, or when the case was flagged before evaluation for possible pretraining familiarity. Lack of activation is an observation, not a reason to drop the case. A familiarity flag is not memorization and is not confirmed leakage. Observed performance does not change membership.

Suspected leakage is handled by the Section 5 audit. Flag the case, keep the original evidence and run records, and record the concern and its basis. Suspicion alone does not remove the case. Do not automatically exclude, replace, rerun, or silently reclassify it. An unresolved integrity concern is reported. Suspected leakage and confirmed leakage stay distinct.

Confirmed leakage is an integrity failure under that same audit. The numerical rule is Option A below. Quarantine does not remove the case from the frozen population.

The Section 5 pretraining sensitivity comparison is unchanged. Otherwise eligible flagged cases stay in the primary population. The comparison on the eligible unflagged subset, when enough such cases exist to interpret it, does not replace the primary analysis and is not a second success claim. This decision sets no sample-size or significance threshold for it. A concern found after the familiarity freeze is recorded and does not silently change either set.

Membership in the predefined population, a complete scorable pair, and a numerical contribution to the paired mean are three separate facts. A case that does not contribute to the mean is not erased from the experimental record. Ordinary missing arms are handled under Missing results below. Confirmed-leakage pairs are handled under Option A below.

### Confirmed-leakage numerical disposition

**Confirmed-leakage numerical disposition approved 2026-10-15 — Option A.** Confirmation requires traceable, verifiable evidence that prohibited case-specific information reached Thoth, under the Section 5 leakage audit. Suspicion, historical familiarity, or a correct answer alone is not confirmation. Suspected leakage stays flagged and is not excluded for suspicion alone. A pretraining-familiarity flag is not confirmed leakage. A flagged case stays in the frozen population and in the primary numerical analysis unless separate evidence establishes an integrity violation.

A confirmed-leakage pair remains in the original frozen evaluation population. Keep its case identifier, both arm records, outputs, logs, any scores, and the leakage evidence. Quarantine the pair and exclude its paired difference from the primary numerical mean. Do not silently drop it from the experiment. Do not describe the pairs that remain in the mean as the complete frozen population. Do not automatically replace, selectively rerun, or substitute the case. A corrective experiment needs a separate approval.

Investigate whether the same failure affects other cases, the frozen Arm B snapshot, the corpus, or another shared component. If that failure undermines the wider A/B comparison, report the primary result as compromised or inconclusive. Do not produce a clean result only by leaving contaminated pairs out of the mean.

Report the frozen evaluation population size, the number of complete scorable pairs, the number of incomplete pairs from ordinary missing outcomes, the number of confirmed-leakage quarantined pairs, and the number of pairs that contribute to the numerical primary mean. Also report the affected identifiers, the documented quarantine reasons, the affected arm or pathway when known, and the effect on the primary analysis. A pair that is both quarantined and missing is described as both. In the reconciliation it is counted once, as a confirmed-leakage pair, and not again as an ordinary incomplete pair. The frozen population equals the pairs in the numerical mean, plus ordinary incomplete pairs that are not confirmed leakage, plus all confirmed-leakage pairs. Complete scorable pairs are reported as their own count. They are not added on top of those three groups.

### Missing results and incomplete pairs

**Missingness and incomplete-pair handling approved 2026-10-15 — Option B.** A timeout, a crash, a failure before a reviewable answer, or an entirely unscorable output is a missing outcome. It is not a diagnosis score of 0 or a remedy score of 0. A completed reviewable answer is scored with the Section 5 rubric. An incorrect answer may receive 0. A missing outcome is not evidence that the diagnosis was incorrect.

The primary mean uses only evaluation cases in which both Arm A and Arm B have a valid, externally scorable answer, and which are not confirmed-leakage pairs under Option A. If either arm has no scorable outcome, the pair is incomplete and does not contribute to that mean. The incomplete pair stays in the frozen population and in the audit trail. It is not deleted or replaced. Keep the original run and the failure evidence. Do not automatically rerun, replace, or selectively repeat a failed case to improve the result.

That mean is conditional on both arms producing scorable answers. It does not automatically represent every attempted case. If missingness differs between arms, or if it appears related to diagnostic difficulty, report the possibility of selection bias.

Report the following separately for Arm A and for Arm B, each with an explicit denominator: attempted runs, scorable runs, timeouts, crashes or execution failures, entirely unscorable outputs, incomplete pairs, complete pairs that contribute to the primary mean, and the corresponding failure and missingness rates.

Ordinary run failure, suspected leakage, and confirmed integrity failure stay distinct. Confirmed-leakage pairs are excluded from the numerical mean under Option A. The shared runtime budget remains the one already stated for both arms: `THOTH_LLM_TIMEOUT_SECONDS` if set, otherwise the code default of 900 seconds. How a log line is classified as a timeout, a crash, or an unscorable output, and which log event supplies that classification, stay deferred. Section 5’s passive timeout and stop-condition measurements stay in force. This decision invents no new threshold.

### Generations

**Single generation per arm and no generation seed approved 2026-10-15.** Each evaluation case runs once on Arm A and once on Arm B. Each of those runs uses the fresh isolated experience state already approved for that arm. llama.cpp is not given a generation seed. Thoth’s existing generation behavior stays as it is. The arm-order seed approved below is not a generation seed.

EGAR-LX v0.1 does not use repeated generations. A further run is not added because the first result was unfavorable or because a run timed out or was unscorable. A missing outcome stays missing under Option B. One generation per arm cannot measure how much the sampler would vary on the same case. That is a limitation of this version.

Repeated-generation experiments and controlled generation-seed experiments may be considered after EGAR-LX v0.1. This subsection does not design or authorize them.

### Arm order

**Arm-order randomization approved 2026-10-15 — Option A: reproducible, approximately balanced randomization of A→B versus B→A execution order.** For every eligible evaluation case, the assigned order is either Arm A followed by Arm B, or Arm B followed by Arm A. The assignments come from a reproducible recorded seed. That seed controls execution order only. It is not sent to llama.cpp and does not change generation behavior. One generation per arm stays in force. This rule does not add generations, retries, or selective reruns.

The number of A→B assignments and the number of B→A assignments are as close as practical across the frozen evaluation population. If the number of cases is odd, a difference of one is acceptable. The full schedule is generated and frozen before final evaluation begins. It does not depend on case difficulty, expected performance, EGAR activation, or observed results. A case keeps its assigned order if an arm times out, crashes, is unscorable, or is under a leakage investigation. The schedule is not selectively changed. If an operational failure prevents the assigned order, keep the original assignment, record the deviation and the reason, and flag it for review. Do not silently use another order.

The audit trail records the seed, the assignment procedure, the case identifiers, the assigned orders, and the order actually run. Arm isolation is unchanged. Experience, context, and evaluation writes do not move between arms or cases because of the order.

The four-pair engineering pilot uses the same rule. When all four pairs are scheduled, two are A→B and two are B→A. The pilot remains a procedural check, not an effectiveness test. This approval does not authorize running the pilot. Final sample size stays unapproved. The family-level cluster bootstrap, the 10,000-replicate count, the percentile interval, the eight-family safeguard, and the stability check are approved. The numerical bootstrap seed is not yet selected.

### Uncertainty

**Primary uncertainty method approved 2026-10-15 — 95% paired bootstrap confidence interval.** **Family-dependence treatment approved 2026-10-15 — Option A: account for Linux problem-family clustering in the final evaluation's 95% paired bootstrap confidence interval.** The point estimate remains the mean paired difference of the external combined 0–4 scores across contributing cases, Arm B minus Arm A. It is not an equally weighted average of family means. The interval is a 95% bootstrap confidence interval for that case-weighted mean. It is an uncertainty summary. It is not proof that EGAR works or fails. No *p*-value threshold, significance gate, family-specific success threshold, or separate confirmatory family hypothesis is added.

Before the final evaluation, every eligible evaluation case is assigned to a documented Linux problem family. The assignment criteria and the case-to-family mapping are frozen before any Arm A or Arm B result is seen. They are not changed afterward to fit those results.

**Family-level cluster bootstrap approved 2026-10-15 — Option B.** For the final evaluation, begin with the frozen evaluation population and apply the approved missingness and confirmed-leakage rules. The contributing cases are the complete, scorable, non-quarantined pairs. Group those pairs by their frozen Linux problem-family identifiers. For each bootstrap replicate, sample entire contributing families with replacement. Each replicate draws as many family selections as there are distinct contributing families. Keep every contributing paired case inside a selected family. Do not resample individual cases inside a selected family. If a family is selected more than once, include its complete contributing case group once for each selection. For that replicate, calculate the case-weighted mean of Arm B minus Arm A combined-score differences over all contributing pair occurrences. The previously approved 95% interval uses that bootstrap distribution. The observed primary point estimate remains the case-weighted mean over all contributing pairs. It is not an equally weighted average of family means. Only those contributing pairs enter the point estimate and the interval. Incomplete pairs and confirmed-leakage pairs stay out, under the approved missingness and leakage rules. The bootstrap does not change the frozen population, reviewer scores, missingness rules, leakage disposition, or randomized execution order. It does not draw the two arms independently.

The independent resampling units are Linux problem families, not individual evaluation cases. Within-family paired observations remain together. Family sizes may be unequal. A family with no contributing evaluation pair remains in the frozen population and in the audit records, but it cannot supply an observed paired difference to this bootstrap. This method does not correct selection bias caused by missing or quarantined pairs. The engineering pilot is not intended to establish EGAR effectiveness or to provide reliable cluster-level inference.

Report the observed paired mean, the interval, and the number of contributing pairs. Also report how many Linux problem families contribute, how many contributing pairs are in each family, and any important imbalance in family representation. Family-level descriptions may be reported. They are not extra success tests. Diagnosis and remedy paired differences continue to be reported separately. A cluster bootstrap can be unreliable when few distinct families are present or when few pairs contribute. If family diversity or the contributing-pair count is too small for a defensible interval, report that limitation. The confirmatory-interval minimum is the eight-family safeguard below. That safeguard sets no further minimum number of cases inside a contributing family.

**Bootstrap specifications approved 2026-10-15.** Use 10,000 bootstrap replicates. The 95% interval is a percentile interval: the 2.5th and 97.5th percentiles of those 10,000 replicate statistics. The bootstrap random seed is a fixed, documented 64-bit seed, selected and frozen before final evaluation. The specific numerical seed is not yet selected. It must be independent of the arm-order randomization seed, and it is not a llama.cpp generation seed. The random-number generator and the sampling convention are documented before final evaluation so the procedure is reproducible. A confirmatory 95% interval requires at least eight distinct Linux problem families, each with at least one complete, scorable, non-quarantined evaluation pair. If fewer than eight families contribute, report the observed case-weighted Arm B minus Arm A paired mean and the descriptive results, and do not report a confirmatory 95% bootstrap interval. Explain that the prespecified minimum-family safeguard was not met. Do not substitute a different interval method without explicit protocol approval. If no complete contributing pair exists, the primary numerical mean is not estimable. Do not invent one.

Compare the two percentile endpoints from the first 5,000 replicates with the endpoints from all 10,000 replicates. If either corresponding endpoint differs by more than 0.1 combined-score points, flag the interval as numerically unstable. Report the observed discrepancy and the limitation. Do not selectively add replicates, change the seed, or rerun until the interval looks favorable. The stability check assesses numerical reproducibility, not statistical validity. The interval may be reported as unstable even when the eight-family minimum is met.

Eight families is an operational minimum, not a guarantee of valid or sufficiently precise inference. The hypothetical Small layout of four families cannot meet this minimum. The hypothetical Medium layout of eight families meets the numerical minimum only if all eight retain at least one contributing pair. The hypothetical Large layout may meet the minimum, depending on how many families actually contribute. A bootstrap interval does not remove missingness selection bias, leakage concerns, family imbalance, or other experimental limitations. The approximate ±0.5-point target half-width remains a planning objective, not a required achieved width or a success criterion. The +0.5-point minimum worthwhile improvement remains a separate planning parameter, not a significance gate. No new hypothesis test and no automatic EGAR success or failure rule is added. These specifications are not chosen or revised after the results are seen. This amendment does not calculate or simulate an interval. The earlier statement that family dependence itself was unapproved is withdrawn. The earlier choice between a bootstrap and an exact paired test stays withdrawn.

The engineering pilot remains a procedural check with four acquisition cases and four paired evaluation cases from at least two families. It is not required to produce a reliable cluster-aware effectiveness estimate. Final sample size and any requirement for how many families the final evaluation must include remain unresolved.

### Engineering pilot

**Engineering pilot design approved 2026-10-15 — Option A: four acquisition cases, four paired evaluation cases, at least two Linux problem families.** This approves the design. It does not authorize collecting cases or running the pilot.

The pilot has four acquisition cases and four paired evaluation cases, from at least two distinct Linux problem families. Each evaluation case runs once on each arm, under the approved single-generation rule. The pilot checks whether the procedure is workable. It does not establish whether EGAR is effective. Four paired evaluation cases are not enough for a reliable effectiveness conclusion. Pilot score differences are not used to declare that EGAR works or fails, to keep favorable cases, to change the scoring rubric, or to tune the later analysis toward a desired result. Procedural problems and any proposed correction are written down for a separate review before a final evaluation is authorized.

The pilot checks these points:

- Historical case eligibility, provenance, and pre-solution cutoffs.
- Separation of visible evidence from sealed resolutions and scoring records.
- Experience generation by Thoth during acquisition.
- Creation and verification of the frozen experience snapshot.
- Arm A and Arm B experience-state isolation.
- The same fixed historical evidence and the same permitted operations on both arms.
- One generation per arm.
- Passive observations and audit records.
- Independent blinded external scoring and adjudication.
- Detection and documentation of leakage and integrity problems.
- Runtime, resource, or procedural problems that could block a larger evaluation.

Pilot cases are not the final evaluation corpus. They are not automatically reused. The final evaluation uses separately designated eligible cases and its own frozen analysis population. Reusing a pilot case would need a later approval that considers exposure, learning contamination, and any protocol revision.

**Final sample size is not approved.** **Minimum worthwhile improvement approved 2026-10-15 — +0.5 points on the combined 0–4 scale, for final sample-size planning only.** The planning target is an average Arm B minus Arm A improvement of +0.5 points on the external combined score. It is the smallest average improvement the final evaluation should be designed to estimate meaningfully. It is used when considering final sample size, expected variation, family dependence, and the precision wanted. It is not a significance threshold, an automatic success rule, or a result the experiment is required to produce. The observed mean is reported and interpreted whether it is above, below, or equal to +0.5. Crossing +0.5 is not a pass or fail.

The primary estimand stays the case-weighted mean of Arm B minus Arm A combined scores across complete, scorable, non-quarantined pairs. The 95% family-cluster-aware paired bootstrap interval stays the uncertainty summary. The four-plus-four pilot stays a procedural check and is not changed by this target.

Acquisition and evaluation sizes for the final evaluation are chosen later. **Sample-size planning approach approved 2026-10-15 — Option A: scenario-based planning.** Compare several documented scenarios. Do not treat one unverified variability figure as established fact. Each scenario states its assumptions, and those assumptions are not observed measurements. A scenario identifies the assumed variability of the paired Arm B minus Arm A combined scores, the assumed within-family dependence, the number of Linux problem families, the number and distribution of evaluation cases across families, the expected number of complete, scorable, non-quarantined pairs, any anticipated missingness or leakage losses, approximate runtime and reviewer workload, and the expected precision relative to the ±0.5 half-width target. The comparison should show how family clustering, the amount of information, the case count, and the achievable precision move together. **Paired-score variability scenarios approved 2026-10-15.** The initial hypothetical standard deviations of the case-level paired difference, Arm B minus Arm A on the combined 0–4 score, are 0.75 (low), 1.25 (moderate), and 2.00 (high). They are not standard deviations of the separate Arm A or Arm B scores. Combined scores run from 0 to 4, so a paired difference runs from −4 to +4. These three figures are planning assumptions, not observed Thoth results. None is designated true, most likely, or preferred. **Within-family dependence scenarios approved 2026-10-15.** The initial hypothetical intraclass correlations among those case-level paired differences are 0.05 (low), 0.15 (moderate), and 0.30 (high). An ICC is not a correlation between the Arm A score and the Arm B score, and it is not a direct measure of how similar two Linux problems are. These figures are planning assumptions, not observed results. None is designated true, most likely, or preferred. Crossed with the three standard deviations, they give nine hypothetical SD × ICC combinations. No combination is preferred or treated as established. Within-family correlation can reduce the effective amount of independent information from several cases in the same family. How much clustering matters depends on the number of families, their sizes, and how cases are distributed across them. **Family-count configurations approved 2026-10-15 — Option A.** Three hypothetical final-evaluation examples hold five paired evaluation cases in each family. Small is 4 families and 20 cases. Medium is 8 families and 40 cases. Large is 12 families and 60 cases. Each case still has one Arm A generation and one Arm B generation. These examples are not the final family count, the final evaluation size, or the final allocation. None is preferred. Holding five cases per family fixed lets a later comparison ask how adding families changes the information available for the precision target and the workload that comes with it. Equal family sizes are assumed only in these examples. Unequal sizes can be considered later. Four families may be too few for a reliable family-cluster-aware bootstrap interval. Including the small configuration does not mean it is statistically adequate. Crossed with the three standard deviations and the three ICCs, these examples give 27 hypothetical planning combinations. No combination is preferred or treated as established. Missingness, leakage, runtime, and reviewer workload are not assigned as separate figures here. **Combined pair-loss scenarios approved 2026-10-15 — Option A.** The initial hypothetical combined pair-loss rates are 0% (low), 10% (moderate), and 20% (high). Combined pair loss is the share of attempted paired evaluation cases that cannot enter the numerical primary analysis because at least one arm lacks a complete, reviewable, scorable answer, or because the pair is quarantined for confirmed prohibited leakage. A pair that meets both conditions is counted once. These rates are planning assumptions. They are not observed failure rates, leakage rates, or predictions of Thoth’s reliability. They do not say how the loss is split between missingness and confirmed leakage, and they do not say how it falls across families. **Pair-loss distribution models approved 2026-10-15 — Option A.** Two hypothetical ways of placing lost pairs across families are recorded. **Spread losses** places them approximately evenly across the families in the layout. It does not require every family to keep a contributing pair. A family may lose all of its contributing pairs if the later allocation rule allows that. **Concentrated losses** places them preferentially in fewer families, and an entire family may have no contributing pair when the assumed loss count allows it. Neither model is the true, preferred, or most likely pattern of missingness or leakage. They are planning assumptions, not observed Thoth behavior. Concentrating losses can reduce the number of families that still contribute, and that may change the reliability and precision of cluster-aware inference. The four approved dimensions still give 81 baseline combinations. Examining each under both models yields up to 162 scenario variants. At 0% loss the two models are the same. **Hypothetical pair-loss allocation method approved 2026-10-15 — Option A: deterministic.** Future sample-size planning places hypothetical lost pairs by a fixed rule, not by a random draw. The same inputs and the same allocation rules must produce the same placement. This convention applies to both spread losses and concentrated losses. It does not predict or control which cases actually become incomplete or quarantined. No allocation seed is used. The exact deterministic algorithm is not yet approved. **Hypothetical combined pair-loss rounding approved 2026-10-15 — Option A: nearest whole pair, exact halves upward.** This is part of the deterministic allocation method. For both spread losses and concentrated losses, the integer count of hypothetical lost pairs is L = floor(N × p + 0.5), where N is the number of attempted paired evaluation cases and p is the hypothetical combined pair-loss proportion, 0.00, 0.10, or 0.20. An exact halfway value rounds upward. For example, 25 attempted pairs at a hypothetical 10% loss give 2.5, which rounds upward to 3. The rule sets only that total. It does not choose families or cases. **Hypothetical pair-loss family ordering approved 2026-10-15 — Option A: fixed neutral family IDs in ascending order.** Each family receives a stable identifier from a documented convention that does not depend on case difficulty, anticipated EGAR effectiveness, observed scores, actual failures, or other outcomes. Illustrative identifiers are F01, F02, F03, F04, and so on. The identifiers and the mapping are assigned and frozen before final evaluation. Hypothetical loss allocation, for both spread losses and concentrated losses, follows ascending identifier order. That order is only for these planning allocations. It does not set evaluation-case execution order and does not change the approved randomized Arm A and Arm B execution order. No randomized family order and no allocation seed are used. **Family-ID assignment convention approved 2026-10-15 — Option A: alphabetical canonical names, then sequential identifiers.** Establish the canonical names of the Linux problem families, sort those names alphabetically, assign F01, F02, F03, and so on in that order, and record the mapping from each canonical name to its identifier. The names, the mapping, and the freeze happen before final evaluation. They do not depend on case difficulty, anticipated EGAR performance, observed results, actual failures, or other outcomes. No actual family name is created by this amendment. After the mapping is frozen, an identifier stays even if a display name is later corrected. A substantive change to a family definition or to membership after that freeze is a documented amendment or deviation. Both spread losses and concentrated losses use these identifiers and the approved ascending-identifier order. The actual canonical names, family definitions, and mappings are not yet established. **Hypothetical pair-loss within-family case ordering approved 2026-10-15 — Option A: stable unique case IDs in ascending order.** Each evaluation case receives a stable unique identifier from a documented procedure that does not depend on case difficulty, anticipated EGAR effectiveness, observed Arm A or Arm B scores, actual failures, EGAR activation or injection, or other outcomes. Illustrative identifiers are C001, C002, C003, and so on. The identifiers and the case mappings are frozen before final evaluation. No actual case is created by this amendment. When a hypothetical loss must be assigned to an individual case, cases inside a family are taken in ascending identifier order. Both spread losses and concentrated losses use that order. The order is only for these planning allocations. It does not determine actual missingness or leakage, evaluation-case execution order, or the randomized Arm A and Arm B order. **Case-ID assignment convention approved 2026-10-15 — Option B: sequential identifiers in documented formal registration order.** The first formally registered eligible evaluation case receives C001, the second C002, and so on. The case register keeps that sequence and the mapping from each identifier to its historical case and Linux problem family. The sequence, the identifiers, and the mappings are frozen before final evaluation. After that freeze, an identifier is not reassigned because cases are reordered for display, execution, or analysis. Registration does not depend on case difficulty, anticipated EGAR effectiveness, observed scores, actual failures or timeouts, EGAR activation or injection, or other outcomes. The sequence is documented and auditable. Cases are not reordered afterward to obtain a preferred hypothetical loss placement. No case is registered and no identifier is assigned by this amendment. Both spread losses and concentrated losses still use ascending family-ID order and ascending case-ID order. Registration order does not set the randomized Arm A and Arm B execution order, and it does not determine actual missingness or leakage. The actual case register and mappings are not yet established. **Hypothetical spread-loss allocation approved 2026-10-15 — Option A: deterministic round-robin.** This procedure defines the spread-loss model only. It does not define concentrated losses. Compute L with the approved formula L = floor(N × p + 0.5). List families in ascending frozen family-ID order and, inside each family, list cases in ascending frozen case-ID order. Starting at the lowest family ID, assign one hypothetical loss to the lowest-ID case in that family that does not already have one. Then do the same for the next family. After the last family, return to the first and repeat. Skip a family that has no remaining case available for a hypothetical loss. Stop when exactly L distinct cases have been marked. If L is 0, mark none. Do not mark the same case twice. The procedure is deterministic and uses no allocation seed. It does not determine actual missingness, leakage, eligibility, or execution order. **Hypothetical concentrated-loss allocation approved 2026-10-15 — Option A: fill one family at a time.** This algorithm defines the concentrated-loss planning model only. It uses the same L, the same frozen family-ID mapping, the same frozen case-ID mapping, ascending family-ID order, and ascending within-family case-ID order as the approved round-robin spread method. The two methods differ only in how hypothetical losses are distributed. Starting with F01, assign hypothetical losses to that family's cases in ascending case-ID order until the family has no remaining unmarked case or exactly L losses have been assigned. If losses remain, continue with the next family in ascending family-ID order and repeat. Stop when exactly L distinct cases have been marked. If L is 0, mark none. Do not mark the same case twice. The method is deterministic and uses no allocation seed. Unequal family sizes are handled by exhausting each family before moving to the next. A family may then have no contributing evaluation pair. Do not artificially keep a contributing case in every family. The algorithm does not determine actual missingness, confirmed leakage, case eligibility, execution order, or randomized arm order, and it does not assign a fabricated score. This assignment convention, the within-family case order, the family-name convention, the ascending family-identifier order, and the rounding convention may be revised only by explicit review and Steve’s approval before the final protocol is frozen. A change after that freeze follows the amendment and deviation rules. The rounded count does not change the frozen analysis population, replace a case, rerun a failed case, or assign a fabricated score of 0. How the combined loss is split between missingness and confirmed leakage also remains unresolved. Do not assume that every family still has a contributing case. Do not silently replace a missing or quarantined pair. A missing or entirely unscorable response is not scored 0. A reviewable incorrect answer may receive 0. Incomplete pairs and confirmed-leakage pairs stay out of the numerical mean. Attempted cases stay in the frozen population. There is no automatic rerun or replacement. Missingness, leakage, and exclusions stay separately reported. Crossed with the three standard deviations, the three ICCs, and the three family layouts, these rates give 81 hypothetical baseline combinations. No combination is preferred or treated as established. Runtime, reviewer workload, and the exact bootstrap parameters are not assigned here. The case-weighted paired mean stays the primary estimand. It is not replaced by an equal-weight family mean. These loss rates may be revised only by explicit review and Steve’s approval before the final protocol is frozen. This decision sets no final family count, final case count, final allocation, separate missingness rate, separate leakage rate, design effect, effective sample size, confidence interval, power calculation, required sample size, or numerical precision estimate. It does not select the bootstrap procedure. It adds no significance threshold and no success criterion. These configurations, and the standard-deviation and ICC assumptions, may be revised only by explicit review and Steve’s approval before the final protocol is frozen. This decision sets no final family count, final case count, final allocation, design effect, effective sample size, confidence interval, power calculation, required sample size, or numerical precision estimate. It does not select the bootstrap procedure. It adds no significance threshold and no success criterion. Numerical assumptions, the calculation method, and the final design need Steve’s approval before the protocol is frozen. The acquisition count stays a separate decision and is not derived from the evaluation-size calculation. The +0.5 minimum worthwhile improvement and the ±0.5 half-width remain separate planning targets. Neither is a significance gate, an automatic success rule, or a mandatory interval width. The family-cluster bootstrap, 10,000 replicates, the 95% percentile interval, the case-weighted paired mean, and the eight-family minimum are approved above. Only the numerical bootstrap seed remains unselected. The earlier proposal that the final size be a multiple of the pilot is withdrawn.

**Target precision approved 2026-10-15 — Option B: approximate ±0.5-point half-width for the final 95% family-cluster-aware paired bootstrap confidence interval, as a planning objective.** The planning aim is a 95% family-cluster-aware paired bootstrap interval whose half-width is about ±0.5 points on the combined 0–4 score. That aim is not a guarantee, a significance threshold, a mandatory interval width, or an automatic success or failure rule. The +0.5 minimum worthwhile improvement stays a separate planning target and is unchanged.

**Design tuning.** Some design numbers are judgments and tradeoffs, not uniquely correct settings. The ±0.5 half-width is the initial approved planning objective. It may be reconsidered when expected paired-score variation, family clustering, the supply of eligible historical cases, runtime, and reviewer workload are examined. A later planning change might concern the number of evaluation cases, the number or balance of Linux problem families, the number of acquisition cases, or the precision target itself. Any such change must be proposed, justified, reviewed, and approved by Steve before it enters the final frozen protocol. Planning assumptions and estimates are not observed outcomes. No parameter is silently changed because of Arm A versus Arm B results, because of an apparent EGAR effect, or in order to obtain a favorable interval. Final design parameters, analysis rules, and the relevant randomization procedures are frozen before the final evaluation begins. A change after that freeze is recorded as a protocol deviation or adopted as a formal amendment. The record states when it happened, why, and what it implies. It does not silently replace the original analysis plan. Information from the engineering pilot may inform feasibility and resource planning. The pilot is not an unacknowledged effectiveness-tuning exercise, and running it is still not authorized. The primary estimand, scoring rubric, analysis population, missingness rules, leakage rules, single-generation rule, and uncertainty method stay as approved.

No power calculation is accepted in v0.1. No *p* threshold is accepted in v0.1.

### EGAR non-injection reporting

**EGAR non-injection reporting approved 2026-10-15 — Option A: report confirmed no-injection frequency and observability gaps for Arm B as secondary descriptive observations.** This report is descriptive. It is not an effectiveness threshold, a pass or fail test, or a claim that a no-injection subgroup succeeded or failed.

Every Arm B evaluation run is placed in one of three categories. **Injection observed** means reliable evidence confirms that an EGAR experience-injection event occurred. **No injection observed** means the instrumentation is sufficient, and the relevant observations are complete, so they establish that no injection event occurred. **Unavailable or indeterminate** means the logs or other evidence cannot show whether injection occurred. A missing log is not proof that injection did not occur. No injection is not the same as no EGAR activity. Retrieval, graph-edge behavior, strategy reuse, and other experience mechanisms may occur without a recorded injection event.

Report the number of Arm B runs in each category. The confirmed no-injection proportion uses all attempted Arm B evaluation runs in the frozen population as the denominator. State that denominator. Also report the number and proportion of unavailable or indeterminate observations, with the same denominator, so a non-injection rate is not read as a complete measurement when some runs cannot be classified. Observation procedures stay the same on both arms. The measurement uses passive instrumentation and existing logs. It does not change reasoning, retrieval ranking, injection decisions, context, the internal `success_score`, or experience learning.

A no-injection run stays in the primary analysis when the paired outcome is otherwise eligible and scorable. That status is not a reason to exclude, replace, or rerun the case. Injection status is still recorded for quarantined and incomplete runs when the evidence exists. That record does not change Option A or Option B: those pairs do not enter the numerical primary mean for the reasons already approved.

### Analysis choices

**Statistical design approved 2026-10-15, except the numerical bootstrap seed.** The family-cluster bootstrap, 10,000 replicates, the 95% percentile interval, the case-weighted paired mean, and the minimum of eight contributing families are approved above. Only the numerical bootstrap seed remains unselected. The notes below record those approvals and the choices that remain open. They do not change the method.

- Primary estimand: the mean paired difference approved above, over complete scorable pairs that are not confirmed-leakage pairs under Option A.
- Uncertainty: the 95% paired bootstrap interval approved above, with family clustering accounted for in the final evaluation. Family-level resampling, 10,000 replicates, the percentile interval, the eight-family safeguard, and the stability check are approved above. The numerical bootstrap seed is selected and frozen before final evaluation and is not yet chosen. The earlier choice of an exact paired test is withdrawn. The earlier statement that family dependence was unapproved is withdrawn.
- No-injection cases stay in the primary population under the rule above. Confirmed no-injection frequency, and the unavailable category, are the descriptive report approved above. The earlier statement that this report was still an open choice is withdrawn.
- Secondary outcomes other than the non-injection report are still descriptive proposals, with the same interval method, and are not a second claim of success. The non-injection report approved above is a frequency report. It does not add an interval requirement or a success claim.
- Missing results and incomplete pairs are approved under Option B above. The earlier proposal to score a timeout or invalid output as unsuccessful, and the earlier proposal to exclude an incomplete pair without the Option B reporting and bias statement, are withdrawn. They are not rules.
- One generation per arm, and no llama.cpp generation seed, are approved above. The earlier unresolved choice between one run and repeated runs is withdrawn.
- The engineering pilot design is approved above and is not an authorization to run it. The +0.5 minimum worthwhile improvement, the approximate ±0.5 interval half-width, and scenario-based sample-size planning are approved above. Final acquisition and evaluation sample sizes remain unapproved. The proposal that the final size be a multiple of the pilot is withdrawn. Expected variability has three approved hypothetical standard deviations above. Within-family dependence has three approved hypothetical ICCs above. No SD, ICC, family-count example, or pair-loss rate is selected as the final design. The two loss-distribution models are approved planning assumptions, and neither is preferred. Hypothetical losses are placed deterministically. The integer count uses nearest-pair rounding, with exact halves upward. Family order for hypothetical losses is ascending neutral family ID. Identifiers come from alphabetical canonical names. Within-family order for hypothetical losses is ascending unique case ID. Identifiers follow documented registration order. The actual register and mappings are not yet established. The split of pair loss remains unapproved. Family-level cluster resampling, 10,000 replicates, the percentile interval, the eight-family safeguard, and the stability check are approved above. The numerical bootstrap seed and the generator implementation are not yet selected. Spread-loss placement is the approved round-robin above. Concentrated-loss placement is the approved one-family-at-a-time procedure above, and that procedure handles unequal family sizes. The earlier statement that desired precision was entirely undecided is withdrawn.
- Arm order is the balanced randomization approved above. The earlier statement that arm order was unapproved is withdrawn. The order seed is not a generation seed.

### Experimental Parameter Register — Approved 2026-10-15

This register summarizes Section 6. The detailed rules above, and the approved rules in Sections 1–5 where this section relies on them, remain the authority. If a row disagrees with those rules, the detailed rule governs and the row is corrected. An unresolved row stays unresolved until Steve approves a decision. No number below was inferred from the four-plus-four pilot.

A proposed change names the parameter, the current setting, the proposed setting, the reason, the dependencies, and the likely consequences. Before the final evaluation, a design change needs explicit review and Steve’s approval. Planning assumptions are not observed results. Final parameters, analysis rules, and the relevant randomization procedures are frozen before that evaluation. After the freeze, a change is a documented deviation or a formal amendment, with its timing and implications kept. Historical decisions are not overwritten without that record. Nothing is tuned silently because an arm scored better, because EGAR looked effective, or because a more favorable interval was wanted. The relationships below guide planning. They do not change a parameter by themselves.

**Experimental controls** protect the comparison. They are not ordinary performance knobs.

| Parameter | Current value or rule | Status | Purpose | Dependencies | Adjustment |
|-----------|----------------------|--------|---------|--------------|------------|
| Frozen analysis population | Eligible cases designated before results, then frozen | Approved | Who belongs in the experiment | Section 5 eligibility; contributing pairs | Approval before freeze; deviation or amendment after |
| Confirmed leakage | Option A: quarantine, keep in the population, exclude from the numerical mean | Approved | Integrity of the estimate | Contributing pairs; missingness | Same. Not a way to clean an unfavorable result |
| Generations | One per arm per evaluation case | Approved | No repeat generations | A missing run is not rerun | Not a tuning knob |
| llama.cpp generation seed | None | Approved | Existing generation behavior | Distinct from the order seed and the bootstrap seed | Not a tuning knob |
| Arm execution order | A→B or B→A, approximately balanced | Approved | Avoids a fixed run-order confound | Order seed; arm isolation | Schedule frozen before evaluation |
| Arm-order seed | Recorded, order only. The value is not yet assigned | Approved | Reproducible order | Arm order. Not sent to llama.cpp | Assign and freeze the value before evaluation. No value is set here |
| Arm A experience state | Fresh empty isolated store per case | Approved | Baseline without carried experience | No write-forward | Not a tuning knob |
| Arm B experience state | Fresh isolated copy of the frozen Thoth-generated snapshot per case | Approved | Experience-bearing arm | Acquisition count; snapshot | Do not edit the snapshot to improve results |
| Reviewer blinding | Blind to arm, experience store, and `success_score`. Adjudication stays as in Section 5 | Approved | Scores are not assigned with the arm in view | Section 5 reviewer procedure | Not a tuning knob |
| Engineering pilot | 4 acquisition cases, 4 paired evaluation cases, at least 2 families | Approved | Procedural check only | Not the final corpus unless a later approval allows reuse | Design approved. Execution is not authorized |

**Planning parameters** may be reconsidered only through the change-control note above.

| Parameter | Current value or rule | Status | Purpose | Dependencies | Adjustment |
|-----------|----------------------|--------|---------|--------------|------------|
| Minimum worthwhile improvement | +0.5 combined points | Approved planning target | Smallest average improvement the final evaluation should be able to estimate | Sample size, expected variation, family dependence, precision target | Explicit approval. Not a pass or fail rule |
| Target interval half-width | About ±0.5 points | Approved planning target | Initial precision aim for the 95% interval | Expected variation, clustering, evaluation count, families, runtime, reviewer workload | Explicit approval. Not a guaranteed or mandatory width |
| Sample-size planning approach | Scenario-based, Option A. Compare the approved hypothetical SD, ICC, family-layout, and pair-loss scenarios. Final sizes are not set | Approved planning method | How possible final evaluation sizes are examined | Variability, within-family dependence, family layouts, combined pair loss, runtime, reviewer workload, ±0.5 half-width | The method is approved. Each numerical assumption and the final size need explicit approval. Not an observed measurement and not a fixed sample size |
| Combined pair loss | Initial approved hypothetical scenarios: 0%, 10%, and 20%; approved 2026-10-15. None is the true, most likely, or preferred value | Approved planning assumptions | Assumed share of attempted pairs that do not enter the numerical mean, counting a pair only once if it is both incomplete and quarantined | Attempted pairs; contributing pairs; missing or unscorable pairs; confirmed-leakage pairs. The split between missingness and leakage stays unresolved. Placement across families is the next row | Revise only with explicit approval before the final freeze. Not an observed failure or leakage rate |
| Pair-loss distribution model | Two approved hypothetical models — spread losses and concentrated losses; approved 2026-10-15. Neither is the true, preferred, or most likely pattern | Approved planning assumptions. Both placement algorithms approved | How lost pairs sit across families, and how that changes the number of contributing families and the expected precision | Combined pair-loss rate; number of families; cases per family; family-size imbalance; contributing pairs and contributing families; cluster-aware uncertainty | Spread placement is the round-robin row. Concentrated placement is the sequential family-exhaustion row |
| Hypothetical pair-loss allocation method | Deterministic; approved 2026-10-15. No allocation seed | Approved planning method. Spread and concentrated placement rows are both approved | Identical planning inputs and the same rules produce the same hypothetical placement for a chosen model | Pair-loss rate; family configuration; distribution model; the rounding row; family ordering; within-family case order | Does not predict actual missingness or leakage. Revise only with explicit approval before the final freeze |
| Hypothetical combined pair-loss rounding | Nearest whole pair, exact halves upward; approved 2026-10-15. L = floor(N × p + 0.5) | Approved planning method | Turns a hypothetical loss proportion into a reproducible integer count of lost pairs | N, the attempted evaluation pair count; p, an approved hypothetical loss rate, 0.00, 0.10, or 0.20 | Same rule for spread and concentrated models. Sets the total only. Both placement algorithms are approved separately |
| Hypothetical pair-loss family ordering | Fixed neutral family IDs, ascending-ID order; approved 2026-10-15. Illustrative IDs F01, F02, F03, F04 | Approved planning method | Reproducible, outcome-independent family order for hypothetical loss allocation only | Family definitions; the ID-assignment row; pair-loss rate; rounding; eventual within-family rules | Not the Arm A/Arm B execution order. No allocation seed. Within-family order is recorded below. Both spread and concentrated placement are approved. The concentrated procedure handles unequal family sizes |
| Family-ID assignment convention | Alphabetical sorting of canonical family names, then sequential F01, F02, F03, and so on; approved 2026-10-15 | Approved planning method | Transparent identifiers for hypothetical loss allocation | Approved family definitions; canonical names; frozen name-to-ID mapping; ascending-ID loss order | Actual family definitions, canonical names, and mappings are not yet established. No names are invented here |
| Hypothetical pair-loss within-family case ordering | Stable unique case IDs in ascending order; approved 2026-10-15. Illustrative IDs C001, C002, C003 | Approved planning method. Case-ID assignment convention is the case-ID row below | Reproducible, outcome-independent order of cases inside each family | Family definitions and identifiers; the case-ID assignment row; ascending family-ID order; the spread-loss row | Planning allocations only. Not execution order and not actual missingness or leakage. Actual mappings remain unresolved. Concentrated-loss placement is the approved sequential family-exhaustion row |
| Hypothetical spread-loss placement algorithm | Deterministic round-robin across ascending family IDs, selecting the lowest-ID remaining case within each family; approved 2026-10-15 | Approved planning method | Distribute hypothetical lost pairs approximately evenly by count across families | Rounded total L; frozen family identifiers; frozen case identifiers and mappings; approved family and within-family ordering | Spread-loss placement approved. Not actual missingness or leakage |
| Hypothetical concentrated-loss placement algorithm | Deterministic sequential family exhaustion, beginning with F01, selecting cases in ascending frozen case-ID order; approved 2026-10-15 | Approved planning method | Model hypothetical pair losses concentrated within fewer Linux problem families, including possible complete loss of a family's contributing cases | Rounded hypothetical loss count L; frozen family identifiers; frozen case identifiers and mappings; approved family and within-family ordering conventions | Concentrated-loss placement approved. Spread-loss placement already approved. Not actual missingness, leakage, eligibility, or execution order |
| Case-ID assignment convention | Sequential C001, C002, C003, and so on, in documented formal registration order; approved 2026-10-15 | Approved planning method | Stable, auditable, unique case identifiers for reproducible hypothetical loss placement | Eligible evaluation-case register; documented registration sequence; frozen case-ID and family mappings; ascending within-family case-ID order | Actual case register and mappings are not yet established. No cases are registered here |
| Final acquisition case count | Undecided | Unresolved | How much experience the snapshot can hold | Arm B snapshot. Not derived from +0.5, from 4, or from the evaluation-size scenarios | Explicit approval |
| Final evaluation case count | Final choice undecided. Hypothetical examples approved 2026-10-15: 20, 40, and 60 paired cases | Approved planning examples; final choice unresolved | How many pairs might be examined | Precision target, SD scenarios, ICC scenarios, family count | Explicit approval before any example becomes the final size |
| Final number of families | Final choice undecided. Hypothetical examples approved 2026-10-15: 4, 8, and 12 | Approved planning examples; final choice unresolved | How family count might change the information available for the precision target | Five cases per family in these examples only; cluster-bootstrap reliability. Four families may be too few for a reliable interval | Explicit approval before any example becomes the final count |
| Distribution across families | Final allocation undecided. Hypothetical examples approved 2026-10-15: 5 cases in every family | Approved planning examples; final choice unresolved | Balanced sizes for the initial comparison only | Family count and total cases above. Unequal sizes remain open | Explicit approval. Balance is not required outside these examples |
| Expected paired-score variability | Initial approved hypothetical scenarios: SD = 0.75, 1.25, and 2.00 points; approved 2026-10-15. None is the true, most likely, or preferred value | Approved planning assumptions | Assumed spread of case-level paired differences, used with the ±0.5 half-width and family dependence when examining possible sample sizes | Sample-size scenarios; target precision; family dependence. Not the observed variability row | Revise only with explicit approval before the final freeze. Not a measured result |
| Expected within-family dependence | Initial approved hypothetical ICC scenarios: 0.05, 0.15, and 0.30; approved 2026-10-15. None is the true, most likely, or preferred value | Approved planning assumptions | Assumed correlation among case-level paired differences within a family. Crossed with the three SD scenarios, this gives nine planning combinations | Number of families; cases per family; family-size imbalance; effective information for the precision target; final evaluation sample-size planning. Not an observed correlation | Revise only with explicit approval before the final freeze. Not a measured result |

**Analysis specifications** must be fully defined and frozen before the final evaluation. The family-cluster bootstrap, 10,000 replicates, the 95% percentile interval, the case-weighted paired mean, and the eight-family minimum are approved. Only the numerical bootstrap seed remains unselected. Rows marked unresolved stay unresolved.

| Parameter | Current value or rule | Status | Purpose | Dependencies | Adjustment |
|-----------|----------------------|--------|---------|--------------|------------|
| Primary estimand | Case-weighted mean of Arm B minus Arm A combined scores on complete, scorable, non-quarantined pairs | Approved | The comparison | Population, Option B, Option A | Not an average of family means. Not retuned after results |
| External score | 0–4, diagnosis 0–2 plus remedy or next action 0–2. Not written into `success_score` | Approved | What the estimand uses | Component reports. Rubric text is in Section 5 | Not a tuning knob |
| Confidence level | 95% | Approved | Interval level | Bootstrap | Not a significance gate |
| Uncertainty method | Paired family-cluster bootstrap for the case-weighted mean | Approved | Uncertainty for the case-weighted mean | Contributing pairs; frozen family map; the approved resampling, replicate-count, and percentile-interval rows below | The numerical bootstrap seed is not selected |
| Missingness | Option B: timeout, crash, or unscorable output is missing, not a score of 0 | Approved | Separates operational failure from a wrong answer | Incomplete pairs stay out of the mean | Not retuned |
| Injection categories | Injection observed; no injection observed; unavailable or indeterminate | Approved | How Arm B injection evidence is classified | Denominator is all attempted Arm B runs in the frozen population | A missing log is not “no injection” |
| Bootstrap resampling steps | Family-level cluster bootstrap, Option B; approved 2026-10-15. Each replicate draws as many contributing families, with replacement, as there are distinct contributing families, and keeps each selected family's contributing pairs together | Approved analysis method | How the paired mean is resampled for the 95% interval | Frozen population; missingness and leakage rules; frozen family identifiers; contributing pairs | Not a two-stage within-family resample. Does not resample the two arms independently |
| Interval construction | Percentile interval: 2.5th and 97.5th percentiles of the 10,000 replicate statistics; approved 2026-10-15 | Approved analysis method | How the 95% bounds are computed | The 10,000 family-level replicate statistics | Stability is the separate row below. Not a required half-width of ±0.5 |
| Bootstrap resample count | 10,000; approved 2026-10-15 | Approved analysis method | How many resamples | Interval construction; bootstrap seed; the stability split at 5,000 | Do not add replicates after seeing the interval |
| Bootstrap random seed | A fixed documented 64-bit seed, selected and frozen before final evaluation. The numerical value is not yet selected | Approved rule; numerical value not yet selected | Reproducible resamples | Independent of the arm-order seed. Not a llama.cpp generation seed. Generator and sampling convention still to be documented | No numerical value is set here |
| Sparse-family safeguard | At least eight distinct families, each with at least one contributing pair, before a confirmatory 95% interval is reported; approved 2026-10-15 | Approved reporting safeguard | What happens when fewer than eight families contribute | Contributing families and pairs. Small (4) cannot meet it. Medium (8) meets it only if every family still contributes. Large may meet it | Operational minimum, not a validity guarantee. If unmet, report the mean and descriptive results without a confirmatory interval. Do not substitute another interval method |
| Bootstrap interval-stability check | Compare percentile endpoints from the first 5,000 replicates with those from all 10,000. Flag the interval if either endpoint differs by more than 0.1 combined-score points; approved 2026-10-15 | Approved numerical-reproducibility check | Whether the percentile endpoints moved when the second half of the replicates was included | The ordered 10,000 replicate statistics | Not a test of statistical validity. Report the discrepancy. Do not change the seed or the replicate count to obtain a favorable interval |
| Other secondary intervals | Undecided | Unresolved | Which descriptions besides the primary mean get an interval | The non-injection report does not require one | Explicit approval |

**Observed measurements** are recorded. They are not settings.

| Parameter | Current value or rule | Status | Purpose | Dependencies | Adjustment |
|-----------|----------------------|--------|---------|--------------|------------|
| Non-injection frequency | Not yet observed | Observation-only | Confirmed Arm B runs with no injection event | Same denominator as the unavailable proportion. No-injection cases stay eligible when otherwise scorable | Recorded, not tuned |
| Observed paired-score variability | Not yet observed | Observation-only | Variation the runs actually produce | Distinct from expected variability | Recorded, not tuned |
| Observed paired mean and interval | Not yet observed | Observation-only | The result and its uncertainty | Contributing pairs only | Reported whether or not it meets +0.5 or ±0.5 |

### Nonbinding Alternative Methods Register

This register is not the Experimental Parameter Register. An entry here is not an approved EGAR-LX v0.1 method. Recording it does not authorize its use and does not change an approved parameter. Adopting one later requires Steve’s explicit approval and a protocol review. A later comparison of an alternative does not retroactively change the frozen v0.1 analysis.

**Alternative: Proportional spread-loss allocation (Option B).** Allocate hypothetical losses approximately in proportion to each family’s share of attempted evaluation cases. Use a deterministic largest-remainder allocation, and break ties by ascending family ID. Inside a family, select cases by ascending frozen case ID. A possible later use is planning with unequal family sizes, or a comparison of how sensitive results are to the allocation rule. Status: preserved for future consideration only. Not approved as the EGAR-LX v0.1 spread-loss method.

**Alternative: Proportional concentrated-loss allocation (Option B).** Select a predetermined subset of Linux problem families and allocate hypothetical losses among those families approximately in proportion to their case counts, using deterministic allocation and tie-breaking rules to be defined if the alternative is ever considered for adoption. A possible later use is a sensitivity analysis with unequal family sizes, or an alternative assumption about clustered missingness and leakage. Status: preserved for future consideration only. Not approved for EGAR-LX v0.1. Adopting it would require Steve’s explicit approval and a protocol review. Neither this alternative nor the proportional spread-loss alternative is authorized by being recorded here.

**Alternative: Two-stage cluster bootstrap (Option A).** Resample Linux problem families with replacement, then resample contributing evaluation pairs within each selected family. A possible later use is a separately approved sensitivity analysis or a future experimental design. Status: preserved for consideration only. Not approved for EGAR-LX v0.1. Adopting it would require Steve’s explicit approval and a protocol review. Recording it does not authorize its use and does not change the approved family-level bootstrap.

---

## 7. Environment isolation and reproducibility

**Section status:** Approved for protocol design, 2026-10-15. This approval does not create a workspace, initialize a database, implement telemetry, or authorize a pilot.

### Forbidden environments

**APPROVED, and required by existing locks.**

| Resource | Rule |
|----------|------|
| C6-LIVE (`thoth-c6`, port 8091, its volumes) | Do not start, mount, read, or write. Do not open a C6.4 window |
| C6.4 window file and `THOTH_C64_CURRENT_FINGERPRINT` | Unset |
| Development workspace `agent_workspace/` and the DEV Compose project `thoth` | Not the experiment store and not the corpus destination |
| Phase E artifacts under `docs/baselines/` | Do not edit or reuse as a starting database |
| MTCP TOKEN-CHAR state | Do not reuse |
| EGAR-LAB (`thoth-egar`, port 8092) | Do not treat the existing volume as this experiment. Scripts that bring it up do not create an EGAR starting state. Using it anyway would mix an undefined volume with this protocol |

**APPROVED location.** A new disposable workspace, or a new Compose project name that is neither `thoth`, `thoth-c6`, nor `thoth-egar`. Creating that project is a later implementation step. This approval does not create it.

### Snapshot and case copies

**APPROVED.** The hashed acquisition snapshot is immutable during evaluation. Every Arm B evaluation case receives a fresh disposable copy of that snapshot. Every Arm A case receives a fresh empty experience database. Writes made for one case stay in that case database. They do not modify the frozen snapshot and they do not modify another case.

### Records each future run must keep

**APPROVED manifest fields.** The manifest names the frozen snapshot and each case copy separately. It does not require a disposable case database to stay unchanged while that case runs. The snapshot hash must match before and after evaluation. A case copy may change during its own run.

- Protocol version and rubric version.
- Product SHA, engine SHA, and whether that worktree was dirty. Engine image digest if Docker is used.
- Chat and embedding artifact hashes, context lengths, and backend name.
- Configuration hashes, including timeout, reflection ceiling, and retrieval-weight file.
- Database schema version when the opened database exposes one. If it does not, record that the version is not exposed.
- Corpus manifest: source URL, retrieval timestamp, cutoff, visible-file hashes, sealed-file hashes stored outside the index.
- Frozen snapshot identity and hash, recorded after acquisition and again after the last evaluation case.
- For each case: arm, case id, workspace path, run id, and case-copy identity, distinct from the snapshot hash.
- Experience injection and reuse: whether strategy, trajectory, or plan reuse was injected, and whether `experience_dropped` was true.
- Retrieval-set count and terminal plan status, where those are observable.
- Graph reinforcement outcome: reward, penalty, no edge, or indeterminate.

An observed absence is a recorded event that the thing did not happen. Missing or insufficient telemetry is indeterminate. It is not recorded as an absence. Structured graph-reinforcement logging does not exist yet. Until an implementation records reward, penalty, or no edge, the graph outcome in the manifest is indeterminate. That logging is an implementation prerequisite for a manifest that can distinguish those three outcomes. This approval does not implement it.

E1 environment rows remain the existing harness mechanism. This approval does not extend them. A later implementation may emit them if the owner asks for that wiring.

---

## 8. Execution procedure

**Section status:** Approved for protocol design, 2026-10-15. This section states the required order. It does not authorize any stage. Approving the engineering-pilot design does not authorize running the pilot. Completing a pilot, if one is later run, does not authorize acquisition. Completing acquisition does not authorize evaluation. Each of those transitions requires review of the evidence from the previous stage and a separate explicit authorization from Steve.

Open rows in §10 block the stage that depends on them. They do not block protocol-design review of a section that does not use them. A stage does not start until the controls that stage needs are decided. This section does not decide the remaining §10 rows.

### Order

1. Protocol-design review of Sections 1–12 is complete. Resolving an open §10 row is required only before the stage that row governs.
2. Before any collection, resolve the §10 rows that block collection, including source choice and the empty-database row-count inspection. Then select and validate candidate cases only under a separate collection authorization. Apply the cutoff test. Do not index sealed bundles.
3. Freeze evaluation case ids, visible bytes, sealed bytes, family mappings, and the scoring rules before evaluation. Acquisition cases stay a distinct set.
4. Verify isolation before any run: C6 window closed, development and sealed baselines untouched, and the experiment workspace free of foreign experience. Record the result. A missing check is indeterminate. It is not a passed check.
5. Run the engineering pilot only after Steve explicitly authorizes pilot execution. The pilot remains a procedural check. It is not an effectiveness result.
6. Review the pilot evidence: leakage, injection logs, arm mismatches, tool or network use, and unusable scores. Absence of injection is a valid observation under §2. It is not by itself a failed pilot. Do not tune weights, prompts, or thresholds to raise a score. This review does not authorize acquisition.
7. Freeze the protocol text and the corpus manifest that will govern later stages only after the evidence those freezes depend on has been reviewed. Final sample size and acquisition count stay unselected until Steve approves them.
8. Build the acquisition store only after Steve explicitly authorizes acquisition. Hash the snapshot and record that hash. Do not start evaluation in the same authorization.
9. Run paired evaluation only after Steve explicitly authorizes it. Each Arm A case starts from a fresh empty experience database. Each Arm B case starts from a fresh disposable copy of the frozen snapshot. The snapshot file is not opened for writing.
10. Score under blinding, compute the pre-registered estimate, and archive the workspace, logs, manifests, and hashes.

### Stop conditions

Stop and do not continue to the next stage when a listed violation is confirmed. Missing or insufficient telemetry is indeterminate. It is not treated as a successful control check, and it is not treated as proof that the violation occurred. Indeterminate evidence pauses the next stage until the record is sufficient or Steve accepts the gap in writing.

Confirmed stop conditions:

- A visible bundle contains post-cutoff resolution text, or a sealed answer is delivered into a run.
- An acquisition case and an evaluation case are the same bug or a near-duplicate.
- Arm A’s experience database is not empty of trajectories, strategies, past plans, or graph edges at the start of that case.
- The frozen acquisition snapshot’s hash changes during evaluation, or a case writes into the snapshot, into another case’s database, or into the other arm’s database.
- The two arms differ in model hash, corpus fingerprint, retrieval weights, or code SHA.
- A run executes a live shell command, reads the host as a substitute for the historical system, or uses the network to fetch case evidence. Recommended commands that remain unexecuted text are not this violation.
- A C6.4 window file is created or a C6 volume is mounted.
- Reviewers cannot apply the approved rubric to the pilot answers. That pause is about the rubric’s use. It does not discard the approved scoring rules.

These are not stop conditions:

- Writes inside the isolated database of the case that is running.
- Arm B showing no strategy, trajectory, or plan-reuse injection. That absence stays a valid observation when the logs show that nothing qualified or that the experience block was dropped. If the logs are missing, the injection result is indeterminate.

### Evidence preservation

Keep raw logs, both answers, reviewer sheets, the sealed key, the snapshot hash, and each case-copy identity. Do not overwrite them with a summary. Do not commit model transcripts that include sealed text into a public branch unless Steve approves a redaction procedure. Hash verification of the frozen snapshot is required before evaluation starts and again after the last evaluation case. The case copies are not required to keep that same hash.

---

## 9. Threats to validity

**Section status:** Approved for protocol design, 2026-10-15. This approval does not authorize collection, a pilot, acquisition, or evaluation. A control below is a design rule. It is not a claim that the control has been implemented or that a run has verified it.

| Threat | Why it matters here | Control |
|--------|---------------------|---------|
| Answer leakage | Later mail and commits state the fix | Cutoff split; sealed bundle unindexed; leakage audit in §5. A correct diagnosis alone is not evidence of leakage |
| Pretraining familiarity | The model may already have seen related material | Flag familiarity before evaluation. Keep otherwise eligible flagged cases in the primary analysis. Report familiarity as a limitation and, when useful, as a descriptive subgroup. Do not drop a case only because of that flag |
| Unfair arms | A hidden flag or a different corpus | Identical pin; only the experience store differs; manifest compare before scoring |
| Store contamination | Evaluation writes must not enter the frozen snapshot or another case | The hashed acquisition snapshot stays unchanged through evaluation. Each Arm B case uses a fresh copy. Each Arm A case uses a fresh empty database. Writes inside the correct case database are allowed. A write across arms or across cases is a validity violation |
| Repeated-case memorization | Same bug in acquisition and evaluation | Distinct ids; an evaluation case must not be the same issue or a near-duplicate |
| Small samples | A few pairs move the mean | No approved final *n*; the approved interval rules; pilot separate from the final evaluation |
| Nondeterminism | Generation is not seed-controlled | One generation per arm per evaluation case. No repeated-generation requirement. Arm order is a reproducible randomization. That seed does not control model generation. Record generation settings and the model-call metadata that exist. Residual sampler variation is a limitation |
| Weak scoring | Patch-id match rewards leakage | Rubric in §5. A patch-text match is not sufficient. External grades are not written into `success_score` |
| EGAR, GRAG, and Cognate confounding | All three run on both arms | Same corpus and same controller. Injection logs show whether EGAR content was present. Mechanical success, graph outcome, injection, and the external grade are reported as separate facts |
| Graph and warm memory | Two or more retrieval sets can reinforce edges after P0–R2. Warm tiers do not gate retrieval | D2 interpretation, 2026-10-15. Edges that are written stay in the Arm B snapshot. One retrieval set writes no edge. Warm-tier behavior remains the §2 verification |
| Thin experience payload | Injected trajectory omits the diagnosis | D2 Option 1 approved. The compressed payload stays as implemented |
| Hardware and timeouts | A slow arm hits the 900-second budget | Same host pin. A timeout is missing under the §6 rule, not a fabricated score of 0 |
| Strategy threshold | Promotion needs three successes at 0.8 | Report non-promotion. Do not retune after outcomes |
| C6 contamination | Official longitudinal evidence must stay observational | Forbidden environments in §7 |

---

## 10. Open decisions and approval gates

**Section status:** Approved for protocol design, 2026-10-15, as a register of decisions already made and gates that remain unresolved. Approval of this register does not approve its open decisions, select their values, or mark any implementation prerequisite complete. It does not authorize implementation, corpus collection, a pilot, acquisition, or evaluation.

Sections 1–12 are approved for protocol design. No experimental baseline is frozen. No sample size or acquisition count is selected. No corpus has been collected. No pilot or A/B experiment is authorized. An open row blocks the earliest stage named for that row. It does not block documentation review of a later section that does not use it.

### Closed decisions

These rows record decisions already approved in Sections 1–9. This register does not reopen them.

| Id | Decision | Status |
|----|----------|--------|
| D1 | Sections 1–12 accepted for protocol design, 2026-10-15 | Decided for the register. Open rows in this section stay open. Approval of the register does not approve those open rows |
| D2 | Option 1 approved, including the 2026-10-15 graph interpretation. No second option | Decided. See §2 |
| D3 | External grades are not written into `success_score` | Decided in §4 and §6. Not a manual database edit |
| D4 | Graph edges that acquisition writes travel in the Arm B snapshot. Arm A starts without them. Warm tiers do not gate retrieval | Decided by the D2 interpretation and §2. One retrieval set writes no edge |
| D5 | Primary score is the external combined 0–4 | Decided in §5 and §6 |
| D6 | Timeouts and unscorable outputs are missing, not a fabricated 0 | Decided in §6, Option B |
| D7 | Pilot design is four plus four | Decided in §6. The pilot size is not the final evaluation size and not the acquisition count |
| D8 | One generation per arm. No llama.cpp generation seed. Arm order uses a separate reproducible seed | Decided in §6. The numerical arm-order seed is not selected in this register. It is frozen with the execution schedule before final evaluation, and it is not a generation seed |
| D9 | Primary uncertainty is the 95% family-cluster bootstrap. Reviewer procedure is the §5 rule. Replicate count, percentile interval, and eight-family minimum are decided | Decided in §5 and §6. The numerical bootstrap seed remains open below |

### Open decisions

No value below is selected by this approval.

| Id | Still unresolved | Earliest stage that requires it | Later requirement |
|----|------------------|---------------------------------|-------------------|
| D10 | Historical Linux case-source selection. The §5 cutoff and fixed-evidence rules are already approved. The sources are not chosen | Corpus collection and case selection | Provenance and the acquisition/evaluation separation are checked again at the engineering pilot, experience acquisition, and evaluation |
| D11 | Experimental workspace or Compose project name, distinct from DEV, C6-LIVE, and EGAR-LAB. Isolation rules are approved in §7. The name is not chosen | Implementation and environment preparation | The named workspace is verified before the engineering pilot, acquisition, and evaluation. Creating it is not authorized by this register |
| D12 | Whether a harness may generate E1 evidence rows. §7 does not extend E1 rows | Implementation and environment preparation, and only for a harness change that would emit those rows | Not required for documentation review, collection, or the current manifest. Evaluation does not depend on E1 rows unless a later implementation is separately authorized to emit them |
| D13 | Transcript redaction and retention rules if transcripts are stored in git | None of the experimental stages must stop for this decision. It is required before any commit of transcripts | The engineering pilot is the first stage that can produce transcripts. Local evidence preservation under §8 may proceed, if that stage is later authorized, without committing those transcripts |
| — | Final evaluation sample size and family layout. The Small, Medium, and Large examples are not a selection | Evaluation and scoring | The confirmatory analysis uses that frozen layout. The engineering pilot and the acquisition count do not require it |
| — | Acquisition count and acquisition-family coverage. The pilot’s four acquisition cases are a separate approved design | Experience acquisition | The frozen snapshot from that acquisition is the store copied for Arm B at evaluation. The count is not derived from the evaluation size |
| — | Bootstrap random seed, a fixed 64-bit value independent of the arm-order seed and not a llama.cpp seed. The generator and sampling convention are documented with it | Evaluation and scoring. §6 requires the seed to be selected and frozen before final evaluation | Confirmatory statistical analysis uses that frozen seed and does not change it after results are seen |

### Implementation prerequisites

Each item is unresolved. None is complete. This register does not treat a design rule as evidence that the check has been performed or that the mechanism exists.

| Prerequisite | Earliest stage | Later verification |
|--------------|----------------|--------------------|
| Verify that a newly initialized experience database is free of experience-bearing state, including trajectories, strategies, past plans, and graph edges | Implementation and environment preparation | Again before the engineering pilot, the first stage that opens an experience database, and before acquisition and evaluation. Arm A emptiness remains a §8 stop condition at each case |
| Implement or otherwise establish an enforceable tool-execution boundary for fixed-evidence runs, so a case cannot execute a live shell command or fetch case evidence over the network | Implementation and environment preparation | The boundary must hold at the engineering pilot, acquisition, and evaluation. The §5 prohibition and the §8 stop condition are design rules. They are not evidence that the boundary exists |
| Make graph reinforcement auditable enough to distinguish reward, penalty, no edge, and indeterminate | Implementation and environment preparation | Until that record exists, a manifest marks the graph outcome indeterminate. That mark is not a pass. The distinction is needed at the engineering pilot and again at acquisition and evaluation |
| Establish the isolated experimental workspace and the frozen-snapshot procedure | Implementation and environment preparation for the workspace. The name is D11 and is not chosen | The engineering pilot is the first run that would create and check a snapshot. Acquisition freezes and hashes the evaluation snapshot. Evaluation checks that hash before the first case and after the last case |
| Preserve acquisition/evaluation separation and case provenance | Corpus collection and case selection, where provenance begins | The same separation is a stop condition if an acquisition case and an evaluation case are the same bug or a near-duplicate. No cases have been collected |
| Verify arm parity and experience-store isolation before execution | Engineering pilot, the first execution stage | Repeated before acquisition and before evaluation. A missing check is indeterminate. It is not a passed check |

### Collection questions

1. The injected payload carries a step pattern, a prior goal string, and a mechanical score. It does not ordinarily carry the previous diagnosis. That limit is accepted under D2 Option 1. It is not a reason to change the payload. This question is closed.
2. Does empty-schema initialization actually leave every experience path the planner and GRAG consult empty? The inspection is the first implementation prerequisite above. It remains open. It is not a source-selection decision.
3. Which historical sources can be split on a cutoff without including the fix in the visible bundle? This is D10. It remains open and blocks corpus collection and case selection.

---

## 11. Evidence labels

**Section status:** Approved for protocol design, 2026-10-15, as an evidence register. This approval does not authorize implementation, corpus collection, a pilot, acquisition, or evaluation. A verified code row is not evidence that a Linux diagnosis was correct, and it is not evidence that graph reinforcement improves diagnostic accuracy.

The first inspection of this table was the parent working tree on 2026-10-14. Those rows are retained below as historical where the observation still holds. Line numbers in this section were checked against the current tree on 2026-10-15, after the P0 and R1–R3 repairs. A 2026-10-14 line number that no longer matches is recorded as historical and is not used as the current citation.

### Current source behavior

| Claim | Label | Support |
|-------|-------|---------|
| Cognate sets terminal plan status before history is stored. A non-empty plan whose steps all succeeded becomes `COMPLETED`. Otherwise it becomes `FAILED`. Abort sets `ABORTED` and does not store plan history | VERIFIED | `external/basic_agent/src/executive_controller.cpp` lines 555–560 and 986–995. `store_plan_history` is called at line 995, after the status assignment |
| A completed plan is not a correct diagnosis. `calculate_trajectory_score` starts at 1.0 when the plan completed, subtracts 0.1 for each recorded step with a non-empty error, adds 0.1 when completion followed a revision, and clamps the result to [0, 1]. No external diagnostic grade enters that score | VERIFIED | `executive_controller.cpp` lines 1464–1479. The separation from the blinded 0–4 rubric is the approved design in §4 and §6 |
| GRAG retrieval diagnostics are attached to the retrieval step result, and the controller reads that `diagnostics` object when it builds a retrieval set | VERIFIED | `external/basic_agent/src/workflow_engine.cpp` lines 583–634; `executive_controller.cpp` lines 1182–1206 |
| Graph reinforcement runs only when at least two retrieval sets were captured. One set writes no edge. The graph score passed to the refiner is 1.0 when plan status is `COMPLETED` and 0.0 otherwise | VERIFIED | `executive_controller.cpp` lines 1398–1449. The call at line 1724 performs that update. A later call at lines 1751–1752 runs only when the mechanical score is above 0, after the sets have already been cleared, so it does not reinforce a second time |
| The refiner treats a graph score at or above 0.8 as a reward and any lower score as a penalty. Because the controller passes 1.0 or 0.0, mechanical completion rewards and mechanical failure penalizes. This is not a test of diagnostic accuracy | VERIFIED | `external/basic_agent/src/graph_refiner.cpp` lines 17–39 |
| Retrieval sets are cleared at goal start, on abort, when fewer than two sets are present, and after reinforcement | VERIFIED | `executive_controller.cpp` lines 469, 560, 1401–1402, and 1449 |
| Graph-edge success and failure counts are stored as the absolute values passed in, not added again to the stored counters | VERIFIED | `external/basic_agent/src/sqlite_memory_repository.cpp` lines 615–622 |
| Strategy promotion requires at least three stored trajectories, and a repeated pattern of at least two steps whose own count is at least 3 and whose mean mechanical `success_score` is at least 0.8 | VERIFIED | `external/basic_agent/src/strategy_engine.cpp` lines 26–30 and 66–84. The 2026-10-14 citation of lines 26–30 and 83–84 still covers the total-trajectory gate and the promotion test |
| Strategy injection keeps one strategy, and only when cosine similarity is at least 0.40 | VERIFIED | `external/basic_agent/include/planner_injection_config.h` lines 32–33; `external/basic_agent/src/llm_planner.cpp` lines 28–55 and 193–206 |
| Trajectory injection text is the prior goal plus its mechanical success score. At most one trajectory is injected, and the similarity floor is 0.55 | VERIFIED | `llm_planner.cpp` lines 209–222; `external/basic_agent/include/plan_reuse_config.h` lines 35–39; `planner_injection_config.h` line 36; the floor is applied in `external/basic_agent/src/memory.cpp` lines 217–219 |
| Past-plan injection uses success floor 0.6, similarity floor 0.55, and a retrieve limit of 1. The injected outline is a step-type summary, `TYPE(step_id)`, not the full plan and not the diagnosis | VERIFIED | `plan_reuse_config.h` lines 20–31; `memory.cpp` lines 725–753; `executive_controller.cpp` lines 1941–1950; `external/basic_agent/include/goal_text_utils.h` lines 79–107 |
| The 500-character outline cap is the log preview, not the injected outline. The 2026-10-14 row cited `executive_controller.cpp` lines 1902–1903 for an injection cap. Those lines are now the cognitive-metrics log. Current truncation of the preview is lines 1920–1922 | VERIFIED current behavior. The 2026-10-14 injection-cap citation is HISTORICAL and is not the current injection path | `plan_reuse_config.h` line 31; `executive_controller.cpp` lines 1920–1922 and 1933 |
| No EGAR disable flag was found. Injection is skipped when no strategy, trajectory, or past plan qualifies | VERIFIED | Search of `external/basic_agent` headers and sources on 2026-10-15 found no disable flag. The empty path is `llm_planner.cpp` lines 190–233 |
| The default text-generation budget is 900 seconds, overridable by `THOTH_LLM_TIMEOUT_SECONDS` | VERIFIED | `external/basic_agent/include/llm_timeout_policy.h` lines 18–36 |
| The reflection ceiling default is 2, and the reflection score threshold is 0.6 | VERIFIED | `plan_reuse_config.h` lines 52–59 |
| Planner validation is called with tool steps disallowed. A `TOOL` step is rejected. `WorkflowEngine::executeTool` sets `confirmed` true when that field is absent, so a tool step that did run would pass the confirmation check inside the registry | VERIFIED | `llm_planner.cpp` line 276; `external/basic_agent/src/plan_validator.cpp` lines 70–84; `workflow_engine.cpp` lines 412–424; `external/basic_agent/src/tools.cpp` lines 60–68 |
| Shell-backed tools are denied unless `allow_shell_exec` is true. That flag defaults to false. `web_scrape` does not require confirmation and is reachable from the chat dispatcher | VERIFIED current boundary. An experiment-wide tool interlock is UNIMPLEMENTED | `external/basic_agent/include/config.h` line 43; `external/basic_agent/src/run_tests_tool.cpp` line 79; `external/basic_agent/src/code_modify_tool.cpp` line 119; `external/basic_agent/src/web_scrape_tool.cpp` lines 25–26; `external/basic_agent/src/command_processor.cpp` lines 1073–1118 |
| Structured graph-reinforcement logging that can distinguish reward, penalty, no edge, and indeterminate is not present. The refiner writes a console line | UNIMPLEMENTED | `graph_refiner.cpp` lines 59–64. Absence of that line is not a recorded no-edge result |
| A fresh experience database has not been inspected for leftover experience-bearing rows. No isolated EGAR-LX workspace and no frozen acquisition snapshot exist | UNIMPLEMENTED | §10 prerequisites. This register does not create them |
| Final evaluation size, acquisition count, and the numerical bootstrap seed are not selected | UNRESOLVED | §6 and §10. The bootstrap method itself is approved design |

### Historical observations retained from 2026-10-14

These observations were recorded on 2026-10-14 and checked again on 2026-10-15. They distinguish C6.4, E2, B1, MTCP, and EGAR-LAB from EGAR-LX. They are not restated as if they were first found after the architectural repairs.

| Claim | Label | Support |
|-------|-------|---------|
| No Linux bug-and-patch corpus file was found. MTCP only names a sealed EGAR Linux troubleshooting corpus | HISTORICAL 2026-10-14, re-checked 2026-10-15 | A repository search on 2026-10-15 finds that name in `docs/MAX_TOKEN_CHARACTERIZATION_PROTOCOL.md` lines 164–165 and in this protocol. It does not find a corpus file |
| C6.4 is an observational study of ordinary goals and has no artificial 30-item battery | HISTORICAL 2026-10-14, re-checked 2026-10-15 | `docs/C6_phase4_protocol.md` lines 28–30 and 120–128 |
| C6.4 forbids causal claims about EGAR. A corpus-fingerprint change is a material change and terminates an open official window | HISTORICAL 2026-10-14, re-checked 2026-10-15 | same file, lines 64–70 and 276–292 |
| C6.4 compares windows on a 28-day span. That comparison is not EGAR-LX | HISTORICAL 2026-10-14, re-checked 2026-10-15 | same file, lines 194, 215, and 472. The 2026-10-14 citation of lines 184–215 covered the membership clock and the planned 28-day end. Line 472 states the two-window comparison |
| C3 “A/B” is a two-case mock reflection fixture, not an experience-store experiment | HISTORICAL 2026-10-14, re-checked 2026-10-15 | `external/basic_agent/src/reflection_ab_cases.cpp` lines 13–31 |
| E2 is three synthetic facts, not Linux cases. C6.4 records Phase E `mean_episodic_lift = 0.0` for `n=3_strict_trio` | HISTORICAL 2026-10-14, re-checked 2026-10-15 | `external/basic_agent/src/episodic_learning_cases.cpp` lines 40–68; `docs/C6_phase4_protocol.md` line 46 |
| The GRAG research corpus is five papers. B1 suite `b1_v1` is a candidate and is not owner-frozen | HISTORICAL 2026-10-14, re-checked 2026-10-15 | `docs/B1_PROTOCOL.md` lines 1–5 and 74–86 |
| EGAR-LAB is an isolated Compose project. The isolated-up script does not ingest a corpus, and the README states that these scripts do not create EGAR S0 | HISTORICAL 2026-10-14, re-checked 2026-10-15 | `docker/README.md` lines 136–179; `docker/isolated-up.sh` lines 1–3 |
| MTCP forbids opening a C6 window and forbids using C6-LIVE for that characterization. C6.4’s own window is a prospective open record of ordinary goals, not this experiment | HISTORICAL 2026-10-14, re-checked 2026-10-15 | `docs/MAX_TOKEN_CHARACTERIZATION_PROTOCOL.md` lines 127–128; `docs/C6_phase4_protocol.md` lines 120–128 and 200–215. The 2026-10-14 row cited C6.4 lines 200–202 as making an unrelated protocol a violation of the open-record rule. Those lines require C6.4’s own open record. They do not by themselves state the EGAR-LX rule |
| `docs/EGAR.md` describes richer episode contents than the planner injects, and it names A/B comparisons as a design aim | HISTORICAL 2026-10-14, re-checked 2026-10-15, as design text | `docs/EGAR.md` lines 78–87 and 200–214. The current injection rows above are the implementation |

### Approved design, not code verification

| Claim | Label | Support |
|-------|-------|---------|
| Sections 1–12 are approved for protocol design. Final evaluation size and acquisition count are not selected. No baseline is frozen and no execution is authorized | APPROVED DESIGN | This document, 2026-10-15 |
| Blinded external grades are not mapped into `success_score`. Graph behavior is the D2 interpretation. No llama.cpp generation seed is used | APPROVED DESIGN | §2, §4, and §6. The numerical bootstrap seed remains UNRESOLVED |

### Conflicts deliberately left in place

- This draft does not edit C6.4, E2, B1, or MTCP.
- Using this corpus inside an official C6.4 window would violate C6.4’s population rule. The experiments stay on separate workspaces.
- Implementing a richer experience prompt, a disable flag, structured graph logging, or a tool interlock would be a later code change under the repository approval gate. It is not part of this evidence approval.

---

## 12. Status

**Section status:** Approved for protocol design, 2026-10-15. This section records the design status. It does not freeze a baseline, select a sample size, or authorize implementation or execution.

EGAR-LX v0.1 remains **DRAFT**. Sections 1–12 are approved for protocol design. No experimental baseline is frozen. No final evaluation sample size or acquisition count is selected. No corpus has been collected under this protocol. No engineering pilot, acquisition, or evaluation has been authorized. No C6.4 experimental window has been opened.

Approval of all twelve design sections does not authorize implementation or execution. Open decisions in §10 stay open. Unimplemented prerequisites stay unimplemented. The Future Research register after this section is not part of EGAR-LX v0.1 and is not an experimental requirement.

---

## Future Research and Follow-Up Experiments

This register records ideas that came up while reviewing EGAR-LX v0.1. None of them is approved. None is required before v0.1 can be completed. Not every item has to be run. No sample size, effect size, threshold, significance rule, or schedule is set here. A later protocol would have to be written and approved before any of these experiments could start.

Sentences already in Sections 5 and 6 that name later retrieval, live execution, repeated generations, or generation seeds are left where they are. This register is the fuller record of those ideas.

### F1 — Repeated-generation reliability

**Status:** Future proposal — not approved.

**Research question.** Is an observed EGAR advantage consistent across repeated generations, or does it vary substantially because of language-model sampling?

**Why it may be valuable.** EGAR-LX v0.1 uses one generation per arm, so it cannot measure that within-case variation.

**Design dependencies.** The same evaluation cases, run more than once on each arm, with fresh isolated experience state for every run. Possible measurements include score variance, repeatability, and the distribution of paired differences. How those repeats would be summarized is not decided here.

### F2 — Controlled generation-seed experiment

**Status:** Future proposal — not approved.

**Research question.** How much of the observed difference depends on model generation randomness?

**Why it may be valuable.** v0.1 does not send a llama.cpp generation seed. A later experiment could ask whether setting seeds makes runs more reproducible and the comparison more controlled.

**Design dependencies.** An explicit seed policy for llama.cpp, and a comparison that still allows prompts, retrieved context, and execution conditions to differ. Identical seeds are not assumed to produce identical outputs when those conditions differ.

### F3 — Interactive historical evidence retrieval

**Status:** Future proposal — not approved.

**Research question.** Does EGAR experience improve Thoth’s choice of investigative actions and its ability to gather useful evidence?

**Why it may be valuable.** v0.1 gives Thoth a fixed pre-cutoff bundle and does not let it request more case evidence during the run.

**Design dependencies.** A deterministic evidence-provider interface limited to pre-cutoff historical evidence, plus provenance controls, leakage protections, and a separate validation of that interface. This is not an EGAR-LX v0.1 arm.

### F4 — Live Linux diagnostic execution

**Status:** Future proposal — not approved.

**Research question.** Does EGAR experience improve autonomous troubleshooting when Thoth can execute diagnostic commands and observe their results?

**Why it may be valuable.** v0.1 allows recommended commands in the written answer and does not execute them.

**Design dependencies.** Controlled, isolated Linux environments with reproducible failures, sandboxing, execution limits, and a separate safety and integrity review. This is not an EGAR-LX v0.1 arm.

### F5 — EGAR activation and strategy-reuse analysis

**Status:** Future proposal — not approved.

**Research question.** Are particular forms of EGAR activity associated with better diagnostic outcomes?

**Why it may be valuable.** v0.1 measures the combined effect of the existing experience mechanisms and keeps cases with no observed activation in the primary population. It does not isolate which mechanism, if any, is responsible.

**Design dependencies.** Descriptive records of retrieval, injection, strategy promotion, trajectory reuse, and graph-edge reinforcement. An association is not a causal effect. Isolating one mechanism would require a later controlled experiment.

### F6 — Generalization across Linux problem families

**Status:** Future proposal — not approved.

**Research question.** How broadly does EGAR experience generalize beyond the types of problems encountered during acquisition?

**Why it may be valuable.** v0.1 allows a related family between an acquisition case and an evaluation case, and it forbids the same bug or a near-duplicate. It does not estimate transfer across families.

**Design dependencies.** Explicit family definitions, cases that stay independent under those definitions, and an analysis that accounts for dependence among cases in the same family.

### F7 — Longitudinal experience development

**Status:** Future proposal — not approved.

**Research question.** How does diagnostic performance change as Thoth accumulates experience over an extended period?

**Why it may be valuable.** v0.1 is a paired comparison on held-out cases after one frozen acquisition snapshot. It does not observe experience accumulating over calendar time.

**Design dependencies.** The separately planned C6.4 observation is two sequential 28-day windows, 56 calendar days in total. That observation is not EGAR-LX v0.1, and its design is not assumed to match this experiment. The two are not merged here.

### F8 — Operational reliability and resource tradeoffs

**Status:** Future proposal — not approved.

**Research question.** Does an improvement in answer quality justify any additional computational or operational cost?

**Why it may be valuable.** v0.1 keeps answer quality separate from operational reliability. A missing or timed-out run is not scored as a wrong diagnosis. This entry is about whether those operational costs change with accumulated experience.

**Design dependencies.** Records of timeout frequency, completion rate, execution time, token consumption, and other operational cost, compared with answer quality. No cost threshold is set here.
