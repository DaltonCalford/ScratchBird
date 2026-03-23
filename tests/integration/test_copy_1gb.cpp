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
 * COPY 1GB Memory Test
 *
 * Tests:
 * - 4.1.6: 1GB COPY test (no OOM)
 * - 4.1.7: COPY throughput (> 10MB/s)
 * - Memory leak detection during COPY operations
 * - Streaming performance validation
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "scratchbird/ipc/engine_ipc_session_handler.h"
#include "scratchbird/ipc/ipc_server.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::ipc;
using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

namespace {

// Test constants
constexpr size_t ONE_GB = 1024ULL * 1024 * 1024;  // 1GB in bytes
constexpr size_t CHUNK_SIZE = 64 * 1024;          // 64KB chunks
constexpr size_t NUM_CHUNKS = ONE_GB / CHUNK_SIZE;
constexpr double MIN_THROUGHPUT_MBPS = 10.0;      // Minimum 10MB/s
constexpr int NUM_STREAMING_ITERATIONS = 10;      // For streaming test
constexpr int NUM_CONCURRENT_SESSIONS = 5;        // Concurrent sessions test
constexpr int NUM_MEMORY_RUNS = 3;                // Memory leak detection runs

// Memory statistics structure
struct MemoryStats {
    size_t current_rss_mb;
    size_t peak_rss_mb;
    size_t vm_size_mb;
};

// Get current memory statistics
MemoryStats getMemoryStats() {
    MemoryStats stats = {};
    std::ifstream status("/proc/self/status");
    std::string line;
    
    while (std::getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            std::istringstream iss(line);
            std::string label;
            size_t value;
            iss >> label >> value;
            stats.current_rss_mb = value / 1024;
        } else if (line.find("VmHWM:") == 0) {
            std::istringstream iss(line);
            std::string label;
            size_t value;
            iss >> label >> value;
            stats.peak_rss_mb = value / 1024;
        } else if (line.find("VmSize:") == 0) {
            std::istringstream iss(line);
            std::string label;
            size_t value;
            iss >> label >> value;
            stats.vm_size_mb = value / 1024;
        }
    }
    
    return stats;
}

// Test fixture for 1GB COPY tests
class Copy1GBTest : public ::testing::Test {
protected:
    void ensureSessionUserExists(const std::string& username, bool is_superuser = false) {
        auto* catalog = database_->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID user_id;
        ErrorContext ctx;
        auto status = catalog->ensureUserExists(username,
                                                "",
                                                ID{},
                                                is_superuser,
                                                user_id,
                                                &ctx);
        ASSERT_EQ(status, Status::OK)
            << "Failed to ensure session user " << username << ": " << ctx.message;
    }

    void SetUp() override {
        core::ErrorContext ctx;
        
        // Create test database file
        db_file_ = std::make_unique<TestDatabaseFile>("copy_1gb_test");
        
        // Create and open database
        ASSERT_EQ(core::Database::create(db_file_->path(), 16384, &ctx), core::Status::OK)
            << "Failed to create database: " << ctx.message;
        
        database_ = std::make_unique<core::Database>();
        ASSERT_EQ(database_->open(db_file_->path(), &ctx), core::Status::OK)
            << "Failed to open database: " << ctx.message;
        
        // Initialize ProcArray
        auto status = core::ProcArrayManager::initialize(database_.get(), 10, &ctx);
        ASSERT_EQ(status, core::Status::OK) << "Failed to initialize ProcArray: " << ctx.message;

        ensureSessionUserExists("test_user");
        
        // Create handler
        handler_ = std::make_unique<EngineIPCSessionHandler>(database_.get());
        
        // Generate test data
        test_data_.resize(CHUNK_SIZE);
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(32, 126);
        for (auto& c : test_data_) {
            c = static_cast<uint8_t>(dist(rng));
        }
    }
    
    void TearDown() override {
        core::ErrorContext ctx;
        
        handler_.reset();
        database_.reset();
        
        core::ProcArrayManager::shutdown(&ctx);
        
        db_file_.reset();
        test_data_.clear();
    }
    
    // Generate a chunk of test data
    std::vector<uint8_t> generateChunk(size_t size) {
        std::vector<uint8_t> chunk(size);
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(32, 126);
        for (auto& c : chunk) {
            c = static_cast<uint8_t>(dist(rng));
        }
        return chunk;
    }
    
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<core::Database> database_;
    std::unique_ptr<EngineIPCSessionHandler> handler_;
    std::vector<uint8_t> test_data_;
};

