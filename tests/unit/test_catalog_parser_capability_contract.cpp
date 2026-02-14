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

class CatalogParserCapabilityContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_parser_capability_contract_" + std::to_string(getpid()) + ".db";
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

    ID createParserProfile(const std::string& profile_name,
                           CatalogManager::EmulationEngine engine,
                           uint16_t major,
                           uint16_t minor,
                           bool is_default)
    {
        CatalogManager::ParserProfileCatalogInfo profile{};
        profile.profile_name = profile_name;
        profile.parser_engine = engine;
        profile.version_major = major;
        profile.version_minor = minor;
        profile.is_native = (engine == CatalogManager::EmulationEngine::NATIVE);
        profile.is_default = is_default;
        profile.is_enabled = true;
        profile.profile_hash = std::string(63, 'a');

        ErrorContext ctx;
        ID profile_id{};
        EXPECT_EQ(catalog_->upsertParserProfileCatalogEntry(profile, profile_id, &ctx), Status::OK)
            << ctx.message;
        return profile_id;
    }
};

TEST_F(CatalogParserCapabilityContractTest, ReservedWordAndEmulationProfileContracts)
{
    ErrorContext ctx;
    ID reserved_word_id{};

    CatalogManager::ReservedWordCatalogInfo invalid_word{};
    EXPECT_EQ(catalog_->upsertReservedWordCatalogEntry(invalid_word, reserved_word_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ReservedWordCatalogInfo native_select{};
    native_select.word = "select";
    native_select.parser_scope = CatalogManager::EmulationEngine::NATIVE;
    native_select.is_reserved = true;
    native_select.is_keyword = true;
    native_select.last_updated_txid = 42;
    ASSERT_EQ(catalog_->upsertReservedWordCatalogEntry(native_select, reserved_word_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(reserved_word_id, ID{});

    CatalogManager::ReservedWordCatalogInfo duplicate = native_select;
    duplicate.reserved_word_id = generateUuidV7();
    ID duplicate_id{};
    EXPECT_EQ(catalog_->upsertReservedWordCatalogEntry(duplicate, duplicate_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::ReservedWordCatalogInfo mysql_select = native_select;
    mysql_select.reserved_word_id = ID{};
    mysql_select.parser_scope = CatalogManager::EmulationEngine::MYSQL;
    ID mysql_word_id{};
    ASSERT_EQ(catalog_->upsertReservedWordCatalogEntry(mysql_select, mysql_word_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ReservedWordCatalogInfo fetched_word{};
    ASSERT_EQ(catalog_->getReservedWordCatalogEntryByWord(
                  "SELECT", CatalogManager::EmulationEngine::NATIVE, fetched_word, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_word.reserved_word_id, reserved_word_id);
    EXPECT_EQ(fetched_word.word, "SELECT");

    CatalogManager::EmulationProfileCatalogInfo invalid_profile{};
    invalid_profile.engine = CatalogManager::EmulationEngine::POSTGRESQL;
    invalid_profile.enabled = true;
    invalid_profile.storage_profile = CatalogManager::StorageProfile::RELATIONAL;
    EXPECT_EQ(catalog_->upsertEmulationProfileCatalogEntry(invalid_profile, duplicate_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::EmulationProfileCatalogInfo profile{};
    profile.engine = CatalogManager::EmulationEngine::POSTGRESQL;
    profile.enabled = true;
    profile.storage_profile = CatalogManager::StorageProfile::NATIVE_EMULATION;
    profile.requested_engine_version = "18.x";
    profile.installed_txid = 100;
    profile.last_modified_txid = 101;
    profile.config_flags = 7;
    ID profile_id{};
    ASSERT_EQ(catalog_->upsertEmulationProfileCatalogEntry(profile, profile_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::EmulationProfileCatalogInfo duplicate_engine = profile;
    duplicate_engine.emulation_profile_id = generateUuidV7();
    ID duplicate_profile_id{};
    EXPECT_EQ(catalog_->upsertEmulationProfileCatalogEntry(duplicate_engine, duplicate_profile_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::EmulationProfileCatalogInfo fetched_profile{};
    ASSERT_EQ(catalog_->getEmulationProfileCatalogEntryByEngine(
                  CatalogManager::EmulationEngine::POSTGRESQL, fetched_profile, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_profile.emulation_profile_id, profile_id);
    EXPECT_EQ(fetched_profile.requested_engine_version, "18.x");
}

TEST_F(CatalogParserCapabilityContractTest, ParserCapabilityContracts)
{
    ErrorContext ctx;
    ID native_profile_id = createParserProfile(
        "native_v3_default",
        CatalogManager::EmulationEngine::NATIVE,
        3,
        0,
        true);

    CatalogManager::ParserProfileCatalogInfo second_default{};
    second_default.profile_name = "native_v3_alt";
    second_default.parser_engine = CatalogManager::EmulationEngine::NATIVE;
    second_default.version_major = 3;
    second_default.version_minor = 1;
    second_default.is_native = true;
    second_default.is_default = true;
    second_default.is_enabled = true;
    second_default.profile_hash = std::string(63, 'b');
    ID second_default_id{};
    EXPECT_EQ(catalog_->upsertParserProfileCatalogEntry(second_default, second_default_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    ID mysql_profile_id = createParserProfile(
        "mysql_v8_default",
        CatalogManager::EmulationEngine::MYSQL,
        8,
        0,
        true);

    CatalogManager::ParserProfileCatalogInfo bad_base{};
    bad_base.profile_name = "pg_bad_base";
    bad_base.parser_engine = CatalogManager::EmulationEngine::POSTGRESQL;
    bad_base.version_major = 18;
    bad_base.version_minor = 0;
    bad_base.is_native = false;
    bad_base.is_default = false;
    bad_base.is_enabled = true;
    bad_base.base_profile_id = mysql_profile_id;
    bad_base.profile_hash = std::string(63, 'c');
    ID bad_base_id{};
    EXPECT_EQ(catalog_->upsertParserProfileCatalogEntry(bad_base, bad_base_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::ParserTransformCatalogInfo non_deterministic{};
    non_deterministic.parser_profile_id = native_profile_id;
    non_deterministic.transform_name = "rewrite_select";
    non_deterministic.feature_family = "ddl";
    non_deterministic.feature_key = "create_database";
    non_deterministic.transform_stage = CatalogManager::ParserTransformStage::AST_REWRITE;
    non_deterministic.input_contract_json = "{\"in\":\"ddl\"}";
    non_deterministic.output_contract_json = "{\"out\":\"sblr\"}";
    non_deterministic.implementation_ref = "native.ddl.create_database";
    non_deterministic.is_deterministic = false;
    non_deterministic.timeout_ms = 10;
    ID transform_id{};
    EXPECT_EQ(catalog_->upsertParserTransformCatalogEntry(non_deterministic, transform_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ParserTransformCatalogInfo transform = non_deterministic;
    transform.is_deterministic = true;
    transform.is_idempotent = true;
    ASSERT_EQ(catalog_->upsertParserTransformCatalogEntry(transform, transform_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ParserCapabilityCatalogInfo bad_implement{};
    bad_implement.parser_profile_id = native_profile_id;
    bad_implement.feature_family = "ddl";
    bad_implement.feature_key = "create_database";
    bad_implement.capability_action = CatalogManager::ParserCapabilityAction::IMPLEMENT;
    bad_implement.parser_transform_id = transform_id;
    ID capability_id{};
    EXPECT_EQ(catalog_->upsertParserCapabilityCatalogEntry(bad_implement, capability_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ParserCapabilityCatalogInfo bad_remap = bad_implement;
    bad_remap.capability_action = CatalogManager::ParserCapabilityAction::REMAP;
    bad_remap.parser_transform_id = ID{};
    EXPECT_EQ(catalog_->upsertParserCapabilityCatalogEntry(bad_remap, capability_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ParserCapabilityCatalogInfo remap = bad_remap;
    remap.parser_transform_id = transform_id;
    remap.precedence_rank = 20;
    ASSERT_EQ(catalog_->upsertParserCapabilityCatalogEntry(remap, capability_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ParserCapabilityCatalogInfo bad_reject = remap;
    bad_reject.parser_capability_id = ID{};
    bad_reject.feature_key = "drop_database";
    bad_reject.capability_action = CatalogManager::ParserCapabilityAction::REJECT;
    bad_reject.parser_transform_id = ID{};
    bad_reject.reject_code.clear();
    ID reject_capability_id{};
    EXPECT_EQ(catalog_->upsertParserCapabilityCatalogEntry(bad_reject, reject_capability_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ParserCapabilityCatalogInfo reject = bad_reject;
    reject.reject_code = "SB-PARSER-CAP-0001";
    reject.precedence_rank = 10;
    ASSERT_EQ(catalog_->upsertParserCapabilityCatalogEntry(reject, reject_capability_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ParserErrorMapCatalogInfo error_map{};
    error_map.parser_profile_id = native_profile_id;
    error_map.reject_code = "SB-PARSER-CAP-0001";
    error_map.dialect_sqlstate = "42000";
    error_map.dialect_error_code = "10001";
    error_map.error_severity = CatalogManager::ParserErrorSeverity::ERROR;
    error_map.message_template = "Feature not supported for this profile";
    error_map.hint_template = std::string("Use supported syntax");
    ID error_map_id{};
    ASSERT_EQ(catalog_->upsertParserErrorMapCatalogEntry(error_map, error_map_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ParserErrorMapCatalogInfo fetched_error_map{};
    ASSERT_EQ(catalog_->getParserErrorMapCatalogEntryByRejectCode(
                  native_profile_id, "SB-PARSER-CAP-0001", fetched_error_map, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_error_map.dialect_sqlstate, "42000");
    EXPECT_EQ(fetched_error_map.message_template, "Feature not supported for this profile");

    CatalogManager::ParserFeaturePrecedenceCatalogInfo precedence_high{};
    precedence_high.parser_profile_id = native_profile_id;
    precedence_high.feature_family = "ddl";
    precedence_high.feature_key = "create_database";
    precedence_high.precedence_rank = 20;
    precedence_high.precedence_tiebreak =
        CatalogManager::ParserPrecedenceTiebreak::SPECIFICITY_FIRST;
    precedence_high.is_terminal = false;
    ID precedence_high_id{};
    ASSERT_EQ(catalog_->upsertParserFeaturePrecedenceCatalogEntry(
                  precedence_high, precedence_high_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::ParserFeaturePrecedenceCatalogInfo precedence_low = precedence_high;
    precedence_low.parser_feature_precedence_id = ID{};
    precedence_low.feature_key = "drop_database";
    precedence_low.precedence_rank = 10;
    precedence_low.is_terminal = true;
    ID precedence_low_id{};
    ASSERT_EQ(catalog_->upsertParserFeaturePrecedenceCatalogEntry(
                  precedence_low, precedence_low_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::ParserFeaturePrecedenceCatalogInfo duplicate_rank = precedence_low;
    duplicate_rank.parser_feature_precedence_id = ID{};
    duplicate_rank.feature_key = "alter_database";
    ID duplicate_rank_id{};
    EXPECT_EQ(catalog_->upsertParserFeaturePrecedenceCatalogEntry(
                  duplicate_rank, duplicate_rank_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    std::vector<CatalogManager::ParserFeaturePrecedenceCatalogInfo> precedence_rows;
    ASSERT_EQ(catalog_->listParserFeaturePrecedenceCatalogEntries(
                  native_profile_id, "ddl", precedence_rows, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(precedence_rows.size(), 2u);
    EXPECT_EQ(precedence_rows[0].precedence_rank, 10u);
    EXPECT_EQ(precedence_rows[1].precedence_rank, 20u);
}
