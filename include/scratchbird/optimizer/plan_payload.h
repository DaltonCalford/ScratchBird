#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "scratchbird/optimizer/join_ordering.h"
#include "scratchbird/optimizer/path.h"

namespace scratchbird::optimizer
{

    inline constexpr uint32_t kRuntimePlanPayloadVersion = 25;
    inline constexpr const char *kRuntimePlanContractId = "sb_runtime_plan/v26";
    inline constexpr const char *kOptimizerProofSurfaceContractId =
        "sb_optimizer_proof_surface/v1";
    inline constexpr const char *kAdaptiveFeedbackCalibrationStoreContractId =
        "sb_runtime_learning_calibration_store/v1";
    inline constexpr const char *kRewriteBeforeSearchContractId =
        "sb_rewrite_before_search/v1";
    inline constexpr const char *kAccessPathTaggingContractId =
        "sb_access_path_tagging/v1";
    inline constexpr const char *kJoinGraphContractId = "sb_join_graph/v1";
    inline constexpr const char *kPlannerFrontDoorContractId =
        "sb_planner_front_door/v1";
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

    struct RuntimePlanCandidateRefusal
    {
        std::string family;
        std::string candidate_label;
        std::string refusal_class;
        std::string refusal_cause_domain;
        std::string refusal_reason_code;
        std::string refusal_detail;
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
        std::string physical_family;
        std::string path_name;
        PlannerAccessFamily scan_family_kind = PlannerAccessFamily::UNKNOWN;
        uint32_t taxonomy_version = kPlannerFamilyTaxonomyVersion;
        std::vector<std::string> scan_family_tags;
        std::vector<std::string> candidate_scan_families;
        std::vector<std::string> candidate_family_identity_signatures;
        std::vector<std::string> candidate_family_statistics_signatures;
        std::vector<RuntimePlanCandidateRefusal> candidate_family_refusals;
        AccessPathExactnessClass exactness_class = AccessPathExactnessClass::UNKNOWN;
        bool requires_recheck = false;
        std::string mga_family_visibility_state;
        std::string mga_recheck_contract_id;
        double coverage_fraction = 0.0;
        uint64_t candidate_budget = 0;
        AccessPathVisibilityEnforcement visibility_enforcement =
            AccessPathVisibilityEnforcement::UNKNOWN;
        uint32_t family_metrics_version = 0;
        uint64_t metrics_publication_epoch = 0;
        std::string metrics_confidence_class;
        std::string metrics_freshness_class;
        std::string metrics_invalidation_state;
        std::string metrics_invalidation_reason;
        AccessPathQueryabilityState queryability_state =
            AccessPathQueryabilityState::UNKNOWN;
        std::string native_trust_class;
        std::string locator_granularity;
        std::string family_capability_contract_id;
        std::string capability_tier;
        std::string publication_model;
        std::string mga_certification_class;
        bool supports_exact = false;
        bool supports_ordered_output = false;
        bool supports_covering_payload = false;
        bool supports_late_materialization = false;
        bool supports_bulk_filter = false;
        bool supports_parallel_merge = false;
        bool supports_specialized_collector_modes = false;
        std::string maintenance_state_class;
        uint64_t publish_lag_xids = 0;
        uint64_t maintenance_backlog_ops = 0;
        uint64_t reclaim_lag_xids = 0;
        std::string pruning_granularity_class;
        std::string projection_layout_id;
        std::string storage_layer_shape;
        std::string collector_specialization_id;
        std::string clustered_lookup_shape;
        std::string parallel_property_signature;
        std::string parallel_distribution_mode;
        std::string parallel_order_preservation;
        std::string exchange_topology_id;
        std::string gather_decision_reason;
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
        bool parallel_eligible = false;
        bool parallel_enabled = false;
        uint32_t parallel_workers_planned = 0;
        std::string parallel_stage;
        std::string parallel_rejection_reason;
        uint64_t base_rows = 0;
        double selectivity = 1.0;
        double startup_cost = 0.0;
        double total_cost = 0.0;
        uint64_t estimated_rows = 0;
        std::string formula_profile_id;
        uint32_t formula_profile_version = 0;
        std::string calibration_profile_id;
        std::string candidate_bundle_contract_id;
        std::string candidate_bundle_owner_pass_id;
        uint64_t candidate_bundle_candidate_count = 0;
        bool candidate_bundle_frozen = false;
        std::vector<std::string> rejected_composition_reasons;
        uint64_t actual_rows = 0;
        uint64_t rows_examined = 0;
        uint64_t rows_filtered = 0;
        uint64_t loop_count = 0;
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
        uint64_t retained_frontier_entry_count = 0;
        uint64_t dominated_state_count = 0;
        uint64_t max_frontier_width = 0;
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
        bool merge_enabled_by_explicit_sort = false;
        std::string merge_viability_source;
        bool ordered_output = false;
        bool order_complete = false;
        uint64_t ordered_prefix_length = 0;
        std::string ordering_class;
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
        bool adaptive_join_enabled = false;
        std::string planned_build_side;
        std::string adaptive_alternative_build_side;
        uint64_t adaptive_probe_sample_rows = 0;
        double adaptive_flip_ratio_threshold = 0.0;
        bool adaptive_join_flip_taken = false;
        uint64_t adaptive_join_sample_rows = 0;
        std::string adaptive_join_selected_build_side;
        bool parallel_eligible = false;
        bool parallel_enabled = false;
        uint32_t parallel_workers_planned = 0;
        std::string parallel_stage;
        std::string parallel_rejection_reason;
        std::string parallel_distribution_mode;
        std::string parallel_order_preservation;
        std::string exchange_topology_id;
        std::string gather_decision_reason;
        uint64_t actual_rows = 0;
        uint64_t rows_examined = 0;
        uint64_t rows_filtered = 0;
        uint64_t loop_count = 0;
        uint64_t memoize_hits = 0;
        uint64_t memoize_misses = 0;
        uint64_t memoize_evictions = 0;
        uint64_t memoize_entries = 0;
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
        uint64_t stats_snapshot_id = 0;
        uint64_t last_analyzed_time = 0;
        double sample_ratio = 0.0;
        uint64_t modified_rows_since_analyze = 0;
        std::string staleness_class;
        std::string confidence_class;
        bool auto_analyze_applied = false;
        uint64_t auto_analyze_threshold = 0;
    };

