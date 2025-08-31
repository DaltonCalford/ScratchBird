/**
 * Master Test Runner for ScratchBird Verification Suite
 * 
 * This runner executes all verification tests and provides a comprehensive
 * report on what actually works vs what is claimed to work.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <map>
#include <vector>

// Custom test listener to track results
class VerificationTestListener : public ::testing::TestEventListener {
public:
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        current_test_ = std::string(test_info.test_suite_name()) + "." + 
                       test_info.name();
        start_time_ = std::chrono::steady_clock::now();
    }
    
    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>
                       (end_time - start_time_).count();
        
        TestResult result;
        result.name = current_test_;
        result.passed = test_info.result()->Passed();
        result.duration_ms = duration;
        
        if (!result.passed && test_info.result()->Failed()) {
            for (int i = 0; i < test_info.result()->total_part_count(); i++) {
                const auto& part = test_info.result()->GetTestPartResult(i);
                if (part.failed()) {
                    result.failure_message = part.message();
                    break;
                }
            }
        }
        
        results_.push_back(result);
        
        // Categorize the test
        if (current_test_.find("Core") != std::string::npos) {
            category_results_["Core"].push_back(result);
        } else if (current_test_.find("Security") != std::string::npos) {
            category_results_["Security"].push_back(result);
        } else if (current_test_.find("Storage") != std::string::npos) {
            category_results_["Storage"].push_back(result);
        } else if (current_test_.find("Concurrency") != std::string::npos) {
            category_results_["Concurrency"].push_back(result);
        } else if (current_test_.find("Integration") != std::string::npos) {
            category_results_["Integration"].push_back(result);
        }
    }
    
    void OnTestProgramEnd(const ::testing::UnitTest& unit_test) override {
        GenerateReport();
    }
    
private:
    struct TestResult {
        std::string name;
        bool passed;
        long duration_ms;
        std::string failure_message;
    };
    
    std::string current_test_;
    std::chrono::steady_clock::time_point start_time_;
    std::vector<TestResult> results_;
    std::map<std::string, std::vector<TestResult>> category_results_;
    
    void GenerateReport() {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "SCRATCHBIRD VERIFICATION SUITE RESULTS\n";
        std::cout << std::string(80, '=') << "\n\n";
        
        // Overall statistics
        int total_tests = results_.size();
        int passed_tests = 0;
        int failed_tests = 0;
        long total_duration = 0;
        
        for (const auto& result : results_) {
            if (result.passed) passed_tests++;
            else failed_tests++;
            total_duration += result.duration_ms;
        }
        
        double pass_rate = (total_tests > 0) ? 
            (100.0 * passed_tests / total_tests) : 0;
        
        std::cout << "SUMMARY:\n";
        std::cout << "--------\n";
        std::cout << "Total Tests: " << total_tests << "\n";
        std::cout << "Passed: " << passed_tests << " (" 
                  << std::fixed << std::setprecision(1) << pass_rate << "%)\n";
        std::cout << "Failed: " << failed_tests << "\n";
        std::cout << "Total Duration: " << total_duration << "ms\n\n";
        
        // Critical failures (Core functionality)
        std::cout << "CRITICAL FAILURES (Must work for basic database):\n";
        std::cout << std::string(50, '-') << "\n";
        
        std::vector<std::string> critical_tests = {
            "CoreDatabaseTest.MainExecutableStartsServer",
            "CoreDatabaseTest.DatabaseCreationCreatesPersistentFiles",
            "CoreDatabaseTest.DatabaseSurvivesProcessRestart",
            "CoreDatabaseTest.BasicCRUDOperations",
            "CoreDatabaseTest.TransactionAtomicity"
        };
        
        for (const auto& test_name : critical_tests) {
            auto it = std::find_if(results_.begin(), results_.end(),
                [&](const TestResult& r) { return r.name == test_name; });
            
            if (it != results_.end()) {
                if (!it->passed) {
                    std::cout << "❌ " << test_name << "\n";
                    std::cout << "   Reason: " << it->failure_message << "\n";
                } else {
                    std::cout << "✅ " << test_name << "\n";
                }
            } else {
                std::cout << "⚠️  " << test_name << " - NOT TESTED\n";
            }
        }
        
        std::cout << "\n";
        
        // Security vulnerabilities
        std::cout << "SECURITY VULNERABILITIES:\n";
        std::cout << std::string(50, '-') << "\n";
        
        std::vector<std::string> security_tests = {
            "SecurityTest.PasswordHashingNotMD5",
            "SecurityTest.ActualBcryptNotFakePBKDF2",
            "SecurityTest.PermissionSystemActuallyWorks",
            "SecurityTest.SQLInjectionPrevention",
            "SecurityTest.AuditLogSecurityAndPersistence"
        };
        
        for (const auto& test_name : security_tests) {
            auto it = std::find_if(results_.begin(), results_.end(),
                [&](const TestResult& r) { return r.name == test_name; });
            
            if (it != results_.end() && !it->passed) {
                std::cout << "🔴 VULNERABILITY: " << test_name << "\n";
            }
        }
        
        std::cout << "\n";
        
        // Category breakdown
        std::cout << "CATEGORY BREAKDOWN:\n";
        std::cout << std::string(50, '-') << "\n";
        
        for (const auto& [category, cat_results] : category_results_) {
            int cat_passed = 0;
            int cat_total = cat_results.size();
            
            for (const auto& result : cat_results) {
                if (result.passed) cat_passed++;
            }
            
            double cat_pass_rate = (cat_total > 0) ? 
                (100.0 * cat_passed / cat_total) : 0;
            
            std::cout << category << ": " << cat_passed << "/" << cat_total
                     << " (" << std::fixed << std::setprecision(1) 
                     << cat_pass_rate << "%)\n";
        }
        
        std::cout << "\n";
        
        // Failed tests details
        if (failed_tests > 0) {
            std::cout << "FAILED TESTS DETAILS:\n";
            std::cout << std::string(50, '-') << "\n";
            
            for (const auto& result : results_) {
                if (!result.passed) {
                    std::cout << "❌ " << result.name << "\n";
                    std::cout << "   " << result.failure_message << "\n\n";
                }
            }
        }
        
        // Final verdict
        std::cout << std::string(80, '=') << "\n";
        std::cout << "FINAL VERDICT:\n";
        std::cout << std::string(80, '=') << "\n";
        
        if (pass_rate >= 95) {
            std::cout << "✅ PRODUCTION READY - All critical tests passed\n";
        } else if (pass_rate >= 80) {
            std::cout << "⚠️  MOSTLY FUNCTIONAL - Some issues need fixing\n";
        } else if (pass_rate >= 50) {
            std::cout << "🟡 PARTIALLY IMPLEMENTED - Major work needed\n";
        } else if (pass_rate >= 20) {
            std::cout << "🔴 SEVERELY BROKEN - Most features don't work\n";
        } else {
            std::cout << "💀 NON-FUNCTIONAL - This is not a database\n";
        }
        
        // Write detailed report to file
        std::ofstream report("verification_report.txt");
        report << "ScratchBird Verification Suite Report\n";
        report << "Generated: " << std::chrono::system_clock::now() << "\n\n";
        
        for (const auto& result : results_) {
            report << (result.passed ? "PASS" : "FAIL") << " | "
                  << result.name << " | "
                  << result.duration_ms << "ms";
            if (!result.passed) {
                report << " | " << result.failure_message;
            }
            report << "\n";
        }
        
        report.close();
        std::cout << "\nDetailed report written to: verification_report.txt\n";
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Add custom listener
    ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();
    
    // Remove default listener and add ours
    delete listeners.Release(listeners.default_result_printer());
    listeners.Append(new VerificationTestListener());
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     SCRATCHBIRD COMPREHENSIVE VERIFICATION SUITE          ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  This test suite verifies actual vs claimed functionality ║\n";
    std::cout << "║  and exposes all implementation issues and vulnerabilities║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    std::cout << "Starting verification tests...\n\n";
    
    int result = RUN_ALL_TESTS();
    
    if (result == 0) {
        std::cout << "\n✅ All tests passed - but check the report for details\n";
    } else {
        std::cout << "\n❌ Tests failed - see report above for details\n";
    }
    
    return result;
}