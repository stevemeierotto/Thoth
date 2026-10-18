# EGAR-LX v0.1 — Statistical sample-size and resource planning report

**Date:** 2026-10-15
**Status:** Planning calculation only. No final acquisition or evaluation sample size is selected or frozen.
**Source of rules:** `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md`, Section 6 and its Experimental Parameter Register, as approved through 2026-10-15.
**This report does not modify that protocol.**

## Audit of the suspicious printed values — 2026-10-15

An intermediate print from the planning session reported lines such as `hw=27.273` and `ess=2.2` for Large layouts. Those figures were mislabeled. They were not written into the scenario table below. An independent recalculation of all 162 rows against that table found no numeric mismatch, no failed monotonicity check, and no disagreement between the 0% spread and concentrated placements.

### What the printed fields actually were

The print walked a tuple whose fields, in order, were configuration, attempted families, attempted pairs, loss rate, L, loss model, SD, ICC, contributing pairs, contributing families, family sizes, SE, half-width, effective sample size, design effect, eligibility, and whether the half-width was at most 0.5.

The print labeled tuple index 13 as `hw` and index 14 as `ess`. Index 13 is the effective sample size. Index 14 is the design effect. The half-width and the standard error were earlier in the tuple and were not printed. The slice that produced the quoted Large lines was restricted to SD 1.25, and that standard deviation was omitted from the printed line. The values are therefore effective sample size and design effect at SD 1.25, not half-widths in score points.

| Quoted line | What `hw=…` actually was | What `ess=…` actually was | Correct half-width at SD 1.25 (score points) | Correct ESS |
|---|---|---|---:|---:|
| Large, 0% concentrated, ICC 0.30, N=60, K=12 | ESS 27.273 | Design effect 2.2 | 0.469 | 27.273 |
| Large, 20% spread, ICC 0.05, N=48, K=12 | ESS 41.739 | Design effect 1.2, from 1.150 | 0.379 | 41.739 |
| Large, 20% spread, ICC 0.30, N=48, K=12 | ESS 25.263 | Design effect 1.9 | 0.487 | 25.263 |
| Large, 20% concentrated, ICC 0.05, N=48, K=10 | ESS 40.209 | Design effect 1.2, from 1.194 | 0.386 | 40.209 |
| Large, 20% concentrated, ICC 0.30, N=48, K=10 | ESS 22.197 | Design effect 2.2, from 2.163 | 0.520 | 22.197 |

No percentage was printed as a half-width. The formula used for the table was not the source of these labels. The root cause is the wrong tuple index in that one print.

### Formula comparison

The audit formula is

Var(mean) = (σ² / N²) × Σ_f [n_f + ρ × n_f(n_f − 1)]

The formula already in Section 2 is

Var(mean) = σ² [ ρ Σ n_f² + (1 − ρ) N ] / N²

They are the same expression. Expanding the audit term gives n_f + ρ n_f² − ρ n_f = (1 − ρ) n_f + ρ n_f², and summing over families gives (1 − ρ) N + ρ Σ n_f². SE = sqrt(Var(mean)). The approximate 95% half-width is 1.96 × SE, in combined-score points. Design effect = Var(mean) / (σ² / N). Effective sample size = N / design effect, which equals σ² / Var(mean). Both definitions were checked on every row.

### Independent examples

| Example | Family counts | N | K | Variance | SE | Design effect | ESS | Half-width |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Medium, SD 0.75, ICC 0.05, 0% loss | eight families of 5 | 40 | 8 | 0.016875 | 0.1299 | 1.200 | 33.33 | 0.2546 |
| Large, SD 0.75, ICC 0.30, 0% loss | twelve families of 5 | 60 | 12 | 0.020625 | 0.1436 | 2.200 | 27.27 | 0.2815 |
| Large, SD 1.25, ICC 0.05, 20% spread | twelve families of 4 | 48 | 12 | 0.037435 | 0.1935 | 1.150 | 41.74 | 0.3792 |
| Large, SD 2.00, ICC 0.30, 20% concentrated | 0, 0, 3, then nine families of 5 | 48 | 10 | 0.180208 | 0.4245 | 2.163 | 22.20 | 0.8320 |
| Medium, SD 1.25, ICC 0.15, 10% spread | 4, 4, 4, 4, 5, 5, 5, 5 | 36 | 8 | 0.066551 | 0.2580 | 1.533 | 23.48 | 0.5056 |

The Medium sanity check matches the previously reported SE 0.1299, half-width 0.2546, ESS 33.33, and design effect 1.200. All five examples match the scenario table after ordinary rounding.

### Invariants checked on all 162 rows

Increasing SD with every other input fixed never decreased SE or half-width. Increasing ICC with the same family sizes and SD never decreased the variance. Every ESS was positive and no greater than the contributing-pair count. Every 0% spread row matched its concentrated twin. Each placement assigned exactly L distinct losses. Families reduced to zero were excluded from the contributing-family count. Eligibility is contributing families ≥ 8.

The scenario table was not rewritten, because the audit did not find a wrong number in it. Sections 3 through 10 use those same checked values. Workload figures were recomputed from two generations per attempted pair and match Section 6.

