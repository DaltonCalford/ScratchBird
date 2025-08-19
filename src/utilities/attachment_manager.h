#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <functional>
#include <queue>

// Forward declarations
extern "C" {
    #include <ibase.h>
}

namespace SBEnhanced {
    
    // Connection Pool Configuration
    struct ConnectionPoolConfig {
        uint32_t min_connections = 1;
        uint32_t max_connections = 10;
        uint32_t initial_connections = 2;
        std::chrono::seconds connection_timeout{30};
        std::chrono::seconds idle_timeout{300};
        std::chrono::seconds cleanup_interval{60};
        bool test_on_borrow = true;
        bool test_on_return = false;
        bool test_while_idle = true;
        std::string validation_query = "SELECT 1 FROM RDB$DATABASE";
    };
    
    // Connection Info
    struct ConnectionInfo {
        std::string database_path;
        std::string username;
        std::string password;
        std::string role;
        std::string charset = "UTF8";
        std::string collation = "UTF8";
        bool trusted_auth = false;
        std::map<std::string, std::string> connection_params;
        
        // Connection state
        isc_db_handle db_handle = 0;
        std::atomic<bool> is_active{false};
        std::atomic<bool> is_valid{true};
        std::chrono::steady_clock::time_point created_time;
        std::chrono::steady_clock::time_point last_used_time;
        std::chrono::steady_clock::time_point last_validated_time;
        std::atomic<uint64_t> query_count{0};
        std::atomic<uint64_t> transaction_count{0};
        std::atomic<uint64_t> error_count{0};
        
        // Thread info
        std::thread::id owner_thread;
        std::atomic<bool> in_use{false};
        std::string current_operation;
        
        // Statistics
        std::atomic<uint64_t> bytes_read{0};
        std::atomic<uint64_t> bytes_written{0};
        std::atomic<std::chrono::microseconds> total_query_time{std::chrono::microseconds::zero()};
        std::atomic<std::chrono::microseconds> total_transaction_time{std::chrono::microseconds::zero()};
    };
    
    // Connection Pool Statistics
    struct ConnectionPoolStats {
        std::atomic<uint32_t> active_connections{0};
        std::atomic<uint32_t> idle_connections{0};
        std::atomic<uint32_t> total_connections{0};
        std::atomic<uint32_t> peak_connections{0};
        std::atomic<uint64_t> connections_created{0};
        std::atomic<uint64_t> connections_destroyed{0};
        std::atomic<uint64_t> connections_borrowed{0};
        std::atomic<uint64_t> connections_returned{0};
        std::atomic<uint64_t> connections_validated{0};
        std::atomic<uint64_t> validation_failures{0};
        std::atomic<uint64_t> connection_timeouts{0};
        std::atomic<uint64_t> connection_errors{0};
        std::atomic<std::chrono::microseconds> average_borrow_time{std::chrono::microseconds::zero()};
        std::atomic<std::chrono::microseconds> average_validation_time{std::chrono::microseconds::zero()};
    };
    
    // Database Link Info
    struct DatabaseLinkInfo {
        std::string link_name;
        std::string remote_database;
        std::string remote_username;
        std::string remote_password;
        std::string remote_role;
        std::string local_schema;
        std::string remote_schema;
        std::string schema_mode;
        bool is_active = false;
        std::chrono::steady_clock::time_point created_time;
        std::chrono::steady_clock::time_point last_used_time;
        std::atomic<uint64_t> query_count{0};
        std::atomic<uint64_t> error_count{0};
    };
    
} // namespace SBEnhanced

// Attachment Manager Class
class AttachmentManager {
private:
    // Connection pool
    std::vector<std::unique_ptr<SBEnhanced::ConnectionInfo>> connection_pool;
    std::queue<size_t> available_connections;
    std::mutex pool_mutex;
    std::condition_variable pool_condition;
    
    // Configuration
    SBEnhanced::ConnectionPoolConfig config;
    std::string default_database;
    std::string default_username;
    std::string default_password;
    std::string default_role;
    
    // Statistics
    mutable SBEnhanced::ConnectionPoolStats stats;
    
    // Database links
    std::map<std::string, std::unique_ptr<SBEnhanced::DatabaseLinkInfo>> database_links;
    std::mutex links_mutex;
    
    // Background maintenance
    std::atomic<bool> maintenance_running{false};
    std::thread maintenance_thread;
    std::condition_variable maintenance_condition;
    std::mutex maintenance_mutex;
    
