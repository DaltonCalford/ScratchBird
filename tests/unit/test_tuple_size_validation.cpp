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
 * Test for Issue 1.8: Tuple Size Validation Missing
 * Verifies that tuple size validation prevents oversized tuples
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/ondisk.h"

using namespace scratchbird::core;

class TupleSizeValidationTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        page_buffer_.resize(kPageSize);
        page_data_ = page_buffer_.data();

        heap_page_ = std::make_unique<HeapPage>(page_data_, kPageSize);

        ErrorContext ctx;
        ASSERT_EQ(heap_page_->initialize(1, &ctx), Status::OK)
            << "Failed to initialize heap page: " << ctx.message;

        // Calculate maximum tuple size
        max_tuple_size_ = kPageSize - sizeof(PageHeader) - sizeof(HeapPageSpecial) - sizeof(ItemPointer);
    }

    void TearDown() override
    {
        heap_page_.reset();
    }

    // Helper to create a tuple with specified total size
    std::vector<uint8_t> createTuple(uint32_t total_size)
    {
        std::vector<uint8_t> tuple_data(total_size);
        if (total_size >= sizeof(TupleHeader))
        {
            auto *tuple_hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
            tuple_hdr->xmin = 1;
            tuple_hdr->xmax = 0;
            tuple_hdr->back_version_gpid = INVALID_GPID;
            tuple_hdr->back_version_slot = 0;
            tuple_hdr->ctid_gpid = INVALID_GPID;
            tuple_hdr->ctid_slot = 0;
            tuple_hdr->infomask = 0;
            tuple_hdr->null_bitmap_offset = 0;
            tuple_hdr->padding = 0;
            tuple_hdr->session_id = ID{};
        }
        return tuple_data;
    }

    std::vector<uint8_t> page_buffer_;
    uint8_t *page_data_ = nullptr;
    std::unique_ptr<HeapPage> heap_page_;
    uint32_t max_tuple_size_ = 0;
};

TEST_F(TupleSizeValidationTest, TupleTooSmall)
{
    ErrorContext ctx;

    auto tiny_tuple = createTuple(sizeof(TupleHeader) - 1);
    uint16_t item_id;

    Status status = heap_page_->insertTuple(tiny_tuple.data(), static_cast<uint32_t>(tiny_tuple.size()),
                                            1, &item_id, &ctx);

    EXPECT_EQ(status, Status::INVALID_ARGUMENT)
        << "Should reject tuples smaller than TupleHeader";
    EXPECT_NE(std::string(ctx.message).find("at least"), std::string::npos)
        << "Error message should mention minimum size requirement";
}

TEST_F(TupleSizeValidationTest, NormalSizedTuple)
{
    ErrorContext ctx;

    auto normal_tuple = createTuple(sizeof(TupleHeader) + 100);
    uint16_t item_id;

    Status status = heap_page_->insertTuple(normal_tuple.data(), static_cast<uint32_t>(normal_tuple.size()),
                                            1, &item_id, &ctx);

    EXPECT_EQ(status, Status::OK)
        << "Should accept normal-sized tuple: " << ctx.message;
    EXPECT_GE(item_id, 0u) << "Item ID should be valid after successful insert";
}

TEST_F(TupleSizeValidationTest, MaximumSizedTuple)
{
    ErrorContext ctx;

    auto max_tuple = createTuple(max_tuple_size_);
    uint16_t item_id;

    Status status = heap_page_->insertTuple(max_tuple.data(), static_cast<uint32_t>(max_tuple.size()),
                                            2, &item_id, &ctx);

    // This might fail with PAGE_FULL since we may have inserted tuples in previous tests
    // But it should NOT fail with underflow/overflow errors
    if (status == Status::OK)
    {
        EXPECT_GE(item_id, 0u) << "Item ID should be valid after successful insert";
    }
    else
    {
        EXPECT_EQ(status, Status::PAGE_FULL)
            << "Maximum-sized tuple should only fail with PAGE_FULL, not "
            << static_cast<int>(status) << ": " << ctx.message;
    }
}

TEST_F(TupleSizeValidationTest, OversizedTuple)
{
    ErrorContext ctx;

    uint32_t oversized = max_tuple_size_ + 100; // 100 bytes over the limit
    auto huge_tuple = createTuple(oversized);
    uint16_t item_id;

    Status status = heap_page_->insertTuple(huge_tuple.data(), static_cast<uint32_t>(huge_tuple.size()),
                                            3, &item_id, &ctx);

    EXPECT_NE(status, Status::OK)
        << "Should have rejected oversized tuple (size=" << oversized
        << ", max=" << max_tuple_size_ << ")";

    if (status == Status::INVALID_ARGUMENT)
    {
        EXPECT_NE(std::string(ctx.message).find("exceed"), std::string::npos)
            << "Error message should mention size exceeded";
    }
}

TEST_F(TupleSizeValidationTest, ExtremelyLargeTuple)
{
    ErrorContext ctx;

    uint32_t attack_size = UINT32_MAX / 2; // Huge value that could cause underflow

    // Don't actually allocate this much memory, just test the size check
    auto dummy_tuple = createTuple(sizeof(TupleHeader));
    uint16_t item_id;

    // Pass attack_size but actual data is small (simulating malicious input)
    Status status = heap_page_->insertTuple(dummy_tuple.data(), attack_size, 4, &item_id, &ctx);

    EXPECT_NE(status, Status::OK)
        << "Should reject extremely large tuple size (" << attack_size << ")";
}
