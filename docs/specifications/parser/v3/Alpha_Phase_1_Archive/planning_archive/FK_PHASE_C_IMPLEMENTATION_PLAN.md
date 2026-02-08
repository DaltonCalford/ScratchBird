# Foreign Key Phase C - Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created**: November 14, 2025
**Status**: ACTIVE
**Estimated Effort**: 40-60 hours
**Goal**: Complete FK system to 100%

---

## Overview

Phase C completes the FK system by adding advanced features:
- Composite (multi-column) FKs
- Table-level FK syntax
- Catalog disk persistence
- Index-based lookups (performance)
- ALTER TABLE FK operations
- MATCH FULL/PARTIAL support
- Deferred constraint checking

---

## Priority Order

### Priority 1: Composite FK Support (10-15 hours) ⚠️ CRITICAL
**Why First**: Enables real-world multi-column keys, blocks many use cases
**Impact**: HIGH - Required for composite primary keys
**Complexity**: MEDIUM - Parser, bytecode, executor changes

**Tasks**:
1. Extend parser for table-level FOREIGN KEY syntax
2. Update bytecode generation for multi-column FKs
3. Extend executor checkForeignKeyExists for composite keys
4. Update FK actions (CASCADE, SET NULL, SET DEFAULT) for multi-column
5. Add integration tests

### Priority 2: Table-Level FK Syntax (5-10 hours)
**Why Second**: Goes hand-in-hand with composite FKs
**Impact**: HIGH - SQL standard compliance
**Complexity**: LOW - Parser extension only

**Tasks**:
1. Add CONSTRAINT name FOREIGN KEY (...) REFERENCES ... syntax
2. Update bytecode for named constraints
3. Integration with composite FK support

### Priority 3: Index-Based FK Lookups (15-20 hours)
**Why Third**: Major performance improvement (10-100x)
**Impact**: MEDIUM - Performance optimization
**Complexity**: HIGH - Requires index integration

**Tasks**:
1. Detect indexes on FK columns
2. Use index scan instead of table scan in checkForeignKeyExists
3. Optimize FK action lookups (CASCADE, etc.)
4. Benchmark performance improvements

### Priority 4: Catalog Disk Persistence (10-15 hours)
**Why Fourth**: Important for durability, but memory works for now
**Impact**: MEDIUM - Persistence across restarts
**Complexity**: MEDIUM - Add pg_constraints table operations

**Tasks**:
1. Design pg_constraints schema (if not exists)
2. Implement saveForeignKey() / loadForeignKeys()
3. Integration with database open/close
4. Migration for existing FKs

### Priority 5: ALTER TABLE FK Operations (5-10 hours)
**Why Fifth**: Convenience feature, workarounds exist (DROP/CREATE TABLE)
**Impact**: LOW - Nice to have
**Complexity**: MEDIUM - Parser + executor integration

**Tasks**:
1. ALTER TABLE ADD CONSTRAINT FOREIGN KEY syntax
2. ALTER TABLE DROP CONSTRAINT syntax
3. Integration with existing FK catalog
4. Validation and error handling

### Priority 6: MATCH FULL/PARTIAL (10-15 hours)
**Why Sixth**: Edge case, MATCH SIMPLE covers 95% of use cases
**Impact**: LOW - Advanced feature
**Complexity**: MEDIUM - Semantic changes in validation

**Tasks**:
1. Parser support for MATCH FULL/PARTIAL keywords
2. Update checkForeignKeyExists semantics
3. Integration tests for MATCH types

### Priority 7: Deferred Constraint Checking (10-15 hours)
**Why Last**: Advanced feature, immediate checking works for most cases
**Impact**: LOW - Niche use case
**Complexity**: HIGH - Transaction integration

**Tasks**:
1. DEFERRABLE keyword parsing
2. Transaction-level constraint tracking
3. Deferred validation at COMMIT
4. Integration with transaction manager

---

## Implementation Strategy

### Session 1: Composite FK Support (Target: 3-4 hours)
**Goal**: Multi-column FKs working end-to-end

