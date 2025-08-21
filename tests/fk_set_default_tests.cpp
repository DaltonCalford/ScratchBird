#include "scratchbird/capi.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/parser.h"

#include "gtest/gtest.h"
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace scratchbird::engine;

static void run_ddl(const std::string& sql)
{
    auto ast = parse_sql(sql);
    execute_ast(ast);
}

static std::string tempdb()
{
    // Use project-local temp dir to avoid /tmp space constraints
    const char* root = "/home/dcalford/CliWork/ScratchBird/temp";
    mkdir(root, 0755);
    std::ostringstream oss;
    oss << root << "/db_" << getpid() << "_" << (unsigned long long)time(nullptr);
    return oss.str();
}

static void create_db_and_set_path(const std::string& base)
{
    SB_CreateDbOptions o{};
    o.page_size = 4096;
    SB_Database* db = nullptr;
    auto st = sb_create_database(base.c_str(), &o, &db);
    (void)st;
    if (db)
        sb_close_database(db);
    set_executor_db_path(base);
    // Ensure catalog roots are bootstrapped before any DDL
    CatalogManager cm(base);
    cm.bootstrap_if_needed();
}

static void cleanup_db(const std::string& base)
{
    // Best-effort: remove segment files base.seg0..base.seg15
    for (int i = 0; i < 16; ++i) {
        std::string seg = base + ".seg" + std::to_string(i);
        unlink(seg.c_str());
    }
    // Also try to remove any .bootstrap.sql file
    std::string bootstrap = base + ".bootstrap.sql";
    unlink(bootstrap.c_str());
}

class FKSetDefaultTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        db_path_ = tempdb();
        create_db_and_set_path(db_path_);
        setup_test_data();
    }

    void TearDown() override
    {
        cleanup_db(db_path_);
    }

    void setup_test_data()
    {
        // Create schema
        run_ddl("CREATE SCHEMA public");

        // Create parent table
        run_ddl("CREATE TABLE public.departments (id INT PRIMARY KEY, name VARCHAR(50) "
                "DEFAULT 'Unknown Department')");

        // Create child table with default values and FK SET DEFAULT
        run_ddl("CREATE TABLE public.employees (id INT PRIMARY KEY, name VARCHAR(50), dept_id INT "
                "DEFAULT 999, salary DECIMAL(10,2) DEFAULT 50000.00, FOREIGN KEY(dept_id) "
                "REFERENCES public.departments(id) ON DELETE SET DEFAULT ON UPDATE SET DEFAULT)");

        // Insert test data
        execute_insert_sql("INSERT INTO public.departments VALUES (1, 'Engineering'), (2, "
                           "'Sales'), (3, 'Marketing')");
        execute_insert_sql(
            "INSERT INTO public.employees VALUES (101, 'Alice', 1, 75000.00), (102, 'Bob', 2, "
            "60000.00), (103, 'Charlie', 3, 55000.00)");

        // Insert default department for testing SET DEFAULT
        execute_insert_sql("INSERT INTO public.departments VALUES (999, 'Unassigned Department')");
    }

    std::string db_path_;
};

TEST_F(FKSetDefaultTest, UpdateParentTriggersFKSetDefault)
{
    // Update a parent department ID - should trigger SET DEFAULT on child records
    auto result = execute_update_sql("UPDATE public.departments SET id = 10 WHERE id = 1");
    EXPECT_TRUE(result.success) << "UPDATE should succeed: " << result.error_message;

    // Check that Alice's dept_id was set to default (999)
    result = execute_select_sql("SELECT name, dept_id FROM public.employees WHERE name = 'Alice'");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    ASSERT_EQ(result.rows.size(), 1);
    EXPECT_EQ(result.rows[0][0], "Alice");
    EXPECT_EQ(result.rows[0][1], "999"); // Should be set to default value

    // Verify other employees are unaffected
    result = execute_select_sql("SELECT name, dept_id FROM public.employees ORDER BY name");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    ASSERT_EQ(result.rows.size(), 3);

    // Alice should have default dept_id, others unchanged
    EXPECT_EQ(result.rows[0][0], "Alice");
    EXPECT_EQ(result.rows[0][1], "999"); // SET DEFAULT
    EXPECT_EQ(result.rows[1][0], "Bob");
    EXPECT_EQ(result.rows[1][1], "2"); // Bob should still be in dept 2
    EXPECT_EQ(result.rows[2][0], "Charlie");
    EXPECT_EQ(result.rows[2][1], "3"); // Charlie should still be in dept 3
}