// Test 1: Basic 1GB COPY without OOM
TEST_F(Copy1GBTest, Copy_1GB_NoOOM) {
    ErrorContext ctx;
    
    IPCStartupPayload startup;
    std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
    startup.database[sizeof(startup.database) - 1] = '\0';
    std::strncpy(startup.user, "test_user", sizeof(startup.user) - 1);
    startup.user[sizeof(startup.user) - 1] = '\0';
    std::strncpy(startup.application, "copy_test", sizeof(startup.application) - 1);
    startup.application[sizeof(startup.application) - 1] = '\0';
    startup.process_id = 12345;
    startup.secret_key = 0;
    startup.feature_flags = 0;
    
    auto status = handler_->onAttach(1, startup, &ctx);
    ASSERT_EQ(status, Status::OK);
    
    status = handler_->onCopyInStart(1, &ctx);
    ASSERT_EQ(status, Status::OK);
    
    // Stream 1GB in chunks
    size_t total_bytes = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_CHUNKS && total_bytes < ONE_GB; i++) {
        size_t chunk_size = std::min(CHUNK_SIZE, ONE_GB - total_bytes);
        auto chunk = generateChunk(chunk_size);
        
        status = handler_->onCopyData(1, chunk.data(), chunk.size(), &ctx);
        EXPECT_EQ(status, Status::OK);
        
        total_bytes += chunk_size;
        
        // Check memory every 100MB
        if (i % 1600 == 0) {
            auto mem = getMemoryStats();
            EXPECT_LT(mem.current_rss_mb, 512) << "Memory usage exceeded 512MB at " 
                                               << total_bytes / (1024*1024) << "MB";
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double throughput_mbps = (total_bytes / (1024.0 * 1024.0)) / (duration.count() / 1000.0);
    
    EXPECT_EQ(total_bytes, ONE_GB);
    EXPECT_GE(throughput_mbps, MIN_THROUGHPUT_MBPS) << "Throughput below minimum: " 
                                                     << throughput_mbps << " MB/s";
    
    status = handler_->onCopyDone(1, &ctx);
    EXPECT_EQ(status, Status::OK);
    
    handler_->onDetach(1, &ctx);
}

// Test 2: COPY throughput validation (> 10MB/s)
TEST_F(Copy1GBTest, Copy_Throughput_10MBps) {
    ErrorContext ctx;
    
    IPCStartupPayload startup;
    std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
    startup.database[sizeof(startup.database) - 1] = '\0';
    std::strncpy(startup.user, "test_user", sizeof(startup.user) - 1);
    startup.user[sizeof(startup.user) - 1] = '\0';
    std::strncpy(startup.application, "copy_test", sizeof(startup.application) - 1);
    startup.application[sizeof(startup.application) - 1] = '\0';
    startup.process_id = 12345;
    startup.secret_key = 0;
    startup.feature_flags = 0;
    
    auto status = handler_->onAttach(1, startup, &ctx);
    ASSERT_EQ(status, Status::OK);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    status = handler_->onCopyInStart(1, &ctx);
    ASSERT_EQ(status, Status::OK);
    
    size_t total_bytes = 0;
    for (size_t i = 0; i < NUM_CHUNKS && total_bytes < ONE_GB; i++) {
        size_t chunk_size = std::min(CHUNK_SIZE, ONE_GB - total_bytes);
        auto chunk = generateChunk(chunk_size);
        
        status = handler_->onCopyData(1, chunk.data(), chunk.size(), &ctx);
        EXPECT_EQ(status, Status::OK);
        
        total_bytes += chunk_size;
    }
    
    status = handler_->onCopyDone(1, &ctx);
    EXPECT_EQ(status, Status::OK);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double throughput_mbps = (ONE_GB / (1024.0 * 1024.0)) / (duration.count() / 1000.0);
    
    std::cout << "COPY throughput: " << std::fixed << std::setprecision(2) 
              << throughput_mbps << " MB/s (target: " << MIN_THROUGHPUT_MBPS << " MB/s)" 
              << std::endl;
    
    EXPECT_GE(throughput_mbps, MIN_THROUGHPUT_MBPS);
    
    handler_->onDetach(1, &ctx);
}

// Test 3: Sustained throughput over multiple iterations
TEST_F(Copy1GBTest, Copy_Throughput_Sustained) {
    ErrorContext ctx;
    
    IPCStartupPayload startup;
    std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
    startup.database[sizeof(startup.database) - 1] = '\0';
    std::strncpy(startup.user, "test_user", sizeof(startup.user) - 1);
    startup.user[sizeof(startup.user) - 1] = '\0';
    std::strncpy(startup.application, "copy_test", sizeof(startup.application) - 1);
    startup.application[sizeof(startup.application) - 1] = '\0';
    startup.process_id = 12345;
    startup.secret_key = 0;
    startup.feature_flags = 0;
    
    auto status = handler_->onAttach(1, startup, &ctx);
    ASSERT_EQ(status, Status::OK);
    
    std::vector<double> throughputs;
    const size_t ITERATION_SIZE = 100 * 1024 * 1024;  // 100MB per iteration
    
    for (int iter = 0; iter < NUM_STREAMING_ITERATIONS; iter++) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        status = handler_->onCopyInStart(1, &ctx);
        ASSERT_EQ(status, Status::OK);
        
        size_t iteration_chunks = ITERATION_SIZE / CHUNK_SIZE;
        for (size_t i = 0; i < iteration_chunks; i++) {
            auto chunk = generateChunk(CHUNK_SIZE);
            status = handler_->onCopyData(1, chunk.data(), chunk.size(), &ctx);
            EXPECT_EQ(status, Status::OK);
        }
        
        status = handler_->onCopyDone(1, &ctx);
        EXPECT_EQ(status, Status::OK);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double throughput = (ITERATION_SIZE / (1024.0 * 1024.0)) / (duration.count() / 1000.0);
        throughputs.push_back(throughput);
    }
    
    // Calculate average and variance
    double avg_throughput = 0;
    for (double t : throughputs) {
        avg_throughput += t;
    }
    avg_throughput /= throughputs.size();
    
    double variance = 0;
    for (double t : throughputs) {
        variance += (t - avg_throughput) * (t - avg_throughput);
    }
    variance /= throughputs.size();
    double std_dev = std::sqrt(variance);
    
    std::cout << "Sustained throughput: avg=" << std::fixed << std::setprecision(2) 
              << avg_throughput << " MB/s, std_dev=" << std_dev << " MB/s" << std::endl;
    
    EXPECT_GE(avg_throughput, MIN_THROUGHPUT_MBPS);
    EXPECT_LT(std_dev / avg_throughput, 0.5);  // Coefficient of variation < 50%
    
    handler_->onDetach(1, &ctx);
}

