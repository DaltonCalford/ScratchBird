// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <scratchbird/engine/cache_aligned_structures.h>
#include <scratchbird/engine/connection_memory_arena.h>
#include <scratchbird/engine/pool_allocator.h>
#include <thread>
#include <vector>

namespace scratchbird::engine::tests
{

    //==============================================================================
    // Pool Allocator Tests
    //==============================================================================

    class PoolAllocatorTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            PoolAllocatorConfig config;
            config.size_classes = {16, 32, 64, 128, 256, 512, 1024};
            config.initial_pool_capacity = 100;
            config.thread_cache_size = 32;
            config.enable_statistics = true;

            allocator_ = std::make_unique<PoolAllocator>(config);
        }

        void TearDown() override
        {
            allocator_.reset();
        }

        std::unique_ptr<PoolAllocator> allocator_;
    };

    TEST_F(PoolAllocatorTest, BasicAllocation)
    {
        // Test basic allocation and deallocation
        void* ptr1 = allocator_->allocate(64);
        ASSERT_NE(ptr1, nullptr);
        EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr1) % 8 == 0); // Check alignment

        void* ptr2 = allocator_->allocate(128);
        ASSERT_NE(ptr2, nullptr);
        EXPECT_NE(ptr1, ptr2);

        allocator_->deallocate(ptr1, 64);
        allocator_->deallocate(ptr2, 128);
    }

    TEST_F(PoolAllocatorTest, TemplatedAllocation)
    {
        struct TestStruct {
            std::uint64_t a, b, c, d;
        };

        TestStruct* obj = allocator_->allocate<TestStruct>();
        ASSERT_NE(obj, nullptr);

        // Initialize object
        new (obj) TestStruct{1, 2, 3, 4};
        EXPECT_EQ(obj->a, 1);

        // Clean up
        obj->~TestStruct();
        allocator_->deallocate(obj);
    }

    TEST_F(PoolAllocatorTest, BatchAllocation)
    {
        const std::size_t batch_size = 50;
        auto ptrs = allocator_->allocate_batch(32, batch_size);

        EXPECT_EQ(ptrs.size(), batch_size);
        for (void* ptr : ptrs) {
            EXPECT_NE(ptr, nullptr);
        }

        allocator_->deallocate_batch(ptrs, 32);
    }

    TEST_F(PoolAllocatorTest, LargeObjectAllocation)
    {
        // Test allocation larger than largest pool size class
        void* large_ptr = allocator_->allocate(10000);
        ASSERT_NE(large_ptr, nullptr);

        allocator_->deallocate(large_ptr, 10000);
    }

    TEST_F(PoolAllocatorTest, Statistics)
    {
        // Allocate some objects
        std::vector<void*> ptrs;
        for (int i = 0; i < 20; ++i) {
            ptrs.push_back(allocator_->allocate(64));
        }

        auto stats = allocator_->get_statistics();
        EXPECT_GT(stats.total_allocations.load(), 0);
        EXPECT_EQ(stats.total_deallocations.load(), 0);

        // Deallocate objects
        for (void* ptr : ptrs) {
            allocator_->deallocate(ptr, 64);
        }

        stats = allocator_->get_statistics();
        EXPECT_GT(stats.total_deallocations.load(), 0);
    }

    TEST_F(PoolAllocatorTest, ThreadSafety)
    {
        const int num_threads = 4;
        const int allocations_per_thread = 100;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};

        auto worker = [&]() {
            std::vector<void*> local_ptrs;
            local_ptrs.reserve(allocations_per_thread);

            // Allocate
            for (int i = 0; i < allocations_per_thread; ++i) {
                void* ptr = allocator_->allocate(64);
                EXPECT_NE(ptr, nullptr);
                local_ptrs.push_back(ptr);
            }

            // Deallocate
            for (void* ptr : local_ptrs) {
                allocator_->deallocate(ptr, 64);
            }

            completed_threads.fetch_add(1);
        };

        // Start threads
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker);
        }

        // Wait for completion
        for (auto& t : threads) {
            t.join();
        }

        EXPECT_EQ(completed_threads.load(), num_threads);
    }

    TEST_F(PoolAllocatorTest, PoolPtrRaii)
    {
        {
            auto ptr = make_pool_unique<int>(*allocator_, 42);
            EXPECT_EQ(*ptr, 42);
            *ptr = 100;
            EXPECT_EQ(*ptr, 100);
        } // ptr should automatically deallocate here

        // Test with custom allocator
        auto ptr2 = make_pool_unique<std::string>(*allocator_, "Hello World");
        EXPECT_EQ(*ptr2, "Hello World");
    }

    //==============================================================================
    // Connection Memory Arena Tests
    //==============================================================================

    class ConnectionMemoryArenaTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            ConnectionArenaConfig config;
            config.temporary_arena_config.initial_block_size = 1024;
            config.persistent_arena_config.initial_block_size = 2048;
            config.enable_statistics = true;

            arena_ = std::make_unique<ConnectionMemoryArena>(1, config);
        }

        std::unique_ptr<ConnectionMemoryArena> arena_;
    };

    TEST_F(ConnectionMemoryArenaTest, BasicAllocation)
    {
        void* temp_ptr = arena_->allocate(128, true); // temporary
        ASSERT_NE(temp_ptr, nullptr);

        void* persist_ptr = arena_->allocate(256, false); // persistent
        ASSERT_NE(persist_ptr, nullptr);

        EXPECT_NE(temp_ptr, persist_ptr);
    }

    TEST_F(ConnectionMemoryArenaTest, TemplatedAllocation)
    {
        struct TestObject {
            int value;
            explicit TestObject(int v) : value(v) {}
        };

        TestObject* obj = arena_->allocate<TestObject>(42);
        ASSERT_NE(obj, nullptr);
        EXPECT_EQ(obj->value, 42);
    }

    TEST_F(ConnectionMemoryArenaTest, QueryLifecycle)
    {
        arena_->begin_query();

        void* ptr = arena_->allocate(100, true); // temporary
        ASSERT_NE(ptr, nullptr);

        arena_->end_query();
        arena_->reset_temporary_allocations();

        // After reset, temporary memory should be reclaimed
        auto stats = arena_->get_stats();
        EXPECT_GE(stats.temporary_resets.load(), 1);
    }

    TEST_F(ConnectionMemoryArenaTest, NestedQueries)
    {
        arena_->begin_query();
        arena_->begin_query(); // Nested query

        void* ptr = arena_->allocate(200, true);
        ASSERT_NE(ptr, nullptr);

        arena_->end_query();
        // Should not reset yet (still in outer query)
        arena_->reset_temporary_allocations();

        arena_->end_query();
        // Now we can reset
        arena_->reset_temporary_allocations();
    }

    TEST_F(ConnectionMemoryArenaTest, Statistics)
    {
        void* temp_ptr = arena_->allocate(100, true);
        void* persist_ptr = arena_->allocate(200, false);

        auto stats = arena_->get_stats();
        EXPECT_GT(stats.temporary_allocations.load(), 0);
        EXPECT_GT(stats.persistent_allocations.load(), 0);
        EXPECT_GT(stats.total_allocations.load(), 0);

        (void)temp_ptr;
        (void)persist_ptr;
    }

    TEST_F(ConnectionMemoryArenaTest, MemoryUsage)
    {
        std::size_t initial_usage = arena_->get_total_memory_usage();

        arena_->allocate(1000, false); // persistent

        std::size_t after_allocation = arena_->get_total_memory_usage();
        EXPECT_GT(after_allocation, initial_usage);
    }

    //==============================================================================
    // Connection Arena Manager Tests
    //==============================================================================

    class ConnectionArenaManagerTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            ManagerConfig config;
            config.enable_statistics = true;
            config.enable_background_cleanup = false; // Disable for deterministic tests

            manager_ = std::make_unique<ConnectionArenaManager>(config);
        }

        std::unique_ptr<ConnectionArenaManager> manager_;
    };

    TEST_F(ConnectionArenaManagerTest, ArenaCreation)
    {
        auto arena1 = manager_->get_arena(1);
        ASSERT_NE(arena1, nullptr);

        auto arena2 = manager_->get_arena(2);
        ASSERT_NE(arena2, nullptr);
        EXPECT_NE(arena1, arena2);

        // Getting same connection ID should return same arena
        auto arena1_again = manager_->get_arena(1);
        EXPECT_EQ(arena1, arena1_again);
    }

    TEST_F(ConnectionArenaManagerTest, ArenaRemoval)
    {
        auto arena = manager_->get_arena(1);
        ASSERT_NE(arena, nullptr);

        manager_->remove_arena(1);

        // Getting arena again should create new one
        auto new_arena = manager_->get_arena(1);
        EXPECT_NE(arena, new_arena);
    }

    TEST_F(ConnectionArenaManagerTest, Statistics)
    {
        manager_->get_arena(1);
        manager_->get_arena(2);

        auto stats = manager_->get_stats();
        EXPECT_EQ(stats.active_arenas, 2);
        EXPECT_GT(stats.arenas_created.load(), 0);
    }

    TEST_F(ConnectionArenaManagerTest, ExpiredArenaCleanup)
    {
        {
            auto arena = manager_->get_arena(1);
            ASSERT_NE(arena, nullptr);
        } // arena goes out of scope, becomes expired

        manager_->cleanup_expired_arenas();

        auto stats = manager_->get_stats();
        EXPECT_EQ(stats.active_arenas, 0);
    }

    //==============================================================================
    // Cache-Aligned Structures Tests
    //==============================================================================

    class CacheAlignedStructuresTest : public ::testing::Test
    {
      protected:
        static constexpr std::size_t CACHE_LINE_SIZE = 64; // Assume standard cache line size
    };

    TEST_F(CacheAlignedStructuresTest, CacheAlignedAtomic)
    {
        cache_aligned_atomic<std::uint64_t> counter;

        EXPECT_EQ(counter.load(), 0);
        counter.store(42);
        EXPECT_EQ(counter.load(), 42);

        EXPECT_EQ(counter.fetch_add(10), 42);
        EXPECT_EQ(counter.load(), 52);

        // Check alignment
        EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(&counter) % CACHE_LINE_SIZE == 0);
    }

    TEST_F(CacheAlignedStructuresTest, CacheAlignedWrapper)
    {
        cache_aligned<int> aligned_int(100);
        EXPECT_EQ(*aligned_int, 100);

        *aligned_int = 200;
        EXPECT_EQ(aligned_int.get(), 200);

        // Check alignment
        EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(&aligned_int) % CACHE_LINE_SIZE == 0);
    }

    TEST_F(CacheAlignedStructuresTest, CacheAlignedStack)
    {
        cache_aligned_stack<int> stack;

        EXPECT_TRUE(stack.empty());
        EXPECT_EQ(stack.size(), 0);

        stack.push(1);
        stack.push(2);
        stack.push(3);

        EXPECT_FALSE(stack.empty());
        EXPECT_EQ(stack.size(), 3);

        int value;
        EXPECT_TRUE(stack.pop(value));
        EXPECT_EQ(value, 3); // Stack is LIFO

        EXPECT_TRUE(stack.pop(value));
        EXPECT_EQ(value, 2);

        stack.emplace(42);
        EXPECT_TRUE(stack.pop(value));
        EXPECT_EQ(value, 42);
    }

    TEST_F(CacheAlignedStructuresTest, CacheAlignedHashTable)
    {
        cache_aligned_hash_table<int, std::string> table(16); // Small table for testing

        EXPECT_TRUE(table.insert(1, "one"));
        EXPECT_TRUE(table.insert(2, "two"));
        EXPECT_FALSE(table.insert(1, "duplicate")); // Should fail

        std::string value;
        EXPECT_TRUE(table.find(1, value));
        EXPECT_EQ(value, "one");

        EXPECT_TRUE(table.contains(2));
        EXPECT_FALSE(table.contains(3));

        EXPECT_TRUE(table.erase(1));
        EXPECT_FALSE(table.find(1, value));
        EXPECT_FALSE(table.erase(1)); // Already removed

        auto stats = table.get_stats();
        EXPECT_EQ(stats.total_entries, 1);
        EXPECT_GT(stats.total_accesses, 0);
    }

    TEST_F(CacheAlignedStructuresTest, CacheAlignedRingBuffer)
    {
        cache_aligned_ring_buffer<int> buffer(8);

        EXPECT_TRUE(buffer.empty());
        EXPECT_FALSE(buffer.full());
        EXPECT_EQ(buffer.size(), 0);

        // Fill buffer
        for (int i = 1; i <= 7; ++i) { // 7 items (capacity - 1)
            EXPECT_TRUE(buffer.push(i));
        }

        EXPECT_TRUE(buffer.full());
        EXPECT_FALSE(buffer.push(99)); // Should fail when full

        // Empty buffer
        int value;
        for (int expected = 1; expected <= 7; ++expected) {
            EXPECT_TRUE(buffer.pop(value));
            EXPECT_EQ(value, expected);
        }

        EXPECT_TRUE(buffer.empty());
        EXPECT_FALSE(buffer.pop(value)); // Should fail when empty
    }

    TEST_F(CacheAlignedStructuresTest, CacheAlignedPtr)
    {
        {
            auto ptr = make_cache_aligned<int>(42);
            EXPECT_EQ(*ptr, 42);
            EXPECT_TRUE(cache_utils::is_cache_aligned(ptr.get()));
        }

        // Array allocation
        {
            auto array_ptr = make_cache_aligned<int>(5, 100);
            EXPECT_EQ(array_ptr.size(), 5);
            for (std::size_t i = 0; i < 5; ++i) {
                EXPECT_EQ(array_ptr[i], 100);
            }
            EXPECT_TRUE(cache_utils::is_cache_aligned(array_ptr.get()));
        }
    }

    TEST_F(CacheAlignedStructuresTest, CacheAlignedAllocator)
    {
        std::vector<int, cache_aligned_allocator<int>> vec;

        vec.reserve(100);
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }

        EXPECT_EQ(vec.size(), 100);
        for (std::size_t i = 0; i < vec.size(); ++i) {
            EXPECT_EQ(vec[i], static_cast<int>(i));
        }

        // Check that large allocations are cache-aligned
        if (vec.capacity() >= 64) {
            EXPECT_TRUE(cache_utils::is_cache_aligned(vec.data()));
        }
    }

    //==============================================================================
    // Performance and Stress Tests
    //==============================================================================

    class MemoryOptimizationPerformanceTest : public ::testing::Test
    {
      protected:
        static constexpr int NUM_ITERATIONS = 10000;
        static constexpr int NUM_THREADS = 4;
    };

    TEST_F(MemoryOptimizationPerformanceTest, PoolAllocatorThroughput)
    {
        PoolAllocator allocator;

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<void*> ptrs;
        ptrs.reserve(NUM_ITERATIONS);

        // Allocation phase
        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            ptrs.push_back(allocator.allocate(64));
        }

        // Deallocation phase
        for (void* ptr : ptrs) {
            allocator.deallocate(ptr, 64);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Pool allocator throughput: " << (NUM_ITERATIONS * 2.0 / duration.count())
                  << " operations per microsecond\n";

        // Should complete reasonably quickly
        EXPECT_LT(duration.count(), 50000); // Less than 50ms
    }

    TEST_F(MemoryOptimizationPerformanceTest, ConnectionArenaAllocationPattern)
    {
        ConnectionMemoryArena arena(1, ConnectionArenaConfig{});

        auto start = std::chrono::high_resolution_clock::now();

        // Simulate query processing pattern
        for (int query = 0; query < 100; ++query) {
            arena.begin_query();

            // Temporary allocations during query
            std::vector<void*> temp_ptrs;
            for (int i = 0; i < 50; ++i) {
                temp_ptrs.push_back(arena.allocate(128, true));
            }

            // Some persistent allocations
            for (int i = 0; i < 5; ++i) {
                arena.allocate(256, false);
            }

            arena.end_query();
            arena.reset_temporary_allocations();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Connection arena query simulation: " << duration.count() << "ms\n";
        EXPECT_LT(duration.count(), 100); // Should be fast
    }

    TEST_F(MemoryOptimizationPerformanceTest, CacheAlignedStackConcurrency)
    {
        cache_aligned_stack<int> stack;
        std::atomic<int> total_pushed{0};
        std::atomic<int> total_popped{0};

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&, t]() {
                std::mt19937 rng(t);
                std::uniform_int_distribution<> dist(1, 1000);

                for (int i = 0; i < NUM_ITERATIONS / NUM_THREADS; ++i) {
                    // Random push/pop operations
                    if (dist(rng) % 2 == 0) {
                        stack.push(i);
                        total_pushed.fetch_add(1);
                    } else {
                        int value;
                        if (stack.pop(value)) {
                            total_popped.fetch_add(1);
                        }
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Cache-aligned stack concurrent operations: " << duration.count() << "ms\n";
        std::cout << "Total pushed: " << total_pushed.load()
                  << ", Total popped: " << total_popped.load() << "\n";

        EXPECT_LT(duration.count(), 1000);                   // Should complete within 1 second
        EXPECT_GE(total_pushed.load(), total_popped.load()); // More or equal pushes than pops
    }

    //==============================================================================
    // Integration Tests
    //==============================================================================

    TEST(MemoryOptimizationIntegrationTest, EndToEndScenario)
    {
        // Create manager with pool allocator integration
        ManagerConfig manager_config;
        manager_config.arena_config.temporary_arena_config.enable_statistics = true;
        manager_config.arena_config.persistent_arena_config.enable_statistics = true;

        ConnectionArenaManager manager(manager_config);
        PoolAllocator pool_allocator;

        // Simulate multiple connections with various memory usage patterns
        const int num_connections = 10;
        const int queries_per_connection = 20;

        std::vector<std::shared_ptr<ConnectionMemoryArena>> arenas;
        arenas.reserve(num_connections);

        // Create arenas for connections
        for (int conn = 0; conn < num_connections; ++conn) {
            arenas.push_back(manager.get_arena(conn));
        }

        // Simulate query processing
        for (int query = 0; query < queries_per_connection; ++query) {
            for (auto& arena : arenas) {
                arena->begin_query();

                // Mix of temporary and persistent allocations
                for (int i = 0; i < 10; ++i) {
                    arena->allocate(64 * (i + 1), true); // temporary
                    if (i % 3 == 0) {
                        arena->allocate(128, false); // persistent
                    }

                    // Some pool allocations too
                    void* pool_ptr = pool_allocator.allocate(256);
                    pool_allocator.deallocate(pool_ptr, 256);
                }

                arena->end_query();
                arena->reset_temporary_allocations();
            }
        }

        // Verify statistics
        auto manager_stats = manager.get_stats();
        EXPECT_EQ(manager_stats.active_arenas, num_connections);
        EXPECT_GT(manager_stats.total_memory_usage, 0);

        auto pool_stats = pool_allocator.get_statistics();
        EXPECT_GT(pool_stats.total_allocations.load(), 0);
        EXPECT_GT(pool_stats.total_deallocations.load(), 0);

        // Test fragmentation analysis
        for (const auto& arena : arenas) {
            double fragmentation = arena->get_fragmentation_ratio();
            EXPECT_GE(fragmentation, 0.0);
            EXPECT_LE(fragmentation, 1.0);
        }

        std::cout << "Integration test completed successfully:\n";
        std::cout << "- Active arenas: " << manager_stats.active_arenas << "\n";
        std::cout << "- Total memory usage: " << manager_stats.total_memory_usage << " bytes\n";
        std::cout << "- Pool allocations: " << pool_stats.total_allocations.load() << "\n";
        std::cout << "- Pool deallocations: " << pool_stats.total_deallocations.load() << "\n";
    }

} // namespace scratchbird::engine::tests
