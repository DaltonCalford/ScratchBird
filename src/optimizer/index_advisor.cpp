// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P2-25: Index Advisor Implementation
//
// November 25, 2025

#include "scratchbird/optimizer/index_advisor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/logger.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace scratchbird::optimizer {

using namespace scratchbird::core;

// =================================================================================================
// IndexAdvisor Implementation
// =================================================================================================

IndexAdvisor::IndexAdvisor(Database* db, const IndexAdvisorConfig& config)
    : db_(db), config_(config)
{
    if (db_) {
        catalog_ = db_->catalog_manager();
        cost_model_ = std::make_unique<CostModel>();
        initializeIndexStats();
    }

    LOG_INFO(GENERAL, "IndexAdvisor initialized");
}

IndexAdvisor::~IndexAdvisor() = default;

void IndexAdvisor::initializeIndexStats()
{
    if (!catalog_) return;

    std::lock_guard<std::mutex> lock(stats_mutex_);

    // Load all existing indexes from catalog
    auto now = std::chrono::steady_clock::now();

    // Get all schemas first
    std::vector<CatalogManager::SchemaInfo> schemas;
    ErrorContext ctx;
    auto status = catalog_->listSchemas(schemas, &ctx);
    if (status != Status::OK) return;

    // Get all tables from each schema
    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        status = catalog_->listTables(schema.schema_id, tables, &ctx);
        if (status != Status::OK) continue;

        for (const auto& table : tables) {
            std::vector<CatalogManager::IndexInfo> indexes;
            status = catalog_->listIndexesForTable(table.table_id, indexes, &ctx);
            if (status != Status::OK) continue;

            for (const auto& idx : indexes) {
                IndexUsageStats stats;
                stats.index_id = idx.index_id;
                stats.table_id = table.table_id;
                stats.index_name = idx.index_name;
                stats.table_name = table.table_name;
                stats.created_at = now; // Approximate
                // Detect primary key by naming convention (pk_ prefix or _pk/_pkey suffix) AND uniqueness
                stats.is_primary_key = idx.is_unique &&
                    (idx.index_name.find("pk_") == 0 ||
                     idx.index_name.find("_pk") != std::string::npos ||
                     idx.index_name.find("_pkey") != std::string::npos);
                stats.is_unique = idx.is_unique;
                index_stats_[idx.index_id] = stats;
            }
        }
    }
}

void IndexAdvisor::recordQuery(const std::string& sql_text, const ID& table_id,
                               const std::vector<ID>& column_ids, bool used_index,
                               double execution_time_ms, uint64_t rows_examined,
                               uint64_t rows_returned)
{
    if (!config_.track_query_patterns) return;

    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto now = std::chrono::steady_clock::now();

    // Update table stats
    auto& table_stats = table_stats_[table_id];
    table_stats.table_id = table_id;
    ++table_stats.total_queries;
    table_stats.rows_fetched += rows_returned;
    table_stats.last_query = now;
    if (used_index) {
        ++table_stats.index_scans;
    } else {
        ++table_stats.seq_scans;
    }

    // Update column usage
    for (const auto& col_id : column_ids) {
        auto& col_usage = table_stats.columns[col_id];
        col_usage.column_id = col_id;
        ++col_usage.total_queries;
    }

    // Record query pattern
    // Check if pattern already exists (simple text match)
    bool found = false;
    for (auto& pattern : query_patterns_) {
        if (pattern.sql_text == sql_text) {
            ++pattern.execution_count;
            pattern.total_time_ms += execution_time_ms;
            pattern.avg_time_ms = pattern.total_time_ms / pattern.execution_count;
            pattern.rows_examined += rows_examined;
            pattern.rows_returned += rows_returned;
            pattern.last_seen = now;
            found = true;
            break;
        }
    }

    if (!found) {
        QueryPattern pattern;
        pattern.sql_text = sql_text.substr(0, 500); // Truncate
        pattern.execution_count = 1;
        pattern.total_time_ms = execution_time_ms;
        pattern.avg_time_ms = execution_time_ms;
        pattern.rows_examined = rows_examined;
        pattern.rows_returned = rows_returned;
        pattern.used_index = used_index;
        pattern.table_id = table_id;
        pattern.column_ids = column_ids;
        pattern.first_seen = now;
        pattern.last_seen = now;
        query_patterns_.push_back(pattern);

        trimQueryPatterns();
    }
}

