/**
 * GiST Index DML Integration Tests
 *
 * Tests DML operations (INSERT/UPDATE/DELETE) with GiST index maintenance.
 *
 * TASK-DML-3: GiST Index DML Integration
 *
 * This test validates:
 * - INSERT operations maintain GiST index
 * - UPDATE operations update GiST index (remove old + insert new)
 * - DELETE operations mark entries with xmax (MGA-compliant)
 * - Index remains consistent after DML operations
 * - Visibility checks work correctly
 *
 * Test Scenarios:
 * 1. INSERT → GiST index updated
 * 2. UPDATE → Old predicate removed, new predicate inserted
 * 3. DELETE → Entry marked with xmax
 * 4. Multiple operations → Index consistency maintained
 *
 * Execution:
 *   From build/: ctest -R GiSTDML -V
 */

#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include <cstring>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/proc_array.h"

using namespace scratchbird::core;

/**
 * Simple Box Operator Class for Testing
 * (Same as in test_gist_mvcc.cpp)
 */
class BoxOperatorClass : public GiSTOperatorClass
{
public:
    struct Box
    {
        double min_x, min_y, max_x, max_y;

        Box() : min_x(0), min_y(0), max_x(0), max_y(0) {}
        Box(double x1, double y1, double x2, double y2)
            : min_x(x1), min_y(y1), max_x(x2), max_y(y2) {}

        bool overlaps(const Box& other) const
        {
            return !(max_x < other.min_x || min_x > other.max_x ||
                    max_y < other.min_y || min_y > other.max_y);
        }

        bool contains(const Box& other) const
        {
            return (min_x <= other.min_x && max_x >= other.max_x &&
                    min_y <= other.min_y && max_y >= other.max_y);
        }

        Box unionWith(const Box& other) const
        {
            return Box(
                std::min(min_x, other.min_x),
                std::min(min_y, other.min_y),
                std::max(max_x, other.max_x),
                std::max(max_y, other.max_y)
            );
        }

        double area() const
        {
            return (max_x - min_x) * (max_y - min_y);
        }
    };

    static constexpr uint32_t OPCLASS_ID = 0;  // Use default opclass ID

    uint32_t getOpClassId() const override { return OPCLASS_ID; }
    std::string getOpClassName() const override { return "box_ops"; }

    bool consistent(const GiSTPredicate& predicate,
                   const std::vector<uint8_t>& query,
                   GiSTStrategy strategy) const override
    {
        Box pred_box = deserialize(predicate.data);
        Box query_box = deserialize(query);

        switch (strategy)
        {
            case GiSTStrategy::OVERLAPS:
                return pred_box.overlaps(query_box);
            case GiSTStrategy::CONTAINS:
                return pred_box.contains(query_box);
            case GiSTStrategy::CONTAINED_BY:
                return query_box.contains(pred_box);
            default:
                return false;
        }
    }

    GiSTPredicate unionPredicates(const std::vector<GiSTPredicate>& entries) const override
    {
        if (entries.empty())
        {
            return GiSTPredicate(serialize(Box()), OPCLASS_ID);
        }

        Box result = deserialize(entries[0].data);
        for (size_t i = 1; i < entries.size(); i++)
        {
            Box box = deserialize(entries[i].data);
            result = result.unionWith(box);
        }

        return GiSTPredicate(serialize(result), OPCLASS_ID);
    }

    double penalty(const GiSTPredicate& base,
                  const GiSTPredicate& add) const override
    {
        Box base_box = deserialize(base.data);
        Box add_box = deserialize(add.data);
        Box union_box = base_box.unionWith(add_box);

        return union_box.area() - base_box.area();
    }

    void picksplit(const std::vector<GiSTPredicate>& entries,
                  std::vector<size_t>& left_indices,
                  std::vector<size_t>& right_indices) const override
    {
        // Simple split: left half goes left, right half goes right
        for (size_t i = 0; i < entries.size() / 2; i++)
        {
            left_indices.push_back(i);
        }
        for (size_t i = entries.size() / 2; i < entries.size(); i++)
        {
            right_indices.push_back(i);
        }
    }

    bool same(const GiSTPredicate& a, const GiSTPredicate& b) const override
    {
        if (a.data.size() != b.data.size()) return false;
        return std::memcmp(a.data.data(), b.data.data(), a.data.size()) == 0;
    }

