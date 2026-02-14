/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <cstddef>
#include <cstring>

#include <gtest/gtest.h>

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{

TEST(IndexPageBaseLayoutContractTest, IndexPageHeaderLayoutAndOffsetsAreCanonical)
{
    EXPECT_EQ(sizeof(IndexPageHeader), 32u);
    EXPECT_EQ(offsetof(IndexPageHeader, index_uuid), 0u);
    EXPECT_EQ(offsetof(IndexPageHeader, page_level), 16u);
    EXPECT_EQ(offsetof(IndexPageHeader, flags), 18u);
    EXPECT_EQ(offsetof(IndexPageHeader, right_sibling), 20u);
    EXPECT_EQ(offsetof(IndexPageHeader, left_sibling), 24u);
    EXPECT_EQ(offsetof(IndexPageHeader, opaque_len), 28u);
    EXPECT_EQ(offsetof(IndexPageHeader, reserved), 30u);
}

TEST(IndexPageBaseLayoutContractTest, FlagValidationRejectsReservedBits)
{
    EXPECT_TRUE(isValidIndexPageFlags(INDEX_PAGE_FLAG_ROOT));
    EXPECT_TRUE(isValidIndexPageFlags(static_cast<uint16_t>(INDEX_PAGE_FLAG_LEFTMOST |
                                                            INDEX_PAGE_FLAG_RIGHTMOST)));
    EXPECT_FALSE(isValidIndexPageFlags(0x0010u));
    EXPECT_FALSE(isValidIndexPageFlags(0x8000u));
}

TEST(IndexPageBaseLayoutContractTest, HeaderValidationEnforcesOpaqueAndReservedContract)
{
    IndexPageHeader header{};
    header.page_level = 0u;
    header.flags = static_cast<uint16_t>(INDEX_PAGE_FLAG_ROOT | INDEX_PAGE_FLAG_LEFTMOST |
                                         INDEX_PAGE_FLAG_RIGHTMOST);
    header.right_sibling = 0u;
    header.left_sibling = 0u;
    header.opaque_len = 24u;
    header.reserved = 0u;

    EXPECT_TRUE(isValidIndexPageHeaderBasic(header, 24u));

    header.opaque_len = 20u;
    EXPECT_FALSE(isValidIndexPageHeaderBasic(header, 24u));

    header.opaque_len = 24u;
    header.reserved = 1u;
    EXPECT_FALSE(isValidIndexPageHeaderBasic(header, 24u));
}

TEST(IndexPageBaseLayoutContractTest, UuidRoundTripIsStable)
{
    IndexPageHeader header{};
    const ID index_uuid = generateUuidV7();
    setIndexPageHeaderUuid(header, index_uuid);

    const ID roundtrip = getIndexPageHeaderUuid(header);
    EXPECT_EQ(0, std::memcmp(index_uuid.bytes.data(), roundtrip.bytes.data(), 16));
}

} // namespace scratchbird::core
