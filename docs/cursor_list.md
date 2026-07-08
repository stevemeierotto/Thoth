# Thoth Working Backlog

**Last updated:** 2026-07-07 (E2 **D4 Step 2 locked** § D.4.0 Step 2; Step 1 ✅ § D.4.0)  
**Purpose:** Active todo list for the next development sessions. Specs live in `improvements.md`; finished work is logged in `completed_improvements_log.md`.

**Workflow gate:** All checkpoint work in this file follows the Planning/Implementation Gate in AGENTS.md — plan and stop, wait for explicit approval, then implement.

**Active E2 work:** 🔒 **D4 Step 2 locked** (§ D.4.0 Step 2) — E2-D4-01 live plugin path; await explicit implementation authorization. Step 1 ✅ · D3 ✅.

**Baseline locked:** Headless cognitive loop verified — `run_test_suite` **TC-01–TC-07 all pass** (2026-06-27) with real `executeLLM`, RETRIEVAL→LLM plans, and GRAG scoring. Prior P0–P2 alignment (2026-06-17) in `completed_improvements_log.md`.

---

> **External review (2026-06, 2026-06-29)** — archived; consolidated in § Reflection & analysis below: see `cursor_list_archive.md` § External review

---

## Reflection & analysis (2026-06-29)

**Context:** Tier 0 (C1–C7) and memory consolidation (M1–M2) are shipped. Headless loop is verified; docs are candid. This section consolidates **external review** (above) with **implementation-grounded analysis** — what to do next and why.

### Where the system actually is

| Layer | Status | What it proves |
|-------|--------|----------------|
| **Wiring** | ✅ | RETRIEVAL→LLM goals complete; tools, reflection, scientific mode operational |
| **Component quality** | ✅/🔶 | C1/C2 hardened context + retrieval; G1 trajectory signal mixed on some buckets |
| **Memory pipeline** | ✅ | Hot→warm→cold consolidation + age policy; M1.5 verified retrieval path |
| **Learning thesis** | 🔶 **Unproven longitudinally** | Strategy promotion + episodic memory **exist**; cross-session behavioral lift **not yet measured** |

The gap is not “missing features” — it is **missing eval that connects features to outcomes over time**.

### Three eval layers (current vs needed)

```
Component harnesses (C3–C5)     →  “Does this mechanism fire correctly?”     ✅
Per-goal metrics (C6)           →  “How did this run perform?”               ✅
Longitudinal / learning eval    →  “Is the system improving across runs?”    ❌ (proposed C6 Phase 3)
```

M1.5 proved the **consolidation pipe** (Apollo E2E). It did **not** prove that consolidated memory improves **later goal success** — that needs **E2** (repeat-goal harness).

### Expert recommendations — agreed priorities

1. **Longitudinal metrics (C6 Phase 3)** — Highest leverage for thesis honesty. Instrumentation exists; analysis is per-run only.
2. **SCR / strategy-impact harness (E3)** — Strategy Engine is real; impact is trace-visible but not regression-tested.
3. **G1 diagnostic before F5** — Negative lift on trajectory-disambiguation bucket → ablate before embedding more into T.
4. **M3/M4 before “long-term memory” claims** — Warm tier is readable via GRAG but **operators cannot replay** archived episodes; write-only for ops.
5. **B1 before V3 Zenodo** — Hardened corpus defends external claims.

### Additional insights (not on prior lists)

| ID | Insight | Rationale |
|----|---------|-----------|
| **E1** | **Benchmark environment pinning** | ✅ Complete 2026-07-01 — Checkpoints A–E; spec **`docs/benchmark_environment.md`**; unblocks E2, B1, V3 |
| **E2** | **Episodic learning eval** | M1.5 = pipeline correctness. STRICT = deterministic retrieval from **declared frozen episodes** (not organic consolidation). Full checkpoint plan: **§ E2**; protocol **`docs/E2_PROTOCOL.md`**. |
| **Doc** | **Sync `COGNATE_V2.html`** | Markdown has mock footnote for 51× depth; HTML export may not — align before any thesis-facing export. |
| **Discipline** | **Mock vs Ollama split** | Fast CI (mock/TfIdf) must never be the sole evidence for learning or retrieval claims. Nightly `--full` + pinned env = authoritative tier (already in C4; enforce in eval culture). |
| **F3 vs M4** | **F3 overlaps M4** | “Richer episodic memory” includes restore/replay — **M4 is prerequisite**, not parallel optional work. |
| **F1 timing** | **F1 after eval, not before** | Planning heuristics need longitudinal + episodic eval to show *where* plans fail (retrieval miss vs decomposition vs tool choice). Otherwise F1 is guesswork. |

### Nuance on expert “F3 + F1 after M3/M4”

Agree on direction; adjust sequencing:

- **M3/M4** — operational completeness (inspect, manual prune, range restore).
- **G1 + E1** — cheap diagnostics; run in parallel with M3/M4 if capacity allows.
- **C6 Phase 3 + E3 + E2** — eval layer that validates (or falsifies) learning claims.
- **B1** — when preparing V3 or external publication.
- **F3/F1** — only after eval identifies the binding constraint; F3 extends what M4 restores, F1 fixes planner quality ceiling.

### Consolidated roadmap (reflection snapshot)

```
Now      E2 Phase D (D1+)           (passive consumer evolution — D0 locked)
Done     E2 Phase C                 (integration tier — locked 2026-07-05)
Done     E2 Phase B                 (authoritative STRICT baseline)
Next     C6 Phase 3 + E3             (longitudinal metrics; SCR harness)
Parallel B1 (if V3 Zenodo)           (hardened corpus)
Later    M4                         (range restore — M3 ✅)
Later    F3 / F1                     (promote when eval shows bottleneck)
Last     UI polish (§6), S1 apply_diff (owner discretion)
```

### What not to do yet

- **F5** (semantic trajectory embeddings) before G1 diagnostic completes.
- **Zenodo V3** before B1 + pinned-env benchmark runs.
- **F-series bulk promotion** — horizon items; eval should drive which F moves first.
- **NODE / self-building** — defer per existing backlog discipline.


---

## E2 — Episodic learning eval (Phases A–C complete; Phase D — D3 Step 2 pending)

**Status:** 🔒 Phase C locked (2026-07-05); Phase D D0 locked — ✅ **D1–D2 complete**; **D3 Step 1 complete** — **D3 Step 2 plan approved** (§ D.3.0); paused before Step 2 implementation  
**Spec:** `docs/E2_PROTOCOL.md` v1.2 (preregistered constants; do not change mid-run)  
**Phase C close-out:** `docs/phases/PHASE_C_COMPLETE.md`  
**Phase D protocol:** `docs/D_PHASE_PROTOCOL.md` v1.0  
**What this is:** Converting a coupled cognitive runtime into a **two-layer evaluation kernel** with determinism boundaries — not a refactor.

| Layer | Role |
|-------|------|
| **`e2_eval_kernel`** | Deterministic lab function: sealed inputs → scored outputs. No heuristics, no hidden state. |
| **`basic_agent` + `rag.cpp`** | Product cognition. **E2-INTEGRATION** tier = diagnostic only, never official evidence. |

**Pause between every sub-checkpoint** (same discipline as E1 D1–D5): build green → tests green → confirm before next step.

**Checkpoint plans** (A1, A2, A3, …) are **approved — subject to revision** as implementation proceeds. Only **`E2_PROTOCOL.md` v1.2** pass/fail rules are preregistered/locked until v1.3.

### Done (protocol steps 1–5)

- `E2EvalConfig`, `SealedEpisodeInjectionLog`, version pins, evaluation fingerprint
- `e2_eval_kernel` CMake target (compile-time exclusion of `rag.cpp`)
- `validateStrictConfigForOfficialRun`, table-driven evaluator, mock harness scaffold
- Unit tests E2-01–E2-07

### Gate architecture (priority order)

Debugging should read **`scoring_block_reason`** in artifacts before reverse-engineering which gate fired.

| Tier | Mechanism | Role |
|------|-----------|------|
| **1 — Source of truth** | `e2_eval_kernel` compile-time exclusion (`THOTH_E2_STRICT_KERNEL=1`; no `rag.cpp`) | **Pre-build guarantee** — fix here if anything disagrees |
| **2 — Verification** | Linker/symbol audit (`nm -C` on scored-path `.o` + harness binary) | **Post-build check** — does not substitute for tier 1 |
| **3 — Behavioral** | Runtime guard (A5), unit tests E2-08–E2-11, wiring-state gate | Catches reintroduction / wiring mistakes |

Source grep for `RAGPipeline` / `retrieveRelevant` = fast sanity check only, **not** an exit criterion.

**Enforcement philosophy** (see **`E2_PROTOCOL.md` § Enforcement philosophy**): compile-time exclusion prevents accidental linkage; static audits verify intended dispatch; runtime guard detects regressions at execution. **No single tier is sufficient alone.** STRICT never silently falls back to heuristic retrieval.

### Scope limits — what STRICT claims (and does not)

| Question | Answered by | Mechanism |
|----------|-------------|-----------|
| Can consolidated memory be retrieved? | **M1.5** ✅ | Organic consolidation pipeline |
| Given **declared frozen episodes**, does deterministic retrieval change goal outcomes? | **E2-STRICT** (official, **Phase B** only) | Pre-sealed `SealedEpisodeInjectionLog` |
| Does organic consolidation → warm tier → retrieval → lift? | **E2-INTEGRATION** (diagnostic) | Real `plantAndConsolidate` path — **non-scoring** |

**STRICT warm ≠ organic consolidation.** Pre–Phase A harness used `plantAndConsolidate` → real episodic → warm tier → GRAG. Phase A warm arm = **pre-sealed case-table entries** (synthetic injection). Intentional for determinism; must be stated plainly in protocol and any publication.

### Wiring modes — disabled vs kernel-verified vs official (Phase A / B)

Do **not** use fake `FAILED_*` to mean "not wired yet" — that conflates system defect with intentional non-evaluation.

| Concept | Artifact signal |
|---------|-----------------|
| **Evaluation disabled (A1–A2)** | `scoring_enabled: false`; `retrieval_enabled: false`; no lift; no `e2_outcome`; `wiring_stage: "A1"` or `"A2"` |
| **Kernel retrieval verified (A3–A5)** | `retrieval_enabled: true`; `evaluation_boundary_verified: true`; `official_scoring: false`; `scoring_enabled: false`; no `e2_outcome`; no benchmark SUCCESS/FAILURE claims |
| **Official benchmark (Phase B+)** | `official_scoring: true`; `scoring_enabled: true`; `e2_outcome` permitted when all gates pass |
| **Evaluation failed (Phase B+ only)** | Official scoring attempted; `arm_scoring_status: FAILED_*`; lift withheld |

| CP | Kernel retrieval? | Harness | `e2_outcome` | `official_scoring` |
|----|-------------------|---------|--------------|-------------------|
| **A1** | No | Builder self-test only (`E2_STRICT_INJECTION_LOG_DIAG`); **no `runCaseArm`** | Not emitted | `false` |
| **A2** | No | Plumbing smoke: **`runCaseArm`** per case; sealed log built/logged; **no kernel retrieval** | Not emitted | `false` |
| **A3** | Yes (`e2StrictRetrieve` @ boundary) | **`runCaseArm`** + kernel boundary diag; executive outputs **non-authoritative** | Not emitted | `false` |
| **A4–A5** | Yes (executive wired / guarded) | Migration continues; still pre-baseline | Not emitted | `false` |
| **B** | Yes | Full scored loop | Permitted if all gates pass | `true` |

**`THOTH_E2_WIRING_STAGE` values (documented contract):**

| Value | Meaning | Protocol? |
|-------|---------|-----------|
| `A1` | Builder-only diagnostic (opt-in regression after A2 lands) | ✅ Checkpoint |
| `A2` | Consolidation decoupled; arm plumbing smoke; evaluation-disabled | ✅ Checkpoint |
| `A3` | Kernel retrieval consumes sealed log; boundary provenance verified; **not** official scoring | ✅ Checkpoint |
| `A4` | Context injection + single dispatch + executive → same `e2StrictRetrieve()`; full equivalence; **not** official scoring | ✅ 2026-07-02 |
| `A5` | Diagnostic fuse; post-A4 regression enforcement; guard-only `rag.cpp` change; **not** official scoring | ✅ 2026-07-02 — E2-11; `runtime_heuristic_guard` |
| *(unset after A5)* | Defaults to current checkpoint; Phase B next | ✅ |
| `SCORING` | **Temporary dev knob only** — legacy full loop. **Must not exist in any authoritative benchmark configuration** (Phase B re-baseline, publication, CI nightly). Not protocol. | ❌ Internal dev only |

