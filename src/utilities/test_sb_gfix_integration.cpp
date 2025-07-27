#include "sb_gfix_enhanced.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <thread>

using namespace SBEnhanced;

class GFixIntegrationTest {
private:
    GFixEnhanced gfix;
    std::string test_database_path;
    
public:
    GFixIntegrationTest() : test_database_path("test_gfix_database.fdb") {}
    
    bool runAllTests() {
        std::cout << "ScratchBird Enhanced GFIX Integration Test Suite" << std::endl;
        std::cout << "================================================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBasicInitialization();
        all_passed &= testDatabaseValidation();
        all_passed &= testRepairOperations();
        all_passed &= testSweepOperations();
        all_passed &= testSpecializedOperations();
        all_passed &= testBackupIntegration();
        all_passed &= testProgressMonitoring();
        all_passed &= testErrorHandling();
        all_passed &= testReportGeneration();
        all_passed &= testUtilityFunctions();
        
        std::cout << "\n================================================" << std::endl;
        if (all_passed) {
            std::cout << "✅ All GFIX integration tests PASSED! Enhanced GFIX is ready." << std::endl;
        } else {
            std::cout << "❌ Some GFIX integration tests FAILED! Please review the implementation." << std::endl;
        }
        std::cout << "================================================" << std::endl;
        
        return all_passed;
    }
    
private:
    bool testBasicInitialization() {
        std::cout << "\n🔧 Testing Basic Initialization..." << std::endl;
        
        try {
            // Test construction and basic state
            if (gfix.isOperationActive()) {
                std::cout << "❌ GFIX should not be active initially" << std::endl;
                return false;
            }
            
            // Test error log functionality
            std::vector<std::string> errors = gfix.getErrors();
            if (!errors.empty()) {
                std::cout << "❌ Error log should be empty initially" << std::endl;
                return false;
            }
            
            std::vector<std::string> warnings = gfix.getWarnings();
            if (!warnings.empty()) {
                std::cout << "❌ Warning log should be empty initially" << std::endl;
                return false;
            }
            
            std::string last_error = gfix.getLastError();
            if (!last_error.empty()) {
                std::cout << "❌ Last error should be empty initially" << std::endl;
                return false;
            }
            
            // Test progress monitoring initial state
            MaintenanceProgress progress = gfix.getCurrentProgress();
            if (progress.operation_active) {
                std::cout << "❌ Progress should not be active initially" << std::endl;
                return false;
            }
            
            std::cout << "✅ Basic initialization test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during initialization test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testDatabaseValidation() {
        std::cout << "\n🔧 Testing Database Validation..." << std::endl;
        
        try {
            // Test validation with different severity levels
            std::vector<ValidationSeverity> severities = {
                ValidationSeverity::BASIC,
                ValidationSeverity::NORMAL,
                ValidationSeverity::FULL
            };
            
            for (ValidationSeverity severity : severities) {
                std::cout << "  Testing validation severity: " << static_cast<int>(severity) << std::endl;
                
                ValidationOptions options;
                options.severity = severity;
                options.check_record_fragments = true;
                options.check_blob_integrity = true;
                options.check_index_consistency = true;
                options.continue_on_errors = true;
                options.generate_detailed_report = false;
                
                ValidationResult result;
                bool validation_success = gfix.performDatabaseValidation(test_database_path, options, result);
                
                // Note: Validation may fail for non-existent test database, but should not crash
                std::cout << "    Validation result: " << (validation_success ? "SUCCESS" : "FAILED") << std::endl;
                
                // Test result structure
                if (result.detailed_stats.operation_type != MaintenanceOperation::VALIDATION) {
                    std::cout << "❌ Operation type should be VALIDATION" << std::endl;
                    return false;
                }
            }
            
            // Test validation options
            ValidationOptions test_options;
            test_options.severity = ValidationSeverity::DEEP;
            test_options.check_referential_integrity = true;
            test_options.max_errors_to_report = 500;
            test_options.output_file_path = "test_validation_report.txt";
            
            ValidationResult detailed_result;
            gfix.performDatabaseValidation(test_database_path, test_options, detailed_result);
            
            std::cout << "✅ Database validation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during validation test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testRepairOperations() {
        std::cout << "\n🔧 Testing Repair Operations..." << std::endl;
        
        try {
            // Test repair with different strategies
            std::vector<RepairStrategy> strategies = {
                RepairStrategy::CONSERVATIVE,
                RepairStrategy::AGGRESSIVE,
                RepairStrategy::VALIDATE_ONLY
            };
            
            for (RepairStrategy strategy : strategies) {
                std::cout << "  Testing repair strategy: " << static_cast<int>(strategy) << std::endl;
                
                RepairOptions options;
                options.strategy = strategy;
                options.create_backup_before_repair = false; // Skip backup for test
                options.fix_record_fragments = true;
                options.fix_blob_corruption = true;
                options.rebuild_corrupt_indexes = true;
                options.continue_on_critical_errors = true;
                
                RepairResult result;
                bool repair_success = gfix.performDatabaseRepair(test_database_path, options, result);
                
                std::cout << "    Repair result: " << (repair_success ? "SUCCESS" : "FAILED") << std::endl;
                
                // Test result structure
                if (result.detailed_stats.operation_type != MaintenanceOperation::REPAIR) {
                    std::cout << "❌ Operation type should be REPAIR" << std::endl;
                    return false;
                }
                
                // Test success rate calculation
                double success_rate = result.getRepairSuccessRate();
                if (success_rate < 0.0 || success_rate > 100.0) {
                    std::cout << "❌ Success rate should be between 0 and 100" << std::endl;
                    return false;
                }
            }
            
            // Test backup integration
            RepairOptions backup_options;
            backup_options.create_backup_before_repair = true;
            backup_options.backup_path = "test_repair_backup.sbk";
            
            RepairResult backup_result;
            gfix.performDatabaseRepair(test_database_path, backup_options, backup_result);
            
            std::cout << "✅ Repair operations test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during repair test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testSweepOperations() {
        std::cout << "\n🔧 Testing Sweep Operations..." << std::endl;
        
        try {
            // Test basic sweep
            SweepOptions options;
            options.force_sweep = false;
            options.cooperative_sweep = true;
            options.update_statistics_during_sweep = true;
            options.optimize_record_versions = true;
            options.reclaim_blob_space = true;
            
            MaintenanceStatistics stats;
            bool sweep_success = gfix.performDatabaseSweep(test_database_path, options, stats);
            
            std::cout << "  Basic sweep result: " << (sweep_success ? "SUCCESS" : "FAILED") << std::endl;
            
            // Test statistics structure
            if (stats.operation_type != MaintenanceOperation::SWEEP) {
                std::cout << "❌ Operation type should be SWEEP" << std::endl;
                return false;
            }
            
            // Test duration calculation
            auto duration = stats.getDuration();
            if (duration.count() < 0) {
                std::cout << "❌ Duration should be non-negative" << std::endl;
                return false;
            }
            
            // Test forced sweep
            SweepOptions force_options;
            force_options.force_sweep = true;
            force_options.cooperative_sweep = false;
            force_options.max_sweep_duration_minutes = 5;
            
            MaintenanceStatistics force_stats;
            gfix.performDatabaseSweep(test_database_path, force_options, force_stats);
            
            std::cout << "✅ Sweep operations test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during sweep test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testSpecializedOperations() {
        std::cout << "\n🔧 Testing Specialized Operations..." << std::endl;
        
        try {
            // Test index rebuilding
            std::vector<std::string> index_names = {"TEST_INDEX_1", "TEST_INDEX_2"};
            MaintenanceStatistics index_stats;
            bool index_success = gfix.rebuildIndexes(test_database_path, index_names, index_stats);
            
            std::cout << "  Index rebuild result: " << (index_success ? "SUCCESS" : "FAILED") << std::endl;
            
            if (index_stats.operation_type != MaintenanceOperation::INDEX_REBUILD) {
                std::cout << "❌ Operation type should be INDEX_REBUILD" << std::endl;
                return false;
            }
            
            // Test limbo transaction resolution
            MaintenanceStatistics limbo_stats;
            bool limbo_success = gfix.resolveLimboTransactions(test_database_path, false, limbo_stats);
            
            std::cout << "  Limbo resolution result: " << (limbo_success ? "SUCCESS" : "FAILED") << std::endl;
            
            if (limbo_stats.operation_type != MaintenanceOperation::LIMBO_RESOLUTION) {
                std::cout << "❌ Operation type should be LIMBO_RESOLUTION" << std::endl;
                return false;
            }
            
            // Test statistics update
            MaintenanceStatistics update_stats;
            bool update_success = gfix.updateDatabaseStatistics(test_database_path, true, update_stats);
            
            std::cout << "  Statistics update result: " << (update_success ? "SUCCESS" : "FAILED") << std::endl;
            
            // Test space reclamation
            MaintenanceStatistics space_stats;
            bool space_success = gfix.reclaimUnusedSpace(test_database_path, space_stats);
            
            std::cout << "  Space reclamation result: " << (space_success ? "SUCCESS" : "FAILED") << std::endl;
            
            if (space_stats.operation_type != MaintenanceOperation::SPACE_RECLAIM) {
                std::cout << "❌ Operation type should be SPACE_RECLAIM" << std::endl;
                return false;
            }
            
            std::cout << "✅ Specialized operations test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during specialized operations test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testBackupIntegration() {
        std::cout << "\n🔧 Testing Backup Integration..." << std::endl;
        
        try {
            // Test backup creation
            std::string backup_path = "test_prerepair_backup.sbk";
            bool backup_success = gfix.createPreRepairBackup(test_database_path, backup_path);
            
            std::cout << "  Backup creation result: " << (backup_success ? "SUCCESS" : "FAILED") << std::endl;
            
            // Test backup verification
            bool verify_success = gfix.verifyBackupIntegrity(backup_path);
            
            std::cout << "  Backup verification result: " << (verify_success ? "SUCCESS" : "FAILED") << std::endl;
            
            std::cout << "✅ Backup integration test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during backup integration test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testProgressMonitoring() {
        std::cout << "\n🔧 Testing Progress Monitoring..." << std::endl;
        
        try {
            // Test progress callback functionality
            bool callback_invoked = false;
            std::string last_object;
            
            auto progress_callback = [&](const MaintenanceProgress& progress) {
                callback_invoked = true;
                last_object = progress.current_object;
                
                // Test progress data validity
                if (progress.getProgressPercentage() < 0.0 || progress.getProgressPercentage() > 100.0) {
                    std::cout << "❌ Invalid progress percentage: " << progress.getProgressPercentage() << std::endl;
                }
                
                auto elapsed = progress.getElapsedTime();
                if (elapsed.count() < 0) {
                    std::cout << "❌ Invalid elapsed time: " << elapsed.count() << std::endl;
                }
                
                auto remaining = progress.getEstimatedTimeRemaining();
                // Remaining time can be 0 but not negative
                if (remaining.count() < 0) {
                    std::cout << "❌ Invalid remaining time: " << remaining.count() << std::endl;
                }
            };
            
            // Test with validation operation
            ValidationOptions options;
            options.severity = ValidationSeverity::BASIC;
            options.progress_callback = progress_callback;
            
            ValidationResult result;
            gfix.performDatabaseValidation(test_database_path, options, result);
            
            // Check if callback was invoked
            std::cout << "  Progress callback invoked: " << (callback_invoked ? "YES" : "NO") << std::endl;
            
            // Test operation cancellation
            if (!gfix.isOperationActive()) {
                std::cout << "  Testing operation cancellation (simulated)" << std::endl;
                gfix.cancelCurrentOperation();
                
                if (gfix.isOperationActive()) {
                    std::cout << "❌ Operation should not be active after cancellation" << std::endl;
                    return false;
                }
            }
            
            std::cout << "✅ Progress monitoring test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during progress monitoring test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testErrorHandling() {
        std::cout << "\n🔧 Testing Error Handling..." << std::endl;
        
        try {
            // Test with invalid database path
            std::string invalid_path = "/invalid/nonexistent/database.fdb";
            
            ValidationOptions options;
            options.severity = ValidationSeverity::BASIC;
            options.continue_on_errors = true;
            
            ValidationResult result;
            bool validation_result = gfix.performDatabaseValidation(invalid_path, options, result);
            
            // Should fail gracefully
            std::cout << "  Invalid database validation: " << (validation_result ? "UNEXPECTED SUCCESS" : "EXPECTED FAILURE") << std::endl;
            
            // Check error logging
            std::vector<std::string> errors = gfix.getErrors();
            if (errors.empty()) {
                std::cout << "❌ Errors should have been logged" << std::endl;
                return false;
            }
            
            std::string last_error = gfix.getLastError();
            if (last_error.empty()) {
                std::cout << "❌ Last error should not be empty" << std::endl;
                return false;
            }
            
            std::cout << "  Last error: " << last_error << std::endl;
            
            // Test error log clearing
            gfix.clearErrorLog();
            errors = gfix.getErrors();
            if (!errors.empty()) {
                std::cout << "❌ Error log should be empty after clearing" << std::endl;
                return false;
            }
            
            std::cout << "✅ Error handling test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during error handling test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testReportGeneration() {
        std::cout << "\n🔧 Testing Report Generation..." << std::endl;
        
        try {
            // Create sample data for report testing
            MaintenanceStatistics stats;
            stats.operation_type = MaintenanceOperation::VALIDATION;
            stats.operation_start = std::chrono::steady_clock::now();
            stats.operation_end = stats.operation_start + std::chrono::seconds(10);
            stats.total_pages_processed = 1000;
            stats.total_records_processed = 50000;
            stats.errors_encountered.push_back("Test error 1");
            stats.warnings_generated.push_back("Test warning 1");
            
            // Test maintenance report generation
            std::string maintenance_report = gfix.generateMaintenanceReport(stats);
            if (maintenance_report.empty()) {
                std::cout << "❌ Maintenance report should not be empty" << std::endl;
                return false;
            }
            
            std::cout << "  Maintenance report length: " << maintenance_report.length() << " chars" << std::endl;
            
            // Create sample validation result
            ValidationResult validation_result;
            validation_result.database_structurally_sound = true;
            validation_result.data_integrity_intact = true;
            validation_result.indexes_consistent = true;
            validation_result.total_errors_found = 5;
            validation_result.critical_errors_found = 1;
            validation_result.warnings_generated = 10;
            validation_result.recommendations.push_back("Test recommendation");
            
            // Test validation report generation
            std::string validation_report = gfix.generateValidationReport(validation_result);
            if (validation_report.empty()) {
                std::cout << "❌ Validation report should not be empty" << std::endl;
                return false;
            }
            
            std::cout << "  Validation report length: " << validation_report.length() << " chars" << std::endl;
            
            // Create sample repair result
            RepairResult repair_result;
            repair_result.repair_successful = true;
            repair_result.database_accessible_after_repair = true;
            repair_result.issues_found = 10;
            repair_result.issues_repaired = 8;
            repair_result.issues_unresolved = 2;
            repair_result.backup_path_created = "test_backup.sbk";
            
            // Test repair report generation
            std::string repair_report = gfix.generateRepairReport(repair_result);
            if (repair_report.empty()) {
                std::cout << "❌ Repair report should not be empty" << std::endl;
                return false;
            }
            
            std::cout << "  Repair report length: " << repair_report.length() << " chars" << std::endl;
            
            // Test success rate calculation
            double success_rate = repair_result.getRepairSuccessRate();
            if (success_rate != 80.0) { // 8/10 * 100
                std::cout << "❌ Success rate calculation is incorrect: " << success_rate << std::endl;
                return false;
            }
            
            std::cout << "✅ Report generation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during report generation test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testUtilityFunctions() {
        std::cout << "\n🔧 Testing Utility Functions..." << std::endl;
        
        try {
            // Test database health analysis
            ValidationResult health_report;
            bool health_success = gfix.analyzeDatabaseHealth(test_database_path, health_report);
            
            std::cout << "  Health analysis result: " << (health_success ? "SUCCESS" : "FAILED") << std::endl;
            
            // Test health status methods
            bool is_healthy = health_report.isDatabaseHealthy();
            bool needs_attention = health_report.requiresImmediateAttention();
            
            std::cout << "  Database healthy: " << (is_healthy ? "YES" : "NO") << std::endl;
            std::cout << "  Needs attention: " << (needs_attention ? "YES" : "NO") << std::endl;
            
            // Test performance recommendations
            std::vector<std::string> recommendations;
            bool recommendations_success = gfix.generatePerformanceRecommendations(test_database_path, recommendations);
            
            std::cout << "  Recommendations generated: " << recommendations.size() << std::endl;
            
            // Test repair time estimation
            RepairOptions repair_options;
            repair_options.strategy = RepairStrategy::CONSERVATIVE;
            repair_options.create_backup_before_repair = true;
            
            std::chrono::minutes estimated_duration;
            bool estimation_success = gfix.estimateRepairTime(test_database_path, repair_options, estimated_duration);
            
            std::cout << "  Repair time estimation: " << (estimation_success ? "SUCCESS" : "FAILED") << std::endl;
            if (estimation_success) {
                std::cout << "  Estimated duration: " << estimated_duration.count() << " minutes" << std::endl;
            }
            
            // Test database information
            std::map<std::string, std::string> db_info;
            bool info_success = gfix.getDatabaseInfo(test_database_path, db_info);
            
            std::cout << "  Database info retrieval: " << (info_success ? "SUCCESS" : "FAILED") << std::endl;
            std::cout << "  Info items: " << db_info.size() << std::endl;
            
            // Test database statistics
            std::map<std::string, uint64_t> db_stats;
            bool stats_success = gfix.getDatabaseStatistics(test_database_path, db_stats);
            
            std::cout << "  Database stats retrieval: " << (stats_success ? "SUCCESS" : "FAILED") << std::endl;
            std::cout << "  Stats items: " << db_stats.size() << std::endl;
            
            std::cout << "✅ Utility functions test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during utility functions test: " << e.what() << std::endl;
            return false;
        }
    }
};

// Test utility functions from namespace
bool testNamespaceUtilities() {
    std::cout << "\n🔧 Testing Namespace Utility Functions..." << std::endl;
    
    try {
        std::string test_db = "test_namespace_db.fdb";
        
        // Test quick validation
        ValidationResult quick_result = quickValidation(test_db);
        std::cout << "  Quick validation completed" << std::endl;
        
        // Test quick repair
        RepairResult quick_repair_result = quickRepair(test_db);
        std::cout << "  Quick repair completed" << std::endl;
        
        // Test health check
        bool is_healthy = isDatabaseHealthy(test_db);
        std::cout << "  Database health check: " << (is_healthy ? "HEALTHY" : "UNHEALTHY") << std::endl;
        
        std::cout << "✅ Namespace utility functions test passed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception during namespace utilities test: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    try {
        std::cout << "ScratchBird Enhanced GFIX Integration Test" << std::endl;
        std::cout << "Version: SB-T0.6.0.1 ScratchBird 0.6 f90eae0" << std::endl;
        std::cout << "Testing database maintenance and repair functionality..." << std::endl;
        
        GFixIntegrationTest test;
        bool success = test.runAllTests();
        
        // Test namespace utilities
        success &= testNamespaceUtilities();
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal test error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal test error" << std::endl;
        return 1;
    }
}