#include "scratchbird/engine/connection_security.h"
#include "scratchbird/engine/tls_server.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using namespace ScratchBird;

namespace
{
    const std::string test_cert_dir = "/tmp/scratchbird_test_certs";

    void create_test_directory()
    {
        std::filesystem::create_directories(test_cert_dir);
    }

    void cleanup_test_directory()
    {
        std::filesystem::remove_all(test_cert_dir);
    }

    std::string get_test_cert_path()
    {
        return test_cert_dir + "/test_cert.pem";
    }

    std::string get_test_key_path()
    {
        return test_cert_dir + "/test_key.pem";
    }

    bool create_test_certificate()
    {
        TLSCertificateManager cert_manager;

        std::vector<std::string> alt_names = {"localhost", "127.0.0.1"};
        if (!cert_manager.generate_self_signed_certificate("Test Certificate", alt_names, 365)) {
            std::cerr << "Failed to generate test certificate" << std::endl;
            return false;
        }

        if (!cert_manager.save_certificate_to_file(get_test_cert_path()) ||
            !cert_manager.save_private_key_to_file(get_test_key_path())) {
            std::cerr << "Failed to save test certificate files" << std::endl;
            return false;
        }

        return true;
    }
} // namespace

void test_tls_configuration_validation()
{
    std::cout << "Testing TLS configuration validation..." << std::endl;

    // Test valid configuration
    TLSConfiguration valid_config;
    valid_config.certificate_file = get_test_cert_path();
    valid_config.private_key_file = get_test_key_path();
    valid_config.min_version = TLSVersion::TLS_1_2;
    valid_config.max_version = TLSVersion::TLS_1_3;

    assert(valid_config.validate());

    // Test invalid configuration - missing certificate
    TLSConfiguration invalid_config;
    invalid_config.private_key_file = get_test_key_path();

    assert(!invalid_config.validate());
    auto errors = invalid_config.get_validation_errors();
    assert(!errors.empty());
    assert(std::find_if(errors.begin(), errors.end(), [](const std::string& e) {
               return e.find("Certificate file") != std::string::npos;
           }) != errors.end());

    // Test invalid version range
    TLSConfiguration version_config;
    version_config.certificate_file = get_test_cert_path();
    version_config.private_key_file = get_test_key_path();
    version_config.min_version = TLSVersion::TLS_1_3;
    version_config.max_version = TLSVersion::TLS_1_2;

    assert(!version_config.validate());

    std::cout << "✓ TLS configuration validation tests passed" << std::endl;
}

void test_certificate_manager()
{
    std::cout << "Testing TLS certificate manager..." << std::endl;

    TLSCertificateManager cert_manager;

    // Test certificate generation
    std::vector<std::string> alt_names = {"localhost", "127.0.0.1", "test.example.com"};
    bool generated = cert_manager.generate_self_signed_certificate("Test Server", alt_names, 365);
    assert(generated);

    // Test certificate information extraction
    TLSCertificateInfo info = cert_manager.get_certificate_info();
    assert(!info.subject.empty());
    assert(!info.issuer.empty());
    assert(!info.fingerprint_sha256.empty());

    // Test certificate validation
    assert(cert_manager.validate_certificate_chain());
    assert(cert_manager.verify_private_key_match());

    // Test certificate expiry checking
    bool expiry_ok = cert_manager.check_certificate_expiry(std::chrono::hours{24 * 30}); // 30 days
    assert(expiry_ok); // Should not be expired for newly generated cert

    std::cout << "✓ Certificate manager tests passed" << std::endl;
}

