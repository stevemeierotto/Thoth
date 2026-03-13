TOOLS.md
Thoth Tool System Specification

Version 1.0
Status: Authoritative

1. Purpose

The Tool System enables deterministic, structured capability execution within Thoth agents.

Tools provide controlled access to external systems (APIs, email, financial services, etc.) through a standardized interface governed by the ToolRegistry.

SQLite-backed memory is authoritative and must never be accessed directly by tools.

This document defines the mandatory contract for all tool implementations.

2. Architectural Principles

Tools are deterministic units of work.

Tools do not mutate agent memory.

Tools do not call other tools.

Tools do not maintain internal persistent state.

Tools operate only on validated JSON input.

Tools return structured JSON output only.

Tool execution must be observable and auditable.

Tool availability is globally registered but restricted per agent via allowlists.

3. Tool Lifecycle

Tool class is implemented.

Tool self-registers with ToolRegistry.

AgentDefinition specifies allowed tools (allowlist).

Runtime validates:

Tool exists

Tool is allowed for the current agent

Runtime executes tool.

Runtime logs invocation.

Structured result returned to agent.

Tools must never bypass this lifecycle.

4. Global Registry & Agent Allowlists
4.1 Global Tool Registry

All tools are globally registered at application startup.

Tool names must be unique and immutable once published.

Naming rules:

snake_case only

lowercase

no spaces

descriptive but concise

Example:

gmail_read

stock_quote

http_request

Duplicate registration must throw a fatal startup error.

4.2 Agent Allowlist

Each AgentDefinition must define:

{
  "allowed_tools": [
    "gmail_read",
    "stock_quote"
  ]
}

If a tool is not in the agent’s allowlist, execution must fail with:

{
  "status": "error",
  "error_message": "Tool not permitted for this agent."
}

No override mechanisms exist in v1.0.

5. Tool Interface Contract

All tools must implement:

class ITool {
public:
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual nlohmann::json input_schema() const = 0;
    virtual nlohmann::json execute(const nlohmann::json& input) = 0;
    virtual ~ITool() = default;
};

No additional required methods in v1.0.

6. Input Schema Requirements

The input_schema() must:

Follow JSON Schema structure.

Define type: "object".

Define all properties explicitly.

Define required fields.

Disallow undeclared fields when appropriate.

Avoid wildcard or arbitrary object types.

Example:

{
  "type": "object",
  "properties": {
    "ticker": { "type": "string" },
    "quantity": { "type": "integer" }
  },
  "required": ["ticker"],
  "additionalProperties": false
}

Runtime must validate input against schema before execution.

Invalid input must prevent execution.

7. Output Contract (Mandatory)

All tools must return the following structure:

{
  "status": "success" | "error",
  "data": {},
  "error_message": null | "string"
}

Rules:

No exceptions may propagate to runtime.

All failures must return "status": "error".

"data" must always exist (empty object allowed).

"error_message" must be null on success.

Example success:

{
  "status": "success",
  "data": {
    "price": 189.42
  },
  "error_message": null
}

Example error:

{
  "status": "error",
  "data": {},
  "error_message": "Ticker not found."
}
8. Credential Handling

Tools must never store or embed secrets.

If credentials are required:

Input schema must include a credential_id field.

Runtime resolves credentials before execution.

Tools receive resolved credentials via input payload.

Tools must not log credential values.

Tools must not cache credentials internally.

Example schema snippet:

{
  "credential_id": { "type": "string" }
}

Credential storage and encryption are handled by the CredentialStore system (separate specification).

9. Execution Constraints

Inside execute() the following are forbidden:

Infinite loops

Thread spawning

Background workers

Static global state mutation

Direct SQLite access

Direct memory repository access

Tool-to-tool calls

Allowed:

Network calls (if declared in schema)

Pure computation

Stateless API interaction

All execution must be synchronous in v1.0.

10. Observability & Logging

Tool logging is handled by runtime, not by the tool.

For every invocation, runtime must log:

tool_name

agent_id

invocation_id

input_hash (not raw input if sensitive)

execution_time_ms

status

error_message (if any)

Credentials must never be logged.

Logs must not be stored in memory.db.

11. Determinism & Idempotency

Tools should be idempotent when possible.

If not idempotent (e.g., order placement), documentation must clearly state side effects in description().

Non-idempotent tools must not auto-retry in v1.0.

12. Tool Categories (Organizational Only)

Categories are descriptive and do not change behavior.

Retrieval Tools

vector_lookup

grag_lookup

Communication Tools

gmail_read

gmail_send

Financial Tools

stock_quote

place_order

Utility Tools

http_request

datetime_now

math_eval

13. Security Model

Tools are capability boundaries.

Security is enforced through:

Agent allowlists

Input schema validation

Credential indirection

No direct memory/database access

Structured error handling

Centralized runtime logging

No tool may bypass runtime validation.

14. Versioning Policy

This document is versioned.

Future versions may introduce:

Retry policies

Timeout policies

Permission tiers

Async execution

Tool chaining

Sandboxed execution

v1.0 does not include these features.

15. Compliance Requirement

Any tool not conforming to this specification must not be merged into the codebase.

All new tools must reference:

“Implemented according to TOOLS.md v1.0”

in their pull request description.

End of Specification.
