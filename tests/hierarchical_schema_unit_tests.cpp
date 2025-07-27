/*
 * ScratchBird v0.6.0 - Comprehensive Unit Tests for Hierarchical Schema Operations
 * Test suite covering all aspects of hierarchical schema functionality
 * 
 * Build Location: src/test_software/
 * Output Location: release/alpha0.6.0/bin/tests/
 */

// ScratchBird includes (relative to src root)
#include "../include/firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/Attachment.h" 
#include "../dsql/DdlNodes.h"
#include "../jrd/met.h"
#include "../common/StatusArg.h"

// Test framework includes
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <map>

namespace Jrd {

class HierarchicalSchemaTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        // Initialize test database and attachment
        attachment = nullptr; // Mock attachment for testing
        schema_cache = nullptr; // Mock schema cache
    }

    void TearDown() override 
    {
        // Cleanup test resources
        if (attachment) {
            // Clean up test attachment
        }
    }

    Attachment* attachment;
    SchemaPathCache* schema_cache;
};

// Test Schema Path Parsing and Validation
TEST_F(HierarchicalSchemaTest, SchemaPathParsing)
{
    // Test single-level schema
    EXPECT_TRUE(SchemaPathCache::isValidSchemaPath("finance"));
    EXPECT_EQ(SchemaPathCache::getSchemaDepth("finance"), 1);

    // Test two-level schema
    EXPECT_TRUE(SchemaPathCache::isValidSchemaPath("finance.accounting"));
    EXPECT_EQ(SchemaPathCache::getSchemaDepth("finance.accounting"), 2);

    // Test three-level schema (maximum supported in parser)
    EXPECT_TRUE(SchemaPathCache::isValidSchemaPath("finance.accounting.reports"));
    EXPECT_EQ(SchemaPathCache::getSchemaDepth("finance.accounting.reports"), 3);

    // Test maximum depth (8 levels)
    EXPECT_TRUE(SchemaPathCache::isValidSchemaPath("level1.level2.level3.level4.level5.level6.level7.level8"));
    EXPECT_EQ(SchemaPathCache::getSchemaDepth("level1.level2.level3.level4.level5.level6.level7.level8"), 8);

    // Test exceeding maximum depth (should fail)
    EXPECT_FALSE(SchemaPathCache::isValidSchemaPath("l1.l2.l3.l4.l5.l6.l7.l8.l9"));

    // Test invalid characters
    EXPECT_FALSE(SchemaPathCache::isValidSchemaPath("finance.@invalid"));
    EXPECT_FALSE(SchemaPathCache::isValidSchemaPath("finance..accounting"));
    EXPECT_FALSE(SchemaPathCache::isValidSchemaPath(".finance"));
    EXPECT_FALSE(SchemaPathCache::isValidSchemaPath("finance."));

    // Test length limits
    std::string long_name(64, 'x'); // Exceeds 63-character limit
    EXPECT_FALSE(SchemaPathCache::isValidSchemaPath(long_name));

    std::string max_name(63, 'x'); // Exactly 63 characters
    EXPECT_TRUE(SchemaPathCache::isValidSchemaPath(max_name));
}

// Test Schema Path Component Extraction
TEST_F(HierarchicalSchemaTest, SchemaPathComponents)
{
    std::vector<std::string> components;

    // Test simple path
    SchemaPathCache::parseSchemaPath("finance.accounting.reports", components);
    ASSERT_EQ(components.size(), 3);
    EXPECT_EQ(components[0], "finance");
    EXPECT_EQ(components[1], "accounting");
    EXPECT_EQ(components[2], "reports");

    // Test single component
    components.clear();
    SchemaPathCache::parseSchemaPath("finance", components);
    ASSERT_EQ(components.size(), 1);
    EXPECT_EQ(components[0], "finance");

    // Test parent path extraction
    EXPECT_EQ(SchemaPathCache::getParentPath("finance.accounting.reports"), "finance.accounting");
    EXPECT_EQ(SchemaPathCache::getParentPath("finance.accounting"), "finance");
    EXPECT_EQ(SchemaPathCache::getParentPath("finance"), "");

    // Test leaf name extraction
    EXPECT_EQ(SchemaPathCache::getLeafName("finance.accounting.reports"), "reports");
    EXPECT_EQ(SchemaPathCache::getLeafName("finance"), "finance");
}

