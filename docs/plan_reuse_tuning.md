# Plan Reuse & Trajectory Tuning Reference

**Purpose:** Single reference for tunable thresholds affecting plan history reuse, trajectory-aware GRAG, and reflection replan. Constants live in `external/basic_agent/include/plan_reuse_config.h`.

**Last updated:** 2026-07-18 (G1d **DROP** — `w_t=0.0`; **G1e** Phase 3 ✅ KEEP@−0.05 candidate; Phase 4 pending)

---

## Plan history reuse (`past_plans` table)

| Constant | Default | Location | Effect |
|----------|---------|----------|--------|
| `PlanReuse::kMinSuccessScore` | **0.6** | `memory.cpp` → `retrieveSimilarPlans()` | Plans below this `success_score` are never injected |
| `PlanReuse::kSuccessBoostThreshold` | **0.8** | `memory.cpp` | Plans at/above this get a ranking boost |
| `PlanReuse::kSuccessBoost` | **0.1** | `memory.cpp` | Added to cosine similarity for high-success plans |
| `PlanReuse::kDefaultRetrieveLimit` | **3** | `ExecutiveController::execute_goal` | Top-K similar plans retrieved |
| `PlanReuse::kOutlineMaxChars` | **500** | `build_plan_reuse_context()` | Max outline JSON chars in planner injection |

**Tuning notes:**
- Raise `kMinSuccessScore` to **0.8** if low-quality plans leak into prompts.
- Lower `kSuccessBoost` if high-success plans dominate unrelated goals.
- Increase `kDefaultRetrieveLimit` for richer context (prompt size cost).

**Storage:** `SQLiteMemoryRepository::storePastPlan()` → table `past_plans` (embedding v2).  
**Not used for reuse:** `cognate_plans` (active snapshots / LLMPlanner `save_plan`).

---

## Trajectory-aware GRAG (`w_t`)

| Constant / config | Default | Location | Effect |
|-------------------|---------|----------|--------|
| `TrajectoryReuse::kMinEpisodeStepsForEmbedding` | **3** | `trajectory_builder.cpp` | Fewer steps → zero T vector |
| `TrajectoryReuse::kZeroVectorEpsilon` | **1e-6** | `executive_controller.cpp` | Forces `wt=0` when T is effectively zero |
| `agent_workspace/retrieval_config.json` → `trajectory` | **0.0** | Loaded at runtime | Global GRAG trajectory weight when T is active |
| `Config::wt` default | 0.0 | `config.h` | Overridden by `retrieval_config.json` when present |

**G1d close-out (2026-07-18):** Bucket ablation on `TRAJECTORY_DISAMBIGUATES` → terminal **DROP**. Production `trajectory` set to **0.0**. Evidence: Phase B `run-1784399242046` + TUNE micro-runs `0.05` / `0.1` — see [`completed_improvements_log.md`](completed_improvements_log.md) and [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md).

**Does not foreclose research:** G1d only tested the current trajectory term at positive weights. Mean Δ(B−A): `0.20` worse, `0.10` worse, `0.05` closer to baseline.

**G1e polarity fork:** [`G1E_POLARITY_PROTOCOL.md`](G1E_POLARITY_PROTOCOL.md) **v1.1** — Phase 3 ✅. KEEP candidate `w_t=−0.05` (`run-1784408754379`); `−0.10`/`−0.20` failed dual KEEP. Production remains `0.0` until Phase 4. No-scalar-rescue does not apply.

**Tuning notes:**
- Do not re-enable non-zero `trajectory` without a KEEP logged under a locked eval (G1e or later).
- T plumbing remains; weight is zero so the term does not contribute when config is loaded.
- Empty-T executive zeroing remains as defense in depth.

---

## Reflection replan

| Constant | Default | Location | Effect |
|----------|---------|----------|--------|
| `Reflection::kScoreThreshold` | **0.6** | `executive_controller.cpp` | Below this trajectory score → reflection replan |
| `Reflection::kMaxReflections` | **2** | `executive_controller.h` (`MAX_REFLECTIONS`) | Max reflection cycles per goal |

Reflection replan now also calls `retrieveSimilarPlans()` (same thresholds as above).

---

## Observability — where to look

### Structured logs (`agent_workspace/app_log.jsonl`)

| Event | Component | When |
|-------|-----------|------|
| `PLAN_REUSE_INJECTION` | `executive` | Past plans injected (`source`: `execute_goal` or `reflection_replan`) |
| `REFLECTION_REPLAN` | `executive` | Low score triggers replan |
| `PLAN_HISTORY_STORED` | `executive` | Plan written to `past_plans` + `cognate_plans` + trajectory |
| `PLANNER_CONTEXT_ASSEMBLY` | `planner` | LLMPlanner summarizes all context sources |
| `TRAJECTORY_INJECTION` | `planner` | Similar trajectories injected |
| `STRATEGY_INJECTION` | `planner` | Strategies injected |
| `COGNATE_PLAN_PERSISTED` | `planner` | Snapshot saved to `cognate_plans` |

### Decision trace (`agent_workspace/decision_trace.jsonl`)

Same events via `ControllerEvent` types:
- `PLAN_REUSE_INJECTION`
- `REFLECTION_REPLAN`
- `PLAN_HISTORY_STORED`

Metadata includes thresholds used at runtime (e.g. `min_success_score`, `reflection_threshold`).

### GUI pipeline

```
ExecutiveController → BasicAgentPlugin::onEvent → AgentInterface → MainFrame
```

- **AgentInterface:** `[AgentInterface] bridge event ...` for the three plan-reuse event types
- **MainFrame:** status bar + stderr for `PLAN_REUSE_INJECTION`, `REFLECTION_REPLAN`, `PLAN_HISTORY_STORED`

---

## Changing constants

1. Edit values in `plan_reuse_config.h` (plan reuse + reflection + trajectory builder).
2. For GRAG `wt`, edit `agent_workspace/retrieval_config.json` (not `memory.db`).
3. Rebuild: `cmake --build --preset build-debug`
4. Re-run benchmarks if changing retrieval weights: `./build/debug/external/basic_agent/run_grag_benchmark`

---

## Related docs

- `docs/GRAG.md` — scoring formula
- `docs/TESTING.md` — unit tests and reflection testing
- `docs/cursor_list.md` — alignment backlog
