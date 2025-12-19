# Test Requirements (MANDATORY)

**Last Updated**: 2024-12-19

This document defines **MANDATORY** test coverage requirements for all features.
**NO EXCEPTIONS** - every feature must include ALL required test types.

---

## Overview

Every feature implementation MUST include the following test types:

1. **Happy Path Tests** - Basic functionality works
2. **Restart/Persistence Tests** - Data survives database restart
3. **Negative/Error Tests** - Error cases handled correctly
4. **Multi-Path Tests** - All access paths work
5. **Concurrency Tests** - Lock ordering and concurrent access (if applicable)
6. **Integration Tests** - Security, audit, resolver integration (if applicable)

---

## 1. Happy Path Tests

**Purpose**: Verify basic functionality works correctly.

**Requirements**:
- Test object creation
- Test object retrieval
- Test object modification
- Test object deletion
- Test basic queries/operations

**Example**:
```cpp
TEST_F(FeatureTest, CreateFeature) {
    ErrorContext ctx;
    ID feature_id;
    ASSERT_EQ(catalog->createFeature(schema_id, "test_feature", feature_id, &ctx), Status::OK);
    EXPECT_NE(feature_id.bytes[0], 0);  // Valid UUID
}

TEST_F(FeatureTest, GetFeature) {
    ErrorContext ctx;
    ID feature_id = createTestFeature("my_feature");

    CatalogManager::FeatureInfo info;
    ASSERT_EQ(catalog->getFeature(feature_id, info, &ctx), Status::OK);
    EXPECT_EQ(info.name, "my_feature");
}

TEST_F(FeatureTest, DropFeature) {
    ErrorContext ctx;
    ID feature_id = createTestFeature("temp_feature");

    ASSERT_EQ(catalog->dropFeature(feature_id, &ctx), Status::OK);

    CatalogManager::FeatureInfo info;
    EXPECT_EQ(catalog->getFeature(feature_id, info, &ctx), Status::NOT_FOUND);
}
```

---

## 2. Restart/Persistence Tests (MANDATORY)

**Purpose**: Verify data persists across database restart.

**Requirements**:
- Create data
- Close database
- Reopen database
- Verify data still exists
- Verify data is correct

**Pattern**:
```cpp
TEST_F(FeatureTest, PersistenceAcrossRestart) {
    ErrorContext ctx;

    // 1. Create feature data
    ID feature_id = createTestFeature("persistent_feature");

    // 2. Verify it exists before restart
    CatalogManager::FeatureInfo info;
    ASSERT_EQ(catalog->getFeature(feature_id, info, &ctx), Status::OK);
    EXPECT_EQ(info.name, "persistent_feature");

    // 3. Close and reopen database
    db->close();
    delete db;
    db = nullptr;
    catalog = nullptr;

    db = new Database();
    ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);
    catalog = db->catalog_manager();
    ASSERT_NE(catalog, nullptr);

    // 4. Verify data still exists after restart
    CatalogManager::FeatureInfo reloaded_info;
    ASSERT_EQ(catalog->getFeature(feature_id, reloaded_info, &ctx), Status::OK);
    EXPECT_EQ(reloaded_info.name, "persistent_feature");
}
```

**CRITICAL**: This test is **MANDATORY** for every feature that stores data.

---

## 3. Negative/Error Tests (MANDATORY)

**Purpose**: Verify error cases are handled correctly.

**Required error cases**:

### 3.1 NOT_FOUND
```cpp
TEST_F(FeatureTest, GetNonExistentFeature) {
    ErrorContext ctx;
    ID invalid_id = generateUuidV7();  // Random UUID

    CatalogManager::FeatureInfo info;
    EXPECT_EQ(catalog->getFeature(invalid_id, info, &ctx), Status::NOT_FOUND);
    EXPECT_NE(std::string(ctx.message).find("not found"), std::string::npos);
}
```

### 3.2 CONSTRAINT_VIOLATION
```cpp
TEST_F(FeatureTest, DropFeatureWithDependents) {
    ErrorContext ctx;

    // Create feature with dependent
    ID feature_id = createTestFeature("base_feature");
    ID dependent_id = createDependentFeature(feature_id, "dependent_feature");

    // Try to drop base feature - should fail
    EXPECT_EQ(catalog->dropFeature(feature_id, &ctx), Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("depend"), std::string::npos);

    // Verify base feature still exists
    CatalogManager::FeatureInfo info;
    EXPECT_EQ(catalog->getFeature(feature_id, info, &ctx), Status::OK);
}
```

### 3.3 INVALID_ARGUMENT
```cpp
TEST_F(FeatureTest, CreateFeatureWithInvalidArguments) {
    ErrorContext ctx;
    ID feature_id;

    // Null name
    EXPECT_EQ(catalog->createFeature(schema_id, nullptr, feature_id, &ctx), Status::INVALID_ARGUMENT);

    // Empty name
    EXPECT_EQ(catalog->createFeature(schema_id, "", feature_id, &ctx), Status::INVALID_ARGUMENT);

    // Invalid schema ID
    ID invalid_schema;
    memset(&invalid_schema, 0, sizeof(ID));
    EXPECT_EQ(catalog->createFeature(invalid_schema, "test", feature_id, &ctx), Status::INVALID_ARGUMENT);
}
```

