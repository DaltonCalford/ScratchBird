#include "scratchbird/optimizer/plan_payload.h"

#include <nlohmann/json.hpp>

namespace scratchbird::optimizer
{
    namespace
    {
        auto relationToJson(const RuntimePlanRelation &relation) -> nlohmann::json
        {
            nlohmann::json out;
            out["source_relation_index"] = relation.source_relation_index;
            out["table_path"] = relation.table_path;
            out["physical_table_path"] = relation.physical_table_path;
            out["alias"] = relation.alias;
            out["table_id_text"] = relation.table_id_text;
            out["scan_kind"] = relation.scan_kind;
            out["index_name"] = relation.index_name;
            out["index_id_text"] = relation.index_id_text;
            out["bitmap_op"] = relation.bitmap_op;
            out["covering_index"] = relation.covering_index;
            out["exact_key_lookup"] = relation.exact_key_lookup;
            out["flattened_derived"] = relation.flattened_derived;
            out["lateral"] = relation.lateral;
            out["parameterized"] = relation.parameterized;
            out["partition_pruned"] = relation.partition_pruned;
            out["partition_strategy"] = relation.partition_strategy;
            out["partition_key_column"] = relation.partition_key_column;
            out["partition_targets"] = relation.partition_targets;
            out["runtime_filter_enabled"] = relation.runtime_filter_enabled;
            out["runtime_filter_column"] = relation.runtime_filter_column;
            out["runtime_filter_index_name"] = relation.runtime_filter_index_name;
            out["runtime_filter_index_id_text"] = relation.runtime_filter_index_id_text;
            out["base_rows"] = relation.base_rows;
            out["selectivity"] = relation.selectivity;
            out["startup_cost"] = relation.startup_cost;
            out["total_cost"] = relation.total_cost;
            out["estimated_rows"] = relation.estimated_rows;

            nlohmann::json predicate;
            predicate["valid"] = relation.index_predicate.valid;
            predicate["index_name"] = relation.index_predicate.index_name;
            predicate["index_id_text"] = relation.index_predicate.index_id_text;
            predicate["column_name"] = relation.index_predicate.column_name;
            predicate["operator_name"] = relation.index_predicate.operator_name;
            predicate["literal_kind"] = relation.index_predicate.literal_kind;
            predicate["literal_text"] = relation.index_predicate.literal_text;
            out["index_predicate"] = std::move(predicate);

            out["index_predicates"] = nlohmann::json::array();
            for (const auto &entry : relation.index_predicates)
            {
                nlohmann::json pred;
                pred["valid"] = entry.valid;
                pred["index_name"] = entry.index_name;
                pred["index_id_text"] = entry.index_id_text;
                pred["column_name"] = entry.column_name;
                pred["operator_name"] = entry.operator_name;
                pred["literal_kind"] = entry.literal_kind;
                pred["literal_text"] = entry.literal_text;
                out["index_predicates"].push_back(std::move(pred));
            }
            return out;
        }

        auto relationFromJson(const nlohmann::json &json_in,
                              RuntimePlanRelation &relation_out,
                              std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan relation must be an object";
                return false;
            }