// Test 4: Memory leak detection over multiple runs
TEST_F(Copy1GBTest, Copy_NoMemoryLeak_MultipleRuns) {
    auto mem_baseline = getMemoryStats();
    std::cout << "Baseline memory: " << mem_baseline.current_rss_mb << " MB RSS" << std::endl;
    
    for (int run = 0; run < NUM_MEMORY_RUNS; run++) {
        ErrorContext ctx;
        
        IPCStartupPayload startup;
        std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
        startup.database[sizeof(startup.database) - 1] = '\0';
        std::strncpy(startup.user, "test_user", sizeof(startup.user) - 1);
        startup.user[sizeof(startup.user) - 1] = '\0';
        std::strncpy(startup.application, "copy_test", sizeof(startup.application) - 1);
        startup.application[sizeof(startup.application) - 1] = '\0';
        startup.process_id = 12345;
        startup.secret_key = 0;
        startup.feature_flags = 0;
        
        auto status = handler_->onAttach(1, startup, &ctx);
        ASSERT_EQ(status, Status::OK);
        
        status = handler_->onCopyInStart(1, &ctx);
        ASSERT_EQ(status, Status::OK);
        
        size_t total_bytes = 0;
        for (size_t i = 0; i < NUM_CHUNKS && total_bytes < ONE_GB; i++) {
            size_t chunk_size = std::min(CHUNK_SIZE, ONE_GB - total_bytes);
            auto chunk = generateChunk(chunk_size);
            
            status = handler_->onCopyData(1, chunk.data(), chunk.size(), &ctx);
            EXPECT_EQ(status, Status::OK);
            
            total_bytes += chunk_size;
        }
        
        status = handler_->onCopyDone(1, &ctx);
        EXPECT_EQ(status, Status::OK);
        
        handler_->onDetach(1, &ctx);
        
        auto mem_after = getMemoryStats();
        std::cout << "Run " << (run + 1) << ": " << mem_after.current_rss_mb << " MB RSS" << std::endl;
    }
    
    // Force memory cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto mem_final = getMemoryStats();
    std::cout << "Final memory: " << mem_final.current_rss_mb << " MB RSS" << std::endl;
    
    // Memory should not grow by more than 50MB across all runs
    EXPECT_LT(mem_final.current_rss_mb, mem_baseline.current_rss_mb + 50)
        << "Memory leak detected: " << (mem_final.current_rss_mb - mem_baseline.current_rss_mb)
        << "MB increase";
}

