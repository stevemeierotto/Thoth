# Thoth Memory Architecture Summary

Thoth uses an advanced, multi-layered cognitive memory model. It has transitioned from simple JSON storage to a robust, SQLite-backed relational system designed for high-integrity autonomous reasoning.

---

## 1) Episodic Memory ("What Happened")

Episodic memory captures raw conversation history and high-level session metadata.

- **Primary Store:** `agent_workspace/memory.db` (SQLite)
  - `messages` table: Durable storage of user/assistant turns with timestamps.
  - `chat_sessions` table: Manages session lifecycles, titles, and creation metadata.
- **Pruning System:** **MemoryPruner** automatically migrates older turns into "Warm" summaries to prevent context window bloat while preserving key decisions.
- **Trace Logs:** `agent_workspace/decision_trace.jsonl` provides a "black box" recording of the agent's internal reasoning stages for every turn.

**Summary:** This layer provides the narrative thread of the agent's interactions over time.

---

## 2) Process Memory ("How We Solved It")

Process memory is a unique Thoth feature that tracks the mechanics of goal execution.

- **Trajectory Tracking:** `episode_steps` table in SQLite.
- **TrajectoryBuilder:** Summarizes the last 7 actions and failures into a structured JSON object.
- **Semantic Feedback:** The trajectory is embedded as vector **T**, allowing retrieval to be aware of what the agent has *already tried* or *just learned* during a complex task.
- **Strategy Engine:** Analyzes completed trajectories to identify successful patterns and store them as reusable `Strategies`.

**Summary:** This layer enables the agent to learn from its own successes and failures across goals.

---

## 3) Semantic Memory ("Facts & Knowledge")

Semantic memory provides durable, verifiable world knowledge and past planning experience.

- **Structured Fact Store:** A dedicated `facts` table in SQLite for persistent project truths (e.g., "The sandbox root is /home/steve/Thoth/agent_workspace").
- **Plan History Reuse:** Stores successful plans as embeddings. Future planning calls automatically retrieve and inject these historical successes to speed up task generation.
- **Graph Memory [PROTOTYPE]:** SQLite-backed nodes and edges representing causal relationships between code units and decisions.

**Summary:** This layer stores distilled, generalized knowledge rather than raw experience.

---

## 4) GRAG: Vector Storage System ("Directional Retrieval")

Thoth has evolved from standard RAG to **GRAG (Goal-Relative Adaptive Graph Retrieval)**. This is the agent's primary navigation engine for large-scale documentation and code.

- **Abstraction Layer:** `IVectorStore` decoupling (Flat vector store vs. future professional vector DBs).
- **The GRAG Formula:** Retrieval is no longer just "similarity." It is **Directional**:
  - **Goal (G):** Where we want to be.
  - **State (C):** Where we are now.
  - **Direction (D = G - C):** A vector pointing toward the objective.
- **Hybrid Scoring:** Dynamically blends four signals for every retrieved chunk:
  1.  **Query Similarity (Q):** Matches the immediate text.
  2.  **Goal Direction (D):** Matches the remaining work.
  3.  **Trajectory (T):** Avoids redundancy based on process memory.
  4.  **Keyword (TF-IDF):** Ensures precision for technical terms.
- **Multi-Index Routing:** Automatically selects between `Code`, `Docs`, and `Memory` based on the active `ControllerState`.

**Summary:** GRAG provides the semantic retrieval backbone, ensuring that retrieved information is always oriented toward the goal.

---

## 5) Overall Classification

The Thoth architecture spans the full spectrum of cognitive memory types:

- **Episodic:** SQL-backed message history and session tracking.
- **Process:** Trajectory embeddings and action logs (`episode_steps`).
- **Semantic:** Persistent Fact Store and Plan History reuse.
- **Working Memory:** Runtime context assembled from the layers above per request.
- **Vector Memory:** Sandboxed GRAG index anchored to `agent_workspace/rag/`.

This multi-layered approach ensures Thoth is not just a "stateless" prompt-chainer, but a truly **Experience-Aware Cognitive System**.
