# Security Phase 3.0 - Phase 2 Completion Tasks

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: November 11, 2025
**Status**: Ready to Start
**Estimated Effort**: 12-18 hours
**Priority**: HIGH (must complete before Phase 3.1+ features)

---

## Overview

Phase 3.0 consists of 6 small tasks that complete the Security Phase 2 implementation. These tasks address TODOs and minor gaps identified during the Phase 2 implementation review.

**Why Phase 3.0?**
- Phase 2 is functionally complete (parser → bytecode → executor)
- These tasks make Phase 2 production-ready
- Required before starting Phase 3.1+ advanced features

---

## Task List

### ✅ Task 3.0.1: Password Hashing (2-3 hours)

**Current**: Placeholder `"hashed_" + password`
**Required**: bcrypt or argon2 integration
**Priority**: HIGH (security vulnerability)

**Files**:
- `src/sblr/executor.cpp` (lines 12456-12467, 12519-12520)
- `CMakeLists.txt` (add bcrypt dependency)

**Implementation**: See `/docs/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md:1228-1273`

---

### ✅ Task 3.0.2: Integration Test API Update (10 minutes)

**Current**: Test uses old Parser API
**Required**: Update to Lexer + StringPool pattern

**Files**:
- `tests/integration/test_security_phase2.cpp`

**Pattern**: See `docs/archive/phase1_standalone_tests/test_aggregation_execution.cpp`

**Implementation**: See `/docs/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md:1276-1306`

---

### ✅ Task 3.0.3: ALTER USER Superuser Flag (1 hour)

**Current**: `updateUser()` doesn't support changing superuser flag
**Required**: Extend catalog API

**Files**:
- `include/scratchbird/core/catalog_manager.h` (line ~800)
- `src/core/catalog_manager.cpp` (UserRecord update)
- `src/sblr/executor.cpp` (line 12523-12525)

**Implementation**: See `/docs/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md:1309-1347`

---

### ✅ Task 3.0.4: checkPermission() Transitive Closure (3-4 hours)

**Current**: Only checks direct user permissions
**Required**: Check active_role, groups, PUBLIC permissions

**Files**:
- `src/sblr/executor.cpp` (lines 13179-13185)

**Implementation**: Full code provided in `/docs/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md:1350-1440`

**Behavior**:
1. Check user's direct permissions
2. Check active role permissions (if SET ROLE active)
3. Check PUBLIC permissions
4. Check all group permissions

---

### ✅ Task 3.0.5: CASCADE Implementation (5-8 hours)

**Current**: CASCADE specified but not enforced
**Required**: Recursive dependency deletion

**Files**:
- `src/core/catalog_manager.cpp` (deleteUser, deleteRole, deleteGroup)
- `src/sblr/executor.cpp` (executeDropUser, executeDropRole, executeDropGroup)

**Implementation**: Full code provided in `/docs/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md:1443-1507`

**Behavior**:
- CASCADE: Remove all dependencies (roles, groups, permissions, grants)
- RESTRICT: Fail if dependencies exist

---

### ✅ Task 3.0.6: Documentation Updates (1 hour)

**Files**:
- `/docs/specifications/parser/v3/guides/SECURITY_SYSTEM_USAGE_GUIDE.md`
- `PROJECT_CONTEXT.md` (already updated)
- `/docs/specifications/parser/v3/SECURITY_SYSTEM_SPECIFICATION.md`

**Content**:
- Document password hashing algorithm
- Document CASCADE behavior
- Update completion percentages
- Add Phase 3.1+ planning

---

## Task Dependencies

```
3.0.1 (Password Hashing) ─┐
3.0.2 (Test API Update)   ─┼─→ Can run in parallel
3.0.3 (ALTER USER)        ─┤
                           │
3.0.4 (checkPermission)   ─┼─→ Independent, can run in parallel
                           │
3.0.5 (CASCADE)           ─┘─→ Depends on catalog functions

3.0.6 (Documentation)     ──→ Run last after all code complete
```

