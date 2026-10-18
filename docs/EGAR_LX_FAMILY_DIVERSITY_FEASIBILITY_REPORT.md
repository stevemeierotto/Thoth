# EGAR-LX v0.1 — Family diversity and final sample-size feasibility

**Date:** 2026-10-15
**Status:** Planning analysis only. No configuration is approved as the final design. No acquisition or evaluation sample size is frozen.
**Rules used:** Section 6 of `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md`, and the audited calculations in `docs/EGAR_LX_SAMPLE_SIZE_PLANNING_REPORT.md`.
**This report does not modify the approved protocol.**

## 1. Objective and scope

Two questions are examined. First, whether 40–60 historical Linux diagnostic cases across 8–12 problem families look operationally realistic to collect, prepare, and grade. Second, whether more families improve the analytical precision and the eight-family safeguard more than more cases inside the same families.

The answers below are planning comparisons. They are not observed EGAR results, not bootstrap intervals, and not a probability that the experiment will succeed. The fraction of hypothetical rows that meet ±0.5 is a count of assumption combinations. It is not a probability.

## 2. Existing verified findings

From the audited planning report, using only the previously defined Small, Medium, and Large layouts:

| Layout | Families | Pairs | Eligible rows | Rows eligible and with analytical half-width at or under 0.5 |
|---|---:|---:|---:|---:|
| Small | 4 | 20 | 0 of 54 | 0 |
| Medium | 8 | 40 | 45 of 54 | 22 |
| Large | 12 | 60 | 54 of 54 | 35 |

No eligible row at a paired-difference standard deviation of 2.00 in those three layouts has an analytical half-width at or under 0.5. Eight contributing families remain the reporting minimum for a confirmatory interval. M1 below is the same grid as Medium. L1 is the same grid as Large. Both were recalculated with the other candidates so the comparisons share one table. The recalculation matches the earlier eligibility and precision counts.

## 3. New hypothetical configurations

These candidates are not approved and do not replace Small, Medium, or Large.

| ID | Families | Cases per family | Attempted pairs | Arm runs | Relation to earlier layouts |
|---|---:|---:|---:|---:|---|
| M1 | 8 | 5 | 40 | 80 | Same as Medium |
| M2 | 10 | 4 | 40 | 80 | Same pair budget as M1, more families |
| M3 | 12 | 4 | 48 | 96 | Same families as L1, fewer cases each |
| L1 | 12 | 5 | 60 | 120 | Same as Large |
| L2 | 15 | 4 | 60 | 120 | Same pair budget as L1, more families |
| L3 | 20 | 3 | 60 | 120 | Same pair budget as L1, still more families |
| L4 | 16 | 5 | 80 | 160 | More families than L1, same cases per family |

Each case is one Arm A generation and one Arm B generation. Standard deviations are 0.75, 1.25, and 2.00. ICCs are 0.05, 0.15, and 0.30. Combined pair-loss rates are 0%, 10%, and 20%. Placements are the approved round-robin and the approved one-family-at-a-time rule. L = floor(N × p + 0.5). None of these assumptions is treated as true or preferred.

The grid has 7 × 3 × 3 × 3 × 2 = 378 rows.

## 4. Statistical method

The case-weighted mean of paired differences uses the audited variance approximation. Families are independent. Pairs in a family have exchangeable correlation ρ and marginal variance σ².

Var(mean) = (σ² / N²) × Σ_f [n_f + ρ × n_f(n_f − 1)]

SE = sqrt(Var(mean)). The approximate 95% half-width is 1.96 × SE, in combined-score points. The design effect is Var(mean) / (σ² / N). The effective sample size is N divided by the design effect.

This is not the approved 10,000-replicate family-level percentile bootstrap. That interval cannot be computed before scores exist. When every contributing family has size n, the design effect is 1 + (n − 1) ρ. A smaller n at the same N lowers the design effect. Adding cases inside a family raises it.

Quality checks on all 378 rows passed. Each placement assigns exactly L losses and never marks a case twice. Zero-loss spread and concentrated rows match. Higher SD never reduced the half-width when other inputs were fixed. Higher ICC never reduced the variance when family sizes and SD were fixed. Every effective sample size was positive and no larger than the contributing-pair count. Eligibility is contributing families ≥ 8. Half-widths are in combined-score points.

## 5. Complete scenario results

Summary counts:

| ID | Rows | Eligible | Eligible and half-width at or under 0.5 | Of those, at SD 0.75 | At SD 1.25 | At SD 2.00 |
|---|---:|---:|---:|---|---|---|
| M1 | 54 | 45 | 22 | 15 of 15 | 7 of 15 | 0 of 15 |
| M2 | 54 | 54 | 28 | 18 of 18 | 10 of 18 | 0 of 18 |
| M3 | 54 | 54 | 32 | 18 of 18 | 14 of 18 | 0 of 18 |
| L1 | 54 | 54 | 35 | 18 of 18 | 17 of 18 | 0 of 18 |
| L2 | 54 | 54 | 36 | 18 of 18 | 18 of 18 | 0 of 18 |
| L3 | 54 | 54 | 36 | 18 of 18 | 18 of 18 | 0 of 18 |
| L4 | 54 | 54 | 38 | 18 of 18 | 18 of 18 | 2 of 18 |

M1 misses eligibility only when 20% of pairs are lost under the concentrated rule: F01 is exhausted and seven families remain. Every other candidate stays eligible in all 54 rows. The hardest concentrated 20% cases leave 8 families for M2, 10 for M3, 10 for L1, 12 for L2, 16 for L3, and 13 for L4.

The only rows in this entire grid that meet the analytical ±0.5 comparison at SD 2.00 are the two L4 rows with 0% loss and ICC 0.05 (the spread and concentrated placements are identical). The half-width there is 0.480. At ICC 0.15 the same layout is about 0.554, and at ICC 0.30 it is about 0.650. Loss makes those wider. L4 does not cover the high-SD scenarios as a set.

The full 378-row table is at the end of this report.

## 6. Same-budget family-diversity comparisons

Half-widths below are at 0% loss, where family sizes are equal, so the spread and concentrated rows agree. Because the half-width scales with σ, the percentage change between two layouts at the same SD does not depend on which SD is chosen. Figures use SD 1.25 so they sit near the ±0.5 line.

