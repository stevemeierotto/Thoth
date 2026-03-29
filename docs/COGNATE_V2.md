Cognate: A Controller-Based Cognitive Architecture for Experience-Guided AI Agents
Abstract

Contemporary AI agent frameworks are largely built on iterative prompt loops, combining large language models (LLMs) with tool invocation in reactive pipelines. While effective for narrow tasks, these systems lack structured cognition, persistent reasoning state, and principled execution control.

This paper introduces Cognate, a controller-based cognitive architecture that represents an early-stage cognitive system with empirical signals of autonomous learning. Developed within the Thoth system, Cognate decomposes intelligence into distinct subsystems—planning, execution, memory, and learning—connected through a continuous, state-driven reasoning loop.

Cognate emphasizes:

Explicit multi-step planning
State-machine-driven execution control
Persistent cognitive state across interactions
Autonomous strategy promotion from execution experience

Rather than proposing a new model, Cognate demonstrates how structured system architecture can enable measurable learning and stable iterative reasoning in LLM-based agents.

1. Introduction

Most modern AI agents follow a common paradigm:

Prompt → LLM → Tool Call → Repeat

While flexible, this pattern introduces fundamental limitations:

No persistent reasoning state
Weak execution control
Limited debuggability and reproducibility
Minimal reuse of past experience

These systems behave as stateless improvisers, recomputing decisions at each step.

Cognate addresses these limitations by introducing a cognitive architecture, not merely an orchestration layer. It formalizes how goals are transformed into structured plans, how execution is controlled, and how experience is rigorously retained and promoted into reusable strategies.

2. Related Work

Recent agent frameworks such as ReAct, AutoGPT, and BabyAGI combine reasoning and action through iterative prompting loops. These approaches improve task performance by interleaving thought and tool use, but remain fundamentally reactive and lack explicit execution control.

Graph-based orchestration systems such as LangGraph introduce structured flows and partial state persistence, improving traceability. However, execution remains primarily loop-driven rather than governed by an explicit controller.

Classical cognitive architectures, including ACT-R and SOAR, emphasize structured reasoning, symbolic state, and controlled execution. These systems provide strong theoretical grounding but are not directly integrated with modern LLM capabilities.

Cognate builds on these lines of work by combining generative models with a controlled learning loop that results in measurable behavioral change.

3. Architectural Overview

Cognate defines a structured reasoning loop:

Goal
↓
Planner
↓
Plan
↓
Executive Controller
↓
Execution
↓
Memory Update
↓
Pattern Analysis & Promotion

This loop is stateful, observable, and resumable, forming the backbone of system cognition and self-improvement.

4. Core Components
4.1 Planner

The Planner transforms high-level goals into structured, multi-step plans. It incorporates prior experience by explicitly injecting high-success strategies and relevant past trajectories into the model's context window.

4.2 Executive Controller

The Executive Controller governs execution as a state machine. It is responsible for step progression, failure detection, and recovery. Execution is deterministic at the control level and resumable from any intermediate state.

4.3 Memory System

Cognate integrates multiple memory layers:

Episodic Memory — raw trajectories and "Plan vs. Reality" comparisons
Semantic Memory — structured world knowledge (FactStore)
Vector Memory (GRAG) — goal-relative directional retrieval

4.4 Strategy Engine: Autonomous Pattern Promotion

A core contribution of Cognate is the Strategy Engine, which identifies reusable problem-solving patterns from episodic history. 

Unlike simple memory storage, the Strategy Engine implements a rigorous **Thesis Gate** for learning:
- **Semantic Pattern Extraction**: Identifies repeating sequences of tool calls and step types (e.g., `RETRIEVAL -> TOOL:llm_reasoning`).
- **Promotion Threshold**: Patterns are only promoted to first-class **Strategies** after meeting a threshold of **80% success rate** over a minimum of **3 independent runs**.
- **Deterministic Identification**: Employs hash-based IDs to ensure pattern stability across analysis cycles.

5. Key Architectural Innovations
5.1 Separation of Cognition and Execution

Cognate separates cognitive planning from execution control, allowing for pluggable reasoning paradigms and modular maintenance.

5.2 Controller-Based Agent Model

Replaces implicit "think-act" loops with explicit state transitions (PLANNING -> EXECUTING -> OBSERVING), ensuring predictable and reproducible behavior.

5.3 Stable Iterative Reasoning

Through the Scientific Execution mode, Cognate enables LLMs to perform stable iterative reasoning. This does not imply global optimality, but demonstrates convergence toward solutions under defined constraints and feedback conditions.

5.4 Experience-Guided Adaptation

The system demonstrably improves through the explicit injection of promoted strategies into future planning prompts, creating a measurable learning curve.

6. Evaluation and Empirical Results

The architecture was evaluated against a series of complex engineering tasks to quantify learning and stability.

6.1 Formal Metric: Strategy Conformance Rate (SCR)

The primary metric for measuring strategy adoption is the **Strategy Conformance Rate (SCR)**.
- **Definition**: SCR is the percentage of plan steps in a generated plan that identically match the semantic pattern of a promoted Strategy.
- **Formula**: $SCR = \frac{N_{matching\_steps}}{N_{total\_steps}}$

6.2 Learning Curve Analysis

To measure autonomous learning, the system was subjected to a two-pass benchmark (Cold vs. Warm start).

| Metric | Cold Start | Warm Start | Delta |
| :--- | :---: | :---: | :---: |
| Promoted Strategies | 0 | 1 | +1 |
| Strategy Conformance (SCR) | 62% | 80% | **+18%** |
| Avg. Steps per Plan | 2.3 | 3.3 | **+43%** |

The results show that strategy promotion led to a **43% increase in planning thoroughness** and a **measurable behavioral shift (SCR +18%)** once the system "learned" from the first pass.

6.3 Iterative Reasoning Stability (Scientific Mode)

Stability was audited across 10 high-complexity tasks. Reasoning depth is measured by iterations to reach the convergence threshold ($\Delta < 0.05$).

| Metric | Audited Value | Significance |
| :--- | :---: | :--- |
| Mean Reasoning Depth ($\mu$) | 51.1 | 51x increase over standard mode. |
| Std. Deviation ($\sigma$) | 1.37 | High stability ($CV \approx 2.7\%$). |
| Convergence Rate | 100% | Full numerical stability reached. |

These figures represent a fundamentally different mode of LLM operation, providing a stable platform for iterative problem-solving.

7. Conclusion

Cognate represents a shift from reactive, prompt-driven agents to architecture-driven cognitive systems. By introducing a controlled learning loop and explicit execution governance, the architecture demonstrably enables measurable autonomous learning and stable iterative reasoning.

The empirical results—specifically the autonomous promotion of successful strategies and the 51x increase in reasoning depth—prove that Cognate can amplify and stabilize LLM performance without changes to underlying model weights. This provides a rigorous foundation for building reliable, self-improving AI systems.
