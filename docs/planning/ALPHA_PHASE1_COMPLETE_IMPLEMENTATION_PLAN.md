# ScratchBird ALPHA Phase 1 - Complete Implementation Plan

**Created**: November 3, 2025
**Updated**: November 7, 2025
**Goal**: 100% implementation of all specified features
**Status**: ACTIVE PLAN

---

## EXECUTIVE SUMMARY

### Current Completion: 75%

**Remaining Work**: ~1,200-1,700 hours (30-43 weeks at 40 hours/week)

**Recent Milestones**:
- ✅ **All 11 index types complete** (B-Tree, Hash, R-Tree, GIN, Bitmap, GiST, HNSW, SP-GiST, BRIN, LSM-Tree, Columnstore) 🎉
- ✅ **All 86 data types complete** (COMPOSITE, VECTOR, VARIANT) 🎉
- ✅ **Domain CHECK constraints** with pattern matching ✨

---

## What's Complete ✅

### Indexes (11/11 = 100%) 🎉
- B-Tree, Hash, R-Tree, GIN, Bitmap
- GiST, HNSW, SP-GiST, BRIN
- Columnstore, LSM-Tree
- All production-ready with MGA compliance

### Data Types (86/86 = 100%) 🎉
- Numeric: INT8-INT128, UINT8-UINT64, DECIMAL, FLOAT, MONEY
- String: CHAR, VARCHAR, TEXT
- Temporal: DATE, TIME, TIMESTAMP, INTERVAL
- Binary: BLOB, BYTEA, VARBINARY
- Special: UUID, JSON/JSONB, XML, BOOLEAN
- Spatial: POINT, LINESTRING, POLYGON
- Advanced: ARRAY, RANGE, COMPOSITE, VECTOR, VARIANT
- Network: INET, CIDR, MACADDR
- Text Search: TSVECTOR, TSQUERY
- **Domains** with CHECK constraints

### SQL Execution (15/35 = 43%)
- ✅ SELECT (WHERE, JOIN, GROUP BY, HAVING, ORDER BY, LIMIT, window functions)
- ✅ INSERT, UPDATE, DELETE
- ✅ CREATE TABLE, CREATE INDEX
- ✅ CREATE/ALTER/DROP TABLESPACE, ATTACH/DETACH TABLESPACE
- ✅ BEGIN, COMMIT, ROLLBACK, SAVEPOINT

### Built-in Functions (60/100 = 60%)
- ✅ String (11), Aggregate (6), Window (8)
- ✅ JSON (13), Array (12), Date/Time (6)
- ✅ Conditional (3), Regex (4), Spatial (4+)

### Constraints (2/10 = 20%)
- ✅ NOT NULL
- ✅ Data type validation

---

## What's Missing ❌

### 🔴 CRITICAL PRIORITY

#### 1. DDL Modifications (0% complete) - 80-100 hours
- ❌ ALTER TABLE - Cannot modify schemas
- ❌ DROP TABLE - Cannot remove tables
- ❌ DROP INDEX - Cannot remove indexes
- **Impact**: Schema evolution impossible

#### 2. Security System (0% complete) - 80-100 hours
- ❌ GRANT/REVOKE - No access control
- ❌ Role management - No user permissions
- **Impact**: All users have full access to all data

#### 3. Mathematical Functions (0/40 implemented) - 30-40 hours
- ❌ No SIN, COS, TAN, SQRT, EXP, LOG, POWER, etc.
- **Impact**: Cannot perform basic math in queries

### 🟠 HIGH PRIORITY

#### 4. Foreign Key Constraints (0% enforced) - 100-140 hours
- ❌ No referential integrity enforcement
- **Impact**: Data integrity cannot be guaranteed

#### 5. Views & Sequences (0% complete) - 60-80 hours
- ❌ No CREATE VIEW, MATERIALIZED VIEW
- ❌ No CREATE SEQUENCE (auto-increment impossible)

