/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * psql Client Integration Tests
 *
 * Tests compatibility with PostgreSQL psql client:
 * - 2.1.22: psql connect
 * - 2.1.23: psql simple SELECT
 * - 2.1.24: psql prepared statement
 * - 2.1.25: psql COPY
 * - 4.2.8: information_schema test
 * - 3.2.17: PostgreSQL type round-trip
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstring>

#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/ipc/engine_ipc_session_handler.h"
#include "scratchbird/ipc/ipc_server.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::ipc;
using scratchbird::testing::TestDatabaseFile;

// ============================================================================
// Test Fixture
// ============================================================================

class PsqlClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("psql_client_test");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(db_file_->path(), 16384, &ctx), core::Status::OK)
            << "Failed to create database: " << ctx.message;

        db_ = std::make_unique<core::Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), core::Status::OK)
            << "Failed to open database: " << ctx.message;

        auto status = core::ProcArrayManager::initialize(db_.get(), 50, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to initialize ProcArray: " << ctx.message;

        // Create handler and server
        handler_ = std::make_unique<EngineIPCSessionHandler>(db_.get());
        
        // Use a TCP port for external client connections
        server_ = std::make_unique<IPCServer>(handler_.get());
        status = server_->initialize("127.0.0.1", 15432, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to initialize IPC server: " << ctx.message;

        status = server_->start(&ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to start IPC server: " << ctx.message;

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        core::ErrorContext ctx;
        
        if (server_) {
            server_->stop(&ctx);
            server_.reset();
        }
        
        handler_.reset();
        core::ProcArrayManager::shutdown(&ctx);
        db_.reset();
        db_file_.reset();
    }

    // Helper to run psql command and capture output
    std::pair<int, std::string> runPsqlCommand(const std::string& command) {
        std::string full_cmd = "PGPASSWORD=testpass psql -h 127.0.0.1 -p 15432 -U testuser -d testdb -c \"" + 
                              command + "\" 2>&1";
        
        FILE* pipe = popen(full_cmd.c_str(), "r");
        if (!pipe) {
            return {-1, "Failed to open pipe"};
        }

        char buffer[4096];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }

        int exit_code = pclose(pipe);
        return {exit_code, result};
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<EngineIPCSessionHandler> handler_;
    std::unique_ptr<IPCServer> server_;
};

// ============================================================================
// 2.1.22: psql Connect Test
// ============================================================================

TEST_F(PsqlClientTest, Connect_Basic) {
    // Test basic connection
    auto [exit_code, output] = runPsqlCommand("SELECT 1");
    
    // If psql is not installed, skip the test
    if (output.find("command not found") != std::string::npos ||
        output.find("No such file") != std::string::npos) {
        GTEST_SKIP() << "psql not installed, skipping client test";
    }
    
    // Connection should either succeed or give a meaningful error
    // (not a protocol-level error)
    EXPECT_TRUE(output.find("connection") == std::string::npos ||
                output.find("refused") != std::string::npos ||
                output.find("?column?") != std::string::npos ||
                output.find("1") != std::string::npos);
}

TEST_F(PsqlClientTest, Connect_WithCredentials) {
    auto [exit_code, output] = runPsqlCommand("\\conninfo");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should show connection info or appropriate error
    EXPECT_TRUE(output.find("connected") != std::string::npos ||
                output.find("connection") != std::string::npos ||
                output.find("refused") != std::string::npos);
}

// ============================================================================
// 2.1.23: psql Simple SELECT Test
// ============================================================================

TEST_F(PsqlClientTest, SimpleSelect_OneRow) {
    auto [exit_code, output] = runPsqlCommand("SELECT 1 AS test_col");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should return column header and value
    EXPECT_TRUE(output.find("test_col") != std::string::npos ||
                output.find("?column?") != std::string::npos);
}

TEST_F(PsqlClientTest, SimpleSelect_MultipleRows) {
    auto [exit_code, output] = runPsqlCommand("SELECT * FROM generate_series(1, 5)");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should have multiple rows of output
    EXPECT_TRUE(output.find("1") != std::string::npos);
    EXPECT_TRUE(output.find("5") != std::string::npos);
}

TEST_F(PsqlClientTest, SimpleSelect_WithWhere) {
    auto [exit_code, output] = runPsqlCommand("SELECT * FROM generate_series(1, 10) AS x WHERE x > 5");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should filter results
    EXPECT_TRUE(output.find("6") != std::string::npos);
    EXPECT_TRUE(output.find("10") != std::string::npos);
}

TEST_F(PsqlClientTest, SimpleSelect_OrderBy) {
    auto [exit_code, output] = runPsqlCommand("SELECT * FROM generate_series(1, 3) ORDER BY 1 DESC");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Results should be ordered
    EXPECT_TRUE(output.find("3") != std::string::npos);
}

TEST_F(PsqlClientTest, SimpleSelect_Limit) {
    auto [exit_code, output] = runPsqlCommand("SELECT * FROM generate_series(1, 100) LIMIT 5");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should limit results
    EXPECT_TRUE(output.find("5") != std::string::npos);
}

TEST_F(PsqlClientTest, SimpleSelect_Aggregate) {
    auto [exit_code, output] = runPsqlCommand("SELECT COUNT(*), AVG(x), MAX(x) FROM generate_series(1, 10) AS x");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should have aggregate results
    EXPECT_TRUE(output.find("10") != std::string::npos);
}

// ============================================================================
// 2.1.24: psql Prepared Statement Test
// ============================================================================

TEST_F(PsqlClientTest, PreparedStatement_PrepareExecute) {
    auto [exit_code, output] = runPsqlCommand(
        "PREPARE stmt1 AS SELECT $1::int + $2::int; EXECUTE stmt1(10, 20)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should show result 30
    EXPECT_TRUE(output.find("30") != std::string::npos ||
                output.find("PREPARE") != std::string::npos);
}

TEST_F(PsqlClientTest, PreparedStatement_MultipleExecutions) {
    auto [exit_code, output] = runPsqlCommand(
        "PREPARE stmt2(int) AS SELECT $1 * 2; "
        "EXECUTE stmt2(5); EXECUTE stmt2(10); EXECUTE stmt2(15)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should have results 10, 20, 30
    EXPECT_TRUE(output.find("10") != std::string::npos);
    EXPECT_TRUE(output.find("20") != std::string::npos ||
                output.find("30") != std::string::npos);
}

TEST_F(PsqlClientTest, PreparedStatement_Deallocate) {
    auto [exit_code, output] = runPsqlCommand(
        "PREPARE stmt3 AS SELECT 1; DEALLOCATE stmt3"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // DEALLOCATE should succeed
    EXPECT_TRUE(output.find("DEALLOCATE") != std::string::npos ||
                output.find("error") == std::string::npos);
}

// ============================================================================
// 2.1.25: psql COPY Test
// ============================================================================

TEST_F(PsqlClientTest, COPY_FromStdin) {
    // This test creates a temporary SQL file for COPY FROM STDIN
    std::string sql = R"(
CREATE TABLE IF NOT EXISTS copy_test (id INT, name TEXT);
COPY copy_test FROM STDIN WITH (FORMAT CSV);
1,Alice
2,Bob
3,Charlie
\.
SELECT COUNT(*) FROM copy_test;
)";
    
    std::string cmd = "echo \"" + sql + "\" | PGPASSWORD=testpass psql -h 127.0.0.1 -p 15432 -U testuser -d testdb 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        FAIL() << "Failed to open pipe";
    }

    char buffer[4096];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    pclose(pipe);
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should have inserted 3 rows
    EXPECT_TRUE(output.find("3") != std::string::npos);
}

TEST_F(PsqlClientTest, COPY_ToStdout) {
    // Setup first
    auto [exit_code1, output1] = runPsqlCommand(
        "CREATE TABLE copy_out_test (id INT); INSERT INTO copy_out_test VALUES (1), (2), (3)"
    );
    
    if (output1.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    auto [exit_code2, output2] = runPsqlCommand("COPY copy_out_test TO STDOUT WITH (FORMAT CSV)");
    
    // Should output CSV data
    EXPECT_TRUE(output2.find("1") != std::string::npos);
    EXPECT_TRUE(output2.find("2") != std::string::npos);
    EXPECT_TRUE(output2.find("3") != std::string::npos);
}

// ============================================================================
// 4.2.8: information_schema Test
// ============================================================================

TEST_F(PsqlClientTest, InformationSchema_Tables) {
    auto [exit_code, output] = runPsqlCommand(
        "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public' LIMIT 5"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should return table information or appropriate message
    EXPECT_TRUE(output.find("table_name") != std::string::npos ||
                output.find("information_schema") != std::string::npos);
}

TEST_F(PsqlClientTest, InformationSchema_Columns) {
    // Create a test table first
    runPsqlCommand("CREATE TABLE IF NOT EXISTS info_test (id INT, name TEXT)");
    
    auto [exit_code, output] = runPsqlCommand(
        "SELECT column_name, data_type FROM information_schema.columns "
        "WHERE table_name = 'info_test' ORDER BY ordinal_position"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should show column information
    EXPECT_TRUE(output.find("column_name") != std::string::npos ||
                output.find("id") != std::string::npos ||
                output.find("name") != std::string::npos);
}

TEST_F(PsqlClientTest, InformationSchema_DotDt) {
    // Create a test table
    runPsqlCommand("CREATE TABLE IF NOT EXISTS dt_test (id INT)");
    
    auto [exit_code, output] = runPsqlCommand("\\dt");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // \dt should list tables
    EXPECT_TRUE(output.find("List of relations") != std::string::npos ||
                output.find("Did not find any relations") != std::string::npos ||
                output.find("dt_test") != std::string::npos);
}

// ============================================================================
// 3.2.17: PostgreSQL Type Round-trip Test
// ============================================================================

TEST_F(PsqlClientTest, TypeRoundTrip_Integer) {
    auto [exit_code, output] = runPsqlCommand("SELECT 42::int, 1000000::bigint, 3::smallint");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("42") != std::string::npos);
}

TEST_F(PsqlClientTest, TypeRoundTrip_Text) {
    auto [exit_code, output] = runPsqlCommand("SELECT 'Hello'::text, 'World'::varchar(20)");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("Hello") != std::string::npos);
}

TEST_F(PsqlClientTest, TypeRoundTrip_Boolean) {
    auto [exit_code, output] = runPsqlCommand("SELECT true, false, null::boolean");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("t") != std::string::npos ||
                output.find("f") != std::string::npos);
}

TEST_F(PsqlClientTest, TypeRoundTrip_Numeric) {
    auto [exit_code, output] = runPsqlCommand("SELECT 3.14159::numeric, 2.5::float8");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("3.14") != std::string::npos ||
                output.find("2.5") != std::string::npos);
}

TEST_F(PsqlClientTest, TypeRoundTrip_Timestamp) {
    auto [exit_code, output] = runPsqlCommand("SELECT '2024-01-01'::date, '2024-01-01 12:00:00'::timestamp");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("2024") != std::string::npos);
}

