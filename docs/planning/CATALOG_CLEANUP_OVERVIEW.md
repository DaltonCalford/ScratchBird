# Catalog System Cleanup - Overview and Master Plan

**Created:** November 26, 2025
**Updated:** November 26, 2025
**Priority:** HIGH - Prerequisite for Alpha Phase 2
**Estimated Total Effort:** 155-215 hours
**Target:** Complete before Alpha Phase 2 development

---

## Executive Summary

This document outlines the comprehensive cleanup of the ScratchBird catalog system to prepare for Alpha Phase 2 features (Distributed MVCC, Wire Protocol Integration, UDR System). The cleanup ensures all system structures are properly defined, all CRUD operations are implemented, and the foundation is ready for Phase 2's virtual catalog requirements.

---

## Current State Assessment

### Structures Defined in catalog_manager.h

| Category | Structures | CRUD Status |
|----------|-----------|-------------|
| **Core Tables** | SchemaInfo, TableInfo, ColumnInfo, IndexInfo | Mostly Complete |
| **Sequences/Views** | SequenceInfo, ViewInfo | Complete |
| **Security** | UserInfo, RoleInfo, GroupInfo, SessionInfo | Mostly Complete |
| **Constraints** | ConstraintInfo, ForeignKeyInfo | Complete |
| **Stored Code** | FunctionInfo, ProcedureInfo, TriggerInfo | Partial |
| **Domains/UDR** | DomainInfo, UDRInfo, PackageInfo | Structures Only |
| **Emulation** | EmulationTypeInfo, EmulationServerInfo, EmulatedDatabaseInfo | Structures Only |
| **Metadata** | DependencyInfo, CommentInfo, PermissionInfo | Complete |

### Missing Elements for Phase 2

1. **Missing CRUD Operations** - 6 structures have no CRUD methods
2. **Missing Structures** - FDW tables, Server Registry, UDR Modules
3. **Missing Virtual Catalog** - No information_schema or protocol-specific views
4. **Incomplete ObjectType Enum** - Missing entries for new structures

---

## Implementation Plan Structure

The cleanup is organized into 4 sequential phases:

### Phase A: Complete Missing CRUD Operations
**Document:** `CATALOG_CLEANUP_PHASE_A_CRUD.md`
**Effort:** 48-63 hours
**Priority:** Critical

Add CRUD methods for existing structures that lack them:
- dropSchema()
- Domain CRUD (create, get, update, delete)
- UDR CRUD (create, get, update, delete)
- Package CRUD (create, get, update, delete)
- Emulation CRUD (all 3 types)
- updateRole(), updateGroup()

### Phase B: Add Missing Structures
**Document:** `CATALOG_CLEANUP_PHASE_B_STRUCTURES.md`
**Effort:** 37-48 hours
**Priority:** High

Define new structures required for Phase 2:
- SchemaType enum and hierarchical schema support
- SynonymInfo for cross-schema pointers
- ForeignServerInfo, ForeignTableInfo, UserMappingInfo (FDW)
- ServerRegistryInfo (Distributed MVCC)
- UDRModuleInfo, UDREngineInfo (UDR System)
- Extended ObjectType enum values

### Phase C: System Table Page Allocation
**Document:** `CATALOG_CLEANUP_PHASE_C_PAGES.md`
**Effort:** 30-43 hours
**Priority:** High

Allocate storage pages for unallocated system tables:
- synonyms_table_page_
- domains_table_page_
- udr_table_page_
- packages_table_page_
- emulation_types_table_page_
- emulation_servers_table_page_
- emulated_dbs_table_page_
- New FDW and UDR tables

### Phase D: Virtual Catalog Infrastructure
**Document:** `CATALOG_CLEANUP_PHASE_D_VIRTUAL.md`
**Effort:** 38-52 hours
**Priority:** Medium (Foundation for Phase 2)

Design and implement virtual catalog layer:
- information_schema view infrastructure
- Protocol-specific view templates (pg_catalog, mysql.*, sys.*, RDB$*)
- On-demand emulation view generation
- View registration and query routing

**Reference:** `SCHEMA_ARCHITECTURE.md` for hierarchical schema design

---

## Dependencies and Ordering

```
Phase A (CRUD) ─────────────────────────────────────┐
                                                    │
Phase B (Structures) ──────────────────────────────┬┼──► Phase D (Virtual)
                                                    ││
Phase C (Pages) ────────────────────────────────────┘│
                                                     │
                                                     ▼
                                            Alpha Phase 2
```

**Phases A, B, C can be worked in parallel** (different files, minimal overlap)
**Phase D depends on A, B, C completion**

---

## File Impact Analysis

### Primary Files Modified

| File | Changes |
|------|---------|
| `include/scratchbird/core/catalog_manager.h` | New structures, CRUD declarations, ObjectType enum |
| `src/core/catalog_manager.cpp` | CRUD implementations, page allocation |
| `include/scratchbird/catalog/virtual_catalog.h` | NEW - Virtual catalog infrastructure |
| `src/catalog/virtual_catalog.cpp` | NEW - Virtual catalog implementation |

### New Files Created

| File | Purpose |
|------|---------|
| `include/scratchbird/catalog/information_schema.h` | information_schema view definitions |
| `src/catalog/information_schema.cpp` | information_schema implementations |
| `include/scratchbird/catalog/protocol_catalog.h` | Protocol-specific catalog mapping |
| `src/catalog/protocol_catalog.cpp` | Protocol catalog implementations |

---

## Success Criteria

### Phase A Complete
- [ ] All 6 structure types have full CRUD operations
- [ ] Unit tests pass for all new CRUD methods
- [ ] No breaking changes to existing APIs

### Phase B Complete
- [ ] All Phase 2 structures defined in catalog_manager.h
- [ ] ObjectType enum extended with new values
- [ ] Structures aligned with Alpha Phase 2 specifications

### Phase C Complete
- [ ] All system tables have allocated page IDs
- [ ] CatalogManager::initialize() creates all pages
- [ ] Catalog loads correctly with new tables

### Phase D Complete
- [ ] information_schema views return correct data
- [ ] Virtual catalog queries routed correctly
- [ ] Foundation ready for wire protocol integration

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking existing APIs | Medium | High | Maintain backward compatibility |
| Page allocation conflicts | Low | High | Careful ID management |
| Performance regression | Low | Medium | Benchmark before/after |
| Structure misalignment with Phase 2 | Medium | High | Regular spec review |

---

## Timeline Recommendation

| Week | Work |
|------|------|
| Week 1 | Phase A (CRUD operations) |
| Week 2 | Phase B (Structures) + Phase C (Pages) in parallel |
| Week 3 | Phase D (Virtual Catalog) |
| Week 4 | Integration testing, bug fixes |

**Total:** 4 weeks (assuming 40h/week)

---

## Related Documents

- `SCHEMA_ARCHITECTURE.md` - **NEW** Hierarchical schema namespace design
- `CATALOG_CLEANUP_PHASE_A_CRUD.md` - CRUD operations implementation
- `CATALOG_CLEANUP_PHASE_B_STRUCTURES.md` - New structure definitions (including Synonyms)
- `CATALOG_CLEANUP_PHASE_C_PAGES.md` - Page allocation
- `CATALOG_CLEANUP_PHASE_D_VIRTUAL.md` - Virtual catalog infrastructure (on-demand emulation)
- `FUTURE_WORK_BLOCKED_ITEMS.md` - Deferred items
- `/docs/specifications/Alpha Phase 2/` - Phase 2 specifications

---

**Document Version:** 1.1
**Last Updated:** November 26, 2025
