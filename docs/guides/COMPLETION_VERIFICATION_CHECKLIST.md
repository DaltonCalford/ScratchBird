# Completion Verification Checklist

**STOP**: Do NOT mark any task complete until ALL items in this checklist are verified.

**Last Updated**: 2024-12-19

---

## How to Use This Checklist

1. **Before marking ANY task complete**, go through this checklist item by item
2. **Check each box** only after verifying with actual evidence
3. **Provide evidence to user** for each checked item
4. **Get user approval** before marking task complete

**ABSOLUTE RULE**: If even ONE item is not checked, the task is NOT complete.

---

## 1. Catalog Schema Verification

### 1.1 Catalog Table Exists
- [ ] DDL exists in `src/core/catalog_manager.cpp`
- [ ] Table creation verified with query
- [ ] Provide evidence:
  ```sql
  SELECT name FROM sqlite_master WHERE type='table' AND name='sb_<feature>';
  ```

### 1.2 Indexes Created
- [ ] Required indexes exist (hash on UUID, B-tree on name/path)
- [ ] Index creation code in `catalog_manager.cpp`
- [ ] Provide evidence:
  ```sql
  SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='sb_<feature>';
  ```

### 1.3 Save/Load Paths Exist
- [ ] Save path implemented (data written to catalog)
- [ ] Load path implemented (data read on DB open)
- [ ] Both paths tested and verified

---

## 2. Persistence Verification (MANDATORY)

### 2.1 Restart Test Exists
- [ ] Test file includes restart test
- [ ] Test closes and reopens database
- [ ] Test verifies data survives restart

### 2.2 Restart Test Passes
- [ ] Restart test runs without errors
- [ ] Data is correctly restored after restart
- [ ] Provide evidence:
  ```bash
  ./build/tests/scratchbird_tests --gtest_filter="*<Feature>*Restart*"
  ```

### 2.3 Restart Test Output Provided
- [ ] Copy/paste actual test output to user
- [ ] Output shows test PASSED
- [ ] Output shows data verified after restart

**CRITICAL**: If restart test fails or doesn't exist, task is NOT complete.

---

## 3. Multi-Path Testing (MANDATORY)

### 3.1 Executor Path Tested
- [ ] Feature works via SQL commands
- [ ] SBLR executor handlers implemented
- [ ] Test uses SQL to exercise feature

### 3.2 CatalogManager API Tested
- [ ] Feature works via direct CatalogManager calls
- [ ] Test calls CatalogManager API directly (not through executor)
- [ ] Verify: `grep "catalog->" tests/unit/test_<feature>.cpp`

### 3.3 All Paths Work
- [ ] SQL path works
- [ ] CatalogManager API path works
- [ ] Executor bytecode path works
- [ ] Provide evidence showing all paths tested

---

## 4. Negative Testing (MANDATORY)

### 4.1 NOT_FOUND Case Tested
- [ ] Test attempts operation on non-existent object
- [ ] Test expects `Status::NOT_FOUND`
- [ ] Test verifies error message is meaningful

### 4.2 CONSTRAINT_VIOLATION Tested (if applicable)
- [ ] Test attempts operation that violates constraints
- [ ] Test expects `Status::CONSTRAINT_VIOLATION`
- [ ] Test verifies error message mentions dependencies

### 4.3 INVALID_ARGUMENT Tested
- [ ] Test passes invalid/null arguments
- [ ] Test expects `Status::INVALID_ARGUMENT`
- [ ] Test verifies proper error handling

### 4.4 Negative Test Output Provided
- [ ] Copy/paste negative test output to user
- [ ] Output shows all error cases handled correctly
- [ ] Provide evidence:
  ```bash
  ./build/tests/scratchbird_tests --gtest_filter="*<Feature>*Error*"
  ./build/tests/scratchbird_tests --gtest_filter="*<Feature>*Fail*"
  ```

**CRITICAL**: If negative tests don't exist or fail, task is NOT complete.

---

## 5. Integration Verification

### 5.1 Security Context (if required)
- [ ] Feature integrated with SecurityContext
- [ ] User/role/session context checked
- [ ] Authorization verified
- [ ] Not applicable: [ ] (explain why)

### 5.2 Audit Logging (if required)
- [ ] Feature creates audit log entries
- [ ] Audit entries include session/authkey/user info
- [ ] Audit log tested and verified
- [ ] Not applicable: [ ] (explain why)

### 5.3 UUID Resolver (if required)
- [ ] Feature updates UUID resolver view
- [ ] Object UUID → name/path resolution works
- [ ] Name/path → UUID resolution works
- [ ] Not applicable: [ ] (explain why)

### 5.4 Dependency Tracking (if required)
- [ ] Dependencies recorded in `sb_dependencies`
- [ ] Dependency checks prevent invalid drops
- [ ] Dependency cleanup on drop
- [ ] Not applicable: [ ] (explain why)

---

## 6. Specification Compliance

### 6.1 Specifications Read
- [ ] List all relevant specifications read
- [ ] All spec requirements identified
- [ ] All requirements mapped to implementation

### 6.2 All Requirements Implemented
- [ ] Every spec requirement has implementation
- [ ] OR: Requirement documented as deferred with user approval
- [ ] No requirements silently skipped

### 6.3 Deviations Documented
- [ ] Any spec deviations documented with reason
- [ ] Deviations have config flags or clear documentation
- [ ] User approved all deviations

---

## 7. Type Coverage Verification

