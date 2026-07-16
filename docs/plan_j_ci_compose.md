# Plan J — CI Compose Packaging Job

**Status:** 🔒 Locked — 2026-07-14 · **Implemented** 2026-07-14 (J1–J4)  
**Prerequisites:** [Plan I](plan_i_docker_compose_v1.md) ✅ · Step 6 hybrid docs ✅  
**Next:** Step 8 nightly containerized inference · [Plan K](plan_k_gui_api_client.md) 🔒 (optional GUI HTTP client)  
**Roadmap:** [docker_roadmap.md](docker_roadmap.md) Step 7 — optional CI packaging verification

---

## Purpose

Add a **lightweight, parallel CI job** that proves Plan I packaging still works:

1. `docker/Dockerfile.engine` builds successfully  
2. A Docker Compose stack starts (CI-safe profile — see below)  
3. The engine HTTP **`GET /health`** endpoint responds  

This is **CI packaging verification only**. It does **not** change engine behavior, cognitive logic, or replace host-native `ctest -L pr`.

Plans F–I remain prerequisites and are exercised unchanged inside containers.

---

## Goal

Protect the Docker packaging surface in PR CI without requiring GGUF models, GPUs, or long inference timeouts.

| Layer | Owner |
|-------|--------|
| Runtime / HTTP / SSE / inference | Plans F–H (unchanged) |
| Compose / Dockerfile / smoke | Plan I (unchanged semantics) |
| CI wiring for packaging smoke | **Plan J** |

---

## Current state (baseline)

| Component | Today |
|-----------|-------|
| Dockerfile / compose / `docker/smoke.sh` | Plan I ✅ |
| Host-native PR suite | `.github/workflows/ci-security.yml` — `cmake --preset engine-only` + `ctest -L pr` |
| Full `docker/smoke.sh` | Needs healthy `llama-server` → **requires GGUF** on `llama-models` |
| Default `docker-compose.yml` | `thoth-engine` `depends_on: llama-server: service_healthy` |
| Plan J CI job | **Not wired** |
| Nightly compose + inference | Roadmap **Step 8** (deferred) |

### Critical packaging constraint (discovered in Plan I ops)

Default compose cannot become healthy without a model file at `${LLAMA_CHAT_MODEL:-/models/chat.gguf}`. Therefore **PR CI must not use vanilla `docker compose up` against the default stack** unless a model is supplied.

Plan J solves this with a **CI compose override** that starts packaging-relevant services without requiring GGUF (engine-only for PR). Full stack + inference remains Step 8 / optional nightly.

---

## Non-negotiable constraints

1. **No engine behavior changes** — no C++ / cognitive / GRAG / planner / memory edits.  
2. **No replacement of native CI** — existing `engine-only` + `ctest -L pr` stays the primary gate.  
3. **No GGUF requirement in PR CI** — PR job must pass without operator-supplied models.  
4. **Heavy inference / goals / full smoke with models** → **Step 8** (nightly), not Plan J PR.  
5. **Reuse Plan I artifacts** — Dockerfile, compose, smoke — extend rather than reimplement.  
6. **Optional plan** — shipping Plan J does not block other roadmap work if deferred.

### Explicitly excluded (Plan J)

| Excluded | Owner / later |
|----------|----------------|
| GGUF download or model fixture in PR | Step 8 / operator |
| `POST /v1/goals` requiring planner LLM | Step 8 |
| Full 8-step `docker/smoke.sh` as PR required gate (without CI mode) | Step 8 or CI-mode subset |
| Plan K GUI HTTP client | Plan K |
| Auth / TLS / K8s | Out of roadmap v1 |
| ASan / GUI / `test-suite-full` expansion | Existing workflow jobs |

---

## PR CI vs nightly / Step 8

| Concern | Plan J (PR) | Step 8 (nightly — out of J scope) |
|---------|-------------|-----------------------------------|
| Build `Dockerfile.engine` | ✅ Required | ✅ |
| Start compose | ✅ **CI override** (engine-focused) | ✅ Default stack + models |
| `GET /health` | ✅ Required | ✅ |
| `GET /ready` | ✅ Allowed (capabilities, no inference) | ✅ |
| `llama-server` + GGUF | ❌ Not required | ✅ Required |
| `POST /v1/goals` / inference | ❌ | ✅ |
| Full `docker/smoke.sh` | Optional **CI subset** only | Full or model-backed smoke |
| Runtime | Bound (e.g. ≤ 20–30 min image build + short smoke) | Longer OK |