TEST_F(FKSetDefaultTest, DeleteParentTriggersFKSetDefault)
{
    // Delete a parent department - should trigger SET DEFAULT on child records
    auto result = execute_delete_sql("DELETE FROM public.departments WHERE id = 2");
    EXPECT_TRUE(result.success) << "DELETE should succeed: " << result.error_message;

    // Check that Bob's dept_id was set to default (999)
    result = execute_select_sql("SELECT name, dept_id FROM public.employees WHERE name = 'Bob'");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    ASSERT_EQ(result.rows.size(), 1);
    EXPECT_EQ(result.rows[0][0], "Bob");
    EXPECT_EQ(result.rows[0][1], "999"); // Should be set to default value

    // Verify other employees are unaffected
    result = execute_select_sql(
        "SELECT name, dept_id FROM public.employees WHERE name != 'Bob' ORDER BY name");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    ASSERT_EQ(result.rows.size(), 2);
    EXPECT_EQ(result.rows[0][0], "Alice");
    EXPECT_EQ(result.rows[0][1], "1"); // Alice should still be in dept 1
    EXPECT_EQ(result.rows[1][0], "Charlie");
    EXPECT_EQ(result.rows[1][1], "3"); // Charlie should still be in dept 3
}

TEST_F(FKSetDefaultTest, MultipleChildrenSetDefault)
{
    // Insert more employees in same department
    execute_insert_sql("INSERT INTO public.employees VALUES (104, 'Dave', 1, 65000.00), (105, "
                       "'Eve', 1, 70000.00)");

    // Delete department 1 - should set all employees in dept 1 to default
    auto result = execute_delete_sql("DELETE FROM public.departments WHERE id = 1");
    EXPECT_TRUE(result.success) << "DELETE should succeed: " << result.error_message;

    // Check that all employees who were in dept 1 are now in default dept
    result =
        execute_select_sql("SELECT name FROM public.employees WHERE dept_id = 999 ORDER BY name");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    EXPECT_EQ(result.rows.size(), 3); // Alice, Dave, Eve should all be in default dept
    EXPECT_EQ(result.rows[0][0], "Alice");
    EXPECT_EQ(result.rows[1][0], "Dave");
    EXPECT_EQ(result.rows[2][0], "Eve");

    // Verify employees in other departments are unaffected
    result = execute_select_sql(
        "SELECT name, dept_id FROM public.employees WHERE dept_id != 999 ORDER BY name");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    EXPECT_EQ(result.rows.size(), 2); // Bob and Charlie should be unaffected
    EXPECT_EQ(result.rows[0][0], "Bob");
    EXPECT_EQ(result.rows[0][1], "2");
    EXPECT_EQ(result.rows[1][0], "Charlie");
    EXPECT_EQ(result.rows[1][1], "3");
}

TEST_F(FKSetDefaultTest, NullDefaultWhenNoDefaultSpecified)
{
    // Create a table without explicit defaults - should set to NULL
    run_ddl("CREATE TABLE public.interns (id INT PRIMARY KEY, name VARCHAR(50), dept_id INT, "
            "FOREIGN KEY(dept_id) REFERENCES public.departments(id) ON DELETE SET DEFAULT)");

    // Insert test data
    execute_insert_sql("INSERT INTO public.interns VALUES (301, 'Intern1', 3)");

    // Delete department 3 - should set intern's dept_id to NULL (no default specified)
    auto result = execute_delete_sql("DELETE FROM public.departments WHERE id = 3");
    EXPECT_TRUE(result.success) << "DELETE should succeed: " << result.error_message;

    // Check that Intern1's dept_id was set to NULL
    result = execute_select_sql("SELECT name, dept_id FROM public.interns WHERE name = 'Intern1'");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    ASSERT_EQ(result.rows.size(), 1);
    EXPECT_EQ(result.rows[0][0], "Intern1");
    // NULL values should be represented as empty string or specific NULL representation
    EXPECT_TRUE(result.rows[0][1].empty() || result.rows[0][1] == "NULL");
}

TEST_F(FKSetDefaultTest, CascadeVsSetDefault)
{
    // Test that SET DEFAULT works correctly alongside other FK actions
    // Create another child table with CASCADE delete
    run_ddl("CREATE TABLE public.projects (id INT PRIMARY KEY, name VARCHAR(50), dept_id INT, "
            "FOREIGN KEY(dept_id) REFERENCES public.departments(id) ON DELETE CASCADE)");

    execute_insert_sql(
        "INSERT INTO public.projects VALUES (401, 'Project Alpha', 2), (402, 'Project Beta', 2)");

    // Delete department 2
    auto result = execute_delete_sql("DELETE FROM public.departments WHERE id = 2");
    EXPECT_TRUE(result.success) << "DELETE should succeed: " << result.error_message;

    // Check employees (SET DEFAULT) - Bob should be in default dept
    result = execute_select_sql("SELECT name, dept_id FROM public.employees WHERE name = 'Bob'");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    ASSERT_EQ(result.rows.size(), 1);
    EXPECT_EQ(result.rows[0][1], "999"); // SET DEFAULT

    // Check projects (CASCADE) - should be deleted
    result = execute_select_sql("SELECT name FROM public.projects");
    EXPECT_TRUE(result.success) << "SELECT should succeed: " << result.error_message;
    EXPECT_EQ(result.rows.size(), 0); // CASCADE delete should remove all projects
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
