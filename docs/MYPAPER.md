GRAG: Goal-Relative Adaptive Graph Retrieval
A Retrieval Architecture for Goal-Directed Autonomous Agents
Abstract

Retrieval-Augmented Generation (RAG) has become a standard method for extending large language models with external knowledge. However, conventional retrieval systems treat queries as static similarity searches, retrieving documents solely based on semantic proximity to the input prompt. This approach is often insufficient for autonomous agents executing multi-step tasks, where the relevance of information depends not only on the current query but also on the agent’s goal and trajectory.

This paper introduces Goal-Relative Adaptive Graph Retrieval (GRAG), a retrieval architecture designed to guide knowledge access for goal-driven agents. GRAG augments traditional semantic retrieval with three novel mechanisms:

Goal-Relative Directional Retrieval – retrieval guided by the vector direction between the agent’s current state and its goal.

Adaptive Retrieval Blending – dynamic weighting between semantic similarity and goal-directed retrieval based on task progress.

Experience-Weighted Graph Memory – a graph structure representing successful retrieval trajectories that can adapt over time.

Experimental benchmarks comparing GRAG with conventional RAG show consistent improvements in retrieval quality on goal-disambiguation tasks, demonstrating that goal-aware retrieval can significantly improve information access in autonomous systems.

1 Introduction

Retrieval-Augmented Generation systems extend language models with access to external knowledge sources. In most implementations, retrieval is performed through vector similarity between the user query and document embeddings.

While effective for question answering, this method assumes that relevance is determined entirely by the current prompt.

Autonomous agents operate differently.

Agents performing complex tasks must:

pursue long-term goals

execute multi-step reasoning

maintain internal task state

adapt information retrieval as the task progresses

In such systems, the relevance of knowledge is not static. Information that is useful early in a task may differ significantly from information required near completion.

This observation motivates the development of Goal-Relative Adaptive Graph Retrieval (GRAG), a retrieval architecture designed to support goal-directed reasoning systems.

GRAG reframes retrieval not as a static similarity search but as navigation through knowledge space toward a goal.

2 Background

Most retrieval systems used in large language model pipelines rely on vector similarity search:

score = cosine(query, document)

While effective for retrieving semantically related information, this approach has limitations in goal-oriented contexts.

Specifically:

Queries may be ambiguous without task context

Retrieval does not account for task trajectory

Systems cannot leverage experience from successful reasoning paths

These limitations become apparent in agent architectures that execute structured plans.

GRAG addresses these limitations by introducing goal-relative retrieval signals and graph-structured memory.

3 GRAG Architecture

GRAG modifies traditional retrieval by introducing three novel mechanisms.

3.1 Novelty 1: Goal-Relative Directional Retrieval

Traditional retrieval evaluates documents based solely on their similarity to the query.

GRAG instead introduces a directional retrieval signal defined by the vector difference between the agent’s goal and its current knowledge state.

direction_vector = goal_embedding − current_state_embedding

This vector represents the direction the agent must move in knowledge space to approach the goal.

Documents are scored using both query similarity and directional similarity:

score =
 (1 − α) * cosine(query, document)
 + α * cosine(direction_vector, document)

This mechanism enables the retriever to prioritize information that advances task progress, rather than merely matching the immediate query.

3.2 Novelty 2: Adaptive Retrieval Blending

The optimal balance between semantic similarity and goal-directed retrieval depends on the stage of the task.

Early in a task, goal-directed information is often more important. Later stages may require more precise query matching.

GRAG therefore introduces adaptive blending, dynamically adjusting the weighting parameter α based on task progress.

α = f(goal_distance)

Where:

large goal distance → higher goal-directed weighting

small goal distance → stronger semantic weighting

This adaptive mechanism allows retrieval to evolve as the agent approaches task completion.

3.3 Novelty 3: Experience-Weighted Graph Memory

GRAG also integrates a graph-structured memory layer that captures relationships between knowledge elements encountered during task execution.

Nodes in the graph represent:

knowledge chunks

documents

structured memory entries

Edges represent relationships such as:

sequential retrieval

reasoning dependencies

contextual relevance

In the current prototype implementation, retrieval combines vector similarity with graph traversal:

score =
 0.7 * vector_similarity
 + 0.3 * graph_connectivity

This hybrid scoring allows the system to retrieve not only semantically similar documents but also documents connected through previously observed reasoning paths.

3.4 Adaptive Graph Learning (Implemented)

Dynamic graph edge learning is operational via `GraphRefiner`. Edge weights are adjusted from execution trajectories using a logistic learning rule (learning rate 0.2). Graph density and contribution metrics are logged in `GragDiagnostics` and `grag_benchmark.jsonl` (see `completed_improvements_log.md`, 2026-03-12).

The retrieval graph therefore evolves toward paths that correlate with successful reasoning, complementing the static hybrid vector + graph score in §3.3.

4 Experimental Evaluation

Benchmarks compared standard RAG and GRAG on progressively harder corpora (full run archive: `benchmark_results.md`). The canonical evaluation uses a **311-chunk research-paper corpus** with a **100-case hardened test suite** (2026-03-14), designed to stress goal-disambiguation and distractor noise.

The evaluation measured:

Precision@5

Mean Reciprocal Rank

nDCG@5

Results indicate consistent improvements in retrieval performance for tasks where goal context disambiguates the query. Mean nDCG@5 deltas shrink as corpus size and case difficulty increase; the strongest signal is on **goal-disambiguation** cases, not the corpus-wide mean alone.

Benchmark Summary (canonical — 311 chunks, 100 hardened cases, 2026-03-14)
Metric	RAG	GRAG	Delta
Precision@5	0.510	0.546	+0.036
MRR	0.608	0.679	+0.071
nDCG@5	0.516	0.557	+0.041

Performance gains were particularly strong in goal-disambiguation tasks, where GRAG achieved a **+0.202 nDCG@5** improvement over baseline RAG on that case bucket (same run). An early 100-chunk sandbox (2026-03-09) showed +0.200 mean nDCG@5 — useful for prototype validation but not representative of hardened suites.

These results support the hypothesis that goal-aware retrieval improves information relevance in multi-step reasoning contexts.

5 Discussion

The experimental results suggest that retrieval architectures designed for goal-directed agents benefit from incorporating additional context beyond the immediate query.

GRAG demonstrates that integrating:

goal-relative directional vectors

adaptive retrieval weighting

graph-structured knowledge relationships

can improve retrieval performance in scenarios where standard semantic search is insufficient.

Importantly, the improvements were most significant in tasks where the goal context disambiguates otherwise ambiguous queries.

This suggests that goal-aware retrieval may be particularly valuable for autonomous reasoning systems.

6 Future Work

Several extensions to GRAG remain under development:

~~Adaptive Graph Learning~~ — **Implemented** (2026-03; `GraphRefiner`, see §3.4)

Trajectory-Based Retrieval Metrics
Measuring retrieval contribution to goal completion.

Large-Scale Vector Store Integration
Migration from flat binary indices to scalable vector databases.

Agent-Level Evaluation
Measuring improvements in task completion efficiency rather than retrieval accuracy alone.

7 Conclusion

This work introduces Goal-Relative Adaptive Graph Retrieval (GRAG), a retrieval architecture designed to support autonomous goal-driven agents.

By integrating goal-relative directional retrieval, adaptive blending of retrieval signals, and graph-structured knowledge memory, GRAG extends traditional retrieval-augmented systems to better support multi-step reasoning tasks.

Preliminary benchmarks demonstrate improved retrieval performance in goal-disambiguation scenarios, suggesting that goal-aware retrieval may play an important role in future agent architectures.
