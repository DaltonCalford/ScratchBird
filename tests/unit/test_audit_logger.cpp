/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Unit Tests for Security Audit Logging
 *
 * P0-3: Security Audit Logging (Security Phase 3.5)
 * Tests audit event logging for compliance and forensics (CWE-778)
 */

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

namespace {

bool readFullyAt(int fd, void* buffer, size_t size, off_t offset)
{
    auto* dst = static_cast<uint8_t*>(buffer);
    size_t transferred = 0;
    while (transferred < size)
    {
        const ssize_t rc = ::pread(fd,
                                   dst + transferred,
                                   size - transferred,
                                   offset + static_cast<off_t>(transferred));
        if (rc <= 0)
        {
            return false;
        }
        transferred += static_cast<size_t>(rc);
    }
    return true;
}

bool writeFullyAt(int fd, const void* buffer, size_t size, off_t offset)
{
    const auto* src = static_cast<const uint8_t*>(buffer);
    size_t transferred = 0;
    while (transferred < size)
    {
        const ssize_t rc = ::pwrite(fd,
                                    src + transferred,
                                    size - transferred,
                                    offset + static_cast<off_t>(transferred));
        if (rc <= 0)
        {
            return false;
        }
        transferred += static_cast<size_t>(rc);
    }
    return true;
}

bool tamperFirstStringOccurrencePreservingPageChecksum(const std::string& db_path,
                                                       uint32_t page_size,
                                                       const std::string& needle)
{
    int fd = ::open(db_path.c_str(), O_RDWR);
    if (fd < 0)
    {
        return false;
    }

    bool mutated = false;
    std::vector<uint8_t> page(page_size, 0);
    off_t offset = 0;
    while (readFullyAt(fd, page.data(), page.size(), offset))
    {
        auto it = std::search(page.begin(), page.end(), needle.begin(), needle.end());
        if (it != page.end())
        {
            *it = static_cast<uint8_t>('X');
            auto* header = reinterpret_cast<PageHeader*>(page.data());
            header->checksum = calculatePageChecksum(page.data(), page_size);
            mutated = writeFullyAt(fd, page.data(), page.size(), offset);
            break;
        }
        offset += static_cast<off_t>(page_size);
    }

    ::close(fd);
    return mutated;
}

bool replaceFirstStringOccurrenceInFile(const std::filesystem::path& path,
                                        const std::string& needle,
                                        const std::string& replacement)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
    {
        return false;
    }
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    const size_t pos = contents.find(needle);
    if (pos == std::string::npos)
    {
        return false;
    }
    contents.replace(pos, needle.size(), replacement);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        return false;
    }
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return out.good();
}

} // namespace

/**
 * Test Fixture for Audit Logger
 */
class AuditLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    // Helper: Create test UUID
    ID createTestID(uint32_t value) {
        ID id;
        std::memset(&id, 0, sizeof(id));
        std::memcpy(&id, &value, sizeof(uint32_t));
        return id;
    }
};

class CatalogBackedAuditLoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("audit_logger", ".sbdb");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        logger_ = db_.audit_logger();
        ASSERT_NE(logger_, nullptr);
    }

    void TearDown() override
    {
        db_.close();
        db_file_.reset();
    }

    void reopen()
    {
        ErrorContext ctx;
        db_.close();
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;
        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);
        logger_ = db_.audit_logger();
        ASSERT_NE(logger_, nullptr);
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    Database db_{};
    CatalogManager* catalog_ = nullptr;
    AuditLogger* logger_ = nullptr;
};

// ===== Basic Functionality Tests =====

TEST_F(AuditLoggerTest, InitialState) {
    AuditLogger logger;

    EXPECT_EQ(0, logger.getTotalEventCount());
}

