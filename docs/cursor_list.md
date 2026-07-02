# Thoth Working Backlog

**Last updated:** 2026-07-02 (E2 **Phase A plans complete** — A5 v2; approved, subject to revision — see **§ E2**)  
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
| `A4` | Context injection + single dispatch + executive → same `e2StrictRetrieve()`; full equivalence; **not** official scoring | ✅ Checkpoint |
| `A5` | Diagnostic fuse; post-A4 regression enforcement; guard-only `rag.cpp` change; **not** official scoring | ✅ Checkpoint |
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
| **A2** | Remove `plantAndConsolidate` from STRICT path; scope-limits doc | See **§ Checkpoint A2** — **E2-09**; Option A test discipline |
| **A3** | `e2StrictRetrieve()` + boundary provenance; layer ownership contract | **E2-10**; `retrieval_enabled: true`; purity verified; **not** official scoring |
| **A4** | Executive delegates to same `e2StrictRetrieve()`; context injection + single dispatch + full equivalence | E2-01–E2-07; static dispatch audit + golden-case runtime proof |
| **A5** | Diagnostic fuse; A3→A4 transition enforced; failure-domain separation; first `rag.cpp` touch (guard only) | **E2-11** |
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

**If E2-10 retrieval cases fail due to ranking:** document — **do not** expand A3 scope unless demonstration is impossible.

#### A3.6 — Protocol doc (`E2_PROTOCOL.md`)

§ STRICT retrieval boundary, § Kernel ownership, A3 scope-limit sentence. Add layer ownership diagram/table if not already present.

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

### Phase B — STRICT re-baseline (reference — not Phase A)

**Authorizes scoring only after A4 proves retrieval equivalence between the Executive and the evaluation harness.** Phase B is the single switch where `official_scoring: true` and authoritative `e2_outcome` begin — not A3, not A4, not A5.

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
