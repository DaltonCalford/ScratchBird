/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "scratchbird/client/connection.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/server/scratchbird_server.h"
#include "test_helpers.h"

using scratchbird::client::Connection;
using scratchbird::client::ConnectionConfig;
using scratchbird::client::ResultSet;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::server::IPCMethod;
using scratchbird::server::ScratchBirdServer;
using scratchbird::server::ServerConfig;

namespace {

class LocalIpcSessionIdentityTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!scratchbird::testing::networkTestsEnabled()) {
            GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
        }

        config_.database_path =
            scratchbird::testing::uniqueTestDbPath("local_ipc_session_identity", ".sbdb");
        std::error_code ec;
        std::filesystem::remove(config_.database_path, ec);

#ifdef _WIN32
        config_.ipc_method = IPCMethod::NAMED_PIPE;
        expected_local_endpoint_ =
            "pipe:" + scratchbird::server::getIPCPath(config_.database_path, config_.ipc_method);
#else
        config_.ipc_method = IPCMethod::UNIX_SOCKET;
        config_.ipc_path = scratchbird::testing::uniqueTestSocketPath("sb_local_identity");
        std::filesystem::remove(config_.ipc_path, ec);
        expected_local_endpoint_ = "unix:" + config_.ipc_path;
#endif

        config_.auto_create_db = true;
        config_.accept_timeout_ms = 50;
        config_.verbose = false;

        server_ = std::make_unique<ScratchBirdServer>(config_);
        ASSERT_EQ(server_->startAsync(&ctx_), Status::OK) << ctx_.message;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ConnectionConfig client_config;
        client_config.database_name = config_.database_path;
        client_config.ipc_method = config_.ipc_method;
        client_config.auto_start_server = false;
        client_config.username = "SysArch";
        client_config.password = "replaceme";
        if (!config_.ipc_path.empty()) {
            client_config.socket_path = config_.ipc_path;
        }

        ASSERT_EQ(connection_.connect(client_config, &ctx_), Status::OK) << ctx_.message;
    }

    void TearDown() override {
        connection_.disconnect();
        if (server_) {
            server_->shutdown();
            server_->waitForShutdown(2000);
            server_.reset();
        }
    }

    std::unordered_map<std::string, std::string> loadContextVariables() {
        ResultSet results;
        ErrorContext query_ctx;
        EXPECT_EQ(connection_.executeQuery(
                      "SELECT variable_name, variable_value FROM sys.context_variables",
                      &results,
                      &query_ctx),
                  Status::OK)
            << query_ctx.message;

        std::unordered_map<std::string, std::string> values;
        while (results.next()) {
            values.emplace(results.getString(0), results.getString(1));
        }
        return values;
    }

    ServerConfig config_{};
    std::unique_ptr<ScratchBirdServer> server_;
    Connection connection_;
    ErrorContext ctx_{};
    std::string expected_local_endpoint_;
};

TEST_F(LocalIpcSessionIdentityTest, ShowVariablesExposeLocalEndpointAndSessionIdentity) {
    const auto values = loadContextVariables();

    auto requireValue = [&](const std::string& name) -> std::string {
        auto it = values.find(name);
        EXPECT_NE(it, values.end()) << "Missing context variable: " << name;
        return it == values.end() ? std::string() : it->second;
    };

    EXPECT_EQ(requireValue("SB$TRANSPORT_METHOD"),
#ifdef _WIN32
              "NAMED_PIPE"
#else
              "UNIX_SOCKET"
#endif
    );
    EXPECT_EQ(requireValue("SB$TRANSPORT_FAMILY"), "LOCAL_NON_IP");
    EXPECT_EQ(requireValue("SB$LOCAL_ENDPOINT"), expected_local_endpoint_);
    EXPECT_FALSE(requireValue("SB$REMOTE_ENDPOINT").empty());
    EXPECT_EQ(requireValue("SB$DATABASE_PATH"), config_.database_path);
    EXPECT_FALSE(requireValue("SB$DATABASE_UUID").empty());
    EXPECT_FALSE(requireValue("SB$SERVER_INSTANCE_ID").empty());
    EXPECT_FALSE(requireValue("SB$PROTOCOL_SESSION_ID").empty());
    EXPECT_FALSE(requireValue("SB$CATALOG_SESSION_ID").empty());
    EXPECT_FALSE(requireValue("SB$AUTHKEY_ID").empty());
    EXPECT_EQ(requireValue("SB$EMULATION_MODE"), "native");
}

} // namespace
