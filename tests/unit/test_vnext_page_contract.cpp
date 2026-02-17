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

#include <gtest/gtest.h>

#include "scratchbird/core/ondisk.h"

namespace scratchbird::core
{

TEST(VNextPageContractTest, PageTypeEnumValuesMatchSpecification)
{
    EXPECT_EQ(PAGE_TYPE_DOC_COLLECTION_ROOT, 0x2000);
    EXPECT_EQ(PAGE_TYPE_DOC_HEAP, 0x2001);
    EXPECT_EQ(PAGE_TYPE_DOC_PATH_DICTIONARY, 0x2002);
    EXPECT_EQ(PAGE_TYPE_DOC_PATH_POSTINGS, 0x2003);
    EXPECT_EQ(PAGE_TYPE_TS_MEASUREMENT_ROOT, 0x2004);
    EXPECT_EQ(PAGE_TYPE_TS_SERIES_INDEX, 0x2005);
    EXPECT_EQ(PAGE_TYPE_TS_CHUNK, 0x2006);
    EXPECT_EQ(PAGE_TYPE_TS_AGG_CACHE, 0x2007);
    EXPECT_EQ(PAGE_TYPE_COL_SEGMENT_HEADER, 0x2008);
    EXPECT_EQ(PAGE_TYPE_COL_SEGMENT_DATA, 0x2009);
    EXPECT_EQ(PAGE_TYPE_COL_ZONE_MAP, 0x200A);
    EXPECT_EQ(PAGE_TYPE_COL_DICTIONARY, 0x200B);
    EXPECT_EQ(PAGE_TYPE_SEARCH_TERM_DICT, 0x200C);
    EXPECT_EQ(PAGE_TYPE_SEARCH_POSTINGS, 0x200D);
    EXPECT_EQ(PAGE_TYPE_SEARCH_DOCVALUES, 0x200E);
    EXPECT_EQ(PAGE_TYPE_VECTOR_GRAPH, 0x200F);
    EXPECT_EQ(PAGE_TYPE_VECTOR_QUANTIZER, 0x2010);
    EXPECT_EQ(PAGE_TYPE_VECTOR_POSTING, 0x2011);
    EXPECT_EQ(PAGE_TYPE_LSM_RUN_MANIFEST, 0x2012);
    EXPECT_EQ(PAGE_TYPE_LSM_RUN_DATA, 0x2013);
    EXPECT_EQ(PAGE_TYPE_LSM_BLOOM, 0x2014);
    EXPECT_EQ(PAGE_TYPE_RETENTION_MANIFEST, 0x2015);
}

TEST(VNextPageContractTest, BaseHeaderLayoutMatchesVNextContract)
{
    EXPECT_EQ(sizeof(VNextBasePageHeader), 64u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, magic), 0u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, page_type), 4u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, layout_version), 6u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, page_id), 8u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, object_id), 12u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, logical_epoch), 16u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, writer_txid), 24u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, slot_count), 32u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, payload_start), 34u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, free_start), 36u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, free_end), 38u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, page_flags), 40u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, reserved0), 42u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, header_crc32c), 44u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, page_crc32c), 48u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, reserved1), 52u);
    EXPECT_EQ(offsetof(VNextBasePageHeader, reserved2), 56u);
}

TEST(VNextPageContractTest, ExtensionHeaderLayoutMatchesVNextContract)
{
    EXPECT_EQ(sizeof(VNextExtensionHeader), 32u);
    EXPECT_EQ(offsetof(VNextExtensionHeader, ext_version), 0u);
    EXPECT_EQ(offsetof(VNextExtensionHeader, ext_flags), 2u);
    EXPECT_EQ(offsetof(VNextExtensionHeader, min_visible_txid), 4u);
    EXPECT_EQ(offsetof(VNextExtensionHeader, max_visible_txid), 12u);
    EXPECT_EQ(offsetof(VNextExtensionHeader, owner_txid), 20u);
    EXPECT_EQ(offsetof(VNextExtensionHeader, payload_crc32c), 28u);
}

TEST(VNextPageContractTest, UnknownReservedTypeIsRejected)
{
    EXPECT_EQ(validateVNextPageTypeKnown(0x20AA), Status::PAGE_CORRUPT);
    EXPECT_EQ(validateVNextPageTypeKnown(PAGE_TYPE_DOC_COLLECTION_ROOT), Status::OK);
    EXPECT_EQ(validateVNextPageTypeKnown(PAGE_TYPE_HEAP), Status::OK);
}

TEST(VNextPageContractTest, HeaderBoundValidationIsDeterministic)
{
    VNextBasePageHeader header{};
    header.magic = K_MAGIC_SBPG;
    header.page_type = PAGE_TYPE_DOC_COLLECTION_ROOT;
    header.layout_version = 1u;
    header.payload_start = VNEXT_PAYLOAD_REGION_START;
    header.free_start = VNEXT_PAYLOAD_REGION_START;
    header.free_end = static_cast<uint16_t>(VNEXT_PAGE_SIZE_BYTES);

    EXPECT_TRUE(isValidVNextPageHeaderBounds(header));

    header.payload_start = 0x50u;
    EXPECT_FALSE(isValidVNextPageHeaderBounds(header));
    header.payload_start = VNEXT_PAYLOAD_REGION_START;

    header.reserved1 = 1u;
    EXPECT_FALSE(isValidVNextPageHeaderBounds(header));
}

TEST(VNextPageContractTest, VisibilityRangeValidationContract)
{
    VNextExtensionHeader ext{};
    EXPECT_TRUE(isValidVNextVisibilityRange(ext));

    ext.min_visible_txid = 100u;
    ext.max_visible_txid = 99u;
    EXPECT_FALSE(isValidVNextVisibilityRange(ext));

    ext.max_visible_txid = 100u;
    EXPECT_TRUE(isValidVNextVisibilityRange(ext));
}

} // namespace scratchbird::core

