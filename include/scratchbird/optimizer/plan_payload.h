#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::optimizer
{

    struct RuntimePlanIndexPredicate
    {
        bool valid = false;
        std::string index_name;
        std::string index_id_text;
        std::string column_name;
        std::string operator_name;
        std::string literal_kind;
        std::string literal_text;
    };

    struct RuntimePlanRelation
    {
        size_t source_relation_index = 0;
        std::string table_path;
        std::string physical_table_path;
        std::string alias;
        std::string table_id_text;
        std::string scan_kind;
        std::string index_name;
        std::string index_id_text;
        RuntimePlanIndexPredicate index_predicate;
        std::vector<RuntimePlanIndexPredicate> index_predicates;
        std::string bitmap_op;
        bool covering_index = false;
        bool exact_key_lookup = false;
        bool flattened_derived = false;
        double startup_cost = 0.0;
        double total_cost = 0.0;
        uint64_t estimated_rows = 0;
    };

    struct RuntimePlanHashKey
    {
        std::string qualifier;
        std::string column_name;
    };

    struct RuntimePlanJoinStep
    {
        size_t source_join_index = 0;
        size_t right_relation_index = 0;
        std::string join_type;
        std::string method;
        bool natural = false;
        std::vector<std::string> using_columns;
        std::string condition_text;
        bool has_hash_keys = false;
        RuntimePlanHashKey left_hash_key;
        RuntimePlanHashKey right_hash_key;
        double startup_cost = 0.0;
        double total_cost = 0.0;
        uint64_t estimated_rows = 0;
    };

    struct RuntimePlanNode
    {
        std::string node_type;
        std::string relation_alias;
        std::string table_path;
        std::string join_type;
        std::string condition_text;
        std::string index_name;
        std::string detail_text;
        double startup_cost = 0.0;
        double total_cost = 0.0;
        uint64_t estimated_rows = 0;
        std::vector<RuntimePlanNode> children;
    };

    struct RuntimePlan
    {
        uint32_t version = 1;
        std::string plan_hash;
        std::string explain_text;
        RuntimePlanNode root;
        std::vector<RuntimePlanRelation> relations;
        std::vector<RuntimePlanJoinStep> join_steps;
    };

    auto encodeRuntimePlan(const RuntimePlan &plan,
                           std::vector<uint8_t> &bytes_out,
                           std::string &error_out) -> bool;

    auto decodeRuntimePlan(const std::vector<uint8_t> &bytes,
                           RuntimePlan &plan_out,
                           std::string &error_out) -> bool;

} // namespace scratchbird::optimizer
