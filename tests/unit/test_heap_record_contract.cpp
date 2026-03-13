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
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include <cstring>
#include <vector>

using namespace scratchbird::core;

namespace
{

auto isZeroId(const ID &id) -> bool
{
    for (uint8_t b : id.bytes)
    {
        if (b != 0)
        {
            return false;
        }
    }
    return true;
}

} // namespace

class HeapRecordContractTest : public ::testing::Test
{
protected:
    static constexpr uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page_buffer_;

    void SetUp() override
    {
        page_buffer_.assign(PAGE_SIZE, 0);
    }

    auto buildTuple(uint32_t payload_len, uint64_t xmin) const -> std::vector<uint8_t>
    {
        TupleHeader hdr{};
        hdr.xmin = xmin;
        hdr.xmax = 0;
        hdr.back_version_gpid = INVALID_GPID;
        hdr.back_version_slot = 0;
        hdr.reserved1 = 0;
        hdr.ctid_gpid = INVALID_GPID;
        hdr.ctid_slot = 0;
        hdr.infomask = 0;
        hdr.null_bitmap_offset = 0;
        hdr.padding = 0;
        hdr.session_id = ID{};
        hdr.row_uuid = ID{};
        hdr.record_flags = 0;
        hdr.record_format = 0;
        hdr.payload_len = 0;

        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_len, 0);
        std::memcpy(tuple.data(), &hdr, sizeof(TupleHeader));
        for (uint32_t i = 0; i < payload_len; ++i)
        {
            tuple[sizeof(TupleHeader) + i] = static_cast<uint8_t>((i + 11u) & 0xFFu);
        }
        return tuple;
    }
};

TEST_F(HeapRecordContractTest, InsertPopulatesCanonicalRecordHeader)
{
    ErrorContext ctx;
    HeapPage page(page_buffer_.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    std::vector<uint8_t> tuple = buildTuple(96, 1001);
    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()),
                               1001, &item_id, &ctx),
              Status::OK);

    const uint8_t *stored = nullptr;
    uint32_t stored_size = 0;
    ASSERT_EQ(page.getTuple(item_id, &stored, &stored_size, &ctx), Status::OK);
    ASSERT_GE(stored_size, sizeof(TupleHeader));

    const auto *hdr = reinterpret_cast<const TupleHeader *>(stored);
    EXPECT_EQ(hdr->getCreateTxid(), 1001u);
    EXPECT_EQ(hdr->getDeleteTxid(), 0u);
    EXPECT_FALSE(isZeroId(hdr->row_uuid));
    EXPECT_EQ(hdr->record_format, TupleHeader::RECORD_FORMAT_V1);
    EXPECT_EQ(hdr->payload_len, 96u);
    EXPECT_FALSE(hdr->hasRecordFlag(TupleHeader::RHD_DELETED));
    EXPECT_FALSE(hdr->hasRecordFlag(TupleHeader::RHD_CHAINED));
    EXPECT_FALSE(hdr->hasRecordFlag(TupleHeader::RHD_MOVED));
    EXPECT_FALSE(hdr->hasRecordFlag(TupleHeader::RHD_TOAST_PTR));
}

TEST_F(HeapRecordContractTest, UpdatePreservesRowUuidAcrossVersionChain)
{
    ErrorContext ctx;
    HeapPage page(page_buffer_.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(2, &ctx), Status::OK);

    std::vector<uint8_t> tuple_v1 = buildTuple(80, 1100);
    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple_v1.data(), static_cast<uint32_t>(tuple_v1.size()),
                               1100, &item_id, &ctx),
              Status::OK);

    const uint8_t *v1_data = nullptr;
    uint32_t v1_size = 0;
    ASSERT_EQ(page.getTuple(item_id, &v1_data, &v1_size, &ctx), Status::OK);
    const auto *v1_hdr = reinterpret_cast<const TupleHeader *>(v1_data);
    ASSERT_FALSE(isZeroId(v1_hdr->row_uuid));
    const ID original_row_uuid = v1_hdr->row_uuid;

    std::vector<uint8_t> tuple_v2 = buildTuple(104, 1200);
    uint16_t updated_item_id = 0;
    ASSERT_EQ(page.updateTuple(item_id, tuple_v2.data(), static_cast<uint32_t>(tuple_v2.size()),
                               1199, 1200, &updated_item_id, &ctx),
              Status::OK);
    EXPECT_EQ(updated_item_id, item_id);

    const uint8_t *v2_data = nullptr;
    uint32_t v2_size = 0;
    ASSERT_EQ(page.getTuple(updated_item_id, &v2_data, &v2_size, &ctx), Status::OK);
    const auto *v2_hdr = reinterpret_cast<const TupleHeader *>(v2_data);
    EXPECT_EQ(v2_hdr->row_uuid, original_row_uuid);
    EXPECT_EQ(v2_hdr->payload_len, 104u);
    EXPECT_FALSE(v2_hdr->hasRecordFlag(TupleHeader::RHD_CHAINED));

    ASSERT_NE(v2_hdr->back_version_gpid, INVALID_GPID);
    ASSERT_NE(v2_hdr->back_version_slot, 0u);
    const uint8_t *back_ptr = nullptr;
    uint32_t back_size = 0;
    ASSERT_EQ(page.getTuple(v2_hdr->back_version_slot, &back_ptr, &back_size, &ctx), Status::OK);
    const auto *back_hdr = reinterpret_cast<const TupleHeader *>(back_ptr);
    EXPECT_EQ(back_hdr->row_uuid, original_row_uuid);
    EXPECT_TRUE(back_hdr->hasRecordFlag(TupleHeader::RHD_CHAINED));
    EXPECT_EQ(back_hdr->payload_len, 80u);
}

