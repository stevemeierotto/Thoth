# Thoth Working Backlog

**Last updated:** 2026-07-04 (E2 **Phase C protocol v1.1** — integration tier roadmap; see **§ C.0.0**)  
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
| **E1** | **Benchmark environment pinning** | ✅ Complete 2026-07-01 — Checkpoints A–E; spec **`docs/benchmark_environment.md`**; unblocks E2, B1, V3 |
| **E2** | **Episodic learning eval** | M1.5 = pipeline correctness. STRICT = deterministic retrieval from **declared frozen episodes** (not organic consolidation). Full checkpoint plan: **§ E2**; protocol **`docs/E2_PROTOCOL.md`**. |
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
Now      G1 diagnostic + E2          (trajectory ablation; episodic learning eval)
Next     C6 Phase 3 + E3             (longitudinal metrics; SCR harness)
Parallel B1 (if V3 Zenodo)           (hardened corpus)
Later    M4                         (range restore — M3 ✅)
Later    F3 / F1                     (promote when eval shows bottleneck)
Last     UI polish (§6), S1 apply_diff (owner discretion)
```

### What not to do yet

- **F5** (semantic trajectory embeddings) before G1 diagnostic completes.
- **Zenodo V3** before B1 + pinned-env benchmark runs.
- **F-series bulk promotion** — horizon items; eval should drive which F moves first.
- **NODE / self-building** — defer per existing backlog discipline.

---

## E2 — Episodic learning eval kernel migration (active plan)

**Status:** 🔶 In progress — protocol steps 1–5 complete; harness wiring pending  
**Spec:** `docs/E2_PROTOCOL.md` v1.2 (preregistered constants; do not change mid-run)  
**What this is:** Converting a coupled cognitive runtime into a **two-layer evaluation kernel** with determinism boundaries — not a refactor.

| Layer | Role |
|-------|------|
| **`e2_eval_kernel`** | Deterministic lab function: sealed inputs → scored outputs. No heuristics, no hidden state. |
| **`basic_agent` + `rag.cpp`** | Product cognition. **E2-INTEGRATION** tier = diagnostic only, never official evidence. |

**Pause between every sub-checkpoint** (same discipline as E1 D1–D5): build green → tests green → confirm before next step.

**Checkpoint plans** (A1, A2, A3, …) are **approved — subject to revision** as implementation proceeds. Only **`E2_PROTOCOL.md` v1.2** pass/fail rules are preregistered/locked until v1.3.

### Done (protocol steps 1–5)

- `E2EvalConfig`, `SealedEpisodeInjectionLog`, version pins, evaluation fingerprint
- `e2_eval_kernel` CMake target (compile-time exclusion of `rag.cpp`)
- `validateStrictConfigForOfficialRun`, table-driven evaluator, mock harness scaffold
- Unit tests E2-01–E2-07

### Known bug (Phase 0 — blocking, before A1)

~~Harness init assigns `embedding_model_version = probeEngine->getInternalVersion()` (returns `int` 2) into a `std::string` → `\u0002` control char, not `"2"`.~~ **✅ Fixed 2026-07-01** — `makeEmbeddingModelVersionPin("TfIdf", …)` + `requirePrintablePin` in `validateStrictConfigForOfficialRun`; regression test `testE2EmbeddingVersionPin`.

### Gate architecture (priority order)

Debugging should read **`scoring_block_reason`** in artifacts before reverse-engineering which gate fired.

| Tier | Mechanism | Role |
|------|-----------|------|
| **1 — Source of truth** | `e2_eval_kernel` compile-time exclusion (`THOTH_E2_STRICT_KERNEL=1`; no `rag.cpp`) | **Pre-build guarantee** — fix here if anything disagrees |
| **2 — Verification** | Linker/symbol audit (`nm -C` on scored-path `.o` + harness binary) | **Post-build check** — does not substitute for tier 1 |
| **3 — Behavioral** | Runtime guard (A5), unit tests E2-08–E2-11, wiring-state gate | Catches reintroduction / wiring mistakes |

Source grep for `RAGPipeline` / `retrieveRelevant` = fast sanity check only, **not** an exit criterion.

**Enforcement philosophy** (see **`E2_PROTOCOL.md` § Enforcement philosophy**): compile-time exclusion prevents accidental linkage; static audits verify intended dispatch; runtime guard detects regressions at execution. **No single tier is sufficient alone.** STRICT never silently falls back to heuristic retrieval.

### Scope limits — what STRICT claims (and does not)

| Question | Answered by | Mechanism |
|----------|-------------|-----------|
| Can consolidated memory be retrieved? | **M1.5** ✅ | Organic consolidation pipeline |
| Given **declared frozen episodes**, does deterministic retrieval change goal outcomes? | **E2-STRICT** (official, **Phase B** only) | Pre-sealed `SealedEpisodeInjectionLog` |
| Does organic consolidation → warm tier → retrieval → lift? | **E2-INTEGRATION** (diagnostic) | Real `plantAndConsolidate` path — **non-scoring** |

**STRICT warm ≠ organic consolidation.** Pre–Phase A harness used `plantAndConsolidate` → real episodic → warm tier → GRAG. Phase A warm arm = **pre-sealed case-table entries** (synthetic injection). Intentional for determinism; must be stated plainly in protocol and any publication.

### Wiring modes — disabled vs kernel-verified vs official (Phase A / B)

Do **not** use fake `FAILED_*` to mean "not wired yet" — that conflates system defect with intentional non-evaluation.

| Concept | Artifact signal |
|---------|-----------------|
| **Evaluation disabled (A1–A2)** | `scoring_enabled: false`; `retrieval_enabled: false`; no lift; no `e2_outcome`; `wiring_stage: "A1"` or `"A2"` |
| **Kernel retrieval verified (A3–A5)** | `retrieval_enabled: true`; `evaluation_boundary_verified: true`; `official_scoring: false`; `scoring_enabled: false`; no `e2_outcome`; no benchmark SUCCESS/FAILURE claims |
| **Official benchmark (Phase B+)** | `official_scoring: true`; `scoring_enabled: true`; `e2_outcome` permitted when all gates pass |
| **Evaluation failed (Phase B+ only)** | Official scoring attempted; `arm_scoring_status: FAILED_*`; lift withheld |

| CP | Kernel retrieval? | Harness | `e2_outcome` | `official_scoring` |
|----|-------------------|---------|--------------|-------------------|
| **A1** | No | Builder self-test only (`E2_STRICT_INJECTION_LOG_DIAG`); **no `runCaseArm`** | Not emitted | `false` |
| **A2** | No | Plumbing smoke: **`runCaseArm`** per case; sealed log built/logged; **no kernel retrieval** | Not emitted | `false` |
| **A3** | Yes (`e2StrictRetrieve` @ boundary) | **`runCaseArm`** + kernel boundary diag; executive outputs **non-authoritative** | Not emitted | `false` |
| **A4–A5** | Yes (executive wired / guarded) | Migration continues; still pre-baseline | Not emitted | `false` |
| **B** | Yes | Full scored loop | Permitted if all gates pass | `true` |

**`THOTH_E2_WIRING_STAGE` values (documented contract):**

| Value | Meaning | Protocol? |
|-------|---------|-----------|
| `A1` | Builder-only diagnostic (opt-in regression after A2 lands) | ✅ Checkpoint |
| `A2` | Consolidation decoupled; arm plumbing smoke; evaluation-disabled | ✅ Checkpoint |
| `A3` | Kernel retrieval consumes sealed log; boundary provenance verified; **not** official scoring | ✅ Checkpoint |
| `A4` | Context injection + single dispatch + executive → same `e2StrictRetrieve()`; full equivalence; **not** official scoring | ✅ 2026-07-02 |
| `A5` | Diagnostic fuse; post-A4 regression enforcement; guard-only `rag.cpp` change; **not** official scoring | ✅ 2026-07-02 — E2-11; `runtime_heuristic_guard` |
| *(unset after A5)* | Defaults to current checkpoint; Phase B next | ✅ |
| `SCORING` | **Temporary dev knob only** — legacy full loop. **Must not exist in any authoritative benchmark configuration** (Phase B re-baseline, publication, CI nightly). Not protocol. | ❌ Internal dev only |

Pre–**Phase B** harness output is **non-authoritative** regardless of exit code.

**JSONL event names (A1 implemented — do not introduce alternates):**

| Event | Used for |
|-------|----------|
| `E2_STRICT_INJECTION_LOG_DIAG` | Per case/arm sealed-log row; **A2** adds plumbing fields after `runCaseArm`; **A3** adds kernel boundary fields (`retrieval_enabled`, `strict_retrieval_status`, chunk summary) |
| `E2_WIRING_CHECKPOINT` | Checkpoint summary row + `BenchmarkRun` emit via `completeWiringCheckpoint()` |

### Provenance at evaluation boundary (A3+)

**Rule:** Complete provenance required **at the evaluation boundary** (where kernel chunks enter `strictProvenanceValid()`), not at every internal pipeline stage. After A3, **STRICT provenance must never come from executive diagnostics** — only from `e2StrictRetrieve()` via `provenanceFromStrictRetrievalResult()`. Any untraced chunk **crossing the boundary** → `FAILED_PROVENANCE`, no lift, **no degraded-score mode**.

**Vacuous retrieval guard (implement at boundary / evaluator):** if `retrieval_status == SUCCESS` **and** expectations require episodic retrieval **and** `chunk_count == 0` → `FAILED_RETRIEVAL` (retrieval owns emptiness; provenance owns trace completeness). Prevents `chunks = []` from passing `strictProvenanceValid()` vacuously.

See **`docs/E2_PROTOCOL.md` § STRICT retrieval boundary** for the canonical diagram.

### Implementation checkpoints

| ID | Covers | Stop criteria |
|----|--------|---------------|
| **0** | Embedding pin `\u0002` fix + semantic validation + regression test | ✅ 2026-07-01 |
| **5.0** | Wiring contract + gate priority in `E2_PROTOCOL.md` | Doc only |
| **A1** | STRICT sealed log builder from case table | ✅ 2026-07-02 — **E2-08**; `buildStrictInjectionLogFromCaseTable`; evaluation-disabled harness |
| **A2** | Remove `plantAndConsolidate` from STRICT path; scope-limits doc | ✅ 2026-07-02 — **E2-09** + **E2-09b**; Option A test discipline |
| **A3** | `e2StrictRetrieve()` + boundary provenance; layer ownership contract | ✅ 2026-07-02 — **E2-10**; `retrieval_enabled: true`; purity verified; **not** official scoring. **Close-out:** token-overlap episode gating + **provisional 0.25 threshold** documented in `E2_PROTOCOL.md` § STRICT kernel scoring (A3) |
| **A4** | Executive delegates to same `e2StrictRetrieve()`; context injection + single dispatch + full equivalence | E2-01–E2-07; static dispatch audit + golden-case runtime proof |
| **A5** | Diagnostic fuse; A3→A4 transition enforced; failure-domain separation; first `rag.cpp` touch (guard only) | ✅ 2026-07-02 — **E2-11** |
| **B** | STRICT re-baseline (after 0 + A1–A5) | Authoritative SUCCESS/FAILURE **only after A4 proves harness–executive retrieval equivalence** |
| **C** | `--tier integration` + E2-06 + `completed_improvements_log.md` close-out | INTEGRATION non-scoring |

**Scope estimate:** Phase A ≈ **3–5 sub-sessions** (not one session) — largest change since E1 Checkpoint C.

**Phase A decomposition (one edge per checkpoint):**

```
A1  →  build sealed log
A2  →  carry sealed log (harness plumbing)
A3  →  kernel consumes sealed log          (Harness → e2StrictRetrieve)
A4  →  executive consumes kernel           (Executive → e2StrictRetrieve)
A5  →  prevent regressions                 (runtime guard)
B   →  authorize scoring
C   →  integration tier
```

### Checkpoint A1 — STRICT sealed log plumbing (approved — subject to revision)

**Expert verdict:** Approved. One isolated checkpoint, one proof obligation, then stop — same discipline as E1 D1–D5.

**Purpose:** Prove deterministic construction of STRICT sealed injection logs from the frozen case table. **No retrieval, scoring, executive, or RAG behavior changes are permitted during A1.**

| In scope | Out of scope |
|----------|--------------|
| Pure builder logic + unit tests | `e2StrictRetrieve()` (A3) |
| Evaluation-disabled harness gate | `plantAndConsolidate` removal (A2) |
| Diagnostic JSONL of sealed logs | Lift, `e2_outcome`, `official_scoring: true` |
| | Executive / `RAGPipeline` changes |

**Time estimate:** **45–90 minutes** focused implementation + ~10 min build/test (one short sub-session).

#### A1.1 — Builder API

Add **`buildStrictInjectionLogFromCaseTable(case, armLabel, builder_timestamp_ms)`** (name must include **STRICT** — integration-tier builders may follow later).

Returns a **sealed** `SealedEpisodeInjectionLog`. No I/O.

#### A1.2 — Arm semantics (case table is authoritative)

**Cold arm is not always empty.** `cold_arm_pre_consolidated` models pre-existing memory on the cold arm.

```
cold arm:
  if cold_arm_pre_consolidated && plant_message non-empty
      → one injected episode (sealed)
  else
      → empty sealed log

warm arm:
  if plant_message non-empty
      → one injected episode (sealed)
  else
      → empty sealed log
```

**E2-03:** `cold_arm_pre_consolidated == true` → cold arm gets the Apollo entry (pollution negative control). Matches protocol — not "cold means empty."

Each entry fields: `episode_id` ← `plant_session_id`, `source` ← `"evaluation"`, `content` ← `plant_message`, `content_hash` ← `sha256Hex(content)`, `injected_at_ms` ← shared builder timestamp.

#### A1.3 — Deterministic timestamps

**Do not** call `now()` per entry. **`injected_at_ms` must come from a single deterministic builder timestamp** passed into the builder (or a test clock). All entries in one invocation share that value. Removes timing flakiness; simplifies later provenance comparison.

#### A1.4 — Immutability (implementation watch)

After seal, expose **const** access only (`const std::vector<EpisodeInjectionEntry>& entries()` — already the API shape). **No** mutable `entries()` or non-const vector references post-seal; otherwise `seal()` is weaker than intended.

#### A1.5 — E2-08 unit tests

| Test | Asserts |
|------|---------|
| Seal immutability | `append()` after `seal()` throws |
| E2-01 cold | Empty entries, `sealed: true` |
| E2-01 warm | One entry; content + hash match case table |
| E2-03 cold | One entry present (`cold_arm_pre_consolidated`) |
| **Builder determinism** | Same case + arm + timestamp → **byte-identical** `toJson().dump()` on two builds (field equality, not pointer equality). Catches accidental UUIDs, per-entry timestamps, etc. |

Tests do **not** assert `arm_scoring_status` or lift.

#### A1.6 — Harness gate (evaluation-disabled, not failed)

| Field | A1 value |
|-------|----------|
| `official_scoring` | `false` |
| `scoring_enabled` | `false` |
| `wiring_stage` | `"A1"` |
| `e2_outcome` | **not emitted** |

Arms are **not scored** — not failed. Optional diagnostic JSONL rows with sealed log JSON per case/arm. Main scored case loop **skipped**. Exit 0 if E2-08 + self-test pass.

#### A1.7 — Exit criteria (stop before A2)

1. Build green  
2. `thoth-unit-tests` green including E2-08 + determinism test  
3. Harness runs evaluation-disabled (no `e2_outcome`)  
4. **Builder determinism:** same case + arm + timestamp ⇒ byte-identical sealed JSON  
5. **Pause for confirmation** — bugs after A1 likely live in retrieval wiring (A3+), not log construction  

#### A1 files (expected)

`episodic_learning_eval.h/.cpp`, `run_episodic_learning_benchmark.cpp`, `tests/unit_tests.cpp`, `docs/E2_PROTOCOL.md` (A1 scope sentence), `cursor_list.md` (mark ✅ on close-out)

### Checkpoint A2 — STRICT consolidation decoupling (approved — subject to revision)

**Expert verdict:** Approved with Option A test discipline + **one-sealed-log-per-arm** invariant. One checkpoint, one proof obligation, then stop.

**Purpose:** Prove STRICT no longer depends on **runtime episodic creation** (`plantAndConsolidate` or any equivalent). Arms declare input only via a **single** sealed log per invocation. **No retrieval, scoring, executive, or RAG behavior changes.**

**Dataflow (one edge per checkpoint):**

```
A1:  case table → sealed log                    (builder only)
A2:  case table → sealed log → runCaseArm       (plumbing; log not consumed by retrieval)
A3:  sealed log → e2StrictRetrieve() → chunks  (retrieval wired — A3 only)
```

**Known pre-A2 gap (to fix in A2):** `main()` treats `A1` and `A2` identically (builder diag only, no `runCaseArm`). A2 must **split branches**: A1 stays builder-only; A2 runs arm plumbing smoke.

| In scope | Out of scope |
|----------|--------------|
| Remove `plantAndConsolidate` from `runCaseArm` | `e2StrictRetrieve()` (A3) |
| **One sealed log per arm** — built once, `const&` for arm lifetime | Sealed log consumed by retrieval (A3) |
| **No episodic storage writes** during STRICT arm execution | `RAGPipeline` / executive changes (A4) |
| Split A1 vs A2 branches; default `wiring_stage=A2` | Official scoring / `e2_outcome` (**Phase B**) |
| E2-09 + **E2-09b** (ownership) + scope-limit doc | Tier-2 linker audit (A4) |
| **Option A:** `runE2TestArm` no plant; skip `testE2CaseById` E2-01–E2-03 until **A4** | |
| `SCORING` env — dev only; **never authoritative** | |

**Time estimate:** **60–120 minutes** focused implementation + ~10 min build/test.

#### A2.0 — Core invariants (A3 may rely on these)

1. **Exactly one** `SealedEpisodeInjectionLog` is constructed **per arm invocation** via `buildStrictInjectionLogFromCaseTable` — not per helper, not lazily re-built on nested calls.
2. **`runCaseArm` owns the log:** build at arm entry → pass **`const SealedEpisodeInjectionLog&`** (or const pointer) to all downstream helpers for that arm's lifetime. No copies, no rebuilds, no mutable aliases.
3. **Read-only after construction:** treat as immutable input for the arm (sealed + const-ref ownership — A2 establishes this contract even before A3 consumes it).
4. **No episodic mutation:** STRICT arm execution **must not write to episodic storage in any form** during A2 — not only `plantAndConsolidate`, but also any replacement (`injectEpisode()`, direct warm-tier writes, consolidation triggers, etc.). Closes the "different function, same coupling" loophole.

#### A2.1 — `runCaseArm` decoupling

- Remove `needsPlant` / `plantAndConsolidate(...)` from `runCaseArm`.
- **Once at arm entry:** `const SealedEpisodeInjectionLog sealedLog = buildStrictInjectionLogFromCaseTable(spec, armLabel, ts)` — suite-level `ts` for JSONL consistency with A1.
- Thread `sealedLog` as **`const&`** through arm body; helpers take `const SealedEpisodeInjectionLog&` if they need the log (A3 retrieval helper stub may accept but not use yet).
- **Forbid** second `buildStrictInjectionLogFromCaseTable` call anywhere in `runCaseArm` call tree (enforce via E2-09b).
- Log `sealedLog.toJson()` on **`E2_STRICT_INJECTION_LOG_DIAG`** (+ optional plumbing fields after executive run).
- Keep `plantAndConsolidate` **uncalled** on STRICT path (retain for future INTEGRATION / Phase C only).

#### A2.2 — Harness branch split

```
THOTH_E2_WIRING_STAGE unset → default "A2" (after A2 lands)

