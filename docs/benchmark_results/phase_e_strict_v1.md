# Phase E STRICT v1 — Authoritative Inference Trio (Step 2)

> **✅ SEALED (2026-07-09)** — Step 2 redo after EP-01.5 + E2-33 completion sync. E2-28 bucket **#0**. Execution gate PASS (all arms tokens > 0, all `COMPLETED`). Investigation hold **released**. Prior invalid / failed-redo pairs archived under `superseded_pre_ep015/` and `superseded_e228_fail_pre_fix/`.

> **L4 verification package (Step 3):** [`phase_e_l4_verification.md`](../baselines/phase_e_l4_verification.md) · [`phase_e_l4_status.json`](../baselines/phase_e_l4_status.json) · [`PHASE_E_PROVENANCE.md`](../baselines/PHASE_E_PROVENANCE.md) · `python3 scripts/verify_phase_e_l4.py`. **Verification ≠ reproduction** — reproduction recipe is documented, not executed.

**Evidence scope:** `n=3_strict_trio`  
**Locked:** 2026-07-09 (sealed post-sync redo)  
**Wiring stage:** `B` (official STRICT)  
**Inference tier:** authoritative (`--authoritative`; External embeddings + pinned LLM metadata)  
**Protocol:** E-AP v1.1 · E2 v1.2  
**Harness:** parent `0a38f22` / submodule `77508c4`

---

## Step 2 sealed result (post EP-01.5 + E2-33)

| Run | `run_id` | `env_hash` | Rollup | lift | cases_passed | INCOMPLETE | tokens |
|-----|----------|------------|--------|------|--------------|------------|--------|
| A | `run-1783639167839` | `155b66a4…` | `FAILURE` / `SCORED_FAILURE` | 0.0 | 1/3 | **0** | all > 0 |
| B | `run-1783639378206` | `155b66a4…` | `FAILURE` / `SCORED_FAILURE` | 0.0 | 1/3 | **0** | all > 0 |

**E2-28:** PASS — scoped snapshot deep-equal; diagnosis bucket **#0**.

**Per-case (A = B):** E2-01/E2-02 `SCORED_SUCCESS` with `passes=false` (cold=1, warm=1 → lift 0); E2-03 `SCORED_SUCCESS` `passes=true` (negative control). Under live LLM the model answers E2-01/E2-02 correctly on the cold arm, so episodic lift is zero.

**Repairs that unblocked this seal:** EP-01.5 (live LLM wiring) → metrics `run_id` filter (`649d32c`/`9bf8fd5`) → controller-first arm wait + E2-33 (`77508c4`/`0a38f22`).

### Step 2 conclusion

After completion of EP-01.5 and synchronization repairs, the authoritative harness produced reproducible results with no instrumentation defects. The current authoritative model configuration exhibited no measurable episodic-learning lift on the E2 corpus (`lift = 0.0`). **This is an observed benchmark outcome, not a benchmark failure** — and not a protocol expectation that lift must be nonzero under live inference.

---

## Investigation hold — RESOLVED (historical)

**Bucket classification:** Case-level arm outcomes in the scored loop (bucket 3) — **not** EP-01 test failure (E2-29/E2-30 were green at implementation) and **not** a reported Phase D regression suite failure.

**Pre-flight (1) result: FAIL — silent LLM no-op, not live executive scoring.**

Evidence from Run A (`run-1783628170667`) `logs/cognitive_metrics.jsonl` (all 6 goal arms):

| Signal | Observed | Expected if live LLM/planner exercised |
|--------|----------|----------------------------------------|
| `plan_id` | `e2-plan` (always) | Mock planner ID — unchanged in `--authoritative` mode |
| `total_tokens` | **0** (all arms) | Nonzero if Ollama LLM invoked |
| `planning_tokens` / `prompt_tokens` / `completion_tokens` | **0** | Nonzero |
| `planning_time_ms` | **0–1** | Real planning latency if LLM planner used |
| `llm_synthesis_time_ms` | **0–1** | Real synthesis latency if Ollama called |
| `total_wall_clock_ms` | ~3100–4600 | Consistent with embedding I/O + executive loop, not LLM |
| `terminal_state` | `FAILED` (all arms) | Phase B mock warm arms were `COMPLETED` |
| `final_success_score` / `trajectory_score` | **0.0** | Phase B mock warm arms were **1.0** |

