# Phase B Baseline v1 — E2 Episodic Learning (STRICT)

**Baseline version:** Phase B v1  
**Locked:** 2026-07-04  
**Wiring stage:** `B` (authoritative)  
**Protocol:** E2 v1.2  

---

## B5 system summary

Phase B established the first **authoritative** STRICT evaluation baseline for Thoth episodic memory learning. Evaluation semantics (`evaluation_resolution`), export contracts (`e2_outcome` derived at export only), fingerprint derivation, and reproducibility requirements are frozen as of this baseline. B5 switched the harness default to `wiring_stage=B` with a single `runScoredEvaluationLoop()` implementation and a two-run reproducibility gate.

---

## Pinned configuration

| Field | Value |
|-------|-------|
| `scoring_tier` | `STRICT` |
| `official_scoring` | `true` |
| `wiring_stage` | `B` |
| `embedding_model_version` | `TfIdf:2` |
| `retrieval_engine_version` | `e2_strict_retrieval_v1` |
| `model_version_or_weights_hash` | `mock` |

### `corpus_snapshot_id` / index hash

```
a3cb8067569c3b59146d0d63eb153d9ae9d094f1cc2b5a28c04fa73f7a010fef
```

### `evaluation_fingerprint_hash`

```
1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4
```

### Environment hash

```
3d07ac545db87101d1fb9006526a3b2c15a0762d8593b81af987c43214cde91a
```

---

## Rollup (authoritative run #1)

| Metric | Value |
|--------|-------|
| `evaluation_resolution` | `SCORED_SUCCESS` |
| `e2_outcome` (derived) | `SUCCESS` |
| `mean_episodic_lift` | 1.0 |
| `scorable_cases` | 3 |
| `not_scorable_cases` | 0 |
| `success_rate` | 1.0 |
| `cases_passed` | 3 / 3 |

### Per-case resolution

| Case | `evaluation_resolution` | `e2_outcome` |
|------|-------------------------|--------------|
| E2-01 | `SCORED_SUCCESS` | `SUCCESS` |
| E2-02 | `SCORED_SUCCESS` | `SUCCESS` |
| E2-03 | `SCORED_SUCCESS` | `SUCCESS` |

---

## Test evidence (E2-25–E2-28)

| ID | Asserts | Status |
|----|---------|--------|
| E2-25 | Official `B` envelope (`official_scoring`, `scoring_enabled`, `wiring_stage`) | PASS |
| E2-26 | Golden trio `SCORED_SUCCESS`, `not_scorable_cases == 0` | PASS |
| E2-27 | `A5` / `SCORING` non-authoritative | PASS |
| E2-28 | Scoped determinism across two evaluation builds | PASS |

---

## Reproducibility statement

**Two-run reproducibility gate passed under E2-28 equivalence constraints.**

See `docs/baselines/phase_b_baseline_verification.md` for the full pass/fail matrix.

---

## Artifacts

| Path | Description |
|------|-------------|
| `docs/baselines/artifacts/phase_b/run_01_summary.json` | Authoritative summary (frozen) |
| `docs/baselines/artifacts/phase_b/run_02_summary.json` | Verification summary (frozen) |
| `docs/baselines/artifacts/phase_b/episodic_learning_benchmark_snapshot.jsonl` | Case + summary rows for both runs |
| `docs/baselines/BASELINE_PROVENANCE.md` | How this baseline was produced |
| `docs/baselines/fingerprint_lock.md` | Fingerprint contract |