A1:  builder diag only (E2_STRICT_INJECTION_LOG_DIAG + E2_WIRING_CHECKPOINT); no runCaseArm
A2:  for each case → runCaseArm(cold) + runCaseArm(warm);
     log E2_STRICT_INJECTION_LOG_DIAG (sealed log + plumbing fields) per arm; no evaluateEpisodicLearningCase
SCORING:  legacy full loop — dev knob only; MUST NOT be used for authoritative runs (Phase B, publication, CI)
```

#### A2.3 — Arm semantics (unchanged from A1.2)

A2 does **not** change builder logic. Sealed log **shape** remains what E2-08 proved deterministic; A2 adds **ownership and wiring** only.

#### A2.4 — Option A: unit test discipline

- Update **`runE2TestArm`** to match harness: **no** `e2PlantAndConsolidate`; same one-log-per-arm invariant.
- **Skip** `testE2CaseById("E2-01")`, `"E2-02"`, `"E2-03"` until **A4** (executive RETRIEVAL via strict kernel):

  `// TODO A4: re-enable testE2CaseById E2-01–E2-03 once executive RETRIEVAL delegates to e2StrictRetrieve()`

  Kernel retrieval for those cases is covered by **E2-10** at A3.
- E2-04 smoke / E2-08 / E2-09 / E2-09b remain green.

#### A2.5 — E2-09 / E2-09b unit tests

| ID | Asserts |
|----|---------|
| **E2-09** | No `plantAndConsolidate` / `e2PlantAndConsolidate` on STRICT arm paths (source contract) |
| **E2-09** | Default A2 harness: exit 0; `E2_WIRING_CHECKPOINT` + arm rows; gate fields; no `e2_outcome` |
| **E2-09** | Sealed log on arm row matches A1 builder (E2-01 warm, E2-03 cold spot-check) |
| **E2-09** | Reuse A1 events only — extend `E2_STRICT_INJECTION_LOG_DIAG` schema if needed; **no new event names** |
| **E2-09b** | **Ownership:** builder invoked **exactly once** per arm execution (test hook / call counter on builder wrapper, or refactor arm entry to single visible build site grepable by E2-09) |
| **E2-09b** | Arm diagnostic JSON sealed log **byte-identical** to standalone builder output for same inputs (proves same log, not a rebuild) |

Does **not** assert retrieval hits, lift, or arm `COMPLETED`.

**Tier-2 linker audit:** Not required for A2. Reserved for A4.

#### A2.6 — Gate contract (harness)

| Field | A2 value |
|-------|----------|
| `official_scoring` | `false` |
| `scoring_enabled` | `false` |
| `wiring_stage` | `"A2"` |
| `e2_outcome` | **not emitted** |

Arms run for plumbing; **not scored** — not failed.

#### A2.7 — Scope-limits doc (`E2_PROTOCOL.md`)

Add under § cold/warm A/B:

> **Scope limit — STRICT vs organic consolidation:** E2-STRICT warm arms use **pre-sealed case-table episodes** (`buildStrictInjectionLogFromCaseTable`), not the product consolidation pipeline (`plantAndConsolidate` → episodic → warm tier). M1.5 validates organic consolidation produces retrievable memory; E2-STRICT validates deterministic retrieval from **declared frozen input**. Organic consolidation → warm → retrieval → lift is **E2-INTEGRATION** (diagnostic, non-scoring) only.

Add under implementation / checkpoint A2:

> **One sealed log per arm:** exactly one `buildStrictInjectionLogFromCaseTable` call per arm invocation; passed by const reference thereafter. STRICT arm execution must not mutate episodic storage during A2.

Cold row footnote: empty unless `cold_arm_pre_consolidated` (E2-03).

#### A2.8 — Exit criteria (stop before A3)

1. Build green  
2. `thoth-unit-tests` green: E2-08 + **E2-09** + **E2-09b**; `testE2CaseById` E2-01–E2-03 **skipped** with TODO A4 comment  
3. Harness default A2: arm plumbing smoke; gate contract satisfied; no `e2_outcome`  
4. Sealed logs on arm rows match A1 builder for same case/arm/ts  
5. Zero `plantAndConsolidate` / `e2PlantAndConsolidate` on STRICT arm paths  
6. **No writes to episodic storage** during STRICT arm execution (architectural point of A2 — no plant, no inject-replacement, no consolidation side effects)  
7. **One sealed log per arm** — E2-09b ownership invariant holds  
8. Scope-limit + ownership sentences in `E2_PROTOCOL.md`  
9. **Pause for confirmation** — retrieval miss or arm `FAILED` in A2 smoke is expected; scoring is disabled  

#### A2 files (expected)

`run_episodic_learning_benchmark.cpp`, `tests/unit_tests.cpp`, `docs/E2_PROTOCOL.md`, `cursor_list.md` — **no** `rag.cpp`, `executive_controller.*`, `e2_strict_retrieval.*`

### Checkpoint A3 — STRICT kernel retrieval + boundary provenance (approved — subject to revision, **v2**)

**Expert verdict:** Approved after scope tightening — one proof obligation (kernel consumes sealed log), then stop. Same discipline as A1/A2. Plan may be revised before or during implementation.

**One-sentence definition:** A3 proves deterministic kernel retrieval consumes sealed evaluation episodes and produces complete boundary provenance **independent of Executive or RAGPipeline behavior**.

**Single proof obligation:** Can deterministic retrieval consume the sealed log?

**Explicitly not A3:** scoring, SUCCESS, lift, benchmark outcome, executive correctness → **A4**, **A5**, **Phase B**.

**Purpose:** Prove **`e2StrictRetrieve()`** consumes the A2 sealed log and produces fully traced `RetrievedChunkRecord[]` at the **evaluation boundary**. A3 proves **kernel retrieval correctness** — it does **not** attempt to prove **benchmark correctness**.

**Dataflow (one edge per checkpoint):**

```
A1:  case table → sealed log
A2:  case table → sealed log → runCaseArm              (plumbing; log not consumed)
A3:  sealed log → e2StrictRetrieve() → chunks @ boundary
A4:  executive RETRIEVAL → strict path
B:   official_scoring: true; authoritative e2_outcome
```

| In scope | Out of scope |
|----------|--------------|
| Wire `e2StrictRetrieve()` with sealed log + index | Official scoring / `e2_outcome` (**Phase B**) |
| `provenanceFromStrictRetrievalResult()` — replace `provenanceFromRetrievalStepResult()` on STRICT boundary | Executive `executeRetrieval()` changes (**A4**) |
| **Layer ownership contract** (see A3.0) | Ranking redesign (**log only** if E2-10 exposes gap) |
| Vacuous-retrieval guard (derived from existing case-table fields) | `evaluateEpisodicLearningCase` / lift / case pass-fail loop |
| Harness `wiring_stage=A3`; log kernel boundary fields | `testE2CaseById` full pass (**A4**) |
| **E2-10** (retrieval, boundary, fail-closed, **retrieval-only** determinism) | Tier-2 linker audit (**A4**) |
| **Purity verified** (formal exit criterion — see A3.7) | `rag.cpp` runtime guard (**A5**) |
| **`runCaseArm` continues**; executive **ignored** on STRICT boundary | Index construction lifecycle (harness/test responsibility) |

**Time estimate:** **90–150 minutes** focused implementation + ~10 min build/test.

#### A3.0 — Layer ownership contract

Each layer consumes **only** the output of the layer before it. No layer bypasses or reaches around its predecessor.

```
builder            →  SealedEpisodeInjectionLog   (A1)
retrieval kernel   →  RetrievedChunkRecord[]       (A3, from sealed log)
boundary mapper    →  provenance                   (A3, from retrieval result)
```

| Rule | Meaning |
|------|---------|
| Builder never sees retrieval | A1 builder has no `e2StrictRetrieve` calls |
| Kernel never sees provenance/scoring | `e2_strict_retrieval.*` does not call evaluator or `provenanceFromStrictRetrievalResult` |
| Boundary mapper never re-derives from case table | `provenanceFromStrictRetrievalResult` maps **only** what the kernel returned — no reading `spec.plant_message` to fabricate hits the kernel did not surface |

**Ownership violation** = any code that skips a layer (e.g. boundary mapper inferring hits from spec fields absent from kernel chunks). Blocks A3 exit.

Inherit **A2.0** (one sealed log, const-ref, no episodic writes).

#### A3.0a — Source of retrieval expectations (no new schema)

**Decision:** Expectations (hit tokens, forbidden tokens, “episodic required”) are derived from the **existing case specification** — not a new schema field. `spec.plant_message`, `spec.cold_arm_pre_consolidated`, and **A1.2 arm semantics** encode what should/shouldn't be retrievable per arm.

**Vacuous-retrieval guard** — “episodic required” computed from existing fields, e.g.:

- Warm arm + non-empty `plant_message` → episodic content expected on retrieval success  
- Cold arm + `cold_arm_pre_consolidated` + non-empty `plant_message` → episodic content expected  

If implementation discovers no existing field can express a needed expectation → **explicit protocol change**, not a quiet addition. Requires checklist item **A3.0b** (schema field, protocol doc, A1 builder update, regression test) and **pause for confirmation** (retroactively touches A1).

#### A3.0b — Schema change gate (only if A3.0a blocked)

| Step | Work |
|------|------|
| Name field | Document in `E2_PROTOCOL.md` + `episodic_learning_eval.h` |
| Update A1 builder | If field affects sealed log shape |
| Regression | Extend E2-08 |
| Confirm | Pause before proceeding |

#### A3.0c — Determinism responsibilities (no overlap with A1)

| Checkpoint | Owns | Tested by |
|------------|------|-----------|
| **A1** | Builder determinism — same case + arm + timestamp ⇒ byte-identical sealed log JSON | E2-08 / A1.5 (existing) |
| **A3** | Retrieval determinism — same **already-sealed** log + same index ⇒ byte-identical chunk ids + ordering from `e2StrictRetrieve()` | E2-10 determinism sub-case |

E2-10 determinism: build sealed log **once**, reuse as fixed input; call `e2StrictRetrieve()` twice — **never** re-invoke the builder. Prevents E2-10 from silently re-testing A1.5.

#### A3.0d — Index contract

`e2StrictRetrieve()` treats the supplied index as **immutable for the duration of the call**. Index construction, caching, freshness, and lifecycle are **outside A3's proof obligation** — harness or test code builds the index before calling the kernel.

A3 asserts only: **given a fixed index and a fixed sealed log, retrieval is deterministic and pure.**

#### A3.0e — Executive execution (harness continuity only)

The Executive may still run during A3 harness cases — **strictly for harness continuity and future A4 comparison** (baseline to diff against later).

| Rule | Meaning |
|------|---------|
| STRICT evaluation ignores Executive retrieval | No Executive-derived chunk, score, or provenance field may feed `obs.retrieval` on the STRICT boundary path |
| Not a dependency | A3 proof holds even if the Executive step were removed |

#### A3.0f — Purity (design + verified exit criterion)

`e2StrictRetrieve()` is a **pure function**: inputs `query`, sealed log, index, config, `top_k`; outputs `E2StrictRetrievalResult`. **No writes, global state, caches, SQLite, Executive, or RAG.**

Purity is a **formal exit criterion** (equal weight to test pass), not guidance-only.

#### A3.1 — Boundary mapping

Add **`provenanceFromStrictRetrievalResult(retrieval, expectations)`** in `episodic_learning_eval.h/.cpp`:

- Copy kernel `RetrievedChunkRecord[]` into `prov.chunks` (kernel output only).
- Derive hit/forbidden-token fields by matching **kernel chunk content** against expectations derived per **A3.0a**.
- Vacuous guard: `retrieval_status == SUCCESS` + episodic required (A3.0a) + `chunk_count == 0` → `FAILED_RETRIEVAL`.
- On STRICT A3+ paths, **forbid** `provenanceFromRetrievalStepResult()` as boundary source of truth.

#### A3.2 — `runCaseArm` wiring

At arm entry (A2 invariants preserved):

1. Build sealed log once → `const&` for arm lifetime.
2. Build index (distractor unchanged — lifecycle outside kernel).
3. `e2StrictRetrieve({ query: spec.goal, episode_log, index, engine, config, top_k })`.
4. Populate **`obs.retrieval`** via `provenanceFromStrictRetrievalResult()` only.
5. Executive may run afterward for continuity — **do not** merge executive RETRIEVAL into STRICT provenance.

#### A3.3 — Harness branch

```
THOTH_E2_WIRING_STAGE unset → default "A3" (after A3 lands)

A1:  builder diag only
A2:  runCaseArm plumbing; no kernel retrieval
A3:  runCaseArm + e2StrictRetrieve @ boundary; log kernel fields on E2_STRICT_INJECTION_LOG_DIAG
     NO evaluateEpisodicLearningCase; NO e2_outcome
SCORING: legacy dev knob — never authoritative
```

#### A3.4 — Gate contract (harness)

| Field | A3 value |
|-------|----------|
| `official_scoring` | `false` |
| `scoring_enabled` | `false` |
| `retrieval_enabled` | `true` |
| `evaluation_boundary_verified` | `true` |
| `wiring_stage` | `"A3"` |
| `e2_outcome` | **not emitted** |

#### A3.5 — E2-10 unit tests (four categories)

| Category | Asserts |
|----------|---------|
| **Retrieval** | E2-01 warm retrieves Apollo; E2-01 cold does not; E2-02 warm hit; E2-03 distractor / no episodic hit |
| **Boundary** | `provenanceFromStrictRetrievalResult` mapping correct; statuses propagate; **no case-table bypass** |
| **Fail-closed** | Unsealed log → `FAILED_STRICT_BOUNDARY`; non-STRICT tier → `FAILED_STRICT_BOUNDARY`; exception → `FAILED_RETRIEVAL`, zero chunks |
| **Determinism** | Pre-built sealed log + fixed index → two `e2StrictRetrieve()` calls → identical chunk ids + ordering (**no builder re-invoke**) |

Does **not** assert: executive `COMPLETED`, lift, `testE2CaseById`, harness `e2_outcome`.

**If E2-10 retrieval cases fail due to ranking:** document — **do not** expand A3 scope unless demonstration is impossible. **A3 landed:** episode gating uses deterministic token overlap (not embeddings) for purity; see **`E2_PROTOCOL.md` § STRICT kernel scoring (A3)**. Threshold **0.25 is provisional** (v1.2 trio gap ~0.17 vs ~0.83) — revisit at B1 or v1.3.

#### A3.6 — Protocol doc (`E2_PROTOCOL.md`)

§ STRICT retrieval boundary, § Kernel ownership, § **STRICT kernel scoring (A3)**, A3 scope-limit sentence. Layer ownership diagram/table present under § STRICT retrieval boundary / Kernel ownership.

#### A3.7 — Exit criteria (stop before A4)

1. Build green  
2. Tests green: E2-08 + E2-09 + E2-09b + **E2-10**; `testE2CaseById` E2-01–E2-03 still **skipped** (A4)  
3. Harness default A3: kernel boundary logged; gate contract satisfied; **no `e2_outcome`**  
4. No `provenanceFromRetrievalStepResult()` on STRICT boundary path in harness  
5. A2 invariants preserved  
6. **Purity verified** — `e2StrictRetrieve()` has no side effects, no `RAGPipeline` / `ExecutiveController` / episodic / SQLite dependency. Verify via: **(a)** code review / grep for forbidden includes or calls; **(b)** unit test calling function twice in isolation (no harness fixtures with hidden state) → identical output  
7. No ranking redesign unless E2-10 blocked and escalated  
8. **Ownership contract respected** — no layer reaches past its predecessor  
9. **Pause for confirmation** — Phase B owns the official-scoring switch  

#### A3 files (expected)

`e2_strict_retrieval.h/.cpp`, `episodic_learning_eval.h/.cpp`, `run_episodic_learning_benchmark.cpp`, `tests/unit_tests.cpp`, `docs/E2_PROTOCOL.md`, `cursor_list.md` — **no** `workflow_engine.cpp`, `rag.cpp`, `executive_controller.*`

### Checkpoint A4 — Executive RETRIEVAL → strict kernel (approved — subject to revision, **v3**)

**Expert verdict:** Approved (~9.8/10) — v3 adds architectural explicitness (context injection, single dispatch, full equivalence including failures, caller independence) **without expanding scope**. Plan may be revised before or during implementation.

**One-sentence definition:** A4 proves the **Executive RETRIEVAL step** delegates **directly** to the **same `e2StrictRetrieve()` implementation** the harness uses — with **identical retrieval outcomes** (success and failure) on representative golden cases.

**Single proof obligation (unchanged):** Does the Executive use the **exact same deterministic retrieval kernel** as the evaluation harness?

**Explicitly not A4:** official scoring / authoritative `e2_outcome` (**Phase B**); runtime guard in `rag.cpp` (**A5**); ranking redesign; lift / benchmark SUCCESS validation; INTEGRATION tier harness (**Phase C**).

**Purpose:** Close the gap between **harness boundary wiring (A3)** and **runtime step dispatch**. After A4, mock LLM `required_token` validation reads RETRIEVAL step output that is **equivalent** to harness kernel output — enabling end-to-end arm completion without `plantAndConsolidate`.

**Dataflow (staged migration — one edge):**

