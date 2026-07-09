# Thoth Working Backlog

**Last updated:** 2026-07-09 (Phase E — **EP-01.5 plan revised** · Step 2 on investigation hold · Step 3 blocked)  
**Purpose:** Active todo list for the next development sessions. Specs live in `improvements.md`; finished work is logged in `completed_improvements_log.md`.

**Workflow gate:** All checkpoint work in this file follows the Planning/Implementation Gate in AGENTS.md — plan and stop, wait for explicit approval, then implement.

**Active E2 work:** ⚠️ **Step 2 UNDER INVESTIGATION HOLD** — EP-01.5 (authoritative LLM wiring) required before Step 2 redo; Step 3 blocked.

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
| **E2-D5** | Evolution trust proof — meta-proof over composed D surface | [`D5_PROTOCOL.md`](D5_PROTOCOL.md); `THOTH_E2_D5=1` + sub-gates; `PHASE_D_COMPLETE.md` |

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

| Step | Proof type | Gate (proposed) |
|------|------------|-----------------|
| **1** | Structural seam proof | `THOTH_E2_D4_STEP1=1` |
| **2** | Live INTEGRATION behavior proof (**E2-D4-01**) | `THOTH_E2_D4_01=1` |
| **3** | STRICT authority preservation proof (**E2-D4-02**) | `THOTH_E2_D4_02=1` |
| **4** | Backward compatibility proof | `THOTH_E2_D4_STEP4=1` |
| **5** | Composition proof | `THOTH_E2_D4=1` |
| **6** | **Pause for review** before **D5** (evolution trust proof) |

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

**Status:** 🔒 **LOCKED FOR IMPLEMENTATION** (2026-07-07) — ✅ **Step 2 complete** (2026-07-07) — paused before Step 3 (E2-D4-02).

###### Core invariant (why Step 2 exists)

> **Step 2 proves that the production path can emit diagnostic INTEGRATION artifacts without acquiring scoring authority.** It does **not** compare, rank, or promote those artifacts relative to STRICT.

If D3 proved observers can observe, Step 2 proves **production can emit diagnostic observations while remaining outside the scoring authority chain.**

###### Step 2 question (locked boundary)

> **Step 2 answers: “On the live plugin path, does the running system produce the expected E2-06 presence **and** satisfy the containment contract?”**

| Step 2 proves (behavioral) | Step 2 does **not** prove (deferred) |
|----------------------------|--------------------------------------|
| **Presence** — INTEGRATION tier, diagnostic metadata, E2-06 required fields | STRICT authority preservation (Step 3) |
| **Containment** — containment contract (§ above) | INTEGRATION ≡ STRICT equivalence or promotion |
| **Behavioral negative** — `integrationDefaults()` with test seam unset; no STRICT config injected | D4 composition proof / **D5** evolution trust proof |
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

##### D.4.0 Step 3 — E2-D4-02 STRICT authority preservation audit (**v1 locked**)

**Status:** 🔒 **LOCKED** (2026-07-08) — ✅ **Step 3 complete** (2026-07-08) — comparator contract locked § Scoped equivalence — paused before Step 4.

###### Core invariant (why Step 3 exists)

> **Observational infrastructure shall be observationally transparent to the authoritative execution path.**

> **Step 3 proves that connecting INTEGRATION to production does not alter STRICT benchmark authority.** D4 wiring may coexist with the official harness; it must not change `wiring_stage=B` scored outcomes, Phase B fingerprints, or official authority envelopes.

Step 2 proved INTEGRATION can observe without acquiring authority. Step 3 proves **STRICT authority is preserved** when that wiring is present — not that INTEGRATION is “as good as” STRICT.

###### Scoped equivalence (canonical definition — locked)

**Comparator contract:** `episodicLearningScopedEquivalenceEqual` determines whether **benchmark authority is unchanged**. It is intentionally **not** a full-object equality comparison. It answers exactly one question: **did the benchmark authority change?** Everything else — diagnostic envelopes, metadata, timestamps, transport, observability — is validated by other tests.

**Scoped equivalence** is the E2-28 comparison over `episodicLearningScopedEquivalenceSnapshot()` — **not** full summary or JSONL row equality.

**Comparator (authoritative):** `episodicLearningScopedEquivalenceEqual(a, b)` — deep structural equality of the snapshot JSON objects (`nlohmann::json::operator==`). Preservation and determinism tests must use this function; no ad-hoc field diffs.

**Minimal by design:** The comparator intentionally includes only the **minimum** set of fields necessary to determine benchmark-authority equivalence. Any additional field requires an **explicit justification** before being added (prevents comparator creep).

**Separation of concerns (locked):**

| Question | Verified by |
|----------|-------------|
| Did STRICT produce the same authoritative result? | Scoped comparator (`episodicLearningScopedEquivalenceEqual`) |
| Is the envelope still an official STRICT envelope? | Step 3 presence/isolation tests |
| Did diagnostics leak into authority? | D4 isolation tests (Step 2 containment + Step 3 isolation) |

**Included in scoped snapshot** (benchmark authority fields — minimum necessary set):

| Field | Content | Why included |
|-------|---------|--------------|
| `case_resolutions[]` | Per case: `case_id`, `evaluation_resolution` (when set) | Per-case verdict — core Phase B authority |
| `scorable_cases` / `not_scorable_cases` | Rollup counts | Scoring eligibility — affects rollup semantics |
| `summary_evaluation_resolution` | Present when summary rollup resolution is set | Run-level verdict |
| `fingerprint_hash` | From `computeEvaluationFingerprint()` for pinned STRICT config | Anchors verdict to the config that produced it |
| `e2_eval_config` | Canonical config JSON for pinned STRICT config | Without both fingerprint + config, identical verdicts under a different evaluation configuration could falsely appear equivalent |

**Excluded from scoped snapshot** (allowed to differ without failing preservation):

| Category | Examples | Why excluded |
|----------|----------|--------------|
| Run attribution | `timestamp_ms`, `run_id`, `env_hash` | Identity of a run, not semantic scoring outcome |
| Diagnostic / observational side-channel | INTEGRATION subscriber summaries, diagnostics, telemetry | Observation layer — not scored-loop authority |
| Non-authority scoring detail | Per-case `lift`, `passes`, `failure_reason`, arm observations, retrieval provenance | Supporting detail; `evaluation_resolution` is the authority field |
| Export-only rollups not in snapshot | `not_scorable_by_reason`, `success_rate`, `mean_episodic_lift` | Export/diagnostic rollups — not in E2-28 contract |
| Envelope / tier labels | `wiring_stage`, `scoring_tier`, `official_scoring` on JSONL rows | See below |
| Ordering / wall-clock | Log line ordering, `wall_clock_ms`, debug metadata | Serialization and performance noise |

**Envelope fields (`wiring_stage`, `scoring_tier`, `official_scoring`):** These are intentionally validated by **dedicated presence/isolation tests** rather than the scoped comparator, to keep **semantic authority comparison independent from envelope validation**. Exclusion here is by design — not omission.

