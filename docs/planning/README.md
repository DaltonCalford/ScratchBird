# ScratchBird Planning Documents

**Last Updated:** November 26, 2025

---

## Active Planning Documents

### Catalog Cleanup (Current Priority)
| Document | Status | Est. Hours | Description |
|----------|--------|------------|-------------|
| [CATALOG_CLEANUP_OVERVIEW.md](CATALOG_CLEANUP_OVERVIEW.md) | ACTIVE | - | Master plan and overview |
| [CATALOG_CLEANUP_PHASE_A_CRUD.md](CATALOG_CLEANUP_PHASE_A_CRUD.md) | NOT STARTED | 48-63 | Complete missing CRUD operations |
| [CATALOG_CLEANUP_PHASE_B_STRUCTURES.md](CATALOG_CLEANUP_PHASE_B_STRUCTURES.md) | NOT STARTED | 31-40 | Add missing structures for Phase 2 |
| [CATALOG_CLEANUP_PHASE_C_PAGES.md](CATALOG_CLEANUP_PHASE_C_PAGES.md) | NOT STARTED | 30-43 | System table page allocation |
| [CATALOG_CLEANUP_PHASE_D_VIRTUAL.md](CATALOG_CLEANUP_PHASE_D_VIRTUAL.md) | NOT STARTED | 30-42 | Virtual catalog infrastructure |

**Total Estimated Effort:** 139-188 hours

### Future Work
| Document | Description |
|----------|-------------|
| [FUTURE_WORK_BLOCKED_ITEMS.md](FUTURE_WORK_BLOCKED_ITEMS.md) | Items blocked by Alpha Phase 2/3 dependencies |
| [LOCAL_SERVER_ARCHITECTURE_PLAN.md](LOCAL_SERVER_ARCHITECTURE_PLAN.md) | Future local server implementation |

---

## Archived Documents

Completed implementation plans have been moved to `archive/`:
- P0/P1/P2/P3 Improvement Plans (100%/100%/100%/70% complete)
- Missing Functions Implementation (153/153 complete)
- TypedValue Enhancements
- CRUD Implementation
- Advanced Grouping Tests
- Data Loaders

See [archive/README.md](archive/README.md) for details.

---

## Implementation Priority

1. **Catalog Cleanup** (Phases A-D) - Prerequisite for Alpha Phase 2
2. **Local Server Architecture** - Required for CLI tools
3. **Future Blocked Items** - After Alpha Phase 2/3 complete

---

## Quick Reference

### Catalog Cleanup Phases
```
Phase A (CRUD)      ──┐
Phase B (Structures) ─┼──► Phase D (Virtual Catalog) ──► Alpha Phase 2
Phase C (Pages)     ──┘
```

### Key Files
- Main catalog: `include/scratchbird/core/catalog_manager.h`
- Catalog indexes: `include/scratchbird/catalog/catalog_index.h`
- Phase 2 specs: `/docs/specifications/Alpha Phase 2/`

---

**Next Action:** Begin implementation of CATALOG_CLEANUP_PHASE_A_CRUD.md
