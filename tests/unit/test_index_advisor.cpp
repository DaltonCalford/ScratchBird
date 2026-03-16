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
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/types.h"
#include "scratchbird/optimizer/index_advisor.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::DataType;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::Status;
using scratchbird::optimizer::IndexAdvisor;
using scratchbird::optimizer::IndexRecommendation;

namespace {

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

class IndexAdvisorQueryV3Test : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = "/tmp/test_index_advisor_v3_" + std::to_string(::getpid()) + ".db";
        std::filesystem::remove(db_path_);
        std::filesystem::remove(db_path_ + "-lock");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog_->getSchema("PUBLIC", schema_info, &ctx), Status::OK) << ctx.message;
        schema_id_ = schema_info.schema_id;

        orders_table_id_ = createTable(
            "orders",
            {{"id", DataType::INT32},
             {"user_id", DataType::INT32},
             {"amount", DataType::INT32}});
        users_table_id_ = createTable(
            "users",
            {{"id", DataType::INT32},
             {"active", DataType::BOOLEAN}});
    }

    void TearDown() override {
        db_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove(db_path_ + "-lock");
    }

    ID createTable(const std::string& name,
                   const std::vector<std::pair<std::string, DataType>>& columns) {
        std::vector<CatalogManager::ColumnInfo> column_infos;
        column_infos.reserve(columns.size());
        for (size_t i = 0; i < columns.size(); ++i) {
            CatalogManager::ColumnInfo col{};
            col.column_name = columns[i].first;
            col.data_type = static_cast<uint16_t>(columns[i].second);
            col.nullable = false;
            col.ordinal = static_cast<uint16_t>(i);
            column_infos.push_back(col);
        }

        ErrorContext ctx;
        ID table_id{};
        const auto status = catalog_->createTable(schema_id_, name, column_infos, table_id, 0, &ctx);
        if (status != Status::OK) {
            ADD_FAILURE() << "createTable failed for " << name << ": " << ctx.message;
        }
        return table_id;
    }

    Database db_;
    CatalogManager* catalog_ = nullptr;
    std::string db_path_;
    ID schema_id_{};
    ID orders_table_id_{};
    ID users_table_id_{};
};

TEST_F(IndexAdvisorQueryV3Test, SuggestsIndexesFromSelectJoinAndWherePredicates) {
    IndexAdvisor advisor(&db_);
    std::vector<IndexRecommendation> recs;
    ErrorContext ctx;

    const auto status = advisor.suggestIndexesForQuery(
        "SELECT o.id FROM orders o JOIN users u ON o.user_id = u.id "
        "WHERE o.amount > 100 AND u.active = 1",
        &recs, &ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_FALSE(recs.empty());

    bool has_orders_amount = false;
    bool has_orders_user_id = false;
    bool has_users_active = false;
    bool has_what_if = false;
    for (const auto& rec : recs) {
        if (rec.column_names.empty()) {
            continue;
        }
        const std::string table = lowerAscii(rec.table_name);
        const std::string col = lowerAscii(rec.column_names.front());
        if (table == "orders" && col == "amount") {
            has_orders_amount = true;
        }
        if (table == "orders" && col == "user_id") {
            has_orders_user_id = true;
        }
        if (table == "users" && col == "active") {
            has_users_active = true;
        }
        if (rec.what_if.replanned) {
            has_what_if = true;
            EXPECT_FALSE(rec.what_if.baseline_access_family.empty());
            EXPECT_FALSE(rec.what_if.hypothetical_access_family.empty());
            EXPECT_GT(rec.what_if.baseline_total_cost,
                      rec.what_if.hypothetical_total_cost);
            EXPECT_GT(rec.what_if.estimated_speedup_ratio, 1.0);
            EXPECT_FALSE(rec.what_if.evidence_detail.empty());
        }
    }

    EXPECT_TRUE(has_orders_amount || has_orders_user_id || has_users_active);
    EXPECT_TRUE(has_what_if);
}

TEST_F(IndexAdvisorQueryV3Test, SkipsColumnWhenSingleColumnIndexAlreadyExists) {
    ErrorContext ctx;
    ID index_id{};
    ASSERT_EQ(catalog_->createIndex(orders_table_id_,
                                    "idx_orders_amount",
                                    {"amount"},
                                    index_id,
                                    false,
                                    CatalogManager::IndexType::BTREE,
                                    0,
                                    &ctx),
              Status::OK)
        << ctx.message;

    IndexAdvisor advisor(&db_);
    std::vector<IndexRecommendation> recs;
    ASSERT_EQ(advisor.suggestIndexesForQuery(
                  "SELECT id FROM orders WHERE amount > 10",
                  &recs, &ctx),
              Status::OK)
        << ctx.message;

    for (const auto& rec : recs) {
        if (rec.column_names.empty()) {
            continue;
        }
        EXPECT_FALSE(lowerAscii(rec.table_name) == "orders" &&
                     lowerAscii(rec.column_names.front()) == "amount");
    }
}

TEST_F(IndexAdvisorQueryV3Test, NonDmlStatementsReturnNoRecommendations) {
    IndexAdvisor advisor(&db_);
    std::vector<IndexRecommendation> recs;
    ErrorContext ctx;

    ASSERT_EQ(advisor.suggestIndexesForQuery(
                  "CREATE TABLE z (id INT)",
                  &recs, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(recs.empty());
}

}  // namespace
