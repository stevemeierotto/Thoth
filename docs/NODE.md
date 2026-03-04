# NODE.md
# Thoth Execution Node System Specification
# Lightweight Workflow Engine (n8n-style Execution Wiring)

---

## 1. Purpose

The Node System is Thoth's execution wiring layer.

It is NOT the cognitive engine.
It is NOT the planner.

It is a modular execution harness that allows:

- Isolated subsystem testing
- Controlled data flow
- Visual workflow composition
- Safe experimentation without impacting core executive logic

The node system exists to:
1. Execute tool chains
2. Test subsystems (e.g., RAG vs GRAG)
3. Inspect inputs and outputs
4. Provide observability during development

Keep it simple.

---

## 2. Architectural Principles

1. Nodes are black-box execution units.
2. Nodes have explicit inputs and outputs.
3. Nodes do NOT contain planning logic.
4. Nodes do NOT autonomously rewire themselves.
5. Nodes are deterministic given the same inputs.
6. The Executive may trigger node graphs, but nodes do not control the Executive.

---

## 3. Core Concepts

### 3.1 Node

A Node is a single execution unit.

Each node must define:

- id
- type
- input_schema
- output_schema
- execute(input_data) -> output_data

Example interface (conceptual):

---

### 3.2 Edge

An Edge connects:

Node A output → Node B input

Edges define:

- source_node_id
- target_node_id
- output_key
- input_key

Edges do not transform data.

---

### 3.3 Graph

A Graph is:

- A collection of nodes
- A collection of edges
- An entry node
- One or more terminal nodes

Execution order is determined by dependency resolution.

No cycles allowed in v1.

---

## 4. Minimal Node Types (v1)

### 4.1 Input Node

Purpose:
- Accept user input
- Accept test input for subsystem validation

Output:
{
  "query": "...",
  "goal": "...",
  "state": "...",
  "metadata": {...}
}

---

### 4.2 Retrieval Node

Supports:
- Standard RAG mode
- GRAG mode

Configuration:
{
  "mode": "rag" | "grag",
  "top_k": int,
  "index": "conversations" | "knowledge" | "codebase" | "auto"
}

Output:
{
  "retrieved_chunks": [...],
  "scores": [...],
  "diagnostics": {...}
}

Diagnostics must include:
- scoring_type
- alpha (if GRAG)
- direction_magnitude (if GRAG)

This enables comparison testing.

---

### 4.3 Prompt Assembly Node

Purpose:
- Construct final prompt before LLM call

Input:
{
  "query": "...",
  "retrieved_chunks": [...],
  "system_prompt": "...",
  "plan_context": "..."
}

Output:
{
  "final_prompt": "..."
}

Must allow inspection before execution.

---

### 4.4 LLM Node

Purpose:
- Call language model

Input:
{
  "final_prompt": "...",
  "model_config": {...}
}

Output:
{
  "response": "...",
  "token_usage": {...}
}

---

### 4.5 Tool Node

Purpose:
- Execute an external tool

Input:
{
  "tool_name": "...",
  "tool_args": {...}
}

Output:
{
  "tool_result": {...},
  "status": "success" | "failure"
}

---

### 4.6 Output Node

Purpose:
- Finalize and return result

Output:
{
  "final_response": "...",
  "execution_trace": {...}
}

---

## 5. Execution Engine Rules

1. Resolve nodes in topological order.
2. Each node executes only when all required inputs are present.
3. Execution state must be logged.
4. Node failures must not crash entire system — return structured error.

Execution log example:

{
  "node_id": "...",
  "execution_time_ms": 123,
  "input_snapshot": {...},
  "output_snapshot": {...}
}

---

## 6. Testing and Validation Use Cases

The node system is the primary validation harness for GRAG.

Example test graph:

Input Node
    → Retrieval Node (mode=rag)
    → Output Node

Duplicate graph:

Input Node
    → Retrieval Node (mode=grag)
    → Output Node

Compare:

- Retrieved chunks
- Score differences
- Direction magnitude
- Alpha value

No Executive involvement required.

This allows safe parallel evaluation.

---

## 7. Configuration Requirements

Nodes must be:

- Serializable to JSON
- Loadable from configuration file
- Editable without recompilation

Graph definition example:

{
  "nodes": [...],
  "edges": [...],
  "entry_node": "input_1"
}

---

## 8. Observability

The node system must support:

- Execution trace logging
- Timing metrics
- Diagnostic payload inspection
- Manual input injection

No hidden execution.

---

## 9. Integration with Executive

The Executive may:

- Trigger a predefined graph
- Supply structured goal + state
- Receive output

The node system does NOT:

- Make planning decisions
- Modify memory autonomously
- Rewrite graphs dynamically (v1)

Keep it simple.

---

## 10. Future Extensions (Not v1)

- Conditional branching
- Loop nodes
- Subgraph execution
- Parallel execution
- Dynamic graph mutation
- Graph visualization UI

These are explicitly postponed.

---

## 11. Design Constraint

The node system must remain:

Simple.
Deterministic.
Testable.

It is an execution harness — not an autonomous agent.

---

End of NODE System Specification