Pre–**Phase B** harness output is **non-authoritative** regardless of exit code.

**JSONL event names (A1 implemented — do not introduce alternates):**

| Event | Used for |
|-------|----------|
| `E2_STRICT_INJECTION_LOG_DIAG` | Per case/arm sealed-log row; **A2** adds plumbing fields after `runCaseArm`; **A3** adds kernel boundary fields (`retrieval_enabled`, `strict_retrieval_status`, chunk summary) |
| `E2_WIRING_CHECKPOINT` | Checkpoint summary row + `BenchmarkRun` emit via `completeWiringCheckpoint()` |

### Provenance at evaluation boundary (A3+)

**Rule:** Complete provenance required **at the evaluation boundary** (where kernel chunks enter `strictProvenanceValid()`), not at every internal pipeline stage. After A3, **STRICT provenance must never come from executive diagnostics** — only from `e2StrictRetrieve()` via `provenanceFromStrictRetrievalResult()`. Any untraced chunk **crossing the boundary** → `FAILED_PROVENANCE`, no lift, **no degraded-score mode**.

**Vacuous retrieval guard (implement at boundary / evaluator):** if `retrieval_status == SUCCESS` **and** expectations require episodic retrieval **and** `chunk_count == 0` → `FAILED_RETRIEVAL` (retrieval owns emptiness; provenance owns trace completeness). Prevents `chunks = []` from passing `strictProvenanceValid()` vacuously.

See **`docs/E2_PROTOCOL.md` § STRICT retrieval boundary** for the canonical diagram.

### Implementation checkpoints

| ID | Covers | Stop criteria |
|----|--------|---------------|
| **0** | Embedding pin `\u0002` fix + semantic validation + regression test | ✅ 2026-07-01 |
| **5.0** | Wiring contract + gate priority in `E2_PROTOCOL.md` | Doc only |
| **A1** | STRICT sealed log builder from case table | ✅ 2026-07-02 — **E2-08**; `buildStrictInjectionLogFromCaseTable`; evaluation-disabled harness |
| **A2** | Remove `plantAndConsolidate` from STRICT path; scope-limits doc | ✅ 2026-07-02 — **E2-09** + **E2-09b**; Option A test discipline |
| **A3** | `e2StrictRetrieve()` + boundary provenance; layer ownership contract | ✅ 2026-07-02 — **E2-10**; `retrieval_enabled: true`; purity verified; **not** official scoring. **Close-out:** token-overlap episode gating + **provisional 0.25 threshold** documented in `E2_PROTOCOL.md` § STRICT kernel scoring (A3) |
| **A4** | Executive delegates to same `e2StrictRetrieve()`; context injection + single dispatch + full equivalence | E2-01–E2-07; static dispatch audit + golden-case runtime proof |
| **A5** | Diagnostic fuse; A3→A4 transition enforced; failure-domain separation; first `rag.cpp` touch (guard only) | ✅ 2026-07-02 — **E2-11** |
| **B** | STRICT re-baseline (after 0 + A1–A5) | ✅ 2026-07-04 — authoritative baseline; fingerprint `1ce31c6a…` |
| **C** | Integration tier (C1–C5) | ✅ 2026-07-05 — passive eval in architecture; path equivalence proven |
| **D** | Evolution tier (D0–D5) | 🔒 D0 locked; D1–D2 ✅; D3 Step 1 ✅ — **Step 2 pending** (§ D.3.0) |

**Scope estimate:** Phase A ≈ **3–5 sub-sessions** (not one session) — largest change since E1 Checkpoint C.

**Phase A decomposition (one edge per checkpoint):**

```
A1  →  build sealed log
A2  →  carry sealed log (harness plumbing)
A3  →  kernel consumes sealed log          (Harness → e2StrictRetrieve)
A4  →  executive consumes kernel           (Executive → e2StrictRetrieve)
A5  →  prevent regressions                 (runtime guard)
B   →  authorize scoring
C   →  integration tier
```


> **Checkpoints A1–A5** (kernel wiring) — ✅ complete, archived: see `cursor_list_archive.md` § Checkpoint A1

> **Phase B (B1–B6)** — ✅ complete, locked: see `cursor_list_archive.md` § Phase B

> **Phase C (C.0.0–C.5.0)** — 🔒 locked: see `cursor_list_archive.md` § C.0.0

### D.0.0 — E2 Phase D (Evolution Tier)

**Authority:** [`docs/D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md) v1.0  
**Prerequisite:** Phase C locked — [`phases/PHASE_C_COMPLETE.md`](phases/PHASE_C_COMPLETE.md)

**Goal:** Prove the architecture can **evolve** — more subscribers, replay, metrics/trace, live INTEGRATION connection — without changing Phase B evaluation semantics or Phase C path equivalence.

**One sentence:** Grow passive consumers and operational modes while proving nothing important changed.

##### D0 — Evolution boundary (informational — not coded)

Phase B + Phase C contracts are **immutable dependencies**. Tweaking evaluation during evolution = protocol change (E2 v1.3+), not Phase D work.

##### Three architectural modes (D0)

| Mode | Role |
|------|------|
| **STRICT** | Authoritative — `wiring_stage=B` only |
| **INTEGRATION** | Diagnostic — `official_scoring: false` |
| **PRODUCTION** | Operational execution — evaluation observes, never decides |

D4 **connects** INTEGRATION to production; it does not introduce INTEGRATION.

##### Phase D Constitutional Rule

> **Every new capability must satisfy: Observe, Record, Replay, Present — Never Decide.**

If a component influences planning, retrieval, memory, evaluation, or benchmark outcomes — it does not belong in Phase D.

##### Passive Consumer Law (all five required)

1. Consumes immutable events  
2. Cannot modify publisher state  
3. Cannot influence execution ordering  
4. Cannot become required for successful execution  
5. Can be removed without changing benchmark results  

##### GUI consequence (not a checkpoint)

> GUI is not part of the evaluation architecture. GUI is merely another subscriber. Dependency is one-way: artifacts → display.

##### Checkpoint sequence (implement in order)

| Step | Work | Gate |
|------|------|------|
| **E2-D1** | Event channel fan-out — subscriber count invisible to Executive | 0 vs N subscribers → identical Executive work |
| **E2-D2** | Replay subscriber — replay changes **time** | Passive Consumer Law; replay removal → benchmark unchanged |
| **E2-D3** | Metrics + trace subscribers — observation changes, not time | Measure, don't interpret (C4 philosophy) |
| **E2-D4** | Live INTEGRATION connection — wire diagnostic mode to production path | E2-06 contract; STRICT path uncontaminated |
| **E2-D5** | Evolution trust proof — mirror C5 | C5 matrix + Phase B fingerprint + `PHASE_D_COMPLETE.md` |

**Time estimate:** D1 **3–5 h**; D2 **3–4 h**; D3 **4–6 h**; D4 **3–5 h**; D5 **2–4 h**.

##### Regression gates (every checkpoint)

| Gate | Requirement |
|------|-------------|
| C5 equivalence | `THOTH_E2_C5=1` — golden fixtures MATCH |
| STRICT benchmark | `THOTH_E2_WIRING_STAGE=B` — fingerprint `1ce31c6a…` stable |
| Phase C regression | E2-C1..C5 green |
| Constitutional Rule | Observe / Record / Replay / Present only |
| Passive Consumer Law | All five conditions per new subscriber |

**Status:** 🔒 **D0 locked** (2026-07-05). ✅ **E2-D1 complete** (2026-07-05). ✅ **D2 complete** + **D2-03 resolved** (2026-07-07). ✅ **D3 complete** (2026-07-07). 🔒 **D4 plan locked** (§ **D.4.0**).

---

> **D1 / D1.1 / D2 / D2.1** — ✅ complete: see `cursor_list_archive.md` § D.1.0

#### D.3.0 — E2-D3 implementation plan (metrics + trace subscribers — **v1 locked**)

**Authority:** [`docs/D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md) § D3  
**Prerequisites:** D1 ✅, D2 ✅, D2-03 ✅, G1/G2 ✅, **D3 Step 1 ✅**  
**Estimate:** 4–6 h (remaining Steps 2–6)  
**Status:** 🔒 **v1 locked** (2026-07-07) — **Step 1 complete** — **Step 2 plan approved** — paused before Step 2 implementation

##### Proof obligation (what D3 must prove)

> **D3 proves that operational observability (metrics and trace) can be added to the architecture without introducing any reverse dependency or decision influence on the cognitive pipeline.**

Prior tiers proved: C1 façade · C2 publication · C3 diagnostics · C4 telemetry · C5 equivalence · D2 observation through **time**. D3 proves observation through **measurement and correlation** only.

##### One sentence

> **D3 is a proof, not a feature.** Prove `MetricsSubscriber` and `TraceSubscriber` can coexist on the same immutable `EpisodeCompleted` fan-out as eval/replay without mutating events, blocking delivery, or feeding back into Executive, planner, evaluation, or benchmark authority.

##### What D2 proved vs what D3 must add

| Already proven (D1–D2) | D3 must add |
|------------------------|-------------|
| Multi-subscriber fan-out; immutability at delivery | **Two distinct** observability subscribers with **non-overlapping ownership** |
| Replay changes **time** (re-observation) | Metrics + trace change **observation** (measurement + correlation) |
| Subscriber failure isolation on channel | **Explicit non-blocking** guarantee for metrics/trace throws (mirror D1/D2) |
| Passive Consumer Law on replay | Same law on metrics/trace; sink-only + structural audits (C4 discipline) |

**Why separate subscribers:** Without an explicit Metrics vs Trace boundary, they merge into one “observability blob” and scope creeps.

##### Subscriber ownership (locked — no overlap)

| Subscriber | **Owns** | **Does NOT own** |
|------------|----------|------------------|
| **`MetricsSubscriber`** | Counters, durations, queue depths, rates, histograms; per-`run_id` aggregate state | Event ordering; evaluation semantics; causal timeline; pass/fail classification |
| **`TraceSubscriber`** | Correlation, chronology, causal **links** (ID joins), run timeline segments | Statistics, scoring, aggregation, histograms, rates |

**Rule:** Metrics **aggregates**; Trace **correlates**. Neither interprets.

##### Metrics interpretation boundary (strengthened)

`MetricsSubscriber` may record **raw** numeric and label values from allowed inputs, but shall **never** derive classifications, pass/fail state, lift interpretation, benchmark authority, or planner decisions from those values — including indirect derivation via thresholds, rolling windows, or composite scores that encode success/failure meaning.

Same discipline applies to `TraceSubscriber` for timeline labels: IDs and stage names only; no `evaluation_resolution` / `e2_outcome` / `official_scoring` on the trace authority path.

##### Event contract (frozen — builds on D1/D2 fan-out)

```
Executive
    │ publish EpisodeCompleted (immutable, fire-and-forget)
    ▼
InProcessEpisodeEventChannel (FIFO delivery; per-subscriber failure isolation)
    ├── EvaluationSubscriber   (Phase C — frozen)
    ├── ReplaySubscriber       (D2)
    ├── MetricsSubscriber      (D3)
    └── TraceSubscriber        (D3)
```

**Subscriber ordering invariant:** Subscriber ordering is **not architecturally significant**. Any subscriber may execute before or after another without changing system behavior. Ordering may affect **log timestamps only** — not evaluation, replay capture, metrics aggregates, trace segments, Executive outcomes, or benchmark results. Correctness must **not** depend on registration order (e.g. Evaluation → Replay → Metrics → Trace is **not** a pipeline).

**Immutable event payload (contractual):** Subscribers receive a **`const` event view** (`const EpisodeCompleted&` or equivalent immutable snapshot). No subscriber may modify `EpisodeCompleted` or any **shared payload** visible to sibling subscribers. Immutability is a **contractual** property of the channel API, not merely an implementation convention. Observation is read-only; durable state is **subscriber-owned copies** only.

