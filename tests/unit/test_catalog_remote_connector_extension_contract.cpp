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
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogRemoteConnectorExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};
    ID system_user_id_{};
    ID fdw_server_id_{};
    ID user_mapping_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_remote_connector_extension_contract_" +
                   std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        ASSERT_EQ(catalog_->createSchema("cat026_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;

        system_user_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{});

        ASSERT_EQ(catalog_->createForeignServer(
                      "cat026_fdw_server", "postgresql", "127.0.0.1", 5432, "{}",
                      fdw_server_id_, &ctx),
                  Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog_->createUserMapping(
                      system_user_id_, fdw_server_id_, "sb_user", "sb_secret",
                      user_mapping_id_, &ctx),
                  Status::OK)
            << ctx.message;
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }
};

TEST_F(CatalogRemoteConnectorExtensionContractTest, RemoteConnectorExtensionCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::UserMappingInfo mapping_view{};
    ASSERT_EQ(catalog_->getUserMapping(system_user_id_, fdw_server_id_, mapping_view, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(mapping_view.mapping_id, user_mapping_id_);
    EXPECT_EQ(mapping_view.remote_user, "sb_user");
    EXPECT_EQ(mapping_view.remote_credentials, "<write-only>");

    CatalogManager::UserMappingInfo mapping_runtime{};
    ASSERT_EQ(catalog_->getUserMappingForRuntime(system_user_id_, fdw_server_id_, mapping_runtime, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(mapping_runtime.mapping_id, user_mapping_id_);
    EXPECT_EQ(mapping_runtime.remote_user, "sb_user");
    EXPECT_EQ(mapping_runtime.remote_credentials, "sb_secret");

    CatalogManager::RemoteConnectorCatalogInfo connector{};
    connector.remote_connector_id = generateUuidV7();
    connector.fdw_server_id = fdw_server_id_;
    connector.fdw_id = generateUuidV7();
    connector.connector_name = "remote_pg_primary";
    connector.engine_name = "postgresql";
    connector.has_engine_version_text = true;
    connector.engine_version_text = "18.0";
    connector.endpoint_uri = "tcp://127.0.0.1:5432";
    connector.has_default_mapping_id = true;
    connector.default_mapping_id = user_mapping_id_;
    connector.state = CatalogManager::RemoteConnectorState::READY;
    connector.failure_count = 0;
    connector.has_last_probe_time = true;
    connector.last_probe_time = 100;
    connector.has_last_ready_time = true;
    connector.last_ready_time = 101;
    connector.module_checksum = 12345;
    ASSERT_EQ(catalog_->upsertRemoteConnectorCatalogEntry(connector, &ctx), Status::OK) << ctx.message;

    CatalogManager::RemoteConnectorCatalogInfo connector_out{};
    ASSERT_EQ(catalog_->getRemoteConnectorCatalogEntry(connector.remote_connector_id, connector_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(connector_out.connector_name, connector.connector_name);
    EXPECT_EQ(connector_out.engine_version_text, "18.0");

    std::vector<CatalogManager::RemoteConnectorCatalogInfo> connector_rows;
    ASSERT_EQ(catalog_->listRemoteConnectorCatalogEntries(connector_rows, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(connector_rows.size(), 1u);

    {
        ErrorContext fail_ctx;
        auto invalid_transition = connector;
        invalid_transition.state = CatalogManager::RemoteConnectorState::PROBING;
        EXPECT_EQ(catalog_->upsertRemoteConnectorCatalogEntry(invalid_transition, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }
    {
        ErrorContext fail_ctx;
        auto missing_attestation = connector;
        missing_attestation.remote_connector_id = generateUuidV7();
        missing_attestation.connector_name = "remote_pg_no_checksum";
        missing_attestation.module_checksum = 0;
        EXPECT_EQ(catalog_->upsertRemoteConnectorCatalogEntry(missing_attestation, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }
    {
        ErrorContext fail_ctx;
        auto missing_engine_version = connector;
        missing_engine_version.remote_connector_id = generateUuidV7();
        missing_engine_version.connector_name = "remote_pg_no_version";
        missing_engine_version.has_engine_version_text = false;
        missing_engine_version.engine_version_text.clear();
        EXPECT_EQ(catalog_->upsertRemoteConnectorCatalogEntry(missing_engine_version, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }

    CatalogManager::RemoteConnectorCapabilityCatalogInfo capability{};
    capability.capability_id = generateUuidV7();
    capability.remote_connector_id = connector.remote_connector_id;
    capability.capability_key = "query.pushdown.join";
    capability.capability_group = "query";
    capability.capability_value_json = "{\"enabled\":true}";
    capability.has_source_version_text = true;
    capability.source_version_text = "18.0";
    capability.is_enabled = true;
    capability.discovered_time = 102;
    ASSERT_EQ(catalog_->upsertRemoteConnectorCapabilityCatalogEntry(capability, &ctx), Status::OK)
        << ctx.message;
    {
        ErrorContext fail_ctx;
        auto missing_lineage = capability;
        missing_lineage.capability_id = generateUuidV7();
        missing_lineage.capability_key = "query.pushdown.hashjoin";
        missing_lineage.has_source_version_text = false;
        missing_lineage.source_version_text.clear();
        EXPECT_EQ(catalog_->upsertRemoteConnectorCapabilityCatalogEntry(missing_lineage, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }
    {
        ErrorContext fail_ctx;
        auto mismatched_lineage = capability;
        mismatched_lineage.capability_id = generateUuidV7();
        mismatched_lineage.capability_key = "query.pushdown.mergejoin";
        mismatched_lineage.source_version_text = "17.9";
        EXPECT_EQ(catalog_->upsertRemoteConnectorCapabilityCatalogEntry(mismatched_lineage, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }
    {
        ErrorContext fail_ctx;
        auto stale_lineage = capability;
        stale_lineage.discovered_time = 101;
        EXPECT_EQ(catalog_->upsertRemoteConnectorCapabilityCatalogEntry(stale_lineage, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }

    CatalogManager::RemoteConnectorCapabilityCatalogInfo capability_out{};
    ASSERT_EQ(catalog_->getRemoteConnectorCapabilityCatalogEntry(capability.capability_id, capability_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(capability_out.capability_key, capability.capability_key);

    std::vector<CatalogManager::RemoteConnectorCapabilityCatalogInfo> capability_rows;
    ASSERT_EQ(catalog_->listRemoteConnectorCapabilityCatalogEntries(connector.remote_connector_id,
                                                                    capability_rows,
                                                                    &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(capability_rows.size(), 1u);

    CatalogManager::RemoteErrorCatalogInfo remote_error{};
    remote_error.remote_error_id = generateUuidV7();
    remote_error.remote_connector_id = connector.remote_connector_id;
    remote_error.error_class = CatalogManager::RemoteErrorClass::EXECUTION;
    remote_error.has_remote_code = true;
    remote_error.remote_code = "42P01";
    remote_error.mapped_code = "SB_REMOTE_EXEC";
    remote_error.message_text =
        "postgres://alice:TopSecret@db01.local:5432/app?password=hunter2&token=abc "
        "password=s3cr3t payload={\"secret\":\"xyz\"}";
    remote_error.first_seen_time = 110;
    remote_error.last_seen_time = 110;
    remote_error.occurrence_count = 1;
    remote_error.is_open = true;
    ASSERT_EQ(catalog_->upsertRemoteErrorCatalogEntry(remote_error, &ctx), Status::OK) << ctx.message;

    CatalogManager::RemoteMetadataSnapshotCatalogInfo snapshot{};
    snapshot.snapshot_id = generateUuidV7();
    snapshot.remote_connector_id = connector.remote_connector_id;
    snapshot.snapshot_seq = 1;
    snapshot.snapshot_kind = CatalogManager::RemoteSnapshotKind::FULL;
    snapshot.snapshot_status = CatalogManager::RemoteSnapshotStatus::COMPLETE;
    snapshot.has_engine_version_text = true;
    snapshot.engine_version_text = "18.0";
    snapshot.object_count = 1;
    snapshot.column_count = 1;
    snapshot.has_catalog_hash = true;
    snapshot.catalog_hash = 0x1234u;
    snapshot.started_time = 111;
    snapshot.has_completed_time = true;
    snapshot.completed_time = 112;
    snapshot.has_error_id = true;
    snapshot.error_id = remote_error.remote_error_id;
    ASSERT_EQ(catalog_->upsertRemoteMetadataSnapshotCatalogEntry(snapshot, &ctx), Status::OK)
        << ctx.message;
    {
        ErrorContext fail_ctx;
        auto immutable_snapshot = snapshot;
        immutable_snapshot.object_count = 2;
        immutable_snapshot.column_count = 2;
        EXPECT_EQ(catalog_->upsertRemoteMetadataSnapshotCatalogEntry(immutable_snapshot, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }

    CatalogManager::RemoteMetadataObjectCatalogInfo remote_object{};
    remote_object.remote_object_id = generateUuidV7();
    remote_object.snapshot_id = snapshot.snapshot_id;
    remote_object.remote_path = "public.orders";
    remote_object.has_remote_schema_name = true;
    remote_object.remote_schema_name = "public";
    remote_object.remote_object_name = "orders";
    remote_object.remote_object_kind = CatalogManager::RemoteObjectKind::TABLE;
    remote_object.remote_signature = 0xAA55u;
    remote_object.has_definition_json = true;
    remote_object.definition_json = "{\"kind\":\"table\"}";
    remote_object.has_mapped_local_schema_id = true;
    remote_object.mapped_local_schema_id = schema_id_;
    ASSERT_EQ(catalog_->upsertRemoteMetadataObjectCatalogEntry(remote_object, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RemoteMetadataColumnCatalogInfo remote_column{};
    remote_column.remote_column_id = generateUuidV7();
    remote_column.remote_object_id = remote_object.remote_object_id;
    remote_column.ordinal_position = 1;
    remote_column.column_name = "id";
    remote_column.remote_type_name = "bigint";
    remote_column.is_nullable = false;
    remote_column.has_default_expr_text = true;
    remote_column.default_expr_text = "nextval('orders_id_seq')";
    remote_column.has_precision_value = true;
    remote_column.precision_value = 19;
    remote_column.has_charset_name = true;
    remote_column.charset_name = "UTF8";
    remote_column.has_collation_name = true;
    remote_column.collation_name = "UNICODE";
    remote_column.has_extra_json = true;
    remote_column.extra_json = "{\"pk\":true}";
    ASSERT_EQ(catalog_->upsertRemoteMetadataColumnCatalogEntry(remote_column, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RemoteSchemaMappingCatalogInfo mapping{};
    mapping.schema_mapping_id = generateUuidV7();
    mapping.remote_connector_id = connector.remote_connector_id;
    mapping.mapping_name = "public_mapping";
    mapping.remote_schema_pattern = "^public$";
    mapping.local_schema_id = schema_id_;
    mapping.mapping_mode = CatalogManager::RemoteSchemaMappingMode::REGEX;
    mapping.include_object_kinds = "TABLE,VIEW";
    mapping.has_exclude_object_patterns = true;
    mapping.exclude_object_patterns = "^tmp_.*";
    mapping.has_rename_rule_json = true;
    mapping.rename_rule_json = "{}";
    mapping.has_last_snapshot_id = true;
    mapping.last_snapshot_id = snapshot.snapshot_id;
    ASSERT_EQ(catalog_->upsertRemoteSchemaMappingCatalogEntry(mapping, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RemotePassthroughPolicyCatalogInfo policy{};
    policy.remote_policy_id = generateUuidV7();
    policy.remote_connector_id = connector.remote_connector_id;
    policy.allow_query = true;
    policy.allow_dml = true;
    policy.allow_ddl = false;
    policy.allow_admin = false;
    policy.allow_procedural = false;
    policy.allow_join_local_txn = true;
    policy.max_rows = 100000;
    policy.max_bytes = 1024 * 1024;
    policy.timeout_ms = 10000;
    policy.has_required_capabilities = true;
    policy.required_capabilities = "query.pushdown.join";
    policy.audit_level = "detailed";
    ASSERT_EQ(catalog_->upsertRemotePassthroughPolicyCatalogEntry(policy, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::SessionInfo session{};
    ASSERT_EQ(catalog_->createSession(system_user_id_, ID{}, "native", session, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RuntimeTransactionCatalogInfo tx{};
    tx.txid = 9001;
    tx.tx_uuid = generateUuidV7();
    tx.database_id = generateUuidV7();
    tx.session_id = session.session_id;
    tx.user_id = system_user_id_;
    tx.emulation_engine = CatalogManager::EmulationEngine::NATIVE;
    tx.isolation_level = 0;
    tx.state = CatalogManager::RuntimeTransactionState::IN_PROGRESS;
    tx.start_time = 120;
    ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx, &ctx), Status::OK) << ctx.message;

    CatalogManager::RemotePreparedStatementCatalogInfo prepared{};
    prepared.remote_prepared_id = generateUuidV7();
    prepared.remote_connector_id = connector.remote_connector_id;
    prepared.session_id = session.session_id;
    prepared.statement_name = "orders_by_id";
    prepared.statement_fingerprint = 0xABCDu;
    prepared.command_text = "select * from orders where id = $1";
    prepared.has_parameter_signature = true;
    prepared.parameter_signature = 0x42u;
    prepared.remote_handle = "ps_001";
    prepared.created_time = 130;
    prepared.last_used_time = 131;
    prepared.has_expires_time = true;
    prepared.expires_time = 1000;
    ASSERT_EQ(catalog_->upsertRemotePreparedStatementCatalogEntry(prepared, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::RemoteTxnBindingCatalogInfo binding{};
    binding.remote_txn_binding_id = generateUuidV7();
    binding.remote_connector_id = connector.remote_connector_id;
    binding.session_id = session.session_id;
    binding.txid = tx.txid;
    binding.txn_mode = CatalogManager::RemoteTxnMode::JOINED;
    binding.txn_state = CatalogManager::RemoteTxnState::ACTIVE;
    binding.remote_txn_token = "token-1";
    binding.begin_time = 140;
    binding.has_last_heartbeat = true;
    binding.last_heartbeat = 141;
    binding.has_last_error_id = true;
    binding.last_error_id = remote_error.remote_error_id;
    ASSERT_EQ(catalog_->upsertRemoteTxnBindingCatalogEntry(binding, &ctx), Status::OK) << ctx.message;
    {
        ErrorContext fail_ctx;
        auto binding_with_invalid_terminal = binding;
        binding_with_invalid_terminal.has_terminal_time = true;
        binding_with_invalid_terminal.terminal_time = 142;
        EXPECT_EQ(catalog_->upsertRemoteTxnBindingCatalogEntry(binding_with_invalid_terminal, &fail_ctx),
                  Status::INVALID_ARGUMENT);
    }

    CatalogManager::RemoteExecutionAuditCatalogInfo audit{};
    audit.remote_exec_audit_id = generateUuidV7();
    audit.remote_connector_id = connector.remote_connector_id;
    audit.session_id = session.session_id;
    audit.has_txid = true;
    audit.txid = tx.txid;
    audit.request_id = generateUuidV7();
    audit.operation_class = CatalogManager::RemoteOperationClass::QUERY;
    audit.statement_fingerprint = prepared.statement_fingerprint;
    audit.used_prepared = true;
    audit.txn_mode = CatalogManager::RemoteTxnMode::JOINED;
    audit.exec_status = CatalogManager::RemoteExecStatus::FAILED;
    audit.rows_returned = 0;
    audit.rows_affected = 0;
    audit.bytes_in = 128;
    audit.bytes_out = 64;
    audit.latency_ms = 7;
    audit.started_time = 150;
    audit.finished_time = 151;
    audit.has_error_id = true;
    audit.error_id = remote_error.remote_error_id;
    ASSERT_EQ(catalog_->upsertRemoteExecutionAuditCatalogEntry(audit, &ctx), Status::OK) << ctx.message;
    {
        ErrorContext fail_ctx;
        auto duplicate_audit = audit;
        duplicate_audit.latency_ms = 8;
        EXPECT_EQ(catalog_->upsertRemoteExecutionAuditCatalogEntry(duplicate_audit, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }

    CatalogManager::RemoteMetadataObjectCatalogInfo remote_object_out{};
    ASSERT_EQ(catalog_->getRemoteMetadataObjectCatalogEntry(remote_object.remote_object_id,
                                                            remote_object_out,
                                                            &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(remote_object_out.remote_path, remote_object.remote_path);

    std::vector<CatalogManager::RemoteMetadataObjectCatalogInfo> remote_object_rows;
    ASSERT_EQ(catalog_->listRemoteMetadataObjectCatalogEntries(snapshot.snapshot_id,
                                                               remote_object_rows,
                                                               &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(remote_object_rows.size(), 1u);

    CatalogManager::RemoteMetadataColumnCatalogInfo remote_column_out{};
    ASSERT_EQ(catalog_->getRemoteMetadataColumnCatalogEntry(remote_column.remote_column_id,
                                                            remote_column_out,
                                                            &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(remote_column_out.column_name, remote_column.column_name);

    std::vector<CatalogManager::RemoteMetadataColumnCatalogInfo> remote_column_rows;
    ASSERT_EQ(catalog_->listRemoteMetadataColumnCatalogEntries(remote_object.remote_object_id,
                                                               remote_column_rows,
                                                               &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(remote_column_rows.size(), 1u);

    CatalogManager::RemoteSchemaMappingCatalogInfo mapping_out{};
    ASSERT_EQ(catalog_->getRemoteSchemaMappingCatalogEntry(mapping.schema_mapping_id, mapping_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(mapping_out.mapping_name, mapping.mapping_name);

    std::vector<CatalogManager::RemoteSchemaMappingCatalogInfo> mapping_rows;
    ASSERT_EQ(catalog_->listRemoteSchemaMappingCatalogEntries(connector.remote_connector_id,
                                                              mapping_rows,
                                                              &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(mapping_rows.size(), 1u);

    CatalogManager::RemotePassthroughPolicyCatalogInfo policy_out{};
    ASSERT_EQ(catalog_->getRemotePassthroughPolicyCatalogEntry(policy.remote_policy_id, policy_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(policy_out.audit_level, policy.audit_level);

    std::vector<CatalogManager::RemotePassthroughPolicyCatalogInfo> policy_rows;
    ASSERT_EQ(catalog_->listRemotePassthroughPolicyCatalogEntries(connector.remote_connector_id,
                                                                  policy_rows,
                                                                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(policy_rows.size(), 1u);

    CatalogManager::RemotePreparedStatementCatalogInfo prepared_out{};
    ASSERT_EQ(catalog_->getRemotePreparedStatementCatalogEntry(prepared.remote_prepared_id,
                                                               prepared_out,
                                                               &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(prepared_out.statement_name, prepared.statement_name);

    std::vector<CatalogManager::RemotePreparedStatementCatalogInfo> prepared_rows;
    ASSERT_EQ(catalog_->listRemotePreparedStatementCatalogEntries(session.session_id,
                                                                  prepared_rows,
                                                                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(prepared_rows.size(), 1u);

    CatalogManager::RemoteTxnBindingCatalogInfo binding_out{};
    ASSERT_EQ(catalog_->getRemoteTxnBindingCatalogEntry(binding.remote_txn_binding_id, binding_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(binding_out.txid, binding.txid);

    std::vector<CatalogManager::RemoteTxnBindingCatalogInfo> binding_rows;
    ASSERT_EQ(catalog_->listRemoteTxnBindingCatalogEntries(connector.remote_connector_id,
                                                           binding_rows,
                                                           &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(binding_rows.size(), 1u);

    {
        ErrorContext fail_ctx;
        auto tx_terminal_blocked = tx;
        tx_terminal_blocked.state = CatalogManager::RuntimeTransactionState::COMMITTED;
        tx_terminal_blocked.has_end_time = true;
        tx_terminal_blocked.end_time = 199;
        EXPECT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx_terminal_blocked, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }

    auto terminal_binding = binding;
    terminal_binding.txn_state = CatalogManager::RemoteTxnState::COMMITTED;
    terminal_binding.has_terminal_time = true;
    terminal_binding.terminal_time = 200;
    terminal_binding.last_heartbeat = 199;
    ASSERT_EQ(catalog_->upsertRemoteTxnBindingCatalogEntry(terminal_binding, &ctx), Status::OK)
        << ctx.message;
    binding = terminal_binding;

    CatalogManager::RuntimeTransactionCatalogInfo tx_terminal = tx;
    tx_terminal.state = CatalogManager::RuntimeTransactionState::COMMITTED;
    tx_terminal.has_end_time = true;
    tx_terminal.end_time = 199;
    ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx_terminal, &ctx), Status::OK)
        << ctx.message;

    {
        ErrorContext fail_ctx;
        auto active_binding_again = binding;
        active_binding_again.txn_state = CatalogManager::RemoteTxnState::ACTIVE;
        active_binding_again.has_terminal_time = false;
        active_binding_again.terminal_time = 0;
        active_binding_again.last_heartbeat = 198;
        EXPECT_EQ(catalog_->upsertRemoteTxnBindingCatalogEntry(active_binding_again, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }

    {
        ErrorContext fail_ctx;
        auto mutate_terminal = binding;
        mutate_terminal.last_heartbeat = 201;
        EXPECT_EQ(catalog_->upsertRemoteTxnBindingCatalogEntry(mutate_terminal, &fail_ctx),
                  Status::CONSTRAINT_VIOLATION);
    }

    CatalogManager::RemoteExecutionAuditCatalogInfo audit_out{};
    ASSERT_EQ(catalog_->getRemoteExecutionAuditCatalogEntry(audit.remote_exec_audit_id, audit_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(audit_out.exec_status, CatalogManager::RemoteExecStatus::FAILED);

    std::vector<CatalogManager::RemoteExecutionAuditCatalogInfo> audit_rows;
    ASSERT_EQ(catalog_->listRemoteExecutionAuditCatalogEntries(connector.remote_connector_id,
                                                               audit_rows,
                                                               &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(audit_rows.size(), 1u);

    CatalogManager::RemoteErrorCatalogInfo remote_error_out{};
    ASSERT_EQ(catalog_->getRemoteErrorCatalogEntry(remote_error.remote_error_id, remote_error_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(remote_error_out.remote_code, remote_error.remote_code);
    EXPECT_EQ(remote_error_out.message_text.find("TopSecret"), std::string::npos);
    EXPECT_EQ(remote_error_out.message_text.find("hunter2"), std::string::npos);
    EXPECT_EQ(remote_error_out.message_text.find("token=abc"), std::string::npos);
    EXPECT_EQ(remote_error_out.message_text.find("s3cr3t"), std::string::npos);
    EXPECT_EQ(remote_error_out.message_text.find("\"xyz\""), std::string::npos);
    EXPECT_EQ(remote_error_out.message_text.find("db01.local"), std::string::npos);
    EXPECT_NE(remote_error_out.message_text.find("<redacted>"), std::string::npos);
    EXPECT_NE(remote_error_out.message_text.find("<endpoint>"), std::string::npos);

    std::vector<CatalogManager::RemoteErrorCatalogInfo> remote_error_rows;
    ASSERT_EQ(catalog_->listRemoteErrorCatalogEntries(connector.remote_connector_id,
                                                      remote_error_rows,
                                                      &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(remote_error_rows.size(), 1u);
    EXPECT_EQ(remote_error_rows.front().message_text.find("TopSecret"), std::string::npos);
    EXPECT_NE(remote_error_rows.front().message_text.find("<redacted>"), std::string::npos);

    ASSERT_EQ(catalog_->deleteRemoteExecutionAuditCatalogEntry(audit.remote_exec_audit_id, &ctx),
              Status::CONSTRAINT_VIOLATION)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemoteTxnBindingCatalogEntry(binding.remote_txn_binding_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemotePreparedStatementCatalogEntry(prepared.remote_prepared_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemotePassthroughPolicyCatalogEntry(policy.remote_policy_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemoteSchemaMappingCatalogEntry(mapping.schema_mapping_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemoteMetadataColumnCatalogEntry(remote_column.remote_column_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemoteMetadataObjectCatalogEntry(remote_object.remote_object_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemoteMetadataSnapshotCatalogEntry(snapshot.snapshot_id, &ctx),
              Status::CONSTRAINT_VIOLATION)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemoteErrorCatalogEntry(remote_error.remote_error_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemoteConnectorCapabilityCatalogEntry(capability.capability_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRemoteConnectorCatalogEntry(connector.remote_connector_id, &ctx), Status::OK)
        << ctx.message;

    EXPECT_EQ(catalog_->getRemoteConnectorCatalogEntry(connector.remote_connector_id, connector_out, &ctx),
              Status::NOT_FOUND);
}