void IndexAdvisor::recordColumnUsage(const ID& table_id, const ID& column_id,
                                     const std::string& usage_type)
{
    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto& table_stats = table_stats_[table_id];
    auto& col_usage = table_stats.columns[column_id];
    col_usage.column_id = column_id;

    if (usage_type == "equality" || usage_type == "=") {
        ++col_usage.equality_count;
    } else if (usage_type == "range" || usage_type == ">" || usage_type == "<" ||
               usage_type == ">=" || usage_type == "<=" || usage_type == "between") {
        ++col_usage.range_count;
    } else if (usage_type == "like") {
        ++col_usage.like_count;
    } else if (usage_type == "in") {
        ++col_usage.in_list_count;
    } else if (usage_type == "join") {
        ++col_usage.join_count;
    } else if (usage_type == "order_by") {
        ++col_usage.order_by_count;
    } else if (usage_type == "group_by") {
        ++col_usage.group_by_count;
    } else if (usage_type == "select") {
        ++col_usage.select_count;
    }
}

void IndexAdvisor::recordIndexUsage(const ID& index_id, uint64_t tuples_read)
{
    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto it = index_stats_.find(index_id);
    if (it != index_stats_.end()) {
        ++it->second.scan_count;
        it->second.tuple_reads += tuples_read;
        it->second.last_used = std::chrono::steady_clock::now();
    }
}

void IndexAdvisor::recordIndexMaintenance(const ID& index_id)
{
    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto it = index_stats_.find(index_id);
    if (it != index_stats_.end()) {
        ++it->second.maintenance_cost;
    }
}

Status IndexAdvisor::analyze(std::vector<IndexRecommendation>* recommendations,
                             ErrorContext* ctx)
{
    if (!recommendations) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null recommendations output");
        return Status::INVALID_ARGUMENT;
    }

    recommendations->clear();

    std::lock_guard<std::mutex> lock(stats_mutex_);

    // Analyze each table with usage stats
    for (auto& [table_id, table_stats] : table_stats_) {
        // Skip tables with too few queries
        if (table_stats.total_queries < config_.min_query_count) {
            continue;
        }

        // Get table info
        CatalogManager::TableInfo table_info;
        ErrorContext local_ctx;
        auto status = catalog_->getTable(table_id, table_info, &local_ctx);
        if (status != Status::OK) continue;

        table_stats.table_name = table_info.table_name;

        // Get columns for this table
        std::vector<CatalogManager::ColumnInfo> columns;
        status = catalog_->getColumns(table_id, columns, &local_ctx);
        if (status != Status::OK) continue;

        // Analyze columns for index candidates
        for (auto& [col_id, col_usage] : table_stats.columns) {
            // Get column info
            auto col_it = std::find_if(columns.begin(), columns.end(),
                                      [&col_id](const CatalogManager::ColumnInfo& c) {
                                          return c.column_id == col_id;
                                      });
            if (col_it != columns.end()) {
                col_usage.column_name = col_it->column_name;
            }

            // Check if column would benefit from index
            if (!canBenefitFromIndex(col_usage)) {
                continue;
            }

            // Check if index already exists
            std::vector<ID> cols = {col_id};
            if (isIndexExisting(table_id, cols)) {
                continue;
            }

            // Generate recommendation
            auto rec = generateCreateRecommendation(table_stats, col_usage);
            if (rec.benefit_score >= config_.min_benefit_score &&
                rec.confidence >= config_.min_confidence) {
                recommendations->push_back(rec);
            }
        }

        // Look for composite index opportunities
        // Find columns that are frequently used together
        std::vector<const ColumnUsage*> high_usage_cols;
        for (const auto& [col_id, col_usage] : table_stats.columns) {
            if (col_usage.getScore() > 50.0) {
                high_usage_cols.push_back(&col_usage);
            }
        }

        if (high_usage_cols.size() >= 2) {
            // Sort by score descending
            std::sort(high_usage_cols.begin(), high_usage_cols.end(),
                     [](const ColumnUsage* a, const ColumnUsage* b) {
                         return a->getScore() > b->getScore();
                     });

            // Try pairs of columns
            for (size_t i = 0; i < high_usage_cols.size() && i < 3; ++i) {
                for (size_t j = i + 1; j < high_usage_cols.size() && j < 4; ++j) {
                    std::vector<const ColumnUsage*> pair = {high_usage_cols[i], high_usage_cols[j]};
                    std::vector<ID> cols = {pair[0]->column_id, pair[1]->column_id};
                    if (!isIndexExisting(table_id, cols)) {
                        auto rec = generateCompositeRecommendation(table_stats, pair);
                        if (rec.benefit_score >= config_.min_benefit_score) {
                            recommendations->push_back(rec);
                        }
                    }
                }
            }
        }
    }

    // Find unused indexes
    std::vector<IndexRecommendation> unused;
    findUnusedIndexes(&unused, ctx);
    recommendations->insert(recommendations->end(), unused.begin(), unused.end());

    // Sort by priority
    std::sort(recommendations->begin(), recommendations->end(),
             [](const IndexRecommendation& a, const IndexRecommendation& b) {
                 return a.priority > b.priority;
             });

    // Limit recommendations
    if (recommendations->size() > config_.max_recommendations) {
        recommendations->resize(config_.max_recommendations);
    }

    LOG_INFO(GENERAL, "IndexAdvisor generated %zu recommendations",
             recommendations->size());

    return Status::OK;
}

