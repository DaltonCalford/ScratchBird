#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <filesystem>
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/storage_engine.h"

using namespace scratchbird;
using namespace scratchbird::core;
using namespace std::chrono;

struct PerformanceMetrics {
    uint32_t page_size;
    size_t file_size_kb;
    double create_time_ms;
    double insert_time_ms;
    size_t rows_inserted;
    double ms_per_insert;
    double rows_per_second;
    size_t max_tuple_size;
    size_t tuples_per_page;
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
    
    // Open database and set up components
    Database db;
    if (db.open(db_path, &ctx) != Status::Ok) {
        std::cerr << "Failed to open database: " << ctx.message << std::endl;
        return;
    }
    
    BufferPool::Config bp_config;
    bp_config.pool_size = 64;  // Larger pool for better performance
    bp_config.page_size = page_size;
    BufferPool bp(&db, bp_config);
    bp.initialize(&ctx);
    
    PageManager pm(&db, page_size);
    pm.initialize(&ctx);
    
    CatalogManager cm(&db);
    cm.initialize(&ctx);
    
    TransactionManager tm(&db);
    tm.initialize(&ctx);
    
    StorageEngine se(&db);
    
    // Create a test table
    uint32_t schema_id = 5; // app schema
    uint32_t table_id;
    std::vector<ColumnInfo> columns;
    
    // Create table structure (id int, data varchar)
    cm.create_schema(schema_id, "test_schema", 0, &ctx);
    cm.create_table(schema_id, "perf_table", {}, table_id, &ctx);
    
    // Begin transaction
    Transaction* txn;
    tm.begin_transaction(false, &txn, &ctx);
    
    // Prepare test data
    const size_t num_rows = 10000;
    const size_t data_size = 100;  // 100 byte payload per row
    
    // Measure insertion time
    start = high_resolution_clock::now();
    
    for (size_t i = 0; i < num_rows; i++) {
        std::vector<uint8_t> row_data;
        
        // Add id (4 bytes)
        int32_t id = i;
        row_data.insert(row_data.end(), 
                       reinterpret_cast<uint8_t*>(&id),
                       reinterpret_cast<uint8_t*>(&id) + sizeof(id));
        
        // Add data (100 bytes)
        std::string data(data_size, 'X');
        row_data.insert(row_data.end(), data.begin(), data.end());
        
        uint16_t item_id;
        uint32_t page_id;
        Status status = se.insert_tuple(table_id, row_data.data(), row_data.size(),
                                      txn->xid(), item_id, page_id, &ctx);
        
        if (status != Status::Ok) {
            // Likely out of space - this is normal
            metrics.rows_inserted = i;
            break;
        }
        
        if (i == num_rows - 1) {
            metrics.rows_inserted = num_rows;
        }
    }
    
    end = high_resolution_clock::now();
    metrics.insert_time_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    
    // Commit transaction
    tm.commit_transaction(txn, &ctx);
    
    // Calculate metrics
    metrics.page_size = page_size;
    metrics.file_size_kb = std::filesystem::file_size(db_path) / 1024;
    metrics.ms_per_insert = metrics.insert_time_ms / metrics.rows_inserted;
    metrics.rows_per_second = (metrics.rows_inserted * 1000.0) / metrics.insert_time_ms;
    
    // Calculate space efficiency
    metrics.max_tuple_size = page_size - sizeof(PageHeader) - sizeof(HeapPageSpecial) - sizeof(ItemPointer);
    metrics.tuples_per_page = (page_size - sizeof(PageHeader) - sizeof(HeapPageSpecial)) / 
                             (data_size + sizeof(TupleHeader) + sizeof(ItemPointer));
    
    // Clean up
    bp.shutdown();
    db.close();
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
    std::cout << "\nPerformance Results:\n";
    std::cout << "Page Size | File Size | Create Time | Insert Time | Rows | ms/row | rows/sec | Max Tuple | Tuples/Page\n";
    std::cout << "----------|-----------|-------------|-------------|------|--------|----------|-----------|------------\n";
    
    for (const auto& m : results) {
        std::cout << std::setw(9) << m.page_size << " | "
                  << std::setw(9) << m.file_size_kb << "KB | "
                  << std::setw(10) << std::fixed << std::setprecision(1) << m.create_time_ms << "ms | "
                  << std::setw(10) << std::fixed << std::setprecision(1) << m.insert_time_ms << "ms | "
                  << std::setw(5) << m.rows_inserted << " | "
                  << std::setw(6) << std::fixed << std::setprecision(3) << m.ms_per_insert << " | "
                  << std::setw(8) << std::fixed << std::setprecision(0) << m.rows_per_second << " | "
                  << std::setw(9) << m.max_tuple_size << " | "
                  << std::setw(11) << m.tuples_per_page << "\n";
    }
    
    // Analysis
    std::cout << "\nPerformance Analysis:\n";
    std::cout << "====================\n";
    
    // Calculate relative performance
    double base_ms_per_row = results[0].ms_per_insert;
    for (size_t i = 0; i < results.size(); i++) {
        double relative = (results[i].ms_per_insert / base_ms_per_row) * 100;
        std::cout << results[i].page_size << " bytes: " 
                  << std::fixed << std::setprecision(1) << relative 
                  << "% of 8KB baseline";
        if (i > 0) {
            double degradation = ((results[i].ms_per_insert - base_ms_per_row) / base_ms_per_row) * 100;
            std::cout << " (+" << std::fixed << std::setprecision(1) << degradation << "% slower)";
        }
        std::cout << "\n";
    }
    
    std::cout << "\nSpace Efficiency:\n";
    std::cout << "=================\n";
    for (const auto& m : results) {
        double overhead_pct = (double)(sizeof(PageHeader) + sizeof(HeapPageSpecial)) / m.page_size * 100;
        std::cout << m.page_size << " bytes: " 
                  << std::fixed << std::setprecision(2) << overhead_pct 
                  << "% overhead, "
                  << m.tuples_per_page << " tuples per page\n";
    }
    
    return 0;
}