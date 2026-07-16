# Plan L — Engine Workspace Corpus (Compose RAG ownership)

**Status:** ✅ **Complete (L3 deferred)** — 2026-07-16 (L4 closeout)  
**Prerequisites:** [Plan I](plan_i_docker_compose_v1.md) ✅ · [Plan K](plan_k_gui_api_client.md) ✅ · Phase 0 empty-volume confirmation · Phase 1 remote timeout/routing ✅  
**Roadmap:** [docker_roadmap.md](docker_roadmap.md) Step 10 ✅  

---

## Purpose

Define how the **remote Compose engine** obtains a usable RAG corpus on `thoth-workspace` (`/workspace/rag`), without breaking Plan I persistence rules or Plan K’s transport-only GUI client.

Plan L completes the **engine workspace ownership model** and the **supported workflow** for an engine-owned RAG corpus. Host GUI remote mode does **not** sync files into that workspace (Plan K).

Phase 0 confirmed a Compose named volume can be nearly empty (`rag_index.bin` ~137 bytes) while the host `agent_workspace/rag/` holds a full corpus — motivating explicit engine-owned seeding rather than host mirroring.

---

## Locked decisions (L0) — 2026-07-15

| Lock | Decision |
|------|----------|
| **Ownership model** | **O1 — Engine-owned corpus** on named volume `thoth-workspace` → `/workspace` |
| **Seed source** | Repo-controlled **`docker/seed_rag/`** only (curated markdown; reviewed in git) |
| **Host mirroring** | **Forbidden** — do not copy or sync live `agent_workspace/` into the volume as the seed path |
| **Memory bind** | **Forbidden** — do not bind-mount host `memory.db` (or full host `agent_workspace/`) into the container by default |
| **Phase 3 ingest** | **Out of scope** — no `POST /v1/rag/ingest`; no remote `setRagFiles` upload |
| **Cognate APIs** | **Out of scope** — Plan K gap remains; no conversation/cognate HTTP |
| **dev-rag bind profile** | **Deferred** until after **L1** verification (optional later; not L1) |
| **Image bake** | **Forbidden** — do not bake corpus into `Dockerfile.engine` (Plan I) |

### Persistence boundary (unchanged from Plan I / K)

```
Host GUI local:     project agent_workspace/     (in-process plugin)
Host GUI remote:    HTTP/SSE only                (no filesystem sync)
Compose engine:     volume thoth-workspace → /workspace
                      ├── memory.db              (engine-only)
                      ├── rag/                   (engine-owned; seeded explicitly)
                      └── rag/rag_index.bin      (engine rebuild after seed)
```

GUI session files, dropped RAG paths, and cognate DB contents on the host remain **local-mode concerns**. Remote engine cognition uses only `/workspace`.

---

## Seed policy (L0 baseline)

| Rule | Detail |
|------|--------|
| Source tree | `docker/seed_rag/` (in-repo). L1 creates this tree. |
| Script | `docker/seed-workspace.sh` (dev/operator, **explicit** run — never auto on `compose up`) |
| Target | Compose-attached `/workspace/rag/` on `thoth-engine` (see L1 volume discovery) |
| Whitelist | Exactly: `GRAG.md`, `HOWTO.md`, `AGENTS.md`, `cognate.md` |
| Non-default | No `--full-host-rag`; no mirroring `agent_workspace/rag/` |
| Reindex | Seed docs → remove `rag_index.bin` → restart `thoth-engine` → `ensureInitialized` |
| CI (Plan J) | Must not require seed + GGUF inference in PR packaging smoke |

---

## L1 plan 🔒 Locked — 2026-07-15

### Boundaries (unchanged)

- O1 engine-owned corpus  
- No host `agent_workspace` mirroring  
- No `memory.db` bind mounts  
- No Phase 3 ingest API  
- No Cognate APIs  
- No `compose.dev-rag.yml` (deferred to L3 after L1 verify)  
- No CI inference dependency  

### Whitelist behavior (locked)

| Rule | Detail |
|------|--------|
| Seed set | **Only** the four locked files: `GRAG.md`, `HOWTO.md`, `AGENTS.md`, `cognate.md` |
| Missing file | Script **fails** (non-zero) if any locked file is absent under `docker/seed_rag/` |
| Extra files | Any other files under `docker/seed_rag/` are **ignored** unless later added to this whitelist by a plan revision |
| Runtime source | Script reads **only** `docker/seed_rag/<whitelist>`; never `agent_workspace/` or live `docs/` at seed time |

Canonical snapshots at implement time (copy once into git under `docker/seed_rag/`):

| Seed file | Copy from |
|-----------|-----------|
| `GRAG.md` | `docs/GRAG.md` |
| `HOWTO.md` | `docs/HOWTO.md` |
| `AGENTS.md` | `AGENTS.md` (repo root) |
| `cognate.md` | `docs/cognate.md` |

