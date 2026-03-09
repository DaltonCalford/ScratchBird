/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * QueryPlanner - Cost-based Query Planner Implementation
 *
 * V3 implementation:
 * - Resolves SELECT statements against catalog metadata
 * - Chooses base access paths using statistics and cost model estimates
 * - Builds left-deep join plans with deterministic join-method selection
 * - Preserves outer/natural join order and fails closed to nested loops there
 */

#include "scratchbird/optimizer/query_planner.h"

#include "scratchbird/core/debug.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>

namespace scratchbird::optimizer
{
    namespace
    {
        struct BaseAccessChoice
        {
            size_t relation_index = 0;
            std::shared_ptr<Path> path;
            std::shared_ptr<PlanNode> plan;
            RuntimePlanRelation runtime_relation;
            double selectivity = 1.0;
        };

        struct JoinDecision
        {
            bool valid = false;
            size_t join_index = 0;
            size_t candidate_relation_index = 0;
            std::shared_ptr<Path> path;
            std::shared_ptr<PlanNode> plan;
            RuntimePlanJoinStep runtime_join;
            double selectivity = 1.0;
            double total_cost = std::numeric_limits<double>::max();
            uint64_t rows = 0;
        };

        auto planJoinTypeToString(parser::JoinType join_type) -> std::string
        {
            switch (join_type)
            {
                case parser::JoinType::INNER: return "INNER";
                case parser::JoinType::LEFT: return "LEFT";
                case parser::JoinType::RIGHT: return "RIGHT";
                case parser::JoinType::FULL: return "FULL";
                case parser::JoinType::CROSS: return "CROSS";
                case parser::JoinType::NATURAL: return "NATURAL";
                case parser::JoinType::NATURAL_LEFT: return "NATURAL_LEFT";
                case parser::JoinType::NATURAL_RIGHT: return "NATURAL_RIGHT";
                case parser::JoinType::NATURAL_FULL: return "NATURAL_FULL";
            }
            return "INNER";
        }

        auto displayRelationName(const ResolvedRelation &relation) -> std::string
        {
            if (!relation.alias.empty() &&
                !core::IdentifierUtils::namesMatch(relation.alias,
                                                   false,
                                                   relation.table_info.table_name,
                                                   false))
            {
                if (!relation.table_info.table_name.empty())
                {
                    return relation.alias + " (" + relation.table_info.table_name + ")";
                }
                if (!relation.table_path.empty())
                {
                    return relation.alias + " (" + relation.table_path + ")";
                }
                return relation.alias;
            }
            if (!relation.alias.empty())
            {
                return relation.alias;
            }
            if (!relation.table_info.table_name.empty())
            {
                return relation.table_info.table_name;
            }
            return relation.table_path.empty() ? "<derived>" : relation.table_path;
        }

        auto relationLookupName(const ResolvedRelation &relation) -> std::string
        {
            if (!relation.alias.empty())
            {
                return relation.alias;
            }
            if (!relation.table_info.table_name.empty())
            {
                return relation.table_info.table_name;
            }
            return relation.table_path;
        }

        auto stablePlanHash(const std::string &text) -> std::string
        {
            uint64_t hash = 1469598103934665603ULL;
            for (unsigned char ch : text)
            {
                hash ^= static_cast<uint64_t>(ch);
                hash *= 1099511628211ULL;
            }
            std::ostringstream out;
            out << std::hex << hash;
            return out.str();
        }

        auto isZeroId(const core::ID &id) -> bool
        {
            return id == core::ID{};
        }

        auto relationOutputRows(uint64_t base_rows, double selectivity) -> uint64_t
        {
            if (base_rows == 0)
            {
                return 0;
            }
            uint64_t rows = static_cast<uint64_t>(
                static_cast<double>(base_rows) * std::max(0.0, std::min(1.0, selectivity)));
            if (rows == 0)
            {
                rows = 1;
            }
            return rows;
        }