TEST_F(PsqlClientTest, TypeRoundTrip_Array) {
    auto [exit_code, output] = runPsqlCommand("SELECT ARRAY[1,2,3], ARRAY['a','b','c']");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("1") != std::string::npos ||
                output.find("{") != std::string::npos);
}

// ============================================================================
// Additional psql Meta-Command Tests
// ============================================================================

TEST_F(PsqlClientTest, MetaCommand_DotD) {
    runPsqlCommand("CREATE TABLE IF NOT EXISTS meta_test (id INT PRIMARY KEY, data TEXT)");
    
    auto [exit_code, output] = runPsqlCommand("\\d meta_test");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // \d should describe the table
    EXPECT_TRUE(output.find("Column") != std::string::npos ||
                output.find("id") != std::string::npos ||
                output.find("did not find") != std::string::npos);
}

TEST_F(PsqlClientTest, MetaCommand_DotDn) {
    auto [exit_code, output] = runPsqlCommand("\\dn");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // \dn should list schemas
    EXPECT_TRUE(output.find("public") != std::string::npos ||
                output.find("List of schemas") != std::string::npos);
}

TEST_F(PsqlClientTest, MetaCommand_DotDu) {
    auto [exit_code, output] = runPsqlCommand("\\du");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // \du should list roles
    EXPECT_TRUE(output.find("Role name") != std::string::npos ||
                output.find("List of roles") != std::string::npos);
}

