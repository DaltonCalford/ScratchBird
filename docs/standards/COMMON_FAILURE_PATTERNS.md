# Common Failure Patterns (MUST AVOID)

**Last Updated**: 2024-12-19
**Source**: Derived from audit findings in `docs/findings/engine_gap_report.md` and remediation plans

This document catalogs the most common failure patterns identified in implementation audits.
**These patterns MUST be avoided in all future work.**

---

## Critical Patterns Identified in Audits

### Pattern 1: Executor-Only Implementation

**Pattern**: Implementing features only through the executor layer, without proper CatalogManager foundation.

**Problem**:
- Direct CatalogManager calls bypass the logic
- Features don't work through API
- Integration impossible
- Restart fails (no persistence)

**Example (WRONG)**:
```cpp
// executor.cpp - implementing feature directly
Status Executor::handleMyFeature() {
    // Logic implemented here only
    // No catalog persistence
    // No CatalogManager API
}
```

**Example (RIGHT)**:
```cpp
// catalog_manager.cpp - foundation first
Status CatalogManager::myFeature(...) {
    // Core logic here
    // Persist to catalog
    // Return result
}

// executor.cpp - thin wrapper
Status Executor::handleMyFeature() {
    return catalog_manager_->myFeature(...);
}
```

**Detection**:
```bash
# Should find implementation in BOTH files
grep -n "myFeature" src/core/catalog_manager.cpp
grep -n "myFeature" src/sblr/executor.cpp
```

**Test Requirements**:
- Test must call CatalogManager API directly (not through executor)
- Verify: `grep "catalog->" tests/unit/test_feature.cpp`

---

### Pattern 2: Missing Persistence

**Pattern**: Implementing cache-only updates without on-disk save/load paths.

**Problem**:
- Database restart loses all behavior
- Data disappears
- Integration tests fail
- Production data loss

**Example (WRONG)**:
```cpp
// Storing in memory only
std::map<ID, FeatureData> feature_cache_;

Status CatalogManager::addFeature(...) {
    feature_cache_[id] = data;  // Memory only!
    return Status::OK;
}
```

**Example (RIGHT)**:
```cpp
// Persist to catalog table
Status CatalogManager::addFeature(...) {
    // 1. Write to catalog table
    Status s = executeCatalogSQL("INSERT INTO sb_features ...");
    if (s != Status::OK) return s;

    // 2. Update cache
    feature_cache_[id] = data;

    return Status::OK;
}

// Load on database open
Status CatalogManager::loadFeatures() {
    // Read from catalog table and populate cache
    return queryCatalog("SELECT * FROM sb_features ...");
}
```

**Detection**:
- Look for catalog table in `catalog_manager.cpp`
- Look for load function called during DB open
- **MANDATORY**: Restart test must exist and pass

**Test Requirements**:
```cpp
TEST_F(FeatureTest, PersistenceAcrossRestart) {
    // Create data
    createFeature();

    // Close and reopen database
    db->close();
    delete db;
    db = new Database();
    db->open(test_db_path, &ctx);

    // Verify data still exists
    ASSERT_EQ(catalog->getFeature(id, data, &ctx), Status::OK);
}
```

---

### Pattern 3: Happy-Path-Only Testing

**Pattern**: Only testing success cases, ignoring error handling and edge cases.

**Problem**:
- Error handling broken
- Undefined behavior on invalid input
- Crashes in production
- Security vulnerabilities

**Example (WRONG)**:
```cpp
// Only tests success
TEST_F(FeatureTest, BasicOperation) {
    ASSERT_EQ(catalog->createFeature(...), Status::OK);
    ASSERT_EQ(catalog->getFeature(...), Status::OK);
}
// No error cases tested!
```

**Example (RIGHT)**:
```cpp
// Test success
TEST_F(FeatureTest, CreateFeatureSuccess) {
    ASSERT_EQ(catalog->createFeature(...), Status::OK);
}

// Test NOT_FOUND
TEST_F(FeatureTest, GetNonExistentFeature) {
    EXPECT_EQ(catalog->getFeature(invalid_id, data, &ctx), Status::NOT_FOUND);
    EXPECT_NE(std::string(ctx.message).find("not found"), std::string::npos);
}

// Test CONSTRAINT_VIOLATION
TEST_F(FeatureTest, DropFeatureWithDependents) {
    createDependentFeature();
    EXPECT_EQ(catalog->dropFeature(id, &ctx), Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("depend"), std::string::npos);
}

// Test INVALID_ARGUMENT
TEST_F(FeatureTest, CreateWithInvalidArguments) {
    EXPECT_EQ(catalog->createFeature(nullptr, ..., &ctx), Status::INVALID_ARGUMENT);
}
```

**Detection**:
```bash
# Must find negative test patterns
grep -n "EXPECT_EQ.*NOT_FOUND" tests/unit/test_feature.cpp
grep -n "EXPECT_EQ.*CONSTRAINT_VIOLATION" tests/unit/test_feature.cpp
grep -n "EXPECT_EQ.*INVALID_ARGUMENT" tests/unit/test_feature.cpp
```

