# B1 Candidate Membership — `b1_v1`

**Lifecycle:** `candidate` (not frozen)
**Protocol:** B1 v1.1
**Suite version:** `b1_v1`
**Selection timestamp (UTC):** 2026-07-19T17:09:42Z
**Thoth commit:** `86f71bb32cde71f3616e160ba834d9a07d3cba8c`
**basic_agent commit:** `df13c98006d7788cd9388708576ae3caadd36505`

This artifact is the Phase 1 **Candidate** membership package produced by the locked §6.5 algorithm.
It is **not** frozen until the owner explicitly confirms. Do not treat as publication suite membership until lifecycle becomes `frozen`.

Machine-readable companion: [`b1_v1_membership_candidate.json`](b1_v1_membership_candidate.json)

---

## 1. Artifact metadata

| Field | Value |
|-------|-------|
| `lifecycle` | `candidate` |
| `protocol_version` | B1 v1.1 |
| `suite_version` | `b1_v1` |
| `selection_timestamp_utc` | 2026-07-19T17:09:42Z |
| `commit_thoth` | `86f71bb32cde71f3616e160ba834d9a07d3cba8c` |
| `commit_basic_agent` | `df13c98006d7788cd9388708576ae3caadd36505` |
| Gap cells | **0** (selection completed) |

---

## 2. Corpus Manifest

Frozen file → `paper_key` map for `b1_v1` (candidate until membership freeze):

| Filename | paper_key | Exists | SHA-256 |
|----------|-----------|--------|---------|
| `2005.11401v4.txt` | `rag` | True | `96684118e6ff6bff9c9121e2cbb4fe377e690ccdf6429bfe8ea57b8de153e8ed` |
| `2210.03629v3.txt` | `react` | True | `29addded17175e5fb5274c4d63475e6daad3556354eacd784175bbe09c21d435` |
| `2304.03442v2.txt` | `genagents` | True | `230c2f964c97d524ebcdb87ce1555e563f36481cf75824ae34a99b65e8de62cd` |
| `2310.08560v2.txt` | `memgpt` | True | `404bd58f1429320719513ec470e79ef998551444dc3d80b8bd974dd53ffd8d2c` |
| `2201.11903v6.txt` | `cot` | True | `b11dfc1d35fd655c83ef9c8f58658ce672c0aafad7acb4a666a4fc89439c904d` |

Corpus path (informative): `agent_workspace/docs/`

---

## 3. Selection Audit

### 3.1 Counts

| Stage | Count |
|-------|-------|
| Pool parsed (registry) | 100 |
| Removed by eligibility (§6.2) | 20 |
| Eligible after eligibility | 80 |
| Removed by deduplication (§6.3) | 0 |
| Eligible after dedup | 80 |
| Excluded by lexicographic cutoff (not first 2) | 50 |
| **Final selected** | **30** |
| Gap cells | 0 |

### 3.2 Eligibility removals by reason

| Reason | Count |
|--------|-------|
| `type_not_in_ugt` | 20 |

### 3.3 Removed by eligibility (case IDs)

