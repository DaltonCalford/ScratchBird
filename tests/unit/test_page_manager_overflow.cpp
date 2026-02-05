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
 * @file test_page_manager_overflow.cpp
 * @brief Test suite for PageManager overflow protection (Issue 1.7)
 *
 * Tests integer overflow protection in bitmap extension during file growth.
 * Verifies that overflow checks happen BEFORE calculations to prevent
 * undefined behavior.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <limits>

#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/status.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

class PageManagerOverflowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_overflow", ".sbrd");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 8192, &ctx), Status::OK)
            << "Database create failed: " << ctx.message;

        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK)
            << "Database open failed: " << ctx.message;

        page_manager_ = db_.page_manager();
        ASSERT_NE(page_manager_, nullptr) << "PageManager should not be null";
    }

    void TearDown() override
    {
        if (db_.is_open())
        {
            db_.close();
        }
        db_file_.reset();
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    Database db_;
    PageManager *page_manager_ = nullptr;
};

/**
 * Test: Verify normal extension works correctly
 *
 * Ensures that reasonable file extensions work without triggering overflow checks.
 */
TEST_F(PageManagerOverflowTest, NormalExtensionWorks)
{
    ErrorContext ctx;

    // Extend by reasonable amount (1000 pages = ~8MB)
    auto status = page_manager_->extendFile(1000, &ctx);
    EXPECT_EQ(status, Status::OK) << "Normal extension failed: " << ctx.message;

    // Verify total pages increased (initial pages + 1000)
    EXPECT_GE(page_manager_->totalPages(), 1000u) << "Total pages should have increased by at least 1000";
}

/**
 * Test: Maximum safe extension
 *
 * Verifies that extensions up to safe limits work correctly.
 */
TEST_F(PageManagerOverflowTest, MaximumSafeExtension)
{
    ErrorContext ctx;

    // Extend by maximum reasonable amount for testing (100K pages)
    // This tests the bitmap resize logic without triggering overflow
    auto status = page_manager_->extendFile(100000, &ctx);
    EXPECT_EQ(status, Status::OK) << "Large extension failed: " << ctx.message;

    EXPECT_GE(page_manager_->totalPages(), 100000u);
}

/**
 * Test: Detect overflow when total_pages + num_pages > UINT32_MAX
 *
 * Critical Test: Verifies the overflow check prevents undefined behavior.
 * The check happens BEFORE the addition, preventing overflow.
 */
TEST_F(PageManagerOverflowTest, DetectAdditionOverflow)
{
    ErrorContext ctx;

    // We can't actually set total_pages_ to near UINT32_MAX in practice,
    // but we can test the boundary condition logic by examining the code path.
    //
    // The fix ensures: if (num_pages > UINT32_MAX - total_pages_)
    // This is mathematically equivalent to: total_pages_ + num_pages > UINT32_MAX
    // but avoids the overflow by checking BEFORE the addition.

    // Test with very large num_pages that would clearly overflow uint32_t
    // Use UINT32_MAX to trigger overflow detection
    constexpr uint32_t huge_extension = UINT32_MAX - 1000;
    auto status = page_manager_->extendFile(huge_extension, &ctx);

    // Should fail with OOM before attempting the overflow
    EXPECT_NE(status, Status::OK) << "Should reject huge extension";
    EXPECT_EQ(status, Status::OOM) << "Should return OOM status";
    EXPECT_NE(ctx.message.find("addressable space"), std::string::npos)
        << "Error message should mention addressable space";
}

/**
 * Test: Detect overflow in bitmap size calculation (new_total + 7)
 *
 * Verifies the second overflow check prevents overflow when
 * calculating bitmap bytes: (new_total + 7) / 8
 */
TEST_F(PageManagerOverflowTest, DetectBitmapCalculationOverflow)
{
    ErrorContext ctx;

    // If new_total were UINT32_MAX - 5, then new_total + 7 would overflow
    // The check prevents this: new_total > (UINT32_MAX - 7)

    // Test with extension that would make new_total close to UINT32_MAX
    // This should be caught by the bitmap overflow check
    constexpr uint32_t near_max = UINT32_MAX - 10;
    auto status = page_manager_->extendFile(near_max, &ctx);

    EXPECT_NE(status, Status::OK) << "Should reject extension causing bitmap overflow";
    EXPECT_EQ(status, Status::OOM);
}

/**
 * Test: Boundary condition at UINT32_MAX - 7
 *
 * Tests the exact boundary where bitmap calculation would overflow.
 */
TEST_F(PageManagerOverflowTest, BitmapOverflowBoundary)
{
    ErrorContext ctx;

    // The check is: new_total > (UINT32_MAX - 7)
    // So new_total = UINT32_MAX - 6 should fail
    // Since initial total_pages_ is small, we need num_pages = UINT32_MAX - 9

    constexpr uint32_t boundary_extension = UINT32_MAX - 9;
    auto status = page_manager_->extendFile(boundary_extension, &ctx);

    EXPECT_EQ(status, Status::OOM) << "Should fail at boundary condition";
}

