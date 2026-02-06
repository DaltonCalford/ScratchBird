/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include <gtest/gtest.h>
#include "scratchbird/ipc/copy_flow_control.h"
#include <thread>
#include <chrono>

using namespace scratchbird::ipc;

// ============================================================================
// CopyFlowController Tests
// ============================================================================

class CopyFlowControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller_ = std::make_unique<CopyFlowController>(
            1,  // session_id
            10, // initial_credits
            1024 * 1024 // max_window_size (1MB)
        );
    }
    
    std::unique_ptr<CopyFlowController> controller_;
};

TEST_F(CopyFlowControllerTest, ConstructorInitializesCorrectly) {
    auto stats = controller_->getStats();
    EXPECT_EQ(stats.session_id, 1);
    EXPECT_EQ(stats.credits_available, 10);
    EXPECT_EQ(stats.buffer_available, 1024 * 1024);
    EXPECT_EQ(stats.max_window_size, 1024 * 1024);
    EXPECT_FALSE(stats.is_paused);
}

TEST_F(CopyFlowControllerTest, CanSendReturnsTrueWhenCreditsAvailable) {
    EXPECT_TRUE(controller_->canSend(100));
    EXPECT_TRUE(controller_->canSend(1024));
}

TEST_F(CopyFlowControllerTest, CanSendReturnsFalseWhenPaused) {
    controller_->pause();
    EXPECT_FALSE(controller_->canSend(1));
}

TEST_F(CopyFlowControllerTest, CanSendReturnsFalseWhenNoCredits) {
    // Exhaust all credits
    for (int i = 0; i < 10; i++) {
        controller_->acquireCredits(100);
    }
    EXPECT_FALSE(controller_->canSend(100));
}

TEST_F(CopyFlowControllerTest, AcquireCreditsSuccess) {
    EXPECT_TRUE(controller_->acquireCredits(1000));
    auto stats = controller_->getStats();
    EXPECT_EQ(stats.credits_available, 9);
}

TEST_F(CopyFlowControllerTest, AcquireCreditsFailureWhenPaused) {
    controller_->pause();
    EXPECT_FALSE(controller_->acquireCredits(1));
}

TEST_F(CopyFlowControllerTest, AcquireCreditsFailureWhenNoCredits) {
    // Exhaust all credits
    for (int i = 0; i < 10; i++) {
        controller_->acquireCredits(100);
    }
    EXPECT_FALSE(controller_->acquireCredits(100));
}

TEST_F(CopyFlowControllerTest, ReleaseCreditsIncreasesCredits) {
    controller_->acquireCredits(100);
    auto stats_before = controller_->getStats();
    EXPECT_EQ(stats_before.credits_available, 9);
    
    controller_->releaseCredits(5, 0);
    auto stats_after = controller_->getStats();
    EXPECT_EQ(stats_after.credits_available, 14);
}

TEST_F(CopyFlowControllerTest, ReleaseCreditsCapsAtMax) {
    // Try to release more than max
    controller_->releaseCredits(100, 0);
    auto stats = controller_->getStats();
    EXPECT_EQ(stats.credits_available, 10); // Should cap at initial value, not exceed max
}

TEST_F(CopyFlowControllerTest, UpdateBufferAvailability) {
    controller_->updateBufferAvailability(500000);
    auto stats = controller_->getStats();
    EXPECT_EQ(stats.buffer_available, 500000);
}

TEST_F(CopyFlowControllerTest, PauseAndResume) {
    EXPECT_FALSE(controller_->isPaused());
    
    controller_->pause();
    EXPECT_TRUE(controller_->isPaused());
    EXPECT_FALSE(controller_->canSend(1));
    
    controller_->resume();
    EXPECT_FALSE(controller_->isPaused());
    EXPECT_TRUE(controller_->canSend(1));
}

TEST_F(CopyFlowControllerTest, RecordReceivedUpdatesStats) {
    controller_->recordReceived(1024);
    controller_->recordReceived(2048);
    
    auto stats = controller_->getStats();
    EXPECT_EQ(stats.total_bytes_received, 3072);
}

