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
#include "scratchbird/core/status.h"
#include "scratchbird/pool/connection_pool.h"
#include "scratchbird/pool/statement_cache.h"

using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::pool::ConnectionConfig;
using scratchbird::pool::ConnectionState;
using scratchbird::pool::DatabasePool;
using scratchbird::pool::PoolConfig;
using scratchbird::pool::PooledConnection;

TEST(AuxPoolLaneTest, ConnectFailsExplicitlyInsteadOfSimulatingSuccess) {
    PooledConnection conn;
    ConnectionConfig config;
    config.host = "127.0.0.1";
    config.port = 3092;
    config.database = "testdb";
    config.user = "tester";

    ErrorContext ctx;
    EXPECT_EQ(conn.connect(config, &ctx), Status::NOT_IMPLEMENTED);
    EXPECT_EQ(ctx.code, Status::NOT_IMPLEMENTED);
    EXPECT_NE(ctx.message.find("scratchbird::core::ConnectionPool"), std::string::npos);
    EXPECT_EQ(conn.state(), ConnectionState::CREATED);
    EXPECT_FALSE(conn.isValid());
}

TEST(AuxPoolLaneTest, PrewarmReturnsNotImplementedWhenTransportHooksAreUnavailable) {
    PoolConfig config;
    config.prewarm = false;
    config.statement_cache_enabled = true;
    config.result_cache_enabled = true;

    DatabasePool pool("testdb", config);
    ASSERT_NE(pool.statementCache(), nullptr);
    ASSERT_NE(pool.resultCache(), nullptr);

    ErrorContext ctx;
    EXPECT_EQ(pool.prewarm(1, &ctx), Status::NOT_IMPLEMENTED);
    EXPECT_EQ(ctx.code, Status::NOT_IMPLEMENTED);
    EXPECT_EQ(pool.idleConnectionCount(), 0u);
    EXPECT_EQ(pool.activeConnectionCount(), 0u);
    EXPECT_TRUE(pool.getAllConnections().empty());
}
