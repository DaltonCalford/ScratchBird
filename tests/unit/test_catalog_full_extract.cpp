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

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"

namespace core = scratchbird::core;

namespace {

struct PageGetterEntry {
    const char* getter_name;
    const char* page_var_name;
    uint32_t (core::CatalogManager::*getter)() const;
};

static const PageGetterEntry kPageGetters[] = {
    {"databaseTablePage", "database_table_page_", &core::CatalogManager::databaseTablePage},
    {"objectTablePage", "object_table_page_", &core::CatalogManager::objectTablePage},
    {"objectNameTablePage", "object_name_table_page_", &core::CatalogManager::objectNameTablePage},
    {"domainsTablePage", "domains_table_page_", &core::CatalogManager::domainsTablePage},
    {"typeTablePage", "type_table_page_", &core::CatalogManager::typeTablePage},
    {"typeModifiersTablePage", "type_modifiers_table_page_", &core::CatalogManager::typeModifiersTablePage},
    {"typeIoTablePage", "type_io_table_page_", &core::CatalogManager::typeIoTablePage},
    {"typeCastsTablePage", "type_casts_table_page_", &core::CatalogManager::typeCastsTablePage},
    {"typeTransformsTablePage", "type_transforms_table_page_", &core::CatalogManager::typeTransformsTablePage},
    {"encodingConversionsTablePage", "encoding_conversions_table_page_", &core::CatalogManager::encodingConversionsTablePage},
    {"domainParamKeysTablePage", "domain_param_keys_table_page_", &core::CatalogManager::domainParamKeysTablePage},
    {"domainParametersTablePage", "domain_parameters_table_page_", &core::CatalogManager::domainParametersTablePage},
    {"domainConstraintsTablePage", "domain_constraints_table_page_", &core::CatalogManager::domainConstraintsTablePage},
    {"domainSecurityTablePage", "domain_security_table_page_", &core::CatalogManager::domainSecurityTablePage},
    {"domainValidationTablePage", "domain_validation_table_page_", &core::CatalogManager::domainValidationTablePage},
    {"domainIntegrityTablePage", "domain_integrity_table_page_", &core::CatalogManager::domainIntegrityTablePage},
    {"charsetAliasesTablePage", "charset_aliases_table_page_", &core::CatalogManager::charsetAliasesTablePage},
    {"collationTailoringTablePage", "collation_tailoring_table_page_", &core::CatalogManager::collationTailoringTablePage},
    {"resourceBundlesTablePage", "resource_bundles_table_page_", &core::CatalogManager::resourceBundlesTablePage},
    {"resourceArtifactsTablePage", "resource_artifacts_table_page_", &core::CatalogManager::resourceArtifactsTablePage},
    {"timezoneTransitionsTablePage", "timezone_transitions_table_page_", &core::CatalogManager::timezoneTransitionsTablePage},
    {"timezoneLeapSecondsTablePage", "timezone_leap_seconds_table_page_", &core::CatalogManager::timezoneLeapSecondsTablePage},
    {"reservedWordsTablePage", "reserved_words_table_page_", &core::CatalogManager::reservedWordsTablePage},
    {"emulationProfileTablePage", "emulation_profile_table_page_", &core::CatalogManager::emulationProfileTablePage},
    {"parserProfilesTablePage", "parser_profiles_table_page_", &core::CatalogManager::parserProfilesTablePage},
    {"parserCapabilityEntriesTablePage", "parser_capability_entries_table_page_", &core::CatalogManager::parserCapabilityEntriesTablePage},
    {"parserTransformEntriesTablePage", "parser_transform_entries_table_page_", &core::CatalogManager::parserTransformEntriesTablePage},
    {"parserErrorMapEntriesTablePage", "parser_error_map_entries_table_page_", &core::CatalogManager::parserErrorMapEntriesTablePage},
    {"parserFeaturePrecedenceTablePage", "parser_feature_precedence_table_page_", &core::CatalogManager::parserFeaturePrecedenceTablePage},
    {"partitionedTablesTablePage", "partitioned_tables_table_page_", &core::CatalogManager::partitionedTablesTablePage},
    {"partitionsTablePage", "partitions_table_page_", &core::CatalogManager::partitionsTablePage},
    {"tableInheritanceTablePage", "table_inheritance_table_page_", &core::CatalogManager::tableInheritanceTablePage},
    {"languagesTablePage", "languages_table_page_", &core::CatalogManager::languagesTablePage},
    {"eventsTablePage", "events_table_page_", &core::CatalogManager::eventsTablePage},
    {"packageMembersTablePage", "package_members_table_page_", &core::CatalogManager::packageMembersTablePage},
    {"indexColumnsTablePage", "index_columns_table_page_", &core::CatalogManager::indexColumnsTablePage},
    {"indexOpclassTablePage", "index_opclass_table_page_", &core::CatalogManager::indexOpclassTablePage},
    {"indexOpclassFunctionTablePage", "index_opclass_fn_table_page_", &core::CatalogManager::indexOpclassFunctionTablePage},
    {"indexOptionsTablePage", "index_options_table_page_", &core::CatalogManager::indexOptionsTablePage},
    {"indexAccessMethodsTablePage", "index_access_methods_table_page_", &core::CatalogManager::indexAccessMethodsTablePage},
    {"indexMaintenanceTablePage", "index_maintenance_table_page_", &core::CatalogManager::indexMaintenanceTablePage},
    {"indexMaintenanceDeltasTablePage", "index_maintenance_deltas_table_page_", &core::CatalogManager::indexMaintenanceDeltasTablePage},
    {"indexBuildDeltasTablePage", "index_build_deltas_table_page_", &core::CatalogManager::indexBuildDeltasTablePage},
    {"indexStatsTablePage", "index_stats_table_page_", &core::CatalogManager::indexStatsTablePage},
    {"indexUsageTablePage", "index_usage_table_page_", &core::CatalogManager::indexUsageTablePage},
    {"indexContentionTablePage", "index_contention_table_page_", &core::CatalogManager::indexContentionTablePage},
    {"indexStorageTablePage", "index_storage_table_page_", &core::CatalogManager::indexStorageTablePage},
    {"indexHealthTablePage", "index_health_table_page_", &core::CatalogManager::indexHealthTablePage},
    {"filespaceStatsTablePage", "filespace_stats_table_page_", &core::CatalogManager::filespaceStatsTablePage},
    {"lobTablePage", "lob_table_page_", &core::CatalogManager::lobTablePage},
    {"lobPageTablePage", "lob_page_table_page_", &core::CatalogManager::lobPageTablePage},
    {"backupHistoryTablePage", "backup_history_table_page_", &core::CatalogManager::backupHistoryTablePage},
    {"connectionTablePage", "connection_table_page_", &core::CatalogManager::connectionTablePage},
    {"transactionTablePage", "transaction_table_page_", &core::CatalogManager::transactionTablePage},
    {"authMappingTablePage", "auth_mapping_table_page_", &core::CatalogManager::authMappingTablePage},
    {"authProviderTablePage", "auth_provider_table_page_", &core::CatalogManager::authProviderTablePage},
    {"authPolicyTablePage", "auth_policy_table_page_", &core::CatalogManager::authPolicyTablePage},
    {"authAttemptLogTablePage", "auth_attempt_log_table_page_", &core::CatalogManager::authAttemptLogTablePage},
    {"connectionRuleTablePage", "connection_rule_table_page_", &core::CatalogManager::connectionRuleTablePage},
    {"connectionRuleEpochTablePage", "connection_rule_epoch_table_page_", &core::CatalogManager::connectionRuleEpochTablePage},
    {"roleSettingTablePage", "role_setting_table_page_", &core::CatalogManager::roleSettingTablePage},
    {"securityLabelTablePage", "security_label_table_page_", &core::CatalogManager::securityLabelTablePage},
    {"securityClassTablePage", "security_class_table_page_", &core::CatalogManager::securityClassTablePage},
    {"certRegistryTablePage", "cert_registry_table_page_", &core::CatalogManager::certRegistryTablePage},
    {"privateKeyStoreTablePage", "private_key_store_table_page_", &core::CatalogManager::privateKeyStoreTablePage},
    {"trustAnchorTablePage", "trust_anchor_table_page_", &core::CatalogManager::trustAnchorTablePage},
    {"channelCertBindingTablePage", "channel_cert_binding_table_page_", &core::CatalogManager::channelCertBindingTablePage},
    {"certRevocationTablePage", "cert_revocation_table_page_", &core::CatalogManager::certRevocationTablePage},
    {"pkiDistributionStateTablePage", "pki_distribution_state_table_page_", &core::CatalogManager::pkiDistributionStateTablePage},
    {"trustAnchorRolloverTablePage", "trust_anchor_rollover_table_page_", &core::CatalogManager::trustAnchorRolloverTablePage},
    {"encryptionProfileTablePage", "encryption_profile_table_page_", &core::CatalogManager::encryptionProfileTablePage},
    {"encryptionKeyTablePage", "encryption_key_table_page_", &core::CatalogManager::encryptionKeyTablePage},
    {"encryptionKeyShardTablePage", "encryption_key_shard_table_page_", &core::CatalogManager::encryptionKeyShardTablePage},
    {"encryptionBootstrapInfoTablePage", "encryption_bootstrap_info_table_page_", &core::CatalogManager::encryptionBootstrapInfoTablePage},
    {"nodeTablePage", "node_table_page_", &core::CatalogManager::nodeTablePage},
    {"nodeRoleBindingTablePage", "node_role_binding_table_page_", &core::CatalogManager::nodeRoleBindingTablePage},
    {"nodeServiceTablePage", "node_service_table_page_", &core::CatalogManager::nodeServiceTablePage},
    {"nodeCapabilityTablePage", "node_capability_table_page_", &core::CatalogManager::nodeCapabilityTablePage},
    {"clockPolicyTablePage", "clock_policy_table_page_", &core::CatalogManager::clockPolicyTablePage},
    {"clockSourceTablePage", "clock_source_table_page_", &core::CatalogManager::clockSourceTablePage},
    {"nodeClockStateTablePage", "node_clock_state_table_page_", &core::CatalogManager::nodeClockStateTablePage},
    {"clockViolationEventTablePage", "clock_violation_event_table_page_", &core::CatalogManager::clockViolationEventTablePage},
    {"clusterTablePage", "cluster_table_page_", &core::CatalogManager::clusterTablePage},
    {"shardPolicyTablePage", "shard_policy_table_page_", &core::CatalogManager::shardPolicyTablePage},
    {"shardPolicyParamTablePage", "shard_policy_param_table_page_", &core::CatalogManager::shardPolicyParamTablePage},
    {"shardKeyTablePage", "shard_key_table_page_", &core::CatalogManager::shardKeyTablePage},
    {"shardTablePage", "shard_table_page_", &core::CatalogManager::shardTablePage},
    {"shardScopeTablePage", "shard_scope_table_page_", &core::CatalogManager::shardScopeTablePage},
    {"shardRangeTablePage", "shard_range_table_page_", &core::CatalogManager::shardRangeTablePage},
    {"shardReplicaTablePage", "shard_replica_table_page_", &core::CatalogManager::shardReplicaTablePage},
    {"shardMigrationTablePage", "shard_migration_table_page_", &core::CatalogManager::shardMigrationTablePage},
    {"shardZoneTablePage", "shard_zone_table_page_", &core::CatalogManager::shardZoneTablePage},
    {"shardZoneRangeTablePage", "shard_zone_range_table_page_", &core::CatalogManager::shardZoneRangeTablePage},
    {"workloadClassTablePage", "workload_class_table_page_", &core::CatalogManager::workloadClassTablePage},
    {"workloadRouteTablePage", "workload_route_table_page_", &core::CatalogManager::workloadRouteTablePage},
    {"admissionPolicyTablePage", "admission_policy_table_page_", &core::CatalogManager::admissionPolicyTablePage},
    {"admissionBindingTablePage", "admission_binding_table_page_", &core::CatalogManager::admissionBindingTablePage},
    {"sloProfileTablePage", "slo_profile_table_page_", &core::CatalogManager::sloProfileTablePage},
    {"sloBindingTablePage", "slo_binding_table_page_", &core::CatalogManager::sloBindingTablePage},
    {"sloWindowTablePage", "slo_window_table_page_", &core::CatalogManager::sloWindowTablePage},
    {"sloBurnEventTablePage", "slo_burn_event_table_page_", &core::CatalogManager::sloBurnEventTablePage},
    {"autoscalePolicyTablePage", "autoscale_policy_table_page_", &core::CatalogManager::autoscalePolicyTablePage},
    {"autoscaleActionTablePage", "autoscale_action_table_page_", &core::CatalogManager::autoscaleActionTablePage},
    {"admissionTuningEventTablePage", "admission_tuning_event_table_page_", &core::CatalogManager::admissionTuningEventTablePage},
    {"clusterPolicyTablePage", "cluster_policy_table_page_", &core::CatalogManager::clusterPolicyTablePage},
    {"failureDetectorTablePage", "failure_detector_table_page_", &core::CatalogManager::failureDetectorTablePage},
    {"alertRuleTablePage", "alert_rule_table_page_", &core::CatalogManager::alertRuleTablePage},
    {"alertTargetTablePage", "alert_target_table_page_", &core::CatalogManager::alertTargetTablePage},
    {"alertRouteTablePage", "alert_route_table_page_", &core::CatalogManager::alertRouteTablePage},
    {"alertEventTablePage", "alert_event_table_page_", &core::CatalogManager::alertEventTablePage},
    {"alertAckTablePage", "alert_ack_table_page_", &core::CatalogManager::alertAckTablePage},
    {"alertSilenceTablePage", "alert_silence_table_page_", &core::CatalogManager::alertSilenceTablePage},
    {"networkPartitionEventTablePage", "network_partition_event_table_page_", &core::CatalogManager::networkPartitionEventTablePage},
    {"networkPartitionMemberTablePage", "network_partition_member_table_page_", &core::CatalogManager::networkPartitionMemberTablePage},
    {"healingPolicyTablePage", "healing_policy_table_page_", &core::CatalogManager::healingPolicyTablePage},
    {"healingActionTablePage", "healing_action_table_page_", &core::CatalogManager::healingActionTablePage},
    {"healingActionParamTablePage", "healing_action_param_table_page_", &core::CatalogManager::healingActionParamTablePage},
    {"healingRunTablePage", "healing_run_table_page_", &core::CatalogManager::healingRunTablePage},
    {"healingStepTablePage", "healing_step_table_page_", &core::CatalogManager::healingStepTablePage},
    {"jobTypeTablePage", "job_type_table_page_", &core::CatalogManager::jobTypeTablePage},
    {"jobTypeParamTablePage", "job_type_param_table_page_", &core::CatalogManager::jobTypeParamTablePage},
    {"jobParamTablePage", "job_param_table_page_", &core::CatalogManager::jobParamTablePage},
    {"jobScheduleTablePage", "job_schedule_table_page_", &core::CatalogManager::jobScheduleTablePage},
    {"jobTypePolicyTablePage", "job_type_policy_table_page_", &core::CatalogManager::jobTypePolicyTablePage},
    {"remoteConnectorTablePage", "remote_connector_table_page_", &core::CatalogManager::remoteConnectorTablePage},
    {"remoteConnectorCapabilityTablePage", "remote_connector_capability_table_page_", &core::CatalogManager::remoteConnectorCapabilityTablePage},
    {"remoteMetadataSnapshotTablePage", "remote_metadata_snapshot_table_page_", &core::CatalogManager::remoteMetadataSnapshotTablePage},
    {"remoteMetadataObjectTablePage", "remote_metadata_object_table_page_", &core::CatalogManager::remoteMetadataObjectTablePage},
    {"remoteMetadataColumnTablePage", "remote_metadata_column_table_page_", &core::CatalogManager::remoteMetadataColumnTablePage},
    {"remoteSchemaMappingTablePage", "remote_schema_mapping_table_page_", &core::CatalogManager::remoteSchemaMappingTablePage},
    {"remotePassthroughPolicyTablePage", "remote_passthrough_policy_table_page_", &core::CatalogManager::remotePassthroughPolicyTablePage},
    {"remotePreparedStatementTablePage", "remote_prepared_statement_table_page_", &core::CatalogManager::remotePreparedStatementTablePage},
    {"remoteTxnBindingTablePage", "remote_txn_binding_table_page_", &core::CatalogManager::remoteTxnBindingTablePage},
    {"remoteExecutionAuditTablePage", "remote_execution_audit_table_page_", &core::CatalogManager::remoteExecutionAuditTablePage},
    {"remoteErrorTablePage", "remote_error_table_page_", &core::CatalogManager::remoteErrorTablePage},
    {"extensionTablePage", "extensions_table_page_", &core::CatalogManager::extensionTablePage},
    {"publicationTablePage", "publication_table_page_", &core::CatalogManager::publicationTablePage},
    {"publicationTableLinkTablePage", "publication_table_link_table_page_", &core::CatalogManager::publicationTableLinkTablePage},
    {"publicationSchemaTablePage", "publication_schema_table_page_", &core::CatalogManager::publicationSchemaTablePage},
    {"subscriptionTablePage", "subscription_table_page_", &core::CatalogManager::subscriptionTablePage},
    {"subscriptionTableLinkTablePage", "subscription_table_link_table_page_", &core::CatalogManager::subscriptionTableLinkTablePage},
    {"clusterFabricLinkTablePage", "cluster_fabric_link_table_page_", &core::CatalogManager::clusterFabricLinkTablePage},
    {"clusterFabricSessionTablePage", "cluster_fabric_session_table_page_", &core::CatalogManager::clusterFabricSessionTablePage},
    {"clusterFabricTxnTablePage", "cluster_fabric_txn_table_page_", &core::CatalogManager::clusterFabricTxnTablePage},
    {"clusterFabricTaskTablePage", "cluster_fabric_task_table_page_", &core::CatalogManager::clusterFabricTaskTablePage},
    {"clusterFabricTaskChunkTablePage", "cluster_fabric_task_chunk_table_page_", &core::CatalogManager::clusterFabricTaskChunkTablePage},
    {"clusterFabricEventTablePage", "cluster_fabric_event_table_page_", &core::CatalogManager::clusterFabricEventTablePage},
    {"clusterFabricErrorTablePage", "cluster_fabric_error_table_page_", &core::CatalogManager::clusterFabricErrorTablePage},
    {"olapWatermarkTablePage", "olap_watermark_table_page_", &core::CatalogManager::olapWatermarkTablePage},
    {"olapPartitionTablePage", "olap_partition_table_page_", &core::CatalogManager::olapPartitionTablePage},
    {"olapSegmentTablePage", "olap_segment_table_page_", &core::CatalogManager::olapSegmentTablePage},
    {"olapIngestLogTablePage", "olap_ingest_log_table_page_", &core::CatalogManager::olapIngestLogTablePage},
    {"cubeTablePage", "cube_table_page_", &core::CatalogManager::cubeTablePage},
    {"cubeDimensionTablePage", "cube_dimension_table_page_", &core::CatalogManager::cubeDimensionTablePage},
    {"cubeLevelTablePage", "cube_level_table_page_", &core::CatalogManager::cubeLevelTablePage},
    {"cubeHierarchyTablePage", "cube_hierarchy_table_page_", &core::CatalogManager::cubeHierarchyTablePage},
    {"cubeHierarchyLevelTablePage", "cube_hierarchy_level_table_page_", &core::CatalogManager::cubeHierarchyLevelTablePage},
    {"cubeMeasureTablePage", "cube_measure_table_page_", &core::CatalogManager::cubeMeasureTablePage},
    {"cubeMaterializationTablePage", "cube_materialization_table_page_", &core::CatalogManager::cubeMaterializationTablePage},
    {"cubeRefreshPolicyTablePage", "cube_refresh_policy_table_page_", &core::CatalogManager::cubeRefreshPolicyTablePage},
    {"cubeJobTablePage", "cube_job_table_page_", &core::CatalogManager::cubeJobTablePage},
    {"cubeJobStepTablePage", "cube_job_step_table_page_", &core::CatalogManager::cubeJobStepTablePage},
    {"cubeStatsTablePage", "cube_stats_table_page_", &core::CatalogManager::cubeStatsTablePage},
    {"tsParserTablePage", "ts_parser_table_page_", &core::CatalogManager::tsParserTablePage},
    {"tsTemplateTablePage", "ts_template_table_page_", &core::CatalogManager::tsTemplateTablePage},
    {"tsDictionaryTablePage", "ts_dictionary_table_page_", &core::CatalogManager::tsDictionaryTablePage},
    {"tsConfigTablePage", "ts_config_table_page_", &core::CatalogManager::tsConfigTablePage},
    {"tsConfigMapTablePage", "ts_config_map_table_page_", &core::CatalogManager::tsConfigMapTablePage},
    {"blobFilterTablePage", "blob_filter_table_page_", &core::CatalogManager::blobFilterTablePage},
    {"triggerMessageTablePage", "trigger_message_table_page_", &core::CatalogManager::triggerMessageTablePage},
    {"columnDropHistoryTablePage", "column_drop_history_table_page_", &core::CatalogManager::columnDropHistoryTablePage},
    {"sblrModuleTablePage", "sblr_module_table_page_", &core::CatalogManager::sblrModuleTablePage},
    {"sblrPlanTablePage", "sblr_plan_table_page_", &core::CatalogManager::sblrPlanTablePage},
    {"sblrPlanDependencyTablePage", "sblr_plan_dependency_table_page_", &core::CatalogManager::sblrPlanDependencyTablePage},
    {"sblrStatementNormTablePage", "sblr_statement_norm_table_page_", &core::CatalogManager::sblrStatementNormTablePage},
    {"sblrArtifactTablePage", "sblr_artifact_table_page_", &core::CatalogManager::sblrArtifactTablePage},
    {"sblrArtifactStatsTablePage", "sblr_artifact_stats_table_page_", &core::CatalogManager::sblrArtifactStatsTablePage},
    {"sblrCompilerTargetTablePage", "sblr_compiler_target_table_page_", &core::CatalogManager::sblrCompilerTargetTablePage},
    {"sblrCompileQueueTablePage", "sblr_compile_queue_table_page_", &core::CatalogManager::sblrCompileQueueTablePage},
    {"replicationChannelTablePage", "replication_channel_table_page_", &core::CatalogManager::replicationChannelTablePage},
    {"replicationChannelMemberTablePage", "replication_channel_member_table_page_", &core::CatalogManager::replicationChannelMemberTablePage},
    {"replicationOriginTablePage", "replication_origin_table_page_", &core::CatalogManager::replicationOriginTablePage},
    {"replicationCursorTablePage", "replication_cursor_table_page_", &core::CatalogManager::replicationCursorTablePage},
    {"replicationOriginProgressTablePage", "replication_origin_progress_table_page_", &core::CatalogManager::replicationOriginProgressTablePage},
    {"replicationTxnBatchTablePage", "replication_txn_batch_table_page_", &core::CatalogManager::replicationTxnBatchTablePage},
    {"replicationApplyLogTablePage", "replication_apply_log_table_page_", &core::CatalogManager::replicationApplyLogTablePage},
    {"replicationRetryQueueTablePage", "replication_retry_queue_table_page_", &core::CatalogManager::replicationRetryQueueTablePage},
    {"replicationConflictTablePage", "replication_conflict_table_page_", &core::CatalogManager::replicationConflictTablePage},
    {"replicationSplitBrainEventTablePage", "replication_split_brain_event_table_page_", &core::CatalogManager::replicationSplitBrainEventTablePage},
    {"replicationErrorTablePage", "replication_error_table_page_", &core::CatalogManager::replicationErrorTablePage},
    {"encryptionKeysTablePage", "encryption_keys_table_page_", &core::CatalogManager::encryptionKeysTablePage},
};

struct PageChainStats {
    uint32_t start_page = 0;
    uint32_t pages_in_chain = 0;
    uint64_t total_record_slots = 0;
    std::string note;
};

std::string statusToString(core::Status status)
{
    return std::to_string(static_cast<int>(status));
}

std::string trim(const std::string& input)
{
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0)
    {
        ++start;
    }
    size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0)
    {
        --end;
    }
    return input.substr(start, end - start);
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string objectTypeToString(core::CatalogManager::ObjectType type)
{
    switch (type)
    {
        case core::CatalogManager::ObjectType::SCHEMA: return "SCHEMA";
        case core::CatalogManager::ObjectType::TABLE: return "TABLE";
        case core::CatalogManager::ObjectType::COLUMN: return "COLUMN";
        case core::CatalogManager::ObjectType::INDEX: return "INDEX";
        case core::CatalogManager::ObjectType::VIEW: return "VIEW";
        case core::CatalogManager::ObjectType::SEQUENCE: return "SEQUENCE";
        case core::CatalogManager::ObjectType::CONSTRAINT: return "CONSTRAINT";
        case core::CatalogManager::ObjectType::TRIGGER: return "TRIGGER";
        case core::CatalogManager::ObjectType::PROCEDURE: return "PROCEDURE";
        case core::CatalogManager::ObjectType::FUNCTION: return "FUNCTION";
        case core::CatalogManager::ObjectType::DOMAIN: return "DOMAIN";
        case core::CatalogManager::ObjectType::PACKAGE: return "PACKAGE";
        case core::CatalogManager::ObjectType::UDR: return "UDR";
        case core::CatalogManager::ObjectType::EXCEPTION: return "EXCEPTION";
        case core::CatalogManager::ObjectType::SYNONYM: return "SYNONYM";
        case core::CatalogManager::ObjectType::FOREIGN_TABLE: return "FOREIGN_TABLE";
        case core::CatalogManager::ObjectType::ROLE: return "ROLE";
        case core::CatalogManager::ObjectType::USER: return "USER";
        case core::CatalogManager::ObjectType::GROUP: return "GROUP";
        case core::CatalogManager::ObjectType::TABLESPACE: return "TABLESPACE";
        case core::CatalogManager::ObjectType::JOB: return "JOB";
        case core::CatalogManager::ObjectType::DATABASE: return "DATABASE";
        default:
            return std::string("UNKNOWN(") + std::to_string(static_cast<uint32_t>(type)) + ")";
    }
}

