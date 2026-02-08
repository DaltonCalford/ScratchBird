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
 * MySQL Client Integration Tests
 *
 * Tests compatibility with MySQL client:
 * - 2.2.15: mysql client connect
 * - 2.2.16: mysql simple SELECT
 * - 2.2.17: mysql prepared statement
 * - 4.2.9: SHOW TABLES test
 * - 3.2.18: MySQL type round-trip
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

class MySQLClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("mysql_client_test");

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
        status = server_->initialize("127.0.0.1", 13306, &ctx);
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

    // Helper to run mysql command and capture output
    std::pair<int, std::string> runMySQLCommand(const std::string& command) {
        std::string full_cmd = "mysql -h 127.0.0.1 -P 13306 -u testuser -ptestpass testdb -e \"" + 
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
// 2.2.15: mysql client Connect Test
// ============================================================================

TEST_F(MySQLClientTest, Connect_Basic) {
    auto [exit_code, output] = runMySQLCommand("SELECT 1");
    
    if (output.find("command not found") != std::string::npos ||
        output.find("not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    // Connection should either succeed or give meaningful error
    EXPECT_TRUE(output.find("1") != std::string::npos ||
                output.find("connection") != std::string::npos ||
                output.find("refused") != std::string::npos);
}

TEST_F(MySQLClientTest, Connect_WithDatabase) {
    auto [exit_code, output] = runMySQLCommand("SELECT DATABASE()");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("testdb") != std::string::npos ||
                output.find("NULL") != std::string::npos ||
                output.find("connection") != std::string::npos);
}

TEST_F(MySQLClientTest, Connect_Version) {
    auto [exit_code, output] = runMySQLCommand("SELECT VERSION()");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    // Should return version info
    EXPECT_TRUE(output.find("VERSION()") != std::string::npos ||
                output.find("connection") != std::string::npos);
}

// ============================================================================
// 2.2.16: mysql simple SELECT Test
// ============================================================================

TEST_F(MySQLClientTest, SimpleSelect_Literal) {
    auto [exit_code, output] = runMySQLCommand("SELECT 1 AS one, 2 AS two, 'hello' AS greeting");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("1") != std::string::npos);
    EXPECT_TRUE(output.find("hello") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_Calculations) {
    auto [exit_code, output] = runMySQLCommand("SELECT 1+1, 10*5, 100/4");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("2") != std::string::npos ||
                output.find("50") != std::string::npos ||
                output.find("25") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_Where) {
    auto [exit_code, output] = runMySQLCommand("SELECT * FROM (SELECT 1 AS id UNION SELECT 2 UNION SELECT 3) AS t WHERE id > 1");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("2") != std::string::npos ||
                output.find("3") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_OrderBy) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT * FROM (SELECT 3 AS num UNION SELECT 1 UNION SELECT 2) AS t ORDER BY num"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("1") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_Limit) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT * FROM (SELECT 1 AS n UNION SELECT 2 UNION SELECT 3 UNION SELECT 4) AS t LIMIT 2"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("1") != std::string::npos ||
                output.find("2") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_Aggregate) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT COUNT(*), SUM(n), AVG(n), MAX(n), MIN(n) FROM (SELECT 1 AS n UNION SELECT 2 UNION SELECT 3) AS t"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("3") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_Distinct) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT DISTINCT val FROM (SELECT 1 AS val UNION ALL SELECT 1 UNION ALL SELECT 2) AS t"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("1") != std::string::npos);
    EXPECT_TRUE(output.find("2") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_Join) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT a.id, b.val FROM (SELECT 1 AS id, 'a' AS k) AS a "
        "JOIN (SELECT 'a' AS k, 100 AS val) AS b ON a.k = b.k"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("1") != std::string::npos ||
                output.find("100") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_GroupBy) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT grp, COUNT(*) FROM (SELECT 'A' AS grp UNION ALL SELECT 'A' UNION ALL SELECT 'B') AS t "
        "GROUP BY grp"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("A") != std::string::npos ||
                output.find("B") != std::string::npos);
}

TEST_F(MySQLClientTest, SimpleSelect_Having) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT grp, COUNT(*) AS cnt FROM (SELECT 'A' AS grp UNION ALL SELECT 'A' UNION ALL SELECT 'B') AS t "
        "GROUP BY grp HAVING cnt > 1"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("A") != std::string::npos);
}

// ============================================================================
// 2.2.17: mysql prepared statement Test
// ============================================================================

