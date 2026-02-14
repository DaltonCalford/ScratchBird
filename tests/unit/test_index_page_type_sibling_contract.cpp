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

#include "scratchbird/core/ondisk.h"

namespace scratchbird::core
{

TEST(IndexPageTypeAndSiblingContractTest, CanonicalIndexPageTypesAreAccepted)
{
    EXPECT_TRUE(isCanonicalIndexPageType(PAGE_TYPE_BTREE_LEAF));
    EXPECT_TRUE(isCanonicalIndexPageType(PAGE_TYPE_GIN_ENTRY));
    EXPECT_TRUE(isCanonicalIndexPageType(PAGE_TYPE_BRIN_DATA));
    EXPECT_TRUE(isCanonicalIndexPageType(PAGE_TYPE_COLUMNSTORE_SEGMENT));
    EXPECT_TRUE(isCanonicalIndexPageType(PAGE_TYPE_LSM_INDEX));
    EXPECT_TRUE(isCanonicalIndexPageType(PAGE_TYPE_HNSW_NODE));
    EXPECT_TRUE(isCanonicalIndexPageType(PAGE_TYPE_VECTOR_FLAT_SEGMENT));
}

TEST(IndexPageTypeAndSiblingContractTest, NonIndexPageTypesAreRejected)
{
    EXPECT_FALSE(isCanonicalIndexPageType(PAGE_TYPE_HEAP));
    EXPECT_FALSE(isCanonicalIndexPageType(PAGE_TYPE_TOAST_CHUNK));
    EXPECT_FALSE(isCanonicalIndexPageType(PAGE_TYPE_DOC_DATA));
    EXPECT_FALSE(isCanonicalIndexPageType(PAGE_TYPE_REDIS_LIST));
}

TEST(IndexPageTypeAndSiblingContractTest, SiblingContractAcceptsValidBoundaryPatterns)
{
    IndexPageHeader root{};
    root.flags = static_cast<uint16_t>(INDEX_PAGE_FLAG_ROOT | INDEX_PAGE_FLAG_LEFTMOST |
                                       INDEX_PAGE_FLAG_RIGHTMOST);
    root.left_sibling = 0u;
    root.right_sibling = 0u;
    EXPECT_TRUE(isValidIndexSiblingContract(root));

    IndexPageHeader leftmost{};
    leftmost.flags = INDEX_PAGE_FLAG_LEFTMOST;
    leftmost.left_sibling = 0u;
    leftmost.right_sibling = 42u;
    EXPECT_TRUE(isValidIndexSiblingContract(leftmost));

    IndexPageHeader rightmost{};
    rightmost.flags = INDEX_PAGE_FLAG_RIGHTMOST;
    rightmost.left_sibling = 21u;
    rightmost.right_sibling = 0u;
    EXPECT_TRUE(isValidIndexSiblingContract(rightmost));
}

TEST(IndexPageTypeAndSiblingContractTest, SiblingContractRejectsInvalidCombinations)
{
    IndexPageHeader invalid_rightmost{};
    invalid_rightmost.flags = INDEX_PAGE_FLAG_RIGHTMOST;
    invalid_rightmost.left_sibling = 10u;
    invalid_rightmost.right_sibling = 11u;
    EXPECT_FALSE(isValidIndexSiblingContract(invalid_rightmost));

    IndexPageHeader missing_leftmost_flag{};
    missing_leftmost_flag.flags = 0u;
    missing_leftmost_flag.left_sibling = 0u;
    missing_leftmost_flag.right_sibling = 77u;
    EXPECT_FALSE(isValidIndexSiblingContract(missing_leftmost_flag));

    IndexPageHeader bad_root{};
    bad_root.flags = INDEX_PAGE_FLAG_ROOT;
    bad_root.left_sibling = 0u;
    bad_root.right_sibling = 0u;
    EXPECT_FALSE(isValidIndexSiblingContract(bad_root));
}

TEST(IndexPageTypeAndSiblingContractTest, HeaderValidationCombinesTypeAndSiblingRules)
{
    PageHeader page{};
    page.page_type = PAGE_TYPE_GIN_ENTRY;

    IndexPageHeader idx{};
    idx.flags = INDEX_PAGE_FLAG_LEFTMOST;
    idx.left_sibling = 0u;
    idx.right_sibling = 9u;
    idx.opaque_len = 12u;
    idx.reserved = 0u;

    EXPECT_TRUE(isValidIndexPageHeaderForType(page, idx, 12u));
    EXPECT_FALSE(isValidIndexPageHeaderForType(page, idx, 16u));

    page.page_type = PAGE_TYPE_HEAP;
    EXPECT_FALSE(isValidIndexPageHeaderForType(page, idx, 12u));
}

} // namespace scratchbird::core