std::string findRepoRoot()
{
    const auto try_from = [](std::filesystem::path current) -> std::string {
        for (int i = 0; i < 16; ++i)
        {
            const auto header = current / "include" / "scratchbird" / "core" / "catalog_manager.h";
            const auto source = current / "src" / "core" / "catalog_manager.cpp";
            if (std::filesystem::exists(header) && std::filesystem::exists(source))
            {
                return current.string();
            }
            if (!current.has_parent_path())
            {
                break;
            }
            current = current.parent_path();
        }
        return "";
    };

    std::string repo_root = try_from(std::filesystem::current_path());
    if (!repo_root.empty())
    {
        return repo_root;
    }

    return try_from(std::filesystem::path(__FILE__).parent_path());
}

std::unordered_map<std::string, std::string> parseSystemAliasMap(const std::filesystem::path& source_path)
{
    std::unordered_map<std::string, std::string> aliases;

    std::ifstream in(source_path);
    if (!in.is_open())
    {
        return aliases;
    }

    bool in_map = false;
    std::regex entry_re("\\{\\s*\"([^\"]+)\"\\s*,\\s*\"([^\"]+)\"\\s*\\}");
    std::string line;
    while (std::getline(in, line))
    {
        if (!in_map)
        {
            if (line.find("kSystemTableAliasMap") != std::string::npos)
            {
                in_map = true;
            }
            continue;
        }

        if (line.find("};") != std::string::npos)
        {
            break;
        }

        std::smatch match;
        if (std::regex_search(line, match, entry_re))
        {
            aliases.emplace(match[1].str(), match[2].str());
        }
    }

    return aliases;
}

