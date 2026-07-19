# Completed Improvements Log

Last updated: 2026-07-19 (G1e Phase 4 ✅ KEEP@−0.05 in production; magnitude tuning paused not dropped)

Source: previous `docs/improvements.md` and `docs/next_steps.md` plan entries marked completed

## July 2026 — completed at a glance (2026-07-01 → 2026-07-18)

| Track | What shipped | Status |
|-------|----------------|--------|
| **G1e Phase 4** | KEEP@−0.05 → production `trajectory: -0.05`; tuning paused open | ✅ 2026-07-19 |
| **G1e Phase 3b** | `−0.30` magnitude probe + win-rate early-stop | ✅ 2026-07-19 |
| **G1e Phase 3** | Authoritative polarity runs `{−0.05,−0.10,−0.20}` | ✅ 2026-07-18 |
| **G1e Phase 2 execute** | Operational preflight (ollama External; `fork=g1e`) | ✅ 2026-07-18 |
| **G1e Phase 2 checklist** | Preflight contract locked (v1.1) | 🔒 2026-07-18 |
| **G1e Phase 0–1** | Polarity protocol lock + harness `--wt` allowlist | ✅ 2026-07-18 |
| **G1d close-out** | A0→D; terminal **DROP**; `w_t=0.0` | ✅ 2026-07-18 |
| **G1d Phase C TUNE** | Micro-runs `w_t∈{0.05,0.1}`; harness `--wt` | ✅ 2026-07-18 |
| **G1d Phase B** | Authoritative ~30-case ablation (`ollama` External) | ✅ 2026-07-18 |
| **G1d Phase A** | Operational preflight (ollama External path) | ✅ 2026-07-18 |
| **G1d Phase A0** | Backend-neutral probe / env_hash / harness labels | ✅ code 2026-07-18 |
| **G1d close-out protocol** | A0→D contracts | 🔒 G1d-CO v1.2 |
| **M4 range restore** | Replay + rehydrate per protocol | ✅ 2026-07-18 |
| E2 Phase E | Empirical certification v0.1 (`n=3_strict_trio`, lift=0.0) | ✅ 2026-07-09 |
| C6.3 | Longitudinal track C6.3-01–06 | ✅ 2026-07-11 |
| Containerization A–E | Portable runtime / inference / consolidation prerequisites | ✅ 2026-07-12 |
| Plans F–I | Engine HTTP/SSE, Compose packaging, smoke | ✅ 2026-07-13–14 |
| Plans J–L + K Phase 1 | Hybrid docs, remote GUI client, timeouts/goals honesty, engine RAG ownership/seed | ✅ 2026-07-14–16 |
| Plan M | Grounded chat RAG (floor, greeting skip, cue B); stops later revised by N | ✅ 2026-07-16 |
| Plan N N0–N6 | Chat generation safety (sanitize, soft-empty, empty stops, source_span, CP wire) | ✅ 2026-07-17 |
| Plan N5 | GRAG diagnostics display honesty (Final Score float; Alpha N/A chat) | ✅ 2026-07-18 |

Commits (parent): `f005e25` … `e08341e`. Submodule: Plan N `405c144`; M4 `99c522f`.

Detail entries below remain the chronological source of truth; this table is the operator index.

---

### G1e — Phase 4 ✅ KEEP @ −0.05 (2026-07-19)

**Protocol:** [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md) **G1e v1.2** Phase 4.  
**Decision:** **KEEP** production trajectory weight at **`−0.05`** (Phase 3 `run-1784408754379`: 60% B vs A, Δ+0.00516).

**Config / defaults**

| Location | Value |
|----------|--------|
| `agent_workspace/retrieval_config.json` → `trajectory` | **−0.05** |
| `Config::wt` (`config.h`) | **−0.05** |
| `RetrievalConfig::wt` | **−0.05** |
| Empty-T executive zeroing | unchanged (still forces 0 when T empty) |

**Tuning status:** **Paused, not dropped.** Phase 3b left magnitude search open (`−0.40` gated). Owner may resume later under separate approval. Not NO_SCALAR_RESCUE; not F5 handoff.

**G1d relation:** G1d DROP remains the record for **positive** `w_t`. Production polarity weight is a **G1e** supersession.

### G1e — Phase 3b executed ✅ (2026-07-19) — `−0.30` magnitude probe

**Protocol:** [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md) **G1e v1.2**.  
**Production:** `trajectory: 0.0` unchanged.

| Item | Value |
|------|--------|
| `w_t` | **−0.30** |
| run_id | `run-1784477417536` |
| B vs A | 8 / 6 (**57.1%**) |
| mean Δ(B−A) | **+0.01406** |
| G1e KEEP | **no** (win % &lt; 60%) |
| Harness label | TUNE† (G1e: Not KEEP) |

†Harness still emits G1d matrix strings.

**Early-stop vs Phase 3 `−0.20` (8/6 = 57.1%):** win rate **equal, not worse** → early-stop **does not fire**. Per v1.2: **do not** run `−0.40` without separate owner approval.

**Lineage note:** `env_hash=7e46b1e7…` differs from Phase 2/3 `116927dd…` because git SHAs advanced (`thoth` `98e87be`, `basic_agent` `fd169e4`). Backend/endpoints/models digest unchanged (`ollama` @ `http://127.0.0.1:11434`, `diagnostic_digest=e33a310d…`).

**Phase 4 candidate:** still **KEEP @ −0.05** (`run-1784408754379`) unless owner revises selection. `−0.30` did not meet KEEP.

**STOP:** Magnitude tuning paused pending owner decision (Phase 4 and/or optional `−0.40`).

### G1e — Phase 3 executed ✅ (2026-07-18) — Checkpoint Phase 3

**Protocol:** [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md) **G1e v1.1** Phase 3.  
**Science:** Full `TRAJECTORY_DISAMBIGUATES` bucket (30 cases × arms A/B/C); no `--sample`; no TfIdf; `fork=g1e`.  
**Production:** `trajectory: 0.0` **unchanged** (Phase 4 owns optional config).

**Shared provenance (matches Phase 2):** `env_hash=116927dd257acfdbb51e863831d0e7f0ecd58f7fe3b18fd5d8d27261ea66e460`, `backend=ollama` @ `http://127.0.0.1:11434`, tier `FULL`, `index_hash=89d910caa37e48527747712db42d31d0e7a17204b73a213e1f0ed7a34c363e8d`, 311 chunks.

| `w_t` | run_id | B vs A (excl. ties) | Win % | mean Δ(B−A) | G1e KEEP? | Harness label* |
|-------|--------|---------------------|-------|-------------|-----------|----------------|
| **−0.05** | `run-1784408754379` | 6 / 4 | **60.0%** | **+0.00516** | **YES** | KEEP |
| **−0.10** | `run-1784410045867` | 8 / 6 | 57.1% | +0.00664 | no | TUNE† |
| **−0.20** | `run-1784411288514` | 8 / 6 | 57.1% | +0.01814 | no | TUNE† |

\*Harness still emits G1d matrix strings (`KEEP`/`TUNE`/`DROP`).  
†Under G1e there is **no TUNE branch** — these are **Not KEEP** only.

**Mechanism note:** All three runs have mean nDCG C = A (empty-T ≈ no-weight), so lift when present is from weight × non-empty `T`, not from the weight alone.

**No-scalar-rescue:** **Does not apply** — schedule produced a KEEP candidate at `w_t=−0.05`.

**Recommended Phase 4 decision (pending owner confirm):** **KEEP @ −0.05** as the polarity production candidate (optional config write). Do not adopt −0.10/−0.20 (failed dual KEEP). Do not hand off to F5 via no-scalar-rescue.

**STOP:** Phase 4 (decision log + optional `retrieval_config.json` write) awaits separate owner approval. Production remains `0.0` until then.

### G1e — Phase 2 executed ✅ (2026-07-18) — Checkpoint Phase 2

**Protocol:** [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md) **G1e v1.1** operational preflight.  
**Not science:** `--sample 2 --wt -0.05` path confirmation only; Phase 3 required for polarity KEEP/NO_SCALAR_RESCUE.  
**Production:** `trajectory: 0.0` unchanged.

| Step | Result |
|------|--------|
| **1 Backend gate** | `probeInferenceBackend()` → reachable; instantiated backend `ollama` |
| **2 Record** | Sidecar `logs/benchmark_env.latest.json` + `BENCHMARK_ENV`: `backend_name=ollama`, `base_url=http://127.0.0.1:11434`, `embed_base_url=http://127.0.0.1:11434`, `diagnostic_digest=e33a310d…` |
| **3 Corpus / index** | Five research docs; index built; **311 chunks** |
| **4 Path confirmation** | `embedding_method=External`, tier `FULL`; no `THOTH_TRAJECTORY_ABLATION_TFIDF`; `--sample 2 --wt -0.05` exit 0 |
| **5 Provenance** | stdout `backend=ollama fork=g1e tune_wt=-0.05`; JSONL `fork=g1e` (not Phase C TUNE); COMPLETE payload includes `fork`/`tune_wt` |
| **6 Evidence packet** | This entry |

**Persistent artifact check (Phase 2 lineage `run-1784407500480`)**

| Field | Sidecar `logs/benchmark_env.latest.json` | `benchmark_env.jsonl` (`BENCHMARK_ENV`) | Ablation JSONL |
|-------|------------------------------------------|----------------------------------------|----------------|
| `run_id` | ✅ | ✅ | ✅ |
| `env_hash` | ✅ | ✅ | ✅ |
| backend | ✅ `environment.inference.backend_name=ollama` | ✅ same | ✅ top-level `backend=ollama` |
| endpoint | ✅ `http://127.0.0.1:11434` (base + embed) | ✅ same | (via env_hash lineage) |
| tier | ✅ `FULL` | ✅ same | — |
| fork | — | COMPLETE payload `fork=g1e` | ✅ `fork=g1e` |

**Preflight record**

- **timestamp:** ~2026-07-18T20:26Z → 20:45Z (~18 min, mostly External embed index)
- **reachable:** true  
- **backend:** ollama (Ollama version 0.18.0; embed model `nomic-embed-text:v1.5`)  
- **endpoints:** `http://127.0.0.1:11434` (base + embed)  
- **run_id:** `run-1784407500480`  
- **env_hash:** `116927dd257acfdbb51e863831d0e7f0ecd58f7fe3b18fd5d8d27261ea66e460` (same lineage as G1d Phase A/B)  
- **index_hash:** `89d910caa37e48527747712db42d31d0e7a17204b73a213e1f0ed7a34c363e8d`  
- **harness:** `trajectory_ablation_benchmark`  
- **sample note:** 2 ties, indicative DROP — **not** authoritative polarity evidence  

**Checkpoint Phase 2 Conclusion:** All preflight criteria satisfied. Phase 3 may proceed without further infrastructure changes **after separate owner approval**.

**STOP:** Phase 3 remains behind its own owner approval gate (full ~30-case runs at `−0.05` → `−0.10` → `−0.20`; no `--sample`; no TfIdf; backend/`env_hash` lineage must match above). G1e is still **not scientifically executed**.

### G1e — Phase 2 preflight checklist locked 🔒 (2026-07-18) — v1.1

**Protocol:** [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md) **G1e v1.1**.

**What locked:** Normative Phase 2 operational preflight checklist (backend gate, provenance record, corpus/index, path confirmation, provenance spot-check, evidence packet). Mirror of G1d Phase A adapted for `fork=g1e`.

**Execution status:** **Phase 2 executed** (ops only). Checklist was locked in v1.1; preflight run logged. **Not scientifically executed** until Phase 3. No probe/sample/bucket science claims from Phase 2.

**Non-claim:** No KEEP / NO_SCALAR_RESCUE / production config change.

**Next:** Owner approval for Phase 3 polarity runs.

### G1e — Phase 0–1 ✅ (2026-07-18) — protocol + harness (STOP before experiments)

**Protocol:** [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md) **G1e v1.0** locked.

**Scope:** Post-G1d polarity fork only. G1d remains ✅ DROP; production `trajectory: 0.0` unchanged. No authoritative bucket runs.

| Phase | Deliverable |
|-------|-------------|
| **0** | Locked protocol; tracking in `cursor_list.md`, `GRAG.md`, `plan_reuse_tuning.md`, `improvements.md` |
| **1** | `isTrajectoryAblationG1eWt` / `isTrajectoryAblationCliWt`; CLI `--wt` accepts `{−0.05,−0.10,−0.20}` + G1d TUNE `{0.05,0.1}`; JSONL/stdout `fork=g1e`; `testG1ePolarityWtAllowlist` |

**Schedule (deferred to Phase 3):** `−0.05` → `−0.10` → `−0.20`. **No-scalar-rescue:** if no KEEP → hand off to F5.

**Next:** Phase 2+ requires separate owner approval.

### G1d — Closed ✅ DROP (2026-07-18) — Phase D

