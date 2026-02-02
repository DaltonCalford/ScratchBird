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
 * LSM Thread Pool Implementation
 *
 * Implements parallel compaction using a worker thread pool with:
 * - Task queue with priority scheduling
 * - Range overlap detection (prevents data corruption)
 * - Graceful shutdown
 * - Thread-safe coordination
 *
 * November 22, 2025
 */

#include "scratchbird/core/lsm_thread_pool.h"
#include "scratchbird/core/lsm_tree_index.h"
#include <algorithm>
#include <queue>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Task Wrapper (Private Implementation)
// ============================================================================

/**
 * Task wrapper with priority and range tracking
 * (file-local struct for implementation)
 */
struct TaskWrapper
{
    CompactionTask task;
    LSMThreadPool::TaskFunction task_fn;
    int priority;  // Higher priority = executed first
};

/**
 * Comparator for priority queue (min-heap: lower priority value = executed later)
 */
struct TaskWrapperCompare
{
    bool operator()(const TaskWrapper& a, const TaskWrapper& b) const
    {
        // Priority queue is max-heap by default, so invert for min-heap behavior
        // Higher priority value = executed first
        return a.priority < b.priority;
    }
};

// Type alias for the priority queue
using TaskQueue = std::priority_queue<TaskWrapper,
                                      std::vector<TaskWrapper>,
                                      TaskWrapperCompare>;

