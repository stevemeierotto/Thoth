🧪 Manual Verification Protocol (Post-Concurrency Stabilization)

You are not just testing GRAG.

You are testing:

Thread coordination

Embedding lifecycle

SQLite plan persistence

Logging correctness

Controller restart safety

✅ Test 1 — Basic Goal Activation

Start application.

Run:

/goal improve recommendation system relevance

While plan is active, send a normal query:

What techniques exist for collaborative filtering?

Verify:

grag_benchmark.jsonl shows:

goal_present: true

scoring_type: grag

decision_trace.jsonl shows:

PLAN_CREATED

STEP_STARTED

STEP_COMPLETED (if applicable)

PLAN_COMPLETED or still active

This confirms embedding lifecycle survived mutex integration.

✅ Test 2 — Concurrent Interrupt

This tests thread restart integrity.

Issue a long-running goal (or simulate delay in planner).

Immediately issue another:

/goal build fraud detection pipeline

Expected behavior:

First controller thread stops cleanly.

Embeddings are cleared.

New PLAN_CREATED emitted.

No crash.

No corrupted plan in SQLite.

No mixed GRAG state.

Then issue a normal query and confirm GRAG scoring reflects new goal.

This is the concurrency stress test.

✅ Test 3 — Pause / Resume Visibility

Start a multi-step goal.

Call pause() (via whatever manual trigger you have).

Confirm:

Loop halts.

STATE_CHANGED or equivalent event emitted.

Call resume().

Confirm execution continues without duplication.

This tests memory visibility and flag synchronization.

🔍 What You’re Actually Verifying

After the mutex pass, you want to confirm:

No deadlock under rapid goal switching

No lost embedding state

No partial plan mutation

No stale callback invocation

No double PLAN_COMPLETED events

No inconsistent current_index in logs

If all three tests pass, your controller is now structurally stable.