**Protocol:** [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) **G1d-CO v1.2** Phase D.  
**Methodology:** [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0 — arms/EPSILON/matrix unchanged.  
**Terminal decision:** **DROP** production trajectory weight on the `TRAJECTORY_DISAMBIGUATES` bucket evidence.

| Run | Role | run_id | Key result |
|-----|------|--------|------------|
| Phase B (`w_t=0.2`) | Authoritative | `run-1784399242046` | Δ(B−A)=−0.0138; pairwise 6/6 (50%) → TUNE |
| TUNE `0.05` | Micro | `run-1784401765860` | Δ=−0.0076; B vs A 2/5 → DROP |
| TUNE `0.1` | Micro | `run-1784402971475` | Δ=−0.0164; B vs A 4/6 (40%) → no KEEP |

**Shared provenance:** `env_hash=116927dd257acfdbb51e863831d0e7f0ecd58f7fe3b18fd5d8d27261ea66e460`, `backend=ollama` @ `http://127.0.0.1:11434`, tier `FULL`, 30 cases.

**Rationale:** After locked TUNE path (`w_t ∈ {0.05, 0.1}`), mean Δ(B−A) ≤ 0 and KEEP dual criteria unmet → **DROP**.

**Config:** `agent_workspace/retrieval_config.json` → `trajectory: 0.0` (was 0.2). No scorer/TrajectoryBuilder/F5 code changes.

**Docs:** `plan_reuse_tuning.md`, `GRAG.md`, `cursor_list.md`, `improvements.md` updated. F5 unblocked as decision gate only (implementation still 📋).

**Research caveat:** DROP zeros the *current* positive-weight trajectory contribution; it does not foreclose alternate formulations (calibration, normalization, polarity, F5). Observed mean Δ(B−A): `0.20` −0.014; `0.10` −0.016; `0.05` −0.008 (closer to baseline).

### G1d — Phase C TUNE micro-runs ✅ (2026-07-18) — Checkpoint C-TUNE

**Accepted from Phase B:** **TUNE** (matrix). Follow-up only: `w_t ∈ {0.05, 0.1}`.  
**Harness:** `--wt 0.05|0.1` override (Arm A=0; B/C use tune wt; C empty T). Matrix code unchanged.  
**No** production `retrieval_config.json` change.

| Micro-run | run_id | tune_wt | A/B/C wins | Ties | B vs A | mean Δ(B−A) | Harness decision |
|-----------|--------|---------|------------|------|--------|-------------|------------------|
| 1 | `run-1784401765860` | **0.05** | 0/2/0 | 28 | 2 / 5 (28.6%) | **−0.00763** | **DROP** (win rate < 40%) |
| 2 | `run-1784402971475` | **0.1** | 0/4/0 | 26 | 4 / 6 (40%) | **−0.01638** | TUNE (no KEEP; mean ≤ 0) |

**Shared provenance:** `env_hash=116927dd…` (Phase A/B), `backend=ollama` @ `http://127.0.0.1:11434`, tier `FULL`, 30 cases, `sample_mode=false`, C mean = A mean both runs.

**Phase B reference (wt=0.2):** `run-1784399242046` — Δ=−0.0138, pairwise 6/6 (50%) → TUNE.

**Matrix after micro-runs (locked):** Neither candidate meets KEEP (≥60% wins **and** mean Δ>0). Both have mean Δ(B−A) ≤ 0 after TUNE path → terminal **DROP** for production `w_t`. Not CONSTRUCTION_BUG (majority `C≈A∧B<A` not met).

**Recommended terminal decision (pending owner confirm):** **DROP** — do not keep/tune production trajectory weight from this bucket evidence.

**Checkpoint C-TUNE Conclusion:** TUNE follow-up complete. Phase D (docs + optional config) awaits owner approval of **DROP**; no config write in this phase.

**STOP:** Owner review before Phase D.

### G1d — Phase B executed ✅ (2026-07-18) — Checkpoint B

**Protocol:** [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) G1d-CO v1.1 Phase B (authoritative run).  
**Methodology:** [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0 — arms/EPSILON/matrix unchanged.  
**Harness decision string is indicative only** — do **not** treat as accepted until Phase C / Checkpoint C.

| Criterion | Result |
|-----------|--------|
| Full bucket | **30 / 30** `TRAJECTORY_DISAMBIGUATES`; `sample_mode=false` |
| No TfIdf | `embedding_method=External`; tier `FULL` |
| Exit | 0 (~23 min wall: 18:05:51Z → 18:29:09Z UTC) |
| Lineage vs Phase A | Same `env_hash`, backend `ollama`, endpoints `http://127.0.0.1:11434`; **new** `run_id` |

**Authoritative provenance**

- **run_id:** `run-1784399242046`  
- **env_hash:** `116927dd257acfdbb51e863831d0e7f0ecd58f7fe3b18fd5d8d27261ea66e460` (matches Phase A)  
- **index_hash:** `89d910caa37e48527747712db42d31d0e7a17204b73a213e1f0ed7a34c363e8d`  
- **backend:** `ollama` @ `http://127.0.0.1:11434` (sidecar `environment.inference.*`)  
- **chunks:** 311  
- **log:** `logs/trajectory_ablation_benchmark.jsonl`  

**Results (harness summary — not yet Phase C accepted)**

| Metric | Value |
|--------|-------|
| A / B / C wins | 0 / **6** / 0 |
| Ties | 24 |
| B vs A (non-tie pairwise) | 6 / 6 |
| mean nDCG A / B / C | 0.441745 / 0.427969 / 0.441745 |
| mean Δ (B−A) | **−0.013776** |
| Harness `decision` | **TUNE** — B win rate ≥ 40% but KEEP dual criteria not met |
| Arm C vs A | mean C = mean A (exact); C wins = 0 |

**B-win cases:** T3, T8, T18, T19, T22, T27  

**Checkpoint B Conclusion:** Authoritative evidence complete and provenance-honest. Decision matrix **not** applied (Phase C). No `retrieval_config.json` change.

**STOP:** Owner review of Checkpoint B required before Phase C.

### G1d — Phase A executed ✅ (2026-07-18) — Checkpoint A

**Protocol:** [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) G1d-CO v1.1 operational preflight.  
**Not close-out evidence:** `--sample 2` path confirmation only; Phase B required for decision.

| Step | Result |
|------|--------|
| **1 Measure** | `probeInferenceBackend()` → reachable; instantiated backend `ollama` |
| **1 Record** | Sidecar + `BENCHMARK_ENV`: `backend_name=ollama`, `base_url=http://127.0.0.1:11434`, `embed_base_url=http://127.0.0.1:11434`, `diagnostic_digest=e33a310d…` |
| **1 Apply** | Gate passed; same identity used for preflight sample |
| **2 Corpus** | Five research docs present; index built; **311 chunks** |
| **3 External path** | `embedding_method=External`, tier `FULL`; no `THOTH_TRAJECTORY_ABLATION_TFIDF`; `--sample 2` exit 0 |
| **4 Provenance** | Persistent artifacts verified (below); stdout `backend=ollama` matches |

**Persistent artifact check (Phase A lineage `run-1784397513912`)**

| Field | Sidecar `benchmark_env.latest.json` | `benchmark_env.jsonl` (`BENCHMARK_ENV`) | Ablation JSONL |
|-------|-------------------------------------|----------------------------------------|----------------|
| `run_id` | ✅ | ✅ | ✅ |
| `env_hash` / `environment_hash` | ✅ | ✅ | ✅ |
| backend | ✅ `environment.inference.backend_name=ollama` | ✅ same | ✅ top-level `backend=ollama` |
| endpoint | ✅ `environment.inference.base_url` / `embed_base_url` = `http://127.0.0.1:11434` | ✅ same | (via env_hash lineage) |
| tier | ✅ `environment.runtime.tier=FULL` | ✅ same | — |

Backend identity is in the persisted sidecar/`BENCHMARK_ENV` inference block (and participates in `env_hash`), not stdout-only.

**Preflight record**

- **timestamp_utc:** 2026-07-18T17:36:49Z → 17:58:41Z (~22 min, mostly External embed index)
- **reachable:** true  
- **backend:** ollama (Ollama version 0.18.0; embed model available: `nomic-embed-text:v1.5`)  
- **endpoints:** `http://127.0.0.1:11434` (base + embed)  
- **run_id:** `run-1784397513912`  
- **env_hash:** `116927dd257acfdbb51e863831d0e7f0ecd58f7fe3b18fd5d8d27261ea66e460`  
- **index_hash:** `89d910caa37e48527747712db42d31d0e7a17204b73a213e1f0ed7a34c363e8d`  
- **harness:** `trajectory_ablation_benchmark`  
- **sample note:** indicative DROP on 2 ties — **not** authoritative  

**Checkpoint A Conclusion:** All preflight criteria satisfied. Phase B may proceed without further infrastructure changes.

**STOP:** Phase B remains behind its own owner approval gate (full ~30-case run, no `--sample`, no TfIdf; backend lineage must match above).

### G1d — Phase A operational contract locked 🔒 (2026-07-18)

- **Protocol:** [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) **G1d-CO v1.1** — Phase A (measure / record / apply backend gate; corpus; External path; provenance spot-check; Checkpoint A evidence packet).
- **Not execution:** Phase A still requires Checkpoint A0 approval before running.
- **Phase B link:** Authoritative run backend identity MUST match Phase A recorded snapshot lineage.

### G1d — Phase A0 implemented ✅ (2026-07-18) — Checkpoint A0

- **Protocol:** [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) v1.0 Phase A0 (infrastructure only; methodology unchanged).
- **Probe:** `inference_backend_probe.{h,cpp}` — `probeInferenceBackend` / `snapshotFromInferenceClient` / `isInferenceBackendReachable`; identity from instantiated `InferenceClient::backendName()`.
- **env_hash:** `InferenceEnvironment` in `benchmark_environment` (backend_name, base_url, embed_base_url, diagnostic_digest) participates in canonical identity.
- **Harness:** `run_trajectory_ablation_benchmark` — preflight via probe (not `isOllamaReachable`); tier `FULL` (authoritative) / `MOCK` (TfIdf); stdout/JSONL `backend=...`.
- **Tests:** `testG1dA0EnvHashDistinguishesInferenceBackend`, `…Endpoint`, `…SnapshotUsesClientBackendName`; E1 JSON round-trip covers inference.
- **Verify:** debug build OK; `thoth-core-tests` all passed; `THOTH_TRAJECTORY_ABLATION_TFIDF=1 --sample 2` smoke prints `backend=tfidf`.
- **STOP:** Owner review of Checkpoint A0 required before Phase A/B (authoritative run).

### G1d — Close-out protocol locked 🔒 (2026-07-18)

- **Normative:** [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) — G1d-CO v1.0 base; **v1.1** adds locked Phase A operational contract.
- **Methodology (unchanged):** [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0 — arms, EPSILON, decision matrix immutable.
- **Key rule:** Fix infrastructure (A0 backend-neutral preflight/provenance) before authoritative run; Checkpoint A0 and Checkpoint B are mandatory review stops.
- **Backend identity:** Instantiated `InferenceClient::backendName()`, not config alone.
- **Next:** Checkpoint A0 approval → execute Phase A per v1.1 → Checkpoint A → Phase B.

---

### M4 — Range restore implemented ✅ (2026-07-18)

- **Protocol:** [`M4_PROTOCOL.md`](M4_PROTOCOL.md) v1.0 (locked then implemented same day).
- **Modes:** Replay (read-only, no memory-store side effects); Rehydrate (all-or-nothing transactional copy cold→hot; cold preserved).
- **API:** `RestoreRequest`/`RestoreResult`; `Memory::runRestore`; `MemoryPruner::restore(session, request)`; legacy `restore(sessionId)` = silent full-session replay.
- **Repo:** Ranged `getArchivedMessages` (order: `original_timestamp_ms ASC`, `archive_id ASC`); `rehydrateArchivedMessages` with duplicate invariant `(ts, role, content)` and `THOTH_INJECT_RESTORE_FAIL` rollback hooks.
- **CLI:** `/prune restore [--rehydrate] [--unsafe] [--start <ms>] [--end <ms>] [session]`.
- **Trace:** `memory_restore_replay` / `memory_restore_rehydrate`.
- **Tests:** M4-01–M4-10 pass under `THOTH_MOCK_EPISODIC=1` (`thoth-core-tests` green).

### M4 — Range restore protocol locked 🔒 (2026-07-18)

- **Normative:** [`M4_PROTOCOL.md`](M4_PROTOCOL.md) v1.0 — locked for implementation.
- **Modes:** Replay (read-only, no memory-store side effects); Rehydrate (all-or-nothing transactional copy cold→hot; cold preserved).
- **Scope:** Timestamp-range (`original_timestamp_ms`) only; duplicate invariant `(ts, role, content)`; stable order `(original_timestamp_ms ASC, archive_id ASC)`.
- **API:** `RestoreRequest`/`RestoreResult`; legacy `restore(sessionId)` = full-session silent replay alias; CLI `/prune restore`.
- **Trace:** Distinct events `memory_restore_replay` / `memory_restore_rehydrate`.
- **Status pointers:** `memory_architecture.md`, `improvements.md`, `cursor_list.md` updated.
- **Next:** Implemented same day — see entry above.

---

### Chat — Plan N5 (GRAG diagnostics display honesty) ✅ (2026-07-18)

**Spec:** [`plan_n5_grag_diagnostics_display.md`](plan_n5_grag_diagnostics_display.md)

Column **Final Score** as plain floats (no `%`); Alpha `N/A (chat)` when no goal embedding; magnitude stays numeric; **Mode** maps `scoring_type` (Conversational / GRAG / …). Pure helpers in `includes/grag_diagnostics_display.h`; panel wiring in `GragDiagnosticsPanel.cpp`. No engine/payload changes. Tests N5-T1–T5. `thoth-core-tests` green; `thoth-control-panel` builds.

---

### Chat — Plan N5 lock (GRAG diagnostics display honesty) 🔒 (2026-07-18)

**Superseded by Plan N5 ✅ (2026-07-18).**

**Spec:** [`plan_n5_grag_diagnostics_display.md`](plan_n5_grag_diagnostics_display.md)

N5 plan **locked** (historical — later implemented). Column **Final Score** as plain float (no `%`); Alpha `N/A` when no goal embedding; keep magnitude numeric + Mode for interpretation; **no engine / payload changes**; no bars/gauges/normalization/probability. Tests N5-T1–T5 (incl. `%` absence + unknown mode).

---

### Chat — Plan N N6 (wire chat generation safety) ✅ (2026-07-17)

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Wire chat generation safety (N6)

`CommandProcessor::runConversationalGenerate` wraps all three chat arms → `generateAndSanitizeChat`. Class A: `formatProviderError`, no `processToolCall`, one memory write. Telemetry: flags/reasons/char counts only. Tests N-T9–T12. `chat_generation_safety.*` untouched. No conversational `llm.query(` in CP. `thoth-core-tests`: all unit tests passed.

**Live note:** Docker engine was rebuilt with N6 for live testing; host GUI uses that image when `THOTH_ENGINE_URL` points at Compose.

---

### Chat — Plan N N6 lock (wire chat generation safety) 🔒 (2026-07-17)

**Superseded by Plan N N6 ✅ (2026-07-17).**

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Wire chat generation safety (N6)

N6 plan **locked** (historical — later implemented). Required one CP conversational wrapper for all three arms; Class A via `formatProviderError` (no tools); telemetry flags/reasons/counts only (no raw text); tests N-T9–T12 (N-T11: no scaffold in stored memory; N-T12: provider failure). `chat_generation_safety.*` untouched / N2 not reopened.

---

### Chat — Plan N N4 (source_span prompt header) ✅ (2026-07-17)

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Prompt-control header (N4)

`formatChunkForPrompt` emits `source_span=` (range / single / omit when `startLine<=0`); no `Lines:` as LLM-facing header. Plan M cue fixture untouched. Tests N-T8, N-T8b, N-T8c. No CP / RAG / grounding changes. `thoth-core-tests`: all unit tests passed.

---

### Chat — Plan N N4 lock (source_span prompt header) 🔒 (2026-07-17)

**Superseded by Plan N N4 ✅ (2026-07-17).**

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Prompt-control header (N4)

N4 plan **locked** (historical — later implemented). `formatChunkForPrompt` → `source_span=`; required **N-T8 / N-T8b / N-T8c**; verify no `Lines:` emit as prompt content (not whole-file grep); Plan M fixture untouched; non-goals: no citation guarantee, no RAG/ranking/grounding/citation changes.

---

### Chat — Plan N N3 (conversational empty stops) ✅ (2026-07-17)

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Conversational stop policy (N3)

`chatStopSequences()` → `{}` (L6); `kChatStopUser` / `kChatStopAgent` retained (tests / explicit non-chat). Three CP arms still call the policy function (empty stops on the chat path). Plan M stop test revised for empty omit + explicit N-T7 serialize. Tests N-T6, N-T6c, N-T7. No CP→`generateAndSanitizeChat` migration in N3 itself. `thoth-core-tests`: all unit tests passed.

**Live-path note (historical until N6):** N3 removed transcript stops from chat generate payloads. Full sanitize / retry / fallback was true until **N6 ✅** wired the helper. Unchanged scaffolding after N3 alone was not necessarily an N3 failure.

---

### Chat — Plan N N3 lock (conversational empty stops) 🔒 (2026-07-17)

**Superseded by Plan N N3 ✅ (2026-07-17).**

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Conversational stop policy (N3)

N3 plan **locked** (historical — later implemented). `chatStopSequences()` → `{}`; keep `kChatStop*` unless audit shows zero refs; three CP arms keep calling the policy function; no CP→helper migration; revise Plan M stop test only; **N-T6c required**.

---

### Chat — Plan N N2 (structured generation + empty handling) ✅ (2026-07-17)

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Structured generation + empty handling (N2)

Soft-empty llama parse (`ok=true` + empty text); `finish_reason` / Ollama `done_reason`; `LLMInterface::queryDetailed` + compatible `query()`; `generateAndSanitizeChat` (sanitize, one stop-free retry, L8 fallback, no UI error strings in result). Tests N-T3–T5 / T3b / T3c / T5b / T6b. No CommandProcessor migration in N2 itself. `thoth-core-tests`: all unit tests passed.

**Current (as of N6 ✅):** Conversational arms use `generateAndSanitizeChat` via `CommandProcessor::runConversationalGenerate`. Soft-empty and structured generation are on the live chat path.

**Historical (at N2 ship only):** N2 capability existed (`queryDetailed` + `generateAndSanitizeChat`) but was not yet the live chat path — CP still called string `llm.query(...)` until N6. Soft-empty via `query()` alone was only an interim mapping.

---

### Chat — Plan N N2 lock (structured generation + empty handling) 🔒 (2026-07-17)

**Superseded by Plan N N2 ✅ (2026-07-17).**

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Structured generation + empty handling (N2)

N2 plan **locked** (historical — later implemented). Soft-empty llama parse; `finish_reason`; `queryDetailed` + compatible `query()`; `generateAndSanitizeChat` with A–D taxonomy, one stop-free retry, L8 fallback; no UI strings in `ChatGenerationResult`; no CommandProcessor migration.

---

### Chat — Plan N N1 (helper foundation) ✅ (2026-07-17)

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Sanitization (N1)

Pure chat safety seam: `SanitizeOutcome` + `sanitizeChatAssistantText` + `ChatGenerationResult` defaults in `chat_generation_safety.{h,cpp}`; CMake registration; unit tests N-T1 / N-T2 / N-T2b. No CommandProcessor, LLMInterface, inference client, retry, stops, or telemetry wiring in N1 itself. Runtime chat behavior unchanged at N1 ship. `thoth-core-tests`: all unit tests passed.

---

### Chat — Plan N N1 lock (helper foundation) 🔒 (2026-07-17)

**Superseded by Plan N N1 ✅ (2026-07-17).**

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md) § Sanitization (N1)

N1 plan **locked** (historical — later implemented). Pure `SanitizeOutcome` + `sanitizeChatAssistantText` + `ChatGenerationResult` defaults; CMake source registration; tests N-T1 / N-T2 / N-T2b. No CommandProcessor, LLMInterface, inference client, retry, stops, or telemetry wiring. No runtime chat behavior change at lock time.

---

### Chat — Plan N N0 lock (Chat Generation Safety) 🔒 (2026-07-17)

**Spec:** [`plan_n_chat_generation_safety.md`](plan_n_chat_generation_safety.md)

Plan N **locked** (N0 documentation). One conversational generation boundary (`generateAndSanitizeChat`); soft provider-empty; sanitize-before-persist; Option A (no transcript stops on chat); N4 `source_span=` prompt-control. Revises Plan M G3 conversational stops only.

**Current (2026-07-18):** N1–N6 ✅ implemented; Plan N5 ✅ implemented (diagnostics display — separate plan). “N5 deferred” in the original N0 lock text is no longer current state.

---

### Chat RAG — Plan M ✅ Complete (G4 docs closeout) (2026-07-16)

**Spec:** [`plan_m_grounded_retrieval_gate.md`](plan_m_grounded_retrieval_gate.md)

**G4 is documentation/status only** — no runtime feature changes. Plan M marked **✅ Complete**.

| Shipped (G0–G3) | Deferred (new plan required) |
|-----------------|------------------------------|
| G1: fail-closed floor `0.01f` on post-boost `final_score`; `grounded` ↔ post-floor injection; attempt/success telemetry | Stronger meaningful-retrieval threshold beyond R1 |
| G2: exact-match greeting skip + `greeting_skip` telemetry | **Embedding / retrieval score analysis** (distributions, embed alignment, zero-score clusters) |
| G3: cue B (no open `[Agent]`); anti-transcript; stops `\n[User]`/`\n[Agent]`; chat max tokens 512 | Optional live Compose/GGUF smoke (not a Plan M PR gate) |

**Post-complete revision:** Conversational transcript stops superseded by [Plan N](plan_n_chat_generation_safety.md) (2026-07-17). Cue B + anti-transcript + 512 remain.

**Operator reading rule:** `grounding_mode=no_retrieval_hits` is ambiguous alone — pair with `retrieval_ran` + `retrieval_skip_reason` (see Plan M telemetry state table). Preserve Plan K/L.

---

### Chat RAG — Plan M G4 plan lock (docs closeout) 🔒 (2026-07-16)

**Spec:** [`plan_m_grounded_retrieval_gate.md`](plan_m_grounded_retrieval_gate.md)

G4 plan **locked** (not implemented). Docs/status closeout only: mark Plan M Complete; operator telemetry state table; name deferred stronger threshold + embedding/retrieval score analysis. No code, Compose, retrieval, greeting, or prompt changes. Awaits explicit Implement approval per AGENTS.md.

---

### Chat RAG — Plan M G3 (prompt cue B + stops) ✅ (2026-07-16)

**Spec:** [`plan_m_grounded_retrieval_gate.md`](plan_m_grounded_retrieval_gate.md)

Implemented and verified (`thoth-core-tests` green). Cue **B**: chat prompts end with `"[User] " + input + "\n"` (no open `[Agent]` slot). Never-truncated `kAntiTranscriptRules`. Chat generate uses stops `\n[User]` / `\n[Agent]` only and `kChatMaxTokens = 512`. Additive `stop_sequences` on Plan H request type (omit when empty) for Ollama + llama-server. Offline T4/T2 prompt + stop-payload tests. No retrieval/greeting/K/L/Docker changes. G4 not started.

---

### Chat RAG — Plan M G3 plan lock (prompt cue + stops) 🔒 (2026-07-16)

**Spec:** [`plan_m_grounded_retrieval_gate.md`](plan_m_grounded_retrieval_gate.md)

G3 plan **locked** (not implemented). Cue **B**: exact user block `"[User] " + user_input + "\n"` (no open `[Agent]` slot). Anti-transcript never-truncated rules. Stops: `\n[User]`, `\n[Agent]` only. Chat max tokens **512**. Additive optional `stop_sequences` on Plan H request type. No retrieval/greeting/K/L/Docker. Awaits explicit Implement approval per AGENTS.md.

---

### Chat RAG — Plan M G2 (greeting skip) ✅ (2026-07-16)

**Spec:** [`plan_m_grounded_retrieval_gate.md`](plan_m_grounded_retrieval_gate.md)

Implemented and verified (`thoth-core-tests` green). Exact-match `isGreetingSkipQuery` skips retrieve for locked greetings (`hello`, `hi`, `thanks`, …) after normalize; empty-index still reports `no_index` first. Skip telemetry: `retrieval_ran=false`, `retrieval_skip_reason=greeting`, `grounding_decision_reason=greeting_skip`, `grounding_mode=no_retrieval_hits` (skip reason disambiguates). Tests: must-skip / must-not-skip (T1) + skip telemetry shape (T5). G3 prompt/stops not started.

---

### Chat RAG — Plan M G2 plan lock (greeting skip) 🔒 (2026-07-16)

**Spec:** [`plan_m_grounded_retrieval_gate.md`](plan_m_grounded_retrieval_gate.md)

G2 plan was locked before implementation (exact-match primary; defensive interrogative wording; no new `grounding_mode`).

---

### Chat RAG — Plan M G1 (grounding floor + contract + telemetry) ✅ (2026-07-16)

**Spec:** [`plan_m_grounded_retrieval_gate.md`](plan_m_grounded_retrieval_gate.md)

Implemented R1 and verified (`thoth-core-tests` green). Chat `grounded=true` / `grounding_mode=retrieved_context` now require chunks that survive a **fail-closed floor** on post-boost `final_score` (`kMinGroundingFinalScore = 0.01f`, rejecting `0.0` / NaN / missing), not merely a non-empty nearest-neighbor string. `CHAT_RAG_CONTEXT` gains attempt-vs-success telemetry (`retrieval_ran`, `retrieval_skip_reason`, `candidates_found`, `candidates_passed_gate`, `grounding_decision_reason`, `grounded`, `max_score`/`min_injected_score`). New helper `applyGroundingFloor` returns injectable chunks + aligned diagnostics + stats. `no_index` path emits `empty_index`. Tests: T3 + pass + telemetry-shape. Greeting skip (G2), prompt/stops (G3), and any stronger threshold deferred as planned.

---

### Chat RAG — Plan M G0 (grounded retrieval gate lock) ✅ (2026-07-16)

**Spec:** [`plan_m_grounded_retrieval_gate.md`](plan_m_grounded_retrieval_gate.md)

Docs-only G0 complete: semantic contract (`grounded` ⇔ meaningful post-gate injection); operator telemetry field list (attempt vs success); locked score-gate default `kMinGroundingFinalScore = 0.01f`; locked Hello exact-phrase skip set + skip telemetry; tests T1–T5 reserved for G1+. Preserve Plan K/L. **No core/GUI code in G0.** Next: Implement G1.

---

### Containerization — Plan L complete ✅ Complete (L3 deferred) (2026-07-16)

**Spec:** [`plan_l_workspace_corpus.md`](plan_l_workspace_corpus.md) · **Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 10 ✅

Plan L completes the **engine workspace ownership model** and the **supported workflow** for an engine-owned RAG corpus. It does **not** alone claim to have fixed the post-migration remote GUI failure mode; that was addressed by **Phase 1 / Plan K** (timeouts, goal routing, honest remote capabilities) **together with** Plan L (ownership, seeding, verification).

| Checkpoint | Result |
|------------|--------|
| L0 | O1 engine-owned corpus; `docker/seed_rag/` source; no host mirror / memory bind / Phase 3 / Cognate |
| L1 | Seed whitelist + `seed-workspace.sh` (Compose-resolved `/workspace`) |
| L2 | Sentinel + structured `chat_rag.jsonl` / decision-trace evidence checklist |
| L3 | 🔒 Deferred — L1 reseed remains sole supported Compose corpus path |
| L4 | Closeout + Final Architecture State |

**Final architecture (remote):** GUI communicates with the engine over HTTP/SSE only (does not own or synchronize engine workspace). Engine owns `/workspace` (RAG corpus, `rag_index.bin`, `memory.db`, traces) and `/logs`. Supported workflow: `docker/seed_rag/` → `./docker/seed-workspace.sh` → engine workspace → L2 verification. This is the supported operational model for remote Compose / hybrid deployments.

---

### Containerization — Plan L L3 (dev-rag bind profile) 🔒 Deferred (2026-07-16)

**Spec:** [`plan_l_workspace_corpus.md`](plan_l_workspace_corpus.md) § L3

Locked decision: **Defer L3**. No `compose.dev-rag.yml`. L1 explicit reseeding remains the only supported Compose corpus path. Architectural justification (L1 vs file-level binds) recorded in Plan L; reopen only with a new Approve L3 lock.

---

### Containerization — Plan L L2 (retrieval evidence checklist) ✅ (2026-07-16)

**Spec:** [`plan_l_workspace_corpus.md`](plan_l_workspace_corpus.md)

Docs-only: operator checklist seed → restart → Evidence A/B/C. Sentinel `THOTH_PLAN_L_SEED_SENTINEL_7f3a9c2e` in `docker/seed_rag/HOWTO.md`. Evidence C prefers `/logs/chat_rag.jsonl` (`CHAT_RAG_*`, `grounding_mode`) and decision-trace stages over answer text. Documents unseeded `no_retrieval_hits` failure. No `SMOKE_SEED`, no smoke.sh / CI changes.

**Files:** `docker/README.md`, `docker/seed_rag/HOWTO.md`, `docker/seed_rag/README.md`, `docs/GETTING_STARTED.md`, plan + log.

---

### Containerization — Plan L L1 (seed workspace RAG) ✅ (2026-07-15)

**Spec:** [`plan_l_workspace_corpus.md`](plan_l_workspace_corpus.md) · **Roadmap:** Step 10

- `docker/seed_rag/` curated whitelist: `GRAG.md`, `HOWTO.md`, `AGENTS.md`, `cognate.md` (+ README)
- `docker/seed-workspace.sh`: fail if whitelist incomplete; ignore extras; seed via `docker compose run` into `thoth-engine:/workspace` (no hardcoded volume name; honors `COMPOSE_PROJECT_NAME`); clear `rag_index.bin`; never touch `memory.db`; `--dry-run` / `--no-restart`
- Operator docs in `docker/README.md`

**Later:** L2 ✅ · L3 deferred · L4 closeout ✅ · Phase 3 ingest / Cognate APIs still out of Plan L scope

---

### Containerization — Plan L L0 (engine workspace corpus lock) 🔒 (2026-07-15)

**Spec:** [`plan_l_workspace_corpus.md`](plan_l_workspace_corpus.md) · **Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 10

Docs-only lock: **O1 engine-owned** corpus on `thoth-workspace`; seed source = repo `docker/seed_rag/` (L1); no host `agent_workspace` mirror; no `memory.db` bind; no Phase 3 ingest; no Cognate APIs; optional dev-rag bind profile deferred until after L1. No scripts/seed files in L0.

**L1 plan lock (2026-07-15):** Whitelist-only four files (fail if missing; ignore extras); Compose-resolved `thoth-engine:/workspace` volume (no hardcoded volume name; respect `COMPOSE_PROJECT_NAME`).

---

### Remote GUI — Phase 1 timeout / goal routing ✅ (2026-07-15)

**Context:** Post–Plan K hybrid regression: `/v1/chat` client timeout (120s) vs engine LLM budget (600s); typed `goal:` blocked on chat path; host RAG/memory sync no-ops spammed while claiming success.

**Changes (Plan K boundary preserved — no cognate/RAG ingest APIs):**
- Remote HTTP chat/goals timeouts aligned with LLM budget (`kChatTimeoutSec=600`, `kGoalsTimeoutSec=1260`); override via `THOTH_REMOTE_HTTP_TIMEOUT_SECONDS`
- Explicit `goal:` / `/goal` → `executeGoal` → `/v1/goals` (SSE for progress), not `/v1/chat`
- `loadConversationMemory*` returns `false` in remote mode; MainFrame skips sync no-ops + honest status text
- Offline test asserts timeout floor

**Files:** `remote_agent_http_utils.h`, `remote_agent_backend.cpp`, `AgentInterface.*`, `MainFrame.*`, `tests/unit_tests.cpp`, `docker/README.md`

**Deferred:** Phase 2 workspace ownership (arch review); Phase 3 RAG ingest API (separate plan)

---

### Containerization — Plan K complete (GUI API client) ✅ (2026-07-15)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 9 · **Spec:** [`plan_k_gui_api_client.md`](plan_k_gui_api_client.md) 🔒

Transport-only GUI client: `IAgentBackend` + `LocalAgentBackend` (default in-process) + `RemoteAgentBackend` (libcurl HTTP/SSE). `THOTH_ENGINE_URL` selects Remote at `AgentInterface` startup only. Offline mapping/selection/SSE tests in `ctest -L pr`; manual smoke checklist in `docker/README.md`; opt-in live via `THOTH_REMOTE_LIVE_URL`. No EngineRuntime/GRAG/planner/tools/F–G contract changes; MainFrame unchanged.

| Checkpoint | Summary |
|------------|---------|
| K1 | Backend abstraction; Local owns `BasicAgentPlugin` |
| K2 | Remote HTTP (`/health`, `/ready`, chat, goals, control) |
| K3 | SSE `/v1/events` thread + pure framing helpers |
| K4 | `THOTH_ENGINE_URL` selection + hybrid docs |
| K5 | Chat/goal mapping tests, smoke docs, audit, closeout |

---

### Containerization — Plan K K4 (THOTH_ENGINE_URL selection) ✅ (2026-07-15)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 9 · **Spec:** [`plan_k_gui_api_client.md`](plan_k_gui_api_client.md) 🔒

`AgentInterface` selects Local vs Remote from `THOTH_ENGINE_URL` at startup (trim/normalize; one log line). Engine need not be up for GUI launch. Hybrid docs updated. Pure selection unit test; backends do not read env.

---

### Containerization — Plan K K3 (SSE client) ✅ (2026-07-15)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 9 · **Spec:** [`plan_k_gui_api_client.md`](plan_k_gui_api_client.md) 🔒

Single joinable `GET /v1/events` SSE thread on `RemoteAgentBackend`; pure framing/mapping helpers; serial callbacks; cancel→abort→join shutdown; no reconnect. AgentInterface still Local-only until K4.

**Files:** `includes/remote_agent_sse_utils.h`, `remote_agent_backend.*`, `tests/unit_tests.cpp` (`testRemoteSseUtilsOffline`).

---

### Containerization — Plan K K2 (RemoteAgentBackend) ✅ (2026-07-15)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 9 · **Spec:** [`plan_k_gui_api_client.md`](plan_k_gui_api_client.md) 🔒

libcurl RemoteAgentBackend for F routes (`/health`, `/ready`, chat, goals, control). Ready gate cached; no retries; no `curl_global_*`. AgentInterface still Local-only until K4. Offline utils tests in `ctest -L pr`; live opt-in via `THOTH_REMOTE_LIVE_URL`.

**Files:** `includes/remote_agent_http_utils.h`, `includes/remote_agent_backend.h`, `src/remote_agent_backend.cpp`, CMake/tests wiring.

---

### Containerization — Plan K K1 (backend abstraction) ✅ (2026-07-14)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 9 · **Spec:** [`plan_k_gui_api_client.md`](plan_k_gui_api_client.md) 🔒

Internal `IAgentBackend` + `LocalAgentBackend` (exclusive `BasicAgentPlugin` owner). `AgentInterface` public API unchanged; always Local in K1; event bridge owned by AgentInterface. No remote/HTTP/`THOTH_ENGINE_URL` yet (K2+).

**Files:** `includes/i_agent_backend.h`, `includes/local_agent_backend.h`, `src/local_agent_backend.cpp`, `AgentInterface.*`, GUI/tests CMake.

---

### Containerization — Plan J complete ✅ (2026-07-14)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 7 · **Spec:** [`plan_j_ci_compose.md`](plan_j_ci_compose.md) 🔒

| Checkpoint | Work |
|------------|------|
| **J1** | `docker/compose.ci.yml` — `depends_on: !override []` (no llama health gate) |
| **J2** | `SMOKE_MODE=ci` in `docker/smoke.sh` — engine-only packaging smoke |
| **J3** | `.github/workflows/ci-compose.yml` — parallel to native `ctest -L pr` |
| **J4** | Docs / roadmap / plan IMPLEMENTED |

**Verification:** `SMOKE_MODE=ci ./docker/smoke.sh` → PASS (no GGUF)

---

### Containerization — Step 6 hybrid docs ✅ (2026-07-14)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 6

Docs-only: host `curl`/scripts against Compose `thoth-engine` on `:8090`; clarify GUI still in-process until Plan K; persistence boundary between container volumes and host `agent_workspace/`.

**Files:** `docker/README.md`, `docs/GETTING_STARTED.md`, `docs/docker_roadmap.md`

---

### Repair — R2 test-suite-dev mutex UB ✅ (2026-07-14)

**Symptom:** Intermittent SEGV in `ExecutiveController::execute_goal` when assigning `benchmark_attribution_` (destination `std::string` with null `_M_p`).

**Cause:** `mutex_.unlock()` / `mutex_.lock()` while a `std::lock_guard` still owned the mutex (also in `resume_from_plan`) — undefined behavior.

**Fix:** Join prior loop thread outside the lock (same pattern as `~ExecutiveController`): move `loop_thread_` under lock, join outside, then continue under a fresh `lock_guard`.

**Verification:** `ctest -R test-suite-dev`; `ctest -L pr -j1`

---

### Containerization — Plan I complete ✅ (2026-07-13)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 5 · **Spec:** [`plan_i_docker_compose_v1.md`](plan_i_docker_compose_v1.md) 🔒

| Checkpoint | Work |
|------------|------|
| **I1** | `docker/Dockerfile.engine` — multi-stage Release headless build; `.dockerignore` |
| **I2** | `llama-server` — `ghcr.io/ggml-org/llama.cpp:server` digest-pinned |
| **I3** | `docker-compose.yml` — `thoth-net`, `depends_on: service_healthy`, `unless-stopped` |
| **I4** | Env table, named volumes, `entrypoint-engine.sh` dir/bootstrap |
| **I5** | Healthchecks both services; `stop_grace_period: 30s` |
| **I6** | `docker/smoke.sh`, `docker/README.md`, `GETTING_STARTED.md` |

**Key files:** `docker/Dockerfile.engine`, `docker-compose.yml`, `docker/entrypoint-engine.sh`, `docker/smoke.sh`, `docker/README.md`

**Verification:** `docker build -f docker/Dockerfile.engine -t thoth-engine:local .`; `docker run --rm thoth-engine:local --version`; isolated HTTP smoke (`/health`, `/ready`, SSE); host `ctest -L pr` — `thoth-core-tests` passed, **`test-suite-dev` SEGFAULT** (unrelated packaging; repair pending approval)

**Packaging note:** Image sets `THOTH_PROJECT_ROOT=/workspace` as a deterministic deploy override. Root-cause fix (2026-07-14): `getProjectRoot()` terminates walk at filesystem root (`parent == current`).

---

### Containerization — Plan H complete ✅ (2026-07-13)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 2 · **Spec:** [`plan_h_inference_adapter.md`](plan_h_inference_adapter.md) 🔒

| Checkpoint | Work |
|------------|------|
| **H1** | `InferenceClient` interface, provider-neutral types, `MockInferenceClient` |
| **H2** | `OllamaClient`; `LLMInterface` + `EmbeddingEngine` wired through abstraction |
| **H3** | `LlamaServerClient` — `/v1/completions`, `/v1/embeddings`, `/health` |
| **H4** | `createInferenceClient()` factory; `THOTH_INFERENCE_BACKEND=ollama\|llama_cpp` |
| **H5** | Bootstrap logging; `GETTING_STARTED.md` backend docs |
| **H6** | Inference unit tests; embedding dimension probe (integration env-gated); PR suite |

**Key files:** `inference_client.{h,cpp}`, `ollama_client.{h,cpp}`, `llama_server_client.{h,cpp}`, `inference_http.{h,cpp}`, `llm_interface.cpp`, `embedding_engine.cpp`

**Verification:** `ctest -L pr -j1`; `THOTH_INFERENCE_INTEGRATION_TESTS=1 ./tests/thoth-core-tests` (when Ollama/llama-server reachable)

---

### Containerization — Plan G complete ✅ (2026-07-13)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 4 · **Spec:** [`plan_g_streaming_observability.md`](plan_g_streaming_observability.md) 🔒

| Checkpoint | Work |
|------------|------|
| **G1** | `EngineEvent` envelope, dispatch thread, bounded ingress, subscriber isolation |
| **G2** | `plugin->onEvent` wiring, `subscribeEvents` / `unsubscribeEvents` |
| **G3** | `GET /v1/events`, `SseSessionManager`, `"events"` capability |
| **G4** | Multi-client SSE fan-out |
| **G5** | Disconnect cleanup, per-client bounded queues (drop on overflow) |
| **G6** | SSE-aware shutdown, `docs/ENGINE_EVENTS.md`, `GETTING_STARTED.md` |

**Key files:** `engine_event.{h,cpp}`, `engine_sse_session.{h,cpp}`, `engine_runtime.{h,cpp}`, `engine_http_transport.cpp`

**Verification:** `THOTH_ENGINE_RUNTIME_TESTS=1 ./tests/thoth-core-tests`; `ctest -L pr -j1`

---

### Containerization — Plan F complete ✅ (2026-07-13)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 3 · **Spec:** [`plan_f_engine_runtime_http.md`](plan_f_engine_runtime_http.md) 🔒

**Delivered (host-native HTTP transport — no Docker images yet):**

| Checkpoint | Work |
|------------|------|
| **F1** | `EngineRuntime` (plugin, worker queue, sessions) + CLI refactor via `submitChat` |
| **F2** | `EngineError` schema, session normalization, unit tests (`THOTH_ENGINE_RUNTIME_TESTS=1`) |
| **F3** | `EngineHttpTransport`, cpp-httplib v0.15.3, `--serve` / `--bind` / `--port`, `GET /health`, `/ready`, `/version` |
| **F4** | `POST /v1/chat` — parity with `submitChat` / `--execute` |
| **F5** | `POST /v1/goals`, `POST /v1/control/{pause,resume,abort}` |
| **F6** | Graceful shutdown (`beginShutdown`, bounded queue drain, `503 ENGINE_BUSY`), `sigtimedwait` signal handling in `--serve`, `GETTING_STARTED.md` HTTP section, engine runtime tests |

**Key files:** `engine_runtime.{h,cpp}`, `engine_error.{h,cpp}`, `engine_http_transport.{h,cpp}`, `thoth_engine_main.cpp`, `third_party/httplib.h`

**Locked semantics:** `POST /v1/goals` returns after acceptance + synchronous initial planning (`create_plan`); step execution continues async on the executive loop. **F+1** `submitGoalAsync()` deferred to [`improvements.md`](improvements.md) § Containerization — Future Enhancements (Plan G SSE).

**Verification:** `THOTH_ENGINE_RUNTIME_TESTS=1 ./tests/thoth-core-tests`; `timeout -s TERM` / `SIGINT` on `--serve`; `ctest -L pr -j1`.

---

### Containerization — Plan F checkpoints F1–F5 ✅ (2026-07-13)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 3 · **Spec:** [`plan_f_engine_runtime_http.md`](plan_f_engine_runtime_http.md) 🔒

**Delivered (host-native HTTP transport — no Docker images yet):**

| Checkpoint | Work |
|------------|------|
| **F1** | `EngineRuntime` (plugin, worker queue, sessions) + CLI refactor via `submitChat` |
| **F2** | `EngineError` schema, session normalization, unit tests (`THOTH_ENGINE_RUNTIME_TESTS=1`) |
| **F3** | `EngineHttpTransport`, cpp-httplib v0.15.3, `--serve` / `--bind` / `--port`, `GET /health`, `/ready`, `/version` |
| **F4** | `POST /v1/chat` — parity with `submitChat` / `--execute` |
| **F5** | `POST /v1/goals`, `POST /v1/control/{pause,resume,abort}` |

**Key files:** `engine_runtime.{h,cpp}`, `engine_error.{h,cpp}`, `engine_http_transport.{h,cpp}`, `thoth_engine_main.cpp`, `third_party/httplib.h`

**Locked semantics:** `POST /v1/goals` returns after acceptance + synchronous initial planning (`create_plan`); step execution continues async on the executive loop. **F+1** `submitGoalAsync()` deferred to [`improvements.md`](improvements.md) § Containerization — Future Enhancements (Plan G SSE).

**Superseded by:** Plan F complete entry above (F6 added graceful shutdown + docs).

---

### Containerization — prerequisite Plans A–E ✅ (2026-07-12)

**Roadmap:** [`docker_roadmap.md`](docker_roadmap.md) Step 0

| Plan | Deliverable |
|------|-------------|
| **A** | Configurable inference endpoints (`THOTH_INFERENCE_BASE_URL`, etc.) |
| **B** | Portable workspace/logs paths (`THOTH_WORKSPACE_PATH`, `THOTH_LOGS_PATH`) |
| **C** | Headless `thoth-engine` + `engine-only` CMake preset |
| **D** | `thoth-core-tests` / `thoth-gui-tests` split; wxWidgets-free PR CI |
| **E** | Runtime bootstrap, `.env` loading, `THOTH_LOG_CONFIG` |

**Verification:** `ctest -L pr` on `engine-only`; portable paths and bootstrap unit tests green.

---

### Bug fix — memory consolidation lockup / control-panel freeze ✅ (2026-07-11)

**Symptom:** Control panel froze (required kill) while stderr repeated
`[Memory] Consolidated 0 turn(s) for session … (reasons: HOT_COUNT) [deferred: batch cap reached]`.

**Root cause (confirmed from `agent_workspace/decision_trace.jsonl`):** A poisoned
SQLite transaction. `SQLiteMemoryRepository::commit()` did not roll back on a
failed `COMMIT`, so after the first COMMIT failure the single shared connection
was left mid-transaction; every subsequent `beginTransaction()` failed instantly
(`transaction_ms: 0`, "cannot start a transaction within a transaction"). Because
the batch never archived, `HOT_COUNT` never cleared, so every new message
re-ran a multi-minute, **timeout-less** LLM summary + embed on the worker thread
(`summary_ms` up to ~227 s), which manifested as a lockup. The
`[deferred: batch cap reached]` label was also misleading — the loop actually
stopped on the no-progress guard, not the batch cap.

**Fixes:**

- `sqlite_memory_repository.cpp` — `commit()` now rolls back on COMMIT failure;
  `beginTransaction()` self-heals a stale open transaction; `rollback()` is a
  no-op when no transaction is active. Connection hardened at open with
  `busy_timeout=5000`, `journal_mode=WAL`, `synchronous=NORMAL`.
- `memory_pruner.{h,cpp}` — added a no-progress circuit breaker
  (`kMaxConsecutiveNoProgress=3`): automatic consolidation that repeatedly makes
  no progress is suppressed (emits `consolidation_backoff` trace, sets
  `ConsolidationResult::blocked`) until a successful archive, a manual
  consolidation, or session (re)activation. The `consolidation_deferred` trace
  now distinguishes an honest batch-cap pause from a no-progress failure.
- `memory.cpp` — `onSessionActivated()` clears the backoff; the `[Memory]` log
  line now reports `no forward progress — consolidation failing` vs
  `batch cap reached` accurately.
- `llm_interface.cpp` — the Ollama curl handle now sets `CONNECTTIMEOUT=10s` and
  a bounded `CURLOPT_TIMEOUT` (default 600 s, override `THOTH_LLM_TIMEOUT_SECONDS`)
  so a stalled Ollama can no longer block the worker thread indefinitely.

**Verification:** Debug build clean; full `thoth-unit-tests` suite passes
(includes `testConsolidationFailureTransaction`, `testConsolidationFailureEmbed`).

### E2 track — status at a glance

| Phase | Question | Status | Close-out |
|-------|----------|--------|-----------|
| **A** | Can execution be trusted? | ✅ Complete 2026-07-02 | A1–A5 checkpoints; E2-08–E2-11 |
| **B** | Can measurement be trusted? | ✅ Complete 2026-07-04 | [`phases/PHASE_B_COMPLETE.md`](phases/PHASE_B_COMPLETE.md); fingerprint `1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4` |
| **C** | Can trusted measurement become architecture? | ✅ Locked 2026-07-05 | [`phases/PHASE_C_COMPLETE.md`](phases/PHASE_C_COMPLETE.md) |
| **D** | Can architecture evolve without losing trust? | ✅ Complete 2026-07-08 | [`phases/PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md); D5 evolution trust proof |
| **E** | Can we defend the results scientifically? | ✅ Certified 2026-07-09 | [`phases/PHASE_E_COMPLETE.md`](phases/PHASE_E_COMPLETE.md) — scoped `n=3_strict_trio`; lift=0.0; paused before Zenodo V3 |

### E2 Phase C — Integration tier ✅ locked (2026-07-05)

**Authority:** [`C_PHASE_PROTOCOL.md`](C_PHASE_PROTOCOL.md) v1.1  
**Baseline:** Phase B v1 fingerprint unchanged  
**Close-out:** [`phases/PHASE_C_COMPLETE.md`](phases/PHASE_C_COMPLETE.md)

**Governing invariant:** Evaluation is a passive architectural service — it may observe execution, but must never influence execution.

| Checkpoint | Deliverable | Tests |
|------------|-------------|-------|
| **E2-C1** | `IEpisodicEvaluationService` — stateless façade over Phase B kernel | E2-C1-01..03 |
| **E2-C2** | `EpisodeCompleted` publication + `EvaluationSubscriber` + event channel | E2-C2-01..04 |
| **E2-C3** | `DiagnosticService` — presentation-only JSONL | E2-C3-01..04 |
| **E2-C4** | `PipelineTelemetryService` — architectural telemetry sink | E2-C4-01..05, E2-C4-03b |
| **E2-C5** | Path equivalence under pinned STRICT config on golden fixtures | E2-C5-01..05 |

**C5 record:** Benchmark orchestration (`runBenchmarkPathArtifacts`) and production orchestration (`runProductionPathArtifacts`) produce identical `evaluation_resolution`, scorable classification, failure/diagnosis buckets, and `fingerprint_hash` on mapping-safe fixtures (E2-01..03). Scope: pinned semantic config — not raw INTEGRATION-default vs STRICT object equality.

**Regression gates (all green):**

| Gate | Result |
|------|--------|
| `THOTH_E2_WIRING_STAGE=B` harness | E2 SUCCESS, 3/3 cases |
| Phase B fingerprint (E2-28) | Stable |
| `THOTH_E2_C5=1` equivalence matrix | E2-01/02/03 MATCH |
| Passive invariant | Publication default OFF; no eval → Executive callback |

**Key files (basic_agent):** `episodic_evaluation_service.*`, `episode_events.h`, `episode_event_channel.*`, `evaluation_subscriber.*`, `diagnostic_service.*`, `pipeline_telemetry_service.*`, `e2_path_equivalence.*`

**Review note:** E2-C5-03 aliases E2-C5-01; proof coverage unchanged via `diffPathEquivalence()`.

**Next:** Phase D — complete (see Phase D section below). Constitutional rule: Observe, Record, Replay, Present — Never Decide.

### E2 Phase E — Empirical validation tier ✅ certified (2026-07-09)

**Authority:** [`E_PHASE_PROTOCOL.md`](E_PHASE_PROTOCOL.md) E v0.1 🔒 · [`phases/E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) E-AP v1.1 🔒 · [`E2_PROTOCOL.md`](E2_PROTOCOL.md) v1.2 🔒  
**Close-out:** [`phases/PHASE_E_COMPLETE.md`](phases/PHASE_E_COMPLETE.md)  
**Prerequisite:** Phase D sealed — [`phases/PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md)

**Governing question:** Are empirical claims made with the trusted evaluator specification-complete, reproducible, defensible, evidence-mapped, and publication-ready within the declared scope?

| Step | Deliverable | Status |
|------|-------------|--------|
| E0 / E1 | Protocol + analysis plan lock | ✅ |
| EP-01 / EP-01.5 | Authoritative inference harness + LLM wiring | ✅ |
| Step 2 | Authoritative STRICT trio evidence | ✅ sealed |
| Step 3 | L4 verification package | ✅ `VERIFIED` |
| Step 4 | Claims audit | ✅ |
| Step 5 | Certification | ✅ |

**Empirical observation (certified scope only):** `mean_episodic_lift = 0.0` · rollup `FAILURE` / `SCORED_FAILURE` · `evidence_scope: n=3_strict_trio`. Completion ≠ positive lift. Generalization beyond the trio is outside the certified record (B1 deferred).

**Next:** Paused before Zenodo V3 / grant submission using new numbers. Post-E forks: B1, C6 Phase 3 (C6.3-01–06 ✅), E3 (SCR), M4, G1d — see `cursor_list.md`.

### C6 Phase 3 — C6.3-06 regression fixtures ✅ (2026-07-11)

**Authority:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) § C6.3-06 implementation lock · § C6.3-06-5 implementation lock  
**Depends on:** C6.3-01–05 ✅ · sub-checkpoints 06-1–06-4 ✅

| Sub-checkpoint | Deliverable | Status |
|----------------|-------------|--------|
| **06-1** | Fixture catalog (`tests/fixtures/cognitive_longitudinal/README.md`) | ✅ |
| **06-2** | Intentionally absent fixture (`missing_decision_trace.jsonl` — README only) | ✅ |
| **06-3** | Official longitudinal companion corpus + `scripts/generate_c6_official_longitudinal_fixtures.py` | ✅ |
| **06-4** | `analyzer_golden_official_longitudinal.json` + F1 + `--write-official-golden` | ✅ |
| **06-5** | Documentation seal (this entry) | ✅ |

| Deliverable | Status |
|-------------|--------|
| Four `*_official_longitudinal.jsonl` companion fixtures | ✅ |
| `analyzer_golden_official_longitudinal.json` (independent of default exploratory golden) | ✅ |
| F1 via `run_official_analysis()` → `analyzer.analyze_longitudinal` | ✅ |
| Fixture README § Regression gate (authoritative command list) | ✅ |
| Implementation invariant honored (fixture/regression infrastructure only; no analyzer behavior changes during 06-1–06-4) | ✅ |

**Phase 3 exit criteria** ([`C6_phase3_protocol.md`](C6_phase3_protocol.md) § Exit criteria):

| # | Criterion | Evidence pointer |
|---|-----------|------------------|
| 1 | C6.3-01–06 sealed in this log | This entry + prior C6.3 seals |
| 2 | Four output artifacts with reproducibility metadata | C6.3-03 Step 03-5 / O1 seal |
| 3 | Report with qualifying `evidence_scope` | F1 + official golden (06-4 seal) |
| 4 | `cursor_list.md` C6 Phase 3 row ✅ | Updated in 06-5 |
| 5 | `improvements.md` references C6.3-04 | C6.3-04 seal |

**Regression evidence:** [`tests/fixtures/cognitive_longitudinal/README.md`](../tests/fixtures/cognitive_longitudinal/README.md) § Regression gate.

**Explicitly deferred beyond C6.3-06:** negative slices (`plan-negxx`), `run_c6_regression.py`, production log samples, AC/RC/JC version bumps.

### C6 Phase 3 — C6.3-05 operator invocation guide ✅ (2026-07-11)

**Authority:** [`cognitive_longitudinal_ops.md`](cognitive_longitudinal_ops.md) · [`C6_phase3_protocol.md`](C6_phase3_protocol.md) C6.3 v0.2.1 checkpoint C6.3-05  
**Depends on:** C6.3-03 ✅ · C6.3-04 ✅

| Deliverable | Status |
|-------------|--------|
| `docs/cognitive_longitudinal_ops.md` — procedural operator guide | ✅ |
| References AC/RC/JC; no implementation semantics duplicated | ✅ |
| Expected successful run + symptom troubleshooting | ✅ |
| Example scheduling patterns (non-prescriptive) | ✅ |
| Protocol doc map updated | ✅ |
| `improvements.md` promotion procedure linked | ✅ |
| Implementation invariant honored (docs only) | ✅ |

**Next:** (historical) superseded by C6.3-06 complete entry above

### C6 Phase 3 — C6.3-04 F-series promotion gate ✅ (2026-07-11)

**Authority:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) C6.3 v0.2.1 § C6.3-04 implementation lock  
**Depends on:** C6.3-03 ✅ sealed  
**Substeps:** 04-a ✅ · 04-b ✅ · 04-c ✅ · 04-d ✅

| Deliverable | Status |
|-------------|--------|
| Normative policy in `improvements.md` (7-part structure) | ✅ |
| Mandatory vs supporting gates split | ✅ |
| Owner approval governance (no auto-promotion) | ✅ |
| Two consecutive official evaluation windows definition | ✅ |
| Promotion reversibility | ✅ |
| Thresholds via AC reference only | ✅ |
| `cursor_list.md` pointers only; §9 resolved | ✅ |
| Implementation invariant honored (docs only) | ✅ |
| Protocol exit criterion #5 (`improvements.md` references C6.3-04) | ✅ |

**Next:** (historical) superseded by C6.3-05 complete entry above

### C6 Phase 3 — C6.3-04 F-series promotion gate plan 🔒 (2026-07-11)

**Authority:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) C6.3 v0.2.1 § C6.3-04 implementation lock  
**Depends on:** C6.3-03 ✅ sealed

| Deliverable | Status |
|-------------|--------|
| Single atomic lock (04-a–d phases, not separate locks) | ✅ locked |
| Implementation invariant (no code, schema, or auto-promotion) | ✅ locked |
| Document ownership: `improvements.md` normative; `cursor_list.md` informational only | ✅ locked |
| Mandatory vs supporting gates split | ✅ locked |
| Owner approval governance (no auto-promotion) | ✅ locked |
| Two consecutive official evaluation windows definition | ✅ locked |
| Promotion reversibility | ✅ locked |
| Thresholds via AC reference only (no duplication) | ✅ locked |
| Seven-part policy structure for `improvements.md` | ✅ locked |

**Internal phases:** 04-a `improvements.md` policy → 04-b `cursor_list.md` pointers → 04-c doc sync → 04-d log seal.

**Deferred:** C6.3-05 ops docs · C6.3-06 fixtures · observational harness markdown · auto-promotion scripts.

**Next:** (historical) superseded by C6.3-04 complete entry above

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-7 implementation lock  
**Depends on:** C6.3-03 Steps 03-0 through 03-6 ✅  
**Substeps:** 03-7a ✅ · 03-7b ✅ · 03-7c ✅ · 03-7d ✅

| Deliverable | Status |
|-------------|--------|
| `analyzer_version` `0.1.0` → `0.2.0` | ✅ |
| Diff-gated golden refresh (`analyzer_golden_summary`, report headers) | ✅ |
| Doc sync (RC, protocol, AC example JSON) | ✅ |
| RC § Step 03 success criteria 1–7 | ✅ |
| Implementation invariant (no semantic changes) | ✅ honored |
| Seven-suite + CTest regression | ✅ |

**Checkpoint summary (03-0–03-7):** RC lock · renderer · threats · gates · plots · orchestration · validation/CI · version seal.

**Next:** (historical) superseded by C6.3-04 plan lock entry above

### C6 Phase 3 — C6.3-03 Step 03-7 close-out plan 🔒 (2026-07-11)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-7 implementation lock  
**Depends on:** C6.3-03 Step 03-6 ✅

| Deliverable | Status |
|-------------|--------|
| Single atomic lock (03-7a–d implementation phases, not separate locks) | ✅ locked |
| Implementation invariant (no semantic changes; version + docs only) | ✅ locked |
| `analyzer_version` `0.1.0` → `0.2.0` | ✅ locked |
| Diff-gated golden refresh (`analyzer_golden_summary`, report headers) | ✅ locked |
| Doc sync (RC, protocol, AC example JSON) | ✅ locked |
| `completed_improvements_log.md` C6.3-03 seal | ✅ locked |
| Seven-suite + CTest regression gate | ✅ locked |

**Internal phases:** 03-7a version constant → 03-7b goldens → 03-7c docs → 03-7d log seal.

**Deferred:** C6.3-04 promotion gate · C6.3-05 ops docs · C6.3-06 fixtures · observational harness markdown · `run_c6_regression.py`.

**Next:** (historical) superseded by C6.3-03 sealed entry above

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-6 implementation lock  
**Depends on:** C6.3-03 Step 03-5 ✅  
**Substeps:** 03-6a ✅ · 03-6b ✅ · 03-6c ✅ · 03-6d ✅

| Deliverable | Status |
|-------------|--------|
| `gate_status_golden.json` + `benchmark_env_gates_green.jsonl` + `metrics_gates_green.jsonl` | ✅ |
| R3 threat parity + R4 prose guard (Layer A) | ✅ |
| R5–R7 gate assertions driven from gate golden (Layer A) | ✅ |
| R8 incomplete orchestration + JSONL immutability (Layer B) | ✅ |
| R9 required artifacts + optional PNGs (Layer B) | ✅ |
| CTest: `c6-longitudinal-join`, `c6-longitudinal-analyzer`, `c6-longitudinal-reporting` | ✅ |
| Implementation invariant (no analyzer/reporting semantic changes) | ✅ honored |

**Regression gate:** all seven C6 Python suites green; `ctest -R c6-longitudinal` after CMake reconfigure.

**Next:** (historical) superseded by 03-7 plan lock entry above

### C6 Phase 3 — C6.3-03 Step 03-6 validation/CI plan 🔒 (2026-07-11)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-6 implementation lock  
**Depends on:** C6.3-03 Step 03-5 ✅

| Deliverable | Status |
|-------------|--------|
| Single atomic lock (03-6a–d implementation phases, not separate locks) | ✅ locked |
| Implementation invariant (no analyzer/reporting semantic changes) | ✅ locked |
| Layer A: R3–R7 protocol validation + `gate_status_golden.json` | ✅ locked |
| Layer B: R8–R9 engineering validation + CTest (CI-independent RC) | ✅ locked |
| R8 strengthened (empty plot_dir, JSONL immutability) | ✅ locked |
| R9 required vs optional artifacts | ✅ locked |
| Fixture read-only / no golden self-modification | ✅ locked |

**Internal phases:** 03-6a gate golden → 03-6b R3/R4 → 03-6c R8/R9 → 03-6d CTest.

**Deferred:** `analyzer_version`, final golden refresh (03-7); `run_c6_regression.py` (future, non-normative).

**Next:** (historical) superseded by 03-6 complete entry above

### C6 Phase 3 — C6.3-03 Step 03-5 CLI orchestration ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-5 implementation lock  
**Depends on:** C6.3-03 Step 03-1 ✅ · 03-4 ✅  
**Substeps:** 03-5a ✅ · 03-5b ✅ · 03-5c ✅ · 03-5d ✅

| Deliverable | Status |
|-------------|--------|
| `WriteOptions` + extended `write_outputs()` | ✅ |
| Frozen write order + JSONL immutability | ✅ |
| Two-phase summary patch (plot flags → summary only) | ✅ |
| CLI `--no-report`, `--no-plots` | ✅ |
| Delegation to report + plot modules | ✅ |
| O1–O6 orchestration tests (tempfile isolation) | ✅ |

**Regression gate:** all seven C6 suites green.

**Next:** (historical) superseded by 03-6 plan lock entry above

### C6 Phase 3 — C6.3-03 Step 03-5 orchestration plan 🔒 (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-5 implementation lock  
**Depends on:** C6.3-03 Step 03-1 ✅ · 03-4 ✅

| Deliverable | Status |
|-------------|--------|
| Single atomic lock (03-5a–d execution substeps, not separate locks) | ✅ locked |
| Artifact pipeline ownership (orchestrator delegates only) | ✅ locked |
| Write order + JSONL immutability | ✅ locked |
| Two-phase summary patch (plot flags → summary only) | ✅ locked |
| Markdown timing (pre-plot; no second pass) | ✅ locked |
| CLI (`--no-report`, `--no-plots`, `--dry-run`) | ✅ locked |
| `WriteOptions` API + O1–O6 + tempfile isolation | ✅ locked |

**Internal substeps:** 03-5a `write_outputs` → 03-5b CLI → 03-5c delegation → 03-5d O1–O6.

**Deferred:** R3–R9, CTest, `analyzer_version`, golden refresh (03-6–03-7).

**Next:** (historical) superseded by 03-5 complete entry above

### C6 Phase 3 — C6.3-03 Step 03-4 plot module ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-4 implementation lock  
**Depends on:** C6.3-03 Step 03-3 ✅  
**Substeps:** 03-4a ✅ · 03-4b ✅ · 03-4c ✅ · 03-4d ✅

| Deliverable | Status |
|-------------|--------|
| `plot_cognitive_longitudinal.py` + `PlotResult` API | ✅ |
| Three PNG renderers (success / segments / efficiency) | ✅ |
| Skip rules (`incomplete`, matplotlib unavailable) | ✅ |
| Standalone CLI (`--summary`, `--plot-dir`) | ✅ |
| P1–P8 test harness | ✅ |
| `analyzer_golden_summary.json` unchanged (read-only) | ✅ |

**Regression gate:** plot suite + five existing C6 suites green.

**Next:** (historical) superseded by 03-5 plan lock entry above

### C6 Phase 3 — C6.3-03 Step 03-4 plot module plan 🔒 (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-4 implementation lock  
**Depends on:** C6.3-03 Step 03-3 ✅

| Deliverable | Status |
|-------------|--------|
| Single atomic lock (03-4a–d execution substeps, not separate locks) | ✅ locked |
| Architectural boundary (plot module vs analyzer payload-only) | ✅ locked |
| Skip behavior (`incomplete`, matplotlib unavailable) | ✅ locked |
| Decision A: current-window-only success rate chart | ✅ locked |
| Decision B: empty segments → annotation, no skip flag | ✅ locked |
| `PlotResult` API + P1–P8 test plan | ✅ locked |
| Golden discipline — read-only `analyzer_golden_summary.json` | ✅ locked |

**Internal substeps:** 03-4a skeleton → 03-4b renderers → 03-4c CLI → 03-4d P1–P8.

**Deferred:** CLI orchestration, `--no-plots`, R8/R9, CTest, `analyzer_version` (03-5–03-7).

**Next:** (historical) superseded by 03-4 complete entry above

### C6 Phase 3 — C6.3-03 Step 03-3 safety gate wiring ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-3 implementation lock  
**Depends on:** C6.3-03 Step 03-2 ✅  
**Substeps:** 03-3a ✅ · 03-3b ✅ · 03-3c ✅ · 03-3d ✅

| Deliverable | Status |
|-------------|--------|
| `evaluate_safety_gates()` + `GateEvaluation` + `REQUIRED_GATE_HARNESSES` | ✅ |
| `compute_safety()` wired — stubs removed | ✅ |
| `THREAT_IDS` 12 → 14; gate threats active | ✅ |
| Golden diff gate — `flags`, `threats_disclosed` only | ✅ |
| G1–G5 + H10–H11 + golden H10 regression | ✅ |

**03-3d golden:** diff gate passed — removed `benchmark_regression_not_wired`; added 3× `gate_evidence_missing:*`; `threats_disclosed` now 6 sorted IDs (added `gate_evidence_missing`). Zero production-source modifications.

**Regression gate:** all six C6 suites green.

**Next:** (historical) superseded by 03-4 plan lock entry above

### C6 Phase 3 — C6.3-03 Step 03-2 threat umbrella ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-2 umbrella  
**Substeps:** 03-2a ✅ · 03-2b ✅ · 03-2c ✅ · 03-2d ✅

| Substep | Evidence | Regression gate |
|---------|----------|-----------------|
| **03-2a** | Provenance contract + P1–P7 | `test_c6_longitudinal_provenance.py` |
| **03-2b** | Threat engine H1–H9 (14 checks) | `test_c6_longitudinal_threats.py` |
| **03-2c** | Analyzer integration I1–I5 | `test_c6_longitudinal_analyzer.py` |
| **03-2d** | Golden seal H10 (full payload) | `test_c6_longitudinal_analyzer.py` |

**03-2d golden:** diff gate passed — only `threats_disclosed` changed to 5 engine-sorted IDs. Zero production-source modifications.

**Next:** (historical) superseded by 03-3 complete entry above

### C6 Phase 3 — C6.3-03 Step 03-2d golden seal ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-2d implementation lock  
**Depends on:** C6.3-03 Step 03-2c ✅

| Deliverable | Status |
|-------------|--------|
| Golden diff gate — only `threats_disclosed` changed | ✅ |
| `analyzer_golden_summary.json` updated (5 sorted threat IDs) | ✅ |
| H10 full payload compare re-enabled | ✅ |
| Production source firewall honored | ✅ |

**Regression gate:** all six C6 suites green.

**Next:** (historical) superseded by 03-2 umbrella entry above

### C6 Phase 3 — C6.3-03 Step 03-2d golden seal plan 🔒 (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-2d implementation lock  
**Depends on:** C6.3-03 Step 03-2c ✅

| Deliverable | Status |
|-------------|--------|
| Golden diff gate — only `threats_disclosed` may change | ✅ locked |
| H10 full payload compare (not threats-only) | ✅ locked |
| Re-enable `golden_summary`; no skipped tests | ✅ locked |
| Evidence block: 03-2b H1–H9 · 03-2c I1–I5 · 03-2d H10 | ✅ locked |
| Production source firewall (analyzer/threat/reporting unchanged) | ✅ locked |

**Next:** Say **implement** to update golden + re-enable H10.

### C6 Phase 3 — C6.3-03 Step 03-2c analyzer integration ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-2c implementation lock  
**Depends on:** C6.3-03 Step 03-2b ✅

| Deliverable | Status |
|-------------|--------|
| `_build_threat_inputs()` single assembly path | ✅ |
| `build_threats()` removed; `detect_threats()` wired | ✅ |
| `prior_longitudinal_path` read-before-write | ✅ |
| I1–I5 integration tests | ✅ |
| `analyzer_golden_summary.json` unchanged (03-2d) | ✅ honored |

**Default fixture threats:** `env_hash_drift`, `fingerprint_mismatch`, `prompt_evolution`, `runtime_environment_drift`, `small_sample`

**Regression gate:** `python3 scripts/test_c6_longitudinal_analyzer.py` — I1–I5 + A1–A9 green; `golden_summary` skipped until 03-2d.

**Next:** (historical) superseded by 03-2 umbrella entry

### C6 Phase 3 — C6.3-03 Step 03-2c analyzer integration plan 🔒 (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-2c implementation lock  
**Depends on:** C6.3-03 Step 03-2b ✅

| Deliverable | Status |
|-------------|--------|
| Architectural boundary — analyzer assembles only; engine detects | ✅ locked |
| `_build_threat_inputs()` single assembly path | ✅ locked |
| Frozen `ThreatInputs`; prior JSONL read-before-write | ✅ locked |
| I1–I5 integration tests (I2 delegation parity) | ✅ locked |
| `golden_summary` deferred to 03-2d | ✅ locked |
| Firewall: no golden, reporting, gates, version bump | ✅ locked |

**Next:** Say **implement** to wire `detect_threats()` into analyzer.

### C6 Phase 3 — C6.3-03 Step 03-2b threat engine ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-2b implementation lock  
**Depends on:** C6.3-03 Step 03-2a ✅

| Deliverable | Status |
|-------------|--------|
| `scripts/c6_longitudinal_threats.py` — `THREAT_IDS`, frozen `ThreatInputs`, `detect_threats()` | ✅ |
| `scripts/test_c6_longitudinal_threats.py` — H1–H8, H-null-1..5, H9 (14 checks) | ✅ |
| H5 `env.prov` on `run-c601`/`run-c602`; H6 `prior_longitudinal.jsonl` | ✅ |

**Regression gate:** `python3 scripts/test_c6_longitudinal_threats.py` — 14/14 green.

**Firewall:** analyzer, `analyzer_golden_summary.json`, reporting — unchanged.

**Next:** **03-2c** — implement per § Step 03-2c lock

### C6 Phase 3 — C6.3-03 Step 03-2b threat engine plan 🔒 (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-2b implementation lock  
**Depends on:** C6.3-03 Step 03-2a ✅

| Deliverable | Status |
|-------------|--------|
| Frozen `ThreatInputs` + pure `detect_threats()` | ✅ locked + implemented |
| `THREAT_IDS` registry (12 IDs; gates deferred 03-3) | ✅ locked |
| `distinct_non_null()` + `extract_env_fields_for_threats` (join parity) | ✅ locked |
| Tests H1–H8, H-null-1..5, H9; analyzer isolated | ✅ locked + implemented |
| Firewall: no analyzer, golden, reporting, CLI, version bump | ✅ honored |

**Next:** (historical) plan superseded by implementation entry above

### C6 Phase 3 — C6.3-03 Step 03-2a provenance contract ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Provenance fields (locked)  
**Depends on:** C6.3-03 Step 03-2 umbrella 🔒

| Deliverable | Status |
|-------------|--------|
| Composite `provenance_pin` = `(thoth_git_sha, basic_agent_git_sha)` | ✅ locked + implemented |
| `scripts/c6_longitudinal_provenance.py` — normalize, extract, `prompt_evolution_detected` | ✅ |
| `scripts/test_c6_longitudinal_provenance.py` — P1–P7 | ✅ |
| H5 fixture spec (`run-c601` / `run-c602` distinct tuples; physical edits in 03-2b) | ✅ locked |

**Regression gate:** `python3 scripts/test_c6_longitudinal_provenance.py` — 7/7 checks green.

**Production survey** (`logs/benchmark_env.jsonl`, 2026-07-10): 74+ `BENCHMARK_ENV` rows; `env.prov` keys `thoth_git_sha`, `basic_agent_git_sha`, `captured_at_ms`; P7 verifies production shape; example tuple drift `("9bf8fd5", "649d32c")` vs `("0a38f22", "77508c4")`.

**Next:** **03-2c** — analyzer integration

### C6 Phase 3 — C6.3-03 Step 03-2 threat umbrella 🔒 (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-2 umbrella lock  
**Depends on:** C6.3-03 Step 03-1 ✅

| Deliverable | Status |
|-------------|--------|
| Substep decomposition 03-2a → 03-2b → 03-2c → 03-2d | ✅ locked |
| `runtime_environment_drift` rename + intent (replaces draft `hardware_env_drift`) | ✅ locked |
| `THREAT_SCHEMA_VERSION` 1.0 normative for detection + reporting | ✅ locked |
| H1–H8 tests; gate threats deferred to 03-3 | ✅ locked |

**Next:** Implement **03-2b** (plan locked; await explicit **implement**)

### C6 Phase 3 — C6.3-03 Step 03-1 renderer ✅ (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-1 implementation lock  
**Depends on:** C6.3-03 Step 03-0 ✅

| Deliverable | Status |
|-------------|--------|
| `scripts/c6_longitudinal_report.py` — `REPORT_SECTIONS`, `THREAT_SCHEMA_VERSION`, `THREAT_LABELS`, `render_report_sections()`, `render_markdown_report()` | ✅ |
| `scripts/test_c6_longitudinal_reporting.py` — R1, R1b, R2 | ✅ |
| `report_header_golden.md`, `report_header_incomplete_golden.md` | ✅ |

**Regression gate:** `python3 scripts/test_c6_longitudinal_reporting.py` — 3/3 checks green.

**Next:** Implement **03-2b** (plan locked; await explicit **implement**)

### C6 Phase 3 — C6.3-03 Step 03-1 renderer plan 🔒 (2026-07-10)

**Authority:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Step 03-1 implementation lock  
**Depends on:** C6.3-03 Step 03-0 ✅

| Deliverable | Status |
|-------------|--------|
| Step 03-1 implementation plan — `REPORT_SECTIONS`, `THREAT_SCHEMA_VERSION`, R1/R1b/R2, file map, scope firewall | ✅ locked (superseded by implementation entry above) |

**Approved for implementation.** Implemented in Step 03-1 renderer entry above.

**Next:** (historical) Step 03-1 code

### C6 Phase 3 — C6.3-03 Step 03-0 reporting contract ✅ (2026-07-10)

**Authority:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) C6.3 v0.2.1 🔒 · [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 🔒  
**Depends on:** C6.3-02 ✅

| Deliverable | Status |
|-------------|--------|
| `docs/C6_phase3_reporting_contract.md` — markdown schema, plot specs, threat table, gate semantics, CI scope, R1–R9 tests | ✅ locked |

**Locked decisions:** separate RC v1.0 (not AC v1.1); conservative missing-gate semantics; summary-JSON-only plots; skip plots on incomplete reports; `analyzer_version` → `0.2.0` at implementation (03-7); required gates `reflection_ab`, `robustness`, `episodic_learning`; `REPORT_SECTIONS` registry; `THREAT_SCHEMA_VERSION` 1.0.

**Next:** Implement **03-2b** (plan locked; await explicit **implement**)


### C6 Phase 3 — C6.3-02 longitudinal analyzer ✅ (2026-07-10)

**Authority:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) C6.3 v0.2.1 🔒 · [`C6_phase3_analyzer_contract.md`](C6_phase3_analyzer_contract.md) AC v1.0 🔒  
**Depends on:** C6.3-01 ✅

| Deliverable | Status |
|-------------|--------|
| `scripts/analyze_cognitive_longitudinal.py` — join → window → stats → full JSONL payload | ✅ |
| `scripts/test_c6_longitudinal_analyzer.py` — A1–A9 + A4b–A4h trend/confidence tests | ✅ |
| Fixture extensions — terminal allowlist anchor, `analyzer_*_golden.json` | ✅ |

**Regression gate:** `python3 scripts/test_c6_longitudinal_analyzer.py` — 13/13 checks green.

**Outputs:** `logs/cognitive_longitudinal.jsonl` (append, full payload) · `logs/cognitive_longitudinal_summary.json` (latest snapshot).

**Join library fix:** `validate_env_rows` skips non-`BENCHMARK_ENV` sidecar rows (harness terminal events) without invalid-row penalty — required for production `benchmark_env.jsonl`.

**Next:** Implement **03-2b** (plan locked; await explicit **implement**)

### C6 Phase 3 — C6.3-01 join library + golden regression ✅ (2026-07-10)

**Authority:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) C6.3 v0.2.1 🔒 · [`C6_phase3_join_contract.md`](C6_phase3_join_contract.md) JC v1.0 🔒  
**Checkpoint:** C6.3-01 Steps 0–4 sealed — join-only (read-only; no runtime influence)

| Step | Deliverable | Status |
|------|-------------|--------|
| 0 | Join contract + v0.2.1 errata | ✅ |
| 1 | Synthetic fixtures + `golden_join_results.json` (10 cases) | ✅ |
| 2 | `scripts/c6_longitudinal_join.py` — deterministic join library | ✅ |
| 3 | `scripts/test_c6_longitudinal_join.py` — T1–T6 golden regression | ✅ |
| 4 | `--smoke` production smoke (segment-level counters, `report_completeness`) | ✅ |

**Regression gate:** `python3 scripts/test_c6_longitudinal_join.py` — 10/10 golden cases, deterministic output, idempotence, input immutability, validation smoke, C6-05 `MISSING_ARTIFACT`.

**Smoke (non-gating):** `python3 scripts/c6_longitudinal_join.py --smoke` — production path defaults; join problems do not fail exit code.

**Key files:** `scripts/c6_longitudinal_join.py`, `scripts/test_c6_longitudinal_join.py`, `tests/fixtures/cognitive_longitudinal/*`

**Next:** Implement **03-2b** (plan locked; await explicit **implement**)

### Phase E — EP-01 episodic authoritative inference harness ✅ (2026-07-09)

**Authority:** `cursor_list.md` § E.0.0 EP-01  
**Gate:** `THOTH_E2_EP01=1` (E2-29 → E2-28 spot-check → E2-30)

- Dual-mode `run_episodic_learning_benchmark`: `--mock` (default) · `--authoritative` / `--full`
- Isolated `inferTier()` branch for `episodic_learning_benchmark`
- Infrastructure only — no Phase E benchmark evidence in EP-01

### Phase E — EP-01.5 authoritative LLM wiring & planner contract ✅ (2026-07-09)

**Authority:** `cursor_list.md` § E.0.0 EP-01.5  
**Gates:** `THOTH_E2_EP015_PHASE1`…`PHASE5`; full close-out `THOTH_E2_EP015=1`

| Phase | Deliverable | Gate |
|-------|-------------|------|
| 1 | Owned `LLMInterface` + `set_llm_interface` in harness only | E2-31 |
| 2 | Declared tier `OLLAMA` at inputs boundary | E2-31b |
| 3 | Execution + pre-summary fail-closed (`AUTHORITATIVE_LLM_NOOP`) | E2-32 |
| 4 | Planner/LLM contract in `E2_PROTOCOL.md` | docs |
| 5 | E2-29 / E2-30 regression preserved | `THOTH_E2_EP015_PHASE5=1` |

- Does **not** change scoring formulas, corpus, or thresholds
- Unblocked Step 2 redo after investigation-hold artifacts (pre-EP-01.5) were invalidated

### Phase E Step 2 — authoritative STRICT trio evidence ✅ sealed (2026-07-09)

**Authority:** `cursor_list.md` § E.0.0 Step 2 · [`phase_e_strict_v1.md`](benchmark_results/phase_e_strict_v1.md)  
**Evidence scope:** `n=3_strict_trio`  
**Status:** ✅ Sealed after EP-01.5 redo — valid empirical evidence for Phase E v0.1  
**Runs:** A `run-1783639167839` · B `run-1783639378206`  
**E2-28:** PASS (bucket #0)  
**Seal commits:** `0a38f22` (sync) · `51b9cf0` (artifact seal) · `d5df718` (empirical-observation wording)  
**Outcome:** `mean_episodic_lift = 0.0` (reported; not softened)  
**Artifacts:** `docs/baselines/artifacts/phase_e/` · `docs/baselines/phase_e_run_manifest.json`

> **Superseded HOLD:** An earlier same-day investigation hold (LLM no-op pre-flight; runs `run-1783628170667` / `run-1783628248447`) is **not** certified evidence. Only the sealed redo above is in the Phase E v0.1 record.

### Phase E Step 3 — L4 verification package ✅ (2026-07-09)

**Authority:** `cursor_list.md` § E.0.0 Step 3  
**Status:** `l4_status: VERIFIED` · `e_q2_verification: true` · execution-based reproduction **deferred**  
**Package SHA-256:** `70d25560981f9c3322e59589e5867dda77c226833b3e1e7fb395fa3aef98a6ff`  
**Artifacts:** [`phase_e_l4_status.json`](baselines/phase_e_l4_status.json) · [`phase_e_l4_verification.md`](baselines/phase_e_l4_verification.md) · [`PHASE_E_PROVENANCE.md`](baselines/PHASE_E_PROVENANCE.md)  
**Plan / implementation:** `07491b4` · `e7e60ff`

### Phase E Step 4 — claims audit ✅ (2026-07-09)

**Authority:** `cursor_list.md` § E.0.0 Step 4  
**Artifacts:** [`phase_e_claims_audit.md`](baselines/phase_e_claims_audit.md) · [`phase_e_claims_audit.json`](baselines/phase_e_claims_audit.json)  
**Plan / implementation:** `3230362` · `6fdf086`  
**Result:** Remaining publishable claims map to certified evidence; negative findings retained (no quiet softening of lift=0.0)

### Phase E Step 5 — certification close-out ✅ (2026-07-09)

**Authority:** `cursor_list.md` § E.0.0 Step 5 · plan lock `a9ee09c`  
**Artifact:** [`phases/PHASE_E_COMPLETE.md`](phases/PHASE_E_COMPLETE.md) (certification only — no new analysis)  
**E-Q1…E-Q5:** answered with outward pointers; E-Q5 = scoped publication readiness only  
**Pause:** before Zenodo V3 / grant submission using new numbers

### E2 Phase D3 — Observability without influence ✅ complete (2026-07-07)

**Authority:** [`D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md) § D3, [`cursor_list.md`](cursor_list.md) § D.3.0  
**Proof obligation:** Operational observability (metrics + trace) without reverse dependency or decision influence on the cognitive pipeline.

| Step | Invariant | Gate |
|------|-----------|------|
| 1 | Subscriber skeleton + coexistence | `THOTH_E2_D3_STEP1=1` |
| 2 | Metrics sink-only (E2-D3-01) | `THOTH_E2_D3_01=1` |
| 3 | Failure isolation (E2-D3-02) | `THOTH_E2_D3_02=1` |
| 4 | Structural audit (E2-D3-03) | `THOTH_E2_D3_03=1` |
| 5 | Plugin/config integration proof | `THOTH_E2_D3_05=1` |
| 6 | Umbrella proof-suite regression | `THOTH_E2_D3=1` |

**Umbrella gate:** `THOTH_E2_D3=1` executes the complete D3 proof suite (Steps 1–5). Each step establishes a different architectural invariant.

**Step 6 close-out (2026-07-07):** `THOTH_E2_D3=1`, `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` green; full unit suite green (~15 min); G2 `ctest -R thoth-unit-tests` **993.5s** (within 1800s budget).

**Key files:** `metrics_subscriber.*`, `trace_subscriber.*`, `basic_agent_plugin.cpp`, `config.h` / `config.cpp`, `tests/unit_tests.cpp`

**Next:** D4–D5 ✅ complete — see Phase D section below.

### E2 Phase D — Evolution tier ✅ complete (2026-07-08)

**Authority:** [`D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md) · [`D5_PROTOCOL.md`](D5_PROTOCOL.md) v0.1 🔒  
**Close-out:** [`phases/PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md)

**Governing question:** Can architecture evolve without losing trust?

| Checkpoint | Deliverable | Gate |
|------------|-------------|------|
| **E2-D1** | Event channel fan-out — Executive invisibility | `THOTH_E2_D1=1` |
| **E2-D2** | Episode replay + benchmark authority isolation | `THOTH_E2_D2=1` |
| **E2-D3** | Observability without authority | `THOTH_E2_D3=1` |
| **E2-D4** | Live INTEGRATION + STRICT authority preservation | `THOTH_E2_D4=1` |
| **E2-D5** | Evolution trust meta-proof (authority + behavioral + determinism + closure) | `THOTH_E2_D5=1` |

**D5 sub-gates:** `THOTH_E2_D5_AUTHORITY=1` (E2-D5-03), `THOTH_E2_D5_C5=1` (E2-D5-01), `THOTH_E2_D5_DETERMINISM=1` (E2-D5-02).

**Closure (2026-07-08):** `THOTH_E2_D5=1` green (~4.3 min sequential). Phase seal recorded in `PHASE_D_COMPLETE.md`. Preservation only — not promotion.

**Next:** Phase E v0.1 ✅ certified 2026-07-09 — [`phases/PHASE_E_COMPLETE.md`](phases/PHASE_E_COMPLETE.md). Paused before Zenodo V3.

### Cognitive hardening roadmap (C1–C7) — status at a glance

| ID | Area | Status | Log section |
|----|------|--------|-------------|
| **C1** | Planning quality / context management | ✅ Phases 1–5 | 2026-06-27 (C1) |
| **C2** | Chat RAG / retrieval ranking | ✅ Phases 0–3 | 2026-06-27 (C2) |
| **C3** | Reflection A/B measurement | ✅ | 2026-06-26 (C3) |
| **C4** | Developer & CI latency | ✅ Phases 1–2 | 2026-06-26 (C4) |
| **C5** | Robustness & failure tests | ✅ 10 cases | 2026-06-28 (C5) |
| **C6** | Cognitive metrics | ✅ Phases 1–2 | 2026-06-27 (C6 P1), 2026-06-29 (C6 P2) |
| **C7** | Runtime / production latency | ✅ Phases 1–3 | 2026-06-26 (C7) |

**GitHub (2026-07-01):** Thoth `de5a469`, Basic_agent `4c45aca` on `main` — E1 checkpoints D3–D5 (robustness, chat-RAG, GRAG harness wiring) pushed.

**NOTE ON DOCUMENTATION ACCURACY (2026-03-30)**: An internal audit revealed that some features listed as "complete" are actually in a prototype or stub state. This document is being updated to reflect the actual implementation status:
- **Self-Building Capability**: Harness tools exist (`project_analyze`, `run_tests`, `code_modify` read); **`apply_diff` is a stub**. Treated as **optional future expansion** — not scheduled active work (2026-06-17).
- **Trajectory Awareness**: Infrastructure is implemented; production `retrieval_config.json` sets `trajectory: 0.2` (2026-06-15). Benchmark shows mixed lift on `TRAJECTORY_DISAMBIGUATES` cases — see `docs/plan_reuse_tuning.md`.
- **Hierarchical Subgoals**: This is still in the planning phase.
- **Trace Resumption**: Full resumption is currently only authoritative through the SQLite persistence layer; log replay is for observability.

### E1 — Benchmark environment pinning ✅ complete

**Spec:** `docs/benchmark_environment.md` (v3.1). **Checkpoints A–E ✅** — closed 2026-07-01.

#### Checkpoint E — 2026-07-01

- **Scope:** Step 7 narrow double-bind mismatch in `BenchmarkRun::bindIndex()`; `scripts/compare_benchmark_env.py`; `check_baseline.py --require-env` (opt-in); five-harness identity + close-out pass; E1-17.
- **Step 7:** Second `bindIndex()` with different `index_hash` populates `index_mismatch { prior_hash, new_hash }` on sidecar + `BENCHMARK_INDEX_BOUND` JSONL; `run_id` / `environment_hash` unchanged. No `IndexManager` or production path changes.
- **Tests:** E1-17 green; E1-01–E1-16 green (`THOTH_MOCK_EPISODIC=1` full unit suite); `run_test_suite --dev` 7/7; `check_baseline.py --require-env` smoke; `compare_benchmark_env.py --strict` smoke on D1 sidecar.
- **Files:** `benchmark_context.cpp`, `tests/unit_tests.cpp`, `scripts/compare_benchmark_env.py`, `check_baseline.py`, `docs/benchmark_environment.md`.

**Five-harness identity pass (single table — close-out evidence):**

| Harness | Trigger | run_id | env_hash | index_hash | Terminal | Cognitive metrics |
|---------|---------|--------|----------|------------|----------|-------------------|
| D1 `run_test_suite` | E1-12 + `--dev` smoke | ✅ sidecar + JSONL | ✅ matches sidecar | ✅ non-empty after bind | `TEST_SUITE_COMPLETE` | ✅ 1 row / goal; attribution match |
| D2 `run_reflection_ab_benchmark` | E1-13 smoke | ✅ | ✅ | ✅ | (smoke path) | ✅ attribution match |
| D3 `run_robustness_suite` | E1-14 smoke | ✅ | ✅ | ✅ | (smoke path) | ✅ attribution match (C5-09 = 6 rows documented) |
| D4 `run_chat_rag_benchmark` | E1-15 smoke | ✅ | ✅ | ✅ | (smoke path) | **0** rows (retrieval-only) |
| D5 `run_grag_benchmark` | E1-16 smoke | ✅ | ✅ | ✅ | (smoke path) | **0** rows (retrieval-only) |

**Close-out:** All 9 boxes in `benchmark_environment.md` § Close-out criteria ticked. **E1 ✅ — proceed to E2 / G1d / B1.**

#### Checkpoint D5 — 2026-07-01

- **Scope:** `run_grag_benchmark` — Ollama preflight; env capture after research corpus index; `BenchmarkRunIdentity` passed to `BenchmarkReporter::reportToFile` (adopts E1 `run_id`/`env_hash`; legacy `benchmark-{timestamp}` fallback when identity empty); `GragBenchmarkRunRecorder` RAII; `sample_mode`/`cases_run` in `GRAG_BENCHMARK_COMPLETE` payload only (not env hash).
- **Pre-flight trace:** No `execute_goal`, `LLMPlanner`, `THOTH_MOCK_LLM`, or test-suite mock plumbing. `--sample` only truncates case list. `BenchmarkRunner` “mock” episode steps are SQLite trajectory fixtures for embedding — not LLM mocks. Real Ollama External embeddings throughout.
- **E1-16:** Full JSONL row assertion via existing `THOTH_WORKSPACE_PATH` (no new FileHandler hook).
- **Metrics note:** **0** `GOAL_COGNITIVE_METRICS` rows — retrieval-only.
- **Tests:** E1-16 green; `--sample` happy path (10 cases, exit 0); `GRAG_BENCHMARK_ABORTED` via `THOTH_GRAG_BENCHMARK_ABORT_SMOKE=1`.
- **Files:** `run_grag_benchmark.cpp`, `benchmark_reporter.h/cpp`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: E**.

#### Checkpoint D4 — 2026-07-01

- **Scope:** `run_chat_rag_benchmark` — env capture after golden corpus index (External embeddings, `BenchmarkTier::OLLAMA`, `fetchOllamaSnapshot`); single `run_id` on all `CHAT_RAG_BENCHMARK_CASE` + `CHAT_RAG_BENCHMARK_SUMMARY` rows; `ChatRagRunRecorder` RAII (`CHAT_RAG_BENCHMARK_COMPLETE` / `CHAT_RAG_BENCHMARK_ABORTED`).
- **Ollama policy:** E1-15 is Ollama-independent (probe-stack smoke). Happy-path Ollama run + abort smoke are required stop gates when Ollama is reachable; if not reachable at commit time, log **“D4 wired, happy-path verification pending”** per `benchmark_environment.md` § Ollama availability.
- **Metrics note:** **0** `GOAL_COGNITIVE_METRICS` rows — retrieval-only harness; no `execute_goal`.
- **Tests:** E1-15 green; Ollama happy path 5/5 hit@1 (`run_id` on 6 JSONL rows); `CHAT_RAG_BENCHMARK_ABORTED` verified via `THOTH_CHAT_RAG_BENCHMARK_ABORT_SMOKE=1`.
- **Files:** `run_chat_rag_benchmark.cpp`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: D5** (`run_grag_benchmark`).

#### Checkpoint D3 — 2026-07-01

- **Scope:** `run_robustness_suite` — probe-stack capture before case loop (`THOTH_MOCK_LLM` baseline, `clearRobustnessEnv()` per-case runs after create); single `run_id` across 7 `execute_goal` calls via `runRobustnessCase(spec, attribution)` / `runExecutiveCase` / C5-09 / C5-10; `RobustnessRunRecorder` RAII; `run_id`/`env_hash` on harness JSONL rows.
- **Metrics note (verified):** 7 attributed `execute_goal` invocations produce **6** `GOAL_COGNITIVE_METRICS` rows. **C5-09** uses one `ExecutiveController` (not two): slow then fast on the same instance. Pre-D3 baseline rebuilt from `fb4fd9f` robustness sources (`THOTH_COGNITIVE_METRICS_LOG` → 6 rows, no `slow concurrent goal`) — same count as post-D3. Mechanism: second `execute_goal` sets `stop_requested_`, joins `loop_thread_`; `run_loop()` exits without COMPLETED/FAILED → no metrics emit (contrast `abort()`, which does emit). Both C5-09 calls pass the same suite-wide `BenchmarkAttribution`; slow goal never reaches terminal emit, so attribution overwrite is immaterial. **Not an E1 regression.**
- **C5-10:** `teardown goal` metrics row present with matching `run_id`/`env_hash`; case still `DESTROYED no_crash=true`.
- **Tests:** E1-14 green; harness 10/10; `ROBUSTNESS_ABORTED` verified via `THOTH_ROBUSTNESS_BENCHMARK_ABORT_SMOKE=1`.
- **Files:** `run_robustness_suite.cpp`, `robustness_cases.h/cpp`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: D4** (`run_chat_rag_benchmark`).

#### Checkpoint D2 — 2026-07-01

- **Scope:** `run_reflection_ab_benchmark` — probe-stack env capture (empty TfIdf index) before case loop; single `run_id` across 4 `execute_goal` calls; `ReflectionAbRunRecorder` RAII (`REFLECTION_AB_COMPLETE` / `REFLECTION_AB_ABORTED`); `run_id`/`env_hash` on `REFLECTION_AB_CASE` + `REFLECTION_AB_SUMMARY`; worker-thread crash limitation documented in `benchmark_environment.md` § Harness terminal events.
- **Pre-flight (D1 crash trace):** D1's worker-thread `std::terminate` was on the **test-suite path** (`LLMPlanner` + `THOTH_TEST_SUITE_DEV` mock responses + multi-step RETRIEVAL/LLM goals). Reflection A/B uses **`ReflectionAbMockPlanner`** (deterministic plans, no LLM JSON parsing), does not wire `LLMInterface`, and does not enable `THOTH_TEST_SUITE_DEV` — **does not share the crash code path**. Shared `THOTH_MOCK_LLM` alone is not the fragility; test-suite-specific mock plumbing is.
- **Tests:** E1-13 green (direct-controller smoke + non-empty `index_hash`); harness 2/2 cases, mean lift 0.5; 4 cognitive metrics rows share `run_id`/`env_hash`; `REFLECTION_AB_ABORTED` verified via `THOTH_REFLECTION_AB_BENCHMARK_ABORT_SMOKE=1`.
- **Files:** `run_reflection_ab_benchmark.cpp`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: D3** (`run_robustness_suite`).

#### Checkpoint D1 — 2026-07-01

- **Scope:** `BasicAgentPlugin::buildTestSuiteBenchmarkInputs()` + `benchmarkIndexEnvironment()` (engine via `rag.engine` after move); `run_test_suite` captures env after `setRagFiles`, single `run_id` for suite; all `executeGoal` calls pass `run.attribution()`; stdout one-line env summary; `TestSuiteRunRecorder` RAII emits `TEST_SUITE_COMPLETE` or `TEST_SUITE_ABORTED` on all exit paths.
- **Tests:** E1-12 green (harness helper path smoke — sidecar + `cognitive_metrics.jsonl` `run_id`/`env_hash` match); `run_test_suite --dev` all TC pass; sidecar + `benchmark_env.jsonl` + cognitive metrics aligned; `TEST_SUITE_ABORTED` verified via `THOTH_TEST_SUITE_BENCHMARK_ABORT_SMOKE=1` (main-thread early exit → JSONL terminal event).
- **Files:** `basic_agent_plugin.h/cpp`, `tests/run_test_suite.cpp`, `tests/unit_tests.cpp`.
- **Safe stop — next checkpoint: D2** (`run_reflection_ab_benchmark`).

#### Checkpoint C — 2026-06-30 (closed 2026-07-01)

- **Scope:** `execute_goal(goal, BenchmarkAttribution={})` on `ExecutiveController`; `benchmark_attribution_` stored before `loop_thread_`; `GoalCognitiveMetricsRecord` + `CognitiveMetricsLogger::toJson` emit optional `run_id`/`env_hash`; `BasicAgentPlugin::executeGoal` overload; `THOTH_COGNITIVE_METRICS_LOG` test override.
- **Tests:** E1-09 – E1-11 green (attribution present; omitted without attribution; worker-thread cross-thread guard).
- **E1-11 guard (2026-07-01):** Asserts `STEP_STARTED` on thread ≠ caller (mid-execution worker read, not post-hoc luck); waits for terminal callback before thread check; final metrics must carry attribution — would fail harness-thread thread-local regression.
- **Files:** `executive_controller.h/cpp`, `cognitive_metrics.h`, `cognitive_metrics_logger.cpp`, `basic_agent_plugin.h`, `benchmark_execution_contract.h`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: D1** (harness wiring begins; one harness per sub-session).

#### Checkpoint B — 2026-06-30

- **Scope:** `BenchmarkRun` / `BenchmarkContext::create`, sidecar + JSONL emit, `bindIndex()` merge under mutex, `GitMetadata`, `fetchOllamaSnapshot` / `isOllamaReachable`, `benchmark_execution_contract.h` design lock.
- **Tests:** E1-07 – E1-08 green (sidecar create + concurrent bindIndex merge).
- **Files:** `benchmark_context.*`, `git_metadata.*`, `ollama_snapshot.*`, `benchmark_execution_contract.h`, `tests/unit_tests.cpp`, `external/basic_agent/CMakeLists.txt`.
- **Safe stop — next checkpoint: C** (still no harness/GUI wiring).

#### Checkpoint A — 2026-06-30

- **Scope:** Nested benchmark environment structs; pure `assembleEnvironment()`; `inferTier()` / `hasTierMismatch()`; `computeEnvironmentHash()` (index-excluded) + `computeIndexHash()`; JSON round-trip; SHA-256 helper.
- **Tests:** E1-01 – E1-06 green (`thoth-unit-tests` with `THOTH_MOCK_EPISODIC=1`).
- **Files:** `external/basic_agent/include/benchmark_environment.h`, `external/basic_agent/src/benchmark_environment.cpp`, `tests/unit_tests.cpp`, `external/basic_agent/CMakeLists.txt`.
- **Safe stop — next checkpoint: B** (no production call sites touched).

<!--
#### Checkpoint B — (date)
- **Scope:** BenchmarkContext, GitMetadata, OllamaSnapshot, design-lock headers
- **Tests:** E1-07 – E1-08 green
- **Next:** Checkpoint C

#### Checkpoint C — (date)
- **Scope:** BenchmarkAttribution → execute_goal → CognitiveMetricsLogger (all call sites)
- **Tests:** E1-09 – E1-11 green; ctest fast green
- **Next:** Checkpoint D1

#### Checkpoint D1–D5 — (date each)
- **Scope:** one harness wired per sub-session
- **Next:** D(n+1) or E

#### Checkpoint E — (date)
- **Scope:** BENCHMARK_INDEX_BOUND, Python scripts, close-out
- **Tests:** E1-12; E1 close-out checklist complete
- **Status:** E1 ✅ — move summary to permanent section below
-->

### 2026-06-26 (E1 — v3.1 spec approved; checkpoint plan)

- **Technical:** v3.1 approved — explicit `BenchmarkAttribution` via `execute_goal()` (C7 `StepExecutionContext` pattern); no `setActive()` / thread-local. Sidecar mutex for `bindIndex()`; `check_baseline.py --require-env` opt-in only.
- **Process:** Multi-session checkpoints **A–E** — each ends compile-clean, test-green, committable. **Checkpoint C isolated** — hot-path signature change must not span sessions. Harness wiring D1–D5 one per sub-session.
- **Doc:** `docs/benchmark_environment.md` — authoritative implementation spec.
- **Next:** Checkpoint **A** (E1-01–E1-06).

### 2026-06-30 (M3 — `/prune` operational interface)

- **API:** `ConsolidationSource` (AUTOMATIC/MANUAL) separate from `ConsolidationReason`; `ConsolidationStatus`, `ConsolidationRequest`, `ConsolidationResult` frozen public contract.
- **Memory:** `getConsolidationStatus()`, `runConsolidation()`, `setGoalActiveChecker()`; goal-active guard unless `--unsafe`.
- **CLI:** `/prune` (default status), `explain`, `batch`, `run`; `--ignore-thresholds`, `--unsafe`.
- **Tracing:** `admin_command` + `memory_consolidation` with `requested_by`, `source`, `decision.reasons`.
- **Fix:** `configureConsolidation` uses `rag.engine.get()` (embed engine after move).
- **Tests:** M3-01 – M3-09 pass under `THOTH_MOCK_EPISODIC=1`.

### 2026-06-29 (Reflection & analysis — eval gap, consolidated roadmap)

- **Context:** C1–C7 + M1–M2 complete; external review (×2) + implementation analysis captured in `cursor_list.md` § Reflection & analysis.
- **Core gap:** Component/per-run eval exists; **longitudinal learning eval** does not — thesis claims (strategy promotion, consolidation → behavioral lift) need C6 Phase 3 + E2/E3.
- **New proposed IDs:** E1 (env pinning), E2 (episodic learning eval), E3 (SCR harness), G1d (trajectory ablation).
- **Roadmap:** M3/M4 → G1d+E1 → C6 Phase 3+E2+E3 → B1 (V3) → F3/F1 when eval shows bottleneck.

### 2026-06-29 (External review — documentation honesty)

Independent review (2026-06): Thoth docs are **unusually candid** about limits — mock Cognate benchmarks report 0.00 task success, trajectory $w_t$ shows mixed lift, subgoal trees are not built, and the 51× Cognate reasoning-depth figure is footnoted as iteration count under mock conditions (not real task completion). Captured in `cursor_list.md` § External review and `audit.md` §5 Known Gaps.

### 2026-06-29 (M2 — Age-based consolidation policy)

- **Policy:** `ConsolidationDecision` with `ConsolidationReason` bitmask (HOT_COUNT, SESSION_INACTIVE, OLDEST_MESSAGE; TOKEN_LIMIT/MEMORY_PRESSURE reserved).
- **Clock:** `Clock` / `SystemClock` / `FakeClock` injected into `MemoryPruner`.
- **Config:** `config.json` → `memory.max_hot_messages`, `memory.max_hot_age_days`, `memory.prune_batch_size`.
- **Orchestration:** `consolidateIfNeeded()` — batch loop (cap 5, no-progress guard, deferred trace); startup discovery without LLM; deferred consolidation on session switch.
- **GUI:** Timestamp-preserving `loadConversation()`; consolidate-before-trim on sync.
- **Tests:** `testM2StaleSessionUnderCap`, `testM2FreshSessionNoOp`, `testM2StartupDeferredNonActive`, `testM2TimestampPreservation`, `testM2MultiTriggerReasons`, `testM2BatchCapDeferred`.

### 2026-06-26 (M1.5 — Episodic verification gate complete; M1 verified)

- **Status:** M1 **verified**; M1.5 **complete**. Proceed to **M2**.
- **Gate spec:** `docs/episodic_memory_benchmark.md`
- **Tests passed** (`THOTH_MOCK_EPISODIC=1`):
  - M1.5a: `testEpisodicRetrievalEndToEnd` — Apollo fact → consolidate → warm → GRAG retrieval
  - M1.5b: `testConsolidationFailureEmbed`, `testConsolidationFailureTransaction` — hot unchanged on embed/DB failure
  - M1.5c: `testConsolidationLatencyRecorded` — `summary_ms`, `embed_ms`, `transaction_ms`, `consolidation_ms`
  - M1.5d: `testEpisodicMemoryBenchmarkNegative` — positive control + unrelated-query negative + under-cap guard

### 2026-06-29 (M1 — Memory consolidation implemented; M1.5 verification gate)

- **Status:** M1 **implemented**; verification tracked as **M1.5** (`episodic_memory_benchmark.md`).
- **Design**: `docs/memory_architecture.md` — working → episodic → semantic → procedural → archival; consolidation terminology.
- **Delivered**: EpisodicMemory, SummaryGenerator, warm_memory + warm_memory_embeddings, atomic txn, GRAG merge, GUI sync path.
- **Verification:** M1.5 gate — see 2026-06-26 entry above.

### 2026-06-29 (M1 — Memory consolidation / warm episodic tier — implementation detail)

- **Design**: `docs/memory_architecture.md` — working → episodic → semantic → procedural → archival; consolidation terminology.
- **EpisodicMemory** structured source of truth; JSON persistence only; `EpisodicMemoryRenderer` for prompt prose; canonical embed text for retrieval.
- **SummaryGenerator** → LLM (or `THOTH_MOCK_EPISODIC=1` in tests); fail-open with `summary_missing` + raw cold archive.
- **Atomic txn**: LLM/embed outside txn; `consolidateSessionBatch` writes warm + embedding + archive + hot delete in one COMMIT.
- **Schema**: `warm_memory`, `warm_memory_embeddings` (separate from content); `derived_from_hash`; `MemoryScope::SESSION`.
- **GRAG**: warm memories merged with document chunks before `GragScorer::rescore`.
- **GUI**: `loadConversationMemorySync` + trim after sync (no silent JSON drops on load).
- **Fixes**: `archived_at_ms` / summary timestamps use milliseconds.
- **Tests**: `testMemoryPruning`, `testMemoryPruningIntegration` extended for warm rows.

### 2026-06-29 (V1 — Manual GUI TEST_SUITE pass)

- **Goal**: Close V1 — GUI pass for `TEST_SUITE.md` TC-01–TC-07 with observability panels.
- **Checklist**: `docs/TEST_SUITE_GUI_CHECKLIST.md`.
- **Evidence**: GUI goal `Tell me about Thoth.` — completed, `grag_alpha=1.0`, 1683 tokens, session `session-1782581237114`; Plan Execution + GRAG Diagnostics confirmed working.
- **Regression**: Headless `run_test_suite --dev` 7/7; `ctest -L fast` 3/3.
- **Trace path**: `agent_workspace/decision_trace.jsonl` (not `logs/`).

### 2026-06-29 (C6 Phase 2 — Analysis tooling, tokens, GUI export)

- **Goal**: Complete C6 Phase 2 — turn append-only metrics into analyzable time-series with real token counts and GUI export.
- **Token tracking** (`llm_interface.h/cpp`):
  - `LlmTokenUsage` struct; `resetSessionTokenUsage()`, `sessionTokenUsage()`, `lastCallTokenUsage()`.
  - Ollama: `prompt_eval_count` + `eval_count` from `/api/generate` response.
  - OpenAI: `usage` object from chat completions.
  - Mocks/dev tier: char/4 estimate when provider counts unavailable.
  - `ExecutiveController` resets session at goal start; snapshots `planning_tokens` after each `create_plan`; emits `total_tokens`, `prompt_tokens`, `completion_tokens`, `planning_tokens`, `synthesis_tokens` in `GOAL_COGNITIVE_METRICS`.
- **Plot script**: `scripts/plot_cognitive_metrics.py` — latency breakdown, retrieval quality, token usage PNGs (`logs/plots/`). Requires `matplotlib`.
- **Summarize script**: extended with token field p50/p95.
- **GUI export**: Benchmarks → **Export Cognitive Metrics…** — copy `logs/cognitive_metrics.jsonl` to user path as JSONL or CSV.
- **Test**: `testLlmTokenUsage` in `thoth-unit-tests`.
- **Verify**: `ctest -L fast` 3/3; run a goal and inspect non-zero `total_tokens` in new log rows.

### 2026-06-28 (GitHub — C4 / C5 / C7 release)

- **Thoth** `5277413` — docs sync, submodule pointer, CTest `WORKING_DIRECTORY` fix.
- **Basic_agent** `2397385` — C5 robustness suite, C7 Phase 3 prefetch, C4 CI labels, mock LLM fixes.
- **Fast gate:** `ctest -L fast` → 3/3 (~70s): `test-suite-dev`, `reflection-ab-benchmark`, `robustness-suite`.
- **Repos:** [Thoth](https://github.com/stevemeierotto/Thoth) · [Basic_agent](https://github.com/stevemeierotto/Basic_agent) on `main`.

### 2026-06-27 (GitHub — C1 / C2 / C6 cognitive hardening release)

- **Thoth** `b4b6adf` — docs backlog + submodule pointer.
- **Basic_agent** `379c0c5` — C1 planner hardening, C2 chat RAG (Phases 0–3), C6 per-goal metrics.
- **Repos:** [Thoth](https://github.com/stevemeierotto/Thoth) · [Basic_agent](https://github.com/stevemeierotto/Basic_agent) on `main`.

### 2026-06-27 (C1 — Planning quality / context management)

- **Goal**: Fix planning quality constrained by context assembly — polluted memory injection, tail truncation dropping rules/schema, missing semantic validation, and unfiltered strategy blast. Expert-reviewed priority: prompt assembly → PlanValidator → memory hygiene → scored strategies → instrumentation.
- **Scope**: Goal execution (`LLMPlanner` → `WorkflowEngine`) only; conversation path handled separately by **C2**.

#### Phase 1 — Structured prompt assembly (memory budgets)

- **`prompt_factory.cpp`**: Planner prompts assembled as fixed sections — **Rules → Schema → Goal → optional experience**. Core sections never tail-truncated; experience competes for remainder and is dropped first when over budget.
- **`planner_injection_config.h`**: Formal budgets — rules 4 KB, schema 2 KB, goal 1 KB, plan reuse 1 KB, strategy 512 B, trajectory 512 B; minimum total budget 8 KB.
- **Default template reorder**: `plan_generation.tmpl` and bundled defaults put Rules/Schema before Goal.
- **`PlannerPromptMetrics`**: Byte counts per section for observability.

#### Phase 2 — PlanValidator (reject + limited repair + C++ fallback)

- **`plan_validator.h` / `plan_validator.cpp`**: New validation layer between JSON parse and execution.
  - **Parser** (`PlanParser`): valid JSON syntax.
  - **Validator** (`PlanValidator`): valid Thoth corpus Q&A plan — requires RETRIEVAL + LLM, rejects TOOL steps, enforces RETRIEVAL-first order.
  - **Limited repair**: wire missing LLM `depends_on` to prior RETRIEVAL only (deterministic).
  - **No semantic repair**: does not insert missing steps or convert step types.
  - **`createFallbackPlan()`**: programmatic RETRIEVAL → LLM plan in C++ after one LLM retry (no third LLM call).
- **`llm_planner.cpp`**: Wired LLM → parse → validate → execute path; logs `plan_generated`, `plan_validation_failed`, `plan_fallback_used`, `depends_on_repaired`.

#### Phase 3 — Memory hygiene

- **`goal_text_utils.h`**: `cleanGoalForStorage()` strips nested `[RELEVANT PAST APPROACHES…]` markers; `sanitizePlanOutline()` emits step-type summaries only.
- **`plan_reuse_config.h`**: Similarity floor 0.55; retrieve limit 1 (one similar plan); trajectory similarity floor.
- **`memory.cpp`**: `retrieveSimilarPlans` / `retrieveSimilarTrajectories` apply similarity floor — inject nothing below threshold.
- **`executive_controller.cpp`**: Store clean goals in `past_plans`; sanitized outlines in plan-reuse injection; reflection replan gets failure summary only (no plan-reuse re-injection).

#### Phase 4 — Strategy scoring (not disable)

- **`llm_planner.cpp`**: Top-1 promoted strategy selected by goal-embedding cosine similarity to strategy description + pattern (floor 0.40); replaces `getAllStrategies()` blast.

#### Phase 5 — Planner instrumentation

- **`PLANNER_CONTEXT_ASSEMBLY`**: Logs `rules_bytes`, `schema_bytes`, `goal_bytes`, `strategy_bytes`, `trajectory_bytes`, `plan_reuse_bytes`, `total_bytes`, `experience_dropped`, `strategy_similarity`.
- **`PLANNER_REVISION_CONTEXT`**: Same byte metrics for revision prompts.
- Feeds **C6** cognitive metrics roadmap.

- **Verification**: `cmake --build build/debug --target thoth-control-panel` ✅. Headless `run_test_suite` TC-01–TC-07 ✅ (2026-06-27).
- **Operator note**: Existing `agent_workspace/prompt_templates/plan_generation.tmpl` is only written when missing — delete or update manually for Rules-first template in an existing workspace. Existing `memory.db` may retain polluted goals until new runs overwrite history.

### 2026-06-27 (C2 — Chat RAG quality — Phases 0–3 complete)

End-to-end chat Q&A pipeline: observability → benchmark → retrieval tuning → grounded prompts. User-validated: GUI “Explain GRAG” returns accurate corpus-grounded answer.

#### Phase 0 — Chat RAG observability

- **Goal**: Instrument the conversation (`processQuery`) pipeline only — no retrieval, prompt, truncation, or tool-injection behavior changes. Make chat diagnosable the same way C1 made the planner diagnosable.
- **Log sink**: `logs/chat_rag.jsonl` (append-only, one `CHAT_RAG_CONTEXT` + one `CHAT_RAG_RESPONSE` per turn, linked by `request_id`).

#### Events and metrics

- **`CHAT_RAG_CONTEXT`** (before `llm.query()`): query, ranked documents (file, rank, chunk_id, score, chars, line range), `top_k`, byte counts (retrieved, tool_schema, conversation_history, memory_context, system_prompt), `final_prompt_chars`, truncation before/after + `truncated_section`, derived ratios (`grounding_ratio`, `tool_ratio`, `history_ratio`, `memory_ratio`), `llm_model`, `grounding_mode`.
- **`CHAT_RAG_RESPONSE`** (after LLM): `answer_chars`, `retrieved_doc_count`, `grounding_mode`, `fallback_used` (always false for chat today).

#### Code locations

| Metric / event | Collected in |
|--------------|--------------|
| Retrieval scores + ranked docs | `RAGPipeline::retrieveRelevant` → `GragDiagnostics*` out-param → `command_processor.cpp` `buildDocumentMetrics()` |
| Prompt section byte counts | `PromptFactory::buildChatPrompt()` → `ConversationPromptMetrics` |
| Truncation before/after + section | Section-protected assembly in `buildChatPrompt()` (Phase 3) |
| Final prompt + grounding ratios | `command_processor.cpp` `buildChatRagContextRecord()` |
| JSONL persistence | `ChatRagLogger` (`chat_rag_logger.cpp`) |
| Structured log + decision trace stages | `StructuredLogger` + `DecisionTraceLogger` stages `chat_rag_context` / `chat_rag_response` |

- **Verification**: `cmake --build build/debug --target thoth-control-panel` ✅. Run a GUI chat query and inspect `logs/chat_rag.jsonl`.

#### Phase 1 — Golden chat-RAG retrieval benchmark

- **Tool**: `./build/debug/external/basic_agent/run_chat_rag_benchmark` (requires Ollama for embeddings).
- **Corpus**: `agent_workspace/rag/` — `GRAG.md`, `cognate.md`, `HOWTO.md`, `AGENTS.md`.
- **Golden queries (5):** Explain GRAG → GRAG.md; What is Cognate → cognate.md; How do I use Thoth → HOWTO.md; agent conventions → AGENTS.md; quote GRAG first sentence → GRAG.md.
- **Log sink:** `logs/chat_rag_benchmark.jsonl`.
- **Baseline (2026-06-27):** hit@1 **2/5**, mean nDCG@1 **0.40** — AGENTS.md dominated definitional queries.

#### Phase 2 — Conversational retrieval tuning

- **Filename-aware boosts**, coverage recall supplementation, min-chunk filter (≥80 chars), session-scoped corpus via `setActiveCorpusFiles`, chunk metadata at injection.
- **Benchmark after tuning:** hit@1 **5/5**, mean nDCG@1 **1.00**, mean MRR **1.00**.

#### Phase 3 — Chat pipeline fixes (user validated)

- **Grounding rules** when RAG context present; **Q&A tool gating** via `looksLikeToolIntent()`; **section-protected truncation** (no tail-chop); fixed `grounding_ratio` / `tool_ratio` metrics; `llm_model` config fallback.
- **User validation (2026-06-27):** GUI “Explain GRAG” — accurate grounded answer, no fabricated acronyms.
- **Key files:** `chat_prompt_config.h`, `chat_query_utils.cpp`, `prompt_factory.cpp` (`buildChatPrompt`), `command_processor.cpp`.

### 2026-06-27 (C6 Phase 1 — Per-goal cognitive metrics logging)

- **Goal**: One unified append-only metrics row per goal execution for time-series analysis (feeds C3, C4, C7, benchmarks).
- **Log sink**: `logs/cognitive_metrics.jsonl` — event `GOAL_COGNITIVE_METRICS`.
- **Emit points**: `PLAN_COMPLETED`, `PLAN_FAILED`, `PLAN_ABORTED` in `ExecutiveController`.
- **Fields**: `plan_id`, `session_id`, `goal`, `outcome`, `total_wall_clock_ms`, `planning_time_ms`, `retrieval_time_ms`, `llm_synthesis_time_ms`, `step_count`, `retrieved_chunk_count`, `grag_alpha`, `grag_routing_mode`, `trajectory_score`, `final_success_score`, `reflection_count`, `revisions_count`, `plan_reused`, `total_tokens` (reserved, 0); **C7** adds `synthesis_prompt_chars`, `synthesis_context_truncated`.
- **Files**: `cognitive_metrics.h`, `cognitive_metrics_logger.cpp`; wired in `executive_controller.cpp`, `workflow_engine.cpp` (GRAG diagnostics on RETRIEVAL steps).
- **Verify**: Run a goal; inspect `logs/cognitive_metrics.jsonl` for one `GOAL_COGNITIVE_METRICS` row with non-zero latencies.

### 2026-06-26 (C3 — Reflection A/B measurement)

- **Goal**: Prove reflection replan improves recoverable failures and does not loop on timeout failures. Compare `max_reflections=0` vs `2`; log A/B outcomes append-only.
- **Policy**: `reflectionSkipReason()` suppresses replan when trajectory contains timeout errors (`timeout_failure`), reflection disabled (`max_reflections=0`), or budget exhausted.
- **Config**: `Config::max_reflections` (default 2); env `THOTH_MAX_REFLECTIONS`; wired via `ExecutiveController::set_max_reflections()` in `basic_agent_plugin.cpp`.
- **Harness**: `run_reflection_ab_benchmark` — mock-only (`THOTH_MOCK_LLM`, `THOTH_MOCK_STEP_TIMEOUT` for `timeout-step`).
  - **C3-01**: Recoverable NODE failure → reflection on rescues to COMPLETED (planner_calls 1→2).
  - **C3-02**: Timeout step → both arms FAILED, planner_calls=1 (no reflection replan).
- **Log sink**: `logs/reflection_ab_benchmark.jsonl` — `REFLECTION_AB_CASE` + `REFLECTION_AB_SUMMARY` (`mean_reflection_lift`).
- **C6 integration**: `GOAL_COGNITIVE_METRICS` now includes `max_reflections`, `reflection_skip_reason`.
- **Verify**: `./build/debug/external/basic_agent/run_reflection_ab_benchmark` → **2/2 cases pass**, mean reflection lift **0.5** (2026-06-26).

### 2026-06-26 (C4 Phase 1 — Developer / CI test-suite dev tier)

- **Goal**: Cut `run_test_suite` feedback from ~40 min (Ollama) to seconds for daily development; keep `--full` for production regression.
- **Harness**: `./build/debug/tests/run_test_suite --dev` (default) vs `--full`.
- **Mechanisms**:
  - `test_suite_dev.cpp` — deterministic mock LLM for planner + chat when `THOTH_TEST_SUITE_DEV=1`.
  - `BasicAgentPlugin` — TfIdf embeddings in dev tier; isolated index via `THOTH_TEST_SUITE_INDEX` (`test_suite_corpus/test_suite.rag_index.bin`); auto-save after index; skip re-index when fingerprints unchanged.
  - `workflow_engine` — existing `THOTH_MOCK_LLM` for LLM synthesis steps.
  - Dev TC-03 checks goal-scoped GRAG row (`goal_present`); full tier still requires `alpha > 0` and `direction_magnitude > 0`.
  - Scoring-type filter accepts `rag_hybrid` / `grag_blended_hybrid` (matches `GragScorer` output).
- **Verify**: `run_test_suite --dev` → **7/7 pass in ~10s** (2026-06-26). Second run skips 3/3 corpus files (cached index).

### 2026-06-26 (C4 Phase 2 — CI tier wiring)

- **Goal**: Wire dev vs full regression into CTest and GitHub Actions so PRs get fast feedback and production path runs on schedule.
- **CTest** (`tests/CMakeLists.txt`):
  - `thoth-unit-tests` — label `pr;unit`
  - `test-suite-dev` — label `pr;fast;cognitive`
  - `reflection-ab-benchmark` — label `pr;fast;cognitive`
  - `robustness-suite` — label `pr;fast;cognitive` (added 2026-06-28 with **C5**)
  - `test-suite-full` — `-DTHOTH_TEST_SUITE_FULL=ON`; labels `nightly;ollama`
- **Local:** `ctest -L fast` (~70s, 3 cognitive tests); `ctest -L pr` (full PR gate)
- **GitHub Actions:**
  - PR/push: `ctest -L pr`
  - `cognitive-regression-nightly`: schedule + `workflow_dispatch`; installs Ollama, pulls `qwen2.5:3b` + `nomic-embed-text`, runs `test-suite-full`
  - Checkout uses `submodules: recursive` for `external/basic_agent`
- **Verify**: `ctest -L fast` passes locally after reconfigure.

### 2026-06-26 (C7 Phase 1–2 — Runtime latency)

- **Goal**: Speed up production goal execution (not CI mocks) while preserving GRAG quality and plan correctness.
- **Phase 1 — hot-path optimizations**:
  - `grag_scorer.cpp`: batch TF-IDF chunk embeddings during keyword rescoring (was N serial `embed()` calls).
  - `executive_controller.cpp`: `refresh_goal_state_embeddings_unlocked()` — single `embedBatch` for goal G + current state C at plan start/resume; per-step loop uses state-only `embed()` (avoids re-embedding goal every tick and fixed reflection-ab stack smash under concurrent teardown).
  - `workflow_engine.cpp`: cap retrieved context for LLM synthesis (`synthesis_max_context_chars`, default 8192).
  - `llm_interface.cpp`: send Ollama `options.num_predict`; synthesis steps use `synthesis_num_predict` (default 512).
  - `config.h`: `synthesis_max_context_chars`, `synthesis_num_predict`.
  - C6 fields: `synthesis_prompt_chars`, `synthesis_context_truncated`.
- **Phase 2 — analysis**: `scripts/summarize_cognitive_metrics.py` — p50/p95 for wall-clock, planning, retrieval, synthesis.
- **Verify**: `cmake --build build/debug --target thoth-control-panel`; `ctest -L fast` 3/3 (~70s after **C5** added robustness-suite).
- **CTest fix**: cognitive tests set `WORKING_DIRECTORY` to repo root so relative `agent_workspace/memory.db` resolves correctly.

### 2026-06-26 (C7 Phase 3 — Parallel RETRIEVAL + prefetch)

- **Goal**: Overlap independent RETRIEVAL steps and hide retrieval latency behind the last blocking dependency.
- **Embedding snapshots**: `StepExecutionContext` carries G/C/T vectors at dispatch; `executeRetrieval` passes them to GRAG (thread-safe parallel retrieval).
- **Scheduling**: `max_parallel_retrieval` cap (default 4); independent ready RETRIEVAL steps dispatch concurrently via existing async harness.
- **Prefetch**: when `enable_retrieval_prefetch=true`, PENDING RETRIEVAL steps with exactly one RUNNING dependency start early; results cached and applied on dependency success (`retrieval_prefetch_hits` tracked internally).
- **Lifecycle**: `~ExecutiveController` drains active + prefetch futures before teardown (fixes stack smash in reflection A/B harness).
- **Config**: `max_parallel_retrieval`, `enable_retrieval_prefetch` in `config.json`.
- **Test**: `testParallelRetrieval` — verifies two independent RETRIEVAL steps overlap.
- **Verify**: `ctest -L fast` 3/3; `./tests/thoth-unit-tests` (parallel retrieval test passes).

### 2026-06-28 (C4 & C7 — Latency work closed)

**C4 (Developer & CI)** and **C7 (Runtime / production)** are separate problems with separate solutions — both complete. Cross-reference: `docs/cursor_list.md` § C4, § C7, and comparison table.

| | C4 | C7 |
|--|----|----|
| **Problem** | Slow feedback loop for developers and CI | Slow per-goal execution in production |
| **Approach** | Mocks, tiny corpus, cached index, tiered CTest | Hot-path optimization, synthesis caps, parallel scheduling |
| **Success** | `run_test_suite --dev` ~10s; `ctest -L fast` ~70s | Configurable prefetch + batch embed; metrics via C6 + `summarize_cognitive_metrics.py` |
| **Regression guard** | Nightly Ollama `test-suite-full` | C1/C2/C3/C5 harnesses unchanged |

### 2026-06-28 (C5 — Robustness & failure tests)

- **Goal**: Fast mock-only regression for planning, retrieval, execution, reflection, and lifecycle failure modes. Observable outcomes only (terminal state, bounded retries, structurally valid plans).
- **Harness**: `run_robustness_suite` + `robustness_cases.*` — 10 cases in five categories; append-only `logs/robustness_suite.jsonl` with `pass_reason` per case.
- **Production tweaks** (behavioral, not test-only):
  - Empty index vs empty retrieval: populated index + zero matches → RETRIEVAL succeeds with `retrieval_empty=true`; empty index → FAILED.
  - Synthesis prompt: explicit `No relevant documents found.` when no chunks in context.
  - `THOTH_MOCK_LLM_UNAVAILABLE` + LLM `[Error]` responses fail steps cleanly.
  - Null-safe step `payload` handling in workflow engine.
  - Mock LLM path builds synthesis prompt before short-circuit (no `LLMInterface` required for mocks).
- **CI**: `ctest -L fast` → 3 tests (~70s): dev TEST_SUITE + reflection A/B + robustness suite.
- **Verify**: `./build/debug/external/basic_agent/run_robustness_suite` — 10/10 pass.

### 2026-06-18 (Tier 1 — Audit refresh + automated verification)

- **`audit.md`:** Full refresh — `allow_shell_exec` marked fixed (P1.5); portable `FileHandler` path wording; 2026-06-18 verification run documented.
- **`VERIFIED_BASELINE.md`:** §5 split automated (ctest + grep, 2026-06-18) vs historical manual TC pass (2026-03-12).
- **`AGENTS.md`:** Removed stale `allow_shell_exec` bypass note.
- **`ctest`:** 100% pass (~100s). Manual TC-01–TC-07 deferred to GUI session.

### 2026-06-17 (P2.6 — Benchmark Interpretation)

- **`benchmark_results.md`:** “How to Read These Runs” guide (corpus / case mix / when to cite which delta).
- **`GRAG.md`:** Validation status corpus-qualified; §5 split canonical (100-case, +0.041 / +0.202) vs early sandbox (+0.200); trajectory audit → PARTIAL; self-building → optional future expansion.
- **`MYPAPER.md` (Zenodo):** §4 table and claims aligned to Mar 14 hardened 100-case run; removed stale 563-chunk figures.
- **`AGENTS.md`:** GRAG empirical validation line updated to match canonical benchmark.

### 2026-06-17 (P2.5 — Scratch Doc Cleanup)

- Merged UI scratch notes into `ui_improvements.md` §9–§12; merged `TEST.md` concurrency protocol into `TESTING.md`.
- Deleted: `tmp.md`, `tmp2.md`, `tmp_ui.md`, `tmp_tools.md`, `tmp_science.md`, `qwen_reply.md`, `TEST.md`, `gemini_thoughts.md`.
- Linked `HOWTO.md`, `new_corpus_tests.md`; banner on `old_improvements.md`.

### 2026-06-17 (P2.4 — Narrative Doc Honesty)

- **`README.md`**: Known gaps table; Cognate benchmark caveat; Phase 3 partial.
- **`COGNATE_V2.md`**: Typo fix; mock-environment footnote for 51× reasoning depth.
- **`MYPAPER.md`**: §3.4 Adaptive Graph Learning marked implemented; §4 benchmark table aligned to hardened 100-case run (P2.6).
- **`cognate.md`**: §5 future frontiers aligned with graph learning complete and self-building as optional expansion.
- **Removed** `gemini_thoughts.md` (superseded by `README.md`, `audit.md`, `cursor_list.md`).

### 2026-06-17 (P2.3 — Roadmap Phase Table Alignment)

- **Goal**: Align `improvements.md` Phase 3–5 table and step headers with `completed_improvements_log.md` reality.
- **Phase Order table**: Phases 3–5 marked 🔶 Partial with status legend.
- **Per-phase rollups**: Step-level ✅ / 🔶 / 📋 / 🚫 status tables added for Phases 3, 4, 5.
- **Step headers**: Completed steps point to log entries; pause gates removed from finished work.
- **Close-outs**: Phase 3–5 close-out checklists updated to show what passes vs. deferred today.

### 2026-06-16 (P1.6 — Memory Pruning Integration + GUI Hot-Tier Trim)

- **Goal**: Wire `MemoryPruner` into runtime and stop unbounded growth in SQLite + `chat_sessions.json`.
- **Session scoping**: `Memory::setActiveSessionId()` / `getActiveSessionId()`; `BasicAgentPlugin::setSessionId()` now updates memory before controller/RAG.
- **Auto-prune**: `MemoryPruner` constructed in `Memory` constructor; `maybePruneAfterWrite()` runs after each `addMessage()` (outside `mtx` lock).
- **Shared limits**: `memory_pruning_config.h` — `kMaxHotMessages=50`, `kPruneBatchSize=10` (used by pruner, tests, and GUI).
- **GUI**: `MainFrame` trims each session to hot-tier cap on load and before save (`TrimSessionMessagesForPersistence`).
- **Tests**: `testMemoryPruningIntegration`, `testMemorySessionScoping` (added; not run in this pass).

### 2026-06-15 / 2026-06-16 (P1 Alignment: Security, Trajectory Config, Plan Reuse)

- **Branch:** `cursor/p1-plan-reuse-security-observability` → merged to `main` (2026-06-16).
- **Submodule:** `external/basic_agent` @ `2953068` (plan reuse, shell gating, observability).
- **P1.1 — Trajectory weight activated**:
    - Set `agent_workspace/retrieval_config.json` → `"trajectory": 0.2` (local runtime file; gitignored).
    - Full GRAG benchmark: overall nDCG +0.042; trajectory case type −0.037 (documented in `docs/benchmark_results.md`).
- **P1.5 — `allow_shell_exec` enforcement**:
    - `run_tests` and `code_modify build` gate on `Config::allow_shell_exec`; `ToolRegistry::setConfig()` wired from plugin; unit test `testAllowShellExecGate`.
- **P1.3 — Plan history reuse**:
    - **Goal**: Make `retrieveSimilarPlans()` operational and observable across executive, planner, storage, and GUI.
    - **Implementation**: Cosine similarity over `past_plans` (v2) with tunable thresholds in `plan_reuse_config.h`; injection in `execute_goal` and reflection replan; events `PLAN_REUSE_INJECTION`, `REFLECTION_REPLAN`, `PLAN_HISTORY_STORED`; planner logs `PLANNER_CONTEXT_ASSEMBLY`, `COGNATE_PLAN_PERSISTED`.
    - **Deadlock fix (2026-06-16)**: Observability helpers no longer call `emit_event()` while holding `ExecutiveController::mutex_`; UI events deferred until after unlock.
    - **Documentation**: `docs/plan_reuse_tuning.md` (thresholds, trajectory, observability map); `docs/cursor_list.md` updated for P1.1/P1.3/P1.5.
    - **Tests**: `testPastPlanRetrieval`, `testAllowShellExecGate`; `ctest --output-on-failure` **100% green** (2026-06-16, ~78s).

### 2026-06-13 (Fresh-Start Alignment & Reflection Test Verification)

- **Alignment Backlog (`docs/cursor_list.md`)**:
    - **Goal**: Produce a prioritized, honest gap list between documentation claims and code/test reality after returning to the project.
    - **Implementation Details**: Audited build, unit tests, specs, and roadmap docs; documented open items by priority (P1–P4). Priority 0 (reflection test + green ctest) marked resolved after verification below.
- **Reflection Loop Unit Test (`testReflectionLoop`)**:
    - **Goal**: Automate verification that low trajectory scores trigger a second `IPlanner::create_plan()` call (reflection replan).
    - **Root Cause**: The test used an all-LLM mock plan; `WorkflowEngine::executeLLM()` stub always succeeds → score 1.0 → reflection never fired. This was a **test design issue**, not an ExecutiveController regression.
    - **Fix**: First mock plan uses a failing `StepType::NODE` step; reflection replan uses `LLM`. Added assertions for `Reflection:` goal suffix, two `PLAN_CREATED` events (async-safe polling), terminal `COMPLETED` state, and active plan id `reflection-plan-2`.
    - **Documentation**: Added reflection testing guidance to `docs/TESTING.md`.
    - **Verification**: `ctest --output-on-failure` passes 100% on `main` (2026-06-13).
- **Main Integration (merged to `main`)**:
    - Consolidated pending GUI/benchmark work (`BenchmarkWindow`, `ChatSessionRenderer`, graph/session UI), thesis doc assets, `.editorconfig`, and `basic_agent` pointer update (`f5d5b37` reflection scoring hardening).

### 2026-03-29 (Cognitive Loop Stability & Score Accuracy)

- **Reflection Loop & Success Scoring Hardening**:
    - **Goal**: Resolve the "Always 0 Success Score" bug and prevent infinite reflection loops.
    - **Implementation Details**:
        - **Decoupled Scoring**: Refactored `calculate_trajectory_score` to use explicit success flags instead of relying on transient controller states, ensuring accurate scoring (1.0 for success) at the moment of plan completion.
        - **Empty Plan Guard**: Updated `decide_transition` to handle plans with 0 steps. These are now correctly scored as 0.0 and routed through the reflection loop for repair or marked as `FAILED` if unrecoverable.
        - **Terminal State Enforcement**: Guaranteed that plans with 0 steps cannot transition to `COMPLETED`, ensuring the "Success" status accurately reflects actual work performed.
    - **Verification**: Successfully recompiled; verified logic via code review. **Automated coverage completed 2026-06-13** via strengthened `testReflectionLoop` (see entry above).

### 2026-03-29 (Production-Grade Benchmark UI)

- **Benchmark Execution & Visualization Layer**:
    - **Goal**: Enable real-time execution and visualization of agent benchmarks (`run_grag_benchmark`, `run_cognate_benchmark`) directly from the Thoth UI.
    - **Implementation Details**:
        - **BenchmarkWindow**: Implemented a dedicated `BenchmarkWindow` for real-time output visualization using a multi-line, monospace `wxTextCtrl`.
        - **Thread-Safe Pipe Handling**: Developed an asynchronous pipe reading mechanism using a 100ms `wxTimer` and thread-safe `wxTheApp->CallAfter` with explicit buffer copying to prevent corruption.
        - **Process Lifecycle Management**: Implemented robust process control with `wxProcess`, supporting graceful termination (SIGTERM) and timed escalation to force-kill (SIGKILL) via a non-blocking `wxTimer`.
        - **SQLite Concurrency Guard**: Added a singleton execution constraint in `MainFrame` to prevent database contention and deadlocks during benchmark runs.
        - **Robust Path Resolution**: Implemented `AgentInterface::GetBenchmarkBinaryPath` to dynamically locate benchmark executables across multiple build configurations (Debug/Release) without hardcoding.
        - **Observability Integration**: Integrated anchored regex parsing for real-time metric extraction (`nDCG@5`, `Success Rate`) and a "View Results" handler to open `docs/benchmark_results.md`.
        - **Shutdown Protection**: Added a `MainFrame` close handler to ensure running benchmarks are terminated before application exit, preventing zombie processes.
    - **Verification**: Successfully recompiled; verified process lifecycle, path resolution, and thread-safe output streaming via integrated GUI testing.

### 2026-03-28 (Cognate V2 — Phase 5.2)

- **Thesis Validation: Strategy Adoption & Learning Curve**:
    - **Goal**: Empirically prove that the system learns from its own history and improves planning via strategy adoption.
    - **Implementation Details**:
        - **Persistence Hardening**: Resolved a multi-stage data persistence failure by implementing SQLite schema migrations for `trajectories` and `strategies` tables and fixing NULL-BLOB binding for empty embeddings.
        - **Learning Curve Benchmark**: Enhanced `run_cognate_benchmark` to perform a two-pass evaluation (Cold vs. Warm start).
        - **Empirical Proof**: Successfully demonstrated **Autonomous Strategy Promotion**:
            - Pass 1 (Cold): 0 strategies.
            - Pattern Extraction: 9 trajectories processed.
            - Promotion: 1 high-success pattern promoted to first-class Strategy (Threshold: 80% Success / 3 Runs).
            - Pass 2 (Warm): Strategy Library active and utilized by the planner.
    - **Verification**: Benchmark tool output confirmed the promotion of `RETRIEVAL -> TOOL:llm_reasoning` pattern after 3 successful runs.

### 2026-03-28 (Cognate V2 — Phase 5.1)

- **Thesis Validation: Standard vs. Scientific Benchmarks**:
    - **Goal**: Generate empirical data for comparative analysis of different cognitive execution strategies.
    - **Implementation Details**:
        - **Benchmark Tooling**: Developed a dedicated `run_cognate_benchmark` executable to automate multi-task evaluation.
        - **Comparative Metrics**: Implemented tracking for Success Rate, Average Steps, and Reasoning Depth (iterations).
        - **Infrastructure Integration**: Wired the benchmark tool to use the same `ExecutiveController` and `ScientificExecutionMode` logic as the main application.
    - **Verification**: Ran a comparative benchmark across 10 complex engineering tasks; confirmed that Scientific mode correctly utilizes iterative convergence (avg depth 198.3 steps per goal) compared to linear Standard mode. Results recorded in `docs/benchmark_results.md`.

### 2026-03-28 (Cognate V2 — Phase 4.2)

- **Observability Layer: Cognitive Loop Graph Visualization**:
    - **Goal**: Provide a real-time visual representation of the Cognate architecture's state transitions for thesis demonstration.
    - **Implementation Details**:
        - **Custom Drawing**: Implemented a directed graph rendering engine in `GraphPanel` using `wxGraphicsContext`.
        - **Architecture Mapping**: Defined 6 core cognitive nodes: Goal Ingestion, LLM Planner, Executive Controller, Scientific Mode, Strategy Engine, and Hybrid Memory.
        - **Dynamic Highlighting**: Wired `ControllerState` events to active node highlighting (e.g., the "Scientific Mode" node glows green when the scientific execution strategy is active).
        - **Sub-stage Mapping**: Mapped `reasoning_stage` metadata (Hypothesis, Feasibility, etc.) to high-level architecture nodes to ensure consistent visual feedback.
    - **Verification**: Successfully recompiled; verified state-to-node mapping via UI code review and event bus integration checks.

### 2026-03-28 (Cognate V2 — Phase 4.1)

- **Observability Layer: Trajectory Viewer 'Plan vs. Reality' Wiring**:
    - **Goal**: Visualize the evolution of goal execution by comparing initial plans with actual steps taken.
    - **Implementation Details**:
        - **ID Alignment**: Corrected a discrepancy in `ExecutiveController` where `episode_id` and `trajectory_id` prefixes were mismatched, ensuring SQLite records are correctly linked in the UI.
        - **UI Enhancement**: Upgraded `TrajectoryViewer` to render a hierarchical "Initial Plan vs. Reality" view using `wxTreeListCtrl`.
        - **Plan Visualization**: Added an "Initial Plan" child node under each trajectory to show the LLM's original reasoning before execution.
        - **Reality Visualization**: Added a "Reality (Steps Taken)" node to show the actual actions, results, and revisions performed by the controller.
    - **Verification**: Successfully recompiled both the core library and the GUI; verified data alignment via unit tests and UI code review.

### 2026-03-28 (Cognate V2 — Phase 3.2)

- **Experience-Guided Planning: Strategy Injection & Verbose Logging**:
    - **Goal**: Prove causation by explicitly injecting learned strategies into the LLM planning prompt and logging the process.
    - **Implementation Details**:
        - **Contextual Injection**: Updated `LLMPlanner::create_plan` to retrieve high-success strategies from memory and inject them as a formatted `[LEARNED STRATEGIES]` block into the prompt.
        - **Trajectory Integration**: Added simultaneous injection of relevant past trajectories to provide the LLM with full execution context.
        - **Verbose Causation Logging**: Implemented explicit `STRATEGY_INJECTION` and `TRAJECTORY_INJECTION` events in `decision_trace.jsonl` to track exactly what knowledge was provided to the planner for every goal.
    - **Verification**: Added `testStrategyInjection` to `thoth-unit-tests`; verified that the strategy retrieval and injection logic is correctly reached during plan generation.

### 2026-03-28 (Cognate V2 — Phase 3.1)

- **Strategy Engine 2.0: Semantic Promotion & Library**:
    - **Goal**: Implement rigorous strategy extraction from execution trajectories to enable experience-guided learning.
    - **Implementation Details**:
        - **Semantic Pattern Extraction**: Upgraded `StrategyEngine` to detect sequences of Tool calls (e.g., `TOOL:project_analyze`) and Step Types (e.g., `RETRIEVAL`), moving beyond raw enums.
        - **Promotion Logic**: Enforced the thesis-critical **80% success / 3-run threshold** for pattern promotion to first-class `Strategies`.
        - **Strategy Library**: Expanded the `Strategy` struct with `reasoning_stage` and metadata for better planning context.
        - **Deterministic Stability**: Implemented hash-based deterministic Strategy IDs to prevent duplication across analysis cycles.
    - **Verification**: Added `testStrategyPromotion` to `thoth-unit-tests`; verified that 3 successful simulated runs correctly trigger a strategy promotion with the expected semantic pattern string.

### 2026-03-28 (Cognate V2 — Phase 2.2)

- **Scientific Reasoning Engine: Convergence & Infinite Loop Protection**:
    - **Goal**: Define rigorous exit conditions for iterative reasoning loops to ensure genuine convergence.
    - **Implementation Details**:
        - **Convergence Logic**: Implemented `is_converged()` in `ScientificExecutionMode` based on numerical stability of confidence scores ($\Delta < 0.05$) over an observation window of 2 iterations.
        - **Confidence History**: Added `confidence_history` to `ProblemState` to track progress across iterations and restarts.
        - **Safety Gates**: Enforced a `MAX_ITERATIONS = 5` fallback to prevent infinite loops.
        - **Loop Exit Events**: Standardized logging and events for convergence detection vs. max iteration limits.
    - **Verification**: Added `testScientificConvergence` to `thoth-unit-tests`; verified the system correctly identifies convergence and transitions back to `IDLE` after 4 iterations.

### 2026-03-28 (Cognate V2 — Phase 2.1)

- **Scientific Reasoning Engine: 4-Stage Iterative Loop**:
    - **Goal**: Implement a real iterative reasoning loop that converges on solutions.
    - **Implementation Details**:
        - **Stage Logic**: Replaced the prototype stub in `ScientificExecutionMode` with a structured 4-stage loop: Hypothesis Generation, Constraint Extraction, Feasibility Evaluation, and Final Selection.
        - **State Integration**: Wired the `ProblemState` into every stage, ensuring hypotheses and constraints are tracked across iterations.
        - **Observability**: Standardized `ControllerEvent` emission for each reasoning stage, providing real-time telemetry for the UI.
    - **Verification**: Added `testScientificLoopStages` to `thoth-unit-tests`; confirmed all 4 stages are visited and the `ProblemState` is correctly updated.

### 2026-03-28 (Cognate V2 — Phase 1.3)

- **Unified Event Bus for UI Observability**:
    - **Goal**: Lock down a structured event schema to ensure consistent observability across reasoning stages.
    - **Implementation Details**:
        - **Schema Standardization**: Standardized the `ControllerEvent` metadata payload to include mandatory fields: `reasoning_stage`, `confidence_score`, `success`, and `iteration_count`.
        - **Automatic Enrichment**: Updated `ExecutiveController::emit_event` to automatically populate these fields based on `EventType` and internal state (e.g., `reflection_count_`).
        - **Terminal Handling**: Guaranteed `success: false` for `PLAN_FAILED` and `PLAN_ABORTED` events.
    - **Verification**: Added `testEventSchemaStandardization` to `thoth-unit-tests`; verified all Cognate V2 fields are present in emitted events.

### 2026-03-28 (Cognate V2 — Phase 1.2)

- **ExecutiveController Mode Switching & Persistence**:
    - **Goal**: Enable dynamic mode transitions with high-integrity state preservation.
    - **Implementation Details**:
        - **Controller Updates**: Added `current_problem_state_` to `ExecutiveController`.
        - **Mode Switching**: Enhanced `set_execution_mode` to atomically persist both the active `Plan` and `ProblemState` during transitions.
        - **Persistence Methods**: Implemented `update_problem_state` and `persist_problem_state_unlocked` in the controller.
        - **Event Emission**: Guaranteed `MODE_SWITCHED` event emission upon successful transition.
    - **Verification**: Added `testModeSwitchPersistence` to `thoth-unit-tests`; confirmed `MODE_SWITCHED` event and SQLite state verification.

### 2026-03-28 (Cognate V2 — Phase 1.1)

- **Formalized ProblemState & SQLite Persistence**:
    - **Goal**: Implement high-fidelity state persistence for scientific reasoning loops.
    - **Implementation Details**:
        - **Data Model**: Created `ProblemState` struct in `problem_state.h` with support for hypotheses, constraints, and iteration tracking.
        - **SQLite Integration**: Added `problem_states` table to the database schema.
        - **Repository Methods**: Implemented `saveProblemState`, `loadProblemState`, and `getLatestProblemState` in `SQLiteMemoryRepository`.
        - **Memory Wrapper**: Exposed the new persistence methods through the `Memory` class.
    - **Verification**: Added `testProblemStatePersistence` to `thoth-unit-tests`; verified successful save/load cycles.

### 2026-03-26 (Ollama Stability & GTK Layout Hardening)

- **Ollama CURL Initialization & Thread-Safety**:
    - **Goal**: Resolve the `Assistant: [Error] Ollama failed: CURL error: Failed initialization` error occurring during concurrent chat/plan execution.
    - **Implementation Details**:
        - **Global Lifecycle**: Added `curl_global_init(CURL_GLOBAL_ALL)` and `curl_global_cleanup()` to `src/main.cpp` ensuring the library is initialized at the process entry point.
        - **Recursive Locking**: Upgraded `LLMInterface` to use a `std::recursive_mutex` to protect the shared CURL handle, preventing deadlocks when internal methods (like `detectOllamaModel`) are called from within an active query lock.
        - **Build Linkage**: Updated the root `CMakeLists.txt` to explicitly find and link `CURL` to the main executable.
- **GTK Layout Assertion Resolution**:
    - **Goal**: Eliminate persistent `Gtk-CRITICAL` assertions regarding negative size allocations and scrollbar distribution failures.
    - **Implementation Details**:
        - **Vertical Decoupling**: Wrapped the right-side observability panels in a `wxScrolledWindow`. This prevents the "stacked list" effect from forcing negative heights when the main window is resized.
        - **Theme-Aware Sizing**: Replaced hardcoded button dimensions (e.g., `24x24`) with `wxDefaultSize` across the Sidebar, Goal Banner, and Chat Bubbles to satisfy GTK's theme-specific padding (extents) requirements.
        - **Padding Optimization**: Reduced `BUBBLE_PADDING` and added `SetMinSize` guards to all `wxDataViewCtrl` and `wxTreeListCtrl` widgets to ensure they never collapse to zero.
        - **AUI Headroom**: Increased the `MinSize` of the bottom notebook to `250px` and the right sidebar to `350px`.
- **Trajectory Viewer Implementation & Data Fallback**:
    - **Goal**: Wire up the `TrajectoryViewer` to display real execution history from SQLite.
    - **Implementation Details**:
        - **Unified Tree-List**: Swapped stacked list controls for a single `wxTreeListCtrl`, providing a hierarchical view (Goal -> Steps) with persistent column labels.
        - **Full Stack Retrieval**: Implemented `getAllEpisodeSteps()` through the entire system (`MemoryRepository` -> `BasicAgentPlugin` -> `AgentInterface`).
        - **Inferred Trajectories**: Added a fallback aggregation layer that groups raw `episode_steps` by ID if the consolidated `trajectories` table is empty, ensuring the UI always reflects recent activity.

### 2026-03-22 (Research Console & Stability Hardening)

- **Cognitive Spine Thread-Safety (ExecutiveController)**:
    - **Goal**: Resolve the "Thread-Safety Vacuum" causing UI crashes and state corruption due to concurrent access between UI (main) and Agent (background) threads.
    - **Issues Resolved**:
        - Unprotected public methods: `transition_to`, `update_goal_embedding`, `update_trajectory_embedding`, `dispatch_step`, `set_workflow_engine`.
        - Reference leakage: `get_current_plan()` returning `const Plan&`.
        - Inconsistent internal locking leading to race conditions and deadlocks.
        - Data races on GRAG embeddings (Goal, Current, Trajectory).
    - **Implementation Details**:
        - **Mutex Protection**: Applied `std::lock_guard<std::mutex>` across all shared state access in `ExecutiveController`.
        - **Snapshot Inspection**: Switched `get_current_plan()` to return by value (snapshot), ensuring UI thread memory safety.
        - **Deadlock-Free Internal Transitions**: Implemented `_unlocked` method variants for safe internal calls within the state machine's lock.
        - **Execution Reliability**: Fixed `execute_goal` execution loop start logic and joined stale threads to prevent leaks.
    - **Verification**: Successfully recompiled and passed full suite of unit tests including parallel tool batching and reflection cycles.
- **UI Restoration & Observability Hardening**:
    - **Menu Bar Implementation**: Restored the missing `SetupMenuBar` and implemented full handlers for File, Agent, Tools, Benchmarks, View, and Help menus.
    - **Plan Execution Panel**: Implemented a new vertical `PlanExecutionPanel` that exposes the real-time state of the `ExecutiveController`, showing current goal and ordered step statuses.
    - **GRAG Diagnostic Fixes**: Resolved a bug where GRAG diagnostics showed zero values by ensuring the `Config` constructor properly initializes retrieval weights ($w_q, w_d, w_t, w_k, w_g$) and the `grag_directional` toggle.
    - **Executive Control Integration**: Exposed `pause`, `resume`, `abort`, and `executeGoal` methods in `AgentInterface`, allowing direct control of the agent's cognitive loop from the UI.
    - **Contextual Explanation Buttons**: Replaced the generic `?` button with explicit `Explain Retrieval` and `Explain Plan` actions, and added a `Revise` button to the goal banner for dynamic goal steering.
- **RAG Indexer Stability & Parallelization**:
    - **Goal**: Decouple the indexing lifecycle from the cognitive loop to prevent system-wide deadlocks and multi-minute blocking during large file batch processing.
    - **Implementation Details**:
        - **Asynchronous Architecture**: Refactored `IndexManager` with a background worker thread and a thread-safe task queue. Added `indexFileAsync` and `indexProjectAsync`.
        - **CURL Handle Pool**: Upgraded `EmbeddingEngine` from a single handle to a thread-safe pool, allowing concurrent embeddings for indexing and real-time chat queries.
        - **Stateless Retrieval**: Hardened `RAGPipeline::retrieveRelevant` to support passing embeddings as arguments, removing reliance on shared internal state and reducing mutex contention.
        - **Granular Locking**: Optimized `IndexManager` to release `chunksMutex` during heavy embedding phases, allowing concurrent reads during long-running write operations.
    - **Verification**: Confirmed fix with successful project-wide compilation and verified background worker lifecycle.
- **Benchmark Integrity & Reliability**:
    - **Mock Data Isolation**: Modified `testBenchmarkReporter` in `tests/unit_tests.cpp` to prevent unit tests from polluting the production `benchmark_results.md` with hardcoded mock data.
    - **Benchmark Sampling**: Added a `--sample` flag to the `run_grag_benchmark` tool, allowing for rapid verification of real retrieval performance on a subset of test cases without environment timeouts.
    - **Log Sanitization**: Cleaned `docs/benchmark_results.md` to remove duplicate mock entries, restoring the document as a reliable source of actual research performance history.
- **Research-Oriented UI Transformation**:
...
    - **3-Column Main Workspace**: Implemented a professional layout with Knowledge Base (Left), Central Chat/Plan (Center), and Diagnostics/Strategy (Right) using `wxAuiManager`.
    - **System State Tray**: Added a tabbed bottom notebook providing dedicated views for RAG context management, Trajectory history, Experiment Lab, Knowledge Graph statistics, and live decision logs.
    - **Structured Tool Rendering**: Upgraded chat bubbles to detect and render JSON-encoded tool results (e.g., diff viewers for `code_modify`, web previews for `web_scrape`).
- **Cognitive Reliability & Persistence**:
    - **Stateful Resumption**: Fixed `checkResumablePlan` to fully restore the agent's internal state (goal, plan, embeddings) upon session activation, ensuring continuity across window reloads.
    - **Session-Aware Event Routing**: Added `session_id` to the `ControllerEvent` system, allowing the UI to accurately save goals and plans to background sessions even if the user switches chats.
    - **Cold-Start GRAG**: Optimized `GragScorer` to activate directional retrieval from Step 1 by treating empty current state as a zero vector, preventing zeroed diagnostics at plan initiation.
- **Stability & Memory Hardening**:
    - **Thread Pool Architecture**: Refactored `AgentInterface` from a leaking thread-per-operation model to a single persistent worker thread with a task queue.
    - **Micro-Batch Indexing**: Implemented batching (size 10) in `IndexManager` and `EmbeddingEngine` to eliminate memory spikes and `std::bad_alloc` crashes during large file indexing.
    - **SQLite Safety**: Deployed a `safe_col_text` helper across all repository methods to prevent crashes when encountering NULL database columns.
    - **UI Resource Guards**: Implemented character limits for chat messages (50k), generic tool outputs (5k), and log tailing (10k) to ensure smooth rendering of massive agent outputs.
    - **Window Lifetime Checks**: Added validity guards in all asynchronous `CallAfter` callbacks to prevent segmentation faults during application shutdown.

### 2026-03-16 (UI & Diagnostics Enhancements)

- **Menu Bar Integration**: Added the requested File/Agent/Tools/Benchmarks/View/Help menu structure with placeholder bindings so the UI now exposes the agent controls, benchmarking hooks, and visibility toggles while keeping the core window responsive. citeturn0exec0
- **Goal Continuity**: Persisted the active goal inside each `Thoth::ChatSession`, saved/loaded it with `chat_sessions.json`, and refreshed the banner on activation or after plan events so the goal stays visible across restarts. citeturn0exec1
- **GRAG Diagnostics Fixes**: Normalized the diagnostics payload (nested `result` & `diagnostics` blobs), restored the retrieved-chunks score column, and added percentage formatting so alpha/magnitude/scoring type reflect the telemetry instead of defaulting to zero. citeturn0exec1

### 2026-03-12 (Adaptive Graph Learning Final Implementation)

- **Adaptive Graph Learning (Phase 5.6)**:
    - **Stable Chunk Identity**: Implemented content-based hashing (SHA256) for chunks to ensure graph associations survive project re-indexing.
    - **Schema Hardening**: Updated SQLite schema to track edge weights, success/failure counts, and last-used timestamps for dormancy detection.
    - **Causal Linking**: Modified `ExecutiveController` to capture the "Active Set" of chunks (Top 5 with 0.8 relative threshold) and link them across successful step transitions.
    - **Logistic Reinforcement**: Deployed `GraphRefiner` with a logistic learning rule ($W_{new} = W + lr \times (1-W)$) to saturate weights and prevent numerical explosion.
    - **One-Hop Activation**: Upgraded `GragScorer` to perform 1-hop neighbor activation from high-confidence query hits ($Q_{sim} \geq 0.7$), ensuring experience-guided retrieval without noise.
    - **Multi-Tier Forgetting**: Integrated a global decay mechanism ($0.995$) with an additional dormancy penalty ($0.97$) for edges unused for > 30 days.
    - **Observability**: Expanded retrieval diagnostics to include `graph_source_node`, `graph_activations`, and contribution metrics in `grag_benchmark.jsonl`.

## Summary

- **Tool Confirmation System**: Implemented `requires_confirmation()` across the `ITool` interface and all 9 tools, enforcing user approval for risky operations.
- **Global Security Enforcement**: Integrated `ConstraintChecker` into the main chat loop (`CommandProcessor`), ensuring security policies apply to both standard interaction and goal execution.
- Completed project rebranding (Thoth), SQLite episodic memory migration, and full Tool System integration.
- Implemented Dynamic Planning (Phase 9) with LLM-generated, variable-length plans and robust JSON parsing.
- Verified Resume Completeness (Phase 10), ensuring the system can fully reconstruct and resume plans from trace logs.
- Implemented Dynamic Plan Revision (Phase 11), allowing the LLM to automatically repair plans after step failures.
- Implemented Extended Agent & Cognitive Spine (Phase 12), re-enabling all tools and verifying the full goal-execution lifecycle with an end-to-end integration test.
- Implemented Self-Building Capability (Phase 3 of improvements.md) — Prototype harness established for analyzing and testing the codebase; unified diff application (`apply_diff`) is currently a non-functional stub.
- **Semantic Transition & Retrieval Hardening (embedding_fix.md Phase 1-4)**:
    - **Dense Semantic Embeddings**: Integrated Ollama REST API (`/api/embed`) with `nomic-embed-text` support.
    - **Metric Hardening**: Implemented nDCG@5 and Directional Rank Lift to detect subtle ranking improvements.
    - **Hybrid Reranking**: Developed a dynamic blend of semantic and keyword-based TF-IDF scoring.
    - **Engineering Safeguards**: Added model versioning, automated re-indexing on metadata mismatch, and robust batching.
    - **Weight Optimization**: Conducted an automated weight sweep to identify optimal hybrid parameters ($w_q=0.4, w_d=0.4, w_k=0.3$).
- **Retrieval Stabilization & Observability (embedding_fix.md Phase 5-7)**:
    - **Config Locking**: Moved optimized weights ($w_q=0.4, w_d=0.4, w_k=0.3$) into `agent_workspace/retrieval_config.json`.
    - **Auto-Archiving**: Updated `BenchmarkReporter` to automatically record all benchmark runs in `docs/benchmark_results.md`.
    - **Glass Box Retrieval**: Refactored `GragScorer` to provide a detailed `ScoreBreakdown` (Query, Goal, Trajectory, Keyword, Graph) for every retrieved chunk.
    - **Reranking Optimization**: Expanded the initial retrieval pool to 40 candidates to give GRAG more semantic headroom before narrowing to the final top 5.
- Implemented Advanced Tool Orchestration (Phase 5 of improvements.md), including parallel step execution (Tool Batching), a production-grade `web_scrape` tool, and expanded Gmail integration (`gmail_read_messages`).
- Implemented Reasoning & Self-Correction (Phase 6 of improvements.md), featuring a `self_correct` reasoning tool, a global `ConstraintChecker` for security policy, and an autonomous `Reflection Loop` for goal recovery.
- **UI Enhancement**: Added individual "X" delete buttons to the RAG file slots, allowing for granular context management.
- Tool System now supports runtime dispatch, automatic prompt injection, and structured trace logging.
- Implemented and stabilized ExecutiveController with thread-safety and GRAG wiring.
- **GRAG Cognitive Retrieval (Phases 1–8)**:
    - **Foundation (Phases 1–2)**: Implemented `GragScorer`, adaptive $\alpha$ blending, and directional $G - C$ embeddings.
    - **Intelligent Routing (Phase 3)**: Added mode-based routing (PLAN_AWARE, GOAL_ONLY, CONVERSATIONAL) with automated fallback.
    - **Structured State (Phase 4)**: Migrated to JSON-structured state embeddings (v2) to prevent semantic drift.
    - **Experience Reuse (Phase 5)**: Implemented Plan History Reuse, automatically injecting past successful approaches into the planner.
    - **Index Lifecycle (Phase 6)**: Added selective re-indexing with file fingerprinting and metadata tracking.
    - **Empirical Evaluation (Phase 7)**: Integrated structured benchmarking (`grag_benchmark.jsonl`) and high-level success metrics.
    - **Relational Context (Phase 8)**: Deployed a prototype Graph Memory layer in SQLite for causal link retrieval.
- **Memory Stability (Phase 4 of roadmap)**:
    - **Fact Store**: Implemented a persistent `FactStore` for structured world knowledge and a corresponding `store_fact` tool.
    - **Vector Abstraction**: Refactored RAG to use `IVectorStore`, enabling future migration to professional vector databases.
- **Advanced Reasoning (Phase 5 of roadmap)**:
    - **Scientific Mode**: Implemented `ScientificExecutionMode` using the Strategy Pattern for iterative hypothesis testing.
    - **Strategy Engine**: Added `StrategyEngine` to process execution trajectories and optimize future plan generation.
- Hardening, security, CI, and RAG index lifecycle improvements completed in previous sessions.

## Completed Work

### 2026-03-10

- **Strict Sandbox Boundaries**:
    - **Enforcement**: Modified `IndexManager` to hard-reject and log any indexing requests outside `/home/steve/Thoth/agent_workspace/`.
    - **Bootstrap Security**: Updated `CommandProcessor::ensureInitialized` to bootstrap RAG exclusively from the `agent_workspace/rag/` sandbox rather than the project root.
    - **Benchmark Hardening**: Rewrote `run_grag_benchmark.cpp` to use a strictly sandboxed corpus and added path-guard logic to prevent accidental directory traversal.
- **External Corpus Integration (Research Papers)**:
    - **New Corpus**: Replaced project documentation with 5 high-density AI research papers (ReAct, RAG, MemGPT, Generative Agents, CoT) as the primary retrieval benchmark.
    - **Test Suite Expansion**: Designed 30 new test cases (10 per type) specifically targeting semantic overlaps between papers (e.g., "memory" in MemGPT vs. Generative Agents).
    - **Retrieval Hardening**: 
        - **Chunk Optimization**: Reduced target chunk size to 2048 chars and added a hard 8000-char truncation guard to prevent Ollama context window errors.
        - **Reliability**: Forced `127.0.0.1` for Ollama connections to prevent IPv6 resolution failures.
        - **Cold Start Fix**: Updated `IndexManager` to train the local TF-IDF engine's vocabulary before chunking, ensuring valid keyword scores even when semantic embeddings are delayed.
    - **Empirical Validation**:
        - **Goal Disambiguation**: Confirmed a massive **+0.216 nDCG@5 lift**, proving that GRAG's directional steering is highly effective at selecting the correct paper when terminology overlaps.
        - **Overall Mean RR**: Improved from 0.587 to 0.647.
        - **Stability**: Verified that the sandbox boundary is strictly enforced across the research corpus.

### 2026-03-09

- **Reranking Optimization (Phase 7)**:
    - **Candidate Expansion**: Modified `RAGPipeline` to fetch 40 candidates in the initial recall phase, allowing more semantic signal to be processed during rescoring.
    - **Verification**: Verified the multi-stage pipeline via the benchmark runner.
- **Observability & Explainable AI (Phase 6)**:
    - **Explicit Score Data Structure**: Introduced `ScoreBreakdown` to capture all retrieval signals independently.
    - **Transparency Hardening**: Modified the rescoring loop to preserve and sort individual signal scores, enabling "Why this document?" explainability.
- **Retrieval Stabilization & Observability (Phase 5)**:
    - **Empirical Weight Locking**: Optimized weights from Phase 4 are now stored in a persistent JSON config and synchronized between the global `Config` and `RAGPipeline`.
    - **Automated Performance Tracking**: Every benchmark run now appends a human-readable Markdown summary to `docs/benchmark_results.md`, including Precision, MRR, and nDCG metrics.

### 2026-03-05

- **GRAG Phase 3–8 Implementation**:
    - **Multi-Index Routing (Phase 3)**: Implemented `GragRoutingMode` logic to automatically switch between PLAN_AWARE, GOAL_ONLY, and CONVERSATIONAL retrieval based on agent state.
    - **Structured State (Phase 4)**: Enforced a strict JSON schema for state embeddings to maintain semantic focus. Completed SQLite migration to Version 2.
    - **Plan History Reuse (Phase 5)**: Wired `MemoryRepository` to retrieve past successful plans by directional similarity, reducing planning latency for recurring tasks.
    - **Index Lifecycle (Phase 6)**: Implemented `shouldReindexFile` fingerprinting to skip re-indexing unmodified source files during startup.
    - **Metrics Harness (Phase 7)**: Enabled per-retrieval diagnostic logging and established the initial performance baseline.
    - **Graph Memory Prototype (Phase 8)**: Built the SQLite `graph_nodes` and `graph_edges` schema with hybrid scoring integration in `GragScorer`.
- **Advanced Core Architecture (Roadmap Phase 4-5)**:
    - **Memory Stability (Phase 4)**: Deployed the `FactStore` and `IVectorStore` abstractions, decoupling RAG from the file-based prototype index.
    - **Scientific Reasoning (Phase 5)**: Implemented `ScientificExecutionMode` and `ProblemState` for iterative hypothesis evaluation. Verified convergence halting in unit tests.

### 2026-03-08
...
