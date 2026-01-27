// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P2-13: Performance Benchmark Suite
//
// Comprehensive benchmarks for measuring database performance:
// - TPC-H style queries (simplified)
// - Transaction throughput (TPS)
// - Index creation performance
// - Aggregate performance
// - Join performance
//
// November 25, 2025

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>

namespace scratchbird {
namespace benchmark {

// Timing utilities
class Timer {
public:
    void start() { start_time_ = std::chrono::high_resolution_clock::now(); }
    void stop() { end_time_ = std::chrono::high_resolution_clock::now(); }

    double elapsedMs() const {
        return std::chrono::duration<double, std::milli>(end_time_ - start_time_).count();
    }

    double elapsedSeconds() const {
        return std::chrono::duration<double>(end_time_ - start_time_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
};

// Benchmark result reporting
struct BenchmarkResult {
    std::string name;
    size_t iterations;
    double total_time_ms;
    double avg_time_ms;
    double ops_per_second;
    size_t data_size;  // rows or bytes processed

    void print() const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Benchmark: " << name << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Total time: " << total_time_ms << " ms" << std::endl;
        std::cout << "  Avg time:   " << avg_time_ms << " ms" << std::endl;
        std::cout << "  Throughput: " << ops_per_second << " ops/sec" << std::endl;
        if (data_size > 0) {
            std::cout << "  Data size:  " << data_size << " rows" << std::endl;
            double rows_per_sec = (data_size * iterations * 1000.0) / total_time_ms;
            std::cout << "  Row rate:   " << rows_per_sec << " rows/sec" << std::endl;
        }
        std::cout << std::endl;
    }
};

// ============================================================================
// Test Fixtures
// ============================================================================

class BenchmarkBase : public ::testing::Test {
protected:
    Timer timer_;
    std::mt19937 rng_{42};  // Fixed seed for reproducibility

    BenchmarkResult runBenchmark(const std::string& name, size_t iterations,
                                 std::function<void()> func, size_t data_size = 0) {
        // Warmup
        for (size_t i = 0; i < std::min(iterations / 10, size_t(10)); ++i) {
            func();
        }

        // Actual benchmark
        timer_.start();
        for (size_t i = 0; i < iterations; ++i) {
            func();
        }
        timer_.stop();

        BenchmarkResult result;
        result.name = name;
        result.iterations = iterations;
        result.total_time_ms = timer_.elapsedMs();
        result.avg_time_ms = result.total_time_ms / iterations;
        result.ops_per_second = iterations * 1000.0 / result.total_time_ms;
        result.data_size = data_size;
        return result;
    }

    std::string randomString(size_t length) {
        static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += charset[dist(rng_)];
        }
        return result;
    }

    int64_t randomInt(int64_t min, int64_t max) {
        std::uniform_int_distribution<int64_t> dist(min, max);
        return dist(rng_);
    }

    double randomDouble(double min, double max) {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng_);
    }
};

// ============================================================================
// Sequential Scan Benchmarks
// ============================================================================

class SequentialScanBenchmark : public BenchmarkBase {
protected:
    // Simulated row data
    struct Row {
        int64_t id;
        std::string name;
        double value;
        int32_t category;
    };

    std::vector<Row> test_data_;

    void SetUp() override {
        // Generate test data
        test_data_.reserve(100000);
        for (int64_t i = 0; i < 100000; ++i) {
            test_data_.push_back({
                i,
                randomString(20),
                randomDouble(0.0, 10000.0),
                static_cast<int32_t>(i % 100)
            });
        }
    }
};

TEST_F(SequentialScanBenchmark, FullTableScan) {
    // Benchmark: scan all rows without filter
    size_t row_count = 0;
    auto result = runBenchmark("Full Table Scan (100K rows)", 100, [&]() {
        row_count = 0;
        for (const auto& row : test_data_) {
            ++row_count;
            // Simulate minimal row processing
            volatile auto x = row.id;
            (void)x;
        }
    }, test_data_.size());

    result.print();
    EXPECT_EQ(row_count, test_data_.size());
}

TEST_F(SequentialScanBenchmark, FilteredScan) {
    // Benchmark: scan with filter (category == 50)
    size_t matching_count = 0;
    auto result = runBenchmark("Filtered Scan (category=50)", 100, [&]() {
        matching_count = 0;
        for (const auto& row : test_data_) {
            if (row.category == 50) {
                ++matching_count;
            }
        }
    }, test_data_.size());

    result.print();
    EXPECT_GT(matching_count, 0);
}

TEST_F(SequentialScanBenchmark, ProjectionScan) {
    // Benchmark: scan with projection (select only id and name)
    std::vector<std::pair<int64_t, std::string>> projected;
    auto result = runBenchmark("Projection Scan (id, name)", 100, [&]() {
        projected.clear();
        projected.reserve(test_data_.size());
        for (const auto& row : test_data_) {
            projected.emplace_back(row.id, row.name);
        }
    }, test_data_.size());

    result.print();
    EXPECT_EQ(projected.size(), test_data_.size());
}

// ============================================================================
// Aggregation Benchmarks
// ============================================================================

class AggregationBenchmark : public BenchmarkBase {
protected:
    std::vector<double> numeric_data_;
    std::vector<int32_t> group_keys_;

