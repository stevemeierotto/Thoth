# R6 Phase 0 — Evidence Record

**Captured:** 2026-07-24  
**Probe script:** `scripts/r6_evidence_probe.sh`  
**Engine:** `http://127.0.0.1:8090` · version `0.2` · llama context `-c 2048`

## Workdirs

| Run | Path | Mode |
|-----|------|------|
| 0A Ingest | `/tmp/r6-evidence-1784923121` | `ingest` |
| 0B/0C Turns | `/tmp/r6-evidence-1784923480` | `turns` |

Reproduce:

```bash
./scripts/r6_evidence_probe.sh ingest
./scripts/r6_evidence_probe.sh turns
# or full:
./scripts/r6_evidence_probe.sh full
```

---

## 0A — Send to Engine Evidence Chain (R6-02)

**Verdict: HTTP/engine path CONFIRMED working.** GUI E1/E8 not exercised in this probe (requires manual GUI pass).

| Step | ID | Result | Evidence |
|------|-----|--------|----------|
| Engine ready | ready | **PASS** | `/ready` includes `ingest`, `corpus`, `conversation`, `events` |
| POST sent | E2 | **PASS** | `POST /v1/rag/documents` |
| HTTP accept | E3 | **PASS** | HTTP **200**, `status: accepted`, `doc-b9d089b36b5c35ae` |
| session_id bound | E4 | **PASS** | Body included `session_id: r6-ingest-1784923121` |
| Indexing complete | E6/E7 | **PASS** | After 15s: `r6-probe-1784923121.md` **indexed**, 1 chunk; corpus 23→24 |
| Retrieval bind | E9 | **PASS** | Chat HTTP 200 (332s); `CHAT_RAG_CONTEXT` shows `active_context_key=r6-ingest-1784923121`, `selected_documents=["r6-probe-1784923121.md"]`, grounded=true |

### Implications for R6-02

The operator symptom *"Send to Engine does nothing"* is **not explained by a broken HTTP ingest path** on a ready Engine. Confirmed failure layer is **GUI-side or environmental** until E1/E8 are captured:

- GUI click → worker enqueue (E1)
- GUI refresh after accept (E8)
- Button gating (`supportsIngest`, Engine Ready, Local Note present)
- Worker blocked by prior hung turn when operator clicked (R6-03 interaction)

**Classification update:** R6-02 shifts from "likely UX" to **confirmed engine/transport OK; GUI feedback path unverified**.

---

## 0B — Turn Comparison (R6-08)

Session: `r6-turns-1784923480` · prompts: `Reply with exactly: R6T1/R6T2/R6T3` · no session attachments (empty retrieval scope).

| Turn | HTTP | Latency | exec_time_ms | final_prompt_chars | history_chars | RAG chars | raw_answer_chars | finish_reason | Assistant (preview) |
|------|------|---------|--------------|-------------------|---------------|-----------|------------------|---------------|---------------------|
| 1 | 200 | **487s** | 487320 | 365 | 0 | 0 | 1205 | length | `I couldn't generate a reply.` (fallback) |
| 2 | 200 | **251s** | 251251 | 542 (+48%) | 81 | 0 | 1320 | length | Leaked transcript markers |
| 3 | 200 | **54s** | 54861 | 710 (+31%) | 208 | 0 | 145 | stop | Scaffold/markdown leakage |

### R6-08 Analysis

**Prompt growth is modest, not exponential.** `final_prompt_chars` 365 → 542 → 710 (~1.9× turn 1→3). Well below `-c 2048` token budget.

**Turn 3 is not inherently the slowest turn.** In this run Turn 1 took 487s — the longest — despite the smallest prompt.

**Dominant latency driver appears to be model generation behavior, not context size:**

- Turn 1/2: model emitted **1200–1320 raw chars** then hit `finish_reason: length`; sanitizer collapsed to fallback or transcript junk.
- Turn 3: model emitted **145 raw chars**, `finish_reason: stop` → fast completion.

