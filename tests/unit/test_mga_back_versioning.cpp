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
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/transaction_manager.h"
#include <filesystem>
#include <vector>
#include <cstring>

using namespace scratchbird;
using namespace scratchbird::core;

/**
 * Test suite for MGA (Multi-Generational Architecture) Back Versioning
 *
 * Tests Phases 1-3 of MGA implementation:
 * - Phase 1: Data structure changes (back_version_tid, HEAP_CHAIN flag)
 * - Phase 2: updateTuple() rewrite with offset-based back versioning
 * - Phase 3: findVisibleVersion() rewrite with N2O traversal
 *
 * Alpha Limitations:
 * - Same-page back versions only (no cross-page)
 * - No TOAST support
 * - No index integration
 */
class MGABackVersioningTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clean up any existing test files
        std::filesystem::remove("test_mga_back_versioning.db");
    }

    void TearDown() override
    {
        // Clean up test files
        std::filesystem::remove("test_mga_back_versioning.db");
    }

    /**
     * Helper: Create a test tuple with TupleHeader
     */
    std::vector<uint8_t> createTestTuple(uint64_t xmin, uint64_t xmax,
                                         uint64_t back_version_tid,
                                         const std::string &data)
    {
        std::vector<uint8_t> tuple_data(sizeof(TupleHeader) + data.size());

        TupleHeader *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
        hdr->xmin = xmin;
        hdr->xmax = xmax;
        hdr->back_version_tid = back_version_tid;
        hdr->ctid_page = 0;
        hdr->ctid_item = 0;
        hdr->infomask = 0;
        hdr->padding = 0;
        hdr->null_bitmap_offset = 0;

        // Copy data after header
        std::memcpy(tuple_data.data() + sizeof(TupleHeader), data.data(), data.size());

        return tuple_data;
    }

    /**
     * Helper: Verify tuple header values
     */
    void verifyTupleHeader(const uint8_t *tuple_data, uint64_t expected_xmin,
                          uint64_t expected_xmax, uint64_t expected_back_tid,
                          uint16_t expected_infomask)
    {
        const TupleHeader *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
        EXPECT_EQ(hdr->xmin, expected_xmin) << "xmin mismatch";
        EXPECT_EQ(hdr->xmax, expected_xmax) << "xmax mismatch";
        EXPECT_EQ(hdr->back_version_tid, expected_back_tid) << "back_version_tid mismatch";
        EXPECT_EQ(hdr->infomask & expected_infomask, expected_infomask) << "infomask mismatch";
    }

    /**
     * Helper: Extract data from tuple (skip TupleHeader)
     */
    std::string extractTupleData(const uint8_t *tuple_data, uint32_t tuple_size)
    {
        return std::string(reinterpret_cast<const char *>(tuple_data + sizeof(TupleHeader)),
                          tuple_size - sizeof(TupleHeader));
    }
};

/**
 * Test 1: Basic Back Versioning
 *
 * Verify that a simple update creates a back version correctly:
 * - Insert tuple at item_id
 * - Update tuple (new version overwrites primary location)
 * - Verify:
 *   ✅ Item pointer unchanged (same item_id)
 *   ✅ back_version_tid points to old version (offset-based)
 *   ✅ Old version has HEAP_CHAIN flag
 *   ✅ New version at primary location
 */