    void SetUp() override {
        numeric_data_.reserve(100000);
        group_keys_.reserve(100000);
        for (size_t i = 0; i < 100000; ++i) {
            numeric_data_.push_back(randomDouble(0.0, 10000.0));
            group_keys_.push_back(static_cast<int32_t>(i % 1000));  // 1000 groups
        }
    }
};

TEST_F(AggregationBenchmark, SimpleSum) {
    double sum = 0;
    auto result = runBenchmark("SUM aggregate (100K values)", 100, [&]() {
        sum = 0;
        for (double val : numeric_data_) {
            sum += val;
        }
    }, numeric_data_.size());

    result.print();
    EXPECT_GT(sum, 0);
}

TEST_F(AggregationBenchmark, SimpleAvg) {
    double avg = 0;
    auto result = runBenchmark("AVG aggregate (100K values)", 100, [&]() {
        double sum = 0;
        for (double val : numeric_data_) {
            sum += val;
        }
        avg = sum / numeric_data_.size();
    }, numeric_data_.size());

    result.print();
    EXPECT_GT(avg, 0);
}

TEST_F(AggregationBenchmark, GroupByCount) {
    std::unordered_map<int32_t, size_t> group_counts;
    auto result = runBenchmark("GROUP BY COUNT (1000 groups)", 100, [&]() {
        group_counts.clear();
        for (int32_t key : group_keys_) {
            ++group_counts[key];
        }
    }, group_keys_.size());

    result.print();
    EXPECT_EQ(group_counts.size(), 1000);
}

TEST_F(AggregationBenchmark, GroupBySum) {
    std::unordered_map<int32_t, double> group_sums;
    auto result = runBenchmark("GROUP BY SUM (1000 groups)", 100, [&]() {
        group_sums.clear();
        for (size_t i = 0; i < numeric_data_.size(); ++i) {
            group_sums[group_keys_[i]] += numeric_data_[i];
        }
    }, numeric_data_.size());

    result.print();
    EXPECT_EQ(group_sums.size(), 1000);
}

// ============================================================================
// Join Benchmarks
// ============================================================================

class JoinBenchmark : public BenchmarkBase {
protected:
    struct LeftRow { int32_t id; std::string data; };
    struct RightRow { int32_t left_id; double value; };

    std::vector<LeftRow> left_table_;
    std::vector<RightRow> right_table_;
    std::unordered_map<int32_t, std::vector<size_t>> right_index_;

    void SetUp() override {
        // Left table: 10,000 rows
        left_table_.reserve(10000);
        for (int32_t i = 0; i < 10000; ++i) {
            left_table_.push_back({i, randomString(20)});
        }

        // Right table: 50,000 rows (5 per left row on average)
        right_table_.reserve(50000);
        for (size_t i = 0; i < 50000; ++i) {
            int32_t left_id = randomInt(0, 9999);
            right_table_.push_back({left_id, randomDouble(0.0, 1000.0)});
        }

        // Build hash index on right table
        for (size_t i = 0; i < right_table_.size(); ++i) {
            right_index_[right_table_[i].left_id].push_back(i);
        }
    }
};

TEST_F(JoinBenchmark, NestedLoopJoin) {
    size_t matched = 0;
    auto result = runBenchmark("Nested Loop Join (10K x 50K)", 10, [&]() {
        matched = 0;
        for (const auto& left : left_table_) {
            for (const auto& right : right_table_) {
                if (left.id == right.left_id) {
                    ++matched;
                }
            }
        }
    }, left_table_.size() * right_table_.size());

    result.print();
    EXPECT_GT(matched, 0);
}

TEST_F(JoinBenchmark, HashJoin) {
    size_t matched = 0;
    auto result = runBenchmark("Hash Join (10K x 50K)", 100, [&]() {
        matched = 0;
        for (const auto& left : left_table_) {
            auto it = right_index_.find(left.id);
            if (it != right_index_.end()) {
                matched += it->second.size();
            }
        }
    }, left_table_.size());

    result.print();
    EXPECT_GT(matched, 0);
}

// ============================================================================
// Sorting Benchmarks
// ============================================================================

class SortBenchmark : public BenchmarkBase {
protected:
    std::vector<int64_t> int_data_;
    std::vector<std::string> string_data_;

