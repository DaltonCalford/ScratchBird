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
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/lexer.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

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

// OPT-3: Helper to extract predicate columns from WHERE clause expressions
static void extractPredicateColumnsForIndex(const parser::Expression* expr,
                                             const parser::StringPool& string_pool,
                                             std::vector<std::pair<std::string, std::string>>& table_column_pairs,
                                             std::unordered_set<std::string>& equality_columns,
                                             std::unordered_set<std::string>& range_columns)
{
    if (!expr) return;

    if (expr->kind() == parser::ASTKind::BINARY_OP)
    {
        auto* bin_expr = static_cast<const parser::BinaryOpExpr*>(expr);

        // Check if this is a comparison operator (EQ, LT, LE, GT, GE, NE, LIKE)
        auto op = bin_expr->op();
        bool is_comparison = (op == parser::BinaryOp::EQ || op == parser::BinaryOp::NE ||
                              op == parser::BinaryOp::LT || op == parser::BinaryOp::LE ||
                              op == parser::BinaryOp::GT || op == parser::BinaryOp::GE ||
                              op == parser::BinaryOp::LIKE);

        if (is_comparison)
        {
            // Extract column from left side
            auto* left = bin_expr->left();
            auto* right = bin_expr->right();

            // Check if left is identifier and right is a literal/constant
            if (left && left->kind() == parser::ASTKind::IDENTIFIER)
            {
                auto* id_expr = static_cast<const parser::IdentifierExpr*>(left);
                std::string col_name(string_pool.get(id_expr->name()));
                std::string table_name;
                if (id_expr->qualifier() != 0)
                {
                    table_name = std::string(string_pool.get(id_expr->qualifier()));
                }

                table_column_pairs.push_back({table_name, col_name});

                // Classify as equality or range predicate
                if (op == parser::BinaryOp::EQ)
                {
                    equality_columns.insert(col_name);
                }
                else if (op == parser::BinaryOp::LT || op == parser::BinaryOp::LE ||
                         op == parser::BinaryOp::GT || op == parser::BinaryOp::GE)
                {
                    range_columns.insert(col_name);
                }
            }

            // Check if right is identifier (for cases like "5 = col")
            if (right && right->kind() == parser::ASTKind::IDENTIFIER)
            {
                auto* id_expr = static_cast<const parser::IdentifierExpr*>(right);
                std::string col_name(string_pool.get(id_expr->name()));
                std::string table_name;
                if (id_expr->qualifier() != 0)
                {
                    table_name = std::string(string_pool.get(id_expr->qualifier()));
                }

                table_column_pairs.push_back({table_name, col_name});

                if (op == parser::BinaryOp::EQ)
                {
                    equality_columns.insert(col_name);
                }
                else if (op == parser::BinaryOp::LT || op == parser::BinaryOp::LE ||
                         op == parser::BinaryOp::GT || op == parser::BinaryOp::GE)
                {
                    range_columns.insert(col_name);
                }
            }
        }
        else
        {
            // Recurse into AND/OR expressions
            extractPredicateColumnsForIndex(bin_expr->left(), string_pool, table_column_pairs,
                                            equality_columns, range_columns);
            extractPredicateColumnsForIndex(bin_expr->right(), string_pool, table_column_pairs,
                                            equality_columns, range_columns);
        }
    }
}

