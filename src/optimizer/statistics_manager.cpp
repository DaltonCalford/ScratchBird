/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/plain_value_reader.h"
#include "scratchbird/core/debug.h"
#include <algorithm>
#include <unordered_map>
#include <random>
#include <cmath>
#include <cstring>
#include <ctime>
#include <chrono>
#include <unordered_set>

// SECURITY FIX (LOW-8): Use OpenSSL for cryptographically secure random numbers
#ifdef __has_include
#if __has_include(<openssl/rand.h>)
#include <openssl/rand.h>
#define HAVE_OPENSSL_RAND 1
#endif
#endif

namespace scratchbird::optimizer
{
    // Hash function for std::vector<uint8_t> (for use in unordered_set)
    struct VectorHash
    {
        size_t operator()(const std::vector<uint8_t> &v) const
        {
            size_t hash = 0;
            for (uint8_t byte : v)
            {
                hash = hash * 31 + byte;
            }
            return hash;
        }
    };

    core::TypeInfo buildTypeInfo(const core::CatalogManager::ColumnInfo &column)
    {
        core::TypeInfo info(static_cast<core::DataType>(column.data_type));
        uint32_t precision = column.type_precision != 0 ? column.type_precision
                                                        : column.max_length;
        info.precision = precision;
        info.scale = column.type_scale;
        info.with_timezone = column.with_timezone;
        info.timezone_hint = column.timezone_hint;
        return info;
    }

    bool resolveColumnEncryption(core::DomainManager *domain_mgr,
                                 const core::CatalogManager::ColumnInfo &column,
                                 bool &encrypted_out,
                                 core::ErrorContext *ctx)
    {
        encrypted_out = false;
        if (column.domain_id == core::ID{} || domain_mgr == nullptr)
        {
            return true;
        }

        core::DomainInfo domain;
        core::Status status = domain_mgr->getDomain(column.domain_id, domain, ctx);
        if (status == core::Status::NOT_FOUND)
        {
            return true;
        }
        if (status != core::Status::OK)
        {
            return false;
        }
        encrypted_out = domain.security.encryption_enabled;
        return true;
    }

    bool isLengthPrefixedType(core::DataType type)
    {
        switch (type)
        {
            case core::DataType::CHAR:
            case core::DataType::VARCHAR:
            case core::DataType::TEXT:
            case core::DataType::JSON:
            case core::DataType::JSONB:
            case core::DataType::XML:
            case core::DataType::BINARY:
            case core::DataType::VARBINARY:
            case core::DataType::BLOB:
            case core::DataType::BYTEA:
            case core::DataType::VECTOR:
                return true;
            default:
                return false;
        }
    }

    StatisticsManager::StatisticsManager(Database *db)
        : db_(db),
          catalog_(nullptr),
          statistics_page_id_(0)
    {
        // The statistics_page_id_ will be set during initialization
        // For now, use a placeholder value that will be updated
        DEBUG_LOG_DB("StatisticsManager created");
    }

    StatisticsManager::~StatisticsManager()
    {
        DEBUG_LOG_DB("StatisticsManager destroyed");
    }

    auto StatisticsManager::analyzeTable(const ID &table_id, float sample_rate,
                                          ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Analyzing table");

        // Phase 1, Task 1.1: Complete statistics collection implementation
        //
        // Steps:
        // 1. Get table info from catalog
        // 2. Determine sample size
        // 3. Sample table using Vitter's Algorithm S
        // 4. For each column, compute statistics
        // 5. Store statistics in catalog (cache)
        // 6. Update statistics cache

        // Get table information from catalog
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        std::vector<core::CatalogManager::ColumnInfo> columns;
        Status status = catalog_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table columns");
            return status;
        }

        if (columns.empty())
        {
            DEBUG_LOG_DB("Table has no columns, skipping analysis");
            return Status::OK;
        }

        // Determine sample size
        // Default: 30,000 rows or 10% of table (PostgreSQL-style)
        // If sample_rate is provided, use that instead
        uint64_t sample_size = 30000; // Default

        if (sample_rate > 0.0f && sample_rate <= 1.0f)
        {
            // User specified sample rate
            // We'll use it directly in sampleTable
            // For now, use default sample size and let sampleTable handle it
            sample_size = 30000;
        }

        // Sample the table using Vitter's Algorithm S
        std::vector<std::vector<uint8_t>> sample_rows;
        status = sampleTable(table_id, sample_size, sample_rows, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to sample table");
            return status;
        }

        if (sample_rows.empty())
        {
            DEBUG_LOG_DB("No rows sampled, table may be empty");
            return Status::OK;
        }

        DEBUG_LOG_DB("Sampled " + std::to_string(sample_rows.size()) + " rows");

        // Analyze each column
        for (const auto &column_info : columns)
        {
            DEBUG_LOG_DB("Analyzing column " + column_info.column_name);

            // Extract column values and compute basic statistics
            // We need to extract column values separately for histogram and MCV
            std::vector<std::vector<uint8_t>> column_values =
                extractColumnValues(table_id, column_info.column_id, sample_rows, columns, ctx);

            if (column_values.empty())
            {
                DEBUG_LOG_DB("Failed to extract column values for " + column_info.column_name);
                continue;
            }

            // Compute column statistics
            ColumnStatistics col_stats;
            col_stats.table_id = table_id;
            col_stats.column_id = column_info.column_id;
            col_stats.column_name = column_info.column_name;
            col_stats.data_type = static_cast<core::DataType>(column_info.data_type);

            // Basic statistics
            uint64_t null_count = 0;
            uint64_t total_width = 0;
            for (const auto &val : column_values)
            {
                if (val.empty())
                {
                    null_count++;
                }
                else
                {
                    total_width += val.size();
                }
            }

            col_stats.num_rows = column_values.size();
            col_stats.num_nulls = null_count;
            col_stats.null_fraction = static_cast<float>(null_count) / static_cast<float>(column_values.size());

            uint64_t non_null_count = column_values.size() - null_count;
            if (non_null_count > 0)
            {
                col_stats.avg_width = static_cast<float>(total_width) / static_cast<float>(non_null_count);
            }
            else
            {
                col_stats.avg_width = 0.0f;
            }

            // n_distinct estimation
            col_stats.num_distinct = estimateNDistinct(column_values, col_stats.num_rows, column_values.size());

            // Set metadata
            col_stats.last_analyzed_time = std::time(nullptr);
            col_stats.sample_size = sample_rows.size();
            col_stats.sample_rate = sample_rate;

            // Generate histogram (equal-height, 100 buckets)
            status = generateHistogram(column_values, 100, HistogramType::EQUAL_HEIGHT,
                                       col_stats.histogram_buckets, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to generate histogram for column " +
                             column_info.column_name);
                // Continue without histogram
            }
            else
            {
                col_stats.histogram_type = HistogramType::EQUAL_HEIGHT;
            }