**Root cause (harness, not protocol):** EP-01 `--authoritative` clears `THOTH_MOCK_LLM` and switches embeddings to External, but `runCaseArm()` still uses `EpisodicLearningMockPlanner` and **never** calls `ExecutiveController::set_llm_interface()`. With mock LLM off and no `LLMInterface`, `WorkflowEngine::executeLLM()` returns `"LLMInterface not available for LLM step"` → plan fails → trajectory score 0. STRICT retrieval still works (`warm_retrieval_hit: true` on E2-01/02).

**Implication:** The `FAILURE` / `lift=0` rollup is an **instrument gap** artifact, not a confirmed falsification of episodic lift under live inference. E2-28 bucket #0 only proves reproducibility of this **broken** configuration.

**Secondary flag:** Both runs emitted `TIER_MISMATCH` (`declared_tier: FULL`, `inferred_tier: OLLAMA`) in `logs/benchmark_env.jsonl`.

**Required before sealing as finding:** Complete **EP-01.5** (✅ 2026-07-09) then **redo** Step 2 pair with the execution gate. Prior run IDs (`run-1783628170667` / `run-1783628248447`) remain **invalid empirical evidence** (instrument gap). **Do not** adjust `LIFT_MARGIN` or episode threshold. **Do not** re-run chasing pass on the pre-EP-01.5 path.

**Planner contract (historical clarification):** `plan_id: e2-plan` / `EpisodicLearningMockPlanner` is expected under authoritative mode — topology provider only; it does not imply mock LLM. See `E2_PROTOCOL.md` § Planner / LLM contract (EP-01.5).

---

## Step 2 redo attempt (2026-07-09 — E2-28 FAILED — HALTED)

**EP-01.5:** complete. Execution gate: **PASS** on both runs (all 6 arms `total_tokens > 0`).

| Run | `run_id` | `env_hash` | Rollup | `mean_episodic_lift` | cases_passed |
|-----|----------|------------|--------|----------------------|--------------|
| A | `run-1783635011687` | `94f45827…` | `SUCCESS` / `SCORED_SUCCESS` | **1.0** | 3/3 |
| B | `run-1783635183819` | `94f45827…` | `FAILURE` / `SCORED_FAILURE` | **0.0** | 2/3 |

**Fingerprint:** identical (`ddc5c865…`). **Tokens:** all arms nonzero on both runs.

**E2-28 scoped equivalence:** **FAIL** — `summary_evaluation_resolution` differs (`SCORED_SUCCESS` vs `SCORED_FAILURE`). Case-level `evaluation_resolution` fields match (`SCORED_SUCCESS` ×3); rollup diverges because **non-scoped** fields (`lift` / `passes` / `final_success_score`) differ across runs.

**Probable root cause:** `readLatestMetricsForGoal()` matches by **goal substring only** (not `run_id` / arm). Logs show `[E2] metrics run_id mismatch` on cold arms. Stale `GOAL_COGNITIVE_METRICS` rows from prior arms/runs can pollute `final_success_score` → wrong lift → unstable `passes` / rollup. Example: Run B E2-01/E2-02 cold arms report `score=1` with `terminal_state=INCOMPLETE` (inconsistent with a true cold failure).

**Files likely involved:** `run_episodic_learning_benchmark.cpp` (`readLatestMetricsForGoal`, metrics attribution in `runCaseArm`).

**Proposed repair (not implemented):** Attribute metrics by `run_id` + goal (+ arm/session if needed); reject mismatched rows instead of warning-and-using; re-run Step 2 pair once. **Forbidden:** adjusting `LIFT_MARGIN`, re-running until pass without fixing attribution.

**Artifacts:** partial extracts under `docs/baselines/artifacts/phase_e/run_0{1,2}_*.json`; prior invalid pair archived in `docs/baselines/artifacts/phase_e/superseded_pre_ep015/`. **Not sealed** as Step 2 evidence.

### Instrumentation proof (2026-07-09 — confirmed)

