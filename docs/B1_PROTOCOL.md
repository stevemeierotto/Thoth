# B1 — 30-Case Publication Benchmark Protocol

**Protocol version:** B1 v1.1  
**Status:** 🔒 **Phase 0–1 methodology locked**; Phase 1 **Candidate** membership materialized 2026-07-19 — **awaiting owner freeze approval**  
**Suite version:** `b1_v1` — lifecycle **`candidate`** (not frozen)  
**Candidate artifact:** [`baselines/b1_v1_membership_candidate.md`](baselines/b1_v1_membership_candidate.md) (+ [`.json`](baselines/b1_v1_membership_candidate.json))  
**Checkpoint tracking:** [`cursor_list.md`](cursor_list.md) § B1  
**Related:** [`benchmark_environment.md`](benchmark_environment.md) (E1); [`new_corpus_tests.md`](new_corpus_tests.md) (historical design notes); full registry suite (development / regression / tuning)

**Supersedes:** B1 v1.0 Phase 0 lock (2026-07-19). v1.1 adds normative membership lifecycle, Selection Audit, Corpus Manifest, mapping freeze, and strengthened gap protocol for Phase 1 reproducibility. Suite structure and §6.5 selection algorithm intent are unchanged.

---

## Document structure

| Class | Sections | Binding? |
|-------|----------|----------|
| **Normative** | Purpose through Publication Outputs; Freeze / versioning; Sequencing | **Yes** — SHALL/MUST |
| **Informative** | Rationale, doc map | **No** |
| **Appendix A** | Implementation notes | **No** — non-normative; may change without new science version if behavior unchanged |

If informative or appendix text conflicts with a normative rule, the **normative** rule governs.

**Protocol Lock Rule:** This document is locked. Do not silently revise normative sections during later phases. If implementation reveals the methodology must change, stop and request owner approval before editing this file. Corrections that change frozen suite *content* after membership freeze require a **new suite version** (`b1_v2`, …), not silent edits to `b1_v1`.

**Science / implementation separation:** Normative methodology SHALL remain valid even if registry APIs, CLI flags, or binary names change. Concrete wiring belongs only in Appendix A or later implementation phases.

---

## 1. Purpose

Define a **stable, reproducible 30-case publication benchmark** for Zenodo V3 and paper-facing claims, evaluated only under **E1-certified** environments, without retuning retrieval or assuming experimental outcomes.

---

## 2. Scope

| In scope | Out of scope |
|----------|--------------|
| Methodology for versioned publication suite `b1_v*` | GRAG / embedding / scorer / weight retunes |
| Objective case-selection rules | Replacing the full development registry suite |
| E1-only authority for publication artifacts | Zenodo upload mechanics (separate fork after B1 close-out) |
| Integrity criteria vs reported metrics | Causal claims that one retrieval method must beat another |
| Reproducibility and publication outputs | G1e magnitude probes |

---

## 3. Research questions

Descriptive questions only (not pass/fail hypotheses):

1. Under an E1-certified environment, what are retrieval quality metrics on the frozen `b1_v1` 30-case publication benchmark?
2. How do metrics break down by case type (UNAMBIGUOUS / GOAL_DISAMBIGUATES / TRAJECTORY_DISAMBIGUATES) and by source paper?
3. Are publication integrity conditions satisfied (all cases execute; expected documents present; provenance complete)?

The protocol **records** outcomes. It does **not** require any retrieval method to outperform any other.

---

## 4. Non-goals

B1 SHALL NOT:

- Retune GRAG weights (including trajectory weight)
- Retune or swap embedding models to improve B1 numbers
- Modify retrieval, scoring formulas, or benchmark weighting
- Require statements such as “GOAL must outperform RAG” (or any hypothesis-as-acceptance-gate)
- Delete or replace the full development/regression suite
- Perform Zenodo V3 upload or rewrite paper narrative beyond citing B1 artifacts
- Redesign the five-paper corpus unless a future suite version explicitly requires it

