// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <random>
#include <scratchbird/engine/parallel_aggregation_sorting.h>
#include <sstream>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace scratchbird::engine
{

    //==============================================================================
    // Value Implementation
    //==============================================================================

    bool Value::operator<(const Value& other) const
    {
        if (type != other.type) {
            return type < other.type;
        }

        switch (type) {
        case DataType::INTEGER:
            return int_val < other.int_val;
        case DataType::BIGINT:
        case DataType::TIMESTAMP:
            return bigint_val < other.bigint_val;
        case DataType::DOUBLE:
            return double_val < other.double_val;
        case DataType::VARCHAR:
            return string_val < other.string_val;
        default:
            return false;
        }
    }

    bool Value::operator==(const Value& other) const
    {
        if (type != other.type) {
            return false;
        }

        switch (type) {
        case DataType::INTEGER:
            return int_val == other.int_val;
        case DataType::BIGINT:
        case DataType::TIMESTAMP:
            return bigint_val == other.bigint_val;
        case DataType::DOUBLE:
            return std::abs(double_val - other.double_val) < 1e-10;
        case DataType::VARCHAR:
            return string_val == other.string_val;
        default:
            return false;
        }
    }

    Value Value::operator+(const Value& other) const
    {
        assert(type == other.type);

        switch (type) {
        case DataType::INTEGER:
            return Value(int_val + other.int_val);
        case DataType::BIGINT:
        case DataType::TIMESTAMP:
            return Value(bigint_val + other.bigint_val);
        case DataType::DOUBLE:
            return Value(double_val + other.double_val);
        default:
            return *this; // Not additive for string types
        }
    }

    Value& Value::operator+=(const Value& other)
    {
        assert(type == other.type);

        switch (type) {
        case DataType::INTEGER:
            int_val += other.int_val;
            break;
        case DataType::BIGINT:
        case DataType::TIMESTAMP:
            bigint_val += other.bigint_val;
            break;
        case DataType::DOUBLE:
            double_val += other.double_val;
            break;
        default:
            break; // Not additive for string types
        }

        return *this;
    }

    //==============================================================================
    // PartialAggregate Implementation
    //==============================================================================

    PartialAggregate::PartialAggregate(std::size_t key_count, std::size_t agg_count)
        : group_keys(key_count), aggregate_values(agg_count), counts(agg_count, 0)
    {
    }

    void PartialAggregate::combine(const PartialAggregate& other)
    {
        assert(group_keys == other.group_keys);
        assert(aggregate_values.size() == other.aggregate_values.size());
        assert(counts.size() == other.counts.size());

        for (std::size_t i = 0; i < aggregate_values.size(); ++i) {
            aggregate_values[i] += other.aggregate_values[i];
            counts[i] += other.counts[i];
        }
    }

    Row PartialAggregate::finalize(const std::vector<AggregateColumn>& agg_columns) const
    {
        Row result;
        result.columns.reserve(group_keys.size() + agg_columns.size());

        // Add group key columns
        for (const auto& key : group_keys) {
            result.columns.push_back(key);
        }

        // Add finalized aggregate columns
        for (std::size_t i = 0; i < agg_columns.size(); ++i) {
            const auto& agg_col = agg_columns[i];

            switch (agg_col.function) {
            case AggregateFunction::COUNT:
                result.columns.emplace_back(static_cast<std::int64_t>(counts[i]));
                break;

            case AggregateFunction::SUM:
            case AggregateFunction::MIN:
            case AggregateFunction::MAX:
                result.columns.push_back(aggregate_values[i]);
                break;

            case AggregateFunction::AVG:
                if (counts[i] > 0) {
                    if (aggregate_values[i].type == DataType::DOUBLE) {
                        result.columns.emplace_back(aggregate_values[i].double_val / counts[i]);
                    } else if (aggregate_values[i].type == DataType::BIGINT) {
                        result.columns.emplace_back(
                            static_cast<double>(aggregate_values[i].bigint_val) / counts[i]);
                    } else {
                        result.columns.emplace_back(
                            static_cast<double>(aggregate_values[i].int_val) / counts[i]);
                    }
                } else {
                    result.columns.emplace_back(0.0);
                }
                break;

            default:
                result.columns.push_back(aggregate_values[i]);
                break;
            }
        }

        return result;
    }

    //==============================================================================
    // Hash and Equality Functions
    //==============================================================================

    std::size_t GroupKeyHash::operator()(const std::vector<Value>& keys) const
    {
        std::size_t hash = 0;
        for (const auto& key : keys) {
            std::size_t key_hash = 0;
            switch (key.type) {
            case DataType::INTEGER:
                key_hash = std::hash<std::int32_t>{}(key.int_val);
                break;
            case DataType::BIGINT:
            case DataType::TIMESTAMP:
                key_hash = std::hash<std::int64_t>{}(key.bigint_val);
                break;
            case DataType::DOUBLE:
                key_hash = std::hash<double>{}(key.double_val);
                break;
            case DataType::VARCHAR:
                key_hash = std::hash<std::string>{}(key.string_val);
                break;
            default:
                key_hash = 0;
            }
            hash ^= key_hash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

    bool GroupKeyEqual::operator()(const std::vector<Value>& lhs,
                                   const std::vector<Value>& rhs) const
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (!(lhs[i] == rhs[i])) {
                return false;
            }
        }

        return true;
    }

    //==============================================================================
    // ParallelAggregator Implementation
    //==============================================================================

    ParallelAggregator::ParallelAggregator() : config_({})
    {
        initialize_workers();
    }

    ParallelAggregator::ParallelAggregator(const Config& config) : config_(config)
    {
        initialize_workers();
    }

    ParallelAggregator::~ParallelAggregator()
    {
        shutdown_workers();
    }

    std::vector<Row> ParallelAggregator::aggregate(const std::vector<Row>& input_rows,
                                                   const std::vector<std::size_t>& group_columns,
                                                   const std::vector<AggregateColumn>& agg_columns)
    {

        auto start_time = std::chrono::high_resolution_clock::now();

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_input_rows = input_rows.size();
        }

        if (input_rows.empty()) {
            return {};
        }

        // Distribute work among workers
        const std::size_t rows_per_worker =
            std::max(static_cast<std::size_t>(1), input_rows.size() / config_.max_workers);

        std::vector<std::future<void>> futures;
        std::size_t start_idx = 0;

        for (std::size_t i = 0; i < workers_.size() && start_idx < input_rows.size(); ++i) {
            std::size_t end_idx = std::min(start_idx + rows_per_worker, input_rows.size());

            auto& worker = *workers_[i];

            // Assign work to worker
            {
                std::lock_guard<std::mutex> lock(worker.work_mutex);
                worker.input_batch.clear();
                worker.input_batch.insert(worker.input_batch.end(), input_rows.begin() + start_idx,
                                          input_rows.begin() + end_idx);
                worker.partial_results.clear();
                worker.active = true;
            }

            // Launch worker task
            futures.push_back(
                std::async(std::launch::async, [&worker, &group_columns, &agg_columns, this]() {
                    process_batch(worker, group_columns, agg_columns);
                }));

            start_idx = end_idx;
        }

        // Wait for all workers to complete
        for (auto& future : futures) {
            future.wait();
        }

        auto worker_end_time = std::chrono::high_resolution_clock::now();

        // Merge partial results from all workers
        auto merge_start_time = std::chrono::high_resolution_clock::now();
        auto result = merge_partial_results(agg_columns);
        auto end_time = std::chrono::high_resolution_clock::now();

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_output_rows = result.size();
            stats_.total_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
            stats_.worker_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(worker_end_time - start_time);
            stats_.merge_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - merge_start_time);
            stats_.efficiency_ratio =
                stats_.total_input_rows > 0
                    ? static_cast<double>(stats_.total_output_rows) / stats_.total_input_rows
                    : 0.0;
        }

        return result;
    }

    ParallelAggregator::Statistics ParallelAggregator::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    void ParallelAggregator::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = Statistics{};
    }

    void ParallelAggregator::initialize_workers()
    {
        workers_.reserve(config_.max_workers);

        for (std::size_t i = 0; i < config_.max_workers; ++i) {
            workers_.push_back(std::make_unique<WorkerData>());
        }
    }

    void ParallelAggregator::shutdown_workers()
    {
        shutdown_ = true;

        for (auto& worker : workers_) {
            if (worker->thread.joinable()) {
                worker->thread.join();
            }
        }
    }

    void ParallelAggregator::process_batch(WorkerData& worker,
                                           const std::vector<std::size_t>& group_columns,
                                           const std::vector<AggregateColumn>& agg_columns)
    {

        for (const auto& row : worker.input_batch) {
            // Extract group key
            std::vector<Value> group_key;
            group_key.reserve(group_columns.size());
            for (std::size_t col_idx : group_columns) {
                if (col_idx < row.size()) {
                    group_key.push_back(row[col_idx]);
                }
            }

            // Find or create partial aggregate for this group
            auto& partial = worker.partial_results[group_key];
            if (partial.group_keys.empty()) {
                partial = PartialAggregate(group_key.size(), agg_columns.size());
                partial.group_keys = group_key;
            }

            // Process each aggregate column
            for (std::size_t i = 0; i < agg_columns.size(); ++i) {
                const auto& agg_col = agg_columns[i];

                if (agg_col.source_column >= row.size()) {
                    continue;
                }

                const auto& source_value = row[agg_col.source_column];

                switch (agg_col.function) {
                case AggregateFunction::COUNT:
                    partial.counts[i]++;
                    break;

                case AggregateFunction::SUM:
                case AggregateFunction::AVG:
                    if (partial.counts[i] == 0) {
                        partial.aggregate_values[i] = source_value;
                    } else {
                        partial.aggregate_values[i] += source_value;
                    }
                    partial.counts[i]++;
                    break;

                case AggregateFunction::MIN:
                    if (partial.counts[i] == 0 || source_value < partial.aggregate_values[i]) {
                        partial.aggregate_values[i] = source_value;
                    }
                    partial.counts[i]++;
                    break;

                case AggregateFunction::MAX:
                    if (partial.counts[i] == 0 || partial.aggregate_values[i] < source_value) {
                        partial.aggregate_values[i] = source_value;
                    }
                    partial.counts[i]++;
                    break;

                default:
                    // For unsupported functions, just count
                    partial.counts[i]++;
                    break;
                }
            }
        }

        worker.active = false;
    }

    std::vector<Row>
    ParallelAggregator::merge_partial_results(const std::vector<AggregateColumn>& agg_columns)
    {

        std::unordered_map<std::vector<Value>, PartialAggregate, GroupKeyHash, GroupKeyEqual>
            merged_results;

        // Merge results from all workers
        for (const auto& worker : workers_) {
            for (const auto& [group_key, partial] : worker->partial_results) {
                auto& merged = merged_results[group_key];
                if (merged.group_keys.empty()) {
                    merged = partial;
                } else {
                    merged.combine(partial);
                }
            }
        }

        // Finalize and convert to rows
        std::vector<Row> result;
        result.reserve(merged_results.size());

        for (const auto& [group_key, partial] : merged_results) {
            result.push_back(partial.finalize(agg_columns));
        }

        return result;
    }

    //==============================================================================
    // ParallelSorter Implementation
    //==============================================================================

    ParallelSorter::SortedChunk::SortedChunk(std::vector<Row>&& chunk_rows)
        : rows(std::move(chunk_rows)), is_external(false), row_count(rows.size())
    {
    }

    ParallelSorter::SortedChunk::SortedChunk(const std::string& file_path, std::size_t rows)
        : temp_file_path(file_path), is_external(true), row_count(rows)
    {
    }

    ParallelSorter::ParallelSorter() : config_({})
    {
        initialize_workers();
    }

    ParallelSorter::ParallelSorter(const Config& config) : config_(config)
    {
        initialize_workers();
    }

    ParallelSorter::~ParallelSorter()
    {
        shutdown_workers();
    }

    std::vector<Row> ParallelSorter::sort(const std::vector<Row>& input_rows,
                                          const std::vector<SortColumn>& sort_columns)
    {

        auto start_time = std::chrono::high_resolution_clock::now();

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_rows = input_rows.size();
        }

        if (input_rows.empty()) {
            return {};
        }

        // Single-threaded fallback for small datasets
        if (input_rows.size() < config_.rows_per_chunk) {
            std::vector<Row> result = input_rows;
            std::sort(result.begin(), result.end(), RowComparator(sort_columns));

            auto end_time = std::chrono::high_resolution_clock::now();
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_time =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
                stats_.sort_time = stats_.total_time;
                stats_.parallelization_efficiency = 1.0;
            }

            return result;
        }

        // Create chunks for parallel sorting
        std::vector<std::unique_ptr<SortedChunk>> chunks;
        std::vector<std::future<std::unique_ptr<SortedChunk>>> futures;

        std::size_t start_idx = 0;
        std::size_t worker_idx = 0;

        while (start_idx < input_rows.size()) {
            std::size_t end_idx = std::min(start_idx + config_.rows_per_chunk, input_rows.size());

            auto& worker = *workers_[worker_idx % workers_.size()];

            // Assign chunk to worker
            {
                std::lock_guard<std::mutex> lock(worker.work_mutex);
                worker.input_chunk.clear();
                worker.input_chunk.insert(worker.input_chunk.end(), input_rows.begin() + start_idx,
                                          input_rows.begin() + end_idx);
                worker.active = true;
            }

            // Launch sorting task
            futures.push_back(std::async(std::launch::async, [&worker, &sort_columns, this]() {
                sort_chunk(worker, sort_columns);
                return std::move(worker.result_chunk);
            }));

            start_idx = end_idx;
            worker_idx++;
        }

        // Collect sorted chunks
        for (auto& future : futures) {
            chunks.push_back(future.get());
        }

        auto sort_end_time = std::chrono::high_resolution_clock::now();

        // Merge sorted chunks
        auto merge_start_time = std::chrono::high_resolution_clock::now();
        auto result = merge_sorted_chunks(chunks, sort_columns);
        auto end_time = std::chrono::high_resolution_clock::now();

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.chunks_created = chunks.size();
            stats_.total_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
            stats_.sort_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(sort_end_time - start_time);
            stats_.merge_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - merge_start_time);
            stats_.parallelization_efficiency =
                stats_.sort_time.count() > 0
                    ? static_cast<double>(stats_.total_time.count()) / stats_.sort_time.count()
                    : 1.0;
        }

        return result;
    }

    ParallelSorter::Statistics ParallelSorter::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    void ParallelSorter::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = Statistics{};
    }

    void ParallelSorter::initialize_workers()
    {
        workers_.reserve(config_.max_workers);

        for (std::size_t i = 0; i < config_.max_workers; ++i) {
            workers_.push_back(std::make_unique<WorkerData>());
        }
    }

    void ParallelSorter::shutdown_workers()
    {
        shutdown_ = true;

        for (auto& worker : workers_) {
            if (worker->thread.joinable()) {
                worker->thread.join();
            }
        }
    }

    void ParallelSorter::sort_chunk(WorkerData& worker, const std::vector<SortColumn>& sort_columns)
    {

        // Sort the chunk
        std::sort(worker.input_chunk.begin(), worker.input_chunk.end(),
                  RowComparator(sort_columns));

        // Decide whether to keep in memory or write to disk
        const std::size_t estimated_size = worker.input_chunk.size() * 100; // Rough estimate

        if (config_.enable_external_sort && estimated_size > config_.max_memory_per_worker) {
            // Write to temporary file
            std::string temp_path = parallel_agg_sort_utils::generate_temp_file_path(
                config_.temp_directory, "sort_chunk");
            write_chunk_to_file(worker.input_chunk, temp_path);

            worker.result_chunk =
                std::make_unique<SortedChunk>(temp_path, worker.input_chunk.size());
        } else {
            // Keep in memory
            worker.result_chunk = std::make_unique<SortedChunk>(std::move(worker.input_chunk));
        }

        worker.active = false;
    }

    std::vector<Row>
    ParallelSorter::merge_sorted_chunks(std::vector<std::unique_ptr<SortedChunk>>& chunks,
                                        const std::vector<SortColumn>& sort_columns)
    {

        if (chunks.empty()) {
            return {};
        }

        if (chunks.size() == 1) {
            if (chunks[0]->is_external) {
                return read_chunk_from_file(chunks[0]->temp_file_path);
            } else {
                return std::move(chunks[0]->rows);
            }
        }

        // Use k-way merge for multiple chunks
        std::vector<Row> result;
        result.reserve(stats_.total_rows);

        MergeIterator merge_iter(chunks, sort_columns);
        while (merge_iter.has_next()) {
            result.push_back(merge_iter.next());
        }

        return result;
    }

    void ParallelSorter::write_chunk_to_file(const std::vector<Row>& rows,
                                             const std::string& file_path)
    {
        std::ofstream file(file_path, std::ios::binary);
        if (!file) {
            return; // Handle error appropriately in production
        }

        // Write number of rows
        std::uint64_t row_count = rows.size();
        file.write(reinterpret_cast<const char*>(&row_count), sizeof(row_count));

        // Write rows (simplified serialization)
        for (const auto& row : rows) {
            std::uint32_t col_count = static_cast<std::uint32_t>(row.size());
            file.write(reinterpret_cast<const char*>(&col_count), sizeof(col_count));

            for (const auto& value : row.columns) {
                std::uint8_t type = static_cast<std::uint8_t>(value.type);
                file.write(reinterpret_cast<const char*>(&type), sizeof(type));

                switch (value.type) {
                case DataType::INTEGER:
                    file.write(reinterpret_cast<const char*>(&value.int_val),
                               sizeof(value.int_val));
                    break;
                case DataType::BIGINT:
                case DataType::TIMESTAMP:
                    file.write(reinterpret_cast<const char*>(&value.bigint_val),
                               sizeof(value.bigint_val));
                    break;
                case DataType::DOUBLE:
                    file.write(reinterpret_cast<const char*>(&value.double_val),
                               sizeof(value.double_val));
                    break;
                case DataType::VARCHAR: {
                    std::uint32_t str_len = static_cast<std::uint32_t>(value.string_val.length());
                    file.write(reinterpret_cast<const char*>(&str_len), sizeof(str_len));
                    file.write(value.string_val.c_str(), str_len);
                } break;
                case DataType::DECIMAL:
                    // For now, treat DECIMAL as DOUBLE
                    file.write(reinterpret_cast<const char*>(&value.double_val),
                               sizeof(value.double_val));
                    break;
                }
            }
        }
    }

    std::vector<Row> ParallelSorter::read_chunk_from_file(const std::string& file_path)
    {
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return {}; // Handle error appropriately in production
        }

        // Read number of rows
        std::uint64_t row_count;
        file.read(reinterpret_cast<char*>(&row_count), sizeof(row_count));

        std::vector<Row> rows;
        rows.reserve(row_count);

        // Read rows
        for (std::uint64_t i = 0; i < row_count; ++i) {
            std::uint32_t col_count;
            file.read(reinterpret_cast<char*>(&col_count), sizeof(col_count));

            Row row(col_count);
            for (std::uint32_t j = 0; j < col_count; ++j) {
                std::uint8_t type;
                file.read(reinterpret_cast<char*>(&type), sizeof(type));

                row[j].type = static_cast<DataType>(type);

                switch (row[j].type) {
                case DataType::INTEGER:
                    file.read(reinterpret_cast<char*>(&row[j].int_val), sizeof(row[j].int_val));
                    break;
                case DataType::BIGINT:
                case DataType::TIMESTAMP:
                    file.read(reinterpret_cast<char*>(&row[j].bigint_val),
                              sizeof(row[j].bigint_val));
                    break;
                case DataType::DOUBLE:
                    file.read(reinterpret_cast<char*>(&row[j].double_val),
                              sizeof(row[j].double_val));
                    break;
                case DataType::VARCHAR: {
                    std::uint32_t str_len;
                    file.read(reinterpret_cast<char*>(&str_len), sizeof(str_len));
                    row[j].string_val.resize(str_len);
                    file.read(&row[j].string_val[0], str_len);
                } break;
                case DataType::DECIMAL:
                    // For now, treat DECIMAL as DOUBLE
                    file.read(reinterpret_cast<char*>(&row[j].double_val),
                              sizeof(row[j].double_val));
                    break;
                }
            }
            rows.push_back(std::move(row));
        }

        return rows;
    }

    bool ParallelSorter::RowComparator::operator()(const Row& lhs, const Row& rhs) const
    {
        for (const auto& sort_col : sort_columns) {
            if (sort_col.column_index >= lhs.size() || sort_col.column_index >= rhs.size()) {
                continue;
            }

            const auto& left_val = lhs[sort_col.column_index];
            const auto& right_val = rhs[sort_col.column_index];

            if (left_val == right_val) {
                continue; // Equal, check next column
            }

            bool less_than = left_val < right_val;
            return sort_col.direction == SortDirection::ASCENDING ? less_than : !less_than;
        }

        return false; // All columns are equal
    }

    //==============================================================================
    // ParallelSorter::MergeIterator Implementation
    //==============================================================================

    ParallelSorter::MergeIterator::MergeIterator(std::vector<std::unique_ptr<SortedChunk>>& chunks,
                                                 const std::vector<SortColumn>& sort_columns)
        : sort_columns_(sort_columns), heap_(IteratorComparator(sort_columns))
    {

        // Initialize iterators for each chunk
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            if (chunks[i]->row_count > 0) {
                ChunkIterator iter(i, chunks[i]->is_external, chunks[i]->temp_file_path);
                if (iter.load_next_buffer()) {
                    heap_.push(std::move(iter));
                }
            }
        }
    }

    bool ParallelSorter::MergeIterator::has_next() const
    {
        return !heap_.empty();
    }

    Row ParallelSorter::MergeIterator::next()
    {
        if (heap_.empty()) {
            throw std::runtime_error("No more rows available");
        }

        ChunkIterator iter = heap_.top();
        heap_.pop();

        Row result = iter.current_buffer[iter.row_index];

        // Advance the iterator
        ++iter.row_index;
        if (iter.row_index >= iter.current_buffer.size()) {
            if (iter.load_next_buffer()) {
                heap_.push(std::move(iter));
            }
        } else {
            heap_.push(std::move(iter));
        }

        return result;
    }

    ParallelSorter::MergeIterator::ChunkIterator::ChunkIterator(std::size_t chunk_idx,
                                                                bool external,
                                                                const std::string& path)
        : chunk_index(chunk_idx), row_index(0), is_external(external), file_path(path)
    {
    }

    bool ParallelSorter::MergeIterator::ChunkIterator::load_next_buffer()
    {
        current_buffer.clear();
        row_index = 0;

        if (is_external) {
            // For external chunks, we would read from file in chunks
            // For now, just return false to indicate no more data
            return false;
        } else {
            // For in-memory chunks, we've already loaded all data
            return false;
        }
    }

    bool
    ParallelSorter::MergeIterator::IteratorComparator::operator()(const ChunkIterator& lhs,
                                                                  const ChunkIterator& rhs) const
    {

        if (lhs.row_index >= lhs.current_buffer.size() ||
            rhs.row_index >= rhs.current_buffer.size()) {
            return lhs.row_index >= lhs.current_buffer.size();
        }

        const Row& lhs_row = lhs.current_buffer[lhs.row_index];
        const Row& rhs_row = rhs.current_buffer[rhs.row_index];

        // Compare rows using sort columns (reverse for min-heap behavior)
        RowComparator comparator(sort_columns);
        return comparator(rhs_row, lhs_row);
    }

    //==============================================================================
    // ParallelAggregationSortingPipeline Implementation
    //==============================================================================

    ParallelAggregationSortingPipeline::ParallelAggregationSortingPipeline()
        : config_({}), aggregator_(), sorter_()
    {
    }

    ParallelAggregationSortingPipeline::ParallelAggregationSortingPipeline(const Config& config)
        : config_(config), aggregator_(config.aggregation_config), sorter_(config.sorting_config)
    {
    }

    ParallelAggregationSortingPipeline::~ParallelAggregationSortingPipeline() = default;

    std::vector<Row> ParallelAggregationSortingPipeline::aggregate_and_sort(
        const std::vector<Row>& input_rows, const std::vector<std::size_t>& group_columns,
        const std::vector<AggregateColumn>& agg_columns,
        const std::vector<SortColumn>& sort_columns)
    {

        auto start_time = std::chrono::high_resolution_clock::now();

        // Step 1: Parallel aggregation
        auto aggregated_rows = aggregator_.aggregate(input_rows, group_columns, agg_columns);

        stats_.intermediate_rows = aggregated_rows.size();

        // Step 2: Parallel sorting of aggregated results
        auto result = sorter_.sort(aggregated_rows, sort_columns);

        auto end_time = std::chrono::high_resolution_clock::now();

        // Update pipeline statistics
        stats_.aggregation_stats = aggregator_.get_statistics();
        stats_.sorting_stats = sorter_.get_statistics();
        stats_.total_pipeline_time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

        if (stats_.aggregation_stats.total_time.count() > 0 &&
            stats_.sorting_stats.total_time.count() > 0) {
            auto sequential_time =
                stats_.aggregation_stats.total_time + stats_.sorting_stats.total_time;
            stats_.pipeline_efficiency =
                static_cast<double>(sequential_time.count()) / stats_.total_pipeline_time.count();
        }

        return result;
    }

    ParallelAggregationSortingPipeline::Statistics
    ParallelAggregationSortingPipeline::get_statistics() const
    {
        return stats_;
    }

    void ParallelAggregationSortingPipeline::reset_statistics()
    {
        stats_ = Statistics{};
        aggregator_.reset_statistics();
        sorter_.reset_statistics();
    }

    //==============================================================================
    // Utility Functions
    //==============================================================================

    namespace parallel_agg_sort_utils
    {

        std::size_t calculate_optimal_workers(std::size_t data_size, std::size_t available_memory)
        {
            // Simple heuristic: one worker per 100MB of data, bounded by CPU cores
            const std::size_t data_per_worker = 100 * 1024 * 1024; // 100MB
            std::size_t workers_by_data =
                std::max(static_cast<std::size_t>(1), data_size / data_per_worker);

            auto system_info = get_system_memory_info();
            std::size_t max_workers = std::min(
                system_info.cpu_count, available_memory / (64 * 1024 * 1024)); // 64MB per worker

            return std::min(workers_by_data, max_workers);
        }

        std::size_t estimate_aggregation_memory(std::size_t /* input_rows */,
                                                std::size_t estimated_groups,
                                                const std::vector<AggregateColumn>& agg_columns)
        {

            // Rough estimate: 64 bytes per group + 32 bytes per aggregate column per group
            const std::size_t bytes_per_group = 64 + (agg_columns.size() * 32);
            return estimated_groups * bytes_per_group;
        }

        std::size_t estimate_sorting_memory(std::size_t input_rows, std::size_t row_size)
        {
            // Sorting requires at least the input size plus some overhead for temporary storage
            return input_rows * row_size * 2; // 2x for temporary storage during merge
        }

        std::string generate_temp_file_path(const std::string& base_dir, const std::string& prefix)
        {
            static std::atomic<std::uint64_t> counter{0};
            std::stringstream ss;
            ss << base_dir << "/" << prefix << "_" << std::this_thread::get_id() << "_"
               << counter.fetch_add(1) << ".tmp";
            return ss.str();
        }

        void cleanup_temp_files(const std::vector<std::string>& file_paths)
        {
            for (const auto& path : file_paths) {
                std::remove(path.c_str());
            }
        }

        SystemMemoryInfo get_system_memory_info()
        {
            SystemMemoryInfo info{};

#ifdef __linux__
            struct sysinfo si;
            if (sysinfo(&si) == 0) {
                info.total_memory = si.totalram * si.mem_unit;
                info.available_memory = si.freeram * si.mem_unit;
            }

            info.cpu_count = static_cast<std::size_t>(sysconf(_SC_NPROCESSORS_ONLN));
            info.cache_line_size = static_cast<std::size_t>(sysconf(_SC_LEVEL1_DCACHE_LINESIZE));
#else
            // Default values for non-Linux systems
            info.total_memory = 8ULL * 1024 * 1024 * 1024;     // 8GB
            info.available_memory = 4ULL * 1024 * 1024 * 1024; // 4GB
            info.cpu_count = 4;
            info.cache_line_size = 64;
#endif

            return info;
        }

    } // namespace parallel_agg_sort_utils

} // namespace scratchbird::engine
