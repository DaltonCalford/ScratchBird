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
 * @file test_overflow_fix.cpp
 * @brief GoogleTest suite for PageManager overflow protection (Issue 1.7)
 *
 * Tests integer overflow protection in bitmap extension during file growth.
 * Verifies that overflow checks happen BEFORE calculations to prevent
 * undefined behavior.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <cstring>

#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/database.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

class OverflowFixTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 8192;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_overflow", ".sbrd");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), kPageSize, &ctx), Status::OK)
            << "Database create failed: " << ctx.message;

        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK)
            << "Database open failed: " << ctx.message;

        page_manager_ = db_.page_manager();
        ASSERT_NE(page_manager_, nullptr) << "PageManager should not be null";

        initial_pages_ = page_manager_->totalPages();
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
    uint32_t initial_pages_ = 0;
};

/**
 * Test: Normal extension works correctly
 *
 * Ensures that reasonable file extensions work without triggering overflow checks.
 */
TEST_F(OverflowFixTest, NormalExtensionWorks)
{
    ErrorContext ctx;

    // Extend by reasonable amount (1000 pages = ~8MB)
    ASSERT_EQ(page_manager_->extendFile(1000, &ctx), Status::OK)
        << "Normal extension failed: " << ctx.message;

    // Verify total pages increased (initial pages + 1000)
    EXPECT_GE(page_manager_->totalPages(), initial_pages_ + 1000)
        << "Total pages should have increased by at least 1000";
}

/**
 * Test: Overflow detection for huge extensions
 *
 * Verifies that extensions that would overflow are properly rejected.
 */
TEST_F(OverflowFixTest, HugeExtensionRejected)
{
    ErrorContext ctx;

    // Try to extend by UINT32_MAX - 1000 (should trigger overflow check)
    constexpr uint32_t huge_extension = UINT32_MAX - 1000;
    Status status = page_manager_->extendFile(huge_extension, &ctx);

    // Should fail
    EXPECT_NE(status, Status::OK)
        << "Should have rejected huge extension that would cause overflow";

    EXPECT_EQ(status, Status::OOM)
        << "Should return OOM status, got: " << static_cast<int>(status);

    // Verify error message mentions addressable space or exceed
    EXPECT_TRUE(ctx.message.find("addressable space") != std::string::npos ||
                ctx.message.find("exceed") != std::string::npos)
        << "Error message should mention overflow/addressable space, got: " << ctx.message;
}

/**
 * Test: Multiple small extensions work correctly
 *
 * Verifies that repeated small extensions don't accumulate to cause
 * unexpected overflow.
 */
TEST_F(OverflowFixTest, MultipleSmallExtensions)
{
    ErrorContext ctx;

    // Extend 100 times by 100 pages each = 10,000 pages total
    for (int i = 0; i < 100; i++)
    {
        ASSERT_EQ(page_manager_->extendFile(100, &ctx), Status::OK)
            << "Extension " << i << " failed: " << ctx.message;
    }

    // Verify total pages
    uint32_t expected_pages = initial_pages_ + 10000;
    EXPECT_EQ(page_manager_->totalPages(), expected_pages)
        << "Total pages should be " << expected_pages << " after 100 extensions";
}

/**
 * Test: Zero extension is handled correctly
 *
 * Edge case: extending by 0 pages should be safe (no overflow possible).
 */
TEST_F(OverflowFixTest, ZeroExtension)
{
    ErrorContext ctx;

    uint32_t initial_total = page_manager_->totalPages();

    ASSERT_EQ(page_manager_->extendFile(0, &ctx), Status::OK)
        << "Zero extension should succeed";

    EXPECT_EQ(page_manager_->totalPages(), initial_total)
        << "Total pages should not change with zero extension";
}

/**
 * Test: Single page extension works
 *
 * Minimal extension case - should always work.
 */
TEST_F(OverflowFixTest, SinglePageExtension)
{
    ErrorContext ctx;

    uint32_t initial_total = page_manager_->totalPages();

    ASSERT_EQ(page_manager_->extendFile(1, &ctx), Status::OK)
        << "Single page extension failed";

    EXPECT_EQ(page_manager_->totalPages(), initial_total + 1)
        << "Should have exactly 1 more page";
}

