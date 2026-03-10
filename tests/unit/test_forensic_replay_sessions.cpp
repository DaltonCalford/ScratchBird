#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using namespace scratchbird::sblr;
using json = nlohmann::json;

class ForensicReplaySessionTest : public ::testing::Test
{
protected:
    class ScopedCurrentConnection
    {
    public:
        explicit ScopedCurrentConnection(ConnectionContext* ctx)
            : previous_(ConnectionContext::getCurrent())
        {
            ConnectionContext::setCurrent(ctx);
        }

        ~ScopedCurrentConnection()
        {
            ConnectionContext::setCurrent(previous_);
        }

    private:
        ConnectionContext* previous_ = nullptr;
    };

    void SetUp() override
    {
        db_path_ = scratchbird::testing::uniqueTestDbPath("test_forensic_replay", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        storage_ = db_->storage_engine();
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(storage_, nullptr);

        ASSERT_EQ(db_->connect(admin_conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(admin_conn_.get());
        system_user_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{});
        admin_conn_->setCurrentUser(system_user_id_, true);
        default_schema_id_ = resolveDefaultSchema(&ctx);
        ASSERT_NE(default_schema_id_, ID{});
        createTestTable(&ctx);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        replay_conn_.reset();
        target_conn_.reset();
        blocker_conn_.reset();
        admin_conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
        }
        std::remove(db_path_.c_str());
    }

    void connect(std::unique_ptr<ConnectionContext>& conn_out)
    {
        ErrorContext ctx;
        ASSERT_EQ(db_->connect(conn_out, &ctx), Status::OK) << ctx.message;
        conn_out->setCurrentUser(system_user_id_, true);
    }

    ID resolveDefaultSchema(ErrorContext* ctx)
    {
        CatalogManager::SchemaInfo schema{};
        Status status = catalog_->getSchema("users.public", schema, ctx);
        if (status == Status::OK)
        {
            return schema.schema_id;
        }

        status = catalog_->getSchema("public", schema, ctx);
        if (status == Status::OK)
        {
            return schema.schema_id;
        }

        ID schema_id{};
        EXPECT_EQ(catalog_->createSchema("public", "SYSTEM", schema_id, ctx), Status::OK)
            << ctx->message;
        return schema_id;
    }

    std::vector<uint8_t> compileSQL(const std::string& sql)
    {
        QueryCompilerV3 compiler(db_.get());
        compiler.setCurrentSchema(default_schema_id_);
        auto result = compiler.compile(sql);
        EXPECT_TRUE(result.success()) << sql;
        if (!result.success())
        {
            if (!result.errors().empty())
            {
                ADD_FAILURE() << result.errors().front();
            }
            return {};
        }
        return result.bytecode();
    }

    ExecutionResult executeSQL(ConnectionContext* conn_ctx, const std::string& sql)
    {
        auto bytecode = compileSQL(sql);
        if (bytecode.empty())
        {
            return ExecutionResult("Failed to compile SQL");
        }

        ScopedCurrentConnection current(conn_ctx);
        conn_ctx->beginStatementTracking(sql);
        Executor executor(db_.get());
        executor.setConnectionContext(conn_ctx);
        executor.setCurrentSchema(default_schema_id_);
        ExecutionResult result = executor.execute(bytecode);
        if (result.success())
        {
            conn_ctx->endStatementTrackingSuccess(0);
        }
        else
        {
            conn_ctx->endStatementTrackingFailure(1, "XX000");
        }
        return result;
    }

    Status findTableByName(const std::string& table_name,
                           CatalogManager::TableInfo& table_info,
                           ErrorContext* ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_->listSchemas(schemas, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& schema : schemas)
        {
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            auto it = std::find_if(
                tables.begin(), tables.end(),
                [&](const CatalogManager::TableInfo& candidate) {
                    return IdentifierUtils::namesMatch(
                        table_name, false, candidate.table_name, candidate.name_is_delimited);
                });
            if (it != tables.end())
            {
                table_info = *it;
                return Status::OK;
            }
        }

        if (ctx != nullptr)
        {
            ctx->message = "Table not found: " + table_name;
        }
        return Status::NOT_FOUND;
    }

    void createTestTable(ErrorContext* ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog_->listSchemas(schemas, ctx), Status::OK) << ctx->message;
        ID schema_id;
        if (schemas.empty())
        {
            ASSERT_EQ(catalog_->createSchema("public", "test", schema_id, ctx), Status::OK)
                << ctx->message;
        }
        else
        {
            schema_id = schemas[0].schema_id;
        }

        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo id_col;
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.max_length = 4;
        id_col.nullable = false;
        columns.push_back(id_col);

        CatalogManager::ColumnInfo value_col;
        value_col.column_name = "val";
        value_col.data_type = static_cast<uint16_t>(DataType::INT32);
        value_col.max_length = 4;
        value_col.nullable = false;
        columns.push_back(value_col);

        ASSERT_EQ(catalog_->createTable(schema_id, "forensic_replay_test", columns, table_id_, 0, ctx),
                  Status::OK) << ctx->message;
    }