**RAG did not contribute** (no session attachments; `candidates_found=0`, `selected_documents=[]`).

### Relation to prior R4 Verify failure (Turn 3 timeout)

Prior record: Turn 3 HTTP 000 after worker held a **~604s** prior turn.

This probe (fresh worker, no backlog): **all three turns returned HTTP 200**. Turn 3 completed in 54s.

**Conclusion:** R6-05/R6-03 Turn 3 failure is **worker saturation / queue backlog**, not intrinsic Turn 3 slowness or prompt bloat — for short reply probes at least.

---

## 0C — Worker / Diagnostics Snapshot

After turn probe:

- `latest_execution_time_ms`: 54861 (Turn 3)
- `latest_goal`: `query`
- `/ready`: still `ready` with full capabilities

No deadlock observed; worker accepted sequential turns and returned to idle.

---

## Checkpoint Summary

| Probe | Passed | Failed |
|-------|--------|--------|
| 0A Ingest | 4 | 0 |
| 0B Turns | 3 | 0 |
| 0C Diagnostics | 1 | 0 |

---

## Findings → Phase 1 Scope (for approval)

| Finding | Confirmed root cause | Recommended Phase 1 action |
|---------|---------------------|---------------------------|
| **R6-02** | Engine ingest OK; GUI path unverified | Manual GUI E1/E8 capture; fix feedback/gating only if GUI layer fails |
| **R6-03** | Single-worker + slow generation on CPU; queue backlog causes timeouts | Do **not** increase timeouts; address worker/model behavior |
| **R6-08** | Prompt size not primary; generation length/fallback primary | Log `raw_answer_chars` + `finish_reason` in timing framework; tune num_predict/stops |
| **R6-05** | Secondary to worker saturation | Re-run `r4_engine_verify.sh` after Phase 1/3 with fresh Engine |
| **R6-01** | Functional (23 global docs vs session scope) | Phase 5 — dual Session Attachments / Engine Inventory UI |
| **R6-04** | Not probed in Phase 0 | Phase 4 — restore goal to Observation panel |

---

## Artifacts

```
/tmp/r6-evidence-1784923121/
  evidence.json
  summary.txt
  0a-corpus-before.json
  0a-corpus-after.json
  0a-ingest-body.json
  0a-chat-rag-tail.jsonl
  0a-retrieval-chat.json

/tmp/r6-evidence-1784923480/
  evidence.json
  summary.txt
  0b-turn{1,2,3}.json
  0b-turn-metrics.json
  0b-chat-rag-tail.jsonl
  0b-conversation.json
  0c-diagnostics-after-turn{1,2,3}.json
  0c-diagnostics-final.json
```

---

## R6 Closeout — 2026-09-27

The sections above are the 2026-07-24 Phase 0 record. They were not re-run for closeout. Phase 0 left the GUI Send path unverified and did not observe Plan Execution.

**R6 CLOSED 2026-09-27.** The sole remaining current gap after the formal audit was R6-04. It was verified by targeted live GUI observation on PID `105077`, workspace `/tmp/thoth-r6-04-gui`:

| Session | Persisted `active_goal` | Plan Execution |
|---------|-------------------------|----------------|
| `r6-04-goal-20260927` | `R6-04 persisted goal alpha` | `Active Goal: R6-04 persisted goal alpha` · `State: Session (chat retrieval)` · no step rows |
| `r6-04-nogoal-20260927` | empty | `Active Goal: None` · `State: Idle` · headers `Step` and `Status` only |

The operator left the goal session and selected it again before the first reading. Restart strengthening was not performed. The center goal banner was not the pass condition. Deferred CSG-A.4 automatic retrieval was not tested. Other finding dispositions are in [`GUI_RESTORATION_PROTOCOL.md`](GUI_RESTORATION_PROTOCOL.md) § Phase R6 Closeout. G3 remains **VERIFIED**. G2b remains **SUPERSEDED/CLOSED**. This R6 record did not certify ALP1. ALP1 was certified later on 2026-09-27. Timeout Phase B remains separate debt.
