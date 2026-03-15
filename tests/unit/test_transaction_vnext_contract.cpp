#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <vector>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/clog.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/vnext_error_codes.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class TransactionVNextContractTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "test_tx_vnext_contract", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Status::OK, Database::create(test_db_file_->path(), 8192, &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.initializeProcArray(32, &ctx)) << ctx.message;

        tm_ = db_.transaction_manager();
        ASSERT_NE(nullptr, tm_);
    }

    void reopenDatabase()
    {
        ErrorContext ctx;
        db_.close();
        ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
        ASSERT_EQ(Status::OK, db_.initializeProcArray(32, &ctx)) << ctx.message;
        tm_ = db_.transaction_manager();
        ASSERT_NE(nullptr, tm_);
    }

    void removeTipEntry(uint64_t xid)
    {
        ErrorContext ctx;
        void *page_buffer = nullptr;
        ASSERT_EQ(Status::OK,
                  db_.buffer_pool()->pinPage(BOOTSTRAP_PAGE_TX_MAP_ROOT, &page_buffer, &ctx))
            << ctx.message;
        ASSERT_EQ(Status::OK, db_.buffer_pool()->lockPage(BOOTSTRAP_PAGE_TX_MAP_ROOT, &ctx))
            << ctx.message;

        auto *tip_header = reinterpret_cast<TIPPageHeader *>(page_buffer);
        auto *entries = reinterpret_cast<TIPEntry *>(
            static_cast<uint8_t *>(page_buffer) + sizeof(TIPPageHeader));

        bool found = false;
        for (uint32_t i = 0; i < tip_header->num_transactions; ++i)
        {
            if (entries[i].xid == xid)
            {
                std::memset(&entries[i], 0, sizeof(TIPEntry));
                preparePageForWrite(static_cast<uint8_t *>(page_buffer),
                                   tip_header->page_header.page_size,
                                   tip_header->page_header.page_id);
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found);

        ASSERT_EQ(Status::OK, db_.buffer_pool()->unlockPage(BOOTSTRAP_PAGE_TX_MAP_ROOT, &ctx))
            << ctx.message;
        ASSERT_EQ(Status::OK, db_.buffer_pool()->unpinPage(BOOTSTRAP_PAGE_TX_MAP_ROOT, true, &ctx))
            << ctx.message;
    }

    void patchTipCommitSequenceInFile(uint64_t xid, uint64_t commit_seqno)
    {
        constexpr size_t kPageSize = 8192;
        std::vector<uint8_t> buffer(kPageSize, 0);
        const int fd = ::open(test_db_file_->path().c_str(), O_RDWR);
        ASSERT_GE(fd, 0) << std::strerror(errno);

        const off_t offset = static_cast<off_t>(BOOTSTRAP_PAGE_TX_MAP_ROOT) *
                             static_cast<off_t>(kPageSize);
        const ssize_t bytes = ::pread(fd, buffer.data(), buffer.size(), offset);
        ASSERT_EQ(bytes, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);

        auto *tip_header = reinterpret_cast<TIPPageHeader *>(buffer.data());
        auto *entries = reinterpret_cast<TIPEntry *>(buffer.data() + sizeof(TIPPageHeader));

        bool found = false;
        for (uint32_t i = 0; i < tip_header->num_transactions; ++i)
        {
            if (entries[i].xid == xid)
            {
                entries[i].commit_time = commit_seqno;
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found);

        tip_header->page_header.checksum = calculatePageChecksum(buffer.data(), buffer.size());
        const ssize_t written = ::pwrite(fd, buffer.data(), buffer.size(), offset);
        ASSERT_EQ(written, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);
        ASSERT_EQ(::fsync(fd), 0) << std::strerror(errno);
        ::close(fd);
    }

    void patchHeaderLatestCommitSequenceInFile(uint64_t latest_commit_seqno)
    {
        constexpr size_t kPageSize = 8192;
        std::vector<uint8_t> buffer(kPageSize, 0);
        const int fd = ::open(test_db_file_->path().c_str(), O_RDWR);
        ASSERT_GE(fd, 0) << std::strerror(errno);

        const ssize_t bytes = ::pread(fd, buffer.data(), buffer.size(), 0);
        ASSERT_EQ(bytes, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);

        auto *header = reinterpret_cast<DatabaseHeader *>(buffer.data());
        header->latest_commit_seqno = latest_commit_seqno;
        header->page_header.checksum = calculatePageChecksum(buffer.data(), buffer.size());

        const ssize_t written = ::pwrite(fd, buffer.data(), buffer.size(), 0);
        ASSERT_EQ(written, static_cast<ssize_t>(buffer.size())) << std::strerror(errno);
        ASSERT_EQ(::fsync(fd), 0) << std::strerror(errno);
        ::close(fd);
    }

    void TearDown() override
    {
        db_.close();
        test_db_file_.reset();
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_file_;
    Database db_;
    TransactionManager *tm_ = nullptr;
};

TEST_F(TransactionVNextContractTest, RejectsInvalidCommitTransitionAfterRollback)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, xid, &ctx)) << ctx.message;

    EXPECT_EQ(Status::INVALID_ARGUMENT, tm_->commitTransaction(proc_id, xid, &ctx));
    EXPECT_EQ(std::string("TXN_0201"), ctx.vnext_code);
    EXPECT_TRUE(isKnownVNextErrorCode(ctx.vnext_code));
    EXPECT_TRUE(statusMatchesVNextErrorCode(ctx.code, ctx.vnext_code));

    TransactionState state = TransactionState::ACTIVE;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::ABORTED, state);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionVNextContractTest, StartupNormalizesResidualActiveToAborted)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid_active = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid_active, &ctx)) << ctx.message;

    // Simulate process drop with in-progress transaction left in TIP.
    ProcArrayManager::unregisterBackend(proc_id, &ctx);
    db_.close();

    ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, db_.initializeProcArray(32, &ctx)) << ctx.message;
    tm_ = db_.transaction_manager();
    ASSERT_NE(nullptr, tm_);

    TransactionState state = TransactionState::ACTIVE;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid_active, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::ABORTED, state);

    TransactionStateResolution resolution{};
    ASSERT_EQ(Status::OK, tm_->getTransactionStateDetailed(xid_active, resolution, &ctx))
        << ctx.message;
    EXPECT_EQ(TransactionState::ABORTED, resolution.state);
    EXPECT_EQ(TransactionStateDetail::STARTUP_REPAIRED_ABORTED, resolution.detail);

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;
    uint64_t reader_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, reader_xid, &ctx)) << ctx.message;
    const auto decision = tm_->evaluateTransactionVisibility(
        xid_active, reader_xid, VisibilityMode::READ_CURRENT_TRANSACTION, nullptr);
    EXPECT_FALSE(decision.visible);
    EXPECT_EQ(TransactionState::ABORTED, decision.state);
    EXPECT_EQ(TransactionStateDetail::STARTUP_REPAIRED_ABORTED, decision.detail);
    EXPECT_EQ(VisibilityReason::ABORTED_INVISIBLE, decision.reason);
    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, reader_xid, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionVNextContractTest, MissingInRangeTipFailsClosedEvenIfClogStillCommitted)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
    reopenDatabase();

    ClogStatus clog_state = ClogStatus::IN_PROGRESS;
    ASSERT_EQ(Status::OK, db_.clog()->getStatus(xid, &clog_state, &ctx)) << ctx.message;
    EXPECT_EQ(ClogStatus::COMMITTED, clog_state);

    removeTipEntry(xid);
    reopenDatabase();

    TransactionState state = TransactionState::ACTIVE;
    ErrorContext lookup_ctx;
    EXPECT_EQ(Status::PAGE_CORRUPT, tm_->getTransactionState(xid, state, &lookup_ctx));
    EXPECT_EQ(std::string("TXN_0215"), lookup_ctx.vnext_code);
    EXPECT_TRUE(isKnownVNextErrorCode(lookup_ctx.vnext_code));
    EXPECT_TRUE(statusMatchesVNextErrorCode(lookup_ctx.code, lookup_ctx.vnext_code));

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;
    uint64_t reader_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, reader_xid, &ctx)) << ctx.message;

    const auto decision = tm_->evaluateTransactionVisibility(
        xid, reader_xid, VisibilityMode::READ_CURRENT_TRANSACTION, nullptr);
    EXPECT_FALSE(decision.visible);
    EXPECT_EQ(VisibilityReason::STATE_LOOKUP_FAILED, decision.reason);
    EXPECT_NE(VisibilityReason::STATE_LOOKUP_ASSUMED_COMMITTED, decision.reason);

    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, reader_xid, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionVNextContractTest, PrehistoricalCommittedXidDoesNotRequireTipEntry)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
    reopenDatabase();

    ASSERT_EQ(Status::OK, tm_->setOldestXid(xid + 1, &ctx)) << ctx.message;
    removeTipEntry(xid);

    TransactionState state = TransactionState::ACTIVE;
    ASSERT_EQ(Status::OK, tm_->getTransactionState(xid, state, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::COMMITTED, state);

    TransactionStateResolution resolution{};
    ASSERT_EQ(Status::OK, tm_->getTransactionStateDetailed(xid, resolution, &ctx)) << ctx.message;
    EXPECT_EQ(TransactionState::COMMITTED, resolution.state);
    EXPECT_EQ(TransactionStateDetail::PREHISTORICAL_COMMITTED, resolution.detail);

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;
    uint64_t reader_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, reader_xid, &ctx)) << ctx.message;

    const auto decision = tm_->evaluateTransactionVisibility(
        xid, reader_xid, VisibilityMode::READ_CURRENT_TRANSACTION, nullptr);
    EXPECT_TRUE(decision.visible);
    EXPECT_EQ(TransactionState::COMMITTED, decision.state);
    EXPECT_EQ(TransactionStateDetail::PREHISTORICAL_COMMITTED, decision.detail);
    EXPECT_EQ(VisibilityReason::COMMITTED_VISIBLE, decision.reason);

    ASSERT_EQ(Status::OK, tm_->rollbackTransaction(proc_id, reader_xid, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionVNextContractTest, SnapshotVisibilityUsesActiveSetDeterministically)
{
    ErrorContext ctx;
    uint32_t writer1 = 0;
    uint32_t writer2 = 0;
    uint32_t reader = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&writer1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&writer2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&reader, &ctx)) << ctx.message;

    uint64_t xid_committed = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(writer1, xid_committed, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(writer1, xid_committed, &ctx)) << ctx.message;

    uint64_t xid_active_then_commit = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(writer2, xid_active_then_commit, &ctx))
        << ctx.message;

    uint64_t xid_reader = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(reader, xid_reader, &ctx)) << ctx.message;

    TransactionSnapshot snapshot_before_commit;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot_before_commit, &ctx)) << ctx.message;

    EXPECT_TRUE(
        tm_->isCreateVisibleInSnapshot(xid_committed, xid_reader, snapshot_before_commit));
    EXPECT_FALSE(
        tm_->isCreateVisibleInSnapshot(xid_active_then_commit, xid_reader, snapshot_before_commit));
    EXPECT_TRUE(tm_->isRecordVersionVisibleInSnapshot(
        xid_committed, xid_active_then_commit, xid_reader, snapshot_before_commit));

    ASSERT_EQ(Status::OK, tm_->commitTransaction(writer2, xid_active_then_commit, &ctx))
        << ctx.message;

    // Old snapshot remains stable even after tx2 commits.
    EXPECT_FALSE(
        tm_->isCreateVisibleInSnapshot(xid_active_then_commit, xid_reader, snapshot_before_commit));
    EXPECT_TRUE(tm_->isRecordVersionVisibleInSnapshot(
        xid_committed, xid_active_then_commit, xid_reader, snapshot_before_commit));

    TransactionSnapshot snapshot_after_commit;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot_after_commit, &ctx)) << ctx.message;
    EXPECT_TRUE(
        tm_->isCreateVisibleInSnapshot(xid_active_then_commit, xid_reader, snapshot_after_commit));
    EXPECT_FALSE(tm_->isRecordVersionVisibleInSnapshot(
        xid_committed, xid_active_then_commit, xid_reader, snapshot_after_commit));

    ASSERT_EQ(Status::OK, tm_->commitTransaction(reader, xid_reader, &ctx)) << ctx.message;

    ProcArrayManager::unregisterBackend(writer1, &ctx);
    ProcArrayManager::unregisterBackend(writer2, &ctx);
    ProcArrayManager::unregisterBackend(reader, &ctx);
}

