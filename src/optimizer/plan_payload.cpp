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
            out["natural"] = join_step.natural;
            out["using_columns"] = join_step.using_columns;
            out["condition_text"] = join_step.condition_text;
            out["has_hash_keys"] = join_step.has_hash_keys;
            out["startup_cost"] = join_step.startup_cost;
            out["total_cost"] = join_step.total_cost;
            out["estimated_rows"] = join_step.estimated_rows;

            nlohmann::json left_hash;
            left_hash["qualifier"] = join_step.left_hash_key.qualifier;
            left_hash["column_name"] = join_step.left_hash_key.column_name;
            out["left_hash_key"] = std::move(left_hash);

            nlohmann::json right_hash;
            right_hash["qualifier"] = join_step.right_hash_key.qualifier;
            right_hash["column_name"] = join_step.right_hash_key.column_name;
            out["right_hash_key"] = std::move(right_hash);
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
            join_step_out.natural = json_in.value("natural", false);
            join_step_out.condition_text = json_in.value("condition_text", std::string());
            join_step_out.has_hash_keys = json_in.value("has_hash_keys", false);
            join_step_out.startup_cost = json_in.value("startup_cost", 0.0);
            join_step_out.total_cost = json_in.value("total_cost", 0.0);
            join_step_out.estimated_rows = json_in.value("estimated_rows", 0ULL);

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

            return true;
        }
        catch (const std::exception &ex)
        {
            error_out = ex.what();
            return false;
        }
    }

} // namespace scratchbird::optimizer
