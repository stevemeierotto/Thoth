Thoth UI + System Integration Prompt

You are working on the Thoth system, an experimental AI architecture designed for goal-directed reasoning, trajectory learning, and scientific experimentation. Your task is to understand, unify, and implement the UI so that it accurately reflects and exposes the underlying system.

This document consolidates previously fragmented notes into a single coherent specification.

1. Core Principle

Thoth is NOT a chatbot UI.

It is:

A research console for an evolving cognitive system.

The UI must expose:

planning (what the system intends to do)

execution (what it is doing)

memory (what it has done)

learning (what it has extracted)

experimentation (how it improves itself)

2. Architecture Mapping (Backend → UI)

Map system components directly to UI:

Planner → Central panel (chat + plan view)

ExecutiveController → Plan execution panel (state machine)

GRAG → Right-side diagnostics + retrieval inspector

StrategyEngine → Strategy library panel

Memory (trajectories) → Trajectory viewer

Benchmarks / Experiments → Experiment lab panel

The UI must reflect REAL system state, not approximations.

3. Layout Structure

The UI is composed of:

Top Menu Bar

File | Agent | Benchmarks | Memory | Tools | View | Help

Main Workspace (3 columns)

LEFT:

Sessions

Active Goal

Goal switching

CENTER:

Chat mode

Plan execution mode (step-by-step plan with status)

RIGHT (stacked panels):

GRAG Diagnostics

Retrieval Inspector

Strategy Panel

Bottom Panel (tabbed)

Trajectories

Graph (visual workflow)

Benchmarks / Experiments

Logs

Status Bar

Displays:

model

embedding system

GRAG status

execution state

4. GRAG Diagnostics (Critical) is fixed it shows values now

GRAG must show non-zero values during execution.

If:

Alpha = 0

Magnitude = 0

Then directional retrieval is NOT active.

Requirements:

Diagnostics must be tied to ExecutiveController state

Only active during plan execution

Must display:

alpha (direction strength)

|G - C| (goal-state distance)

scoring mode

5. Plan Execution Panel

This replaces "No active plan execution".

It must show:

current goal

ordered steps

step status (pending, active, complete)

execution state (running, paused, complete)

Support:

step-by-step execution (debug mode)

live updates

6. Tool Output Rendering

Tools must NOT output raw text only.

Each tool must map to a UI renderer:

Examples:

code_modify → diff viewer

web_scrape → web preview pane

summarize_text → structured summary block

Pattern:

Tool → structured result → UI renderer

7. Strategy Engine Visualization

Expose learned strategies.

UI must show:

strategy name

step pattern

usage count

success rate

Optional:

source trajectories

visual flow graph

This is critical to demonstrate learning.

8. Trajectory Viewer

Display full past executions.

Each trajectory includes:

goal

steps taken

tools used

outcome

extracted strategies

This is the system’s memory.

9. Experiment / Scientific System

Thoth supports scientific experimentation.

An Experiment consists of:

hypothesis

configuration

dataset

metrics

results

UI must include:

Experiment Lab Panel

Features:

create experiment

run benchmark

compare results

Example:

Experiment:
GRAG weight optimization

Config:
w_q, w_d, w_k, w_t

Metrics:
nDCG, recall

10. Execution Trace (Unifying Concept)

Every run should produce a visible trace:

Goal → Steps → Tool Outputs → Retrieval → Strategy Updates

The UI should allow users to inspect the full lifecycle of execution.

11. UI Testing / Validation

Implement a GUI test harness.

Purpose:
Ensure backend state matches UI state.

Examples:

strategy added → appears in UI

GRAG runs → diagnostics update

trajectory stored → visible in viewer

12. Implementation Priorities

Fix GRAG diagnostics (must not show zero during execution)

Add menu bar

Add plan execution panel

Add tool renderers

Add strategy panel

Add experiment lab

Add trajectory viewer

Add graph visualization

13. Design Philosophy

The UI must make the system:

observable

debuggable

explainable

reproducible

Avoid:

hidden state

opaque reasoning

chat-only interaction

Final Instruction

When implementing any UI component:

Ask:
"Which part of the system does this expose?"

If it does not expose a real system component, it should not exist.