TEST_F(TransactionVNextContractTest, SnapshotCommitSequenceHighTracksDurableCommits)
{
    ErrorContext ctx;
    uint32_t writer = 0;
    uint32_t reader = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&writer, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&reader, &ctx)) << ctx.message;

    uint64_t xid1 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(writer, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(writer, xid1, &ctx)) << ctx.message;

    uint64_t commit_seq1 = 0;
    ASSERT_EQ(Status::OK, tm_->getCommittedTransactionSequence(xid1, commit_seq1, &ctx))
        << ctx.message;
    EXPECT_GT(commit_seq1, 0u);

    uint64_t reader_xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(reader, reader_xid, &ctx)) << ctx.message;

    TransactionSnapshot snapshot_after_first_commit;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot_after_first_commit, &ctx)) << ctx.message;
    EXPECT_EQ(snapshot_after_first_commit.snapshot_commit_seqno_high, commit_seq1);

    uint64_t xid2 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(writer, xid2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(writer, xid2, &ctx)) << ctx.message;

    uint64_t commit_seq2 = 0;
    ASSERT_EQ(Status::OK, tm_->getCommittedTransactionSequence(xid2, commit_seq2, &ctx))
        << ctx.message;
    EXPECT_GT(commit_seq2, commit_seq1);

    TransactionSnapshot snapshot_after_second_commit;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot_after_second_commit, &ctx)) << ctx.message;
    EXPECT_EQ(snapshot_after_second_commit.snapshot_commit_seqno_high, commit_seq2);
    EXPECT_TRUE(tm_->isCreateVisibleInSnapshot(xid2, reader_xid, snapshot_after_second_commit));

    TransactionSnapshot clamped_snapshot = snapshot_after_second_commit;
    clamped_snapshot.snapshot_commit_seqno_high = commit_seq1;
    const auto clamped_decision = tm_->evaluateTransactionVisibility(
        xid2, reader_xid, VisibilityMode::SNAPSHOT, &clamped_snapshot);
    EXPECT_FALSE(clamped_decision.visible);
    EXPECT_EQ(VisibilityReason::COMMITTED_AFTER_SNAPSHOT, clamped_decision.reason);

    ASSERT_EQ(Status::OK, tm_->commitTransaction(reader, reader_xid, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(writer, &ctx);
    ProcArrayManager::unregisterBackend(reader, &ctx);
}

TEST_F(TransactionVNextContractTest, CommitSequencePersistsAcrossReopen)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;
    uint64_t xid1 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid1, &ctx)) << ctx.message;

    uint64_t commit_seq1 = 0;
    ASSERT_EQ(Status::OK, tm_->getCommittedTransactionSequence(xid1, commit_seq1, &ctx))
        << ctx.message;
    EXPECT_GT(commit_seq1, 0u);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
    reopenDatabase();

    TransactionSnapshot reopened_snapshot;
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(reopened_snapshot, &ctx)) << ctx.message;
    EXPECT_EQ(reopened_snapshot.snapshot_commit_seqno_high, commit_seq1);

    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;
    uint64_t xid2 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid2, &ctx)) << ctx.message;

    uint64_t commit_seq2 = 0;
    ASSERT_EQ(Status::OK, tm_->getCommittedTransactionSequence(xid2, commit_seq2, &ctx))
        << ctx.message;
    EXPECT_GT(commit_seq2, commit_seq1);

    ProcArrayManager::unregisterBackend(proc_id, &ctx);
}