| case_id | type | expected_file | reasons |
|---------|------|---------------|---------|
| `D1` | `DISTRACTOR_NOISE` | `2310.08560v2.txt` | `type_not_in_ugt` |
| `D2` | `DISTRACTOR_NOISE` | `2304.03442v2.txt` | `type_not_in_ugt` |
| `D3` | `DISTRACTOR_NOISE` | `2210.03629v3.txt` | `type_not_in_ugt` |
| `D4` | `DISTRACTOR_NOISE` | `2304.03442v2.txt` | `type_not_in_ugt` |
| `D5` | `DISTRACTOR_NOISE` | `2005.11401v4.txt` | `type_not_in_ugt` |
| `D6` | `DISTRACTOR_NOISE` | `2310.08560v2.txt` | `type_not_in_ugt` |
| `D7` | `DISTRACTOR_NOISE` | `2201.11903v6.txt` | `type_not_in_ugt` |
| `D8` | `DISTRACTOR_NOISE` | `2210.03629v3.txt` | `type_not_in_ugt` |
| `D9` | `DISTRACTOR_NOISE` | `2304.03442v2.txt` | `type_not_in_ugt` |
| `D10` | `DISTRACTOR_NOISE` | `2210.03629v3.txt` | `type_not_in_ugt` |
| `M1` | `MULTI_HOP` | `2304.03442v2.txt` | `type_not_in_ugt` |
| `M2` | `MULTI_HOP` | `2210.03629v3.txt` | `type_not_in_ugt` |
| `M3` | `MULTI_HOP` | `2310.08560v2.txt` | `type_not_in_ugt` |
| `M4` | `MULTI_HOP` | `2005.11401v4.txt` | `type_not_in_ugt` |
| `M5` | `MULTI_HOP` | `2201.11903v6.txt` | `type_not_in_ugt` |
| `M6` | `MULTI_HOP` | `2210.03629v3.txt` | `type_not_in_ugt` |
| `M7` | `MULTI_HOP` | `2304.03442v2.txt` | `type_not_in_ugt` |
| `M8` | `MULTI_HOP` | `2310.08560v2.txt` | `type_not_in_ugt` |
| `M9` | `MULTI_HOP` | `2005.11401v4.txt` | `type_not_in_ugt` |
| `M10` | `MULTI_HOP` | `2201.11903v6.txt` | `type_not_in_ugt` |

### 3.4 Deduplication removals

None (0 duplicates under §6.3).

### 3.5 Lexicographic cutoff (eligible but not selected)

50 cases ranked ≥3rd in their cell after sort. Full list in JSON companion `excluded_by_lexicographic_cutoff`.

| Cell | Cutoff IDs (lex order) |
|------|------------------------|
| `rag/UNAMBIGUOUS` | `U16`, `U6` |
| `rag/GOAL_DISAMBIGUATES` | `G26`, `G5` |
| `rag/TRAJECTORY_DISAMBIGUATES` | `T22`, `T27`, `T3`, `T9` |
| `react/UNAMBIGUOUS` | `U3`, `U8` |
| `react/GOAL_DISAMBIGUATES` | `G18`, `G20`, `G24`, `G28`, `G3`, `G8` |
| `react/TRAJECTORY_DISAMBIGUATES` | `T2`, `T21`, `T26`, `T30`, `T6` |
| `genagents/UNAMBIGUOUS` | `U4`, `U9` |
| `genagents/GOAL_DISAMBIGUATES` | `G2`, `G23`, `G29`, `G4`, `G6`, `G9` |
| `genagents/TRAJECTORY_DISAMBIGUATES` | `T12`, `T14`, `T16`, `T23`, `T29`, `T7` |
| `memgpt/UNAMBIGUOUS` | `U2`, `U7` |
| `memgpt/GOAL_DISAMBIGUATES` | `G16`, `G22`, `G27` |
| `memgpt/TRAJECTORY_DISAMBIGUATES` | `T24`, `T28`, `T4`, `T8` |
| `cot/UNAMBIGUOUS` | `U20`, `U5` |
| `cot/GOAL_DISAMBIGUATES` | `G25`, `G30`, `G7` |
| `cot/TRAJECTORY_DISAMBIGUATES` | `T5` |

### 3.6 Gaps

**None.** All 15 paper×type cells have ≥2 eligible cases after eligibility + dedup.

---

## 4. Candidate Membership Table (30)

