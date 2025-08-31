/**
 * Test Execution Order Configuration
 * 
 * This file defines the logical order for running tests based on dependencies.
 * Tests are grouped into phases where each phase must pass before proceeding.
 */

#ifndef TEST_EXECUTION_ORDER_H
#define TEST_EXECUTION_ORDER_H

#include <vector>
#include <string>
#include <map>

namespace TestOrder {

/**
 * PHASE 1: FOUNDATION
 * These must work before anything else makes sense
 */
struct Phase1_Foundation {
    static constexpr const char* name = "Foundation";
    static constexpr const char* description = 
        "Basic executable and file system operations - without these, nothing else matters";
    
    static std::vector<std::string> tests() {
        return {
            // First, verify the executable does something
            "CoreDatabaseTest.MainExecutableStartsServer",
            
            // Then verify it can create files
            "CoreDatabaseTest.DatabaseCreationCreatesPersistentFiles",
            
            // Basic storage layer must exist
            "StorageTest.SegmentFileManagement"
        };
    }
    
    static bool is_blocking() { return true; }  // Must pass to continue
};

/**
 * PHASE 2: BASIC PERSISTENCE
 * Once we have files, verify data can be stored and retrieved
 */
struct Phase2_BasicPersistence {
    static constexpr const char* name = "Basic Persistence";
    static constexpr const char* description = 
        "Data must persist to disk and survive restart";
    
    static std::vector<std::string> tests() {
        return {
            // Can we write and read back data?
            "StorageTest.PageConsistencyAndChecksums",
            
            // Does data survive process restart?
            "CoreDatabaseTest.DatabaseSurvivesProcessRestart",
            
            // Basic WAL functionality
            "StorageTest.WriteAheadLoggingWorks"
        };
    }
    
    static bool is_blocking() { return true; }
};

/**
 * PHASE 3: BASIC SQL OPERATIONS
 * With persistence working, test basic SQL
 */
struct Phase3_BasicSQL {
    static constexpr const char* name = "Basic SQL Operations";
    static constexpr const char* description = 
        "CRUD operations must work before testing advanced features";
    
    static std::vector<std::string> tests() {
        return {
            // Basic CRUD
            "CoreDatabaseTest.BasicCRUDOperations",
            
            // Can handle reasonable data volumes
            "CoreDatabaseTest.LargeDatasetHandling",
            
            // Basic error handling
            "IntegrationTest.ErrorHandlingAndRecovery"
        };
    }
    
    static bool is_blocking() { return true; }
};

/**
 * PHASE 4: TRANSACTIONS
 * With basic SQL working, test transaction support
 */
struct Phase4_Transactions {
    static constexpr const char* name = "Transaction Support";
    static constexpr const char* description = 
        "ACID properties required before testing concurrent operations";
    
    static std::vector<std::string> tests() {
        return {
            // Basic transaction atomicity
            "CoreDatabaseTest.TransactionAtomicity",
            
            // Crash recovery
            "StorageTest.CrashRecoveryWorks",
            
            // Durability guarantees
            "StorageTest.DurabilityGuarantees",
            
            // MVCC basics
            "StorageTest.MVCCImplementation"
        };
    }
    
    static bool is_blocking() { return true; }
};

/**
 * PHASE 5: INDEXING
 * With data storage working, test indexing
 */
struct Phase5_Indexing {
    static constexpr const char* name = "Index Support";
    static constexpr const char* description = 
        "Indexes needed for performance and constraint enforcement";
    
    static std::vector<std::string> tests() {
        return {
            // Basic index creation and usage
            "CoreDatabaseTest.IndexesImprovePerformance",
            
            // Various index types if claimed
            "IntegrationTest.SQLComplianceTestSuite"  // Includes index tests
        };
    }
    
    static bool is_blocking() { return false; }  // Can continue without perfect indexing
};

/**
 * PHASE 6: CONCURRENCY (BASIC)
 * With transactions working, test basic concurrent operations
 */
struct Phase6_BasicConcurrency {
    static constexpr const char* name = "Basic Concurrency";
    static constexpr const char* description = 
        "Single-threaded operations must work before testing multi-threaded";
    
