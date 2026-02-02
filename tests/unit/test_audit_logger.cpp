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
#include "scratchbird/core/audit_logger.h"
#include <thread>
#include <chrono>

using namespace scratchbird::core;

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
    EXPECT_EQ("LOGIN_SUCCESS", AuditLogger::getEventTypeName(AuditEventType::LOGIN_SUCCESS));
    EXPECT_EQ("LOGIN_FAILURE", AuditLogger::getEventTypeName(AuditEventType::LOGIN_FAILURE));
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