### 3.4 ALREADY_EXISTS (if applicable)
```cpp
TEST_F(FeatureTest, CreateDuplicateFeature) {
    ErrorContext ctx;

    ID feature1 = createTestFeature("duplicate_name");

    ID feature2;
    EXPECT_EQ(catalog->createFeature(schema_id, "duplicate_name", feature2, &ctx), Status::ALREADY_EXISTS);
}
```

---

## 4. Multi-Path Tests (MANDATORY)

**Purpose**: Verify feature works through all access paths.

**Required paths**:
1. SQL commands (via executor)
2. CatalogManager API (direct calls)
3. Executor opcodes (if applicable)

**Example**:
```cpp
TEST_F(FeatureTest, CreateFeatureViaSQL) {
    ErrorContext ctx;

    // Test via SQL command
    std::string sql = "CREATE FEATURE my_feature";
    auto result = executor->execute(sql, &ctx);
    ASSERT_EQ(result.status, Status::OK);

    // Verify via CatalogManager API
    CatalogManager::FeatureInfo info;
    ASSERT_EQ(catalog->getFeature("my_feature", info, &ctx), Status::OK);
}

TEST_F(FeatureTest, CreateFeatureViaCatalogAPI) {
    ErrorContext ctx;

    // Test via direct CatalogManager call
    ID feature_id;
    ASSERT_EQ(catalog->createFeature(schema_id, "api_feature", feature_id, &ctx), Status::OK);

    // Verify via SQL query
    std::string sql = "SELECT * FROM features WHERE name='api_feature'";
    auto result = executor->execute(sql, &ctx);
    ASSERT_EQ(result.status, Status::OK);
    ASSERT_EQ(result.rows.size(), 1);
}
```

**Verification**: Test file must include direct catalog API calls
```bash
grep "catalog->" tests/unit/test_feature.cpp
```

---

## 5. Concurrency Tests (if applicable)

**Purpose**: Verify lock ordering and concurrent access.

**When required**: Features that modify shared state

**Example**:
```cpp
TEST_F(FeatureTest, ConcurrentCreation) {
    ErrorContext ctx1, ctx2;

    // Create two transactions
    auto txn1 = db->beginTransaction(&ctx1);
    auto txn2 = db->beginTransaction(&ctx2);

    // Both try to create feature concurrently
    ID feature1, feature2;
    Status s1 = catalog->createFeature(schema_id, "concurrent1", feature1, &ctx1);
    Status s2 = catalog->createFeature(schema_id, "concurrent2", feature2, &ctx2);

    // At least one should succeed
    EXPECT_TRUE(s1 == Status::OK || s2 == Status::OK);

    // Commit both
    txn1->commit(&ctx1);
    txn2->commit(&ctx2);
}

TEST_F(FeatureTest, ConcurrentModification) {
    ErrorContext ctx;
    ID feature_id = createTestFeature("shared_feature");

    // Test concurrent modifications
    // Verify proper locking and serialization
    // ...
}
```

---

## 6. Integration Tests (if applicable)

**Purpose**: Verify integration with security, audit, resolver, dependencies.

### 6.1 Security Context Integration
```cpp
TEST_F(FeatureTest, SecurityContextEnforcement) {
    ErrorContext ctx;

    // Create security context without permission
    SecurityContext unprivileged_ctx;
    unprivileged_ctx.user_id = unprivileged_user_id;
    unprivileged_ctx.has_permission = [](Permission p) { return false; };

    // Operation should fail
    ID feature_id;
    EXPECT_EQ(catalog->createFeature(schema_id, "test", feature_id, &ctx), Status::PERMISSION_DENIED);
}
```

### 6.2 Audit Logging Integration
```cpp
TEST_F(FeatureTest, AuditLogEntryCreated) {
    ErrorContext ctx;

    // Create feature
    ID feature_id = createTestFeature("audited_feature");

    // Verify audit log entry
    std::vector<AuditEvent> events;
    audit_logger->queryAuditLog(AuditQuery{.object_id = feature_id}, events, &ctx);

    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].action, "CREATE_FEATURE");
    EXPECT_EQ(events[0].object_id, feature_id);
}
```

### 6.3 UUID Resolver Integration
```cpp
TEST_F(FeatureTest, UUIDResolverUpdated) {
    ErrorContext ctx;

    // Create feature
    ID feature_id = createTestFeature("resolver_test");

    // Verify UUID resolver updated
    ResolvedObject obj;
    ASSERT_EQ(resolver->resolveObject(feature_id, obj, &ctx), Status::OK);
    EXPECT_EQ(obj.name, "resolver_test");
    EXPECT_EQ(obj.type, ObjectType::FEATURE);
}
```

