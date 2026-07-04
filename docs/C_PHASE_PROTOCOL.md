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
| **C1** | Evaluation service extraction | Medium |
| **C2** | Episode publication (Executive → service) | Medium |
| **C3** | Diagnostic layer (presentation only) | Large |
| **C4** | Architectural telemetry (separate from benchmark metrics) | Medium |
| **C5** | Production validation (path equivalence) | Small |

There is **no** “invent a better evaluator” checkpoint.

---

### C1 — Evaluation Service Boundary

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

### C2 — Episode Publication

**Question:** Can the Executive publish completed episodes without knowing how evaluation works?

#### Goal

The Executive **publishes** a completed-episode event to the evaluation service. It does **not** invoke evaluation directly.

```
Before:  Benchmark → evaluate()

After:   Executive → Episode Completed → Evaluation Service
         Benchmark → Evaluation Service          (unchanged authority path)
```

The Executive emits an observation payload (episode-complete). The service subscribes asynchronously. Executive has **zero dependency** on evaluation internals.

#### Deliverables

| Item | Requirement |
|------|-------------|
| **Episode event** | Structured `EpisodeCompleted` (or equivalent) emitted post-goal or post-step — contains `EpisodicLearningArmObservation`-compatible read-only artifacts |
| **Publication hook** | Executive publishes; does not call `evaluateEpisodicLearningCase` directly |
| **Feature flag** | Config or env gate (e.g. `enable_episodic_evaluation_service: false` default) — publication **OFF** unless explicitly enabled |
| **Tier routing** | STRICT path only via benchmark / explicit lab config; production publication defaults to **INTEGRATION** diagnostic tier or **no publication** |
| **Non-authoritative envelope** | Production service output emits `official_scoring: false`; never `wiring_stage=B` authority from ambient runs |

#### Forbidden

- Executive calling evaluation logic directly (must publish event only)
- Auto-enable publication in default production config
- Emit `official_scoring: true` outside benchmark `wiring_stage=B`
- Alter Executive state machine transitions, planner calls, or retrieval dispatch
- Require evaluation for goal completion
- Evaluation service calling back into Executive

#### Exit criteria

1. Executive publishes episode events when flag enabled; does not import evaluation semantics
2. Executive behavior unchanged when flag disabled (regression: existing unit tests + headless loop)
3. Benchmark `wiring_stage=B` path unchanged and authoritative
4. Production service output produces INTEGRATION-tier envelope or no output — never authoritative `e2_outcome`
5. **Pause before C3**

---

### C3 — Diagnostic Layer

**Question:** Can diagnostics present evaluation output without becoming a second evaluator?

#### Data ownership

| Layer | Owns |
|-------|------|
| **Execution** | State — plans, memory, retrieval results, goal outcomes |
| **Evaluation** | Interpretation — `evaluation_resolution`, lift, pass/fail, fingerprint inputs |
| **Diagnostics** | Presentation — timelines, failure explanations, resolution summaries |

Diagnostics **present** evaluation results. They do **not** compute alternate scores, resolutions, or outcomes. This ownership model prevents later checkpoints from letting diagnostics drift into evaluator territory.

#### Goal

Establish strict layering:

```
Execution        ← owns state
    ↓
Evaluation       ← owns interpretation (Phase B contract, frozen)
    ↓
Diagnostics      ← owns presentation only
```

#### Deliverables

| Item | Requirement |
|------|-------------|
| **Diagnostic events** | Structured events emitted **after** evaluation (e.g. `E2_DIAGNOSTIC_RESOLUTION_SUMMARY`, `E2_DIAGNOSTIC_FAILURE_EXPLANATION`) |
| **Timeline** | Ordered diagnostic trace linking execution steps → evaluation input → resolution |
| **Failure explanations** | Human-readable mapping from `run_block_reason`, `scoring_block_reason`, arm status → diagnostic narrative |
| **Resolution summaries** | Re-export `evaluation_resolution` + context — never re-derive resolution in diagnostic layer |
| **INTEGRATION harness** | `run_episodic_learning_benchmark --tier integration` or equivalent diagnostic path per E2-06 |

#### Forbidden

- Diagnostic layer emitting `e2_outcome` or `evaluation_resolution` as a **second authority**
- Diagnostics that change pass/fail or lift calculations
- Comparing INTEGRATION diagnostics to STRICT baseline as “improvement”
- Dashboards, GUI panels, visualization widgets, or reporting UI (out of scope — consumers built later)

#### Exit criteria

1. Diagnostic events flow from evaluation output only; ownership boundaries enforced in schema
2. E2-06 (INTEGRATION diagnostic run) green — `official_scoring: false`, no `e2_outcome`
3. STRICT benchmark regression unchanged (Phase B fingerprint stable)
4. Diagnostic JSONL schema documented; no overlap with authoritative summary fields
5. **Pause before C4**

---

### C4 — Architectural Telemetry

**Question:** Can we observe architectural behavior without conflating it with benchmark metrics?

#### Metric classification (must never mix)

**Benchmark metrics** — authoritative measurement; Phase B contract:

