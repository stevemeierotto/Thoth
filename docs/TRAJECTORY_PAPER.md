Trajectory-Based Workflow Learning for Autonomous AI Agents
Experience-Guided Strategy Formation in the Thoth Cognitive Architecture

Author: Steve Meierotto
Project: Thoth Cognitive Agent Architecture
Date: 2026

Abstract

Most modern AI agent frameworks rely on large language models to generate plans and actions on demand. While effective for many tasks, these systems rarely retain detailed knowledge of how tasks were successfully completed in the past.

This paper introduces Trajectory-Based Workflow Learning, a mechanism implemented within the Thoth cognitive architecture that enables agents to record, retrieve, and reuse complete execution histories.

Rather than storing only task outcomes or final answers, the system records full task trajectories, including plans, actions, failures, revisions, and final outcomes. These trajectories form a persistent experience memory that can guide future problem solving.

Over time, repeated successful trajectories can be analyzed and compressed into reusable strategies, allowing the system to transition from reactive planning toward experience-guided behavior.

This work explores how trajectory memory can provide a foundation for self-improving agent workflows without requiring model retraining.

1. Introduction

Large language models have enabled the creation of powerful AI agents capable of planning and executing multi-step tasks. However, most current systems operate in a stateless or short-memory paradigm, where the agent generates solutions from scratch for each new problem.

While some systems store conversational context or vectorized documents, few preserve the process by which tasks were completed.

Human expertise is built largely through experience. When solving a familiar problem, humans rarely start from zero; they recall previous workflows that succeeded or failed.

Trajectory-Based Workflow Learning attempts to provide agents with a similar capability by recording complete task execution histories and making them available for reuse.

The central idea is simple:

Instead of storing only knowledge, store experience.

2. Limitations of Current Agent Memory Systems

Most existing agent frameworks rely on one of three memory types:

Document Memory

Stores external knowledge such as documents or code.

Query → Retrieve documents → LLM generates answer
Conversation Memory

Stores past interactions with the user.

Conversation history → prompt context
Plan Memory

Stores previously generated plans.

Goal → retrieve similar plan → adapt plan

While useful, these memory systems omit a critical dimension:

the actual execution process.

A plan alone does not capture:

what actions were taken

which steps failed

how errors were corrected

which tools were effective

how the final solution was reached

Trajectory-Based Workflow Learning addresses this gap.

3. Task Trajectories

A trajectory represents the full lifecycle of a task.

Rather than storing only the final plan, the system records the complete sequence of events during execution.

A simplified trajectory may appear as:

Goal: Implement GRAG directional scoring

Initial Plan:
  1. Locate scoring implementation
  2. Modify scoring formula
  3. Recompile system
  4. Run unit tests

Execution:
  Step 1 → success
  Step 2 → compile error
  Revision → fix header include
  Step 3 → success
  Step 4 → success

Outcome:
  Task completed successfully

This structure captures the dynamic reasoning process rather than a static description.

The trajectory therefore becomes a case study of problem solving.

4. Trajectory Data Model

Each trajectory stores structured information describing the task lifecycle.

Example representation:

Trajectory
{
    goal
    initial_plan
    steps[]
    revisions[]
    results[]
    success_score
    metadata
}

Each step may contain:

Step
{
    step_id
    description
    action_type
    tool_used
    result
    error
    timestamp
}

This design enables the system to reconstruct the entire reasoning process that led to the final outcome.

5. Trajectory Retrieval

When a new goal is introduced, the system searches its trajectory memory for similar past experiences.

The retrieval process may consider:

goal similarity

success rate

recency

usage frequency

Example ranking concept:

score =
    similarity(goal, trajectory_goal)
  + success_weight
  + recency_weight

The top trajectories are injected into the planning context as execution examples.

Example planner input:

Goal:
Implement directional retrieval scoring.

Relevant past trajectories:

Trajectory A:
- Modified retrieval scoring in rag.cpp
- Required header change in scorer.h
- Compile error resolved by including vector header

Trajectory B:
- Updated ranking algorithm
- Introduced normalization fix

These examples allow the planner to learn from previous processes, not just final answers.

6. Experience-Guided Planning

By injecting prior trajectories into the planning stage, the agent can adapt previous workflows to new tasks.

This introduces a form of case-based reasoning:

Past experience
↓
Adaptation
↓
New solution

The planner is therefore not generating plans purely from language patterns but from observed historical behavior.

This can improve:

tool selection

debugging procedures

workflow ordering

recovery from errors

7. Strategy Emergence

A further extension of trajectory learning is strategy extraction.

If the system repeatedly observes the same successful step sequence, that pattern can be promoted into a reusable strategy.

Example repeated trajectory pattern:

search → extract data → summarize

This may become a reusable strategy:

web_research_strategy

Future plans can then invoke the strategy directly instead of reconstructing the workflow from scratch.

This mechanism allows the system to gradually build a library of proven execution templates.

8. Advantages of Trajectory Learning

Trajectory-Based Workflow Learning provides several advantages over traditional memory systems.

Process Awareness

The system understands not just what worked, but how it worked.

Failure Learning

Failures and revisions become valuable training signals.

Workflow Reuse

Successful execution patterns can be reused across tasks.

Self-Improvement Without Retraining

The agent improves its behavior through stored experience rather than weight updates.

9. Relationship to Cognitive Architectures

Trajectory learning aligns with several historical approaches in AI research, including:

case-based reasoning

episodic memory systems

experience-based planning

However, the integration with large language models enables the system to interpret and adapt stored trajectories in flexible ways.

This creates a hybrid system where:

symbolic execution histories provide structure

language models provide adaptive reasoning

10. Future Research Directions

Several extensions are planned for trajectory-based learning systems:

Automatic Strategy Discovery

Detect recurring successful workflows automatically.

Failure Pattern Detection

Identify repeated mistakes and avoid them in future tasks.

Hierarchical Strategy Learning

Combine small workflows into larger composite strategies.

Multi-Agent Experience Sharing

Allow agents to share trajectories to accelerate collective learning.

11. Conclusion

Trajectory-Based Workflow Learning introduces a new memory paradigm for AI agents: storing and reusing complete problem-solving experiences.

By recording the full lifecycle of tasks and injecting these experiences into future planning, agents can transition from purely reactive systems toward experience-guided problem solving.

This approach enables incremental improvement without model retraining and opens the door to agents that gradually develop libraries of reusable strategies.

While still an early exploration, trajectory learning suggests a path toward more adaptive and persistent autonomous systems.based learning