Status IndexAdvisor::analyzeTable(const ID& table_id,
                                  std::vector<IndexRecommendation>* recommendations,
                                  ErrorContext* ctx)
{
    if (!recommendations) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null recommendations output");
        return Status::INVALID_ARGUMENT;
    }

    recommendations->clear();

    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto it = table_stats_.find(table_id);
    if (it == table_stats_.end()) {
        // No usage data for this table
        return Status::OK;
    }

    auto& table_stats = it->second;

    // Get table info
    CatalogManager::TableInfo table_info;
    auto status = catalog_->getTable(table_id, table_info, ctx);
    if (status != Status::OK) return status;

    table_stats.table_name = table_info.table_name;

    // Get columns for this table
    std::vector<CatalogManager::ColumnInfo> columns;
    status = catalog_->getColumns(table_id, columns, ctx);
    if (status != Status::OK) return status;

    // Analyze each column
    for (auto& [col_id, col_usage] : table_stats.columns) {
        auto col_it = std::find_if(columns.begin(), columns.end(),
                                  [&col_id](const CatalogManager::ColumnInfo& c) {
                                      return c.column_id == col_id;
                                  });
        if (col_it != columns.end()) {
            col_usage.column_name = col_it->column_name;
        }

        if (!canBenefitFromIndex(col_usage)) continue;

        std::vector<ID> cols = {col_id};
        if (isIndexExisting(table_id, cols)) continue;

        auto rec = generateCreateRecommendation(table_stats, col_usage);
        if (rec.benefit_score >= config_.min_benefit_score) {
            recommendations->push_back(rec);
        }
    }

    return Status::OK;
}

Status IndexAdvisor::findUnusedIndexes(std::vector<IndexRecommendation>* recommendations,
                                        ErrorContext* ctx)
{
    if (!recommendations) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null recommendations output");
        return Status::INVALID_ARGUMENT;
    }

    auto now = std::chrono::steady_clock::now();
    auto threshold = std::chrono::hours(24 * config_.unused_threshold_days);

    std::lock_guard<std::mutex> lock(stats_mutex_);

    for (const auto& [index_id, stats] : index_stats_) {
        // Skip primary keys, unique constraints, and FK indexes
        if (stats.is_primary_key || stats.is_unique || stats.is_foreign_key) {
            continue;
        }

        // Check if unused
        auto time_since_use = std::chrono::duration_cast<std::chrono::hours>(
            now - stats.last_used);

        if (stats.scan_count == 0 || time_since_use > threshold) {
            auto rec = generateDropRecommendation(stats);
            recommendations->push_back(rec);
        }
    }

    return Status::OK;
}

Status IndexAdvisor::suggestIndexesForQuery(const std::string& sql_text,
                                             std::vector<IndexRecommendation>* recommendations,
                                             ErrorContext* ctx)
{
    // This would require query parsing - placeholder for now
    if (!recommendations) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null recommendations output");
        return Status::INVALID_ARGUMENT;
    }

    recommendations->clear();

    // TODO: Parse query and analyze predicates
    // For now, return empty list
    return Status::OK;
}

