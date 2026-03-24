# Thoth Documentation Index

**Last Updated:** 2026-03-10  
**Purpose:** Central navigation for all Thoth documentation

---

## 🎯 Start Here

### For New Contributors
1. **[GETTING_STARTED.md](GETTING_STARTED.md)** — **START HERE** — Setup instructions and first run
2. **[AGENTS.md](../AGENTS.md)** — **READ THIS FIRST** — Architecture guide, coding conventions, and critical rules
3. **[README.md](README.md)** — High-level system overview and capabilities
4. **[completed_improvements_log.md](completed_improvements_log.md)** — **Authoritative source of truth** for what's actually implemented

### For Active Development
- **[improvements.md](improvements.md)** — **ACTIVE ROADMAP** — Current work items (Phases 3-5 planned, Phases 9-12 complete)
- **[AGENTS.md](../AGENTS.md)** — Architecture reference and coding guidelines
- **[TESTING.md](TESTING.md)** — Test structure and how to run tests

---

## 📚 Core Documentation

### Getting Started & Setup
| Document | Purpose | Status |
|----------|---------|--------|
| **[GETTING_STARTED.md](GETTING_STARTED.md)** | Complete setup guide, build instructions, first run | ✅ Current |
| **[TESTING.md](TESTING.md)** | Test structure, how to run tests, adding new tests | ✅ Current |

### Architecture & Design
| Document | Purpose | Status |
|----------|---------|--------|
| **[AGENTS.md](../AGENTS.md)** | Main architecture guide, conventions, critical rules, system overview | ✅ Current |
| **[README.md](README.md)** | High-level system overview, capabilities, research | ✅ Current |
| **[GRAG.md](GRAG.md)** | GRAG retrieval architecture specification | ✅ Current |
| **[TOOLS.md](TOOLS.md)** | Tool system specification (v1.0) | ✅ Current |
| **[PLAN.md](PLAN.md)** | ExecutiveController and planning architecture | ✅ Current |
| **[architectural_facts.md](architectural_facts.md)** | Implementation status, audits, known issues | ✅ Current |

### Roadmaps & Tracking
| Document | Purpose | Status |
|----------|---------|--------|
| **[improvements.md](improvements.md)** | **ACTIVE ROADMAP** — Current work (Phases 3-5) | ✅ Active |
| **[completed_improvements_log.md](completed_improvements_log.md)** | **AUTHORITATIVE** — Completed work log | ✅ Authoritative |
| **[benchmark_results.md](benchmark_results.md)** | Auto-archived GRAG performance metrics | ✅ Auto-updated |

### Historical / Reference
| Document | Purpose | Status |
|----------|---------|--------|
| **[old_improvements.md](old_improvements.md)** | Historical roadmap entries | 📜 Archive |
| **[basic_agent_design_goal.md](basic_agent_design_goal.md)** | Design philosophy for Basic Agent phase | 📜 Reference |

**Note:** `next_steps.md` and `about_thoth.md` have been deleted. Their content was merged into `improvements.md` and `README.md` respectively.

---

## 🔍 Quick Reference

### "How do I get started?"
→ **[GETTING_STARTED.md](GETTING_STARTED.md)** — Complete setup guide

### "What's implemented?"
→ **[completed_improvements_log.md](completed_improvements_log.md)** — Authoritative source

### "What should I work on next?"
→ **[improvements.md](improvements.md)** — Active roadmap (Phases 3-5)

### "How do I run tests?"
→ **[TESTING.md](TESTING.md)** — Test guide

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
- `GETTING_STARTED.md` — Setup guide (always current)
- `TESTING.md` — Test guide (always current)
- `AGENTS.md` — Main guide (always current)
- `README.md` — System overview (always current)
- `improvements.md` — Active roadmap
- `completed_improvements_log.md` — Completed work log
- `GRAG.md`, `TOOLS.md`, `PLAN.md` — Architecture specs
- `architectural_facts.md` — Status audits

### 📜 Historical / Archive
- `old_improvements.md` — Historical roadmap
- `basic_agent_design_goal.md` — Design philosophy reference

### 🗑️ Deleted (content merged)
- ~~`next_steps.md`~~ — Merged into `improvements.md` (Phases 9-12 marked complete)
- ~~`about_thoth.md`~~ — Superseded by `README.md`

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