/**
 * Test: Large but safe extension
 *
 * Tests extension by a large number that doesn't overflow.
 */
TEST_F(OverflowFixTest, LargeSafeExtension)
{
    ErrorContext ctx;

    // Extend by 10,000 pages (should be safe)
    ASSERT_EQ(page_manager_->extendFile(10000, &ctx), Status::OK)
        << "Large safe extension failed: " << ctx.message;

    EXPECT_EQ(page_manager_->totalPages(), initial_pages_ + 10000)
        << "Should have exactly 10,000 more pages";
}

/**
 * Test: Extension preserves existing allocations
 *
 * Verifies that extending the file doesn't corrupt existing page allocations.
 */
TEST_F(OverflowFixTest, ExtensionPreservesAllocations)
{
    ErrorContext ctx;

    // Allocate some pages first
    uint32_t page_id1, page_id2, page_id3;
    ASSERT_EQ(page_manager_->allocatePage(page_id1, &ctx), Status::OK)
        << "Failed to allocate page 1";
    ASSERT_EQ(page_manager_->allocatePage(page_id2, &ctx), Status::OK)
        << "Failed to allocate page 2";
    ASSERT_EQ(page_manager_->allocatePage(page_id3, &ctx), Status::OK)
        << "Failed to allocate page 3";

    // Verify pages are allocated
    EXPECT_TRUE(page_manager_->isAllocated(page_id1))
        << "Page 1 should be allocated";
    EXPECT_TRUE(page_manager_->isAllocated(page_id2))
        << "Page 2 should be allocated";
    EXPECT_TRUE(page_manager_->isAllocated(page_id3))
        << "Page 3 should be allocated";

    // Extend file (triggers bitmap resize)
    ASSERT_EQ(page_manager_->extendFile(1000, &ctx), Status::OK)
        << "Extension failed: " << ctx.message;

    // Verify previously allocated pages are still marked as allocated
    EXPECT_TRUE(page_manager_->isAllocated(page_id1))
        << "Page 1 should still be allocated after resize";
    EXPECT_TRUE(page_manager_->isAllocated(page_id2))
        << "Page 2 should still be allocated after resize";
    EXPECT_TRUE(page_manager_->isAllocated(page_id3))
        << "Page 3 should still be allocated after resize";
}

/**
 * Test: Error messages are descriptive for overflow
 *
 * Ensures that overflow detection provides useful error messages for debugging.
 */
TEST_F(OverflowFixTest, ErrorMessagesAreDescriptive)
{
    ErrorContext ctx;

    // Try to trigger overflow
    Status status = page_manager_->extendFile(std::numeric_limits<uint32_t>::max() - 100, &ctx);

    EXPECT_NE(status, Status::OK)
        << "Should have failed with huge extension";
    EXPECT_FALSE(ctx.message.empty())
        << "Error message should not be empty";

    EXPECT_TRUE(ctx.message.find("addressable space") != std::string::npos ||
                ctx.message.find("exceed") != std::string::npos ||
                ctx.message.find("overflow") != std::string::npos)
        << "Error message should describe the overflow condition, got: " << ctx.message;
}

/**
 * Test: Boundary condition near overflow
 *
 * Tests the exact boundary where overflow protection should trigger.
 */
TEST_F(OverflowFixTest, BoundaryCondition)
{
    ErrorContext ctx;

    // Test with a value that would be just at the edge of causing overflow
    // The protection should trigger before any actual overflow occurs
    constexpr uint32_t near_max = std::numeric_limits<uint32_t>::max() / 2;

    // This should fail (too large) but not crash or cause undefined behavior
    Status status = page_manager_->extendFile(near_max, &ctx);

    // We expect this to fail due to OOM or similar, not crash
    // The exact behavior depends on system memory, but it shouldn't crash
    if (status != Status::OK)
    {
        // Expected - the extension was rejected
        EXPECT_TRUE(status == Status::OOM || status == Status::IO_ERROR)
            << "Should return OOM or IO_ERROR for huge extension, got: "
            << static_cast<int>(status);
    }
    else
    {
        // If it succeeded, verify the pages were actually added
        EXPECT_GE(page_manager_->totalPages(), near_max)
            << "If extension succeeded, total pages should reflect it";
    }
}
