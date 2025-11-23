# SCRATCHBIRD ALPHA COMPLETION - PLANNING INDEX

**Date**: October 16, 2025
**Purpose**: Navigation guide for all Alpha planning and audit documents

---

## QUICK START

**New to the project?** Read documents in this order:
1. [`AUDIT_SUMMARY_OCT_16_2025.md`](#audit-summary) - 5 min read, current status
2. [`ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md`](#comprehensive-analysis) - 15 min read, high-level roadmap
3. [`ALPHA_ISSUES_TRACKER.md`](#issues-tracker) - 10 min read, critical fixes needed
4. Detailed TODO parts 1-3 - reference as needed for implementation

---

## DOCUMENT OVERVIEW

### 1. Audit Reports

#### AUDIT_SUMMARY_OCT_16_2025.md
- **Purpose**: Executive summary of October 16, 2025 code audit
- **Size**: 2,000 words (5 min read)
- **Contents**:
  - Current status: B+ grade (83/100)
  - Top 5 critical issues to fix this week
  - Feature completeness: 88%
  - Timeline and blockers
  - Quick reference tables
- **Audience**: Project managers, developers, QA
- **When to read**: Start here for current project status

#### ALPHA_ISSUES_TRACKER.md
- **Purpose**: Track all identified issues with priorities and sprint planning
- **Size**: 2,000 words (10 min read)
- **Contents**:
  - 5 CRITICAL issues (fix this week)
  - 8 HIGH priority issues (next sprint)
  - 7 MEDIUM priority issues (monitor)
  - 1 LOW priority issue
  - Testing requirements per issue
  - CI/CD enhancements needed
  - Blockers for Beta release
- **Audience**: Developers, QA engineers
- **When to read**: Before starting bug fixes, for sprint planning

#### ALPHA_FINAL_COMPREHENSIVE_AUDIT.md
- **Purpose**: Full detailed audit of entire codebase
- **Size**: 14,000+ words (45 min read)
- **Contents**:
  - Memory management analysis (9.5/10 score)
  - Thread safety analysis (3 CRITICAL issues)
  - Error handling review (5 CRITICAL issues)
  - Buffer overflow protection
  - Feature implementation status (88% complete)
  - TODO/FIXME summary (47 markers)
  - Detailed findings by category
  - Appendices with tables and matrices
- **Audience**: Lead developers, architects, auditors
- **When to read**: For deep technical review, design decisions

---

### 2. Alpha Completion Planning

#### ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md
- **Purpose**: High-level roadmap for completing Alpha with full Firebird compatibility
- **Size**: 39 KB (15 min read)
- **Contents**:
  - **227 features identified**:
    - 89 CRITICAL
    - 76 HIGH
    - 45 MEDIUM
    - 17 LOW
  - **12 phases**, 52.5 weeks of effort
  - Gap analysis: current vs. required functionality
  - Timeline scenarios:
    - Solo developer: 12-14 months
    - 2 developers: 7-9 months
    - 3 developers: 5-7 months
  - Critical gaps and priorities
  - Success criteria
- **Audience**: Project leads, architects, management
- **When to read**: For strategic planning, resource allocation, timeline estimates

#### ALPHA_COMPLETION_DETAILED_TODO.md (Part 1)
- **Purpose**: Detailed actionable tasks for Phases 1-6
- **Size**: 55 KB (30 min read)
- **Contents**:
  - **Phase 1: Core DML Execution** (30 days, 18 features)
    - UPDATE, DELETE parsing and execution
    - JOIN operations (INNER, LEFT, RIGHT, FULL, CROSS)
    - ORDER BY, LIMIT, OFFSET
  - **Phase 2: Aggregation & Grouping** (17.5 days, 13 features)
    - GROUP BY, HAVING
    - Aggregates: COUNT, SUM, AVG, MIN, MAX
    - DISTINCT
  - **Phase 3: Subqueries & Set Operations** (20 days, 9 features)
    - Scalar, IN, EXISTS subqueries
    - UNION, INTERSECT, EXCEPT
  - **Phase 4: DDL Expansion** (27.5 days, 31 features)
    - ALTER TABLE (add/drop/modify columns, constraints)
    - DROP statements
    - CREATE INDEX, VIEW, SEQUENCE
  - **Phase 5: Constraints & Indexes** (25 days, 10 features)
    - PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK
    - Constraint enforcement
    - Index-backed query execution
  - **Phase 6: PSQL Basics** (30 days, 22 features)
    - Variables, IF/WHILE/FOR
    - Cursors, exception handling
    - RETURN, EXIT, CONTINUE
- **Format**: Each feature includes:
  - Feature ID, priority, status, component, effort estimate
  - Dependencies, files to modify, specific tasks, acceptance criteria
- **Audience**: Developers implementing features
- **When to read**: During Phases 1-6 implementation

#### ALPHA_COMPLETION_DETAILED_TODO_PART2.md
- **Purpose**: Detailed actionable tasks for Phases 7-9
- **Size**: 37 KB (25 min read)
- **Contents**:
  - **Phase 7: Stored Procedures & Triggers** (27.5 days, 22 features)
    - CREATE PROCEDURE, CALL statement
    - CREATE FUNCTION, function calls in expressions
    - CREATE TRIGGER, BEFORE/AFTER, INSERT/UPDATE/DELETE
  - **Phase 8: Advanced Features** (25 days, 12 features)
    - WITH clause (CTEs), WITH RECURSIVE
    - Window functions: OVER, PARTITION BY, ROW_NUMBER, RANK, LEAD, LAG
  - **Phase 9: Built-in Functions** (17.5 days, 35 features)
    - Math: ABS, SIGN, CEIL, FLOOR, ROUND, SQRT, POWER, trigonometry
    - String: UPPER, LOWER, SUBSTRING, POSITION, TRIM, LPAD, RPAD, REPLACE
    - Date/Time: CURRENT_TIMESTAMP, EXTRACT, DATEADD, DATEDIFF
    - Conditional: COALESCE, NULLIF, IIF, DECODE
    - Conversion: CAST, CONVERT
- **Audience**: Developers implementing features
- **When to read**: During Phases 7-9 implementation

#### ALPHA_COMPLETION_DETAILED_TODO_PART3.md
- **Purpose**: Detailed actionable tasks for Phases 10-12 + Beta roadmap
- **Size**: 35 KB (25 min read)
- **Contents**:
  - **Phase 10: System Tables & Metadata** (12.5 days, 8 features)
    - Firebird-compatible RDB$ system tables
    - INFORMATION_SCHEMA views
    - Metadata query functions
    - Sequence/generator support
    - View, domain, collation support
  - **Phase 11: Query Optimizer** (17.5 days, 10 features)
    - Statistics collection (ANALYZE)
    - Cardinality estimation
    - Cost model and index selection
    - Join order optimization
    - Predicate/projection pushdown
    - Query plan caching
  - **Phase 12: Embedded Engine Integration** (12.5 days, 6 features)
    - Embedded engine API (C++)
    - ResultSet, PreparedStatement
    - Firebird SQL parser integration
    - ISQL command-line tool
    - SQL script execution
  - **Final Alpha Checklist**: All deliverables and success criteria
  - **Beta Roadmap Preview**: Network layer, WAL, replication, enterprise features (38 weeks)
- **Audience**: Developers, architects, project managers
- **When to read**: During Phases 10-12 implementation, for Beta planning

---

## NAVIGATION BY ROLE

### For Project Managers
1. Start: [`AUDIT_SUMMARY_OCT_16_2025.md`](#audit-summary) - current status
2. Read: [`ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md`](#comprehensive-analysis) - roadmap and timeline
3. Track: [`ALPHA_ISSUES_TRACKER.md`](#issues-tracker) - sprint planning
4. Reference: [`ALPHA_COMPLETION_DETAILED_TODO_PART3.md`](#alpha_completion_detailed_todo_part3md) - final checklist and success criteria

### For Developers (Implementing Features)
1. Start: [`ALPHA_ISSUES_TRACKER.md`](#issues-tracker) - fix critical bugs first
2. Read: [`ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md`](#comprehensive-analysis) - understand overall architecture
3. Implement:
   - Phases 1-6: [`ALPHA_COMPLETION_DETAILED_TODO.md`](#alpha_completion_detailed_todomd-part-1)
   - Phases 7-9: [`ALPHA_COMPLETION_DETAILED_TODO_PART2.md`](#alpha_completion_detailed_todo_part2md)
   - Phases 10-12: [`ALPHA_COMPLETION_DETAILED_TODO_PART3.md`](#alpha_completion_detailed_todo_part3md)
4. Reference: [`ALPHA_FINAL_COMPREHENSIVE_AUDIT.md`](#alpha_final_comprehensive_auditmd) - deep technical details

### For QA Engineers
1. Start: [`ALPHA_ISSUES_TRACKER.md`](#issues-tracker) - testing requirements per issue
2. Read: [`AUDIT_SUMMARY_OCT_16_2025.md`](#audit-summary) - CI/CD requirements
3. Reference: [`ALPHA_FINAL_COMPREHENSIVE_AUDIT.md`](#alpha_final_comprehensive_auditmd) - test coverage gaps
4. Track: [`ALPHA_COMPLETION_DETAILED_TODO_PART3.md`](#alpha_completion_detailed_todo_part3md) - final checklist

### For Architects / Tech Leads
1. Read: [`ALPHA_FINAL_COMPREHENSIVE_AUDIT.md`](#alpha_final_comprehensive_auditmd) - full technical audit
2. Review: [`ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md`](#comprehensive-analysis) - gap analysis and architecture
3. Plan: All detailed TODO parts for implementation details
4. Future: [`ALPHA_COMPLETION_DETAILED_TODO_PART3.md`](#alpha_completion_detailed_todo_part3md) - Beta roadmap

---

## NAVIGATION BY TASK

### I need to fix bugs
→ [`ALPHA_ISSUES_TRACKER.md`](#issues-tracker)
- 5 CRITICAL issues with line numbers and fix instructions
- 8 HIGH priority issues
- Effort estimates per issue

### I need to implement new features
→ Find your phase in detailed TODO documents:
- **Core DML** (UPDATE, DELETE, JOIN): Part 1, Phase 1
- **Aggregation** (GROUP BY, COUNT, SUM): Part 1, Phase 2
- **Subqueries** (IN, EXISTS, UNION): Part 1, Phase 3
- **DDL** (ALTER TABLE, DROP, CREATE INDEX): Part 1, Phase 4
- **Constraints** (PK, FK, CHECK): Part 1, Phase 5
- **PSQL** (IF, WHILE, FOR, cursors): Part 1, Phase 6
- **Procedures/Triggers**: Part 2, Phase 7
- **CTEs/Windows**: Part 2, Phase 8
- **Built-in Functions**: Part 2, Phase 9
- **System Tables**: Part 3, Phase 10
- **Optimizer**: Part 3, Phase 11
- **Embedded API/ISQL**: Part 3, Phase 12

### I need timeline estimates
→ [`ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md`](#comprehensive-analysis)
- Solo: 12-14 months
- 2 developers: 7-9 months
- 3 developers: 5-7 months

### I need to know what's already done
→ [`AUDIT_SUMMARY_OCT_16_2025.md`](#audit-summary)
- Feature completeness: 88%
- Fully implemented: 10 components
- Partially implemented: 7 components

### I need to set up CI/CD
→ [`ALPHA_ISSUES_TRACKER.md`](#issues-tracker), section "CI/CD Enhancements Needed"
- ThreadSanitizer (TSAN)
- Helgrind
- AddressSanitizer (ASan)
- Clang-tidy
- Resource leak detection

### I need to plan the Beta release
→ [`ALPHA_COMPLETION_DETAILED_TODO_PART3.md`](#alpha_completion_detailed_todo_part3md), section "Post-Alpha: Beta Roadmap Preview"
- 6 Beta phases, 38 weeks
- Network layer, WAL, replication, enterprise features

---

## FEATURE CROSS-REFERENCE

### Storage & Transactions
- ✅ **MGA (Multi-Generational Architecture)**: Fully implemented (Phases 1-4 complete)
- ✅ **MVCC (Multi-Version Concurrency Control)**: Fully implemented
- ✅ **Transaction Manager**: Fully implemented (4 isolation levels, group commit)
- ✅ **Buffer Pool**: Fully implemented (LRU-K, pin/unpin)
- ✅ **Indexes**: B-Tree, Hash, GIN, Bitmap implemented

### SQL Features
- ⚠️ **SELECT**: Basic implemented, JOINs/subqueries/GROUP BY pending (Phases 1-3)
- ⚠️ **INSERT**: Implemented
- ❌ **UPDATE**: Pending (Phase 1, DML-001 to DML-003)
- ❌ **DELETE**: Pending (Phase 1, DML-004 to DML-006)
- ❌ **JOINs**: Pending (Phase 1, DML-007 to DML-011)
- ❌ **GROUP BY**: Pending (Phase 2, AGG-003)
- ❌ **Subqueries**: Pending (Phase 3, SUB-001 to SUB-004)
- ❌ **CTEs**: Pending (Phase 8, CTE-001 to CTE-006)
- ❌ **Window Functions**: Pending (Phase 8, WIN-001 to WIN-006)

### DDL
- ⚠️ **CREATE TABLE**: Implemented
- ❌ **ALTER TABLE**: Pending (Phase 4, DDL-001 to DDL-015)
- ❌ **DROP TABLE**: Pending (Phase 4, DDL-016)
- ⚠️ **CREATE INDEX**: Implemented for B-Tree/Hash/GIN
- ❌ **CREATE VIEW**: Pending (Phase 10, SYS-005)
- ❌ **CREATE SEQUENCE**: Pending (Phase 10, SYS-004)
- ❌ **CREATE DOMAIN**: Pending (Phase 10, SYS-006)

### Constraints
- ❌ **PRIMARY KEY**: Catalog support exists, enforcement pending (Phase 5, CON-001)
- ❌ **FOREIGN KEY**: Pending (Phase 5, CON-003 to CON-005)
- ❌ **UNIQUE**: Pending (Phase 5, CON-002)
- ❌ **CHECK**: Pending (Phase 5, CON-006 to CON-007)
- ❌ **NOT NULL**: Pending (Phase 5, CON-008)

### PSQL (Procedural SQL)
- ❌ **Variables**: Pending (Phase 6, PSQL-001 to PSQL-003)
- ❌ **IF/WHILE/FOR**: Pending (Phase 6, PSQL-004 to PSQL-006)
- ❌ **Cursors**: Pending (Phase 6, PSQL-008 to PSQL-011)
- ❌ **Exception Handling**: Pending (Phase 6, PSQL-012 to PSQL-015)
- ❌ **Stored Procedures**: Pending (Phase 7, PROC-001 to PROC-007)
- ❌ **Functions**: Pending (Phase 7, FUNC-001 to FUNC-007)
- ❌ **Triggers**: Pending (Phase 7, TRIG-001 to TRIG-008)

### Built-in Functions
- ✅ **Type System**: 30+ types implemented
- ❌ **Math Functions**: Pending (Phase 9, FUNC-100 to FUNC-101)
- ❌ **String Functions**: Pending (Phase 9, FUNC-102 to FUNC-105)
- ❌ **Date/Time Functions**: Pending (Phase 9, FUNC-106 to FUNC-108)
- ❌ **Conditional Functions**: Pending (Phase 9, FUNC-109)
- ❌ **Conversion Functions**: Pending (Phase 9, FUNC-110)

### Query Optimizer
- ❌ **Statistics Collection**: Pending (Phase 11, OPT-001)
- ❌ **Cardinality Estimation**: Pending (Phase 11, OPT-002 to OPT-003)
- ❌ **Cost Model**: Pending (Phase 11, OPT-004)
- ❌ **Index Selection**: Pending (Phase 11, OPT-005)
- ❌ **Join Order Optimization**: Pending (Phase 11, OPT-006)
- ❌ **Predicate Pushdown**: Pending (Phase 11, OPT-008)

### Embedded Engine & Tools
- ❌ **Embedded API**: Pending (Phase 12, EMB-001 to EMB-003)
- ❌ **Firebird Parser**: Pending (Phase 12, EMB-004)
- ❌ **ISQL Tool**: Pending (Phase 12, EMB-005 to EMB-006)

---

## ISSUE QUICK REFERENCE

### Critical Issues (Fix This Week)
| ID | Issue | File | Effort |
|----|-------|------|--------|
| CRITICAL-1 | BufferPool Frame Metadata Race | buffer_pool.cpp:101-108 | 2-4h |
| CRITICAL-2 | TransactionManager Cache Corruption | transaction_manager.cpp:548-552 | 1-2h |
| CRITICAL-3 | Lock Ordering Inconsistency | transaction_manager.cpp:699-744 | 4-6h |
| ERROR-CRITICAL-1 | Missing Unpin on Allocation Failure | heap_page.cpp:741 | 1h |
| ERROR-CRITICAL-2 | Limited Exception Handling | Multiple files | 8-12h |

**Total Effort**: 16-25 hours (2-3 days)

See [`ALPHA_ISSUES_TRACKER.md`](#issues-tracker) for full details.

---

## EFFORT SUMMARY

### By Phase
| Phase | Description | Days | Weeks |
|-------|-------------|------|-------|
| 1 | Core DML Execution | 30 | 6 |
| 2 | Aggregation & Grouping | 17.5 | 3.5 |
| 3 | Subqueries & Set Operations | 20 | 4 |
| 4 | DDL Expansion | 27.5 | 5.5 |
| 5 | Constraints & Indexes | 25 | 5 |
| 6 | PSQL Basics | 30 | 6 |
| 7 | Stored Procedures & Triggers | 27.5 | 5.5 |
| 8 | Advanced Features | 25 | 5 |
| 9 | Built-in Functions | 17.5 | 3.5 |
| 10 | System Tables & Metadata | 12.5 | 2.5 |
| 11 | Query Optimizer | 17.5 | 3.5 |
| 12 | Embedded Engine Integration | 12.5 | 2.5 |
| **TOTAL** | **All Phases** | **262.5** | **52.5** |

### By Priority (227 features)
- **CRITICAL**: 89 features
- **HIGH**: 76 features
- **MEDIUM**: 45 features
- **LOW**: 17 features

---

## SUCCESS METRICS

### Alpha Complete When:
- [x] Storage engine: 100% ✅
- [x] Transaction system: 100% ✅
- [ ] SQL coverage: 100% (currently 25%) ⏳
- [ ] Firebird compatibility: 100% (currently 20%) ⏳
- [ ] Test coverage: ≥60% (currently 40%) ⏳
- [ ] All critical issues resolved ⏳
- [ ] ISQL tool operational ⏳
- [ ] Embedded API stable ⏳

### Beta Ready When:
- [ ] All Alpha criteria met
- [ ] 6/8 high-priority issues resolved
- [ ] Network layer implemented
- [ ] WAL implemented
- [ ] Replication implemented

---

## FILE LOCATIONS

All planning documents are in `/home/dcalford/CliWork/ScratchBird/docs/audit/`:

```
docs/audit/
├── AUDIT_SUMMARY_OCT_16_2025.md                    # Executive summary
├── ALPHA_ISSUES_TRACKER.md                          # Issue tracking
├── ALPHA_FINAL_COMPREHENSIVE_AUDIT.md               # Full audit (14K words)
├── ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md       # High-level roadmap
├── ALPHA_COMPLETION_DETAILED_TODO.md                # Phases 1-6 details
├── ALPHA_COMPLETION_DETAILED_TODO_PART2.md          # Phases 7-9 details
├── ALPHA_COMPLETION_DETAILED_TODO_PART3.md          # Phases 10-12 + Beta
└── ALPHA_PLANNING_INDEX.md                          # This file
```

---

## NEXT STEPS

### Immediate (This Week)
1. ✅ Review all planning documents
2. ⏳ Fix 5 critical issues from [`ALPHA_ISSUES_TRACKER.md`](#issues-tracker)
3. ⏳ Set up CI/CD (TSAN, Helgrind, ASan)
4. ⏳ Create project tracking board (GitHub Projects / Jira)

### Short-term (Next 2 Weeks)
1. ⏳ Begin Phase 1: Core DML Execution
2. ⏳ Fix 8 high-priority issues
3. ⏳ Increase test coverage to 50%
4. ⏳ Document lock ordering protocol

### Medium-term (2-3 Months)
1. ⏳ Complete Phases 1-3 (core SQL)
2. ⏳ Complete Phases 4-6 (DDL + PSQL)
3. ⏳ Test coverage to 60%
4. ⏳ Alpha feature freeze

### Long-term (6-12 Months)
1. ⏳ Complete Phases 7-12
2. ⏳ Alpha release
3. ⏳ Begin Beta development
4. ⏳ Production-ready release (Q4 2026)

---

**Last Updated**: October 16, 2025
**Total Documentation**: 166 KB across 8 files
**Total Features Planned**: 227
**Estimated Effort**: 262.5 days (52.5 weeks)

**Questions?** Contact project lead or refer to specific documents above.