Temporary `[E2][METRICS_ATTR]` logging in `runCaseArm` (requested vs returned `run_id`/`goal`). Two consecutive `--authoritative` + `wiring_stage=B` runs after rebuild.

**Run B** (`run-1783636328980`) — **4× `CROSS_RUN_METRICS_HIT`**:

| Case / arm | Requested `run_id` | Returned `run_id` | Returned score | Obs terminal |
|------------|--------------------|-------------------|----------------|--------------|
| E2-01 cold | `run-1783636328980` | `run-1783636174290` (Run A) | **1** | INCOMPLETE |
| E2-02 cold | `run-1783636328980` | `run-1783636174290` | **1** | INCOMPLETE |
| E2-02 warm | `run-1783636328980` | `run-1783636174290` | **1** | INCOMPLETE |
| E2-03 cold | `run-1783636328980` | `run-1783636174290` | **1** | INCOMPLETE |

Example (verbatim shape):

```
requested:
  run_id = run-1783636328980
  goal = What is my dog's name?
returned:
  run_id = run-1783636174290
  goal = What is my dog's name?
  final_success_score = 1
  terminal_state(obs) = INCOMPLETE
*** CROSS_RUN_METRICS_HIT ***
```

Cold arm scored **1** from a **prior run’s** metrics while the current arm was still `INCOMPLETE` → lift collapse / E2-28 rollup drift. Defect proven. Repair: filter metrics by `run_id` (reject mismatch). Instrumentation still in tree pending repair approval.

### Repair applied (2026-07-09) — cross-run fixed; E2-28 still FAIL

**Commit:** submodule `649d32c` / parent `9bf8fd5` — `readLatestMetricsForGoal(..., runId)` filters by `run_id`; temp `[METRICS_ATTR]` logging removed.

**Post-fix pair:**

| Run | `run_id` | Rollup | lift | cases_passed | Cross-run hits |
|-----|----------|--------|------|--------------|----------------|
| A | `run-1783636708099` | `FAILURE` | 0.5 | 2/3 | **0** |
| B | `run-1783636847032` | `SUCCESS` | 0.5 | 3/3 | **0** |

Cross-run pollution is **gone**. E2-28 scoped equality still **FAIL** (`SCORED_FAILURE` vs `SCORED_SUCCESS`) because case `passes` / lift still diverge.

**New probable root cause (within-run race):** harness wait timeout (~15s) vs live LLM wall clock. Example Run B E2-01 cold: `terminal_state=INCOMPLETE` but metrics for same `run_id`+goal show `outcome=completed`, `final_success_score=1.0`, `total_wall_clock_ms=18248` (> timeout). Harness records INCOMPLETE while later-emitted metrics supply score=1 → cold score inflated → lift collapses (E2-01 lift 0 on B vs 1 on A). Similar: Run A E2-02 warm `INCOMPLETE` score=0 with tokens=124 (metrics not yet visible / timed out).

### Repair applied (2026-07-09) — completion sync ✅ → Step 2 sealed

**Commit:** submodule `77508c4` / parent `0a38f22` — controller-first `waitForArmCompletion` (60s authoritative budget; mock 15s); non-terminal → `INCOMPLETE` + score 0; **E2-33** gate `THOTH_E2_EP015_SYNC=1` PASS.

**Authority precedence (locked):**

1. **Primary:** Controller terminal (`PLAN_COMPLETED` / `PLAN_FAILED` / `PLAN_ABORTED`).
2. **Secondary:** Metrics `(run_id, goal)` only as persisted record / late-event grace — not a score substitute when controller never signaled.
3. **Else:** `TIMEOUT` / `INCOMPLETE` (score 0).

Sealed pair: see § **Step 2 sealed result** above. Failed pre-sync extracts archived in `superseded_e228_fail_pre_fix/`.

---

## Step 2 summary (superseded — pre-EP-01.5 invalid pair)

