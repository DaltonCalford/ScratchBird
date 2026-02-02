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
#include <filesystem>
#include <atomic>
#include <unistd.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/config.h"

using namespace scratchbird::core;

class TablespaceRecoveryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_id_ = std::to_string(getpid()) + "_" + std::to_string(test_counter_++);
        db_path_ = "/tmp/test_tablespace_recovery_" + test_id_ + ".db";
        ts_path_ = "/tmp/test_tablespace_recovery_" + test_id_ + ".sbts";
        std::filesystem::remove(db_path_);
        std::filesystem::remove(ts_path_);
        Config::getInstance().addCommandLineArg("storage", "tablespace_recovery_mode", "strict");
    }

    void TearDown() override
    {
        Config::getInstance().addCommandLineArg("storage", "tablespace_recovery_mode", "strict");
        std::filesystem::remove(db_path_);
        std::filesystem::remove(ts_path_);
    }

    std::string test_id_;
    std::string db_path_;
    std::string ts_path_;
    static inline std::atomic<int> test_counter_{0};
};

TEST_F(TablespaceRecoveryTest, MissingTablespaceStrictFailsOpen)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    uint16_t tablespace_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(db.catalog_manager()->createTablespace("ts_recovery", ts_path_, true, 1, 0, 2,
                                                     tablespace_id, &ctx),
              Status::OK)
        << ctx.message;
    db.close();

    std::filesystem::remove(ts_path_);

    Database reopened;
    Status status = reopened.open(db_path_);
    EXPECT_NE(status, Status::OK);
}

TEST_F(TablespaceRecoveryTest, MissingTablespaceAllowMissingOpens)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    uint16_t tablespace_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(db.catalog_manager()->createTablespace("ts_recovery", ts_path_, true, 1, 0, 2,
                                                     tablespace_id, &ctx),
              Status::OK)
        << ctx.message;
    db.close();

    std::filesystem::remove(ts_path_);
    Config::getInstance().addCommandLineArg("storage", "tablespace_recovery_mode", "allow_missing");

    Database reopened;
    Status status = reopened.open(db_path_);
    EXPECT_EQ(status, Status::OK);
    reopened.close();
}

TEST_F(TablespaceRecoveryTest, AttachRejectsUuidMismatchWithoutOverride)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database source_db;
    ASSERT_EQ(source_db.open(db_path_), Status::OK);

    uint16_t tablespace_id = 0;
    ErrorContext ctx;
    ASSERT_EQ(source_db.catalog_manager()->createTablespace("ts_attach", ts_path_, true, 1, 0, 2,
                                                            tablespace_id, &ctx),
              Status::OK)
        << ctx.message;
    source_db.close();

    std::string other_db_path = "/tmp/test_tablespace_recovery_other_" + test_id_ + ".db";
    std::filesystem::remove(other_db_path);
    ASSERT_EQ(Database::create(other_db_path, 8192), Status::OK);

    Database target_db;
    ASSERT_EQ(target_db.open(other_db_path), Status::OK);

    uint16_t attached_id = 0;
    Status attach_status = target_db.catalog_manager()->attachTablespace(
        ts_path_, "", true, false, attached_id, &ctx);
    EXPECT_NE(attach_status, Status::OK);

    target_db.close();
    std::filesystem::remove(other_db_path);
}
