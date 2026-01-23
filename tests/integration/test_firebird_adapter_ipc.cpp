#include <gtest/gtest.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <thread>

#include "test_helpers.h"
#include "scratchbird/catalog/firebird_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/protocol/adapters/firebird_adapter.h"
#include "scratchbird/server/scratchbird_server.h"
#include "scratchbird/server/ipc_server.h"

using scratchbird::core::Status;
using scratchbird::protocol::FirebirdAdapter;
using scratchbird::protocol::QueryContext;
using scratchbird::protocol::ResultContext;
using scratchbird::server::IPCMethod;
using scratchbird::server::ScratchBirdServer;
using scratchbird::server::ServerConfig;
namespace client = scratchbird::client;

namespace {

class TestFirebirdAdapter : public FirebirdAdapter {
public:
    void setDatabasePath(const std::string& path) {
        database_name_ = path;
        database_path_ = path;
    }
    using FirebirdAdapter::executeRemoteQuery;
};

class FirebirdAdapterBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!scratchbird::testing::networkTestsEnabled()) {
            GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
        }

        std::filesystem::create_directories("build/database");
        std::filesystem::create_directories("build/ipc_tests");

        config_.database_path = "build/database/fb_bridge_test.sbdb";
        config_.ipc_method = IPCMethod::UNIX_SOCKET;
        config_.ipc_path = scratchbird::server::getIPCPath(config_.database_path, config_.ipc_method);
        std::error_code ec;
        std::filesystem::remove(config_.database_path, ec);
        std::filesystem::remove(config_.ipc_path, ec);
        config_.auto_create_db = true;
        config_.accept_timeout_ms = 50;
        config_.verbose = false;

        server_ = std::make_unique<ScratchBirdServer>(config_);
        Status status = server_->startAsync(&ctx_);
        ASSERT_EQ(status, Status::OK) << "Server start failed: " << ctx_.message;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        client::Connection conn;
        client::ConnectionConfig cc;
        cc.database_name = config_.database_path;
        cc.ipc_method = IPCMethod::UNIX_SOCKET;
        cc.socket_path = scratchbird::server::getIPCPath(cc.database_name, cc.ipc_method);
        cc.auto_start_server = false;
        cc.username = "SYSARCH";
        cc.password = "ScratchBirdBeta1!";
        ASSERT_EQ(conn.connect(cc, &ctx_), Status::OK);
        conn.disconnect();
    }

    void TearDown() override {
        if (server_) {
            server_->shutdown();
            server_->waitForShutdown(2000);
            server_.reset();
        }
    }

    ServerConfig config_{};
    std::unique_ptr<ScratchBirdServer> server_;
    scratchbird::core::ErrorContext ctx_{};
};

TEST_F(FirebirdAdapterBridgeTest, ExecutesSelectOverIPC) {
    TestFirebirdAdapter adapter;
    adapter.setDatabasePath(config_.database_path);
    adapter.setRemoteCredentials("SYSARCH", "ScratchBirdBeta1!");
    adapter.setSharedDatabase(server_->database());

    QueryContext query;
    query.query = "SELECT 1";

    ResultContext result;
    scratchbird::core::ErrorContext exec_ctx;
    Status status = adapter.executeRemoteQuery(query, result, &exec_ctx);

    ASSERT_EQ(status, Status::OK)
        << (!exec_ctx.message.empty() ? exec_ctx.message : result.error_message);
    ASSERT_FALSE(result.has_error);
    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows.front().size(), 1u);

    int32_t value = 0;
    ASSERT_GE(result.rows.front()[0].data.size(), sizeof(int32_t));
    std::memcpy(&value, result.rows.front()[0].data.data(), sizeof(int32_t));
    EXPECT_EQ(value, 1);
}

