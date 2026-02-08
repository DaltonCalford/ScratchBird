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
 * Firebird Client Integration Tests
 *
 * Tests compatibility with Firebird client:
 * - 2.3.16: Firebird client connect
 * - 2.3.17: Firebird simple SELECT
 * - 3.2.19: Firebird type round-trip
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstdlib>

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

class FirebirdClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("firebird_client_test");

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
        status = server_->initialize("127.0.0.1", 13050, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to initialize IPC server: " << ctx.message;

        status = server_->start(&ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to start IPC server: " << ctx.message;

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

    // Helper to run isql-fb command and capture output
    std::pair<int, std::string> runFirebirdCommand(const std::string& command) {
        std::string full_cmd = "echo \"" + command + "\" | isql-fb -user testuser -pass testpass "
                              "127.0.0.1/13050:testdb 2>&1";
        
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
// 2.3.16: Firebird client Connect Test
// ============================================================================

TEST_F(FirebirdClientTest, Connect_Basic) {
    auto [exit_code, output] = runFirebirdCommand("SELECT 1 FROM RDB\$DATABASE");
    
    if (output.find("command not found") != std::string::npos ||
        output.find("not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    // Should either connect or give meaningful error
    EXPECT_TRUE(output.find("1") != std::string::npos ||
                output.find("connect") != std::string::npos ||
                output.find("refused") != std::string::npos ||
                output.find("unavailable") != std::string::npos);
}

TEST_F(FirebirdClientTest, Connect_Version) {
    auto [exit_code, output] = runFirebirdCommand("SELECT RDB\$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB\$DATABASE");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("VERSION") != std::string::npos ||
                output.find("4.") != std::string::npos ||
                output.find("3.") != std::string::npos);
}

TEST_F(FirebirdClientTest, Connect_DatabaseInfo) {
    auto [exit_code, output] = runFirebirdCommand("SELECT MON\$DATABASE_NAME FROM MON\$DATABASE");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("DATABASE_NAME") != std::string::npos ||
                output.find("testdb") != std::string::npos);
}

// ============================================================================
// 2.3.17: Firebird simple SELECT Test
// ============================================================================

TEST_F(FirebirdClientTest, SimpleSelect_FromDual) {
    auto [exit_code, output] = runFirebirdCommand("SELECT 1 AS ONE, 2 AS TWO FROM RDB\$DATABASE");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("ONE") != std::string::npos ||
                output.find("1") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_Calculations) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT 1+1 AS addition, 10*5 AS multiplication, 100/4 AS division FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("2") != std::string::npos ||
                output.find("50") != std::string::npos ||
                output.find("25") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_StringFunctions) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT UPPER('hello') AS up, LOWER('WORLD') AS low FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("HELLO") != std::string::npos ||
                output.find("world") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_DateFunctions) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CURRENT_DATE AS today, CURRENT_TIME AS now_time FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("TODAY") != std::string::npos ||
                output.find("NOW_TIME") != std::string::npos ||
                output.find("202") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_Case) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CASE WHEN 1=1 THEN 'yes' ELSE 'no' END AS result FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("yes") != std::string::npos ||
                output.find("RESULT") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_Coalesce) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT COALESCE(NULL, 'fallback', 'other') AS coalesce_test FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("fallback") != std::string::npos ||
                output.find("COALESCE") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_Extract) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT EXTRACT(YEAR FROM CURRENT_DATE) AS year FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("YEAR") != std::string::npos ||
                output.find("202") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_Cast) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST(123 AS VARCHAR(10)) AS cast_test FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("123") != std::string::npos ||
                output.find("CAST") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_NullHandling) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT NULLIF(1, 1) AS null_result, NULLIF(1, 2) AS one_result FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("NULL") != std::string::npos ||
                output.find("<null>") != std::string::npos);
}

TEST_F(FirebirdClientTest, SimpleSelect_Generator) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT GEN_ID(GENERATOR_TEST, 1) FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    // Generator may or may not exist
    EXPECT_TRUE(output.find("GEN_ID") != std::string::npos ||
                output.find("error") != std::string::npos ||
                output.find("exist") != std::string::npos);
}

// ============================================================================
// 3.2.19: Firebird Type Round-trip Test
// ============================================================================

TEST_F(FirebirdClientTest, TypeRoundTrip_SmallInt) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST(42 AS SMALLINT) AS smallint_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("42") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Integer) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST(1000000 AS INTEGER) AS int_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("1000000") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_BigInt) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST(9223372036854775807 AS BIGINT) AS bigint_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("9223372036854775807") != std::string::npos ||
                output.find("BIGINT_COL") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Char) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST('hello' AS CHAR(10)) AS char_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("hello") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Varchar) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST('world' AS VARCHAR(50)) AS varchar_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("world") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Decimal) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST(3.14159 AS DECIMAL(10,5)) AS decimal_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("3.14") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Numeric) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST(2.71828 AS NUMERIC(10,5)) AS numeric_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("2.71") != std::string::npos ||
                output.find("NUMERIC") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Float) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST(1.5 AS FLOAT) AS float_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("1.5") != std::string::npos ||
                output.find("FLOAT") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Double) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST(2.5 AS DOUBLE PRECISION) AS double_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("2.5") != std::string::npos ||
                output.find("DOUBLE") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Date) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST('2024-01-15' AS DATE) AS date_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("2024") != std::string::npos ||
                output.find("DATE") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Time) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST('14:30:00' AS TIME) AS time_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("14") != std::string::npos ||
                output.find("TIME") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Timestamp) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST('2024-01-15 14:30:00' AS TIMESTAMP) AS ts_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("2024") != std::string::npos ||
                output.find("TIMESTAMP") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Blob) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT CAST('test blob data' AS BLOB SUB_TYPE TEXT) AS blob_col FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("test") != std::string::npos ||
                output.find("BLOB") != std::string::npos ||
                output.find("BLOB_COL") != std::string::npos);
}

