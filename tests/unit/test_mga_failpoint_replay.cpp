#include <gtest/gtest.h>

// Section 35 invariant: this test anchors bounded MGA restart and incident
// behavior only. It is not proof of universal replay-log or auto-repair
// recovery semantics.

#include <cerrno>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <memory>
#include <thread>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace
{
    struct StartupScenarioResult
    {
        Status status = Status::OK;
        std::string message;
        std::vector<MgaFailpointEvent> events;
    };

    auto runStartupScenario(const std::filesystem::path& db_path) -> StartupScenarioResult
    {
        StartupScenarioResult result{};
        Database db;
        ErrorContext ctx;

        std::vector<MgaFailpointDefinition> defs{
            {std::string(MgaFailpointTriggers::kAfterTipLoadBeforeActiveNormalization),
             MgaFailpointAction::RETURN_ERROR,
             1,
             Status::IO_ERROR,
             0,
             "startup_normalization_blocked"}};
        EXPECT_EQ(db.mga_failpoint_manager()->installSeed("startup-seed", defs, &ctx), Status::OK)
            << ctx.message;

        result.status = db.open(db_path.string(), &ctx);
        result.message = ctx.message;
        EXPECT_EQ(db.mga_failpoint_manager()->listEvents(result.events, nullptr), Status::OK);

        if (db.is_open())
        {
            db.close();
        }
        return result;
    }

    auto makeLockTag(uint64_t page_num) -> LockTag
    {
        LockTag tag{};
        tag.target_type = LockTarget::LOCK_TARGET_PAGE;
        for (size_t i = 0; i < tag.object_uuid.bytes.size(); ++i)
        {
            tag.object_uuid.bytes[i] = static_cast<uint8_t>(i + 1);
        }
        tag.page_num = page_num;
        tag.offset_num = 0;
        tag.padding = 0;
        return tag;
    }
} // namespace

