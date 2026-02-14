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

auto makeIndexPage(uint32_t page_id, const ID &index_uuid, uint16_t page_type, uint16_t opaque_len)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> page(8192u, 0u);
    auto *header = reinterpret_cast<PageHeader *>(page.data());
    header->magic = K_MAGIC_SBRD;
    header->version = 1u;
    header->page_type = page_type;
    header->page_size = static_cast<uint32_t>(page.size());
    header->flags = PAGE_FLAG_CHECKSUM_VALID;
    header->page_id = page_id;
    header->free_offset = sizeof(PageHeader);
    header->item_count = 0u;
    header->free_space = static_cast<uint16_t>(page.size() - sizeof(PageHeader));
    pageSetSpecial(*header, page.size() - sizeof(IndexPageHeader) - opaque_len);

    auto *idx = reinterpret_cast<IndexPageHeader *>(page.data() + pageSpecial(*header));
    idx->page_level = 0u;
    idx->flags = static_cast<uint16_t>(INDEX_PAGE_FLAG_ROOT | INDEX_PAGE_FLAG_LEFTMOST |
                                       INDEX_PAGE_FLAG_RIGHTMOST);
    idx->left_sibling = 0u;
    idx->right_sibling = 0u;
    idx->opaque_len = opaque_len;
    idx->reserved = 0u;
    setIndexPageHeaderUuid(*idx, index_uuid);

    header->checksum = calculatePageChecksum(page.data(), static_cast<uint32_t>(page.size()));
    return page;
}

} // namespace

TEST(IndexPageWalkConformanceTest, WalkPassesWhenAllPagesAreValid)
{
    const ID index_uuid = generateUuidV7();
    std::vector<std::vector<uint8_t>> owned_pages{};
    owned_pages.push_back(makeIndexPage(10u, index_uuid, PAGE_TYPE_BTREE_LEAF, 16u));
    owned_pages.push_back(makeIndexPage(11u, index_uuid, PAGE_TYPE_BTREE_LEAF, 16u));
    owned_pages.push_back(makeIndexPage(12u, index_uuid, PAGE_TYPE_BTREE_LEAF, 16u));

    std::vector<const uint8_t *> pages{};
    pages.reserve(owned_pages.size());
    for (const auto &p : owned_pages)
    {
        pages.push_back(p.data());
    }

    IndexPageWalkReport report{};
    ErrorContext ctx{};
    const Status status = IndexPageDiagnostics::walkPages(pages, 8192u, PAGE_TYPE_BTREE_LEAF, 16u,
                                                          &index_uuid, &report, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(report.ok());
    EXPECT_EQ(report.pages_ok, 3u);
    EXPECT_EQ(report.pages_failed, 0u);
    ASSERT_EQ(report.entries.size(), 3u);
    EXPECT_EQ(report.entries[0].page_id, 10u);
    EXPECT_EQ(report.entries[1].page_id, 11u);
    EXPECT_EQ(report.entries[2].page_id, 12u);
}

TEST(IndexPageWalkConformanceTest, WalkReportsCorruptPageAndContinues)
{
    const ID index_uuid = generateUuidV7();
    std::vector<std::vector<uint8_t>> owned_pages{};
    owned_pages.push_back(makeIndexPage(20u, index_uuid, PAGE_TYPE_BTREE_LEAF, 16u));
    owned_pages.push_back(makeIndexPage(21u, index_uuid, PAGE_TYPE_BTREE_LEAF, 16u));
    owned_pages.push_back(makeIndexPage(22u, index_uuid, PAGE_TYPE_BTREE_LEAF, 16u));

    // Corrupt one page checksum.
    owned_pages[1][400] ^= 0x7Fu;

    std::vector<const uint8_t *> pages{};
    pages.reserve(owned_pages.size());
    for (const auto &p : owned_pages)
    {
        pages.push_back(p.data());
    }

    IndexPageWalkReport report{};
    ErrorContext ctx{};
    const Status status = IndexPageDiagnostics::walkPages(pages, 8192u, PAGE_TYPE_BTREE_LEAF, 16u,
                                                          &index_uuid, &report, &ctx);

    EXPECT_EQ(status, Status::CHECKSUM_MISMATCH);
    EXPECT_FALSE(report.ok());
    EXPECT_EQ(report.pages_ok, 2u);
    EXPECT_EQ(report.pages_failed, 1u);
    ASSERT_EQ(report.entries.size(), 3u);
    EXPECT_EQ(report.entries[0].status, Status::OK);
    EXPECT_EQ(report.entries[1].status, Status::CHECKSUM_MISMATCH);
    EXPECT_EQ(report.entries[2].status, Status::OK);
}

} // namespace scratchbird::core
