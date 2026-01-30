/**
 * Test for Issue 1.15: Tuple Header Alignment
 * Verifies that tuples are properly aligned to 8-byte boundaries per specification
 */

#include "include/scratchbird/core/heap_page.h"
#include "include/scratchbird/core/database.h"
#include "include/scratchbird/core/buffer_pool.h"
#include "include/scratchbird/core/page_manager.h"
#include "include/scratchbird/core/ondisk.h"
#include "test_helpers.h"
#include <iostream>
#include <cstdio>
#include <cstring>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestShortPath;

// Helper function to check if a value is aligned to 8 bytes
bool isAligned8(uint32_t offset)
{
    return (offset % 8) == 0;
}

int main()
{
    std::cout << "=== Testing Issue 1.15: Tuple Header Alignment ===\n" << std::endl;

    std::string db_path = uniqueTestShortPath("test_tuple_alignment", ".sbrd");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    if (Database::create(db_path.c_str(), 8192, &ctx) != Status::OK)
    {
        std::cout << "FAILED (create): " << ctx.message << std::endl;
        return 1;
    }

    Database db;
    if (db.open(db_path.c_str(), &ctx) != Status::OK)
    {
        std::cout << "FAILED (open): " << ctx.message << std::endl;
        return 1;
    }

    // Test 1: Single tuple insertion - verify 8-byte alignment
    {
        std::cout << "Test 1: Single tuple insertion alignment... ";

        uint32_t page_id;
        if (db.page_manager()->allocatePage(page_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (allocate): " << ctx.message << std::endl;
            return 1;
        }

        void *page_data;
        if (db.buffer_pool()->pinPage(page_id, &page_data, &ctx) != Status::OK)
        {
            std::cout << "FAILED (pin): " << ctx.message << std::endl;
            return 1;
        }

        HeapPage heap_page(reinterpret_cast<uint8_t *>(page_data), 8192);
        if (heap_page.initialize(page_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (initialize): " << ctx.message << std::endl;
            return 1;
        }

        // Create a tuple with odd size to test alignment
        uint32_t tuple_size = sizeof(TupleHeader) + 77; // 77 is not 8-byte aligned
        std::vector<uint8_t> tuple_data(tuple_size);
        auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
        hdr->xmin = 1;
        hdr->xmax = 0;
        hdr->back_version_tid = 0;
        hdr->infomask = 0;

        uint16_t item_id;
        if (heap_page.insertTuple(tuple_data.data(), tuple_size, 1, &item_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (insert): " << ctx.message << std::endl;
            return 1;
        }

        // Get the tuple's offset
        auto *special = reinterpret_cast<HeapPageSpecial *>(
            reinterpret_cast<uint8_t *>(page_data) + 8192 - sizeof(HeapPageSpecial));

        auto *item_pointers = reinterpret_cast<ItemPointer *>(
            reinterpret_cast<uint8_t *>(page_data) + sizeof(PageHeader));

        uint32_t tuple_offset = item_pointers[item_id].offset;

        if (!isAligned8(tuple_offset))
        {
            std::cout << "FAILED: Tuple offset " << tuple_offset
                      << " is NOT 8-byte aligned!" << std::endl;
            return 1;
        }

        db.buffer_pool()->unpinPage(page_id, false, &ctx);

        std::cout << "PASSED" << std::endl;
        std::cout << "  - Tuple offset: " << tuple_offset << " (8-byte aligned) ✅" << std::endl;
    }

    // Test 2: Multiple tuple insertions with varying sizes
    {
        std::cout << "Test 2: Multiple tuples with varying sizes... ";

        uint32_t page_id;
        if (db.page_manager()->allocatePage(page_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (allocate): " << ctx.message << std::endl;
            return 1;
        }

        void *page_data;
        if (db.buffer_pool()->pinPage(page_id, &page_data, &ctx) != Status::OK)
        {
            std::cout << "FAILED (pin): " << ctx.message << std::endl;
            return 1;
        }

        HeapPage heap_page(reinterpret_cast<uint8_t *>(page_data), 8192);
        if (heap_page.initialize(page_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (initialize): " << ctx.message << std::endl;
            return 1;
        }

        // Insert tuples with sizes that would naturally be misaligned
        const uint32_t test_sizes[] = {31, 63, 95, 127, 159, 191, 223, 255};
        std::vector<uint16_t> item_ids;

        for (uint32_t size : test_sizes)
        {
            uint32_t tuple_size = sizeof(TupleHeader) + size;
            std::vector<uint8_t> tuple_data(tuple_size);
            auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
            hdr->xmin = 1;
            hdr->xmax = 0;
            hdr->back_version_tid = 0;
            hdr->infomask = 0;

            uint16_t item_id;
            if (heap_page.insertTuple(tuple_data.data(), tuple_size, 1, &item_id, &ctx) != Status::OK)
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
                std::cout << "FAILED: Tuple " << item_id << " at offset "
                          << tuple_offset << " is NOT 8-byte aligned!" << std::endl;
                misaligned_count++;
            }
        }

        db.buffer_pool()->unpinPage(page_id, false, &ctx);

        if (misaligned_count > 0)
        {
            return 1;
        }

        std::cout << "PASSED" << std::endl;
        std::cout << "  - Inserted " << item_ids.size() << " tuples with varying sizes ✅" << std::endl;
        std::cout << "  - All tuples are 8-byte aligned ✅" << std::endl;
    }

    // Test 3: Alignment after defragmentation
    {
        std::cout << "Test 3: Alignment after defragmentation... ";

        uint32_t page_id;
        if (db.page_manager()->allocatePage(page_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (allocate): " << ctx.message << std::endl;
            return 1;
        }

        void *page_data;
        if (db.buffer_pool()->pinPage(page_id, &page_data, &ctx) != Status::OK)
        {
            std::cout << "FAILED (pin): " << ctx.message << std::endl;
            return 1;
        }

        HeapPage heap_page(reinterpret_cast<uint8_t *>(page_data), 8192);
        if (heap_page.initialize(page_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (initialize): " << ctx.message << std::endl;
            return 1;
        }

        // Insert several tuples
        std::vector<uint16_t> item_ids;
        for (int i = 0; i < 10; i++)
        {
            uint32_t tuple_size = sizeof(TupleHeader) + 50 + (i * 7); // Varying sizes
            std::vector<uint8_t> tuple_data(tuple_size);
            auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
            hdr->xmin = 1;
            hdr->xmax = 0;
            hdr->back_version_tid = 0;
            hdr->infomask = 0;

            uint16_t item_id;
            if (heap_page.insertTuple(tuple_data.data(), tuple_size, 1, &item_id, &ctx) == Status::OK)
            {
                item_ids.push_back(item_id);
            }
        }

        // Delete some tuples to create fragmentation
        if (item_ids.size() >= 3)
        {
            heap_page.deleteTuple(item_ids[1], 2, &ctx); // xmax = 2
            heap_page.deleteTuple(item_ids[3], 2, &ctx); // xmax = 2

            // Trigger defragmentation
            uint32_t bytes_reclaimed = 0;
            if (heap_page.defragmentPage(&bytes_reclaimed, &ctx) != Status::OK)
            {
                std::cout << "FAILED (defragment): " << ctx.message << std::endl;
                return 1;
            }

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
                    std::cout << "FAILED: After defragmentation, tuple " << item_id
                              << " at offset " << tuple_offset << " is NOT 8-byte aligned!" << std::endl;
                    misaligned_count++;
                }
            }

            if (misaligned_count > 0)
            {
                db.buffer_pool()->unpinPage(page_id, false, &ctx);
                return 1;
            }
        }

        db.buffer_pool()->unpinPage(page_id, false, &ctx);

        std::cout << "PASSED" << std::endl;
        std::cout << "  - Defragmentation maintains 8-byte alignment ✅" << std::endl;
    }

    // Test 4: Strict alignment architecture simulation
    {
        std::cout << "Test 4: Strict alignment verification (architectural safety)... ";

        uint32_t page_id;
        if (db.page_manager()->allocatePage(page_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (allocate): " << ctx.message << std::endl;
            return 1;
        }

        void *page_data;
        if (db.buffer_pool()->pinPage(page_id, &page_data, &ctx) != Status::OK)
        {
            std::cout << "FAILED (pin): " << ctx.message << std::endl;
            return 1;
        }

        HeapPage heap_page(reinterpret_cast<uint8_t *>(page_data), 8192);
        if (heap_page.initialize(page_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (initialize): " << ctx.message << std::endl;
            return 1;
        }

        // Insert a tuple and verify we can safely read 64-bit values
        uint32_t tuple_size = sizeof(TupleHeader) + 100;
        std::vector<uint8_t> tuple_data(tuple_size);
        auto *hdr = reinterpret_cast<TupleHeader *>(tuple_data.data());
        hdr->xmin = 1;
        hdr->xmax = 0;
        hdr->back_version_tid = 0;
        hdr->infomask = 0;

        // Fill data with a test pattern (64-bit values)
        uint64_t *data_ptr = reinterpret_cast<uint64_t *>(tuple_data.data() + sizeof(TupleHeader));
        for (size_t i = 0; i < (tuple_size - sizeof(TupleHeader)) / sizeof(uint64_t); i++)
        {
            data_ptr[i] = 0x123456789ABCDEF0ULL + i;
        }

        uint16_t item_id;
        if (heap_page.insertTuple(tuple_data.data(), tuple_size, 1, &item_id, &ctx) != Status::OK)
        {
            std::cout << "FAILED (insert): " << ctx.message << std::endl;
            return 1;
        }

        // Read the tuple back
        auto *item_pointers = reinterpret_cast<ItemPointer *>(
            reinterpret_cast<uint8_t *>(page_data) + sizeof(PageHeader));

        uint32_t tuple_offset = item_pointers[item_id].offset;
        auto *stored_hdr = reinterpret_cast<TupleHeader *>(
            reinterpret_cast<uint8_t *>(page_data) + tuple_offset);

        // Verify tuple header is properly aligned for 64-bit access
        uintptr_t tuple_addr = reinterpret_cast<uintptr_t>(stored_hdr);
        if ((tuple_addr % 8) != 0)
        {
            std::cout << "FAILED: TupleHeader address 0x" << std::hex << tuple_addr
                      << " is NOT 8-byte aligned!" << std::endl;
            return 1;
        }

        // Verify we can read 64-bit values from the data portion without issues
        uint64_t *stored_data = reinterpret_cast<uint64_t *>(
            reinterpret_cast<uint8_t *>(page_data) + tuple_offset + sizeof(TupleHeader));
        for (size_t i = 0; i < (tuple_size - sizeof(TupleHeader)) / sizeof(uint64_t); i++)
        {
            if (stored_data[i] != (0x123456789ABCDEF0ULL + i))
            {
                std::cout << "FAILED: Data value " << i << " corrupted (alignment issue?)" << std::endl;
                return 1;
            }
        }

        db.buffer_pool()->unpinPage(page_id, false, &ctx);

        std::cout << "PASSED" << std::endl;
        std::cout << "  - TupleHeader address is 8-byte aligned ✅" << std::endl;
        std::cout << "  - 64-bit values readable without corruption ✅" << std::endl;
        std::cout << "  - Safe for strict-alignment architectures (ARM, SPARC) ✅" << std::endl;
    }

    db.close();
    std::remove(db_path.c_str());

    std::cout << "\n=== All tests PASSED ===" << std::endl;
    std::cout << "\nIssue 1.15 FIX VERIFIED!" << std::endl;
    std::cout << "\nSummary:" << std::endl;
    std::cout << "  ✅ Single tuple insertion maintains 8-byte alignment" << std::endl;
    std::cout << "  ✅ Multiple tuples with varying sizes all aligned" << std::endl;
    std::cout << "  ✅ Defragmentation preserves 8-byte alignment" << std::endl;
    std::cout << "  ✅ Safe for strict-alignment architectures" << std::endl;
    std::cout << "\nAlignment fixes implemented at:" << std::endl;
    std::cout << "  - heap_page.cpp:206 (insertTuple function)" << std::endl;
    std::cout << "  - heap_page.cpp:975 (defragmentPage function)" << std::endl;
    std::cout << "\nPer specification (ON_DISK_FORMAT.md):" << std::endl;
    std::cout << "  'All structures are aligned to 8-byte boundaries'" << std::endl;

    return 0;
}