Status IndexAdvisor::getTableStats(const ID& table_id, TableUsageStats* stats,
                                   ErrorContext* ctx)
{
    if (!stats) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null stats output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto it = table_stats_.find(table_id);
    if (it == table_stats_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No stats for table");
        return Status::NOT_FOUND;
    }

    *stats = it->second;
    return Status::OK;
}

Status IndexAdvisor::getAllTableStats(std::vector<TableUsageStats>* stats,
                                      ErrorContext* ctx)
{
    if (!stats) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null stats output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats->clear();
    stats->reserve(table_stats_.size());
    for (const auto& [id, s] : table_stats_) {
        stats->push_back(s);
    }

    return Status::OK;
}

Status IndexAdvisor::getIndexStats(const ID& index_id, IndexUsageStats* stats,
                                   ErrorContext* ctx)
{
    if (!stats) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null stats output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto it = index_stats_.find(index_id);
    if (it == index_stats_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No stats for index");
        return Status::NOT_FOUND;
    }

    *stats = it->second;
    return Status::OK;
}

Status IndexAdvisor::getAllIndexStats(std::vector<IndexUsageStats>* stats,
                                      ErrorContext* ctx)
{
    if (!stats) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null stats output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats->clear();
    stats->reserve(index_stats_.size());
    for (const auto& [id, s] : index_stats_) {
        stats->push_back(s);
    }

    return Status::OK;
}

Status IndexAdvisor::getQueryPatterns(std::vector<QueryPattern>* patterns,
                                      ErrorContext* ctx)
{
    if (!patterns) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null patterns output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);
    *patterns = query_patterns_;
    return Status::OK;
}

Status IndexAdvisor::getTopSlowQueries(uint64_t limit, std::vector<QueryPattern>* patterns,
                                       ErrorContext* ctx)
{
    if (!patterns) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null patterns output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);

    // Copy and filter patterns without indexes
    std::vector<QueryPattern> slow_queries;
    for (const auto& p : query_patterns_) {
        if (!p.used_index) {
            slow_queries.push_back(p);
        }
    }

    // Sort by average time descending
    std::sort(slow_queries.begin(), slow_queries.end(),
             [](const QueryPattern& a, const QueryPattern& b) {
                 return a.avg_time_ms > b.avg_time_ms;
             });

    // Limit results
    if (slow_queries.size() > limit) {
        slow_queries.resize(limit);
    }

    *patterns = std::move(slow_queries);
    return Status::OK;
}

void IndexAdvisor::setConfig(const IndexAdvisorConfig& config)
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    config_ = config;
}

void IndexAdvisor::resetStats()
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    table_stats_.clear();
    for (auto& [id, stats] : index_stats_) {
        stats.scan_count = 0;
        stats.tuple_reads = 0;
        stats.maintenance_cost = 0;
    }
    query_patterns_.clear();
}

void IndexAdvisor::startWorkloadCapture()
{
    capturing_ = true;
    capture_start_ = std::chrono::steady_clock::now();
    resetStats();
    LOG_INFO(GENERAL, "IndexAdvisor workload capture started");
}

void IndexAdvisor::stopWorkloadCapture()
{
    capturing_ = false;
    LOG_INFO(GENERAL, "IndexAdvisor workload capture stopped after %ld seconds",
             getCaptureDuration().count());
}

std::chrono::seconds IndexAdvisor::getCaptureDuration() const
{
    if (!capturing_) return std::chrono::seconds(0);
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - capture_start_);
}

// Internal helpers