**Test Requirements**: MANDATORY negative tests for every feature

---

### Pattern 4: Incomplete Type Coverage

**Pattern**: Switch statements with missing enum values or default fallthrough.

**Problem**:
- Produces `<unknown>` output
- Wrong behavior for some object types
- Silent failures
- Incomplete feature support

**Example (WRONG)**:
```cpp
std::string objectTypeToString(ObjectType type) {
    switch (type) {
        case ObjectType::TABLE: return "TABLE";
        case ObjectType::VIEW: return "VIEW";
        // Missing other types!
        default: return "<unknown>";  // Silently hides missing cases
    }
}
```

**Example (RIGHT)**:
```cpp
std::string objectTypeToString(ObjectType type) {
    switch (type) {
        case ObjectType::TABLE: return "TABLE";
        case ObjectType::VIEW: return "VIEW";
        case ObjectType::INDEX: return "INDEX";
        case ObjectType::SEQUENCE: return "SEQUENCE";
        case ObjectType::FUNCTION: return "FUNCTION";
        case ObjectType::PROCEDURE: return "PROCEDURE";
        // ... ALL enum values handled explicitly
    }
    // No default case - compiler will warn if new enum value added
}
```

**Detection**:
```bash
# Find switch statements
grep -n "switch.*object_type\|ObjectType" src/

# Check for <unknown> patterns
grep -r "<unknown>" src/ | grep -i "feature"

# Verify all enum values handled
grep "enum.*ObjectType" include/ -A 20
```

**Compiler Enforcement**:
- Compile with `-Wswitch-enum` to catch missing cases
- Never use `default:` in enum switches

---

### Pattern 5: Missing Cascade/Restrict Logic

**Pattern**: Ignoring dependency constraints when dropping objects.

**Problem**:
- Data corruption (orphaned dependencies)
- Constraint violations
- Broken references
- Inconsistent behavior

**Example (WRONG)**:
```cpp
Status CatalogManager::dropTable(ID table_id, ...) {
    // Just drop it, no dependency check!
    return deleteFromCatalog("sb_tables", table_id);
}
```

**Example (RIGHT)**:
```cpp
Status CatalogManager::dropTable(ID table_id, bool cascade, ...) {
    // 1. Check for dependencies
    std::vector<DependencyInfo> dependents;
    Status s = getDependents(table_id, dependents, ctx);
    if (s != Status::OK) return s;

    // 2. Filter blocking vs owned dependencies
    auto filtered = filterDependencies(table_id, ObjectType::TABLE, dependents, ctx);

    // 3. Handle based on cascade flag
    if (!cascade && !filtered.blocking.empty()) {
        // RESTRICT: fail if blocking dependencies exist
        std::string msg = buildDependencyErrorMessage(
            table_name, ObjectType::TABLE, filtered.blocking, ctx);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // 4. If cascade: drop dependents first
    if (cascade) {
        for (const auto& dep : dependents) {
            Status s = dropObject(dep.dependent_id, cascade, ctx);
            if (s != Status::OK) return s;
        }
    }

    // 5. Drop owned dependencies (indexes, triggers, etc.)
    for (const auto& dep : filtered.owned) {
        dropObject(dep.dependent_id, false, ctx);
    }

    // 6. Finally drop the table
    return deleteFromCatalog("sb_tables", table_id);
}
```

**Test Requirements**:
```cpp
TEST_F(FeatureTest, DropBlockedByDependency) {
    createDependentObject();
    EXPECT_EQ(catalog->dropObject(base_id, false, &ctx), Status::CONSTRAINT_VIOLATION);
}

TEST_F(FeatureTest, DropSucceedsAfterDroppingDependent) {
    createDependentObject();
    ASSERT_EQ(catalog->dropObject(dependent_id, false, &ctx), Status::OK);
    ASSERT_EQ(catalog->dropObject(base_id, false, &ctx), Status::OK);
}
```

---

### Pattern 6: Specification Deviations

**Pattern**: Implementing features without reading specifications, or deviating without documentation.

**Problem**:
- Incompatible behavior
- Missing requirements
- Breaks client expectations
- Difficult to maintain

**Example (WRONG)**:
```cpp
// Implementing based on assumptions
// Never read the spec
// Behavior differs from documented standard
```

**Example (RIGHT)**:
```cpp
// 1. Read specification first
// 2. List all requirements
// 3. Implement each requirement
// 4. Document any deviations with config flags

// Example: Feature requires configurable behavior per spec
if (config_->allow_feature_X) {
    // Spec-compliant path
} else {
    // Alternative path with documented reason
    // See: docs/specifications/FEATURE_SPEC.md section 4.2
}
```

**Process**:
1. Before implementing, read ALL relevant specifications
2. Create requirement checklist
3. Map each requirement to implementation
4. Document deviations with explicit config flags
5. Get user approval on any deviations

---

