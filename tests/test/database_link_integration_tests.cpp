/*
 * ScratchBird v0.6.0 - Integration Tests for Database Link Schema Awareness
 * Test suite for schema-aware database links and remote schema resolution
 */

// ScratchBird includes (relative to src root)
#include "../include/firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/DatabaseLink.h"
#include "../jrd/Attachment.h"
#include "../dsql/DatabaseLinkNodes.h"
#include "../jrd/SchemaPathCache.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <map>

namespace Jrd {

class DatabaseLinkIntegrationTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        // Initialize test database and attachment
        attachment = nullptr; // Mock attachment for testing
        link_manager = nullptr; // Mock link manager
        setupTestDatabases();
    }

    void TearDown() override 
    {
        // Cleanup test resources
        cleanupTestDatabases();
    }

    void setupTestDatabases()
    {
        // Setup local and remote test databases with schema hierarchies
        local_schemas = {
            {"finance", ""},
            {"finance.accounting", "finance"},
            {"finance.accounting.reports", "finance.accounting"},
            {"hr", ""},
            {"hr.payroll", "hr"},
            {"hr.payroll.monthly", "hr.payroll"}
        };

        remote_schemas = {
            {"accounting", ""},
            {"accounting.general", "accounting"},
            {"accounting.reports", "accounting"},
            {"human_resources", ""},
            {"human_resources.payroll", "human_resources"},
            {"enterprise", ""},
            {"enterprise.business_unit", "enterprise"}
        };
    }

    void cleanupTestDatabases()
    {
        // Cleanup mock databases
    }

    Attachment* attachment;
    DatabaseLinkManager* link_manager;
    std::map<std::string, std::string> local_schemas;
    std::map<std::string, std::string> remote_schemas;
};

// Test Database Link Creation with Schema Modes
TEST_F(DatabaseLinkIntegrationTest, DatabaseLinkCreationSchemaModes)
{
    DatabaseLink link;

    // Test SCHEMA_MODE_NONE (legacy mode)
    link.setSchemaMode(SCHEMA_MODE_NONE);
    link.setLocalSchema("");
    link.setRemoteSchema("");
    EXPECT_TRUE(link.validateConfiguration());
    EXPECT_EQ(link.getSchemaMode(), SCHEMA_MODE_NONE);

    // Test SCHEMA_MODE_FIXED
    link.setSchemaMode(SCHEMA_MODE_FIXED);
    link.setLocalSchema("finance");
    link.setRemoteSchema("accounting.reports");
    EXPECT_TRUE(link.validateConfiguration());
    
    // Test invalid fixed mode (missing remote schema)
    link.setRemoteSchema("");
    EXPECT_FALSE(link.validateConfiguration());

    // Test SCHEMA_MODE_CONTEXT_AWARE
    link.setSchemaMode(SCHEMA_MODE_CONTEXT_AWARE);
    link.setRemoteSchema("CURRENT");
    EXPECT_TRUE(link.validateConfiguration());

    link.setRemoteSchema("HOME");
    EXPECT_TRUE(link.validateConfiguration());

    link.setRemoteSchema("USER");
    EXPECT_TRUE(link.validateConfiguration());

    // Test invalid context
    link.setRemoteSchema("INVALID_CONTEXT");
    EXPECT_FALSE(link.validateConfiguration());

    // Test SCHEMA_MODE_HIERARCHICAL
    link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
    link.setLocalSchema("hr");
    link.setRemoteSchema("human_resources");
    EXPECT_TRUE(link.validateConfiguration());

    // Test SCHEMA_MODE_MIRROR
    link.setSchemaMode(SCHEMA_MODE_MIRROR);
    link.setLocalSchema("");
    link.setRemoteSchema("");
    EXPECT_TRUE(link.validateConfiguration());
}

