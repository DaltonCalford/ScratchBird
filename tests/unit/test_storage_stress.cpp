#include <gtest/gtest.h>
#include <cstring>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <cstdio>

using namespace scratchbird::core;

// Helper to create a test UUID
static inline UuidV7Bytes makeTestUUID(uint8_t value = 0xAB) {
    UuidV7Bytes uuid;
    memset(uuid.bytes.data(), value, 16);
    return uuid;
}
using namespace std::chrono;

class StorageStressTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        cleanup_test_files();
    }

    void TearDown() override
    {
        cleanup_test_files();
    }

    void cleanup_test_files()
    {
        std::filesystem::remove("test_stress.db");
    }

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

// Stress test: Insert 10,000 tuples
TEST_F(StorageStressTest, Insert10000Tuples)
{
    ASSERT_EQ(Database::create("test_stress.db", 32768), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_stress.db"), Status::OK);

    StorageEngine engine(&db);

    const int num_tuples = 10000;
    std::vector<uint8_t> tuple_data(500, 0xAA); // 500 byte tuples

    auto start = high_resolution_clock::now();

    for (int i = 0; i < num_tuples; i++)
    {
        // Vary the data slightly
        tuple_data[0] = i & 0xFF;
        tuple_data[1] = (i >> 8) & 0xFF;

        uint32_t page_id;
        uint16_t item_id;
        Status status =
            engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed at tuple " << i;

        // Progress report every 1000 tuples
        if (i % 1000 == 999)
        {
            auto elapsed = high_resolution_clock::now() - start;
            double rate = (i + 1) / duration<double>(elapsed).count();
            std::cout << "Inserted " << (i + 1) << " tuples, rate: " << std::fixed
                      << std::setprecision(0) << rate << " tuples/sec\n";
        }
    }

    auto end = high_resolution_clock::now();
    auto elapsed = end - start;

    double rate = num_tuples / duration_cast<duration<double>>(elapsed).count();
    std::cout << "\nInsert Performance Summary:\n";
    std::cout << "  Total tuples: " << num_tuples << "\n";
    std::cout << "  Total time: " << format_duration(duration_cast<duration<double>>(elapsed))
              << "\n";
    std::cout << "  Insert rate: " << std::fixed << std::setprecision(0) << rate << " tuples/sec\n";

    // Verify all tuples with scan
    auto scan_start = high_resolution_clock::now();
    auto iterator = engine.createScan(makeTestUUID(1), nullptr);
    int count = 0;
    Tuple tuple;
    while (!iterator->isDone())
    {
        if (iterator->next(&tuple, nullptr) == Status::OK)
        {
            count++;
        }
    }
    auto scan_end = high_resolution_clock::now();
    auto scan_duration = scan_end - scan_start;

    EXPECT_EQ(count, num_tuples) << "Scan found wrong number of tuples";
    std::cout << "\nScan Performance:\n";
    std::cout << "  Scanned " << count << " tuples in "
              << format_duration(duration_cast<duration<double>>(scan_duration)) << "\n";
    std::cout << "  Scan rate: " << std::fixed << std::setprecision(0)
              << count / duration_cast<duration<double>>(scan_duration).count() << " tuples/sec\n";

    db.close();
}

// Stress test: Random delete pattern
TEST_F(StorageStressTest, RandomDeletePattern)
{
    ASSERT_EQ(Database::create("test_stress.db", 16384), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_stress.db"), Status::OK);

    StorageEngine engine(&db);

    // Insert 5000 tuples
    const int num_tuples = 5000;
    std::vector<std::pair<uint32_t, uint16_t>> tuple_ids;
    std::vector<uint8_t> tuple_data(200, 0xBB);

    for (int i = 0; i < num_tuples; i++)
    {
        tuple_data[0] = i & 0xFF;

        uint32_t page_id;
        uint16_t item_id;
        Status status =
            engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK);
        tuple_ids.push_back({page_id, item_id});
    }

    // Randomly delete 50% of tuples
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_tuples - 1);

    std::vector<bool> deleted(num_tuples, false);
    int delete_count = 0;

    auto delete_start = high_resolution_clock::now();
    while (delete_count < num_tuples / 2)
    {
        int idx = dis(gen);
        if (!deleted[idx])
        {
            Status status =
                engine.deleteTuple(tuple_ids[idx].first, tuple_ids[idx].second, nullptr);
            ASSERT_EQ(status, Status::OK);
            deleted[idx] = true;
            delete_count++;
        }
    }
    auto delete_end = high_resolution_clock::now();

    std::cout << "\nDelete Performance:\n";
    std::cout << "  Deleted " << delete_count << " tuples in "
              << format_duration(duration_cast<duration<double>>(delete_end - delete_start))
              << "\n";

    // Count visible tuples after deletion
    // Transaction management is now handled by TransactionManager // New transaction to see deletes
    auto iterator = engine.createScan(makeTestUUID(1), nullptr);
    int visible_count = 0;
    Tuple tuple;
    while (!iterator->isDone())
    {
        if (iterator->next(&tuple, nullptr) == Status::OK)
        {
            visible_count++;
        }
    }

    EXPECT_EQ(visible_count, num_tuples - delete_count)
        << "Wrong number of visible tuples after deletion";

    // Insert more tuples to test slot reuse
    int reuse_count = 0;
    for (int i = 0; i < 1000; i++)
    {
        uint32_t page_id;
        uint16_t item_id;
        Status status =
            engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Check if we're reusing deleted slots
        for (int j = 0; j < num_tuples; j++)
        {
            if (deleted[j] && tuple_ids[j].first == page_id && tuple_ids[j].second == item_id)
            {
                reuse_count++;
                break;
            }
        }
    }

    std::cout << "\nSlot Reuse:\n";
    std::cout << "  Reused " << reuse_count << " deleted slots out of 1000 inserts\n";

    db.close();
}

