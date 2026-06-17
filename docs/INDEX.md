# Thoth Documentation Index

**Last Updated:** 2026-06-17  
**Purpose:** Central navigation for all Thoth documentation

---

## 🎯 Start Here

### For Users
1. **[HOWTO.md](HOWTO.md)** — Using `/goal`, goal mode vs chat, confirmations, and what to expect from the agent

### For New Contributors
1. **[GETTING_STARTED.md](GETTING_STARTED.md)** — **START HERE** — Setup instructions and first run
2. **[AGENTS.md](../AGENTS.md)** — **READ THIS FIRST** — Architecture guide, coding conventions, and critical rules
3. **[README.md](README.md)** — High-level system overview and capabilities
4. **[completed_improvements_log.md](completed_improvements_log.md)** — **Authoritative source of truth** for what's actually implemented

### For Active Development
- **[cursor_list.md](cursor_list.md)** — **ALIGNMENT BACKLOG** — Claim vs. reality gaps by priority (start here after time away)
- **[improvements.md](improvements.md)** — **ACTIVE ROADMAP** — Phases 3–4 🔶 partial; Phase 5 optional future expansion; Phases 9–12 complete
- **[AGENTS.md](../AGENTS.md)** — Architecture reference and coding guidelines
- **[TESTING.md](TESTING.md)** — Test structure and how to run tests
- **[plan_reuse_tuning.md](plan_reuse_tuning.md)** — Tunable plan reuse, trajectory, and reflection thresholds

---

## 📚 Core Documentation

### Getting Started & Setup
| Document | Purpose | Status |
|----------|---------|--------|
| **[GETTING_STARTED.md](GETTING_STARTED.md)** | Complete setup guide, build instructions, first run | ✅ Current |
| **[TESTING.md](TESTING.md)** | Unit tests (`ctest`), manual concurrency checks, pipeline tests | ✅ Current |
| **[TEST_SUITE.md](TEST_SUITE.md)** | TC-01–TC-07 repeatable GUI/log pipeline tests | ✅ Current |
| **[HOWTO.md](HOWTO.md)** | End-user guide: `/goal`, goal mode, tool confirmations | ✅ Current |
| **[ui_improvements.md](ui_improvements.md)** | Research console UI status, GRAG troubleshooting, future UI | ✅ Current |

### Architecture & Design
| Document | Purpose | Status |
|----------|---------|--------|
| **[AGENTS.md](../AGENTS.md)** | Main architecture guide, conventions, critical rules, system overview | ✅ Current |
| **[README.md](README.md)** | High-level system overview, capabilities, research | ✅ Current |
| **[GRAG.md](GRAG.md)** | GRAG retrieval architecture specification | ✅ Current |
| **[TOOLS.md](TOOLS.md)** | Tool system specification (v1.0) | ✅ Current |
| **[PLAN.md](PLAN.md)** | ExecutiveController and planning architecture | ✅ Current |
| **[cognate.md](cognate.md)** | Cognate V2 cognitive architecture (thesis reference) | ✅ Current |
| **[memory_summary.md](memory_summary.md)** | Memory layers, strategy promotion, observability synthesis | ✅ Current |
| **[architectural_facts.md](architectural_facts.md)** | Implementation status, audit notes; security in `audit.md` | ✅ Current |

### Roadmaps & Tracking
| Document | Purpose | Status |
|----------|---------|--------|
| **[improvements.md](improvements.md)** | **ACTIVE ROADMAP** — Phases 3–4 partial; Phase 5 future expansion | ✅ Active |
| **[cursor_list.md](cursor_list.md)** | **ALIGNMENT BACKLOG** — Doc/code/test gaps by priority | ✅ Active |
| **[completed_improvements_log.md](completed_improvements_log.md)** | **AUTHORITATIVE** — Completed work log | ✅ Authoritative |
| **[benchmark_results.md](benchmark_results.md)** | Auto-archived GRAG performance metrics | ✅ Auto-updated |
| **[new_corpus_tests.md](new_corpus_tests.md)** | Research-paper corpus benchmark case design | ✅ Active |

### Historical / Reference
| Document | Purpose | Status |
|----------|---------|--------|
| **[old_improvements.md](old_improvements.md)** | Historical roadmap entries | 📜 Archive |
| **[VERIFIED_BASELINE.md](VERIFIED_BASELINE.md)** | Locked behavioral baseline for the cognitive spine | 📜 Reference |
| **[PLAN.md](PLAN.md)** | ExecutiveController design philosophy | 📜 Reference |