    static std::vector<std::string> tests() {
        return {
            // No data loss under concurrent inserts
            "ConcurrencyTest.ConcurrentInsertsNoDataLoss",
            
            // Basic locking works
            "ConcurrencyTest.ReaderWriterLockCorrectness"
        };
    }
    
    static bool is_blocking() { return true; }
};

/**
 * PHASE 7: SECURITY BASICS
 * With a working database, add security
 */
struct Phase7_SecurityBasics {
    static constexpr const char* name = "Basic Security";
    static constexpr const char* description = 
        "Fix critical security issues before adding features";
    
    static std::vector<std::string> tests() {
        return {
            // CRITICAL: No MD5 passwords
            "SecurityTest.PasswordHashingNotMD5",
            "SecurityTest.ActualBcryptNotFakePBKDF2",
            
            // CRITICAL: SQL injection prevention
            "SecurityTest.SQLInjectionPrevention",
            
            // Input validation
            "SecurityTest.InputValidationAndSanitization"
        };
    }
    
    static bool is_blocking() { return true; }  // Security is mandatory
};

/**
 * PHASE 8: ACCESS CONTROL
 * With basic security, add access control
 */
struct Phase8_AccessControl {
    static constexpr const char* name = "Access Control";
    static constexpr const char* description = 
        "Permission system must actually work";
    
    static std::vector<std::string> tests() {
        return {
            // Permissions must be checked
            "SecurityTest.PermissionSystemActuallyWorks",
            
            // Timing attack prevention
            "SecurityTest.PasswordVerificationResistantToTimingAttacks"
        };
    }
    
    static bool is_blocking() { return true; }
};

/**
 * PHASE 9: ADVANCED CONCURRENCY
 * With basic concurrency working, test advanced scenarios
 */
struct Phase9_AdvancedConcurrency {
    static constexpr const char* name = "Advanced Concurrency";
    static constexpr const char* description = 
        "Deadlock detection, connection pools, etc.";
    
    static std::vector<std::string> tests() {
        return {
            // Deadlock handling
            "ConcurrencyTest.DeadlockDetectionAndResolution",
            
            // Connection pool safety
            "ConcurrencyTest.ConnectionPoolThreadSafety",
            
            // Prepared statement cache
            "ConcurrencyTest.PreparedStatementCacheThreadSafety",
            
            // Global state protection
            "ConcurrencyTest.GlobalStateRaceConditions",
            
            // Stress testing
            "ConcurrencyTest.StressTestMixedOperations"
        };
    }
    
    static bool is_blocking() { return false; }
};

/**
 * PHASE 10: AUDIT AND COMPLIANCE
 * With security working, add audit trails
 */
struct Phase10_AuditCompliance {
    static constexpr const char* name = "Audit and Compliance";
    static constexpr const char* description = 
        "Audit logs and compliance features";
    
    static std::vector<std::string> tests() {
        return {
            // Audit must persist
            "SecurityTest.AuditLogSecurityAndPersistence",
            
            // GDPR compliance
            "ComplianceTest.GDPR_RightToErasure",
            "ComplianceTest.GDPR_DataPortability",
            "ComplianceTest.GDPR_ConsentTracking",
            
            // HIPAA compliance
            "ComplianceTest.HIPAA_EncryptionAtRest",
            "ComplianceTest.HIPAA_AccessLogging",
            "ComplianceTest.HIPAA_MinimumNecessary",
            
            // PCI DSS compliance
            "ComplianceTest.PCIDSS_CardDataProtection",
            "ComplianceTest.PCIDSS_KeyManagement",
            
            // SOX compliance
            "ComplianceTest.SOX_ImmutableAuditTrail",
            "ComplianceTest.SOX_FinancialDataRetention"
        };
    }
    
    static bool is_blocking() { return false; }
};

/**
 * PHASE 11: ADVANCED SECURITY
 * Additional security features
 */
struct Phase11_AdvancedSecurity {
    static constexpr const char* name = "Advanced Security";
    static constexpr const char* description = 
        "2FA, TLS, and other advanced security features";
    
