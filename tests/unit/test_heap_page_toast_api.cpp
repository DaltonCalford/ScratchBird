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

class HeapPageToastAPITest : public ::testing::Test
{
protected:
    static constexpr uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page_buffer;

    void SetUp() override
    {
        page_buffer.resize(PAGE_SIZE);
    }

    std::vector<uint8_t> buildTuple(const std::vector<uint8_t>& payload,
                                    uint64_t xmin,
                                    uint64_t xmax = 0)
    {
        TupleHeader header{};
        header.xmin = xmin;
        header.xmax = xmax;
        header.back_version_gpid = INVALID_GPID;
        header.back_version_slot = 0;
        header.reserved1 = 0;
        header.ctid_gpid = INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = 0;
        header.null_bitmap_offset = 0;
        header.padding = 0;
        header.session_id = ID{};

        std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload.size());
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        if (!payload.empty())
        {
            std::memcpy(tuple.data() + sizeof(TupleHeader), payload.data(), payload.size());
        }
        return tuple;
    }
};

TEST_F(HeapPageToastAPITest, BasicConstructors)
{
    // Test basic constructor
    HeapPage page1(page_buffer.data(), PAGE_SIZE);
    EXPECT_NE(page1.header(), nullptr);

    // Test TOAST-enabled constructor
    ID table_id = generateUuidV7();
    HeapPage page2(page_buffer.data(), PAGE_SIZE, nullptr, nullptr, table_id);
    EXPECT_NE(page2.header(), nullptr);
}

TEST_F(HeapPageToastAPITest, InsertWithoutToastManager)
{
    ErrorContext error_ctx;

    // Create heap page without TOAST support
    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &error_ctx), Status::OK);

    // Create test data
    std::vector<uint8_t> test_data(100);
    for (size_t i = 0; i < test_data.size(); i++)
    {
        test_data[i] = static_cast<uint8_t>(i % 256);
    }

    // Insert tuple (should work normally without TOAST)
    uint16_t item_id;
    std::vector<uint8_t> tuple = buildTuple(test_data, 100);
    ASSERT_EQ(page.insertTuple(tuple.data(), tuple.size(), 100,
                                &item_id, &error_ctx),
              Status::OK);

    // Retrieve tuple
    const uint8_t *retrieved_data;
    uint32_t retrieved_size;
    ASSERT_EQ(page.getTuple(item_id, &retrieved_data, &retrieved_size, &error_ctx), Status::OK);

    // Verify size and header
    EXPECT_EQ(retrieved_size, tuple.size());

    // Verify payload content (header fields may be normalized by HeapPage)
    EXPECT_EQ(memcmp(retrieved_data + sizeof(TupleHeader),
                     test_data.data(),
                     test_data.size()),
              0);
}

TEST_F(HeapPageToastAPITest, GetTupleDetoastedWithoutToastManager)
{
    ErrorContext error_ctx;

    // Create heap page without TOAST support
    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &error_ctx), Status::OK);

    // Create test data
    std::vector<uint8_t> test_data(200);
    for (size_t i = 0; i < test_data.size(); i++)
    {
        test_data[i] = static_cast<uint8_t>((i * 3) % 256);
    }

    // Insert tuple
    uint16_t item_id;
    std::vector<uint8_t> tuple = buildTuple(test_data, 100);
    ASSERT_EQ(page.insertTuple(tuple.data(), tuple.size(), 100,
                                &item_id, &error_ctx),
              Status::OK);

    // Get detoasted tuple (should return same as regular get_tuple when no TOAST)
    std::vector<uint8_t> detoasted_buffer;
    ASSERT_EQ(page.getTupleDetoasted(item_id, &detoasted_buffer, 100, &error_ctx), Status::OK);

    // Verify size and content
    EXPECT_EQ(detoasted_buffer.size(), tuple.size());
    EXPECT_EQ(memcmp(detoasted_buffer.data() + sizeof(TupleHeader),
                     test_data.data(),
                     test_data.size()),
              0);
}

TEST_F(HeapPageToastAPITest, DeleteTupleWithoutToastManager)
{
    ErrorContext error_ctx;

    // Create heap page without TOAST support
    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &error_ctx), Status::OK);

    // Insert and delete a tuple
    std::vector<uint8_t> test_data(50);
    uint16_t item_id;
    std::vector<uint8_t> tuple = buildTuple(test_data, 100);
    ASSERT_EQ(page.insertTuple(tuple.data(), tuple.size(), 100,
                                &item_id, &error_ctx),
              Status::OK);

    // Delete the tuple
    ASSERT_EQ(page.deleteTuple(item_id, 200, &error_ctx), Status::OK);

    // Verify tuple is deleted
    const uint8_t *retrieved_data;
    uint32_t retrieved_size;
    EXPECT_EQ(page.getTuple(item_id, &retrieved_data, &retrieved_size, &error_ctx),
              Status::NOT_FOUND);
}

TEST_F(HeapPageToastAPITest, LargeTupleWithoutToastFails)
{
    ErrorContext error_ctx;

    // Create heap page without TOAST support
    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &error_ctx), Status::OK);

    // Try to insert a tuple larger than page can hold
    uint32_t max_payload =
        PAGE_SIZE - sizeof(PageHeader) - sizeof(HeapPageSpecial) - sizeof(ItemPointer) -
        sizeof(TupleHeader);
    uint32_t large_size = max_payload + 1;
    std::vector<uint8_t> large_data(large_size);

    uint16_t item_id;
    std::vector<uint8_t> tuple = buildTuple(large_data, 100);
    Status result = page.insertTuple(tuple.data(), tuple.size(),
                                      100, &item_id, &error_ctx);

    // Should fail because tuple is too large for page
    EXPECT_TRUE(result == Status::PAGE_FULL || result == Status::INVALID_ARGUMENT);
}

TEST_F(HeapPageToastAPITest, MultipleTuplesNoToast)
{
    ErrorContext error_ctx;

    // Create heap page without TOAST support
    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &error_ctx), Status::OK);

    // Insert multiple tuples
    std::vector<uint16_t> item_ids;
    std::vector<uint32_t> data_sizes;
    for (int i = 0; i < 10; i++)
    {
        // Keep sizes small to avoid false positives for TOAST detection
        uint32_t data_size = 20 + i * 5; // Max will be 65 bytes
        data_sizes.push_back(data_size);

        std::vector<uint8_t> data(data_size);
        for (size_t j = 0; j < data.size(); j++)
        {
            // Use pattern that won't be mistaken for TOAST marker (0x01)
            data[j] = static_cast<uint8_t>((i + j + 2) % 256);
        }

        uint16_t item_id;
        std::vector<uint8_t> tuple = buildTuple(data, 100 + i);
        ASSERT_EQ(page.insertTuple(tuple.data(), tuple.size(), 100 + i,
                                    &item_id, &error_ctx),
                  Status::OK);
        item_ids.push_back(item_id);
    }

    // Verify all tuples
    for (size_t i = 0; i < item_ids.size(); i++)
    {
        std::vector<uint8_t> detoasted_buffer;
        // Use same xmin as when tuple was inserted
        ASSERT_EQ(page.getTupleDetoasted(item_ids[i], &detoasted_buffer, 100 + i, &error_ctx),
                  Status::OK);

        // Verify size
        uint32_t expected_size = data_sizes[i] + sizeof(TupleHeader);
        EXPECT_EQ(detoasted_buffer.size(), expected_size);
    }
}