// Test Schema Resolution for Different Modes
TEST_F(DatabaseLinkIntegrationTest, SchemaResolutionModes)
{
    DatabaseLink link;
    std::string resolved_schema;

    // Test FIXED mode resolution
    link.setSchemaMode(SCHEMA_MODE_FIXED);
    link.setRemoteSchema("accounting.reports");
    
    resolved_schema = link.resolveRemoteSchema("finance.accounting.monthly", "finance", "testuser");
    EXPECT_EQ(resolved_schema, "accounting.reports");

    // Test CONTEXT_AWARE mode resolution
    link.setSchemaMode(SCHEMA_MODE_CONTEXT_AWARE);
    link.setRemoteSchema("CURRENT");
    
    resolved_schema = link.resolveRemoteSchema("finance.accounting", "finance", "testuser");
    EXPECT_EQ(resolved_schema, "finance.accounting");

    link.setRemoteSchema("HOME");
    resolved_schema = link.resolveRemoteSchema("temp.table", "finance", "testuser");
    EXPECT_EQ(resolved_schema, "finance");

    link.setRemoteSchema("USER");
    resolved_schema = link.resolveRemoteSchema("temp.table", "finance", "testuser");
    EXPECT_EQ(resolved_schema, "testuser");

    // Test HIERARCHICAL mode resolution
    link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
    link.setLocalSchema("hr");
    link.setRemoteSchema("human_resources");

    resolved_schema = link.resolveRemoteSchema("hr.payroll.monthly", "hr", "testuser");
    EXPECT_EQ(resolved_schema, "human_resources.payroll.monthly");

    resolved_schema = link.resolveRemoteSchema("hr.benefits", "hr", "testuser");
    EXPECT_EQ(resolved_schema, "human_resources.benefits");

    // Test MIRROR mode resolution
    link.setSchemaMode(SCHEMA_MODE_MIRROR);
    
    resolved_schema = link.resolveRemoteSchema("finance.accounting", "finance", "testuser");
    EXPECT_EQ(resolved_schema, "finance.accounting");
}

// Test Remote Schema Validation
TEST_F(DatabaseLinkIntegrationTest, RemoteSchemaValidation)
{
    DatabaseLink link("test_link", "remote_server:remote_db", "testuser", "testpass");

    // Mock remote schema existence check
    auto mock_schema_validator = [this](const std::string& schema) -> bool {
        return remote_schemas.find(schema) != remote_schemas.end();
    };

    // Test valid remote schemas
    EXPECT_TRUE(mock_schema_validator("accounting"));
    EXPECT_TRUE(mock_schema_validator("accounting.reports"));
    EXPECT_TRUE(mock_schema_validator("human_resources.payroll"));

    // Test invalid remote schemas
    EXPECT_FALSE(mock_schema_validator("nonexistent"));
    EXPECT_FALSE(mock_schema_validator("accounting.nonexistent"));

    // Test schema validation during link creation
    link.setSchemaMode(SCHEMA_MODE_FIXED);
    link.setRemoteSchema("accounting.reports");
    EXPECT_TRUE(link.validateSchemaAccess(remote_schemas));

    link.setRemoteSchema("nonexistent.schema");
    EXPECT_FALSE(link.validateSchemaAccess(remote_schemas));

    // Test hierarchical validation
    link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
    link.setLocalSchema("hr");
    link.setRemoteSchema("human_resources");
    EXPECT_TRUE(link.validateSchemaAccess(remote_schemas));

    // Invalid hierarchical mapping (target doesn't exist)
    link.setRemoteSchema("nonexistent_hr");
    EXPECT_FALSE(link.validateSchemaAccess(remote_schemas));
}

// Test Database Link DDL Operations
TEST_F(DatabaseLinkIntegrationTest, DatabaseLinkDDLOperations)
{
    // Test CREATE DATABASE LINK with schema targeting
    CreateDatabaseLinkNode create_node;
    create_node.linkName = "finance_link";
    create_node.connectString = "server2:finance_db";
    create_node.userName = "dbuser";
    create_node.password = "dbpass";
    create_node.schemaMode = SCHEMA_MODE_FIXED;
    create_node.remoteSchema = "accounting.reports";

    // Validate CREATE statement
    EXPECT_TRUE(create_node.validate());
    EXPECT_EQ(create_node.schemaMode, SCHEMA_MODE_FIXED);
    EXPECT_EQ(create_node.remoteSchema, "accounting.reports");

    // Test ALTER DATABASE LINK operations
    AlterDatabaseLinkNode alter_node;
    alter_node.linkName = "finance_link";
    alter_node.schemaMode = SCHEMA_MODE_HIERARCHICAL;
    alter_node.localSchema = "finance";
    alter_node.remoteSchema = "accounting";

    EXPECT_TRUE(alter_node.validate());

    // Test DROP DATABASE LINK
    DropDatabaseLinkNode drop_node;
    drop_node.linkName = "finance_link";
    EXPECT_TRUE(drop_node.validate());

    // Test invalid DDL operations
    CreateDatabaseLinkNode invalid_create;
    invalid_create.linkName = ""; // Empty name
    EXPECT_FALSE(invalid_create.validate());

    invalid_create.linkName = "valid_link";
    invalid_create.schemaMode = SCHEMA_MODE_FIXED;
    invalid_create.remoteSchema = ""; // Missing remote schema for fixed mode
    EXPECT_FALSE(invalid_create.validate());
}

