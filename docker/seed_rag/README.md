# Compose engine RAG seed corpus (Plan L)

Repo-controlled snapshots for **`./docker/seed-workspace.sh`**.

## Locked whitelist

Only these files are seeded into the engine volume (`/workspace/rag/`):

| File | Origin (copied into git once) |
|------|--------------------------------|
| `GRAG.md` | `docs/GRAG.md` |
| `HOWTO.md` | `docs/HOWTO.md` (+ Plan L sentinel section) |
| `AGENTS.md` | `AGENTS.md` (repo root) |
| `cognate.md` | `docs/cognate.md` |

- The seed script **fails** if any whitelist file is missing.
- **Any other files** in this directory are ignored unless Plan L revises the whitelist.
- Do **not** sync from live `agent_workspace/` (Plan L lock).

## Retrieval sentinel (Plan L L2)

`HOWTO.md` contains the unique phrase:

```text
THOTH_PLAN_L_SEED_SENTINEL_7f3a9c2e
```

Use that exact string in the L2 chat probe and when grepping `CHAT_RAG_CONTEXT` / decision-trace stages. See [`docker/README.md`](../README.md) § Plan L L2 checklist.

Spec: [`docs/plan_l_workspace_corpus.md`](../../docs/plan_l_workspace_corpus.md)