            // Identify MCVs (top 100)
            status = identifyMCVs(column_values, 100, col_stats.mcv_list, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to identify MCVs for column " +
                             column_info.column_name);
                // Continue without MCVs
            }

            // Store statistics
            status = storeColumnStatistics(col_stats, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to store statistics for column " +
                             column_info.column_name);
                // Continue with other columns
            }
            else
            {
                DEBUG_LOG_DB("Successfully analyzed column " + column_info.column_name);
            }
        }

        DEBUG_LOG_DB("Successfully analyzed table with " + std::to_string(columns.size()) + " columns");

        return Status::OK;
    }

    auto StatisticsManager::analyzeColumn(const ID &table_id, const ID &column_id,
                                           float sample_rate, ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Analyzing column " << column_id.toString() << " of table " << table_id.toString());

        // Initialize catalog manager if needed
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        // Step 1: Get table columns to find target column
        std::vector<core::CatalogManager::ColumnInfo> columns;
        Status status = catalog_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table columns");
            return status;
        }

        // Step 2: Find the target column
        bool found = false;
        size_t column_index = 0;
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (std::memcmp(columns[i].column_id.bytes.data(), column_id.bytes.data(), 16) == 0)
            {
                found = true;
                column_index = i;
                break;
            }
        }

        if (!found)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column not found in table");
            return Status::NOT_FOUND;
        }

        // Step 3: Sample the table
        uint64_t sample_size = 30000; // Default sample size (PostgreSQL-style)
        std::vector<std::vector<uint8_t>> sample_rows;

        status = sampleTable(table_id, sample_size, sample_rows, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to sample table");
            return status;
        }

        DEBUG_LOG_DB("Sampled " << sample_rows.size() << " rows for column analysis");

        // Step 4: Compute statistics for this column
        ColumnStatistics col_stats;
        status = computeColumnStats(table_id, column_id, sample_rows, col_stats, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to compute column statistics");
            return status;
        }

        // Step 5: Store statistics in cache
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            uint64_t cache_key = getCacheKey(table_id, column_id);
            column_stats_cache_[cache_key] = col_stats;
        }

        // Step 6: Persist to catalog
        status = storeColumnStatistics(col_stats, ctx);
        if (status != Status::OK)
        {
            DEBUG_LOG_DB("Warning: Failed to persist column statistics to catalog");
            // Don't fail the operation if persistence fails
        }

        DEBUG_LOG_DB("Column analysis complete: " << col_stats.num_distinct
                     << " distinct values, " << col_stats.num_nulls << " nulls");

        return Status::OK;
    }

    auto StatisticsManager::getColumnStatistics(const ID &table_id, const ID &column_id,
                                                 ColumnStatistics &stats,
                                                 ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        // Check cache first
        uint64_t cache_key = getCacheKey(table_id, column_id);
        auto it = column_stats_cache_.find(cache_key);
        if (it != column_stats_cache_.end())
        {
            stats = it->second;
            return Status::OK;
        }

        // Load from catalog
        Status status = loadColumnStatistics(table_id, column_id, stats, ctx);
        if (status == Status::OK)
        {
            // Cache for future use
            column_stats_cache_[cache_key] = stats;
        }

        return status;
    }

    auto StatisticsManager::getTableStatistics(const ID &table_id, TableStatistics &stats,
                                                ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        // Check cache first
        uint64_t cache_key = 0;
        std::memcpy(&cache_key, table_id.bytes.data(), sizeof(uint64_t));

        auto it = table_stats_cache_.find(cache_key);
        if (it != table_stats_cache_.end())
        {
            stats = it->second;
            return Status::OK;
        }

        // Cache miss - load from catalog
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        // Get table information from catalog
        core::CatalogManager::TableInfo table_info;
        Status status = catalog_->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table information");
            return status;
        }

        // Build table statistics from catalog info
        stats.table_id = table_id;
        stats.table_name = table_info.table_name;
        stats.num_rows = table_info.row_count;

        // OPT-M1: Estimate num_pages from row count and average row size
        // Since TableInfo doesn't track page count directly, we estimate it
        // by calculating average row size from column definitions
        constexpr uint64_t PAGE_SIZE = 8192;          // 8KB pages
        constexpr uint64_t TUPLE_HEADER_SIZE = 24;    // Per-tuple overhead
        constexpr double PAGE_FILL_FACTOR = 0.8;      // 80% fill factor

        // Get column info to estimate row size
        std::vector<core::CatalogManager::ColumnInfo> columns;
        auto col_status = catalog_->getColumns(table_id, columns, ctx);

        double estimated_row_size = TUPLE_HEADER_SIZE;
        if (col_status == core::Status::OK && !columns.empty())
        {
            for (const auto& col : columns)
            {
                // Estimate size per column based on data type
                switch (static_cast<core::DataType>(col.data_type))
                {
                    case core::DataType::BOOLEAN:
                    case core::DataType::INT8:
                        estimated_row_size += 1;
                        break;
                    case core::DataType::INT16:
                        estimated_row_size += 2;
                        break;
                    case core::DataType::INT32:
                    case core::DataType::FLOAT32:
                    case core::DataType::DATE:
                        estimated_row_size += 4;
                        break;
                    case core::DataType::INT64:
                    case core::DataType::FLOAT64:
                    case core::DataType::TIMESTAMP:
                    case core::DataType::TIME:
                    case core::DataType::INTERVAL:
                    case core::DataType::DECFLOAT16:
                        estimated_row_size += 8;
                        break;
                    case core::DataType::UUID:
                    case core::DataType::DECIMAL:
                    case core::DataType::DECFLOAT34:
                        estimated_row_size += 16;
                        break;
                    case core::DataType::VARCHAR:
                    case core::DataType::BYTEA:
                    case core::DataType::TEXT:
                    case core::DataType::JSON:
                    case core::DataType::JSONB:
                        // Variable-length types: estimate average 20 bytes
                        estimated_row_size += 24; // 4-byte length prefix + avg 20 bytes
                        break;
                    default:
                        estimated_row_size += 8; // Default estimate
                        break;
                }
            }
        }
        else
        {
            // Fallback: use 100 bytes per row if no column info
            estimated_row_size = 100;
        }

        stats.avg_row_size = static_cast<float>(estimated_row_size);

        // Calculate number of pages needed
        if (stats.num_rows > 0)
        {
            double usable_page_size = PAGE_SIZE * PAGE_FILL_FACTOR;
            double rows_per_page = usable_page_size / estimated_row_size;
            stats.num_pages = static_cast<uint64_t>(
                std::ceil(static_cast<double>(stats.num_rows) / rows_per_page));
            // Ensure at least 1 page for non-empty tables
            if (stats.num_pages == 0)
            {
                stats.num_pages = 1;
            }
        }
        else
        {
            stats.num_pages = 0;
        }

        // Set last analyzed time (use current time as approximation)
        stats.last_analyzed_time = static_cast<uint64_t>(std::time(nullptr));

        // Cache for future use
        table_stats_cache_[cache_key] = stats;

        DEBUG_LOG_DB("Loaded table statistics: " << stats.num_rows << " rows, "
                     << stats.num_pages << " pages, avg_size=" << stats.avg_row_size);

        return Status::OK;
    }

    auto StatisticsManager::dropStatistics(const ID &table_id, ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Dropping statistics for table " << table_id.toString());

        // Step 1: Clear from cache
        invalidateCache(table_id);

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);

            // Remove table-level statistics
            uint64_t table_cache_key = 0;
            std::memcpy(&table_cache_key, table_id.bytes.data(), sizeof(uint64_t));
            table_stats_cache_.erase(table_cache_key);

            // Remove all column-level statistics for this table
            // We need to iterate through all cached columns and remove those belonging to this table
            std::vector<uint64_t> keys_to_remove;
            for (const auto& [key, col_stats] : column_stats_cache_)
            {
                if (std::memcmp(col_stats.table_id.bytes.data(), table_id.bytes.data(), 16) == 0)
                {
                    keys_to_remove.push_back(key);
                }
            }

            for (uint64_t key : keys_to_remove)
            {
                column_stats_cache_.erase(key);
            }

            DEBUG_LOG_DB("Removed " << keys_to_remove.size() << " column statistics from cache");
        }

        // Step 2: Delete from sb_statistic catalog table
        // Note: Since storeColumnStatistics may not have catalog persistence implemented yet,
        // we don't fail if catalog deletion is not available
        // Future enhancement: Delete records from sb_statistic catalog table

        DEBUG_LOG_DB("Statistics dropped successfully for table");

        return Status::OK;
    }

    auto StatisticsManager::invalidateCache(const ID &table_id) -> void
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        if (std::all_of(table_id.bytes.begin(), table_id.bytes.end(),
                        [](uint8_t b) { return b == 0; }))
        {
            // Clear entire cache if table_id is zero
            column_stats_cache_.clear();
            table_stats_cache_.clear();
            DEBUG_LOG_DB("Cleared entire statistics cache");
        }
        else
        {
            // OPT-L1: Targeted invalidation - remove only entries for this specific table
            // This preserves statistics for other tables, improving efficiency

            // Remove table-level statistics
            uint64_t table_cache_key = 0;
            std::memcpy(&table_cache_key, table_id.bytes.data(), sizeof(uint64_t));
            auto table_it = table_stats_cache_.find(table_cache_key);
            if (table_it != table_stats_cache_.end())
            {
                table_stats_cache_.erase(table_it);
            }

            // Remove column-level statistics for this table
            // The cache key is table_key XOR column_key, so we need to check each entry
            // to see if its table_id matches
            std::vector<uint64_t> keys_to_remove;
            for (const auto& [cache_key, stats] : column_stats_cache_)
            {
                if (std::memcmp(stats.table_id.bytes.data(), table_id.bytes.data(), 16) == 0)
                {
                    keys_to_remove.push_back(cache_key);
                }
            }

            for (uint64_t key : keys_to_remove)
            {
                column_stats_cache_.erase(key);
            }

            DEBUG_LOG_DB("Invalidated statistics cache for table: removed " +
                         std::to_string(keys_to_remove.size()) + " column entries");
        }
    }

    // -------------------------------------------------------------------------
    // Private Helper Methods
    // -------------------------------------------------------------------------

    auto StatisticsManager::sampleTable(const ID &table_id, uint64_t sample_size,
                                         std::vector<std::vector<uint8_t>> &sample_rows,
                                         ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Sampling table using Vitter's Algorithm S");

        // Phase 1, Task 1.1.3 - Vitter's Algorithm S (Reservoir Sampling)
        //
        // Reference: Vitter, J. S. (1985). Random Sampling with a Reservoir.
        // ACM Transactions on Mathematical Software, 11(1), 37-57.
        //
        // This algorithm provides uniform random sampling of large datasets
        // with a single pass and O(sample_size) memory.
        //
        // Time Complexity: O(n * (1 + log(N/n))) - nearly linear
        // Space Complexity: O(n) - only reservoir in memory
        // Uniformity: Each row has exactly n/N probability of selection

        sample_rows.clear();

        if (sample_size == 0)
        {
            return Status::OK; // Nothing to sample
        }

        // Create a sequential scan iterator for the table
        auto iterator = db_->storage_engine()->createScan(table_id, ctx);
        if (!iterator)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Failed to create table scan iterator");
            return Status::IO_ERROR;
        }

        // Initialize random number generator
        // SECURITY FIX (LOW-8): Use OpenSSL RAND_bytes for cryptographically secure seed
        uint64_t seed;
