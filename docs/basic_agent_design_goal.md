# Design Goal: The Basic Agent Core

## 1. Scope Definition
The current priority for Thoth is the **Basic Agent**. A Basic Agent is defined as a cognitive system that operates *without* external tools. Its intelligence is derived entirely from its ability to:
- **Retrieve** relevant context from its own codebase and documentation via RAG/GRAG.
- **Remember** past interactions and goals via Episodic and Semantic memory (SQLite).
- **Reason** through a plan (ExecutiveController) to answer user queries based *only* on the evidence it can retrieve.

## 2. Why "No Tools" (For Now)?
Stabilizing the "Cognitive Spine" is more important than expanding the "Arms" (Tools). If the agent cannot reliably use its own memory or retrieve the correct code chunks to answer a question, giving it the ability to read emails or run shell commands only introduces more points of failure and prompt pollution.

### Current Constraints:
- **Tool Access:** Disabled by default in `Config`.
- **Knowledge Source:** Local file indexing and conversation history only.
- **Success Metric:** High-fidelity answers derived from RAG context, not hallucinations or unauthenticated tool calls.

## 3. Transition to "Extended Agent"
Tools (Gmail, Shell, etc.) are explicitly moved to the **Phase 4: Extended Capabilities** roadmap. They will only be re-enabled once the following "Basic" criteria are met:
1. **Zero-Chunk Handling:** The agent must correctly report when it cannot find information instead of hallucinating.
2. **Context Integrity:** The agent should not "leak" its own internal prompt structure (like tool schemas) into its conversational output.
3. **Resumption Stability:** The `ExecutiveController` must be able to resume any RAG-based plan from the SQLite database without state corruption.

## 4. Immediate Architectural Adjustments
- `Config::enable_tools` should be set to `false` by default in the constructor.
- `PromptFactory` will skip `getToolList()` if `enable_tools` is false, significantly reducing the token overhead and potential for model confusion.
- The `ToolRegistry` remains as an architectural scaffold, but will not be "plumbed" into the primary conversation loop until the Basic Agent is verified stable.
