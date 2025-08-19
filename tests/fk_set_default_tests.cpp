#include "scratchbird/capi.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/ods.h"

#include "gtest/gtest.h"
#include <sstream>
#include <sys/stat.h>

class FKSetDefaultTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        test_dir_ = "/tmp/scratchbird_fk_setdefault_test";
        std::system(("rm -rf " + test_dir_).c_str());
        std::system(("mkdir -p " + test_dir_).c_str());

        // Create test database
        std::string base = test_dir_ + "/test.db";
        ScratchBirdCreateOptions o{};
        o.page_size = 4096;
        auto st = sb_create_database(base.c_str(), &o, &db_);
        ASSERT_EQ(st, SCRATCHBIRD_OK);

        setup_test_data();
    }

    void TearDown() override
    {
        if (db_) {
            sb_close_database(db_);
            db_ = nullptr;
        }
        std::system(("rm -rf " + test_dir_).c_str());
    }

    void setup_test_data()
    {
        // Create parent table
        auto result = execute_sql("CREATE TABLE departments (id INT PRIMARY KEY, name VARCHAR(50) "
                                  "DEFAULT 'Unknown Department')");
        ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error")
            << "Failed to create departments table";

        // Create child table with default values
        result =
            execute_sql("CREATE TABLE employees (id INT PRIMARY KEY, name VARCHAR(50), dept_id INT "
                        "DEFAULT 999, salary DECIMAL(10,2) DEFAULT 50000.00, FOREIGN KEY(dept_id) "
                        "REFERENCES departments(id) ON DELETE SET DEFAULT ON UPDATE SET DEFAULT)");
        ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error")
            << "Failed to create employees table with FK SET DEFAULT";

        // Insert test data
        execute_sql(
            "INSERT INTO departments VALUES (1, 'Engineering'), (2, 'Sales'), (3, 'Marketing')");
        execute_sql("INSERT INTO employees VALUES (101, 'Alice', 1, 75000.00), (102, 'Bob', 2, "
                    "60000.00), (103, 'Charlie', 3, 55000.00)");

        // Insert default department for testing SET DEFAULT
        execute_sql("INSERT INTO departments VALUES (999, 'Unassigned Department')");
    }

    scratchbird::engine::ExecutionResult execute_sql(const std::string& sql)
    {
        return scratchbird::engine::execute_select_sql(sql);
    }

    std::string test_dir_;
    ScratchBirdDatabase* db_ = nullptr;
};

TEST_F(FKSetDefaultTest, UpdateParentTriggersFKSetDefault)
{
    // Update a parent department ID - should trigger SET DEFAULT on child records
    auto result = execute_sql("UPDATE departments SET id = 10 WHERE id = 1");
    ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error") << "UPDATE should succeed";

    // Check that Alice's dept_id was set to default (999)
    result = execute_sql("SELECT name, dept_id FROM employees WHERE name = 'Alice'");
    ASSERT_FALSE(result.rows.empty());
    EXPECT_EQ(result.rows[0].size(), 2);
    EXPECT_EQ(result.rows[0][0], "Alice");
    EXPECT_EQ(result.rows[0][1], "999") << "Alice's dept_id should be set to default value 999";

    // Verify other employees are unchanged
    result = execute_sql(
        "SELECT name, dept_id FROM employees WHERE name IN ('Bob', 'Charlie') ORDER BY name");
    ASSERT_EQ(result.rows.size(), 2);
    EXPECT_EQ(result.rows[0][1], "2"); // Bob should still be in dept 2
    EXPECT_EQ(result.rows[1][1], "3"); // Charlie should still be in dept 3
}

