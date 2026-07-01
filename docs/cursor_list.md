# Thoth Working Backlog

**Last updated:** 2026-06-29 (M2 complete; reflection & external review)  
**Purpose:** Active todo list for the next development sessions. Specs live in `improvements.md`; finished work is logged in `completed_improvements_log.md`.

**Baseline locked:** Headless cognitive loop verified — `run_test_suite` **TC-01–TC-07 all pass** (2026-06-27) with real `executeLLM`, RETRIEVAL→LLM plans, and GRAG scoring. Prior P0–P2 alignment (2026-06-17) in `completed_improvements_log.md`.

---

## External review — documentation honesty (2026-06)

Independent review noted that Thoth docs are **unusually candid** about limits — a strength, not a gap:

| Topic | How docs handle it |
|-------|-------------------|
| **Mock Cognate benchmarks** | `benchmark_results.md` / `COGNATE_V2.md` report **0.00\*** task success under mock setup; metrics framed as process signals, not end-to-end completion |
| **Trajectory weight ($w_t$)** | Mixed lift on trajectory-disambiguation cases documented (`plan_reuse_tuning.md`, `benchmark_results.md`) |
| **Hierarchical subgoals** | Explicitly **not implemented** (`improvements.md` Step 4.4, `audit.md`, `GRAG.md` planned) |
| **Scientific mode “51× depth”** | Footnoted as **iteration count under mock conditions**, not real task completion (`COGNATE_V2.md`, `benchmark_results.md`) |

**Maintain this standard:** new benchmarks and thesis-facing claims must label environment (mock vs Ollama vs GUI), distinguish iteration depth from task success, and log partial features in `audit.md` + `completed_improvements_log.md` before narrative docs.

---

## External review — staleness & beyond backlog (2026-06-29)

Second pass: docs are **current** (dated through 2026-06-29). No meaningful code/doc drift yet. **Watch externally:** Ollama model versions, `nomic-embed-text`, and local LLM choice — these can change benchmark reproducibility without any Thoth commit.

### Gaps gestured at but not fully developed

| Priority | Gap | Expert view | Maps to |
|----------|-----|-------------|---------|
| **High** | **Longitudinal system eval** — per-run metrics exist (`C6`, `logs/cognitive_metrics.jsonl`) but no time-series view across many runs to answer “is the system improving across sessions?” (core thesis: strategy promotion + consolidation → behavioral lift) | Extend `summarize_cognitive_metrics.py` / new script: trend lines over weeks; segment by strategy-injected vs not, pre/post consolidation | **C6 Phase 3** (proposed), thesis evidence |
| **High** | **Strategy impact measurement** — SCR defined in `COGNATE_V2.md` but not wired into a **regular automated benchmark**; promoted strategies should show detectable plan-structure change continuously, not only in one cold/warm paper run | Add SCR (or proxy) to CI/nightly harness; log to JSONL alongside reflection A/B | **C3/C6 extension**, `StrategyEngine` |
| **Medium** | **Trajectory signal diagnostic (G1)** — mixed/negative lift on some buckets; understand **why** (noisy T, weight too high, short-trajectory construction) **before** F5 semantic embeddings | Targeted benchmark on `TRAJECTORY_DISAMBIGUATES` bucket only; tune / redesign / drop based on evidence | **G1** (active), blocks **F5** |
| **Medium** | **M3/M4 operational gap** — without restore/replay, episodic memory is **write-only** for operators; long-running accumulation needs inspect + range replay | Finish `/prune` admin + `restore(session, range)` before claiming long-term episodic ops | **M3**, **M4** |
| **Medium** | **B1 hardened corpus** — 30 research cases listed, not done; strengthens future benchmark claims and **V3** Zenodo re-upload | Expand hardened case set beyond 100-case research corpus baseline | **B1**, **V3** |
| **Horizon** | **Post M3/M4 quality ceiling** — context management + memory hygiene alone won't raise planning/episodic quality further | **F3** (richer episodic memory) + **F1** (planning heuristics) highest leverage after memory ops close | **F3**, **F1** (§8) |

**Suggested order (expert-aligned, not yet scheduled):** M3/M4 → G1 trajectory diagnostic → C6 time-series + SCR harness → B1 → promote F3/F1 when eval shows where lift is blocked.

---

## Reflection & analysis (2026-06-29)

**Context:** Tier 0 (C1–C7) and memory consolidation (M1–M2) are shipped. Headless loop is verified; docs are candid. This section consolidates **external review** (above) with **implementation-grounded analysis** — what to do next and why.

### Where the system actually is

| Layer | Status | What it proves |
|-------|--------|----------------|
| **Wiring** | ✅ | RETRIEVAL→LLM goals complete; tools, reflection, scientific mode operational |
| **Component quality** | ✅/🔶 | C1/C2 hardened context + retrieval; G1 trajectory signal mixed on some buckets |
| **Memory pipeline** | ✅ | Hot→warm→cold consolidation + age policy; M1.5 verified retrieval path |
| **Learning thesis** | 🔶 **Unproven longitudinally** | Strategy promotion + episodic memory **exist**; cross-session behavioral lift **not yet measured** |

The gap is not “missing features” — it is **missing eval that connects features to outcomes over time**.

### Three eval layers (current vs needed)

```
Component harnesses (C3–C5)     →  “Does this mechanism fire correctly?”     ✅
Per-goal metrics (C6)           →  “How did this run perform?”               ✅
Longitudinal / learning eval    →  “Is the system improving across runs?”    ❌ (proposed C6 Phase 3)
```

M1.5 proved the **consolidation pipe** (Apollo E2E). It did **not** prove that consolidated memory improves **later goal success** — that needs **E2** (repeat-goal harness).

