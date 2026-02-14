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

class CatalogTextSearchExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_text_search_extension_contract_" + std::to_string(getpid()) + ".db";
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

TEST_F(CatalogTextSearchExtensionContractTest, TextSearchCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::TsParserCatalogInfo parser{};
    parser.parser_id = generateUuidV7();
    parser.parser_name = "ts_default_parser";
    parser.start_proc_id = generateUuidV7();
    parser.gettoken_proc_id = generateUuidV7();
    parser.end_proc_id = generateUuidV7();
    parser.lextypes_proc_id = generateUuidV7();
    parser.has_headline_proc_id = true;
    parser.headline_proc_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertTsParserCatalogEntry(parser, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsParserCatalogInfo parser_dup = parser;
    parser_dup.parser_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertTsParserCatalogEntry(parser_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::TsTemplateCatalogInfo templ{};
    templ.template_id = generateUuidV7();
    templ.template_name = "ts_template_simple";
    templ.init_proc_id = generateUuidV7();
    templ.lexize_proc_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertTsTemplateCatalogEntry(templ, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsTemplateCatalogInfo templ_dup = templ;
    templ_dup.template_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertTsTemplateCatalogEntry(templ_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::TsDictionaryCatalogInfo dict{};
    dict.dictionary_id = generateUuidV7();
    dict.dictionary_name = "ts_dict_english";
    dict.template_id = templ.template_id;
    dict.has_init_options = true;
    dict.init_options_json = "{\"stopwords\":\"english\",\"stemmer\":\"snowball\"}";
    ASSERT_EQ(catalog_->upsertTsDictionaryCatalogEntry(dict, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsDictionaryCatalogInfo dict_out{};
    ASSERT_EQ(catalog_->getTsDictionaryCatalogEntry(dict.dictionary_id, dict_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(dict_out.has_init_options);
    EXPECT_EQ(dict_out.init_options_json, dict.init_options_json);

    CatalogManager::TsDictionaryCatalogInfo dict_dup = dict;
    dict_dup.dictionary_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertTsDictionaryCatalogEntry(dict_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::TsConfigCatalogInfo cfg{};
    cfg.config_id = generateUuidV7();
    cfg.config_name = "ts_cfg_en";
    cfg.parser_id = parser.parser_id;
    cfg.has_default_dictionary_id = true;
    cfg.default_dictionary_id = dict.dictionary_id;
    ASSERT_EQ(catalog_->upsertTsConfigCatalogEntry(cfg, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsConfigCatalogInfo cfg_dup = cfg;
    cfg_dup.config_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertTsConfigCatalogEntry(cfg_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::TsConfigMapCatalogInfo map{};
    map.map_id = generateUuidV7();
    map.config_id = cfg.config_id;
    map.token_type = "asciiword";
    map.dictionary_ids = {dict.dictionary_id};
    map.is_override = true;
    ASSERT_EQ(catalog_->upsertTsConfigMapCatalogEntry(map, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsConfigMapCatalogInfo map_out{};
    ASSERT_EQ(catalog_->getTsConfigMapCatalogEntry(map.map_id, map_out, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(map_out.dictionary_ids.size(), 1u);
    EXPECT_EQ(map_out.dictionary_ids[0], dict.dictionary_id);
    EXPECT_FALSE(map_out.dict_list_uuid == ID{});

    CatalogManager::TsConfigMapCatalogInfo map_dup = map;
    map_dup.map_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertTsConfigMapCatalogEntry(map_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::TsConfigMapCatalogInfo map_bad = map;
    map_bad.map_id = generateUuidV7();
    map_bad.token_type = "hword";
    map_bad.dictionary_ids.clear();
    EXPECT_EQ(catalog_->upsertTsConfigMapCatalogEntry(map_bad, &ctx), Status::INVALID_ARGUMENT);

    std::vector<CatalogManager::TsConfigMapCatalogInfo> map_rows;
    ASSERT_EQ(catalog_->listTsConfigMapCatalogEntries(cfg.config_id, map_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(map_rows.size(), 1u);
    EXPECT_EQ(map_rows[0].token_type, "asciiword");
    ASSERT_EQ(map_rows[0].dictionary_ids.size(), 1u);
    EXPECT_EQ(map_rows[0].dictionary_ids[0], dict.dictionary_id);

    ASSERT_EQ(catalog_->deleteTsConfigMapCatalogEntry(map.map_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getTsConfigMapCatalogEntry(map.map_id, map_out, &ctx), Status::NOT_FOUND);
}