TEST_F(CopyFlowControllerTest, ResetRestoresState) {
    controller_->acquireCredits(100);
    controller_->recordReceived(1024);
    controller_->pause();
    
    controller_->reset();
    
    auto stats = controller_->getStats();
    EXPECT_EQ(stats.credits_available, 5); // Half of max
    EXPECT_EQ(stats.total_bytes_received, 0);
    EXPECT_EQ(stats.total_bytes_sent, 0);
    EXPECT_FALSE(stats.is_paused);
}

TEST_F(CopyFlowControllerTest, WaitForCreditsBlocksUntilAvailable) {
    std::thread producer([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        controller_->releaseCredits(5, 0);
    });
    
    // Exhaust credits first
    for (int i = 0; i < 10; i++) {
        controller_->acquireCredits(100);
    }
    
    auto start = std::chrono::steady_clock::now();
    controller_->waitForCredits(1, 0);
    auto end = std::chrono::steady_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(elapsed.count(), 40); // Should have waited for producer
    
    producer.join();
}

TEST_F(CopyFlowControllerTest, WaitForCreditsWithTimeoutReturnsTrue) {
    bool result = controller_->waitForCreditsWithTimeout(1, 0, 100);
    EXPECT_TRUE(result);
}

TEST_F(CopyFlowControllerTest, WaitForCreditsWithTimeoutReturnsFalse) {
    // Exhaust credits
    for (int i = 0; i < 10; i++) {
        controller_->acquireCredits(100);
    }
    
    bool result = controller_->waitForCreditsWithTimeout(1, 0, 50);
    EXPECT_FALSE(result);
}

// ============================================================================
// CopyFlowControlManager Tests
// ============================================================================

class CopyFlowControlManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing controllers
        CopyFlowControlManager::instance().cleanupExpired();
    }
    
    void TearDown() override {
        // Clean up after tests
        CopyFlowControlManager::instance().cleanupExpired();
    }
};

TEST_F(CopyFlowControlManagerTest, CreateAndGetController) {
    auto controller = CopyFlowControlManager::instance().createController(1, 10, 1024);
    EXPECT_NE(controller, nullptr);
    
    auto retrieved = CopyFlowControlManager::instance().getController(1);
    EXPECT_EQ(retrieved, controller);
}

TEST_F(CopyFlowControlManagerTest, GetNonExistentControllerReturnsNull) {
    auto controller = CopyFlowControlManager::instance().getController(999);
    EXPECT_EQ(controller, nullptr);
}

TEST_F(CopyFlowControlManagerTest, DestroyControllerRemovesIt) {
    auto controller = CopyFlowControlManager::instance().createController(2, 10, 1024);
    EXPECT_NE(CopyFlowControlManager::instance().getController(2), nullptr);
    
    CopyFlowControlManager::instance().destroyController(2);
    EXPECT_EQ(CopyFlowControlManager::instance().getController(2), nullptr);
}

TEST_F(CopyFlowControlManagerTest, PauseAllPausesAllControllers) {
    auto c1 = CopyFlowControlManager::instance().createController(3, 10, 1024);
    auto c2 = CopyFlowControlManager::instance().createController(4, 10, 1024);
    
    CopyFlowControlManager::instance().pauseAll();
    
    EXPECT_TRUE(c1->isPaused());
    EXPECT_TRUE(c2->isPaused());
}

TEST_F(CopyFlowControlManagerTest, ResumeAllResumesAllControllers) {
    auto c1 = CopyFlowControlManager::instance().createController(5, 10, 1024);
    auto c2 = CopyFlowControlManager::instance().createController(6, 10, 1024);
    
    CopyFlowControlManager::instance().pauseAll();
    EXPECT_TRUE(c1->isPaused());
    
    CopyFlowControlManager::instance().resumeAll();
    EXPECT_FALSE(c1->isPaused());
    EXPECT_FALSE(c2->isPaused());
}

TEST_F(CopyFlowControlManagerTest, GetAllStatsReturnsAllControllers) {
    CopyFlowControlManager::instance().createController(7, 10, 1024);
    CopyFlowControlManager::instance().createController(8, 10, 1024);
    
    auto stats = CopyFlowControlManager::instance().getAllStats();
    EXPECT_GE(stats.size(), 2);
}

