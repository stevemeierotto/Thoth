about_thoth.md
Thoth — A Cognitive Architecture for Experience-Guided AI Agents
Overview

Thoth is an experimental AI agent architecture designed to explore experience-guided autonomous reasoning.

Unlike most modern agent frameworks that rely primarily on prompt chaining and tool calls, Thoth introduces a structured cognitive loop combining planning, execution, memory, and learning from past experience.

The system is designed to move beyond reactive LLM behavior toward persistent, adaptive problem-solving agents.

At its core, Thoth integrates:

structured planning

persistent episodic memory

execution control

experience reuse

trajectory learning

The project explores how these components can form a self-improving reasoning system without requiring model retraining.

Core Design Principles

Thoth was built around several architectural principles.

1. Planning Before Execution

Most agent systems rely on iterative prompting.

Thoth separates planning from execution.

Goal
↓
Planner
↓
Plan
↓
Executor

This allows the system to generate structured multi-step plans that can be revised during execution.

2. Experience-Guided Reasoning

Thoth introduces GRAG (Goal-Relative Adaptive Graph Retrieval).

GRAG allows the agent to retrieve past experiences relevant to the current goal before planning.

Instead of inventing solutions every time, the system can do:

Goal
↓
Retrieve similar past experiences
↓
Adapt previous solutions
↓
Generate improved plan

This enables case-based reasoning, a long-standing concept in AI research.

3. Deterministic Execution Control

Execution is managed by an Executive Controller responsible for:

step execution

tool invocation

failure detection

plan progress tracking

resumable execution

This separation makes the system:

debuggable

observable

reproducible

4. Persistent Cognitive Memory

Thoth maintains several types of memory:

Plan Memory

Stores structured plans created by the planner.

Trajectory Memory

Stores full execution histories including:

goal
plan
actions
results
revisions
outcome

Graph Memory

A semantic structure connecting knowledge and experiences.

These memory systems allow the agent to reuse experience instead of relying solely on prompts.

Cognate Architecture

The planning and execution system within Thoth is called Cognate.

Cognate provides the structural framework for agent cognition:

Goal
↓
Planner
↓
Plan
↓
Executive Controller
↓
Execution Mode
↓
Results
↓
Memory
↓
Planner Revision

This forms a continuous reasoning loop.

Two execution modes currently exist:

Standard Execution Mode

Sequential plan execution.

Scientific Execution Mode

Designed for experimentation and hypothesis testing.

Hypothesis
↓
Experiment
↓
Result
↓
Evaluation

This allows the system to explore research-style workflows.

Trajectory-Based Learning

A key advancement in the system is trajectory learning.

Instead of storing only plans, Thoth records the entire reasoning and action sequence for each task.

Example trajectory:

goal
↓
initial plan
↓
step execution
↓
failure
↓
plan revision
↓
successful completion

These trajectories allow the planner to learn:

recovery strategies

tool usage patterns

debugging procedures

improved planning approaches

Over time, this enables experience-driven improvement.

Strategy Emergence

Future versions of Thoth aim to detect patterns in trajectories and convert them into reusable strategies.

Example:

Repeated pattern:

search
→ extract data
→ summarize

Can become a reusable strategy:

web_research_strategy

Strategies represent higher-level knowledge extracted from experience.

Observability and Reproducibility

A key goal of the architecture is transparent reasoning.

All execution steps are logged through a structured event system.

This allows:

step-by-step trace analysis

replaying execution

debugging agent decisions

This is critical for building trustworthy autonomous systems.

What Makes Thoth Different

Most agent frameworks today focus on:

prompt engineering

tool orchestration

workflow graphs

Thoth instead focuses on cognitive architecture.

Key differences:

Feature	Typical Agent	Thoth
Structured planning	limited	native
Experience reuse	minimal	core design
Execution controller	simple loops	dedicated subsystem
Trajectory learning	rare	built-in
Strategy emergence	none	planned

The goal is to explore whether structured cognition plus LLM reasoning can produce more capable autonomous systems.

Research Directions

Thoth is designed as a research platform.

Future exploration areas include:

Improved GRAG Retrieval

More sophisticated retrieval and ranking of past experiences.

Strategy Learning

Automatic extraction of reusable strategies from trajectories.

Memory Scaling

Efficient indexing and aging for large trajectory databases.

Multi-Agent Collaboration

Multiple Cognate instances working together on complex problems.

Self-Evaluation

Improved scoring systems for plans and trajectories.

Long-Term Vision

The long-term vision of Thoth is to investigate how persistent experience, structured planning, and adaptive reasoning can enable more capable AI agents.

Rather than relying purely on larger models, the project explores how architectural improvements can produce better autonomous behavior.

The goal is not to create a single agent but to develop a framework for building experience-driven AI systems.

Current Status

Thoth currently includes:

GRAG memory retrieval

Cognate planning architecture

plan persistence

trajectory recording

execution modes

structured observability

Development continues with a focus on:

trajectory learning

strategy extraction

improved planning models

Name

The system is named Thoth, after the Egyptian deity associated with knowledge, writing, and wisdom.

The name reflects the system's emphasis on recording experience and learning from it.

Contact / Collaboration

Thoth is an experimental project exploring new agent architectures.

Researchers, engineers, and organizations interested in:

autonomous agents

cognitive architectures

experience-driven AI systems

may find the project relevant for collaboration or research discussion.