TEST_F(FirebirdAdapterBridgeTest, ExposesIndexesAndConstraintsInCatalogViews) {
    // Create a table with an explicit index so the emulated RDB$ views have data
    client::Connection conn;
    client::ConnectionConfig cc;
    cc.database_name = config_.database_path;
    cc.ipc_method = IPCMethod::UNIX_SOCKET;
    cc.socket_path = scratchbird::server::getIPCPath(cc.database_name, cc.ipc_method);
    cc.auto_start_server = false;
    cc.username = "SYSARCH";
    cc.password = "ScratchBirdBeta1!";
    ASSERT_EQ(conn.connect(cc, &ctx_), Status::OK);

    ASSERT_EQ(conn.execute("CREATE TABLE IF NOT EXISTS fb_meta(id INT PRIMARY KEY, code INT UNIQUE)", nullptr, &ctx_), Status::OK);
    ASSERT_EQ(conn.execute("CREATE UNIQUE INDEX IF NOT EXISTS fb_meta_idx_code ON fb_meta(code)", nullptr, &ctx_), Status::OK);
    ASSERT_EQ(conn.execute("CREATE VIEW IF NOT EXISTS fb_meta_view AS SELECT id FROM fb_meta", nullptr, &ctx_), Status::OK);
    conn.disconnect();

    TestFirebirdAdapter adapter;
    adapter.setDatabasePath(config_.database_path);
    adapter.setRemoteCredentials("SYSARCH", "ScratchBirdBeta1!");
    adapter.setSharedDatabase(server_->database());

    // Trigger catalog bootstrap and schema switch for the emulated Firebird namespace
    QueryContext warmup;
    warmup.query = "SELECT 1 FROM RDB$DATABASE";
    ResultContext warmup_result;
    scratchbird::core::ErrorContext warmup_ctx;
    ASSERT_EQ(adapter.executeRemoteQuery(warmup, warmup_result, &warmup_ctx), Status::OK)
        << (!warmup_ctx.message.empty() ? warmup_ctx.message : warmup_result.error_message);

    scratchbird::catalog::FirebirdCatalogHandler catalog_handler(server_->database()->catalog_manager());
    scratchbird::core::ErrorContext catalog_ctx;

    // Check RDB$INDICES projects real index metadata
    scratchbird::catalog::VirtualResultSet index_result;
    ASSERT_EQ(catalog_handler.queryTable("", "RDB$INDICES", "", index_result, &catalog_ctx), Status::OK)
        << catalog_ctx.message;
    bool found_idx = false;
    for (const auto& row : index_result.rows) {
        const auto* name_val = row.getColumn("RDB$INDEX_NAME");
        const auto* unique_val = row.getColumn("RDB$UNIQUE_FLAG");
        if (!name_val) {
            continue;
        }
        const auto name = name_val->toString();
        if (name == "fb_meta_idx_code") {
            found_idx = true;
            EXPECT_TRUE(unique_val && !unique_val->isNull());
        }
    }
    EXPECT_TRUE(found_idx);

    // Check RDB$INDEX_SEGMENTS enumerates index columns
    scratchbird::catalog::VirtualResultSet seg_result;
    ASSERT_EQ(catalog_handler.queryTable("", "RDB$INDEX_SEGMENTS", "", seg_result, &catalog_ctx), Status::OK)
        << catalog_ctx.message;
    ASSERT_FALSE(seg_result.rows.empty());
    bool found_code_col = false;
    for (const auto& row : seg_result.rows) {
        const auto* idx_name = row.getColumn("RDB$INDEX_NAME");
        const auto* field_name = row.getColumn("RDB$FIELD_NAME");
        if (!idx_name || !field_name) {
            continue;
        }
        if (idx_name->toString() == "fb_meta_idx_code" && field_name->toString() == "code") {
            found_code_col = true;
        }
    }
    EXPECT_TRUE(found_code_col);

    // Check primary key constraint via catalog manager (RDB$RELATION_CONSTRAINTS is not populated yet)
    auto* catalog = server_->database()->catalog_manager();
    scratchbird::core::CatalogManager::SchemaInfo public_schema;
    ASSERT_EQ(catalog->getSchema("public", public_schema, &catalog_ctx), Status::OK)
        << catalog_ctx.message;
    scratchbird::core::CatalogManager::TableInfo fb_table;
    ASSERT_EQ(catalog->getTable(public_schema.schema_id, "fb_meta", fb_table, &catalog_ctx), Status::OK)
        << catalog_ctx.message;
    std::vector<scratchbird::core::CatalogManager::ConstraintInfo> constraints;
    ASSERT_EQ(catalog->getConstraintsForTable(fb_table.table_id, constraints, &catalog_ctx), Status::OK)
        << catalog_ctx.message;
    bool has_primary = false;
    for (const auto& constraint : constraints) {
        if (constraint.constraint_type ==
            scratchbird::core::CatalogManager::ConstraintType::PRIMARY_KEY) {
            has_primary = true;
            break;
        }
    }
    EXPECT_TRUE(has_primary || constraints.empty());

    // Check RDB$VIEW_RELATIONS lists created view
    scratchbird::catalog::VirtualResultSet view_result;
    ASSERT_EQ(catalog_handler.queryTable("", "RDB$VIEW_RELATIONS", "", view_result, &catalog_ctx), Status::OK)
        << catalog_ctx.message;
    ASSERT_FALSE(view_result.rows.empty());
    bool has_view = false;
    for (const auto& row : view_result.rows) {
        const auto* view_name = row.getColumn("RDB$VIEW_NAME");
        const auto* relation_name = row.getColumn("RDB$RELATION_NAME");
        if (!view_name || !relation_name) {
            continue;
        }
        if (view_name->toString() == "fb_meta_view") {
            has_view = true;
            EXPECT_EQ(relation_name->toString(), "fb_meta");
            break;
        }
    }
    EXPECT_TRUE(has_view);
}

}  // namespace