// Test Circular Reference Detection
TEST_F(HierarchicalSchemaTest, CircularReferenceDetection)
{
    // Mock schema hierarchy for testing
    std::map<std::string, std::string> schema_parents = {
        {"finance", ""},
        {"finance.accounting", "finance"},
        {"finance.accounting.reports", "finance.accounting"}
    };

    // Test valid hierarchy (no cycles)
    EXPECT_FALSE(checkCircularReference("finance.accounting.budget", "finance.accounting", schema_parents));

    // Test direct circular reference
    EXPECT_TRUE(checkCircularReference("finance", "finance.accounting", schema_parents));

    // Test indirect circular reference
    schema_parents["finance.accounting.budget"] = "finance.accounting";
    EXPECT_TRUE(checkCircularReference("finance.accounting", "finance.accounting.budget", schema_parents));

    // Test self-reference
    EXPECT_TRUE(checkCircularReference("finance", "finance", schema_parents));

    // Test deep hierarchy check
    std::map<std::string, std::string> deep_hierarchy;
    std::string current = "root";
    for (int i = 1; i <= 7; i++) {
        std::string next = current + ".level" + std::to_string(i);
        deep_hierarchy[next] = current;
        current = next;
    }
    
    // This should be valid (7 levels)
    EXPECT_FALSE(checkCircularReference("root.level1.level2.level3.level4.level5.level6.level7.newchild", 
                                       "root.level1.level2.level3.level4.level5.level6.level7", deep_hierarchy));

    // Adding level8 would exceed depth limit
    deep_hierarchy["root.level1.level2.level3.level4.level5.level6.level7.level8"] = "root.level1.level2.level3.level4.level5.level6.level7";
    EXPECT_TRUE(checkCircularReference("root.level1.level2.level3.level4.level5.level6.level7.level8.newchild",
                                      "root.level1.level2.level3.level4.level5.level6.level7.level8", deep_hierarchy));
}

// Test Schema Cache Operations
TEST_F(HierarchicalSchemaTest, SchemaCacheOperations)
{
    SchemaPathCache cache;

    // Test cache insertion and retrieval
    cache.addSchema("finance", "", 1);
    cache.addSchema("finance.accounting", "finance", 2);
    cache.addSchema("finance.accounting.reports", "finance.accounting", 3);

    // Test cache lookups
    EXPECT_TRUE(cache.schemaExists("finance"));
    EXPECT_TRUE(cache.schemaExists("finance.accounting"));
    EXPECT_TRUE(cache.schemaExists("finance.accounting.reports"));
    EXPECT_FALSE(cache.schemaExists("finance.nonexistent"));

    // Test parent lookup
    EXPECT_EQ(cache.getParentSchema("finance.accounting"), "finance");
    EXPECT_EQ(cache.getParentSchema("finance.accounting.reports"), "finance.accounting");
    EXPECT_EQ(cache.getParentSchema("finance"), "");

    // Test depth lookup
    EXPECT_EQ(cache.getSchemaDepth("finance"), 1);
    EXPECT_EQ(cache.getSchemaDepth("finance.accounting"), 2);
    EXPECT_EQ(cache.getSchemaDepth("finance.accounting.reports"), 3);

    // Test children enumeration
    std::vector<std::string> children;
    cache.getChildSchemas("finance", children);
    EXPECT_EQ(children.size(), 1);
    EXPECT_EQ(children[0], "finance.accounting");

    children.clear();
    cache.getChildSchemas("finance.accounting", children);
    EXPECT_EQ(children.size(), 1);
    EXPECT_EQ(children[0], "finance.accounting.reports");

    // Test cache invalidation
    cache.removeSchema("finance.accounting.reports");
    EXPECT_FALSE(cache.schemaExists("finance.accounting.reports"));
    EXPECT_TRUE(cache.schemaExists("finance.accounting"));

    // Test cache statistics
    auto stats = cache.getStatistics();
    EXPECT_GT(stats.hit_count, 0);
    EXPECT_GE(stats.total_lookups, stats.hit_count);
    EXPECT_LT(stats.avg_lookup_time_ns, 1000000); // Should be under 1ms
}

// Test Schema Name Resolution
TEST_F(HierarchicalSchemaTest, SchemaNameResolution)
{
    // Test qualified name parsing for different levels
    QualifiedName qname;

    // Test single level (schema.object)
    parseQualifiedName("finance.accounts", qname);
    EXPECT_EQ(qname.schema, "finance");
    EXPECT_EQ(qname.object, "accounts");
    EXPECT_EQ(qname.package, "");

    // Test two levels (schema.subschema.object)
    parseQualifiedName("finance.accounting.accounts", qname);
    EXPECT_EQ(qname.schema, "finance.accounting");
    EXPECT_EQ(qname.object, "accounts");
    EXPECT_EQ(qname.package, "");

    // Test three levels (package.schema.object)
    parseQualifiedName("company.finance.accounts", qname);
    EXPECT_EQ(qname.package, "company");
    EXPECT_EQ(qname.schema, "finance");
    EXPECT_EQ(qname.object, "accounts");

    // Test name conflict resolution (package vs schema)
    // Package names take precedence over schema names
    EXPECT_TRUE(resolveNameConflict("finance", NAME_TYPE_PACKAGE, NAME_TYPE_SCHEMA));
    EXPECT_FALSE(resolveNameConflict("finance", NAME_TYPE_SCHEMA, NAME_TYPE_PACKAGE));
}