---

## 5. Corpus definition

### 5.1 Papers (publication corpus)

The publication corpus is the five external research papers used by the research benchmark, as sandboxed text artifacts:

| Paper key | Role (informative) | Canonical filename pattern |
|-----------|--------------------|----------------------------|
| `rag` | Retrieval-Augmented Generation (Lewis et al.) | `2005.11401*.txt` |
| `react` | ReAct | `2210.03629*.txt` |
| `genagents` | Generative Agents | `2304.03442*.txt` |
| `memgpt` | MemGPT | `2310.08560*.txt` |
| `cot` | Chain-of-Thought (Wei et al.) | `2201.11903*.txt` |

**Normative binding for `b1_v1`:** expected files and the **filename → `paper_key` mapping** SHALL be recorded in the Phase 1 **Corpus Manifest**. That mapping is part of the benchmark definition for `b1_v1` (see §6.7).

### 5.2 Boundary

- Expected documents SHALL lie inside the agent workspace docs sandbox used by the research corpus indexer.  
- Expected documents SHALL NOT be live application source under `external/` or the Thoth GUI tree.  
- Cases identify an **expected source file**, not a transient chunk id.

### 5.3 Relationship to other suites

| Suite | Role | Stability |
|-------|------|-----------|
| **Full registry suite** | Development, regression detection, tuning | May evolve for engineering |
| **30-case publication benchmark** (`b1_v1`, …) | Publication, Zenodo, paper-facing results | **Frozen** after membership freeze |

Publication, Zenodo, and paper claims that cite B1 SHALL reference a frozen `suite_id` and E1-certified run provenance—not ad-hoc full-suite means alone.

---

## 6. Case selection rules

### 6.1 Suite structure (frozen at Phase 0)

For every publication suite version `b1_v*`:

| Constraint | Value |
|------------|--------|
| Total cases | **30** |
| Papers | **5** (corpus §5.1) |
| Cases per paper | **6** |
| Types used | `UNAMBIGUOUS`, `GOAL_DISAMBIGUATES`, `TRAJECTORY_DISAMBIGUATES` only |
| Global type counts | **10** of each type |
| Per paper × type | Exactly **2 UNAMBIGUOUS + 2 GOAL_DISAMBIGUATES + 2 TRAJECTORY_DISAMBIGUATES** |

### 6.2 Eligibility (pool filters)

A pool case is eligible for `b1_v1` only if all hold:

1. **Type** ∈ {`UNAMBIGUOUS`, `GOAL_DISAMBIGUATES`, `TRAJECTORY_DISAMBIGUATES`}  
2. **Expected file** maps to exactly one paper key in §5.1 and the file exists in the corpus tree  
3. **Query** is non-empty deterministic literal text  
4. For `TRAJECTORY_DISAMBIGUATES`, **trajectory** text is non-empty deterministic literal text  
5. **Case ID** is unique in the pool  

### 6.3 Duplicate semantic intent

Within a single `(paper_key, type)` cell, two eligible cases are **duplicates** if either:

- Normalized query strings are equal, where normalize = Unicode NFC, trim, collapse internal whitespace to single spaces, case-fold; **or**  
- Declared `intent_id` values are equal (if present). For the existing pool, `intent_id` defaults to `case_id` (so distinct IDs are not duplicates under this clause alone).

When duplicates exist, **retain the case with the lexicographically smallest `case_id`**; discard others from that cell’s eligible set.

### 6.4 Prior E1 validation tag (informational only)

For each selected case, Phase 1 SHALL record one of:

| Tag | Meaning |
|-----|---------|
| `e1_attested` | Case ID appears in at least one prior E1-attributed research-corpus run log available at selection time |
| `unvalidated_until_b1_run` | No such attestation found |

These tags are **informational metadata only**. They SHALL NOT influence eligibility, deduplication, lexicographic ranking, cell fills, or membership inclusion/exclusion. Incomplete attestation scans SHOULD prefer `unvalidated_until_b1_run`. Missing attestation is not a publication integrity failure by itself.