---

## Proposed design

### CI compose override (locked intent)

Add a Compose override file used **only** by CI / local packaging smoke, e.g. `docker/compose.ci.yml` (exact name locked at implementation):

- Runs **`thoth-engine`** build + publish port `8090`  
- Does **not** require `llama-server` to be healthy (omit service, or disable `depends_on` condition)  
- Keeps Plan I env for workspace/logs bind defaults inside the container  
- Does **not** bake models into the image  

Invocation:

```bash
docker compose -f docker-compose.yml -f docker/compose.ci.yml up -d --build
```

### CI smoke script (locked intent)

Either:

**Option A (preferred):** extend `docker/smoke.sh` with `SMOKE_MODE=ci` (or `--ci`) that:

1. Uses the CI compose file set  
2. Waits for **`thoth-engine` healthy only**  
3. `GET /health` (required); `GET /ready` (required — engine feature readiness, not remote inference)  
4. Optionally `POST /v1/chat` with `/help` (no inference backend required for builtins)  
5. Skips goals / skips llama  
6. `docker compose … down`

**Option B:** add `docker/ci-smoke.sh` that wraps the same sequence and calls into shared helpers — only if extending `smoke.sh` would harm operator UX.

Full default `docker/smoke.sh` (8 steps, llama + optional goals) remains for **operators / Step 8**.

### GitHub Actions job (locked intent)

Add a **parallel** job to CI (new workflow file or job in existing workflow — prefer **separate** `ci-compose.yml` so failures are obvious and do not block security/ASan matrix semantics incorrectly):

| Setting | Value |
|---------|--------|
| Trigger | `pull_request` + `push` to `main` (match existing CI patterns) |
| Runner | `ubuntu-latest` with Docker |
| Steps | checkout → (optional Buildx) → CI smoke script |
| Native `ctest -L pr` | **Unchanged** in existing job |

Job may be marked `continue-on-error: false` once green; until first green landings, implementers may use draft PRs — do not silently weaken native PR gates.

---

## Checkpoints

Same stop-gate discipline as Plans F–I: implement one checkpoint, verify, **STOP**.

| CP | Scope | Intentionally NOT included | Verify |
|----|-------|------------------------------|--------|
| **J1** | Spec lock + CI compose override file (`docker/compose.ci.yml` or agreed name) documenting engine-only packaging up | Workflow wiring, smoke rewrite | `docker compose -f … -f … config` validates; `up -d --build` starts engine without GGUF; `curl /health` |
| **J2** | CI smoke entrypoint (`SMOKE_MODE=ci` or `docker/ci-smoke.sh`) — build, wait engine healthy, `/health` (+ `/ready`), down | Goals, llama, Step 8 | Script exit 0 on clean machine with Docker, no models |
| **J3** | GitHub Actions job calling CI smoke | Nightly models, `test-suite-full` | Job green on PR/push; native `ctest -L pr` job still present and unchanged |
| **J4** | Docs: `docker/README.md`, roadmap Step 7 status, plan status → IMPLEMENTED; note Step 8 boundary | Plan K, auth | Docs accurate; operator still uses full smoke with models |

### Gate rule (mandatory)

After each checkpoint: **STOP.** Confirm:

1. Constraints 1–6 hold.  
2. No C++ / cognitive changes.  
3. Verification criteria met.  
4. No GGUF requirement crept into PR CI.

**J4 is the final implementation stop** before Plan J is complete.

---

## Files expected to change

| Path | Role |
|------|------|
| `docker/compose.ci.yml` (name locked at J1) | CI override — no GGUF / no llama health dependency |
| `docker/smoke.sh` and/or `docker/ci-smoke.sh` | CI-mode packaging smoke |
| `.github/workflows/ci-compose.yml` (or equivalent) | Parallel packaging job |
| `docker/README.md` | Operator vs CI smoke distinction |
| `docs/docker_roadmap.md` | Step 7 → Done when J complete |
| `docs/plan_j_ci_compose.md` | This spec (status updates) |
| `docs/completed_improvements_log.md` | Completion entry at J4 |

**Must not change (Plan J):** `external/basic_agent/**` sources, GUI, GRAG, planner, evaluation harnesses, Plan I default compose semantics for operators (override is additive).

