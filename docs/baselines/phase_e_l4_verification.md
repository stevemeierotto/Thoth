# Phase E L4 Verification Package

**Package version:** 1  
**Mode:** **Verification only** (reproduction deferred)  
**Evidence scope:** `n=3_strict_trio`  
**Plan:** `docs/cursor_list.md` § E.0.0 Step 3 v3  

---

## Verification vs reproduction (locked)

| Term | Meaning | Step 3 |
|------|---------|--------|
| **Verification** | Confirm the published package is intact and consistent with sealed Step 2 evidence | **This document / verifier** |
| **Reproduction** | Re-execute the harness under the published recipe and compare to sealed evidence | **Deferred** — recipe documented, not executed |

> Step 3 proves: *“This package correctly describes and integrity-checks the published result.”*  
> Step 3 does **not** prove: *“A fresh run today matches the published result.”*

| E-Q2 sub-question | Status |
|-------------------|--------|
| `e_q2_verification` | **true** when `scripts/verify_phase_e_l4.py` exits 0 |
| `e_q2_reproduction` | **false** / `DEFERRED` |

`l4_status: VERIFIED` means package verification passed — **not** that reproduction succeeded.

---

## Cited runs

| Label | `run_id` | Summary | L2 sidecar (recovered) |
|-------|----------|---------|------------------------|
| A | `run-1783639167839` | `run_01_summary.json` | `run_01_l2_environment.json` |
| B | `run-1783639378206` | `run_02_summary.json` | `run_02_l2_environment.json` |

Sealed `run_0{1,2}_benchmark_env.json` files are the Step 2 **completion envelopes** (read-only). Full L2 environment objects for env-compare were recovered from `logs/benchmark_env.jsonl` into `run_0{1,2}_l2_environment.json` without re-running the harness.

---

## Independent-lab verification checklist

1. Check out Thoth at a commit that contains this package (see provenance).  
2. Confirm cited artifact bytes match SHA-256 values in `phase_e_run_manifest.json`.  
3. Confirm protocol/interpretation doc hashes match the manifest.  
4. Confirm `phase_e_l4_package_sha256` matches the canonical listing digest.  
5. Run:
   ```bash
   python3 scripts/verify_phase_e_l4.py
   ```
   Expect exit code **0** and `docs/baselines/phase_e_l4_status.json` with `l4_status: VERIFIED`.  
6. Optionally inspect env-compare (also performed by the verifier):
   ```bash
   python3 scripts/compare_benchmark_env.py \
     docs/baselines/artifacts/phase_e/run_01_l2_environment.json \
     docs/baselines/artifacts/phase_e/run_02_l2_environment.json
   ```
   Expect matching `environment_hash` / `index_hash`; differing `run_id` is expected.  
7. Confirm sealed summaries: both runs `e2_outcome=FAILURE`, `mean_episodic_lift=0.0`, case resolutions `SCORED_SUCCESS` ×3, `cases_passed=1`.  
8. Read Step 2 conclusion in `phase_e_strict_v1.md`: lift=0 is an **observed outcome**, not a benchmark failure.

**Do not** treat a fresh `--authoritative` run as part of this verification package.

---

## Expected sealed rollup (both runs)

| Field | Value |
|-------|-------|
| `summary_evaluation_resolution` / rollup | `SCORED_FAILURE` / `FAILURE` |
| `mean_episodic_lift` | `0.0` |
| `cases_passed` | `1/3` |
| `evaluation_fingerprint` | `ddc5c865b7edbff73a2702ac1b1d2a00075baa6992f480d23d490fe2d551668e` |
| `env_hash` | `155b66a41bbca1bdba441e301841baaa35fd08b6681c3ebc1d7158610a96f790` |
| E2-28 | bucket **#0** |

---

## Reproduction recipe (documented, not executed)

```bash
THOTH_E2_WIRING_STAGE=B ./build/debug/external/basic_agent/run_episodic_learning_benchmark --authoritative
```

Status in manifest: `DOCUMENTED_NOT_EXECUTED`.

---

## Pointers

| Doc | Path |
|-----|------|
| Provenance | `docs/baselines/PHASE_E_PROVENANCE.md` |
| Status JSON | `docs/baselines/phase_e_l4_status.json` |
| Manifest | `docs/baselines/phase_e_run_manifest.json` |
| Step 2 record | `docs/benchmark_results/phase_e_strict_v1.md` |
