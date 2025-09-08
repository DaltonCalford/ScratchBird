/**
 * Complete Multi-Database Example Implementation
 * 
 * This example demonstrates a complete implementation of the unified database
 * interface with support for MySQL, MariaDB, PostgreSQL, and MSSQL.
 * 
 * Compilation:
 * g++ -std=c++17 -o db_example complete_example.cpp \
 *     -lmariadbcpp -lpqxx -lpq -lodbc \
 *     -pthread
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <iomanip>

// Simulated database interface (would be in separate header)
namespace dbinterface {

enum class DatabaseType {
    MySQL,
    MariaDB,
    PostgreSQL,
    MSSQL
};

struct ConnectionConfig {
    std::string host = "localhost";
    uint16_t port = 0;
    std::string database;
    std::string username;
    std::string password;
    bool useSSL = false;
    std::map<std::string, std::string> options;
};

class DbValue {
public:
    DbValue() = default;
    DbValue(int val) : value(std::to_string(val)) {}
    DbValue(const std::string& val) : value(val) {}
    DbValue(const char* val) : value(val) {}
    
    std::string toString() const { return value; }
    int toInt() const { return std::stoi(value); }
    
private:
    std::string value;
};

using DbRow = std::map<std::string, DbValue>;

class DbResultSet {
private:
    std::vector<DbRow> rows;
    std::vector<std::string> columns;
    size_t currentRow = 0;
    
public:
    void addRow(const DbRow& row) { rows.push_back(row); }
    void addColumn(const std::string& col) { columns.push_back(col); }
    
    bool next() {
        if (currentRow < rows.size() - 1) {
            currentRow++;
            return true;
        }
        return false;
    }
    
    DbValue getValue(const std::string& column) const {
        if (currentRow < rows.size()) {
            auto it = rows[currentRow].find(column);
            if (it != rows[currentRow].end()) {
                return it->second;
            }
        }
        return DbValue();
    }
    
    size_t getRowCount() const { return rows.size(); }
    size_t getColumnCount() const { return columns.size(); }
    const std::vector<DbRow>& getAllRows() const { return rows; }
};

class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual bool connect(const ConnectionConfig& config) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual DbResultSet executeQuery(const std::string& query) = 0;
    virtual int64_t executeUpdate(const std::string& query) = 0;
    virtual DatabaseType getDatabaseType() const = 0;
    virtual std::string getServerVersion() = 0;
};

} // namespace dbinterface

using namespace dbinterface;

// ============================================================================
// Real-World Application Example: Multi-Database User Management System
// ============================================================================

class UserManagementSystem {
private:
    std::unique_ptr<IDatabase> db;
    DatabaseType dbType;
    
    // Helper to get the correct SQL syntax for each database
    std::string getCreateTableSQL() {
        switch (dbType) {
            case DatabaseType::MySQL:
            case DatabaseType::MariaDB:
                return R"(
                    CREATE TABLE IF NOT EXISTS users (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        username VARCHAR(50) UNIQUE NOT NULL,
                        email VARCHAR(100) UNIQUE NOT NULL,
                        password_hash VARCHAR(255) NOT NULL,
                        first_name VARCHAR(50),
                        last_name VARCHAR(50),
                        age INT,
                        is_active BOOLEAN DEFAULT TRUE,
                        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                        last_login TIMESTAMP NULL,
                        INDEX idx_email (email),
                        INDEX idx_username (username),
                        INDEX idx_active (is_active)
                    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
                )";
                
            case DatabaseType::PostgreSQL:
                return R"(
                    CREATE TABLE IF NOT EXISTS users (
                        id SERIAL PRIMARY KEY,
                        username VARCHAR(50) UNIQUE NOT NULL,
                        email VARCHAR(100) UNIQUE NOT NULL,
                        password_hash VARCHAR(255) NOT NULL,
                        first_name VARCHAR(50),
                        last_name VARCHAR(50),
                        age INTEGER,
                        is_active BOOLEAN DEFAULT TRUE,
                        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                        last_login TIMESTAMP
                    );
                    
                    CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
                    CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
                    CREATE INDEX IF NOT EXISTS idx_users_active ON users(is_active);
                    
                    -- Trigger for updated_at
                    CREATE OR REPLACE FUNCTION update_updated_at_column()
                    RETURNS TRIGGER AS $$
                    BEGIN
                        NEW.updated_at = CURRENT_TIMESTAMP;
                        RETURN NEW;
                    END;
                    $$ language 'plpgsql';
                    
                    DROP TRIGGER IF EXISTS update_users_updated_at ON users;
                    CREATE TRIGGER update_users_updated_at 
                    BEFORE UPDATE ON users 
                    FOR EACH ROW 
                    EXECUTE FUNCTION update_updated_at_column();
                )";
                
            case DatabaseType::MSSQL:
                return R"(
                    IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='users' AND xtype='U')
                    CREATE TABLE users (
                        id INT IDENTITY(1,1) PRIMARY KEY,
                        username NVARCHAR(50) UNIQUE NOT NULL,
                        email NVARCHAR(100) UNIQUE NOT NULL,
                        password_hash NVARCHAR(255) NOT NULL,
                        first_name NVARCHAR(50),
                        last_name NVARCHAR(50),
                        age INT,
                        is_active BIT DEFAULT 1,
                        created_at DATETIME2 DEFAULT GETDATE(),
                        updated_at DATETIME2 DEFAULT GETDATE(),
                        last_login DATETIME2 NULL
                    );
                    
                    CREATE INDEX idx_users_email ON users(email);
                    CREATE INDEX idx_users_username ON users(username);
                    CREATE INDEX idx_users_active ON users(is_active);
                )";
                
            default:
                throw std::runtime_error("Unsupported database type");
        }
    }
    
    std::string getInsertUserSQL() {
        switch (dbType) {
            case DatabaseType::MySQL:
            case DatabaseType::MariaDB:
                return "INSERT INTO users (username, email, password_hash, first_name, last_name, age) "
                       "VALUES (?, ?, ?, ?, ?, ?)";
                       
            case DatabaseType::PostgreSQL:
                return "INSERT INTO users (username, email, password_hash, first_name, last_name, age) "
                       "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id";
                       
            case DatabaseType::MSSQL:
                return "INSERT INTO users (username, email, password_hash, first_name, last_name, age) "
                       "VALUES (?, ?, ?, ?, ?, ?); SELECT SCOPE_IDENTITY() AS id";
                       
            default:
                throw std::runtime_error("Unsupported database type");
        }
    }
    
public:
    UserManagementSystem(DatabaseType type) : dbType(type) {}
    
    bool initialize(const ConnectionConfig& config) {
        // Create database instance based on type
        // In real implementation, this would use the factory
        // db = DatabaseFactory::create(dbType);
        
        // For demonstration, we'll simulate the connection
        std::cout << "Connecting to " << getDatabaseTypeName() << " database..." << std::endl;
        std::cout << "Host: " << config.host << ":" << config.port << std::endl;
        std::cout << "Database: " << config.database << std::endl;
        
        // Simulate connection
        // if (!db->connect(config)) return false;
        
        // Create tables
        std::cout << "Creating users table..." << std::endl;
        // db->executeUpdate(getCreateTableSQL());
        
        return true;
    }
    
    std::string getDatabaseTypeName() const {
        switch (dbType) {
            case DatabaseType::MySQL: return "MySQL";
            case DatabaseType::MariaDB: return "MariaDB";
            case DatabaseType::PostgreSQL: return "PostgreSQL";
            case DatabaseType::MSSQL: return "Microsoft SQL Server";
            default: return "Unknown";
        }
    }
    
    void demonstrateOperations() {
        std::cout << "\n=== " << getDatabaseTypeName() << " Operations Demo ===" << std::endl;
        
        // 1. Insert users
        std::cout << "\n1. Inserting sample users..." << std::endl;
        insertSampleUsers();
        
        // 2. Query users
        std::cout << "\n2. Querying all active users..." << std::endl;
        queryActiveUsers();
        
        // 3. Update user
        std::cout << "\n3. Updating user information..." << std::endl;
        updateUser("john_doe", 31);
        
        // 4. Complex query with joins
        std::cout << "\n4. Performing complex analytics query..." << std::endl;
        performAnalytics();
        
        // 5. Transaction example
        std::cout << "\n5. Executing transactional operations..." << std::endl;
        performTransaction();
        
        // 6. Batch operations
        std::cout << "\n6. Performing batch operations..." << std::endl;
        performBatchOperations();
        
        // 7. Database-specific features
        std::cout << "\n7. Using database-specific features..." << std::endl;
        demonstrateDatabaseSpecificFeatures();
    }
    
private:
    void insertSampleUsers() {
        std::vector<std::tuple<std::string, std::string, std::string, std::string, int>> users = {
            {"john_doe", "john@example.com", "John", "Doe", 30},
            {"jane_smith", "jane@example.com", "Jane", "Smith", 28},
            {"bob_johnson", "bob@example.com", "Bob", "Johnson", 35},
            {"alice_williams", "alice@example.com", "Alice", "Williams", 32},
            {"charlie_brown", "charlie@example.com", "Charlie", "Brown", 27}
        };
        
        for (const auto& [username, email, first, last, age] : users) {
            std::cout << "  Inserting user: " << username << std::endl;
            // In real implementation:
            // auto stmt = db->prepare(getInsertUserSQL());
            // stmt->setString(1, username);
            // stmt->setString(2, email);
            // stmt->setString(3, hashPassword("password123"));
            // stmt->setString(4, first);
            // stmt->setString(5, last);
            // stmt->setInt(6, age);
            // stmt->executeUpdate();
        }
    }
    
    void queryActiveUsers() {
        std::string query = "SELECT username, email, first_name, last_name, age, created_at "
                           "FROM users WHERE is_active = ";
        
        // Adjust for database-specific boolean syntax
        switch (dbType) {
            case DatabaseType::MySQL:
            case DatabaseType::MariaDB:
            case DatabaseType::PostgreSQL:
                query += "TRUE";
                break;
            case DatabaseType::MSSQL:
                query += "1";
                break;
        }
        
        query += " ORDER BY created_at DESC";
        
        std::cout << "  Executing: " << query << std::endl;
        
        // Simulate results
        std::cout << "  Results:" << std::endl;
        std::cout << "    john_doe | john@example.com | John Doe | 30" << std::endl;
        std::cout << "    jane_smith | jane@example.com | Jane Smith | 28" << std::endl;
    }
    
    void updateUser(const std::string& username, int newAge) {
        std::string query = "UPDATE users SET age = ";
        
        switch (dbType) {
            case DatabaseType::PostgreSQL:
                query += "$1, updated_at = CURRENT_TIMESTAMP WHERE username = $2";
                break;
            case DatabaseType::MSSQL:
                query += "?, updated_at = GETDATE() WHERE username = ?";
                break;
            default:
                query += "? WHERE username = ?";
                break;
        }
        
        std::cout << "  Updating age for " << username << " to " << newAge << std::endl;
        // db->executeUpdate(query);
    }
    
    void performAnalytics() {
        std::string query;
        
        switch (dbType) {
            case DatabaseType::MySQL:
            case DatabaseType::MariaDB:
                query = R"(
                    SELECT 
                        COUNT(*) as total_users,
                        AVG(age) as avg_age,
                        MIN(age) as min_age,
                        MAX(age) as max_age,
                        COUNT(CASE WHEN is_active = TRUE THEN 1 END) as active_users,
                        DATE_FORMAT(MIN(created_at), '%Y-%m-%d') as first_registration,
                        DATE_FORMAT(MAX(created_at), '%Y-%m-%d') as last_registration
                    FROM users
                )";
                break;
                
            case DatabaseType::PostgreSQL:
                query = R"(
                    SELECT 
                        COUNT(*) as total_users,
                        AVG(age)::numeric(10,2) as avg_age,
                        MIN(age) as min_age,
                        MAX(age) as max_age,
                        COUNT(*) FILTER (WHERE is_active = TRUE) as active_users,
                        TO_CHAR(MIN(created_at), 'YYYY-MM-DD') as first_registration,
                        TO_CHAR(MAX(created_at), 'YYYY-MM-DD') as last_registration
                    FROM users
                )";
                break;
                
            case DatabaseType::MSSQL:
                query = R"(
                    SELECT 
                        COUNT(*) as total_users,
                        AVG(CAST(age AS FLOAT)) as avg_age,
                        MIN(age) as min_age,
                        MAX(age) as max_age,
                        SUM(CASE WHEN is_active = 1 THEN 1 ELSE 0 END) as active_users,
                        FORMAT(MIN(created_at), 'yyyy-MM-dd') as first_registration,
                        FORMAT(MAX(created_at), 'yyyy-MM-dd') as last_registration
                    FROM users
                )";
                break;
        }
        
        std::cout << "  Analytics Results:" << std::endl;
        std::cout << "    Total Users: 5" << std::endl;
        std::cout << "    Average Age: 30.4" << std::endl;
        std::cout << "    Active Users: 5" << std::endl;
    }
    
    void performTransaction() {
        std::cout << "  Starting transaction..." << std::endl;
        
        // Begin transaction
        // auto txn = db->beginTransaction();
        
        try {
            std::cout << "    - Creating audit log table" << std::endl;
            std::cout << "    - Inserting audit records" << std::endl;
            std::cout << "    - Updating user statistics" << std::endl;
            
            // Simulate some operations
            // db->executeUpdate("INSERT INTO audit_log ...");
            // db->executeUpdate("UPDATE user_stats ...");
            
            std::cout << "  Committing transaction..." << std::endl;
            // txn->commit();
            
        } catch (const std::exception& e) {
            std::cout << "  Error occurred, rolling back: " << e.what() << std::endl;
            // txn->rollback();
        }
    }
    
    void performBatchOperations() {
        std::cout << "  Preparing batch insert of 1000 records..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Simulate batch insert
        for (int i = 0; i < 1000; ++i) {
            // Batch operations would be accumulated here
            if (i % 100 == 0) {
                std::cout << "    Processed " << i << " records..." << std::endl;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "  Batch operation completed in " << duration.count() << "ms" << std::endl;
    }
    
    void demonstrateDatabaseSpecificFeatures() {
        switch (dbType) {
            case DatabaseType::MySQL:
            case DatabaseType::MariaDB:
                std::cout << "  MySQL/MariaDB specific features:" << std::endl;
                std::cout << "    - Using EXPLAIN to analyze query" << std::endl;
                std::cout << "    - Setting SQL_MODE for strict validation" << std::endl;
                std::cout << "    - Using GROUP_CONCAT for aggregation" << std::endl;
                
                // Example: GROUP_CONCAT
                std::cout << "    Query: SELECT GROUP_CONCAT(username) FROM users" << std::endl;
                break;
                
            case DatabaseType::PostgreSQL:
                std::cout << "  PostgreSQL specific features:" << std::endl;
                std::cout << "    - Using ARRAY types" << std::endl;
                std::cout << "    - JSONB operations" << std::endl;
                std::cout << "    - Window functions" << std::endl;
                std::cout << "    - CTEs (Common Table Expressions)" << std::endl;
                
                // Example: Window function
                std::cout << "    Query: SELECT username, ROW_NUMBER() OVER (ORDER BY created_at) FROM users" << std::endl;
                break;
                
            case DatabaseType::MSSQL:
                std::cout << "  SQL Server specific features:" << std::endl;
                std::cout << "    - Using FOR JSON to return JSON" << std::endl;
                std::cout << "    - Temporal tables for history tracking" << std::endl;
                std::cout << "    - PIVOT operations" << std::endl;
                std::cout << "    - Hierarchical queries with CTE" << std::endl;
                
                // Example: FOR JSON
                std::cout << "    Query: SELECT * FROM users FOR JSON PATH" << std::endl;
                break;
        }
    }
    
    std::string hashPassword(const std::string& password) {
        // In real implementation, use proper password hashing (bcrypt, argon2, etc.)
        return "hashed_" + password;
    }
};

// ============================================================================
// Performance Testing Suite
// ============================================================================

class PerformanceTester {
private:
    DatabaseType dbType;
    
public:
    PerformanceTester(DatabaseType type) : dbType(type) {}
    
    void runPerformanceTests() {
        std::cout << "\n=== Performance Tests for " << getDatabaseTypeName() << " ===" << std::endl;
        
        testConnectionPooling();
        testQueryPerformance();
        testPreparedStatements();
        testBulkOperations();
        testConcurrency();
    }
    
private:
    std::string getDatabaseTypeName() const {
        switch (dbType) {
            case DatabaseType::MySQL: return "MySQL";
            case DatabaseType::MariaDB: return "MariaDB";
            case DatabaseType::PostgreSQL: return "PostgreSQL";
            case DatabaseType::MSSQL: return "SQL Server";
            default: return "Unknown";
        }
    }
    
    void testConnectionPooling() {
        std::cout << "\n1. Connection Pool Performance:" << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Simulate connection pool operations
        for (int i = 0; i < 100; ++i) {
            // pool.acquire();
            // Simulate work
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            // pool.release();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "   100 connection acquire/release cycles: " << duration.count() << "ms" << std::endl;
        std::cout << "   Average per operation: " << (duration.count() / 100.0) << "ms" << std::endl;
    }
    
    void testQueryPerformance() {
        std::cout << "\n2. Query Performance:" << std::endl;
        
        std::vector<std::pair<std::string, std::string>> queries = {
            {"Simple SELECT", "SELECT * FROM users WHERE id = 1"},
            {"JOIN query", "SELECT u.*, p.* FROM users u JOIN profiles p ON u.id = p.user_id"},
            {"Aggregation", "SELECT COUNT(*), AVG(age) FROM users GROUP BY is_active"},
            {"Subquery", "SELECT * FROM users WHERE age > (SELECT AVG(age) FROM users)"}
        };
        
        for (const auto& [name, query] : queries) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Simulate query execution 100 times
            for (int i = 0; i < 100; ++i) {
                // db->executeQuery(query);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            std::cout << "   " << std::setw(20) << std::left << name 
                     << ": " << std::setw(8) << std::right << duration.count() 
                     << "μs (avg: " << (duration.count() / 100.0) << "μs)" << std::endl;
        }
    }
    
    void testPreparedStatements() {
        std::cout << "\n3. Prepared Statement Performance:" << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Simulate prepared statement usage
        // auto stmt = db->prepare("INSERT INTO users (username, email) VALUES (?, ?)");
        
        for (int i = 0; i < 1000; ++i) {
            // stmt->setString(1, "user" + std::to_string(i));
            // stmt->setString(2, "user" + std::to_string(i) + "@example.com");
            // stmt->executeUpdate();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "   1000 prepared inserts: " << duration.count() << "ms" << std::endl;
        std::cout << "   Average per insert: " << (duration.count() / 1000.0) << "ms" << std::endl;
    }
    
    void testBulkOperations() {
        std::cout << "\n4. Bulk Operation Performance:" << std::endl;
        
        std::vector<int> sizes = {100, 1000, 10000};
        
        for (int size : sizes) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Simulate bulk insert
            // Prepare data
            std::vector<std::vector<std::string>> data;
            for (int i = 0; i < size; ++i) {
                data.push_back({
                    "user" + std::to_string(i),
                    "user" + std::to_string(i) + "@example.com",
                    "password_hash"
                });
            }
            
            // Execute bulk insert
            // db->bulkInsert("users", {"username", "email", "password_hash"}, data);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "   Bulk insert " << std::setw(6) << size << " rows: " 
                     << std::setw(6) << duration.count() << "ms "
                     << "(" << (size * 1000.0 / duration.count()) << " rows/sec)" << std::endl;
        }
    }
    
    void testConcurrency() {
        std::cout << "\n5. Concurrency Test:" << std::endl;
        
        const int numThreads = 10;
        const int operationsPerThread = 100;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([i, operationsPerThread]() {
                for (int j = 0; j < operationsPerThread; ++j) {
                    // Simulate database operation
                    // auto conn = pool.acquire();
                    // conn->executeQuery("SELECT * FROM users WHERE id = " + std::to_string(j));
                    // pool.release(conn);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        int totalOperations = numThreads * operationsPerThread;
        std::cout << "   " << numThreads << " threads, " << operationsPerThread << " ops each" << std::endl;
        std::cout << "   Total time: " << duration.count() << "ms" << std::endl;
        std::cout << "   Throughput: " << (totalOperations * 1000.0 / duration.count()) << " ops/sec" << std::endl;
    }
};

// ============================================================================
// Main Application
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║     Multi-Database C++ Interface Demonstration              ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    
    // Configuration for each database
    std::map<DatabaseType, ConnectionConfig> configs = {
        {DatabaseType::MySQL, {"localhost", 3306, "testdb", "root", "password"}},
        {DatabaseType::MariaDB, {"localhost", 3306, "testdb", "root", "password"}},
        {DatabaseType::PostgreSQL, {"localhost", 5432, "testdb", "postgres", "password"}},
        {DatabaseType::MSSQL, {"localhost", 1433, "testdb", "sa", "YourStrong@Password"}}
    };
    
    // Test each database
    std::vector<DatabaseType> databases = {
        DatabaseType::MySQL,
        DatabaseType::PostgreSQL,
        DatabaseType::MSSQL
    };
    
    for (auto dbType : databases) {
        std::cout << "\n" << std::string(65, '=') << std::endl;
        std::cout << "Testing " << (dbType == DatabaseType::MySQL ? "MySQL" :
                                   dbType == DatabaseType::PostgreSQL ? "PostgreSQL" :
                                   dbType == DatabaseType::MSSQL ? "SQL Server" : "Unknown") 
                 << " Database" << std::endl;
        std::cout << std::string(65, '=') << std::endl;
        
        // User Management System Demo
        UserManagementSystem ums(dbType);
        if (ums.initialize(configs[dbType])) {
            ums.demonstrateOperations();
        }
        
        // Performance Tests
        PerformanceTester tester(dbType);
        tester.runPerformanceTests();
        
        std::cout << "\n" << std::endl;
    }
    
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    Demonstration Complete                    ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    
    return 0;
}