| # | case_id | type | paper_key | expected_file | attestation |
|---|---------|------|-----------|---------------|-------------|
| 1 | `U1` | `UNAMBIGUOUS` | `rag` | `2005.11401v4.txt` | `unvalidated_until_b1_run` |
| 2 | `U11` | `UNAMBIGUOUS` | `rag` | `2005.11401v4.txt` | `unvalidated_until_b1_run` |
| 3 | `G15` | `GOAL_DISAMBIGUATES` | `rag` | `2005.11401v4.txt` | `unvalidated_until_b1_run` |
| 4 | `G21` | `GOAL_DISAMBIGUATES` | `rag` | `2005.11401v4.txt` | `unvalidated_until_b1_run` |
| 5 | `T15` | `TRAJECTORY_DISAMBIGUATES` | `rag` | `2005.11401v4.txt` | `e1_attested` |
| 6 | `T20` | `TRAJECTORY_DISAMBIGUATES` | `rag` | `2005.11401v4.txt` | `e1_attested` |
| 7 | `U13` | `UNAMBIGUOUS` | `react` | `2210.03629v3.txt` | `unvalidated_until_b1_run` |
| 8 | `U18` | `UNAMBIGUOUS` | `react` | `2210.03629v3.txt` | `unvalidated_until_b1_run` |
| 9 | `G12` | `GOAL_DISAMBIGUATES` | `react` | `2210.03629v3.txt` | `unvalidated_until_b1_run` |
| 10 | `G14` | `GOAL_DISAMBIGUATES` | `react` | `2210.03629v3.txt` | `unvalidated_until_b1_run` |
| 11 | `T13` | `TRAJECTORY_DISAMBIGUATES` | `react` | `2210.03629v3.txt` | `e1_attested` |
| 12 | `T18` | `TRAJECTORY_DISAMBIGUATES` | `react` | `2210.03629v3.txt` | `e1_attested` |
| 13 | `U14` | `UNAMBIGUOUS` | `genagents` | `2304.03442v2.txt` | `unvalidated_until_b1_run` |
| 14 | `U19` | `UNAMBIGUOUS` | `genagents` | `2304.03442v2.txt` | `unvalidated_until_b1_run` |
| 15 | `G13` | `GOAL_DISAMBIGUATES` | `genagents` | `2304.03442v2.txt` | `unvalidated_until_b1_run` |
| 16 | `G19` | `GOAL_DISAMBIGUATES` | `genagents` | `2304.03442v2.txt` | `unvalidated_until_b1_run` |
| 17 | `T1` | `TRAJECTORY_DISAMBIGUATES` | `genagents` | `2304.03442v2.txt` | `e1_attested` |
| 18 | `T10` | `TRAJECTORY_DISAMBIGUATES` | `genagents` | `2304.03442v2.txt` | `e1_attested` |
| 19 | `U12` | `UNAMBIGUOUS` | `memgpt` | `2310.08560v2.txt` | `unvalidated_until_b1_run` |
| 20 | `U17` | `UNAMBIGUOUS` | `memgpt` | `2310.08560v2.txt` | `unvalidated_until_b1_run` |
| 21 | `G1` | `GOAL_DISAMBIGUATES` | `memgpt` | `2310.08560v2.txt` | `unvalidated_until_b1_run` |
| 22 | `G11` | `GOAL_DISAMBIGUATES` | `memgpt` | `2310.08560v2.txt` | `unvalidated_until_b1_run` |
| 23 | `T11` | `TRAJECTORY_DISAMBIGUATES` | `memgpt` | `2310.08560v2.txt` | `e1_attested` |
| 24 | `T19` | `TRAJECTORY_DISAMBIGUATES` | `memgpt` | `2310.08560v2.txt` | `e1_attested` |
| 25 | `U10` | `UNAMBIGUOUS` | `cot` | `2201.11903v6.txt` | `unvalidated_until_b1_run` |
| 26 | `U15` | `UNAMBIGUOUS` | `cot` | `2201.11903v6.txt` | `unvalidated_until_b1_run` |
| 27 | `G10` | `GOAL_DISAMBIGUATES` | `cot` | `2201.11903v6.txt` | `unvalidated_until_b1_run` |
| 28 | `G17` | `GOAL_DISAMBIGUATES` | `cot` | `2201.11903v6.txt` | `unvalidated_until_b1_run` |
| 29 | `T17` | `TRAJECTORY_DISAMBIGUATES` | `cot` | `2201.11903v6.txt` | `e1_attested` |
| 30 | `T25` | `TRAJECTORY_DISAMBIGUATES` | `cot` | `2201.11903v6.txt` | `e1_attested` |