```
A3   Harness        →  e2StrictRetrieve()
A4   Executive      →  e2StrictRetrieve()     (same implementation; independent callers)
A5   Runtime guard  →  prevent RAG bypass if miswired
B    Scoring        →  official_scoring + e2_outcome
```

| In scope | Out of scope |
|----------|--------------|
| **STRICT retrieval context injection** — sealed log ownership + lifetime (A4.0a) | Official scoring / `e2_outcome` (**Phase B**) |
| **Single dispatch decision point** — STRICT vs NON-STRICT (A4.0b) | `rag.cpp` abort/throw guard (**A5**) |
| Executive **direct** delegation to `e2StrictRetrieve()` | Ranking / merge tuning in kernel |
| **Kernel identity** — exactly one STRICT retrieval implementation | Lift validation (Phase B) |
| **Full retrieval equivalence** — success + failure paths (A4.0e) | Phase B re-baseline run |
| **Caller independence** — kernel result independent of caller identity | Phase C INTEGRATION |
| **Re-enable `testE2CaseById` E2-01–E2-03** + E2-04–E2-07 green | New JSONL event names |
| **Static dispatch audit** + golden-case runtime proof | |
| **NON-STRICT negative test** — legacy RAG preserved | |
| Harness `wiring_stage=A4`; equivalence logged on `E2_STRICT_INJECTION_LOG_DIAG` | |
| Tier-1 compile isolation (`e2_eval_kernel` unchanged) | |

**Time estimate:** **2–4 hours** (context injection plumbing + equivalence tests + static audit).

#### A4.0 — Core invariants (inherit A3)

1. **Kernel identity** — Exactly **one** authoritative STRICT retrieval implementation: `e2StrictRetrieve()`. Harness and Executive are **independent callers** — not parallel implementations. **Forbidden:** `executeRetrievalStrict()`, private merge helpers, or any fork that could diverge silently.

2. **Direct delegation with explicit marshalling rules** — Executive RETRIEVAL calls the kernel directly.
   - **Allowed (input marshalling):** adapting `StepExecutionContext` → `E2StrictRetrievalInput`; argument construction; parameter passing; **format-only** mapping of kernel output to step JSON field names.
   - **Forbidden (output transformation):** modifying retrieval results; reordering chunks; filtering; augmenting; alternate merge logic; alternate provenance logic.
   - **Prohibited chain:** Executive → wrapper → modified retrieval → `e2StrictRetrieve()`.

3. **Caller independence (referential transparency)** — For identical inputs (`query`, sealed log, index, config, `top_k`), `e2StrictRetrieve()` must return **identical** results regardless of caller (harness vs Executive). Caller identity must **never** influence retrieval.

4. **Full retrieval equivalence (required exit criterion)** — On representative golden arms (E2-01–E2-03), harness and Executive must agree on **all** retrieval outcomes:
   - Success: chunk ids, ordering, provenance fields, chunk count
   - Failure: `retrieval.status`, fail-closed behavior, `FAILED_RETRIEVAL`, `FAILED_PROVENANCE`, `FAILED_STRICT_BOUNDARY`, vacuous-retrieval guard outcomes, and any other terminal retrieval status
   - Mismatch on **either** success or failure path **blocks A4 exit**.

5. **No episodic writes** — A2 invariant holds.

6. **NON-STRICT preserved** — Product / non-STRICT paths **must continue** to use legacy `RAGPipeline::retrieveRelevant` (negative regression test required).

#### A4.0a — STRICT retrieval context injection contract

Eliminates ambiguity: **where does the Executive get its sealed log?**

| Question | Answer |
|----------|--------|
| **Who creates the sealed log?** | **`runCaseArm` / harness arm entry** — exactly one `buildStrictInjectionLogFromCaseTable(spec, armLabel, ts)` per arm invocation (A2 ownership). |
| **Who owns it?** | The **arm execution scope** — `const SealedEpisodeInjectionLog` built once; **`const&` or const pointer** for arm lifetime. No duplicate builds. |
| **How does it reach the Executive?** | Harness **injects** the sealed log pointer (and `E2EvalConfig`, index/engine handles) into **Workflow/Executive step context** before `execute_goal` / RETRIEVAL dispatch. Executive reads context; it does **not** build or seal logs. |
| **Lifetime** | Valid from arm entry through last RETRIEVAL step of that arm; invalidated when arm scope ends. Executive must not retain pointer beyond arm lifetime. |
| **Evaluation only** | This injection path exists **only** for E2 evaluation / golden-case harness execution (`run_episodic_learning_benchmark`, `runE2TestArm` under STRICT). |
| **Production** | Normal product goal execution **does not** create or inject sealed evaluation logs. GUI / chat paths do not use `buildStrictInjectionLogFromCaseTable` unless explicitly in an eval harness. |

#### A4.0b — Single dispatch decision point (long-term invariant)

| Rule | Meaning |
|------|---------|
| **Exactly one decision** | One authoritative branch selects STRICT kernel vs NON-STRICT `RAGPipeline` retrieval — typically inside `WorkflowEngine::executeRetrieval` (or one named delegate it calls unconditionally). |
| **No scattered flags** | Avoid multiple independent `if (strict)` checks across Executive, harness, and RAG that could disagree. Tier/mode flows **into** the single decision point. |
| **Documented invariant** | STRICT mode selection has **one** authoritative decision point — document location in code comment + `E2_PROTOCOL.md` A4 sentence. |

#### A4.0c — STRICT retrieval context ownership (diagram)

```
Evaluation harness (runCaseArm arm entry)
        │
        │  owns: one build per arm
        ▼
buildStrictInjectionLogFromCaseTable()
        │
        ▼
SealedEpisodeInjectionLog  (const, sealed, arm lifetime)
        │
        │  inject pointer + E2EvalConfig into step context
        ▼
WorkflowEngine / Executive context
        │
        │  marshal → E2StrictRetrievalInput (allowed)
        ▼
e2StrictRetrieve()                    ← same function as A3 harness boundary
        │
        │  no output transformation (forbidden)
        ▼
Executive RETRIEVAL step result
```

**No layer creates a duplicate sealed log.** Builder (A1) → kernel (A3) → executive caller (A4) — same ownership chain as A3 layer contract, extended with context injection.

#### A4.0d — Golden-case equivalence scope (not exhaustive)

| Clarification | Meaning |
|---------------|---------|
| **Representative tests** | E2-01–E2-03 (all arms) are **architectural equivalence tests** — they prove harness and Executive call the same kernel with the same outcomes. |
| **Not exhaustive** | They do **not** prove every production Executive retrieval scenario, every goal type, or every future case table entry. |
| **A4 claim** | A4 proves **kernel dispatch equivalence** on golden cases — not universal production retrieval coverage. |

#### A4.1 — Executive wiring

Target: **`WorkflowEngine::executeRetrieval`** — the **single dispatch decision point** (A4.0b).

| Step | Work |
|------|------|
| **Context receive** | Read injected `const SealedEpisodeInjectionLog*`, `E2EvalConfig`, index/engine from step context (A4.0a) |
| **STRICT branch** | Marshal to `E2StrictRetrievalInput`; call `e2StrictRetrieve()` **directly**; map to step `result.data.chunks` (**format-only**) |
| **Non-STRICT branch** | Unchanged — `RAGPipeline::retrieveRelevant` |
| **Fail-closed** | Kernel non-OK → failed RETRIEVAL step; failure status must match harness boundary behavior (equivalence includes failures) |

Harness `runCaseArm`: **retain** A3 boundary retrieval — compare harness vs executive on **success and failure** paths for golden arms; mismatch → fail.

#### A4.2 — Harness branch

```
THOTH_E2_WIRING_STAGE unset → default "A4" (after A4 lands)

A3:  kernel @ harness boundary; executive ignored for provenance
A4:  context injection + executive RETRIEVAL → e2StrictRetrieve(); harness boundary retained for equivalence proof
     NO evaluateEpisodicLearningCase; NO e2_outcome
SCORING: legacy dev knob — never authoritative
```

#### A4.3 — Gate contract (harness)

| Field | A4 value |
|-------|----------|
| `official_scoring` | `false` |
| `scoring_enabled` | `false` |
| `retrieval_enabled` | `true` |
| `evaluation_boundary_verified` | `true` |
| `executive_strict_retrieval` | `true` |
| `harness_executive_retrieval_equivalent` | `true` (required on golden arms — success + failure) |
| `wiring_stage` | `"A4"` |
| `e2_outcome` | **not emitted** |

Separates **retrieval correctness** from **benchmark authority**.

#### A4.4 — RAG bypass evidence (static + runtime — do not conflate)

| Evidence type | What it proves | How |
|---------------|----------------|-----|
| **Static (tier-2 audit)** | STRICT **dispatch code** at the **single decision point** references `e2StrictRetrieve()`, not `RAGPipeline::retrieveRelevant` in the STRICT branch | Audit `executeRetrieval` (or delegate) TU + grep; no parallel strict retrieval fork |
| **Runtime (golden cases)** | STRICT execution produces retrieval **identical** to harness — **including failure paths** | E2-01–E2-03 equivalence on status, chunks, and fail-closed outcomes |

**Defensible claim (A4 exit):** No STRICT execution path invokes `RAGPipeline::retrieveRelevant`, as demonstrated by **static dispatch inspection** and **golden-case runtime equivalence** — not linker absence alone.

Does **not** substitute for tier-1 `e2_eval_kernel` compile exclusion.

#### A4.5 — Unit / case tests

| ID | Asserts |
|----|---------|
| **E2-01–E2-03** | `testE2CaseById` re-enabled — Executive RETRIEVAL consumes STRICT kernel output; mock LLM token check passes on success paths |
| **E2-04–E2-07** | Remain green |
| **E2-08–E2-10** | Regression |
| **Equivalence (success)** | Golden arms: harness == executive (ordering, ids, provenance, chunk count) |
| **Equivalence (failure)** | Injected fault or negative cases: harness == executive on `retrieval.status`, fail-closed, vacuous guard (where applicable) |
| **Caller independence** | Same inputs via harness-direct vs executive path → identical kernel-equivalent outcome |
| **Negative (NON-STRICT)** | Non-STRICT path still calls `RAGPipeline::retrieveRelevant` |
| **Static audit** | Single dispatch point; STRICT branch → kernel symbol |

**Does not assert:** lift margins, `e2_outcome`, benchmark SUCCESS/FAILURE, exhaustive production coverage.

#### A4.6 — Exit criteria (stop before A5)

1. Build green  
2. `thoth-unit-tests` green: **E2-01–E2-07** + E2-08–E2-10 + full equivalence + caller independence + NON-STRICT negative  
3. Harness default A4; **no `e2_outcome`**  
4. **Context injection contract** implemented and documented (A4.0a) — no duplicate sealed logs  
5. **Single dispatch decision point** documented (A4.0b)  
6. **Static dispatch audit** passes  
7. **Golden-case runtime equivalence** — harness == executive on **success and failure** retrieval outcomes  
8. **Kernel identity** + **caller independence** verified  
9. A2 + A3 invariants preserved  
10. **Pause for confirmation** — **Phase B** after A4 equivalence; **A5** adds runtime guard  

#### A4 files (expected)

`workflow_engine.cpp`, `executive_controller.cpp` (context injection plumbing only if needed), `run_episodic_learning_benchmark.cpp`, `tests/unit_tests.cpp`, static audit script/test, `docs/E2_PROTOCOL.md`, `cursor_list.md` — **no** `rag.cpp` behavior change (**A5**)

### Checkpoint A5 — Runtime guard (approved — subject to revision, **v2**)

**Expert verdict:** Approved — architecturally complete. v2 resolves A3→A4→A5 transition, failure-domain separation, guard contract, signal precedence, defense-in-depth rationale, observable golden regression, and blast radius — **without expanding scope**. Plan may be revised before or during implementation.

**One-sentence definition:** A5 adds a **tier-3 diagnostic fuse** at heuristic retrieval entry points so that if STRICT execution is **accidentally routed** into heuristic retrieval, the system **fails immediately** rather than silently continuing.

**Single proof obligation (unchanged):**

> If STRICT execution is accidentally routed into heuristic retrieval, does the system fail immediately rather than silently continuing?

**Do not add to A5:** retrieval redesign; Executive wiring; scoring; benchmark authority; `e2_outcome`; ranking work; integration work (Phase C).

**Explicitly not A5:** proving Executive wiring is correct (A4); kernel retrieval (A3); official scoring (Phase B).

**Purpose:** **Defense-in-depth fuse** — protects against bypass or **regression of A4**, not a new retrieval algorithm. The guard **detects** architectural violations; it **never redirects, repairs, or substitutes** kernel retrieval.

#### A5.0 — Architectural timeline: A3 → A4 → A5

| Phase | Executive under STRICT eval | Heuristic retrieval under STRICT |
|-------|----------------------------|----------------------------------|
| **A3** | May run for **harness continuity**; Executive retrieval **ignored** for STRICT evaluation | Not authoritative; kernel boundary is truth |
| **A4** | **Wired directly** to `e2StrictRetrieve()` — continuity pattern **retired** | Must not be invoked on happy path |
| **A5** | (A4 unchanged) | **Any reach = architectural regression** — guard enforces hard fail |

**No runtime carve-out** for pre-A4 A3 continuity. After A4 lands, heuristic retrieval under STRICT is always a wiring defect — A5 makes that defect **observable and fatal**.

#### A5.0a — Two failure domains (intentional)

| Domain | Checkpoint | Behavior | Example statuses / signals |
|--------|------------|----------|----------------------------|
| **Operational retrieval failure** | A3+ kernel / boundary | Expected typed outcomes within STRICT lab function | `FAILED_RETRIEVAL`, `FAILED_PROVENANCE`, `FAILED_STRICT_BOUNDARY` |
| **Architectural invariant violation** | A5 runtime guard | Broken wiring — **not** a retrieval result | Hard abort/throw; `LINK:RUNTIME_HEURISTIC` |

These domains are **deliberately different**. Guard trips are **not** retrieval failures and must not be mapped to `FAILED_*` arm scoring as if the kernel ran.

#### A5.0b — Runtime guard contract (architectural)

| Contract element | Specification |
|------------------|---------------|
| **Where** | At heuristic retrieval **entry point(s)** — minimum: `RAGPipeline::retrieveRelevant` (`rag.cpp`); additional sites only if protocol documents reachable leakage |
| **What it inspects** | **Authoritative execution/evaluation context** — propagated `E2EvalConfig::tier` (or equivalent plan/arm context object) |
| **How STRICT is identified** | `E2EvalTier::STRICT` on that authoritative context — same signal A4 dispatch uses |
| **Env vars** | **`THOTH_*` alone are never authoritative** — may exist for harness convenience; must not arm guard without context tier |
| **On violation** | Immediate hard failure — **detect only**; no redirect to kernel, no repair, no partial heuristic results |
| **On NON-STRICT** | Guard **silent** — heuristic retrieval proceeds unchanged |

No implementation detail beyond this contract in the checkpoint plan.

#### A5.0c — Signal precedence

| Layer | Role |
|-------|------|
| **Compile-time** (`e2_eval_kernel`, no `rag.cpp` in kernel TUs) | Determines **available capabilities** — heuristics not in eval kernel |
| **Runtime context** (`E2EvalConfig::tier` on execution context) | Determines **requested retrieval mode** for this arm/plan/step |
| **Contradictory combination** (STRICT context + heuristic entry reached) | **Architectural configuration error** — fail closed via guard; not silent fallback |

Compile-time and runtime answer **different questions**. Both must agree for a healthy STRICT run; mismatch at heuristic entry is A5's domain.

#### A5.0d — Why A5 is not redundant with A4

| Layer | Purpose | A5 relationship |
|-------|---------|-----------------|
| **Compile-time exclusion** | Prevent incorrect binary composition | Unchanged by A5 |
| **Static dispatch audit (A4)** | Verify intended dispatch path | A5 catches **bypass/regression of** A4 at runtime |
| **Runtime guard (A5)** | Detect architectural regressions **during execution** | Independent fuse — same philosophy as protocol § Enforcement philosophy |

A4 proves the happy path is wired correctly. A5 ensures a later change cannot **silently** route STRICT into heuristics without detection.

#### A5.0e — Core invariants (preserve all prior strengths)

1. **Diagnostic only** — never redirects, repairs, or substitutes execution.  
2. **No silent fallback** — STRICT uses `e2StrictRetrieve()` or fails immediately; never degrades to heuristic retrieval.  
3. **Guard ≠ success path** — golden A4 runs complete **without** guard violation (observable — see A5.4).  
4. **NON-STRICT preserved** — heuristic retrieval still works when tier is not STRICT (E2-11 smoke).  
5. **A2 + A3 + A4 invariants preserved** — guard is additive only.  
6. **`official_scoring: false`**, no `e2_outcome`.

#### A5.0f — Blast radius (architectural)

**A5 is the first Phase A checkpoint that modifies production heuristic retrieval (`rag.cpp`).**

Therefore:

- Change must be **intentionally minimal** — only the runtime guard contract (A5.0b).  
- **No other retrieval behavior** should change under NON-STRICT.  
- Review focus: **isolation of the guard** — not retrieval algorithm, ranking, or Executive dispatch.

#### A5.1 — Implementation scope (minimal)

Add guard contract (A5.0b) at documented entry point(s) only. Minimal context plumbing **only if** A4 did not already propagate authoritative tier to heuristic layer. **Do not** re-open A4 dispatch design.

#### A5.2 — Harness branch

```
THOTH_E2_WIRING_STAGE unset → default "A5" (after A5 lands)

A4:  executive → e2StrictRetrieve(); equivalence proven; A3 continuity retired
A5:  same as A4 + guard live
     NO evaluateEpisodicLearningCase; NO e2_outcome
SCORING: legacy dev knob — never authoritative
```

#### A5.3 — Gate contract (harness)

| Field | A5 value |
|-------|----------|
| `official_scoring` | `false` |
| `scoring_enabled` | `false` |
| `retrieval_enabled` | `true` |
| `evaluation_boundary_verified` | `true` |
| `executive_strict_retrieval` | `true` (from A4) |
| `runtime_heuristic_guard` | `true` |
| `wiring_stage` | `"A5"` |
| `e2_outcome` | **not emitted** |

#### A5.4 — E2-11 and observable golden regression

