# Implementation Standards (MANDATORY)

**Last Updated**: 2024-12-19
**Status**: ABSOLUTE REQUIREMENT - Violations require immediate halt and rework

This document defines **ABSOLUTE requirements** for all implementation work in ScratchBird.
Violations of these standards are treated as critically as violating MGA_RULES.md.

**NO EXCEPTIONS. NO SHORTCUTS. EVIDENCE REQUIRED FOR EVERYTHING.**

---

## Quick Reference

**BEFORE starting ANY implementation:**
1. Read this document completely
2. Run foundation audit: `./scripts/verify_foundation.sh <feature>`
3. Read ALL relevant specifications
4. Get USER APPROVAL on approach

**BEFORE marking ANY task complete:**
1. Verify against: `/COMPLETION_VERIFICATION_CHECKLIST.md`
2. Run completion check: `./scripts/verify_completion.sh <feature>`
3. Provide ALL required evidence to user
4. Get USER APPROVAL on completion

---

## BEFORE Starting ANY Implementation

### 1. Foundation Audit (MANDATORY)

**STOP**: Do not write ANY code until foundation audit passes.

```bash
# Run this command:
./scripts/verify_foundation.sh <feature_area>

# Example:
./scripts/verify_foundation.sh dependency_tracking
```

**Manual verification checklist:**

- [ ] **Catalog Infrastructure**: Do required catalog tables exist?
  - Check: `grep "sb_<feature>" src/core/catalog_manager.cpp`
  - If missing: Add catalog creation to task list FIRST

- [ ] **Specifications**: Have you identified ALL relevant specs?
  - Search: `find docs/specifications -name "*.md" | xargs grep -l "<feature>"`
  - List all specs and read them BEFORE coding

- [ ] **Integration Points**: Are security/audit/resolver hooks needed?
  - Check: Does feature need SecurityContext? AuditLogger? UUID resolver?
  - If yes: Add integration to task list

- [ ] **Dependencies**: Does this depend on other incomplete work?
  - Check: Review `docs/findings/engine_gap_report.md`
  - If yes: Block until dependencies are complete

**If ANY item is missing: STOP and get user approval before proceeding.**

### 2. Specification Reading (MANDATORY)

**You MUST read specifications before implementing.**

Required readings vary by feature area:

| Feature Area | Required Specifications |
|--------------|------------------------|
| Catalog/Metadata | `SYSTEM_CATALOG_STRUCTURE.md`, `DDL_*.md` |
| Security/Auth | `draft_security_architecture_specification.md`, `SECURITY_*.md` |
| Storage/GC | `STORAGE_ENGINE_*.md`, `INDEX_GC_PROTOCOL.md` |
| Transactions | `FIREBIRD_TRANSACTION_MODEL_SPEC.md`, `MGA_RULES.md` |
| Parsers | `EMULATED_DATABASE_PARSER_SPECIFICATION.md`, dialect specs |
| Protocol | `wire_protocols/*.md`, gap reports in `docs/findings/` |

**After reading:**
- [ ] List all requirements from specs
- [ ] Map requirements to implementation tasks
- [ ] Identify any gaps or conflicts
- [ ] Get user approval on interpretation

### 3. Test Plan Creation (MANDATORY)

**Before writing implementation code, create test plan with ALL types:**

Required test types (NO EXCEPTIONS):

| Test Type | Purpose | Example |
|-----------|---------|---------|
| **Happy Path** | Basic functionality works | Create object, verify it exists |
| **Restart** | Data persists across restart | Create, close DB, reopen, verify |
| **Negative** | Errors handled correctly | Try invalid operation, check error |
| **Multi-Path** | All APIs work | Test via SQL, CatalogManager, executor |
| **Concurrency** | Lock ordering correct | Two transactions, same object |
| **Integration** | Security/audit wired | Verify audit log, security context |