std::vector<std::pair<std::string, std::string>> parseBootstrapSchemaNodes(const std::filesystem::path& source_path)
{
    std::vector<std::pair<std::string, std::string>> nodes;

    std::ifstream in(source_path);
    if (!in.is_open())
    {
        return nodes;
    }

    bool in_array = false;
    std::string line;
    while (std::getline(in, line))
    {
        if (!in_array)
        {
            if (line.find("kBootstrapSchemas") != std::string::npos)
            {
                in_array = true;
            }
            continue;
        }

        if (line.find("}};") != std::string::npos)
        {
            break;
        }

        std::string cleaned = trim(line);
        if (cleaned.empty() || cleaned[0] != '{')
        {
            continue;
        }

        // Parse manually to avoid brittle regex groups on nullptr variant.
        // Expected forms:
        // {"sys", "sys", nullptr, false},
        // {"sys.security", "security", "sys", false},
        std::vector<std::string> quoted;
        std::string token;
        bool in_quote = false;
        for (size_t i = 0; i < cleaned.size(); ++i)
        {
            char c = cleaned[i];
            if (c == '"')
            {
                if (in_quote)
                {
                    quoted.push_back(token);
                    token.clear();
                }
                in_quote = !in_quote;
                continue;
            }
            if (in_quote)
            {
                token.push_back(c);
            }
        }

        if (quoted.size() >= 2)
        {
            const std::string path = quoted[0];
            const std::string parent = (quoted.size() >= 3) ? quoted[2] : "";
            nodes.emplace_back(path, parent);
        }
    }

    return nodes;
}