    // Helper: Serialize box to bytes
    static std::vector<uint8_t> serialize(const Box& box)
    {
        std::vector<uint8_t> data(sizeof(Box));
        std::memcpy(data.data(), &box, sizeof(Box));
        return data;
    }

    // Helper: Deserialize box from bytes
    static Box deserialize(const std::vector<uint8_t>& data)
    {
        Box box;
        if (data.size() >= sizeof(Box))
        {
            std::memcpy(&box, data.data(), sizeof(Box));
        }
        return box;
    }
};

class GiSTDMLTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = "/tmp/test_gist_dml.db";
        std::remove(test_db_path_);

        // Register box operator class
        auto opclass = std::make_shared<BoxOperatorClass>();
        GiSTOperatorClassRegistry::instance().registerOperatorClass(opclass);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 8192, &ctx);
        ASSERT_EQ(Status::OK, status) << "Failed to create database: " << ctx.message;

        db_ = Database::open(test_db_path_, &ctx);
        ASSERT_NE(nullptr, db_) << "Failed to open database: " << ctx.message;

        catalog_ = db_->catalog_manager();
        storage_ = db_->storage_engine();
        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(nullptr, catalog_);
        ASSERT_NE(nullptr, storage_);
        ASSERT_NE(nullptr, txn_mgr_);

        // Register backend for MGA
        proc_id_ = ProcArrayManager::instance().registerBackend();
    }

    void TearDown() override
    {
        // Unregister backend
        if (proc_id_ != 0)
        {
            ProcArrayManager::instance().unregisterBackend(proc_id_);
        }

        if (db_)
        {
            db_->close();
            db_.reset();
        }
        std::remove(test_db_path_);
    }

    // Helper: Begin transaction with MGA API
    uint64_t beginTxn(ErrorContext* ctx)
    {
        uint64_t xid = 0;
        Status status = txn_mgr_->beginTransaction(proc_id_, xid, ctx);
        EXPECT_EQ(status, Status::OK);
        return xid;
    }

    // Helper: Commit transaction with MGA API
    Status commitTxn(uint64_t xid, ErrorContext* ctx)
    {
        return txn_mgr_->commitTransaction(proc_id_, xid, ctx);
    }

    // Helper: Create table with box column
    ID createTestTable()
    {
        ErrorContext ctx;

        // Create schema
        CatalogManager::SchemaInfo schema;
        schema.schema_name = "test_schema";
        schema.schema_id = UUIDv7::generate();
        schema.owner_id = UUIDv7::generate();
        Status status = catalog_->createSchema(schema, &ctx);
        EXPECT_EQ(Status::OK, status);

        // Create table
        CatalogManager::TableInfo table;
        table.table_name = "test_table";
        table.table_id = UUIDv7::generate();
        table.schema_id = schema.schema_id;
        table.owner_id = schema.owner_id;
        status = catalog_->createTable(table, &ctx);
        EXPECT_EQ(Status::OK, status);

        // Add box column (stored as BYTEA for simplicity)
        CatalogManager::ColumnInfo col;
        col.column_id = UUIDv7::generate();
        col.table_id = table.table_id;
        col.column_name = "box_col";
        col.ordinal_position = 1;
        col.data_type = static_cast<uint16_t>(DataType::BYTEA);
        col.type_precision = sizeof(BoxOperatorClass::Box);
        col.nullable = false;
        status = catalog_->createColumn(col, &ctx);
        EXPECT_EQ(Status::OK, status);

        return table.table_id;
    }

    // Helper: Create GiST index on table
    ID createGiSTIndex(const ID& table_id, const ID& column_id)
    {
        ErrorContext ctx;

        CatalogManager::IndexInfo index;
        index.index_id = UUIDv7::generate();
        index.index_name = "test_gist_idx";
        index.table_id = table_id;
        index.schema_id = UUIDv7::generate();  // Same schema as table
        index.column_ids = {column_id};
        index.is_unique = false;

        // Create index using factory
        void* index_ptr = nullptr;
        Status status = IndexFactory::createIndex(
            CatalogManager::IndexType::GIST,
            db_.get(),
            index,
            &index_ptr,
            &ctx);

        EXPECT_EQ(Status::OK, status) << "Failed to create GiST index: " << ctx.message;
        EXPECT_NE(nullptr, index_ptr);

        // Register index in catalog
        status = catalog_->createIndex(index, &ctx);
        EXPECT_EQ(Status::OK, status);

        // Add to index object cache
        catalog_->addIndexToCache(index.index_id, index_ptr, CatalogManager::IndexType::GIST);

        return index.index_id;
    }

    // Helper: Insert tuple with box value
    TID insertTuple(const ID& table_id, const BoxOperatorClass::Box& box)
    {
        ErrorContext ctx;

        // Serialize box as tuple data
        std::vector<uint8_t> tuple_data = BoxOperatorClass::serialize(box);

        uint32_t page_id = 0;
        uint16_t item_id = 0;
        uint64_t xid = txn_mgr_->getCurrentXid();

        Status status = storage_->insertTuple(
            table_id,
            tuple_data.data(),
            tuple_data.size(),
            &page_id,
            &item_id,
            &ctx);

        EXPECT_EQ(Status::OK, status) << "Failed to insert tuple: " << ctx.message;

        return TID(0, page_id, item_id);  // tablespace_id=0, page_number, slot
    }

    const char* test_db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_;
    StorageEngine* storage_;
    TransactionManager* txn_mgr_;
    uint32_t proc_id_ = 0;
};