class MgaFailpointReplayTest : public ::testing::Test
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
        Config::getInstance().set("garbage_collection", "enabled", "true");
        Config::getInstance().set("garbage_collection", "policy", "COMBINED");
        Config::getInstance().set("garbage_collection", "cooperative_rate", "1");

        db_path_ = scratchbird::testing::uniqueTestDbPath("mga_failpoint_replay", ".db");
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_->initializeProcArray(16, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        txn_mgr_ = db_->transaction_manager();
        lock_mgr_ = db_->lock_manager();
        sweep_mgr_ = db_->sweep_manager();
        storage_ = db_->storage_engine();
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(txn_mgr_, nullptr);
        ASSERT_NE(lock_mgr_, nullptr);
        ASSERT_NE(sweep_mgr_, nullptr);
        ASSERT_NE(storage_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        createTestTable(&ctx);
        system_user_id_ = catalog_->getSystemUserId(&ctx);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
        }
        std::filesystem::remove(db_path_);
    }

    void createTestTable(ErrorContext* ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog_->listSchemas(schemas, ctx), Status::OK) << ctx->message;

        ID schema_id{};
        if (schemas.empty())
        {
            ASSERT_EQ(catalog_->createSchema("public", "test", schema_id, ctx), Status::OK)
                << ctx->message;
        }
        else
        {
            schema_id = schemas.front().schema_id;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.max_length = 4;
        id_col.nullable = false;
        columns.push_back(id_col);

        CatalogManager::ColumnInfo value_col{};
        value_col.column_name = "val";
        value_col.data_type = static_cast<uint16_t>(DataType::INT32);
        value_col.max_length = 4;
        value_col.nullable = true;
        columns.push_back(value_col);

        ASSERT_EQ(catalog_->createTable(schema_id, "mga_failpoint_table", columns, table_id_, 0, ctx),
                  Status::OK)
            << ctx->message;
    }

    auto registerBackend() -> uint32_t
    {
        ErrorContext ctx;
        uint32_t proc_id = 0;
        EXPECT_EQ(ProcArrayManager::registerBackend(&proc_id, &ctx), Status::OK)
            << ctx.message;
        return proc_id;
    }

    void unregisterBackend(uint32_t proc_id)
    {
        ErrorContext ctx;
        (void)ProcArrayManager::unregisterBackend(proc_id, &ctx);
    }

    void armFailpoint(const std::string& seed_id, const MgaFailpointDefinition& definition)
    {
        ErrorContext ctx;
        ASSERT_EQ(db_->mga_failpoint_manager()->installSeed(seed_id, {definition}, &ctx), Status::OK)
            << ctx.message;
    }

    auto listEvents() -> std::vector<MgaFailpointEvent>
    {
        std::vector<MgaFailpointEvent> events;
        EXPECT_EQ(db_->mga_failpoint_manager()->listEvents(events, nullptr), Status::OK);
        return events;
    }

    auto makeTuple(int32_t id, int32_t value) const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer(sizeof(TupleHeader) + sizeof(int32_t) * 2);
        std::memset(buffer.data(), 0, buffer.size());
        std::memcpy(buffer.data() + sizeof(TupleHeader), &id, sizeof(int32_t));
        std::memcpy(buffer.data() + sizeof(TupleHeader) + sizeof(int32_t), &value, sizeof(int32_t));
        return buffer;
    }

    auto makeCommittedDeletedTuplePage(int32_t key_base) -> uint32_t
    {
        ScopedCurrentConnection scope(conn_.get());
        ErrorContext ctx;

        auto tuple = makeTuple(key_base, key_base * 10);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        if (storage_->insertTuple(table_id_, tuple.data(), tuple.size(), &page_id, &item_id, &ctx) !=
            Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return 0;
        }
        if (conn_->commit(&ctx) != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return 0;
        }

        if (storage_->deleteTuple(table_id_, page_id, item_id, UINT16_MAX, &ctx) != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return 0;
        }
        if (conn_->commit(&ctx) != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return 0;
        }
        return page_id;
    }

    void closeDatabase()
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
        }
    }

    auto readSystemStatePageFromFile() -> scratchbird::core::BootstrapSystemStatePage
    {
        scratchbird::core::BootstrapSystemStatePage state{};
        std::vector<uint8_t> buffer(8192, 0);
        const int fd = ::open(db_path_.c_str(), O_RDWR);
        EXPECT_GE(fd, 0) << std::strerror(errno);
        if (fd < 0)
        {
            return state;
        }

        const off_t offset = static_cast<off_t>(scratchbird::core::BOOTSTRAP_PAGE_SYSTEM_STATE) *
                             static_cast<off_t>(buffer.size());
        const ssize_t bytes = ::pread(fd, buffer.data(), buffer.size(), offset);
        EXPECT_EQ(bytes, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);
        if (bytes == static_cast<ssize_t>(buffer.size()))
        {
            std::memcpy(&state, buffer.data(), sizeof(state));
        }
        ::close(fd);
        return state;
    }

    void writeSystemStatePageToFile(
        const scratchbird::core::BootstrapSystemStatePage& state_in)
    {
        std::vector<uint8_t> buffer(8192, 0);
        const int fd = ::open(db_path_.c_str(), O_RDWR);
        ASSERT_GE(fd, 0) << std::strerror(errno);

        const off_t offset = static_cast<off_t>(scratchbird::core::BOOTSTRAP_PAGE_SYSTEM_STATE) *
                             static_cast<off_t>(buffer.size());
        const ssize_t bytes = ::pread(fd, buffer.data(), buffer.size(), offset);
        ASSERT_EQ(bytes, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);

        auto state = state_in;
        std::memcpy(buffer.data(), &state, sizeof(state));
        scratchbird::core::preparePageForWrite(
            buffer.data(),
            static_cast<uint32_t>(buffer.size()),
            scratchbird::core::BOOTSTRAP_PAGE_SYSTEM_STATE);

        const ssize_t written = ::pwrite(fd, buffer.data(), buffer.size(), offset);
        ASSERT_EQ(written, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);
        ASSERT_EQ(::fsync(fd), 0) << std::strerror(errno);
        ::close(fd);
    }

    void patchTipStateInFile(uint64_t xid, scratchbird::core::TransactionState new_state)
    {
        std::vector<uint8_t> buffer(8192, 0);
        const int fd = ::open(db_path_.c_str(), O_RDWR);
        ASSERT_GE(fd, 0) << std::strerror(errno);

        const off_t offset = static_cast<off_t>(scratchbird::core::BOOTSTRAP_PAGE_TX_MAP_ROOT) *
                             static_cast<off_t>(buffer.size());
        const ssize_t bytes = ::pread(fd, buffer.data(), buffer.size(), offset);
        ASSERT_EQ(bytes, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);

        auto* tip_header =
            reinterpret_cast<scratchbird::core::TIPPageHeader*>(buffer.data());
        auto* entries = reinterpret_cast<scratchbird::core::TIPEntry*>(
            buffer.data() + sizeof(scratchbird::core::TIPPageHeader));

        bool found = false;
        for (uint32_t i = 0; i < tip_header->num_transactions; ++i)
        {
            if (entries[i].xid == xid)
            {
                entries[i].state = static_cast<uint8_t>(new_state);
                entries[i].commit_time = 0;
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found);

        scratchbird::core::preparePageForWrite(
            buffer.data(),
            static_cast<uint32_t>(buffer.size()),
            scratchbird::core::BOOTSTRAP_PAGE_TX_MAP_ROOT);
        const ssize_t written = ::pwrite(fd, buffer.data(), buffer.size(), offset);
        ASSERT_EQ(written, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);
        ASSERT_EQ(::fsync(fd), 0) << std::strerror(errno);
        ::close(fd);
    }

    void markNextOpenAsUnclean()
    {
        auto state_page = readSystemStatePageFromFile();
        state_page.clean_shutdown = 0;
        writeSystemStatePageToFile(state_page);
    }

    void reopenDatabase()
    {
        ErrorContext ctx;
        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_->initializeProcArray(16, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        txn_mgr_ = db_->transaction_manager();
        lock_mgr_ = db_->lock_manager();
        sweep_mgr_ = db_->sweep_manager();
        storage_ = db_->storage_engine();
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(txn_mgr_, nullptr);
        ASSERT_NE(lock_mgr_, nullptr);
        ASSERT_NE(sweep_mgr_, nullptr);
        ASSERT_NE(storage_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
        system_user_id_ = catalog_->getSystemUserId(&ctx);
    }

    auto visibleRows(ConnectionContext* conn) -> std::vector<std::pair<int32_t, int32_t>>
    {
        ScopedCurrentConnection scope(conn);
        ErrorContext ctx;
        auto scan = storage_->createScan(table_id_, &ctx);
        EXPECT_NE(scan, nullptr) << ctx.message;

        std::vector<std::pair<int32_t, int32_t>> rows;
        if (!scan)
        {
            return rows;
        }

        Tuple tuple{};
        while (true)
        {
            Status status = scan->next(&tuple, &ctx);
            if (status == Status::NOT_FOUND)
            {
                break;
            }
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                break;
            }

            EXPECT_GE(tuple.data_size, sizeof(TupleHeader) + sizeof(int32_t) * 2);
            if (tuple.data_size < sizeof(TupleHeader) + sizeof(int32_t) * 2)
            {
                break;
            }
            int32_t id = 0;
            int32_t value = 0;
            std::memcpy(&id, tuple.data + sizeof(TupleHeader), sizeof(int32_t));
            std::memcpy(&value,
                        tuple.data + sizeof(TupleHeader) + sizeof(int32_t),
                        sizeof(int32_t));
            rows.emplace_back(id, value);
        }

        return rows;
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    TransactionManager* txn_mgr_ = nullptr;
    LockManager* lock_mgr_ = nullptr;
    SweepManager* sweep_mgr_ = nullptr;
    StorageEngine* storage_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID table_id_{};
    ID system_user_id_{};
};

TEST(MgaFailpointReplayStandaloneTest, StartupFailpointIsReplayable)
{
    const std::filesystem::path db_path =
        scratchbird::testing::uniqueTestDbPath("mga_failpoint_startup", ".db");
    std::filesystem::remove(db_path);

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path.string(), 8192, &ctx), Status::OK) << ctx.message;

    StartupScenarioResult first = runStartupScenario(db_path);
    StartupScenarioResult second = runStartupScenario(db_path);

    ASSERT_EQ(first.status, Status::IO_ERROR);
    ASSERT_EQ(second.status, Status::IO_ERROR);
    ASSERT_EQ(first.events.size(), 1u);
    ASSERT_EQ(second.events.size(), 1u);
    EXPECT_EQ(first.events[0].event_id, second.events[0].event_id);
    EXPECT_EQ(first.events[0].seed_id, "startup-seed");
    EXPECT_EQ(first.events[0].trigger_name,
              MgaFailpointTriggers::kAfterTipLoadBeforeActiveNormalization);
    EXPECT_EQ(first.events[0].outcome, "startup_normalization_blocked");

    std::filesystem::remove(db_path);
}