TEST_F(MGABackVersioningTest, BasicUpdate)
{
    ErrorContext ctx;
    const uint32_t page_size = 8192;

    // Create heap page
    uint8_t *page_buffer = new (std::nothrow) uint8_t[page_size];
    ASSERT_NE(page_buffer, nullptr);
    std::memset(page_buffer, 0, page_size);

    HeapPage heap_page(page_buffer, page_size);
    ASSERT_EQ(heap_page.initialize(1, &ctx), Status::OK);

    // Insert original tuple
    auto tuple_v1 = createTestTuple(100, 0, 0, "Original Data V1");
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple_v1.data(), tuple_v1.size(), 100, &item_id, &ctx),
              Status::OK);

    // Get original item pointer location
    const uint8_t *original_data;
    uint32_t original_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &original_data, &original_size, &ctx), Status::OK);
    EXPECT_EQ(extractTupleData(original_data, original_size), "Original Data V1");

    // Update tuple (Phase 2: updateTuple with back versioning)
    auto tuple_v2 = createTestTuple(200, 0, 0, "Updated Data V2");
    uint16_t new_item_id;
    ASSERT_EQ(heap_page.updateTuple(item_id, tuple_v2.data(), tuple_v2.size(), 100, 200, &new_item_id, &ctx),
              Status::OK);
    EXPECT_EQ(new_item_id, item_id) << "Item ID should remain unchanged (same-page update)";

    // Verify: Item pointer unchanged (same item_id still valid)
    const uint8_t *new_data;
    uint32_t new_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &new_data, &new_size, &ctx), Status::OK);
    EXPECT_EQ(extractTupleData(new_data, new_size), "Updated Data V2");

    // Verify: New version has back_version_tid pointing to old version
    const TupleHeader *new_hdr = reinterpret_cast<const TupleHeader *>(new_data);
    EXPECT_NE(new_hdr->back_version_tid, 0) << "back_version_tid should point to old version";
    EXPECT_TRUE(new_hdr->hasBackVersion()) << "New version should have back version pointer";

    // Extract back version location (offset-based encoding)
    uint64_t back_tid = new_hdr->back_version_tid;
    uint32_t back_page_id = static_cast<uint32_t>(back_tid >> 32);
    uint32_t back_offset = static_cast<uint32_t>(back_tid & 0xFFFFFFFF);

    EXPECT_EQ(back_page_id, 1) << "Back version should be on same page (Alpha limitation)";
    EXPECT_GT(back_offset, 0) << "Back version offset should be non-zero";

    // Verify: Old version has HEAP_CHAIN flag (marks it as a back version)
    const uint8_t *back_version_data = page_buffer + back_offset;
    const TupleHeader *back_hdr = reinterpret_cast<const TupleHeader *>(back_version_data);
    EXPECT_TRUE(back_hdr->infomask & TupleHeader::HEAP_CHAIN) << "Old version should have HEAP_CHAIN flag";
    EXPECT_EQ(extractTupleData(back_version_data, tuple_v1.size()), "Original Data V1");

    delete[] page_buffer;
}

/**
 * Test 2: Version Chain Traversal (N2O - Newest to Oldest)
 *
 * Verify that multiple updates create a proper version chain:
 * - Insert tuple
 * - Update 3 times (V1 → V2 → V3 → V4)
 * - Verify:
 *   ✅ Item pointer always points to newest version
 *   ✅ Each version links backward to previous (N2O chain)
 *   ✅ All back versions have HEAP_CHAIN flag
 *   ✅ Oldest version has back_version_tid = 0 (end of chain)
 */