    std::vector<uint8_t> makeTuple(int32_t id, int32_t value)
    {
        std::vector<uint8_t> buffer(sizeof(TupleHeader) + sizeof(int32_t) * 2);
        std::memset(buffer.data(), 0, buffer.size());
        std::memcpy(buffer.data() + sizeof(TupleHeader), &id, sizeof(int32_t));
        std::memcpy(buffer.data() + sizeof(TupleHeader) + sizeof(int32_t),
                    &value,
                    sizeof(int32_t));
        return buffer;
    }

    int32_t tupleValue(const Tuple& tuple) const
    {
        int32_t value = 0;
        std::memcpy(&value,
                    tuple.data + sizeof(TupleHeader) + sizeof(int32_t),
                    sizeof(int32_t));
        return value;
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    StorageEngine* storage_ = nullptr;
    ID system_user_id_{};
    ID default_schema_id_{};
    ID table_id_{};
    std::unique_ptr<ConnectionContext> admin_conn_;
    std::unique_ptr<ConnectionContext> blocker_conn_;
    std::unique_ptr<ConnectionContext> target_conn_;
    std::unique_ptr<ConnectionContext> replay_conn_;
};

TEST_F(ForensicReplaySessionTest, ReplayByTransactionUuidUsesRetainedSnapshotBoundary)
{
    ErrorContext ctx;

    uint32_t seed_page = 0;
    uint16_t seed_slot = 0;
    {
        ScopedCurrentConnection current(admin_conn_.get());
        auto seed = makeTuple(1, 10);
        ASSERT_EQ(storage_->insertTuple(table_id_, seed.data(), seed.size(), &seed_page, &seed_slot, &ctx),
                  Status::OK) << ctx.message;
        ASSERT_EQ(admin_conn_->commit(&ctx), Status::OK) << ctx.message;
    }

    connect(blocker_conn_);
    uint32_t blocker_page = 0;
    uint16_t blocker_slot = 0;
    {
        ScopedCurrentConnection current(blocker_conn_.get());
        auto pending = makeTuple(2, 20);
        ASSERT_EQ(storage_->insertTuple(
                      table_id_, pending.data(), pending.size(), &blocker_page, &blocker_slot, &ctx),
                  Status::OK) << ctx.message;
    }

    connect(target_conn_);
    const uint64_t target_txid = target_conn_->getCurrentXid();
    const ID target_tx_uuid = target_conn_->getCurrentTransactionUuid();
    ASSERT_NE(target_txid, 0u);
    ASSERT_NE(target_tx_uuid, ID{});

    {
        ScopedCurrentConnection current(target_conn_.get());
        ASSERT_EQ(target_conn_->commit(&ctx), Status::OK) << ctx.message;
    }

    {
        ScopedCurrentConnection current(blocker_conn_.get());
        ASSERT_EQ(blocker_conn_->commit(&ctx), Status::OK) << ctx.message;
    }

    CatalogManager::RuntimeTransactionCatalogInfo target_row{};
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(target_txid, target_row, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(target_row.tx_uuid, target_tx_uuid);
    EXPECT_EQ(target_row.state, CatalogManager::RuntimeTransactionState::COMMITTED);
    ASSERT_NE(target_row.forensic_snapshot_capsule_uuid, ID{});

    CatalogManager::ForensicSnapshotCapsuleCatalogInfo target_capsule{};
    ASSERT_EQ(catalog_->getForensicSnapshotCapsuleCatalogEntry(
                  target_row.forensic_snapshot_capsule_uuid, target_capsule, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(target_capsule.tx_uuid, target_tx_uuid);
    EXPECT_EQ(target_capsule.txid, target_txid);
    EXPECT_EQ(target_capsule.status, "COMMITTED");

    connect(replay_conn_);
    ASSERT_EQ(replay_conn_->openForensicReplayByTransactionUuid(target_tx_uuid, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(replay_conn_->isForensicReplayActive());

    const auto replay_status = replay_conn_->getForensicReplayStatus();
    EXPECT_TRUE(replay_status.is_active);
    EXPECT_EQ(replay_status.resolved_tx_uuid, target_tx_uuid);
    EXPECT_EQ(replay_status.resolved_txid, target_txid);
    EXPECT_NE(replay_status.capsule_uuid, ID{});

    {
        ScopedCurrentConnection current(replay_conn_.get());
        Tuple seed_visible{};
        ASSERT_EQ(storage_->getTuple(seed_page, seed_slot, &seed_visible, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(tupleValue(seed_visible), 10);

        Tuple hidden{};
        EXPECT_EQ(storage_->getTuple(blocker_page, blocker_slot, &hidden, &ctx), Status::NOT_FOUND);
    }

    CatalogManager::RuntimeTransactionCatalogInfo replay_row{};
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(replay_conn_->getCurrentXid(), replay_row, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(replay_row.forensic_snapshot_capsule_uuid, replay_status.capsule_uuid);

    ASSERT_EQ(replay_conn_->closeForensicReplay(&ctx), Status::OK) << ctx.message;
    EXPECT_FALSE(replay_conn_->isForensicReplayActive());

    {
        ScopedCurrentConnection current(replay_conn_.get());
        Tuple visible_after_close{};
        ASSERT_EQ(storage_->getTuple(blocker_page, blocker_slot, &visible_after_close, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(tupleValue(visible_after_close), 20);
    }
}

TEST_F(ForensicReplaySessionTest, ReplayFailsClosedWhenNoRetainedSnapshotExists)
{
    ErrorContext ctx;

    connect(target_conn_);
    {
        ScopedCurrentConnection current(target_conn_.get());
        ASSERT_EQ(target_conn_->startTransaction(
                      false, IsolationLevel::READ_COMMITTED, true, &ctx),
                  Status::OK) << ctx.message;
        const uint64_t txid = target_conn_->getCurrentXid();
        ASSERT_NE(txid, 0u);
        ASSERT_EQ(target_conn_->commit(&ctx), Status::OK) << ctx.message;

        connect(replay_conn_);
        EXPECT_EQ(replay_conn_->openForensicReplayByTxid(txid, &ctx), Status::NOT_FOUND);
        EXPECT_NE(ctx.message.find("FORENSIC_CAPSULE_UNAVAILABLE"), std::string::npos);
    }
}

TEST_F(ForensicReplaySessionTest, ReplayResolvesHistoricalSchemaAcrossCommittedDdl)
{
    ErrorContext ctx;

    auto create_result = executeSQL(
        admin_conn_.get(),
        "CREATE TABLE forensic_schema_history (id INT NOT NULL, val INT NOT NULL)");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    const uint64_t create_txid = admin_conn_->getCurrentXid();
    const ID create_tx_uuid = admin_conn_->getCurrentTransactionUuid();
    ASSERT_NE(create_txid, 0u);
    ASSERT_NE(create_tx_uuid, ID{});
    ASSERT_EQ(admin_conn_->commit(&ctx), Status::OK) << ctx.message;

    CatalogManager::TableInfo history_table{};
    ASSERT_EQ(findTableByName("forensic_schema_history", history_table, &ctx), Status::OK)
        << ctx.message;

    auto alter_result = executeSQL(
        admin_conn_.get(),
        "ALTER TABLE forensic_schema_history ADD COLUMN extra INT");
    ASSERT_TRUE(alter_result.success()) << alter_result.error();

    const uint64_t alter_txid = admin_conn_->getCurrentXid();
    const ID alter_tx_uuid = admin_conn_->getCurrentTransactionUuid();
    ASSERT_NE(alter_txid, 0u);
    ASSERT_NE(alter_tx_uuid, ID{});
    ASSERT_EQ(admin_conn_->commit(&ctx), Status::OK) << ctx.message;

    CatalogManager::RuntimeTransactionCatalogInfo create_row{};
    CatalogManager::RuntimeTransactionCatalogInfo alter_row{};
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(create_txid, create_row, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->getRuntimeTransactionCatalogEntry(alter_txid, alter_row, &ctx), Status::OK)
        << ctx.message;
    EXPECT_NE(create_row.schema_epoch_uuid, ID{});
    EXPECT_NE(alter_row.schema_epoch_uuid, ID{});
    EXPECT_NE(create_row.schema_epoch_uuid, alter_row.schema_epoch_uuid);

    std::vector<CatalogManager::ColumnInfo> live_columns;
    ASSERT_EQ(catalog_->getColumns(history_table.table_id, live_columns, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(live_columns.size(), 3u);

    auto requireDdlPayload = [&](const ID& tx_uuid, uint64_t txid) {
        std::vector<CatalogManager::TransactionLineageEventCatalogInfo> events;
        if (catalog_->listTransactionLineageEventCatalogEntries(tx_uuid, txid, events, &ctx) !=
            Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return json::object();
        }

        auto it = std::find_if(
            events.begin(), events.end(),
            [](const CatalogManager::TransactionLineageEventCatalogInfo& event) {
                return event.event_kind == CatalogManager::TransactionLineageEventKind::TX_DDL_BATCH;
            });
        if (it == events.end())
        {
            ADD_FAILURE() << "missing TX_DDL_BATCH event";
            return json::object();
        }
        EXPECT_FALSE(it->payload_json.empty());
        return json::parse(it->payload_json);
    };

    const json create_payload = requireDdlPayload(create_tx_uuid, create_txid);
    const json alter_payload = requireDdlPayload(alter_tx_uuid, alter_txid);
    EXPECT_EQ(create_payload.at("schema_epoch_after_uuid").get<std::string>(),
              create_row.schema_epoch_uuid.toString());
    EXPECT_EQ(alter_payload.at("schema_epoch_before_uuid").get<std::string>(),
              create_row.schema_epoch_uuid.toString());
    EXPECT_EQ(alter_payload.at("schema_epoch_after_uuid").get<std::string>(),
              alter_row.schema_epoch_uuid.toString());
    ASSERT_EQ(create_payload.at("operations").size(), 1u);
    ASSERT_EQ(alter_payload.at("operations").size(), 1u);
    EXPECT_EQ(create_payload.at("operations")[0].at("operation_class").get<std::string>(),
              "CREATE_TABLE");
    EXPECT_EQ(alter_payload.at("operations")[0].at("operation_class").get<std::string>(),
              "ALTER_TABLE");
    EXPECT_EQ(create_payload.at("operations")[0].at("object_uuid").get<std::string>(),
              history_table.table_id.toString());
    EXPECT_EQ(alter_payload.at("operations")[0].at("object_uuid").get<std::string>(),
              history_table.table_id.toString());

    connect(replay_conn_);
    ASSERT_EQ(replay_conn_->openForensicReplayByTransactionUuid(create_tx_uuid, &ctx), Status::OK)
        << ctx.message;
    {
        ScopedCurrentConnection current(replay_conn_.get());
        std::vector<CatalogManager::ColumnInfo> replay_columns;
        ASSERT_EQ(catalog_->getColumns(history_table.table_id, replay_columns, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(replay_columns.size(), 2u);
        EXPECT_EQ(replay_columns[0].column_name, "id");
        EXPECT_EQ(replay_columns[1].column_name, "val");
    }
    ASSERT_EQ(replay_conn_->closeForensicReplay(&ctx), Status::OK) << ctx.message;

    ASSERT_EQ(replay_conn_->openForensicReplayByTransactionUuid(alter_tx_uuid, &ctx), Status::OK)
        << ctx.message;
    {
        ScopedCurrentConnection current(replay_conn_.get());
        std::vector<CatalogManager::ColumnInfo> replay_columns;
        ASSERT_EQ(catalog_->getColumns(history_table.table_id, replay_columns, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(replay_columns.size(), 3u);
        EXPECT_EQ(replay_columns[2].column_name, "extra");
    }
}