TEST_F(MgaFailpointReplayTest, TransactionLifecycleFailpointsAreWired)
{
    ErrorContext ctx;
    const uint32_t proc_id = registerBackend();
    uint64_t xid = 0;

    armFailpoint("begin-seed",
                 {std::string(MgaFailpointTriggers::kAfterTxidAllocationBeforeActive),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "begin_blocked"});
    ASSERT_EQ(txn_mgr_->beginTransaction(proc_id, xid, &ctx), Status::IO_ERROR);
    auto events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name, MgaFailpointTriggers::kAfterTxidAllocationBeforeActive);
    EXPECT_TRUE(events[0].has_txid);

    ASSERT_EQ(db_->mga_failpoint_manager()->clear(&ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;

    armFailpoint("commit-pre-seed",
                 {std::string(MgaFailpointTriggers::kAfterDirtyFlushBeforeTipTerminal),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "commit_pre_tip_blocked"});
    ASSERT_EQ(txn_mgr_->commitTransaction(proc_id, xid, &ctx), Status::IO_ERROR);
    TransactionState state = TransactionState::ABORTED;
    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, TransactionState::ACTIVE);
    ASSERT_EQ(txn_mgr_->rollbackTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(db_->mga_failpoint_manager()->clear(&ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;

    armFailpoint("commit-post-seed",
                 {std::string(MgaFailpointTriggers::kAfterTipTerminalBeforeClientAck),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "commit_durable_before_ack"});
    ASSERT_EQ(txn_mgr_->commitTransaction(proc_id, xid, &ctx), Status::IO_ERROR);
    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, TransactionState::COMMITTED);

    ASSERT_EQ(db_->mga_failpoint_manager()->clear(&ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;

    const std::string gid = "mgw-014-prepare";
    armFailpoint("prepare-seed",
                 {std::string(MgaFailpointTriggers::kBetweenPreparedRecordAndTipPrepared),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "prepare_catalog_only"});
    ASSERT_EQ(txn_mgr_->prepareTransaction(proc_id, xid, gid, system_user_id_, &ctx), Status::IO_ERROR);
    std::vector<CatalogManager::PreparedTransactionInfo> prepared;
    ASSERT_EQ(catalog_->listPreparedTransactions(prepared, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(prepared.size(), 1u);
    EXPECT_EQ(prepared[0].gid, gid);
    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, TransactionState::ACTIVE);

    ASSERT_EQ(catalog_->deletePreparedTransaction(gid, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr_->rollbackTransaction(proc_id, xid, &ctx), Status::OK) << ctx.message;
    unregisterBackend(proc_id);
}

TEST_F(MgaFailpointReplayTest, GarbageCollectionFailpointsAreRecorded)
{
    ErrorContext ctx;

    const uint32_t chain_page = makeCommittedDeletedTuplePage(11);
    armFailpoint("gc-seed",
                 {std::string(MgaFailpointTriggers::kAfterChainUnlinkBeforeCompactionPublish),
                  MgaFailpointAction::MARK_ONLY,
                  1,
                  Status::OK,
                  0,
                  "chain_unlinked"});
    db_->garbage_collector()->processPageCooperative(chain_page, &ctx);
    auto events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name,
              MgaFailpointTriggers::kAfterChainUnlinkBeforeCompactionPublish);
    EXPECT_EQ(events[0].outcome, "chain_unlinked");

    ASSERT_EQ(db_->mga_failpoint_manager()->clear(&ctx), Status::OK) << ctx.message;
    const uint32_t index_page = makeCommittedDeletedTuplePage(22);
    armFailpoint("index-seed",
                 {std::string(MgaFailpointTriggers::kAfterHeapReclaimBeforeDeadEntryDelete),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "index_cleanup_skipped"});
    db_->garbage_collector()->processPageCooperative(index_page, &ctx);
    events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name,
              MgaFailpointTriggers::kAfterHeapReclaimBeforeDeadEntryDelete);
    EXPECT_EQ(events[0].outcome, "index_cleanup_skipped");
}

TEST_F(MgaFailpointReplayTest, SweepCheckpointLossCanBeReplayedAndRecovered)
{
    ErrorContext ctx;
    armFailpoint("sweep-seed",
                 {std::string(MgaFailpointTriggers::kSweepCheckpointWriteLoss),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "checkpoint_lost"});

    ASSERT_EQ(sweep_mgr_->executeSweep(true, &ctx), Status::IO_ERROR);
    auto events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name, MgaFailpointTriggers::kSweepCheckpointWriteLoss);

    ConnectionContext::setCurrent(nullptr);
    conn_.reset();
    db_->close();

    db_ = std::make_unique<Database>();
    ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(db_->initializeProcArray(16, &ctx), Status::OK) << ctx.message;
    catalog_ = db_->catalog_manager();
    txn_mgr_ = db_->transaction_manager();
    lock_mgr_ = db_->lock_manager();
    sweep_mgr_ = db_->sweep_manager();
    storage_ = db_->storage_engine();
    ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
    ConnectionContext::setCurrent(conn_.get());
    ASSERT_EQ(db_->mga_failpoint_manager()->clear(&ctx), Status::OK) << ctx.message;

    ASSERT_EQ(sweep_mgr_->executeSweep(true, &ctx), Status::OK) << ctx.message;
}

TEST_F(MgaFailpointReplayTest, CommitPreTipFailpointAbortsInsertedRowAcrossUncleanRestart)
{
    ErrorContext ctx;
    {
        ScopedCurrentConnection scope(conn_.get());
        auto tuple = makeTuple(101, 1001);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(storage_->insertTuple(table_id_,
                                        tuple.data(),
                                        tuple.size(),
                                        &page_id,
                                        &item_id,
                                        &ctx),
                  Status::OK)
            << ctx.message;
    }

    const uint64_t xid = conn_->getCurrentXid();
    armFailpoint("commit-pre-restart-seed",
                 {std::string(MgaFailpointTriggers::kAfterDirtyFlushBeforeTipTerminal),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "commit_pre_tip_blocked"});

    ASSERT_EQ(conn_->commit(&ctx), Status::IO_ERROR);
    auto events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name, MgaFailpointTriggers::kAfterDirtyFlushBeforeTipTerminal);

    scratchbird::core::TransactionState state = scratchbird::core::TransactionState::COMMITTED;
    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::ACTIVE);

    closeDatabase();
    markNextOpenAsUnclean();
    patchTipStateInFile(xid, scratchbird::core::TransactionState::ACTIVE);
    reopenDatabase();

    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::ABORTED);
    EXPECT_TRUE(visibleRows(conn_.get()).empty());
}