TEST_F(HeapRecordContractTest, DeleteMarksCanonicalDeletedFlagOnTupleHeader)
{
    ErrorContext ctx;
    HeapPage page(page_buffer_.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(3, &ctx), Status::OK);

    std::vector<uint8_t> tuple = buildTuple(48, 1300);
    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()),
                               1300, &item_id, &ctx),
              Status::OK);

    ASSERT_EQ(page.deleteTuple(item_id, 1400, &ctx), Status::OK);

    const auto *pg_hdr = page.header();
    const auto *items =
        reinterpret_cast<const ItemPointer *>(page_buffer_.data() + sizeof(PageHeader));
    ASSERT_LT(item_id, static_cast<uint16_t>((pageLower(*pg_hdr) - sizeof(PageHeader)) / sizeof(ItemPointer)));
    EXPECT_TRUE(items[item_id].isDeleted());

    const auto *tuple_hdr =
        reinterpret_cast<const TupleHeader *>(page_buffer_.data() + items[item_id].offset);
    EXPECT_EQ(tuple_hdr->xmax, 1400u);
    EXPECT_TRUE(tuple_hdr->hasRecordFlag(TupleHeader::RHD_DELETED));
    EXPECT_EQ(tuple_hdr->payload_len, 48u);
}

TEST_F(HeapRecordContractTest, SystemColumnExtractionReturnsRowUuidAndCreateTxidForLiveTuple)
{
    ErrorContext ctx;
    HeapPage page(page_buffer_.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(4, &ctx), Status::OK);

    std::vector<uint8_t> tuple = buildTuple(40, 1500);
    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()),
                               1500, &item_id, &ctx),
              Status::OK);

    const uint8_t *stored = nullptr;
    uint32_t stored_size = 0;
    ASSERT_EQ(page.getTuple(item_id, &stored, &stored_size, &ctx), Status::OK);

    ID row_uuid{};
    uint64_t last_edit_txid = 0;
    ASSERT_EQ(HeapPage::extractSystemColumns(stored, stored_size, &row_uuid, &last_edit_txid, &ctx),
              Status::OK);
    EXPECT_FALSE(isZeroId(row_uuid));
    EXPECT_EQ(last_edit_txid, 1500u);
}

TEST_F(HeapRecordContractTest, SystemColumnExtractionReturnsDeleteTxidForTombstone)
{
    ErrorContext ctx;
    HeapPage page(page_buffer_.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(5, &ctx), Status::OK);

    std::vector<uint8_t> tuple = buildTuple(32, 1600);
    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()),
                               1600, &item_id, &ctx),
              Status::OK);
    ASSERT_EQ(page.deleteTuple(item_id, 1700, &ctx), Status::OK);

    const auto *pg_hdr = page.header();
    const auto *items =
        reinterpret_cast<const ItemPointer *>(page_buffer_.data() + sizeof(PageHeader));
    ASSERT_LT(item_id, static_cast<uint16_t>((pageLower(*pg_hdr) - sizeof(PageHeader)) / sizeof(ItemPointer)));

    const uint8_t *raw_tombstone = page_buffer_.data() + items[item_id].offset;
    uint32_t raw_tombstone_size = items[item_id].length;

    ID row_uuid{};
    uint64_t last_edit_txid = 0;
    ASSERT_EQ(HeapPage::extractSystemColumns(raw_tombstone, raw_tombstone_size,
                                             &row_uuid, &last_edit_txid, &ctx),
              Status::OK);
    EXPECT_FALSE(isZeroId(row_uuid));
    EXPECT_EQ(last_edit_txid, 1700u);
}