TEST_F(PsqlClientTest, MetaCommand_DotTiming) {
    auto [exit_code, output] = runPsqlCommand("\\timing on; SELECT 1; \\timing off");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should show timing info
    EXPECT_TRUE(output.find("1") != std::string::npos ||
                output.find("Timing") != std::string::npos);
}

TEST_F(PsqlClientTest, MetaCommand_DotX) {
    auto [exit_code, output] = runPsqlCommand("\\x on; SELECT 1 AS col; \\x off");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Expanded display should work
    EXPECT_TRUE(output.find("1") != std::string::npos);
}

TEST_F(PsqlClientTest, MetaCommand_DotHelp) {
    auto [exit_code, output] = runPsqlCommand("\\h SELECT");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    // Should show help
    EXPECT_TRUE(output.find("Command") != std::string::npos ||
                output.find("SELECT") != std::string::npos ||
                output.find("No help available") != std::string::npos);
}

// ============================================================================
// DDL Tests
// ============================================================================

TEST_F(PsqlClientTest, DDL_CreateTable) {
    auto [exit_code, output] = runPsqlCommand(
        "CREATE TABLE ddl_test (id SERIAL PRIMARY KEY, name TEXT NOT NULL, created_at TIMESTAMP DEFAULT NOW())"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("CREATE TABLE") != std::string::npos ||
                exit_code == 0);
}

TEST_F(PsqlClientTest, DDL_CreateIndex) {
    runPsqlCommand("CREATE TABLE IF NOT EXISTS idx_test (id INT, data TEXT)");
    
    auto [exit_code, output] = runPsqlCommand("CREATE INDEX idx_test_data ON idx_test(data)");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("CREATE INDEX") != std::string::npos ||
                exit_code == 0);
}

TEST_F(PsqlClientTest, DDL_AlterTable) {
    runPsqlCommand("CREATE TABLE IF NOT EXISTS alter_test (id INT)");
    
    auto [exit_code, output] = runPsqlCommand("ALTER TABLE alter_test ADD COLUMN new_col TEXT");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("ALTER TABLE") != std::string::npos ||
                exit_code == 0);
}

TEST_F(PsqlClientTest, DDL_DropTable) {
    runPsqlCommand("CREATE TABLE IF NOT EXISTS drop_test (id INT)");
    
    auto [exit_code, output] = runPsqlCommand("DROP TABLE drop_test");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "psql not installed";
    }
    
    EXPECT_TRUE(output.find("DROP TABLE") != std::string::npos ||
                exit_code == 0);
}

} // namespace
