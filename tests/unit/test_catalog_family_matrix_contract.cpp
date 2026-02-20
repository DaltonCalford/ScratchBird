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

#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

class CatalogFamilyMatrixContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_family_matrix_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }
};

TEST_F(CatalogFamilyMatrixContractTest, CanonicalCatalogFamilyPagesAreMaterialized)
{
    struct PageEntry
    {
        const char* name;
        uint32_t page_id;
    };

    const std::vector<PageEntry> pages = {
        {"database", catalog_->databaseTablePage()},
        {"object", catalog_->objectTablePage()},
        {"object_name", catalog_->objectNameTablePage()},
        {"domain", catalog_->domainsTablePage()},
        {"type", catalog_->typeTablePage()},
        {"type_modifier", catalog_->typeModifiersTablePage()},
        {"type_io", catalog_->typeIoTablePage()},
        {"type_cast", catalog_->typeCastsTablePage()},
        {"type_transform", catalog_->typeTransformsTablePage()},
        {"encoding_conversion", catalog_->encodingConversionsTablePage()},
        {"domain_param_key", catalog_->domainParamKeysTablePage()},
        {"domain_parameter", catalog_->domainParametersTablePage()},
        {"domain_constraint", catalog_->domainConstraintsTablePage()},
        {"domain_security", catalog_->domainSecurityTablePage()},
        {"domain_validation", catalog_->domainValidationTablePage()},
        {"domain_integrity", catalog_->domainIntegrityTablePage()},
        {"charset_alias", catalog_->charsetAliasesTablePage()},
        {"collation_tailoring", catalog_->collationTailoringTablePage()},
        {"resource_bundle", catalog_->resourceBundlesTablePage()},
        {"resource_artifact", catalog_->resourceArtifactsTablePage()},
        {"timezone_transition", catalog_->timezoneTransitionsTablePage()},
        {"timezone_leap_second", catalog_->timezoneLeapSecondsTablePage()},
        {"reserved_word", catalog_->reservedWordsTablePage()},
        {"emulation_profile", catalog_->emulationProfileTablePage()},
        {"parser_profile", catalog_->parserProfilesTablePage()},
        {"parser_capability_entry", catalog_->parserCapabilityEntriesTablePage()},
        {"parser_transform_entry", catalog_->parserTransformEntriesTablePage()},
        {"parser_error_map_entry", catalog_->parserErrorMapEntriesTablePage()},
        {"parser_feature_precedence", catalog_->parserFeaturePrecedenceTablePage()},
        {"partitioned_table", catalog_->partitionedTablesTablePage()},
        {"partition", catalog_->partitionsTablePage()},
        {"table_inheritance", catalog_->tableInheritanceTablePage()},
        {"language", catalog_->languagesTablePage()},
        {"event", catalog_->eventsTablePage()},
        {"package_member", catalog_->packageMembersTablePage()},
        {"index_column", catalog_->indexColumnsTablePage()},
        {"index_opclass", catalog_->indexOpclassTablePage()},
        {"index_opclass_function", catalog_->indexOpclassFunctionTablePage()},
        {"index_option", catalog_->indexOptionsTablePage()},
        {"index_access_method", catalog_->indexAccessMethodsTablePage()},
        {"index_maintenance", catalog_->indexMaintenanceTablePage()},
        {"index_maintenance_delta", catalog_->indexMaintenanceDeltasTablePage()},
        {"index_build_delta", catalog_->indexBuildDeltasTablePage()},
        {"index_stats", catalog_->indexStatsTablePage()},
        {"index_usage", catalog_->indexUsageTablePage()},
        {"index_contention", catalog_->indexContentionTablePage()},
        {"index_storage", catalog_->indexStorageTablePage()},
        {"index_health", catalog_->indexHealthTablePage()},
        {"filespace_stats", catalog_->filespaceStatsTablePage()},
        {"lob", catalog_->lobTablePage()},
        {"lob_page", catalog_->lobPageTablePage()},
        {"backup_history", catalog_->backupHistoryTablePage()},
        {"runtime_connection", catalog_->connectionTablePage()},
        {"runtime_transaction", catalog_->transactionTablePage()},
        {"auth_mapping", catalog_->authMappingTablePage()},
        {"role_setting", catalog_->roleSettingTablePage()},
        {"security_label", catalog_->securityLabelTablePage()},
        {"security_class", catalog_->securityClassTablePage()},
        {"cert_registry", catalog_->certRegistryTablePage()},
        {"private_key_store", catalog_->privateKeyStoreTablePage()},
        {"trust_anchor", catalog_->trustAnchorTablePage()},
        {"channel_cert_binding", catalog_->channelCertBindingTablePage()},
        {"cert_revocation", catalog_->certRevocationTablePage()},
        {"pki_distribution_state", catalog_->pkiDistributionStateTablePage()},
        {"trust_anchor_rollover", catalog_->trustAnchorRolloverTablePage()},
        {"encryption_profile", catalog_->encryptionProfileTablePage()},
        {"encryption_key", catalog_->encryptionKeyTablePage()},
        {"encryption_key_shard", catalog_->encryptionKeyShardTablePage()},
        {"encryption_bootstrap_info", catalog_->encryptionBootstrapInfoTablePage()},
        {"node", catalog_->nodeTablePage()},
        {"node_role_binding", catalog_->nodeRoleBindingTablePage()},
        {"node_service", catalog_->nodeServiceTablePage()},
        {"node_capability", catalog_->nodeCapabilityTablePage()},
        {"clock_policy", catalog_->clockPolicyTablePage()},
        {"clock_source", catalog_->clockSourceTablePage()},
        {"node_clock_state", catalog_->nodeClockStateTablePage()},
        {"clock_violation_event", catalog_->clockViolationEventTablePage()},
        {"cluster", catalog_->clusterTablePage()},
        {"shard_policy", catalog_->shardPolicyTablePage()},
        {"shard_policy_param", catalog_->shardPolicyParamTablePage()},
        {"shard_key", catalog_->shardKeyTablePage()},
        {"shard", catalog_->shardTablePage()},
        {"shard_scope", catalog_->shardScopeTablePage()},
        {"shard_range", catalog_->shardRangeTablePage()},
        {"shard_replica", catalog_->shardReplicaTablePage()},
        {"shard_migration", catalog_->shardMigrationTablePage()},
        {"shard_zone", catalog_->shardZoneTablePage()},
        {"shard_zone_range", catalog_->shardZoneRangeTablePage()},
        {"workload_class", catalog_->workloadClassTablePage()},
        {"workload_route", catalog_->workloadRouteTablePage()},
        {"admission_policy", catalog_->admissionPolicyTablePage()},
        {"admission_binding", catalog_->admissionBindingTablePage()},
        {"slo_profile", catalog_->sloProfileTablePage()},
        {"slo_binding", catalog_->sloBindingTablePage()},
        {"slo_window", catalog_->sloWindowTablePage()},
        {"slo_burn_event", catalog_->sloBurnEventTablePage()},
        {"autoscale_policy", catalog_->autoscalePolicyTablePage()},
        {"autoscale_action", catalog_->autoscaleActionTablePage()},
        {"admission_tuning_event", catalog_->admissionTuningEventTablePage()},
        {"cluster_policy", catalog_->clusterPolicyTablePage()},
        {"failure_detector", catalog_->failureDetectorTablePage()},
        {"alert_rule", catalog_->alertRuleTablePage()},
        {"alert_target", catalog_->alertTargetTablePage()},
        {"alert_route", catalog_->alertRouteTablePage()},
        {"alert_event", catalog_->alertEventTablePage()},
        {"alert_ack", catalog_->alertAckTablePage()},
        {"alert_silence", catalog_->alertSilenceTablePage()},
        {"network_partition_event", catalog_->networkPartitionEventTablePage()},
        {"network_partition_member", catalog_->networkPartitionMemberTablePage()},
        {"healing_policy", catalog_->healingPolicyTablePage()},
        {"healing_action", catalog_->healingActionTablePage()},
        {"healing_action_param", catalog_->healingActionParamTablePage()},
        {"healing_run", catalog_->healingRunTablePage()},
        {"healing_step", catalog_->healingStepTablePage()},
        {"job_type", catalog_->jobTypeTablePage()},
        {"job_type_param", catalog_->jobTypeParamTablePage()},
        {"job_param", catalog_->jobParamTablePage()},
        {"job_schedule", catalog_->jobScheduleTablePage()},
        {"job_type_policy", catalog_->jobTypePolicyTablePage()},
        {"remote_connector", catalog_->remoteConnectorTablePage()},
        {"remote_connector_capability", catalog_->remoteConnectorCapabilityTablePage()},
        {"remote_metadata_snapshot", catalog_->remoteMetadataSnapshotTablePage()},
        {"remote_metadata_object", catalog_->remoteMetadataObjectTablePage()},
        {"remote_metadata_column", catalog_->remoteMetadataColumnTablePage()},
        {"remote_schema_mapping", catalog_->remoteSchemaMappingTablePage()},
        {"remote_passthrough_policy", catalog_->remotePassthroughPolicyTablePage()},
        {"remote_prepared_statement", catalog_->remotePreparedStatementTablePage()},
        {"remote_txn_binding", catalog_->remoteTxnBindingTablePage()},
        {"remote_execution_audit", catalog_->remoteExecutionAuditTablePage()},
        {"remote_error", catalog_->remoteErrorTablePage()},
        {"extension", catalog_->extensionTablePage()},
        {"publication", catalog_->publicationTablePage()},
        {"publication_table_link", catalog_->publicationTableLinkTablePage()},
        {"publication_schema", catalog_->publicationSchemaTablePage()},
        {"subscription", catalog_->subscriptionTablePage()},
        {"subscription_table_link", catalog_->subscriptionTableLinkTablePage()},
        {"cluster_fabric_link", catalog_->clusterFabricLinkTablePage()},
        {"cluster_fabric_session", catalog_->clusterFabricSessionTablePage()},
        {"cluster_fabric_txn", catalog_->clusterFabricTxnTablePage()},
        {"cluster_fabric_task", catalog_->clusterFabricTaskTablePage()},
        {"cluster_fabric_task_chunk", catalog_->clusterFabricTaskChunkTablePage()},
        {"cluster_fabric_event", catalog_->clusterFabricEventTablePage()},
        {"cluster_fabric_error", catalog_->clusterFabricErrorTablePage()},
        {"olap_watermark", catalog_->olapWatermarkTablePage()},
        {"olap_partition", catalog_->olapPartitionTablePage()},
        {"olap_segment", catalog_->olapSegmentTablePage()},
        {"olap_ingest_log", catalog_->olapIngestLogTablePage()},
        {"cube", catalog_->cubeTablePage()},
        {"cube_dimension", catalog_->cubeDimensionTablePage()},
        {"cube_level", catalog_->cubeLevelTablePage()},
        {"cube_hierarchy", catalog_->cubeHierarchyTablePage()},
        {"cube_hierarchy_level", catalog_->cubeHierarchyLevelTablePage()},
        {"cube_measure", catalog_->cubeMeasureTablePage()},
        {"cube_materialization", catalog_->cubeMaterializationTablePage()},
        {"cube_refresh_policy", catalog_->cubeRefreshPolicyTablePage()},
        {"cube_job", catalog_->cubeJobTablePage()},
        {"cube_job_step", catalog_->cubeJobStepTablePage()},
        {"cube_stats", catalog_->cubeStatsTablePage()},
        {"ts_parser", catalog_->tsParserTablePage()},
        {"ts_template", catalog_->tsTemplateTablePage()},
        {"ts_dictionary", catalog_->tsDictionaryTablePage()},
        {"ts_config", catalog_->tsConfigTablePage()},
        {"ts_config_map", catalog_->tsConfigMapTablePage()},
        {"blob_filter", catalog_->blobFilterTablePage()},
        {"trigger_message", catalog_->triggerMessageTablePage()},
        {"column_drop_history", catalog_->columnDropHistoryTablePage()},
        {"sblr_module", catalog_->sblrModuleTablePage()},
        {"sblr_plan", catalog_->sblrPlanTablePage()},
        {"sblr_plan_dependency", catalog_->sblrPlanDependencyTablePage()},
        {"sblr_statement_norm", catalog_->sblrStatementNormTablePage()},
        {"sblr_artifact", catalog_->sblrArtifactTablePage()},
        {"sblr_artifact_stats", catalog_->sblrArtifactStatsTablePage()},
        {"sblr_compiler_target", catalog_->sblrCompilerTargetTablePage()},
        {"sblr_compile_queue", catalog_->sblrCompileQueueTablePage()},
        {"replication_channel", catalog_->replicationChannelTablePage()},
        {"replication_channel_member", catalog_->replicationChannelMemberTablePage()},
        {"replication_origin", catalog_->replicationOriginTablePage()},
        {"replication_cursor", catalog_->replicationCursorTablePage()},
        {"replication_origin_progress", catalog_->replicationOriginProgressTablePage()},
        {"replication_txn_batch", catalog_->replicationTxnBatchTablePage()},
        {"replication_apply_log", catalog_->replicationApplyLogTablePage()},
        {"replication_retry_queue", catalog_->replicationRetryQueueTablePage()},
        {"replication_conflict", catalog_->replicationConflictTablePage()},
        {"replication_split_brain_event", catalog_->replicationSplitBrainEventTablePage()},
        {"replication_error", catalog_->replicationErrorTablePage()},
    };

    for (const auto& entry : pages)
    {
        EXPECT_NE(entry.page_id, 0u) << "missing canonical catalog family page for " << entry.name;
    }
}