| Category | Asserts | Observable success criterion |
|----------|---------|------------------------------|
| **STRICT miswire (positive)** | Intentional heuristic invocation under STRICT eval context | Process **hard-fails** (abort/throw) — test catches failure; no heuristic chunks returned |
| **NON-STRICT heuristic works** | Heuristic path without STRICT context | Retrieval **completes with results** — not merely absence of abort |
| **Golden regression (falsifiable)** | E2-01–E2-10 on correct A4 wiring | Tests **exit 0 / pass** — normal completion **without** guard violation. Guard trip would cause hard fail → test failure. **No new spies/counters required** unless already present |
| **No silent fallback** | Review + negative STRICT miswire test | STRICT never returns heuristic chunks without guard or kernel |

**Does not assert:** lift, `e2_outcome`, benchmark authority, exhaustive production coverage.

#### A5.5 — Exit criteria (stop before Phase B)

1. Build green  
2. **E2-11** passes (STRICT miswire hard-fail + NON-STRICT heuristic smoke)  
3. **Prior regression suite green** — E2-01–E2-10 + A4 equivalence tests  
4. **Golden STRICT regression completes without guard trip** — observable as normal test pass (falsifiable: trip → hard fail → suite red)  
5. **Runtime guard rejects** intentional heuristic invocation under STRICT context  
6. **No behavioral changes outside the guard** — NON-STRICT heuristic retrieval unchanged (review + E2-11)  
7. A3→A4 transition documented; **no carve-out** for retired A3 continuity  
8. Signal precedence + failure domains documented (`E2_PROTOCOL.md` + this section)  
9. A2 + A3 + A4 invariants preserved  
10. **Pause for confirmation** — Phase B re-baseline next  

**Time estimate:** **30–60 minutes**.

#### A5 files (expected)

`rag.cpp` (guard only), minimal context plumbing if required, `tests/unit_tests.cpp`, `docs/E2_PROTOCOL.md`, `cursor_list.md` — **no** changes to `e2StrictRetrieve`, executive dispatch, scoring loop, or non-guard heuristic behavior

### Phase B — Failure semantics + official re-baseline (approved — subject to revision, **v4**)

**Expert verdict (v2):** Phase A built **control** (kernel, dispatch, guard). Phase B must build **meaning** — stop losing semantics when data flows runtime → arm → outcome. **Do not** treat Phase B as “flip `official_scoring: true`” until taxonomy is fixed; `e2_outcome` today is a post-hoc binary collapse, not a semantic aggregator.

**Phase B milestone (v2):** Clean retrieval quality (arm layer) + clean architecture violation signal (block layer) + clean benchmark signal (scorable-only outcome layer). Stop mixing **system integrity failures** with **model performance failures**.

**One-sentence definition:** Phase B **canonicalizes failure semantics** across three layers, then runs the first **authoritative** STRICT re-baseline under `official_scoring: true`.

**Explicitly not Phase B:** kernel ranking redesign; A4 dispatch changes; A5 guard behavior changes; INTEGRATION tier harness (**Phase C**); new retrieval algorithms.

---

#### B.0 — Problem statement (why Phase B exists)

| Layer | Role | Status after A5 |
|-------|------|-----------------|
| **Layer 1 — Runtime truth** | A5 guard + RAG execution behavior | ✅ Solid |
| **Layer 2 — Arm scoring** | `E2ArmScoringStatus` (`OK` / `FAILED_*`) | ✅ Structured, usable for cognitive-quality failures |
| **Layer 3 — Evaluation truth** | `e2_outcome` + summary rollup | ❌ Binary sink — **meaning leaks upward** |

**Current contamination risks:**

1. **`LINK:RUNTIME_HEURISTIC` is an orphan signal** — runtime architectural violation; **not** retrieval failure, **not** arm status, **not** scored failure class. Today it throws/catches as generic exception text; evaluator may flatten to `FAILED_RETRIEVAL`, missing-case noise, or generic `FAILURE`.
2. **Evaluator runs correctness math on incomplete labels** — `warm_retrieval_hit`, lift, provenance checks assume operational arm labels. Guard trips are **out of namespace** → benchmark can treat architectural violation as “bad retrieval.”
3. **`scoring_block_reason` is documented (`E2_PROTOCOL.md`) but not implemented** — no structured block layer in code.

**Phase B core obligation:** **Canonicalize failure semantics** — not “add scoring.”

---

#### B.0a — Three-axis model (missing bridge)

| Axis | Namespace | Examples |
|------|-----------|----------|
| **Arm status** | `E2ArmScoringStatus` | `OK`, `FAILED_RETRIEVAL`, `FAILED_PROVENANCE`, `FAILED_STRICT_BOUNDARY` |
| **Run block** | **`E2RunBlockReason` (new)** | `RUNTIME_HEURISTIC_GUARD`, wiring/provenance blocks |
| **Outcome resolution** | **`E2EvaluationResolution` (new)** | `SCORED_SUCCESS`, `SCORED_FAILURE`, **`NOT_SCORABLE`** |

**`LINK:RUNTIME_HEURISTIC` belongs on axis 2 (run block), never on axis 1.**

**Arm status boundary rule (v2 — enforce both directions):**

| Rule | Meaning |
|------|---------|
| **Do not** add `RUNTIME_HEURISTIC` (or any guard/block signal) to `E2ArmScoringStatus` | Block layer owns structural aborts |
| **Arm status reflects only within-system execution quality** | What happened **inside** the evaluation contract after a valid run started — not pre-execution aborts, wiring gates, or “why the evaluation was invalid” |

**Forbidden contamination paths (explicit):**

- guard trip → `FAILED_STRICT_BOUNDARY` ❌
- guard trip → `FAILED_RETRIEVAL` ❌
- wiring gate → any `FAILED_*` arm status ❌

Arm status answers: *“Did retrieval/provenance/lift fail under a scorable run?”*  
Run block answers: *“Was this run invalid for scoring at all?”*

Two failure domains (inherit A5.0a):

| Domain | Layer | Phase B handling |
|--------|-------|------------------|
| **Operational retrieval failure** | Arm | `FAILED_*` → eligible for `SCORED_FAILURE` after block check |
| **Architectural invariant violation** | Run block | `E2RunBlockReason::*` → **`NOT_SCORABLE`** — never `SCORED_FAILURE` |

---

#### B.0b — Precedence (mandatory logic)

**Single source of truth (v2):** `evaluation_resolution` is the **canonical decision field**. All other outcome fields are **derived** or **export metadata** — never independent authorities.

**Block-first rule (v4 — enforced, not implied):** When `run_block_reason != NONE`, **`arm_status` MUST be ignored for scoring**. Arm inputs (`FAILED_RETRIEVAL`, `FAILED_PROVENANCE`, etc.) are **only valid scoring signals when `run_block_reason == NONE`**. Otherwise you reintroduce ambiguity between “retrieval failed” and “run was invalid.”

```
if run_block_reason != NONE
    evaluation_resolution = NOT_SCORABLE          ← canonical; arm_status NOT consulted
    e2_outcome = derived: absent or non-authoritative
    scoring_block_reason = metadata export of run_block_reason
    // arm_status may still be logged for diagnostics — MUST NOT affect resolution
else if any arm fail-closed (FAILED_*) or case table miss   ← only reachable when block == NONE
    evaluation_resolution = SCORED_FAILURE        ← canonical
    e2_outcome = derived: FAILURE
else if mean lift + table pass
    evaluation_resolution = SCORED_SUCCESS        ← canonical
    e2_outcome = derived: SUCCESS
```

**Arm validity constraint:** `FAILED_RETRIEVAL → SCORED_FAILURE` **only** when `run_block_reason == NONE`. Same for all `FAILED_*` arm statuses.

**Guard trip → NOT_SCORABLE, not FAILURE.** Prevents guard regressions from poisoning retrieval-quality baseline.

**Anti-drift rule:** No code path may set `e2_outcome` without first computing `evaluation_resolution`. JSONL fields are export layer only.

**One-direction dependency (v3 — mandatory invariant):**

Canonical resolution must be **pure** — computed **without reading `e2_outcome`**. Forbidden pattern: compute outcome first, then infer resolution from outcome state (silently re-collapses the separation).

```
if run_block_reason != NONE:
    evaluation_resolution = NOT_SCORABLE                    ← block branch; arm ignored
else:
    evaluation_resolution = f(arm_status, lift, table_gates) ← arm branch; block == NONE only
e2_outcome = g(evaluation_resolution)                       ← derived only
```

| Allowed | Forbidden |
|---------|-----------|
| resolution → outcome | outcome → resolution |
| block checked first; arm consulted **only** when `run_block_reason == NONE` | reading `arm_status` when `run_block_reason != NONE` |
| `FAILED_*` → `SCORED_FAILURE` only on non-blocked runs | `FAILED_RETRIEVAL` influencing resolution when run is blocked |
| `e2_outcome` set after resolution is final | any branch that reads `e2_outcome` while computing resolution |

Implement B3 as **`resolveEvaluation()`** with an explicit early return on block — arm fields must not appear in that branch (enforced in code review + unit test).

---

#### B.0c — Minimal schema additions (small, critical)

**1. `E2RunBlockReason` enum** (names TBD at implement — align protocol strings):

| Value | Maps from |
|-------|-----------|
| `NONE` | Normal scored path |
| `RUNTIME_HEURISTIC_GUARD` | `E2RuntimeHeuristicGuardViolation` / `LINK:RUNTIME_HEURISTIC` |
| `STRICT_BOUNDARY_VIOLATION` | Optional: harness/wiring gate (future) |
| `PROVENANCE_VIOLATION` | Optional: run-level block before arm rollup (if distinct from arm `FAILED_PROVENANCE`) |
| `WIRING_GATE` | Phase A checkpoint not satisfied (e.g. `WIRING:A2`) |

**2. Attach to evaluation context:**

- `EpisodicLearningRunContext` or fields on `EpisodicLearningSummary`:
  - `run_block_reason`
  - `evaluation_resolution` (`SCORED_SUCCESS` | `SCORED_FAILURE` | `NOT_SCORABLE`)
  - `evaluation_resolution_detail` (human string; supersedes vague `outcome_rationale` for blocks)

**3. JSONL artifacts — export layer (derived from canonical resolution):**

| Field | Role | When |
|-------|------|------|
| **`evaluation_resolution`** | **Canonical truth** | Always on official runs |
| `scoring_block_reason` | Metadata view of `NOT_SCORABLE` | When `evaluation_resolution == NOT_SCORABLE` — e.g. `"LINK:RUNTIME_HEURISTIC"` |
| `e2_outcome` | Derived collapse | Only when `evaluation_resolution` is `SCORED_*` (`SUCCESS` / `FAILURE`) |
| `e2_outcome_detail` | Optional structured companion | Resolution + arm summary + block (debug/export; not a second authority) |

**Summary rollup metrics (v2 — NOT_SCORABLE must be visible, not silently dropped):**

| Metric | Definition |
|--------|------------|
| `scorable_cases` | Cases where `evaluation_resolution` is `SCORED_*` |
| `not_scorable_cases` | Cases where `evaluation_resolution == NOT_SCORABLE` |
| `not_scorable_by_reason` | Breakdown by `run_block_reason` (e.g. `RUNTIME_HEURISTIC_GUARD: 3`) |
| `success_rate` | Computed **over scorable cases only** — never diluted by block-layer violations |

**Do not** add `RUNTIME_HEURISTIC` to `E2ArmScoringStatus`. See **B.0a arm boundary rule**.

---

#### B.0d — Wiring: orphan signal → block layer

| Source | Phase B action |
|--------|----------------|
| `RAGPipeline::retrieveRelevant` guard throw | Catch at **harness/arm boundary** → set `run_block_reason = RUNTIME_HEURISTIC_GUARD`; **do not** map to `FAILED_RETRIEVAL` |
| `WorkflowEngine::executeRetrieval` typed catch | Catch **`E2RuntimeHeuristicGuardViolation`** → set **`StepResult.run_block_reason`** (**B2**); **not** plan JSON, **not** `FAILED_RETRIEVAL` |
| Missing executive STRICT step (miswire symptom) | Distinguish: operational `FAILED_RETRIEVAL` vs block if guard fired |

E2-11 remains guard unit test; add **E2-12** (or B-phase tests) for block → `NOT_SCORABLE` precedence.

---

#### B.1 — Checkpoint sequence (implement in order)

| Step | Work | Gate |
|------|------|------|
| **B1** | Add `E2RunBlockReason`, `E2EvaluationResolution`, serializers, JSON fields | Build green; no behavior change on A5 path |
| **B2** | Typed catch in `executeRetrieval` → `StepResult.run_block_reason`; **no plan/JSON inference** | See **§ B.2.0 v2**; case field stays `NONE` until B3 |
| **B3** | **`resolveEvaluation()`** + minimal transport + rollup | See **§ B.3.0 v4.1** — plumbing-first; `PlanStepOutcome` deferred post–Phase B |
| **B4** | JSONL export: `evaluation_resolution` (canonical), `scoring_block_reason` (NOT_SCORABLE metadata), derived `e2_outcome`, optional `e2_outcome_detail` | Protocol doc alignment; single-authority rule documented |
| **B5** | Harness `wiring_stage=B` only: `official_scoring: true`, full pins, shared scored loop, re-baseline run + fingerprint reproducibility gate | First authoritative `e2_outcome` only after B1–B4 green; **two identical runs** before B6 |
| **B6** | Golden baseline archive + Phase B completion gate — verification docs, fingerprint lock, immutable JSONL artifacts | **No runtime semantic changes**; pause before Phase C |

**Time estimate:** B1–B4 **2–4 hours**; B5 re-baseline run **1–2 hours**; B6 archive **~1–2 hours** (verification + docs only).

---

#### B.1.0 — B1 implementation plan (schema phase — **v2**)

**Scope:** Types, defaults, serializers, JSON field stubs only. **No precedence logic, no guard wiring, no harness branch changes.**

##### 1. Exact mutation entry points (strict order)

| Order | File | What changes | First symbol |
|-------|------|--------------|--------------|
| **1** | `external/basic_agent/include/episodic_learning_eval.h` | **Enums + struct fields only** — no logic | `enum class E2RunBlockReason` (insert after `E2ArmScoringStatus`, ~L64) |
| **2** | `external/basic_agent/include/episodic_learning_eval.h` | Second enum + forward declarations | `enum class E2EvaluationResolution` |
| **3** | `external/basic_agent/include/episodic_learning_eval.h` | Case-level carry fields | `EpisodicLearningCaseEvaluation::run_block_reason` (default `NONE`) |
| **4** | `external/basic_agent/include/episodic_learning_eval.h` | Summary rollup placeholders (zeroed defaults) | `EpisodicLearningSummary::{scorable_cases, not_scorable_cases}` + optional `evaluation_resolution` |
| **5** | `external/basic_agent/src/episodic_learning_eval.cpp` | **Serializers only** — first function added | `e2RunBlockReasonToString(E2RunBlockReason)` |
| **6** | `external/basic_agent/src/episodic_learning_eval.cpp` | Protocol string export (metadata, not arm) | `e2RunBlockReasonToProtocolString()` → e.g. `"LINK:RUNTIME_HEURISTIC"` for guard |
| **7** | `external/basic_agent/src/episodic_learning_eval.cpp` | JSON export stubs | extend `caseEvaluationToJson()` + new `summaryToJson()` **or** extend harness inline JSON — emit new keys with defaults only |
| **8** | `external/basic_agent/src/episodic_learning_eval.cpp` | **No-op stub for B2** (declared in header) | `e2RunBlockReasonFromException(const std::exception&)` → always `NONE` in B1 |
| **9** | `tests/unit_tests.cpp` | Serializer smoke + **NONE invariant** | `testE2B1BlockResolutionSchema` — enum round-trip, default struct values, **assert no production path emits non-`NONE` in B1** |

**First function modified (existing):** `caseEvaluationToJson()` in `episodic_learning_eval.cpp` (~L766) — add `"run_block_reason": "NONE"` key only; **do not** change field values from current runs.

**First data structure introduced:** `enum class E2RunBlockReason` in `episodic_learning_eval.h`.

##### Files explicitly unchanged in B1

| File | B1 rule |
|------|---------|
| `run_episodic_learning_benchmark.cpp` | **NO CHANGES** — guard catch + `run_block_reason` assignment is **B2** |
| `workflow_engine.cpp` | **NO CHANGES** — block capture hook lands **B2** at `executeRetrieval` catch (~L621) |
| `rag.cpp` | **NO CHANGE** (A5 guard frozen) |
| `summarizeEpisodicLearning()` | **NO LOGIC CHANGE** — B3 owns precedence |
| `evaluateEpisodicLearningCase()` | **NO LOGIC CHANGE** |
| `docs/E2_PROTOCOL.md` | **NO CHANGE in B1** — protocol alignment is **B4** |

##### 2. No-op safety rule (B1 gate — mandatory)

> **A5 behavior must not change under any input.**

Verification checklist after B1:

| Check | Expected |
|-------|----------|
| `THOTH_E2_WIRING_STAGE=A5` harness exit code | Same as pre-B1 |
| E2-01–E2-11 unit tests | All pass unchanged |
| `summarizeEpisodicLearning()` return values | Identical `outcome`, `mean_episodic_lift`, `passes` for same inputs |
| Guard throw path (`testE2RuntimeHeuristicGuard`) | Unchanged — still throws `E2RuntimeHeuristicGuardViolation` |
| New JSON fields on existing runs | Defaults only (`run_block_reason: "NONE"`); must not alter scoring branches |

B1 is a **schema phase, not a behavioral phase**. If any test outcome or harness exit code changes, B1 has leaked into B2/B3 scope — revert logic changes.

##### 2a — NONE-default invariant (guards against silent wiring bugs in B2/B3)

`run_block_reason` defaults to `NONE` everywhere in B1 — correct for a no-op schema phase. **Risk:** in B2/B3, forgetting to set block reason silently stays `NONE` → false scorable success (guard trip looks like clean retrieval).

**B1 invariant (mandatory — code comment + test):**

> **Any non-`NONE` `run_block_reason` in B1 is invalid unless explicitly assigned in B2 wiring.**

| Where | What to add in B1 |
|-------|-------------------|
| `EpisodicLearningCaseEvaluation::run_block_reason` field comment | `// B1: default NONE. Non-NONE only legal after B2 explicit assignment.` |
| `e2RunBlockReasonFromException()` stub | Document: returns `NONE` always in B1; B2 owns non-`NONE` |
| `testE2B1BlockResolutionSchema` | After a representative A5-path eval build, assert `run_block_reason == NONE` on every case result |
| B2 plan cross-ref | B2 must add a **positive test** that guard path sets non-`NONE` (E2-12) — B1 test only guards the negative (no accidental SET in schema phase) |

