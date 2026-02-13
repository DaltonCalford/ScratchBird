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
#include <cstdint>
#include <array>
#include <vector>
#include <cstring>
#include <random>

#include "scratchbird/version.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

static void fill_page(std::vector<uint8_t> &page, uint32_t page_size, uint32_t page_id,
                      uint16_t page_type)
{
    ASSERT_GE(page_size, 64u);
    page.assign(page_size, 0);
    auto *header = reinterpret_cast<PageHeader *>(page.data());
    header->magic = K_MAGIC_SBRD;
    header->version = 1;
    header->page_type = page_type;
    header->page_size = page_size;
    header->lsn = 0;
    header->page_id = page_id;
    header->flags = 0;
    header->generation = 1;
    pageSetLower(*header, sizeof(PageHeader));
    pageSetUpper(*header, page_size);
    pageSetSpecial(*header, page_size);
    // Compute checksum
    header->checksum = calculatePageChecksum(page.data(), page_size);
}

TEST(Version, VersionStringPrefix)
{
    std::string vs = SCRATCHBIRD_VERSION_STRING;
    ASSERT_NE(vs.find("ScratchBird v"), std::string::npos);
}

TEST(OnDiskFormat, AlphaPageSizes)
{
    // Stage 0 page sizes
    EXPECT_TRUE(isValidAlphaPageSize(8192));
    EXPECT_TRUE(isValidAlphaPageSize(16384));
    EXPECT_TRUE(isValidAlphaPageSize(32768));

    // Stage 1.1 extended page sizes (now valid)
    EXPECT_TRUE(isValidAlphaPageSize(65536));
    EXPECT_TRUE(isValidAlphaPageSize(131072));

    // Invalid page sizes
    EXPECT_FALSE(isValidAlphaPageSize(0));
    EXPECT_FALSE(isValidAlphaPageSize(4096));
    EXPECT_FALSE(isValidAlphaPageSize(262144));
}

TEST(OnDiskFormat, HeaderLayoutAndChecksum)
{
    for (uint32_t size : {8192u, 16384u, 32768u, 65536u, 131072u})
    {
        std::vector<uint8_t> page;
        fill_page(page, size, 0, PAGE_TYPE_DATABASE_HEADER);
        auto *header = reinterpret_cast<const PageHeader *>(page.data());
        // Magic and basic fields
        EXPECT_EQ(header->magic, K_MAGIC_SBRD);
        EXPECT_EQ(header->version, 1);
        EXPECT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
        EXPECT_EQ(header->page_size, size);
        EXPECT_EQ(header->page_id, 0u);
        // Checksum must validate
        EXPECT_TRUE(validatePageChecksum(page.data(), size));
        // Tamper then expect mismatch
        page[64] ^= 0xFF;
        EXPECT_FALSE(validatePageChecksum(page.data(), size));
    }
}

TEST(CRC32C, KnownVectors)
{
    // Empty input
    uint32_t crc = crc32cCompute(nullptr, 0, 0xFFFFFFFFu) ^ 0xFFFFFFFFu;
    EXPECT_EQ(crc, 0x00000000u);
    // "123456789" standard CRC32C = 0xE3069283
    const uint8_t msg[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint32_t c = 0xFFFFFFFFu;
    c = crc32cCompute(msg, sizeof(msg), c);
    c ^= 0xFFFFFFFFu;
    EXPECT_EQ(c, 0xE3069283u);
}

TEST(UUIDv7, StructureAndMonotonicity)
{
    const int N = 1000;
    std::array<uint8_t, 16> prev{};
    for (int i = 0; i < N; ++i)
    {
        auto u = generateUuidV7();
        // Version (bits 48..51) == 0b0111 in byte[6] high nibble
        EXPECT_EQ((u.bytes[6] >> 4) & 0x0F, 0x07);
        // Variant 0b10xxxxxx in byte[8] bits 6..7
        EXPECT_EQ((u.bytes[8] >> 6) & 0x03, 0x02);
        if (i > 0)
        {
            // Compare timestamp portion bytes[0..5] lexicographically non-decreasing
            for (int b = 0; b < 6; ++b)
            {
                if (u.bytes[b] != prev[b])
                {
                    EXPECT_GE(u.bytes[b], prev[b]);
                    break;
                }
            }
        }
        prev = u.bytes;
    }
}
