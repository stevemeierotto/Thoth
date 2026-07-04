# Phase B Fingerprint Lock

**Baseline version:** Phase B v1  
**Locked:** 2026-07-04  
**Authoritative fingerprint:**

```
1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4
```

---

## Primary rule

> **Fingerprint is a derived artifact of evaluation stability, not a primary identifier.**

Fingerprint stability is a **function of** evaluation stability — not a separate truth domain. Do not treat fingerprint mismatches as config bugs when the underlying cause is semantic drift, and do not treat retrieval noise as semantic drift without ruling out config and corpus drift first.

---

## Derivation

`evaluation_fingerprint_hash` is computed by `computeEvaluationFingerprint(strictConfig)`:

1. Canonical JSON is built from pinned `E2EvalConfig` (tier + version pins).
2. `fingerprint_hash = SHA-256(canonical_json)`.

The fingerprint is a deterministic function of:

- **`strictConfig` / `e2_eval_config`** — version pins, tier
- **Retrieval corpus hash** — `corpus_snapshot_id` / index hash (embedded in config at run time)
- **`evaluation_resolution` output** — per-case + summary classification (must be stable for the fingerprint gate to be meaningful)

---

## Dependency chain

```
strictConfig (pinned)
    → corpus_snapshot_id / index_hash
    → retrieval canonicalization (sorted + chunk_id tie-break)
    → evaluation_resolution (per case + rollup)
    → fingerprint_hash (export artifact)
    → e2_outcome (derived at export only, SCORED_* only)
```

---

## Fingerprint mismatch diagnosis

| # | Cause | How to detect |
|---|-------|---------------|
| **1** | Config mismatch | `e2_eval_config` diff between runs |
| **2** | Corpus drift | `corpus_snapshot_id` / index hash diff |
| **3** | Retrieval nondeterminism | Identical inputs, different retrieved order/hits |
| **4** | Semantic drift | Identical inputs + retrieval, different `evaluation_resolution` |

**Shortcut:** Fingerprint mismatch with **same** `evaluation_resolution` → investigate #1–#3 first. Fingerprint match with **different** `evaluation_resolution` → #4 semantic drift.

---

## E2-28 equivalence (reproducibility gate)

Two official `B` runs are equivalent **iff**:

1. `evaluation_resolution` identical (per case + summary)
2. Scoped fields match (`fingerprint_hash`, `e2_eval_config`, scorable classification)
3. Diagnosis bucket identical (#0 = equivalent)

Excluded from comparison: timestamps, `run_id`, log ordering, debug metadata.

---

## Phase B freeze

From Phase B v1 onward, the meaning of `evaluation_resolution` and `e2_outcome` is **immutable**. Future protocol versions must bump explicitly (e.g. E2 v1.3) to change semantics or invalidate this fingerprint contract.
