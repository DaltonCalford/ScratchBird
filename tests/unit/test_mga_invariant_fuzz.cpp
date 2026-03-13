#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace
{
    struct TestRow
    {
        int32_t id = 0;
        int32_t value = 0;
    };

    struct VisibleScanResult
    {
        Status status = Status::OK;
        std::string message;
        std::map<int32_t, int32_t> rows;
        bool duplicate_row_ids = false;
    };

    struct RowState
    {
        int32_t value = 0;
        TID tid{};
    };

    enum class WriterOperationKind : uint8_t
    {
        INSERT,
        UPDATE,
        DELETE_ROW,
        NOOP,
    };

    struct WriterPlan
    {
        WriterOperationKind kind = WriterOperationKind::NOOP;
        int32_t row_id = 0;
        int32_t value = 0;
        bool commit = true;
        TID current_tid{};
    };

    struct WriterOutcome
    {
        Status status = Status::OK;
        std::string message;
        WriterPlan plan{};
        TID new_tid{};
    };

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

    auto buildTuple(int32_t id, int32_t value) -> std::vector<uint8_t>
    {
        TupleHeader header{};
        header.xmin = config::DEFAULT_INITIAL_XID;
        header.xmax = 0;
        header.back_version_gpid = INVALID_GPID;
        header.back_version_slot = 0;
        header.ctid_gpid = INVALID_GPID;
        header.ctid_slot = 0;
        header.infomask = 0;
        header.null_bitmap_offset = 0;
        header.padding = 0;
        header.session_id = ID{};

        TestRow row{id, value};
        std::vector<uint8_t> tuple(sizeof(TupleHeader) + sizeof(TestRow), 0);
        std::memcpy(tuple.data(), &header, sizeof(TupleHeader));
        std::memcpy(tuple.data() + sizeof(TupleHeader), &row, sizeof(TestRow));
        return tuple;
    }

    auto decodeVisibleRows(Database& db,
                           StorageEngine& storage,
                           ConnectionContext* conn,
                           const ID& table_id) -> VisibleScanResult
    {
        ScopedCurrentConnection scope(conn);
        ErrorContext ctx;
        VisibleScanResult result{};

        auto scan = storage.createScan(table_id, &ctx);
        if (!scan)
        {
            result.status = Status::IO_ERROR;
            result.message = "createScan returned null";
            return result;
        }

        Tuple tuple{};
        while (true)
        {
            Status status = scan->next(&tuple, &ctx);
            if (status == Status::NOT_FOUND)
            {
                break;
            }
            if (status != Status::OK)
            {
                result.status = status;
                result.message = ctx.message;
                return result;
            }

            if (tuple.data_size < sizeof(TupleHeader) + sizeof(TestRow))
            {
                result.status = Status::DATA_CORRUPTED;
                result.message = "Visible scan returned undersized tuple";
                return result;
            }

            const auto* row =
                reinterpret_cast<const TestRow*>(tuple.data + sizeof(TupleHeader));
            auto inserted = result.rows.emplace(row->id, row->value);
            if (!inserted.second)
            {
                result.duplicate_row_ids = true;
                inserted.first->second = row->value;
            }
        }

        return result;
    }

    auto describePhysicalRows(Database& db,
                              StorageEngine& storage,
                              ConnectionContext* conn,
                              const ID& table_id) -> std::string
    {
        ScopedCurrentConnection scope(conn);
        ErrorContext ctx;
        std::ostringstream out;
        auto scan = storage.createScanAll(table_id, &ctx);
        if (!scan)
        {
            out << "createScanAll returned null";
            return out.str();
        }

        Tuple tuple{};
        while (true)
        {
            Status status = scan->next(&tuple, &ctx);
            if (status == Status::NOT_FOUND)
            {
                break;
            }
            if (status != Status::OK)
            {
                out << "scan_all_status=" << static_cast<int>(status)
                    << " message=" << ctx.message;
                break;
            }

            if (tuple.data_size < sizeof(TupleHeader) + sizeof(TestRow))
            {
                out << " undersized_tuple tid=(" << getPageNumber(tuple.tid.gpid)
                    << "," << tuple.tid.slot << ")";
                continue;
            }

            const auto* hdr = reinterpret_cast<const TupleHeader*>(tuple.data);
            const auto* row =
                reinterpret_cast<const TestRow*>(tuple.data + sizeof(TupleHeader));
            out << " tid=(" << getPageNumber(tuple.tid.gpid) << "," << tuple.tid.slot << ")"
                << " row_id=" << row->id
                << " value=" << row->value
                << " xmin=" << hdr->xmin
                << " xmax=" << hdr->xmax
                << " flags=" << hdr->infomask
                << " ctid=(" << getPageNumber(hdr->ctid_gpid) << "," << hdr->ctid_slot << ")"
                << " back=(" << getPageNumber(hdr->back_version_gpid) << ","
                << hdr->back_version_slot << ")";
        }

        return out.str();
    }

    auto describeSnapshot(ConnectionContext* conn) -> std::string
    {
        std::ostringstream out;
        out << "reader_xid=" << (conn ? conn->getCurrentXid() : 0);
        if (conn == nullptr)
        {
            return out.str();
        }

        const TransactionSnapshot* snapshot = conn->getRetainedTransactionSnapshot();
        if (snapshot == nullptr)
        {
            out << " snapshot=null";
            return out.str();
        }

        out << " snapshot_low=" << snapshot->snapshot_txid_low
            << " snapshot_high=" << snapshot->snapshot_txid_high
            << " active=[";
        for (size_t i = 0; i < snapshot->active_txid_set.size(); ++i)
        {
            if (i != 0)
            {
                out << ",";
            }
            out << snapshot->active_txid_set[i];
        }
        out << "]";
        return out.str();
    }
} // namespace

class MgaInvariantFuzzTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Config::getInstance().set("garbage_collection", "enabled", "true");
        Config::getInstance().set("garbage_collection", "policy", "COMBINED");
        Config::getInstance().set("garbage_collection", "cooperative_rate", "1");

        db_path_ = scratchbird::testing::uniqueTestDbPath("mga_invariant_fuzz", ".db");
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_->initializeProcArray(32, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        txn_mgr_ = db_->transaction_manager();
        storage_ = db_->storage_engine();
        sweep_mgr_ = db_->sweep_manager();
        ASSERT_NE(catalog_, nullptr);
        ASSERT_NE(txn_mgr_, nullptr);
        ASSERT_NE(storage_, nullptr);
        ASSERT_NE(sweep_mgr_, nullptr);

        system_user_id_ = catalog_->getSystemUserId(&ctx);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        if (db_)
        {
            db_->close();
            db_.reset();
        }
        std::filesystem::remove(db_path_);
    }

    auto createTestTable(const std::string& table_name) -> ID
    {
        ErrorContext ctx;
        std::vector<CatalogManager::SchemaInfo> schemas;
        if (catalog_->listSchemas(schemas, &ctx) != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return ID{};
        }

        ID schema_id{};
        if (schemas.empty())
        {
            if (catalog_->createSchema("public", "test", schema_id, &ctx) != Status::OK)
            {
                ADD_FAILURE() << ctx.message;
                return ID{};
            }
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
        value_col.nullable = false;
        columns.push_back(value_col);

        ID table_id{};
        if (catalog_->createTable(schema_id, table_name, columns, table_id, 0, &ctx) != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return ID{};
        }
        return table_id;
    }

    auto connectContext(bool read_only, IsolationLevel isolation_level)
        -> std::unique_ptr<ConnectionContext>
    {
        ErrorContext ctx;
        std::unique_ptr<ConnectionContext> conn;
        EXPECT_EQ(db_->connect(conn, &ctx), Status::OK) << ctx.message;
        if (conn)
        {
            EXPECT_EQ(conn->startTransaction(read_only, isolation_level, true, &ctx), Status::OK)
                << ctx.message;
        }
        return conn;
    }

    auto seedBaseRows(ConnectionContext* writer,
                      const ID& table_id,
                      const std::map<int32_t, int32_t>& baseline)
        -> std::map<int32_t, RowState>
    {
        ScopedCurrentConnection scope(writer);
        ErrorContext ctx;
        std::map<int32_t, RowState> model;

        for (const auto& [row_id, value] : baseline)
        {
            auto tuple = buildTuple(row_id, value);
            uint32_t page_id = 0;
            uint16_t item_id = 0;
            EXPECT_EQ(storage_->insertTuple(table_id,
                                            tuple.data(),
                                            static_cast<uint32_t>(tuple.size()),
                                            &page_id,
                                            &item_id,
                                            &ctx),
                      Status::OK)
                << ctx.message;
            model.emplace(row_id,
                          RowState{value,
                                   TID(makeGPID(PRIMARY_TABLESPACE_ID,
                                                static_cast<uint64_t>(page_id)),
                                       item_id)});
        }

        EXPECT_EQ(writer->commit(&ctx), Status::OK) << ctx.message;
        return model;
    }

    auto buildWriterPlan(const std::map<int32_t, RowState>& model,
                         std::mt19937_64& rng,
                         uint64_t round) const -> WriterPlan
    {
        WriterPlan plan{};
        std::uniform_int_distribution<int> commit_pick(0, 3);
        plan.commit = commit_pick(rng) != 0;

        std::vector<int32_t> existing_ids;
        std::vector<int32_t> missing_ids;
        for (int32_t row_id = 1; row_id <= 3; ++row_id)
        {
            if (auto it = model.find(row_id); it != model.end())
            {
                existing_ids.push_back(row_id);
            }
            else
            {
                missing_ids.push_back(row_id);
            }
        }

        std::uniform_int_distribution<int> op_pick(0, 3);
        int op = op_pick(rng);
        if (missing_ids.empty() && op == 0)
        {
            op = 1;
        }
        if (existing_ids.empty() && (op == 1 || op == 2))
        {
            op = 0;
        }

        switch (op)
        {
            case 0:
            {
                plan.kind = WriterOperationKind::INSERT;
                std::uniform_int_distribution<size_t> row_pick(0, missing_ids.size() - 1);
                plan.row_id = missing_ids[row_pick(rng)];
                plan.value = static_cast<int32_t>(round * 10 + plan.row_id);
                break;
            }
            case 1:
            {
                plan.kind = WriterOperationKind::UPDATE;
                std::uniform_int_distribution<size_t> row_pick(0, existing_ids.size() - 1);
                plan.row_id = existing_ids[row_pick(rng)];
                plan.current_tid = model.at(plan.row_id).tid;
                plan.value = static_cast<int32_t>(round * 10 + plan.row_id + 1000);
                break;
            }
            case 2:
            {
                plan.kind = WriterOperationKind::DELETE_ROW;
                std::uniform_int_distribution<size_t> row_pick(0, existing_ids.size() - 1);
                plan.row_id = existing_ids[row_pick(rng)];
                plan.current_tid = model.at(plan.row_id).tid;
                break;
            }
            default:
            {
                plan.kind = WriterOperationKind::NOOP;
                plan.commit = false;
                break;
            }
        }

        return plan;
    }

    auto executeWriterPlan(ConnectionContext* writer,
                           const ID& table_id,
                           const WriterPlan& plan) -> WriterOutcome
    {
        ScopedCurrentConnection scope(writer);
        ErrorContext ctx;
        WriterOutcome outcome{};
        outcome.plan = plan;

        switch (plan.kind)
        {
            case WriterOperationKind::INSERT:
            {
                auto tuple = buildTuple(plan.row_id, plan.value);
                uint32_t page_id = 0;
                uint16_t item_id = 0;
                outcome.status = storage_->insertTuple(table_id,
                                                       tuple.data(),
                                                       static_cast<uint32_t>(tuple.size()),
                                                       &page_id,
                                                       &item_id,
                                                       &ctx);
                if (outcome.status == Status::OK)
                {
                    outcome.new_tid = TID(makeGPID(PRIMARY_TABLESPACE_ID,
                                                   static_cast<uint64_t>(page_id)),
                                          item_id);
                }
                break;
            }
            case WriterOperationKind::UPDATE:
            {
                auto tuple = buildTuple(plan.row_id, plan.value);
                uint32_t new_page_id = 0;
                uint16_t new_item_id = 0;
                outcome.status = storage_->updateTuple(table_id,
                                                       static_cast<uint32_t>(getPageNumber(plan.current_tid.gpid)),
                                                       plan.current_tid.slot,
                                                       tuple.data(),
                                                       static_cast<uint32_t>(tuple.size()),
                                                       &new_page_id,
                                                       &new_item_id,
                                                       &ctx);
                if (outcome.status == Status::OK)
                {
                    outcome.new_tid = TID(makeGPID(PRIMARY_TABLESPACE_ID,
                                                   static_cast<uint64_t>(new_page_id)),
                                          new_item_id);
                }
                break;
            }
            case WriterOperationKind::DELETE_ROW:
            {
                outcome.status = storage_->deleteTuple(table_id,
                                                       static_cast<uint32_t>(getPageNumber(plan.current_tid.gpid)),
                                                       plan.current_tid.slot,
                                                       UINT16_MAX,
                                                       &ctx);
                break;
            }
            case WriterOperationKind::NOOP:
                outcome.status = Status::OK;
                break;
        }

        if (outcome.status == Status::OK)
        {
            outcome.status = plan.commit ? writer->commit(&ctx) : writer->rollback(&ctx);
        }

        outcome.message = ctx.message;
        return outcome;
    }

    auto valuesOnly(const std::map<int32_t, RowState>& model) const
        -> std::map<int32_t, int32_t>
    {
        std::map<int32_t, int32_t> values;
        for (const auto& [row_id, state] : model)
        {
            values.emplace(row_id, state.value);
        }
        return values;
    }

    void assertTransactionInvariants(const std::map<uint64_t, TransactionState>& expected_states)
    {
        ErrorContext ctx;
        TransactionSnapshot snapshot{};
        ASSERT_EQ(txn_mgr_->captureSnapshot(snapshot, &ctx), Status::OK) << ctx.message;
        EXPECT_TRUE(std::is_sorted(snapshot.active_txid_set.begin(), snapshot.active_txid_set.end()));
        EXPECT_EQ(std::adjacent_find(snapshot.active_txid_set.begin(), snapshot.active_txid_set.end()),
                  snapshot.active_txid_set.end());
        EXPECT_LE(snapshot.snapshot_txid_low, snapshot.snapshot_txid_high);

        const uint64_t future_reader = txn_mgr_->getCurrentXid() + 1;
        EXPECT_LE(txn_mgr_->getOldestXid(), txn_mgr_->getOldestActiveXid());
        EXPECT_LE(txn_mgr_->getOldestXid(), txn_mgr_->getOldestSnapshot());
        EXPECT_LE(txn_mgr_->getOldestActiveXid(), future_reader);
        EXPECT_LE(txn_mgr_->getOldestSnapshot(), future_reader);

        for (uint64_t active_xid : snapshot.active_txid_set)
        {
            TransactionState state = TransactionState::ABORTED;
            ASSERT_EQ(txn_mgr_->getTransactionState(active_xid, state, &ctx), Status::OK)
                << ctx.message;
            EXPECT_TRUE(state == TransactionState::ACTIVE || state == TransactionState::PREPARED);
            EXPECT_LT(active_xid, snapshot.snapshot_txid_high);
        }

        for (const auto& [xid, expected_state] : expected_states)
        {
            TransactionState actual_state = TransactionState::ABORTED;
            ASSERT_EQ(txn_mgr_->getTransactionState(xid, actual_state, &ctx), Status::OK)
                << ctx.message;
            EXPECT_EQ(actual_state, expected_state);

            const bool current_visible = txn_mgr_->isVersionVisible(xid, future_reader);
            EXPECT_EQ(current_visible, expected_state == TransactionState::COMMITTED);

            const bool in_snapshot_active =
                std::binary_search(snapshot.active_txid_set.begin(),
                                   snapshot.active_txid_set.end(),
                                   xid);
            const bool snapshot_visible =
                txn_mgr_->isCreateVisibleInSnapshot(xid, future_reader, snapshot);
            const bool expected_snapshot_visible =
                expected_state == TransactionState::COMMITTED &&
                xid < snapshot.snapshot_txid_high &&
                !in_snapshot_active;
            EXPECT_EQ(snapshot_visible, expected_snapshot_visible);

            if (expected_state == TransactionState::ACTIVE ||
                expected_state == TransactionState::PREPARED)
            {
                EXPECT_TRUE(in_snapshot_active);
            }
            else
            {
                EXPECT_FALSE(in_snapshot_active);
            }
        }
    }

    void runPublicationCorpus(uint64_t seed)
    {
        struct BackendState
        {
            uint32_t proc_id = 0;
            std::optional<uint64_t> active_xid;
            std::optional<std::string> prepared_gid;
            std::optional<uint64_t> prepared_xid;
        };

        ErrorContext ctx;
        std::array<BackendState, 3> backends{};
        for (auto& backend : backends)
        {
            ASSERT_EQ(ProcArrayManager::registerBackend(&backend.proc_id, &ctx), Status::OK)
                << ctx.message;
        }

        std::map<uint64_t, TransactionState> expected_states;
        std::mt19937_64 rng(seed);
        for (uint64_t round = 0; round < 36; ++round)
        {
            std::uniform_int_distribution<size_t> backend_pick(0, backends.size() - 1);
            BackendState& backend = backends[backend_pick(rng)];

            if (backend.prepared_gid.has_value())
            {
                const bool commit_prepared = (rng() & 1ULL) == 0;
                if (commit_prepared)
                {
                    ASSERT_EQ(txn_mgr_->commitPreparedTransaction(*backend.prepared_gid, &ctx), Status::OK)
                        << ctx.message;
                    expected_states[*backend.prepared_xid] = TransactionState::COMMITTED;
                }
                else
                {
                    ASSERT_EQ(txn_mgr_->rollbackPreparedTransaction(*backend.prepared_gid, &ctx), Status::OK)
                        << ctx.message;
                    expected_states[*backend.prepared_xid] = TransactionState::ABORTED;
                }
                backend.prepared_gid.reset();
                backend.prepared_xid.reset();
                assertTransactionInvariants(expected_states);
                continue;
            }

            if (!backend.active_xid.has_value())
            {
                uint64_t xid = 0;
                ASSERT_EQ(txn_mgr_->beginTransaction(backend.proc_id, xid, &ctx), Status::OK)
                    << ctx.message;
                backend.active_xid = xid;
                expected_states[xid] = TransactionState::ACTIVE;
                assertTransactionInvariants(expected_states);
                continue;
            }

            std::uniform_int_distribution<int> action_pick(0, 2);
            const int action = action_pick(rng);
            const uint64_t xid = *backend.active_xid;

            if (action == 0)
            {
                ASSERT_EQ(txn_mgr_->commitTransaction(backend.proc_id, xid, &ctx), Status::OK)
                    << ctx.message;
                expected_states[xid] = TransactionState::COMMITTED;
                backend.active_xid.reset();
            }
            else if (action == 1)
            {
                ASSERT_EQ(txn_mgr_->rollbackTransaction(backend.proc_id, xid, &ctx), Status::OK)
                    << ctx.message;
                expected_states[xid] = TransactionState::ABORTED;
                backend.active_xid.reset();
            }
            else
            {
                const std::string gid =
                    "mgw015_prepare_" + std::to_string(seed) + "_" + std::to_string(round);
                ASSERT_EQ(txn_mgr_->prepareTransaction(backend.proc_id,
                                                       xid,
                                                       gid,
                                                       system_user_id_,
                                                       &ctx),
                          Status::OK)
                    << ctx.message;
                expected_states[xid] = TransactionState::PREPARED;
                backend.prepared_gid = gid;
                backend.prepared_xid = xid;
                backend.active_xid.reset();
            }

            assertTransactionInvariants(expected_states);
        }

        for (auto& backend : backends)
        {
            if (backend.prepared_gid.has_value())
            {
                ASSERT_EQ(txn_mgr_->rollbackPreparedTransaction(*backend.prepared_gid, &ctx), Status::OK)
                    << ctx.message;
                expected_states[*backend.prepared_xid] = TransactionState::ABORTED;
                backend.prepared_gid.reset();
                backend.prepared_xid.reset();
            }
            if (backend.active_xid.has_value())
            {
                ASSERT_EQ(txn_mgr_->rollbackTransaction(backend.proc_id, *backend.active_xid, &ctx), Status::OK)
                    << ctx.message;
                expected_states[*backend.active_xid] = TransactionState::ABORTED;
                backend.active_xid.reset();
            }
            ProcArrayManager::unregisterBackend(backend.proc_id, nullptr);
        }
        assertTransactionInvariants(expected_states);
    }

    void runVisibilityCorpus(uint64_t seed)
    {
        const ID table_id = createTestTable("mgw015_visibility_" + std::to_string(seed));
        auto writer = connectContext(false, IsolationLevel::SNAPSHOT);
        auto snapshot_reader = connectContext(true, IsolationLevel::SNAPSHOT);
        auto current_reader = connectContext(true, IsolationLevel::SNAPSHOT);
        ASSERT_NE(writer, nullptr);
        ASSERT_NE(snapshot_reader, nullptr);
        ASSERT_NE(current_reader, nullptr);

        const std::map<int32_t, int32_t> baseline_values{{1, 10}, {2, 20}};
        std::map<int32_t, RowState> current_model = seedBaseRows(writer.get(), table_id, baseline_values);

        {
            ErrorContext ctx;
            ASSERT_EQ(snapshot_reader->startTransaction(true, IsolationLevel::SNAPSHOT, true, &ctx), Status::OK)
                << ctx.message;
            ASSERT_EQ(current_reader->startTransaction(true, IsolationLevel::SNAPSHOT, true, &ctx), Status::OK)
                << ctx.message;
        }

        const VisibleScanResult baseline_scan =
            decodeVisibleRows(*db_, *storage_, snapshot_reader.get(), table_id);
        ASSERT_EQ(baseline_scan.status, Status::OK) << baseline_scan.message;
        ASSERT_FALSE(baseline_scan.duplicate_row_ids);
        ASSERT_EQ(baseline_scan.rows, baseline_values);

        std::mt19937_64 rng(seed);
        std::vector<std::string> history;
        for (uint64_t round = 0; round < 24; ++round)
        {
            const WriterPlan plan = buildWriterPlan(current_model, rng, round + 1);
            SCOPED_TRACE(::testing::Message()
                         << "seed=" << seed
                         << " round=" << round
                         << " kind=" << static_cast<int>(plan.kind)
                         << " commit=" << plan.commit
                         << " row_id=" << plan.row_id
                         << " value=" << plan.value
                         << " current_tid=(" << getPageNumber(plan.current_tid.gpid)
                         << "," << plan.current_tid.slot << ")");

            auto snapshot_future = std::async(std::launch::async,
                                              [&]() {
                                                  return decodeVisibleRows(
                                                      *db_, *storage_, snapshot_reader.get(), table_id);
                                              });
            auto writer_future = std::async(std::launch::async,
                                            [&]() {
                                                return executeWriterPlan(writer.get(), table_id, plan);
                                            });

            const VisibleScanResult snapshot_result = snapshot_future.get();
            const WriterOutcome writer_outcome = writer_future.get();

            ASSERT_EQ(snapshot_result.status, Status::OK) << snapshot_result.message;
            ASSERT_FALSE(snapshot_result.duplicate_row_ids);
            ASSERT_EQ(snapshot_result.rows, baseline_values);
            ASSERT_EQ(writer_outcome.status, Status::OK) << writer_outcome.message;
            {
                std::ostringstream step;
                step << "round=" << round
                     << " kind=" << static_cast<int>(plan.kind)
                     << " commit=" << plan.commit
                     << " row_id=" << plan.row_id
                     << " value=" << plan.value
                     << " tid=(" << getPageNumber(plan.current_tid.gpid) << ","
                     << plan.current_tid.slot << ")"
                     << " new_tid=(" << getPageNumber(writer_outcome.new_tid.gpid) << ","
                     << writer_outcome.new_tid.slot << ")";
                history.push_back(step.str());
            }

            if (plan.commit)
            {
                switch (plan.kind)
                {
                    case WriterOperationKind::INSERT:
                        current_model[plan.row_id] = RowState{plan.value, writer_outcome.new_tid};
                        break;
                    case WriterOperationKind::UPDATE:
                        current_model[plan.row_id] = RowState{plan.value, writer_outcome.new_tid};
                        break;
                    case WriterOperationKind::DELETE_ROW:
                        current_model.erase(plan.row_id);
                        break;
                    case WriterOperationKind::NOOP:
                        break;
                }
            }

            if ((round % 4u) == 3u)
            {
                ErrorContext ctx;
                ASSERT_EQ(sweep_mgr_->executeSweep(true, &ctx), Status::OK) << ctx.message;
                const VisibleScanResult after_sweep_snapshot =
                    decodeVisibleRows(*db_, *storage_, snapshot_reader.get(), table_id);
                ASSERT_EQ(after_sweep_snapshot.status, Status::OK) << after_sweep_snapshot.message;
                ASSERT_FALSE(after_sweep_snapshot.duplicate_row_ids);
                ASSERT_EQ(after_sweep_snapshot.rows, baseline_values);
            }

            {
                ScopedCurrentConnection current_scope(current_reader.get());
                ErrorContext ctx;
                ASSERT_EQ(current_reader->commit(&ctx), Status::OK) << ctx.message;
            }
            const VisibleScanResult current_scan =
                decodeVisibleRows(*db_, *storage_, current_reader.get(), table_id);
            ASSERT_EQ(current_scan.status, Status::OK) << current_scan.message;
            ASSERT_FALSE(current_scan.duplicate_row_ids);
            std::ostringstream history_out;
            for (const auto& step : history)
            {
                history_out << "\n  " << step;
            }
            ASSERT_EQ(current_scan.rows, valuesOnly(current_model))
                << "\nhistory:" << history_out.str()
                << "\nreader_snapshot=" << describeSnapshot(current_reader.get())
                << "\nphysical_rows="
                << describePhysicalRows(*db_, *storage_, current_reader.get(), table_id);
        }

        {
            ScopedCurrentConnection snapshot_scope(snapshot_reader.get());
            ErrorContext ctx;
            ASSERT_EQ(snapshot_reader->commit(&ctx), Status::OK) << ctx.message;
            ASSERT_EQ(sweep_mgr_->executeSweep(true, &ctx), Status::OK) << ctx.message;
        }

        const VisibleScanResult final_current =
            decodeVisibleRows(*db_, *storage_, current_reader.get(), table_id);
        ASSERT_EQ(final_current.status, Status::OK) << final_current.message;
        ASSERT_FALSE(final_current.duplicate_row_ids);
        ASSERT_EQ(final_current.rows, valuesOnly(current_model));
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    TransactionManager* txn_mgr_ = nullptr;
    StorageEngine* storage_ = nullptr;
    SweepManager* sweep_mgr_ = nullptr;
    ID system_user_id_{};
};

TEST_F(MgaInvariantFuzzTest, PublicationAndSnapshotInvariantsHoldAcrossSeededStateCorpus)
{
    for (uint64_t seed : {0x514d4701ULL, 0x514d4702ULL, 0x514d4703ULL})
    {
        SCOPED_TRACE(::testing::Message() << "seed=" << seed);
        runPublicationCorpus(seed);
    }
}

TEST_F(MgaInvariantFuzzTest, RolledBackDeletePreservesVisibilityForFreshSnapshot)
{
    const ID table_id = createTestTable("mgw015_delete_rollback");
    auto writer = connectContext(false, IsolationLevel::SNAPSHOT);
    ASSERT_NE(writer, nullptr);

    std::map<int32_t, RowState> current_model =
        seedBaseRows(writer.get(), table_id, {{1, 10}, {2, 20}});

    auto reader = connectContext(true, IsolationLevel::SNAPSHOT);
    ASSERT_NE(reader, nullptr);

    WriterPlan insert_plan{};
    insert_plan.kind = WriterOperationKind::INSERT;
    insert_plan.row_id = 3;
    insert_plan.value = 13;
    insert_plan.commit = true;

    WriterOutcome insert_outcome = executeWriterPlan(writer.get(), table_id, insert_plan);
    ASSERT_EQ(insert_outcome.status, Status::OK) << insert_outcome.message;
    current_model[3] = RowState{13, insert_outcome.new_tid};

    {
        ScopedCurrentConnection reader_scope(reader.get());
        ErrorContext ctx;
        ASSERT_EQ(reader->commit(&ctx), Status::OK) << ctx.message;
    }
    VisibleScanResult after_insert = decodeVisibleRows(*db_, *storage_, reader.get(), table_id);
    ASSERT_EQ(after_insert.status, Status::OK) << after_insert.message;
    ASSERT_FALSE(after_insert.duplicate_row_ids);
    ASSERT_EQ(after_insert.rows, valuesOnly(current_model));

    WriterPlan delete_plan{};
    delete_plan.kind = WriterOperationKind::DELETE_ROW;
    delete_plan.row_id = 3;
    delete_plan.current_tid = current_model.at(3).tid;
    delete_plan.commit = false;

    WriterOutcome delete_outcome = executeWriterPlan(writer.get(), table_id, delete_plan);
    ASSERT_EQ(delete_outcome.status, Status::OK) << delete_outcome.message;

    {
        ScopedCurrentConnection reader_scope(reader.get());
        ErrorContext ctx;
        ASSERT_EQ(reader->commit(&ctx), Status::OK) << ctx.message;
    }
    VisibleScanResult after_rollback = decodeVisibleRows(*db_, *storage_, reader.get(), table_id);
    ASSERT_EQ(after_rollback.status, Status::OK) << after_rollback.message;
    ASSERT_FALSE(after_rollback.duplicate_row_ids);
    ASSERT_EQ(after_rollback.rows, valuesOnly(current_model));
}

TEST_F(MgaInvariantFuzzTest, SeededConcurrentVisibilityAndReclaimInvariantsHold)
{
    for (uint64_t seed : {0x514d4711ULL, 0x514d4712ULL, 0x514d4713ULL})
    {
        SCOPED_TRACE(::testing::Message() << "seed=" << seed);
        runVisibilityCorpus(seed);
    }
}