## 1. Scope and assumptions

The question is whether the three previously approved hypothetical evaluation layouts could reasonably produce a 95% interval half-width of about ±0.5 combined-score points for the case-weighted mean of Arm B minus Arm A, and what evaluation workload those layouts imply.

The layouts are planning examples. Each family starts with five evaluation cases. Each case has one Arm A generation and one Arm B generation.

| Configuration | Families | Attempted pairs | Arm runs |
|---|---:|---:|---:|
| Small | 4 | 20 | 40 |
| Medium | 8 | 40 | 80 |
| Large | 12 | 60 | 120 |

Hypothetical paired-difference standard deviations are 0.75, 1.25, and 2.00. Hypothetical within-family ICCs are 0.05, 0.15, and 0.30. Combined pair-loss rates are 0%, 10%, and 20%. Losses are placed by the approved round-robin spread rule or by the approved one-family-at-a-time concentrated rule. The lost-pair count is L = floor(N × p + 0.5). Family order is ascending illustrative family ID. Within a family, cases are removed in ascending case ID. No scenario is treated as the true, most likely, or preferred assumption.

The +0.5 minimum worthwhile improvement is a separate planning target. It is not the ±0.5 half-width, and it is not a significance gate. No p-value is calculated.

No Arm A score, Arm B score, paired difference, observed ICC, or observed standard deviation is invented. No Monte Carlo simulation and no synthetic outcome is generated. The 10,000 bootstrap replicates in the approved analysis are resamples of completed scores. They are not Thoth generation runs, and they are not executed here.

## 2. Calculation method and formulas

### Loss placement

N is the number of attempted pairs. p is 0.00, 0.10, or 0.20.

L = floor(N × p + 0.5)

For these three layouts that formula gives:

| Layout | N | 0% | 10% | 20% |
|---|---:|---:|---:|---:|
| Small | 20 | 0 | 2 | 4 |
| Medium | 40 | 0 | 4 | 8 |
| Large | 60 | 0 | 6 | 12 |

Spread allocation walks families in ascending ID order and removes one still-present case per visit, then repeats. Concentrated allocation empties F01, then F02, and so on. At 0% loss the two placements are identical. Both are listed, so the table has 162 rows: 3 layouts × 3 standard deviations × 3 ICCs × 3 loss rates × 2 placement methods. The 81 baseline combinations are those factors before the placement split.

### Analytical standard error

This is a normal approximation for clustered paired differences. It is not the approved family-level percentile bootstrap. The approved interval is the 2.5th and 97.5th percentiles of 10,000 family-level replicates. That width cannot be known before the evaluation scores exist.

Model for a contributing pair in family i:

- The paired difference has variance σ².
- Pairs in the same family have correlation ρ.
- Pairs in different families are independent.
- The case-weighted mean weights each remaining pair equally.

If family i retains nᵢ contributing pairs, and N₊ = Σ nᵢ over families that still have at least one,

Var(mean) = σ² [ ρ Σ nᵢ² + (1 − ρ) N₊ ] / N₊²

SE = sqrt(Var(mean))

Approximate 95% half-width = 1.96 × SE

Effective sample size = σ² / Var(mean). That is the number of independent paired differences that would have the same variance. The design effect is N₊ / ESS.

When every contributing family has the same size n and there are K such families, this reduces to

SE = σ × sqrt( [1 + (n − 1) ρ] / N₊ )

Unequal sizes after loss placement use the general formula, so a family that retains one pair and a family that retains five pairs do not contribute the same amount of variance. Families reduced to zero pairs stay in the attempted population for audit purposes and drop out of N₊, K, and the variance sum.

### What the approximation does not include

The half-width is conditional on the pairs that remain. It does not add variance for selection bias from missing or quarantined pairs. The approved protocol already states that the bootstrap does not remove that bias.

Independence between families is an assumption of this variance formula. The approved bootstrap also treats families as the resampling units, but a percentile interval from eight or twelve families can be wider and less stable than 1.96 × SE. Eight families is only an operational reporting minimum.

The normal multiplier 1.96 is a planning convenience. It is not a t interval, and it is not the percentile method.

## 3. Scenario comparison

Full rows are in the table at the end. The distinct family-count patterns are:

| Layout | Loss | Model | L | Counts by family ID | Contributing pairs | Contributing families |
|---|---:|---|---:|---|---:|---:|
| Small | 0% | both | 0 | 5,5,5,5 | 20 | 4 |
| Small | 10% | spread | 2 | 4,4,5,5 | 18 | 4 |
| Small | 10% | concentrated | 2 | 3,5,5,5 | 18 | 4 |
| Small | 20% | spread | 4 | 4,4,4,4 | 16 | 4 |
| Small | 20% | concentrated | 4 | 1,5,5,5 | 16 | 4 |
| Medium | 0% | both | 0 | eight families of 5 | 40 | 8 |
| Medium | 10% | spread | 4 | 4,4,4,4,5,5,5,5 | 36 | 8 |
| Medium | 10% | concentrated | 4 | 1,5,5,5,5,5,5,5 | 36 | 8 |
| Medium | 20% | spread | 8 | eight families of 4 | 32 | 8 |
| Medium | 20% | concentrated | 8 | 0,2,5,5,5,5,5,5 | 32 | 7 |
| Large | 0% | both | 0 | twelve families of 5 | 60 | 12 |
| Large | 10% | spread | 6 | six families of 4, then six of 5 | 54 | 12 |
| Large | 10% | concentrated | 6 | 0,4, then ten families of 5 | 54 | 11 |
| Large | 20% | spread | 12 | twelve families of 4 | 48 | 12 |
| Large | 20% | concentrated | 12 | 0,0,3, then nine families of 5 | 48 | 10 |