### Expert recommendations — agreed priorities

1. **Longitudinal metrics (C6 Phase 3)** — Highest leverage for thesis honesty. Instrumentation exists; analysis is per-run only.
2. **SCR / strategy-impact harness (E3)** — Strategy Engine is real; impact is trace-visible but not regression-tested.
3. **G1 diagnostic before F5** — Negative lift on trajectory-disambiguation bucket → ablate before embedding more into T.
4. **M3/M4 before “long-term memory” claims** — Warm tier is readable via GRAG but **operators cannot replay** archived episodes; write-only for ops.
5. **B1 before V3 Zenodo** — Hardened corpus defends external claims.

### Additional insights (not on prior lists)

| ID | Insight | Rationale |
|----|---------|-----------|
| **E1** | **Benchmark environment pinning** | Checkpoints **A–C ✅**, **D1–D5 ✅**. Spec: **`docs/benchmark_environment.md`**. Next: **E** (close-out). |
| **E2** | **Episodic learning eval** | M1.5 = pipeline correctness. Need harness: same goal class pre/post consolidation, or multi-session repeat task, measuring success/latency/plan reuse when warm memory should help. Directly tests thesis. |
| **Doc** | **Sync `COGNATE_V2.html`** | Markdown has mock footnote for 51× depth; HTML export may not — align before any thesis-facing export. |
| **Discipline** | **Mock vs Ollama split** | Fast CI (mock/TfIdf) must never be the sole evidence for learning or retrieval claims. Nightly `--full` + pinned env = authoritative tier (already in C4; enforce in eval culture). |
| **F3 vs M4** | **F3 overlaps M4** | “Richer episodic memory” includes restore/replay — **M4 is prerequisite**, not parallel optional work. |
| **F1 timing** | **F1 after eval, not before** | Planning heuristics need longitudinal + episodic eval to show *where* plans fail (retrieval miss vs decomposition vs tool choice). Otherwise F1 is guesswork. |

### Nuance on expert “F3 + F1 after M3/M4”

Agree on direction; adjust sequencing:

- **M3/M4** — operational completeness (inspect, manual prune, range restore).
- **G1 + E1** — cheap diagnostics; run in parallel with M3/M4 if capacity allows.
- **C6 Phase 3 + E3 + E2** — eval layer that validates (or falsifies) learning claims.
- **B1** — when preparing V3 or external publication.
- **F3/F1** — only after eval identifies the binding constraint; F3 extends what M4 restores, F1 fixes planner quality ceiling.

### Consolidated roadmap (reflection snapshot)

```
Now      E1 Checkpoint E              (identity pass, scripts, close-out)
Next     G1 diagnostic + E2          (trajectory ablation; episodic learning eval)
Then     C6 Phase 3 + E3             (longitudinal metrics; SCR harness)
Parallel B1 (if V3 Zenodo)           (hardened corpus)
Later    M3 → M4                     (memory ops: prune admin + restore)
Later    F3 / F1                     (promote when eval shows bottleneck)
Last     UI polish (§6), S1 apply_diff (owner discretion)
```

### What not to do yet

- **F5** (semantic trajectory embeddings) before G1 diagnostic completes.
- **Zenodo V3** before B1 + pinned-env benchmark runs.
- **F-series bulk promotion** — horizon items; eval should drive which F moves first.
- **NODE / self-building** — defer per existing backlog discipline.

---

## How to use

1. Work **top to bottom** — finish internals before UI polish or self-building.
2. After code changes: `cmake --build --preset build-debug` and `ctest --output-on-failure`.
3. Mark items done only when implemented **and** logged in `completed_improvements_log.md`.
4. When docs conflict: **code** → `audit.md` + `completed_improvements_log.md` → specs (`GRAG.md`, `PLAN.md`, …) → `improvements.md` → narrative (`README.md`, Zenodo papers).

---

## What's solid (do not re-implement)

| Area | Status |
|------|--------|
| GRAG directional core + graph learning | ✅ |
| Executive loop, reflection, scientific mode, strategy promotion | ✅ |
| `executeLLM` (real synthesis, not stub) | ✅ 2026-06-27 |
| Headless `run_test_suite` TC-01–TC-07 | ✅ full ~40 min (2026-06-27); **dev ~10s** (2026-06-26) |
| Dev cognitive harnesses (reflection A/B, robustness suite) | ✅ 2026-06-28 |
| Developer & CI latency (**C4**) | ✅ dev tier + CI tiers + `ctest -L fast` |
| Runtime latency (**C7**) | ✅ batch embed, synthesis cap, parallel RETRIEVAL + prefetch |
| Robustness & failure tests (**C5**) | ✅ `run_robustness_suite` 10/10 |
| Reflection A/B measurement (**C3**) | ✅ 2026-06-26 |
| Production plan templates (RETRIEVAL → LLM) + plan-reuse strip | ✅ 2026-06-27 |
| Plan history reuse + observability events | ✅ |
| Chat RAG pipeline (observability + 5/5 benchmark + grounded Q&A) | ✅ 2026-06-27 |
| Per-goal cognitive metrics (`logs/cognitive_metrics.jsonl`) | ✅ 2026-06-27 |
| C1 planner context management (budgets, validator, memory hygiene) | ✅ 2026-06-27 |
| `allow_shell_exec` tool gating | ✅ |
| Memory consolidation (M2 age-based policy) | ✅ 2026-06-29 |
| Trajectory $w_t$ in scoring path | 🔶 config 0.2; mixed lift on trajectory-disambiguation cases |
| Unit tests | ✅ full suite green (~78s, 2026-06-16) |
| Doc alignment P2.1–P2.6 | ✅ |

