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
 * Thread Pool
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * High-performance thread pool for handling concurrent client connections.
 * Supports work stealing, priority queues, and graceful shutdown.
 */

#pragma once

#include "scratchbird/network/socket_types.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

#include <memory>
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <chrono>

namespace scratchbird {
namespace network {

// ============================================================================
// Task Types
// ============================================================================

/**
 * Task priority levels
 */
enum class TaskPriority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * Task state
 */
enum class TaskState : uint8_t {
    PENDING = 0,        // Waiting in queue
    RUNNING = 1,        // Currently executing
    COMPLETED = 2,      // Finished successfully
    FAILED = 3,         // Failed with exception
    CANCELLED = 4       // Cancelled before execution
};

/**
 * Task function type
 */
using TaskFunction = std::function<void()>;

/**
 * Task ID
 */
using TaskId = uint64_t;
constexpr TaskId INVALID_TASK_ID = 0;

// ============================================================================
// Task
// ============================================================================

/**
 * Task wrapper with metadata
 */
class Task {
public:
    Task(TaskFunction func, TaskPriority priority = TaskPriority::NORMAL);
    ~Task() = default;

    // Movable
    Task(Task&&) noexcept;
    Task& operator=(Task&&) noexcept;

    // Non-copyable
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    /**
     * Execute the task
     */
    void execute();

    /**
     * Cancel the task (if not yet started)
     */
    bool cancel();

    /**
     * Wait for task completion
     */
    void wait();

    /**
     * Wait with timeout
     */
    bool waitFor(std::chrono::milliseconds timeout);

    TaskId getId() const { return id_; }
    TaskPriority getPriority() const { return priority_; }
    TaskState getState() const { return state_.load(std::memory_order_acquire); }

    /**
     * Generate a new unique task ID
     */
    static TaskId generateId() { return next_id_.fetch_add(1); }

    bool isPending() const { return getState() == TaskState::PENDING; }
    bool isRunning() const { return getState() == TaskState::RUNNING; }
    bool isDone() const {
        auto state = getState();
        return state == TaskState::COMPLETED || state == TaskState::FAILED || state == TaskState::CANCELLED;
    }

    /**
     * Comparison for priority queue (higher priority first)
     */
    bool operator<(const Task& other) const {
        return priority_ < other.priority_;
    }

private:
    TaskId id_;
    TaskFunction func_;
    TaskPriority priority_;
    std::atomic<TaskState> state_{TaskState::PENDING};
    std::promise<void> promise_;
    std::future<void> future_;

    static std::atomic<TaskId> next_id_;
};

// ============================================================================
// Thread Pool Configuration
// ============================================================================

/**
 * Thread pool configuration
 */
struct ThreadPoolConfig {
    uint32_t min_threads = 2;               // Minimum worker threads
    uint32_t max_threads = 0;               // Maximum threads (0 = CPU cores * 2)
    uint32_t max_queue_size = 10000;        // Maximum pending tasks
    std::chrono::milliseconds idle_timeout{60000}; // Idle thread timeout
    bool enable_priority_queue = true;      // Use priority queue
    bool enable_work_stealing = false;      // Work stealing (experimental)
    std::string name_prefix = "worker";     // Thread name prefix
};

// ============================================================================
// Thread Pool Statistics
// ============================================================================

/**
 * Thread pool statistics
 */
struct ThreadPoolStats {
    std::atomic<uint32_t> active_threads{0};    // Currently running threads
    std::atomic<uint32_t> idle_threads{0};      // Idle threads
    std::atomic<uint32_t> pending_tasks{0};     // Tasks waiting in queue
    std::atomic<uint64_t> total_tasks{0};       // Total tasks submitted
    std::atomic<uint64_t> completed_tasks{0};   // Successfully completed tasks
    std::atomic<uint64_t> failed_tasks{0};      // Failed tasks
    std::atomic<uint64_t> rejected_tasks{0};    // Rejected (queue full)
    std::chrono::steady_clock::time_point started_at;