/**
 * Test 1: INSERT operation updates GiST index
 */
TEST_F(GiSTDMLTest, InsertUpdateIndex)
{
    // Create table and index
    ID table_id = createTestTable();

    // Get column ID
    ErrorContext ctx;
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = catalog_->getColumns(table_id, columns, &ctx);
    ASSERT_EQ(Status::OK, status);
    ASSERT_EQ(1, columns.size());
    ID column_id = columns[0].column_id;

    ID index_id = createGiSTIndex(table_id, column_id);

    // Insert tuple
    BoxOperatorClass::Box box(0.0, 0.0, 10.0, 10.0);
    TID tid = insertTuple(table_id, box);

    // Verify index was updated
    CatalogManager::IndexType index_type;
    void* index_ptr = catalog_->getIndexPtr(index_id, &index_type);
    ASSERT_NE(nullptr, index_ptr);
    ASSERT_EQ(CatalogManager::IndexType::GIST, index_type);

    auto* gist = static_cast<GiSTIndex*>(index_ptr);

    // Search for the box
    GiSTPredicate query_pred(BoxOperatorClass::serialize(box), 0);
    std::vector<TID> results;
    uint64_t xid = txn_mgr_->getCurrentXid();

    status = gist->search(
        BoxOperatorClass::serialize(box),
        GiSTStrategy::OVERLAPS,
        xid,
        &results,
        &ctx);

    EXPECT_EQ(Status::OK, status) << "Search failed: " << ctx.message;
    EXPECT_EQ(1, results.size()) << "Expected 1 result, got " << results.size();
    if (!results.empty())
    {
        EXPECT_EQ(tid, results[0]) << "TID mismatch";
    }
}

/**
 * Test 2: UPDATE operation updates GiST index
 */