### Volume discovery (locked)

| Rule | Detail |
|------|--------|
| **Forbidden** | Hardcoding `thoth_thoth-workspace` (or any fixed volume name) as the seed target mechanism |
| **Required** | Use the Compose service definition so `/workspace` is whatever volume Compose attaches to **`thoth-engine:/workspace`** |
| **Project name** | Respect `COMPOSE_PROJECT_NAME` and alternate project names (via `docker compose` in the repo / caller env — same as operator `docker compose` invocations) |
| **Preferred mechanism** | `docker compose run` (or equivalent) against service `thoth-engine` with Compose-managed `/workspace` mount + bind-mount of `docker/seed_rag` read-only for copy; optional inspect of the running/stopped service mount only for diagnostics/logging — not as a hardcoded name dependency |
| **Safety** | Never modify `/workspace/memory.db`; only write whitelist files under `/workspace/rag/` and remove `/workspace/rag/rag_index.bin` |

### Script CLI (locked)

```bash
./docker/seed-workspace.sh              # seed + clear index + restart thoth-engine
./docker/seed-workspace.sh --dry-run    # validate whitelist + print actions; no writes
./docker/seed-workspace.sh --no-restart # seed + clear index; operator restarts later
```

Algorithm:

1. Resolve repo root; require all four whitelist files under `docker/seed_rag/` (fail if any missing).  
2. Ignore unrelated files in `docker/seed_rag/`.  
3. Via Compose (`docker compose` from repo root, inheriting `COMPOSE_PROJECT_NAME`): copy only whitelist → `/workspace/rag/` on the engine service volume; `rm -f /workspace/rag/rag_index.bin`.  
4. Unless `--no-restart` / `--dry-run`: restart `thoth-engine`.  
5. Print verification hints (`ls` via compose exec/run).

### L1 deliverables

| Artifact | Action |
|----------|--------|
| `docker/seed_rag/{GRAG,HOWTO,AGENTS,cognate}.md` | New curated snapshots |
| `docker/seed_rag/README.md` | Whitelist + ignore-extras note |
| `docker/seed-workspace.sh` | Script per rules above |
| `docker/README.md` | Operator seed section |
| This plan + `completed_improvements_log.md` | L1 ✅ after verify |

### L1 verification

1. `--dry-run` exit 0 with four files listed; missing-file test fails.  
2. Real seed: four files in `/workspace/rag`; index removed; `memory.db` untouched.  
3. Restart healthy when restart requested.  
4. Volume resolution works under default and alternate `COMPOSE_PROJECT_NAME` (at least documented + one manual check).  
5. Plan J CI path unchanged (no seed required).

### Explicitly not L1

`compose.dev-rag.yml`, Phase 3, Cognate, host mirror, image bake, auto-seed on `up`, CI+GGUF seed gate.

---

## L2 plan 🔒 Locked + ✅ implemented — 2026-07-16

### Locks

| Lock | Choice |
|------|--------|
| Scope | **Docs-only** — no `SMOKE_SEED=1` / smoke.sh changes |
| Evidence C | Prefer **`/logs/chat_rag.jsonl`** (`CHAT_RAG_CONTEXT` / `CHAT_RAG_RESPONSE`) and decision-trace stages `chat_rag_context` / `chat_rag_response` over qualitative answer inspection |
| Sentinel | Exact phrase in curated seed: `THOTH_PLAN_L_SEED_SENTINEL_7f3a9c2e` (in `docker/seed_rag/HOWTO.md`) |
| Unseeded case | Document expected empty/`no_retrieval_hits` failure mode |
| Boundaries | Same as L0/L1 (O1, explicit seed, no host mirror, no memory bind, no Phase 3, no Cognate, no `compose.dev-rag.yml`) |

### Deliverables

- `docker/README.md` — Plan L L2 checklist (happy path + unseeded failure + skips)
- `docker/seed_rag/HOWTO.md` — sentinel section
- `docker/seed_rag/README.md` — sentinel pointer
- `docs/GETTING_STARTED.md` — hybrid pointer to L2 checklist
- This plan + `completed_improvements_log.md`

---

## L3 — deferred (locked 2026-07-16)

**Status:** 🔒 **Deferred** — 2026-07-16 · **not implemented** · no `compose.dev-rag.yml`  
**Decision:** Retain **L1 explicit reseeding** as the only supported Compose corpus development path until a concrete, recurring need for high-frequency `docker/seed_rag/` iteration justifies revisiting.

No `compose.dev-rag.yml` (or any L3 code) may be added while L3 remains deferred. Reopening L3 requires a new plan lock (Approve L3) after this deferral.

### Architectural justification (locked basis for deferral)

### What L3 would be (if approved)