std::vector<std::string> loadFileLines(const std::filesystem::path& file_path)
{
    std::vector<std::string> lines;
    std::ifstream in(file_path);
    if (!in.is_open())
    {
        return lines;
    }
    std::string line;
    while (std::getline(in, line))
    {
        lines.push_back(line);
    }
    return lines;
}

bool hasIdentifierToken(const std::string& line, const std::string& token)
{
    size_t pos = line.find(token);
    while (pos != std::string::npos)
    {
        const bool left_ok = (pos == 0) ||
            !(std::isalnum(static_cast<unsigned char>(line[pos - 1])) != 0 || line[pos - 1] == '_');
        const size_t end = pos + token.size();
        const bool right_ok = (end >= line.size()) ||
            !(std::isalnum(static_cast<unsigned char>(line[end])) != 0 || line[end] == '_');
        if (left_ok && right_ok)
        {
            return true;
        }
        pos = line.find(token, pos + 1);
    }
    return false;
}

PageChainStats inspectPageChain(const core::Database& db, uint32_t start_page, core::ErrorContext* ctx)
{
    PageChainStats stats;
    stats.start_page = start_page;

    if (start_page == 0)
    {
        stats.note = "unassigned";
        return stats;
    }

    std::vector<uint8_t> page(db.page_size(), 0);
    std::unordered_set<uint32_t> visited;
    uint32_t current = start_page;

    while (current != 0)
    {
        if (!visited.insert(current).second)
        {
            stats.note = "cycle_detected";
            return stats;
        }

        core::Status read_status = db.read_page(current, page.data(), ctx);
        if (read_status != core::Status::OK)
        {
            stats.note = std::string("read_failed_") + statusToString(read_status);
            return stats;
        }

        const auto* header = reinterpret_cast<const core::PageHeader*>(page.data());
        if (header->page_type != core::PAGE_TYPE_HEAP)
        {
            stats.note = std::string("non_heap_page_type_") + std::to_string(header->page_type);
            return stats;
        }

        const auto* heap = reinterpret_cast<const core::CatalogHeapPage*>(page.data());
        ++stats.pages_in_chain;
        stats.total_record_slots += heap->record_count;
        current = heap->next_page;
    }

    stats.note = "ok";
    return stats;
}

