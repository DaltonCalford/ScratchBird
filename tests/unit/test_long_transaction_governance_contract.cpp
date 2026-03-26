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
#include <memory>
#include <string>

#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"

using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::IsolationLevel;
using scratchbird::core::LongTransactionMonitor;
using scratchbird::core::LongTransactionPolicy;
using scratchbird::core::LongTransactionReason;
using scratchbird::core::ProcArrayManager;
using scratchbird::core::Status;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    auto nowMicrosLocal() -> uint64_t
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    class LongTransactionGovernanceContractTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            db_file_ = std::make_unique<TestDatabaseFile>("long_txn_governance", ".sbdb");
            ErrorContext ctx;
            ASSERT_EQ(Database::create(db_file_->path(), 8192, &ctx), Status::OK) << ctx.message;
            ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;
            ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
            monitor_ = db_.long_transaction_monitor();
            ASSERT_NE(monitor_, nullptr);
        }

        void TearDown() override
        {
            conn_.reset();
            db_.close();
        }

        void ageCurrentTransaction(uint32_t seconds)
        {
            ErrorContext ctx;
            ASSERT_EQ(ProcArrayManager::setTransactionStartTime(
                          conn_->getProcId(),
                          nowMicrosLocal() - (static_cast<uint64_t>(seconds) * 1000000ULL),
                          &ctx),
                      Status::OK)
                << ctx.message;
        }

        auto openPeerConnection() -> std::unique_ptr<ConnectionContext>
        {
            std::unique_ptr<ConnectionContext> peer;
            ErrorContext ctx;
            EXPECT_EQ(db_.connect(peer, &ctx), Status::OK) << ctx.message;
            return peer;
        }

        void advanceXids(ConnectionContext *peer, uint32_t rounds)
        {
            ASSERT_NE(peer, nullptr);
            for (uint32_t i = 0; i < rounds; ++i)
            {
                ErrorContext ctx;
                ASSERT_EQ(peer->commit(&ctx), Status::OK) << ctx.message;
            }
        }

        auto noticesContain(const std::vector<std::string> &notices,
                            const std::string &needle) const -> bool
        {
            for (const auto &notice : notices)
            {
                if (notice.find(needle) != std::string::npos)
                {
                    return true;
                }
            }
            return false;
        }

        Database db_;
        LongTransactionMonitor *monitor_ = nullptr;
        std::unique_ptr<ConnectionContext> conn_;
        std::unique_ptr<TestDatabaseFile> db_file_;
    };

    TEST_F(LongTransactionGovernanceContractTest, WarningNoticeDeliveredAtStatementBoundary)
    {
        monitor_->setPolicy(LongTransactionPolicy::LOG);
        monitor_->setWarningThreshold(1);
        monitor_->setCriticalThreshold(120);
        monitor_->setClientNoticeEnabled(true);
        ageCurrentTransaction(3);

        ErrorContext check_ctx;
        EXPECT_EQ(monitor_->checkLongTransactions(&check_ctx), 1u);

        auto stats = monitor_->getStatistics();
        EXPECT_EQ(stats.current_long_transactions, 1u);
        EXPECT_EQ(stats.warnings_logged, 1u);
        EXPECT_EQ(stats.notices_emitted, 1u);
        EXPECT_NE(stats.last_reason_mask &
                      static_cast<uint8_t>(LongTransactionReason::AGE_THRESHOLD),
                  0u);

        ErrorContext stmt_ctx;
        EXPECT_EQ(conn_->beginStatementTracking("SELECT 1", &stmt_ctx), Status::OK)
            << stmt_ctx.message;
        auto notices = conn_->consumeNotices();
        EXPECT_TRUE(noticesContain(notices, "Long-running transaction warning"));
        conn_->endStatementTrackingSuccess(1);
    }

    TEST_F(LongTransactionGovernanceContractTest,
           CriticalGcPinRequestsBackendOwnedRollbackAndFreshTransaction)
    {
        monitor_->setPolicy(LongTransactionPolicy::ROLLBACK_ALL);
        monitor_->setWarningThreshold(3600);
        monitor_->setCriticalThreshold(7200);
        monitor_->setWarningXidLagThreshold(1);
        monitor_->setCriticalXidLagThreshold(2);
        monitor_->setClientNoticeEnabled(true);

        const uint64_t original_xid = conn_->getCurrentXid();
        ErrorContext pin_ctx;
        ASSERT_EQ(
            ProcArrayManager::setBackendXmin(conn_->getProcId(), original_xid, &pin_ctx),
            Status::OK)
            << pin_ctx.message;

        auto peer = openPeerConnection();
        advanceXids(peer.get(), 4);

        ErrorContext check_ctx;
        EXPECT_GE(monitor_->checkLongTransactions(&check_ctx), 1u);

        scratchbird::core::BackendGovernanceDirective directive;
        ErrorContext directive_ctx;
        ASSERT_EQ(ProcArrayManager::getBackendGovernanceDirective(
                      conn_->getProcId(), &directive, &directive_ctx),
                  Status::OK)
            << directive_ctx.message;
        EXPECT_TRUE(directive.rollback_requested);
        EXPECT_NE(directive.reason_mask &
                      static_cast<uint8_t>(LongTransactionReason::GC_HORIZON_PIN),
                  0u);
        EXPECT_NE(directive.reason_mask &
                      static_cast<uint8_t>(LongTransactionReason::SNAPSHOT_HORIZON_PIN),
                  0u);

        auto stats = monitor_->getStatistics();
        EXPECT_GE(stats.readwrite_rolled_back, 1u);
        EXPECT_GE(stats.gc_horizon_flags, 1u);
        EXPECT_GE(stats.snapshot_horizon_flags, 1u);

        ErrorContext stmt_ctx;
        EXPECT_EQ(conn_->beginStatementTracking("SELECT 1", &stmt_ctx), Status::QUERY_CANCELED);
        EXPECT_NE(std::string(stmt_ctx.message).find("rollback requested"), std::string::npos);
        EXPECT_GT(conn_->getCurrentXid(), original_xid);

        auto notices = conn_->consumeNotices();
        EXPECT_TRUE(noticesContain(notices, "rollback requested"));

        ErrorContext recovery_ctx;
        EXPECT_EQ(conn_->beginStatementTracking("SELECT 2", &recovery_ctx), Status::OK)
            << recovery_ctx.message;
        conn_->endStatementTrackingSuccess(1);
    }

    TEST_F(LongTransactionGovernanceContractTest,
           CriticalAgeRequestsTerminationWithDetailedMessage)
    {
        monitor_->setPolicy(LongTransactionPolicy::TERMINATE_CONNECTION);
        monitor_->setWarningThreshold(1);
        monitor_->setCriticalThreshold(1);
        monitor_->setClientNoticeEnabled(true);
        ageCurrentTransaction(2);

        ErrorContext check_ctx;
        EXPECT_EQ(monitor_->checkLongTransactions(&check_ctx), 1u);

        auto stats = monitor_->getStatistics();
        EXPECT_EQ(stats.connections_terminated, 1u);
        EXPECT_NE(stats.last_reason_mask &
                      static_cast<uint8_t>(LongTransactionReason::AGE_THRESHOLD),
                  0u);

        ErrorContext commit_ctx;
        EXPECT_EQ(conn_->commit(&commit_ctx), Status::IO_ERROR);
        EXPECT_NE(std::string(commit_ctx.message).find("Connection termination requested"),
                  std::string::npos);
        EXPECT_EQ(conn_->getCurrentXid(), 0u);

        auto notices = conn_->consumeNotices();
        EXPECT_TRUE(noticesContain(notices, "Connection termination requested"));
    }
} // namespace