Optional Compose override that bind-mounts the **four locked** `docker/seed_rag/*.md` files **individually, read-only** onto `thoth-engine:/workspace/rag/<file>`, while leaving:

- `thoth-workspace` named volume on `/workspace` (including `memory.db`)
- writable `rag_index.bin` on that volume (must **not** bind the whole `rag/` directory `:ro`)

Default `docker-compose.yml` unchanged. Explicit opt-in only. No CI. No Phase 3 / Cognate.

### Option comparison

#### Path A — L1 explicit reseeding only (status quo)

Operator / developer workflow:

```bash
# edit docker/seed_rag/* in git
./docker/seed-workspace.sh    # copy whitelist → volume; clear rag_index.bin; restart
# then L2 sentinel / chat_rag.jsonl probe
```

Corpus **bytes** on the volume are copies owned by the engine workspace. Host `docker/seed_rag/` is the **source of truth for content**; the volume holds the **runtime corpus**.

#### Path B — L3 optional file-level bind profile

Developer workflow:

```bash
docker compose -f docker-compose.yml -f docker/compose.dev-rag.yml up -d thoth-engine
# edit docker/seed_rag/HOWTO.md on host → visible immediately in container
# still must clear rag_index.bin + restart after content changes that affect embeddings
# L2 probe
```

Document **bytes** are host-backed (repo bind); index + memory remain engine-volume-owned. This is a **narrow development exception** to pure O1 “corpus lives only as volume copies,” not a production ownership model.

### Trade-off analysis

| Dimension | L1 reseed only | L3 file-level binds |
|-----------|----------------|---------------------|
| **Architectural simplicity** | **Stronger.** One ownership story: O1 engine-owned volume corpus; seed script is the only ingress. Matches L0 lock and Plan I “optional seed, not auto.” No second Compose topology to reason about. | **Weaker.** Two ways documents appear under `/workspace/rag` (copy vs bind). Operators must know which mode they are in; docs and failure modes double. |
| **Maintenance burden** | **Lower.** Maintain `seed-workspace.sh` + whitelist + L2 checklist. No override YAML, no bind-path drift, no “why can’t I write the index?” bugs from wrong `:ro` directory mounts. | **Higher.** Must keep four bind paths in sync with whitelist; document reindex after edit; guard against whole-dir `:ro` mistakes; test `compose config` for project-name / relative paths; risk of stale index when host files change without clearing `rag_index.bin`. |
| **Developer convenience** | **Adequate for infrequent seed edits.** Each content change costs an explicit reseed (~seconds) + restart. Fine for curated snapshots that change rarely. Awkward only if someone iterates on seed markdown many times per hour. | **Better for rapid seed-doc iteration.** Host edits show up without copy. **Does not** remove reindex/restart cost for embedding changes—convenience gain is limited to skipping the copy step. |
| **Consistency with O1** | **Fully consistent.** Volume holds independent corpus copies; host never mounts into runtime paths. Clear persistence boundary with Plan K remote GUI. | **Partial exception.** Document content is host-backed while memory/index stay volume-owned. Still far safer than binding `agent_workspace/` or `memory.db`, but it **softens** “engine-owned corpus” into “engine-owned index + host-owned seed bytes in dev.” Must be labeled as opt-in dev aid, never default. |

### Residual risks if L3 ships

1. **Stale embeddings:** Host file update without deleting `rag_index.bin` → L2 may pass Evidence A (sentinel in file) but fail or lie on Evidence C until reindex.  
2. **Wrong mount shape:** Binding `./docker/seed_rag:/workspace/rag:ro` breaks index writes — must be forbidden in the locked design.  
3. **Mode confusion:** “I seeded” vs “I’m on the bind profile” → support burden.  
4. **Scope creep:** Pressure to bind host `agent_workspace/rag` next — must remain rejected under Plan L.

### Recommendation

**Defer / cancel L3 as an active Plan L checkpoint** unless there is a demonstrated, recurring need for high-frequency editing of `docker/seed_rag/` **inside** a live Compose engine.

**Rationale:**

1. L1 + L2 already close the original regression (empty volume → no retrieval) with a clear O1 story and verified telemetry.  
2. L3’s convenience win is **narrow** (skip copy; still reindex/restart).  
3. L3 adds a second topology and a soft exception to O1 for limited benefit.  
4. Architectural simplicity and maintenance cost favor **one** supported path: edit curated seed → `./docker/seed-workspace.sh` → L2 probe.

**If** the human later wants L3, treat it as an **optional, explicitly opted-in development aid** with the file-level `:ro` design above—not as a replacement for L1, and not as production Compose default. Prefer shipping L4 closeout (roadmap/docs) next rather than L3.

**Decision locked:** **Defer L3** (2026-07-16).