---

## Active backlog

### 0. Cognitive loop hardening — **work now**

End-to-end goal execution works; next focus is **quality, speed, and evidence** — not wiring.

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **C1** | **Improve planning quality** | ✅ | Phases 1–5 shipped — see **§ C1**. Goal execution only; chat path separate (**C2**). |
| **C2** | **Improve retrieval ranking** | ✅ | Phase 0–3 complete — see **§ C2**. |
| **C3** | **Measure reflection outcomes** | ✅ | Headless A/B harness — see **§ C3**. `max_reflections` 0 vs 2; timeout skip; `logs/reflection_ab_benchmark.jsonl`. |
| **C4** | **Developer & CI latency** | ✅ | Dev tier + CI wiring — see **§ C4**. PR: `ctest -L pr`; dev: `ctest -L fast` 3/3 (~70s). Nightly: `test-suite-full` with Ollama. |
| **C5** | **Robustness & failure tests** | ✅ | `run_robustness_suite` — 10 mock cases; `logs/robustness_suite.jsonl`. See **§ C5**. |
| **C6** | **Cognitive metrics** | ✅ | Phase 1–2 complete — logging, summarize + plot scripts, token counts, GUI export. See **§ C6**. |
| **C7** | **Runtime latency** | ✅ | Phase 1–3 complete — batch embeddings, synthesis cap, parallel RETRIEVAL + prefetch. See **§ C7**. |

**Dependencies:** C3 benefits from C1/C2 (reflection currently retriggers on failed LLM/timeouts, not just bad answers). **C4** and **C7** are independent — do not mix mock-fast-CI work with runtime optimization. **C6** should start early (append-only logging) and deepen as C1–C7 land — metrics make every subsequent tuning iteration measurable.

#### C1 — Planning quality (context management)

Planning quality is constrained by **context assembly**, not the planner template or execution engine (headless TEST_SUITE proves the scaffold). Expert-reviewed implementation order:

| Phase | Task | Status |
|-------|------|--------|
| **1** | **Structured prompt assembly** — Rules → Schema → Goal → optional experience. Core sections never tail-truncated; experience competes for remainder. Memory budgets: rules 4 KB, schema 2 KB, goal 1 KB, plan reuse 1 KB, strategy 512 B, trajectory 512 B. | ✅ |
| **2** | **PlanValidator** — Separate from JSON parser. Pipeline: LLM → Parser → Validator → Execute. Reject invalid semantics (TOOL steps, missing RETRIEVAL/LLM, wrong order). **Limited repair:** wire missing `depends_on` only. **No semantic repair** (never insert steps or convert TOOL→RETRIEVAL). Programmatic C++ fallback (RETRIEVAL→LLM) after one retry. | ✅ |
| **3** | **Memory hygiene** — Store clean goals (strip `[RELEVANT PAST APPROACHES…]`). Similarity floor on plan reuse and trajectories (inject nothing vs weak matches). Sanitized reuse outlines (step-type summary only). Top-1 similar plan. Reflection channel gets failure summary only — no plan-reuse re-injection. | ✅ |
| **4** | **Strategy scoring** — Top-1 promoted strategy by goal-embedding similarity (do not disable; do not inject all). | ✅ |
| **5** | **Planner instrumentation** — Log rules/schema/goal/reuse/strategy/trajectory bytes, `experience_dropped`, `depends_on_repaired`, `fallback_used`. Feeds **C6**. | ✅ |

**Memory channel separation (target architecture):**

| Channel | Gets |
|---------|------|
| Planner | schema, goal, one similar plan, one relevant strategy |
| Conversation | chat history |
| Reflection | previous failure summary |

**Do not:** tail-truncate planner prompts; inject all promoted strategies; auto-insert missing plan steps; re-inject nested plan-reuse blocks into stored goals.

**Validate on:** GUI goals outside test corpus; `run_test_suite` regression after each phase.

#### C2 — Retrieval ranking & chat RAG observability

Two pipelines: goal execution (**C1** hardened) vs conversation (`processQuery`, **C2** complete).

| Phase | Task | Status |
|-------|------|--------|
| **0** | **Chat RAG observability** — `CHAT_RAG_CONTEXT` + `CHAT_RAG_RESPONSE` → `logs/chat_rag.jsonl`; ranked docs + scores from `GragDiagnostics`; byte counts (retrieval, tools, history, memory); `grounding_ratio`, `tool_ratio`, `history_ratio`; truncation before/after + section; linked by `request_id`. | ✅ |
| **1** | **Golden corpus benchmark** — `run_chat_rag_benchmark`: 5 queries, 4-file markdown corpus (`GRAG.md`, `cognate.md`, `HOWTO.md`, `AGENTS.md`); doc-level hit@1, nDCG@1, MRR; output → `logs/chat_rag_benchmark.jsonl`. Requires Ollama. | ✅ |
| **2** | **Retrieval tuning** — conversational filename boost, filename coverage recall, min-chunk filter, chunk metadata at injection, session-scoped corpus via `setActiveCorpusFiles`. Goal execution path unchanged. | ✅ |
| **3** | **Chat pipeline fixes** — grounding rules, Q&A tool gating, section-protected truncation (no tail-chop), fixed grounding/tool ratios, `llm_model` fallback. | ✅ |

**Run benchmark:** `./build/debug/external/basic_agent/run_chat_rag_benchmark` (Ollama required).  
**Baseline (2026-06-27):** hit@1 **2/5**, mean nDCG@1 **0.40** — AGENTS.md dominates short definitional queries.  
**After Phase 2 (2026-06-27):** hit@1 **5/5**, mean nDCG@1 **1.00**, mean MRR **1.00**.