**Out of scope for D3 channel contract:** New event types on `IEpisodeEventChannel` (`EpisodeStarted`, `ReplayStarted`, etc.) — deferred; D3 does not expand the live channel schema.

##### Trace input contract — what Trace records (D3 v1)

| Source | D3 v1 behavior | Rationale |
|--------|----------------|-----------|
| **`EpisodeCompleted`** (channel) | **Mandatory** — append one trace segment per delivery; primary spine | Proves fan-out + Passive Consumer Law with eval/replay/metrics present |
| **`decision_trace.jsonl`** | **Read-only correlation** — join by `run_id` / `plan_id` / `step_id` when flushing or building timeline export; **no** live Executive hook in D3 | “Correlate across decision_trace + E2 streams” without new Executive emissions |
| **E2 / telemetry JSONL** | **Read-only correlation** — join by IDs on allowed non-semantic fields only | Cross-stream chronology without consuming eval meaning |
| **`ReplaySubscriber` replay sink** (test seam / optional wiring) | **Optional** — record `replay_observed` with `replay_index` + content fingerprint; **not** a channel republication | Observes D2 replay path without changing replay proof |
| **New Executive lifecycle events** | **Not in D3** | Avoid channel schema drift |

**Trace segment schema (D3 v1 — observational only):**

| Field | Required | Notes |
|-------|----------|-------|
| `segment_type` | ✅ | e.g. `episode_completed`, `replay_observed`, `correlation_join` |
| `timestamp_ms` | ✅ | Wall or event timestamp |
| `run_id` | ✅ when present on source | |
| `goal_id` | ✅ when present | |
| `plan_id` | ✅ when present | |
| `episode_id` | ✅ for `episode_completed` | Stable ID on `EpisodeCompleted` snapshot |
| `replay_index` | Optional | When `replay_observed` |
| `replay_id` | Optional | Subscriber-generated UUID per replay delivery |
| `parent_run_id` | Optional | For nested / resumed runs when attribution provides it |
| `correlation_keys` | Optional | Map of join keys used for read-only merges |
| `source` | ✅ | e.g. `channel`, `decision_trace`, `replay_sink` |

**Forbidden on trace authority records:** `official_scoring`, `e2_outcome`, `evaluation_resolution`, lift, case pass/fail.

##### Metrics timestamp sources (locked — explicit split)

| Class | Source | Option |
|-------|--------|--------|
| **Episode-scoped metrics** | `EpisodeCompleted` payload only — timestamps, counts, allowed labels on the snapshot | **A** |
| **Pipeline-scoped metrics** | `PipelineTelemetryService` / C4 telemetry envelopes — counters, latencies, queue depth | **B** |

**Locked:** D3 uses **both**, with strict separation: episode metrics never call eval/diag helpers; pipeline metrics consume **telemetry sink only** (same boundary as E2-C4-03b). No third ad-hoc timestamp path in D3.

##### Failure isolation (locked — E2-D3-02)

Channel delivery uses per-subscriber **catch / log / continue**:

```
for each subscriber:
    try {
        subscriber.onEpisodeCompleted(const_event_view)
    } catch (...) {
        log isolated failure
        continue   // next subscriber still runs
    }
```

**Failure isolation invariants (testable — E2-D3-02):**

1. One subscriber failure must **not** suppress another subscriber  
2. One subscriber failure must **not** suppress the Executive  
3. One subscriber failure must **not** suppress event publication (publish completes before fan-out; fan-out failures do not roll back publication)

Mirror D1/D2 discipline: throwing `MetricsSubscriber` or `TraceSubscriber` must not block siblings or alter Executive-path terminal outcome — same rigor as E2-D1-02.

##### Storage (locked — subscriber-owned)

| Subscriber | Hot state | Durable sink (optional) | Notes |
|------------|-----------|-------------------------|-------|
| **`MetricsSubscriber`** | In-memory aggregates keyed by `run_id` (counters, histograms, rates) | Append-only JSONL: `logs/e2_metrics.jsonl` | No SQLite in D3; no shared global singleton visible to Executive |
| **`TraceSubscriber`** | In-memory ring buffer (recent N segments; test seam for assertions) | Append-only JSONL: `logs/e2_trace.jsonl` | No binary format in D3; correlation joins may read existing JSONL read-only |

Flush failures on JSONL sinks: **log and drop** — never block channel delivery or Executive (same as C4 / D2 JSONL optional sinks).

##### Configuration (locked — production wiring)

Register in `basic_agent_plugin` behind flags — mirror `enable_episode_replay_subscriber`:

| Config | Default | Subscriber |
|--------|---------|------------|
| `enable_metrics_subscriber` | `false` | `MetricsSubscriber` |
| `enable_trace_subscriber` | `false` | `TraceSubscriber` |

Independent flags — either, both, or neither may be enabled. Defaults OFF prove wiring without runtime behavior change until D5 trust re-proof.

##### Required correlation IDs (frozen vocabulary)

| ID | Required when | Owner |
|----|---------------|-------|
| `run_id` | Attribution present on episode | Source event / attribution block |
| `goal_id` | Goal execution | Source event |
| `plan_id` | Plan-bound episode | Source event |
| `episode_id` | Every `EpisodeCompleted` | Snapshot on event |
| `replay_id` | Optional — per replay sink delivery | `TraceSubscriber` generates |
| `parent_run_id` | Optional — resumed / nested runs | Attribution when available |

**Schema drift rule:** New IDs require protocol bump (E2 v1.3+ or D3 v2) — not ad-hoc fields in D3 implementation.

##### Non-goals (explicit — prevents scope creep)

D3 does **not**:

- Analyze performance or recommend optimizations  
- Optimize execution paths or scheduling  
- Change scheduling, parallelism, or Executive ordering  
- Influence planner or tool selection  
- Influence ExecutiveController decisions or goal completion  
- Alter evaluation (`resolveEvaluation`, `EvaluationService`)  
- Change replay semantics (`ReplaySubscriber` contract)  
- Alter benchmark semantics, STRICT authority, or Phase B fingerprints  
- Introduce new channel event types or require subscribers for goal success  
- Feed metrics/trace aggregates back into GRAG, memory, or strategy promotion  

##### Implementation order (pause between steps)

| Step | Work | Gate |
|------|------|------|
| **1** | `metrics_subscriber.h` / `.cpp`, `trace_subscriber.h` / `.cpp` — skeleton; subscriber-owned storage; no eval/Executive imports; optional JSONL sinks | ✅ Build green (`THOTH_E2_D3_STEP1=1`) |
| **2** | **E2-D3-01** — metrics sink-only; opaque score observation; frozen aggregation ops; frozen Metrics JSONL v1.0; eval-independence audit; backward-compat exit | D3-01 green |
| **3** | **E2-D3-02** — failure isolation; exactly-once delivery; locked terminal outcome comparison; mandatory ordering permutation | D3-02 green |
| **4** | **E2-D3-03** — structural audit; exclusive ownership; interpret boundary; authority boundary; publication-mechanism audit | D3-03 green |
| **5** | **Plugin/config integration proof** — production wiring, JSON persistence, independent flags, post–Steps 2–4 default-OFF safety | D3-05 green |
| **6** | Regression — umbrella `THOTH_E2_D3=1` (full D3 proof suite, Steps 1–5); `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1`, G2 `ctest` | Full green; pause before D4 |

**Gate env:** `THOTH_E2_D3=1` — executes the complete D3 proof suite (Steps 1–5); per-step gates remain available (`THOTH_E2_D3_01=1`, …)

##### D3 Step 2 plan — E2-D3-01 (approved 2026-07-07)

**Scope:** `MetricsSubscriber` only — trace behavior unchanged from Step 1 skeleton.

**Objective:** Prove metrics records **raw observational values** from allowed sources without importing evaluation meaning, diagnostic scoring, or Executive influence.

###### Opaque `final_success_score` rule (locked)

`final_success_score` on `EpisodeCompleted` may be stored only as an **opaque observation**:

| Allowed | Forbidden |
|---------|-----------|
| Record raw float under neutral key `observed_final_success_score` | Thresholds, bucketing, or pass/fail classification derived from the value |
| `histogram_observe` / `gauge_set` on the raw float | Comparison to `e2_expectations`, `expected_lift`, `min_cold_score`, `min_warm_score` |
| Include in per-`run_id` aggregate state as an uninterpreted sample | Metric names implying success semantics (`success_score`, `pass_score`, etc.) |

The value is **telemetry of what the Executive published** — not an evaluation outcome and not benchmark authority.

###### Allowed aggregation operations (frozen — MetricsSubscriber only)

D3 v1 permits **only** these operations on subscriber-owned state:

| Operation | Semantics | Example use |
|-----------|-----------|-------------|
| `counter_increment` | `+1` on discrete event | `episode_completed_total` |
| `counter_add` | Add raw numeric delta (no value-derived label) | `plan_step_count` sum from snapshot |
| `gauge_set` | Store last observed raw value | `observed_final_success_score` |
| `histogram_observe` | Record raw sample into fixed buckets | Duration or opaque score samples |
| `duration_observe_ms` | Record raw millisecond delta from allowed timestamps | `completed_at_ms` deltas, C4 stage durations |

**Forbidden aggregation patterns:** rolling windows with classification; rates with success/failure numerators; composite scores; percentile-to-pass/fail mapping; any aggregation that reads `e2_expectations` or eval/diag payloads for scoring meaning.

###### Metrics JSONL schema (frozen v1.0)

**Path:** `logs/e2_metrics.jsonl` (append-only, optional sink)  
**Schema version:** `metrics_schema_version: "1.0"` — bump requires protocol note (E2 v1.3+ or D3 v2), not ad-hoc fields.

**Record shape (one JSON object per line):**

```json
{
  "metrics_schema_version": "1.0",
  "record_type": "episode_observation",
  "timestamp_ms": 1700000000000,
  "run_id": "run-abc",
  "plan_id": "plan-abc",
  "observations": {
    "episode_completed_total": 1,
    "observed_final_success_score": 0.85,
    "plan_step_count": 3,
    "terminal_state_label": "COMPLETED"
  }
}
```

Pipeline-scoped records use `"record_type": "pipeline_observation"` and a `pipeline` object carrying **C4 telemetry fields only** (stage durations, `episodes_processed`, queue depth) — same E2-C4-03b boundary.

**Forbidden keys on metrics authority path:** `official_scoring`, `e2_outcome`, `evaluation_resolution`, `lift`, `pass`, `fail`, `success_rate`, `scorable_cases`, `not_scorable_cases`, and any eval/diag authority field.

Flush failures: **log and drop** — never block channel delivery or Executive.

###### E2-D3-01 structural audit — evaluation independence (Step 2)

`metrics_subscriber.h` / `metrics_subscriber.cpp` must remain **architecturally independent** of evaluation implementation:

| Forbidden in metrics sources | Required |
|------------------------------|----------|
| `#include` of `episodic_evaluation_service.h`, eval scoring headers | Channel + allowed episode fields + C4 telemetry interface only |
| Calls to `resolveEvaluation`, `evaluateCase`, `evaluateEpisodicLearningCase` | No `IEpisodicEvaluationService` usage |
| Diagnostic scoring helpers (`summarize`, case resolution applicators) | No branching on eval/diag outputs |
| Reading `plan_snapshot.e2_expectations` for metric meaning | Structural grep audit in `testE2D3_01MetricsEvalIndependence` |

Independence is **structural** (no eval implementation imports) and **behavioral** (metrics path identical whether eval subscriber is present).

###### D3 Step 2 exit criteria

1. `THOTH_E2_D3_01=1` green — sink-only, opaque score, frozen ops, JSONL v1.0 shape, eval-independence audit  
2. **Backward compatibility (explicit):** with `enable_metrics_subscriber=false`, runtime behavior and architectural fingerprints are **identical** to pre–Step 2 baseline:
   - `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` green with flag OFF  
   - No Executive, evaluation, or replay behavior change  
   - No new branching on metrics state in frozen components  
3. `TraceSubscriber` unchanged beyond Step 1 skeleton  
4. **Pause for review** before Step 3 (E2-D3-02)

###### D3 Step 2 implementation discipline (locked)

Build **only** what is required to satisfy **E2-D3-01**.