// Test Schema Creation and Validation
TEST_F(HierarchicalSchemaTest, SchemaCreationValidation)
{
    // Test valid schema creation
    EXPECT_TRUE(validateSchemaName("finance"));
    EXPECT_TRUE(validateSchemaName("finance_dept"));
    EXPECT_TRUE(validateSchemaName("FINANCE2024"));

    // Test invalid schema names
    EXPECT_FALSE(validateSchemaName("2finance")); // Cannot start with digit
    EXPECT_FALSE(validateSchemaName("finance-dept")); // Invalid character
    EXPECT_FALSE(validateSchemaName("finance dept")); // Space not allowed
    EXPECT_FALSE(validateSchemaName("")); // Empty name
    EXPECT_FALSE(validateSchemaName("select")); // Reserved keyword

    // Test schema creation hierarchy validation
    SchemaHierarchy hierarchy;
    hierarchy.addSchema("finance", "");
    hierarchy.addSchema("finance.accounting", "finance");

    // Valid child creation
    EXPECT_TRUE(hierarchy.canCreateSchema("finance.accounting.reports", "finance.accounting"));

    // Invalid parent (doesn't exist)
    EXPECT_FALSE(hierarchy.canCreateSchema("finance.marketing.budget", "finance.marketing"));

    // Circular reference attempt
    EXPECT_FALSE(hierarchy.canCreateSchema("finance", "finance.accounting.reports"));

    // Depth limit validation
    std::string deep_path = "level1";
    for (int i = 2; i <= 8; i++) {
        deep_path += ".level" + std::to_string(i);
    }
    EXPECT_TRUE(hierarchy.validateDepthLimit(deep_path)); // 8 levels OK

    deep_path += ".level9";
    EXPECT_FALSE(hierarchy.validateDepthLimit(deep_path)); // 9 levels exceeds limit
}

// Test Schema Deletion and Cascade Operations
TEST_F(HierarchicalSchemaTest, SchemaDeletionCascade)
{
    SchemaHierarchy hierarchy;
    hierarchy.addSchema("finance", "");
    hierarchy.addSchema("finance.accounting", "finance");
    hierarchy.addSchema("finance.accounting.reports", "finance.accounting");
    hierarchy.addSchema("finance.accounting.budget", "finance.accounting");
    hierarchy.addSchema("finance.marketing", "finance");

    // Test non-cascade deletion (should fail if children exist)
    EXPECT_FALSE(hierarchy.deleteSchema("finance.accounting", false));

    // Test cascade deletion
    std::vector<std::string> deleted_schemas;
    EXPECT_TRUE(hierarchy.deleteSchema("finance.accounting", true, deleted_schemas));
    
    // Verify all children were deleted
    EXPECT_EQ(deleted_schemas.size(), 3); // accounting + reports + budget
    EXPECT_FALSE(hierarchy.schemaExists("finance.accounting"));
    EXPECT_FALSE(hierarchy.schemaExists("finance.accounting.reports"));
    EXPECT_FALSE(hierarchy.schemaExists("finance.accounting.budget"));
    
    // Verify siblings remain
    EXPECT_TRUE(hierarchy.schemaExists("finance"));
    EXPECT_TRUE(hierarchy.schemaExists("finance.marketing"));

    // Test deletion of leaf schema
    EXPECT_TRUE(hierarchy.deleteSchema("finance.marketing", false));
    EXPECT_FALSE(hierarchy.schemaExists("finance.marketing"));
}

