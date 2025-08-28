#include "scratchbird/engine/connection_security.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>

namespace ScratchBird
{

    // ConnectionSecurityConfig Implementation
    bool ConnectionSecurityConfig::validate() const
    {
        return get_validation_errors().empty();
    }

    std::vector<std::string> ConnectionSecurityConfig::get_validation_errors() const
    {
        std::vector<std::string> errors;

        // Validate encryption policy consistency
        if (encryption_policy == EncryptionPolicy::Required ||
            encryption_policy == EncryptionPolicy::Mandatory) {
            if (!force_tls_for_auth) {
                errors.push_back("force_tls_for_auth should be true when encryption is required");
            }
        }

        // Validate rate limiting configuration
        if (rate_limiting.max_connections_per_ip == 0) {
            errors.push_back("max_connections_per_ip must be greater than 0");
        }

        if (rate_limiting.max_attempts_per_minute == 0) {
            errors.push_back("max_attempts_per_minute must be greater than 0");
        }

        // Validate IP ranges format
        IPAddressValidator validator;
        for (const auto& range : allowed_ip_ranges) {
            if (range.find('/') == std::string::npos ||
                !validator.is_valid_ip(range.substr(0, range.find('/')))) {
                errors.push_back("Invalid IP range format: " + range);
            }
        }

        for (const auto& range : blocked_ip_ranges) {
            if (range.find('/') == std::string::npos ||
                !validator.is_valid_ip(range.substr(0, range.find('/')))) {
                errors.push_back("Invalid blocked IP range format: " + range);
            }
        }

        // Validate country codes (should be 2-letter ISO codes)
        for (const auto& country : allowed_countries) {
            if (country.length() != 2) {
                errors.push_back("Invalid country code (must be 2 letters): " + country);
            }
        }

        for (const auto& country : blocked_countries) {
            if (country.length() != 2) {
                errors.push_back("Invalid blocked country code (must be 2 letters): " + country);
            }
        }

        return errors;
    }

    SecurityLevel ConnectionSecurityConfig::calculate_effective_security_level() const
    {
        SecurityLevel effective = min_security_level;

        // Upgrade based on configuration
        if (encryption_policy == EncryptionPolicy::Required ||
            encryption_policy == EncryptionPolicy::Mandatory) {
            effective = std::max(effective, SecurityLevel::Standard);
        }

        if (require_perfect_forward_secrecy && !allow_weak_ciphers) {
            effective = std::max(effective, SecurityLevel::High);
        }

        if (require_mutual_auth && !allowed_countries.empty()) {
            effective = std::max(effective, SecurityLevel::Maximum);
        }

        return effective;
    }

    // SecurityEvent Implementation
    std::string SecurityEvent::to_string() const
    {
        std::ostringstream oss;
        oss << "["
            << std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch())
                   .count()
            << "] ";

        switch (type) {
        case ConnectionAccepted:
            oss << "CONNECTION_ACCEPTED";
            break;
        case ConnectionRejected:
            oss << "CONNECTION_REJECTED";
            break;
        case EncryptionUpgraded:
            oss << "ENCRYPTION_UPGRADED";
            break;
        case EncryptionRequired:
            oss << "ENCRYPTION_REQUIRED";
            break;
        case CertificateValidated:
            oss << "CERTIFICATE_VALIDATED";
            break;
        case CertificateRejected:
            oss << "CERTIFICATE_REJECTED";
            break;
        case IPBlocked:
            oss << "IP_BLOCKED";
            break;
        case IPUnblocked:
            oss << "IP_UNBLOCKED";
            break;
        case RateLimitExceeded:
            oss << "RATE_LIMIT_EXCEEDED";
            break;
        case SecurityViolation:
            oss << "SECURITY_VIOLATION";
            break;
        case PolicyViolation:
            oss << "POLICY_VIOLATION";
            break;
        case GeographicBlocked:
            oss << "GEOGRAPHIC_BLOCKED";
            break;
        case WeakCipherDetected:
            oss << "WEAK_CIPHER_DETECTED";
            break;
        case ProtocolDowngrade:
            oss << "PROTOCOL_DOWNGRADE";
            break;
        default:
            oss << "UNKNOWN_EVENT";
            break;
        }