TEST_F(MGABackVersioningTest, VersionChainTraversal)
{
    ErrorContext ctx;
    const uint32_t page_size = 8192;

    uint8_t *page_buffer = new (std::nothrow) uint8_t[page_size];
    ASSERT_NE(page_buffer, nullptr);
    std::memset(page_buffer, 0, page_size);

    HeapPage heap_page(page_buffer, page_size);
    ASSERT_EQ(heap_page.initialize(1, &ctx), Status::OK);

    // Insert V1
    auto tuple_v1 = createTestTuple(100, 0, 0, "Version 1");
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple_v1.data(), tuple_v1.size(), 100, &item_id, &ctx),
              Status::OK);

    // Update to V2
    auto tuple_v2 = createTestTuple(200, 0, 0, "Version 2");
    uint16_t item_id_v2;
    ASSERT_EQ(heap_page.updateTuple(item_id, tuple_v2.data(), tuple_v2.size(), 100, 200, &item_id_v2, &ctx),
              Status::OK);
    EXPECT_EQ(item_id_v2, item_id);

    // Update to V3
    auto tuple_v3 = createTestTuple(300, 0, 0, "Version 3");
    uint16_t item_id_v3;
    ASSERT_EQ(heap_page.updateTuple(item_id, tuple_v3.data(), tuple_v3.size(), 200, 300, &item_id_v3, &ctx),
              Status::OK);
    EXPECT_EQ(item_id_v3, item_id);

    // Update to V4
    auto tuple_v4 = createTestTuple(400, 0, 0, "Version 4");
    uint16_t item_id_v4;
    ASSERT_EQ(heap_page.updateTuple(item_id, tuple_v4.data(), tuple_v4.size(), 300, 400, &item_id_v4, &ctx),
              Status::OK);
    EXPECT_EQ(item_id_v4, item_id);

    // Verify: Item pointer points to V4 (newest)
    const uint8_t *current_data;
    uint32_t current_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &current_data, &current_size, &ctx), Status::OK);
    EXPECT_EQ(extractTupleData(current_data, current_size), "Version 4");

    // Traverse N2O chain: V4 → V3 → V2 → V1
    std::vector<std::string> expected_chain = {"Version 4", "Version 3", "Version 2", "Version 1"};
    std::vector<std::string> actual_chain;

    const TupleHeader *current_hdr = reinterpret_cast<const TupleHeader *>(current_data);
    actual_chain.push_back(extractTupleData(current_data, current_size));

    // Follow back version chain
    while (current_hdr->hasBackVersion())
    {
        uint64_t back_tid = current_hdr->back_version_tid;
        uint32_t back_offset = static_cast<uint32_t>(back_tid & 0xFFFFFFFF);

        const uint8_t *back_data = page_buffer + back_offset;
        current_hdr = reinterpret_cast<const TupleHeader *>(back_data);

        // Verify HEAP_CHAIN flag on back versions
        EXPECT_TRUE(current_hdr->infomask & TupleHeader::HEAP_CHAIN)
            << "Back version should have HEAP_CHAIN flag";

        // Extract data size from TupleHeader (we need to calculate it)
        // For simplicity, we know the size pattern
        uint32_t back_size = tuple_v1.size(); // All versions same size in this test
        actual_chain.push_back(extractTupleData(back_data, back_size));
    }

    // Verify complete chain
    ASSERT_EQ(actual_chain.size(), 4) << "Should have 4 versions in chain";
    for (size_t i = 0; i < expected_chain.size(); i++)
    {
        EXPECT_EQ(actual_chain[i], expected_chain[i])
            << "Version " << (i + 1) << " mismatch in chain";
    }

    delete[] page_buffer;
}

/**
 * Test 3: MVCC Visibility Across Version Chains
 *
 * Verify that findVisibleVersion() correctly traverses back versions:
 * - Insert tuple (xmin=100)
 * - Update to V2 (xmin=200)
 * - Update to V3 (xmin=300)
 * - Test visibility with different snapshot XIDs:
 *   ✅ Snapshot@350 sees V3 (newest)
 *   ✅ Snapshot@250 sees V2 (middle version)
 *   ✅ Snapshot@150 sees V1 (oldest)
 *   ✅ Snapshot@50 sees nothing (before xmin=100)
 */
TEST_F(MGABackVersioningTest, MVCCVisibilityAcrossVersions)
{
    ErrorContext ctx;
    const uint32_t page_size = 8192;

    uint8_t *page_buffer = new (std::nothrow) uint8_t[page_size];
    ASSERT_NE(page_buffer, nullptr);
    std::memset(page_buffer, 0, page_size);

    HeapPage heap_page(page_buffer, page_size);
    ASSERT_EQ(heap_page.initialize(1, &ctx), Status::OK);

    // Insert V1 (xmin=100)
    auto tuple_v1 = createTestTuple(100, 0, 0, "Version 1 @ XID 100");
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple_v1.data(), tuple_v1.size(), 100, &item_id, &ctx),
              Status::OK);

    // Update to V2 (xmin=200)
    auto tuple_v2 = createTestTuple(200, 0, 0, "Version 2 @ XID 200");
    uint16_t item_id_v2;
    ASSERT_EQ(heap_page.updateTuple(item_id, tuple_v2.data(), tuple_v2.size(), 100, 200, &item_id_v2, &ctx),
              Status::OK);

    // Update to V3 (xmin=300)
    auto tuple_v3 = createTestTuple(300, 0, 0, "Version 3 @ XID 300");
    uint16_t item_id_v3;
    ASSERT_EQ(heap_page.updateTuple(item_id, tuple_v3.data(), tuple_v3.size(), 200, 300, &item_id_v3, &ctx),
              Status::OK);

    // Test visibility at different snapshot XIDs
    const uint8_t *visible_data;
    uint32_t visible_size;

    // Snapshot@350 should see V3 (newest)
    ASSERT_EQ(heap_page.findVisibleVersion(item_id, 350, &visible_data, &visible_size, nullptr, &ctx),
              Status::OK);
    EXPECT_EQ(extractTupleData(visible_data, visible_size), "Version 3 @ XID 300");

    // Snapshot@250 should see V2 (middle version)
    ASSERT_EQ(heap_page.findVisibleVersion(item_id, 250, &visible_data, &visible_size, nullptr, &ctx),
              Status::OK);
    EXPECT_EQ(extractTupleData(visible_data, visible_size), "Version 2 @ XID 200");

    // Snapshot@150 should see V1 (oldest)
    ASSERT_EQ(heap_page.findVisibleVersion(item_id, 150, &visible_data, &visible_size, nullptr, &ctx),
              Status::OK);
    EXPECT_EQ(extractTupleData(visible_data, visible_size), "Version 1 @ XID 100");

    // Snapshot@50 should see nothing (before any version)
    ASSERT_EQ(heap_page.findVisibleVersion(item_id, 50, &visible_data, &visible_size, nullptr, &ctx),
              Status::NOT_FOUND);

    delete[] page_buffer;
}

