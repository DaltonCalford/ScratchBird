/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P2-25: Index Advisor Implementation
//
// V3 MIGRATION STATUS: COMPLETE
//
// November 25, 2025

#include "scratchbird/optimizer/index_advisor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/optimizer/index_family_lowering.h"
#include "scratchbird/parser/parser_v3.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <optional>

namespace scratchbird::optimizer {

using namespace scratchbird::core;

namespace {

enum class SignalKind : uint8_t {
    EQUALITY,
    RANGE,
    LIKE,
    IN_LIST,
    JOIN,
    ORDER_BY,
    GROUP_BY,
    SELECT_LIST,
};

struct SignalCounter {
    uint32_t equality = 0;
    uint32_t range = 0;
    uint32_t like = 0;
    uint32_t in_list = 0;
    uint32_t join = 0;
    uint32_t order_by = 0;
    uint32_t group_by = 0;
    uint32_t select_list = 0;

    uint32_t total() const {
        return equality + range + like + in_list + join + order_by + group_by + select_list;
    }
};

struct SignalEntry {
    std::string qualifier_lower;
    std::string column_lower;
    SignalCounter counts;
};

struct TableInput {
    parser::v3::SchemaPath table_path;
    std::string alias;
};

struct TableBinding {
    CatalogManager::TableInfo table_info;
    std::string table_name_lower;
    std::string alias_lower;
    std::unordered_map<std::string, CatalogManager::ColumnInfo> columns_by_lower;
};

struct ColumnToken {
    std::string qualifier_lower;
    std::string column_lower;
};

struct AdvisorWhatIfPlan {
    PlannerFamilyLoweringResult lowering;
    std::string index_name;
    double total_cost = std::numeric_limits<double>::infinity();
    uint64_t estimated_rows = 0;
    bool covering = false;
    bool ordered_output = false;
    std::string detail;
};

struct AdvisorTableProfile {
    const TableBinding* binding = nullptr;
    const TableUsageStats* table_stats = nullptr;
    TableStatistics table_statistics;
    bool have_table_statistics = false;
    uint64_t table_rows = 0;
    uint64_t table_pages = 1;
    uint64_t row_width_bytes = 64;
    bool needs_ordering = false;
    uint32_t order_key_count = 0;
    uint32_t predicate_count = 0;
    std::vector<CatalogManager::IndexInfo> existing_indexes;
    std::unordered_set<ID, IDHash> referenced_column_ids;
};

std::string toLowerAscii(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return out;
}

std::string signalKey(const std::string& qualifier_lower, const std::string& column_lower) {
    return qualifier_lower + '\x1F' + column_lower;
}

auto recommendationIndexType(IndexRecommendationType type)
    -> CatalogManager::IndexType {
    switch (type) {
        case IndexRecommendationType::CREATE_HASH:
            return CatalogManager::IndexType::HASH;
        case IndexRecommendationType::CREATE_LSM:
            return CatalogManager::IndexType::LSM;
        default:
            return CatalogManager::IndexType::BTREE;
    }
}

bool containsId(const std::vector<ID>& ids, const ID& target) {
    return std::find(ids.begin(), ids.end(), target) != ids.end();
}

auto strongestPredicateShape(const ColumnUsage& usage) -> PredicateMatchShape {
    if (usage.equality_count > 0 || usage.join_count > 0 || usage.in_list_count > 0) {
        return PredicateMatchShape::EQUALITY;
    }
    if (usage.range_count > 0) {
        return PredicateMatchShape::RANGE;
    }
    if (usage.like_count > 0) {
        return PredicateMatchShape::LIKE_PREFIX;
    }
    return PredicateMatchShape::NONE;
}

auto operatorNameForShape(PredicateMatchShape shape) -> std::string {
    switch (shape) {
        case PredicateMatchShape::EQUALITY:
            return "=";
        case PredicateMatchShape::RANGE:
            return ">=";
        case PredicateMatchShape::LIKE_PREFIX:
            return "LIKE";
        case PredicateMatchShape::NONE:
        default:
            return "";
    }
}

double clampSelectivity(double value) {
    return std::clamp(value, 1e-6, 1.0);
}

auto estimatePagesForRows(uint64_t rows, double rows_per_page) -> uint64_t {
    if (rows == 0) {
        return 1;
    }
    if (rows_per_page <= 0.0) {
        rows_per_page = 64.0;
    }
    return std::max<uint64_t>(1, static_cast<uint64_t>(
        std::ceil(static_cast<double>(rows) / rows_per_page)));
}

auto defaultColumnSelectivity(const ColumnUsage& usage,
                              const ColumnStatistics* stats) -> double {
    const double distinct = stats && stats->num_distinct > 0
        ? static_cast<double>(stats->num_distinct)
        : 100.0;
    if (usage.equality_count > 0 || usage.join_count > 0) {
        return clampSelectivity(1.0 / std::max(2.0, distinct));
    }
    if (usage.in_list_count > 0) {
        return clampSelectivity(std::min(0.25, 4.0 / std::max(4.0, distinct)));
    }
    if (usage.range_count > 0) {
        return 0.18;
    }
    if (usage.like_count > 0) {
        return 0.12;
    }
    return 1.0;
}

auto estimateCompositeSelectivity(StatisticsManager* stats_manager,
                                  const AdvisorTableProfile& profile,
                                  const std::vector<ID>& column_ids,
                                  const std::unordered_map<ID, ColumnStatistics, IDHash>&
                                      column_stats)
    -> double {
    if (column_ids.empty()) {
        return 1.0;
    }

    double combined = 1.0;
    for (const auto& column_id : column_ids) {
        const auto usage_it = profile.table_stats->columns.find(column_id);
        if (usage_it == profile.table_stats->columns.end()) {
            continue;
        }
        const auto stats_it = column_stats.find(column_id);
        const ColumnStatistics* stats = stats_it != column_stats.end()
            ? &stats_it->second
            : nullptr;
        combined *= defaultColumnSelectivity(usage_it->second, stats);
    }

    if (column_ids.size() >= 2 && stats_manager != nullptr) {
        MultivariateStatistics multi;
        if (stats_manager->getMultivariateStatistics(profile.binding->table_info.table_id,
                                                     column_ids,
                                                     multi,
                                                     nullptr) == Status::OK &&
            multi.ndistinct.num_distinct > 0) {
            combined = std::min(
                combined,
                clampSelectivity(
                    1.0 / static_cast<double>(multi.ndistinct.num_distinct)));
        }
    }

    return clampSelectivity(combined);
}

bool loadAdvisorTableProfile(Database* db,
                             CatalogManager* catalog,
                             const TableBinding& binding,
                             const TableUsageStats& table_stats,
                             AdvisorTableProfile& profile_out) {
    if (db == nullptr || catalog == nullptr) {
        return false;
    }

    profile_out = AdvisorTableProfile{};
    profile_out.binding = &binding;
    profile_out.table_stats = &table_stats;
    for (const auto& [column_id, _] : table_stats.columns) {
        profile_out.referenced_column_ids.insert(column_id);
    }

    profile_out.needs_ordering = std::any_of(
        table_stats.columns.begin(),
        table_stats.columns.end(),
        [](const auto& entry) { return entry.second.order_by_count > 0; });
    profile_out.order_key_count = static_cast<uint32_t>(std::count_if(
        table_stats.columns.begin(),
        table_stats.columns.end(),
        [](const auto& entry) { return entry.second.order_by_count > 0; }));
    profile_out.predicate_count = static_cast<uint32_t>(std::count_if(
        table_stats.columns.begin(),
        table_stats.columns.end(),
        [](const auto& entry) {
            const auto& usage = entry.second;
            return usage.equality_count > 0 || usage.range_count > 0 ||
                   usage.like_count > 0 || usage.in_list_count > 0 ||
                   usage.join_count > 0;
        }));

    if (db->statistics_manager()->getTableStatistics(binding.table_info.table_id,
                                                     profile_out.table_statistics,
                                                     nullptr) == Status::OK) {
        profile_out.have_table_statistics = true;
        if (profile_out.table_statistics.avg_row_size > 0.0f) {
            profile_out.row_width_bytes = std::max<uint64_t>(
                16,
                static_cast<uint64_t>(
                    std::ceil(profile_out.table_statistics.avg_row_size)));
        }
        profile_out.table_rows = profile_out.table_statistics.num_rows == 0
            ? 1000
            : profile_out.table_statistics.num_rows;
        profile_out.table_pages = profile_out.table_statistics.num_pages == 0
            ? estimatePagesForRows(
                  profile_out.table_rows,
                  std::max(1.0,
                           8192.0 / static_cast<double>(profile_out.row_width_bytes)))
            : profile_out.table_statistics.num_pages;
    } else {
        profile_out.table_rows = std::max<uint64_t>(
            1, binding.table_info.row_count == 0 ? 1000 : binding.table_info.row_count);
        profile_out.row_width_bytes = 64;
        profile_out.table_pages = estimatePagesForRows(
            profile_out.table_rows,
            std::max(1.0, 8192.0 / static_cast<double>(profile_out.row_width_bytes)));
    }

    catalog->listIndexesForTable(binding.table_info.table_id,
                                 profile_out.existing_indexes,
                                 nullptr);
    return true;
}

bool isCoveringIndex(const AdvisorTableProfile& profile,
                     const std::vector<ID>& index_columns,
                     const std::vector<ID>& include_columns) {
    for (const auto& column_id : profile.referenced_column_ids) {
        if (!containsId(index_columns, column_id) &&
            !containsId(include_columns, column_id)) {
            return false;
        }
    }
    return !profile.referenced_column_ids.empty();
}

bool recommendationIsCovering(const AdvisorTableProfile& profile,
                              const std::vector<ID>& column_ids) {
    for (const auto& column_id : profile.referenced_column_ids) {
        if (!containsId(column_ids, column_id)) {
            return false;
        }
    }
    return !profile.referenced_column_ids.empty();
}

auto buildSequentialPlan(CostModel& cost_model,
                         const AdvisorTableProfile& profile) -> AdvisorWhatIfPlan {
    AdvisorWhatIfPlan plan;
    plan.lowering = lowerSequentialPlannerFamily();
    plan.estimated_rows = profile.table_rows;
    const double qual_cost = std::max(1U, profile.predicate_count) * 0.0025;
    const auto seq_cost = cost_model.costSeqScan(profile.table_pages,
                                                 profile.table_rows,
                                                 qual_cost,
                                                 nullptr);
    double total_cost = seq_cost.total_cost;
    if (profile.needs_ordering) {
        const auto sort_cost = cost_model.costSort(profile.table_rows,
                                                   profile.row_width_bytes,
                                                   std::max<uint32_t>(1, profile.order_key_count),
                                                   nullptr);
        total_cost += sort_cost.total_cost;
    }
    plan.total_cost = total_cost;
    std::ostringstream detail;
    detail << "SEQ_SCAN baseline, rows=" << plan.estimated_rows
           << ", cost=" << total_cost;
    plan.detail = detail.str();
    return plan;
}

auto buildExistingIndexPlan(Database* db,
                            CatalogManager* catalog,
                            CostModel& cost_model,
                            const AdvisorTableProfile& profile,
                            const CatalogManager::IndexInfo& index)
    -> AdvisorWhatIfPlan {
    AdvisorWhatIfPlan plan;
    plan.index_name = index.index_name;

    std::vector<ID> matched_columns;
    PredicateMatchShape predicate_shape = PredicateMatchShape::NONE;
    std::string operator_name;
    bool ordered_output = false;
    for (const auto& column_id : index.column_ids) {
        const auto usage_it = profile.table_stats->columns.find(column_id);
        if (usage_it == profile.table_stats->columns.end()) {
            break;
        }
        const auto shape = strongestPredicateShape(usage_it->second);
        if (shape == PredicateMatchShape::NONE) {
            if (profile.needs_ordering && usage_it->second.order_by_count > 0) {
                ordered_output = true;
            }
            continue;
        }
        if (predicate_shape == PredicateMatchShape::NONE) {
            predicate_shape = shape;
            operator_name = operatorNameForShape(shape);
        }
        matched_columns.push_back(column_id);
        if (shape != PredicateMatchShape::EQUALITY) {
            break;
        }
    }

    if (matched_columns.empty() && !ordered_output) {
        return plan;
    }

    const auto request = buildPlannerFamilyLoweringRequest(catalog,
                                                           index,
                                                           predicate_shape,
                                                           operator_name,
                                                           ordered_output);
    plan.lowering = lowerPlannerFamily(request);
    if (plan.lowering.queryability_state == AccessPathQueryabilityState::INVALID) {
        return plan;
    }

    std::unordered_map<ID, ColumnStatistics, IDHash> column_stats;
    for (const auto& column_id : matched_columns) {
        ColumnStatistics stats;
        if (db->statistics_manager()->getColumnStatistics(index.table_id,
                                                          column_id,
                                                          stats,
                                                          nullptr) == Status::OK) {
            column_stats.emplace(column_id, std::move(stats));
        }
    }

    const double selectivity = matched_columns.empty()
        ? 1.0
        : estimateCompositeSelectivity(db->statistics_manager(),
                                       profile,
                                       matched_columns,
                                       column_stats);
    const uint64_t estimated_rows = std::max<uint64_t>(
        1,
        static_cast<uint64_t>(
            std::ceil(static_cast<double>(profile.table_rows) * selectivity)));

    IndexFamilyMetricsPacket metrics;
    const bool have_metrics = db->statistics_manager()->getIndexFamilyMetrics(index.index_id,
                                                                              metrics,
                                                                              nullptr) == Status::OK;
    const uint64_t index_height = have_metrics && metrics.height > 0 ? metrics.height : 3;
    const uint64_t leaf_pages = have_metrics && metrics.leaf_pages > 0
        ? metrics.leaf_pages
        : std::max<uint64_t>(1, static_cast<uint64_t>(
              std::ceil((estimated_rows * 32.0) / 8192.0)));
    const double entries_per_page = have_metrics && metrics.leaf_pages > 0 &&
            metrics.live_entry_count_est > 0
        ? static_cast<double>(metrics.live_entry_count_est) /
              static_cast<double>(metrics.leaf_pages)
        : 128.0;
    const uint64_t index_tuples = std::max<uint64_t>(
        estimated_rows,
        static_cast<uint64_t>(
            std::ceil(static_cast<double>(estimated_rows) *
                      (1.0 + (have_metrics ? metrics.recheck_ratio_est : 0.0)))));
    const uint64_t index_pages_touched = std::max<uint64_t>(
        1,
        std::min<uint64_t>(
            leaf_pages,
            static_cast<uint64_t>(
                std::ceil(static_cast<double>(index_tuples) /
                          std::max(1.0, entries_per_page)))));
    const double rows_per_page = static_cast<double>(profile.table_rows) /
        static_cast<double>(std::max<uint64_t>(1, profile.table_pages));
    const uint64_t heap_pages = estimatePagesForRows(
        estimated_rows,
        rows_per_page * std::max(0.1, 1.0 - (have_metrics ? metrics.correlation : 0.0) * 0.25));
    const bool covering = isCoveringIndex(profile,
                                          index.column_ids,
                                          index.include_column_ids);

    const double qual_cost = std::max(1U, profile.predicate_count) * 0.0025;
    CostEstimate cost;
    switch (plan.lowering.family) {
        case PlannerAccessFamily::BRIN_SCAN:
        case PlannerAccessFamily::SUMMARY_FILTER_SCAN:
            cost = cost_model.costSummaryScan(index_pages_touched,
                                              heap_pages,
                                              estimated_rows,
                                              qual_cost,
                                              0.0,
                                              0.0,
                                              nullptr);
            break;
        case PlannerAccessFamily::BITMAP_STORAGE_SCAN:
        case PlannerAccessFamily::BITMAP_COMBINE_SCAN:
            cost = cost_model.costBitmapStorageScan(index_pages_touched,
                                                    heap_pages,
                                                    estimated_rows,
                                                    qual_cost,
                                                    0.0,
                                                    have_metrics
                                                        ? metrics.recheck_ratio_est
                                                        : 0.0,
                                                    nullptr);
            break;
        case PlannerAccessFamily::LSM_EQ_SCAN:
        case PlannerAccessFamily::LSM_RANGE_SCAN:
        case PlannerAccessFamily::LSM_ORDERED_RANGE_SCAN:
            cost = cost_model.costLSMScan(3,
                                          2,
                                          index_tuples,
                                          heap_pages,
                                          estimated_rows,
                                          qual_cost,
                                          have_metrics ? metrics.correlation : 0.0,
                                          nullptr);
            break;
        default:
            if (covering || plan.lowering.supports_covering) {
                cost = cost_model.costIndexOnlyScan(index_height,
                                                    index_pages_touched,
                                                    index_tuples,
                                                    qual_cost,
                                                    have_metrics
                                                        ? metrics.correlation
                                                        : 0.0,
                                                    nullptr);
            } else {
                cost = cost_model.costIndexScan(index_height,
                                                index_pages_touched,
                                                index_tuples,
                                                heap_pages,
                                                estimated_rows,
                                                qual_cost,
                                                have_metrics ? metrics.correlation : 0.0,
                                                nullptr);
            }
            break;
    }

    double total_cost = cost.total_cost;
    if (profile.needs_ordering && !plan.lowering.supports_ordering) {
        const auto sort_cost = cost_model.costSort(estimated_rows,
                                                   profile.row_width_bytes,
                                                   std::max<uint32_t>(1, profile.order_key_count),
                                                   nullptr);
        total_cost += sort_cost.total_cost;
    }

    plan.total_cost = total_cost;
    plan.estimated_rows = estimated_rows;
    plan.covering = covering || plan.lowering.supports_covering;
    plan.ordered_output = plan.lowering.supports_ordering;
    std::ostringstream detail;
    detail << plannerAccessFamilyName(plan.lowering.family)
           << " using existing index " << index.index_name
           << ", rows=" << estimated_rows
           << ", cost=" << total_cost;
    plan.detail = detail.str();
    return plan;
}

auto buildHypotheticalRecommendationPlan(Database* db,
                                         CostModel& cost_model,
                                         const AdvisorTableProfile& profile,
                                         const IndexRecommendation& recommendation)
    -> AdvisorWhatIfPlan {
    AdvisorWhatIfPlan plan;
    plan.index_name = recommendation.index_name;

    std::vector<ID> column_ids;
    column_ids.reserve(recommendation.column_names.size());
    for (const auto& column_name : recommendation.column_names) {
        const auto lower_name = toLowerAscii(column_name);
        const auto column_it = profile.binding->columns_by_lower.find(lower_name);
        if (column_it != profile.binding->columns_by_lower.end()) {
            column_ids.push_back(column_it->second.column_id);
        }
    }
    if (column_ids.empty()) {
        return plan;
    }

    const auto usage_it = profile.table_stats->columns.find(column_ids.front());
    const auto predicate_shape = usage_it != profile.table_stats->columns.end()
        ? strongestPredicateShape(usage_it->second)
        : PredicateMatchShape::NONE;

    PlannerFamilyLoweringRequest request;
    request.index_type = recommendationIndexType(recommendation.type);
    request.ordered_output = profile.needs_ordering &&
        recommendation.type != IndexRecommendationType::CREATE_HASH;
    request.predicate_shape = predicate_shape;
    plan.lowering = lowerPlannerFamily(request);
    if (plan.lowering.queryability_state == AccessPathQueryabilityState::INVALID) {
        return plan;
    }

    std::unordered_map<ID, ColumnStatistics, IDHash> column_stats;
    for (const auto& column_id : column_ids) {
        ColumnStatistics stats;
        if (db->statistics_manager()->getColumnStatistics(profile.binding->table_info.table_id,
                                                          column_id,
                                                          stats,
                                                          nullptr) == Status::OK) {
            column_stats.emplace(column_id, std::move(stats));
        }
    }

    const double selectivity = estimateCompositeSelectivity(db->statistics_manager(),
                                                            profile,
                                                            column_ids,
                                                            column_stats);
    const uint64_t estimated_rows = std::max<uint64_t>(
        1,
        static_cast<uint64_t>(
            std::ceil(static_cast<double>(profile.table_rows) * selectivity)));
    const bool covering = recommendationIsCovering(profile, column_ids);
    const uint64_t estimated_index_pages = std::max<uint64_t>(
        1,
        static_cast<uint64_t>(
            std::ceil((recommendation.estimated_size_mb * 1024.0 * 1024.0) /
                      8192.0)));
    const uint64_t index_height = estimated_index_pages > 4096 ? 4 : 3;
    const uint64_t index_tuples = estimated_rows;
    const double rows_per_page = static_cast<double>(profile.table_rows) /
        static_cast<double>(std::max<uint64_t>(1, profile.table_pages));
    const uint64_t heap_pages = estimatePagesForRows(estimated_rows, rows_per_page);
    const double qual_cost = std::max(1U, profile.predicate_count) * 0.0025;

    CostEstimate cost;
    switch (plan.lowering.family) {
        case PlannerAccessFamily::HASH_EQ_SCAN:
            cost = cost_model.costIndexScan(2,
                                            1,
                                            index_tuples,
                                            heap_pages,
                                            estimated_rows,
                                            qual_cost,
                                            0.0,
                                            nullptr);
            break;
        case PlannerAccessFamily::LSM_EQ_SCAN:
        case PlannerAccessFamily::LSM_RANGE_SCAN:
        case PlannerAccessFamily::LSM_ORDERED_RANGE_SCAN:
            cost = cost_model.costLSMScan(3,
                                          2,
                                          index_tuples,
                                          heap_pages,
                                          estimated_rows,
                                          qual_cost,
                                          0.0,
                                          nullptr);
            break;
        default:
            if (covering || plan.lowering.supports_covering) {
                cost = cost_model.costIndexOnlyScan(index_height,
                                                    estimated_index_pages,
                                                    index_tuples,
                                                    qual_cost,
                                                    0.2,
                                                    nullptr);
            } else {
                cost = cost_model.costIndexScan(index_height,
                                                estimated_index_pages,
                                                index_tuples,
                                                heap_pages,
                                                estimated_rows,
                                                qual_cost,
                                                0.2,
                                                nullptr);
            }
            break;
    }

    double total_cost = cost.total_cost;
    if (profile.needs_ordering && !plan.lowering.supports_ordering) {
        const auto sort_cost = cost_model.costSort(estimated_rows,
                                                   profile.row_width_bytes,
                                                   std::max<uint32_t>(1, profile.order_key_count),
                                                   nullptr);
        total_cost += sort_cost.total_cost;
    }

    plan.total_cost = total_cost;
    plan.estimated_rows = estimated_rows;
    plan.covering = covering || plan.lowering.supports_covering;
    plan.ordered_output = plan.lowering.supports_ordering;
    std::ostringstream detail;
    detail << plannerAccessFamilyName(plan.lowering.family)
           << " using hypothetical index " << recommendation.index_name
           << ", rows=" << estimated_rows
           << ", cost=" << total_cost;
    plan.detail = detail.str();
    return plan;
}

auto chooseBetterPlan(const AdvisorWhatIfPlan& left, const AdvisorWhatIfPlan& right)
    -> const AdvisorWhatIfPlan& {
    if (left.total_cost < right.total_cost) {
        return left;
    }
    if (right.total_cost < left.total_cost) {
        return right;
    }
    if (left.covering != right.covering) {
        return left.covering ? left : right;
    }
    if (left.ordered_output != right.ordered_output) {
        return left.ordered_output ? left : right;
    }
    return left;
}

void recordSignal(std::unordered_map<std::string, SignalEntry>& signals,
                  const std::string& qualifier_lower,
                  const std::string& column_lower,
                  SignalKind kind) {
    if (column_lower.empty()) {
        return;
    }

    const std::string key = signalKey(qualifier_lower, column_lower);
    auto& entry = signals[key];
    if (entry.column_lower.empty()) {
        entry.column_lower = column_lower;
        entry.qualifier_lower = qualifier_lower;
    }

    switch (kind) {
        case SignalKind::EQUALITY:
            ++entry.counts.equality;
            break;
        case SignalKind::RANGE:
            ++entry.counts.range;
            break;
        case SignalKind::LIKE:
            ++entry.counts.like;
            break;
        case SignalKind::IN_LIST:
            ++entry.counts.in_list;
            break;
        case SignalKind::JOIN:
            ++entry.counts.join;
            break;
        case SignalKind::ORDER_BY:
            ++entry.counts.order_by;
            break;
        case SignalKind::GROUP_BY:
            ++entry.counts.group_by;
            break;
        case SignalKind::SELECT_LIST:
            ++entry.counts.select_list;
            break;
    }
}

std::optional<ColumnToken> getColumnToken(const parser::v3::ColumnRefExpr* column_ref,
                                          const parser::v3::StringPool& pool) {
    if (!column_ref) {
        return std::nullopt;
    }
    auto column_id = column_ref->column.column_name;
    if (column_id == parser::v3::StringPool::INVALID_ID) {
        return std::nullopt;
    }

    ColumnToken token;
    token.column_lower = toLowerAscii(pool.get(column_id));
    if (column_ref->column.has_table_qualifier &&
        !column_ref->column.table_path.components.empty()) {
        token.qualifier_lower = toLowerAscii(
            pool.get(column_ref->column.table_path.components.back()));
    }
    return token;
}

std::optional<ColumnToken> extractDirectColumnToken(parser::v3::Expression* expr,
                                                    const parser::v3::StringPool& pool) {
    while (expr && expr->kind() == parser::v3::ASTKind::CastExpr) {
        auto* cast_expr = static_cast<parser::v3::CastExpr*>(expr);
        expr = cast_expr->expr;
    }

    if (!expr || expr->kind() != parser::v3::ASTKind::ColumnRefExpr) {
        return std::nullopt;
    }
    return getColumnToken(static_cast<parser::v3::ColumnRefExpr*>(expr), pool);
}

template <typename Fn>
void collectColumnRefs(parser::v3::Expression* expr, Fn&& fn) {
    if (!expr) {
        return;
    }

    using parser::v3::ASTKind;
    switch (expr->kind()) {
        case ASTKind::ColumnRefExpr: {
            fn(static_cast<parser::v3::ColumnRefExpr*>(expr));
            break;
        }
        case ASTKind::BinaryExpr: {
            auto* binary = static_cast<parser::v3::BinaryExpr*>(expr);
            collectColumnRefs(binary->left, fn);
            collectColumnRefs(binary->right, fn);
            break;
        }
        case ASTKind::UnaryExpr: {
            auto* unary = static_cast<parser::v3::UnaryExpr*>(expr);
            collectColumnRefs(unary->operand, fn);
            break;
        }
        case ASTKind::FunctionCallExpr: {
            auto* call = static_cast<parser::v3::FunctionCallExpr*>(expr);
            for (auto* arg : call->arguments) {
                collectColumnRefs(arg, fn);
            }
            if (call->filter) {
                collectColumnRefs(call->filter, fn);
            }
            for (auto* order_item : call->order_by) {
                if (order_item) {
                    collectColumnRefs(order_item->expr, fn);
                }
            }
            break;
        }
        case ASTKind::CastExpr: {
            auto* cast_expr = static_cast<parser::v3::CastExpr*>(expr);
            collectColumnRefs(cast_expr->expr, fn);
            break;
        }
        case ASTKind::CaseExpr: {
            auto* case_expr = static_cast<parser::v3::CaseExpr*>(expr);
            collectColumnRefs(case_expr->operand, fn);
            for (const auto& item : case_expr->when_clauses) {
                collectColumnRefs(item.when_expr, fn);
                collectColumnRefs(item.then_expr, fn);
            }
            collectColumnRefs(case_expr->else_expr, fn);
            break;
        }
        case ASTKind::InExpr: {
            auto* in_expr = static_cast<parser::v3::InExpr*>(expr);
            collectColumnRefs(in_expr->expr, fn);
            for (auto* value : in_expr->values) {
                collectColumnRefs(value, fn);
            }
            break;
        }
        case ASTKind::BetweenExpr: {
            auto* between_expr = static_cast<parser::v3::BetweenExpr*>(expr);
            collectColumnRefs(between_expr->expr, fn);
            collectColumnRefs(between_expr->low, fn);
            collectColumnRefs(between_expr->high, fn);
            break;
        }
        case ASTKind::LikeExpr: {
            auto* like_expr = static_cast<parser::v3::LikeExpr*>(expr);
            collectColumnRefs(like_expr->expr, fn);
            collectColumnRefs(like_expr->pattern, fn);
            collectColumnRefs(like_expr->escape, fn);
            break;
        }
        case ASTKind::IsNullExpr: {
            auto* null_expr = static_cast<parser::v3::IsNullExpr*>(expr);
            collectColumnRefs(null_expr->expr, fn);
            break;
        }
        case ASTKind::ArrayExpr: {
            auto* array_expr = static_cast<parser::v3::ArrayExpr*>(expr);
            for (auto* element : array_expr->elements) {
                collectColumnRefs(element, fn);
            }
            break;
        }
        case ASTKind::ExtractExpr: {
            auto* extract_expr = static_cast<parser::v3::ExtractExpr*>(expr);
            collectColumnRefs(extract_expr->source, fn);
            for (auto* arg : extract_expr->selector.args) {
                collectColumnRefs(arg, fn);
            }
            collectColumnRefs(extract_expr->selector.expr, fn);
            break;
        }
        case ASTKind::AlterElementExpr: {
            auto* alter_expr = static_cast<parser::v3::AlterElementExpr*>(expr);
            collectColumnRefs(alter_expr->source, fn);
            collectColumnRefs(alter_expr->new_value, fn);
            for (auto* arg : alter_expr->selector.args) {
                collectColumnRefs(arg, fn);
            }
            collectColumnRefs(alter_expr->selector.expr, fn);
            break;
        }
        default:
            break;
    }
}

void recordExpressionColumns(parser::v3::Expression* expr,
                             const parser::v3::StringPool& pool,
                             std::unordered_map<std::string, SignalEntry>& signals,
                             SignalKind kind) {
    collectColumnRefs(expr, [&](parser::v3::ColumnRefExpr* ref) {
        auto token = getColumnToken(ref, pool);
        if (token) {
            recordSignal(signals, token->qualifier_lower, token->column_lower, kind);
        }
    });
}

void analyzePredicateExpression(parser::v3::Expression* expr,
                                const parser::v3::StringPool& pool,
                                std::unordered_map<std::string, SignalEntry>& signals,
                                bool join_context = false) {
    if (!expr) {
        return;
    }

    using parser::v3::ASTKind;
    switch (expr->kind()) {
        case ASTKind::BinaryExpr: {
            auto* binary = static_cast<parser::v3::BinaryExpr*>(expr);
            using parser::v3::BinaryOp;

            if (binary->op == BinaryOp::AND || binary->op == BinaryOp::OR) {
                analyzePredicateExpression(binary->left, pool, signals, join_context);
                analyzePredicateExpression(binary->right, pool, signals, join_context);
                return;
            }

            const bool is_eq = (binary->op == BinaryOp::EQ);
            const bool is_range = (binary->op == BinaryOp::LT ||
                                   binary->op == BinaryOp::LE ||
                                   binary->op == BinaryOp::GT ||
                                   binary->op == BinaryOp::GE);
            const bool is_ne = (binary->op == BinaryOp::NE);
            const bool is_comparison = (is_eq || is_range || is_ne);
            if (!is_comparison) {
                analyzePredicateExpression(binary->left, pool, signals, join_context);
                analyzePredicateExpression(binary->right, pool, signals, join_context);
                return;
            }

            auto left_col = extractDirectColumnToken(binary->left, pool);
            auto right_col = extractDirectColumnToken(binary->right, pool);
            const bool join_like = join_context || (left_col.has_value() && right_col.has_value());

            auto record_for_token = [&](const ColumnToken& token) {
                if (is_eq) {
                    recordSignal(signals, token.qualifier_lower, token.column_lower,
                                 SignalKind::EQUALITY);
                } else if (is_range) {
                    recordSignal(signals, token.qualifier_lower, token.column_lower,
                                 SignalKind::RANGE);
                }
                if (join_like) {
                    recordSignal(signals, token.qualifier_lower, token.column_lower,
                                 SignalKind::JOIN);
                }
            };

            if (left_col) {
                record_for_token(*left_col);
            } else if (!is_ne) {
                const SignalKind fallback_kind = is_range ? SignalKind::RANGE : SignalKind::EQUALITY;
                recordExpressionColumns(binary->left, pool, signals, fallback_kind);
                if (join_like) {
                    recordExpressionColumns(binary->left, pool, signals, SignalKind::JOIN);
                }
            }

            if (right_col) {
                record_for_token(*right_col);
            } else if (!is_ne) {
                const SignalKind fallback_kind = is_range ? SignalKind::RANGE : SignalKind::EQUALITY;
                recordExpressionColumns(binary->right, pool, signals, fallback_kind);
                if (join_like) {
                    recordExpressionColumns(binary->right, pool, signals, SignalKind::JOIN);
                }
            }
            return;
        }
        case ASTKind::LikeExpr: {
            auto* like_expr = static_cast<parser::v3::LikeExpr*>(expr);
            recordExpressionColumns(like_expr->expr, pool, signals, SignalKind::LIKE);
            return;
        }
        case ASTKind::InExpr: {
            auto* in_expr = static_cast<parser::v3::InExpr*>(expr);
            recordExpressionColumns(in_expr->expr, pool, signals, SignalKind::IN_LIST);
            return;
        }
        case ASTKind::BetweenExpr: {
            auto* between_expr = static_cast<parser::v3::BetweenExpr*>(expr);
            recordExpressionColumns(between_expr->expr, pool, signals, SignalKind::RANGE);
            return;
        }
        default:
            break;
    }
}

void collectTableRefs(parser::v3::TableRefNode* table_ref, std::vector<TableInput>& out,
                      const parser::v3::StringPool& pool) {
    if (!table_ref) {
        return;
    }

    if (table_ref->ref_type != parser::v3::TableRefNode::Type::TABLE) {
        return;
    }

    TableInput input;
    input.table_path = table_ref->table_path;
    if (table_ref->has_alias && table_ref->alias != parser::v3::StringPool::INVALID_ID) {
        input.alias = std::string(pool.get(table_ref->alias));
    }
    out.push_back(std::move(input));
}

void analyzeSelectStatement(parser::v3::SelectStmt* stmt,
                            const parser::v3::StringPool& pool,
                            std::vector<TableInput>& tables,
                            std::unordered_map<std::string, SignalEntry>& signals);

void analyzeStatement(parser::v3::Statement* statement,
                      const parser::v3::StringPool& pool,
                      std::vector<TableInput>& tables,
                      std::unordered_map<std::string, SignalEntry>& signals) {
    if (!statement) {
        return;
    }

    using parser::v3::ASTKind;
    switch (statement->kind()) {
        case ASTKind::SelectStmt: {
            analyzeSelectStatement(static_cast<parser::v3::SelectStmt*>(statement),
                                   pool, tables, signals);
            break;
        }
        case ASTKind::UpdateStmt: {
            auto* update_stmt = static_cast<parser::v3::UpdateStmt*>(statement);
            tables.push_back({update_stmt->table_path,
                              update_stmt->has_alias && update_stmt->alias != parser::v3::StringPool::INVALID_ID
                                  ? std::string(pool.get(update_stmt->alias))
                                  : std::string()});
            collectTableRefs(update_stmt->from, tables, pool);
            for (auto* join : update_stmt->joins) {
                if (!join) {
                    continue;
                }
                collectTableRefs(join->right, tables, pool);
                analyzePredicateExpression(join->on_condition, pool, signals, true);
            }
            analyzePredicateExpression(update_stmt->where, pool, signals, false);
            break;
        }
        case ASTKind::DeleteStmt: {
            auto* delete_stmt = static_cast<parser::v3::DeleteStmt*>(statement);
            tables.push_back({delete_stmt->table_path,
                              delete_stmt->has_alias && delete_stmt->alias != parser::v3::StringPool::INVALID_ID
                                  ? std::string(pool.get(delete_stmt->alias))
                                  : std::string()});
            collectTableRefs(delete_stmt->using_clause, tables, pool);
            for (auto* join : delete_stmt->using_joins) {
                if (!join) {
                    continue;
                }
                collectTableRefs(join->right, tables, pool);
                analyzePredicateExpression(join->on_condition, pool, signals, true);
            }
            analyzePredicateExpression(delete_stmt->where, pool, signals, false);
            break;
        }
        case ASTKind::ExplainStmt: {
            auto* explain_stmt = static_cast<parser::v3::ExplainStmt*>(statement);
            analyzeStatement(explain_stmt->query, pool, tables, signals);
            break;
        }
        default:
            break;
    }
}

void analyzeSelectStatement(parser::v3::SelectStmt* stmt,
                            const parser::v3::StringPool& pool,
                            std::vector<TableInput>& tables,
                            std::unordered_map<std::string, SignalEntry>& signals) {
    if (!stmt) {
        return;
    }

    collectTableRefs(stmt->from, tables, pool);
    analyzePredicateExpression(stmt->where, pool, signals, false);

    for (auto* join : stmt->joins) {
        if (!join) {
            continue;
        }
        collectTableRefs(join->right, tables, pool);
        analyzePredicateExpression(join->on_condition, pool, signals, true);
        if (join->has_using) {
            for (auto column_id : join->using_columns) {
                if (column_id == parser::v3::StringPool::INVALID_ID) {
                    continue;
                }
                recordSignal(signals, "", toLowerAscii(pool.get(column_id)), SignalKind::JOIN);
                recordSignal(signals, "", toLowerAscii(pool.get(column_id)), SignalKind::EQUALITY);
            }
        }
    }

    for (auto* group_expr : stmt->group_by) {
        recordExpressionColumns(group_expr, pool, signals, SignalKind::GROUP_BY);
    }

    for (auto* order_item : stmt->order_by) {
        if (order_item) {
            recordExpressionColumns(order_item->expr, pool, signals, SignalKind::ORDER_BY);
        }
    }

    for (auto* select_item : stmt->items) {
        if (!select_item) {
            continue;
        }
        if (select_item->item_type == parser::v3::SelectItem::Type::EXPRESSION) {
            recordExpressionColumns(select_item->expr, pool, signals, SignalKind::SELECT_LIST);
        }
    }

    if (stmt->set_op_right) {
        analyzeSelectStatement(stmt->set_op_right, pool, tables, signals);
    }
}

std::string schemaNameFromPath(const parser::v3::SchemaPath& path,
                               const parser::v3::StringPool& pool) {
    if (path.components.size() <= 1) {
        return {};
    }

    std::ostringstream out;
    for (size_t i = 0; i + 1 < path.components.size(); ++i) {
        if (i > 0) {
            out << '.';
        }
        out << pool.get(path.components[i]);
    }
    return out.str();
}

bool addSchemaCandidate(CatalogManager* catalog, const std::string& name, std::vector<ID>& out) {
    if (!catalog || name.empty()) {
        return false;
    }

    CatalogManager::SchemaInfo schema_info;
    ErrorContext ctx;
    if (catalog->getSchema(name, schema_info, &ctx) != Status::OK) {
        return false;
    }

    auto exists = std::find(out.begin(), out.end(), schema_info.schema_id) != out.end();
    if (!exists) {
        out.push_back(schema_info.schema_id);
    }
    return true;
}

bool resolveTableBinding(CatalogManager* catalog,
                         const TableInput& input,
                         const parser::v3::StringPool& pool,
                         TableBinding& binding) {
    if (!catalog || input.table_path.components.empty()) {
        return false;
    }

    const std::string table_name =
        std::string(pool.get(input.table_path.components.back()));
    if (table_name.empty()) {
        return false;
    }

    std::vector<ID> schema_candidates;
    addSchemaCandidate(catalog, schemaNameFromPath(input.table_path, pool), schema_candidates);
    addSchemaCandidate(catalog, "users.public", schema_candidates);
    addSchemaCandidate(catalog, "public", schema_candidates);

    CatalogManager::TableInfo table_info;
    ErrorContext ctx;
    bool found = false;
    for (const auto& schema_id : schema_candidates) {
        if (catalog->getTable(schema_id, table_name, table_info, &ctx) == Status::OK) {
            found = true;
            break;
        }
    }

    if (!found) {
        std::vector<CatalogManager::SchemaInfo> schemas;
        if (catalog->listSchemas(schemas, &ctx) == Status::OK) {
            CatalogManager::TableInfo matched{};
            size_t match_count = 0;
            for (const auto& schema : schemas) {
                std::vector<CatalogManager::TableInfo> tables;
                if (catalog->listTables(schema.schema_id, tables, &ctx) != Status::OK) {
                    continue;
                }
                for (const auto& table : tables) {
                    if (toLowerAscii(table.table_name) == toLowerAscii(table_name)) {
                        matched = table;
                        ++match_count;
                    }
                }
            }
            if (match_count == 1) {
                table_info = matched;
                found = true;
            }
        }
    }

    if (!found) {
        return false;
    }

    std::vector<CatalogManager::ColumnInfo> columns;
    if (catalog->getColumns(table_info.table_id, columns, &ctx) != Status::OK) {
        return false;
    }

    binding.table_info = table_info;
    binding.table_name_lower = toLowerAscii(table_info.table_name);
    binding.alias_lower = toLowerAscii(input.alias);
    binding.columns_by_lower.clear();
    for (const auto& column : columns) {
        binding.columns_by_lower.emplace(toLowerAscii(column.column_name), column);
    }
    return true;
}

}  // namespace

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
            status = catalog_->listIndexesForTable(table.table_id, indexes, &ctx, false);
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
    if (!recommendations) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null recommendations output");
        return Status::INVALID_ARGUMENT;
    }

    recommendations->clear();
    if (!catalog_ || sql_text.empty()) {
        return Status::OK;
    }

    parser::v3::Parser parser(sql_text);
    auto parse_result = parser.parseStatement();
    if (!parse_result.statement()) {
        return Status::OK;
    }

    std::vector<TableInput> table_inputs;
    std::unordered_map<std::string, SignalEntry> signals;
    analyzeStatement(parse_result.statement(), parser.stringPool(), table_inputs, signals);

    if (table_inputs.empty() || signals.empty()) {
        return Status::OK;
    }

    std::vector<TableBinding> bindings;
    bindings.reserve(table_inputs.size());
    for (const auto& input : table_inputs) {
        TableBinding binding;
        if (resolveTableBinding(catalog_, input, parser.stringPool(), binding)) {
            bindings.push_back(std::move(binding));
        }
    }

    if (bindings.empty()) {
        return Status::OK;
    }

    std::unordered_map<ID, TableUsageStats, IDHash> usage_by_table;
    for (const auto& [_, entry] : signals) {
        std::vector<const TableBinding*> matches;
        if (!entry.qualifier_lower.empty()) {
            for (const auto& binding : bindings) {
                if (binding.alias_lower == entry.qualifier_lower ||
                    binding.table_name_lower == entry.qualifier_lower) {
                    matches.push_back(&binding);
                }
            }
        } else {
            for (const auto& binding : bindings) {
                if (binding.columns_by_lower.find(entry.column_lower) !=
                    binding.columns_by_lower.end()) {
                    matches.push_back(&binding);
                }
            }
        }

        // Skip ambiguous or unresolved references.
        if (matches.size() != 1) {
            continue;
        }

        const auto* binding = matches.front();
        auto col_it = binding->columns_by_lower.find(entry.column_lower);
        if (col_it == binding->columns_by_lower.end()) {
            continue;
        }

        auto& table_stats = usage_by_table[binding->table_info.table_id];
        table_stats.table_id = binding->table_info.table_id;
        table_stats.table_name = binding->table_info.table_name;
        table_stats.total_queries += std::max<uint32_t>(1, entry.counts.total());
        table_stats.seq_scans += std::max<uint32_t>(1, entry.counts.total());

        auto& col_usage = table_stats.columns[col_it->second.column_id];
        col_usage.column_id = col_it->second.column_id;
        col_usage.column_name = col_it->second.column_name;
        col_usage.equality_count += entry.counts.equality;
        col_usage.range_count += entry.counts.range;
        col_usage.like_count += entry.counts.like;
        col_usage.in_list_count += entry.counts.in_list;
        col_usage.join_count += entry.counts.join;
        col_usage.order_by_count += entry.counts.order_by;
        col_usage.group_by_count += entry.counts.group_by;
        col_usage.select_count += entry.counts.select_list;
        col_usage.total_queries += std::max<uint32_t>(1, entry.counts.total());
    }

    if (usage_by_table.empty()) {
        return Status::OK;
    }

    std::unordered_map<ID, const TableBinding*, IDHash> binding_by_table;
    for (const auto& binding : bindings) {
        binding_by_table.emplace(binding.table_info.table_id, &binding);
    }

    for (auto& [table_id, table_stats] : usage_by_table) {
        std::vector<const ColumnUsage*> equality_candidates;
        for (auto& [column_id, col_usage] : table_stats.columns) {
            std::vector<ID> one_col = {column_id};
            if (isIndexExisting(table_id, one_col)) {
                continue;
            }

            if (col_usage.equality_count == 0 && col_usage.range_count == 0 &&
                col_usage.like_count == 0 && col_usage.in_list_count == 0 &&
                col_usage.join_count == 0) {
                continue;
            }

            auto rec = generateCreateRecommendation(table_stats, col_usage);
            if (col_usage.like_count > 0 && col_usage.equality_count == 0) {
                rec.type = IndexRecommendationType::CREATE_BTREE;
                rec.index_name = generateIndexName(table_stats.table_name,
                                                   rec.column_names, rec.type);
                rec.create_sql = generateCreateSQL(rec);
            }

            // Query-local advice should not be filtered by historical confidence thresholds.
            rec.confidence = std::max(rec.confidence, 0.75);
            rec.priority = std::max(rec.priority, rec.net_benefit + 10.0);
            recommendations->push_back(std::move(rec));

            if (col_usage.equality_count > 0 || col_usage.join_count > 0) {
                equality_candidates.push_back(&col_usage);
            }
        }

        if (equality_candidates.size() >= 2) {
            std::sort(equality_candidates.begin(), equality_candidates.end(),
                      [](const ColumnUsage* a, const ColumnUsage* b) {
                          return a->getScore() > b->getScore();
                      });
            std::vector<const ColumnUsage*> pair = {
                equality_candidates[0],
                equality_candidates[1],
            };
            std::vector<ID> pair_ids = {pair[0]->column_id, pair[1]->column_id};
            if (!isIndexExisting(table_id, pair_ids)) {
                auto composite = generateCompositeRecommendation(table_stats, pair);
                composite.confidence = std::max(composite.confidence, 0.75);
                recommendations->push_back(std::move(composite));
            }
        }
    }

    std::unordered_map<ID, AdvisorTableProfile, IDHash> profile_by_table;
    for (const auto& [table_id, table_stats] : usage_by_table) {
        const auto binding_it = binding_by_table.find(table_id);
        if (binding_it == binding_by_table.end()) {
            continue;
        }

        AdvisorTableProfile profile;
        if (loadAdvisorTableProfile(db_,
                                    catalog_,
                                    *binding_it->second,
                                    table_stats,
                                    profile)) {
            profile_by_table.emplace(table_id, std::move(profile));
        }
    }

    std::vector<IndexRecommendation> replanned;
    replanned.reserve(recommendations->size());
    for (auto& rec : *recommendations) {
        const auto profile_it = profile_by_table.find(rec.table_id);
        if (profile_it == profile_by_table.end()) {
            continue;
        }

        auto baseline = buildSequentialPlan(*cost_model_, profile_it->second);
        for (const auto& existing_index : profile_it->second.existing_indexes) {
            const auto candidate = buildExistingIndexPlan(db_,
                                                          catalog_,
                                                          *cost_model_,
                                                          profile_it->second,
                                                          existing_index);
            baseline = chooseBetterPlan(baseline, candidate);
        }

        const auto hypothetical = buildHypotheticalRecommendationPlan(db_,
                                                                      *cost_model_,
                                                                      profile_it->second,
                                                                      rec);
        if (!std::isfinite(hypothetical.total_cost) ||
            hypothetical.total_cost >= baseline.total_cost * 0.99) {
            continue;
        }

        rec.what_if.replanned = true;
        rec.what_if.baseline_access_family =
            plannerAccessFamilyName(baseline.lowering.family);
        rec.what_if.baseline_index_name = baseline.index_name;
        rec.what_if.baseline_total_cost = baseline.total_cost;
        rec.what_if.baseline_estimated_rows = baseline.estimated_rows;
        rec.what_if.hypothetical_access_family =
            plannerAccessFamilyName(hypothetical.lowering.family);
        rec.what_if.hypothetical_index_name = hypothetical.index_name;
        rec.what_if.hypothetical_total_cost = hypothetical.total_cost;
        rec.what_if.hypothetical_estimated_rows = hypothetical.estimated_rows;
        rec.what_if.estimated_cost_delta =
            baseline.total_cost - hypothetical.total_cost;
        rec.what_if.estimated_speedup_ratio = hypothetical.total_cost > 0.0
            ? baseline.total_cost / hypothetical.total_cost
            : baseline.total_cost;
        rec.what_if.ordering_improved =
            !baseline.ordered_output && hypothetical.ordered_output;
        rec.what_if.covering_improved =
            !baseline.covering && hypothetical.covering;
        std::ostringstream evidence;
        evidence << baseline.detail << " -> " << hypothetical.detail;
        rec.what_if.evidence_detail = evidence.str();
        rec.estimated_speedup = std::max(rec.estimated_speedup,
                                         rec.what_if.estimated_speedup_ratio);
        rec.priority = std::max(rec.priority,
                                rec.net_benefit +
                                    rec.what_if.estimated_cost_delta +
                                    (rec.what_if.ordering_improved ? 5.0 : 0.0) +
                                    (rec.what_if.covering_improved ? 3.0 : 0.0));
        rec.reason += " What-if delta: " + rec.what_if.evidence_detail;
        replanned.push_back(std::move(rec));
    }
    recommendations->swap(replanned);

    std::sort(recommendations->begin(), recommendations->end(),
              [](const IndexRecommendation& a, const IndexRecommendation& b) {
                  if (a.what_if.estimated_speedup_ratio !=
                      b.what_if.estimated_speedup_ratio) {
                      return a.what_if.estimated_speedup_ratio >
                             b.what_if.estimated_speedup_ratio;
                  }
                  if (a.what_if.estimated_cost_delta !=
                      b.what_if.estimated_cost_delta) {
                      return a.what_if.estimated_cost_delta >
                             b.what_if.estimated_cost_delta;
                  }
                  return a.priority > b.priority;
              });

    // Deduplicate by table + index name to avoid duplicates from repeated predicates.
    std::unordered_set<std::string> seen;
    std::vector<IndexRecommendation> deduped;
    deduped.reserve(recommendations->size());
    for (auto& rec : *recommendations) {
        const std::string key = rec.table_name + "|" + rec.index_name;
        if (seen.insert(key).second) {
            deduped.push_back(std::move(rec));
        }
    }
    recommendations->swap(deduped);

    if (recommendations->size() > config_.max_recommendations) {
        recommendations->resize(config_.max_recommendations);
    }

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
        case 14: return 16.0;  // UINT128

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
    auto status = catalog_->listIndexesForTable(table_id, indexes, &ctx, false);
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
