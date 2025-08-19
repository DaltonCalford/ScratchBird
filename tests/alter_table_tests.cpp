#include "scratchbird/engine/executor.h"

#include <iostream>
#include <string>

namespace scratchbird::engine
{

    void test_alter_table_operations()
    {
        std::cout << "Testing ALTER TABLE operations..." << std::endl;

        try {
            // Test CREATE TABLE first
            std::string create_sql = R"(
            CREATE TABLE test_alter (
                id INTEGER PRIMARY KEY,
                name VARCHAR(50) NOT NULL,
                email VARCHAR(100)
            )
        )";

            auto result = execute_ddl_sql(create_sql);
            if (!result.success) {
                std::cerr << "CREATE TABLE failed: " << result.error_message << std::endl;
                return;
            }
            std::cout << "✅ CREATE TABLE succeeded" << std::endl;

            // Test ADD COLUMN
            std::string add_col_sql = "ALTER TABLE test_alter ADD COLUMN age INTEGER";
            result = execute_ddl_sql(add_col_sql);
            if (result.success) {
                std::cout << "✅ ALTER TABLE ADD COLUMN succeeded" << std::endl;
            } else {
                std::cout << "❌ ALTER TABLE ADD COLUMN failed: " << result.error_message
                          << std::endl;
            }

            // Test ALTER COLUMN TYPE
            std::string alter_type_sql =
                "ALTER TABLE test_alter ALTER COLUMN name TYPE VARCHAR(100)";
            result = execute_ddl_sql(alter_type_sql);
            if (result.success) {
                std::cout << "✅ ALTER TABLE ALTER COLUMN TYPE succeeded" << std::endl;
            } else {
                std::cout << "❌ ALTER TABLE ALTER COLUMN TYPE failed: " << result.error_message
                          << std::endl;
            }

            // Test SET DEFAULT
            std::string set_default_sql = "ALTER TABLE test_alter ALTER COLUMN age SET DEFAULT 0";
            result = execute_ddl_sql(set_default_sql);
            if (result.success) {
                std::cout << "✅ ALTER TABLE SET DEFAULT succeeded" << std::endl;
            } else {
                std::cout << "❌ ALTER TABLE SET DEFAULT failed: " << result.error_message
                          << std::endl;
            }

            // Test RENAME COLUMN
            std::string rename_col_sql =
                "ALTER TABLE test_alter RENAME COLUMN email TO email_address";
            result = execute_ddl_sql(rename_col_sql);
            if (result.success) {
                std::cout << "✅ ALTER TABLE RENAME COLUMN succeeded" << std::endl;
            } else {
                std::cout << "❌ ALTER TABLE RENAME COLUMN failed: " << result.error_message
                          << std::endl;
            }

            // Test DROP COLUMN
            std::string drop_col_sql = "ALTER TABLE test_alter DROP COLUMN age";
            result = execute_ddl_sql(drop_col_sql);
            if (result.success) {
                std::cout << "✅ ALTER TABLE DROP COLUMN succeeded" << std::endl;
            } else {
                std::cout << "❌ ALTER TABLE DROP COLUMN failed: " << result.error_message
                          << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "Exception during ALTER TABLE tests: " << e.what() << std::endl;
        }
    }

} // namespace scratchbird::engine

int main()
{
    scratchbird::engine::test_alter_table_operations();
    return 0;
}
