# Phase D — Evolution Tier Protocol

**Protocol version:** D v1.0  
**Status:** 🔒 **D0 locked** — design complete; paused before D1  
**Supersedes:** None (first Phase D protocol)  
**Depends on:** [`C_PHASE_PROTOCOL.md`](C_PHASE_PROTOCOL.md) v1.1 (Phase C locked 2026-07-05), [`E2_PROTOCOL.md`](E2_PROTOCOL.md) v1.2, Phase B v1 baseline ([`phases/PHASE_B_COMPLETE.md`](phases/PHASE_B_COMPLETE.md))  
**Checkpoint tracking:** `cursor_list.md` § **D.0.0**

> **Scope:** Phase D is **evolution**, not reinvention. The evaluator is finished and integrated. Phase D proves the architecture can grow — more subscribers, replay, metrics, live diagnostic paths, presentation — without changing what Phase B and Phase C established as authoritative.

---

## Phase narrative

Each E2 phase answers one question in sequence:

| Phase | Question | Scientific role |
|-------|----------|-----------------|
| **A** | Can **execution** be trusted? | Build deterministic kernel |
| **B** | Can **measurement** be trusted? | Authorize scoring |
| **C** | Can trusted measurement become **architecture**? | Prove architecture doesn't affect truth |
| **D** | Can architecture **evolve** without losing trust? | Prove evolution doesn't affect truth |
| **E** | Can we **defend the results** scientifically? | External claims and publication |

Phase C proved passive integration. Phase D proves **growth** preserves trust. Phase E is out of scope here.

---

## D0 — Evolution boundary (informational — not coded)

Not an implementation checkpoint. Defines what may evolve and what is frozen forever.

Phase B established the **authoritative evaluation contract**. Phase C wired it as passive infrastructure. Phase D **extends consumers and operational modes** around that contract — it does not modify it.

| World | Role in Phase D |
|-------|-----------------|
| **Phase B contract** | Immutable dependency — consumed, not modified |
| **Phase C integration** | Immutable passive spine — extended, not redesigned |
| **Evolution layer** | New subscribers, replay, metrics/trace, live INTEGRATION connection, presentation reads |

**Answer for future contributors:** “Can I tweak evaluation while adding subscribers?” → **No. That is a protocol change (E2 v1.3+), not Phase D work.**

