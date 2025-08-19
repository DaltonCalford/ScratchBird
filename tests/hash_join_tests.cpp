#include "scratchbird/capi.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/executor_nodes.h"
#include "scratchbird/engine/parser.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace scratchbird::engine
{

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
        CatalogManager cm(get_executor_db_path());
        cm.bootstrap_if_needed();
        // Ensure default schema 'public' exists for SELECT executor and trigger runners
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

    // Helper function to execute SQL and check for errors
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

    bool has_error(const ExecutionResult& result)
    {
        return result.columns.size() > 0 && result.columns[0] == "error";
    }

    std::string get_error_message(const ExecutionResult& result)
    {
        if (has_error(result) && !result.rows.empty()) {
            return result.rows[0][0];
        }
        return "unknown error";
    }

    void print_result(const ExecutionResult& result)
    {
        if (has_error(result)) {
            std::cout << "❌ Error: " << get_error_message(result) << std::endl;
            return;
        }

        // Print columns
        for (size_t i = 0; i < result.columns.size(); ++i) {
            std::cout << result.columns[i];
            if (i < result.columns.size() - 1)
                std::cout << " | ";
        }
        std::cout << std::endl;

        // Print separator
        for (size_t i = 0; i < result.columns.size(); ++i) {
            std::cout << std::string(result.columns[i].length(), '-');
            if (i < result.columns.size() - 1)
                std::cout << "-+-";
        }
        std::cout << std::endl;

        // Print rows
        for (const auto& row : result.rows) {
            for (size_t i = 0; i < row.size() && i < result.columns.size(); ++i) {
                std::cout << row[i];
                if (i < std::min(row.size(), result.columns.size()) - 1)
                    std::cout << " | ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    void test_hash_join_basic()
    {
        std::cout << "=== Testing Hash Join Implementation ===" << std::endl;

        try {
            // Create execution context
            ExecutorContext ctx;
            ctx.db_path = get_executor_db_path();

            // Create mock data for testing
            // Left table: employees (id, name)
            auto left_scan = std::make_unique<SeqScanNode>("public", "employees", "e");

            // Right table: departments (id, name)
            auto right_scan = std::make_unique<SeqScanNode>("public", "departments", "d");

            // Create hash join: employees.dept_id = departments.id
            std::vector<std::string> left_keys = {"dept_id"};
            std::vector<std::string> right_keys = {"id"};

            auto hash_join =
                std::make_unique<HashJoinNode>(std::move(left_scan), std::move(right_scan),
                                               left_keys, right_keys, HashJoinNode::Inner);

            // Execute join
            std::cout << "\n🔍 Test 1: Basic Hash Join (Inner)" << std::endl;
            std::cout << "Columns: ";
            for (const auto& col : hash_join->columns()) {
                std::cout << col << " ";
            }
            std::cout << std::endl;

            hash_join->open(ctx);

            Tuple result;
            int row_count = 0;
            while (hash_join->next(result)) {
                std::cout << "Row " << row_count++ << ": ";
                for (const auto& val : result) {
                    std::cout << "[" << val.bytes << "] ";
                }
                std::cout << std::endl;
            }

            hash_join->close();

            // Print instrumentation
            const auto& instr = hash_join->get_instrumentation();
            std::cout << "Instrumentation:" << std::endl;
            std::cout << "  Input rows: " << instr.input_rows << std::endl;
            std::cout << "  Output rows: " << instr.output_rows << std::endl;
            std::cout << "  Memory peak (bytes): " << instr.memory_bytes_peak << std::endl;
            std::cout << "  Wall time (ms): " << instr.wall_time_ms << std::endl;

        } catch (const std::exception& e) {
            std::cout << "❌ Exception during hash join test: " << e.what() << std::endl;
        }
    }

    void test_nested_loop_join_basic()
    {
        std::cout << "\n=== Testing Nested Loop Join Implementation ===" << std::endl;

        try {
            // Create execution context
            ExecutorContext ctx;
            ctx.db_path = get_executor_db_path();

            // Create mock data for testing
            auto left_scan = std::make_unique<SeqScanNode>("public", "test", "t1");
            auto right_scan = std::make_unique<SeqScanNode>("public", "test", "t2");

            // Create nested loop join with basic predicate
            auto nlj =
                std::make_unique<NestedLoopJoinNode>(std::move(left_scan), std::move(right_scan),
                                                     "t1.id = t2.id", NestedLoopJoinNode::Inner);

            std::cout << "\n🔍 Test 2: Nested Loop Join (Inner)" << std::endl;
            std::cout << "Columns: ";
            for (const auto& col : nlj->columns()) {
                std::cout << col << " ";
            }
            std::cout << std::endl;

            nlj->open(ctx);

            Tuple result;
            int row_count = 0;
            while (nlj->next(result) && row_count < 5) { // Limit to avoid infinite loop
                std::cout << "Row " << row_count++ << ": ";
                for (const auto& val : result) {
                    std::cout << "[" << val.bytes << "] ";
                }
                std::cout << std::endl;
            }

            nlj->close();

            // Print instrumentation
            const auto& instr = nlj->get_instrumentation();
            std::cout << "Instrumentation:" << std::endl;
            std::cout << "  Input rows: " << instr.input_rows << std::endl;
            std::cout << "  Output rows: " << instr.output_rows << std::endl;
            std::cout << "  Wall time (ms): " << instr.wall_time_ms << std::endl;

        } catch (const std::exception& e) {
            std::cout << "❌ Exception during nested loop join test: " << e.what() << std::endl;
        }
    }

} // namespace scratchbird::engine

int main()
{
    using namespace scratchbird::engine;

    // Setup database
    std::string db_path = tempdb();
    create_db_and_set_path(db_path);
    std::cout << "✅ Database created at: " << db_path << std::endl;

    // Run tests
    test_hash_join_basic();
    test_nested_loop_join_basic();

    std::cout << "\n🎯 Hash Join Implementation Summary:" << std::endl;
    std::cout << "   - Created node-based executor architecture (Volcano-style)" << std::endl;
    std::cout << "   - Implemented HashJoinNode with build/probe phases" << std::endl;
    std::cout << "   - Implemented NestedLoopJoinNode for comparison" << std::endl;
    std::cout << "   - Added SeqScanNode with mock data support" << std::endl;
    std::cout << "   - Included comprehensive instrumentation" << std::endl;
    std::cout << "   - Foundation ready for integration with real heap scanning" << std::endl;

    return 0;
}