### 6.5 Deterministic selection algorithm (`b1_v1`)

Phase 1 SHALL materialize a **Candidate b1_v1 Membership** by this algorithm (no human “prefer better cases”):

1. Load the research-paper case pool (all cases whose expected file maps to §5.1).  
2. Bind the **Corpus Manifest** (§6.7) — filenames + `filename → paper_key` mapping.  
3. Apply eligibility filters (§6.2); record removals in the **Selection Audit** (§6.8).  
4. Within each `(paper_key, type)` cell, remove duplicates (§6.3); record removals in the audit.  
5. Sort remaining IDs in each cell by **lexicographic ascending `case_id`**.  
6. Take the **first 2** IDs in each of the 15 cells (5 papers × 3 types); record lexicographic cutoff exclusions in the audit.  
7. Attach attestation tags (§6.4) as metadata only.  
8. Emit **benchmark summary statistics** (§6.9) and **artifact metadata** (§6.10).  
9. If any gap (§6.6) → **STOP** (no Candidate freeze path).  
10. Else emit **Candidate b1_v1 Membership** + audit + manifest + metadata; **STOP for owner review**.

Union of the 30 IDs is the Candidate membership table. It becomes **Frozen** only after explicit owner approval (§6.11).

### 6.6 Gap protocol

If any `(paper_key × type)` cell has **fewer than two** eligible cases after eligibility + deduplication:

| Forbidden | Required |
|-----------|----------|
| Borrow from another paper | **STOP** for owner review |
| Borrow another case type | Do not emit a Frozen membership |
| Relax eligibility rules | Record gap cell(s) in the Selection Audit |
| Manually substitute cases | Do not silently “repair” |

No automatic fill. No partial freeze of an incomplete suite (e.g. 28 cases). Owner may later authorize pool repair or a new suite version with amended rules—outside this Phase 1 path.

### 6.7 Corpus Manifest and paper mapping freeze

Phase 1 SHALL produce a **Corpus Manifest** containing:

- Exact filenames used at selection time  
- Frozen `filename → paper_key` mapping for suite version `b1_v1`  
- Content hashes **if already available** (record `n/a` if absent; Phase 1 SHALL NOT invent a new hashing project)

| Rule | Requirement |
|------|-------------|
| Freeze | Mapping recorded in the Phase 1 artifact is **frozen for `b1_v1`** once membership is owner-frozen |
| Filename drift | Later filesystem renames SHALL NOT silently remap existing `b1_v1` cases |
| Change control | Any mapping change requires a **new suite version** (e.g. `b1_v2`), not an in-place remap of `b1_v1` |

### 6.8 Selection Audit

Phase 1 SHALL produce a **Selection Audit** that explains every inclusion/exclusion path:

| Audit field | Content |
|-------------|---------|
| Total eligible candidates | Count after §6.2 eligibility (before dedup), with staged counts as needed |
| Removed during eligibility filtering | Count + IDs (and/or grouped reasons) |
| Removed during deduplication | Count + IDs (losing duplicate → kept winner) |
| Excluded by lexicographic cutoff | Per cell: eligible-after-dedup IDs **not** taken because only first 2 kept |
| Final selected IDs | The 30 (if no gap) |
| Gap cells | List of `(paper_key, type)` with count &lt; 2, if any |

Purpose: an external reader can see **why** each case was kept or dropped.

### 6.9 Benchmark summary statistics

Alongside the candidate membership table, Phase 1 SHALL record (publication metadata, not pass/fail gates):

- Total eligible cases (post-eligibility; stage clarified in the artifact)  
- Removed by eligibility  
- Removed by deduplication  
- Final selected count (30 or “incomplete—gap”)  
- Per-paper counts (selected)  
- Per-type counts (selected)  

### 6.10 Artifact metadata

Every Phase 1 membership artifact SHALL stand alone and record:

- Protocol version  
- Suite version (`b1_v1`)  
- Selection timestamp  
- Commit hash(es) if available (Thoth / `basic_agent` as applicable)  
- Lifecycle state: **`candidate`** until owner approval; then **`frozen`**

### 6.11 Membership lifecycle

| Stage | Name | Meaning |
|-------|------|---------|
| Phase 1 output | **Candidate b1_v1 Membership** | Algorithm result + audit; **not** yet citable as frozen publication suite content |
| After owner approval | **Frozen b1_v1 Membership** | Immutable under §7; corrections require `b1_v2`+ |

Phase 1 SHALL NOT treat the candidate as frozen. Publication claims SHALL NOT cite candidate membership as `b1_v1` frozen content. Until owner freeze approval: suite **structure** is locked; **IDs are not** frozen; no Phase 2 wiring and no E1 publication run citing `b1_v1` as frozen.

---

## 7. Freeze / versioning rules

After `b1_v1` membership is owner-confirmed (**Frozen b1_v1 Membership**):

| Immutable | Rule |
|-----------|------|
| Case IDs | SHALL NOT change |
| Query text | SHALL NOT change |
| Trajectory text (T cases) | SHALL NOT change |
| Expected files | SHALL NOT change |
| Membership set | SHALL NOT change |

**Corrections** that alter meaning or membership require a new suite version (`b1_v2`, …) with a new freeze date and table. Prior `b1_v1` artifacts remain citable.

---

## 8. Evaluation methodology

### 8.1 Unit of evaluation

One **case** = fixed goal (if any), query, trajectory (if any), expected source file, type.

### 8.2 Arms (observational)

Authoritative runs SHOULD report at least:

- **Configured production retrieval** (system-as-shipped under recorded config, including current trajectory weight if loaded)  
- **RAG baseline** (query-similarity-oriented baseline as defined by the existing research benchmark harness)

Arms are for **reporting**. Neither arm is required to outperform the other.

### 8.3 Reported metrics (always publish; never acceptance gates)

- nDCG@5  
- Precision@5  
- MRR  
- Breakdowns by case type  
- Breakdowns by paper  

Optional observational deltas (e.g. production − RAG) may be reported without pass/fail thresholds.

### 8.4 Top-K

Default reporting depth: **K = 5**, consistent with the research benchmark unless a suite version amends this (new version required).

---

## 9. Authoritative environment (E1)

**Only E1-certified runs** MAY produce publication, Zenodo, or paper claim artifacts for B1.

| Run class | May cite as B1 publication result? |
|-----------|-------------------------------------|
| E1-certified External authoritative | **Yes** |
| Mock / TfIdf / `--sample` / incomplete provenance | **No** |

Authoritative runs SHALL follow [`benchmark_environment.md`](benchmark_environment.md) human checklist (git SHA ≠ `unknown`, honest backend identity, `env_hash` / `index_hash` consistency, tier matches intent).

---

## 10. Publication acceptance criteria

### 10.1 Required integrity (all MUST pass for “publication-valid B1 run”)

1. Suite ID is a frozen version (`b1_v1` after membership freeze).  
2. All 30 cases execute to completion.  
3. Every expected document exists in the corpus used for the run.  
4. No missing corpus entries for declared suite membership.  
5. Provenance completeness per §11.  
6. Deterministic execution under the recorded E1 environment: same suite + same env inputs yield the same scored outputs within any numerical tolerance documented in the run record (or bit-identical if no tolerance stated).  

Failure of a scientific *hypothesis* (e.g. no GOAL lift) does **not** fail integrity.

### 10.2 Reported metrics (MUST be published; MUST NOT be used as beat-the-baseline gates)

- Aggregate and per-type / per-paper nDCG@5, Precision@5, MRR  
- Arm comparison tables as observational reporting only  

---

## 11. Reproducibility requirements

Authoritative B1 artifacts SHALL record, where applicable:

