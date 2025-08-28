#pragma once

#include "scratchbird/engine/authentication.h"
#include "scratchbird/engine/tls_server.h"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace ScratchBird
{

    /**
     * Security level enforcement
     */
    enum class SecurityLevel {
        Minimal,  // Basic security, backwards compatibility
        Standard, // Good security for most applications
        High,     // High security for sensitive applications
        Maximum   // Maximum security, may impact compatibility
    };

    /**
     * Encryption policy enforcement
     */
    enum class EncryptionPolicy {
        Optional,  // Encryption is optional
        Preferred, // Prefer encryption but allow unencrypted
        Required,  // Require encryption for all connections
        Mandatory  // Mandatory encryption with strict validation
    };

    /**
     * Connection security configuration
     */
    struct ConnectionSecurityConfig {
        // Encryption policies
        EncryptionPolicy encryption_policy = EncryptionPolicy::Preferred;
        bool force_tls_for_auth = true;              // Require TLS for authentication data
        bool require_perfect_forward_secrecy = true; // Require PFS cipher suites

        // Security levels
        SecurityLevel min_security_level = SecurityLevel::Standard;
        bool allow_weak_ciphers = false; // Allow weak/deprecated ciphers
        bool allow_compression = false;  // Allow TLS compression (CRIME vulnerability)

        // Connection validation
        bool validate_client_certificates = true;     // Validate client certificates if provided
        bool require_mutual_auth = false;             // Require mutual TLS authentication
        std::chrono::minutes max_connection_age{480}; // Max connection lifetime (8 hours)

        // Protocol restrictions
        std::vector<std::string> allowed_protocols{"TLSv1.2", "TLSv1.3"};
        std::vector<std::string> blocked_cipher_suites;  // Explicitly blocked cipher suites
        std::vector<std::string> required_cipher_suites; // Required cipher suites

        // IP and network restrictions
        std::vector<std::string> allowed_ip_ranges; // CIDR notation: "192.168.1.0/24"
        std::vector<std::string> blocked_ip_ranges; // Blocked IP ranges
        bool enable_geo_blocking = false;           // Enable geographic IP blocking
        std::vector<std::string> allowed_countries; // ISO country codes
        std::vector<std::string> blocked_countries; // Blocked countries

        // Rate limiting and abuse prevention
        struct RateLimiting {
            std::uint32_t max_connections_per_ip = 10;  // Max concurrent connections per IP
            std::uint32_t max_attempts_per_minute = 60; // Max connection attempts per minute
            std::uint32_t max_auth_failures = 5;        // Max auth failures before IP block
            std::chrono::minutes ip_block_duration{30}; // Duration to block abusive IPs
            bool enable_progressive_delays = true;      // Progressive delay for repeated failures
        } rate_limiting;

        // Security headers and policies
        std::map<std::string, std::string> security_headers; // Custom security headers
        bool enable_security_audit = true;                   // Enable detailed security auditing
        std::string audit_log_path = "/var/log/scratchbird/security.log";

        // Validation
        bool validate() const;
        std::vector<std::string> get_validation_errors() const;
        SecurityLevel calculate_effective_security_level() const;
    };

    /**
     * Connection security metrics
     */
    struct SecurityMetrics {
        std::uint64_t total_connections = 0;
        std::uint64_t encrypted_connections = 0;
        std::uint64_t unencrypted_connections = 0;
        std::uint64_t rejected_connections = 0;
        std::uint64_t blocked_ips = 0;
        std::uint64_t certificate_errors = 0;
        std::uint64_t protocol_violations = 0;
        std::uint64_t rate_limit_violations = 0;
        std::chrono::system_clock::time_point last_reset;

        double get_encryption_ratio() const
        {
            if (total_connections == 0)
                return 0.0;
            return static_cast<double>(encrypted_connections) / total_connections;
        }

        double get_rejection_ratio() const
        {
            if (total_connections == 0)
                return 0.0;
            return static_cast<double>(rejected_connections) / total_connections;
        }
    };

    /**
     * Security event for audit logging
     */
    struct SecurityEvent {
        enum Type {
            ConnectionAccepted,
            ConnectionRejected,
            EncryptionUpgraded,
            EncryptionRequired,
            CertificateValidated,
            CertificateRejected,
            IPBlocked,
            IPUnblocked,
            RateLimitExceeded,
            SecurityViolation,
            PolicyViolation,
            GeographicBlocked,
            WeakCipherDetected,
            ProtocolDowngrade
        };

        Type type;
        std::string remote_address;
        std::string user_agent;
        std::string details;
        SecurityLevel security_level;
        std::chrono::system_clock::time_point timestamp;

        std::string to_string() const;
    };

    /**
     * IP address and geolocation utilities
     */
    class IPAddressValidator
    {
      public:
        IPAddressValidator() = default;
        ~IPAddressValidator() = default;

        // IP address validation
        bool is_valid_ipv4(const std::string& ip) const;
        bool is_valid_ipv6(const std::string& ip) const;
        bool is_valid_ip(const std::string& ip) const;

        // CIDR range validation
        bool is_in_cidr_range(const std::string& ip, const std::string& cidr) const;
        bool is_allowed(const std::string& ip,
                        const std::vector<std::string>& allowed_ranges) const;
        bool is_blocked(const std::string& ip,
                        const std::vector<std::string>& blocked_ranges) const;

        // Geographic validation (simplified - in production use MaxMind GeoIP)
        std::string get_country_code(const std::string& ip) const;
        bool is_country_allowed(const std::string& ip,
                                const std::vector<std::string>& allowed_countries) const;
        bool is_country_blocked(const std::string& ip,
                                const std::vector<std::string>& blocked_countries) const;

        // Private/special address detection
        bool is_private_address(const std::string& ip) const;
        bool is_loopback_address(const std::string& ip) const;
        bool is_multicast_address(const std::string& ip) const;

      private:
        struct IPv4Address {
            std::uint32_t address;
            std::uint8_t prefix_length;
        };

        struct IPv6Address {
            std::uint8_t address[16];
            std::uint8_t prefix_length;
        };

        bool parse_ipv4_cidr(const std::string& cidr, IPv4Address& result) const;
        bool parse_ipv6_cidr(const std::string& cidr, IPv6Address& result) const;
        std::uint32_t ipv4_string_to_int(const std::string& ip) const;
    };

    /**
     * Rate limiting and abuse prevention
     */
    class RateLimiter
    {
      public:
        RateLimiter(const ConnectionSecurityConfig::RateLimiting& config);
        ~RateLimiter() = default;

        // Connection rate limiting
        bool allow_connection(const std::string& remote_ip);
        bool allow_authentication_attempt(const std::string& remote_ip);
        void record_authentication_failure(const std::string& remote_ip);
        void record_authentication_success(const std::string& remote_ip);

        // IP blocking
        bool is_ip_blocked(const std::string& remote_ip) const;
        void block_ip(const std::string& remote_ip,
                      std::chrono::minutes duration = std::chrono::minutes{0});
        void unblock_ip(const std::string& remote_ip);
        void cleanup_expired_blocks();

        // Statistics
        std::map<std::string, std::uint32_t> get_connection_counts() const;
        std::vector<std::string> get_blocked_ips() const;
        std::uint32_t get_total_blocks() const;

      private:
        struct IPConnectionInfo {
            std::uint32_t active_connections = 0;
            std::uint32_t attempts_this_minute = 0;
            std::uint32_t auth_failures = 0;
            std::chrono::system_clock::time_point last_attempt;
            std::chrono::system_clock::time_point minute_start;
            std::chrono::system_clock::time_point blocked_until;
            bool is_blocked = false;
        };

        ConnectionSecurityConfig::RateLimiting config_;
        mutable std::mutex rate_limiter_mutex_;
        std::map<std::string, IPConnectionInfo> ip_info_;

        void update_minute_counter(IPConnectionInfo& info);
        std::chrono::minutes calculate_progressive_delay(std::uint32_t failure_count) const;
    };

    /**
     * Connection security manager
     */
    class ConnectionSecurityManager
    {
      public:
        ConnectionSecurityManager();
        ~ConnectionSecurityManager();

        // Configuration
        bool initialize(const ConnectionSecurityConfig& config);
        void shutdown();
        bool is_initialized() const
        {
            return initialized_;
        }

        bool update_configuration(const ConnectionSecurityConfig& new_config);
        const ConnectionSecurityConfig& get_configuration() const
        {
            return config_;
        }

        // Connection validation
        struct ValidationResult {
            bool allowed = false;
            std::string reason;
            SecurityLevel security_level = SecurityLevel::Minimal;
            bool requires_encryption = false;
            bool requires_client_cert = false;
            std::vector<std::string> required_security_headers;
        };

        ValidationResult validate_connection(const std::string& remote_ip,
                                             const std::string& user_agent = "",
                                             const TLSSession* tls_session = nullptr) const;

        // Encryption policy enforcement
        bool is_encryption_required(const std::string& remote_ip) const;
        bool is_cipher_allowed(const std::string& cipher_suite) const;
        bool validate_tls_session(const TLSSession& session) const;

        // Authentication security
        bool is_authentication_secure(const AuthenticationContext& context) const;
        bool requires_secure_channel_for_auth(AuthenticationMethod method) const;

        // Rate limiting integration
        bool allow_connection(const std::string& remote_ip);
        void record_connection_event(const std::string& remote_ip, bool successful);
        void record_authentication_event(const std::string& remote_ip, bool successful);

        // Security monitoring
        SecurityMetrics get_security_metrics() const;
        void reset_security_metrics();
        std::vector<SecurityEvent>
        get_recent_security_events(std::chrono::minutes lookback = std::chrono::minutes{60}) const;

        // Audit logging
        using SecurityEventCallback = std::function<void(const SecurityEvent&)>;
        void set_security_event_callback(SecurityEventCallback callback)
        {
            event_callback_ = callback;
        }
        void log_security_event(SecurityEvent::Type type, const std::string& remote_ip,
                                const std::string& details = "",
                                SecurityLevel level = SecurityLevel::Standard);

        // Administrative functions
        void block_ip(const std::string& ip,
                      std::chrono::minutes duration = std::chrono::minutes{0});
        void unblock_ip(const std::string& ip);
        std::vector<std::string> get_blocked_ips() const;
        void cleanup_expired_blocks();

        // Security policy queries
        SecurityLevel get_effective_security_level(const std::string& remote_ip) const;
        std::vector<std::string> get_required_cipher_suites() const;
        std::vector<std::string> get_blocked_cipher_suites() const;
        bool requires_perfect_forward_secrecy() const;

      private:
        // Core components
        ConnectionSecurityConfig config_;
        std::unique_ptr<IPAddressValidator> ip_validator_;
        std::unique_ptr<RateLimiter> rate_limiter_;

        // State
        bool initialized_;
        mutable SecurityMetrics metrics_;
        mutable std::mutex metrics_mutex_;

        // Event logging
        SecurityEventCallback event_callback_;
        std::vector<SecurityEvent> recent_events_;
        mutable std::mutex events_mutex_;

        // Security validation helpers
        bool validate_ip_restrictions(const std::string& remote_ip) const;
        bool validate_protocol_requirements(const TLSSession* tls_session) const;
        SecurityLevel calculate_connection_security_level(const std::string& remote_ip,
                                                          const TLSSession* tls_session) const;

        // Configuration validation
        bool validate_cipher_suites(const std::vector<std::string>& cipher_suites) const;
        bool validate_security_headers(const std::map<std::string, std::string>& headers) const;

        mutable std::shared_mutex security_mutex_;
    };

    /**
     * Security policy templates for common configurations
     */
    class SecurityPolicyTemplates
    {
      public:
        // Pre-defined security configurations
        static ConnectionSecurityConfig get_minimal_security();
        static ConnectionSecurityConfig get_standard_security();
        static ConnectionSecurityConfig get_high_security();
        static ConnectionSecurityConfig get_maximum_security();

        // Industry-specific configurations
        static ConnectionSecurityConfig get_financial_services_config();
        static ConnectionSecurityConfig get_healthcare_config();
        static ConnectionSecurityConfig get_government_config();
        static ConnectionSecurityConfig get_development_config();

        // Compliance configurations
        static ConnectionSecurityConfig get_pci_dss_compliant_config();
        static ConnectionSecurityConfig get_hipaa_compliant_config();
        static ConnectionSecurityConfig get_gdpr_compliant_config();

        // Custom configuration builders
        class ConfigBuilder
        {
          public:
            ConfigBuilder() = default;

            ConfigBuilder& set_security_level(SecurityLevel level);
            ConfigBuilder& set_encryption_policy(EncryptionPolicy policy);
            ConfigBuilder& require_client_certificates(bool require = true);
            ConfigBuilder& allow_ip_ranges(const std::vector<std::string>& ranges);
            ConfigBuilder& block_ip_ranges(const std::vector<std::string>& ranges);
            ConfigBuilder& set_rate_limits(std::uint32_t max_connections,
                                           std::uint32_t max_attempts);
            ConfigBuilder& enable_geo_blocking(const std::vector<std::string>& allowed_countries);
            ConfigBuilder& add_security_headers(const std::map<std::string, std::string>& headers);

            ConnectionSecurityConfig build() const;

          private:
            ConnectionSecurityConfig config_;
        };
    };

    // Utility functions
    std::string security_level_to_string(SecurityLevel level);
    SecurityLevel parse_security_level(const std::string& level_str);
    std::string encryption_policy_to_string(EncryptionPolicy policy);
    EncryptionPolicy parse_encryption_policy(const std::string& policy_str);

    // Security validation utilities
    bool is_weak_cipher_suite(const std::string& cipher_suite);
    bool has_perfect_forward_secrecy(const std::string& cipher_suite);
    SecurityLevel assess_cipher_security_level(const std::string& cipher_suite);

} // namespace ScratchBird
