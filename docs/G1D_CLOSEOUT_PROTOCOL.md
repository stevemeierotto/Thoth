# G1d — Close-Out Protocol (Infrastructure + Checkpoints)

**Protocol version:** G1d-CO v1.2  
**Status:** 🔒 **Locked for implementation** — v1.0 (2026-07-18); Phase A detail v1.1; **Phase D DROP close-out locked** v1.2 (2026-07-18)  
**Methodology authority (immutable for science):** [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0 (locked 2026-07-01)  
**Prerequisite:** E1 ✅; harness wired; Phases A0–C-TUNE complete with terminal **DROP** pending Phase D  
**Blocks:** F5 until Phase D logs the terminal decision (then F5 is unblocked as a gate only)  
**Checkpoint tracking:** [`cursor_list.md`](cursor_list.md) § G1d  

---

## Document structure

| Class | Sections | Binding? |
|-------|----------|----------|
| **Normative** | Separation of concerns, Phases A0–D (incl. Phase A operational contract), checkpoints A0–D, sequencing, backend identity, env_hash, non-goals, exit criteria | **Yes** — SHALL/MUST |
| **Informative** | Rationale, deferred implement-time choices, doc map | **No** |

If informative text conflicts with a normative rule, the **normative** rule governs.

**Protocol Lock Rule:** This document is locked. Do not silently revise it during implementation. If implementation reveals this close-out protocol must change, stop and request owner approval before editing this file or dependent code.

**Methodology firewall:** Arms A/B/C, case filter, EPSILON, winner rules, and KEEP/TUNE/DROP/CONSTRUCTION_BUG decision matrix are defined **only** in [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0. This close-out protocol **SHALL NOT** alter those rules. Changes to methodology require **Protocol v1.1** of the ablation benchmark doc with a new lock date and commit SHA.

---

## Informative — One sentence

Finish G1d by making benchmark **infrastructure** backend-neutral and provenance-honest, then run the locked ablation experiment, review results, apply the locked decision matrix, and close documentation — without rewriting the science.

---

## Normative — Separation of concerns

| Layer | Authority | May change in this close-out? |
|-------|-----------|-------------------------------|
| **Methodology** | [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0 | **No** |
| **Infrastructure** | This document (preflight, backend detection, provenance, labeling, `env_hash` inputs for G1d) | **Yes** — Phase A0 only, as specified |
| **Operational preflight** | This document Phase A | **Verify only** — no methodology/scoring changes; no new infrastructure unless Checkpoint A0 left a blocking gap (owner-approved) |

**Architectural rule:** Do not rewrite the protocol experiment. Make the harness report honestly which production inference backend it is actually testing.

---

## Normative — Checkpoint table

| Phase | Checkpoint | Tests / evidence (required) |
|-------|------------|-----------------------------|
| **A0** Backend-neutral infrastructure | Backend abstraction complete | Build succeeds; relevant unit tests pass; existing G1d/TfIdf smoke still passes; `env_hash` + sidecar show correct backend; stdout reports `backend=...` instead of `tier=ollama` |
| **A** Operational preflight | Environment verified | Probe gate + recorded backend identity; corpus/index contract verified; External inference path confirmed; Checkpoint A evidence packet (below) |
| **B** Authoritative run | Benchmark completed | Full ~30-case run finishes successfully; JSONL contains one `run_id`, one `env_hash`, all A/B/C case evidence, summary produced with accurate backend provenance |
| **C** Decision | Decision accepted | KEEP/TUNE/DROP/CONSTRUCTION_BUG determined **strictly** from the locked decision matrix in methodology v1.0. **If TUNE → stop here** until the tuning follow-up is complete |
| **D** Documentation / optional config | G1d closed | Documentation updated; completed log written; any allowed config changes made; regression tests (if code changed) still pass |

---

## Normative — Sequencing (review gates mandatory)

```
A0  Backend-neutral infrastructure  (code)
  ↓
Checkpoint A0 — STOP for owner review
  ✓ Build succeeds
  ✓ Relevant unit tests pass
  ✓ Existing benchmark smoke still passes
  ✓ Provenance correctly reports backend=<name> (e.g. llama_cpp or ollama)
  ↓
A   Operational preflight
  ↓
Checkpoint A — STOP for owner review
  ↓
B   Authoritative benchmark
  ↓
Checkpoint B — STOP for owner review (review results; do not auto-apply matrix)
  ↓
C   Apply KEEP/TUNE/DROP matrix
  ↓
Checkpoint C — STOP for owner review (confirm decision)
  ↓
D   Documentation / optional configuration
  ↓
Final validation & close-out
```

**Rules:**

- **A0 is normal development work.** Implement → verify Checkpoint A0 → owner review → only then pay for the expensive authoritative run.
- **Phase A is operational verification** after Checkpoint A0 approval. It is not a scoring phase and not the authoritative run.
- **B → C is a deliberate gate.** Phase B produces evidence; humans review at Checkpoint B; only then may Phase C apply the locked matrix. Automatic KEEP/TUNE/DROP without Checkpoint B is **forbidden**.
- Each **STOP for owner review** requires explicit go-ahead before the next phase (same AGENTS.md implementation gate discipline).

---

## Normative — Phase A0 (Backend-neutral infrastructure)

### Goal

Backend-neutral preflight and provenance for G1d. No methodology changes.

### Backend identity (source of truth)

Backend identity **SHALL** be taken from the **instantiated** `InferenceClient` (e.g. `client->backendName()`), **not** directly from configuration or environment variables alone, so provenance reflects the backend **actually exercised**.

Configuration / `THOTH_INFERENCE_BACKEND` may select which client is constructed; labeling, snapshots, and `env_hash` inputs **SHALL** use the live client after `createInferenceClient`.

### Required changes

| Item | Requirement |
|------|-------------|
| Preflight | Replace `isOllamaReachable()` gate with health of the resolved/instantiated client (`health().ok` or equivalent wrapper) |
| Labels | Stop hardcoding `tier=ollama` / `BenchmarkTier::OLLAMA` when the exercised backend is not Ollama; print resolved `backend=...` from `backendName()` |
| Snapshot | Generic inference snapshot: backend id from client, endpoint identity, reachable; **backend-specific diagnostic fields when available** (e.g. model identifier or version). Model enumeration is **not** required for every backend |
| `env_hash` | **SHALL** uniquely distinguish authoritative runs performed against different inference backends or endpoint configurations |
| Harness | Wire `run_trajectory_ablation_benchmark` to the above |

### Scope control

Shared helpers **MAY** be generalized if required for A0. Migration of **unrelated** benchmark harnesses is **out of scope** unless the change is essentially free. **G1d must not wait on a full harness sweep.**

### Intent

Existing authoritative benchmark results remain **comparable except for corrected provenance metadata**. A0 is infrastructure-only; it is not supposed to change benchmark scoring behavior.

### A0 exit criteria (Checkpoint A0)

1. Build succeeds  
2. Relevant unit tests pass  
3. Existing benchmark smoke still passes (`THOTH_TRAJECTORY_ABLATION_TFIDF=1` path)  
4. Provenance correctly reports `backend=<exercised name>` (e.g. `llama_cpp` or `ollama`)  
5. `env_hash` / sidecar distinguish different backends or endpoints  
6. No change to arm configs or `computeTrajectoryAblationSummary`  
7. **STOP for owner review** before Phase A/B  

### Deferred to implement time (allowed without protocol reopen)

- Exact `BenchmarkTier` enum strategy (`FULL` vs new name vs legacy synonym)  
- Exact per-backend snapshot field set  

---

## Normative — Phase A (Operational preflight) — locked v1.1

### Goal

Confirm the live environment can support an authoritative External-inference G1d run. No methodology or scoring changes.

### Preconditions

1. Checkpoint A0 approved by owner  
2. `THOTH_TRAJECTORY_ABLATION_TFIDF` **MUST NOT** be set for Phase A evidence  
3. Phase A uses the same production inference path A0 wired (`EmbeddingEngine::External` → instantiated `InferenceClient`)

### Step 1 — Production inference backend: measure, record, apply

Reachability is a **gate + provenance contract**, not an informal check.

| Action | Requirement |
|--------|-------------|
| **Measure** | Call `probeInferenceBackend()` (same path as A0). Pass **SHALL** mean `reachable == true` on the **instantiated** client. Backend identity **SHALL** come from `backendName()`, not config alone. |
| **Record** | Persist the probe snapshot into run provenance **before** any ablation work: `backend_name`, `base_url`, `embed_base_url`, diagnostics → sidecar `environment.inference.*` and participating `env_hash` inputs; stdout `backend=<name>`; include a short preflight record (at minimum: timestamp, reachable, backend name, endpoints). |
| **Apply** | If unreachable → **stop Phase A** immediately. Do **not** sample. Do **not** start Phase B. Log **“wired, authoritative decision pending.”** If reachable → that recorded snapshot is the **only** backend identity allowed for Checkpoint A evidence and for Phase B (same backend identity / `env_hash` lineage). |

### Step 2 — Research corpus / index contract

Verify the same corpus contract as methodology v1.0 / `run_grag_benchmark` (the research papers the ablation harness indexes under `agent_workspace/docs/`).

- Required files exist and are readable inside the sandbox  
- Index builds successfully  
- Chunk count is non-zero and plausible for the corpus  

### Step 3 — Production inference path confirmation

Confirm the binary exercises `EmbeddingEngine::External` → `InferenceClient` (not TfIdf).

- Optional cheap probe: short `--sample N` **without** TfIdf — allowed only as **path evidence**  
- Sample output is **not** close-out / decision evidence (Phase B only)

### Step 4 — Provenance spot-check

Confirm stdout / sidecar / JSONL show:

- `backend=<exercised name>` (not a hardcoded `tier=ollama` when the exercised backend is not Ollama)  
- One `run_id` and one `env_hash` for the preflight run lineage  
- Inference fields present in sidecar when External path is used  

### Out of scope (Phase A)

- Full `TRAJECTORY_DISAMBIGUATES` bucket run (Phase **B**)  
- KEEP/TUNE/DROP / CONSTRUCTION_BUG (Phase **C**)  
- Config / documentation close-out (Phase **D**)  
- Arm configs, EPSILON, or decision-matrix changes  
- New infrastructure work unless a blocking A0 gap is found and owner-approved  

### Phase A exit criteria (Checkpoint A)

1. Step 1 gate passed (reachable) and backend identity recorded as specified  
2. Corpus / index contract verified  
3. External inference path confirmed  
4. Provenance spot-check passed  
5. Evidence packet available for owner review (backend name, endpoints, `run_id` / `env_hash` if a probe/sample run was made, corpus/chunk notes, pass/fail per step)  
6. **STOP for owner review** before Phase B  

Fail any step → do **not** start Phase B.

---

## Normative — Phase B (Authoritative run)

Execute methodology step 3 from [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0:

- Full `TRAJECTORY_DISAMBIGUATES` bucket (~30 cases)  
- **No** `--sample` for close-out evidence  
- **No** `THOTH_TRAJECTORY_ABLATION_TFIDF` for close-out evidence  
- One shared `run_id` and `env_hash`; A/B/C evidence; summary with accurate backend provenance  
- Backend identity **SHALL** match the Phase A recorded snapshot lineage (same exercised backend / endpoints as approved at Checkpoint A)

If the resolved backend is unreachable: log **“wired, authoritative decision pending”** — do **not** mark G1d ✅.

**Checkpoint B — STOP for owner review** of results. Do **not** auto-apply the decision matrix.

---

## Normative — Phase C (Decision)

Apply the locked decision matrix from methodology v1.0 **only** (B vs A for KEEP/TUNE/DROP; C for CONSTRUCTION_BUG / mechanism).

- **No** production `retrieval_config.json` change until decision is logged in Phase D  
- If decision is **TUNE**: **stop** after Checkpoint C confirmation until tuning follow-up (`w_t ∈ {0.05, 0.1}` per methodology) completes; do not close G1d as DROP/KEEP yet  
- If **CONSTRUCTION_BUG**: do not tune weight; redesign is out of this close-out  

**Checkpoint C — STOP for owner review** to confirm the decision string and rationale.

---

## Normative — Phase D (Documentation / optional config) — locked v1.2 (DROP close-out)

After Checkpoint C / C-TUNE approval of a **terminal** decision (for this close-out: **DROP**):

### Goal

Log the terminal decision, align tracking docs, apply the config change implied by DROP, verify regressions, mark G1d ✅.

### Preconditions

1. Checkpoint C-TUNE owner confirmation of **DROP**  
2. Evidence lineage frozen (Phase B + TUNE micro-runs at `w_t ∈ {0.05, 0.1}`)

### Required steps

1. **Decision log** — Append terminal **DROP** to [`completed_improvements_log.md`](completed_improvements_log.md) with Phase B + both micro-run `run_id` / `env_hash` / backend / win counts / mean Δ(B−A) and one-line matrix rationale  
2. **Tracking / notes** — Update [`plan_reuse_tuning.md`](plan_reuse_tuning.md), GRAG trajectory notes as needed, [`cursor_list.md`](cursor_list.md), [`improvements.md`](improvements.md); methodology **status** line only (not arms/EPSILON/matrix)  
3. **Allowed config change (DROP)** — Set production `agent_workspace/retrieval_config.json` → `retrieval_weights.trajectory` = **0.0** (was 0.2). Do **not** change GRAG scorer, TrajectoryBuilder, or F5 code  
4. **Regression** — Relevant unit tests / build still pass after A0/C harness changes already shipped  
5. **Final validation** — G1d ✅; F5 unblocked as a *decision gate* only (F5 implementation remains 📋 future work)

### Out of scope

- Re-running benchmarks  
- Weight sweeps / KEEP@tuned  
- CONSTRUCTION_BUG redesign / F5 implementation  
- Methodology science edits  

### Phase D exit criteria

1. Terminal **DROP** logged with full provenance  
2. Docs consistent with DROP  
3. Production `trajectory` weight **0.0**  
4. Regressions green  
5. G1d marked ✅ in tracking docs  

---

## Normative — Non-goals (all phases)

- Scoring, retrieval, or trajectory-construction logic changes (unless a later CONSTRUCTION_BUG follow-up is separately approved)  
- Decision-matrix or arm-weight methodology edits  
- Full multi-harness Ollama cleanup  
- F5 semantic trajectory embeddings  
- B1 corpus expansion  
- Silent edits to [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) methodology sections  

---

## Informative — Historical note

Methodology v1.0 text that says “E2 must not start until step 4” is **obsolete** (E2 Phase E certified). It does **not** block this close-out. F5’s G1d decision gate is **cleared** once Phase D logs the terminal decision (2026-07-18 **DROP**); F5 implementation remains optional future work.

Authoritative provenance language in methodology v1.0 that says “Ollama” means **historical labeling**. Under this close-out protocol, authoritative means: External embeddings via the **instantiated production `InferenceClient`**, with honest `backend=` provenance.

## Informative — Scope of the DROP (does not foreclose research)

G1d evaluated the **current** trajectory term under configured **positive** weights only (production `0.2`, then locked TUNE micro-runs `{0.05, 0.1}`). Terminal **DROP** zeros production influence of that formulation; it does **not** eliminate future trajectory investigations.

**Post-G1d fork:** Polarity analysis is tracked as **G1e** — [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md) **v1.1** (**prepared, not scientifically executed**; Phase 2 checklist locked; schedule `{−0.05, −0.10, −0.20}`; no-scalar-rescue → F5).

**Weight response pattern (mean Δ(B−A), same bucket/backend lineage):** `w_t=0.20` → −0.014 (worse); `0.10` → −0.016 (worse); `0.05` → −0.008 (closer to baseline). That monotonic-toward-zero trend is part of the reasoning trail and may matter for later redesign.

---

## Informative — Doc map

| Need | File |
|------|------|
| **This close-out protocol (normative for A0–D)** | `G1D_CLOSEOUT_PROTOCOL.md` |
| **Ablation methodology (locked science)** | `trajectory_ablation_benchmark.md` v1.0 |
| E1 env pinning | `benchmark_environment.md` |
| Roadmap status | `cursor_list.md`, `improvements.md` |
| Decision log | `completed_improvements_log.md` |

---

*G1d-CO v1.0 locked 2026-07-18. Phase A operational contract locked in v1.1. Phase D DROP close-out locked in v1.2 (2026-07-18). Each phase still requires its checkpoint review approval before proceeding.*