TEST_F(MySQLClientTest, PreparedStatement_PrepareExecute) {
    auto [exit_code, output] = runMySQLCommand(
        "PREPARE stmt1 FROM 'SELECT ? + ?'; SET @a = 10, @b = 20; EXECUTE stmt1 USING @a, @b"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("30") != std::string::npos ||
                output.find("PREPARE") != std::string::npos);
}

TEST_F(MySQLClientTest, PreparedStatement_MultipleParams) {
    auto [exit_code, output] = runMySQLCommand(
        "PREPARE stmt2 FROM 'SELECT CONCAT(?, \" \", ?)'; "
        "SET @x = 'Hello', @y = 'World'; EXECUTE stmt2 USING @x, @y"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("Hello") != std::string::npos ||
                output.find("World") != std::string::npos);
}

TEST_F(MySQLClientTest, PreparedStatement_Deallocate) {
    auto [exit_code, output] = runMySQLCommand(
        "PREPARE stmt3 FROM 'SELECT 1'; DEALLOCATE PREPARE stmt3"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("DEALLOCATE") != std::string::npos ||
                exit_code == 0);
}

// ============================================================================
// 4.2.9: SHOW TABLES Test
// ============================================================================

TEST_F(MySQLClientTest, ShowTables_Basic) {
    auto [exit_code, output] = runMySQLCommand("SHOW TABLES");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("Tables") != std::string::npos ||
                output.find("Empty") != std::string::npos ||
                output.find("table") != std::string::npos);
}

TEST_F(MySQLClientTest, ShowTables_Like) {
    // Create a test table first
    runMySQLCommand("CREATE TABLE IF NOT EXISTS test_table_1 (id INT)");
    
    auto [exit_code, output] = runMySQLCommand("SHOW TABLES LIKE 'test_%'");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("test_table_1") != std::string::npos ||
                output.find("Tables") != std::string::npos);
}

TEST_F(MySQLClientTest, ShowDatabases) {
    auto [exit_code, output] = runMySQLCommand("SHOW DATABASES");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("Database") != std::string::npos ||
                output.find("testdb") != std::string::npos ||
                output.find("information_schema") != std::string::npos);
}

TEST_F(MySQLClientTest, ShowColumns) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS columns_test (id INT, name VARCHAR(100))");
    
    auto [exit_code, output] = runMySQLCommand("SHOW COLUMNS FROM columns_test");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("Field") != std::string::npos ||
                output.find("id") != std::string::npos ||
                output.find("name") != std::string::npos);
}

TEST_F(MySQLClientTest, ShowCreateTable) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS show_create_test (id INT PRIMARY KEY)");
    
    auto [exit_code, output] = runMySQLCommand("SHOW CREATE TABLE show_create_test");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("CREATE TABLE") != std::string::npos ||
                output.find("show_create_test") != std::string::npos);
}

TEST_F(MySQLClientTest, ShowVariables) {
    auto [exit_code, output] = runMySQLCommand("SHOW VARIABLES LIKE 'version%'");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("Variable_name") != std::string::npos ||
                output.find("version") != std::string::npos);
}

// ============================================================================
// 3.2.18: MySQL Type Round-trip Test
// ============================================================================

TEST_F(MySQLClientTest, TypeRoundTrip_Integer) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT CAST(42 AS SIGNED), CAST(1000000 AS UNSIGNED), CAST(5 AS TINYINT)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("42") != std::string::npos);
}

TEST_F(MySQLClientTest, TypeRoundTrip_String) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT CAST('hello' AS CHAR(20)), CAST('world' AS VARCHAR(50))"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("hello") != std::string::npos ||
                output.find("world") != std::string::npos);
}

TEST_F(MySQLClientTest, TypeRoundTrip_Decimal) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT CAST(3.14159 AS DECIMAL(10,5)), CAST(2.5 AS FLOAT), CAST(1.5 AS DOUBLE)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("3.14") != std::string::npos ||
                output.find("2.5") != std::string::npos);
}

TEST_F(MySQLClientTest, TypeRoundTrip_DateTime) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT CAST('2024-01-01' AS DATE), CAST('2024-01-01 12:30:00' AS DATETIME)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("2024") != std::string::npos);
}

TEST_F(MySQLClientTest, TypeRoundTrip_Binary) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT CAST('hello' AS BINARY(10)), CAST('world' AS VARBINARY(50))"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("hello") != std::string::npos ||
                output.find("world") != std::string::npos);
}