Among eligible rows, approximate half-widths span about 0.255 to 0.962 for Medium and about 0.208 to 0.832 for Large. Small's analytical half-widths span about 0.360 to 1.429, and every Small row fails the family minimum.

## 4. Eight-family eligibility

A confirmatory 95% interval requires eight distinct families that each retain at least one complete, scorable, non-quarantined pair.

| Configuration | Scenario rows | Rows with at least eight contributing families |
|---|---:|---:|
| Small | 54 | 0 |
| Medium | 54 | 45 |
| Large | 54 | 54 |

Small has only four families, so it cannot meet the safeguard at any loss rate. Its analytical half-widths are shown only for comparison. A narrow Small half-width does not authorize a confirmatory interval.

Medium fails the safeguard in the nine concentrated 20% rows: F01 is exhausted and F02 retains two pairs, leaving seven contributing families. Spread losses at 20% leave eight families of four, so those nine rows remain eligible. Medium at 0% and 10% remains eligible under both placement methods.

Large remains eligible in every row. The hardest concentrated case, 20% loss, still leaves ten contributing families.

Eight or twelve families is a small number of resampling units. The approved stability check can still flag an interval as numerically unstable when the family minimum is met. A percentile interval with eight clusters is a weak basis for a precise confirmatory claim even when 1.96 × SE is near 0.5.

## 5. Approximate precision relative to ±0.5

“At or under 0.5” below means the analytical half-width is at most 0.5. It is a planning comparison with the ±0.5 objective. It is not an achieved bootstrap width and not a success rule.

| Configuration | Eligible rows | Eligible rows with half-width at or under 0.5 |
|---|---:|---:|
| Small | 0 | 0 |
| Medium | 45 | 22 |
| Large | 54 | 35 |

By assumed standard deviation, counting only rows that meet the eight-family rule:

| Configuration | SD 0.75 | SD 1.25 | SD 2.00 |
|---|---|---|---|
| Medium | 15 of 15 at or under 0.5 | 7 of 15 | 0 of 15 |
| Large | 18 of 18 at or under 0.5 | 17 of 18 | 0 of 18 |

At SD 0.75, every eligible Medium and Large row meets the planning half-width. Medium's eligible half-widths at this SD are about 0.25 to 0.36. Large's are about 0.21 to 0.31.

At SD 1.25, Medium meets the half-width when ICC is 0.05, and also at 0% loss when ICC is 0.15 (half-width about 0.490). It misses when ICC is 0.30, and it misses at 10% or 20% loss when ICC is 0.15 or 0.30. The misses are about 0.51 to 0.60. Large meets every SD 1.25 row except concentrated 20% loss at ICC 0.30, where the half-width is about 0.520.

At SD 2.00, no Medium or Large row meets ±0.5. Eligible Medium half-widths are about 0.68 to 0.96. Large half-widths are about 0.55 to 0.83. Higher ICC widens the interval more than moving from 0% to 20% loss. Concentrating losses widens the analytical interval only slightly relative to spreading the same L, except that Medium's concentrated 20% case also loses confirmatory eligibility.

## 6. Resource and reviewer workload

Acquisition runs are not included. The final acquisition count is unapproved, and it is not derived from these evaluation sizes or from the four-case pilot.

Evaluation runs are two generations per attempted case: 40, 80, or 120. Hypothetical pair loss does not add runs. Automatic reruns and replacements are prohibited, so a failed or timed-out generation is still one attempted run. The shared generation budget is `THOTH_LLM_TIMEOUT_SECONDS` if set, otherwise 900 seconds. A generation that reaches that budget stops near 15 minutes of model time. The 5-minute figure can understate a run that times out. The 30-minute figure cannot be pure generation time under that budget. It is usable only as a hypothetical allowance for generation plus setup or other overhead. Parallel execution is not assumed. If parallelism is later available, machine elapsed time could fall. Reviewer labor would not.

Sequential machine time at the three hypothetical per-run durations:

| Configuration | Runs | 5 min/run | 15 min/run | 30 min/run |
|---|---:|---:|---:|---:|
| Small | 40 | 3.3 hours | 10 hours | 20 hours |
| Medium | 80 | 6.7 hours | 20 hours | 40 hours |
| Large | 120 | 10 hours | 30 hours | 60 hours |

These durations are planning assumptions, not measured Thoth performance.