void test_tls_context()
{
    std::cout << "Testing TLS context..." << std::endl;

    TLSContext context;

    TLSConfiguration config;
    config.certificate_file = get_test_cert_path();
    config.private_key_file = get_test_key_path();
    config.min_version = TLSVersion::TLS_1_2;
    config.max_version = TLSVersion::TLS_1_3;
    config.client_verification = TLSVerificationMode::Optional;

    // Test context initialization
    bool initialized = context.initialize(config);
    assert(initialized);
    assert(context.is_initialized());

    // Test certificate chain verification
    assert(context.verify_certificate_chain());

    // Test cipher suite configuration
    std::vector<std::string> cipher_suites = {"ECDHE-RSA-AES256-GCM-SHA384",
                                              "ECDHE-RSA-CHACHA20-POLY1305",
                                              "ECDHE-RSA-AES128-GCM-SHA256"};
    context.set_cipher_suites(cipher_suites);

    // Test curve configuration
    std::vector<std::string> curves = {"X25519", "P-256", "P-384"};
    context.set_curves(curves);

    // Test verification mode
    context.set_verification_mode(TLSVerificationMode::Required);

    // Test session cache
    context.enable_session_cache(true);

    // Test shutdown
    context.shutdown();
    assert(!context.is_initialized());

    std::cout << "✓ TLS context tests passed" << std::endl;
}

void test_tls_server_initialization()
{
    std::cout << "Testing TLS server initialization..." << std::endl;

    TLSServer server;

    TLSConfiguration config;
    config.certificate_file = get_test_cert_path();
    config.private_key_file = get_test_key_path();
    config.min_version = TLSVersion::TLS_1_2;
    config.max_version = TLSVersion::TLS_1_3;
    config.client_verification = TLSVerificationMode::None;
    config.enable_session_resumption = true;
    config.session_timeout = std::chrono::minutes{60};

    // Test server initialization
    bool initialized = server.initialize(config);
    assert(initialized);

    // Test configuration access
    const auto& server_config = server.get_configuration();
    assert(server_config.certificate_file == config.certificate_file);
    assert(server_config.private_key_file == config.private_key_file);

    // Test statistics
    TLSServer::TLSStatistics stats = server.get_statistics();
    assert(stats.total_connections == 0);
    assert(stats.successful_handshakes == 0);
    assert(stats.failed_handshakes == 0);

    // Test listening (simplified - not actually binding to socket)
    bool listening = server.start_listening("127.0.0.1", 8443);
    assert(listening);
    assert(server.is_running());

    server.stop_listening();
    assert(!server.is_running());

    // Test configuration update
    TLSConfiguration new_config = config;
    new_config.session_timeout = std::chrono::minutes{30};
    bool updated = server.update_configuration(new_config);
    assert(updated);

    // Test certificate reload
    bool reloaded = server.reload_certificates();
    assert(reloaded);

    server.shutdown();

    std::cout << "✓ TLS server initialization tests passed" << std::endl;
}

void test_connection_security_config()
{
    std::cout << "Testing connection security configuration..." << std::endl;

    // Test valid configuration
    ConnectionSecurityConfig valid_config;
    valid_config.encryption_policy = EncryptionPolicy::Required;
    valid_config.min_security_level = SecurityLevel::Standard;
    valid_config.force_tls_for_auth = true;
    valid_config.require_perfect_forward_secrecy = true;
    valid_config.allowed_ip_ranges = {"192.168.1.0/24", "10.0.0.0/8"};
    valid_config.rate_limiting.max_connections_per_ip = 10;
    valid_config.rate_limiting.max_attempts_per_minute = 30;

    assert(valid_config.validate());

    // Test security level calculation
    SecurityLevel effective_level = valid_config.calculate_effective_security_level();
    assert(effective_level >= SecurityLevel::Standard);

    // Test invalid configuration
    ConnectionSecurityConfig invalid_config;
    invalid_config.rate_limiting.max_connections_per_ip = 0;    // Invalid
    invalid_config.allowed_countries = {"USA", "UK", "CANADA"}; // Invalid country codes

    assert(!invalid_config.validate());
    auto errors = invalid_config.get_validation_errors();
    assert(!errors.empty());

    std::cout << "✓ Connection security configuration tests passed" << std::endl;
}