### 7.1 All Enum Values Handled
- [ ] All ObjectType values handled (if applicable)
- [ ] All IndexType values handled (if applicable)
- [ ] All other enum types handled
- [ ] No default fallthrough cases

### 7.2 Switch Statement Verification
- [ ] Grep for switch statements: `grep -n "switch.*object_type\|ObjectType" src/`
- [ ] Verify all cases handled
- [ ] No `default:` cases that silently ignore values

### 7.3 No Unknown Outputs
- [ ] Grep for `<unknown>`: `grep -r "<unknown>" src/` (feature-related)
- [ ] No code paths produce `<unknown>` for feature objects
- [ ] All object types have proper string conversion

---

## 8. TODO Verification (MANDATORY)

### 8.1 TODO Scan Run
- [ ] Run: `grep -r "TODO" src/ include/ | grep -i "<feature>"`
- [ ] List all related TODOs found

### 8.2 All TODOs Resolved
- [ ] Every related TODO either:
  - Implemented and removed, OR
  - Documented as deferred with user approval
- [ ] No TODOs silently left in code

### 8.3 TODO Verification Output Provided
- [ ] Copy/paste grep output to user
- [ ] If TODOs found: explain status of each
- [ ] If no TODOs: show output proving none exist

---

## 9. Evidence Provided to User

### 9.1 File Paths Listed
- [ ] All modified files listed with line numbers
- [ ] All new files listed
- [ ] Brief description of changes

### 9.2 Test Output Provided
- [ ] Happy path test output
- [ ] Restart test output
- [ ] Negative test output
- [ ] Multi-path test output
- [ ] Integration test output (if applicable)

### 9.3 Catalog Verification Provided
- [ ] SQL query showing catalog table exists
- [ ] SQL query showing catalog data persisted
- [ ] Output provided to user

### 9.4 All Evidence Formatted Clearly
- [ ] Use code blocks for terminal output
- [ ] Use SQL blocks for queries
- [ ] Clear headers for each evidence section

---

## 10. User Verification

### 10.1 User Reviewed Evidence
- [ ] All evidence provided to user
- [ ] User had opportunity to review
- [ ] Waiting for user approval

### 10.2 User Approved Completion
- [ ] User explicitly approved marking task complete
- [ ] User confirmed restart test passes
- [ ] User confirmed all evidence satisfactory

**CRITICAL**: Do NOT mark task complete without explicit user approval.

---

## Evidence Template

Use this template when providing evidence to user:

```markdown
## Completion Evidence: <Feature Name>

### 1. Modified Files
- `src/core/catalog_manager.cpp` (lines X-Y: catalog table, lines A-B: APIs)
- `src/sblr/executor.cpp` (lines C-D: executor handlers)
- `tests/unit/test_<feature>.cpp` (NEW: all test types)

### 2. Catalog Schema Verification
\`\`\`sql
-- Catalog table exists:
SELECT name FROM sqlite_master WHERE type='table' AND name='sb_<feature>';
-- Result: sb_<feature>

-- Indexes exist:
SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='sb_<feature>';
-- Results: sb_<feature>_uuid_hash, sb_<feature>_name_idx
\`\`\`

### 3. Test Results Summary
- [✓] Happy path tests: X/X passing
- [✓] Restart tests: Y/Y passing
- [✓] Negative tests: Z/Z passing
- [✓] Multi-path tests: N/N passing

### 4. Restart Test Output
\`\`\`bash
$ ./build/tests/scratchbird_tests --gtest_filter="*<Feature>*Restart*"
[paste actual output showing test PASSED]
\`\`\`

### 5. Negative Test Output
\`\`\`bash
$ ./build/tests/scratchbird_tests --gtest_filter="*<Feature>*Error*"
[paste actual output showing error handling works]
\`\`\`

### 6. Multi-Path Verification
\`\`\`bash
$ ./build/tests/scratchbird_tests --gtest_filter="*<Feature>*SQL*"
[output showing SQL path works]

$ ./build/tests/scratchbird_tests --gtest_filter="*<Feature>*API*"
[output showing CatalogManager API path works]
\`\`\`

### 7. TODO Verification
\`\`\`bash
$ grep -r "TODO" src/ include/ | grep -i "<feature>"
[output: either no TODOs, or list with explanations]
\`\`\`

### 8. Integration Verification (if applicable)
- Security context: [verified/not applicable]
- Audit logging: [verified/not applicable]
- UUID resolver: [verified/not applicable]
- Dependencies: [verified/not applicable]

### 9. Specification Compliance
Specifications read:
- `docs/specifications/SPEC1.md` - all requirements implemented
- `docs/specifications/SPEC2.md` - requirements X,Y,Z deferred (user approved)

### 10. User Approval Request
Please review the above evidence and approve before I mark this task complete.

Specific items to verify:
1. Restart test output shows data survives database restart
2. Negative tests show proper error handling
3. All TODO items are resolved or documented
4. Catalog queries show data is persisted

Do you approve marking this task complete?
\`\`\`

---

## Completion Declaration

**ONLY after ALL checklist items are verified and user has approved:**

```
Task <X.Y>: <Task Name> - COMPLETE

Evidence provided: <date/time>
User approval: <date/time>
All checklist items: VERIFIED
```

**If ANY item is not verified: Task is NOT complete.**

---

## Related Documents

- `/IMPLEMENTATION_STANDARDS.md` - Overall implementation standards
- `/docs/standards/COMMON_FAILURE_PATTERNS.md` - Patterns to avoid
- `/docs/standards/TEST_REQUIREMENTS.md` - Detailed test requirements
- `/scripts/verify_completion.sh` - Automated verification script
