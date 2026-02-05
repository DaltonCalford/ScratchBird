/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Test for Issue 1.15: Tuple Header Alignment
 * Verifies that tuples are properly aligned to 8-byte boundaries per specification
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/page_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class TupleAlignmentTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_.path(), kPageSize, &ctx), Status::OK)
            << "Failed to create database: " << ctx.message;

        ASSERT_EQ(db_.open(db_file_.path(), &ctx), Status::OK)
            << "Failed to open database: " << ctx.message;
    }

    void TearDown() override
    {
        db_.close();
    }

    // Helper function to check if a value is aligned to 8 bytes
    static bool isAligned8(uint32_t offset)
    {
        return (offset % 8) == 0;
    }

    // Helper to create a tuple with specified payload size
    std::vector<uint8_t> createTuple(uint32_t payload_size, uint64_t xmin = 1)
    {
        uint32_t tuple_size = sizeof(TupleHeader) + payload_size;
        std::vector<uint8_t> tuple_data(tuple_size);
        auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
        hdr->xmin = xmin;
        hdr->xmax = 0;
        hdr->back_version_gpid = INVALID_GPID;
        hdr->back_version_slot = 0;
        hdr->ctid_gpid = INVALID_GPID;
        hdr->ctid_slot = 0;
        hdr->infomask = 0;
        hdr->null_bitmap_offset = 0;
        hdr->padding = 0;
        hdr->session_id = ID{};
        return tuple_data;
    }

    scratchbird::testing::TestDatabaseFile db_file_{"tuple_alignment_test"};
    Database db_;
};

TEST_F(TupleAlignmentTest, SingleTupleInsertionAlignment)
{
    ErrorContext ctx;

    uint32_t page_id;
    ASSERT_EQ(db_.page_manager()->allocatePage(page_id, &ctx), Status::OK)
        << "Failed to allocate page: " << ctx.message;

    void *page_data;
    ASSERT_EQ(db_.buffer_pool()->pinPage(page_id, &page_data, &ctx), Status::OK)
        << "Failed to pin page: " << ctx.message;

    HeapPage heap_page(reinterpret_cast<uint8_t *>(page_data), kPageSize);
    ASSERT_EQ(heap_page.initialize(page_id, &ctx), Status::OK)
        << "Failed to initialize heap page: " << ctx.message;

    // Create a tuple with odd size to test alignment
    auto tuple_data = createTuple(77); // 77 is not 8-byte aligned

    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple_data.data(), static_cast<uint32_t>(tuple_data.size()),
                                    1, &item_id, &ctx),
              Status::OK)
        << "Failed to insert tuple: " << ctx.message;

    // Get the tuple's offset
    auto *item_pointers = reinterpret_cast<ItemPointer *>(
        reinterpret_cast<uint8_t *>(page_data) + sizeof(PageHeader));

    uint32_t tuple_offset = item_pointers[item_id].offset;

    EXPECT_TRUE(isAligned8(tuple_offset))
        << "Tuple offset " << tuple_offset << " is NOT 8-byte aligned!";

    db_.buffer_pool()->unpinPage(page_id, false, &ctx);
}

TEST_F(TupleAlignmentTest, MultipleTuplesWithVaryingSizes)
{
    ErrorContext ctx;

    uint32_t page_id;
    ASSERT_EQ(db_.page_manager()->allocatePage(page_id, &ctx), Status::OK)
        << "Failed to allocate page: " << ctx.message;

    void *page_data;
    ASSERT_EQ(db_.buffer_pool()->pinPage(page_id, &page_data, &ctx), Status::OK)
        << "Failed to pin page: " << ctx.message;

    HeapPage heap_page(reinterpret_cast<uint8_t *>(page_data), kPageSize);
    ASSERT_EQ(heap_page.initialize(page_id, &ctx), Status::OK)
        << "Failed to initialize heap page: " << ctx.message;

    // Insert tuples with sizes that would naturally be misaligned
    const uint32_t test_sizes[] = {31, 63, 95, 127, 159, 191, 223, 255};
    std::vector<uint16_t> item_ids;

    for (uint32_t size : test_sizes)
    {
        auto tuple_data = createTuple(size);

        uint16_t item_id;
        if (heap_page.insertTuple(tuple_data.data(), static_cast<uint32_t>(tuple_data.size()),
                                  1, &item_id, &ctx) != Status::OK)
        {
            break; // Page might be full
        }
        item_ids.push_back(item_id);
    }

    // Verify all inserted tuples are 8-byte aligned
    auto *item_pointers = reinterpret_cast<ItemPointer *>(
        reinterpret_cast<uint8_t *>(page_data) + sizeof(PageHeader));

    uint32_t misaligned_count = 0;
    for (uint16_t item_id : item_ids)
    {
        uint32_t tuple_offset = item_pointers[item_id].offset;
        if (!isAligned8(tuple_offset))
        {
            ADD_FAILURE() << "Tuple " << item_id << " at offset " << tuple_offset
                          << " is NOT 8-byte aligned!";
            misaligned_count++;
        }
    }

    EXPECT_EQ(misaligned_count, 0u) << "Found " << misaligned_count << " misaligned tuples";
    EXPECT_GT(item_ids.size(), 0u) << "No tuples were inserted";

    db_.buffer_pool()->unpinPage(page_id, false, &ctx);
}