TEST_F(MgaFailpointReplayTest, CommitPostTipFailpointKeepsInsertedRowCommittedAcrossRestart)
{
    ErrorContext ctx;
    {
        ScopedCurrentConnection scope(conn_.get());
        auto tuple = makeTuple(202, 2002);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(storage_->insertTuple(table_id_,
                                        tuple.data(),
                                        tuple.size(),
                                        &page_id,
                                        &item_id,
                                        &ctx),
                  Status::OK)
            << ctx.message;
    }

    const uint64_t xid = conn_->getCurrentXid();
    armFailpoint("commit-post-restart-seed",
                 {std::string(MgaFailpointTriggers::kAfterTipTerminalBeforeClientAck),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "commit_durable_before_ack"});

    ASSERT_EQ(conn_->commit(&ctx), Status::IO_ERROR);
    auto events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name, MgaFailpointTriggers::kAfterTipTerminalBeforeClientAck);

    scratchbird::core::TransactionState state = scratchbird::core::TransactionState::ACTIVE;
    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::COMMITTED);

    uint64_t commit_seq_before_restart = 0;
    ASSERT_EQ(txn_mgr_->getCommittedTransactionSequence(xid, commit_seq_before_restart, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_GT(commit_seq_before_restart, 0u);

    closeDatabase();
    markNextOpenAsUnclean();
    reopenDatabase();

    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::COMMITTED);

    uint64_t commit_seq_after_restart = 0;
    ASSERT_EQ(txn_mgr_->getCommittedTransactionSequence(xid, commit_seq_after_restart, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(commit_seq_after_restart, 0u);

    TransactionSnapshot snapshot{};
    ASSERT_EQ(txn_mgr_->captureSnapshot(snapshot, &ctx), Status::OK) << ctx.message;
    EXPECT_GE(snapshot.snapshot_commit_seqno_high, commit_seq_before_restart);

    const auto rows = visibleRows(conn_.get());
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].first, 202);
    EXPECT_EQ(rows[0].second, 2002);
}

