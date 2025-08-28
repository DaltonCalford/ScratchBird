#include "scratchbird/engine/executor.h"

#include <iostream>
#include <string>

namespace scratchbird::engine
{

    void test_create_drop_index_operations()
    {
        std::cout << "Testing CREATE/DROP INDEX operations..." << std::endl;

        try {
            // Test CREATE TABLE first
            std::string create_table_sql = R"(
            CREATE TABLE test_index_table (
                id INTEGER PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                email VARCHAR(150),
                age INTEGER,
                score DECIMAL(10,2)
            )
        )";

            auto result = execute_select_sql(create_table_sql);
            if (!result.success) {
                std::cerr << "CREATE TABLE failed: " << result.error_message << std::endl;
                return;
            }
            std::cout << "✅ CREATE TABLE succeeded" << std::endl;

            // Test 1: CREATE simple single-column INDEX
            std::string create_idx1_sql = "CREATE INDEX idx_name ON test_index_table (name)";
            result = execute_select_sql(create_idx1_sql);
            if (result.success) {
                std::cout << "✅ CREATE INDEX (single column) succeeded" << std::endl;
            } else {
                std::cout << "❌ CREATE INDEX (single column) failed: " << result.error_message
                          << std::endl;
            }

            // Test 2: CREATE UNIQUE INDEX
            std::string create_unique_sql =
                "CREATE UNIQUE INDEX idx_email_unique ON test_index_table (email)";
            result = execute_select_sql(create_unique_sql);
            if (result.success) {
                std::cout << "✅ CREATE UNIQUE INDEX succeeded" << std::endl;
            } else {
                std::cout << "❌ CREATE UNIQUE INDEX failed: " << result.error_message << std::endl;
            }

            // Test 3: CREATE multi-column INDEX with directions
            std::string create_multi_sql =
                "CREATE INDEX idx_age_score ON test_index_table (age ASC, score DESC)";
            result = execute_select_sql(create_multi_sql);
            if (result.success) {
                std::cout << "✅ CREATE INDEX (multi-column with directions) succeeded"
                          << std::endl;
            } else {
                std::cout << "❌ CREATE INDEX (multi-column with directions) failed: "
                          << result.error_message << std::endl;
            }

            // Test 4: CREATE INDEX with specific method
            std::string create_btree_sql =
                "CREATE INDEX idx_name_btree ON test_index_table USING BTREE (name)";
            result = execute_select_sql(create_btree_sql);
            if (result.success) {
                std::cout << "✅ CREATE INDEX USING BTREE succeeded" << std::endl;
            } else {
                std::cout << "❌ CREATE INDEX USING BTREE failed: " << result.error_message
                          << std::endl;
            }

            // Test 5: CREATE expression INDEX (computed by)
            std::string create_expr_sql =
                "CREATE INDEX idx_name_upper ON test_index_table COMPUTED BY (UPPER(name))";
            result = execute_select_sql(create_expr_sql);
            if (result.success) {
                std::cout << "✅ CREATE INDEX (expression/computed by) succeeded" << std::endl;
            } else {
                std::cout << "❌ CREATE INDEX (expression/computed by) failed: "
                          << result.error_message << std::endl;
            }

            // Test 6: Error case - CREATE INDEX on non-existent table
            std::string create_error_sql = "CREATE INDEX idx_error ON nonexistent_table (id)";
            result = execute_select_sql(create_error_sql);
            if (!result.success) {
                std::cout << "✅ CREATE INDEX on non-existent table correctly failed: "
                          << result.error_message << std::endl;
            } else {
                std::cout << "❌ CREATE INDEX on non-existent table should have failed but didn't"
                          << std::endl;
            }

            // Test 7: Error case - CREATE INDEX without columns
            std::string create_no_cols_sql = "CREATE INDEX idx_empty ON test_index_table ()";
            result = execute_select_sql(create_no_cols_sql);
            if (!result.success) {
                std::cout << "✅ CREATE INDEX without columns correctly failed: "
                          << result.error_message << std::endl;
            } else {
                std::cout << "❌ CREATE INDEX without columns should have failed but didn't"
                          << std::endl;
            }

            // Test 8: DROP INDEX operations
            std::string drop_idx1_sql = "DROP INDEX idx_name";
            result = execute_select_sql(drop_idx1_sql);
            if (result.success) {
                std::cout << "✅ DROP INDEX succeeded" << std::endl;
            } else {
                std::cout << "❌ DROP INDEX failed: " << result.error_message << std::endl;
            }

            // Test 9: DROP UNIQUE INDEX
            std::string drop_unique_sql = "DROP INDEX idx_email_unique";
            result = execute_select_sql(drop_unique_sql);
            if (result.success) {
                std::cout << "✅ DROP INDEX (unique) succeeded" << std::endl;
            } else {
                std::cout << "❌ DROP INDEX (unique) failed: " << result.error_message << std::endl;
            }

            // Test 10: Error case - DROP non-existent INDEX
            std::string drop_error_sql = "DROP INDEX nonexistent_index";
            result = execute_select_sql(drop_error_sql);
            if (!result.success) {
                std::cout << "✅ DROP INDEX on non-existent index correctly failed: "
                          << result.error_message << std::endl;
            } else {
                std::cout << "❌ DROP INDEX on non-existent index should have failed but didn't"
                          << std::endl;
            }

            // Test 11: Error case - DROP INDEX without name
            std::string drop_no_name_sql = "DROP INDEX";
            result = execute_select_sql(drop_no_name_sql);
            if (!result.success) {
                std::cout << "✅ DROP INDEX without name correctly failed: " << result.error_message
                          << std::endl;
            } else {
                std::cout << "❌ DROP INDEX without name should have failed but didn't"
                          << std::endl;
            }

            std::cout << "\n🎯 INDEX DDL Test Summary:" << std::endl;
            std::cout << "   - CREATE INDEX (single, multi-column, unique, btree, expression)"
                      << std::endl;
            std::cout << "   - DROP INDEX with proper validation" << std::endl;
            std::cout << "   - Error handling for invalid operations" << std::endl;
            std::cout << "   - Schema resolution and table validation" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Exception during CREATE/DROP INDEX tests: " << e.what() << std::endl;
        }
    }

} // namespace scratchbird::engine

int main()
{
    scratchbird::engine::test_create_drop_index_operations();
    return 0;
}
