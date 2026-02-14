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

class CatalogSchedulerExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_scheduler_extension_contract_" +
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

TEST_F(CatalogSchedulerExtensionContractTest, SchedulerExtensionCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::JobTypeCatalogInfo job_type{};
    job_type.job_type_id = generateUuidV7();
    job_type.job_type_name = "etl_sync";
    job_type.job_group = CatalogManager::JobGroup::GROUP;
    job_type.is_system = false;
    job_type.is_enabled = true;
    job_type.default_timeout_ms = 60000;
    job_type.default_max_retries = 4;
    job_type.default_priority = 7;
    job_type.has_description = true;
    job_type.description = "synchronization jobs";
    ASSERT_EQ(catalog_->upsertJobTypeCatalogEntry(job_type, &ctx), Status::OK) << ctx.message;

    CatalogManager::JobTypeCatalogInfo dup_job_type = job_type;
    dup_job_type.job_type_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertJobTypeCatalogEntry(dup_job_type, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::JobTypeCatalogInfo job_type_out{};
    ASSERT_EQ(catalog_->getJobTypeCatalogEntry(job_type.job_type_id, job_type_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(job_type_out.job_type_name, "etl_sync");
    EXPECT_EQ(job_type_out.default_timeout_ms, 60000u);

    CatalogManager::JobTypeParamCatalogInfo type_param{};
    type_param.param_id = generateUuidV7();
    type_param.job_type_id = job_type.job_type_id;
    type_param.param_key = "batch_size";
    type_param.param_type = CatalogManager::JobParamType::INT;
    type_param.is_required = true;
    type_param.has_default_value = true;
    type_param.default_value = "1000";
    ASSERT_EQ(catalog_->upsertJobTypeParamCatalogEntry(type_param, &ctx), Status::OK) << ctx.message;

    CatalogManager::JobTypeParamCatalogInfo dup_type_param = type_param;
    dup_type_param.param_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertJobTypeParamCatalogEntry(dup_type_param, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::JobInfo job{};
    job.job_name = "nightly_etl";
    job.job_type = CatalogManager::JobType::SQL;
    job.job_sql = "select 1";
    job.state = CatalogManager::JobState::ENABLED;
    job.schedule_kind = CatalogManager::ScheduleKind::AT;
    job.created_by_user_uuid = catalog_->getSystemUserId(&ctx);
    ID job_id{};
    ASSERT_EQ(catalog_->createJob(job, job_id, &ctx), Status::OK) << ctx.message;

    CatalogManager::JobParamCatalogInfo job_param{};
    job_param.param_id = generateUuidV7();
    job_param.job_id = job_id;
    job_param.param_key = "tenant";
    job_param.param_type = CatalogManager::JobParamType::STRING;
    job_param.param_value = "tenant_a";
    ASSERT_EQ(catalog_->upsertJobParamCatalogEntry(job_param, &ctx), Status::OK) << ctx.message;

    CatalogManager::JobScheduleCatalogInfo invalid_schedule{};
    invalid_schedule.schedule_id = generateUuidV7();
    invalid_schedule.schedule_kind = CatalogManager::ScheduleKind::EVERY;
    invalid_schedule.has_interval_ms = false;
    EXPECT_EQ(catalog_->upsertJobScheduleCatalogEntry(invalid_schedule, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::JobScheduleCatalogInfo schedule{};
    schedule.schedule_id = generateUuidV7();
    schedule.schedule_kind = CatalogManager::ScheduleKind::CRON;
    schedule.has_cron_expr = true;
    schedule.cron_expr = "*/5 * * * *";
    ASSERT_EQ(catalog_->upsertJobScheduleCatalogEntry(schedule, &ctx), Status::OK) << ctx.message;

    CatalogManager::JobTypePolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.job_type_id = job_type.job_type_id;
    policy.max_concurrent = 3;
    ASSERT_EQ(catalog_->upsertJobTypePolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::JobTypePolicyCatalogInfo dup_policy = policy;
    dup_policy.policy_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertJobTypePolicyCatalogEntry(dup_policy, &ctx), Status::CONSTRAINT_VIOLATION);

    std::vector<CatalogManager::JobTypeCatalogInfo> job_type_rows;
    ASSERT_EQ(catalog_->listJobTypeCatalogEntries(job_type_rows, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(job_type_rows.empty());

    std::vector<CatalogManager::JobTypeParamCatalogInfo> type_param_rows;
    ASSERT_EQ(catalog_->listJobTypeParamCatalogEntries(job_type.job_type_id, type_param_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(type_param_rows.size(), 1u);

    std::vector<CatalogManager::JobParamCatalogInfo> job_param_rows;
    ASSERT_EQ(catalog_->listJobParamCatalogEntries(job_id, job_param_rows, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(job_param_rows.size(), 1u);

    std::vector<CatalogManager::JobScheduleCatalogInfo> schedule_rows;
    ASSERT_EQ(catalog_->listJobScheduleCatalogEntries(schedule_rows, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(schedule_rows.empty());

    std::vector<CatalogManager::JobTypePolicyCatalogInfo> policy_rows;
    ASSERT_EQ(catalog_->listJobTypePolicyCatalogEntries(job_type.job_type_id, policy_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(policy_rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteJobTypePolicyCatalogEntry(policy.policy_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteJobScheduleCatalogEntry(schedule.schedule_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteJobParamCatalogEntry(job_param.param_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteJobTypeParamCatalogEntry(type_param.param_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteJobTypeCatalogEntry(job_type.job_type_id, &ctx), Status::OK) << ctx.message;
}
