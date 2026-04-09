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

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/tablespace.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace
{

constexpr std::array<uint32_t, 5> kSupportedPageSizes = {
    8192u,
    16384u,
    32768u,
    65536u,
    131072u,
};

struct BootstrapPageExpectation
{
    uint32_t page_id;
    uint16_t page_type;
    const char *label;
};

constexpr std::array<BootstrapPageExpectation, 5> kBootstrapPages = {{
    {BOOTSTRAP_PAGE_SYSTEM_STATE, PAGE_TYPE_SYSTEM_STATE, "system_state"},
    {BOOTSTRAP_PAGE_CATALOG_ROOT, PAGE_TYPE_CATALOG_ROOT, "catalog_root"},
    {BOOTSTRAP_PAGE_FSM_ROOT, PAGE_TYPE_FSM_ROOT, "fsm_root"},
    {BOOTSTRAP_PAGE_TX_MAP_ROOT, PAGE_TYPE_TRANSACTION_MAP, "tx_map_root"},
    {BOOTSTRAP_PAGE_RESERVED, PAGE_TYPE_BOOTSTRAP_RESERVED, "reserved"},
}};

bool readFullyAt(int fd, void *buffer, size_t size, off_t offset)
{
    auto *dst = static_cast<uint8_t *>(buffer);
    size_t transferred = 0;
    while (transferred < size)
    {
        const ssize_t rc = ::pread(fd,
                                   dst + transferred,
                                   size - transferred,
                                   offset + static_cast<off_t>(transferred));
        if (rc <= 0)
        {
            return false;
        }
        transferred += static_cast<size_t>(rc);
    }
    return true;
}

bool writeFullyAt(int fd, const void *buffer, size_t size, off_t offset)
{
    const auto *src = static_cast<const uint8_t *>(buffer);
    size_t transferred = 0;
    while (transferred < size)
    {
        const ssize_t rc = ::pwrite(fd,
                                    src + transferred,
                                    size - transferred,
                                    offset + static_cast<off_t>(transferred));
        if (rc <= 0)
        {
            return false;
        }
        transferred += static_cast<size_t>(rc);
    }
    return true;
}

void rewriteBootstrapPageHeader(const std::string &path,
                                uint32_t page_size,
                                uint32_t page_id,
                                const std::function<void(PageHeader *)> &mutator)
{
    int fd = ::open(path.c_str(), O_RDWR);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> page(page_size, 0);
    const off_t offset = static_cast<off_t>(page_id) * static_cast<off_t>(page_size);
    ASSERT_TRUE(readFullyAt(fd, page.data(), page.size(), offset));

    auto *header = reinterpret_cast<PageHeader *>(page.data());
    mutator(header);

    ASSERT_TRUE(writeFullyAt(fd, page.data(), page.size(), offset));
    ::close(fd);
}

class StorageRecoveryGateContractTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        for (const auto &path : cleanup_paths_)
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }

    std::string registerDbPath(const std::string &base)
    {
        const std::string path = scratchbird::testing::uniqueTestDbPath(base, ".sbdb");
        cleanup_paths_.push_back(path);
        return path;
    }

    std::string registerTablespacePath(const std::string &base)
    {
        const std::string path = scratchbird::testing::uniqueTestShortPath(base, ".sbts");
        cleanup_paths_.push_back(path);
        return path;
    }

private:
    std::vector<std::filesystem::path> cleanup_paths_;
};

TEST_F(StorageRecoveryGateContractTest,
       BootstrapPageTypeMismatchRefusesOpenAcrossSupportedPageSizes)
{
    for (uint32_t page_size : kSupportedPageSizes)
    {
        for (const auto &expected : kBootstrapPages)
        {
            SCOPED_TRACE("page_size=" + std::to_string(page_size) +
                         " page=" + std::to_string(expected.page_id) +
                         " label=" + expected.label);

            const std::string db_path =
                registerDbPath("bootstrap_type_gate_" + std::to_string(page_size) + "_" +
                               std::to_string(expected.page_id));

            ErrorContext create_ctx;
            ASSERT_EQ(Database::create(db_path, page_size, &create_ctx), Status::OK)
                << create_ctx.message;

            rewriteBootstrapPageHeader(
                db_path,
                page_size,
                expected.page_id,
                [&](PageHeader *header) {
                    header->page_type = PAGE_TYPE_DATABASE_HEADER;
                });

            Database db;
            ErrorContext open_ctx;
            EXPECT_EQ(db.open(db_path, &open_ctx), Status::PAGE_CORRUPT)
                << open_ctx.message;
            EXPECT_NE(open_ctx.message.find("has wrong type"),
                      std::string::npos)
                << open_ctx.message;
        }
    }
}

