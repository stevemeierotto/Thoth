# GRAG — Goal-Relative Adaptive Graph Retrieval
**Thoth Cognitive Retrieval Architecture**

---

## 1. Status
**Current Version:** 1.2
**Implementation Level:** Phase 2 (Directional Core) + Hybrid Scoring + Multi-Index Routing.
**Validation Status:** Empirical lift verified via Phase 13 Benchmarks (nDCG@5 delta +0.200).

### [SYSTEM AUDIT NOTE]
- **Core Scoring:** Fully functional.
- **Codebase Indexing:** Read-only verified. Selective re-indexing active.
- **Trajectory Awareness:** **[PLANNED — NOT YET IMPLEMENTED]**. Trajectory vectors currently initialize but do not influence scoring in the standard pipeline.
- **Subgoal Trees:** **[PLANNED — NOT YET IMPLEMENTED]**. Root goal embedding is used for the entire plan duration.
- **Self-Modification:** **[STUB]**. The `code_modify` tool exists but its `apply_diff` operation is a non-functional prototype.

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
- **$w_t$ (Trajectory):** 0.2 **[STUBBED to 0.0 in current pipeline]**
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
*Verified on 2026-03-10*

| Metric | RAG (Baseline) | GRAG (Optimized) | Delta |
| :--- | :---: | :---: | :---: |
| Mean Precision@5 | 0.500 | 0.700 | +0.200 |
| Mean MRR | 0.400 | 0.600 | +0.200 |
| Mean nDCG@5 | 0.450 | 0.650 | +0.200 |

---

## 6. Known Gaps & Planned Upgrades
1.  **[PLANNED] Hierarchical Subgoals (Upgrade 1):** Moving from a single $G$ to an active subgoal embedding $G_{active}$ to reduce direction noise in complex plans.
2.  **[PLANNED] Trajectory Awareness (Upgrade 2):** Activating the trajectory embedding $T$ to prevent retrieval redundancy and re-surface failure context.
3.  **[UNCLEAR] Dynamic Graph Learning:** Current graph edges are added upon step success but weights are static. Needs research on dynamic edge weighting.
4.  **[STUB] Code Modification:** `CodeModifyTool` needs a functional `apply_diff` logic to fulfill the "Self-Building" promise.

---

## 7. Changelog
- **2026-03-10:** Integrated `ConstraintChecker` into retrieval pipeline. Added `ScoreBreakdown` for explainable retrieval.
- **2026-03-09:** Config Locking: weights moved to `retrieval_config.json`.
- **2026-03-05:** Phase 1-4 completed. Adaptive alpha blending stabilized.
- **2025-12-15:** Initial GRAG Specification draft (Original $G - C$ concept).