- If a helper, abstraction, or optimization is not needed to pass the Step 2 proof, **defer it**.  
- **Resist** adding future-facing infrastructure “because Step 3 will need it.”  
- Each checkpoint introduces **only** what its proof requires — Step 3 (failure isolation) and Step 4 (structural audit) add their own code and tests when explicitly authorized.

**Verification scope (locked):** Run **only** the tests required for this checkpoint — do **not** run the full unit-test suite (~1 h). Step 2 minimum:

| Command | Purpose |
|---------|---------|
| `THOTH_E2_D3_01=1 ./build/debug/tests/thoth-unit-tests` | E2-D3-01 proof |
| `THOTH_E2_D2=1 ./build/debug/tests/thoth-unit-tests` | Backward compat (flag OFF) |
| `THOTH_E2_D1=1 ./build/debug/tests/thoth-unit-tests` | Backward compat (flag OFF) |
| `THOTH_E2_C5=1 ./build/debug/tests/thoth-unit-tests` | Backward compat (flag OFF) |

Full suite / G2 `ctest` deferred to **D3 Step 6** unless explicitly requested.

##### E2-D3-01 (metrics sink-only — test contract)

1. Register `MetricsSubscriber` on channel with eval + replay present  
2. Publish fixture `EpisodeCompleted`  
3. Assert aggregates updated from **allowed** fields only (frozen aggregation ops)  
4. Assert `observed_final_success_score` recorded as opaque float — **no** derived pass/fail or threshold logic  
5. Assert metrics sink / JSONL (`metrics_schema_version: "1.0"`) contains **no** forbidden authority keys  
6. Assert metrics code does **not** call `resolveEvaluation`, `evaluateCase`, or diagnostic scoring helpers (structural + behavioral)  
7. **Eval-independence audit:** metrics sources include no evaluation implementation headers or symbols  
8. Pipeline metrics: assert telemetry envelope consumption path does not import eval/diag semantics (E2-C4-03b discipline)  
9. **Backward compat:** `enable_metrics_subscriber=false` — `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` unchanged

##### D3 Step 3 plan — E2-D3-02 (failure isolation — approved refinements pending lock)

**Scope:** Failure isolation proof for D3 subscribers — channel + Executive path. Channel `try/catch/continue` already exists (D1); Step 3 proves **MetricsSubscriber** and **TraceSubscriber** participate without breaking invariants.

**Gate (proposed):** `THOTH_E2_D3_02=1`

###### Terminal outcome equality (locked comparison set)

When comparing baseline vs throwing-subscriber runs, **terminal outcome equality** includes **only**:

| Included | Comparison |
|----------|------------|
| Executive state | `ControllerState` identical |
| Final goal status | Plan `status` identical |
| Plan success/failure | Terminal plan outcome fields identical |
| Produced result | Goal/plan outcome-carrying fields identical (where applicable) |

**Explicitly excluded** from terminal outcome comparison (expected to differ or irrelevant):

- Metrics emitted / `MetricsSubscriber` aggregates / metrics JSONL
- Trace records / `TraceSubscriber` segments / trace JSONL
- Structured log contents (log is **evidence** of isolation, not the property being proven)
- Subscriber-local state (capture counts, delivery counters, replay storage)

Do **not** compare metrics/trace/log output when asserting Executive terminal outcome equality.

###### Publication invariant (locked)

The property being proven:

1. `publish()` completes before fan-out begins  
2. `lastPublishedEventForTests()` (or Executive-path equivalent snapshot) reflects the published episode **regardless** of downstream subscriber throws  
3. Fan-out failures do **not** roll back or suppress publication  

Structured logs (`subscriber_failure`) are corroborating evidence only.

###### Delivery invariant (locked — exactly once)

For each **non-throwing** subscriber registered before `publish()`:

- Receives **exactly one** `onEpisodeCompleted` delivery per published episode  
- **No** duplicate deliveries  
- **No** skipped deliveries  

Applies to **all** non-throwing subscribers on the channel (eval, replay, metrics, trace, test captures) — not a single named “healthy capture” seam.

The **throwing** subscriber: invoked once, then throws; its failure must not affect sibling delivery counts.

###### Ordering invariant (mandatory — not optional)

Prove registration-order independence with at least two permutations, e.g.:

- `Evaluation → Replay → Metrics → Trace`
- `Trace → Metrics → Replay → Evaluation`

For each permutation: same fixture publish → all non-throwing subscribers **exactly once**; terminal outcome equality holds; throwing-subscriber case still isolates correctly.

###### D3 Step 3 implementation discipline

- Build only what E2-D3-02 requires (tests first; production changes only if channel gap found)  
- Test-only throwing subscriber classes in `unit_tests.cpp` preferred over production throw hooks  
- **Verification scope:** `THOTH_E2_D3_02=1` + `THOTH_E2_D3_01=1` + `THOTH_E2_D2=1` / `D1=1` (flag OFF) — not full suite

###### D3 Step 3 exit criteria

1. `THOTH_E2_D3_02=1` green — throwing metrics **and** throwing trace cases  
2. Terminal outcome equality per locked comparison set (exclusions respected)  
3. Exactly-once delivery for all non-throwing subscribers  
4. Mandatory ordering permutation green  
5. **Pause for review** before Step 4 (E2-D3-03)

##### E2-D3-02 (failure isolation — test contract)

1. Register **all** channel subscribers including one **throwing** `TraceSubscriber` (separate case: throwing `MetricsSubscriber`)  
2. Publish fixture episode — **publication invariant** holds (`lastPublishedEventForTests` / outcome snapshot present)  
3. Throwing subscriber: invoked once, failure **logged** (evidence only); delivery loop **continues**  
4. **Every non-throwing subscriber** receives **exactly one** delivery — no duplicates, no skips  
5. **Terminal outcome equality** (locked set): Executive state, plan status, goal/plan success-failure, outcome-carrying fields — **excluding** metrics, trace, logs, subscriber-local state  
6. Executive-path baseline vs throwing-subscriber run: terminal outcome equal per locked set (mirror E2-D1-02)  
7. **Mandatory ordering permutation:** at least two registration orders (e.g. eval→replay→metrics→trace vs trace→metrics→replay→eval) — identical delivery and terminal outcome

##### D3 Step 4 plan — E2-D3-03 (structural audit — refinements pending lock)

**Scope:** Audit tests + minimal fixes only if violations found. Consolidates Step 1–3 partial audits into one gate.

**Gate (proposed):** `THOTH_E2_D3_03=1`

###### Objective — measure, don't interpret

**Interpretation** (forbidden in metrics/trace subscribers) includes:

- Pass/fail or success/warning/failure classification  
- Evaluation scoring or resolution semantics  
- Benchmark comparison or lift interpretation  
- Policy decisions or recommendation generation  

Observability subscribers may **only record facts** (timestamps, durations, counters, identifiers, correlation IDs, trace segments, opaque numeric observations).

###### Authority boundary (heart of D3)

`MetricsSubscriber` and `TraceSubscriber` outputs are **observational artifacts only**. They must **never** become inputs to evaluation or Executive decision-making. No reverse edges into planner, `EvaluationService`, or benchmark authority.

###### Exclusive ownership (locked)

Ownership is **exclusive** — prevents gradual duplication:

| Subscriber | Sole owner of | Must not duplicate |
|------------|---------------|-------------------|
| **MetricsSubscriber** | Metric aggregation semantics (counters, gauges, histograms, durations, rates) | Chronological tracing, correlation graphs, timeline segments |
| **TraceSubscriber** | Chronological tracing semantics (segments, correlation IDs, timeline) | Histogram/rate aggregation, metric rollup logic |

###### Publication-mechanism invariant (broader than channel handles)

Subscribers own **no publication mechanism**. Forbidden in subscriber class bodies and sources:

- `IEpisodeEventChannel` handles (pointer, reference, `shared_ptr`)  
- Publisher abstractions, `publish(` on any channel  
- Callback registration objects or anything capable of initiating publication  

Architectural rule: **observe only; never publish.**

###### Ordering audit (structural — expanded)

Forbidden in metrics/trace sources:

- `subscriberCount`, registration-index branching, “first subscriber” logic  
- Stateful sequencing across deliveries: `firstDelivery`, `lastSubscriber`, `if (!seen_before)`, delivery-ordinal gates, or equivalent order-dependent state  

Behavioral ordering proof remains Step 3 (`testE2D3_02OrderingPermutation`); Step 4 adds structural grep for subtle order dependence.

###### JSONL audit (allowed vs forbidden — explicit)

| Allowed on observational path | Forbidden on observational path |
|-------------------------------|--------------------------------|
| Timestamps, durations | Scores used as verdicts |
| Counters, gauges (opaque) | Pass/fail labels |
| Identifiers, correlation IDs | Recommendations |
| Trace segments (observational) | Benchmark outcomes / lift |
| `terminal_state_label` (raw label copy) | Official classifications (`official_scoring`, `e2_outcome`, `evaluation_resolution`, etc.) |

###### Forbidden symbol grep (authority only — keep narrow)

Audit **architectural authority symbols** only — do not expand to generic helper names (avoids brittle noise):

`resolveEvaluation`, `evaluateCase`, `evaluateEpisodicLearningCase`, `ExecutiveController`, `execute_goal`, `episodic_evaluation_service`, `IEpisodicEvaluationService`, `evaluation_subscriber`, `episodicDiagnosticService`, `publish(`, `e2_expectations`, `official_scoring`, `evaluation_resolution`, `e2_outcome`

###### D3 Step 4 implementation discipline

- Audit tests only — no new subscriber features  
- Reuse Step 1–3 behavioral tests; Step 4 gate calls structural audits + may require `THOTH_E2_D3_02=1` / `THOTH_E2_D3_01=1` as regression  
- **Verification scope:** `THOTH_E2_D3_03=1` + targeted regressions — not full suite

###### D3 Step 4 exit criteria

1. `THOTH_E2_D3_03=1` green — exclusive ownership, interpret boundary, authority boundary, publication-mechanism, ordering structural, JSONL allowed/forbidden  
2. Steps 1–3 gates still green  
3. Production changes only if audit finds real violation  
4. **Pause for review** before Step 5

##### D3 Step 5 plan — plugin/config integration proof (scope rename)

**Former label:** “Plugin registration + both flags default `false`” — understates scope.

**Accurate name:** **Plugin/config integration proof** (production integration and configuration proof).

Step 1 introduced skeleton wiring; Step 5 proves the **production boundary** after Steps 2–4 subscriber behavior is complete.

###### What Step 5 proves (not re-proving subscriber semantics)

| Area | Proof obligation |
|------|------------------|
| **Plugin wiring** | `BasicAgentPlugin` registers metrics/trace **only** when flags ON + channel exists; no registration when OFF |
| **JSON persistence** | `enable_metrics_subscriber` / `enable_trace_subscriber` round-trip via `loadFromJson` / `saveToJson` |
| **Independent flags** | Either, both, or neither — no cross-coupling in plugin init |
| **Post–D3 regression** | Flags OFF → `THOTH_E2_D3_01=1`, `THOTH_E2_D2=1`, `THOTH_E2_D1=1` unchanged (targeted gates) |

###### What Step 5 does **not** re-prove (already Steps 1–4)

- Subscriber sink-only semantics (Step 2)  
- Failure isolation (Step 3)  
- Structural audits (Step 4)  
- Direct `register*` on test channel (Step 1)

###### Gate (proposed)

`THOTH_E2_D3_05=1` or fold into eventual `THOTH_E2_D3=1` (Step 6)

###### D3 Step 5 exit criteria

1. Plugin structural audit — flag-gated blocks mirror replay pattern; Executive does not branch on metrics/trace state  
2. Config JSON round-trip for both flags  
3. Plugin-path integration test: flags ON → subscribers on production channel; flags OFF → not registered; subscriber **identity** matches enabled flags (not count alone)  
4. Production integration path is proven to be the only registration path for observability subscribers  
5. Targeted post–D3 regressions green with flags default OFF  
6. **Pause for review** before Step 6 (umbrella `THOTH_E2_D3=1` proof-suite gate + G2)

##### D3 Step 6 plan — full proof-suite regression (umbrella gate)

**Scope:** Test harness + documentation only — no new subscriber features.

**Objective:** Close D3 by running the **complete proof suite** (Steps 1–5) under one env gate, then confirm backward-compat and full-suite / G2 regression.