        oss << " from " << remote_address;
        if (!details.empty()) {
            oss << ": " << details;
        }

        return oss.str();
    }

    // IPAddressValidator Implementation
    bool IPAddressValidator::is_valid_ipv4(const std::string& ip) const
    {
        struct sockaddr_in sa;
        return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
    }

    bool IPAddressValidator::is_valid_ipv6(const std::string& ip) const
    {
        struct sockaddr_in6 sa;
        return inet_pton(AF_INET6, ip.c_str(), &(sa.sin6_addr)) != 0;
    }

    bool IPAddressValidator::is_valid_ip(const std::string& ip) const
    {
        return is_valid_ipv4(ip) || is_valid_ipv6(ip);
    }

    bool IPAddressValidator::is_in_cidr_range(const std::string& ip, const std::string& cidr) const
    {
        size_t slash_pos = cidr.find('/');
        if (slash_pos == std::string::npos)
            return false;

        std::string network = cidr.substr(0, slash_pos);
        int prefix_len = std::stoi(cidr.substr(slash_pos + 1));

        if (is_valid_ipv4(ip) && is_valid_ipv4(network)) {
            // IPv4 CIDR matching
            std::uint32_t ip_int = ipv4_string_to_int(ip);
            std::uint32_t network_int = ipv4_string_to_int(network);
            std::uint32_t mask = (~0U) << (32 - prefix_len);

            return (ip_int & mask) == (network_int & mask);
        }

        // IPv6 CIDR matching would be implemented here
        // For now, return false for IPv6
        return false;
    }

    bool IPAddressValidator::is_allowed(const std::string& ip,
                                        const std::vector<std::string>& allowed_ranges) const
    {
        if (allowed_ranges.empty())
            return true; // No restrictions = allow all

        for (const auto& range : allowed_ranges) {
            if (is_in_cidr_range(ip, range)) {
                return true;
            }
        }

        return false;
    }

    bool IPAddressValidator::is_blocked(const std::string& ip,
                                        const std::vector<std::string>& blocked_ranges) const
    {
        for (const auto& range : blocked_ranges) {
            if (is_in_cidr_range(ip, range)) {
                return true;
            }
        }

        return false;
    }

    std::string IPAddressValidator::get_country_code(const std::string& ip) const
    {
        // Simplified implementation - in production, use MaxMind GeoIP or similar
        if (is_private_address(ip) || is_loopback_address(ip)) {
            return "XX"; // Local/private address
        }

        // For demo purposes, return a mock country code based on IP ranges
        if (ip.substr(0, 3) == "192")
            return "US"; // Private range, but for demo
        if (ip.substr(0, 2) == "10")
            return "US"; // Private range, but for demo

        return "XX"; // Unknown
    }

    bool
    IPAddressValidator::is_country_allowed(const std::string& ip,
                                           const std::vector<std::string>& allowed_countries) const
    {
        if (allowed_countries.empty())
            return true;

        std::string country = get_country_code(ip);
        return std::find(allowed_countries.begin(), allowed_countries.end(), country) !=
               allowed_countries.end();
    }

    bool
    IPAddressValidator::is_country_blocked(const std::string& ip,
                                           const std::vector<std::string>& blocked_countries) const
    {
        std::string country = get_country_code(ip);
        return std::find(blocked_countries.begin(), blocked_countries.end(), country) !=
               blocked_countries.end();
    }

    bool IPAddressValidator::is_private_address(const std::string& ip) const
    {
        if (!is_valid_ipv4(ip))
            return false;

        std::uint32_t ip_int = ipv4_string_to_int(ip);

        // 10.0.0.0/8
        if ((ip_int & 0xFF000000) == 0x0A000000)
            return true;

        // 172.16.0.0/12
        if ((ip_int & 0xFFF00000) == 0xAC100000)
            return true;

        // 192.168.0.0/16
        if ((ip_int & 0xFFFF0000) == 0xC0A80000)
            return true;

        return false;
    }

    bool IPAddressValidator::is_loopback_address(const std::string& ip) const
    {
        if (!is_valid_ipv4(ip)) {
            return ip == "::1"; // IPv6 loopback
        }

        std::uint32_t ip_int = ipv4_string_to_int(ip);
        return (ip_int & 0xFF000000) == 0x7F000000; // 127.0.0.0/8
    }

    bool IPAddressValidator::is_multicast_address(const std::string& ip) const
    {
        if (!is_valid_ipv4(ip))
            return false;

        std::uint32_t ip_int = ipv4_string_to_int(ip);
        return (ip_int & 0xF0000000) == 0xE0000000; // 224.0.0.0/4
    }

    std::uint32_t IPAddressValidator::ipv4_string_to_int(const std::string& ip) const
    {
        struct sockaddr_in sa;
        inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr));
        return ntohl(sa.sin_addr.s_addr);
    }

    // RateLimiter Implementation
    RateLimiter::RateLimiter(const ConnectionSecurityConfig::RateLimiting& config) : config_(config)
    {
    }

    bool RateLimiter::allow_connection(const std::string& remote_ip)
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        auto& info = ip_info_[remote_ip];

        // Check if IP is blocked
        if (info.is_blocked && std::chrono::system_clock::now() < info.blocked_until) {
            return false;
        } else if (info.is_blocked) {
            // Unblock expired block
            info.is_blocked = false;
            info.blocked_until = std::chrono::system_clock::time_point{};
        }

        // Check connection limit per IP
        if (info.active_connections >= config_.max_connections_per_ip) {
            return false;
        }

        // Update minute counter and check attempt limit
        update_minute_counter(info);

        if (info.attempts_this_minute >= config_.max_attempts_per_minute) {
            return false;
        }

        // Allow connection
        info.active_connections++;
        info.attempts_this_minute++;
        info.last_attempt = std::chrono::system_clock::now();

        return true;
    }

    bool RateLimiter::allow_authentication_attempt(const std::string& remote_ip)
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        auto& info = ip_info_[remote_ip];

        // Check if IP is blocked due to auth failures
        if (info.is_blocked && std::chrono::system_clock::now() < info.blocked_until) {
            return false;
        }

        return true;
    }

    void RateLimiter::record_authentication_failure(const std::string& remote_ip)
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        auto& info = ip_info_[remote_ip];
        info.auth_failures++;

        if (info.auth_failures >= config_.max_auth_failures) {
            // Block the IP
            auto delay = config_.enable_progressive_delays
                             ? calculate_progressive_delay(info.auth_failures)
                             : config_.ip_block_duration;

            info.is_blocked = true;
            info.blocked_until = std::chrono::system_clock::now() + delay;
        }
    }

    void RateLimiter::record_authentication_success(const std::string& remote_ip)
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        auto& info = ip_info_[remote_ip];
        info.auth_failures = 0; // Reset failure count on success
    }

    bool RateLimiter::is_ip_blocked(const std::string& remote_ip) const
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        auto it = ip_info_.find(remote_ip);
        if (it == ip_info_.end())
            return false;

        const auto& info = it->second;
        return info.is_blocked && std::chrono::system_clock::now() < info.blocked_until;
    }

    void RateLimiter::block_ip(const std::string& remote_ip, std::chrono::minutes duration)
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        auto& info = ip_info_[remote_ip];
        info.is_blocked = true;

        if (duration.count() == 0) {
            duration = config_.ip_block_duration;
        }

        info.blocked_until = std::chrono::system_clock::now() + duration;
    }

    void RateLimiter::unblock_ip(const std::string& remote_ip)
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        auto it = ip_info_.find(remote_ip);
        if (it != ip_info_.end()) {
            it->second.is_blocked = false;
            it->second.blocked_until = std::chrono::system_clock::time_point{};
            it->second.auth_failures = 0;
        }
    }

    void RateLimiter::cleanup_expired_blocks()
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        auto now = std::chrono::system_clock::now();

        for (auto& [ip, info] : ip_info_) {
            if (info.is_blocked && now >= info.blocked_until) {
                info.is_blocked = false;
                info.blocked_until = std::chrono::system_clock::time_point{};
            }

            // Clean up old minute counters
            if (now - info.minute_start > std::chrono::minutes{2}) {
                info.attempts_this_minute = 0;
                info.minute_start = now;
            }
        }
    }

    std::map<std::string, std::uint32_t> RateLimiter::get_connection_counts() const
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        std::map<std::string, std::uint32_t> counts;
        for (const auto& [ip, info] : ip_info_) {
            if (info.active_connections > 0) {
                counts[ip] = info.active_connections;
            }
        }

        return counts;
    }

    std::vector<std::string> RateLimiter::get_blocked_ips() const
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        std::vector<std::string> blocked_ips;
        auto now = std::chrono::system_clock::now();

        for (const auto& [ip, info] : ip_info_) {
            if (info.is_blocked && now < info.blocked_until) {
                blocked_ips.push_back(ip);
            }
        }

        return blocked_ips;
    }

    std::uint32_t RateLimiter::get_total_blocks() const
    {
        std::lock_guard<std::mutex> lock(rate_limiter_mutex_);

        std::uint32_t total = 0;
        auto now = std::chrono::system_clock::now();

        for (const auto& [ip, info] : ip_info_) {
            if (info.is_blocked && now < info.blocked_until) {
                total++;
            }
        }

        return total;
    }

    void RateLimiter::update_minute_counter(IPConnectionInfo& info)
    {
        auto now = std::chrono::system_clock::now();
        auto minute_diff =
            std::chrono::duration_cast<std::chrono::minutes>(now - info.minute_start);

        if (minute_diff >= std::chrono::minutes{1}) {
            info.attempts_this_minute = 0;
            info.minute_start = now;
        }
    }

    std::chrono::minutes RateLimiter::calculate_progressive_delay(std::uint32_t failure_count) const
    {
        // Progressive delay: 1min, 2min, 5min, 10min, 30min, 60min, ...
        if (failure_count <= 5)
            return config_.ip_block_duration;
        if (failure_count <= 10)
            return config_.ip_block_duration * 2;
        if (failure_count <= 15)
            return config_.ip_block_duration * 5;
        if (failure_count <= 20)
            return config_.ip_block_duration * 10;

        return config_.ip_block_duration * 30; // Maximum delay
    }

    // ConnectionSecurityManager Implementation
    ConnectionSecurityManager::ConnectionSecurityManager() : initialized_(false)
    {
        ip_validator_ = std::make_unique<IPAddressValidator>();
    }

    ConnectionSecurityManager::~ConnectionSecurityManager()
    {
        shutdown();
    }

    bool ConnectionSecurityManager::initialize(const ConnectionSecurityConfig& config)
    {
        std::unique_lock<std::shared_mutex> lock(security_mutex_);

        if (!config.validate()) {
            return false;
        }

        config_ = config;
        rate_limiter_ = std::make_unique<RateLimiter>(config.rate_limiting);

        metrics_.last_reset = std::chrono::system_clock::now();
        initialized_ = true;

        return true;
    }

    void ConnectionSecurityManager::shutdown()
    {
        std::unique_lock<std::shared_mutex> lock(security_mutex_);

        initialized_ = false;
        rate_limiter_.reset();
        recent_events_.clear();
    }

    bool ConnectionSecurityManager::update_configuration(const ConnectionSecurityConfig& new_config)
    {
        if (!new_config.validate()) {
            return false;
        }

        std::unique_lock<std::shared_mutex> lock(security_mutex_);

        config_ = new_config;
        rate_limiter_ = std::make_unique<RateLimiter>(new_config.rate_limiting);

        log_security_event(SecurityEvent::PolicyViolation, "", "Configuration updated");

        return true;
    }

    ConnectionSecurityManager::ValidationResult
    ConnectionSecurityManager::validate_connection(const std::string& remote_ip,
                                                   const std::string& user_agent,
                                                   const TLSSession* tls_session) const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);

        ValidationResult result;

        if (!initialized_) {
            result.reason = "Security manager not initialized";
            return result;
        }

        // Check IP restrictions
        if (!validate_ip_restrictions(remote_ip)) {
            result.reason = "IP address blocked or not in allowed range";
            return result;
        }

        // Check rate limiting
        if (rate_limiter_->is_ip_blocked(remote_ip)) {
            result.reason = "IP address is rate limited/blocked";
            return result;
        }

        // Check protocol requirements
        if (!validate_protocol_requirements(tls_session)) {
            result.reason = "Protocol requirements not met";
            return result;
        }

        // Calculate security level
        result.security_level = calculate_connection_security_level(remote_ip, tls_session);

        // Determine encryption requirements
        result.requires_encryption = is_encryption_required(remote_ip);
        result.requires_client_cert = config_.require_mutual_auth;

        // Check if connection meets minimum security requirements
        if (result.security_level < config_.min_security_level) {
            result.reason = "Connection does not meet minimum security level";
            return result;
        }

        result.allowed = true;
        return result;
    }

    bool ConnectionSecurityManager::is_encryption_required(const std::string& remote_ip) const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);

        switch (config_.encryption_policy) {
        case EncryptionPolicy::Optional:
            return false;
        case EncryptionPolicy::Preferred:
            return !ip_validator_->is_private_address(remote_ip);
        case EncryptionPolicy::Required:
        case EncryptionPolicy::Mandatory:
            return true;
        default:
            return false;
        }
    }

    bool ConnectionSecurityManager::is_cipher_allowed(const std::string& cipher_suite) const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);

        // Check blocked cipher suites
        for (const auto& blocked : config_.blocked_cipher_suites) {
            if (cipher_suite.find(blocked) != std::string::npos) {
                return false;
            }
        }

        // Check required cipher suites
        if (!config_.required_cipher_suites.empty()) {
            bool found_required = false;
            for (const auto& required : config_.required_cipher_suites) {
                if (cipher_suite.find(required) != std::string::npos) {
                    found_required = true;
                    break;
                }
            }
            if (!found_required)
                return false;
        }

        // Check weak ciphers
        if (!config_.allow_weak_ciphers && is_weak_cipher_suite(cipher_suite)) {
            return false;
        }

        // Check perfect forward secrecy requirement
        if (config_.require_perfect_forward_secrecy && !has_perfect_forward_secrecy(cipher_suite)) {
            return false;
        }

        return true;
    }

    bool ConnectionSecurityManager::validate_tls_session(const TLSSession& session) const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);

        // Check cipher suite
        std::string cipher = session.get_cipher_suite();
        if (!is_cipher_allowed(cipher)) {
            return false;
        }

        // Check protocol version
        std::string protocol = session.get_protocol_version();
        bool protocol_allowed = false;
        for (const auto& allowed : config_.allowed_protocols) {
            if (protocol.find(allowed) != std::string::npos) {
                protocol_allowed = true;
                break;
            }
        }

        if (!protocol_allowed) {
            return false;
        }

        // Check client certificate if required
        if (config_.require_mutual_auth && !session.has_client_certificate()) {
            return false;
        }

        if (session.has_client_certificate() && config_.validate_client_certificates) {
            if (!session.verify_client_certificate()) {
                return false;
            }
        }

        return true;
    }

    bool
    ConnectionSecurityManager::is_authentication_secure(const AuthenticationContext& context) const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);

        // Check if secure channel is required for this auth method
        // This would integrate with the authentication system
        return true; // Simplified implementation
    }

    bool
    ConnectionSecurityManager::requires_secure_channel_for_auth(AuthenticationMethod method) const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);

        if (!config_.force_tls_for_auth)
            return false;

        switch (method) {
        case AuthenticationMethod::Password:
        case AuthenticationMethod::TwoFactor:
            return true;
        case AuthenticationMethod::TrustedOS:
        case AuthenticationMethod::Certificate:
            return false; // Already secure
        default:
            return true; // Default to secure
        }
    }

    bool ConnectionSecurityManager::allow_connection(const std::string& remote_ip)
    {
        if (!rate_limiter_)
            return true;

        bool allowed = rate_limiter_->allow_connection(remote_ip);

        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.total_connections++;
            if (!allowed) {
                metrics_.rate_limit_violations++;
            }
        }

        return allowed;
    }

    void ConnectionSecurityManager::record_connection_event(const std::string& remote_ip,
                                                            bool successful)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        if (successful) {
            // Connection tracking would be handled by rate limiter
        } else {
            metrics_.rejected_connections++;
        }
    }

    void ConnectionSecurityManager::record_authentication_event(const std::string& remote_ip,
                                                                bool successful)
    {
        if (!rate_limiter_)
            return;

        if (successful) {
            rate_limiter_->record_authentication_success(remote_ip);
        } else {
            rate_limiter_->record_authentication_failure(remote_ip);
        }
    }

    SecurityMetrics ConnectionSecurityManager::get_security_metrics() const
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        return metrics_;
    }

    void ConnectionSecurityManager::reset_security_metrics()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_ = SecurityMetrics{};
        metrics_.last_reset = std::chrono::system_clock::now();
    }

    std::vector<SecurityEvent>
    ConnectionSecurityManager::get_recent_security_events(std::chrono::minutes lookback) const
    {
        std::lock_guard<std::mutex> lock(events_mutex_);

        auto cutoff = std::chrono::system_clock::now() - lookback;
        std::vector<SecurityEvent> filtered_events;

        for (const auto& event : recent_events_) {
            if (event.timestamp >= cutoff) {
                filtered_events.push_back(event);
            }
        }

        return filtered_events;
    }

    void ConnectionSecurityManager::log_security_event(SecurityEvent::Type type,
                                                       const std::string& remote_ip,
                                                       const std::string& details,
                                                       SecurityLevel level)
    {
        SecurityEvent event;
        event.type = type;
        event.remote_address = remote_ip;
        event.details = details;
        event.security_level = level;
        event.timestamp = std::chrono::system_clock::now();

        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            recent_events_.push_back(event);

            // Keep only recent events (last 1000 or last 24 hours)
            const size_t max_events = 1000;
            const auto cutoff = std::chrono::system_clock::now() - std::chrono::hours{24};

            if (recent_events_.size() > max_events) {
                recent_events_.erase(recent_events_.begin(),
                                     recent_events_.begin() + (recent_events_.size() - max_events));
            }

            // Remove old events
            auto it =
                std::remove_if(recent_events_.begin(), recent_events_.end(),
                               [cutoff](const SecurityEvent& e) { return e.timestamp < cutoff; });
            recent_events_.erase(it, recent_events_.end());
        }

        if (event_callback_) {
            event_callback_(event);
        }
    }

    void ConnectionSecurityManager::block_ip(const std::string& ip, std::chrono::minutes duration)
    {
        if (rate_limiter_) {
            rate_limiter_->block_ip(ip, duration);
            log_security_event(SecurityEvent::IPBlocked, ip);
        }
    }

    void ConnectionSecurityManager::unblock_ip(const std::string& ip)
    {
        if (rate_limiter_) {
            rate_limiter_->unblock_ip(ip);
            log_security_event(SecurityEvent::IPUnblocked, ip);
        }
    }

    std::vector<std::string> ConnectionSecurityManager::get_blocked_ips() const
    {
        if (!rate_limiter_)
            return {};
        return rate_limiter_->get_blocked_ips();
    }

    void ConnectionSecurityManager::cleanup_expired_blocks()
    {
        if (rate_limiter_) {
            rate_limiter_->cleanup_expired_blocks();
        }
    }

    SecurityLevel
    ConnectionSecurityManager::get_effective_security_level(const std::string& remote_ip) const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);
        return calculate_connection_security_level(remote_ip, nullptr);
    }

    std::vector<std::string> ConnectionSecurityManager::get_required_cipher_suites() const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);
        return config_.required_cipher_suites;
    }

    std::vector<std::string> ConnectionSecurityManager::get_blocked_cipher_suites() const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);
        return config_.blocked_cipher_suites;
    }

    bool ConnectionSecurityManager::requires_perfect_forward_secrecy() const
    {
        std::shared_lock<std::shared_mutex> lock(security_mutex_);
        return config_.require_perfect_forward_secrecy;
    }

    // Private helper methods
    bool ConnectionSecurityManager::validate_ip_restrictions(const std::string& remote_ip) const
    {
        // Check blocked IPs
        if (ip_validator_->is_blocked(remote_ip, config_.blocked_ip_ranges)) {
            return false;
        }

        // Check allowed IPs
        if (!ip_validator_->is_allowed(remote_ip, config_.allowed_ip_ranges)) {
            return false;
        }

        // Check geographic restrictions
        if (config_.enable_geo_blocking) {
            if (!ip_validator_->is_country_allowed(remote_ip, config_.allowed_countries)) {
                return false;
            }

            if (ip_validator_->is_country_blocked(remote_ip, config_.blocked_countries)) {
                return false;
            }
        }

        return true;
    }

    bool
    ConnectionSecurityManager::validate_protocol_requirements(const TLSSession* tls_session) const
    {
        if (config_.encryption_policy == EncryptionPolicy::Required ||
            config_.encryption_policy == EncryptionPolicy::Mandatory) {
            if (!tls_session) {
                return false; // TLS required but not present
            }

            return validate_tls_session(*tls_session);
        }

        return true;
    }

    SecurityLevel ConnectionSecurityManager::calculate_connection_security_level(
        const std::string& remote_ip, const TLSSession* tls_session) const
    {
        SecurityLevel level = SecurityLevel::Minimal;

        // Upgrade based on encryption
        if (tls_session) {
            level = std::max(level, SecurityLevel::Standard);

            std::string cipher = tls_session->get_cipher_suite();
            level = std::max(level, assess_cipher_security_level(cipher));

            if (tls_session->has_perfect_forward_secrecy()) {
                level = std::max(level, SecurityLevel::High);
            }

            if (tls_session->has_client_certificate() && tls_session->verify_client_certificate()) {
                level = std::max(level, SecurityLevel::Maximum);
            }
        }

        // Adjust based on IP reputation
        if (ip_validator_->is_private_address(remote_ip)) {
            level = std::max(level, SecurityLevel::Standard); // Private networks get some trust
        }

        return level;
    }

    // Security Policy Templates Implementation
    ConnectionSecurityConfig SecurityPolicyTemplates::get_minimal_security()
    {
        ConnectionSecurityConfig config;
        config.encryption_policy = EncryptionPolicy::Optional;
        config.min_security_level = SecurityLevel::Minimal;
        config.allow_weak_ciphers = true;
        config.require_perfect_forward_secrecy = false;
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_standard_security()
    {
        ConnectionSecurityConfig config;
        config.encryption_policy = EncryptionPolicy::Preferred;
        config.min_security_level = SecurityLevel::Standard;
        config.require_perfect_forward_secrecy = true;
        config.force_tls_for_auth = true;
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_high_security()
    {
        ConnectionSecurityConfig config;
        config.encryption_policy = EncryptionPolicy::Required;
        config.min_security_level = SecurityLevel::High;
        config.require_perfect_forward_secrecy = true;
        config.require_mutual_auth = true;
        config.validate_client_certificates = true;
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_maximum_security()
    {
        ConnectionSecurityConfig config;
        config.encryption_policy = EncryptionPolicy::Mandatory;
        config.min_security_level = SecurityLevel::Maximum;
        config.require_perfect_forward_secrecy = true;
        config.require_mutual_auth = true;
        config.validate_client_certificates = true;
        config.allow_weak_ciphers = false;
        config.allow_compression = false;
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_financial_services_config()
    {
        auto config = get_high_security();
        config.min_security_level = SecurityLevel::Maximum;
        config.encryption_policy = EncryptionPolicy::Mandatory;
        config.require_mutual_auth = true;
        config.validate_client_certificates = true;
        config.rate_limiting.max_connections_per_ip = 20;
        config.rate_limiting.max_attempts_per_minute = 10;
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_healthcare_config()
    {
        auto config = get_high_security();
        config.min_security_level = SecurityLevel::High;
        config.encryption_policy = EncryptionPolicy::Required;
        config.enable_security_audit = true;
        config.audit_log_path = "/var/log/scratchbird/healthcare_security.log";
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_government_config()
    {
        auto config = get_maximum_security();
        config.enable_geo_blocking = true;
        config.allowed_countries = {"US"}; // Example restriction
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_development_config()
    {
        auto config = get_minimal_security();
        config.allowed_ip_ranges = {"127.0.0.0/8", "192.168.0.0/16", "10.0.0.0/8"};
        config.rate_limiting.max_connections_per_ip = 100;
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_pci_dss_compliant_config()
    {
        auto config = get_high_security();
        config.encryption_policy = EncryptionPolicy::Mandatory;
        config.require_perfect_forward_secrecy = true;
        config.allow_weak_ciphers = false;
        config.enable_security_audit = true;
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_hipaa_compliant_config()
    {
        auto config = get_high_security();
        config.encryption_policy = EncryptionPolicy::Required;
        config.enable_security_audit = true;
        config.audit_log_path = "/var/log/scratchbird/hipaa_audit.log";
        return config;
    }

    ConnectionSecurityConfig SecurityPolicyTemplates::get_gdpr_compliant_config()
    {
        auto config = get_standard_security();
        config.enable_security_audit = true;
        config.enable_geo_blocking = true;
        config.blocked_countries = {}; // Would be configured based on data processing agreements
        return config;
    }

    // Utility functions
    std::string security_level_to_string(SecurityLevel level)
    {
        switch (level) {
        case SecurityLevel::Minimal:
            return "Minimal";
        case SecurityLevel::Standard:
            return "Standard";
        case SecurityLevel::High:
            return "High";
        case SecurityLevel::Maximum:
            return "Maximum";
        default:
            return "Unknown";
        }
    }

    SecurityLevel parse_security_level(const std::string& level_str)
    {
        if (level_str == "Minimal")
            return SecurityLevel::Minimal;
        if (level_str == "Standard")
            return SecurityLevel::Standard;
        if (level_str == "High")
            return SecurityLevel::High;
        if (level_str == "Maximum")
            return SecurityLevel::Maximum;
        return SecurityLevel::Standard; // Default
    }

    std::string encryption_policy_to_string(EncryptionPolicy policy)
    {
        switch (policy) {
        case EncryptionPolicy::Optional:
            return "Optional";
        case EncryptionPolicy::Preferred:
            return "Preferred";
        case EncryptionPolicy::Required:
            return "Required";
        case EncryptionPolicy::Mandatory:
            return "Mandatory";
        default:
            return "Unknown";
        }
    }

    EncryptionPolicy parse_encryption_policy(const std::string& policy_str)
    {
        if (policy_str == "Optional")
            return EncryptionPolicy::Optional;
        if (policy_str == "Preferred")
            return EncryptionPolicy::Preferred;
        if (policy_str == "Required")
            return EncryptionPolicy::Required;
        if (policy_str == "Mandatory")
            return EncryptionPolicy::Mandatory;
        return EncryptionPolicy::Preferred; // Default
    }

    bool is_weak_cipher_suite(const std::string& cipher_suite)
    {
        // Check for known weak cipher patterns
        std::vector<std::string> weak_patterns = {"NULL", "EXPORT", "DES", "RC4",
                                                  "MD5",  "SHA1",   "3DES"};

        std::string upper_cipher = cipher_suite;
        std::transform(upper_cipher.begin(), upper_cipher.end(), upper_cipher.begin(), ::toupper);

        for (const auto& pattern : weak_patterns) {
            if (upper_cipher.find(pattern) != std::string::npos) {
                return true;
            }
        }

        return false;
    }

    bool has_perfect_forward_secrecy(const std::string& cipher_suite)
    {
        return cipher_suite.find("ECDHE") != std::string::npos ||
               cipher_suite.find("DHE") != std::string::npos;
    }

    SecurityLevel assess_cipher_security_level(const std::string& cipher_suite)
    {
        if (is_weak_cipher_suite(cipher_suite)) {
            return SecurityLevel::Minimal;
        }

        if (has_perfect_forward_secrecy(cipher_suite)) {
            if (cipher_suite.find("AESGCM") != std::string::npos ||
                cipher_suite.find("CHACHA20") != std::string::npos) {
                return SecurityLevel::High;
            }
            return SecurityLevel::Standard;
        }

        return SecurityLevel::Standard;
    }

} // namespace ScratchBird
