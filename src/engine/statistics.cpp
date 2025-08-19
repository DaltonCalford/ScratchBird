#include "scratchbird/engine/statistics.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/heap_rel.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_map>

namespace scratchbird::engine
{

    // ========== ColumnStatistics Implementation ==========

    std::string ColumnStatistics::to_json() const
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "{";
        oss << "\"n_distinct\":" << n_distinct << ",";
        oss << "\"n_null\":" << n_null << ",";
        oss << "\"n_total\":" << n_total << ",";
        oss << "\"min_value\":\"" << min_value << "\",";
        oss << "\"max_value\":\"" << max_value << "\",";
        oss << "\"avg_width\":" << avg_width << ",";
        oss << "\"last_analyzed\":" << last_analyzed << ",";
        oss << "\"sample_size\":" << sample_size << ",";

        // Most Common Values
        oss << "\"most_common_values\":[";
        for (size_t i = 0; i < most_common_values.size(); ++i) {
            if (i > 0)
                oss << ",";
            const auto& mcv = most_common_values[i];
            oss << "{\"value\":\"" << mcv.value << "\",\"frequency\":" << mcv.frequency
                << ",\"fraction\":" << mcv.fraction << "}";
        }
        oss << "],";

        // Histogram
        oss << "\"histogram\":[";
        for (size_t i = 0; i < histogram.size(); ++i) {
            if (i > 0)
                oss << ",";
            const auto& bucket = histogram[i];
            oss << "{\"lower_bound\":\"" << bucket.lower_bound << "\",\"upper_bound\":\""
                << bucket.upper_bound << "\",\"frequency\":" << bucket.frequency
                << ",\"fraction\":" << bucket.fraction << "}";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }

    ColumnStatistics ColumnStatistics::from_json(const std::string& json)
    {
        // Simple JSON parsing for basic fields
        // In a production system, use a proper JSON library
        ColumnStatistics stats;

        auto find_field = [&json](const std::string& field) -> std::string {
            std::string pattern = "\"" + field + "\":";
            auto pos = json.find(pattern);
            if (pos == std::string::npos)
                return "";

            pos += pattern.length();
            if (json[pos] == '"') {
                // String value
                pos++;
                auto end = json.find('"', pos);
                return json.substr(pos, end - pos);
            } else {
                // Numeric value
                auto end = json.find_first_of(",}", pos);
                return json.substr(pos, end - pos);
            }
        };

        try {
            stats.n_distinct = std::stoull(find_field("n_distinct"));
            stats.n_null = std::stoull(find_field("n_null"));
            stats.n_total = std::stoull(find_field("n_total"));
            stats.min_value = find_field("min_value");
            stats.max_value = find_field("max_value");
            stats.avg_width = std::stod(find_field("avg_width"));
            stats.last_analyzed = std::stoull(find_field("last_analyzed"));
            stats.sample_size = std::stoull(find_field("sample_size"));
        } catch (...) {
            // Return default statistics on parse error
        }

        return stats;
    }

    double ColumnStatistics::estimate_selectivity(const std::string& op,
                                                  const std::string& value) const
    {
        if (n_total == 0)
            return 0.1; // Default estimate

        // Handle NULL comparisons
        if (value == "NULL" || value.empty()) {
            if (op == "IS NULL") {
                return static_cast<double>(n_null) / n_total;
            } else if (op == "IS NOT NULL") {
                return static_cast<double>(n_total - n_null) / n_total;
            }
        }

        // Check Most Common Values first
        for (const auto& mcv : most_common_values) {
            if (mcv.value == value) {
                if (op == "=" || op == "==") {
                    return mcv.fraction;
                } else if (op == "!=" || op == "<>") {
                    return 1.0 - mcv.fraction;
                }
            }
        }

        // Range estimates using histogram
        if (op == "<" || op == "<=" || op == ">" || op == ">=") {
            // Simple histogram-based estimation
            // This is a simplified version - production would be more sophisticated
            if (!histogram.empty()) {
                // Find bucket containing the value
                for (const auto& bucket : histogram) {
                    if (value >= bucket.lower_bound && value <= bucket.upper_bound) {
                        if (op == "<" || op == "<=") {
                            return bucket.fraction *
                                   0.5; // Assume uniform distribution within bucket
                        } else {
                            return 1.0 - (bucket.fraction * 0.5);
                        }
                    }
                }
            }

            // Default range selectivity estimates
            if (op == "<" || op == "<=")
                return 0.33;
            if (op == ">" || op == ">=")
                return 0.33;
        }

        // Default selectivity for equality on non-MCV values
        if (op == "=" || op == "==") {
            if (n_distinct > 0) {
                return 1.0 / n_distinct; // Uniform distribution assumption
            }
            return 0.01; // Very selective default
        }

        if (op == "!=" || op == "<>") {
            return 1.0 - estimate_selectivity("=", value);
        }

        // Default for unknown operators
        return 0.1;
    }

