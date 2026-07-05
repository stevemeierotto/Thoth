# Phase C — Integration Tier Protocol

**Protocol version:** C v1.1  
**Status:** 🔒 Locked for implementation — design complete; C1 not yet opened  
**Supersedes:** C v1.0  
**Depends on:** [`E2_PROTOCOL.md`](E2_PROTOCOL.md) v1.2 (locked 2026-07-01), Phase B v1 baseline ([`phases/PHASE_B_COMPLETE.md`](phases/PHASE_B_COMPLETE.md))  
**Checkpoint tracking:** `cursor_list.md` § **C.0.0**

> **Scope:** Phase C is **integration**, not invention. The evaluator is finished. Phase C moves the E2 measurement system into the production cognitive architecture without changing agent behavior or Phase B evaluation semantics.

---

## Phase narrative

Each E2 phase answers one question in sequence:

| Phase | Question |
|-------|----------|
| **A** | Can **execution** be trusted? |
| **B** | Can **measurement** be trusted? |
| **C** | Can trusted measurement become **architecture**? |
| **D** | Can architecture **evolve** without losing trust? |
| **E** | Can we **defend the results** scientifically? |

Phase B established a reproducible, authoritative STRICT baseline. Phase C wires that contract into the live system as passive infrastructure. Phases D and E are out of scope here but define what comes after integration is proven.

---

## C0 — Integration boundary (informational)

Not an implementation checkpoint. Defines the boundary between the benchmark world and the architecture.

Phase B established the **authoritative evaluation contract**.

Phase C **consumes** that contract unchanged.