void test_ip_address_validator()
{
    std::cout << "Testing IP address validator..." << std::endl;

    IPAddressValidator validator;

    // Test IPv4 validation
    assert(validator.is_valid_ipv4("192.168.1.1"));
    assert(validator.is_valid_ipv4("10.0.0.1"));
    assert(validator.is_valid_ipv4("127.0.0.1"));
    assert(!validator.is_valid_ipv4("256.1.1.1"));
    assert(!validator.is_valid_ipv4("192.168.1"));
    assert(!validator.is_valid_ipv4("not.an.ip.address"));

    // Test IPv6 validation
    assert(validator.is_valid_ipv6("2001:db8::1"));
    assert(validator.is_valid_ipv6("::1"));
    assert(!validator.is_valid_ipv6("192.168.1.1"));

    // Test CIDR range matching
    assert(validator.is_in_cidr_range("192.168.1.100", "192.168.1.0/24"));
    assert(validator.is_in_cidr_range("10.0.5.10", "10.0.0.0/8"));
    assert(!validator.is_in_cidr_range("172.16.1.1", "192.168.1.0/24"));

    // Test allowed/blocked lists
    std::vector<std::string> allowed_ranges = {"192.168.1.0/24", "10.0.0.0/8"};
    assert(validator.is_allowed("192.168.1.50", allowed_ranges));
    assert(validator.is_allowed("10.0.0.1", allowed_ranges));
    assert(!validator.is_allowed("172.16.1.1", allowed_ranges));

    std::vector<std::string> blocked_ranges = {"192.168.100.0/24"};
    assert(validator.is_blocked("192.168.100.1", blocked_ranges));
    assert(!validator.is_blocked("192.168.1.1", blocked_ranges));

    // Test private address detection
    assert(validator.is_private_address("192.168.1.1"));
    assert(validator.is_private_address("10.0.0.1"));
    assert(validator.is_private_address("172.16.1.1"));
    assert(!validator.is_private_address("8.8.8.8"));

    // Test loopback detection
    assert(validator.is_loopback_address("127.0.0.1"));
    assert(validator.is_loopback_address("127.0.0.100"));
    assert(!validator.is_loopback_address("192.168.1.1"));

    std::cout << "✓ IP address validator tests passed" << std::endl;
}

void test_rate_limiter()
{
    std::cout << "Testing rate limiter..." << std::endl;

    ConnectionSecurityConfig::RateLimiting config;
    config.max_connections_per_ip = 3;
    config.max_attempts_per_minute = 10;
    config.max_auth_failures = 2;
    config.ip_block_duration = std::chrono::minutes{1};
    config.enable_progressive_delays = false;

    RateLimiter limiter(config);

    // Test connection limiting
    std::string test_ip = "192.168.1.100";

    assert(limiter.allow_connection(test_ip));
    assert(limiter.allow_connection(test_ip));
    assert(limiter.allow_connection(test_ip));
    // Should fail on 4th connection
    assert(!limiter.allow_connection(test_ip));

    // Test authentication failure handling
    assert(limiter.allow_authentication_attempt(test_ip));
    limiter.record_authentication_failure(test_ip);
    assert(limiter.allow_authentication_attempt(test_ip));
    limiter.record_authentication_failure(test_ip);

    // Should be blocked after max failures
    assert(limiter.is_ip_blocked(test_ip));
    assert(!limiter.allow_authentication_attempt(test_ip));

    // Test manual IP blocking/unblocking
    std::string another_ip = "192.168.1.101";
    limiter.block_ip(another_ip, std::chrono::minutes{5});
    assert(limiter.is_ip_blocked(another_ip));

    limiter.unblock_ip(another_ip);
    assert(!limiter.is_ip_blocked(another_ip));

    // Test statistics
    auto connection_counts = limiter.get_connection_counts();
    assert(connection_counts.find(test_ip) != connection_counts.end());

    auto blocked_ips = limiter.get_blocked_ips();
    assert(std::find(blocked_ips.begin(), blocked_ips.end(), test_ip) != blocked_ips.end());

    std::cout << "✓ Rate limiter tests passed" << std::endl;
}