// Test Query Execution with Schema-Aware Links
TEST_F(DatabaseLinkIntegrationTest, QueryExecutionSchemaAware)
{
    // Setup database link
    DatabaseLink link("hr_link", "hr_server:hr_db", "dbuser", "dbpass");
    link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
    link.setLocalSchema("hr");
    link.setRemoteSchema("human_resources");

    // Test simple query resolution
    std::string query = "SELECT * FROM employees@hr_link";
    std::string resolved_query = link.resolveQuerySchema(query, "hr.payroll", "hr", "testuser");
    std::string expected = "SELECT * FROM human_resources.payroll.employees";
    // Note: Actual implementation would handle @ syntax resolution
    
    // Test complex query with multiple tables
    query = "SELECT e.name, p.amount FROM employees@hr_link e JOIN payroll@hr_link p ON e.id = p.emp_id";
    resolved_query = link.resolveQuerySchema(query, "hr.payroll.monthly", "hr", "testuser");
    // Expected: All table references resolved to human_resources.payroll.monthly.*

    // Test query with explicit schema in current context
    query = "SELECT * FROM hr.benefits.plans@hr_link";
    resolved_query = link.resolveQuerySchema(query, "hr.payroll", "hr", "testuser");
    // Expected: Should resolve to human_resources.benefits.plans

    // Test error handling for invalid schema references
    link.setSchemaMode(SCHEMA_MODE_FIXED);
    link.setRemoteSchema("accounting.invalid");
    
    EXPECT_THROW(link.resolveQuerySchema(query, "finance", "finance", "testuser"), 
                 SchemaResolutionException);
}

// Test Connection Management with Schema Context
TEST_F(DatabaseLinkIntegrationTest, ConnectionManagementSchemaContext)
{
    DatabaseLinkManager manager;

    // Create multiple links with different schema configurations
    auto link1 = std::make_unique<DatabaseLink>("finance_link", "server1:finance_db", "user1", "pass1");
    link1->setSchemaMode(SCHEMA_MODE_FIXED);
    link1->setRemoteSchema("accounting.reports");

    auto link2 = std::make_unique<DatabaseLink>("hr_link", "server2:hr_db", "user2", "pass2");
    link2->setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
    link2->setLocalSchema("hr");
    link2->setRemoteSchema("human_resources");

    manager.addLink(std::move(link1));
    manager.addLink(std::move(link2));

    // Test link retrieval by name
    DatabaseLink* retrieved_link = manager.getLink("finance_link");
    ASSERT_NE(retrieved_link, nullptr);
    EXPECT_EQ(retrieved_link->getSchemaMode(), SCHEMA_MODE_FIXED);
    EXPECT_EQ(retrieved_link->getRemoteSchema(), "accounting.reports");

    // Test connection pooling with schema context
    Connection* conn1 = manager.getConnection("finance_link", "finance.accounting");
    Connection* conn2 = manager.getConnection("finance_link", "finance.accounting");
    
    // Should reuse connection for same schema context
    EXPECT_EQ(conn1, conn2);

    Connection* conn3 = manager.getConnection("finance_link", "finance.marketing");
    // Different schema context might use different connection
    // (depending on implementation)

    // Test connection cleanup
    manager.releaseConnection(conn1);
    manager.releaseConnection(conn3);

    // Test error handling for invalid links
    EXPECT_EQ(manager.getLink("nonexistent_link"), nullptr);
    EXPECT_THROW(manager.getConnection("nonexistent_link", "schema"), 
                 DatabaseLinkNotFoundException);
}

// Test Schema Depth and Path Optimization
TEST_F(DatabaseLinkIntegrationTest, SchemaDepthPathOptimization)
{
    DatabaseLink link;
    SchemaPathCache cache;

    // Test schema depth caching
    link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
    link.setLocalSchema("level1.level2.level3");
    link.setRemoteSchema("remote1.remote2.remote3");

    // Cache should optimize repeated path parsing
    EXPECT_EQ(link.getLocalSchemaDepth(), 3);
    EXPECT_EQ(link.getRemoteSchemaDepth(), 3);

    // Test path parsing optimization
    std::vector<std::string> local_components = link.getLocalSchemaComponents();
    ASSERT_EQ(local_components.size(), 3);
    EXPECT_EQ(local_components[0], "level1");
    EXPECT_EQ(local_components[1], "level2");
    EXPECT_EQ(local_components[2], "level3");

    // Test performance with deep hierarchies
    std::string deep_local = "l1.l2.l3.l4.l5.l6.l7.l8";
    std::string deep_remote = "r1.r2.r3.r4.r5.r6.r7.r8";
    
    link.setLocalSchema(deep_local);
    link.setRemoteSchema(deep_remote);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        std::string resolved = link.resolveRemoteSchema(deep_local + ".table", deep_local, "user");
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete 1000 resolutions in under 10ms
    EXPECT_LT(duration.count(), 10000);
}

