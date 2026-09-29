# MTCP v1.1 characterization result

| Field | Value |
| --- | --- |
| Protocol | MTCP v1.1, sealed |
| Protocol path | `docs/MAX_TOKEN_CHARACTERIZATION_PROTOCOL_v1.1.md` |
| Protocol seal | `bf6602ca26c580e75ea61b07f09896ee788dee94` |
| Result date | 2026-09-29 |
| Decision source | Sealed §17 applied to the Stage A records below |
| Runtime evidence | `/home/steve/mtcp-char-s0/v11-stage-a` and `/home/steve/mtcp-char-s0/v11-seed-gate` |

This document records the completed characterization. It does not change the sealed protocol, the analyzer thresholds, the Stage A files, S0-CHAR, or any deployment setting.

## Decision

| Quantity | Result |
| --- | --- |
| 512 pressure slots | 3 |
| 1024 pressure slots | 3 |
| Pressure reduction | 0 |
| 512 programmatic fallbacks | 2 |
| 1024 programmatic fallbacks | 2 |
| 512 length-associated structured failures | 3 |
| 1024 length-associated structured failures | 3 |
| 1024 material improvement over 512 | NO |
| Selected ceiling | 512 |
| Adequacy | NOT MET |
| Stage B | NOT TRIGGERED |
| 2048 | NOT RUN |

Sealed analyzer label: `512 selected; adequacy not met`. `stage_b` is false.

**“512 selected” does not mean 512 was demonstrated adequate. Adequacy was not met.**

MTCP v1.1 provides no experimental basis for selecting 2048 because the prospective Stage B predicate was not triggered.

Pressure locations changed between conditions. That movement is recorded and is not a material improvement. The sealed decision uses the prospective §17 counts.

512 pressure slots:

- G2 `plan`
- G2 `plan_retry`
- R1 `revision_retry`

1024 pressure slots:

- G1 `plan_retry`
- R1 `revision`
- R1 `revision_retry`

## Frozen execution identities

| Identity | Value |
| --- | --- |
| Product | `adec59e4d9981a2abb46a5eea350ebfc65053a61` |
| Engine | `a31215db040eba5fa3b2fbd87fb9c96728c38efa` |
| Engine image | `sha256:a272bd9f2a4d6867a6bf12d702ef6ce879d1e788af9ca54fab8acb8c64e639c3` |
| llama.cpp image | `sha256:823b6f019cafbee8878dfdd0d4750eae4f81dfafb60dc1fbefb66794a59903c8` |
| llama.cpp build | `b9994-14d3ba45f` |
| Chat model | `/models/Qwen2.5-7B-Instruct-Q4_K_M.gguf` |
| Chat model SHA-256 | `65b8fcd92af6b4fefa935c625d1ac27ea29dcb6ee14589c55a8f115ceaaa1423` |
| Embedding model | `/models/nomic-embed-text.gguf` |
| Embedding model SHA-256 | `f7af6f66802f4df86eda10fe9bbcfc75c39562bed48ef6ace719a251cf1c2fdb` |
| Context | `n_ctx` 8192 |
| Embedding context | 512 |
| Backend / mode | `llama_cpp` / `chat` |
| Seed | 17001, deterministic paired mode |
| Text observation limit | 4356 seconds, identical for the seed gate, 512, and 1024 |
| Embedding timeout | 300 seconds |
| Retrieval SHA-256 | `b4eab95e104f84de05ee7c1ac26e1117447104c983ce924942e987db073df396` |
| Corpus-set SHA-256 | `155f9f0be3a5210730fd5042bcaf228ea1d4421d21b411ea1890e769d58a8921` |
| Task-set SHA-256 | `a41031ff84308edbaf40804f4e96aae8b85395a0ff9e00fffc7afe402002ec90` |
| Appendix A asset SHA-256 | `7d8a3b4bd07663737c50d3a56304dedaca37e2b6db9cddea8c6e0140a353ec39` |
| S0 archive SHA-256 | `76153c66d1fa10bf0c05d7a568eff5d4b0b83c8332988de62f193985a2b7d9c3` |
| S0 manifest SHA-256 | `3afdb4d20aa2acb6ea1e711523be0ac9ae57e54750adb53103a69b605bdfd528` |
| Restored `memory.db` | `a7bc9da2abbe814805965109ad016b0606da7786ebb3d4574f069bd1ae15bb52` |
| Restored `rag_index.bin` | `aaf5056496dc943081b745675b794b58131dca8015160b805dad5d775bc9bf3b` |
| R1 wrapper SHA-256 | `a3ec75149480a7574cbf36358e05a3154fe4f2dbdf9ea64537efed40795fd144` at both ceilings |
| R1 input timestamps | unchanged at `1710000000000` |

