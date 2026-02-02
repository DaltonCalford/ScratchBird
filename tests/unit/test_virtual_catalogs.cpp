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
#include <string>
#include <vector>
#include <unistd.h>
#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/lock_manager.h"

using namespace scratchbird::core;
using namespace scratchbird::catalog;

class VirtualCatalogTest : public ::testing::Test
{
protected:
    std::string db_path;
    std::unique_ptr<Database> db;
    std::unique_ptr<ConnectionContext> conn;
    CatalogManager* catalog = nullptr;
    CatalogManager::SessionInfo session;

    void SetUp() override
    {
        db_path = "/tmp/test_virtual_catalog_" + std::to_string(getpid()) + ".db";
        std::remove(db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path, 16384, &ctx), Status::OK);

        db = std::make_unique<Database>();
        ASSERT_EQ(db->open(db_path, &ctx), Status::OK);
        ASSERT_EQ(db->connect(conn, &ctx), Status::OK);

        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID system_user_id = catalog->getSystemUserId(&ctx);
        ASSERT_EQ(catalog->createSession(system_user_id, ID{}, "mysql", session, &ctx), Status::OK);

        conn->setSessionContext(session.session_id, session.authkey_id,
                                session.emulation_mode, session.policy_epoch_global,
                                session.policy_epoch_table);
        conn->beginStatementTracking("SELECT 1");
    }

    void TearDown() override
    {
        if (conn)
        {
            conn->endStatementTrackingSuccess(0);
            conn.reset();
        }
        if (db)
        {
            db->close();
            db.reset();
        }
        std::remove(db_path.c_str());
    }
};

TEST_F(VirtualCatalogTest, PgStatActivityAndLocks)
{
    ErrorContext ctx;

    ID schema_id;
    ASSERT_EQ(catalog->createSchema("test_schema", "system", schema_id, &ctx), Status::OK);

    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_name = "id";
    col.data_type = static_cast<uint16_t>(DataType::INT64);
    col.nullable = false;
    columns.push_back(col);

    ID table_id;
    ASSERT_EQ(catalog->createTable(schema_id, "test_table", columns, table_id, 0, &ctx), Status::OK);

    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_TABLE;
    tag.object_uuid = table_id;
    tag.page_num = 0;
    tag.offset_num = 0;
    tag.padding = 0;

    ASSERT_EQ(db->lock_manager()->acquireLock(conn->getProcId(), tag,
                                              LockMode::LOCK_ACCESS_SHARE,
                                              false, 0, &ctx),
              Status::OK);

    VirtualResultSet activity_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::POSTGRESQL, "pg_catalog",
                                  "pg_stat_activity", "", activity_results, &ctx),
              Status::OK);
    EXPECT_FALSE(activity_results.empty());

    bool found_user = false;
    for (const auto& row : activity_results.rows)
    {
        const auto* user_val = row.getColumn("usename");
        if (user_val && !user_val->isNull() &&
            user_val->toString() == session.username)
        {
            found_user = true;
            break;
        }
    }
    EXPECT_TRUE(found_user);

    VirtualResultSet lock_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::POSTGRESQL, "pg_catalog",
                                  "pg_locks", "", lock_results, &ctx),
              Status::OK);
    EXPECT_FALSE(lock_results.empty());

    bool found_relation = false;
    for (const auto& row : lock_results.rows)
    {
        const auto* rel_val = row.getColumn("relation");
        if (rel_val && !rel_val->isNull())
        {
            found_relation = true;
            break;
        }
    }
    EXPECT_TRUE(found_relation);

    db->lock_manager()->releaseLock(conn->getProcId(), tag,
                                    LockMode::LOCK_ACCESS_SHARE, &ctx);
}

