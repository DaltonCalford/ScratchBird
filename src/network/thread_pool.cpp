/**
 * Thread Pool Implementation
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * High-performance thread pool for handling concurrent client connections.
 */

#include "scratchbird/network/thread_pool.h"

#include <algorithm>
#include <cstring>

#ifdef __linux__
    #include <pthread.h>
#endif

namespace scratchbird {
namespace network {

// ============================================================================
// Task Implementation
// ============================================================================

std::atomic<TaskId> Task::next_id_{1};

Task::Task(TaskFunction func, TaskPriority priority)
    : id_(next_id_.fetch_add(1)), func_(std::move(func)), priority_(priority),
      future_(promise_.get_future()) {}

Task::Task(Task&& other) noexcept
    : id_(other.id_), func_(std::move(other.func_)), priority_(other.priority_),
      state_(other.state_.load()), promise_(std::move(other.promise_)),
      future_(std::move(other.future_)) {}

Task& Task::operator=(Task&& other) noexcept {
    if (this != &other) {
        id_ = other.id_;
        func_ = std::move(other.func_);
        priority_ = other.priority_;
        state_.store(other.state_.load());
        promise_ = std::move(other.promise_);
        future_ = std::move(other.future_);
    }
    return *this;
}

void Task::execute() {
    TaskState expected = TaskState::PENDING;
    if (!state_.compare_exchange_strong(expected, TaskState::RUNNING)) {
        return;  // Already cancelled or running
    }

    try {
        if (func_) {
            func_();
        }
        state_.store(TaskState::COMPLETED, std::memory_order_release);
        promise_.set_value();
    } catch (...) {
        state_.store(TaskState::FAILED, std::memory_order_release);
        try {
            promise_.set_exception(std::current_exception());
        } catch (...) {
            // Ignore - promise may have already been satisfied
        }
    }
}

bool Task::cancel() {
    TaskState expected = TaskState::PENDING;
    if (state_.compare_exchange_strong(expected, TaskState::CANCELLED)) {
        promise_.set_value();  // Unblock waiters
        return true;
    }
    return false;
}

void Task::wait() {
    future_.wait();
}

bool Task::waitFor(std::chrono::milliseconds timeout) {
    return future_.wait_for(timeout) == std::future_status::ready;
}

// ============================================================================
// Thread Pool Implementation
// ============================================================================

ThreadPool::ThreadPool(const ThreadPoolConfig& config) : config_(config) {
    // Set defaults
    if (config_.max_threads == 0) {
        config_.max_threads = std::max(1u, std::thread::hardware_concurrency() * 2);
    }
    if (config_.min_threads > config_.max_threads) {
        config_.min_threads = config_.max_threads;
    }
}

ThreadPool::~ThreadPool() {
    stop(false);
}

std::unique_ptr<ThreadPool> ThreadPool::create(core::ErrorContext* /*ctx*/) {
    return create(ThreadPoolConfig());
}

std::unique_ptr<ThreadPool> ThreadPool::create(const ThreadPoolConfig& config,
                                                core::ErrorContext* /*ctx*/) {
    auto pool = std::unique_ptr<ThreadPool>(new ThreadPool(config));
    return pool;
}

std::unique_ptr<ThreadPool> ThreadPool::create(uint32_t num_threads,
                                                core::ErrorContext* ctx) {
    ThreadPoolConfig config;
    config.min_threads = num_threads;
    config.max_threads = num_threads;
    return create(config, ctx);
}

core::Status ThreadPool::start(core::ErrorContext* /*ctx*/) {
    if (running_.load()) {
        return core::Status::OK;  // Already running
    }

    running_.store(true, std::memory_order_release);
    stopping_.store(false, std::memory_order_release);

    // Start minimum number of workers
    std::lock_guard<std::mutex> lock(workers_mutex_);
    for (uint32_t i = 0; i < config_.min_threads; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this, static_cast<uint32_t>(workers_.size()));
    }

    // Start scheduler thread for delayed tasks
    scheduler_thread_ = std::thread(&ThreadPool::schedulerLoop, this);

