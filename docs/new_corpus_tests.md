External Corpus Integration & Test Case Design
I have added five external AI research papers as text files to agent_workspace/docs/. These will become the primary benchmark corpus, replacing the mark down files. Before writing any code, read and understand the papers.

Step 1 — Locate and Inventory the Files (NO CODE YET)
Find the following files in agent_workspace/docs/ and confirm they exist and are readable:

2210.03629.txt — ReAct: Reasoning and Acting in Language Models
2005.11401.txt — RAG: Retrieval-Augmented Generation (Lewis et al.)
2304.03442.txt — Generative Agents: Interactive Simulacra of Human Behavior
2310.08560.txt — MemGPT: Towards LLMs as Operating Systems
2201.11903.txt — Chain-of-Thought Prompting (Wei et al.)

For each file report:

File exists: yes/no
Approximate size
First 3 lines so I can confirm it's the right paper

⏸ Stop here. Wait for confirmation before Step 2.

Step 2 — Read the Papers and Understand the Vocabulary Overlap (NO CODE YET)
Read all five papers in full. Then produce a vocabulary overlap map — terms that appear in multiple papers but mean something different in each context. This is what we will use to write hard disambiguation test cases.
Format:
TermPaper A meaningPaper B meaningPaper C meaning"memory"MemGPT: tiered storage systemGenerative Agents: reflection + retrieval streamRAG: external knowledge store"trajectory"ReAct: action/observation sequenceGRAG: embedding of recent steps...
Find at least 10 such overlapping terms. These are the ammunition for the test cases.
⏸ Stop here. Show me the overlap map and wait for confirmation before Step 3.

Step 3 — Design 30 Test Cases (NO CODE YET)
Using the vocabulary overlap map from Step 2, design 30 test cases — 6 per paper, distributed across the three case types.
Distribution per paper:

2 UNAMBIGUOUS — query is specific enough that only one paper could answer it
2 GOAL_DISAMBIGUATES — query uses a shared term, but the goal context points to one specific paper
2 TRAJECTORY_DISAMBIGUATES — query and goal are still ambiguous, only the trajectory (recent failed attempts or actions) resolves which paper is relevant

Rules for writing good test cases:
UNAMBIGUOUS — use terminology that is unique to one paper:
goal:     "understand how ReAct interleaves reasoning and acting"
query:    "Thought Action Observation loop"
expected: 2210.03629.txt
why:      "Thought/Action/Observation" is ReAct-specific terminology
GOAL_DISAMBIGUATES — use a term that appears in multiple papers, but make the goal point clearly to one:
goal:     "implement a memory system that pages context in and out like an operating system"
query:    "managing memory limits"
expected: 2310.08560.txt  (MemGPT)
why:      "managing memory limits" appears in RAG and Generative Agents too,
          but the goal's OS/paging metaphor points specifically to MemGPT
TRAJECTORY_DISAMBIGUATES — the trajectory describes a recent failure that narrows the context:
goal:     "improve agent reasoning on multi-step tasks"
query:    "handling failed steps"
trajectory: "Attempted to use chain of thought prompting. Model lost track of
             goal after 4 steps. Looking for approaches that ground reasoning
             in external observations."
expected: 2210.03629.txt  (ReAct — grounds reasoning in environment feedback)
why:      Without trajectory, "handling failed steps" could match any paper.
          The trajectory explicitly rules out CoT and points to ReAct.
Present all 30 cases in a table before writing any code:
Case ID | Type | Goal | Query | Trajectory | Expected File | Why this tests disambiguation
⏸ Stop here. Show me the 30 cases and wait for my confirmation before writing any code.

Step 4 — Update the Benchmark Corpus and Case Registry
After I confirm the 30 test cases:

Update the corpus indexer to include agent_workspace/docs/*.txt alongside agent_workspace/rag/docs/*.md
Confirm the hard boundary is still enforced — nothing outside agent_workspace/ is indexed
Replace the existing 15 cases in BenchmarkCaseRegistry with the new 30 cases
Preserve the existing Thoth doc cases if they are still valid — fold them in as additional cases rather than replacing if appropriate

⏸ Stop here. Wait for confirmation before Step 5.

Step 5 — Run the Benchmark

Run the full benchmark against the new corpus and 30 cases
Report results in the standard format broken down by case type AND by paper
For each paper, report: how many of its 6 cases did GRAG win vs RAG?
Flag any case scoring zero in both modes — means the expected chunk isn't in the index
Flag any UNAMBIGUOUS case where GRAG scores lower than RAG

The result we are looking for:

GOAL_DISAMBIGUATES cases should show the largest GRAG advantage — this is the core thesis
UNAMBIGUOUS cases should show no regression — GRAG should be at least as good as RAG
TRAJECTORY_DISAMBIGUATES cases should show modest improvement that grows as the episode history fills up

⏸ Stop after results. Wait for confirmation before any further changes.

Definition of done:

Corpus contains the 5 external papers from agent_workspace/docs/
All 30 expected chunks resolve to files inside agent_workspace/
Zero files from external/ or the live codebase appear anywhere
Benchmark results show meaningful per-paper and per-type breakdowns
grag_benchmark.jsonl contains the full run record