void test_connection_security_manager()
{
    std::cout << "Testing connection security manager..." << std::endl;

    ConnectionSecurityManager manager;

    // Test initialization
    ConnectionSecurityConfig config;
    config.encryption_policy = EncryptionPolicy::Preferred;
    config.min_security_level = SecurityLevel::Standard;
    config.allowed_ip_ranges = {"192.168.0.0/16", "10.0.0.0/8"};
    config.blocked_ip_ranges = {"192.168.100.0/24"};
    config.rate_limiting.max_connections_per_ip = 5;
    config.rate_limiting.max_attempts_per_minute = 20;

    bool initialized = manager.initialize(config);
    assert(initialized);
    assert(manager.is_initialized());

    // Test connection validation
    std::string allowed_ip = "192.168.1.100";
    std::string blocked_ip = "192.168.100.50";
    std::string external_ip = "8.8.8.8";

    auto allowed_result = manager.validate_connection(allowed_ip);
    assert(allowed_result.allowed);
    assert(allowed_result.security_level >= SecurityLevel::Standard);

    auto blocked_result = manager.validate_connection(blocked_ip);
    assert(!blocked_result.allowed);
    assert(blocked_result.reason.find("blocked") != std::string::npos);

    // Test encryption requirements
    assert(!manager.is_encryption_required(allowed_ip)); // Private IP with Preferred policy
    assert(manager.is_encryption_required(external_ip)); // External IP with Preferred policy

    // Test cipher validation
    assert(manager.is_cipher_allowed("ECDHE-RSA-AES256-GCM-SHA384"));
    assert(!manager.is_cipher_allowed("NULL-MD5")); // Weak cipher

    // Test rate limiting integration
    assert(manager.allow_connection(allowed_ip));
    manager.record_connection_event(allowed_ip, true);

    // Test authentication event recording
    manager.record_authentication_event(allowed_ip, true);
    manager.record_authentication_event(allowed_ip, false);

    // Test security metrics
    SecurityMetrics metrics = manager.get_security_metrics();
    assert(metrics.total_connections > 0);

    // Test IP blocking
    manager.block_ip("1.2.3.4", std::chrono::minutes{10});
    auto blocked_ips = manager.get_blocked_ips();
    assert(std::find(blocked_ips.begin(), blocked_ips.end(), "1.2.3.4") != blocked_ips.end());

    manager.unblock_ip("1.2.3.4");
    blocked_ips = manager.get_blocked_ips();
    assert(std::find(blocked_ips.begin(), blocked_ips.end(), "1.2.3.4") == blocked_ips.end());

    // Test security event logging
    bool event_received = false;
    manager.set_security_event_callback([&event_received](const SecurityEvent& event) {
        event_received = true;
        assert(!event.remote_address.empty());
        assert(!event.to_string().empty());
    });

    manager.log_security_event(SecurityEvent::ConnectionAccepted, allowed_ip, "Test event");
    assert(event_received);

    // Test recent events
    auto recent_events = manager.get_recent_security_events(std::chrono::minutes{5});
    assert(!recent_events.empty());

    manager.shutdown();
    assert(!manager.is_initialized());

    std::cout << "✓ Connection security manager tests passed" << std::endl;
}

void test_security_policy_templates()
{
    std::cout << "Testing security policy templates..." << std::endl;

    // Test pre-defined configurations
    auto minimal = SecurityPolicyTemplates::get_minimal_security();
    assert(minimal.encryption_policy == EncryptionPolicy::Optional);
    assert(minimal.min_security_level == SecurityLevel::Minimal);
    assert(minimal.validate());

    auto standard = SecurityPolicyTemplates::get_standard_security();
    assert(standard.encryption_policy == EncryptionPolicy::Preferred);
    assert(standard.min_security_level == SecurityLevel::Standard);
    assert(standard.validate());

    auto high = SecurityPolicyTemplates::get_high_security();
    assert(high.encryption_policy == EncryptionPolicy::Required);
    assert(high.min_security_level == SecurityLevel::High);
    assert(high.validate());

    auto maximum = SecurityPolicyTemplates::get_maximum_security();
    assert(maximum.encryption_policy == EncryptionPolicy::Mandatory);
    assert(maximum.min_security_level == SecurityLevel::Maximum);
    assert(maximum.validate());

    // Test industry-specific configs
    auto financial = SecurityPolicyTemplates::get_financial_services_config();
    assert(financial.validate());
    assert(financial.min_security_level >= SecurityLevel::High);

    auto healthcare = SecurityPolicyTemplates::get_healthcare_config();
    assert(healthcare.validate());
    assert(healthcare.min_security_level >= SecurityLevel::High);

    std::cout << "✓ Security policy templates tests passed" << std::endl;
}