The integration layer shall treat Phase B as an **immutable dependency**. Any change requiring modification of Phase B semantics (`evaluation_resolution`, `e2_outcome`, fingerprint derivation, diagnosis buckets #1–#4) constitutes a **new protocol version** (E2 v1.3+) and is **out of scope for Phase C**.

**Answer for future contributors:** “Can I tweak evaluation while integrating it?” → **No. That is a protocol change, not integration work.**

| World | Role in Phase C |
|-------|-----------------|
| **Benchmark** (`wiring_stage=B`) | Authoritative regression gate — unchanged |
| **Phase B contract** | Immutable dependency — consumed, not modified |
| **Integration layer** | Wiring only — service, events, diagnostics, telemetry |

---

## Dependency flow (layer stack)

Ownership and dependency direction are fixed:

```
Execution
    ↓
Episode
    ↓
Evaluation
    ↓
Diagnostics
    ↓
Consumers
```

| Layer | Owns | Phase C role |
|-------|------|--------------|
| **Execution** | State | Unchanged — planning, retrieval, memory, goals |
| **Episode** | Completed-episode observation payload | C2 publication event |
| **Evaluation** | Interpretation | C1 service — Phase B kernel, frozen |
| **Diagnostics** | Presentation | C3 — timelines, explanations (JSONL only) |
| **Consumers** | UI, dashboards, reports | **Out of scope** — built later |

### Dependency invariant

> **Dependencies flow downward only.** Lower layers may consume higher-layer outputs, but higher layers shall not depend on lower layers.

| Allowed | Forbidden |
|---------|-----------|
| Diagnostics reads evaluation output | Evaluation imports diagnostic types |
| Evaluation reads episode payload | Episode event waits on diagnostic flush |
| Episode event emitted after execution | Execution calls evaluation synchronously |
| Consumers read diagnostics JSONL | Evaluation depends on GUI or dashboard code |

This prevents circular dependencies between execution, measurement, and presentation.

---

## Phase C architectural invariant

> **Evaluation is a passive architectural service. It may observe execution, but it must never influence execution.**

This single sentence governs every checkpoint (C1–C5) and explains almost every forbidden rule. Checkpoints are organized by functionality — service extraction, Executive wiring, diagnostics, telemetry, validation — but they all stem from this principle.

### Consequences (observe, don't influence)

| Forbidden influence | Why |
|---------------------|-----|
| Modify planning | Execution owns planning; evaluation observes outcomes |
| Modify retrieval | Retrieval dispatch is Executive/kernel territory |
| Change memory | Memory writes are execution-side effects |
| Alter execution order | Evaluation runs after episodes complete |
| Change goal success | Goal completion is independent of evaluation |
| Inject prompts | Evaluation must not feed back into the LLM loop |

When evaluation is disabled, the Executive path must be **behavior-identical** to pre–Phase C production (modulo no-op hook sites that compile away or short-circuit).

---

## One sentence

Phase C moves E2 from a benchmark harness into the architecture while preserving Phase B's evaluation semantics and reproducibility guarantees.

---

## Theme: Integration Tier

### Goal

Integrate the E2 measurement system into the production cognitive architecture so evaluation becomes an **architectural service** rather than a standalone benchmark executable.

### What Phase C is not

| Not Phase C | Why |
|-------------|-----|
| Invent a better evaluator | Finished in Phase B |
| Redefine `evaluation_resolution` or `e2_outcome` | Frozen at B5 — requires E2 v1.3 |
| Change STRICT scoring or fingerprint derivation | Phase B v1 contract is authoritative |
| Use INTEGRATION results as benchmark evidence | E2-INTEGRATION remains diagnostic only |
| Dashboards, UI, visualization, reporting | **Consumers** — Phase C is infrastructure only |
| Modify Executive planning, retrieval ranking, or tool dispatch | Violates passive-service invariant |

### Relationship to E2-INTEGRATION tier

[`E2_PROTOCOL.md`](E2_PROTOCOL.md) defines **E2-INTEGRATION** as a non-scoring diagnostic evaluation mode (`official_scoring: false`, no `e2_outcome`). Phase C **consumes** that tier definition. Phase C checkpoints add **architectural plumbing** — service boundaries, episode publication, diagnostics, telemetry — so production runs can feed completed episodes to the evaluation service **without** altering STRICT benchmark authority.

**Rule:** Phase B benchmark (`wiring_stage=B`, `official_scoring: true`) remains the sole authoritative regression gate throughout Phase C. Every C checkpoint must preserve E2-28 scoped equivalence against the frozen Phase B v1 fingerprint.

---

## Architectural target

### Today (post–Phase B)

```
run_episodic_learning_benchmark
        │
        ├── execute arms (Executive / kernel)
        ├── evaluateEpisodicLearningCase()   ← evaluation lives inside harness
        ├── resolveEvaluation / export
        └── JSONL append
```

Evaluation logic is **correct** but **coupled** to the benchmark executable.

### Target (post–Phase C)

```
ExecutiveController
        │
        ├── normal execution (planning, retrieval, tools, memory)
        │         │
        │         ▼
        │   Episode Completed  ──publish──▶  Evaluation Service (passive, optional)
        │                                              │
        │                                              ├── Evaluation (frozen kernel)
        │                                              ├── Diagnostics (presentation)
        │                                              └── Export / telemetry (JSONL)
        │
        └── (evaluation OFF by default — no influence on execution path)
```

The Executive does **not** know how evaluation works. It publishes a **completed-episode event**. The evaluation service subscribes, observes, and interprets — it never calls back into execution.

---

## Phase C checkpoints

Five logical stages. All derive from the passive-service invariant. Pause for review between each — same discipline as Phase B.

| Checkpoint | Purpose | Relative size |
|------------|---------|---------------|
| **E2-C1** | Evaluation service extraction | Medium |
| **E2-C2** | Episode publication (Executive → service) | Medium |
| **E2-C3** | Diagnostic layer (presentation only) | Large |
| **E2-C4** | Architectural telemetry (separate from benchmark metrics) | Medium |
| **E2-C5** | Production validation (path equivalence) | Small |

There is **no** “invent a better evaluator” checkpoint.

---

### E2-C1 — Evaluation Service Boundary

**Question:** Can evaluation be invoked as a service without living inside the benchmark binary?

#### Goal

Separate evaluation from the benchmark executable.

```
Before                          After

Benchmark                       Benchmark
    │                               │
    ├── Execution                   ├── Execution (orchestrates)
    ├── Evaluation                  ├── Evaluation Service (calls)
    └── Export                      └── Export

                                Evaluation Service
                                    │
                                    ├── Resolution
                                    ├── Summary
                                    ├── Fingerprint
                                    └── Equivalence
```

**C1 does not prove production integration or path equivalence** — that is C5. C1 only proves the evaluation kernel can live behind a service boundary while the benchmark remains authoritative and semantically identical.

#### C1 service lifetime invariant

> **The evaluation service is stateless.** All evaluation state is supplied explicitly through method parameters and returned values. The service shall retain **no cross-run state**.

This prevents cached evaluation state (e.g. in Phase D) from introducing nondeterminism.

#### Façade rule (not a redesign)

The service is a **façade**, not a redesign. Method boundaries intentionally mirror the existing Phase B free functions (`evaluateEpisodicLearningCase`, `resolveEvaluation`, `summarizeEpisodicLearning`, fingerprint helpers) to minimize behavioral risk. **Consolidation may occur only after Phase C.**

#### Deliverables

| Item | Requirement |
|------|-------------|
| **Evaluation interface** | Extract a stable service boundary (e.g. `IEpisodicEvaluationService` or equivalent) — 1:1 façade over Phase B functions |
| **Dependency direction** | **Allowed:** Benchmark → Evaluation Service. **Forbidden:** Evaluation Service → Benchmark. The service shall be **buildable and unit-testable without linking the benchmark executable** |
| **Service contract** | Documented inputs (observations, `E2EvalConfig`, block reasons) and outputs (`EpisodicLearningCaseEvaluation`, `EpisodicLearningSummary`) — **no new fields** |
| **Export path** | JSONL builders remain single-authority (`caseEvaluationToJson`, `episodicLearningSummaryLogRow`) — callable from service or thin harness wrapper; benchmark owns log append |
| **Passive contract** | Service API has no callbacks into Executive, planner, RAG, or memory |
| **Ownership** | **Benchmark orchestrates. Service evaluates.** Benchmark executable contains no evaluation algorithm |

#### Forbidden

- Change `resolveEvaluation()` semantics
- Add harness-only logic into the service (see E2-C1-03)
- Split STRICT / INTEGRATION scoring rules
- Service methods that mutate execution state
- Cross-run state inside the service (cached fingerprints, summaries, config)
- Service dependency on `run_episodic_learning_benchmark` or harness-only targets
- Premature API consolidation during C1 extraction

#### Exit criteria

1. Service interface exists and is linked by benchmark (thin caller)
2. **Benchmark executable contains no evaluation algorithm** — orchestration only
3. Service is stateless — no cross-run retained state
4. Service builds/tests without benchmark executable linkage
5. `run_episodic_learning_benchmark` with `wiring_stage=B` passes unchanged
6. Phase B v1 fingerprint unchanged (E2-28 gate on two consecutive `B` runs)
7. E2-25–E2-28 + E2-C1-01–E2-C1-03 green
8. **Pause before C2**

---

### E2-C2 — Episode Publication

**Question:** Can the Executive publish completed episodes without knowing how evaluation works?

**C1 answered:** Can evaluation exist independently?  
**C2 answers:** Can execution publish information without knowing evaluation exists?

#### Goal

The Executive **publishes** a completed-episode event. It does **not** invoke evaluation, subscribe to results, or know which consumers exist.

```
Before (post-C1)                 After (C2)

Benchmark → Evaluation Service   Benchmark → Evaluation Service  (unchanged)

(no production path)             Executive
                                     │ publishes (fire-and-forget)
                                     ▼
                               Event channel
                                     │ delivers (outside Executive)
                                     ▼
                               EvaluationSubscriber (first consumer)
                                     ▼
                               Evaluation Service
```

Future consumers (Phase D+): `ReplaySubscriber`, `MetricsSubscriber`, `TraceSubscriber` — Executive unchanged.

#### Publication vs delivery

| Phase | Owner | Responsibility |
|-------|-------|----------------|
| **Publication** | Executive | Create and emit immutable `EpisodeCompleted` — fire-and-forget, **best-effort** |
| **Delivery** | Event channel | Route event to registered consumers — **outside Executive** |
| **Consumption** | Subscribers (e.g. `EvaluationSubscriber`) | Receive immutable event; derive side effects |

> **Publication is fire-and-forget. Delivery is outside Executive responsibility.**

> **Best-effort invariant:** Failure to publish or consume an `EpisodeCompleted` event must **never** affect Executive execution, plan completion, or goal outcome.

#### `EpisodeCompleted` — execution-domain event

`EpisodeCompleted` belongs to the **execution domain**, not the evaluation domain. It is immutable, reusable by evaluation, telemetry, replay, and tracing consumers. It must **not** embed evaluation-specific semantics (`evaluation_resolution`, `e2_outcome`, fingerprint fields, etc.).

#### Trajectory-to-observation mapping invariant

> **Trajectory-to-observation mapping is owned exclusively by the subscriber** (`EvaluationSubscriber`). The mapping must be **deterministic** and **side-effect free**.

Neither the Executive nor the evaluation service shall perform production trajectory extraction or mapping.

#### Subscriber failure isolation

> **Subscriber failures are isolated.** Publication and delivery failures may be logged, but shall **not** alter plan completion, goal success, or Executive state.

| Failure | Effect |
|---------|--------|
| `EvaluationSubscriber` throws | Logged; goal outcome unchanged |
| Event channel delivery error | Logged; Executive already continued |
| Evaluation service error | Contained in subscriber; no Executive callback |

This codifies the passive-service invariant: evaluation is never on the critical execution path.

#### Deliverables

| Item | Requirement |
|------|-------------|
| **`EpisodeCompleted` event** | Immutable struct — goal/plan ids, attribution, terminal trajectory facts sufficient for subscriber mapping |
| **Event channel** | `IEpisodeEventChannel` (or equivalent) — accepts publication; delivers to registered subscribers; no eval logic |
| **Publication hook** | Executive publishes to channel on terminal plan events — gated by feature flag |
| **`EvaluationSubscriber`** | First consumer — maps event → observations → `IEpisodicEvaluationService` (INTEGRATION tier) |
| **Feature flag** | `enable_episodic_evaluation_publication: false` default — publication **OFF** unless explicitly enabled |
| **Tier routing** | STRICT only via benchmark; `EvaluationSubscriber` uses **INTEGRATION** or no-op |
| **Non-authoritative envelope** | Subscriber output: `official_scoring: false`; never `wiring_stage=B` from ambient runs |

#### Forbidden

- Executive calling evaluation logic or importing `IEpisodicEvaluationService`
- Executive knowing subscriber count or consumer identity
- Auto-enable publication in default production config
- Emit `official_scoring: true` outside benchmark `wiring_stage=B`
- Alter Executive state machine transitions, planner calls, or retrieval dispatch
- Require evaluation for goal completion
- Subscriber failures affecting goal outcome or Executive state
- Evaluation service calling back into Executive
- Trajectory extraction in Executive or evaluation service (subscriber only)
- Mutable `EpisodeCompleted` shared with Executive after publish

#### Exit criteria

1. Executive publishes immutable `EpisodeCompleted` when flag enabled; zero eval imports
2. Event channel delivers fire-and-forget; delivery outside Executive
3. `EvaluationSubscriber` consumes events; trajectory mapping in subscriber only
4. Subscriber failure does not change plan completion or goal success (E2-C2-04 related)
5. Executive behavior unchanged when flag disabled
6. Benchmark `wiring_stage=B` path unchanged and authoritative
7. Subscriber output: INTEGRATION-tier envelope or silent — never authoritative `e2_outcome`
8. E2-C2-01–E2-C2-04 green
9. **Pause before C3**

---

### E2-C3 — Diagnostic Layer

**Question:** Can diagnostics explain evaluation output without becoming a second evaluator?

**E2-C1 answered:** Can evaluation exist independently? ✅  
**E2-C2 answered:** Can execution publish without knowing evaluation? ✅  
**E2-C3 answers:** Can we explain evaluation results without participating in producing them?

#### C3.0 — Hard boundary invariant (diagnostic isolation)

> **Diagnostics explain evaluation — they do not participate in producing it.**

Diagnostics must **NOT**:

| Forbidden | Why |
|-----------|-----|
| Call evaluation service (`evaluateCase`, `resolveEvaluation`, `summarize`) | Evaluation already complete |
| Call Executive or access `EpisodeCompleted` | Execution domain — C2 owns the bridge |
| Subscribe to event channel directly | Must consume **evaluation outputs only** |
| Modify evaluation outputs | Presentation layer is read-only |
| Influence scoring or execution | Passive downstream layer |
| Influence evaluation selection, scoring, or output formatting | No feedback into evaluation — ever |

Diagnostics are **read-only interpretation** of evaluation results only.

> **Diagnostics are not part of the evaluation contract.** They are downstream presentation — not extended evaluation output.

> **Future-proofing:** Diagnostics must never influence evaluation selection, scoring, or output formatting. No "adjust classification buckets based on diagnostic feedback" — that breaks Phase B/C separation immediately.

#### Data ownership (unchanged)

| Layer | Owns |
|-------|------|
| **Execution** | State |
| **Evaluation** | Interpretation (`evaluation_resolution`, lift, fingerprint inputs) |
| **Diagnostics** | Presentation — explanations, classification, trace links |

#### Pipeline (matches implemented E2-C2 reality)

```
EpisodeCompleted          ← execution domain (E2-C2)
      ↓
EvaluationSubscriber    ← C2: mapping + service invocation
      ↓
EvaluationService       ← C1: frozen Phase B kernel
      ↓
DiagnosticService       ← E2-C3: pure interpretation
      ↓
Logs / artifacts        ← JSONL only; no UI/dashboards
```

**Key rule:** Diagnostics see **evaluation outputs only** — never raw execution events, never the event channel, never Executive state.

#### C3.1 — Input contract (C2 output only)

Diagnostics consume outputs produced by `EvaluationSubscriber` after evaluation completes:

| Input | Source |
|-------|--------|
| `EpisodicLearningCaseEvaluation` | Subscriber post-`applyCaseResolution` |
| `EpisodicLearningSummary` | Subscriber post-`summarize` |
| `EvaluationDiagnosticsContext` | Thin wrapper: `run_id`, `env_hash`, `fingerprint_hash`, `e2_eval_config` snapshot |
| `evaluation_resolution` | Read from evaluation structs — **never re-derived** |

**`EvaluationDiagnosticsContext` ownership rule:** Context carries run/config attribution only. It must **not** include evaluation results or derived fields (`evaluation_resolution`, scores, classification, lift, diagnosis buckets). Those belong in the evaluation artifact inputs — not the context wrapper. Prevents layering creep.

Inputs do **not** come from Executive, `EpisodeCompleted`, or event channel.

#### C3.2 — Diagnostic service

**Interface:** `IDiagnosticService`  
**Implementation:** `EvaluationDiagnosticService` (stateless façade)

| Responsibility | Detail |
|----------------|--------|
| Map evaluation → explanation | Human-readable `structured_explanation` |
| Classify failure modes | E2-28 diagnosis buckets #0–#4 |
| Per-case diagnostics | `generateDiagnostics(eval, config, context)` |
| Run-level summary | `generateRunDiagnostics(summary, context)` |

| Must NOT | |
|----------|--|
| Re-score / re-evaluate | |
| Recompute fingerprint | |
| Mutate evaluation data | |
| Trigger evaluation service | |

#### C3.3 — Output schema: `EvaluationDiagnostics`

| Field | Purpose |
|-------|---------|
| `run_id` | Attribution |
| `case_id` | Optional per-case |
| `evaluation_resolution_snapshot` | Copy from evaluation — not recomputed |
| `diagnosis_bucket` | #0–#4 per Phase B fingerprint taxonomy |
| `failure_classification` | `config_mismatch` \| `corpus_drift` \| `retrieval_nondeterminism` \| `semantic_drift` \| `none` |
| `structured_explanation` | Narrative from `run_block_reason`, arm status, resolution |
| `trace_links` | Optional ordered references (evaluation input ids — not execution replay) |

**Canonical JSONL `event_type` values (E2-C3 only — no alternates):**

| `event_type` | Scope |
|--------------|-------|
| `E2_EVAL_DIAGNOSTIC_CASE` | Per-case diagnostic row |
| `E2_EVAL_DIAGNOSTIC_SUMMARY` | Run-level diagnostic summary |

Separate from authoritative `EPISODIC_LEARNING_*` rows. Do **not** use `E2_DIAGNOSTIC_*` (without `EVAL`) or other variants — prevents `E2` / `EPISODIC` / `EVAL` namespace drift.

#### C3.4 — Integration point

C3 runs as a **post-processing stage** after the `EvaluationSubscriber` has produced evaluation artifacts (per-case evaluation + run summary). C2 owns evaluation invocation and summary generation; C3 consumes those artifacts — it is not a hook inside subscriber evaluation logic.

```cpp
// Subscriber has finished: evaluateCase → applyCaseResolution → summarize
const auto diagnostics = diagnosticService.generateRunDiagnostics(summary, context);
// Append diagnostic JSONL — non-authoritative
```

Implementation may live inside the subscriber pipeline, a dedicated post-eval callback, or a thin orchestrator — all valid as long as C3 sees **completed evaluation artifacts only**, never raw execution events or the event channel.

#### C3.5 — Determinism requirement

Diagnostics must be:

- **Deterministic** — pure function of evaluation output + context
- **Independent of runtime state** — no cross-run cache
- Same evaluation input → same diagnostics output

#### C3.6 — Implementation deliverables

| # | Deliverable |
|---|-------------|
| 1 | `diagnostic_service.h` / `diagnostic_service.cpp` in `e2_eval_kernel` |
| 2 | `EvaluationDiagnosticsContext` struct |
| 3 | `generateDiagnostics()` per case |
| 4 | `generateRunDiagnostics()` run aggregator |
| 5 | JSONL serializers (`evaluationDiagnosticsToJson`) |
| 6 | Post-evaluation stage invokes diagnostics after subscriber artifacts are complete |
| 7 | E2-06 INTEGRATION harness path verification (or extend subscriber test coverage) |

No file-path mandates in protocol — placement follows dependency boundaries (diagnostics depend on eval outputs only; eval service does not import diagnostics).

#### C3.7 — Tests (preregistered)

| ID | Asserts |
|----|---------|
| **E2-C3-01** | Diagnostics do NOT call `evaluateCase`, `resolveEvaluation`, scoring functions |
| **E2-C3-02** | Same evaluation input → identical diagnostics output |
| **E2-C3-03** | Diagnostics do not modify `evaluation_resolution`, `fingerprint_hash`, scoring results |
| **E2-C3-04** | C2→C3 pipeline preserves E2-25–E2-28, fingerprint stability, SCORED_SUCCESS on benchmark path |

#### C3.8 — Structural audits

**Dependency rule (elevated):** Evaluation → Diagnostics is a **one-way** dependency. Diagnostics may import evaluation **exported data structures only** — not evaluation internals, shared utility helpers, or scoring implementation details. Prevents accidental coupling via helper creep.

| Audit | Requirement |
|-------|-------------|
| Evaluation service | Does NOT import diagnostics |
| Diagnostics | Depend ONLY on exported evaluation output types |
| No reverse edges | Diagnostics → eval internals forbidden; event channel → diagnostics forbidden |
| No shared utility creep | Diagnostic code must not call private eval/scoring helpers |

#### Forbidden

- Diagnostic layer as second authority for `evaluation_resolution` or `e2_outcome`
- Diagnostics that change pass/fail or lift
- INTEGRATION-vs-STRICT comparison as benchmark improvement
- Dashboards, GUI, visualization, reporting UI (consumers — out of scope)
- Direct `EpisodeCompleted` or Executive access in diagnostic code

#### Exit criteria

1. `IDiagnosticService` exists; consumes only C2 evaluation outputs
2. Structured `EvaluationDiagnostics` JSONL emitted post-evaluation
3. E2-C3-01–E2-C3-04 green
4. E2-06 INTEGRATION rules hold — `official_scoring: false`, no `e2_outcome` on diagnostic authority path
5. STRICT benchmark regression unchanged (Phase B fingerprint + E2-28)
6. Full C2→C3 pipeline preserves `evaluation_resolution` and fingerprint semantics
7. **Pause before E2-C4**

**E2-C3 summary:** Introduces interpretability without touching correctness — pure downstream reasoning that explains evaluation but never produces it.

---

### E2-C4 — Architectural Telemetry

**Question:** Can the integrated evaluation pipeline be instrumented without influencing execution, evaluation, or diagnostics?

**E2-C1 answered:** Can evaluation exist independently? ✅  
**E2-C2 answered:** Can execution publish without knowing evaluation? ✅  
**E2-C3 answered:** Can we explain evaluation without producing it? ✅  
**E2-C4 answers:** Can we **measure** the pipeline architecture without participating in it?

#### C4.0 — Hard boundary invariant (telemetry isolation)

> **E2-C4 introduces architectural telemetry that measures the evaluation pipeline while remaining completely observational.**

> **Telemetry is measurement, not decision-making.**

> **Telemetry is not part of the evaluation contract, the diagnostic contract, or the execution contract.**

Telemetry must **NOT**:

| Forbidden | Why |
|-----------|-----|
| Alter execution, planning, retrieval, or memory | Execution domain — C2 owns publication |
| Call evaluation service (`evaluateCase`, `resolveEvaluation`, `summarize`, `computeFingerprint`) | Evaluation already complete |
| Call diagnostic service (`generateDiagnostics`, `generateRunDiagnostics`) | Diagnostics already complete |
| Modify evaluation, diagnostic, or episode artifacts | Read-only measurement |
| Influence evaluation selection, scoring, or output formatting | No feedback into correctness |
| Influence diagnostic classification or explanation text | No feedback into interpretation |
| Change `evaluation_resolution`, `fingerprint_hash`, lift, or `e2_outcome` | Phase B authority frozen |
| Block Executive, subscriber, evaluation, or diagnostics on flush failure | Non-blocking observability |
| Share schema namespace with benchmark metrics | Segregation invariant |

Telemetry **observes only** — never alters execution, evaluation, diagnostics, output formatting, fingerprints, `evaluation_resolution`, or scheduling/control flow.

#### Data ownership (unchanged + telemetry layer)

| Layer | Owns |
|-------|------|
| **Execution** | State, terminal outcomes |
| **Evaluation** | Correctness (`evaluation_resolution`, lift, fingerprint inputs) |
| **Diagnostics** | Interpretation (buckets, failure classification, explanations) |
| **Telemetry** | **Measurement** of pipeline architecture (latencies, counts, throughput) |

Telemetry must **never reinterpret** evaluation results. It records *how long* and *how often* — not *whether the case passed*.

#### Pipeline (matches implemented C1–C3 reality)

```
Execution
    ↓
EpisodeCompleted              ← execution domain (C2)
    ↓
EvaluationSubscriber          ← C2: mapping + orchestration
    ↓
EvaluationService             ← C1: frozen Phase B kernel
    ↓
DiagnosticService             ← C3: pure interpretation
    ↓
TelemetryService              ← C4: architectural measurement
    ↓
JSONL logs / artifacts        ← non-authoritative observability only
```

**Key rule:** Telemetry observes the **completed pipeline** — stage timings and counts recorded **after** evaluation and diagnostic artifacts exist. Telemetry must **never become part of the pipeline** (no gating, no callbacks upstream, no control-flow branches based on telemetry output).

#### C4.1 — Metric classification (must never mix)

**Benchmark metrics** — authoritative measurement; Phase B contract (forbidden in telemetry schema):

| Metric | Authority |
|--------|-----------|
| `success` / `passes` | Evaluation |
| `lift` / `mean_episodic_lift` | Evaluation |
| `evaluation_resolution` | Evaluation (canonical) |
| `evaluation_fingerprint_hash` / `fingerprint_hash` | Evaluation (derived) |
| `e2_outcome` | Export (derived from resolution) |
| `diagnosis_bucket` (#0–#4) | Diagnostics (interpretation) |
| `failure_classification` | Diagnostics (interpretation) |

**Architectural telemetry** — engineering observability; non-authoritative:

| Signal | Purpose |
|--------|---------|
| `publication_to_subscriber_ms` | Executive publish → subscriber entry delay |
| `mapping_duration_ms` | Trajectory-to-observation mapping (subscriber-owned) |
| `evaluation_duration_ms` | `evaluateCase` + `applyCaseResolution` + `summarize` wall time |
| `diagnostic_duration_ms` | Diagnostic generation wall time |
| `pipeline_duration_ms` | Subscriber entry → telemetry record (end-to-end integration path) |
| `episodes_processed` | Count — integration path throughput |
| `queue_delay_ms` | Reserved — `0` or omitted for synchronous in-process channel; field reserved for future async delivery |

**Rule:** Benchmark metrics and architectural telemetry MUST NOT share a schema namespace, dashboard panel, or promotion trigger. Graphing `pipeline_duration_ms` beside `mean_episodic_lift` and treating them as the same kind of evidence is a protocol violation.

All telemetry rows carry `telemetry_tier: "ARCHITECTURE"` — distinct from `scoring_tier: "STRICT"` / `"INTEGRATION"`.

#### C4.2 — Input contract (pipeline artifacts + timing snapshots only)

**Clock invariant (E2-C4):**

| Purpose | Clock |
|---------|-------|
| Duration measurement | `steady_clock` |
| Absolute correlation / timestamps | `system_clock` |

Never derive durations from `system_clock`.

Telemetry consumes **exported timing snapshots and attribution** produced by the subscriber orchestration layer after C1–C3 complete. Telemetry does **not** re-read evaluation or diagnostic internals.

| Input | Source | Allowed content |
|-------|--------|-----------------|
| `E2PipelineTelemetryContext` | Subscriber-built thin wrapper | `run_id`, `env_hash`, `plan_id`, `goal_id` (optional), `episode_completed_at_ms`, `subscriber_entry_ms` |
| `E2PipelineStageTimings` | Subscriber-recorded wall-clock segments | Per-stage `duration_ms` only — no eval/diag payloads |
| Stage labels | Fixed enum / string constants | `mapping`, `evaluation`, `diagnostics`, `pipeline_total` (+ optional `publication_to_subscriber`) |

**`E2PipelineTelemetryContext` ownership rule:** Context carries run/episode attribution and clock anchors only. It must **not** include evaluation results, diagnostic classifications, scores, resolution, lift, fingerprint, or `structured_explanation`. Prevents layering creep.

Inputs do **not** come from Executive callbacks, diagnostic service return values used for reinterpretation, or evaluation service scoring paths.

#### C4.3 — Telemetry service

**Interface:** `IE2PipelineTelemetryService`  
**Implementation:** `E2PipelineTelemetryService` (stateless façade)

| Responsibility | Detail |
|----------------|--------|
| Aggregate stage timings | Sum/validate segments; compute `pipeline_duration_ms` |
| Emit structured telemetry record | `recordPipelineRun(timings, context)` |
| JSONL serialization | `e2PipelineTelemetryToJson(record)` |

| Must NOT | |
|----------|--|
| Re-evaluate / re-diagnose | |
| Recompute fingerprint | |
| Mutate evaluation or diagnostic data | |
| Trigger evaluation or diagnostic services | |
| Branch on metric thresholds to alter upstream behavior | |

**API (proposed):**

```cpp
E2PipelineTelemetryRecord recordPipelineRun(
    const E2PipelineStageTimings& timings,
    const E2PipelineTelemetryContext& context);
```

#### C4.4 — Output schema: `E2PipelineTelemetryRecord`

| Field | Purpose |
|-------|---------|
| `run_id` | Attribution |
| `plan_id` | Episode attribution (optional) |
| `telemetry_tier` | Always `"ARCHITECTURE"` |
| `telemetry_schema_version` | Version pin (e.g. `"1.0"`) |
| `pipeline_duration_ms` | End-to-end integration path duration |
| `stages` | Array of `{stage, duration_ms}` |
| `publication_to_subscriber_ms` | Optional — `subscriber_entry_ms - episode_completed_at_ms` |
| `episodes_processed` | Count (typically `1` per event) |
| `queue_delay_ms` | Reserved; `0` or omitted for in-process channel |

**Canonical JSONL `event_type` values (E2-C4 only — no alternates):**

| `event_type` | Scope |
|--------------|-------|
| `E2_EVAL_TELEMETRY_PIPELINE` | Per-episode pipeline timing record |

Separate from `EPISODIC_LEARNING_*` (evaluation authority) and `E2_EVAL_DIAGNOSTIC_*` (interpretation). Do **not** use `E2_TELEMETRY_*` (without `EVAL`) or other variants.

#### C4.5 — Integration point

C4 runs as a **post-processing stage** after the subscriber has produced evaluation artifacts, diagnostic artifacts, and stage timing snapshots. C4 is **not** a hook inside evaluation or diagnostic logic.

```cpp
// Subscriber has finished: mapping → evaluateCase → summarize → generateRunDiagnostics
// Stage segments captured with steady_clock; system_clock anchors for correlation only
const auto record = telemetryService.recordPipelineRun(timings, context);
// Synchronous computation; best-effort persistence — failures swallowed
```

**Integration model:** **synchronous computation, best-effort persistence.** `recordPipelineRun` and JSON assembly run inline in the subscriber tail; persistence/emission is best-effort only (try/catch, swallow). No async dispatch, no upstream gating.

Implementation may live at the end of `EvaluationSubscriber::onEpisodeCompleted`, a dedicated post-pipeline callback, or a thin orchestrator — all valid as long as telemetry sees **completed pipeline timing snapshots only** and never blocks upstream stages.

**Non-blocking invariant:** Telemetry emission failures are logged and discarded. They must not propagate to Executive, event channel, evaluation, or diagnostics.

**Checkpoint 2 fingerprint separation:** Integration-subscriber proof does not assert STRICT `fingerprint_hash`. Unit-test fixture `makeE2StrictTestConfig()` uses `corpus_snapshot_id="e2-test-corpus"`; official harness uses `corpus_snapshot_id=index_hash()` — intentional E2-28 bucket #1 divergence. Phase B baseline fingerprint is verified only on the `wiring_stage=B` harness path.

#### C4.6 — Config / feature flag

| Flag | Default | Behavior |
|------|---------|----------|
| `enable_episodic_pipeline_telemetry` | `false` | When OFF: no telemetry records emitted; subscriber may still collect timings locally for tests |
| `enable_episodic_evaluation_publication` | `false` (existing) | Telemetry is meaningful only when publication path is active; telemetry flag does not auto-enable publication |

Telemetry follows the same **opt-in, default OFF** family as C2. Enabling telemetry must not change execution, evaluation, or diagnostic outputs when compared run-to-run with telemetry disabled.

#### C4.7 — Determinism requirement

Telemetry records are **derived from wall-clock measurements** — they are deterministic in *structure* (same stages, same schema) but not in *numeric values* across runs. Tests assert schema shape and non-interference, not exact millisecond equality.

Telemetry must be independent of evaluation correctness: a failed case and a passed case produce telemetry of the same schema.

#### C4.8 — Implementation deliverables

| # | Deliverable |
|---|-------------|
| 1 | `pipeline_telemetry_service.h` / `pipeline_telemetry_service.cpp` in `e2_eval_kernel` |
| 2 | `E2PipelineTelemetryContext`, `E2PipelineStageTimings`, `E2PipelineTelemetryRecord` structs |
| 3 | `IE2PipelineTelemetryService` + stateless `E2PipelineTelemetryService` |
| 4 | `recordPipelineRun()` + `e2PipelineTelemetryToJson()` |
| 5 | Stage timing capture in subscriber orchestration (`steady_clock` durations; `system_clock` anchors only) |
| 6 | Post-pipeline telemetry: synchronous computation + best-effort persistence |
| 7 | `Config::enable_episodic_pipeline_telemetry` (default `false`) |
| 8 | Tests E2-C4-01–E2-C4-05 |

No dashboards, GUI, visualization, or `DecisionTraceLogger` schema mixing in C4 scope.

**Testing observation interfaces** (not production telemetry API): `lastStageTimingsForTests()`, `lastTelemetryRecordForTests()` on `EvaluationSubscriber` — permanent hooks for unit tests and checkpoint verification; same family as C2/C3 `lastSummaryForTests()` / `lastRunDiagnosticsForTests()`.

#### C4.9 — Tests (preregistered)

| ID | Asserts |
|----|---------|
| **E2-C4-01** | Telemetry JSONL emitted when flag ON; schema has `telemetry_tier: "ARCHITECTURE"`; **excludes** benchmark fields (`evaluation_resolution`, `lift`, `passes`, `fingerprint_hash`, `e2_outcome`, `diagnosis_bucket`, `failure_classification`) |
| **E2-C4-02** | Telemetry failure (injected throw) does not alter Executive/subscriber goal outcome; evaluation + diagnostic artifacts unchanged |
| **E2-C4-03** | Structural audit — telemetry code does NOT call `evaluateCase`, `generateRunDiagnostics`, `resolveEvaluation`, or scoring/diagnostic helpers |
| **E2-C4-04** | Non-interference — telemetry does not modify evaluation, diagnostic, or fingerprint outputs (same inputs before/after `recordPipelineRun`) |
| **E2-C4-05** | Full regression — E2-25–E2-28, E2-C1-01–03, E2-C2-01–04, E2-C3-01–04 unchanged; `wiring_stage=B` fingerprint stable |

#### C4.10 — Structural audits

**Dependency rule (elevated):** Pipeline → Telemetry is a **one-way** dependency. Telemetry may import **exported attribution/timing structs only** — not evaluation internals, diagnostic internals, Executive internals, or shared scoring helpers.

| Audit | Requirement |
|-------|-------------|
| Evaluation service | Does NOT import telemetry |
| Diagnostic service | Does NOT import telemetry |
| Executive | Does NOT import telemetry; not blocked on telemetry flush |
| Telemetry | Does NOT import `episodic_evaluation_service`, `diagnostic_service` implementation paths, or `executive_controller` |
| No reverse edges | Telemetry → eval/diag/exec forbidden |
| No shared utility creep | Telemetry must not call private eval/diagnostic helpers |

#### Forbidden

- Treating telemetry aggregates as official E2 evidence
- Mixing architecture telemetry into `evaluation_fingerprint` canonical JSON
- Blocking Executive or subscriber on telemetry flush
- Combining benchmark metrics and telemetry in a single rollup field
- Telemetry that changes pass/fail, lift, resolution, or diagnosis buckets
- INTEGRATION-vs-STRICT telemetry comparison as benchmark improvement
- Extending `DecisionTraceLogger` with benchmark metric fields in C4
- Dashboards, GUI, visualization (consumers — out of scope)
- Path equivalence claims (deferred to E2-C5)

#### Exit criteria

1. `IE2PipelineTelemetryService` exists; consumes only pipeline timing snapshots + attribution
2. Structured `E2_EVAL_TELEMETRY_PIPELINE` JSONL emitted post-pipeline when flag ON
3. E2-C4-01–E2-C4-05 green
4. E2-06 INTEGRATION rules hold — telemetry rows carry no `e2_outcome` or authoritative scoring fields
5. STRICT benchmark regression unchanged (Phase B fingerprint + E2-28)
6. Full C1→C2→C3→C4 pipeline preserves `evaluation_resolution` and fingerprint semantics
7. **Pause before E2-C5**

**E2-C4 summary:** Introduces architectural observability without touching correctness or interpretation — pure downstream measurement that records pipeline behavior but never participates in producing or explaining results.

**E2-C4 telemetry closure:** `TelemetryService` is a **pure sink/formatter** — timings and pipeline metadata in, `E2_EVAL_TELEMETRY_PIPELINE` JSON out. It must not interpret evaluation results, classify outcomes, derive success/failure meaning, or access diagnostic semantics.

**E2-C4 does not prove:** Path equivalence (benchmark vs production resolution match) — that is **E2-C5 only**.

**Status:** ✅ **E2-C4 complete** (2026-07-05). Checkpoints 1–3 green; E2-C4-01–E2-C4-05 + E2-25–E2-28 + E2-C1–E2-C3 regression green; `wiring_stage=B` harness SUCCESS. **Paused before E2-C5.**

---
### E2-C5 — Path Equivalence Validation

**Question:** Does the production integration pipeline produce the same evaluation semantics as the benchmark pipeline — when both are driven by equivalent kernel inputs under pinned evaluation configuration?

> **C5 is a proof, not a feature.** No changes to evaluation logic, diagnostics, telemetry, or scoring systems.

#### C5.0 — Scope label (read this first)

C5 proves **equivalence under pinned evaluation semantics** — not raw production-runtime vs benchmark-runtime comparison.

| What C5 compares | What C5 does NOT compare |
|------------------|--------------------------|
| Benchmark orchestration (`runScoredEvaluationLoop` kernel path) | Default INTEGRATION subscriber config vs STRICT harness config as an apples-to-apples object equality test |
| Production orchestration (`EpisodeCompleted` → subscriber → eval → diag) under **test-pinned config** | “Production behavior” with live INTEGRATION defaults (that is C2/C3 envelope coverage) |
| Evaluation kernel outputs after normalization to identical observation inputs | Raw `E2EvalConfig` object identity across tiers |

Production defaults **INTEGRATION**; benchmark uses **STRICT**. That is intentional and unchanged. C5 controlled tests pin equivalent semantic configuration on both paths via a **test-only** subscriber config seam.

#### C5.1 — Core invariant

Both paths must produce identical evaluation semantics for mapping-compatible episodes under pinned config:

| Field | Compared |
|-------|----------|
| `evaluation_resolution` | Per-case + summary |
| Scorable classification | `scorable_cases`, arm scoring status |
| `not_scorable_by_reason` | Per-case + summary rollup |
| `failure_classification` | Per-case + run diagnostic |
| `diagnosis_bucket` (#0–#4) | Per-case + run diagnostic |

**Ignored in diff:** timestamps, telemetry rows, log paths, envelope-only tier labels (`official_scoring`, `wiring_stage`) unless explicitly testing INTEGRATION envelope rules (deferred to C2/C3).

#### C5.2 — Config semantics (corrected)

> C5 compares **evaluation equivalence under equivalent semantic configuration** — not `E2EvalConfig` object equality across paths.

The test-only seam `setEvaluationSubscriberEvalConfigForTests(std::optional<E2EvalConfig>)`:

- **Allowed:** Pin STRICT semantic config during C5 controlled tests so both paths invoke the same evaluation kernel rules
- **Forbidden:** Alter production execution when the seam is unset; compile-time or runtime production dependency on test hooks
- **Label:** “Production pipeline under strict-mode simulation” — orchestration proof, not live INTEGRATION tier proof

When unset, subscriber retains `integrationDefaults()` — production path unchanged.

#### C5.3 — Fingerprint rule (corrected)

`fingerprint_hash` equality across paths is valid **only when**:

1. Both paths use identical corpus pins, retrieval pins, strict config pins, and ordering rules
2. Fingerprint is computed **after** normalization into evaluation-kernel inputs (post-mapping, pre-export)
3. Comparison uses the same `computeFingerprint(pinnedConfig)` call — not raw path-local state before normalization

Otherwise expect false failures. Tests must document pinned config used for fingerprint comparison.

#### C5.4 — Mapping fidelity (formal tolerance)

`mapEpisodeToProductionObservations` is a **lossy encoder**. Before any path-equivalence proof (Checkpoints 1–3), Checkpoint 0 validates **mapping fidelity** — that the production mapper preserves all evaluation-relevant semantics the benchmark path supplies to the evaluation kernel.

C5 defines **mapping fidelity** as:

> The production mapping produces `ProductionEpisodeMapping` whose **evaluation-relevant observation fields** match the benchmark arm observations — the inputs the evaluation kernel actually consumes.

**This is not path equivalence.** Mapping fidelity is a **prerequisite** that proves both orchestration paths *can* supply identical evaluator inputs. Equivalence testing (E2-C5-01..03) begins only after Checkpoint 0 passes on the mapping-safe fixture set.

**Evaluation-relevant fields (must match for mapping-safe fixtures):**

| Field group | Fields |
|-------------|--------|
| Cold arm | `terminal_state`, `final_success_score`, `arm_scoring_status`, retrieval hit flags, runtime guard / block state (`run_block_reason` where applicable) |
| Warm arm | `terminal_state`, `final_success_score`, `arm_scoring_status`, `retrieval.chunks` (ids/sources), `warm_retrieval_hit`, runtime guard / block state |
| Expectations | `lift_threshold`, `lift_constraint`, `allow_binary_pass`, `include_in_mean_episodic_lift` |

**Excluded from comparison (metadata / transport):** timestamps, event IDs, telemetry IDs, subscriber envelopes, logging fields, `goal` / `run_id` / `completed_at_ms`, plan_snapshot structure not consumed by mapping, extra steps not mapped to retrieval chunks.

**Checkpoint 0 round-trip (mapping fidelity validation only):**

```text
Benchmark arm observations (cold, warm, expectations)
    → episodeFromBenchmarkArms()     [synthetic EpisodeCompleted — tests only, NOT production]
    → mapEpisodeToProductionObservations()
    → ProductionEpisodeMapping (evaluation observations)
    → compare evaluation-relevant fields to original benchmark arm observations
```

Rules:

1. `episodeFromBenchmarkArms(spec, cold, warm)` creates a **synthetic** `EpisodeCompleted` strictly for testing — it is not production functionality and must not ship as a runtime code path.
2. The comparison baseline is the **original benchmark evaluation observations** (arm observations + expectations), **not** the `EpisodeCompleted` object itself.
3. Only evaluation-relevant fields participate in the comparison (see table above).
4. Round-trip failure → **mapping analysis issue first** (fixture scope report). Do **not** modify the evaluator. Correct the mapper only if evaluation semantics are actually being lost.

**Checkpoint 0 gate:**

> Prove that the benchmark-to-production mapping preserves all evaluation-relevant semantics required by the E2 evaluation kernel. This validates that both orchestration paths can provide identical evaluator inputs before equivalence testing begins.

**Fixture policy:** Use **mapping-safe golden cases** first. Do **not** expand mapping complexity during C5. Fix mapping only if it violates evaluation semantics — not structural differences alone.

#### C5.4.1 — Checkpoint 0 vs Checkpoints 1–3 (scope boundary)

| Checkpoint | Proves | Is path equivalence? |
|------------|--------|---------------------|
| **0** | Mapper preserves evaluation-relevant observations (benchmark arms → production mapping) | **No** — prerequisite only |
| **1–3** | Benchmark orchestration vs production orchestration produce identical evaluation semantics under pinned config | **Yes** — the C5 proof |

#### C5.5 — Architecture under test

```text
Benchmark path:
  runScoredEvaluationLoop
      → EvaluationService
      → DiagnosticService

Production path:
  EpisodeCompleted
      → mapEpisodeToProductionObservations
      → EvaluationSubscriber
      → EvaluationService
      → DiagnosticService
```

Executive is out of unit-test equivalence scope. Manual validation uses ≥1 real goal run per deliverables.

#### C5.6 — Test-only config seam

```cpp
// evaluation_subscriber.h — testing only; production never sets this
void setEvaluationSubscriberEvalConfigForTests(std::optional<E2EvalConfig> config);
```

Unset → `integrationDefaults()` + `INTEGRATION` tier (production). Set → pinned config for C5 controlled equivalence only.

#### C5.7 — Comparison harness deliverables

| Deliverable | Requirement |
|-------------|-------------|
| Comparison harness | `e2_path_equivalence.h/.cpp` — artifact runners + normalized diff |
| Equivalence matrix | Per-case pass/fail on semantic fields |
| Mismatch dump | JSON per-case field diff on failure |
| Close-out | `docs/phases/PHASE_C_COMPLETE.md` |

#### C5.8 — Tests (preregistered — full set)

| ID | Asserts |
|----|---------|
| **E2-C5-01** | Semantic equivalence — per-case `evaluation_resolution`, scorable classification, `diagnosis_bucket` identical across paths under pinned config |
| **E2-C5-02** | Fingerprint stability — `fingerprint_hash` + semantic config pins match when computed post-normalization on same pinned STRICT config |
| **E2-C5-03** | Cross-path artifact consistency — per-case eval + summary outputs match (normalized); ignore timestamps/telemetry/logs |
| **E2-C5-04** | No hidden coupling — structural audit: Evaluation ↔ Executive ↔ Diagnostics ↔ Telemetry; no reverse dependencies |
| **E2-C5-05** | Full regression — E2-25–E2-28 + E2-C1–C4 green; `wiring_stage=B` fingerprint stable |

#### C5.9 — Execution lock checklist (required before implementation)

Proceed with C5 implementation only after confirming:

1. **Config semantics** — C5 compares evaluation semantics under pinned configs; not raw config object equality across paths
2. **Fixture scope** — mapping-safe golden cases only initially; no mapping expansion during C5
3. **Config seam** — `setEvaluationSubscriberEvalConfigForTests()` is test-only; must not affect production when unset
4. **Mapping rule** — Checkpoint 0 is mapping fidelity validation (prerequisite), not equivalence; round-trip mismatch = mapping analysis first
5. **Test registry** — E2-C5-01 through E2-C5-05 locked (this section)
6. **C4 hardening** — telemetry sink-only closure documented; E2-C4-03b subscriber telemetry block audit green

#### C5.10 — Implementation checkpoints

| Checkpoint | Work | Gate |
|------------|------|------|
| **0** | **Mapping fidelity validation** (prerequisite — not path equivalence): `episodeFromBenchmarkArms()` synthetic fixtures; round-trip benchmark arms → `EpisodeCompleted` → `mapEpisodeToProductionObservations()` → compare evaluation-relevant observations to original benchmark arms; fixture scope report | Prove benchmark-to-production mapping preserves all evaluation-relevant semantics required by the E2 evaluation kernel — both paths *can* supply identical evaluator inputs before equivalence testing begins |
| 1 | `e2_path_equivalence` harness + test config seam | Build green; production default unchanged |
| 2 | E2-C5-01, E2-C5-02, E2-C5-03 | Equivalence matrix green on mapping-safe fixtures |
| 3 | E2-C5-04, E2-C5-05, manual real-goal run, `PHASE_C_COMPLETE.md` | Phase C close-out |

#### Forbidden

- Modifying evaluation, diagnostic, or telemetry logic to make tests pass
- Claiming C5 proves live INTEGRATION-tier == STRICT-tier config identity
- Fixing mapping for structural differences that do not affect evaluation semantics
- Skipping Phase B fingerprint regression
- Synthetic-only validation as sole evidence (manual run still required)

#### Exit criteria

1. E2-C5-01–E2-C5-05 green on mapping-safe fixtures
2. Equivalence matrix recorded; mismatch dumps available on failure
3. `THOTH_E2_WIRING_STAGE=B` fingerprint stable
4. Structural audit — no cross-layer reverse dependencies
5. `PHASE_C_COMPLETE.md` written
6. **Pause before Phase D**

**Status:** 🔒 **Phase C locked** (2026-07-05). Path equivalence proven on mapping-safe golden fixtures; E2-C5-01–05 green; `wiring_stage=B` harness SUCCESS; `docs/phases/PHASE_C_COMPLETE.md` written. **Paused before Phase D.**

---

## Phase C rules (forbidden)

All forbidden work derives from the **passive-service invariant**. Explicit list:

| # | Forbidden | Consequence of |
|---|-----------|----------------|
| 1 | Change `evaluation_resolution` semantics | Passive service must not redefine interpretation |
| 2 | Redefine `evaluation_fingerprint_hash` derivation | [`baselines/fingerprint_lock.md`](baselines/fingerprint_lock.md) |
| 3 | Modify scoring (`evaluateEpisodicLearningCase` pass/fail rules) | Evaluator finished in Phase B |
| 4 | Change diagnosis buckets #1–#4 | B5 reproducibility taxonomy |
| 5 | Alter E2 Protocol v1.2 STRICT / INTEGRATION tier definitions | Locked 2026-07-01 |
| 6 | Emit `official_scoring: true` outside `wiring_stage=B` benchmark | Official Harness Invariant |
| 7 | Compare INTEGRATION to STRICT as benchmark improvement | Protocol violation |
| 8 | Couple evaluation enablement to goal success | Observe, don't influence |
| 9 | Evaluation modifying planning, retrieval, memory, or prompts | Observe, don't influence |
| 10 | Mix benchmark metrics with architectural telemetry | C4 classification |
| 11 | Dashboards, UI, visualization, reporting in Phase C | Infrastructure only — consumers later |

**Everything must consume the Phase B contract.**

---

## Regression gates (all checkpoints)

Every C checkpoint exit **requires**:

| Gate | Source |
|------|--------|
| Unit tests green | `thoth-unit-tests` including E2-25–E2-28 |
| STRICT benchmark regression | `THOTH_E2_WIRING_STAGE=B ./build/.../run_episodic_learning_benchmark` |
| E2-28 two-run equivalence | Scoped fields match Phase B v1 baseline |
| Fingerprint | `1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4` unless config pins legitimately change (semantic drift = fail) |
| Structural audit | `testE2B5ScoredLoopStructuralAudit` — no stage branching inside `runScoredEvaluationLoop()` |
| Passive invariant | No evaluation callback into execution verified per checkpoint |

---

## Phase C success definition

By the end of Phase C:

| Criterion | Meaning |
|-----------|---------|
| Evaluation is no longer benchmark-specific | Service callable from episode events and harness |
| Executive publishes episodes | C2 event publication proven; Executive unaware of evaluation internals |
| Diagnostics present evaluation | C3 ownership model enforced — presentation only |
| Benchmark remains reproducible | Phase B v1 fingerprint gate passes |
| **Path equivalence** | Same episode → same `evaluation_resolution` via benchmark and production paths |
| Benchmark metrics ≠ telemetry | C4 classification enforced — never mixed |
| No semantic drift from Phase B | `evaluation_resolution` and `e2_outcome` meaning unchanged |

**Phase C is complete when integration is proven, not when new evaluation features are added.**

---

## Out of scope (Phase C)

Explicitly deferred — **consumers**, not infrastructure:

| Item | When |
|------|------|
| Dashboards | Post–Phase C / GUI work |
| Visualization widgets | Post–Phase C |
| Reporting UI | Post–Phase C |
| `AddCollapsiblePane` sidebar panels | Separate UI task |

Phase C delivers JSONL events, service contracts, and validation records. Presentation layers consume them later.

---

## Naming disambiguation

Thoth `cursor_list.md` documents a separate **Cognate track** (C1–C7: planning, retrieval, reflection, CI latency, robustness). **E2 Phase C** (this document) is **distinct**:

| Name | Scope |
|------|-------|
| **E2 Phase C** (C1–C5 here) | E2 integration tier — evaluation service in architecture |
| **Cognate C1–C7** | Component quality harnesses — already largely complete |

Implementation tasks for E2 Phase C should be tracked under **§ E2 Phase C** in `cursor_list.md`, not under Cognate C1–C7.

---

## Implementation order

| Order | Checkpoint | Runtime semantic change? |
|-------|------------|-------------------------|
| 0 | This protocol reviewed and locked | No |
| 1 | C1 — Evaluation service boundary | Refactor only — semantics frozen |
| 2 | C2 — Episode publication | Additive — default OFF |
| 3 | C3 — Diagnostic layer (JSONL only) | Additive — no scoring change |
| 4 | C4 — Architectural telemetry | Additive — observability only |
| 5 | C5 — Production validation + path equivalence + close-out | No evaluator change |

**Time estimate (rough):** C1–C2 **4–6 hours**; C3 **4–8 hours**; C4 **4–6 hours**; C5 **2–4 hours**.

---

## Key files (expected touch)

| Area | Likely files |
|------|--------------|
| Evaluation service | `episodic_learning_eval.h` / `.cpp` (interface extraction) |
| Harness (thin caller) | `run_episodic_learning_benchmark.cpp` |
| Episode publication | `executive_controller.h` / `.cpp` (event emit only) |
| Config / flag | `config.h` or equivalent |
| Diagnostics | `diagnostic_service.h` / `.cpp`; JSONL `event_type`: `E2_EVAL_DIAGNOSTIC_CASE`, `E2_EVAL_DIAGNOSTIC_SUMMARY` |
| Telemetry | `pipeline_telemetry_service.h` / `.cpp`; JSONL `event_type`: `E2_EVAL_TELEMETRY_PIPELINE` |
| Tests | `tests/unit_tests.cpp` (C1–C5 test IDs preregistered below) |
| Docs | This file, `E2_PROTOCOL.md` appendix (if needed), `PHASE_C_COMPLETE.md` |

---

## Test IDs (preregistered — to implement with checkpoints)

| ID | Checkpoint | Asserts |
|----|------------|---------|
| E2-C1-01 | C1 | Service interface callable; benchmark output unchanged vs pre-extraction |
| E2-C1-02 | C1 | Harness has no duplicate evaluation logic outside service |
| E2-C1-03 | C1 | Service contains no benchmark-specific logic (`wiring_stage`, CLI parsing, JSONL paths, benchmark logging) |
| E2-C2-01 | C2 | Publication disabled → Executive behavior identical (flag OFF) |
| E2-C2-02 | C2 | Publication enabled → `EvaluationSubscriber` receives event via channel; Executive has no direct eval import |
| E2-C2-03 | C2 | Subscriber output → INTEGRATION envelope only; `official_scoring: false` |
| E2-C2-04 | C2 | Subscriber contains no execution logic (no planner, RAG, memory mutation, Executive state access) |
| E2-C3-01 | E2-C3 | Diagnostics do not call evaluation/scoring functions |
| E2-C3-02 | E2-C3 | Same evaluation input → identical diagnostics output |
| E2-C3-03 | E2-C3 | Diagnostics do not modify evaluation_resolution / fingerprint / scores |
| E2-C3-04 | E2-C3 | C2→C3 pipeline preserves E2-25–E2-28 + fingerprint stability |
| E2-C4-01 | E2-C4 | Telemetry JSONL emitted; `telemetry_tier: ARCHITECTURE`; excludes benchmark/diagnostic authority fields |
| E2-C4-02 | E2-C4 | Telemetry failure does not block subscriber or alter eval/diagnostic artifacts |
| E2-C4-03 | E2-C4 | Telemetry code does not call evaluation or diagnostic services |
| E2-C4-03b | E2-C4 | Subscriber telemetry block does not reference eval/diag semantic fields |
| E2-C4-04 | E2-C4 | Telemetry non-interference — eval/diagnostic/fingerprint unchanged |
| E2-C4-05 | E2-C4 | E2-25–E2-28 + E2-C1–C3 regression; fingerprint stable |
| E2-C5-01 | E2-C5 | Semantic equivalence under pinned config — resolution, scorable, diagnosis bucket |
| E2-C5-02 | E2-C5 | Fingerprint stability post-normalization on identical semantic pins |
| E2-C5-03 | E2-C5 | Cross-path artifact consistency — per-case eval + summary (normalized diff) |
| E2-C5-04 | E2-C5 | No hidden coupling — eval/exec/diag/telemetry structural audit |
| E2-C5-05 | E2-C5 | E2-25–E2-28 + E2-C1–C4 regression; `wiring_stage=B` fingerprint stable |

---

## References

| Document | Role |
|----------|------|
| [`E2_PROTOCOL.md`](E2_PROTOCOL.md) | Evaluation semantics, STRICT / INTEGRATION tiers |
| [`phases/PHASE_B_COMPLETE.md`](phases/PHASE_B_COMPLETE.md) | Phase B completion + layer roles |
| [`baselines/fingerprint_lock.md`](baselines/fingerprint_lock.md) | Fingerprint contract |
| [`baselines/BASELINE_PROVENANCE.md`](baselines/BASELINE_PROVENANCE.md) | Phase B baseline provenance |
| [`benchmark_results/phase_b_baseline_v1.md`](benchmark_results/phase_b_baseline_v1.md) | Golden baseline record |
| [`cursor_list.md`](cursor_list.md) | Active implementation backlog |

---

## System framing

Phase C is **wiring**, not **redesign**. The evaluation function proven in Phase B becomes passive infrastructure — subscribed to completed episodes, observable via diagnostics and telemetry, and validated by path equivalence. STRICT benchmark authority remains the scientific record; INTEGRATION and telemetry provide engineering visibility without contaminating measurement.

Keep it boring. Infrastructure first. Consumers later.

---

*Locked v1.1 — 2026-07-04. Implementation begins only when C1 is explicitly opened in `cursor_list.md`.*
