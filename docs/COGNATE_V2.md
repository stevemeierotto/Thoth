Cognate: A Controller-Based Cognitive Architecture for Experience-Guided AI Agents
Abstract

Contemporary AI agent frameworks are largely built on iterative prompt loops, combining large language models (LLMs) with tool invocation in reactive pipelines. While effective for narrow tasks, these systems lack structured cognition, persistent reasoning state, and principled execution control.

This paper introduces Cognate, a controller-based cognitive architecture developed within the Thoth system. Cognate decomposes intelligence into distinct subsystems—planning, execution, memory, and learning—connected through a continuous, state-driven reasoning loop.

Cognate emphasizes:

Explicit multi-step planning
State-machine-driven execution control
Persistent cognitive state across interactions
Integration with experience-based memory systems

Rather than proposing a new model, Cognate demonstrates how structured system design can improve the reliability, observability, and adaptability of LLM-based agents.

1. Introduction

Most modern AI agents follow a common paradigm:

Prompt → LLM → Tool Call → Repeat

While flexible, this pattern introduces fundamental limitations:

No persistent reasoning state
Weak execution control
Limited debuggability and reproducibility
Minimal reuse of past experience

These systems behave as stateless improvisers, recomputing decisions at each step.

Cognate addresses these limitations by introducing a cognitive architecture, not merely an orchestration layer. It formalizes how goals are transformed into structured plans, how execution is controlled, and how experience is retained and reused.

2. Related Work

Recent agent frameworks such as ReAct, AutoGPT, and BabyAGI combine reasoning and action through iterative prompting loops. These approaches improve task performance by interleaving thought and tool use, but remain fundamentally reactive and lack explicit execution control.

Graph-based orchestration systems such as LangGraph introduce structured flows and partial state persistence, improving traceability. However, execution remains primarily loop-driven rather than governed by an explicit controller.

Classical cognitive architectures, including ACT-R and SOAR, emphasize structured reasoning, symbolic state, and controlled execution. These systems provide strong theoretical grounding but are not directly integrated with modern LLM capabilities.

Cognate builds on these lines of work by combining:

LLM-driven planning
Controller-based execution (inspired by classical architectures)
Persistent, multi-layer memory systems
Experience-guided adaptation mechanisms

The result is a hybrid approach that integrates modern generative models with structured cognitive control.

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
Planner Revision

This loop is stateful, observable, and resumable, forming the backbone of system cognition.

4. Core Components
4.1 Planner

The Planner transforms high-level goals into structured, multi-step plans.

It:

Generates explicit step sequences
Incorporates prior experience when available
Supports iterative refinement

The Planner operates on abstract reasoning, independent of execution.

4.2 Executive Controller

The Executive Controller governs execution as a state machine.

It is responsible for:

Step execution
Tool invocation
Failure detection
Plan progression
Recovery and re-planning

Execution is:

Deterministic at the control level
Fully traceable
Resumable from intermediate states

This separates decision-making from action, enabling controlled system behavior.

4.3 Memory System

Cognate integrates multiple memory layers:

Plan Memory — structured plans
Trajectory Memory — execution histories
Graph Memory (GRAG) — relational knowledge structures

The GRAG system is described in a companion paper and provides the underlying retrieval and knowledge representation layer.

Together, these enable reuse of both knowledge and process.

4.4 Execution Modes

Execution strategies are abstracted through configurable modes:

Standard Mode — sequential task execution
Scientific Mode — hypothesis-driven reasoning

This abstraction allows the same architecture to support different reasoning paradigms.

5. Key Architectural Innovations
5.1 Separation of Cognition and Execution

Cognate separates:

Cognition → Planner + Memory
Execution → Executive Controller

This enables modular reasoning, controlled execution, and improved observability.

5.2 Controller-Based Agent Model

Traditional agents rely on implicit loops:

while not done:
    think()
    act()

Cognate replaces this with explicit state transitions:

state → transition → execution → next state

This introduces predictable, reproducible behavior.

5.3 Persistent Cognitive State

The system maintains structured state including:

Current goal
Active plan
Execution progress

This allows reasoning to evolve over time rather than reset each iteration.

5.4 Experience-Guided Planning

Cognate integrates:

Trajectory memory
Graph-based retrieval (GRAG)

This enables planning informed by prior executions rather than isolated inference.

6. Architectural Implications

Cognate suggests that system architecture plays a critical role in agent capability.

Rather than relying solely on increasing model scale, improvements can emerge from:

Structured reasoning processes
Persistent state management
Controlled execution mechanisms

This does not eliminate the importance of model capability, but indicates that architecture can amplify and stabilize existing model performance.

7. Future Work

Several directions remain for further development:

Strategy extraction from trajectories
Identifying reusable patterns from execution histories to improve planning efficiency.
Multi-agent Cognate systems
Extending the architecture to coordinate multiple agents under shared control structures.
Hierarchical goal decomposition
Enabling nested planning for complex, long-horizon tasks.
Self-evaluation mechanisms
Integrating internal feedback loops for assessing plan quality and execution outcomes.

These directions focus on extending Cognate from a single-agent architecture toward more general adaptive systems.

8. Conclusion

Cognate represents a shift from prompt-driven agents to architecture-driven cognitive systems.

By introducing structured planning, controlled execution, and experience integration, the system improves:

Observability
Reproducibility
Adaptability

This architecture provides a foundation for building more reliable and extensible AI agents without requiring changes to underlying model weights.