**Recommendation**: Tackle tasks in order 3.0.1 → 3.0.2 → 3.0.4 → 3.0.3 → 3.0.5 → 3.0.6

---

## Testing Strategy

### Unit Tests
- Password hashing performance (~50-100ms per hash)
- checkPermission() with various permission sources
- CASCADE recursive deletion
- RESTRICT dependency detection

### Integration Tests
- Run `test_security_phase2` after 3.0.2 update
- End-to-end CREATE USER with hashed password
- GRANT to role, SET ROLE, verify access
- DROP USER CASCADE removes all traces
- DROP USER RESTRICT fails with dependencies

### Manual Testing
1. CREATE USER with password → verify bcrypt hash stored
2. GRANT SELECT ON table TO role
3. GRANT role TO user
4. SET ROLE, SELECT from table → should work
5. REVOKE role FROM user
6. SELECT from table → should fail
7. DROP USER CASCADE → should remove all memberships

---

## Success Criteria

**Phase 3.0 Complete When**:
- ✅ Password hashing uses bcrypt (12 rounds)
- ✅ Integration test compiles and passes all 15 tests
- ✅ ALTER USER can change superuser flag
- ✅ checkPermission() checks user/role/group/PUBLIC
- ✅ CASCADE removes all dependencies recursively
- ✅ RESTRICT properly detects dependencies
- ✅ Documentation updated with Phase 2 completion

**Result**: Phase 2 is 100% production-ready

---

## Risk Assessment

| Task | Risk | Mitigation |
|------|------|------------|
| 3.0.1 Password Hashing | LOW | Standard bcrypt library, well-documented |
| 3.0.2 Test API Update | NONE | Straightforward API update |
| 3.0.3 ALTER USER | LOW | Simple catalog API extension |
| 3.0.4 checkPermission | LOW | Clear logic, existing catalog functions |
| 3.0.5 CASCADE | MEDIUM | Recursive deletion, need careful testing |
| 3.0.6 Documentation | NONE | Standard documentation update |

**Overall Risk**: LOW

---

## Effort Breakdown

| Task | Estimate | Actual | Notes |
|------|----------|--------|-------|
| 3.0.1 Password Hashing | 2-3h | TBD | Includes bcrypt setup |
| 3.0.2 Test API | 10m | TBD | Simple find/replace |
| 3.0.3 ALTER USER | 1h | TBD | Catalog API extension |
| 3.0.4 checkPermission | 3-4h | TBD | Full permission hierarchy |
| 3.0.5 CASCADE | 5-8h | TBD | Most complex task |
| 3.0.6 Documentation | 1h | TBD | Update 3 docs |
| **Total** | **12-18h** | **TBD** | **~2-3 days** |

---

## Next Steps After 3.0

Once Phase 3.0 is complete, the security system will be production-ready for basic use cases. The team can then proceed to:

1. **Phase 3.1**: External Authentication (LDAP/AD) - 30-40 hours
2. **Phase 3.2**: LDAP Authenticator - 40-60 hours
3. **Phase 3.3**: Group Mapping - 20-30 hours
4. **Phase 3.4**: Periodic Revalidation - 20-30 hours
5. **Phase 3.5**: Column-Level Permissions - 40-60 hours

**Total Phase 3**: 150-250 hours (4-6 weeks)

---

## References

- **Implementation Plan**: `/docs/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md` (lines 1221-1540)
- **Specification**: `/docs/specifications/parser/v3/SECURITY_SYSTEM_SPECIFICATION.md`
- **Phase 2 Complete**: `/docs/specifications/parser/v3/status/SECURITY_PHASE2_COMPLETE_2025-11-11.md`
- **Project Context**: `/PROJECT_CONTEXT.md` (updated Nov 11, 2025)

---

**Document Version**: 1.0
**Author**: Claude (Anthropic)
**Status**: Ready for Implementation
**Created**: November 11, 2025