    double ColumnStatistics::estimate_distinct_rows() const
    {
        return static_cast<double>(n_distinct);
    }

    double ColumnStatistics::estimate_null_fraction() const
    {
        if (n_total == 0)
            return 0.0;
        return static_cast<double>(n_null) / n_total;
    }

    // ========== TableStatistics Implementation ==========

    std::string TableStatistics::to_json() const
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "{";
        oss << "\"n_rows\":" << n_rows << ",";
        oss << "\"n_pages\":" << n_pages << ",";
        oss << "\"avg_row_size\":" << avg_row_size << ",";
        oss << "\"last_analyzed\":" << last_analyzed << ",";
        oss << "\"column_stats\":{";

        bool first = true;
        for (const auto& kv : column_stats) {
            if (!first)
                oss << ",";
            first = false;
            oss << "\"" << kv.first << "\":" << kv.second.to_json();
        }
        oss << "}";
        oss << "}";
        return oss.str();
    }

    TableStatistics TableStatistics::from_json(const std::string& json)
    {
        // Simple JSON parsing - production would use proper JSON library
        TableStatistics stats;

        auto find_field = [&json](const std::string& field) -> std::string {
            std::string pattern = "\"" + field + "\":";
            auto pos = json.find(pattern);
            if (pos == std::string::npos)
                return "";

            pos += pattern.length();
            if (json[pos] == '"') {
                // String value
                pos++;
                auto end = json.find('"', pos);
                return json.substr(pos, end - pos);
            } else {
                // Numeric value
                auto end = json.find_first_of(",}", pos);
                return json.substr(pos, end - pos);
            }
        };

        try {
            stats.n_rows = std::stoull(find_field("n_rows"));
            stats.n_pages = std::stoull(find_field("n_pages"));
            stats.avg_row_size = std::stod(find_field("avg_row_size"));
            stats.last_analyzed = std::stoull(find_field("last_analyzed"));
        } catch (...) {
            // Return default statistics on parse error
        }

        return stats;
    }

    // ========== StatisticsCollector Implementation ==========

    StatisticsCollector::StatisticsCollector(const std::string& db_path) : db_path_(db_path) {}

    bool StatisticsCollector::analyze_table(const std::string& schema, const std::string& table)
    {
        try {
            std::fprintf(stderr, "[ANALYZE] Analyzing table %s.%s\n", schema.c_str(),
                         table.c_str());

            // Sample table data
            auto sample_data = sample_table_data(schema, table);
            if (sample_data.empty()) {
                std::fprintf(stderr, "[ANALYZE] No data found in table %s.%s\n", schema.c_str(),
                             table.c_str());
                return false;
            }

            // Get column names
            CatalogManager cm(db_path_);
            auto soid = cm.lookup_schema_oid_by_name(schema);
            if (!soid) {
                std::fprintf(stderr, "[ANALYZE] Schema not found: %s\n", schema.c_str());
                return false;
            }

            auto column_names = cm.list_column_names_by_name(*soid, table);
            if (column_names.empty()) {
                std::fprintf(stderr, "[ANALYZE] No columns found for table %s.%s\n", schema.c_str(),
                             table.c_str());
                return false;
            }

            // Build table statistics
            TableStatistics table_stats;
            table_stats.n_rows = sample_data.size();
            table_stats.last_analyzed = std::time(nullptr);

            std::uint64_t total_row_size = 0;
            for (const auto& row : sample_data) {
                for (const auto& val : row) {
                    total_row_size += val.bytes.size();
                }
            }
            table_stats.avg_row_size =
                sample_data.empty() ? 0.0
                                    : static_cast<double>(total_row_size) / sample_data.size();

            // Analyze each column
            for (size_t col_idx = 0;
                 col_idx < column_names.size() && col_idx < sample_data[0].size(); ++col_idx) {
                const std::string& column_name = column_names[col_idx];

                // Extract column values
                std::vector<Value> column_values;
                for (const auto& row : sample_data) {
                    if (col_idx < row.size()) {
                        column_values.push_back(row[col_idx]);
                    }
                }

                // Collect column statistics
                auto col_stats = collect_column_statistics(column_values);
                table_stats.column_stats[column_name] = col_stats;

                // Store individual column statistics
                store_column_statistics(schema, table, column_name, col_stats);

                std::fprintf(stderr, "[ANALYZE] Column %s: %lu distinct, %lu null, %lu total\n",
                             column_name.c_str(), static_cast<unsigned long>(col_stats.n_distinct),
                             static_cast<unsigned long>(col_stats.n_null),
                             static_cast<unsigned long>(col_stats.n_total));
            }

            // Store table statistics
            store_table_statistics(schema, table, table_stats);

            std::fprintf(stderr,
                         "[ANALYZE] Completed analysis of %s.%s: %lu rows, %.2f avg row size\n",
                         schema.c_str(), table.c_str(),
                         static_cast<unsigned long>(table_stats.n_rows), table_stats.avg_row_size);

            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[ANALYZE] Error analyzing table %s.%s: %s\n", schema.c_str(),
                         table.c_str(), e.what());
            return false;
        }
    }

    ColumnStatistics
    StatisticsCollector::collect_column_statistics(const std::vector<Value>& values)
    {
        ColumnStatistics stats;

        if (values.empty()) {
            return stats;
        }

        stats.n_total = values.size();
        stats.last_analyzed = std::time(nullptr);
        stats.sample_size = values.size();

        // Count NULLs and collect non-null values
        std::vector<std::string> non_null_values;
        std::uint64_t total_width = 0;

        for (const auto& val : values) {
            total_width += val.bytes.size();

            if (val.is_null) {
                stats.n_null++;
            } else {
                non_null_values.push_back(val.bytes);
            }
        }

        stats.avg_width =
            stats.n_total > 0 ? static_cast<double>(total_width) / stats.n_total : 0.0;

        if (non_null_values.empty()) {
            return stats;
        }

        // Sort for min/max and histogram
        std::sort(non_null_values.begin(), non_null_values.end());
        stats.min_value = non_null_values.front();
        stats.max_value = non_null_values.back();

        // Count distinct values
        auto unique_end = std::unique(non_null_values.begin(), non_null_values.end());
        stats.n_distinct = std::distance(non_null_values.begin(), unique_end);

        // Restore original (unsorted) values for MCV calculation
        std::vector<Value> original_non_null;
        for (const auto& val : values) {
            if (!val.is_null) {
                original_non_null.push_back(val);
            }
        }

        // Compute Most Common Values
        stats.most_common_values = compute_most_common_values(original_non_null);

        // Compute Histogram
        stats.histogram = compute_histogram(original_non_null);

        return stats;
    }

    std::vector<ColumnStatistics::MCV>
    StatisticsCollector::compute_most_common_values(const std::vector<Value>& values,
                                                    std::size_t max_mcvs)
    {
        std::unordered_map<std::string, std::uint64_t> value_counts;

        // Count frequencies
        for (const auto& val : values) {
            if (!val.is_null) {
                value_counts[val.bytes]++;
            }
        }

        // Convert to vector and sort by frequency
        std::vector<std::pair<std::string, std::uint64_t>> freq_pairs(value_counts.begin(),
                                                                      value_counts.end());
        std::sort(freq_pairs.begin(), freq_pairs.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        // Build MCV list
        std::vector<ColumnStatistics::MCV> mcvs;
        std::size_t total_non_null = values.size();

        for (size_t i = 0; i < std::min(max_mcvs, freq_pairs.size()); ++i) {
            ColumnStatistics::MCV mcv;
            mcv.value = freq_pairs[i].first;
            mcv.frequency = freq_pairs[i].second;
            mcv.fraction =
                total_non_null > 0 ? static_cast<double>(mcv.frequency) / total_non_null : 0.0;

            // Only include if frequency is significant
            if (mcv.fraction >= 0.01) { // At least 1% frequency
                mcvs.push_back(mcv);
            }
        }

        return mcvs;
    }

    std::vector<ColumnStatistics::HistogramBucket>
    StatisticsCollector::compute_histogram(const std::vector<Value>& values,
                                           std::size_t num_buckets)
    {
        std::vector<ColumnStatistics::HistogramBucket> histogram;

        if (values.size() < num_buckets * 2) {
            return histogram; // Not enough data for meaningful histogram
        }

        // Extract and sort non-null values
        std::vector<std::string> sorted_values;
        for (const auto& val : values) {
            if (!val.is_null) {
                sorted_values.push_back(val.bytes);
            }
        }

        if (sorted_values.empty()) {
            return histogram;
        }

        std::sort(sorted_values.begin(), sorted_values.end());

        // Create equal-depth histogram buckets
        std::size_t bucket_size = sorted_values.size() / num_buckets;
        if (bucket_size == 0)
            bucket_size = 1;

        for (std::size_t i = 0; i < num_buckets && i * bucket_size < sorted_values.size(); ++i) {
            ColumnStatistics::HistogramBucket bucket;

            std::size_t start_idx = i * bucket_size;
            std::size_t end_idx = std::min((i + 1) * bucket_size, sorted_values.size());

            bucket.lower_bound = sorted_values[start_idx];
            bucket.upper_bound = sorted_values[end_idx - 1];
            bucket.frequency = end_idx - start_idx;
            bucket.fraction = static_cast<double>(bucket.frequency) / sorted_values.size();

            histogram.push_back(bucket);
        }

        return histogram;
    }

    std::vector<std::vector<Value>>
    StatisticsCollector::sample_table_data(const std::string& schema, const std::string& table,
                                           std::size_t max_samples)
    {
        std::vector<std::vector<Value>> sample_data;

        try {
            // Use HeapRelation to scan the table
            CatalogManager cm(db_path_);
            auto soid = cm.lookup_schema_oid_by_name(schema);
            if (!soid)
                return sample_data;

            auto root = cm.get_relation_root_page_by_name(soid, table);
            if (!root)
                return sample_data;

            auto column_names = cm.list_column_names_by_name(*soid, table);
            if (column_names.empty())
                return sample_data;

            // Set up heap scanning (same pattern as SeqScanNode)
            FileOptions fo{};
            fo.direct_io = false;
            auto fh = FileManager::open(db_path_ + ".seg0", fo, false);
            std::vector<std::uint8_t> hb(4096, 0);
            FileManager::pread(fh, hb.data(), hb.size(), 0);
            auto* hh = reinterpret_cast<const ods::PageHeader*>(hb.data());
            std::uint32_t ps = hh->page_size ? hh->page_size : 4096u;

            FileMap::Layout layout{};
            layout.page_size = ps;
            layout.pages_per_segment = 262144;
            layout.options.direct_io = false;

            auto s = db_path_.find_last_of('/');
            std::string dir = (s == std::string::npos) ? std::string(".") : db_path_.substr(0, s);
            std::string base = (s == std::string::npos) ? db_path_ : db_path_.substr(s + 1);

            FileMap fm(layout);
            fm.set_base_path(dir, base);

            TupleLayout tuple_layout;
            for (size_t i = 0; i < column_names.size(); ++i) {
                tuple_layout.attrs.push_back({AttrType::VarBytes, 0, false, true});
            }

            auto hrel = HeapRelation::open(std::move(fm), ps, *root, tuple_layout);
            auto scanner = hrel.open_scan();

            // Scan data with sampling
            std::vector<Value> row;
            ods::RowId rid{};
            std::uint64_t row_count = 0;

            // Simple sampling: take every Nth row for large tables
            std::uint64_t sampling_rate = 1;

            while (scanner.next(row, &rid)) {
                row_count++;

                if (row_count % sampling_rate == 0) {
                    sample_data.push_back(row);

                    if (sample_data.size() >= max_samples) {
                        // Increase sampling rate to stay under limit
                        sampling_rate *= 2;

                        // Keep only half the samples
                        std::vector<std::vector<Value>> reduced_sample;
                        for (size_t i = 0; i < sample_data.size(); i += 2) {
                            reduced_sample.push_back(sample_data[i]);
                        }
                        sample_data = std::move(reduced_sample);
                    }
                }
            }

            std::fprintf(stderr,
                         "[ANALYZE] Sampled %zu rows from %lu total in %s.%s (rate: 1/%lu)\n",
                         sample_data.size(), static_cast<unsigned long>(row_count), schema.c_str(),
                         table.c_str(), static_cast<unsigned long>(sampling_rate));

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[ANALYZE] Error sampling table %s.%s: %s\n", schema.c_str(),
                         table.c_str(), e.what());
        }

        return sample_data;
    }

    bool StatisticsCollector::store_table_statistics(const std::string& schema,
                                                     const std::string& table,
                                                     const TableStatistics& stats)
    {
        try {
            CatalogManager cm(db_path_);
            std::string key = make_stats_key(schema, table);
            std::string json = stats.to_json();

            // Store in catalog using the existing set_stats method
            auto soid = cm.lookup_schema_oid_by_name(schema);
            if (!soid)
                return false;

            auto oid = cm.lookup_object_oid(*soid, "RELATION", table);
            if (!oid)
                return false;

            return cm.set_stats(*oid, json);

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[ANALYZE] Error storing table statistics for %s.%s: %s\n",
                         schema.c_str(), table.c_str(), e.what());
            return false;
        }
    }

    bool StatisticsCollector::store_column_statistics(const std::string& schema,
                                                      const std::string& table,
                                                      const std::string& column,
                                                      const ColumnStatistics& stats)
    {
        try {
            CatalogManager cm(db_path_);
            std::string key = make_stats_key(schema, table, column);
            std::string json = stats.to_json();

            // For column stats, we'll use a composite key approach
            // Store under a generated OID for this column statistic
            UuidBytes column_stats_oid{};
            {
                std::hash<std::string> h;
                auto v = h(key);
                memcpy(column_stats_oid.data(), &v, std::min(sizeof(v), column_stats_oid.size()));
            }

            return cm.set_stats(column_stats_oid, json);

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[ANALYZE] Error storing column statistics for %s.%s.%s: %s\n",
                         schema.c_str(), table.c_str(), column.c_str(), e.what());
            return false;
        }
    }

    TableStatistics StatisticsCollector::get_table_statistics(const std::string& schema,
                                                              const std::string& table)
    {
        TableStatistics stats;
        try {
            CatalogManager cm(db_path_);
            auto soid = cm.lookup_schema_oid_by_name(schema);
            if (!soid)
                return stats;

            auto oid = cm.lookup_object_oid(*soid, "RELATION", table);
            if (!oid)
                return stats;

            auto json = cm.get_stats(*oid);
            if (json.has_value()) {
                stats = TableStatistics::from_json(*json);
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[ANALYZE] Error retrieving table statistics for %s.%s: %s\n",
                         schema.c_str(), table.c_str(), e.what());
        }
        return stats;
    }

    ColumnStatistics StatisticsCollector::get_column_statistics(const std::string& schema,
                                                                const std::string& table,
                                                                const std::string& column)
    {
        ColumnStatistics stats;
        try {
            CatalogManager cm(db_path_);
            std::string key = make_stats_key(schema, table, column);

            // Generate same OID as used for storage
            UuidBytes column_stats_oid{};
            {
                std::hash<std::string> h;
                auto v = h(key);
                memcpy(column_stats_oid.data(), &v, std::min(sizeof(v), column_stats_oid.size()));
            }

            auto json = cm.get_stats(column_stats_oid);
            if (json.has_value()) {
                stats = ColumnStatistics::from_json(*json);
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[ANALYZE] Error retrieving column statistics for %s.%s.%s: %s\n",
                         schema.c_str(), table.c_str(), column.c_str(), e.what());
        }
        return stats;
    }

    std::string StatisticsCollector::make_stats_key(const std::string& schema,
                                                    const std::string& table,
                                                    const std::string& column)
    {
        if (column.empty()) {
            return "TABLE:" + schema + "." + table;
        } else {
            return "COLUMN:" + schema + "." + table + "." + column;
        }
    }

    // ========== High-level Interface Functions ==========

    bool execute_analyze_command(const std::string& sql)
    {
        // Simple ANALYZE command parsing
        // Production would integrate with the main SQL parser

        std::string lower_sql = sql;
        std::transform(lower_sql.begin(), lower_sql.end(), lower_sql.begin(), ::tolower);

        if (lower_sql.find("analyze") != 0) {
            return false;
        }

        // Extract table name from "ANALYZE [schema.]table"
        std::istringstream iss(sql);
        std::string analyze_keyword, table_spec;
        iss >> analyze_keyword >> table_spec;

        std::string schema = "public";
        std::string table = table_spec;

        auto dot_pos = table_spec.find('.');
        if (dot_pos != std::string::npos) {
            schema = table_spec.substr(0, dot_pos);
            table = table_spec.substr(dot_pos + 1);
        }

        // Execute analysis
        StatisticsCollector collector(get_executor_db_path());
        return collector.analyze_table(schema, table);
    }

} // namespace scratchbird::engine
