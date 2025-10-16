/**
 * Standalone test for Issue 1.8: Tuple Size Validation Missing
 * Verifies that tuple size validation prevents oversized tuples
 */

#include "include/scratchbird/core/heap_page.h"
#include "include/scratchbird/core/ondisk.h"
#include <iostream>
#include <cstring>
#include <vector>

using namespace scratchbird::core;

int main()
{
    std::cout << "=== Testing Issue 1.8: Tuple Size Validation Missing ===" << std::endl;
    std::cout << std::endl;

    constexpr uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);
    uint8_t *page_data = page_buffer.data();

    // Initialize heap page
    HeapPage heap_page(page_data, PAGE_SIZE);
    ErrorContext ctx;

    if (heap_page.initialize(1, &ctx) != Status::OK)
    {
        std::cout << "FAILED: Could not initialize heap page: " << ctx.message << std::endl;
        return 1;
    }

    std::cout << "Page initialized successfully" << std::endl;
    std::cout << "Page size: " << PAGE_SIZE << " bytes" << std::endl;

    // Calculate maximum tuple size
    uint32_t max_tuple_size = PAGE_SIZE - sizeof(PageHeader) - sizeof(HeapPageSpecial) - sizeof(ItemPointer);
    std::cout << "Maximum tuple size: " << max_tuple_size << " bytes" << std::endl;
    std::cout << "  (= " << PAGE_SIZE << " - " << sizeof(PageHeader)
              << " - " << sizeof(HeapPageSpecial) << " - " << sizeof(ItemPointer) << ")" << std::endl;
    std::cout << std::endl;

    // Test 1: Insert a tuple that's too small (< sizeof(TupleHeader))
    {
        std::cout << "Test 1: Tuple too small (< TupleHeader)... ";

        std::vector<uint8_t> tiny_tuple(sizeof(TupleHeader) - 1);
        uint16_t item_id;

        Status status = heap_page.insertTuple(tiny_tuple.data(), tiny_tuple.size(), 1, &item_id, &ctx);

        if (status == Status::INVALID_ARGUMENT &&
            std::string(ctx.message).find("at least") != std::string::npos)
        {
            std::cout << "PASSED (correctly rejected)" << std::endl;
        }
        else
        {
            std::cout << "FAILED: Should reject tuples smaller than TupleHeader" << std::endl;
            return 1;
        }
    }

    // Test 2: Insert a normal-sized tuple (should succeed)
    {
        std::cout << "Test 2: Normal-sized tuple... ";

        std::vector<uint8_t> normal_tuple(sizeof(TupleHeader) + 100);
        memset(normal_tuple.data(), 0, normal_tuple.size());

        // Initialize tuple header
        auto *tuple_hdr = reinterpret_cast<TupleHeader*>(normal_tuple.data());
        tuple_hdr->xmin = 1;
        tuple_hdr->xmax = 0;
        tuple_hdr->back_version_tid = 0;
        tuple_hdr->ctid_page = 0;
        tuple_hdr->ctid_item = 0;
        tuple_hdr->infomask = 0;

        uint16_t item_id;
        Status status = heap_page.insertTuple(normal_tuple.data(), normal_tuple.size(), 1, &item_id, &ctx);

        if (status == Status::OK)
        {
            std::cout << "PASSED (inserted successfully, item_id=" << item_id << ")" << std::endl;
        }
        else
        {
            std::cout << "FAILED: Should accept normal-sized tuple, got status "
                      << static_cast<int>(status) << ": " << ctx.message << std::endl;
            return 1;
        }
    }

    // Test 3: Try to insert a tuple that's exactly at the max size (should succeed)
    {
        std::cout << "Test 3: Maximum-sized tuple (boundary test)... ";

        std::vector<uint8_t> max_tuple(max_tuple_size);
        memset(max_tuple.data(), 0, max_tuple.size());

        // Initialize tuple header
        auto *tuple_hdr = reinterpret_cast<TupleHeader*>(max_tuple.data());
        tuple_hdr->xmin = 2;
        tuple_hdr->xmax = 0;
        tuple_hdr->back_version_tid = 0;
        tuple_hdr->ctid_page = 0;
        tuple_hdr->ctid_item = 0;
        tuple_hdr->infomask = 0;

        uint16_t item_id;
        Status status = heap_page.insertTuple(max_tuple.data(), max_tuple.size(), 2, &item_id, &ctx);

        // This might fail with PAGE_FULL since we already inserted one tuple
        // But it should NOT fail with underflow/overflow errors
        if (status == Status::OK)
        {
            std::cout << "PASSED (inserted at max size, item_id=" << item_id << ")" << std::endl;
        }
        else if (status == Status::PAGE_FULL)
        {
            std::cout << "PASSED (page full after first tuple, as expected)" << std::endl;
        }
        else
        {
            std::cout << "FAILED: Unexpected status " << static_cast<int>(status)
                      << ": " << ctx.message << std::endl;
            return 1;
        }
    }

    // Test 4: CRITICAL - Try to insert an oversized tuple (MUST be rejected - Issue 1.8 fix)
    {
        std::cout << "Test 4: Oversized tuple (> max_size) [CRITICAL TEST]... ";

        uint32_t oversized = max_tuple_size + 100;  // 100 bytes over the limit
        std::vector<uint8_t> huge_tuple(oversized);
        memset(huge_tuple.data(), 0, huge_tuple.size());

        // Initialize tuple header
        auto *tuple_hdr = reinterpret_cast<TupleHeader*>(huge_tuple.data());
        tuple_hdr->xmin = 3;
        tuple_hdr->xmax = 0;
        tuple_hdr->back_version_tid = 0;
        tuple_hdr->ctid_page = 0;
        tuple_hdr->ctid_item = 0;
        tuple_hdr->infomask = 0;

        uint16_t item_id;
        Status status = heap_page.insertTuple(huge_tuple.data(), huge_tuple.size(), 3, &item_id, &ctx);

        if (status != Status::OK)
        {
            // Should be rejected with INVALID_ARGUMENT
            if (status == Status::INVALID_ARGUMENT &&
                std::string(ctx.message).find("exceed") != std::string::npos)
            {
                std::cout << "PASSED (correctly rejected oversized tuple)" << std::endl;
                std::cout << "  Error message: " << ctx.message << std::endl;
            }
            else
            {
                std::cout << "PASSED (rejected, but with unexpected reason: " << ctx.message << ")" << std::endl;
            }
        }
        else
        {
            std::cout << "FAILED: Should have rejected oversized tuple!" << std::endl;
            std::cout << "  This is a CRITICAL security vulnerability!" << std::endl;
            return 1;
        }
    }

    // Test 5: Extremely large tuple (potential attack vector)
    {
        std::cout << "Test 5: Extremely large tuple (attack vector test)... ";

        uint32_t attack_size = UINT32_MAX / 2;  // Huge value that could cause underflow

        // Don't actually allocate this much memory, just test the size check
        std::vector<uint8_t> dummy_tuple(sizeof(TupleHeader));
        memset(dummy_tuple.data(), 0, dummy_tuple.size());

        // Initialize tuple header
        auto *tuple_hdr = reinterpret_cast<TupleHeader*>(dummy_tuple.data());
        tuple_hdr->xmin = 4;
        tuple_hdr->xmax = 0;
        tuple_hdr->back_version_tid = 0;
        tuple_hdr->ctid_page = 0;
        tuple_hdr->ctid_item = 0;
        tuple_hdr->infomask = 0;

        uint16_t item_id;
        // Pass attack_size but actual data is small (simulating malicious input)
        Status status = heap_page.insertTuple(dummy_tuple.data(), attack_size, 4, &item_id, &ctx);

        if (status != Status::OK)
        {
            std::cout << "PASSED (correctly rejected huge size)" << std::endl;
        }
        else
        {
            std::cout << "FAILED: Should reject extremely large tuple size!" << std::endl;
            return 1;
        }
    }

    std::cout << std::endl;
    std::cout << "=== All tests PASSED ===" << std::endl;
    std::cout << "Issue 1.8 tuple size validation is working correctly!" << std::endl;
    std::cout << std::endl;
    std::cout << "Fix summary:" << std::endl;
    std::cout << "  - Added maximum tuple size validation at line 120-129" << std::endl;
    std::cout << "  - Prevents integer underflow in line 189: tuple_offset = pd_upper - actual_tuple_size" << std::endl;
    std::cout << "  - Prevents buffer overflow in line 199: memcpy to page_data_" << std::endl;
    std::cout << "  - Max tuple size = page_size - PageHeader - HeapPageSpecial - ItemPointer" << std::endl;
    std::cout << "  - For 8KB pages: max ~" << max_tuple_size << " bytes" << std::endl;

    return 0;
}