            relation_out.source_relation_index = json_in.value("source_relation_index", 0U);
            relation_out.table_path = json_in.value("table_path", std::string());
            relation_out.physical_table_path =
                json_in.value("physical_table_path", std::string());
            relation_out.alias = json_in.value("alias", std::string());
            relation_out.table_id_text = json_in.value("table_id_text", std::string());
            relation_out.scan_kind = json_in.value("scan_kind", std::string());
            relation_out.index_name = json_in.value("index_name", std::string());
            relation_out.index_id_text = json_in.value("index_id_text", std::string());
            relation_out.bitmap_op = json_in.value("bitmap_op", std::string());
            relation_out.covering_index = json_in.value("covering_index", false);
            relation_out.exact_key_lookup = json_in.value("exact_key_lookup", false);
            relation_out.flattened_derived = json_in.value("flattened_derived", false);
            relation_out.lateral = json_in.value("lateral", false);
            relation_out.parameterized = json_in.value("parameterized", false);
            relation_out.partition_pruned = json_in.value("partition_pruned", false);
            relation_out.partition_strategy =
                json_in.value("partition_strategy", std::string());
            relation_out.partition_key_column =
                json_in.value("partition_key_column", std::string());
            relation_out.partition_targets.clear();
            const auto partition_targets_it = json_in.find("partition_targets");
            if (partition_targets_it != json_in.end() && partition_targets_it->is_array())
            {
                for (const auto &entry : *partition_targets_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.partition_targets.push_back(entry.get<std::string>());
                    }
                }
            }
            relation_out.runtime_filter_enabled =
                json_in.value("runtime_filter_enabled", false);
            relation_out.runtime_filter_column =
                json_in.value("runtime_filter_column", std::string());
            relation_out.runtime_filter_index_name =
                json_in.value("runtime_filter_index_name", std::string());
            relation_out.runtime_filter_index_id_text =
                json_in.value("runtime_filter_index_id_text", std::string());
            relation_out.base_rows = json_in.value("base_rows", 0ULL);
            relation_out.selectivity = json_in.value("selectivity", 1.0);
            relation_out.startup_cost = json_in.value("startup_cost", 0.0);
            relation_out.total_cost = json_in.value("total_cost", 0.0);
            relation_out.estimated_rows = json_in.value("estimated_rows", 0ULL);

            const auto predicate_it = json_in.find("index_predicate");
            if (predicate_it != json_in.end() && predicate_it->is_object())
            {
                relation_out.index_predicate.valid = predicate_it->value("valid", false);
                relation_out.index_predicate.index_name =
                    predicate_it->value("index_name", std::string());
                relation_out.index_predicate.index_id_text =
                    predicate_it->value("index_id_text", std::string());
                relation_out.index_predicate.column_name =
                    predicate_it->value("column_name", std::string());
                relation_out.index_predicate.operator_name =
                    predicate_it->value("operator_name", std::string());
                relation_out.index_predicate.literal_kind =
                    predicate_it->value("literal_kind", std::string());
                relation_out.index_predicate.literal_text =
                    predicate_it->value("literal_text", std::string());
            }