TEST_F(VirtualCatalogTest, MySQLProcesslist)
{
    ErrorContext ctx;

    conn->endStatementTrackingSuccess(1);
    conn->beginStatementTracking("SELECT 1");

    VirtualResultSet info_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "information_schema",
                                  "processlist", "", info_results, &ctx),
              Status::OK);
    EXPECT_FALSE(info_results.empty());

    bool found_user = false;
    bool found_info = false;
    for (const auto& row : info_results.rows)
    {
        const auto* user_val = row.getColumn("USER");
        if (user_val && !user_val->isNull() &&
            user_val->toString() == session.username)
        {
            found_user = true;
        }

        const auto* info_val = row.getColumn("INFO");
        if (info_val && !info_val->isNull())
        {
            if (info_val->toString().find("SELECT 1") != std::string::npos)
            {
                found_info = true;
            }
        }
    }
    EXPECT_TRUE(found_user);
    EXPECT_TRUE(found_info);

    VirtualResultSet perf_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "processlist", "", perf_results, &ctx),
              Status::OK);
    EXPECT_FALSE(perf_results.empty());

    VirtualResultSet threads_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "threads", "", threads_results, &ctx),
              Status::OK);
    EXPECT_FALSE(threads_results.empty());

    bool found_thread = false;
    for (const auto& row : threads_results.rows)
    {
        const auto* user_val = row.getColumn("PROCESSLIST_USER");
        const auto* info_val = row.getColumn("PROCESSLIST_INFO");
        if (user_val && !user_val->isNull() &&
            user_val->toString() == session.username)
        {
            if (info_val && !info_val->isNull() &&
                info_val->toString().find("SELECT 1") != std::string::npos)
            {
                found_thread = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found_thread);

    VirtualResultSet history_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_statements_history_long", "", history_results, &ctx),
              Status::OK);
    EXPECT_FALSE(history_results.empty());

    bool found_statement = false;
    for (const auto& row : history_results.rows)
    {
        const auto* sql_val = row.getColumn("SQL_TEXT");
        if (sql_val && !sql_val->isNull() &&
            sql_val->toString().find("SELECT 1") != std::string::npos)
        {
            found_statement = true;
            break;
        }
    }
    EXPECT_TRUE(found_statement);

    VirtualResultSet digest_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_statements_summary_by_digest", "", digest_results, &ctx),
              Status::OK);
    EXPECT_FALSE(digest_results.empty());

    bool found_digest = false;
    std::string observed_digest;
    for (const auto& row : digest_results.rows)
    {
        const auto* digest_text_val = row.getColumn("DIGEST_TEXT");
        const auto* digest_val = row.getColumn("DIGEST");
        const auto* count_val = row.getColumn("COUNT_STAR");
        if (digest_text_val && !digest_text_val->isNull() &&
            digest_text_val->toString().find("SELECT") != std::string::npos)
        {
            if (!count_val || count_val->isNull() || count_val->toInt64() >= 1)
            {
                found_digest = true;
                if (digest_val && !digest_val->isNull())
                {
                    observed_digest = digest_val->toString();
                }
                break;
            }
        }
    }
    EXPECT_TRUE(found_digest);

    VirtualResultSet digest_account_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_statements_summary_by_account_by_digest", "", digest_account_results, &ctx),
              Status::OK);
    EXPECT_FALSE(digest_account_results.empty());

    bool found_account_digest = false;
    for (const auto& row : digest_account_results.rows)
    {
        const auto* user_val = row.getColumn("USER");
        const auto* host_val = row.getColumn("HOST");
        const auto* digest_val = row.getColumn("DIGEST");
        const auto* count_val = row.getColumn("COUNT_STAR");
        if (user_val && !user_val->isNull() &&
            user_val->toString() == session.username &&
            host_val && !host_val->isNull() &&
            host_val->toString() == "local")
        {
            if (observed_digest.empty() ||
                (digest_val && !digest_val->isNull() &&
                 digest_val->toString() == observed_digest))
            {
                if (!count_val || count_val->isNull() || count_val->toInt64() >= 1)
                {
                    found_account_digest = true;
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(found_account_digest);

    VirtualResultSet digest_user_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_statements_summary_by_user_by_digest", "", digest_user_results, &ctx),
              Status::OK);
    EXPECT_FALSE(digest_user_results.empty());

    bool found_user_digest = false;
    for (const auto& row : digest_user_results.rows)
    {
        const auto* user_val = row.getColumn("USER");
        const auto* digest_val = row.getColumn("DIGEST");
        const auto* count_val = row.getColumn("COUNT_STAR");
        if (user_val && !user_val->isNull() &&
            user_val->toString() == session.username)
        {
            if (observed_digest.empty() ||
                (digest_val && !digest_val->isNull() &&
                 digest_val->toString() == observed_digest))
            {
                if (!count_val || count_val->isNull() || count_val->toInt64() >= 1)
                {
                    found_user_digest = true;
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(found_user_digest);

    VirtualResultSet digest_host_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_statements_summary_by_host_by_digest", "", digest_host_results, &ctx),
              Status::OK);
    EXPECT_FALSE(digest_host_results.empty());

    bool found_host_digest = false;
    for (const auto& row : digest_host_results.rows)
    {
        const auto* host_val = row.getColumn("HOST");
        const auto* digest_val = row.getColumn("DIGEST");
        const auto* count_val = row.getColumn("COUNT_STAR");
        if (host_val && !host_val->isNull() &&
            host_val->toString() == "local")
        {
            if (observed_digest.empty() ||
                (digest_val && !digest_val->isNull() &&
                 digest_val->toString() == observed_digest))
            {
                if (!count_val || count_val->isNull() || count_val->toInt64() >= 1)
                {
                    found_host_digest = true;
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(found_host_digest);

    VirtualResultSet hist_by_digest_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_statements_histogram_by_digest", "", hist_by_digest_results, &ctx),
              Status::OK);
    EXPECT_FALSE(hist_by_digest_results.empty());

    uint64_t digest_bucket_total = 0;
    for (const auto& row : hist_by_digest_results.rows)
    {
        const auto* digest_val = row.getColumn("DIGEST");
        const auto* count_val = row.getColumn("COUNT_BUCKET");
        if (!observed_digest.empty() && digest_val && !digest_val->isNull() &&
            digest_val->toString() == observed_digest)
        {
            if (count_val && !count_val->isNull())
            {
                digest_bucket_total += static_cast<uint64_t>(count_val->toInt64());
            }
        }
    }
    EXPECT_GE(digest_bucket_total, 1u);

    VirtualResultSet hist_global_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_statements_histogram_global", "", hist_global_results, &ctx),
              Status::OK);
    EXPECT_FALSE(hist_global_results.empty());

    uint64_t global_bucket_total = 0;
    for (const auto& row : hist_global_results.rows)
    {
        const auto* count_val = row.getColumn("COUNT_BUCKET");
        if (count_val && !count_val->isNull())
        {
            global_bucket_total += static_cast<uint64_t>(count_val->toInt64());
        }
    }
    EXPECT_GE(global_bucket_total, 1u);

    VirtualResultSet txn_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_transactions_current", "", txn_results, &ctx),
              Status::OK);
    EXPECT_FALSE(txn_results.empty());

    bool found_txn = false;
    uint64_t current_xid = conn->getCurrentXid();
    for (const auto& row : txn_results.rows)
    {
        const auto* trx_val = row.getColumn("TRX_ID");
        if (trx_val && !trx_val->isNull() &&
            static_cast<uint64_t>(trx_val->toInt64()) == current_xid)
        {
            found_txn = true;
            break;
        }
    }
    EXPECT_TRUE(found_txn);

    VirtualResultSet waits_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_waits_current", "", waits_results, &ctx),
              Status::OK);

    ID schema_id;
    ASSERT_EQ(catalog->createSchema("mysql_schema", "system", schema_id, &ctx), Status::OK);

    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_name = "id";
    col.data_type = static_cast<uint16_t>(DataType::INT64);
    col.nullable = false;
    columns.push_back(col);

    ID table_id;
    ASSERT_EQ(catalog->createTable(schema_id, "mysql_lock_table", columns, table_id, 0, &ctx), Status::OK);

    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_TABLE;
    tag.object_uuid = table_id;
    tag.page_num = 0;
    tag.offset_num = 0;
    tag.padding = 0;

    ASSERT_EQ(db->lock_manager()->acquireLock(conn->getProcId(), tag,
                                              LockMode::LOCK_ACCESS_SHARE,
                                              false, 0, &ctx),
              Status::OK);

    std::unique_ptr<ConnectionContext> conn2;
    ASSERT_EQ(db->connect(conn2, &ctx), Status::OK);
    CatalogManager::SessionInfo session2;
    ID system_user_id = catalog->getSystemUserId(&ctx);
    ASSERT_EQ(catalog->createSession(system_user_id, ID{}, "mysql", session2, &ctx), Status::OK);
    conn2->setSessionContext(session2.session_id, session2.authkey_id,
                             session2.emulation_mode, session2.policy_epoch_global,
                             session2.policy_epoch_table);

    Status wait_status = db->lock_manager()->acquireLock(conn2->getProcId(), tag,
                                                         LockMode::LOCK_ACCESS_EXCLUSIVE,
                                                         true, 5, &ctx);
    EXPECT_EQ(wait_status, Status::LOCK_TIMEOUT);

    VirtualResultSet waits_history_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_waits_history_long", "", waits_history_results, &ctx),
              Status::OK);
    EXPECT_FALSE(waits_history_results.empty());

    bool found_timeout = false;
    for (const auto& row : waits_history_results.rows)
    {
        const auto* flags_val = row.getColumn("FLAGS");
        if (flags_val && !flags_val->isNull() &&
            flags_val->toString().find("TIMEOUT") != std::string::npos)
        {
            found_timeout = true;
            break;
        }
    }
    EXPECT_TRUE(found_timeout);

    VirtualResultSet metadata_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "metadata_locks", "", metadata_results, &ctx),
              Status::OK);
    EXPECT_FALSE(metadata_results.empty());

    bool found_lock = false;
    for (const auto& row : metadata_results.rows)
    {
        const auto* schema_val = row.getColumn("OBJECT_SCHEMA");
        const auto* name_val = row.getColumn("OBJECT_NAME");
        const auto* status_val = row.getColumn("LOCK_STATUS");
        if (schema_val && name_val && !schema_val->isNull() && !name_val->isNull())
        {
            if (schema_val->toString() == "mysql_schema" &&
                name_val->toString() == "mysql_lock_table")
            {
                if (!status_val || status_val->isNull() ||
                    status_val->toString() == "GRANTED")
                {
                    found_lock = true;
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(found_lock);

    db->lock_manager()->releaseLock(conn->getProcId(), tag,
                                    LockMode::LOCK_ACCESS_SHARE, &ctx);

    ASSERT_EQ(conn->commit(&ctx), Status::OK);

    VirtualResultSet txn_history_results;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema",
                                  "events_transactions_history_long", "", txn_history_results, &ctx),
              Status::OK);
    EXPECT_FALSE(txn_history_results.empty());

    bool found_txn_history = false;
    for (const auto& row : txn_history_results.rows)
    {
        const auto* trx_val = row.getColumn("TRX_ID");
        if (trx_val && !trx_val->isNull() &&
            static_cast<uint64_t>(trx_val->toInt64()) == current_xid)
        {
            found_txn_history = true;
            break;
        }
    }
    EXPECT_TRUE(found_txn_history);
    conn2.reset();
}