// Test Error Handling and Recovery
TEST_F(DatabaseLinkIntegrationTest, ErrorHandlingRecovery)
{
    DatabaseLink link;

    // Test invalid schema mode
    EXPECT_THROW(link.setSchemaMode(999), InvalidSchemaModeException);

    // Test null/empty parameters
    EXPECT_THROW(link.resolveRemoteSchema("", "", ""), InvalidParameterException);
    EXPECT_THROW(link.resolveRemoteSchema("schema", "", ""), InvalidParameterException);

    // Test schema resolution failures
    link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
    link.setLocalSchema("finance");
    link.setRemoteSchema("nonexistent");

    EXPECT_THROW(link.resolveRemoteSchema("finance.accounting", "finance", "user"),
                 RemoteSchemaNotFoundException);

    // Test connection failures
    DatabaseLink failing_link("failing_link", "invalid_server:invalid_db", "user", "pass");
    EXPECT_THROW(failing_link.establishConnection(), ConnectionFailedException);

    // Test recovery mechanisms
    DatabaseLinkManager manager;
    manager.setRetryPolicy(3, std::chrono::seconds(1)); // 3 retries, 1 second apart

    // Should retry failed connections
    auto result = manager.getConnectionWithRetry("failing_link", "schema");
    EXPECT_EQ(result, nullptr); // Should fail after retries

    // Test graceful degradation
    link.setSchemaMode(SCHEMA_MODE_FIXED);
    link.setRemoteSchema("fallback_schema");
    
    // Should fall back to fixed schema when hierarchical resolution fails
    std::string fallback = link.resolveRemoteSchemaWithFallback("invalid.path", "invalid", "user");
    EXPECT_EQ(fallback, "fallback_schema");
}

// Test Transaction Support with Schema Links
TEST_F(DatabaseLinkIntegrationTest, TransactionSupportSchemaLinks)
{
    DatabaseLink link("trans_link", "server:db", "user", "pass");
    link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);

    // Test transaction propagation across links
    Transaction trans;
    trans.begin();

    // Execute operations on linked database
    auto conn = link.getConnection(&trans);
    EXPECT_NE(conn, nullptr);

    // Test schema context in transactions
    std::string current_schema = conn->getCurrentSchema();
    conn->setCurrentSchema("finance.accounting");
    
    // Operations should use the set schema context
    std::string resolved = link.resolveRemoteSchema("accounts", "finance.accounting", "user");
    EXPECT_TRUE(resolved.find("finance.accounting") != std::string::npos ||
                resolved.find("accounts") != std::string::npos);

    // Test rollback with schema context
    trans.rollback();
    
    // Schema context should be restored
    EXPECT_EQ(conn->getCurrentSchema(), current_schema);

    // Test commit with schema operations
    trans.begin();
    conn->setCurrentSchema("hr.payroll");
    trans.commit();
    
    // Schema context should persist after commit
    EXPECT_EQ(conn->getCurrentSchema(), "hr.payroll");
}

// Test Concurrent Access and Thread Safety
TEST_F(DatabaseLinkIntegrationTest, ConcurrentAccessThreadSafety)
{
    DatabaseLinkManager manager;
    auto link = std::make_unique<DatabaseLink>("concurrent_link", "server:db", "user", "pass");
    link->setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
    manager.addLink(std::move(link));

    std::atomic<int> success_count(0);
    std::atomic<int> error_count(0);
    std::vector<std::thread> threads;

    // Test concurrent schema resolution
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&manager, &success_count, &error_count, i]() {
            try {
                auto* link = manager.getLink("concurrent_link");
                for (int j = 0; j < 100; j++) {
                    std::string schema = "schema" + std::to_string(i) + "_" + std::to_string(j);
                    std::string resolved = link->resolveRemoteSchema(schema, "base", "user");
                    if (!resolved.empty()) {
                        success_count++;
                    }
                }
            } catch (...) {
                error_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most operations should succeed
    EXPECT_GT(success_count.load(), 900);
    EXPECT_LT(error_count.load(), 100);

    // Test concurrent connection management
    threads.clear();
    success_count = 0;
    error_count = 0;

    for (int i = 0; i < 5; i++) {
        threads.emplace_back([&manager, &success_count, &error_count, i]() {
            try {
                for (int j = 0; j < 50; j++) {
                    std::string schema = "schema" + std::to_string(i);
                    auto* conn = manager.getConnection("concurrent_link", schema);
                    if (conn) {
                        success_count++;
                        manager.releaseConnection(conn);
                    }
                }
            } catch (...) {
                error_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Connection management should be thread-safe
    EXPECT_GT(success_count.load(), 200);
    EXPECT_EQ(error_count.load(), 0);
}

} // namespace Jrd

// Test Runner Main Function
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}