TEST_F(GiSTDMLTest, UpdateUpdateIndex)
{
    // Create table and index
    ID table_id = createTestTable();

    ErrorContext ctx;
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = catalog_->getColumns(table_id, columns, &ctx);
    ASSERT_EQ(Status::OK, status);
    ID column_id = columns[0].column_id;

    ID index_id = createGiSTIndex(table_id, column_id);

    // Insert tuple
    BoxOperatorClass::Box old_box(0.0, 0.0, 10.0, 10.0);
    TID tid = insertTuple(table_id, old_box);

    // Update tuple (note: this test assumes updateTuple exists)
    BoxOperatorClass::Box new_box(20.0, 20.0, 30.0, 30.0);
    std::vector<uint8_t> new_tuple_data = BoxOperatorClass::serialize(new_box);

    uint64_t xid = txn_mgr_->getCurrentXid();
    status = storage_->updateTuple(
        table_id,
        tid,
        new_tuple_data.data(),
        new_tuple_data.size(),
        &ctx);

    // Verify old predicate is not visible
    CatalogManager::IndexType index_type;
    void* index_ptr = catalog_->getIndexPtr(index_id, &index_type);
    ASSERT_NE(nullptr, index_ptr);

    auto* gist = static_cast<GiSTIndex*>(index_ptr);

    std::vector<TID> results;
    status = gist->search(
        BoxOperatorClass::serialize(old_box),
        GiSTStrategy::OVERLAPS,
        xid,
        &results,
        &ctx);

    // After update, old box should not be found
    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(0, results.size()) << "Old box should not be visible after update";

    // Verify new predicate is visible
    results.clear();
    status = gist->search(
        BoxOperatorClass::serialize(new_box),
        GiSTStrategy::OVERLAPS,
        xid,
        &results,
        &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(1, results.size()) << "New box should be visible";
}

/**
 * Test 3: DELETE operation marks entry with xmax
 */
TEST_F(GiSTDMLTest, DeleteMarkEntry)
{
    // Create table and index
    ID table_id = createTestTable();

    ErrorContext ctx;
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = catalog_->getColumns(table_id, columns, &ctx);
    ASSERT_EQ(Status::OK, status);
    ID column_id = columns[0].column_id;

    ID index_id = createGiSTIndex(table_id, column_id);

    // Insert tuple
    BoxOperatorClass::Box box(0.0, 0.0, 10.0, 10.0);
    TID tid = insertTuple(table_id, box);

    // Delete tuple
    uint64_t xid = txn_mgr_->getCurrentXid();
    status = storage_->deleteTuple(table_id, tid, &ctx);

    // Verify entry is not visible (marked with xmax)
    CatalogManager::IndexType index_type;
    void* index_ptr = catalog_->getIndexPtr(index_id, &index_type);
    ASSERT_NE(nullptr, index_ptr);

    auto* gist = static_cast<GiSTIndex*>(index_ptr);

    std::vector<TID> results;
    status = gist->search(
        BoxOperatorClass::serialize(box),
        GiSTStrategy::OVERLAPS,
        xid,
        &results,
        &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(0, results.size()) << "Deleted entry should not be visible";
}

/**
 * Test 4: Multiple operations maintain index consistency
 */
TEST_F(GiSTDMLTest, MultipleOperationsConsistent)
{
    // Create table and index
    ID table_id = createTestTable();

    ErrorContext ctx;
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = catalog_->getColumns(table_id, columns, &ctx);
    ASSERT_EQ(Status::OK, status);
    ID column_id = columns[0].column_id;

    ID index_id = createGiSTIndex(table_id, column_id);

    // Insert multiple tuples
    BoxOperatorClass::Box box1(0.0, 0.0, 10.0, 10.0);
    BoxOperatorClass::Box box2(20.0, 20.0, 30.0, 30.0);
    BoxOperatorClass::Box box3(40.0, 40.0, 50.0, 50.0);

    TID tid1 = insertTuple(table_id, box1);
    TID tid2 = insertTuple(table_id, box2);
    TID tid3 = insertTuple(table_id, box3);

    // Verify all are visible
    CatalogManager::IndexType index_type;
    void* index_ptr = catalog_->getIndexPtr(index_id, &index_type);
    ASSERT_NE(nullptr, index_ptr);

    auto* gist = static_cast<GiSTIndex*>(index_ptr);
    uint64_t xid = txn_mgr_->getCurrentXid();

    // Search for overlapping boxes (large query box)
    BoxOperatorClass::Box query_box(0.0, 0.0, 50.0, 50.0);
    std::vector<TID> results;
    status = gist->search(
        BoxOperatorClass::serialize(query_box),
        GiSTStrategy::OVERLAPS,
        xid,
        &results,
        &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(3, results.size()) << "Expected 3 overlapping boxes";
}

/**
 * Test 5: Index remains consistent after transaction commit
 */
TEST_F(GiSTDMLTest, ConsistentAfterCommit)
{
    // Create table and index
    ID table_id = createTestTable();

    ErrorContext ctx;
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = catalog_->getColumns(table_id, columns, &ctx);
    ASSERT_EQ(Status::OK, status);
    ID column_id = columns[0].column_id;

    ID index_id = createGiSTIndex(table_id, column_id);

    // Begin transaction
    uint64_t xid = beginTxn(&ctx);

    // Insert tuple
    BoxOperatorClass::Box box(0.0, 0.0, 10.0, 10.0);
    TID tid = insertTuple(table_id, box);

    // Commit transaction
    commitTxn(xid, &ctx);

    // Start new transaction and verify
    uint64_t new_xid = beginTxn(&ctx);

    CatalogManager::IndexType index_type;
    void* index_ptr = catalog_->getIndexPtr(index_id, &index_type);
    ASSERT_NE(nullptr, index_ptr);

    auto* gist = static_cast<GiSTIndex*>(index_ptr);

    std::vector<TID> results;
    status = gist->search(
        BoxOperatorClass::serialize(box),
        GiSTStrategy::OVERLAPS,
        new_xid,
        &results,
        &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(1, results.size()) << "Committed entry should be visible";

    commitTxn(new_xid, &ctx);
}