This does **not** change B1 behavior; it documents the contract so B2 cannot regress silently.

##### 2b — What B1 should feel like (sanity check)

After B1 lands:

| Property | Expected |
|----------|----------|
| System behavior | **Identical** to pre-B1 (A5 harness, unit tests, outcomes) |
| Data model | Has **hooks** (`E2RunBlockReason`, `E2EvaluationResolution`, carry fields, JSON keys) |
| Active semantics | **None** — block/resolution/precedence not consulted for scoring |

If anything “scores differently” or guard trips change classification, B1 scope was violated.

##### 3. Propagation path diagram (B2–B3 target — **not active in B1**)

Documents where block semantics will flow; B1 only reserves types/fields.

```
rag.cpp
  guardAgainstStrictHeuristicRetrieval()
    └─ throw E2RuntimeHeuristicGuardViolation  [A5 — frozen in B1]

workflow_engine.cpp  [B2 — hook site, not B1]
  executeRetrieval() catch (std::exception&)
    └─ StepResult.run_block_reason = RUNTIME_HEURISTIC_GUARD  [B2]

executive_controller.cpp  [B3 minimal transport]
  handle_step_completion()
    └─ PlanStep.run_block_reason = StepResult.run_block_reason  [single struct copy — NOT JSON]

run_episodic_learning_benchmark.cpp  [B3]
  runCaseArm() warm arm
    └─ E2CaseArmPlumbingResult.run_block_reason from PlanStep
    └─ EpisodicLearningCaseEvaluation.run_block_reason = warmArm only  [single source]

episodic_learning_eval.cpp  [B3]
  resolveEvaluation()  [new in B3]
    if run_block_reason != NONE → NOT_SCORABLE (arm ignored)
    else → f(arm_status, lift, table)
    e2_outcome = g(evaluation_resolution)
```

B1 ends at: **evaluator data model can represent NONE/SET** — nothing in this path writes SET yet.

##### 4. Schema detail (B1 enums + defaults)

**`E2RunBlockReason`** (align B4 protocol strings):

| Value | `toString()` | `toProtocolString()` (export metadata) |
|-------|--------------|----------------------------------------|
| `NONE` | `"NONE"` | *(omit or empty)* |
| `RUNTIME_HEURISTIC_GUARD` | `"RUNTIME_HEURISTIC_GUARD"` | `"LINK:RUNTIME_HEURISTIC"` |
| `WIRING_GATE` | `"WIRING_GATE"` | `"WIRING:…"` (B2+) |
| `STRICT_BOUNDARY_VIOLATION` | `"STRICT_BOUNDARY_VIOLATION"` | reserved |
| `PROVENANCE_VIOLATION` | `"PROVENANCE_VIOLATION"` | reserved |

**`E2EvaluationResolution`:**

| Value | Notes |
|-------|-------|
| `SCORED_SUCCESS` | B3 only |
| `SCORED_FAILURE` | B3 only |
| `NOT_SCORABLE` | B3 only |

B1: field may be `std::optional<E2EvaluationResolution>` unset on summary/case — **do not compute in B1**.

**Struct additions (defaults preserve A5 behavior):**

```cpp
// EpisodicLearningCaseEvaluation
E2RunBlockReason run_block_reason = E2RunBlockReason::NONE;
std::optional<E2EvaluationResolution> evaluation_resolution;  // unset in B1

// EpisodicLearningSummary
int scorable_cases = 0;
int not_scorable_cases = 0;
std::optional<E2EvaluationResolution> evaluation_resolution;  // unset in B1
// not_scorable_by_reason → B3/B4 (map) — defer unless trivial JSON stub needed
```

##### 5. Explicit do-not-touch list (B1 lock)

| Component | B1 rule |
|-----------|---------|
| `RAGPipeline::retrieveRelevant` | **NO CHANGE** |
| `guardAgainstStrictHeuristicRetrieval()` | **NO CHANGE** |
| `E2ArmScoringStatus` enum + all arm fail-closed logic | **NO CHANGE** |
| `e2StrictRetrieve()` / kernel ranking | **NO CHANGE** |
| `summarizeEpisodicLearning()` precedence / `E2Outcome` assignment | **NO CHANGE** |
| `evaluateEpisodicLearningCase()` pass/fail logic | **NO CHANGE** |
| Harness `wiring_stage` branches | **NO CHANGE** |
| `official_scoring` flag semantics | **NO CHANGE** |
| `WorkflowEngine::executeRetrieval` catch body | **NO CHANGE** (B2) |

**Prevents premature coupling:** B1 must not map guard → arm status, guard → `e2_outcome`, or block → `NOT_SCORABLE` (all B2/B3).

##### 6. B1 exit gate (before starting B2)

1. Build green (`cmake --build --preset build-debug`)  
2. `./build/debug/tests/thoth-unit-tests` — E2-01–E2-11 + new B1 schema test green  
3. A5 harness byte-identical outcomes (exit code + key metrics) vs pre-B1  
4. No `.cpp` changes outside `episodic_learning_eval.cpp` (+ `unit_tests.cpp`)  
5. **NONE invariant verified:** all case results on A5 path have `run_block_reason == NONE` (test assertion)  
6. **Pause for confirmation** — then B2 guard wiring  

**Status:** ✅ **B1 implemented** (2026-07-02). **Frozen — do not start B2 until instructed.**

##### 6a. B1 regression snapshot (frozen baseline before B2)

Recorded immediately after B1 landed; use to verify B2+ does not regress A5 behavior unintentionally.

| Artifact | Value |
|----------|-------|
| Parent git (Thoth) | `a8ae0f4` + **uncommitted B1 working tree** |
| Submodule (`external/basic_agent`) | `840ab0d` + **uncommitted B1 working tree** |
| Build | `cmake --build --preset build-debug` → **exit 0** |
| Unit tests | `./build/debug/tests/thoth-unit-tests` → **exit 0**, `All unit tests passed.` (includes **`testE2B1BlockResolutionSchema`**) |
| A5 harness | `THOTH_E2_WIRING_STAGE=A5 ./build/debug/external/basic_agent/run_episodic_learning_benchmark` → **exit 0** |
| Harness summary line | `wiring checkpoint complete — 3 case(s), equivalence=yes` |
| Harness fingerprint | `1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4` |
| Key JSONL flags (A5 row sample) | `wiring_stage=A5`, `runtime_heuristic_guard=true`, `official_scoring=false`, `scoring_enabled=false`, `harness_executive_retrieval_equivalent=true` |
| B1 schema on eval path | `run_block_reason=NONE` (test-enforced); `evaluation_resolution` **not emitted** when unset |

**B1 files touched:** `episodic_learning_eval.h`, `episodic_learning_eval.cpp`, `tests/unit_tests.cpp` only.

**Captured logs (local):** `/tmp/b1_unit_tests_snapshot.txt`, `/tmp/b1_a5_harness_snapshot.txt`

---

#### B.2.0 — B2 implementation plan (run-block capture wiring — **v3**)

**Goal:** **Strictly dumb propagation** — single switch at the workflow boundary: **typed exception → enum**. No plan inference, no JSON reconstruction, no string parsing, no secondary routing.

**Principle:** `B2 = event capture only` — **not** semantic analysis of plans, steps, or error text.

> **Mental model:** `exception → enum` — **not** `exception → plan → step → JSON → enum`

**Layer separation (do not blur):**

| Phase | Responsibility |
|-------|----------------|
| A5 | Runtime enforcement (throw) |
| **B2** | Event capture (typed catch → `run_block_reason`) |
| B3 | Semantic resolution (`NOT_SCORABLE`, precedence, case rollup) |

##### B2.0a — Inspected inputs (code map)

| Input | Location | B2 role |
|-------|----------|---------|
| **Runtime signal** | `E2RuntimeHeuristicGuardViolation` (`e2_strict_enforcement.cpp` ~L96) | **Only** mappable guard type in B2 |
| **Throw site** | `rag.cpp` → `guardAgainstStrictHeuristicRetrieval()` | **A5 frozen — NO CHANGE** |
| **Catch boundary (ONLY B2 edit)** | `WorkflowEngine::executeRetrieval` (~L621) | Typed catch + assignment |
| **Transport (B2 add)** | `StepResult::run_block_reason` (`workflow_engine.h`) | Dumb field; default `NONE` |
| **Case field (B1)** | `EpisodicLearningCaseEvaluation::run_block_reason` | **Stays `NONE` in B2** — B3 copies from transport |
| **Enum (B1)** | `E2RunBlockReason` | B2 sets only `RUNTIME_HEURISTIC_GUARD` or leaves `NONE` |

**When guard fires:** Heuristic `retrieveRelevant()` under active STRICT eval config. Happy A4/A5 kernel path uses `e2StrictRetrieve()` → guard not hit → § 6a harness unchanged.

##### B2.0b — Mapping rules (B2-only — one mapping, no inference)

**ONLY allowed mapping in B2:**

```
E2RuntimeHeuristicGuardViolation  →  RUNTIME_HEURISTIC_GUARD
everything else                   →  NONE (default)
```

**Explicitly forbidden in B2:**

| Forbidden | Why |
|-----------|-----|
| `e.what()` string contains `LINK:RUNTIME_HEURISTIC` | String parsing = interpretation |
| `runBlockReasonFromPlan(Plan)` | Plan-level semantic reconstruction |
| Scanning RETRIEVAL steps / `step.result` JSON | Cross-layer inference — **B3+ if ever** |
| `result.data["run_block_reason"]` JSON mutation | Plan persistence semantics — not B2 capture |
| Secondary routing via `e2RunBlockReasonFromException()` on generic catch | Soft reasoning in instrumentation layer |

**`e2RunBlockReasonFromException()`:** **Leave B1 stub** (always `NONE`) — guard mapping lives **only** in the typed catch. Do not extend with string fallback.

##### B2.0c — Exact mutation entry points (strict order)

| Order | File | Change |
|-------|------|--------|
| **1** | `workflow_engine.h` | Add `E2RunBlockReason run_block_reason = NONE` to **`StepResult`** |
| **2** | `workflow_engine.cpp` | **`executeRetrieval`**: typed catch + assignment (see B2.0d) |
| **3** | `unit_tests.cpp` | **E2-12** / **E2-13** / **E2-14** + update B1 schema test expectations |

**First function modified:** `WorkflowEngine::executeRetrieval` catch block (~L621).

**First data written:** `result.run_block_reason = RUNTIME_HEURISTIC_GUARD` (struct field — not plan JSON).

**NOT in B2:** `episodic_learning_eval.cpp` logic changes (optional `#include` only if `StepResult` needs enum in header — already via workflow_engine.h).

##### B2.0d — Workflow catch behavior (single catch site, single assignment)

**Default-at-construction rule (v3 — instrumentation integrity):**

- `StepResult::run_block_reason` defaults to **`NONE`** at struct construction (field initializer in `workflow_engine.h`).
- **Only override on typed guard match** — do **not** assign `NONE` in generic catch handlers.

**Why:** Explicit `result.run_block_reason = NONE` in `catch (const std::exception&)` or `catch (...)` can **silently erase** future typed block signals if new guard types are added later but not yet given dedicated catches ahead of the generic handler. Prefer **“absence of signal = NONE”** over **“explicit overwrite to NONE”**.

Replace guard handling with **one typed catch** before the existing generic catches:

```
StepResult result;   // result.run_block_reason == NONE by default

try {
    ... existing executeRetrieval body unchanged ...
} catch (const E2RuntimeHeuristicGuardViolation& e) {
    result.success = false;
    result.run_block_reason = RUNTIME_HEURISTIC_GUARD;   ← ONLY B2 override
    result.error_message = e.what();                     ← diagnostic text only
    // DO NOT set strict_retrieval_status / FAILED_RETRIEVAL
    // DO NOT write run_block_reason into result.data / plan JSON
} catch (const std::exception& e) {
    // run_block_reason unchanged (stays NONE unless a prior typed catch matched)
    result.success = false;
    result.error_message = std::string("Retrieval exception: ") + e.what();
} catch (...) {
    // run_block_reason unchanged
    result.success = false;
    result.error_message = "Retrieval unknown exception";
}
```

**Forbidden in generic catches:** `result.run_block_reason = NONE` (redundant and masks future typed handlers).

**Future guard types:** add **new typed catches above** the generic handler — each sets its own `E2RunBlockReason`; generic path must never clobber them.

**Guard triggers MUST NOT:** become `FAILED_RETRIEVAL`, mutate `E2ArmScoringStatus`, touch evaluator/scoring/resolution.

##### B2.0e — Propagation model (B2 scope — minimal)

```
rag.cpp: throw E2RuntimeHeuristicGuardViolation     [A5 — unchanged]
        ↓
WorkflowEngine::executeRetrieval typed catch       [B2 — result.run_block_reason = GUARD]
        ↓
StepResult returned to caller                      [B2 ends here]
        ↓
(plan persistence / case eval / resolution)        [NOT B2 — B3+]
```

**`EpisodicLearningCaseEvaluation::run_block_reason`:** remains **`NONE`** on all B2 production paths. B3 owns copying `StepResult` → case when building evaluation.

**Benchmark runner:** **NO CHANGES.** A5 snapshot § 6a must hold.

##### B2.0f — Removed from B2 (v1 mistakes — do not implement)

- ~~`runBlockReasonFromPlan(Plan)`~~
- ~~RETRIEVAL step scanning~~
- ~~`step.result` JSON block fields~~
- ~~`e.what()` heuristic fallback~~
- ~~`mergeRunBlockReasonForCase`~~
- ~~`scoring_block_reason` in StepResult.data~~
- ~~Extending `e2RunBlockReasonFromException()` beyond B1 stub~~

##### B2.0g — Strict do-not-touch list (B2 lock)

| Component | B2 rule |
|-----------|---------|
| `RAGPipeline::retrieveRelevant` / guard | **NO CHANGE** |
| `e2StrictRetrieve()` / kernel | **NO CHANGE** |
| `E2ArmScoringStatus` / arm fail-closed logic | **NO CHANGE** |
| `evaluateEpisodicLearningCase()` / `summarizeEpisodicLearning()` | **NO CHANGE** |
| `ExecutiveController` plan/step persistence | **NO CHANGE** (no new JSON keys for block) |
| `run_episodic_learning_benchmark.cpp` | **NO CHANGE** |
| `resolveEvaluation()` / `evaluation_resolution` | **B3** |
| Case-level `run_block_reason` population | **B3** (minimal `PlanStep` struct copy → warm arm → case — not JSON, not merge) |

##### B2.0h — Required tests

| ID | Asserts | How (B2-safe) |
|----|---------|---------------|
| **E2-12** | Guard miswire → `StepResult.run_block_reason == RUNTIME_HEURISTIC_GUARD` | Direct **`WorkflowEngine::executeRetrieval`** miswire (RAG active STRICT config, non-STRICT dispatch context) — read **`StepResult` return value** immediately; **no plan scan** |
| **E2-13** | Normal retrieval → `StepResult.run_block_reason == NONE` | Same API on happy STRICT kernel path **or** assert case eval still `NONE` on E2-01 (field untouched in B2) |
| **E2-14** | Happy path arm status unchanged | E2-01–03 regression — `arm_scoring_status` / provenance identical to pre-B2 |
| **Regression** | E2-01–E2-11 + `testE2B1BlockResolutionSchema` | `e2RunBlockReasonFromException(guard)` **still returns `NONE`** (stub unchanged); guard mapping tested via **typed catch / StepResult** only |
| **A5 harness** | § 6a snapshot | exit **0**, `equivalence=yes`, fingerprint unchanged |

##### B2.0i — No-op safety rule (relative to B1 snapshot § 6a)

> **A5 happy-path behavior must not change.** B2 adds instrumentation on guard catch only; no new semantics on case eval or harness.

| Check | Expected |
|-------|----------|
| A5 harness | Same exit code, fingerprint, `equivalence=yes` |
| E2-01–E2-11 | Pass unchanged |
| E2-11 direct `retrieveRelevant` throw | Still throws (uncaught at RAG — unchanged) |
| Case eval `run_block_reason` on happy paths | **`NONE`** (B2 does not populate case field) |

##### B2.0j — Exit criteria (B2 complete)

1. Build green  
2. `StepResult::run_block_reason` field added; defaults `NONE`  
3. Single typed catch in `executeRetrieval` maps **`E2RuntimeHeuristicGuardViolation` → `RUNTIME_HEURISTIC_GUARD`**  
4. Generic catch paths **do not mutate** `run_block_reason` (field stays default `NONE` unless typed guard catch matched)  
5. **No** plan/step JSON inference; **no** string parsing; **no** evaluator/harness/RAG changes  
6. **`e2RunBlockReasonFromException()` stub unchanged** (always `NONE`)  
7. E2-12, E2-13, E2-14 green; E2-01–E2-11 regression green  
8. A5 harness matches § 6a snapshot  
9. **Pause for confirmation** — then **B3** (case propagation + `resolveEvaluation()`)

**Time estimate:** **~1 hour** (smaller than v1 — fewer files)

**Files touched:** `workflow_engine.h`, `workflow_engine.cpp`, `tests/unit_tests.cpp` only.

**Status:** ✅ **B2 implemented** (2026-07-02). **B2.1 complete — paused before B3** (see **§ B.2.1k**).

##### B2.0k — B2 completion snapshot

| Check | Result |
|-------|--------|
| Unit tests | exit **0** — includes `testE2RunBlockReasonGuardCapture` (E2-12), `testE2RunBlockReasonHappyPathNone` (E2-13), `testE2RunBlockReasonArmStatusUnchanged` (E2-14) |
| A5 harness | exit **0**, `equivalence=yes` (matches § 6a behavior) |
| `e2RunBlockReasonFromException()` | B1 stub unchanged (always `NONE`) |
| Case eval `run_block_reason` on happy paths | **`NONE`** (B3 owns case propagation) |

**B2 files touched:** `workflow_engine.h`, `workflow_engine.cpp`, `tests/unit_tests.cpp` only.

---

#### B.2.1 — Pre-B3 stabilization plan (transport integrity — **v3**)

**Context:** B2 is implemented. Before B3 adds **interpretation**, lock transport integrity and freeze the meaning of `StepResult.run_block_reason`.

