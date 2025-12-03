# Work Package 4: SQL Executor - Functions

**Status:** 93% COMPLETE (13/14 tasks done)
**Priority:** P1-P2 Mixed
**Estimated Hours:** 20-28
**File:** src/sblr/executor.cpp
**Last Updated:** December 3, 2025

---

## Overview

Several SQL functions throw "not yet implemented" errors or have incomplete field support. This work package addresses scalar statistical functions, encoding functions, and EXTRACT field gaps.

---

## Tasks

### EXEC-1: STDDEV_SAMP scalar (HIGH)
**Line:** 23671
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Uses Welford's online algorithm for numerical stability
- Calculates sample standard deviation from ARRAY input
- Returns NULL for arrays with < 2 non-NULL elements

**Verification:**
- [x] STDDEV_SAMP(ARRAY[1,2,3,4,5]) returns correct value

---

### EXEC-2: STDDEV_POP scalar (HIGH)
**Line:** 23698
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Uses Welford's online algorithm for numerical stability
- Calculates population standard deviation from ARRAY input
- Returns NULL for empty arrays

**Verification:**
- [x] STDDEV_POP(ARRAY[1,2,3,4,5]) returns correct value

---

### EXEC-3: VAR_SAMP scalar (HIGH)
**Line:** 23725
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Uses Welford's online algorithm for numerical stability
- Calculates sample variance from ARRAY input
- Returns NULL for arrays with < 2 non-NULL elements

**Verification:**
- [x] VAR_SAMP(ARRAY[1,2,3,4,5]) returns correct value

---

### EXEC-4: VAR_POP scalar (HIGH)
**Line:** 23752
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Uses Welford's online algorithm for numerical stability
- Calculates population variance from ARRAY input
- Returns NULL for empty arrays

**Verification:**
- [x] VAR_POP(ARRAY[1,2,3,4,5]) returns correct value

---

### EXEC-5: CORR scalar (HIGH)
**Line:** 23779
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Calculates Pearson correlation coefficient from two ARRAY inputs
- Handles NULL elements (skips pairs where either is NULL)
- Returns NULL if < 2 valid pairs or zero variance

**Verification:**
- [x] CORR(ARRAY[1,2,3], ARRAY[2,4,6]) returns ~1.0

---

### EXEC-6: COVAR_POP scalar (HIGH)
**Line:** 23839
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Uses Welford's online algorithm for numerical stability
- Calculates population covariance from two ARRAY inputs
- Handles NULL elements (skips pairs where either is NULL)

**Verification:**
- [x] COVAR_POP(ARRAY[1,2,3], ARRAY[2,4,6]) returns correct value

---

### EXEC-7: ENCODE function (MEDIUM)
**Line:** 25048
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- ENCODE(data bytea, format text) -> text
- Supports 'base64', 'hex', 'escape' formats
- Base64 uses standard encoding with padding

**Verification:**
- [x] ENCODE('hello'::bytea, 'base64') returns 'aGVsbG8='
- [x] ENCODE('hello'::bytea, 'hex') returns '68656c6c6f'

---

### EXEC-8: DECODE function (MEDIUM)
**Line:** 25111
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- DECODE(text, format text) -> bytea
- Supports 'base64', 'hex', 'escape' formats
- Handles padding and whitespace in input

**Verification:**
- [x] DECODE('aGVsbG8=', 'base64') returns 'hello'::bytea

---

### EXEC-M1: EXTRACT UUID fields (MEDIUM)
**Line:** ~24940
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Added CLOCK_SEQ field extraction for UUID v1
- Added NODE field extraction for UUID v1 (returns MAC address string)
- Existing fields: VERSION, VARIANT

**Verification:**
- [x] EXTRACT(CLOCK_SEQ FROM uuid) returns clock sequence for v1
- [x] EXTRACT(NODE FROM uuid) returns MAC address for v1

---

### EXEC-M2: EXTRACT ARRAY fields (MEDIUM)
**Line:** ~24970
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Added DIMS field extraction (returns array of dimension sizes)
- Existing fields: CARDINALITY, NDIMS, LOWER, UPPER

**Verification:**
- [x] EXTRACT(DIMS FROM array) returns dimension sizes

---

### EXEC-L1: GRANT WITH ADMIN (LOW)
**Line:** 20479
**Status:** [ ] BLOCKED - Requires parser changes

**Current Code:**
```cpp
// Phase 2 Enhancement: WITH ADMIN OPTION requires bytecode generator update
bool with_admin_option = false;  // Hardcoded
```

**Required Changes:**
1. Add `with_admin_option_` to GrantRoleStmt AST node
2. Update parser grammar to parse "WITH ADMIN OPTION"
3. Update bytecode generator to emit WITH ADMIN flag
4. Read flag from bytecode stream in executor

**Blocking Issue:**
This requires changes across parser, AST, bytecode generator, and executor.
Deferred to WP-6 (Parser/Bytecode) work package.

---

### EXEC-L2: SQL LIKE pattern matching (LOW)
**Line:** 106-161 (helper function), ~20944 (usage)
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Added `matchSqlLike()` helper function
- Implements full SQL LIKE semantics:
  - `%` matches any sequence of characters
  - `_` matches any single character
  - `\` escape character support
- Used by SHOW TABLES LIKE and similar commands

**Verification:**
- [x] SHOW TABLES LIKE 'test%' works correctly

---

### EXEC-L3: ST_AsWKT multi-geometry (LOW)
**Line:** ~13352
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Extended GEOMETRYCOLLECTION WKT output to handle:
  - MULTIPOINT
  - MULTILINESTRING
  - MULTIPOLYGON
  - Nested GEOMETRYCOLLECTION

**Verification:**
- [x] ST_AsWKT(multipolygon) returns valid WKT

---

### EXEC-L4: EXTRACT unsupported types (LOW)
**Line:** 25041
**Status:** [x] COMPLETE - December 3, 2025

**Implementation:**
- Improved error message to show actual type and list supported types
- Error now reads: "EXTRACT not supported for <TYPE> type. Supported types: DATE, TIME, TIMESTAMP, INTERVAL, UUID, ARRAY, GEOMETRY (POINT)"

**Verification:**
- [x] Clear error for unsupported combinations

---

## Completion Summary

| Status | Count | Items |
|--------|-------|-------|
| ✅ Complete | 13 | EXEC-1 to EXEC-8, EXEC-M1, EXEC-M2, EXEC-L2, EXEC-L3, EXEC-L4 |
| ⏸️ Blocked | 1 | EXEC-L1 (requires parser changes) |

---

## Completion Checklist

- [x] 13/14 tasks implemented (93%)
- [ ] EXEC-L1 blocked on parser changes (WP-6)
- [x] All 1053 existing tests pass
- [x] Code compiles without warnings

---

**Last Updated:** December 3, 2025
