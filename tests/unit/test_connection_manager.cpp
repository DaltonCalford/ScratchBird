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

#include "scratchbird/core/error_context.h"
#include "scratchbird/network/connection_handler.h"
#include "scratchbird/network/event_loop.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/thread_pool.h"

using namespace scratchbird::core;
using namespace scratchbird::network;

class ConnectionManagerProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_NE(loop_ = EventLoop::create(&ctx_), nullptr);
        ASSERT_NE(pool_ = ThreadPool::create(2, &ctx_), nullptr);
        ASSERT_EQ(pool_->start(), Status::OK);
    }

    void TearDown() override {
        if (pool_) {
            pool_->stop();
        }
    }

    std::unique_ptr<Socket> makeSocket() {
        auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx_);
        EXPECT_NE(sock, nullptr);
        return sock;
    }

    ErrorContext ctx_;
    std::unique_ptr<EventLoop> loop_;
    std::unique_ptr<ThreadPool> pool_;
};

TEST_F(ConnectionManagerProtocolTest, DefaultConfigBindsNativeProtocol) {
    auto manager = ConnectionManager::create(loop_.get(), pool_.get(), {}, &ctx_);
    ASSERT_NE(manager, nullptr);

    auto id = manager->acceptConnection(makeSocket());
    ASSERT_NE(id, INVALID_CONNECTION_ID);

    auto* conn = manager->getConnection(id);
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(conn->getState(), ConnectionState::AUTHENTICATING);
    EXPECT_EQ(conn->getProtocol(), ProtocolType::NATIVE);

    manager->closeConnection(id);
}

TEST_F(ConnectionManagerProtocolTest, FixedProtocolBypassesDetection) {
    ConnectionManagerConfig config;
    config.fixed_protocol = ProtocolType::POSTGRESQL;
    config.allowed_protocols = {ProtocolType::POSTGRESQL};

    auto manager = ConnectionManager::create(loop_.get(), pool_.get(), config, &ctx_);
    ASSERT_NE(manager, nullptr);

    auto id = manager->acceptConnection(makeSocket());
    ASSERT_NE(id, INVALID_CONNECTION_ID);

    auto* conn = manager->getConnection(id);
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(conn->getProtocol(), ProtocolType::POSTGRESQL);
    EXPECT_EQ(conn->getState(), ConnectionState::AUTHENTICATING);

    manager->closeConnection(id);
}

TEST_F(ConnectionManagerProtocolTest, FixedProtocolRespectsAllowList) {
    ConnectionManagerConfig config;
    config.fixed_protocol = ProtocolType::POSTGRESQL;
    config.allowed_protocols = {ProtocolType::MYSQL};

    auto manager = ConnectionManager::create(loop_.get(), pool_.get(), config, &ctx_);
    EXPECT_EQ(manager, nullptr);
}

TEST_F(ConnectionManagerProtocolTest, AutoDetectFixedProtocolRejected) {
    ConnectionManagerConfig config;
    config.fixed_protocol = ProtocolType::AUTO_DETECT;
    config.allowed_protocols = {ProtocolType::NATIVE};

    auto manager = ConnectionManager::create(loop_.get(), pool_.get(), config, &ctx_);
    EXPECT_EQ(manager, nullptr);
}

TEST_F(ConnectionManagerProtocolTest, InvalidAllowListRejected) {
    ConnectionManagerConfig config;
    config.fixed_protocol = ProtocolType::NATIVE;
    config.allowed_protocols.clear();

    auto manager = ConnectionManager::create(loop_.get(), pool_.get(), config, &ctx_);
    EXPECT_EQ(manager, nullptr);
}
