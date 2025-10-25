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
#include <unordered_set>

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

        // TODO: Phase 1, Task 1.1.1 - Get table info from catalog
        // TODO: Phase 1, Task 1.1.2 - Determine sample size
        // TODO: Phase 1, Task 1.1.3 - Sample table using Vitter's Algorithm S
        // TODO: Phase 1, Task 1.1.4 - For each column, compute statistics
        // TODO: Phase 1, Task 1.1.5 - Store statistics in catalog
        // TODO: Phase 1, Task 1.1.6 - Update statistics cache

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "analyzeTable not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto StatisticsManager::analyzeColumn(const ID &table_id, const ID &column_id,
                                           float sample_rate, ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Analyzing column");

        // TODO: Phase 1, Task 1.1 - Implement column-level analysis
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "analyzeColumn not yet implemented");
        return Status::NOT_IMPLEMENTED;
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
        uint64_t cache_key = 0; // TODO: Compute proper cache key from table_id
        std::memcpy(&cache_key, table_id.bytes.data(), sizeof(uint64_t));

        auto it = table_stats_cache_.find(cache_key);
        if (it != table_stats_cache_.end())
        {
            stats = it->second;
            return Status::OK;
        }

        // TODO: Load from catalog
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "getTableStatistics not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto StatisticsManager::dropStatistics(const ID &table_id, ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Dropping statistics for table");

        // TODO: Phase 1, Task 1.1 - Remove statistics records from pg_statistic
        // TODO: Invalidate cache for this table

        invalidateCache(table_id);

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "dropStatistics not yet implemented");
        return Status::NOT_IMPLEMENTED;
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
        std::random_device rd;
        std::mt19937 gen(rd());
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

    auto StatisticsManager::generateHistogram(const std::vector<uint8_t> &values,
                                               uint32_t bucket_count,
                                               HistogramType histogram_type,
                                               std::vector<HistogramBucket> &buckets,
                                               ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Generating histogram");

        // TODO: Phase 1, Task 1.1.5 - Implement histogram generation
        //
        // Equal-Height Histogram:
        // 1. Sort values
        // 2. Divide into bucket_count buckets with ~equal number of values
        // 3. Store min/max of each bucket
        //
        // Equal-Width Histogram:
        // 1. Find min and max values
        // 2. Divide range into bucket_count equal intervals
        // 3. Count values in each bucket

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "generateHistogram not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto StatisticsManager::identifyMCVs(const std::vector<uint8_t> &values,
                                          uint32_t max_mcv_count,
                                          std::vector<MCVEntry> &mcv_list,
                                          ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Identifying Most Common Values");

        // TODO: Phase 1, Task 1.1.6 - Implement MCV identification
        //
        // Algorithm:
        // 1. Build frequency map (value -> count)
        // 2. Sort by frequency (descending)
        // 3. Take top max_mcv_count entries
        // 4. Compute frequency as fraction of total values
        // 5. Store in mcv_list

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "identifyMCVs not yet implemented");
        return Status::NOT_IMPLEMENTED;
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

        // TODO: Phase 1, Task 1.1.8 - Implement statistics storage
        //
        // Steps:
        // 1. Serialize MCVs to TOAST if large
        // 2. Serialize histogram to TOAST if large
        // 3. Create StatisticsRecord with basic stats + TOAST refs
        // 4. Write to pg_statistic catalog page
        // 5. Update cache

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "storeColumnStatistics not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto StatisticsManager::loadColumnStatistics(const ID &table_id, const ID &column_id,
                                                  ColumnStatistics &stats,
                                                  ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Loading column statistics from catalog");

        // TODO: Phase 1, Task 1.4 - Implement statistics loading
        //
        // Steps:
        // 1. Read StatisticsRecord from pg_statistic catalog
        // 2. Load MCVs from TOAST if needed
        // 3. Load histogram from TOAST if needed
        // 4. Deserialize into ColumnStatistics struct

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "loadColumnStatistics not yet implemented");
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

} // namespace scratchbird::optimizer
