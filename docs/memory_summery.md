# Memory Architecture Summary

This project uses a practical hybrid memory model across conversation state, persisted summaries, execution traces, and retrieval index data.

## 1) Episodic Memory ("what happened")

Episodic memory captures conversation events and response lifecycle outcomes.

- Primary store: `agent_workspace/memory.json`
  - `conversation`: ordered user/assistant turns with timestamps
  - `short_summary` and `extended_summary`: condensed session narrative
- Session timeline store: `agent_workspace/chat_sessions.json`
  - Keeps session-level histories (`id`, `messages`, `created_at_ms`, `updated_at_ms`, `title`)
- Decision/event trace store: `agent_workspace/decision_trace.jsonl`
  - Structured per-request stages (intent, validation, policy, context, generation, post-check)
- Operational log stream: `agent_workspace/app_log.jsonl`
  - Events like `query_generated`, `memory_updated`, `query_complete`

In short: this layer records *what happened over time* for chats and request execution.

## 2) Semantic Memory ("facts/knowledge")

Semantic memory is represented by durable summaries and retrievable indexed knowledge.

- Persistent factual/concept abstraction in `memory.json` summaries:
  - `short_summary` for quick recall
  - `extended_summary` for larger context accumulation
- External/project knowledge is accessed through RAG retrieval, not only chat replay.
- Query-time semantic context is merged into prompts through the prompt-building path (memory + retrieved chunks + current query).

In short: this layer stores and retrieves *generalized knowledge and distilled facts* rather than raw turn-by-turn text only.

## 3) Working Memory ("active context now")

Working memory is the temporary context assembled for the current response.

- Built during request processing in the command pipeline.
- Typical composition:
  - recent conversation window from `Memory`
  - retrieved RAG chunks (if available)
  - current user prompt
- Evidence in traces:
  - `decision_trace.jsonl` context stage reports whether RAG chunks were loaded (or fallback to conversation-only prompt).

In short: this is *short-lived, in-flight context* used to generate the next answer.

## 4) Vector Storage System (RAG index)

The vector storage subsystem provides similarity search for retrieval-augmented generation.

- Index file: `agent_workspace/rag/rag_index.bin`
- Logical flow:
  1. Query embedding generation
  2. Similarity scoring in vector store
  3. Top-K chunk retrieval
  4. Prompt augmentation with retrieved chunks
- Architecture notes (from embedded `basic_agent` docs):
  - `IndexManager` manages chunks and index interactions
  - `VectorStore` supports pluggable similarity metrics (cosine, dot-product, euclidean, jaccard)
  - `RAGPipeline` coordinates retrieval and passes relevant chunks to prompt assembly

In short: vector storage is the project’s *semantic retrieval backbone* for non-conversational knowledge.

## 5) Overall Classification

Yes — the architecture effectively spans all requested memory types:

- **Episodic**: conversation/session/history + decision/log traces
- **Semantic**: summaries + retrievable indexed knowledge via RAG
- **Working memory**: runtime prompt context assembly per request
- **Vector storage**: persisted RAG index (`rag_index.bin`) with similarity retrieval

This gives the system both continuity of dialogue and expandable knowledge retrieval beyond the immediate chat window.
