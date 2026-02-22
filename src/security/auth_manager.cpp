/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Authentication Manager Implementation
 *
 * Alpha 3 Phase 3.4: Security Suite
 */

#include "scratchbird/security/auth_manager.h"
#include "scratchbird/security/scram_auth.h"
#include "scratchbird/security/cert_auth.h"

#include <openssl/rand.h>
#include <openssl/evp.h>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <syslog.h>
#endif

namespace scratchbird {
namespace security {

// ============================================================================
// HBA Rule Implementation
// ============================================================================

static bool parseIPAddress(const std::string& addr, struct in_addr* ipv4,
                           struct in6_addr* ipv6, bool& is_ipv6)
{
    if (inet_pton(AF_INET, addr.c_str(), ipv4) == 1) {
        is_ipv6 = false;
        return true;
    }
    if (inet_pton(AF_INET6, addr.c_str(), ipv6) == 1) {
        is_ipv6 = true;
        return true;
    }
    return false;
}

static bool matchIPv4(const struct in_addr& addr, const struct in_addr& network,
                      uint8_t prefix_length)
{
    uint32_t mask = prefix_length == 0 ? 0 : htonl(~((1 << (32 - prefix_length)) - 1));
    return (addr.s_addr & mask) == (network.s_addr & mask);
}

static bool matchIPv6(const struct in6_addr& addr, const struct in6_addr& network,
                      uint8_t prefix_length)
{
    for (int i = 0; i < 16; i++) {
        int bits = std::min(8, static_cast<int>(prefix_length) - i * 8);
        if (bits <= 0) break;

        uint8_t mask = bits == 8 ? 0xFF : static_cast<uint8_t>(~((1 << (8 - bits)) - 1));
        if ((addr.s6_addr[i] & mask) != (network.s6_addr[i] & mask)) {
            return false;
        }
    }
    return true;
}

bool HBARule::matches(const ConnectionInfo& conn, const std::string& username,
                      const std::string& database_name,
                      const std::vector<std::string>& roles) const
{
    // Check connection type
    switch (connection_type) {
        case HBAConnectionType::LOCAL:
            if (!conn.is_unix_socket) return false;
            break;

        case HBAConnectionType::HOST:
            if (conn.is_unix_socket) return false;
            break;

        case HBAConnectionType::HOSTSSL:
            if (conn.is_unix_socket || !conn.is_ssl) return false;
            break;

        case HBAConnectionType::HOSTNOSSL:
            if (conn.is_unix_socket || conn.is_ssl) return false;
            break;

        case HBAConnectionType::HOSTGSSENC:
            // GSSAPI encryption - not implemented yet
            return false;
    }

    // Check database
    if (database != "all") {
        if (database == "sameuser") {
            if (database_name != username) return false;
        } else if (database == "samerole") {
            bool found = false;
            for (const auto& role : roles) {
                if (role == database_name) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        } else if (database[0] == '@') {
            // Database list from file
            bool found = false;
            for (const auto& db : databases) {
                if (db == database_name) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        } else if (database_is_regex) {
            if (!std::regex_match(database_name, database_regex)) {
                return false;
            }
        } else {
            // Comma-separated list
            bool found = false;
            std::istringstream iss(database);
            std::string token;
            while (std::getline(iss, token, ',')) {
                if (token == database_name) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }

    // Check user
    if (user != "all") {
        if (user[0] == '+') {
            std::string role_name = user.substr(1);
            bool found = false;
            for (const auto& role : roles) {
                if (role == role_name) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        } else if (user[0] == '@') {
            // User list from file
            bool found = false;
            for (const auto& u : users) {
                if (u == username) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        } else if (user_is_regex) {
            if (!std::regex_match(username, user_regex)) {
                return false;
            }
        } else {
            // Comma-separated list
            bool found = false;
            std::istringstream iss(user);
            std::string token;
            while (std::getline(iss, token, ',')) {
                if (token == username) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }

    // Check address (only for non-local connections)
    if (!conn.is_unix_socket) {
        switch (ip_match_type) {
            case IPMatchType::ANY:
                break;  // Match any

            case IPMatchType::SINGLE:
            case IPMatchType::CIDR: {
                struct in_addr client_ipv4, rule_ipv4;
                struct in6_addr client_ipv6, rule_ipv6;
                bool client_is_ipv6, rule_is_ipv6;

                if (!parseIPAddress(conn.client_address, &client_ipv4,
                                   &client_ipv6, client_is_ipv6)) {
                    return false;
                }

                if (!parseIPAddress(address, &rule_ipv4, &rule_ipv6, rule_is_ipv6)) {
                    return false;
                }

                if (client_is_ipv6 != rule_is_ipv6) {
                    return false;
                }

                if (client_is_ipv6) {
                    if (!matchIPv6(client_ipv6, rule_ipv6, prefix_length)) {
                        return false;
                    }
                } else {
                    if (!matchIPv4(client_ipv4, rule_ipv4, prefix_length)) {
                        return false;
                    }
                }
                break;
            }

            case IPMatchType::SAMEHOST:
                if (conn.client_address != "127.0.0.1" &&
                    conn.client_address != "::1") {
                    return false;
                }
                break;

            case IPMatchType::SAMENET:
            {
                if (conn.server_address.empty()) {
                    return false;
                }

                struct in_addr client_ipv4, server_ipv4;
                struct in6_addr client_ipv6, server_ipv6;
                bool client_is_ipv6, server_is_ipv6;

                if (!parseIPAddress(conn.client_address, &client_ipv4,
                                   &client_ipv6, client_is_ipv6)) {
                    return false;
                }

                if (!parseIPAddress(conn.server_address, &server_ipv4,
                                   &server_ipv6, server_is_ipv6)) {
                    return false;
                }

                if (client_is_ipv6 != server_is_ipv6) {
                    return false;
                }

                uint8_t prefix = client_is_ipv6 ? 64 : 24;
                if (client_is_ipv6) {
                    if (!matchIPv6(client_ipv6, server_ipv6, prefix)) {
                        return false;
                    }
                } else {
                    if (!matchIPv4(client_ipv4, server_ipv4, prefix)) {
                        return false;
                    }
                }
                break;
            }
        }
    }

    return true;
}

core::Status HBARule::parse(const std::string& line, HBARule& rule,
                             core::ErrorContext* ctx)
{
    std::istringstream iss(line);
    std::string type_str;

    // Parse connection type
    if (!(iss >> type_str)) {
        if (ctx) ctx->message = "Missing connection type";
        return core::Status::INVALID_ARGUMENT;
    }

    if (type_str == "local") {
        rule.connection_type = HBAConnectionType::LOCAL;
    } else if (type_str == "host") {
        rule.connection_type = HBAConnectionType::HOST;
    } else if (type_str == "hostssl") {
        rule.connection_type = HBAConnectionType::HOSTSSL;
    } else if (type_str == "hostnossl") {
        rule.connection_type = HBAConnectionType::HOSTNOSSL;
    } else if (type_str == "hostgssenc") {
        rule.connection_type = HBAConnectionType::HOSTGSSENC;
    } else {
        if (ctx) ctx->message = "Invalid connection type: " + type_str;
        return core::Status::INVALID_ARGUMENT;
    }

    // Parse database
    if (!(iss >> rule.database)) {
        if (ctx) ctx->message = "Missing database";
        return core::Status::INVALID_ARGUMENT;
    }

    // Parse user
    if (!(iss >> rule.user)) {
        if (ctx) ctx->message = "Missing user";
        return core::Status::INVALID_ARGUMENT;
    }

    // Parse address (not for local)
    if (rule.connection_type != HBAConnectionType::LOCAL) {
        std::string addr;
        if (!(iss >> addr)) {
            if (ctx) ctx->message = "Missing address";
            return core::Status::INVALID_ARGUMENT;
        }

        if (addr == "all") {
            rule.ip_match_type = IPMatchType::ANY;
        } else if (addr == "samehost") {
            rule.ip_match_type = IPMatchType::SAMEHOST;
        } else if (addr == "samenet") {
            rule.ip_match_type = IPMatchType::SAMENET;
        } else {
            // CIDR or single IP
            size_t slash_pos = addr.find('/');
            if (slash_pos != std::string::npos) {
                rule.ip_match_type = IPMatchType::CIDR;
                rule.address = addr.substr(0, slash_pos);
                rule.prefix_length = static_cast<uint8_t>(
                    std::stoi(addr.substr(slash_pos + 1)));
            } else {
                rule.ip_match_type = IPMatchType::SINGLE;
                rule.address = addr;
                rule.prefix_length = 128;  // Exact match
            }
        }
    }

    // Parse auth method
    std::string method_str;
    if (!(iss >> method_str)) {
        if (ctx) ctx->message = "Missing authentication method";
        return core::Status::INVALID_ARGUMENT;
    }

    if (!parseAuthType(method_str, rule.auth_type)) {
        if (ctx) ctx->message = "Invalid auth method: " + method_str;
        return core::Status::INVALID_ARGUMENT;
    }

    // Parse auth options
    std::string option;
    while (iss >> option) {
        size_t eq_pos = option.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = option.substr(0, eq_pos);
            std::string value = option.substr(eq_pos + 1);
            // Remove quotes if present
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            rule.auth_options[key] = value;
        }
    }

    return core::Status::OK;
}

// ============================================================================
// HBAConfig Implementation
// ============================================================================

HBAConfig::HBAConfig() = default;
HBAConfig::~HBAConfig() = default;

core::Status HBAConfig::loadFromFile(const std::string& path, core::ErrorContext* ctx) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (ctx) ctx->message = "Failed to open HBA file: " + path;
        return core::Status::NOT_FOUND;
    }

    config_path_ = path;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return parseFile(content, ctx);
}

core::Status HBAConfig::loadFromString(const std::string& content, core::ErrorContext* ctx) {
    return parseFile(content, ctx);
}

core::Status HBAConfig::parseFile(const std::string& content, core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.clear();

    std::istringstream stream(content);
    std::string line;
    int line_num = 0;

    while (std::getline(stream, line)) {
        line_num++;

        // Skip empty lines and comments
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }

        // Handle line continuations
        while (!line.empty() && line.back() == '\\') {
            line.pop_back();
            std::string next_line;
            if (std::getline(stream, next_line)) {
                line_num++;
                line += next_line;
            }
        }

        // Parse rule
        HBARule rule;
        rule.line_number = line_num;

        core::ErrorContext rule_ctx;
        auto status = HBARule::parse(line.substr(first), rule, &rule_ctx);
        if (status != core::Status::OK) {
            if (ctx) {
                ctx->message = "Line " + std::to_string(line_num) + ": " + rule_ctx.message;
                ctx->code = status;
            }
            return status;
        }

        rules_.push_back(std::move(rule));
    }

    return core::Status::OK;
}

void HBAConfig::addRule(const HBARule& rule) {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.push_back(rule);
}

void HBAConfig::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.clear();
}

const HBARule* HBAConfig::findMatchingRule(const ConnectionInfo& conn,
                                            const std::string& username,
                                            const std::string& database,
                                            const std::vector<std::string>& roles) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& rule : rules_) {
        if (rule.matches(conn, username, database, roles)) {
            return &rule;
        }
    }
    return nullptr;
}

core::Status HBAConfig::reload(core::ErrorContext* ctx) {
    if (config_path_.empty()) {
        return core::Status::OK;
    }
    return loadFromFile(config_path_, ctx);
}

// ============================================================================
// RateLimiter Implementation
// ============================================================================

RateLimiter::RateLimiter()
    : config_{}
{}

RateLimiter::RateLimiter(const Config& config)
    : config_(config)
{}

RateLimiter::~RateLimiter() = default;

std::string RateLimiter::makeKey(const std::string& username,
                                  const std::string& address) const {
    std::string key;
    if (config_.per_user && !username.empty()) {
        key += "u:" + username;
    }
    if (config_.per_address && !address.empty()) {
        if (!key.empty()) key += ":";
        key += "a:" + address;
    }
    return key;
}

void RateLimiter::cleanupOldAttempts(AttemptRecord& record) {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - config_.window;

    record.attempts.erase(
        std::remove_if(record.attempts.begin(), record.attempts.end(),
                       [cutoff](const auto& t) { return t < cutoff; }),
        record.attempts.end());
}

bool RateLimiter::allow(const std::string& username, const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = makeKey(username, address);
    if (key.empty()) return true;

    auto& record = records_[key];

    // Check if currently locked
    auto now = std::chrono::steady_clock::now();
    if (record.locked_until > now) {
        return false;
    }

    // Clean up old attempts
    cleanupOldAttempts(record);

    // Check if under limit
    return record.attempts.size() < config_.max_attempts;
}

void RateLimiter::recordFailure(const std::string& username, const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = makeKey(username, address);
    if (key.empty()) return;

    auto& record = records_[key];
    auto now = std::chrono::steady_clock::now();

    // Add attempt
    record.attempts.push_back(now);

    // Clean up old attempts
    cleanupOldAttempts(record);

    // Check if should lock
    if (record.attempts.size() >= config_.max_attempts) {
        record.locked_until = now + config_.lockout_duration;
    }
}

void RateLimiter::recordSuccess(const std::string& username, const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = makeKey(username, address);
    if (key.empty()) return;

    records_.erase(key);
}

bool RateLimiter::isLocked(const std::string& username, const std::string& address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = makeKey(username, address);
    if (key.empty()) return false;

    auto it = records_.find(key);
    if (it == records_.end()) return false;

    return it->second.locked_until > std::chrono::steady_clock::now();
}

std::chrono::seconds RateLimiter::remainingLockout(const std::string& username,
                                                    const std::string& address) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = makeKey(username, address);
    if (key.empty()) return std::chrono::seconds(0);

    auto it = records_.find(key);
    if (it == records_.end()) return std::chrono::seconds(0);

    auto now = std::chrono::steady_clock::now();
    if (it->second.locked_until <= now) return std::chrono::seconds(0);

    return std::chrono::duration_cast<std::chrono::seconds>(
        it->second.locked_until - now);
}

void RateLimiter::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.clear();
}

// ============================================================================
// FileAuditLogger Implementation
// ============================================================================

FileAuditLogger::FileAuditLogger(const std::string& path)
    : path_(path)
{
    file_ = fopen(path.c_str(), "a");
}

FileAuditLogger::~FileAuditLogger() {
    if (file_) {
        fclose(file_);
    }
}

void FileAuditLogger::log(const AuthAuditEvent& event) {
    if (!file_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // Format timestamp
    auto time_t_ts = std::chrono::system_clock::to_time_t(event.timestamp);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&time_t_ts));