**Scope:** Stabilization only — **not** new features, normalization, or scoring.

##### Causal separation model (what this enforces)

This is not just a pipeline — it enforces **event ≠ observation ≠ evaluation** (same structure as evaluation frameworks, distributed tracing, formal benchmarking):

| Layer | Phase | Responsibility |
|-------|-------|------------------|
| **1 — Event source** | A5 | Throws exception (`E2RuntimeHeuristicGuardViolation`) |
| **2 — Observation** | B2 | Records event as **raw fact** on `StepResult` |
| **3 — Evaluation** | B3 | Interprets observation + arm status → `evaluation_resolution` |

**B2 must NEVER interpret whether the event matters.** Forbidden in B2/B2.1:

- fallback reasoning  
- semantic normalization  
- plan awareness  
- arm awareness  

B3 is the **first phase where interpretation begins.**

##### Semantic freeze (mandatory doc statement)

> **`StepResult.run_block_reason` is semantically frozen after B2 completion.**

Not necessarily `const` in code — but **meaning is locked**: later phases must not “quietly improve” transport for convenience (normalize `NONE`, infer missing blocks, clean inconsistent step results).

| Rule | Meaning |
|------|---------|
| **B3 reads facts** | Consume `StepResult` as recorded |
| **B3 does not repair observations** | No normalization of bad B2 output |
| **B3 MUST NOT modify `StepResult`** | **Only consume it** — no in-place cleanup, backfill, or “fixup” on transport objects |

This prevents the common evaluation failure mode where resolution layers **rewrite instrumentation** for convenience.

##### Issue 1 — `executeStep` field forwarding (hidden coupling risk)

**Current state:** Two code paths touch the field:

| Path | Role |
|------|------|
| **Semantic write** | `executeRetrieval` typed catch → **only** site that may assign non-`NONE` |
| **Structural forward** | `executeStep`: copies `currentAttempt.run_block_reason → result` |

**Risk:** If `currentAttempt.run_block_reason` is ever derived, normalized, or default-filled upstream, `executeStep` becomes an **implicit transformation layer**.

**Safer invariant (v3):**

> **`executeStep` MUST NOT mutate `run_block_reason` in any semantic form.**

The copy is **pure field forwarding with no meaning** — not “pass-through logic,” not normalization.

**Single-assignment per invocation (v3 — tightens whitelist):**

> **`executeStep` must perform exactly one `run_block_reason` assignment per invocation** — forward the value after the attempt loop resolves, **not** separate success/failure branch writes.

**Why:** Allowing “success branch + failure branch” invites dual-write semantics later (e.g. success → `NONE`, failure → override).

```
ALLOWED (structural only — once per executeStep return path):
  result.run_block_reason = currentAttempt.run_block_reason;   // single site, after attempt outcome known

FORBIDDEN in executeStep:
  two branch-based assignments (success vs failure)
  assign any literal (including NONE)
  conditional assignment on run_block_reason
  branch / if on run_block_reason value
  merge, max, coalesce, or infer from error_message / arm / plan
  “helpful” reset after failed attempt
```

**B2.1 implementation note:** Refactor current dual-branch copies (~L310, ~L317) to **one forward** immediately before `return result` (or once after loop break), preserving behavior.

**Fix plan:**

| Step | Work |
|------|------|
| **B2.1a** | Single forward assignment + comment: `// B2 — one structural forward per invocation; no semantic mutation` |
| **B2.1b** | **`testE2RunBlockReasonWriteSiteAudit`** — see below |
| **B2.1c** | Confirm dispatcher catches in `executeStep` never mention `run_block_reason` |

##### Issue 2 — `NONE` semantics (B3 boundary)

**Construction default (B2):** `StepResult::run_block_reason = NONE` at struct init; no explicit overwrite in generic catches.

> **`NONE` = absence of signal** — not a classification, not “no processing happened,” not “scorable by default.”

**Additional B3 safety invariant (v3):**

> **`NONE` MUST NOT be interpreted as `SCORED_SUCCESS` without explicit arm confirmation.**

`NONE` only means “no block event was captured” — it does **not** imply pass, lift success, or scorable outcome. Resolution must still run the arm/lift/table path and only then derive success.

| Value | Meaning |
|-------|---------|
| `NONE` | No block event at observation boundary — **not** implicit success |
| non-`NONE` | Raw fact: guard event was captured |

**B3 hidden risk (watch for):** Normalize `StepResult`, infer missing blocks, treat `NONE` as implicit pass → **silent scoring inflation / pass-through bias**.

**B3 must:**

- **`NONE` → arm/resolution branch** (after mechanical copy only) — not `SCORED_SUCCESS` by default  
- **non-`NONE` → `NOT_SCORABLE`** (block-first; arm ignored)  
- **Never modify `StepResult`** — consume only  

##### B2.1d — Static audit test (`testE2RunBlockReasonWriteSiteAudit`)

Scan `workflow_engine.cpp` for **`run_block_reason`** assignments.

**Must enforce ALL of:**

| Rule | Reject if |
|------|-----------|
| Single semantic write | Any non-`NONE` literal outside typed `E2RuntimeHeuristicGuardViolation` catch |
| No conditional semantic writes | `if` / `?:` / `switch` on same line or block as `run_block_reason =` |
| No post-assignment reset | `NONE` assigned after non-`NONE` in same function (except struct default init) |
| No branch overwrites in `executeRetrieval` | Multiple semantic assignments outside catch + init |
| **Exactly one forward in `executeStep`** | More than one `run_block_reason =` assignment in `executeStep` body |
| No branch-differentiated forwards | Success vs failure branches both assigning (dual-write pattern) |
| No normalization helpers | Functions that “clean up” `run_block_reason` |

**Allowed assignment sites (whitelist — v3):**

1. `StepResult` default member init (implicit)  
2. `executeRetrieval` — typed guard catch → `RUNTIME_HEURISTIC_GUARD`  
3. `executeStep` — **exactly one** structural forward per invocation (not per branch)  

##### B2.1e — B3 preamble items (fold into B3 plan when written)

| Item | Constraint |
|------|------------|
| **B2.1f** | Causal separation + semantic freeze + **B3 must not modify `StepResult`** |
| **B2.1g** | `NONE` table + **no SUCCESS without arm confirmation** |
| **B2.1h** | Case field = mechanical copy from `StepResult` — no plan scan |
| **B2.1i** | E2-12 precedence tests (block-first, arm ignored when blocked) |
| **B2.1j** | Freeze statement in `docs/E2_PROTOCOL.md` § observation layer (optional one paragraph) |

##### Pre-B3 checkpoint (must pass before B3 implementation)

| # | Condition |
|---|-----------|
| 1 | Audit test green — whitelist + single forward in `executeStep` |
| 2 | `executeStep` — one structural forward per invocation; no semantic mutation |
| 3 | **`StepResult` semantically frozen** — B3 consume-only, no repair |
| 4 | **`NONE` = absence of signal** — not implicit success |
| 5 | Causal separation documented (event / observation / evaluation) |
| 6 | A5 harness + E2-01–E2-14 regression green after B2.1 |
| 7 | **Pause for confirmation** — then B3 |

**Estimated effort:** B2.1 implementation **~30–45 minutes** (single forward refactor + comments + audit test + freeze statement).

**Files (B2.1 only):** `workflow_engine.cpp`, `tests/unit_tests.cpp`, `cursor_list.md` (+ optional `docs/E2_PROTOCOL.md` paragraph).

**Expert assessment (v3):** Structurally correct; stabilization scoped; strict instrumentation model; B3 interpretation boundary isolated.

**Status:** ✅ **B2.1 implemented** (2026-07-02). **Paused before B3.**

##### B2.1k — B2.1 completion snapshot

| Check | Result |
|-------|--------|
| `executeStep` | Single structural forward per invocation (not dual-branch) |
| `executeRetrieval` | Typed guard catch — sole semantic write |
| `testE2RunBlockReasonWriteSiteAudit` | Green — 2 total assignments in `workflow_engine.cpp` |
| Unit tests + A5 harness | Regression green (post-implement) |
| `docs/E2_PROTOCOL.md` | Observation layer + semantic freeze paragraph added |

**Files touched:** `workflow_engine.h`, `workflow_engine.cpp`, `tests/unit_tests.cpp`, `docs/E2_PROTOCOL.md`, `cursor_list.md`

---

#### B.3.0 — B3 implementation plan (evaluation interpretation — **v4.1**)

**Goal:** Land **evaluation semantics** — **`evaluation_resolution = f(run_block_reason, arm_status)`** via `resolveEvaluation()`, precedence, rollup, and derived `e2_outcome`.

**Scope split (v4 discipline):**

| In B3 | Deferred |
|-------|----------|
| `resolveEvaluation()` + `evaluation_resolution` | `PlanStepOutcome` envelope refactor → **§ B.3.1 post–Phase B** |
| Block-first precedence (Rules A–D) | Full `StepResult` envelope merge (`error_message`, `latency_ms`, …) |
| `applyCaseEvaluationResolution()`, summary rollup | `PlanStep.to_json()` envelope serialization |
| **Minimal transport** only — **single struct copy** at existing merge site | JSON observation keys / rehydration / merge helpers |

**Why v4:** Keeps B3 about **interpretation**, not architecture cleanup. Lower risk, easier debug, same mathematical contract as v2/v3. Transport choice does **not** change evaluation semantics if values are identical.

**Mental model:**

```
B2 produces:   StepResult.run_block_reason
B3 transport:  PlanStep.run_block_reason  (= single struct copy at handle_step_completion — temporary)
B3 consumes:   CaseEvaluation.run_block_reason  (= warm arm single source)
B3 interprets: resolveEvaluation(block, arm) → evaluation_resolution → derived e2_outcome
```

##### Causal separation (frozen)

| Layer | Phase | Role |
|-------|-------|------|
| Event | A5 | Guard throws |
| Observation | B2 | `StepResult.run_block_reason` — **sole observation object** |
| **Transport** | **B3 (non-semantic, minimal)** | Survive `StepResult` across async executive completion |
| **Evaluation** | **B3** | `resolveEvaluation()` → **`evaluation_resolution`** |

##### B3.0a — Removed from v1 (DO NOT implement)

| Removed | Why |
|---------|-----|
| ~~`step.result["e2_observation_run_block_reason"]` JSON export~~ | Second transport layer |
| ~~`observationRunBlockReasonFromRetrievalStep()`~~ | Re-parsing serialized observation |
| ~~JSON rehydration into evaluation~~ | Breaks single observation guarantee |
| ~~`mergeCaseRunBlockReason(cold, warm)`~~ | Multi-source reconciliation |
| ~~`EpisodicLearningArmObservation::run_block_reason`~~ | Duplicate observation carrier |

##### B3.0b — Minimal transport (existing architecture)

**Problem:** Harness reads `Plan` after `execute_goal`; `StepResult` is ephemeral. Today `handle_step_completion` copies only `result.data` → `step.result` and drops `run_block_reason`.

**B3 fix (minimal):** One temporary native field on `PlanStep`:

```cpp
// plan.h — transport only; NOT evaluation state; refactor target → PlanStepOutcome (§ B.3.1)
E2RunBlockReason run_block_reason = E2RunBlockReason::NONE;
```

**Merge** (`handle_step_completion` — **sole write site**):

```cpp
step.result = result.data;
step.status = result.success ? StepStatus::SUCCESS : StepStatus::FAILED;
step.run_block_reason = result.run_block_reason;   // single struct copy — no parsing, NOT step.result JSON
```

**Copy chain:**

```
StepResult.run_block_reason
  → PlanStep.run_block_reason              [handle_step_completion — single struct copy]
  → E2CaseArmPlumbingResult.run_block_reason [warm RETRIEVAL step read]
  → CaseEvaluation.run_block_reason          [warm arm only — single source]
  → resolveEvaluation()                    [after semantics wired — see § B.3.0f]
```

| Rule | Meaning |
|------|---------|
| **Single observation value** | Same enum value end-to-end; transport is survival copy only |
| **Single source per case** | `eval.run_block_reason = warmArm.run_block_reason` — **no merge** |
| **No JSON observation keys** | Never write block reason into `step.result` |
| **Unit tests** | E2-15–17 may set `eval.run_block_reason` or read `StepResult` directly — no harness |

**Helper (harness):** `runBlockReasonFromPlan(const Plan& plan)` — locate warm RETRIEVAL step, return `step.run_block_reason` (struct field — not JSON parse).

##### B3.0c — B3 core (evaluation semantics)

**Functions** (`episodic_learning_eval.h/.cpp`):

```cpp
E2EvaluationResolution resolveEvaluation(E2RunBlockReason run_block_reason,
                                       E2ArmScoringStatus arm_status);

E2ArmScoringStatus caseArmStatusForResolution(
    const EpisodicLearningArmObservation& cold,
    const EpisodicLearningArmObservation& warm);

void applyCaseEvaluationResolution(EpisodicLearningCaseEvaluation& eval);

E2Outcome deriveE2OutcomeFromResolution(E2EvaluationResolution resolution, ...);
```

**Precedence (block-first — canonical):**

```
if run_block_reason != NONE  → NOT_SCORABLE   // arm NOT read
if arm is any FAILED_*       → SCORED_FAILURE
else                         → SCORED_SUCCESS
```

**Rules A–D:** Block dominates · `NONE` ≠ success · B3 read-only on B2 · two inputs only · **`resolveEvaluation()` never reads `e2_outcome`**.

**One-direction dependency:**

```cpp
eval.evaluation_resolution = resolveEvaluation(eval.run_block_reason, arm_status);
eval.e2_outcome = deriveE2OutcomeFromResolution(eval.evaluation_resolution, ...);  // derived only
```

##### B3.0d — Integration point (per case)

```
1. evaluateEpisodicLearningCase() — table/lift UNCHANGED
2. eval.run_block_reason = warmArm.run_block_reason   // single source (from minimal transport)
3. applyCaseEvaluationResolution(eval)
4. summarizeEpisodicLearning() — scorable/not_scorable rollup + derived e2_outcome
```

##### B3.0e — Hard constraints (DO NOT)

- Modify RAG / A5 / B2 capture / `executeStep`  
- JSON export or rehydration of `run_block_reason`  
- `mergeCaseRunBlockReason` or cold/warm block reconciliation  
- String parsing / heuristic inference of block reason  
- Modify or "repair" `StepResult`  
- Treat `NONE` as implicit success  
- **`PlanStepOutcome` in B3** — defer to § B.3.1  

##### B3.0f — Implementation order (plumbing before integration tests)

**Principle:** End-to-end resolver tests need observation at `CaseEvaluation` — land transport first (no scoring behavior change), then semantics, then integration.

| Phase | Order | Work | Behavior change? |
|-------|-------|------|------------------|
| **A — Transport** | **1** | `plan.h` — `PlanStep.run_block_reason` (default `NONE`) | No |
| | **2** | `executive_controller.cpp` — **single struct copy** in `handle_step_completion` | No |
| | **3** | `run_episodic_learning_benchmark.cpp` — `E2CaseArmPlumbingResult.run_block_reason`; warm-arm → `eval.run_block_reason` | No — field populated; **`evaluation_resolution` not consulted yet** |
| | **4** | `episodic_learning_eval.cpp` — `runBlockReasonFromPlan()` helper | No |
| | **5** | `tests/unit_tests.cpp` — **E2-18** transport merge smoke test | No |
| **B — Semantics** | **6** | `episodic_learning_eval.h/.cpp` — `resolveEvaluation()`, `caseArmStatusForResolution()` (pure; no side effects) | No until wired |
| | **7** | `tests/unit_tests.cpp` — **E2-15, E2-16, E2-17** — resolver in isolation | Tests resolver only |
| **C — Wire + derive** | **8** | `applyCaseEvaluationResolution()`, `deriveE2OutcomeFromResolution()` | Yes — first scoring semantics |
| | **9** | `summarizeEpisodicLearning()` — rollup (`scorable_cases`, `not_scorable_cases`, derived `e2_outcome`) | Yes |
| | **10** | Call `applyCaseEvaluationResolution()` in case assembly path | Yes |
| **D — Integration** | **11** | `tests/unit_tests.cpp` — **E2-12** integrated path (transport + resolver + precedence) | Yes |
| | **12** | Full regression — E2-01–E2-14, B2.1 audit, A5 harness | Verify no unintended drift |

**Do not touch:** `workflow_engine.cpp` B2 capture, `rag.cpp`, `step.result` JSON schema.

##### B3.0g — Tests (run in phase order)

| Phase | ID | Asserts |
|-------|-----|---------|
| Transport | **E2-18** | `handle_step_completion`: `StepResult` → `PlanStep.run_block_reason` (**single struct copy**) |
| Resolver (isolated) | **E2-15** | GUARD + arm OK → `NOT_SCORABLE` |
| | **E2-16** | NONE + `FAILED_RETRIEVAL` → `SCORED_FAILURE` |
| | **E2-17** | NONE + arm OK → `SCORED_SUCCESS` |
| Integrated | **E2-12** | Case `run_block_reason=GUARD` + arm fail → `NOT_SCORABLE` |
| Regression | — | E2-01–E2-14, B2.1 audit, A5 harness |

##### B3.0h — Exit criteria

1. **`resolveEvaluation()`** — deterministic, block-first, 2 inputs only  
2. **`evaluation_resolution`** canonical; `e2_outcome` derived only  
3. **Minimal transport** — **single struct copy**; no JSON; no merge; warm arm single source  
4. E2-18 → E2-15/16/17 → E2-12 green; full regression green  
5. Rollup metrics populated (`scorable_cases`, `not_scorable_cases`)  
6. **Pause for review before B4** — confirm checkpoint; then JSONL export + re-baseline

**Estimate:** **2–3 hours** · **Files:** `episodic_learning_eval.*`, `plan.h`, `executive_controller.cpp`, `run_episodic_learning_benchmark.cpp`, `tests/unit_tests.cpp`

---

#### B.3.1 — Planned architectural refactor (post–Phase B — **not B3**)

**Trigger:** After B6 golden re-baseline green and Phase B exit gate (§ B.5) confirmed.

**Goal:** Replace temporary `PlanStep.run_block_reason` with structured **`PlanStepOutcome`** envelope — complete the incomplete `StepResult` → `PlanStep` merge without changing evaluation semantics.

```cpp
struct PlanStepOutcome {
    E2RunBlockReason run_block_reason = E2RunBlockReason::NONE;
    // optional: error_message, latency_ms, final_retry_count
};
struct PlanStep {
    nlohmann::json result;       // domain payload only
    PlanStepOutcome outcome;     // execution envelope
};
```