TEST_F(CopyFlowControlManagerTest, CleanupExpiredRemovesDeadControllers) {
    {
        auto controller = CopyFlowControlManager::instance().createController(9, 10, 1024);
        // controller goes out of scope here, but manager still holds weak_ptr
    }
    
    CopyFlowControlManager::instance().cleanupExpired();
    
    // After cleanup, the expired controller should be removed
    auto stats = CopyFlowControlManager::instance().getAllStats();
    bool found = false;
    for (const auto& s : stats) {
        if (s.session_id == 9) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST_F(CopyFlowControlManagerTest, SingletonReturnsSameInstance) {
    auto& instance1 = CopyFlowControlManager::instance();
    auto& instance2 = CopyFlowControlManager::instance();
    EXPECT_EQ(&instance1, &instance2);
}

// ============================================================================
// Integration/Stress Tests
// ============================================================================

TEST(CopyFlowIntegrationTest, CreditExhaustionScenario) {
    auto controller = std::make_unique<CopyFlowController>(1, 5, 1024);
    
    // Acquire all credits
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(controller->acquireCredits(100));
    }
    
    // Should not be able to send more
    EXPECT_FALSE(controller->canSend(100));
    
    // Release some credits
    controller->releaseCredits(3, 0);
    
    // Should be able to send again
    EXPECT_TRUE(controller->canSend(100));
}

TEST(CopyFlowIntegrationTest, BufferExhaustionScenario) {
    auto controller = std::make_unique<CopyFlowController>(1, 100, 1000);
    
    // Acquire with buffer consumption
    EXPECT_TRUE(controller->acquireCredits(100, 500)); // Use 500 bytes
    
    auto stats = controller->getStats();
    EXPECT_EQ(stats.buffer_available, 500);
    
    // Try to acquire more than remaining buffer
    EXPECT_FALSE(controller->acquireCredits(100, 600));
}

TEST(CopyFlowIntegrationTest, ConcurrentAccess) {
    auto controller = std::make_unique<CopyFlowController>(1, 1000, 1024 * 1024);
    
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    
    // Spawn multiple threads trying to acquire credits
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 10; j++) {
                if (controller->acquireCredits(1)) {
                    success_count++;
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    controller->releaseCredits(1, 0);
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_GT(success_count, 0);
}

TEST(CopyFlowIntegrationTest, ThroughputMeasurement) {
    auto controller = std::make_unique<CopyFlowController>(1, 1000, 1024 * 1024);
    
    auto start = std::chrono::steady_clock::now();
    
    // Simulate data transfer
    for (int i = 0; i < 100; i++) {
        controller->acquireCredits(1, 1024);
        controller->recordReceived(1024);
        controller->releaseCredits(1, 1024);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    auto stats = controller->getStats();
    EXPECT_EQ(stats.total_bytes_received, 100 * 1024);
    
    // Throughput should be available
    EXPECT_GE(stats.throughput_bps, 0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(CopyFlowEdgeCaseTest, ZeroCredits) {
    auto controller = std::make_unique<CopyFlowController>(1, 0, 1024);
    EXPECT_FALSE(controller->canSend(1));
}

TEST(CopyFlowEdgeCaseTest, ZeroBuffer) {
    auto controller = std::make_unique<CopyFlowController>(1, 10, 0);
    EXPECT_FALSE(controller->canSend(1));
}

TEST(CopyFlowEdgeCaseTest, ReleaseZeroCredits) {
    auto controller = std::make_unique<CopyFlowController>(1, 10, 1024);
    auto stats_before = controller->getStats();
    
    controller_->releaseCredits(0, 0);
    
    auto stats_after = controller->getStats();
    EXPECT_EQ(stats_after.credits_available, stats_before.credits_available);
}

TEST(CopyFlowEdgeCaseTest, MultiplePauseResume) {
    auto controller = std::make_unique<CopyFlowController>(1, 10, 1024);
    
    for (int i = 0; i < 5; i++) {
        controller->pause();
        EXPECT_TRUE(controller->isPaused());
        
        controller->resume();
        EXPECT_FALSE(controller->isPaused());
    }
}

TEST(CopyFlowEdgeCaseTest, ResetDuringPause) {
    auto controller = std::make_unique<CopyFlowController>(1, 10, 1024);
    
    controller->pause();
    controller->reset();
    
    EXPECT_FALSE(controller->isPaused());
}