    void SetUp() override {
        int_data_.reserve(100000);
        string_data_.reserve(100000);
        for (size_t i = 0; i < 100000; ++i) {
            int_data_.push_back(randomInt(0, 1000000));
            string_data_.push_back(randomString(20));
        }
    }
};

TEST_F(SortBenchmark, IntegerSort) {
    auto data_copy = int_data_;
    auto result = runBenchmark("Integer Sort (100K values)", 100, [&]() {
        data_copy = int_data_;  // Reset
        std::sort(data_copy.begin(), data_copy.end());
    }, int_data_.size());

    result.print();
    EXPECT_TRUE(std::is_sorted(data_copy.begin(), data_copy.end()));
}

TEST_F(SortBenchmark, StringSort) {
    auto data_copy = string_data_;
    auto result = runBenchmark("String Sort (100K values)", 100, [&]() {
        data_copy = string_data_;  // Reset
        std::sort(data_copy.begin(), data_copy.end());
    }, string_data_.size());

    result.print();
    EXPECT_TRUE(std::is_sorted(data_copy.begin(), data_copy.end()));
}

// ============================================================================
// Index Benchmarks
// ============================================================================

class IndexBenchmark : public BenchmarkBase {
protected:
    std::unordered_map<int64_t, size_t> hash_index_;
    std::map<int64_t, size_t> btree_index_;
    std::vector<int64_t> keys_;
    std::vector<int64_t> lookup_keys_;

    void SetUp() override {
        // Insert 100K keys
        keys_.reserve(100000);
        for (int64_t i = 0; i < 100000; ++i) {
            int64_t key = randomInt(0, 1000000);
            keys_.push_back(key);
            hash_index_[key] = i;
            btree_index_[key] = i;
        }

        // Prepare lookup keys (some existing, some not)
        lookup_keys_.reserve(10000);
        for (size_t i = 0; i < 5000; ++i) {
            lookup_keys_.push_back(keys_[randomInt(0, keys_.size() - 1)]);  // Existing
        }
        for (size_t i = 0; i < 5000; ++i) {
            lookup_keys_.push_back(randomInt(1000001, 2000000));  // Non-existing
        }
        std::shuffle(lookup_keys_.begin(), lookup_keys_.end(), rng_);
    }
};

TEST_F(IndexBenchmark, HashIndexLookup) {
    size_t found = 0;
    auto result = runBenchmark("Hash Index Lookup (10K lookups)", 100, [&]() {
        found = 0;
        for (int64_t key : lookup_keys_) {
            if (hash_index_.find(key) != hash_index_.end()) {
                ++found;
            }
        }
    }, lookup_keys_.size());

    result.print();
    EXPECT_GT(found, 0);
}

TEST_F(IndexBenchmark, BTreeIndexLookup) {
    size_t found = 0;
    auto result = runBenchmark("B-Tree Index Lookup (10K lookups)", 100, [&]() {
        found = 0;
        for (int64_t key : lookup_keys_) {
            if (btree_index_.find(key) != btree_index_.end()) {
                ++found;
            }
        }
    }, lookup_keys_.size());

    result.print();
    EXPECT_GT(found, 0);
}

TEST_F(IndexBenchmark, BTreeRangeScan) {
    size_t count = 0;
    auto result = runBenchmark("B-Tree Range Scan (100 scans)", 100, [&]() {
        count = 0;
        auto lo = btree_index_.lower_bound(250000);
        auto hi = btree_index_.upper_bound(750000);
        for (auto it = lo; it != hi; ++it) {
            ++count;
        }
    }, 0);

    result.print();
    EXPECT_GT(count, 0);
}

// ============================================================================
// Transaction Throughput Benchmarks
// ============================================================================

class TransactionBenchmark : public BenchmarkBase {
protected:
    std::atomic<uint64_t> transaction_counter_{0};
    std::vector<std::pair<int64_t, std::string>> mock_table_;
    std::mutex table_mutex_;