## Seed gate

Deterministic paired mode passed. Both raw provider completions are byte-identical.

| Call | provider_ok | Usage | Finish | Prompt | Completion | Elapsed | Raw SHA-256 |
| --- | --- | --- | --- | ---: | ---: | ---: | --- |
| A | true | reported | stop | 235 | 52 | 97964 ms | `0061a7bd6fd6e588ffd74537644f0953f13f76d1ebd3c145a7e23607aca20683` |
| B | true | reported | stop | 235 | 52 | 34324 ms | `0061a7bd6fd6e588ffd74537644f0953f13f76d1ebd3c145a7e23607aca20683` |

No independent `total_tokens` field was recorded. The chat path wrote no progress file because it did not supply the observer generation id. That absence did not enter the gate decision.

## Condition validity

Both Stage A conditions are valid. Every scored call has `provider_ok` true and `provider_usage` reported. No call has context overflow. No call has unavailable provider usage. No scored task was rerun.

Chat calls on both ceilings produced no progress file. Goal and revision calls produced `GENERATION_PROGRESS` samples, with diagnostic `GENERATION_PROGRESS_GAP` rows on 512 G2, 512 R1, 1024 G2, and 1024 R1. Progress telemetry did not enter §17 scoring.

## 512 call records

| Task | Call | Finish | Prompt | Completion | Ceiling | Pressure | Structured | Length-associated | Elapsed |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- | ---: |
| C1 | chat | stop | 150 | 8 | 512 | no | no | no | 11724 ms |
| C2 | chat | stop | 163 | 3 | 512 | no | no | no | 11847 ms |
| C3 | chat | stop | 279 | 356 | 512 | no | no | no | 207580 ms |
| G1 | plan | stop | 201 | 77 | 512 | no | validation failed | no | 60025 ms |
| G1 | plan_retry | stop | 312 | 77 | 512 | no | validation failed, fallback | no | 45975 ms |
| G1 | synthesis | stop | 64 | 81 | 512 | no | no | no | 28580 ms |
| G2 | plan | length | 272 | 512 | 512 | yes | validation failed | yes | 198811 ms |
| G2 | plan_retry | length | 819 | 512 | 512 | yes | validation failed, fallback | yes | 404554 ms |
| G2 | synthesis | stop | 135 | 236 | 512 | no | no | no | 156729 ms |
| R1 | revision | stop | 575 | 450 | 512 | no | validation failed | no | 304162 ms |
| R1 | revision_retry | length | 1059 | 512 | 512 | yes | kept existing plan | yes | 538511 ms |

G2 goal outcome is `completed`.

## 1024 call records

| Task | Call | Finish | Prompt | Completion | Ceiling | Pressure | Structured | Length-associated | Elapsed |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- | ---: |
| C1 | chat | stop | 150 | 8 | 1024 | no | no | no | 42550 ms |
| C2 | chat | stop | 163 | 4 | 1024 | no | no | no | 12384 ms |
| C3 | chat | stop | 279 | 182 | 1024 | no | no | no | 149506 ms |
| G1 | plan | stop | 201 | 77 | 1024 | no | validation failed | no | 46968 ms |
| G1 | plan_retry | length | 312 | 1024 | 1024 | yes | validation failed, fallback | yes | 823203 ms |
| G1 | synthesis | stop | 64 | 81 | 1024 | no | no | no | 57170 ms |
| G2 | plan | stop | 272 | 252 | 1024 | no | validation failed | no | 179043 ms |
| G2 | plan_retry | stop | 558 | 188 | 1024 | no | validation failed, fallback | no | 197399 ms |
| G2 | synthesis | stop | 135 | 236 | 1024 | no | no | no | 140674 ms |
| R1 | revision | length | 575 | 1024 | 1024 | yes | validation failed | yes | 806631 ms |
| R1 | revision_retry | length | 1634 | 1024 | 1024 | yes | kept existing plan | yes | 1575133 ms |

## What was not done

Stage B was not opened. No 2048 generation was run. The engine image was not rebuilt. The sealed protocols were not edited. S0-CHAR was not replaced. C6-LIVE and EGAR-LAB were not used. This result was not pushed by the act of writing it.