/**
 * Test: Multiple small extensions don't cause overflow
 *
 * Ensures that repeated small extensions work correctly and don't
 * accumulate to cause overflow unexpectedly.
 */
TEST_F(PageManagerOverflowTest, MultipleSmallExtensions)
{
    ErrorContext ctx;

    // Extend 100 times by 100 pages each = 10,000 pages total
    for (int i = 0; i < 100; i++)
    {
        auto status = page_manager_->extendFile(100, &ctx);
        ASSERT_EQ(status, Status::OK) << "Extension " << i << " failed: " << ctx.message;
    }

    EXPECT_GE(page_manager_->totalPages(), 10000u) << "Should have at least 10000 pages";
}

/**
 * Test: Zero extension is handled correctly
 *
 * Edge case: extending by 0 pages should be safe (no overflow possible).
 */
TEST_F(PageManagerOverflowTest, ZeroExtension)
{
    ErrorContext ctx;

    uint32_t initial_total = page_manager_->totalPages();

    auto status = page_manager_->extendFile(0, &ctx);
    EXPECT_EQ(status, Status::OK) << "Zero extension should succeed";

    EXPECT_EQ(page_manager_->totalPages(), initial_total)
        << "Total pages should not change";
}

/**
 * Test: Single page extension works
 *
 * Minimal extension case - should always work.
 */
TEST_F(PageManagerOverflowTest, SinglePageExtension)
{
    ErrorContext ctx;

    uint32_t initial_total = page_manager_->totalPages();

    auto status = page_manager_->extendFile(1, &ctx);
    EXPECT_EQ(status, Status::OK) << "Single page extension failed";

    EXPECT_EQ(page_manager_->totalPages(), initial_total + 1) << "Should have 1 more page";
}

/**
 * Test: Verify error messages are descriptive
 *
 * Ensures that overflow detection provides useful error messages for debugging.
 */
TEST_F(PageManagerOverflowTest, ErrorMessagesAreDescriptive)
{
    ErrorContext ctx;

    // Try to trigger overflow
    auto status = page_manager_->extendFile(UINT32_MAX - 100, &ctx);

    EXPECT_NE(status, Status::OK);
    EXPECT_FALSE(ctx.message.empty()) << "Error message should not be empty";

    std::string msg = ctx.message;
    EXPECT_TRUE(msg.find("addressable space") != std::string::npos ||
                msg.find("exceed") != std::string::npos)
        << "Error message should describe the overflow condition, got: " << msg;
}

/**
 * Test: Bitmap resize doesn't corrupt existing data
 *
 * Verifies that when bitmap is resized during extension, existing
 * allocation data is preserved.
 */
TEST_F(PageManagerOverflowTest, BitmapResizePreservesData)
{
    ErrorContext ctx;

    // Allocate some pages first
    uint32_t page_id1, page_id2, page_id3;
    ASSERT_EQ(page_manager_->allocatePage(page_id1, &ctx), Status::OK);
    ASSERT_EQ(page_manager_->allocatePage(page_id2, &ctx), Status::OK);
    ASSERT_EQ(page_manager_->allocatePage(page_id3, &ctx), Status::OK);

    // Verify pages are allocated
    EXPECT_TRUE(page_manager_->isAllocated(page_id1));
    EXPECT_TRUE(page_manager_->isAllocated(page_id2));
    EXPECT_TRUE(page_manager_->isAllocated(page_id3));

    // Extend file (triggers bitmap resize)
    ASSERT_EQ(page_manager_->extendFile(1000, &ctx), Status::OK);

    // Verify previously allocated pages are still marked as allocated
    EXPECT_TRUE(page_manager_->isAllocated(page_id1))
        << "Page " << page_id1 << " should still be allocated after resize";
    EXPECT_TRUE(page_manager_->isAllocated(page_id2))
        << "Page " << page_id2 << " should still be allocated after resize";
    EXPECT_TRUE(page_manager_->isAllocated(page_id3))
        << "Page " << page_id3 << " should still be allocated after resize";
}

/**
 * Test: Stress test with many extensions
 *
 * Performance and stability test with repeated extensions.
 */
TEST_F(PageManagerOverflowTest, StressTestManyExtensions)
{
    ErrorContext ctx;

    // Extend 1000 times by 10 pages each
    for (int i = 0; i < 1000; i++)
    {
        auto status = page_manager_->extendFile(10, &ctx);
        ASSERT_EQ(status, Status::OK) << "Extension " << i << " failed";
    }

    EXPECT_GE(page_manager_->totalPages(), 10000u) << "Should have at least 10000 pages";
}