#### 6. PSQL/SBLR Execution (10% complete) - 140-180 hours
- ❌ Triggers don't fire (CREATE works, execution doesn't)
- ❌ Stored procedures don't execute
- ❌ Bytecode generation incomplete

### 🟡 MEDIUM PRIORITY

#### 7. Advanced SQL (0% complete) - 80-110 hours
- ❌ CTEs (WITH clause)
- ❌ MERGE statement
- ❌ RETURNING clause

#### 8. Additional Functions (40 missing) - 85-125 hours
- ❌ Statistical functions
- ❌ Cryptographic functions
- ❌ XML functions

#### 9. Additional Constraints (6 missing) - 130-180 hours
- ❌ CHECK enforcement (catalog exists, evaluation stubbed)
- ❌ UNIQUE enforcement (indexes exist, hooks missing)
- ❌ DEFAULT values (parser recognizes, execution missing)
- ❌ PRIMARY KEY special handling
- ❌ Exclusion constraints
- ❌ Generated/computed columns

**TOTAL REMAINING**: 1,200-1,700 hours

---

## Detailed Breakdown

### Part 1: Built-in Functions (115-165 hours)

#### Mathematical Functions - CRITICAL (30-40 hours)
**Tasks**:
- ABS, SIGN, ROUND, CEIL, FLOOR, TRUNC, MOD, SQRT, CBRT, POWER
- SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, DEGREES, RADIANS, PI
- EXP, LN, LOG, LOG10

**Files**: `src/sblr/opcodes.h`, `src/sblr/executor.cpp`, `src/sblr/expression_evaluator.cpp`

#### Statistical Functions (25-35 hours)
**Tasks**:
- STDDEV, STDDEV_POP, STDDEV_SAMP, VARIANCE, VAR_POP, VAR_SAMP
- CORR, COVAR_POP, COVAR_SAMP, REGR_*

**Files**: `src/sblr/opcodes.h`, `src/sblr/executor.cpp`

#### Cryptographic Functions (15-20 hours)
**Tasks**:
- MD5, SHA1, SHA256, SHA512

**Files**: `src/sblr/opcodes.h`, `src/sblr/executor.cpp`, `CMakeLists.txt` (link OpenSSL)

#### XML Functions (40-50 hours)
**Tasks**:
- XMLPARSE, XMLSERIALIZE, XMLVALIDATE
- XMLTABLE, XMLEXISTS, XMLQUERY, XPATH

**Files**: `src/sblr/opcodes.h`, `src/sblr/executor.cpp`, `CMakeLists.txt` (link libxml2)

#### Advanced String Functions (15-25 hours)
**Tasks**:
- POSITION, OVERLAY, TRANSLATE, REPEAT, REVERSE, SPLIT_PART
- LPAD, RPAD, LTRIM, RTRIM, BTRIM

**Files**: `src/sblr/opcodes.h`, `src/sblr/executor.cpp`

---

### Part 2: SQL Statement Completions (420-580 hours)

#### DDL Modifications - CRITICAL (80-100 hours)
**ALTER TABLE (50-60 hours)**:
- ADD/DROP COLUMN, ALTER COLUMN type, RENAME COLUMN
- ADD/DROP CONSTRAINT, ALTER CONSTRAINT

**DROP Statements (30-40 hours)**:
- DROP TABLE/INDEX/VIEW/SEQUENCE [IF EXISTS] [CASCADE | RESTRICT]
- Dependency management

**Files**: `src/core/catalog_manager.cpp`, `src/parser/parser.cpp`, `src/sblr/executor.cpp`

#### Views (60-80 hours)
**Tasks**:
- CREATE VIEW, CREATE OR REPLACE VIEW, DROP VIEW
- Updatable views (INSERT/UPDATE/DELETE through views)
- Materialized views (CREATE, REFRESH, DROP)

**Files**: `src/core/catalog_manager.cpp`, `src/optimizer/query_planner.cpp`, `src/parser/parser.cpp`