**After Phase 3 (2026-06-27):** GUI litmus test “Explain GRAG” — **accurate grounded answer** ✅ (user validated).

**Litmus test (manual):** “Quote the first sentence of GRAG.md” — check `logs/chat_rag.jsonl` for rank/score/grounding_ratio.

#### C3 — Reflection A/B measurement

Prove reflection replan helps recoverable failures and does **not** loop on timeouts.

| Phase | Task | Status |
|-------|------|--------|
| **1** | **Policy helpers** — `trajectoryHasTimeoutFailure()`, `reflectionSkipReason()`; configurable `max_reflections` (default 2, env `THOTH_MAX_REFLECTIONS`). | ✅ |
| **2** | **Golden cases + harness** — `run_reflection_ab_benchmark`: C3-01 recoverable NODE→LLM rescue; C3-02 timeout skip. Log → `logs/reflection_ab_benchmark.jsonl`. Mock-only (`THOTH_MOCK_LLM`, `THOTH_MOCK_STEP_TIMEOUT`). | ✅ |

**Run benchmark:** `./build/debug/external/basic_agent/run_reflection_ab_benchmark` (no Ollama).

**Results (2026-06-26):** 2/2 cases pass; mean reflection lift **0.5** (C3-01 FAILED→COMPLETED with reflection on; C3-02 both arms FAILED, planner_calls=1).

**Key files:** `reflection_utils.*`, `reflection_ab_cases.*`, `run_reflection_ab_benchmark.cpp`, `executive_controller.cpp` (`set_max_reflections`), `cognitive_metrics.h` (`max_reflections`, `reflection_skip_reason`).

#### C5 — Robustness & failure tests

Mock-only harness for planning, retrieval, execution, reflection, and lifecycle edge cases. Asserts **observable behavior** (terminal state, bounded retries, structurally valid plans) — not internal mechanism names.

| Category | Cases |
|----------|-------|
| **Retrieval** | C5-01 empty index; C5-02 empty retrieval result → LLM empty-context message |
| **Planning** | C5-03 invalid JSON→retry→valid plan; C5-04 invalid step order; C5-05 missing `depends_on` repair |
| **Reflection** | C5-06 reflection budget exhausted (bounded terminal state) |
| **Execution** | C5-07 step timeout; C5-08 `THOTH_MOCK_LLM_UNAVAILABLE`; C5-09 concurrent goals |
| **Lifecycle** | C5-10 controller teardown (no crash) |

**Run:** `./build/debug/external/basic_agent/run_robustness_suite` (~40s, no Ollama)  
**Log:** `logs/robustness_suite.jsonl` — per-case `terminal_state`, `failure_reason`, `reflection_cycles`, `pass_reason`, `duration_ms`

**Mock gates:** `THOTH_MOCK_LLM`, `THOTH_MOCK_LLM_UNAVAILABLE`, `THOTH_MOCK_STEP_TIMEOUT`, `THOTH_MOCK_LLM_DELAY_MS`, `RobustnessMockResponses` queue (scripted planner JSON).

#### C4 — Developer & CI latency

Fast feedback while coding; keep full Ollama path for regression.

| Phase | Task | Status |
|-------|------|--------|
| **1** | **`run_test_suite --dev`** — TfIdf embeddings, mock LLM (`test_suite_dev`), tiny `test_suite_corpus`, cached `test_suite.rag_index.bin`, skip re-index. ~10s vs ~40 min full. | ✅ |
| **2** | **CI tiers** — PR: `ctest -L pr` (unit + dev TEST_SUITE + reflection A/B + robustness). Dev loop: `ctest -L fast` (~70s). Nightly: Ollama `test-suite-full`. | ✅ |

**Run:**
```bash
./build/debug/tests/run_test_suite --dev    # default; no Ollama
./build/debug/tests/run_test_suite --full   # Ollama regression (~40 min)
```

**CI (GitHub Actions):** `.github/workflows/ci-security.yml` — PR/push runs `ctest -L pr`; scheduled nightly runs full Ollama `test-suite-full`.

**Local CTest labels:** `fast` (cognitive mocks ~70s: dev TEST_SUITE + reflection A/B + robustness), `pr` (unit + cognitive), `nightly` (configure with `-DTHOTH_TEST_SUITE_FULL=ON`).

**Dev vs full TC-03:** dev checks goal-scoped GRAG row (`goal_present`); full requires `alpha > 0` and `direction_magnitude > 0`.

#### C7 — Runtime latency (production goals)

Optimize real goal execution without touching the C4 mock/CI path.

| Phase | Task | Status |
|-------|------|--------|
| **1** | **Hot-path wins** — batch TF-IDF keyword rescoring; `embedBatch` for goal+state G/C; cap synthesis context (`synthesis_max_context_chars`); Ollama `options.num_predict` for synthesis (`synthesis_num_predict`); log `synthesis_prompt_chars` in C6. | ✅ |
| **2** | **Analysis + tuning loop** — `scripts/summarize_cognitive_metrics.py`; tune config from p50/p95 wall-clock. | ✅ |
| **3** | **Parallel prefetch / scheduling** — overlap independent RETRIEVAL where plan DAG allows; snapshot G/C/T at dispatch; prefetch cache for single RUNNING dependency. | ✅ |

**Config** (`agent_workspace/config.json`): `synthesis_max_context_chars` (default 8192), `synthesis_num_predict` (default 512), `max_parallel_retrieval` (default 4), `enable_retrieval_prefetch` (default true). Planner still uses `max_tokens`.

**Analyze latencies:**
```bash
python3 scripts/summarize_cognitive_metrics.py --log logs/cognitive_metrics.jsonl
```

