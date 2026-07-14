# Plan G — Streaming & Observability

**Status:** ✅ Implemented 2026-07-13 (G1–G6) · 🔒 Locked spec — 2026-07-13  
**Prerequisites:** [Plan F — Engine Runtime & HTTP](plan_f_engine_runtime_http.md) ✅  
**Next:** [Plan I — Docker Compose v1](plan_i_docker_compose_v1.md) ✅  
**Roadmap:** [docker_roadmap.md](docker_roadmap.md) Step 4

---

## Goal

Add an **observability transport** on top of `EngineRuntime`: executive lifecycle events flow to HTTP clients via SSE **without affecting command execution, planning, or controller state**.

### Event pipeline

```
                 Engine Worker Thread
                         |
                         |
                  ControllerEvent
                         |
                         v
                    EventBus
                         |
              +----------+----------+
              |                     |
              v                     v
       Internal subscribers     SSE bridge
                                      |
                                      v
                              Client queues
                                      |
                                      v
                               HTTP clients
```

**Critical design rule:**

- **`EngineRuntime` owns event production.**
- **SSE layer owns client delivery.**

The worker thread emits `ControllerEvent` into the bus path via **non-blocking enqueue only**; an `EventDispatchThread` assigns `sequence` / `EngineEvent` envelopes and invokes `EventBus` fan-out (see invariants). The worker never reaches client queues or HTTP.

| Diagram node | Owner | Implementation |
|--------------|-------|----------------|
| Engine Worker Thread | `EngineRuntime` | Command worker; `plugin->onEvent` enqueue |
| EventBus | `EngineRuntime` | Dispatch-thread fan-out; internal subscribers |
| Internal subscribers | `EngineRuntime` | `subscribeEvents()` taps, tests, future in-process sinks |
| SSE bridge | `EngineHttpTransport` | `EventBus` subscriber; forwards to session manager |
| Client queues | `EngineHttpTransport` | `SseSessionManager` bounded per-client buffers |
| HTTP clients | External | `GET /v1/events` consumers |

SSE is deceptively deep — connection management, disconnect handling, buffering, and concurrency deserve their own plan separate from Plan F request/response HTTP.

---

## Non-Negotiable Runtime Invariants

These invariants are **locked requirements** for Plan G. Any implementation that violates them is out of spec.

1. **Event publishing must never block `EngineRuntime` execution.**
   - `plugin->onEvent` on the command worker may only enqueue a copy of the event and return.
   - Enqueue must be O(1) bounded work; no waiting on subscribers, network, or serialization.

2. **Engine worker threads must never perform:**
   - network writes
   - SSE serialization
   - client lifecycle management

3. **Subscriber failures must be isolated:**
   - one bad client cannot affect other clients
   - one handler failure cannot affect event publication

4. **Event delivery is observational only:**
   - events do not modify controller state
   - events do not change planning behavior
   - `ControllerEvent` semantics remain unchanged

### Derived design rules

| Invariant | Enforcement |
|-----------|-------------|
| Non-blocking publish | Bounded ingress queue; worker drops or coalesces only if queue full (policy: drop + log at lock; never block worker) |
| No network on worker | All httplib writes and SSE framing live in `SseSessionManager` on HTTP/dispatch paths only |
| Failure isolation | Per-subscriber `try/catch`; slow SSE clients dropped via bounded per-client queue overflow |
| Observational only | No subscriber may call `pause` / `resume` / `abort` / `submitGoal` in response to an event; bus is read-only export |

---

## V1 Event Stream Guarantees

These are the **explicit delivery promises** for Plan G v1. Client implementations and `docs/ENGINE_EVENTS.md` must document both supported and not-supported items.

### Supported

- **Live event delivery** — subscribers connected at emit time receive events as they are produced
- **Multiple concurrent clients** — independent SSE sessions; fan-out does not affect engine execution
- **Ordered events per process** — `sequence` / `event_id` strictly monotonic within a single `thoth-engine` process
- **Disconnect cleanup** — dropped clients are unsubscribed; buffers released; no leak or worker block

