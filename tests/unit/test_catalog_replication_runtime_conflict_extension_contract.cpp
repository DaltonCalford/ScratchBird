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

class CatalogReplicationRuntimeConflictExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};
    ID system_user_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_replication_runtime_conflict_extension_contract_" +
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

        ASSERT_EQ(catalog_->createSchema("cat027_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;

        system_user_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{});
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

TEST_F(CatalogReplicationRuntimeConflictExtensionContractTest,
       ReplicationRuntimeConflictCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::PublicationCatalogInfo publication{};
    publication.publication_id = generateUuidV7();
    publication.publication_name = "cat027_publication";
    publication.owner_id = system_user_id_;
    publication.publish_insert = true;
    publication.publish_update = true;
    publication.publish_delete = true;
    publication.publish_truncate = true;
    publication.publish_via_partition_root = false;
    ASSERT_EQ(catalog_->upsertPublicationCatalogEntry(publication, &ctx), Status::OK) << ctx.message;

    CatalogManager::SubscriptionCatalogInfo subscription{};
    subscription.subscription_id = generateUuidV7();
    subscription.subscription_name = "cat027_subscription";
    subscription.owner_id = system_user_id_;
    subscription.enabled = true;
    subscription.sync_commit = true;
    subscription.copy_data = true;
    subscription.create_slot = true;
    subscription.refresh_on_start = true;
    ASSERT_EQ(catalog_->upsertSubscriptionCatalogEntry(subscription, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationChannelCatalogInfo channel{};
    channel.replication_channel_id = generateUuidV7();
    channel.channel_name = "primary_channel";
    channel.direction = CatalogManager::ReplicationDirection::ONE_WAY;
    channel.channel_state = CatalogManager::ReplicationChannelState::INIT;
    channel.mode_version = 1;
    channel.has_publication_id = true;
    channel.publication_id = publication.publication_id;
    channel.has_subscription_id = true;
    channel.subscription_id = subscription.subscription_id;
    channel.ddl_policy = CatalogManager::ReplicationDdlPolicy::SAFE_ONLY;
    channel.conflict_policy = CatalogManager::ReplicationConflictPolicy::MANUAL_REQUIRED;
    channel.max_retry_count = 5;
    channel.retry_backoff_base_ms = 10;
    channel.retry_backoff_max_ms = 500;
    channel.lag_warn_ms = 1000;
    channel.lag_critical_ms = 5000;
    channel.batch_max_txn = 128;
    channel.batch_max_bytes = 4 * 1024 * 1024;
    channel.split_brain_fence_enabled = true;
    channel.split_brain_detect_window_ms = 3000;
    channel.created_by_id = system_user_id_;
    ASSERT_EQ(catalog_->upsertReplicationChannelCatalogEntry(channel, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationChannelCatalogInfo channel_out{};
    ASSERT_EQ(catalog_->getReplicationChannelCatalogEntry(channel.replication_channel_id, channel_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(channel_out.channel_name, channel.channel_name);

    std::vector<CatalogManager::ReplicationChannelCatalogInfo> channel_rows;
    ASSERT_EQ(catalog_->listReplicationChannelCatalogEntries(channel_rows, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(channel_rows.size(), 1u);

    CatalogManager::ReplicationOriginCatalogInfo origin_pub{};
    origin_pub.origin_id = generateUuidV7();
    origin_pub.origin_name = "origin_pub";
    origin_pub.origin_scope = "cluster_a";
    origin_pub.origin_priority = 1;
    ASSERT_EQ(catalog_->upsertReplicationOriginCatalogEntry(origin_pub, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationOriginCatalogInfo origin_sub{};
    origin_sub.origin_id = generateUuidV7();
    origin_sub.origin_name = "origin_sub";
    origin_sub.origin_scope = "cluster_a";
    origin_sub.origin_priority = 2;
    ASSERT_EQ(catalog_->upsertReplicationOriginCatalogEntry(origin_sub, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationChannelMemberCatalogInfo publisher{};
    publisher.channel_member_id = generateUuidV7();
    publisher.replication_channel_id = channel.replication_channel_id;
    publisher.member_name = "publisher_member";
    publisher.member_role = CatalogManager::ReplicationMemberRole::PUBLISHER;
    publisher.local_endpoint = true;
    publisher.priority_rank = 1;
    publisher.origin_id = origin_pub.origin_id;
    ASSERT_EQ(catalog_->upsertReplicationChannelMemberCatalogEntry(publisher, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ReplicationChannelMemberCatalogInfo subscriber{};
    subscriber.channel_member_id = generateUuidV7();
    subscriber.replication_channel_id = channel.replication_channel_id;
    subscriber.member_name = "subscriber_member";
    subscriber.member_role = CatalogManager::ReplicationMemberRole::SUBSCRIBER;
    subscriber.local_endpoint = false;
    subscriber.priority_rank = 2;
    subscriber.origin_id = origin_sub.origin_id;
    ASSERT_EQ(catalog_->upsertReplicationChannelMemberCatalogEntry(subscriber, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ReplicationErrorCatalogInfo repl_error{};
    repl_error.replication_error_id = generateUuidV7();
    repl_error.replication_channel_id = channel.replication_channel_id;
    repl_error.source_component = "apply_worker";
    repl_error.source_code = "E_DUP_KEY";
    repl_error.message_text = "duplicate key during apply";
    repl_error.recoverable = true;
    repl_error.has_retry_after_ms = true;
    repl_error.retry_after_ms = 250;
    repl_error.first_seen_time = 1000;
    repl_error.last_seen_time = 1000;
    repl_error.occurrence_count = 1;
    repl_error.is_open = true;
    ASSERT_EQ(catalog_->upsertReplicationErrorCatalogEntry(repl_error, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationCursorCatalogInfo cursor{};
    cursor.replication_cursor_id = generateUuidV7();
    cursor.replication_channel_id = channel.replication_channel_id;
    cursor.channel_member_id = publisher.channel_member_id;
    cursor.cursor_name = "pub_cursor";
    cursor.cursor_state = CatalogManager::ReplicationCursorState::ACTIVE;
    cursor.cursor_payload = "{\"lsn\":\"0/16B6D30\"}";
    cursor.source_commit_seq = 100;
    cursor.has_source_commit_time = true;
    cursor.source_commit_time = 1100;
    cursor.applied_commit_seq = 95;
    cursor.has_applied_time = true;
    cursor.applied_time = 1110;
    cursor.lag_ms = 50;
    cursor.has_heartbeat_time = true;
    cursor.heartbeat_time = 1120;
    cursor.has_last_error_id = true;
    cursor.last_error_id = repl_error.replication_error_id;
    ASSERT_EQ(catalog_->upsertReplicationCursorCatalogEntry(cursor, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationOriginProgressCatalogInfo progress{};
    progress.origin_progress_id = generateUuidV7();
    progress.replication_channel_id = channel.replication_channel_id;
    progress.target_member_id = subscriber.channel_member_id;
    progress.origin_id = origin_pub.origin_id;
    progress.max_applied_commit_seq = 95;
    progress.has_max_applied_time = true;
    progress.max_applied_time = 1110;
    ASSERT_EQ(catalog_->upsertReplicationOriginProgressCatalogEntry(progress, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ReplicationTxnBatchCatalogInfo batch{};
    batch.replication_batch_id = generateUuidV7();
    batch.replication_channel_id = channel.replication_channel_id;
    batch.source_member_id = publisher.channel_member_id;
    batch.origin_id = origin_pub.origin_id;
    batch.source_txn_id = "txn-100";
    batch.source_commit_seq = 100;
    batch.source_commit_time = 1100;
    batch.txn_state = CatalogManager::ReplicationTxnState::RECEIVED;
    batch.change_count = 42;
    batch.payload_bytes = 8096;
    batch.batch_checksum = 0xAABBCCDDu;
    batch.received_time = 1130;
    batch.retry_count = 0;
    ASSERT_EQ(catalog_->upsertReplicationTxnBatchCatalogEntry(batch, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationApplyLogCatalogInfo apply_log{};
    apply_log.replication_apply_log_id = generateUuidV7();
    apply_log.replication_batch_id = batch.replication_batch_id;
    apply_log.target_member_id = subscriber.channel_member_id;
    apply_log.apply_order = 1;
    apply_log.txn_state = CatalogManager::ReplicationTxnState::APPLIED;
    apply_log.apply_start_time = 1140;
    apply_log.has_apply_end_time = true;
    apply_log.apply_end_time = 1145;
    apply_log.has_applied_commit_seq = true;
    apply_log.applied_commit_seq = 100;
    apply_log.rows_inserted = 10;
    apply_log.rows_updated = 20;
    apply_log.rows_deleted = 12;
    apply_log.ddl_count = 0;
    ASSERT_EQ(catalog_->upsertReplicationApplyLogCatalogEntry(apply_log, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationRetryQueueCatalogInfo retry{};
    retry.replication_retry_id = generateUuidV7();
    retry.replication_batch_id = batch.replication_batch_id;
    retry.retry_state = CatalogManager::ReplicationRetryState::QUEUED;
    retry.retry_count = 1;
    retry.next_retry_time = 1200;
    retry.has_last_error_id = true;
    retry.last_error_id = repl_error.replication_error_id;
    ASSERT_EQ(catalog_->upsertReplicationRetryQueueCatalogEntry(retry, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationRetryQueueCatalogInfo invalid_retry = retry;
    invalid_retry.replication_retry_id = generateUuidV7();
    invalid_retry.retry_state = CatalogManager::ReplicationRetryState::DEAD_LETTER;
    invalid_retry.has_dead_letter_reason = false;
    EXPECT_EQ(catalog_->upsertReplicationRetryQueueCatalogEntry(invalid_retry, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ReplicationConflictCatalogInfo conflict{};
    conflict.replication_conflict_id = generateUuidV7();
    conflict.replication_channel_id = channel.replication_channel_id;
    conflict.replication_batch_id = batch.replication_batch_id;
    conflict.conflict_kind = CatalogManager::ReplicationConflictKind::UPDATE_UPDATE;
    conflict.source_origin_id = origin_pub.origin_id;
    conflict.has_target_origin_id = true;
    conflict.target_origin_id = origin_sub.origin_id;
    conflict.target_object_id = db_->uuid();
    conflict.source_commit_seq = 100;
    conflict.source_payload = "{\"before\":1,\"after\":2}";
    conflict.resolution_state = CatalogManager::ReplicationResolutionState::OPEN;
    ASSERT_EQ(catalog_->upsertReplicationConflictCatalogEntry(conflict, &ctx), Status::OK) << ctx.message;

    CatalogManager::ReplicationSplitBrainEventCatalogInfo split_brain{};
    split_brain.replication_split_brain_id = generateUuidV7();
    split_brain.replication_channel_id = channel.replication_channel_id;
    split_brain.event_kind = CatalogManager::ReplicationEventKind::SPLIT_BRAIN_DETECTED;
    split_brain.detected_time = 1300;
    split_brain.detection_payload = "{\"partition\":\"A|B\"}";
    split_brain.fence_applied = true;
    split_brain.fence_cleared = false;
    ASSERT_EQ(catalog_->upsertReplicationSplitBrainEventCatalogEntry(split_brain, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ReplicationCursorCatalogInfo cursor_out{};
    ASSERT_EQ(catalog_->getReplicationCursorCatalogEntry(cursor.replication_cursor_id, cursor_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(cursor_out.cursor_name, cursor.cursor_name);

    std::vector<CatalogManager::ReplicationOriginProgressCatalogInfo> progress_rows;
    ASSERT_EQ(catalog_->listReplicationOriginProgressCatalogEntries(channel.replication_channel_id,
                                                                    progress_rows,
                                                                    &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(progress_rows.size(), 1u);

    std::vector<CatalogManager::ReplicationTxnBatchCatalogInfo> batch_rows;
    ASSERT_EQ(catalog_->listReplicationTxnBatchCatalogEntries(channel.replication_channel_id, batch_rows, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(batch_rows.size(), 1u);

    std::vector<CatalogManager::ReplicationApplyLogCatalogInfo> apply_rows;
    ASSERT_EQ(catalog_->listReplicationApplyLogCatalogEntries(batch.replication_batch_id, apply_rows, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(apply_rows.size(), 1u);

    std::vector<CatalogManager::ReplicationRetryQueueCatalogInfo> retry_rows;
    ASSERT_EQ(catalog_->listReplicationRetryQueueCatalogEntries(retry_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(retry_rows.size(), 1u);

    std::vector<CatalogManager::ReplicationConflictCatalogInfo> conflict_rows;
    ASSERT_EQ(catalog_->listReplicationConflictCatalogEntries(channel.replication_channel_id,
                                                              conflict_rows,
                                                              &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(conflict_rows.size(), 1u);

    std::vector<CatalogManager::ReplicationSplitBrainEventCatalogInfo> split_rows;
    ASSERT_EQ(catalog_->listReplicationSplitBrainEventCatalogEntries(channel.replication_channel_id,
                                                                     split_rows,
                                                                     &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(split_rows.size(), 1u);

    std::vector<CatalogManager::ReplicationErrorCatalogInfo> error_rows;
    ASSERT_EQ(catalog_->listReplicationErrorCatalogEntries(channel.replication_channel_id, error_rows, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(error_rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteReplicationSplitBrainEventCatalogEntry(
                  split_brain.replication_split_brain_id, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationConflictCatalogEntry(conflict.replication_conflict_id, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationRetryQueueCatalogEntry(retry.replication_retry_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationApplyLogCatalogEntry(apply_log.replication_apply_log_id, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationTxnBatchCatalogEntry(batch.replication_batch_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationOriginProgressCatalogEntry(progress.origin_progress_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationCursorCatalogEntry(cursor.replication_cursor_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationErrorCatalogEntry(repl_error.replication_error_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationChannelMemberCatalogEntry(subscriber.channel_member_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationChannelMemberCatalogEntry(publisher.channel_member_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationOriginCatalogEntry(origin_sub.origin_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationOriginCatalogEntry(origin_pub.origin_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteReplicationChannelCatalogEntry(channel.replication_channel_id, &ctx), Status::OK)
        << ctx.message;

    EXPECT_EQ(catalog_->getReplicationChannelCatalogEntry(channel.replication_channel_id, channel_out, &ctx),
              Status::NOT_FOUND);
}