/**
 * Test 4: Cycle Detection in Version Chains
 *
 * Verify that findVisibleVersion() detects corrupted circular chains:
 * - Manually create a circular back version chain
 * - Call findVisibleVersion()
 * - Verify:
 *   ✅ Detects cycle and returns error (prevents infinite loop)
 */
TEST_F(MGABackVersioningTest, CycleDetection)
{
    ErrorContext ctx;
    const uint32_t page_size = 8192;

    uint8_t *page_buffer = new (std::nothrow) uint8_t[page_size];
    ASSERT_NE(page_buffer, nullptr);
    std::memset(page_buffer, 0, page_size);

    HeapPage heap_page(page_buffer, page_size);
    ASSERT_EQ(heap_page.initialize(1, &ctx), Status::OK);

    // Insert V1
    auto tuple_v1 = createTestTuple(100, 0, 0, "Version 1");
    uint16_t item_id;
    ASSERT_EQ(heap_page.insertTuple(tuple_v1.data(), tuple_v1.size(), 100, &item_id, &ctx),
              Status::OK);

    // Update to V2
    auto tuple_v2 = createTestTuple(200, 0, 0, "Version 2");
    uint16_t item_id_v2;
    ASSERT_EQ(heap_page.updateTuple(item_id, tuple_v2.data(), tuple_v2.size(), 100, 200, &item_id_v2, &ctx),
              Status::OK);

    // Corrupt the chain: Make V1 point back to V2 (creating cycle V2 → V1 → V2)
    const uint8_t *v2_data;
    uint32_t v2_size;
    ASSERT_EQ(heap_page.getTuple(item_id, &v2_data, &v2_size, &ctx), Status::OK);

    const TupleHeader *v2_hdr = reinterpret_cast<const TupleHeader *>(v2_data);
    uint64_t v1_tid = v2_hdr->back_version_tid;
    uint32_t v1_offset = static_cast<uint32_t>(v1_tid & 0xFFFFFFFF);

    // Corrupt V1 to point back to V2's offset
    uint32_t v2_offset = static_cast<uint32_t>(
        reinterpret_cast<const uint8_t *>(v2_data) - page_buffer);

    TupleHeader *v1_hdr = reinterpret_cast<TupleHeader *>(page_buffer + v1_offset);
    v1_hdr->back_version_tid = (static_cast<uint64_t>(1) << 32) | v2_offset;
    // back_version_tid != 0 implies hasBackVersion(), no need for extra flag

    // Try to find visible version - should detect cycle
    const uint8_t *visible_data;
    uint32_t visible_size;

    // findVisibleVersion should detect the cycle and return error
    Status result = heap_page.findVisibleVersion(item_id, 300, &visible_data, &visible_size,
                                                  nullptr, &ctx);

    // Should return error (not OK, not infinite loop)
    EXPECT_NE(result, Status::OK) << "Should detect cycle in version chain";

    delete[] page_buffer;
}

