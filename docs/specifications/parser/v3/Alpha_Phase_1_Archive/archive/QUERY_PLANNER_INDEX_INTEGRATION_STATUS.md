# Query Planner Index Integration Status

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

**Date:** November 20, 2025
**Task:** TASK-BYTECODE-4: Query Planner Integration
**Status:** In Progress

---

## CURRENT STATE

### Implemented Path Types
- ✅ SeqScanPath - Sequential scan
- ✅ IndexScanPath - Generic index scan with heap fetch
- ✅ RTreeScanPath - Spatial index scan (specialized)
- ❌ BitmapIndexScanPath - **MISSING** (bitmap index scan)
- ❌ IndexOnlyScanPath - **MISSING** (covering index, no heap access)

### Implemented Plan Node Types
- ✅ SeqScanNode - Sequential scan
- ✅ IndexScanNode - Generic index scan
- ✅ RTreeScanNode - Spatial index scan
- ❌ BitmapIndexScanNode - **MISSING**
- ❌ IndexOnlyScanNode - **MISSING**

### Index Type Integration Status

| Index Type | Cost Estimation | Path Generation | Applicability Check | Status |
|------------|----------------|-----------------|---------------------|--------|
| B-Tree | ✅ costIndexScan() | ✅ generateIndexScanPaths() | ⚠️ Generic | 90% |
| Hash | ⚠️ Generic costIndexScan() | ✅ generateIndexScanPaths() | ⚠️ Generic | 60% |
| LSM-Tree | ✅ costLSMScan() | ✅ generateIndexScanPaths() | ✅ Specialized | 100% |
| R-Tree | ✅ costIndexScan() | ✅ generateRTreeScanPaths() | ✅ Spatial predicates | 95% |
| GIN | ⚠️ Generic costIndexScan() | ✅ generateIndexScanPaths() | ⚠️ Generic | 50% |
| HNSW | ⚠️ Generic costIndexScan() | ✅ generateIndexScanPaths() | ⚠️ Generic | 50% |
| GiST | ⚠️ Generic costIndexScan() | ✅ generateIndexScanPaths() | ⚠️ Generic | 50% |
| SP-GiST | ⚠️ Generic costIndexScan() | ✅ generateIndexScanPaths() | ⚠️ Generic | 50% |
| BRIN | ⚠️ Generic costIndexScan() | ✅ generateIndexScanPaths() | ⚠️ Generic | 50% |
| Columnstore | ⚠️ Generic costIndexScan() | ✅ generateIndexScanPaths() | ⚠️ Generic | 50% |
| Bitmap | ⚠️ Generic costIndexScan() | ❌ **MISSING** | ❌ **MISSING** | 30% |

---

## WHAT'S MISSING FOR 100% INTEGRATION

### Priority 1: Core Missing Features (High Impact)

#### 1. BitmapIndexScan Support (4-6 hours)
**Purpose:** Combine multiple indexes with AND/OR operations
- Add `BITMAP_INDEX_SCAN` to PathType enum (path.h)
- Add `BitmapIndexScanPath` class (path.h)
- Add `BITMAP_INDEX_SCAN` to PlanNodeType enum (plan_node.h)
- Add `BitmapIndexScanNode` class (plan_node.h)
- Add `generateBitmapIndexScanPaths()` to QueryPlanner (query_planner.cpp)
- Add `costBitmapIndexScan()` to CostModel (cost_model.h/cpp)
- Handle in `pathToPlanNode()` conversion

**Use Case:**
```sql
SELECT * FROM users WHERE age > 25 AND city = 'Seattle';
-- Can use: idx_users_age AND idx_users_city via bitmap scan
```

#### 2. IndexOnlyScan Support (3-4 hours)
**Purpose:** Avoid heap access when index covers all needed columns
- Add `INDEX_ONLY_SCAN` to PathType enum (path.h)
- Add `IndexOnlyScanPath` class (path.h)
- Add `INDEX_ONLY_SCAN` to PlanNodeType enum (plan_node.h)
- Add `IndexOnlyScanNode` class (plan_node.h)
- Add covering index detection in `generateIndexScanPaths()` (query_planner.cpp)
- Add visibility map integration (check TIP instead of heap)
- Handle in `pathToPlanNode()` conversion