    struct RuntimePlanAdaptiveFeedback
    {
        bool available = false;
        bool replan_required = false;
        bool replan_suppressed = false;
        bool stats_refresh_requested = false;
        bool stats_refresh_applied = false;
        bool calibration_bundle_proposed = false;
        bool correction_applied = false;
        bool calibration_applied = false;
        uint64_t observation_count = 0;
        uint64_t replan_action_count = 0;
        uint64_t last_estimated_rows = 0;
        uint64_t last_actual_rows = 0;
        double estimation_error_ratio = 1.0;
        double correction_factor = 1.0;
        double cost_reweight_factor = 1.0;
        std::string calibration_store_contract_id =
            kAdaptiveFeedbackCalibrationStoreContractId;
        std::string calibration_store_state = "EMPTY";
        bool calibration_fail_closed = true;
        uint32_t calibration_profile_version = 0;
        std::string last_plan_hash;
        std::string calibration_profile_id;
        std::string calibration_profile_delta_id;
        std::string calibration_evidence_id;
        std::string guardrail_reason;
    };

    struct RuntimePlanControlEntry
    {
        std::string name;
        std::string value;
        std::string source;
        bool enforced = true;
    };

    struct RuntimePlanAdvisorSignal
    {
        std::string signal_name;
        std::string severity;
        std::string provenance_source;
        std::string detail;
    };

    struct RuntimePlanAdvisorRecommendation
    {
        uint32_t rank = 0;
        std::string recommendation_type;
        std::string table_name;
        std::string index_name;
        std::vector<std::string> column_names;
        std::string create_sql;
        std::string drop_sql;
        std::string reason;
        std::string provenance_source;
        std::string query_fingerprint;
        std::vector<std::string> signal_names;
        double benefit_score = 0.0;
        double cost_score = 0.0;
        double net_benefit = 0.0;
        uint64_t affected_queries = 0;
        double estimated_size_mb = 0.0;
        double estimated_speedup = 0.0;
        double priority = 0.0;
        double confidence = 0.0;
        bool what_if_replanned = false;
        std::string baseline_access_family;
        std::string baseline_index_name;
        double baseline_total_cost = 0.0;
        uint64_t baseline_estimated_rows = 0;
        std::string hypothetical_access_family;
        std::string hypothetical_index_name;
        double hypothetical_total_cost = 0.0;
        uint64_t hypothetical_estimated_rows = 0;
        double estimated_cost_delta = 0.0;
        double estimated_speedup_ratio = 1.0;
        bool ordering_improved = false;
        bool covering_improved = false;
        std::string evidence_detail;
    };

    struct RuntimePlanCostInputEstimate
    {
        std::string name;
        double value = 0.0;
        std::string unit;
    };