### Not supported

- **Event replay** — no server-side history; `Last-Event-ID` does not trigger backfill in v1
- **Durable event history** — events are not persisted to disk or database
- **Cross-process event ordering** — no ordering guarantee across multiple engine instances or restarts
- **Guaranteed delivery after disconnect** — clients that reconnect join the live stream only; gaps are detectable via `sequence` but not filled
- **Event persistence** — events exist only in memory for the duration of fan-out / per-client buffering

**Client implication:** treat `/v1/events` as a **live telemetry feed**, not a durable log. For audit trails, use `decision_trace.jsonl` or other existing persistence mechanisms.

---

## Boundary

### In scope
- `EventBus` + dedicated **dispatch thread** on `EngineRuntime`
- `EngineEvent` wire envelope (`event_id`, `sequence`, `timestamp`, mapped `ControllerEvent` fields)
- `subscribeEvents()` / `unsubscribeEvents()` on `EngineRuntime`
- Wire `BasicAgentPlugin::onEvent` → ingress queue (G2)
- `GET /v1/events` — Server-Sent Events (G3+)
- `SseSessionManager` — per-connection queues and HTTP writes (G3+)
- Multiple concurrent SSE clients (G4)
- Client disconnect handling with no engine impact (G5)
- Graceful shutdown extended for open SSE connections (G6)
- `/ready` capabilities adds `"events"` when SSE is live (G3+)
- `docs/ENGINE_EVENTS.md` — event catalog and client guidance (G6)
- Tests: dispatch non-blocking, multi-client fan-out, disconnect cleanup, shutdown with active SSE

### Explicitly excluded
- WebSocket transport
- Docker / compose (**Plan I**)
- Inference adapter (**Plan H**) — see [plan_h_inference_adapter.md](plan_h_inference_adapter.md) 🔒
- GUI HTTP client (**Plan K**)
- Authentication on `/v1/events` (v1)
- Changing `ControllerEvent` or `ExecutiveController` semantics
- New `EventType` values (map existing types only)
- **F+1** `submitGoalAsync()` — separate backlog in [`improvements.md`](improvements.md); consumes G’s stream but is not a G checkpoint
- Event replay / ring buffer (deferred — see § Deferred)
- Diagnostics metrics (event rate, subscriber count) — post-G backlog unless needed for a test

---

## Ownership model

Aligns with **§ Event pipeline** — production vs delivery split:

```
thoth-engine
    └── EngineRuntime                    ← owns event production
            ├── BasicAgentPlugin
            │       └── onEvent → ingress enqueue ONLY (command worker)
            ├── command worker thread          (Plan F — unchanged)
            ├── EventDispatchThread            (NEW)
            │       └── EventBus
            │               ├── internal subscribers (subscribeEvents, tests)
            │               └── SSE bridge subscriber (registered by transport)
            └── capabilities() includes "events" when G3+

EngineHttpTransport                      ← owns client delivery
    └── SseSessionManager
            ├── client queues (bounded per connection)
            ├── httplib SSE writes
            └── never called from command worker
```

**Rules:**
- `EngineRuntime` owns event production; `EngineHttpTransport` owns client delivery.
- HTTP transport registers the SSE bridge on `EventBus`; it does not own the bus or dispatch thread.

## Event envelope

`EngineEvent` wraps `ControllerEvent` for the wire. Mapping is owned by the dispatch thread at envelope creation time.

```json
{
  "event_id": 391,
  "sequence": 391,
  "timestamp": "2026-07-13T20:15:31Z",
  "type": "PLAN_CREATED",
  "session_id": "default",
  "plan_id": "...",
  "step_id": "",
  "controller_state_name": "PLANNING",
  "metadata": {}
}
```

| Field | Rule |
|-------|------|
| `event_id` | Monotonic per process; equals `sequence` in v1 |
| `sequence` | Strictly increasing per process; assigned on **dispatch thread** at dequeue time (`std::atomic<uint64_t>`). Defines **v1 ordering guarantee** (single process only). |
| `timestamp` | ISO-8601 UTC; prefer `ControllerEvent::timestamp_ms` when non-zero, else dispatch-time UTC |
| `type` | `EventType` enum string |
| `metadata` | Copied verbatim from `ControllerEvent::metadata` |

