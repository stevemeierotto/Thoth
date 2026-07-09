# Phase E STRICT Verification (Step 2)

**Status:** PASS (E2-28 reproducibility)  
**Verified:** 2026-07-09 (post EP-01.5 + E2-33 completion sync)  
**Protocol:** E-AP v1.1 · E2 v1.2 · E2-28 scoped equivalence  
**Evidence scope:** `n=3_strict_trio`  
**Harness:** parent `0a38f22` / submodule `77508c4`

---

## Runs compared

| Run | `run_id` | Role |
|-----|----------|------|
| A (#1) | `run-1783639167839` | Authoritative STRICT evidence |
| B (#2) | `run-1783639378206` | Reproducibility verification |

Source: `docs/baselines/artifacts/phase_e/run_01_summary.json`, `run_02_summary.json`

---

## Pass/fail matrix (E2-28 scoped fields)

| Field | Run A | Run B | Match |
|-------|-------|-------|-------|
| `summary_evaluation_resolution` | `SCORED_FAILURE` | `SCORED_FAILURE` | PASS |
| `evaluation_resolution` (E2-01) | `SCORED_SUCCESS` | `SCORED_SUCCESS` | PASS |
| `evaluation_resolution` (E2-02) | `SCORED_SUCCESS` | `SCORED_SUCCESS` | PASS |
| `evaluation_resolution` (E2-03) | `SCORED_SUCCESS` | `SCORED_SUCCESS` | PASS |
| `scorable_cases` | 3 | 3 | PASS |
| `not_scorable_cases` | 0 | 0 | PASS |
| `fingerprint_hash` | `ddc5c865…` | same | PASS |
| `e2_eval_config` (canonical) | identical | identical | PASS |
| `env_hash` | `155b66a4…` | same | PASS |
| `e2_outcome` | `FAILURE` | `FAILURE` | PASS |
| `mean_episodic_lift` | 0.0 | 0.0 | PASS |
| `cases_passed` | 1/3 | 1/3 | PASS |

**Excluded from comparison (allowed to differ):** `timestamp_ms`, `run_id`, wall-clock fields, per-arm `wall_clock_ms`.

---

## Execution gate / completion sync

| Check | Result |
|-------|--------|
| All arms `terminal_state=COMPLETED` | PASS (0× `INCOMPLETE`) |
| All arms `total_tokens > 0` | PASS |
| Cross-run metrics hits | 0 (run_id filter) |
| E2-33 (`THOTH_E2_EP015_SYNC=1`) | PASS before this pair |

---

## Fingerprint comparison

```
Run A: ddc5c865b7edbff73a2702ac1b1d2a00075baa6992f480d23d490fe2d551668e
Run B: ddc5c865b7edbff73a2702ac1b1d2a00075baa6992f480d23d490fe2d551668e
```

---

## Diagnosis confirmation (buckets #1–#4)

| Bucket | Cause | Status |
|--------|-------|--------|
| #0 | Equivalent (no mismatch) | **Confirmed** |
| #1 | Config mismatch | Ruled out — `e2_eval_config` identical |
| #2 | Corpus drift | Ruled out — `corpus_snapshot_id` identical |
| #3 | Retrieval nondeterminism | Ruled out — resolutions identical with same config/corpus |
| #4 | Semantic drift | Ruled out — scoped fields identical across runs |

E2-28 diagnostic bucket: **0 (equivalent)**.

---

## Conclusion

**Semantic equivalence confirmed** between consecutive authoritative STRICT builds after EP-01.5 + completion sync. Step 2 reproducibility gate **green**.

### Step 2 conclusion (empirical observation)

After completion of EP-01.5 and synchronization repairs, the authoritative harness produced reproducible results with no instrumentation defects. The current authoritative model configuration exhibited no measurable episodic-learning lift on the E2 corpus (`lift = 0.0`). **This is an observed benchmark outcome, not a benchmark failure** — and not a protocol expectation that lift must be nonzero under live inference.

Under live LLM, cold and warm arms both score 1.0 on E2-01/E2-02 (model answers without needing the planted episode) → lift 0. Do not compare to Phase B mock `SUCCESS` baseline without tier labels.