| Metric | Authority |
|--------|-----------|
| `success` / `passes` | Evaluation |
| `lift` / `mean_episodic_lift` | Evaluation |
| `evaluation_resolution` | Evaluation (canonical) |
| `evaluation_fingerprint_hash` | Evaluation (derived) |
| `e2_outcome` | Export (derived from resolution) |

**Architectural telemetry** — engineering observability; non-authoritative:

| Signal | Purpose |
|--------|---------|
| Planning latency | Performance observability |
| Retrieval latency | Performance observability |
| Reflection latency | Performance observability |
| Memory hits | Reuse observability |
| Cache reuse | Reuse observability |
| Token counts | Cost observability |
| Evaluation service latency | Infrastructure overhead |

**Rule:** Benchmark metrics and architectural telemetry MUST NOT share a schema namespace, dashboard panel, or promotion trigger. Graphing latency beside `mean_episodic_lift` and calling them the same kind of metric is a protocol violation.

#### Goal

Capture architecture signals as telemetry channels with `telemetry_tier: "ARCHITECTURE"` — distinct from `scoring_tier: "STRICT"`.

#### Deliverables

| Item | Requirement |
|------|-------------|
| **Telemetry schema** | Versioned event types; explicit `telemetry_tier` field; no benchmark metric fields |
| **Non-blocking emission** | Telemetry failure must not fail goal execution |
| **Attribution** | `run_id`, `env_hash`, goal id — aligned with existing `BenchmarkAttribution` patterns |
| **Opt-in** | Same feature flag family as C2 — default OFF |

#### Forbidden

- Treating telemetry aggregates as official E2 evidence
- Mixing architecture telemetry into `evaluation_fingerprint` canonical JSON
- Blocking Executive on telemetry flush
- Combining benchmark metrics and telemetry in a single “score” or rollup field

#### Exit criteria

1. Telemetry events emitted on flagged production runs; schema segregated from benchmark fields
2. Telemetry schema documented in this protocol or `E2_PROTOCOL.md` appendix
3. STRICT benchmark unaffected (fingerprint + resolution stable)
4. No telemetry field redefines diagnosis buckets #1–#4
5. **Pause before C5**

---

### C5 — Production Validation

**Question:** Does integration produce the same evaluation for the same episode — regardless of path?

#### Goal

Run the **real agent** on real goals. Prove **path equivalence**: the production integration path and the benchmark path yield identical evaluation for identical completed episodes. This is the **production acceptance gate** for Phase C.

#### Path equivalence (primary acceptance criterion)

For any completed episode *E* with identical observation payload and `E2EvalConfig`:

```
Episode E  →  Benchmark path  →  evaluation_resolution  =  R

Episode E  →  Production service path  →  evaluation_resolution  =  R
```

Same episode. Same result. Different integration path.

“Zero semantic drift” is necessary but insufficient — path equivalence is the **exact** test.

#### Additional verify

| Check | Requirement |
|-------|-------------|
| STRICT fingerprint stable | Phase B `wiring_stage=B` regression still passes E2-28 |
| Diagnostics correct | Diagnostic narratives align with evaluation output (presentation only) |
| Agent behavior | Goal outcomes unchanged vs pre–Phase C when publication disabled |
| Passive invariant | No evaluation influence on execution observed in validation run |

#### Deliverables

| Item | Requirement |
|------|-------------|
| **Path equivalence record** | Documented test: ≥1 real episode evaluated via both paths; resolutions match |
| **Validation log** | `docs/baselines/` or `logs/` record of production validation run(s) |
| **Phase C completion doc** | `docs/phases/PHASE_C_COMPLETE.md` with integration summary |
| **Regression matrix** | STRICT benchmark + disabled-publication production run side-by-side |

#### Forbidden

- Claiming production INTEGRATION runs as benchmark baseline
- Skipping Phase B fingerprint regression
- Synthetic-only validation as sole evidence for path equivalence
- Accepting path mismatch with “close enough” tolerance on `evaluation_resolution`

#### Exit criteria

1. Path equivalence proven: same episode → same `evaluation_resolution` via benchmark and production service paths
2. Real-goal production run completed with diagnostics (INTEGRATION)
3. Phase B authoritative benchmark re-run passes (fingerprint `1ce31c6a…` or successor only if **config pins** change — resolution semantics must not)
4. `PHASE_C_COMPLETE.md` written
5. **Pause before Phase D**

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
| Diagnostics | New `e2_diagnostic_*` or extend `DecisionTraceLogger` |
| Telemetry | `decision_trace.jsonl` schema extension |
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
| E2-C2-02 | C2 | Publication enabled → service receives episode event; Executive has no direct eval import |
| E2-C2-03 | C2 | Production output → INTEGRATION envelope only; `official_scoring: false` |
| E2-C3-01 | C3 | Diagnostic events derived from evaluation; no second `evaluation_resolution` |
| E2-C3-02 | C3 | E2-06 INTEGRATION harness green |
| E2-C4-01 | C4 | Telemetry events emitted; schema excludes benchmark metric fields |
| E2-C4-02 | C4 | Executive not blocked on telemetry failure |
| E2-C5-01 | C5 | Path equivalence: same episode → same `evaluation_resolution` (benchmark vs production) |
| E2-C5-02 | C5 | Real-goal run + STRICT regression matrix PASS |

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
