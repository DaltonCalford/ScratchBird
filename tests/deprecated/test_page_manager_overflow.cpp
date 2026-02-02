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

#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/status.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <cstdio>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

class PageManagerOverflowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary test database
        db_path_ = uniqueTestDbPath("test_overflow", ".sbrd");
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        // Use static create method
        ASSERT_EQ(Status::OK, Database::create(db_path_.c_str(), 8192, &ctx))
            << "Database create failed: " << ctx.message;

        // Open the database
        db_ = new Database();
        ASSERT_EQ(Status::OK, db_->open(db_path_.c_str(), &ctx))
            << "Database open failed: " << ctx.message;

        page_manager_ = db_->page_manager();
        ASSERT_NE(nullptr, page_manager_) << "PageManager should not be null";
    }

    void TearDown() override
    {
        if (db_)
        {
            ErrorContext ctx;
            db_->close(&ctx);
            delete db_;
        }
        std::remove(db_path_.c_str());
    }

    std::string db_path_;
    Database *db_ = nullptr;
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
    EXPECT_EQ(Status::OK, status) << "Normal extension failed: " << ctx.message;

    // Verify total pages increased
    EXPECT_EQ(1003u, page_manager_->totalPages()) << "Total pages should be 3 (initial) + 1000";
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
    EXPECT_EQ(Status::OK, status) << "Large extension failed: " << ctx.message;

    EXPECT_EQ(100003u, page_manager_->totalPages());
}

/**
 * Test: Detect overflow when total_pages + num_pages > SIZE_MAX
 *
 * Critical Test: Verifies the overflow check at line 246 prevents undefined behavior.
 * The check happens BEFORE the addition, preventing overflow.
 */
TEST_F(PageManagerOverflowTest, DetectAdditionOverflow)
{
    ErrorContext ctx;

    // We can't actually set total_pages_ to near SIZE_MAX in practice,
    // but we can test the boundary condition logic by examining the code path.
    //
    // The fix ensures: if (num_pages > SIZE_MAX - total_pages_)
    // This is mathematically equivalent to: total_pages_ + num_pages > SIZE_MAX
    // but avoids the overflow by checking BEFORE the addition.

    // Calculate a scenario that would overflow
    // If total_pages_ were SIZE_MAX - 100, and num_pages = 200,
    // then total_pages_ + num_pages would overflow
    // The new check catches this: 200 > SIZE_MAX - (SIZE_MAX - 100) = 100

    // We can't practically test with actual SIZE_MAX values due to memory constraints,
    // so we verify the logic is correct and test near-boundary conditions

    // Test with very large num_pages that would clearly overflow
    constexpr size_t huge_extension = SIZE_MAX - 1000;
    auto status = page_manager_->extendFile(huge_extension, &ctx);

    // Should fail with OOM before attempting the overflow
    EXPECT_NE(Status::OK, status) << "Should reject huge extension";
    EXPECT_EQ(Status::OOM, status) << "Should return OOM status";
    EXPECT_NE(std::string::npos, std::string(ctx.message).find("addressable space"))
        << "Error message should mention addressable space";
}

/**
 * Test: Detect overflow in bitmap size calculation (new_total + 7)
 *
 * Verifies the second overflow check at line 256 prevents overflow when
 * calculating bitmap bytes: (new_total + 7) / 8
 */
TEST_F(PageManagerOverflowTest, DetectBitmapCalculationOverflow)
{
    ErrorContext ctx;

    // If new_total were SIZE_MAX - 5, then new_total + 7 would overflow
    // The check prevents this: new_total > (SIZE_MAX - 7)

    // Test with extension that would make new_total close to SIZE_MAX
    // This should be caught by the bitmap overflow check
    constexpr size_t near_max = SIZE_MAX - 10;
    auto status = page_manager_->extendFile(near_max, &ctx);

    EXPECT_NE(Status::OK, status) << "Should reject extension causing bitmap overflow";
    EXPECT_EQ(Status::OOM, status);
}

/**
 * Test: Boundary condition at SIZE_MAX - 7
 *
 * Tests the exact boundary where bitmap calculation would overflow.
 */
TEST_F(PageManagerOverflowTest, BitmapOverflowBoundary)
{
    ErrorContext ctx;

    // The check is: new_total > (SIZE_MAX - 7)
    // So new_total = SIZE_MAX - 6 should fail
    // Since initial total_pages_ = 3, we need num_pages = SIZE_MAX - 9

    constexpr size_t boundary_extension = SIZE_MAX - 9;
    auto status = page_manager_->extendFile(boundary_extension, &ctx);

    EXPECT_EQ(Status::OOM, status) << "Should fail at boundary condition";
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
        ASSERT_EQ(Status::OK, status) << "Extension " << i << " failed: " << ctx.message;
    }

    EXPECT_EQ(10003u, page_manager_->totalPages()) << "Should have 3 + 10000 pages";
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
    EXPECT_EQ(Status::OK, status) << "Zero extension should succeed";

    EXPECT_EQ(initial_total, page_manager_->totalPages())
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

    auto status = page_manager_->extendFile(1, &ctx);
    EXPECT_EQ(Status::OK, status) << "Single page extension failed";

    EXPECT_EQ(4u, page_manager_->totalPages()) << "Should have 3 + 1 = 4 pages";
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
    auto status = page_manager_->extendFile(SIZE_MAX - 100, &ctx);

    EXPECT_NE(Status::OK, status);
    EXPECT_NE(nullptr, ctx.message);
    EXPECT_NE(0u, strlen(ctx.message)) << "Error message should not be empty";

    std::string msg(ctx.message);
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
    ASSERT_EQ(Status::OK, page_manager_->allocatePage(page_id1, &ctx));
    ASSERT_EQ(Status::OK, page_manager_->allocatePage(page_id2, &ctx));
    ASSERT_EQ(Status::OK, page_manager_->allocatePage(page_id3, &ctx));

    // Verify pages are allocated
    EXPECT_TRUE(page_manager_->isAllocated(page_id1));
    EXPECT_TRUE(page_manager_->isAllocated(page_id2));
    EXPECT_TRUE(page_manager_->isAllocated(page_id3));

    // Extend file (triggers bitmap resize)
    ASSERT_EQ(Status::OK, page_manager_->extendFile(1000, &ctx));

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
        ASSERT_EQ(Status::OK, status) << "Extension " << i << " failed";
    }

    EXPECT_EQ(10003u, page_manager_->totalPages()) << "Should have 3 + 10000 pages";
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