    // Format event type
    const char* type_str = "UNKNOWN";
    switch (event.type) {
        case AuthAuditEvent::Type::AUTH_START: type_str = "AUTH_START"; break;
        case AuthAuditEvent::Type::AUTH_SUCCESS: type_str = "AUTH_SUCCESS"; break;
        case AuthAuditEvent::Type::AUTH_FAILURE: type_str = "AUTH_FAILURE"; break;
        case AuthAuditEvent::Type::PASSWORD_CHANGE: type_str = "PASSWORD_CHANGE"; break;
        case AuthAuditEvent::Type::ACCOUNT_LOCKED: type_str = "ACCOUNT_LOCKED"; break;
        case AuthAuditEvent::Type::ACCOUNT_UNLOCKED: type_str = "ACCOUNT_UNLOCKED"; break;
        case AuthAuditEvent::Type::SESSION_START: type_str = "SESSION_START"; break;
        case AuthAuditEvent::Type::SESSION_END: type_str = "SESSION_END"; break;
    }

    fprintf(file_, "%s %s user=%s db=%s client=%s:%u method=%s",
            time_buf, type_str,
            event.username.c_str(),
            event.database.c_str(),
            event.client_address.c_str(),
            event.client_port,
            authTypeToString(event.auth_type));

    if (event.type == AuthAuditEvent::Type::AUTH_FAILURE) {
        fprintf(file_, " reason=%s", authFailReasonToString(event.failure_reason));
        if (!event.failure_message.empty()) {
            fprintf(file_, " message=\"%s\"", event.failure_message.c_str());
        }
    }