Reviewer labor uses two independent reviewers for every attempted answer. Attempted answers equal arm runs. Minutes per review are hypothetical and are not observed productivity. Person-hours below are the sum across both reviewers.

| Configuration | Answers | 8 min/review | 15 min/review | 25 min/review |
|---|---:|---:|---:|---:|
| Small | 40 | 10.7 person-hours | 20 person-hours | 33.3 person-hours |
| Medium | 80 | 21.3 person-hours | 40 person-hours | 66.7 person-hours |
| Large | 120 | 32 person-hours | 60 person-hours | 100 person-hours |

A third review is not assumed to happen for every answer. If a hypothetical share of answers needs one extra review at the same minutes, add that share times answers times minutes. At 15 minutes per review, a 10% adjudication share adds 1.0, 2.0, or 3.0 person-hours for Small, Medium, and Large. A 25% share adds 2.5, 5.0, or 7.5 person-hours. A 40% share adds 4.0, 8.0, or 12.0 person-hours. Those shares are not measured disagreement rates.

Evidence preparation and audit are separate from grading. At a hypothetical 15 minutes of operator time per attempted evaluation case, Small, Medium, and Large take 5, 10, and 15 hours. At 30 minutes per case, those figures double. That overhead is not a measured rate.

Calendar time is not the sum of these figures. Sequential machine time is a lower bound on elapsed time if runs are not parallel. Reviewer labor can overlap later runs or can extend the calendar after generation, and that overlap is not known. Two reviewers working at once can shorten elapsed review time without reducing person-hours. Bootstrap computation on finished scores is negligible beside generation and grading, and it is not 10,000 model runs.

## 7. Sensitivity

Standard deviation moves the half-width in proportion to σ. Going from 0.75 to 2.00 multiplies every half-width by 2.00 / 0.75 ≈ 2.67. That is the largest swing in this grid.

ICC changes the design effect. With five cases per family, the equal-size design effect is 1 + (5 − 1) ρ: 1.20 at ICC 0.05, 1.60 at ICC 0.15, and 2.20 at ICC 0.30. Half-widths scale with the square root of the design effect. From ICC 0.05 to ICC 0.30 the equal-size half-width grows by about sqrt(2.20 / 1.20) ≈ 1.35.

Pair loss from 0% to 20% removes 4, 8, or 12 pairs. Under spread placement the half-width increase is modest next to the SD and ICC swings. For equal sizes the SE scales with 1 / sqrt(N₊) once the design effect is fixed, so Medium's drop from 40 to 32 pairs widens the interval by about sqrt(40/32) ≈ 1.12.

Concentration matters most when it deletes a whole family. Medium at 20% concentrated loss falls from eight families to seven and loses confirmatory eligibility even though 32 pairs remain. The analytical half-width in that ineligible case is similar to the spread case with the same L. Large at 20% concentrated loss falls from twelve families to ten and stays eligible. Unequal remaining sizes change the variance only slightly relative to an equal split of the same pairs, because the formula uses Σ nᵢ² rather than assuming equal nᵢ.

## 8. Limitations

- The half-widths are analytical approximations under independence between families and a constant within-family correlation. They are not bootstrap intervals.
- With only eight contributing families, a percentile bootstrap can miss the ±0.5 planning objective even when 1.96 × SE is just under 0.5. The approved stability check can also flag the interval.
- Hypothetical losses are removals for planning. They are not a prediction of which cases will fail, and they do not measure selection bias.
- The combined loss rate is not split into missingness versus confirmed leakage. That split remains unresolved in the protocol and is not needed for this combined-count calculation.
- Illustrative family IDs are used only to apply the approved ordering rules. No real Linux family or case is registered.
- Reviewer minutes, adjudication shares, audit minutes, and the 5/15/30-minute run times are hypothetical. They are not protocol parameters and not measured performance.
- The 900-second generation budget limits how a 30-minute run time can be interpreted.
- No final sample size, family count, or case allocation is implied by a row that meets ±0.5.

## 9. Practical comparison

Small cannot support a confirmatory interval. Some of its analytical half-widths are under 0.5, and that does not override the eight-family rule. Small is not a promising final confirmatory layout under the approved safeguard.

Medium is the smallest example that can meet the safeguard. It does so in 45 of 54 rows. It fails when 20% of pairs are lost under the concentrated rule. It meets the ±0.5 planning comparison in every eligible low-SD row, in the milder moderate-SD rows, and in no high-SD row. Its evaluation workload is 80 generations: about 6.7, 20, or 40 sequential hours at 5, 15, or 30 minutes per run, plus on the order of 21 to 67 reviewer person-hours at the hypothetical review times above.

Large meets the safeguard in all 54 rows, including concentrated 20% loss, which still leaves ten families. It meets the ±0.5 comparison in every low-SD row and in 17 of 18 moderate-SD rows. It meets none of the high-SD rows. The one moderate-SD miss is concentrated 20% loss at ICC 0.30, with an analytical half-width of about 0.52. Its evaluation workload is 120 generations, 1.5 times Medium: about 10, 30, or 60 sequential hours, plus about 32 to 100 reviewer person-hours at the same hypothetical review times.

