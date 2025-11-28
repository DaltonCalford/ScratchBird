# ScratchBird Planning Documents

**Last Updated:** November 27, 2025 (Audit Issues Fixed)

---

## Active Planning Documents

### Architecture Reference
| Document | Description |
|----------|-------------|
| [SCHEMA_ARCHITECTURE.md](SCHEMA_ARCHITECTURE.md) | Hierarchical schema namespace design, synonym support |
| [SCHEMA_NAVIGATION_AND_SEARCH_PATH.md](SCHEMA_NAVIGATION_AND_SEARCH_PATH.md) | Navigation commands, search path, system table locations |

### Local Server Architecture 🔄 IN PROGRESS (Phase 1-3 Complete!)
| Document | Status | Est. Hours | Description |
|----------|--------|------------|-------------|
| [LOCAL_SERVER_ARCHITECTURE_PLAN.md](LOCAL_SERVER_ARCHITECTURE_PLAN.md) | 🔄 IN PROGRESS | 30-50 remaining | Local client-server architecture |
| Phase 1: IPC Infrastructure | ✅ COMPLETE | - | Unix sockets, Named pipes, TCP localhost (22 tests) |
| Phase 2: Wire Protocol | ✅ COMPLETE | - | Binary message format, result streaming (37 tests) |
| Phase 3: Server Process | ✅ COMPLETE | - | sb_server daemon, session management |
| Phase 4: Client Library | ❌ Not started | 20-30 | libscratchbird_client, auto-start |
| Phase 5: Integration | ❌ Not started | 10-20 | Security, testing, documentation |

### Catalog Cleanup ✅ ALL PHASES COMPLETE
| Document | Status | Est. Hours | Description |
|----------|--------|------------|-------------|
| [CATALOG_CLEANUP_OVERVIEW.md](CATALOG_CLEANUP_OVERVIEW.md) | ✅ COMPLETE | - | Master plan and overview |
| [CATALOG_CLEANUP_PHASE_A_CRUD.md](CATALOG_CLEANUP_PHASE_A_CRUD.md) | ✅ COMPLETE | 48-63 | Complete missing CRUD operations (37 methods) |
| [CATALOG_CLEANUP_PHASE_B_STRUCTURES.md](CATALOG_CLEANUP_PHASE_B_STRUCTURES.md) | ✅ COMPLETE | 37-48 | Add structures (46 method decls, 11 structs/enums) |
| [CATALOG_CLEANUP_PHASE_C_PAGES.md](CATALOG_CLEANUP_PHASE_C_PAGES.md) | ✅ COMPLETE | 30-43 | System table page allocation (7 new tables) |
| [CATALOG_CLEANUP_PHASE_D_VIRTUAL.md](CATALOG_CLEANUP_PHASE_D_VIRTUAL.md) | ✅ COMPLETE | 38-52 | Virtual catalog + on-demand emulation (~4,290 lines) |

**Total:** ALL PHASES COMPLETE - Ready for Alpha Phase 2

### Future Work
| Document | Description |
|----------|-------------|
| [FUTURE_WORK_BLOCKED_ITEMS.md](FUTURE_WORK_BLOCKED_ITEMS.md) | Items blocked by Alpha Phase 2/3 dependencies |

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

1. ~~**Catalog Cleanup** (Phases A-D)~~ - ✅ COMPLETE
2. ~~**Local Server Phase 1-3** (IPC + Wire Protocol + sb_server)~~ - ✅ COMPLETE (59 tests)
3. 🔄 **Local Server Phase 4-5** (Client Library, Integration) - ~30-50 hours remaining
4. **CLI Tools** (sb_isql, sb_verify, sb_backup, sb_security) - 90-110 hours
5. **Future Blocked Items** - After Alpha Phase 2/3 complete

---

## Quick Reference

### Catalog Cleanup Phases
```
Phase A (CRUD) ✅ COMPLETE ──────┐
Phase B (Structures) ✅ COMPLETE ┼──► Phase D (Virtual Catalog) ✅ COMPLETE ──► Alpha Phase 2
Phase C (Pages) ✅ COMPLETE ─────┘
```

### Key Files
- Main catalog: `include/scratchbird/core/catalog_manager.h`
- Catalog indexes: `include/scratchbird/catalog/catalog_index.h`
- Virtual catalog: `include/scratchbird/catalog/virtual_catalog.h`
- information_schema: `include/scratchbird/catalog/information_schema.h`
- pg_catalog: `include/scratchbird/catalog/pg_catalog.h`
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
SET SCHEMA path;             -- Change to schema (absolute path)
SET SCHEMA UP;               -- Go to parent schema
SET SCHEMA UP.sibling;       -- Go to sibling (relative)
SET SCHEMA .child;           -- Go to child (explicit relative)
SET SCHEMA HOME;             -- Home schema (users.{username})
SET SCHEMA ROOT;             -- Go to root schema
SHOW SCHEMA;                 -- Show current schema name
SHOW SCHEMA PATH;            -- Show full schema path
SHOW TABLES LIKE '%';        -- List tables with filter
SHOW TABLE name IN DETAIL;   -- Show detailed table info
SHOW LOCATION OF object;     -- Find object in search path
SET SEARCH PATH TO '...';    -- Configure search path
```

---

### Key Server Files (New)
- IPC layer: `include/scratchbird/server/ipc_server.h`
- Wire protocol: `include/scratchbird/protocol/wire_protocol.h`
- IPC implementations: `src/server/ipc_unix.cpp`, `ipc_windows.cpp`, `ipc_tcp.cpp`
- Protocol codec: `src/protocol/wire_protocol.cpp`
- Tests: `tests/unit/test_ipc_server.cpp`, `tests/unit/test_wire_protocol.cpp`

---

**Status:** 🔄 Local Server Architecture IN PROGRESS - Phase 1-3 Complete (59 tests passing), Audit Issues Fixed
