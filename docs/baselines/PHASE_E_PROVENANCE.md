# Phase E Provenance — STRICT v1 (Step 2 seal → Step 3 L4)

Records **how** the Phase E authoritative STRICT evidence came to exist, and how the L4 **verification** package pins it.  
**Verification ≠ reproduction** — this document supports package verification; it does not claim a fresh harness re-run was executed.

| Field | Value |
|-------|-------|
| **Evidence scope** | `n=3_strict_trio` |
| **Step 2 seal (parent)** | `0a38f22` (sync repair) · docs seal `51b9cf0` · empirical-conclusion `d5df718` |
| **Step 3 plan lock** | `07491b4` (v3) |
| **basic_agent** | `77508c4` |
| **Cited runs** | A `run-1783639167839` · B `run-1783639378206` |
| **L4 mode** | Verification only (`e_q2_reproduction: DEFERRED`) |

---

## Repository state at sealed evidence

| Component | SHA |
|-----------|-----|
| **Thoth** (harness at run time) | `0a38f22` (short) / full in run L2 sidecars |
| **basic_agent** submodule | `77508c4` |
| **Step 3 plan lock commit** | `07491b4a25e47ceca359139591b8a7c4d373bbf7` |

---

## Toolchain and build (not protocol)

| Field | Value |
|-------|-------|
| **Compiler** | g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |
| **CMake** | 3.28.3 |
| **CMake preset** | `build-debug` |
| **Python (L4 verifier)** | 3.12.4 |
| **Docker image** | N/A — not used for this seal |
| **Harness binary** | `./build/debug/external/basic_agent/run_episodic_learning_benchmark` |
| **Authoritative evaluation backend** | Ollama (operational choice; protocol is backend-agnostic) |
| **Ollama client version** | 0.18.0 |
| **LLM model** | `qwen2.5:3b` |
| **Embedding model** | `nomic-embed-text:v1.5` |
| **Embedding method** | External |
| **llama.cpp** | N/A for this seal (Ollama path) |

---

## Known Investigation History

### Investigation hold (pre-EP-01.5)

| Item | Detail |
|------|--------|
| **Symptom** | Authoritative Step 2 pair reported `FAILURE` / `lift=0` with **zero tokens** on all arms |
| **Classification** | Harness instrument gap — not a confirmed empirical falsification |
| **Root cause** | EP-01 cleared mock LLM but never injected `LLMInterface` → synthesis no-op |
| **Evidence superseded** | `docs/baselines/artifacts/phase_e/superseded_pre_ep015/` |

### EP-01.5 — authoritative LLM wiring

| Item | Detail |
|------|--------|
| **Repair** | Own `LLMInterface` on harness runtime; `set_llm_interface` / `set_config`; execution gate (tokens required); planner contract documented |
| **Result** | Live tokens on authoritative arms; fail-closed if no-op |

### Cross-run metrics pollution

| Item | Detail |
|------|--------|
| **Symptom** | E2-28 fail after EP-01.5; cold arms scored from prior `run_id` |
| **Repair** | `readLatestMetricsForGoal(..., runId)` filter — submodule `649d32c` / parent `9bf8fd5` |
| **Evidence superseded** | Partial extracts under investigation; see hold narrative in `phase_e_strict_v1.md` |

### Completion sync race (E2-33)

| Item | Detail |
|------|--------|
| **Symptom** | ~15s harness wait vs ~18s live LLM → `INCOMPLETE` while metrics later showed completed/score 1 |
| **Repair** | Controller-first wait (60s authoritative budget); non-terminal → score 0; E2-33 gate |
| **Commits** | submodule `77508c4` / parent `0a38f22` |
| **Evidence superseded** | `docs/baselines/artifacts/phase_e/superseded_e228_fail_pre_fix/` |

### Current authoritative evidence

| Run | `run_id` | Rollup | lift | E2-28 |
|-----|----------|--------|------|-------|
| A | `run-1783639167839` | `FAILURE` / `SCORED_FAILURE` | 0.0 | bucket #0 vs B |
| B | `run-1783639378206` | `FAILURE` / `SCORED_FAILURE` | 0.0 | bucket #0 vs A |

**Step 2 conclusion:** After EP-01.5 and synchronization repairs, the authoritative harness produced reproducible results with no instrumentation defects. The current authoritative model configuration exhibited no measurable episodic-learning lift on the E2 corpus (`lift = 0.0`). **This is an observed benchmark outcome, not a benchmark failure.**

---

## Harness command (reproduction recipe — documented, not executed in Step 3)

```bash
THOTH_E2_WIRING_STAGE=B ./build/debug/external/basic_agent/run_episodic_learning_benchmark --authoritative
```

`reproduction_recipe.status = DOCUMENTED_NOT_EXECUTED` in the L4 manifest.

---

## L4 package pointers

| Artifact | Path |
|----------|------|
| Verification doc | `docs/baselines/phase_e_l4_verification.md` |
| Machine-readable status | `docs/baselines/phase_e_l4_status.json` |
| Manifest | `docs/baselines/phase_e_run_manifest.json` |
| Verifier | `scripts/verify_phase_e_l4.py` |
| Recovered L2 sidecars | `docs/baselines/artifacts/phase_e/run_0{1,2}_l2_environment.json` |
