/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <vector>

#include <gtest/gtest.h>

#include "scratchbird/core/index_page_diagnostics.h"

namespace scratchbird::core
{

namespace
{

struct IndexPageFixture
{
    std::vector<uint8_t> page;
    ID index_uuid;
};

auto buildValidIndexPage(uint16_t page_type, uint16_t opaque_len) -> IndexPageFixture
{
    IndexPageFixture fixture{};
    fixture.page.resize(8192u, 0u);

    auto *header = reinterpret_cast<PageHeader *>(fixture.page.data());
    header->magic = K_MAGIC_SBRD;
    header->version = 1u;
    header->page_type = page_type;
    header->page_size = static_cast<uint32_t>(fixture.page.size());
    header->flags = PAGE_FLAG_CHECKSUM_VALID;
    header->page_id = 99u;
    header->free_offset = sizeof(PageHeader);
    header->item_count = 0u;
    header->free_space = static_cast<uint16_t>(fixture.page.size() - sizeof(PageHeader));
    pageSetSpecial(*header, fixture.page.size() - sizeof(IndexPageHeader) - opaque_len);

    const uint32_t special_offset = pageSpecial(*header);
    auto *idx = reinterpret_cast<IndexPageHeader *>(fixture.page.data() + special_offset);
    idx->page_level = 0u;
    idx->flags = static_cast<uint16_t>(INDEX_PAGE_FLAG_ROOT | INDEX_PAGE_FLAG_LEFTMOST |
                                       INDEX_PAGE_FLAG_RIGHTMOST);
    idx->left_sibling = 0u;
    idx->right_sibling = 0u;
    idx->opaque_len = opaque_len;
    idx->reserved = 0u;
    fixture.index_uuid = generateUuidV7();
    setIndexPageHeaderUuid(*idx, fixture.index_uuid);

    header->checksum = calculatePageChecksum(fixture.page.data(),
                                             static_cast<uint32_t>(fixture.page.size()));
    return fixture;
}

} // namespace

TEST(IndexCorruptionErrorContractTest, ValidPageReturnsOk)
{
    IndexPageFixture fixture = buildValidIndexPage(PAGE_TYPE_BTREE_LEAF, 16u);
    IndexPageDiagnosticReport report{};
    ErrorContext ctx{};

    const Status status = IndexPageDiagnostics::validatePage(
        fixture.page.data(), 8192u, PAGE_TYPE_BTREE_LEAF, 16u, &fixture.index_uuid, &report, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(report.ok());
}

TEST(IndexCorruptionErrorContractTest, InvalidChecksumReturnsChecksumMismatch)
{
    IndexPageFixture fixture = buildValidIndexPage(PAGE_TYPE_BTREE_LEAF, 16u);
    fixture.page[512] ^= 0x5Au;

    IndexPageDiagnosticReport report{};
    ErrorContext ctx{};
    const Status status = IndexPageDiagnostics::validatePage(
        fixture.page.data(), 8192u, PAGE_TYPE_BTREE_LEAF, 16u, &fixture.index_uuid, &report, &ctx);

    ASSERT_EQ(status, Status::CHECKSUM_MISMATCH);
    ASSERT_FALSE(report.ok());
    EXPECT_EQ(report.issues[0].code, IndexPageIssueCode::INVALID_CHECKSUM);
}

TEST(IndexCorruptionErrorContractTest, InvalidPageTypeReturnsPageCorrupt)
{
    IndexPageFixture fixture = buildValidIndexPage(PAGE_TYPE_BTREE_LEAF, 16u);
    auto *header = reinterpret_cast<PageHeader *>(fixture.page.data());
    header->page_type = PAGE_TYPE_HEAP;
    header->checksum = calculatePageChecksum(fixture.page.data(), 8192u);

    IndexPageDiagnosticReport report{};
    ErrorContext ctx{};
    const Status status = IndexPageDiagnostics::validatePage(
        fixture.page.data(), 8192u, PAGE_TYPE_BTREE_LEAF, 16u, &fixture.index_uuid, &report, &ctx);

    ASSERT_EQ(status, Status::PAGE_CORRUPT);
    ASSERT_FALSE(report.ok());
    EXPECT_EQ(report.issues[0].code, IndexPageIssueCode::INVALID_PAGE_TYPE);
}

TEST(IndexCorruptionErrorContractTest, InvalidOpaqueSizeReturnsPageCorrupt)
{
    IndexPageFixture fixture = buildValidIndexPage(PAGE_TYPE_BTREE_LEAF, 16u);
    IndexPageDiagnosticReport report{};
    ErrorContext ctx{};

    const Status status = IndexPageDiagnostics::validatePage(
        fixture.page.data(), 8192u, PAGE_TYPE_BTREE_LEAF, 20u, &fixture.index_uuid, &report, &ctx);

    ASSERT_EQ(status, Status::PAGE_CORRUPT);
    ASSERT_FALSE(report.ok());
    EXPECT_EQ(report.issues[0].code, IndexPageIssueCode::INVALID_INDEX_OPAQUE_SIZE);
}

TEST(IndexCorruptionErrorContractTest, InvalidSiblingContractReturnsPageCorrupt)
{
    IndexPageFixture fixture = buildValidIndexPage(PAGE_TYPE_BTREE_LEAF, 16u);
    auto *header = reinterpret_cast<PageHeader *>(fixture.page.data());
    auto *idx = reinterpret_cast<IndexPageHeader *>(fixture.page.data() + pageSpecial(*header));
    idx->flags = INDEX_PAGE_FLAG_RIGHTMOST;
    idx->left_sibling = 10u;
    idx->right_sibling = 33u;
    header->checksum = calculatePageChecksum(fixture.page.data(), 8192u);

    IndexPageDiagnosticReport report{};
    ErrorContext ctx{};
    const Status status = IndexPageDiagnostics::validatePage(
        fixture.page.data(), 8192u, PAGE_TYPE_BTREE_LEAF, 16u, &fixture.index_uuid, &report, &ctx);

    ASSERT_EQ(status, Status::PAGE_CORRUPT);
    ASSERT_FALSE(report.ok());
    EXPECT_EQ(report.issues[0].code, IndexPageIssueCode::INVALID_INDEX_SIBLING);
}

TEST(IndexCorruptionErrorContractTest, IndexUuidMismatchReturnsPageCorrupt)
{
    IndexPageFixture fixture = buildValidIndexPage(PAGE_TYPE_BTREE_LEAF, 16u);
    const ID wrong_uuid = generateUuidV7();

    IndexPageDiagnosticReport report{};
    ErrorContext ctx{};
    const Status status = IndexPageDiagnostics::validatePage(
        fixture.page.data(), 8192u, PAGE_TYPE_BTREE_LEAF, 16u, &wrong_uuid, &report, &ctx);

    ASSERT_EQ(status, Status::PAGE_CORRUPT);
    ASSERT_FALSE(report.ok());
    EXPECT_EQ(report.issues[0].code, IndexPageIssueCode::INVALID_INDEX_UUID);
}

} // namespace scratchbird::core
