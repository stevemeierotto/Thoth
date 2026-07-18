# G1d — Trajectory Bucket Ablation Benchmark

**Protocol version:** 1.0  
**Locked:** 2026-07-01  
**Applies to:** Thoth `8960c27`, basic_agent `4c45aca`  
**Status:** ✅ Closed **DROP** 2026-07-18 — methodology v1.0 immutable; close-out [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) **G1d-CO v1.2**; production `w_t=0.0`  
**Prerequisite:** E1 ✅ (`docs/benchmark_environment.md`)  
**Blocks:** F5 (semantic trajectory embeddings) — **no F5 work until G1d reaches a documented decision**  
**Related:** `plan_reuse_tuning.md`, `benchmark_case_registry.cpp`, `run_grag_benchmark`, **`G1D_CLOSEOUT_PROTOCOL.md`**

> **Immutability:** Methodology sections in this document may not be edited during an in-flight G1d run. Revisions require **Protocol v1.1** with a new lock date and commit SHA.  
> **Close-out:** Phased infrastructure + checkpoints are normative in [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) (locked 2026-07-18). That document does not alter arms, EPSILON, or the decision matrix below.  
> **Provenance note:** “Ollama authoritative tier” below is historical wording. Under the close-out protocol, authoritative means External embeddings via the instantiated production `InferenceClient`, with honest `backend=` provenance.

---

## Research question

On `TRAJECTORY_DISAMBIGUATES` cases only, does trajectory weight `w_t` improve retrieval quality, hurt it, or add noise?

This is **not** a global weight sweep. It is a bucket-targeted ablation to decide: **keep**, **tune**, or **drop** `w_t` in production config.

---

## Case filter

- Source: `BenchmarkCaseRegistry::getCases()`
- Include: `case_type == "TRAJECTORY_DISAMBIGUATES"` only (~30 cases in the 100-case research corpus)
- Corpus: same hardened research corpus as `run_grag_benchmark` (Ollama authoritative tier)
- Exclude: UNAMBIGUOUS, GOAL_DISAMBIGUATES, Thoth doc cases

---

## Three-arm ablation design

| Arm | `w_q` / `w_d` / `w_k` | `w_t` | Trajectory `T` | Isolates |
|-----|------------------------|-------|----------------|----------|
| **A — no weight** | 0.4 / 0.4 / 0.3 | **0.0** | Built via `TrajectoryBuilder` | GRAG without trajectory term |
| **B — production** | 0.4 / 0.4 / 0.3 | **0.2** | Built via `TrajectoryBuilder` | Current shipped behavior |
| **C — empty T** | 0.4 / 0.4 / 0.3 | **0.2** | **Forced zero vector** | Weight on but no signal (construction vs weight) |

Shared settings across arms: `top_k = 5`, same goal/query/trajectory text per case, same index binding.

**Reference (informational only):** RAG baseline (`w_q=1, w_d=0, w_t=0`) from existing `runComparison` — report in summary but **not** used for keep/drop decision.

---

## Per-case winner (required)

Aggregate mean nDCG alone is insufficient. Every case must record a **winner** among A / B / C by **nDCG@5** (higher wins).

| Outcome | Rule |
|---------|------|
| Single winner | Unique highest nDCG@5 |
| **TIE** | For arms X and Y with highest scores: **TIE if \|X − Y\| < EPSILON** where **EPSILON = 0.001** (nDCG@5). Equality counts as TIE. |

Three-way ties among A/B/C use the same EPSILON pairwise among co-top arms.

**Summary must report:**

```
A wins: N
B wins: N
C wins: N
Tie:  N
```

Plus per-case table (case_id → winner arm → nDCG A/B/C).

**Rationale:** Mean nDCG can hide localized failures (e.g. B wins 16/30 but loses badly on 2 cases → negative mean delta).

---

## Decision matrix (locked — dual criteria for KEEP)

Evaluate **Arm B vs Arm A** only for production decision. Arm C informs mechanism.

