# Evidence Templates

**Last Updated**: 2024-12-19

This document provides templates for the **MANDATORY evidence** that must be provided to the user before marking any task complete.

**ABSOLUTE REQUIREMENT**: All evidence must be provided, no exceptions.

---

## Complete Evidence Package Template

Use this template when providing completion evidence to the user:

```markdown
## Completion Evidence: <Feature Name>

Task: <Task ID> - <Task Description>
Date: <Date>
Status: Ready for user review and approval

---

### 1. Modified and New Files

**Modified Files:**
- `src/core/catalog_manager.cpp`
  - Lines XXX-YYY: Added catalog table DDL for sb_<feature>
  - Lines AAA-BBB: Implemented create<Feature>() API
  - Lines CCC-DDD: Implemented get<Feature>() API
  - Lines EEE-FFF: Implemented drop<Feature>() with dependency checks

- `src/sblr/executor.cpp`
  - Lines GGG-HHH: Added CREATE <FEATURE> handler
  - Lines III-JJJ: Added DROP <FEATURE> handler

- `include/scratchbird/core/catalog_manager.h`
  - Lines KKK-LLL: Added FeatureInfo struct
  - Lines MMM-NNN: Added public API declarations

**New Files:**
- `tests/unit/test_<feature>.cpp` (NEW - 500 lines)
  - 15 tests covering all required test types

---

### 2. Catalog Schema Verification

**Catalog Table Creation:**
\`\`\`sql
-- Verify catalog table exists:
SELECT name FROM sqlite_master WHERE type='table' AND name='sb_<feature>';

-- Result:
-- sb_<feature>
\`\`\`

**Index Creation:**
\`\`\`sql
-- Verify indexes exist:
SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='sb_<feature>';

-- Results:
-- sb_<feature>_uuid_hash
-- sb_<feature>_name_btree
\`\`\`

**Sample Data Query:**
\`\`\`sql
-- Verify data can be queried:
SELECT feature_id, schema_id, name FROM sb_<feature> LIMIT 5;

-- Results show data is correctly stored in catalog
\`\`\`

---

### 3. Test Results Summary

**All Tests Passing:**
- [✓] Happy path tests: 5/5 passing
- [✓] Restart/persistence tests: 1/1 passing
- [✓] Negative/error tests: 5/5 passing
- [✓] Multi-path tests: 3/3 passing
- [✓] Integration tests: 2/2 passing
- **Total: 16/16 tests passing**

---

### 4. Restart/Persistence Test Output

**CRITICAL: This proves data survives database restart**

\`\`\`bash
$ ./build/tests/scratchbird_tests --gtest_filter="*Feature*Restart*"

[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from FeatureTest
[ RUN      ] FeatureTest.PersistenceAcrossRestart
[       OK ] FeatureTest.PersistenceAcrossRestart (125 ms)
[----------] 1 test from FeatureTest (125 ms total)
[----------] Global test environment tear-down
[==========] 1 test from 1 test suite ran. (125 ms total)
[  PASSED  ] 1 test.
\`\`\`

**Restart test verified:**
- Creates feature data
- Closes database
- Reopens database
- Verifies data still exists
- **Status: PASSED**

---

### 5. Negative/Error Test Output

**This proves error handling works correctly**

\`\`\`bash
$ ./build/tests/scratchbird_tests --gtest_filter="*Feature*Error*:*Feature*Fail*:*Feature*Invalid*"

[==========] Running 5 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 5 tests from FeatureTest
[ RUN      ] FeatureTest.GetNonExistentFeatureReturnsNotFound
[       OK ] FeatureTest.GetNonExistentFeatureReturnsNotFound (12 ms)
[ RUN      ] FeatureTest.DropFeatureWithDependentsFailsWithConstraintViolation
[       OK ] FeatureTest.DropFeatureWithDependentsFailsWithConstraintViolation (18 ms)
[ RUN      ] FeatureTest.CreateFeatureWithInvalidArgumentsReturnsInvalidArgument
[       OK ] FeatureTest.CreateFeatureWithInvalidArgumentsReturnsInvalidArgument (8 ms)
[ RUN      ] FeatureTest.CreateDuplicateFeatureFailsWithAlreadyExists
[       OK ] FeatureTest.CreateDuplicateFeatureFailsWithAlreadyExists (15 ms)
[ RUN      ] FeatureTest.DropNonExistentFeatureReturnsNotFound
[       OK ] FeatureTest.DropNonExistentFeatureReturnsNotFound (7 ms)
[----------] 5 tests from FeatureTest (60 ms total)
[----------] Global test environment tear-down
[==========] 5 tests from 1 test suite ran. (60 ms total)
[  PASSED  ] 5 tests.
\`\`\`

**Error cases verified:**
- NOT_FOUND error handling: ✓
- CONSTRAINT_VIOLATION handling: ✓
- INVALID_ARGUMENT handling: ✓
- ALREADY_EXISTS handling: ✓
- **Status: All error cases handled correctly**

---

### 6. Multi-Path Test Output

**This proves feature works through all access paths**

\`\`\`bash
$ ./build/tests/scratchbird_tests --gtest_filter="*Feature*SQL*:*Feature*API*"

[==========] Running 3 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 3 tests from FeatureTest
[ RUN      ] FeatureTest.CreateFeatureViaSQL
[       OK ] FeatureTest.CreateFeatureViaSQL (22 ms)
[ RUN      ] FeatureTest.CreateFeatureViaCatalogAPI
[       OK ] FeatureTest.CreateFeatureViaCatalogAPI (18 ms)
[ RUN      ] FeatureTest.CreateFeatureViaExecutor
[       OK ] FeatureTest.CreateFeatureViaExecutor (20 ms)
[----------] 3 tests from FeatureTest (60 ms total)
[----------] Global test environment tear-down
[==========] 3 tests from 1 test suite ran. (60 ms total)
[  PASSED  ] 3 tests.
\`\`\`

**Access paths verified:**
- SQL command path: ✓
- CatalogManager API path: ✓
- Executor bytecode path: ✓
- **Status: All paths work correctly**

---

### 7. TODO Verification

**This proves no related TODOs remain**

\`\`\`bash
$ grep -r "TODO" src/ include/ | grep -i "<feature>"

[No output - no TODOs found]
\`\`\`

**Alternative (if TODOs found):**
\`\`\`bash
$ grep -r "TODO" src/ include/ | grep -i "<feature>"

src/core/catalog_manager.cpp:1234: // TODO: Optimize feature query performance
\`\`\`

**Status of TODOs:**
- Line 1234: Optimization TODO - not required for correctness, documented as future enhancement in issue #XXX
- User approved deferring this optimization to next release

---

### 8. Integration Verification

**Security Context Integration:**
- [✓] Feature checks user permissions via SecurityContext
- [✓] Unauthorized access returns PERMISSION_DENIED
- [✓] Test: FeatureTest.SecurityContextEnforcement passes

**Audit Logging Integration:**
- [✓] CREATE operations logged with user/session/authkey
- [✓] DROP operations logged
- [✓] Test: FeatureTest.AuditLogEntryCreated passes

**UUID Resolver Integration:**
- [✓] Feature UUID registered in resolver view
- [✓] UUID → name resolution works
- [✓] Name → UUID resolution works
- [✓] Test: FeatureTest.UUIDResolverUpdated passes

**Dependency Tracking Integration:**
- [✓] Dependencies recorded in sb_dependencies
- [✓] DROP blocked when dependents exist
- [✓] Dependencies cleaned up on DROP
- [✓] Test: FeatureTest.DependenciesTracked passes

---

### 9. Specification Compliance

**Specifications Read:**
1. `docs/specifications/DDL_FEATURES.md`
   - All requirements implemented
   - Section 3.2: CREATE FEATURE syntax - ✓ implemented
   - Section 3.3: DROP FEATURE semantics - ✓ implemented
   - Section 4.1: Dependency tracking - ✓ implemented

2. `docs/specifications/SYSTEM_CATALOG_STRUCTURE.md`
   - Section 5.7: sb_features table - ✓ implemented
   - Section 5.8: Indexes - ✓ implemented

**Deviations:**
- None - full compliance with specifications

**Requirements Map:**
- [✓] Requirement 1: CREATE FEATURE command
- [✓] Requirement 2: DROP FEATURE command
- [✓] Requirement 3: Catalog persistence
- [✓] Requirement 4: Dependency tracking
- [✓] Requirement 5: Security integration
- [✓] Requirement 6: Audit logging

---

### 10. Foundation Audit Results

**Pre-implementation foundation check:**
\`\`\`bash
$ ./scripts/verify_foundation.sh feature

======================================================================
Foundation Verification for: feature
======================================================================

1. CATALOG INFRASTRUCTURE CHECK
✓ Found catalog table references

2. SPECIFICATION CHECK
✓ Found relevant specifications

3. INTEGRATION POINTS CHECK
✓ Security context integration found
✓ Audit logging integration found
✓ UUID resolver integration found

4. EXISTING TESTS CHECK
ℹ️  No existing test file (created as part of implementation)

5. TODO SCAN
✓ No related TODOs found

6. DEPENDENCY CHECK
✓ No mentions in gap report

======================================================================
FOUNDATION VERIFICATION SUMMARY
======================================================================
Warnings: 0
Errors: 0
✓ PASSED: Foundation verification completed successfully
\`\`\`

---

### 11. Completion Verification Results

**Pre-completion check:**
\`\`\`bash
$ ./scripts/verify_completion.sh feature

======================================================================
Completion Verification for: feature
======================================================================

1. TODO CHECK
✓ PASSED: No related TODOs found

2. TEST FILE CHECK
✓ Found test file: tests/unit/test_feature.cpp

2a. Restart/Persistence Test Check
✓ PASSED: Restart/persistence test(s) found

2b. Negative Test Check
✓ PASSED: Negative test(s) found

2c. Multi-Path Test Check
✓ PASSED: Direct CatalogManager API calls found

3. CATALOG SCHEMA CHECK
✓ PASSED: Catalog references found

4. IMPLEMENTATION CHECK
✓ Found implementation in catalog_manager.cpp
✓ Found implementation in executor.cpp

5. BUILD STATUS CHECK
✓ Test binary exists

======================================================================
COMPLETION VERIFICATION SUMMARY
======================================================================
Failed checks: 0
Warnings: 0

✓ PASSED: Basic verification completed successfully
\`\`\`

---

### 12. User Approval Request

**Please review the above evidence and verify:**

1. ✓ Restart test output shows data survives database restart
2. ✓ Negative tests show proper error handling (NOT_FOUND, CONSTRAINT_VIOLATION, INVALID_ARGUMENT)
3. ✓ All TODO items are resolved
4. ✓ Catalog queries show data is persisted correctly
5. ✓ Multi-path tests show feature works via SQL, API, and executor
6. ✓ Integration tests show security/audit/resolver/dependencies work
7. ✓ All specifications have been read and requirements implemented
8. ✓ Foundation audit passed before implementation
9. ✓ Completion verification passed

**Do you approve marking this task complete?**

If yes, I will mark Task <ID> as complete.
If no, please specify what additional work is needed.

---
\`\`\`

---

## Quick Evidence Checklist

Before providing evidence, verify you have:

- [ ] Listed all modified/new files with line numbers
- [ ] Provided catalog query output showing table exists
- [ ] Provided catalog query output showing indexes exist
- [ ] Provided test summary (X/X passing for each type)
- [ ] Provided restart test output (MUST SHOW PASSING)
- [ ] Provided negative test output (MUST SHOW PASSING)
- [ ] Provided multi-path test output
- [ ] Provided TODO verification (grep output)
- [ ] Listed integration points and verification status
- [ ] Listed specifications read and compliance status
- [ ] Provided foundation audit results
- [ ] Provided completion verification results
- [ ] Asked for explicit user approval

---

## Evidence Format Guidelines

### Use Code Blocks
- Always use code blocks with language hints
- Bash commands: \`\`\`bash
- SQL queries: \`\`\`sql
- C++ code: \`\`\`cpp

### Clear Headers
- Use markdown headers (###) for each evidence section
- Use checkmarks (✓) for passed items
- Use (❌) for failed items
- Use (⚠️) for warnings

### Actual Output
- Always provide ACTUAL test output (copy/paste)
- Never provide hypothetical or summarized output
- Include timestamps when available

### Completeness
- Don't skip any sections
- If a section is "not applicable", explain why
- Provide all requested evidence, no exceptions

---

## Related Documents

- `/IMPLEMENTATION_STANDARDS.md` - Overall implementation standards
- `/COMPLETION_VERIFICATION_CHECKLIST.md` - Pre-completion checklist
- `/docs/standards/TEST_REQUIREMENTS.md` - Test requirements
- `/docs/standards/COMMON_FAILURE_PATTERNS.md` - Patterns to avoid