// Helper to get typed pointer
static TaskQueue* getTaskQueue(void* ptr)
{
    return static_cast<TaskQueue*>(ptr);
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

LSMThreadPool::LSMThreadPool(size_t num_threads)
    : num_threads_(num_threads),
      task_queue_ptr_(nullptr),
      stop_flag_(false),
      started_(false),
      tasks_completed_(0),
      tasks_failed_(0)
{
    // Default: use hardware_concurrency - 1 (leave one core for foreground work)
    if (num_threads_ == 0)
    {
        num_threads_ = std::thread::hardware_concurrency();
        if (num_threads_ > 1)
        {
            num_threads_ -= 1;
        }
        else
        {
            num_threads_ = 1;  // At least one thread
        }
    }

    // Create priority queue
    task_queue_ptr_ = new TaskQueue();
}

LSMThreadPool::~LSMThreadPool()
{
    stop(true);  // Wait for completion

    // Delete priority queue
    if (task_queue_ptr_)
    {
        delete getTaskQueue(task_queue_ptr_);
        task_queue_ptr_ = nullptr;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void LSMThreadPool::start()
{
    if (started_.exchange(true))
    {
        return;  // Already started
    }

    stop_flag_.store(false);

    // Create worker threads
    for (size_t i = 0; i < num_threads_; i++)
    {
        workers_.emplace_back(&LSMThreadPool::workerLoop, this);
    }
}

void LSMThreadPool::stop(bool wait_for_completion)
{
    if (!started_.load())
    {
        return;  // Not started
    }

    // Signal stop
    stop_flag_.store(true);

    if (!wait_for_completion)
    {
        // Clear pending tasks
        std::lock_guard<std::mutex> lock(queue_mutex_);
        TaskQueue* queue = getTaskQueue(task_queue_ptr_);
        while (!queue->empty())
        {
            queue->pop();
        }
    }

    // Wake up all workers
    queue_cv_.notify_all();

    // Join all worker threads
    for (auto& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    workers_.clear();
    started_.store(false);
}

// ============================================================================
// Task Submission
// ============================================================================

bool LSMThreadPool::submit(const CompactionTask& task, TaskFunction task_fn)
{
    if (stop_flag_.load())
    {
        return false;  // Pool is shutting down
    }

    // Determine priority (Level 0 compactions have higher priority)
    int priority = 100 - task.source_level;  // Level 0 = priority 100, Level 3 = priority 97

    // Create task wrapper
    TaskWrapper wrapper;
    wrapper.task = task;
    wrapper.task_fn = task_fn;
    wrapper.priority = priority;

    // Add to queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        TaskQueue* queue = getTaskQueue(task_queue_ptr_);
        queue->push(wrapper);
    }

    // Wake up a worker
    queue_cv_.notify_one();

    return true;
}

// ============================================================================
// Statistics
// ============================================================================

size_t LSMThreadPool::getPendingTaskCount() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    TaskQueue* queue = getTaskQueue(const_cast<void*>(task_queue_ptr_));
    return queue->size();
}

size_t LSMThreadPool::getActiveTaskCount() const
{
    std::lock_guard<std::mutex> lock(ranges_mutex_);
    return active_ranges_.size();
}

bool LSMThreadPool::isRangeBeingCompacted(const std::vector<uint8_t>& min_key,
                                         const std::vector<uint8_t>& max_key) const
{
    std::lock_guard<std::mutex> lock(ranges_mutex_);

    for (const auto& range : active_ranges_)
    {
        if (rangesOverlap(min_key, max_key, range.min_key, range.max_key))
        {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Worker Thread
// ============================================================================

void LSMThreadPool::workerLoop()
{
    while (!stop_flag_.load())
    {
        TaskWrapper wrapper;

        // Get next task
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            TaskQueue* queue = getTaskQueue(task_queue_ptr_);

            // Wait for task or stop signal
            queue_cv_.wait(lock, [this, queue] {
                return !queue->empty() || stop_flag_.load();
            });

            if (stop_flag_.load() && queue->empty())
            {
                break;  // Shutdown
            }

            if (queue->empty())
            {
                continue;  // Spurious wakeup
            }

            // Get highest priority task
            wrapper = queue->top();
            queue->pop();
        }

        // Check for range overlap before executing
        // (This prevents data corruption from concurrent overlapping compactions)
        bool can_execute = false;
        std::vector<uint8_t> min_key, max_key;

        // Extract min/max keys from task.source_sstables
        // Open each SSTable and get its key range
        if (!wrapper.task.source_sstables.empty())
        {
            bool first = true;
            for (const auto& sstable_path : wrapper.task.source_sstables)
            {
                // Open SSTable reader to get min/max keys
                SSTableReader reader(sstable_path);
                ErrorContext ctx;
                Status s = reader.open(&ctx);
                if (s != Status::OK)
                {
                    // Cannot read SSTable - skip this task for now
                    // Re-queue for retry
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    TaskQueue* queue = getTaskQueue(task_queue_ptr_);
                    queue->push(wrapper);
                    continue;
                }

                std::vector<uint8_t> sstable_min = reader.getMinKey();
                std::vector<uint8_t> sstable_max = reader.getMaxKey();
                reader.close(&ctx);

                // Expand overall range
                if (first)
                {
                    min_key = sstable_min;
                    max_key = sstable_max;
                    first = false;
                }
                else
                {
                    if (sstable_min < min_key) min_key = sstable_min;
                    if (sstable_max > max_key) max_key = sstable_max;
                }
            }
        }

        // Check for overlapping active ranges
        if (!min_key.empty() && !max_key.empty())
        {
            std::lock_guard<std::mutex> lock(ranges_mutex_);
            can_execute = true;
            for (const auto& range : active_ranges_)
            {
                if (rangesOverlap(min_key, max_key, range.min_key, range.max_key))
                {
                    can_execute = false;
                    break;
                }
            }
        }
        else
        {
            // No key range - safe to execute
            can_execute = true;
        }

        if (!can_execute)
        {
            // Re-queue task (another worker may be able to execute it later)
            std::lock_guard<std::mutex> lock(queue_mutex_);
            TaskQueue* queue = getTaskQueue(task_queue_ptr_);
            queue->push(wrapper);
            continue;
        }

        // Register active range
        if (!min_key.empty() && !max_key.empty())
        {
            registerActiveRange(min_key, max_key);
        }

        // Execute task
        bool success = false;
        try
        {
            success = wrapper.task_fn(wrapper.task);
        }
        catch (...)
        {
            success = false;
        }

        // Unregister active range
        if (!min_key.empty() && !max_key.empty())
        {
            unregisterActiveRange(min_key, max_key);
        }

        // Update statistics
        if (success)
        {
            tasks_completed_.fetch_add(1);
        }
        else
        {
            tasks_failed_.fetch_add(1);
        }
    }
}

// ============================================================================
// Range Management (Private)
// ============================================================================

bool LSMThreadPool::rangesOverlap(const std::vector<uint8_t>& min1,
                                 const std::vector<uint8_t>& max1,
                                 const std::vector<uint8_t>& min2,
                                 const std::vector<uint8_t>& max2)
{
    // Two ranges [min1, max1] and [min2, max2] overlap if:
    // NOT (max1 < min2 OR max2 < min1)
    // Equivalent to: min1 <= max2 AND min2 <= max1

    bool max1_lt_min2 = (max1 < min2);
    bool max2_lt_min1 = (max2 < min1);

    return !max1_lt_min2 && !max2_lt_min1;
}

void LSMThreadPool::registerActiveRange(const std::vector<uint8_t>& min_key,
                                       const std::vector<uint8_t>& max_key)
{
    std::lock_guard<std::mutex> lock(ranges_mutex_);

    ActiveRange range;
    range.min_key = min_key;
    range.max_key = max_key;
    range.thread_id = std::this_thread::get_id();

    active_ranges_.push_back(range);
}

void LSMThreadPool::unregisterActiveRange(const std::vector<uint8_t>& min_key,
                                         const std::vector<uint8_t>& max_key)
{
    std::lock_guard<std::mutex> lock(ranges_mutex_);

    // Find and remove matching range
    auto it = std::find_if(active_ranges_.begin(), active_ranges_.end(),
        [&](const ActiveRange& r) {
            return r.min_key == min_key && r.max_key == max_key &&
                   r.thread_id == std::this_thread::get_id();
        });

    if (it != active_ranges_.end())
    {
        active_ranges_.erase(it);
    }
}

} // namespace core
} // namespace scratchbird
