#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <thread>
#include <chrono>

using namespace scratchbird::core;
namespace fs = std::filesystem;

class TransactionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_transactions.db";
        if (fs::exists(test_db_)) {
            fs::remove(test_db_);
        }
    }
    
    void TearDown() override {
        if (db_) {
            db_->close();
            db_.reset();
        }
        if (fs::exists(test_db_)) {
            fs::remove(test_db_);
        }
    }
    
    std::string test_db_;
    std::unique_ptr<Database> db_;
};

TEST_F(TransactionManagerTest, BasicTransaction) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    TransactionManager* tm = db_->transaction_manager();
    ASSERT_NE(nullptr, tm);
    
    // Begin transaction
    uint64_t xid;
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid, &ctx));
    EXPECT_GT(xid, 0u);
    EXPECT_EQ(xid, tm->get_active_xid());
    
    // Check transaction state
    TransactionState state;
    ASSERT_EQ(Status::Ok, tm->get_transaction_state(xid, state, &ctx));
    EXPECT_EQ(TransactionState::ACTIVE, state);
    
    // Commit transaction
    ASSERT_EQ(Status::Ok, tm->commit_transaction(xid, &ctx));
    EXPECT_EQ(0u, tm->get_active_xid());
    
    // Check state after commit
    ASSERT_EQ(Status::Ok, tm->get_transaction_state(xid, state, &ctx));
    EXPECT_EQ(TransactionState::COMMITTED, state);
}

TEST_F(TransactionManagerTest, TransactionRollback) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    TransactionManager* tm = db_->transaction_manager();
    
    // Begin transaction
    uint64_t xid;
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid, &ctx));
    
    // Rollback transaction
    ASSERT_EQ(Status::Ok, tm->rollback_transaction(xid, &ctx));
    EXPECT_EQ(0u, tm->get_active_xid());
    
    // Check state after rollback
    TransactionState state;
    ASSERT_EQ(Status::Ok, tm->get_transaction_state(xid, state, &ctx));
    EXPECT_EQ(TransactionState::ABORTED, state);
}

TEST_F(TransactionManagerTest, SingleConnectionLimit) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    TransactionManager* tm = db_->transaction_manager();
    
    // Begin first transaction
    uint64_t xid1;
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid1, &ctx));
    
    // Try to begin second transaction - should fail
    uint64_t xid2;
    EXPECT_EQ(Status::InvalidArgument, tm->begin_transaction(xid2, &ctx));
    
    // Commit first transaction
    ASSERT_EQ(Status::Ok, tm->commit_transaction(xid1, &ctx));
    
    // Now second transaction should succeed
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid2, &ctx));
    EXPECT_GT(xid2, xid1);
    
    // Cleanup
    tm->rollback_transaction(xid2, &ctx);
}

TEST_F(TransactionManagerTest, XIDGeneration) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    TransactionManager* tm = db_->transaction_manager();
    
    // Begin multiple transactions and check XIDs are increasing
    std::vector<uint64_t> xids;
    
    for (int i = 0; i < 10; i++) {
        uint64_t xid;
        ASSERT_EQ(Status::Ok, tm->begin_transaction(xid, &ctx));
        xids.push_back(xid);
        ASSERT_EQ(Status::Ok, tm->commit_transaction(xid, &ctx));
    }
    
    // Verify XIDs are strictly increasing
    for (size_t i = 1; i < xids.size(); i++) {
        EXPECT_GT(xids[i], xids[i-1]);
    }
    
    // Verify no XIDs are reserved values
    for (uint64_t xid : xids) {
        EXPECT_GT(xid, 2u);  // Greater than FROZEN_XID
    }
}

TEST_F(TransactionManagerTest, TransactionVisibility) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    TransactionManager* tm = db_->transaction_manager();
    
    // Create some transactions
    uint64_t xid1, xid2, xid3;
    
    // Committed transaction
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid1, &ctx));
    ASSERT_EQ(Status::Ok, tm->commit_transaction(xid1, &ctx));
    
    // Aborted transaction
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid2, &ctx));
    ASSERT_EQ(Status::Ok, tm->rollback_transaction(xid2, &ctx));
    
    // Active transaction
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid3, &ctx));
    
    // Test visibility from xid3's perspective
    EXPECT_TRUE(tm->is_transaction_visible(xid1, xid3));  // Sees committed
    EXPECT_FALSE(tm->is_transaction_visible(xid2, xid3)); // Doesn't see aborted
    EXPECT_TRUE(tm->is_transaction_visible(xid3, xid3));  // Sees itself
    
    // Future transaction should not be visible
    EXPECT_FALSE(tm->is_transaction_visible(xid3 + 100, xid3));
    
    // Cleanup
    tm->commit_transaction(xid3, &ctx);
}