TEST_F(MgaFailpointReplayTest, DevelopmentUnsafeCommitSkipsPreTipFenceAndTracksRisk)
{
    ErrorContext ctx;
    txn_mgr_->setDurabilityMode(DurabilityMode::DEVELOPMENT_UNSAFE);

    {
        ScopedCurrentConnection scope(conn_.get());
        auto tuple = makeTuple(303, 3003);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(storage_->insertTuple(table_id_,
                                        tuple.data(),
                                        tuple.size(),
                                        &page_id,
                                        &item_id,
                                        &ctx),
                  Status::OK)
            << ctx.message;
    }

    const uint64_t xid = conn_->getCurrentXid();
    armFailpoint("commit-unsafe-pretip-seed",
                 {std::string(MgaFailpointTriggers::kAfterDirtyFlushBeforeTipTerminal),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "unsafe_should_skip_pre_tip_fence"});

    ASSERT_EQ(conn_->commit(&ctx), Status::OK) << ctx.message;

    auto events = listEvents();
    EXPECT_TRUE(events.empty());

    scratchbird::core::TransactionState state = scratchbird::core::TransactionState::ACTIVE;
    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::COMMITTED);

    const auto unsafe_stats = txn_mgr_->getStats();
    EXPECT_EQ(unsafe_stats.durability_mode, DurabilityMode::DEVELOPMENT_UNSAFE);
    EXPECT_EQ(unsafe_stats.commits_acknowledged_at_risk, 1u);

    ASSERT_EQ(db_->mga_failpoint_manager()->clear(&ctx), Status::OK) << ctx.message;
    txn_mgr_->setDurabilityMode(DurabilityMode::STRICT);

    ASSERT_EQ(conn_->commit(&ctx), Status::OK) << ctx.message;
    const auto strict_stats = txn_mgr_->getStats();
    EXPECT_EQ(strict_stats.durability_mode, DurabilityMode::STRICT);
    EXPECT_EQ(strict_stats.commits_acknowledged_at_risk, 0u);
}