// Stress test: Fragmentation handling
TEST_F(StorageStressTest, FragmentationHandling)
{
    ASSERT_EQ(Database::create("test_stress.db", 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_stress.db"), Status::OK);

    StorageEngine engine(&db);

    // Create fragmentation by alternating small and large tuples
    std::vector<uint8_t> small_tuple(50, 0xCC);
    std::vector<uint8_t> large_tuple(500, 0xDD);

    const int iterations = 1000;
    std::vector<std::pair<uint32_t, uint16_t>> large_tuple_ids;

    // Phase 1: Insert alternating tuples
    for (int i = 0; i < iterations; i++)
    {
        uint32_t page_id;
        uint16_t item_id;

        // Small tuple
        Status status =
            engine.insertTuple(makeTestUUID(1), small_tuple.data(), small_tuple.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Large tuple
        status =
            engine.insertTuple(makeTestUUID(1), large_tuple.data(), large_tuple.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK);
        large_tuple_ids.push_back({page_id, item_id});
    }

    // Phase 2: Delete all large tuples (create fragmentation)
    for (const auto &id : large_tuple_ids)
    {
        Status status = engine.deleteTuple(id.first, id.second, nullptr);
        ASSERT_EQ(status, Status::OK);
    }

    // Phase 3: Try to insert medium tuples in fragmented space
    std::vector<uint8_t> medium_tuple(250, 0xEE);
    int successful_inserts = 0;
    int failed_inserts = 0;

    for (int i = 0; i < iterations; i++)
    {
        uint32_t page_id;
        uint16_t item_id;
        Status status =
            engine.insertTuple(makeTestUUID(1), medium_tuple.data(), medium_tuple.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        if (status == Status::OK)
        {
            successful_inserts++;
        }
        else
        {
            failed_inserts++;
        }
    }

    std::cout << "\nFragmentation Test Results:\n";
    std::cout << "  Initial small tuples: " << iterations << "\n";
    std::cout << "  Deleted large tuples: " << iterations << "\n";
    std::cout << "  Medium tuples inserted: " << successful_inserts << "\n";
    std::cout << "  Medium tuples failed: " << failed_inserts << "\n";
    std::cout << "  Space utilization: " << (successful_inserts * 100.0 / iterations) << "%\n";

    db.close();
}

// Stress test: Large database file growth
TEST_F(StorageStressTest, LargeDatabaseGrowth)
{
    ASSERT_EQ(Database::create("test_stress.db", 32768), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_stress.db"), Status::OK);

    StorageEngine engine(&db);

    // Insert until we reach 100MB or 50000 tuples
    const size_t target_size = 100 * 1024 * 1024; // 100MB
    const int max_tuples = 50000;

    std::vector<uint8_t> tuple_data(2000, 0xFF); // 2KB tuples

    auto start = high_resolution_clock::now();
    int tuple_count = 0;
    size_t last_size = 0;

    while (tuple_count < max_tuples)
    {
        uint32_t page_id;
        uint16_t item_id;
        Status status =
            engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        if (status != Status::OK)
        {
            std::cout << "Insert failed at tuple " << tuple_count << " with status "
                      << static_cast<int>(status) << "\n";
            break;
        }

        tuple_count++;

        // Check file size every 1000 tuples
        if (tuple_count % 1000 == 0)
        {
            db.sync(nullptr); // Ensure data is written
            size_t file_size = std::filesystem::file_size("test_stress.db");

            std::cout << "Progress: " << tuple_count
                      << " tuples, file size: " << format_bytes(file_size) << "\n";

            if (file_size >= target_size)
            {
                std::cout << "Reached target size\n";
                break;
            }

            last_size = file_size;
        }
    }

    auto end = high_resolution_clock::now();
    auto elapsed = end - start;

    // Final statistics
    db.sync(nullptr);
    size_t final_size = std::filesystem::file_size("test_stress.db");

    std::cout << "\nLarge Database Statistics:\n";
    std::cout << "  Total tuples: " << tuple_count << "\n";
    std::cout << "  Final file size: " << format_bytes(final_size) << "\n";
    std::cout << "  Time taken: " << format_duration(duration_cast<duration<double>>(elapsed))
              << "\n";
    std::cout << "  Write throughput: "
              << format_bytes(final_size / duration_cast<duration<double>>(elapsed).count())
              << "/sec\n";
    std::cout << "  Bytes per tuple: " << (final_size / tuple_count) << "\n";

    db.close();
}

// Stress test: Transaction ID wraparound
TEST_F(StorageStressTest, TransactionIDStress)
{
    ASSERT_EQ(Database::create("test_stress.db", 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_stress.db"), Status::OK);

    StorageEngine engine(&db);

    // Insert tuples with different transaction IDs
    std::vector<uint8_t> tuple_data(100, 0x11);

    // Simulate many transactions
    const int num_transactions = 10000;
    std::vector<std::pair<uint32_t, uint16_t>> tuple_ids;

    for (int i = 0; i < num_transactions; i++)
    {
        // Transaction management is now handled by TransactionManager // Increment XID

        uint32_t page_id;
        uint16_t item_id;
        Status status =
            engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        ASSERT_EQ(status, Status::OK);
        tuple_ids.push_back({page_id, item_id});

        // Every 100 transactions, delete some old tuples
        if (i % 100 == 99 && i > 200)
        {
            // Transaction management is now handled by TransactionManager // Delete in new
            // transaction

            // Delete tuples from 200 transactions ago
            int delete_idx = i - 200;
            status = engine.deleteTuple(tuple_ids[delete_idx].first, tuple_ids[delete_idx].second,
                                         nullptr);
            ASSERT_EQ(status, Status::OK);
        }
    }

    // Check visibility rules after many transactions
    // Transaction management is now handled by TransactionManager
    uint64_t final_xid = engine.getCurrentXid();

    std::cout << "\nTransaction ID Stress Test:\n";
    std::cout << "  Transactions executed: " << num_transactions << "\n";
    std::cout << "  Final XID: " << final_xid << "\n";
    std::cout << "  XID is 64-bit: " << (sizeof(final_xid) == 8 ? "YES" : "NO") << "\n";

    // Scan and count visible tuples
    auto iterator = engine.createScan(makeTestUUID(1), nullptr);
    int visible_count = 0;
    Tuple tuple;
    while (!iterator->isDone())
    {
        if (iterator->next(&tuple, nullptr) == Status::OK)
        {
            visible_count++;
        }
    }

    std::cout << "  Visible tuples: " << visible_count << "\n";

    db.close();
}

// Stress test: Concurrent-style access pattern (single-threaded simulation)
TEST_F(StorageStressTest, ConcurrentAccessPattern)
{
    ASSERT_EQ(Database::create("test_stress.db", 16384), Status::OK);

    Database db;
    ASSERT_EQ(db.open("test_stress.db"), Status::OK);

    StorageEngine engine(&db);

    // Simulate multiple "sessions" with interleaved operations
    const int num_sessions = 5;
    const int ops_per_session = 1000;

    struct Session
    {
        int id;
        int insert_count = 0;
        int delete_count = 0;
        int scan_count = 0;
        std::vector<std::pair<uint32_t, uint16_t>> my_tuples;
    };

    std::vector<Session> sessions(num_sessions);
    for (int i = 0; i < num_sessions; i++)
    {
        sessions[i].id = i;
    }

    // Random operation generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> session_dis(0, num_sessions - 1);
    std::uniform_int_distribution<> op_dis(0, 2); // 0=insert, 1=delete, 2=scan

    auto start = high_resolution_clock::now();

    for (int op = 0; op < ops_per_session * num_sessions; op++)
    {
        int session_idx = session_dis(gen);
        Session &session = sessions[session_idx];
        int op_type = op_dis(gen);

        // Transaction management is now handled by TransactionManager // Each operation in new
        // transaction

        switch (op_type)
        {
            case 0:
            { // Insert
                std::vector<uint8_t> data(100 + session.id * 10, session.id);
                uint32_t page_id;
                uint16_t item_id;
                Status status = engine.insertTuple(
                    makeTestUUID(1), data.data(), data.size() + sizeof(TupleHeader), &page_id, &item_id, nullptr);
                if (status == Status::OK)
                {
                    session.my_tuples.push_back({page_id, item_id});
                    session.insert_count++;
                }
                break;
            }
            case 1:
            { // Delete
                if (!session.my_tuples.empty())
                {
                    int idx = gen() % session.my_tuples.size();
                    Status status = engine.deleteTuple(session.my_tuples[idx].first,
                                                        session.my_tuples[idx].second, nullptr);
                    if (status == Status::OK)
                    {
                        session.my_tuples.erase(session.my_tuples.begin() + idx);
                        session.delete_count++;
                    }
                }
                break;
            }
            case 2:
            { // Scan
                auto iterator = engine.createScan(makeTestUUID(1), nullptr);
                int count = 0;
                Tuple tuple;
                while (!iterator->isDone() && count < 100)
                { // Limit scan
                    if (iterator->next(&tuple, nullptr) == Status::OK)
                    {
                        count++;
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
    std::cout << "  Total operations: " << (ops_per_session * num_sessions) << "\n";
    std::cout << "  Time taken: " << format_duration(duration_cast<duration<double>>(elapsed))
              << "\n";
    std::cout << "  Operations/sec: " << std::fixed << std::setprecision(0)
              << (ops_per_session * num_sessions) / duration_cast<duration<double>>(elapsed).count()
              << "\n\n";

    for (const auto &session : sessions)
    {
        std::cout << "  Session " << session.id << ": " << session.insert_count << " inserts, "
                  << session.delete_count << " deletes, " << session.scan_count << " scans\n";
    }

    db.close();
}