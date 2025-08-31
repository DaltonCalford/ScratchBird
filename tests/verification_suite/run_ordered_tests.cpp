/**
 * Ordered Test Runner for ScratchBird Verification Suite
 * 
 * This runner executes tests in logical dependency order, stopping at blocking
 * phases if they fail. This prevents wasting time testing features that can't
 * possibly work without foundational components.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include "test_execution_order.h"

class OrderedTestRunner {
public:
    OrderedTestRunner() : current_phase_(1), tests_run_(0), tests_passed_(0) {}
    
    int run() {
        print_header();
        
        // Phase 1: Foundation
        if (!run_phase(1, "Foundation", {
            "CoreDatabaseTest.MainExecutableStartsServer",
            "CoreDatabaseTest.DatabaseCreationCreatesPersistentFiles",
            "StorageTest.SegmentFileManagement"
        }, true)) {
            return print_early_termination(1);
        }
        
        // Phase 2: Basic Persistence
        if (!run_phase(2, "Basic Persistence", {
            "StorageTest.PageConsistencyAndChecksums",
            "CoreDatabaseTest.DatabaseSurvivesProcessRestart",
            "StorageTest.WriteAheadLoggingWorks"
        }, true)) {
            return print_early_termination(2);
        }
        
        // Phase 3: Basic SQL Operations
        if (!run_phase(3, "Basic SQL Operations", {
            "CoreDatabaseTest.BasicCRUDOperations",
            "CoreDatabaseTest.LargeDatasetHandling",
            "IntegrationTest.ErrorHandlingAndRecovery"
        }, true)) {
            return print_early_termination(3);
        }
        
        // Phase 4: Transactions
        if (!run_phase(4, "Transaction Support", {
            "CoreDatabaseTest.TransactionAtomicity",
            "StorageTest.CrashRecoveryWorks",
            "StorageTest.DurabilityGuarantees",
            "StorageTest.MVCCImplementation"
        }, true)) {
            return print_early_termination(4);
        }
        
        // Phase 5: Indexing (non-blocking)
        run_phase(5, "Index Support", {
            "CoreDatabaseTest.IndexesImprovePerformance",
            "IntegrationTest.SQLComplianceTestSuite"
        }, false);
        
        // Phase 6: Basic Concurrency
        if (!run_phase(6, "Basic Concurrency", {
            "ConcurrencyTest.ConcurrentInsertsNoDataLoss",
            "ConcurrencyTest.ReaderWriterLockCorrectness"
        }, true)) {
            return print_early_termination(6);
        }
        
        // Phase 7: Security Basics
        if (!run_phase(7, "Basic Security", {
            "SecurityTest.PasswordHashingNotMD5",
            "SecurityTest.ActualBcryptNotFakePBKDF2",
            "SecurityTest.SQLInjectionPrevention",
            "SecurityTest.InputValidationAndSanitization"
        }, true)) {
            return print_early_termination(7);
        }
        
        // Phase 8: Access Control
        if (!run_phase(8, "Access Control", {
            "SecurityTest.PermissionSystemActuallyWorks",
            "SecurityTest.PasswordVerificationResistantToTimingAttacks"
        }, true)) {
            return print_early_termination(8);
        }
        
        // Phase 9: Advanced Concurrency (non-blocking)
        run_phase(9, "Advanced Concurrency", {
            "ConcurrencyTest.DeadlockDetectionAndResolution",
            "ConcurrencyTest.ConnectionPoolThreadSafety",
            "ConcurrencyTest.PreparedStatementCacheThreadSafety",
            "ConcurrencyTest.GlobalStateRaceConditions",
            "ConcurrencyTest.StressTestMixedOperations"
        }, false);
        
        // Phase 10: Audit and Compliance (non-blocking)
        run_phase(10, "Audit and Compliance", {
            "SecurityTest.AuditLogSecurityAndPersistence",
            "ComplianceTest.GDPR_RightToErasure",
            "ComplianceTest.GDPR_DataPortability",
            "ComplianceTest.GDPR_ConsentTracking",
            "ComplianceTest.HIPAA_EncryptionAtRest",
            "ComplianceTest.HIPAA_AccessLogging",
            "ComplianceTest.HIPAA_MinimumNecessary",
            "ComplianceTest.PCIDSS_CardDataProtection",
            "ComplianceTest.PCIDSS_KeyManagement",
            "ComplianceTest.SOX_ImmutableAuditTrail",
            "ComplianceTest.SOX_FinancialDataRetention"
        }, false);
        
        // Phase 11: Advanced Security (non-blocking)
        run_phase(11, "Advanced Security", {
            "SecurityTest.TwoFactorAuthenticationSecurity",
            "SecurityTest.ConnectionSecurityAndTLS"
        }, false);
        
        // Phase 12: Client-Server (non-blocking)
        run_phase(12, "Client-Server Mode", {
            "IntegrationTest.ClientServerMode"
        }, false);
        
        // Phase 13: Tools (non-blocking)
        run_phase(13, "Database Tools", {
            "IntegrationTest.ISQLToolFunctionality",
            "IntegrationTest.CompleteDatabaseLifecycle"
        }, false);
        
        // Phase 14: Performance (non-blocking)
        run_phase(14, "Performance", {
            "IntegrationTest.PerformanceBenchmarks",
            "StorageTest.StorageSpaceManagement",
            "StorageTest.LargeObjectStorage"
        }, false);
        
        return print_final_report();
    }
    
private:
    int current_phase_;
    int tests_run_;
    int tests_passed_;
    std::vector<PhaseResult> phase_results_;
    
    struct PhaseResult {
        int phase_number;
        std::string phase_name;
        int tests_total;
        int tests_passed;
        bool blocking;
        bool passed;
        std::vector<std::string> failed_tests;
    };
    
    void print_header() {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║        SCRATCHBIRD ORDERED VERIFICATION TEST SUITE            ║\n";
        std::cout << "║                                                                ║\n";
        std::cout << "║  Tests run in dependency order with blocking phases           ║\n";
        std::cout << "║  Blocking phases must pass to continue testing                ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }
    
    bool run_phase(int phase_num, const std::string& phase_name, 
                   const std::vector<std::string>& tests, bool blocking) {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "PHASE " << phase_num << ": " << phase_name;
        if (blocking) {
            std::cout << " [BLOCKING]";
        }
        std::cout << "\n" << std::string(70, '-') << "\n";
        
        PhaseResult result;
        result.phase_number = phase_num;
        result.phase_name = phase_name;
        result.tests_total = tests.size();
        result.tests_passed = 0;
        result.blocking = blocking;
        
        for (const auto& test : tests) {
            std::cout << "Running: " << test << "... ";
            
            // Create filter for specific test
            ::testing::GTEST_FLAG(filter) = test;
            
            // Capture test result
            ::testing::TestEventListeners& listeners = 
                ::testing::UnitTest::GetInstance()->listeners();
            
            // Run the specific test
            int test_result = RUN_ALL_TESTS();
            
            tests_run_++;
            
            if (test_result == 0) {
                std::cout << "✅ PASS\n";
                tests_passed_++;
                result.tests_passed++;
            } else {
                std::cout << "❌ FAIL\n";
                result.failed_tests.push_back(test);
                
                // For blocking phases, ask if we should continue
                if (blocking) {
                    std::cout << "\n⚠️  BLOCKING TEST FAILED!\n";
                }
            }
        }
        
        // Phase summary
        result.passed = (result.tests_passed == result.tests_total);
        phase_results_.push_back(result);
        
        std::cout << "\nPhase " << phase_num << " Summary: " 
                  << result.tests_passed << "/" << result.tests_total << " passed\n";
        
        if (blocking && !result.passed) {
            std::cout << "\n🛑 STOPPING: Blocking phase failed. "
                      << "Fix these issues before continuing.\n";
            return false;
        }
        
        return true;
    }
    
    int print_early_termination(int failed_phase) {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "EARLY TERMINATION AT PHASE " << failed_phase << "\n";
        std::cout << std::string(70, '=') << "\n\n";
        
        std::cout << "❌ CRITICAL FAILURE: Phase " << failed_phase 
                  << " is a blocking phase that must pass.\n\n";
        
        std::cout << "Failed tests that MUST be fixed:\n";
        for (const auto& phase : phase_results_) {
            if (phase.phase_number == failed_phase) {
                for (const auto& test : phase.failed_tests) {
                    std::cout << "  - " << test << "\n";
                }
            }
        }
        
        std::cout << "\nRecommendation: Fix these foundational issues before proceeding.\n";
        std::cout << "The database cannot function without these basic capabilities.\n\n";
        
        print_progress_bar();
        
        return 1;  // Failure
    }
    
    int print_final_report() {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "FINAL VERIFICATION REPORT\n";
        std::cout << std::string(70, '=') << "\n\n";
        
        // Overall statistics
        std::cout << "Total Tests Run: " << tests_run_ << "\n";
        std::cout << "Total Tests Passed: " << tests_passed_ << "\n";
        double pass_rate = (tests_run_ > 0) ? 
            (100.0 * tests_passed_ / tests_run_) : 0;
        std::cout << "Overall Pass Rate: " << std::fixed << std::setprecision(1) 
                  << pass_rate << "%\n\n";
        
        // Phase breakdown
        std::cout << "PHASE BREAKDOWN:\n";
        std::cout << std::string(70, '-') << "\n";
        
        for (const auto& phase : phase_results_) {
            std::cout << "Phase " << std::setw(2) << phase.phase_number 
                      << " - " << std::setw(25) << std::left << phase.phase_name;
            
            if (phase.passed) {
                std::cout << " ✅ PASSED";
            } else {
                std::cout << " ❌ FAILED";
            }
            
            std::cout << " (" << phase.tests_passed << "/" 
                      << phase.tests_total << ")";
            
            if (phase.blocking && !phase.passed) {
                std::cout << " [BLOCKER]";
            }
            
            std::cout << "\n";
        }
        
        std::cout << "\n";
        print_progress_bar();
        
        // Final verdict
        std::cout << "\nVERDICT: ";
        
        if (pass_rate >= 95) {
            std::cout << "✅ PRODUCTION READY\n";
            std::cout << "The database meets all critical requirements.\n";
        } else if (pass_rate >= 80) {
            std::cout << "🟡 MOSTLY FUNCTIONAL\n";
            std::cout << "Core features work but some issues remain.\n";
        } else if (pass_rate >= 60) {
            std::cout << "🟠 PARTIALLY WORKING\n";
            std::cout << "Basic functionality exists but major gaps remain.\n";
        } else if (pass_rate >= 40) {
            std::cout << "🔴 SEVERELY BROKEN\n";
            std::cout << "Some components work but the system is not usable.\n";
        } else if (pass_rate >= 20) {
            std::cout << "💀 NON-FUNCTIONAL\n";
            std::cout << "This is not a working database system.\n";
        } else {
            std::cout << "☠️  COMPLETE FAILURE\n";
            std::cout << "Nothing works. This is not even close to a database.\n";
        }
        
        // Identify critical missing features
        std::cout << "\nCRITICAL ISSUES:\n";
        bool has_critical = false;
        
        for (const auto& phase : phase_results_) {
            if (phase.blocking && !phase.passed) {
                has_critical = true;
                std::cout << "  ❌ " << phase.phase_name 
                          << " - MUST BE FIXED\n";
            }
        }
        
        if (!has_critical) {
            std::cout << "  ✅ All critical phases passed\n";
        }
        
        return (pass_rate >= 95) ? 0 : 1;
    }
    
    void print_progress_bar() {
        std::cout << "\nPROGRESS TO WORKING DATABASE:\n";
        
        // Calculate progress through phases
        int total_phases = 14;
        int blocking_phases_passed = 0;
        int total_blocking = 8;
        
        for (const auto& phase : phase_results_) {
            if (phase.blocking && phase.passed) {
                blocking_phases_passed++;
            }
        }
        
        double progress = (100.0 * blocking_phases_passed / total_blocking);
        
        std::cout << "[";
        int bar_width = 50;
        int filled = static_cast<int>(bar_width * progress / 100);
        
        for (int i = 0; i < bar_width; i++) {
            if (i < filled) {
                std::cout << "█";
            } else {
                std::cout << "░";
            }
        }
        
        std::cout << "] " << std::fixed << std::setprecision(1) 
                  << progress << "%\n";
        
        std::cout << "Blocking Phases Passed: " << blocking_phases_passed 
                  << "/" << total_blocking << "\n";
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "\nInitializing test environment...\n";
    
    OrderedTestRunner runner;
    return runner.run();
}