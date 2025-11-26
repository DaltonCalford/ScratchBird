# ScratchBird Planning Documents

**Last Updated:** November 26, 2025

---

## Active Planning Documents

### Architecture Reference
| Document | Description |
|----------|-------------|
| [SCHEMA_ARCHITECTURE.md](SCHEMA_ARCHITECTURE.md) | Hierarchical schema namespace design, synonym support |
| [SCHEMA_NAVIGATION_AND_SEARCH_PATH.md](SCHEMA_NAVIGATION_AND_SEARCH_PATH.md) | **NEW** Navigation commands, search path, system table locations |

### Catalog Cleanup (Current Priority)
| Document | Status | Est. Hours | Description |
|----------|--------|------------|-------------|
| [CATALOG_CLEANUP_OVERVIEW.md](CATALOG_CLEANUP_OVERVIEW.md) | ACTIVE | - | Master plan and overview |
| [CATALOG_CLEANUP_PHASE_A_CRUD.md](CATALOG_CLEANUP_PHASE_A_CRUD.md) | NOT STARTED | 48-63 | Complete missing CRUD operations |
| [CATALOG_CLEANUP_PHASE_B_STRUCTURES.md](CATALOG_CLEANUP_PHASE_B_STRUCTURES.md) | NOT STARTED | 37-48 | Add structures (SchemaType, Synonyms, FDW, UDR) |
| [CATALOG_CLEANUP_PHASE_C_PAGES.md](CATALOG_CLEANUP_PHASE_C_PAGES.md) | NOT STARTED | 30-43 | System table page allocation |
| [CATALOG_CLEANUP_PHASE_D_VIRTUAL.md](CATALOG_CLEANUP_PHASE_D_VIRTUAL.md) | NOT STARTED | 38-52 | Virtual catalog + on-demand emulation |

**Total Estimated Effort:** 153-206 hours

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

### Schema Hierarchy
```
/ (root)
├── sys/                  # System tables
│   ├── catalog/          #   schemas, tables, columns, indexes...
│   ├── security/         #   users, roles, permissions...
│   ├── storage/          #   tablespaces...
│   ├── transactions/     #   active_transactions, locks...
│   ├── config/           #   settings, search_paths...
│   └── monitoring/       #   connections, queries...
├── information_schema/   # SQL standard views
├── users/{username}/     # User home directories
├── remote/
│   ├── scratchbird/      # Remote ScratchBird mounts
│   └── emulated/         # ON-DEMAND emulation
│       ├── firebird/{server}/{db}/RDB$*
│       ├── postgresql/{server}/{db}/pg_catalog/*
│       ├── mysql/{server}/{db}/mysql/*
│       └── mssql/{server}/{db}/sys/*
├── public/               # Default schema
└── temp/                 # Global temporary objects
```

### Navigation Commands
```sql
USE schema_path;          -- Change current schema
CD ..;                    -- Parent schema
CD ~;                     -- Home schema (users.{username})
PWD;                      -- Show current schema
SHOW TABLES LIKE '%';     -- List tables with filter
LOCATE object_name;       -- Find object in search path
SET SEARCH_PATH = '...';  -- Configure search path
```

---

**Next Action:** Begin implementation of CATALOG_CLEANUP_PHASE_A_CRUD.md