#### Sequences (30-40 hours)
**Tasks**:
- CREATE/ALTER/DROP SEQUENCE
- NEXT VALUE FOR, CURRENT VALUE FOR, SETVAL, LASTVAL

**Files**: `src/core/catalog_manager.cpp`, `src/core/sequence.cpp` (NEW), `src/sblr/executor.cpp`

#### Security System - CRITICAL (80-100 hours)
**Tasks**:
- Permission framework (sys_privileges, sys_roles, sys_role_members tables)
- GRANT/REVOKE privileges and roles
- Permission checking hooks in executor

**Files**: `src/core/security.cpp` (NEW), `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`

#### Advanced DML (80-110 hours)
**MERGE Statement (40-50 hours)**:
- MERGE INTO ... USING ... ON ... WHEN MATCHED/NOT MATCHED

**TRUNCATE (10-15 hours)**:
- TRUNCATE TABLE with CASCADE/RESTRICT

**RETURNING Clause (15-20 hours)**:
- INSERT/UPDATE/DELETE ... RETURNING

**CTEs (15-25 hours)**:
- WITH ... SELECT, WITH RECURSIVE, MATERIALIZED hint

**Files**: `src/parser/parser.cpp`, `src/sblr/executor.cpp`, `src/optimizer/query_planner.cpp`

---

### Part 3: Constraint Implementations (230-320 hours)

#### CHECK Constraints (25-35 hours)
**Tasks**:
- Table CHECK constraint validation on INSERT/UPDATE
- Domain CHECK constraint validation

**Files**: `src/core/domain_manager.cpp`, `src/core/constraint.cpp` (NEW), `src/sblr/executor.cpp`

#### UNIQUE Constraints (30-40 hours)
**Tasks**:
- Uniqueness checking on INSERT/UPDATE
- NULL handling
- Violation error messages

**Files**: `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`, `src/core/btree.cpp`

#### DEFAULT Values (15-20 hours)
**Tasks**:
- Apply DEFAULT on INSERT when column omitted
- Support constants and function calls

**Files**: `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`

#### PRIMARY KEY Constraints (20-30 hours)
**Tasks**:
- One primary key per table enforcement
- Automatic unique index creation
- NOT NULL enforcement on PK columns

**Files**: `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`

#### FOREIGN KEY Constraints - CRITICAL (100-140 hours)
**Tasks**:
- FK catalog (MATCH FULL/PARTIAL/SIMPLE)
- INSERT/UPDATE validation
- DELETE/UPDATE validation on referenced table
- Referential actions (CASCADE, SET NULL, SET DEFAULT, RESTRICT, NO ACTION)

**Files**: `src/core/foreign_key.cpp` (NEW), `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`

#### Exclusion Constraints (50-70 hours)
**Tasks**:
- EXCLUDE USING index_method
- Check exclusion on INSERT/UPDATE
- Temporal exclusion support

**Files**: `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`, `src/core/gist_index.cpp`

#### Generated/Computed Columns (40-50 hours)
**Tasks**:
- GENERATED ALWAYS AS (expression) STORED/VIRTUAL
- Compute on INSERT, recompute on UPDATE
- Dependency tracking

**Files**: `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`

---

### Part 4: PSQL/SBLR Procedural Language (140-180 hours)

#### Stored Procedures & Functions (80-100 hours)
**Tasks**:
- Procedure/function catalog
- Complete bytecode generation (Assignment at line 3126, ELSIF at line 3165)
- Execution framework (call stack, parameters, return values)

**Files**: `src/core/catalog_manager.cpp`, `src/sblr/bytecode_generator.cpp`, `src/sblr/executor.cpp`

#### Cursors (30-40 hours)
**Tasks**:
- DECLARE cursor [SCROLL] FOR select
- OPEN, FETCH (NEXT, PRIOR, FIRST, LAST, ABSOLUTE, RELATIVE), CLOSE
- FOR variable IN cursor LOOP

