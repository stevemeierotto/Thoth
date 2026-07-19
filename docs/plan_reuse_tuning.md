# Plan Reuse & Trajectory Tuning Reference

**Purpose:** Single reference for tunable thresholds affecting plan history reuse, trajectory-aware GRAG, and reflection replan. Constants live in `external/basic_agent/include/plan_reuse_config.h`.

**Last updated:** 2026-07-19 (G1e Phase 4 ✅ production `w_t=-0.05`; magnitude tuning paused open)

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
| `agent_workspace/retrieval_config.json` → `trajectory` | **−0.05** | Loaded at runtime | Global GRAG trajectory weight when T is active |
| `Config::wt` default | **−0.05** | `config.h` | Overridden by `retrieval_config.json` when present |

**G1d close-out (2026-07-18):** Positive-weight ablation → **DROP**. Superseded for production by G1e Phase 4.

**G1e Phase 4 (2026-07-19):** Production `trajectory: -0.05` (KEEP). Evidence: `run-1784408754379`. Magnitude tuning **paused, not dropped** (e.g. `−0.40` later). Empty-T executive zeroing remains.

**Tuning notes:**
- Resume magnitude probes only under a locked G1e amendment / owner approval.
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