        auto toRuntimePlanNode(const std::shared_ptr<PlanNode> &plan) -> RuntimePlanNode
        {
            RuntimePlanNode node;
            if (!plan)
            {
                return node;
            }

            node.startup_cost = plan->startupCost();
            node.total_cost = plan->totalCost();
            node.estimated_rows = plan->rows();

            switch (plan->type())
            {
                case PlanNodeType::SEQ_SCAN: {
                    auto seq = std::dynamic_pointer_cast<SeqScanNode>(plan);
                    node.node_type = "SeqScan";
                    if (seq)
                    {
                        node.table_path = seq->tableName();
                    }
                    break;
                }
                case PlanNodeType::INDEX_SCAN: {
                    auto idx = std::dynamic_pointer_cast<IndexScanNode>(plan);
                    node.node_type = "IndexScan";
                    if (idx)
                    {
                        node.table_path = idx->tableName();
                        node.index_name = idx->indexName();
                        node.condition_text = idx->indexCond();
                    }
                    break;
                }
                case PlanNodeType::NESTED_LOOP_JOIN: {
                    auto join = std::dynamic_pointer_cast<NestedLoopJoinNode>(plan);
                    node.node_type = "NestedLoopJoin";
                    if (join)
                    {
                        node.join_type = planJoinTypeToString(join->joinType());
                        node.condition_text = join->joinCondString();
                        node.children.push_back(toRuntimePlanNode(join->outerPlan()));
                        node.children.push_back(toRuntimePlanNode(join->innerPlan()));
                    }
                    break;
                }
                case PlanNodeType::HASH_JOIN: {
                    auto join = std::dynamic_pointer_cast<HashJoinNode>(plan);
                    node.node_type = "HashJoin";
                    if (join)
                    {
                        node.join_type = planJoinTypeToString(join->joinType());
                        node.condition_text = join->joinCondString();
                        node.children.push_back(toRuntimePlanNode(join->outerPlan()));
                        node.children.push_back(toRuntimePlanNode(join->innerPlan()));
                    }
                    break;
                }
                default:
                    node.node_type = "PlanNode";
                    for (const auto &child : plan->children())
                    {
                        node.children.push_back(toRuntimePlanNode(child));
                    }
                    break;
            }

            return node;
        }

        auto isJoinReorderSafe(const ResolvedSelectQuery &resolved) -> bool
        {
            if (!resolved.all_joins_inner)
            {
                return false;
            }
            for (const auto &join : resolved.joins)
            {
                if (join.natural || !join.using_columns.empty())
                {
                    return false;
                }
            }
            return true;
        }

