# Phase E STRICT v1 — Authoritative Inference Trio (Step 2)

**Evidence scope:** `n=3_strict_trio`  
**Locked:** 2026-07-09  
**Wiring stage:** `B` (official STRICT)  
**Inference tier:** authoritative (`--authoritative`; External embeddings + pinned LLM metadata)  
**Protocol:** E-AP v1.1 · E2 v1.2  

---

## Step 2 summary

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
af20b692ac59e45f2ba3d65d71433c1b42b2f1bc042652a8085fe7ca8f81cb5f
```

---

## Runs

| Run | `run_id` | Role |
|-----|----------|------|
| A | `run-1783628170667` | Authoritative STRICT evidence |
| B | `run-1783628248447` | Reproducibility verification |

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
