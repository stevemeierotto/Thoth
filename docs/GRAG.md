# GRAG.md
# Goal-Relative Adaptive Graph Retrieval (GRAG)
# Thoth Cognitive Retrieval Architecture

---

## 1. Overview

GRAG (Goal-Relative Adaptive Graph Retrieval) is Thoth's advanced retrieval and cognitive navigation system.

Unlike traditional RAG (Retrieval-Augmented Generation), which retrieves context based solely on query similarity, GRAG performs **goal-directed contextual retrieval** and **planner-aligned navigation**.

GRAG serves two purposes:

1. Context Retrieval
2. Planner Guidance

GRAG is not just a retrieval engine.  
It is a directional cognition layer.

---

## 2. Core Concepts

### 2.1 Embedding Definitions

Let:

G = Goal Embedding  
C = Current State Embedding  
Q = Query Embedding  
D = Direction Vector = G − C  

All embeddings must exist in the same semantic vector space.

---

### 2.2 Adaptive Directional Blending

Directional scoring alone fails when G ≈ C.

To avoid instability, GRAG uses adaptive blending:

m = ||G − C||  

alpha = clamp(m / DIRECTION_THRESHOLD, 0, 1)

Final chunk score:

score = (1 - alpha) * cosine(Q, chunk)
      + alpha * cosine(D, chunk)

Behavior:

- Early stage (low m): behaves like standard RAG
- Mid stage: blended retrieval
- Late stage: fully directional retrieval

This prevents noise amplification when little progress has been made.

---

## 3. Multi-Index Architecture

GRAG operates over multiple vector indexes:

- conversations_index
- knowledge_index
- codebase_index
- plan_history_index
- scientific_index (future)

---

### 3.1 Routing Modes

GRAG operates in three modes:

#### Mode A — Plan-Aware Retrieval
Active goal + active plan step.

Routing logic:
- Determine step_type
- Select relevant indexes
- Retrieve per-index top-K
- Merge and globally re-score using adaptive scoring

#### Mode B — Goal-Only Retrieval
Goal exists but no plan step.

Routing logic:
- Retrieve small K from all major indexes
- Merge and re-score directionally

#### Mode C — Conversational Fallback
No active goal.

Routing logic:
- Default to conversations_index
- Standard cosine similarity only

This prevents brittle index selection.

---

## 4. Planner Integration (Cognitive Navigation)

GRAG influences:

- Context retrieval
- Plan adaptation
- Alternative strategy selection
- Plan history reuse

When generating a new plan:

1. Embed goal G
2. Retrieve past successful plan summaries
3. Score past plans using directional alignment
4. Rank by:
   - goal similarity
   - success score
   - structural similarity

This enables:

- Pattern reuse
- Strategy inheritance
- Progressive refinement

GRAG thus becomes a planner advisor.

---

## 5. Codebase Index Integrity

The codebase_index must NEVER become stale.

Rules:

- Re-embed files only on modification
- Trigger reindex on:
    - file write events
    - git commit events
    - build success events
- Replace only affected chunks
- Never full reindex unless manually triggered

Store metadata:

- file_path
- last_modified_timestamp
- commit_hash (if available)
- embedding_version

Stale embeddings must be detected and invalidated.

---

## 6. Structured State Embedding

To maintain embedding alignment, state must be structured.

Instead of embedding raw state text, embed:

{
  "goal": "...",
  "current_step": "...",
  "completed_steps_summary": "...",
  "remaining_steps_summary": "...",
  "constraints": "...",
  "known_blockers": "..."
}

This reduces semantic drift.

---

## 7. Graph Memory Extension (Future Phase)

GRAG may evolve into hybrid Graph + Vector retrieval.

Nodes:
- goals
- plan steps
- code modules
- scientific hypotheses
- experiments
- failures

Edges:
- contributes_to
- depends_on
- refines
- contradicts
- succeeded_after

Directional scoring can then be combined with graph traversal weights.

This becomes true cognitive navigation.

---

## 8. Failure Modes to Monitor

1. Direction vector collapse (G ≈ C)
2. Index routing misclassification
3. Codebase staleness
4. Embedding drift across model upgrades
5. Overfitting to past plan history

All must be logged and measurable.

---

## 9. Metrics

Track:

- Retrieval relevance score
- Goal completion speed
- Plan revision frequency
- Redundant retrieval rate
- Code modification success rate
- Plan reuse success rate

GRAG must be measurable, not assumed effective.

---

## 10. Long-Term Vision

GRAG transforms Thoth from:

"Context lookup agent"

into:

"Goal-directed cognitive system"

Future enhancements:

- Adaptive threshold tuning
- Reinforcement learning on retrieval success
- Multi-vector goal decomposition
- Temporal goal progression modeling
- Confidence-aware retrieval weighting

---

End of GRAG Specification