Phase E Step 2 executed two consecutive authoritative STRICT harness runs on the v1.2 trio (E2-01..03) with L2 environment pinning. **E2-28 scoped equivalence passed (bucket #0)** across Run A and Run B.

**Authoritative rollup (Run A = Run B):**

| Metric | Value |
|--------|-------|
| `evaluation_resolution` | `SCORED_FAILURE` |
| `e2_outcome` (derived) | `FAILURE` |
| `mean_episodic_lift` | 0.0 |
| `scorable_cases` | 3 |
| `not_scorable_cases` | 0 |
| `cases_passed` | 1 / 3 |
| `success_rate` | ≈ 0.333 |

### Per-case resolution

| Case | `evaluation_resolution` | `passes` | `lift` | Notes |
|------|-------------------------|----------|--------|-------|
| E2-01 | `SCORED_SUCCESS` | false | 0.0 | Warm retrieval hit; trajectory scores 0 → no lift |
| E2-02 | `SCORED_SUCCESS` | false | 0.0 | Warm retrieval hit; trajectory scores 0 → no lift |
| E2-03 | `SCORED_SUCCESS` | true | 0.0 | Negative control — no spurious lift |

---

## Tier labeling (mandatory)

| Comparison | Valid? |
|------------|--------|
| Phase E authoritative Run A vs Run B (E2-28) | **Yes** — same tier, same config |
| Phase E authoritative vs Phase B mock baseline | **No** — different inference tiers |

Phase B mock baseline (`phase_b_baseline_v1.md`) reported `mean_episodic_lift = 1.0` and `e2_outcome = SUCCESS` under TfIdf + `THOTH_MOCK_*`. Authoritative runs use real embedding backend metadata and **different trajectory score behavior** on the Executive path — lift numbers are **not** comparable without explicit tier labels.

---

## Pinned configuration (authoritative)

| Field | Value |
|-------|-------|
| `scoring_tier` | `STRICT` |
| `official_scoring` | `true` |
| `wiring_stage` | `B` |
| `embedding_model_version` | `External:2` |
| `model_version_or_weights_hash` | `qwen2.5:3b` |
| `embedding_model` | `nomic-embed-text:v1.5` |
| `embedding_method` | `External` |
| `retrieval_engine_version` | `e2_strict_retrieval_v1` |
| `inference_backend_identifier` | `ollama` (from env sidecar) |
| `ollama.version` | `0.18.0` |

### `corpus_snapshot_id` / index hash

```
c150f0362342c32cefa53e3653f84fa812aa9545b180e2fb6d8982c0124c50f8
```

### `evaluation_fingerprint_hash`

```
ddc5c865b7edbff73a2702ac1b1d2a00075baa6992f480d23d490fe2d551668e
```

### Environment hash (both runs)

```
155b66a41bbca1bdba441e301841baaa35fd08b6681c3ebc1d7158610a96f790
```

---

## Runs (sealed)

| Run | `run_id` | Role |
|-----|----------|------|
| A | `run-1783639167839` | Authoritative STRICT evidence |
| B | `run-1783639378206` | Reproducibility verification |

**Command (locked — identical for A and B):**

```bash
THOTH_E2_WIRING_STAGE=B ./build/debug/external/basic_agent/run_episodic_learning_benchmark --authoritative
```

---

## E2-28 reproducibility

**Result:** PASS — scoped snapshot deep-equal; diagnosis bucket **#0**.

See `docs/baselines/phase_e_baseline_verification.md` for the field matrix.

---

## E-Q2 / E-Q3 partial (Step 2 scope)

| Question | Step 2 delivers |
|----------|----------------|
| **E-Q2** (partial) | L2 pinning + L3 reproduction on declared trio — manifest + frozen artifacts |
| **E-Q3** (partial) | Preregistered protocol executed; FAILURE reported per E2 rules; tier labeled |

L4 publication package: **Step 3**.

---

## Artifacts

| Path | Description |
|------|-------------|
| `docs/baselines/artifacts/phase_e/run_01_summary.json` | Run A summary (frozen) |
| `docs/baselines/artifacts/phase_e/run_02_summary.json` | Run B summary (frozen) |
| `docs/baselines/artifacts/phase_e/run_01_benchmark_env.json` | Run A L2 sidecar |
| `docs/baselines/artifacts/phase_e/run_02_benchmark_env.json` | Run B L2 sidecar |
| `docs/baselines/artifacts/phase_e/episodic_learning_benchmark_snapshot.jsonl` | Case + summary rows (both runs) |
| `docs/baselines/phase_e_run_manifest.json` | L4-ready run manifest |
