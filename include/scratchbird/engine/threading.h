#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{

    /// Forward declarations
    class TcpConnection;
    class ProtocolHandlerManager;
    class CatalogManager;

    /// Threading model types
    enum class ThreadingModel {
        ThreadPerConnection, // One thread per connection
        ThreadPool,          // Fixed thread pool with work queue
        AsyncEventLoop,      // Async I/O with event loops
        HybridModel          // Combination based on workload
    };

    /// Work item priority levels
    enum class WorkPriority { Low = 0, Normal = 1, High = 2, Critical = 3 };

    /// Base work item interface
    class WorkItem
    {
      public:
        WorkItem(WorkPriority priority = WorkPriority::Normal);
        virtual ~WorkItem() = default;

        virtual void execute() = 0;
        virtual std::string get_description() const = 0;

        WorkPriority get_priority() const
        {
            return priority_;
        }
        std::uint64_t get_work_id() const
        {
            return work_id_;
        }
        std::int64_t get_created_time() const
        {
            return created_time_;
        }
        std::int64_t get_execution_time() const
        {
            return execution_time_;
        }
        bool is_executed() const
        {
            return executed_.load();
        }

        void mark_executed();

      private:
        WorkPriority priority_;
        std::uint64_t work_id_;
        std::int64_t created_time_;
        std::atomic<std::int64_t> execution_time_;
        std::atomic<bool> executed_;

        static std::atomic<std::uint64_t> next_work_id_;
    };

    /// Connection work item - handles client connection
    class ConnectionWorkItem : public WorkItem
    {
      public:
        ConnectionWorkItem(std::unique_ptr<TcpConnection> connection, CatalogManager* catalog,
                           WorkPriority priority = WorkPriority::Normal);

        void execute() override;
        std::string get_description() const override;

      private:
        std::unique_ptr<TcpConnection> connection_;
        CatalogManager* catalog_;
        std::unique_ptr<ProtocolHandlerManager> protocol_manager_;
    };

    /// Generic function work item
    class FunctionWorkItem : public WorkItem
    {
      public:
        using WorkFunction = std::function<void()>;

        FunctionWorkItem(WorkFunction func, const std::string& description,
                         WorkPriority priority = WorkPriority::Normal);

        void execute() override;
        std::string get_description() const override;

      private:
        WorkFunction function_;
        std::string description_;
    };

    /// Worker thread statistics
    struct WorkerThreadStats {
        std::uint64_t thread_id = 0;
        std::string thread_name;
        std::uint64_t work_items_processed = 0;
        std::uint64_t total_execution_time_ms = 0;
        std::uint64_t average_execution_time_ms = 0;
        std::int64_t last_activity_time = 0;
        bool is_active = false;
        std::uint32_t queue_size = 0;
    };

    /// Thread pool statistics
    struct ThreadPoolStats {
        std::uint32_t total_threads = 0;
        std::uint32_t active_threads = 0;
        std::uint32_t idle_threads = 0;
        std::uint64_t total_work_items_processed = 0;
        std::uint64_t pending_work_items = 0;
        std::uint64_t rejected_work_items = 0;
        std::uint64_t average_queue_wait_time_ms = 0;
        std::uint64_t average_execution_time_ms = 0;
    };

    /// Worker thread class
    class WorkerThread
    {
      public:
        WorkerThread(const std::string& name, std::uint32_t thread_id);
        ~WorkerThread();

        /// Thread lifecycle
        bool start();
        void stop();
        void join();
        bool is_running() const
        {
            return running_.load();
        }

        /// Work item management
        void enqueue_work(std::unique_ptr<WorkItem> work);
        bool try_dequeue_work(std::unique_ptr<WorkItem>& work, std::uint32_t timeout_ms = 0);
        std::size_t get_queue_size() const;

        /// Statistics
        WorkerThreadStats get_stats() const;
        std::uint64_t get_thread_id() const
        {
            return thread_id_;
        }
        std::string get_thread_name() const
        {
            return thread_name_;
        }

      private:
        std::string thread_name_;
        std::uint64_t thread_id_;
        std::atomic<bool> running_;
        std::atomic<bool> stop_requested_;

        std::unique_ptr<std::thread> worker_thread_;

        mutable std::mutex work_queue_mutex_;
        std::condition_variable work_available_;
        std::queue<std::unique_ptr<WorkItem>> work_queue_;

        // Statistics
        std::atomic<std::uint64_t> work_items_processed_;
        std::atomic<std::uint64_t> total_execution_time_ms_;
        std::atomic<std::int64_t> last_activity_time_;

        void worker_thread_main();
        void update_activity_time();
        std::int64_t get_current_time_ms() const;
    };

    /// Thread pool implementation
    class ThreadPool
    {
      public:
        explicit ThreadPool(std::uint32_t num_threads = std::thread::hardware_concurrency());
        ~ThreadPool();

        /// Pool lifecycle
        bool start();
        void stop();
        void shutdown_gracefully();
        bool is_running() const
        {
            return running_.load();
        }

        /// Work item submission
        bool submit_work(std::unique_ptr<WorkItem> work);
        bool submit_work_with_timeout(std::unique_ptr<WorkItem> work, std::uint32_t timeout_ms);

        /// Pool management
        bool resize_pool(std::uint32_t new_size);
        std::uint32_t get_pool_size() const
        {
            return pool_size_;
        }

        /// Statistics and monitoring
        ThreadPoolStats get_stats() const;
        std::vector<WorkerThreadStats> get_worker_stats() const;
        void reset_stats();

        /// Load balancing
        std::uint32_t get_least_loaded_worker() const;
        void distribute_work_evenly();

      private:
        std::uint32_t pool_size_;
        std::atomic<bool> running_;
        std::atomic<bool> shutdown_requested_;

        std::vector<std::unique_ptr<WorkerThread>> workers_;
        mutable std::mutex pool_mutex_;

        // Statistics
        std::atomic<std::uint64_t> total_submitted_;
        std::atomic<std::uint64_t> total_processed_;
        std::atomic<std::uint64_t> total_rejected_;

        // Load balancing
        std::atomic<std::uint32_t> next_worker_;

        bool create_workers();
        void destroy_workers();
    };

    /// Thread-per-connection manager
    class ThreadPerConnectionManager
    {
      public:
        ThreadPerConnectionManager(std::uint32_t max_connections = 1000);
        ~ThreadPerConnectionManager();

        /// Connection handling
        bool handle_connection(std::unique_ptr<TcpConnection> connection, CatalogManager* catalog);
        void cleanup_finished_connections();

        /// Management
        std::uint32_t get_active_connections() const
        {
            return active_connections_.load();
        }
        std::uint32_t get_max_connections() const
        {
            return max_connections_;
        }
        bool is_connection_limit_reached() const
        {
            return active_connections_.load() >= max_connections_;
        }

        /// Statistics
        struct ConnectionStats {
            std::uint32_t active_connections = 0;
            std::uint32_t total_connections_handled = 0;
            std::uint32_t rejected_connections = 0;
            std::uint64_t average_connection_duration_ms = 0;
        };

        ConnectionStats get_stats() const;

      private:
        std::uint32_t max_connections_;
        std::atomic<std::uint32_t> active_connections_;
        std::atomic<std::uint32_t> total_handled_;
        std::atomic<std::uint32_t> total_rejected_;

        mutable std::mutex connections_mutex_;
        std::vector<std::pair<std::thread, std::int64_t>> connection_threads_;

        void connection_handler_thread(std::unique_ptr<TcpConnection> connection,
                                       CatalogManager* catalog, std::int64_t start_time);
    };

    /// Async I/O event loop (basic implementation)
    class AsyncEventLoop
    {
      public:
        using EventCallback = std::function<void(int fd, std::uint32_t events)>;

        AsyncEventLoop();
        ~AsyncEventLoop();

        /// Event loop lifecycle
        bool start();
        void stop();
        bool is_running() const
        {
            return running_.load();
        }

        /// Event registration
        bool register_fd(int fd, std::uint32_t events, EventCallback callback);
        bool unregister_fd(int fd);
        bool modify_fd(int fd, std::uint32_t events);

        /// Statistics
        struct EventLoopStats {
            std::uint64_t events_processed = 0;
            std::uint64_t registered_fds = 0;
            std::uint64_t average_event_processing_time_us = 0;
        };

        EventLoopStats get_stats() const;

      private:
        std::atomic<bool> running_;
        std::unique_ptr<std::thread> event_thread_;

        int epoll_fd_;
        mutable std::mutex callbacks_mutex_;
        std::unordered_map<int, EventCallback> fd_callbacks_;

        // Statistics
        std::atomic<std::uint64_t> events_processed_;
        std::atomic<std::uint64_t> total_processing_time_us_;

        void event_loop_main();
        std::int64_t get_current_time_us() const;
    };

    /// Hybrid threading coordinator
    class HybridThreadingCoordinator
    {
      public:
        struct HybridConfig {
            std::uint32_t thread_pool_size = std::thread::hardware_concurrency();
            std::uint32_t max_dedicated_connections = 100;
            bool enable_async_io = true;
            bool enable_load_balancing = true;
        };

        explicit HybridThreadingCoordinator(const HybridConfig& config);
        ~HybridThreadingCoordinator();

        /// Coordinator lifecycle
        bool start();
        void stop();
        bool is_running() const;

        /// Connection handling
        bool handle_connection(std::unique_ptr<TcpConnection> connection, CatalogManager* catalog);

        /// Workload management
        bool submit_cpu_intensive_work(std::unique_ptr<WorkItem> work);
        bool submit_io_work(std::unique_ptr<WorkItem> work);

        /// Statistics and monitoring
        struct HybridStats {
            ThreadPoolStats thread_pool_stats;
            ThreadPerConnectionManager::ConnectionStats connection_stats;
            AsyncEventLoop::EventLoopStats event_loop_stats;
            std::uint64_t total_connections_routed = 0;
            std::uint64_t thread_pool_routed = 0;
            std::uint64_t dedicated_thread_routed = 0;
            std::uint64_t async_io_routed = 0;
        };

        HybridStats get_stats() const;

      private:
        HybridConfig config_;

        std::unique_ptr<ThreadPool> thread_pool_;
        std::unique_ptr<ThreadPerConnectionManager> connection_manager_;
        std::unique_ptr<AsyncEventLoop> event_loop_;

        std::atomic<std::uint64_t> total_routed_;
        std::atomic<std::uint64_t> thread_pool_routed_;
        std::atomic<std::uint64_t> dedicated_routed_;
        std::atomic<std::uint64_t> async_io_routed_;

        ThreadingModel choose_threading_model(const TcpConnection* connection) const;
        bool is_cpu_intensive_connection(const TcpConnection* connection) const;
        bool is_long_running_connection(const TcpConnection* connection) const;
    };

    /// Concurrency control and deadlock prevention
    class ConcurrencyController
    {
      public:
        ConcurrencyController();
        ~ConcurrencyController();

        /// Resource management
        class ResourceLock
        {
          public:
            ResourceLock(const std::string& resource_id, ConcurrencyController* controller);
            ~ResourceLock();

            bool try_lock(std::uint32_t timeout_ms = 1000);
            void unlock();
            bool is_locked() const
            {
                return locked_;
            }

          private:
            std::string resource_id_;
            ConcurrencyController* controller_;
            bool locked_;
        };

        /// Deadlock detection and prevention
        bool is_deadlock_possible(const std::string& resource_id, std::uint64_t thread_id) const;
        void register_lock_request(const std::string& resource_id, std::uint64_t thread_id);
        void unregister_lock_request(const std::string& resource_id, std::uint64_t thread_id);

        /// Performance monitoring
        struct ConcurrencyStats {
            std::uint64_t active_locks = 0;
            std::uint64_t total_lock_requests = 0;
            std::uint64_t failed_lock_requests = 0;
            std::uint64_t deadlocks_prevented = 0;
            std::uint64_t average_lock_wait_time_ms = 0;
            std::uint64_t lock_contention_events = 0;
        };

        ConcurrencyStats get_stats() const;
        void reset_stats();

      private:
        mutable std::mutex resources_mutex_;
        std::unordered_map<std::string, std::mutex> resource_mutexes_;
        std::unordered_map<std::string, std::uint64_t> resource_owners_;
        std::unordered_map<std::uint64_t, std::vector<std::string>> thread_resources_;

        // Statistics
        std::atomic<std::uint64_t> active_locks_;
        std::atomic<std::uint64_t> total_requests_;
        std::atomic<std::uint64_t> failed_requests_;
        std::atomic<std::uint64_t> deadlocks_prevented_;
        std::atomic<std::uint64_t> contention_events_;

        bool detect_cycle(std::uint64_t thread_id, const std::string& target_resource,
                          std::unordered_set<std::uint64_t>& visited) const;
    };

} // namespace scratchbird::engine