`ControllerEvent` fields are **not** modified during mapping.

---

## `EngineRuntime` additions

```cpp
uint64_t subscribeEvents(std::function<void(const EngineEvent&)> handler);
void unsubscribeEvents(uint64_t subscription_id);
```

**Contracts:**
- Handlers are invoked on the **dispatch thread** (documented).
- Handlers must be non-blocking (invariant 1).
- Handler exceptions are caught, logged, and do not affect other subscribers (invariant 3).
- `subscribe` / `unsubscribe` are thread-safe concurrent with dispatch.

**Plugin wiring (G2):**

```cpp
plugin->onEvent = [runtime](const ControllerEvent& ev) {
    runtime->enqueueControllerEvent(ev);  // copy + return; never blocks
};
```

---

## `GET /v1/events`

Server-Sent Events:

```
event: engine
id: 391
data: {"event_id":391,"sequence":391,...}

```

### v1 policies (lock at G3)

See also **§ V1 Event Stream Guarantees**.

| Concern | Policy |
|---------|--------|
| Multiple clients | Supported; independent `SseSession` per connection (G4) |
| Client disconnect | Failed `write` → unsubscribe + release buffers silently (**disconnect cleanup** guarantee) |
| Backpressure | Bounded per-client queue (default **256** events); overflow → **drop client** + log (invariant 3) |
| Reconnect | `Last-Event-ID` header accepted; **no replay** — live stream only (**not supported** guarantee) |
| Ordering | Monotonic `sequence` per process only; no cross-process or post-restart ordering |
| Durability | None — in-memory fan-out only; not a persistent log |
| Engine execution | Unaffected by subscriber count (invariant 1; verified in G4) |
| During shutdown | New SSE connections → `503 ENGINE_BUSY`; existing sessions closed after bounded flush (G6) |

All SSE serialization and network writes occur in `SseSessionManager` — never on the command worker (invariant 2).

---

## Graceful shutdown (extends Plan F)

Shutdown order is **load-bearing**. Plan F steps are integrated, not replaced.

1. `beginShutdown()` — not ready; reject new HTTP/API work (`503`)
2. Stop accepting **new** connections (including `/v1/events`)
3. Drain **command queue** (Plan F bounded wait, default 5 s)
4. Tear down **plugin** (no further `onEvent` emissions)
5. Drain **event ingress + dispatch queues** (bounded, e.g. 2 s)
6. Flush / close **open SSE sessions** (bounded, e.g. 2 s)
7. Stop **dispatch thread**; unsubscribe all `EventBus` handlers
8. Close HTTP listener (Plan F)

SSE teardown happens **after** plugin destruction so no new events arrive, but **before** the HTTP listener fully exits.

---