    // Error handling
    mutable std::vector<std::string> error_log;
    mutable std::mutex error_mutex;
    std::atomic<uint64_t> error_count{0};
    
    // Performance monitoring
    std::atomic<bool> performance_monitoring_enabled{false};
    std::map<std::string, std::atomic<uint64_t>> performance_counters;
    mutable std::mutex performance_mutex;
    
public:
    AttachmentManager();
    ~AttachmentManager();
    
    // Configuration
    bool initialize(const SBEnhanced::ConnectionPoolConfig& config);
    bool setDefaultConnection(const std::string& database, const std::string& username,
                             const std::string& password, const std::string& role);
    bool shutdown();
    
    // Connection management
    bool createConnection(const std::string& database, const std::string& username,
                         const std::string& password, const std::string& role,
                         const std::map<std::string, std::string>& params = {});
    std::shared_ptr<SBEnhanced::ConnectionInfo> borrowConnection(std::chrono::seconds timeout = std::chrono::seconds{30});
    bool returnConnection(std::shared_ptr<SBEnhanced::ConnectionInfo> connection);
    bool testConnection(SBEnhanced::ConnectionInfo* connection);
    bool validateConnection(SBEnhanced::ConnectionInfo* connection);
    
    // Connection pool operations
    bool expandPool(uint32_t additional_connections);
    bool shrinkPool(uint32_t remove_connections);
    bool clearPool();
    bool warmupPool();
    
    // Database link management
    bool createDatabaseLink(const std::string& link_name, const std::string& remote_database,
                           const std::string& remote_username, const std::string& remote_password,
                           const std::string& remote_role, const std::string& local_schema,
                           const std::string& remote_schema, const std::string& schema_mode);
    bool dropDatabaseLink(const std::string& link_name);
    bool testDatabaseLink(const std::string& link_name);
    std::vector<std::string> getDatabaseLinks() const;
    bool getDatabaseLinkInfo(const std::string& link_name, SBEnhanced::DatabaseLinkInfo& info) const;
    
    // Connection monitoring
    SBEnhanced::ConnectionPoolStats getPoolStatistics() const;
    std::vector<SBEnhanced::ConnectionInfo> getActiveConnections() const;
    bool killConnection(const std::string& connection_id);
    bool killAllConnections();
    
    // Performance monitoring
    bool enablePerformanceMonitoring(bool enable = true);
    std::map<std::string, uint64_t> getPerformanceCounters() const;
    bool resetPerformanceCounters();
    
    // Error handling
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();
    uint64_t getErrorCount() const;
    std::string getLastError() const;
    
    // Maintenance operations
    bool startMaintenanceThread();
    bool stopMaintenanceThread();
    bool runMaintenance();
    
    // Configuration queries
    SBEnhanced::ConnectionPoolConfig getConfiguration() const;
    bool updateConfiguration(const SBEnhanced::ConnectionPoolConfig& config);
    
    // Utility methods
    bool isInitialized() const;
    uint32_t getActiveConnectionCount() const;
    uint32_t getIdleConnectionCount() const;
    uint32_t getTotalConnectionCount() const;
    
private:
    // Internal connection management
    bool createConnectionInternal(const std::string& database, const std::string& username,
                                 const std::string& password, const std::string& role,
                                 const std::map<std::string, std::string>& params,
                                 SBEnhanced::ConnectionInfo* connection);
    bool destroyConnectionInternal(SBEnhanced::ConnectionInfo* connection);
    bool validateConnectionInternal(SBEnhanced::ConnectionInfo* connection);
    
    // Connection pool maintenance
    void maintenanceLoop();
    bool cleanupIdleConnections();
    bool validatePoolConnections();
    bool ensureMinimumConnections();
    
    // Database parameter building
    std::string buildDPB(const std::string& username, const std::string& password,
                        const std::string& role, const std::map<std::string, std::string>& params);
    
    // Error handling helpers
    void logError(const std::string& operation, const std::string& error) const;
    void logPerformance(const std::string& operation, std::chrono::microseconds duration) const;
    std::string formatISCError(const ISC_STATUS* status_vector) const;
    
    // Statistics helpers
    void updateStats(const std::string& operation, std::chrono::microseconds duration) const;
    void incrementCounter(const std::string& counter_name) const;
    
    // Utility helpers
    bool isConnectionExpired(const SBEnhanced::ConnectionInfo* connection) const;
    bool isConnectionIdle(const SBEnhanced::ConnectionInfo* connection) const;
    std::string generateConnectionId(const SBEnhanced::ConnectionInfo* connection) const;
};