| Work | Detail |
|------|--------|
| `runE2D3Tests()` | Orchestrates Step 1 → 2 (D3-01) → 3 (D3-02) → 4 (D3-03) → 5 (D3-05) |
| `THOTH_E2_D3=1` | Early-exit in `main()` — umbrella gate executes the full proof suite |
| Verification | `THOTH_E2_D3=1`, then `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1`, default full suite, `ctest -R thoth-unit-tests` |

Each step establishes a different architectural invariant; the umbrella gate proves they still hold together.

###### D3 Step 6 exit criteria

1. `THOTH_E2_D3=1` green — full D3 proof suite (Steps 1–5)  
2. `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` green (flags default OFF)  
3. Default full unit-test suite green  
4. G2 `ctest -R thoth-unit-tests` green within 1800s budget  
5. **Pause before D4**

##### E2-D3-03 (structural audit — test contract)

1. **Forbidden authority symbols** — narrow grep on `metrics_subscriber.*`, `trace_subscriber.*` (see Step 4 plan list)  
2. **Exclusive ownership audit** — metrics class: no trace/timeline constructs; trace class: no metric aggregation constructs  
3. **Immutability audit** — `const EpisodeCompleted&` contract; no shared-payload mutation (structural + Step 1 behavioral reference)  
4. **Ordering structural audit** — no `subscriberCount`/index branching; no delivery-sequencing state (`firstDelivery`, `lastSubscriber`, etc.)  
5. **Publication-mechanism audit** — subscribers own no channel handles or publish capability  
6. **JSONL authority audit** — allowed vs forbidden field classes on metrics (and trace when sink exists) observational path  
7. **Authority boundary audit** — metrics/trace outputs are not inputs to eval/Executive (structural: no reverse-edge symbols; behavioral: Steps 1–3 regressions)

##### Forbidden (D3)

- Merging Metrics + Trace into one subscriber class  
- Mutating `EpisodeCompleted` or publisher state  
- Metrics/trace as prerequisite for goal or benchmark success  
- Blocking channel delivery on sink flush failure  
- Consuming eval/diag fields for scoring meaning (direct or derived)  
- New `IEpisodeEventChannel` event types in D3  
- Depending on subscriber registration order for correctness  
- Reverse edges into Executive, planner, or `EvaluationService`  

##### Exit criteria (D3 complete)

1. Proof obligation satisfied — observability without reverse dependency  
2. Subscriber ownership boundary enforced in code + audit  
3. **Immutability + ordering invariants** hold under fan-out with all subscribers  
4. E2-D3-01..03 green  
5. `enable_metrics_subscriber=false`, `enable_trace_subscriber=false` registered  
6. `THOTH_E2_C5=1` unchanged; `wiring_stage=B` fingerprint stable; G2 green  
7. **Pause before D4**

##### Files (expected touch)

| File | Change |
|------|--------|
| `metrics_subscriber.h` / `.cpp` | **New** |
| `trace_subscriber.h` / `.cpp` | **New** |
| `external/basic_agent/CMakeLists.txt` | Add sources |
| `basic_agent_plugin.cpp` | Flag-gated registration |
| `config.h` / `config.cpp` | `enable_metrics_subscriber`, `enable_trace_subscriber` |
| `tests/unit_tests.cpp` | E2-D3-01..03 + gate |
| `docs/D_PHASE_PROTOCOL.md` | § D3 refinements |
| `docs/cursor_list.md` | § D.3.0 (this section) |

| Untouched | |
|-----------|--|
| `evaluation_subscriber.*`, `episodic_evaluation_service.*`, `resolveEvaluation()` | Semantics frozen |
| `replay_subscriber.*` | D2 contract frozen |
| `executive_controller.*` | No subscriber-count or metrics/trace branching |
| Phase B export / STRICT harness | Authority frozen |

**Status:** 🔒 **v1 locked** (2026-07-07). **D3 complete** — proof suite Steps 1–6 green; paused before D4 implementation.

#### D.4.0 — E2-D4 implementation plan (live INTEGRATION connection — **v1 locked**)

