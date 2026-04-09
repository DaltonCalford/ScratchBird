/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/optimizer/index_family_lowering.h"
#include "scratchbird/optimizer/plan_payload.h"
#include "scratchbird/optimizer/query_profiler.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_opcode_registry.h"
#include "test_helpers.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
namespace sblr_v3 = scratchbird::sblr::v3;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    class ScopedEnvVar
    {
    public:
        ScopedEnvVar(const char* key, const std::string& value)
            : key_(key), had_original_(false)
        {
            if (const char* current = std::getenv(key_))
            {
                had_original_ = true;
                original_value_ = current;
            }
            set(value);
        }

        ~ScopedEnvVar()
        {
            if (had_original_)
            {
                set(original_value_);
            }
            else
            {
                unset();
            }
        }

    private:
        void set(const std::string& value)
        {
#ifdef _WIN32
            _putenv_s(key_, value.c_str());
#else
            setenv(key_, value.c_str(), 1);
#endif
        }

        void unset()
        {
#ifdef _WIN32
            _putenv_s(key_, "");
#else
            unsetenv(key_);
#endif
        }

        const char* key_;
        bool had_original_;
        std::string original_value_;
    };

    auto isZeroId(const ID& id) -> bool
    {
        return std::all_of(id.bytes.begin(), id.bytes.end(),
                           [](uint8_t byte) { return byte == 0; });
    }

    auto normalizedMisestimateRatio(uint64_t estimated_rows,
                                    uint64_t actual_rows) -> double
    {
        const double estimated =
            static_cast<double>(std::max<uint64_t>(estimated_rows, 1));
        const double actual =
            static_cast<double>(std::max<uint64_t>(actual_rows, 1));
        return std::max(estimated, actual) / std::min(estimated, actual);
    }

    auto plannerNormalizedStatementIdForSql(const std::string& sql)
        -> std::string
    {
        return std::to_string(
            sblr_v3::stableHash64(
                optimizer::QueryProfiler::getInstance().fingerprintQuery(sql)));
    }

    auto memoryGrantFeedbackKeyHashForTest(const ID& database_uuid,
                                           const ID& schema_root_uuid,
                                           const std::string& normalized_statement_id,
                                           const std::string& planner_policy_snapshot_id,
                                           const std::string& cache_mode,
                                           const std::string& execution_intent_class,
                                           const std::string& storage_layer_shape,
                                           const std::string& operator_kind)
        -> uint64_t
    {
        std::ostringstream seed;
        seed << database_uuid.toString() << '|'
             << schema_root_uuid.toString() << '|'
             << normalized_statement_id << '|'
             << planner_policy_snapshot_id << '|'
             << cache_mode << '|'
             << execution_intent_class << '|'
             << storage_layer_shape << '|'
             << operator_kind;
        return sblr_v3::stableHash64(seed.str());
    }

    auto runtimeNodeContainsType(const optimizer::RuntimePlanNode& node,
                                 const std::string& node_type) -> bool
    {
        if (node.node_type == node_type)
        {
            return true;
        }
        for (const auto& child : node.children)
        {
            if (runtimeNodeContainsType(child, node_type))
            {
                return true;
            }
        }
        return false;
    }

    auto joinStrings(const std::vector<std::string>& values,
                     const std::string& separator) -> std::string
    {
        std::ostringstream out;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0)
            {
                out << separator;
            }
            out << values[i];
        }
        return out.str();
    }

    auto csvEscape(const std::string& value) -> std::string
    {
        std::string escaped = "\"";
        escaped.reserve(value.size() + 2);
        for (const char ch : value)
        {
            if (ch == '"')
            {
                escaped += "\"\"";
            }
            else
            {
                escaped.push_back(ch);
            }
        }
        escaped.push_back('"');
        return escaped;
    }

    auto csvRow(const std::vector<std::string>& columns) -> std::string
    {
        std::ostringstream out;
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (i > 0)
            {
                out << ',';
            }
            out << csvEscape(columns[i]);
        }
        return out.str();
    }

    auto formatTraceEntries(
        const std::vector<scratchbird::optimizer::RuntimePlanTraceEntry>& entries)
        -> std::string
    {
        std::ostringstream out;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (i > 0)
            {
                out << " | ";
            }
            out << entries[i].phase << ':' << entries[i].subject << ':'
                << entries[i].candidate << ':' << entries[i].verdict << ':'
                << entries[i].reason;
        }
        return out.str();
    }

    auto writeDelimitedLines(const std::filesystem::path& path,
                             const std::vector<std::string>& lines) -> bool
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream out(path);
        if (!out.is_open())
        {
            return false;
        }
        for (const auto& line : lines)
        {
            out << line << '\n';
        }
        return true;
    }

    auto readTextFile(const std::filesystem::path& path) -> std::string
    {
        std::ifstream in(path);
        if (!in.is_open())
        {
            return {};
        }
        std::ostringstream out;
        out << in.rdbuf();
        return out.str();
    }

    auto normalizedExplainSnapshot(const nlohmann::json& explain_json)
        -> nlohmann::json
    {
        nlohmann::json snapshot;
        snapshot["runtime_plan_contract"] =
            explain_json.value("runtime_plan_contract", "");

        const auto join_graph_it = explain_json.find("join_graph");
        if (join_graph_it != explain_json.end() && join_graph_it->is_object())
        {
            snapshot["join_graph"]["contract"] =
                join_graph_it->value("contract", "");
            snapshot["join_graph"]["relations"] = nlohmann::json::array();
            const auto relations_it = join_graph_it->find("relations");
            if (relations_it != join_graph_it->end() && relations_it->is_array())
            {
                for (const auto& relation : *relations_it)
                {
                    nlohmann::json normalized_relation;
                    normalized_relation["alias"] = relation.value("alias", "");
                    normalized_relation["table_path"] =
                        relation.value("table_path", "");
                    normalized_relation["scan_kind"] =
                        relation.value("scan_kind", "");
                    normalized_relation["scan_family"] =
                        relation.value("scan_family", "");
                    normalized_relation["path_name"] =
                        relation.value("path_name", "");
                    normalized_relation["scan_family_kind"] =
                        relation.value("scan_family_kind", "");
                    normalized_relation["scan_family_tags"] =
                        relation.value("scan_family_tags",
                                       nlohmann::json::array());
                    normalized_relation["candidate_scan_families"] =
                        relation.value("candidate_scan_families",
                                       nlohmann::json::array());
                    normalized_relation["candidate_family_refusals"] =
                        relation.value("candidate_family_refusals",
                                       nlohmann::json::array());
                    normalized_relation["exactness_class"] =
                        relation.value("exactness_class", "");
                    normalized_relation["visibility_enforcement"] =
                        relation.value("visibility_enforcement", "");
                    normalized_relation["queryability_state"] =
                        relation.value("queryability_state", "");
                    normalized_relation["index_name"] =
                        relation.value("index_name", "");
                    normalized_relation["covering_index"] =
                        relation.value("covering_index", false);
                    normalized_relation["parameterized"] =
                        relation.value("parameterized", false);
                    normalized_relation["ordered_output"] =
                        relation.value("ordered_output", false);
                    normalized_relation["ordered_prefix_length"] =
                        relation.value("ordered_prefix_length", 0u);
                    normalized_relation["required_outer_relation_aliases"] =
                        relation.value("required_outer_relation_aliases",
                                       nlohmann::json::array());
                    normalized_relation["partition_pruned"] =
                        relation.value("partition_pruned", false);
                    normalized_relation["partition_key_columns"] =
                        relation.value("partition_key_columns",
                                       nlohmann::json::array());
                    normalized_relation["partition_targets_pruned_at_plan"] =
                        relation.value("partition_targets_pruned_at_plan",
                                       nlohmann::json::array());
                    normalized_relation["runtime_partition_pruning_eligible"] =
                        relation.value("runtime_partition_pruning_eligible", false);
                    normalized_relation["runtime_partition_pruning_sources"] =
                        relation.value("runtime_partition_pruning_sources",
                                       nlohmann::json::array());
                    snapshot["join_graph"]["relations"].push_back(
                        std::move(normalized_relation));
                }
            }

            snapshot["join_graph"]["join_steps"] = nlohmann::json::array();
            const auto steps_it = join_graph_it->find("join_steps");
            if (steps_it != join_graph_it->end() && steps_it->is_array())
            {
                for (const auto& step : *steps_it)
                {
                    nlohmann::json normalized_step;
                    normalized_step["join_type"] = step.value("join_type", "");
                    normalized_step["method"] = step.value("method", "");
                    normalized_step["join_edge_left_alias"] =
                        step.value("join_edge_left_alias", "");
                    normalized_step["join_edge_right_alias"] =
                        step.value("join_edge_right_alias", "");
                    normalized_step["disconnected_component"] =
                        step.value("disconnected_component", false);
                    normalized_step["legality_class"] =
                        step.value("legality_class", "");
                    normalized_step["legal_method_families"] =
                        step.value("legal_method_families",
                                   nlohmann::json::array());
                    normalized_step["method_enablers"] =
                        step.value("method_enablers", nlohmann::json::array());
                    normalized_step["reorderable"] =
                        step.value("reorderable", true);
                    normalized_step["outer_reorder_barrier"] =
                        step.value("outer_reorder_barrier", false);
                    normalized_step["semi_reorder_barrier"] =
                        step.value("semi_reorder_barrier", false);
                    normalized_step["anti_reorder_barrier"] =
                        step.value("anti_reorder_barrier", false);
                    normalized_step["using_reorder_barrier"] =
                        step.value("using_reorder_barrier", false);
                    normalized_step["natural_reorder_barrier"] =
                        step.value("natural_reorder_barrier", false);
                    normalized_step["lateral_reorder_barrier"] =
                        step.value("lateral_reorder_barrier", false);
                    snapshot["join_graph"]["join_steps"].push_back(
                        std::move(normalized_step));
                }
            }
        }

        const auto trace_it = explain_json.find("optimizer_trace");
        if (trace_it != explain_json.end() && trace_it->is_object())
        {
            snapshot["optimizer_trace"]["diagnostics_contract"] =
                trace_it->value("diagnostics_contract", "");
            snapshot["optimizer_trace"]["search_summary"] =
                trace_it->value("search_summary", nlohmann::json::object());
            const auto considered_it = trace_it->find("considered_paths");
            if (considered_it != trace_it->end() && considered_it->is_array())
            {
                snapshot["optimizer_trace"]["considered_path_count"] =
                    considered_it->size();
            }
            const auto rejected_it = trace_it->find("rejected_paths");
            if (rejected_it != trace_it->end() && rejected_it->is_array())
            {
                snapshot["optimizer_trace"]["rejected_path_count"] =
                    rejected_it->size();
            }
            const auto provenance_it = trace_it->find("statistics_provenance");
            if (provenance_it != trace_it->end() && provenance_it->is_array())
            {
                snapshot["optimizer_trace"]["statistics_provenance_count"] =
                    provenance_it->size();
            }
            const auto signals_it = trace_it->find("advisor_signals");
            if (signals_it != trace_it->end() && signals_it->is_array())
            {
                snapshot["optimizer_trace"]["advisor_signal_count"] =
                    signals_it->size();
            }
            const auto recommendations_it =
                trace_it->find("advisor_recommendations");
            if (recommendations_it != trace_it->end() &&
                recommendations_it->is_array())
            {
                snapshot["optimizer_trace"]["advisor_recommendation_count"] =
                    recommendations_it->size();
            }
        }

        const auto plan_root_it = explain_json.find("plan_root");
        if (plan_root_it != explain_json.end() && plan_root_it->is_object())
        {
            snapshot["plan_root"]["node_type"] =
                plan_root_it->value("node_type", "");
            snapshot["plan_root"]["join_type"] =
                plan_root_it->value("join_type", "");
            snapshot["plan_root"]["estimated_rows"] =
                plan_root_it->value("estimated_rows", 0u);
            snapshot["plan_root"]["actuals_available"] =
                plan_root_it->value("actuals_available", false);
            snapshot["plan_root"]["actual_rows"] =
                plan_root_it->value("actual_rows", 0u);
            snapshot["plan_root"]["spill_expected"] =
                plan_root_it->value("spill_expected", false);
        }

        const auto analyze_it = explain_json.find("analyze");
        if (analyze_it != explain_json.end() && analyze_it->is_object())
        {
            snapshot["analyze"]["rows"] = analyze_it->value("rows", 0u);
        }

        return snapshot;
    }

    auto normalizedRuntimeJoinStep(
        const scratchbird::optimizer::RuntimePlanJoinStep& step)
        -> nlohmann::json
    {
        return nlohmann::json{
            {"join_type", step.join_type},
            {"method", step.method},
            {"join_edge_left_alias", step.join_edge_left_alias},
            {"join_edge_right_alias", step.join_edge_right_alias},
            {"disconnected_component", step.disconnected_component},
            {"legality_class", step.legality_class},
            {"legal_method_families", step.legal_method_families},
            {"method_enablers", step.method_enablers},
            {"reorderable", step.reorderable},
            {"outer_reorder_barrier", step.outer_reorder_barrier},
            {"semi_reorder_barrier", step.semi_reorder_barrier},
            {"anti_reorder_barrier", step.anti_reorder_barrier},
            {"using_reorder_barrier", step.using_reorder_barrier},
            {"natural_reorder_barrier", step.natural_reorder_barrier},
            {"lateral_reorder_barrier", step.lateral_reorder_barrier},
            {"parameterized_dependency", step.parameterized_dependency},
            {"estimated_rows", step.estimated_rows},
            {"selectivity", step.selectivity},
        };
    }
}

TEST(IndexFamilyLoweringTest, OrderedExactFamiliesLowerToCanonicalTaxonomy)
{
    scratchbird::optimizer::PlannerFamilyLoweringRequest btree_range;
    btree_range.index_type = scratchbird::core::CatalogManager::IndexType::BTREE;
    btree_range.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::RANGE;
    const auto lowered_btree_range =
        scratchbird::optimizer::lowerPlannerFamily(btree_range);
    EXPECT_EQ(lowered_btree_range.family,
              scratchbird::optimizer::PlannerAccessFamily::BTREE_RANGE_SCAN);
    EXPECT_EQ(lowered_btree_range.family_name, "BTREE_RANGE_SCAN");
    EXPECT_EQ(lowered_btree_range.path_name, "BTREE_RANGE_SCAN");
    EXPECT_EQ(lowered_btree_range.exactness_class,
              scratchbird::optimizer::AccessPathExactnessClass::EXACT_KEY);
    EXPECT_EQ(lowered_btree_range.visibility_enforcement,
              scratchbird::optimizer::AccessPathVisibilityEnforcement::HYBRID);

    scratchbird::optimizer::PlannerFamilyLoweringRequest lsm_eq;
    lsm_eq.index_type = scratchbird::core::CatalogManager::IndexType::LSM;
    lsm_eq.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::EQUALITY;
    const auto lowered_lsm_eq =
        scratchbird::optimizer::lowerPlannerFamily(lsm_eq);
    EXPECT_EQ(lowered_lsm_eq.family,
              scratchbird::optimizer::PlannerAccessFamily::LSM_EQ_SCAN);
    EXPECT_EQ(lowered_lsm_eq.family_name, "LSM_EQ_SCAN");

    scratchbird::optimizer::PlannerFamilyLoweringRequest bitmap;
    bitmap.bitmap_combine = true;
    const auto lowered_bitmap =
        scratchbird::optimizer::lowerPlannerFamily(bitmap);
    EXPECT_EQ(lowered_bitmap.family,
              scratchbird::optimizer::PlannerAccessFamily::BITMAP_COMBINE_SCAN);
    EXPECT_EQ(lowered_bitmap.exactness_class,
              scratchbird::optimizer::AccessPathExactnessClass::CANDIDATE_REGION);
    EXPECT_EQ(lowered_bitmap.visibility_enforcement,
              scratchbird::optimizer::AccessPathVisibilityEnforcement::POST_FILTER);
}

TEST(IndexFamilyLoweringTest, UnsupportedOrIllegalLoweringFailsClosed)
{
    scratchbird::optimizer::PlannerFamilyLoweringRequest hash_range;
    hash_range.index_type = scratchbird::core::CatalogManager::IndexType::HASH;
    hash_range.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::RANGE;
    const auto lowered_hash_range =
        scratchbird::optimizer::lowerPlannerFamily(hash_range);
    EXPECT_EQ(lowered_hash_range.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);

    scratchbird::optimizer::PlannerFamilyLoweringRequest hnsw;
    hnsw.index_type = scratchbird::core::CatalogManager::IndexType::HNSW;
    const auto lowered_hnsw =
        scratchbird::optimizer::lowerPlannerFamily(hnsw);
    EXPECT_EQ(lowered_hnsw.family,
              scratchbird::optimizer::PlannerAccessFamily::HNSW_SCAN);
    EXPECT_EQ(lowered_hnsw.exactness_class,
              scratchbird::optimizer::AccessPathExactnessClass::UNKNOWN);
    EXPECT_EQ(lowered_hnsw.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
}

TEST(IndexFamilyLoweringTest, BroaderFamilyMatrixPublishesTypedMetadata)
{
    using IndexType = scratchbird::core::CatalogManager::IndexType;
    using PlannerAccessFamily = scratchbird::optimizer::PlannerAccessFamily;
    using ExactnessClass = scratchbird::optimizer::AccessPathExactnessClass;
    using VisibilityEnforcement =
        scratchbird::optimizer::AccessPathVisibilityEnforcement;
    using QueryabilityState =
        scratchbird::optimizer::AccessPathQueryabilityState;
    using PredicateMatchShape = scratchbird::optimizer::PredicateMatchShape;

    struct LoweringCase
    {
        IndexType index_type;
        PredicateMatchShape predicate_shape;
        bool nearest_order = false;
        bool strategy_bound = false;
        uint16_t strategy_number = 0;
        bool support_consistent = false;
        bool support_distance = false;
        bool nearest_lower_bound_validated = false;
        bool ranking_requested = false;
        bool corpus_stats_available = false;
        bool candidate_bitmap_available = false;
        uint64_t candidate_budget = 0;
        bool ann_metric_compatible = false;
        bool ann_rerank_enabled = false;
        bool ann_exact_fallback = false;
        PlannerAccessFamily expected_family = PlannerAccessFamily::UNKNOWN;
        ExactnessClass expected_exactness = ExactnessClass::UNKNOWN;
        VisibilityEnforcement expected_visibility =
            VisibilityEnforcement::UNKNOWN;
        QueryabilityState expected_queryability =
            QueryabilityState::QUERYABLE;
    };

    const std::vector<LoweringCase> cases = {
        {IndexType::BRIN,
         PredicateMatchShape::RANGE,
         false,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::BRIN_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::QUERYABLE},
        {IndexType::COLUMNSTORE,
         PredicateMatchShape::RANGE,
         false,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::COLUMNSTORE_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::GIST,
         PredicateMatchShape::EQUALITY,
         false,
         true,
         8,
         true,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::GIST_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::GIST,
         PredicateMatchShape::RANGE,
         false,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::GIST_SCAN,
         ExactnessClass::UNKNOWN,
         VisibilityEnforcement::UNKNOWN,
         QueryabilityState::INVALID},
        {IndexType::GIST,
         PredicateMatchShape::EQUALITY,
         true,
         true,
         8,
         true,
         true,
         true,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::GIST_NEAREST_SCAN,
         ExactnessClass::LOWER_BOUND_ORDERED,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::SPGIST,
         PredicateMatchShape::EQUALITY,
         false,
         true,
         8,
         true,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::SPGIST_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::RTREE,
         PredicateMatchShape::EQUALITY,
         false,
         true,
         8,
         true,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::RTREE_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::RTREE,
         PredicateMatchShape::EQUALITY,
         true,
         true,
         8,
         true,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::RTREE_NEAREST_SCAN,
         ExactnessClass::UNKNOWN,
         VisibilityEnforcement::UNKNOWN,
         QueryabilityState::INVALID},
        {IndexType::GIN,
         PredicateMatchShape::EQUALITY,
         false,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::GIN_FILTER_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::FULLTEXT,
         PredicateMatchShape::LIKE_PREFIX,
         false,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::TEXT_RECHECK_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::FULLTEXT,
         PredicateMatchShape::EQUALITY,
         false,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         true,
         32,
         false,
         false,
         false,
         PlannerAccessFamily::TEXT_BITMAP_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::FULLTEXT,
         PredicateMatchShape::EQUALITY,
         false,
         false,
         0,
         false,
         false,
         false,
         true,
         false,
         true,
         64,
         false,
         false,
         false,
         PlannerAccessFamily::TEXT_SCORE_SCAN,
         ExactnessClass::UNKNOWN,
         VisibilityEnforcement::UNKNOWN,
         QueryabilityState::INVALID},
        {IndexType::FULLTEXT,
         PredicateMatchShape::EQUALITY,
         false,
         false,
         0,
         false,
         false,
         false,
         true,
         true,
         true,
         64,
         false,
         false,
         false,
         PlannerAccessFamily::TEXT_SCORE_SCAN,
         ExactnessClass::CANDIDATE_REGION,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::HNSW,
         PredicateMatchShape::NONE,
         false,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         0,
         false,
         false,
         false,
         PlannerAccessFamily::HNSW_SCAN,
         ExactnessClass::UNKNOWN,
         VisibilityEnforcement::UNKNOWN,
         QueryabilityState::INVALID},
        {IndexType::HNSW,
         PredicateMatchShape::NONE,
         true,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         96,
         true,
         false,
         false,
         PlannerAccessFamily::HNSW_SCAN,
         ExactnessClass::APPROX_TOPK,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::HNSW,
         PredicateMatchShape::NONE,
         true,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         128,
         true,
         true,
         false,
         PlannerAccessFamily::ANN_RERANK_SCAN,
         ExactnessClass::APPROX_TOPK,
         VisibilityEnforcement::POST_FILTER,
         QueryabilityState::LIMITED},
        {IndexType::IVF,
         PredicateMatchShape::NONE,
         true,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         160,
         true,
         false,
         true,
         PlannerAccessFamily::ANN_HYBRID_FALLBACK_SCAN,
         ExactnessClass::EXACT_ROW,
         VisibilityEnforcement::HYBRID,
         QueryabilityState::QUERYABLE},
        {IndexType::VECTOR_FLAT,
         PredicateMatchShape::NONE,
         true,
         false,
         0,
         false,
         false,
         false,
         false,
         false,
         false,
         32,
         true,
         false,
         false,
         PlannerAccessFamily::VECTOR_FLAT_SCAN,
         ExactnessClass::EXACT_ROW,
         VisibilityEnforcement::HYBRID,
         QueryabilityState::QUERYABLE},
    };

    for (const auto& test_case : cases)
    {
        SCOPED_TRACE(static_cast<int>(test_case.index_type));
        scratchbird::optimizer::PlannerFamilyLoweringRequest request;
        request.index_type = test_case.index_type;
        request.predicate_shape = test_case.predicate_shape;
        request.nearest_order = test_case.nearest_order;
        request.ranking_requested = test_case.ranking_requested;
        request.corpus_stats_available = test_case.corpus_stats_available;
        request.candidate_bitmap_available =
            test_case.candidate_bitmap_available;
        request.candidate_budget = test_case.candidate_budget;
        request.ann_metric_compatible = test_case.ann_metric_compatible;
        request.ann_rerank_enabled = test_case.ann_rerank_enabled;
        request.ann_exact_fallback = test_case.ann_exact_fallback;
        request.strategy_bound = test_case.strategy_bound;
        request.strategy_number = test_case.strategy_number;
        request.support_consistent = test_case.support_consistent;
        request.support_distance = test_case.support_distance;
        request.nearest_lower_bound_validated =
            test_case.nearest_lower_bound_validated;

        const auto lowered = scratchbird::optimizer::lowerPlannerFamily(request);
        EXPECT_EQ(lowered.family, test_case.expected_family);
        EXPECT_EQ(lowered.family_name,
                  scratchbird::optimizer::plannerAccessFamilyName(
                      test_case.expected_family));
        EXPECT_EQ(lowered.path_name, lowered.family_name);
        EXPECT_EQ(lowered.exactness_class, test_case.expected_exactness);
        EXPECT_EQ(lowered.visibility_enforcement,
                  test_case.expected_visibility);
        EXPECT_EQ(lowered.queryability_state,
                  test_case.expected_queryability);
    }
}

TEST(IndexFamilyLoweringTest,
     GeneralizedInvalidCasesPublishSpecificRejectionCodes)
{
    using IndexType = scratchbird::core::CatalogManager::IndexType;

    scratchbird::optimizer::PlannerFamilyLoweringRequest gist_unbound;
    gist_unbound.index_type = IndexType::GIST;
    gist_unbound.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::EQUALITY;
    const auto lowered_gist_unbound =
        scratchbird::optimizer::lowerPlannerFamily(gist_unbound);
    EXPECT_EQ(lowered_gist_unbound.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            gist_unbound, lowered_gist_unbound),
        "P08_OPERATOR_STRATEGY_UNBOUND");
    EXPECT_NE(
        scratchbird::optimizer::plannerFamilyLoweringRejectionDetail(
            gist_unbound, lowered_gist_unbound)
            .find("strategy binding"),
        std::string::npos);

    scratchbird::optimizer::PlannerFamilyLoweringRequest gist_nearest_missing_distance;
    gist_nearest_missing_distance.index_type = IndexType::GIST;
    gist_nearest_missing_distance.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::EQUALITY;
    gist_nearest_missing_distance.nearest_order = true;
    gist_nearest_missing_distance.strategy_bound = true;
    gist_nearest_missing_distance.strategy_number = 8;
    gist_nearest_missing_distance.support_consistent = true;
    gist_nearest_missing_distance.support_distance = false;
    gist_nearest_missing_distance.nearest_lower_bound_validated = false;
    const auto lowered_gist_nearest_missing_distance =
        scratchbird::optimizer::lowerPlannerFamily(
            gist_nearest_missing_distance);
    EXPECT_EQ(lowered_gist_nearest_missing_distance.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            gist_nearest_missing_distance,
            lowered_gist_nearest_missing_distance),
        "P08_DISTANCE_SUPPORT_UNVALIDATED");

    scratchbird::optimizer::PlannerFamilyLoweringRequest gist_nearest_missing_lb;
    gist_nearest_missing_lb.index_type = IndexType::GIST;
    gist_nearest_missing_lb.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::EQUALITY;
    gist_nearest_missing_lb.nearest_order = true;
    gist_nearest_missing_lb.strategy_bound = true;
    gist_nearest_missing_lb.strategy_number = 8;
    gist_nearest_missing_lb.support_consistent = true;
    gist_nearest_missing_lb.support_distance = true;
    gist_nearest_missing_lb.nearest_lower_bound_validated = false;
    const auto lowered_gist_nearest_missing_lb =
        scratchbird::optimizer::lowerPlannerFamily(gist_nearest_missing_lb);
    EXPECT_EQ(lowered_gist_nearest_missing_lb.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            gist_nearest_missing_lb,
            lowered_gist_nearest_missing_lb),
        "P08_NEAREST_ORDER_UNVALIDATED");

    scratchbird::optimizer::PlannerFamilyLoweringRequest gist_missing_support_fn;
    gist_missing_support_fn.index_type = IndexType::GIST;
    gist_missing_support_fn.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::EQUALITY;
    gist_missing_support_fn.strategy_bound = true;
    gist_missing_support_fn.support_consistent = false;
    const auto lowered_gist_missing_support_fn =
        scratchbird::optimizer::lowerPlannerFamily(
            gist_missing_support_fn);
    EXPECT_EQ(lowered_gist_missing_support_fn.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            gist_missing_support_fn,
            lowered_gist_missing_support_fn),
        "P08_SUPPORT_FUNCTION_UNVALIDATED");
    EXPECT_NE(
        scratchbird::optimizer::plannerFamilyLoweringRejectionDetail(
            gist_missing_support_fn,
            lowered_gist_missing_support_fn)
            .find("validated support function"),
        std::string::npos);
}

TEST(IndexFamilyLoweringTest, AnnInvalidCasesPublishSpecificRejectionCodes)
{
    using IndexType = scratchbird::core::CatalogManager::IndexType;

    scratchbird::optimizer::PlannerFamilyLoweringRequest hnsw_without_nearest;
    hnsw_without_nearest.index_type = IndexType::HNSW;
    hnsw_without_nearest.candidate_budget = 64;
    const auto lowered_hnsw_without_nearest =
        scratchbird::optimizer::lowerPlannerFamily(hnsw_without_nearest);
    EXPECT_EQ(lowered_hnsw_without_nearest.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            hnsw_without_nearest, lowered_hnsw_without_nearest),
        "P08_ANN_NEAREST_ORDER_REQUIRED");
    EXPECT_NE(
        scratchbird::optimizer::plannerFamilyLoweringRejectionDetail(
            hnsw_without_nearest, lowered_hnsw_without_nearest)
            .find("nearest-order"),
        std::string::npos);

    scratchbird::optimizer::PlannerFamilyLoweringRequest ivf_metric_incompatible;
    ivf_metric_incompatible.index_type = IndexType::IVF;
    ivf_metric_incompatible.nearest_order = true;
    ivf_metric_incompatible.candidate_budget = 128;
    ivf_metric_incompatible.ann_metric_compatible = false;
    const auto lowered_ivf_metric_incompatible =
        scratchbird::optimizer::lowerPlannerFamily(ivf_metric_incompatible);
    EXPECT_EQ(lowered_ivf_metric_incompatible.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            ivf_metric_incompatible, lowered_ivf_metric_incompatible),
        "P08_ANN_METRIC_INCOMPATIBLE");

    scratchbird::optimizer::PlannerFamilyLoweringRequest vector_budget_missing;
    vector_budget_missing.index_type = IndexType::VECTOR_FLAT;
    vector_budget_missing.nearest_order = true;
    vector_budget_missing.ann_metric_compatible = true;
    vector_budget_missing.candidate_budget = 0;
    const auto lowered_vector_budget_missing =
        scratchbird::optimizer::lowerPlannerFamily(vector_budget_missing);
    EXPECT_EQ(lowered_vector_budget_missing.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            vector_budget_missing, lowered_vector_budget_missing),
        "P08_ANN_CANDIDATE_BUDGET_REQUIRED");
    EXPECT_NE(
        scratchbird::optimizer::plannerFamilyLoweringRejectionDetail(
            vector_budget_missing, lowered_vector_budget_missing)
            .find("candidate budget"),
        std::string::npos);
}

TEST(IndexFamilyLoweringTest, TextInvalidCasesPublishSpecificRejectionCodes)
{
    using IndexType = scratchbird::core::CatalogManager::IndexType;

    scratchbird::optimizer::PlannerFamilyLoweringRequest text_missing_stats;
    text_missing_stats.index_type = IndexType::FULLTEXT;
    text_missing_stats.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::EQUALITY;
    text_missing_stats.ranking_requested = true;
    text_missing_stats.corpus_stats_available = false;
    text_missing_stats.candidate_budget = 64;
    const auto lowered_text_missing_stats =
        scratchbird::optimizer::lowerPlannerFamily(text_missing_stats);
    EXPECT_EQ(lowered_text_missing_stats.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            text_missing_stats, lowered_text_missing_stats),
        "P08_TEXT_CORPUS_STATS_REQUIRED");
    EXPECT_NE(
        scratchbird::optimizer::plannerFamilyLoweringRejectionDetail(
            text_missing_stats, lowered_text_missing_stats)
            .find("corpus scoring statistics"),
        std::string::npos);

    scratchbird::optimizer::PlannerFamilyLoweringRequest text_missing_budget;
    text_missing_budget.index_type = IndexType::FULLTEXT;
    text_missing_budget.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::EQUALITY;
    text_missing_budget.ranking_requested = true;
    text_missing_budget.corpus_stats_available = true;
    text_missing_budget.candidate_budget = 0;
    const auto lowered_text_missing_budget =
        scratchbird::optimizer::lowerPlannerFamily(text_missing_budget);
    EXPECT_EQ(lowered_text_missing_budget.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            text_missing_budget, lowered_text_missing_budget),
        "P08_TEXT_CANDIDATE_BUDGET_REQUIRED");
    EXPECT_NE(
        scratchbird::optimizer::plannerFamilyLoweringRejectionDetail(
            text_missing_budget, lowered_text_missing_budget)
            .find("candidate budget"),
        std::string::npos);
}

TEST(IndexFamilyLoweringTest, HashInvalidCasesPublishSpecificRejectionCodes)
{
    using IndexType = scratchbird::core::CatalogManager::IndexType;

    scratchbird::optimizer::PlannerFamilyLoweringRequest hash_range;
    hash_range.index_type = IndexType::HASH;
    hash_range.predicate_shape =
        scratchbird::optimizer::PredicateMatchShape::RANGE;
    const auto lowered_hash_range =
        scratchbird::optimizer::lowerPlannerFamily(hash_range);
    EXPECT_EQ(lowered_hash_range.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::INVALID);
    EXPECT_STREQ(
        scratchbird::optimizer::plannerFamilyLoweringRejectionCode(
            hash_range, lowered_hash_range),
        "P08_HASH_EQ_PREDICATE_REQUIRED");
    EXPECT_NE(
        scratchbird::optimizer::plannerFamilyLoweringRejectionDetail(
            hash_range, lowered_hash_range)
            .find("only equality predicates"),
        std::string::npos);
}

TEST(IndexFamilyLoweringTest,
     CanonicalPlannerBundleRefusalClassifiesFamilyLegalityFailures)
{
    const auto undeclared_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_FAMILY_LEGALITY_UNDECLARED",
            "trust=UNKNOWN locator=UNKNOWN visibility=UNKNOWN");
    EXPECT_EQ(undeclared_class,
              "missing canonical family legality classification");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            undeclared_class),
        "POLICY");

    const auto trust_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_FAMILY_LEGALITY_TRUST",
            "candidate violates canonical family trust legality matrix: trust=NATIVE_EXACT exactness=EXACT_KEY");
    EXPECT_EQ(trust_class,
              "trust class violates canonical family legality matrix");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            trust_class),
        "POLICY");

    const auto locator_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_FAMILY_LEGALITY_LOCATOR",
            "candidate violates canonical family trust legality matrix: locator=PAGE_RANGE exactness=EXACT_ROW");
    EXPECT_EQ(locator_class,
              "locator granularity violates canonical family legality matrix");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            locator_class),
        "POLICY");

    const auto visibility_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_FAMILY_LEGALITY_VISIBILITY",
            "candidate violates canonical family trust legality matrix: visibility=POST_FILTER exactness=EXACT_ROW");
    EXPECT_EQ(
        visibility_class,
        "visibility enforcement violates canonical family legality matrix");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            visibility_class),
        "POLICY");
}

TEST(IndexFamilyLoweringTest,
     CanonicalPlannerBundleRefusalClassifiesRemainingFamilyFailureCodes)
{
    const auto support_function_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_SUPPORT_FUNCTION_UNVALIDATED",
            "generalized or spatial candidate was rejected because a validated support function was not available");
    EXPECT_EQ(support_function_class, "unsupported operator shape");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            support_function_class),
        "OPERATOR");

    const auto bitmap_compose_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_BITMAP_COMPOSE_UNAVAILABLE",
            "bitmap composition could not be lowered to a queryable planner family");
    EXPECT_EQ(bitmap_compose_class, "unsupported operator shape");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            bitmap_compose_class),
        "OPERATOR");

    const auto skip_scan_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_SKIP_SCAN_UNAVAILABLE",
            "skip-scan candidate did not lower to a queryable skip family");
    EXPECT_EQ(skip_scan_class, "unsupported operator shape");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            skip_scan_class),
        "OPERATOR");

    const auto expression_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_EXPRESSION_INDEX_MISMATCH",
            "expression index expression not present in relation filter");
    EXPECT_EQ(expression_class, "semantic mismatch");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            expression_class),
        "SEMANTICS");

    const auto generic_fail_closed_class =
        scratchbird::optimizer::canonicalPlannerBundleRefusalClass(
            "P08_FAMILY_NOT_QUERYABLE",
            "planner family lowering marked the candidate as invalid for query execution");
    EXPECT_EQ(generic_fail_closed_class,
              "fail-closed family-specific safety rule");
    EXPECT_EQ(
        scratchbird::optimizer::canonicalPlannerBundleRefusalCauseDomain(
            generic_fail_closed_class),
        "POLICY");
}

/**
 * Integration test for Query Planner (Phase 1, Task 1.3)
 *
 * Verifies that:
 * 1. Query planner is properly integrated with QueryCompilerV3
 * 2. Optimizer components are initialized
 * 3. Bytecode structure is preserved
 */
class QueryPlannerIntegrationTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.reset();
        db_file_.reset();
    }

    bool createDatabase()
    {
        static std::once_flag log_level_once;
        std::call_once(log_level_once, []() {
            Logger::getInstance().setLogLevel(LogLevel::ERROR);
        });

        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
        QueryCompilerV3::invalidateAllPlanCache();
        QueryCompilerV3::resetPlanCacheStats();
        db_file_ = std::make_unique<TestDatabaseFile>("test_query_planner");

        ErrorContext ctx;
        Status status = Database::create(db_file_->path(), 16384, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        db_ = std::make_unique<Database>();
        status = db_->open(db_file_->path(), &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        auto* catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            return false;
        }

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog->getSchema("public", public_schema_info, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        status = db_->connect(connection_ctx_, &ctx);
        if (status != Status::OK)
        {
            return false;
        }
        connection_ctx_->setCurrentSchemaId(public_schema_info.schema_id);
        const auto system_user_id = catalog->getSystemUserId(&ctx);
        if (isZeroId(system_user_id))
        {
            return false;
        }
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());

        const std::vector<std::string> ddl = {
            "CREATE TABLE users (id INTEGER, name VARCHAR(100), email VARCHAR(100), age INTEGER)",
            "CREATE TABLE products (id INTEGER, name VARCHAR(100), price DOUBLE)",
            "CREATE TABLE orders (id INTEGER, user_id INTEGER, amount DOUBLE)",
            "CREATE TABLE test (id INTEGER)",
            "GRANT SELECT ON users TO PUBLIC",
            "GRANT SELECT ON products TO PUBLIC",
            "GRANT SELECT ON orders TO PUBLIC",
            "GRANT SELECT ON test TO PUBLIC"
        };

        for (const auto& sql : ddl)
        {
            auto compile_result = compiler_->compile(sql);
            if (!compile_result.success())
            {
                return false;
            }
            auto exec_result = executor_->execute(compile_result.bytecode());
            if (!exec_result.success())
            {
                return false;
            }
        }

        return true;
    }

    bool publishSevereMgaChurn(const std::string &table_name,
                               size_t advisory_count = 8,
                               uint64_t reclaimable_bytes = 65536,
                               uint32_t deleted_slots = 64,
                               uint16_t chain_depth_hint = 8,
                               double same_page_update_ratio = 0.05)
    {
        if (db_ == nullptr || connection_ctx_ == nullptr)
        {
            return false;
        }

        CatalogManager::TableInfo table_info;
        if (db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                             table_name,
                                             table_info,
                                             nullptr) != Status::OK)
        {
            return false;
        }

        for (size_t i = 0; i < advisory_count; ++i)
        {
            StorageEngine::FragmentationAdvisory advisory{};
            advisory.page_id = static_cast<uint32_t>(100 + i);
            advisory.reclaimable_bytes = reclaimable_bytes;
            advisory.deleted_slots = deleted_slots;
            advisory.chain_depth_hint = chain_depth_hint;
            advisory.same_page_back_versions =
                std::max<uint16_t>(1, chain_depth_hint / 8);
            advisory.same_page_update_ratio = same_page_update_ratio;
            advisory.dead_space_ratio = 0.85;
            advisory.rewrite_recommended = true;
            db_->storage_engine()->publishFragmentationAdvisory(
                table_info.table_id, advisory.page_id, advisory);
        }

        return true;
    }

    std::vector<uint8_t> compileSQL(const std::string &sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            last_compile_errors_.clear();
            for (const auto& err : compile_result.errors())
            {
                if (!last_compile_errors_.empty())
                {
                    last_compile_errors_ += "\n";
                }
                last_compile_errors_ += err;
            }
            return {};
        }
        last_compile_errors_.clear();
        return compile_result.bytecode();
    }

    QueryCompilerV3::CompileResult compileSQLWithParameters(
        const std::string& sql,
        const optimizer::ParameterBindings& bindings)
    {
        auto compile_result = compiler_->compileWithParameters(sql, bindings);
        if (!compile_result.success())
        {
            last_compile_errors_.clear();
            for (const auto& err : compile_result.errors())
            {
                if (!last_compile_errors_.empty())
                {
                    last_compile_errors_ += "\n";
                }
                last_compile_errors_ += err;
            }
        }
        else
        {
            last_compile_errors_.clear();
        }
        return compile_result;
    }

    ExecutionResult executeSQL(const std::string &sql)
    {
        auto bytecode = compileSQL(sql);
        if (bytecode.empty())
        {
            return ExecutionResult("Compilation failed: " + last_compile_errors_);
        }
        return executor_->execute(bytecode);
    }

    ExecutionResult executeBytecode(const std::vector<uint8_t>& bytecode)
    {
        if (bytecode.empty())
        {
            return ExecutionResult("Bytecode payload is empty");
        }
        return executor_->execute(bytecode);
    }

    ExecutionResult executeBytecodeWithParameters(
        const std::vector<uint8_t>& bytecode,
        const std::vector<std::string>& values,
        const std::vector<bool>& nulls = {})
    {
        if (bytecode.empty())
        {
            return ExecutionResult("Bytecode payload is empty");
        }
        executor_->setParameters(values, nulls);
        auto result = executor_->execute(bytecode);
        executor_->clearParameters();
        return result;
    }

    bool containsOpcode(const std::vector<uint8_t> &bytecode, sblr_v3::Opcode opcode)
    {
        const auto target = static_cast<uint16_t>(opcode);
        std::function<bool(const sblr_v3::Value&)> valueContainsOpcode;
        std::function<bool(const sblr_v3::Instruction&)> instructionContainsOpcode;

        valueContainsOpcode = [&](const sblr_v3::Value& value) -> bool {
            if (const auto* stmt = std::get_if<sblr_v3::Value::InstrPtr>(&value.data)) {
                return *stmt && instructionContainsOpcode(**stmt);
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data)) {
                for (const auto& element : *list) {
                    if (valueContainsOpcode(element)) {
                        return true;
                    }
                }
                return false;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data)) {
                for (const auto& [_, element] : *obj) {
                    if (valueContainsOpcode(element)) {
                        return true;
                    }
                }
                return false;
            }
            return false;
        };

        instructionContainsOpcode = [&](const sblr_v3::Instruction& inst) -> bool {
            if (inst.opcode == target) {
                return true;
            }
            return valueContainsOpcode(inst.payload);
        };

        sblr_v3::Container container;
        std::string err;
        if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        {
            return false;
        }
        size_t offset = 0;
        sblr_v3::DecodeError decode_err;
        while (offset < container.bytecode_stream.size())
        {
            sblr_v3::Instruction inst;
            if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                      container.bytecode_stream.size(),
                                                      offset,
                                                      inst,
                                                      decode_err) &&
                !sblr_v3::decodeInstruction(container.bytecode_stream.data(),
                                            container.bytecode_stream.size(),
                                            offset,
                                            inst,
                                            decode_err))
            {
                break;
            }
            if (instructionContainsOpcode(inst))
            {
                return true;
            }
        }
        return false;
    }

    bool decodeFirstSelect(const std::vector<uint8_t>& bytecode, sblr_v3::Instruction& out)
    {
        sblr_v3::Container container;
        std::string err;
        if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        {
            return false;
        }
        size_t offset = 0;
        sblr_v3::DecodeError decode_err;
        while (offset < container.bytecode_stream.size())
        {
            sblr_v3::Instruction inst;
            if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                      container.bytecode_stream.size(),
                                                      offset,
                                                      inst,
                                                      decode_err))
            {
                return false;
            }
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_SELECT)
            {
                out = std::move(inst);
                return true;
            }
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_EXPLAIN_PLAN)
            {
                const auto* obj = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
                if (!obj)
                {
                    return false;
                }
                auto it_query = obj->find("query");
                if (it_query == obj->end())
                {
                    return false;
                }
                const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&it_query->second.data);
                if (ptr && *ptr &&
                    static_cast<sblr_v3::Opcode>((*ptr)->opcode) == sblr_v3::Opcode::SBLR3_SELECT)
                {
                    out = **ptr;
                    return true;
                }
            }
        }
        return false;
    }

    bool decodeRuntimePlan(const std::vector<uint8_t>& bytecode,
                           scratchbird::optimizer::RuntimePlan& plan_out)
    {
        sblr_v3::Instruction select_inst;
        if (!decodeFirstSelect(bytecode, select_inst))
        {
            return false;
        }
        const auto* obj = std::get_if<sblr_v3::Value::Object>(&select_inst.payload.data);
        if (!obj)
        {
            return false;
        }
        auto it_plan = obj->find("plan");
        if (it_plan == obj->end())
        {
            return false;
        }
        const auto* bytes = std::get_if<sblr_v3::Value::Bytes>(&it_plan->second.data);
        if (!bytes)
        {
            return false;
        }
        std::string err;
        return scratchbird::optimizer::decodeRuntimePlan(*bytes, plan_out, err);
    }

    const scratchbird::optimizer::RuntimePlanControlEntry* findOptimizerControl(
        const scratchbird::optimizer::RuntimePlan& plan,
        const std::string& name)
    {
        for (const auto& entry : plan.optimizer_controls)
        {
            if (entry.name == name)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    void seedMemoryGrantFeedback(const std::string& sql,
                                 const std::string& operator_kind,
                                 uint64_t last_grant_bytes,
                                 uint64_t p50_bytes,
                                 uint64_t p90_bytes,
                                 uint64_t peak_bytes,
                                 uint64_t spill_count = 1,
                                 const std::string& state = "STABLE",
                                 uint64_t sample_count = 8,
                                 uint8_t underuse_streak = 0,
                                 uint64_t oscillation_count = 0,
                                 int8_t last_adjustment_direction = 0,
                                 uint8_t oscillation_disable_count = 0,
                                 const std::string& cache_mode = "GENERIC",
                                 const std::string& execution_intent_class =
                                     "EXECUTE",
                                 const std::string& storage_layer_shape =
                                     "ROW_STORE_MGA",
                                 const std::string& planner_policy_snapshot_id =
                                     std::string())
    {
        ASSERT_NE(db_, nullptr);
        auto* catalog = db_->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ErrorContext ctx;
        CatalogManager::SchemaInfo public_schema;
        ASSERT_EQ(catalog->getSchema("public", public_schema, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
        feedback.grant_feedback_uuid = generateUuidV7();
        feedback.database_uuid = db_->uuid();
        feedback.schema_root_uuid = public_schema.schema_id;
        feedback.operator_kind = operator_kind;
        feedback.sample_count = sample_count;
        feedback.last_grant_bytes = last_grant_bytes;
        feedback.p50_bytes = p50_bytes;
        feedback.p90_bytes = p90_bytes;
        feedback.peak_bytes = peak_bytes;
        feedback.spill_count = spill_count;
        feedback.cancel_count = 0;
        feedback.oscillation_count = oscillation_count;
        feedback.underuse_streak = underuse_streak;
        feedback.last_adjustment_direction = last_adjustment_direction;
        feedback.oscillation_disable_count = oscillation_disable_count;
        feedback.state = state;
        feedback.updated_at = 1;
        const std::string effective_policy_snapshot_id =
            !planner_policy_snapshot_id.empty()
                ? planner_policy_snapshot_id
                : (connection_ctx_ != nullptr
                       ? connection_ctx_->dialect_tag()
                       : std::string());
        feedback.grant_key_hash = memoryGrantFeedbackKeyHashForTest(
            db_->uuid(),
            public_schema.schema_id,
            plannerNormalizedStatementIdForSql(sql),
            effective_policy_snapshot_id,
            cache_mode,
            execution_intent_class,
            storage_layer_shape,
            operator_kind);
        ASSERT_NE(feedback.grant_key_hash, 0u);
        ASSERT_EQ(catalog->upsertMemoryGrantFeedbackCatalogEntry(feedback, &ctx),
                  Status::OK)
            << ctx.message;
    }

    bool loadMemoryGrantFeedback(const std::string& sql,
                                 const std::string& operator_kind,
                                 CatalogManager::MemoryGrantFeedbackCatalogInfo& feedback_out,
                                 const std::string& cache_mode = "GENERIC",
                                 const std::string& execution_intent_class =
                                     "EXECUTE",
                                 const std::string& storage_layer_shape =
                                     "ROW_STORE_MGA",
                                 const std::string& planner_policy_snapshot_id =
                                     std::string())
    {
        if (db_ == nullptr || db_->catalog_manager() == nullptr)
        {
            return false;
        }

        auto* catalog = db_->catalog_manager();
        ErrorContext ctx;
        CatalogManager::SchemaInfo public_schema;
        if (catalog->getSchema("public", public_schema, &ctx) != Status::OK)
        {
            return false;
        }

        const std::string effective_policy_snapshot_id =
            !planner_policy_snapshot_id.empty()
                ? planner_policy_snapshot_id
                : (connection_ctx_ != nullptr
                       ? connection_ctx_->dialect_tag()
                       : std::string());
        const uint64_t grant_key_hash = memoryGrantFeedbackKeyHashForTest(
            db_->uuid(),
            public_schema.schema_id,
            plannerNormalizedStatementIdForSql(sql),
            effective_policy_snapshot_id,
            cache_mode,
            execution_intent_class,
            storage_layer_shape,
            operator_kind);
        if (grant_key_hash == 0)
        {
            return false;
        }

        return catalog->getMemoryGrantFeedbackCatalogEntry(grant_key_hash,
                                                           feedback_out,
                                                           &ctx) == Status::OK;
    }

    nlohmann::json loadTableMetadataJson(const std::string& table_name)
    {
        nlohmann::json metadata = nlohmann::json::object();
        if (db_ == nullptr)
        {
            ADD_FAILURE() << "Database is not initialized";
            return metadata;
        }
        auto* catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            ADD_FAILURE() << "Catalog manager is not available";
            return metadata;
        }

        ErrorContext ctx;
        CatalogManager::SchemaInfo public_schema;
        if (catalog->getSchema("public", public_schema, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to resolve public schema: " << ctx.message;
            return metadata;
        }

        CatalogManager::TableInfo table_info;
        if (catalog->getTable(public_schema.schema_id, table_name, table_info, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to load table metadata row: " << ctx.message;
            return metadata;
        }
        if (isZeroId(table_info.storage_params_oid))
        {
            ADD_FAILURE() << "Table storage_params_oid was empty for " << table_name;
            return metadata;
        }

        std::string params;
        if (catalog->loadStringFromToast(table_info.storage_params_oid, 0, params, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to load table metadata TOAST payload: " << ctx.message;
            return metadata;
        }
        if (params.empty())
        {
            ADD_FAILURE() << "Table metadata payload was empty for " << table_name;
            return metadata;
        }

        try
        {
            metadata = nlohmann::json::parse(params);
        }
        catch (const std::exception& ex)
        {
            ADD_FAILURE() << "Failed to parse metadata JSON: " << ex.what();
        }
        return metadata;
    }

    bool findFirstOpcode(const sblr_v3::Instruction& root,
                         sblr_v3::Opcode target,
                         sblr_v3::Instruction& out)
    {
        std::function<bool(const sblr_v3::Instruction&)> visit_inst;
        std::function<bool(const sblr_v3::Value&)> visit_value;

        visit_value = [&](const sblr_v3::Value& value) -> bool {
            if (const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&value.data))
            {
                return ptr != nullptr && *ptr != nullptr && visit_inst(**ptr);
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data))
            {
                for (const auto& entry : *list)
                {
                    if (visit_value(entry))
                    {
                        return true;
                    }
                }
                return false;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data))
            {
                for (const auto& [_, entry] : *obj)
                {
                    if (visit_value(entry))
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        visit_inst = [&](const sblr_v3::Instruction& inst) -> bool {
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == target)
            {
                out = inst;
                return true;
            }
            return visit_value(inst.payload);
        };

        return visit_inst(root);
    }

    std::vector<std::pair<std::string, std::string>> collectColumnRefs(
        const sblr_v3::Instruction& root)
    {
        std::vector<std::pair<std::string, std::string>> refs;
        std::function<void(const sblr_v3::Instruction&)> visit_inst;
        std::function<void(const sblr_v3::Value&)> visit_value;

        visit_value = [&](const sblr_v3::Value& value) {
            if (const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&value.data))
            {
                if (ptr != nullptr && *ptr != nullptr)
                {
                    visit_inst(**ptr);
                }
                return;
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data))
            {
                for (const auto& entry : *list)
                {
                    visit_value(entry);
                }
                return;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data))
            {
                for (const auto& [_, entry] : *obj)
                {
                    visit_value(entry);
                }
            }
        };

        visit_inst = [&](const sblr_v3::Instruction& inst) {
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_COLUMN_REF)
            {
                const auto* obj = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
                if (obj != nullptr)
                {
                    std::string column_name;
                    std::string qualifier;
                    auto col_it = obj->find("column");
                    if (col_it != obj->end())
                    {
                        if (const auto* col = std::get_if<std::string>(&col_it->second.data))
                        {
                            column_name = *col;
                        }
                    }
                    auto path_it = obj->find("path");
                    if (path_it != obj->end())
                    {
                        if (const auto* path =
                                std::get_if<sblr_v3::Value::List>(&path_it->second.data))
                        {
                            for (auto it = path->rbegin(); it != path->rend(); ++it)
                            {
                                if (const auto* part = std::get_if<std::string>(&it->data))
                                {
                                    if (!part->empty())
                                    {
                                        qualifier = *part;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    refs.emplace_back(std::move(qualifier), std::move(column_name));
                }
            }
            visit_value(inst.payload);
        };

        visit_inst(root);
        return refs;
    }

    std::vector<std::string> resultStrings(const ExecutionResult& result)
    {
        std::vector<std::string> lines;
        if (!result.success() || !result.hasResultSet() || result.resultSet() == nullptr)
        {
            return lines;
        }

        auto* rs = result.resultSet();
        for (size_t row = 0; row < rs->rowCount(); ++row)
        {
            lines.push_back(rs->getValue(row, 0).toString());
        }
        return lines;
    }

    size_t resultRowCount(const ExecutionResult& result)
    {
        if (!result.success() || !result.hasResultSet() || result.resultSet() == nullptr)
        {
            return 0;
        }
        return result.resultSet()->rowCount();
    }

    double meanDurationMs(size_t iterations,
                          const std::function<bool()>& fn)
    {
        if (iterations == 0)
        {
            return 0.0;
        }

        using clock = std::chrono::steady_clock;
        double total_ms = 0.0;
        for (size_t i = 0; i < iterations; ++i)
        {
            const auto start = clock::now();
            if (!fn())
            {
                return -1.0;
            }
            const auto end = clock::now();
            total_ms += std::chrono::duration<double, std::milli>(end - start).count();
        }
        return total_ms / static_cast<double>(iterations);
    }

    auto sampleDurationsMs(size_t iterations,
                           const std::function<bool()>& fn)
        -> std::vector<double>
    {
        std::vector<double> samples;
        samples.reserve(iterations);
        using clock = std::chrono::steady_clock;
        for (size_t i = 0; i < iterations; ++i)
        {
            const auto start = clock::now();
            if (!fn())
            {
                return {};
            }
            const auto end = clock::now();
            samples.push_back(
                std::chrono::duration<double, std::milli>(end - start).count());
        }
        return samples;
    }

    auto meanOfSamplesMs(const std::vector<double>& samples) -> double
    {
        if (samples.empty())
        {
            return -1.0;
        }

        double total_ms = 0.0;
        for (const double sample : samples)
        {
            total_ms += sample;
        }
        return total_ms / static_cast<double>(samples.size());
    }

    auto percentileOfSamplesMs(std::vector<double> samples,
                               double percentile) -> double
    {
        if (samples.empty())
        {
            return -1.0;
        }

        percentile = std::clamp(percentile, 0.0, 1.0);
        std::sort(samples.begin(), samples.end());
        const double scaled_index =
            percentile * static_cast<double>(samples.size() - 1);
        const size_t lower_index = static_cast<size_t>(std::floor(scaled_index));
        const size_t upper_index = static_cast<size_t>(std::ceil(scaled_index));
        if (lower_index == upper_index)
        {
            return samples[lower_index];
        }

        const double fraction =
            scaled_index - static_cast<double>(lower_index);
        return samples[lower_index] +
               ((samples[upper_index] - samples[lower_index]) * fraction);
    }

    void enableParallelPlanning(const std::string& setup_cost = "0",
                                const std::string& tuple_cost = "0")
    {
        ASSERT_NE(connection_ctx_, nullptr);
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL", "ON");
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL_SCAN", "ON");
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL_HASH", "ON");
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL_AGGREGATE", "ON");
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL_JOIN", "ON");
        connection_ctx_->setSessionVariable("PARALLEL_LEADER_PARTICIPATION", "ON");
        connection_ctx_->setSessionVariable("MAX_PARALLEL_WORKERS", "4");
        connection_ctx_->setSessionVariable("MAX_PARALLEL_WORKERS_PER_GATHER", "4");
        connection_ctx_->setSessionVariable("MIN_PARALLEL_ROWS_PER_WORKER", "1");
        connection_ctx_->setSessionVariable("MIN_PARALLEL_TABLE_SCAN_SIZE", "1");
        connection_ctx_->setSessionVariable("PARALLEL_SETUP_COST", setup_cost);
        connection_ctx_->setSessionVariable("PARALLEL_TUPLE_COST", tuple_cost);
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
    std::string last_compile_errors_;
};

// ===== Basic Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, QueryCompilerV3ProducesBytecode)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM users");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_TABLE_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_COLUMN_REF));
}

TEST_F(QueryPlannerIntegrationTest, DatabaseHasQueryPlannerComponents)
{
    ASSERT_TRUE(createDatabase());

    EXPECT_NE(db_->statistics_manager(), nullptr);
    scratchbird::optimizer::QueryPlanner planner(
        db_.get(),
        scratchbird::optimizer::CostModel(),
        db_->statistics_manager());
    (void)planner;
}

TEST_F(QueryPlannerIntegrationTest, CompilerRequiresCatalog)
{
    QueryCompilerV3 compiler(nullptr);
    auto result = compiler.compile("SELECT 1");
    EXPECT_FALSE(result.success());
}

// ===== Optimizer Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, SelectGeneratesWithPlanner)
{
    ASSERT_TRUE(createDatabase());

    auto select_bytecode = compileSQL("SELECT * FROM users");
    EXPECT_FALSE(select_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(select_bytecode, sblr_v3::Opcode::SBLR3_SELECT));
}

TEST_F(QueryPlannerIntegrationTest, OptimizedSelectEmbedsRuntimePlanPayload)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id > 10");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_FALSE(plan.plan_hash.empty());
    EXPECT_FALSE(plan.explain_text.empty());
    EXPECT_EQ(plan.cache_mode, "GENERIC");
    EXPECT_FALSE(plan.plan_profile_signature.empty());
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().table_path, "users");
    EXPECT_FALSE(plan.relations.front().scan_kind.empty());
    EXPECT_FALSE(plan.root.node_type.empty());
}

TEST_F(QueryPlannerIntegrationTest, DisabledOptimizationsDoNotEmbedRuntimePlanPayload)
{
    ASSERT_TRUE(createDatabase());

    compiler_->setOptimizationsEnabled(false);
    auto bytecode = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    EXPECT_FALSE(decodeRuntimePlan(bytecode, plan));
}

TEST_F(QueryPlannerIntegrationTest, RepeatedSelectHitsPlanCache)
{
    ASSERT_TRUE(createDatabase());

    QueryCompilerV3::resetPlanCacheStats();
    const auto baseline = QueryCompilerV3::planCacheStats();

    auto first = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(first.empty()) << last_compile_errors_;
    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);
    EXPECT_EQ(after_first.entries, baseline.entries + 1u);

    auto second = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(second.empty()) << last_compile_errors_;
    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 1u);
    EXPECT_EQ(after_second.misses, 1u);
    EXPECT_EQ(after_second.inserts, 1u);
    EXPECT_EQ(after_second.entries, baseline.entries + 1u);
    EXPECT_EQ(first, second);
}

TEST_F(QueryPlannerIntegrationTest, SchemaMutationInvalidatesCachedPlansLocally)
{
    ASSERT_TRUE(createDatabase());

    QueryCompilerV3::resetPlanCacheStats();
    const auto baseline = QueryCompilerV3::planCacheStats();

    auto first = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(first.empty()) << last_compile_errors_;
    auto second = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(second.empty()) << last_compile_errors_;
    auto mutation = compileSQL("CREATE INDEX idx_users_age ON users (age)");
    ASSERT_FALSE(mutation.empty()) << last_compile_errors_;

    auto after_mutation = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_mutation.hits, 1u);
    EXPECT_EQ(after_mutation.misses, 1u);
    EXPECT_EQ(after_mutation.inserts, 1u);
    EXPECT_EQ(after_mutation.invalidations, 1u);
    EXPECT_EQ(after_mutation.entries, baseline.entries);
}

TEST_F(QueryPlannerIntegrationTest, DmlDoesNotInvalidateCachedPlans)
{
    ASSERT_TRUE(createDatabase());

    QueryCompilerV3::resetPlanCacheStats();
    const auto baseline = QueryCompilerV3::planCacheStats();

    auto first = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(first.empty()) << last_compile_errors_;
    auto second = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(second.empty()) << last_compile_errors_;
    auto dml = compileSQL("INSERT INTO users (id, name) VALUES (1, 'alice')");
    ASSERT_FALSE(dml.empty()) << last_compile_errors_;
    auto third = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(third.empty()) << last_compile_errors_;

    auto after_dml = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_dml.hits, 2u);
    EXPECT_EQ(after_dml.misses, 1u);
    EXPECT_EQ(after_dml.inserts, 1u);
    EXPECT_EQ(after_dml.invalidations, 0u);
    EXPECT_EQ(after_dml.entries, baseline.entries + 1u);
}

TEST_F(QueryPlannerIntegrationTest, ParameterizedSelectUsesCustomPlanProfileAndHitsBucketedCache)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "5"});

    QueryCompilerV3::resetPlanCacheStats();

    auto first = compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(first.success()) << last_compile_errors_;
    EXPECT_EQ(first.planProfile().mode,
              scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::CUSTOM);
    EXPECT_TRUE(first.planProfile().parameter_sensitive);
    EXPECT_NE(first.planProfile().signature.find("CUSTOM:"), std::string::npos);
    scratchbird::optimizer::RuntimePlan custom_plan;
    ASSERT_TRUE(decodeRuntimePlan(first.bytecode(), custom_plan));
    EXPECT_EQ(custom_plan.cache_mode, "CUSTOM");
    EXPECT_TRUE(custom_plan.parameter_sensitive);
    EXPECT_EQ(custom_plan.plan_profile_signature, first.planProfile().signature);
    EXPECT_FALSE(custom_plan.considered_paths.empty());
    EXPECT_FALSE(custom_plan.statistics_provenance.empty());

    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);

    auto second = compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(second.success()) << last_compile_errors_;
    EXPECT_EQ(first.planProfile().signature, second.planProfile().signature);

    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 1u);
    EXPECT_EQ(after_second.misses, 1u);
    EXPECT_EQ(after_second.inserts, 1u);
}

TEST_F(QueryPlannerIntegrationTest, ParameterizedRangeBucketsChangeWithBindingSelectivity)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    optimizer::ParameterBindings selective;
    selective.positional.push_back({false, "5"});
    optimizer::ParameterBindings broad;
    broad.positional.push_back({false, "250"});

    auto selective_compile =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", selective);
    ASSERT_TRUE(selective_compile.success()) << last_compile_errors_;
    auto broad_compile =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", broad);
    ASSERT_TRUE(broad_compile.success()) << last_compile_errors_;

    EXPECT_NE(selective_compile.planProfile().selectivity_bucket_signature,
              broad_compile.planProfile().selectivity_bucket_signature);
    EXPECT_NE(selective_compile.planProfile().signature,
              broad_compile.planProfile().signature);

    scratchbird::optimizer::RuntimePlan selective_plan;
    scratchbird::optimizer::RuntimePlan broad_plan;
    ASSERT_TRUE(decodeRuntimePlan(selective_compile.bytecode(), selective_plan));
    ASSERT_TRUE(decodeRuntimePlan(broad_compile.bytecode(), broad_plan));
    ASSERT_FALSE(selective_plan.relations.empty());
    ASSERT_FALSE(broad_plan.relations.empty());
    EXPECT_EQ(selective_plan.cache_mode, "CUSTOM");
    EXPECT_TRUE(selective_plan.parameter_sensitive);
    EXPECT_EQ(selective_plan.selectivity_bucket_signature,
              selective_compile.planProfile().selectivity_bucket_signature);
    EXPECT_FALSE(selective_plan.considered_paths.empty());
    EXPECT_FALSE(selective_plan.statistics_provenance.empty());
    EXPECT_LT(selective_plan.relations.front().estimated_rows,
              broad_plan.relations.front().estimated_rows);
}

TEST_F(QueryPlannerIntegrationTest, PlanProfileDirectiveForcesGenericParameterizedPlan)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES",
                                        "PLAN_PROFILE=GENERIC");

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "5"});

    auto compile_result =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(compile_result.success()) << last_compile_errors_;
    EXPECT_EQ(compile_result.planProfile().mode,
              scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::GENERIC);
    EXPECT_FALSE(compile_result.planProfile().parameter_sensitive);

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(compile_result.bytecode(), plan));
    EXPECT_EQ(plan.cache_mode, "GENERIC");
    EXPECT_FALSE(plan.parameter_sensitive);
    const auto* plan_profile = findOptimizerControl(plan, "PLAN_PROFILE");
    ASSERT_NE(plan_profile, nullptr);
    EXPECT_EQ(plan_profile->value, "GENERIC");
    EXPECT_EQ(plan_profile->source, "DIRECTIVE");
}

TEST_F(QueryPlannerIntegrationTest, AutoPlanProfileUsesChooserAndPublishesReuseMetadata)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES",
                                        "PLAN_PROFILE=AUTO");
    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "5"});

    QueryCompilerV3::resetPlanCacheStats();

    auto first =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(first.success()) << last_compile_errors_;
    EXPECT_EQ(first.planProfile().decision_source, "CHOOSER");
    EXPECT_FALSE(first.planProfile().statistics_snapshot_signature.empty());
    EXPECT_FALSE(first.planProfile().cost_profile_id.empty());
    EXPECT_FALSE(first.planProfile().policy_snapshot_id.empty());
    EXPECT_FALSE(first.planProfile().index_family_signature.empty());
    EXPECT_FALSE(first.planProfile().family_statistics_signature.empty());

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(first.bytecode(), plan));
    EXPECT_EQ(plan.cache_mode,
              first.planProfile().mode ==
                      scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::CUSTOM
                  ? "CUSTOM"
                  : "GENERIC");
    EXPECT_EQ(plan.index_family_signature,
              first.planProfile().index_family_signature);
    EXPECT_EQ(plan.family_statistics_signature,
              first.planProfile().family_statistics_signature);
    const auto *plan_profile = findOptimizerControl(plan, "PLAN_PROFILE");
    ASSERT_NE(plan_profile, nullptr);
    EXPECT_EQ(plan_profile->value, "AUTO");
    EXPECT_EQ(plan_profile->source, "DIRECTIVE");
    const auto *reuse_decision = findOptimizerControl(plan, "PLAN_REUSE_DECISION");
    ASSERT_NE(reuse_decision, nullptr);
    EXPECT_EQ(reuse_decision->value, plan.cache_mode);
    EXPECT_EQ(reuse_decision->source, "CHOOSER");
    const auto *stats_snapshot = findOptimizerControl(plan, "PLAN_STATS_SNAPSHOT");
    ASSERT_NE(stats_snapshot, nullptr);
    EXPECT_EQ(stats_snapshot->value,
              first.planProfile().statistics_snapshot_signature);
    const auto *cost_profile = findOptimizerControl(plan, "PLAN_COST_PROFILE");
    ASSERT_NE(cost_profile, nullptr);
    EXPECT_EQ(cost_profile->value, first.planProfile().cost_profile_id);
    const auto *policy_snapshot = findOptimizerControl(plan, "PLAN_POLICY_SNAPSHOT");
    ASSERT_NE(policy_snapshot, nullptr);
    EXPECT_EQ(policy_snapshot->value, first.planProfile().policy_snapshot_id);
    const auto *grant_policy_snapshot =
        findOptimizerControl(plan, "PLAN_GRANT_POLICY_SNAPSHOT");
    ASSERT_NE(grant_policy_snapshot, nullptr);
    EXPECT_EQ(grant_policy_snapshot->value, connection_ctx_->dialect_tag());
    const auto *memory_feedback_snapshot =
        findOptimizerControl(plan, "PLAN_MEMORY_FEEDBACK_SNAPSHOT");
    ASSERT_NE(memory_feedback_snapshot, nullptr);
    EXPECT_NE(memory_feedback_snapshot->value.find("mgf:"),
              std::string::npos);
    const auto *family_identity = findOptimizerControl(plan, "PLAN_FAMILY_IDENTITY");
    ASSERT_NE(family_identity, nullptr);
    EXPECT_EQ(family_identity->value, first.planProfile().index_family_signature);
    const auto *family_stats = findOptimizerControl(plan, "PLAN_FAMILY_STATS");
    ASSERT_NE(family_stats, nullptr);
    EXPECT_EQ(family_stats->value,
              first.planProfile().family_statistics_signature);

    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);

    auto second =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(second.success()) << last_compile_errors_;
    EXPECT_EQ(second.planProfile().decision_source, "CHOOSER");
    EXPECT_EQ(second.planProfile().signature, first.planProfile().signature);
    EXPECT_EQ(second.planProfile().statistics_snapshot_signature,
              first.planProfile().statistics_snapshot_signature);
    EXPECT_EQ(second.planProfile().cost_profile_id,
              first.planProfile().cost_profile_id);
    EXPECT_EQ(second.planProfile().index_family_signature,
              first.planProfile().index_family_signature);
    EXPECT_EQ(second.planProfile().family_statistics_signature,
              first.planProfile().family_statistics_signature);

    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 1u);
    EXPECT_EQ(after_second.misses, 1u);
    EXPECT_EQ(after_second.inserts, 1u);
}

TEST_F(QueryPlannerIntegrationTest,
       MemoryGrantFeedbackSnapshotControlChangesWhenDurableFeedbackChanges)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_PROFILE", "GENERIC");

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'snapgrant" +
                               std::to_string(257 - i) + "', 'snapgrant" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT name FROM users WHERE age < $1 ORDER BY name";
    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "40"});

    auto baseline_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(baseline_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline_compile.bytecode(), baseline_plan));
    const auto* baseline_snapshot =
        findOptimizerControl(baseline_plan, "PLAN_MEMORY_FEEDBACK_SNAPSHOT");
    ASSERT_NE(baseline_snapshot, nullptr);
    EXPECT_NE(baseline_snapshot->value.find("mgf:"),
              std::string::npos);
    const auto* baseline_feedback =
        findOptimizerControl(baseline_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    EXPECT_EQ(baseline_feedback, nullptr);

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            24ULL * 1024ULL * 1024ULL,
                            12ULL * 1024ULL * 1024ULL,
                            18ULL * 1024ULL * 1024ULL,
                            18ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8,
                            0,
                            0,
                            0,
                            baseline_plan.cache_mode,
                            baseline_plan.execution_intent_class,
                            baseline_plan.storage_layer_shape,
                            connection_ctx_->dialect_tag());

    QueryCompilerV3::invalidateAllPlanCache();
    auto refreshed_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(refreshed_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan refreshed_plan;
    ASSERT_TRUE(
        decodeRuntimePlan(refreshed_compile.bytecode(), refreshed_plan));
    const auto* refreshed_snapshot =
        findOptimizerControl(refreshed_plan, "PLAN_MEMORY_FEEDBACK_SNAPSHOT");
    ASSERT_NE(refreshed_snapshot, nullptr);
    EXPECT_NE(refreshed_snapshot->value.find("mgf:"),
              std::string::npos);
    EXPECT_NE(refreshed_snapshot->value, baseline_snapshot->value);
    const auto* refreshed_feedback =
        findOptimizerControl(refreshed_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    ASSERT_NE(refreshed_feedback, nullptr);
    EXPECT_EQ(refreshed_feedback->source, "CATALOG");
    EXPECT_EQ(std::stoull(refreshed_feedback->value),
              24ULL * 1024ULL * 1024ULL);
}

TEST_F(QueryPlannerIntegrationTest,
       AutoPlanProfilePublishesPeakOperatorMemoryReservationForWrappedSort)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 4096; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'sortplan" +
                               std::to_string(4097 - i) + "', 'sortplan" +
                               std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 40)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES",
                                        "PLAN_PROFILE=AUTO");

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "45"});

    auto compile_result = compileSQLWithParameters(
        "SELECT name FROM users WHERE age < $1 ORDER BY name LIMIT 10",
        bindings);
    ASSERT_TRUE(compile_result.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(compile_result.bytecode(), plan));
    EXPECT_EQ(plan.root.node_type, "Limit");
    EXPECT_TRUE(runtimeNodeContainsType(plan.root, "Sort"));

    const auto* reservation =
        findOptimizerControl(plan, "PLAN_MEMORY_RESERVATION_BYTES");
    ASSERT_NE(reservation, nullptr);
    EXPECT_EQ(reservation->source, "CHOOSER");

    const auto peak_budget =
        [&]() -> uint64_t {
            std::function<uint64_t(const scratchbird::optimizer::RuntimePlanNode&)> visit =
                [&](const scratchbird::optimizer::RuntimePlanNode& node) -> uint64_t {
                    uint64_t peak = node.memory_budget_bytes;
                    for (const auto& child : node.children)
                    {
                        peak = std::max(peak, visit(child));
                    }
                    return peak;
                };
            return visit(plan.root);
        }();

    const uint64_t expected_reservation =
        std::max<uint64_t>(peak_budget, 4ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(std::stoull(reservation->value), expected_reservation);
}

TEST_F(QueryPlannerIntegrationTest,
       FamilyStatisticsSignatureBypassesReusablePlanCacheOnMetricsVersionChange)
{
    ASSERT_TRUE(createDatabase());

    constexpr int kRowCount = 4096;

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= kRowCount; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@x', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(
        executeSQL("ANALYZE INDEX users.idx_users_id WITH (sample_rate = 0.25)")
            .success());

    ErrorContext ctx;
    CatalogManager::TableInfo users_table{};
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               users_table,
                                               &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo users_index{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(users_table.table_id,
                                               "idx_users_id",
                                               users_index,
                                               &ctx),
              Status::OK)
        << ctx.message;

    auto publishFamilyMetrics =
        [&](uint32_t stats_version, uint32_t family_metrics_version) {
            const uint64_t refresh_xid =
                std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid());
            CatalogManager::IndexStatsCatalogInfo stats{};
            stats.index_id = users_index.index_id;
            stats.stats_version = stats_version;
            stats.last_analyze_txid = refresh_xid;
            stats.row_count_est = kRowCount;
            stats.distinct_count_est = kRowCount;
            stats.null_frac = 0.0f;
            stats.avg_key_len = 8;
            stats.avg_entry_len = 16;
            stats.leaf_pages = 1;
            stats.height = 1;
            stats.correlation = 1.0f;
            stats.bloat_ratio = 0.0f;
            stats.metrics_last_refresh_xid = refresh_xid;
            stats.family_metrics_version = family_metrics_version;
            stats.family_metrics_type =
                scratchbird::optimizer::IndexFamilyMetricsType::ORDERED_EXACT;
            stats.metrics_confidence_class =
                scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
            stats.queryability_state =
                scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE;
            stats.is_valid = true;
            stats.family_metrics_payload =
                nlohmann::json{
                    {"shared_metrics_envelope",
                     {{"index_uuid", users_index.index_id.toString()},
                      {"physical_family", "BTREE"},
                      {"planner_family", "BTREE_EQ_SCAN"},
                      {"queryability_state", "QUERYABLE"},
                      {"metrics_last_refresh_xid", refresh_xid},
                      {"metrics_confidence_class", "HIGH"},
                      {"leaf_pages", 1},
                      {"height", 1},
                      {"row_count_est", kRowCount},
                      {"live_entry_count_est", kRowCount},
                      {"dead_fraction", 0.0},
                      {"bloat_ratio", 0.0},
                      {"recheck_ratio_est", 0.0},
                      {"correlation", 1.0},
                      {"coverage_fraction", 1.0},
                      {"maintenance_backlog_ops", 0},
                      {"publish_lag_xids", 0},
                      {"reclaim_lag_xids", 0}}},
                    {"family_metrics_type", "ORDERED_EXACT"},
                    {"family_metrics",
                     {{"avg_probe_pages", 1.0},
                      {"avg_range_pages_per_row",
                       1.0 / static_cast<double>(kRowCount)},
                      {"duplicate_density", 0.0},
                      {"prefix_selectivity",
                       1.0 / static_cast<double>(kRowCount)},
                      {"skip_group_count", kRowCount},
                      {"overflow_chain_depth", 0},
                      {"run_count", 0},
                      {"level_count", 0},
                      {"tombstone_fraction", 0.0},
                      {"L0_run_count", 0}}}}
                    .dump();
            ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats, &ctx),
                      Status::OK)
                << ctx.message;
            db_->statistics_manager()->invalidateCache(users_table.table_id);
        };

    publishFamilyMetrics(31, 7);

    QueryCompilerV3::resetPlanCacheStats();

    auto first = compiler_->compile("SELECT id FROM users WHERE id = 2048");
    ASSERT_TRUE(first.success());
    scratchbird::optimizer::RuntimePlan first_plan;
    ASSERT_TRUE(decodeRuntimePlan(first.bytecode(), first_plan));
    ASSERT_EQ(first_plan.relations.size(), 1u);
    const auto &first_relation = first_plan.relations.front();
    EXPECT_NE(std::find(first_relation.candidate_scan_families.begin(),
                        first_relation.candidate_scan_families.end(),
                        "BTREE_EQ_SCAN"),
              first_relation.candidate_scan_families.end());
    auto containsSignatureFragment =
        [](const std::vector<std::string> &entries,
           const std::string &fragment) -> bool {
            return std::any_of(entries.begin(),
                               entries.end(),
                               [&](const std::string &entry) {
                                   return entry.find(fragment) !=
                                          std::string::npos;
                               });
        };
    EXPECT_TRUE(containsSignatureFragment(
        first_relation.candidate_family_statistics_signatures,
        ":BTREE_EQ_SCAN:7:"));
    EXPECT_FALSE(first.planProfile().index_family_signature.empty());
    EXPECT_FALSE(first.planProfile().family_statistics_signature.empty());
    EXPECT_NE(first.planProfile().index_family_signature.find("BTREE_EQ_SCAN"),
              std::string::npos);
    EXPECT_NE(first.planProfile().family_statistics_signature.find(
                  ":BTREE_EQ_SCAN:7:"),
              std::string::npos);

    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);

    publishFamilyMetrics(31, 8);

    auto second = compiler_->compile("SELECT id FROM users WHERE id = 2048");
    ASSERT_TRUE(second.success());
    scratchbird::optimizer::RuntimePlan second_plan;
    ASSERT_TRUE(decodeRuntimePlan(second.bytecode(), second_plan));
    ASSERT_EQ(second_plan.relations.size(), 1u);
    const auto &second_relation = second_plan.relations.front();
    EXPECT_NE(std::find(second_relation.candidate_scan_families.begin(),
                        second_relation.candidate_scan_families.end(),
                        "BTREE_EQ_SCAN"),
              second_relation.candidate_scan_families.end());
    EXPECT_TRUE(containsSignatureFragment(
        second_relation.candidate_family_statistics_signatures,
        ":BTREE_EQ_SCAN:8:"));

    EXPECT_EQ(second.planProfile().statistics_snapshot_signature,
              first.planProfile().statistics_snapshot_signature);
    EXPECT_NE(second.planProfile().cost_profile_id,
              first.planProfile().cost_profile_id);
    EXPECT_EQ(second.planProfile().index_family_signature,
              first.planProfile().index_family_signature);
    EXPECT_NE(second.planProfile().family_statistics_signature,
              first.planProfile().family_statistics_signature);
    EXPECT_EQ(second_plan.index_family_signature,
              first.planProfile().index_family_signature);
    EXPECT_EQ(second_plan.family_statistics_signature,
              second.planProfile().family_statistics_signature);
    EXPECT_NE(second.planProfile().family_statistics_signature.find(
                  ":BTREE_EQ_SCAN:8:"),
              std::string::npos);

    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 0u);
    EXPECT_EQ(after_second.misses, 2u);
    EXPECT_EQ(after_second.inserts, 2u);
}

TEST_F(QueryPlannerIntegrationTest, CardinalityFeedbackBypassesStaleCacheAndRebuildsPlan)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    QueryCompilerV3::resetPlanCacheStats();

    const std::string sql = "SELECT id FROM users WHERE id > 0";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    ASSERT_GT(stale_plan.root.estimated_rows, 0u);

    for (int i = 5; i <= 200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', 30)")
                        .success());
    }

    auto stale_result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(stale_result.success()) << stale_result.error();
    ASSERT_TRUE(stale_result.hasResultSet());
    ASSERT_EQ(stale_result.resultSet()->rowCount(), 200u);

    const auto feedback_key =
        std::to_string(sblr_v3::stableHash64(
            optimizer::QueryProfiler::getInstance().fingerprintQuery(sql)));
    auto feedback =
        optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(feedback_key);
    ASSERT_TRUE(feedback.has_value());
    EXPECT_TRUE(feedback->available);
    EXPECT_TRUE(feedback->replan_required);
    EXPECT_EQ(feedback->last_estimated_rows, stale_plan.root.estimated_rows);
    EXPECT_EQ(feedback->last_actual_rows, 200u);

    auto refreshed_bytecode = compileSQL(sql);
    ASSERT_FALSE(refreshed_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan refreshed_plan;
    ASSERT_TRUE(decodeRuntimePlan(refreshed_bytecode, refreshed_plan));
    EXPECT_TRUE(refreshed_plan.adaptive_feedback.available);
    EXPECT_FALSE(refreshed_plan.adaptive_feedback.replan_required);
    EXPECT_FALSE(refreshed_plan.adaptive_feedback.replan_suppressed);
    EXPECT_TRUE(refreshed_plan.adaptive_feedback.stats_refresh_applied);
    EXPECT_EQ(refreshed_plan.adaptive_feedback.observation_count, 1u);
    EXPECT_EQ(refreshed_plan.adaptive_feedback.last_actual_rows, 200u);
    EXPECT_NE(refreshed_plan.root.estimated_rows, stale_plan.root.estimated_rows);
    EXPECT_LT(normalizedMisestimateRatio(refreshed_plan.root.estimated_rows, 200u),
              normalizedMisestimateRatio(stale_plan.root.estimated_rows, 200u));

    auto adaptive_it = std::find_if(
        refreshed_plan.statistics_provenance.begin(),
        refreshed_plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance& entry) {
            return entry.source == "ADAPTIVE_CARDINALITY_FEEDBACK";
        });
    EXPECT_NE(adaptive_it, refreshed_plan.statistics_provenance.end());

    const auto cache_stats = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(cache_stats.hits, 0u);
    EXPECT_EQ(cache_stats.misses, 1u);
    EXPECT_EQ(cache_stats.inserts, 2u);
    EXPECT_GE(cache_stats.invalidations, 1u);
}

TEST_F(QueryPlannerIntegrationTest,
       AdaptiveFeedbackCachedPlanReflectsLatestFeedbackStateAfterRepeatExecution)
{
    ASSERT_TRUE(createDatabase());

    const std::vector<std::string> setup_sql = {
        "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)",
        "INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 31)",
        "INSERT INTO users (id, name, email, age) VALUES (3, 'carol', 'c@example.com', 32)",
        "INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)",
        "INSERT INTO products (id, name, price) VALUES (2, 'p2', 11.5)",
        "INSERT INTO test (id) VALUES (10)",
        "INSERT INTO test (id) VALUES (20)",
        "CREATE INDEX idx_users_id ON users (id)",
        "CREATE INDEX idx_products_id ON products (id)"
    };

    for (const auto& sql : setup_sql)
    {
        ASSERT_TRUE(executeSQL(sql).success()) << sql;
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());
    ASSERT_TRUE(executeSQL("ANALYZE test").success());

    QueryCompilerV3::resetPlanCacheStats();

    const std::string sql =
        "SELECT users.id FROM users CROSS JOIN test JOIN products ON users.id = products.id";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));

    auto stale_result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(stale_result.success()) << stale_result.error();
    ASSERT_TRUE(stale_result.hasResultSet());
    ASSERT_EQ(stale_result.resultSet()->rowCount(), 4u);

    const auto feedback_key =
        std::to_string(sblr_v3::stableHash64(
            optimizer::QueryProfiler::getInstance().fingerprintQuery(sql)));
    auto first_feedback =
        optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(feedback_key);
    ASSERT_TRUE(first_feedback.has_value());
    ASSERT_TRUE(first_feedback->replan_required);
    const std::string first_feedback_plan_hash = first_feedback->last_plan_hash;

    auto refreshed_bytecode = compileSQL(sql);
    ASSERT_FALSE(refreshed_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan refreshed_plan;
    ASSERT_TRUE(decodeRuntimePlan(refreshed_bytecode, refreshed_plan));
    EXPECT_TRUE(refreshed_plan.adaptive_feedback.available);
    EXPECT_TRUE(refreshed_plan.adaptive_feedback.correction_applied);

    auto refreshed_result = executeBytecode(refreshed_bytecode);
    ASSERT_TRUE(refreshed_result.success()) << refreshed_result.error();
    ASSERT_TRUE(refreshed_result.hasResultSet());
    ASSERT_EQ(refreshed_result.resultSet()->rowCount(), 4u);

    auto guarded_feedback =
        optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(feedback_key);
    ASSERT_TRUE(guarded_feedback.has_value());
    EXPECT_FALSE(guarded_feedback->replan_required);
    EXPECT_EQ(guarded_feedback->observation_count, 2u);
    EXPECT_EQ(guarded_feedback->replan_action_count, 1u);
    EXPECT_EQ(guarded_feedback->last_plan_hash, first_feedback_plan_hash);
    if (guarded_feedback->replan_suppressed)
    {
        EXPECT_EQ(guarded_feedback->guardrail_reason,
                  "SAME_PLAN_HASH_REPLAN_LIMIT");
    }
    else
    {
        EXPECT_TRUE(guarded_feedback->guardrail_reason.empty());
    }

    const auto before_cached_compile = QueryCompilerV3::planCacheStats();
    auto cached_bytecode = compileSQL(sql);
    ASSERT_FALSE(cached_bytecode.empty()) << last_compile_errors_;
    const auto after_cached_compile = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_cached_compile.hits, before_cached_compile.hits + 1u);
    EXPECT_EQ(after_cached_compile.invalidations,
              before_cached_compile.invalidations);

    scratchbird::optimizer::RuntimePlan cached_plan;
    ASSERT_TRUE(decodeRuntimePlan(cached_bytecode, cached_plan));
    EXPECT_EQ(cached_plan.adaptive_feedback.replan_suppressed,
              guarded_feedback->replan_suppressed);
    EXPECT_EQ(cached_plan.adaptive_feedback.guardrail_reason,
              guarded_feedback->guardrail_reason);
    EXPECT_EQ(cached_plan.adaptive_feedback.observation_count,
              guarded_feedback->observation_count);
    EXPECT_EQ(cached_plan.adaptive_feedback.replan_action_count,
              guarded_feedback->replan_action_count);
    EXPECT_EQ(cached_plan.adaptive_feedback.last_plan_hash,
              guarded_feedback->last_plan_hash);
}

TEST_F(QueryPlannerIntegrationTest, EqualityJoinChoosesHashJoinPlan)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "INNER");
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");
    EXPECT_TRUE(plan.join_steps.front().has_hash_keys);
    EXPECT_EQ(plan.join_steps.front().left_hash_key.column_name, "id");
    EXPECT_EQ(plan.join_steps.front().right_hash_key.column_name, "id");
}

TEST_F(QueryPlannerIntegrationTest, FullOuterJoinFailsClosedToNestedLoop)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users FULL JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "FULL");
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");
    EXPECT_FALSE(plan.join_steps.front().has_hash_keys);
}

TEST_F(QueryPlannerIntegrationTest,
       RuntimePlanCarriesVersionedJoinGraphAndSearchSummaryContracts)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users LEFT JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.version, scratchbird::optimizer::kRuntimePlanPayloadVersion);
    EXPECT_EQ(plan.contract_id, scratchbird::optimizer::kRuntimePlanContractId);
    EXPECT_EQ(plan.join_graph_contract_id,
              scratchbird::optimizer::kJoinGraphContractId);
    EXPECT_EQ(plan.diagnostics_contract_id,
              scratchbird::optimizer::kOptimizerDiagnosticsContractId);
    EXPECT_FALSE(plan.search_summary.requested_strategy.empty());
    EXPECT_FALSE(plan.search_summary.selected_strategy.empty());
    EXPECT_GE(plan.search_summary.considered_state_count, 1u);
    EXPECT_EQ(plan.search_summary.rejected_candidate_count,
              plan.rejected_paths.size());

    ASSERT_EQ(plan.join_steps.size(), 1u);
    const auto& step = plan.join_steps.front();
    EXPECT_EQ(step.join_edge_left_relation_index, 0u);
    EXPECT_EQ(step.join_edge_right_relation_index, 1u);
    EXPECT_EQ(step.join_edge_left_alias, "users");
    EXPECT_EQ(step.join_edge_right_alias, "products");
    EXPECT_FALSE(step.join_edge_left_id_text.empty());
    EXPECT_FALSE(step.join_edge_right_id_text.empty());
    EXPECT_FALSE(step.legal_method_families.empty());
    EXPECT_TRUE(step.outer_reorder_barrier);
    EXPECT_FALSE(step.using_reorder_barrier);
    EXPECT_FALSE(step.natural_reorder_barrier);
    EXPECT_FALSE(step.lateral_reorder_barrier);
    EXPECT_FALSE(step.parameterized_dependency);
    ASSERT_EQ(step.equijoin_keys.size(), 1u);
    EXPECT_EQ(step.equijoin_keys.front().left_column_name, "id");
    EXPECT_EQ(step.equijoin_keys.front().right_column_name, "id");
    EXPECT_TRUE(step.residual_predicates.empty());
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonPublishesStatsHealthAndStalePenalty)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE TABLE planner_stats_users (id INTEGER, age INTEGER)").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON planner_stats_users TO PUBLIC").success());

    for (int i = 1; i <= 1000; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO planner_stats_users (id, age) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(20 + (i % 10)) + ")")
                        .success());
    }
    ErrorContext commit_ctx;
    ASSERT_EQ(connection_ctx_->commit(&commit_ctx), Status::OK)
        << commit_ctx.message;
    ASSERT_TRUE(executeSQL("ANALYZE planner_stats_users").success());
    ErrorContext analyze_commit_ctx;
    ASSERT_EQ(connection_ctx_->commit(&analyze_commit_ctx), Status::OK)
        << analyze_commit_ctx.message;

    auto fresh_bytecode =
        compileSQL("SELECT id FROM planner_stats_users WHERE age = 25");
    ASSERT_FALSE(fresh_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan fresh_plan;
    ASSERT_TRUE(decodeRuntimePlan(fresh_bytecode, fresh_plan));
    auto initial_freshness_trace = std::find_if(
        fresh_plan.considered_paths.begin(),
        fresh_plan.considered_paths.end(),
        [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
            return entry.phase == "STATS_FRESHNESS";
        });
    EXPECT_EQ(initial_freshness_trace, fresh_plan.considered_paths.end());

    for (int i = 1001; i <= 1100; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO planner_stats_users (id, age) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(20 + (i % 10)) + ")")
                        .success());
    }
    ErrorContext stale_commit_ctx;
    ASSERT_EQ(connection_ctx_->commit(&stale_commit_ctx), Status::OK)
        << stale_commit_ctx.message;

    auto stale_bytecode =
        compileSQL("SELECT id FROM planner_stats_users WHERE age = 26");
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));

    auto relation_stats_it = std::find_if(
        stale_plan.statistics_provenance.begin(),
        stale_plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance &entry) {
            const bool source_is_column_stats =
                entry.source.find("MCV") != std::string::npos ||
                entry.source.find("NDISTINCT") != std::string::npos ||
                entry.source.find("COLUMN_STATS") != std::string::npos;
            return entry.subject == "relation:planner_stats_users" &&
                   source_is_column_stats &&
                   entry.detail.find("age") != std::string::npos;
        });
    ASSERT_NE(relation_stats_it, stale_plan.statistics_provenance.end());
    EXPECT_GT(relation_stats_it->stats_snapshot_id, 0u);
    EXPECT_GT(relation_stats_it->last_analyzed_time, 0u);
    EXPECT_GT(relation_stats_it->sample_ratio, 0.0);
    EXPECT_FALSE(relation_stats_it->staleness_class.empty());
    EXPECT_FALSE(relation_stats_it->confidence_class.empty());
    EXPECT_GT(relation_stats_it->auto_analyze_threshold, 0u);

    auto explain_result = executeSQL(
        "EXPLAIN (FORMAT JSON) SELECT id FROM planner_stats_users WHERE age = 26");
    ASSERT_TRUE(explain_result.success()) << explain_result.error();
    ASSERT_TRUE(explain_result.hasResultSet());

    const auto explain_lines = resultStrings(explain_result);
    ASSERT_EQ(explain_lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(explain_lines.front());
    ASSERT_TRUE(parsed.contains("optimizer_trace"));
    const auto &stats_array = parsed["optimizer_trace"]["statistics_provenance"];
    auto explain_stats_it = std::find_if(
        stats_array.begin(),
        stats_array.end(),
        [](const nlohmann::json &entry) {
            const std::string source = entry.value("source", std::string());
            const bool source_is_column_stats =
                source.find("MCV") != std::string::npos ||
                source.find("NDISTINCT") != std::string::npos ||
                source.find("COLUMN_STATS") != std::string::npos;
            return entry.value("subject", std::string()) ==
                       "relation:planner_stats_users" &&
                   source_is_column_stats &&
                   entry.value("detail", std::string()).find("age") != std::string::npos;
        });
    ASSERT_NE(explain_stats_it, stats_array.end());
    EXPECT_EQ(explain_stats_it->value("stats_snapshot_id", 0ULL),
              relation_stats_it->stats_snapshot_id);
    EXPECT_EQ(explain_stats_it->value("last_analyzed_time", 0ULL),
              relation_stats_it->last_analyzed_time);
    EXPECT_DOUBLE_EQ(explain_stats_it->value("sample_ratio", 0.0),
                     relation_stats_it->sample_ratio);
    EXPECT_EQ(explain_stats_it->value("modified_rows_since_analyze", 0ULL),
              relation_stats_it->modified_rows_since_analyze);
    EXPECT_EQ(explain_stats_it->value("staleness_class", std::string()),
              relation_stats_it->staleness_class);
    EXPECT_EQ(explain_stats_it->value("confidence_class", std::string()),
              relation_stats_it->confidence_class);
    EXPECT_EQ(explain_stats_it->value("auto_analyze_threshold", 0ULL),
              relation_stats_it->auto_analyze_threshold);
}

TEST_F(QueryPlannerIntegrationTest, LeftJoinCarriesFormalLegalityMetadata)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users LEFT JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "LEFT");
    EXPECT_EQ(plan.join_steps.front().legality_class, "LEFT_OUTER_BARRIER");
    EXPECT_EQ(plan.join_steps.front().legal_method_families.size(), 3u);
    EXPECT_EQ(plan.join_steps.front().legal_method_families.front(),
              "NESTED_LOOP");
    EXPECT_EQ(plan.join_steps.front().legal_method_families[1], "HASH_JOIN");
    EXPECT_EQ(plan.join_steps.front().legal_method_families[2], "MERGE_JOIN");
    EXPECT_FALSE(plan.join_steps.front().reorderable);
    EXPECT_TRUE(plan.join_steps.front().preserves_left_rows);
    EXPECT_FALSE(plan.join_steps.front().preserves_right_rows);
    EXPECT_FALSE(plan.join_steps.front().null_introduces_left);
    EXPECT_TRUE(plan.join_steps.front().null_introduces_right);
    EXPECT_TRUE(plan.join_steps.front().requires_original_order);
    EXPECT_TRUE(plan.join_steps.front().outer_reorder_barrier);
    EXPECT_FALSE(plan.join_steps.front().using_reorder_barrier);
    EXPECT_FALSE(plan.join_steps.front().natural_reorder_barrier);
}

TEST_F(QueryPlannerIntegrationTest,
       CanonicalJoinBackendOwnsMultiRelationSearchStrategy)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 2u);
    EXPECT_EQ(plan.search_summary.requested_strategy, "AUTO");
    EXPECT_EQ(plan.search_summary.selected_strategy, "EXHAUSTIVE_DP");
    EXPECT_TRUE(plan.search_summary.fallback_reason.empty());
    EXPECT_GE(plan.search_summary.considered_state_count, 2u);
}

TEST_F(QueryPlannerIntegrationTest, AutoMediumJoinGraphSelectsBoundedDp)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.EXHAUSTIVE_JOIN_LIMIT", "2");
    connection_ctx_->setSessionVariable("OPTIMIZER.BOUNDED_DP_JOIN_LIMIT", "3");
    connection_ctx_->setSessionVariable("OPTIMIZER.MAX_STATES_CONSIDERED", "32");
    connection_ctx_->setSessionVariable("OPTIMIZER.SEARCH_DEPTH", "32");

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.search_summary.requested_strategy, "AUTO");
    EXPECT_EQ(plan.search_summary.selected_strategy, "BOUNDED_DP");
    EXPECT_EQ(plan.search_summary.exhaustive_join_limit, 2u);
    EXPECT_EQ(plan.search_summary.bounded_dp_join_limit, 3u);
    EXPECT_EQ(plan.search_summary.max_states_considered, 32u);
    EXPECT_EQ(plan.search_summary.max_pair_evaluations, 32u);
    EXPECT_TRUE(plan.search_summary.fallback_reason.empty());
}

TEST_F(QueryPlannerIntegrationTest, SearchBudgetFallbackTelemetryPublishesThreshold)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_SEARCH", "BOUNDED_DP");
    connection_ctx_->setSessionVariable("OPTIMIZER.EXHAUSTIVE_JOIN_LIMIT", "2");
    connection_ctx_->setSessionVariable("OPTIMIZER.BOUNDED_DP_JOIN_LIMIT", "4");
    connection_ctx_->setSessionVariable("OPTIMIZER.MAX_STATES_CONSIDERED", "1");
    connection_ctx_->setSessionVariable("OPTIMIZER.FALLBACK_PRUNE_LEVEL", "2");

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.search_summary.requested_strategy, "BOUNDED_DP");
    EXPECT_EQ(plan.search_summary.selected_strategy, "HEURISTIC_GREEDY");
    EXPECT_EQ(plan.search_summary.fallback_reason, "MAX_STATES_CONSIDERED");
    EXPECT_EQ(plan.search_summary.fallback_threshold_name,
              "MAX_STATES_CONSIDERED");
    EXPECT_EQ(plan.search_summary.fallback_threshold_value, 1u);
    EXPECT_EQ(plan.search_summary.max_states_considered, 1u);
}

TEST_F(QueryPlannerIntegrationTest, UsingJoinMarksReorderBarrierAndFailsClosedToNestedLoop)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products USING (id)");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "INNER");
    EXPECT_EQ(plan.join_steps.front().legality_class, "USING_BARRIER");
    EXPECT_FALSE(plan.join_steps.front().reorderable);
    EXPECT_TRUE(plan.join_steps.front().requires_original_order);
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");
    EXPECT_FALSE(plan.join_steps.front().has_hash_keys);
    EXPECT_TRUE(plan.join_steps.front().using_reorder_barrier);
    EXPECT_FALSE(plan.join_steps.front().outer_reorder_barrier);
}

TEST_F(QueryPlannerIntegrationTest, MixedOuterAndInnerJoinKeepsSyntacticRelationOrder)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "LEFT JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 3u);
    ASSERT_EQ(plan.join_steps.size(), 2u);
    EXPECT_EQ(plan.join_steps[0].source_join_index, 0u);
    EXPECT_EQ(plan.join_steps[1].source_join_index, 1u);
    EXPECT_EQ(plan.join_steps[0].legality_class, "LEFT_OUTER_BARRIER");
    EXPECT_FALSE(plan.join_steps[0].reorderable);
    EXPECT_EQ(plan.join_steps[1].join_type, "INNER");
    EXPECT_EQ(plan.search_summary.selected_strategy, "INPUT_ORDER_ONLY");
    EXPECT_EQ(plan.search_summary.fallback_reason, "JOIN_REORDER_BARRIER");
}

TEST_F(QueryPlannerIntegrationTest, DisconnectedJoinGraphKeepsExplicitCrossJoinStep)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO test (id) VALUES (10)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO test (id) VALUES (20)").success());

    const std::string sql =
        "SELECT users.id "
        "FROM users "
        "JOIN products ON users.id = products.id "
        "CROSS JOIN test";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 2u);
    EXPECT_EQ(plan.search_summary.selected_strategy, "EXHAUSTIVE_DP");

    auto cross_it = std::find_if(plan.join_steps.begin(),
                                 plan.join_steps.end(),
                                 [](const scratchbird::optimizer::RuntimePlanJoinStep &step) {
                                     return step.join_type == "CROSS";
                                 });
    ASSERT_NE(cross_it, plan.join_steps.end());
    EXPECT_EQ(cross_it->method, "NESTED_LOOP");
    EXPECT_TRUE(cross_it->disconnected_component);
    EXPECT_EQ(cross_it->join_type, "CROSS");
    EXPECT_EQ(cross_it->legality_class, "CROSS_REORDERABLE");
    EXPECT_TRUE(cross_it->residual_predicates.empty());
}

TEST_F(QueryPlannerIntegrationTest, MgaCanonicalTelemetryFeedsPlannerCostingAndRuntimePlanProvenance)
{
    ASSERT_TRUE(createDatabase());

    CatalogManager::TableInfo orders_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "orders",
                                               orders_info,
                                               nullptr),
              Status::OK);

    StorageEngine::FragmentationAdvisory advisory{};
    advisory.page_id = 23;
    advisory.reclaimable_bytes = 8192;
    advisory.deleted_slots = 12;
    advisory.chain_depth_hint = 8;
    advisory.same_page_back_versions = 1;
    advisory.same_page_update_ratio = 0.125;
    advisory.dead_space_ratio = 0.45;
    advisory.rewrite_recommended = true;
    db_->storage_engine()->publishFragmentationAdvisory(
        orders_info.table_id, advisory.page_id, advisory);

    auto bytecode = compileSQL("SELECT * FROM orders WHERE user_id = 7");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));

    const auto *mga_active = findOptimizerControl(plan, "MGA_COSTING_ACTIVE");
    ASSERT_NE(mga_active, nullptr);
    EXPECT_EQ(mga_active->value, "true");

    const auto *mga_contract = findOptimizerControl(plan, "MGA_COSTING_CONTRACT");
    ASSERT_NE(mga_contract, nullptr);
    EXPECT_EQ(mga_contract->value, "sb_mga_observability/v1");

    auto provenance_it = std::find_if(
        plan.statistics_provenance.begin(),
        plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance &entry) {
            return entry.subject == "relation:orders" &&
                   entry.source == "MGA_CANONICAL_METRICS";
        });
    ASSERT_NE(provenance_it, plan.statistics_provenance.end());
    EXPECT_NE(provenance_it->detail.find("cleanup_debt_bytes=8192"),
              std::string::npos);
    EXPECT_NE(provenance_it->detail.find("same_page_update_ratio=0.125"),
              std::string::npos);

    auto trace_it = std::find_if(
        plan.considered_paths.begin(),
        plan.considered_paths.end(),
        [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
            return entry.phase == "MGA_COSTING" &&
                   entry.subject == "relation:orders" &&
                   entry.verdict == "ADJUSTED";
        });
    ASSERT_NE(trace_it, plan.considered_paths.end());
    EXPECT_NE(trace_it->reason.find("penalty="), std::string::npos);
    EXPECT_GT(trace_it->total_cost, 0.0);
}

TEST_F(QueryPlannerIntegrationTest,
       MgaCanonicalTelemetryRejectsBroadCoveringIndexPathUnderSevereChurn)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 4000; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 40)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(publishSevereMgaChurn("users"));

    auto bytecode =
        compileSQL("SELECT id, name FROM users WHERE id >= 3800");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_kind, "SEQ_SCAN");
    EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "INDEX_ONLY_SCAN"),
              relation.candidate_scan_families.end());

    auto trace_it = std::find_if(
        plan.rejected_paths.begin(),
        plan.rejected_paths.end(),
        [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
            return entry.phase == "MGA_SWITCHOVER" &&
                   entry.subject == "relation:users" &&
                   entry.candidate == "INDEX_ONLY_SCAN[idx_users_id_name]";
        });
    ASSERT_NE(trace_it, plan.rejected_paths.end())
        << "scan_kind=" << relation.scan_kind
        << ", candidates=" << joinStrings(relation.candidate_scan_families, ",")
        << ", rejected_paths=" << formatTraceEntries(plan.rejected_paths)
        << ", considered_paths=" << formatTraceEntries(plan.considered_paths);
    EXPECT_NE(trace_it->reason.find("rewrite-equivalent MGA churn"),
              std::string::npos);
    const auto refusal_it = std::find_if(
        relation.candidate_family_refusals.begin(),
        relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.candidate_label ==
                       "INDEX_ONLY_SCAN[idx_users_id_name]" &&
                   refusal.refusal_reason_code ==
                       "P08_MGA_GOVERNANCE_REJECTED" &&
                   refusal.refusal_class ==
                       "MGA governance rejected candidate under current pressure" &&
                   refusal.refusal_cause_domain == "POLICY" &&
                   refusal.refusal_detail.find(
                       "rewrite-equivalent MGA churn rejects broad index-only access") !=
                       std::string::npos;
        });
    EXPECT_NE(refusal_it, relation.candidate_family_refusals.end());
}

TEST_F(QueryPlannerIntegrationTest,
       MgaCanonicalTelemetryPreservesExactCoveringProbeUnderSevereChurn)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 4000; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 40)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(publishSevereMgaChurn("users"));

    auto bytecode =
        compileSQL("SELECT id, name FROM users WHERE id = 1777");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_kind, "INDEX_ONLY_SCAN");
    EXPECT_EQ(plan.relations.front().index_name, "idx_users_id_name");

    const bool rejected_exact_probe =
        std::any_of(plan.rejected_paths.begin(),
                    plan.rejected_paths.end(),
                    [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                        return entry.phase == "MGA_SWITCHOVER" &&
                               entry.candidate ==
                                   "INDEX_ONLY_SCAN[idx_users_id_name]";
                    });
    EXPECT_FALSE(rejected_exact_probe);
    const bool refused_exact_probe =
        std::any_of(plan.relations.front().candidate_family_refusals.begin(),
                    plan.relations.front().candidate_family_refusals.end(),
                    [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
                        return refusal.candidate_label ==
                                   "INDEX_ONLY_SCAN[idx_users_id_name]" &&
                               refusal.refusal_reason_code ==
                                   "P08_MGA_GOVERNANCE_REJECTED";
                    });
    EXPECT_FALSE(refused_exact_probe);
}

TEST_F(QueryPlannerIntegrationTest, HashJoinPlanExecutesAndReturnsExpectedRows)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 32)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (3, 'carol', 'c@example.com', 34)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (2, 'p2', 20.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (4, 'p4', 40.5)").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");

    auto result =
        executeSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 2u);
    std::sort(rows.begin(), rows.end());
    EXPECT_EQ(rows[0], "1");
    EXPECT_EQ(rows[1], "2");
}

TEST_F(QueryPlannerIntegrationTest,
       AdaptiveHashJoinPublishesReversibleBuildSideMetadata)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE probe_users (user_id INTEGER)")
                    .success());
    ASSERT_TRUE(
        executeSQL("INSERT INTO probe_users (user_id) VALUES (1)").success());
    ASSERT_TRUE(
        executeSQL("INSERT INTO probe_users (user_id) VALUES (2)").success());
    for (int order_id = 1; order_id <= 12; ++order_id)
    {
        const int user_id = order_id <= 6 ? 1 : 2;
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(order_id) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(10 + order_id) + ".0)")
                        .success());
    }

    ASSERT_TRUE(executeSQL("ANALYZE probe_users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());
    ASSERT_TRUE(executeSQL("SET OPTIMIZER.JOIN_METHOD = 'HASH_JOIN'").success());

    const std::string sql =
        "SELECT p.user_id, o.id "
        "FROM probe_users AS p "
        "JOIN orders AS o ON p.user_id = o.user_id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");
    EXPECT_TRUE(plan.join_steps.front().adaptive_join_enabled);
    EXPECT_EQ(plan.join_steps.front().planned_build_side, "RIGHT");
    EXPECT_EQ(plan.join_steps.front().adaptive_alternative_build_side,
              "LEFT");
    EXPECT_GT(plan.join_steps.front().adaptive_probe_sample_rows, 0u);
    EXPECT_DOUBLE_EQ(plan.join_steps.front().adaptive_flip_ratio_threshold,
                     1.25);
    EXPECT_NE(std::find(plan.join_steps.front().method_enablers.begin(),
                        plan.join_steps.front().method_enablers.end(),
                        "ADAPTIVE_BUILD_SIDE_HASH_JOIN"),
              plan.join_steps.front().method_enablers.end());

    const auto adaptive_trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry& entry) {
                         return entry.phase == "ADAPTIVE_JOIN" &&
                                entry.candidate ==
                                    "ADAPTIVE_BUILD_SIDE_HASH_JOIN[RIGHT<->LEFT]" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(adaptive_trace_it, plan.considered_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       AdaptiveHashJoinFlipsToObservedSmallerBuildSide)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE probe_users (user_id INTEGER)")
                    .success());
    ASSERT_TRUE(
        executeSQL("INSERT INTO probe_users (user_id) VALUES (1)").success());
    ASSERT_TRUE(
        executeSQL("INSERT INTO probe_users (user_id) VALUES (2)").success());
    for (int order_id = 1; order_id <= 20; ++order_id)
    {
        const int user_id = order_id <= 9 ? 1 : 2;
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(order_id) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(100 + order_id) + ".0)")
                        .success());
    }

    ASSERT_TRUE(executeSQL("ANALYZE probe_users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());
    ASSERT_TRUE(executeSQL("SET OPTIMIZER.JOIN_METHOD = 'HASH_JOIN'").success());

    const std::string sql =
        "SELECT p.user_id, o.id "
        "FROM probe_users AS p "
        "JOIN orders AS o ON p.user_id = o.user_id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_TRUE(plan.join_steps.front().adaptive_join_enabled);
    EXPECT_EQ(plan.join_steps.front().planned_build_side, "RIGHT");

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_adaptive_hash_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 20u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("SELECT TRACE join[0] method=HASH_JOIN adaptive_build=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("planned_build_side=RIGHT"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("selected_build_side=LEFT"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("flip_taken=1"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       QualifiedTableStarPreservesOwningRelationProjection)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(2, 'bob', 'b@example.com', 32)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES "
                           "(1, 'p1', 10.5)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES "
                           "(2, 'p2', 20.5)")
                    .success());

    auto result =
        executeSQL("SELECT u.*, p.name "
                   "FROM users AS u "
                   "JOIN products AS p ON u.id = p.id "
                   "ORDER BY u.id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->columnCount(), 5u);
    ASSERT_EQ(result.resultSet()->rowCount(), 2u);

    EXPECT_EQ(result.resultSet()->columnName(0), "id");
    EXPECT_EQ(result.resultSet()->columnName(1), "name");
    EXPECT_EQ(result.resultSet()->columnName(2), "email");
    EXPECT_EQ(result.resultSet()->columnName(3), "age");
    EXPECT_EQ(result.resultSet()->columnName(4), "name");

    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "alice");
    EXPECT_EQ(result.resultSet()->getValue(0, 2).toString(),
              "a@example.com");
    EXPECT_EQ(result.resultSet()->getValue(0, 3).toInt64(), 30);
    EXPECT_EQ(result.resultSet()->getValue(0, 4).toString(), "p1");
    EXPECT_EQ(result.resultSet()->getValue(1, 0).toInt64(), 2);
    EXPECT_EQ(result.resultSet()->getValue(1, 1).toString(), "bob");
    EXPECT_EQ(result.resultSet()->getValue(1, 2).toString(),
              "b@example.com");
    EXPECT_EQ(result.resultSet()->getValue(1, 3).toInt64(), 32);
    EXPECT_EQ(result.resultSet()->getValue(1, 4).toString(), "p2");
}

TEST_F(QueryPlannerIntegrationTest,
       CountStarExecutesWithProjectionPrunedRelationLoad)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 64; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'countuser" +
                               std::to_string(i) + "', 'count" +
                               std::to_string(i) +
                               "@example.com', 30)")
                        .success());
    }

    auto result = executeSQL("SELECT COUNT(*) FROM users");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    ASSERT_EQ(result.resultSet()->columnCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 64);
}

TEST_F(QueryPlannerIntegrationTest,
       NestedLoopSelfJoinResidualComparisonExecutesExpectedPairs)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "NESTED_LOOP");

    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(2, 'bob', 'b@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(3, 'carol', 'c@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(4, 'dave', 'd@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(5, 'erin', 'e@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT u1.id * 100 + u2.id AS pair_code "
        "FROM users u1 "
        "JOIN users u2 ON u1.age = u2.age AND u1.id < u2.id "
        "WHERE u1.age >= 30";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    std::sort(rows.begin(), rows.end());
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0], "102");
    EXPECT_EQ(rows[1], "103");
    EXPECT_EQ(rows[2], "203");
    EXPECT_EQ(rows[3], "405");
}

TEST_F(QueryPlannerIntegrationTest,
       DefaultSelfJoinResidualComparisonExecutesExpectedPairs)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_age ON users (age)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(2, 'bob', 'b@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(3, 'carol', 'c@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(4, 'dave', 'd@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES "
                           "(5, 'erin', 'e@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT u1.id * 100 + u2.id AS pair_code "
        "FROM users u1 "
        "JOIN users u2 ON u1.age = u2.age AND u1.id < u2.id "
        "WHERE u1.age >= 30";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_FALSE(plan.join_steps.front().method.empty());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    std::sort(rows.begin(), rows.end());
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0], "102");
    EXPECT_EQ(rows[1], "103");
    EXPECT_EQ(rows[2], "203");
    EXPECT_EQ(rows[3], "405");
}

TEST_F(QueryPlannerIntegrationTest, HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");

    for (int i = 1; i <= 1600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");
    EXPECT_TRUE(plan.join_steps.front().spill_expected);
    EXPECT_GT(plan.join_steps.front().spill_passes, 0u);
    EXPECT_GT(plan.join_steps.front().estimated_memory_bytes, 0u);
    EXPECT_GT(plan.join_steps.front().memory_budget_bytes, 0u);
    EXPECT_EQ(plan.join_steps.front().spill_policy, "ALLOW");
    EXPECT_EQ(plan.root.node_type, "HashJoin");
    EXPECT_TRUE(plan.root.spill_expected);
    EXPECT_GT(plan.root.memory_budget_bytes, 0u);
    const auto* work_mem = findOptimizerControl(plan, "WORK_MEM");
    ASSERT_NE(work_mem, nullptr);
    EXPECT_EQ(work_mem->source, "SESSION");
    const auto* spill_policy = findOptimizerControl(plan, "SPILL_POLICY");
    ASSERT_NE(spill_policy, nullptr);
    EXPECT_EQ(spill_policy->value, "ALLOW");
}

TEST_F(QueryPlannerIntegrationTest, JoinMethodControlForcesNestedLoopAndChangesCacheIdentity)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id";
    QueryCompilerV3::resetPlanCacheStats();

    auto default_bytecode = compileSQL(sql);
    ASSERT_FALSE(default_bytecode.empty()) << last_compile_errors_;
    auto after_default = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_default.misses, 1u);

    scratchbird::optimizer::RuntimePlan default_plan;
    ASSERT_TRUE(decodeRuntimePlan(default_bytecode, default_plan));

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "NESTED_LOOP");

    auto forced_bytecode = compileSQL(sql);
    ASSERT_FALSE(forced_bytecode.empty()) << last_compile_errors_;
    auto after_forced = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_forced.misses, 2u);

    scratchbird::optimizer::RuntimePlan forced_plan;
    ASSERT_TRUE(decodeRuntimePlan(forced_bytecode, forced_plan));
    ASSERT_EQ(forced_plan.join_steps.size(), 1u);
    EXPECT_EQ(forced_plan.join_steps.front().method, "NESTED_LOOP");
    const auto* join_method = findOptimizerControl(forced_plan, "JOIN_METHOD");
    ASSERT_NE(join_method, nullptr);
    EXPECT_EQ(join_method->value, "NESTED_LOOP");
    EXPECT_EQ(join_method->source, "SESSION");
    EXPECT_NE(default_plan.plan_hash, forced_plan.plan_hash);
}

TEST_F(QueryPlannerIntegrationTest, UnsupportedOptimizerDirectiveFailsClosed)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES", "FOO=BAR");
    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    EXPECT_TRUE(bytecode.empty());
    EXPECT_NE(last_compile_errors_.find("Unsupported optimizer directive"),
              std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, SpillPolicyDisallowRejectsSpilledHashJoin)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 1600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");

    bool found_hash_rejection = false;
    for (const auto& entry : plan.rejected_paths)
    {
        if (entry.candidate == "HASH_JOIN" &&
            entry.reason.find("spill policy disallows") != std::string::npos)
        {
            found_hash_rejection = true;
            break;
        }
    }
    EXPECT_TRUE(found_hash_rejection);
}

TEST_F(QueryPlannerIntegrationTest,
       SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashJoin)
{
    ASSERT_TRUE(createDatabase());

    auto* catalog = db_ != nullptr ? db_->catalog_manager() : nullptr;
    ASSERT_NE(catalog, nullptr);

    CatalogManager::SchemaInfo public_schema;
    ErrorContext ctx;
    ASSERT_EQ(catalog->getSchema("public", public_schema, &ctx), Status::OK)
        << ctx.message;

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id";
    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback;
    feedback.grant_feedback_uuid = generateUuidV7();
    feedback.database_uuid = db_->uuid();
    feedback.schema_root_uuid = public_schema.schema_id;
    feedback.operator_kind = "HASH_JOIN";
    feedback.sample_count = 8;
    feedback.last_grant_bytes = 256ULL * 1024ULL;
    feedback.p50_bytes = 128ULL * 1024ULL;
    feedback.p90_bytes = 256ULL * 1024ULL;
    feedback.peak_bytes = 256ULL * 1024ULL;
    feedback.spill_count = 1;
    feedback.state = "STABLE";
    feedback.updated_at = 1;
    feedback.grant_key_hash = memoryGrantFeedbackKeyHashForTest(
        db_->uuid(),
        public_schema.schema_id,
        plannerNormalizedStatementIdForSql(sql),
        connection_ctx_ != nullptr ? connection_ctx_->dialect_tag()
                                   : std::string(),
        "GENERIC",
        "EXECUTE",
        "ROW_STORE_MGA",
        feedback.operator_kind);
    ASSERT_NE(feedback.grant_key_hash, 0u);
    ASSERT_EQ(catalog->upsertMemoryGrantFeedbackCatalogEntry(feedback, &ctx),
              Status::OK)
        << ctx.message;

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 1600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_HASH_JOIN");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    EXPECT_GE(std::stoull(feedback_budget->value),
              16ULL * 1024ULL * 1024ULL);
    if (plan.join_steps.front().method == "HASH_JOIN")
    {
        EXPECT_EQ(std::stoull(feedback_budget->value),
                  plan.join_steps.front().memory_budget_bytes);
        EXPECT_FALSE(plan.join_steps.front().spill_expected);
    }

    const auto* feedback_state =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_STATE_HASH_JOIN");
    ASSERT_NE(feedback_state, nullptr);
    EXPECT_EQ(feedback_state->value, "STABLE");
    EXPECT_EQ(feedback_state->source, "CATALOG");

    bool found_feedback_trace = false;
    bool found_hash_considered = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "JOIN_METHOD" &&
            entry.candidate == "HASH_JOIN" &&
            entry.verdict == "CONSIDERED")
        {
            found_hash_considered = true;
        }
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "HASH_JOIN" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
        }
    }
    EXPECT_TRUE(found_hash_considered) << formatTraceEntries(plan.considered_paths);
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);

    bool found_spill_rejection = false;
    for (const auto& entry : plan.rejected_paths)
    {
        if (entry.candidate == "HASH_JOIN" &&
            entry.reason.find("spill policy disallows") != std::string::npos)
        {
            found_spill_rejection = true;
            break;
        }
    }
    EXPECT_FALSE(found_spill_rejection) << formatTraceEntries(plan.rejected_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedHashJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 1600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'joinuser" +
                               std::to_string(i) + "', 'joinu" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'joinp" +
                               std::to_string(i) + "', " +
                               std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id";
    auto baseline_bytecode = compileSQL(sql);
    ASSERT_FALSE(baseline_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline_bytecode, baseline_plan));
    ASSERT_EQ(baseline_plan.join_steps.size(), 1u);
    EXPECT_EQ(baseline_plan.join_steps.front().method, "HASH_JOIN");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "HASH_JOIN", feedback));
    EXPECT_EQ(feedback.operator_kind, "HASH_JOIN");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GT(feedback.p90_bytes, 0u);
    EXPECT_GE(feedback.peak_bytes, feedback.p90_bytes);
    if (baseline_plan.join_steps.front().spill_expected)
    {
        EXPECT_GE(feedback.spill_count, 1u);
    }
    EXPECT_EQ(feedback.state, "WARMING");

    QueryCompilerV3::invalidateAllPlanCache();
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_HASH_JOIN");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    if (plan.join_steps.front().method == "HASH_JOIN")
    {
        EXPECT_EQ(std::stoull(feedback_budget->value),
                  plan.join_steps.front().memory_budget_bytes);
        EXPECT_FALSE(plan.join_steps.front().spill_expected);
    }

    bool found_feedback_trace = false;
    bool found_spill_rejection = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "HASH_JOIN" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
        }
    }
    for (const auto& entry : plan.rejected_paths)
    {
        if (entry.candidate == "HASH_JOIN" &&
            entry.reason.find("spill policy disallows") != std::string::npos)
        {
            found_spill_rejection = true;
            break;
        }
    }
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);
    EXPECT_FALSE(found_spill_rejection) << formatTraceEntries(plan.rejected_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedHashJoinCapturesActualSpillOnStaleBytecode)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "128KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");
    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "HASH_JOIN");

    for (int i = 1; i <= 64; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'stalehashu" +
                               std::to_string(i) + "', 'stalehashu" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'stalehashp" +
                               std::to_string(i) + "', 10.0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    ASSERT_EQ(stale_plan.join_steps.size(), 1u);
    EXPECT_EQ(stale_plan.join_steps.front().method, "HASH_JOIN");
    EXPECT_FALSE(stale_plan.join_steps.front().spill_expected);

    for (int i = 65; i <= 4096; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'stalehashu" +
                               std::to_string(i) + "', 'stalehashu" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'stalehashp" +
                               std::to_string(i) + "', 10.0)")
                        .success());
    }

    auto result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "HASH_JOIN", feedback));
    EXPECT_EQ(feedback.operator_kind, "HASH_JOIN");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GE(feedback.spill_count, 1u);
    EXPECT_EQ(feedback.state, "WARMING");
}

TEST_F(QueryPlannerIntegrationTest,
       SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForMergeJoin)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_products_id ON products (id)").success());
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");
    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "MERGE_JOIN");

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());
    const std::string sql =
        "SELECT users.id "
        "FROM users JOIN products ON users.id = products.id "
        "WHERE users.id = 2 AND products.id = 2";

    seedMemoryGrantFeedback(sql,
                            "MERGE_JOIN",
                            256ULL * 1024ULL * 1024ULL,
                            128ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL);

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_EQ(plan.root.node_type, "MergeJoin");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_MERGE_JOIN");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    const uint64_t expected_budget = std::stoull(feedback_budget->value);
    EXPECT_GT(expected_budget, 64ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(plan.join_steps.front().memory_budget_bytes, expected_budget);
    EXPECT_FALSE(plan.join_steps.front().spill_expected);
    EXPECT_EQ(plan.root.memory_budget_bytes, expected_budget);
    EXPECT_FALSE(plan.root.spill_expected);

    bool found_feedback_trace = false;
    bool found_spill_rejection = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "MERGE_JOIN" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
        }
    }
    for (const auto& entry : plan.rejected_paths)
    {
        if (entry.candidate == "MERGE_JOIN" &&
            entry.reason.find("spill policy disallows") != std::string::npos)
        {
            found_spill_rejection = true;
            break;
        }
    }
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);
    EXPECT_FALSE(found_spill_rejection) << formatTraceEntries(plan.rejected_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedMergeJoinPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_products_id ON products (id)").success());
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");
    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "MERGE_JOIN");

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'mergeuser" +
                               std::to_string(i) + "', 'mergeu" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'mergep" +
                               std::to_string(i) + "', " +
                               std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id "
        "FROM users JOIN products ON users.id = products.id "
        "WHERE users.id = 2 AND products.id = 2";

    auto baseline_bytecode = compileSQL(sql);
    ASSERT_FALSE(baseline_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline_bytecode, baseline_plan));
    ASSERT_EQ(baseline_plan.join_steps.size(), 1u);
    EXPECT_EQ(baseline_plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_EQ(baseline_plan.root.node_type, "MergeJoin");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "MERGE_JOIN", feedback));
    EXPECT_EQ(feedback.operator_kind, "MERGE_JOIN");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GT(feedback.p90_bytes, 0u);
    EXPECT_GE(feedback.peak_bytes, feedback.p90_bytes);
    if (baseline_plan.join_steps.front().spill_expected)
    {
        EXPECT_GE(feedback.spill_count, 1u);
    }
    EXPECT_EQ(feedback.state, "WARMING");

    QueryCompilerV3::invalidateAllPlanCache();
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_EQ(plan.root.node_type, "MergeJoin");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_MERGE_JOIN");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    const uint64_t expected_budget = std::stoull(feedback_budget->value);
    EXPECT_EQ(plan.join_steps.front().memory_budget_bytes, expected_budget);
    EXPECT_FALSE(plan.join_steps.front().spill_expected);
    EXPECT_EQ(plan.root.memory_budget_bytes, expected_budget);
    EXPECT_FALSE(plan.root.spill_expected);

    bool found_feedback_trace = false;
    bool found_spill_rejection = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "MERGE_JOIN" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
        }
    }
    for (const auto& entry : plan.rejected_paths)
    {
        if (entry.candidate == "MERGE_JOIN" &&
            entry.reason.find("spill policy disallows") != std::string::npos)
        {
            found_spill_rejection = true;
            break;
        }
    }
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);
    EXPECT_FALSE(found_spill_rejection) << formatTraceEntries(plan.rejected_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecuteBytecodeRunsSpilledMergeJoinThroughWorkfileAndPersistsFeedback)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");
    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "MERGE_JOIN");

    for (int i = 1; i <= 1024; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'spillmergeu" +
                               std::to_string(i) + "', 'spillmergeu" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'spillmergep" +
                               std::to_string(i) + "', " +
                               std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_EQ(plan.root.node_type, "MergeJoin");
    ASSERT_EQ(plan.root.children.size(), 2u);
    EXPECT_EQ(plan.root.children[0].node_type, "Sort");
    EXPECT_EQ(plan.root.children[1].node_type, "Sort");

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    EXPECT_EQ(result.resultSet()->rowCount(), 1024u);

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "MERGE_JOIN", feedback));
    EXPECT_EQ(feedback.operator_kind, "MERGE_JOIN");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GE(feedback.spill_count, 1u);
    EXPECT_EQ(feedback.state, "WARMING");
}

TEST_F(QueryPlannerIntegrationTest,
       SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashAggregate)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 32768; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'agg" + std::to_string(i) +
                               "', 'agg" + std::to_string(i) +
                               "@example.com', " + std::to_string(i) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT age, COUNT(*) FROM users GROUP BY age";
    uint64_t baseline_budget = 0;
    auto baseline_bytecode = compileSQL(sql);
    if (!baseline_bytecode.empty())
    {
        scratchbird::optimizer::RuntimePlan baseline_plan;
        ASSERT_TRUE(decodeRuntimePlan(baseline_bytecode, baseline_plan));
        EXPECT_EQ(baseline_plan.root.node_type, "Aggregate");
        baseline_budget = baseline_plan.root.memory_budget_bytes;
    }
    else
    {
        EXPECT_NE(last_compile_errors_.find(
                      "Aggregate operator exceeds work_mem under spill-disallow policy"),
                  std::string::npos);
    }

    seedMemoryGrantFeedback(sql,
                            "HASH_AGG",
                            256ULL * 1024ULL * 1024ULL,
                            128ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL);

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_HASH_AGG");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    const uint64_t expected_budget = std::stoull(feedback_budget->value);
    EXPECT_GT(expected_budget, 64ULL * 1024ULL);
    if (baseline_budget > 0)
    {
        EXPECT_GT(expected_budget, baseline_budget);
    }
    EXPECT_EQ(plan.root.memory_budget_bytes, expected_budget);
    EXPECT_FALSE(plan.root.spill_expected);

    bool found_feedback_trace = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "HASH_AGG" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
            break;
        }
    }
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForSort)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 16384; ++i)
    {
        const int reversed = 16385 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'sort" +
                               std::to_string(reversed) + "', 'sort" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    uint64_t baseline_budget = 0;
    auto baseline_bytecode = compileSQL(sql);
    if (!baseline_bytecode.empty())
    {
        scratchbird::optimizer::RuntimePlan baseline_plan;
        ASSERT_TRUE(decodeRuntimePlan(baseline_bytecode, baseline_plan));
        EXPECT_EQ(baseline_plan.root.node_type, "Sort");
        baseline_budget = baseline_plan.root.memory_budget_bytes;
    }
    else
    {
        EXPECT_NE(last_compile_errors_.find(
                      "Sort operator exceeds work_mem under spill-disallow policy"),
                  std::string::npos);
    }

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            256ULL * 1024ULL * 1024ULL,
                            128ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL);

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Sort");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_SORT");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    const uint64_t expected_budget = std::stoull(feedback_budget->value);
    EXPECT_GT(expected_budget, 64ULL * 1024ULL);
    if (baseline_budget > 0)
    {
        EXPECT_GT(expected_budget, baseline_budget);
    }
    EXPECT_EQ(plan.root.memory_budget_bytes, expected_budget);
    EXPECT_FALSE(plan.root.spill_expected);

    bool found_feedback_trace = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "SORT" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
            break;
        }
    }
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedSortPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 16384; ++i)
    {
        const int reversed = 16385 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'persist" +
                               std::to_string(reversed) + "', 'persist" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "SORT", feedback));
    EXPECT_EQ(feedback.operator_kind, "SORT");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GT(feedback.p90_bytes, 0u);
    EXPECT_GE(feedback.peak_bytes, feedback.p90_bytes);
    EXPECT_GE(feedback.spill_count, 1u);
    EXPECT_EQ(feedback.state, "WARMING");

    QueryCompilerV3::invalidateAllPlanCache();
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Sort");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_SORT");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    EXPECT_EQ(plan.root.memory_budget_bytes, std::stoull(feedback_budget->value));
    EXPECT_FALSE(plan.root.spill_expected);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedSortFeedbackRecordsUnderuseBeforeShrinkThreshold)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        const int reversed = 65 - i;
        std::ostringstream name;
        name << "shrinksort" << std::setw(5) << std::setfill('0')
             << reversed;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', 'shrinksort" + std::to_string(i) +
                               "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    const uint64_t seeded_grant = 128ULL * 1024ULL * 1024ULL;
    seedMemoryGrantFeedback(sql,
                            "SORT",
                            seeded_grant,
                            96ULL * 1024ULL * 1024ULL,
                            seeded_grant,
                            seeded_grant,
                            0,
                            "STABLE",
                            8);

    QueryCompilerV3::invalidateAllPlanCache();
    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "SORT", feedback));
    EXPECT_EQ(feedback.operator_kind, "SORT");
    EXPECT_GE(feedback.sample_count, 9u);
    EXPECT_EQ(feedback.last_grant_bytes, seeded_grant);
    EXPECT_GE(feedback.underuse_streak, 1u);
    EXPECT_EQ(feedback.state, "STABLE");
}

TEST_F(QueryPlannerIntegrationTest,
       StableUnderuseFeedbackShrinksSortReservationUnderSpillDisallow)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        const int reversed = 65 - i;
        std::ostringstream name;
        name << "rightsizesort" << std::setw(5) << std::setfill('0')
             << reversed;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', 'rightsizesort" + std::to_string(i) +
                               "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto baseline = compileSQL(sql);
    ASSERT_FALSE(baseline.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline, baseline_plan));
    ASSERT_EQ(baseline_plan.root.node_type, "Sort");
    const uint64_t baseline_budget = baseline_plan.root.memory_budget_bytes;
    EXPECT_GT(baseline_budget, 8ULL * 1024ULL * 1024ULL);
    const auto* baseline_reservation =
        findOptimizerControl(baseline_plan, "PLAN_MEMORY_RESERVATION_BYTES");
    ASSERT_NE(baseline_reservation, nullptr);
    const uint64_t baseline_statement_reservation =
        std::stoull(baseline_reservation->value);

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            8ULL * 1024ULL * 1024ULL,
                            4ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8);

    QueryCompilerV3::invalidateAllPlanCache();
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Sort");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_SORT");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    EXPECT_EQ(std::stoull(feedback_budget->value),
              8ULL * 1024ULL * 1024ULL);
    const auto* reservation =
        findOptimizerControl(plan, "PLAN_MEMORY_RESERVATION_BYTES");
    ASSERT_NE(reservation, nullptr);
    EXPECT_EQ(plan.root.memory_budget_bytes,
              8ULL * 1024ULL * 1024ULL);
    EXPECT_LT(plan.root.memory_budget_bytes, baseline_budget);
    EXPECT_EQ(std::stoull(reservation->value),
              std::max<uint64_t>(plan.root.memory_budget_bytes,
                                 4ULL * 1024ULL * 1024ULL));
    EXPECT_LT(std::stoull(reservation->value), baseline_statement_reservation);
    EXPECT_FALSE(plan.root.spill_expected);

    bool found_feedback_trace = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "SORT" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
            EXPECT_NE(entry.reason.find("right-sized operator budget"),
                      std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       OscillatingSortFeedbackDoesNotShrinkReservation)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        const int reversed = 65 - i;
        std::ostringstream name;
        name << "oscillatingsort" << std::setw(5) << std::setfill('0')
             << reversed;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', 'oscillatingsort" + std::to_string(i) +
                               "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto baseline = compileSQL(sql);
    ASSERT_FALSE(baseline.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline, baseline_plan));
    ASSERT_EQ(baseline_plan.root.node_type, "Sort");
    const uint64_t baseline_budget = baseline_plan.root.memory_budget_bytes;
    const auto* baseline_reservation =
        findOptimizerControl(baseline_plan, "PLAN_MEMORY_RESERVATION_BYTES");
    ASSERT_NE(baseline_reservation, nullptr);
    const uint64_t baseline_statement_reservation =
        std::stoull(baseline_reservation->value);

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            8ULL * 1024ULL * 1024ULL,
                            4ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            0,
                            "OSCILLATING",
                            16,
                            8,
                            5,
                            -1,
                            2);

    QueryCompilerV3::invalidateAllPlanCache();
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Sort");
    const auto* reservation =
        findOptimizerControl(plan, "PLAN_MEMORY_RESERVATION_BYTES");
    ASSERT_NE(reservation, nullptr);
    EXPECT_EQ(plan.root.memory_budget_bytes, baseline_budget);
    EXPECT_EQ(std::stoull(reservation->value), baseline_statement_reservation);

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_SORT");
    EXPECT_EQ(feedback_budget, nullptr);
}

TEST_F(QueryPlannerIntegrationTest,
       FeedbackShrunkSortReservationChangesRuntimeSpillDisallowAdmission)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        const int reversed = 65 - i;
        std::ostringstream name;
        name << "admissionrightsize" << std::setw(5) << std::setfill('0')
             << reversed;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', 'admissionrightsize" + std::to_string(i) +
                               "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto baseline = compileSQL(sql);
    ASSERT_FALSE(baseline.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline, baseline_plan));
    ASSERT_EQ(baseline_plan.root.node_type, "Sort");
    const auto* baseline_reservation =
        findOptimizerControl(baseline_plan, "PLAN_MEMORY_RESERVATION_BYTES");
    ASSERT_NE(baseline_reservation, nullptr);
    const uint64_t baseline_statement_reservation =
        std::stoull(baseline_reservation->value);
    ASSERT_GT(baseline_statement_reservation, 8ULL * 1024ULL * 1024ULL);

    const uint64_t live_work_mem_bytes =
        (baseline_statement_reservation + (8ULL * 1024ULL * 1024ULL)) / 2ULL;
    ASSERT_GT(live_work_mem_bytes, 8ULL * 1024ULL * 1024ULL);
    ASSERT_LT(live_work_mem_bytes, baseline_statement_reservation);

    connection_ctx_->setSessionVariable("WORK_MEM",
                                        std::to_string(live_work_mem_bytes));
    auto baseline_result = executeBytecode(baseline);
    ASSERT_FALSE(baseline_result.success());
    EXPECT_NE(baseline_result.error().find("memory reservation"),
              std::string::npos);

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");
    seedMemoryGrantFeedback(sql,
                            "SORT",
                            8ULL * 1024ULL * 1024ULL,
                            4ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8);

    QueryCompilerV3::invalidateAllPlanCache();
    auto shrunk = compileSQL(sql);
    ASSERT_FALSE(shrunk.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan shrunk_plan;
    ASSERT_TRUE(decodeRuntimePlan(shrunk, shrunk_plan));
    ASSERT_EQ(shrunk_plan.root.node_type, "Sort");
    const auto* shrunk_reservation =
        findOptimizerControl(shrunk_plan, "PLAN_MEMORY_RESERVATION_BYTES");
    ASSERT_NE(shrunk_reservation, nullptr);
    EXPECT_EQ(std::stoull(shrunk_reservation->value),
              8ULL * 1024ULL * 1024ULL);

    connection_ctx_->setSessionVariable("WORK_MEM",
                                        std::to_string(live_work_mem_bytes));
    auto shrunk_result = executeBytecode(shrunk);
    ASSERT_TRUE(shrunk_result.success()) << shrunk_result.error();
    ASSERT_TRUE(shrunk_result.hasResultSet());
    ASSERT_NE(shrunk_result.resultSet(), nullptr);
    EXPECT_EQ(shrunk_result.resultSet()->rowCount(), 64u);
}

TEST_F(QueryPlannerIntegrationTest,
       MemoryGrantFeedbackDoesNotCrossPlanProfileIdentity)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'profilegrant" +
                               std::to_string(257 - i) + "', 'profilegrant" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT name FROM users WHERE age < $1 ORDER BY name";
    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "40"});

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            8ULL * 1024ULL * 1024ULL,
                            4ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8,
                            0,
                            0,
                            0,
                            "GENERIC");

    QueryCompilerV3::invalidateAllPlanCache();
    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_PROFILE", "GENERIC");
    auto generic_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(generic_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan generic_plan;
    ASSERT_TRUE(decodeRuntimePlan(generic_compile.bytecode(), generic_plan));
    EXPECT_EQ(generic_plan.cache_mode, "GENERIC");
    const auto* generic_feedback =
        findOptimizerControl(generic_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    ASSERT_NE(generic_feedback, nullptr);
    EXPECT_EQ(generic_feedback->source, "CATALOG");
    EXPECT_EQ(std::stoull(generic_feedback->value),
              8ULL * 1024ULL * 1024ULL);

    QueryCompilerV3::invalidateAllPlanCache();
    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_PROFILE", "CUSTOM");
    auto custom_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(custom_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan custom_plan;
    ASSERT_TRUE(decodeRuntimePlan(custom_compile.bytecode(), custom_plan));
    EXPECT_EQ(custom_plan.cache_mode, "CUSTOM");
    const auto* custom_feedback =
        findOptimizerControl(custom_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    EXPECT_EQ(custom_feedback, nullptr);
}

TEST_F(QueryPlannerIntegrationTest,
       MemoryGrantFeedbackRequiresPolicySnapshotAndStorageShapeIdentity)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_PROFILE", "GENERIC");

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'policygrant" +
                               std::to_string(257 - i) + "', 'policygrant" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT name FROM users WHERE age < $1 ORDER BY name";
    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "40"});

    auto baseline_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(baseline_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline_compile.bytecode(), baseline_plan));
    ASSERT_EQ(baseline_plan.cache_mode, "GENERIC");
    ASSERT_FALSE(baseline_plan.execution_intent_class.empty());
    ASSERT_FALSE(baseline_plan.storage_layer_shape.empty());
    const auto* baseline_policy_control =
        findOptimizerControl(baseline_plan, "PLAN_POLICY_SNAPSHOT");
    ASSERT_NE(baseline_policy_control, nullptr);
    ASSERT_FALSE(baseline_policy_control->value.empty());
    const std::string planner_policy_snapshot_id =
        connection_ctx_->dialect_tag();
    ASSERT_FALSE(planner_policy_snapshot_id.empty());

    const auto* baseline_feedback =
        findOptimizerControl(baseline_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    EXPECT_EQ(baseline_feedback, nullptr);

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            8ULL * 1024ULL * 1024ULL,
                            4ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8,
                            0,
                            0,
                            0,
                            baseline_plan.cache_mode,
                            baseline_plan.execution_intent_class,
                            baseline_plan.storage_layer_shape,
                            planner_policy_snapshot_id + "|wrong");

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            16ULL * 1024ULL * 1024ULL,
                            8ULL * 1024ULL * 1024ULL,
                            12ULL * 1024ULL * 1024ULL,
                            12ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8,
                            0,
                            0,
                            0,
                            baseline_plan.cache_mode,
                            baseline_plan.execution_intent_class,
                            baseline_plan.storage_layer_shape + "_ALT",
                            planner_policy_snapshot_id);

    QueryCompilerV3::invalidateAllPlanCache();
    auto mismatched_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(mismatched_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan mismatched_plan;
    ASSERT_TRUE(
        decodeRuntimePlan(mismatched_compile.bytecode(), mismatched_plan));
    ASSERT_EQ(mismatched_plan.cache_mode, "GENERIC");
    const auto* mismatched_feedback =
        findOptimizerControl(mismatched_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    EXPECT_EQ(mismatched_feedback, nullptr);

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            24ULL * 1024ULL * 1024ULL,
                            12ULL * 1024ULL * 1024ULL,
                            18ULL * 1024ULL * 1024ULL,
                            18ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8,
                            0,
                            0,
                            0,
                            baseline_plan.cache_mode,
                            baseline_plan.execution_intent_class,
                            baseline_plan.storage_layer_shape,
                            planner_policy_snapshot_id);

    QueryCompilerV3::invalidateAllPlanCache();
    auto matched_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(matched_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan matched_plan;
    ASSERT_TRUE(decodeRuntimePlan(matched_compile.bytecode(), matched_plan));
    ASSERT_EQ(matched_plan.cache_mode, "GENERIC");
    const auto* matched_feedback =
        findOptimizerControl(matched_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    ASSERT_NE(matched_feedback, nullptr);
    EXPECT_EQ(matched_feedback->source, "CATALOG");
    EXPECT_EQ(std::stoull(matched_feedback->value),
              24ULL * 1024ULL * 1024ULL);
}

TEST_F(QueryPlannerIntegrationTest,
       MemoryGrantFeedbackRequiresExecutionIntentIdentity)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_PROFILE", "GENERIC");

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'intentgrant" +
                               std::to_string(257 - i) + "', 'intentgrant" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT name FROM users WHERE age < $1 ORDER BY name";
    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "40"});

    auto baseline_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(baseline_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline_compile.bytecode(), baseline_plan));
    ASSERT_EQ(baseline_plan.cache_mode, "GENERIC");
    ASSERT_FALSE(baseline_plan.execution_intent_class.empty());
    ASSERT_FALSE(baseline_plan.storage_layer_shape.empty());

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            8ULL * 1024ULL * 1024ULL,
                            4ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            6ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8,
                            0,
                            0,
                            0,
                            baseline_plan.cache_mode,
                            "EXPLAIN",
                            baseline_plan.storage_layer_shape,
                            connection_ctx_->dialect_tag());

    QueryCompilerV3::invalidateAllPlanCache();
    auto wrong_intent_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(wrong_intent_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan wrong_intent_plan;
    ASSERT_TRUE(
        decodeRuntimePlan(wrong_intent_compile.bytecode(), wrong_intent_plan));
    const auto* wrong_intent_feedback =
        findOptimizerControl(wrong_intent_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    EXPECT_EQ(wrong_intent_feedback, nullptr);

    seedMemoryGrantFeedback(sql,
                            "SORT",
                            24ULL * 1024ULL * 1024ULL,
                            12ULL * 1024ULL * 1024ULL,
                            18ULL * 1024ULL * 1024ULL,
                            18ULL * 1024ULL * 1024ULL,
                            0,
                            "STABLE",
                            16,
                            8,
                            0,
                            0,
                            0,
                            baseline_plan.cache_mode,
                            baseline_plan.execution_intent_class,
                            baseline_plan.storage_layer_shape,
                            connection_ctx_->dialect_tag());

    QueryCompilerV3::invalidateAllPlanCache();
    auto matched_compile = compileSQLWithParameters(sql, bindings);
    ASSERT_TRUE(matched_compile.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan matched_plan;
    ASSERT_TRUE(decodeRuntimePlan(matched_compile.bytecode(), matched_plan));
    const auto* matched_feedback =
        findOptimizerControl(matched_plan, "MEMORY_GRANT_FEEDBACK_SORT");
    ASSERT_NE(matched_feedback, nullptr);
    EXPECT_EQ(matched_feedback->source, "CATALOG");
    EXPECT_EQ(std::stoull(matched_feedback->value),
              24ULL * 1024ULL * 1024ULL);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedWindowPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 16384; ++i)
    {
        const int reversed = 16385 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'windowpersist" +
                               std::to_string(reversed) + "', 'windowpersist" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT ROW_NUMBER() OVER (ORDER BY name) FROM users";
    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "WINDOW", feedback));
    EXPECT_EQ(feedback.operator_kind, "WINDOW");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GT(feedback.p90_bytes, 0u);
    EXPECT_GE(feedback.peak_bytes, feedback.p90_bytes);
    EXPECT_GE(feedback.spill_count, 1u);
    EXPECT_EQ(feedback.state, "WARMING");

    QueryCompilerV3::invalidateAllPlanCache();
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Window");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_WINDOW");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    EXPECT_EQ(plan.root.memory_budget_bytes, std::stoull(feedback_budget->value));
    EXPECT_FALSE(plan.root.spill_expected);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecuteBytecodeRunsSpilledSortThroughWorkfileAndPreservesOrdering)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 4096; ++i)
    {
        const int reversed = 4097 - i;
        std::ostringstream name;
        name << "spill" << std::setw(5) << std::setfill('0') << reversed;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', 'spill" + std::to_string(i) +
                               "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Sort");
    EXPECT_TRUE(plan.root.spill_expected);

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "spill00001");
    EXPECT_EQ(result.resultSet()->getValue(result.resultSet()->rowCount() - 1,
                                           0)
                  .toString(),
              "spill04096");

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "SORT", feedback));
    EXPECT_EQ(feedback.operator_kind, "SORT");
    EXPECT_GE(feedback.spill_count, 1u);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedSortCapturesActualSpillOnStaleBytecode)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        const int reversed = 65 - i;
        std::ostringstream name;
        name << "stalesort" << std::setw(5) << std::setfill('0') << reversed;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', 'stalesort" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    EXPECT_EQ(stale_plan.root.node_type, "Sort");
    EXPECT_FALSE(stale_plan.root.spill_expected);

    for (int i = 65; i <= 4096; ++i)
    {
        const int reversed = 4097 - i;
        std::ostringstream name;
        name << "stalesort" << std::setw(5) << std::setfill('0') << reversed;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', 'stalesort" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");

    auto result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "stalesort00001");
    EXPECT_EQ(result.resultSet()->getValue(result.resultSet()->rowCount() - 1,
                                           0)
                  .toString(),
              "stalesort04032");

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "SORT", feedback));
    EXPECT_EQ(feedback.operator_kind, "SORT");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GE(feedback.spill_count, 1u);
    EXPECT_EQ(feedback.state, "WARMING");
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedWindowCapturesActualSortSpillOnStaleBytecode)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        const int reversed = 65 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'stalewindow" +
                               std::to_string(reversed) + "', 'stalewindow" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT ROW_NUMBER() OVER (ORDER BY name) FROM users";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    EXPECT_EQ(stale_plan.root.node_type, "Window");
    EXPECT_FALSE(stale_plan.root.spill_expected);

    for (int i = 65; i <= 4096; ++i)
    {
        const int reversed = 4097 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'stalewindow" +
                               std::to_string(reversed) + "', 'stalewindow" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }

    auto result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "WINDOW", feedback));
    EXPECT_EQ(feedback.operator_kind, "WINDOW");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GE(feedback.spill_count, 1u);
    EXPECT_EQ(feedback.state, "WARMING");
}

TEST_F(QueryPlannerIntegrationTest,
       SortTopNShortcutsAlreadyOrderedInputWithoutFullSort)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 256; ++i)
    {
        std::ostringstream name;
        name << "ordered" << std::setw(5) << std::setfill('0') << i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', '" + name.str() + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_sort_topn_ordered_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result =
        executeSQL("SELECT name FROM users ORDER BY name LIMIT 10");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 10u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "ordered00001");
    EXPECT_EQ(result.resultSet()->getValue(9, 0).toString(), "ordered00010");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE sort mode=TOP_N_ALREADY_SORTED spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("input_rows=256 output_rows=10"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("top_n=10"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledSortSkipsRunSortForAlreadyOrderedInput)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 4096; ++i)
    {
        std::ostringstream name;
        name << "presorted" << std::setw(5) << std::setfill('0') << i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name.str() +
                               "', '" + name.str() + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_sort_spill_ordered_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL("SELECT name FROM users ORDER BY name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "presorted00001");
    EXPECT_EQ(result.resultSet()->getValue(result.resultSet()->rowCount() - 1,
                                           0)
                  .toString(),
              "presorted04096");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE sort mode=PRESORTED_RUNS spill=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("input_rows=4096 output_rows=4096"),
              std::string::npos)
        << trace;
    EXPECT_EQ(trace.find("presorted_runs=0"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       IncrementalSortUsesPrefixOrderedInputInMemory)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE sort_pairs (bucket INTEGER, name TEXT)")
            .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int bucket = 0; bucket < 8; ++bucket)
    {
        for (int item = 15; item >= 0; --item)
        {
            std::ostringstream name;
            name << "bucket" << std::setw(2) << std::setfill('0') << bucket
                 << "_item" << std::setw(2) << std::setfill('0') << item;
            ASSERT_TRUE(
                executeSQL("INSERT INTO sort_pairs (bucket, name) VALUES (" +
                           std::to_string(bucket) + ", '" + name.str() +
                           "')")
                    .success());
        }
    }
    ASSERT_TRUE(executeSQL("ANALYZE sort_pairs").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_sort_incremental_in_memory_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT bucket, name FROM sort_pairs ORDER BY bucket, name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 128u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 0);
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(),
              "bucket00_item00");
    EXPECT_EQ(result.resultSet()->getValue(15, 1).toString(),
              "bucket00_item15");
    EXPECT_EQ(result.resultSet()->getValue(16, 0).toInt64(), 1);
    EXPECT_EQ(result.resultSet()->getValue(16, 1).toString(),
              "bucket01_item00");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE sort mode=PREFIX_1_INCREMENTAL spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("prefix_order_keys=1"), std::string::npos) << trace;
    EXPECT_NE(trace.find("incremental_groups=8"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledSortUsesIncrementalRunOrderingOnPrefixOrderedInput)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE sort_pairs (bucket INTEGER, name TEXT)")
            .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int bucket = 0; bucket < 128; ++bucket)
    {
        for (int item = 31; item >= 0; --item)
        {
            std::ostringstream name;
            name << "bucket" << std::setw(3) << std::setfill('0') << bucket
                 << "_item" << std::setw(2) << std::setfill('0') << item;
            ASSERT_TRUE(
                executeSQL("INSERT INTO sort_pairs (bucket, name) VALUES (" +
                           std::to_string(bucket) + ", '" + name.str() +
                           "')")
                    .success());
        }
    }
    ASSERT_TRUE(executeSQL("ANALYZE sort_pairs").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_sort_incremental_spill_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT bucket, name FROM sort_pairs ORDER BY bucket, name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 0);
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(),
              "bucket000_item00");
    EXPECT_EQ(result.resultSet()->getValue(31, 1).toString(),
              "bucket000_item31");
    EXPECT_EQ(result.resultSet()->getValue(32, 0).toInt64(), 1);
    EXPECT_EQ(result.resultSet()->getValue(result.resultSet()->rowCount() - 1,
                                           1)
                  .toString(),
              "bucket127_item31");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE sort mode=PREFIX_1_INCREMENTAL_RUNS spill=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("prefix_order_keys=1"), std::string::npos) << trace;
    EXPECT_EQ(trace.find("incremental_runs=0"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       IncrementalSortUsesDeepPrefixOrderedInputInMemory)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL(
            "CREATE TABLE sort_triples (bucket INTEGER, shard INTEGER, name TEXT)")
            .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int bucket = 0; bucket < 4; ++bucket)
    {
        for (int shard = 0; shard < 4; ++shard)
        {
            for (int item = 7; item >= 0; --item)
            {
                std::ostringstream name;
                name << "bucket" << std::setw(2) << std::setfill('0') << bucket
                     << "_shard" << std::setw(2) << std::setfill('0') << shard
                     << "_item" << std::setw(2) << std::setfill('0') << item;
                ASSERT_TRUE(
                    executeSQL(
                        "INSERT INTO sort_triples (bucket, shard, name) VALUES (" +
                        std::to_string(bucket) + ", " +
                        std::to_string(shard) + ", '" + name.str() + "')")
                        .success());
            }
        }
    }
    ASSERT_TRUE(executeSQL("ANALYZE sort_triples").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_sort_prefix2_in_memory_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT bucket, shard, name FROM sort_triples ORDER BY bucket, shard, name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 128u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 0);
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toInt64(), 0);
    EXPECT_EQ(result.resultSet()->getValue(0, 2).toString(),
              "bucket00_shard00_item00");
    EXPECT_EQ(result.resultSet()->getValue(7, 2).toString(),
              "bucket00_shard00_item07");
    EXPECT_EQ(result.resultSet()->getValue(8, 1).toInt64(), 1);
    EXPECT_EQ(result.resultSet()->getValue(8, 2).toString(),
              "bucket00_shard01_item00");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE sort mode=PREFIX_2_INCREMENTAL spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("prefix_order_keys=2"), std::string::npos) << trace;
    EXPECT_NE(trace.find("incremental_groups=16"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledSortUsesDeepPrefixRunOrderingOnPrefixOrderedInput)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL(
            "CREATE TABLE sort_triples (bucket INTEGER, shard INTEGER, name TEXT)")
            .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int bucket = 0; bucket < 64; ++bucket)
    {
        for (int shard = 0; shard < 8; ++shard)
        {
            for (int item = 7; item >= 0; --item)
            {
                std::ostringstream name;
                name << "bucket" << std::setw(3) << std::setfill('0') << bucket
                     << "_shard" << std::setw(2) << std::setfill('0') << shard
                     << "_item" << std::setw(2) << std::setfill('0') << item;
                ASSERT_TRUE(
                    executeSQL(
                        "INSERT INTO sort_triples (bucket, shard, name) VALUES (" +
                        std::to_string(bucket) + ", " +
                        std::to_string(shard) + ", '" + name.str() + "')")
                        .success());
            }
        }
    }
    ASSERT_TRUE(executeSQL("ANALYZE sort_triples").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_sort_prefix2_spill_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT bucket, shard, name FROM sort_triples ORDER BY bucket, shard, name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 0);
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toInt64(), 0);
    EXPECT_EQ(result.resultSet()->getValue(0, 2).toString(),
              "bucket000_shard00_item00");
    EXPECT_EQ(result.resultSet()->getValue(7, 2).toString(),
              "bucket000_shard00_item07");
    EXPECT_EQ(result.resultSet()->getValue(8, 1).toInt64(), 1);
    EXPECT_EQ(result.resultSet()->getValue(result.resultSet()->rowCount() - 1,
                                           2)
                  .toString(),
              "bucket063_shard07_item07");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE sort mode=PREFIX_2_INCREMENTAL_RUNS spill=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("prefix_order_keys=2"), std::string::npos) << trace;
    EXPECT_EQ(trace.find("incremental_runs=0"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForWindow)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 16384; ++i)
    {
        const int reversed = 16385 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'window" +
                               std::to_string(reversed) + "', 'window" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT ROW_NUMBER() OVER (ORDER BY name) FROM users";
    uint64_t baseline_budget = 0;
    auto baseline_bytecode = compileSQL(sql);
    if (!baseline_bytecode.empty())
    {
        scratchbird::optimizer::RuntimePlan baseline_plan;
        ASSERT_TRUE(decodeRuntimePlan(baseline_bytecode, baseline_plan));
        EXPECT_EQ(baseline_plan.root.node_type, "Window");
        baseline_budget = baseline_plan.root.memory_budget_bytes;
    }
    else
    {
        EXPECT_NE(last_compile_errors_.find(
                      "Window operator exceeds work_mem under spill-disallow policy"),
                  std::string::npos);
    }

    seedMemoryGrantFeedback(sql,
                            "WINDOW",
                            256ULL * 1024ULL * 1024ULL,
                            128ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL);

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Window");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_WINDOW");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    const uint64_t expected_budget = std::stoull(feedback_budget->value);
    EXPECT_GT(expected_budget, 64ULL * 1024ULL);
    if (baseline_budget > 0)
    {
        EXPECT_GT(expected_budget, baseline_budget);
    }
    EXPECT_EQ(plan.root.memory_budget_bytes, expected_budget);
    EXPECT_FALSE(plan.root.spill_expected);

    bool found_feedback_trace = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "WINDOW" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
            break;
        }
    }
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedHashAggregatePersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 32768; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'aggpersist" +
                               std::to_string(i) + "', 'aggpersist" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(i) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT age, COUNT(*) FROM users GROUP BY age";
    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "HASH_AGG", feedback));
    EXPECT_EQ(feedback.operator_kind, "HASH_AGG");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GT(feedback.p90_bytes, 0u);
    EXPECT_GE(feedback.peak_bytes, feedback.p90_bytes);
    EXPECT_EQ(feedback.state, "WARMING");

    QueryCompilerV3::invalidateAllPlanCache();
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_HASH_AGG");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    EXPECT_EQ(plan.root.memory_budget_bytes, std::stoull(feedback_budget->value));
    EXPECT_FALSE(plan.root.spill_expected);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedHashAggregateCapturesActualSpillOnStaleBytecode)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "128KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'staleagg" +
                               std::to_string(i) + "', 'staleagg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(i) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT age, COUNT(*) FROM users GROUP BY age";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    EXPECT_EQ(stale_plan.root.node_type, "Aggregate");
    EXPECT_FALSE(stale_plan.root.spill_expected);

    for (int i = 65; i <= 4096; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'staleagg" +
                               std::to_string(i) + "', 'staleagg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(i) + ")")
                        .success());
    }

    auto result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "HASH_AGG", feedback));
    EXPECT_EQ(feedback.operator_kind, "HASH_AGG");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GE(feedback.spill_count, 1u);
    EXPECT_EQ(feedback.state, "WARMING");
}

TEST_F(QueryPlannerIntegrationTest,
       HashAggregateUsesFastScalarGroupKeysForAdmittedIntegerGrouping)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        const int age = 20 + (i % 4);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'fastagg" +
                               std::to_string(i) + "', 'fastagg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(age) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_hash_agg_fast_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result =
        executeSQL("SELECT age, COUNT(*) FROM users GROUP BY age");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 4u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_SCALAR spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("groups=4"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedIntegerGrouping)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 2048; ++i)
    {
        const int age = 100 + (i % 256);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'spillfastagg" +
                               std::to_string(i) + "', 'spillfastagg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(age) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_hash_agg_spill_fast_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result =
        executeSQL("SELECT age, COUNT(*) FROM users GROUP BY age");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 256u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_SCALAR spill=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("groups=256"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       HashAggregateUsesFastCompositeGroupKeysForAdmittedIntegerGrouping)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE agg_pairs (bucket INTEGER, shard INTEGER)")
            .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 0; i < 96; ++i)
    {
        const int bucket = i % 4;
        const int shard = i % 3;
        ASSERT_TRUE(
            executeSQL("INSERT INTO agg_pairs (bucket, shard) VALUES (" +
                       std::to_string(bucket) + ", " +
                       std::to_string(shard) + ")")
                .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE agg_pairs").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_hash_agg_fast_composite_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT bucket, shard, COUNT(*) FROM agg_pairs GROUP BY bucket, shard");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 12u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_COMPOSITE spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("groups=12"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledHashAggregateUsesFastCompositeGroupKeysForAdmittedIntegerGrouping)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE agg_pairs (bucket INTEGER, shard INTEGER)")
            .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 0; i < 4096; ++i)
    {
        const int bucket = i % 256;
        const int shard = (i / 256) % 4;
        ASSERT_TRUE(
            executeSQL("INSERT INTO agg_pairs (bucket, shard) VALUES (" +
                       std::to_string(bucket) + ", " +
                       std::to_string(shard) + ")")
                .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE agg_pairs").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_hash_agg_spill_fast_composite_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT bucket, shard, COUNT(*) FROM agg_pairs GROUP BY bucket, shard");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1024u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_COMPOSITE spill=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("groups=1024"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       HashAggregateUsesFastScalarGroupKeysForAdmittedTextGrouping)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE agg_labels (label TEXT)")
            .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    const std::array<const char*, 4> labels = {"alpha", "beta", "gamma", "delta"};
    for (int i = 0; i < 96; ++i)
    {
        ASSERT_TRUE(
            executeSQL("INSERT INTO agg_labels (label) VALUES ('" +
                       std::string(labels[static_cast<size_t>(i % 4)]) +
                       "')")
                .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE agg_labels").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_hash_agg_fast_text_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT label, COUNT(*) FROM agg_labels GROUP BY label");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 4u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_SCALAR spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("groups=4"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedTextGrouping)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE agg_labels (label TEXT)")
            .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 0; i < 4096; ++i)
    {
        std::ostringstream label;
        label << "label" << std::setw(3) << std::setfill('0') << (i % 256);
        ASSERT_TRUE(
            executeSQL("INSERT INTO agg_labels (label) VALUES ('" +
                       label.str() + "')")
                .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE agg_labels").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_hash_agg_spill_fast_text_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT label, COUNT(*) FROM agg_labels GROUP BY label");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 256u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_SCALAR spill=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("groups=256"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       HashAggregateUsesFastScalarGroupKeysForAdmittedScalarExpressionGrouping)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "256KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 128; ++i)
    {
        const int age = 20 + (i % 8);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'expragg" +
                               std::to_string(i) + "', 'expragg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(age) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_hash_agg_fast_expr_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT age > 23, COUNT(*) FROM users GROUP BY age > 23");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 2u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_SCALAR spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("groups=2"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledHashAggregateUsesFastScalarGroupKeysForAdmittedScalarExpressionGrouping)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 4096; ++i)
    {
        const int age = 100 + (i % 256);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'exprspill" +
                               std::to_string(i) + "', 'exprspill" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(age) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_hash_agg_spill_fast_expr_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT age > 227, COUNT(*) FROM users GROUP BY age > 227");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 2u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_SCALAR spill=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("groups=2"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       DistinctUsesFastScalarKeysForAdmittedIntegerProjection)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 96; ++i)
    {
        const int age = 20 + (i % 4);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'distinctfast" +
                               std::to_string(i) + "', 'distinctfast" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(age) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_distinct_fast_scalar_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL("SELECT DISTINCT age FROM users");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 4u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find("SELECT TRACE distinct key_mode=FAST_SCALAR spill=0"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("output_rows=4"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledDistinctUsesFastScalarKeysForAdmittedIntegerProjection)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 4096; ++i)
    {
        const int age = 100 + (i % 256);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'distinctspill" +
                               std::to_string(i) + "', 'distinctspill" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(age) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_distinct_fast_scalar_spill_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL("SELECT DISTINCT age FROM users");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 256u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find("SELECT TRACE distinct key_mode=FAST_SCALAR spill=1"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("output_rows=256"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       DistinctUsesFastCompositeKeysForAdmittedCompositeProjection)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(
        executeSQL("CREATE TABLE distinct_pairs (bucket INTEGER, shard INTEGER)")
            .success());

    for (int i = 0; i < 96; ++i)
    {
        const int bucket = i % 4;
        const int shard = (i / 4) % 3;
        ASSERT_TRUE(
            executeSQL("INSERT INTO distinct_pairs (bucket, shard) VALUES (" +
                       std::to_string(bucket) + ", " +
                       std::to_string(shard) + ")")
                .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE distinct_pairs").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_distinct_fast_composite_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result =
        executeSQL("SELECT DISTINCT bucket, shard FROM distinct_pairs");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 12u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find("SELECT TRACE distinct key_mode=FAST_COMPOSITE spill=0"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("output_rows=12"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       SpilledDistinctUsesFastCompositeKeysForAdmittedCompositeProjection)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(
        executeSQL("CREATE TABLE distinct_pairs (bucket INTEGER, shard INTEGER)")
            .success());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 0; i < 4096; ++i)
    {
        const int bucket = i % 64;
        const int shard = (i / 64) % 8;
        ASSERT_TRUE(
            executeSQL("INSERT INTO distinct_pairs (bucket, shard) VALUES (" +
                       std::to_string(bucket) + ", " +
                       std::to_string(shard) + ")")
                .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE distinct_pairs").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_distinct_fast_composite_spill_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result =
        executeSQL("SELECT DISTINCT bucket, shard FROM distinct_pairs");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 512u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find("SELECT TRACE distinct key_mode=FAST_COMPOSITE spill=1"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("output_rows=512"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedHashDistinctPersistsDurableMemoryGrantFeedbackForFutureSpillDisallowCompile)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 32768; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'distinctpersist" +
                               std::to_string(i) + "', 'distinctpersist" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(i) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT DISTINCT age FROM users";
    auto baseline_bytecode = compileSQL(sql);
    ASSERT_FALSE(baseline_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan baseline_plan;
    ASSERT_TRUE(decodeRuntimePlan(baseline_bytecode, baseline_plan));
    EXPECT_EQ(baseline_plan.root.node_type, "Aggregate");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "HASH_AGG", feedback));
    EXPECT_EQ(feedback.operator_kind, "HASH_AGG");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GT(feedback.p90_bytes, 0u);
    EXPECT_GE(feedback.peak_bytes, feedback.p90_bytes);
    if (baseline_plan.root.spill_expected)
    {
        EXPECT_GE(feedback.spill_count, 1u);
    }
    EXPECT_EQ(feedback.state, "WARMING");

    QueryCompilerV3::invalidateAllPlanCache();
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_HASH_AGG");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    EXPECT_EQ(plan.root.memory_budget_bytes, std::stoull(feedback_budget->value));
    EXPECT_FALSE(plan.root.spill_expected);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecutedHashDistinctCapturesActualSpillOnStaleBytecode)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "128KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 64; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'staledistinct" +
                               std::to_string(i) + "', 'staledistinct" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(i) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT DISTINCT age FROM users";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    EXPECT_EQ(stale_plan.root.node_type, "Aggregate");
    EXPECT_FALSE(stale_plan.root.spill_expected);

    for (int i = 65; i <= 4096; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'staledistinct" +
                               std::to_string(i) + "', 'staledistinct" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(i) + ")")
                        .success());
    }

    auto result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 4096u);

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "HASH_AGG", feedback));
    EXPECT_EQ(feedback.operator_kind, "HASH_AGG");
    EXPECT_GE(feedback.sample_count, 1u);
    EXPECT_GT(feedback.last_grant_bytes, 0u);
    EXPECT_GE(feedback.spill_count, 1u);
    EXPECT_EQ(feedback.state, "WARMING");
}

TEST_F(QueryPlannerIntegrationTest,
       ExecuteBytecodeRejectsPreviouslyCompiledSpillPlanWhenLiveSpillPolicyDisallows)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 16384; ++i)
    {
        const int reversed = 16385 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'stalespill" +
                               std::to_string(reversed) + "', 'stalespill" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    EXPECT_EQ(stale_plan.root.node_type, "Sort");
    EXPECT_TRUE(stale_plan.root.spill_expected);

    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    auto stale_result = executeBytecode(stale_bytecode);
    EXPECT_FALSE(stale_result.success());
    EXPECT_NE(stale_result.error().find("spill-expected operators"),
              std::string::npos)
        << stale_result.error();

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "SORT", feedback));
    EXPECT_EQ(feedback.operator_kind, "SORT");
    EXPECT_GE(feedback.cancel_count, 1u);
    EXPECT_GE(feedback.last_grant_bytes, stale_plan.root.memory_budget_bytes);
}

TEST_F(QueryPlannerIntegrationTest,
       ExecuteBytecodeRejectsPlanWhoseReservationExceedsLiveWorkMemUnderSpillDisallow)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 16384; ++i)
    {
        const int reversed = 16385 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'staleres" +
                               std::to_string(reversed) + "', 'staleres" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    EXPECT_EQ(stale_plan.root.node_type, "Sort");
    EXPECT_FALSE(stale_plan.root.spill_expected);

    const auto peak_budget =
        [&]() -> uint64_t {
            std::function<uint64_t(const scratchbird::optimizer::RuntimePlanNode&)> visit =
                [&](const scratchbird::optimizer::RuntimePlanNode& node) -> uint64_t {
                    uint64_t peak = node.memory_budget_bytes;
                    for (const auto& child : node.children)
                    {
                        peak = std::max(peak, visit(child));
                    }
                    return peak;
                };
            return visit(stale_plan.root);
        }();
    ASSERT_GT(peak_budget, 4ULL * 1024ULL);

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    auto stale_result = executeBytecode(stale_bytecode);
    EXPECT_FALSE(stale_result.success());
    EXPECT_NE(stale_result.error().find("exceeds live work_mem"),
              std::string::npos)
        << stale_result.error();

    CatalogManager::MemoryGrantFeedbackCatalogInfo feedback{};
    ASSERT_TRUE(loadMemoryGrantFeedback(sql, "SORT", feedback));
    EXPECT_EQ(feedback.operator_kind, "SORT");
    EXPECT_GE(feedback.cancel_count, 1u);
    EXPECT_GE(feedback.last_grant_bytes, peak_budget);
}

TEST_F(QueryPlannerIntegrationTest,
       SpillPolicyDisallowChoosesOrderedGroupAggregateWhenAvailable)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 16384; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(i) +
                               ", 10.0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT user_id, COUNT(*) "
        "FROM orders "
        "WHERE user_id <= 4096 "
        "GROUP BY user_id "
        "ORDER BY user_id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry& entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:group_by" &&
                                entry.candidate == "GROUP_ORDER_REUSE" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry& entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:aggregate" &&
                                entry.candidate == "HASH_AGGREGATE" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());
    EXPECT_TRUE(rejected_it->reason.find("spill policy disallows hash aggregate temp spill") !=
                    std::string::npos ||
                rejected_it->reason.find("ordered/group aggregate is cheaper") !=
                    std::string::npos)
        << rejected_it->reason;
}

TEST_F(QueryPlannerIntegrationTest,
       SpillPolicyDisallowUsesDurableMemoryGrantFeedbackForHashDistinct)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 32768; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'distinct" +
                               std::to_string(i) + "', 'distinct" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(i) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT DISTINCT age FROM users";
    uint64_t baseline_budget = 0;
    auto baseline_bytecode = compileSQL(sql);
    if (!baseline_bytecode.empty())
    {
        scratchbird::optimizer::RuntimePlan baseline_plan;
        ASSERT_TRUE(decodeRuntimePlan(baseline_bytecode, baseline_plan));
        EXPECT_EQ(baseline_plan.root.node_type, "Aggregate");
        baseline_budget = baseline_plan.root.memory_budget_bytes;
    }
    else
    {
        EXPECT_NE(last_compile_errors_.find(
                      "Distinct operator exceeds work_mem under spill-disallow policy"),
                  std::string::npos);
    }

    seedMemoryGrantFeedback(sql,
                            "HASH_AGG",
                            256ULL * 1024ULL * 1024ULL,
                            128ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL,
                            192ULL * 1024ULL * 1024ULL);

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");

    const auto* feedback_budget =
        findOptimizerControl(plan, "MEMORY_GRANT_FEEDBACK_HASH_AGG");
    ASSERT_NE(feedback_budget, nullptr);
    EXPECT_EQ(feedback_budget->source, "CATALOG");
    const uint64_t expected_budget = std::stoull(feedback_budget->value);
    EXPECT_GT(expected_budget, 64ULL * 1024ULL);
    if (baseline_budget > 0)
    {
        EXPECT_GT(expected_budget, baseline_budget);
    }
    EXPECT_EQ(plan.root.memory_budget_bytes, expected_budget);
    EXPECT_FALSE(plan.root.spill_expected);

    bool found_feedback_trace = false;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.phase == "MEMORY_GRANT_FEEDBACK" &&
            entry.candidate == "HASH_AGG" &&
            entry.verdict == "APPLIED")
        {
            found_feedback_trace = true;
            break;
        }
    }
    EXPECT_TRUE(found_feedback_trace) << formatTraceEntries(plan.considered_paths);
}

TEST_F(QueryPlannerIntegrationTest,
       SpillPolicyDisallowChoosesOrderedDistinctWhenAvailable)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());
    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 16384; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(i) +
                               ", 10.0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT DISTINCT user_id FROM orders WHERE user_id <= 4096 ORDER BY user_id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry& entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:distinct" &&
                                entry.candidate == "ORDERED_DISTINCT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());
    EXPECT_TRUE(chosen_it->reason.find("spill policy disallows hash distinct temp spill") !=
                    std::string::npos ||
                chosen_it->reason.find("existing order is sufficient for streaming DISTINCT") !=
                    std::string::npos)
        << chosen_it->reason;

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry& entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:distinct" &&
                                entry.candidate == "HASH_DISTINCT" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());
    EXPECT_TRUE(rejected_it->reason.find("spill policy disallows hash distinct temp spill") !=
                    std::string::npos ||
                rejected_it->reason.find("ordered distinct is cheaper on the current input order") !=
                    std::string::npos)
        << rejected_it->reason;
}

TEST_F(QueryPlannerIntegrationTest, MergeJoinPlanExecutesAndPreservesRuntimeMetadata)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_products_id ON products (id)").success());

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + i) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + i) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());
    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "MERGE_JOIN");

    const std::string sql =
        "SELECT users.id "
        "FROM users JOIN products ON users.id = products.id "
        "WHERE users.id = 2 AND products.id = 2";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_TRUE(plan.join_steps.front().has_merge_keys);
    EXPECT_EQ(plan.join_steps.front().left_merge_key.column_name, "id");
    EXPECT_EQ(plan.join_steps.front().right_merge_key.column_name, "id");
    EXPECT_EQ(plan.root.node_type, "MergeJoin");
    ASSERT_EQ(plan.root.children.size(), 2u);
    for (const auto& child : plan.root.children)
    {
        if (child.node_type == "Sort")
        {
            ASSERT_EQ(child.children.size(), 1u);
            EXPECT_FALSE(child.children.front().node_type.empty());
        }
    }

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows.front(), "2");
}

TEST_F(QueryPlannerIntegrationTest, ForcedMergeJoinUsesExplicitSortToMergeCandidate)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + i) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + i) + ".0)")
                        .success());
    }

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "MERGE_JOIN");

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_TRUE(plan.join_steps.front().has_merge_keys);
    EXPECT_FALSE(plan.join_steps.front().merge_outer_presorted);
    EXPECT_FALSE(plan.join_steps.front().merge_inner_presorted);
    ASSERT_EQ(plan.join_steps.front().method_enablers.size(), 2u);
    EXPECT_EQ(plan.join_steps.front().method_enablers[0], "SORT_OUTER");
    EXPECT_EQ(plan.join_steps.front().method_enablers[1], "SORT_INNER");
    ASSERT_EQ(plan.root.node_type, "MergeJoin");
    ASSERT_EQ(plan.root.children.size(), 2u);
    EXPECT_EQ(plan.root.children[0].node_type, "Sort");
    EXPECT_EQ(plan.root.children[1].node_type, "Sort");
    ASSERT_EQ(plan.root.children[0].children.size(), 1u);
    ASSERT_EQ(plan.root.children[1].children.size(), 1u);
    EXPECT_EQ(plan.root.children[0].children[0].node_type, "SeqScan");
    EXPECT_EQ(plan.root.children[1].children[0].node_type, "SeqScan");
}

TEST_F(QueryPlannerIntegrationTest, ForcedHashJoinFailsClosedOnNonEquiJoin)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "HASH_JOIN");

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id > products.id");
    EXPECT_TRUE(bytecode.empty());
    EXPECT_NE(last_compile_errors_.find("JOIN_METHOD_NON_EQUI"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonFormatsRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    auto result = executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) "
                             "SELECT id FROM users WHERE id = 42");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines.front().find("\"plan_hash\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"options\":["), std::string::npos);
    EXPECT_NE(lines.front().find("\"VERBOSE\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan_root\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"node_type\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"cache_mode\":\"GENERIC\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan_profile_signature\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"runtime_plan_contract\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"join_graph\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"optimizer_trace\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"diagnostics_contract\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"search_summary\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"considered_paths\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"rejected_paths\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"statistics_provenance\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"adaptive_feedback\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"analyze\":{\"rows\":"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, ExplainAnalyzeJsonPublishesPerNodeActuals)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(1, 'alice', 'a@example.com', 26)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(2, 'bob', 'b@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(3, 'carol', 'c@example.com', 26)")
                    .success());

    auto result = executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) "
                             "SELECT id FROM users WHERE age = 26 "
                             "ORDER BY name LIMIT 1");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(lines.front());

    ASSERT_TRUE(parsed.contains("plan_root"));
    const auto& plan_root = parsed.at("plan_root");
    EXPECT_TRUE(plan_root.at("actuals_available").get<bool>());
    EXPECT_TRUE(plan_root.contains("actual_rows"));
    EXPECT_TRUE(plan_root.contains("rows_examined"));
    EXPECT_TRUE(plan_root.contains("rows_filtered"));
    EXPECT_TRUE(plan_root.contains("loop_count"));

    std::function<const nlohmann::json*(const nlohmann::json&)> find_scan_node;
    find_scan_node = [&](const nlohmann::json& node) -> const nlohmann::json* {
        const std::string node_type = node.value("node_type", "");
        if (node_type == "SeqScan" || node_type == "IndexScan" ||
            node_type == "IndexOnlyScan" || node_type == "BitmapIndexScan")
        {
            return &node;
        }
        const auto children_it = node.find("children");
        if (children_it == node.end() || !children_it->is_array())
        {
            return nullptr;
        }
        for (const auto& child : *children_it)
        {
            if (const auto* match = find_scan_node(child))
            {
                return match;
            }
        }
        return nullptr;
    };

    const auto* scan_node = find_scan_node(plan_root);
    ASSERT_NE(scan_node, nullptr);
    EXPECT_TRUE(scan_node->at("actuals_available").get<bool>());
    EXPECT_TRUE(scan_node->contains("actual_rows"));
    EXPECT_TRUE(scan_node->contains("rows_examined"));
    EXPECT_TRUE(scan_node->contains("rows_filtered"));
}

TEST_F(QueryPlannerIntegrationTest,
       ExplainAnalyzeJsonPublishesAdvisorRecommendationsWithProvenance)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(1, 'alice', 'a@example.com', 26)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(2, 'bob', 'b@example.com', 31)")
                    .success());

    auto result = executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) "
                             "SELECT u.id FROM users u "
                             "WHERE u.age = 26 ORDER BY u.name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(lines.front());

    ASSERT_TRUE(parsed.contains("optimizer_trace"));
    const auto& optimizer_trace = parsed.at("optimizer_trace");
    ASSERT_TRUE(optimizer_trace.contains("advisor_signals"));
    ASSERT_TRUE(optimizer_trace.contains("advisor_recommendations"));
    ASSERT_TRUE(optimizer_trace.at("advisor_signals").is_array());
    ASSERT_TRUE(optimizer_trace.at("advisor_recommendations").is_array());
    EXPECT_FALSE(optimizer_trace.at("advisor_signals").empty());
    EXPECT_FALSE(optimizer_trace.at("advisor_recommendations").empty());

    const auto& recommendation =
        optimizer_trace.at("advisor_recommendations").front();
    EXPECT_EQ(recommendation.at("rank").get<uint32_t>(), 1u);
    EXPECT_FALSE(recommendation.at("recommendation_type").get<std::string>().empty());
    EXPECT_FALSE(recommendation.at("provenance_source").get<std::string>().empty());
    EXPECT_FALSE(recommendation.at("query_fingerprint").get<std::string>().empty());
    ASSERT_TRUE(recommendation.at("signal_names").is_array());
    EXPECT_TRUE(recommendation.at("what_if_replanned").get<bool>());
    EXPECT_FALSE(recommendation.at("baseline_access_family").get<std::string>().empty());
    EXPECT_FALSE(recommendation.at("hypothetical_access_family").get<std::string>().empty());
    EXPECT_GT(recommendation.at("baseline_total_cost").get<double>(),
              recommendation.at("hypothetical_total_cost").get<double>());
    EXPECT_GT(recommendation.at("estimated_speedup_ratio").get<double>(), 1.0);
    EXPECT_FALSE(recommendation.at("evidence_detail").get<std::string>().empty());

    ASSERT_TRUE(optimizer_trace.contains("statistics_provenance"));
    const auto& statistics_provenance =
        optimizer_trace.at("statistics_provenance");
    const auto advisor_stats_it = std::find_if(
        statistics_provenance.begin(),
        statistics_provenance.end(),
        [](const auto& entry) {
            return entry.value("source", std::string()) == "ADVISOR_FEEDBACK";
        });
    EXPECT_NE(advisor_stats_it, statistics_provenance.end());
}

TEST_F(QueryPlannerIntegrationTest, CompiledRuntimePlanCarriesAdvisorRecommendations)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT u.id FROM users u WHERE u.age = 26 ORDER BY u.name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_FALSE(plan.advisor_signals.empty());
    EXPECT_FALSE(plan.advisor_recommendations.empty());
    ASSERT_FALSE(plan.advisor_recommendations.empty());
    const auto& recommendation = plan.advisor_recommendations.front();
    EXPECT_EQ(recommendation.rank, 1u);
    EXPECT_FALSE(recommendation.recommendation_type.empty());
    EXPECT_EQ(recommendation.provenance_source, "INDEX_ADVISOR");
    EXPECT_FALSE(recommendation.query_fingerprint.empty());
    EXPECT_TRUE(recommendation.what_if_replanned);
    EXPECT_FALSE(recommendation.baseline_access_family.empty());
    EXPECT_FALSE(recommendation.hypothetical_access_family.empty());
    EXPECT_GT(recommendation.baseline_total_cost,
              recommendation.hypothetical_total_cost);
    EXPECT_GT(recommendation.estimated_speedup_ratio, 1.0);
    EXPECT_FALSE(recommendation.evidence_detail.empty());
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonPublishesFormulaProfileAndExpandedCostTerms)
{
    ASSERT_TRUE(createDatabase());

    auto result = executeSQL("EXPLAIN (FORMAT JSON) "
                             "SELECT id FROM users WHERE id = 42");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(lines.front());

    ASSERT_TRUE(parsed.contains("plan_root"));
    const auto &plan_root = parsed.at("plan_root");
    EXPECT_TRUE(plan_root.contains("formula_profile_id"));
    EXPECT_TRUE(plan_root.contains("formula_profile_version"));
    EXPECT_TRUE(plan_root.contains("calibration_profile_id"));
    EXPECT_TRUE(plan_root.contains("resource_governance_outcome"));
    EXPECT_TRUE(plan_root.contains("input_estimates"));
    EXPECT_TRUE(plan_root.contains("expanded_cost_terms"));
    ASSERT_TRUE(plan_root.at("input_estimates").is_array());
    ASSERT_TRUE(plan_root.at("expanded_cost_terms").is_array());
    EXPECT_FALSE(plan_root.at("formula_profile_id").get<std::string>().empty());
    EXPECT_GT(plan_root.at("formula_profile_version").get<uint32_t>(), 0u);
    EXPECT_FALSE(plan_root.at("calibration_profile_id").get<std::string>().empty());
    EXPECT_FALSE(plan_root.at("input_estimates").empty());
    EXPECT_FALSE(plan_root.at("expanded_cost_terms").empty());
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonPublishesJoinGraphContractFields)
{
    ASSERT_TRUE(createDatabase());

    auto result = executeSQL("EXPLAIN (FORMAT JSON) "
                             "SELECT users.id FROM users LEFT JOIN products ON users.id = products.id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);

    const auto parsed = nlohmann::json::parse(lines.front());
    EXPECT_EQ(parsed.at("runtime_plan_contract").get<std::string>(),
              scratchbird::optimizer::kRuntimePlanContractId);

    const auto& join_graph = parsed.at("join_graph");
    EXPECT_EQ(join_graph.at("contract").get<std::string>(),
              scratchbird::optimizer::kJoinGraphContractId);
    ASSERT_TRUE(join_graph.at("relations").is_array());
    ASSERT_TRUE(join_graph.at("join_steps").is_array());
    ASSERT_EQ(join_graph.at("relations").size(), 2u);
    ASSERT_EQ(join_graph.at("join_steps").size(), 1u);
    EXPECT_TRUE(join_graph.at("relations")[0].contains("scan_family"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("path_name"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("scan_family_kind"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("scan_family_tags"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("candidate_scan_families"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("exactness_class"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("visibility_enforcement"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("queryability_state"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("ordered_output"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("ordered_prefix_length"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("required_outer_relation_aliases"));
    EXPECT_EQ(join_graph.at("join_steps")[0].at("join_edge_left_alias").get<std::string>(),
              "users");
    EXPECT_EQ(join_graph.at("join_steps")[0].at("join_edge_right_alias").get<std::string>(),
              "products");
    EXPECT_TRUE(join_graph.at("join_steps")[0].contains("legal_method_families"));
    EXPECT_TRUE(join_graph.at("join_steps")[0].contains("method_enablers"));
    EXPECT_TRUE(join_graph.at("join_steps")[0].at("outer_reorder_barrier").get<bool>());

    const auto& optimizer_trace = parsed.at("optimizer_trace");
    EXPECT_EQ(optimizer_trace.at("diagnostics_contract").get<std::string>(),
              scratchbird::optimizer::kOptimizerDiagnosticsContractId);
    const auto& search_summary = optimizer_trace.at("search_summary");
    EXPECT_TRUE(search_summary.contains("requested_strategy"));
    EXPECT_TRUE(search_summary.contains("selected_strategy"));
    EXPECT_TRUE(search_summary.contains("considered_state_count"));
    EXPECT_TRUE(search_summary.contains("pruned_state_count"));
    EXPECT_TRUE(search_summary.contains("pair_evaluation_count"));
    EXPECT_TRUE(search_summary.contains("rejected_candidate_count"));
    EXPECT_TRUE(search_summary.contains("max_pair_evaluations"));
    EXPECT_TRUE(search_summary.contains("max_states_considered"));
    EXPECT_TRUE(search_summary.contains("fallback_threshold_name"));
    EXPECT_TRUE(search_summary.contains("fallback_threshold_value"));
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonIncludesOperatorMemoryAndSpillMetadata)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");
    for (int i = 1; i <= 2000; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
    }

    auto result = executeSQL("EXPLAIN (FORMAT JSON) "
                             "SELECT name FROM users ORDER BY name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines.front().find("\"estimated_memory_bytes\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"memory_budget_bytes\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"spill_expected\":true"), std::string::npos);
    EXPECT_NE(lines.front().find("\"spill_policy\":\"ALLOW\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"optimizer_controls\":"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, ReorderedJoinPayloadKeepsOriginalRelationIndexes)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_products_id ON products (id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (42, 'widget', 12.5)")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id "
        "WHERE products.id = 42";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.relations.front().source_relation_index, 1u);

    sblr_v3::Instruction select_inst;
    ASSERT_TRUE(decodeFirstSelect(bytecode, select_inst));
    const auto* select_payload = std::get_if<sblr_v3::Value::Object>(&select_inst.payload.data);
    ASSERT_NE(select_payload, nullptr);
    auto from_it = select_payload->find("from");
    ASSERT_NE(from_it, select_payload->end());
    const auto* from_obj = std::get_if<sblr_v3::Value::Object>(&from_it->second.data);
    ASSERT_NE(from_obj, nullptr);
    auto relation_index_it = from_obj->find("source_relation_index");
    ASSERT_NE(relation_index_it, from_obj->end());
    const auto* relation_index = std::get_if<uint64_t>(&relation_index_it->second.data);
    ASSERT_NE(relation_index, nullptr);
    EXPECT_EQ(*relation_index, 1u);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows.front(), "42");
}

TEST_F(QueryPlannerIntegrationTest, OptimizerParityBaselineCorpusCapturesStableSummary)
{
    ASSERT_TRUE(createDatabase());

    constexpr size_t kBenchmarkSamples = 7;

    const std::vector<std::string> setup_sql = {
        "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)",
        "INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 31)",
        "INSERT INTO users (id, name, email, age) VALUES (3, 'carol', 'c@example.com', 32)",
        "INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)",
        "INSERT INTO products (id, name, price) VALUES (2, 'p2', 11.5)",
        "INSERT INTO orders (id, user_id, amount) VALUES (1, 1, 12.0)",
        "INSERT INTO orders (id, user_id, amount) VALUES (2, 1, 24.0)",
        "INSERT INTO orders (id, user_id, amount) VALUES (3, 2, 36.0)",
        "INSERT INTO test (id) VALUES (10)",
        "INSERT INTO test (id) VALUES (20)",
        "CREATE INDEX idx_users_id ON users (id)",
        "CREATE INDEX idx_products_id ON products (id)",
        "CREATE INDEX idx_orders_user_id ON orders (user_id)"
    };

    for (const auto& sql : setup_sql)
    {
        ASSERT_TRUE(executeSQL(sql).success()) << sql;
    }

    struct BaselineQuery
    {
        std::string id;
        std::string sql;
    };

    const std::vector<BaselineQuery> corpus = {
        {"single_relation_filter",
         "SELECT id FROM users WHERE id = 1"},
        {"left_join_barrier",
         "SELECT users.id FROM users LEFT JOIN products ON users.id = products.id"},
        {"disconnected_cross_bridge",
         "SELECT users.id FROM users CROSS JOIN test JOIN products ON users.id = products.id"},
        {"ordered_inner_join",
         "SELECT users.id FROM users JOIN products ON users.id = products.id ORDER BY users.id"},
        {"grouped_aggregate",
         "SELECT user_id FROM orders GROUP BY user_id ORDER BY user_id"},
    };

    nlohmann::json baseline;
    baseline["schema"] = "scratchbird.optimizer_parity.baseline.v1";
    baseline["query_count"] = corpus.size();
    baseline["queries"] = nlohmann::json::array();
    std::vector<std::string> parity_corpus_rows = {
        csvRow({"case_id",
                "root_node_type",
                "selected_strategy",
                "relation_count",
                "join_count",
                "estimated_root_rows",
                "actual_rows",
                "misestimate_ratio",
                "compile_p50_ms",
                "compile_p95_ms",
                "compile_p99_ms",
                "explain_p50_ms",
                "explain_p95_ms",
                "explain_p99_ms",
                "execute_p50_ms",
                "execute_p95_ms",
                "execute_p99_ms"})};

    for (const auto& entry : corpus)
    {
        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();

        auto bytecode = compileSQL(entry.sql);
        ASSERT_FALSE(bytecode.empty()) << entry.id << ": " << last_compile_errors_;

        scratchbird::optimizer::RuntimePlan plan;
        ASSERT_TRUE(decodeRuntimePlan(bytecode, plan)) << entry.id;

        auto explain_result =
            executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) " + entry.sql);
        ASSERT_TRUE(explain_result.success()) << entry.id << ": "
                                              << explain_result.error();
        ASSERT_TRUE(explain_result.hasResultSet()) << entry.id;
        const auto explain_lines = resultStrings(explain_result);
        ASSERT_EQ(explain_lines.size(), 1u) << entry.id;

        const auto explain_json = nlohmann::json::parse(explain_lines.front());

        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
        auto count_result =
            executeSQL("SELECT COUNT(*) FROM (" + entry.sql + ") AS baseline_count");
        ASSERT_TRUE(count_result.success()) << entry.id << ": "
                                            << count_result.error();
        ASSERT_TRUE(count_result.hasResultSet()) << entry.id;
        const auto count_rows = resultStrings(count_result);
        ASSERT_EQ(count_rows.size(), 1u) << entry.id;

        uint64_t actual_rows = 0;
        try
        {
            actual_rows = static_cast<uint64_t>(std::stoull(count_rows.front()));
        }
        catch (const std::exception&)
        {
            FAIL() << entry.id << ": invalid row-count payload '" << count_rows.front()
                   << "'";
        }
        const auto compile_samples_ms =
            sampleDurationsMs(kBenchmarkSamples, [&]() {
                optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
                optimizer::QueryProfiler::getInstance().clearProfiles();
                return !compileSQL(entry.sql).empty();
            });
        ASSERT_FALSE(compile_samples_ms.empty()) << entry.id;
        const double compile_mean_ms = meanOfSamplesMs(compile_samples_ms);
        ASSERT_GE(compile_mean_ms, 0.0) << entry.id;

        const auto explain_samples_ms =
            sampleDurationsMs(kBenchmarkSamples, [&]() {
                optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
                optimizer::QueryProfiler::getInstance().clearProfiles();
                auto result =
                    executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) " + entry.sql);
                return result.success() && result.hasResultSet();
            });
        ASSERT_FALSE(explain_samples_ms.empty()) << entry.id;
        const double explain_mean_ms = meanOfSamplesMs(explain_samples_ms);
        ASSERT_GE(explain_mean_ms, 0.0) << entry.id;

        const auto execute_samples_ms =
            sampleDurationsMs(kBenchmarkSamples, [&]() {
                optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
                optimizer::QueryProfiler::getInstance().clearProfiles();
                auto result = executeSQL(entry.sql);
                return result.success();
            });
        ASSERT_FALSE(execute_samples_ms.empty()) << entry.id;
        const double execute_mean_ms = meanOfSamplesMs(execute_samples_ms);
        ASSERT_GE(execute_mean_ms, 0.0) << entry.id;

        nlohmann::json join_steps = nlohmann::json::array();
        for (const auto& join_step : plan.join_steps)
        {
            join_steps.push_back(normalizedRuntimeJoinStep(join_step));
        }

        nlohmann::json query_summary;
        query_summary["id"] = entry.id;
        query_summary["sql"] = entry.sql;
        query_summary["runtime_plan_contract"] = plan.contract_id;
        query_summary["join_graph_contract"] = plan.join_graph_contract_id;
        query_summary["diagnostics_contract"] = plan.diagnostics_contract_id;
        query_summary["cache_mode"] = plan.cache_mode;
        query_summary["plan_profile_signature"] = plan.plan_profile_signature;
        query_summary["selected_strategy"] = plan.search_summary.selected_strategy;
        query_summary["requested_strategy"] = plan.search_summary.requested_strategy;
        query_summary["search_budget"] = plan.search_summary.search_budget;
        query_summary["considered_state_count"] =
            plan.search_summary.considered_state_count;
        query_summary["pruned_state_count"] =
            plan.search_summary.pruned_state_count;
        query_summary["rejected_candidate_count"] =
            plan.search_summary.rejected_candidate_count;
        query_summary["relation_count"] = plan.relations.size();
        query_summary["join_count"] = plan.join_steps.size();
        query_summary["root_node_type"] = plan.root.node_type;
        query_summary["estimated_root_rows"] = plan.root.estimated_rows;
        query_summary["actual_rows"] = actual_rows;
        query_summary["misestimate_ratio"] =
            normalizedMisestimateRatio(plan.root.estimated_rows, actual_rows);
        query_summary["compile_mean_ms"] = compile_mean_ms;
        query_summary["compile_p50_ms"] =
            percentileOfSamplesMs(compile_samples_ms, 0.50);
        query_summary["compile_p95_ms"] =
            percentileOfSamplesMs(compile_samples_ms, 0.95);
        query_summary["compile_p99_ms"] =
            percentileOfSamplesMs(compile_samples_ms, 0.99);
        query_summary["explain_mean_ms"] = explain_mean_ms;
        query_summary["explain_p50_ms"] =
            percentileOfSamplesMs(explain_samples_ms, 0.50);
        query_summary["explain_p95_ms"] =
            percentileOfSamplesMs(explain_samples_ms, 0.95);
        query_summary["explain_p99_ms"] =
            percentileOfSamplesMs(explain_samples_ms, 0.99);
        query_summary["execute_mean_ms"] = execute_mean_ms;
        query_summary["execute_p50_ms"] =
            percentileOfSamplesMs(execute_samples_ms, 0.50);
        query_summary["execute_p95_ms"] =
            percentileOfSamplesMs(execute_samples_ms, 0.95);
        query_summary["execute_p99_ms"] =
            percentileOfSamplesMs(execute_samples_ms, 0.99);
        query_summary["runtime_join_steps"] = std::move(join_steps);
        query_summary["explain_snapshot"] =
            normalizedExplainSnapshot(explain_json);
        baseline["queries"].push_back(std::move(query_summary));

        parity_corpus_rows.push_back(
            csvRow({entry.id,
                    plan.root.node_type,
                    plan.search_summary.selected_strategy,
                    std::to_string(plan.relations.size()),
                    std::to_string(plan.join_steps.size()),
                    std::to_string(plan.root.estimated_rows),
                    std::to_string(actual_rows),
                    std::to_string(
                        normalizedMisestimateRatio(plan.root.estimated_rows,
                                                   actual_rows)),
                    std::to_string(
                        percentileOfSamplesMs(compile_samples_ms, 0.50)),
                    std::to_string(
                        percentileOfSamplesMs(compile_samples_ms, 0.95)),
                    std::to_string(
                        percentileOfSamplesMs(compile_samples_ms, 0.99)),
                    std::to_string(
                        percentileOfSamplesMs(explain_samples_ms, 0.50)),
                    std::to_string(
                        percentileOfSamplesMs(explain_samples_ms, 0.95)),
                    std::to_string(
                        percentileOfSamplesMs(explain_samples_ms, 0.99)),
                    std::to_string(
                        percentileOfSamplesMs(execute_samples_ms, 0.50)),
                    std::to_string(
                        percentileOfSamplesMs(execute_samples_ms, 0.95)),
                    std::to_string(
                        percentileOfSamplesMs(execute_samples_ms, 0.99))}));
    }

    const char* output_path = std::getenv("SB_OPTIMIZER_PARITY_BASELINE_JSON");
    if (output_path != nullptr && *output_path != '\0')
    {
        const std::filesystem::path path(output_path);
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open()) << path.string();
        out << baseline.dump(2) << '\n';
    }
    if (const char* path = std::getenv("SB_OPTIMIZER_PARITY_CORPUS_CSV"))
    {
        ASSERT_TRUE(writeDelimitedLines(path, parity_corpus_rows)) << path;
    }

    ASSERT_TRUE(baseline["queries"].is_array());
    ASSERT_EQ(baseline["queries"].size(), corpus.size());
    for (const auto& query : baseline["queries"])
    {
        EXPECT_EQ(query.at("runtime_plan_contract").get<std::string>(),
                  scratchbird::optimizer::kRuntimePlanContractId);
        EXPECT_EQ(query.at("join_graph_contract").get<std::string>(),
                  scratchbird::optimizer::kJoinGraphContractId);
        EXPECT_EQ(query.at("diagnostics_contract").get<std::string>(),
                  scratchbird::optimizer::kOptimizerDiagnosticsContractId);
        EXPECT_TRUE(query.at("misestimate_ratio").get<double>() >= 1.0);
    }
}

TEST_F(QueryPlannerIntegrationTest,
       OptimizerQualityHarnessCapturesPhysicalCorrectnessAndPlanQuality)
{
    ASSERT_TRUE(createDatabase());

    const std::vector<std::string> setup_sql = {
        "CREATE TABLE cover_users (id INTEGER, name VARCHAR(100), email VARCHAR(100), age INTEGER)",
        "CREATE TABLE merge_users (id INTEGER, name VARCHAR(100))",
        "CREATE TABLE merge_products (id INTEGER, name VARCHAR(100))",
        "CREATE TABLE search_users (id INTEGER, age INTEGER, city VARCHAR(64), cohort VARCHAR(32))",
        "CREATE TABLE order_users (id INTEGER, name VARCHAR(100), email VARCHAR(100), age INTEGER)",
        "GRANT SELECT ON cover_users TO PUBLIC",
        "GRANT SELECT ON merge_users TO PUBLIC",
        "GRANT SELECT ON merge_products TO PUBLIC",
        "GRANT SELECT ON search_users TO PUBLIC",
        "GRANT SELECT ON order_users TO PUBLIC",
        "CREATE INDEX idx_cover_users_id_name ON cover_users (id, name)",
        "CREATE INDEX idx_search_users_age ON search_users (age)",
        "CREATE INDEX idx_search_users_city ON search_users (city)",
        "CREATE INDEX idx_order_users_age_brin ON order_users USING BRIN (age)",
        "CREATE INDEX idx_order_users_age_btree ON order_users (age)"
    };

    for (const auto& sql : setup_sql)
    {
        ASSERT_TRUE(executeSQL(sql).success()) << sql;
    }

    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO cover_users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'cover" + std::to_string(i) +
                               "', 'cover" + std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 40)) + ")")
                        .success());
    }

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO merge_users (id, name) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "')")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO merge_products (id, name) VALUES (" +
                               std::to_string(i) + ", 'product" +
                               std::to_string(i) + "')")
                        .success());
    }

    for (int i = 1; i <= 1600; ++i)
    {
        const int age = 20 + (i % 8);
        const std::string city = (i % 4 == 0) ? "Seattle"
                                 : (i % 4 == 1) ? "Austin"
                                 : (i % 4 == 2) ? "Boston"
                                                : "Denver";
        ASSERT_TRUE(executeSQL("INSERT INTO search_users (id, age, city, cohort) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) + ", '" +
                               city + "', 'c" + std::to_string(i % 5) + "')")
                        .success());
    }
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO search_users (id, age, city, cohort) VALUES (9001, 30, 'Seattle', 'target')")
                    .success());

    for (int i = 1; i <= 4096; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO order_users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'order" +
                               std::to_string(i) + "', 'order" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 80)) + ")")
                        .success());
    }

    ASSERT_TRUE(executeSQL("ANALYZE cover_users").success());
    ASSERT_TRUE(executeSQL("ANALYZE merge_users").success());
    ASSERT_TRUE(executeSQL("ANALYZE merge_products").success());
    ASSERT_TRUE(executeSQL("ANALYZE search_users").success());
    ASSERT_TRUE(executeSQL("ANALYZE order_users").success());

    struct QualityHarnessCase
    {
        std::string id;
        std::string objective;
        std::string sql;
        size_t expected_rows = 0;
        double max_misestimate_ratio = 1.0;
        std::function<void()> before_compile;
        std::function<void()> after_execute;
        std::function<void(const scratchbird::optimizer::RuntimePlan&,
                           const nlohmann::json&,
                           const ExecutionResult&)>
            validate;
    };

    const std::vector<QualityHarnessCase> corpus = {
        {"covering_index_lookup",
         "index-only covering probe remains physically exact",
         "SELECT id, name FROM cover_users WHERE id = 777",
         1,
         16.0,
         nullptr,
         nullptr,
         [&](const scratchbird::optimizer::RuntimePlan& plan,
             const nlohmann::json& explain_json,
             const ExecutionResult& result) {
             ASSERT_EQ(plan.relations.size(), 1u);
             const auto& relation = plan.relations.front();
             EXPECT_EQ(plan.root.node_type, "IndexOnlyScan");
             EXPECT_EQ(explain_json.at("plan_root").at("node_type").get<std::string>(),
                       "IndexOnlyScan");
             EXPECT_EQ(relation.scan_kind, "INDEX_ONLY_SCAN");
             EXPECT_EQ(relation.index_name, "idx_cover_users_id_name");
             EXPECT_TRUE(relation.covering_index);
             EXPECT_TRUE(relation.exact_key_lookup);
             EXPECT_FALSE(relation.requires_recheck);
             ASSERT_TRUE(result.success()) << result.error();
             ASSERT_TRUE(result.hasResultSet());
             ASSERT_EQ(result.resultSet()->rowCount(), 1u);
             EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "777");
         }},
        {"forced_merge_explicit_sort",
         "merge enforcement remains explicit in the physical plan tree",
         "SELECT merge_users.id FROM merge_users JOIN merge_products "
         "ON merge_users.id = merge_products.id",
         4,
         100000.0,
         [&]() { connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "MERGE_JOIN"); },
         [&]() { connection_ctx_->clearSessionVariable("OPTIMIZER.JOIN_METHOD"); },
         [&](const scratchbird::optimizer::RuntimePlan& plan,
             const nlohmann::json& explain_json,
             const ExecutionResult& result) {
             ASSERT_EQ(plan.join_steps.size(), 1u);
             EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
             EXPECT_FALSE(plan.join_steps.front().merge_outer_presorted);
             EXPECT_FALSE(plan.join_steps.front().merge_inner_presorted);
             ASSERT_EQ(plan.root.node_type, "MergeJoin");
             ASSERT_EQ(plan.root.children.size(), 2u);
             EXPECT_EQ(plan.root.children[0].node_type, "Sort");
             EXPECT_EQ(plan.root.children[1].node_type, "Sort");
             EXPECT_EQ(explain_json.at("plan_root").at("node_type").get<std::string>(),
                       "MergeJoin");
             ASSERT_TRUE(result.success()) << result.error();
             ASSERT_TRUE(result.hasResultSet());
             EXPECT_EQ(result.resultSet()->rowCount(), 4u);
         }},
        {"bitmap_exact_probe",
         "bitmap conjunctions remain exact-key probes with post-filter visibility",
         "SELECT id FROM search_users WHERE age = 30 AND city = 'Seattle'",
         1,
         32.0,
         nullptr,
         nullptr,
         [&](const scratchbird::optimizer::RuntimePlan& plan,
             const nlohmann::json& explain_json,
             const ExecutionResult& result) {
             ASSERT_EQ(plan.relations.size(), 1u);
             const auto& relation = plan.relations.front();
             EXPECT_EQ(relation.scan_kind, "BITMAP_INDEX_SCAN");
             EXPECT_EQ(relation.scan_family, "BITMAP_COMBINE_SCAN");
             EXPECT_EQ(relation.bitmap_op, "AND");
             EXPECT_TRUE(relation.exact_key_lookup);
             EXPECT_EQ(relation.visibility_enforcement,
                       scratchbird::optimizer::AccessPathVisibilityEnforcement::POST_FILTER);
             EXPECT_EQ(explain_json.at("plan_root").at("node_type").get<std::string>(),
                       "BitmapIndexScan");
             const auto rows = resultStrings(result);
             EXPECT_EQ(rows.size(), 1u);
             EXPECT_NE(std::find(rows.begin(), rows.end(), "9001"), rows.end());
         }},
        {"ordered_btree_over_summary",
         "exact ordered B-tree access beats summary family matches when it avoids sort",
         "SELECT id, age FROM order_users WHERE age >= 40 AND age <= 41 ORDER BY age",
         102,
         16.0,
         nullptr,
         nullptr,
         [&](const scratchbird::optimizer::RuntimePlan& plan,
             const nlohmann::json& explain_json,
             const ExecutionResult& result) {
             ASSERT_EQ(plan.relations.size(), 1u);
             const auto& relation = plan.relations.front();
             EXPECT_EQ(relation.scan_family, "BTREE_ORDERED_SCAN");
             EXPECT_EQ(relation.index_name, "idx_order_users_age_btree");
             EXPECT_FALSE(relation.requires_recheck);
             EXPECT_NE(plan.root.node_type, "Sort");
             EXPECT_NE(explain_json.at("plan_root").at("node_type").get<std::string>(),
                       "Sort");
             EXPECT_EQ(resultRowCount(result), 102u);
         }},
    };

    std::vector<std::string> physical_results = {
        csvRow({"case_id",
                "objective",
                "root_node_type",
                "scan_families",
                "index_names",
                "exactness_classes",
                "requires_recheck_count",
                "covering_relation_count",
                "explicit_sort_present",
                "actual_rows",
                "status"})};
    std::vector<std::string> plan_quality = {
        csvRow({"case_id",
                "objective",
                "estimated_root_rows",
                "actual_rows",
                "misestimate_ratio",
                "root_total_cost",
                "relation_count",
                "join_count",
                "root_node_type",
                "notes"})};

    for (const auto& entry : corpus)
    {
        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
        connection_ctx_->clearSessionVariable("OPTIMIZER.JOIN_METHOD");
        if (entry.before_compile)
        {
            entry.before_compile();
        }

        auto bytecode = compileSQL(entry.sql);
        ASSERT_FALSE(bytecode.empty()) << entry.id << ": " << last_compile_errors_;

        scratchbird::optimizer::RuntimePlan plan;
        ASSERT_TRUE(decodeRuntimePlan(bytecode, plan)) << entry.id;

        auto explain_result =
            executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) " + entry.sql);
        ASSERT_TRUE(explain_result.success()) << entry.id << ": "
                                              << explain_result.error();
        ASSERT_TRUE(explain_result.hasResultSet()) << entry.id;
        const auto explain_lines = resultStrings(explain_result);
        ASSERT_EQ(explain_lines.size(), 1u) << entry.id;
        const auto explain_json = nlohmann::json::parse(explain_lines.front());

        auto result = executeSQL(entry.sql);
        ASSERT_TRUE(result.success()) << entry.id << ": " << result.error();
        ASSERT_TRUE(result.hasResultSet()) << entry.id;

        entry.validate(plan, explain_json, result);
        if (entry.after_execute)
        {
            entry.after_execute();
        }

        const size_t actual_rows = resultRowCount(result);
        ASSERT_EQ(actual_rows, entry.expected_rows) << entry.id;

        const double misestimate_ratio =
            normalizedMisestimateRatio(plan.root.estimated_rows,
                                       static_cast<uint64_t>(actual_rows));
        EXPECT_LE(misestimate_ratio, entry.max_misestimate_ratio) << entry.id;

        std::vector<std::string> scan_families;
        std::vector<std::string> index_names;
        std::vector<std::string> exactness_classes;
        size_t requires_recheck_count = 0;
        size_t covering_relation_count = 0;
        for (const auto& relation : plan.relations)
        {
            scan_families.push_back(relation.scan_family);
            if (!relation.index_name.empty())
            {
                index_names.push_back(relation.index_name);
            }
            exactness_classes.push_back(
                scratchbird::optimizer::accessPathExactnessClassName(
                    relation.exactness_class));
            if (relation.requires_recheck)
            {
                ++requires_recheck_count;
            }
            if (relation.covering_index)
            {
                ++covering_relation_count;
            }
        }

        physical_results.push_back(
            csvRow({entry.id,
                    entry.objective,
                    plan.root.node_type,
                    joinStrings(scan_families, ";"),
                    joinStrings(index_names, ";"),
                    joinStrings(exactness_classes, ";"),
                    std::to_string(requires_recheck_count),
                    std::to_string(covering_relation_count),
                    runtimeNodeContainsType(plan.root, "Sort") ? "true" : "false",
                    std::to_string(actual_rows),
                    "pass"}));

        plan_quality.push_back(
            csvRow({entry.id,
                    entry.objective,
                    std::to_string(plan.root.estimated_rows),
                    std::to_string(actual_rows),
                    std::to_string(misestimate_ratio),
                    std::to_string(plan.root.total_cost),
                    std::to_string(plan.relations.size()),
                    std::to_string(plan.join_steps.size()),
                    plan.root.node_type,
                    plan.search_summary.selected_strategy}));
    }

    if (const char* path = std::getenv("SB_OPTIMIZER_PHYSICAL_CORRECTNESS_CSV"))
    {
        ASSERT_TRUE(writeDelimitedLines(path, physical_results)) << path;
    }
    if (const char* path = std::getenv("SB_OPTIMIZER_PLAN_QUALITY_MATRIX_CSV"))
    {
        ASSERT_TRUE(writeDelimitedLines(path, plan_quality)) << path;
    }
}

TEST_F(QueryPlannerIntegrationTest, CoveringIndexPlanUsesIndexOnlyScan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'user" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 40)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id = 777");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_kind, "INDEX_ONLY_SCAN");
    EXPECT_TRUE(plan.relations.front().covering_index);
    EXPECT_TRUE(plan.relations.front().exact_key_lookup);
    EXPECT_EQ(plan.relations.front().index_name, "idx_users_id_name");

    auto result = executeSQL("SELECT id, name FROM users WHERE id = 777");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "777");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "user777");
}

TEST_F(QueryPlannerIntegrationTest,
       MulticolumnOrderedAccessFamiliesSurviveIntoRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id, name FROM users WHERE id >= 1000 ORDER BY id, name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_kind, "INDEX_ONLY_SCAN");
    EXPECT_EQ(relation.scan_family, "BTREE_ORDERED_SCAN");
    EXPECT_EQ(relation.path_name, "BTREE_ORDERED_SCAN");
    EXPECT_EQ(relation.scan_family_kind,
              scratchbird::optimizer::PlannerAccessFamily::BTREE_ORDERED_SCAN);
    EXPECT_EQ(relation.exactness_class,
              scratchbird::optimizer::AccessPathExactnessClass::EXACT_KEY);
    EXPECT_EQ(relation.visibility_enforcement,
              scratchbird::optimizer::AccessPathVisibilityEnforcement::HYBRID);
    EXPECT_EQ(relation.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::QUERYABLE);
    EXPECT_TRUE(relation.ordered_output);
    EXPECT_GE(relation.ordered_prefix_length, 2u);
    EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "MULTICOLUMN_PREFIX_INDEX_SCAN"),
              relation.candidate_scan_families.end());
    EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "BTREE_ORDERED_SCAN"),
              relation.candidate_scan_families.end());

    auto result =
        executeSQL("SELECT id, name FROM users WHERE id >= 1000 ORDER BY id, name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 201u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1000");
}

TEST_F(QueryPlannerIntegrationTest,
       ExactIndexOnlyPlanUsesProvedRuntimePathWhenSingleRelationProjectionIsCovered)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id = 777");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    ASSERT_EQ(plan.relations.front().scan_kind, "INDEX_ONLY_SCAN");

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_exact_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "777");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "user777");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("table=users"), std::string::npos) << trace;
    EXPECT_NE(trace.find("scan_kind=INDEX_ONLY_SCAN"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("runtime_access=BTREE_PROVED_INDEX_ONLY_SCAN"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("visibility_proof=HEAP_STABLE_KEY_RECHECK"),
              std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedCoveringRangeUsesProvedIndexOnlyRuntimePath)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id, name FROM users WHERE id >= 1000 ORDER BY id, name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_ordered_range_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 201u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1000");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "user1000");
    EXPECT_EQ(result.resultSet()->getValue(200, 0).toString(), "1200");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("scan_kind=INDEX_ONLY_SCAN"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("runtime_access=BTREE_PROVED_INDEX_ONLY_SCAN"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("ordered_output=1"), std::string::npos) << trace;
    EXPECT_NE(trace.find("visibility_proof=HEAP_STABLE_KEY_RECHECK"),
              std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedCoveringRangePreservesInclusiveLowerBoundAtLeafBoundary)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id FROM users WHERE id >= 200 ORDER BY id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_inclusive_lower_bound_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 57u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "200");
    EXPECT_EQ(result.resultSet()->getValue(56, 0).toString(), "256");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("runtime_access=BTREE_PROVED_INDEX_ONLY_SCAN"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("candidate_count=57"), std::string::npos) << trace;
    EXPECT_NE(trace.find("visible_count=57"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       NonCoveringRangeUsesTidOrderedSecondaryHeapFetchPath)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL(
        "SELECT email FROM users WHERE id >= 1175 AND id <= 1180");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto& relation = plan.relations.front();
    std::ostringstream refusal_dump;
    for (const auto& refusal : relation.candidate_family_refusals)
    {
        refusal_dump << "[" << refusal.family << "|"
                     << refusal.candidate_label << "|"
                     << refusal.refusal_class << "|"
                     << refusal.refusal_cause_domain << "|"
                     << refusal.refusal_reason_code << "|"
                     << refusal.refusal_detail << "]";
    }
    std::ostringstream considered_dump;
    for (const auto& entry : plan.considered_paths)
    {
        if (entry.subject != "users")
        {
            continue;
        }
        considered_dump << "[" << entry.phase << "|"
                        << entry.candidate << "|"
                        << entry.verdict << "|"
                        << entry.reason << "|"
                        << entry.total_cost << "|"
                        << entry.estimated_rows << "]";
    }
    ASSERT_EQ(relation.scan_kind, "INDEX_SCAN")
        << " family=" << relation.scan_family
        << " base_rows=" << relation.base_rows
        << " selectivity=" << relation.selectivity
        << " est_rows=" << relation.estimated_rows
        << " candidates=" << joinStrings(relation.candidate_scan_families, ",")
        << " refusals=" << refusal_dump.str()
        << " considered=" << considered_dump.str();
    EXPECT_FALSE(relation.covering_index);
    EXPECT_EQ(relation.index_name, "idx_users_id_name");
    EXPECT_FALSE(relation.ordered_output);

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_noncovering_range_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 6u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(),
              "user1175@example.com");
    EXPECT_EQ(result.resultSet()->getValue(5, 0).toString(),
              "user1180@example.com");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("table=users"), std::string::npos) << trace;
    EXPECT_NE(trace.find("scan_kind=INDEX_SCAN"), std::string::npos) << trace;
    EXPECT_NE(trace.find("runtime_access=BTREE_TID_ORDERED_SECONDARY_SCAN"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("heap_fetch_mode=TID_ORDERED"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("visibility_proof=HEAP_STABLE_KEY_RECHECK"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("index_only_rows=0"), std::string::npos) << trace;
    EXPECT_NE(trace.find("heap_rows=6"), std::string::npos) << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       PartialOrderedPrefixRetainsExplicitSortGovernance)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_age ON users (age)").success());
    for (int i = 1; i <= 512; ++i)
    {
        const int age = 20 + (i % 4);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'mix" +
                               std::to_string(512 - i) + "', 'mix" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(age) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT age, name FROM users "
        "WHERE age >= 20 AND age <= 23 "
        "ORDER BY age, name";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_family, "BTREE_ORDERED_SCAN");
    EXPECT_TRUE(relation.ordered_output);
    EXPECT_EQ(relation.ordered_prefix_length, 1u);
    EXPECT_NE(std::find(relation.scan_family_tags.begin(),
                        relation.scan_family_tags.end(),
                        "ORDER_PREFIX_ONLY"),
              relation.scan_family_tags.end());
    EXPECT_EQ(plan.root.node_type, "Sort");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "IndexScan");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    EXPECT_GT(result.resultSet()->rowCount(), 0u);
}

TEST_F(QueryPlannerIntegrationTest,
       ExactOrderedBtreeBeatsEarlierSummaryFamilyMatch)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_age_brin ON users USING BRIN (age)")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_age_btree ON users (age)").success());
    for (int i = 1; i <= 4096; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'sum" +
                               std::to_string(i) + "', 'sum" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 80)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT id, age FROM users "
        "WHERE age >= 40 AND age <= 41 "
        "ORDER BY age";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_family, "BTREE_ORDERED_SCAN");
    EXPECT_EQ(relation.index_name, "idx_users_age_btree");
    EXPECT_FALSE(relation.requires_recheck);
    EXPECT_NE(plan.root.node_type, "Sort");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    EXPECT_GT(result.resultSet()->rowCount(), 0u);
}

TEST_F(QueryPlannerIntegrationTest,
       ExactBtreeEqualityBeatsEarlierBitmapStorageMatch)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_age_bitmap ON users USING BITMAP (age)")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_age_btree ON users (age)").success());
    for (int i = 1; i <= 2048; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'eq" +
                               std::to_string(i) + "', 'eq" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 12)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT id FROM users WHERE age = 25";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_family, "BTREE_EQ_SCAN");
    EXPECT_EQ(relation.index_name, "idx_users_age_btree");
    EXPECT_FALSE(relation.requires_recheck);
    EXPECT_EQ(std::find(relation.scan_family_tags.begin(),
                        relation.scan_family_tags.end(),
                        "RECHECK_REQUIRED"),
              relation.scan_family_tags.end());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    EXPECT_GT(result.resultSet()->rowCount(), 0u);
}

TEST_F(QueryPlannerIntegrationTest,
       AnalyzeIndexPublishesTypedFamilyMetricsIntoRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= 600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(
        executeSQL("ANALYZE INDEX users.idx_users_id WITH (sample_rate = 0.25)")
            .success());

    auto bytecode = compileSQL("SELECT id FROM users WHERE id = 512");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_family, "BTREE_EQ_SCAN");
    EXPECT_GE(relation.family_metrics_version, 1u);
    EXPECT_GT(relation.metrics_publication_epoch, 0u);
    EXPECT_EQ(relation.metrics_confidence_class, "HIGH");
    EXPECT_EQ(relation.metrics_freshness_class, "CURRENT");
    EXPECT_EQ(relation.metrics_invalidation_state, "VALID");
    EXPECT_TRUE(relation.metrics_invalidation_reason.empty());
    EXPECT_TRUE(relation.candidate_family_refusals.empty());
    EXPECT_EQ(relation.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::QUERYABLE);

    auto provenance_it = std::find_if(
        plan.statistics_provenance.begin(),
        plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance &entry) {
            return entry.subject == "relation:users" &&
                   entry.source == "FAMILY_NATIVE_METRICS" &&
                   entry.detail.find("family=BTREE_EQ_SCAN") !=
                       std::string::npos;
        });
    ASSERT_NE(provenance_it, plan.statistics_provenance.end());
    EXPECT_NE(provenance_it->detail.find("freshness=CURRENT"),
              std::string::npos);
    EXPECT_NE(provenance_it->detail.find("refresh_attempted=false"),
              std::string::npos);
    EXPECT_NE(provenance_it->detail.find(
                  "replan_boundary=FAMILY_STATISTICS_SIGNATURE"),
              std::string::npos);

    auto explain_result =
        executeSQL("EXPLAIN (FORMAT JSON) SELECT id FROM users WHERE id = 512");
    ASSERT_TRUE(explain_result.success()) << explain_result.error();
    ASSERT_TRUE(explain_result.hasResultSet());
    const auto explain_lines = resultStrings(explain_result);
    ASSERT_EQ(explain_lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(explain_lines.front());
    ASSERT_TRUE(parsed.contains("optimizer_trace"));
    const auto &stats_array = parsed["optimizer_trace"]["statistics_provenance"];
    auto explain_provenance_it = std::find_if(
        stats_array.begin(),
        stats_array.end(),
        [](const nlohmann::json &entry) {
            return entry.value("subject", std::string()) == "relation:users" &&
                   entry.value("source", std::string()) ==
                       "FAMILY_NATIVE_METRICS" &&
                   entry.value("detail", std::string()).find(
                       "family=BTREE_EQ_SCAN") != std::string::npos;
        });
    ASSERT_NE(explain_provenance_it, stats_array.end());
    EXPECT_NE(explain_provenance_it->value("detail", std::string())
                  .find("refresh_attempted=false"),
              std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest,
       UnusablePublishedFamilyMetricsRefuseIndexForWinnerSelection)
{
    ASSERT_TRUE(createDatabase());

    constexpr int kRowCount = 600;
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= kRowCount; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    ErrorContext ctx;
    CatalogManager::TableInfo users_table{};
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               users_table,
                                               &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo users_index{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(users_table.table_id,
                                               "idx_users_id",
                                               users_index,
                                               &ctx),
              Status::OK)
        << ctx.message;

    const uint64_t refresh_xid =
        std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid());
    CatalogManager::IndexStatsCatalogInfo stats{};
    stats.index_id = users_index.index_id;
    stats.stats_version = 11;
    stats.last_analyze_txid = refresh_xid;
    stats.row_count_est = kRowCount;
    stats.distinct_count_est = kRowCount;
    stats.null_frac = 0.0f;
    stats.avg_key_len = 8;
    stats.avg_entry_len = 16;
    stats.leaf_pages = 4;
    stats.height = 2;
    stats.correlation = 1.0f;
    stats.bloat_ratio = 0.0f;
    stats.metrics_last_refresh_xid = refresh_xid;
    stats.family_metrics_version = 11;
    stats.family_metrics_type =
        scratchbird::optimizer::IndexFamilyMetricsType::ORDERED_EXACT;
    stats.metrics_confidence_class =
        scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
    stats.queryability_state =
        scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE;
    stats.is_valid = true;
    stats.family_metrics_payload =
        nlohmann::json{
            {"shared_metrics_envelope",
             {{"index_uuid", users_index.index_id.toString()},
              {"physical_family", "BTREE"},
              {"planner_family", "BTREE_EQ_SCAN"},
              {"metrics_publication_epoch", refresh_xid},
              {"queryability_state", "QUERYABLE"},
              {"metrics_last_refresh_xid", refresh_xid},
              {"metrics_confidence_class", "HIGH"},
              {"freshness_class", "UNUSABLE"},
              {"invalidation_state", "INVALIDATED_HARD"},
              {"invalidation_reason", "CONFIDENCE_INVALID"},
              {"leaf_pages", 4},
              {"height", 2},
              {"row_count_est", kRowCount},
              {"live_entry_count_est", kRowCount},
              {"dead_fraction", 0.0},
              {"bloat_ratio", 0.0},
              {"recheck_ratio_est", 0.0},
              {"correlation", 1.0},
              {"coverage_fraction", 1.0},
              {"maintenance_backlog_ops", 0},
              {"publish_lag_xids", 0},
              {"reclaim_lag_xids", 0}}},
            {"family_metrics_type", "ORDERED_EXACT"},
            {"family_metrics",
             {{"avg_probe_pages", 2.0},
              {"avg_range_pages_per_row",
               4.0 / static_cast<double>(kRowCount)},
              {"duplicate_density", 0.0},
              {"prefix_selectivity", 1.0 / static_cast<double>(kRowCount)},
              {"skip_group_count", kRowCount},
              {"overflow_chain_depth", 0},
              {"run_count", 0},
              {"level_count", 0},
              {"tombstone_fraction", 0.0},
              {"L0_run_count", 0}}}}
            .dump();
    ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats, &ctx),
              Status::OK)
        << ctx.message;
    db_->statistics_manager()->invalidateCache(users_table.table_id);

    auto bytecode = compileSQL("SELECT id FROM users WHERE id = 512");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_kind, "SEQ_SCAN");
    EXPECT_EQ(relation.scan_family, "SEQ_SCAN");
    EXPECT_EQ(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "BTREE_EQ_SCAN"),
              relation.candidate_scan_families.end());
    const auto refusal_it = std::find_if(
        relation.candidate_family_refusals.begin(),
        relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.family == "BTREE_EQ_SCAN" &&
                   refusal.refusal_class == "unusable metrics" &&
                   refusal.refusal_cause_domain == "METRICS" &&
                   refusal.refusal_reason_code ==
                       "P08_MAINTENANCE_STATE_INCOMPATIBLE" &&
                   refusal.refusal_detail.find("INVALIDATED_HARD") !=
                       std::string::npos;
        });
    std::ostringstream refusal_dump;
    for (const auto &refusal : relation.candidate_family_refusals)
    {
        refusal_dump << "[" << refusal.family << "|"
                     << refusal.refusal_class << "|"
                     << refusal.refusal_cause_domain << "|"
                     << refusal.refusal_reason_code << "|"
                     << refusal.refusal_detail << "]";
    }
    EXPECT_NE(refusal_it, relation.candidate_family_refusals.end())
        << refusal_dump.str();

    auto provenance_it = std::find_if(
        plan.statistics_provenance.begin(),
        plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance &entry) {
            return entry.subject == "relation:users" &&
                   entry.source == "FAMILY_NATIVE_METRICS" &&
                   entry.detail.find("family=BTREE_EQ_SCAN") !=
                       std::string::npos;
        });
    ASSERT_NE(provenance_it, plan.statistics_provenance.end());
    EXPECT_NE(provenance_it->detail.find("freshness=UNUSABLE"),
              std::string::npos);
    EXPECT_NE(provenance_it->detail.find("invalidation=INVALIDATED_HARD"),
              std::string::npos);
    EXPECT_NE(provenance_it->detail.find("refresh_attempted=false"),
              std::string::npos);
    EXPECT_NE(provenance_it->detail.find(
                  "replan_boundary=FAMILY_STATISTICS_SIGNATURE"),
              std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest,
       FamilyStatisticsSignatureBoundaryPublishesExplicitReplanProof)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= 600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    ErrorContext ctx;
    CatalogManager::TableInfo users_table{};
    ASSERT_EQ(db_->catalog_manager()->getTable(
                  connection_ctx_->getCurrentSchemaId(),
                  "users",
                  users_table,
                  &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo users_index{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(users_table.table_id,
                                               "idx_users_id",
                                               users_index,
                                               &ctx),
              Status::OK)
        << ctx.message;

    auto publish_metrics =
        [&](uint32_t family_metrics_version, uint64_t refresh_xid) {
            CatalogManager::IndexStatsCatalogInfo stats{};
            stats.index_id = users_index.index_id;
            stats.stats_version = family_metrics_version;
            stats.last_analyze_txid = refresh_xid;
            stats.row_count_est = 600;
            stats.distinct_count_est = 600;
            stats.null_frac = 0.0f;
            stats.avg_key_len = 8;
            stats.avg_entry_len = 16;
            stats.leaf_pages = 4;
            stats.height = 2;
            stats.correlation = 1.0f;
            stats.bloat_ratio = 0.0f;
            stats.metrics_last_refresh_xid = refresh_xid;
            stats.family_metrics_version = family_metrics_version;
            stats.family_metrics_type =
                scratchbird::optimizer::IndexFamilyMetricsType::ORDERED_EXACT;
            stats.metrics_confidence_class =
                scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
            stats.queryability_state =
                scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE;
            stats.is_valid = true;
            stats.family_metrics_payload =
                nlohmann::json{
                    {"shared_metrics_envelope",
                     {{"index_uuid", users_index.index_id.toString()},
                      {"physical_family", "BTREE"},
                      {"planner_family", "BTREE_EQ_SCAN"},
                      {"metrics_publication_epoch", refresh_xid},
                      {"queryability_state", "QUERYABLE"},
                      {"metrics_last_refresh_xid", refresh_xid},
                      {"metrics_confidence_class", "HIGH"},
                      {"freshness_class", "CURRENT"},
                      {"invalidation_state", "VALID"},
                      {"invalidation_reason", ""},
                      {"leaf_pages", 4},
                      {"height", 2},
                      {"row_count_est", 600},
                      {"live_entry_count_est", 600},
                      {"dead_fraction", 0.0},
                      {"bloat_ratio", 0.0},
                      {"recheck_ratio_est", 0.0},
                      {"correlation", 1.0},
                      {"coverage_fraction", 1.0},
                      {"maintenance_backlog_ops", 0},
                      {"publish_lag_xids", 0},
                      {"reclaim_lag_xids", 0}}},
                    {"family_metrics_type", "ORDERED_EXACT"},
                    {"family_metrics",
                     {{"avg_probe_pages", 2.0},
                      {"avg_range_pages_per_row", 4.0 / 600.0},
                      {"duplicate_density", 0.0},
                      {"prefix_selectivity", 1.0 / 600.0},
                      {"skip_group_count", 600},
                      {"overflow_chain_depth", 0},
                      {"run_count", 0},
                      {"level_count", 0},
                      {"tombstone_fraction", 0.0},
                      {"L0_run_count", 0}}}}
                    .dump();

            ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats,
                                                                           &ctx),
                      Status::OK)
                << ctx.message;
            db_->statistics_manager()->invalidateCache(users_table.table_id);
        };

    QueryCompilerV3::invalidateAllPlanCache();
    QueryCompilerV3::resetPlanCacheStats();

    const std::string sql = "SELECT id FROM users WHERE id = 512";
    publish_metrics(41, std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid()));

    auto first_bytecode = compileSQL(sql);
    ASSERT_FALSE(first_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan first_plan;
    ASSERT_TRUE(decodeRuntimePlan(first_bytecode, first_plan));
    EXPECT_FALSE(first_plan.family_statistics_signature.empty());
    EXPECT_EQ(std::count_if(first_plan.statistics_provenance.begin(),
                            first_plan.statistics_provenance.end(),
                            [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance &entry) {
                                return entry.source == "FAMILY_STATISTICS_REPLAN";
                            }),
              0);

    const auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);

    const std::string first_signature = first_plan.family_statistics_signature;
    publish_metrics(42,
                    std::max<uint64_t>(2,
                                       db_->storage_engine()->getCurrentXid() + 17));

    auto second_bytecode = compileSQL(sql);
    ASSERT_FALSE(second_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan second_plan;
    ASSERT_TRUE(decodeRuntimePlan(second_bytecode, second_plan));
    EXPECT_NE(second_plan.family_statistics_signature, first_signature);

    auto replan_it = std::find_if(
        second_plan.statistics_provenance.begin(),
        second_plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance &entry) {
            return entry.subject == "query" &&
                   entry.source == "FAMILY_STATISTICS_REPLAN";
        });
    ASSERT_NE(replan_it, second_plan.statistics_provenance.end());
    EXPECT_NE(replan_it->detail.find("boundary=FAMILY_STATISTICS_SIGNATURE"),
              std::string::npos);
    EXPECT_NE(replan_it->detail.find("previous_cached_variants=1"),
              std::string::npos);
    EXPECT_NE(replan_it->detail.find("refresh_attempted=false"),
              std::string::npos);
    EXPECT_NE(replan_it->detail.find("replan_required=true"),
              std::string::npos);

    auto cache_reject_it = std::find_if(
        second_plan.rejected_paths.begin(),
        second_plan.rejected_paths.end(),
        [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
            return entry.phase == "PLAN_CACHE" &&
                   entry.subject == "query" &&
                   entry.candidate == "CACHE_REUSE" &&
                   entry.verdict == "REJECTED" &&
                   entry.reason.find("family statistics signature boundary crossed") !=
                       std::string::npos;
        });
    EXPECT_NE(cache_reject_it, second_plan.rejected_paths.end());

    const auto control_it = std::find_if(
        second_plan.optimizer_controls.begin(),
        second_plan.optimizer_controls.end(),
        [](const scratchbird::optimizer::RuntimePlanControlEntry &entry) {
            return entry.name == "FAMILY_STATISTICS_REPLAN_BOUNDARY" &&
                   entry.value == "FAMILY_STATISTICS_SIGNATURE" &&
                   entry.source == "PLAN_CACHE";
        });
    EXPECT_NE(control_it, second_plan.optimizer_controls.end());

    const auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 0u);
    EXPECT_EQ(after_second.misses, 2u);
    EXPECT_EQ(after_second.inserts, 2u);
}

TEST_F(QueryPlannerIntegrationTest,
       AgedPublishedFamilyMetricsPreserveMaintenanceStateRefusal)
{
    const auto maintenance_class =
        optimizer::canonicalPlannerBundleRefusalClass(
            "P08_MAINTENANCE_STATE_INCOMPATIBLE",
            "maintenance state LIMITED incompatible with trust class "
            "NATIVE_EXACT: trust=NATIVE_EXACT freshness=AGED invalidation=VALID");
    EXPECT_EQ(maintenance_class,
              "maintenance state incompatible with trust class");
    EXPECT_EQ(
        optimizer::canonicalPlannerBundleRefusalCauseDomain(maintenance_class),
        "METRICS");

    const auto trust_class = optimizer::canonicalPlannerBundleRefusalClass(
        "P08_TRUST_LOCATOR_UNDECLARED",
        "path lacks explicit trust, locator, or maintenance classification: "
        "trust=UNKNOWN freshness=AGED invalidation=VALID");
    EXPECT_EQ(trust_class, "missing trust or locator classification");
    EXPECT_EQ(optimizer::canonicalPlannerBundleRefusalCauseDomain(trust_class),
              "POLICY");
}

TEST_F(QueryPlannerIntegrationTest,
       NonIndexedPredicatePublishesSemanticMismatchRefusal)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= 200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id FROM users WHERE email = 'user42@example.com'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_kind, "SEQ_SCAN");
    const auto refusal_it = std::find_if(
        relation.candidate_family_refusals.begin(),
        relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.family == "INDEX_SCAN" &&
                   refusal.candidate_label == "INDEX_SCAN" &&
                   refusal.refusal_class == "semantic mismatch" &&
                   refusal.refusal_cause_domain == "SEMANTICS" &&
                   refusal.refusal_reason_code ==
                       "P08_NO_MATCHING_INDEX_FOR_PREDICATE" &&
                   refusal.refusal_detail.find("email = 'user42@example.com'") !=
                       std::string::npos;
        });
    std::ostringstream refusal_dump;
    for (const auto &refusal : relation.candidate_family_refusals)
    {
        refusal_dump << "[" << refusal.family << "|"
                     << refusal.candidate_label << "|"
                     << refusal.refusal_class << "|"
                     << refusal.refusal_cause_domain << "|"
                     << refusal.refusal_reason_code << "|"
                     << refusal.refusal_detail << "]";
    }
    EXPECT_NE(refusal_it, relation.candidate_family_refusals.end())
        << refusal_dump.str();
}

TEST_F(QueryPlannerIntegrationTest,
       TypedFamilyMetricsAffectWinnerSelectionAndCalibrationIdentity)
{
    ASSERT_TRUE(createDatabase());

    constexpr int kRowCount = 10000;

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= kRowCount; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(
        executeSQL("ANALYZE INDEX users.idx_users_id WITH (sample_rate = 0.25)")
            .success());

    ErrorContext ctx;
    CatalogManager::TableInfo users_table{};
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               users_table,
                                               &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo users_index{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(users_table.table_id,
                                               "idx_users_id",
                                               users_index,
                                               &ctx),
              Status::OK)
        << ctx.message;

    auto publishFamilyMetrics =
        [&](uint32_t family_metrics_version,
            uint32_t leaf_pages,
            uint16_t height,
            double correlation,
            double bloat_ratio) {
            const uint64_t refresh_xid =
                std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid());
            CatalogManager::IndexStatsCatalogInfo stats{};
            stats.index_id = users_index.index_id;
            stats.stats_version = family_metrics_version;
            stats.last_analyze_txid = refresh_xid;
            stats.row_count_est = kRowCount;
            stats.distinct_count_est = kRowCount;
            stats.null_frac = 0.0f;
            stats.avg_key_len = 8;
            stats.avg_entry_len = 16;
            stats.leaf_pages = leaf_pages;
            stats.height = height;
            stats.correlation = static_cast<float>(correlation);
            stats.bloat_ratio = static_cast<float>(bloat_ratio);
            stats.metrics_last_refresh_xid = refresh_xid;
            stats.family_metrics_version = family_metrics_version;
            stats.family_metrics_type =
                scratchbird::optimizer::IndexFamilyMetricsType::ORDERED_EXACT;
            stats.metrics_confidence_class =
                scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
            stats.queryability_state =
                scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE;
            stats.is_valid = true;
            stats.family_metrics_payload =
                    nlohmann::json{
                    {"shared_metrics_envelope",
                     {{"index_uuid", users_index.index_id.toString()},
                      {"physical_family", "BTREE"},
                      {"planner_family", "BTREE_EQ_SCAN"},
                      {"queryability_state", "QUERYABLE"},
                      {"metrics_last_refresh_xid", refresh_xid},
                      {"metrics_confidence_class", "HIGH"},
                      {"leaf_pages", leaf_pages},
                      {"height", height},
                      {"row_count_est", kRowCount},
                      {"live_entry_count_est", kRowCount},
                      {"dead_fraction", bloat_ratio},
                      {"bloat_ratio", bloat_ratio},
                      {"recheck_ratio_est", 0.0},
                      {"correlation", correlation},
                      {"coverage_fraction", 1.0},
                      {"maintenance_backlog_ops", 0},
                      {"publish_lag_xids", 0},
                      {"reclaim_lag_xids", 0}}},
                    {"family_metrics_type", "ORDERED_EXACT"},
                    {"family_metrics",
                     {{"avg_probe_pages", static_cast<double>(height)},
                      {"avg_range_pages_per_row",
                       static_cast<double>(leaf_pages) /
                           static_cast<double>(kRowCount)},
                      {"duplicate_density", 0.0},
                      {"prefix_selectivity",
                       1.0 / static_cast<double>(kRowCount)},
                      {"skip_group_count", kRowCount},
                      {"overflow_chain_depth", 0},
                      {"run_count", 0},
                      {"level_count", 0},
                      {"tombstone_fraction", bloat_ratio},
                      {"L0_run_count", 0}}}}
                    .dump();
            ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats, &ctx),
                      Status::OK)
                << ctx.message;
            db_->statistics_manager()->invalidateCache(users_table.table_id);
        };

    publishFamilyMetrics(7, 1, 1, 1.0, 0.0);

    scratchbird::optimizer::IndexFamilyMetricsPacket cheap_packet;
    ASSERT_EQ(db_->statistics_manager()->getIndexFamilyMetrics(users_index.index_id,
                                                               cheap_packet,
                                                               &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(cheap_packet.family_metrics_version, 7u);
    EXPECT_EQ(cheap_packet.leaf_pages, 1u);
    EXPECT_EQ(cheap_packet.height, 1u);

    auto cheap_bytecode = compileSQL("SELECT id FROM users WHERE id = 8192");
    ASSERT_FALSE(cheap_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan cheap_plan;
    ASSERT_TRUE(decodeRuntimePlan(cheap_bytecode, cheap_plan));
    ASSERT_EQ(cheap_plan.relations.size(), 1u);
    const auto &cheap_relation = cheap_plan.relations.front();
    EXPECT_EQ(cheap_relation.scan_kind, "INDEX_ONLY_SCAN");
    EXPECT_EQ(cheap_relation.index_name, "idx_users_id");
    EXPECT_EQ(cheap_relation.scan_family, "BTREE_EQ_SCAN");
    EXPECT_EQ(cheap_relation.family_metrics_version, 7u);
    EXPECT_NE(cheap_relation.formula_profile_id.find("btree_eq_scan"),
              std::string::npos);
    EXPECT_NE(cheap_relation.calibration_profile_id.find("ordered_exact"),
              std::string::npos);
    EXPECT_FALSE(cheap_plan.considered_paths.empty());

    publishFamilyMetrics(8, 1000000, 64, 0.0, 1.0);

    scratchbird::optimizer::IndexFamilyMetricsPacket expensive_packet;
    ASSERT_EQ(db_->statistics_manager()->getIndexFamilyMetrics(users_index.index_id,
                                                               expensive_packet,
                                                               &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(expensive_packet.family_metrics_version, 8u);
    EXPECT_EQ(expensive_packet.leaf_pages, 1000000u);
    EXPECT_EQ(expensive_packet.height, 64u);

    auto expensive_bytecode = compileSQL("SELECT id FROM users WHERE id = 8193");
    ASSERT_FALSE(expensive_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan expensive_plan;
    ASSERT_TRUE(decodeRuntimePlan(expensive_bytecode, expensive_plan));
    ASSERT_EQ(expensive_plan.relations.size(), 1u);
    const auto &expensive_relation = expensive_plan.relations.front();
    EXPECT_EQ(expensive_relation.scan_kind, "SEQ_SCAN");
    EXPECT_TRUE(expensive_relation.index_name.empty());
    EXPECT_EQ(expensive_relation.family_metrics_version, 0u);
    EXPECT_FALSE(expensive_plan.considered_paths.empty());
    EXPECT_FALSE(expensive_plan.rejected_paths.empty());
    const auto expensive_refusal_it = std::find_if(
        expensive_relation.candidate_family_refusals.begin(),
        expensive_relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.family == "BTREE_EQ_SCAN" &&
                   refusal.candidate_label == "INDEX_ONLY_SCAN[idx_users_id]" &&
                   refusal.refusal_class ==
                       "structurally overweight path under current metrics" &&
                   refusal.refusal_cause_domain == "COST" &&
                   refusal.refusal_reason_code ==
                       "P08_STRUCTURALLY_OVERWEIGHT_EXACT_PATH" &&
                   refusal.refusal_detail ==
                       "family metrics mark exact path structurally overweight";
        });
    std::ostringstream expensive_refusal_dump;
    for (const auto &refusal : expensive_relation.candidate_family_refusals)
    {
        expensive_refusal_dump << "[" << refusal.family << "|"
                               << refusal.candidate_label << "|"
                               << refusal.refusal_class << "|"
                               << refusal.refusal_cause_domain << "|"
                               << refusal.refusal_reason_code << "|"
                               << refusal.refusal_detail << "]";
    }
    EXPECT_NE(expensive_refusal_it,
              expensive_relation.candidate_family_refusals.end())
        << expensive_refusal_dump.str();
}

TEST_F(QueryPlannerIntegrationTest, ExistingOrderedIndexPathAvoidsExplicitSortNode)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id, name FROM users WHERE id >= 1000 ORDER BY id, name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "IndexOnlyScan");
    EXPECT_EQ(plan.root.children.size(), 0u);

    const auto trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "EXISTING_ORDER" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(trace_it, plan.considered_paths.end());
    EXPECT_NE(trace_it->reason.find("satisfies ORDER BY"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, BitmapIndexPlanExecutesExactProbes)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE search_users (id INTEGER, age INTEGER, city VARCHAR(64), cohort VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON search_users TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_search_users_age ON search_users (age)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_search_users_city ON search_users (city)").success());
    for (int i = 1; i <= 1600; ++i)
    {
        const int age = 20 + (i % 8);
        const std::string city = (i % 4 == 0) ? "Seattle" :
                                 (i % 4 == 1) ? "Austin" :
                                 (i % 4 == 2) ? "Boston" : "Denver";
        ASSERT_TRUE(executeSQL("INSERT INTO search_users (id, age, city, cohort) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) + ", '" +
                               city + "', 'c" + std::to_string(i % 5) + "')")
                        .success());
    }
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO search_users (id, age, city, cohort) VALUES (9001, 30, 'Seattle', 'target')")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE search_users").success());

    auto bytecode =
        compileSQL("SELECT id FROM search_users WHERE age = 30 AND city = 'Seattle'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_kind, "BITMAP_INDEX_SCAN");
    EXPECT_EQ(plan.relations.front().scan_family, "BITMAP_COMBINE_SCAN");
    EXPECT_EQ(plan.relations.front().path_name, "BITMAP_COMBINE_SCAN");
    EXPECT_EQ(plan.relations.front().scan_family_kind,
              scratchbird::optimizer::PlannerAccessFamily::BITMAP_COMBINE_SCAN);
    EXPECT_EQ(plan.relations.front().exactness_class,
              scratchbird::optimizer::AccessPathExactnessClass::CANDIDATE_REGION);
    EXPECT_EQ(plan.relations.front().visibility_enforcement,
              scratchbird::optimizer::AccessPathVisibilityEnforcement::POST_FILTER);
    EXPECT_EQ(plan.relations.front().bitmap_op, "AND");
    EXPECT_TRUE(plan.relations.front().exact_key_lookup);
    ASSERT_GE(plan.relations.front().index_predicates.size(), 2u);

    auto result =
        executeSQL("SELECT id FROM search_users WHERE age = 30 AND city = 'Seattle'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    ASSERT_FALSE(rows.empty());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "9001"), rows.end());
}

TEST_F(QueryPlannerIntegrationTest,
       SummaryBitmapColumnstoreFamiliesPublishDistinctRuntimePaths)
{
    ASSERT_TRUE(createDatabase());

    constexpr int kRowCount = 2048;

    ASSERT_TRUE(
        executeSQL("CREATE TABLE brin_events (id INTEGER, age INTEGER, payload VARCHAR(64))")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE TABLE filter_events (id INTEGER, age INTEGER, payload VARCHAR(64))")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE TABLE bitmap_events (id INTEGER, status VARCHAR(16), payload VARCHAR(64))")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE TABLE column_events (id INTEGER, age INTEGER, payload VARCHAR(64))")
            .success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON brin_events TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON filter_events TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON bitmap_events TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON column_events TO PUBLIC").success());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_brin_events_age ON brin_events USING BRIN (age)")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_filter_events_age ON filter_events USING ZONEMAP (age)")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_bitmap_events_status ON bitmap_events USING BITMAP (status)")
            .success());
    ASSERT_TRUE(
        executeSQL(
            "CREATE INDEX idx_column_events_age_payload ON column_events USING COLUMNSTORE (age, payload)")
            .success());

    for (int i = 1; i <= kRowCount; ++i)
    {
        const int age = 20 + (i % 64);
        const std::string payload = "payload_" + std::to_string(i);
        const std::string status = (i % 64 == 0) ? "target" : "other";

        ASSERT_TRUE(executeSQL("INSERT INTO brin_events (id, age, payload) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) +
                               ", '" + payload + "')")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO filter_events (id, age, payload) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) +
                               ", '" + payload + "')")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO bitmap_events (id, status, payload) VALUES (" +
                               std::to_string(i) + ", '" + status + "', '" +
                               payload + "')")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO column_events (id, age, payload) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) +
                               ", '" + payload + "')")
                        .success());
    }

    ASSERT_TRUE(executeSQL("ANALYZE brin_events").success());
    ASSERT_TRUE(executeSQL("ANALYZE filter_events").success());
    ASSERT_TRUE(executeSQL("ANALYZE bitmap_events").success());
    ASSERT_TRUE(executeSQL("ANALYZE column_events").success());

    ErrorContext ctx;
    auto publishSummaryCandidateMetrics =
        [&](const std::string &table_name,
            const std::string &index_name,
            const std::string &physical_family,
            const std::string &planner_family,
            uint32_t family_metrics_version,
            uint32_t leaf_pages,
            uint16_t height,
            double coverage_fraction,
            double recheck_ratio_est,
            double correlation,
            uint64_t distinct_count_est,
            const nlohmann::json &family_metrics) {
            CatalogManager::TableInfo table_info{};
            ASSERT_EQ(db_->catalog_manager()->getTable(
                          connection_ctx_->getCurrentSchemaId(),
                          table_name,
                          table_info,
                          &ctx),
                      Status::OK)
                << ctx.message;

            CatalogManager::IndexInfo index_info{};
            ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id,
                                                       index_name,
                                                       index_info,
                                                       &ctx),
                      Status::OK)
                << ctx.message;

            const uint64_t refresh_xid =
                std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid());

            CatalogManager::IndexStatsCatalogInfo stats{};
            stats.index_id = index_info.index_id;
            stats.stats_version = family_metrics_version;
            stats.last_analyze_txid = refresh_xid;
            stats.row_count_est = kRowCount;
            stats.distinct_count_est = distinct_count_est;
            stats.null_frac = 0.0f;
            stats.avg_key_len = 8;
            stats.avg_entry_len = 16;
            stats.leaf_pages = leaf_pages;
            stats.height = height;
            stats.correlation = static_cast<float>(correlation);
            stats.bloat_ratio = 0.0f;
            stats.metrics_last_refresh_xid = refresh_xid;
            stats.family_metrics_version = family_metrics_version;
            stats.family_metrics_type =
                scratchbird::optimizer::IndexFamilyMetricsType::SUMMARY_CANDIDATE;
            stats.metrics_confidence_class =
                scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
            stats.queryability_state =
                scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE;
            stats.is_valid = true;
            stats.family_metrics_payload =
                nlohmann::json{
                    {"shared_metrics_envelope",
                     {{"index_uuid", index_info.index_id.toString()},
                      {"physical_family", physical_family},
                      {"planner_family", planner_family},
                      {"queryability_state", "QUERYABLE"},
                      {"metrics_last_refresh_xid", refresh_xid},
                      {"metrics_confidence_class", "HIGH"},
                      {"leaf_pages", leaf_pages},
                      {"height", height},
                      {"row_count_est", kRowCount},
                      {"live_entry_count_est", kRowCount},
                      {"dead_fraction", 0.0},
                      {"bloat_ratio", 0.0},
                      {"recheck_ratio_est", recheck_ratio_est},
                      {"correlation", correlation},
                      {"coverage_fraction", coverage_fraction},
                      {"maintenance_backlog_ops", 0},
                      {"publish_lag_xids", 0},
                      {"reclaim_lag_xids", 0}}},
                    {"family_metrics_type", "SUMMARY_CANDIDATE"},
                    {"family_metrics", family_metrics}}
                    .dump();

            ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats,
                                                                           &ctx),
                      Status::OK)
                << ctx.message;
            db_->statistics_manager()->invalidateCache(table_info.table_id);
        };

    publishSummaryCandidateMetrics(
        "brin_events",
        "idx_brin_events_age",
        "BRIN",
        "BRIN_SCAN",
        11,
        4,
        1,
        0.90,
        0.05,
        0.0,
        kRowCount,
        nlohmann::json{
            {"pages_per_range", 8},
            {"prune_ratio_est", 0.98},
            {"unsummarized_range_fraction", 0.01},
            {"summary_staleness_fraction", 0.00}});

    publishSummaryCandidateMetrics(
        "filter_events",
        "idx_filter_events_age",
        "BRIN",
        "SUMMARY_FILTER_SCAN",
        12,
        4,
        1,
        0.82,
        0.08,
        0.0,
        kRowCount,
        nlohmann::json{
            {"pages_per_range", 8},
            {"prune_ratio_est", 0.97},
            {"unsummarized_range_fraction", 0.02},
            {"summary_staleness_fraction", 0.01}});

    publishSummaryCandidateMetrics(
        "bitmap_events",
        "idx_bitmap_events_status",
        "BITMAP",
        "BITMAP_STORAGE_SCAN",
        13,
        1,
        1,
        0.35,
        0.00,
        0.0,
        2,
        nlohmann::json{
            {"bitmap_density", 0.02},
            {"bitmap_false_positive_ratio", 0.00},
            {"lossy_container_fraction", 0.00}});

    publishSummaryCandidateMetrics(
        "column_events",
        "idx_column_events_age_payload",
        "COLUMNSTORE",
        "COLUMNSTORE_SCAN",
        14,
        8,
        1,
        0.60,
        0.00,
        0.0,
        kRowCount,
        nlohmann::json{
            {"column_bytes_pruned_ratio", 0.95},
            {"row_groups_touched_ratio", 0.05},
            {"late_materialization_gain_est", 0.90},
            {"projection_width_bytes", 8.0},
            {"delta_fraction", 0.01}});

    auto assertSingleFamilyPlan =
        [&](const std::string &sql,
            const std::string &expected_scan_kind,
            scratchbird::optimizer::PlannerAccessFamily expected_family_kind,
            scratchbird::optimizer::AccessPathQueryabilityState
                expected_queryability_state,
            uint32_t expected_family_metrics_version,
            const std::string &expected_index_name,
            double expected_coverage_fraction) {
            auto bytecode = compileSQL(sql);
            ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

            scratchbird::optimizer::RuntimePlan plan;
            ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
            ASSERT_EQ(plan.relations.size(), 1u);
            const auto &relation = plan.relations.front();
            EXPECT_EQ(relation.scan_kind, expected_scan_kind);
            EXPECT_EQ(relation.scan_family, expected_scan_kind);
            EXPECT_EQ(relation.path_name, expected_scan_kind);
            EXPECT_EQ(relation.scan_family_kind, expected_family_kind);
            EXPECT_EQ(relation.exactness_class,
                      scratchbird::optimizer::AccessPathExactnessClass::
                          CANDIDATE_REGION);
            EXPECT_EQ(relation.visibility_enforcement,
                      scratchbird::optimizer::AccessPathVisibilityEnforcement::
                          POST_FILTER);
            EXPECT_TRUE(relation.requires_recheck);
            EXPECT_FALSE(relation.exact_key_lookup);
            EXPECT_EQ(relation.queryability_state, expected_queryability_state);
            EXPECT_EQ(relation.family_metrics_version,
                      expected_family_metrics_version);
            EXPECT_EQ(relation.index_name, expected_index_name);
            EXPECT_GT(relation.candidate_budget, 0u);
            EXPECT_NEAR(relation.coverage_fraction,
                        expected_coverage_fraction,
                        1e-6);
            EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                                relation.candidate_scan_families.end(),
                                expected_scan_kind),
                      relation.candidate_scan_families.end());

            auto result = executeSQL(sql);
            ASSERT_TRUE(result.success()) << result.error();
            ASSERT_TRUE(result.hasResultSet());
            EXPECT_GT(result.resultSet()->rowCount(), 0u);
        };

    assertSingleFamilyPlan(
        "SELECT id FROM brin_events WHERE age >= 40 AND age <= 41",
        "BRIN_SCAN",
        scratchbird::optimizer::PlannerAccessFamily::BRIN_SCAN,
        scratchbird::optimizer::AccessPathQueryabilityState::QUERYABLE,
        11,
        "idx_brin_events_age",
        0.90);

    assertSingleFamilyPlan(
        "SELECT id FROM filter_events WHERE age >= 55 AND age <= 55",
        "SUMMARY_FILTER_SCAN",
        scratchbird::optimizer::PlannerAccessFamily::SUMMARY_FILTER_SCAN,
        scratchbird::optimizer::AccessPathQueryabilityState::QUERYABLE,
        12,
        "idx_filter_events_age",
        0.82);

    assertSingleFamilyPlan(
        "SELECT id FROM bitmap_events WHERE status = 'target'",
        "BITMAP_STORAGE_SCAN",
        scratchbird::optimizer::PlannerAccessFamily::BITMAP_STORAGE_SCAN,
        scratchbird::optimizer::AccessPathQueryabilityState::LIMITED,
        13,
        "idx_bitmap_events_status",
        0.35);

    assertSingleFamilyPlan(
        "SELECT age FROM column_events WHERE age >= 60 AND age <= 60",
        "COLUMNSTORE_SCAN",
        scratchbird::optimizer::PlannerAccessFamily::COLUMNSTORE_SCAN,
        scratchbird::optimizer::AccessPathQueryabilityState::LIMITED,
        14,
        "idx_column_events_age_payload",
        0.60);
}

TEST_F(QueryPlannerIntegrationTest,
       SummaryCandidateFamilyBelowPromotionThresholdPublishesStructuredRefusal)
{
    ASSERT_TRUE(createDatabase());

    constexpr int kRowCount = 1024;

    ASSERT_TRUE(
        executeSQL("CREATE TABLE brin_events (id INTEGER, age INTEGER, payload VARCHAR(64))")
            .success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON brin_events TO PUBLIC").success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_brin_events_age ON brin_events USING BRIN (age)")
            .success());

    for (int i = 1; i <= kRowCount; ++i)
    {
        const int age = 20 + (i % 64);
        ASSERT_TRUE(executeSQL("INSERT INTO brin_events (id, age, payload) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) +
                               ", 'payload_" + std::to_string(i) + "')")
                        .success());
    }

    ASSERT_TRUE(executeSQL("ANALYZE brin_events").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info{};
    ASSERT_EQ(db_->catalog_manager()->getTable(
                  connection_ctx_->getCurrentSchemaId(),
                  "brin_events",
                  table_info,
                  &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo index_info{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id,
                                               "idx_brin_events_age",
                                               index_info,
                                               &ctx),
              Status::OK)
        << ctx.message;

    const uint64_t refresh_xid =
        std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid());

    CatalogManager::IndexStatsCatalogInfo stats{};
    stats.index_id = index_info.index_id;
    stats.stats_version = 31;
    stats.last_analyze_txid = refresh_xid;
    stats.row_count_est = kRowCount;
    stats.distinct_count_est = kRowCount;
    stats.null_frac = 0.0f;
    stats.avg_key_len = 8;
    stats.avg_entry_len = 16;
    stats.leaf_pages = 4;
    stats.height = 1;
    stats.correlation = 0.0f;
    stats.bloat_ratio = 0.0f;
    stats.metrics_last_refresh_xid = refresh_xid;
    stats.family_metrics_version = 31;
    stats.family_metrics_type =
        scratchbird::optimizer::IndexFamilyMetricsType::SUMMARY_CANDIDATE;
    stats.metrics_confidence_class =
        scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
    stats.queryability_state =
        scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE;
    stats.is_valid = true;
    stats.family_metrics_payload =
        nlohmann::json{
            {"shared_metrics_envelope",
             {{"index_uuid", index_info.index_id.toString()},
              {"physical_family", "BRIN"},
              {"planner_family", "BRIN_SCAN"},
              {"queryability_state", "QUERYABLE"},
              {"metrics_last_refresh_xid", refresh_xid},
              {"metrics_confidence_class", "HIGH"},
              {"leaf_pages", 4},
              {"height", 1},
              {"row_count_est", kRowCount},
              {"live_entry_count_est", kRowCount},
              {"dead_fraction", 0.0},
              {"bloat_ratio", 0.0},
              {"recheck_ratio_est", 0.05},
              {"correlation", 0.0},
              {"coverage_fraction", 0.90},
              {"maintenance_backlog_ops", 0},
              {"publish_lag_xids", 0},
              {"reclaim_lag_xids", 0}}},
            {"family_metrics_type", "SUMMARY_CANDIDATE"},
            {"family_metrics",
             {{"pages_per_range", 8},
              {"prune_ratio_est", 0.08},
              {"unsummarized_range_fraction", 0.02},
              {"summary_staleness_fraction", 0.01}}}}
            .dump();

    ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats, &ctx),
              Status::OK)
        << ctx.message;
    db_->statistics_manager()->invalidateCache(table_info.table_id);

    auto bytecode =
        compileSQL("SELECT id FROM brin_events WHERE age >= 40 AND age <= 41");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_kind, "SEQ_SCAN");
    EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "BRIN_SCAN"),
              relation.candidate_scan_families.end());

    const auto refusal_it = std::find_if(
        relation.candidate_family_refusals.begin(),
        relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.family == "BRIN_SCAN" &&
                   refusal.candidate_label ==
                       "BRIN_SCAN[idx_brin_events_age]" &&
                   refusal.refusal_class ==
                       "family-specific promotion threshold not met" &&
                   refusal.refusal_cause_domain == "METRICS" &&
                   refusal.refusal_reason_code ==
                       "P08_SUMMARY_NATIVE_PROMOTION_THRESHOLD_NOT_MET" &&
                   refusal.refusal_detail.find(
                       "summary native promotion threshold not met") !=
                       std::string::npos;
        });

    std::ostringstream refusal_dump;
    for (const auto &refusal : relation.candidate_family_refusals)
    {
        refusal_dump << "[" << refusal.family << "|"
                     << refusal.candidate_label << "|"
                     << refusal.refusal_class << "|"
                     << refusal.refusal_cause_domain << "|"
                     << refusal.refusal_reason_code << "|"
                     << refusal.refusal_detail << "]";
    }
    EXPECT_NE(refusal_it, relation.candidate_family_refusals.end())
        << refusal_dump.str();
}

TEST_F(QueryPlannerIntegrationTest,
       BitmapAndColumnstoreFamiliesBelowPromotionThresholdPublishStructuredRefusals)
{
    ASSERT_TRUE(createDatabase());

    constexpr int kRowCount = 1024;

    ASSERT_TRUE(
        executeSQL("CREATE TABLE bitmap_events (id INTEGER, status VARCHAR(16), payload VARCHAR(64))")
            .success());
    ASSERT_TRUE(
        executeSQL("CREATE TABLE column_events (id INTEGER, age INTEGER, payload VARCHAR(64))")
            .success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON bitmap_events TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON column_events TO PUBLIC").success());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_bitmap_events_status ON bitmap_events USING BITMAP (status)")
            .success());
    ASSERT_TRUE(
        executeSQL(
            "CREATE INDEX idx_column_events_age_payload ON column_events USING COLUMNSTORE (age, payload)")
            .success());

    for (int i = 1; i <= kRowCount; ++i)
    {
        const int age = 20 + (i % 64);
        const std::string payload = "payload_" + std::to_string(i);
        const std::string status = (i % 64 == 0) ? "target" : "other";

        ASSERT_TRUE(executeSQL("INSERT INTO bitmap_events (id, status, payload) VALUES (" +
                               std::to_string(i) + ", '" + status + "', '" +
                               payload + "')")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO column_events (id, age, payload) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) +
                               ", '" + payload + "')")
                        .success());
    }

    ASSERT_TRUE(executeSQL("ANALYZE bitmap_events").success());
    ASSERT_TRUE(executeSQL("ANALYZE column_events").success());

    ErrorContext ctx;
    auto publishSummaryCandidateMetrics =
        [&](const std::string &table_name,
            const std::string &index_name,
            const std::string &physical_family,
            const std::string &planner_family,
            uint32_t family_metrics_version,
            uint32_t leaf_pages,
            uint16_t height,
            double coverage_fraction,
            double recheck_ratio_est,
            double correlation,
            uint64_t distinct_count_est,
            const nlohmann::json &family_metrics) {
            CatalogManager::TableInfo table_info{};
            ASSERT_EQ(db_->catalog_manager()->getTable(
                          connection_ctx_->getCurrentSchemaId(),
                          table_name,
                          table_info,
                          &ctx),
                      Status::OK)
                << ctx.message;

            CatalogManager::IndexInfo index_info{};
            ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id,
                                                       index_name,
                                                       index_info,
                                                       &ctx),
                      Status::OK)
                << ctx.message;

            const uint64_t refresh_xid =
                std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid());

            CatalogManager::IndexStatsCatalogInfo stats{};
            stats.index_id = index_info.index_id;
            stats.stats_version = family_metrics_version;
            stats.last_analyze_txid = refresh_xid;
            stats.row_count_est = kRowCount;
            stats.distinct_count_est = distinct_count_est;
            stats.null_frac = 0.0f;
            stats.avg_key_len = 8;
            stats.avg_entry_len = 16;
            stats.leaf_pages = leaf_pages;
            stats.height = height;
            stats.correlation = static_cast<float>(correlation);
            stats.bloat_ratio = 0.0f;
            stats.metrics_last_refresh_xid = refresh_xid;
            stats.family_metrics_version = family_metrics_version;
            stats.family_metrics_type =
                scratchbird::optimizer::IndexFamilyMetricsType::SUMMARY_CANDIDATE;
            stats.metrics_confidence_class =
                scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
            stats.queryability_state =
                scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE;
            stats.is_valid = true;
            stats.family_metrics_payload =
                nlohmann::json{
                    {"shared_metrics_envelope",
                     {{"index_uuid", index_info.index_id.toString()},
                      {"physical_family", physical_family},
                      {"planner_family", planner_family},
                      {"queryability_state", "QUERYABLE"},
                      {"metrics_last_refresh_xid", refresh_xid},
                      {"metrics_confidence_class", "HIGH"},
                      {"leaf_pages", leaf_pages},
                      {"height", height},
                      {"row_count_est", kRowCount},
                      {"live_entry_count_est", kRowCount},
                      {"dead_fraction", 0.0},
                      {"bloat_ratio", 0.0},
                      {"recheck_ratio_est", recheck_ratio_est},
                      {"correlation", correlation},
                      {"coverage_fraction", coverage_fraction},
                      {"maintenance_backlog_ops", 0},
                      {"publish_lag_xids", 0},
                      {"reclaim_lag_xids", 0}}},
                    {"family_metrics_type", "SUMMARY_CANDIDATE"},
                    {"family_metrics", family_metrics}}
                    .dump();

            ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats,
                                                                           &ctx),
                      Status::OK)
                << ctx.message;
            db_->statistics_manager()->invalidateCache(table_info.table_id);
        };

    publishSummaryCandidateMetrics(
        "bitmap_events",
        "idx_bitmap_events_status",
        "BITMAP",
        "BITMAP_STORAGE_SCAN",
        21,
        1,
        1,
        0.35,
        0.04,
        0.0,
        2,
        nlohmann::json{
            {"bitmap_density", 0.98},
            {"bitmap_false_positive_ratio", 0.04},
            {"lossy_container_fraction", 0.05}});

    publishSummaryCandidateMetrics(
        "column_events",
        "idx_column_events_age_payload",
        "COLUMNSTORE",
        "COLUMNSTORE_SCAN",
        22,
        8,
        1,
        0.60,
        0.00,
        0.0,
        kRowCount,
        nlohmann::json{
            {"column_bytes_pruned_ratio", 0.00},
            {"row_groups_touched_ratio", 1.00},
            {"late_materialization_gain_est", 0.00},
            {"projection_width_bytes", 512.0},
            {"delta_fraction", 0.10},
            {"chunk_prune_ratio", 0.00},
            {"bulk_filter_gain_est", 0.00},
            {"mutable_buffer_fraction", 0.25}});

    auto assertThresholdRefusal =
        [&](const std::string &sql,
            const std::string &expected_family,
            const std::string &expected_candidate_label,
            const std::string &expected_reason_code,
            const std::string &expected_detail) {
            auto bytecode = compileSQL(sql);
            ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

            scratchbird::optimizer::RuntimePlan plan;
            ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
            ASSERT_EQ(plan.relations.size(), 1u);
            const auto &relation = plan.relations.front();
            EXPECT_EQ(relation.scan_kind, "SEQ_SCAN");
            EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                                relation.candidate_scan_families.end(),
                                expected_family),
                      relation.candidate_scan_families.end());

            const auto refusal_it = std::find_if(
                relation.candidate_family_refusals.begin(),
                relation.candidate_family_refusals.end(),
                [&](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
                    return refusal.family == expected_family &&
                           refusal.candidate_label == expected_candidate_label &&
                           refusal.refusal_class ==
                               "family-specific promotion threshold not met" &&
                           refusal.refusal_cause_domain == "METRICS" &&
                           refusal.refusal_reason_code == expected_reason_code &&
                           refusal.refusal_detail.find(expected_detail) !=
                               std::string::npos;
                });

            std::ostringstream refusal_dump;
            for (const auto &refusal : relation.candidate_family_refusals)
            {
                refusal_dump << "[" << refusal.family << "|"
                             << refusal.candidate_label << "|"
                             << refusal.refusal_class << "|"
                             << refusal.refusal_cause_domain << "|"
                             << refusal.refusal_reason_code << "|"
                             << refusal.refusal_detail << "]";
            }
            EXPECT_NE(refusal_it, relation.candidate_family_refusals.end())
                << refusal_dump.str();
        };

    assertThresholdRefusal("SELECT id FROM bitmap_events WHERE status = 'target'",
                           "BITMAP_STORAGE_SCAN",
                           "BITMAP_STORAGE_SCAN[idx_bitmap_events_status]",
                           "P08_BITMAP_NATIVE_PROMOTION_THRESHOLD_NOT_MET",
                           "bitmap storage native promotion threshold not met");

    assertThresholdRefusal("SELECT age, payload FROM column_events WHERE age >= 60 AND age <= 60",
                           "COLUMNSTORE_SCAN",
                           "COLUMNSTORE_SCAN[idx_column_events_age_payload]",
                           "P08_COLUMNSTORE_NATIVE_PROMOTION_THRESHOLD_NOT_MET",
                           "columnstore native promotion threshold not met");
}

TEST_F(QueryPlannerIntegrationTest,
       TextFamilyPlanPublishesCandidateBudgetAndMetricsIdentity)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE docs (id INTEGER, title TEXT)").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON docs TO PUBLIC").success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_docs_title_text ON docs USING FULLTEXT (title)")
            .success());

    ASSERT_TRUE(
        executeSQL("INSERT INTO docs (id, title) VALUES (1, 'alpha')").success());
    ASSERT_TRUE(
        executeSQL("INSERT INTO docs (id, title) VALUES (2, 'beta')").success());
    ASSERT_TRUE(
        executeSQL("INSERT INTO docs (id, title) VALUES (3, 'alpha')").success());

    auto analyze_result = executeSQL("ANALYZE docs");
    ASSERT_TRUE(analyze_result.success()) << analyze_result.error();

    core::ErrorContext ctx;
    CatalogManager::TableInfo table_info{};
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                              "docs",
                                              table_info,
                                              &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo index_info{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id,
                                               "idx_docs_title_text",
                                               index_info,
                                               &ctx),
              Status::OK)
        << ctx.message;

    const uint64_t refresh_xid =
        std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid());

    CatalogManager::IndexStatsCatalogInfo stats{};
    stats.index_id = index_info.index_id;
    stats.stats_version = 21;
    stats.last_analyze_txid = refresh_xid;
    stats.row_count_est = 3;
    stats.distinct_count_est = 2;
    stats.null_frac = 0.0f;
    stats.avg_key_len = 12;
    stats.avg_entry_len = 20;
    stats.leaf_pages = 2;
    stats.height = 1;
    stats.correlation = 0.0f;
    stats.bloat_ratio = 0.0f;
    stats.metrics_last_refresh_xid = refresh_xid;
    stats.family_metrics_version = 21;
    stats.family_metrics_type =
        scratchbird::optimizer::IndexFamilyMetricsType::TEXT_SEARCH;
    stats.metrics_confidence_class =
        scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
    stats.queryability_state =
        scratchbird::optimizer::IndexMetricsQueryabilityState::LIMITED;
    stats.is_valid = true;
    stats.family_metrics_payload =
        nlohmann::json{
            {"shared_metrics_envelope",
             {{"index_uuid", index_info.index_id.toString()},
              {"physical_family", "FULLTEXT"},
              {"planner_family", "TEXT_BITMAP_SCAN"},
              {"queryability_state", "LIMITED"},
              {"metrics_last_refresh_xid", refresh_xid},
              {"metrics_confidence_class", "HIGH"},
              {"leaf_pages", 2},
              {"height", 1},
              {"row_count_est", 3},
              {"live_entry_count_est", 3},
              {"dead_fraction", 0.0},
              {"bloat_ratio", 0.0},
              {"recheck_ratio_est", 0.05},
              {"correlation", 0.0},
              {"coverage_fraction", 0.25},
              {"maintenance_backlog_ops", 0},
              {"publish_lag_xids", 0},
              {"reclaim_lag_xids", 0}}},
            {"family_metrics_type", "TEXT_SEARCH"},
            {"family_metrics",
             {{"term_df", 2},
              {"term_df_skew", 0.0},
              {"avg_postings_per_term", 2.0},
              {"pending_list_fraction", 0.0},
              {"phrase_hit_rate", 1.0},
              {"score_rows_est", 2.0},
              {"merge_debt", 0.0}}}}
            .dump();

    ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats, &ctx),
              Status::OK)
        << ctx.message;
    db_->statistics_manager()->invalidateCache(table_info.table_id);

    auto bytecode = compileSQL("SELECT id FROM docs WHERE title = 'alpha'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_kind, "TEXT_BITMAP_SCAN");
    EXPECT_EQ(relation.scan_family, "TEXT_BITMAP_SCAN");
    EXPECT_EQ(relation.path_name, "TEXT_BITMAP_SCAN");
    EXPECT_EQ(relation.scan_family_kind,
              scratchbird::optimizer::PlannerAccessFamily::TEXT_BITMAP_SCAN);
    EXPECT_EQ(relation.exactness_class,
              scratchbird::optimizer::AccessPathExactnessClass::
                  CANDIDATE_REGION);
    EXPECT_EQ(relation.visibility_enforcement,
              scratchbird::optimizer::AccessPathVisibilityEnforcement::
                  POST_FILTER);
    EXPECT_TRUE(relation.requires_recheck);
    EXPECT_EQ(relation.queryability_state,
              scratchbird::optimizer::AccessPathQueryabilityState::LIMITED);
    EXPECT_EQ(relation.family_metrics_version, 21u);
    EXPECT_EQ(relation.index_name, "idx_docs_title_text");
    EXPECT_GE(relation.candidate_budget, 2u);
    EXPECT_NEAR(relation.coverage_fraction, 0.25, 1e-6);

    auto result = executeSQL("SELECT id FROM docs WHERE title = 'alpha'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    EXPECT_EQ(result.resultSet()->rowCount(), 2u);
}

TEST_F(QueryPlannerIntegrationTest,
       TextFamilyBelowPromotionThresholdPublishesStructuredRefusal)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE docs (id INTEGER, title TEXT)").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON docs TO PUBLIC").success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_docs_title_text ON docs USING FULLTEXT (title)")
            .success());

    ASSERT_TRUE(
        executeSQL("INSERT INTO docs (id, title) VALUES (1, 'alpha')").success());
    ASSERT_TRUE(
        executeSQL("INSERT INTO docs (id, title) VALUES (2, 'beta')").success());
    ASSERT_TRUE(
        executeSQL("INSERT INTO docs (id, title) VALUES (3, 'alpha')").success());

    auto analyze_result = executeSQL("ANALYZE docs");
    ASSERT_TRUE(analyze_result.success()) << analyze_result.error();

    core::ErrorContext ctx;
    CatalogManager::TableInfo table_info{};
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                              "docs",
                                              table_info,
                                              &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexInfo index_info{};
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id,
                                               "idx_docs_title_text",
                                               index_info,
                                               &ctx),
              Status::OK)
        << ctx.message;

    const uint64_t refresh_xid =
        std::max<uint64_t>(1, db_->storage_engine()->getCurrentXid());

    CatalogManager::IndexStatsCatalogInfo stats{};
    stats.index_id = index_info.index_id;
    stats.stats_version = 22;
    stats.last_analyze_txid = refresh_xid;
    stats.row_count_est = 3;
    stats.distinct_count_est = 2;
    stats.null_frac = 0.0f;
    stats.avg_key_len = 12;
    stats.avg_entry_len = 20;
    stats.leaf_pages = 2;
    stats.height = 1;
    stats.correlation = 0.0f;
    stats.bloat_ratio = 0.0f;
    stats.metrics_last_refresh_xid = refresh_xid;
    stats.family_metrics_version = 22;
    stats.family_metrics_type =
        scratchbird::optimizer::IndexFamilyMetricsType::TEXT_SEARCH;
    stats.metrics_confidence_class =
        scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
    stats.queryability_state =
        scratchbird::optimizer::IndexMetricsQueryabilityState::LIMITED;
    stats.is_valid = true;
    stats.family_metrics_payload =
        nlohmann::json{
            {"shared_metrics_envelope",
             {{"index_uuid", index_info.index_id.toString()},
              {"physical_family", "FULLTEXT"},
              {"planner_family", "TEXT_BITMAP_SCAN"},
              {"queryability_state", "LIMITED"},
              {"metrics_last_refresh_xid", refresh_xid},
              {"metrics_confidence_class", "HIGH"},
              {"leaf_pages", 2},
              {"height", 1},
              {"row_count_est", 3},
              {"live_entry_count_est", 3},
              {"dead_fraction", 0.0},
              {"bloat_ratio", 0.0},
              {"recheck_ratio_est", 1.0},
              {"correlation", 0.0},
              {"coverage_fraction", 0.25},
              {"maintenance_backlog_ops", 0},
              {"publish_lag_xids", 0},
              {"reclaim_lag_xids", 0}}},
            {"family_metrics_type", "TEXT_SEARCH"},
            {"family_metrics",
             {{"term_df", 2},
              {"term_df_skew", 1.0},
              {"avg_postings_per_term", 2.0},
              {"pending_list_fraction", 1.0},
              {"phrase_hit_rate", 0.0},
              {"score_rows_est", 1.0},
              {"merge_debt", 100.0},
              {"stale_hit_ratio", 1.0},
              {"collector_early_stop_gain", 0.0},
              {"mutable_overlay_fraction", 1.0}}}}
            .dump();

    ASSERT_EQ(db_->catalog_manager()->upsertIndexStatsCatalogEntry(stats, &ctx),
              Status::OK)
        << ctx.message;
    db_->statistics_manager()->invalidateCache(table_info.table_id);

    auto bytecode = compileSQL("SELECT id FROM docs WHERE title = 'alpha'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_kind, "SEQ_SCAN");
    EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "TEXT_BITMAP_SCAN"),
              relation.candidate_scan_families.end());

    const auto refusal_it = std::find_if(
        relation.candidate_family_refusals.begin(),
        relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.family == "TEXT_BITMAP_SCAN" &&
                   refusal.candidate_label ==
                       "TEXT_BITMAP_SCAN[idx_docs_title_text]" &&
                   refusal.refusal_class ==
                       "family-specific promotion threshold not met" &&
                   refusal.refusal_cause_domain == "METRICS" &&
                   refusal.refusal_reason_code ==
                       "P08_TEXT_NATIVE_PROMOTION_THRESHOLD_NOT_MET" &&
                   refusal.refusal_detail.find(
                       "text native promotion threshold not met") !=
                       std::string::npos;
        });

    std::ostringstream refusal_dump;
    for (const auto &refusal : relation.candidate_family_refusals)
    {
        refusal_dump << "[" << refusal.family << "|"
                     << refusal.candidate_label << "|"
                     << refusal.refusal_class << "|"
                     << refusal.refusal_cause_domain << "|"
                     << refusal.refusal_reason_code << "|"
                     << refusal.refusal_detail << "]";
    }
    EXPECT_NE(refusal_it, relation.candidate_family_refusals.end())
        << refusal_dump.str();
}

TEST_F(QueryPlannerIntegrationTest,
       CanonicalPlannerBundleRefusalClassifiesAdditionalPromotionThresholdFailures)
{
    const auto ann_native_class =
        optimizer::canonicalPlannerBundleRefusalClass(
            "P08_ANN_NATIVE_PROMOTION_THRESHOLD_NOT_MET",
            "ann native promotion threshold not met");
    EXPECT_EQ(ann_native_class, "family-specific promotion threshold not met");
    EXPECT_EQ(optimizer::canonicalPlannerBundleRefusalCauseDomain(
                  ann_native_class),
              "METRICS");

    const auto ann_fallback_class =
        optimizer::canonicalPlannerBundleRefusalClass(
            "P08_ANN_HYBRID_FALLBACK_NOT_JUSTIFIED",
            "hybrid fallback exactness not justified for a fully healthy ann path");
    EXPECT_EQ(ann_fallback_class,
              "family-specific promotion threshold not met");
    EXPECT_EQ(optimizer::canonicalPlannerBundleRefusalCauseDomain(
                  ann_fallback_class),
              "METRICS");

    const auto generalized_nearest_class =
        optimizer::canonicalPlannerBundleRefusalClass(
            "P08_GENERALIZED_NEAREST_NATIVE_PROMOTION_THRESHOLD_NOT_MET",
            "generalized nearest native promotion threshold not met");
    EXPECT_EQ(generalized_nearest_class,
              "family-specific promotion threshold not met");
    EXPECT_EQ(optimizer::canonicalPlannerBundleRefusalCauseDomain(
                  generalized_nearest_class),
              "METRICS");

    const auto generalized_class =
        optimizer::canonicalPlannerBundleRefusalClass(
            "P08_GENERALIZED_NATIVE_PROMOTION_THRESHOLD_NOT_MET",
            "generalized native promotion threshold not met");
    EXPECT_EQ(generalized_class,
              "family-specific promotion threshold not met");
    EXPECT_EQ(optimizer::canonicalPlannerBundleRefusalCauseDomain(
                  generalized_class),
              "METRICS");

    const auto text_score_rows_class =
        optimizer::canonicalPlannerBundleRefusalClass(
            "P08_TEXT_SCORE_ROWS_REQUIRED",
            "text score rows required for ranked path");
    EXPECT_EQ(text_score_rows_class,
              "family-specific promotion threshold not met");
    EXPECT_EQ(optimizer::canonicalPlannerBundleRefusalCauseDomain(
                  text_score_rows_class),
              "METRICS");
}

TEST_F(QueryPlannerIntegrationTest, SkipScanFamilyCanWinSingleRelationPlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_age_name ON users (age, name)").success());
    for (int i = 1; i <= 1600; ++i)
    {
        const std::string name = (i == 777) ? "needle" : "user" + std::to_string(i);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 8)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM users WHERE name = 'needle'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_family, "BTREE_SKIP_SCAN");
    EXPECT_EQ(plan.relations.front().path_name, "BTREE_SKIP_SCAN");
    EXPECT_EQ(plan.relations.front().scan_family_kind,
              scratchbird::optimizer::PlannerAccessFamily::BTREE_SKIP_SCAN);
    EXPECT_EQ(plan.relations.front().exactness_class,
              scratchbird::optimizer::AccessPathExactnessClass::EXACT_KEY);
    EXPECT_NE(std::find(plan.relations.front().candidate_scan_families.begin(),
                        plan.relations.front().candidate_scan_families.end(),
                        "BTREE_SKIP_SCAN"),
              plan.relations.front().candidate_scan_families.end());

    auto result = executeSQL("SELECT id FROM users WHERE name = 'needle'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "777");
}

TEST_F(QueryPlannerIntegrationTest,
       MultiRelationPlanPublishesBaseCandidateBundleDiagnostics)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(1000 + i) + ", " +
                               std::to_string(i) + ", " +
                               std::to_string(10 + i) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    auto bytecode = compileSQL(
        "SELECT u.id, o.amount "
        "FROM users AS u "
        "JOIN orders AS o ON o.user_id = u.id "
        "WHERE u.id = 150 AND o.user_id = 150");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);

    const auto user_relation_it =
        std::find_if(plan.relations.begin(),
                     plan.relations.end(),
                     [](const scratchbird::optimizer::RuntimePlanRelation &relation) {
                         return relation.alias == "u";
                     });
    ASSERT_NE(user_relation_it, plan.relations.end());
    EXPECT_GE(user_relation_it->candidate_budget, 2u);
    EXPECT_NE(std::find(user_relation_it->candidate_scan_families.begin(),
                        user_relation_it->candidate_scan_families.end(),
                        "SEQ_SCAN"),
              user_relation_it->candidate_scan_families.end());
    EXPECT_NE(std::find(user_relation_it->candidate_scan_families.begin(),
                        user_relation_it->candidate_scan_families.end(),
                        "BTREE_EQ_SCAN"),
              user_relation_it->candidate_scan_families.end());

    const auto order_relation_it =
        std::find_if(plan.relations.begin(),
                     plan.relations.end(),
                     [](const scratchbird::optimizer::RuntimePlanRelation &relation) {
                         return relation.alias == "o";
                     });
    ASSERT_NE(order_relation_it, plan.relations.end());
    EXPECT_NE(std::find(order_relation_it->candidate_scan_families.begin(),
                        order_relation_it->candidate_scan_families.end(),
                        "SEQ_SCAN"),
              order_relation_it->candidate_scan_families.end());
    EXPECT_NE(std::find(order_relation_it->candidate_scan_families.begin(),
                        order_relation_it->candidate_scan_families.end(),
                        "BTREE_EQ_SCAN"),
              order_relation_it->candidate_scan_families.end());
}

TEST_F(QueryPlannerIntegrationTest,
       PartialAndExpressionIndexesDoNotBreakSingleRelationPlanning)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE INDEX idx_users_active_email ON users(email) WHERE age = 30")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE INDEX idx_users_lower_name ON users((LOWER(name)))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'MiXeD', 'mixed@example.com', 30)")
                    .success());
    for (int i = 2; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 15)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL(
        "SELECT id FROM users "
        "WHERE age = 30 AND email = 'mixed@example.com' AND LOWER(name) = 'mixed'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_FALSE(plan.relations.front().scan_family.empty());
    EXPECT_NE(plan.relations.front().scan_family_kind,
              scratchbird::optimizer::PlannerAccessFamily::UNKNOWN);

    auto result = executeSQL(
        "SELECT id FROM users "
        "WHERE age = 30 AND email = 'mixed@example.com' AND LOWER(name) = 'mixed'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
}

TEST_F(QueryPlannerIntegrationTest,
       HashEqualityPredicatePublishesCanonicalFamilyCandidate)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_age_hash ON users USING HASH (age)")
            .success());
    for (int i = 1; i <= 512; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 40)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM users WHERE age = 30");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_NE(std::find(plan.relations.front().candidate_scan_families.begin(),
                        plan.relations.front().candidate_scan_families.end(),
                        "HASH_EQ_SCAN"),
              plan.relations.front().candidate_scan_families.end());
}

TEST_F(QueryPlannerIntegrationTest,
       HashRangePredicateFailsClosedToSequentialScan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_age_hash ON users USING HASH (age)")
            .success());
    for (int i = 1; i <= 512; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 40)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM users WHERE age > 30");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_family, "SEQ_SCAN");
    EXPECT_TRUE(relation.index_name.empty());
    EXPECT_EQ(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "HASH_EQ_SCAN"),
              relation.candidate_scan_families.end());

    const auto refusal_it = std::find_if(
        relation.candidate_family_refusals.begin(),
        relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.family == "HASH_EQ_SCAN" &&
                   refusal.refusal_class == "unsupported operator shape" &&
                   refusal.refusal_cause_domain == "OPERATOR" &&
                   refusal.refusal_reason_code ==
                       "P08_HASH_EQ_PREDICATE_REQUIRED";
        });
    std::ostringstream refusal_dump;
    for (const auto &refusal : relation.candidate_family_refusals)
    {
        refusal_dump << "[" << refusal.family << "|"
                     << refusal.candidate_label << "|"
                     << refusal.refusal_class << "|"
                     << refusal.refusal_cause_domain << "|"
                     << refusal.refusal_reason_code << "|"
                     << refusal.refusal_detail << "]";
    }
    EXPECT_NE(refusal_it, relation.candidate_family_refusals.end())
        << refusal_dump.str();
}

TEST_F(QueryPlannerIntegrationTest,
       GistCandidateRequiresBoundOpclassStrategySupport)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE gist_docs (id INT PRIMARY KEY, geom INT)")
                    .success());
    ASSERT_TRUE(
        executeSQL("GRANT SELECT ON gist_docs TO PUBLIC").success());
    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_gist_docs_geom ON gist_docs USING GIST (geom)")
            .success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO gist_docs (id, geom) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(i % 32) +
                               ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE gist_docs").success());

    const std::string sql = "SELECT id FROM gist_docs WHERE geom = 5";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &initial_relation = plan.relations.front();
    EXPECT_EQ(std::find(initial_relation.candidate_scan_families.begin(),
                        initial_relation.candidate_scan_families.end(),
                        "GIST_SCAN"),
              initial_relation.candidate_scan_families.end());
    auto* catalog = db_ != nullptr ? db_->catalog_manager() : nullptr;
    ASSERT_NE(catalog, nullptr);

    ErrorContext ctx;
    CatalogManager::SchemaInfo public_schema;
    ASSERT_EQ(catalog->getSchema("public", public_schema, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::TableInfo table_info;
    ASSERT_EQ(
        catalog->getTable(public_schema.schema_id, "gist_docs", table_info, &ctx),
        Status::OK)
        << ctx.message;

    std::vector<CatalogManager::ColumnInfo> columns;
    ASSERT_EQ(catalog->getColumns(table_info.table_id, columns, &ctx), Status::OK)
        << ctx.message;
    const auto column_it = std::find_if(
        columns.begin(),
        columns.end(),
        [](const CatalogManager::ColumnInfo& info) {
            return info.column_name == "geom";
        });
    ASSERT_NE(column_it, columns.end());

    std::vector<CatalogManager::IndexInfo> indexes;
    ASSERT_EQ(catalog->listIndexesForTable(table_info.table_id, indexes, &ctx),
              Status::OK)
        << ctx.message;
    const auto index_it = std::find_if(
        indexes.begin(),
        indexes.end(),
        [](const CatalogManager::IndexInfo& info) {
            return info.index_name == "idx_gist_docs_geom";
        });
    ASSERT_NE(index_it, indexes.end());

    std::vector<CatalogManager::SchemaInfo> schemas;
    ASSERT_EQ(catalog->listSchemas(schemas, &ctx), Status::OK) << ctx.message;
    ID any_type_id{};
    for (const auto& schema : schemas)
    {
        std::vector<CatalogManager::TypeCatalogInfo> types;
        if (catalog->listTypeCatalogEntries(schema.schema_id, types, &ctx) !=
                Status::OK ||
            types.empty())
        {
            continue;
        }
        any_type_id = types.front().type_id;
        break;
    }
    if (isZeroId(any_type_id))
    {
        CatalogManager::TypeCatalogInfo synthetic_type{};
        synthetic_type.schema_id = public_schema.schema_id;
        synthetic_type.type_name = "gist_docs_test_type";
        synthetic_type.type_kind = CatalogManager::TypeKind::SCALAR;
        ASSERT_EQ(catalog->upsertTypeCatalogEntry(synthetic_type,
                                                  any_type_id,
                                                  &ctx),
                  Status::OK)
            << ctx.message;
    }
    ASSERT_FALSE(isZeroId(any_type_id));

    CatalogManager::IndexOpclassCatalogInfo opclass{};
    opclass.opclass_name = "gist_docs_eq_support";
    opclass.index_type_name = "GIST";
    opclass.input_type_id = any_type_id;
    opclass.owner_schema_id = public_schema.schema_id;

    ID opclass_id{};
    ASSERT_EQ(catalog->upsertIndexOpclassCatalogEntry(opclass, opclass_id, &ctx),
              Status::OK)
        << ctx.message;

    std::vector<CatalogManager::IndexColumnCatalogInfo> index_columns;
    ASSERT_EQ(catalog->listIndexColumnCatalogEntries(index_it->index_id,
                                                     index_columns,
                                                     &ctx),
              Status::OK)
        << ctx.message;
    CatalogManager::IndexColumnCatalogInfo key_column{};
    if (!index_columns.empty())
    {
        key_column = index_columns.front();
    }
    else
    {
        key_column.index_id = index_it->index_id;
        key_column.position = 1;
        key_column.column_id = column_it->column_id;
        key_column.sort_order = CatalogManager::IndexSortOrder::ASC;
        key_column.null_order = CatalogManager::IndexNullOrder::LAST;
        key_column.is_valid = true;
    }
    ID index_column_id = key_column.index_column_id;
    key_column.opclass_id = opclass_id;
    ASSERT_EQ(catalog->upsertIndexColumnCatalogEntry(key_column,
                                                     index_column_id,
                                                     &ctx),
              Status::OK)
        << ctx.message;

    bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &pre_support_relation = plan.relations.front();
    EXPECT_EQ(std::find(pre_support_relation.candidate_scan_families.begin(),
                        pre_support_relation.candidate_scan_families.end(),
                        "GIST_SCAN"),
              pre_support_relation.candidate_scan_families.end());
    const auto pre_support_refusal_it = std::find_if(
        pre_support_relation.candidate_family_refusals.begin(),
        pre_support_relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.family == "GIST_SCAN" &&
                   refusal.refusal_class == "unsupported operator shape" &&
                   refusal.refusal_cause_domain == "OPERATOR" &&
                   refusal.refusal_reason_code ==
                       "P08_OPERATOR_STRATEGY_UNBOUND" &&
                   refusal.refusal_detail.find(
                       "strategy binding was not available") !=
                       std::string::npos;
        });
    std::ostringstream refusal_dump;
    for (const auto &refusal : pre_support_relation.candidate_family_refusals)
    {
        refusal_dump << "[" << refusal.family << "|"
                     << refusal.candidate_label << "|"
                     << refusal.refusal_class << "|"
                     << refusal.refusal_cause_domain << "|"
                     << refusal.refusal_reason_code << "|"
                     << refusal.refusal_detail << "]";
    }
    EXPECT_NE(pre_support_refusal_it,
              pre_support_relation.candidate_family_refusals.end())
        << refusal_dump.str();

    CatalogManager::IndexOpclassFunctionCatalogInfo consistent{};
    consistent.opclass_id = opclass_id;
    consistent.fn_kind =
        CatalogManager::IndexOpclassFunctionKind::CONSISTENT;
    consistent.function_id = generateUuidV7();
    consistent.support_number = 8;

    ID consistent_fn_id{};
    ASSERT_EQ(catalog->upsertIndexOpclassFunctionCatalogEntry(
                  consistent, consistent_fn_id, &ctx),
              Status::OK)
        << ctx.message;

    bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_NE(std::find(plan.relations.front().candidate_scan_families.begin(),
                        plan.relations.front().candidate_scan_families.end(),
                        "GIST_SCAN"),
              plan.relations.front().candidate_scan_families.end());
}

TEST_F(QueryPlannerIntegrationTest,
       PartialIndexRequiresPredicateImplicationBeforeEnumeration)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE INDEX idx_users_active_email ON users(email) WHERE age = 30")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'MiXeD', 'mixed@example.com', 30)")
                    .success());
    for (int i = 2; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 15)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id FROM users WHERE email = 'mixed@example.com'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "PARTIAL_INDEX_SCAN"),
              relation.candidate_scan_families.end());
    EXPECT_EQ(relation.scan_family, "SEQ_SCAN");

    const auto refusal_it = std::find_if(
        relation.candidate_family_refusals.begin(),
        relation.candidate_family_refusals.end(),
        [](const scratchbird::optimizer::RuntimePlanCandidateRefusal &refusal) {
            return refusal.refusal_class == "semantic mismatch" &&
                   refusal.refusal_cause_domain == "SEMANTICS" &&
                   refusal.refusal_reason_code ==
                       "P08_PARTIAL_INDEX_PREDICATE_MISMATCH" &&
                   refusal.refusal_detail ==
                       "partial index predicate not implied by relation filter";
        });
    std::ostringstream refusal_dump;
    for (const auto &refusal : relation.candidate_family_refusals)
    {
        refusal_dump << "[" << refusal.family << "|"
                     << refusal.candidate_label << "|"
                     << refusal.refusal_class << "|"
                     << refusal.refusal_cause_domain << "|"
                     << refusal.refusal_reason_code << "|"
                     << refusal.refusal_detail << "]";
    }
    EXPECT_NE(refusal_it, relation.candidate_family_refusals.end())
        << refusal_dump.str();
}

TEST_F(QueryPlannerIntegrationTest, PassThroughViewIsFlattenedInRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE VIEW v_users_flat AS SELECT * FROM users").success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (42, 'flattened', 'f@example.com', 25)")
                    .success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM v_users_flat WHERE id = 42");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_TRUE(plan.relations.front().flattened_derived);
    EXPECT_FALSE(plan.relations.front().physical_table_path.empty());
    EXPECT_EQ(plan.relations.front().physical_table_path, "users");

    auto result = executeSQL("SELECT id FROM v_users_flat WHERE id = 42");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "42");
}

TEST_F(QueryPlannerIntegrationTest, LateralJoinUsesParameterizedNestedLoopPath)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (10, 1, 50.0)")
                    .success());

    const std::string sql =
        "SELECT u.id, o.order_id "
        "FROM users AS u "
        "LEFT JOIN LATERAL "
        "(SELECT id AS order_id FROM orders WHERE orders.user_id = u.id) AS o "
        "ON TRUE "
        "ORDER BY u.id";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);
    EXPECT_TRUE(plan.relations[1].lateral);
    EXPECT_TRUE(plan.relations[1].parameterized);
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps[0].method, "PARAMETERIZED_NESTED_LOOP");
    EXPECT_TRUE(plan.join_steps[0].parameterized_dependency);
    ASSERT_EQ(plan.join_steps[0].legal_method_families.size(), 1u);
    EXPECT_EQ(plan.join_steps[0].legal_method_families[0],
              "PARAMETERIZED_NESTED_LOOP");
    ASSERT_EQ(plan.join_steps[0].method_enablers.size(), 1u);
    EXPECT_EQ(plan.join_steps[0].method_enablers[0],
              "OUTER_PARAMETER_BINDING");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 2u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "10");
    EXPECT_EQ(result.resultSet()->getValue(1, 0).toString(), "2");
    EXPECT_TRUE(result.resultSet()->getValue(1, 1).isNull());
}

TEST_F(QueryPlannerIntegrationTest,
       LateralJoinPublishesParameterizedIndexCandidateFamily)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (10, 1, 50.0)")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT u.id, o.order_id "
        "FROM users AS u "
        "LEFT JOIN LATERAL "
        "(SELECT id AS order_id FROM orders WHERE orders.user_id = u.id) AS o "
        "ON TRUE "
        "ORDER BY u.id";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);

    const auto relation_it =
        std::find_if(plan.relations.begin(),
                     plan.relations.end(),
                     [](const scratchbird::optimizer::RuntimePlanRelation &relation) {
                         return relation.alias == "o" || relation.table_path == "orders";
                     });
    ASSERT_NE(relation_it, plan.relations.end());
    EXPECT_TRUE(relation_it->parameterized);
    EXPECT_NE(std::find(relation_it->candidate_scan_families.begin(),
                        relation_it->candidate_scan_families.end(),
                        "PARAMETERIZED_INDEX_SCAN"),
              relation_it->candidate_scan_families.end());
    EXPECT_NE(std::find(relation_it->required_outer_relation_aliases.begin(),
                        relation_it->required_outer_relation_aliases.end(),
                        "u"),
              relation_it->required_outer_relation_aliases.end());
}

TEST_F(QueryPlannerIntegrationTest,
       ParameterizedNestedLoopMemoizesRepeatedOuterBindings)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE probe_keys (seq INTEGER, user_id INTEGER)")
                    .success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());

    for (int seq = 1; seq <= 5; ++seq)
    {
        const int user_id = seq <= 3 ? 1 : 2;
        ASSERT_TRUE(executeSQL("INSERT INTO probe_keys (seq, user_id) VALUES (" +
                               std::to_string(seq) + ", " +
                               std::to_string(user_id) + ")")
                        .success());
    }

    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (10, 1, 50.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (11, 1, 60.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (20, 2, 70.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (21, 2, 80.0)")
                    .success());

    ASSERT_TRUE(executeSQL("ANALYZE probe_keys").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT p.seq, o.order_id "
        "FROM probe_keys AS p "
        "LEFT JOIN LATERAL "
        "(SELECT id AS order_id FROM orders WHERE orders.user_id = p.user_id) AS o "
        "ON TRUE "
        "ORDER BY p.seq, o.order_id";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "PARAMETERIZED_NESTED_LOOP");
    EXPECT_TRUE(plan.join_steps.front().parameterized_dependency);

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_parameterized_memoize_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 10u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "10");
    EXPECT_EQ(result.resultSet()->getValue(5, 0).toString(), "3");
    EXPECT_EQ(result.resultSet()->getValue(5, 1).toString(), "11");
    EXPECT_EQ(result.resultSet()->getValue(9, 0).toString(), "5");
    EXPECT_EQ(result.resultSet()->getValue(9, 1).toString(), "21");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("SELECT TRACE join[0] method=PARAMETERIZED_NESTED_LOOP"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("memoize_hits=3"), std::string::npos) << trace;
    EXPECT_NE(trace.find("memoize_misses=2"), std::string::npos) << trace;
    EXPECT_NE(trace.find("memoize_entries=2"), std::string::npos) << trace;
    EXPECT_NE(trace.find("physical_loops=2"), std::string::npos) << trace;

    size_t orders_trace_count = 0;
    std::istringstream trace_stream(trace);
    std::string trace_line;
    while (std::getline(trace_stream, trace_line))
    {
        if (trace_line.find("SELECT TRACE table=orders") != std::string::npos)
        {
            ++orders_trace_count;
        }
    }
    EXPECT_EQ(orders_trace_count, 2u) << trace;
}

TEST_F(QueryPlannerIntegrationTest, CorrelatedExistsAndNotExistsUseOuterRowBindings)
{
    ASSERT_TRUE(createDatabase());

    for (int id : {1, 2, 3})
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(id) + ", 'u" + std::to_string(id) +
                               "', 'u" + std::to_string(id) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (100, 1, 25.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (101, 3, 30.0)")
                    .success());

    auto exists_result = executeSQL(
        "SELECT u.id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id) "
        "ORDER BY u.id");
    ASSERT_TRUE(exists_result.success()) << exists_result.error();
    ASSERT_TRUE(exists_result.hasResultSet());
    ASSERT_EQ(exists_result.resultSet()->rowCount(), 2u);
    EXPECT_EQ(exists_result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(exists_result.resultSet()->getValue(1, 0).toString(), "3");

    auto not_exists_result = executeSQL(
        "SELECT u.id FROM users AS u "
        "WHERE NOT EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id) "
        "ORDER BY u.id");
    ASSERT_TRUE(not_exists_result.success()) << not_exists_result.error();
    ASSERT_TRUE(not_exists_result.hasResultSet());
    ASSERT_EQ(not_exists_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(not_exists_result.resultSet()->getValue(0, 0).toString(), "2");
}

TEST_F(QueryPlannerIntegrationTest, PartitionedParentSelectPrunesChildTargetsAndReturnsRows)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement (id INTEGER, region INTEGER, note VARCHAR(32)) "
                    "PARTITION BY RANGE (region)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_p1 (id INTEGER, region INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_p2 (id INTEGER, region INTEGER, note VARCHAR(32))")
                    .success());
    const auto create_metadata = loadTableMetadataJson("measurement");
    ASSERT_TRUE(create_metadata.contains("partition"));
    ASSERT_TRUE(create_metadata["partition"].is_object());
    EXPECT_EQ(create_metadata["partition"].value("strategy", std::string()), "RANGE");
    ASSERT_TRUE(create_metadata["partition"].contains("columns"));
    ASSERT_TRUE(create_metadata["partition"]["columns"].is_array());
    ASSERT_EQ(create_metadata["partition"]["columns"].size(), 1u);
    EXPECT_EQ(create_metadata["partition"]["columns"][0].get<std::string>(), "region");
    {
        auto attach_p1 = executeSQL(
            "ALTER TABLE measurement ATTACH PARTITION measurement_p1 "
            "FOR VALUES FROM (0) TO (100)");
        ASSERT_TRUE(attach_p1.success()) << attach_p1.error();
    }
    {
        auto attach_p2 = executeSQL(
            "ALTER TABLE measurement ATTACH PARTITION measurement_p2 "
            "FOR VALUES FROM (100) TO (200)");
        ASSERT_TRUE(attach_p2.success()) << attach_p2.error();
    }

    const auto metadata = loadTableMetadataJson("measurement");
    ASSERT_TRUE(metadata.contains("partition"));
    ASSERT_TRUE(metadata["partition"].is_object());
    EXPECT_EQ(metadata["partition"].value("strategy", std::string()), "RANGE");
    ASSERT_TRUE(metadata["partition"].contains("columns"));
    ASSERT_TRUE(metadata["partition"]["columns"].is_array());
    ASSERT_EQ(metadata["partition"]["columns"].size(), 1u);
    EXPECT_EQ(metadata["partition"]["columns"][0].get<std::string>(), "region");
    ASSERT_TRUE(metadata["partition"].contains("children"));
    ASSERT_TRUE(metadata["partition"]["children"].is_array());
    ASSERT_EQ(metadata["partition"]["children"].size(), 2u);

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_p1 (id, region, note) VALUES (1, 10, 'west')")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_p2 (id, region, note) VALUES (2, 150, 'east')")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_p1").success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_p2").success());

    const std::string sql =
        "SELECT id, note FROM measurement WHERE region >= 100 AND region < 200 ORDER BY id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_TRUE(plan.relations.front().partition_pruned);
    EXPECT_EQ(plan.relations.front().partition_strategy, "RANGE");
    EXPECT_EQ(plan.relations.front().partition_key_column, "region");
    ASSERT_EQ(plan.relations.front().partition_targets.size(), 1u);
    EXPECT_EQ(plan.relations.front().partition_targets.front(), "measurement_p2");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "east");
}

TEST_F(QueryPlannerIntegrationTest, MultiColumnStaticPartitionPruningPublishesPrunedTargets)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32)) "
                    "PARTITION BY LIST (region, bucket)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p1 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p2 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p1 "
                    "FOR VALUES IN ((1, 10), (1, 20))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p2 "
                    "FOR VALUES IN ((2, 30), (2, 40))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_mc_p1 (id, region, bucket, note) "
                    "VALUES (1, 1, 10, 'west')")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_mc_p2 (id, region, bucket, note) "
                    "VALUES (2, 2, 40, 'east')")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_mc_p1").success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_mc_p2").success());

    const std::string sql =
        "SELECT id, note FROM measurement_mc WHERE region = 2 AND bucket = 40";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto& relation = plan.relations.front();
    EXPECT_TRUE(relation.partition_pruned);
    EXPECT_EQ(relation.partition_strategy, "LIST");
    ASSERT_EQ(relation.partition_key_columns.size(), 2u);
    EXPECT_EQ(relation.partition_key_columns[0], "region");
    EXPECT_EQ(relation.partition_key_columns[1], "bucket");
    ASSERT_EQ(relation.partition_targets.size(), 1u);
    EXPECT_EQ(relation.partition_targets.front(), "measurement_mc_p2");
    ASSERT_EQ(relation.partition_targets_pruned_at_plan.size(), 1u);
    EXPECT_EQ(relation.partition_targets_pruned_at_plan.front(),
              "measurement_mc_p1");
    EXPECT_FALSE(relation.runtime_partition_pruning_eligible);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "east");
}

TEST_F(QueryPlannerIntegrationTest, ParameterSensitivePartitionPruningUsesBoundValuesAtPlanTime)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32)) "
                    "PARTITION BY LIST (region, bucket)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p1 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p2 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p1 "
                    "FOR VALUES IN ((1, 10), (1, 20))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p2 "
                    "FOR VALUES IN ((2, 30), (2, 40))")
                    .success());

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "2"});
    bindings.positional.push_back({false, "40"});

    auto compile_result = compileSQLWithParameters(
        "SELECT id FROM measurement_mc WHERE region = $1 AND bucket = $2",
        bindings);
    ASSERT_TRUE(compile_result.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(compile_result.bytecode(), plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto& relation = plan.relations.front();
    EXPECT_TRUE(plan.parameter_sensitive);
    EXPECT_TRUE(relation.partition_pruned);
    EXPECT_FALSE(relation.runtime_partition_pruning_eligible);
    ASSERT_EQ(relation.partition_targets.size(), 1u);
    EXPECT_EQ(relation.partition_targets.front(), "measurement_mc_p2");
    ASSERT_EQ(relation.partition_targets_pruned_at_plan.size(), 1u);
    EXPECT_EQ(relation.partition_targets_pruned_at_plan.front(),
              "measurement_mc_p1");
}

TEST_F(QueryPlannerIntegrationTest, GenericPlanRuntimePartitionPruningUsesLateParameterValues)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32)) "
                    "PARTITION BY LIST (region, bucket)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p1 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p2 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p1 "
                    "FOR VALUES IN ((1, 10), (1, 20))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p2 "
                    "FOR VALUES IN ((2, 30), (2, 40))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_mc_p1 (id, region, bucket, note) "
                    "VALUES (1, 1, 10, 'west')")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_mc_p2 (id, region, bucket, note) "
                    "VALUES (2, 2, 40, 'east')")
                    .success());

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES",
                                        "PLAN_PROFILE=GENERIC");
    optimizer::ParameterBindings compile_bindings;
    compile_bindings.positional.push_back({false, "1"});
    compile_bindings.positional.push_back({false, "10"});

    auto compile_result = compileSQLWithParameters(
        "SELECT id, note FROM measurement_mc WHERE region = $1 AND bucket = $2",
        compile_bindings);
    ASSERT_TRUE(compile_result.success()) << last_compile_errors_;
    EXPECT_EQ(compile_result.planProfile().mode,
              scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::GENERIC);
    EXPECT_FALSE(compile_result.planProfile().parameter_sensitive);
    const auto& bytecode = compile_result.bytecode();

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto& relation = plan.relations.front();
    EXPECT_FALSE(plan.parameter_sensitive);
    EXPECT_FALSE(relation.partition_pruned);
    EXPECT_TRUE(relation.runtime_partition_pruning_eligible);
    ASSERT_EQ(relation.runtime_partition_pruning_sources.size(), 1u);
    EXPECT_EQ(relation.runtime_partition_pruning_sources.front(), "PARAMETER");
    ASSERT_EQ(relation.partition_predicates.size(), 2u);

    auto result = executeBytecodeWithParameters(bytecode,
                                                {"2", "40"},
                                                {false, false});
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    CatalogManager::TableInfo p1_info;
    CatalogManager::TableInfo p2_info;
    ErrorContext table_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "measurement_mc_p1",
                                               p1_info,
                                               &table_ctx),
              Status::OK)
        << table_ctx.message;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "measurement_mc_p2",
                                               p2_info,
                                               &table_ctx),
              Status::OK)
        << table_ctx.message;

    const auto& touched_tables = executor_->getLastSelectTableIds();
    EXPECT_EQ(touched_tables.count(p1_info.table_id), 0u);
    EXPECT_EQ(touched_tables.count(p2_info.table_id), 1u);
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "east");
}

TEST_F(QueryPlannerIntegrationTest, JoinRuntimeFilterUsesRightSideIndexMetadata)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)").success());
    for (int id = 1; id <= 4; ++id)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(id) + ", 'u" + std::to_string(id) +
                               "', 'u" + std::to_string(id) + "@example.com', 30)")
                        .success());
    }
    for (int order_id = 1; order_id <= 12; ++order_id)
    {
        const int user_id = ((order_id - 1) % 4) + 1;
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(order_id) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(10 + order_id) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT o.user_id, o.amount "
        "FROM users AS u "
        "JOIN orders AS o ON u.id = o.user_id "
        "WHERE u.id <= 2 "
        "ORDER BY o.user_id, o.amount";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    auto relation_it =
        std::find_if(plan.relations.begin(),
                     plan.relations.end(),
                     [](const scratchbird::optimizer::RuntimePlanRelation& relation) {
                         return relation.table_path == "orders" || relation.alias == "o";
                     });
    ASSERT_NE(relation_it, plan.relations.end());
    EXPECT_TRUE(relation_it->runtime_filter_enabled);
    EXPECT_EQ(relation_it->runtime_filter_column, "user_id");
    EXPECT_EQ(relation_it->runtime_filter_index_name, "idx_orders_user_id");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 6u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(5, 0).toString(), "2");
}

TEST_F(QueryPlannerIntegrationTest,
       NestedLoopJoinUsesBatchedKeyAccessRuntimeFilterProbe)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE probe_users (seq INTEGER, user_id INTEGER)")
                    .success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());

    for (int seq = 1; seq <= 7; ++seq)
    {
        const int user_id = seq <= 3 ? 1 : (seq <= 5 ? 2 : 9);
        ASSERT_TRUE(executeSQL("INSERT INTO probe_users (seq, user_id) VALUES (" +
                               std::to_string(seq) + ", " +
                               std::to_string(user_id) + ")")
                        .success());
    }

    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (10, 1, 50.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (11, 1, 60.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (20, 2, 70.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (21, 2, 80.0)")
                    .success());

    ASSERT_TRUE(executeSQL("ANALYZE probe_users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());
    ASSERT_TRUE(executeSQL("SET OPTIMIZER.JOIN_METHOD = 'NESTED_LOOP'").success());
    ASSERT_TRUE(executeSQL("SET OPTIMIZER.JOIN_SEARCH = 'INPUT_ORDER'").success());

    const std::string sql =
        "SELECT p.seq, o.id "
        "FROM probe_users AS p "
        "JOIN orders AS o ON p.user_id = o.user_id "
        "ORDER BY p.seq, o.id";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");
    EXPECT_NE(std::find(plan.join_steps.front().method_enablers.begin(),
                        plan.join_steps.front().method_enablers.end(),
                        "BATCHED_KEY_ACCESS"),
              plan.join_steps.front().method_enablers.end());

    auto relation_it =
        std::find_if(plan.relations.begin(),
                     plan.relations.end(),
                     [](const scratchbird::optimizer::RuntimePlanRelation& relation) {
                         return relation.table_path == "orders" || relation.alias == "o";
                     });
    ASSERT_NE(relation_it, plan.relations.end());
    EXPECT_TRUE(relation_it->runtime_filter_enabled);
    EXPECT_EQ(relation_it->runtime_filter_index_name, "idx_orders_user_id");

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_bka_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 10u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "10");
    EXPECT_EQ(result.resultSet()->getValue(9, 0).toString(), "5");
    EXPECT_EQ(result.resultSet()->getValue(9, 1).toString(), "21");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("SELECT TRACE join[0] method=NESTED_LOOP bka=1"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("batched_keys=3"), std::string::npos) << trace;
    EXPECT_NE(trace.find("index=idx_orders_user_id"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("runtime_access=RUNTIME_FILTER_INDEX_PROBE"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("probe_mode=ORDERED_EXACT_BKA_PROBE"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("rows_examined=10"), std::string::npos) << trace;
    EXPECT_NE(trace.find("replay_rows=10"), std::string::npos) << trace;

    size_t orders_trace_count = 0;
    std::istringstream trace_stream(trace);
    std::string trace_line;
    while (std::getline(trace_stream, trace_line))
    {
        if (trace_line.find("SELECT TRACE table=orders") != std::string::npos)
        {
            ++orders_trace_count;
        }
    }
    EXPECT_EQ(orders_trace_count, 1u) << trace;
}

TEST_F(QueryPlannerIntegrationTest, TopNSortPlanAnnotatesRuntimePlanAndExecutesCorrectly)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'topn" + std::to_string(i) +
                               "', 'topn" + std::to_string(i) +
                               "@example.com', 20)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT id FROM users ORDER BY id DESC LIMIT 5";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Limit");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "Sort");
    EXPECT_NE(plan.root.children.front().detail_text.find("topn=5"), std::string::npos);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 5u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "128");
    EXPECT_EQ(result.resultSet()->getValue(4, 0).toString(), "124");
}

TEST_F(QueryPlannerIntegrationTest,
       GroupedAggregateReusesOrderedInputAndAvoidsExplicitSort)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());
    for (int i = 1; i <= 8192; ++i)
    {
        const int user_id = 1 + (i % 32);
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(i) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(10 + (i % 17)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT user_id, COUNT(*) "
        "FROM orders "
        "WHERE user_id <= 5 "
        "GROUP BY user_id "
        "ORDER BY user_id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");

    const auto group_trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:group_by" &&
                                entry.candidate == "GROUP_ORDER_REUSE" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(group_trace_it, plan.considered_paths.end());

    const auto order_trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "EXISTING_ORDER" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(order_trace_it, plan.considered_paths.end());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 5u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(4, 0).toString(), "5");
}

TEST_F(QueryPlannerIntegrationTest,
       AggregateStagePublishesHashStrategyWhenGroupOrderUnavailable)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'agg" +
                               std::to_string(i) + "', 'agg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 7)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT age, COUNT(*) FROM users GROUP BY age";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:aggregate" &&
                                entry.candidate == "HASH_AGGREGATE" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:aggregate" &&
                                entry.candidate == "GROUP_AGGREGATE_REUSE_ORDER" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       DistinctStageUsesOrderedDistinctWhenInputOrderIsAvailable)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());
    for (int i = 1; i <= 512; ++i)
    {
        const int user_id = 1 + (i % 8);
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(i) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(10 + (i % 11)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT DISTINCT user_id FROM orders WHERE user_id <= 5 ORDER BY user_id";
    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_ordered_distinct_runtime_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");
    EXPECT_EQ(plan.root.detail_text, "DISTINCT");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:distinct" &&
                                entry.candidate == "ORDERED_DISTINCT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    auto plain_result =
        executeSQL("SELECT user_id FROM orders WHERE user_id <= 5 ORDER BY user_id");
    ASSERT_TRUE(plain_result.success()) << plain_result.error();
    const auto plain_rows = resultStrings(plain_result);
    ASSERT_EQ(plain_rows.size(), 320u);
    EXPECT_EQ(plain_rows.front(), "1");
    EXPECT_EQ(plain_rows.back(), "5");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows.front(), "1");
    EXPECT_EQ(rows.back(), "5");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find(
            "SELECT TRACE distinct key_mode=FAST_SCALAR spill=0 input_rows=320 output_rows=5 mode=ORDERED_STREAM"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("stream_compare=FAST_ADJACENT"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("SELECT TRACE output handoff=BATCH_ROWS rows=5"),
              std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       DistinctStageUsesHashDistinctWhenOrderIsUnavailable)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'distinct" +
                               std::to_string(i) + "', 'distinct" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 6)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT DISTINCT age FROM users");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:distinct" &&
                                entry.candidate == "HASH_DISTINCT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:distinct" &&
                                entry.candidate == "ORDERED_DISTINCT" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedCompositeDistinctUsesFastAdjacentStreamingComparison)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE TABLE distinct_pairs (bucket INTEGER, shard INTEGER)")
            .success());
    for (int bucket = 1; bucket <= 4; ++bucket)
    {
        for (int shard = 1; shard <= 3; ++shard)
        {
            for (int dup = 0; dup < 16; ++dup)
            {
                ASSERT_TRUE(
                    executeSQL("INSERT INTO distinct_pairs (bucket, shard) VALUES (" +
                               std::to_string(bucket) + ", " +
                               std::to_string(shard) + ")")
                        .success());
            }
        }
    }
    ASSERT_TRUE(executeSQL("ANALYZE distinct_pairs").success());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_ordered_distinct_composite_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT DISTINCT bucket, shard "
        "FROM distinct_pairs "
        "ORDER BY bucket, shard");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 12u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(11, 0).toString(), "4");
    EXPECT_EQ(result.resultSet()->getValue(11, 1).toString(), "3");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find(
            "SELECT TRACE distinct key_mode=FAST_COMPOSITE spill=0 input_rows=192 output_rows=12 mode=ORDERED_STREAM"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("stream_compare=FAST_ADJACENT"), std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       WindowPreservesOrderedInputAndAvoidsFinalSort)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'wuser" +
                               std::to_string(i) + "', 'wuser" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT id, ROW_NUMBER() OVER (ORDER BY id) AS rn "
        "FROM users "
        "WHERE id >= 200 "
        "ORDER BY id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Window");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");

    const auto trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "EXISTING_ORDER" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(trace_it, plan.considered_paths.end());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 57u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "200");
    EXPECT_EQ(result.resultSet()->getValue(56, 0).toString(), "256");
}

TEST_F(QueryPlannerIntegrationTest,
       WindowStagePublishesLocalSortStrategyWhenOrderIsMissing)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 64; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'z" +
                               std::to_string(65 - i) + "', 'window" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }

    auto bytecode =
        compileSQL("SELECT ROW_NUMBER() OVER (ORDER BY name) FROM users");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:window" &&
                                entry.candidate == "WINDOW_LOCAL_SORT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:window" &&
                                entry.candidate == "WINDOW_REUSE_ORDER" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       WindowReusesAlreadyOrderedPartitionInputWithoutSpill)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int bucket = 0; bucket < 4; ++bucket)
    {
        const int age = 20 + bucket;
        const int start_id = bucket * 128 + 1;
        const int end_id = start_id + 127;
        for (int id = start_id; id <= end_id; ++id)
        {
            ASSERT_TRUE(
                executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                           std::to_string(id) + ", 'wordered" +
                           std::to_string(id) + "', 'wordered" +
                           std::to_string(id) + "@example.com', " +
                           std::to_string(age) + ")")
                    .success());
        }
    }

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_window_reuse_partition_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT age, id, ROW_NUMBER() OVER (PARTITION BY age ORDER BY id) AS rn "
        "FROM users ORDER BY age, id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 512u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "20");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 2).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(127, 0).toString(), "20");
    EXPECT_EQ(result.resultSet()->getValue(127, 1).toString(), "128");
    EXPECT_EQ(result.resultSet()->getValue(127, 2).toString(), "128");
    EXPECT_EQ(result.resultSet()->getValue(128, 0).toString(), "21");
    EXPECT_EQ(result.resultSet()->getValue(128, 1).toString(), "129");
    EXPECT_EQ(result.resultSet()->getValue(128, 2).toString(), "1");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find("SELECT TRACE window mode=INPUT_ALREADY_ORDERED spill=0"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("input_rows=512"), std::string::npos) << trace;
    EXPECT_NE(trace.find("partitions=4"), std::string::npos) << trace;
    EXPECT_NE(trace.find("partition_state=MARKERS"), std::string::npos)
        << trace;
    EXPECT_NE(
        trace.find("SELECT TRACE window_projection mode=BATCH_FAST rows=512"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("SELECT TRACE output handoff=BATCH_ROWS rows=512"),
              std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       WindowUsesPartitionPrefixIncrementalSortWithoutSpill)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int bucket = 0; bucket < 4; ++bucket)
    {
        const int age = 20 + bucket;
        const int start_id = bucket * 128 + 1;
        const int end_id = start_id + 127;
        for (int id = end_id; id >= start_id; --id)
        {
            ASSERT_TRUE(
                executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                           std::to_string(id) + ", 'wincremental" +
                           std::to_string(id) + "', 'wincremental" +
                           std::to_string(id) + "@example.com', " +
                           std::to_string(age) + ")")
                    .success());
        }
    }

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_window_incremental_partition_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT age, id, ROW_NUMBER() OVER (PARTITION BY age ORDER BY id) AS rn "
        "FROM users ORDER BY age, id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 512u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "20");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 2).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(127, 0).toString(), "20");
    EXPECT_EQ(result.resultSet()->getValue(127, 1).toString(), "128");
    EXPECT_EQ(result.resultSet()->getValue(127, 2).toString(), "128");
    EXPECT_EQ(result.resultSet()->getValue(128, 0).toString(), "21");
    EXPECT_EQ(result.resultSet()->getValue(128, 1).toString(), "129");
    EXPECT_EQ(result.resultSet()->getValue(128, 2).toString(), "1");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE window mode=PARTITION_PREFIX_INCREMENTAL spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("input_rows=512"), std::string::npos) << trace;
    EXPECT_NE(trace.find("partitions=4"), std::string::npos) << trace;
    EXPECT_NE(trace.find("incremental_groups=4"), std::string::npos) << trace;
    EXPECT_NE(trace.find("partition_state=MARKERS"), std::string::npos)
        << trace;
    EXPECT_NE(
        trace.find("SELECT TRACE window_projection mode=BATCH_FAST rows=512"),
        std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       WindowUsesPartitionOrderPrefixIncrementalSortWithoutSpill)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "4KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    int next_id = 1;
    for (int user_id = 1; user_id <= 4; ++user_id)
    {
        for (int amount = 1; amount <= 4; ++amount)
        {
            std::vector<int> ids;
            for (int i = 0; i < 8; ++i)
            {
                ids.push_back(next_id++);
            }
            for (auto it = ids.rbegin(); it != ids.rend(); ++it)
            {
                ASSERT_TRUE(
                    executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(*it) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(amount) + ".0)")
                        .success());
            }
        }
    }

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_window_incremental_order_prefix_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT user_id, amount, id, "
        "ROW_NUMBER() OVER (PARTITION BY user_id ORDER BY amount, id) AS rn "
        "FROM orders "
        "ORDER BY user_id, amount, id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 128u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 2).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 3).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(7, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(7, 1).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(7, 2).toString(), "8");
    EXPECT_EQ(result.resultSet()->getValue(7, 3).toString(), "8");
    EXPECT_EQ(result.resultSet()->getValue(8, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(8, 1).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(8, 2).toString(), "9");
    EXPECT_EQ(result.resultSet()->getValue(8, 3).toString(), "9");
    EXPECT_EQ(result.resultSet()->getValue(31, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(31, 1).toString(), "4");
    EXPECT_EQ(result.resultSet()->getValue(31, 2).toString(), "32");
    EXPECT_EQ(result.resultSet()->getValue(31, 3).toString(), "32");
    EXPECT_EQ(result.resultSet()->getValue(32, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(32, 1).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(32, 2).toString(), "33");
    EXPECT_EQ(result.resultSet()->getValue(32, 3).toString(), "1");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find(
            "SELECT TRACE window mode=PARTITION_ORDER_PREFIX_INCREMENTAL spill=0"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("input_rows=128"), std::string::npos) << trace;
    EXPECT_NE(trace.find("partitions=4"), std::string::npos) << trace;
    EXPECT_NE(trace.find("incremental_groups=16"), std::string::npos) << trace;
    EXPECT_NE(trace.find("partition_state=MARKERS"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("prefix_order_keys=1"), std::string::npos) << trace;
    EXPECT_NE(
        trace.find("SELECT TRACE window_projection mode=BATCH_FAST rows=128"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("SELECT TRACE output handoff=BATCH_ROWS rows=128"),
              std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       UnorderedInputRequiresExplicitSortAndRecordsRejectionTrace)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        const int reversed = 129 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(reversed) + "', 'sort" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Sort");
    ASSERT_EQ(plan.root.children.size(), 1u);

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "EXISTING_ORDER" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());

    const auto sort_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_STRATEGY" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "FULL_SORT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(sort_it, plan.considered_paths.end());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 128u);
    EXPECT_TRUE(std::is_sorted(rows.begin(), rows.end()));
    EXPECT_EQ(rows.front(), "user1");
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedLimitPublishesOrderedLimitStrategyWithoutSortNode)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'limit" +
                               std::to_string(i) + "', 'limit" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT id FROM users WHERE id >= 200 ORDER BY id LIMIT 5";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Limit");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:top_n" &&
                                entry.candidate == "ORDERED_LIMIT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       ParallelSeqScanWrapsPlanInGatherAndPublishesRelationMetadata)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();

    for (int i = 1; i <= 2048; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'pseq" +
                               std::to_string(i) + "', 'pseq" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(18 + (i % 5)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM users WHERE age >= 18");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Gather");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "SeqScan");
    EXPECT_TRUE(plan.root.parallel_enabled);
    EXPECT_GT(plan.root.parallel_workers_planned, 1u);
    ASSERT_FALSE(plan.relations.empty());
    EXPECT_TRUE(plan.relations.front().parallel_enabled);
    EXPECT_EQ(plan.relations.front().parallel_stage, "SCAN");
    EXPECT_EQ(plan.relations.front().parallel_distribution_mode,
              "WORKER_PARTITIONED");
    EXPECT_EQ(plan.relations.front().exchange_topology_id,
              "SCAN_PARTITIONED->GATHER");
    EXPECT_FALSE(plan.relations.front().gather_decision_reason.empty());
    EXPECT_NE(std::find(plan.relations.front().candidate_scan_families.begin(),
                        plan.relations.front().candidate_scan_families.end(),
                        "PARALLEL_SEQ_SCAN"),
              plan.relations.front().candidate_scan_families.end());
    EXPECT_TRUE(std::any_of(
        plan.relations.front().candidate_family_identity_signatures.begin(),
        plan.relations.front().candidate_family_identity_signatures.end(),
        [](const std::string &entry) {
            return entry.find("PARALLEL_SEQ_SCAN") != std::string::npos;
        }));

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "PARALLEL" &&
                                entry.candidate == "PARALLEL_SEQ_SCAN" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_parallel_scan_live_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL("SELECT id FROM users WHERE age >= 18");
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_EQ(resultRowCount(result), 2048u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("runtime_access=PARALLEL_SEQ_SCAN"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("parallel_scan=1"), std::string::npos) << trace;
    EXPECT_NE(trace.find("parallel_exchange=GATHER"), std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       ParallelHashJoinWrapsPlanInGatherAndPublishesJoinMetadata)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();
    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "HASH_JOIN");

    for (int i = 1; i <= 4096; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'phj" +
                               std::to_string(i) + "', 'phj" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(i) +
                               ", 10.0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    auto bytecode = compileSQL(
        "SELECT users.id FROM users JOIN orders ON users.id = orders.user_id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Gather");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "HashJoin");
    EXPECT_GT(plan.root.memory_budget_bytes,
              plan.root.children.front().memory_budget_bytes);
    ASSERT_FALSE(plan.join_steps.empty());
    EXPECT_TRUE(plan.join_steps.back().parallel_enabled);
    EXPECT_EQ(plan.join_steps.back().parallel_stage, "HASH_JOIN");
    EXPECT_EQ(plan.join_steps.back().parallel_distribution_mode,
              "WORKER_PARTITIONED");
    EXPECT_EQ(plan.join_steps.back().exchange_topology_id,
              "HASH_JOIN_PARTITIONED->GATHER");
    EXPECT_FALSE(plan.join_steps.back().gather_decision_reason.empty());

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "PARALLEL" &&
                                entry.candidate == "PARALLEL_HASH_JOIN" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_parallel_hash_join_live_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(
        "SELECT users.id FROM users JOIN orders ON users.id = orders.user_id");
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_EQ(resultRowCount(result), 4096u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find("parallel_hash_join=1"), std::string::npos) << trace;
    EXPECT_NE(trace.find("parallel_exchange=REPARTITION_PROBE"),
              std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       ParallelAggregateWrapsPlanInGatherAndPublishesStageMetadata)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();

    for (int i = 1; i <= 8192; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'pagg" +
                               std::to_string(i) + "', 'pagg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 16)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT age, COUNT(*) FROM users GROUP BY age");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Gather");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "Aggregate");
    EXPECT_GT(plan.root.memory_budget_bytes,
              plan.root.children.front().memory_budget_bytes);
    EXPECT_TRUE(plan.root.children.front().parallel_aware);
    EXPECT_TRUE(plan.root.children.front().parallel_enabled);
    EXPECT_EQ(plan.root.children.front().parallel_stage, "AGGREGATE");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "PARALLEL" &&
                                entry.candidate == "PARALLEL_AGGREGATE" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_parallel_agg_live_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL("SELECT age, COUNT(*) FROM users GROUP BY age");
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_EQ(resultRowCount(result), 16u);

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(trace.find(
                  "SELECT TRACE aggregate kind=HASH_AGG group_key_mode=FAST_SCALAR spill=0"),
              std::string::npos)
        << trace;
    EXPECT_NE(trace.find("parallel_aggregate=1"), std::string::npos) << trace;
    EXPECT_NE(trace.find("parallel_exchange=GATHER"), std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       CountStarWithoutGroupExecutesSafelyWithParallelPlanningEnabled)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();

    for (int i = 1; i <= 8192; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'pcount" +
                               std::to_string(i) + "', 'pcount" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 16)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto result = executeSQL("SELECT COUNT(*) FROM users");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toInt64(), 8192);
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedParallelPlanUsesGatherMergeAndExplainJsonPublishesParallelFields)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();

    for (int i = 1; i <= 8192; ++i)
    {
        const int reversed = 2049 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'pgm" +
                               std::to_string(reversed) + "', 'pgm" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT name FROM users ORDER BY name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "GatherMerge");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "Sort");
    EXPECT_TRUE(plan.root.parallel_enabled);
    EXPECT_TRUE(plan.root.gather_merge);
    EXPECT_EQ(plan.root.parallel_stage, "GATHER_MERGE");
    EXPECT_GT(plan.root.memory_budget_bytes,
              plan.root.children.front().memory_budget_bytes);
    EXPECT_NE(plan.root.detail_text.find("leader=true"), std::string::npos);

    auto explain_result =
        executeSQL("EXPLAIN (FORMAT JSON) SELECT name FROM users ORDER BY name");
    ASSERT_TRUE(explain_result.success()) << explain_result.error();
    const auto explain_lines = resultStrings(explain_result);
    ASSERT_EQ(explain_lines.size(), 1u);
    const auto explain_json = nlohmann::json::parse(explain_lines.front());
    ASSERT_TRUE(explain_json.contains("plan_root"));
    const auto &plan_root = explain_json.at("plan_root");
    EXPECT_EQ(plan_root.value("node_type", std::string()), "GatherMerge");
    EXPECT_TRUE(plan_root.value("parallel_enabled", false));
    EXPECT_TRUE(plan_root.value("gather_merge", false));
    EXPECT_EQ(plan_root.value("parallel_stage", std::string()),
              "GATHER_MERGE");
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedParallelPlanExecutesLiveParallelSortRuntime)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();
    connection_ctx_->setSessionVariable("WORK_MEM", "512KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int i = 1; i <= 1024; ++i)
    {
        std::ostringstream name;
        name << "liveparallel" << std::setw(5) << std::setfill('0')
             << (1025 - i);
        ASSERT_TRUE(
            executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                       std::to_string(i) + ", '" + name.str() +
                       "', 'liveparallel" + std::to_string(i) +
                       "@example.com', 30)")
                .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT name FROM users ORDER BY name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "GatherMerge");
    ASSERT_TRUE(plan.root.parallel_enabled);
    ASSERT_GT(plan.root.parallel_workers_planned, 1u);

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_parallel_sort_live_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL("SELECT name FROM users ORDER BY name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1024u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(),
              "liveparallel00001");
    EXPECT_EQ(result.resultSet()
                  ->getValue(result.resultSet()->rowCount() - 1, 0)
                  .toString(),
              "liveparallel01024");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find("SELECT TRACE sort mode=PARALLEL_FULL_SORT spill=0"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("parallel_sort=1"), std::string::npos) << trace;
    EXPECT_NE(trace.find("parallel_exchange=GATHER_MERGE"),
              std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedParallelWindowPlanExecutesLiveParallelRowNumberRuntime)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();
    connection_ctx_->setSessionVariable("WORK_MEM", "1MB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "ALLOW");

    for (int bucket = 0; bucket < 4; ++bucket)
    {
        const int age = 20 + bucket;
        const int start_id = bucket * 4096 + 1;
        const int end_id = start_id + 4095;
        for (int id = start_id; id <= end_id; ++id)
        {
            ASSERT_TRUE(
                executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                           std::to_string(id) + ", 'pwindow" +
                           std::to_string(id) + "', 'pwindow" +
                           std::to_string(id) + "@example.com', " +
                           std::to_string(age) + ")")
                    .success());
        }
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT age, id, ROW_NUMBER() OVER (PARTITION BY age ORDER BY id) AS rn "
        "FROM users ORDER BY age, id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "GatherMerge");
    ASSERT_TRUE(plan.root.parallel_enabled);
    ASSERT_GT(plan.root.parallel_workers_planned, 1u);

    const auto trace_path =
        std::filesystem::temp_directory_path() /
        ("sb_select_trace_parallel_window_live_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         ".log");
    std::filesystem::remove(trace_path);
    ScopedEnvVar trace_enabled("SCRATCHBIRD_SELECT_TRACE", "1");
    ScopedEnvVar trace_file("SCRATCHBIRD_SELECT_TRACE_FILE",
                            trace_path.string());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 16384u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "20");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 2).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(4095, 2).toString(), "4096");
    EXPECT_EQ(result.resultSet()->getValue(4096, 0).toString(), "21");
    EXPECT_EQ(result.resultSet()->getValue(4096, 2).toString(), "1");

    const std::string trace = readTextFile(trace_path);
    EXPECT_NE(
        trace.find("SELECT TRACE window mode=INPUT_ALREADY_ORDERED spill=0"),
        std::string::npos)
        << trace;
    EXPECT_NE(trace.find("parallel_row_number=1"), std::string::npos)
        << trace;
    EXPECT_NE(trace.find("parallel_exchange=GATHER"), std::string::npos)
        << trace;
    EXPECT_NE(
        trace.find("SELECT TRACE window_projection mode=BATCH_FAST rows=16384"),
        std::string::npos)
        << trace;
}

TEST_F(QueryPlannerIntegrationTest, CorrelatedExistsPreservesQualifiedOuterReferenceInBytecode)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT u.id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id)");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    sblr_v3::Instruction select_inst;
    ASSERT_TRUE(decodeFirstSelect(bytecode, select_inst));

    sblr_v3::Instruction exists_inst;
    ASSERT_TRUE(findFirstOpcode(select_inst, sblr_v3::Opcode::SBLR3_SUBQUERY_EXISTS, exists_inst));

    const auto* exists_obj = std::get_if<sblr_v3::Value::Object>(&exists_inst.payload.data);
    ASSERT_NE(exists_obj, nullptr);
    auto query_it = exists_obj->find("query");
    ASSERT_NE(query_it, exists_obj->end());
    const auto* query_ptr = std::get_if<sblr_v3::Value::InstrPtr>(&query_it->second.data);
    ASSERT_NE(query_ptr, nullptr);
    ASSERT_TRUE(*query_ptr != nullptr);

    const auto refs = collectColumnRefs(**query_ptr);
    EXPECT_NE(std::find(refs.begin(), refs.end(), std::make_pair(std::string("orders"),
                                                                 std::string("user_id"))),
              refs.end());
    EXPECT_NE(std::find(refs.begin(), refs.end(), std::make_pair(std::string("u"),
                                                                 std::string("id"))),
              refs.end());
}

TEST_F(QueryPlannerIntegrationTest, RuntimePlanCapturesAggregateWindowSortAndLimitNodes)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'window" + std::to_string(i) +
                               "', 'window" + std::to_string(i) +
                               "@example.com', " + std::to_string(18 + (i % 10)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto aggregate_bytecode = compileSQL("SELECT COUNT(*) FROM users");
    ASSERT_FALSE(aggregate_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan aggregate_plan;
    ASSERT_TRUE(decodeRuntimePlan(aggregate_bytecode, aggregate_plan));
    EXPECT_EQ(aggregate_plan.root.node_type, "Aggregate");

    auto window_bytecode =
        compileSQL("SELECT ROW_NUMBER() OVER (ORDER BY id) FROM users");
    ASSERT_FALSE(window_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan window_plan;
    ASSERT_TRUE(decodeRuntimePlan(window_bytecode, window_plan));
    EXPECT_EQ(window_plan.root.node_type, "Window");

    auto ordered_bytecode =
        compileSQL("SELECT id FROM users ORDER BY id DESC LIMIT 5");
    ASSERT_FALSE(ordered_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan ordered_plan;
    ASSERT_TRUE(decodeRuntimePlan(ordered_bytecode, ordered_plan));
    EXPECT_EQ(ordered_plan.root.node_type, "Limit");
    ASSERT_EQ(ordered_plan.root.children.size(), 1u);
    EXPECT_EQ(ordered_plan.root.children.front().node_type, "Sort");
}

TEST_F(QueryPlannerIntegrationTest, BytecodeContainsVersionHeader)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM test");
    ASSERT_FALSE(bytecode.empty());
    sblr_v3::Container container;
    std::string err;
    ASSERT_TRUE(sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        << err;
    EXPECT_EQ(std::string(container.header.magic, 4), std::string("SBL3"));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithWhereClause)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id > 10");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_GT));
}

TEST_F(QueryPlannerIntegrationTest, NonSelectStatementsBypassPlanner)
{
    ASSERT_TRUE(createDatabase());

    auto insert_bytecode = compileSQL("INSERT INTO users (id) VALUES (1)");
    EXPECT_FALSE(insert_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(insert_bytecode, sblr_v3::Opcode::SBLR3_INSERT));

    auto create_bytecode = compileSQL("CREATE TABLE test2 (id INTEGER)");
    EXPECT_FALSE(create_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(create_bytecode, sblr_v3::Opcode::SBLR3_CREATE_TABLE));
}

// ===== Stress Tests =====

TEST_F(QueryPlannerIntegrationTest, ComplexQueryWithPlanner)
{
    ASSERT_TRUE(createDatabase());

    std::string complex_sql =
        "SELECT id, name, price * 1.1 "
        "FROM products "
        "WHERE price >= 100.0 AND name LIKE '%sale%'";

    auto bytecode = compileSQL(complex_sql);
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_AND));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_GE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_LIKE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_MULTIPLY));
}

TEST_F(QueryPlannerIntegrationTest, MultipleSelectStatements)
{
    ASSERT_TRUE(createDatabase());

    std::vector<std::string> queries = {
        "SELECT * FROM users",
        "SELECT id FROM users WHERE id > 10",
        "SELECT name, email FROM users",
        "SELECT COUNT(*) FROM users"
    };

    for (const auto &sql : queries)
    {
        auto bytecode = compileSQL(sql);
        EXPECT_FALSE(bytecode.empty()) << "Failed for query: " << sql << "\n"
                                       << last_compile_errors_;
        EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT))
            << "Missing SELECT opcode for query: " << sql;
    }
}

// ===== Edge Cases =====

TEST_F(QueryPlannerIntegrationTest, EmptySelectList)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM test");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_TABLE_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_COLUMN_REF));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithoutWhereClause)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_FALSE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
}

TEST_F(QueryPlannerIntegrationTest,
       SharedLoweringSiblingFamiliesRemainDistinctInRuntimePlanIdentities)
{
    ASSERT_TRUE(createDatabase());
    auto create_btree =
        executeSQL("CREATE INDEX idx_users_name_btree ON users (name)");
    ASSERT_TRUE(create_btree.success()) << create_btree.error();
    auto create_art =
        executeSQL("CREATE INDEX idx_users_name_art ON users USING ART (name)");
    ASSERT_TRUE(create_art.success()) << create_art.error();

    for (int i = 1; i <= 1024; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'alias" +
                               std::to_string(i) + "', 'alias" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id FROM users WHERE name = 'alias512'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();

    EXPECT_EQ(relation.scan_family, "BTREE_EQ_SCAN");
    EXPECT_TRUE(relation.physical_family == "BTREE" ||
                relation.physical_family == "ART");
    EXPECT_NE(relation.physical_family, relation.scan_family);

    EXPECT_GE(relation.candidate_family_identity_signatures.size(), 2u);
    EXPECT_TRUE(std::any_of(
        relation.candidate_family_identity_signatures.begin(),
        relation.candidate_family_identity_signatures.end(),
        [](const std::string &entry) {
            return entry.find(":BTREE:BTREE_EQ_SCAN:") != std::string::npos;
        }))
        << joinStrings(relation.candidate_family_identity_signatures, " || ");
    EXPECT_TRUE(std::any_of(
        relation.candidate_family_identity_signatures.begin(),
        relation.candidate_family_identity_signatures.end(),
        [](const std::string &entry) {
            return entry.find(":ART:BTREE_EQ_SCAN:") != std::string::npos;
        }))
        << joinStrings(relation.candidate_family_identity_signatures, " || ");
    EXPECT_TRUE(std::any_of(
        relation.candidate_family_statistics_signatures.begin(),
        relation.candidate_family_statistics_signatures.end(),
        [](const std::string &entry) {
            return entry.find(":BTREE:BTREE_EQ_SCAN:") != std::string::npos;
        }))
        << joinStrings(relation.candidate_family_statistics_signatures, " || ");
    EXPECT_TRUE(std::any_of(
        relation.candidate_family_statistics_signatures.begin(),
        relation.candidate_family_statistics_signatures.end(),
        [](const std::string &entry) {
            return entry.find(":ART:BTREE_EQ_SCAN:") != std::string::npos;
        }))
        << joinStrings(relation.candidate_family_statistics_signatures, " || ");
}

// ===== Diagnostic Test =====

TEST_F(QueryPlannerIntegrationTest, DiagnosticDisassemblyOutput)
{
    ASSERT_TRUE(createDatabase());

    std::string sql = "SELECT id, name FROM users WHERE id = 42";
    auto bytecode = compileSQL(sql);

    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
}