IndexRecommendation IndexAdvisor::generateCreateRecommendation(
    const TableUsageStats& table_stats, const ColumnUsage& col_usage)
{
    IndexRecommendation rec;
    rec.table_id = table_stats.table_id;
    rec.table_name = table_stats.table_name;
    rec.column_names = {col_usage.column_name};

    // Determine index type
    if (col_usage.equality_count > col_usage.range_count * 2) {
        rec.type = IndexRecommendationType::CREATE_HASH;
    } else {
        rec.type = IndexRecommendationType::CREATE_BTREE;
    }

    // Calculate benefit/cost
    rec.benefit_score = estimateIndexBenefit(table_stats, col_usage);
    rec.cost_score = estimateIndexCost(table_stats, col_usage);
    rec.net_benefit = rec.benefit_score - rec.cost_score;
    rec.affected_queries = col_usage.total_queries;

    // Estimate size (rough estimate: 20 bytes per row for single column)
    TableStatistics ts;
    if (db_->statistics_manager()->getTableStatistics(table_stats.table_id, ts, nullptr) == Status::OK) {
        rec.estimated_size_mb = ts.num_rows * 20.0 / (1024.0 * 1024.0);
    }

    // Calculate speedup factor
    if (table_stats.seq_scans > 0) {
        rec.estimated_speedup = std::min(100.0,
            static_cast<double>(table_stats.seq_scans) * col_usage.getScore() / 1000.0);
    }

    // Calculate priority and confidence
    rec.priority = rec.net_benefit * std::log10(col_usage.total_queries + 1);
    rec.confidence = std::min(1.0, col_usage.total_queries / 100.0);

    // Generate SQL
    rec.index_name = generateIndexName(table_stats.table_name, rec.column_names, rec.type);
    rec.create_sql = generateCreateSQL(rec);

    // Generate reason
    std::ostringstream reason;
    reason << "Column '" << col_usage.column_name << "' is used in ";
    if (col_usage.equality_count > 0) reason << col_usage.equality_count << " equality, ";
    if (col_usage.range_count > 0) reason << col_usage.range_count << " range, ";
    if (col_usage.join_count > 0) reason << col_usage.join_count << " join ";
    reason << "predicates. " << table_stats.seq_scans << " sequential scans could benefit.";
    rec.reason = reason.str();

    return rec;
}

IndexRecommendation IndexAdvisor::generateCompositeRecommendation(
    const TableUsageStats& table_stats, const std::vector<const ColumnUsage*>& columns)
{
    IndexRecommendation rec;
    rec.table_id = table_stats.table_id;
    rec.table_name = table_stats.table_name;
    rec.type = IndexRecommendationType::CREATE_COMPOSITE;

    for (const auto* col : columns) {
        rec.column_names.push_back(col->column_name);
    }

    // Calculate combined benefit
    double total_score = 0.0;
    uint64_t total_queries = 0;
    for (const auto* col : columns) {
        total_score += col->getScore();
        total_queries = std::max(total_queries, col->total_queries);
    }

    rec.benefit_score = total_score * 1.5; // Composite bonus
    rec.cost_score = estimateIndexCost(table_stats, *columns[0]) * 1.2;
    rec.net_benefit = rec.benefit_score - rec.cost_score;
    rec.affected_queries = total_queries;

    rec.priority = rec.net_benefit * std::log10(total_queries + 1);
    rec.confidence = std::min(1.0, total_queries / 100.0);

    rec.index_name = generateIndexName(table_stats.table_name, rec.column_names, rec.type);
    rec.create_sql = generateCreateSQL(rec);

    std::ostringstream reason;
    reason << "Composite index on (";
    for (size_t i = 0; i < rec.column_names.size(); ++i) {
        if (i > 0) reason << ", ";
        reason << rec.column_names[i];
    }
    reason << ") could cover multiple predicates efficiently.";
    rec.reason = reason.str();

    return rec;
}

IndexRecommendation IndexAdvisor::generateDropRecommendation(const IndexUsageStats& index_stats)
{
    IndexRecommendation rec;
    rec.table_id = index_stats.table_id;
    rec.table_name = index_stats.table_name;
    rec.index_name = index_stats.index_name;
    rec.type = IndexRecommendationType::DROP_UNUSED;

    rec.benefit_score = index_stats.size_pages * 0.1; // Space savings
    rec.cost_score = 0.0;
    rec.net_benefit = rec.benefit_score;
    rec.priority = rec.net_benefit;
    rec.confidence = 0.8;

    rec.drop_sql = generateDropSQL(index_stats.index_name);

    std::ostringstream reason;
    reason << "Index '" << index_stats.index_name << "' has not been used in "
           << config_.unused_threshold_days << " days. Scans: " << index_stats.scan_count
           << ", Maintenance ops: " << index_stats.maintenance_cost;
    rec.reason = reason.str();

    return rec;
}

double IndexAdvisor::estimateIndexBenefit(const TableUsageStats& table_stats,
                                          const ColumnUsage& col_usage)
{
    // Base benefit from usage patterns
    double benefit = col_usage.getScore();

    // Bonus for high seq_scan ratio
    if (table_stats.total_queries > 0) {
        double seq_ratio = static_cast<double>(table_stats.seq_scans) / table_stats.total_queries;
        benefit *= (1.0 + seq_ratio);
    }

    return benefit;
}