| Property | Requirement |
|----------|-------------|
| **Semantic equivalence** | Bit-identical `run_block_reason` at case eval boundary vs B3 minimal transport |
| **Regression** | E2-18 adapted; full E2 suite + A5 harness green |
| **Scope** | Refactor/relocate field only — **no** `resolveEvaluation()` changes |
| **Serialization** | Optional `PlanStep.to_json()` envelope fields in same pass or later |

**Rationale recorded:** Correct long-term abstraction (payload vs envelope separation). Deferred from B3 to preserve A1–B2.1 discipline — semantics first, architecture cleanup after authoritative baseline.


---

#### B.4.0 — B4 implementation plan (JSONL export — **v1**)

**Goal:** Turn internal B3 truth into a stable external JSONL contract — **export-only**; no evaluation semantics changes.

| In B4 | Forbidden |
|-------|-----------|
| `caseEvaluationToJson()` / `episodicLearningSummaryToJson()` completeness | Modify `resolveEvaluation()` |
| Conditional `e2_outcome` (omit when `NOT_SCORABLE`) | New runtime persisted fields |
| `scoring_block_reason`, `not_scorable_by_reason`, `success_rate` | Harness inline case/summary JSON |
| `episodicLearningCaseLogRow()` / `episodicLearningSummaryLogRow()` | B3/B3.1 behavior changes |

**Invariant:** `e2_outcome` is never stored as source-of-truth — only derived at export time via `e2OutcomeForExport()`.

**Status:** ✅ **B4 implemented** (2026-07-02). **Paused before B5.**

##### B4 completion snapshot

| Check | Result |
|-------|--------|
| Export helpers | `e2OutcomeForExport()`, `notScorableByReasonMap()`, `successRateForExport()` |
| Unified serializers | Harness uses `episodicLearning*LogRow()` only — no inline case/summary JSON |
| Tests E2-20–E2-24 | Export contract green |
| `docs/E2_PROTOCOL.md` | Export layer + single-authority rule documented |

---

#### B.5.0 — B5 implementation plan (official harness switch — **v4**)

**Goal:** Establish the **single authoritative** STRICT checkpoint — `wiring_stage=B` — and produce the first official baseline run with reproducible evaluation fingerprint. B5 is a **protocol milestone**, not another evaluator redesign.

**One sentence:** Flip default harness to `B`, enforce that only `B` may emit `official_scoring: true`, run the shared scored loop twice with identical fingerprints, then pause before B6 archives the baseline.

##### Phase narrative (checkpoint story)

| Phase | Question |
|-------|----------|
| **A** | Can we trust execution? |
| **B** | Can we trust evaluation? |
| **C** | Can we trust integration? |
| **D** | Can we trust evolution? |
| **E** | Can we trust the science? |

B5 is where Phase B answers “yes” for the first time — under frozen protocol output.

##### Scope split

| In B5 | Deferred to B6 |
|-------|------------------|
| `wiring_stage=B` harness branch ( **`B` only — no `OFFICIAL` alias** ) | Golden re-baseline commit to `docs/benchmark_results.md` |
| Default `THOTH_E2_WIRING_STAGE` → `"B"` | Publication-ready baseline narrative |
| Official Harness Invariant + Protocol Freeze (documented) | Phase C (INTEGRATION tier) |
| Single scored-loop extraction (shared by B; A5 uses checkpoint envelope only) | CI nightly authoritative config |
| E2-25–E2-28 tests | Fingerprint archival as project record |
| First + second verification re-baseline runs | |

**Forbidden in B5:**

- `OFFICIAL` as a second wiring-stage identifier — protocol is `A5`, `B`, `C`, … only
- Modify `resolveEvaluation()` or B3/B4 semantics
- Duplicate scored-loop logic (A5 checkpoint vs B official must share one implementation)
- Treat `SCORING` or any non-`B` stage as authoritative
- Archive baseline in B5 (B6 only, after reproducibility gate)

##### B5.0a — Official Harness Invariant

> **Exactly one wiring stage may be authoritative.**

| Class | Stages |
|-------|--------|
| **Authoritative** | `B` **only** |
| **Non-authoritative** | `A1`–`A5`, `SCORING`, experimental, future dev branches |

**Rule:** No stage other than `B` may emit `official_scoring: true`. Harness must enforce at JSONL emission and `EpisodicLearningRunRecorder` emit sites.

Env contract: `THOTH_E2_WIRING_STAGE=B` — no alternate name.

##### B5.0b — Protocol freeze (beginning B5)

Beginning with B5, these become **immutable protocol output**:

| Field | Freeze |
|-------|--------|
| **`evaluation_resolution`** | Meaning frozen — `SCORED_SUCCESS` \| `SCORED_FAILURE` \| `NOT_SCORABLE` |
| **`e2_outcome`** | Derived export artifact only; semantics frozen at B4 rules |

Future checkpoints (Phase C+) may **extend metadata** but **may not redefine** the meaning of `evaluation_resolution` or `e2_outcome`. Phase C adds INTEGRATION tier behavior; it does not retroactively change STRICT B semantics.

##### B5.0b1 — Fingerprint dependency (immutability layering)

B5 introduces two immutability layers that must **not diverge conceptually**:

| Layer | What it guards |
|-------|----------------|
| **Protocol freeze** | `evaluation_resolution` meaning is immutable from B5 |
| **Reproducibility gate** | `fingerprint_hash` identical across two official runs |

**Dependency rule (mandatory):**

> Fingerprint stability is a **function of** evaluation stability — not a separate truth domain.

`evaluation_fingerprint_hash` is a deterministic function of:

- `strictConfig` (`e2_eval_config` — version pins, tier)
- retrieval corpus hash (`corpus_snapshot_id` / index hash)
- **`evaluation_resolution` output** (per-case + summary rollup classification)

**Hidden assumption (must hold for fingerprint gate to be meaningful):**

Corpus hash + retrieval stability are fully deterministic only when:

- embedding model is **frozen** (pinned in `e2_eval_config`)
- retrieval ordering is **stable** (no nondeterministic tie-breaking)
- vector/index tie-breaking is **deterministic** (STRICT kernel path)

If any of these are unpinned, fingerprint instability will **masquerade as** evaluation instability. Root-cause via diagnosis table below — do not default to “semantic drift.”

**Fingerprint mismatch diagnosis (mandatory reference):**

| # | Cause | How to detect | Action |
|---|-------|---------------|--------|
| **1** | **Config mismatch** | `e2_eval_config` / `strictConfig` diff between runs | Fix pins; re-run |
| **2** | **Corpus drift** | `corpus_hash` / `index_hash` diff | Re-index or pin corpus snapshot |
| **3** | **Retrieval nondeterminism** | Identical inputs, **different** retrieved chunk order or hit set | Fix ordering/tie-break; do not archive baseline |
| **4** | **Semantic drift** (real evaluation change) | Identical inputs + identical retrieval, **different** `evaluation_resolution` | Resolver/case assembly changed — baseline invalid |

Shortcut rules:

| Signal | Likely cause |
|--------|--------------|
| Fingerprint mismatch, **same** `evaluation_resolution` | #1, #2, or #3 — not semantic drift |
| Fingerprint match, **different** `evaluation_resolution` | **#4 semantic drift** — do not treat as config noise |
| Both differ | Start at #1–#3 before assuming #4 |

Do not treat fingerprint mismatches as config bugs when the underlying cause is semantic drift — and do not treat retrieval noise as semantic drift without ruling out #1–#3.

##### B5.0b2 — Retrieval canonicalization rule

> All retrieval outputs must be **sorted and normalized** before entering `runScoredEvaluationLoop()`.

