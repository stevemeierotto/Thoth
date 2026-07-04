# Phase B Baseline Verification (B6.1)

**Status:** PASS  
**Verified:** 2026-07-04  
**Protocol:** E2 v1.2 · E2-28 scoped equivalence  

---

## Runs compared

| Run | `run_id` | Role |
|-----|----------|------|
| #1 | `run-1783192389220` | Authoritative B5 baseline |
| #2 | `run-1783192418592` | Reproducibility verification |

Source: `docs/baselines/artifacts/phase_b/run_01_summary.json`, `run_02_summary.json`

---

## Pass/fail matrix (E2-28 scoped fields)

| Field | Run #1 | Run #2 | Match |
|-------|--------|--------|-------|
| `evaluation_resolution` (summary) | `SCORED_SUCCESS` | `SCORED_SUCCESS` | PASS |
| `evaluation_resolution` (E2-01) | `SCORED_SUCCESS` | `SCORED_SUCCESS` | PASS |
| `evaluation_resolution` (E2-02) | `SCORED_SUCCESS` | `SCORED_SUCCESS` | PASS |
| `evaluation_resolution` (E2-03) | `SCORED_SUCCESS` | `SCORED_SUCCESS` | PASS |
| `scorable_cases` | 3 | 3 | PASS |
| `not_scorable_cases` | 0 | 0 | PASS |
| `not_scorable_by_reason` | `{}` | `{}` | PASS |
| `success_rate` | 1.0 | 1.0 | PASS |
| `fingerprint_hash` | `1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4` | same | PASS |
| `e2_eval_config` (canonical) | identical | identical | PASS |
| `env_hash` | `3d07ac545db87101d1fb9006526a3b2c15a0762d8593b81af987c43214cde91a` | same | PASS |

**Excluded from comparison (allowed to differ):** `timestamp_ms`, `run_id`, log line ordering, debug metadata, wall-clock fields.

---

## Fingerprint comparison

```
Run #1: 1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4
Run #2: 1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4
```

---

## Diagnosis confirmation (buckets #1–#4)

| Bucket | Cause | Status |
|--------|-------|--------|
| #0 | Equivalent (no mismatch) | **Confirmed** |
| #1 | Config mismatch | Ruled out — `e2_eval_config` identical |
| #2 | Corpus drift | Ruled out — `corpus_snapshot_id` identical |
| #3 | Retrieval nondeterminism | Ruled out — resolutions identical with same config/corpus |
| #4 | Semantic drift | Ruled out — `evaluation_resolution` identical across runs |

E2-28 diagnostic bucket: **0 (equivalent)** for both runs.

---

## Unit test evidence

| Test | Result |
|------|--------|
| E2-25 Official harness envelope | Green |
| E2-26 Golden trio `SCORED_SUCCESS` | Green |
| E2-27 Non-authoritative envelope | Green |
| E2-28 Scoped determinism | Green |
| B5 structural audit (`runScoredEvaluationLoop`) | Green |

Verified via `./build/debug/tests/thoth-unit-tests` (2026-07-04).

---

## Conclusion

**Semantic equivalence confirmed.** Two-run reproducibility gate passed under E2-28 equivalence constraints.