TEST_F(StorageRecoveryGateContractTest,
       BootstrapPageIdMismatchRefusesOpenAcrossSupportedPageSizes)
{
    for (uint32_t page_size : kSupportedPageSizes)
    {
        for (const auto &expected : kBootstrapPages)
        {
            SCOPED_TRACE("page_size=" + std::to_string(page_size) +
                         " page=" + std::to_string(expected.page_id) +
                         " label=" + expected.label);

            const std::string db_path =
                registerDbPath("bootstrap_id_gate_" + std::to_string(page_size) + "_" +
                               std::to_string(expected.page_id));

            ErrorContext create_ctx;
            ASSERT_EQ(Database::create(db_path, page_size, &create_ctx), Status::OK)
                << create_ctx.message;

            rewriteBootstrapPageHeader(
                db_path,
                page_size,
                expected.page_id,
                [&](PageHeader *header) {
                    header->page_id = expected.page_id + 100u;
                });

            Database db;
            ErrorContext open_ctx;
            EXPECT_EQ(db.open(db_path, &open_ctx), Status::PAGE_CORRUPT)
                << open_ctx.message;
            EXPECT_NE(open_ctx.message.find("Bootstrap page id mismatch"),
                      std::string::npos)
                << open_ctx.message;
        }
    }
}

TEST_F(StorageRecoveryGateContractTest,
       BootstrapMapTruncationRefusesOpenAcrossSupportedPageSizes)
{
    for (uint32_t page_size : kSupportedPageSizes)
    {
        SCOPED_TRACE("page_size=" + std::to_string(page_size));

        const std::string db_path =
            registerDbPath("bootstrap_truncate_gate_" + std::to_string(page_size));

        ErrorContext create_ctx;
        ASSERT_EQ(Database::create(db_path, page_size, &create_ctx), Status::OK)
            << create_ctx.message;

        ASSERT_EQ(::truncate(db_path.c_str(), static_cast<off_t>(page_size) * 5), 0);

        Database db;
        ErrorContext open_ctx;
        EXPECT_EQ(db.open(db_path, &open_ctx), Status::IO_ERROR)
            << open_ctx.message;
        EXPECT_NE(open_ctx.message.find("Failed to read bootstrap page"),
                  std::string::npos)
            << open_ctx.message;
    }
}

TEST_F(StorageRecoveryGateContractTest,
       InvalidPersistedHeaderPageSizeRefusesOpenAcrossSupportedPageSizes)
{
    for (uint32_t page_size : kSupportedPageSizes)
    {
        SCOPED_TRACE("page_size=" + std::to_string(page_size));

        const std::string db_path =
            registerDbPath("invalid_header_page_size_" + std::to_string(page_size));

        ErrorContext create_ctx;
        ASSERT_EQ(Database::create(db_path, page_size, &create_ctx), Status::OK)
            << create_ctx.message;

        rewriteBootstrapPageHeader(
            db_path,
            page_size,
            BOOTSTRAP_PAGE_DATABASE_HEADER,
            [](PageHeader *header) {
                header->page_size = 4096;
            });

        Database db;
        ErrorContext open_ctx;
        EXPECT_EQ(db.open(db_path, &open_ctx), Status::PAGE_CORRUPT)
            << open_ctx.message;
        EXPECT_NE(open_ctx.message.find("Invalid page size in database header"),
                  std::string::npos)
            << open_ctx.message;
    }
}

TEST_F(StorageRecoveryGateContractTest, TablespacePageSizeMismatchRefusesOpen)
{
    const std::string source_db_path = registerDbPath("tablespace_source_8k");
    const std::string target_db_path = registerDbPath("tablespace_target_16k");
    const std::string tablespace_path = registerTablespacePath("tablespace_mismatch_gate");

    ASSERT_EQ(Database::create(source_db_path, 8192), Status::OK);
    ASSERT_EQ(Database::create(target_db_path, 16384), Status::OK);

    {
        Database source_db;
        ErrorContext open_ctx;
        ASSERT_EQ(source_db.open(source_db_path, &open_ctx), Status::OK)
            << open_ctx.message;

        TablespaceConfig config;
        config.autoextend_enabled = true;
        config.autoextend_size_mb = 1;
        config.max_size_mb = 0;
        config.prealloc_pages = 2;

        ErrorContext create_ts_ctx;
        ASSERT_EQ(source_db.page_manager()->createTablespace(2,
                                                             "ts_gate_mismatch",
                                                             tablespace_path,
                                                             config,
                                                             &create_ts_ctx),
                  Status::OK)
            << create_ts_ctx.message;
        source_db.close();
    }

    Database target_db;
    ErrorContext open_ctx;
    ASSERT_EQ(target_db.open(target_db_path, &open_ctx), Status::OK)
        << open_ctx.message;

    ErrorContext attach_ctx;
    EXPECT_EQ(target_db.page_manager()->openTablespace(2, tablespace_path, true, &attach_ctx),
              Status::INVALID_ARGUMENT)
        << attach_ctx.message;
    EXPECT_NE(attach_ctx.message.find("Cannot open tablespace with different page size"),
              std::string::npos)
        << attach_ctx.message;
}

} // namespace