**Note:** `next_steps.md` and `about_thoth.md` have been deleted. Their content was merged into `improvements.md` and `README.md` respectively.

---

## 🔍 Quick Reference

### "How do I get started?"
→ **[GETTING_STARTED.md](GETTING_STARTED.md)** — Complete setup guide

### "What's implemented?"
→ **[completed_improvements_log.md](completed_improvements_log.md)** — Authoritative source

### "What should I work on next?"
→ **[cursor_list.md](cursor_list.md)** — Alignment backlog by priority  
→ **[improvements.md](improvements.md)** — Active roadmap (Phases 3–5; see phase table + `completed_improvements_log.md`)

### "How do I run tests?"
→ **[TESTING.md](TESTING.md)** — Unit tests and manual concurrency checks  
→ **[TEST_SUITE.md](TEST_SUITE.md)** — TC-01–TC-07 pipeline tests

### "How do I use /goal and goal mode?"
→ **[HOWTO.md](HOWTO.md)** — User-facing behavior guide

### "How do I add a tool?"
→ **[TOOLS.md](TOOLS.md)** — Tool specification  
→ **[AGENTS.md](../AGENTS.md)** — Tool system section

### "How does GRAG work?"
→ **[GRAG.md](GRAG.md)** — Complete GRAG specification  
→ **[AGENTS.md](../AGENTS.md)** — GRAG overview

### "How does planning work?"
→ **[PLAN.md](PLAN.md)** — ExecutiveController design  
→ **[AGENTS.md](../AGENTS.md)** — ExecutiveController section

### "What are the coding conventions?"
→ **[AGENTS.md](../AGENTS.md)** — Coding conventions section

### "What's the current system status?"
→ **[AGENTS.md](../AGENTS.md)** — Current System Status section  
→ **[architectural_facts.md](architectural_facts.md)** — Implementation audits

---

## 📁 Document Categories

### ✅ Active / Current
- `GETTING_STARTED.md`, `HOWTO.md`, `TESTING.md`, `TEST_SUITE.md` — Setup and verification
- `ui_improvements.md` — Research console UI
- `AGENTS.md` — Main guide (always current)
- `README.md` — System overview (always current)
- `cursor_list.md` — Alignment backlog
- `improvements.md` — Active roadmap
- `completed_improvements_log.md` — Completed work log
- `GRAG.md`, `TOOLS.md`, `PLAN.md`, `cognate.md`, `memory_summary.md` — Architecture specs
- `architectural_facts.md` — Status audits

### 📜 Historical / Archive
- `old_improvements.md` — Historical roadmap
- `VERIFIED_BASELINE.md`, `PLAN.md` — Design baseline and planning architecture reference

### 🗑️ Deleted (content merged)
- ~~`next_steps.md`~~ — Merged into `improvements.md` (Phases 9-12 marked complete)
- ~~`about_thoth.md`~~ — Superseded by `README.md`
- ~~`basic_agent_design_goal.md`~~ — Superseded by `VERIFIED_BASELINE.md` and `PLAN.md`

### 📊 Auto-Generated
- `benchmark_results.md` — Auto-archived from benchmark runs

---

## 🔗 Cross-References

### Architecture Flow
```
AGENTS.md (overview)
  ├── GRAG.md (retrieval details)
  ├── TOOLS.md (tool system)
  ├── PLAN.md (planning/execution)
  ├── cognate.md (Cognate V2 architecture)
  ├── memory_summary.md (memory & learning synthesis)
  └── architectural_facts.md (implementation status)
```

### Development Flow
```
improvements.md (what to do)
  → completed_improvements_log.md (what's done)
  → AGENTS.md (how to do it)
```

---

## 📝 Document Maintenance

### Rules
1. **Single Source of Truth**: `completed_improvements_log.md` is authoritative for completed work
2. **Active Roadmap**: Only `improvements.md` should contain active work items
3. **Status Updates**: Update `AGENTS.md` when major features complete
4. **No Duplication**: Don't create overlapping roadmap files

### When to Update
- **After completing work**: Move entry from `improvements.md` to `completed_improvements_log.md`
- **After major feature**: Update `AGENTS.md` status sections
- **After architecture change**: Update relevant spec (`GRAG.md`, `TOOLS.md`, etc.)

---

## ❓ Questions?

If you can't find what you need:
1. Check `AGENTS.md` — it's the main reference
2. Check `completed_improvements_log.md` — it's the authoritative status
3. Check `architectural_facts.md` — it has implementation audits
4. Review relevant architecture spec (`GRAG.md`, `TOOLS.md`, `PLAN.md`)

**When in doubt, `AGENTS.md` is the starting point.**