    ThreadPoolStats() : started_at(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// Thread Pool
// ============================================================================

/**
 * Thread Pool
 *
 * Manages a pool of worker threads for executing tasks concurrently.
 * Supports dynamic thread scaling, priority queues, and graceful shutdown.
 *
 * Thread safety: All methods are thread-safe.
 */
class ThreadPool {
public:
    /**
     * Create thread pool with default configuration
     */
    static std::unique_ptr<ThreadPool> create(core::ErrorContext* ctx = nullptr);

    /**
     * Create thread pool with custom configuration
     */
    static std::unique_ptr<ThreadPool> create(const ThreadPoolConfig& config,
                                               core::ErrorContext* ctx = nullptr);

    /**
     * Create thread pool with specified number of threads
     */
    static std::unique_ptr<ThreadPool> create(uint32_t num_threads,
                                               core::ErrorContext* ctx = nullptr);

    virtual ~ThreadPool();

    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // ========================================================================
    // Task Submission
    // ========================================================================

    /**
     * Submit a task for execution
     *
     * @param func Function to execute
     * @param priority Task priority
     * @return Task ID, or INVALID_TASK_ID if queue is full
     */
    TaskId submit(TaskFunction func, TaskPriority priority = TaskPriority::NORMAL);

    /**
     * Submit a task and get a future for the result
     *
     * @tparam F Function type
     * @tparam R Return type
     * @param func Function to execute
     * @param priority Task priority
     * @return Future for the result
     */
    template<typename F>
    auto submitWithFuture(F&& func, TaskPriority priority = TaskPriority::NORMAL)
        -> std::future<typename std::invoke_result<F>::type>
    {
        using ReturnType = typename std::invoke_result<F>::type;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(func));
        std::future<ReturnType> future = task->get_future();

        submit([task]() { (*task)(); }, priority);

        return future;
    }

    /**
     * Schedule a task with delay
     *
     * @param func Function to execute
     * @param delay Delay before execution
     * @param priority Task priority
     * @return Task ID
     */
    TaskId schedule(TaskFunction func, std::chrono::milliseconds delay,
                    TaskPriority priority = TaskPriority::NORMAL);

    /**
     * Schedule a repeating task
     *
     * @param func Function to execute
     * @param interval Interval between executions
     * @param priority Task priority
     * @return Task ID (use to cancel)
     */
    TaskId scheduleRepeating(TaskFunction func, std::chrono::milliseconds interval,
                             TaskPriority priority = TaskPriority::NORMAL);

    /**
     * Cancel a scheduled task
     *
     * @param id Task ID
     * @return true if task was cancelled
     */
    bool cancel(TaskId id);

    // ========================================================================
    // Control
    // ========================================================================

    /**
     * Start the thread pool
     *
     * Starts min_threads workers initially.
     */
    core::Status start(core::ErrorContext* ctx = nullptr);

    /**
     * Stop the thread pool
     *
     * @param wait_for_tasks If true, wait for pending tasks to complete
     * @param timeout Maximum time to wait (0 = indefinite)
     */
    void stop(bool wait_for_tasks = true,
              std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    /**
     * Wait for all current tasks to complete
     */
    void waitAll();

    /**
     * Wait for all tasks with timeout
     */
    bool waitAllFor(std::chrono::milliseconds timeout);

    /**
     * Check if thread pool is running
     */
    bool isRunning() const { return running_.load(std::memory_order_acquire); }

    /**
     * Pause task processing (tasks stay queued)
     */
    void pause();

    /**
     * Resume task processing
     */
    void resume();

    /**
     * Check if paused
     */
    bool isPaused() const { return paused_.load(std::memory_order_acquire); }

    // ========================================================================
    // Thread Management
    // ========================================================================

    /**
     * Set minimum number of threads
     */
    void setMinThreads(uint32_t count);

    /**
     * Set maximum number of threads
     */
    void setMaxThreads(uint32_t count);

    /**
     * Get current number of threads
     */
    uint32_t getThreadCount() const;

    /**
     * Get number of active (non-idle) threads
     */
    uint32_t getActiveThreadCount() const { return stats_.active_threads.load(); }

    /**
     * Get number of idle threads
     */
    uint32_t getIdleThreadCount() const { return stats_.idle_threads.load(); }

    // ========================================================================
    // Queue Management
    // ========================================================================

    /**
     * Get number of pending tasks
     */
    uint32_t getPendingTaskCount() const { return stats_.pending_tasks.load(); }

    /**
     * Check if queue is empty
     */
    bool isEmpty() const { return getPendingTaskCount() == 0; }

    /**
     * Check if queue is full
     */
    bool isFull() const { return getPendingTaskCount() >= config_.max_queue_size; }

    /**
     * Clear all pending tasks
     */
    void clearQueue();

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * Get thread pool statistics
     */
    const ThreadPoolStats& getStats() const { return stats_; }

    /**
     * Get configuration
     */
    const ThreadPoolConfig& getConfig() const { return config_; }

private:
    explicit ThreadPool(const ThreadPoolConfig& config);

    // Worker thread function
    void workerLoop(uint32_t thread_id);

    // Get next task from queue
    std::unique_ptr<Task> getNextTask();

    // Add worker thread
    void addWorker();

    // Remove idle worker thread
    void removeIdleWorker();

    // Adjust thread count based on load
    void adjustThreadCount();

    ThreadPoolConfig config_;
    ThreadPoolStats stats_;

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> stopping_{false};

    // Worker threads
    std::vector<std::thread> workers_;
    mutable std::mutex workers_mutex_;

    // Task queue comparator (lower priority tasks are greater to get min-heap behavior)
    struct TaskComparator {
        bool operator()(const std::unique_ptr<Task>& a, const std::unique_ptr<Task>& b) const {
            return a->getPriority() < b->getPriority();  // Higher priority first
        }
    };

    // Task queue
    std::priority_queue<std::unique_ptr<Task>,
                        std::vector<std::unique_ptr<Task>>,
                        TaskComparator> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Scheduled tasks
    struct ScheduledTask {
        TaskId id;
        std::chrono::steady_clock::time_point execute_at;
        std::chrono::milliseconds interval;  // 0 = one-shot
        TaskFunction func;
        TaskPriority priority;
        bool cancelled = false;

        bool operator>(const ScheduledTask& other) const {
            return execute_at > other.execute_at;
        }
    };

    std::priority_queue<ScheduledTask,
                        std::vector<ScheduledTask>,
                        std::greater<ScheduledTask>> scheduled_tasks_;
    mutable std::mutex scheduled_mutex_;
    std::condition_variable scheduled_cv_;
    std::thread scheduler_thread_;

    void schedulerLoop();
};

// ============================================================================
// Global Thread Pool
// ============================================================================

/**
 * Get the global thread pool instance
 *
 * Creates a thread pool with default configuration on first call.
 */
ThreadPool& getGlobalThreadPool();

/**
 * Set the global thread pool instance
 *
 * Must be called before first use of getGlobalThreadPool().
 */
void setGlobalThreadPool(std::unique_ptr<ThreadPool> pool);

} // namespace network
} // namespace scratchbird