### 8×5 versus 10×4 at 40 pairs (M1 versus M2)

| ICC | M1 half-width | M2 half-width | M2 relative to M1 |
|---:|---:|---:|---|
| 0.05 | 0.424 | 0.415 | about 2% narrower |
| 0.15 | 0.490 | 0.466 | about 5% narrower |
| 0.30 | 0.575 | 0.534 | about 7% narrower |

Both use 80 evaluation generations. M2 also stays eligible if 20% of losses are concentrated (8 families remain). M1 does not (7 remain). The precision gain is a few hundredths of a score point. The clearer difference is the safeguard, not the width. At SD 1.25 and ICC 0.30, M2 is still over 0.5 (0.534). More families at this budget do not bring the high-ICC moderate-SD case under the planning line.

### 12×5 versus 15×4 versus 20×3 at 60 pairs (L1, L2, L3)

| ICC | L1 (12×5) | L2 (15×4) | L3 (20×3) | L3 relative to L1 |
|---:|---:|---:|---:|---|
| 0.05 | 0.346 | 0.339 | 0.332 | about 4% narrower |
| 0.15 | 0.400 | 0.381 | 0.361 | about 10% narrower |
| 0.30 | 0.469 | 0.436 | 0.400 | about 15% narrower |

All three use 120 generations. At SD 1.25, L2 and L3 meet 0.5 at every ICC, including 0.30. L1 meets it at ICC 0.30 when there is no loss (0.469) and misses it in the concentrated 20% row (about 0.520 in the earlier Large audit). The largest same-budget gain in this set is L3 versus L1 at ICC 0.30: about 0.07 score points on the half-width. That is real and smaller than the jump from SD 1.25 to SD 2.00, which multiplies every half-width by 1.60. At SD 2.00, L3's best half-width is still about 0.531.

### 12×4 versus 12×5 (M3 versus L1)

Same 12 families. Pairs rise from 48 to 60, and generations from 96 to 120.

| ICC | M3 half-width | L1 half-width | L1 relative to M3 |
|---:|---:|---:|---|
| 0.05 | 0.379 | 0.346 | about 9% narrower |
| 0.15 | 0.426 | 0.400 | about 6% narrower |
| 0.30 | 0.487 | 0.469 | about 4% narrower |

Adding one case per family costs 12 pairs and 24 generations. At high ICC most of that extra information is absorbed by the higher design effect, 1.90 versus 2.20. The half-width moves by a few hundredths. Both layouts stay eligible under concentrated 20% loss. Extra cases inside a family that is already size 4 are a weak precision lever under these ICCs.

### 12×5 versus 16×5 (L1 versus L4)

Cases per family stay at 5. Families rise from 12 to 16. Pairs rise from 60 to 80. Generations rise from 120 to 160.

| ICC | L1 half-width | L4 half-width | L4 relative to L1 |
|---:|---:|---:|---|
| 0.05 | 0.346 | 0.300 | about 13% narrower |
| 0.15 | 0.400 | 0.346 | about 13% narrower |
| 0.30 | 0.469 | 0.406 | about 13% narrower |

The design effect does not change, so the half-width scales with 1/sqrt(N). The gain is about 0.05 to 0.06 score points at SD 1.25, bought with one third more evaluation runs and four more families to document. L4 is the only candidate here that crosses under 0.5 at SD 2.00, and only when ICC is 0.05 and no pairs are lost.

### What is substantial

Reallocating a fixed pair budget toward more, smaller families narrows the analytical interval by roughly 2% to 15% across the ICCs above, and it adds margin under the eight-family rule. Moving from SD 0.75 to SD 2.00 multiplies the half-width by about 2.67. Family structure is the secondary lever. It is the lever that decides whether Medium-like layouts remain eligible after concentrated loss. It does not make the SD 2.00 scenarios meet ±0.5, except for the single mild L4 corner noted above.

## 7. Eight-family eligibility

| ID | Concentrated 20% contributing families | Eligible rows |
|---|---:|---:|
| M1 | 7 | 45 of 54 |
| M2 | 8 | 54 of 54 |
| M3 | 10 | 54 of 54 |
| L1 | 10 | 54 of 54 |
| L2 | 12 | 54 of 54 |
| L3 | 16 | 54 of 54 |
| L4 | 13 | 54 of 54 |

Eight families is still an operational minimum. A percentile bootstrap with eight to twelve families can be wider and less stable than 1.96 × SE. M2's concentrated 20% case sits exactly on eight families. That meets the reporting rule and remains a thin cluster count.

## 8. Approximate precision

At SD 0.75, every eligible row in every candidate has an analytical half-width under 0.5. At SD 1.25, M1 and M2 miss the higher-ICC and higher-loss rows; M3 misses some high-ICC loss rows; L1 misses one concentrated high-ICC loss row; L2, L3, and L4 meet every SD 1.25 row. At SD 2.00, only L4 at 0% loss and ICC 0.05 meets 0.5, with half-width 0.480. Meeting the line in a row is a planning comparison. It is not an achieved bootstrap width and not a success rule. The +0.5 minimum worthwhile improvement remains a separate planning target.

## 9. Historical case sourcing

No inventory of eligible Linux cases was consulted, and none is claimed. Whether 40–60 evaluation cases across 8–12 families can be assembled is not known. The protocol states that a Linux diagnostic corpus for this experiment does not already exist, and that corpus collection waits on a read-only investigation of how a diagnostic goal writes trajectories, plans, and `success_score`. That investigation has not been done.

Before anyone asserts that a family count is achievable, an inventory would need to record, for each candidate issue, at least the following. This report does not perform that inventory.

- A verified historical Linux kernel or userspace problem, with a documented resolution. A closed ticket or a merged change is not, by itself, evidence the issue was resolved.
- A defensible pre-solution cutoff, so every visible document is on or before the cutoff and the resolution stays sealed.
- A visible bundle with enough diagnostic evidence for a non-trivial external grade, including the environment constraints the remedy rubric uses.
- A sealed record of essential causal elements, kept out of the goal, the corpus, retrieval, and the experience store.
- A near-duplicate screen. Evaluation cases must not repeat an acquisition case, and must not be a near-duplicate or carry the evaluation solution. Cases inside a family still have to be distinct problems.
- A family definition that groups meaningfully related problems and is frozen before results. The definition must not be chosen to fit scores. Relatedness that copies the answer across cases is leakage, not a family.
- A pretraining-familiarity flag. Famous bugs can be flagged and still remain eligible. The flag does not create a supply of obscure cases, and this report does not count how many obscure cases exist.
- Provenance and a license that allows the visible text to be stored and shown to reviewers. Distribution-tracker closure text is sealed-bundle material under the protocol, not visible evidence.
- Two reviewers who can apply the diagnosis and remedy rubrics independently, plus a path for a third reading when they disagree. Reviewer availability is a constraint on the workstation plan, separate from model runtime.