**Files**: `src/sblr/bytecode_generator.cpp`, `src/sblr/executor.cpp`

#### Exception Handling (30-40 hours)
**Tasks**:
- Exception framework
- RAISE EXCEPTION/NOTICE/WARNING/INFO/DEBUG
- TRY...EXCEPT execution

**Files**: `src/sblr/bytecode_generator.cpp`, `src/sblr/executor.cpp`

#### Triggers (60-80 hours)
**Tasks**:
- Trigger catalog (CREATE, ALTER, DROP)
- BEFORE/AFTER/INSTEAD OF firing
- FOR EACH ROW/STATEMENT
- OLD/NEW row variables, OLD_TABLE/NEW_TABLE

**Files**: `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`, `src/sblr/bytecode_generator.cpp`

---

## Implementation Timeline

### With 3 Developers (Recommended)

**Phase 1A: Critical Blockers** (6-8 weeks)
- DDL & Security (160-180 hours)
- Constraints & Types (200-260 hours)
- Functions (80-110 hours)

**Phase 1B: Advanced SQL** (4-6 weeks)
- Advanced DML (80-110 hours)
- PSQL Foundation (110-140 hours)

**Phase 1C: Final Features** (4-6 weeks)
- PSQL Completion (60-80 hours)
- Advanced Constraints (50-70 hours)
- Testing (40-60 hours)

**Phase 1D: Testing & Polish** (2-4 weeks)
- Integration testing, bug fixes, optimization (120-160 hours)

**Total Timeline**: 16-24 weeks (4-6 months)

### With 2 Developers
**Total**: 22-32 weeks (5.5-8 months)

### With 1 Developer
**Total**: 44-63 weeks (11-16 months)

---

## Success Criteria

Before Phase 2 (parser separation) can begin, ALL of the following must be ✅:

### Indexes (11/11 = 100%)
- [x] All 11 index types complete and production-ready

### Data Types (86/86 = 100%)
- [x] All 86 types complete with full operation support

### Built-in Functions (100/100 = 100%)
- [x] 60 functions complete
- [ ] 40 missing functions (math, statistical, crypto, XML, string)

### SQL Statements (35/35 = 100%)
- [x] 15 statements complete
- [ ] 20 missing (ALTER, DROP, VIEW, SEQUENCE, GRANT/REVOKE, MERGE, TRUNCATE, RETURNING, CTEs)

### Constraints (10/10 = 100%)
- [x] 2 constraints complete
- [ ] 8 missing (CHECK, UNIQUE, DEFAULT, PRIMARY KEY, FOREIGN KEY, Exclusion, Generated columns)

### PSQL/SBLR (100%)
- [ ] Stored procedures (bytecode + execution)
- [ ] Functions (bytecode + execution)
- [ ] All control flow complete
- [ ] Cursors (DECLARE, OPEN, FETCH, CLOSE)
- [ ] Exception handling (RAISE, TRY...EXCEPT)
- [ ] Triggers (CREATE, execution, OLD/NEW variables)

---

## MGA Compliance

**Status**: 100% Firebird MGA Compliant ✅

**All transaction/index work MUST**:
1. Read `/MGA_RULES.md` FIRST
2. Use TIP-based visibility only
3. Maintain stable TIDs
4. Use in-place updates with back-versioning
5. Never use PostgreSQL MVCC patterns

**Violations are architecturally WRONG and must be rewritten.**

---

## After Phase 1

**Phase 2**: Parser separation into embeddable library + standalone SQL application
**Timeline**: 3-4 weeks
**Deliverable**: Complete embeddable SQL engine with multiple SQL dialect support

---

**Document Version**: 1.2
**Created**: November 3, 2025
**Updated**: November 7, 2025
**Status**: ACTIVE PLAN
**Target**: Phase 1 Complete in 4-6 months (3 developers)
