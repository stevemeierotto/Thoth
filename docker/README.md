# Thoth Docker Compose v1

Plan I packages the headless `thoth-engine` runtime (Plans F–H) with a co-located `llama-server` inference container. This is **packaging only** — no changes to engine semantics.

## Prerequisites

- Docker Engine 24+ with Compose v2
- GGUF models on the `llama-models` named volume (see below)
- ~4 GB free disk for images + models

## Quick start

From the repository root:

```bash
# 1. Place models (example paths inside the volume)
docker volume create llama-models 2>/dev/null || true
docker run --rm -v llama-models:/models -v "$PWD/models:/seed:ro" alpine \
  sh -c 'cp -n /seed/*.gguf /models/ 2>/dev/null || true'

# 2. Start stack
docker compose up -d --build

# 3. Verify
curl -s http://127.0.0.1:8090/health
curl -s http://127.0.0.1:8090/ready | jq .
```

Full smoke (requires chat model at `/models/chat.gguf` by default):

```bash
chmod +x docker/smoke.sh
./docker/smoke.sh
```

Without models, use `SMOKE_SKIP_INFERENCE=1` to skip the `/v1/goals` step when planning cannot reach inference.

## Services

| Service | Image | Host port | Role |
|---------|-------|-----------|------|
| `thoth-engine` | `thoth-engine:local` (built) | `8090` | HTTP API + SSE (`--serve`) |
| `llama-server` | `ghcr.io/ggerganov/llama.cpp:server-b4719` | *(internal)* | `llama_cpp` inference |

**Pinned llama image:** `ghcr.io/ggerganov/llama.cpp:server-b4719` — never `:latest`. When upgrading, pick a new version tag from [GitHub Packages](https://github.com/ggerganov/llama.cpp/pkgs/container/llama.cpp) and update `docker-compose.yml`.

## Model setup

`llama-server` loads the chat model from:

```
${LLAMA_CHAT_MODEL:-/models/chat.gguf}
```

1. Copy a chat GGUF to the `llama-models` volume as `chat.gguf`, **or** set `LLAMA_CHAT_MODEL` in `.env`.
2. For embeddings / RAG, also place an embedding GGUF and set `OLLAMA_EMBED_MODEL` / `config.json` `embedding_model` to match the name you pass to the API.
3. First start with an empty `thoth-workspace` volume creates SQLite on demand; initial RAG indexing can be **slow** — allow extra time before `/ready` reports full capability.

Example `.env` (optional, do not commit secrets):

```bash
THOTH_ENGINE_PUBLISH_PORT=8090
LLAMA_CHAT_MODEL=/models/qwen2.5-3b-instruct-q4_k_m.gguf
OLLAMA_MODEL=chat
OLLAMA_EMBED_MODEL=nomic-embed-text
THOTH_LOG_CONFIG=1
```

## Configuration precedence (locked)

```
compose .env / env_file
        ↓
docker-compose.yml environment: block
        ↓
EnvLoader / workspace config.json (unset keys only)
```

Compose-owned keys (`THOTH_WORKSPACE_PATH`, `THOTH_LOGS_PATH`, `THOTH_INFERENCE_*`, `THOTH_ENGINE_*`) are set in `docker-compose.yml` and should not be overridden via `config.json` inside the container.

## Volumes

| Volume | Mount | Contents |
|--------|-------|----------|
| `thoth-workspace` | `/workspace` | `memory.db`, `rag/`, `config.json`, traces |
| `thoth-logs` | `/logs` | metrics / benchmark JSONL |
| `llama-models` | `/models` (ro on server) | Operator-supplied GGUF files |

Use **named local volumes** for SQLite (`memory.db`). Do not mount workspace over NFS — WAL corruption risk.

## Environment (compose defaults)

| Variable | Value |
|----------|-------|
| `THOTH_WORKSPACE_PATH` | `/workspace` |
| `THOTH_LOGS_PATH` | `/logs` |
| `THOTH_PROJECT_ROOT` | `/workspace` (deterministic deploy override; optional when walk terminates) |
| `THOTH_INFERENCE_BASE_URL` | `http://llama-server:8080` |
| `THOTH_INFERENCE_BACKEND` | `llama_cpp` |
| `THOTH_ENGINE_BIND` | `0.0.0.0` |
| `THOTH_ENGINE_PORT` | `8090` |

## Ollama profile (optional, not default)

For transitional parity with host-native Ollama:

1. Run Ollama separately (host or container on `thoth-net`).
2. Override inference env on `thoth-engine`:

```yaml
environment:
  THOTH_INFERENCE_BASE_URL: http://host.docker.internal:11434
  THOTH_INFERENCE_BACKEND: ollama
```

Remove or disable the `llama-server` service and its `depends_on` when using Ollama exclusively.

## Operations

```bash
# Logs
docker compose logs -f thoth-engine

# Graceful stop (30s grace, SIGTERM)
docker compose stop thoth-engine

# Rebuild engine after code changes
docker compose up -d --build thoth-engine

# Tear down (volumes persist)
docker compose down
```

## Build engine image only

```bash
docker build -f docker/Dockerfile.engine -t thoth-engine:local .
docker run --rm thoth-engine:local --version
```

Verify no GUI binary in the image:

```bash
docker run --rm --entrypoint sh thoth-engine:local -c \
  'command -v thoth-engine && ! command -v thoth-control-panel 2>/dev/null'
```

## Hybrid development

Host-native builds remain primary (`cmake --preset engine-only`). Point a local GUI or `curl` at `http://127.0.0.1:8090` while the engine runs in Docker.

## See also

- [plan_i_docker_compose_v1.md](../docs/plan_i_docker_compose_v1.md)
- [docker_roadmap.md](../docs/docker_roadmap.md)
- [GETTING_STARTED.md](../docs/GETTING_STARTED.md)
