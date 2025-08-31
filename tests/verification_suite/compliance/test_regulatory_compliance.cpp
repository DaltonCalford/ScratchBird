/**
 * Regulatory Compliance Tests
 * 
 * These tests verify compliance with GDPR, HIPAA, PCI DSS, and SOX requirements.
 * They ensure the database meets legal and regulatory standards.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <set>
#include "scratchbird/engine.h"
#include "scratchbird/audit/audit_engine.h"

namespace fs = std::filesystem;

class ComplianceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "compliance_test";
        fs::create_directories(test_dir);
        
        scratchbird::Status status;
        db = scratchbird::create_database(test_dir / "test.db", {}, status);
    }
    
    void TearDown() override {
        if (db) scratchbird::close_database(db);
        fs::remove_all(test_dir);
    }
    
    fs::path test_dir;
    std::shared_ptr<scratchbird::Database> db;
};

// GDPR Compliance Tests
TEST_F(ComplianceTest, GDPR_RightToErasure) {
    // GDPR Article 17 - Right to erasure ("right to be forgotten")
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create user data
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT, "
        "personal_data TEXT, created_at TIMESTAMP)", status), {});
    
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO users VALUES (1, 'John Doe', 'john@example.com', "
        "'Sensitive personal information', datetime('now'))", status), {});
    
    // Request deletion
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "DELETE FROM users WHERE id = 1", status), {});
    ASSERT_EQ(result.code, scratchbird::StatusCode::Ok);
    
    // Verify complete deletion (including from indexes, logs, backups)
    result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM users WHERE id = 1", status), {});
    EXPECT_TRUE(result.rows.empty()) << "User data not deleted from main table";
    
    // Check audit logs still record the deletion (but not the data)
    auto& audit = scratchbird::audit::AuditEngine::instance();
    auto events = audit.recent(10);
    
    bool deletion_logged = false;
    for (const auto& event : events) {
        if (event.operation == "DELETE" && event.object == "users") {
            deletion_logged = true;
            // Audit should NOT contain the actual deleted data
            EXPECT_TRUE(event.detail.find("john@example.com") == std::string::npos)
                << "Personal data found in audit logs after deletion - GDPR violation!";
        }
    }
    EXPECT_TRUE(deletion_logged) << "Deletion not logged in audit trail";
    
    // Verify data is also removed from any cache/temp files
    for (const auto& entry : fs::directory_iterator(test_dir)) {
        if (entry.path().extension() == ".tmp" || 
            entry.path().extension() == ".cache") {
            std::ifstream file(entry.path());
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            EXPECT_TRUE(content.find("john@example.com") == std::string::npos)
                << "Personal data found in temp file: " << entry.path();
        }
    }
}

TEST_F(ComplianceTest, GDPR_DataPortability) {
    // GDPR Article 20 - Right to data portability
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create user data
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE user_profile (user_id INTEGER, attribute TEXT, value TEXT)", status), {});
    
    for (int i = 0; i < 10; i++) {
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO user_profile VALUES (1, ?, ?)", status),
            {"attr" + std::to_string(i), "value" + std::to_string(i)});
    }
    
    // Export user data in machine-readable format
    auto export_result = scratchbird::export_user_data(session, 1, 
        scratchbird::ExportFormat::JSON, status);
    
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok)
        << "Failed to export user data for GDPR compliance";
    
    // Verify export is complete and machine-readable
    EXPECT_FALSE(export_result.data.empty());
    EXPECT_TRUE(export_result.format == "application/json" ||
                export_result.format == "text/csv")
        << "Export format not machine-readable";
}

TEST_F(ComplianceTest, GDPR_ConsentTracking) {
    // GDPR requires explicit consent tracking
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // System should have consent tracking table
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM information_schema.tables WHERE table_name = 'consent_log'", status), {});
    
    EXPECT_FALSE(result.rows.empty())
        << "No consent tracking mechanism found - GDPR requires consent logs";
}

// HIPAA Compliance Tests
TEST_F(ComplianceTest, HIPAA_EncryptionAtRest) {
    // HIPAA requires encryption of PHI at rest
    fs::path db_file = test_dir / "test.db.seg0";
    
    // Check if database file is encrypted
    std::ifstream file(db_file, std::ios::binary);
    std::vector<uint8_t> header(1024);
    file.read(reinterpret_cast<char*>(header.data()), header.size());
    
    // Check for encryption markers or high entropy
    double entropy = calculate_entropy(header);
    EXPECT_GT(entropy, 7.5)
        << "Database file appears unencrypted - HIPAA violation for PHI storage";
    
    // Verify encryption key management
    scratchbird::Status status;
    auto key_info = scratchbird::get_encryption_info(db, status);
    EXPECT_EQ(status.code, scratchbird::StatusCode::Ok);
    EXPECT_FALSE(key_info.key_id.empty())
        << "No encryption key management - HIPAA requires key tracking";
}

TEST_F(ComplianceTest, HIPAA_AccessLogging) {
    // HIPAA requires detailed access logging for PHI
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create PHI table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE patient_records (id INTEGER, name TEXT, diagnosis TEXT)", status), {});
    
    // Access PHI
    scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM patient_records", status), {});
    
    // Verify access is logged with required details
    auto& audit = scratchbird::audit::AuditEngine::instance();
    auto events = audit.recent(10);
    
    bool access_logged = false;
    for (const auto& event : events) {
        if (event.operation == "SELECT" && event.object == "patient_records") {
            access_logged = true;
            // HIPAA requires: who, what, when, where
            EXPECT_FALSE(event.user.empty()) << "User not logged";
            EXPECT_FALSE(event.object.empty()) << "Object not logged";
            EXPECT_GT(event.ts_epoch_ms, 0) << "Timestamp not logged";
            EXPECT_FALSE(event.client_ip.empty()) << "Client IP not logged - HIPAA requires 'where'";
        }
    }
    EXPECT_TRUE(access_logged) << "PHI access not logged - HIPAA violation";
}

TEST_F(ComplianceTest, HIPAA_MinimumNecessary) {
    // HIPAA minimum necessary standard - limit data exposure
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create role-based access control
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE ROLE nurse", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE ROLE doctor", status), {});
    
    // Nurses should see limited data
    scratchbird::execute(scratchbird::prepare(session,
        "GRANT SELECT (id, name) ON patient_records TO nurse", status), {});
    
    // Doctors can see all
    scratchbird::execute(scratchbird::prepare(session,
        "GRANT SELECT ON patient_records TO doctor", status), {});
    
    // Test column-level security
    auto nurse_session = scratchbird::create_session_as(db, "nurse_user", status);
    auto result = scratchbird::execute(scratchbird::prepare(nurse_session,
        "SELECT diagnosis FROM patient_records", status), {});
    
    EXPECT_NE(result.code, scratchbird::StatusCode::Ok)
        << "Column-level security not enforced - HIPAA minimum necessary violation";
}

// PCI DSS Compliance Tests
TEST_F(ComplianceTest, PCIDSS_CardDataProtection) {
    // PCI DSS 3.4 - Render PAN unreadable
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Try to store credit card number
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE payments (id INTEGER, card_number TEXT)", status), {});
    
    // Insert card number - should be automatically encrypted/tokenized
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO payments VALUES (1, '4111111111111111')", status), {});
    
    // Read back - should not see plain card number
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT card_number FROM payments WHERE id = 1", status), {});
    
    if (!result.rows.empty()) {
        std::string stored_value = result.rows[0]["card_number"];
        EXPECT_NE(stored_value, "4111111111111111")
            << "Credit card stored in plain text - PCI DSS violation!";
        
        // Should be masked or tokenized
        EXPECT_TRUE(stored_value.find("****") != std::string::npos ||
                   stored_value.length() > 20)  // Encrypted/tokenized
            << "Credit card not properly protected";
    }
}

TEST_F(ComplianceTest, PCIDSS_KeyManagement) {
    // PCI DSS 3.5 & 3.6 - Cryptographic key management
    scratchbird::Status status;
    auto key_manager = scratchbird::get_key_manager(db, status);
    
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok)
        << "No key management system - PCI DSS requires key management";
    
    // Verify key rotation capability
    auto keys = key_manager.list_keys();
    EXPECT_FALSE(keys.empty()) << "No encryption keys found";
    
    for (const auto& key : keys) {
        // Keys should have expiration
        EXPECT_GT(key.expiration_date, 0)
            << "Key without expiration - PCI DSS requires key lifecycle";
        
        // Keys should be strong
        EXPECT_GE(key.bit_length, 256)
            << "Weak encryption key - PCI DSS requires strong cryptography";
    }
    
    // Test key rotation
    bool rotation_success = key_manager.rotate_keys();
    EXPECT_TRUE(rotation_success)
        << "Key rotation failed - PCI DSS requires periodic key changes";
}

// SOX Compliance Tests
TEST_F(ComplianceTest, SOX_ImmutableAuditTrail) {
    // SOX Section 404 - Immutable audit trail
    auto& audit = scratchbird::audit::AuditEngine::instance();
    
    // Generate audit event
    audit.record(scratchbird::audit::AuditEventKind::Admin,
                "admin", "financial_table", "UPDATE", "Modified financial data");
    
    auto events_before = audit.recent(10);
    ASSERT_FALSE(events_before.empty());
    
    uint64_t event_id = events_before[0].id;
    std::string original_detail = events_before[0].detail;
    
    // Attempt to modify audit log (should fail)
    bool modification_prevented = true;
    try {
        audit.modify_event(event_id, "Tampered detail");
        modification_prevented = false;
    } catch (...) {
        // Expected - modification should be prevented
    }
    
    EXPECT_TRUE(modification_prevented)
        << "Audit log modification allowed - SOX violation!";
    
    // Verify event unchanged
    auto events_after = audit.recent(10);
    for (const auto& event : events_after) {
        if (event.id == event_id) {
            EXPECT_EQ(event.detail, original_detail)
                << "Audit log was modified - SOX compliance failure";
        }
    }
    
    // Verify audit logs have integrity protection (hash chain/signature)
    for (const auto& event : events_after) {
        EXPECT_FALSE(event.integrity_hash.empty())
            << "Audit event lacks integrity protection - SOX requires tamper evidence";
    }
}

TEST_F(ComplianceTest, SOX_FinancialDataRetention) {
    // SOX requires 7-year retention for financial records
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create financial table with retention policy
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE financial_records (id INTEGER, amount DECIMAL, "
        "created_at TIMESTAMP) WITH (retention_years = 7)", status), {});
    
    // Verify retention policy is enforced
    auto table_info = scratchbird::get_table_info(session, "financial_records", status);
    EXPECT_GE(table_info.retention_period_days, 7 * 365)
        << "Financial records retention too short - SOX requires 7 years";
    
    // Try to delete old financial record (should be prevented)
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO financial_records VALUES (1, 1000.00, '2020-01-01')", status), {});
    
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "DELETE FROM financial_records WHERE created_at < '2027-01-01'", status), {});
    
    EXPECT_NE(result.code, scratchbird::StatusCode::Ok)
        << "Allowed deletion of financial records within retention period - SOX violation";
}

// Helper function for entropy calculation
double calculate_entropy(const std::vector<uint8_t>& data) {
    std::array<int, 256> freq = {0};
    for (uint8_t byte : data) {
        freq[byte]++;
    }
    
    double entropy = 0.0;
    for (int f : freq) {
        if (f > 0) {
            double p = static_cast<double>(f) / data.size();
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}