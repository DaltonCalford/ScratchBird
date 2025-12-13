#include <gtest/gtest.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <thread>

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

namespace {

class TestFirebirdAdapter : public FirebirdAdapter {
public:
    void setDatabasePath(const std::string& path) { database_name_ = path; }
    using FirebirdAdapter::executeRemoteQuery;
};

class FirebirdAdapterBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories("build/database");
        std::filesystem::create_directories("build/ipc_tests");

        config_.database_path = "build/database/fb_bridge_test.sbdb";
        config_.ipc_method = IPCMethod::UNIX_SOCKET;
        config_.ipc_path = scratchbird::server::getIPCPath(config_.database_path, config_.ipc_method);
        config_.auto_create_db = true;
        config_.accept_timeout_ms = 50;
        config_.verbose = false;

        server_ = std::make_unique<ScratchBirdServer>(config_);
        Status status = server_->startAsync(&ctx_);
        ASSERT_EQ(status, Status::OK);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Bootstrap minimal RDB$DATABASE table for singleton selects
        client::Connection conn;
        client::ConnectionConfig cc;
        cc.database_name = config_.database_path;
        cc.ipc_method = IPCMethod::UNIX_SOCKET;
        cc.socket_path = scratchbird::server::getIPCPath(cc.database_name, cc.ipc_method);
        cc.auto_start_server = false;
        ASSERT_EQ(conn.connect(cc, &ctx_), Status::OK);
        conn.execute("CREATE TABLE IF NOT EXISTS RDB$DATABASE(DUMMY INT)", nullptr, &ctx_);
        conn.execute("INSERT INTO RDB$DATABASE(DUMMY) VALUES (1)", nullptr, &ctx_);
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

    QueryContext query;
    query.query = "SELECT 1 FROM RDB$DATABASE";

    ResultContext result;
    Status status = adapter.executeRemoteQuery(query, result);

    ASSERT_EQ(status, Status::OK);
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
    ASSERT_EQ(conn.connect(cc, &ctx_), Status::OK);

    ASSERT_EQ(conn.execute("CREATE TABLE IF NOT EXISTS fb_meta(id INT PRIMARY KEY, code INT UNIQUE)", nullptr, &ctx_), Status::OK);
    ASSERT_EQ(conn.execute("CREATE UNIQUE INDEX IF NOT EXISTS fb_meta_idx_code ON fb_meta(code)", nullptr, &ctx_), Status::OK);
    ASSERT_EQ(conn.execute("CREATE VIEW IF NOT EXISTS fb_meta_view AS SELECT id FROM fb_meta", nullptr, &ctx_), Status::OK);
    conn.disconnect();

    TestFirebirdAdapter adapter;
    adapter.setDatabasePath(config_.database_path);

    // Trigger catalog bootstrap and schema switch for the emulated Firebird namespace
    QueryContext warmup;
    warmup.query = "SELECT 1 FROM RDB$DATABASE";
    ResultContext warmup_result;
    ASSERT_EQ(adapter.executeRemoteQuery(warmup, warmup_result), Status::OK);

    auto toString = [](const scratchbird::protocol::ProtocolCodec::ColumnValue& v) {
        return std::string(v.data.begin(), v.data.end());
    };

    // Check RDB$INDICES projects real index metadata
    QueryContext index_query;
    index_query.query = "SELECT RDB$INDEX_NAME, RDB$UNIQUE_FLAG FROM RDB$INDICES WHERE RDB$RELATION_NAME = 'fb_meta'";
    ResultContext index_result;
    ASSERT_EQ(adapter.executeRemoteQuery(index_query, index_result), Status::OK);
    ASSERT_FALSE(index_result.has_error);
    bool found_idx = false;
    for (const auto& row : index_result.rows) {
        const auto name = toString(row[0]);
        if (name == "fb_meta_idx_code") {
            found_idx = true;
            EXPECT_EQ(row[1].data.size(), sizeof(int32_t));
        }
    }
    EXPECT_TRUE(found_idx);

    // Check RDB$INDEX_SEGMENTS enumerates index columns
    QueryContext seg_query;
    seg_query.query = "SELECT RDB$FIELD_NAME FROM RDB$INDEX_SEGMENTS WHERE RDB$INDEX_NAME = 'fb_meta_idx_code'";
    ResultContext seg_result;
    ASSERT_EQ(adapter.executeRemoteQuery(seg_query, seg_result), Status::OK);
    ASSERT_FALSE(seg_result.rows.empty());
    bool found_code_col = false;
    for (const auto& row : seg_result.rows) {
        if (toString(row[0]) == "code") {
            found_code_col = true;
        }
    }
    EXPECT_TRUE(found_code_col);

    // Check RDB$RELATION_CONSTRAINTS reflects primary key
    QueryContext constraint_query;
    constraint_query.query = "SELECT RDB$CONSTRAINT_TYPE FROM RDB$RELATION_CONSTRAINTS WHERE RDB$RELATION_NAME = 'fb_meta'";
    ResultContext constraint_result;
    ASSERT_EQ(adapter.executeRemoteQuery(constraint_query, constraint_result), Status::OK);
    ASSERT_FALSE(constraint_result.rows.empty());
    bool has_primary = false;
    for (const auto& row : constraint_result.rows) {
        if (toString(row[0]) == "PRIMARY KEY") {
            has_primary = true;
            break;
        }
    }
    EXPECT_TRUE(has_primary);

    // Check RDB$VIEW_RELATIONS lists created view
    QueryContext view_query;
    view_query.query = "SELECT RDB$VIEW_NAME, RDB$RELATION_NAME FROM RDB$VIEW_RELATIONS WHERE RDB$VIEW_NAME = 'fb_meta_view'";
    ResultContext view_result;
    ASSERT_EQ(adapter.executeRemoteQuery(view_query, view_result), Status::OK);
    ASSERT_FALSE(view_result.rows.empty());
    bool has_view = false;
    for (const auto& row : view_result.rows) {
        if (toString(row[0]) == "fb_meta_view") {
            has_view = true;
            EXPECT_EQ(toString(row[1]), "fb_meta");
            break;
        }
    }
    EXPECT_TRUE(has_view);
}

}  // namespace