### 4.1 Grid (paper × type)

| paper_key | U (2) | G (2) | T (2) |
|-----------|-------|-------|-------|
| `rag` | `U1`, `U11` | `G15`, `G21` | `T15`, `T20` |
| `react` | `U13`, `U18` | `G12`, `G14` | `T13`, `T18` |
| `genagents` | `U14`, `U19` | `G13`, `G19` | `T1`, `T10` |
| `memgpt` | `U12`, `U17` | `G1`, `G11` | `T11`, `T19` |
| `cot` | `U10`, `U15` | `G10`, `G17` | `T17`, `T25` |

### 4.2 Attestation summary (informational only)

- `e1_attested`: **10**
- `unvalidated_until_b1_run`: **20**

Attestation does **not** affect selection preference (§6.4).

### 4.3 Case literals (query / trajectory / goal)

#### `U1` (rag / UNAMBIGUOUS)

- **expected_file:** `2005.11401v4.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** Maximum Inner Product Search MIPS for dense vector retrieval

#### `U11` (rag / UNAMBIGUOUS)

- **expected_file:** `2005.11401v4.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** DPR dense passage retriever Wikipedia snippets

#### `G15` (rag / GOAL_DISAMBIGUATES)

- **expected_file:** `2005.11401v4.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Build a high-performance semantic search engine
- **query:** dense vector indices

#### `G21` (rag / GOAL_DISAMBIGUATES)

- **expected_file:** `2005.11401v4.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Implement a RAG-based question answering system
- **query:** retrieval mechanism

#### `T15` (rag / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2005.11401v4.txt`
- **attestation:** `e1_attested`
- **goal:** Improve retrieval accuracy
- **query:** document ranking
- **trajectory:** I am optimizing the DPR (Dense Passage Retriever) component.

#### `T20` (rag / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2005.11401v4.txt`
- **attestation:** `e1_attested`
- **goal:** Manage agent memory
- **query:** tiered storage
- **trajectory:** I'm looking at the non-parametric memory implementation.

#### `U13` (react / UNAMBIGUOUS)

- **expected_file:** `2210.03629v3.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** HotpotQA and StrategyQA experimental results

#### `U18` (react / UNAMBIGUOUS)

- **expected_file:** `2210.03629v3.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** interleaving thoughts and actions for decision making

#### `G12` (react / GOAL_DISAMBIGUATES)

- **expected_file:** `2210.03629v3.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Improve decision making in dynamic environments
- **query:** context management

#### `G14` (react / GOAL_DISAMBIGUATES)

- **expected_file:** `2210.03629v3.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Debug the reasoning trace of an agent
- **query:** agent observation

#### `T13` (react / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2210.03629v3.txt`
- **attestation:** `e1_attested`
- **goal:** Solve reasoning tasks
- **query:** action sequence
- **trajectory:** I just emitted a 'Thought' about the search results.

#### `T18` (react / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2210.03629v3.txt`
- **attestation:** `e1_attested`
- **goal:** Verify reasoning quality
- **query:** rationales
- **trajectory:** I am evaluating the ALFWorld trajectory success.

#### `U14` (genagents / UNAMBIGUOUS)

- **expected_file:** `2304.03442v2.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** Hobbs Cafe and generative agent architecture

#### `U19` (genagents / UNAMBIGUOUS)

- **expected_file:** `2304.03442v2.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** reflection engine for agent generalization

#### `G13` (genagents / GOAL_DISAMBIGUATES)