## API update

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/v1/events` | SSE stream of `EngineEvent` JSON |

### `GET /ready` (updated at G3)

```json
{
  "status": "ready",
  "workspace": "/data/agent_workspace",
  "capabilities": ["chat", "goals", "control", "events"]
}
```

`capabilities` describes **engine features** (`EngineRuntime::capabilities()`), not “routes wired this checkpoint.”

---

## Event documentation (G6)

Add `docs/ENGINE_EVENTS.md`:

- **V1 Event Stream Guarantees** (supported vs not supported) — required client-facing section
- List of `EventType` values and when they fire
- Example SSE session during a goal run (`curl -N`)
- Reconnect guidance: use `sequence` for gap detection; no server-side replay; no guaranteed delivery after disconnect
- Explicit note: events are observational; clients must not infer control side-effects
- Pointer to `decision_trace.jsonl` for durable audit needs

Update `GETTING_STARTED.md` with `/v1/events` summary and link.

---

## Checkpoints

Plan G uses the same **stop-gate discipline** as Plan F: implement one checkpoint, verify, **STOP**, then wait for explicit approval before the next.

### Gate rule (mandatory)

**After each checkpoint: STOP. Review implementation against [Non-Negotiable Runtime Invariants](#non-negotiable-runtime-invariants) and [V1 Event Stream Guarantees](#v1-event-stream-guarantees) before proceeding.**

At each stop, confirm:

1. Invariants 1–4 still hold (no blocking publish, no network on worker, failure isolation, observational-only delivery).
2. Production vs delivery ownership is preserved (`EngineRuntime` vs SSE layer).
3. Verification criteria for the checkpoint are met.
4. No scope from “intentionally NOT included” crept in.

G6 is the **final implementation stop** before Plan G implementation is considered complete.

### Checkpoint summary

| Checkpoint | Purpose | Lock point |
|------------|---------|------------|
| **G1** | EventBus + envelope | Internal event contract approved |
| **G2** | Engine integration | Controller events successfully exported |
| **G3** | SSE single client | External API works |
| **G4** | Multi-client fan-out | Isolation proven |
| **G5** | Disconnect cleanup | Resource safety proven |
| **G6** | Docs + shutdown | Feature complete |

### Checkpoint detail

| CP | Scope | Intentionally NOT included | Verify |
|----|-------|---------------------------|--------|
| **G1** | `EngineEvent` mapping; `EventBus`; dispatch thread; bounded ingress queue; sequence assignment; subscriber isolation | Plugin wiring, HTTP, SSE, shutdown | Unit test: 1000 enqueues from a stand-in worker complete without blocking; sequences strictly increasing; throwing subscriber does not affect others or publication |
| **G2** | Wire `plugin->onEvent` → ingress queue; `subscribeEvents` / `unsubscribeEvents`; internal test tap | SSE, HTTP routes, F+1, `ControllerEvent` changes | Tap receives `ControllerEvent` exports during goal/chat; worker remains responsive under concurrent `submitChat` |
| **G3** | `GET /v1/events` single client; SSE bridge + `SseSessionManager`; `/ready` adds `"events"` | Multi-client stress, disconnect tests, shutdown, replay | `curl -N` during `POST /v1/goals` shows lifecycle events with valid `sequence` / `timestamp` |
| **G4** | 3+ concurrent SSE clients; engine latency guard | Replay, auth, F+1 | 3 clients receive identical sequences; goal/chat duration with 3 clients within tolerance of 0-client baseline (e.g. ±10%) |
| **G5** | Disconnect detection; unsubscribe; client queue release; no subscriber leak | Shutdown, replay | Kill client mid-stream 10×; subscriber count returns to baseline; no hang |
| **G6** | Shutdown with open SSE; `ENGINE_EVENTS.md`; `GETTING_STARTED` update; full PR suite | Replay buffer, F+1, diagnostics | `timeout -s TERM` with active `curl -N` → clean exit; `ctest -L pr -j1` green |

### Verification commands

Assume `engine-only` preset.

**G1 — EventBus + envelope**

```bash
THOTH_ENGINE_RUNTIME_TESTS=1 ./build/engine-only/tests/thoth-core-tests
# (G1 tests added to engine runtime suite or dedicated filter)
```

**G3 — Single-client SSE**

```bash
./build/engine-only/external/basic_agent/thoth-engine --serve --port 8090 &
curl -N -H 'Accept: text/event-stream' http://127.0.0.1:8090/v1/events &
curl -s -X POST http://127.0.0.1:8090/v1/goals \
  -H 'Content-Type: application/json' \
  -d '{"goal":"List files in agent_workspace","session_id":"default"}'