| Choice | Meaning | Outcome |
|--------|---------|---------|
| Cancel L3 | Checkpoint cancelled permanently | — |
| **Defer L3** | No implementation; L1 remains sole supported path; may reopen later | ✅ **Locked** |
| Approve L3 | Implement file-level bind profile | — |

---

## Checkpoint sequence

| ID | Work | Status |
|----|------|--------|
| **L0** | Spec lock (this document) + roadmap pointer | ✅ Locked 2026-07-15 |
| **L1** | `docker/seed_rag/` + `seed-workspace.sh` + README | ✅ Implemented 2026-07-15 |
| **L2** | Docs polish + seed→restart→retrieval checklist + sentinel | ✅ Implemented 2026-07-16 |
| **L3** | Optional `compose.dev-rag.yml` (file-level binds) | 🔒 **Deferred** 2026-07-16 — not implemented |
| **L4** | Closeout (status, architecture record, roadmap/log) | ✅ Implemented 2026-07-16 |

Each checkpoint: **Plan → Review → Approve → Implement → Verify → STOP.**

---

## Plan L closeout — ✅ Complete (L3 deferred) (2026-07-16)

### Historical attribution

Plan L **does not** claim sole credit for resolving the post-migration remote GUI failure mode.

Accurate record:

- Plan L **completes the engine workspace ownership model** and the **supported workflow** for the **engine-owned RAG corpus** (O1 ownership, curated `docker/seed_rag/`, explicit `seed-workspace.sh`, L2 structured verification).
- Empty-workspace / weak-retrieval issues observed after the GUI↔remote migration were addressed by the **combination** of:
  1. **Phase 1 / Plan K** — remote HTTP timeout alignment, explicit-goal routing to `/v1/goals`, and honest remote capability messaging (no false memory/RAG sync success), and
  2. **Plan L** — engine-owned `/workspace` corpus ownership, seeding, and retrieval-evidence checklist.

### Final Architecture State

Supported operational model for **remote deployments** (Compose `thoth-engine` and hybrid host GUI against that engine):

```
Remote GUI (optional, Plan K)
    │  HTTP/SSE only — does NOT own or synchronize engine workspace
    ▼
thoth-engine
    │
    ▼
/workspace  (Compose volume thoth-workspace — ENGINE-OWNED)
    ├── rag/                 curated corpus (seeded explicitly)
    ├── rag/rag_index.bin    vector index
    ├── memory.db            cognitive / episodic memory
    └── decision_trace.jsonl workspace traces
/logs       (Compose volume thoth-logs — engine logs, e.g. chat_rag.jsonl)
```

**Supported corpus workflow:**

`docker/seed_rag/` → `./docker/seed-workspace.sh` → engine `/workspace` → L2 verification checklist

- Host `agent_workspace/` remains the **local in-process** path only.
- Remote mode does **not** mirror, bind, or sync host session files, dropped RAG paths, or `memory.db` into the engine volume.
- L3 file-level bind profile remains **deferred**; L1 reseeding is the only supported Compose corpus path.

### Boundaries preserved

O1 engine-owned corpus · explicit seeding only · no host `agent_workspace` mirror · no `memory.db` bind · no Phase 3 ingest · no Cognate HTTP · no image-baked corpus · Plan J CI unchanged · no default Compose bind of host rag.

### L3 reopen

Only via a new explicit **Approve L3** lock after this deferral.

---

## Out of scope (explicit)

- Cognate / conversation-memory / resume HTTP APIs  
- RAG ingest HTTP API (Phase 3 — separate plan)  
- Changing EngineRuntime F/G routes or GRAG scoring  
- Baking `agent_workspace/` or seed corpus into the engine image  
- Default compose bind-mount of host rag or memory  
- Automatic seeding on container start  

---

## Relationship to other plans

| Plan | Relationship |
|------|----------------|
| **I** | Volumes + “optional seed script, not auto” — Plan L **implements** that seed with a locked source tree |
| **K** | GUI remains transport-only; remote `setRagFiles` stays no-op until Phase 3 |
| **J** | CI smoke stays packaging-only; seed is operator/dev |
| **Phase 3** | Future GUI→engine document push; requires its own locked plan after L shipping |

---

## L0 verification (docs-only) ✅

- [x] O1 engine-owned model written and locked  
- [x] `docker/seed_rag/` named as sole seed source  
- [x] Host mirror / memory bind / Phase 3 / Cognate excluded  
- [x] dev-rag profile deferred past L1  
- [x] No seed script, seed files, or compose overrides in L0  

## Current gate

**Plan L is ✅ Complete (L3 deferred).** Supported remote corpus path:

`docker/seed_rag/` → `./docker/seed-workspace.sh` → engine `/workspace` → L2 verification.

Do **not** implement L3 while deferred. Further corpus work (e.g. Phase 3 ingest) requires a separate plan.

STATUS: PLAN L CLOSED