**Test plan template:**
```markdown
## Test Plan: <Feature Name>

### Happy Path Tests
1. Test: Create <object>
2. Test: Query <object>
3. Test: Modify <object>

### Restart/Persistence Tests
1. Test: Create <object>, restart, verify exists
2. Test: Modify <object>, restart, verify changes

### Negative Tests
1. Test: Operation on non-existent object → NOT_FOUND
2. Test: Invalid operation → CONSTRAINT_VIOLATION
3. Test: Invalid arguments → INVALID_ARGUMENT

### Multi-Path Tests
1. Test: Operation via SQL
2. Test: Operation via CatalogManager API
3. Test: Operation via executor directly

### Concurrency Tests (if applicable)
1. Test: Concurrent creates
2. Test: Concurrent modifications
3. Test: Lock ordering verification

### Integration Tests
1. Test: Security context propagation
2. Test: Audit log entries created
3. Test: UUID resolver updated
```

---

## DURING Implementation

### Checkpoint System (MANDATORY)

All multi-phase work requires checkpoints with USER APPROVAL:

**Checkpoint 1: Catalog Layer Complete**
- [ ] Catalog tables implemented
- [ ] CatalogManager APIs implemented
- [ ] Save/load paths implemented
- [ ] Basic persistence test passes
- [ ] **USER APPROVAL** required before proceeding

**Checkpoint 2: Executor Integration Complete**
- [ ] SBLR opcodes implemented
- [ ] Executor handlers implemented
- [ ] SQL commands work end-to-end
- [ ] **USER APPROVAL** required before proceeding

**Checkpoint 3: Multi-Path Verification Complete**
- [ ] All access paths tested
- [ ] Negative tests pass
- [ ] Error handling verified
- [ ] **USER APPROVAL** required before proceeding

**Checkpoint 4: Integration Complete**
- [ ] Security context integrated
- [ ] Audit logging working
- [ ] UUID resolver updated
- [ ] Full regression passes
- [ ] **USER APPROVAL** required to mark complete

### Code Quality Requirements

**All code must:**
- Handle ALL enum values (no default fallthrough)
- Include error checking for all operations
- Use consistent naming conventions
- Follow existing patterns in codebase
- Include inline comments for complex logic

**Forbidden patterns:**
- Implementing only in executor (must be in CatalogManager too)
- Cache-only updates (must persist to disk)
- Happy-path-only testing
- Marking TODOs without implementing or documenting
- Skipping error cases

---

## BEFORE Marking Complete

### Completion Verification (MANDATORY)

**STOP**: Do NOT mark task complete until you verify against:

**`/COMPLETION_VERIFICATION_CHECKLIST.md`**

Run completion verification:
```bash
./scripts/verify_completion.sh <feature_area>
```

### Evidence Requirements (MANDATORY)

**You MUST provide ALL of the following to the user:**

1. **File Paths**
   - List all modified files
   - List all new files
   - Provide line numbers for major changes

2. **Test Output** (ALL test types)
   ```
   [✓] Happy path tests: 10/10 passing
   [✓] Restart tests: 3/3 passing
   [✓] Negative tests: 5/5 passing
   [✓] Multi-path tests: 4/4 passing
   [✓] Integration tests: 2/2 passing
   ```

3. **Restart Verification Output**
   ```bash
   # Show actual test output proving restart works
   ./build/tests/scratchbird_tests --gtest_filter="*Restart*"
   ```

4. **Catalog Persistence Verification**
   ```sql
   # Show catalog query proving data is stored
   SELECT name FROM sqlite_master WHERE type='table' AND name='sb_<feature>';
   ```

5. **Negative Test Output**
   ```bash
   # Show error handling works correctly
   ./build/tests/scratchbird_tests --gtest_filter="*Error*"
   ```

6. **TODO Verification**
   ```bash
   # Prove no related TODOs remain
   grep -r "TODO" src/ include/ | grep -i "<feature>" || echo "No TODOs found"
   ```

**Format for providing evidence:**
```markdown
## Completion Evidence: <Feature Name>

### Modified Files
- src/core/catalog_manager.cpp (lines 1234-1567)
- src/sblr/executor.cpp (lines 890-923)
- tests/unit/test_<feature>.cpp (new file)

### Test Results
[paste test output showing ALL test types passing]

### Restart Verification
[paste output showing data survives restart]

### Catalog Verification
[paste SQL query showing catalog table exists and has data]

### Negative Test Verification
[paste output showing proper error handling]

### TODO Verification
[paste grep output showing no related TODOs]

### User Approval Required
Please verify the above evidence before I mark this complete.
```

---

## Absolute Requirements (NO EXCEPTIONS)

### 1. Catalog Infrastructure FIRST

**Rule**: Catalog tables and persistence MUST exist before implementing features.