The sourcing tradeoff cuts against the statistical preference for many thin families. More families means more definitions to defend and more distinct resolutions to seal. Fewer cases per family makes it easier to fill one family and easier for one bad source to wipe that family out of the contributing set. Five well-documented cases in one family may be scarce if the family is narrow, and twenty families of three may be scarce if each family must be coherent. Which constraint binds is an empirical question. It is not answered by the half-width table.

Acquisition cases, if they share a family with evaluation cases, consume additional distinct issues in that family. The evaluation counts above are not the whole historical set.

## 10. Acquisition cases

Acquisition stays separate from evaluation. The approved pilot design remains four acquisition cases and four paired evaluation cases. It is not authorized to run, and it does not set the final acquisition count. The final acquisition count is not derived from the evaluation layouts, from +0.5, or from ±0.5.

Arm B's experience has to come from actual Thoth runs on acquisition cases. Hand-written solutions and external grades are not seeded into the snapshot. An evaluation case may share a family with an acquisition case and must not be the same issue or a near-duplicate.

More acquisition cases, and more than one family among them, give the snapshot more than one neighborhood in which repeated trajectories could appear. Whether those rows meet the existing retrieval and promotion thresholds is exactly the investigation the protocol still requires. A larger acquisition set does not guarantee a promoted strategy, a trajectory injection, or a useful experience effect. A small set can still produce a valid Arm B snapshot whose main observation is that injection did not occur. That observation is already an approved descriptive outcome. No acquisition count is recommended here.

Practical considerations before a later choice of that count: how many distinct issues remain after the evaluation set and the near-duplicate screen are reserved; whether several acquisition cases sit in families the evaluation will actually use; disk and time for acquisition runs on the same workstation, in addition to the evaluation runs below; and the still-open question of whether diagnostic goals produce promotable records at all.

## 11. Machine and reviewer workload

Runs are sequential. GPU acceleration and parallel execution are not assumed. The generation budget remains `THOTH_LLM_TIMEOUT_SECONDS` or the 900-second default, so a 30-minute figure is generation plus overhead, and a 5-minute figure can understate a timeout. Hypothetical pair loss does not add runs, because automatic reruns are prohibited. Reviewer minutes are not machine time. The 10,000 bootstrap replicates are not generation runs. Durations and review times are hypothetical, not measured Thoth or reviewer performance.

Sequential machine time:

| ID | Generations | 5 min | 15 min | 30 min |
|---|---:|---:|---:|---:|
| M1, M2 | 80 | 6.7 hours | 20 hours | 40 hours |
| M3 | 96 | 8 hours | 24 hours | 48 hours |
| L1, L2, L3 | 120 | 10 hours | 30 hours | 60 hours |
| L4 | 160 | 13.3 hours | 40 hours | 80 hours |

Two independent reviewers, person-hours summed across both, for every attempted answer:

| ID | Answers | 8 min/review | 15 min/review | 25 min/review |
|---|---:|---:|---:|---:|
| M1, M2 | 80 | 21.3 | 40 | 66.7 |
| M3 | 96 | 25.6 | 48 | 80 |
| L1, L2, L3 | 120 | 32 | 60 | 100 |
| L4 | 160 | 42.7 | 80 | 133.3 |

A third review of a share s of answers, at the same minutes m, adds s × answers × m / 60 person-hours. At 15 minutes and s = 0.25, that is 5.0, 6.0, 7.5, and 10.0 person-hours for 80, 96, 120, and 160 answers. Those shares are not measured disagreement rates.

Evidence preparation and provenance checks are operator time, not model time. At a hypothetical 15 minutes per attempted evaluation case, M1 and M2 take 10 hours, M3 takes 12 hours, L1–L3 take 15 hours, and L4 takes 20 hours. At 30 minutes per case, those figures double. That rate is not measured.

Each evaluation case needs two disposable databases and its logs. The acquisition snapshot is one hashed database, copied per Arm B case and not written during evaluation. Byte sizes are not measured. On a constrained workstation the practical storage question is whether two databases per evaluation case, plus logs, fit beside the development workspace without using C6-LIVE, the development `agent_workspace/`, or any sealed store. That fit is not calculated here.

Calendar time is at least the sequential machine hours if runs are not parallel. Reviewer labor can overlap later runs or can extend the calendar afterward. That overlap is unknown. Two reviewers working at once reduce elapsed review time and do not reduce person-hours. Acquisition runs, if later approved, add further machine time on the same machine and are not in the table.

M2 costs the same evaluation runtime as M1. L2 and L3 cost the same as L1. The extra burden of those reallocations is case finding and family definition, not extra generations. M3 costs 16 more generations than M1. L4 costs 40 more generations than L1.

## 12. Limitations and unresolved questions

- Half-widths are normal approximations. They are not the approved percentile bootstrap. Few families can make that bootstrap unstable even when 1.96 × SE is under 0.5.
- The approximations ignore selection bias from missing and quarantined pairs.
- No SD, ICC, or loss rate is a measurement.
- No count of available historical cases is known. This report does not establish that any layout can be filled.
- Corpus collection still waits on the Section 4 investigation of stored trajectories and promotion. That gate is unchanged.
- Reviewer time, audit time, and per-run minutes are hypothetical.
- Final evaluation size, family count, case allocation, and acquisition count remain unapproved.
- The numerical bootstrap seed and the generator documentation remain unselected.
- The split of combined pair loss into missingness versus confirmed leakage remains unresolved and was not needed for these combined counts.

## 13. Recommendations for the next design decision

Do not freeze a final design from this report.

