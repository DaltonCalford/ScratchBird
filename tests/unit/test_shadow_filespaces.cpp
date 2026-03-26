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

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/page_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;
using scratchbird::testing::uniqueTestShortPath;

namespace
{
    constexpr size_t kPatternOffset = 256;
    constexpr size_t kPatternSize = 64;

    auto readFileSlice(const std::string& path,
                       uint64_t offset,
                       size_t size) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer(size, 0);
        const int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            return {};
        }

        const ssize_t bytes = ::pread(fd,
                                      buffer.data(),
                                      static_cast<size_t>(buffer.size()),
                                      static_cast<off_t>(offset));
        ::close(fd);
        if (bytes != static_cast<ssize_t>(size))
        {
            return {};
        }
        return buffer;
    }

    void stampPattern(std::vector<uint8_t>& page,
                      uint8_t seed)
    {
        if (page.size() < kPatternOffset + kPatternSize)
        {
            return;
        }

        for (size_t i = 0; i < kPatternSize; ++i)
        {
            page[kPatternOffset + i] = static_cast<uint8_t>(seed + i);
        }

        auto* header = reinterpret_cast<PageHeader*>(page.data());
        header->checksum = calculatePageChecksum(page.data(),
                                                 static_cast<uint32_t>(page.size()));
    }

    auto extractPattern(const std::vector<uint8_t>& page) -> std::vector<uint8_t>
    {
        if (page.size() < kPatternOffset + kPatternSize)
        {
            return {};
        }

        return std::vector<uint8_t>(page.begin() + static_cast<std::ptrdiff_t>(kPatternOffset),
                                    page.begin() + static_cast<std::ptrdiff_t>(kPatternOffset + kPatternSize));
    }
} // namespace

class ShadowFilespaceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = uniqueTestDbPath("test_shadow_filespaces", ".db");
        primary_shadow_path_ =
            uniqueTestShortPath("test_shadow_filespaces_primary_shadow", ".sbs");
        tablespace_path_ =
            uniqueTestShortPath("test_shadow_filespaces_tablespace", ".sbts");
        tablespace_shadow_path_ =
            uniqueTestShortPath("test_shadow_filespaces_tablespace_shadow", ".sbs");
        tablespace_shadow_path_2 =
            uniqueTestShortPath("test_shadow_filespaces_tablespace_shadow_2", ".sbs");
        drop_keep_shadow_path_ =
            uniqueTestShortPath("test_shadow_filespaces_drop_keep", ".sbs");
        drop_remove_shadow_path_ =
            uniqueTestShortPath("test_shadow_filespaces_drop_remove", ".sbs");

        std::filesystem::remove(db_path_);
        std::filesystem::remove(primary_shadow_path_);
        std::filesystem::remove(tablespace_path_);
        std::filesystem::remove(tablespace_shadow_path_);
        std::filesystem::remove(tablespace_shadow_path_2);
        std::filesystem::remove(drop_keep_shadow_path_);
        std::filesystem::remove(drop_remove_shadow_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_path_, &ctx), Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        db_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove(primary_shadow_path_);
        std::filesystem::remove(tablespace_path_);
        std::filesystem::remove(tablespace_shadow_path_);
        std::filesystem::remove(tablespace_shadow_path_2);
        std::filesystem::remove(drop_keep_shadow_path_);
        std::filesystem::remove(drop_remove_shadow_path_);
    }

    auto rewritePrimaryPagePattern(uint32_t page_id,
                                   uint8_t seed) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> page(db_.page_size(), 0);
        ErrorContext ctx;
        const Status read_status = db_.read_page(page_id, page.data(), &ctx);
        if (read_status != Status::OK)
        {
            ADD_FAILURE() << "Failed to read primary page: " << ctx.message;
            return {};
        }

        stampPattern(page, seed);
        ErrorContext write_ctx;
        const Status write_status = db_.write_page(page_id, page.data(), &write_ctx);
        if (write_status != Status::OK)
        {
            ADD_FAILURE() << "Failed to write primary page: " << write_ctx.message;
            return {};
        }

        return page;
    }

    auto rewriteTablespacePagePattern(GPID gpid,
                                      uint8_t seed) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> page(db_.page_size(), 0);
        ErrorContext ctx;
        const Status read_status = db_.read_page_global(gpid, page.data(), &ctx);
        if (read_status != Status::OK)
        {
            ADD_FAILURE() << "Failed to read tablespace page: " << ctx.message;
            return {};
        }

        stampPattern(page, seed);
        ErrorContext write_ctx;
        const Status write_status = db_.write_page_global(gpid, page.data(), &write_ctx);
        if (write_status != Status::OK)
        {
            ADD_FAILURE() << "Failed to write tablespace page: " << write_ctx.message;
            return {};
        }

        return page;
    }

    Database db_;
    std::string db_path_;
    std::string primary_shadow_path_;
    std::string tablespace_path_;
    std::string tablespace_shadow_path_;
    std::string tablespace_shadow_path_2;
    std::string drop_keep_shadow_path_;
    std::string drop_remove_shadow_path_;
};