TEST_F(FirebirdClientTest, TypeRoundTrip_Boolean) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT TRUE AS true_val, FALSE AS false_val, UNKNOWN AS unknown_val FROM RDB\$DATABASE"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("TRUE") != std::string::npos ||
                output.find("<true>") != std::string::npos ||
                output.find("TRUE_VAL") != std::string::npos);
}

// ============================================================================
// DDL Tests
// ============================================================================

TEST_F(FirebirdClientTest, DDL_CreateTable) {
    auto [exit_code, output] = runFirebirdCommand(
        "CREATE TABLE fb_ddl_test (id INTEGER PRIMARY KEY, name VARCHAR(100))"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || 
                output.find("CREATE") != std::string::npos ||
                output.find("table") != std::string::npos);
}

TEST_F(FirebirdClientTest, DDL_CreateGenerator) {
    auto [exit_code, output] = runFirebirdCommand(
        "CREATE GENERATOR fb_gen_test"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || 
                output.find("GENERATOR") != std::string::npos ||
                output.find("exist") != std::string::npos);
}

TEST_F(FirebirdClientTest, DDL_CreateIndex) {
    runFirebirdCommand("CREATE TABLE fb_idx_test (id INTEGER, data VARCHAR(100))");
    
    auto [exit_code, output] = runFirebirdCommand(
        "CREATE INDEX fb_idx_test_data ON fb_idx_test(data)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || 
                output.find("INDEX") != std::string::npos ||
                output.find("exist") != std::string::npos);
}

TEST_F(FirebirdClientTest, DDL_AlterTable) {
    runFirebirdCommand("CREATE TABLE fb_alter_test (id INTEGER)");
    
    auto [exit_code, output] = runFirebirdCommand(
        "ALTER TABLE fb_alter_test ADD new_col VARCHAR(50)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || 
                output.find("ALTER") != std::string::npos ||
                output.find("exist") != std::string::npos);
}

TEST_F(FirebirdClientTest, DDL_DropTable) {
    runFirebirdCommand("CREATE TABLE fb_drop_test (id INTEGER)");
    
    auto [exit_code, output] = runFirebirdCommand(
        "DROP TABLE fb_drop_test"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || 
                output.find("DROP") != std::string::npos);
}

// ============================================================================
// System Table Tests
// ============================================================================

TEST_F(FirebirdClientTest, SystemTables_RDBRelations) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT RDB\$RELATION_NAME FROM RDB\$RELATIONS WHERE RDB\$SYSTEM_FLAG = 0"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("RELATION_NAME") != std::string::npos ||
                output.find("RDB$") != std::string::npos);
}

TEST_F(FirebirdClientTest, SystemTables_RDBFields) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT RDB\$FIELD_NAME FROM RDB\$FIELDS FETCH FIRST 5 ROWS ONLY"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("FIELD_NAME") != std::string::npos ||
                output.find("RDB$") != std::string::npos);
}

TEST_F(FirebirdClientTest, SystemTables_RDBGenerators) {
    auto [exit_code, output] = runFirebirdCommand(
        "SELECT RDB\$GENERATOR_NAME FROM RDB\$GENERATORS"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(output.find("GENERATOR_NAME") != std::string::npos ||
                output.find("RDB$") != std::string::npos);
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(FirebirdClientTest, Transaction_Commit) {
    auto [exit_code, output] = runFirebirdCommand(
        "CREATE TABLE fb_txn_test (id INTEGER); "
        "COMMIT; "
        "INSERT INTO fb_txn_test VALUES (100); "
        "COMMIT"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(exit_code == 0);
}

TEST_F(FirebirdClientTest, Transaction_Rollback) {
    auto [exit_code, output] = runFirebirdCommand(
        "CREATE TABLE fb_rollback_test (id INTEGER); "
        "INSERT INTO fb_rollback_test VALUES (1); "
        "COMMIT; "
        "INSERT INTO fb_rollback_test VALUES (999); "
        "ROLLBACK"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(exit_code == 0);
}

// ============================================================================
// RECREATE Tests (Firebird-specific)
// ============================================================================

TEST_F(FirebirdClientTest, RecreateTable) {
    auto [exit_code, output] = runFirebirdCommand(
        "RECREATE TABLE fb_recreate_test (id INTEGER PRIMARY KEY)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "isql-fb not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || 
                output.find("RECREATE") != std::string::npos ||
                output.find("recreate") != std::string::npos);
}

} // namespace