**Verification**:
- [ ] Catalog table DDL exists in `catalog_manager.cpp`
- [ ] Save path implemented
- [ ] Load path implemented
- [ ] Restart test verifies persistence

**If missing**: Add catalog infrastructure to task list and implement FIRST.

### 2. Test Coverage COMPLETE

**Rule**: ALL test types MUST pass before marking complete.

**Required test types:**
- [ ] Happy path
- [ ] Restart/persistence
- [ ] Negative/error
- [ ] Multi-path (SQL, API, executor)
- [ ] Concurrency (if applicable)
- [ ] Integration (security, audit, resolver)

**If any missing**: Implement missing tests before marking complete.

### 3. Multi-Path Verification

**Rule**: Feature MUST work through ALL access paths, not just executor.

**Required paths:**
- [ ] SQL commands (via executor)
- [ ] CatalogManager API (direct calls)
- [ ] Executor opcodes (bytecode)

**Verification**: Write tests that use each path independently.

### 4. Specification Compliance

**Rule**: ALL spec requirements MUST be implemented or explicitly deferred.

**Process**:
1. Read all relevant specs
2. List all requirements
3. Map each requirement to implementation
4. Document any deferred items with user approval

### 5. Parser/SBLR Spec Traceability

**Rule**: Any parser or SBLR change that affects SELECT planning/execution MUST reference the vector SELECT spec.

**Required reference** (comment or doc entry in the change):

`Spec: docs/specifications/DraftQueryOptimizationSpecification.md`

If the change makes vector planning ambiguous or incomplete, the vector path MUST fall back in `vector_select=auto`.

### 6. Evidence Provision

**Rule**: NEVER mark complete without providing ALL required evidence.

**Required evidence** (see Evidence Requirements section above):
- File paths
- Test output (all types)
- Restart verification
- Catalog verification
- Negative test verification
- TODO verification

---

## Common Failure Patterns (MUST AVOID)

See: `/docs/standards/COMMON_FAILURE_PATTERNS.md`

**Most Critical Patterns:**

### Pattern 1: Executor-Only Implementation
❌ **Wrong**: Implementing feature only in executor layer
✅ **Right**: Implement in CatalogManager, then wire to executor
**Test**: Call CatalogManager API directly

### Pattern 2: Missing Persistence
❌ **Wrong**: Cache updates without save/load paths
✅ **Right**: Persist to catalog, verify with restart test
**Test**: MANDATORY restart test

### Pattern 3: Happy-Path-Only Testing
❌ **Wrong**: Only testing success cases
✅ **Right**: Test NOT_FOUND, CONSTRAINT_VIOLATION, errors
**Test**: Negative test suite required

### Pattern 4: Incomplete Type Coverage
❌ **Wrong**: Switch with default fallthrough
✅ **Right**: Handle ALL enum values explicitly
**Test**: Grep for switch statements, verify all cases

### Pattern 5: Missing Foundation
❌ **Wrong**: Implementing without verifying catalog exists
✅ **Right**: Run foundation audit FIRST
**Test**: Catalog query must show schema

---

## Enforcement

**These standards are ABSOLUTE requirements.**

**If standards are violated:**
1. Work MUST be halted immediately
2. Violations MUST be documented
3. Work MUST be redone to meet standards
4. No exceptions

**Violations include:**
- Starting work without foundation audit
- Marking complete without all test types
- Marking complete without evidence
- Skipping specification reading
- Parser/SBLR changes affecting SELECT without spec reference
- Bypassing checkpoints without user approval

**Treatment**: Violations are as serious as violating MGA_RULES.md.

---

## Related Documents

- `/COMPLETION_VERIFICATION_CHECKLIST.md` - Pre-completion checklist
- `/docs/standards/COMMON_FAILURE_PATTERNS.md` - Detailed failure patterns
- `/docs/standards/TEST_REQUIREMENTS.md` - Detailed test requirements
- `/docs/standards/EVIDENCE_TEMPLATES.md` - Evidence format templates
- `/scripts/verify_foundation.sh` - Automated foundation verification
- `/scripts/verify_completion.sh` - Automated completion verification

---

## Questions or Clarifications

If anything in this document is unclear:
1. Ask the user for clarification
2. Do NOT proceed with assumptions
3. Document the clarification in this file
