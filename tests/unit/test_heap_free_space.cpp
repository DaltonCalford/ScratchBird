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
 * @file test_heap_free_space.cpp
 * @brief GoogleTest suite for Heap Page free space management (Issue 1.12)
 *
 * Tests that free space checks properly include ItemPointer size and
 * verifies boundary conditions for heap page operations.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/ondisk.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

class HeapFreeSpaceTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_heap_free_space", ".sbrd");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), kPageSize, &ctx), Status::OK)
            << "Database create failed: " << ctx.message;

        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK)
            << "Database open failed: " << ctx.message;

        // Allocate a heap page
        ASSERT_EQ(db_.page_manager()->allocatePage(page_id_, &ctx), Status::OK)
            << "Page allocation failed: " << ctx.message;

        // Pin the page
        void *page_data = nullptr;
        ASSERT_EQ(db_.buffer_pool()->pinPage(page_id_, &page_data, &ctx), Status::OK)
            << "Page pin failed: " << ctx.message;

        page_buffer_ = static_cast<uint8_t *>(page_data);

        // Initialize heap page
        heap_page_ = std::make_unique<HeapPage>(page_buffer_, kPageSize);
        ASSERT_EQ(heap_page_->initialize(page_id_, &ctx), Status::OK)
            << "Heap page initialization failed: " << ctx.message;
    }

    void TearDown() override
    {
        if (heap_page_ && page_buffer_)
        {
            ErrorContext ctx;
            db_.buffer_pool()->unpinPage(page_id_, false, &ctx);
        }
        heap_page_.reset();
        if (db_.is_open())
        {
            db_.close();
        }
        db_file_.reset();
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    Database db_;
    uint32_t page_id_ = 0;
    uint8_t *page_buffer_ = nullptr;
    std::unique_ptr<HeapPage> heap_page_;
};

/**
 * Test: Verify initial free space calculation is correct
 *
 * Expected initial free space = page_size - PageHeader - HeapPageSpecial
 */
TEST_F(HeapFreeSpaceTest, InitialFreeSpaceCalculation)
{
    uint32_t initial_free_space = heap_page_->getFreeSpace();

    // Expected initial free space:
    // page_size - PageHeader - HeapPageSpecial
    uint32_t expected_initial = kPageSize - sizeof(PageHeader) - sizeof(HeapPageSpecial);

    EXPECT_EQ(initial_free_space, expected_initial)
        << "Initial free space mismatch: got " << initial_free_space
        << ", expected " << expected_initial;
}

/**
 * Test: Verify free space decreases correctly after tuple insertion
 *
 * Free space should decrease by: tuple_size + sizeof(ItemPointer)
 */
TEST_F(HeapFreeSpaceTest, FreeSpaceDecreaseAfterInsert)
{
    uint32_t initial_free_space = heap_page_->getFreeSpace();

    // Create a small tuple
    uint32_t tuple_size = sizeof(TupleHeader) + 100;
    std::vector<uint8_t> tuple_data(tuple_size);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
    hdr->xmin = 1;
    hdr->xmax = 0;
    hdr->back_version_gpid = 0;
    hdr->back_version_slot = 0;
    hdr->infomask = 0;

    // Insert tuple
    uint16_t item_id;
    ErrorContext ctx;
    ASSERT_EQ(heap_page_->insertTuple(tuple_data.data(), tuple_size, 1, &item_id, &ctx), Status::OK)
        << "Tuple insertion failed: " << ctx.message;

    // Get free space after insertion
    uint32_t free_space_after = heap_page_->getFreeSpace();

    // Free space should have decreased by: tuple_size + sizeof(ItemPointer)
    uint32_t expected_decrease = tuple_size + sizeof(ItemPointer);
    uint32_t actual_decrease = initial_free_space - free_space_after;

    EXPECT_EQ(actual_decrease, expected_decrease)
        << "Free space decrease mismatch:\n"
        << "  Initial: " << initial_free_space << "\n"
        << "  After: " << free_space_after << "\n"
        << "  Tuple size: " << tuple_size << "\n"
        << "  ItemPointer size: " << sizeof(ItemPointer) << "\n"
        << "  Expected decrease: " << expected_decrease << "\n"
        << "  Actual decrease: " << actual_decrease;
}

/**
 * Test: Fill page to capacity and verify overflow protection
 *
 * Verifies that page correctly detects full condition and stops inserting
 * before corruption occurs.
 */