    static std::vector<std::string> tests() {
        return {
            // Two-factor authentication
            "SecurityTest.TwoFactorAuthenticationSecurity",
            
            // TLS configuration
            "SecurityTest.ConnectionSecurityAndTLS"
        };
    }
    
    static bool is_blocking() { return false; }
};

/**
 * PHASE 12: CLIENT-SERVER
 * With core working, test network functionality
 */
struct Phase12_ClientServer {
    static constexpr const char* name = "Client-Server Mode";
    static constexpr const char* description = 
        "Network server functionality";
    
    static std::vector<std::string> tests() {
        return {
            // Server mode
            "IntegrationTest.ClientServerMode"
        };
    }
    
    static bool is_blocking() { return false; }
};

/**
 * PHASE 13: TOOLS
 * With database working, test tools
 */
struct Phase13_Tools {
    static constexpr const char* name = "Database Tools";
    static constexpr const char* description = 
        "isql and other tools";
    
    static std::vector<std::string> tests() {
        return {
            // isql tool
            "IntegrationTest.ISQLToolFunctionality",
            
            // Complete lifecycle with backup
            "IntegrationTest.CompleteDatabaseLifecycle"
        };
    }
    
    static bool is_blocking() { return false; }
};

/**
 * PHASE 14: PERFORMANCE
 * Only test performance after functionality works
 */
struct Phase14_Performance {
    static constexpr const char* name = "Performance";
    static constexpr const char* description = 
        "Performance benchmarks - only meaningful if database works";
    
    static std::vector<std::string> tests() {
        return {
            // Performance benchmarks
            "IntegrationTest.PerformanceBenchmarks",
            
            // Storage efficiency
            "StorageTest.StorageSpaceManagement",
            
            // Large objects
            "StorageTest.LargeObjectStorage"
        };
    }
    
    static bool is_blocking() { return false; }
};

/**
 * Test Phase Manager
 */
class PhaseManager {
public:
    static std::vector<std::string> get_execution_order() {
        std::vector<std::string> order;
        
        // Add phases in dependency order
        add_phase<Phase1_Foundation>(order);
        add_phase<Phase2_BasicPersistence>(order);
        add_phase<Phase3_BasicSQL>(order);
        add_phase<Phase4_Transactions>(order);
        add_phase<Phase5_Indexing>(order);
        add_phase<Phase6_BasicConcurrency>(order);
        add_phase<Phase7_SecurityBasics>(order);
        add_phase<Phase8_AccessControl>(order);
        add_phase<Phase9_AdvancedConcurrency>(order);
        add_phase<Phase10_AuditCompliance>(order);
        add_phase<Phase11_AdvancedSecurity>(order);
        add_phase<Phase12_ClientServer>(order);
        add_phase<Phase13_Tools>(order);
        add_phase<Phase14_Performance>(order);
        
        return order;
    }
    
    static std::map<int, std::string> get_phase_descriptions() {
        return {
            {1, Phase1_Foundation::description},
            {2, Phase2_BasicPersistence::description},
            {3, Phase3_BasicSQL::description},
            {4, Phase4_Transactions::description},
            {5, Phase5_Indexing::description},
            {6, Phase6_BasicConcurrency::description},
            {7, Phase7_SecurityBasics::description},
            {8, Phase8_AccessControl::description},
            {9, Phase9_AdvancedConcurrency::description},
            {10, Phase10_AuditCompliance::description},
            {11, Phase11_AdvancedSecurity::description},
            {12, Phase12_ClientServer::description},
            {13, Phase13_Tools::description},
            {14, Phase14_Performance::description}
        };
    }
    
    static std::vector<int> get_blocking_phases() {
        return {1, 2, 3, 4, 6, 7, 8};  // Phases that must pass to continue
    }
    
private:
    template<typename Phase>
    static void add_phase(std::vector<std::string>& order) {
        auto tests = Phase::tests();
        order.insert(order.end(), tests.begin(), tests.end());
    }
};

} // namespace TestOrder

#endif // TEST_EXECUTION_ORDER_H