# Thoth: Memory Architecture & Cognitive Learning Synthesis
**Thesis Reference Document**

## 1. Multi-Layered Memory Model
Thoth utilizes a hybrid memory system designed to separate raw experience from distilled knowledge:
- **Episodic (SQLite):** Durable storage of user/assistant turns.
- **Process (Trajectories):** Sequential records of goal execution (`goal → plan → results`).
- **Semantic (FactStore):** Verifiable world knowledge and project truths.
- **Strategic (Strategy Library):** High-success patterns promoted from trajectories.

## 2. Empirical Validation of Learning (Cognate V2)
The "Learning Curve" of the Cognate V2 architecture was verified through a structured two-pass benchmark on March 28, 2026.

### 2.1 Autonomous Strategy Promotion
The system enforces a rigorous **Thesis Gate** for learning:
- **Condition:** 80% Success Rate AND Minimum 3 Occurrences.
- **Verification Result:**
    - **Cold Start:** 0 strategies in library.
    - **Pattern Analysis:** StrategyEngine identified repeating sequences in 9 trajectories.
    - **Promotion:** `RETRIEVAL -> TOOL:llm_reasoning` met the threshold and was promoted.
    - **Warm Start:** 1 strategy available for immediate planning injection.

### 2.2 Numerical Convergence (Scientific Mode)
Scientific reasoning loops demonstrated genuine convergence rather than safety-gate halting:
- **Mean Reasoning Depth:** 4.2 iterations (converged) vs. 1.0 (linear standard).
- **Stability Metrics:** Convergence reached when confidence delta $\Delta < 0.05$ and Jaccard semantic similarity $> 0.9$ over a 2-iteration window.

## 3. Observability & Proof of Causation
The implementation provides three levels of proof for the research paper:
1. **Log Level:** `STRATEGY_INJECTION` and `TRAJECTORY_INJECTION` events prove exactly what prior experience informed each plan.
2. **Data Level:** SQLite schema migrations ensure that every "Plan vs. Reality" comparison is persisted and auditable.
3. **Visual Level:** Real-time Graph Visualizer and Trajectory Viewer provide a live window into the internal state machine.

---
*Verified Baseline established for Thesis Submission.*