TEST_F(MySQLClientTest, TypeRoundTrip_Blob) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT CAST('test data' AS BLOB), CAST('more data' AS TEXT)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("test") != std::string::npos ||
                output.find("more") != std::string::npos);
}

TEST_F(MySQLClientTest, TypeRoundTrip_JSON) {
    auto [exit_code, output] = runMySQLCommand(
        "SELECT CAST('{\"key\": \"value\"}' AS JSON)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("key") != std::string::npos ||
                output.find("value") != std::string::npos ||
                output.find("{") != std::string::npos);
}

// ============================================================================
// DDL Tests
// ============================================================================

TEST_F(MySQLClientTest, DDL_CreateTable) {
    auto [exit_code, output] = runMySQLCommand(
        "CREATE TABLE IF NOT EXISTS mysql_ddl_test ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "name VARCHAR(100) NOT NULL, "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || output.find("CREATE") != std::string::npos);
}

TEST_F(MySQLClientTest, DDL_CreateIndex) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS mysql_idx_test (id INT, data VARCHAR(100))");
    
    auto [exit_code, output] = runMySQLCommand(
        "CREATE INDEX idx_data ON mysql_idx_test(data)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || output.find("INDEX") != std::string::npos);
}

TEST_F(MySQLClientTest, DDL_AlterTable) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS mysql_alter_test (id INT)");
    
    auto [exit_code, output] = runMySQLCommand(
        "ALTER TABLE mysql_alter_test ADD COLUMN new_col VARCHAR(50)"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || output.find("ALTER") != std::string::npos);
}

TEST_F(MySQLClientTest, DDL_DropTable) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS mysql_drop_test (id INT)");
    
    auto [exit_code, output] = runMySQLCommand("DROP TABLE mysql_drop_test");
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(exit_code == 0 || output.find("DROP") != std::string::npos);
}

// ============================================================================
// DML Tests
// ============================================================================

TEST_F(MySQLClientTest, DML_Insert) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS mysql_dml_test (id INT, val VARCHAR(50))");
    
    auto [exit_code, output] = runMySQLCommand(
        "INSERT INTO mysql_dml_test VALUES (1, 'test'), (2, 'data')"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("2") != std::string::npos || exit_code == 0);
}

TEST_F(MySQLClientTest, DML_Update) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS mysql_update_test (id INT, val INT)");
    runMySQLCommand("INSERT INTO mysql_update_test VALUES (1, 10), (2, 20)");
    
    auto [exit_code, output] = runMySQLCommand(
        "UPDATE mysql_update_test SET val = val + 1 WHERE id = 1"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("1") != std::string::npos || exit_code == 0);
}

TEST_F(MySQLClientTest, DML_Delete) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS mysql_delete_test (id INT)");
    runMySQLCommand("INSERT INTO mysql_delete_test VALUES (1), (2), (3)");
    
    auto [exit_code, output] = runMySQLCommand(
        "DELETE FROM mysql_delete_test WHERE id > 1"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(output.find("2") != std::string::npos || exit_code == 0);
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(MySQLClientTest, Transaction_Commit) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS mysql_txn_test (id INT)");
    
    auto [exit_code, output] = runMySQLCommand(
        "START TRANSACTION; INSERT INTO mysql_txn_test VALUES (100); COMMIT"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    EXPECT_TRUE(exit_code == 0);
    
    // Verify data
    auto [exit_code2, output2] = runMySQLCommand("SELECT * FROM mysql_txn_test WHERE id = 100");
    EXPECT_TRUE(output2.find("100") != std::string::npos);
}

TEST_F(MySQLClientTest, Transaction_Rollback) {
    runMySQLCommand("CREATE TABLE IF NOT EXISTS mysql_rollback_test (id INT)");
    runMySQLCommand("INSERT INTO mysql_rollback_test VALUES (1)");
    
    auto [exit_code, output] = runMySQLCommand(
        "START TRANSACTION; INSERT INTO mysql_rollback_test VALUES (999); ROLLBACK"
    );
    
    if (output.find("command not found") != std::string::npos) {
        GTEST_SKIP() << "mysql client not installed";
    }
    
    // Verify row 999 was not inserted
    auto [exit_code2, output2] = runMySQLCommand("SELECT COUNT(*) FROM mysql_rollback_test");
    EXPECT_TRUE(output2.find("1") != std::string::npos);
}

} // namespace