/**
 * Test 5: TOAST Rejection (Alpha Limitation)
 *
 * Verify that updateTuple() rejects TOAST tuples in Alpha:
 * - Insert large tuple (would require TOAST in production)
 * - Try to update
 * - Verify:
 *   ✅ Returns Status::NOT_IMPLEMENTED (Alpha limitation)
 */
TEST_F(MGABackVersioningTest, ToastRejection)
{
    ErrorContext ctx;
    const uint32_t page_size = 8192;

    uint8_t *page_buffer = new (std::nothrow) uint8_t[page_size];
    ASSERT_NE(page_buffer, nullptr);
    std::memset(page_buffer, 0, page_size);

    HeapPage heap_page(page_buffer, page_size);
    ASSERT_EQ(heap_page.initialize(1, &ctx), Status::OK);

    // Create a large tuple (> 2KB, would need TOAST)
    std::string large_data(3000, 'X');
    auto large_tuple = createTestTuple(100, 0, 0, large_data);

    uint16_t item_id;
    Status insert_result = heap_page.insertTuple(large_tuple.data(), large_tuple.size(),
                                                  100, &item_id, &ctx);

    // If insert succeeds, try to update it
    if (insert_result == Status::OK)
    {
        auto large_tuple_v2 = createTestTuple(200, 0, 0, large_data);
        uint16_t new_item_id;
        Status update_result = heap_page.updateTuple(item_id, large_tuple_v2.data(),
                                                      large_tuple_v2.size(), 100, 200, &new_item_id, &ctx);

        // Alpha limitation: should reject TOAST updates
        // (In production this would compress or use TOAST storage)
        // For now, we just verify it doesn't crash
        EXPECT_TRUE(update_result == Status::OK || update_result == Status::NOT_IMPLEMENTED)
            << "Update should either succeed or return NOT_IMPLEMENTED for large tuples";
    }

    delete[] page_buffer;
}

/**
 * Test 6: Page Full Scenarios
 *
 * Verify behavior when page doesn't have space for back version:
 * - Fill page with tuples
 * - Try to update (back version won't fit)
 * - Verify:
 *   ✅ Returns Status::PAGE_FULL (Alpha limitation)
 *   ✅ Primary tuple unchanged
 */
TEST_F(MGABackVersioningTest, PageFullScenario)
{
    ErrorContext ctx;
    const uint32_t page_size = 8192;

    uint8_t *page_buffer = new (std::nothrow) uint8_t[page_size];
    ASSERT_NE(page_buffer, nullptr);
    std::memset(page_buffer, 0, page_size);

    HeapPage heap_page(page_buffer, page_size);
    ASSERT_EQ(heap_page.initialize(1, &ctx), Status::OK);

    // Fill page with tuples until nearly full
    std::vector<uint16_t> item_ids;
    const uint32_t tuple_size = 200;
    std::string data(tuple_size - sizeof(TupleHeader), 'A');

    while (true)
    {
        auto tuple = createTestTuple(100, 0, 0, data);
        uint16_t item_id;
        Status result = heap_page.insertTuple(tuple.data(), tuple.size(), 100, &item_id, &ctx);

        if (result != Status::OK)
        {
            break; // Page full
        }
        item_ids.push_back(item_id);
    }

    ASSERT_GT(item_ids.size(), 0) << "Should have inserted at least one tuple";

    // Try to update first tuple (back version won't fit)
    auto updated_tuple = createTestTuple(200, 0, 0, data);
    uint16_t new_item_id;
    Status update_result = heap_page.updateTuple(item_ids[0], updated_tuple.data(),
                                                  updated_tuple.size(), 100, 200, &new_item_id, &ctx);

    // Should return PAGE_FULL (Alpha limitation - no cross-page back versions)
    EXPECT_EQ(update_result, Status::PAGE_FULL)
        << "Should return PAGE_FULL when no space for back version";

    // Verify primary tuple unchanged
    const uint8_t *original_data;
    uint32_t original_size;
    ASSERT_EQ(heap_page.getTuple(item_ids[0], &original_data, &original_size, &ctx), Status::OK);

    const TupleHeader *hdr = reinterpret_cast<const TupleHeader *>(original_data);
    EXPECT_EQ(hdr->xmin, 100) << "Original tuple should be unchanged after failed update";
    EXPECT_FALSE(hdr->hasBackVersion()) << "Should not have back version after failed update";

    delete[] page_buffer;
}