- **expected_file:** `2304.03442v2.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Track the behavior of thousands of simulated humans
- **query:** agent observation

#### `G19` (genagents / GOAL_DISAMBIGUATES)

- **expected_file:** `2304.03442v2.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Create believable NPC routines in a game
- **query:** daily schedules

#### `T1` (genagents / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2304.03442v2.txt`
- **attestation:** `e1_attested`
- **goal:** Implement autonomous agents
- **query:** planning architecture
- **trajectory:** I just finished reviewing the memory stream mechanism.

#### `T10` (genagents / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2304.03442v2.txt`
- **attestation:** `e1_attested`
- **goal:** Enable complex behaviors
- **query:** acting and behavior
- **trajectory:** The agent just decided to head to Hobbs Cafe for lunch.

#### `U12` (memgpt / UNAMBIGUOUS)

- **expected_file:** `2310.08560v2.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** paging and disk swaps for context management

#### `U17` (memgpt / UNAMBIGUOUS)

- **expected_file:** `2310.08560v2.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** fixed-length context window as physical memory

#### `G1` (memgpt / GOAL_DISAMBIGUATES)

- **expected_file:** `2310.08560v2.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Manage limited context windows in long conversations
- **query:** hierarchical memory system

#### `G11` (memgpt / GOAL_DISAMBIGUATES)

- **expected_file:** `2310.08560v2.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Optimize memory usage for OS-style agents
- **query:** context management

#### `T11` (memgpt / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2310.08560v2.txt`
- **attestation:** `e1_attested`
- **goal:** Scale context handling
- **query:** memory management
- **trajectory:** I have implemented the virtual context manager.

#### `T19` (memgpt / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2310.08560v2.txt`
- **attestation:** `e1_attested`
- **goal:** Manage agent memory
- **query:** tiered storage
- **trajectory:** I just moved a block from main to external storage.

#### `U10` (cot / UNAMBIGUOUS)

- **expected_file:** `2201.11903v6.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** series of intermediate reasoning steps emerge naturally

#### `U15` (cot / UNAMBIGUOUS)

- **expected_file:** `2201.11903v6.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** N/A
- **query:** few-shot prompting reasoning chain exemplars

#### `G10` (cot / GOAL_DISAMBIGUATES)

- **expected_file:** `2201.11903v6.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Explain the series of steps taken to solve a problem
- **query:** reflection and planning

#### `G17` (cot / GOAL_DISAMBIGUATES)

- **expected_file:** `2201.11903v6.txt`
- **attestation:** `unvalidated_until_b1_run`
- **goal:** Solve complex math word problems
- **query:** step-by-step logic

#### `T17` (cot / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2201.11903v6.txt`
- **attestation:** `e1_attested`
- **goal:** Verify reasoning quality
- **query:** rationales
- **trajectory:** I am providing 8-shot examples to the 540B model.

#### `T25` (cot / TRAJECTORY_DISAMBIGUATES)

- **expected_file:** `2201.11903v6.txt`
- **attestation:** `e1_attested`
- **goal:** Study model reasoning
- **query:** chain of thought
- **trajectory:** I am looking at the LaMDA math results.

---

## 5. Summary stats

| Dimension | Distribution |
|-----------|--------------|
| Per paper | `rag`=6, `react`=6, `genagents`=6, `memgpt`=6, `cot`=6 |
| Per type | `UNAMBIGUOUS`=10, `GOAL_DISAMBIGUATES`=10, `TRAJECTORY_DISAMBIGUATES`=10 |
| Structure check | 5×6 = 30; 10 U + 10 G + 10 T |

---

## 6. Freeze gate

**Status:** Candidate only.

Owner action required to freeze:

1. Review this table and Selection Audit.
2. Explicitly confirm freeze (e.g. “freeze b1_v1” / “confirm membership”).
3. After confirmation, lifecycle becomes `frozen` and §7 freeze rules apply.

Until then: no Phase 2 wiring, no E1 publication run under this suite id as frozen.

*Generated by B1 Phase 1 selection — 2026-07-19.*