            relation_out.index_predicates.clear();
            const auto preds_it = json_in.find("index_predicates");
            if (preds_it != json_in.end() && preds_it->is_array())
            {
                for (const auto &entry : *preds_it)
                {
                    if (!entry.is_object())
                    {
                        continue;
                    }
                    RuntimePlanIndexPredicate pred;
                    pred.valid = entry.value("valid", false);
                    pred.index_name = entry.value("index_name", std::string());
                    pred.index_id_text = entry.value("index_id_text", std::string());
                    pred.column_name = entry.value("column_name", std::string());
                    pred.operator_name = entry.value("operator_name", std::string());
                    pred.literal_kind = entry.value("literal_kind", std::string());
                    pred.literal_text = entry.value("literal_text", std::string());
                    relation_out.index_predicates.push_back(std::move(pred));
                }
            }
            return true;
        }

        auto joinStepToJson(const RuntimePlanJoinStep &join_step) -> nlohmann::json
        {
            nlohmann::json out;
            out["source_join_index"] = join_step.source_join_index;
            out["right_relation_index"] = join_step.right_relation_index;
            out["join_type"] = join_step.join_type;
            out["method"] = join_step.method;
            out["disconnected_component"] = join_step.disconnected_component;
            out["legality_class"] = join_step.legality_class;
            out["reorderable"] = join_step.reorderable;
            out["natural"] = join_step.natural;
            out["using_columns"] = join_step.using_columns;
            out["condition_text"] = join_step.condition_text;
            out["preserves_left_rows"] = join_step.preserves_left_rows;
            out["preserves_right_rows"] = join_step.preserves_right_rows;
            out["null_introduces_left"] = join_step.null_introduces_left;
            out["null_introduces_right"] = join_step.null_introduces_right;
            out["requires_original_order"] = join_step.requires_original_order;
            out["has_hash_keys"] = join_step.has_hash_keys;
            out["has_merge_keys"] = join_step.has_merge_keys;
            out["merge_outer_presorted"] = join_step.merge_outer_presorted;
            out["merge_inner_presorted"] = join_step.merge_inner_presorted;
            out["selectivity"] = join_step.selectivity;
            out["startup_cost"] = join_step.startup_cost;
            out["total_cost"] = join_step.total_cost;
            out["estimated_rows"] = join_step.estimated_rows;
            out["estimated_memory_bytes"] = join_step.estimated_memory_bytes;
            out["memory_budget_bytes"] = join_step.memory_budget_bytes;
            out["spill_expected"] = join_step.spill_expected;
            out["spill_passes"] = join_step.spill_passes;
            out["spill_bytes"] = join_step.spill_bytes;
            out["spill_policy"] = join_step.spill_policy;

            nlohmann::json left_hash;
            left_hash["qualifier"] = join_step.left_hash_key.qualifier;
            left_hash["column_name"] = join_step.left_hash_key.column_name;
            out["left_hash_key"] = std::move(left_hash);

            nlohmann::json right_hash;
            right_hash["qualifier"] = join_step.right_hash_key.qualifier;
            right_hash["column_name"] = join_step.right_hash_key.column_name;
            out["right_hash_key"] = std::move(right_hash);

            nlohmann::json left_merge;
            left_merge["qualifier"] = join_step.left_merge_key.qualifier;
            left_merge["column_name"] = join_step.left_merge_key.column_name;
            out["left_merge_key"] = std::move(left_merge);

            nlohmann::json right_merge;
            right_merge["qualifier"] = join_step.right_merge_key.qualifier;
            right_merge["column_name"] = join_step.right_merge_key.column_name;
            out["right_merge_key"] = std::move(right_merge);
            return out;
        }

        auto joinStepFromJson(const nlohmann::json &json_in,
                              RuntimePlanJoinStep &join_step_out,
                              std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan join step must be an object";
                return false;
            }

            join_step_out.source_join_index = json_in.value("source_join_index", 0U);
            join_step_out.right_relation_index = json_in.value("right_relation_index", 0U);
            join_step_out.join_type = json_in.value("join_type", std::string());
            join_step_out.method = json_in.value("method", std::string());
            join_step_out.disconnected_component =
                json_in.value("disconnected_component", false);
            join_step_out.legality_class =
                json_in.value("legality_class", std::string());
            join_step_out.reorderable = json_in.value("reorderable", true);
            join_step_out.natural = json_in.value("natural", false);
            join_step_out.condition_text = json_in.value("condition_text", std::string());
            join_step_out.preserves_left_rows =
                json_in.value("preserves_left_rows", false);
            join_step_out.preserves_right_rows =
                json_in.value("preserves_right_rows", false);
            join_step_out.null_introduces_left =
                json_in.value("null_introduces_left", false);
            join_step_out.null_introduces_right =
                json_in.value("null_introduces_right", false);
            join_step_out.requires_original_order =
                json_in.value("requires_original_order", false);
            join_step_out.has_hash_keys = json_in.value("has_hash_keys", false);
            join_step_out.has_merge_keys = json_in.value("has_merge_keys", false);
            join_step_out.merge_outer_presorted =
                json_in.value("merge_outer_presorted", false);
            join_step_out.merge_inner_presorted =
                json_in.value("merge_inner_presorted", false);
            join_step_out.selectivity = json_in.value("selectivity", 1.0);
            join_step_out.startup_cost = json_in.value("startup_cost", 0.0);
            join_step_out.total_cost = json_in.value("total_cost", 0.0);
            join_step_out.estimated_rows = json_in.value("estimated_rows", 0ULL);
            join_step_out.estimated_memory_bytes =
                json_in.value("estimated_memory_bytes", 0ULL);
            join_step_out.memory_budget_bytes =
                json_in.value("memory_budget_bytes", 0ULL);
            join_step_out.spill_expected = json_in.value("spill_expected", false);
            join_step_out.spill_passes = json_in.value("spill_passes", 0U);
            join_step_out.spill_bytes = json_in.value("spill_bytes", 0ULL);
            join_step_out.spill_policy =
                json_in.value("spill_policy", std::string());

            const auto using_it = json_in.find("using_columns");
            if (using_it != json_in.end() && using_it->is_array())
            {
                join_step_out.using_columns.clear();
                for (const auto &entry : *using_it)
                {
                    if (entry.is_string())
                    {
                        join_step_out.using_columns.push_back(entry.get<std::string>());
                    }
                }
            }

            const auto left_hash_it = json_in.find("left_hash_key");
            if (left_hash_it != json_in.end() && left_hash_it->is_object())
            {
                join_step_out.left_hash_key.qualifier =
                    left_hash_it->value("qualifier", std::string());
                join_step_out.left_hash_key.column_name =
                    left_hash_it->value("column_name", std::string());
            }

            const auto right_hash_it = json_in.find("right_hash_key");
            if (right_hash_it != json_in.end() && right_hash_it->is_object())
            {
                join_step_out.right_hash_key.qualifier =
                    right_hash_it->value("qualifier", std::string());
                join_step_out.right_hash_key.column_name =
                    right_hash_it->value("column_name", std::string());
            }

            const auto left_merge_it = json_in.find("left_merge_key");
            if (left_merge_it != json_in.end() && left_merge_it->is_object())
            {
                join_step_out.left_merge_key.qualifier =
                    left_merge_it->value("qualifier", std::string());
                join_step_out.left_merge_key.column_name =
                    left_merge_it->value("column_name", std::string());
            }

            const auto right_merge_it = json_in.find("right_merge_key");
            if (right_merge_it != json_in.end() && right_merge_it->is_object())
            {
                join_step_out.right_merge_key.qualifier =
                    right_merge_it->value("qualifier", std::string());
                join_step_out.right_merge_key.column_name =
                    right_merge_it->value("column_name", std::string());
            }
            return true;
        }

        auto nodeToJson(const RuntimePlanNode &node) -> nlohmann::json
        {
            nlohmann::json out;
            out["node_type"] = node.node_type;
            out["relation_alias"] = node.relation_alias;
            out["table_path"] = node.table_path;
            out["join_type"] = node.join_type;
            out["condition_text"] = node.condition_text;
            out["index_name"] = node.index_name;
            out["detail_text"] = node.detail_text;
            out["startup_cost"] = node.startup_cost;
            out["total_cost"] = node.total_cost;
            out["estimated_rows"] = node.estimated_rows;
            out["estimated_memory_bytes"] = node.estimated_memory_bytes;
            out["memory_budget_bytes"] = node.memory_budget_bytes;
            out["spill_expected"] = node.spill_expected;
            out["spill_passes"] = node.spill_passes;
            out["spill_bytes"] = node.spill_bytes;
            out["spill_policy"] = node.spill_policy;
            out["children"] = nlohmann::json::array();
            for (const auto &child : node.children)
            {
                out["children"].push_back(nodeToJson(child));
            }
            return out;
        }

        auto nodeFromJson(const nlohmann::json &json_in,
                          RuntimePlanNode &node_out,
                          std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan node must be an object";
                return false;
            }

            node_out.node_type = json_in.value("node_type", std::string());
            node_out.relation_alias = json_in.value("relation_alias", std::string());
            node_out.table_path = json_in.value("table_path", std::string());
            node_out.join_type = json_in.value("join_type", std::string());
            node_out.condition_text = json_in.value("condition_text", std::string());
            node_out.index_name = json_in.value("index_name", std::string());
            node_out.detail_text = json_in.value("detail_text", std::string());
            node_out.startup_cost = json_in.value("startup_cost", 0.0);
            node_out.total_cost = json_in.value("total_cost", 0.0);
            node_out.estimated_rows = json_in.value("estimated_rows", 0ULL);
            node_out.estimated_memory_bytes =
                json_in.value("estimated_memory_bytes", 0ULL);
            node_out.memory_budget_bytes =
                json_in.value("memory_budget_bytes", 0ULL);
            node_out.spill_expected = json_in.value("spill_expected", false);
            node_out.spill_passes = json_in.value("spill_passes", 0U);
            node_out.spill_bytes = json_in.value("spill_bytes", 0ULL);
            node_out.spill_policy = json_in.value("spill_policy", std::string());

            const auto children_it = json_in.find("children");
            node_out.children.clear();
            if (children_it != json_in.end() && children_it->is_array())
            {
                for (const auto &child : *children_it)
                {
                    RuntimePlanNode child_node;
                    if (!nodeFromJson(child, child_node, error_out))
                    {
                        return false;
                    }
                    node_out.children.push_back(std::move(child_node));
                }
            }
            return true;
        }

        auto traceEntryToJson(const RuntimePlanTraceEntry &entry) -> nlohmann::json
        {
            nlohmann::json out;
            out["phase"] = entry.phase;
            out["subject"] = entry.subject;
            out["candidate"] = entry.candidate;
            out["verdict"] = entry.verdict;
            out["reason"] = entry.reason;
            out["startup_cost"] = entry.startup_cost;
            out["total_cost"] = entry.total_cost;
            out["estimated_rows"] = entry.estimated_rows;
            return out;
        }

        auto traceEntryFromJson(const nlohmann::json &json_in,
                                RuntimePlanTraceEntry &entry_out,
                                std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan trace entry must be an object";
                return false;
            }

            entry_out.phase = json_in.value("phase", std::string());
            entry_out.subject = json_in.value("subject", std::string());
            entry_out.candidate = json_in.value("candidate", std::string());
            entry_out.verdict = json_in.value("verdict", std::string());
            entry_out.reason = json_in.value("reason", std::string());
            entry_out.startup_cost = json_in.value("startup_cost", 0.0);
            entry_out.total_cost = json_in.value("total_cost", 0.0);
            entry_out.estimated_rows = json_in.value("estimated_rows", 0ULL);
            return true;
        }

        auto statsProvenanceToJson(const RuntimePlanStatisticsProvenance &entry)
            -> nlohmann::json
        {
            nlohmann::json out;
            out["subject"] = entry.subject;
            out["source"] = entry.source;
            out["detail"] = entry.detail;
            return out;
        }

        auto statsProvenanceFromJson(const nlohmann::json &json_in,
                                     RuntimePlanStatisticsProvenance &entry_out,
                                     std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan statistics provenance entry must be an object";
                return false;
            }

            entry_out.subject = json_in.value("subject", std::string());
            entry_out.source = json_in.value("source", std::string());
            entry_out.detail = json_in.value("detail", std::string());
            return true;
        }

        auto adaptiveFeedbackToJson(const RuntimePlanAdaptiveFeedback &feedback)
            -> nlohmann::json
        {
            nlohmann::json out;
            out["available"] = feedback.available;
            out["replan_required"] = feedback.replan_required;
            out["stats_refresh_requested"] = feedback.stats_refresh_requested;
            out["stats_refresh_applied"] = feedback.stats_refresh_applied;
            out["observation_count"] = feedback.observation_count;
            out["replan_action_count"] = feedback.replan_action_count;
            out["last_estimated_rows"] = feedback.last_estimated_rows;
            out["last_actual_rows"] = feedback.last_actual_rows;
            out["estimation_error_ratio"] = feedback.estimation_error_ratio;
            out["correction_factor"] = feedback.correction_factor;
            out["last_plan_hash"] = feedback.last_plan_hash;
            return out;
        }

        auto adaptiveFeedbackFromJson(const nlohmann::json &json_in,
                                      RuntimePlanAdaptiveFeedback &feedback_out,
                                      std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan adaptive feedback entry must be an object";
                return false;
            }

            feedback_out.available = json_in.value("available", false);
            feedback_out.replan_required = json_in.value("replan_required", false);
            feedback_out.stats_refresh_requested =
                json_in.value("stats_refresh_requested", false);
            feedback_out.stats_refresh_applied =
                json_in.value("stats_refresh_applied", false);
            feedback_out.observation_count =
                json_in.value("observation_count", 0ULL);
            feedback_out.replan_action_count =
                json_in.value("replan_action_count", 0ULL);
            feedback_out.last_estimated_rows =
                json_in.value("last_estimated_rows", 0ULL);
            feedback_out.last_actual_rows =
                json_in.value("last_actual_rows", 0ULL);
            feedback_out.estimation_error_ratio =
                json_in.value("estimation_error_ratio", 1.0);
            feedback_out.correction_factor =
                json_in.value("correction_factor", 1.0);
            feedback_out.last_plan_hash =
                json_in.value("last_plan_hash", std::string());
            return true;
        }

        auto controlEntryToJson(const RuntimePlanControlEntry &entry)
            -> nlohmann::json
        {
            nlohmann::json out;
            out["name"] = entry.name;
            out["value"] = entry.value;
            out["source"] = entry.source;
            out["enforced"] = entry.enforced;
            return out;
        }

        auto controlEntryFromJson(const nlohmann::json &json_in,
                                  RuntimePlanControlEntry &entry_out,
                                  std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan control entry must be an object";
                return false;
            }

            entry_out.name = json_in.value("name", std::string());
            entry_out.value = json_in.value("value", std::string());
            entry_out.source = json_in.value("source", std::string());
            entry_out.enforced = json_in.value("enforced", true);
            return true;
        }
    } // namespace

    auto encodeRuntimePlan(const RuntimePlan &plan,
                           std::vector<uint8_t> &bytes_out,
                           std::string &error_out) -> bool
    {
        try
        {
            nlohmann::json root;
            root["version"] = plan.version;
            root["plan_hash"] = plan.plan_hash;
            root["explain_text"] = plan.explain_text;
            root["cache_mode"] = plan.cache_mode;
            root["plan_profile_signature"] = plan.plan_profile_signature;
            root["selectivity_bucket_signature"] =
                plan.selectivity_bucket_signature;
            root["query_feedback_key"] = plan.query_feedback_key;
            root["parameter_sensitive"] = plan.parameter_sensitive;
            root["root"] = nodeToJson(plan.root);
            root["relations"] = nlohmann::json::array();
            for (const auto &relation : plan.relations)
            {
                root["relations"].push_back(relationToJson(relation));
            }
            root["join_steps"] = nlohmann::json::array();
            for (const auto &join_step : plan.join_steps)
            {
                root["join_steps"].push_back(joinStepToJson(join_step));
            }
            root["considered_paths"] = nlohmann::json::array();
            for (const auto &entry : plan.considered_paths)
            {
                root["considered_paths"].push_back(traceEntryToJson(entry));
            }
            root["rejected_paths"] = nlohmann::json::array();
            for (const auto &entry : plan.rejected_paths)
            {
                root["rejected_paths"].push_back(traceEntryToJson(entry));
            }
            root["statistics_provenance"] = nlohmann::json::array();
            for (const auto &entry : plan.statistics_provenance)
            {
                root["statistics_provenance"].push_back(
                    statsProvenanceToJson(entry));
            }
            root["adaptive_feedback"] =
                adaptiveFeedbackToJson(plan.adaptive_feedback);
            root["optimizer_controls"] = nlohmann::json::array();
            for (const auto &entry : plan.optimizer_controls)
            {
                root["optimizer_controls"].push_back(controlEntryToJson(entry));
            }

            const std::string dumped = root.dump();
            bytes_out.assign(dumped.begin(), dumped.end());
            return true;
        }
        catch (const std::exception &ex)
        {
            error_out = ex.what();
            return false;
        }
    }

    auto decodeRuntimePlan(const std::vector<uint8_t> &bytes,
                           RuntimePlan &plan_out,
                           std::string &error_out) -> bool
    {
        try
        {
            const std::string text(bytes.begin(), bytes.end());
            const nlohmann::json root = nlohmann::json::parse(text);
            if (!root.is_object())
            {
                error_out = "runtime plan payload must be an object";
                return false;
            }

            plan_out.version = root.value("version", 1U);
            plan_out.plan_hash = root.value("plan_hash", std::string());
            plan_out.explain_text = root.value("explain_text", std::string());
            plan_out.cache_mode = root.value("cache_mode", std::string());
            plan_out.plan_profile_signature =
                root.value("plan_profile_signature", std::string());
            plan_out.selectivity_bucket_signature =
                root.value("selectivity_bucket_signature", std::string());
            plan_out.query_feedback_key =
                root.value("query_feedback_key", std::string());
            plan_out.parameter_sensitive =
                root.value("parameter_sensitive", false);

            const auto root_node_it = root.find("root");
            if (root_node_it != root.end())
            {
                if (!nodeFromJson(*root_node_it, plan_out.root, error_out))
                {
                    return false;
                }
            }

            plan_out.relations.clear();
            const auto relations_it = root.find("relations");
            if (relations_it != root.end() && relations_it->is_array())
            {
                for (const auto &entry : *relations_it)
                {
                    RuntimePlanRelation relation;
                    if (!relationFromJson(entry, relation, error_out))
                    {
                        return false;
                    }
                    plan_out.relations.push_back(std::move(relation));
                }
            }

            plan_out.join_steps.clear();
            const auto join_steps_it = root.find("join_steps");
            if (join_steps_it != root.end() && join_steps_it->is_array())
            {
                for (const auto &entry : *join_steps_it)
                {
                    RuntimePlanJoinStep join_step;
                    if (!joinStepFromJson(entry, join_step, error_out))
                    {
                        return false;
                    }
                    plan_out.join_steps.push_back(std::move(join_step));
                }
            }

            plan_out.considered_paths.clear();
            const auto considered_it = root.find("considered_paths");
            if (considered_it != root.end() && considered_it->is_array())
            {
                for (const auto &entry : *considered_it)
                {
                    RuntimePlanTraceEntry trace_entry;
                    if (!traceEntryFromJson(entry, trace_entry, error_out))
                    {
                        return false;
                    }
                    plan_out.considered_paths.push_back(std::move(trace_entry));
                }
            }

            plan_out.rejected_paths.clear();
            const auto rejected_it = root.find("rejected_paths");
            if (rejected_it != root.end() && rejected_it->is_array())
            {
                for (const auto &entry : *rejected_it)
                {
                    RuntimePlanTraceEntry trace_entry;
                    if (!traceEntryFromJson(entry, trace_entry, error_out))
                    {
                        return false;
                    }
                    plan_out.rejected_paths.push_back(std::move(trace_entry));
                }
            }

            plan_out.statistics_provenance.clear();
            const auto provenance_it = root.find("statistics_provenance");
            if (provenance_it != root.end() && provenance_it->is_array())
            {
                for (const auto &entry : *provenance_it)
                {
                    RuntimePlanStatisticsProvenance provenance_entry;
                    if (!statsProvenanceFromJson(entry,
                                                 provenance_entry,
                                                 error_out))
                    {
                        return false;
                    }
                    plan_out.statistics_provenance.push_back(
                        std::move(provenance_entry));
                }
            }

            const auto adaptive_feedback_it = root.find("adaptive_feedback");
            if (adaptive_feedback_it != root.end())
            {
                if (!adaptiveFeedbackFromJson(*adaptive_feedback_it,
                                              plan_out.adaptive_feedback,
                                              error_out))
                {
                    return false;
                }
            }

            plan_out.optimizer_controls.clear();
            const auto controls_it = root.find("optimizer_controls");
            if (controls_it != root.end() && controls_it->is_array())
            {
                for (const auto &entry : *controls_it)
                {
                    RuntimePlanControlEntry control_entry;
                    if (!controlEntryFromJson(entry, control_entry, error_out))
                    {
                        return false;
                    }
                    plan_out.optimizer_controls.push_back(std::move(control_entry));
                }
            }

            return true;
        }
        catch (const std::exception &ex)
        {
            error_out = ex.what();
            return false;
        }
    }

} // namespace scratchbird::optimizer