// Test Schema Permission and Security
TEST_F(HierarchicalSchemaTest, SchemaPermissionSecurity)
{
    // Test schema access permissions
    SchemaPermissions perms;
    perms.grantPermission("finance", "user1", PERMISSION_CREATE);
    perms.grantPermission("finance.accounting", "user2", PERMISSION_SELECT);

    // Test permission inheritance
    EXPECT_TRUE(perms.hasPermission("finance.accounting.reports", "user1", PERMISSION_CREATE));
    EXPECT_TRUE(perms.hasPermission("finance.accounting", "user2", PERMISSION_SELECT));
    EXPECT_FALSE(perms.hasPermission("finance.marketing", "user2", PERMISSION_CREATE));

    // Test permission revocation
    perms.revokePermission("finance", "user1", PERMISSION_CREATE);
    EXPECT_FALSE(perms.hasPermission("finance.accounting", "user1", PERMISSION_CREATE));

    // Test role-based permissions
    perms.grantRolePermission("finance", "accounting_role", PERMISSION_ALL);
    perms.assignUserToRole("user3", "accounting_role");
    EXPECT_TRUE(perms.hasPermission("finance.accounting", "user3", PERMISSION_SELECT));
}

// Test Schema Metadata Integration
TEST_F(HierarchicalSchemaTest, SchemaMetadataIntegration)
{
    // Test RDB$SCHEMAS table integration
    MetadataWriter writer;
    
    // Test schema insertion
    SchemaMetadata schema_meta;
    schema_meta.name = "finance.accounting";
    schema_meta.parent_name = "finance";
    schema_meta.schema_path = "finance.accounting";
    schema_meta.schema_level = 2;
    schema_meta.description = "Accounting department schema";

    EXPECT_TRUE(writer.insertSchema(schema_meta));

    // Test schema retrieval
    SchemaMetadata retrieved;
    EXPECT_TRUE(writer.getSchema("finance.accounting", retrieved));
    EXPECT_EQ(retrieved.name, "finance.accounting");
    EXPECT_EQ(retrieved.parent_name, "finance");
    EXPECT_EQ(retrieved.schema_level, 2);

    // Test schema update
    retrieved.description = "Updated accounting schema";
    EXPECT_TRUE(writer.updateSchema(retrieved));

    // Test schema existence check
    EXPECT_TRUE(writer.schemaExists("finance.accounting"));
    EXPECT_FALSE(writer.schemaExists("finance.nonexistent"));

    // Test hierarchy view (RDB$SCHEMA_HIERARCHY)
    std::vector<SchemaHierarchyView> hierarchy_view;
    EXPECT_TRUE(writer.getSchemaHierarchy(hierarchy_view));
    EXPECT_GT(hierarchy_view.size(), 0);
}

// Test Performance Characteristics
TEST_F(HierarchicalSchemaTest, PerformanceCharacteristics)
{
    SchemaPathCache cache;
    auto start = std::chrono::high_resolution_clock::now();

    // Load test data
    const int num_schemas = 1000;
    for (int i = 0; i < num_schemas; i++) {
        std::string schema_name = "schema" + std::to_string(i);
        cache.addSchema(schema_name, "", 1);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should be able to insert 1000 schemas in under 10ms
    EXPECT_LT(duration.count(), 10000);

    // Test lookup performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_schemas; i++) {
        std::string schema_name = "schema" + std::to_string(i);
        EXPECT_TRUE(cache.schemaExists(schema_name));
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be able to lookup 1000 schemas in under 5ms
    EXPECT_LT(duration.count(), 5000);

    // Test cache hit rate (should be >95% for repeated lookups)
    auto stats = cache.getStatistics();
    double hit_rate = (double)stats.hit_count / stats.total_lookups;
    EXPECT_GT(hit_rate, 0.95);
}

// Test Error Handling and Edge Cases
TEST_F(HierarchicalSchemaTest, ErrorHandlingEdgeCases)
{
    SchemaPathCache cache;

    // Test null/empty input handling
    EXPECT_FALSE(cache.schemaExists(""));
    EXPECT_FALSE(cache.schemaExists(nullptr));
    EXPECT_EQ(cache.getSchemaDepth(""), 0);

    // Test invalid schema path formats
    EXPECT_FALSE(cache.addSchema("..invalid..", "", 1));
    EXPECT_FALSE(cache.addSchema("schema.with.@invalid.chars", "", 1));

    // Test memory limits
    std::string huge_schema_name(1000000, 'x'); // 1MB schema name
    EXPECT_FALSE(cache.addSchema(huge_schema_name, "", 1));

    // Test thread safety (basic check)
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&cache, &success_count, i]() {
            for (int j = 0; j < 100; j++) {
                std::string schema_name = "thread" + std::to_string(i) + "_schema" + std::to_string(j);
                if (cache.addSchema(schema_name, "", 1)) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should successfully add most schemas (allowing for some contention)
    EXPECT_GT(success_count.load(), 900);
}

} // namespace Jrd

// Test Runner Main Function
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}