double IndexAdvisor::estimateIndexCost(const TableUsageStats& table_stats,
                                       const ColumnUsage& col_usage)
{
    // Rough estimate: maintenance cost proportional to writes
    // Assume 30% of queries are writes
    double write_ratio = 0.3;
    double maintenance = col_usage.total_queries * write_ratio * 0.5;

    return maintenance;
}

double IndexAdvisor::estimateIndexSize(const ID& table_id, const std::vector<ID>& column_ids)
{
    // Estimate index size based on table row count and column widths
    TableStatistics ts;
    if (db_->statistics_manager()->getTableStatistics(table_id, ts, nullptr) != Status::OK) {
        return 0.0;
    }

    // Rough estimate: 16 bytes overhead + column width per entry
    double bytes_per_entry = 16.0;
    for (const auto& col_id : column_ids) {
        ColumnStatistics cs;
        if (db_->statistics_manager()->getColumnStatistics(table_id, col_id, cs, nullptr) == Status::OK) {
            bytes_per_entry += cs.avg_width;
        } else {
            bytes_per_entry += 8.0; // Default assumption
        }
    }

    return ts.num_rows * bytes_per_entry / (1024.0 * 1024.0);
}

std::string IndexAdvisor::generateIndexName(const std::string& table_name,
                                            const std::vector<std::string>& column_names,
                                            IndexRecommendationType type)
{
    std::ostringstream name;
    name << "idx_" << table_name;
    for (const auto& col : column_names) {
        name << "_" << col;
    }
    return name.str();
}

std::string IndexAdvisor::generateCreateSQL(const IndexRecommendation& rec)
{
    std::ostringstream sql;
    sql << "CREATE ";

    switch (rec.type) {
        case IndexRecommendationType::CREATE_HASH:
            sql << "INDEX " << rec.index_name << " ON " << rec.table_name
                << " USING HASH (";
            break;
        case IndexRecommendationType::CREATE_LSM:
            sql << "INDEX " << rec.index_name << " ON " << rec.table_name
                << " USING LSM (";
            break;
        default:
            sql << "INDEX " << rec.index_name << " ON " << rec.table_name << " (";
            break;
    }

    for (size_t i = 0; i < rec.column_names.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << rec.column_names[i];
    }
    sql << ")";

    if (!rec.where_clause.empty()) {
        sql << " WHERE " << rec.where_clause;
    }

    return sql.str();
}

std::string IndexAdvisor::generateDropSQL(const std::string& index_name)
{
    return "DROP INDEX " + index_name;
}

bool IndexAdvisor::isIndexExisting(const ID& table_id, const std::vector<ID>& column_ids)
{
    // Check if an index already covers these columns
    std::vector<CatalogManager::IndexInfo> indexes;
    ErrorContext ctx;
    auto status = catalog_->listIndexesForTable(table_id, indexes, &ctx);
    if (status != Status::OK) return false;

    for (const auto& idx : indexes) {
        // Simple check: same columns in same order
        if (idx.column_ids.size() >= column_ids.size()) {
            bool match = true;
            for (size_t i = 0; i < column_ids.size(); ++i) {
                if (idx.column_ids[i] != column_ids[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }

    return false;
}

bool IndexAdvisor::isIndexUsed(const ID& index_id)
{
    auto it = index_stats_.find(index_id);
    return it != index_stats_.end() && it->second.scan_count > 0;
}

bool IndexAdvisor::canBenefitFromIndex(const ColumnUsage& col_usage)
{
    // Minimum score threshold for recommendation
    return col_usage.getScore() >= 20.0 &&
           (col_usage.equality_count > 0 || col_usage.range_count > 0 ||
            col_usage.join_count > 0 || col_usage.in_list_count > 0);
}

void IndexAdvisor::trimQueryPatterns()
{
    if (query_patterns_.size() <= config_.pattern_history_size) {
        return;
    }

    // Sort by last seen (oldest first)
    std::sort(query_patterns_.begin(), query_patterns_.end(),
             [](const QueryPattern& a, const QueryPattern& b) {
                 return a.last_seen < b.last_seen;
             });

    // Remove oldest patterns
    size_t to_remove = query_patterns_.size() - config_.pattern_history_size;
    query_patterns_.erase(query_patterns_.begin(), query_patterns_.begin() + to_remove);
}

} // namespace scratchbird::optimizer
