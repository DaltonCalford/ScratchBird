#include "scratchbird/capi.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace scratchbird::engine;

static std::string tempdb()
{
    const char* root = "/home/dcalford/CliWork/ScratchBird/temp";
    mkdir(root, 0755);
    std::ostringstream oss;
    oss << root << "/debug_" << getpid() << "_" << (unsigned long long)time(nullptr);
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
    CatalogManager cm(get_executor_db_path());
    cm.bootstrap_if_needed();
    if (!cm.lookup_schema_oid_by_name("public")) {
        UuidBytes gen{};
        {
            std::hash<std::string> h;
            auto v = h(std::string("public"));
            memcpy(gen.data(), &v, std::min(sizeof(v), gen.size()));
        }
        cm.create_schema(gen, "public", std::nullopt, "public schema");
    }
}

ExecutionResult execute_sql(const std::string& sql)
{
    std::string sql_upper = sql;
    std::transform(sql_upper.begin(), sql_upper.end(), sql_upper.begin(), ::toupper);

    // Remove leading whitespace
    size_t start = sql_upper.find_first_not_of(" \t\n\r");
    if (start != std::string::npos) {
        sql_upper = sql_upper.substr(start);
    }

    // Route to appropriate executor based on SQL type
    if (sql_upper.substr(0, 6) == "INSERT") {
        return execute_insert_sql(sql);
    } else if (sql_upper.substr(0, 6) == "UPDATE") {
        return execute_update_sql(sql);
    } else if (sql_upper.substr(0, 6) == "DELETE") {
        return execute_delete_sql(sql);
    } else if (sql_upper.substr(0, 6) == "SELECT") {
        return execute_select_sql(sql);
    } else {
        // For DDL statements, use AST-based execution
        auto ast = parse_sql(sql);
        return execute_ast(ast);
    }
}

void print_result(const ExecutionResult& result)
{
    std::cout << "Columns: " << result.columns.size() << std::endl;
    for (size_t i = 0; i < result.columns.size(); ++i) {
        std::cout << "  [" << i << "] " << result.columns[i] << std::endl;
    }
    std::cout << "Rows: " << result.rows.size() << std::endl;
    for (size_t i = 0; i < result.rows.size(); ++i) {
        std::cout << "  Row " << i << ": ";
        for (size_t j = 0; j < result.rows[i].size(); ++j) {
            std::cout << "[" << result.rows[i][j] << "] ";
        }
        std::cout << std::endl;
    }
}

int main()
{
    std::cout << "=== Simple Debug Test ===" << std::endl;

    std::string db_path = tempdb();
    create_db_and_set_path(db_path);
    std::cout << "Database created at: " << db_path << std::endl;

    // Test 1: CREATE TABLE
    std::cout << "\n1. CREATE TABLE" << std::endl;
    auto result = execute_sql("CREATE TABLE test (id INTEGER, name VARCHAR(50))");
    print_result(result);

    // Test 2: INSERT data
    std::cout << "\n2. INSERT data" << std::endl;
    result = execute_sql("INSERT INTO test VALUES (1, 'Alice')");
    print_result(result);

    result = execute_sql("INSERT INTO test VALUES (2, 'Bob')");
    print_result(result);

    // Test 3: SELECT all
    std::cout << "\n3. SELECT * FROM test" << std::endl;
    result = execute_sql("SELECT * FROM test");
    print_result(result);

    // Test 4: COUNT
    std::cout << "\n4. SELECT COUNT(*) FROM test" << std::endl;
    result = execute_sql("SELECT COUNT(*) FROM test");
    print_result(result);

    return 0;
}