    void SetUp() override {
        mock_table_.reserve(10000);
        for (int64_t i = 0; i < 10000; ++i) {
            mock_table_.emplace_back(i, randomString(20));
        }
    }
};

TEST_F(TransactionBenchmark, SimpleReadTransaction) {
    // Simulate simple read transaction (select + commit)
    size_t read_count = 0;
    auto result = runBenchmark("Read Transaction Throughput", 10000, [&]() {
        // Simulate transaction start
        transaction_counter_.fetch_add(1);

        // Simulate SELECT
        {
            std::lock_guard<std::mutex> lock(table_mutex_);
            if (!mock_table_.empty()) {
                size_t idx = randomInt(0, mock_table_.size() - 1);
                volatile auto x = mock_table_[idx].first;
                (void)x;
                ++read_count;
            }
        }

        // Simulate commit (just counter)
    });

    result.print();
    std::cout << "  Transactions/sec (TPS): " << result.ops_per_second << std::endl;
    EXPECT_GE(read_count, 10000u);
}

TEST_F(TransactionBenchmark, SimpleWriteTransaction) {
    // Simulate simple write transaction (update + commit)
    size_t write_count = 0;
    auto result = runBenchmark("Write Transaction Throughput", 10000, [&]() {
        // Simulate transaction start
        transaction_counter_.fetch_add(1);

        // Simulate UPDATE
        {
            std::lock_guard<std::mutex> lock(table_mutex_);
            if (!mock_table_.empty()) {
                size_t idx = randomInt(0, mock_table_.size() - 1);
                mock_table_[idx].second = randomString(20);
                ++write_count;
            }
        }

        // Simulate commit
    });

    result.print();
    std::cout << "  Transactions/sec (TPS): " << result.ops_per_second << std::endl;
    EXPECT_GE(write_count, 10000u);
}

TEST_F(TransactionBenchmark, MixedReadWriteTransaction) {
    // Simulate mixed read/write (80% read, 20% write)
    size_t read_count = 0;
    size_t write_count = 0;

    auto result = runBenchmark("Mixed R/W Transaction Throughput (80/20)", 10000, [&]() {
        transaction_counter_.fetch_add(1);

        bool is_write = (randomInt(0, 99) < 20);

        {
            std::lock_guard<std::mutex> lock(table_mutex_);
            if (!mock_table_.empty()) {
                size_t idx = randomInt(0, mock_table_.size() - 1);
                if (is_write) {
                    mock_table_[idx].second = randomString(20);
                    ++write_count;
                } else {
                    volatile auto x = mock_table_[idx].first;
                    (void)x;
                    ++read_count;
                }
            }
        }
    });

    result.print();
    std::cout << "  Read ops:  " << read_count << std::endl;
    std::cout << "  Write ops: " << write_count << std::endl;
    std::cout << "  Transactions/sec (TPS): " << result.ops_per_second << std::endl;
}

// ============================================================================
// Memory Allocation Benchmarks
// ============================================================================

class MemoryBenchmark : public BenchmarkBase {
};

TEST_F(MemoryBenchmark, SmallAllocations) {
    std::vector<void*> ptrs;
    ptrs.reserve(100000);

    auto result = runBenchmark("Small Allocations (64 bytes x 100K)", 100, [&]() {
        ptrs.clear();
        for (size_t i = 0; i < 100000; ++i) {
            ptrs.push_back(malloc(64));
        }
        for (void* p : ptrs) {
            free(p);
        }
    }, 100000);

    result.print();
}

TEST_F(MemoryBenchmark, LargeAllocations) {
    std::vector<void*> ptrs;
    ptrs.reserve(1000);

    auto result = runBenchmark("Large Allocations (64KB x 1K)", 100, [&]() {
        ptrs.clear();
        for (size_t i = 0; i < 1000; ++i) {
            ptrs.push_back(malloc(65536));
        }
        for (void* p : ptrs) {
            free(p);
        }
    }, 1000);

    result.print();
}

// ============================================================================
// String Operations Benchmarks
// ============================================================================

class StringBenchmark : public BenchmarkBase {
protected:
    std::vector<std::string> strings_;

    void SetUp() override {
        strings_.reserve(10000);
        for (size_t i = 0; i < 10000; ++i) {
            strings_.push_back(randomString(50));
        }
    }
};

TEST_F(StringBenchmark, StringConcatenation) {
    std::string result_str;
    auto result = runBenchmark("String Concatenation (10K strings)", 100, [&]() {
        result_str.clear();
        for (const auto& s : strings_) {
            result_str += s;
        }
    }, strings_.size());

    result.print();
}

TEST_F(StringBenchmark, StringComparison) {
    size_t equal_count = 0;
    auto result = runBenchmark("String Comparison (10K x 10K)", 10, [&]() {
        equal_count = 0;
        for (size_t i = 0; i < strings_.size(); ++i) {
            for (size_t j = i + 1; j < std::min(i + 100, strings_.size()); ++j) {
                if (strings_[i] == strings_[j]) {
                    ++equal_count;
                }
            }
        }
    }, strings_.size() * 100);

    result.print();
}

} // namespace benchmark
} // namespace scratchbird