TEST_F(MgaFailpointReplayTest, PrepareCatalogOnlyFailpointPromotesToPreparedAcrossRestart)
{
    ErrorContext ctx;
    {
        ScopedCurrentConnection scope(conn_.get());
        auto tuple = makeTuple(303, 3003);
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        ASSERT_EQ(storage_->insertTuple(table_id_,
                                        tuple.data(),
                                        tuple.size(),
                                        &page_id,
                                        &item_id,
                                        &ctx),
                  Status::OK)
            << ctx.message;
    }

    const uint64_t xid = conn_->getCurrentXid();
    const std::string gid = "rmga016_prepare_catalog_only";
    armFailpoint("prepare-restart-seed",
                 {std::string(MgaFailpointTriggers::kBetweenPreparedRecordAndTipPrepared),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::IO_ERROR,
                  0,
                  "prepare_catalog_only"});

    ASSERT_EQ(conn_->prepareTransaction(gid, &ctx), Status::IO_ERROR);
    auto events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name, MgaFailpointTriggers::kBetweenPreparedRecordAndTipPrepared);

    CatalogManager::PreparedTransactionInfo info{};
    ASSERT_EQ(catalog_->getPreparedTransactionByGid(gid, info, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(info.txn_id, xid);

    scratchbird::core::TransactionState state = scratchbird::core::TransactionState::COMMITTED;
    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::ACTIVE);

    closeDatabase();
    markNextOpenAsUnclean();
    patchTipStateInFile(xid, scratchbird::core::TransactionState::ACTIVE);
    reopenDatabase();

    ASSERT_EQ(txn_mgr_->getTransactionState(xid, state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, scratchbird::core::TransactionState::PREPARED);
    std::unique_ptr<ConnectionContext> prepared_reader;
    ASSERT_EQ(db_->connect(prepared_reader, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(visibleRows(prepared_reader.get()).empty());

    ASSERT_EQ(txn_mgr_->commitPreparedTransaction(gid, &ctx), Status::OK) << ctx.message;
    std::unique_ptr<ConnectionContext> committed_reader;
    ASSERT_EQ(db_->connect(committed_reader, &ctx), Status::OK) << ctx.message;
    const auto rows = visibleRows(committed_reader.get());
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].first, 303);
    EXPECT_EQ(rows[0].second, 3003);
}

TEST_F(MgaFailpointReplayTest, DeadlockFailpointsAreReachable)
{
    ErrorContext ctx;
    const uint32_t proc1 = registerBackend();
    const uint32_t proc2 = registerBackend();

    uint64_t xid1 = 0;
    uint64_t xid2 = 0;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc1, xid1, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc2, xid2, &ctx), Status::OK) << ctx.message;

    const LockTag tag_a = makeLockTag(100);
    const LockTag tag_b = makeLockTag(200);

    std::atomic<int> stage1{0};
    std::atomic<int> stage2{0};
    std::atomic<Status> t1_status{Status::OK};
    std::atomic<Status> t2_status{Status::OK};

    auto worker = [&](uint32_t proc_id,
                      const LockTag& first,
                      const LockTag& second,
                      std::atomic<Status>& status_out) {
        ErrorContext thread_ctx;

        Status first_status = lock_mgr_->acquireLock(
            proc_id, first, LockMode::LOCK_EXCLUSIVE, false, 0, &thread_ctx);
        if (first_status != Status::OK)
        {
            status_out.store(first_status);
            return;
        }
        stage1.fetch_add(1);

        auto start_wait = std::chrono::steady_clock::now();
        while (stage1.load() < 2)
        {
            if (std::chrono::steady_clock::now() - start_wait > std::chrono::seconds(1))
            {
                status_out.store(Status::LOCK_TIMEOUT);
                lock_mgr_->releaseAllLocks(proc_id, nullptr);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        stage2.fetch_add(1);
        Status status = lock_mgr_->acquireLock(
            proc_id, second, LockMode::LOCK_EXCLUSIVE, true, 1000, &thread_ctx);
        status_out.store(status);
        lock_mgr_->releaseAllLocks(proc_id, nullptr);
    };

    std::thread t1(worker, proc1, tag_a, tag_b, std::ref(t1_status));
    std::thread t2(worker, proc2, tag_b, tag_a, std::ref(t2_status));

    auto wait_start = std::chrono::steady_clock::now();
    while (stage2.load() < 2)
    {
        ASSERT_LT(std::chrono::steady_clock::now() - wait_start, std::chrono::seconds(2));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    armFailpoint("deadlock-stall-seed",
                 {std::string(MgaFailpointTriggers::kDeadlockDetectorStall),
                  MgaFailpointAction::STALL_THEN_CONTINUE,
                  1,
                  Status::OK,
                  1,
                  "detector_stalled"});
    ASSERT_EQ(lock_mgr_->detectDeadlocks(&ctx), Status::OK) << ctx.message;

    t1.join();
    t2.join();

    auto events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name, MgaFailpointTriggers::kDeadlockDetectorStall);
    EXPECT_EQ(events[0].outcome, "detector_stalled");

    ASSERT_EQ(db_->mga_failpoint_manager()->clear(&ctx), Status::OK) << ctx.message;
    TransactionState state = TransactionState::ABORTED;
    if (txn_mgr_->getTransactionState(xid1, state, &ctx) == Status::OK && state == TransactionState::ACTIVE)
    {
        ASSERT_EQ(txn_mgr_->rollbackTransaction(proc1, xid1, &ctx), Status::OK) << ctx.message;
    }
    if (txn_mgr_->getTransactionState(xid2, state, &ctx) == Status::OK && state == TransactionState::ACTIVE)
    {
        ASSERT_EQ(txn_mgr_->rollbackTransaction(proc2, xid2, &ctx), Status::OK) << ctx.message;
    }

    // Rebuild a simple deadlock and fail at victim selection before any abort happens.
    xid1 = 0;
    xid2 = 0;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc1, xid1, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc2, xid2, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(lock_mgr_->acquireLock(proc1, tag_a, LockMode::LOCK_EXCLUSIVE, false, 0, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(lock_mgr_->acquireLock(proc2, tag_b, LockMode::LOCK_EXCLUSIVE, false, 0, &ctx), Status::OK)
        << ctx.message;

    std::atomic<Status> wait1{Status::OK};
    std::atomic<Status> wait2{Status::OK};
    std::thread w1([&] {
        ErrorContext thread_ctx;
        wait1.store(lock_mgr_->acquireLock(proc1, tag_b, LockMode::LOCK_EXCLUSIVE, true, 500, &thread_ctx));
    });
    std::thread w2([&] {
        ErrorContext thread_ctx;
        wait2.store(lock_mgr_->acquireLock(proc2, tag_a, LockMode::LOCK_EXCLUSIVE, true, 500, &thread_ctx));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    armFailpoint("deadlock-victim-seed",
                 {std::string(MgaFailpointTriggers::kDeadlockVictimSelectionFailure),
                  MgaFailpointAction::RETURN_ERROR,
                  1,
                  Status::DEADLOCK,
                  0,
                  "victim_selection_failed"});
    ASSERT_EQ(lock_mgr_->detectDeadlocks(&ctx), Status::DEADLOCK);

    lock_mgr_->releaseAllLocks(proc1, nullptr);
    lock_mgr_->releaseAllLocks(proc2, nullptr);
    w1.join();
    w2.join();

    events = listEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].trigger_name, MgaFailpointTriggers::kDeadlockVictimSelectionFailure);
    EXPECT_EQ(events[0].outcome, "victim_selection_failed");

    state = TransactionState::ABORTED;
    if (txn_mgr_->getTransactionState(xid1, state, &ctx) == Status::OK && state == TransactionState::ACTIVE)
    {
        ASSERT_EQ(txn_mgr_->rollbackTransaction(proc1, xid1, &ctx), Status::OK) << ctx.message;
    }
    if (txn_mgr_->getTransactionState(xid2, state, &ctx) == Status::OK && state == TransactionState::ACTIVE)
    {
        ASSERT_EQ(txn_mgr_->rollbackTransaction(proc2, xid2, &ctx), Status::OK) << ctx.message;
    }

    unregisterBackend(proc1);
    unregisterBackend(proc2);
}
