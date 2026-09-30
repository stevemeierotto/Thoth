# E.G.A.R.

## Experience-Guided Autonomous Reasoning

**Status:** Draft Architecture Paper. Production loop closure verified 2026-09-29 — see §11.

**Project:** Thoth Cognitive Architecture

---

# Abstract

Large Language Models possess impressive reasoning capabilities but remain fundamentally stateless. While they can generate sophisticated plans and perform complex reasoning within a single interaction, they rarely improve because of their own experiences. Most existing agent frameworks similarly treat each task as an isolated event, relying on prompt engineering, retrieval, or manually authored workflows rather than continuously improving their reasoning process.

Experience-Guided Autonomous Reasoning (EGAR) introduces a framework in which autonomous agents learn from experience through structured evaluation, reflection, and strategy promotion. Rather than memorizing previous conversations, EGAR captures successful reasoning trajectories, measures their effectiveness, extracts reusable strategies, and applies those strategies to future problems.

EGAR is not a replacement for planning or memory systems. Instead, it provides the adaptive learning layer that transforms an autonomous agent from a reactive system into one that continually improves through experience.

---

# 1. Introduction

Human expertise develops through experience.

Individuals attempt tasks, evaluate outcomes, recognize successful patterns, discard ineffective approaches, and gradually construct increasingly reliable strategies.

Modern AI systems rarely follow this process.

Although current LLMs possess enormous prior knowledge, they generally cannot improve from their own execution history without additional offline training. Autonomous agents therefore suffer from repeated failures, inefficient planning, and an inability to accumulate operational knowledge over time.

EGAR addresses this limitation by introducing a disciplined architecture for experience-guided improvement.

Rather than retraining model weights, EGAR improves the cognitive process surrounding the model.

---

# 2. Motivation

Current autonomous agents typically provide:

* Planning
* Tool usage
* Memory retrieval
* Reflection

These capabilities alone do not create continuous learning.

A reflection that is never evaluated becomes another log entry.

A successful plan that is never promoted becomes forgotten history.

EGAR closes this gap by introducing a structured pipeline that converts execution history into reusable reasoning knowledge.

---

# 3. Core Principle

The central hypothesis of EGAR is simple:

> Successful reasoning patterns should become reusable cognitive strategies.

Rather than storing entire conversations, EGAR captures the reasoning process itself.

The objective is not remembering *what* happened.

The objective is learning *how to reason better*.

---

# 4. Architecture

EGAR consists of five major stages.

## 4.1 Experience Capture

Every completed goal produces an Experience Episode.

An episode records:

* Goal
* Plan
* Execution
* Tool usage
* Intermediate reasoning
* Reflection
* Outcome
* Performance metrics

The episode becomes a permanent cognitive artifact.

---

## 4.2 Evaluation

Experiences are evaluated using measurable criteria.

Examples include:

* Success rate
* Plan efficiency
* Tool effectiveness
* Reflection quality
* Strategy reuse
* Stability
* Self-Consistency Rating (SCR)

Evaluation converts qualitative execution into quantitative evidence.

---

## 4.3 Pattern Extraction

Successful episodes are analyzed for recurring reasoning patterns.

These patterns may include:

* Planning approaches
* Decision sequences
* Recovery behaviors
* Tool ordering
* Verification techniques
* Error correction methods

The objective is identifying reasoning behaviors that consistently outperform alternatives.

---

## 4.4 Strategy Promotion

Validated patterns are promoted into reusable cognitive strategies.

Strategies become first-class knowledge objects that may later influence:

* Planning
* Reflection
* Tool selection
* Goal decomposition
* Failure recovery

Promotion transforms temporary experience into long-term operational knowledge.

---

## 4.5 Experience-Guided Reasoning

When solving a new goal, the Executive retrieves relevant experiences and promoted strategies.

These strategies become part of the planning context.

Rather than beginning from an empty prompt, the system begins from accumulated operational experience.

---

# 5. Relationship to GRAG

GRAG answers:

**What knowledge is relevant?**

It retrieves information from long-term memory using goal-relative similarity.

EGAR builds upon GRAG by asking:

**Which reasoning strategies have previously succeeded for similar goals?**

GRAG retrieves knowledge.

EGAR retrieves experience.

Together they provide both factual context and procedural guidance.

---

# 6. Relationship to Cognate

Cognate defines the cognitive architecture.

It separates:

* Executive control
* Planning
* Memory
* Tool execution
* Reflection

EGAR supplies the adaptive learning layer.

Within the Cognate architecture:

The Executive executes goals.

Reflection evaluates execution.

EGAR converts successful reasoning into reusable knowledge.

The result is a closed cognitive learning loop.

---

# 7. Scientific Methodology

A central design objective of EGAR is measurable improvement.

Learning claims should be supported by objective evaluation rather than anecdotal examples.

Accordingly, EGAR emphasizes:

* Controlled benchmark tasks
* Reproducible evaluation harnesses
* A/B comparisons
* Reflection scoring
* Statistical metrics
* Continuous regression testing

This methodology distinguishes experience-guided learning from subjective prompt engineering.