#### C4 & C7 — Latency (developer vs runtime) ✅ complete

Two different engineering problems; separate solutions and success criteria. **Both shipped 2026-06-26 – 2026-06-28** (see `completed_improvements_log.md`).

| | **C4 — Developer & CI** | **C7 — Runtime / production** |
|--|---------------------------|-------------------------------|
| **Status** | ✅ Complete | ✅ Complete |
| **Goal** | Fast feedback while coding; cheap regression in CI | Faster real goal execution for users |
| **Delivered** | `run_test_suite --dev` (~10s), mock LLM + TfIdf + cached index; CTest labels (`fast`/`pr`/`nightly`); GitHub Actions PR + nightly Ollama | Batch TF-IDF rescoring; `embedBatch` G/C; synthesis context cap + `num_predict`; parallel RETRIEVAL + dependency prefetch; `summarize_cognitive_metrics.py` |
| **Measures** | CI wall time, developer iteration loop | Per-goal `total_wall_clock_ms`, step latencies (via **C6**) |
| **Verify** | `ctest -L fast` 3/3 (~70s); `run_test_suite --dev` 7/7 | `testParallelRetrieval`; config in `agent_workspace/config.json` |
| **Risk** | Mocks drift from production — mitigated by nightly `test-suite-full` | GRAG quality (**C2**) and plan correctness (**C1**) preserved via regression harnesses |

#### C6 — Cognitive metrics

Move beyond pass/fail: record **quantitative metrics for every goal execution**, persist them append-only, and aggregate over hundreds of runs.

| Phase | Task | Status |
|-------|------|--------|
| **1** | **Append-only per-goal logging** — `GOAL_COGNITIVE_METRICS` → `logs/cognitive_metrics.jsonl` on `PLAN_COMPLETED` / `PLAN_FAILED` / `PLAN_ABORTED`. | ✅ |
| **2** | **Analysis tooling** — `scripts/summarize_cognitive_metrics.py`; `scripts/plot_cognitive_metrics.py` (matplotlib); token counts from `LLMInterface`; GUI export (Benchmarks → Export Cognitive Metrics, JSONL/CSV). | ✅ |
| **3** | **Longitudinal analysis** — time-series over many runs: trend success/latency/tokens; segment by `plan_reused`, strategy injection, pre/post consolidation; answer “is the system improving?” | 📋 | See **§ Reflection**; expert + analysis consensus |

**Per-goal fields (Phase 1 schema):**

| Metric | Purpose |
|--------|---------|
| `planning_time_ms` | LLM plan generation latency |
| `retrieval_time_ms` | RETRIEVAL step wall time |
| `llm_synthesis_time_ms` | LLM step wall time |
| `retrieved_chunk_count` | Context volume delivered to synthesis |
| `grag_alpha` | Directional blend at retrieval (from GRAG diagnostics) |
| `trajectory_score` | ExecutiveController outcome score |
| `reflection_count` | Reflection replan cycles consumed |
| `max_reflections` | Configured reflection limit for the goal |
| `reflection_skip_reason` | Why reflection was suppressed (`timeout_failure`, `reflection_disabled`, etc.) |
| `final_success_score` | Plan history / past_plans success signal |
| `total_tokens` | LLM token usage (plan + synthesis; Ollama/OpenAI or char estimate for mocks) |
| `planning_tokens` / `synthesis_tokens` | Split at last planning boundary |
| `total_wall_clock_ms` | Goal start → PLAN_COMPLETED/FAILED |

**Questions metrics should answer over time:**

- Is planning getting faster?
- Is retrieval quality improving (alpha, chunk count, downstream success)?
- Does reflection trigger less often after tuning?
- Did a change increase average latency or token cost?

**Implementation notes:**

- **Partial data today:** `StepMetricsRepository`, `grag_benchmark.jsonl`, `decision_trace.jsonl`, `GragMetricsLogger` — now supplemented by unified per-goal rows.
- **Phase 1 (✅):** `logs/cognitive_metrics.jsonl` — `GOAL_COGNITIVE_METRICS` on `PLAN_COMPLETED` / `PLAN_FAILED` / `PLAN_ABORTED`; fields below.
- **Phase 2 (✅):** `summarize_cognitive_metrics.py`, `plot_cognitive_metrics.py`, `LLMInterface` session token tracking, GUI export (JSONL/CSV).
- **Consumers:** C3 A/B runs, C4/C7 regression checks, `benchmark_results.md` archive, thesis/paper figures.

---

### 1. Verification & audit refresh

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **V1** | Re-run `TEST_SUITE.md` TC-01–TC-07 | ✅ | **Headless:** `run_test_suite` ✅ 2026-06-27; **GUI:** ✅ 2026-06-29 — see **`TEST_SUITE_GUI_CHECKLIST.md`** |
| **V2** | Refresh `audit.md` | ✅ | 2026-06-18 — shell gate fixed, portable paths, verification checklist |
| **V3** | Re-upload **`MYPAPER.md`** to Zenodo | ⏸️ | Deferred — more benchmark runs first (see **C2**) |

---

### 2. Phase 3 — Memory stability (finish internals)

**Phase status:** 🔶 Partial — `improvements.md` Phase 3

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **M1** | Memory consolidation (warm tier) | ✅ | **Verified** 2026-06-26 — **`memory_architecture.md`**; M1.5 gate passed |
| **M1.5** | Episodic verification gate | ✅ | **`episodic_memory_benchmark.md`** — E2E retrieval, failure injection, latency, negative cases |
| **M2** | Pruning: age-based trigger | ✅ | Policy-driven consolidation — `memory` config, `ConsolidationDecision`, Clock |
| **M3** | Pruning: admin `/prune` command | ✅ | Manual trigger + `DecisionTraceLogger`; `getConsolidationStatus` / `runConsolidation` API |
| **M4** | Pruning: `MemoryPruner::restore(session_id, range)` | 📋 | On-demand historical replay; transactional SQLite |
| **M5** | Vector store benchmark scaffold | 📋 | Step 3.4: ingest/latency/memory contract tests at 10k/50k/100k chunks; dual-write stub (disabled) |