#ifdef HAVE_OPENSSL_RAND
        unsigned char seed_bytes[8];
        if (RAND_bytes(seed_bytes, sizeof(seed_bytes)) == 1)
        {
            // Use OpenSSL's cryptographically secure random for seed
            std::memcpy(&seed, seed_bytes, sizeof(seed));
            LOG_DEBUG(GENERAL, "Using OpenSSL RAND_bytes for statistics sampling seed");
        }
        else
        {
            // OpenSSL failed, fall back to random_device
            LOG_WARNING(GENERAL, "OpenSSL RAND_bytes failed, falling back to random_device");
            std::random_device rd;
            seed = (static_cast<uint64_t>(rd()) << 32) | rd();
        }
#else
        // OpenSSL not available, use random_device with entropy check
        std::random_device rd;
        if (rd.entropy() == 0.0)
        {
            // Fallback to time-based seed if random_device has zero entropy
            LOG_WARNING(GENERAL, "random_device has zero entropy, using time-based seed for statistics sampling");
            seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        }
        else
        {
            // Use random_device for seed
            seed = (static_cast<uint64_t>(rd()) << 32) | rd();
        }
#endif

        std::mt19937 gen(seed);
        std::uniform_real_distribution<double> uniform_real(0.0, 1.0);

        // Phase 1: Fill reservoir with first sample_size rows
        uint64_t rows_read = 0;
        Tuple tuple;

        while (rows_read < sample_size && !iterator->isDone())
        {
            Status status = iterator->next(&tuple, ctx);
            if (status != Status::OK)
            {
                if (status == Status::NOT_FOUND)
                {
                    break; // No more rows
                }
                SET_ERROR_CONTEXT(ctx, status, "Failed to read tuple during sampling");
                return status;
            }

            // Copy tuple data into sample
            std::vector<uint8_t> row_data(tuple.data, tuple.data + tuple.data_size);
            sample_rows.push_back(std::move(row_data));
            rows_read++;
        }

        DEBUG_LOG_DB("Filled reservoir with " + std::to_string(rows_read) + " rows");

        // If we read fewer rows than sample size, we're done
        if (rows_read < sample_size)
        {
            DEBUG_LOG_DB("Table has fewer rows than sample size");
            return Status::OK;
        }

        // Phase 2: Geometric skipping (Vitter's Algorithm S)
        // For each subsequent row, use geometric distribution to skip rows
        // and randomly replace items in the reservoir

        double W = std::exp(std::log(uniform_real(gen)) / static_cast<double>(sample_size));

        while (!iterator->isDone())
        {
            // Calculate number of rows to skip according to geometric distribution
            uint64_t skip = static_cast<uint64_t>(
                std::floor(std::log(uniform_real(gen)) / std::log(1.0 - W)));

            // Skip the calculated number of rows
            for (uint64_t i = 0; i <= skip && !iterator->isDone(); i++)
            {
                Status status = iterator->next(&tuple, ctx);
                if (status == Status::NOT_FOUND)
                {
                    // Reached end of table
                    DEBUG_LOG_DB("Completed sampling: " + std::to_string(rows_read) +
                                 " rows scanned, " + std::to_string(sample_rows.size()) +
                                 " rows in sample");
                    return Status::OK;
                }
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to read tuple during sampling");
                    return status;
                }
                rows_read++;
            }

            if (iterator->isDone())
            {
                break;
            }

            // Replace a random item in the reservoir with the current row
            std::uniform_int_distribution<uint64_t> uniform_int(0, sample_size - 1);
            uint64_t replace_idx = uniform_int(gen);

            std::vector<uint8_t> row_data(tuple.data, tuple.data + tuple.data_size);
            sample_rows[replace_idx] = std::move(row_data);

            // Update W for next iteration
            W = W * std::exp(std::log(uniform_real(gen)) / static_cast<double>(sample_size));
        }

        DEBUG_LOG_DB("Completed sampling: " + std::to_string(rows_read) +
                     " rows scanned, " + std::to_string(sample_rows.size()) + " rows in sample");

        return Status::OK;
    }

    auto StatisticsManager::computeColumnStats(const ID &table_id, const ID &column_id,
                                                const std::vector<std::vector<uint8_t>> &sample_rows,
                                                ColumnStatistics &stats,
                                                ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Computing column statistics from sample");

        // Phase 1, Task 1.1.4 - Column statistics computation
        //
        // Steps:
        // 1. Get table schema from catalog
        // 2. Extract column values from sample_rows
        // 3. Compute null_fraction (count NULLs / total)
        // 4. Estimate n_distinct (using exact count or HyperLogLog)
        // 5. Compute avg_width (average bytes per value)
        // 6. Identify Most Common Values (top 100 by frequency)
        // 7. Generate histogram (equal-height or equal-width)
        // 8. Set metadata (last_analyzed_time, sample_size, sample_rate)

        if (sample_rows.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty sample");
            return Status::INVALID_ARGUMENT;
        }

        // Get table and column information from catalog
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        std::vector<core::CatalogManager::ColumnInfo> columns;
        Status status = catalog_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table columns");
            return status;
        }

        // Find the target column
        size_t target_column_idx = SIZE_MAX;
        core::CatalogManager::ColumnInfo target_column_info;

        for (size_t i = 0; i < columns.size(); i++)
        {
            if (std::memcmp(columns[i].column_id.bytes.data(), column_id.bytes.data(), 16) == 0)
            {
                target_column_idx = i;
                target_column_info = columns[i];
                break;
            }
        }

        if (target_column_idx == SIZE_MAX)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column not found in table");
            return Status::NOT_FOUND;
        }

        // Extract column values from sample rows
        std::vector<std::vector<uint8_t>> column_values;
        column_values.reserve(sample_rows.size());
        uint64_t null_count = 0;
        uint64_t total_width = 0;

        for (const auto &tuple_data : sample_rows)
        {
            if (tuple_data.size() < sizeof(core::TupleHeader))
            {
                continue; // Skip malformed tuples
            }

            // Read TupleHeader
            const auto *header = reinterpret_cast<const core::TupleHeader *>(tuple_data.data());

            // Get null bitmap if present
            const uint8_t *null_bitmap = nullptr;
            if (header->hasNulls() && header->null_bitmap_offset > 0 &&
                header->null_bitmap_offset < tuple_data.size())
            {
                null_bitmap = tuple_data.data() + header->null_bitmap_offset;
            }

            // Check if our target column is null
            bool is_null = false;
            if (null_bitmap)
            {
                size_t byte_offset = target_column_idx / 8;
                size_t bit_pos = target_column_idx % 8;
                is_null = (null_bitmap[byte_offset] & (1 << bit_pos)) != 0;
            }

            if (is_null)
            {
                null_count++;
                column_values.push_back(std::vector<uint8_t>()); // Empty vector for NULL
                continue;
            }

            // Calculate data offset for our target column
            size_t data_offset = sizeof(core::TupleHeader);
            if (header->hasNulls() && null_bitmap)
            {
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                data_offset = header->null_bitmap_offset + bitmap_bytes;
            }

            bool malformed = false;
            core::DomainManager *domain_mgr = db_->domain_manager();

            // Skip columns before our target
            for (size_t i = 0; i < target_column_idx; i++)
            {
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    if (null_bitmap[byte_offset] & (1 << bit_pos))
                    {
                        continue; // Null column, no data to skip
                    }
                }

                bool encrypted = false;
                core::ErrorContext enc_ctx;
                if (!resolveColumnEncryption(domain_mgr, columns[i], encrypted, &enc_ctx))
                {
                    malformed = true;
                    break;
                }

                size_t col_size = 0;
                if (encrypted)
                {
                    size_t len_offset = data_offset;
                    uint32_t len = 0;
                    if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                    {
                        malformed = true;
                        break;
                    }
                    col_size = sizeof(uint32_t) + len;
                }
                else
                {
                    core::TypeInfo type_info = buildTypeInfo(columns[i]);
                    core::ErrorContext size_ctx;
                    core::Status size_status = core::computePlainValueSize(type_info.type,
                                                                           type_info,
                                                                           tuple_data.data() + data_offset,
                                                                           tuple_data.size() - data_offset,
                                                                           col_size,
                                                                           &size_ctx);
                    if (size_status != core::Status::OK)
                    {
                        malformed = true;
                        break;
                    }
                }

                if (data_offset + col_size > tuple_data.size())
                {
                    malformed = true;
                    break;
                }
                data_offset += col_size;
            }

            if (malformed)
            {
                continue;
            }

            // Extract the target column value
            core::DataType target_type = static_cast<core::DataType>(target_column_info.data_type);
            std::vector<uint8_t> value;
            size_t value_size = 0;
            size_t payload_width = 0;

            bool target_encrypted = false;
            core::ErrorContext enc_ctx;
            if (!resolveColumnEncryption(domain_mgr, target_column_info, target_encrypted, &enc_ctx))
            {
                continue;
            }

            if (target_encrypted)
            {
                size_t len_offset = data_offset;
                uint32_t len = 0;
                if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                {
                    continue;
                }
                value_size = sizeof(uint32_t) + len;
                payload_width = len;
            }
            else
            {
                core::TypeInfo type_info = buildTypeInfo(target_column_info);
                core::ErrorContext size_ctx;
                core::Status size_status = core::computePlainValueSize(type_info.type,
                                                                       type_info,
                                                                       tuple_data.data() + data_offset,
                                                                       tuple_data.size() - data_offset,
                                                                       value_size,
                                                                       &size_ctx);
                if (size_status != core::Status::OK)
                {
                    continue;
                }
                if (isLengthPrefixedType(target_type))
                {
                    size_t len_offset = data_offset;
                    uint32_t len = 0;
                    if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                    {
                        continue;
                    }
                    payload_width = len;
                }
                else
                {
                    payload_width = value_size;
                }
            }

            if (data_offset + value_size > tuple_data.size())
            {
                continue;
            }

            value.resize(value_size);
            std::memcpy(value.data(), tuple_data.data() + data_offset, value_size);
            total_width += payload_width;

            column_values.push_back(std::move(value));
        }

        // Compute statistics
        stats.table_id = table_id;
        stats.column_id = column_id;
        stats.column_name = target_column_info.column_name;
        stats.data_type = static_cast<core::DataType>(target_column_info.data_type);

        stats.num_rows = sample_rows.size();
        stats.num_nulls = null_count;
        stats.null_fraction = static_cast<float>(null_count) / static_cast<float>(sample_rows.size());

        // Compute avg_width (average bytes per non-null value)
        uint64_t non_null_count = sample_rows.size() - null_count;
        if (non_null_count > 0)
        {
            stats.avg_width = static_cast<float>(total_width) / static_cast<float>(non_null_count);
        }
        else
        {
            stats.avg_width = 0.0f;
        }

        // Estimate n_distinct
        // For now, we'll use exact counting; HyperLogLog can be added later
        stats.num_distinct = estimateNDistinct(column_values, stats.num_rows, sample_rows.size());

        // Set metadata
        stats.last_analyzed_time = std::time(nullptr);
        stats.sample_size = sample_rows.size();
        stats.sample_rate = 0.0f; // Will be set by caller

        DEBUG_LOG_DB("Column statistics computed: " + std::to_string(stats.num_rows) + " rows, " +
                     std::to_string(stats.num_nulls) + " nulls, " +
                     std::to_string(stats.num_distinct) + " distinct");

        return Status::OK;
    }

    auto StatisticsManager::generateHistogram(const std::vector<std::vector<uint8_t>> &values,
                                               uint32_t bucket_count,
                                               HistogramType histogram_type,
                                               std::vector<HistogramBucket> &buckets,
                                               ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Generating histogram");

        // Phase 1, Task 1.1.5 - Histogram generation
        //
        // Two types supported:
        // - Equal-Height: Buckets contain ~equal number of values (PostgreSQL-style)
        // - Equal-Width: Buckets span equal value ranges (MySQL-style)

        buckets.clear();

        if (values.empty() || bucket_count == 0)
        {
            return Status::OK; // No histogram to generate
        }

        // Filter out NULL values (represented as empty vectors)
        std::vector<std::vector<uint8_t>> non_null_values;
        non_null_values.reserve(values.size());
        for (const auto &value : values)
        {
            if (!value.empty())
            {
                non_null_values.push_back(value);
            }
        }

        if (non_null_values.empty())
        {
            return Status::OK; // All values are NULL
        }

        // Cap bucket count at number of distinct values
        if (bucket_count > non_null_values.size())
        {
            bucket_count = non_null_values.size();
        }

        if (histogram_type == HistogramType::EQUAL_HEIGHT)
        {
            // Equal-Height Histogram (PostgreSQL-style)
            // Sort values and divide into buckets with ~equal number of values

            // Sort values
            std::vector<std::vector<uint8_t>> sorted_values = non_null_values;
            std::sort(sorted_values.begin(), sorted_values.end());

            // Calculate values per bucket
            size_t values_per_bucket = sorted_values.size() / bucket_count;
            size_t remainder = sorted_values.size() % bucket_count;

            buckets.reserve(bucket_count);
            size_t idx = 0;

            for (uint32_t i = 0; i < bucket_count; i++)
            {
                HistogramBucket bucket;

                // Calculate how many values in this bucket
                size_t bucket_size = values_per_bucket + (i < remainder ? 1 : 0);

                if (bucket_size == 0)
                    break;

                // Set lower bound (first value in bucket)
                const auto &lower = sorted_values[idx];
                size_t lower_size = std::min(lower.size(), sizeof(bucket.lower_bound));
                std::memcpy(bucket.lower_bound, lower.data(), lower_size);
                if (lower_size < sizeof(bucket.lower_bound))
                {
                    std::memset(bucket.lower_bound + lower_size, 0,
                                sizeof(bucket.lower_bound) - lower_size);
                }

                // Set upper bound (last value in bucket)
                const auto &upper = sorted_values[idx + bucket_size - 1];
                size_t upper_size = std::min(upper.size(), sizeof(bucket.upper_bound));
                std::memcpy(bucket.upper_bound, upper.data(), upper_size);
                if (upper_size < sizeof(bucket.upper_bound))
                {
                    std::memset(bucket.upper_bound + upper_size, 0,
                                sizeof(bucket.upper_bound) - upper_size);
                }

                // Set row count and frequency
                bucket.row_count = bucket_size;
                bucket.frequency = static_cast<float>(bucket_size) / static_cast<float>(sorted_values.size());

                // TOAST references (0 = inline data, not TOASTed)
                bucket.lower_oid = 0;
                bucket.upper_oid = 0;

                buckets.push_back(bucket);
                idx += bucket_size;
            }

            DEBUG_LOG_DB("Generated equal-height histogram with " + std::to_string(buckets.size()) + " buckets");
        }
        else if (histogram_type == HistogramType::EQUAL_WIDTH)
        {
            // Equal-Width Histogram (MySQL-style)
            // Divide value range into equal intervals

            // Find min and max values
            auto min_value = *std::min_element(non_null_values.begin(), non_null_values.end());
            auto max_value = *std::max_element(non_null_values.begin(), non_null_values.end());

            // For discrete types or single-value columns, fall back to equal-height
            if (min_value == max_value)
            {
                HistogramBucket bucket;

                size_t value_size = std::min(min_value.size(), sizeof(bucket.lower_bound));
                std::memcpy(bucket.lower_bound, min_value.data(), value_size);
                std::memcpy(bucket.upper_bound, min_value.data(), value_size);

                if (value_size < sizeof(bucket.lower_bound))
                {
                    std::memset(bucket.lower_bound + value_size, 0,
                                sizeof(bucket.lower_bound) - value_size);
                    std::memset(bucket.upper_bound + value_size, 0,
                                sizeof(bucket.upper_bound) - value_size);
                }

                bucket.row_count = non_null_values.size();
                bucket.frequency = 1.0f;
                bucket.lower_oid = 0;
                bucket.upper_oid = 0;

                buckets.push_back(bucket);
                DEBUG_LOG_DB("Generated single-bucket histogram (all values equal)");
                return Status::OK;
            }

            // For now, equal-width is primarily useful for numeric types
            // For complex types, we fall back to equal-height
            // Phase 4 Enhancement: Implement proper equal-width for numeric types with range calculation
            DEBUG_LOG_DB("Equal-width histogram for complex types not yet implemented, using equal-height");
            return generateHistogram(values, bucket_count, HistogramType::EQUAL_HEIGHT, buckets, ctx);
        }
        else
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown histogram type");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    auto StatisticsManager::identifyMCVs(const std::vector<std::vector<uint8_t>> &values,
                                          uint32_t max_mcv_count,
                                          std::vector<MCVEntry> &mcv_list,
                                          ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Identifying Most Common Values");

        // Phase 1, Task 1.1.6 - MCV identification
        //
        // Algorithm:
        // 1. Build frequency map (value -> count)
        // 2. Sort by frequency (descending)
        // 3. Take top max_mcv_count entries
        // 4. Compute frequency as fraction of total values
        // 5. Store in mcv_list

        mcv_list.clear();

        if (values.empty() || max_mcv_count == 0)
        {
            return Status::OK; // No MCVs to identify
        }

        // Build frequency map
        std::unordered_map<std::vector<uint8_t>, uint64_t, VectorHash> frequency_map;

        uint64_t total_non_null = 0;
        for (const auto &value : values)
        {
            if (!value.empty()) // Skip NULLs
            {
                frequency_map[value]++;
                total_non_null++;
            }
        }

        if (frequency_map.empty())
        {
            return Status::OK; // All values are NULL
        }

        // Convert to vector for sorting
        std::vector<std::pair<std::vector<uint8_t>, uint64_t>> freq_vector;
        freq_vector.reserve(frequency_map.size());

        for (const auto &entry : frequency_map)
        {
            freq_vector.push_back(entry);
        }

        // Sort by frequency (descending)
        std::sort(freq_vector.begin(), freq_vector.end(),
                  [](const auto &a, const auto &b)
                  {
                      return a.second > b.second; // Higher frequency first
                  });

        // Take top max_mcv_count entries
        size_t mcv_count = std::min(static_cast<size_t>(max_mcv_count), freq_vector.size());

        mcv_list.reserve(mcv_count);

        for (size_t i = 0; i < mcv_count; i++)
        {
            const auto &value = freq_vector[i].first;
            uint64_t count = freq_vector[i].second;

            MCVEntry mcv;

            // Copy value data
            size_t value_size = std::min(value.size(), sizeof(mcv.value_data));
            std::memcpy(mcv.value_data, value.data(), value_size);

            // Zero out remaining bytes
            if (value_size < sizeof(mcv.value_data))
            {
                std::memset(mcv.value_data + value_size, 0,
                            sizeof(mcv.value_data) - value_size);
            }

            // Compute frequency as fraction
            mcv.frequency = static_cast<float>(count) / static_cast<float>(total_non_null);

            // TOAST reference (0 = inline data, not TOASTed)
            mcv.value_oid = 0;

            mcv_list.push_back(mcv);
        }

        DEBUG_LOG_DB("Identified " + std::to_string(mcv_list.size()) + " most common values");

        return Status::OK;
    }

    auto StatisticsManager::estimateNDistinct(const std::vector<std::vector<uint8_t>> &values,
                                               uint64_t total_rows,
                                               uint64_t sample_size) -> uint64_t
    {
        DEBUG_LOG_DB("Estimating number of distinct values");

        // Phase 1, Task 1.1.7 - n_distinct estimation
        //
        // For now, use exact count in sample with simple extrapolation
        // Future enhancement: HyperLogLog for large cardinality estimation

        if (values.empty())
        {
            return 0;
        }

        // Count distinct values in sample using a hash set
        std::unordered_set<std::vector<uint8_t>, VectorHash> distinct_values;

        for (const auto &value : values)
        {
            if (!value.empty()) // Skip NULLs (represented as empty vectors)
            {
                distinct_values.insert(value);
            }
        }

        uint64_t distinct_in_sample = distinct_values.size();

        // If we sampled the entire table, return exact count
        if (sample_size >= total_rows)
        {
            return distinct_in_sample;
        }

        // For small distinct counts relative to sample size, likely saw everything
        if (distinct_in_sample < 100 || distinct_in_sample < sample_size / 10)
        {
            return distinct_in_sample;
        }

        // Otherwise, use linear extrapolation with a cap
        // n_distinct_estimate = distinct_in_sample * (total_rows / sample_size)
        //
        // But cap at total_rows since we can't have more distinct values than rows
        double sample_fraction = static_cast<double>(sample_size) / static_cast<double>(total_rows);
        uint64_t estimate = static_cast<uint64_t>(
            static_cast<double>(distinct_in_sample) / sample_fraction);

        // Cap at total rows
        if (estimate > total_rows)
        {
            estimate = total_rows;
        }

        DEBUG_LOG_DB("Estimated " + std::to_string(estimate) + " distinct values from " +
                     std::to_string(distinct_in_sample) + " in sample");

        return estimate;
    }

    auto StatisticsManager::storeColumnStatistics(const ColumnStatistics &stats,
                                                   ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Storing column statistics to catalog");

        // OPT-1: Statistics persistence implementation
        //
        // Steps:
        // 1. Serialize MCVs to JSON and store via TOAST if large
        // 2. Serialize histogram to JSON and store via TOAST if large
        // 3. Create StatisticInfo with basic stats + TOAST refs
        // 4. Write to sb_statistic catalog via CatalogManager
        // 5. Update in-memory cache

        // Ensure catalog is initialized
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        // Create StatisticInfo from ColumnStatistics
        core::CatalogManager::StatisticInfo info;
        info.statistic_id = core::generateUuidV7();  // Generate new ID
        info.table_id = stats.table_id;
        info.column_id = stats.column_id;
        info.data_type = static_cast<uint16_t>(stats.data_type);
        info.num_rows = stats.num_rows;
        info.num_nulls = stats.num_nulls;
        info.null_fraction = stats.null_fraction;
        info.num_distinct = stats.num_distinct;
        info.avg_width = stats.avg_width;
        info.histogram_type = static_cast<uint8_t>(stats.histogram_type);
        info.histogram_bucket_count = stats.histogram_bucket_count;
        info.last_analyzed_time = stats.last_analyzed_time;
        info.sample_size = stats.sample_size;
        info.sample_rate = stats.sample_rate;

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        info.created_time = now_time;
        info.last_modified_time = now_time;

        // Serialize MCVs to JSON format for TOAST storage
        // Format: [{"value":"base64","freq":0.5}, ...]
        if (!stats.mcv_list.empty())
        {
            std::string mcv_json = "[";
            for (size_t i = 0; i < stats.mcv_list.size(); ++i)
            {
                if (i > 0) mcv_json += ",";
                const auto& mcv = stats.mcv_list[i];

                // Convert value_data to hex string (simpler than base64)
                std::string hex_value;
                for (size_t j = 0; j < 256 && mcv.value_data[j] != 0; ++j)
                {
                    char buf[3];
                    snprintf(buf, sizeof(buf), "%02x", mcv.value_data[j]);
                    hex_value += buf;
                }

                mcv_json += "{\"v\":\"" + hex_value + "\",\"f\":" +
                           std::to_string(mcv.frequency) + "}";
            }
            mcv_json += "]";

            // Store via TOAST and get OID (xmin=0 for catalog operations)
            Status status = catalog_->storeStringInToast(mcv_json, 0, info.mcv_oid, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to store MCV list via TOAST");
                // Continue anyway - stats will work without MCVs
                info.mcv_oid = 0;
            }
        }

        // Serialize histogram to JSON format for TOAST storage
        // Format: [{"lo":"hex","hi":"hex","cnt":100,"freq":0.1}, ...]
        if (!stats.histogram_buckets.empty())
        {
            std::string hist_json = "[";
            for (size_t i = 0; i < stats.histogram_buckets.size(); ++i)
            {
                if (i > 0) hist_json += ",";
                const auto& bucket = stats.histogram_buckets[i];

                // Convert bounds to hex strings
                std::string lo_hex, hi_hex;
                for (size_t j = 0; j < 256; ++j)
                {
                    if (bucket.lower_bound[j] == 0 && j > 0) break;
                    char buf[3];
                    snprintf(buf, sizeof(buf), "%02x", bucket.lower_bound[j]);
                    lo_hex += buf;
                }
                for (size_t j = 0; j < 256; ++j)
                {
                    if (bucket.upper_bound[j] == 0 && j > 0) break;
                    char buf[3];
                    snprintf(buf, sizeof(buf), "%02x", bucket.upper_bound[j]);
                    hi_hex += buf;
                }

                hist_json += "{\"lo\":\"" + lo_hex + "\",\"hi\":\"" + hi_hex +
                            "\",\"cnt\":" + std::to_string(bucket.row_count) +
                            ",\"f\":" + std::to_string(bucket.frequency) + "}";
            }
            hist_json += "]";

            // Store via TOAST and get OID (xmin=0 for catalog operations)
            Status status = catalog_->storeStringInToast(hist_json, 0, info.histogram_oid, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to store histogram via TOAST");
                // Continue anyway - stats will work without histogram
                info.histogram_oid = 0;
            }
        }

        // Store to catalog
        Status status = catalog_->storeStatistic(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to store statistics to catalog");
            return status;
        }

        // Also update in-memory cache for fast access
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            uint64_t cache_key = getCacheKey(stats.table_id, stats.column_id);
            column_stats_cache_[cache_key] = stats;
        }

        DEBUG_LOG_DB("Stored column statistics to catalog for table_id=" +
                     std::to_string(*reinterpret_cast<const uint64_t*>(stats.table_id.bytes.data())));

        return Status::OK;
    }

    auto StatisticsManager::loadColumnStatistics(const ID &table_id, const ID &column_id,
                                                  ColumnStatistics &stats,
                                                  ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Loading column statistics from catalog");

        // OPT-2: Statistics loading from persistent storage
        //
        // Steps:
        // 1. Read StatisticInfo from sb_statistic catalog via CatalogManager
        // 2. Load MCVs from TOAST if OID is set
        // 3. Load histogram from TOAST if OID is set
        // 4. Deserialize into ColumnStatistics struct
        // 5. Cache for fast access

        // Ensure catalog is initialized
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        // Try to load from catalog
        core::CatalogManager::StatisticInfo info;
        Status status = catalog_->getStatistic(table_id, column_id, info, ctx);
        if (status != Status::OK)
        {
            // Not found in catalog - this is normal for columns that haven't been analyzed
            return status;
        }

        // Convert StatisticInfo to ColumnStatistics
        stats.table_id = info.table_id;
        stats.column_id = info.column_id;
        stats.data_type = static_cast<core::DataType>(info.data_type);
        stats.num_rows = info.num_rows;
        stats.num_nulls = info.num_nulls;
        stats.null_fraction = info.null_fraction;
        stats.num_distinct = info.num_distinct;
        stats.avg_width = info.avg_width;
        stats.histogram_type = static_cast<HistogramType>(info.histogram_type);
        stats.histogram_bucket_count = info.histogram_bucket_count;
        stats.last_analyzed_time = info.last_analyzed_time;
        stats.sample_size = info.sample_size;
        stats.sample_rate = info.sample_rate;

        // Load MCVs from TOAST if available
        if (info.mcv_oid != 0)
        {
            std::string json;
            status = catalog_->loadStringFromToast(info.mcv_oid, 0, json, ctx);
            if (status == Status::OK && !json.empty())
            {
                // Parse JSON: [{"v":"hex","f":0.5}, ...]
                stats.mcv_list.clear();

                // Simple JSON parser for MCV entries
                size_t pos = 0;
                while ((pos = json.find("{\"v\":\"", pos)) != std::string::npos)
                {
                    pos += 6; // Skip {"v":"
                    size_t end = json.find("\"", pos);
                    if (end == std::string::npos) break;

                    std::string hex_value = json.substr(pos, end - pos);
                    pos = end;

                    // Find frequency
                    size_t freq_pos = json.find("\"f\":", pos);
                    if (freq_pos == std::string::npos) break;
                    freq_pos += 4; // Skip "f":

                    size_t freq_end = json.find_first_of(",}", freq_pos);
                    if (freq_end == std::string::npos) break;

                    float freq = std::stof(json.substr(freq_pos, freq_end - freq_pos));

                    // Convert hex to bytes
                    MCVEntry entry;
                    size_t byte_idx = 0;
                    for (size_t i = 0; i < hex_value.size() && byte_idx < 256; i += 2)
                    {
                        unsigned int byte_val;
                        sscanf(hex_value.c_str() + i, "%02x", &byte_val);
                        entry.value_data[byte_idx++] = static_cast<uint8_t>(byte_val);
                    }
                    entry.frequency = freq;
                    stats.mcv_list.push_back(entry);

                    pos = freq_end;
                }
            }
        }

        // Load histogram from TOAST if available
        if (info.histogram_oid != 0)
        {
            std::string json;
            status = catalog_->loadStringFromToast(info.histogram_oid, 0, json, ctx);
            if (status == Status::OK && !json.empty())
            {
                // Parse JSON: [{"lo":"hex","hi":"hex","cnt":100,"f":0.1}, ...]
                stats.histogram_buckets.clear();

                // Simple JSON parser for histogram entries
                size_t pos = 0;
                while ((pos = json.find("{\"lo\":\"", pos)) != std::string::npos)
                {
                    pos += 7; // Skip {"lo":"
                    size_t end = json.find("\"", pos);
                    if (end == std::string::npos) break;

                    std::string lo_hex = json.substr(pos, end - pos);
                    pos = end;

                    // Find hi
                    size_t hi_pos = json.find("\"hi\":\"", pos);
                    if (hi_pos == std::string::npos) break;
                    hi_pos += 6;
                    size_t hi_end = json.find("\"", hi_pos);
                    if (hi_end == std::string::npos) break;

                    std::string hi_hex = json.substr(hi_pos, hi_end - hi_pos);
                    pos = hi_end;

                    // Find cnt
                    size_t cnt_pos = json.find("\"cnt\":", pos);
                    if (cnt_pos == std::string::npos) break;
                    cnt_pos += 6;
                    size_t cnt_end = json.find(",", cnt_pos);
                    if (cnt_end == std::string::npos) break;

                    uint64_t cnt = std::stoull(json.substr(cnt_pos, cnt_end - cnt_pos));
                    pos = cnt_end;

                    // Find f
                    size_t f_pos = json.find("\"f\":", pos);
                    if (f_pos == std::string::npos) break;
                    f_pos += 4;
                    size_t f_end = json.find_first_of(",}", f_pos);
                    if (f_end == std::string::npos) break;

                    float freq = std::stof(json.substr(f_pos, f_end - f_pos));

                    // Create bucket
                    HistogramBucket bucket;

                    // Convert hex to bytes for lower bound
                    size_t byte_idx = 0;
                    for (size_t i = 0; i < lo_hex.size() && byte_idx < 256; i += 2)
                    {
                        unsigned int byte_val;
                        sscanf(lo_hex.c_str() + i, "%02x", &byte_val);
                        bucket.lower_bound[byte_idx++] = static_cast<uint8_t>(byte_val);
                    }

                    // Convert hex to bytes for upper bound
                    byte_idx = 0;
                    for (size_t i = 0; i < hi_hex.size() && byte_idx < 256; i += 2)
                    {
                        unsigned int byte_val;
                        sscanf(hi_hex.c_str() + i, "%02x", &byte_val);
                        bucket.upper_bound[byte_idx++] = static_cast<uint8_t>(byte_val);
                    }

                    bucket.row_count = cnt;
                    bucket.frequency = freq;
                    stats.histogram_buckets.push_back(bucket);

                    pos = f_end;
                }
            }
        }

        // Cache for fast access
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            uint64_t cache_key = getCacheKey(table_id, column_id);
            column_stats_cache_[cache_key] = stats;
        }

        DEBUG_LOG_DB("Loaded column statistics from catalog for table_id=" +
                     std::to_string(*reinterpret_cast<const uint64_t*>(table_id.bytes.data())));

        return Status::OK;
    }

    auto StatisticsManager::getCacheKey(const ID &table_id, const ID &column_id) -> uint64_t
    {
        // Combine table_id and column_id into a single uint64_t key
        // Use XOR of first 8 bytes of each ID
        uint64_t table_key = 0;
        uint64_t column_key = 0;
        std::memcpy(&table_key, table_id.bytes.data(), sizeof(uint64_t));
        std::memcpy(&column_key, column_id.bytes.data(), sizeof(uint64_t));
        return table_key ^ column_key;
    }

    auto StatisticsManager::extractColumnValues(
        const ID &table_id,
        const ID &column_id,
        const std::vector<std::vector<uint8_t>> &sample_rows,
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        ErrorContext *ctx) -> std::vector<std::vector<uint8_t>>
    {
        std::vector<std::vector<uint8_t>> column_values;
        column_values.reserve(sample_rows.size());

        // Find the target column index
        size_t target_column_idx = SIZE_MAX;
        core::CatalogManager::ColumnInfo target_column_info;

        for (size_t i = 0; i < columns.size(); i++)
        {
            if (std::memcmp(columns[i].column_id.bytes.data(), column_id.bytes.data(), 16) == 0)
            {
                target_column_idx = i;
                target_column_info = columns[i];
                break;
            }
        }

        if (target_column_idx == SIZE_MAX)
        {
            return column_values; // Empty vector
        }

        std::vector<bool> encryption_flags(columns.size(), false);
        auto *domain_mgr = db_->domain_manager();
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (columns[i].domain_id == core::ID{})
            {
                continue;
            }
            if (domain_mgr == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Domain manager unavailable for encryption lookup");
                return {};
            }
            core::DomainInfo domain;
            Status status = domain_mgr->getDomain(columns[i].domain_id, domain, ctx);
            if (status != Status::OK)
            {
                return {};
            }
            encryption_flags[i] = domain.security.encryption_enabled;
        }

        // Extract column values from each tuple
        for (const auto &tuple_data : sample_rows)
        {
            if (tuple_data.size() < sizeof(core::TupleHeader))
            {
                continue; // Skip malformed tuples
            }

            // Read TupleHeader
            const auto *header = reinterpret_cast<const core::TupleHeader *>(tuple_data.data());

            // Get null bitmap if present
            const uint8_t *null_bitmap = nullptr;
            if (header->hasNulls() && header->null_bitmap_offset > 0 &&
                header->null_bitmap_offset < tuple_data.size())
            {
                null_bitmap = tuple_data.data() + header->null_bitmap_offset;
            }

            // Check if target column is null
            bool is_null = false;
            if (null_bitmap)
            {
                size_t byte_offset = target_column_idx / 8;
                size_t bit_pos = target_column_idx % 8;
                is_null = (null_bitmap[byte_offset] & (1 << bit_pos)) != 0;
            }

            if (is_null)
            {
                column_values.push_back(std::vector<uint8_t>()); // Empty vector for NULL
                continue;
            }

            // Calculate data offset for target column
            size_t data_offset = sizeof(core::TupleHeader);
            if (header->hasNulls() && null_bitmap)
            {
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                data_offset = header->null_bitmap_offset + bitmap_bytes;
            }

            bool malformed = false;

            // Skip columns before target
            for (size_t i = 0; i < target_column_idx; i++)
            {
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    if (null_bitmap[byte_offset] & (1 << bit_pos))
                    {
                        continue;
                    }
                }

                size_t col_size = 0;
                if (encryption_flags[i])
                {
                    size_t len_offset = data_offset;
                    uint32_t len = 0;
                    if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                    {
                        malformed = true;
                        break;
                    }
                    col_size = sizeof(uint32_t) + len;
                }
                else
                {
                    core::TypeInfo type_info = buildTypeInfo(columns[i]);
                    core::ErrorContext size_ctx;
                    core::Status size_status = core::computePlainValueSize(type_info.type,
                                                                           type_info,
                                                                           tuple_data.data() + data_offset,
                                                                           tuple_data.size() - data_offset,
                                                                           col_size,
                                                                           &size_ctx);
                    if (size_status != core::Status::OK)
                    {
                        malformed = true;
                        break;
                    }
                }

                if (data_offset + col_size > tuple_data.size())
                {
                    malformed = true;
                    break;
                }
                data_offset += col_size;
            }

            if (malformed)
            {
                continue;
            }

            core::DataType target_type = static_cast<core::DataType>(target_column_info.data_type);
            std::vector<uint8_t> value;
            size_t value_size = 0;

            if (encryption_flags[target_column_idx])
            {
                size_t len_offset = data_offset;
                uint32_t len = 0;
                if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                {
                    continue;
                }
                value_size = sizeof(uint32_t) + len;
            }
            else
            {
                core::TypeInfo type_info = buildTypeInfo(target_column_info);
                core::ErrorContext size_ctx;
                core::Status size_status = core::computePlainValueSize(type_info.type,
                                                                       type_info,
                                                                       tuple_data.data() + data_offset,
                                                                       tuple_data.size() - data_offset,
                                                                       value_size,
                                                                       &size_ctx);
                if (size_status != core::Status::OK)
                {
                    continue;
                }
            }

            if (data_offset + value_size > tuple_data.size())
            {
                continue;
            }

            value.resize(value_size);
            std::memcpy(value.data(), tuple_data.data() + data_offset, value_size);

            column_values.push_back(std::move(value));
        }

        return column_values;
    }

} // namespace scratchbird::optimizer
