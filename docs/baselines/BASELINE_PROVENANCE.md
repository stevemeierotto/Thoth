# Baseline Provenance — Phase B v1

Records **how** the Phase B baseline came to exist.

| Field | Value |
|-------|-------|
| **Baseline version** | Phase B v1 |
| **Created from** | B5 implementation (`wiring_stage=B`, `runScoredEvaluationLoop`) |
| **Archived by** | B6 (2026-07-04) |
| **Creation date** | 2026-07-04 (UTC-8) |

---

## Repository state at baseline runs

| Component | SHA / state |
|-----------|-------------|
| **Thoth** (`main`) | `e143efe6c656f60c66cd185de404c032c18f79aa` — *plus uncommitted B5 harness/eval changes at run time* |
| **basic_agent** submodule | `dd73d1b253f99b685d9528032dd1107e221f1428` — *local modifications present* |
| **Last committed message** | `Complete E2 Phase B (B1–B4): failure semantics, transport, and JSONL export.` (2026-07-02) |

> **Note:** B5 authoritative runs were executed against the local `build/debug` tree including Phase B5 changes not yet committed to `main`. Frozen JSON artifacts in `docs/baselines/artifacts/phase_b/` are the authoritative record of baseline outputs.

---

## Toolchain and build

| Field | Value |
|-------|-------|
| **Compiler** | g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |
| **CMake preset** | `build-debug` (Debug) |
| **Harness binary** | `./build/debug/external/basic_agent/run_episodic_learning_benchmark` |
| **Unit tests** | `./build/debug/tests/thoth-unit-tests` — all passed (2026-07-04) |
| **Mock tier** | `THOTH_MOCK_EPISODIC=1`, `THOTH_MOCK_LLM=true` (harness default) |

> **EP-01 / EP-01.5 note (2026-07-09):** Phase B baseline remains **mock-tier** evidence. Authoritative inference (`--authoritative`) is additive (EP-01 dual mode + EP-01.5 live `LLMInterface` wiring). Mock and authoritative lift numbers are **not** comparable without explicit tier labels — see `E2_PROTOCOL.md` § Harness contract · Planner / LLM contract.

---

## Protocol

| Field | Value |
|-------|-------|
| **E2 Protocol version** | 1.2 (locked 2026-07-01) |
| **Wiring stage** | `B` (default; authoritative) |
| **Official scoring** | `true` |

---

## Run provenance

| Run | `run_id` | Timestamp (ms) | Role |
|-----|----------|----------------|------|
| #1 | `run-1783192389220` | (see artifact) | Authoritative baseline |
| #2 | `run-1783192418592` | (see artifact) | E2-28 verification |

Command:

```bash
THOTH_E2_WIRING_STAGE=B ./build/debug/external/basic_agent/run_episodic_learning_benchmark
```

Executed twice consecutively; exit code 0 both times.

---

## Notes

- Baseline uses mock-tier episodic learning harness (no Ollama).
- Corpus fingerprint mode: FAST (TfIdf probe index at harness init).
- A5 equivalence regression verified separately (`THOTH_E2_WIRING_STAGE=A5`, `equivalence=yes`).
- **EP-01 / EP-01.5 (2026-07-09):** Authoritative inference (`--authoritative`) is additive. Phase B mock-tier lift is **not** comparable to Phase E authoritative lift without explicit tier labels — see `E2_PROTOCOL.md` § Planner / LLM contract (EP-01.5).