std::string removeSuffix(std::string value, const std::string& suffix)
{
    if (value.size() >= suffix.size() &&
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        value.resize(value.size() - suffix.size());
    }
    return value;
}

}  // namespace

TEST(CatalogFullExtractAudit, ExportCatalogStructureAndUsage)
{
    const std::string repo_root = findRepoRoot();
    ASSERT_FALSE(repo_root.empty()) << "Could not locate repository root from cwd="
                                    << std::filesystem::current_path().string();

    const std::filesystem::path source_path =
        std::filesystem::path(repo_root) / "src" / "core" / "catalog_manager.cpp";

    const std::filesystem::path out_dir =
        std::filesystem::path(repo_root) / "docs" / "audit" / "catalog_extract_2026-02-19";
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    ASSERT_FALSE(ec) << "Failed to create output directory: " << ec.message();

    std::ostringstream db_name;
    db_name << "catalog_extract_" << std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path db_path =
        std::filesystem::path("/tmp") / (db_name.str() + ".sbdb");

    std::filesystem::remove(db_path, ec);
    std::filesystem::remove(db_path.string() + "-lock", ec);

    core::ErrorContext ctx;
    ASSERT_EQ(core::Database::create(db_path.string(), 8192, &ctx), core::Status::OK)
        << ctx.message;

    core::Database db;
    ASSERT_EQ(db.open(db_path.string(), &ctx), core::Status::OK) << ctx.message;

    core::CatalogManager* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    const auto system_alias_map = parseSystemAliasMap(source_path);

    std::vector<core::CatalogManager::SchemaInfo> schemas;
    ASSERT_EQ(catalog->listSchemas(schemas, &ctx), core::Status::OK) << ctx.message;
    std::vector<std::pair<std::string, core::CatalogManager::SchemaInfo>> schemas_with_paths;
    schemas_with_paths.reserve(schemas.size());
    for (const auto& schema : schemas)
    {
        std::string path;
        core::ErrorContext path_ctx;
        core::Status path_status = catalog->getSchemaPath(schema.schema_id, path, &path_ctx);
        if (path_status != core::Status::OK || path.empty())
        {
            path = schema.full_path.empty() ? schema.schema_name : schema.full_path;
        }
        schemas_with_paths.emplace_back(path, schema);
    }
    std::sort(schemas_with_paths.begin(), schemas_with_paths.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    {
        std::ofstream out(out_dir / "01_schema_tree.tsv");
        ASSERT_TRUE(out.is_open());
        out << "path\tschema_name\tschema_id\tparent_schema_id\tschema_type\tname_is_delimited\n";
        for (const auto& [path, schema] : schemas_with_paths)
        {
            out << path << '\t'
                << schema.schema_name << '\t'
                << schema.schema_id.toString() << '\t'
                << schema.parent_schema_id.toString() << '\t'
                << static_cast<uint32_t>(schema.schema_type) << '\t'
                << static_cast<uint32_t>(schema.name_is_delimited ? 1 : 0) << '\n';
        }
    }

    std::vector<core::CatalogManager::ResolvedObject> objects;
    core::CatalogManager::ResolveFilter filter;
    ASSERT_EQ(catalog->listResolvedObjects(filter, objects, &ctx), core::Status::OK) << ctx.message;
    std::sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) {
        if (a.full_path != b.full_path)
        {
            return a.full_path < b.full_path;
        }
        return a.object_id.toString() < b.object_id.toString();
    });

    {
        std::ofstream out(out_dir / "02_resolved_objects.tsv");
        ASSERT_TRUE(out.is_open());
        out << "object_type\tfull_path\tschema_path\tobject_name\tobject_id\tschema_id\tparent_object_id\tdialect_tag\tcompat_name\n";
        for (const auto& obj : objects)
        {
            out << objectTypeToString(obj.object_type) << '\t'
                << obj.full_path << '\t'
                << obj.schema_path << '\t'
                << obj.object_name << '\t'
                << obj.object_id.toString() << '\t'
                << obj.schema_id.toString() << '\t'
                << obj.parent_object_id.toString() << '\t'
                << obj.dialect_tag << '\t'
                << obj.compat_name << '\n';
        }
    }

    const auto bootstrap_nodes = parseBootstrapSchemaNodes(source_path);
    {
        std::ofstream out(out_dir / "00_bootstrap_schema_nodes.tsv");
        ASSERT_TRUE(out.is_open());
        out << "path\tparent_path\n";
        for (const auto& node : bootstrap_nodes)
        {
            out << node.first << '\t' << node.second << '\n';
        }
    }

    std::vector<std::string> core_cpp_lines = loadFileLines(source_path);
    std::vector<std::string> core_h_lines =
        loadFileLines(std::filesystem::path(repo_root) / "include" / "scratchbird" / "core" / "catalog_manager.h");
    std::vector<std::string> executor_lines =
        loadFileLines(std::filesystem::path(repo_root) / "src" / "sblr" / "executor.cpp");

    // Persist dirty catalog pages before raw page reads for occupancy.
    ASSERT_EQ(db.sync(&ctx), core::Status::OK) << ctx.message;

    {
        std::ofstream out(out_dir / "03_catalog_page_occupancy.tsv");
        ASSERT_TRUE(out.is_open());
        out << "getter\tpage_var\tlogical_name\tspec_alias\tcanonical_guess\tpage_id\tpages_in_chain\ttotal_record_slots\thas_data\tnote\trefs_catalog_cpp\trefs_catalog_h\trefs_executor_cpp\tfirst_refs\n";

        for (const auto& entry : kPageGetters)
        {
            uint32_t page_id = (catalog->*(entry.getter))();
            PageChainStats stats = inspectPageChain(db, page_id, &ctx);

            std::string logical_name = removeSuffix(entry.page_var_name, "_table_page_");
            if (logical_name == entry.page_var_name)
            {
                logical_name = removeSuffix(logical_name, "_page_");
            }

            std::string alias;
            auto alias_it = system_alias_map.find(logical_name);
            if (alias_it != system_alias_map.end())
            {
                alias = alias_it->second;
            }

            std::string canonical_guess;
            if (!alias.empty())
            {
                canonical_guess = "sys.catalog." + logical_name;
            }

            size_t refs_core_cpp = 0;
            size_t refs_core_h = 0;
            size_t refs_executor = 0;
            std::vector<std::string> first_refs;

            auto capture_refs = [&](const std::vector<std::string>& lines,
                                    const std::string& file_tag,
                                    size_t& ref_counter) {
                for (size_t i = 0; i < lines.size(); ++i)
                {
                    if (!hasIdentifierToken(lines[i], entry.page_var_name))
                    {
                        continue;
                    }
                    ++ref_counter;
                    if (first_refs.size() < 6)
                    {
                        std::ostringstream ref;
                        ref << file_tag << ':' << (i + 1);
                        first_refs.push_back(ref.str());
                    }
                }
            };

            capture_refs(core_cpp_lines, "src/core/catalog_manager.cpp", refs_core_cpp);
            capture_refs(core_h_lines, "include/scratchbird/core/catalog_manager.h", refs_core_h);
            capture_refs(executor_lines, "src/sblr/executor.cpp", refs_executor);

            std::ostringstream first_refs_joined;
            for (size_t i = 0; i < first_refs.size(); ++i)
            {
                if (i > 0)
                {
                    first_refs_joined << ',';
                }
                first_refs_joined << first_refs[i];
            }

            out << entry.getter_name << '\t'
                << entry.page_var_name << '\t'
                << logical_name << '\t'
                << alias << '\t'
                << canonical_guess << '\t'
                << page_id << '\t'
                << stats.pages_in_chain << '\t'
                << stats.total_record_slots << '\t'
                << (stats.total_record_slots > 0 ? "yes" : "no") << '\t'
                << stats.note << '\t'
                << refs_core_cpp << '\t'
                << refs_core_h << '\t'
                << refs_executor << '\t'
                << first_refs_joined.str() << '\n';
        }
    }

    {
        std::ofstream out(out_dir / "04_system_table_alias_map.tsv");
        ASSERT_TRUE(out.is_open());
        out << "logical_name\talias\n";

        std::vector<std::pair<std::string, std::string>> aliases(system_alias_map.begin(),
                                                                  system_alias_map.end());
        std::sort(aliases.begin(), aliases.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        for (const auto& [logical, alias] : aliases)
        {
            out << logical << '\t' << alias << '\n';
        }
    }

    {
        std::set<std::string> outline;
        for (const auto& [path, schema] : schemas_with_paths)
        {
            outline.insert(path + " [s]");
        }
        for (const auto& obj : objects)
        {
            if (obj.full_path.empty())
            {
                continue;
            }
            outline.insert(obj.full_path + " [o] (" + objectTypeToString(obj.object_type) + ")");
        }

        std::ofstream out(out_dir / "05_catalog_outline.txt");
        ASSERT_TRUE(out.is_open());
        for (const auto& line : outline)
        {
            out << line << '\n';
        }
    }

    {
        std::ofstream out(out_dir / "README.md");
        ASSERT_TRUE(out.is_open());
        out << "# Catalog Extract (Generated 2026-02-19)\n\n";
        out << "This directory contains a full extract from a freshly bootstrapped ScratchBird database.\n\n";
        out << "## Files\n";
        out << "- `00_bootstrap_schema_nodes.tsv`: Canonical bootstrap schema nodes from source.\n";
        out << "- `01_schema_tree.tsv`: Runtime schema tree from catalog API.\n";
        out << "- `02_resolved_objects.tsv`: Runtime object resolver output.\n";
        out << "- `03_catalog_page_occupancy.tsv`: Catalog page variable, page id, occupancy, and reference counts.\n";
        out << "- `04_system_table_alias_map.tsv`: System logical table aliases (`logical -> sys.*record`).\n";
        out << "- `05_catalog_outline.txt`: Combined schema/object outline with `[s]` and `[o]` markers.\n";
    }

    db.close();
    std::filesystem::remove(db_path, ec);
    std::filesystem::remove(db_path.string() + "-lock", ec);
}