**Approach**:
1. Start with table-level FK syntax parsing
2. Extend bytecode for column lists
3. Update executor to handle multiple columns
4. Test with 2-column and 3-column FKs

### Session 2: Index-Based Lookups (Target: 4-5 hours)
**Goal**: 10-100x performance improvement

**Approach**:
1. Detect B-Tree indexes on FK columns
2. Use index scan API instead of table scan
3. Benchmark before/after on large tables
4. Optimize CASCADE operations

### Session 3: Catalog Persistence (Target: 3-4 hours)
**Goal**: FKs survive database restart

**Approach**:
1. Add FK save/load to catalog manager
2. Call on database open/close
3. Test persistence across restarts

### Session 4: ALTER TABLE + Polish (Target: 2-3 hours)
**Goal**: Complete remaining features

**Approach**:
1. Implement ALTER TABLE FK operations
2. Add MATCH FULL support (PARTIAL can wait)
3. Documentation and final testing

---

## Success Criteria

### Phase C Complete When:
- ✓ Composite FKs work (2+ columns)
- ✓ Table-level FK syntax supported
- ✓ Index-based lookups operational
- ✓ FKs persist across database restarts
- ✓ ALTER TABLE ADD/DROP CONSTRAINT works
- ✓ All tests passing
- ✓ Documentation updated

### Optional (Can Defer):
- MATCH FULL/PARTIAL (if time allows)
- Deferred constraint checking (future phase)
- Automatic index creation (future phase)

---

## Risk Assessment

### High Risk Areas:
1. **Composite FK Validation**: Complex logic with multiple columns
   - Mitigation: Comprehensive test cases, incremental implementation

2. **Index Integration**: Coupling with index system
   - Mitigation: Use existing index scan API, fallback to table scan

3. **Bytecode Compatibility**: Changes to FK opcode format
   - Mitigation: Version bytecode format, maintain backward compatibility

### Low Risk Areas:
1. Catalog persistence - straightforward CRUD
2. ALTER TABLE syntax - parser extension only
3. MATCH FULL - semantic change, well-defined

---

## Testing Strategy

### Unit Tests:
- Composite FK validation (2, 3, 4 columns)
- Index-based lookup correctness
- Catalog save/load round-trip

### Integration Tests:
- Multi-column CASCADE operations
- Index performance benchmarks
- ALTER TABLE operations
- MATCH SIMPLE vs MATCH FULL semantics

### Regression Tests:
- Ensure Phase A/B tests still pass
- Backward compatibility with single-column FKs

---

## Documentation Plan

### Update Documents:
- README.md - Update FK status to 100%
- PROJECT_CONTEXT.md - Mark Phase C complete
- IMPLEMENTATION_AUDIT.md - Add Phase C locations
- FK_PHASE_C_COMPLETE_2025-11-14.md - New completion doc

### New Documentation:
- Composite FK usage examples
- Index-based FK performance guide
- ALTER TABLE FK reference

---

## Timeline Estimate

| Task | Estimated Hours | Priority |
|------|----------------|----------|
| Composite FK Support | 10-15 | P1 |
| Table-Level FK Syntax | 5-10 | P1 |
| Index-Based Lookups | 15-20 | P2 |
| Catalog Persistence | 10-15 | P2 |
| ALTER TABLE Operations | 5-10 | P3 |
| MATCH FULL Support | 5-10 | P3 |
| Testing & Documentation | 5-10 | P1 |
| **TOTAL** | **55-90 hours** | |

**Realistic Target**: 60-70 hours (2-3 weeks at 40 hours/week)

---

## Next Steps

**Immediate Actions**:
1. ✅ Create Phase C plan (this document)
2. Start composite FK parser implementation
3. Update bytecode generator for multi-column
4. Extend executor validation logic
5. Add integration tests

**Decision Points**:
- Include MATCH FULL in Phase C? (Recommend: YES if time)
- Include deferred checking? (Recommend: NO, defer to Phase D)
- Auto-create indexes on FKs? (Recommend: NO, defer to Phase D)

---

**Status**: Ready to begin
**Next Milestone**: Composite FK support operational
**Target Completion**: November 2025