Status IndexAdvisor::suggestIndexesForQuery(const std::string& sql_text,
                                             std::vector<IndexRecommendation>* recommendations,
                                             ErrorContext* ctx)
{
    // OPT-3: Parse query and analyze predicates for index recommendations
    if (!recommendations) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null recommendations output");
        return Status::INVALID_ARGUMENT;
    }

    recommendations->clear();

    if (sql_text.empty()) {
        return Status::OK;
    }

    // Parse the SQL query
    parser::Lexer lexer(sql_text);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (!result.statement()) {
        // Failed to parse - not an error, just can't make recommendations
        return Status::OK;
    }

    // Only analyze SELECT statements for now
    if (result.statement()->kind() != parser::ASTKind::SELECT) {
        return Status::OK;
    }

    auto* select_stmt = static_cast<parser::SelectStmt*>(result.statement());
    const auto& string_pool = parser.stringPool();

    // Extract table name from FROM clause
    std::string base_table_name(string_pool.get(select_stmt->fromClause().base_table.table_name));

    // Extract predicate columns from WHERE clause
    std::vector<std::pair<std::string, std::string>> table_column_pairs;
    std::unordered_set<std::string> equality_columns;
    std::unordered_set<std::string> range_columns;

    if (select_stmt->whereClause()) {
        extractPredicateColumnsForIndex(select_stmt->whereClause(), string_pool,
                                        table_column_pairs, equality_columns, range_columns);
    }

    // Extract join condition columns
    for (const auto& join : select_stmt->fromClause().joins) {
        if (join.on_condition) {
            extractPredicateColumnsForIndex(join.on_condition, string_pool,
                                            table_column_pairs, equality_columns, range_columns);
        }
    }

    // Build recommendations for columns that could benefit from indexes
    // Prioritize: 1) equality predicates, 2) range predicates, 3) join columns
    std::unordered_set<std::string> recommended_columns;

    // Get table info for ID lookup
    ID default_schema_id{};
    default_schema_id.bytes[1] = 1;  // Public schema

    CatalogManager::TableInfo table_info;
    if (!catalog_ || catalog_->getTable(default_schema_id, base_table_name, table_info, ctx) != Status::OK) {
        // Can't find table - still generate recommendation without table ID
        table_info.table_id = ID{};
    }

    // Check existing indexes for this table
    // Note: IndexInfo stores column_ids (IDs), so we would need column name lookup
    // For simplicity, we use the index_name to infer which columns might be indexed
    std::unordered_set<std::string> indexed_columns;
    if (catalog_) {
        auto existing_indexes = catalog_->getTableIndexes(table_info.table_id, ctx);
        for (const auto& idx : existing_indexes) {
            // Try to extract column name from index name (convention: idx_tablename_colname)
            // This is a heuristic - not perfect but avoids ID lookup overhead
            size_t last_underscore = idx.index_name.rfind('_');
            if (last_underscore != std::string::npos && last_underscore > 0) {
                std::string potential_col = idx.index_name.substr(last_underscore + 1);
                if (!potential_col.empty()) {
                    indexed_columns.insert(potential_col);
                }
            }
        }
    }

    // Recommend indexes for equality columns first (highest priority)
    for (const auto& col : equality_columns) {
        if (indexed_columns.find(col) == indexed_columns.end() &&
            recommended_columns.find(col) == recommended_columns.end()) {

            IndexRecommendation rec;
            rec.table_id = table_info.table_id;
            rec.table_name = base_table_name;
            rec.column_names.push_back(col);
            rec.type = IndexRecommendationType::CREATE_BTREE;
            rec.index_name = "idx_" + base_table_name + "_" + col;
            rec.create_sql = "CREATE INDEX " + rec.index_name + " ON " + base_table_name + " (" + col + ")";
            rec.benefit_score = 80.0;  // High benefit for equality predicates
            rec.cost_score = 10.0;
            rec.net_benefit = rec.benefit_score - rec.cost_score;
            rec.priority = 85.0;
            rec.confidence = 0.8;
            rec.estimated_speedup = 10.0;  // Index can be 10x faster than seq scan

            recommendations->push_back(rec);
            recommended_columns.insert(col);
        }
    }

    // Recommend indexes for range columns (medium priority)
    for (const auto& col : range_columns) {
        if (indexed_columns.find(col) == indexed_columns.end() &&
            recommended_columns.find(col) == recommended_columns.end()) {

            IndexRecommendation rec;
            rec.table_id = table_info.table_id;
            rec.table_name = base_table_name;
            rec.column_names.push_back(col);
            rec.type = IndexRecommendationType::CREATE_BTREE;
            rec.index_name = "idx_" + base_table_name + "_" + col;
            rec.create_sql = "CREATE INDEX " + rec.index_name + " ON " + base_table_name + " (" + col + ")";
            rec.benefit_score = 60.0;  // Medium benefit for range predicates
            rec.cost_score = 10.0;
            rec.net_benefit = rec.benefit_score - rec.cost_score;
            rec.priority = 65.0;
            rec.confidence = 0.7;
            rec.estimated_speedup = 5.0;

            recommendations->push_back(rec);
            recommended_columns.insert(col);
        }
    }

    // Consider composite index if multiple equality columns exist
    if (equality_columns.size() >= 2) {
        std::vector<std::string> composite_cols(equality_columns.begin(), equality_columns.end());
        // Sort for consistent naming
        std::sort(composite_cols.begin(), composite_cols.end());

        // Only suggest if a composite index doesn't already exist
        // (heuristic: check if index name contains both column names)
        bool already_has_composite = false;
        // Simple check: if both columns are already individually indexed, skip composite
        // This is a conservative heuristic
        if (indexed_columns.count(composite_cols[0]) > 0 &&
            indexed_columns.count(composite_cols[1]) > 0) {
            // Both columns already indexed individually - lower priority for composite
            already_has_composite = false;  // Still suggest but will have lower priority
        }

        if (!already_has_composite) {
            IndexRecommendation rec;
            rec.table_id = table_info.table_id;
            rec.table_name = base_table_name;
            rec.column_names = composite_cols;
            rec.type = IndexRecommendationType::CREATE_BTREE;

            std::string col_list = composite_cols[0];
            std::string name_suffix = composite_cols[0];
            for (size_t i = 1; i < composite_cols.size() && i < 3; ++i) {
                col_list += ", " + composite_cols[i];
                name_suffix += "_" + composite_cols[i];
            }

            rec.index_name = "idx_" + base_table_name + "_" + name_suffix;
            rec.create_sql = "CREATE INDEX " + rec.index_name + " ON " + base_table_name + " (" + col_list + ")";
            rec.benefit_score = 90.0;  // High benefit for composite covering multiple predicates
            rec.cost_score = 15.0;
            rec.net_benefit = rec.benefit_score - rec.cost_score;
            rec.priority = 75.0;  // Lower priority than single-column since more expensive
            rec.confidence = 0.75;
            rec.estimated_speedup = 15.0;

            recommendations->push_back(rec);
        }
    }

    // Sort recommendations by priority (highest first)
    std::sort(recommendations->begin(), recommendations->end(),
              [](const IndexRecommendation& a, const IndexRecommendation& b) {
                  return a.priority > b.priority;
              });

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