```

**G4 — Multi-client**

```bash
# 3× curl -N in parallel; compare goal completion time vs no subscribers
```

**G6 — Shutdown + regression**

```bash
timeout -s TERM 5 ./build/engine-only/external/basic_agent/thoth-engine --serve --port 8090
ctest --test-dir build/engine-only -L pr -j1 --output-on-failure
```

### Plan G testing note

cpp-httplib does not provide reliable client-side validation of chunked SSE response bodies.

SSE correctness is validated through:

- `SseSessionManager` unit/integration tests
- subscription lifecycle tests
- multi-client fanout tests
- HTTP route smoke validation

Future HTTP streaming tests should use a client capable of consuming long-lived chunked responses.

---

## Success criteria

- [ ] Non-negotiable invariants 1–4 enforced by construction and tested
- [ ] V1 Event Stream Guarantees documented in `ENGINE_EVENTS.md` and honored by implementation
- [ ] `GET /v1/events` streams `EngineEvent` JSON over SSE (live delivery only)
- [ ] Events include `event_id`, `sequence`, `timestamp`
- [ ] Multiple concurrent SSE clients without affecting engine execution
- [ ] Client disconnect does not crash, block worker, or leak subscribers
- [ ] Graceful shutdown closes SSE cleanly (ordered teardown)
- [ ] `/ready` lists `"events"` capability
- [ ] `docs/ENGINE_EVENTS.md` published
- [ ] `ctest -L pr` green

---

## Deferred

| Item | Where | Why deferred |
|------|-------|--------------|
| **F+1** `submitGoalAsync()` | [`improvements.md`](improvements.md) backlog | API semantics change; needs G’s stream but out of G transport scope |
| Replay / ring buffer | Plan G+ backlog | **Not supported** in v1 guarantees |
| `Last-Event-ID` replay | Plan G+ backlog | Header accepted; no backfill per v1 guarantees |
| Durable event history / persistence | Plan G+ backlog | **Not supported** in v1; use `decision_trace.jsonl` for audit |
| Diagnostics (event rate, subscriber count) | Post-G ops backlog | Not required for SSE MVP |
| WebSocket | Future | SSE satisfies v1 |
| Auth on event stream | v1 non-goal | Matches Plan F |
| GUI client | Plan K | Consumes `/v1/events` |
| Docker / compose | Plan I | Packaging only |
| Inference adapter | [Plan H](plan_h_inference_adapter.md) 🔒 | Unrelated to event delivery |

---

## Rollback

Remove `EventBus`, dispatch thread, SSE route, and event docs. Plan F HTTP API remains fully functional without streaming.

---

## Plan history

| Date | Change |
|------|--------|
| 2026-07-13 | Draft revision: non-negotiable invariants, dispatch-thread architecture, checkpoint stop gates, v1 backpressure/replay policies, shutdown ordering |
| 2026-07-13 | Added V1 Event Stream Guarantees (supported vs not supported) |
| 2026-07-13 | Added event pipeline diagram; production vs delivery ownership rule |
| 2026-07-13 | Checkpoint summary (purpose / lock point); mandatory STOP + invariant review gate |
| **Locked** | 2026-07-13 — invariants, v1 stream guarantees, production/delivery split, checkpoints G1–G6 with stop gates |
| **Implemented** | 2026-07-13 — G1–G6 complete; `ctest -L pr -j1` green |

---

## Plan lock record

| Field | Value |
|-------|-------|
| **Locked** | 2026-07-13 |
| **Invariants (locked)** | Non-blocking publish; no network on worker; subscriber failure isolation; observational-only delivery |
| **V1 guarantees (locked)** | Live delivery, multi-client, per-process ordering, disconnect cleanup — no replay, persistence, or cross-process ordering |
| **Ownership (locked)** | `EngineRuntime` owns event production; SSE layer owns client delivery |
| **Backpressure (locked)** | Bounded per-client queue (default 256); overflow drops client |
| **Reconnect (locked)** | `Last-Event-ID` accepted; no replay in v1 |
| **Deferred (backlog)** | **F+1** `submitGoalAsync()`, replay ring buffer, durable history — see § Deferred |
| **Scope** | `EventBus`, dispatch thread, `GET /v1/events`, multi-client SSE, shutdown extension, `ENGINE_EVENTS.md` |
| **Implementation** | ✅ G1–G6 complete (2026-07-13); see [`completed_improvements_log.md`](completed_improvements_log.md) |
| **Post-lock changes** | Protocol lock rule — stop and request approval before editing this spec |

---

STATUS: IMPLEMENTED (G1–G6, 2026-07-13)