TEST_F(HeapFreeSpaceTest, FillPageToCapacity)
{
    // Create small tuples for filling
    uint32_t tuple_size = sizeof(TupleHeader) + 64;
    std::vector<uint8_t> tuple_data(tuple_size);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
    hdr->xmin = 1;
    hdr->xmax = 0;
    hdr->back_version_gpid = 0;
    hdr->back_version_slot = 0;
    hdr->infomask = 0;

    uint32_t tuples_inserted = 0;
    uint16_t item_id;
    ErrorContext ctx;

    // Insert tuples until page is full
    while (heap_page_->insertTuple(tuple_data.data(), tuple_size, 1, &item_id, &ctx) == Status::OK)
    {
        tuples_inserted++;
        ASSERT_LT(tuples_inserted, 1000u) << "Infinite loop detected - page should have filled up";
    }

    // Last insertion should have failed with PAGE_FULL
    EXPECT_EQ(ctx.code, Status::PAGE_FULL)
        << "Expected PAGE_FULL status, got: " << static_cast<int>(ctx.code);

    // Verify page boundaries are sane
    auto *page_hdr = reinterpret_cast<PageHeader *>(page_buffer_);
    auto *special = reinterpret_cast<HeapPageSpecial *>(
        page_buffer_ + kPageSize - sizeof(HeapPageSpecial));

    EXPECT_GE(special->pd_upper, special->pd_lower)
        << "Page corruption detected: pd_upper < pd_lower";

    uint32_t remaining_space = special->pd_upper - special->pd_lower;

    // Should have less than (tuple_size + ItemPointer) remaining
    EXPECT_LT(remaining_space, tuple_size + sizeof(ItemPointer))
        << "Should not have space for another tuple\n"
        << "  Remaining: " << remaining_space << "\n"
        << "  Needed: " << (tuple_size + sizeof(ItemPointer));

    // Verify we inserted a reasonable number of tuples
    EXPECT_GT(tuples_inserted, 0u) << "Should have inserted at least one tuple";
    EXPECT_LT(tuples_inserted, 1000u) << "Should have stopped before safety limit";
}

/**
 * Test: Verify ItemPointer is included in free space check
 *
 * This test verifies the fix for Issue 1.12 - the free space check
 * correctly includes sizeof(ItemPointer) when needed.
 */
TEST_F(HeapFreeSpaceTest, ItemPointerIncludedInFreeSpaceCheck)
{
    uint32_t initial_free_space = heap_page_->getFreeSpace();

    // Insert a tuple and verify space accounting
    uint32_t tuple_size = sizeof(TupleHeader) + 50;
    std::vector<uint8_t> tuple_data(tuple_size);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
    hdr->xmin = 1;
    hdr->xmax = 0;
    hdr->back_version_gpid = 0;
    hdr->back_version_slot = 0;
    hdr->infomask = 0;

    uint16_t item_id;
    ErrorContext ctx;
    ASSERT_EQ(heap_page_->insertTuple(tuple_data.data(), tuple_size, 1, &item_id, &ctx), Status::OK);

    uint32_t free_space_after = heap_page_->getFreeSpace();
    uint32_t space_used = initial_free_space - free_space_after;

    // Verify that space_used includes both tuple and ItemPointer
    EXPECT_EQ(space_used, tuple_size + sizeof(ItemPointer))
        << "Space calculation should include ItemPointer size\n"
        << "  Space used: " << space_used << "\n"
        << "  Tuple size: " << tuple_size << "\n"
        << "  ItemPointer size: " << sizeof(ItemPointer) << "\n"
        << "  Expected total: " << (tuple_size + sizeof(ItemPointer));
}

/**
 * Test: Multiple tuple insertions with varying sizes
 */
TEST_F(HeapFreeSpaceTest, MultipleTupleSizes)
{
    std::vector<size_t> tuple_sizes = {10, 50, 100, 200, 500};
    std::vector<uint16_t> item_ids;
    ErrorContext ctx;

    for (size_t body_size : tuple_sizes)
    {
        uint32_t tuple_size = sizeof(TupleHeader) + body_size;
        std::vector<uint8_t> tuple_data(tuple_size);
        auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
        hdr->xmin = 1;
        hdr->xmax = 0;
        hdr->back_version_gpid = 0;
        hdr->back_version_slot = 0;
        hdr->infomask = 0;

        // Fill with pattern data
        for (size_t i = 0; i < body_size; i++)
        {
            tuple_data[sizeof(TupleHeader) + i] = static_cast<uint8_t>((i + body_size) & 0xFF);
        }

        uint16_t item_id;
        Status status = heap_page_->insertTuple(tuple_data.data(), tuple_size, 1, &item_id, &ctx);
        if (status != Status::OK)
        {
            // Page is full, that's ok for this test
            break;
        }
        item_ids.push_back(item_id);
    }

    // Verify all inserted tuples can be retrieved
    for (size_t i = 0; i < item_ids.size(); i++)
    {
        const uint8_t *data;
        uint32_t size;
        EXPECT_EQ(heap_page_->getTuple(item_ids[i], &data, &size, &ctx), Status::OK)
            << "Failed to retrieve tuple " << i;
    }
}