### Pattern 7: Missing Foundation

**Pattern**: Implementing features without verifying catalog infrastructure exists.

**Problem**:
- Nothing persists
- Integration impossible
- Restart fails
- Wasted implementation effort

**Example (WRONG)**:
```cpp
// Starting implementation without checking if catalog table exists
Status Executor::handleFeature() {
    // Implementing feature...
    // But there's no sb_features table!
    // No way to persist data!
}
```

**Example (RIGHT)**:
```bash
# BEFORE implementing, verify foundation
./scripts/verify_foundation.sh feature_name

# If catalog table missing, add it FIRST:
# 1. Add catalog table DDL
# 2. Implement save/load paths
# 3. Test persistence
# 4. THEN implement feature
```

**Detection**: Run foundation audit before starting work

---

### Pattern 8: No Integration Testing

**Pattern**: Implementing features in isolation without testing integration points.

**Problem**:
- Security context not propagated
- Audit logs not created
- UUID resolver not updated
- Dependencies not tracked

**Example (WRONG)**:
```cpp
// Feature implemented but never wired to:
// - SecurityContext
// - AuditLogger
// - UUID resolver
// - Dependency tracking
```

**Example (RIGHT)**:
```cpp
Status CatalogManager::createFeature(...) {
    // 1. Check security context
    if (!security_context.hasPermission(Permission::CREATE_FEATURE)) {
        return Status::PERMISSION_DENIED;
    }

    // 2. Create feature
    Status s = createFeatureInternal(...);
    if (s != Status::OK) return s;

    // 3. Log to audit
    audit_logger_->logEvent(AuditEvent{
        .action = "CREATE_FEATURE",
        .object_id = feature_id,
        .user_id = security_context.user_id,
        .session_id = security_context.session_id
    });

    // 4. Update UUID resolver (if needed)
    resolver_->register(feature_id, feature_name, ObjectType::FEATURE);

    // 5. Track dependencies (if needed)
    for (const auto& dep : dependencies) {
        dependency_tracker_->addDependency(feature_id, dep);
    }

    return Status::OK;
}
```

**Test Requirements**:
- Verify security context checked
- Verify audit log entry created
- Verify UUID resolver updated
- Verify dependencies tracked

---

## Detection Methods

Run these commands before marking ANY task complete:

### Pattern 1: Executor-Only Implementation
```bash
# Should find implementation in BOTH files
grep -n "<feature>" src/core/catalog_manager.cpp
grep -n "<feature>" src/sblr/executor.cpp
```

### Pattern 2: Missing Persistence
```bash
# Must find restart test
grep -n "Restart\|Persistence" tests/unit/test_<feature>.cpp
# Must find catalog table
grep -n "sb_<feature>" src/core/catalog_manager.cpp
```

### Pattern 3: Happy-Path-Only Testing
```bash
# Must find negative tests
grep -n "EXPECT_EQ.*NOT_FOUND\|CONSTRAINT_VIOLATION\|INVALID_ARGUMENT" tests/unit/test_<feature>.cpp
```

### Pattern 4: Incomplete Type Coverage
```bash
# Find switch statements and verify all cases
grep -n "switch.*object_type" src/ | grep -i "<feature>"
# Check for <unknown> outputs
grep -r "<unknown>" src/ | grep -i "<feature>"
```

### Pattern 5: Missing Cascade/Restrict
```bash
# Check for dependency checks in drop operations
grep -n "getDependents\|filterDependencies" src/core/catalog_manager.cpp
```

### Pattern 6: Specification Deviations
```bash
# Verify specs exist and were read
find docs/specifications -name "*.md" | xargs grep -l "<feature>"
```

### Pattern 7: Missing Foundation
```bash
# Run foundation audit
./scripts/verify_foundation.sh <feature>
```

### Pattern 8: No Integration Testing
```bash
# Check for security/audit/resolver integration
grep -n "SecurityContext\|AuditLog\|resolver" src/core/catalog_manager.cpp
```

---

## Verification Checklist

Before marking ANY task complete, verify NONE of these patterns exist:

- [ ] **Pattern 1**: Feature works through CatalogManager API (not just executor)
- [ ] **Pattern 2**: Restart test passes, data persists
- [ ] **Pattern 3**: Negative tests pass, errors handled
- [ ] **Pattern 4**: All enum values handled, no `<unknown>`
- [ ] **Pattern 5**: Dependencies checked, cascade/restrict enforced
- [ ] **Pattern 6**: Specifications read, requirements implemented
- [ ] **Pattern 7**: Catalog infrastructure exists and verified
- [ ] **Pattern 8**: Integration points wired (security, audit, resolver)

---

## Related Documents

- `/IMPLEMENTATION_STANDARDS.md` - Overall implementation standards
- `/COMPLETION_VERIFICATION_CHECKLIST.md` - Pre-completion checklist
- `/docs/findings/engine_gap_report.md` - Audit findings
- `/docs/findings/plans/plan_09_audit_methodology.md` - Audit methodology
