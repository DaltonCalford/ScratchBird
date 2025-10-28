# ScratchBird Database Engine

An educational relational database engine built from scratch featuring Firebird-style MGA (Multi-Generational Architecture), multi-tablespace support, MVCC with all isolation levels, 6 index types, TOAST storage, and comprehensive transaction management.

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. && make

# Run tests
ctest --output-on-failure
```

## Current Status

**Version:** Alpha 1.0.7
**Status:** Educational/Development (**Phase 1: 100% COMPLETE** 🎉 - All Critical Blockers Resolved)
**Last Updated:** October 28, 2025 (Phase 1 Completion - Conditional Functions)

✅ **MILESTONE ACHIEVED**: Phase 1 is 100% complete with comprehensive audit verification. See `/docs/audit/PHASE_1_COMPLETION_AUDIT.md` for full details.

### Latest Achievements

**🎉 Phase 1 Complete (October 28, 2025)** - All 8 Critical Tasks Delivered

**Comprehensive Audit Result**: ✅ **100% COMPLETE** - Zero blocking issues found

| Task | Feature | Status | Lines | Tests |
|------|---------|--------|-------|-------|
| 1 | Query Optimizer | ✅ 100% | 3,653 | Multiple |
| 2 | UPDATE/DELETE | ✅ 100% | 1,183 | 7 files |
| 3 | JOINs | ✅ 100% | 3,910 | 10+ cases |
| 4 | Aggregation | ✅ 100% | 1,510 | 2+ files |
| 5 | Sorting/Limiting | ✅ 100% | 855 | 2 files |
| 6 | Window Functions | ✅ 100% | 1,050 | 60+ cases |
| 7 | JSON Functions | ✅ 100% | 500 | 53 tests |
| 8 | Conditional Functions | ✅ 100% | 320 | 40+ cases |

**Total Implementation**: 12,981 lines of production code + 200+ test cases

**Phase 1 Acceptance Test - ALL Features Working:**
```sql
EXPLAIN
SELECT
    u.name,
    COUNT(o.id) as order_count,
    SUM(o.total) as total_spent,
    COALESCE(u.email, 'no-email') as email,
    ROW_NUMBER() OVER (ORDER BY SUM(o.total) DESC) as rank,
    CASE
        WHEN SUM(o.total) > 10000 THEN 'premium'
        WHEN SUM(o.total) > 1000 THEN 'regular'
        ELSE 'basic'
    END as tier,
    o.metadata->>'preferences' as prefs
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
WHERE u.created_at > '2024-01-01'
GROUP BY u.id, u.name, u.email, o.metadata
HAVING COUNT(o.id) > 5
ORDER BY total_spent DESC
LIMIT 100;
```

**Features Demonstrated**:
- ✅ Query Optimization (EXPLAIN)
- ✅ Multi-table queries (LEFT JOIN)
- ✅ Filtering (WHERE)
- ✅ Aggregation (COUNT, SUM, GROUP BY)
- ✅ Post-aggregation filtering (HAVING)
- ✅ Window functions (ROW_NUMBER)
- ✅ Conditional logic (COALESCE, CASE)
- ✅ JSON operations (->>, metadata access)
- ✅ Sorting and pagination (ORDER BY, LIMIT)

**Previous: 1:1 Feature Parity Audit Complete (Oct 25, 2025)** 📋 - CORRECTED PERSPECTIVE

⚠️ **CRITICAL UPDATE**: Previous percentages used engineering judgment to mark features as "optional."
**Market Requirement**: ScratchBird must achieve **1:1 feature parity** with ALL 4 target databases.

**Corrected Completion Percentages** (1:1 Parity vs. All 4 DBs):
- ⚠️ **Type System**: 60-65% (~3,407 lines) - Missing spatial, network, text search, range types
- ⚠️ **Index Types**: 70-75% (~11,376 lines) - Missing spatial indexes, expression indexes
- ❌ **Functions/Operators**: 10-15% (~4,458 lines) - Missing 85-90% of comprehensive library
- ⚠️ **Schema Structure**: 70-80% (~6,432 lines) - Missing procedure/trigger/view catalogs
- ⚠️ **Parser**: 45-50% (~6,720 lines) - JOINs ✅, GROUP BY ✅, ORDER BY ✅, LIMIT ✅, UPDATE/DELETE TODO
- ⚠️ **Query Optimizer**: 85-90% (~3,700 lines) - Cost model ✅, Statistics ✅, Planner ✅, Execution ~30%
- ✅ **Storage Engine**: 100% (~34,000 lines) - Full MGA/MVCC implementation

**Why Percentages Changed**:
- Previous: Assumed specialized features (spatial, text search, etc.) were "optional"
- Corrected: ALL features in target databases are REQUIRED for market competitiveness
- Impact: GIS apps, network tools, full-text search, analytics CANNOT use ScratchBird

**Total Verified Code**: ~67,300+ lines (was ~65,000, added ~2,300 lines for aggregation/sorting)
**Total Work Remaining**: ~1,820-2,945 hours (~11-18 months with 1 developer, reduced by 200 hours)

### What's Implemented ✅ (Code-Verified - Oct 25, 2025)

- **Storage Engine:** Buffer pool, page management, heap pages, TOAST (✅ 100% verified)
- **Transaction Management:** Firebird MGA, 4 isolation levels, sweep, GC (✅ 100% verified)
- **MVCC/MGA:** Back versioning, cross-page support, stable TIDs, N2O chains (✅ 100% verified)
- **Concurrency:** Multi-connection, locking, deadlock detection (✅ 100% verified)
- **Indexing:** B-tree, Hash, GIN, Bitmap, HNSW, BRIN (✅ all 6 types exist, ~11,376 lines verified)
- **Tablespace:** Core infrastructure, GPID/TID, autoextend (✅ verified, ⚠️ ONLINE migration incomplete)
- **Type System:** 29 data types, UUIDv7, timezones, collations (✅ 90-95% complete, ~3,407 lines)
- **Parser:** Lexer, AST, semantic analyzer (⚠️ 45-50% - ~6,720 lines, JOINs/GROUP BY ✅, UPDATE/DELETE TODO)
- **Query Executor:** SBLR bytecode generator + executor (⚠️ 30-35% functions, ~4,600 lines)
- **Query Optimizer:** Cost-based optimization, statistics (⚠️ 85-90% - ~3,700 lines, execution ~30%)
- **Schema Catalog:** Recursive schema, 7 catalog structures (✅ 100% complete, ~6,432 lines)
- **Code Quality:** RAII, comprehensive logging, const-correct APIs (✅ verified in audit)
- **CI/CD:** TSAN, ASAN, Helgrind, Valgrind, Clang-Tidy (⚠️ configs not verified)

### Known Limitations ⚠️

**Per Corrected Audit (Updated Oct 27, 2025)**:

- **Query Optimizer ~85-90% Complete** (⚠️ Cost model ✅, Statistics ✅, Planner ✅, Execution ~30%)
- **Incomplete Parser** (⚠️ 45-50% - JOINs ✅, GROUP BY ✅, ORDER BY ✅, missing UPDATE/DELETE)
- **Minimal Functions** (⚠️ 30-35% - ~25 functions vs. ~200+ in production databases)
- **Incomplete ONLINE Migration** (❌ Sprint 5 not implemented, ⚠️ Sprint 4 partial)
- **No WAL** (no crash recovery - Beta requirement)
- **No network layer** (local database only - Beta requirement)

### Remaining for 1:1 Feature Parity

**Per 1:1 Parity Audit (Oct 25, 2025)**: ~2,020-3,145 hours estimated (~12-19 months with 1 dev)

**Phase 1: Critical Blockers** ✅ **100% COMPLETE** (Oct 28, 2025):
1. ✅ **Query Optimizer** (100-160h) - Cost model, statistics, plan selection, EXPLAIN
2. ✅ **Core CRUD** (35-55h) - UPDATE, DELETE statements with WHERE clause
3. ✅ **Basic Queries** (30-50h) - JOINs (all types), GROUP BY, ORDER BY, LIMIT
4. ✅ **Window Functions** (60-90h) - 8 functions: ROW_NUMBER, RANK, DENSE_RANK, LAG, LEAD, etc.
5. ✅ **JSON Functions** (80-120h) - 14 functions with nlohmann/json integration
6. ✅ **Conditional Functions** (20-30h) - COALESCE, NULLIF, CASE (simple and searched)

**Audit Result**: Zero blocking TODOs, 92 non-critical TODOs (optimizations and future phases)

**Phase 2: Competitive Parity** (~800-1,200 hours) - SHOULD HAVE to compete:
1. ❌ **Spatial Types + Indexes + Functions** (420-630h) - GIS/mapping market
2. ❌ **Triggers/Procedures** (200-300h) - Business logic in database
3. ❌ **CTEs and Subqueries** (110-170h) - Complex queries
4. ❌ **Array/Text Search Functions** (90-140h) - PostgreSQL compatibility

**Phase 3: Full Parity** (~800-1,300 hours) - NICE TO HAVE for complete replacement:
1. ❌ **Text Search Types** (130-200h) - PostgreSQL full-text search
2. ❌ **Range Types** (100-150h) - Temporal/booking applications
3. ❌ **Network Types** (40-60h) - Network/DevOps tools
4. ❌ **Expression/Filtered Indexes** (120-180h) - Advanced optimization
5. ❌ **Extended Functions** (300-450h) - String, numeric, datetime, system
6. ❌ **Sequences, Permissions, Remaining DDL** (80-140h)
7. ❌ **Bit String, DECFLOAT, ENUM/SET** (90-140h) - Full compatibility

**See Detailed Audit Reports**:
- ✅ `/docs/audit/PHASE_1_COMPLETION_AUDIT.md` - **NEW!** Phase 1 completion verification (Oct 28, 2025)
- `/docs/audit/FEATURE_PARITY_GAP_ANALYSIS.md` - 🔴 CRITICAL gaps identified
- `/docs/audit/AUDIT_CORRECTIONS_SUMMARY.md` - Why percentages changed
- `/docs/audit/TYPE_SYSTEM_COMPLETENESS_AUDIT.md` - ⚠️ 60-65% (was 90-95%)
- `/docs/audit/INDEX_TYPE_COMPLETENESS_AUDIT.md` - ⚠️ 70-75% (was 95-100%)
- `/docs/audit/FUNCTION_COMPLETENESS_AUDIT.md` - ❌ 10-15% (was 25-30%)
- `/docs/audit/SCHEMA_STRUCTURE_AUDIT.md` - ⚠️ 70-80% (was 100%)
- `/docs/audit/PARSER_COVERAGE_AUDIT.md` - ❌ 20-25% (was 64%)
- `/docs/audit/QUERY_OPTIMIZATION_AUDIT.md` - ❌ 0%

## Project Structure

- **`docs/`** - All documentation
  - `specifications/` - TABLESPACE_SPECIFICATION.md, MGA_IMPLEMENTATION.md
  - `planning/` - TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md
  - `guides/` - Developer guides (locking, error handling, concurrency, resources)
  - `STATUS_*.md` - Sprint completion reports
- **`src/`** - Source code
  - `core/` - Storage engine, indexes, transactions, catalog
  - `parser/` - SQL parser
  - `sblr/` - Query executor
- **`tests/`** - Test suites
  - `unit/` - Unit tests
  - `integration/` - Integration tests
- **`include/`** - Public headers

## Development Process

1. **READ FIRST**: Review corrected audit reports in `/docs/audit/` (Oct 25, 2025):
   - `TYPE_SYSTEM_COMPLETENESS_AUDIT.md` - ✅ 90-95% complete
   - `INDEX_TYPE_COMPLETENESS_AUDIT.md` - ✅ 95-100% complete
   - `FUNCTION_COMPLETENESS_AUDIT.md` - ⚠️ 25-30% complete
   - `SCHEMA_STRUCTURE_AUDIT.md` - ✅ 100% complete
   - `PARSER_COVERAGE_AUDIT.md` - ⚠️ 64% complete
   - `QUERY_OPTIMIZATION_AUDIT.md` - ❌ 0% complete
2. Review [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) for current state
3. Review [TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md](docs/planning/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md) for priorities
4. Check [TODO.md](docs/development/TODO.md) for work items
5. Review developer documentation:
   - [Locking Protocol](docs/LOCKING_PROTOCOL.md)
   - [Error Handling Guide](docs/ERROR_HANDLING_GUIDE.md)
   - [Concurrency Patterns](docs/CONCURRENCY_PATTERNS.md)
   - [Resource Management](docs/RESOURCE_MANAGEMENT.md)
6. Follow [Coding Standards](docs/development/CODING_STANDARDS.md)
7. Run sanitizers before commit: `./tools/run_sanitizers.sh --all`
8. Run tests frequently with `ctest --output-on-failure`

## Building

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Build Types

```bash
# Debug (default)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release
cmake -DCMAKE_BUILD_TYPE=Release ..