TEST_F(TransactionVNextContractTest, MissingCommittedSequenceFailsClosedOnRestart)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_id, &ctx);

    db_.close();
    patchTipCommitSequenceInFile(xid, 0);

    ErrorContext reopen_ctx;
    EXPECT_EQ(Status::PAGE_CORRUPT, db_.open(test_db_file_->path(), &reopen_ctx));
    EXPECT_EQ(std::string("TXN_0215"), reopen_ctx.vnext_code);
    EXPECT_NE(reopen_ctx.message.find("durable commit sequence"), std::string::npos);
}

TEST_F(TransactionVNextContractTest, DuplicateCommittedSequenceFailsClosedOnRestart)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid1 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid1, &ctx)) << ctx.message;

    uint64_t xid2 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid2, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_id, &ctx);

    uint64_t commit_seq1 = 0;
    ASSERT_EQ(Status::OK, tm_->getCommittedTransactionSequence(xid1, commit_seq1, &ctx))
        << ctx.message;

    db_.close();
    patchTipCommitSequenceInFile(xid2, commit_seq1);

    ErrorContext reopen_ctx;
    EXPECT_EQ(Status::PAGE_CORRUPT, db_.open(test_db_file_->path(), &reopen_ctx));
    EXPECT_EQ(std::string("TXN_0215"), reopen_ctx.vnext_code);
    EXPECT_NE(reopen_ctx.message.find("Duplicate durable commit sequence"), std::string::npos);
}