**Determinism invariant (E2-28):** Two consecutive identical `B` harness builds with the same D4 wiring state must produce scoped snapshots for which `episodicLearningScopedEquivalenceEqual` returns **true** (diagnostic bucket **#0**). This is **deep structural equality** on the E2-28 snapshot object — the locked comparator. It is **not** a byte-compare of full JSONL log rows or raw `dump()` strings; it is authoritative JSON value equality on the included fields only.

**Preservation invariant:** Scoped snapshot with eval publication ON must be **deep-equal** to baseline (publication OFF) under the same pinned `makeE2StrictTestConfig()` — observational infrastructure must not change authority-relevant fields.

###### Step 3 question (locked boundary)

> **Step 3 answers: “With D4 eval publication wiring enabled, is `wiring_stage=B` benchmark authority unchanged?”**

| Step 3 proves | Step 3 does **not** prove (deferred) |
|---------------|--------------------------------------|
| **STRICT presence** — official harness envelope fields (`wiring_stage=B`, `official_scoring=true`, golden rollup) | INTEGRATION presence/containment (Step 2) |
| **Authority preservation** — scoped `B` snapshot matches baseline with eval publication ON | INTEGRATION ≡ STRICT equivalence or promotion |
| **Isolation** — no INTEGRATION/diagnostic authority on STRICT official artifacts | D4 composition proof / **D5** evolution trust proof |
| **Determinism** — two identical `B` builds with D4 wiring produce identical scoped snapshots | Deployed / external-user runtime |
| D4 workspace + channel publication during golden harness runs | Production code changes without verified wiring defect |

**Proof ladder:** Step 1 = config selection · Step 2 = INTEGRATION presence + containment · **Step 3 = STRICT authority preserved under D4 wiring**

###### STRICT authority preservation contract (locked — all Step 3 preservation/isolation tests use this)

**STRICT authority preserved** means the official harness path satisfies **all** of:

| Rule | Requirement |
|------|-------------|
| `wiring_stage` | `"B"` on official harness envelope |
| `official_scoring` | `true` on STRICT authority path |
| Golden rollup | `evaluation_resolution` present on official golden trio summary where Phase B expects SCORED_SUCCESS |
| Scoped equivalence | `episodicLearningScopedEquivalenceEqual(baseline_snap, wired_snap)` **true** — deep structural equality on E2-28 snapshot (§ Scoped equivalence above) when eval publication is ON during harness runs |
| Fingerprint determinism | Two consecutive identical `B` harness builds with D4 wiring → `episodicLearningScopedEquivalenceEqual(snap_a, snap_b)` **true** (E2-28 bucket #0) |
| No INTEGRATION leak | Official STRICT JSONL row: `scoring_tier != "INTEGRATION"`, `official_scoring != false` |
| Side-channel isolation | Episode / replay JSON from channel fan-out lacks STRICT authority fields (`episodeJsonLacksStrictAuthorityFields`) — same contract as E2-D2-02 |

Every preservation/isolation test checks this same contract. Do not invent per-test forbidden lists.

**Presence vs preservation vs isolation (proof ladder):**

| Class | Proves |
|-------|--------|
| **Presence (STRICT authority)** | Official envelope fields present on `wiring_stage=B` path |
| **Preservation (equivalence)** | E2-28 scoped snapshot deep-equal to baseline when D4 eval publication ON (`episodicLearningScopedEquivalenceEqual`) |
| **Isolation (absence)** | No INTEGRATION/diagnostic authority on STRICT official artifacts; channel side-paths lack authority fields |

Step 3 tests **separate** presence, preservation, and isolation (distinct test functions).

###### Step 3 forbidden (locked)

- Claim INTEGRATION ≡ STRICT or promotion suitability  
- Use INTEGRATION-vs-STRICT comparison as improvement evidence  
- Re-open C5 equivalence under pinned config  
- Modify `resolveEvaluation()`, Phase B export, or `E2_PROTOCOL.md` tier semantics  
- Inject `integrationDefaults()` into the **official scored loop** — STRICT harness must remain `makeE2StrictTestConfig()` / `strictDefaults()` pins only  
- Full unit-test suite / G2 ctest (deferred to D5)  
- Rewriting harness or subscriber because tests are awkward — see production-change rule below  

###### Harness (locked)

**Primary harness:** extend existing golden harness helpers — do **not** duplicate `buildOfficialGoldenSummary()`.

| Helper | Role |
|--------|------|
| `buildOfficialGoldenSummary()` | Baseline — no episode publication (reuse) |
| `episodicLearningScopedBSnapshot()` | Scoped `B` snapshot (reuse) |
| `buildOfficialGoldenSummaryWithChannelHarness(bool replay)` | Executive + channel + `EvaluationSubscriber` during arms (reuse from E2-D2-02) |
| `E2D4PluginWorkspaceGuard` | Temp workspace with `enable_episodic_evaluation_publication=true`, other subscriber flags OFF (reuse from Step 2) |
| `setenv("THOTH_E2_WIRING_STAGE", "B", 1)` | Official harness stage — **required** for all Step 3 tests; unset in guard destructor |

**D4 wiring variant:** `buildOfficialGoldenSummaryWithD4EvalPublicationHarness()` — same as `buildOfficialGoldenSummaryWithChannelHarness(false)` (publication ON, replay OFF) **while** `E2D4PluginWorkspaceGuard` is active (workspace config proves D4 flags loaded). Executive channel still uses `registerEvaluationSubscriber` path (production registration function, not test-only stub).

**Behavioral negative obligation:** eval publication ON must **not** change the E2-28 scoped snapshot vs baseline — if `episodicLearningScopedEquivalenceEqual` fails, STRICT authority is **not** preserved (observational infrastructure is not transparent).

###### Proposed tests (`THOTH_E2_D4_02=1`)

| Test | Class | Purpose |
|------|-------|---------|
| `testE2D4_02StrictOfficialEnvelopePresence` | **Presence** | Golden summary under `wiring_stage=B`: official JSONL row has `wiring_stage=="B"`, `official_scoring==true`, `evaluation_resolution` present; `scoring_tier=="STRICT"` on summary |
| `testE2D4_02ScopedEquivalencePreservedWithEvalPublication` | **Preservation** | `episodicLearningScopedEquivalenceEqual(baseline_snap, publication_snap)` (D2-02 discipline; may delegate to `testE2D2BenchmarkAuthorityIsolation` without duplicating) |
| `testE2D4_02ScopedEquivalencePreservedWithD4Workspace` | **Preservation** | Same deep-equal scoped snapshot with `E2D4PluginWorkspaceGuard` active during harness build |
| `testE2D4_02StrictFingerprintDeterminismWithD4Wiring` | **Preservation** | Two consecutive D4-wiring harness runs → `episodicLearningScopedEquivalenceEqual(snap_a, snap_b)` (E2-28 bucket #0) |
| `testE2D4_02NoIntegrationLeakIntoStrictArtifacts` | **Isolation** | Official STRICT row/envelope has no INTEGRATION tier; subscriber side-channel during harness publish produces INTEGRATION summary **separate from** official STRICT rollup; captured episode JSON lacks authority fields |

**Regression dependencies (call, do not duplicate):** `runE2D4_01Tests()` · `testE2D2BenchmarkAuthorityIsolation()` (or thin wrapper if already green).

**Orchestrator:** `runE2D4_02Tests()` · gate `THOTH_E2_D4_02=1` in `main()`.

###### Step 3 implementation discipline

- Tests only first  
- Reuse `buildOfficialGoldenSummary*`, `episodicLearningScopedBSnapshot`, `E2D4PluginWorkspaceGuard` — no parallel harness framework  
- **Production changes are permitted only to correct verified production wiring defects discovered by the behavioral proof** — not to reshape harness or subscriber for test convenience  
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D4_02=1` + `THOTH_E2_D4_01=1`  
- **Not** full suite / G2 (deferred to D5)  

###### Step 3 evidence artifact

1. `THOTH_E2_D4_02=1` pass  
2. `THOTH_E2_WIRING_STAGE=B` exercised with D4 workspace config present  
3. Presence proof — official STRICT envelope fields present  
4. Preservation proof — scoped `B` equivalence unchanged with eval publication ON (+ D4 workspace variant)  
5. Isolation proof — no INTEGRATION authority on STRICT official artifacts  
6. **Deferred:** Step 4 backward compatibility · Step 5 composition proof (`THOTH_E2_D4=1`)  

###### Step 3 exit criteria

1. Plan locked in § D.4.0 Step 3 — committed before implementation  
2. `THOTH_E2_D4_02=1` green after implementation approval  
3. `THOTH_E2_D4_01=1` regression green  
4. Build green  
5. **Pause for review** before Step 4  

###### Step 3 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | E2-D4-02 tests + `buildOfficialGoldenSummaryWithD4EvalPublicationHarness()` + `runE2D4_02Tests()` + gate |
| `external/basic_agent/*` | Only for **verified wiring defects** found by behavioral proof — default: none |

##### D.4.0 Step 4 — backward-compat regressions (**v1 locked**)

**Status:** 🔒 **LOCKED** (2026-07-08) — ✅ **Step 4 complete** (2026-07-08) — paused before Step 5.

###### Core invariant (why Step 4 exists)

> **D4 must not regress prior E2 phase contracts or alter previously verified authority behavior when all new D4 wiring is disabled.**

Steps 1–3 proved D4 wiring is structurally correct, diagnostically contained, and STRICT-authority-preserving **when enabled**. Step 4 proves **prior phases still pass** under their established gates with subscriber/eval flags at safe defaults — i.e. D4 did not break D1, D2, D3, or C5, and did not disturb authority behavior verified before D4 wiring is activated.

Step 4 is **regression only** — no new proof obligations, no new preregistered test IDs.

###### Step 4 question (locked boundary)

> **Step 4 answers: “With D4 code present but eval/subscriber flags at defaults, do D1–D3 and C5 still green?”**

| Step 4 proves | Step 4 does **not** prove (deferred) |
|---------------|--------------------------------------|
| **D3 regression** — full D3 proof suite (`THOTH_E2_D3=1`) | D4 Steps 1–3 obligations (already proven) |
| **D2 regression** — replay + authority isolation (`THOTH_E2_D2=1`) | D4 composition close-out (Step 5) |
| **D1 regression** — channel fan-out + invisibility (`THOTH_E2_D1=1`) | Full unit-test suite / G2 (D5) |
| **C5 regression** — path equivalence matrix (`THOTH_E2_C5=1`) | New subscriber or protocol behavior |
| Flags-default-OFF discipline during regression run | Deployed / external-user runtime |

**Proof ladder:** Step 1 = structural seam · Step 2 = live INTEGRATION behavior · Step 3 = STRICT authority preservation · **Step 4 = backward compatibility** · Step 5 = composition proof · D5 = evolution trust proof

###### Default flag contract (locked — regression run discipline)

Regression gates run with **no D4 wiring activated**. The following must remain at **defaults OFF** unless the called regression orchestrator explicitly sets them for its own proof:

| Flag | Default for Step 4 | Rationale |
|------|-------------------|-----------|
| `enable_episodic_evaluation_publication` | **OFF** | D4 eval path inactive — regress prior phases |
| `enable_episodic_pipeline_telemetry` | **OFF** | No telemetry side effects |
| `enable_episode_replay_subscriber` | **OFF** | D2 gate exercises replay on its own terms |
| `enable_metrics_subscriber` | **OFF** | D3 gate exercises metrics on its own terms |
| `enable_trace_subscriber` | **OFF** | D3 gate exercises trace on its own terms |

**Environment discipline:** Step 4 must **not** use the D4 temporary workspace harness with evaluation publication enabled. Orchestrator runs in clean process context; each regression gate owns its own env setup/teardown.

###### Regression contract (locked — all Step 4 gates use this)

| Gate | Orchestrator | Proves |
|------|--------------|--------|
| `THOTH_E2_D3=1` | `runE2D3Tests()` | D3 Steps 1–5 — observability without authority |
| `THOTH_E2_D2=1` | `runE2D2Tests()` | D2-01 replay + D2-02 benchmark authority isolation |
| `THOTH_E2_D1=1` | `runE2D1Tests()` | D1-01..03 channel + invisibility |
| `THOTH_E2_C5=1` | `runE2C5RegressionGate()` | C5-01..05 path equivalence — extract from `main()` only; **no assertion or semantics change** |

**Call, do not duplicate:** Step 4 invokes existing orchestrators — no reimplementation of D1/D2/D3/C5 test bodies.

**D4 Step 3 not re-run in Step 4:** Step 3 is not re-run in Step 4 because authority preservation was already proven and committed at the D4 Step 3 checkpoint; Step 4 is limited to backward-compatibility regression validation. (`runE2D4_02Tests()` is omitted — avoids ~7 min duplicate.) Step 5 composition proof includes D4 Steps 1–4 together.

###### Step 4 forbidden (locked)

- New proof obligations or preregistered test IDs  
- Re-open INTEGRATION ≡ STRICT equivalence or C5 semantic claims beyond existing C5 gate  
- Enable D4 eval publication during regression run  
- Full unit-test suite / G2 `ctest` (deferred to D5)  
- Production code changes — Step 4 is orchestration only  
- Protocol document edits without explicit approval  

###### Proposed work (`THOTH_E2_D4_STEP4=1`)

| Work | Detail |
|------|--------|
| `runE2C5RegressionGate()` | Extract C5 inline block from `main()` into callable helper (6 existing tests). **No C5 assertions or semantics change** — extraction is purely to reuse the existing gate under Step 4 orchestration. |
| `runE2D4Step4Tests()` | Sequential: `runE2D3Tests()` → `runE2D2Tests()` → `runE2D1Tests()` → `runE2C5RegressionGate()` |
| `main()` early-exit | `THOTH_E2_D4_STEP4=1` → `runE2D4Step4Tests()` |

**No new `testE2D4_*` test functions** — Step 4 is pure regression orchestration.

**Orchestrator:** `runE2D4Step4Tests()` · gate `THOTH_E2_D4_STEP4=1` in `main()`.

###### Step 4 implementation discipline

- Orchestration only — extract `runE2C5RegressionGate()` if needed; no new test logic  
- **No production changes** expected  
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D4_STEP4=1` only  
- **Not** `THOTH_E2_D4_02=1` / full D4 suite (Step 5) · **not** full suite / G2 (D5)  
- Estimated wall time: D3 gate dominates (~similar to prior D3 full suite runs)  

###### Step 4 evidence artifact

On green gate, Step 4 records:

1. `THOTH_E2_D4_STEP4=1` pass  
2. `THOTH_E2_D3=1` pass (via `runE2D3Tests()`)  
3. `THOTH_E2_D2=1` pass (via `runE2D2Tests()`)  
4. `THOTH_E2_D1=1` pass (via `runE2D1Tests()`)  
5. `THOTH_E2_C5=1` pass (via `runE2C5RegressionGate()`)  
6. Default flag contract verified — no D4 workspace harness with eval publication enabled  
7. **Conclusion:** no backward-compat regression detected  
8. **Deferred:** Step 5 composition proof (`THOTH_E2_D4=1`)  

###### Step 4 exit criteria

1. Plan locked in § D.4.0 Step 4 — committed before implementation  
2. `THOTH_E2_D4_STEP4=1` green after implementation approval  
3. Build green  
4. **Pause for review** before Step 5  

###### Step 4 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | `runE2C5RegressionGate()` + `runE2D4Step4Tests()` + gate |
| `external/basic_agent/*` | **None** |

##### D.4.0 Step 5 — composition proof (**v1 locked**)

**Status:** 🔒 **LOCKED** (2026-07-08) — ✅ **Step 5 complete** (2026-07-08) — **D4 complete** — paused before **D5** (evolution trust proof).

###### D4 proof ladder (canonical — locked)

| Step | Proof type | Gate |
|------|------------|------|
| **1** | Structural seam proof | `THOTH_E2_D4_STEP1=1` |
| **2** | Live INTEGRATION behavior proof | `THOTH_E2_D4_01=1` |
| **3** | STRICT authority preservation proof | `THOTH_E2_D4_02=1` |
| **4** | Backward compatibility proof | `THOTH_E2_D4_STEP4=1` |
| **5** | Composition proof | `THOTH_E2_D4=1` |
| **D5** | Evolution trust proof | (D5 phase — deferred) |

###### Core invariant (why Step 5 exists)

> **Each D4 step proved a distinct obligation in isolation. Step 5 proves all D4 obligations still hold together under one gate.**

Steps 1–4 are individually green and committed. Step 5 is the **D4 composition orchestrator** — not new proof logic. It answers: “If I run the full D4 proof ladder once, does everything still pass?”

###### Step 5 question (locked boundary)

> **Step 5 answers: “Under `THOTH_E2_D4=1`, do structural seam, live INTEGRATION behavior, STRICT authority preservation, and backward compatibility proofs all green in one run?”**

| Step 5 proves | Step 5 does **not** prove (deferred) |
|---------------|--------------------------------------|
| **Composition** — Steps 1–4 orchestrators compose without failure | New D4 proof obligations or preregistered test IDs |
| **Live INTEGRATION behavior** — E2-D4-01 presence + containment + `integrationDefaults()` negative (via nested `runE2D4_01Tests()` in Phase A) | Full unit-test suite / G2 `ctest` (**D5** evolution trust proof) |
| **STRICT authority preservation** — E2-D4-02 scoped equivalence stable (via `runE2D4_02Tests()`) | Phase B two-run fingerprint gate (**D5**) |
| **Backward compatibility** — D3, D2, D1, C5 with flags default OFF (via `runE2D4Step4Tests()`) | Deployed / external-user runtime |
| **D4-I1..I7** — collectively satisfied by Steps 1–3 evidence chain | INTEGRATION ≡ STRICT promotion or lift claims |
| Evidence chain recorded for **D5** evolution trust proof | Protocol document edits without explicit approval |

**Proof ladder (complete):** Step 1 = structural seam · Step 2 = live INTEGRATION behavior · Step 3 = STRICT authority preservation · Step 4 = backward compatibility · **Step 5 = composition proof** · D5 = evolution trust proof

###### Orchestration contract (locked — call composition, not duplication)

**Critical rule:** Do **not** naively call all four step orchestrators sequentially. Nested regressions already exist:

| Orchestrator | Already includes |
|--------------|------------------|
| `runE2D4_01Tests()` | Step 1 via `runE2D4Step1Tests()` at end |
| `runE2D4_02Tests()` | Steps 1–2 via `runE2D4_01Tests()` at end |
| `runE2D4Step4Tests()` | D3, D2, D1, C5 only — **no** D4 Steps 1–3 |

**Composition (locked):**

| Phase | Orchestrator | Covers | Gate equivalent |
|-------|--------------|--------|-----------------|
| **A** | `runE2D4_02Tests()` | Steps 1–3 (structural + live behavior + authority preservation) | `THOTH_E2_D4_02=1` |
| **B** | `runE2D4Step4Tests()` | Step 4 (backward compatibility) | `THOTH_E2_D4_STEP4=1` |

```cpp
static bool runE2D4Tests() {
    if (!runE2D4_02Tests()) return false;   // Phase A: Steps 1–3
    if (!runE2D4Step4Tests()) return false; // Phase B: Step 4
    return true;
}
```

**Call, do not duplicate:** Step 5 invokes existing orchestrators only — no reimplementation of Step 1–4 test bodies.

**Order locked:** Phase A (D4 wiring obligations) **before** Phase B (flags-default-OFF backward compatibility). Rationale: prove containment and authority preservation first; then prove prior phases unchanged with wiring disabled.

###### Default flag contract (Step 5 discipline)

| Segment | Flag discipline |
|---------|-----------------|
| Phase A (`runE2D4_02Tests()`) | D4 workspace harness **may** enable `enable_episodic_evaluation_publication` per existing Step 2/3 tests |
| Phase B (`runE2D4Step4Tests()`) | `verifyD4Step4DefaultFlagContract()` runs first — all five subscriber/eval flags **OFF** before D3/D2/D1/C5 regressions |

Step 5 does **not** merge these contexts — each phase owns its env setup/teardown via existing guards.

###### Step 5 forbidden (locked)

- New proof obligations or preregistered test IDs (`E2-D4-03`, etc.)
- New `testE2D4_*` test functions — orchestration only
- Calling `runE2D4Step1Tests()`, `runE2D4_01Tests()`, `runE2D4_02Tests()`, and `runE2D4Step4Tests()` **all four** sequentially (triple-runs Step 1, double-runs Steps 1–2)
- Re-open INTEGRATION ≡ STRICT equivalence or C5 semantic claims beyond existing gates
- Full unit-test suite / G2 `ctest` (deferred to **D5** evolution trust proof)
- Production code changes — Step 5 is harness-only
- Protocol document edits without explicit approval
- Claiming **D5** complete — Step 5 closes **D4** only

###### Proposed work (`THOTH_E2_D4=1`)

| Work | Detail |
|------|--------|
| `runE2D4Tests()` | `runE2D4_02Tests()` → `runE2D4Step4Tests()` + composition evidence artifact |
| `main()` early-exit | `THOTH_E2_D4=1` → `runE2D4Tests()` — **insert before** `THOTH_E2_D4_STEP4` check (composition gate must win over subset gates) |
| Evidence output | Print consolidated pass record for all sub-gates (see below) |
| Deferred strings | Update Step 3/4 `deferred:` lines from composition proof → **D5** evolution trust proof |

**No new test functions.** Step 5 is pure orchestration + evidence printing.

**Orchestrator:** `runE2D4Tests()` · gate `THOTH_E2_D4=1` in `main()`.

###### Step 5 implementation discipline

- Orchestration only — reuse existing `runE2D4_02Tests()` and `runE2D4Step4Tests()`
- **No production changes** expected
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D4=1` **only**
- **Not** default full suite / G2 (**D5** evolution trust proof)
- Estimated wall time: **~12–15 min** (Phase A ~7 min; Phase B ~5 min)
- On failure: stop per AGENTS.md Build/Test Failure Rule

###### Step 5 evidence artifact

On green gate, `runE2D4Tests()` records:

1. `THOTH_E2_D4=1` pass — composition gate  
2. Phase A pass — structural seam + live INTEGRATION behavior + STRICT authority preservation (`THOTH_E2_D4_02=1`)  
3. Phase B pass — backward compatibility (`THOTH_E2_D4_STEP4=1`)  
4. E2-D4-01 obligations satisfied  
5. E2-D4-02 obligations satisfied  
6. D4-I1..I7 satisfied (evidence chain from Steps 1–3)  
7. `THOTH_E2_D3=1`, `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` pass (via Step 4 orchestrator)  
8. Default flag contract verified during Phase B  
9. **Conclusion:** D4 proof suite complete — all obligations compose  
10. **Deferred:** **D5** evolution trust proof (full suite, G2, Phase B fingerprint two-run gate)

###### Step 5 exit criteria

1. Plan locked in § D.4.0 Step 5 — committed before implementation  
2. `THOTH_E2_D4=1` green after implementation approval  
3. Build green  
4. D4 exit criteria 1–7 satisfied (see `D_PHASE_PROTOCOL.md` § D4 Exit criteria)  
5. **Pause for review** before **D5** (evolution trust proof)

###### Step 5 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | `runE2D4Tests()` + `THOTH_E2_D4=1` early-exit (before `THOTH_E2_D4_STEP4`); update deferred strings |
| `docs/cursor_list.md` | § D.4.0 Step 5 (this section) |
| `docs/D_PHASE_PROTOCOL.md` | D4 composition proof + pause before D5 |
| `external/basic_agent/*` | **None** |

###### Relationship to D5 (explicit boundary)

| D4 Step 5 — composition proof (`THOTH_E2_D4=1`) | D5 — evolution trust proof |
|------------------------------------------------|----------------------------|
| D4 Steps 1–4 compose under one gate | Full unit-test suite + G2 `ctest` |
| Targeted env gates only | Phase B fingerprint two-run gate |
| Proves D4 obligations compose | Proves **nothing important changed** after all D changes |
| Closes D4 | Closes Phase D |

##### D4 Step 5 exit criteria (composition gate)

1. `THOTH_E2_D4=1` green — full D4 proof suite (Steps 1–4)  
2. E2-D4-01: presence + containment on live plugin path; `integrationDefaults()` behavioral negative proof  
3. E2-D4-02: STRICT authority preserved; `wiring_stage=B` fingerprint stable  
4. Post–D3 regressions green with flags default OFF where applicable  
5. **Pause for review** before **D5** (evolution trust proof)

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

**Status:** 🔒 **v1 locked** (2026-07-07). **D4 Step 1 ✅** — **D4 Step 2 ✅** — **D4 Step 3 ✅** — **D4 Step 4 ✅** — **D4 Step 5 ✅** — **D4 complete** — paused before **D5** protocol lock.

#### D.5.0 — E2-D5 implementation plan (evolution trust proof — **v1 locked**)

**Authority:** [`docs/D5_PROTOCOL.md`](D5_PROTOCOL.md) v0.1 🔒  
**Prerequisites:** D1 ✅, D2 ✅, D3 ✅, D4 ✅ (`THOTH_E2_D4=1`, commit `d4216c8`)  
**Estimate:** 2–4 h total (orchestration + evidence only — no production changes)  
**Status:** 🔒 **v1 locked** (2026-07-08) — **Step 1 locked** — paused before Step 1 implementation (AGENTS.md gate)

##### Proof obligation (what D5 must prove)

> **D5 proves that accumulated D-phase evolution preserves previously established authority boundaries, deterministic behavior, and benchmark equivalence — without re-proving every lower-level invariant.**

D5 is **evidence composition, not proof regeneration** ([`D5_PROTOCOL.md`](D5_PROTOCOL.md) § Evidence composition rule). D4 built the trust mechanisms; D5 proves they survived evolution.

##### One sentence

> **D5 is a meta-proof seal on Phase D — it verifies the composed proof surface coheres after D1–D4 evolution; it does not introduce runtime behavior or grant promotion authority.**

##### D5 proof ladder (canonical — locked)

| Step | Proof type | Gate | Preregistered ID |
|------|------------|------|------------------|
| **1** | Authority preservation meta-proof | `THOTH_E2_D5_AUTHORITY=1` | E2-D5-03 |
| **2** | Behavioral preservation meta-proof | `THOTH_E2_D5_C5=1` | E2-D5-01 |
| **3** | Determinism preservation meta-proof | `THOTH_E2_D5_DETERMINISM=1` | E2-D5-02 |
| **4** | Phase closure — evidence completeness + seal | `THOTH_E2_D5=1` | (orchestrator) |
| **—** | Preservation, not promotion (interpretive) | — | Invariant 5 (no sub-gate) |

**Verification scope:** Targeted env gates only. Full suite / G2 deferred to optional post-D5 hygiene ([`D5_PROTOCOL.md`](D5_PROTOCOL.md) § Optional post-D5 hygiene).

##### Evidence composition rule (locked — all D5 steps)

| D5 does | D5 does **not** |
|---------|-----------------|
| Consume D1–D4 close-out evidence by attestation | Re-run `runE2D4Tests()`, `runE2D3Tests()`, etc. by default |
| Run D5-specific meta-proofs (Steps 1–3) | Build recursive proof tree D5 → D4 → D3 → D2 → D1 |
| Record phase closure (Step 4) | Regenerate lower-phase behavioral suites |

**Coverage-gap rule:** Lower-phase gates run only when a D5 sub-gate failure identifies a missing invariant **and** the plan is amended with justification.

**Attestation helper naming (locked — all D5 steps):** Evidence printers use `attest*` names that encode **what** is attested, not **who runs** it. They print reference constants only — **never** call lower-step orchestrators (`runE2D5AuthorityMetaProof()`, `runE2D5C5Proof()`, etc.).

| Pattern | Example | Role |
|---------|---------|------|
| `attestD5Step{N}{Domain}Evidence()` | `attestD5Step1AuthorityEvidence()`, `attestD5Step2BehavioralEvidence()` | D5 sub-step close-out attestation |
| `attest{Phase}{ProofId}Evidence()` | `attestPhaseBE2_28Evidence()`, `attestD4CompositionEvidence()` | Prior-phase proof attestation |

**Hygiene note (non-blocker):** Step 2 shipped `attestD5Step1Evidence()` — align to `attestD5Step1AuthorityEvidence()` when Step 3 lands (harness rename only; behavior unchanged). Step 4 closure should follow the same convention for Steps 1–3 attestations.

---

##### D.5.0 Step 1 — authority preservation meta-proof (**v1 locked**)

**Status:** 🔒 **v1 locked** (2026-07-08) — ✅ **Step 1 complete** (2026-07-08) — paused before Step 2.

###### Core invariant (why Step 1 exists)

> **Did any D-phase change accidentally alter who has authority?**

Step 1 is the **constitutional structural audit** at evolution close-out. It consumes D4 composition evidence for D4-specific authority claims and runs only the **narrow audit bundle** not covered by Steps 2–3 (C5 equivalence, Phase B determinism).

###### Step 1 question (locked boundary)

> **Step 1 answers: “After D1–D4 evolution, do authority boundaries still hold — without re-running full D4 composition?”**

| Step 1 proves | Step 1 does **not** prove (deferred) |
|---------------|--------------------------------------|
| D4 composition evidence **attested** (reference only) | Full D4 composition re-run (`runE2D4Tests()`) |
| D3 structural authority boundary (D3-03 bundle) | D3 behavioral suite (`runE2D3Tests()`) |
| D4-02 isolation absence (single test) | Full D4-02 suite (`runE2D4_02Tests()`) |
| D1 Executive structural invisibility (grep only) | D1-03 behavioral 0-vs-N audit (proven at D1 close-out) |
| Passive Consumer Law structural surface (+ D3-01 spot-check via D3-03 authority boundary) | C5 equivalence (Step 2) |
| D2 replay authority — **consumed by reference** (D4 Step 4 attestation) | D2 behavioral re-run (`THOTH_E2_D2=1`) |
| | Cross-layer **service import** coupling (`testE2C5NoHiddenCoupling` in Step 2 — C5 layer audit, not Step 1 authority duplicate) |
| | Phase B determinism (Step 3) |
| | Promotion or INTEGRATION ≡ STRICT claims |

###### Authority meta-proof contract (locked)

**Consume D4 evidence (attestation only — no re-execution):**

| Attested gate | Close-out reference |
|---------------|---------------------|
| `THOTH_E2_D4=1` | D4 composition proof green (2026-07-08, `d4216c8`) |
| E2-D4-01 | Live INTEGRATION containment |
| E2-D4-02 | STRICT authority preservation |
| D4-I1..I7 | Structural + behavioral chain |

**Run structural audit bundle (existing helpers — call, do not duplicate bodies):**

| Helper | Role |
|--------|------|
| `e2D1ExecutiveInvisibilityStructuralAudit()` | Extract Executive grep from `testE2D1ExecutiveInvisibilityAudit()` — Passive Consumer Law §3 symbols only; **no** 0-vs-N behavioral run |
| `runE2D3_03Tests()` | D3-03 structural authority boundary (7 tests). **Includes** `testE2D3_01MetricsSinkOnly()` via `testE2D3_03AuthorityBoundary()` — lightweight spot-check by design, not full D3 suite |
| `testE2D4_02NoIntegrationLeakIntoStrictArtifacts()` | D4-02 isolation absence — single harness test (dominates wall time), not full D4-02 orchestrator |

**Refactor note:** Extract `e2D1ExecutiveInvisibilityStructuralAudit()` as shared static helper; `testE2D1ExecutiveInvisibilityAudit()` calls it then runs behavioral portion. **No new preregistered test ID** — extraction only.

###### Step 1 forbidden (locked)

- Re-run `runE2D4Tests()` or any full D-phase orchestrator  
- Re-run `testE2D1ExecutiveInvisibilityAudit()` behavioral 0-vs-N portion (D1 close-out attestation sufficient)  
- Re-run full `runE2D4_02Tests()`  
- New preregistered test IDs  
- Production code changes — Step 1 is harness-only  
- INTEGRATION ≡ STRICT promotion claims  

###### Proposed work (`THOTH_E2_D5_AUTHORITY=1`)

| Work | Detail |
|------|--------|
| `e2D1ExecutiveInvisibilityStructuralAudit()` | Extract grep-only portion from D1-03 test (shared helper) |
| `attestD4CompositionEvidence()` | Print/verify attestation constants (gate ID, date, commit ref) — **no** `runE2D4Tests()` call |
| `runE2D5AuthorityMetaProof()` | Attest D4 → structural audit bundle → evidence artifact |
| `main()` early-exit | `THOTH_E2_D5_AUTHORITY=1` → `runE2D5AuthorityMetaProof()` |

**Orchestrator:** `runE2D5AuthorityMetaProof()` · gate `THOTH_E2_D5_AUTHORITY=1`.

###### Step 1 implementation discipline

- Harness-only — extract helper + orchestrator; no new proof logic  
- **No production changes** expected  
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D5_AUTHORITY=1` only  
- Estimated wall time: **~1–3 min** (D4-02 isolation harness dominates; D3-03 includes D3-01 spot-check; Executive grep is fast)  
- On failure: stop per AGENTS.md Build/Test Failure Rule  

###### Step 1 evidence artifact

On green gate, Step 1 records:

1. D4 composition evidence attested (reference: `THOTH_E2_D4=1`, `d4216c8`)  
2. `e2D1ExecutiveInvisibilityStructuralAudit()` pass  
3. `runE2D3_03Tests()` pass (includes D3-01 metrics sink-only spot-check via authority boundary)  
4. `testE2D4_02NoIntegrationLeakIntoStrictArtifacts()` pass  
5. **Conclusion:** authority boundaries preserved post-evolution  
6. **Deferred:** Step 2 behavioral preservation · Step 3 determinism · Step 4 closure  

###### Step 1 exit criteria

1. Plan locked in § D.5.0 Step 1 — committed before implementation  
2. `THOTH_E2_D5_AUTHORITY=1` green after implementation approval  
3. Build green  
4. **Pause for review** before Step 2  

###### Step 1 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | `e2D1ExecutiveInvisibilityStructuralAudit()` extract + `runE2D5AuthorityMetaProof()` + gate |
| `external/basic_agent/*` | **None** |

---

##### D.5.0 Step 2 — behavioral preservation meta-proof (**v1 locked**)

**Status:** 🔒 **LOCKED** (2026-07-08) — ✅ **Step 2 complete** (2026-07-08, `f16664d`) — paused before Step 3.

###### Core invariant (why Step 2 exists)

> **Did evolution preserve existing trusted outputs?**

Step 1 proved authority boundaries held. Step 2 proves **behavioral equivalence** on the trusted C5 surface survived D1–D4 evolution — benchmark vs production paths still MATCH under pinned config.

###### Step 2 question (locked boundary)

> **Step 2 answers: “After D-phase evolution, does C5 equivalence still MATCH on mapping-safe fixtures under `makeE2StrictTestConfig()`?”**

| Step 2 proves | Step 2 does **not** prove (deferred) |
|---------------|--------------------------------------|
| C5 mapping-safe equivalence MATCH (**E2-D5-01**) | Full Phase C suite (C1–C4) |
| Path fingerprint stability on pinned config (within C5 gate) | Full D4 composition (`runE2D4Tests()`) |
| Runtime semantic equivalence (`testE2C5SemanticEquivalence`) | Phase B two-run determinism (Step 3) |
| C5 service-layer import coupling (`testE2C5NoHiddenCoupling` — **not** a repeat of Step 1 authority checks) | Executive/subscriber authority audit (Step 1 — attested) |
| | INTEGRATION ≡ STRICT promotion claims |
| | `THOTH_E2_C5_MATRIX=1` evidence printer (optional hygiene) |

**`testE2C5NoHiddenCoupling()` scope (locked):** C5 **service-layer** import-boundary audit (eval/diag/telemetry/path-equivalence modules). Step 1 deferred **cross-layer service import coupling** here — it does **not** duplicate D5 authority preservation (Executive grep, D3 subscriber bundle, D4-02 isolation). Primary behavioral proof in Step 2 is `testE2C5SemanticEquivalence()` + `testE2C5FingerprintStability()`.

###### Behavioral meta-proof contract (locked)

**Consume prior evidence (attestation only — no re-execution):**

| Attested | Reference |
|----------|-----------|
| Phase C C5 gate | `THOTH_E2_C5=1` — committed at Phase C close-out |
| D4 backward-compat C5 pass | D4 Step 4 evidence chain |
| D5 Step 1 authority | `THOTH_E2_D5_AUTHORITY=1` green (`0b4df02`) |

**Run behavioral meta-proof (single orchestrator — call, do not duplicate):**

| Helper | Role |
|--------|------|
| `runE2C5RegressionGate()` | **E2-D5-01** — existing C5 regression bundle (6 tests, unchanged) |

**`runE2C5RegressionGate()` composition (existing — no change):**

| Test | Proves |
|------|--------|
| `testE2C5MappingFidelity()` | Benchmark arms survive production mapper round-trip |
| `testE2C5SemanticEquivalence()` | **Primary behavioral** — benchmark vs production MATCH |
| `testE2C5FingerprintStability()` | Fingerprint stable on pinned config |
| `testE2C5CrossPathArtifactConsistency()` | Cross-path artifact consistency |
| `testE2C5NoHiddenCoupling()` | C5 service-layer import coupling (structural complement) |
| `testE2C5PathEquivalenceGoldenFixtures()` | Golden fixture equivalence (E2-01..03) |

###### Step 2 forbidden (locked)

- Re-run full C1–C4 orchestrators  
- Re-run `runE2D4Tests()`, `runE2D3Tests()`, or any D-phase suite  
- Duplicate C5 test bodies into new `testE2D5_*` functions  
- New preregistered test IDs  
- Production code changes — Step 2 is harness-only  
- Promotion / lift claims in evidence output  

###### Proposed work (`THOTH_E2_D5_C5=1`)

| Work | Detail |
|------|--------|
| `attestD5Step1AuthorityEvidence()` | Print Step 1 authority attestation (reference only) — shared with Step 2 |
| `runE2D5C5Proof()` | Attest → `runE2C5RegressionGate()` → evidence artifact |
| `main()` early-exit | `THOTH_E2_D5_C5=1` → `runE2D5C5Proof()` — after `THOTH_E2_D5_AUTHORITY`, before D4 gates |

**Orchestrator:** `runE2D5C5Proof()` · gate `THOTH_E2_D5_C5=1` · preregistered **E2-D5-01**.

###### Step 2 implementation discipline

- Harness-only — thin wrapper + evidence printing  
- **No production changes** expected  
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D5_C5=1` only  
- Estimated wall time: **~2–5 min** (semantic equivalence + fingerprint stability iterate episodic cases)  
- On failure: stop per AGENTS.md Build/Test Failure Rule  

###### Step 2 evidence artifact

On green gate, Step 2 records:

1. D5 Step 1 attested (`THOTH_E2_D5_AUTHORITY=1`, `0b4df02`)  
2. Phase C C5 attested (consumed by reference)  
3. `runE2C5RegressionGate()` pass — E2-D5-01  
4. Mapping-safe fixtures MATCH (`testE2C5SemanticEquivalence`)  
5. `testE2C5NoHiddenCoupling()` pass — C5 service-layer import coupling (not Step 1 authority duplicate)  
6. **Conclusion:** behavioral equivalence preserved post-evolution (**preservation only — not promotion**)  
7. **Deferred:** Step 3 determinism · Step 4 closure  

###### Step 2 exit criteria

1. Plan locked in § D.5.0 Step 2 — committed before implementation  
2. `THOTH_E2_D5_C5=1` green after implementation approval  
3. Build green  
4. **Pause for review** before Step 3  

###### Step 2 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | `attestD5Step1AuthorityEvidence()` + `runE2D5C5Proof()` + gate |
| `external/basic_agent/*` | **None** |

---

##### D.5.0 Step 3 — determinism preservation meta-proof (**v1 locked**)

**Status:** 🔒 **v1 locked** (2026-07-08) — ✅ **Step 3 complete** (2026-07-08) — paused before Step 4.

###### Core invariant (why Step 3 exists)

> **Did evolution preserve reproducibility of authoritative evaluation?**

Steps 1–2 proved authority and behavioral equivalence held. Step 3 proves **deterministic trust** survived D1–D4 evolution — consecutive identical strict builds still produce equivalent scoped-equivalence snapshots under the canonical E2-28 contract.

###### Step 3 question (locked boundary)

> **Step 3 answers: “After D-phase evolution, do consecutive identical strict builds still produce equivalent scoped-equivalence snapshots per E2-28?”**

| Step 3 proves | Step 3 does **not** prove (deferred) |
|---------------|--------------------------------------|
| Consecutive strict runs are reproducible (**E2-D5-02**) | Behavioral equivalence (Step 2 — attested) |
| Scoped-equivalence snapshots are identical (E2-28) | Authority preservation (Step 1 — attested) |
| Fingerprint stability across two identical builds | Phase closure / evidence completeness (Step 4) |
| Diagnosis bucket stable on golden trio (bucket #0) | Full Phase B suite (`runE2B5Tests()` or B5 battery) |
| Deterministic trust preservation | D4-02 determinism re-run (proven at D4 Step 3 close-out) |
| | Production promotion or runtime correctness claims |
| | INTEGRATION ≡ STRICT equivalence |

###### Determinism meta-proof contract (locked)

**Evidence composition rule:** Step 3 **attests** prior checkpoint evidence and **executes** only the E2-28 determinism helper. It does **not** recursively invoke Step 1 or Step 2 orchestrators.

```
runE2D5DeterminismProof()
 ├─ attestD5Step1AuthorityEvidence()     [print only — no runE2D5AuthorityMetaProof()]
 ├─ attestD5Step2BehavioralEvidence()     [print only — no runE2D5C5Proof()]
 ├─ attestPhaseBE2_28Evidence()           [print only — Phase B E2-28 close-out ref]
 └─ testE2B5OfficialFingerprintDeterminism()   [E2-D5-02 — existing helper, unchanged]
```

**Anti-pattern (forbidden):** `runE2D5DeterminismProof()` → `runE2D5C5Proof()` → `runE2D5AuthorityMetaProof()` — that is recursive orchestration, not evidence composition.

**Consume prior evidence (attestation only — no re-execution):**

| Attested | Reference |
|----------|-----------|
| D5 Step 1 authority | `THOTH_E2_D5_AUTHORITY=1` green (`0b4df02`) |
| D5 Step 2 behavioral | `THOTH_E2_D5_C5=1` green (`f16664d`) |
| Phase B E2-28 determinism | Phase B close-out — `testE2B5OfficialFingerprintDeterminism()` proven at B5; consumed by reference |

**Run determinism meta-proof (single existing helper — call, do not duplicate):**

| Helper | Role |
|--------|------|
| `testE2B5OfficialFingerprintDeterminism()` | **E2-D5-02** — E2-28 scoped equivalence: two `buildOfficialGoldenSummary()` passes → snapshot deep-equal + diagnosis bucket #0 |

###### Gate contract — `THOTH_E2_D5_DETERMINISM=1` (locked)

`THOTH_E2_D5_DETERMINISM=1` indicates that **deterministic evaluation behavior was preserved** across consecutive identical strict builds using the canonical E2-28 proof helper (`testE2B5OfficialFingerprintDeterminism()`). It is a **preservation proof only** — it does **not** imply broader behavioral equivalence (Step 2), authority boundary preservation (Step 1), phase closure (Step 4), or production promotion authority.

On invocation: early-exit in `main()` after `THOTH_E2_D5_C5`, before `THOTH_E2_D5_AUTHORITY` and D4 gates → `runE2D5DeterminismProof()`.

###### Step 3 forbidden (locked)

- Re-run full Phase B suite or `runE2B5Tests()` battery  
- Re-run D4-02 determinism tests (`testE2D4_02*` determinism helpers)  
- Call `runE2D5AuthorityMetaProof()` or `runE2D5C5Proof()` inside Step 3 orchestrator  
- Duplicate E2-28 test bodies into new `testE2D5_*` functions  
- New preregistered test IDs  
- Production code changes — Step 3 is harness-only  
- Promotion / INTEGRATION ≡ STRICT claims in evidence output  

###### Proposed work (`THOTH_E2_D5_DETERMINISM=1`)

| Work | Detail |
|------|--------|
| `attestD5Step1AuthorityEvidence()` | Print Step 1 authority attestation (reference only) |
| `attestD5Step2BehavioralEvidence()` | Print Step 2 behavioral attestation (reference only) |
| `attestPhaseBE2_28Evidence()` | Print Phase B E2-28 close-out attestation (reference only) |
| `runE2D5DeterminismProof()` | Flat attest trio → `testE2B5OfficialFingerprintDeterminism()` → evidence artifact |
| `main()` early-exit | `THOTH_E2_D5_DETERMINISM=1` → `runE2D5DeterminismProof()` — after `THOTH_E2_D5_C5`, before `THOTH_E2_D5_AUTHORITY` |

**Orchestrator:** `runE2D5DeterminismProof()` · gate `THOTH_E2_D5_DETERMINISM=1` · preregistered **E2-D5-02**.

###### Step 3 implementation discipline

- Harness-only — thin wrapper + flat attestation printers + evidence output  
- **No production changes** expected  
- Attestation helpers are **evidence printers only** — they must not invoke lower-step orchestrators  
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D5_DETERMINISM=1` only  
- Estimated wall time: **~30s–2 min** (`buildOfficialGoldenSummary()` × 2 dominates; snapshot compare is fast; no C5 regression gate)  
- On failure: stop per AGENTS.md Build/Test Failure Rule  

###### Step 3 evidence artifact

On green gate, Step 3 records:

1. D5 Step 1 authority attested (`THOTH_E2_D5_AUTHORITY=1`, `0b4df02`)  
2. D5 Step 2 behavioral attested (`THOTH_E2_D5_C5=1`, `f16664d`)  
3. Phase B E2-28 attested (consumed by reference)  
4. `testE2B5OfficialFingerprintDeterminism()` pass — E2-D5-02  
5. Scoped-equivalence snapshots deep-equal across consecutive builds  
6. Diagnosis bucket #0 (equivalent)  
7. **Conclusion:** deterministic trust preserved post-evolution (**preservation only — not promotion**)  
8. **Deferred:** Step 4 closure  

###### Step 3 exit criteria

1. Plan locked in § D.5.0 Step 3 — committed before implementation  
2. Existing E2-28 helper passes (`testE2B5OfficialFingerprintDeterminism()`)  
3. Scoped-equivalence snapshots compare equal  
4. Fingerprint matches across consecutive identical builds  
5. Diagnosis bucket remains #0  
6. Evidence artifact recorded (flat attestation chain — no recursive orchestration)  
7. No new determinism logic introduced  
8. `THOTH_E2_D5_DETERMINISM=1` green after implementation approval  
9. Build green  
10. **Pause for review** before Step 4  

###### Step 3 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | `attestD5Step1AuthorityEvidence()` + `attestD5Step2BehavioralEvidence()` + `attestPhaseBE2_28Evidence()` + `runE2D5DeterminismProof()` + gate |
| `external/basic_agent/*` | **None** |

---

##### D.5.0 Step 4 — phase closure (**v1 locked**)

**Status:** 🔒 **v1 locked** (2026-07-08) — ✅ **Step 4 complete** (2026-07-08) — **Phase D complete** — paused before Phase E.

###### Core invariant (why Step 4 exists)

> **Can we prove Phase D completion — and seal the evolution trust boundary?**

Steps 1–3 proved authority, behavioral equivalence, and determinism individually. Step 4 **composes** those meta-proofs, attests D1–D4 close-out evidence by reference, records the phase seal artifact, and applies Invariant 5 (preservation, not promotion) to the recorded conclusion.

###### Step 4 question (locked boundary)

> **Step 4 answers: “Do all D5 meta-proofs compose, and is the full D1–D5 evidence chain complete enough to seal Phase D?”**

| Step 4 proves | Step 4 does **not** prove (deferred) |
|---------------|--------------------------------------|
| D5 meta-proofs compose sequentially (Steps 1–3) | Individual invariant re-proof in isolation (sub-gates remain for diagnosis) |
| D1–D4 close-out evidence **attested** (reference only) | Full D4 composition re-run (`runE2D4Tests()`) |
| D1–D5 evidence chain completeness (Invariant 4) | Full D3 / D2 / D1 orchestrator re-run by default |
| Phase D trust seal recorded (`PHASE_D_COMPLETE.md`) | Default full unit-test suite / G2 (optional post-D5 hygiene) |
| Invariant 5 conclusion wording (**preservation only — not promotion**) | Phase E scientific defense |
| Safe to proceed to Phase E planning | Production promotion or INTEGRATION ≡ STRICT equivalence |
| | Claiming Phase E complete |

###### Closure meta-proof contract (locked)

**Composition vs recursion (locked distinction):** Step 4 **calls** the three D5 step orchestrators sequentially — this is **intentional meta-proof composition**. It does **not** call `runE2D4Tests()`, `runE2D3Tests()`, `runE2D2Tests()`, or `runE2D1Tests()` — that would be forbidden D-phase proof regeneration.

```
runE2D5Tests()
 ├─ attestD1CloseOutEvidence()              [print only]
 ├─ attestD2CloseOutEvidence()              [print only]
 ├─ attestD3CloseOutEvidence()              [print only]
 ├─ attestD4CompositionEvidence()           [print only — reuse existing helper]
 ├─ runE2D5AuthorityMetaProof()             [Step 1 — E2-D5-03]
 ├─ runE2D5C5Proof()                         [Step 2 — E2-D5-01]
 ├─ runE2D5DeterminismProof()                [Step 3 — E2-D5-02]
 └─ emit closure evidence + Invariant 5 wording
```

**Anti-pattern (forbidden):** `runE2D5Tests()` → `runE2D4Tests()` → `runE2D3Tests()` → … — that is D-phase proof regeneration, not D5 closure.

**Consume D-phase evidence (attestation only — no re-execution):**

| Attested gate | Close-out reference |
|---------------|---------------------|
| `THOTH_E2_D1=1` | D1 channel + invisibility — 2026-07-05 close-out |
| `THOTH_E2_D2=1` | D2 replay + benchmark authority isolation — 2026-07-07 close-out |
| `THOTH_E2_D3=1` | D3 observability without authority — 2026-07-07 close-out |
| `THOTH_E2_D4=1` | D4 composition proof — 2026-07-08 (`d4216c8`) |

**Run D5 meta-proof composition (existing orchestrators — call, do not duplicate):**

| Orchestrator | Role |
|--------------|------|
| `runE2D5AuthorityMetaProof()` | Invariant 1 — authority preservation (E2-D5-03) |
| `runE2D5C5Proof()` | Invariant 2 — behavioral preservation (E2-D5-01) |
| `runE2D5DeterminismProof()` | Invariant 3 — determinism preservation (E2-D5-02) |

**Note:** Step 1 orchestrator internally re-attests D4 (`attestD4CompositionEvidence()`). Closure attests D4 explicitly first for Invariant 4 completeness; duplicate print-only attestation is acceptable.

###### Gate contract — `THOTH_E2_D5=1` (locked)

`THOTH_E2_D5=1` means **all D5 meta-proofs passed** against their declared evidence sources and the **Phase D trust boundary is sealed**. It is a **preservation proof only** — it does **not** mean every historical test suite was rerun, does **not** grant promotion authority, and does **not** establish INTEGRATION ≡ STRICT equivalence.

On invocation: early-exit in `main()` **before** all D5 sub-gates and **before** D4 gates → `runE2D5Tests()`.

###### Step 4 forbidden (locked)

- Re-run `runE2D4Tests()`, `runE2D3Tests()`, `runE2D2Tests()`, or `runE2D1Tests()` inside closure orchestrator  
- Default full unit-test suite / G2 `ctest` (optional post-D5 hygiene only)  
- New preregistered test IDs  
- Duplicate D5 meta-proof bodies into new `testE2D5_*` functions  
- Production code changes — Step 4 is harness + documentation only  
- Promotion / INTEGRATION ≡ STRICT claims in evidence output or `PHASE_D_COMPLETE.md`  
- Claiming Phase E complete  

###### Proposed work (`THOTH_E2_D5=1`)

| Work | Detail |
|------|--------|
| `attestD1CloseOutEvidence()` | Print D1 close-out attestation (reference only) |
| `attestD2CloseOutEvidence()` | Print D2 close-out attestation (reference only) |
| `attestD3CloseOutEvidence()` | Print D3 close-out attestation (reference only) |
| `attestD4CompositionEvidence()` | Reuse existing D4 composition attestation helper |
| `runE2D5Tests()` | Attest D1–D4 → Steps 1–3 orchestrators → closure evidence artifact |
| `main()` early-exit | `THOTH_E2_D5=1` → `runE2D5Tests()` — **before** all D5 sub-gates and D4 gates |
| `docs/phases/PHASE_D_COMPLETE.md` | Phase seal artifact (Invariant 4 + Invariant 5 wording) |

**Orchestrator:** `runE2D5Tests()` · gate `THOTH_E2_D5=1` · preregistered IDs consumed: E2-D5-01, E2-D5-02, E2-D5-03 (via sub-orchestrators).

###### Step 4 implementation discipline

- Harness + documentation only — orchestration + evidence printing + phase seal doc  
- **No production changes** expected  
- Reuse existing D5 step orchestrators and `attestD4CompositionEvidence()` — no new proof logic  
- Verification: `cmake --build --preset build-debug` + `THOTH_E2_D5=1` only  
- Estimated wall time: **~4–7 min** (Step 1 ~1–3 min + Step 2 ~2.5 min + Step 3 ~65s, sequential)  
- On failure: stop per AGENTS.md Build/Test Failure Rule  

###### Step 4 evidence artifact

On green gate, Step 4 records:

1. `THOTH_E2_D1=1` attested (D1 close-out)  
2. `THOTH_E2_D2=1` attested (D2 close-out)  
3. `THOTH_E2_D3=1` attested (D3 close-out)  
4. `THOTH_E2_D4=1` attested (`d4216c8`)  
5. `runE2D5AuthorityMetaProof()` pass — E2-D5-03  
6. `runE2D5C5Proof()` pass — E2-D5-01  
7. `runE2D5DeterminismProof()` pass — E2-D5-02  
8. D5 sub-gate commits referenced: Step 1 (`0b4df02`), Step 2 (`f16664d`), Step 3 (`6dec86b`)  
9. **Conclusion:** evolution trust proof green — Phase D trust boundary sealed (**preservation only — not promotion**)  
10. `docs/phases/PHASE_D_COMPLETE.md` written  
11. **Deferred:** Phase E scientific defense  

###### `PHASE_D_COMPLETE.md` outline (locked)

Mirror [`PHASE_C_COMPLETE.md`](phases/PHASE_C_COMPLETE.md) structure. The seal doc is a **long-lived audit artifact** — it must be sufficient to reconstruct what was sealed months later without re-running gates.

**Mandatory fields (minimum — all required):**

| Field | Requirement |
|-------|-------------|
| **Date** | Phase D completion date (ISO) |
| **Protocol version** | `D_PHASE_PROTOCOL.md` + `D5_PROTOCOL.md` v0.1 🔒 |
| **D1 evidence reference** | Gate `THOTH_E2_D1=1` + close-out date |
| **D2 evidence reference** | Gate `THOTH_E2_D2=1` + close-out date |
| **D3 evidence reference** | Gate `THOTH_E2_D3=1` + close-out date |
| **D4 evidence reference** | Gate `THOTH_E2_D4=1` + close-out commit |
| **D5 evidence reference** | Sub-gates + closure gate (`THOTH_E2_D5_AUTHORITY=1`, `THOTH_E2_D5_C5=1`, `THOTH_E2_D5_DETERMINISM=1`, `THOTH_E2_D5=1`) |
| **Commit hash(es)** | D4 close-out (`d4216c8`); D5 Steps 1–3 (`0b4df02`, `f16664d`, `6dec86b`); Step 4 closure commit (record at implementation) |
| **Final conclusion** | Evolution trust proof green — Phase D trust boundary sealed (**preservation only — not promotion**) |

**Suggested sections (beyond mandatory minimum):**

| Section | Content |
|---------|---------|
| Header | Completed date, protocol authority, status 🔒 Phase D locked |
| Summary table | D1–D4 checkpoint deliverables + D5 meta-proof seal |
| D5 evolution trust record | Sub-gates + closure gate + commit refs (expand mandatory D5 row) |
| Regression matrix | All four D5 env gates green |
| Architectural invariants preserved | Constitutional Rule, Passive Consumer Law, preservation-not-promotion |
| Key files | Harness orchestrators, protocol docs |
| Footer | **Paused before Phase E** |

###### Step 4 exit criteria

1. Plan locked in § D.5.0 Step 4 — committed before implementation  
2. `attestD1CloseOutEvidence()` / `attestD2CloseOutEvidence()` / `attestD3CloseOutEvidence()` added (print only)  
3. `runE2D5Tests()` composes Steps 1–3 without calling D-phase orchestrators  
4. `THOTH_E2_D5=1` early-exit inserted before D5 sub-gates in `main()`  
5. `THOTH_E2_D5=1` green after implementation approval  
6. Build green  
7. `docs/phases/PHASE_D_COMPLETE.md` recorded — **all mandatory seal fields present** (D1–D5 evidence refs, commit hash(es), protocol version, date, final conclusion)  
8. Invariant 5 preserved in evidence wording and phase seal doc  
9. **Pause for review** before Phase E  

###### Step 4 files (expected touch)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | D1–D3 attest helpers + `runE2D5Tests()` + `THOTH_E2_D5=1` gate |
| `docs/phases/PHASE_D_COMPLETE.md` | Phase D close-out artifact (new) |
| `docs/cursor_list.md` | § D.5.0 Step 4 status |
| `docs/D_PHASE_PROTOCOL.md` | D5 complete pointer |
| `external/basic_agent/*` | **None** |

---

##### D5 forbidden (locked — all steps)

- Proof regeneration (recursive D-phase re-run)  
- New preregistered test IDs beyond E2-D5-01..03  
- Production runtime changes  
- Protocol semantics edits  
- Promotion / INTEGRATION ≡ STRICT claims in evidence output  
- Claiming Phase E complete  

##### D5 exit criteria (phase complete)

1. `THOTH_E2_D5_AUTHORITY=1`, `THOTH_E2_D5_C5=1`, `THOTH_E2_D5_DETERMINISM=1` green  
2. `THOTH_E2_D5=1` green — closure orchestrator  
3. `docs/phases/PHASE_D_COMPLETE.md` recorded  
4. Invariant 5 preserved in evidence wording (**preservation only — not promotion**)  
5. **Pause for review** before Phase E  

##### Files (expected touch — full D5)

| File | Change |
|------|--------|
| `tests/unit_tests.cpp` | D5 orchestrators + gates (Steps 1–4) |
| `docs/phases/PHASE_D_COMPLETE.md` | Phase D close-out artifact (Step 4) |
| `docs/cursor_list.md` | § D.5.0 (this section) |
| `docs/D_PHASE_PROTOCOL.md` | D5 complete pointer |
| `external/basic_agent/*` | **None** |

**Status:** 🔒 **v1 locked** (2026-07-08). **Step 1 ✅** — **Step 2 ✅** — **Step 3 ✅** — **Step 4 ✅** — **Phase D complete** — paused before Phase E (empirical validation).

---

### E.0.0 — E2 Phase E (Empirical Validation Tier)

**Authority:** [`docs/E_PHASE_PROTOCOL.md`](E_PHASE_PROTOCOL.md) v0.1 🔒  
**Prerequisite:** Phase D sealed — [`phases/PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md) (`e0a9ba5`)

**Goal:** Establish **empirical confidence** in published benchmark claims — separate from **engineering confidence** (Phase D).

**One sentence:** Phase D proved the evaluator preserved trust; Phase E proves empirical claims made with that evaluator are specification-complete, reproducible, defensible, evidence-mapped, and publication-ready.

##### Phase E proof ladder (canonical — locked)

| Step | Work | Artifact | Primary question |
|------|------|----------|------------------|
| **0** | Protocol lock | `E_PHASE_PROTOCOL.md` 🔒 | — |
| **1** | Analysis plan lock | `phases/E_ANALYSIS_PLAN.md` | E-Q1 |
| **EP-01** | Episodic authoritative inference harness | Live-backend path in `run_episodic_learning_benchmark` | — (engineering prereq) |
| **EP-01.5** | Authoritative LLM wiring + planner contract | Live `LLMInterface` on `--authoritative` + `wiring_stage=B` | — (harness repair; § **E.0.0 EP-01.5**) |
| **2** | Authoritative STRICT runs | [`phase_e_strict_v1.md`](benchmark_results/phase_e_strict_v1.md) ⚠️ hold | E-Q2 (partial), E-Q3 |
| **3** | L4 reproducibility package | Manifests + verification doc | E-Q2 |
| **4** | Claims audit | Claim → evidence tier map | E-Q4 |
| **5** | Phase close-out | `PHASE_E_COMPLETE.md` | E-Q5 |

**Verification scope:** Documentation + pinned authoritative runs for in-scope claims. Full suite / G2 optional post-E hygiene.

##### Current maturity (honest)

| Area | Assessment |
|------|------------|
| Engineering rigor | Strong |
| Reproducibility infrastructure | Strong |
| Benchmark methodology | Good but limited |
| Statistical rigor | Early — inference intentionally deferred |
| Publication readiness | Not yet |

##### Five questions Phase E must answer (locked intent — see protocol)

| ID | Question |
|----|----------|
| **E-Q1** | Is the evaluation protocol completely specified? |
| **E-Q2** | Can an independent lab reproduce every published benchmark? |
| **E-Q3** | Are reported results methodologically defensible? |
| **E-Q4** | Do all published claims map to benchmark evidence? |
| **E-Q5** | Is the project ready for external publication? |

##### Metric definition vs validation (locked distinction)

| Layer | Phase E claim |
|-------|---------------|
| **Definition** | `LIFT_MARGIN`, `evaluation_resolution`, `mean_episodic_lift`, etc. — specified in `E2_PROTOCOL.md` |
| **Validation** | Whether constants are meaningful/optimal/stable at scale — **open**; B1 or v1.3+ |

> Phase E validates **protocol correctness**, not universal optimality of engineering constants.

##### Reproducibility levels

| Level | Status |
|-------|--------|
| L1 Source reproducible | ✅ A–D |
| L2 Environment reproducible | ✅ E1 |
| L3 Benchmark reproducible | ✅ B (v1.2 trio) |
| L4 Publication package reproducible | 📋 Phase E target |

##### Publication targets (separate)

| Target | Question | Status |
|--------|----------|--------|
| **Architecture** | Built correctly? Authority preserved? Deterministic? | Phase D ✅ |
| **Empirical** | Does memory improve learning? How much? Uncertainty? | Phase E 📋 |

##### Threats to validity (document in close-out)

| Threat | Current answer |
|--------|----------------|
| Internal | Yes — STRICT isolates intended variable |
| Construct | Partial — lift ≠ full “learning” construct |
| External | Not yet — n=3 official corpus |
| Conclusion | Descriptive only — inference deferred |

##### Proposed checkpoint ladder

| Step | Work | Delivers |
|------|------|----------|
| **E0** | Lock `E_PHASE_PROTOCOL.md` + § E.0.0 | ✅ 2026-07-09 |
| **E1** | Protocol + analysis plan lock | `phases/E_ANALYSIS_PLAN.md` — E-AP v1.1 ✅ |
| **EP-01** | Episodic authoritative inference harness | Engineering prereq — § **E.0.0 EP-01** ✅ |
| **EP-01.5** | Authoritative LLM wiring + planner contract | Harness repair — § **E.0.0 EP-01.5** 🔒 Phase 1 |
| **E2** | Authoritative STRICT runs (trio; B1 deferred) | `phase_e_strict_v1.md` + manifest — § **E.0.0 Step 2** ⚠️ hold |
| **E3** | L4 reproducibility package | Manifests, verification, baseline compare |
| **E4** | Claims audit | Paper sentence → evidence tier |
| **E5** | Close-out | `PHASE_E_COMPLETE.md` + E-Q1..Q5 seal |

**Time estimate (rough):** E0 ✅ — E1 **3–5 h** — **EP-01 6–12 h** — E2 **4–8 h** + authoritative inference runtime — E3 **2–4 h** — E4 **3–6 h** — E5 **1–2 h**.

**Phase E flow (locked — post Step 1):**

| Step | Role |
|------|------|
| **E1** | Freeze the protocol (what will be measured) |
| **EP-01** | Add episodic harness authoritative inference capability (infrastructure only) |
| **EP-01.5** | Wire live `LLMInterface` so authoritative mode actually invokes inference (harness repair only) |
| **E2** | Execute the frozen protocol (collect evidence) — redo after EP-01.5 |
| **E3** | Assemble the reproducibility package (others can reproduce it) |
| **E4** | Audit every external claim against frozen evidence tiers |
| **E5** | Issue the publication / readiness seal |

**Status:** 🔒 **E0 locked** (2026-07-09). **E1 complete** (2026-07-09). **EP-01 complete** (2026-07-09). **EP-01.5 LOCKED** (2026-07-09) — Phase 1 in progress. **Step 2 UNDER INVESTIGATION HOLD** — Step 3 blocked.

---

##### E.0.0 — Phase E planning format lock (**v1 locked**)

**Status:** 🔒 **LOCKED** (2026-07-09) — applies to **EP-01** and **Steps 2–5** (Step 1 complete under prior format; not rewritten).

**Objective:** Restore the structured planning style used successfully in Phases C and D. Every remaining Phase E step plan must be **reviewable, implementation-independent, and lockable before any work begins** — without inventing a new format.

**Scope:** Documentation and workflow lock only. **No code, no benchmark runs, no harness gates** until a step plan is locked **and** explicit implementation approval is given per AGENTS.md.

###### Mandatory section ordering (locked — every E.0.0 Step N plan)

Each step subsection under § E.0.0 **must** contain these sections **in this exact order** (use `######` headings):

| # | Section | Purpose |
|---|---------|---------|
| 1 | **Step title and status** | Name, version, lock state, completion state |
| 2 | **Objective** | One paragraph — what the step accomplishes |
| 3 | **Core invariant** | Single quoted question — why this step exists |
| 4 | **What this step proves** | Table or bullets — positive proof obligations |
| 5 | **What this step does not prove** | Table or bullets — explicit deferrals to later steps |
| 6 | **Scope** | In / out of scope; planning vs implementation boundary |
| 7 | **Files touched** | Exact paths; **None** for planning-only locks |
| 8 | **Detailed work items** | Numbered or table — implementation tasks when approved |
| 9 | **Dangers / failure modes / things that must not change** | Architectural risks, authority-preservation risks, protocol freeze violations |
| 10 | **Forbidden changes** | Implementation shortcuts that are not allowed |
| 11 | **Exit criteria** | Objective, checkable conditions for step complete |
| 12 | **Deliverables / evidence produced** | Artifacts, manifests, gate results, doc paths |
| 13 | **Dependencies on previous steps** | Required seals, documents, and prior step outcomes |
| 14 | **Pause** | **STOP** — await explicit implementation approval (AGENTS.md gate) |

**Consistency requirements (locked):**

- Identical section ordering for Steps 2–5 — no reordering, no omitted sections  
- Planning **clearly separated** from implementation (`Planning artifact only` when applicable)  
- Every step identifies **exactly which documents** are modified  
- Every step lists **architectural risks** and **authority-preservation risks** in §9  
- Every step lists **forbidden shortcuts** in §10  
- Every step ends with **pause for explicit approval** before implementation  

**Terminology lock (Phase E — backend-neutral):**

| Use | Do not use (Phase E planning/docs) |
|-----|-------------------------------------|
| **authoritative inference tier** | “Ollama tier,” “Ollama-tier runs” |
| **authoritative LLM backend** | Coupling evaluation protocol to a specific provider |
| **pinned authoritative backend** | Implying Thoth requires one vendor |

**Ollama** (or other provider names) appear **only** for historical implementation detail, legacy CI docs, or recorded env sidecar fields — not as a protocol requirement. See [`E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) § Authoritative evaluation backend.

**Step plan index (format lock applies):**

| Step | Plan status |
|------|-------------|
| **E1** | ✅ Complete (`E_ANALYSIS_PLAN.md` E-AP v1.1) — predates this format lock |
| **EP-01** | ✅ Complete (2026-07-09) — `THOTH_E2_EP01=1` |
| **E2** | ✅ Complete (2026-07-09) — `phase_e_strict_v1.md` · E2-28 PASS · `evidence_scope: n=3_strict_trio` |
| **E3** | 📋 Pending — must conform to this format before lock |
| **E4** | 📋 Pending — must conform to this format before lock |
| **E5** | 📋 Pending — must conform to this format before lock |

---

##### E.0.0 Step 1 — evaluation protocol + analysis plan lock (**v1 locked**)

**Status:** ✅ **Step 1 complete** (2026-07-09) — [`E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) E-AP v1.1; B1 **deferred**; paused before Step 2.

###### Core invariant (why Step 1 exists)

> **Is the evaluation protocol completely specified before any expanded authoritative runs?**

Step 1 is the **preregistration gate** for empirical validation. It locks what will be measured, on what corpus, under what exclusions, and with what validity caveats — **before** Step 2 executes authoritative STRICT runs. Step 1 delivers **E-Q1**; it does not run benchmarks.

###### Step 1 question (locked boundary)

> **Step 1 answers: “Is there a single, reviewer-auditable document that fully specifies the evaluation protocol (definitions, corpus, authority, exclusions) and the analysis plan (reporting, hypotheses, interpretation, claim taxonomy) for Phase E empirical claims?”**

| Step 1 proves | Step 1 does **not** prove (deferred) |
|---------------|--------------------------------------|
| Evaluation protocol **completely specified** for in-scope claims (**E-Q1**) | Authoritative benchmark runs (Step 2) |
| **Protocol frozen** — Step 2 cannot quietly redefine success | Metric **validation** / optimality of constants |
| Metric **definitions** referenced and bounded (not redefined ad hoc) | External validity beyond declared corpus |
| Corpus scope **explicitly declared** (v1.2 trio; B1 in/out decision locked) | L4 reproducibility package (Step 3) |
| Exclusions + evidence tiers **defined** (not just named) | Claims audit / paper mapping (Step 4) |
| Threats to validity **and mitigations** documented | Formal significance testing |
| **Claim taxonomy** maps claim types → required evidence tier | Phase E complete (Step 5) |
| Statistical posture locked (**inference intentionally deferred**) | Universal optimality of `LIFT_MARGIN`, `0.25`, etc. |
| Publication target scope declared (architecture vs empirical) | Confidence intervals, bootstrap, or inferential testing (deferred — post-B1 if ever) |

> **Phase E Step 1 validates protocol completeness — not universal optimality of engineering constants.**

###### Artifact contract (locked)

**Artifact:** [`docs/phases/E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) — preregistered **before** Step 2 runs.

**Filename note:** `E_ANALYSIS_PLAN.md` is retained for continuity with `E_PHASE_PROTOCOL.md` and § E.0.0 tracking. The document is **both** protocol specification and analysis plan — reviewers must see that split **inside** the file, not only in the filename.

**Naming collision:** Phase **E** Step 1 artifact — not benchmark environment pinning ([`benchmark_environment.md`](benchmark_environment.md) “E1” track).

**Required document structure (locked for Step 1 implementation):**

```
E_ANALYSIS_PLAN.md
├── Preamble (header, publication scope, normative document precedence)
├── Part I — Evaluation Protocol
│     definitions · corpus · authority · exclusions · evidence tiers ·
│     protocol freeze · protocol amendments
└── Part II — Analysis Plan
      research questions · hypotheses · reporting · statistical posture ·
      threats & mitigations · falsification · claim taxonomy · step handoffs
```

| Part | Question it answers | Typical content |
|------|---------------------|-----------------|
| **Part I — Evaluation Protocol** | *What is allowed to count as evidence, on what corpus, under what rules?* | Definitions, corpus, authority, exclusions, tier definitions, protocol freeze, amendments |
| **Part II — Analysis Plan** | *What will we report, how will we interpret it, and what claims are in scope?* | Hypotheses, reporting, validity threats/mitigations, claim taxonomy, Step 2–4 handoffs |

**Mandatory sections (all required in Step 1 artifact):**

| Section | Part | Content |
|---------|------|---------|
| **Header** | Preamble | Date; `E_PHASE_PROTOCOL.md` v0.1; `E2_PROTOCOL.md` v1.2; Phase D seal ref (`PHASE_D_COMPLETE.md`) |
| **Publication scope** | Preamble | Architecture vs empirical targets; in-scope external docs (e.g. `MYPAPER.md`, thesis) |
| **Normative document precedence** | Preamble | Conflict-resolution order — see precedence table below |
| **Definitions** | I — Protocol | Reference `E2_PROTOCOL.md` — `evaluation_resolution`, cold/warm arms, pass/fail semantics |
| **Corpus declaration** | I — Protocol | v1.2 trio (E2-01..03) **required**; B1 (30-case) **in or out** — decision locked here |
| **Authority** | I — Protocol | Who may emit `official_scoring`; Phase D “who has scoring authority?” pointer |
| **Exclusions** | I — Protocol | INTEGRATION non-scoring; v1.1 retraction; mock/TfIdf CI as sole external evidence; organic consolidation path |
| **Evidence tiers** | I — Protocol | Tier table with **one-sentence definition each** (see below) |
| **Metrics (definition)** | I — Protocol | `LIFT_MARGIN`, `mean_episodic_lift`, retrieval hit, `GTE`/`ABS_LT` — reference only, do not redefine |
| **Constants posture** | I — Protocol | Provisional `0.25` inclusion threshold; **not** claiming optimality |
| **Protocol freeze** | I — Protocol | Preregistration lock — see freeze text below |
| **Protocol amendments** | I — Protocol | Controlled change process — see amendment text below |
| **Reproducibility prerequisites** | I — Protocol | L2 (benchmark E1 env pinning) required for Step 2; L4 target for close-out |
| **B1 / E2 timing** | I — Protocol | Fork table (`E_PHASE_PROTOCOL.md` § B1/E2 timing) — E2 corpus declared here |
| **Research questions** | II — Analysis | Primary + secondary — map to `E2_PROTOCOL.md` research question |
| **Hypotheses** | II — Analysis | Descriptive expectations under STRICT — no causal inference claims |
| **Reporting rules** | II — Analysis | All STRICT outcomes including failures; GRAG bucket citation discipline |
| **Statistical posture** | II — Analysis | Inference intentionally deferred + scope sentence (see below) — **no CIs, bootstrap, or hypothesis tests** |
| **Threats to validity and mitigations** | II — Analysis | Four subsections — each **threat** + **mitigation** (see format below) |
| **Falsification posture** | II — Analysis | L3/L4 failure + negative B1 — report, do not quiet-soften |
| **Claim taxonomy** | II — Analysis | Claim type → minimum evidence tier (see table below) |
| **E4 handoff** | II — Analysis | Cold-read audit rule (≥24 h gap or second reviewer) |
| **Step 2 handoff** | II — Analysis | Exact runs Step 2 will execute (harness, tier, corpus per B1 fork, env tier) |

**Normative document precedence (required in Preamble):**

> **This document** governs Phase E empirical evaluation. Where referenced documents appear to disagree, resolve in this order:

| Priority | Source |
|----------|--------|
| 1 | **This document** — locked `E_ANALYSIS_PLAN.md` (Part I + Part II) |
| 2 | **Current protocol revision** — `E2_PROTOCOL.md` version declared in Part I header |
| 3 | **Phase D authority definitions** — `PHASE_D_COMPLETE.md`, `D_PHASE_PROTOCOL.md`, `D5_PROTOCOL.md` |
| 4 | **E2 benchmark protocol** — harness rules, case tables, and constants in cited `E2_PROTOCOL.md` revision |
| 5 | **Supporting specs** — `benchmark_environment.md`, `GRAG.md`, phase seals (consistent with 2–4) |
| 6 | **Historical documents** — audit history only; never normative for current claims |

**Protocol freeze (required verbatim intent in Part I):**

> Once this document is locked, no metric definitions, corpus membership, reporting rules, authority tiers, hypotheses, exclusions, or evaluation constants may change without a **protocol amendment** (see below). Step 2 cannot quietly redefine success.

**Protocol amendments (required in Part I):**

> Any modification after protocol lock requires a **new protocol revision identifier** and **explicit supersession** of the previous revision before additional benchmark execution.

Amendments typically coincide with **`E2_PROTOCOL.md` v1.3+** or a dated `E_ANALYSIS_PLAN.md` revision; each must be committed, cite what it supersedes, and state justification per the falsification clause. No further authoritative STRICT runs for external claims until the superseding revision is locked.

**Evidence tier definitions (required in Part I — one sentence each):**

| Tier | Definition (template — adapt in artifact) |
|------|-------------------------------------------|
| **STRICT** | Only source of **official episodic benchmark claims**. |
| **INTEGRATION** | Engineering diagnostics only — never official benchmark evidence. |
| **Phase D** | Machinery trust and authority preservation only — not empirical lift evidence. |
| **GRAG** | Retrieval quality characterization; bucket rules apply when cited. |
| **Historical** | Superseded results retained only for audit history (e.g. v1.1 retraction). |
| **`n=3_strict_trio`** | Scope label — claims backed only by v1.2 trio; not generalization evidence. |

**Statistical posture (required in Part II — two sentences; no inferential machinery):**

1. **Statistical inference intentionally deferred.**
2. Phase E reports **descriptive benchmark outcomes and protocol compliance**; inferential statistical claims remain **outside the scope of Protocol v1.2**.

**Threats to validity and mitigations (required format in Part II):**

Each of the four validity types gets **threat** + **mitigation** subsections (not a single combined answer):

| Validity type | Threat (template) | Mitigation (template) |
|---------------|-------------------|------------------------|
| **Internal** | Does STRICT isolate the intended variable? | Sealed log, kernel isolation, Phase D authority proof |
| **Construct** | Does episodic lift measure “learning”? | Declare lab construct; organic path excluded |
| **External** | Generalizes beyond declared corpus? | n=3 tier label; B1 required for generalization claims |
| **Conclusion** | Can statistical conclusions be drawn? | Descriptive-only posture; inference deferred |

**Claim taxonomy (required table in Part II — extend as needed):**

| Claim type | Minimum evidence required |
|------------|---------------------------|
| Architecture correctness | Phase D seal + machinery gates |
| Benchmark machinery trust | Phase D (authority, determinism, equivalence) |
| Official episodic lift (scoped) | STRICT + corpus tier label (`n=3_strict_trio` or B1-inclusive) |
| Generalization beyond trio | STRICT on B1 registry (only if B1 in scope in Part I) |
| Retrieval quality | GRAG diagnostics (not STRICT official) |
| Engineering diagnostic | INTEGRATION (non-scoring) |
| Historical comparison | Historical tier only — not current official evidence |

**Consume prior evidence (reference only — no re-execution in Step 1):**

| Attested | Reference |
|----------|-----------|
| Phase D sealed | `PHASE_D_COMPLETE.md` (`e0a9ba5`) |
| E2 protocol | `E2_PROTOCOL.md` v1.2 🔒 |
| Phase B baseline | `PHASE_B_COMPLETE.md`; fingerprint `1ce31c6a…` |
| Benchmark env pinning | `benchmark_environment.md` (E1 track ✅) |

###### Step 1 forbidden (locked)

- Execute authoritative STRICT runs (Step 2) before `E_ANALYSIS_PLAN.md` is locked  
- Redefine `E2_PROTOCOL.md` pass/fail rules without protocol amendment (v1.3+ or superseding plan revision)  
- **Change frozen protocol fields after lock** without amendment process (metrics, corpus, reporting, authority, hypotheses, exclusions, constants)  
- **Introduce confidence intervals, bootstrap, or hypothesis testing** in Step 1 — inferential statistics deferred (post-B1 if ever)  
- Present engineering constants as theoretically optimal  
- Include INTEGRATION results as official benchmark evidence in the plan  
- Citing Phase D machinery proofs (D5, C5 equivalence) as empirical lift evidence  
- Evidence tiers listed **without** one-sentence definitions  
- Threats to validity without paired mitigations  
- Production code changes — Step 1 is **documentation only**  
- New harness env gates (Step 1 is plan lock + artifact commit — no `THOTH_*` gate)  

###### Proposed work (Phase E Step 1)

| Work | Detail |
|------|--------|
| `docs/phases/E_ANALYSIS_PLAN.md` | Preregistered doc — Part I (protocol) + Part II (analysis); all mandatory sections |
| `docs/cursor_list.md` | § E.0.0 Step 1 status |
| `docs/E_PHASE_PROTOCOL.md` | Step 1 artifact pointer (post-lock) |

**No production or harness changes expected.**

###### Step 1 implementation discipline

- Documentation only — no code, no benchmark execution  
- **Two-part structure required** — Part I (protocol) before Part II (analysis) in the artifact  
- **B1 in/out decision must be explicit** in Part I before Step 2 — do not defer silently  
- Plan must be committed **before** any Step 2 **pinned authoritative backend** runs at the **authoritative inference tier** (`--full` / live tier, L2 env-pinned) used for external claims  
- Estimated effort: **3–5 h** (writing + review — v2 adds claim taxonomy, tier definitions, mitigations)  
- On scope ambiguity: stop and amend plan before Step 2 — do not run first and document later  

###### Step 1 evidence artifact

✅ **Step 1 complete** (2026-07-09):

1. [`E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) committed — Preamble + Part I + Part II + all mandatory sections (**E-AP v1.1** — v1.1 clarifies LLM-backend-agnostic wording)  
2. **Normative document precedence** present in Preamble  
3. **Protocol freeze** + **protocol amendments** clauses present in Part I  
4. E-Q1 checklist satisfied  
5. B1 corpus scope: **deferred** (trio only for Step 2; generalization claims forbidden)  
6. Threats to validity **and mitigations** documented in Part II  
7. **Claim taxonomy** table present in Part II  
8. Statistical posture: inference deferred + descriptive-only (no CIs/bootstrap/tests)  
9. **Conclusion:** evaluation protocol completely specified and frozen — `evidence_scope: n=3_strict_trio`  
10. **Deferred:** Step 2 authoritative runs · Step 3 L4 package · Step 4 claims audit · Step 5 close-out  

###### Step 1 exit criteria

1. ✅ Plan locked in § E.0.0 Step 1 (v1) — 2026-07-09  
2. ✅ `E_ANALYSIS_PLAN.md` contains Preamble + Part I + Part II and all mandatory sections  
3. ✅ Protocol freeze + amendments + claim taxonomy + precedence present  
4. ✅ B1 in/out decision explicit in Part I — **deferred**  
5. ✅ E-Q1 answerable **yes** from the artifact alone  
6. **Pause for review** before Step 2  

###### Step 1 files (expected touch)

| File | Change |
|------|--------|
| `docs/phases/E_ANALYSIS_PLAN.md` | **New** — Part I Evaluation Protocol + Part II Analysis Plan |
| `docs/cursor_list.md` | § E.0.0 Step 1 status |
| `docs/E_PHASE_PROTOCOL.md` | Step 1 artifact pointer (optional cross-ref) |
| `external/basic_agent/*` | **None** |
| `tests/unit_tests.cpp` | **None** |

---

##### E.0.0 EP-01 — episodic authoritative inference harness (**v1 locked — complete**)

**Status:** ✅ **EP-01 complete** (2026-07-09) — dual inference mode harness + `inferTier()` episodic branch + `THOTH_E2_EP01=1` (E2-29 → E2-28 → E2-30).

###### Objective

Design and implement a **live-backend execution path** for `run_episodic_learning_benchmark` so Phase E Step 2 can run against the **pinned authoritative inference backend** per [`E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) E-AP v1.1.

This milestone adds **infrastructure only**. It does **not** execute Phase E benchmark evidence runs, generate benchmark reports, change benchmark semantics, or weaken the frozen protocol to match today's mock-only harness.

###### Core invariant

> **Add authoritative inference capability while preserving all existing STRICT evaluation semantics.**

The following must remain **unchanged** by EP-01:

- Scoring authority (`wiring_stage=B` only for `official_scoring: true`)
- `evaluation_resolution` / `e2_outcome` export contract
- `wiring_stage` semantics (A1..A5 checkpoints vs **B** official)
- STRICT retrieval kernel (`e2StrictRetrieve`, sealed log)
- Official scoring envelopes and `validateStrictConfigForOfficialRun` rules
- Corpus definitions (v1.2 case table)
- Benchmark metrics, thresholds, pass/fail criteria
- Reporting rules (`E2_PROTOCOL.md` § Reporting policy)
- Evaluation fingerprint **derivation contract** (pins may differ per backend — semantics must not)
- Phase D authority guarantees

Changing the inference backend must **never** redefine benchmark behavior — only which LLM/embedding backend executes the **existing** scored loop.

###### What this step proves

| This milestone proves | Mechanism |
|-----------------------|-----------|
| Episodic harness supports **authoritative inference** execution mode | Live-backend path distinct from mock default |
| Backend selection is **configurable** | CLI and/or env — mirror existing harness patterns |
| Backend metadata captured in L2 env pinning | `BenchmarkEnvironment` / sidecar fields populated |
| Reproducibility metadata preserved | `run_id`, `env_hash`, `index_hash`, `inferTier`, `TIER_MISMATCH` discipline |
| Evaluation authority unchanged | Mock-mode regression: Phase B fingerprint / E2-28 on default mock path |
| Phase D trust guarantees intact | Targeted D5 determinism or E2-28 attestation post-change — not full D5 re-run by default |

###### What this step does not prove

| Deferred | Owner |
|----------|-------|
| Episodic lift / learning improvement | Phase E Step 2 |
| Benchmark SUCCESS/FAILURE as empirical claim | Phase E Step 2 |
| Phase E completion | Step 5 |
| Publication readiness | Steps 3–5 |
| Statistical validity | Out of scope v0.1 |
| B1 / generalization | Deferred in E-AP |
| That a specific vendor backend is required | Protocol remains backend-agnostic |

###### Scope

| In scope | Out of scope |
|----------|--------------|
| Authoritative inference tier path for `run_episodic_learning_benchmark` | Phase E benchmark execution (Step 2) |
| Mock mode **preserved** as default for CI / Phase B regression | Scoring loop / kernel / case table changes |
| CLI or config selection (`--mock` default · `--full` / `--authoritative` for live path) | Protocol amendment to drop authoritative inference requirement |
| **Isolated** `inferTier()` branch for `harness == "episodic_learning_benchmark"` only | Modifying existing `inferTier()` branches for `test_suite`, `chat_rag_benchmark`, `grag_benchmark`, `robustness_suite` |
| `BenchmarkEnvironmentInputs` population for episodic harness | INTEGRATION tier work |
| Model/version **metadata** capture in env sidecar | Fingerprint **derivation** / `computeFingerprint()` semantics changes |
| Unit tests **E2-29**, **E2-30** (+ mock E2-28 regression) | Full G2 `ctest` as EP-01 gate |
| Authoritative smoke — **non-scoring** path only (see exit § no `official_scoring`) | `wiring_stage=B` official scoring runs in EP-01 |
| Documentation of backend selection contract | Phase E run manifests / `phase_e_strict_v1.md` |

**`inferTier()` discipline (locked):** `benchmark_environment.cpp` is **shared** across every harness. EP-01 adds a **new isolated branch** keyed on `inputs.harness == "episodic_learning_benchmark"` — mirroring how `chat_rag_benchmark` / `grag_benchmark` / `test_suite` each have their own branches. **Do not** refactor, reorder, or alter predicates in existing harness branches; treat any change outside the new episodic branch as a **Phase D regression risk** requiring explicit justification and broader regression.

**Code review finding (2026-07-09):** Harness unconditionally sets `THOTH_MOCK_*`, uses TfIdf-only `EmbeddingEngine`, and pins `model_version_or_weights_hash = "mock"`. This is **intentional** for Phase B machinery proof — **not** a routing bug. EP-01 adds missing capability; Step 2 must **not** proceed without it.

**Planning vs implementation:** This section is **locked**. No code until explicitly approved for implementation (AGENTS.md gate).

###### Files touched

| Category | File | Change (on implementation) |
|----------|------|----------------------------|
| **Harness** | `external/basic_agent/src/run_episodic_learning_benchmark.cpp` | Backend mode selection; conditional mock env; authoritative `BenchmarkEnvironmentInputs`; shared engine lifecycle for `runCaseArm` |
| **Infrastructure** | `external/basic_agent/src/benchmark_environment.cpp` | **Add isolated** `inferTier()` branch for `harness == "episodic_learning_benchmark"` only — **no edits** to existing harness branches |
| **Infrastructure** | `external/basic_agent/include/benchmark_environment.h` | Only if new enum/helper needed — prefer reuse |
| **Infrastructure** | `external/basic_agent/src/benchmark_context.cpp` | Only if episodic-specific env flags needed |
| **Infrastructure** | `external/basic_agent/src/ollama_snapshot.cpp` | **Reuse only** — no change unless gap found |
| **Configuration** | `external/basic_agent/include/config.h` | **Reuse only** — read `llm_model`, `embedding_model` for authoritative path |
| **Build** | `external/basic_agent/CMakeLists.txt` | **None** expected |
| **Tests** | `tests/unit_tests.cpp` | **E2-29** (mock regression / E2-28 preserved) · **E2-30** (authoritative smoke + no-official-scoring invariant); orchestrator `runE2Ep01Tests()` · gate `THOTH_E2_EP01=1` |
| **Documentation** | `docs/E2_PROTOCOL.md` | Harness § — document mock vs authoritative inference modes |
| **Documentation** | `docs/benchmark_environment.md` | Episodic harness row in tier / checklist tables |
| **Documentation** | `docs/cursor_list.md` | § E.0.0 EP-01 status + evidence |
| **Documentation** | `docs/baselines/BASELINE_PROVENANCE.md` | Note: Phase B baseline = mock tier; authoritative mode is additive |
| **Documentation** | `docs/phases/E_ANALYSIS_PLAN.md` | **None** — frozen |
| **Production** | `executive_controller.*`, `workflow_engine.*`, `e2_strict_retrieval.*`, `episodic_learning_eval.*` | **None** — authority + scoring semantics frozen |

###### Detailed work items

| # | Work item | Detail |
|---|-----------|--------|
| **1** | **Pattern audit** | Document parity gaps vs `run_chat_rag_benchmark`, `run_test_suite --full`, `BasicAgentPlugin::buildTestSuiteBenchmarkInputs` |
| **2** | **Backend selection mechanism** | Add CLI (`--mock` default · `--full` or `--authoritative` for live path) and/or documented env contract; **do not** invent parallel architecture |
| **3** | **Remove unconditional mock forcing** | `main()` and `runCaseArm()` must not `setenv(THOTH_MOCK_*)` when authoritative mode selected |
| **4** | **Authoritative `BenchmarkEnvironmentInputs`** | `harness = "episodic_learning_benchmark"`; `tier = FULL` or harness-specific; real `llm_model` / embedding method; `ollama_reachable` + snapshot probe where applicable; backend-agnostic hook points for future providers |
| **5** | **`inferTier()` — isolated episodic branch** | Add **new** branch for `harness == "episodic_learning_benchmark"` only; classify mock vs authoritative inference tier; emit `TIER_MISMATCH` on mismatch. **Forbidden:** changing `test_suite`, `chat_rag_benchmark`, `grag_benchmark`, or `reflection_ab` / `robustness_suite` branch logic |
| **6** | **Embedding / engine lifecycle** | Authoritative path: `EmbeddingEngine::External` (or config-driven) shared into `runCaseArm`; mock path: preserve current TfIdf behavior |
| **7** | **Version pins (metadata only)** | Authoritative mode: populate `strictConfig.versions.*` with real backend pins (replace literal `"mock"`). Mock mode: unchanged pins. **Do not** alter `computeFingerprint()` / B5.0b1 derivation contract |
| **8** | **Env fingerprint** | Sidecar captures inference backend identifier + model ids per `benchmark_environment.md` |
| **9** | **E2-28 mock regression (E2-29)** | Default/mock run reproduces Phase B scoped equivalence — bucket #0; preregistered test **E2-29** |
| **10** | **Authoritative smoke (E2-30)** | Authoritative mode reaches live backend; harness completes smoke path; **no** `wiring_stage=B` scored official run; JSONL audited for **zero** `official_scoring: true` rows |
| **11** | **Phase D spot-check (gate — after 9, before 10)** | `testE2B5OfficialFingerprintDeterminism()` (E2-28 helper) on **mock** path — **hard gate** after E2-29, before E2-30; attests Phase D authority preserved post-`inferTier()` |
| **12** | **Documentation** | E2 harness contract + benchmark_environment checklist + EP-01 evidence in `cursor_list.md` |

**Work sequencing (locked — verification gates are ordered, not parallel):**

| Phase | Items | Gate rule |
|-------|-------|-----------|
| **Implementation** | **1 → 8** (sequential; **5** `inferTier()` episodic branch must land before any verification) | No `THOTH_E2_EP01=1` / E2-29 / E2-30 until items **1–8** complete |
| **Verification** | **9 → 11 → 10** | Each step is a **hard gate** — failure stops EP-01; do not declare EP-01 done until all three pass |
| ↳ | **9 — E2-29** | Mock path / E2-28 regression after full harness + `inferTier()` change |
| ↳ | **11 — Phase D spot-check** | `testE2B5OfficialFingerprintDeterminism()` (or equivalent E2-28 helper) on **mock** path — runs **after 9**, **before 10**; confirms Phase D authority preserved post-`inferTier()` |
| ↳ | **10 — E2-30** | Authoritative inference smoke — runs **last** among verification gates (mock path + authority already green) |
| **Close-out** | **12** | Documentation + § E.0.0 EP-01 evidence record — only after **9, 11, 10** green |

`runE2Ep01Tests()` / `THOTH_E2_EP01=1` orchestrator must invoke **E2-29 → Phase D E2-28 spot-check → E2-30** in that order.

###### Dangers / failure modes / things that must not change

| Risk | Mitigation |
|------|------------|
| **Trajectory score divergence (PRIMARY)** — live authoritative backend changes **Executive / planner / LLM trajectory scores** (`calculate_trajectory_score()` path) even though **`e2StrictRetrieve` does not call the LLM**. Mock-trio lift numbers are **not** comparable to authoritative-inference lift numbers without explicit backend + corpus tier labeling. Future readers may misread a Step 2 lift delta as apples-to-apples with Phase B mock baseline. | Name this risk in EP-01 docs; EP-01 smoke must **not** emit official scored rows; Step 2 run record must declare backend + `evidence_scope`; never compare Phase B mock rollup to Phase E authoritative rollup without tier label |
| **Accidental scoring changes** | No edits to `runScoredEvaluationLoop`, `evaluateCase`, `summarize`, pass/fail table |
| **Evaluation authority drift** | EP-01 smoke forbids `wiring_stage=B` official scoring runs; E2-30 asserts **no** `official_scoring: true` in smoke JSONL |
| **Shared `inferTier()` regression** | New episodic branch only — existing harness branches untouched; grep audit in E2-29 |
| **Benchmark semantics drift** | EP-01 exit requires E2-28 / E2-29 mock regression green |
| **Fingerprint meaning drift** | Version **pin values** may change per backend; `computeFingerprint()` / canonical JSON **derivation** must not (see Forbidden) |
| **Protocol changes** | E-AP v1.1 not amended to drop authoritative inference tier |
| **Phase D regressions** | E2-28 mock spot-check after harness change |
| **Backend behavior leaking into STRICT kernel** | `e2StrictRetrieve` remains token-overlap kernel — no LLM in kernel path |
| **Loss of deterministic mock metadata** | Mock mode remains default; CI path unchanged |
| **Reproducibility degradation** | Authoritative mode populates full L2 sidecar; `TIER_MISMATCH` on lie |
| **Scope creep into Step 2** | No trio evidence runs, no `phase_e_strict_v1.md` in EP-01 |
| **Things that must not change** | See Core invariant list |

###### Forbidden changes

- Changing scoring logic, `evaluation_resolution`, or export rollups  
- Changing STRICT retrieval kernel or sealed-log semantics  
- Changing corpus, case table, metrics, thresholds, or pass/fail rules  
- Changing authority model (`official_scoring`, subscriber firewalls, Phase D boundaries)  
- Changing protocol definitions or reporting rules in `E2_PROTOCOL.md` without v1.3+ amendment  
- Changing benchmark success criteria or fingerprint **meaning**  
- **Changing `computeEvaluationFingerprint()` / canonical fingerprint derivation** (B5.0b1 contract) — authoritative mode may supply **real** `strictConfig.versions.*` pin **values** in place of `"mock"`; the pin **mechanism** and fingerprint **semantics** must not change  
- Modifying existing `inferTier()` branches for non-episodic harnesses  
- Running `wiring_stage=B` official scoring smoke in EP-01 (infrastructure only — no `official_scoring: true` rows)  
- Changing Phase D guarantees or re-running full D5 as EP-01 gate  
- Weakening E-AP authoritative inference requirement to “mock is enough”  
- Phase E benchmark execution disguised as harness work  
- Coupling protocol text to a single LLM vendor  

###### Exit criteria

1. Plan locked in § E.0.0 EP-01 — committed before implementation  
2. Episodic harness supports **selectable** mock (default) and **authoritative inference** modes  
3. **Mock mode** still functions — **E2-29** / E2-28 mock regression **green**  
4. **Authoritative mode** smoke completes — live backend contacted, metadata captured (**E2-30**)  
5. **Zero official scoring in EP-01 smoke** — grep/audit of EP-01 authoritative smoke JSONL: **no** rows with `official_scoring: true` (A1-style NONE invariant; EP-01 does not produce benchmark evidence)  
6. `inferTier()` episodic branch + sidecar record backend identity for authoritative runs  
7. Phase D behavior preserved — **item 11** E2-28 spot-check on mock path **green** (after E2-29, before E2-30)  
8. `THOTH_E2_EP01=1` green — orchestrator ran **E2-29 → 11 → E2-30** in order  
9. Documentation updated (E2 harness + benchmark_environment)  
10. **No Phase E benchmark evidence** produced in EP-01  
11. **Pause for review** before Phase E Step 2 lock/implementation  

###### Deliverables / evidence produced

| Deliverable | Path / ID |
|-------------|-----------|
| Updated episodic harness | `run_episodic_learning_benchmark.cpp` — dual mode |
| Backend selection documentation | `docs/E2_PROTOCOL.md` § Harness + `docs/benchmark_environment.md` |
| **E2-29** — mock regression preserved | `testE2Ep01MockRegressionPreservesE28()` — Phase B / E2-28 discipline on default mock path |
| **E2-30** — authoritative smoke + no official scoring | `testE2Ep01AuthoritativeInferenceSmoke()` — live backend path; asserts **zero** `official_scoring: true` in smoke JSONL |
| EP-01 orchestrator | `runE2Ep01Tests()` · gate `THOTH_E2_EP01=1` in `main()` |
| Compatibility note | `docs/baselines/BASELINE_PROVENANCE.md` or EP-01 evidence block in `cursor_list.md` |
| EP-01 evidence record | § E.0.0 EP-01 status in `cursor_list.md` — modes proven, commits, gates green |

**No** `phase_e_strict_v1.md`, **no** Phase E run manifest, **no** empirical lift claims.

###### Dependencies on previous steps

| Dependency | Reference |
|------------|-----------|
| **Phase D complete** | [`PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md) — authority sealed |
| **E1 — locked (hard gate)** | [`E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) E-AP v1.1 🔒 — authoritative inference tier requirement **not** weakened |
| **Benchmark E1** | [`benchmark_environment.md`](benchmark_environment.md) — L2 pinning infrastructure |
| **E2 protocol** | [`E2_PROTOCOL.md`](E2_PROTOCOL.md) v1.2 — harness contract |
| **Reference harnesses** | `run_chat_rag_benchmark`, `run_test_suite`, `BasicAgentPlugin::buildTestSuiteBenchmarkInputs` |

**Blocks:** Phase E **Step 2** may not lock or implement until EP-01 exit criteria are **green**.

###### Pause

**STATUS: EP-01 COMPLETE — EP-01.5 REQUIRED BEFORE STEP 2 REDO**

EP-01 dual-mode harness is **green** for its locked scope. Pre-flight (1) on Step 2 showed authoritative mode still short-circuits LLM synthesis (no `set_llm_interface`). **EP-01.5** repairs that gap. Do **not** redo Step 2 until EP-01.5 exit criteria are green.

---

##### E.0.0 EP-01.5 — authoritative LLM wiring & planner contract (**v1 LOCKED**)

**Status:** 🔒 **LOCKED FOR IMPLEMENTATION** (2026-07-09) — Phase 1 ✅ · **Phase 2 gate green** (E2-31b) · Phases 3–5 blocked.

**Context:** Step 2 investigation hold — pre-flight (1) failed. Authoritative `--authoritative` + `wiring_stage=B` runs produced `total_tokens=0` / `terminal_state=FAILED` because `runCaseArm()` never wired `LLMInterface`. Failure mode = **harness infrastructure**, not a falsified benchmark result. See [`phase_e_strict_v1.md`](benchmark_results/phase_e_strict_v1.md) § Investigation hold.

###### Objective

Make the authoritative episodic harness **actually authoritative**: wire live `LLMInterface` for `--authoritative` + `wiring_stage=B`, clarify the planner contract, fix declared-tier mismatch at the inputs boundary, and add execution gates that prove inference occurred — **without** amending scoring, corpus, thresholds, or Phase B fingerprints.

###### Core invariant

> **EP-01.5 makes the authoritative harness actually authoritative; it does not make the benchmark easier.**

Wire live LLM execution for authoritative mode while preserving STRICT scoring authority, evaluation semantics, case table, and mock-tier behavior. EP-01.5 is **harness infrastructure only** — not a protocol amendment, not a metric/threshold change, not Step 2 evidence collection.

###### Non-goals (locked — proof boundary)

EP-01.5 does **not**:

- change scoring formulas
- change trajectory quality thresholds
- change goal corpus
- change retrieval behavior
- change planner selection semantics
- change Phase B fingerprints
- modify existing EP-01 artifacts
- improve benchmark outcomes

This section protects the integrity argument: EP-01.5 must not accidentally become a benchmark amendment.

###### Planner contract (locked)

| Tier | Planner | LLM execution | Plan topology | Trajectory scoring |
|------|---------|---------------|---------------|-------------------|
| **Mock** (`--mock`) | `EpisodicLearningMockPlanner` | `THOTH_MOCK_LLM` — mock validates `required_token` in prior RETRIEVAL context | Fixed: RETRIEVAL → LLM | `calculate_trajectory_score()` on mock success/failure |
| **Authoritative** (`--authoritative`) | **Same** `EpisodicLearningMockPlanner` | **Live** `LLMInterface` (pinned `config.llm_model`) | **Same** fixed topology | **Same** `calculate_trajectory_score()` on real plan completion |

**Contract rules:**

1. **`EpisodicLearningMockPlanner` is retained in authoritative mode exclusively as a deterministic topology provider. It MUST NOT influence LLM execution success, trajectory quality, or scoring outcomes.**
2. Do **not** switch authoritative arms to `LLMPlanner` — non-deterministic plan shape would break the E2 lab corpus contract.
3. Do **not** port mock `required_token` gating to the live path — that is mock-tier instrumentation, not authoritative scoring semantics.
4. In authoritative mode, the class name “Mock” is historical: the planner supplies RETRIEVAL→LLM structure only.

**Authoritative arm is “live” when** the execution gate below passes (actual inference, not mere latency).

###### What this milestone proves

| Proves | Mechanism |
|--------|-----------|
| Authoritative `runCaseArm` invokes live LLM | `set_llm_interface` + E2-31 token proof |
| Planner is topology-only in authoritative mode | Contract above + no live `required_token` gate |
| Failed LLM startup cannot seal official summary | Execution + pre-summary gates |
| Declared tier matches `inferTier` for episodic authoritative inputs | Inputs-boundary fix (E2-31b) |
| Mock path / Phase B fingerprints unchanged | E2-29 first in orchestrator |

###### What this milestone does not prove

| Deferred | Owner |
|----------|-------|
| Empirical lift / SUCCESS/FAILURE as claim | Step 2 **redo** (separate approval) |
| That live LLM “working” equals benchmark pass | Explicitly **not** — see execution gate vs trajectory success |
| Protocol / threshold / corpus changes | Forbidden |
| Step 3 L4 package | Blocked until Step 2 redo |

###### Scope

| In scope | Out of scope |
|----------|--------------|
| `LLMInterface` wiring in authoritative `runCaseArm` | Step 2 evidence pair / new sealed lift numbers |
| Planner contract documentation | Scoring / kernel / case table edits |
| Declared-tier fix at `makeEpisodicBenchmarkInputs` | Reordering shared `inferTier()` predicates |
| Execution + pre-summary gates (inference proof) | Tuning generation params to chase pass |
| E2-31 / E2-31b + `THOTH_E2_EP015=1` | Full G2 as required gate |
| Stepwise Phases 1–5 below | One-shot “giant” implementation without phase stops |

**Planning vs implementation:** 🔒 Locked. Implementation follows AGENTS.md + **phase-commit discipline** below.

###### Implementation discipline (locked)

| Rule | Meaning |
|------|---------|
| **One phase at a time** | Do not implement Phases 2–5 until the current phase’s verification gate passes |
| **Commit per phase** | Each phase is committed **only after** its stated verification gate passes |
| **No automatic continuation** | After a partial implementation, **stop** — do not proceed to the next phase without explicit go-ahead |
| **Gate failure** | If a gate fails: **stop**, report findings, wait for repair approval (AGENTS.md Build/Test Failure Rule) |
| **Harness localization** | `LLMInterface` wiring stays in `run_episodic_learning_benchmark.cpp` only — do **not** change `ExecutiveController` constructors or make production paths depend on benchmark-only wiring |
| **Phase 1 review checkpoint** | After Phase 1: show the diff; **do not** continue to Phase 2 until approved |

**Final implementation sequence (locked):**

| Phase | Work | Verification gate |
|-------|------|-------------------|
| **1 — Live LLM wiring** | `LLMInterface` ownership on `HarnessRuntimeContext`; inject via `set_llm_interface` / `set_config` in `runCaseArm`; E2-31 | `LLMInterface::query` path executed; tokens recorded; **no** scoring changes |
| **2 — Tier declaration** | Authoritative input construction only (`makeEpisodicBenchmarkInputs`) | E2-31b |
| **3 — Fail-closed guards** | Execution gate + pre-summary rollup gate | Invalid/no-op runs cannot generate official summaries |
| **4 — Documentation** | Protocol / historical references | Contract text matches locked planner rules |
| **5 — Regression** | Mock preservation + authoritative smoke | E2-29 + E2-30 green |

###### Files touched (on implementation)

| File | Change |
|------|--------|
| `external/basic_agent/src/run_episodic_learning_benchmark.cpp` | LLM ownership + wiring; tier declaration; execution / rollup gates; optional smoke env |
| `tests/unit_tests.cpp` | E2-31, E2-31b, `runE2Ep015Tests()`, `THOTH_E2_EP015=1` |
| `docs/E2_PROTOCOL.md` | Harness § planner contract |
| `docs/cursor_list.md` | § E.0.0 EP-01.5 status + evidence |
| `docs/completed_improvements_log.md` | Close-out entry only |
| `docs/benchmark_results/phase_e_strict_v1.md` | **Not** in EP-01.5 — superseded note belongs to Step 2 redo |
| `episodic_learning_eval.*`, `e2_eval_kernel`, case table, `LIFT_MARGIN` | **Forbidden** |
| Existing non-episodic `inferTier()` branches | **Forbidden** |

###### Detailed work items

| # | Work item | Detail |
|---|-----------|--------|
| **1** | **`HarnessRuntimeContext` LLM ownership** | `HarnessRuntimeContext` **owns** the `LLMInterface` lifetime for the duration of a **single** authoritative benchmark execution. Sequential arm reuse is permitted **only if** `LLMInterface` is proven stateless between arms (e.g. session token reset / no cross-arm prompt state). Prefer deterministic isolation over optimization. |
| **2** | **Wire executive in `runCaseArm()`** | When `!runtime.useMockInference()`: `controller.set_config(&cfg)`; `controller.set_llm_interface(...)` **before** `execute_goal()`. Mock path unchanged (no live `LLMInterface`). Pattern: `testE1RobustnessBenchmarkSmoke` (~3241–3244). |
| **3** | **Harness-local temperature pin only** | Authoritative arms: harness-local `Config` override `temperature = 0.0` for E2-28 reproducibility. **No other generation parameter changes** are permitted under EP-01.5 unless required to initialize the existing configured backend. Model identity, context limits, token limits, prompts, and planner topology remain unchanged. |
| **4** | **Fix `TIER_MISMATCH` at inputs boundary** | In `makeEpisodicBenchmarkInputs()` authoritative branch: set `inputs.tier = BenchmarkTier::OLLAMA` when External + Ollama reachable (align declared with `inferTier()`). **Do not** reorder or alter shared / generic `inferTier()` logic — fix the declaration where benchmark inputs are created. |
| **5** | **Authoritative execution gate** | After each goal arm in authoritative scored loop (`wiring_stage=B`), require proof of **actual inference**: `authoritative_llm_invoked == true` **AND** (`token_count > 0` where available). If no single `token_count` field: `llm_synthesis_time_ms > 0` **AND** `(prompt_tokens + completion_tokens) > 0`. Latency alone is **insufficient** (failed requests can consume time). On failure: abort with clear error (`AUTHORITATIVE_LLM_NOOP`) — fail closed; do not emit sealable official summary. **Execution gate success is independent from trajectory success.** A completed LLM invocation may still produce `FAILED` `terminal_state` due to downstream cognitive failure. |
| **6** | **Pre-summary rollup gate** | Before `EPISODIC_LEARNING_SUMMARY` in authoritative official mode: all 6 goal arms (3 × cold/warm) must have **passed the execution gate** (inference proof), not “passed the benchmark.” Prevents repeating Step 2’s silent-zero artifact. |
| **7** | **Document harness contract** | Update `E2_PROTOCOL.md` harness § with planner contract. Update this § on close-out. Prior Step 2 artifacts remain under investigation hold until redo. |
| **8** | **Mock path regression** | Confirm mock still sets `THOTH_MOCK_*`; mock `runCaseArm` does not instantiate live `LLMInterface`. E2-29 / Phase B fingerprint behavior unchanged. |

###### Implementation order (locked — see Implementation discipline)

Same as **Final implementation sequence** above. Work items **1–2** (+ harness-local `temperature=0.0` on owned config when constructing authoritative `LLMInterface`) → Phase 1; **4** → Phase 2; **5–6** → Phase 3; **7** → Phase 4; **8** → Phase 5.

**Orchestrator (after Phases 1–5 complete):** `runE2Ep015Tests()` / `THOTH_E2_EP015=1` — sequence **E2-29 → E2-28 spot → E2-30 → E2-31** (and E2-31b). Leave `THOTH_E2_EP01=1` unchanged for historical EP-01 seal.

###### Tests

| ID | Purpose |
|----|---------|
| **E2-29** (existing) | Mock regression — must stay green |
| **E2-30** (existing) | Authoritative A2 smoke, zero `official_scoring` |
| **E2-31** (new) | Authoritative LLM wiring — Ollama required; prove tokens / invocation (Phase 1) |
| **E2-31b** (new) | Tier alignment — no mismatch after inputs fix (Phase 2) |

###### Dangers / failure modes

| Risk | Mitigation |
|------|------------|
| EP-01.5 becomes a silent benchmark amendment | Non-goals + Forbidden; no threshold/corpus/scoring edits |
| Lifetime / shared-state bugs across arms | Ownership rule: context owns LLM; reuse only if proven stateless |
| “Fixing” reliability via generation tuning | Temperature pin only; all other gen params frozen |
| Latency mistaken for inference | Token-count required in execution gate |
| LLM worked ⇒ benchmark passed | Execution gate independent of trajectory success |
| Touching shared `inferTier()` | Inputs-boundary declaration fix only |
| Scope creep into Step 2 | No sealable Step 2 pair; no new official lift numbers in EP-01.5 |
| Live non-determinism vs E2-28 | `temperature=0.0` only; if bucket ≠ 0 after wiring, treat as real variance (falsification path) — do not chase pass |

###### Forbidden changes

- Scoring formulas, `evaluation_resolution`, export rollups  
- Trajectory quality thresholds (`LIFT_MARGIN`, episode inclusion `0.25`)  
- Goal corpus / case table / retrieval behavior  
- Planner selection semantics (no switch to `LLMPlanner`)  
- Phase B fingerprints / mock-path scoring  
- Existing EP-01 sealed artifacts  
- Shared / non-episodic `inferTier()` branch logic  
- Generation-parameter tuning beyond harness-local `temperature=0.0`  
- Step 2 redo or Step 3 work disguised as EP-01.5  
- Adjusting outcomes to make the benchmark easier  

###### Exit criteria

1. Plan locked in § E.0.0 EP-01.5 — committed before implementation  
2. Phases 1–5 complete with per-phase verification stops  
3. Authoritative `runCaseArm` invokes live LLM — E2-31 green (Ollama up)  
4. Execution gate requires invocation **and** nonzero tokens (not latency alone)  
5. Pre-summary gate blocks official summary if any arm fails execution gate  
6. `hasTierMismatch()` false for authoritative episodic inputs (E2-31b)  
7. `THOTH_E2_EP015=1` green (E2-29 → E2-28 spot → E2-30 → E2-31)  
8. Mock path / E2-29 unchanged  
9. Planner contract documented in `E2_PROTOCOL.md`  
10. **No** new Phase E sealed lift evidence in EP-01.5  
11. Step 2 investigation hold remains until **separate** Step 2 redo approval  

###### Step 2 redo handoff (after EP-01.5 — separate approval)

Same locked invocation:

```bash
THOTH_E2_WIRING_STAGE=B ./build/debug/external/basic_agent/run_episodic_learning_benchmark --authoritative
```

Mandatory pre-seal gates on redo: all 6 arms pass **execution gate**; E2-28 bucket #0; no unexplained `TIER_MISMATCH`; release investigation hold and mark prior run IDs superseded. **Still forbidden:** adjusting `LIFT_MARGIN` / episode threshold, or re-running until pass.

###### Pause

**STATUS: EP-01.5 PHASE 1 GATE GREEN — SHOW DIFF; DO NOT START PHASE 2**

Phase 1 verification: `THOTH_E2_EP015_PHASE1=1` → E2-31 pass (tokens recorded; `official_scoring=false`). Do **not** auto-continue to Phase 2.

---

##### E.0.0 Step 2 — authoritative STRICT runs (**v1 locked — complete**)

**Status:** ⚠️ **Step 2 complete — UNDER INVESTIGATION HOLD** (2026-07-09) — artifacts sealed but pre-flight (1) failed: authoritative runs did not invoke live LLM (zero tokens); evidence **not** valid for empirical claims until **EP-01.5** + Step 2 redo.

**Evidence:** [`phase_e_strict_v1.md`](benchmark_results/phase_e_strict_v1.md) · [`phase_e_run_manifest.json`](baselines/phase_e_run_manifest.json) · [`phase_e_baseline_verification.md`](baselines/phase_e_baseline_verification.md)

###### Objective

Execute the **frozen** evaluation protocol in [`E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) (E-AP v1.1) on the **v1.2 STRICT trio** (E2-01..03) at the **authoritative inference tier** with **L2 environment pinning**, producing sealed run artifacts that partially satisfy **E-Q2** (reproducibility) and **E-Q3** (methodological defensibility). Step 2 **collects evidence** — it does not audit paper claims or close Phase E.

###### Core invariant

> **Execute the frozen protocol without modifying protocol definitions, scoring authority, evaluation semantics, or benchmark success criteria.**

Step 2 runs the declared corpus (v1.2 trio) under E-AP v1.1 and E2 v1.2 **verbatim** — collecting pinned STRICT evidence only. Success/failure is judged by the **preregistered** rules, not by post-hoc interpretation.

###### What this step proves

| This step proves | Mechanism |
|------------------|-----------|
| Authoritative STRICT runs executed per frozen E-AP v1.1 Step 2 handoff | `run_episodic_learning_benchmark --authoritative` · `THOTH_E2_WIRING_STAGE=B` · trio only |
| L2 environment pinning on every authoritative run | `BenchmarkContext::create()` · `run_id` · `env_hash` · `index_hash` · sidecar |
| **Pinned authoritative backend** recorded in env manifest | Model/provider fields in sidecar — backend-agnostic |
| All STRICT outcomes reported (including FAILURE) | Per E2 reporting policy + E-AP Part II |
| L3 reproduction on declared corpus (trio) | E2-28 scoped equivalence across two consecutive identical STRICT builds |
| `evidence_scope: n=3_strict_trio` on collected lift evidence | Corpus tier label on run record |
| Phase D authority boundaries **unchanged** by benchmark execution | No INTEGRATION → official scoring leakage |

###### What this step does not prove

| Deferred | Step |
|----------|------|
| L4 publication package (independent lab reproduction doc) | **E3** |
| Claims audit / paper sentence mapping | **E4** |
| Phase E complete / E-Q5 seal | **E5** |
| Generalization beyond trio | Forbidden — B1 deferred in E-AP |
| Inferential statistics (CIs, bootstrap, hypothesis tests) | Out of scope v0.1 |
| Metric validation / optimality of `LIFT_MARGIN`, `0.25` | Open — not Phase E claim |
| Architecture correctness or machinery trust | Phase D — reference only |
| INTEGRATION diagnostic value | Non-scoring — not Step 2 evidence |

###### Scope

| In scope | Out of scope |
|----------|--------------|
| Two (minimum) consecutive authoritative STRICT harness runs on trio | B1 corpus expansion |
| Extract + freeze JSONL summary rows + env sidecars to immutable artifact paths | Full unit-test suite / G2 `ctest` (optional hygiene) |
| E2-28 scoped equivalence check on Phase E artifacts | Claims audit writing |
| Run manifest / Phase E strict run record | Protocol constant changes |
| `cursor_list.md` § E.0.0 Step 2 status update | Harness backend work (**EP-01** — ✅ complete) |

**Planning vs implementation:** This section is **locked**. No authoritative evidence runs until explicitly approved for implementation (AGENTS.md gate).

**Canonical invocation (locked — Run A and Run B must use identical flags/build):**

```bash
THOTH_E2_WIRING_STAGE=B ./build/debug/external/basic_agent/run_episodic_learning_benchmark --authoritative
```

- **`--authoritative`** (or `--full`) — authoritative inference tier per EP-01; **`--mock` is forbidden** for Step 2 evidence.
- **`THOTH_E2_WIRING_STAGE=B`** — official STRICT scoring only.
- Ollama must be reachable; abort if `inferTier` ≠ OLLAMA/FULL or `TIER_MISMATCH` appears unexplained.
- **Not comparable:** Phase B mock-tier rollups — Step 2 lift is authoritative-tier only; manifest must label `evidence_scope: n=3_strict_trio` + backend fields.

###### Files touched

| File | Change (on implementation) |
|------|----------------------------|
| `docs/benchmark_results/phase_e_strict_v1.md` | **New** — authoritative run record (rollup, fingerprint, env pins, backend declaration) |
| `docs/baselines/artifacts/phase_e/` | **New** — frozen JSONL excerpts + env sidecar copies (immutable) |
| `docs/baselines/phase_e_run_manifest.json` | **New** — L4-ready reproduction fields (see Deliverables § manifest schema) |
| `docs/cursor_list.md` | § E.0.0 Step 2 status + evidence artifact |
| `docs/phases/E_ANALYSIS_PLAN.md` | **None** — frozen; amend only via protocol amendment process |
| `external/basic_agent/src/run_episodic_learning_benchmark.cpp` | **None** — backend path delivered by **EP-01** |
| `external/basic_agent/*` (other) | **None** |
| `tests/unit_tests.cpp` | **Optional** — Phase E E2-28 attestation wrapper only if not covered by harness two-run; default: use existing `testE2B5OfficialFingerprintDeterminism()` pattern |
| `logs/episodic_learning_benchmark.jsonl` | Runtime append — extract to `docs/baselines/artifacts/phase_e/` for seal |
| `logs/benchmark_env.latest.json` | Runtime — copy to artifact dir per run |

###### Detailed work items

| # | Work item | Detail |
|---|-----------|--------|
| **1** | **Pre-flight — EP-01 complete** | ✅ Satisfied 2026-07-09 (`THOTH_E2_EP01=1`); re-verify if harness changed since lock |
| **2** | **Pre-flight — git + workspace** | Clean or documented tree; git SHA ≠ `unknown`; workspace paths per `benchmark_environment.md` checklist |
| **3** | **Pre-flight — authoritative mode** | Dry-run: `--authoritative` + `inferTier` → OLLAMA/FULL; sidecar has real `llm_model` / embedding ids; **no** `--mock` |
| **4** | **Run A — STRICT trio** | `THOTH_E2_WIRING_STAGE=B` + `--authoritative` (canonical invocation above) · L2 pinning · trio only |
| **5** | **Run B — STRICT trio (reproduction)** | **Identical** command, build, and config to Run A — no drift between runs |
| **6** | **E2-28 equivalence** | Scoped snapshots Run A vs Run B — bucket #0 |
| **7** | **Artifact freeze** | Extract to `docs/baselines/artifacts/phase_e/` · write manifest |
| **8** | **Run record** | `phase_e_strict_v1.md` — `evidence_scope: n=3_strict_trio` |
| **9** | **Regression spot-check** | Optional D5 determinism attestation — not full D5 unless failure implicates authority |

###### Dangers / failure modes / things that must not change

| Risk | Mitigation |
|------|------------|
| **Authority leakage** — INTEGRATION or subscriber path influences `official_scoring` | Runs use `wiring_stage=B` only; verify envelope fields post-run |
| **Protocol drift** — constants/corpus changed mid-run | E-AP v1.1 frozen; E2 v1.2 locked; stop if mismatch |
| **Trajectory / lift tier mismatch** — comparing Phase E authoritative lift to Phase B mock baseline | Manifest `evidence_scope` + backend fields; never claim apples-to-apples with Phase B mock rollup |
| **Mock tier masquerading as authoritative** | Step 2 runs **`--authoritative` only** (EP-01); `--mock` invalid for external episodic claims per E-AP |
| **Silent generalization** — wording or artifacts imply beyond trio | Mandatory `n=3_strict_trio` label on all Step 2 deliverables |
| **E2-28 failure** | Stop per falsification clause — do not publish Phase B numbers as Phase E current |
| **Harness semantic change** | Scoring loop body, `evaluation_resolution`, STRICT kernel, metrics, thresholds, reporting — **forbidden** (harness changes belong to EP-01 only) |
| **Backend coupling in protocol docs** | Record backend in manifest only — do not amend E-AP to require a vendor |
| **Things that must not change** | Protocol definitions · scoring authority · evaluation semantics · benchmark success criteria · `LIFT_MARGIN` · case table · corpus membership · pass/fail rules · reporting rules · Phase D authority boundaries · `evaluation_resolution` / `e2_outcome` export contract |

###### Forbidden changes

- Redefine E2 pass/fail or case expectations without protocol amendment  
- Use INTEGRATION results as Step 2 official evidence  
- Cite Phase D machinery proofs as empirical lift evidence  
- Run B1 cases (deferred in E-AP)  
- Quiet-soften STRICT FAILURE outcomes  
- Full-suite regression as Step 2 gate requirement  
- Claim E-Q2 complete (L4 is Step 3) or Phase E complete  
- Couple Phase E protocol text to a specific LLM vendor  
- Implement or patch harness backend path in Step 2 (**EP-01** owns harness infrastructure)  
- Begin Step 2 before **EP-01** exit criteria green  

###### Exit criteria

1. **EP-01 complete** — episodic authoritative inference harness green ✅ (2026-07-09)  
2. Plan locked in § E.0.0 Step 2 — committed before implementation  
3. Pre-flight confirms **`--authoritative`** inference (not `--mock` default)  
4. Two consecutive authoritative STRICT trio runs complete with L2 sidecars  
5. E2-28 scoped equivalence **green** on Phase E artifact pair (Run A vs Run B)  
6. Frozen artifacts + manifest + `phase_e_strict_v1.md` committed  
7. Run manifest contains **all mandatory fields** (protocol revision, backend, model, env hash, run_id, corpus id, evaluation fingerprint)  
8. All STRICT outcomes reported (including failures if any)  
9. `evidence_scope: n=3_strict_trio` on run record  
10. **Pause for review** before Step 3  

###### Deliverables / evidence produced

| Deliverable | Path / ID |
|-------------|-----------|
| Phase E STRICT run record | `docs/benchmark_results/phase_e_strict_v1.md` |
| Immutable artifact bundle | `docs/baselines/artifacts/phase_e/` |
| Run manifest | `docs/baselines/phase_e_run_manifest.json` |
| L2 env sidecars | Copied per run in artifact bundle |
| E2-28 equivalence result | Recorded in run record (PASS/FAIL + diagnosis bucket) |
| E-Q2 partial | L2 + L3 on trio — pointer in `cursor_list.md` |
| E-Q3 partial | Protocol adherence + reporting — pointer in run record |

**Run manifest — mandatory fields (confirm present per run):**

| Field | Source / example |
|-------|------------------|
| `protocol_revision` | `E-AP v1.1` + `E2_PROTOCOL.md v1.2` |
| `analysis_plan_commit` | Git SHA of locked `E_ANALYSIS_PLAN.md` |
| `step2_plan_commit` | Git SHA of locked Step 2 plan (this section) |
| `inference_backend_identifier` | Provider id from env sidecar (backend-agnostic) |
| `model_identifier` | LLM + embedding model ids from `BenchmarkEnvironment` |
| `environment_hash` | `env_hash` from L2 sidecar |
| `index_hash` | Post-bind index hash |
| `run_id` | Per-run `BenchmarkAttribution.run_id` |
| `benchmark_corpus_identifier` | `v1.2_strict_trio` (E2-01..03) + `corpus_snapshot_id` |
| `evaluation_fingerprint` | `fingerprint_hash` from STRICT summary row |
| `wiring_stage` | `B` |
| `scoring_tier` | `STRICT` |
| `evidence_scope` | `n=3_strict_trio` |
| `artifact_paths` | Frozen JSONL + sidecar paths under `docs/baselines/artifacts/phase_e/` |
| `git_sha` | Thoth (+ submodule if applicable) at run time |

###### Dependencies on previous steps

| Dependency | Reference |
|------------|-----------|
| **E0** | [`E_PHASE_PROTOCOL.md`](E_PHASE_PROTOCOL.md) v0.1 🔒 |
| **E1 — locked (hard gate)** | [`E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) **E-AP v1.1 🔒 committed** — Step 2 executes frozen protocol verbatim |
| **EP-01 — complete (hard gate)** | § E.0.0 EP-01 ✅ — `THOTH_E2_EP01=1` green (2026-07-09) |
| **Phase D** | [`PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md) — authority preserved |
| **Phase B** | E2-28 contract + fingerprint discipline (historical baseline — not substituted for Phase E runs) |
| **Benchmark E1** | [`benchmark_environment.md`](benchmark_environment.md) — L2 pinning spec |
| **E2 protocol** | [`E2_PROTOCOL.md`](E2_PROTOCOL.md) v1.2 — STRICT rules frozen |

> **Preregistration rule:** Step 2 may not begin until E1 is locked **and EP-01 is complete**. Step 2 does not reinterpret the analysis plan or patch harness infrastructure.

###### Pause

**STATUS: STEP 2 COMPLETE — PAUSED BEFORE STEP 3**

Authoritative STRICT evidence is sealed. Do **not** begin Step 3 (L4 package) until Step 3 plan is locked and approved (AGENTS.md gate).

---

##### E.0.0 Step 3 — L4 reproducibility package (**plan pending**)

**Status:** 📋 **Plan pending** — must conform to § **E.0.0 Planning format lock** before draft.

*Placeholder: L4 manifests, verification doc, baseline compare tooling — delivers E-Q2 complete.*

---

##### E.0.0 Step 4 — claims audit (**plan pending**)

**Status:** 📋 **Plan pending** — must conform to § **E.0.0 Planning format lock** before draft.

*Placeholder: cold-read audit · paper sentence → evidence tier map — delivers E-Q4.*

---

##### E.0.0 Step 5 — phase close-out (**plan pending**)

**Status:** 📋 **Plan pending** — must conform to § **E.0.0 Planning format lock** before draft.

*Placeholder: `PHASE_E_COMPLETE.md` · E-Q1..Q5 seal · max evidence scope — delivers E-Q5.*

---

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
Done    E2 Phase D4 Step 2 — E2-D4-01 live plugin path (`THOTH_E2_D4_01=1`) ✅ 2026-07-07
Done    E2 Phase D4 Step 3 — E2-D4-02 STRICT authority preservation (`THOTH_E2_D4_02=1`) ✅ 2026-07-08
Done    E2 Phase D4 Step 4 — backward-compat regressions (`THOTH_E2_D4_STEP4=1`) ✅ 2026-07-08
Done    E2 Phase D4 Step 5 — composition proof (`THOTH_E2_D4=1`) ✅ 2026-07-08
Done    E2 Phase D5 Step 1 — authority meta-proof (`THOTH_E2_D5_AUTHORITY=1`) ✅ 2026-07-08
Done    E2 Phase D5 Step 2 — behavioral preservation (`THOTH_E2_D5_C5=1`) ✅ 2026-07-08
Done    E2 Phase D5 Step 3 — determinism meta-proof (`THOTH_E2_D5_DETERMINISM=1`) ✅ 2026-07-08
Done    E2 Phase D5 Step 4 — phase closure (`THOTH_E2_D5=1`) + `PHASE_D_COMPLETE.md` ✅ 2026-07-08
Next 1  **Phase E Step 3** — L4 reproducibility package (§ **E.0.0 Step 3** pending — plan draft required)
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
