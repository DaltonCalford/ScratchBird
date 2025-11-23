# Security Implementation Plan - Major Updates
**Date:** November 10, 2025
**Status:** Planning Complete - Ready to Begin Phase 3

---

## Summary of Changes

The Alpha Advanced Security Implementation Plan has been significantly enhanced based on architectural insights. Two critical features have been added that were missing from the original plan.

---

## What Was Added

### 1. Query Plan Security Integration (Phase 3.0) ✅

**Why Added:** User insight - "identify all security details at the time the query is prepared"

**What It Does:**
- Moves permission checks from **execution time** to **planning time**
- Checks permissions **once during planning** instead of per-row
- Pre-compiles RLS policies (once, not per-row)
- Fails fast with better error messages

**Performance Impact:**
- Current: 60,001 security operations for 10K row query
- New: 10,008 security operations
- **Speedup: 10-100x for permission-heavy queries**

**Duration:** 11-17 hours

**Files:**
- New: `security_analyzer.h/cpp` (~400 lines)
- Modified: Bytecode generator, Executor

---

### 2. Group Membership Caching (Part of Phase 3.0) ✅

**Why Added:** User question - "would it be good to also cache group membership?"

**What It Does:**
- Cache user's group memberships once per transaction
- Permission checks use cached groups (not catalog lookups)
- Groups are checked alongside roles in permission resolution

**Performance Impact:**
- Without: 1-2ms catalog lookup per permission check
- With: < 1μs memory lookup
- **Speedup: 1000-2000x for group lookups**

**Duration:** 2-3 hours (part of 3.0.2)

**Files:**
- `connection_context.h/cpp` - Add getUserGroups()
- `catalog_manager.h/cpp` - Implement getUserGroups()

---

### 3. SQL Object Permissions (Phase 3.1) ✅

**Why Added:** User requirement - "grant permissions to other sql objects ie grant select on table [tablename] to procedure [procedurename]"

**What It Does:**
- Grant permissions TO procedures/functions/views/triggers
- Implements **ownership chaining** (definer rights)
- Security context stack for nested procedure calls
- SQL SECURITY DEFINER/INVOKER support

**SQL Syntax:**
```sql
-- Grant table permission to procedure
GRANT SELECT ON TABLE employees TO PROCEDURE get_employee_salary;

-- User only needs EXECUTE permission
GRANT EXECUTE ON PROCEDURE get_employee_salary TO alice;

-- Alice can call procedure, which accesses employees
-- Alice does NOT need direct SELECT on employees!
```

**Duration:** 14-21 hours

**Files:**
- Catalog: Add security_type, owner_id columns
- ConnectionContext: Add security context stack
- SecurityAnalyzer: Check object permissions first
- Executor: Push/pop context on procedure calls

---

## Updated Timeline

### Original Plan
- **30-43 hours** - Column permissions, RLS, policy-based access

### Updated Plan
- **Phase 3.0:** Query Plan Security (11-17 hours) - NEW
- **Phase 3.1:** SQL Object Permissions (14-21 hours) - NEW
- **Phase 3.3:** Column Permissions (10-15 hours) - Same
- **Phase 3.4:** Row-Level Security (15-20 hours) - Same
- **Phase 3.5:** Policy-Based Access (8-12 hours - optional) - Same

**Total: 50-73 hours** (was 30-43 hours)

**Net increase: ~20-30 hours, but delivers:**
- 10-100x better performance
- Required feature for stored procedures
- Better architecture and fail-fast behavior

---

## Permission Check Flow (Updated)

### Old Flow
```
Execute Query
  ↓
For each row:
  Check user permissions (catalog lookup)
  Check role permissions (catalog lookup)
  Check group permissions (catalog lookup)  ← SLOW!
  Check PUBLIC permissions (catalog lookup)
```

### New Flow
```
Plan Query
  ↓
Load security context (ONCE):
  - User ID
  - Role IDs (cached)
  - Group IDs (cached)  ← NEW!
  - Active role ID
  ↓
Check permissions (ONCE):
  1. If in SQL object context:
     a. Check object's permissions     ← NEW!
     b. Check definer's permissions    ← NEW!
  2. Check user's permissions
  3. Check role permissions (from cache)
  4. Check group permissions (from cache)  ← NEW!
  5. Check PUBLIC permissions
  ↓
Generate bytecode with SecurityPlan
  ↓
Execute
  ↓
Apply pre-validated permissions (no lookups!)
```

---

## Implementation Priority

### Week 1 (CRITICAL)
1. Query Plan Security (Phase 3.0) - Foundation for everything
2. SQL Object Permissions (Phase 3.1) - Required for procedures

### Week 2-3 (HIGH PRIORITY)
3. Column Permissions (Phase 3.3)
4. Row-Level Security (Phase 3.4)

### Week 4 (OPTIONAL)
5. Policy-Based Access (Phase 3.5)

---

## Key Architectural Improvements

### 1. Fail Fast
```sql
-- Old: Parse succeeds, fails during execution
SELECT * FROM secret_table;  -- Error after scanning rows

-- New: Fails immediately during planning
SELECT * FROM secret_table;  -- Error before execution starts
```

### 2. Ownership Chaining
```sql
-- Setup
CREATE PROCEDURE get_salary(emp_id INT) SQL SECURITY DEFINER
AS BEGIN
    SELECT salary FROM employees WHERE id = emp_id;
END;

GRANT SELECT ON TABLE employees TO PROCEDURE get_salary;
GRANT EXECUTE ON PROCEDURE get_salary TO alice;

-- Alice executes
CALL get_salary(123);  -- ✓ Works!

-- Alice tries direct access
SELECT * FROM employees;  -- ✗ Denied!
```

### 3. Performance at Scale
```
Query with 10,000 rows, 4 columns, RLS policies:

Old approach:
- Permission checks: 10,000
- RLS compilations: 10,000
- Column checks: 40,000
- Total: 60,001 operations (~600ms)

New approach:
- Permission checks: 1
- RLS compilations: 1
- Column checks: 4
- RLS evaluations: 10,000
- Total: 10,008 operations (~8ms)

Speedup: 75x faster!
```

---

## Related Documents

1. **Implementation Plan:** `/docs/planning/ALPHA_ADVANCED_SECURITY_IMPLEMENTATION_PLAN.md`
2. **Query Plan Design:** `/docs/planning/QUERY_PLAN_SECURITY_INTEGRATION.md`
3. **Object Permissions Design:** `/docs/planning/SQL_OBJECT_PERMISSIONS_DESIGN.md`
4. **Connection Context Integration:** `/docs/status/CONNECTION_CONTEXT_SECURITY_INTEGRATION_2025-11-10.md`

---

## Next Steps

**Ready to begin implementation!**

**Recommended start:** Phase 3.0.1 - Security Analyzer Component

Would you like to proceed with implementing Phase 3.0 (Query Plan Security)?

---

**Document Version:** 1.0
**Last Updated:** November 10, 2025
**Status:** Planning Complete ✅