    fprintf(file_, "\n");
    fflush(file_);
}

void FileAuditLogger::flush() {
    if (file_) {
        std::lock_guard<std::mutex> lock(mutex_);
        fflush(file_);
    }
}

// ============================================================================
// SyslogAuditLogger Implementation
// ============================================================================

SyslogAuditLogger::SyslogAuditLogger(const std::string& ident)
    : ident_(ident)
{
#ifndef _WIN32
    openlog(ident_.c_str(), LOG_PID | LOG_NDELAY, LOG_AUTH);
#endif
}

SyslogAuditLogger::~SyslogAuditLogger() {
#ifndef _WIN32
    closelog();
#endif
}

void SyslogAuditLogger::log(const AuthAuditEvent& event) {
#ifdef _WIN32
    (void)event;
#else
    int priority = LOG_INFO;
    if (event.type == AuthAuditEvent::Type::AUTH_FAILURE) {
        priority = LOG_WARNING;
    } else if (event.type == AuthAuditEvent::Type::ACCOUNT_LOCKED) {
        priority = LOG_NOTICE;
    }

    const char* type_str = "UNKNOWN";
    switch (event.type) {
        case AuthAuditEvent::Type::AUTH_START: type_str = "AUTH_START"; break;
        case AuthAuditEvent::Type::AUTH_SUCCESS: type_str = "AUTH_SUCCESS"; break;
        case AuthAuditEvent::Type::AUTH_FAILURE: type_str = "AUTH_FAILURE"; break;
        case AuthAuditEvent::Type::PASSWORD_CHANGE: type_str = "PASSWORD_CHANGE"; break;
        case AuthAuditEvent::Type::ACCOUNT_LOCKED: type_str = "ACCOUNT_LOCKED"; break;
        case AuthAuditEvent::Type::ACCOUNT_UNLOCKED: type_str = "ACCOUNT_UNLOCKED"; break;
        case AuthAuditEvent::Type::SESSION_START: type_str = "SESSION_START"; break;
        case AuthAuditEvent::Type::SESSION_END: type_str = "SESSION_END"; break;
    }

    if (event.type == AuthAuditEvent::Type::AUTH_FAILURE) {
        syslog(priority, "%s user=%s db=%s client=%s:%u method=%s reason=%s",
               type_str,
               event.username.c_str(),
               event.database.c_str(),
               event.client_address.c_str(),
               event.client_port,
               authTypeToString(event.auth_type),
               authFailReasonToString(event.failure_reason));
    } else {
        syslog(priority, "%s user=%s db=%s client=%s:%u method=%s",
               type_str,
               event.username.c_str(),
               event.database.c_str(),
               event.client_address.c_str(),
               event.client_port,
               authTypeToString(event.auth_type));
    }
#endif
}