Any change requiring modification of Phase B semantics (`evaluation_resolution`, `e2_outcome`, fingerprint derivation, diagnosis buckets #1–#4) constitutes a **new protocol version** and is **out of scope for Phase D**.

---

## Three architectural modes (established at D0)

Phase D operates across three modes. D4 **connects** the already-defined INTEGRATION mode to the production path — it does not introduce INTEGRATION.

| Mode | Role | Authority |
|------|------|-----------|
| **STRICT** | Authoritative scoring | `wiring_stage=B` only; `official_scoring: true`; Phase B fingerprint gate |
| **INTEGRATION** | Diagnostic evaluation envelope | `official_scoring: false`; no `e2_outcome` on diagnostic authority path; E2-06 rules |
| **PRODUCTION** | Operational execution | Executive planning, retrieval, memory, goals — evaluation observes, never decides |

**Dependency direction:** PRODUCTION publishes episodes → subscribers observe → STRICT and INTEGRATION envelopes are **downstream interpretations**, never upstream controllers.

---

## Phase D Constitutional Rule

> **Every new capability added during Phase D must satisfy: Observe, Record, Replay, Present — Never Decide.**

| Allowed | Forbidden |
|---------|-----------|
| **Observe** — read immutable execution artifacts | Influence planning |
| **Record** — persist observational JSONL / metrics | Influence retrieval |
| **Replay** — re-observe sealed or published episodes | Influence memory |
| **Present** — render diagnostics to humans | Influence evaluation semantics |
| | Influence benchmark outcomes |

**If a component can influence planning, retrieval, memory, evaluation, or benchmark outcomes, it does not belong in Phase D.**

This rule is the Phase D equivalent of Phase B's deterministic kernel rules and Phase C's passive evaluation invariant. It governs every checkpoint (D1–D5).

---

## Passive Consumer Law

Any Phase D subscriber must satisfy **all five** conditions:

1. **Consumes immutable events** — read-only on `EpisodeCompleted` (or derived sealed replay input); no mutation of the published snapshot.
2. **Cannot modify publisher state** — no writes back to Executive, planner, RAG, or memory from subscriber code paths.
3. **Cannot influence execution ordering** — subscribers run outside the execution critical path; delivery order does not affect plan steps.
4. **Cannot become required for successful execution** — goal completion, plan success, and benchmark runs succeed with zero subscribers registered.
5. **Can be removed without changing benchmark results** — unregister all Phase D subscribers; `wiring_stage=B` output and Phase B fingerprint unchanged.

If every future subscriber satisfies these five properties, Phase D remains **mathematically passive**.

---

## GUI consequence (architectural law — not a checkpoint)

> **GUI is not part of the evaluation architecture. GUI is merely another subscriber.**

| Property | Requirement |
|----------|-------------|
| Dependency direction | **One-way:** benchmark/eval artifacts → GUI display |
| GUI removal | Benchmark outputs remain identical |
| Artifact removal | GUI has nothing authoritative to display |
| Scoring in UI | Forbidden — GUI reads JSONL; never produces `evaluation_resolution` or `e2_outcome` |

Presentation layers (dashboards, `AddCollapsiblePane` panels per `architectural_facts.md` §8) are **consumers**, not infrastructure. They may land during or after D1–D5 but are governed by the Passive Consumer Law and Constitutional Rule — not a separate scoring checkpoint.

---

## Dependency flow (post–Phase C stack)

```
Execution (PRODUCTION)
    ↓ publishes
EpisodeCompleted (immutable)
    ↓ delivers
Subscribers (Phase D growth)
    ├── EvaluationSubscriber (Phase C — frozen)
    ├── ReplaySubscriber (D2)
    ├── MetricsSubscriber (D3)
    ├── TraceSubscriber (D3)
    └── GUI / presentation (consumer — not eval architecture)
    ↓
Evaluation → Diagnostics → Telemetry (Phase C spine — unchanged semantics)
```

### Dependency invariant (carried forward from Phase C)

> **Dependencies flow downward only.** Lower layers may consume higher-layer outputs; higher layers shall not depend on lower layers.

`EvaluationService` remains **stateless**. Any replay buffers, metric aggregates, or trace correlation state live in subscriber-owned storage — never inside the evaluation kernel.

---

## One sentence

Phase D grows the integrated architecture with passive consumers and operational modes while proving Phase B evaluation semantics and Phase C path equivalence remain unchanged.

---

## Theme: Evolution Tier

### Goal

Prove the architecture can **evolve** — additional subscribers, replay, longitudinal observability, live INTEGRATION diagnostic connection — without semantic drift from Phase B or structural regression from Phase C.

### What Phase D is / is not

| Phase D **is** | Phase D **is not** |
|----------------|-------------------|
| Multi-subscriber fan-out on `EpisodeCompleted` | Redesign of `resolveEvaluation()` |
| Replay (changes **time** — re-observation) | A second scoring authority |
| Metrics/trace (changes **observation** — measure, don't interpret) | Eval service statefulness |
| Connect INTEGRATION mode to production path | STRICT re-baseline or E2 v1.3 |
| Trust re-proof at close-out (D5) | Phase E scientific defense |

---

## Checkpoint sequence

Implement in order. **Pause between every checkpoint** — build green → tests green → report → explicit go-ahead before next step.

---

### D1 — Event channel maturity (fan-out without coupling)

**Question:** Can multiple subscribers consume the same immutable `EpisodeCompleted` without the Executive knowing or caring?

**Scope**

- Formalize `IEpisodeEventChannel` subscriber registry (registration, delivery order policy, failure containment)
- Prove Executive unchanged: no subscriber count, no consumer identity, fire-and-forget publication

#### D1 invisibility invariant

> **Subscriber count must be invisible to the Executive.**

The Executive publishes exactly **one** immutable `EpisodeCompleted` event. Whether **zero**, **one**, or **fifty** subscribers exist, the Executive performs **identical work**.

This is stronger than “Executive unchanged” — it is a provable isolation property.

#### Forbidden

- Executive branching on subscriber count or consumer type
- Synchronous subscriber completion as a publication gate
- Channel logic that imports evaluation, diagnostic, or telemetry semantics

#### Exit criteria

1. Multi-subscriber delivery proven; failures isolated (logged only)
2. Throwing subscriber does not alter Executive goal completion or terminal state (E2-D1-02 — **mandatory Executive-path run**, not channel-only)
3. Executive terminal outcome identical with 0 vs N subscribers — not merely identical emitted payload (E2-D1-03)
4. C5 path equivalence unchanged on golden fixtures
5. E2-D1-01–E2-D1-03 green
6. **Pause before D2**

---

### D2 — Replay subscriber (deterministic re-observation)

**Question:** Can completed episodes be replayed for debugging without mutating execution or evaluation state?

**Scope**

- `ReplaySubscriber` — read-only consumer; **replay changes time** (re-observation of captured `EpisodeCompleted` snapshots)
- Replay produces diagnostic or observational artifacts only; no `official_scoring: true`
- Replay state is subscriber-owned; `EvaluationService` remains stateless
- **Live capture** is mandatory; **JSONL persistence** is optional and purely observational

**Why separate from D3:** Replay and metrics are different architectural proofs. Replay changes **time**; metrics change **observation**.

#### Replay path (not channel republication)

Replay **never** republishes `EpisodeCompleted` into `IEpisodeEventChannel`. Replay is internal to `ReplaySubscriber`:

```
Executive
    │ publish EpisodeCompleted (live, fire-and-forget)
    ▼
Event Channel
    ├── EvaluationSubscriber
    └── ReplaySubscriber
            │ live: append immutable copy (append-only FIFO)
            │ replay: ReplaySink / replay callback only
            ▼
        (never publish() back into Event Channel)
```

Do **not** describe replay as “re-emitting” into the channel — that invites replay recursion.

#### Forbidden

- **`ReplaySubscriber` must never call `publish()` on `IEpisodeEventChannel`**
- Replay mutating `EpisodeCompleted` or publisher state
- Replay as a prerequisite for benchmark or goal success
- Replay path emitting STRICT authority fields (`official_scoring: true`, `e2_outcome`, `evaluation_resolution`)
- Replay calling `EvaluationService` for scoring
- Shared/static replay state visible to Executive or eval kernel
- Blocking channel delivery on replay buffer flush failure
- `replayAll()` or keyed-by-`plan_id` indexing in D2 (defer to later)

#### Exit criteria

1. Replay idempotent on captured input; Passive Consumer Law satisfied
2. **Replay idempotence invariant:** replaying an episode **must not increase** `ReplaySubscriber`'s stored episode count
3. Phase B fingerprint unchanged on consecutive `wiring_stage=B` runs
4. E2-D2-01–E2-D2-02 green (coexistence with `EvaluationSubscriber` asserted inside E2-D2-01 — no E2-D2-03 replay checkpoint)
5. Production wiring registered behind `enable_episode_replay_subscriber=false` (default OFF)
6. **D2-03 / FLAKE-UT-02 resolved** — reliability close-out complete (see below); separate from replay architectural proof
7. **Pause before D3**

#### D2-03 — FLAKE-UT-02 reliability close-out (complete — 2026-07-07)

**Not** an E2-D2 replay test checkpoint. Separate reliability investigation triggered by intermittent `nlohmann::json` string invariant assert during G2 (`ctest` @ ~386s). GRAG benchmark stdout was a **crash marker** only.

| Item | Record |
|------|--------|
| **Root cause** | `SQLiteMemoryRepository::consolidateSessionBatch()` — `SQLITE_STATIC` bound to temporary `metadata.dump().c_str()`; heap-use-after-free confirmed by ASan in `testMemoryPruning` (~37s) |
| **Classification** | Delayed heap corruption (object lifetime); **not** `nlohmann::json` ownership; GRAG path ruled out |
| **Fix** | Named `metadataJson` string preserving buffer through `sqlite3_step()`; no behavioral or persistence semantic change |
| **Commits** | `basic_agent` **`27192fd`**; Thoth **`2993fb3`** |
| **Verification** | ASan full suite clean; G2 ×3 consecutive pass (excluding resource-contention timeout); `THOTH_FLAKE_UT02_STRESS=100` green |
| **Instrumentation** | Removed from production `main`; retained on `wip/flake-ut02-instrumentation` branches only |

Full execution record: `docs/cursor_list.md` § **D.2.1**.

---

### D3 — Metrics & trace subscribers (longitudinal observability)

**Question:** Can architecture-level metrics and traces be collected without becoming decision inputs?

**Proof obligation:** D3 proves that operational observability (metrics and trace) can be added to the architecture **without introducing any reverse dependency or decision influence** on the cognitive pipeline.

**Scope**

- `MetricsSubscriber` — counters, durations, queue depths, rates, histograms (subscriber-owned aggregates)
- `TraceSubscriber` — correlation, chronology, causal links, run timeline (no statistics/scoring)
- Same philosophy as C4: **measure, don't interpret**
- **Implementation plan:** `docs/cursor_list.md` § **D.3.0** (v1 locked)

#### Subscriber ownership (no overlap)

| Subscriber | Owns | Does NOT own |
|------------|------|--------------|
| `MetricsSubscriber` | Counters, durations, queue depths, rates, histograms | Event ordering, evaluation semantics, timelines |
| `TraceSubscriber` | Correlation, chronology, causal links, run timeline | Statistics, scoring, aggregation |

#### Event contract (immutable fan-out)

`EpisodeCompleted` delivers to Evaluation, Replay, Metrics, and Trace subscribers.

**Ordering invariant:** Subscriber ordering is **not architecturally significant** — any subscriber may run before or after another without changing system behavior; ordering affects log timestamps only.

**Immutable payload:** Subscribers receive a **`const` event view**. No subscriber may modify `EpisodeCompleted` or any shared payload visible to sibling subscribers. Immutability is **contractual**.

New channel event types are **out of scope** for D3.

#### Failure isolation (E2-D3-02)

Per subscriber: `try { deliver } catch { log; continue }`.

| Invariant | Requirement |
|-----------|-------------|
| Siblings | One failure must not suppress another subscriber |
| Executive | One failure must not suppress the Executive |
| Publication | `publish()` completes; fan-out failures do not roll back publication |
| Delivery | Each non-throwing subscriber receives **exactly one** delivery per publish — no duplicates, no skips |

**Terminal outcome equality (locked):** Compare Executive state, plan status, goal/plan success-failure, and outcome-carrying fields only. **Exclude** metrics emitted, trace records, structured log contents, and subscriber-local state.

**Ordering (mandatory in E2-D3-02):** At least two registration permutations must yield identical delivery and terminal outcome.

Structured logs are evidence of isolation, not the property under test.

#### Trace records (D3 v1)

- **Mandatory:** `EpisodeCompleted` via channel  
- **Read-only correlation:** `decision_trace.jsonl`, E2/telemetry JSONL by ID join  
- **Optional:** `replay_observed` via replay sink seam — not channel republication  
- **Not in D3:** `EpisodeStarted`, `ReplayStarted`, live DecisionTrace subscription  

Required IDs: `run_id`, `goal_id`, `plan_id`, `episode_id`; optional `replay_id`, `parent_run_id`.

#### Metrics timestamp sources (locked)

- **Episode-scoped:** from `EpisodeCompleted` only  
- **Pipeline-scoped:** from C4 telemetry envelopes only — no eval/diag semantics  

#### D3 measurement boundary

`MetricsSubscriber` may record **raw** values from allowed inputs but shall **never** derive classifications, pass/fail state, lift interpretation, benchmark authority, or planner decisions from those values.

**Interpretation (forbidden):** pass/fail; success/warning/failure classification; evaluation scoring; benchmark comparison; policy decisions; recommendation generation. Subscribers may **only record facts**.

**Authority boundary:** Metrics and trace outputs are observational artifacts only — never inputs to evaluation or Executive decision-making.

**Opaque score rule:** `final_success_score` may be stored only as `observed_final_success_score` — an uninterpreted float observation. No thresholds, bucketing, or success semantics may be derived from it.

**Exclusive ownership:** `MetricsSubscriber` is the sole owner of metric aggregation semantics; `TraceSubscriber` is the sole owner of chronological tracing semantics — no overlap.

**Frozen aggregation operations (MetricsSubscriber v1):** `counter_increment`, `counter_add`, `gauge_set`, `histogram_observe`, `duration_observe_ms` only. No rolling-window classification, success/failure rates, or composite scores.

Trace subscribers follow the same interpret boundary for timeline labels.

**Publication-mechanism rule:** D3 subscribers own no publication mechanism (no channel handles, no `publish`, no publisher abstractions).

#### Configuration (defaults OFF)

| Flag | Default |
|------|---------|
| `enable_metrics_subscriber` | `false` |
| `enable_trace_subscriber` | `false` |

**D3 Step 5 (plugin/config integration proof):** After Steps 2–4, prove production `BasicAgentPlugin` wiring, config JSON round-trip, independent flags, subscriber identity on the production channel, and default-OFF safety — not subscriber semantics (see `cursor_list.md` § D.3.0 Step 5). Exit criterion: the production integration path is proven to be the only registration path for observability subscribers.

**D3 Step 6 (umbrella proof-suite gate):** `THOTH_E2_D3=1` executes the complete D3 proof suite (Steps 1–5), then backward-compat gates (`THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1`) and G2 `ctest` confirm D3 close-out. Each step establishes a different architectural invariant; the umbrella gate proves they hold together.

**D4 (live INTEGRATION connection):** Full protocol § **D4** below; detailed step plan in `cursor_list.md` § **D.4.0**. **D4 complete** (Steps 1–5 ✅); paused before **D5** (evolution trust proof).

#### Storage (subscriber-owned)

| Subscriber | Hot | Durable (optional) |
|------------|-----|-------------------|
| Metrics | In-memory aggregates by `run_id` | `logs/e2_metrics.jsonl` (`metrics_schema_version: "1.0"`) |
| Trace | Ring buffer (recent N) | `logs/e2_trace.jsonl` |

**Metrics JSONL v1.0 (frozen):** one object per line; `record_type` ∈ `episode_observation` \| `pipeline_observation`; `observations` holds counter/gauge/histogram samples only; pipeline records carry C4 telemetry fields in `pipeline`. Forbidden on metrics path: `official_scoring`, `e2_outcome`, `evaluation_resolution`, lift, pass/fail authority fields.

#### D3 Step 2 — E2-D3-01 refinements (approved)

Step 2 implements metrics sink-only behavior per `cursor_list.md` § D.3.0 Step 2 plan:

- Opaque `final_success_score` observation only  
- Frozen aggregation operations (see measurement boundary)  
- Frozen Metrics JSONL schema v1.0  
- Structural audit: `MetricsSubscriber` independent of evaluation implementation (no eval service headers, symbols, or calls)  
- Backward compatibility: `enable_metrics_subscriber=false` → identical runtime behavior and architectural fingerprints; `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` unchanged

**Implementation discipline:** Build only what E2-D3-01 requires. Defer helpers, abstractions, and optimizations not needed for the Step 2 proof. Do not add future-facing infrastructure because later checkpoints will need it — each step introduces only what its proof requires.

**Verification scope:** Run only checkpoint-targeted env gates (`THOTH_E2_D3_01=1` plus backward-compat `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1`). Do not run the full unit-test suite during Step 2 — full regression is Step 6.

#### Non-goals

D3 does not analyze performance, optimize execution, change scheduling, influence planner/Executive, alter evaluation/replay/benchmark semantics, or require subscribers for goal success.

#### Forbidden

- Merging metrics + trace into one subscriber
- Metrics subscriber calling private eval/scoring helpers
- Trace correlation feeding back into Executive or planner
- Blocking upstream layers on metrics/trace flush failure
- Mutating `EpisodeCompleted` or shared payloads visible to siblings
- Depending on subscriber registration order for correctness

#### Exit criteria

1. Proof obligation satisfied; ownership + immutability invariants enforced
2. Sink-only audits pass (E2-C4-03b discipline)
3. Subscriber failures non-blocking (E2-D3-02)
4. E2-D3-01–E2-D3-03 green
5. Both config flags registered default OFF
6. **Pause before D4**

---

### D4 — Live INTEGRATION connection (operational mode wiring)

**Protocol status:** 🔒 **v1 locked** (2026-07-07) — Step 1 ✅; Steps 2–5 paused per AGENTS.md gate  
**Checkpoint plan:** `cursor_list.md` § **D.4.0**  
**Depends on:** E2 Protocol v1.2 (E2-06, INTEGRATION tier), Phase C locked, D1–D3 complete, D4 Step 1 green

#### Objective

Prove that the **existing** INTEGRATION diagnostic mode is correctly connected to the production wiring path under `integrationDefaults()`, and that this connection **cannot emit scoring authority artifacts**.

D4 is a **containment proof**, not a feature addition:

- **D3** proved observers can **observe** without becoming authority.
- **D4** proves observers can observe the **real production wiring path** without **becoming** or **leaking** authority.

D4 does **not** introduce INTEGRATION. D0 and Phase C defined it; D4 **connects and contains** it on the operational path.

> **One sentence:** D4 connects INTEGRATION to production; it does not introduce INTEGRATION. Prove valid E2-06 envelopes on the real subscriber/plugin path while STRICT remains the sole scoring authority.

#### Scope

**Live plugin path (canonical):** Production initialization through `BasicAgentPlugin` using normal registration and `integrationDefaults()`, executed in a test harness rather than deployed runtime. (Synonym: *live production path* — same meaning.)

**Containment contract (all D4 containment tests):** `official_scoring == false`; no `e2_outcome`; no STRICT authority metadata; no benchmark authority fields; diagnostic INTEGRATION envelope only.

**Core invariant (Step 2):** The production path can emit diagnostic INTEGRATION artifacts **without acquiring scoring authority**. Step 2 does not compare, rank, or promote those artifacts relative to STRICT.

**In scope**

| Area | Obligation |
|------|------------|
| Production wiring | Real subscriber/plugin registration under `enable_episodic_evaluation_publication` |
| Configuration selection | Production subscriber operates under `integrationDefaults()` when test config seam unset |
| E2-06 envelope | Valid non-scoring INTEGRATION diagnostic artifacts on the live production path |
| Authority containment | No production-path emission of STRICT/scoring authority fields |
| STRICT isolation | Benchmark authority (`wiring_stage=B`, Phase B fingerprint) unchanged by D4 wiring |
| Structural confirmation | Production seam verified before behavioral proof (D4 Step 1) |

**“Live production path” (locked definition)**

**Live production path** means the production wiring path exercised through real subscriber/plugin registration under `integrationDefaults()` — typically via integration tests that construct the production plugin with publication enabled.

It does **not** mean deployed service traffic, external user runtime, or production-ops validation.

**Out of scope**

| Item | Rationale |
|------|-----------|
| Deployed / external-user “live ops” | Architectural proof, not deployment validation |
| INTEGRATION ≡ STRICT equivalence | C5 addressed equivalence under pinned config; D4 does not reopen |
| INTEGRATION-vs-STRICT promotion or lift claims | Protocol violation |
| Changes to `resolveEvaluation()` or Phase B export | Authority frozen |
| Protocol tier redefinition | E2 v1.3+ and separate approval |
| D5 trust re-proof | Separate checkpoint |

#### Preconditions

1. D1–D3 complete — channel, replay, observability proof suites green  
2. Phase C locked — `EvaluationSubscriber`, event publication, diagnostic/telemetry tiers operational  
3. G1/G2 green — unit-test CTest budget satisfied  
4. **D4 Step 1 complete** — production wiring seam structurally confirmed (`THOTH_E2_D4_STEP1=1`)  
5. E2 Protocol v1.2 locked — INTEGRATION tier and E2-06 rules authoritative  
6. This D4 protocol section locked before Step 2+ implementation  

#### Required invariants

**Constitutional Rule (Phase D):** Every D4 capability must satisfy **Observe, Record, Replay, Present — Never Decide.**

**Passive Consumer Law:** All D4-touching subscribers satisfy all five conditions (immutable consumption; no publisher mutation; no execution-order influence; not required for success; removable without benchmark change).

**D4-specific invariants**

| ID | Invariant |
|----|-----------|
| **D4-I1** | Production subscriber config selection uses `integrationDefaults()` when test seam unset |
| **D4-I2** | `enable_episodic_evaluation_publication` defaults OFF; registration is flag-gated only |
| **D4-I3** | Executive publication gated on config flag; Executive does not branch on eval tier or scoring state |
| **D4-I4** | `registerEvaluationSubscriber` invoked only from production integration path (plugin + subscriber definition) |
| **D4-I5** | Test config seam does not affect production when unset |
| **D4-I6** | No production subscriber initialization path may implicitly select `strictDefaults()` under integration production configuration |
| **D4-I7** | STRICT benchmark path and Phase B fingerprint remain stable regardless of D4 wiring state |

**Note on D4-I6:** Tests may legitimately use `strictDefaults()`. The prohibition is **authority confusion** on the production integration path, not the existence of the function.

#### Authority boundaries

**INTEGRATION may:** emit diagnostic envelopes under E2-06; record observational fields; use organic warm tier, cross-session reads, and heuristics per `integrationDefaults()`; coexist with D2/D3 subscribers on the same channel.

**INTEGRATION must not:** emit `official_scoring: true`; emit `e2_outcome` on the diagnostic authority path; influence planner, Executive, retrieval, memory, or benchmark outcomes; become a prerequisite for goal or benchmark success; be cited as benchmark evidence without the label **“non-scoring diagnostic mode”**.

**STRICT authority (untouched):** `wiring_stage=B` harness remains sole official scoring path; Phase B fingerprint stable; C5 equivalence under pinned config remains valid; D4 does **not** claim INTEGRATION object equality with STRICT.

**Containment vs presence (proof ladder):**

| Class | Proves |
|-------|--------|
| **Presence** | `scoring_tier: INTEGRATION`; expected diagnostic metadata; E2-06 required fields present |
| **Containment (absence)** | Containment contract (§ Scope) — every containment test checks the same contract |

Step 2 tests **separate** presence proofs from containment proofs (distinct test functions).

**Step 2 implementation discipline:** Tests only first. **Production changes are permitted only to correct verified production wiring defects discovered by the behavioral proof** — not to reshape subscriber semantics for test convenience.

**E2-D4-01 preregistered tests:** `testE2D4_01LivePluginPathPresence`, `testE2D4_01LivePluginPathJsonlPresence` (presence); `testE2D4_01LivePluginPathContainment` (containment); `testE2D4_01IntegrationDefaultsBehavioralNegative` (negative — seam unset → `integrationDefaults()` only; no STRICT config injected).

**Step 3 — STRICT authority preservation (E2-D4-02):** See `cursor_list.md` § D.4.0 Step 3.

**Theorem-like invariant:** Observational infrastructure shall be observationally transparent to the authoritative execution path.

**Comparator contract:** `episodicLearningScopedEquivalenceEqual` determines whether benchmark authority is unchanged — intentionally **not** full-object equality. Answers one question only: **did the benchmark authority change?** Minimal field set; additions require explicit justification (no comparator creep). Full contract: `cursor_list.md` § D.4.0 Step 3 “Scoped equivalence.”

**Separation of concerns:** (1) Same authoritative result? → scoped comparator. (2) Official STRICT envelope? → presence/isolation tests. (3) Diagnostics leaked into authority? → D4 isolation tests.

**Scoped equivalence (E2-28):** Deep equality on `episodicLearningScopedEquivalenceSnapshot`. **Included:** `case_resolutions[]`, `scorable_cases`, `not_scorable_cases`, `summary_evaluation_resolution`, `fingerprint_hash`, `e2_eval_config` (fingerprint + config anchor verdict to producing configuration). **Excluded:** timestamps/run attribution, observational side-channel, non-authority scoring detail, export rollups not in snapshot, envelope labels (`wiring_stage`, `scoring_tier`, `official_scoring` — validated by presence/isolation tests to keep semantic authority independent from envelope validation), ordering/wall-clock.

**Determinism:** Two consecutive identical `B` builds with D4 wiring → `episodicLearningScopedEquivalenceEqual` true (bucket #0). **Preservation:** publication ON vs OFF baseline → same deep-equal scoped snapshot under pinned `makeE2StrictTestConfig()`.

**STRICT authority preservation contract:** `wiring_stage=="B"`; `official_scoring==true` on official path; golden `evaluation_resolution` rollup present; scoped equivalence preserved; fingerprint determinism under D4 wiring; no INTEGRATION tier on official STRICT artifacts; channel side-paths lack authority fields (E2-D2-02 discipline).

**E2-D4-02 preregistered tests:** `testE2D4_02StrictOfficialEnvelopePresence` (presence); `testE2D4_02ScopedEquivalencePreservedWithEvalPublication`, `testE2D4_02ScopedEquivalencePreservedWithD4Workspace`, `testE2D4_02StrictFingerprintDeterminismWithD4Wiring` (preservation); `testE2D4_02NoIntegrationLeakIntoStrictArtifacts` (isolation).

**Step 3 implementation discipline:** Tests only first; reuse golden harness helpers; production changes only for verified wiring defects; gate `THOTH_E2_D4_02=1` + `THOTH_E2_D4_01=1` regression.

#### E2-06 enforcement requirements

Per `E2_PROTOCOL.md` test ID **E2-06** and INTEGRATION artifact fields.

**Required on INTEGRATION artifacts**

| Field / property | Requirement |
|------------------|-------------|
| `scoring_tier` | `"INTEGRATION"` |
| `official_scoring` | `false` |
| Diagnostic content | Trace/diagnostic fields only |
| `e2_outcome` | **Must not be present** on diagnostic authority path |

**Runtime contract**

| Condition | Required behavior |
|-----------|-------------------|
| Production path, test seam unset | Subscriber selects `integrationDefaults()` / `INTEGRATION` tier |
| Summary or JSONL on live path | E2-06 field rules hold |
| Pipeline telemetry (if enabled) | No authoritative scoring fields |
| STRICT harness with D4 wiring present | Unchanged authority artifacts and fingerprint |

E2-06 violations at test time → checkpoint FAIL. D4 does not define new violation taxonomies beyond E2 Protocol v1.2.

#### Proof obligations

D4 is organized as a **proof suite**. Each step establishes a distinct proof type; the composition gate proves they hold together.

| Step | Proof type | Gate |
|------|------------|------|
| **1** | Structural seam proof | `THOTH_E2_D4_STEP1=1` |
| **2** | Live INTEGRATION behavior proof — **E2-D4-01** presence + containment + `integrationDefaults()` behavioral negative | `THOTH_E2_D4_01=1` |
| **3** | STRICT authority preservation proof — **E2-D4-02** benchmark authority unchanged | `THOTH_E2_D4_02=1` |
| **4** | Backward compatibility proof — D3, D2, D1, C5 with flags default OFF | `THOTH_E2_D4_STEP4=1` |
| **5** | Composition proof — full D4 proof suite (Steps 1–4) | `THOTH_E2_D4=1` |
| **D5** | Evolution trust proof | (D5 phase — deferred) |

**Step 1 vs Step 2 boundary (locked)**

| Step 1 (structural) | Step 2 (behavioral) |
|---------------------|---------------------|
| Does the code select the expected config? | Does the running path produce the expected envelope? |

**Step 1 forbidden:** establish INTEGRATION ≡ STRICT equivalence; scoring parity or promotion suitability claims; live-path envelope proof (deferred to Step 2).

**Preregistered test IDs:** E2-D4-01 (live plugin path presence + containment + integrationDefaults negative); E2-D4-02 (STRICT authority preservation).

#### Forbidden changes

**Protocol lock (AGENTS.md):** D4 implements locked E2 v1.2 INTEGRATION semantics — does not revise them. Silent protocol-document edits require explicit approval; `resolveEvaluation()` / Phase B changes require E2 v1.3+.

**D4 forbidden**

- `official_scoring: true` from production live subscriber path  
- `e2_outcome` on INTEGRATION diagnostic authority path  
- INTEGRATION-vs-STRICT promotion, lift, or equivalence claims  
- Treating D4 as “add integration support” rather than prove existing wiring  
- Executive branching on subscriber/eval state for correctness  
- Deployed-traffic validation as D4 proof  
- Mandatory full suite / G2 mid-D4 (deferred to D5 unless explicitly requested)  

#### Required verification gates

**Per-step (targeted only):** `THOTH_E2_D4_STEP1=1`, `THOTH_E2_D4_01=1`, `THOTH_E2_D4_02=1`, `THOTH_E2_D4_STEP4=1`, `THOTH_E2_D4=1`.

**Regression (Step 4):** `THOTH_E2_D3=1`, `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` — orchestrated by `runE2D4Step4Tests()`; flags default OFF; Step 3 not re-run (authority already proven at Step 3 checkpoint); no new test IDs. See `cursor_list.md` § D.4.0 Step 4.

**Deferred to D5:** default full unit-test suite; G2 `ctest`; Phase B fingerprint two-run gate; C5 matrix re-pass.

**Build:** `cmake --build --preset build-debug` green after each step that touches code.

#### Exit criteria (D4 complete)

1. Proof obligation satisfied — INTEGRATION connected with E2-06 containment  
2. D4-I1..I7 verified (structural + behavioral)  
3. E2-D4-01 green — presence + containment contract on live plugin path; `integrationDefaults()` behavioral negative proof  
4. E2-D4-02 green — STRICT authority preserved; `wiring_stage=B` fingerprint stable  
5. `THOTH_E2_D4=1` green — full D4 proof suite (Steps 1–4)  
6. Post-D3 regressions green with applicable flags default OFF  
7. Evidence chain recorded for D5 (Step 1 artifact + Steps 2–3 behavioral proof)  
8. **Pause before D5**

#### Pause discipline

Pause for review after each D4 step and after D4 composition gate (`THOTH_E2_D4=1`). Build/test failure → stop per AGENTS.md Build/Test Failure Rule.

#### Relationship to adjacent checkpoints

| Checkpoint | Relationship |
|------------|--------------|
| **C2** | INTEGRATION envelope on fixtures; D4 extends to live production path |
| **C5** | Equivalence under pinned config; D4 does not reopen |
| **D3** | Observability without authority; D4 applies containment to eval/diagnostic path |
| **D5** | Trust re-proof after all D changes |

**Time estimate (D4):** 3–5 h (excludes D5).

---

### D5 — Evolution trust proof & close-out

**Authority:** [`docs/D5_PROTOCOL.md`](D5_PROTOCOL.md) v0.1 🔒 — D5 trust contract  
**Implementation plan:** `cursor_list.md` § **D.5.0** — Step 1 locked; Steps 2–4 outline; await explicit implementation approval

**Question:** After all D changes, is trust still intact?

> **D5 mirrors C5:** not “prove the new thing works” — **prove nothing important changed.**

D5 is a **meta-proof**, not a feature phase. It verifies the composed proof surface after D1–D4 evolution. Full contract: four constitutional invariants, sub-gates (`THOTH_E2_D5_AUTHORITY`, `THOTH_E2_D5_C5`, `THOTH_E2_D5_DETERMINISM`), closure gate (`THOTH_E2_D5=1`), coverage-gap rule, and reopening boundary — see **`D5_PROTOCOL.md`**.

**Preregistered IDs:** E2-D5-01 (C5 re-pass), E2-D5-02 (Phase B two-run gate), E2-D5-03 (Passive Consumer Law audit).

**Pause before Phase E.**

---

## Regression gates (every checkpoint)

| Gate | Requirement |
|------|-------------|
| Unit tests | E2-C1..C5 + D-phase tests as added |
| STRICT benchmark | `THOTH_E2_WIRING_STAGE=B` — fingerprint `1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4` stable |
| C5 equivalence | `THOTH_E2_C5=1` — golden fixtures MATCH |
| Passive invariant | Structural audits — no reverse edges into Executive |
| Constitutional Rule | Observe / Record / Replay / Present only |

---

## Out of scope (Phase D)

| Item | When |
|------|------|
| Change `evaluation_resolution` / `e2_outcome` semantics | E2 v1.3+ protocol change |
| Eval service cross-run state | Forbidden in all phases |
| Phase E scientific defense | Phase E |
| E3 SCR harness | Separate track (`improvements.md`) |
| GUI as scoring authority | Never — GUI is a subscriber |

---

## Implementation order

| Order | Checkpoint | Runtime semantic change? |
|-------|------------|--------------------------|
| 0 | D0 — Protocol lock ✅ | No |
| 1 | D1 — Channel fan-out + invisibility proof | Additive |
| 2 | D2 — Replay subscriber | Additive — opt-in |
| 3 | D3 — Metrics + trace subscribers | Additive — observability only |
| 4 | D4 — Live INTEGRATION connection | Additive — diagnostic tier only |
| 5 | D5 — Trust re-proof + `PHASE_D_COMPLETE.md` | Validation only |

**Time estimate (rough):** D1 **3–5 h**; D2 **3–4 h**; D3 **4–6 h**; D4 **3–5 h**; D5 **2–4 h**.

---

## Test IDs (preregistered — implement with checkpoints)

| ID | Checkpoint | Asserts |
|----|------------|---------|
| E2-D1-01 | D1 | Multi-subscriber delivery; all receive identical immutable event (byte-identical cross-subscriber comparison) |
| E2-D1-02 | D1 | **Mandatory Executive-path:** one goal run to completion with throwing subscriber registered; `PLAN_COMPLETED` / terminal state identical to baseline; healthy subscriber still delivered; Passive Consumer Law §4 |
| E2-D1-03 | D1 | Invisibility audit: structural (no subscriber-count branching; **Passive Consumer Law §3**) + behavioral: Executive terminal outcome identical at 0 vs N; published outcome fields identical; same-publish immutability via byte-identical subscriber copies |
| E2-D2-01 | D2 | Replay idempotent on captured input; three-part idempotence (storage size, stored object, replay payload); append-only after replays; coexistence — no observable change in EvaluationSubscriber outputs; structural audit — no `publish`, no channel member; never `IEpisodeEventChannel::publish` |
| E2-D2-02 | D2 | Replay subscriber registered or removed — `wiring_stage=B` results unchanged (Passive Consumer Law §5); no STRICT authority from replay path |
| E2-D3-01 | D3 | Metrics sink-only — frozen aggregation ops; opaque `observed_final_success_score`; Metrics JSONL v1.0; eval-independence structural audit; backward compat with `enable_metrics_subscriber=false` |
| E2-D3-02 | D3 | Failure isolation: exactly-once delivery for non-throwing subscribers; locked terminal outcome comparison (excludes metrics/trace/logs); mandatory ordering permutation; catch/log/continue |
| E2-D3-03 | D3 | Structural audit — exclusive ownership; interpret + authority boundaries; publication-mechanism invariant; ordering/JSONL structural audits (narrow authority grep only) |
| E2-D4-01 | D4 | Live plugin path — E2-06 presence + containment contract; `integrationDefaults()` behavioral negative |
| E2-D4-02 | D4 | STRICT authority preservation audit — benchmark authority unchanged |
| E2-D5-01 | D5 | C5 equivalence matrix re-pass |
| E2-D5-02 | D5 | Phase B fingerprint two-run gate |
| E2-D5-03 | D5 | Passive Consumer Law audit on all D subscribers |

---

## Key files (expected touch)

| Area | Likely files |
|------|--------------|
| Event channel | `episode_event_channel.h` / `.cpp` |
| Replay | `replay_subscriber.h` / `.cpp` (new) |
| Metrics / trace | `metrics_subscriber.h` / `.cpp`, `trace_subscriber.h` / `.cpp` (new) |
| Integration connection | `evaluation_subscriber.cpp` (live path verification only) |
| Tests | `tests/unit_tests.cpp` (E2-D1..D5) |
| Docs | [`D5_PROTOCOL.md`](D5_PROTOCOL.md), this file, `PHASE_D_COMPLETE.md`, `cursor_list.md` § D.0.0 / § D.5.0 |

---

## Naming disambiguation

| Name | Scope |
|------|-------|
| **E2 Phase D** (this document) | Evolution tier — passive consumer growth |
| **E1 D1–D5** | E1 harness checkpoints — unrelated |
| **Cognate C1–C7** | Component quality harnesses — unrelated |

---

## D0 lock record

**Locked:** 2026-07-05 (D0); **D3:** complete 2026-07-07; **D4 protocol:** v1 locked 2026-07-07 (§ D4); **D4:** complete 2026-07-08 (`THOTH_E2_D4=1` composition proof green)  
**Review incorporated:** Constitutional Rule elevated; three architectural modes at D0; Passive Consumer Law; GUI as subscriber consequence; D1 invisibility invariant; D2/D3 separation; D3 measure-don't-interpret boundary + subscriber ownership split; D4 containment + live-path definition + protocol lock; D5 as trust re-proof.

**Status:** 🔒 D0 locked — D1 ✅ — D2 ✅ — D3 ✅ — **D4 ✅ complete** — **D5 protocol locked** v0.1 — **D5 Step 1 ✅** — **D5 Step 2 ✅** — paused before Step 3.
