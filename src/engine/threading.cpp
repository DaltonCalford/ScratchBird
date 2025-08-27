#include "scratchbird/engine/threading.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/network_server.h"
#include "scratchbird/engine/protocol_handler.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <unordered_set>

namespace scratchbird::engine
{

    //=============================================================================
    // WorkItem Implementation
    //=============================================================================

    std::atomic<std::uint64_t> WorkItem::next_work_id_{1};

    WorkItem::WorkItem(WorkPriority priority)
        : priority_(priority), work_id_(next_work_id_++), executed_(false)
    {
        auto now = std::chrono::system_clock::now();
        created_time_ =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        execution_time_ = 0;
    }

    void WorkItem::mark_executed()
    {
        auto now = std::chrono::system_clock::now();
        execution_time_ =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        executed_ = true;
    }

    //=============================================================================
    // ConnectionWorkItem Implementation
    //=============================================================================

    ConnectionWorkItem::ConnectionWorkItem(std::unique_ptr<TcpConnection> connection,
                                           CatalogManager* catalog, WorkPriority priority)
        : WorkItem(priority), connection_(std::move(connection)), catalog_(catalog)
    {
    }

    void ConnectionWorkItem::execute()
    {
        if (!connection_ || !catalog_) {
            mark_executed();
            return;
        }

        try {
            // Create protocol handler manager for this connection
            protocol_manager_ =
                std::make_unique<ProtocolHandlerManager>(connection_.get(), catalog_);

            if (!protocol_manager_->initialize()) {
                std::cerr << "Failed to initialize protocol handler for connection" << std::endl;
                mark_executed();
                return;
            }

            // Simple connection handling loop
            std::vector<std::uint8_t> buffer(8192);
            while (connection_->is_connected()) {
                if (connection_->receive_data(buffer, buffer.size())) {
                    if (!buffer.empty()) {
                        auto result = protocol_manager_->process_incoming_data(buffer);
                        if (result == ProtocolResult::ConnectionClosed ||
                            result == ProtocolResult::ProtocolError) {
                            break;
                        }
                        buffer.clear();
                    }
                } else {
                    break; // Connection error or closed
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "Connection work item error: " << e.what() << std::endl;
        }

        mark_executed();
    }

    std::string ConnectionWorkItem::get_description() const
    {
        if (connection_) {
            return "Connection handler for " + connection_->get_peer_address();
        }
        return "Connection handler (invalid connection)";
    }

    //=============================================================================
    // FunctionWorkItem Implementation
    //=============================================================================

    FunctionWorkItem::FunctionWorkItem(WorkFunction func, const std::string& description,
                                       WorkPriority priority)
        : WorkItem(priority), function_(std::move(func)), description_(description)
    {
    }

    void FunctionWorkItem::execute()
    {
        if (function_) {
            try {
                function_();
            } catch (const std::exception& e) {
                std::cerr << "Function work item error: " << e.what() << std::endl;
            }
        }
        mark_executed();
    }

    std::string FunctionWorkItem::get_description() const
    {
        return description_;
    }

    //=============================================================================
    // WorkerThread Implementation
    //=============================================================================

    WorkerThread::WorkerThread(const std::string& name, std::uint32_t thread_id)
        : thread_name_(name), thread_id_(thread_id), running_(false), stop_requested_(false),
          work_items_processed_(0), total_execution_time_ms_(0), last_activity_time_(0)
    {
    }

    WorkerThread::~WorkerThread()
    {
        stop();
        join();
    }

    bool WorkerThread::start()
    {
        if (running_.load()) {
            return true;
        }

        stop_requested_ = false;
        worker_thread_ = std::make_unique<std::thread>(&WorkerThread::worker_thread_main, this);
        running_ = true;

        return true;
    }

    void WorkerThread::stop()
    {
        stop_requested_ = true;
        work_available_.notify_all();
    }

    void WorkerThread::join()
    {
        if (worker_thread_ && worker_thread_->joinable()) {
            worker_thread_->join();
        }
        running_ = false;
    }

    void WorkerThread::enqueue_work(std::unique_ptr<WorkItem> work)
    {
        if (!work || stop_requested_.load()) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(work_queue_mutex_);
            work_queue_.push(std::move(work));
        }
        work_available_.notify_one();
    }

    bool WorkerThread::try_dequeue_work(std::unique_ptr<WorkItem>& work, std::uint32_t timeout_ms)
    {
        std::unique_lock<std::mutex> lock(work_queue_mutex_);

        if (timeout_ms == 0) {
            if (work_queue_.empty()) {
                return false;
            }
        } else {
            if (!work_available_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
                    return !work_queue_.empty() || stop_requested_.load();
                })) {
                return false;
            }
        }