// Test 5: Concurrent COPY sessions
TEST_F(Copy1GBTest, Copy_ConcurrentSessions) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};
    
    auto session_task = [&](int session_id) {
        ErrorContext ctx;
        
        IPCStartupPayload startup;
        std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
        startup.database[sizeof(startup.database) - 1] = '\0';
        std::strncpy(startup.user, "test_user", sizeof(startup.user) - 1);
        startup.user[sizeof(startup.user) - 1] = '\0';
        std::strncpy(startup.application, "copy_test", sizeof(startup.application) - 1);
        startup.application[sizeof(startup.application) - 1] = '\0';
        startup.process_id = 12345 + session_id;
        startup.secret_key = 0;
        startup.feature_flags = 0;
        
        auto status = handler_->onAttach(session_id, startup, &ctx);
        if (status != Status::OK) {
            fail_count++;
            return;
        }
        
        status = handler_->onCopyInStart(session_id, &ctx);
        if (status != Status::OK) {
            fail_count++;
            handler_->onDetach(session_id, &ctx);
            return;
        }
        
        const size_t SESSION_DATA_SIZE = 200 * 1024 * 1024;  // 200MB per session
        size_t session_chunks = SESSION_DATA_SIZE / CHUNK_SIZE;
        bool success = true;
        
        for (size_t i = 0; i < session_chunks; i++) {
            auto chunk = generateChunk(CHUNK_SIZE);
            status = handler_->onCopyData(session_id, chunk.data(), chunk.size(), &ctx);
            if (status != Status::OK) {
                success = false;
                break;
            }
        }
        
        if (success) {
            status = handler_->onCopyDone(session_id, &ctx);
            if (status == Status::OK) {
                success_count++;
            } else {
                fail_count++;
            }
        } else {
            fail_count++;
        }
        
        handler_->onDetach(session_id, &ctx);
    };
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_CONCURRENT_SESSIONS; i++) {
        threads.emplace_back(session_task, i + 1);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    double total_data_mb = (NUM_CONCURRENT_SESSIONS * 200.0);
    double throughput_mbps = total_data_mb / (duration.count() / 1000.0);
    
    std::cout << "Concurrent COPY: " << success_count << " succeeded, " 
              << fail_count << " failed" << std::endl;
    std::cout << "Total throughput: " << std::fixed << std::setprecision(2) 
              << throughput_mbps << " MB/s" << std::endl;
    
    EXPECT_EQ(success_count, NUM_CONCURRENT_SESSIONS);
    EXPECT_EQ(fail_count, 0);
    EXPECT_GE(throughput_mbps, MIN_THROUGHPUT_MBPS);
}

// Test 6: COPY fail and recovery
TEST_F(Copy1GBTest, Copy_FailAndRecover) {
    ErrorContext ctx;
    
    IPCStartupPayload startup;
    std::strncpy(startup.database, "test_db", sizeof(startup.database) - 1);
    startup.database[sizeof(startup.database) - 1] = '\0';
    std::strncpy(startup.user, "test_user", sizeof(startup.user) - 1);
    startup.user[sizeof(startup.user) - 1] = '\0';
    std::strncpy(startup.application, "copy_test", sizeof(startup.application) - 1);
    startup.application[sizeof(startup.application) - 1] = '\0';
    startup.process_id = 12345;
    startup.secret_key = 0;
    startup.feature_flags = 0;
    
    auto status = handler_->onAttach(1, startup, &ctx);
    ASSERT_EQ(status, Status::OK);
    
    // First COPY: simulate failure after 100MB
    status = handler_->onCopyInStart(1, &ctx);
    ASSERT_EQ(status, Status::OK);
    
    size_t fail_after = 100 * 1024 * 1024;  // 100MB
    size_t bytes_sent = 0;
    
    while (bytes_sent < fail_after) {
        auto chunk = generateChunk(CHUNK_SIZE);
        status = handler_->onCopyData(1, chunk.data(), chunk.size(), &ctx);
        EXPECT_EQ(status, Status::OK);
        bytes_sent += CHUNK_SIZE;
    }
    
    // Fail the COPY
    status = handler_->onCopyFail(1, "Simulated failure", &ctx);
    EXPECT_EQ(status, Status::OK);
    
    // Recovery: start new COPY
    status = handler_->onCopyInStart(1, &ctx);
    ASSERT_EQ(status, Status::OK);
    
    bytes_sent = 0;
    while (bytes_sent < ONE_GB) {
        size_t chunk_size = std::min(CHUNK_SIZE, ONE_GB - bytes_sent);
        auto chunk = generateChunk(chunk_size);
        status = handler_->onCopyData(1, chunk.data(), chunk.size(), &ctx);
        EXPECT_EQ(status, Status::OK);
        bytes_sent += chunk_size;
    }
    
    status = handler_->onCopyDone(1, &ctx);
    EXPECT_EQ(status, Status::OK);
    
    handler_->onDetach(1, &ctx);
}

} // namespace