TEST_F(AuditLoggerTest, LogSingleEvent) {
    AuditLogger logger;

    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.user_id = createTestID(1);
    event.username = "user1";
    event.success = true;

    ErrorContext ctx;
    Status status = logger.logEvent(event, &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(1, logger.getTotalEventCount());
    EXPECT_GT(event.event_id, 0);
    EXPECT_GT(event.timestamp, 0);
}

TEST_F(AuditLoggerTest, EventIDSequential) {
    AuditLogger logger;

    AuditEvent event1, event2, event3;
    event1.event_type = AuditEventType::LOGIN_SUCCESS;
    event2.event_type = AuditEventType::LOGIN_FAILURE;
    event3.event_type = AuditEventType::LOGOUT;

    ErrorContext ctx;
    logger.logEvent(event1, &ctx);
    logger.logEvent(event2, &ctx);
    logger.logEvent(event3, &ctx);

    EXPECT_EQ(1, event1.event_id);
    EXPECT_EQ(2, event2.event_id);
    EXPECT_EQ(3, event3.event_id);
}

TEST_F(AuditLoggerTest, TimestampAutoFilled) {
    AuditLogger logger;

    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;

    uint64_t before = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    ErrorContext ctx;
    logger.logEvent(event, &ctx);

    uint64_t after = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    EXPECT_GE(event.timestamp, before);
    EXPECT_LE(event.timestamp, after);
}

// ===== Event Type Tests =====

TEST_F(AuditLoggerTest, AllEventTypes) {
    AuditLogger logger;

    // Test all event types can be logged
    std::vector<AuditEventType> types = {
        AuditEventType::LOGIN_SUCCESS,
        AuditEventType::LOGIN_FAILURE,
        AuditEventType::LOGOUT,
        AuditEventType::PASSWORD_CHANGE,
        AuditEventType::PERMISSION_DENIED,
        AuditEventType::USER_CREATED,
        AuditEventType::ROLE_GRANTED,
        AuditEventType::DDL_CREATE,
        AuditEventType::DATABASE_STARTUP,
    };

    ErrorContext ctx;
    for (auto type : types) {
        AuditEvent event;
        event.event_type = type;
        event.username = "testuser";
        event.success = true;

        Status status = logger.logEvent(event, &ctx);
        EXPECT_EQ(Status::OK, status);
    }

    EXPECT_EQ(types.size(), logger.getTotalEventCount());
}

TEST_F(AuditLoggerTest, EventTypeNames) {
    // Auth parity checks
    EXPECT_EQ("LOGIN_SUCCESS", AuditLogger::getEventTypeName(AuditEventType::LOGIN_SUCCESS));
    EXPECT_EQ("LOGIN_FAILURE", AuditLogger::getEventTypeName(AuditEventType::LOGIN_FAILURE));
    EXPECT_EQ("BOOTSTRAP_ATTEMPT", AuditLogger::getEventTypeName(AuditEventType::BOOTSTRAP_ATTEMPT));
    EXPECT_EQ("BOOTSTRAP_SUCCESS", AuditLogger::getEventTypeName(AuditEventType::BOOTSTRAP_SUCCESS));
    EXPECT_EQ("BOOTSTRAP_FAILURE", AuditLogger::getEventTypeName(AuditEventType::BOOTSTRAP_FAILURE));
    EXPECT_EQ("BOOTSTRAP_REVOKED", AuditLogger::getEventTypeName(AuditEventType::BOOTSTRAP_REVOKED));
    EXPECT_EQ("REATTACH_TOKEN_ISSUED", AuditLogger::getEventTypeName(AuditEventType::REATTACH_TOKEN_ISSUED));
    EXPECT_EQ("REATTACH_SUCCESS", AuditLogger::getEventTypeName(AuditEventType::REATTACH_SUCCESS));
    EXPECT_EQ("REATTACH_FAILURE", AuditLogger::getEventTypeName(AuditEventType::REATTACH_FAILURE));
    EXPECT_EQ("REATTACH_TOKEN_REVOKED", AuditLogger::getEventTypeName(AuditEventType::REATTACH_TOKEN_REVOKED));
    EXPECT_EQ("AUTH_POLICY_DECISION", AuditLogger::getEventTypeName(AuditEventType::AUTH_POLICY_DECISION));
    EXPECT_EQ("TOKEN_AUTH_USED", AuditLogger::getEventTypeName(AuditEventType::TOKEN_AUTH_USED));
    EXPECT_EQ("TOKEN_AUTH_REVOKED", AuditLogger::getEventTypeName(AuditEventType::TOKEN_AUTH_REVOKED));
    EXPECT_EQ("MANAGED_PREFACE_DECISION",
              AuditLogger::getEventTypeName(AuditEventType::MANAGED_PREFACE_DECISION));
    EXPECT_EQ("MANAGED_DBBT_ISSUED", AuditLogger::getEventTypeName(AuditEventType::MANAGED_DBBT_ISSUED));

    // Service/runtime parity checks
    EXPECT_EQ("DATABASE_STARTUP", AuditLogger::getEventTypeName(AuditEventType::DATABASE_STARTUP));
    EXPECT_EQ("DATABASE_SHUTDOWN", AuditLogger::getEventTypeName(AuditEventType::DATABASE_SHUTDOWN));

    EXPECT_EQ("PERMISSION_DENIED", AuditLogger::getEventTypeName(AuditEventType::PERMISSION_DENIED));
    EXPECT_EQ("USER_CREATED", AuditLogger::getEventTypeName(AuditEventType::USER_CREATED));
}

// ===== Query Tests =====

TEST_F(AuditLoggerTest, QueryAllEvents) {
    AuditLogger logger;
    ErrorContext ctx;

    // Log 5 events
    for (int i = 0; i < 5; i++) {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_SUCCESS;
        event.username = "user" + std::to_string(i);
        event.success = true;
        logger.logEvent(event, &ctx);
    }

    // Query all
    AuditQuery query;
    query.limit = 100;
    std::vector<AuditEvent> results;

    Status status = logger.queryAuditLog(query, results, &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(5, results.size());
}

TEST_F(AuditLoggerTest, QueryByUsername) {
    AuditLogger logger;
    ErrorContext ctx;

    // Log events for different users
    for (int i = 0; i < 3; i++) {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_SUCCESS;
        event.username = "alice";
        event.success = true;
        logger.logEvent(event, &ctx);
    }

    for (int i = 0; i < 2; i++) {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_SUCCESS;
        event.username = "bob";
        event.success = true;
        logger.logEvent(event, &ctx);
    }

    // Query by username
    AuditQuery query;
    query.username = "alice";
    std::vector<AuditEvent> results;

    Status status = logger.queryAuditLog(query, results, &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(3, results.size());
    for (const auto& event : results) {
        EXPECT_EQ("alice", event.username);
    }
}

TEST_F(AuditLoggerTest, QueryByEventType) {
    AuditLogger logger;
    ErrorContext ctx;

    // Log mixed events
    AuditEvent login;
    login.event_type = AuditEventType::LOGIN_SUCCESS;
    login.username = "user1";
    login.success = true;
    logger.logEvent(login, &ctx);

    AuditEvent logout;
    logout.event_type = AuditEventType::LOGOUT;
    logout.username = "user1";
    logout.success = true;
    logger.logEvent(logout, &ctx);

    AuditEvent failed;
    failed.event_type = AuditEventType::LOGIN_FAILURE;
    failed.username = "user2";
    failed.success = false;
    logger.logEvent(failed, &ctx);

    // Query only failures
    AuditQuery query;
    query.event_type = AuditEventType::LOGIN_FAILURE;
    std::vector<AuditEvent> results;

    Status status = logger.queryAuditLog(query, results, &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(1, results.size());
    EXPECT_EQ(AuditEventType::LOGIN_FAILURE, results[0].event_type);
}

TEST_F(AuditLoggerTest, QueryBySuccess) {
    AuditLogger logger;
    ErrorContext ctx;

    // Log successful and failed events
    for (int i = 0; i < 3; i++) {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_SUCCESS;
        event.username = "user1";
        event.success = true;
        logger.logEvent(event, &ctx);
    }

    for (int i = 0; i < 2; i++) {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_FAILURE;
        event.username = "user2";
        event.success = false;
        logger.logEvent(event, &ctx);
    }

    // Query only failures
    AuditQuery query;
    query.success = false;
    std::vector<AuditEvent> results;

    Status status = logger.queryAuditLog(query, results, &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(2, results.size());
    for (const auto& event : results) {
        EXPECT_FALSE(event.success);
    }
}

TEST_F(AuditLoggerTest, QueryPagination) {
    AuditLogger logger;
    ErrorContext ctx;

    // Log 10 events
    for (int i = 0; i < 10; i++) {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_SUCCESS;
        event.username = "user" + std::to_string(i);
        event.success = true;
        logger.logEvent(event, &ctx);
    }

    // Query first page (5 items)
    AuditQuery query1;
    query1.limit = 5;
    query1.offset = 0;
    std::vector<AuditEvent> page1;
    logger.queryAuditLog(query1, page1, &ctx);

    EXPECT_EQ(5, page1.size());

    // Query second page
    AuditQuery query2;
    query2.limit = 5;
    query2.offset = 5;
    std::vector<AuditEvent> page2;
    logger.queryAuditLog(query2, page2, &ctx);

    EXPECT_EQ(5, page2.size());

    // No overlap
    EXPECT_NE(page1[0].event_id, page2[0].event_id);
}

TEST_F(AuditLoggerTest, QuerySortingDescending) {
    AuditLogger logger;
    ErrorContext ctx;

    // Log events with delays
    for (int i = 0; i < 3; i++) {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_SUCCESS;
        event.username = "user" + std::to_string(i);
        event.success = true;
        logger.logEvent(event, &ctx);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Query descending (newest first)
    AuditQuery query;
    query.descending = true;
    std::vector<AuditEvent> results;
    logger.queryAuditLog(query, results, &ctx);

    EXPECT_EQ(3, results.size());
    EXPECT_GT(results[0].timestamp, results[1].timestamp);
    EXPECT_GT(results[1].timestamp, results[2].timestamp);
}

// ===== Helper Functions Tests =====

TEST_F(AuditLoggerTest, CreateLoginSuccessEvent) {
    ID user_id = createTestID(123);
    AuditEvent event = AuditLogger::createLoginSuccessEvent(user_id, "alice");

    EXPECT_EQ(AuditEventType::LOGIN_SUCCESS, event.event_type);
    EXPECT_EQ("alice", event.username);
    EXPECT_TRUE(event.success);
    EXPECT_EQ(0, std::memcmp(&event.user_id, &user_id, sizeof(ID)));
}

TEST_F(AuditLoggerTest, CreateLoginFailureEvent) {
    AuditEvent event = AuditLogger::createLoginFailureEvent("bob", "invalid_password");

    EXPECT_EQ(AuditEventType::LOGIN_FAILURE, event.event_type);
    EXPECT_EQ("bob", event.username);
    EXPECT_FALSE(event.success);
    EXPECT_NE(std::string::npos, event.details.find("invalid_password"));
}

TEST_F(AuditLoggerTest, CreatePermissionDeniedEvent) {
    ID user_id = createTestID(456);
    AuditEvent event = AuditLogger::createPermissionDeniedEvent(
        user_id, "charlie", "TABLE", "employees", "SELECT");

    EXPECT_EQ(AuditEventType::PERMISSION_DENIED, event.event_type);
    EXPECT_EQ("charlie", event.username);
    EXPECT_EQ("TABLE", event.object_type);
    EXPECT_EQ("employees", event.object_name);
    EXPECT_FALSE(event.success);
    EXPECT_NE(std::string::npos, event.details.find("SELECT"));
}

TEST_F(AuditLoggerTest, CreateUserCreatedEvent) {
    ID creator_id = createTestID(1);
    ID new_user_id = createTestID(2);

    AuditEvent event = AuditLogger::createUserCreatedEvent(
        creator_id, "admin", new_user_id, "newuser");

    EXPECT_EQ(AuditEventType::USER_CREATED, event.event_type);
    EXPECT_EQ("admin", event.username);
    EXPECT_EQ("newuser", event.target_username);
    EXPECT_EQ("USER", event.object_type);
    EXPECT_TRUE(event.success);
}

// ===== Thread Safety Tests =====

TEST_F(AuditLoggerTest, ConcurrentLogging) {
    AuditLogger logger;

    // Multiple threads logging events
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&logger, i]() {
            ErrorContext ctx;
            for (int j = 0; j < 10; j++) {
                AuditEvent event;
                event.event_type = AuditEventType::LOGIN_SUCCESS;
                event.username = "user" + std::to_string(i);
                event.success = true;
                logger.logEvent(event, &ctx);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have 100 events
    EXPECT_EQ(100, logger.getTotalEventCount());
}

TEST_F(AuditLoggerTest, ConcurrentQueryAndLog) {
    AuditLogger logger;
    ErrorContext ctx;

    // Log some initial events
    for (int i = 0; i < 10; i++) {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_SUCCESS;
        event.username = "user1";
        event.success = true;
        logger.logEvent(event, &ctx);
    }

    // Concurrent logging and querying
    std::thread logger_thread([&logger]() {
        ErrorContext ctx;
        for (int i = 0; i < 50; i++) {
            AuditEvent event;
            event.event_type = AuditEventType::LOGIN_SUCCESS;
            event.username = "user2";
            event.success = true;
            logger.logEvent(event, &ctx);
        }
    });

    std::thread query_thread([&logger]() {
        ErrorContext ctx;
        for (int i = 0; i < 10; i++) {
            AuditQuery query;
            std::vector<AuditEvent> results;
            logger.queryAuditLog(query, results, &ctx);
        }
    });

    logger_thread.join();
    query_thread.join();

    // Should have 60 total events
    EXPECT_EQ(60, logger.getTotalEventCount());
}

// ===== Edge Cases Tests =====

TEST_F(AuditLoggerTest, EmptyUsername) {
    AuditLogger logger;
    ErrorContext ctx;

    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.username = "";
    event.success = true;

    Status status = logger.logEvent(event, &ctx);
    EXPECT_EQ(Status::OK, status);
}

TEST_F(AuditLoggerTest, VeryLongDetails) {
    AuditLogger logger;
    ErrorContext ctx;

    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_FAILURE;
    event.username = "user1";
    event.success = false;
    event.details = std::string(10000, 'x');

    Status status = logger.logEvent(event, &ctx);
    EXPECT_EQ(Status::OK, status);
}

TEST_F(AuditLoggerTest, QueryNoResults) {
    AuditLogger logger;
    ErrorContext ctx;

    // Log events for user1
    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.username = "user1";
    event.success = true;
    logger.logEvent(event, &ctx);

    // Query for non-existent user
    AuditQuery query;
    query.username = "nonexistent";
    std::vector<AuditEvent> results;

    Status status = logger.queryAuditLog(query, results, &ctx);

    EXPECT_EQ(Status::OK, status);
    EXPECT_EQ(0, results.size());
}

// ===== Integration Test: Compliance Scenario =====

TEST_F(AuditLoggerTest, ComplianceAuditTrail) {
    AuditLogger logger;
    ErrorContext ctx;

    ID admin_id = createTestID(1);
    ID user_id = createTestID(2);

    // Scenario: Admin creates user, user logs in, user tries unauthorized access

    // 1. Admin creates user
    AuditEvent create = AuditLogger::createUserCreatedEvent(
        admin_id, "admin", user_id, "newuser");
    logger.logEvent(create, &ctx);

    // 2. User attempts login (fails - wrong password)
    AuditEvent login_fail = AuditLogger::createLoginFailureEvent(
        "newuser", "invalid_password");
    logger.logEvent(login_fail, &ctx);

    // 3. User logs in successfully
    AuditEvent login_success = AuditLogger::createLoginSuccessEvent(
        user_id, "newuser");
    logger.logEvent(login_success, &ctx);

    // 4. User tries to access restricted table
    AuditEvent denied = AuditLogger::createPermissionDeniedEvent(
        user_id, "newuser", "TABLE", "sensitive_data", "SELECT");
    logger.logEvent(denied, &ctx);

    // Query all events for this user
    AuditQuery query;
    query.username = "newuser";
    std::vector<AuditEvent> trail;
    logger.queryAuditLog(query, trail, &ctx);

    // Should have 3 events (create uses target_username)
    EXPECT_EQ(3, trail.size());

    // Verify audit trail completeness
    EXPECT_EQ(AuditEventType::LOGIN_FAILURE, trail[2].event_type);
    EXPECT_EQ(AuditEventType::LOGIN_SUCCESS, trail[1].event_type);
    EXPECT_EQ(AuditEventType::PERMISSION_DENIED, trail[0].event_type);
}

TEST_F(CatalogBackedAuditLoggerTest, VerifyIntegrityPassesForPersistedAuditChain)
{
    ErrorContext ctx;

    for (int i = 0; i < 3; ++i)
    {
        AuditEvent event;
        event.event_type = AuditEventType::LOGIN_SUCCESS;
        event.username = "catalog_user_" + std::to_string(i);
        event.success = true;
        ASSERT_EQ(logger_->logEvent(event, &ctx), Status::OK) << ctx.message;
    }

    ASSERT_EQ(logger_->flush(&ctx), Status::OK) << ctx.message;

    AuditIntegrityResult result;
    ASSERT_EQ(logger_->verifyIntegrity(result, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(result.chain_intact);
    EXPECT_EQ(3u, result.verified_event_count);
    EXPECT_EQ(3u, result.last_verified_event_id);
    EXPECT_EQ(0u, result.first_bad_event_id);
    EXPECT_TRUE(result.failure_reason.empty());
}

TEST_F(CatalogBackedAuditLoggerTest, CatalogRejectsOutOfSequenceAuditAppend)
{
    ErrorContext ctx;

    AuditEvent first;
    first.event_type = AuditEventType::LOGIN_SUCCESS;
    first.username = "append_guard";
    first.success = true;
    ASSERT_EQ(logger_->logEvent(first, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(logger_->flush(&ctx), Status::OK) << ctx.message;

    AuditEvent forged;
    forged.event_id = 1;
    forged.timestamp = first.timestamp + 1;
    forged.event_type = AuditEventType::LOGIN_FAILURE;
    forged.username = "append_guard";
    forged.success = false;

    std::array<uint8_t, 32> zero_hash{};
    const auto hash_curr = AuditLogger::computeChainHash(forged, zero_hash);
    const Status status = catalog_->appendAuditLog(forged, zero_hash, hash_curr, &ctx);

    EXPECT_EQ(Status::CONSTRAINT_VIOLATION, status);
    EXPECT_NE(std::string(ctx.message).find("append-only"), std::string::npos);
}

TEST_F(CatalogBackedAuditLoggerTest, VerifyIntegrityDetectsTamperedPersistedAuditRecord)
{
    ErrorContext ctx;
    static std::atomic<uint64_t> counter{0};
    const std::string marker =
        "audit_tamper_target_" + std::to_string(counter.fetch_add(1));

    AuditEvent event1;
    event1.event_type = AuditEventType::LOGIN_SUCCESS;
    event1.username = marker;
    event1.success = true;
    ASSERT_EQ(logger_->logEvent(event1, &ctx), Status::OK) << ctx.message;

    AuditEvent event2;
    event2.event_type = AuditEventType::LOGOUT;
    event2.username = "audit_tail";
    event2.success = true;
    ASSERT_EQ(logger_->logEvent(event2, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(logger_->flush(&ctx), Status::OK) << ctx.message;
    ASSERT_EQ(db_.buffer_pool()->flushAll(&ctx), Status::OK) << ctx.message;
    db_.close();

    ASSERT_TRUE(tamperFirstStringOccurrencePreservingPageChecksum(
        db_file_->path(), 16384, marker));

    reopen();

    AuditIntegrityResult result;
    const Status status = logger_->verifyIntegrity(result, &ctx);
    EXPECT_EQ(Status::CHECKSUM_MISMATCH, status);
    EXPECT_FALSE(result.chain_intact);
    EXPECT_EQ(1u, result.first_bad_event_id);
    EXPECT_NE(std::string(result.failure_reason).find("hash_curr"), std::string::npos);
}

TEST_F(CatalogBackedAuditLoggerTest, ExportAuditPackagePersistsSegmentAndValidates)
{
    ErrorContext ctx;

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    profile.audit_sink_profile_id = generateUuidV7();
    profile.profile_name = "local_compliance_export";
    profile.sink_type = "LOCAL_APPEND_ONLY";
    profile.failure_policy = "BEST_EFFORT";
    profile.config_json = "{\"tier\":\"hot_queryable\"}";
    ASSERT_EQ(catalog_->upsertAuditSinkProfileCatalogEntry(profile, &ctx), Status::OK) << ctx.message;

    AuditEvent first;
    first.event_type = AuditEventType::LOGIN_SUCCESS;
    first.username = "export_user_a";
    first.details = "token=super-secret endpoint=https://audit.internal:9443";
    first.success = true;
    ASSERT_EQ(logger_->logEvent(first, &ctx), Status::OK) << ctx.message;

    AuditEvent second;
    second.event_type = AuditEventType::PERMISSION_DENIED;
    second.username = "export_user_b";
    second.object_type = "TABLE";
    second.object_name = "sensitive_table";
    second.success = false;
    ASSERT_EQ(logger_->logEvent(second, &ctx), Status::OK) << ctx.message;

    const std::filesystem::path export_path =
        scratchbird::testing::uniqueTestDbPath("audit_export_package", ".sbpkg");

    AuditExportPackageRequest request;
    request.sink_profile_id = profile.audit_sink_profile_id;
    request.output_path = export_path.string();

    AuditExportPackageResult export_result;
    ASSERT_EQ(logger_->exportAuditPackage(request, export_result, &ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(std::filesystem::exists(export_path));
    EXPECT_EQ(2u, export_result.event_count);

    std::ifstream exported(export_path, std::ios::binary);
    ASSERT_TRUE(exported.is_open());
    std::ostringstream exported_contents;
    exported_contents << exported.rdbuf();
    EXPECT_EQ(exported_contents.str().find("super-secret"), std::string::npos);
    EXPECT_EQ(exported_contents.str().find("audit.internal"), std::string::npos);
    EXPECT_NE(exported_contents.str().find("<redacted>"), std::string::npos);
    EXPECT_NE(exported_contents.str().find("<endpoint>"), std::string::npos);

    AuditExportValidationResult validation;
    ASSERT_EQ(logger_->validateAuditPackage(export_path.string(), validation, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(validation.manifest_matches_catalog);
    EXPECT_TRUE(validation.payload_checksum_valid);
    EXPECT_TRUE(validation.package_valid);
    EXPECT_EQ(2u, validation.event_count);

    std::vector<CatalogManager::AuditExportSegmentCatalogInfo> segments;
    ASSERT_EQ(catalog_->listAuditExportSegmentCatalogEntries(profile.audit_sink_profile_id, segments, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(1u, segments.size());
    EXPECT_EQ(export_result.segment_id, segments[0].audit_export_segment_id);
    EXPECT_EQ(1u, segments[0].segment_seq);
    EXPECT_EQ("EXPORT_DELIVERY_EVENT", segments[0].evidence_class);
    EXPECT_NE(std::string::npos, segments[0].payload_manifest.find("event_count=2"));

    std::filesystem::remove(export_path);
}

TEST_F(CatalogBackedAuditLoggerTest, AuditExportSegmentRejectsOutOfSequenceAppend)
{
    ErrorContext ctx;

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    profile.audit_sink_profile_id = generateUuidV7();
    profile.profile_name = "segment_append_guard";
    profile.sink_type = "LOCAL_APPEND_ONLY";
    profile.failure_policy = "BEST_EFFORT";
    ASSERT_EQ(catalog_->upsertAuditSinkProfileCatalogEntry(profile, &ctx), Status::OK) << ctx.message;

    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.username = "segment_guard_user";
    event.success = true;
    ASSERT_EQ(logger_->logEvent(event, &ctx), Status::OK) << ctx.message;

    const std::filesystem::path export_path =
        scratchbird::testing::uniqueTestDbPath("audit_export_guard", ".sbpkg");
    AuditExportPackageRequest request;
    request.sink_profile_id = profile.audit_sink_profile_id;
    request.output_path = export_path.string();

    AuditExportPackageResult export_result;
    ASSERT_EQ(logger_->exportAuditPackage(request, export_result, &ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::AuditExportSegmentCatalogInfo> segments;
    ASSERT_EQ(catalog_->listAuditExportSegmentCatalogEntries(profile.audit_sink_profile_id, segments, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(1u, segments.size());

    CatalogManager::AuditExportSegmentCatalogInfo forged;
    forged.audit_export_segment_id = generateUuidV7();
    forged.audit_sink_profile_id = profile.audit_sink_profile_id;
    forged.evidence_class = "EXPORT_DELIVERY_EVENT";
    forged.segment_seq = 3;
    forged.range_start_time = 10;
    forged.range_end_time = 11;
    forged.delivery_state = "LOCAL_COMMITTED";
    forged.payload_manifest =
        "SB_AUDIT_EXPORT_PACKAGE_V1\n"
        "manifest_version=1\n"
        "segment_uuid=" + forged.audit_export_segment_id.toString() + "\n"
        "database_uuid=" + db_.uuid().toString() + "\n"
        "sink_profile_uuid=" + profile.audit_sink_profile_id.toString() + "\n"
        "profile_name=" + profile.profile_name + "\n"
        "sink_type=LOCAL_APPEND_ONLY\n"
        "failure_policy=BEST_EFFORT\n"
        "evidence_class=EXPORT_DELIVERY_EVENT\n"
        "segment_seq=3\n"
        "range_start_time=10\n"
        "range_end_time=11\n"
        "event_count=1\n"
        "first_event_id=2\n"
        "last_event_id=2\n"
        "first_event_hash_prev=0000000000000000000000000000000000000000000000000000000000000000\n"
        "last_event_hash_curr=1111111111111111111111111111111111111111111111111111111111111111\n"
        "payload_sha256=2222222222222222222222222222222222222222222222222222222222222222\n"
        "payload_bytes=1\n"
        "END_MANIFEST\n";
    forged.hash_prev = segments[0].hash_curr;
    forged.hash_curr = AuditLogger::computeExportSegmentHash(forged.payload_manifest, forged.hash_prev);

    const Status status = catalog_->appendAuditExportSegmentCatalogEntry(forged, &ctx);
    EXPECT_EQ(Status::CONSTRAINT_VIOLATION, status);
    EXPECT_NE(std::string(ctx.message).find("append-only"), std::string::npos);

    std::filesystem::remove(export_path);
}

TEST_F(CatalogBackedAuditLoggerTest, ValidateAuditPackageDetectsPayloadTamper)
{
    ErrorContext ctx;

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    profile.audit_sink_profile_id = generateUuidV7();
    profile.profile_name = "tamper_validate_export";
    profile.sink_type = "LOCAL_APPEND_ONLY";
    profile.failure_policy = "BEST_EFFORT";
    ASSERT_EQ(catalog_->upsertAuditSinkProfileCatalogEntry(profile, &ctx), Status::OK) << ctx.message;

    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.username = "alice";
    event.success = true;
    ASSERT_EQ(logger_->logEvent(event, &ctx), Status::OK) << ctx.message;

    const std::filesystem::path export_path =
        scratchbird::testing::uniqueTestDbPath("audit_export_tamper", ".sbpkg");
    AuditExportPackageRequest request;
    request.sink_profile_id = profile.audit_sink_profile_id;
    request.output_path = export_path.string();

    AuditExportPackageResult export_result;
    ASSERT_EQ(logger_->exportAuditPackage(request, export_result, &ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(replaceFirstStringOccurrenceInFile(export_path, "\"username\":\"alice\"", "\"username\":\"al1ce\""));

    AuditExportValidationResult validation;
    const Status status = logger_->validateAuditPackage(export_path.string(), validation, &ctx);
    EXPECT_EQ(Status::CHECKSUM_MISMATCH, status);
    EXPECT_FALSE(validation.payload_checksum_valid);
    EXPECT_FALSE(validation.package_valid);
    EXPECT_NE(std::string(ctx.message).find("checksum"), std::string::npos);

    std::filesystem::remove(export_path);
}

TEST_F(CatalogBackedAuditLoggerTest, LegalHoldBlocksRetentionEligibilityAndAppendsEvidence)
{
    ErrorContext ctx;

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    profile.audit_sink_profile_id = generateUuidV7();
    profile.profile_name = "legal_hold_profile";
    profile.sink_type = "LOCAL_APPEND_ONLY";
    profile.failure_policy = "BEST_EFFORT";
    profile.config_json =
        "{\"retention_policy\":{\"hot_retention_days\":30,\"archive_retention_days\":365}}";
    ASSERT_EQ(catalog_->upsertAuditSinkProfileCatalogEntry(profile, &ctx), Status::OK) << ctx.message;

    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.username = "hold_user";
    event.success = true;
    ASSERT_EQ(logger_->logEvent(event, &ctx), Status::OK) << ctx.message;

    const std::filesystem::path export_path =
        scratchbird::testing::uniqueTestDbPath("audit_hold_export", ".sbpkg");
    AuditExportPackageRequest package_request;
    package_request.sink_profile_id = profile.audit_sink_profile_id;
    package_request.output_path = export_path.string();

    AuditExportPackageResult package_result;
    ASSERT_EQ(logger_->exportAuditPackage(package_request, package_result, &ctx), Status::OK) << ctx.message;

    AuditLegalHoldCommand hold_command;
    hold_command.sink_profile_id = profile.audit_sink_profile_id;
    hold_command.enable_hold = true;
    hold_command.actor = "secops";
    hold_command.reason = "forensic investigation";

    AuditLegalHoldResult hold_result;
    ASSERT_EQ(logger_->setAuditLegalHold(hold_command, hold_result, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(hold_result.legal_hold_active);

    AuditRetentionEvaluationRequest retention_request;
    retention_request.sink_profile_id = profile.audit_sink_profile_id;
    retention_request.requested_by = "secops";
    retention_request.now_time =
        static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()) +
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::hours(24 * 400)).count());

    AuditRetentionEvaluationResult retention_result;
    ASSERT_EQ(logger_->evaluateRetentionPolicy(retention_request, retention_result, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(retention_result.legal_hold_active);
    EXPECT_EQ(0u, retention_result.segments_eligible);
    EXPECT_GE(retention_result.segments_blocked, 1u);

    std::vector<CatalogManager::AuditExportSegmentCatalogInfo> segments;
    ASSERT_EQ(catalog_->listAuditExportSegmentCatalogEntries(profile.audit_sink_profile_id, segments, &ctx),
              Status::OK) << ctx.message;
    ASSERT_GE(segments.size(), 3u);
    EXPECT_EQ("LEGAL_HOLD_ENABLED", segments[segments.size() - 2].evidence_class);
    EXPECT_EQ("RETENTION_POLICY_DECISION", segments.back().evidence_class);
    EXPECT_NE(segments.back().payload_manifest.find("legal_hold_active=true"), std::string::npos);

    std::filesystem::remove(export_path);
}

TEST_F(CatalogBackedAuditLoggerTest, RetentionEvaluationMarksExpiredSegmentsEligibleWithoutHold)
{
    ErrorContext ctx;

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    profile.audit_sink_profile_id = generateUuidV7();
    profile.profile_name = "retention_profile";
    profile.sink_type = "LOCAL_APPEND_ONLY";
    profile.failure_policy = "BEST_EFFORT";
    profile.config_json =
        "{\"retention_policy\":{\"hot_retention_days\":1,\"archive_retention_days\":1}}";
    ASSERT_EQ(catalog_->upsertAuditSinkProfileCatalogEntry(profile, &ctx), Status::OK) << ctx.message;

    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.username = "retention_user";
    event.success = true;
    ASSERT_EQ(logger_->logEvent(event, &ctx), Status::OK) << ctx.message;

    const std::filesystem::path export_path =
        scratchbird::testing::uniqueTestDbPath("audit_retention_export", ".sbpkg");
    AuditExportPackageRequest package_request;
    package_request.sink_profile_id = profile.audit_sink_profile_id;
    package_request.output_path = export_path.string();

    AuditExportPackageResult package_result;
    ASSERT_EQ(logger_->exportAuditPackage(package_request, package_result, &ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::AuditExportSegmentCatalogInfo> segments;
    ASSERT_EQ(catalog_->listAuditExportSegmentCatalogEntries(profile.audit_sink_profile_id, segments, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(1u, segments.size());

    AuditRetentionEvaluationRequest retention_request;
    retention_request.sink_profile_id = profile.audit_sink_profile_id;
    retention_request.append_evidence = false;
    retention_request.now_time =
        segments.front().created_time +
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::hours(24 * 2)).count());

    AuditRetentionEvaluationResult retention_result;
    ASSERT_EQ(logger_->evaluateRetentionPolicy(retention_request, retention_result, &ctx), Status::OK)
        << ctx.message;
    EXPECT_FALSE(retention_result.legal_hold_active);
    EXPECT_EQ(1u, retention_result.segments_examined);
    EXPECT_EQ(1u, retention_result.segments_eligible);
    EXPECT_EQ(0u, retention_result.segments_blocked);

    std::filesystem::remove(export_path);
}