void test_utility_functions()
{
    std::cout << "Testing utility functions..." << std::endl;

    // Test TLS version conversion
    assert(tls_version_to_string(TLSVersion::TLS_1_2) == "TLSv1.2");
    assert(tls_version_to_string(TLSVersion::TLS_1_3) == "TLSv1.3");
    assert(parse_tls_version("TLSv1.2") == TLSVersion::TLS_1_2);
    assert(parse_tls_version("TLSv1.3") == TLSVersion::TLS_1_3);

    // Test verification mode conversion
    assert(verification_mode_to_string(TLSVerificationMode::Required) == "Required");
    assert(parse_verification_mode("Optional") == TLSVerificationMode::Optional);

    // Test security level conversion
    assert(security_level_to_string(SecurityLevel::High) == "High");
    assert(parse_security_level("Standard") == SecurityLevel::Standard);

    // Test encryption policy conversion
    assert(encryption_policy_to_string(EncryptionPolicy::Required) == "Required");
    assert(parse_encryption_policy("Preferred") == EncryptionPolicy::Preferred);

    // Test cipher validation utilities
    assert(is_weak_cipher_suite("NULL-MD5"));
    assert(is_weak_cipher_suite("EXPORT-RC4-MD5"));
    assert(!is_weak_cipher_suite("ECDHE-RSA-AES256-GCM-SHA384"));

    assert(has_perfect_forward_secrecy("ECDHE-RSA-AES256-GCM-SHA384"));
    assert(has_perfect_forward_secrecy("DHE-RSA-AES256-SHA"));
    assert(!has_perfect_forward_secrecy("RSA-AES256-SHA"));

    SecurityLevel cipher_level = assess_cipher_security_level("ECDHE-RSA-AES256-GCM-SHA384");
    assert(cipher_level >= SecurityLevel::High);

    cipher_level = assess_cipher_security_level("NULL-MD5");
    assert(cipher_level == SecurityLevel::Minimal);

    std::cout << "✓ Utility functions tests passed" << std::endl;
}

void test_openssl_integration()
{
    std::cout << "Testing OpenSSL integration..." << std::endl;

    // Test OpenSSL initialization
    initialize_openssl();

    // Test certificate manager with actual OpenSSL operations
    TLSCertificateManager cert_manager;

    // Load existing test certificate
    bool loaded_cert = cert_manager.load_certificate_from_file(get_test_cert_path());
    assert(loaded_cert);

    bool loaded_key = cert_manager.load_private_key_from_file(get_test_key_path());
    assert(loaded_key);

    // Verify key matches certificate
    assert(cert_manager.verify_private_key_match());

    // Get certificate information
    TLSCertificateInfo info = cert_manager.get_certificate_info();
    assert(!info.subject.empty());
    assert(!info.issuer.empty());
    assert(!info.fingerprint_sha256.empty());
    assert(info.is_valid());
    assert(!info.is_expired());

    cleanup_openssl();

    std::cout << "✓ OpenSSL integration tests passed" << std::endl;
}

int main()
{
    std::cout << "Starting TLS Server and Connection Security Tests..." << std::endl;

    try {
        // Setup
        create_test_directory();

        // Initialize OpenSSL for testing
        initialize_openssl();

        // Create test certificates
        if (!create_test_certificate()) {
            std::cerr << "Failed to create test certificates" << std::endl;
            return 1;
        }

        // Run tests
        test_tls_configuration_validation();
        test_certificate_manager();
        test_tls_context();
        test_tls_server_initialization();
        test_connection_security_config();
        test_ip_address_validator();
        test_rate_limiter();
        test_connection_security_manager();
        test_security_policy_templates();
        test_utility_functions();
        test_openssl_integration();

        std::cout << "\n🎉 All TLS Server and Connection Security tests passed!" << std::endl;
        std::cout << "✅ Phase 11.5: TLS and Security implementation validated" << std::endl;

        // Cleanup
        cleanup_openssl();
        cleanup_test_directory();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        cleanup_test_directory();
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        cleanup_test_directory();
        return 1;
    }
}