If the high standard deviation of 2.00 remains a scenario the design should cover, none of these three layouts is enough. A larger layout would be a new proposal. It is not calculated or approved here.

If the operating concern is the low and moderate standard deviations, together with confirmatory eligibility after concentrated loss, Large is the more robust of the examples already on the books. Medium is the lighter example. It covers the low-SD cases and part of the moderate-SD cases, and it loses the confirmatory interval under concentrated 20% loss.

The narrowest interval is not selected. Large's best half-width, about 0.21 at SD 0.75, ICC 0.05, and no loss, is not a reason to freeze Large. Medium's lighter workload is not a reason to freeze Medium. Acquisition size is a separate decision and is not set from these evaluation figures.

## 10. Open decisions

Steve's approval is still required for:

- The final evaluation sample size, family count, and case allocation.
- The final acquisition-case count.
- Whether any layout beyond Small, Medium, and Large should be proposed if the SD 2.00 scenarios are meant to meet ±0.5.
- The numerical bootstrap seed, the random-number generator, and the sampling convention.
- The still-unresolved split of combined pair loss into missingness versus confirmed leakage.
- Actual family names, case registration, and frozen mappings.

No design is frozen by this report.

## Complete scenario table

Half-widths are the analytical approximation in Section 2. Rows labeled “Not eligible for a confirmatory interval” fail the eight-family safeguard. Their half-widths are planning comparisons only.