**Use Case:**
```sql
SELECT id, name FROM users WHERE id > 100;
-- If idx_users_id_name(id, name) exists → index-only scan
```

### Priority 2: Specialized Cost Models (Medium Impact)

#### 3. GIN Index Cost Model (2-3 hours)
- Add `costGINScan()` to CostModel
- Multi-key lookups (arrays, JSONB)
- Posting list traversal costs
- Update `generateIndexScanPaths()` to use specialized cost

#### 4. HNSW Index Cost Model (2-3 hours)
- Add `costHNSWScan()` to CostModel
- K-NN search cost (ef_search parameter)
- Graph traversal cost (M neighbors per layer)
- Update `generateIndexScanPaths()` to use specialized cost

#### 5. GiST Index Cost Model (2 hours)
- Add `costGiSTScan()` to CostModel
- Predicate-based tree traversal
- Extensible operator support

#### 6. BRIN Index Cost Model (2 hours)
- Add `costBRINScan()` to CostModel
- Block range summary costs
- Min/max filtering effectiveness

#### 7. Columnstore Cost Model (2-3 hours)
- Add `costColumnstoreScan()` to CostModel
- Columnar vs row-based access patterns
- Compression effectiveness

### Priority 3: Enhanced Applicability Checks (Medium Impact)

#### 8. Index Type Specific Predicate Matching (4-6 hours)
- GIN: Array contains (@>), JSONB operators
- HNSW: K-NN distance operators (<->, <#>, <=>)
- GiST: Geometric operators (&&, @>, <@)
- SP-GiST: Quad-tree predicates
- BRIN: Range predicates on clustered data
- Hash: Equality only (=)

Current code uses generic `isIndexApplicable()` which just checks for WHERE clause existence.
Need specialized checks per index type.

### Priority 4: Testing (Low Impact, High Importance)

#### 9. Integration Tests (3-4 hours)
- Test index selection for all 11 index types
- Test cost-based selection (verify cheapest path chosen)
- Test BitmapIndexScan generation
- Test IndexOnlyScan detection
- Test multi-index queries

---

## IMPLEMENTATION PLAN

### Phase 1: Core Infrastructure (10-12 hours)
1. **Add BitmapIndexScan** (6h)
   - Path and PlanNode types
   - Cost model
   - Path generation
   - Plan conversion

2. **Add IndexOnlyScan** (4h)
   - Path and PlanNode types
   - Covering index detection
   - Visibility map integration

3. **Tests for Phase 1** (2h)
   - Bitmap scan tests
   - Index-only scan tests

### Phase 2: Specialized Cost Models (8-10 hours)
4. **GIN Cost Model** (3h)
5. **HNSW Cost Model** (3h)
6. **GiST/BRIN/Columnstore Cost Models** (4h)

### Phase 3: Enhanced Applicability (6-8 hours)
7. **Index-specific predicate matching** (6h)
8. **Integration tests** (2h)

### Phase 4: Documentation (2 hours)
9. Update PROJECT_CONTEXT.md with completion status
10. Document query planner architecture

---

## ESTIMATED TOTAL EFFORT
- Priority 1 (Core): 10-12 hours
- Priority 2 (Cost Models): 8-10 hours
- Priority 3 (Applicability): 6-8 hours
- Priority 4 (Testing): 3-4 hours
**Total: 27-34 hours**

---

## IMMEDIATE NEXT STEPS (for this session)

1. ✅ Audit current state (THIS DOCUMENT)
2. Add BitmapIndexScan support:
   - Update path.h with BitmapIndexScanPath
   - Update plan_node.h with BitmapIndexScanNode
   - Add costBitmapIndexScan() to cost_model.h/cpp
   - Add generation logic to query_planner.cpp
3. Add IndexOnlyScan support:
   - Update path.h with IndexOnlyScanPath
   - Update plan_node.h with IndexOnlyScanNode
   - Add covering index detection
4. Add basic integration tests
5. Commit and push

---

**Document Created:** November 20, 2025
**Status:** Planning → Implementation