| Field | Requirement |
|-------|-------------|
| `suite_id` | e.g. `b1_v1` |
| Commit hashes | Thoth and `basic_agent` (or equivalent) as exercised |
| `env_hash` / environment fingerprint | Present and consistent with sidecar / event stream |
| `index_hash` | Present after index bind |
| LLM model | If the harness path uses one; else `n/a` |
| Embedding model | Required for External embedding path |
| Timestamp | Run start/end or sealed event time |
| Configuration identity | Retrieval weights / config hash or equivalent as loaded |
| Random seed | If any stochasticity; else explicitly `n/a` (deterministic) |

---

## 12. Publication outputs

- This protocol document  
- **Candidate** then **Frozen** membership package for the suite version (Phase 1+: table + Selection Audit + summary stats + Corpus Manifest + metadata)  
- E1-certified run record (JSONL + summary) with §11 fields  
- Entry in [`completed_improvements_log.md`](completed_improvements_log.md)  
- Claim-facing pointers (results / paper notes) to **`suite_id` + `run_id` + `env_hash`**  

Zenodo V3 packaging is **out of scope** for B1 close-out proper (separate approval).

---

## 13. Sequencing

```
Phase 0  Methodology lock + suite structure          ✅ locked 2026-07-19 (v1.0)
  ↓
STOP — owner review
  ↓
Phase 1  Candidate membership + audit + manifest     ✅ Candidate 2026-07-19 (v1.1)
  ↓
STOP — owner confirms → Frozen b1_v1 Membership
  ↓
Phase 2  Implementation wiring (appendix)            [separate approval]
  ↓
STOP — before authoritative run
  ↓
Phase 3  E1-certified authoritative run              [separate approval]
  ↓
STOP — owner reviews artifacts
  ↓
Phase 4  Publication close-out / claim pointers      [separate approval]
```

**Phase 1 exit (current):** Protocol **v1.1**; **Candidate** `b1_v1` membership package produced; **gaps = 0**; lifecycle remains **`candidate`** until owner freeze approval. **No** Phase 2 implementation; **no** authoritative run citing frozen `b1_v1`.

---

## Informative — Rationale

A separate publication suite prevents tuning churn in the full registry from silently moving paper-facing numbers. Objective selection, Selection Audit, and suite versioning make the 30-case benchmark auditable by an external researcher. E1 authority prevents mock or incomplete runs from entering Zenodo/paper claims. Separating integrity from reported metrics keeps the benchmark honest when retrieval hypotheses fail. Distinguishing Candidate vs Frozen membership prevents premature citation of unapproved suite content.

---

## Informative — Doc map

| Need | File |
|------|------|
| **This protocol** | `B1_PROTOCOL.md` |
| Candidate / Frozen membership | `baselines/b1_v1_membership_candidate.md` (+ `.json`) |
| E1 environment | `benchmark_environment.md` |
| Historical case-design notes | `new_corpus_tests.md` |
| Tracking | `cursor_list.md`, `improvements.md` |
| Decision / run log | `completed_improvements_log.md` |

---

## Appendix A — Implementation notes (non-normative)

These notes do **not** bind science. They may be revised when wiring Phase 2 without a new B1 science version **if** normative behavior is preserved.

| Topic | Note |
|-------|------|
| Case pool source (today) | Research-paper cases live in the benchmark case registry used by the GRAG research harness |
| Phase 1 selection | Deterministic offline enumeration of the registry pool; no harness/CLI required |
| Likely Phase 2 mechanism | Filter or export frozen `b1_v1` IDs; run via existing GRAG benchmark entrypoint with E1 provenance |
| Forbidden in Phase 2 | Changing queries, expected files, scoring, or weights “to improve B1” |
| Full suite | Remain available for development / G1e / regression |

Concrete API names (`BenchmarkCaseRegistry`, CLI flags, etc.) are deliberately omitted from normative sections.

---

*B1 protocol v1.1 — Phase 0 locked 2026-07-19; Phase 1 Candidate membership 2026-07-19. Frozen membership, implementation, and authoritative runs require separate owner approval.*