At a fixed evaluation-pair budget, more and smaller families are the better statistical shape under the approved ICC range and the concentrated-loss rule. M2 dominates M1 on eligibility and is slightly narrower, at the same 80 generations. At 60 pairs, L3 is narrower than L2, which is narrower than L1, especially at ICC 0.30, and all three cost 120 generations. The width differences are modest beside uncertainty about σ. They are large enough to prefer an extra family over a fifth case in a family that already has four, when both options exist: M3 to L1 spends 24 generations to narrow the high-ICC half-width by about 4%.

L4's extra families and cases buy about 13% on the half-width and a small corner of the SD 2.00 grid. They do not make SD 2.00 a solved planning case. They add 40 generations and 20 evaluation cases on a single workstation.

The binding unknown is not another half-width. It is whether distinct, licensable, cutoff-separable issues exist in families of three, four, or five after a separate acquisition pool is reserved. The useful next decision is whether to authorize a documentation-only inventory of that supply. This report does not authorize the inventory, case registration, or a sample size.

## 14. Complete scenario table

Half-widths are analytical planning approximations in combined-score points. Rows with eight-family eligibility "no" are not a basis for a confirmatory interval. M1 and L1 repeat the audited Medium and Large grids.

| Configuration | Families | Cases per family | Attempted pairs | Arm runs | Loss rate | L | Loss model | SD | ICC | Contributing pairs | Contributing families | Family counts after loss | Design effect | ESS | SE (score points) | 95% half-width (score points) | Eight-family eligible | Half-width at or under 0.5 |
|---|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|---|
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 0.75 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.200 | 33.33 | 0.130 | 0.255 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 0.75 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.600 | 25.00 | 0.150 | 0.294 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 0.75 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | 2.200 | 18.18 | 0.176 | 0.345 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 1.25 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.200 | 33.33 | 0.217 | 0.424 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 1.25 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.600 | 25.00 | 0.250 | 0.490 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 1.25 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | 2.200 | 18.18 | 0.293 | 0.575 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 2.00 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.200 | 33.33 | 0.346 | 0.679 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 2.00 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.600 | 25.00 | 0.400 | 0.784 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | spread | 2.00 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | 2.200 | 18.18 | 0.469 | 0.919 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.200 | 33.33 | 0.130 | 0.255 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.600 | 25.00 | 0.150 | 0.294 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | 2.200 | 18.18 | 0.176 | 0.345 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.200 | 33.33 | 0.217 | 0.424 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.600 | 25.00 | 0.250 | 0.490 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | 2.200 | 18.18 | 0.293 | 0.575 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.200 | 33.33 | 0.346 | 0.679 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | 1.600 | 25.00 | 0.400 | 0.784 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | 2.200 | 18.18 | 0.469 | 0.919 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 0.75 | 0.05 | 36 | 8 | 4,4,4,4,5,5,5,5 | 1.178 | 30.57 | 0.136 | 0.266 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 0.75 | 0.15 | 36 | 8 | 4,4,4,4,5,5,5,5 | 1.533 | 23.48 | 0.155 | 0.303 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 0.75 | 0.30 | 36 | 8 | 4,4,4,4,5,5,5,5 | 2.067 | 17.42 | 0.180 | 0.352 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 1.25 | 0.05 | 36 | 8 | 4,4,4,4,5,5,5,5 | 1.178 | 30.57 | 0.226 | 0.443 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 1.25 | 0.15 | 36 | 8 | 4,4,4,4,5,5,5,5 | 1.533 | 23.48 | 0.258 | 0.506 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 1.25 | 0.30 | 36 | 8 | 4,4,4,4,5,5,5,5 | 2.067 | 17.42 | 0.299 | 0.587 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 2.00 | 0.05 | 36 | 8 | 4,4,4,4,5,5,5,5 | 1.178 | 30.57 | 0.362 | 0.709 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 2.00 | 0.15 | 36 | 8 | 4,4,4,4,5,5,5,5 | 1.533 | 23.48 | 0.413 | 0.809 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | spread | 2.00 | 0.30 | 36 | 8 | 4,4,4,4,5,5,5,5 | 2.067 | 17.42 | 0.479 | 0.939 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.05 | 36 | 8 | 1,5,5,5,5,5,5,5 | 1.194 | 30.14 | 0.137 | 0.268 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.15 | 36 | 8 | 1,5,5,5,5,5,5,5 | 1.583 | 22.74 | 0.157 | 0.308 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.30 | 36 | 8 | 1,5,5,5,5,5,5,5 | 2.167 | 16.62 | 0.184 | 0.361 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.05 | 36 | 8 | 1,5,5,5,5,5,5,5 | 1.194 | 30.14 | 0.228 | 0.446 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.15 | 36 | 8 | 1,5,5,5,5,5,5,5 | 1.583 | 22.74 | 0.262 | 0.514 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.30 | 36 | 8 | 1,5,5,5,5,5,5,5 | 2.167 | 16.62 | 0.307 | 0.601 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.05 | 36 | 8 | 1,5,5,5,5,5,5,5 | 1.194 | 30.14 | 0.364 | 0.714 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.15 | 36 | 8 | 1,5,5,5,5,5,5,5 | 1.583 | 22.74 | 0.419 | 0.822 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.30 | 36 | 8 | 1,5,5,5,5,5,5,5 | 2.167 | 16.62 | 0.491 | 0.962 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 0.75 | 0.05 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.150 | 27.83 | 0.142 | 0.279 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 0.75 | 0.15 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.450 | 22.07 | 0.160 | 0.313 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 0.75 | 0.30 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.900 | 16.84 | 0.183 | 0.358 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 1.25 | 0.05 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.150 | 27.83 | 0.237 | 0.464 | yes | yes |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 1.25 | 0.15 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.450 | 22.07 | 0.266 | 0.522 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 1.25 | 0.30 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.900 | 16.84 | 0.305 | 0.597 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 2.00 | 0.05 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.150 | 27.83 | 0.379 | 0.743 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 2.00 | 0.15 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.450 | 22.07 | 0.426 | 0.834 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | spread | 2.00 | 0.30 | 32 | 8 | 4,4,4,4,4,4,4,4 | 1.900 | 16.84 | 0.487 | 0.955 | yes | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.05 | 32 | 7 | 0,2,5,5,5,5,5,5 | 1.191 | 26.88 | 0.145 | 0.284 | no | yes |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.15 | 32 | 7 | 0,2,5,5,5,5,5,5 | 1.572 | 20.36 | 0.166 | 0.326 | no | yes |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.30 | 32 | 7 | 0,2,5,5,5,5,5,5 | 2.144 | 14.93 | 0.194 | 0.380 | no | yes |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.05 | 32 | 7 | 0,2,5,5,5,5,5,5 | 1.191 | 26.88 | 0.241 | 0.473 | no | yes |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.15 | 32 | 7 | 0,2,5,5,5,5,5,5 | 1.572 | 20.36 | 0.277 | 0.543 | no | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.30 | 32 | 7 | 0,2,5,5,5,5,5,5 | 2.144 | 14.93 | 0.324 | 0.634 | no | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.05 | 32 | 7 | 0,2,5,5,5,5,5,5 | 1.191 | 26.88 | 0.386 | 0.756 | no | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.15 | 32 | 7 | 0,2,5,5,5,5,5,5 | 1.572 | 20.36 | 0.443 | 0.869 | no | no |
| M1 | 8 | 5 | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.30 | 32 | 7 | 0,2,5,5,5,5,5,5 | 2.144 | 14.93 | 0.518 | 1.015 | no | no |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 0.75 | 0.05 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.150 | 34.78 | 0.127 | 0.249 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 0.75 | 0.15 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.450 | 27.59 | 0.143 | 0.280 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 0.75 | 0.30 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.900 | 21.05 | 0.163 | 0.320 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 1.25 | 0.05 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.150 | 34.78 | 0.212 | 0.415 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 1.25 | 0.15 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.450 | 27.59 | 0.238 | 0.466 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 1.25 | 0.30 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.900 | 21.05 | 0.272 | 0.534 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 2.00 | 0.05 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.150 | 34.78 | 0.339 | 0.665 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 2.00 | 0.15 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.450 | 27.59 | 0.381 | 0.746 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | spread | 2.00 | 0.30 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.900 | 21.05 | 0.436 | 0.854 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.05 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.150 | 34.78 | 0.127 | 0.249 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.15 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.450 | 27.59 | 0.143 | 0.280 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.30 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.900 | 21.05 | 0.163 | 0.320 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.05 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.150 | 34.78 | 0.212 | 0.415 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.15 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.450 | 27.59 | 0.238 | 0.466 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.30 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.900 | 21.05 | 0.272 | 0.534 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.05 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.150 | 34.78 | 0.339 | 0.665 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.15 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.450 | 27.59 | 0.381 | 0.746 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.30 | 40 | 10 | 4,4,4,4,4,4,4,4,4,4 | 1.900 | 21.05 | 0.436 | 0.854 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 0.75 | 0.05 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.133 | 31.76 | 0.133 | 0.261 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 0.75 | 0.15 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.400 | 25.71 | 0.148 | 0.290 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 0.75 | 0.30 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.800 | 20.00 | 0.168 | 0.329 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 1.25 | 0.05 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.133 | 31.76 | 0.222 | 0.435 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 1.25 | 0.15 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.400 | 25.71 | 0.247 | 0.483 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 1.25 | 0.30 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.800 | 20.00 | 0.280 | 0.548 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 2.00 | 0.05 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.133 | 31.76 | 0.355 | 0.696 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 2.00 | 0.15 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.400 | 25.71 | 0.394 | 0.773 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | spread | 2.00 | 0.30 | 36 | 10 | 3,3,3,3,4,4,4,4,4,4 | 1.800 | 20.00 | 0.447 | 0.877 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.05 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.150 | 31.30 | 0.134 | 0.263 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.15 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.450 | 24.83 | 0.151 | 0.295 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.30 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.900 | 18.95 | 0.172 | 0.338 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.05 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.150 | 31.30 | 0.223 | 0.438 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.15 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.450 | 24.83 | 0.251 | 0.492 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.30 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.900 | 18.95 | 0.287 | 0.563 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.05 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.150 | 31.30 | 0.357 | 0.701 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.15 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.450 | 24.83 | 0.401 | 0.787 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.30 | 36 | 9 | 0,4,4,4,4,4,4,4,4,4 | 1.900 | 18.95 | 0.459 | 0.901 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 0.75 | 0.05 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.112 | 28.76 | 0.140 | 0.274 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 0.75 | 0.15 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.337 | 23.93 | 0.153 | 0.301 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 0.75 | 0.30 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.675 | 19.10 | 0.172 | 0.336 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 1.25 | 0.05 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.112 | 28.76 | 0.233 | 0.457 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 1.25 | 0.15 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.337 | 23.93 | 0.256 | 0.501 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 1.25 | 0.30 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.675 | 19.10 | 0.286 | 0.561 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 2.00 | 0.05 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.112 | 28.76 | 0.373 | 0.731 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 2.00 | 0.15 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.337 | 23.93 | 0.409 | 0.801 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | spread | 2.00 | 0.30 | 32 | 10 | 3,3,3,3,3,3,3,3,4,4 | 1.675 | 19.10 | 0.458 | 0.897 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.05 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.150 | 27.83 | 0.142 | 0.279 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.15 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.450 | 22.07 | 0.160 | 0.313 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.30 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.900 | 16.84 | 0.183 | 0.358 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.05 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.150 | 27.83 | 0.237 | 0.464 | yes | yes |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.15 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.450 | 22.07 | 0.266 | 0.522 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.30 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.900 | 16.84 | 0.305 | 0.597 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.05 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.150 | 27.83 | 0.379 | 0.743 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.15 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.450 | 22.07 | 0.426 | 0.834 | yes | no |
| M2 | 10 | 4 | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.30 | 32 | 8 | 0,0,4,4,4,4,4,4,4,4 | 1.900 | 16.84 | 0.487 | 0.955 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 0.75 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.116 | 0.228 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 0.75 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.130 | 0.255 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 0.75 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.149 | 0.292 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 1.25 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.193 | 0.379 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 1.25 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.217 | 0.426 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 1.25 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.249 | 0.487 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 2.00 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.310 | 0.607 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 2.00 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.348 | 0.681 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | spread | 2.00 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.398 | 0.780 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 0.75 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.116 | 0.228 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 0.75 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.130 | 0.255 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 0.75 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.149 | 0.292 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 1.25 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.193 | 0.379 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 1.25 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.217 | 0.426 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 1.25 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.249 | 0.487 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 2.00 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.310 | 0.607 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 2.00 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.348 | 0.681 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 0% | 0 | concentrated | 2.00 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.398 | 0.780 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 0.75 | 0.05 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.133 | 37.97 | 0.122 | 0.239 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 0.75 | 0.15 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.398 | 30.77 | 0.135 | 0.265 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 0.75 | 0.30 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.795 | 23.95 | 0.153 | 0.300 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 1.25 | 0.05 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.133 | 37.97 | 0.203 | 0.398 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 1.25 | 0.15 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.398 | 30.77 | 0.225 | 0.442 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 1.25 | 0.30 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.795 | 23.95 | 0.255 | 0.501 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 2.00 | 0.05 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.133 | 37.97 | 0.325 | 0.636 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 2.00 | 0.15 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.398 | 30.77 | 0.361 | 0.707 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | spread | 2.00 | 0.30 | 43 | 12 | 3,3,3,3,3,4,4,4,4,4,4,4 | 1.795 | 23.95 | 0.409 | 0.801 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 0.75 | 0.05 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.147 | 37.51 | 0.122 | 0.240 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 0.75 | 0.15 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.440 | 29.87 | 0.137 | 0.269 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 0.75 | 0.30 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.879 | 22.88 | 0.157 | 0.307 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 1.25 | 0.05 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.147 | 37.51 | 0.204 | 0.400 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 1.25 | 0.15 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.440 | 29.87 | 0.229 | 0.448 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 1.25 | 0.30 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.879 | 22.88 | 0.261 | 0.512 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 2.00 | 0.05 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.147 | 37.51 | 0.327 | 0.640 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 2.00 | 0.15 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.440 | 29.87 | 0.366 | 0.717 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 10% | 5 | concentrated | 2.00 | 0.30 | 43 | 11 | 0,3,4,4,4,4,4,4,4,4,4,4 | 1.879 | 22.88 | 0.418 | 0.819 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 0.75 | 0.05 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.111 | 34.22 | 0.128 | 0.251 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 0.75 | 0.15 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.332 | 28.54 | 0.140 | 0.275 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 0.75 | 0.30 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.663 | 22.85 | 0.157 | 0.308 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 1.25 | 0.05 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.111 | 34.22 | 0.214 | 0.419 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 1.25 | 0.15 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.332 | 28.54 | 0.234 | 0.459 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 1.25 | 0.30 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.663 | 22.85 | 0.262 | 0.513 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 2.00 | 0.05 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.111 | 34.22 | 0.342 | 0.670 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 2.00 | 0.15 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.332 | 28.54 | 0.374 | 0.734 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | spread | 2.00 | 0.30 | 38 | 12 | 3,3,3,3,3,3,3,3,3,3,4,4 | 1.663 | 22.85 | 0.418 | 0.820 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 0.75 | 0.05 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.145 | 33.20 | 0.130 | 0.255 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 0.75 | 0.15 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.434 | 26.50 | 0.146 | 0.286 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 0.75 | 0.30 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.868 | 20.34 | 0.166 | 0.326 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 1.25 | 0.05 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.145 | 33.20 | 0.217 | 0.425 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 1.25 | 0.15 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.434 | 26.50 | 0.243 | 0.476 | yes | yes |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 1.25 | 0.30 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.868 | 20.34 | 0.277 | 0.543 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 2.00 | 0.05 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.145 | 33.20 | 0.347 | 0.680 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 2.00 | 0.15 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.434 | 26.50 | 0.389 | 0.762 | yes | no |
| M3 | 12 | 4 | 48 | 96 | 20% | 10 | concentrated | 2.00 | 0.30 | 38 | 10 | 0,0,2,4,4,4,4,4,4,4,4,4 | 1.868 | 20.34 | 0.443 | 0.869 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 50.00 | 0.106 | 0.208 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 37.50 | 0.122 | 0.240 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 27.27 | 0.144 | 0.281 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 50.00 | 0.177 | 0.346 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 37.50 | 0.204 | 0.400 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 27.27 | 0.239 | 0.469 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 50.00 | 0.283 | 0.554 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 37.50 | 0.327 | 0.640 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 27.27 | 0.383 | 0.751 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 50.00 | 0.106 | 0.208 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 37.50 | 0.122 | 0.240 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 27.27 | 0.144 | 0.281 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 50.00 | 0.177 | 0.346 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 37.50 | 0.204 | 0.400 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 27.27 | 0.239 | 0.469 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 50.00 | 0.283 | 0.554 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 37.50 | 0.327 | 0.640 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 27.27 | 0.383 | 0.751 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.05 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 1.178 | 45.85 | 0.111 | 0.217 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.15 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 1.533 | 35.22 | 0.126 | 0.248 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.30 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 2.067 | 26.13 | 0.147 | 0.288 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.05 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 1.178 | 45.85 | 0.185 | 0.362 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.15 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 1.533 | 35.22 | 0.211 | 0.413 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.30 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 2.067 | 26.13 | 0.245 | 0.479 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.05 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 1.178 | 45.85 | 0.295 | 0.579 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.15 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 1.533 | 35.22 | 0.337 | 0.661 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.30 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | 2.067 | 26.13 | 0.391 | 0.767 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.05 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 1.196 | 45.14 | 0.112 | 0.219 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.15 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 1.589 | 33.99 | 0.129 | 0.252 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.30 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 2.178 | 24.80 | 0.151 | 0.295 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.05 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 1.196 | 45.14 | 0.186 | 0.365 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.15 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 1.589 | 33.99 | 0.214 | 0.420 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.30 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 2.178 | 24.80 | 0.251 | 0.492 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.05 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 1.196 | 45.14 | 0.298 | 0.583 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.15 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 1.589 | 33.99 | 0.343 | 0.672 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.30 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | 2.178 | 24.80 | 0.402 | 0.787 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.116 | 0.228 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.130 | 0.255 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.149 | 0.292 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.193 | 0.379 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.217 | 0.426 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.249 | 0.487 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.310 | 0.607 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.348 | 0.681 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.398 | 0.780 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.05 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 1.194 | 40.21 | 0.118 | 0.232 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.15 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 1.581 | 30.36 | 0.136 | 0.267 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.30 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 2.163 | 22.20 | 0.159 | 0.312 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.05 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 1.194 | 40.21 | 0.197 | 0.386 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.15 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 1.581 | 30.36 | 0.227 | 0.445 | yes | yes |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.30 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 2.162 | 22.20 | 0.265 | 0.520 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.05 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 1.194 | 40.21 | 0.315 | 0.618 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.15 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 1.581 | 30.36 | 0.363 | 0.711 | yes | no |
| L1 | 12 | 5 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.30 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | 2.163 | 22.20 | 0.425 | 0.832 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.05 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 52.17 | 0.104 | 0.204 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.15 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 41.38 | 0.117 | 0.229 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.30 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 31.58 | 0.133 | 0.262 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.05 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 52.17 | 0.173 | 0.339 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.15 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 41.38 | 0.194 | 0.381 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.30 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 31.58 | 0.222 | 0.436 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.05 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 52.17 | 0.277 | 0.543 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.15 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 41.38 | 0.311 | 0.609 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.30 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 31.58 | 0.356 | 0.698 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.05 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 52.17 | 0.104 | 0.204 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.15 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 41.38 | 0.117 | 0.229 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.30 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 31.58 | 0.133 | 0.262 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.05 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 52.17 | 0.173 | 0.339 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.15 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 41.38 | 0.194 | 0.381 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.30 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 31.58 | 0.222 | 0.436 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.05 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 52.17 | 0.277 | 0.543 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.15 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 41.38 | 0.311 | 0.609 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.30 | 60 | 15 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 31.58 | 0.356 | 0.698 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.05 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.133 | 47.65 | 0.109 | 0.213 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.15 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.400 | 38.57 | 0.121 | 0.237 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.30 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.800 | 30.00 | 0.137 | 0.268 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.05 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.133 | 47.65 | 0.181 | 0.355 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.15 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.400 | 38.57 | 0.201 | 0.394 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.30 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.800 | 30.00 | 0.228 | 0.447 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.05 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.133 | 47.65 | 0.290 | 0.568 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.15 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.400 | 38.57 | 0.322 | 0.631 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.30 | 54 | 15 | 3,3,3,3,3,3,4,4,4,4,4,4,4,4,4 | 1.800 | 30.00 | 0.365 | 0.716 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.05 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.146 | 47.11 | 0.109 | 0.214 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.15 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.439 | 37.53 | 0.122 | 0.240 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.30 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.878 | 28.76 | 0.140 | 0.274 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.05 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.146 | 47.11 | 0.182 | 0.357 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.15 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.439 | 37.53 | 0.204 | 0.400 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.30 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.878 | 28.76 | 0.233 | 0.457 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.05 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.146 | 47.11 | 0.291 | 0.571 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.15 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.439 | 37.53 | 0.326 | 0.640 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.30 | 54 | 14 | 0,2,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.878 | 28.76 | 0.373 | 0.731 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.05 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.113 | 43.15 | 0.114 | 0.224 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.15 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.338 | 35.89 | 0.125 | 0.245 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.30 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.675 | 28.66 | 0.140 | 0.275 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.05 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.112 | 43.15 | 0.190 | 0.373 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.15 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.337 | 35.89 | 0.209 | 0.409 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.30 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.675 | 28.66 | 0.234 | 0.458 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.05 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.113 | 43.15 | 0.304 | 0.597 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.15 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.337 | 35.89 | 0.334 | 0.654 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.30 | 48 | 15 | 3,3,3,3,3,3,3,3,3,3,3,3,4,4,4 | 1.675 | 28.66 | 0.374 | 0.732 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.05 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.116 | 0.228 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.15 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.130 | 0.255 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.30 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.149 | 0.292 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.05 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.193 | 0.379 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.15 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.217 | 0.426 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.30 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.249 | 0.487 | yes | yes |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.05 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 41.74 | 0.310 | 0.607 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.15 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 33.10 | 0.348 | 0.681 | yes | no |
| L2 | 15 | 4 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.30 | 48 | 12 | 0,0,0,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 25.26 | 0.398 | 0.780 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.05 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 54.55 | 0.102 | 0.199 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.15 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 46.15 | 0.110 | 0.216 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 0.75 | 0.30 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 37.50 | 0.122 | 0.240 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.05 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 54.55 | 0.169 | 0.332 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.15 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 46.15 | 0.184 | 0.361 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 1.25 | 0.30 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 37.50 | 0.204 | 0.400 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.05 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 54.55 | 0.271 | 0.531 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.15 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 46.15 | 0.294 | 0.577 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | spread | 2.00 | 0.30 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 37.50 | 0.327 | 0.640 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.05 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 54.55 | 0.102 | 0.199 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.15 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 46.15 | 0.110 | 0.216 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.30 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 37.50 | 0.122 | 0.240 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.05 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 54.55 | 0.169 | 0.332 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.15 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 46.15 | 0.184 | 0.361 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.30 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 37.50 | 0.204 | 0.400 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.05 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 54.55 | 0.271 | 0.531 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.15 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 46.15 | 0.294 | 0.577 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.30 | 60 | 20 | 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 37.50 | 0.327 | 0.640 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.05 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.089 | 49.59 | 0.107 | 0.209 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.15 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.267 | 42.63 | 0.115 | 0.225 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 0.75 | 0.30 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.533 | 35.22 | 0.126 | 0.248 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.05 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.089 | 49.59 | 0.178 | 0.348 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.15 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.267 | 42.63 | 0.191 | 0.375 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 1.25 | 0.30 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.533 | 35.22 | 0.211 | 0.413 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.05 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.089 | 49.59 | 0.284 | 0.557 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.15 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.267 | 42.63 | 0.306 | 0.600 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | spread | 2.00 | 0.30 | 54 | 20 | 2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.533 | 35.22 | 0.337 | 0.661 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.05 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 49.09 | 0.107 | 0.210 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.15 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 41.54 | 0.116 | 0.228 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.30 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 33.75 | 0.129 | 0.253 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.05 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 49.09 | 0.178 | 0.350 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.15 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 41.54 | 0.194 | 0.380 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.30 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 33.75 | 0.215 | 0.422 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.05 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 49.09 | 0.285 | 0.559 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.15 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 41.54 | 0.310 | 0.608 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.30 | 54 | 18 | 0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 33.75 | 0.344 | 0.675 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.05 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.075 | 44.65 | 0.112 | 0.220 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.15 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.225 | 39.18 | 0.120 | 0.235 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 0.75 | 0.30 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.450 | 33.10 | 0.130 | 0.255 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.05 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.075 | 44.65 | 0.187 | 0.367 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.15 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.225 | 39.18 | 0.200 | 0.391 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 1.25 | 0.30 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.450 | 33.10 | 0.217 | 0.426 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.05 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.075 | 44.65 | 0.299 | 0.587 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.15 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.225 | 39.18 | 0.320 | 0.626 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | spread | 2.00 | 0.30 | 48 | 20 | 2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3 | 1.450 | 33.10 | 0.348 | 0.681 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.05 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 43.64 | 0.114 | 0.223 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.15 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 36.92 | 0.123 | 0.242 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.30 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 30.00 | 0.137 | 0.268 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.05 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 43.64 | 0.189 | 0.371 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.15 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 36.92 | 0.206 | 0.403 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.30 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 30.00 | 0.228 | 0.447 | yes | yes |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.05 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.100 | 43.64 | 0.303 | 0.593 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.15 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.300 | 36.92 | 0.329 | 0.645 | yes | no |
| L3 | 20 | 3 | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.30 | 48 | 16 | 0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 | 1.600 | 30.00 | 0.365 | 0.716 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 0.75 | 0.05 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 66.67 | 0.092 | 0.180 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 0.75 | 0.15 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 50.00 | 0.106 | 0.208 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 0.75 | 0.30 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 36.36 | 0.124 | 0.244 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 1.25 | 0.05 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 66.67 | 0.153 | 0.300 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 1.25 | 0.15 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 50.00 | 0.177 | 0.346 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 1.25 | 0.30 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 36.36 | 0.207 | 0.406 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 2.00 | 0.05 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 66.67 | 0.245 | 0.480 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 2.00 | 0.15 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 50.00 | 0.283 | 0.554 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | spread | 2.00 | 0.30 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 36.36 | 0.332 | 0.650 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 0.75 | 0.05 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 66.67 | 0.092 | 0.180 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 0.75 | 0.15 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 50.00 | 0.106 | 0.208 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 0.75 | 0.30 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 36.36 | 0.124 | 0.244 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 1.25 | 0.05 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 66.67 | 0.153 | 0.300 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 1.25 | 0.15 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 50.00 | 0.177 | 0.346 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 1.25 | 0.30 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 36.36 | 0.207 | 0.406 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 2.00 | 0.05 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.200 | 66.67 | 0.245 | 0.480 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 2.00 | 0.15 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.600 | 50.00 | 0.283 | 0.554 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 0% | 0 | concentrated | 2.00 | 0.30 | 80 | 16 | 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.200 | 36.36 | 0.332 | 0.650 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 0.75 | 0.05 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 1.178 | 61.13 | 0.096 | 0.188 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 0.75 | 0.15 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 1.533 | 46.96 | 0.109 | 0.215 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 0.75 | 0.30 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 2.067 | 34.84 | 0.127 | 0.249 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 1.25 | 0.05 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 1.178 | 61.13 | 0.160 | 0.313 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 1.25 | 0.15 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 1.533 | 46.96 | 0.182 | 0.358 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 1.25 | 0.30 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 2.067 | 34.84 | 0.212 | 0.415 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 2.00 | 0.05 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 1.178 | 61.13 | 0.256 | 0.501 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 2.00 | 0.15 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 1.533 | 46.96 | 0.292 | 0.572 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | spread | 2.00 | 0.30 | 72 | 16 | 4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5 | 2.067 | 34.84 | 0.339 | 0.664 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 0.75 | 0.05 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.196 | 60.21 | 0.097 | 0.189 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 0.75 | 0.15 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.587 | 45.35 | 0.111 | 0.218 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 0.75 | 0.30 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.175 | 33.10 | 0.130 | 0.255 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 1.25 | 0.05 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.196 | 60.21 | 0.161 | 0.316 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 1.25 | 0.15 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.587 | 45.35 | 0.186 | 0.364 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 1.25 | 0.30 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.175 | 33.10 | 0.217 | 0.426 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 2.00 | 0.05 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.196 | 60.21 | 0.258 | 0.505 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 2.00 | 0.15 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 1.587 | 45.35 | 0.297 | 0.582 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 10% | 8 | concentrated | 2.00 | 0.30 | 72 | 15 | 0,2,5,5,5,5,5,5,5,5,5,5,5,5,5,5 | 2.175 | 33.10 | 0.348 | 0.681 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 0.75 | 0.05 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 55.65 | 0.101 | 0.197 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 0.75 | 0.15 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 44.14 | 0.113 | 0.221 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 0.75 | 0.30 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 33.68 | 0.129 | 0.253 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 1.25 | 0.05 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 55.65 | 0.168 | 0.328 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 1.25 | 0.15 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 44.14 | 0.188 | 0.369 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 1.25 | 0.30 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 33.68 | 0.215 | 0.422 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 2.00 | 0.05 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.150 | 55.65 | 0.268 | 0.525 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 2.00 | 0.15 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.450 | 44.14 | 0.301 | 0.590 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | spread | 2.00 | 0.30 | 64 | 16 | 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 | 1.900 | 33.68 | 0.345 | 0.675 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 0.75 | 0.05 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 1.197 | 53.47 | 0.103 | 0.201 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 0.75 | 0.15 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 1.591 | 40.24 | 0.118 | 0.232 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 0.75 | 0.30 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 2.181 | 29.34 | 0.138 | 0.271 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 1.25 | 0.05 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 1.197 | 53.47 | 0.171 | 0.335 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 1.25 | 0.15 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 1.591 | 40.24 | 0.197 | 0.386 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 1.25 | 0.30 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 2.181 | 29.34 | 0.231 | 0.452 | yes | yes |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 2.00 | 0.05 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 1.197 | 53.47 | 0.274 | 0.536 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 2.00 | 0.15 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 1.591 | 40.24 | 0.315 | 0.618 | yes | no |
| L4 | 16 | 5 | 80 | 160 | 20% | 16 | concentrated | 2.00 | 0.30 | 64 | 13 | 0,0,0,4,5,5,5,5,5,5,5,5,5,5,5,5 | 2.181 | 29.34 | 0.369 | 0.724 | yes | no |