void SyslogAuditLogger::flush() {
    // Syslog doesn't need explicit flush
}

// ============================================================================
// Password Hashing Utilities
// ============================================================================

void generateSalt(std::vector<uint8_t>& salt, size_t length) {
    salt.resize(length);
    RAND_bytes(salt.data(), static_cast<int>(length));
}

bool constantTimeCompare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }

    volatile uint8_t result = 0;
    for (size_t i = 0; i < a.size(); i++) {
        result |= a[i] ^ b[i];
    }
    return result == 0;
}

core::Status hashPasswordPBKDF2(
    const std::string& password,
    const std::vector<uint8_t>& salt,
    uint32_t iterations,
    std::vector<uint8_t>& hash)
{
    hash.resize(32);  // SHA-256 output

    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            salt.data(), static_cast<int>(salt.size()),
            static_cast<int>(iterations),
            EVP_sha256(),
            32, hash.data()) != 1) {
        return core::Status::INTERNAL_ERROR;
    }

    return core::Status::OK;
}

// ============================================================================
// AuthManager Implementation
// ============================================================================

AuthManager::AuthManager() = default;

AuthManager::~AuthManager() {
    shutdown();
}

core::Status AuthManager::initialize(const AuthManagerConfig& config,
                                      core::ErrorContext* ctx)
{
    config_ = config;

    // Load HBA configuration
    if (config_.hba_enabled && !config_.hba_file.empty()) {
        auto status = hba_config_.loadFromFile(config_.hba_file, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // Initialize rate limiter
    if (config_.rate_limit_enabled) {
        rate_limiter_ = std::make_unique<RateLimiter>(config_.rate_limit);
    }

    // Initialize audit logger
    if (config_.audit_enabled && !config_.audit_log_file.empty()) {
        audit_logger_ = std::make_shared<FileAuditLogger>(config_.audit_log_file);
    }

    // Create default auth methods
    auth_methods_[AuthType::TRUST] = std::make_unique<TrustAuthMethod>();
    auth_methods_[AuthType::REJECT] = std::make_unique<RejectAuthMethod>();
    auth_methods_[AuthType::PEER] = std::make_unique<PeerAuthMethod>();
    auth_methods_[AuthType::SCRAM_SHA_256] = std::make_unique<ScramSHA256AuthMethod>();
    auth_methods_[AuthType::SCRAM_SHA_512] = std::make_unique<ScramSHA512AuthMethod>();
    auth_methods_[AuthType::CERTIFICATE] = std::make_unique<CertAuthMethod>();

    // Initialize all methods
    for (auto& [type, method] : auth_methods_) {
        std::map<std::string, std::string> method_config;
        auto status = method->initialize(method_config, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        // Set credential store if available
        if (credential_store_) {
            if (type == AuthType::SCRAM_SHA_256) {
                static_cast<ScramSHA256AuthMethod*>(method.get())->
                    setCredentialStore(credential_store_);
            } else if (type == AuthType::SCRAM_SHA_512) {
                static_cast<ScramSHA512AuthMethod*>(method.get())->
                    setCredentialStore(credential_store_);
            }
        }
    }

    initialized_.store(true);
    return core::Status::OK;
}

void AuthManager::shutdown() {
    initialized_.store(false);

    std::lock_guard<std::mutex> lock(active_auths_mutex_);
    active_auths_.clear();
    auth_methods_.clear();
}

void AuthManager::setCredentialStore(std::shared_ptr<CredentialStore> store) {
    credential_store_ = std::move(store);

    // Update SCRAM methods
    if (auth_methods_.count(AuthType::SCRAM_SHA_256)) {
        static_cast<ScramSHA256AuthMethod*>(auth_methods_[AuthType::SCRAM_SHA_256].get())->
            setCredentialStore(credential_store_);
    }
    if (auth_methods_.count(AuthType::SCRAM_SHA_512)) {
        static_cast<ScramSHA512AuthMethod*>(auth_methods_[AuthType::SCRAM_SHA_512].get())->
            setCredentialStore(credential_store_);
    }
}

void AuthManager::setAuditLogger(std::shared_ptr<AuditLogger> logger) {
    audit_logger_ = std::move(logger);
}

AuthResult AuthManager::startAuthentication(AuthContext& ctx) {
    stats_.total_authentications++;

    const auto& conn = ctx.connectionInfo();

    // Log auth start
    if (config_.log_connections) {
        logAuthEvent(AuthAuditEvent::Type::AUTH_START, ctx);
    }

    // Check rate limiting
    if (rate_limiter_ && !rate_limiter_->allow(ctx.username(), conn.client_address)) {
        stats_.rate_limited++;
        ctx.setFailure(AuthFailReason::RATE_LIMITED, "Too many failed attempts");
        logAuthEvent(AuthAuditEvent::Type::AUTH_FAILURE, ctx);
        return AuthResult::failure(AuthFailReason::RATE_LIMITED,
                                   "Too many failed authentication attempts");
    }

    // Find matching HBA rule
    const HBARule* rule = nullptr;
    if (config_.hba_enabled) {
        std::vector<std::string> roles;
        if (credential_store_) {
            UserCredential cred;
            if (credential_store_->getCredential(ctx.username(), cred) == core::Status::OK) {
                roles = cred.roles;
            }
        }

        rule = hba_config_.findMatchingRule(conn, ctx.username(),
                                            conn.database_name, roles);
        if (!rule) {
            stats_.hba_rejected++;
            ctx.setFailure(AuthFailReason::NOT_ALLOWED,
                          "No matching HBA rule for connection");
            logAuthEvent(AuthAuditEvent::Type::AUTH_FAILURE, ctx);
            return AuthResult::failure(AuthFailReason::NOT_ALLOWED,
                                       "No pg_hba.conf entry for host, user, database");
        }
    }

    // Use default auth type if no HBA
    AuthType auth_type = rule ? rule->auth_type : config_.default_auth_type;
    ctx.setAuthType(auth_type);

    // Get auth method
    auto it = auth_methods_.find(auth_type);
    if (it == auth_methods_.end()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR,
                                   "Authentication method not implemented");
    }

    AuthMethod* method = it->second.get();

    // Check if method is suitable for this connection
    if (!method->isSuitable(conn)) {
        return AuthResult::failure(AuthFailReason::NOT_ALLOWED,
                                   "Authentication method not suitable for this connection");
    }

    // Initialize method with options from HBA rule
    if (rule && !rule->auth_options.empty()) {
        method->initialize(rule->auth_options, nullptr);
    }

    // Start authentication
    AuthResult result = method->start(ctx);

    if (result.state == AuthState::SUCCESS) {
        stats_.successful_authentications++;
        if (rate_limiter_) {
            rate_limiter_->recordSuccess(ctx.username(), conn.client_address);
        }
        logAuthEvent(AuthAuditEvent::Type::AUTH_SUCCESS, ctx, result);
    } else if (result.state == AuthState::FAILURE) {
        stats_.failed_authentications++;
        if (rate_limiter_) {
            rate_limiter_->recordFailure(ctx.username(), conn.client_address);
        }
        logAuthEvent(AuthAuditEvent::Type::AUTH_FAILURE, ctx, result);
    }

    return result;
}

AuthResult AuthManager::continueAuthentication(AuthContext& ctx,
                                                const std::vector<uint8_t>& data)
{
    auto it = auth_methods_.find(ctx.authType());
    if (it == auth_methods_.end()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR,
                                   "Authentication method not found");
    }

    AuthResult result = it->second->continueAuth(ctx, data);

    if (result.state == AuthState::SUCCESS) {
        stats_.successful_authentications++;
        const auto& conn = ctx.connectionInfo();
        if (rate_limiter_) {
            rate_limiter_->recordSuccess(ctx.username(), conn.client_address);
        }
        logAuthEvent(AuthAuditEvent::Type::AUTH_SUCCESS, ctx, result);
    } else if (result.state == AuthState::FAILURE) {
        stats_.failed_authentications++;
        const auto& conn = ctx.connectionInfo();
        if (rate_limiter_) {
            rate_limiter_->recordFailure(ctx.username(), conn.client_address);
        }
        logAuthEvent(AuthAuditEvent::Type::AUTH_FAILURE, ctx, result);
    }

    return result;
}

void AuthManager::abortAuthentication(AuthContext& ctx) {
    auto it = auth_methods_.find(ctx.authType());
    if (it != auth_methods_.end()) {
        it->second->abort(ctx);
    }
}

bool AuthManager::verifyPassword(const std::string& username,
                                  const std::string& password)
{
    // Try SCRAM-SHA-256 first
    auto it = auth_methods_.find(AuthType::SCRAM_SHA_256);
    if (it != auth_methods_.end() && it->second->supportsPasswordVerification()) {
        return it->second->verifyPassword(username, password);
    }

    return false;
}

core::Status AuthManager::reloadHBA(core::ErrorContext* ctx) {
    return hba_config_.reload(ctx);
}

bool AuthManager::userExists(const std::string& username) {
    if (!credential_store_) return false;
    return credential_store_->userExists(username);
}

void AuthManager::logAuthEvent(AuthAuditEvent::Type type,
                                const AuthContext& ctx,
                                const AuthResult& result)
{
    if (!audit_logger_) return;

    AuthAuditEvent event;
    event.type = type;
    event.timestamp = std::chrono::system_clock::now();
    event.username = ctx.username();
    event.client_address = ctx.connectionInfo().client_address;
    event.client_port = ctx.connectionInfo().client_port;
    event.database = ctx.connectionInfo().database_name;
    event.protocol = ctx.connectionInfo().protocol;
    event.auth_type = ctx.authType();
    event.failure_reason = result.failure_reason;
    event.failure_message = result.failure_message;

    audit_logger_->log(event);
}

}  // namespace security
}  // namespace scratchbird