    return core::Status::OK;
}

void ThreadPool::stop(bool wait_for_tasks, std::chrono::milliseconds timeout) {
    if (!running_.load()) {
        return;
    }

    stopping_.store(true, std::memory_order_release);

    if (wait_for_tasks) {
        // Wait for tasks to complete
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (stats_.pending_tasks.load() > 0) {
            if (timeout.count() > 0 && std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    running_.store(false, std::memory_order_release);

    // Wake up all workers
    queue_cv_.notify_all();
    scheduled_cv_.notify_all();

    // Join worker threads
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    // Join scheduler thread
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }

    // Clear remaining tasks
    clearQueue();
}

TaskId ThreadPool::submit(TaskFunction func, TaskPriority priority) {
    if (!running_.load() || stopping_.load()) {
        stats_.rejected_tasks.fetch_add(1);
        return INVALID_TASK_ID;
    }

    // Check queue size
    if (stats_.pending_tasks.load() >= config_.max_queue_size) {
        stats_.rejected_tasks.fetch_add(1);
        return INVALID_TASK_ID;
    }

    auto task = std::make_unique<Task>(std::move(func), priority);
    TaskId id = task->getId();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
        stats_.pending_tasks.fetch_add(1);
        stats_.total_tasks.fetch_add(1);
    }

    queue_cv_.notify_one();

    // Consider adding more workers if queue is growing
    adjustThreadCount();

    return id;
}

TaskId ThreadPool::schedule(TaskFunction func, std::chrono::milliseconds delay,
                            TaskPriority priority) {
    if (!running_.load() || stopping_.load()) {
        return INVALID_TASK_ID;
    }

    ScheduledTask st;
    st.id = Task::generateId();
    st.execute_at = std::chrono::steady_clock::now() + delay;
    st.interval = std::chrono::milliseconds::zero();
    st.func = std::move(func);
    st.priority = priority;

    {
        std::lock_guard<std::mutex> lock(scheduled_mutex_);
        scheduled_tasks_.push(std::move(st));
    }

    scheduled_cv_.notify_one();

    return st.id;
}

TaskId ThreadPool::scheduleRepeating(TaskFunction func, std::chrono::milliseconds interval,
                                     TaskPriority priority) {
    if (!running_.load() || stopping_.load()) {
        return INVALID_TASK_ID;
    }

    ScheduledTask st;
    st.id = Task::generateId();
    st.execute_at = std::chrono::steady_clock::now() + interval;
    st.interval = interval;
    st.func = std::move(func);
    st.priority = priority;

    {
        std::lock_guard<std::mutex> lock(scheduled_mutex_);
        scheduled_tasks_.push(std::move(st));
    }

    scheduled_cv_.notify_one();

    return st.id;
}

bool ThreadPool::cancel(TaskId /*id*/) {
    // Note: Cancellation is complex with priority queues
    // For now, mark as cancelled in scheduled tasks
    std::lock_guard<std::mutex> lock(scheduled_mutex_);

    std::vector<ScheduledTask> temp;
    bool found = false;

    while (!scheduled_tasks_.empty()) {
        auto st = std::move(const_cast<ScheduledTask&>(scheduled_tasks_.top()));
        scheduled_tasks_.pop();
        // Can't cancel by ID in this implementation without major refactoring
        temp.push_back(std::move(st));
    }

    for (auto& t : temp) {
        scheduled_tasks_.push(std::move(t));
    }

    return found;
}

void ThreadPool::waitAll() {
    while (stats_.pending_tasks.load() > 0 || stats_.active_threads.load() > stats_.idle_threads.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool ThreadPool::waitAllFor(std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (stats_.pending_tasks.load() > 0 || stats_.active_threads.load() > stats_.idle_threads.load()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

void ThreadPool::pause() {
    paused_.store(true, std::memory_order_release);
}

void ThreadPool::resume() {
    paused_.store(false, std::memory_order_release);
    queue_cv_.notify_all();
}

void ThreadPool::setMinThreads(uint32_t count) {
    config_.min_threads = count;
    adjustThreadCount();
}

void ThreadPool::setMaxThreads(uint32_t count) {
    config_.max_threads = count;
    adjustThreadCount();
}

uint32_t ThreadPool::getThreadCount() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    return static_cast<uint32_t>(workers_.size());
}

void ThreadPool::clearQueue() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!task_queue_.empty()) {
        task_queue_.pop();
    }
    stats_.pending_tasks.store(0);
}

void ThreadPool::workerLoop(uint32_t thread_id) {
    // Set thread name for debugging
#ifdef __linux__
    std::string name = config_.name_prefix + "-" + std::to_string(thread_id);
    pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#endif

    stats_.active_threads.fetch_add(1);

    while (running_.load(std::memory_order_acquire)) {
        auto task = getNextTask();

        if (!task) {
            // No task available
            if (stopping_.load()) {
                break;
            }
            continue;
        }

        stats_.idle_threads.fetch_sub(1);

        task->execute();

        if (task->getState() == TaskState::COMPLETED) {
            stats_.completed_tasks.fetch_add(1);
        } else if (task->getState() == TaskState::FAILED) {
            stats_.failed_tasks.fetch_add(1);
        }

        stats_.idle_threads.fetch_add(1);
    }

    stats_.active_threads.fetch_sub(1);
}

std::unique_ptr<Task> ThreadPool::getNextTask() {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    stats_.idle_threads.fetch_add(1);

    // Wait for task or stop signal
    auto timeout = config_.idle_timeout;
    bool got_task = queue_cv_.wait_for(lock, timeout, [this] {
        return !task_queue_.empty() ||
               !running_.load(std::memory_order_acquire) ||
               stopping_.load(std::memory_order_acquire);
    });

    stats_.idle_threads.fetch_sub(1);

    if (!got_task || task_queue_.empty()) {
        return nullptr;
    }

    // Check if paused
    while (paused_.load(std::memory_order_acquire) && running_.load(std::memory_order_acquire)) {
        queue_cv_.wait(lock);
    }

    if (task_queue_.empty()) {
        return nullptr;
    }

    auto task = std::move(const_cast<std::unique_ptr<Task>&>(task_queue_.top()));
    task_queue_.pop();
    stats_.pending_tasks.fetch_sub(1);

    return task;
}

void ThreadPool::addWorker() {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    if (workers_.size() < config_.max_threads) {
        workers_.emplace_back(&ThreadPool::workerLoop, this, static_cast<uint32_t>(workers_.size()));
    }
}

void ThreadPool::adjustThreadCount() {
    uint32_t pending = stats_.pending_tasks.load();
    uint32_t idle = stats_.idle_threads.load();
    uint32_t current = getThreadCount();

    // Add workers if queue is building up and we have few idle workers
    if (pending > 0 && idle == 0 && current < config_.max_threads) {
        addWorker();
    }
}

void ThreadPool::schedulerLoop() {
    while (running_.load(std::memory_order_acquire)) {
        std::unique_lock<std::mutex> lock(scheduled_mutex_);

        if (scheduled_tasks_.empty()) {
            scheduled_cv_.wait_for(lock, std::chrono::milliseconds(100));
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto& top = scheduled_tasks_.top();

        if (top.execute_at > now) {
            // Wait until next task is due
            auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                top.execute_at - now);
            scheduled_cv_.wait_for(lock, wait_time);
            continue;
        }

        // Get the task to execute
        auto st = std::move(const_cast<ScheduledTask&>(scheduled_tasks_.top()));
        scheduled_tasks_.pop();

        if (st.cancelled) {
            continue;
        }

        // Reschedule if repeating
        if (st.interval.count() > 0) {
            ScheduledTask repeat = st;
            repeat.execute_at = now + st.interval;
            scheduled_tasks_.push(std::move(repeat));
        }

        lock.unlock();

        // Submit to regular queue
        submit(std::move(st.func), st.priority);
    }
}

// ============================================================================
// Global Thread Pool
// ============================================================================

namespace {
    std::unique_ptr<ThreadPool> g_global_pool;
    std::once_flag g_global_pool_init;
}

ThreadPool& getGlobalThreadPool() {
    std::call_once(g_global_pool_init, []() {
        g_global_pool = ThreadPool::create();
        g_global_pool->start();
    });
    return *g_global_pool;
}

void setGlobalThreadPool(std::unique_ptr<ThreadPool> pool) {
    g_global_pool = std::move(pool);
}

} // namespace network
} // namespace scratchbird