TEST_F(ShadowFilespaceTest, PrimaryShadowBackfillMirrorsSubsequentWrites)
{
    auto* page_manager = db_.page_manager();
    ASSERT_NE(page_manager, nullptr);

    ErrorContext ctx;
    uint32_t page_id = 0;
    ASSERT_EQ(page_manager->allocatePage(page_id, &ctx), Status::OK) << ctx.message;

    const auto initial_page = rewritePrimaryPagePattern(page_id, 0x10);
    ASSERT_FALSE(initial_page.empty());
    ASSERT_EQ(db_.sync(&ctx), Status::OK) << ctx.message;

    ID shadow_id{};
    ASSERT_EQ(db_.createShadowFilespace(PRIMARY_TABLESPACE_ID,
                                        primary_shadow_path_,
                                        &shadow_id,
                                        &ctx),
              Status::OK)
        << ctx.message;

    std::vector<Database::ShadowFilespaceSnapshot> shadows;
    ASSERT_EQ(db_.listShadowFilespaces(shadows, &ctx), Status::OK) << ctx.message;
    auto shadow_it =
        std::find_if(shadows.begin(), shadows.end(), [&shadow_id](const auto& item) {
            return item.shadow_id == shadow_id;
        });
    ASSERT_NE(shadow_it, shadows.end());
    EXPECT_TRUE(shadow_it->active);
    EXPECT_FALSE(shadow_it->promoted);
    EXPECT_EQ(shadow_it->source_tablespace_id, PRIMARY_TABLESPACE_ID);
    EXPECT_EQ(shadow_it->source_path, db_path_);

    const uint64_t page_offset = static_cast<uint64_t>(page_id) * db_.page_size();
    EXPECT_EQ(readFileSlice(db_path_, page_offset + kPatternOffset, kPatternSize),
              extractPattern(initial_page));
    EXPECT_EQ(readFileSlice(primary_shadow_path_, page_offset + kPatternOffset, kPatternSize),
              extractPattern(initial_page));

    const auto updated_page = rewritePrimaryPagePattern(page_id, 0x40);
    ASSERT_FALSE(updated_page.empty());
    ASSERT_EQ(db_.sync(&ctx), Status::OK) << ctx.message;

    EXPECT_EQ(readFileSlice(primary_shadow_path_, page_offset + kPatternOffset, kPatternSize),
              extractPattern(updated_page));
}