# TSAN (thread safety)
cmake -DCMAKE_BUILD_TYPE=TSan ..

# ASAN (memory errors)
cmake -DCMAKE_BUILD_TYPE=ASan ..
```

## Testing

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific tests
ctest -R "test_name"

# Run sanitizers
./tools/run_sanitizers.sh --all
```

## Architecture Highlights

### Firebird MGA (Multi-Generational Architecture)
- **In-place updates** with back versions (not PostgreSQL's append-only)
- **Stable TIDs** - indexes never updated unless indexed column changes
- **N2O version chains** - Newest-to-Oldest traversal
- **Zero heap fragmentation** by design

### Tablespace System
- **GPID Addressing** - 64-bit (16-bit tablespace + 48-bit page)
- **ONLINE Migration** - Zero-downtime table migration with < 5% overhead
- **Dual-Source Visibility** - TIDResolver with Bloom filters
- **Multi-file Support** - Storage tiering, data lifecycle management

### Index Types
1. **B-Tree** - General purpose, with prefix compression
2. **Hash** - Equality lookups
3. **GIN** - Inverted index for arrays, full-text
4. **Bitmap** - Low-cardinality columns, Roaring compression
5. **HNSW** - Vector similarity search (ANN)
6. **BRIN** - Block range indexes for sequential data

## License

See LICENSE file for details.
