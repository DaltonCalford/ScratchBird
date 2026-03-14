#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::optimizer
{

    inline constexpr uint32_t kRuntimePlanPayloadVersion = 3;
    inline constexpr const char *kRuntimePlanContractId = "sb_runtime_plan/v3";
    inline constexpr const char *kJoinGraphContractId = "sb_join_graph/v1";
    inline constexpr const char *kOptimizerDiagnosticsContractId =
        "sb_optimizer_diagnostics/v1";

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
        std::string scan_family;
        std::vector<std::string> scan_family_tags;
        std::vector<std::string> candidate_scan_families;
        std::string index_name;
        std::string index_id_text;
        RuntimePlanIndexPredicate index_predicate;
        std::vector<RuntimePlanIndexPredicate> index_predicates;
        std::string bitmap_op;
        bool covering_index = false;
        bool exact_key_lookup = false;
        bool flattened_derived = false;
        bool lateral = false;
        bool parameterized = false;
        bool ordered_output = false;
        uint64_t ordered_prefix_length = 0;
        std::vector<size_t> required_outer_relation_indexes;
        std::vector<std::string> required_outer_relation_aliases;
        bool partition_pruned = false;
        std::string partition_strategy;
        std::string partition_key_column;
        std::vector<std::string> partition_key_columns;
        std::vector<std::string> partition_targets;
        std::vector<std::string> partition_targets_pruned_at_plan;
        bool runtime_partition_pruning_eligible = false;
        std::vector<std::string> runtime_partition_pruning_sources;
        std::vector<RuntimePlanIndexPredicate> partition_predicates;
        bool runtime_filter_enabled = false;
        std::string runtime_filter_column;
        std::string runtime_filter_index_name;
        std::string runtime_filter_index_id_text;
        uint64_t base_rows = 0;
        double selectivity = 1.0;
        double startup_cost = 0.0;
        double total_cost = 0.0;
        uint64_t estimated_rows = 0;
    };

    struct RuntimePlanHashKey
    {
        std::string qualifier;
        std::string column_name;
    };

    struct RuntimePlanMergeKey
    {
        std::string qualifier;
        std::string column_name;
    };

    struct RuntimePlanJoinKeyPair
    {
        std::string left_qualifier;
        std::string left_column_name;
        std::string right_qualifier;
        std::string right_column_name;
    };

    struct RuntimePlanSearchSummary
    {
        std::string requested_strategy;
        std::string selected_strategy;
        uint64_t search_budget = 0;
        uint64_t considered_state_count = 0;
        uint64_t pruned_state_count = 0;
        uint64_t pair_evaluation_count = 0;
        uint64_t rejected_candidate_count = 0;
        uint64_t max_pair_evaluations = 0;
        uint64_t max_states_considered = 0;
        uint64_t exhaustive_join_limit = 0;
        uint64_t bounded_dp_join_limit = 0;
        uint64_t fallback_prune_level = 0;
        std::string fallback_reason;
        std::string fallback_threshold_name;
        uint64_t fallback_threshold_value = 0;
    };

    struct RuntimePlanJoinStep
    {
        size_t source_join_index = 0;
        size_t right_relation_index = 0;
        size_t join_edge_left_relation_index = 0;
        size_t join_edge_right_relation_index = 0;
        std::string join_edge_left_alias;
        std::string join_edge_right_alias;
        std::string join_edge_left_id_text;
        std::string join_edge_right_id_text;
        std::string join_type;
        std::string method;
        bool disconnected_component = false;
        std::string legality_class;
        std::vector<std::string> legal_method_families;
        std::vector<std::string> method_enablers;
        bool reorderable = true;
        bool natural = false;
        std::vector<std::string> using_columns;
        std::string condition_text;
        std::vector<RuntimePlanJoinKeyPair> equijoin_keys;
        std::vector<std::string> residual_predicates;
        bool preserves_left_rows = false;
        bool preserves_right_rows = false;
        bool null_introduces_left = false;
        bool null_introduces_right = false;
        bool requires_original_order = false;
        bool outer_reorder_barrier = false;
        bool semi_reorder_barrier = false;
        bool anti_reorder_barrier = false;
        bool using_reorder_barrier = false;
        bool natural_reorder_barrier = false;
        bool lateral_reorder_barrier = false;
        bool parameterized_dependency = false;
        std::vector<size_t> parameter_dependency_relation_indexes;
        std::vector<std::string> parameter_dependency_relation_aliases;
        bool has_hash_keys = false;
        RuntimePlanHashKey left_hash_key;
        RuntimePlanHashKey right_hash_key;
        bool has_merge_keys = false;
        RuntimePlanMergeKey left_merge_key;
        RuntimePlanMergeKey right_merge_key;
        bool merge_outer_presorted = false;
        bool merge_inner_presorted = false;
        double selectivity = 1.0;
        double startup_cost = 0.0;
        double total_cost = 0.0;
        uint64_t estimated_rows = 0;
        uint64_t estimated_memory_bytes = 0;
        uint64_t memory_budget_bytes = 0;
        bool spill_expected = false;
        uint32_t spill_passes = 0;
        uint64_t spill_bytes = 0;
        std::string spill_policy;
    };

    struct RuntimePlanTraceEntry
    {
        std::string phase;
        std::string subject;
        std::string candidate;
        std::string verdict;
        std::string reason;
        double startup_cost = 0.0;
        double total_cost = 0.0;
        uint64_t estimated_rows = 0;
    };

    struct RuntimePlanStatisticsProvenance
    {
        std::string subject;
        std::string source;
        std::string detail;
    };

    struct RuntimePlanAdaptiveFeedback
    {
        bool available = false;
        bool replan_required = false;
        bool stats_refresh_requested = false;
        bool stats_refresh_applied = false;
        uint64_t observation_count = 0;
        uint64_t replan_action_count = 0;
        uint64_t last_estimated_rows = 0;
        uint64_t last_actual_rows = 0;
        double estimation_error_ratio = 1.0;
        double correction_factor = 1.0;
        std::string last_plan_hash;
    };

    struct RuntimePlanControlEntry
    {
        std::string name;
        std::string value;
        std::string source;
        bool enforced = true;
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
        uint64_t estimated_memory_bytes = 0;
        uint64_t memory_budget_bytes = 0;
        bool spill_expected = false;
        uint32_t spill_passes = 0;
        uint64_t spill_bytes = 0;
        std::string spill_policy;
        std::vector<RuntimePlanNode> children;
    };

    struct RuntimePlan
    {
        uint32_t version = kRuntimePlanPayloadVersion;
        std::string contract_id = kRuntimePlanContractId;
        std::string join_graph_contract_id = kJoinGraphContractId;
        std::string diagnostics_contract_id = kOptimizerDiagnosticsContractId;
        std::string plan_hash;
        std::string explain_text;
        std::string cache_mode;
        std::string plan_profile_signature;
        std::string selectivity_bucket_signature;
        std::string query_feedback_key;
        bool parameter_sensitive = false;
        RuntimePlanSearchSummary search_summary;
        RuntimePlanNode root;
        std::vector<RuntimePlanRelation> relations;
        std::vector<RuntimePlanJoinStep> join_steps;
        std::vector<RuntimePlanTraceEntry> considered_paths;
        std::vector<RuntimePlanTraceEntry> rejected_paths;
        std::vector<RuntimePlanStatisticsProvenance> statistics_provenance;
        RuntimePlanAdaptiveFeedback adaptive_feedback;
        std::vector<RuntimePlanControlEntry> optimizer_controls;
    };

    auto encodeRuntimePlan(const RuntimePlan &plan,
                           std::vector<uint8_t> &bytes_out,
                           std::string &error_out) -> bool;

    auto decodeRuntimePlan(const std::vector<uint8_t> &bytes,
                           RuntimePlan &plan_out,
                           std::string &error_out) -> bool;

} // namespace scratchbird::optimizer