TEST_F(ShadowFilespaceTest, PromotedTablespaceShadowBecomesLiveRouteForLaterShadows)
{
    auto* page_manager = db_.page_manager();
    auto* catalog_manager = db_.catalog_manager();
    ASSERT_NE(page_manager, nullptr);
    ASSERT_NE(catalog_manager, nullptr);

    ErrorContext ctx;
    uint16_t tablespace_id = 0;
    ASSERT_EQ(catalog_manager->createTablespace("ts_shadow",
                                                tablespace_path_,
                                                true,
                                                1,
                                                0,
                                                2,
                                                tablespace_id,
                                                &ctx),
              Status::OK)
        << ctx.message;

    GPID gpid = INVALID_GPID;
    ASSERT_EQ(page_manager->allocatePageInTablespace(tablespace_id, &gpid, &ctx), Status::OK)
        << ctx.message;

    const auto original_page = rewriteTablespacePagePattern(gpid, 0x21);
    ASSERT_FALSE(original_page.empty());
    ASSERT_EQ(db_.sync(&ctx), Status::OK) << ctx.message;

    ID first_shadow_id{};
    ASSERT_EQ(db_.createShadowFilespace(tablespace_id,
                                        tablespace_shadow_path_,
                                        &first_shadow_id,
                                        &ctx),
              Status::OK)
        << ctx.message;

    const uint64_t page_offset = getPageNumber(gpid) * db_.page_size();
    EXPECT_EQ(readFileSlice(tablespace_path_, page_offset + kPatternOffset, kPatternSize),
              extractPattern(original_page));
    EXPECT_EQ(readFileSlice(tablespace_shadow_path_, page_offset + kPatternOffset, kPatternSize),
              extractPattern(original_page));

    ASSERT_EQ(db_.promoteShadowFilespace(first_shadow_id, &ctx), Status::OK) << ctx.message;

    const auto promoted_page = rewriteTablespacePagePattern(gpid, 0x61);
    ASSERT_FALSE(promoted_page.empty());
    ASSERT_EQ(db_.sync(&ctx), Status::OK) << ctx.message;

    EXPECT_EQ(readFileSlice(tablespace_shadow_path_, page_offset + kPatternOffset, kPatternSize),
              extractPattern(promoted_page));
    EXPECT_EQ(readFileSlice(tablespace_path_, page_offset + kPatternOffset, kPatternSize),
              extractPattern(original_page));

    ID second_shadow_id{};
    ASSERT_EQ(db_.createShadowFilespace(tablespace_id,
                                        tablespace_shadow_path_2,
                                        &second_shadow_id,
                                        &ctx),
              Status::OK)
        << ctx.message;

    std::vector<Database::ShadowFilespaceSnapshot> shadows;
    ASSERT_EQ(db_.listShadowFilespaces(shadows, &ctx), Status::OK) << ctx.message;
    auto second_shadow_it =
        std::find_if(shadows.begin(), shadows.end(), [&second_shadow_id](const auto& item) {
            return item.shadow_id == second_shadow_id;
        });
    ASSERT_NE(second_shadow_it, shadows.end());
    EXPECT_EQ(second_shadow_it->source_path, tablespace_shadow_path_);
    EXPECT_EQ(readFileSlice(tablespace_shadow_path_2, page_offset + kPatternOffset, kPatternSize),
              extractPattern(promoted_page));
}

TEST_F(ShadowFilespaceTest, DropShadowFilespaceRespectsKeepFilePolicy)
{
    ErrorContext ctx;
    ID keep_shadow_id{};
    ASSERT_EQ(db_.createShadowFilespace(PRIMARY_TABLESPACE_ID,
                                        drop_keep_shadow_path_,
                                        &keep_shadow_id,
                                        &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_.dropShadowFilespace(keep_shadow_id, true, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(std::filesystem::exists(drop_keep_shadow_path_));

    ID remove_shadow_id{};
    ASSERT_EQ(db_.createShadowFilespace(PRIMARY_TABLESPACE_ID,
                                        drop_remove_shadow_path_,
                                        &remove_shadow_id,
                                        &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_.dropShadowFilespace(remove_shadow_id, false, &ctx), Status::OK) << ctx.message;
    EXPECT_FALSE(std::filesystem::exists(drop_remove_shadow_path_));

    std::vector<Database::ShadowFilespaceSnapshot> shadows;
    ASSERT_EQ(db_.listShadowFilespaces(shadows, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(shadows.empty());
}
