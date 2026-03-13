# Thoth: Retrieval Stabilization & Observability
**Version:** 2.0
**Status:** IMPLEMENTATION — Phase 5-7
**Goal:** Transition Thoth from a "black box" retrieval system to an explainable, configurable, and high-performance semantic engine.
Rule #1 read AGENT.md.
Rule #2 read docs/architectural_facts.md.
Rule #3 rebuild after every step that changes code the code must compile before moving on.
Rule #4  Log to docs/completed_improvements_log.md after every phase is completed.
---

## Phase 5: Configuration & Persistence
Lock in the gains from Phase 4 and ensure they are reproducible.

### Step 5.1: Retrieval Config Locking
Move the empirically optimized weights into a persistent configuration.
- **Weights:** $w_q=0.4, w_d=0.4, w_t=0.0, w_k=0.3$.
- **Storage:** Update `Config` class to load these from `agent_workspace/retrieval_config.json`.

### Step 5.2: Benchmark Auto-Archiving
Update `BenchmarkReporter` to generate a human-readable record.
- **File:** `docs/benchmark_results.md`.
- **Content:** Embedding model, corpus size, best weights, and a side-by-side comparison of Precision@5, MRR, and nDCG@5.

---

## Phase 6: Observability (Explainable AI)
Make the GRAG decision process transparent for users and developers.

### Step 6.1: Explicit Score Breakdown
Refactor internal scoring to use an explicit data structure.
```cpp
struct ScoreBreakdown {
    float query_sim;
    float goal_sim;
    float trajectory_sim;
    float keyword_score;
    float final_score;
};
```

### Step 6.2: Diagnostics Injection
Update the `RETRIEVAL_DIAGNOSTICS` event to include the per-chunk breakdown.
- **UI Benefit:** Allows future UI components to show "Why this document?" tooltips.

---

## Phase 7: Reranking Optimization
Enhance the "Search then Rank" pipeline.

### Step 7.1: Candidate Pool Expansion
Modify `RAGPipeline::retrieveRelevant` to use a 2-stage process:
1.  **Stage 1 (Recall):** Retrieve 40 candidates using pure vector similarity + keyword boost.
2.  **Stage 2 (Precision):** Apply full GRAG rescoring to the top 40.
3.  **Stage 3 (Selection):** Return the top 5 rescored results.

### Step 7.2: Verification
Re-run the Phase 13 benchmark. Success is defined by a measurable lift in **nDCG@5** due to the larger candidate pool allowing GRAG more influence.