| Decision | Criteria (all required for KEEP) |
|----------|----------------------------------|
| **KEEP** | (1) B wins ≥ **60%** of cases (ties excluded from denominator) **AND** (2) mean nDCG@5 delta **(B − A) > 0** |
| **TUNE** | B wins ≥ 40% but KEEP criteria not met → follow-up micro-run at `w_t ∈ {0.05, 0.1}` before drop |
| **DROP** | B wins < 40% **OR** mean delta (B − A) ≤ 0 after TUNE micro-run |
| **CONSTRUCTION BUG** | C ≈ A but B < A on majority → noisy `T`; do not tune weight; fix/redesign trajectory path (F5 candidate **after** G1d closes) |

| Arm C pattern | Interpretation |
|---------------|----------------|
| C ≈ A, B < A | Trajectory **construction** hurts when present |
| C ≈ B | Weight active but empty T behaves like full T → executive zeroing may not match benchmark path |
| B > A and B > C | Trajectory signal helps when both weight and T present |

**No production `retrieval_config.json` change** until decision is logged in `completed_improvements_log.md`.

---

## Harness contract

| Item | Value |
|------|-------|
| Binary | `run_trajectory_ablation_benchmark` |
| JSONL | `logs/trajectory_ablation_benchmark.jsonl` |
| E1 | `BenchmarkRun::create` + `bindIndex`; one `run_id` per run |
| Terminal events | `TRAJECTORY_ABLATION_COMPLETE` / `TRAJECTORY_ABLATION_ABORTED` |
| Per-case event | `TRAJECTORY_ABLATION_CASE` |
| Summary event | `TRAJECTORY_ABLATION_SUMMARY` |

### `TRAJECTORY_ABLATION_CASE` fields

- `run_id`, `env_hash`, `case_id`, `case_type`
- `arm` (`A` | `B` | `C`)
- `ndcg_at_5`, `reciprocal_rank`, `precision_at_5`
- `winner` (`A` | `B` | `C` | `TIE`) — computed after all three arms for that case
- `ndcg_a`, `ndcg_b`, `ndcg_c` (for audit)

### `TRAJECTORY_ABLATION_SUMMARY` fields

- `cases_run`, `sample_mode`
- `a_wins`, `b_wins`, `c_wins`, `ties`
- `mean_ndcg_a`, `mean_ndcg_b`, `mean_ndcg_c`
- `mean_ndcg_delta_b_vs_a` (B − A)
- `decision` (`KEEP` | `TUNE` | `DROP` | `CONSTRUCTION_BUG` | `PENDING`)
- `decision_rationale` (one line)

---

## Test IDs (planned)

| ID | Asserts |
|----|---------|
| G1d-01 | Case filter returns only `TRAJECTORY_DISAMBIGUATES` |
| G1d-02 | Arm C forces zero `T` |
| G1d-03 | Smoke: 2 cases × 3 arms; shared `run_id` |
| G1d-04 | Arm configs differ as specified |

---

## Tiers

| Tier | Stack | Purpose |
|------|-------|---------|
| CI / smoke | TfIdf probe stack | Wiring, winner logic, E1 identity |
| Authoritative | Ollama + research corpus | Decision matrix |

If Ollama unreachable at close-out: log **“wired, Ollama decision pending”** — do not mark G1d ✅.

---

## Out of scope

- F5 semantic trajectory embeddings
- `TrajectoryBuilder` redesign (unless decision = CONSTRUCTION_BUG)
- Global weight sweep across all case types
- B1 corpus expansion
- Production GRAG path changes
- IndexManager drift monitoring

---

## Implementation sequence (locked methodology; close-out superseded)

1. **G1d spec** — this document ✅  
2. **G1d harness** — `run_trajectory_ablation_benchmark` ✅  
3. **G1d run** — authoritative External inference + CI smoke  
4. **G1d decision** — log entry + update `plan_reuse_tuning.md`, `GRAG.md`, `cursor_list.md`  

**Close-out execution order** (including Phase A0 backend-neutral infrastructure and review gates): **normative in [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) v1.0** — do not start the expensive authoritative run until Checkpoint A0 passes owner review.

---

*Locked: 2026-07-01 — per review: per-case winners, dual KEEP criteria (win rate + mean delta). Close-out protocol locked 2026-07-18.*