---

# 8. Expected Benefits

Compared to traditional autonomous agents, EGAR aims to provide:

* Reduced repeated failures
* Better planning quality
* Improved recovery from errors
* More efficient tool usage
* Increasing performance over time
* Reusable organizational knowledge
* Explainable learning behavior

The architecture prioritizes continuous improvement without modifying underlying model weights.

---

# 9. Position within Thoth

Within the Thoth architecture:

* GRAG provides memory retrieval.
* Cognate provides cognitive control.
* EGAR provides adaptive reasoning.

Together these components create an autonomous system capable of planning, acting, evaluating, learning, and improving through accumulated experience.

---

# 10. Future Work

Several research directions remain open.

These include:

* Automatic strategy evolution
* Cross-agent knowledge sharing
* Longitudinal learning studies
* Adaptive benchmark generation
* Confidence-aware strategy selection
* Hierarchical experience abstraction
* Multi-agent experience exchange

These extensions represent natural evolutions of the EGAR framework.

---

# 11. Production loop-closure record (2026-09-29)

**Status:** verified 2026-09-29. This section records a causal-chain check of the current production path. It does not change promotion rules, the 0.40 selection floor, GRAG scoring, or certified E2 Phase E.

The checked chain is:

completed production goals → executed trajectories → persisted typed trajectory steps → pattern extraction → strategy promotion → strategy persistence → a later goal → production dense strategy embeddings → cosine similarity → the 0.40 selection gate → `STRATEGY_INJECTION` → promoted strategy text in the model-facing planner prompt.

The pre-registered goals, the 3-trajectory / 0.8 promotion rule (`StrategyEngine`), and `PlannerInjection::kMinStrategySimilarity` (0.40) were left unchanged. Generation used a deterministic fake behind `LLMInterface`. Embeddings used the real production dense path. State was a temporary SQLite database and workspace. The integration test stays opt-in (`THOTH_EGAR_LOOP_CLOSURE=1`) because it needs a reachable embedding service; default tests do not start it.

Observed on that run:

- Embedding method: External (`EmbeddingEngine` on the RAG pipeline). Production uses External unless `THOTH_TEST_SUITE_DEV` selects TF-IDF.
- Embedding model: `nomic-embed-text`. Dense dimension: 768.
- Goal norm: `1.00000012`. Strategy norm: `1`.
- Goal 4 cosine to the promoted strategy: `0.53022629`, above 0.40.
- Promoted pattern: `RETRIEVAL->LLM`, after three successful trajectories, success rate `1`.
- `STRATEGY_INJECTION` carried that strategy id. The captured planner prompt contained `Successful pattern detected: RETRIEVAL->LLM` and `["RETRIEVAL","LLM"]`.

Strategy selection embeds the goal and each strategy description plus pattern with the RAG pipeline `EmbeddingEngine`, then scores them with cosine similarity. GRAG’s keyword channel is a separate `IndexManager` TF-IDF engine (`localTfIdfEngine`). Indexing updates that engine’s document frequencies. Strategy selection does not use it.

When strict embedding is off, a failed External embed falls back to local TF-IDF (`THOTH_EMBED_STRICT` unset). `THOTH_EMBED_STRICT=1` returns an empty vector, or throws from `embedBatch`, instead of that fallback. Generation and embedding are separate services. This verification exercised real dense embeddings while plan text came from the fake generator.

The same verification found a trajectory serialization defect and repaired it without rewriting old rows. `RecordedStep` did not store `PlanStep::type`, so executed step types were dropped on the way into trajectory JSON. `StrategyEngine` could also throw on a JSON null `tool` and drop the trajectory in `catch (...)`. The reader now uses a non-empty tool string other than `"none"`; otherwise it uses an integer `type`. A step with neither is skipped. A missing type is not treated as step type 0. Persisted rows that never stored `type` stay non-qualifying. There is no backfill.

An earlier stop on the same test, at strategy similarity, was not a production similarity result. That run built a direct TF-IDF `EmbeddingEngine` whose document-frequency table was empty, so both vectors were zero and cosine returned 0. The production-faithful External rerun above produced `0.53022629`.

This record establishes that the current production EGAR learning, selection, and injection chain reaches the model-facing planner prompt. It does not establish that EGAR improves answer quality, improves planning quality, produces positive longitudinal lift, or outperforms a baseline. Certified E2 Phase E remains its own frozen protocol, including `mean_episodic_lift = 0.0` on `n=3_strict_trio`. Phase E does not become this end-to-end check, and this check does not revise Phase E.

---

# Conclusion

Experience-Guided Autonomous Reasoning proposes that autonomous intelligence should not merely execute tasks—it should improve because of them.

By transforming execution history into structured cognitive strategies, EGAR enables autonomous agents to accumulate operational knowledge in a measurable, explainable, and reusable manner.

Rather than replacing large language models, EGAR augments them with the missing capability of experiential learning.

Within the Thoth architecture, this adaptive layer completes the cognitive loop, allowing autonomous reasoning systems to evolve through disciplined experience rather than static prompts alone.