---

## Verification commands

Assume repo root; Docker available.

**J1 — CI compose override**

```bash
docker compose -f docker-compose.yml -f docker/compose.ci.yml config >/dev/null
docker compose -f docker-compose.yml -f docker/compose.ci.yml up -d --build
# Wait until thoth-engine healthy (no llama required)
curl -sf http://127.0.0.1:8090/health
docker compose -f docker-compose.yml -f docker/compose.ci.yml down
```

**J2 — CI smoke script**

```bash
SMOKE_MODE=ci ./docker/smoke.sh
# or: ./docker/ci-smoke.sh
```

**J3 — Actions**

- Open PR → `ci-compose` (or named) job green  
- Confirm `ci-security` (or existing) still runs `ctest -L pr`

**J4 — Docs**

- `docker/README.md` describes PR CI path vs full operator smoke  

**Regression (always):**

```bash
# Host-native — must remain the primary gate; not replaced by J
ctest --test-dir build/engine-only -L pr -j1 --output-on-failure
```

---

## Stop conditions

Stop and request approval if implementation would require any of:

- Adding GGUF download or large model fixtures to PR CI  
- Changing `ExecutiveController` / inference / GRAG for CI convenience  
- Making the compose CI job **replace** `ctest -L pr`  
- Softening Plan I default compose (operators must still get llama + engine with models)  
- Expanding into Step 8 nightly inference scope under the Plan J label  

---

## Completion criteria

- [x] CI compose override starts `thoth-engine` **without** GGUF  
- [x] CI smoke proves image build + `/health` (and `/ready`)  
- [x] GitHub Actions packaging job runs on PR/push in parallel with native tests  
- [x] Native `ctest -L pr` CI path unchanged  
- [x] Full model-backed smoke / goals deferred and documented as Step 8  
- [x] Roadmap Step 7 marked done; this plan marked IMPLEMENTED  

---

## Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Image build time dominates PR | Slow CI | Cache Buildx layers; fail fast on Dockerfile; bound job timeout |
| Override drifts from default compose | False confidence | Override only removes llama dependency / health gate; keep engine build/env aligned with Plan I |
| Operators run CI override by mistake | Missing inference | Document clearly; default `docker compose up` unchanged |
| `/ready` confused with inference health | False failures | Document Plan F/G semantics — capabilities ≠ llama reachable |
| Full `smoke.sh` used in PR without CI mode | Flaky red PRs | Explicit `SMOKE_MODE=ci`; default smoke remains operator/Step 8 |

---

## Rollback

Remove workflow job + CI override + CI smoke mode. Plan I operator compose and native CI unchanged.

---

## Plan history

| Date | Change |
|------|--------|
| 2026-07-14 | Initial draft: J1–J4, CI override for no-GGUF PR, reuse smoke with CI mode, Step 8 boundary |
| **Locked** | 2026-07-14 — packaging CI only; CI compose override; no GGUF in PR; native `ctest -L pr` unchanged |

---

## Plan lock record

| Field | Value |
|-------|-------|
| **Locked** | 2026-07-14 |
| **Purpose (locked)** | Parallel CI packaging job: image build + CI compose + `/health` |
| **PR policy (locked)** | No GGUF required |
| **Native CI (locked)** | `ctest -L pr` unchanged and not replaced |
| **Compose approach (locked)** | Additive CI override; default Plan I compose for operators |
| **Smoke (locked)** | CI subset of smoke; full model smoke → Step 8 |
| **CI smoke checks (locked)** | Engine healthy; `GET /health` required; `GET /ready` required; optional `/v1/chat` `/help`; skip goals/llama |
| **Preferred smoke shape (locked)** | Extend `docker/smoke.sh` with `SMOKE_MODE=ci` (Option A); Option B only if needed |
| **Workflow shape (locked)** | Prefer separate `.github/workflows/ci-compose.yml` parallel job |
| **Scope (locked)** | Workflow + compose override + smoke mode + docs only — no engine/cognitive code |
| **Deferred** | Step 8 nightly inference; Plan K; auth/TLS/K8s |
| **Implementation** | Requires explicit approval per [AGENTS.md](../AGENTS.md); checkpoints J1–J4 with STOP gates |
| **Post-lock changes** | Protocol lock rule — stop and request approval before editing this spec |

---

STATUS: IMPLEMENTED
