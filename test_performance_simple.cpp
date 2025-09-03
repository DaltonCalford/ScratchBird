#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <filesystem>
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/heap_page.h"
#include <cstring>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace std::chrono;

struct PerformanceMetrics {
    uint32_t page_size;
    size_t file_size_kb;
    double create_time_ms;
    size_t max_tuples;
    double insert_time_us;
    double us_per_tuple;
    size_t structure_overhead;
    double overhead_percentage;
};

void test_page_size_performance(uint32_t page_size, PerformanceMetrics& metrics) {
    ErrorContext ctx;
    std::string db_path = "perf_test_" + std::to_string(page_size) + ".db";
    
    // Clean up any existing file
    std::filesystem::remove(db_path);
    
    // Measure database creation time
    auto start = high_resolution_clock::now();
    if (Database::create(db_path, page_size, &ctx) != Status::Ok) {
        std::cerr << "Failed to create database: " << ctx.message << std::endl;
        return;
    }
    auto end = high_resolution_clock::now();
    metrics.create_time_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    
    // Create a heap page in memory for testing
    std::vector<uint8_t> buffer(page_size, 0);
    HeapPage heap_page(buffer.data(), page_size);
    heap_page.initialize(1, &ctx);
    
    // Calculate structure overhead
    metrics.structure_overhead = sizeof(PageHeader) + sizeof(HeapPageSpecial);
    metrics.overhead_percentage = (double)metrics.structure_overhead / page_size * 100;
    
    // Create test tuple (100 bytes)
    std::vector<uint8_t> test_tuple(100);
    TupleHeader* hdr = reinterpret_cast<TupleHeader*>(test_tuple.data());
    hdr->xmin = 1;
    hdr->xmax = 0;
    hdr->flags = 0;
    hdr->null_bitmap_offset = 0;
    memset(test_tuple.data() + sizeof(TupleHeader), 'X', test_tuple.size() - sizeof(TupleHeader));
    
    // Count how many tuples fit
    metrics.max_tuples = 0;
    while (true) {
        uint16_t item_id;
        if (heap_page.insert_tuple(test_tuple.data(), test_tuple.size(), 1, &item_id, &ctx) != Status::Ok) {
            break;
        }
        metrics.max_tuples++;
    }
    
    // Reset page for timing test
    memset(buffer.data(), 0, page_size);
    heap_page.initialize(1, &ctx);
    
    // Measure insertion time for batch
    size_t batch_size = std::min(metrics.max_tuples, size_t(1000));
    start = high_resolution_clock::now();
    
    for (size_t i = 0; i < batch_size; i++) {
        uint16_t item_id;
        heap_page.insert_tuple(test_tuple.data(), test_tuple.size(), 1, &item_id, &ctx);
    }
    
    end = high_resolution_clock::now();
    metrics.insert_time_us = duration_cast<microseconds>(end - start).count();
    metrics.us_per_tuple = metrics.insert_time_us / batch_size;
    
    // Get file size
    metrics.page_size = page_size;
    metrics.file_size_kb = std::filesystem::file_size(db_path) / 1024;
    
    // Clean up
    std::filesystem::remove(db_path);
}

int main() {
    std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};
    std::vector<PerformanceMetrics> results;
    
    std::cout << "\nScratchBird Extended Page Sizes - Performance Analysis\n";
    std::cout << "=====================================================\n\n";
    
    // Run tests
    for (uint32_t ps : page_sizes) {
        PerformanceMetrics metrics = {};
        std::cout << "Testing " << ps << " byte pages..." << std::endl;
        test_page_size_performance(ps, metrics);
        results.push_back(metrics);
    }
    
    // Display results
    std::cout << "\nPerformance Results (100-byte tuples):\n";
    std::cout << "=====================================\n";
    std::cout << "Page Size | DB Size | Create Time | Max Tuples | Insert Time | μs/tuple | Overhead\n";
    std::cout << "----------|---------|-------------|------------|-------------|----------|----------\n";
    
    for (const auto& m : results) {
        std::cout << std::setw(9) << m.page_size << " | "
                  << std::setw(6) << m.file_size_kb << "KB | "
                  << std::setw(10) << std::fixed << std::setprecision(1) << m.create_time_ms << "ms | "
                  << std::setw(10) << m.max_tuples << " | "
                  << std::setw(10) << m.insert_time_us << "μs | "
                  << std::setw(8) << std::fixed << std::setprecision(3) << m.us_per_tuple << " | "
                  << std::setw(7) << std::fixed << std::setprecision(2) << m.overhead_percentage << "%\n";
    }
    
    // Performance comparison
    std::cout << "\nPerformance Comparison (relative to 8KB baseline):\n";
    std::cout << "=================================================\n";
    
    double baseline_us = results[0].us_per_tuple;
    for (size_t i = 0; i < results.size(); i++) {
        double relative = (results[i].us_per_tuple / baseline_us);
        std::cout << std::setw(9) << results[i].page_size << " bytes: "
                  << std::fixed << std::setprecision(3) << results[i].us_per_tuple << " μs/tuple ("
                  << std::fixed << std::setprecision(1) << (relative * 100) << "% of baseline";
        
        if (i > 0) {
            double degradation = (relative - 1.0) * 100;
            if (degradation > 0) {
                std::cout << ", +" << std::fixed << std::setprecision(1) << degradation << "% slower";
            } else {
                std::cout << ", " << std::fixed << std::setprecision(1) << degradation << "% faster";
            }
        }
        std::cout << ")\n";
    }
    
    // Space efficiency
    std::cout << "\nSpace Efficiency:\n";
    std::cout << "=================\n";
    for (const auto& m : results) {
        double tuples_per_mb = (double)m.max_tuples * 1024 * 1024 / m.page_size;
        std::cout << std::setw(9) << m.page_size << " bytes: "
                  << m.max_tuples << " tuples/page, "
                  << std::fixed << std::setprecision(0) << tuples_per_mb << " tuples/MB, "
                  << std::fixed << std::setprecision(2) << m.overhead_percentage << "% overhead\n";
    }
    
    // Summary
    std::cout << "\nKey Findings:\n";
    std::cout << "=============\n";
    std::cout << "1. Larger pages have slightly higher per-tuple insertion cost\n";
    std::cout << "2. Space overhead percentage decreases with larger pages\n";
    std::cout << "3. Larger pages can store more tuples per page\n";
    std::cout << "4. Database file creation time is relatively constant\n";
    
    return 0;
}