| Configuration | Attempted pairs | Arm runs | Loss rate | L | Loss model | SD | ICC | Contributing pairs | Contributing families | Family counts after loss | Eight-family eligible | Approx. SE | Approx. 95% half-width | Half-width at or under 0.5 | ESS | Design effect | Planning label |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|---|---:|---:|---|---:|---:|---|
| Small | 20 | 40 | 0% | 0 | spread | 0.75 | 0.05 | 20 | 4 | 5,5,5,5 | no | 0.184 | 0.360 | yes | 16.7 | 1.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | spread | 0.75 | 0.15 | 20 | 4 | 5,5,5,5 | no | 0.212 | 0.416 | yes | 12.5 | 1.60 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | spread | 0.75 | 0.30 | 20 | 4 | 5,5,5,5 | no | 0.249 | 0.488 | yes | 9.1 | 2.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | spread | 1.25 | 0.05 | 20 | 4 | 5,5,5,5 | no | 0.306 | 0.600 | no | 16.7 | 1.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | spread | 1.25 | 0.15 | 20 | 4 | 5,5,5,5 | no | 0.354 | 0.693 | no | 12.5 | 1.60 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | spread | 1.25 | 0.30 | 20 | 4 | 5,5,5,5 | no | 0.415 | 0.813 | no | 9.1 | 2.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | spread | 2.00 | 0.05 | 20 | 4 | 5,5,5,5 | no | 0.490 | 0.960 | no | 16.7 | 1.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | spread | 2.00 | 0.15 | 20 | 4 | 5,5,5,5 | no | 0.566 | 1.109 | no | 12.5 | 1.60 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | spread | 2.00 | 0.30 | 20 | 4 | 5,5,5,5 | no | 0.663 | 1.300 | no | 9.1 | 2.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 0.75 | 0.05 | 20 | 4 | 5,5,5,5 | no | 0.184 | 0.360 | yes | 16.7 | 1.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 0.75 | 0.15 | 20 | 4 | 5,5,5,5 | no | 0.212 | 0.416 | yes | 12.5 | 1.60 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 0.75 | 0.30 | 20 | 4 | 5,5,5,5 | no | 0.249 | 0.488 | yes | 9.1 | 2.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 1.25 | 0.05 | 20 | 4 | 5,5,5,5 | no | 0.306 | 0.600 | no | 16.7 | 1.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 1.25 | 0.15 | 20 | 4 | 5,5,5,5 | no | 0.354 | 0.693 | no | 12.5 | 1.60 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 1.25 | 0.30 | 20 | 4 | 5,5,5,5 | no | 0.415 | 0.813 | no | 9.1 | 2.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 2.00 | 0.05 | 20 | 4 | 5,5,5,5 | no | 0.490 | 0.960 | no | 16.7 | 1.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 2.00 | 0.15 | 20 | 4 | 5,5,5,5 | no | 0.566 | 1.109 | no | 12.5 | 1.60 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 0% | 0 | concentrated | 2.00 | 0.30 | 20 | 4 | 5,5,5,5 | no | 0.663 | 1.300 | no | 9.1 | 2.20 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 0.75 | 0.05 | 18 | 4 | 4,4,5,5 | no | 0.192 | 0.376 | yes | 15.3 | 1.18 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 0.75 | 0.15 | 18 | 4 | 4,4,5,5 | no | 0.219 | 0.429 | yes | 11.7 | 1.53 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 0.75 | 0.30 | 18 | 4 | 4,4,5,5 | no | 0.254 | 0.498 | yes | 8.7 | 2.07 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 1.25 | 0.05 | 18 | 4 | 4,4,5,5 | no | 0.320 | 0.627 | no | 15.3 | 1.18 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 1.25 | 0.15 | 18 | 4 | 4,4,5,5 | no | 0.365 | 0.715 | no | 11.7 | 1.53 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 1.25 | 0.30 | 18 | 4 | 4,4,5,5 | no | 0.424 | 0.830 | no | 8.7 | 2.07 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 2.00 | 0.05 | 18 | 4 | 4,4,5,5 | no | 0.512 | 1.003 | no | 15.3 | 1.18 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 2.00 | 0.15 | 18 | 4 | 4,4,5,5 | no | 0.584 | 1.144 | no | 11.7 | 1.53 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | spread | 2.00 | 0.30 | 18 | 4 | 4,4,5,5 | no | 0.678 | 1.328 | no | 8.7 | 2.07 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 0.75 | 0.05 | 18 | 4 | 3,5,5,5 | no | 0.192 | 0.377 | yes | 15.2 | 1.18 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 0.75 | 0.15 | 18 | 4 | 3,5,5,5 | no | 0.220 | 0.431 | yes | 11.6 | 1.55 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 0.75 | 0.30 | 18 | 4 | 3,5,5,5 | no | 0.256 | 0.502 | no | 8.6 | 2.10 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 1.25 | 0.05 | 18 | 4 | 3,5,5,5 | no | 0.320 | 0.628 | no | 15.2 | 1.18 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 1.25 | 0.15 | 18 | 4 | 3,5,5,5 | no | 0.367 | 0.719 | no | 11.6 | 1.55 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 1.25 | 0.30 | 18 | 4 | 3,5,5,5 | no | 0.427 | 0.837 | no | 8.6 | 2.10 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 2.00 | 0.05 | 18 | 4 | 3,5,5,5 | no | 0.513 | 1.005 | no | 15.2 | 1.18 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 2.00 | 0.15 | 18 | 4 | 3,5,5,5 | no | 0.587 | 1.150 | no | 11.6 | 1.55 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 10% | 2 | concentrated | 2.00 | 0.30 | 18 | 4 | 3,5,5,5 | no | 0.683 | 1.339 | no | 8.6 | 2.10 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 0.75 | 0.05 | 16 | 4 | 4,4,4,4 | no | 0.201 | 0.394 | yes | 13.9 | 1.15 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 0.75 | 0.15 | 16 | 4 | 4,4,4,4 | no | 0.226 | 0.443 | yes | 11.0 | 1.45 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 0.75 | 0.30 | 16 | 4 | 4,4,4,4 | no | 0.258 | 0.507 | no | 8.4 | 1.90 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 1.25 | 0.05 | 16 | 4 | 4,4,4,4 | no | 0.335 | 0.657 | no | 13.9 | 1.15 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 1.25 | 0.15 | 16 | 4 | 4,4,4,4 | no | 0.376 | 0.738 | no | 11.0 | 1.45 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 1.25 | 0.30 | 16 | 4 | 4,4,4,4 | no | 0.431 | 0.844 | no | 8.4 | 1.90 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 2.00 | 0.05 | 16 | 4 | 4,4,4,4 | no | 0.536 | 1.051 | no | 13.9 | 1.15 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 2.00 | 0.15 | 16 | 4 | 4,4,4,4 | no | 0.602 | 1.180 | no | 11.0 | 1.45 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | spread | 2.00 | 0.30 | 16 | 4 | 4,4,4,4 | no | 0.689 | 1.351 | no | 8.4 | 1.90 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 0.75 | 0.05 | 16 | 4 | 1,5,5,5 | no | 0.204 | 0.400 | yes | 13.5 | 1.19 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 0.75 | 0.15 | 16 | 4 | 1,5,5,5 | no | 0.234 | 0.459 | yes | 10.2 | 1.56 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 0.75 | 0.30 | 16 | 4 | 1,5,5,5 | no | 0.273 | 0.536 | no | 7.5 | 2.12 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 1.25 | 0.05 | 16 | 4 | 1,5,5,5 | no | 0.341 | 0.667 | no | 13.5 | 1.19 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 1.25 | 0.15 | 16 | 4 | 1,5,5,5 | no | 0.391 | 0.766 | no | 10.2 | 1.56 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 1.25 | 0.30 | 16 | 4 | 1,5,5,5 | no | 0.456 | 0.893 | no | 7.5 | 2.12 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 2.00 | 0.05 | 16 | 4 | 1,5,5,5 | no | 0.545 | 1.068 | no | 13.5 | 1.19 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 2.00 | 0.15 | 16 | 4 | 1,5,5,5 | no | 0.625 | 1.225 | no | 10.2 | 1.56 | Not eligible for a confirmatory interval |
| Small | 20 | 40 | 20% | 4 | concentrated | 2.00 | 0.30 | 16 | 4 | 1,5,5,5 | no | 0.729 | 1.429 | no | 7.5 | 2.12 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 0% | 0 | spread | 0.75 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.130 | 0.255 | yes | 33.3 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | spread | 0.75 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.150 | 0.294 | yes | 25.0 | 1.60 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | spread | 0.75 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.176 | 0.345 | yes | 18.2 | 2.20 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | spread | 1.25 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.217 | 0.424 | yes | 33.3 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | spread | 1.25 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.250 | 0.490 | yes | 25.0 | 1.60 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | spread | 1.25 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.293 | 0.575 | no | 18.2 | 2.20 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 0% | 0 | spread | 2.00 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.346 | 0.679 | no | 33.3 | 1.20 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 0% | 0 | spread | 2.00 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.400 | 0.784 | no | 25.0 | 1.60 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 0% | 0 | spread | 2.00 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.469 | 0.919 | no | 18.2 | 2.20 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.130 | 0.255 | yes | 33.3 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.150 | 0.294 | yes | 25.0 | 1.60 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 0.75 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.176 | 0.345 | yes | 18.2 | 2.20 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.217 | 0.424 | yes | 33.3 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.250 | 0.490 | yes | 25.0 | 1.60 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 1.25 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.293 | 0.575 | no | 18.2 | 2.20 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.05 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.346 | 0.679 | no | 33.3 | 1.20 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.15 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.400 | 0.784 | no | 25.0 | 1.60 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 0% | 0 | concentrated | 2.00 | 0.30 | 40 | 8 | 5,5,5,5,5,5,5,5 | yes | 0.469 | 0.919 | no | 18.2 | 2.20 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 0.75 | 0.05 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.136 | 0.266 | yes | 30.6 | 1.18 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 0.75 | 0.15 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.155 | 0.303 | yes | 23.5 | 1.53 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 0.75 | 0.30 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.180 | 0.352 | yes | 17.4 | 2.07 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 1.25 | 0.05 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.226 | 0.443 | yes | 30.6 | 1.18 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 1.25 | 0.15 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.258 | 0.506 | no | 23.5 | 1.53 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 1.25 | 0.30 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.299 | 0.587 | no | 17.4 | 2.07 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 2.00 | 0.05 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.362 | 0.709 | no | 30.6 | 1.18 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 2.00 | 0.15 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.413 | 0.809 | no | 23.5 | 1.53 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | spread | 2.00 | 0.30 | 36 | 8 | 4,4,4,4,5,5,5,5 | yes | 0.479 | 0.939 | no | 17.4 | 2.07 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.05 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.137 | 0.268 | yes | 30.1 | 1.19 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.15 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.157 | 0.308 | yes | 22.7 | 1.58 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 0.75 | 0.30 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.184 | 0.361 | yes | 16.6 | 2.17 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.05 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.228 | 0.446 | yes | 30.1 | 1.19 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.15 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.262 | 0.514 | no | 22.7 | 1.58 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 1.25 | 0.30 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.307 | 0.601 | no | 16.6 | 2.17 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.05 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.364 | 0.714 | no | 30.1 | 1.19 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.15 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.419 | 0.822 | no | 22.7 | 1.58 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 10% | 4 | concentrated | 2.00 | 0.30 | 36 | 8 | 1,5,5,5,5,5,5,5 | yes | 0.491 | 0.962 | no | 16.6 | 2.17 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 0.75 | 0.05 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.142 | 0.279 | yes | 27.8 | 1.15 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 0.75 | 0.15 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.160 | 0.313 | yes | 22.1 | 1.45 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 0.75 | 0.30 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.183 | 0.358 | yes | 16.8 | 1.90 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 1.25 | 0.05 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.237 | 0.464 | yes | 27.8 | 1.15 | Eligible; approximate half-width at or under 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 1.25 | 0.15 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.266 | 0.522 | no | 22.1 | 1.45 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 1.25 | 0.30 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.305 | 0.597 | no | 16.8 | 1.90 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 2.00 | 0.05 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.379 | 0.743 | no | 27.8 | 1.15 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 2.00 | 0.15 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.426 | 0.834 | no | 22.1 | 1.45 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 20% | 8 | spread | 2.00 | 0.30 | 32 | 8 | 4,4,4,4,4,4,4,4 | yes | 0.487 | 0.955 | no | 16.8 | 1.90 | Eligible; approximate half-width over 0.5 |
| Medium | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.05 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.145 | 0.284 | yes | 26.9 | 1.19 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.15 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.166 | 0.326 | yes | 20.4 | 1.57 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 20% | 8 | concentrated | 0.75 | 0.30 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.194 | 0.380 | yes | 14.9 | 2.14 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.05 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.241 | 0.473 | yes | 26.9 | 1.19 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.15 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.277 | 0.543 | no | 20.4 | 1.57 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 20% | 8 | concentrated | 1.25 | 0.30 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.324 | 0.634 | no | 14.9 | 2.14 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.05 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.386 | 0.756 | no | 26.9 | 1.19 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.15 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.443 | 0.869 | no | 20.4 | 1.57 | Not eligible for a confirmatory interval |
| Medium | 40 | 80 | 20% | 8 | concentrated | 2.00 | 0.30 | 32 | 7 | 0,2,5,5,5,5,5,5 | no | 0.518 | 1.015 | no | 14.9 | 2.14 | Not eligible for a confirmatory interval |
| Large | 60 | 120 | 0% | 0 | spread | 0.75 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.106 | 0.208 | yes | 50.0 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | spread | 0.75 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.122 | 0.240 | yes | 37.5 | 1.60 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | spread | 0.75 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.144 | 0.281 | yes | 27.3 | 2.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | spread | 1.25 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.177 | 0.346 | yes | 50.0 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | spread | 1.25 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.204 | 0.400 | yes | 37.5 | 1.60 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | spread | 1.25 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.239 | 0.469 | yes | 27.3 | 2.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | spread | 2.00 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.283 | 0.554 | no | 50.0 | 1.20 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 0% | 0 | spread | 2.00 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.327 | 0.640 | no | 37.5 | 1.60 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 0% | 0 | spread | 2.00 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.383 | 0.751 | no | 27.3 | 2.20 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.106 | 0.208 | yes | 50.0 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.122 | 0.240 | yes | 37.5 | 1.60 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 0.75 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.144 | 0.281 | yes | 27.3 | 2.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.177 | 0.346 | yes | 50.0 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.204 | 0.400 | yes | 37.5 | 1.60 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 1.25 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.239 | 0.469 | yes | 27.3 | 2.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.05 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.283 | 0.554 | no | 50.0 | 1.20 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.15 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.327 | 0.640 | no | 37.5 | 1.60 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 0% | 0 | concentrated | 2.00 | 0.30 | 60 | 12 | 5,5,5,5,5,5,5,5,5,5,5,5 | yes | 0.383 | 0.751 | no | 27.3 | 2.20 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 0.75 | 0.05 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.111 | 0.217 | yes | 45.8 | 1.18 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 0.75 | 0.15 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.126 | 0.248 | yes | 35.2 | 1.53 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 0.75 | 0.30 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.147 | 0.288 | yes | 26.1 | 2.07 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 1.25 | 0.05 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.185 | 0.362 | yes | 45.8 | 1.18 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 1.25 | 0.15 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.211 | 0.413 | yes | 35.2 | 1.53 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 1.25 | 0.30 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.245 | 0.479 | yes | 26.1 | 2.07 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 2.00 | 0.05 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.295 | 0.579 | no | 45.8 | 1.18 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 2.00 | 0.15 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.337 | 0.661 | no | 35.2 | 1.53 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 10% | 6 | spread | 2.00 | 0.30 | 54 | 12 | 4,4,4,4,4,4,5,5,5,5,5,5 | yes | 0.391 | 0.767 | no | 26.1 | 2.07 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.05 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.112 | 0.219 | yes | 45.1 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.15 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.129 | 0.252 | yes | 34.0 | 1.59 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 0.75 | 0.30 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.151 | 0.295 | yes | 24.8 | 2.18 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.05 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.186 | 0.365 | yes | 45.1 | 1.20 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.15 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.214 | 0.420 | yes | 34.0 | 1.59 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 1.25 | 0.30 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.251 | 0.492 | yes | 24.8 | 2.18 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.05 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.298 | 0.583 | no | 45.1 | 1.20 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.15 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.343 | 0.672 | no | 34.0 | 1.59 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 10% | 6 | concentrated | 2.00 | 0.30 | 54 | 11 | 0,4,5,5,5,5,5,5,5,5,5,5 | yes | 0.402 | 0.787 | no | 24.8 | 2.18 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 0.75 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.116 | 0.228 | yes | 41.7 | 1.15 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 0.75 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.130 | 0.255 | yes | 33.1 | 1.45 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 0.75 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.149 | 0.292 | yes | 25.3 | 1.90 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 1.25 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.193 | 0.379 | yes | 41.7 | 1.15 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 1.25 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.217 | 0.426 | yes | 33.1 | 1.45 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 1.25 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.249 | 0.487 | yes | 25.3 | 1.90 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 2.00 | 0.05 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.310 | 0.607 | no | 41.7 | 1.15 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 2.00 | 0.15 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.348 | 0.681 | no | 33.1 | 1.45 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 20% | 12 | spread | 2.00 | 0.30 | 48 | 12 | 4,4,4,4,4,4,4,4,4,4,4,4 | yes | 0.398 | 0.780 | no | 25.3 | 1.90 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.05 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.118 | 0.232 | yes | 40.2 | 1.19 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.15 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.136 | 0.267 | yes | 30.4 | 1.58 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 0.75 | 0.30 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.159 | 0.312 | yes | 22.2 | 2.16 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.05 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.197 | 0.386 | yes | 40.2 | 1.19 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.15 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.227 | 0.445 | yes | 30.4 | 1.58 | Eligible; approximate half-width at or under 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 1.25 | 0.30 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.265 | 0.520 | no | 22.2 | 2.16 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.05 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.315 | 0.618 | no | 40.2 | 1.19 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.15 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.363 | 0.711 | no | 30.4 | 1.58 | Eligible; approximate half-width over 0.5 |
| Large | 60 | 120 | 20% | 12 | concentrated | 2.00 | 0.30 | 48 | 10 | 0,0,3,5,5,5,5,5,5,5,5,5 | yes | 0.425 | 0.832 | no | 22.2 | 2.16 | Eligible; approximate half-width over 0.5 |