TEST_F(TransactionVNextContractTest, RestartRepairsLostCommitSequenceHighWater)
{
    ErrorContext ctx;
    uint32_t proc_id = 0;
    ASSERT_EQ(Status::OK, ProcArrayManager::registerBackend(&proc_id, &ctx)) << ctx.message;

    uint64_t xid1 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid1, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid1, &ctx)) << ctx.message;

    uint64_t xid2 = 0;
    ASSERT_EQ(Status::OK, tm_->beginTransaction(proc_id, xid2, &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, tm_->commitTransaction(proc_id, xid2, &ctx)) << ctx.message;
    ProcArrayManager::unregisterBackend(proc_id, &ctx);

    uint64_t commit_seq2 = 0;
    ASSERT_EQ(Status::OK, tm_->getCommittedTransactionSequence(xid2, commit_seq2, &ctx))
        << ctx.message;
    ASSERT_GT(commit_seq2, 0u);

    db_.close();
    patchHeaderLatestCommitSequenceInFile(commit_seq2 - 1);

    ASSERT_EQ(Status::OK, db_.open(test_db_file_->path(), &ctx)) << ctx.message;
    ASSERT_EQ(Status::OK, db_.initializeProcArray(32, &ctx)) << ctx.message;
    tm_ = db_.transaction_manager();
    ASSERT_NE(nullptr, tm_);

    EXPECT_EQ(tm_->getLatestCommitSequence(), commit_seq2);

    TransactionSnapshot snapshot{};
    ASSERT_EQ(Status::OK, tm_->captureSnapshot(snapshot, &ctx)) << ctx.message;
    EXPECT_EQ(snapshot.snapshot_commit_seqno_high, commit_seq2);
}
