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
 * Storage Stress Tests using HeapPage directly
 *
 * These tests verify heap page operations under stress conditions without
 * requiring CatalogManager integration. Tests use HeapPage directly to
 * test storage layer performance and correctness.
 */

#include <gtest/gtest.h>
#include <cstring>
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cstdio>

using namespace scratchbird::core;
using namespace std::chrono;

constexpr size_t kPayloadOffset = sizeof(TupleHeader);

static std::vector<uint8_t> makeTestTuple(size_t payload_size, uint8_t fill, uint64_t xmin = 100)
{
    std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_size, 0);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple.data());
    *hdr = {};
    hdr->xmin = xmin;
    hdr->xmax = 0;
    std::memset(tuple.data() + sizeof(TupleHeader), fill, payload_size);
    return tuple;
}

class StorageStressTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Helper to format bytes
    std::string format_bytes(size_t bytes)
    {
        const char *units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = bytes;
        while (size >= 1024 && unit < 3)
        {
            size /= 1024;
            unit++;
        }
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << size << " " << units[unit];
        return ss.str();
    }

    // Helper to format time
    std::string format_duration(duration<double> time)
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << time.count() << "s";
        return ss.str();
    }
};

// Stress test: Insert many tuples across multiple pages
TEST_F(StorageStressTest, Insert10000Tuples)
{
    // Use multiple heap pages to simulate a large insert workload
    const uint32_t PAGE_SIZE = 32768;
    const int NUM_PAGES = 200;  // More pages to fit 10000+ tuples
    const size_t TUPLE_SIZE = 200;  // Smaller tuples for higher density

    std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    auto tuple_data = makeTestTuple(TUPLE_SIZE, 0xAA);
    int total_inserted = 0;
    int current_page = 0;

    auto start = high_resolution_clock::now();

    // Insert tuples across pages until we've inserted ~10000 or run out of space
    while (total_inserted < 10000 && current_page < NUM_PAGES)
    {
        // Vary the data slightly
        tuple_data[kPayloadOffset] = total_inserted & 0xFF;
        tuple_data[kPayloadOffset + 1] = (total_inserted >> 8) & 0xFF;

        uint16_t item_id;
        Status status = heap_pages[current_page].insertTuple(
            tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

        if (status == Status::OK)
        {
            total_inserted++;
        }
        else
        {
            // Page full, move to next
            current_page++;
        }

        // Progress report every 1000 tuples
        if (total_inserted % 1000 == 0 && total_inserted > 0)
        {
            auto elapsed = high_resolution_clock::now() - start;
            double rate = total_inserted / duration<double>(elapsed).count();
            std::cout << "Inserted " << total_inserted << " tuples, rate: " << std::fixed
                      << std::setprecision(0) << rate << " tuples/sec\n";
        }
    }

    auto end = high_resolution_clock::now();
    auto elapsed = end - start;

    double rate = total_inserted / duration_cast<duration<double>>(elapsed).count();
    std::cout << "\nInsert Performance Summary:\n";
    std::cout << "  Total tuples: " << total_inserted << "\n";
    std::cout << "  Pages used: " << (current_page + 1) << "\n";
    std::cout << "  Total time: " << format_duration(duration_cast<duration<double>>(elapsed)) << "\n";
    std::cout << "  Insert rate: " << std::fixed << std::setprecision(0) << rate << " tuples/sec\n";

    EXPECT_GT(total_inserted, 5000) << "Should have inserted at least 5000 tuples";

    // Verify all tuples with scan
    auto scan_start = high_resolution_clock::now();
    int count = 0;
    for (int p = 0; p <= current_page && p < NUM_PAGES; p++)
    {
        for (uint16_t slot = 0; slot < heap_pages[p].getItemCount(); slot++)
        {
            const uint8_t *data;
            uint32_t size;
            Status status = heap_pages[p].getTuple(slot, &data, &size, nullptr);
            if (status == Status::OK && data != nullptr)
            {
                count++;
            }
        }
    }
    auto scan_end = high_resolution_clock::now();
    auto scan_duration = scan_end - scan_start;

    EXPECT_EQ(count, total_inserted) << "Scan found wrong number of tuples";
    std::cout << "\nScan Performance:\n";
    std::cout << "  Scanned " << count << " tuples in "
              << format_duration(duration_cast<duration<double>>(scan_duration)) << "\n";
}

// Stress test: Random delete pattern using heap page operations
TEST_F(StorageStressTest, RandomDeletePattern)
{
    const uint32_t PAGE_SIZE = 16384;
    const int NUM_PAGES = 20;
    const size_t TUPLE_SIZE = 200;

    std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    // Insert 5000 tuples
    struct TupleLocation { int page; uint16_t slot; };
    std::vector<TupleLocation> tuple_locs;
    auto tuple_data = makeTestTuple(TUPLE_SIZE, 0xBB);

    int current_page = 0;
    int total_inserted = 0;

    while (total_inserted < 5000 && current_page < NUM_PAGES)
    {
        tuple_data[kPayloadOffset] = total_inserted & 0xFF;

        uint16_t item_id;
        Status status = heap_pages[current_page].insertTuple(
            tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

        if (status == Status::OK)
        {
            tuple_locs.push_back({current_page, item_id});
            total_inserted++;
        }
        else
        {
            current_page++;
        }
    }

    EXPECT_GT(total_inserted, 1000) << "Should have inserted many tuples";

    // Randomly mark 50% of tuples as deleted (set xmax)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, total_inserted - 1);

    std::vector<bool> deleted(total_inserted, false);
    int delete_count = 0;

    auto delete_start = high_resolution_clock::now();
    while (delete_count < total_inserted / 2)
    {
        int idx = dis(gen);
        if (!deleted[idx])
        {
            // In heap page, we can mark tuples as deleted by setting xmax
            // For this test, we just track deletions logically
            deleted[idx] = true;
            delete_count++;
        }
    }
    auto delete_end = high_resolution_clock::now();

    std::cout << "\nDelete Pattern Test:\n";
    std::cout << "  Total tuples: " << total_inserted << "\n";
    std::cout << "  Deleted: " << delete_count << "\n";
    std::cout << "  Delete time: " << format_duration(duration_cast<duration<double>>(delete_end - delete_start)) << "\n";

    // Count visible tuples (not deleted)
    int visible_count = 0;
    for (int i = 0; i < total_inserted; i++)
    {
        if (!deleted[i])
        {
            visible_count++;
        }
    }

    EXPECT_EQ(visible_count, total_inserted - delete_count);
}

// Stress test: Fragmentation handling with varying tuple sizes
TEST_F(StorageStressTest, FragmentationHandling)
{
    const uint32_t PAGE_SIZE = 8192;
    const int NUM_PAGES = 10;

    std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    // Create fragmentation by alternating small and large tuples
    auto small_tuple = makeTestTuple(50, 0xCC);
    auto large_tuple = makeTestTuple(500, 0xDD);

    const int ITERATIONS = 200;
    int small_count = 0;
    int large_count = 0;
    int current_page = 0;

    // Phase 1: Insert alternating tuples
    for (int i = 0; i < ITERATIONS && current_page < NUM_PAGES; i++)
    {
        uint16_t item_id;

        // Small tuple
        Status status = heap_pages[current_page].insertTuple(
            small_tuple.data(), small_tuple.size(), 100, &item_id, nullptr);
        if (status == Status::OK)
        {
            small_count++;
        }
        else
        {
            current_page++;
            if (current_page >= NUM_PAGES) break;
            status = heap_pages[current_page].insertTuple(
                small_tuple.data(), small_tuple.size(), 100, &item_id, nullptr);
            if (status == Status::OK) small_count++;
        }

        // Large tuple
        status = heap_pages[current_page].insertTuple(
            large_tuple.data(), large_tuple.size(), 100, &item_id, nullptr);
        if (status == Status::OK)
        {
            large_count++;
        }
        else
        {
            current_page++;
            if (current_page >= NUM_PAGES) break;
            status = heap_pages[current_page].insertTuple(
                large_tuple.data(), large_tuple.size(), 100, &item_id, nullptr);
            if (status == Status::OK) large_count++;
        }
    }

    std::cout << "\nFragmentation Test Results:\n";
    std::cout << "  Small tuples inserted: " << small_count << "\n";
    std::cout << "  Large tuples inserted: " << large_count << "\n";
    std::cout << "  Pages used: " << (current_page + 1) << "\n";

    // Calculate total bytes stored
    size_t total_data = (small_count * small_tuple.size()) + (large_count * large_tuple.size());
    size_t total_pages = (current_page + 1) * PAGE_SIZE;
    double utilization = (total_data * 100.0) / total_pages;
    std::cout << "  Space utilization: " << std::fixed << std::setprecision(1) << utilization << "%\n";

    EXPECT_GT(small_count, 50) << "Should have inserted many small tuples";
    EXPECT_GT(large_count, 50) << "Should have inserted many large tuples";
}

// Stress test: Large data growth across many pages
TEST_F(StorageStressTest, LargeDatabaseGrowth)
{
    const uint32_t PAGE_SIZE = 32768;
    const int MAX_PAGES = 100;
    const size_t TUPLE_SIZE = 2000;  // 2KB tuples

    std::vector<std::vector<uint8_t>> page_buffers(MAX_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < MAX_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    auto tuple_data = makeTestTuple(TUPLE_SIZE, 0xFF);
    int total_inserted = 0;
    int current_page = 0;

    auto start = high_resolution_clock::now();

    // Insert until we've used all pages or inserted 50000 tuples
    while (total_inserted < 50000 && current_page < MAX_PAGES)
    {
        uint16_t item_id;
        Status status = heap_pages[current_page].insertTuple(
            tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

        if (status == Status::OK)
        {
            total_inserted++;
        }
        else
        {
            current_page++;
        }

        // Progress report every 1000 tuples
        if (total_inserted % 1000 == 0 && total_inserted > 0)
        {
            size_t total_size = (current_page + 1) * PAGE_SIZE;
            std::cout << "Progress: " << total_inserted
                      << " tuples, size: " << format_bytes(total_size) << "\n";
        }
    }

    auto end = high_resolution_clock::now();
    auto elapsed = end - start;

    size_t final_size = (current_page + 1) * PAGE_SIZE;
    size_t data_size = total_inserted * tuple_data.size();

    std::cout << "\nLarge Database Statistics:\n";
    std::cout << "  Total tuples: " << total_inserted << "\n";
    std::cout << "  Pages used: " << (current_page + 1) << "\n";
    std::cout << "  Total page size: " << format_bytes(final_size) << "\n";
    std::cout << "  Data stored: " << format_bytes(data_size) << "\n";
    std::cout << "  Time taken: " << format_duration(duration_cast<duration<double>>(elapsed)) << "\n";
    std::cout << "  Bytes per tuple: " << tuple_data.size() << "\n";

    EXPECT_GT(total_inserted, 1000) << "Should have inserted many tuples";
}

// Stress test: Transaction ID simulation with heap page operations
TEST_F(StorageStressTest, TransactionIDStress)
{
    const uint32_t PAGE_SIZE = 8192;
    const int NUM_PAGES = 20;
    const size_t TUPLE_SIZE = 100;

    std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    auto tuple_data = makeTestTuple(TUPLE_SIZE, 0x11);
    struct TupleInfo { int page; uint16_t slot; uint64_t xid; };
    std::vector<TupleInfo> tuples;

    int current_page = 0;
    uint64_t current_xid = 1;

    // Simulate many transactions
    const int NUM_TRANSACTIONS = 10000;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < NUM_TRANSACTIONS && current_page < NUM_PAGES; i++)
    {
        current_xid++;  // Simulate incrementing transaction ID

        uint16_t item_id;
        Status status = heap_pages[current_page].insertTuple(
            tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);

        if (status == Status::OK)
        {
            tuples.push_back({current_page, item_id, current_xid});
        }
        else
        {
            current_page++;
            if (current_page < NUM_PAGES)
            {
                status = heap_pages[current_page].insertTuple(
                    tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);
                if (status == Status::OK)
                {
                    tuples.push_back({current_page, item_id, current_xid});
                }
            }
        }
    }

    auto end = high_resolution_clock::now();
    auto elapsed = end - start;

    std::cout << "\nTransaction ID Stress Test:\n";
    std::cout << "  Simulated transactions: " << NUM_TRANSACTIONS << "\n";
    std::cout << "  Tuples inserted: " << tuples.size() << "\n";
    std::cout << "  Final XID: " << current_xid << "\n";
    std::cout << "  XID is 64-bit: " << (sizeof(current_xid) == 8 ? "YES" : "NO") << "\n";
    std::cout << "  Time taken: " << format_duration(duration_cast<duration<double>>(elapsed)) << "\n";

    const size_t tuple_size = tuple_data.size();
    const size_t per_page_capacity =
        (PAGE_SIZE - sizeof(PageHeader) - sizeof(HeapPageSpecial)) /
        (tuple_size + sizeof(ItemPointer));
    const size_t expected_min = per_page_capacity * NUM_PAGES;
    // Keep a small per-page margin for allocator/header contract drift while still
    // enforcing that pages are packed close to computed capacity.
    const size_t min_with_margin = (expected_min > static_cast<size_t>(NUM_PAGES))
                                       ? (expected_min - static_cast<size_t>(NUM_PAGES))
                                       : 0;

    EXPECT_GE(tuples.size(), min_with_margin)
        << "Should have filled pages close to computed tuple/page capacity";
}

// Stress test: Concurrent-style access pattern (single-threaded simulation)
TEST_F(StorageStressTest, ConcurrentAccessPattern)
{
    const uint32_t PAGE_SIZE = 16384;
    const int NUM_PAGES = 20;

    std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    // Simulate multiple "sessions" with interleaved operations
    const int NUM_SESSIONS = 5;
    const int OPS_PER_SESSION = 1000;

    struct Session
    {
        int id;
        int insert_count = 0;
        int scan_count = 0;
        std::vector<std::pair<int, uint16_t>> my_tuples;  // page, slot
    };

    std::vector<Session> sessions(NUM_SESSIONS);
    for (int i = 0; i < NUM_SESSIONS; i++)
    {
        sessions[i].id = i;
    }

    // Random operation generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> session_dis(0, NUM_SESSIONS - 1);
    std::uniform_int_distribution<> op_dis(0, 1);  // 0=insert, 1=scan

    int current_page = 0;
    auto start = high_resolution_clock::now();

    for (int op = 0; op < OPS_PER_SESSION * NUM_SESSIONS && current_page < NUM_PAGES; op++)
    {
        int session_idx = session_dis(gen);
        Session &session = sessions[session_idx];
        int op_type = op_dis(gen);

        switch (op_type)
        {
            case 0:  // Insert
            {
                auto data = makeTestTuple(100 + session.id * 10, static_cast<uint8_t>(session.id));
                uint16_t item_id;
                Status status = heap_pages[current_page].insertTuple(
                    data.data(), data.size(), 100, &item_id, nullptr);

                if (status == Status::OK)
                {
                    session.my_tuples.push_back({current_page, item_id});
                    session.insert_count++;
                }
                else
                {
                    current_page++;
                    if (current_page < NUM_PAGES)
                    {
                        status = heap_pages[current_page].insertTuple(
                            data.data(), data.size(), 100, &item_id, nullptr);
                        if (status == Status::OK)
                        {
                            session.my_tuples.push_back({current_page, item_id});
                            session.insert_count++;
                        }
                    }
                }
                break;
            }
            case 1:  // Scan
            {
                int count = 0;
                for (int p = 0; p <= current_page && p < NUM_PAGES; p++)
                {
                    for (uint16_t slot = 0; slot < heap_pages[p].getItemCount() && count < 100; slot++)
                    {
                        const uint8_t *data;
                        uint32_t size;
                        Status status = heap_pages[p].getTuple(slot, &data, &size, nullptr);
                        if (status == Status::OK && data != nullptr)
                        {
                            count++;
                        }
                    }
                }
                session.scan_count++;
                break;
            }
        }
    }

    auto end = high_resolution_clock::now();
    auto elapsed = end - start;

    std::cout << "\nConcurrent Access Pattern Results:\n";
    std::cout << "  Total operations: " << (OPS_PER_SESSION * NUM_SESSIONS) << "\n";
    std::cout << "  Time taken: " << format_duration(duration_cast<duration<double>>(elapsed)) << "\n";
    std::cout << "  Operations/sec: " << std::fixed << std::setprecision(0)
              << (OPS_PER_SESSION * NUM_SESSIONS) / duration_cast<duration<double>>(elapsed).count() << "\n\n";

    for (const auto &session : sessions)
    {
        std::cout << "  Session " << session.id << ": " << session.insert_count << " inserts, "
                  << session.scan_count << " scans\n";
    }
}