    struct RuntimePlanCostTerm
    {
        std::string name;
        double coefficient = 0.0;
        double input_value = 0.0;
        double contribution = 0.0;
        std::string unit;
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
        bool parallel_aware = false;
        bool parallel_enabled = false;
        uint32_t parallel_workers_planned = 0;
        bool gather_merge = false;
        std::string parallel_stage;
        std::string parallel_reason;
        double startup_cost = 0.0;
        double total_cost = 0.0;
        uint64_t estimated_rows = 0;
        bool actuals_available = false;
        uint64_t actual_rows = 0;
        uint64_t rows_examined = 0;
        uint64_t rows_filtered = 0;
        uint64_t loop_count = 0;
        uint64_t startup_time_us = 0;
        uint64_t execution_time_us = 0;
        uint64_t estimated_memory_bytes = 0;
        uint64_t memory_budget_bytes = 0;
        bool spill_expected = false;
        uint32_t spill_passes = 0;
        uint64_t spill_bytes = 0;
        std::string spill_policy;
        std::string formula_profile_id;
        uint32_t formula_profile_version = 0;
        std::string calibration_profile_id;
        std::string storage_profile;
        std::string workload_profile;
        std::string resource_governance_outcome;
        std::vector<RuntimePlanCostInputEstimate> input_estimates;
        std::vector<RuntimePlanCostTerm> expanded_cost_terms;
        std::vector<RuntimePlanNode> children;
    };

    struct RuntimePlan
    {
        uint32_t version = kRuntimePlanPayloadVersion;
        std::string contract_id = kRuntimePlanContractId;
        std::string join_graph_contract_id = kJoinGraphContractId;
        std::string join_search_contract_id = kJoinSearchContractId;
        std::string join_search_property_signature_contract_id =
            kJoinSearchPropertySignatureContractId;
        std::string join_search_frontier_mode = kJoinSearchFrontierMode;
        std::string join_search_mode_source;
        std::string base_candidate_bundle_contract_id;
        std::string base_candidate_bundle_owner_pass_id;
        std::string base_candidate_bundle_consumer_pass_id;
        bool base_candidate_bundle_frozen = false;
        uint64_t base_candidate_bundle_rejection_count = 0;
        std::string planner_front_door_contract_id =
            kPlannerFrontDoorContractId;
        std::string diagnostics_contract_id = kOptimizerDiagnosticsContractId;
        uint32_t planner_status_code = 0;
        std::string plan_hash;
        std::string explain_text;
        std::string normalized_request_digest;
        std::string normalized_statement_id;
        std::string statement_kind;
        std::string cache_mode;
        std::string chosen_reuse_mode;
        std::string plan_profile_signature;
        std::string index_family_signature;
        std::string family_statistics_signature;
        std::string selectivity_bucket_signature;
        std::string query_feedback_key;
        std::string storage_layer_shape = "ROW_STORE_MGA";
        std::string publication_state_summary;
        std::string collector_specialization_id;
        std::string execution_intent_class;
        std::string continuation_token_contract;
        std::string rewrite_before_search_contract_id =
            kRewriteBeforeSearchContractId;
        std::string rewrite_before_search_owner_pass_id =
            "P01_SEMANTIC_NORMALIZE";
        std::string rewrite_before_search_terminal_pass_id =
            "P07_FILTER_PUSH_DOWN";
        bool rewrite_before_search_frozen = false;
        std::string tagging_contract_id = kAccessPathTaggingContractId;
        std::string tagging_owner_pass_id = "P08_ACCESS_PATH_ANNOTATE";
        std::string join_search_owner_pass_id = "P09_JOIN_ORDER_PLAN";
        std::string result_shape_finalize_pass_id =
            "P10_RESULT_SHAPE_FINALIZE";
        std::string proof_surface_contract_id =
            kOptimizerProofSurfaceContractId;
        bool proof_surface_complete = false;
        uint32_t proof_surface_claim_count = 0;
        std::string proof_surface_json;
        std::string diagnostics_payload_json;
        bool parameter_sensitive = false;
        uint64_t join_search_base_candidate_count = 0;
        std::vector<std::string> invalidation_dependencies;
        std::vector<std::string> compatibility_version_identifiers;
        std::vector<std::string> fallback_and_rejection_stream;
        RuntimePlanSearchSummary search_summary;
        RuntimePlanNode root;
        std::vector<RuntimePlanRelation> relations;
        std::vector<RuntimePlanJoinStep> join_steps;
        std::vector<RuntimePlanTraceEntry> considered_paths;
        std::vector<RuntimePlanTraceEntry> rejected_paths;
        std::vector<RuntimePlanStatisticsProvenance> statistics_provenance;
        RuntimePlanAdaptiveFeedback adaptive_feedback;
        std::vector<RuntimePlanControlEntry> optimizer_controls;
        std::vector<RuntimePlanAdvisorSignal> advisor_signals;
        std::vector<RuntimePlanAdvisorRecommendation> advisor_recommendations;
    };

    auto encodeRuntimePlan(const RuntimePlan &plan,
                           std::vector<uint8_t> &bytes_out,
                           std::string &error_out) -> bool;

    auto decodeRuntimePlan(const std::vector<uint8_t> &bytes,
                           RuntimePlan &plan_out,
                           std::string &error_out) -> bool;

    auto adaptiveFeedbackPlanHash(const RuntimePlan &plan) -> std::string;

} // namespace scratchbird::optimizer
