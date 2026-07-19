# GRAG — Goal-Relative Adaptive Graph Retrieval
**Thoth Cognitive Retrieval Architecture**

---

## 1. Status
**Current Version:** 1.2
**Implementation Level:** Phase 2 (Directional Core) + Hybrid Scoring + Multi-Index Routing.
**Validation Status:** Empirical lift verified via Phase 13 benchmarks — see [`benchmark_results.md`](benchmark_results.md). **Canonical claim:** +0.041 mean nDCG@5 (311-chunk research corpus, 100-case hardened suite, 2026-03-14); **+0.202** nDCG@5 on goal-disambiguation bucket. Early 100-chunk sandbox (2026-03-09) showed +0.200 mean — not representative of hardened suites.

### [SYSTEM AUDIT NOTE]
- **Core Scoring:** Fully functional.
- **Codebase Indexing:** Read-only verified. Selective re-indexing active.
- **Trajectory Awareness:** **[ACTIVE — G1e KEEP]**. Production `retrieval_config.json` `trajectory: -0.05` (2026-07-19). G1d DROP remains the record for positive weights. Magnitude tuning paused open — [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md). Empty-T executive zeroing remains.
  - **G1e:** KEEP@−0.05 in production; further probes (e.g. `−0.40`) deferred, not abandoned.
- **Subgoal Trees:** **[PLANNED — NOT YET IMPLEMENTED]**. Root goal embedding is used for the entire plan duration.
- **Self-Modification:** **[STUB — optional future expansion]**. The `code_modify` tool exists but its `apply_diff` operation is a non-functional prototype.

---

## 2. Architecture
GRAG (Goal-Relative Adaptive Graph Retrieval) is Thoth's directional cognition layer. It ensures that the knowledge retrieved is not just "similar" to a query, but oriented toward the active objective.

### 2.1 The Retrieval Loop
1.  **Goal (G):** Structured JSON representation of the user objective.
2.  **Current State (C):** Structured JSON representation of the plan progress, recomputed after every successful step.
3.  **Direction (D):** The raw difference vector $D = G - C$.
4.  **Adaptive Blend ($\alpha$):** Dynamically adjusts retrieval focus based on proximity to the goal.

### 2.2 Multi-Index Routing
GRAG routes across four primary indexes with an automated fallback chain:
1.  **PLAN_AWARE (Mode A):** Active goal + Active step. Scans all relevant indexes based on step type.
2.  **GOAL_ONLY (Mode B):** Active goal, no active step. Scans all major indexes with reduced K.
3.  **CONVERSATIONAL (Mode C):** No active goal. Fallback to `conversations_index` using standard RAG.

---

## 3. Scoring Formula
The authoritative scoring formula as implemented in `GragScorer::rescore`:

$$score = \text{HybridVectorScore} + (w_k \times \text{KeywordScore}) + (w_g \times \text{GraphScore})$$

### 3.1 Hybrid Vector Score (The GRAG Core)
Uses the adaptive blend weight $\alpha$:
- $m = \|G - C\|$ (Direction Magnitude)
- $\alpha = \text{clamp}(m / \text{THRESHOLD}, 0.0, 1.0)$

$$\text{HybridVectorScore} = (1 - \alpha) \times w_q \times \text{cos}(Q, \text{chunk}) + \alpha \times w_d \times \text{cos}(D, \text{chunk}) + w_t \times \text{cos}(T, \text{chunk})$$

### 3.2 Confirmed Weights
Optimized during Phase 4 weight sweep:
- **$w_q$ (Query):** 0.4
- **$w_d$ (Direction):** 0.4
- **$w_t$ (Trajectory):** −0.05 **[G1e KEEP 2026-07-19 — polarity production weight; inactive when T empty]**
- **$w_k$ (Keyword/TF-IDF):** 0.3
- **$w_g$ (Graph):** 0.3 **[PROTOTYPE]**
- **THRESHOLD:** 0.3

---

## 4. Embedding Pipeline
- **Backend:** Ollama REST API (`/api/embed`)
- **Model:** `nomic-embed-text`
- **Dimensions:** 768
- **Normalization:** Vectors are normalized during retrieval but stored raw. $D$ is a raw difference vector.

### 4.1 Structured State Schema
To prevent semantic drift, $G$ and $C$ are embedded as structured JSON:
```json
{
  "schema_version": 2,
  "goal": "...",
  "completed_steps_summary": "...",
  "remaining_steps_summary": "...",
  "constraints": "...",
  "known_blockers": "..."
}
```

---

## 5. Benchmark Results

Full run archive: [`benchmark_results.md`](benchmark_results.md). **Always cite corpus size and case mix** when quoting a delta.

### 5.1 Canonical — Hardened 100-case suite (2026-03-14)

*311-chunk research paper corpus; weights $w_q=0.4, w_d=0.4, w_k=0.3, w_t=0.2, w_g=0.3$*

| Metric | RAG (Baseline) | GRAG (Optimized) | Delta |
| :--- | :---: | :---: | :---: |
| Mean Precision@5 | 0.510 | 0.546 | +0.036 |
| Mean MRR | 0.608 | 0.679 | +0.071 |
| Mean nDCG@5 | 0.516 | 0.557 | **+0.041** |

- **Goal-disambiguation bucket:** +0.202 nDCG@5 (primary thesis signal — directional steering on ambiguous queries)
- **Distractor noise resistance:** +0.050 nDCG@5

### 5.2 Historical — Early 100-chunk sandbox (2026-03-09)

*Smoke test only; optimistic mean deltas on a small corpus. Do not use as the general GRAG claim.*

| Metric | RAG (Baseline) | GRAG (Optimized) | Delta |
| :--- | :---: | :---: | :---: |
| Mean Precision@5 | 0.500 | 0.700 | +0.200 |
| Mean MRR | 0.400 | 0.600 | +0.200 |
| Mean nDCG@5 | 0.450 | 0.650 | +0.200 |

---

## 6. Known Gaps & Planned Upgrades
1.  **[PLANNED] Hierarchical Subgoals (Upgrade 1):** Moving from a single $G$ to an active subgoal embedding $G_{active}$ to reduce direction noise in complex plans.
2.  **[ACTIVE — G1e KEEP] Trajectory Awareness (Upgrade 2):** Production `w_t=−0.05` (2026-07-19). G1d DROP remains historical for positive weights. Magnitude tuning paused open — see [`plan_reuse_tuning.md`](plan_reuse_tuning.md), [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md).
3.  **[COMPLETE] Dynamic Graph Learning:** Graph edges are dynamically updated via `GraphRefiner` based on execution success. Edge weights are adjusted using a logistic learning rule (learning_rate=0.2) that rewards successful trajectories and penalizes failures. Graph density metrics (node count, edge count, avg weight, activations) are logged in `grag_benchmark.jsonl`.
4.  **[STUB — optional future expansion] Code Modification:** `CodeModifyTool` needs a functional `apply_diff` before self-building claims apply.

---

## 7. Changelog
- **2026-06-17:** Benchmark §5 split canonical vs. sandbox runs; trajectory audit updated to PARTIAL; corpus-qualified validation status.
- **2026-03-10:** Integrated `ConstraintChecker` into retrieval pipeline. Added `ScoreBreakdown` for explainable retrieval.
- **2026-03-09:** Config Locking: weights moved to `retrieval_config.json`.
- **2026-03-05:** Phase 1-4 completed. Adaptive alpha blending stabilized.
- **2025-12-15:** Initial GRAG Specification draft (Original $G - C$ concept).