TEST_F(TransactionManagerTest, TransactionPersistence) {
    ErrorContext ctx;
    uint64_t xid1, xid2;
    
    // Create database and perform transactions
    {
        ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
        db_ = std::make_unique<Database>();
        ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
        
        TransactionManager* tm = db_->transaction_manager();
        
        // Create committed transaction
        ASSERT_EQ(Status::Ok, tm->begin_transaction(xid1, &ctx));
        ASSERT_EQ(Status::Ok, tm->commit_transaction(xid1, &ctx));
        
        // Create aborted transaction
        ASSERT_EQ(Status::Ok, tm->begin_transaction(xid2, &ctx));
        ASSERT_EQ(Status::Ok, tm->rollback_transaction(xid2, &ctx));
        
        // Close database
        db_->close();
        db_.reset();
    }
    
    // Reopen database and verify transaction states
    {
        db_ = std::make_unique<Database>();
        ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
        
        TransactionManager* tm = db_->transaction_manager();
        
        // Check persisted states
        TransactionState state;
        ASSERT_EQ(Status::Ok, tm->get_transaction_state(xid1, state, &ctx));
        EXPECT_EQ(TransactionState::COMMITTED, state);
        
        ASSERT_EQ(Status::Ok, tm->get_transaction_state(xid2, state, &ctx));
        EXPECT_EQ(TransactionState::ABORTED, state);
        
        // New XIDs should be greater than previous
        uint64_t xid3;
        ASSERT_EQ(Status::Ok, tm->begin_transaction(xid3, &ctx));
        EXPECT_GT(xid3, xid2);
        tm->commit_transaction(xid3, &ctx);
    }
}

TEST_F(TransactionManagerTest, StorageEngineIntegration) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    TransactionManager* tm = db_->transaction_manager();
    StorageEngine* se = db_->storage_engine();
    ASSERT_NE(nullptr, se);
    
    // Begin transaction
    uint64_t xid;
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid, &ctx));
    
    // Insert tuple (storage engine should use active XID)
    uint8_t tuple_data[100] = {0};
    uint32_t page_id;
    uint16_t item_id;
    ASSERT_EQ(Status::Ok, se->insert_tuple(1, tuple_data, 
                                          sizeof(tuple_data) + sizeof(TupleHeader),
                                          &page_id, &item_id, &ctx));
    
    // Verify tuple is visible to current transaction
    Tuple retrieved;
    ASSERT_EQ(Status::Ok, se->get_tuple(page_id, item_id, &retrieved, &ctx));
    
    // Commit transaction
    ASSERT_EQ(Status::Ok, tm->commit_transaction(xid, &ctx));
    
    // Begin new transaction
    uint64_t xid2;
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid2, &ctx));
    
    // Tuple should still be visible
    ASSERT_EQ(Status::Ok, se->get_tuple(page_id, item_id, &retrieved, &ctx));
    
    // Delete tuple
    ASSERT_EQ(Status::Ok, se->delete_tuple(page_id, item_id, &ctx));
    
    // Rollback
    ASSERT_EQ(Status::Ok, tm->rollback_transaction(xid2, &ctx));
    
    // Begin another transaction
    uint64_t xid3;
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid3, &ctx));
    
    // Tuple should still be visible (delete was rolled back)
    ASSERT_EQ(Status::Ok, se->get_tuple(page_id, item_id, &retrieved, &ctx));
    
    tm->commit_transaction(xid3, &ctx);
}

TEST_F(TransactionManagerTest, Snapshot) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    TransactionManager* tm = db_->transaction_manager();
    
    // Get snapshot with no active transaction
    TransactionManager::Snapshot snap1;
    ASSERT_EQ(Status::Ok, tm->get_snapshot(snap1, &ctx));
    EXPECT_EQ(0u, snap1.active_xids.size());
    
    // Begin transaction and get snapshot
    uint64_t xid;
    ASSERT_EQ(Status::Ok, tm->begin_transaction(xid, &ctx));
    
    TransactionManager::Snapshot snap2;
    ASSERT_EQ(Status::Ok, tm->get_snapshot(snap2, &ctx));
    EXPECT_EQ(1u, snap2.active_xids.size());
    EXPECT_EQ(xid, snap2.active_xids[0]);
    EXPECT_EQ(xid, snap2.xmin);
    EXPECT_GT(snap2.xmax, xid);
    
    tm->commit_transaction(xid, &ctx);
}

TEST_F(TransactionManagerTest, TIPPageValidation) {
    ErrorContext ctx;
    
    // Create and open database
    ASSERT_EQ(Status::Ok, Database::create(test_db_, 8192, &ctx));
    db_ = std::make_unique<Database>();
    ASSERT_EQ(Status::Ok, db_->open(test_db_, &ctx));
    
    TransactionManager* tm = db_->transaction_manager();
    
    // Create many transactions to test TIP page
    const int num_transactions = 50;
    std::vector<uint64_t> xids;
    
    for (int i = 0; i < num_transactions; i++) {
        uint64_t xid;
        ASSERT_EQ(Status::Ok, tm->begin_transaction(xid, &ctx));
        xids.push_back(xid);
        
        // Alternate between commit and rollback
        if (i % 2 == 0) {
            ASSERT_EQ(Status::Ok, tm->commit_transaction(xid, &ctx));
        } else {
            ASSERT_EQ(Status::Ok, tm->rollback_transaction(xid, &ctx));
        }
    }
    
    // Verify all transaction states
    for (size_t i = 0; i < xids.size(); i++) {
        TransactionState state;
        ASSERT_EQ(Status::Ok, tm->get_transaction_state(xids[i], state, &ctx));
        
        if (i % 2 == 0) {
            EXPECT_EQ(TransactionState::COMMITTED, state);
        } else {
            EXPECT_EQ(TransactionState::ABORTED, state);
        }
    }
}