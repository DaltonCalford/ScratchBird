/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/cluster_write_safety.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace scratchbird::core
{

    namespace
    {

        auto makeTmpCommitLogDir() -> std::filesystem::path
        {
            std::filesystem::path path = std::filesystem::temp_directory_path() /
                ("sb_shard_commit_log_" + generateUuidV7().toString());
            std::filesystem::remove_all(path);
            return path;
        }

    } // namespace

    TEST(ShardCommitLogPipelineTest, AppendIsStrictlyOrderedPerShard)
    {
        const std::filesystem::path tmp_dir = makeTmpCommitLogDir();
        const ID shard_id = generateUuidV7();
        ShardCommitLog log(tmp_dir.string());

        ShardCommitLogEntry first{};
        first.gtxid.shard_id = shard_id;
        first.gtxid.local_txn_id = 1;
        first.commit_timestamp_ns = 100;
        first.payload_format = "logical";
        first.payload = "insert into t values (1)";

        ShardCommitLogAppendResult result{};
        ASSERT_EQ(log.append(first, &result), Status::OK);
        EXPECT_TRUE(result.appended);
        EXPECT_EQ(result.reason, ShardCommitLogAppendReason::NONE);
        EXPECT_EQ(result.expected_next_local_txn_id, 2u);

        ShardCommitLogEntry out_of_order = first;
        out_of_order.gtxid.local_txn_id = 3;
        out_of_order.commit_timestamp_ns = 300;
        out_of_order.payload = "insert into t values (3)";

        ASSERT_EQ(log.append(out_of_order, &result), Status::INVALID_TRANSACTION_STATE);
        EXPECT_FALSE(result.appended);
        EXPECT_EQ(result.reason, ShardCommitLogAppendReason::OUT_OF_ORDER_LOCAL_TXN_ID);
        EXPECT_EQ(result.expected_next_local_txn_id, 2u);

        ShardCommitLogEntry second = first;
        second.gtxid.local_txn_id = 2;
        second.commit_timestamp_ns = 200;
        second.payload = "insert into t values (2)";

        ASSERT_EQ(log.append(second, &result), Status::OK);
        EXPECT_TRUE(result.appended);
        EXPECT_EQ(result.reason, ShardCommitLogAppendReason::NONE);
        EXPECT_EQ(result.expected_next_local_txn_id, 3u);

        std::vector<ShardCommitLogEntry> read_back;
        ASSERT_EQ(log.readEntries(shard_id, read_back), Status::OK);
        ASSERT_EQ(read_back.size(), 2u);
        EXPECT_EQ(read_back[0].gtxid.local_txn_id, 1u);
        EXPECT_EQ(read_back[1].gtxid.local_txn_id, 2u);
    }

    TEST(ShardCommitLogPipelineTest, AppendsAreDurableAndReadableAcrossInstances)
    {
        const std::filesystem::path tmp_dir = makeTmpCommitLogDir();
        const ID shard_id = generateUuidV7();
        const std::string durable_path = (tmp_dir / (shard_id.toString() + ".scl")).string();

        {
            ShardCommitLog log(tmp_dir.string());

            ShardCommitLogEntry e1{};
            e1.gtxid.shard_id = shard_id;
            e1.gtxid.local_txn_id = 1;
            e1.commit_timestamp_ns = 111;
            e1.payload = "payload-1";

            ShardCommitLogEntry e2 = e1;
            e2.gtxid.local_txn_id = 2;
            e2.commit_timestamp_ns = 222;
            e2.payload = "payload-2";

            ASSERT_EQ(log.append(e1), Status::OK);
            ASSERT_EQ(log.append(e2), Status::OK);
        }

        ASSERT_TRUE(std::filesystem::exists(durable_path));
        std::ifstream raw(durable_path);
        ASSERT_TRUE(raw.is_open());

        std::string line1;
        std::string line2;
        ASSERT_TRUE(static_cast<bool>(std::getline(raw, line1)));
        ASSERT_TRUE(static_cast<bool>(std::getline(raw, line2)));
        EXPECT_EQ(line1.rfind("1\t", 0), 0u);
        EXPECT_EQ(line2.rfind("2\t", 0), 0u);

        ShardCommitLog reopen(tmp_dir.string());
        std::vector<ShardCommitLogEntry> entries;
        ASSERT_EQ(reopen.readEntries(shard_id, entries), Status::OK);
        ASSERT_EQ(entries.size(), 2u);
        EXPECT_EQ(entries[0].payload, "payload-1");
        EXPECT_EQ(entries[1].payload, "payload-2");
    }

} // namespace scratchbird::core