**Authority:** [`docs/D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md) § D4, [`docs/E2_PROTOCOL.md`](E2_PROTOCOL.md) E2-06  
**Prerequisites:** D1 ✅, D2 ✅, D3 ✅, G1/G2 ✅, Phase C locked  
**Estimate:** 3–5 h  
**Status:** 🔒 **v1 locked** (2026-07-07) — paused before implementation (AGENTS.md Planning/Implementation Gate)

##### Proof obligation (what D4 must prove)

> **D4 proves that the production subscriber can produce valid non-scoring INTEGRATION diagnostic envelopes while proving that no production-path execution can emit scoring authority artifacts.**

D3 proved: observers can **observe**. D4 proves: observers can observe the **real system** without **becoming authority**. D4 is **containment**, not only presence.

Prior tiers proved: C2 INTEGRATION envelope on **fixtures** · C5 equivalence under **test-pinned** config · D1–D3 passive consumers. D4 proves **live production wiring** under `integrationDefaults()` without contaminating STRICT benchmark authority.

##### One sentence

> **D4 connects INTEGRATION to production; it does not introduce INTEGRATION.** Prove valid E2-06 envelopes on the real subscriber/plugin path while STRICT remains the sole scoring authority.

##### “Live plugin path” (canonical definition — locked)

Use this phrase consistently across D4 Steps 2–3.

> **Live plugin path:** Production initialization through `BasicAgentPlugin` using normal registration and `integrationDefaults()`, executed in a test harness rather than deployed runtime.

Synonym in protocol text: **live production path** — same meaning; prefer **live plugin path** in plans and tests.

| In scope | Out of scope |
|----------|--------------|
| Live plugin path (above) | Deployed service traffic or external user runtime |
| `integrationDefaults()` with test config seam **unset** | Claiming INTEGRATION object equality with STRICT |
| Organic warm tier, cross-session, heuristics per service config | Changing `resolveEvaluation()` or Phase B export |

This prevents scope creep into “production ops” or deployment validation.

##### Containment contract (locked — all containment tests use this)

**Containment** means the live plugin path output satisfies **all** of:

| Rule | Requirement |
|------|-------------|
| `official_scoring` | `false` |
| `e2_outcome` | **absent** on diagnostic authority path |
| STRICT authority metadata | **absent** (e.g. `wiring_stage: "B"` with official scoring, Phase B export authority fields) |
| Benchmark authority fields | **absent** (`evaluation_resolution` used as verdict, lift, promotion labels, etc.) |
| Envelope class | **Diagnostic only** — E2-06 INTEGRATION envelope |

Every containment test checks this same contract. Do not invent per-test forbidden lists.

**Presence vs absence (proof ladder):**

| Class | Proves |
|-------|--------|
| **Presence** | `scoring_tier: INTEGRATION`; expected diagnostic metadata; E2-06 required fields present |
| **Containment (absence)** | Containment contract above — nothing from the absence column may appear |

Step 2 tests **separate** presence proofs from containment proofs (distinct test functions).

##### What C + D3 proved vs what D4 must add

| Already proven | D4 must add |
|----------------|-------------|
| C2: INTEGRATION envelope on fixture (`testE2C2IntegrationEnvelope`) | **Live** path: plugin/subscriber registration → goal or publication → INTEGRATION envelope |
| C5: Equivalence under test-pinned config | Production defaults (`integrationDefaults()`) — C5 explicitly defers this |
| D3: Metrics/trace observe without authority | Evaluation subscriber observes **real** execution without emitting scoring authority |
| D0: INTEGRATION mode defined | INTEGRATION **connected** to operational execution path |

**Code starting point:** `EvaluationSubscriber` uses `E2EvalConfig::integrationDefaults()` when test seam unset; `BasicAgentPlugin` registers on `enable_episodic_evaluation_publication`.

##### E2-06 contract (required on every INTEGRATION artifact)

| Required | Forbidden |
|----------|-----------|
| `scoring_tier`: `"INTEGRATION"` | `official_scoring: true` on production live path |
| `official_scoring`: `false` | `e2_outcome` on diagnostic authority path |
| Diagnostic trace fields only | INTEGRATION-vs-STRICT promotion comparisons |

##### No protocol changes (locked — AGENTS.md Protocol Lock Rule)

D4 **implements** locked E2 v1.2 INTEGRATION semantics — it does **not** revise them.

| Forbidden during D4 | If ambiguity found |
|---------------------|-------------------|
| Silent edits to `E2_PROTOCOL.md`, `D_PHASE_PROTOCOL.md`, `C_PHASE_PROTOCOL.md` tier definitions | **Pause** — request explicit approval per AGENTS.md Planning/Implementation Gate |
| Changes to `resolveEvaluation()`, Phase B export, or STRICT harness contract | Protocol bump (E2 v1.3+) + separate approval — not D4 scope |
| Redefining INTEGRATION/STRICT modes in code without protocol alignment | Stop; fix plan or protocol first |

Implementation may add tests and minimal wiring only; protocol documents change only with explicit human approval.

##### Preregistered tests

| ID | Proves |
|----|--------|
| **E2-D4-01** | Live plugin path — E2-06 presence + containment contract; `integrationDefaults()` behavioral negative proof |
| **E2-D4-02** | STRICT authority preservation audit — `wiring_stage=B` benchmark authority unchanged |

##### Implementation order (pause between steps)

| Step | Work | Gate (proposed) |
|------|------|-----------------|
| **1** | Production wiring seam confirmation — structural audit only (§ D.4.0 Step 1) | `THOTH_E2_D4_STEP1=1` |
| **2** | **E2-D4-01** — live plugin path: presence + containment + `integrationDefaults()` behavioral negative proof | `THOTH_E2_D4_01=1` |
| **3** | **E2-D4-02** — STRICT authority preservation audit | `THOTH_E2_D4_02=1` |
| **4** | Targeted regressions — `THOTH_E2_D3=1`, `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` | Backward compat |
| **5** | Umbrella `THOTH_E2_D4=1` (full D4 proof suite) | D4 close-out |
| **6** | **Pause for review** before D5 |

**Verification scope (Steps 2–5):** Targeted env gates only — full suite / G2 deferred to **D5** unless explicitly requested (same discipline as D3).

##### D.4.0 Step 1 — Production wiring seam confirmation (**v1 locked**)

**Status:** 🔒 **LOCKED FOR IMPLEMENTATION** (2026-07-07) — ✅ **Step 1 complete** (2026-07-07) — paused before Step 2 (E2-D4-01).

**Planning artifact only:** Step 1 planning produces **no source modifications**. Repository working tree must remain unchanged until implementation approval. (D4 tests the governance process as well as the architecture.)

###### Step 1 question (locked boundary)

> **Step 1 answers: “Is INTEGRATION already wired the way D4 assumes?”**

D4 is **not** “add integration support.” D4 is **prove existing integration support is correctly connected.**

| Step 1 proves (structural) | Step 1 does **not** prove (deferred) |
|----------------------------|--------------------------------------|
| Production wiring seam exists and is flag-gated | E2-D4-01 live envelope on plugin path |
| `integrationDefaults()` is the production subscriber config selection | Executive goal run with publication ON |
| No authority confusion in production init paths | `wiring_stage=B` fingerprint / STRICT authority preservation (Step 3) |
| Verified seam inventory for D5 evidence | INTEGRATION ≡ STRICT equivalence or promotion suitability |

**Proof ladder:** Step 1 = *does the code select the expected config?* · Step 2 = *does the running path produce the expected envelope?*

###### Step 1 forbidden (locked)

- Establish **equivalence** between INTEGRATION and STRICT (scoring parity, ranking equivalence, promotion suitability, object equality claims) — C5 addressed equivalence under **controlled pinned config**; D4 Step 1 does **not** reopen that question  
- Live-path behavioral envelope proof (Step 2)  
- STRICT harness / fingerprint runs (Step 3)  
- Protocol document edits without explicit approval  
- Deployed-traffic or external-user “live ops” validation  
- Any production subscriber initialization path may **implicitly select `strictDefaults()`** when operating under the integration production configuration (`integrationDefaults()` / test seam unset) — tests may legitimately use `strictDefaults()`; the prohibition is **authority confusion**, not the existence of the function  

###### Proposed tests (`THOTH_E2_D4_STEP1=1`)

| Test | Purpose |
|------|---------|
| `testE2D4Step1ConfigDefaultOff` | `enable_episodic_evaluation_publication` defaults OFF (may assert via `testE2C2PublicationDisabledByDefault`) |
| `testE2D4Step1ConfigJsonRoundTrip` | Flag round-trips via `saveToJson` / `loadFromJson` |
| `testE2D4Step1IntegrationDefaultsContract` | `integrationDefaults()` → `INTEGRATION` tier, `officialScoring()==false`, cross-session + heuristics enabled |
| `testE2D4Step1PluginStructuralAudit` | Flag-gated `registerEvaluationSubscriber`; telemetry flag only when eval ON; no test config seam in plugin |
| `testE2D4Step1ProductionOnlyRegistrationPath` | `registerEvaluationSubscriber` only in plugin + subscriber definition `.cpp` |
| `testE2D4Step1SubscriberConfigurationSelectionAudit` | **Structural:** subscriber source selects `integrationDefaults()` when test seam unset; no production init path implicitly selects `strictDefaults()` under integration production configuration |
| `testE2D4Step1ExecutivePublicationGate` | Executive publication gated on `enable_episodic_evaluation_publication` only |
| `testE2D4Step1TestSeamIsolation` | `setEvaluationSubscriberEvalConfigForTests` not called from production init / plugin |

**Regression dependency (call, do not duplicate):** `testE2C2IntegrationEnvelope()` — fixture E2-06 baseline.

**Orchestrator:** `runE2D4Step1Tests()` · gate `THOTH_E2_D4_STEP1=1` in `main()`.

###### Step 1 implementation discipline

- Tests only unless structural audit finds a **real** violation  
- No new plugin test seam required (deferred to Step 2)  
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D4_STEP1=1` only  

###### Step 1 evidence artifact (for D5 chain)

On green gate, Step 1 produces:

1. **D4 Step 1 gate result** — `THOTH_E2_D4_STEP1=1` pass  
2. **Structural audit summary** — which greps/audits ran  
3. **Verified seams list** — e.g. config flag, plugin registration, `integrationDefaults()` selection, Executive publication gate, test-seam isolation  
4. **Deferred proof obligations** — explicitly listed for Steps 2–3 (live envelope, STRICT authority preservation audit)  

###### Step 1 exit criteria

1. Plan locked in `cursor_list.md` § D.4.0 Step 1 (this section) — committed before implementation  
2. `THOTH_E2_D4_STEP1=1` green after implementation approval  
3. Build green  
4. Production wiring seam confirmed structurally (minimal production fix only if audit fails)  
5. **Pause for review** before Step 2 (E2-D4-01)  

###### Step 1 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | Step 1 tests + `runE2D4Step1Tests()` + gate |
| `external/basic_agent/*` | **Only if audit finds violation** — default: none |

##### D.4.0 Step 2 — E2-D4-01 live plugin path (**v1 locked**)

**Status:** 🔒 **LOCKED FOR IMPLEMENTATION** (2026-07-07) — paused before implementation (AGENTS.md gate)

###### Core invariant (why Step 2 exists)

> **Step 2 proves that the production path can emit diagnostic INTEGRATION artifacts without acquiring scoring authority.** It does **not** compare, rank, or promote those artifacts relative to STRICT.

If D3 proved observers can observe, Step 2 proves **production can emit diagnostic observations while remaining outside the scoring authority chain.**

###### Step 2 question (locked boundary)

> **Step 2 answers: “On the live plugin path, does the running system produce the expected E2-06 presence **and** satisfy the containment contract?”**

| Step 2 proves (behavioral) | Step 2 does **not** prove (deferred) |
|----------------------------|--------------------------------------|
| **Presence** — INTEGRATION tier, diagnostic metadata, E2-06 required fields | STRICT authority preservation (Step 3) |
| **Containment** — containment contract (§ above) | INTEGRATION ≡ STRICT equivalence or promotion |
| **Behavioral negative** — `integrationDefaults()` with test seam unset; no STRICT config injected | Full D4 umbrella / D5 regression |
| Live plugin path (canonical definition § above) | Deployed / external-user runtime |

**Proof ladder:** Step 1 = config selection · **Step 2 = presence + containment + integrationDefaults negative proof on live plugin path**

###### Step 2 forbidden (locked)

- Establish **equivalence** between INTEGRATION and STRICT  
- Inject STRICT config via `setEvaluationSubscriberEvalConfigForTests` on live-plugin-path tests  
- STRICT harness / fingerprint assertions (Step 3)  
- Protocol document edits without explicit approval  
- Rewriting subscriber behavior because tests are “awkward” — see production-change rule below  

###### Proposed tests (`THOTH_E2_D4_01=1`)

**Harness:** `E2D4PluginWorkspaceGuard` — temp workspace, `enable_episodic_evaluation_publication=true`, other subscriber flags OFF; `THOTH_WORKSPACE_PATH`, `THOTH_TEST_SUITE_DEV=1`, `THOTH_MOCK_LLM=true`; construct `BasicAgentPlugin` → `episodeEventChannelForTests()` → publish fixture.

| Test | Class | Purpose |
|------|-------|---------|
| `testE2D4_01LivePluginPathPresence` | **Presence** | After live plugin path publish: `lastSummaryForTests()` has `INTEGRATION` tier, `official_scoring == false`, expected diagnostic metadata |
| `testE2D4_01LivePluginPathJsonlPresence` | **Presence** | `episodicLearningSummaryLogRow` with INTEGRATION envelope — required E2-06 fields present |
| `testE2D4_01LivePluginPathContainment` | **Containment** | Summary + JSONL satisfy **containment contract** (§ above) — absence proofs only |
| `testE2D4_01IntegrationDefaultsBehavioralNegative` | **Negative** | `setEvaluationSubscriberEvalConfigForTests(std::nullopt)` before plugin construct; **only** changing nothing except leaving seam unset yields `integrationDefaults()` behavior — no STRICT config ever injected |

**Regression dependencies (call, do not duplicate):** `runE2D4Step1Tests()` · `testE2C2IntegrationEnvelope()` (fixture baseline).

**Orchestrator:** `runE2D4_01Tests()` · gate `THOTH_E2_D4_01=1` in `main()`.

###### Step 2 implementation discipline

- Tests only first  
- **Production changes are permitted only to correct verified production wiring defects discovered by the behavioral proof** — not to reshape subscriber semantics for test convenience  
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D4_01=1` + `THOTH_E2_D4_STEP1=1`  
- **Not** full suite / G2 (deferred to D5)  

###### Step 2 evidence artifact

1. `THOTH_E2_D4_01=1` pass  
2. Live plugin path exercised (plugin registration → channel → subscriber)  
3. Presence proof — E2-06 fields present  
4. Containment proof — containment contract satisfied  
5. Behavioral negative — `integrationDefaults()` path confirmed, no STRICT injection  
6. **Deferred:** Step 3 STRICT authority preservation audit  

###### Step 2 exit criteria

1. Plan locked in § D.4.0 Step 2 — committed before implementation  
2. `THOTH_E2_D4_01=1` green after implementation approval  
3. `THOTH_E2_D4_STEP1=1` regression green  
4. Build green  
5. **Pause for review** before Step 3  

###### Step 2 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | E2-D4-01 tests + `runE2D4_01Tests()` + gate |
| `evaluation_subscriber.*` / `basic_agent_plugin.cpp` | Only for **verified wiring defects** found by behavioral proof |

##### D.4.0 Step 3 — E2-D4-02 STRICT authority preservation audit (**outline — lock at Step 2 close**)

**Purpose (preferred name):** **STRICT authority preservation audit** — proves STRICT benchmark authority is **preserved** when D4 wiring is present. (“Contamination” is useful internally; “authority preservation” states the proof obligation.)

**Question:** With live plugin path wiring enabled, is `wiring_stage=B` / Phase B benchmark authority unchanged?

| Proves | Gate |
|--------|------|
| Golden / official harness outcomes unchanged (eval publication ON vs OFF) | `THOTH_E2_D4_02=1` |
| Phase B fingerprint or scoped equivalence snapshot stable | |
| No INTEGRATION authority leaking into STRICT artifacts | |

**Forbidden:** STRICT score promotion; INTEGRATION-vs-STRICT comparison as improvement.

**Deferred detail:** Full step plan locked at Step 2 close-out (same discipline as Step 1 → Step 2).

##### D4 Step 5 exit criteria (umbrella gate)

1. `THOTH_E2_D4=1` green — full D4 proof suite (Steps 1–4)  
2. E2-D4-01: presence + containment on live plugin path; `integrationDefaults()` behavioral negative proof  
3. E2-D4-02: STRICT authority preserved; `wiring_stage=B` fingerprint stable  
4. Post–D3 regressions green with flags default OFF where applicable  
5. **Pause for review** before D5

##### Forbidden (D4)

- `official_scoring: true` from production live subscriber path  
- Emitting `e2_outcome` on INTEGRATION diagnostic authority path  
- INTEGRATION-vs-STRICT promotion or lift claims  
- D4 changes to `resolveEvaluation()` or Phase B export contract  
- Protocol document changes without explicit approval (see **No protocol changes**)  
- Deployed-traffic or external-user “live ops” validation  

##### Files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | E2-D4-01..02 + `runE2D4Tests()` + gates |
| `basic_agent_plugin.cpp` | Wiring audit only if gap found |
| `evaluation_subscriber.*` | Minimal if envelope gap found |
| `docs/cursor_list.md` | § D.4.0 (this section) |
| `docs/D_PHASE_PROTOCOL.md` | Pointer to D.4.0 only if approved |

| Untouched | |
|-----------|--|
| `E2_PROTOCOL.md` tier semantics | Locked v1.2 unless E2 v1.3+ approved |
| `resolveEvaluation()`, Phase B export | Authority frozen |
| STRICT harness / `wiring_stage=B` fingerprint | Must remain stable |
| D3 subscribers | Contract frozen |

**Status:** 🔒 **v1 locked** (2026-07-07). **D4 Step 1 ✅** — **D4 Step 2 locked** (§ D.4.0 Step 2) — paused pending AGENTS.md approval.

### Separation debt (acknowledged)

STRICT / INTEGRATION share eval types and schema → **behavioral separation**, not fully structural. Acceptable for v1.2; future hardening (kernel-only harness binary, versioned ABI) deferred post–Phase C.

### Key files

| File | Role |
|------|------|
| `docs/E2_PROTOCOL.md` | Preregistered protocol + gate priority + scope limits |
| `external/basic_agent/include/episodic_learning_eval.h` | Schema |
| `external/basic_agent/src/e2_strict_enforcement.cpp` | Pin validation, fingerprint |
| `external/basic_agent/src/e2_strict_retrieval.cpp` | Deterministic retrieval |
| `external/basic_agent/src/run_episodic_learning_benchmark.cpp` | Harness (migration target) |
| `tests/unit_tests.cpp` | E2-01–E2-07 (+ E2-08–E2-11 planned) |

**Run (current, non-authoritative until Phase B):** `./build/debug/external/basic_agent/run_episodic_learning_benchmark`

---

## How to use

1. Work **top to bottom** — finish internals before UI polish or self-building.
2. After code changes: `cmake --build --preset build-debug` and `ctest --output-on-failure`.
3. Mark items done only when implemented **and** logged in `completed_improvements_log.md`.
4. When docs conflict: **code** → `audit.md` + `completed_improvements_log.md` → specs (`GRAG.md`, `PLAN.md`, …) → `improvements.md` → narrative (`README.md`, Zenodo papers).

---

## What's solid (do not re-implement)

| Area | Status |
|------|--------|
| GRAG directional core + graph learning | ✅ |
| Executive loop, reflection, scientific mode, strategy promotion | ✅ |
| `executeLLM` (real synthesis, not stub) | ✅ 2026-06-27 |
| Headless `run_test_suite` TC-01–TC-07 | ✅ full ~40 min (2026-06-27); **dev ~10s** (2026-06-26) |
| Dev cognitive harnesses (reflection A/B, robustness suite) | ✅ 2026-06-28 |
| Developer & CI latency (**C4**) | ✅ dev tier + CI tiers + `ctest -L fast` |
| Runtime latency (**C7**) | ✅ batch embed, synthesis cap, parallel RETRIEVAL + prefetch |
| Robustness & failure tests (**C5**) | ✅ `run_robustness_suite` 10/10 |
| Reflection A/B measurement (**C3**) | ✅ 2026-06-26 |
| Production plan templates (RETRIEVAL → LLM) + plan-reuse strip | ✅ 2026-06-27 |
| Plan history reuse + observability events | ✅ |
| Chat RAG pipeline (observability + 5/5 benchmark + grounded Q&A) | ✅ 2026-06-27 |
| Per-goal cognitive metrics (`logs/cognitive_metrics.jsonl`) | ✅ 2026-06-27 |
| C1 planner context management (budgets, validator, memory hygiene) | ✅ 2026-06-27 |
| `allow_shell_exec` tool gating | ✅ |
| Memory consolidation (M2 age-based policy) | ✅ 2026-06-29 |
| Trajectory $w_t$ in scoring path | 🔶 config 0.2; mixed lift on trajectory-disambiguation cases |
| Unit tests | ✅ full suite green (~78s, 2026-06-16) |
| Doc alignment P2.1–P2.6 | ✅ |

---

## Active backlog

### 0. Cognitive loop hardening — **work now**

End-to-end goal execution works; next focus is **quality, speed, and evidence** — not wiring.

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **C1** | **Improve planning quality** | ✅ | Phases 1–5 shipped — see **§ C1**. Goal execution only; chat path separate (**C2**). |
| **C2** | **Improve retrieval ranking** | ✅ | Phase 0–3 complete — see **§ C2**. |
| **C3** | **Measure reflection outcomes** | ✅ | Headless A/B harness — see **§ C3**. `max_reflections` 0 vs 2; timeout skip; `logs/reflection_ab_benchmark.jsonl`. |
| **C4** | **Developer & CI latency** | ✅ | Dev tier + CI wiring — see **§ C4**. PR: `ctest -L pr`; dev: `ctest -L fast` 3/3 (~70s). Nightly: `test-suite-full` with Ollama. |
| **C5** | **Robustness & failure tests** | ✅ | `run_robustness_suite` — 10 mock cases; `logs/robustness_suite.jsonl`. See **§ C5**. |
| **C6** | **Cognitive metrics** | ✅ | Phase 1–2 complete — logging, summarize + plot scripts, token counts, GUI export. See **§ C6**. |
| **C7** | **Runtime latency** | ✅ | Phase 1–3 complete — batch embeddings, synthesis cap, parallel RETRIEVAL + prefetch. See **§ C7**. |

**Dependencies:** C3 benefits from C1/C2 (reflection currently retriggers on failed LLM/timeouts, not just bad answers). **C4** and **C7** are independent — do not mix mock-fast-CI work with runtime optimization. **C6** should start early (append-only logging) and deepen as C1–C7 land — metrics make every subsequent tuning iteration measurable.

> **Cognate C1–C5, C7 implementation write-ups** — ✅ complete, archived: see `cursor_list_archive.md` § C1

#### C6 — Cognitive metrics

Move beyond pass/fail: record **quantitative metrics for every goal execution**, persist them append-only, and aggregate over hundreds of runs.

| Phase | Task | Status |
|-------|------|--------|
| **1** | **Append-only per-goal logging** — `GOAL_COGNITIVE_METRICS` → `logs/cognitive_metrics.jsonl` on `PLAN_COMPLETED` / `PLAN_FAILED` / `PLAN_ABORTED`. | ✅ |
| **2** | **Analysis tooling** — `scripts/summarize_cognitive_metrics.py`; `scripts/plot_cognitive_metrics.py` (matplotlib); token counts from `LLMInterface`; GUI export (Benchmarks → Export Cognitive Metrics, JSONL/CSV). | ✅ |
| **3** | **Longitudinal analysis** — time-series over many runs: trend success/latency/tokens; segment by `plan_reused`, strategy injection, pre/post consolidation; answer “is the system improving?” | 📋 | See **§ Reflection**; expert + analysis consensus |

**Per-goal fields (Phase 1 schema):**

| Metric | Purpose |
|--------|---------|
| `planning_time_ms` | LLM plan generation latency |
| `retrieval_time_ms` | RETRIEVAL step wall time |
| `llm_synthesis_time_ms` | LLM step wall time |
| `retrieved_chunk_count` | Context volume delivered to synthesis |
| `grag_alpha` | Directional blend at retrieval (from GRAG diagnostics) |
| `trajectory_score` | ExecutiveController outcome score |
| `reflection_count` | Reflection replan cycles consumed |
| `max_reflections` | Configured reflection limit for the goal |
| `reflection_skip_reason` | Why reflection was suppressed (`timeout_failure`, `reflection_disabled`, etc.) |
| `final_success_score` | Plan history / past_plans success signal |
| `total_tokens` | LLM token usage (plan + synthesis; Ollama/OpenAI or char estimate for mocks) |
| `planning_tokens` / `synthesis_tokens` | Split at last planning boundary |
| `total_wall_clock_ms` | Goal start → PLAN_COMPLETED/FAILED |

**Questions metrics should answer over time:**

- Is planning getting faster?
- Is retrieval quality improving (alpha, chunk count, downstream success)?
- Does reflection trigger less often after tuning?
- Did a change increase average latency or token cost?

**Implementation notes:**

- **Partial data today:** `StepMetricsRepository`, `grag_benchmark.jsonl`, `decision_trace.jsonl`, `GragMetricsLogger` — now supplemented by unified per-goal rows.
- **Phase 1 (✅):** `logs/cognitive_metrics.jsonl` — `GOAL_COGNITIVE_METRICS` on `PLAN_COMPLETED` / `PLAN_FAILED` / `PLAN_ABORTED`; fields below.
- **Phase 2 (✅):** `summarize_cognitive_metrics.py`, `plot_cognitive_metrics.py`, `LLMInterface` session token tracking, GUI export (JSONL/CSV).
- **Consumers:** C3 A/B runs, C4/C7 regression checks, `benchmark_results.md` archive, thesis/paper figures.

---

### 1. Verification & audit refresh

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **V1** | Re-run `TEST_SUITE.md` TC-01–TC-07 | ✅ | **Headless:** `run_test_suite` ✅ 2026-06-27; **GUI:** ✅ 2026-06-29 — see **`TEST_SUITE_GUI_CHECKLIST.md`** |
| **V2** | Refresh `audit.md` | ✅ | 2026-06-18 — shell gate fixed, portable paths, verification checklist |
| **V3** | Re-upload **`MYPAPER.md`** to Zenodo | ⏸️ | Deferred — more benchmark runs first (see **C2**) |

---

### 2. Phase 3 — Memory stability (finish internals)

**Phase status:** 🔶 Partial — `improvements.md` Phase 3

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **M1** | Memory consolidation (warm tier) | ✅ | **Verified** 2026-06-26 — **`memory_architecture.md`**; M1.5 gate passed |
| **M1.5** | Episodic verification gate | ✅ | **`episodic_memory_benchmark.md`** — E2E retrieval, failure injection, latency, negative cases |
| **M2** | Pruning: age-based trigger | ✅ | Policy-driven consolidation — `memory` config, `ConsolidationDecision`, Clock |
| **M3** | Pruning: admin `/prune` command | ✅ | Manual trigger + `DecisionTraceLogger`; `getConsolidationStatus` / `runConsolidation` API |
| **M4** | Pruning: `MemoryPruner::restore(session_id, range)` | 📋 | On-demand historical replay; transactional SQLite |
| **M5** | Vector store benchmark scaffold | 📋 | Step 3.4: ingest/latency/memory contract tests at 10k/50k/100k chunks; dual-write stub (disabled) |

**Done already (don't redo):** `FactStore`, `MemoryPruner` hot-tier auto-prune, `IVectorStore` / `FlatVectorStore` wrapper.

---

### 3. Phase 4 — Advanced reasoning & GRAG

**Phase status:** 🔶 Partial — `improvements.md` Phase 4

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **G1** | Trajectory tuning (Step 4.5) | 🔶 | Overlaps **C2** — $w_t=0.2$ active; trajectory-disambiguation bucket **−0.037** in one run |
| **G2** | Hierarchical subgoal trees (Step 4.4) | 📋 | `GoalNode`, active subgoal embedding in `GragScorer`; single root $G$ today |
| **G3** | Model upgrade path (Step 4.7) | 📋 | Audit `LLMInterface` for model-specific assumptions; `ModelConfig` + migration playbook |
| **G4** | Trace replay vs SQLite resume | 🔶 | Document-only acceptable: trace is observability; `resume_from_plan()` is authoritative |

**Done already:** Scientific mode, `StrategyEngine`, `GraphRefiner`, `TrajectoryBuilder` wiring.

---

### 4. Benchmarks, eval & automated testing

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **B1** | Research-paper corpus: 30 hardened cases | 📋 | `new_corpus_tests.md`; feeds **V3** Zenodo |
| **B2** | Automate critical manual suite signals | ✅ | `run_test_suite.cpp` + `check_baseline.py` (2026-06-27); **C5** extends coverage |
| **B3** | Reduce test log noise | 📋 | Repeated embedding migration log per fixture |
| **B4** | Compiler warnings (~14 on debug build) | 📋 | Unused params in stubs/GUI |
| **E1** | Benchmark environment pinning | ✅ | A–E complete 2026-07-01 — **`docs/benchmark_environment.md`** |
| **E2** | Episodic memory learning eval | 🔶 | Protocol v1.2 + eval kernel ✅; harness still on runtime path — **§ E2** for checkpoint plan (`docs/E2_PROTOCOL.md`) |
| **E3** | Strategy impact / SCR harness | 📋 | Automated SCR or plan-structure proxy in nightly/CI; `COGNATE_V2.md` metric → regression JSONL |
| **G1d** | Trajectory bucket diagnostic | 🔶 | Harness wired — `run_trajectory_ablation_benchmark`; Ollama 30-case run + decision pending — **`docs/trajectory_ablation_benchmark.md` v1.0** |

---

### 5. NODE execution harness

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **N1** | Decide: implement or defer | 📋 | `NODE.md` + `build_node.md` are spec-only; no `/core/node` in repo |
| **N2** | If defer: one-line status in `README.md` / `NODE.md` | 📋 | Ensure nothing claims NODE is operational |

**Default recommendation:** defer until Phase 3–4 memory/GRAG items above are closed.

---

### 6. UI polish (after core stable)

From `ui_improvements.md` §11–§12 — research console shell exists; these are enhancements:

| Priority | Item |
|----------|------|
| 1 | Tool output renderers (`summarize_text` first) |
| 2 | Step Execution debug mode (manual step → observe) |
| 3 | Retrieval Inspector chunk drill-down + alpha/magnitude color cues |
| 4 | Execution Trace unified timeline |
| 5 | GRAG vector visualizer + interactive plan graph |
| 6 | GUI sync test harness (backend writes → panel assertions) |

---

### 7. Self-building — last (optional future expansion)

**Do not start until Tier 2–4 are in good shape.** Phase 5 in `improvements.md` is **not scheduled**.

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **S1** | `code_modify` / `apply_diff` | 🚫 stub | Returns prototype error; read + `project_analyze` + `run_tests` work |
| **S2** | Self-correction loop in production goals | 📋 | Harness exists; not a closed autonomous edit-verify loop |
| **S3** | Phase 5 close-out criteria | 📋 | See `improvements.md` Phase 5 — only after S1–S2 |

---

### 8. Future cognitive expansion — research horizon

**Do not start until C1–C7 are complete and Phase 3–4 internals are in good shape.** These are the next layer of capability — beyond fixing context management and measuring what exists today. Spec detail belongs in `improvements.md` when any item is promoted to active work.

| ID | Direction | Status | Notes |
|----|-----------|--------|-------|
| **F1** | **Better planning heuristics** | 📋 | Move past scaffold adherence: goal decomposition, cost/risk-aware step ordering, dynamic RETRIEVAL depth, domain-aware plan templates. Builds on **C1** (validator + budgets) and **G2** (subgoal trees). |
| **F2** | **Smarter reflection strategies** | 📋 | Selective replan by failure type (timeout vs wrong answer vs tool error), not only low trajectory score. Separate reflection prompts and memory channel (started in **C1**). Measure first via **C3**, then implement targeted strategies. |
| **F3** | **Richer episodic memory** | 📋 | Full episodic store: searchable trajectories, summarize-before-archive warm tier, restore/replay. Procedural vs episodic channel separation for planner vs conversation vs reflection. Overlaps **M1–M4**, **C1** memory channels. |
| **F4** | **Long-term learning** | 📋 | Cross-session competence: strategy promotion over weeks, forgetting/decay, graph edge consolidation, plan-pattern libraries that improve without polluting prompts. Extends `StrategyEngine`, `GraphRefiner`, **C6** time-series metrics. |
| **F5** | **More sophisticated trajectory scoring** | 📋 | Semantic trajectory embeddings (not stub/zero), step-sequence similarity, outcome-weighted $T$, better disambiguation on trajectory bucket. Overlaps **G1**, **C2**, `TrajectoryBuilder`. |
| **F6** | **Multi-agent collaboration** | 📋 | Delegate subgoals to specialized agents/roles, coordination protocol, shared memory boundaries. No implementation today — architecture TBD (`PLAN.md`, `cognate.md`). |
| **F7** | **Better tool selection** | 📋 | Goal-aware tool ranking from `StepMetricsRepository` success rates, strategy hints in planner, reduce unnecessary TOOL steps in non-corpus modes. Extends `ToolRegistry`, `StrategyEngine`, **F1**. |
| **F8** | **Curriculum generation** | 📋 | Auto-generate eval/training goals from corpus gaps, progressive difficulty, regression suites for planner/retrieval/reflection. Feeds **B1**, **C5**, **C6**; supports thesis/paper benchmark expansion. |

**Suggested dependency order (when promoted):** F3/F5 (memory + trajectory signal) → F1/F7 (planning + tools) → F2 (reflection) → F4 (long-term learning) → F8 (curriculum/eval automation) → F6 (multi-agent, highest architectural lift).

---

### 9. Strategic review — external reflection 2026-06-29

#### Architectural milestone acknowledgment

During C1–C7 and M1–M2, Thoth crossed an architectural threshold. The project now has a coherent cognitive architecture with automated verification and policy-driven memory management. Future milestones are no longer about inventing core infrastructure — they are about demonstrating measurable improvements in cognition over time. That is a fundamentally different and more mature problem.

#### The honest gap

Thoth is in a strong engineering position and a weaker evidence position. The cognitive loop works, memory consolidates, and the docs tell the truth about limits. What does not exist yet is proof that the system learns over time — which is the thesis claim that strategy promotion and episodic memory are meant to support.

Three independent reviewers reading the same codebase converged on the same diagnosis: instrumentation (C6) and component tests (C3–C5) are solid, but the longitudinal question has not been asked. That convergence is a strong signal this is the right place to invest next.

#### The three testing tiers (do not conflate)

| Tier | Examples | Valid claims |
|------|----------|--------------|
| **Fast mock CI** | `--dev`, C3/C5, reflection A/B | Mechanism fires, no crash, bounded retries |
| **Slow Ollama truth** | nightly `--full`, chat RAG 5/5 | Retrieval quality, real synthesis |
| **Accumulated multi-session** | C6 Phase 3 over weeks of runs | System gets better — the learning claim |

The third tier does not exist yet. It is the missing bridge between "it works" and "it gets better." Building it is the primary remaining research obligation.

#### Consolidated priority order (all reviewers)

| Step | Item | Rationale |
|------|------|-----------|
| **1** | **M3** — `/prune` admin command | Closes operational memory loop; unblocks debugging and consolidation demos |
| **2** | **E1** — Environment pinning | ✅ Complete 2026-07-01 — see `docs/benchmark_environment.md` |
| **3** | **E2** — Kernel wiring migration | Phase 0 (pin bug) → A1–A5 sub-checkpoints → Phase B re-baseline. See **`cursor_list.md` § E2** + **`docs/E2_PROTOCOL.md`**. STRICT tests declared-episode retrieval, not organic consolidation (M1.5 covers that). |
| **4** | **M4** — `MemoryPruner::restore(session_id, range)` | Built into already-verified eval environment. Foundation F3 needs; do not parallel-track with E2 |
| **5** | **G1 diagnostic** — Trajectory scoring ablation | `w_t=0` vs `0.2` vs empty T ablation is cheap. Tells you tune vs drop vs redesign before touching anything else in retrieval. Do not promote F5 until this completes |
| **6** | **E3** — SCR in CI | Wire Strategy Conformance Rate into continuous benchmark. Makes strategy promotion a regression signal not a one-off paper figure |
| **7** | **C6 Phase 3** — Accumulated multi-session analysis | Longitudinal analysis tooling over weeks of Ollama/GUI runs. Establishes the third testing tier |
| **8** | **B1** — 30 hardened corpus cases | Run under pinned E1 environment. Defensible benchmark claims for V3 |
| **9** | **V3** — Zenodo re-upload | Only after B1 + E1 pinned runs. Do not re-publish stale or unreproducible numbers |
| **10** | **F-series** — Chosen by evidence | F1/F3 are highest-leverage next capability layer. Do not promote until E2/E3 data identifies the actual bottleneck — retrieval, decomposition, or tool choice |

#### What to avoid

- **Zenodo V3 before B1 and E1 are complete** — re-publishing unreproducible numbers weakens the paper
- **F5 before G1 diagnostic completes** — risk of amplifying noise in the trajectory signal
- **Bulk F-series promotion** — the horizon list is correctly deferred; eval data should drive the order
- **Parallel-tracking M4 and F3** — M4 is the foundation F3 needs

#### Missing definition (open question to resolve)

The roadmap correctly defers F-series until eval data points the way, but no promotion criteria are defined. Before starting E2/E3, document: how many sessions, what delta in SCR or nDCG, and what threshold triggers promotion of the first F-series item. Without this the eval layer has no exit condition.

---

## Suggested session order

```
Done    Headless TEST_SUITE TC-01–TC-07 (2026-06-27)
Done    executeLLM + plan templates + workflow_engine fixes (2026-06-27)
Done    V2 — audit.md refreshed (2026-06-18)
Done    B2 — run_test_suite + check_baseline (2026-06-27)

Done    C2 Phase 0 — chat RAG observability (logs/chat_rag.jsonl)
Done    C2 Phase 1 — golden corpus benchmark (run_chat_rag_benchmark)
Done    C2 Phase 2 — conversational retrieval tuning (5/5 hit@1)
Done    C2 Phase 3 — chat pipeline fixes (grounding, tool gating, truncation) — user validated ✅
Done    C1 phases 1–5 — planner context management (pushed 379c0c5)
Done    C6 Phase 1 — cognitive metrics logging (`logs/cognitive_metrics.jsonl`)
Done    C3 — reflection A/B measurement (2/2 cases, mean lift 0.5)
Done    C4 Phase 1 — run_test_suite --dev (~10s, mock LLM + TfIdf + cached index)
Done    C4 Phase 2 — CI tiers (PR fast / nightly full Ollama)
Done    C7 Phase 1–2 — hot-path wins + summarize_cognitive_metrics.py
Done    C7 Phase 3 — parallel RETRIEVAL dispatch + dependency prefetch
Done    C5 — robustness suite (10 cases, logs/robustness_suite.jsonl)
Done    C6 Phase 2 — plot script, LLM token counts, GUI export
Done    V1 — manual GUI TEST_SUITE TC-01–TC-07 (2026-06-29, observability confirmed)
Done    M1.5 — episodic verification (E2E retrieval, failure inject, latency, benchmark) ✅ 2026-06-26
Done    M2 — age-based consolidation policy (config, Clock, structured decisions) ✅
Done    E1 — benchmark environment pinning (Checkpoints A–E, 2026-07-01)
Done    E2 Phase 0 + A1–A5 — kernel wiring migration ✅ 2026-07-02
Done    E2 Phase B — STRICT re-baseline (B1–B6) ✅ 2026-07-04
Done    E2 Phase C — integration tier (C1–C5) ✅ 2026-07-05
Done    E2 Phase D1 — event channel fan-out ✅ 2026-07-05
Done    E2 Phase D2 — replay subscriber + D2-03/FLAKE-UT-02 ✅ 2026-07-07
Done    E2 Phase D3 — observability proof suite (Steps 1–6, `THOTH_E2_D3=1`) ✅ 2026-07-07
Done    E2 Phase D4 Step 1 — production wiring seam confirmation (`THOTH_E2_D4_STEP1=1`) ✅ 2026-07-07
Next 1  **E2 Phase D4 Step 2** — E2-D4-01 live plugin path (§ **D.4.0 Step 2** locked; await explicit implementation approval)
Next 3  C6 Phase 3 + E3 — longitudinal metrics; SCR harness
Next 4  M4 — range restore (M3 ✅)
Next 5  B1 (if V3 Zenodo) — hardened research corpus
Later   F3/F1 — when eval identifies bottleneck (§ Reflection)
Later   Tier 6 UI polish
Last    Tier 7 self-building / apply_diff (owner discretion)
Horizon Tier 8 future cognitive expansion (F1–F8; see §8)
External V3 — Zenodo MYPAPER re-upload when benchmark corpus stable (C2 ✅; E1 ✅)
```

**GitHub (2026-07-01):** Thoth workspace + Basic_agent submodule — E1 Checkpoints D3–D5 harness wiring pushed; **Checkpoint E** (double-bind, Python tooling, close-out) implemented locally.

---

## Quick reference — partial vs not started

| Item | Today |
|------|-------|
| End-to-end cognitive loop (RETRIEVAL → LLM → PLAN_COMPLETED) | ✅ headless 2026-06-27 |
| Headless TEST_SUITE 7/7 | ✅ dev ~10s (`--dev`); full ~40 min with Ollama |
| Manual TEST_SUITE (GUI) | ✅ 2026-06-29 — `TEST_SUITE_GUI_CHECKLIST.md` |
| Chat RAG observability (C2 Phase 0) | ✅ `logs/chat_rag.jsonl` |
| Golden chat retrieval benchmark (C2 Phase 1) | ✅ `run_chat_rag_benchmark` — baseline 2/5 hit@1 |
| Conversational retrieval tuning (C2 Phase 2) | ✅ 5/5 hit@1 on golden corpus |
| Chat pipeline fixes (C2 Phase 3) | ✅ user-validated grounded Q&A |
| Planning quality (C1) | ✅ phases 1–5 shipped |
| Chat / retrieval quality (C2) | ✅ phases 0–3; user-validated grounded Q&A |
| Per-goal cognitive metrics (C6) | ✅ logging + summarize/plot scripts + tokens + GUI export |
| Reflection A/B measurement (C3) | ✅ `run_reflection_ab_benchmark` — 2/2 cases |
| Developer / CI latency (C4) | ✅ complete — `ctest -L fast` 3/3 (~70s); PR `ctest -L pr`; nightly `--full` |
| Runtime latency (C7) | ✅ complete — Phases 1–3 (embed batch, synthesis cap, parallel RETRIEVAL + prefetch) |
| Robustness test coverage (C5) | ✅ `run_robustness_suite` 10/10 |
| Future cognitive expansion (F1–F8) | 📋 research horizon — §8 |

---

## Doc map (where to read)

| Need | File |
|------|------|
| **E2 protocol + implementation checkpoints** | **`cursor_list.md` § E2**, **`docs/E2_PROTOCOL.md`** |
| **Completed E2/Cognate checkpoint history** | **`cursor_list_archive.md`** (verbatim archive; active work in **`cursor_list.md`**) |
| **E1 benchmark environment** | **`docs/benchmark_environment.md`** |
| Full phase specs | `improvements.md` |
| What's actually shipped | `completed_improvements_log.md` |
| Honest gaps + external review | `audit.md` §5 |
| **Reflection & next priorities** | **`cursor_list.md` § Reflection & analysis** |
| GRAG implementation truth | `GRAG.md`, `benchmark_results.md` |
| Cognate / executive truth | `cognate.md`, `PLAN.md` |
| GRAG paper (Zenodo) | `MYPAPER.md` |
| Cognate paper (Zenodo) | `COGNATE_V2.md` |
| Manual test protocol | `TEST_SUITE.md`, `TESTING.md` |
| UI backlog | `ui_improvements.md` §11–§12 |

---

*Update this file when starting or finishing a tier. Append summaries to `completed_improvements_log.md`; do not mark phases complete in `improvements.md` until close-out criteria pass.*
