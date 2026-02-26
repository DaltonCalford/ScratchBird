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

class CatalogSblrArtifactExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_sblr_artifact_extension_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
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

TEST_F(CatalogSblrArtifactExtensionContractTest, SblrArtifactCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::SblrModuleCatalogInfo module{};
    module.module_id = generateUuidV7();
    module.sblr_checksum = 1001;
    module.feature_key = "select_projection";
    module.result_shape_id = "shape_rowset";
    module.payload_schema_id = "schema_v3";
    module.container_blob_id = generateUuidV7();
    module.normalization_evidence_hash = 0xabcddcbaU;
    module.statement_norm_count = 0;
    module.capability_profile_version = 5;
    module.created_txid = 101;
    ASSERT_EQ(catalog_->upsertSblrModuleCatalogEntry(module, &ctx), Status::OK) << ctx.message;

    CatalogManager::SblrModuleCatalogInfo module_dup = module;
    module_dup.module_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertSblrModuleCatalogEntry(module_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::SblrStatementNormCatalogInfo norm{};
    norm.module_id = module.module_id;
    norm.statement_id = generateUuidV7();
    norm.statement_order = 0;
    norm.feature_key = module.feature_key;
    norm.ast_family = "dml_select";
    norm.normalization_rule_set_id = 1;
    norm.clause_presence_mask_lo = 0x1U;
    norm.clause_presence_mask_hi = 0x0U;
    norm.clause_order_checksum = 0x1234U;
    norm.alias_rewrite_flags = 0;
    norm.created_txid = 102;
    ASSERT_EQ(catalog_->upsertSblrStatementNormCatalogEntry(norm, &ctx), Status::OK) << ctx.message;

    CatalogManager::SblrStatementNormCatalogInfo norm_dup_order = norm;
    norm_dup_order.statement_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertSblrStatementNormCatalogEntry(norm_dup_order, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::SblrPlanCatalogInfo plan{};
    plan.plan_id = generateUuidV7();
    plan.module_id = module.module_id;
    plan.catalog_epoch = 10;
    plan.security_epoch = 20;
    plan.normalization_evidence_hash = module.normalization_evidence_hash;
    plan.plan_checksum = 0x1111222233334444ULL;
    plan.dependency_count = 1;
    plan.plan_blob_id = generateUuidV7();
    plan.created_txid = 103;
    EXPECT_EQ(catalog_->upsertSblrPlanCatalogEntry(plan, &ctx), Status::CONSTRAINT_VIOLATION);

    module.statement_norm_count = 1;
    ASSERT_EQ(catalog_->upsertSblrModuleCatalogEntry(module, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(catalog_->upsertSblrPlanCatalogEntry(plan, &ctx), Status::OK) << ctx.message;

    CatalogManager::SblrPlanCatalogInfo plan_dup = plan;
    plan_dup.plan_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertSblrPlanCatalogEntry(plan_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::SblrPlanDependencyCatalogInfo dep{};
    dep.plan_id = plan.plan_id;
    dep.object_id = generateUuidV7();
    dep.object_kind = CatalogManager::ObjectType::TABLE;
    ASSERT_EQ(catalog_->upsertSblrPlanDependencyCatalogEntry(dep, &ctx), Status::OK) << ctx.message;

    CatalogManager::SblrPlanDependencyCatalogInfo dep_dup = dep;
    EXPECT_EQ(catalog_->upsertSblrPlanDependencyCatalogEntry(dep_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::SblrArtifactCatalogInfo artifact{};
    artifact.artifact_id = generateUuidV7();
    artifact.module_id = module.module_id;
    artifact.plan_id = plan.plan_id;
    artifact.object_uuid = generateUuidV7();
    artifact.canonical_sblr_hash =
        "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    artifact.target_platform = "linux_x86_64";
    artifact.target_triple = "x86_64-unknown-linux-gnu";
    artifact.cpu_feature_profile = "generic";
    artifact.native_abi_version = "sb_abi_v1";
    artifact.compiler_id = "sb_native";
    artifact.compiler_identity = "scratchbird_jit";
    artifact.compiler_version = "1.0.0";
    artifact.optimization_profile = "O2";
    artifact.security_policy_version = 1;
    artifact.artifact_state = CatalogManager::SblrArtifactState::READY;
    artifact.binary_blob_id = generateUuidV7();
    artifact.hash_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    artifact.catalog_epoch = plan.catalog_epoch;
    artifact.security_epoch = plan.security_epoch;
    artifact.created_txid = 104;
    ASSERT_EQ(catalog_->upsertSblrArtifactCatalogEntry(artifact, &ctx), Status::OK) << ctx.message;

    CatalogManager::SblrArtifactCatalogInfo artifact_dup = artifact;
    artifact_dup.artifact_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertSblrArtifactCatalogEntry(artifact_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::SblrArtifactStatsCatalogInfo stats{};
    stats.artifact_id = artifact.artifact_id;
    stats.execution_count = 3;
    stats.execution_cpu_us = 500;
    stats.last_used_at = 123456;
    stats.fallback_count = 1;
    stats.load_failure_count = 0;
    ASSERT_EQ(catalog_->upsertSblrArtifactStatsCatalogEntry(stats, &ctx), Status::OK) << ctx.message;

    CatalogManager::SblrArtifactStatsCatalogInfo stats_out{};
    ASSERT_EQ(catalog_->getSblrArtifactStatsCatalogEntry(artifact.artifact_id, stats_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(stats_out.execution_count, 3u);

    CatalogManager::SblrCompilerTargetCatalogInfo target{};
    target.target_name = "linux_x86_64";
    target.abi_name = "gnu";
    target.enabled = true;
    target.min_compiler_version = "1.0.0";
    target.policy_flags = 0x7;
    ASSERT_EQ(catalog_->upsertSblrCompilerTargetCatalogEntry(target, &ctx), Status::OK) << ctx.message;

    CatalogManager::SblrCompilerTargetCatalogInfo target_out{};
    ASSERT_EQ(catalog_->getSblrCompilerTargetCatalogEntry("linux_x86_64", target_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(target_out.abi_name, "gnu");

    CatalogManager::SblrCompileQueueCatalogInfo queue{};
    queue.queue_id = generateUuidV7();
    queue.module_id = module.module_id;
    queue.target_platform = "linux_x86_64";
    queue.queue_state = CatalogManager::SblrQueueState::QUEUED;
    queue.priority = 5;
    queue.attempt_count = 0;
    ASSERT_EQ(catalog_->upsertSblrCompileQueueCatalogEntry(queue, &ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::SblrCompileQueueCatalogInfo> queues;
    ASSERT_EQ(catalog_->listSblrCompileQueueCatalogEntries(module.module_id, queues, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(queues.size(), 1u);

    ASSERT_EQ(catalog_->deleteSblrCompileQueueCatalogEntry(queue.queue_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->deleteSblrCompileQueueCatalogEntry(queue.queue_id, &ctx), Status::NOT_FOUND);

    ASSERT_EQ(catalog_->deleteSblrArtifactStatsCatalogEntry(artifact.artifact_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteSblrArtifactCatalogEntry(artifact.artifact_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteSblrPlanDependencyCatalogEntry(dep.plan_id, dep.object_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteSblrPlanCatalogEntry(plan.plan_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteSblrStatementNormCatalogEntry(norm.module_id, norm.statement_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteSblrCompilerTargetCatalogEntry(target.target_name, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteSblrModuleCatalogEntry(module.module_id, &ctx), Status::OK) << ctx.message;
}