// OPT-M8: Helper function to estimate column width from data type
static double estimateColumnWidthFromType(uint16_t data_type, uint32_t type_precision)
{
    // Based on DataType enum values from types.h
    switch (data_type) {
        // Numeric types (1-9)
        case 1:  return 1.0;   // INT8
        case 2:  return 2.0;   // INT16
        case 3:  return 4.0;   // INT32
        case 4:  return 8.0;   // INT64
        case 5:  return 16.0;  // INT128
        case 6:  return 1.0;   // UINT8
        case 7:  return 2.0;   // UINT16
        case 8:  return 4.0;   // UINT32
        case 9:  return 8.0;   // UINT64
        case 10: return 4.0;   // FLOAT32
        case 11: return 8.0;   // FLOAT64
        case 12: return 16.0;  // DECIMAL (varies, assume 16)
        case 13: return 8.0;   // MONEY

        // String types (20-29)
        case 20: return type_precision > 0 ? static_cast<double>(type_precision) : 16.0;  // CHAR
        case 21: return type_precision > 0 ? static_cast<double>(type_precision) / 2.0 : 32.0;  // VARCHAR (assume 50% fill)
        case 22: return 64.0;  // TEXT (variable, estimate average)

        // Binary types (30-39)
        case 30: return type_precision > 0 ? static_cast<double>(type_precision) : 16.0;  // BINARY
        case 31: return type_precision > 0 ? static_cast<double>(type_precision) / 2.0 : 32.0;  // VARBINARY
        case 32: return 256.0; // BLOB (usually large, estimate)
        case 33: return 64.0;  // BYTEA

        // Date/Time types (40-49)
        case 40: return 4.0;   // DATE
        case 41: return 8.0;   // TIME
        case 42: return 8.0;   // TIMESTAMP
        case 43: return 16.0;  // INTERVAL

        // Boolean type (50)
        case 50: return 1.0;   // BOOL

        // UUID (60)
        case 60: return 16.0;  // UUID

        // JSON types (70-79)
        case 70: return 128.0; // JSON (variable, estimate)
        case 71: return 128.0; // JSONB

        // Spatial types (80-89)
        case 80: return 24.0;  // POINT (16 bytes + SRID)
        case 81: return 48.0;  // LINE
        case 82: return 64.0;  // POLYGON (varies)
        case 83: return 64.0;  // GEOMETRY

        // Network types (90-99)
        case 90: return 4.0;   // INET (IPv4)
        case 91: return 16.0;  // INET6 (IPv6)
        case 92: return 6.0;   // MACADDR

        default: return 8.0;   // Unknown type, assume 8 bytes
    }
}

double IndexAdvisor::estimateIndexSize(const ID& table_id, const std::vector<ID>& column_ids)
{
    // OPT-M8: Estimate index size based on table row count and column widths
    // Try statistics manager first, then fall back to catalog metadata

    uint64_t num_rows = 0;
    bool have_row_count = false;

    // Try to get row count from statistics
    TableStatistics ts;
    if (db_->statistics_manager()->getTableStatistics(table_id, ts, nullptr) == Status::OK) {
        num_rows = ts.num_rows;
        have_row_count = true;
    }

    // Fall back to catalog for row count if statistics unavailable
    if (!have_row_count && catalog_) {
        CatalogManager::TableInfo table_info;
        if (catalog_->getTable(table_id, table_info, nullptr) == Status::OK) {
            num_rows = table_info.row_count;
            have_row_count = true;
        }
    }

    // If we still don't have row count, return 0
    if (!have_row_count || num_rows == 0) {
        return 0.0;
    }

    // B-tree index overhead: 16 bytes per entry (pointers, node overhead)
    double bytes_per_entry = 16.0;

    // Get column information from catalog
    std::vector<CatalogManager::ColumnInfo> all_columns;
    if (catalog_) {
        catalog_->getColumns(table_id, all_columns, nullptr);
    }

    for (const auto& col_id : column_ids) {
        // First try to get avg_width from column statistics
        ColumnStatistics cs;
        if (db_->statistics_manager()->getColumnStatistics(table_id, col_id, cs, nullptr) == Status::OK) {
            bytes_per_entry += cs.avg_width;
        } else {
            // Fall back to estimating from column metadata
            bool found = false;
            for (const auto& col_info : all_columns) {
                if (col_info.column_id == col_id) {
                    bytes_per_entry += estimateColumnWidthFromType(col_info.data_type, col_info.type_precision);
                    found = true;
                    break;
                }
            }
            if (!found) {
                bytes_per_entry += 8.0; // Default assumption if column not found
            }
        }
    }

    // Return size in MB
    return static_cast<double>(num_rows) * bytes_per_entry / (1024.0 * 1024.0);
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