**Done already (don't redo):** `FactStore`, `MemoryPruner` hot-tier auto-prune, `IVectorStore` / `FlatVectorStore` wrapper.

---

### 3. Phase 4 — Advanced reasoning & GRAG

**Phase status:** 🔶 Partial — `improvements.md` Phase 4

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **G1** | Trajectory tuning (Step 4.5) | 🔶 | Overlaps **C2** — $w_t=0.2$ active; trajectory-disambiguation bucket **−0.037** in one run |
| **G2** | Hierarchical subgoal trees (Step 4.4) | 📋 | `GoalNode`, active subgoal embedding in `GragScorer`; single root $G$ today |
| **G3** | Model upgrade path (Step 4.7) | 📋 | Audit `LLMInterface` for model-specific assumptions; `ModelConfig` + migration playbook |
| **G4** | Trace replay vs SQLite resume | 🔶 | Document-only acceptable: trace is observability; `resume_from_plan()` is authoritative |

**Done already:** Scientific mode, `StrategyEngine`, `GraphRefiner`, `TrajectoryBuilder` wiring.

---

### 4. Benchmarks, eval & automated testing

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **B1** | Research-paper corpus: 30 hardened cases | 📋 | `new_corpus_tests.md`; feeds **V3** Zenodo |
| **B2** | Automate critical manual suite signals | ✅ | `run_test_suite.cpp` + `check_baseline.py` (2026-06-27); **C5** extends coverage |
| **B3** | Reduce test log noise | 📋 | Repeated embedding migration log per fixture |
| **B4** | Compiler warnings (~14 on debug build) | 📋 | Unused params in stubs/GUI |
| **E1** | Benchmark environment pinning | 🔶 | A–C ✅, **D1–D5 ✅** — all harnesses wired; E1-12…16 smoke. Next: **E** — **`docs/benchmark_environment.md`** |
| **E2** | Episodic memory learning eval | 📋 | Repeat-goal / multi-session harness: does consolidation improve later success? (beyond M1.5 pipe test) |
| **E3** | Strategy impact / SCR harness | 📋 | Automated SCR or plan-structure proxy in nightly/CI; `COGNATE_V2.md` metric → regression JSONL |
| **G1d** | Trajectory bucket diagnostic | 📋 | Ablation on `TRAJECTORY_DISAMBIGUATES` (`w_t=0` vs `0.2`, empty T) before **F5** |

---

### 5. NODE execution harness

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **N1** | Decide: implement or defer | 📋 | `NODE.md` + `build_node.md` are spec-only; no `/core/node` in repo |
| **N2** | If defer: one-line status in `README.md` / `NODE.md` | 📋 | Ensure nothing claims NODE is operational |

**Default recommendation:** defer until Phase 3–4 memory/GRAG items above are closed.

---

### 6. UI polish (after core stable)

From `ui_improvements.md` §11–§12 — research console shell exists; these are enhancements:

| Priority | Item |
|----------|------|
| 1 | Tool output renderers (`summarize_text` first) |
| 2 | Step Execution debug mode (manual step → observe) |
| 3 | Retrieval Inspector chunk drill-down + alpha/magnitude color cues |
| 4 | Execution Trace unified timeline |
| 5 | GRAG vector visualizer + interactive plan graph |
| 6 | GUI sync test harness (backend writes → panel assertions) |

---

### 7. Self-building — last (optional future expansion)

**Do not start until Tier 2–4 are in good shape.** Phase 5 in `improvements.md` is **not scheduled**.

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **S1** | `code_modify` / `apply_diff` | 🚫 stub | Returns prototype error; read + `project_analyze` + `run_tests` work |
| **S2** | Self-correction loop in production goals | 📋 | Harness exists; not a closed autonomous edit-verify loop |
| **S3** | Phase 5 close-out criteria | 📋 | See `improvements.md` Phase 5 — only after S1–S2 |

---

### 8. Future cognitive expansion — research horizon

**Do not start until C1–C7 are complete and Phase 3–4 internals are in good shape.** These are the next layer of capability — beyond fixing context management and measuring what exists today. Spec detail belongs in `improvements.md` when any item is promoted to active work.

| ID | Direction | Status | Notes |
|----|-----------|--------|-------|
| **F1** | **Better planning heuristics** | 📋 | Move past scaffold adherence: goal decomposition, cost/risk-aware step ordering, dynamic RETRIEVAL depth, domain-aware plan templates. Builds on **C1** (validator + budgets) and **G2** (subgoal trees). |
| **F2** | **Smarter reflection strategies** | 📋 | Selective replan by failure type (timeout vs wrong answer vs tool error), not only low trajectory score. Separate reflection prompts and memory channel (started in **C1**). Measure first via **C3**, then implement targeted strategies. |
| **F3** | **Richer episodic memory** | 📋 | Full episodic store: searchable trajectories, summarize-before-archive warm tier, restore/replay. Procedural vs episodic channel separation for planner vs conversation vs reflection. Overlaps **M1–M4**, **C1** memory channels. |
| **F4** | **Long-term learning** | 📋 | Cross-session competence: strategy promotion over weeks, forgetting/decay, graph edge consolidation, plan-pattern libraries that improve without polluting prompts. Extends `StrategyEngine`, `GraphRefiner`, **C6** time-series metrics. |
| **F5** | **More sophisticated trajectory scoring** | 📋 | Semantic trajectory embeddings (not stub/zero), step-sequence similarity, outcome-weighted $T$, better disambiguation on trajectory bucket. Overlaps **G1**, **C2**, `TrajectoryBuilder`. |
| **F6** | **Multi-agent collaboration** | 📋 | Delegate subgoals to specialized agents/roles, coordination protocol, shared memory boundaries. No implementation today — architecture TBD (`PLAN.md`, `cognate.md`). |
| **F7** | **Better tool selection** | 📋 | Goal-aware tool ranking from `StepMetricsRepository` success rates, strategy hints in planner, reduce unnecessary TOOL steps in non-corpus modes. Extends `ToolRegistry`, `StrategyEngine`, **F1**. |
| **F8** | **Curriculum generation** | 📋 | Auto-generate eval/training goals from corpus gaps, progressive difficulty, regression suites for planner/retrieval/reflection. Feeds **B1**, **C5**, **C6**; supports thesis/paper benchmark expansion. |

**Suggested dependency order (when promoted):** F3/F5 (memory + trajectory signal) → F1/F7 (planning + tools) → F2 (reflection) → F4 (long-term learning) → F8 (curriculum/eval automation) → F6 (multi-agent, highest architectural lift).

---

### 9. Strategic review — external reflection 2026-06-29

#### Architectural milestone acknowledgment

During C1–C7 and M1–M2, Thoth crossed an architectural threshold. The project now has a coherent cognitive architecture with automated verification and policy-driven memory management. Future milestones are no longer about inventing core infrastructure — they are about demonstrating measurable improvements in cognition over time. That is a fundamentally different and more mature problem.

#### The honest gap

Thoth is in a strong engineering position and a weaker evidence position. The cognitive loop works, memory consolidates, and the docs tell the truth about limits. What does not exist yet is proof that the system learns over time — which is the thesis claim that strategy promotion and episodic memory are meant to support.

Three independent reviewers reading the same codebase converged on the same diagnosis: instrumentation (C6) and component tests (C3–C5) are solid, but the longitudinal question has not been asked. That convergence is a strong signal this is the right place to invest next.

#### The three testing tiers (do not conflate)

| Tier | Examples | Valid claims |
|------|----------|--------------|
| **Fast mock CI** | `--dev`, C3/C5, reflection A/B | Mechanism fires, no crash, bounded retries |
| **Slow Ollama truth** | nightly `--full`, chat RAG 5/5 | Retrieval quality, real synthesis |
| **Accumulated multi-session** | C6 Phase 3 over weeks of runs | System gets better — the learning claim |

The third tier does not exist yet. It is the missing bridge between "it works" and "it gets better." Building it is the primary remaining research obligation.

#### Consolidated priority order (all reviewers)

| Step | Item | Rationale |
|------|------|-----------|
| **1** | **M3** — `/prune` admin command | Closes operational memory loop; unblocks debugging and consolidation demos |
| **2** | **E1** — Environment pinning | Pin model version, Ollama version, corpus fingerprint. One day of work that saves months of unreproducible benchmarks. Must exist before E2 or B1 mean anything |
| **3** | **E2** — Repeat-goal multi-session harness | Run the same goal cold then warm across sessions; measure delta systematically. This is the actual learning proof — the question M1.5 deliberately did not ask |
| **4** | **M4** — `MemoryPruner::restore(session_id, range)` | Built into already-verified eval environment. Foundation F3 needs; do not parallel-track with E2 |
| **5** | **G1 diagnostic** — Trajectory scoring ablation | `w_t=0` vs `0.2` vs empty T ablation is cheap. Tells you tune vs drop vs redesign before touching anything else in retrieval. Do not promote F5 until this completes |
| **6** | **E3** — SCR in CI | Wire Strategy Conformance Rate into continuous benchmark. Makes strategy promotion a regression signal not a one-off paper figure |
| **7** | **C6 Phase 3** — Accumulated multi-session analysis | Longitudinal analysis tooling over weeks of Ollama/GUI runs. Establishes the third testing tier |
| **8** | **B1** — 30 hardened corpus cases | Run under pinned E1 environment. Defensible benchmark claims for V3 |
| **9** | **V3** — Zenodo re-upload | Only after B1 + E1 pinned runs. Do not re-publish stale or unreproducible numbers |
| **10** | **F-series** — Chosen by evidence | F1/F3 are highest-leverage next capability layer. Do not promote until E2/E3 data identifies the actual bottleneck — retrieval, decomposition, or tool choice |

#### What to avoid

- **Zenodo V3 before B1 and E1 are complete** — re-publishing unreproducible numbers weakens the paper
- **F5 before G1 diagnostic completes** — risk of amplifying noise in the trajectory signal
- **Bulk F-series promotion** — the horizon list is correctly deferred; eval data should drive the order
- **Parallel-tracking M4 and F3** — M4 is the foundation F3 needs

#### Missing definition (open question to resolve)

The roadmap correctly defers F-series until eval data points the way, but no promotion criteria are defined. Before starting E2/E3, document: how many sessions, what delta in SCR or nDCG, and what threshold triggers promotion of the first F-series item. Without this the eval layer has no exit condition.

---

## Suggested session order

```
Done    Headless TEST_SUITE TC-01–TC-07 (2026-06-27)
Done    executeLLM + plan templates + workflow_engine fixes (2026-06-27)
Done    V2 — audit.md refreshed (2026-06-18)
Done    B2 — run_test_suite + check_baseline (2026-06-27)

Done    C2 Phase 0 — chat RAG observability (logs/chat_rag.jsonl)
Done    C2 Phase 1 — golden corpus benchmark (run_chat_rag_benchmark)
Done    C2 Phase 2 — conversational retrieval tuning (5/5 hit@1)
Done    C2 Phase 3 — chat pipeline fixes (grounding, tool gating, truncation) — user validated ✅
Done    C1 phases 1–5 — planner context management (pushed 379c0c5)
Done    C6 Phase 1 — cognitive metrics logging (`logs/cognitive_metrics.jsonl`)
Done    C3 — reflection A/B measurement (2/2 cases, mean lift 0.5)
Done    C4 Phase 1 — run_test_suite --dev (~10s, mock LLM + TfIdf + cached index)
Done    C4 Phase 2 — CI tiers (PR fast / nightly full Ollama)
Done    C7 Phase 1–2 — hot-path wins + summarize_cognitive_metrics.py
Done    C7 Phase 3 — parallel RETRIEVAL dispatch + dependency prefetch
Done    C5 — robustness suite (10 cases, logs/robustness_suite.jsonl)
Done    C6 Phase 2 — plot script, LLM token counts, GUI export
Done    V1 — manual GUI TEST_SUITE TC-01–TC-07 (2026-06-29, observability confirmed)
Done    M1.5 — episodic verification (E2E retrieval, failure inject, latency, benchmark) ✅ 2026-06-26
Done    M2 — age-based consolidation policy (config, Clock, structured decisions) ✅
Next 1  M3–M4 — /prune admin + range restore (memory ops)
Next 2  G1d + E1 — trajectory ablation; benchmark env pinning
Next 3  C6 Phase 3 + E2 + E3 — longitudinal metrics; episodic lift; SCR harness
Next 4  B1 (if V3 Zenodo) — hardened research corpus
Later   F3/F1 — when eval identifies bottleneck (§ Reflection)
Later   Tier 6 UI polish
Last    Tier 7 self-building / apply_diff (owner discretion)
Horizon Tier 8 future cognitive expansion (F1–F8; see §8)
External V3 — Zenodo MYPAPER re-upload when benchmark corpus stable (C2 ✅)
Next    M3 — /prune admin command
Next    E1 Checkpoint E — close-out (identity pass, Python scripts)
Next    E2 — repeat-goal multi-session harness (the learning proof)
Next    M4 — range restore (built into verified eval environment)
Next    G1d — trajectory ablation benchmark (w_t=0 vs 0.2 vs empty T)
Next    E3 — SCR wired into CI
Next    C6 Phase 3 — accumulated multi-session longitudinal analysis
Later   B1 — 30 hardened corpus cases (under pinned E1 environment)
Later   V3 — Zenodo re-upload (after B1 + E1)
Later   F-series — promoted by eval evidence, not schedule
```

**GitHub (2026-07-01):** Thoth `fd247c8`, Basic_agent `4c45aca` on `main` — E1 D3–D5 harness wiring pushed.

---

## Quick reference — partial vs not started

| Item | Today |
|------|-------|
| End-to-end cognitive loop (RETRIEVAL → LLM → PLAN_COMPLETED) | ✅ headless 2026-06-27 |
| Headless TEST_SUITE 7/7 | ✅ dev ~10s (`--dev`); full ~40 min with Ollama |
| Manual TEST_SUITE (GUI) | ✅ 2026-06-29 — `TEST_SUITE_GUI_CHECKLIST.md` |
| Chat RAG observability (C2 Phase 0) | ✅ `logs/chat_rag.jsonl` |
| Golden chat retrieval benchmark (C2 Phase 1) | ✅ `run_chat_rag_benchmark` — baseline 2/5 hit@1 |
| Conversational retrieval tuning (C2 Phase 2) | ✅ 5/5 hit@1 on golden corpus |
| Chat pipeline fixes (C2 Phase 3) | ✅ user-validated grounded Q&A |
| Planning quality (C1) | ✅ phases 1–5 shipped |
| Chat / retrieval quality (C2) | ✅ phases 0–3; user-validated grounded Q&A |
| Per-goal cognitive metrics (C6) | ✅ logging + summarize/plot scripts + tokens + GUI export |
| Reflection A/B measurement (C3) | ✅ `run_reflection_ab_benchmark` — 2/2 cases |
| Developer / CI latency (C4) | ✅ complete — `ctest -L fast` 3/3 (~70s); PR `ctest -L pr`; nightly `--full` |
| Runtime latency (C7) | ✅ complete — Phases 1–3 (embed batch, synthesis cap, parallel RETRIEVAL + prefetch) |
| Robustness test coverage (C5) | ✅ `run_robustness_suite` 10/10 |
| Future cognitive expansion (F1–F8) | 📋 research horizon — §8 |

---

## Doc map (where to read)

| Need | File |
|------|------|
| Full phase specs | `improvements.md` |
| What's actually shipped | `completed_improvements_log.md` |
| Honest gaps + external review | `audit.md` §5 |
| **Reflection & next priorities** | **`cursor_list.md` § Reflection & analysis** |
| GRAG implementation truth | `GRAG.md`, `benchmark_results.md` |
| Cognate / executive truth | `cognate.md`, `PLAN.md` |
| GRAG paper (Zenodo) | `MYPAPER.md` |
| Cognate paper (Zenodo) | `COGNATE_V2.md` |
| Manual test protocol | `TEST_SUITE.md`, `TESTING.md` |
| UI backlog | `ui_improvements.md` §11–§12 |

---

*Update this file when starting or finishing a tier. Append summaries to `completed_improvements_log.md`; do not mark phases complete in `improvements.md` until close-out criteria pass.*
