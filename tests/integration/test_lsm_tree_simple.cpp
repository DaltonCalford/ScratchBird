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
 * LSM-Tree Simple Integration Tests
 *
 * Simplified tests focusing on LSM-Tree functionality
 */

#include <gtest/gtest.h>
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <string>
#include <cstdio>
#include <unistd.h>
#include <filesystem>

using namespace scratchbird::core;

namespace {

std::string sanitizeName(const char* name)
{
    std::string out;
    for (char c : std::string(name ? name : "test"))
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_')
        {
            out.push_back(c);
        }
        else
        {
            out.push_back('_');
        }
    }
    return out;
}

struct LsmTempPaths
{
    std::string index_path;
    std::string db_path;

    ~LsmTempPaths()
    {
        std::error_code ec;
        if (!index_path.empty())
        {
            std::filesystem::remove_all(index_path, ec);
        }
        if (!db_path.empty())
        {
            std::filesystem::remove(db_path, ec);
        }
    }
};

LsmTempPaths makeTempPaths(const std::string& prefix)
{
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name = info ? sanitizeName(info->name()) : "test";
    std::string suffix = "_" + std::to_string(getpid()) + "_" + test_name;
    LsmTempPaths paths;
    paths.index_path = "/tmp/" + prefix + suffix;
    paths.db_path = "/tmp/" + prefix + suffix + ".db";
    return paths;
}

std::vector<uint8_t> makeKey(size_t index)
{
    std::string key_str = "key_" + std::to_string(index);
    return std::vector<uint8_t>(key_str.begin(), key_str.end());
}

std::vector<uint8_t> makeValue(size_t index)
{
    std::string value_str = "value_" + std::to_string(index) + "_data";
    return std::vector<uint8_t>(value_str.begin(), value_str.end());
}

} // namespace

TEST(LSMTreeSimpleIntegrationTest, BasicPutGet)
{
    auto paths = makeTempPaths("lsm_test_simple");

    ErrorContext ctx;
    Status status = Database::create(paths.db_path, 8192, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

    Database db;
    status = db.open(paths.db_path, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

    TransactionManager *txn_mgr = db.transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);
    uint64_t xid = txn_mgr->getCurrentXid();

    LSMTreeIndex index(&db, paths.index_path, txn_mgr, 4);
    status = index.create(nullptr);
    ASSERT_EQ(status, Status::OK);

    for (size_t i = 0; i < 10; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> value = makeValue(i);
        status = index.put(key, value, xid, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to put key " << i;
    }

    size_t found_count = 0;
    for (size_t i = 0; i < 10; i++)
    {
        std::vector<uint8_t> key = makeKey(i);
        std::vector<uint8_t> expected_value = makeValue(i);
        std::vector<uint8_t> actual_value;
        bool found = false;

        status = index.get(key, xid, &actual_value, &found, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to get key " << i;

        if (found && actual_value == expected_value)
        {
            found_count++;
        }
    }

    EXPECT_EQ(found_count, 10u);
    index.close(nullptr);
}
