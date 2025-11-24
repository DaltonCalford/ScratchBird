#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/logger.h"
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
        stats.num_rows = table_info.tuple_count;
        stats.num_pages = table_info.num_pages;

        // Calculate average row size
        if (stats.num_rows > 0 && stats.num_pages > 0)
        {
            // Estimate avg row size from pages and tuple count
            constexpr uint64_t PAGE_SIZE = 8192; // 8KB pages
            uint64_t total_bytes = stats.num_pages * PAGE_SIZE;
            stats.avg_row_size = static_cast<float>(total_bytes) / stats.num_rows;
        }
        else
        {
            stats.avg_row_size = 0.0f;
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

        // Step 2: Delete from pg_statistic catalog table
        // Note: Since storeColumnStatistics may not have catalog persistence implemented yet,
        // we don't fail if catalog deletion is not available
        // Future enhancement: Delete records from pg_statistic catalog table

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
            // TODO: Remove only entries for this specific table
            // For now, clear entire cache
            column_stats_cache_.clear();
            table_stats_cache_.clear();
            DEBUG_LOG_DB("Invalidated statistics cache for table");
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

            // Skip columns before our target
            for (size_t i = 0; i < target_column_idx; i++)
            {
                // Check if this column is null
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    if (null_bitmap[byte_offset] & (1 << bit_pos))
                    {
                        continue; // Null column, no data to skip
                    }
                }

                // Skip this column's data based on its type
                core::DataType col_type = static_cast<core::DataType>(columns[i].data_type);
                if (col_type == core::DataType::INT32)
                {
                    data_offset += sizeof(int32_t);
                }
                else if (col_type == core::DataType::INT64)
                {
                    data_offset += sizeof(int64_t);
                }
                else if (col_type == core::DataType::FLOAT64)
                {
                    data_offset += sizeof(double);
                }
                else if (col_type == core::DataType::VARCHAR)
                {
                    // VARCHAR has length prefix
                    if (data_offset + sizeof(uint32_t) > tuple_data.size())
                        break;
                    uint32_t len;
                    std::memcpy(&len, tuple_data.data() + data_offset, sizeof(uint32_t));
                    data_offset += sizeof(uint32_t) + len;
                }
                // TODO: Add support for other types as needed
            }

            // Extract the target column value
            core::DataType target_type = static_cast<core::DataType>(target_column_info.data_type);
            std::vector<uint8_t> value;

            if (target_type == core::DataType::INT32 && data_offset + sizeof(int32_t) <= tuple_data.size())
            {
                value.resize(sizeof(int32_t));
                std::memcpy(value.data(), tuple_data.data() + data_offset, sizeof(int32_t));
                total_width += sizeof(int32_t);
            }
            else if (target_type == core::DataType::INT64 && data_offset + sizeof(int64_t) <= tuple_data.size())
            {
                value.resize(sizeof(int64_t));
                std::memcpy(value.data(), tuple_data.data() + data_offset, sizeof(int64_t));
                total_width += sizeof(int64_t);
            }
            else if (target_type == core::DataType::FLOAT64 && data_offset + sizeof(double) <= tuple_data.size())
            {
                value.resize(sizeof(double));
                std::memcpy(value.data(), tuple_data.data() + data_offset, sizeof(double));
                total_width += sizeof(double);
            }
            else if (target_type == core::DataType::VARCHAR && data_offset + sizeof(uint32_t) <= tuple_data.size())
            {
                uint32_t len;
                std::memcpy(&len, tuple_data.data() + data_offset, sizeof(uint32_t));
                if (data_offset + sizeof(uint32_t) + len <= tuple_data.size())
                {
                    value.resize(sizeof(uint32_t) + len);
                    std::memcpy(value.data(), tuple_data.data() + data_offset, sizeof(uint32_t) + len);
                    total_width += len; // Width is actual string length, not including length prefix
                }
            }

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
            // TODO: Implement proper equal-width for numeric types with range calculation
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

        // Phase 1, Task 1.1.8 - Statistics storage
        //
        // For now, we store statistics in the in-memory cache only.
        // Future enhancement: persist to pg_statistic catalog table
        //
        // Steps for full implementation:
        // 1. Serialize MCVs to TOAST if large (> inline threshold)
        // 2. Serialize histogram to TOAST if large
        // 3. Create StatisticsRecord with basic stats + TOAST refs
        // 4. Write to pg_statistic catalog page
        // 5. Update cache

        std::lock_guard<std::mutex> lock(cache_mutex_);

        // Generate cache key
        uint64_t cache_key = getCacheKey(stats.table_id, stats.column_id);

        // Store in cache
        column_stats_cache_[cache_key] = stats;

        DEBUG_LOG_DB("Stored column statistics in cache for table=" +
                     std::to_string(cache_key >> 32) + " column=" +
                     std::to_string(cache_key & 0xFFFFFFFF));

        // TODO: Persist to pg_statistic catalog
        // For now, statistics are volatile (lost on database restart)
        // This is acceptable for Alpha release

        return Status::OK;
    }

    auto StatisticsManager::loadColumnStatistics(const ID &table_id, const ID &column_id,
                                                  ColumnStatistics &stats,
                                                  ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Loading column statistics from catalog");

        // Phase 1, Task 1.1.8 - Statistics loading
        //
        // For now, we load from in-memory cache only.
        // Future enhancement: load from pg_statistic catalog table
        //
        // Steps for full implementation:
        // 1. Read StatisticsRecord from pg_statistic catalog
        // 2. Load MCVs from TOAST if needed
        // 3. Load histogram from TOAST if needed
        // 4. Deserialize into ColumnStatistics struct

        // Note: The getColumnStatistics method already checks cache first,
        // so this is only called on cache miss. Since we don't have
        // persistent storage yet, we return NOT_FOUND.

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Statistics not found in cache (persistence not yet implemented)");
        return Status::NOT_FOUND;
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

            // Skip columns before target
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

                core::DataType col_type = static_cast<core::DataType>(columns[i].data_type);
                if (col_type == core::DataType::INT32)
                {
                    data_offset += sizeof(int32_t);
                }
                else if (col_type == core::DataType::INT64)
                {
                    data_offset += sizeof(int64_t);
                }
                else if (col_type == core::DataType::FLOAT64)
                {
                    data_offset += sizeof(double);
                }
                else if (col_type == core::DataType::VARCHAR)
                {
                    if (data_offset + sizeof(uint32_t) > tuple_data.size())
                        break;
                    uint32_t len;
                    std::memcpy(&len, tuple_data.data() + data_offset, sizeof(uint32_t));
                    data_offset += sizeof(uint32_t) + len;
                }
            }

            // Extract target column value
            core::DataType target_type = static_cast<core::DataType>(target_column_info.data_type);
            std::vector<uint8_t> value;

            if (target_type == core::DataType::INT32 && data_offset + sizeof(int32_t) <= tuple_data.size())
            {
                value.resize(sizeof(int32_t));
                std::memcpy(value.data(), tuple_data.data() + data_offset, sizeof(int32_t));
            }
            else if (target_type == core::DataType::INT64 && data_offset + sizeof(int64_t) <= tuple_data.size())
            {
                value.resize(sizeof(int64_t));
                std::memcpy(value.data(), tuple_data.data() + data_offset, sizeof(int64_t));
            }
            else if (target_type == core::DataType::FLOAT64 && data_offset + sizeof(double) <= tuple_data.size())
            {
                value.resize(sizeof(double));
                std::memcpy(value.data(), tuple_data.data() + data_offset, sizeof(double));
            }
            else if (target_type == core::DataType::VARCHAR && data_offset + sizeof(uint32_t) <= tuple_data.size())
            {
                uint32_t len;
                std::memcpy(&len, tuple_data.data() + data_offset, sizeof(uint32_t));
                if (data_offset + sizeof(uint32_t) + len <= tuple_data.size())
                {
                    value.resize(sizeof(uint32_t) + len);
                    std::memcpy(value.data(), tuple_data.data() + data_offset, sizeof(uint32_t) + len);
                }
            }

            column_values.push_back(std::move(value));
        }

        return column_values;
    }

} // namespace scratchbird::optimizer