TEST_F(TupleAlignmentTest, AlignmentAfterDefragmentation)
{
    ErrorContext ctx;

    uint32_t page_id;
    ASSERT_EQ(db_.page_manager()->allocatePage(page_id, &ctx), Status::OK)
        << "Failed to allocate page: " << ctx.message;

    void *page_data;
    ASSERT_EQ(db_.buffer_pool()->pinPage(page_id, &page_data, &ctx), Status::OK)
        << "Failed to pin page: " << ctx.message;

    HeapPage heap_page(reinterpret_cast<uint8_t *>(page_data), kPageSize);
    ASSERT_EQ(heap_page.initialize(page_id, &ctx), Status::OK)
        << "Failed to initialize heap page: " << ctx.message;

    // Insert several tuples
    std::vector<uint16_t> item_ids;
    for (int i = 0; i < 10; i++)
    {
        uint32_t payload_size = 50 + (i * 7); // Varying sizes
        auto tuple_data = createTuple(payload_size);

        uint16_t item_id;
        if (heap_page.insertTuple(tuple_data.data(), static_cast<uint32_t>(tuple_data.size()),
                                  1, &item_id, &ctx) == Status::OK)
        {
            item_ids.push_back(item_id);
        }
    }

    ASSERT_GE(item_ids.size(), 4u) << "Need at least 4 tuples for deletion test";

    // Delete some tuples to create fragmentation
    ASSERT_EQ(heap_page.deleteTuple(item_ids[1], 2, &ctx), Status::OK)
        << "Failed to delete tuple: " << ctx.message;
    ASSERT_EQ(heap_page.deleteTuple(item_ids[3], 2, &ctx), Status::OK)
        << "Failed to delete tuple: " << ctx.message;

    // Trigger defragmentation
    uint32_t bytes_reclaimed = 0;
    ASSERT_EQ(heap_page.defragmentPage(&bytes_reclaimed, &ctx), Status::OK)
        << "Failed to defragment page: " << ctx.message;

    // Verify all remaining tuples are still 8-byte aligned
    auto *item_pointers = reinterpret_cast<ItemPointer *>(
        reinterpret_cast<uint8_t *>(page_data) + sizeof(PageHeader));

    uint32_t misaligned_count = 0;
    for (uint16_t item_id : item_ids)
    {
        if (item_id == item_ids[1] || item_id == item_ids[3])
            continue; // Skip deleted tuples

        uint32_t tuple_offset = item_pointers[item_id].offset;
        if (!isAligned8(tuple_offset))
        {
            ADD_FAILURE() << "After defragmentation, tuple " << item_id
                          << " at offset " << tuple_offset << " is NOT 8-byte aligned!";
            misaligned_count++;
        }
    }

    EXPECT_EQ(misaligned_count, 0u) << "Found " << misaligned_count << " misaligned tuples after defragmentation";

    db_.buffer_pool()->unpinPage(page_id, false, &ctx);
}

TEST_F(TupleAlignmentTest, StrictAlignmentVerification)
{
    ErrorContext ctx;

    uint32_t page_id;
    ASSERT_EQ(db_.page_manager()->allocatePage(page_id, &ctx), Status::OK)
        << "Failed to allocate page: " << ctx.message;

    void *page_data;
    ASSERT_EQ(db_.buffer_pool()->pinPage(page_id, &page_data, &ctx), Status::OK)
        << "Failed to pin page: " << ctx.message;

    HeapPage heap_page(reinterpret_cast<uint8_t *>(page_data), kPageSize);
    ASSERT_EQ(heap_page.initialize(page_id, &ctx), Status::OK)
        << "Failed to initialize heap page: " << ctx.message;

    // Insert a tuple with 64-bit test pattern
    uint32_t payload_size = 100;
    auto tuple_data = createTuple(payload_size);

    // Fill data with a test pattern (64-bit values)
    uint64_t *data_ptr = reinterpret_cast<uint64_t *>(tuple_data.data() + sizeof(TupleHeader));
    size_t num_values = payload_size / sizeof(uint64_t);
    for (size_t i = 0; i < num_values; i++)
    {
        data_ptr[i] = 0x123456789ABCDEF0ULL + i;
    }

    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple_data.data(), static_cast<uint32_t>(tuple_data.size()),
                                    1, &item_id, &ctx),
              Status::OK)
        << "Failed to insert tuple: " << ctx.message;

    // Read the tuple back
    auto *item_pointers = reinterpret_cast<ItemPointer *>(
        reinterpret_cast<uint8_t *>(page_data) + sizeof(PageHeader));

    uint32_t tuple_offset = item_pointers[item_id].offset;
    auto *stored_hdr = reinterpret_cast<TupleHeader *>(
        reinterpret_cast<uint8_t *>(page_data) + tuple_offset);

    // Verify tuple header is properly aligned for 64-bit access
    uintptr_t tuple_addr = reinterpret_cast<uintptr_t>(stored_hdr);
    EXPECT_EQ(tuple_addr % 8, 0u)
        << "TupleHeader address 0x" << std::hex << tuple_addr << " is NOT 8-byte aligned!";

    // Verify we can read 64-bit values from the data portion without issues
    uint64_t *stored_data = reinterpret_cast<uint64_t *>(
        reinterpret_cast<uint8_t *>(page_data) + tuple_offset + sizeof(TupleHeader));
    for (size_t i = 0; i < num_values; i++)
    {
        EXPECT_EQ(stored_data[i], 0x123456789ABCDEF0ULL + i)
            << "Data value " << i << " corrupted (alignment issue?)";
    }

    db_.buffer_pool()->unpinPage(page_id, false, &ctx);
}