### 6.4 Dependency Tracking Integration
```cpp
TEST_F(FeatureTest, DependenciesTracked) {
    ErrorContext ctx;

    // Create base object
    ID table_id = createTestTable("base_table");

    // Create feature that depends on table
    std::vector<std::pair<ID, ObjectType>> deps;
    deps.emplace_back(table_id, ObjectType::TABLE);
    ID feature_id = createFeatureWithDependencies("dependent_feature", deps);

    // Verify dependency recorded
    std::vector<DependencyInfo> dependencies;
    ASSERT_EQ(catalog->getDependenciesFor(feature_id, dependencies, &ctx), Status::OK);

    ASSERT_EQ(dependencies.size(), 1);
    EXPECT_EQ(dependencies[0].referenced_object_id, table_id);
    EXPECT_EQ(dependencies[0].referenced_type, ObjectType::TABLE);
}
```

---

## Test Organization

### File Structure
```
tests/unit/test_<feature>.cpp
```

### Test Fixture
```cpp
class FeatureTest : public ::testing::Test {
protected:
    std::string test_db_path;
    Database* db = nullptr;
    CatalogManager* catalog = nullptr;
    ID schema_id;

    void SetUp() override {
        test_db_path = "/tmp/test_feature_" + std::to_string(getpid()) + ".sbdb";
        std::remove(test_db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path, 16384, &ctx), Status::OK);

        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);

        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        // Get schema
        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog->getSchema("PUBLIC", schema_info, &ctx), Status::OK);
        schema_id = schema_info.schema_id;
    }

    void TearDown() override {
        if (db) {
            db->close();
            delete db;
            db = nullptr;
        }
        std::remove(test_db_path.c_str());
        std::remove((test_db_path + "-lock").c_str());
    }

    // Helper functions
    ID createTestFeature(const std::string& name) {
        ErrorContext ctx;
        ID feature_id;
        Status s = catalog->createFeature(schema_id, name, feature_id, &ctx);
        if (s != Status::OK) {
            ADD_FAILURE() << "Failed to create feature: " << ctx.message;
        }
        return feature_id;
    }
};
```

---

## Test Coverage Requirements

### Minimum Test Count

For a typical feature, minimum test count:
- **Happy path**: 3-5 tests (create, get, modify, delete, query)
- **Restart**: 1 test (MANDATORY)
- **Negative**: 3-5 tests (NOT_FOUND, CONSTRAINT_VIOLATION, INVALID_ARGUMENT, etc.)
- **Multi-path**: 2-3 tests (SQL, API, executor)
- **Concurrency**: 1-2 tests (if applicable)
- **Integration**: 1 test per integration point (security, audit, resolver, dependencies)

**Total minimum**: 10-15 tests per feature

---

## Running Tests

### Run all tests for feature
```bash
./build/tests/scratchbird_tests --gtest_filter="*Feature*"
```

### Run restart tests specifically
```bash
./build/tests/scratchbird_tests --gtest_filter="*Feature*Restart*"
```

### Run negative tests specifically
```bash
./build/tests/scratchbird_tests --gtest_filter="*Feature*Error*"
./build/tests/scratchbird_tests --gtest_filter="*Feature*Fail*"
```

### Run with verbose output
```bash
./build/tests/scratchbird_tests --gtest_filter="*Feature*" --gtest_print_time=1
```

---

## Test Quality Standards

### Good Test Characteristics
- **Clear names**: Test name describes what is being tested
- **Focused**: Each test tests one specific behavior
- **Independent**: Tests don't depend on each other
- **Repeatable**: Tests produce same results every time
- **Fast**: Tests run quickly (< 1 second per test typically)

### Test Naming Convention
```cpp
TEST_F(FeatureTest, <Action><Scenario><ExpectedResult>)

// Examples:
TEST_F(FeatureTest, CreateFeatureSucceeds)
TEST_F(FeatureTest, GetNonExistentFeatureReturnsNotFound)
TEST_F(FeatureTest, DropFeatureWithDependentsFailsWithConstraintViolation)
TEST_F(FeatureTest, PersistenceAcrossRestartMaintainsData)
```

---

## Completion Criteria

Before marking a feature complete, verify:

- [ ] All happy path tests pass
- [ ] **Restart test exists and passes** (MANDATORY)
- [ ] All negative tests pass
- [ ] Multi-path tests pass
- [ ] Concurrency tests pass (if applicable)
- [ ] Integration tests pass (if applicable)
- [ ] Test output provided to user
- [ ] No test failures or warnings

---

## Related Documents

- `/IMPLEMENTATION_STANDARDS.md` - Overall implementation standards
- `/COMPLETION_VERIFICATION_CHECKLIST.md` - Pre-completion checklist
- `/docs/standards/COMMON_FAILURE_PATTERNS.md` - Patterns to avoid