        if (work_queue_.empty()) {
            return false;
        }

        work = std::move(work_queue_.front());
        work_queue_.pop();
        return true;
    }

    std::size_t WorkerThread::get_queue_size() const
    {
        std::lock_guard<std::mutex> lock(work_queue_mutex_);
        return work_queue_.size();
    }

    WorkerThreadStats WorkerThread::get_stats() const
    {
        WorkerThreadStats stats;
        stats.thread_id = thread_id_;
        stats.thread_name = thread_name_;
        stats.work_items_processed = work_items_processed_.load();
        stats.total_execution_time_ms = total_execution_time_ms_.load();
        stats.last_activity_time = last_activity_time_.load();
        stats.is_active = running_.load();
        stats.queue_size = static_cast<std::uint32_t>(get_queue_size());

        if (stats.work_items_processed > 0) {
            stats.average_execution_time_ms =
                stats.total_execution_time_ms / stats.work_items_processed;
        }

        return stats;
    }

    void WorkerThread::worker_thread_main()
    {
        update_activity_time();

        while (!stop_requested_.load()) {
            std::unique_ptr<WorkItem> work;

            if (try_dequeue_work(work, 1000)) { // 1 second timeout
                if (work) {
                    update_activity_time();

                    auto start_time = get_current_time_ms();
                    work->execute();
                    auto end_time = get_current_time_ms();

                    work_items_processed_++;
                    total_execution_time_ms_ += (end_time - start_time);

                    update_activity_time();
                }
            }
        }
    }

    void WorkerThread::update_activity_time()
    {
        last_activity_time_ = get_current_time_ms();
    }

    std::int64_t WorkerThread::get_current_time_ms() const
    {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
            .count();
    }

    //=============================================================================
    // ThreadPool Implementation
    //=============================================================================

    ThreadPool::ThreadPool(std::uint32_t num_threads)
        : pool_size_(num_threads > 0 ? num_threads : 1), running_(false),
          shutdown_requested_(false), total_submitted_(0), total_processed_(0), total_rejected_(0),
          next_worker_(0)
    {
    }

    ThreadPool::~ThreadPool()
    {
        stop();
    }

    bool ThreadPool::start()
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        if (running_.load()) {
            return true;
        }

        if (!create_workers()) {
            return false;
        }

        running_ = true;
        return true;
    }

    void ThreadPool::stop()
    {
        shutdown_requested_ = true;
        destroy_workers();
        running_ = false;
    }

    void ThreadPool::shutdown_gracefully()
    {
        // Allow current work to complete
        while (true) {
            bool all_idle = true;
            for (const auto& worker : workers_) {
                if (worker->get_queue_size() > 0) {
                    all_idle = false;
                    break;
                }
            }
            if (all_idle)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        stop();
    }

    bool ThreadPool::submit_work(std::unique_ptr<WorkItem> work)
    {
        return submit_work_with_timeout(std::move(work), 0);
    }

    bool ThreadPool::submit_work_with_timeout(std::unique_ptr<WorkItem> work,
                                              std::uint32_t /*timeout_ms*/)
    {
        if (!work || shutdown_requested_.load() || !running_.load()) {
            total_rejected_++;
            return false;
        }

        std::lock_guard<std::mutex> lock(pool_mutex_);

        if (workers_.empty()) {
            total_rejected_++;
            return false;
        }

        // Simple round-robin distribution
        std::uint32_t worker_index = next_worker_.fetch_add(1) % workers_.size();
        workers_[worker_index]->enqueue_work(std::move(work));

        total_submitted_++;
        return true;
    }

    bool ThreadPool::resize_pool(std::uint32_t new_size)
    {
        if (new_size == 0) {
            return false;
        }

        std::lock_guard<std::mutex> lock(pool_mutex_);

        if (new_size == pool_size_) {
            return true;
        }

        // For simplicity, we'll stop and restart with new size
        bool was_running = running_.load();
        if (was_running) {
            destroy_workers();
        }

        pool_size_ = new_size;

        if (was_running) {
            return create_workers();
        }

        return true;
    }

    ThreadPoolStats ThreadPool::get_stats() const
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        ThreadPoolStats stats;
        stats.total_threads = static_cast<std::uint32_t>(workers_.size());
        stats.total_work_items_processed = 0;
        stats.pending_work_items = 0;
        stats.active_threads = 0;
        stats.idle_threads = 0;

        for (const auto& worker : workers_) {
            auto worker_stats = worker->get_stats();
            stats.total_work_items_processed += worker_stats.work_items_processed;
            stats.pending_work_items += worker_stats.queue_size;

            if (worker_stats.is_active && worker_stats.queue_size > 0) {
                stats.active_threads++;
            } else {
                stats.idle_threads++;
            }
        }

        stats.rejected_work_items = total_rejected_.load();

        if (stats.total_work_items_processed > 0) {
            // Calculate averages (simplified)
            std::uint64_t total_time = 0;
            for (const auto& worker : workers_) {
                total_time += worker->get_stats().total_execution_time_ms;
            }
            stats.average_execution_time_ms = total_time / stats.total_work_items_processed;
        }

        return stats;
    }

    std::vector<WorkerThreadStats> ThreadPool::get_worker_stats() const
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        std::vector<WorkerThreadStats> stats;
        for (const auto& worker : workers_) {
            stats.push_back(worker->get_stats());
        }

        return stats;
    }

    void ThreadPool::reset_stats()
    {
        total_submitted_ = 0;
        total_processed_ = 0;
        total_rejected_ = 0;
    }

    std::uint32_t ThreadPool::get_least_loaded_worker() const
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        if (workers_.empty()) {
            return 0;
        }

        std::uint32_t least_loaded = 0;
        std::size_t min_queue_size = workers_[0]->get_queue_size();

        for (std::uint32_t i = 1; i < workers_.size(); ++i) {
            std::size_t queue_size = workers_[i]->get_queue_size();
            if (queue_size < min_queue_size) {
                min_queue_size = queue_size;
                least_loaded = i;
            }
        }

        return least_loaded;
    }

    void ThreadPool::distribute_work_evenly()
    {
        // This is a placeholder for advanced load balancing
        // In a full implementation, this would redistribute pending work
        // across workers to balance the load
    }

    bool ThreadPool::create_workers()
    {
        workers_.clear();
        workers_.reserve(pool_size_);

        for (std::uint32_t i = 0; i < pool_size_; ++i) {
            std::string name = "Worker-" + std::to_string(i);
            auto worker = std::make_unique<WorkerThread>(name, i);

            if (!worker->start()) {
                return false;
            }

            workers_.push_back(std::move(worker));
        }

        return true;
    }

    void ThreadPool::destroy_workers()
    {
        for (auto& worker : workers_) {
            if (worker) {
                worker->stop();
                worker->join();
            }
        }
        workers_.clear();
    }

    //=============================================================================
    // ThreadPerConnectionManager Implementation
    //=============================================================================

    ThreadPerConnectionManager::ThreadPerConnectionManager(std::uint32_t max_connections)
        : max_connections_(max_connections), active_connections_(0), total_handled_(0),
          total_rejected_(0)
    {
    }

    ThreadPerConnectionManager::~ThreadPerConnectionManager()
    {
        cleanup_finished_connections();

        // Wait for all connections to finish
        while (active_connections_.load() > 0) {
            cleanup_finished_connections();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool ThreadPerConnectionManager::handle_connection(std::unique_ptr<TcpConnection> connection,
                                                       CatalogManager* catalog)
    {
        if (!connection || !catalog) {
            return false;
        }

        if (is_connection_limit_reached()) {
            total_rejected_++;
            return false;
        }

        cleanup_finished_connections();

        auto start_time = std::chrono::system_clock::now().time_since_epoch().count();

        std::thread connection_thread(&ThreadPerConnectionManager::connection_handler_thread, this,
                                      std::move(connection), catalog, start_time);

        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connection_threads_.emplace_back(std::move(connection_thread), start_time);
        }

        active_connections_++;
        total_handled_++;

        return true;
    }

    void ThreadPerConnectionManager::cleanup_finished_connections()
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);

        auto it = connection_threads_.begin();
        while (it != connection_threads_.end()) {
            if (it->first.joinable()) {
                // Try to join with no wait (non-blocking check)
                // In a full implementation, we'd use a different approach to check if thread is
                // done
                ++it;
            } else {
                it = connection_threads_.erase(it);
                active_connections_--;
            }
        }
    }

    ThreadPerConnectionManager::ConnectionStats ThreadPerConnectionManager::get_stats() const
    {
        ConnectionStats stats;
        stats.active_connections = active_connections_.load();
        stats.total_connections_handled = total_handled_.load();
        stats.rejected_connections = total_rejected_.load();

        // Average connection duration calculation would require more tracking
        stats.average_connection_duration_ms = 0;

        return stats;
    }

    void
    ThreadPerConnectionManager::connection_handler_thread(std::unique_ptr<TcpConnection> connection,
                                                          CatalogManager* catalog,
                                                          std::int64_t /*start_time*/)
    {
        try {
            auto work_item = std::make_unique<ConnectionWorkItem>(std::move(connection), catalog);
            work_item->execute();
        } catch (const std::exception& e) {
            std::cerr << "Connection thread error: " << e.what() << std::endl;
        }
    }

    //=============================================================================
    // AsyncEventLoop Implementation (Basic)
    //=============================================================================

    AsyncEventLoop::AsyncEventLoop()
        : running_(false), epoll_fd_(-1), events_processed_(0), total_processing_time_us_(0)
    {
    }

    AsyncEventLoop::~AsyncEventLoop()
    {
        stop();
    }

    bool AsyncEventLoop::start()
    {
        if (running_.load()) {
            return true;
        }

        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ == -1) {
            return false;
        }

        running_ = true;
        event_thread_ = std::make_unique<std::thread>(&AsyncEventLoop::event_loop_main, this);

        return true;
    }

    void AsyncEventLoop::stop()
    {
        running_ = false;

        if (event_thread_ && event_thread_->joinable()) {
            event_thread_->join();
        }

        if (epoll_fd_ != -1) {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
    }

    bool AsyncEventLoop::register_fd(int fd, std::uint32_t events, EventCallback callback)
    {
        if (epoll_fd_ == -1 || !callback) {
            return false;
        }

        epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            fd_callbacks_[fd] = std::move(callback);
        }

        return true;
    }

    bool AsyncEventLoop::unregister_fd(int fd)
    {
        if (epoll_fd_ == -1) {
            return false;
        }

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            fd_callbacks_.erase(fd);
        }

        return true;
    }

    bool AsyncEventLoop::modify_fd(int fd, std::uint32_t events)
    {
        if (epoll_fd_ == -1) {
            return false;
        }

        epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;

        return epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) != -1;
    }

    AsyncEventLoop::EventLoopStats AsyncEventLoop::get_stats() const
    {
        EventLoopStats stats;
        stats.events_processed = events_processed_.load();

        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            stats.registered_fds = fd_callbacks_.size();
        }

        if (stats.events_processed > 0) {
            stats.average_event_processing_time_us =
                total_processing_time_us_.load() / stats.events_processed;
        }

        return stats;
    }

    void AsyncEventLoop::event_loop_main()
    {
        const int max_events = 64;
        epoll_event events[max_events];

        while (running_.load()) {
            int num_events = epoll_wait(epoll_fd_, events, max_events, 1000); // 1 second timeout

            if (num_events == -1) {
                if (errno == EINTR) {
                    continue;
                }
                break; // Error
            }

            auto start_time = get_current_time_us();

            for (int i = 0; i < num_events; ++i) {
                int fd = events[i].data.fd;
                std::uint32_t event_mask = events[i].events;

                EventCallback callback;
                {
                    std::lock_guard<std::mutex> lock(callbacks_mutex_);
                    auto it = fd_callbacks_.find(fd);
                    if (it != fd_callbacks_.end()) {
                        callback = it->second;
                    }
                }

                if (callback) {
                    try {
                        callback(fd, event_mask);
                    } catch (const std::exception& e) {
                        std::cerr << "Event callback error: " << e.what() << std::endl;
                    }
                }
            }

            auto end_time = get_current_time_us();
            events_processed_ += num_events;
            total_processing_time_us_ += (end_time - start_time);
        }
    }

    std::int64_t AsyncEventLoop::get_current_time_us() const
    {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch())
            .count();
    }

    //=============================================================================
    // HybridThreadingCoordinator Implementation
    //=============================================================================

    HybridThreadingCoordinator::HybridThreadingCoordinator(const HybridConfig& config)
        : config_(config), total_routed_(0), thread_pool_routed_(0), dedicated_routed_(0),
          async_io_routed_(0)
    {
        thread_pool_ = std::make_unique<ThreadPool>(config_.thread_pool_size);
        connection_manager_ =
            std::make_unique<ThreadPerConnectionManager>(config_.max_dedicated_connections);

        if (config_.enable_async_io) {
            event_loop_ = std::make_unique<AsyncEventLoop>();
        }
    }

    HybridThreadingCoordinator::~HybridThreadingCoordinator()
    {
        stop();
    }

    bool HybridThreadingCoordinator::start()
    {
        bool success = true;

        if (!thread_pool_->start()) {
            success = false;
        }

        if (event_loop_ && !event_loop_->start()) {
            success = false;
        }

        return success;
    }

    void HybridThreadingCoordinator::stop()
    {
        if (thread_pool_) {
            thread_pool_->shutdown_gracefully();
        }

        if (event_loop_) {
            event_loop_->stop();
        }
    }

    bool HybridThreadingCoordinator::is_running() const
    {
        bool running = true;

        if (thread_pool_ && !thread_pool_->is_running()) {
            running = false;
        }

        if (event_loop_ && !event_loop_->is_running()) {
            running = false;
        }

        return running;
    }

    bool HybridThreadingCoordinator::handle_connection(std::unique_ptr<TcpConnection> connection,
                                                       CatalogManager* catalog)
    {
        if (!connection || !catalog) {
            return false;
        }

        total_routed_++;

        ThreadingModel model = choose_threading_model(connection.get());

        switch (model) {
        case ThreadingModel::ThreadPool: {
            auto work = std::make_unique<ConnectionWorkItem>(std::move(connection), catalog);
            if (thread_pool_->submit_work(std::move(work))) {
                thread_pool_routed_++;
                return true;
            }
            break;
        }

        case ThreadingModel::ThreadPerConnection: {
            if (connection_manager_->handle_connection(std::move(connection), catalog)) {
                dedicated_routed_++;
                return true;
            }
            break;
        }

        case ThreadingModel::AsyncEventLoop: {
            // For async I/O, we'd register the connection's socket with the event loop
            // This is a simplified implementation
            if (event_loop_) {
                async_io_routed_++;
                return true;
            }
            break;
        }

        default:
            break;
        }

        return false;
    }

    bool HybridThreadingCoordinator::submit_cpu_intensive_work(std::unique_ptr<WorkItem> work)
    {
        if (thread_pool_) {
            return thread_pool_->submit_work(std::move(work));
        }
        return false;
    }

    bool HybridThreadingCoordinator::submit_io_work(std::unique_ptr<WorkItem> work)
    {
        // For I/O work, prefer thread pool or async handling
        if (thread_pool_) {
            return thread_pool_->submit_work(std::move(work));
        }
        return false;
    }

    HybridThreadingCoordinator::HybridStats HybridThreadingCoordinator::get_stats() const
    {
        HybridStats stats;

        if (thread_pool_) {
            stats.thread_pool_stats = thread_pool_->get_stats();
        }

        if (connection_manager_) {
            stats.connection_stats = connection_manager_->get_stats();
        }

        if (event_loop_) {
            stats.event_loop_stats = event_loop_->get_stats();
        }

        stats.total_connections_routed = total_routed_.load();
        stats.thread_pool_routed = thread_pool_routed_.load();
        stats.dedicated_thread_routed = dedicated_routed_.load();
        stats.async_io_routed = async_io_routed_.load();

        return stats;
    }

    ThreadingModel
    HybridThreadingCoordinator::choose_threading_model(const TcpConnection* connection) const
    {
        // Simple heuristics for choosing threading model
        // In a real implementation, this would be more sophisticated

        if (is_long_running_connection(connection)) {
            return ThreadingModel::ThreadPerConnection;
        }

        if (is_cpu_intensive_connection(connection)) {
            return ThreadingModel::ThreadPool;
        }

        if (config_.enable_async_io && event_loop_) {
            return ThreadingModel::AsyncEventLoop;
        }

        return ThreadingModel::ThreadPool;
    }

    bool HybridThreadingCoordinator::is_cpu_intensive_connection(
        const TcpConnection* /*connection*/) const
    {
        // Placeholder - would analyze connection characteristics
        return false;
    }

    bool HybridThreadingCoordinator::is_long_running_connection(
        const TcpConnection* /*connection*/) const
    {
        // Placeholder - would analyze expected connection duration
        return true; // Default to dedicated thread for now
    }

    //=============================================================================
    // ConcurrencyController Implementation
    //=============================================================================

    ConcurrencyController::ConcurrencyController()
        : active_locks_(0), total_requests_(0), failed_requests_(0), deadlocks_prevented_(0),
          contention_events_(0)
    {
    }

    ConcurrencyController::~ConcurrencyController()
    {
        // Clean up any remaining locks
        std::lock_guard<std::mutex> lock(resources_mutex_);
        resource_owners_.clear();
        thread_resources_.clear();
    }

    bool ConcurrencyController::is_deadlock_possible(const std::string& resource_id,
                                                     std::uint64_t thread_id) const
    {
        std::lock_guard<std::mutex> lock(resources_mutex_);

        auto owner_it = resource_owners_.find(resource_id);
        if (owner_it == resource_owners_.end()) {
            return false; // Resource is free
        }

        std::uint64_t current_owner = owner_it->second;
        if (current_owner == thread_id) {
            return false; // Same thread already owns it
        }

        // Check for circular dependency
        std::unordered_set<std::uint64_t> visited;
        return detect_cycle(thread_id, resource_id, visited);
    }

    void ConcurrencyController::register_lock_request(const std::string& resource_id,
                                                      std::uint64_t thread_id)
    {
        total_requests_++;

        if (is_deadlock_possible(resource_id, thread_id)) {
            deadlocks_prevented_++;
            failed_requests_++;
            return;
        }

        std::lock_guard<std::mutex> lock(resources_mutex_);
        resource_owners_[resource_id] = thread_id;
        thread_resources_[thread_id].push_back(resource_id);
        active_locks_++;
    }

    void ConcurrencyController::unregister_lock_request(const std::string& resource_id,
                                                        std::uint64_t thread_id)
    {
        std::lock_guard<std::mutex> lock(resources_mutex_);

        auto owner_it = resource_owners_.find(resource_id);
        if (owner_it != resource_owners_.end() && owner_it->second == thread_id) {
            resource_owners_.erase(owner_it);
            active_locks_--;
        }

        auto thread_it = thread_resources_.find(thread_id);
        if (thread_it != thread_resources_.end()) {
            auto& resources = thread_it->second;
            resources.erase(std::remove(resources.begin(), resources.end(), resource_id),
                            resources.end());

            if (resources.empty()) {
                thread_resources_.erase(thread_it);
            }
        }
    }

    ConcurrencyController::ConcurrencyStats ConcurrencyController::get_stats() const
    {
        ConcurrencyStats stats;
        stats.active_locks = active_locks_.load();
        stats.total_lock_requests = total_requests_.load();
        stats.failed_lock_requests = failed_requests_.load();
        stats.deadlocks_prevented = deadlocks_prevented_.load();
        stats.lock_contention_events = contention_events_.load();

        // Average wait time would require more detailed tracking
        stats.average_lock_wait_time_ms = 0;

        return stats;
    }

    void ConcurrencyController::reset_stats()
    {
        total_requests_ = 0;
        failed_requests_ = 0;
        deadlocks_prevented_ = 0;
        contention_events_ = 0;
    }

    bool ConcurrencyController::detect_cycle(std::uint64_t thread_id,
                                             const std::string& target_resource,
                                             std::unordered_set<std::uint64_t>& visited) const
    {
        if (visited.find(thread_id) != visited.end()) {
            return true; // Cycle detected
        }

        visited.insert(thread_id);

        // Check what resources this thread is waiting for
        auto thread_it = thread_resources_.find(thread_id);
        if (thread_it != thread_resources_.end()) {
            for (const auto& resource : thread_it->second) {
                auto owner_it = resource_owners_.find(resource);
                if (owner_it != resource_owners_.end()) {
                    if (detect_cycle(owner_it->second, target_resource, visited)) {
                        return true;
                    }
                }
            }
        }

        visited.erase(thread_id);
        return false;
    }

    //=============================================================================
    // ConcurrencyController::ResourceLock Implementation
    //=============================================================================

    ConcurrencyController::ResourceLock::ResourceLock(const std::string& resource_id,
                                                      ConcurrencyController* controller)
        : resource_id_(resource_id), controller_(controller), locked_(false)
    {
    }

    ConcurrencyController::ResourceLock::~ResourceLock()
    {
        if (locked_) {
            unlock();
        }
    }

    bool ConcurrencyController::ResourceLock::try_lock(std::uint32_t /*timeout_ms*/)
    {
        if (!controller_ || locked_) {
            return false;
        }

        std::uint64_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());

        if (controller_->is_deadlock_possible(resource_id_, thread_id)) {
            return false;
        }

        controller_->register_lock_request(resource_id_, thread_id);
        locked_ = true;

        return true;
    }

    void ConcurrencyController::ResourceLock::unlock()
    {
        if (controller_ && locked_) {
            std::uint64_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
            controller_->unregister_lock_request(resource_id_, thread_id);
            locked_ = false;
        }
    }

} // namespace scratchbird::engine