STRICT kernel (`e2StrictRetrieve`) already ranks by score with `chunk_id` tie-break — this rule makes that requirement **explicit at protocol level** so fingerprint/evaluation layers do not mis-diagnose retrieval ordering drift as semantic drift (#3 vs #4).

Prerequisite for the three overlapping guarantees (structural, semantic, reproducibility) to remain consistent.

##### B5.0b3 — Three guarantees (must hold together)

| Guarantee | Layer | Enforcement |
|-----------|-------|-------------|
| **A — Structural** | Single `runScoredEvaluationLoop()`; **zero** stage branches inside | Source audit test |
| **B — Semantic** | `evaluation_resolution` authoritative; `SCORING` is config-only | Official Harness Invariant + resolver freeze |
| **C — Reproducibility** | E2-28 equivalence-class rule | Two-run gate before B6 |

These are consistent **only if** retrieval canonicalization (B5.0b2) holds before evaluation begins.

##### B5.0c — Structural invariant (scored loop)

> **Exactly one scored-loop implementation shall exist — enforced structurally, not by convention alone.**

| Rule | Meaning |
|------|---------|
| **Single entry point** | `runScoredEvaluationLoop(const ScoredLoopConfig& config)` (name TBD) — sole owner of case assembly, `applyCaseEvaluationResolution()`, `summarizeEpisodicLearning()`, JSONL emit |
| **Envelope-only branching** | `A5`, `B`, and `SCORING` differ only in **outer** `ScoredLoopConfig` envelope (`official_scoring`, `wiring_stage`, early-return policy) — **no** stage conditionals **inside** the loop |
| **Forbidden inside loop** | `if (wiringStage == …)` anywhere in loop body — forking is not the only risk; conditional logic inside shared code silently diverges semantics |
| **Enforcement** | Unit test or source audit: scored-loop function body contains **zero** `wiring_stage` / `wiringStage` references — this is the real invariant mechanism |

`A5` continues to return early after equivalence proof (checkpoint envelope). `B` calls `runScoredEvaluationLoop()` with `official_scoring: true`.

> **`SCORING` is a configuration of the evaluation loop, not an alternative evaluation loop.** It delegates to `runScoredEvaluationLoop()` with a non-authoritative envelope (`official_scoring: false`) — never a forked body, never alternate control paths inside the loop.

##### B5.0d — Official gate contract (live `B` run)

| Field | Value |
|-------|-------|
| `wiring_stage` | `"B"` |
| `official_scoring` | `true` |
| `scoring_enabled` | `true` |
| `scoring_tier` | `"STRICT"` |
| `evaluation_fingerprint` | From `computeEvaluationFingerprint(strictConfig)` — **must be reproducible** |
| `e2_eval_config` | Full pinned `E2EvalConfig` |
| **`evaluation_resolution`** | Canonical per case + run rollup (frozen output) |
| `e2_outcome` | Derived — only when `SCORED_*` |
| `scorable_cases` / `not_scorable_cases` | Always on official summary |
| `not_scorable_by_reason` / `success_rate` | Per B4 export rules |

##### B5.0e — Implementation order

| Phase | Order | Work |
|-------|-------|------|
| **A — Single loop** | 1 | Extract `runScoredEvaluationLoop(ScoredLoopConfig)` — **zero** `wiringStage` branches inside |
| | 2 | Add audit test: scored-loop body has no `wiring_stage` / `wiringStage` references |
| | 3 | A5 branch: early-return checkpoint envelope only — **must not** duplicate loop body |
| **B — B branch** | 4 | `wiringStage == "B"` only — official banner, flags, recorder |
| | 5 | Default unset env → `"B"` |
| | 6 | Assert/guard: only `B` path sets `official_scoring: true` on JSONL |
| **C — Recorder** | 7 | `EpisodicLearningRunRecorder::completeOfficial()` — resolution-aware via `e2OutcomeForExport()` |
| **D — Tests** | 8 | E2-25–E2-27 (see below) |
| | 9 | **E2-28** — scoped determinism (see B5.0f) |
| **E — Re-baseline** | 10 | Run 1: first authoritative baseline |
| | 11 | Run 2: verification — scoped fields match per E2-28 |
| **F — Docs** | 12 | `E2_PROTOCOL.md` — Official Harness Invariant + Protocol Freeze + Fingerprint dependency |
| | 13 | B5 completion snapshot in `cursor_list.md` |

**Time estimate:** ~1.5–2.5 h code + ~1–2 h two-run verification/analysis.

##### B5.0f — Tests

| ID | Asserts |
|----|---------|
| **E2-25** | `THOTH_E2_WIRING_STAGE=B` smoke: `official_scoring: true`, `scoring_enabled: true`, `wiring_stage: "B"`, `evaluation_resolution` present |
| **E2-26** | Golden trio under `B`: all `SCORED_SUCCESS`, derived `e2_outcome: SUCCESS`, `not_scorable_cases == 0` |
| **E2-27** | Explicit `A5`: `official_scoring: false`; no authoritative claims |
| **E2-28** | **Official determinism (scoped)** — run identical `B` benchmark twice; compare **only** the fields below; runs may differ elsewhere |
| **Regression** | E2-01–E2-24; A5 equivalence when `THOTH_E2_WIRING_STAGE=A5` |

**E2-28 comparison scope (mandatory):**

| **Included** (must match exactly) | **Excluded** (may differ) |
|-----------------------------------|---------------------------|
| `evaluation_resolution` per case + summary | `timestamp_ms`, `run_id` |
| `evaluation_fingerprint.fingerprint_hash` | Log line ordering |
| `e2_eval_config` (canonical JSON) | Debug / diagnostic metadata |
| Scorable classification (`scorable_cases`, `not_scorable_cases`, per-case resolution) | `e2_outcome_detail`, wall-clock fields |

**Equivalence class rule:** Reproducibility means membership in the same **equivalence class of evaluation outputs** — not raw JSONL byte equality. This prevents false negatives (ordering), false positives (timestamps), and CI noise failures.

**Equivalence class rule (E2-28):** Two runs are equivalent **iff**:

1. `evaluation_resolution` is identical (per case + summary)
2. E2-28 scoped fields match (`fingerprint_hash`, `e2_eval_config`, scorable classification)
3. Fingerprint mismatch diagnosis (buckets #1–#4) maps to the **same bucket** on both runs — the diagnostic system itself must be reproducible

Reproducibility = membership in the same **equivalence class of evaluation outputs** — not raw JSONL byte equality.

##### B5.0g — Exit criteria (stop before B6)

1. `wiring_stage=B` is default; **`OFFICIAL` alias does not exist**
2. Official Harness Invariant enforced — only `B` emits `official_scoring: true`
3. Structural scored-loop invariant — single `runScoredEvaluationLoop()`; **zero** `wiringStage` branches inside; audit test green
4. Protocol Freeze + Fingerprint dependency documented — layers linked, not conflated
5. E2-25–E2-28 green; full regression green
6. **First** authoritative re-baseline run completed; scoped fields verified
7. **Second** verification run: E2-28 scoped fields match (resolution + fingerprint + config + scorable classification)
8. A1–A5 invariants preserved when checkpoints explicitly selected
9. **Pause for review before B6** — baseline archive only after reproducibility gate (items 6–7)

**B6 gate:** One run proves it works. Two runs prove it's reproducible. Only then may B6 archive the golden baseline.

**Formal definition:** B5 establishes a **deterministic evaluation kernel** with a reproducibility gate, a structured failure taxonomy (diagnosis #1–#4), and strict separation between execution, retrieval, and semantic evaluation layers — a **measurement theory boundary**, not merely a checkpoint flag.

**Status:** ✅ **B5 implemented** (2026-07-04). B6 archived baseline (2026-07-04).

##### B5 completion snapshot

| Check | Result |
|-------|--------|
| Default `THOTH_E2_WIRING_STAGE` | `B` |
| `runScoredEvaluationLoop()` | Single implementation; structural audit green |
| Official Harness Invariant | Only `B` emits `official_scoring: true` |
| `SCORING` | Config envelope of same loop — not alternative |
| E2-25–E2-28 + structural audit | Green |
| Two-run `B` gate | Matching scoped fields (fingerprint + resolution + scorable rollup) |
| A5 explicit regression | `equivalence=yes` |

---

#### B.6.0 — B6 implementation plan (golden baseline archive — **v1**)

**Goal:** Archive the first authoritative Phase B baseline produced by B5, verify reproducibility evidence, and finalize Phase B as a **measurement-stable** evaluation system.

**One sentence:** B6 is an archival + verification checkpoint — lock the Phase B baseline, document reproducibility proof, and mark Phase B complete **without changing any runtime semantics**.

| In B6 | Forbidden |
|-------|-----------|
| Verification docs + immutable artifact archive | Modify `resolveEvaluation()` or scoring logic |
| Fingerprint lock + Phase B completion declaration | Modify retrieval logic or canonicalization |
| JSONL snapshot extraction (run #1 / #2) | Alter E2-25–E2-28 definitions |
| Optional CI hardening (B6.6) | Introduce new `evaluation_resolution` fields |

##### B6.0a — Inputs (must exist from B5 — hard prereq)

If **any** item missing → **FAIL B6** (do not proceed).

| # | Artifact / evidence |
|---|---------------------|
| 1 | B5 run #1 JSONL output (authoritative `wiring_stage=B` summary row) |
| 2 | B5 run #2 JSONL output (reproducibility run) |
| 3 | `evaluation_fingerprint.fingerprint_hash` (run #1) |
| 4 | `evaluation_fingerprint.fingerprint_hash` (run #2) — must match #3 |
| 5 | `e2_eval_config` snapshot (pinned `strictConfig`) |
| 6 | `corpus_hash` / `index_hash` snapshot |
| 7 | Scorable / not_scorable summary rollup |
| 8 | E2-25–E2-28 + structural audit — green |

**Note:** `logs/episodic_learning_benchmark.jsonl` is append-only. B6 must **extract and freeze** the two B summary rows into immutable artifact files — not assume a single-run log path.

##### B6.1 — Reproducibility verification (hard gate)

Compare run #1 vs run #2 using **E2-28 scoped equivalence only**:

| Must match | Must NOT fail on |
|------------|------------------|
| `evaluation_resolution` (per case + summary) | `timestamp_ms` |
| Scorable classification (`scorable_cases`, `not_scorable_cases`, per-case resolution) | `run_id` / UUIDs |
| `not_scorable_by_reason` breakdown | Log line ordering |
| E2-28 diagnostic bucket (same bucket #0 = equivalent) | Debug metadata |
| `fingerprint_hash` | File ordering |
| `e2_eval_config` (canonical JSON) | |

**Output:** `docs/baselines/phase_b_baseline_verification.md`

| Section | Content |
|---------|---------|
| Pass/fail matrix | Per scoped field |
| Fingerprint comparison | hash #1 vs #2 |
| Diagnosis confirmation | Buckets #1–#4 ruled out or resolved |
| Conclusion | **“Semantic equivalence confirmed”** (only if all gates pass) |

##### B6.2 — Golden baseline snapshot

**Output:** `docs/benchmark_results/phase_b_baseline_v1.md`

| Required section | Content |
|------------------|---------|
| B5 system summary | Short — measurement boundary, not checkpoint |
| `e2_eval_config` | Canonical JSON + hash |
| `corpus_hash` / `index_hash` | From B5 run attribution |
| `evaluation_fingerprint_hash` | Locked value |
| Rollup | `scorable_cases`, `not_scorable_cases`, `mean_episodic_lift` |
| Test evidence | E2-25–E2-28 summary (pass) |
| Consistency statement | *“Two-run reproducibility gate passed under E2-28 equivalence constraints.”* |

Cross-ref existing `docs/benchmark_results.md` archive entry if applicable — do not overwrite unrelated benchmark history.

##### B6.3 — JSONL archival

Copy or extract immutable artifacts to:

```
docs/baselines/artifacts/phase_b/
  run_01_summary.json      # authoritative B5 run #1 EPISODIC_LEARNING_SUMMARY
  run_02_summary.json      # B5 run #2 verification summary
  episodic_learning_benchmark_snapshot.jsonl   # optional full append-only excerpt
```

| Rule | Meaning |
|------|---------|
| Run #1 + #2 preserved | Frozen at B6 time |
| No post-B6 modification | Artifacts are read-only record |
| Source | Extract from `logs/episodic_learning_benchmark.jsonl` by `wiring_stage=B` summary rows |

##### B6.4 — Fingerprint lock declaration

**Output:** `docs/baselines/fingerprint_lock.md`

| Must define | Source |
|-------------|--------|
| `fingerprint_hash` derivation | B5.0b1 dependency chain |
| Dependency chain | `strictConfig` → `corpus_hash` → `evaluation_resolution` output |
| Diagnosis table | Buckets #1–#4 (config / corpus / retrieval nondeterminism / semantic drift) |
| Primary rule | *“Fingerprint is a derived artifact of evaluation stability, not a primary identifier.”* |

##### B6.4a — Baseline provenance

**Output:** `docs/baselines/BASELINE_PROVENANCE.md`

Archive **how** the baseline came to exist — not just what it is. Required for reproducibility years later.

| Field | Example |
|-------|---------|
| Baseline version | Phase B v1 |
| Created from | B5 implementation |
| Archived by | B6 |
| Git commit SHA | `e143efe` (or current HEAD at archive time) |
| Creation date | ISO date |
| Toolchain / compiler | `g++` version from build |
| Build configuration | Debug / Release preset |
| Protocol version | E2 Protocol revision (e.g. v1.2) |
| Notes | Optional freeform |

##### B6.5 — Phase B completion gate

**Output:** `docs/phases/PHASE_B_COMPLETE.md`

**Lead with Phase B Summary** (top of file):

> Phase B established the first authoritative evaluation baseline for Thoth. Evaluation semantics, export contracts, fingerprint derivation, and reproducibility requirements are now frozen. Future phases may extend evaluation metadata but may not redefine the meaning of `evaluation_resolution`, `e2_outcome`, or the Phase B fingerprint contract without creating a new protocol version.

| Required declaration | |
|------------------------|---|
| B5 → B6 transition summary | |
| Structural invariant satisfied | Single `runScoredEvaluationLoop()` |
| Semantic invariants satisfied | Protocol freeze; only `B` authoritative |
| Reproducibility gate passed | E2-28 two-run evidence |
| **Phase B complete statement** | *“Phase B is complete: evaluation is now reproducible and authoritative.”* |
| Layer roles post-B6 | **`B`** = baseline evaluation layer; **`A5`** = execution-only legacy compatibility; **`SCORING`** = non-authoritative configuration mode |

##### B6.6 — Optional CI hardening (non-breaking)

Only if trivial — **not required for B6 exit**:

| Item | Behavior |
|------|----------|
| Nightly regression | Uses `wiring_stage=B` config only |
| Fingerprint gate | Fail CI if two consecutive `B` runs mismatch E2-28 scoped fields |
| Structural audit | `testE2B5ScoredLoopStructuralAudit` in CI fast path |

##### B6.0b — Implementation order

| Order | Work | Runtime change? |
|-------|------|-----------------|
| 1 | Verify B6.0a inputs exist | No |
| 2 | Run E2-28 comparison script or manual matrix → `phase_b_baseline_verification.md` | No |
| 3 | Write `phase_b_baseline_v1.md` | No |
| 4 | Extract + copy JSONL artifacts → `docs/baselines/artifacts/phase_b/` | No |
| 5 | Write `fingerprint_lock.md` + `BASELINE_PROVENANCE.md` | No |
| 6 | Write `PHASE_B_COMPLETE.md` (with Phase B Summary lead) | No |
| 7 | Update `cursor_list.md` B6 snapshot; append `benchmark_results.md` index line | No |
| 8 | (Optional) B6.6 CI | Yes — harness/CI only |

**Time estimate:** ~1–2 hours (docs + artifact extraction only).

##### B6.0c — Exit criteria

B6 complete **only if**:

1. Golden baseline document exists (`phase_b_baseline_v1.md`)
2. Fingerprint lock documented (`fingerprint_lock.md`)
3. Two-run reproducibility proven and recorded (`phase_b_baseline_verification.md`)
4. JSONL archived immutable (`docs/baselines/artifacts/phase_b/`)
5. Phase B marked COMPLETE (`PHASE_B_COMPLETE.md`)
6. **No semantic changes** introduced in code
7. **Pause for confirmation** — Phase C (INTEGRATION tier) next

**Status:** ✅ **B6 complete** (2026-07-04). **Paused before Phase C.**

##### B6 completion snapshot

| Check | Result |
|-------|--------|
| B6.0a inputs | All present (two B runs, fingerprint, config, corpus hash, E2-25–E2-28 green) |
| `phase_b_baseline_verification.md` | PASS — E2-28 scoped equivalence |
| `phase_b_baseline_v1.md` | Golden baseline locked |
| `fingerprint_lock.md` | Fingerprint contract documented |
| `BASELINE_PROVENANCE.md` | Provenance archived (commit SHA, toolchain, protocol) |
| `PHASE_B_COMPLETE.md` | Phase B Summary + completion declaration |
| JSONL artifacts | `docs/baselines/artifacts/phase_b/` (run_01, run_02, snapshot) |
| Runtime semantic changes | None |
| Fingerprint | `1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4` |

---

#### C.0.0 — Phase C implementation plan (integration tier — **v1.1**)

**Authority:** [`docs/C_PHASE_PROTOCOL.md`](C_PHASE_PROTOCOL.md) v1.1 (locked for implementation)  
**Prerequisite:** Phase B complete — [`phases/PHASE_B_COMPLETE.md`](phases/PHASE_B_COMPLETE.md), baseline [`benchmark_results/phase_b_baseline_v1.md`](benchmark_results/phase_b_baseline_v1.md)

**Goal:** Integrate the E2 measurement system into the production cognitive architecture as a **passive service** — without changing agent behavior or Phase B evaluation semantics.

**One sentence:** Move E2 from benchmark harness into architecture while preserving Phase B reproducibility; prove path equivalence at close-out.

##### C0 — Integration boundary (informational — not coded)

Phase B contract is an **immutable dependency**. Tweaking evaluation during integration = protocol change (E2 v1.3+), not Phase C work. See `C_PHASE_PROTOCOL.md` § C0.

##### Governing invariant

> **Evaluation is a passive architectural service. It may observe execution, but it must never influence execution.**

All forbidden rules and checkpoint constraints derive from this. Dependency flow is **downward only**: Execution → Episode → Evaluation → Diagnostics → Consumers.

##### Checkpoint sequence (implement in order)

| Step | Work | Gate |
|------|------|------|
| **C1** | Evaluation service boundary — extract interface; benchmark becomes thin caller | `wiring_stage=B` unchanged; E2-25–E2-28 green |
| **C2** | Episode publication — Executive emits `EpisodeCompleted`; no direct eval import | Flag OFF → behavior identical; flag ON → INTEGRATION envelope only |
| **C3** | Diagnostic layer — presentation only (JSONL); ownership: execution/eval/diagnostics | E2-06 green; no second `evaluation_resolution` authority |
| **C4** | Architectural telemetry — segregated from benchmark metrics | No benchmark fields in telemetry schema |
| **C5** | Production validation — **path equivalence** + `PHASE_C_COMPLETE.md` | Same episode → same `evaluation_resolution` (benchmark vs production paths) |

**Time estimate:** C1–C2 **4–6 h**; C3 **4–8 h**; C4 **4–6 h**; C5 **2–4 h**.

##### Scope split

| In Phase C | Forbidden |
|------------|-----------|
| Service extraction, episode events, diagnostics JSONL, telemetry schema | Change `evaluation_resolution` / `e2_outcome` semantics |
| Feature flag (default OFF) | Redefine fingerprint or diagnosis buckets |
| Path equivalence validation | `official_scoring: true` outside `wiring_stage=B` |
| `PHASE_C_COMPLETE.md` close-out | Dashboards, UI, visualization, reporting |
| INTEGRATION tier harness (E2-06) | Mix benchmark metrics with architectural telemetry |
| | Evaluation modifying planning, retrieval, memory, prompts |

##### Regression gates (every checkpoint)

| Gate | Requirement |
|------|-------------|
| Unit tests | E2-25–E2-28 + C-phase tests (E2-C1-01 … E2-C5-02) as added |
| STRICT benchmark | `THOTH_E2_WIRING_STAGE=B` — fingerprint `1ce31c6a…` stable |
| E2-28 | Two-run scoped equivalence vs Phase B v1 baseline |
| Structural audit | `testE2B5ScoredLoopStructuralAudit` green |
| Passive invariant | No evaluation callback into execution |

##### C5 acceptance criterion (path equivalence)

```
Episode E → Benchmark path       → evaluation_resolution = R
Episode E → Production service   → evaluation_resolution = R
```

Same episode. Same result. Different integration path.

##### Implementation order

| Order | Work | Runtime change? |
|-------|------|-----------------|
| 0 | Protocol v1.1 locked ✅ | No |
| 1 | C1 — Evaluation service boundary | Refactor only |
| 2 | C2 — Episode publication | Additive — default OFF |
| 3 | C3 — Diagnostic layer | Additive — JSONL only |
| 4 | C4 — Architectural telemetry | Additive |
| 5 | C5 — Path equivalence + close-out | Validation only |

**Status:** 📋 **Planned** — protocol locked; **paused before C1**. No code until C1 explicitly opened.

##### Phase narrative (context)

| Phase | Question |
|-------|----------|
| A | Can execution be trusted? ✅ |
| B | Can measurement be trusted? ✅ |
| **C** | Can trusted measurement become architecture? |
| D | Can architecture evolve without losing trust? |
| E | Can we defend the results scientifically? |

---

#### B.2 — Harness branch (after B1–B4)

```
THOTH_E2_WIRING_STAGE unset → default "B" (after Phase B lands)

A5:  kernel + guard; no official scoring
B:   official_scoring: true; evaluation_resolution emitted; e2_outcome only when SCORED_*
     run_block_reason / scoring_block_reason when NOT_SCORABLE
SCORING: configuration of same scored loop — dev envelope only; never authoritative
```

#### B.3 — Gate contract (official run)

| Field | Phase B value |
|-------|----------------|
| `official_scoring` | `true` |
| `scoring_enabled` | `true` |
| **`evaluation_resolution`** | **Canonical** per case + run rollup: `SCORED_SUCCESS` \| `SCORED_FAILURE` \| `NOT_SCORABLE` |
| `scoring_block_reason` | Metadata export when `NOT_SCORABLE` (not a second authority) |
| `e2_outcome` | **Derived** — emitted **only** for `SCORED_*` |
| `scorable_cases` / `not_scorable_cases` | Rollup visibility — `NOT_SCORABLE` counted explicitly |
| `success_rate` | Scorable-only denominator |
| `wiring_stage` | `"B"` |

---

#### B.4 — Tests

| ID | Asserts |
|----|---------|
| **E2-01–E2-11** | Regression (unchanged) |
| **E2-12** | Guard transport → case `NOT_SCORABLE` even if arm `FAILED_RETRIEVAL` |
| **E2-13** | `NONE` + `FAILED_RETRIEVAL` → `SCORED_FAILURE` |
| **E2-14** | Happy path arm status unchanged (B2 wiring) |
| **E2-15** | `GUARD` + arm OK → `NOT_SCORABLE` (B3 resolver) |
| **E2-16** | `NONE` + `FAILED_RETRIEVAL` → `SCORED_FAILURE` |
| **E2-17** | `NONE` + arm OK → `SCORED_SUCCESS` |
| **E2-18** | `handle_step_completion` transport merge: `StepResult` → `PlanStep.run_block_reason` |
| **E2-14** | Full golden trio under official config → `SCORED_SUCCESS` + authoritative derived `e2_outcome`; **additionally assert:** `not_scorable_cases == 0` **OR** explicitly documented non-zero count with `not_scorable_by_reason` breakdown (baseline must not hide guard activity by excluding it from metrics) |
| **Precedence** | Block + arm failure (e.g. guard + `FAILED_RETRIEVAL`) → `NOT_SCORABLE` wins; **arm ignored** |
| **Rollup hygiene** | `success_rate` uses scorable denominator only; `not_scorable_cases` always emitted on official runs |

---

#### B.5 — Exit criteria (stop before Phase C)

1. Build green  
2. Failure semantics: guard ≠ arm status ≠ outcome (three-axis verified)  
3. **`evaluation_resolution` is sole canonical decision field** — pure; **never reads `e2_outcome`**; one-direction dependency enforced  
4. **Block-first:** when `run_block_reason != NONE`, `arm_status` ignored for scoring — enforced in `resolveEvaluation()`, not implied  
5. `e2_outcome` derived from resolution only — no drift paths  
6. Arm status never encodes structural aborts (boundary rule verified)  
7. `scoring_block_reason` + rollup metrics implemented — `NOT_SCORABLE` visible, not silently dropped  
8. Precedence tests green (E2-12 block+arm, E2-13 non-block only, E2-14 rollup assertions)  
9. First authoritative re-baseline completed **and** second verification run matches E2-28 scoped fields (resolution + `fingerprint_hash` + `e2_eval_config` + scorable classification) — fingerprint stability treated as function of evaluation stability, not independent truth  
10. A1–A5 invariants preserved  
11. **Pause for confirmation** — Phase C (INTEGRATION tier) next; B6 archives baseline only after item 9  

#### B files (expected)

`episodic_learning_eval.h/.cpp`, `run_episodic_learning_benchmark.cpp`, `workflow_engine.cpp` (guard exception path only), `tests/unit_tests.cpp`, `docs/E2_PROTOCOL.md`, `cursor_list.md` — **no** `e2StrictRetrieve` ranking changes; **no** A5 guard policy change

---

### Separation debt (acknowledged)

STRICT / INTEGRATION share eval types and schema → **behavioral separation**, not fully structural. Acceptable for v1.2; future hardening (kernel-only harness binary, versioned ABI) deferred post–Phase C.

### Key files

| File | Role |
|------|------|
| `docs/E2_PROTOCOL.md` | Preregistered protocol + gate priority + scope limits |
| `external/basic_agent/include/episodic_learning_eval.h` | Schema |
| `external/basic_agent/src/e2_strict_enforcement.cpp` | Pin validation, fingerprint |
| `external/basic_agent/src/e2_strict_retrieval.cpp` | Deterministic retrieval |
| `external/basic_agent/src/run_episodic_learning_benchmark.cpp` | Harness (migration target) |
| `tests/unit_tests.cpp` | E2-01–E2-07 (+ E2-08–E2-11 planned) |

**Run (current, non-authoritative until Phase B):** `./build/debug/external/basic_agent/run_episodic_learning_benchmark`

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
| **E1** | Benchmark environment pinning | ✅ | A–E complete 2026-07-01 — **`docs/benchmark_environment.md`** |
| **E2** | Episodic memory learning eval | 🔶 | Protocol v1.2 + eval kernel ✅; harness still on runtime path — **§ E2** for checkpoint plan (`docs/E2_PROTOCOL.md`) |
| **E3** | Strategy impact / SCR harness | 📋 | Automated SCR or plan-structure proxy in nightly/CI; `COGNATE_V2.md` metric → regression JSONL |
| **G1d** | Trajectory bucket diagnostic | 🔶 | Harness wired — `run_trajectory_ablation_benchmark`; Ollama 30-case run + decision pending — **`docs/trajectory_ablation_benchmark.md` v1.0** |

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
| **2** | **E1** — Environment pinning | ✅ Complete 2026-07-01 — see `docs/benchmark_environment.md` |
| **3** | **E2** — Kernel wiring migration | Phase 0 (pin bug) → A1–A5 sub-checkpoints → Phase B re-baseline. See **`cursor_list.md` § E2** + **`docs/E2_PROTOCOL.md`**. STRICT tests declared-episode retrieval, not organic consolidation (M1.5 covers that). |
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
Next 1  **E2 Phase 0 + A1–A5** — kernel wiring migration (see **§ E2**); G1d Ollama run in parallel if capacity
Next 2  E2 Phase B–C — STRICT re-baseline + INTEGRATION tier + close-out
Next 3  C6 Phase 3 + E3 — longitudinal metrics; SCR harness
Next 4  M4 — range restore (M3 ✅)
Next 5  B1 (if V3 Zenodo) — hardened research corpus
Later   F3/F1 — when eval identifies bottleneck (§ Reflection)
Later   Tier 6 UI polish
Last    Tier 7 self-building / apply_diff (owner discretion)
Horizon Tier 8 future cognitive expansion (F1–F8; see §8)
External V3 — Zenodo MYPAPER re-upload when benchmark corpus stable (C2 ✅; E1 ✅)
Done    E1 — benchmark environment pinning (Checkpoints A–E, 2026-07-01)
```

**GitHub (2026-07-01):** Thoth workspace + Basic_agent submodule — E1 Checkpoints D3–D5 harness wiring pushed; **Checkpoint E** (double-bind, Python tooling, close-out) implemented locally.

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
| **E2 protocol + implementation checkpoints** | **`cursor_list.md` § E2**, **`docs/E2_PROTOCOL.md`** |
| **E1 benchmark environment** | **`docs/benchmark_environment.md`** |
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