TEST_F(CatalogFamilyMatrixContractTest, CanonicalSchemaGraphNodesExistAndHaveCorrectParents)
{
    struct SchemaNode
    {
        const char* full_path;
        const char* parent_path;
    };

    const std::vector<SchemaNode> nodes = {
        {"sys", nullptr},
        {"connections", nullptr},
        {"users", nullptr},
        {"group", nullptr},
        {"cluster", nullptr},
        {"remote", nullptr},
        {"local", nullptr},
        {"nosql", nullptr},
        {"emulated", nullptr},
        {"sys.information", "sys"},
        {"sys.security", "sys"},
        {"sys.system", "sys"},
        {"sys.schema", "sys"},
        {"sys.cluster", "sys"},
        {"sys.connections", "sys"},
        {"sys.emulation", "sys"},
        {"sys.jobs", "sys"},
        {"users.public", "users"},
        {"users.app_data", "users"},
        {"users.roles", "users"},
        {"users.groups", "users"},
        {"remote.emulation", "remote"},
        {"remote.fdw", "remote"},
        {"remote.links", "remote"},
        {"local.instances", "local"},
        {"local.links", "local"},
        {"nosql.cassandra", "nosql"},
        {"nosql.mongodb", "nosql"},
        {"nosql.neo4j", "nosql"},
        {"nosql.redis", "nosql"},
        {"nosql.milvus", "nosql"},
        {"sys.security.users", "sys.security"},
        {"sys.security.roles", "sys.security"},
        {"sys.security.groups", "sys.security"},
        {"sys.security.auth", "sys.security"},
        {"remote.emulation.firebird", "remote.emulation"},
        {"remote.emulation.postgresql", "remote.emulation"},
        {"remote.emulation.mysql", "remote.emulation"},
        {"remote.emulation.cassandra", "remote.emulation"},
        {"remote.emulation.mongodb", "remote.emulation"},
        {"remote.emulation.neo4j", "remote.emulation"},
        {"remote.emulation.redis", "remote.emulation"},
        {"remote.emulation.milvus", "remote.emulation"},
    };

    ErrorContext ctx;
    for (const auto& node : nodes)
    {
        CatalogManager::SchemaInfo info{};
        ASSERT_EQ(catalog_->getSchema(node.full_path, info, &ctx), Status::OK)
            << "missing schema node " << node.full_path << ": " << ctx.message;

        if (node.parent_path == nullptr)
        {
            continue;
        }

        CatalogManager::SchemaInfo parent{};
        ASSERT_EQ(catalog_->getSchema(node.parent_path, parent, &ctx), Status::OK)
            << "missing parent schema node " << node.parent_path << ": " << ctx.message;

        EXPECT_EQ(info.parent_schema_id, parent.schema_id)
            << "parent mismatch for schema node " << node.full_path;
    }
}