        auto makeScanPredicateLiteralBytes(const ResolvedScanPredicate &predicate,
                                           std::vector<uint8_t> &bytes_out) -> bool
        {
            if (predicate.literal_kind == "STRING")
            {
                bytes_out.assign(predicate.literal_text.begin(), predicate.literal_text.end());
                return true;
            }
            if (predicate.literal_kind == "INTEGER")
            {
                try
                {
                    int64_t value = std::stoll(predicate.literal_text);
                    bytes_out.resize(sizeof(value));
                    std::memcpy(bytes_out.data(), &value, sizeof(value));
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            if (predicate.literal_kind == "FLOAT")
            {
                try
                {
                    double value = std::stod(predicate.literal_text);
                    bytes_out.resize(sizeof(value));
                    std::memcpy(bytes_out.data(), &value, sizeof(value));
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            if (predicate.literal_kind == "BOOLEAN")
            {
                bytes_out = {static_cast<uint8_t>(predicate.literal_text == "TRUE" ? 1 : 0)};
                return true;
            }
            if (predicate.literal_kind == "NULL")
            {
                bytes_out.clear();
                return true;
            }
            return false;
        }
    } // namespace

    auto QueryPlanner::planQuery(const parser::v3::SelectStmt *select_stmt,
                                 core::ErrorContext *ctx,
                                 core::ConnectionContext *conn_ctx)
        -> std::shared_ptr<PlanNode>
    {
        conn_ctx_ = conn_ctx;
        if (select_stmt == nullptr)
        {
            return nullptr;
        }

        DEBUG_LOG_DB("QueryPlanner::planQuery requires string pool context; "
                     "use buildSelectPlan for V3 execution planning");
        return nullptr;
    }

    auto QueryPlanner::planAnalyze(const parser::v3::AnalyzeStmt *analyze_stmt,
                                   core::ErrorContext *ctx)
        -> std::shared_ptr<PlanNode>
    {
        (void)analyze_stmt;
        (void)ctx;
        return nullptr;
    }

    auto QueryPlanner::buildSelectPlan(const parser::v3::SelectStmt *select_stmt,
                                       const parser::v3::StringPool &pool,
                                       PlannedSelectQuery &planned_out,
                                       core::ErrorContext *ctx,
                                       core::ConnectionContext *conn_ctx,
                                       const core::ID &current_schema_id)
        -> core::Status
    {
        planned_out = PlannedSelectQuery{};
        conn_ctx_ = conn_ctx;
        if (select_stmt == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }

        V3SemanticAnalyzer analyzer(db_, stats_manager_);
        core::Status status = analyzer.resolveSelect(select_stmt,
                                                    pool,
                                                    planned_out.resolved_query,
                                                    conn_ctx,
                                                    current_schema_id,
                                                    ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        if (planned_out.resolved_query.relations.empty())
        {
            RuntimePlan plan;
            plan.version = 1;
            plan.explain_text = "Result";
            plan.plan_hash = stablePlanHash(plan.explain_text);
            plan.root.node_type = "Result";
            planned_out.runtime_plan = std::move(plan);
            return core::Status::OK;
        }

        std::vector<BaseAccessChoice> access_choices(planned_out.resolved_query.relations.size());
        for (size_t relation_index = 0;
             relation_index < planned_out.resolved_query.relations.size();
             ++relation_index)
        {
            const auto &relation = planned_out.resolved_query.relations[relation_index];
            BaseAccessChoice choice;
            choice.relation_index = relation_index;

            const std::string relation_name = displayRelationName(relation);
            double qual_cost = 0.0;
            double selectivity = 1.0;

            if (relation.local_predicate.has_value())
            {
                const auto &predicate = *relation.local_predicate;
                qual_cost = cost_model_.operatorCost(predicate.operator_name);
                if (relation.resolved &&
                    !isZeroId(relation.table_info.table_id) &&
                    predicate.expression != nullptr)
                {
                    selectivity = selectivity_estimator_.estimateWhereClause(
                        predicate.expression,
                        relation.table_info.table_id,
                        &pool,
                        ctx);
                }
                if (selectivity <= 0.0)
                {
                    selectivity = 0.01;
                }
            }

            const uint64_t seq_rows =
                relationOutputRows(relation.estimated_rows == 0 ? 1000 : relation.estimated_rows,
                                   selectivity);
            CostEstimate best_cost =
                cost_model_.costSeqScan(std::max<uint64_t>(1, relation.estimated_pages),
                                        seq_rows,
                                        qual_cost,
                                        ctx);
            std::shared_ptr<Path> best_path;
            auto seq_path = std::make_shared<SeqScanPath>(relation.table_info.table_id,
                                                          relation_name,
                                                          std::max<uint64_t>(1, relation.estimated_pages),
                                                          seq_rows,
                                                          qual_cost,
                                                          best_cost);
            if (relation.local_predicate.has_value())
            {
                seq_path->setWhereExpr(relation.local_predicate->expression);
            }
            best_path = seq_path;

            std::shared_ptr<PlanNode> best_plan;
            auto seq_plan = std::make_shared<SeqScanNode>(relation.table_info.table_id,
                                                          relation_name);
            seq_plan->setQualCost(qual_cost);
            if (relation.local_predicate.has_value())
            {
                seq_plan->setFilter(relation.local_predicate->predicate_text);
            }
            seq_plan->setCost(best_cost.startup_cost, best_cost.total_cost, best_cost.rows);
            best_plan = seq_plan;

            std::string best_scan_kind = "SEQ_SCAN";
            core::CatalogManager::IndexInfo matched_index{};
            bool use_index = false;
            ResolvedScanPredicate matched_predicate{};

            if (relation.local_predicate.has_value() &&
                relation.local_predicate->has_index_match)
            {
                matched_predicate = *relation.local_predicate;
                matched_index = matched_predicate.matched_index;

                const uint64_t expected_rows = std::max<uint64_t>(1, seq_rows);
                const uint64_t index_pages =
                    std::max<uint64_t>(1, relation.estimated_pages / 8);
                const uint64_t heap_pages = std::max<uint64_t>(
                    1,
                    static_cast<uint64_t>(static_cast<double>(
                                               std::max<uint64_t>(1, relation.estimated_pages)) *
                                           selectivity));
                const double correlation =
                    matched_predicate.kind == ResolvedPredicateKind::EQUALITY ? 0.9 :
                    matched_predicate.kind == ResolvedPredicateKind::LIKE_PREFIX ? 0.6 :
                    0.35;

                CostEstimate index_cost{};
                std::string scan_kind = "INDEX_SCAN";
                if (matched_index.index_type == core::CatalogManager::IndexType::LSM)
                {
                    index_cost = cost_model_.costLSMScan(3,
                                                         2,
                                                         expected_rows,
                                                         heap_pages,
                                                         expected_rows,
                                                         qual_cost,
                                                         correlation,
                                                         ctx);
                    scan_kind = "LSM_SCAN";
                }
                else
                {
                    index_cost = cost_model_.costIndexScan(3,
                                                           index_pages,
                                                           expected_rows,
                                                           heap_pages,
                                                           expected_rows,
                                                           qual_cost,
                                                           correlation,
                                                           ctx);
                }

                if (index_cost.total_cost <= best_cost.total_cost)
                {
                    use_index = true;
                    best_cost = index_cost;
                    best_scan_kind = scan_kind;
                    best_path = std::make_shared<IndexScanPath>(relation.table_info.table_id,
                                                                relation_name,
                                                                matched_index.index_id,
                                                                matched_index.index_name,
                                                                3,
                                                                index_pages,
                                                                expected_rows,
                                                                heap_pages,
                                                                expected_rows,
                                                                qual_cost,
                                                                correlation,
                                                                index_cost);

                    auto index_plan = std::make_shared<IndexScanNode>(relation.table_info.table_id,
                                                                      relation_name,
                                                                      matched_index.index_id,
                                                                      matched_index.index_name);
                    index_plan->setIndexQualCost(qual_cost);
                    index_plan->setHeapQualCost(qual_cost);
                    index_plan->setCorrelation(correlation);
                    index_plan->setIndexCond(matched_predicate.predicate_text);
                    index_plan->setCost(index_cost.startup_cost,
                                        index_cost.total_cost,
                                        index_cost.rows);
                    best_plan = index_plan;
                }
            }

            choice.path = best_path;
            choice.plan = best_plan;
            choice.selectivity = selectivity;
            choice.runtime_relation.source_relation_index = relation_index;
            choice.runtime_relation.table_path = relation.table_path;
            choice.runtime_relation.alias = relation.alias;
            choice.runtime_relation.table_id_text =
                relation.resolved ? relation.table_info.table_id.toString() : std::string();
            choice.runtime_relation.scan_kind = best_scan_kind;
            choice.runtime_relation.startup_cost = best_cost.startup_cost;
            choice.runtime_relation.total_cost = best_cost.total_cost;
            choice.runtime_relation.estimated_rows = best_cost.rows;
            if (use_index)
            {
                choice.runtime_relation.index_name = matched_index.index_name;
                choice.runtime_relation.index_id_text = matched_index.index_id.toString();
                choice.runtime_relation.index_predicate.valid = true;
                choice.runtime_relation.index_predicate.column_name = matched_predicate.column_name;
                choice.runtime_relation.index_predicate.operator_name =
                    matched_predicate.operator_name;
                choice.runtime_relation.index_predicate.literal_kind =
                    matched_predicate.literal_kind;
                choice.runtime_relation.index_predicate.literal_text =
                    matched_predicate.literal_text;
            }
            access_choices[relation_index] = std::move(choice);
        }

        auto estimateJoinSelectivityFor =
            [&](const ResolvedJoin &join) -> double {
                if (!join.equi_join ||
                    !join.has_hash_column_ids ||
                    join.left_relation_index >= planned_out.resolved_query.relations.size() ||
                    join.right_relation_index >= planned_out.resolved_query.relations.size())
                {
                    const auto &left_relation =
                        planned_out.resolved_query.relations[join.left_relation_index];
                    const auto &right_relation =
                        planned_out.resolved_query.relations[join.right_relation_index];
                    return selectivity_estimator_.estimateJoinSelectivity(
                        join.condition,
                        left_relation.table_info.table_id,
                        right_relation.table_info.table_id,
                        &pool,
                        ctx);
                }

                const auto &left_relation =
                    planned_out.resolved_query.relations[join.left_relation_index];
                const auto &right_relation =
                    planned_out.resolved_query.relations[join.right_relation_index];
                return selectivity_estimator_.estimateEquiJoinSelectivity(
                    join.left_hash_column_id,
                    join.right_hash_column_id,
                    left_relation.table_info.table_id,
                    right_relation.table_info.table_id,
                    ctx);
            };

        auto buildJoinDecision =
            [&](const std::shared_ptr<Path> &current_path,
                const std::shared_ptr<PlanNode> &current_plan,
                uint64_t current_rows,
                size_t candidate_relation_index,
                const ResolvedJoin &join,
                bool candidate_is_right_relation) -> JoinDecision {
                JoinDecision decision;
                decision.valid = true;
                decision.join_index = join.source_join_index;
                decision.candidate_relation_index = candidate_relation_index;
                decision.selectivity = estimateJoinSelectivityFor(join);
                if (decision.selectivity <= 0.0)
                {
                    decision.selectivity = 0.01;
                }

                const auto &candidate_access = access_choices[candidate_relation_index];
                const auto &candidate_relation =
                    planned_out.resolved_query.relations[candidate_relation_index];
                const auto join_type = join.join_type;
                CostEstimate nl_cost = cost_model_.costNestedLoopJoin(
                    current_path ? current_path->cost() : CostEstimate{},
                    candidate_access.path ? candidate_access.path->cost() : CostEstimate{},
                    current_rows,
                    candidate_access.path ? candidate_access.path->rows() : 0,
                    decision.selectivity,
                    join_type,
                    ctx);

                bool allow_hash =
                    (join_type == parser::JoinType::INNER || join_type == parser::JoinType::LEFT) &&
                    join.equi_join;
                CostEstimate hash_cost{};
                if (allow_hash)
                {
                    hash_cost = cost_model_.costHashJoin(
                        current_path ? current_path->cost() : CostEstimate{},
                        candidate_access.path ? candidate_access.path->cost() : CostEstimate{},
                        current_rows,
                        candidate_access.path ? candidate_access.path->rows() : 0,
                        decision.selectivity,
                        join_type,
                        ctx);
                }
                else
                {
                    hash_cost.total_cost = std::numeric_limits<double>::max();
                }

                bool use_hash = allow_hash && hash_cost.total_cost < nl_cost.total_cost;
                decision.total_cost = use_hash ? hash_cost.total_cost : nl_cost.total_cost;
                decision.rows = use_hash ? hash_cost.rows : nl_cost.rows;
                decision.runtime_join.source_join_index = join.source_join_index;
                decision.runtime_join.right_relation_index = candidate_relation_index;
                decision.runtime_join.join_type = planJoinTypeToString(join_type);
                decision.runtime_join.natural = join.natural;
                decision.runtime_join.using_columns = join.using_columns;
                decision.runtime_join.condition_text = join.condition_text;
                decision.runtime_join.startup_cost =
                    use_hash ? hash_cost.startup_cost : nl_cost.startup_cost;
                decision.runtime_join.total_cost =
                    use_hash ? hash_cost.total_cost : nl_cost.total_cost;
                decision.runtime_join.estimated_rows =
                    use_hash ? hash_cost.rows : nl_cost.rows;
                decision.runtime_join.method = use_hash ? "HASH_JOIN" : "NESTED_LOOP";

                if (join.equi_join)
                {
                    decision.runtime_join.has_hash_keys = true;
                    if (candidate_is_right_relation)
                    {
                        decision.runtime_join.left_hash_key.qualifier =
                            join.left_hash_qualifier;
                        decision.runtime_join.left_hash_key.column_name =
                            join.left_hash_column;
                        decision.runtime_join.right_hash_key.qualifier =
                            join.right_hash_qualifier;
                        decision.runtime_join.right_hash_key.column_name =
                            join.right_hash_column;
                    }
                    else
                    {
                        decision.runtime_join.left_hash_key.qualifier =
                            join.right_hash_qualifier;
                        decision.runtime_join.left_hash_key.column_name =
                            join.right_hash_column;
                        decision.runtime_join.right_hash_key.qualifier =
                            join.left_hash_qualifier;
                        decision.runtime_join.right_hash_key.column_name =
                            join.left_hash_column;
                    }
                }

                if (use_hash)
                {
                    std::vector<parser::v3::Expression *> hash_keys_outer;
                    std::vector<parser::v3::Expression *> hash_keys_inner;
                    auto hash_plan = std::make_shared<HashJoinNode>(join_type,
                                                                   current_plan,
                                                                   candidate_access.plan,
                                                                   const_cast<parser::v3::Expression *>(
                                                                       join.condition),
                                                                   hash_keys_outer,
                                                                   hash_keys_inner);
                    hash_plan->setJoinCondString(join.condition_text);
                    hash_plan->setCost(hash_cost.startup_cost,
                                       hash_cost.total_cost,
                                       hash_cost.rows);
                    decision.plan = hash_plan;
                    decision.path = std::make_shared<HashJoinPath>(
                        join_type,
                        current_path,
                        candidate_access.path,
                        const_cast<parser::v3::Expression *>(join.condition),
                        hash_keys_outer,
                        hash_keys_inner,
                        decision.selectivity,
                        hash_cost);
                }
                else
                {
                    auto nested_plan = std::make_shared<NestedLoopJoinNode>(
                        join_type,
                        current_plan,
                        candidate_access.plan,
                        const_cast<parser::v3::Expression *>(join.condition));
                    nested_plan->setJoinCondString(join.condition_text);
                    nested_plan->setCost(nl_cost.startup_cost,
                                         nl_cost.total_cost,
                                         nl_cost.rows);
                    decision.plan = nested_plan;
                    decision.path = std::make_shared<NestedLoopJoinPath>(
                        join_type,
                        current_path,
                        candidate_access.path,
                        const_cast<parser::v3::Expression *>(join.condition),
                        decision.selectivity,
                        nl_cost);
                }

                if (!decision.runtime_join.has_hash_keys)
                {
                    decision.runtime_join.left_hash_key.qualifier = relationLookupName(
                        planned_out.resolved_query.relations[join.left_relation_index]);
                    decision.runtime_join.right_hash_key.qualifier = relationLookupName(
                        candidate_relation);
                }

                return decision;
            };

        std::vector<size_t> relation_order;
        std::vector<RuntimePlanRelation> runtime_relations;
        std::vector<RuntimePlanJoinStep> runtime_joins;
        std::shared_ptr<Path> current_path;
        std::shared_ptr<PlanNode> current_plan;
        uint64_t current_rows = 0;

        if (planned_out.resolved_query.joins.empty())
        {
            relation_order.push_back(0);
            runtime_relations.push_back(access_choices[0].runtime_relation);
            current_path = access_choices[0].path;
            current_plan = access_choices[0].plan;
            current_rows = current_path ? current_path->rows() : 0;
        }
        else if (isJoinReorderSafe(planned_out.resolved_query))
        {
            planned_out.reordered_relations = true;
            std::vector<bool> joined(planned_out.resolved_query.relations.size(), false);
            size_t start_relation = 0;
            double best_start_cost = std::numeric_limits<double>::max();
            for (size_t relation_index = 0;
                 relation_index < access_choices.size();
                 ++relation_index)
            {
                if (!access_choices[relation_index].path)
                {
                    continue;
                }
                if (access_choices[relation_index].path->totalCost() < best_start_cost)
                {
                    best_start_cost = access_choices[relation_index].path->totalCost();
                    start_relation = relation_index;
                }
            }

            joined[start_relation] = true;
            relation_order.push_back(start_relation);
            runtime_relations.push_back(access_choices[start_relation].runtime_relation);
            current_path = access_choices[start_relation].path;
            current_plan = access_choices[start_relation].plan;
            current_rows = current_path ? current_path->rows() : 0;

            while (relation_order.size() < planned_out.resolved_query.relations.size())
            {
                JoinDecision best_join;
                for (const auto &join : planned_out.resolved_query.joins)
                {
                    const bool left_joined = joined[join.left_relation_index];
                    const bool right_joined = joined[join.right_relation_index];
                    if (left_joined == right_joined)
                    {
                        continue;
                    }

                    const bool candidate_is_right = !right_joined;
                    const size_t candidate_relation_index =
                        candidate_is_right ? join.right_relation_index : join.left_relation_index;
                    auto decision = buildJoinDecision(current_path,
                                                      current_plan,
                                                      current_rows,
                                                      candidate_relation_index,
                                                      join,
                                                      candidate_is_right);
                    if (!best_join.valid || decision.total_cost < best_join.total_cost)
                    {
                        best_join = std::move(decision);
                    }
                }

                if (!best_join.valid)
                {
                    size_t candidate = 0;
                    for (; candidate < joined.size(); ++candidate)
                    {
                        if (!joined[candidate])
                        {
                            break;
                        }
                    }
                    if (candidate >= joined.size())
                    {
                        break;
                    }

                    ResolvedJoin cross_join;
                    cross_join.source_join_index = runtime_joins.size();
                    cross_join.left_relation_index = relation_order.back();
                    cross_join.right_relation_index = candidate;
                    cross_join.join_type = parser::JoinType::CROSS;
                    cross_join.condition = nullptr;
                    cross_join.condition_text.clear();
                    auto decision = buildJoinDecision(current_path,
                                                      current_plan,
                                                      current_rows,
                                                      candidate,
                                                      cross_join,
                                                      true);
                    best_join = std::move(decision);
                }

                joined[best_join.candidate_relation_index] = true;
                relation_order.push_back(best_join.candidate_relation_index);
                runtime_relations.push_back(
                    access_choices[best_join.candidate_relation_index].runtime_relation);
                runtime_joins.push_back(best_join.runtime_join);
                current_path = best_join.path;
                current_plan = best_join.plan;
                current_rows = best_join.rows;
            }
        }
        else
        {
            relation_order.push_back(0);
            runtime_relations.push_back(access_choices[0].runtime_relation);
            current_path = access_choices[0].path;
            current_plan = access_choices[0].plan;
            current_rows = current_path ? current_path->rows() : 0;

            for (const auto &join : planned_out.resolved_query.joins)
            {
                const size_t candidate_relation_index = join.right_relation_index;
                auto decision = buildJoinDecision(current_path,
                                                  current_plan,
                                                  current_rows,
                                                  candidate_relation_index,
                                                  join,
                                                  true);
                if (!decision.valid)
                {
                    return core::Status::INTERNAL_ERROR;
                }
                if (join.join_type == parser::JoinType::RIGHT ||
                    join.join_type == parser::JoinType::FULL ||
                    join.natural ||
                    !join.using_columns.empty())
                {
                    decision.runtime_join.method = "NESTED_LOOP";
                    decision.runtime_join.has_hash_keys = false;
                }
                relation_order.push_back(candidate_relation_index);
                runtime_relations.push_back(access_choices[candidate_relation_index].runtime_relation);
                runtime_joins.push_back(decision.runtime_join);
                current_path = decision.path;
                current_plan = decision.plan;
                current_rows = decision.rows;
            }
        }

        planned_out.root_plan = current_plan;
        planned_out.runtime_plan.version = 1;
        planned_out.runtime_plan.relations = std::move(runtime_relations);
        planned_out.runtime_plan.join_steps = std::move(runtime_joins);
        planned_out.runtime_plan.root = toRuntimePlanNode(current_plan);
        planned_out.runtime_plan.explain_text =
            current_plan ? current_plan->toString() : std::string("Result");

        std::ostringstream hash_seed;
        hash_seed << planned_out.runtime_plan.explain_text << '|';
        for (const auto &relation : planned_out.runtime_plan.relations)
        {
            hash_seed << relation.source_relation_index << ':'
                      << relation.scan_kind << ':'
                      << relation.total_cost << '|';
        }
        for (const auto &join : planned_out.runtime_plan.join_steps)
        {
            hash_seed << join.source_join_index << ':'
                      << join.method << ':'
                      << join.total_cost << '|';
        }
        planned_out.runtime_plan.plan_hash = stablePlanHash(hash_seed.str());
        return core::Status::OK;
    }

} // namespace scratchbird::optimizer
