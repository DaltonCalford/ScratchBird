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
#include <vector>

#include <gtest/gtest.h>

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{

namespace
{

auto makeCanonicalPage(uint16_t page_type, const ID &database_uuid, const ID &object_uuid)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> page(8192u, 0u);
    auto *header = reinterpret_cast<PageHeader *>(page.data());
    header->magic = K_MAGIC_SBRD;
    header->version = 1u;
    header->page_type = page_type;
    header->page_size = static_cast<uint32_t>(page.size());
    header->flags = PAGE_FLAG_CHECKSUM_VALID;
    header->page_id = 1u;
    header->lsn = 0u;
    header->generation = 1u;
    pageSetLower(*header, sizeof(PageHeader));
    pageSetUpper(*header, static_cast<uint32_t>(page.size()));
    pageSetSpecial(*header, static_cast<uint32_t>(page.size()));
    setDatabaseUuid(*header, database_uuid);
    setObjectUuid(*header, object_uuid);
    header->checksum = calculatePageChecksum(page.data(), static_cast<uint32_t>(page.size()));
    return page;
}

} // namespace

TEST(PageContractStageS1Test, CanonicalPageTypeEnumValuesMatchSpecification)
{
    EXPECT_EQ(PAGE_TYPE_DATABASE_HEADER, 0x0000);
    EXPECT_EQ(PAGE_TYPE_FILESPACE_HEADER, 0x000F);
    EXPECT_EQ(PAGE_TYPE_BTREE_META, 0x0100);
    EXPECT_EQ(PAGE_TYPE_GIN_PENDING, 0x0123);
    EXPECT_EQ(PAGE_TYPE_BRIN_DATA, 0x0152);
    EXPECT_EQ(PAGE_TYPE_COLUMNSTORE_META, 0x0200);
    EXPECT_EQ(PAGE_TYPE_LSM_FILTER, 0x0213);
    EXPECT_EQ(PAGE_TYPE_HNSW_META, 0x0300);
    EXPECT_EQ(PAGE_TYPE_VECTOR_FLAT_SEGMENT, 0x0371);
    EXPECT_EQ(PAGE_TYPE_DOC_META, 0x0400);
    EXPECT_EQ(PAGE_TYPE_REDIS_GEO, 0x0458);
}

TEST(PageContractStageS1Test, PageHeaderLayoutOffsetsAreCanonical)
{
    EXPECT_EQ(sizeof(PageHeader), 80u);
    EXPECT_EQ(offsetof(PageHeader, magic), 0u);
    EXPECT_EQ(offsetof(PageHeader, version), 4u);
    EXPECT_EQ(offsetof(PageHeader, page_type), 6u);
    EXPECT_EQ(offsetof(PageHeader, page_size), 8u);
    EXPECT_EQ(offsetof(PageHeader, checksum), 12u);
    EXPECT_EQ(offsetof(PageHeader, lsn), 16u);
    EXPECT_EQ(offsetof(PageHeader, page_id), 24u);
    EXPECT_EQ(offsetof(PageHeader, flags), 28u);
    EXPECT_EQ(offsetof(PageHeader, database_uuid), 32u);
    EXPECT_EQ(offsetof(PageHeader, object_uuid), 48u);
    EXPECT_EQ(offsetof(PageHeader, generation), 64u);
    EXPECT_EQ(offsetof(PageHeader, free_space), 72u);
    EXPECT_EQ(offsetof(PageHeader, item_count), 74u);
    EXPECT_EQ(offsetof(PageHeader, free_offset), 76u);
    EXPECT_EQ(offsetof(PageHeader, special_size), 78u);
}

TEST(PageContractStageS1Test, ChecksumValidationFollowsChecksumFlagSemantics)
{
    const ID database_uuid = generateUuidV7();
    const ID object_uuid = generateUuidV7();
    std::vector<uint8_t> page = makeCanonicalPage(PAGE_TYPE_HEAP, database_uuid, object_uuid);

    EXPECT_EQ(validatePageContract(page.data(), 8192u, PAGE_TYPE_HEAP, &database_uuid,
                                   &object_uuid),
              Status::OK);

    page[256] ^= 0xAAu;
    EXPECT_EQ(validatePageContract(page.data(), 8192u, PAGE_TYPE_HEAP, &database_uuid,
                                   &object_uuid),
              Status::CHECKSUM_MISMATCH);

    auto *header = reinterpret_cast<PageHeader *>(page.data());
    header->flags &= ~PAGE_FLAG_CHECKSUM_VALID;
    EXPECT_EQ(validatePageContract(page.data(), 8192u, PAGE_TYPE_HEAP, &database_uuid,
                                   &object_uuid),
              Status::OK);
}

TEST(PageContractStageS1Test, HeaderValidationEnforcesTypeAndUuidContracts)
{
    const ID database_uuid = generateUuidV7();
    const ID object_uuid = generateUuidV7();
    std::vector<uint8_t> page = makeCanonicalPage(PAGE_TYPE_HEAP, database_uuid, object_uuid);

    EXPECT_EQ(validatePageContract(page.data(), 8192u, PAGE_TYPE_HEAP, &database_uuid,
                                   &object_uuid),
              Status::OK);

    const ID wrong_object_uuid = generateUuidV7();
    EXPECT_EQ(validatePageContract(page.data(), 8192u, PAGE_TYPE_HEAP, &database_uuid,
                                   &wrong_object_uuid),
              Status::PAGE_CORRUPT);

    EXPECT_EQ(validatePageContract(page.data(), 8192u, PAGE_TYPE_BTREE_LEAF, &database_uuid,
                                   &object_uuid),
              Status::PAGE_CORRUPT);
}

TEST(PageContractStageS1Test, HeaderValidationRejectsInvalidPageBoundaries)
{
    const ID database_uuid = generateUuidV7();
    const ID object_uuid = generateUuidV7();
    std::vector<uint8_t> page = makeCanonicalPage(PAGE_TYPE_HEAP, database_uuid, object_uuid);

    auto *header = reinterpret_cast<PageHeader *>(page.data());
    header->free_offset = static_cast<uint16_t>(header->page_size);
    EXPECT_EQ(validatePageContract(page.data(), 8192u, PAGE_TYPE_HEAP, &database_uuid,
                                   &object_uuid),
              Status::PAGE_CORRUPT);
}

} // namespace scratchbird::core
