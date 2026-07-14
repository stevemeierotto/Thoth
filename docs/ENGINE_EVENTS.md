# Engine Events — SSE Observability (Plan G)

**Status:** Plan G v1  
**Endpoint:** `GET /v1/events` (Server-Sent Events)  
**Spec:** [`plan_g_streaming_observability.md`](plan_g_streaming_observability.md) 🔒

---

## V1 Event Stream Guarantees

### Supported

- Live event delivery
- Multiple concurrent clients
- Ordered events per process (`sequence` / `event_id` monotonic within one `thoth-engine` process)
- Disconnect cleanup

### Not supported

- Event replay (`Last-Event-ID` is accepted but does not backfill history)
- Durable event history
- Cross-process event ordering
- Guaranteed delivery after disconnect
- Event persistence

Treat `/v1/events` as a **live telemetry feed**, not a durable log. For audit trails, use `logs/decision_trace.jsonl`.

---

## Wire format

Each SSE message:

```
event: engine
id: 42
data: {"event_id":42,"sequence":42,"timestamp":"2026-07-13T20:15:31Z",...}

```

### `EngineEvent` JSON fields

| Field | Description |
|-------|-------------|
| `event_id` | Monotonic per process (equals `sequence` in v1) |
| `sequence` | Strictly increasing per process |
| `timestamp` | ISO-8601 UTC |
| `type` | `EventType` string |
| `session_id` | UI / engine session |
| `plan_id` | Plan correlation id |
| `step_id` | Step id when applicable |
| `controller_state_name` | Executive state name |
| `metadata` | JSON payload from `ControllerEvent` |

---

## `EventType` values

| Type | Typical trigger |
|------|-----------------|
| `PLAN_CREATED` | Initial plan created |
| `STEP_STARTED` | Step execution begins |
| `STEP_COMPLETED` | Step succeeds |
| `STEP_FAILED` | Step fails |
| `STEP_RETRYING` | Step retry scheduled |
| `PLAN_REVISED` | Plan repaired after failure |
| `PLAN_COMPLETED` | Goal completes successfully |
| `PLAN_ABORTED` | User/system abort |
| `PLAN_FAILED` | Goal fails |
| `STATE_CHANGED` | Executive state transition |
| `MODE_SWITCHED` | Execution mode change |
| `EMBEDDING_FAILED` | Embedding backend failure |
| `RETRIEVAL_DIAGNOSTICS` | GRAG retrieval diagnostics |
| `INDEXING_STARTED` | RAG indexing started |
| `INDEXING_COMPLETED` | RAG indexing finished |
| `PLAN_REUSE_INJECTION` | Plan history hint injected |
| `REFLECTION_REPLAN` | Reflection loop replan |
| `PLAN_HISTORY_STORED` | Successful plan stored for reuse |

Events are **observational only** — clients must not infer permission to mutate controller state from the stream.

---

## Example session

```bash
./build/engine-only/external/basic_agent/thoth-engine --serve --port 8090

# Terminal 1 — live stream
curl -N -H 'Accept: text/event-stream' http://127.0.0.1:8090/v1/events

# Terminal 2 — start a goal
curl -s -X POST http://127.0.0.1:8090/v1/goals \
  -H 'Content-Type: application/json' \
  -d '{"goal":"List files in agent_workspace","session_id":"default"}'
```

You should see lifecycle events (`PLAN_CREATED`, `STATE_CHANGED`, `STEP_STARTED`, …) in Terminal 1.

---

## Reconnect guidance

- On reconnect, the client joins the **live** stream only.
- Use `sequence` to detect gaps; the server does not replay missed events in v1.
- `Last-Event-ID` may be sent; it is not used for backfill in v1.

---

## Readiness

When SSE is available, `GET /ready` includes `"events"` in `capabilities` alongside `chat`, `goals`, and `control`.