TEST_F(FKSetDefaultTest, DeleteParentTriggersFKSetDefault)
{
    // Delete a parent department - should trigger SET DEFAULT on child records
    auto result = execute_sql("DELETE FROM departments WHERE id = 2");
    ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error") << "DELETE should succeed";

    // Check that Bob's dept_id was set to default (999)
    result = execute_sql("SELECT name, dept_id FROM employees WHERE name = 'Bob'");
    ASSERT_FALSE(result.rows.empty());
    EXPECT_EQ(result.rows[0].size(), 2);
    EXPECT_EQ(result.rows[0][0], "Bob");
    EXPECT_EQ(result.rows[0][1], "999") << "Bob's dept_id should be set to default value 999";

    // Verify other employees are unchanged
    result = execute_sql(
        "SELECT name, dept_id FROM employees WHERE name IN ('Alice', 'Charlie') ORDER BY name");
    ASSERT_EQ(result.rows.size(), 2);
    EXPECT_EQ(result.rows[0][1], "1"); // Alice should still be in dept 1
    EXPECT_EQ(result.rows[1][1], "3"); // Charlie should still be in dept 3
}

TEST_F(FKSetDefaultTest, MultipleChildrenSetDefault)
{
    // Insert more employees in the same department
    execute_sql(
        "INSERT INTO employees VALUES (104, 'David', 1, 70000.00), (105, 'Eve', 1, 65000.00)");

    // Delete department 1 - should affect Alice, David, and Eve
    auto result = execute_sql("DELETE FROM departments WHERE id = 1");
    ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error") << "DELETE should succeed";

    // Check that all employees in dept 1 were set to default
    result = execute_sql("SELECT name, dept_id FROM employees WHERE name IN ('Alice', 'David', "
                         "'Eve') ORDER BY name");
    ASSERT_EQ(result.rows.size(), 3);

    for (const auto& row : result.rows) {
        EXPECT_EQ(row[1], "999") << "Employee " << row[0]
                                 << " should have dept_id set to default 999";
    }
}

TEST_F(FKSetDefaultTest, DefaultValueFromDomain)
{
    // Test with domain-level defaults (if supported)
    // Create a table with domain defaults instead of column defaults
    auto result = execute_sql("CREATE DOMAIN dept_id_domain AS INT DEFAULT 777");
    if (result.columns.empty() || result.columns[0] != "error") {
        // Only run this test if domain creation is supported
        result = execute_sql("CREATE TABLE contractors (id INT PRIMARY KEY, name VARCHAR(50), "
                             "dept_id dept_id_domain, FOREIGN KEY(dept_id) REFERENCES "
                             "departments(id) ON DELETE SET DEFAULT)");
        ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error")
            << "Failed to create contractors table with domain FK";

        // Insert test data
        execute_sql("INSERT INTO contractors VALUES (201, 'Freelancer1', 3)");

        // Delete department 3 - should trigger domain default (777)
        result = execute_sql("DELETE FROM departments WHERE id = 3");
        ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error")
            << "DELETE should succeed";

        // Check that contractor's dept_id was set to domain default (777)
        result = execute_sql("SELECT name, dept_id FROM contractors WHERE name = 'Freelancer1'");
        ASSERT_FALSE(result.rows.empty());
        EXPECT_EQ(result.rows[0][1], "777")
            << "Contractor's dept_id should be set to domain default 777";
    }
}

TEST_F(FKSetDefaultTest, NullDefaultWhenNoDefaultSpecified)
{
    // Create a table without explicit defaults - should set to NULL
    auto result =
        execute_sql("CREATE TABLE interns (id INT PRIMARY KEY, name VARCHAR(50), dept_id INT, "
                    "FOREIGN KEY(dept_id) REFERENCES departments(id) ON DELETE SET DEFAULT)");
    ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error")
        << "Failed to create interns table";

    // Insert test data
    execute_sql("INSERT INTO interns VALUES (301, 'Intern1', 3)");

    // Delete department 3 - should set dept_id to NULL (no default specified)
    result = execute_sql("DELETE FROM departments WHERE id = 3");
    ASSERT_FALSE(result.columns.empty() && result.columns[0] == "error") << "DELETE should succeed";

    // Check that intern's dept_id was set to NULL
    result = execute_sql("SELECT name, dept_id FROM interns WHERE name = 'Intern1'");
    ASSERT_FALSE(result.rows.empty());
    EXPECT_EQ(result.rows[0][1], "NULL")
        << "Intern's dept_id should be set to NULL when no default exists";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
