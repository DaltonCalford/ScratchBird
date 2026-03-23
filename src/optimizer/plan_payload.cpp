#include "scratchbird/optimizer/plan_payload.h"
#include "scratchbird/sblr/v3_plan_cache_key.h"

#include <nlohmann/json.hpp>
#include <sstream>

namespace scratchbird::optimizer
{
    namespace
    {
        auto appendNodeIdentitySeed(std::ostringstream &out,
                                    const RuntimePlanNode &node) -> void
        {
            out << node.node_type << ':'
                << node.relation_alias << ':'
                << node.table_path << ':'
                << node.join_type << ':'
                << node.index_name << ':'
                << (node.parallel_enabled ? 1 : 0) << ':'
                << (node.gather_merge ? 1 : 0) << ':'
                << node.parallel_stage << ':'
                << node.formula_profile_id << ':'
                << node.formula_profile_version << ':'
                << node.calibration_profile_id << '|';
            for (const auto &child : node.children)
            {
                appendNodeIdentitySeed(out, child);
            }
            out << '#';
        }

        auto relationToJson(const RuntimePlanRelation &relation) -> nlohmann::json
        {
            nlohmann::json out;
            out["source_relation_index"] = relation.source_relation_index;
            out["table_path"] = relation.table_path;
            out["physical_table_path"] = relation.physical_table_path;
            out["alias"] = relation.alias;
            out["table_id_text"] = relation.table_id_text;
            out["scan_kind"] = relation.scan_kind;
            out["scan_family"] = relation.scan_family;
            out["physical_family"] = relation.physical_family;
            out["path_name"] = relation.path_name;
            out["scan_family_kind"] =
                plannerAccessFamilyName(relation.scan_family_kind);
            out["scan_family_kind_id"] =
                static_cast<uint32_t>(relation.scan_family_kind);
            out["taxonomy_version"] = relation.taxonomy_version;
            out["scan_family_tags"] = relation.scan_family_tags;
            out["candidate_scan_families"] = relation.candidate_scan_families;
            out["candidate_family_identity_signatures"] =
                relation.candidate_family_identity_signatures;
            out["candidate_family_statistics_signatures"] =
                relation.candidate_family_statistics_signatures;
            out["exactness_class"] =
                accessPathExactnessClassName(relation.exactness_class);
            out["exactness_class_id"] =
                static_cast<uint32_t>(relation.exactness_class);
            out["requires_recheck"] = relation.requires_recheck;
            out["mga_family_visibility_state"] =
                relation.mga_family_visibility_state;
            out["mga_recheck_contract_id"] =
                relation.mga_recheck_contract_id;
            out["coverage_fraction"] = relation.coverage_fraction;
            out["candidate_budget"] = relation.candidate_budget;
            out["visibility_enforcement"] =
                accessPathVisibilityEnforcementName(
                    relation.visibility_enforcement);
            out["visibility_enforcement_id"] =
                static_cast<uint32_t>(relation.visibility_enforcement);
            out["family_metrics_version"] = relation.family_metrics_version;
            out["metrics_confidence_class"] = relation.metrics_confidence_class;
            out["queryability_state"] =
                accessPathQueryabilityStateName(relation.queryability_state);
            out["queryability_state_id"] =
                static_cast<uint32_t>(relation.queryability_state);
            out["native_trust_class"] = relation.native_trust_class;
            out["locator_granularity"] = relation.locator_granularity;
            out["family_capability_contract_id"] =
                relation.family_capability_contract_id;
            out["capability_tier"] = relation.capability_tier;
            out["publication_model"] = relation.publication_model;
            out["mga_certification_class"] =
                relation.mga_certification_class;
            out["supports_exact"] = relation.supports_exact;
            out["supports_ordered_output"] =
                relation.supports_ordered_output;
            out["supports_covering_payload"] =
                relation.supports_covering_payload;
            out["supports_late_materialization"] =
                relation.supports_late_materialization;
            out["supports_bulk_filter"] = relation.supports_bulk_filter;
            out["supports_parallel_merge"] =
                relation.supports_parallel_merge;
            out["supports_specialized_collector_modes"] =
                relation.supports_specialized_collector_modes;
            out["maintenance_state_class"] =
                relation.maintenance_state_class;
            out["publish_lag_xids"] = relation.publish_lag_xids;
            out["maintenance_backlog_ops"] =
                relation.maintenance_backlog_ops;
            out["reclaim_lag_xids"] = relation.reclaim_lag_xids;
            out["pruning_granularity_class"] =
                relation.pruning_granularity_class;
            out["projection_layout_id"] = relation.projection_layout_id;
            out["storage_layer_shape"] = relation.storage_layer_shape;
            out["collector_specialization_id"] =
                relation.collector_specialization_id;
            out["clustered_lookup_shape"] =
                relation.clustered_lookup_shape;
            out["parallel_property_signature"] =
                relation.parallel_property_signature;
            out["index_name"] = relation.index_name;
            out["index_id_text"] = relation.index_id_text;
            out["bitmap_op"] = relation.bitmap_op;
            out["covering_index"] = relation.covering_index;
            out["exact_key_lookup"] = relation.exact_key_lookup;
            out["flattened_derived"] = relation.flattened_derived;
            out["lateral"] = relation.lateral;
            out["parameterized"] = relation.parameterized;
            out["ordered_output"] = relation.ordered_output;
            out["ordered_prefix_length"] = relation.ordered_prefix_length;
            out["required_outer_relation_indexes"] =
                relation.required_outer_relation_indexes;
            out["required_outer_relation_aliases"] =
                relation.required_outer_relation_aliases;
            out["partition_pruned"] = relation.partition_pruned;
            out["partition_strategy"] = relation.partition_strategy;
            out["partition_key_column"] = relation.partition_key_column;
            out["partition_key_columns"] = relation.partition_key_columns;
            out["partition_targets"] = relation.partition_targets;
            out["partition_targets_pruned_at_plan"] =
                relation.partition_targets_pruned_at_plan;
            out["runtime_partition_pruning_eligible"] =
                relation.runtime_partition_pruning_eligible;
            out["runtime_partition_pruning_sources"] =
                relation.runtime_partition_pruning_sources;
            out["runtime_filter_enabled"] = relation.runtime_filter_enabled;
            out["runtime_filter_column"] = relation.runtime_filter_column;
            out["runtime_filter_index_name"] = relation.runtime_filter_index_name;
            out["runtime_filter_index_id_text"] = relation.runtime_filter_index_id_text;
            out["parallel_eligible"] = relation.parallel_eligible;
            out["parallel_enabled"] = relation.parallel_enabled;
            out["parallel_workers_planned"] = relation.parallel_workers_planned;
            out["parallel_stage"] = relation.parallel_stage;
            out["parallel_rejection_reason"] = relation.parallel_rejection_reason;
            out["parallel_distribution_mode"] =
                relation.parallel_distribution_mode;
            out["parallel_order_preservation"] =
                relation.parallel_order_preservation;
            out["exchange_topology_id"] = relation.exchange_topology_id;
            out["gather_decision_reason"] =
                relation.gather_decision_reason;
            out["base_rows"] = relation.base_rows;
            out["selectivity"] = relation.selectivity;
            out["startup_cost"] = relation.startup_cost;
            out["total_cost"] = relation.total_cost;
            out["estimated_rows"] = relation.estimated_rows;
            out["formula_profile_id"] = relation.formula_profile_id;
            out["formula_profile_version"] = relation.formula_profile_version;
            out["calibration_profile_id"] = relation.calibration_profile_id;
            out["candidate_bundle_contract_id"] =
                relation.candidate_bundle_contract_id;
            out["candidate_bundle_owner_pass_id"] =
                relation.candidate_bundle_owner_pass_id;
            out["candidate_bundle_candidate_count"] =
                relation.candidate_bundle_candidate_count;
            out["candidate_bundle_frozen"] =
                relation.candidate_bundle_frozen;
            out["rejected_composition_reasons"] =
                relation.rejected_composition_reasons;
            out["actual_rows"] = relation.actual_rows;
            out["rows_examined"] = relation.rows_examined;
            out["rows_filtered"] = relation.rows_filtered;
            out["loop_count"] = relation.loop_count;

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
            out["partition_predicates"] = nlohmann::json::array();
            for (const auto &entry : relation.partition_predicates)
            {
                nlohmann::json pred;
                pred["valid"] = entry.valid;
                pred["index_name"] = entry.index_name;
                pred["index_id_text"] = entry.index_id_text;
                pred["column_name"] = entry.column_name;
                pred["operator_name"] = entry.operator_name;
                pred["literal_kind"] = entry.literal_kind;
                pred["literal_text"] = entry.literal_text;
                out["partition_predicates"].push_back(std::move(pred));
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
            relation_out.scan_family = json_in.value("scan_family", std::string());
            relation_out.physical_family =
                json_in.value("physical_family",
                              relation_out.scan_family);
            relation_out.path_name =
                json_in.value("path_name", relation_out.scan_family);
            relation_out.taxonomy_version =
                json_in.value("taxonomy_version", kPlannerFamilyTaxonomyVersion);
            const auto scan_family_kind_id_it =
                json_in.find("scan_family_kind_id");
            if (scan_family_kind_id_it != json_in.end() &&
                scan_family_kind_id_it->is_number_unsigned())
            {
                relation_out.scan_family_kind = static_cast<PlannerAccessFamily>(
                    scan_family_kind_id_it->get<uint32_t>());
            }
            else
            {
                relation_out.scan_family_kind = plannerAccessFamilyFromLegacy(
                    json_in.value("scan_family_kind", relation_out.scan_family),
                    relation_out.scan_kind);
            }
            relation_out.scan_family_tags.clear();
            const auto scan_family_tags_it = json_in.find("scan_family_tags");
            if (scan_family_tags_it != json_in.end() && scan_family_tags_it->is_array())
            {
                for (const auto &entry : *scan_family_tags_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.scan_family_tags.push_back(entry.get<std::string>());
                    }
                }
            }
            relation_out.candidate_scan_families.clear();
            const auto candidate_scan_families_it =
                json_in.find("candidate_scan_families");
            if (candidate_scan_families_it != json_in.end() &&
                candidate_scan_families_it->is_array())
            {
                for (const auto &entry : *candidate_scan_families_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.candidate_scan_families.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            relation_out.candidate_family_identity_signatures.clear();
            const auto candidate_family_identity_signatures_it =
                json_in.find("candidate_family_identity_signatures");
            if (candidate_family_identity_signatures_it != json_in.end() &&
                candidate_family_identity_signatures_it->is_array())
            {
                for (const auto &entry : *candidate_family_identity_signatures_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.candidate_family_identity_signatures.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            relation_out.candidate_family_statistics_signatures.clear();
            const auto candidate_family_statistics_signatures_it =
                json_in.find("candidate_family_statistics_signatures");
            if (candidate_family_statistics_signatures_it != json_in.end() &&
                candidate_family_statistics_signatures_it->is_array())
            {
                for (const auto &entry :
                     *candidate_family_statistics_signatures_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.candidate_family_statistics_signatures
                            .push_back(entry.get<std::string>());
                    }
                }
            }
            const auto exactness_class_id_it = json_in.find("exactness_class_id");
            if (exactness_class_id_it != json_in.end() &&
                exactness_class_id_it->is_number_unsigned())
            {
                relation_out.exactness_class =
                    static_cast<AccessPathExactnessClass>(
                        exactness_class_id_it->get<uint32_t>());
            }
            else
            {
                relation_out.exactness_class = accessPathExactnessFromLegacy(
                    relation_out.scan_family, relation_out.scan_kind);
            }
            relation_out.requires_recheck =
                json_in.value("requires_recheck",
                              relation_out.exactness_class ==
                                      AccessPathExactnessClass::CANDIDATE_REGION ||
                                  relation_out.exactness_class ==
                                      AccessPathExactnessClass::LOWER_BOUND_ORDERED ||
                                  relation_out.exactness_class ==
                                      AccessPathExactnessClass::APPROX_TOPK);
            relation_out.mga_family_visibility_state =
                json_in.value("mga_family_visibility_state", std::string());
            relation_out.mga_recheck_contract_id =
                json_in.value("mga_recheck_contract_id", std::string());
            relation_out.coverage_fraction =
                json_in.value("coverage_fraction", 0.0);
            relation_out.candidate_budget =
                json_in.value("candidate_budget", 0ULL);
            const auto visibility_enforcement_id_it =
                json_in.find("visibility_enforcement_id");
            if (visibility_enforcement_id_it != json_in.end() &&
                visibility_enforcement_id_it->is_number_unsigned())
            {
                relation_out.visibility_enforcement =
                    static_cast<AccessPathVisibilityEnforcement>(
                        visibility_enforcement_id_it->get<uint32_t>());
            }
            else
            {
                relation_out.visibility_enforcement =
                    accessPathVisibilityEnforcementFromLegacy(
                        relation_out.scan_family, relation_out.scan_kind);
            }
            relation_out.family_metrics_version =
                json_in.value("family_metrics_version", 0U);
            relation_out.metrics_confidence_class =
                json_in.value("metrics_confidence_class", std::string());
            const auto queryability_state_id_it =
                json_in.find("queryability_state_id");
            if (queryability_state_id_it != json_in.end() &&
                queryability_state_id_it->is_number_unsigned())
            {
                relation_out.queryability_state =
                    static_cast<AccessPathQueryabilityState>(
                        queryability_state_id_it->get<uint32_t>());
            }
            else
            {
                relation_out.queryability_state =
                    relation_out.scan_family_kind == PlannerAccessFamily::UNKNOWN
                        ? AccessPathQueryabilityState::UNKNOWN
                        : AccessPathQueryabilityState::QUERYABLE;
            }
            relation_out.native_trust_class =
                json_in.value("native_trust_class",
                              std::string(accessPathNativeTrustClassName(
                                  relation_out.scan_family_kind,
                                  relation_out.exactness_class,
                                  relation_out.visibility_enforcement,
                                  relation_out.requires_recheck)));
            relation_out.locator_granularity =
                json_in.value("locator_granularity",
                              std::string(accessPathLocatorGranularityName(
                                  relation_out.scan_family_kind,
                                  relation_out.exactness_class,
                                  relation_out.visibility_enforcement)));
            relation_out.family_capability_contract_id =
                json_in.value("family_capability_contract_id",
                              std::string(kIndexFamilyCapabilityContractId));
            relation_out.capability_tier =
                json_in.value("capability_tier", std::string());
            relation_out.publication_model =
                json_in.value("publication_model",
                              std::string(accessPathPublicationModelName(
                                  relation_out.scan_family_kind,
                                  relation_out.exactness_class,
                                  relation_out.visibility_enforcement)));
            relation_out.mga_certification_class =
                json_in.value("mga_certification_class",
                              std::string(accessPathMgaCertificationClassName(
                                  relation_out.exactness_class,
                                  relation_out.visibility_enforcement,
                                  relation_out.requires_recheck)));
            relation_out.supports_exact =
                json_in.value("supports_exact",
                              accessPathSupportsExact(
                                  relation_out.exactness_class));
            relation_out.supports_ordered_output =
                json_in.value("supports_ordered_output",
                              accessPathSupportsOrderedOutput(
                                  relation_out.scan_family_kind,
                                  relation_out.ordered_output));
            relation_out.supports_covering_payload =
                json_in.value("supports_covering_payload",
                              accessPathSupportsCoveringPayload(
                                  relation_out.scan_family_kind,
                                  relation_out.covering_index));
            relation_out.supports_late_materialization =
                json_in.value("supports_late_materialization",
                              accessPathSupportsLateMaterialization(
                                  relation_out.locator_granularity));
            relation_out.supports_bulk_filter =
                json_in.value("supports_bulk_filter",
                              accessPathSupportsBulkFilter(
                                  relation_out.scan_family_kind));
            relation_out.supports_parallel_merge =
                json_in.value("supports_parallel_merge",
                              accessPathSupportsParallelMerge(
                                  relation_out.scan_family_kind));
            relation_out.supports_specialized_collector_modes =
                json_in.value(
                    "supports_specialized_collector_modes",
                    accessPathSupportsSpecializedCollectorModes(
                        relation_out.collector_specialization_id));
            relation_out.publish_lag_xids =
                json_in.value("publish_lag_xids", 0ULL);
            relation_out.maintenance_backlog_ops =
                json_in.value("maintenance_backlog_ops", 0ULL);
            relation_out.reclaim_lag_xids =
                json_in.value("reclaim_lag_xids", 0ULL);
            relation_out.maintenance_state_class =
                json_in.value("maintenance_state_class",
                              std::string(accessPathMaintenanceStateClassName(
                                  relation_out.queryability_state,
                                  relation_out.metrics_confidence_class,
                                  relation_out.publish_lag_xids,
                                  relation_out.maintenance_backlog_ops,
                                  relation_out.reclaim_lag_xids)));
            relation_out.pruning_granularity_class =
                json_in.value("pruning_granularity_class", std::string());
            relation_out.projection_layout_id =
                json_in.value("projection_layout_id", std::string());
            relation_out.storage_layer_shape =
                json_in.value("storage_layer_shape", std::string());
            relation_out.collector_specialization_id =
                json_in.value("collector_specialization_id", std::string());
            relation_out.clustered_lookup_shape =
                json_in.value("clustered_lookup_shape", std::string());
            relation_out.parallel_property_signature =
                json_in.value("parallel_property_signature", std::string());
            relation_out.index_name = json_in.value("index_name", std::string());
            relation_out.index_id_text = json_in.value("index_id_text", std::string());
            relation_out.bitmap_op = json_in.value("bitmap_op", std::string());
            relation_out.covering_index = json_in.value("covering_index", false);
            relation_out.exact_key_lookup = json_in.value("exact_key_lookup", false);
            relation_out.flattened_derived = json_in.value("flattened_derived", false);
            relation_out.lateral = json_in.value("lateral", false);
            relation_out.parameterized = json_in.value("parameterized", false);
            relation_out.ordered_output = json_in.value("ordered_output", false);
            relation_out.ordered_prefix_length =
                json_in.value("ordered_prefix_length", 0ULL);
            relation_out.supports_ordered_output =
                json_in.value("supports_ordered_output",
                              accessPathSupportsOrderedOutput(
                                  relation_out.scan_family_kind,
                                  relation_out.ordered_output));
            relation_out.supports_covering_payload =
                json_in.value("supports_covering_payload",
                              accessPathSupportsCoveringPayload(
                                  relation_out.scan_family_kind,
                                  relation_out.covering_index));
            relation_out.supports_specialized_collector_modes =
                json_in.value(
                    "supports_specialized_collector_modes",
                    accessPathSupportsSpecializedCollectorModes(
                        relation_out.collector_specialization_id));
            const auto capability = buildAccessPathFamilyCapability(
                relation_out.scan_family_kind,
                relation_out.exactness_class,
                relation_out.visibility_enforcement,
                relation_out.requires_recheck,
                relation_out.ordered_output,
                relation_out.covering_index,
                relation_out.collector_specialization_id);
            relation_out.native_trust_class = capability.native_trust_class;
            relation_out.locator_granularity =
                capability.locator_granularity;
            relation_out.family_capability_contract_id =
                capability.contract_id;
            relation_out.capability_tier = capability.capability_tier;
            relation_out.publication_model =
                capability.publication_model;
            relation_out.mga_certification_class =
                capability.mga_certification_class;
            relation_out.supports_exact = capability.supports_exact;
            relation_out.supports_ordered_output =
                capability.supports_ordered_output;
            relation_out.supports_covering_payload =
                capability.supports_covering_payload;
            relation_out.supports_late_materialization =
                capability.supports_late_materialization;
            relation_out.supports_bulk_filter =
                capability.supports_bulk_filter;
            relation_out.supports_parallel_merge =
                capability.supports_parallel_merge;
            relation_out.supports_specialized_collector_modes =
                capability.supports_specialized_collector_modes;
            relation_out.required_outer_relation_indexes.clear();
            const auto required_outer_relation_indexes_it =
                json_in.find("required_outer_relation_indexes");
            if (required_outer_relation_indexes_it != json_in.end() &&
                required_outer_relation_indexes_it->is_array())
            {
                for (const auto &entry : *required_outer_relation_indexes_it)
                {
                    if (entry.is_number_unsigned())
                    {
                        relation_out.required_outer_relation_indexes.push_back(
                            entry.get<size_t>());
                    }
                }
            }
            relation_out.required_outer_relation_aliases.clear();
            const auto required_outer_relation_aliases_it =
                json_in.find("required_outer_relation_aliases");
            if (required_outer_relation_aliases_it != json_in.end() &&
                required_outer_relation_aliases_it->is_array())
            {
                for (const auto &entry : *required_outer_relation_aliases_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.required_outer_relation_aliases.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            relation_out.partition_pruned = json_in.value("partition_pruned", false);
            relation_out.partition_strategy =
                json_in.value("partition_strategy", std::string());
            relation_out.partition_key_column =
                json_in.value("partition_key_column", std::string());
            relation_out.partition_key_columns.clear();
            const auto partition_key_columns_it =
                json_in.find("partition_key_columns");
            if (partition_key_columns_it != json_in.end() &&
                partition_key_columns_it->is_array())
            {
                for (const auto &entry : *partition_key_columns_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.partition_key_columns.push_back(
                            entry.get<std::string>());
                    }
                }
            }
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
            relation_out.partition_targets_pruned_at_plan.clear();
            const auto partition_targets_pruned_it =
                json_in.find("partition_targets_pruned_at_plan");
            if (partition_targets_pruned_it != json_in.end() &&
                partition_targets_pruned_it->is_array())
            {
                for (const auto &entry : *partition_targets_pruned_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.partition_targets_pruned_at_plan.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            relation_out.runtime_partition_pruning_eligible =
                json_in.value("runtime_partition_pruning_eligible", false);
            relation_out.runtime_partition_pruning_sources.clear();
            const auto runtime_partition_sources_it =
                json_in.find("runtime_partition_pruning_sources");
            if (runtime_partition_sources_it != json_in.end() &&
                runtime_partition_sources_it->is_array())
            {
                for (const auto &entry : *runtime_partition_sources_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.runtime_partition_pruning_sources.push_back(
                            entry.get<std::string>());
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
            relation_out.parallel_eligible =
                json_in.value("parallel_eligible", false);
            relation_out.parallel_enabled =
                json_in.value("parallel_enabled", false);
            relation_out.parallel_workers_planned =
                json_in.value("parallel_workers_planned", 0U);
            relation_out.parallel_stage =
                json_in.value("parallel_stage", std::string());
            relation_out.parallel_rejection_reason =
                json_in.value("parallel_rejection_reason", std::string());
            relation_out.parallel_distribution_mode =
                json_in.value("parallel_distribution_mode", std::string());
            relation_out.parallel_order_preservation =
                json_in.value("parallel_order_preservation", std::string());
            relation_out.exchange_topology_id =
                json_in.value("exchange_topology_id", std::string());
            relation_out.gather_decision_reason =
                json_in.value("gather_decision_reason", std::string());
            relation_out.base_rows = json_in.value("base_rows", 0ULL);
            relation_out.selectivity = json_in.value("selectivity", 1.0);
            relation_out.startup_cost = json_in.value("startup_cost", 0.0);
            relation_out.total_cost = json_in.value("total_cost", 0.0);
            relation_out.estimated_rows = json_in.value("estimated_rows", 0ULL);
            relation_out.formula_profile_id =
                json_in.value("formula_profile_id", std::string());
            relation_out.formula_profile_version =
                json_in.value("formula_profile_version", 0U);
            relation_out.calibration_profile_id =
                json_in.value("calibration_profile_id", std::string());
            relation_out.candidate_bundle_contract_id =
                json_in.value("candidate_bundle_contract_id", std::string());
            relation_out.candidate_bundle_owner_pass_id =
                json_in.value("candidate_bundle_owner_pass_id", std::string());
            relation_out.candidate_bundle_candidate_count =
                json_in.value("candidate_bundle_candidate_count", 0ULL);
            relation_out.candidate_bundle_frozen =
                json_in.value("candidate_bundle_frozen", false);
            relation_out.rejected_composition_reasons.clear();
            const auto rejected_compositions_it =
                json_in.find("rejected_composition_reasons");
            if (rejected_compositions_it != json_in.end() &&
                rejected_compositions_it->is_array())
            {
                for (const auto &entry : *rejected_compositions_it)
                {
                    if (entry.is_string())
                    {
                        relation_out.rejected_composition_reasons.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            relation_out.actual_rows = json_in.value("actual_rows", 0ULL);
            relation_out.rows_examined = json_in.value("rows_examined", 0ULL);
            relation_out.rows_filtered = json_in.value("rows_filtered", 0ULL);
            relation_out.loop_count = json_in.value("loop_count", 0ULL);

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

            relation_out.partition_predicates.clear();
            const auto partition_preds_it = json_in.find("partition_predicates");
            if (partition_preds_it != json_in.end() && partition_preds_it->is_array())
            {
                for (const auto &entry : *partition_preds_it)
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
                    relation_out.partition_predicates.push_back(std::move(pred));
                }
            }
            return true;
        }

        auto joinStepToJson(const RuntimePlanJoinStep &join_step) -> nlohmann::json
        {
            nlohmann::json out;
            out["source_join_index"] = join_step.source_join_index;
            out["right_relation_index"] = join_step.right_relation_index;
            out["join_edge_left_relation_index"] =
                join_step.join_edge_left_relation_index;
            out["join_edge_right_relation_index"] =
                join_step.join_edge_right_relation_index;
            out["join_edge_left_alias"] = join_step.join_edge_left_alias;
            out["join_edge_right_alias"] = join_step.join_edge_right_alias;
            out["join_edge_left_id_text"] = join_step.join_edge_left_id_text;
            out["join_edge_right_id_text"] = join_step.join_edge_right_id_text;
            out["join_type"] = join_step.join_type;
            out["method"] = join_step.method;
            out["disconnected_component"] = join_step.disconnected_component;
            out["legality_class"] = join_step.legality_class;
            out["legal_method_families"] = join_step.legal_method_families;
            out["method_enablers"] = join_step.method_enablers;
            out["reorderable"] = join_step.reorderable;
            out["natural"] = join_step.natural;
            out["using_columns"] = join_step.using_columns;
            out["condition_text"] = join_step.condition_text;
            out["equijoin_keys"] = nlohmann::json::array();
            for (const auto &entry : join_step.equijoin_keys)
            {
                nlohmann::json key;
                key["left_qualifier"] = entry.left_qualifier;
                key["left_column_name"] = entry.left_column_name;
                key["right_qualifier"] = entry.right_qualifier;
                key["right_column_name"] = entry.right_column_name;
                out["equijoin_keys"].push_back(std::move(key));
            }
            out["residual_predicates"] = join_step.residual_predicates;
            out["preserves_left_rows"] = join_step.preserves_left_rows;
            out["preserves_right_rows"] = join_step.preserves_right_rows;
            out["null_introduces_left"] = join_step.null_introduces_left;
            out["null_introduces_right"] = join_step.null_introduces_right;
            out["requires_original_order"] = join_step.requires_original_order;
            out["outer_reorder_barrier"] = join_step.outer_reorder_barrier;
            out["semi_reorder_barrier"] = join_step.semi_reorder_barrier;
            out["anti_reorder_barrier"] = join_step.anti_reorder_barrier;
            out["using_reorder_barrier"] = join_step.using_reorder_barrier;
            out["natural_reorder_barrier"] = join_step.natural_reorder_barrier;
            out["lateral_reorder_barrier"] = join_step.lateral_reorder_barrier;
            out["parameterized_dependency"] = join_step.parameterized_dependency;
            out["parameter_dependency_relation_indexes"] =
                join_step.parameter_dependency_relation_indexes;
            out["parameter_dependency_relation_aliases"] =
                join_step.parameter_dependency_relation_aliases;
            out["has_hash_keys"] = join_step.has_hash_keys;
            out["has_merge_keys"] = join_step.has_merge_keys;
            out["merge_outer_presorted"] = join_step.merge_outer_presorted;
            out["merge_inner_presorted"] = join_step.merge_inner_presorted;
            out["merge_enabled_by_explicit_sort"] =
                join_step.merge_enabled_by_explicit_sort;
            out["merge_viability_source"] = join_step.merge_viability_source;
            out["ordered_output"] = join_step.ordered_output;
            out["order_complete"] = join_step.order_complete;
            out["ordered_prefix_length"] = join_step.ordered_prefix_length;
            out["ordering_class"] = join_step.ordering_class;
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
            out["parallel_eligible"] = join_step.parallel_eligible;
            out["parallel_enabled"] = join_step.parallel_enabled;
            out["parallel_workers_planned"] = join_step.parallel_workers_planned;
            out["parallel_stage"] = join_step.parallel_stage;
            out["parallel_rejection_reason"] =
                join_step.parallel_rejection_reason;
            out["parallel_distribution_mode"] =
                join_step.parallel_distribution_mode;
            out["parallel_order_preservation"] =
                join_step.parallel_order_preservation;
            out["exchange_topology_id"] = join_step.exchange_topology_id;
            out["gather_decision_reason"] =
                join_step.gather_decision_reason;
            out["actual_rows"] = join_step.actual_rows;
            out["rows_examined"] = join_step.rows_examined;
            out["rows_filtered"] = join_step.rows_filtered;
            out["loop_count"] = join_step.loop_count;

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
            join_step_out.join_edge_left_relation_index =
                json_in.value("join_edge_left_relation_index", 0U);
            join_step_out.join_edge_right_relation_index =
                json_in.value("join_edge_right_relation_index", 0U);
            join_step_out.join_edge_left_alias =
                json_in.value("join_edge_left_alias", std::string());
            join_step_out.join_edge_right_alias =
                json_in.value("join_edge_right_alias", std::string());
            join_step_out.join_edge_left_id_text =
                json_in.value("join_edge_left_id_text", std::string());
            join_step_out.join_edge_right_id_text =
                json_in.value("join_edge_right_id_text", std::string());
            join_step_out.join_type = json_in.value("join_type", std::string());
            join_step_out.method = json_in.value("method", std::string());
            join_step_out.disconnected_component =
                json_in.value("disconnected_component", false);
            join_step_out.legality_class =
                json_in.value("legality_class", std::string());
            join_step_out.legal_method_families.clear();
            const auto legal_families_it = json_in.find("legal_method_families");
            if (legal_families_it != json_in.end() && legal_families_it->is_array())
            {
                for (const auto &entry : *legal_families_it)
                {
                    if (entry.is_string())
                    {
                        join_step_out.legal_method_families.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            join_step_out.method_enablers.clear();
            const auto method_enablers_it = json_in.find("method_enablers");
            if (method_enablers_it != json_in.end() && method_enablers_it->is_array())
            {
                for (const auto &entry : *method_enablers_it)
                {
                    if (entry.is_string())
                    {
                        join_step_out.method_enablers.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            join_step_out.reorderable = json_in.value("reorderable", true);
            join_step_out.natural = json_in.value("natural", false);
            join_step_out.condition_text = json_in.value("condition_text", std::string());
            join_step_out.equijoin_keys.clear();
            const auto equijoin_it = json_in.find("equijoin_keys");
            if (equijoin_it != json_in.end() && equijoin_it->is_array())
            {
                for (const auto &entry : *equijoin_it)
                {
                    if (!entry.is_object())
                    {
                        continue;
                    }
                    RuntimePlanJoinKeyPair key_pair;
                    key_pair.left_qualifier =
                        entry.value("left_qualifier", std::string());
                    key_pair.left_column_name =
                        entry.value("left_column_name", std::string());
                    key_pair.right_qualifier =
                        entry.value("right_qualifier", std::string());
                    key_pair.right_column_name =
                        entry.value("right_column_name", std::string());
                    join_step_out.equijoin_keys.push_back(std::move(key_pair));
                }
            }
            join_step_out.residual_predicates.clear();
            const auto residuals_it = json_in.find("residual_predicates");
            if (residuals_it != json_in.end() && residuals_it->is_array())
            {
                for (const auto &entry : *residuals_it)
                {
                    if (entry.is_string())
                    {
                        join_step_out.residual_predicates.push_back(
                            entry.get<std::string>());
                    }
                }
            }
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
            join_step_out.outer_reorder_barrier =
                json_in.value("outer_reorder_barrier", false);
            join_step_out.semi_reorder_barrier =
                json_in.value("semi_reorder_barrier", false);
            join_step_out.anti_reorder_barrier =
                json_in.value("anti_reorder_barrier", false);
            join_step_out.using_reorder_barrier =
                json_in.value("using_reorder_barrier", false);
            join_step_out.natural_reorder_barrier =
                json_in.value("natural_reorder_barrier", false);
            join_step_out.lateral_reorder_barrier =
                json_in.value("lateral_reorder_barrier", false);
            join_step_out.parameterized_dependency =
                json_in.value("parameterized_dependency", false);
            join_step_out.parameter_dependency_relation_indexes.clear();
            const auto dependency_indexes_it =
                json_in.find("parameter_dependency_relation_indexes");
            if (dependency_indexes_it != json_in.end() &&
                dependency_indexes_it->is_array())
            {
                for (const auto &entry : *dependency_indexes_it)
                {
                    if (entry.is_number_unsigned())
                    {
                        join_step_out.parameter_dependency_relation_indexes.push_back(
                            entry.get<size_t>());
                    }
                }
            }
            join_step_out.parameter_dependency_relation_aliases.clear();
            const auto dependency_aliases_it =
                json_in.find("parameter_dependency_relation_aliases");
            if (dependency_aliases_it != json_in.end() &&
                dependency_aliases_it->is_array())
            {
                for (const auto &entry : *dependency_aliases_it)
                {
                    if (entry.is_string())
                    {
                        join_step_out.parameter_dependency_relation_aliases.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            join_step_out.has_hash_keys = json_in.value("has_hash_keys", false);
            join_step_out.has_merge_keys = json_in.value("has_merge_keys", false);
            join_step_out.merge_outer_presorted =
                json_in.value("merge_outer_presorted", false);
            join_step_out.merge_inner_presorted =
                json_in.value("merge_inner_presorted", false);
            join_step_out.merge_enabled_by_explicit_sort =
                json_in.value("merge_enabled_by_explicit_sort", false);
            join_step_out.merge_viability_source =
                json_in.value("merge_viability_source", std::string());
            join_step_out.ordered_output =
                json_in.value("ordered_output", false);
            join_step_out.order_complete =
                json_in.value("order_complete", false);
            join_step_out.ordered_prefix_length =
                json_in.value("ordered_prefix_length", 0ULL);
            join_step_out.ordering_class =
                json_in.value("ordering_class", std::string());
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
            join_step_out.parallel_eligible =
                json_in.value("parallel_eligible", false);
            join_step_out.parallel_enabled =
                json_in.value("parallel_enabled", false);
            join_step_out.parallel_workers_planned =
                json_in.value("parallel_workers_planned", 0U);
            join_step_out.parallel_stage =
                json_in.value("parallel_stage", std::string());
            join_step_out.parallel_rejection_reason =
                json_in.value("parallel_rejection_reason", std::string());
            join_step_out.parallel_distribution_mode =
                json_in.value("parallel_distribution_mode", std::string());
            join_step_out.parallel_order_preservation =
                json_in.value("parallel_order_preservation", std::string());
            join_step_out.exchange_topology_id =
                json_in.value("exchange_topology_id", std::string());
            join_step_out.gather_decision_reason =
                json_in.value("gather_decision_reason", std::string());
            join_step_out.actual_rows = json_in.value("actual_rows", 0ULL);
            join_step_out.rows_examined = json_in.value("rows_examined", 0ULL);
            join_step_out.rows_filtered = json_in.value("rows_filtered", 0ULL);
            join_step_out.loop_count = json_in.value("loop_count", 0ULL);

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
            out["parallel_aware"] = node.parallel_aware;
            out["parallel_enabled"] = node.parallel_enabled;
            out["parallel_workers_planned"] = node.parallel_workers_planned;
            out["gather_merge"] = node.gather_merge;
            out["parallel_stage"] = node.parallel_stage;
            out["parallel_reason"] = node.parallel_reason;
            out["startup_cost"] = node.startup_cost;
            out["total_cost"] = node.total_cost;
            out["estimated_rows"] = node.estimated_rows;
            out["actuals_available"] = node.actuals_available;
            out["actual_rows"] = node.actual_rows;
            out["rows_examined"] = node.rows_examined;
            out["rows_filtered"] = node.rows_filtered;
            out["loop_count"] = node.loop_count;
            out["startup_time_us"] = node.startup_time_us;
            out["execution_time_us"] = node.execution_time_us;
            out["estimated_memory_bytes"] = node.estimated_memory_bytes;
            out["memory_budget_bytes"] = node.memory_budget_bytes;
            out["spill_expected"] = node.spill_expected;
            out["spill_passes"] = node.spill_passes;
            out["spill_bytes"] = node.spill_bytes;
            out["spill_policy"] = node.spill_policy;
            out["formula_profile_id"] = node.formula_profile_id;
            out["formula_profile_version"] = node.formula_profile_version;
            out["calibration_profile_id"] = node.calibration_profile_id;
            out["storage_profile"] = node.storage_profile;
            out["workload_profile"] = node.workload_profile;
            out["resource_governance_outcome"] = node.resource_governance_outcome;
            out["input_estimates"] = nlohmann::json::array();
            for (const auto &entry : node.input_estimates)
            {
                out["input_estimates"].push_back({
                    {"name", entry.name},
                    {"value", entry.value},
                    {"unit", entry.unit},
                });
            }
            out["expanded_cost_terms"] = nlohmann::json::array();
            for (const auto &entry : node.expanded_cost_terms)
            {
                out["expanded_cost_terms"].push_back({
                    {"name", entry.name},
                    {"coefficient", entry.coefficient},
                    {"input_value", entry.input_value},
                    {"contribution", entry.contribution},
                    {"unit", entry.unit},
                });
            }
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
            node_out.parallel_aware = json_in.value("parallel_aware", false);
            node_out.parallel_enabled = json_in.value("parallel_enabled", false);
            node_out.parallel_workers_planned =
                json_in.value("parallel_workers_planned", 0U);
            node_out.gather_merge = json_in.value("gather_merge", false);
            node_out.parallel_stage =
                json_in.value("parallel_stage", std::string());
            node_out.parallel_reason =
                json_in.value("parallel_reason", std::string());
            node_out.startup_cost = json_in.value("startup_cost", 0.0);
            node_out.total_cost = json_in.value("total_cost", 0.0);
            node_out.estimated_rows = json_in.value("estimated_rows", 0ULL);
            node_out.actuals_available =
                json_in.value("actuals_available", false);
            node_out.actual_rows = json_in.value("actual_rows", 0ULL);
            node_out.rows_examined = json_in.value("rows_examined", 0ULL);
            node_out.rows_filtered = json_in.value("rows_filtered", 0ULL);
            node_out.loop_count = json_in.value("loop_count", 0ULL);
            node_out.startup_time_us = json_in.value("startup_time_us", 0ULL);
            node_out.execution_time_us =
                json_in.value("execution_time_us", 0ULL);
            node_out.estimated_memory_bytes =
                json_in.value("estimated_memory_bytes", 0ULL);
            node_out.memory_budget_bytes =
                json_in.value("memory_budget_bytes", 0ULL);
            node_out.spill_expected = json_in.value("spill_expected", false);
            node_out.spill_passes = json_in.value("spill_passes", 0U);
            node_out.spill_bytes = json_in.value("spill_bytes", 0ULL);
            node_out.spill_policy = json_in.value("spill_policy", std::string());
            node_out.formula_profile_id =
                json_in.value("formula_profile_id", std::string());
            node_out.formula_profile_version =
                json_in.value("formula_profile_version", 0U);
            node_out.calibration_profile_id =
                json_in.value("calibration_profile_id", std::string());
            node_out.storage_profile =
                json_in.value("storage_profile", std::string());
            node_out.workload_profile =
                json_in.value("workload_profile", std::string());
            node_out.resource_governance_outcome =
                json_in.value("resource_governance_outcome", std::string());

            node_out.input_estimates.clear();
            const auto input_estimates_it = json_in.find("input_estimates");
            if (input_estimates_it != json_in.end() && input_estimates_it->is_array())
            {
                for (const auto &entry : *input_estimates_it)
                {
                    if (!entry.is_object())
                    {
                        error_out =
                            "runtime plan node input estimate must be an object";
                        return false;
                    }
                    RuntimePlanCostInputEstimate parsed;
                    parsed.name = entry.value("name", std::string());
                    parsed.value = entry.value("value", 0.0);
                    parsed.unit = entry.value("unit", std::string());
                    node_out.input_estimates.push_back(std::move(parsed));
                }
            }

            node_out.expanded_cost_terms.clear();
            const auto expanded_terms_it = json_in.find("expanded_cost_terms");
            if (expanded_terms_it != json_in.end() && expanded_terms_it->is_array())
            {
                for (const auto &entry : *expanded_terms_it)
                {
                    if (!entry.is_object())
                    {
                        error_out =
                            "runtime plan node expanded cost term must be an object";
                        return false;
                    }
                    RuntimePlanCostTerm parsed;
                    parsed.name = entry.value("name", std::string());
                    parsed.coefficient = entry.value("coefficient", 0.0);
                    parsed.input_value = entry.value("input_value", 0.0);
                    parsed.contribution = entry.value("contribution", 0.0);
                    parsed.unit = entry.value("unit", std::string());
                    node_out.expanded_cost_terms.push_back(std::move(parsed));
                }
            }

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
            out["stats_snapshot_id"] = entry.stats_snapshot_id;
            out["last_analyzed_time"] = entry.last_analyzed_time;
            out["sample_ratio"] = entry.sample_ratio;
            out["modified_rows_since_analyze"] =
                entry.modified_rows_since_analyze;
            out["staleness_class"] = entry.staleness_class;
            out["confidence_class"] = entry.confidence_class;
            out["auto_analyze_applied"] = entry.auto_analyze_applied;
            out["auto_analyze_threshold"] = entry.auto_analyze_threshold;
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
            entry_out.stats_snapshot_id =
                json_in.value("stats_snapshot_id", uint64_t{0});
            entry_out.last_analyzed_time =
                json_in.value("last_analyzed_time", uint64_t{0});
            entry_out.sample_ratio = json_in.value("sample_ratio", 0.0);
            entry_out.modified_rows_since_analyze =
                json_in.value("modified_rows_since_analyze", uint64_t{0});
            entry_out.staleness_class =
                json_in.value("staleness_class", std::string());
            entry_out.confidence_class =
                json_in.value("confidence_class", std::string());
            entry_out.auto_analyze_applied =
                json_in.value("auto_analyze_applied", false);
            entry_out.auto_analyze_threshold =
                json_in.value("auto_analyze_threshold", uint64_t{0});
            return true;
        }

        auto adaptiveFeedbackToJson(const RuntimePlanAdaptiveFeedback &feedback)
            -> nlohmann::json
        {
            nlohmann::json out;
            out["available"] = feedback.available;
            out["replan_required"] = feedback.replan_required;
            out["replan_suppressed"] = feedback.replan_suppressed;
            out["stats_refresh_requested"] = feedback.stats_refresh_requested;
            out["stats_refresh_applied"] = feedback.stats_refresh_applied;
            out["calibration_bundle_proposed"] =
                feedback.calibration_bundle_proposed;
            out["correction_applied"] = feedback.correction_applied;
            out["calibration_applied"] = feedback.calibration_applied;
            out["observation_count"] = feedback.observation_count;
            out["replan_action_count"] = feedback.replan_action_count;
            out["last_estimated_rows"] = feedback.last_estimated_rows;
            out["last_actual_rows"] = feedback.last_actual_rows;
            out["estimation_error_ratio"] = feedback.estimation_error_ratio;
            out["correction_factor"] = feedback.correction_factor;
            out["cost_reweight_factor"] = feedback.cost_reweight_factor;
            out["calibration_store_contract_id"] =
                feedback.calibration_store_contract_id;
            out["calibration_store_state"] = feedback.calibration_store_state;
            out["calibration_fail_closed"] =
                feedback.calibration_fail_closed;
            out["calibration_profile_version"] =
                feedback.calibration_profile_version;
            out["last_plan_hash"] = feedback.last_plan_hash;
            out["calibration_profile_id"] = feedback.calibration_profile_id;
            out["calibration_profile_delta_id"] =
                feedback.calibration_profile_delta_id;
            out["calibration_evidence_id"] =
                feedback.calibration_evidence_id;
            out["guardrail_reason"] = feedback.guardrail_reason;
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
            feedback_out.replan_suppressed =
                json_in.value("replan_suppressed", false);
            feedback_out.stats_refresh_requested =
                json_in.value("stats_refresh_requested", false);
            feedback_out.stats_refresh_applied =
                json_in.value("stats_refresh_applied", false);
            feedback_out.calibration_bundle_proposed =
                json_in.value("calibration_bundle_proposed", false);
            feedback_out.correction_applied =
                json_in.value("correction_applied", false);
            feedback_out.calibration_applied =
                json_in.value("calibration_applied", false);
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
            feedback_out.cost_reweight_factor =
                json_in.value("cost_reweight_factor", 1.0);
            feedback_out.calibration_store_contract_id =
                json_in.value("calibration_store_contract_id",
                              std::string(
                                  kAdaptiveFeedbackCalibrationStoreContractId));
            feedback_out.calibration_store_state =
                json_in.value("calibration_store_state",
                              std::string("EMPTY"));
            feedback_out.calibration_fail_closed =
                json_in.value("calibration_fail_closed", true);
            feedback_out.calibration_profile_version =
                json_in.value("calibration_profile_version", 0U);
            feedback_out.last_plan_hash =
                json_in.value("last_plan_hash", std::string());
            feedback_out.calibration_profile_id =
                json_in.value("calibration_profile_id", std::string());
            feedback_out.calibration_profile_delta_id =
                json_in.value("calibration_profile_delta_id", std::string());
            feedback_out.calibration_evidence_id =
                json_in.value("calibration_evidence_id", std::string());
            feedback_out.guardrail_reason =
                json_in.value("guardrail_reason", std::string());
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

        auto advisorSignalToJson(const RuntimePlanAdvisorSignal &signal)
            -> nlohmann::json
        {
            nlohmann::json out;
            out["signal_name"] = signal.signal_name;
            out["severity"] = signal.severity;
            out["provenance_source"] = signal.provenance_source;
            out["detail"] = signal.detail;
            return out;
        }

        auto advisorSignalFromJson(const nlohmann::json &json_in,
                                   RuntimePlanAdvisorSignal &signal_out,
                                   std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan advisor signal must be an object";
                return false;
            }

            signal_out.signal_name =
                json_in.value("signal_name", std::string());
            signal_out.severity = json_in.value("severity", std::string());
            signal_out.provenance_source =
                json_in.value("provenance_source", std::string());
            signal_out.detail = json_in.value("detail", std::string());
            return true;
        }

        auto advisorRecommendationToJson(
            const RuntimePlanAdvisorRecommendation &recommendation)
            -> nlohmann::json
        {
            nlohmann::json out;
            out["rank"] = recommendation.rank;
            out["recommendation_type"] = recommendation.recommendation_type;
            out["table_name"] = recommendation.table_name;
            out["index_name"] = recommendation.index_name;
            out["column_names"] = recommendation.column_names;
            out["create_sql"] = recommendation.create_sql;
            out["drop_sql"] = recommendation.drop_sql;
            out["reason"] = recommendation.reason;
            out["provenance_source"] = recommendation.provenance_source;
            out["query_fingerprint"] = recommendation.query_fingerprint;
            out["signal_names"] = recommendation.signal_names;
            out["benefit_score"] = recommendation.benefit_score;
            out["cost_score"] = recommendation.cost_score;
            out["net_benefit"] = recommendation.net_benefit;
            out["affected_queries"] = recommendation.affected_queries;
            out["estimated_size_mb"] = recommendation.estimated_size_mb;
            out["estimated_speedup"] = recommendation.estimated_speedup;
            out["priority"] = recommendation.priority;
            out["confidence"] = recommendation.confidence;
            out["what_if_replanned"] = recommendation.what_if_replanned;
            out["baseline_access_family"] =
                recommendation.baseline_access_family;
            out["baseline_index_name"] = recommendation.baseline_index_name;
            out["baseline_total_cost"] = recommendation.baseline_total_cost;
            out["baseline_estimated_rows"] =
                recommendation.baseline_estimated_rows;
            out["hypothetical_access_family"] =
                recommendation.hypothetical_access_family;
            out["hypothetical_index_name"] =
                recommendation.hypothetical_index_name;
            out["hypothetical_total_cost"] =
                recommendation.hypothetical_total_cost;
            out["hypothetical_estimated_rows"] =
                recommendation.hypothetical_estimated_rows;
            out["estimated_cost_delta"] = recommendation.estimated_cost_delta;
            out["estimated_speedup_ratio"] =
                recommendation.estimated_speedup_ratio;
            out["ordering_improved"] = recommendation.ordering_improved;
            out["covering_improved"] = recommendation.covering_improved;
            out["evidence_detail"] = recommendation.evidence_detail;
            return out;
        }

        auto advisorRecommendationFromJson(
            const nlohmann::json &json_in,
            RuntimePlanAdvisorRecommendation &recommendation_out,
            std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out =
                    "runtime plan advisor recommendation must be an object";
                return false;
            }

            recommendation_out.rank = json_in.value("rank", 0U);
            recommendation_out.recommendation_type =
                json_in.value("recommendation_type", std::string());
            recommendation_out.table_name =
                json_in.value("table_name", std::string());
            recommendation_out.index_name =
                json_in.value("index_name", std::string());
            recommendation_out.column_names.clear();
            const auto column_names_it = json_in.find("column_names");
            if (column_names_it != json_in.end() && column_names_it->is_array())
            {
                for (const auto &entry : *column_names_it)
                {
                    if (entry.is_string())
                    {
                        recommendation_out.column_names.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            recommendation_out.create_sql =
                json_in.value("create_sql", std::string());
            recommendation_out.drop_sql =
                json_in.value("drop_sql", std::string());
            recommendation_out.reason =
                json_in.value("reason", std::string());
            recommendation_out.provenance_source =
                json_in.value("provenance_source", std::string());
            recommendation_out.query_fingerprint =
                json_in.value("query_fingerprint", std::string());
            recommendation_out.signal_names.clear();
            const auto signal_names_it = json_in.find("signal_names");
            if (signal_names_it != json_in.end() && signal_names_it->is_array())
            {
                for (const auto &entry : *signal_names_it)
                {
                    if (entry.is_string())
                    {
                        recommendation_out.signal_names.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            recommendation_out.benefit_score =
                json_in.value("benefit_score", 0.0);
            recommendation_out.cost_score = json_in.value("cost_score", 0.0);
            recommendation_out.net_benefit =
                json_in.value("net_benefit", 0.0);
            recommendation_out.affected_queries =
                json_in.value("affected_queries", 0ULL);
            recommendation_out.estimated_size_mb =
                json_in.value("estimated_size_mb", 0.0);
            recommendation_out.estimated_speedup =
                json_in.value("estimated_speedup", 0.0);
            recommendation_out.priority = json_in.value("priority", 0.0);
            recommendation_out.confidence = json_in.value("confidence", 0.0);
            recommendation_out.what_if_replanned =
                json_in.value("what_if_replanned", false);
            recommendation_out.baseline_access_family =
                json_in.value("baseline_access_family", std::string());
            recommendation_out.baseline_index_name =
                json_in.value("baseline_index_name", std::string());
            recommendation_out.baseline_total_cost =
                json_in.value("baseline_total_cost", 0.0);
            recommendation_out.baseline_estimated_rows =
                json_in.value("baseline_estimated_rows", 0ULL);
            recommendation_out.hypothetical_access_family =
                json_in.value("hypothetical_access_family", std::string());
            recommendation_out.hypothetical_index_name =
                json_in.value("hypothetical_index_name", std::string());
            recommendation_out.hypothetical_total_cost =
                json_in.value("hypothetical_total_cost", 0.0);
            recommendation_out.hypothetical_estimated_rows =
                json_in.value("hypothetical_estimated_rows", 0ULL);
            recommendation_out.estimated_cost_delta =
                json_in.value("estimated_cost_delta", 0.0);
            recommendation_out.estimated_speedup_ratio =
                json_in.value("estimated_speedup_ratio", 1.0);
            recommendation_out.ordering_improved =
                json_in.value("ordering_improved", false);
            recommendation_out.covering_improved =
                json_in.value("covering_improved", false);
            recommendation_out.evidence_detail =
                json_in.value("evidence_detail", std::string());
            return true;
        }

        auto searchSummaryToJson(const RuntimePlanSearchSummary &summary)
            -> nlohmann::json
        {
            nlohmann::json out;
            out["requested_strategy"] = summary.requested_strategy;
            out["selected_strategy"] = summary.selected_strategy;
            out["search_budget"] = summary.search_budget;
            out["considered_state_count"] = summary.considered_state_count;
            out["pruned_state_count"] = summary.pruned_state_count;
            out["pair_evaluation_count"] = summary.pair_evaluation_count;
            out["retained_frontier_entry_count"] =
                summary.retained_frontier_entry_count;
            out["dominated_state_count"] = summary.dominated_state_count;
            out["max_frontier_width"] = summary.max_frontier_width;
            out["rejected_candidate_count"] = summary.rejected_candidate_count;
            out["max_pair_evaluations"] = summary.max_pair_evaluations;
            out["max_states_considered"] = summary.max_states_considered;
            out["exhaustive_join_limit"] = summary.exhaustive_join_limit;
            out["bounded_dp_join_limit"] = summary.bounded_dp_join_limit;
            out["fallback_prune_level"] = summary.fallback_prune_level;
            out["fallback_reason"] = summary.fallback_reason;
            out["fallback_threshold_name"] = summary.fallback_threshold_name;
            out["fallback_threshold_value"] = summary.fallback_threshold_value;
            return out;
        }

        auto searchSummaryFromJson(const nlohmann::json &json_in,
                                   RuntimePlanSearchSummary &summary_out,
                                   std::string &error_out) -> bool
        {
            if (!json_in.is_object())
            {
                error_out = "runtime plan search summary entry must be an object";
                return false;
            }

            summary_out.requested_strategy =
                json_in.value("requested_strategy", std::string());
            summary_out.selected_strategy =
                json_in.value("selected_strategy", std::string());
            summary_out.search_budget = json_in.value("search_budget", 0ULL);
            summary_out.considered_state_count =
                json_in.value("considered_state_count", 0ULL);
            summary_out.pruned_state_count =
                json_in.value("pruned_state_count", 0ULL);
            summary_out.pair_evaluation_count =
                json_in.value("pair_evaluation_count", 0ULL);
            summary_out.retained_frontier_entry_count =
                json_in.value("retained_frontier_entry_count", 0ULL);
            summary_out.dominated_state_count =
                json_in.value("dominated_state_count", 0ULL);
            summary_out.max_frontier_width =
                json_in.value("max_frontier_width", 0ULL);
            summary_out.rejected_candidate_count =
                json_in.value("rejected_candidate_count", 0ULL);
            summary_out.max_pair_evaluations =
                json_in.value("max_pair_evaluations", 0ULL);
            summary_out.max_states_considered =
                json_in.value("max_states_considered", 0ULL);
            summary_out.exhaustive_join_limit =
                json_in.value("exhaustive_join_limit", 0ULL);
            summary_out.bounded_dp_join_limit =
                json_in.value("bounded_dp_join_limit", 0ULL);
            summary_out.fallback_prune_level =
                json_in.value("fallback_prune_level", 0ULL);
            summary_out.fallback_reason =
                json_in.value("fallback_reason", std::string());
            summary_out.fallback_threshold_name =
                json_in.value("fallback_threshold_name", std::string());
            summary_out.fallback_threshold_value =
                json_in.value("fallback_threshold_value", 0ULL);
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
            root["contract_id"] = plan.contract_id;
            root["join_graph_contract_id"] = plan.join_graph_contract_id;
            root["join_search_contract_id"] = plan.join_search_contract_id;
            root["join_search_property_signature_contract_id"] =
                plan.join_search_property_signature_contract_id;
            root["join_search_frontier_mode"] =
                plan.join_search_frontier_mode;
            root["join_search_mode_source"] =
                plan.join_search_mode_source;
            root["base_candidate_bundle_contract_id"] =
                plan.base_candidate_bundle_contract_id;
            root["base_candidate_bundle_owner_pass_id"] =
                plan.base_candidate_bundle_owner_pass_id;
            root["base_candidate_bundle_consumer_pass_id"] =
                plan.base_candidate_bundle_consumer_pass_id;
            root["base_candidate_bundle_frozen"] =
                plan.base_candidate_bundle_frozen;
            root["base_candidate_bundle_rejection_count"] =
                plan.base_candidate_bundle_rejection_count;
            root["planner_front_door_contract_id"] =
                plan.planner_front_door_contract_id;
            root["diagnostics_contract_id"] = plan.diagnostics_contract_id;
            root["planner_status_code"] = plan.planner_status_code;
            root["plan_hash"] = plan.plan_hash;
            root["explain_text"] = plan.explain_text;
            root["normalized_request_digest"] =
                plan.normalized_request_digest;
            root["normalized_statement_id"] = plan.normalized_statement_id;
            root["statement_kind"] = plan.statement_kind;
            root["cache_mode"] = plan.cache_mode;
            root["chosen_reuse_mode"] = plan.chosen_reuse_mode;
            root["plan_profile_signature"] = plan.plan_profile_signature;
            root["index_family_signature"] = plan.index_family_signature;
            root["family_statistics_signature"] =
                plan.family_statistics_signature;
            root["selectivity_bucket_signature"] =
                plan.selectivity_bucket_signature;
            root["query_feedback_key"] = plan.query_feedback_key;
            root["storage_layer_shape"] = plan.storage_layer_shape;
            root["publication_state_summary"] =
                plan.publication_state_summary;
            root["collector_specialization_id"] =
                plan.collector_specialization_id;
            root["execution_intent_class"] =
                plan.execution_intent_class;
            root["continuation_token_contract"] =
                plan.continuation_token_contract;
            root["rewrite_before_search_contract_id"] =
                plan.rewrite_before_search_contract_id;
            root["rewrite_before_search_owner_pass_id"] =
                plan.rewrite_before_search_owner_pass_id;
            root["rewrite_before_search_terminal_pass_id"] =
                plan.rewrite_before_search_terminal_pass_id;
            root["rewrite_before_search_frozen"] =
                plan.rewrite_before_search_frozen;
            root["tagging_contract_id"] = plan.tagging_contract_id;
            root["tagging_owner_pass_id"] = plan.tagging_owner_pass_id;
            root["join_search_owner_pass_id"] =
                plan.join_search_owner_pass_id;
            root["result_shape_finalize_pass_id"] =
                plan.result_shape_finalize_pass_id;
            root["proof_surface_contract_id"] =
                plan.proof_surface_contract_id;
            root["proof_surface_complete"] =
                plan.proof_surface_complete;
            root["proof_surface_claim_count"] =
                plan.proof_surface_claim_count;
            root["proof_surface_json"] =
                plan.proof_surface_json;
            root["diagnostics_payload_json"] =
                plan.diagnostics_payload_json;
            root["parameter_sensitive"] = plan.parameter_sensitive;
            root["join_search_base_candidate_count"] =
                plan.join_search_base_candidate_count;
            root["invalidation_dependencies"] =
                plan.invalidation_dependencies;
            root["compatibility_version_identifiers"] =
                plan.compatibility_version_identifiers;
            root["fallback_and_rejection_stream"] =
                plan.fallback_and_rejection_stream;
            root["search_summary"] = searchSummaryToJson(plan.search_summary);
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
            root["advisor_signals"] = nlohmann::json::array();
            for (const auto &entry : plan.advisor_signals)
            {
                root["advisor_signals"].push_back(advisorSignalToJson(entry));
            }
            root["advisor_recommendations"] = nlohmann::json::array();
            for (const auto &entry : plan.advisor_recommendations)
            {
                root["advisor_recommendations"].push_back(
                    advisorRecommendationToJson(entry));
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
            plan_out.contract_id =
                root.value("contract_id", std::string(kRuntimePlanContractId));
            plan_out.join_graph_contract_id =
                root.value("join_graph_contract_id",
                           std::string(kJoinGraphContractId));
            plan_out.join_search_contract_id =
                root.value("join_search_contract_id",
                           std::string(kJoinSearchContractId));
            plan_out.join_search_property_signature_contract_id =
                root.value("join_search_property_signature_contract_id",
                           std::string(kJoinSearchPropertySignatureContractId));
            plan_out.join_search_frontier_mode =
                root.value("join_search_frontier_mode",
                           std::string(kJoinSearchFrontierMode));
            plan_out.join_search_mode_source =
                root.value("join_search_mode_source", std::string());
            plan_out.base_candidate_bundle_contract_id =
                root.value("base_candidate_bundle_contract_id", std::string());
            plan_out.base_candidate_bundle_owner_pass_id =
                root.value("base_candidate_bundle_owner_pass_id", std::string());
            plan_out.base_candidate_bundle_consumer_pass_id =
                root.value("base_candidate_bundle_consumer_pass_id",
                           std::string());
            plan_out.base_candidate_bundle_frozen =
                root.value("base_candidate_bundle_frozen", false);
            plan_out.base_candidate_bundle_rejection_count =
                root.value("base_candidate_bundle_rejection_count", 0ULL);
            plan_out.planner_front_door_contract_id =
                root.value("planner_front_door_contract_id",
                           std::string(kPlannerFrontDoorContractId));
            plan_out.diagnostics_contract_id =
                root.value("diagnostics_contract_id",
                           std::string(kOptimizerDiagnosticsContractId));
            plan_out.planner_status_code =
                root.value("planner_status_code", 0U);
            plan_out.plan_hash = root.value("plan_hash", std::string());
            plan_out.explain_text = root.value("explain_text", std::string());
            plan_out.normalized_request_digest =
                root.value("normalized_request_digest", std::string());
            plan_out.normalized_statement_id =
                root.value("normalized_statement_id", std::string());
            plan_out.statement_kind =
                root.value("statement_kind", std::string());
            plan_out.cache_mode = root.value("cache_mode", std::string());
            plan_out.chosen_reuse_mode =
                root.value("chosen_reuse_mode", plan_out.cache_mode);
            plan_out.plan_profile_signature =
                root.value("plan_profile_signature", std::string());
            plan_out.index_family_signature =
                root.value("index_family_signature", std::string());
            plan_out.family_statistics_signature =
                root.value("family_statistics_signature", std::string());
            plan_out.selectivity_bucket_signature =
                root.value("selectivity_bucket_signature", std::string());
            plan_out.query_feedback_key =
                root.value("query_feedback_key", std::string());
            plan_out.storage_layer_shape =
                root.value("storage_layer_shape", std::string("ROW_STORE_MGA"));
            plan_out.publication_state_summary =
                root.value("publication_state_summary", std::string());
            plan_out.collector_specialization_id =
                root.value("collector_specialization_id", std::string());
            plan_out.execution_intent_class =
                root.value("execution_intent_class", std::string());
            plan_out.continuation_token_contract =
                root.value("continuation_token_contract", std::string());
            plan_out.rewrite_before_search_contract_id =
                root.value("rewrite_before_search_contract_id",
                           std::string(kRewriteBeforeSearchContractId));
            plan_out.rewrite_before_search_owner_pass_id =
                root.value("rewrite_before_search_owner_pass_id",
                           std::string("P01_SEMANTIC_NORMALIZE"));
            plan_out.rewrite_before_search_terminal_pass_id =
                root.value("rewrite_before_search_terminal_pass_id",
                           std::string("P07_FILTER_PUSH_DOWN"));
            plan_out.rewrite_before_search_frozen =
                root.value("rewrite_before_search_frozen", false);
            plan_out.tagging_contract_id =
                root.value("tagging_contract_id",
                           std::string(kAccessPathTaggingContractId));
            plan_out.tagging_owner_pass_id =
                root.value("tagging_owner_pass_id",
                           std::string("P08_ACCESS_PATH_ANNOTATE"));
            plan_out.join_search_owner_pass_id =
                root.value("join_search_owner_pass_id",
                           std::string("P09_JOIN_ORDER_PLAN"));
            plan_out.result_shape_finalize_pass_id =
                root.value("result_shape_finalize_pass_id",
                           std::string("P10_RESULT_SHAPE_FINALIZE"));
            plan_out.proof_surface_contract_id =
                root.value("proof_surface_contract_id",
                           std::string(kOptimizerProofSurfaceContractId));
            plan_out.proof_surface_complete =
                root.value("proof_surface_complete", false);
            plan_out.proof_surface_claim_count =
                root.value("proof_surface_claim_count", 0U);
            plan_out.proof_surface_json =
                root.value("proof_surface_json", std::string());
            plan_out.diagnostics_payload_json =
                root.value("diagnostics_payload_json", std::string());
            plan_out.parameter_sensitive =
                root.value("parameter_sensitive", false);
            plan_out.join_search_base_candidate_count =
                root.value("join_search_base_candidate_count", 0ULL);
            plan_out.invalidation_dependencies.clear();
            const auto invalidation_dependencies_it =
                root.find("invalidation_dependencies");
            if (invalidation_dependencies_it != root.end() &&
                invalidation_dependencies_it->is_array())
            {
                for (const auto &entry : *invalidation_dependencies_it)
                {
                    if (entry.is_string())
                    {
                        plan_out.invalidation_dependencies.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            plan_out.compatibility_version_identifiers.clear();
            const auto compatibility_versions_it =
                root.find("compatibility_version_identifiers");
            if (compatibility_versions_it != root.end() &&
                compatibility_versions_it->is_array())
            {
                for (const auto &entry : *compatibility_versions_it)
                {
                    if (entry.is_string())
                    {
                        plan_out.compatibility_version_identifiers.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            if (plan_out.compatibility_version_identifiers.empty())
            {
                plan_out.compatibility_version_identifiers = {
                    plan_out.planner_front_door_contract_id,
                    plan_out.contract_id,
                    plan_out.join_graph_contract_id,
                    plan_out.diagnostics_contract_id,
                };
            }
            plan_out.fallback_and_rejection_stream.clear();
            const auto fallback_stream_it =
                root.find("fallback_and_rejection_stream");
            if (fallback_stream_it != root.end() &&
                fallback_stream_it->is_array())
            {
                for (const auto &entry : *fallback_stream_it)
                {
                    if (entry.is_string())
                    {
                        plan_out.fallback_and_rejection_stream.push_back(
                            entry.get<std::string>());
                    }
                }
            }
            const auto search_summary_it = root.find("search_summary");
            if (search_summary_it != root.end())
            {
                if (!searchSummaryFromJson(*search_summary_it,
                                           plan_out.search_summary,
                                           error_out))
                {
                    return false;
                }
            }
            else
            {
                plan_out.search_summary = RuntimePlanSearchSummary{};
            }

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

            plan_out.advisor_signals.clear();
            const auto advisor_signals_it = root.find("advisor_signals");
            if (advisor_signals_it != root.end() && advisor_signals_it->is_array())
            {
                for (const auto &entry : *advisor_signals_it)
                {
                    RuntimePlanAdvisorSignal signal_entry;
                    if (!advisorSignalFromJson(entry, signal_entry, error_out))
                    {
                        return false;
                    }
                    plan_out.advisor_signals.push_back(std::move(signal_entry));
                }
            }

            plan_out.advisor_recommendations.clear();
            const auto advisor_recommendations_it =
                root.find("advisor_recommendations");
            if (advisor_recommendations_it != root.end() &&
                advisor_recommendations_it->is_array())
            {
                for (const auto &entry : *advisor_recommendations_it)
                {
                    RuntimePlanAdvisorRecommendation recommendation_entry;
                    if (!advisorRecommendationFromJson(entry,
                                                       recommendation_entry,
                                                       error_out))
                    {
                        return false;
                    }
                    plan_out.advisor_recommendations.push_back(
                        std::move(recommendation_entry));
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

    auto adaptiveFeedbackPlanHash(const RuntimePlan &plan) -> std::string
    {
        std::ostringstream seed;
        for (const auto &relation : plan.relations)
        {
            seed << relation.source_relation_index << ':'
                 << (relation.scan_family.empty()
                         ? relation.scan_kind
                         : relation.scan_family) << ':'
                 << accessPathExactnessClassName(relation.exactness_class) << ':'
                 << (relation.requires_recheck ? 1 : 0) << ':'
                 << relation.index_name << ':'
                 << relation.bitmap_op << ':'
                 << (relation.covering_index ? 1 : 0) << ':'
                 << (relation.exact_key_lookup ? 1 : 0) << ':'
                 << (relation.ordered_output ? 1 : 0) << ':'
                 << relation.ordered_prefix_length << ':'
                 << (relation.parallel_eligible ? 1 : 0) << ':'
                 << (relation.parallel_enabled ? 1 : 0) << ':'
                 << relation.parallel_workers_planned << ':'
                 << relation.parallel_stage << ':'
                 << relation.parallel_distribution_mode << ':'
                 << relation.parallel_order_preservation << ':'
                 << relation.exchange_topology_id << ':'
                 << relation.gather_decision_reason << '|';
            for (size_t outer_index : relation.required_outer_relation_indexes)
            {
                seed << outer_index << ';';
            }
            seed << '|';
        }

        for (const auto &join : plan.join_steps)
        {
            seed << join.source_join_index << ':'
                << join.right_relation_index << ':'
                << join.join_edge_left_relation_index << ':'
                << join.join_edge_right_relation_index << ':'
                << join.join_type << ':'
                << join.method << ':'
                 << (join.has_hash_keys ? 1 : 0) << ':'
                 << join.left_hash_key.qualifier << ':'
                 << join.left_hash_key.column_name << ':'
                 << join.right_hash_key.qualifier << ':'
                 << join.right_hash_key.column_name << ':'
                 << (join.has_merge_keys ? 1 : 0) << ':'
                 << join.left_merge_key.qualifier << ':'
                 << join.left_merge_key.column_name << ':'
                 << join.right_merge_key.qualifier << ':'
                 << join.right_merge_key.column_name << ':'
                 << (join.merge_outer_presorted ? 1 : 0) << ':'
                 << (join.merge_inner_presorted ? 1 : 0) << ':'
                 << (join.merge_enabled_by_explicit_sort ? 1 : 0) << ':'
                 << join.merge_viability_source << ':'
                 << (join.ordered_output ? 1 : 0) << ':'
                 << (join.order_complete ? 1 : 0) << ':'
                 << join.ordered_prefix_length << ':'
                 << join.ordering_class << ':'
                 << (join.parallel_eligible ? 1 : 0) << ':'
                 << (join.parallel_enabled ? 1 : 0) << ':'
                 << join.parallel_workers_planned << ':'
                 << join.parallel_stage << ':'
                 << join.parallel_distribution_mode << ':'
                 << join.parallel_order_preservation << ':'
                 << join.exchange_topology_id << ':'
                 << join.gather_decision_reason << ':'
                 << join.parallel_rejection_reason << '|';
            for (const auto &key_pair : join.equijoin_keys)
            {
                seed << key_pair.left_qualifier << '.'
                     << key_pair.left_column_name << '='
                     << key_pair.right_qualifier << '.'
                     << key_pair.right_column_name << ';';
            }
            seed << '|';
        }

        appendNodeIdentitySeed(seed, plan.root);
        return std::to_string(scratchbird::sblr::v3::stableHash64(seed.str()));
    }

} // namespace scratchbird::optimizer
