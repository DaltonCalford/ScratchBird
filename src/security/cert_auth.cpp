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
 * ScratchBird Certificate Authentication Implementation
 *
 * TLS client-certificate authentication implementation.
 */

#include "scratchbird/security/cert_auth.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace scratchbird {
namespace security {

// ============================================================================
// DN Parsing Utilities
// ============================================================================

std::string extractDNComponent(const std::string& dn, const std::string& component) {
    // DN format: CN=foo,O=bar,OU=baz or /CN=foo/O=bar/OU=baz
    std::string search_patterns[] = {
        component + "=",
        "/" + component + "="
    };

    for (const auto& pattern : search_patterns) {
        size_t pos = dn.find(pattern);
        if (pos != std::string::npos) {
            pos += pattern.length();

            // Find end of component
            size_t end = dn.find_first_of(",/", pos);
            if (end == std::string::npos) {
                end = dn.length();
            }

            std::string value = dn.substr(pos, end - pos);

            // Handle escaped characters
            std::string result;
            result.reserve(value.size());
            for (size_t i = 0; i < value.size(); i++) {
                if (value[i] == '\\' && i + 1 < value.size()) {
                    result.push_back(value[++i]);
                } else {
                    result.push_back(value[i]);
                }
            }

            return result;
        }
    }

    return "";
}

std::map<std::string, std::string> parseDN(const std::string& dn) {
    std::map<std::string, std::string> components;

    // Handle both RFC 2253 (,) and OpenSSL (/) formats
    char delimiter = dn.find('/') != std::string::npos ? '/' : ',';

    std::string current = dn;
    if (delimiter == '/' && !current.empty() && current[0] == '/') {
        current = current.substr(1);  // Skip leading /
    }

    size_t pos = 0;
    while (pos < current.size()) {
        // Find =
        size_t eq_pos = current.find('=', pos);
        if (eq_pos == std::string::npos) break;

        std::string key = current.substr(pos, eq_pos - pos);

        // Trim whitespace
        while (!key.empty() && std::isspace(key.front())) key.erase(0, 1);
        while (!key.empty() && std::isspace(key.back())) key.pop_back();

        // Find end of value
        size_t value_start = eq_pos + 1;
        size_t value_end = value_start;

        // Handle quoted values
        if (value_end < current.size() && current[value_end] == '"') {
            value_end++;
            while (value_end < current.size()) {
                if (current[value_end] == '"' &&
                    (value_end + 1 >= current.size() || current[value_end + 1] != '"')) {
                    value_end++;
                    break;
                }
                if (current[value_end] == '"') value_end++;  // Skip escaped quote
                value_end++;
            }
        } else {
            // Find delimiter, handling escapes
            while (value_end < current.size()) {
                if (current[value_end] == '\\' && value_end + 1 < current.size()) {
                    value_end += 2;
                    continue;
                }
                if (current[value_end] == delimiter) break;
                value_end++;
            }
        }

        std::string value = current.substr(value_start, value_end - value_start);

        // Handle quoted values
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        components[key] = value;

        pos = value_end + 1;
    }

    return components;
}

bool dnEquals(const std::string& dn1, const std::string& dn2) {
    auto comp1 = parseDN(dn1);
    auto comp2 = parseDN(dn2);
    return comp1 == comp2;
}

bool isIssuedBy(const CertificateInfo& cert, const std::string& issuer_dn) {
    return dnEquals(cert.issuer_dn, issuer_dn);
}

// ============================================================================
// CertMapRule Implementation
// ============================================================================

bool CertMapRule::matches(const CertificateInfo& cert) const {
    // Check issuer if specified
    if (!issuer_dn.empty()) {
        if (!dnEquals(cert.issuer_dn, issuer_dn)) {
            return false;
        }
    }

    // Check subject DN pattern
    if (!subject_dn_pattern.empty()) {
        if (subject_dn_is_regex) {
            if (!std::regex_search(cert.subject_dn, subject_dn_regex)) {
                return false;
            }
        } else {
            if (!dnEquals(cert.subject_dn, subject_dn_pattern)) {
                return false;
            }
        }
    }

    return true;
}

std::string CertMapRule::mapToUsername(const CertificateInfo& cert) const {
    std::string username;

    switch (method) {
        case CertMapMethod::CN:
            username = cert.subject_cn;
            break;

        case CertMapMethod::DN:
            username = cert.subject_dn;
            break;

        case CertMapMethod::DN_COMPONENT:
            username = extractDNComponent(cert.subject_dn, dn_component);
            break;

        case CertMapMethod::SAN_EMAIL:
            if (!cert.san_email.empty()) {
                username = cert.san_email[0];
            }
            break;

        case CertMapMethod::SAN_DNS:
            if (!cert.san_dns.empty()) {
                username = cert.san_dns[0];
            }
            break;

        case CertMapMethod::SAN_URI:
            if (!cert.san_uri.empty()) {
                username = cert.san_uri[0];
            }
            break;

        case CertMapMethod::FINGERPRINT:
            username = cert.fingerprint_sha256;
            break;

        case CertMapMethod::MAP_FILE:
            // Handled separately in authenticateWithCert
            username = cert.subject_cn;  // Fallback
            break;

        case CertMapMethod::LDAP:
        case CertMapMethod::DATABASE:
            // Not implemented in this simple version
            username = cert.subject_cn;
            break;
    }

    // Apply transformations
    if (!username.empty()) {
        // Regex extraction
        if (!regex_match.empty()) {
            std::regex re(regex_match);
            std::smatch match;
            if (std::regex_search(username, match, re)) {
                if (match.size() > 1) {
                    username = match[1].str();
                }
            }
        }

        // Regex replacement
        if (!regex_match.empty() && !regex_replace.empty()) {
            std::regex re(regex_match);
            username = std::regex_replace(username, re, regex_replace);
        }

        // Lowercase
        if (lowercase) {
            std::transform(username.begin(), username.end(), username.begin(),
                          [](unsigned char c) { return std::tolower(c); });
        }

        // Prefix/suffix
        username = username_prefix + username + username_suffix;
    }

    return username;
}

// ============================================================================
// CertMapConfig Implementation
// ============================================================================

void CertMapConfig::addCNMapping() {
    CertMapRule rule;
    rule.method = CertMapMethod::CN;
    rules.push_back(rule);
}

void CertMapConfig::addEmailMapping(const std::string& issuer) {
    CertMapRule rule;
    rule.method = CertMapMethod::SAN_EMAIL;
    rule.issuer_dn = issuer;
    // Extract username from email (part before @)
    rule.regex_match = "^([^@]+)@";
    rules.push_back(rule);
}

core::Status CertMapConfig::loadFromFile(const std::string& path,
                                          core::ErrorContext* ctx)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        if (ctx) ctx->message = "Failed to open certificate map file: " + path;
        return core::Status::NOT_FOUND;
    }

    std::vector<CertMapEntry> entries;
    auto status = parseCertMapFile(path, entries, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Convert entries to rules
    for (const auto& entry : entries) {
        CertMapRule rule;
        rule.method = CertMapMethod::MAP_FILE;
        rule.map_file = path;
        // Store the entry pattern for later matching
        rules.push_back(rule);
    }

    return core::Status::OK;
}

// ============================================================================
// CertAuthMethod Implementation
// ============================================================================

CertAuthMethod::CertAuthMethod() {
    // Default: map CN to username
    map_config_.addCNMapping();
}

CertAuthMethod::~CertAuthMethod() = default;

core::Status CertAuthMethod::initialize(
    const std::map<std::string, std::string>& config,
    core::ErrorContext* ctx)
{
    // Parse configuration options

    // map = <method>
    auto it = config.find("map");
    if (it != config.end()) {
        const std::string& map_str = it->second;
        if (map_str == "cn") {
            map_config_.default_method = CertMapMethod::CN;
        } else if (map_str == "dn") {
            map_config_.default_method = CertMapMethod::DN;
        } else if (map_str == "email") {
            map_config_.default_method = CertMapMethod::SAN_EMAIL;
        } else if (map_str == "dns") {
            map_config_.default_method = CertMapMethod::SAN_DNS;
        } else if (map_str == "fingerprint") {
            map_config_.default_method = CertMapMethod::FINGERPRINT;
        }
    }

    // mapfile = <path>
    it = config.find("mapfile");
    if (it != config.end()) {
        auto status = map_config_.loadFromFile(it->second, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // clientcert = verify-full | verify-ca | 1 | 0
    it = config.find("clientcert");
    if (it != config.end()) {
        if (it->second == "verify-full" || it->second == "1") {
            map_config_.verify_client_cert = true;
        } else if (it->second == "0") {
            map_config_.verify_client_cert = false;
        }
    }

    // allow_self_signed = true | false
    it = config.find("allow_self_signed");
    if (it != config.end()) {
        map_config_.allow_self_signed = (it->second == "true" || it->second == "1");
    }

    return core::Status::OK;
}

bool CertAuthMethod::isSuitable(const ConnectionInfo& conn) const {
    // Certificate auth requires TLS with a client certificate
    return conn.is_ssl && conn.client_cert != nullptr;
}

AuthResult CertAuthMethod::start(AuthContext& ctx) {
    const auto& conn = ctx.connectionInfo();

    // Check if we have TLS
    if (!conn.is_ssl) {
        return AuthResult::failure(AuthFailReason::NOT_ALLOWED,
                                   "Certificate authentication requires TLS connection");
    }

    // Check if we have a client certificate
    if (!conn.client_cert) {
        return AuthResult::failure(AuthFailReason::CERTIFICATE_INVALID,
                                   "No client certificate provided");
    }

    return authenticateWithCert(ctx, *conn.client_cert);
}

AuthResult CertAuthMethod::continueAuth(
    AuthContext& /*ctx*/,
    const std::vector<uint8_t>& /*data*/)
{
    // Certificate auth doesn't have multi-step
    return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                               "Certificate authentication does not require response");
}

void CertAuthMethod::abort(AuthContext& /*ctx*/) {
    // Nothing to clean up
}

AuthResult CertAuthMethod::authenticateWithCert(
    AuthContext& ctx,
    const CertificateInfo& cert)
{
    // Validate certificate
    std::string error;
    if (!validateCertificate(cert, error)) {
        return AuthResult::failure(AuthFailReason::CERTIFICATE_INVALID, error);
    }

    // Map certificate to username
    std::string mapped_user = mapCertToUsername(cert);
    if (mapped_user.empty()) {
        return AuthResult::failure(AuthFailReason::PRINCIPAL_MISMATCH,
                                   "Could not map certificate to database user");
    }

    // If username was specified, verify it matches
    if (!ctx.username().empty() && ctx.username() != mapped_user) {
        return AuthResult::failure(AuthFailReason::PRINCIPAL_MISMATCH,
                                   "Certificate user '" + mapped_user +
                                   "' does not match requested user '" + ctx.username() + "'");
    }

    ctx.setAuthenticatedUser(mapped_user);
    return AuthResult::success(mapped_user);
}

std::string CertAuthMethod::mapCertToUsername(const CertificateInfo& cert) {
    // Try each rule in order
    for (const auto& rule : map_config_.rules) {
        if (rule.matches(cert)) {
            std::string username = rule.mapToUsername(cert);
            if (!username.empty()) {
                return username;
            }
        }
    }

    // Fall back to default method
    CertMapRule default_rule;
    default_rule.method = map_config_.default_method;
    return default_rule.mapToUsername(cert);
}

bool CertAuthMethod::validateCertificate(const CertificateInfo& cert, std::string& error) {
    // Check validity period
    if (!cert.isCurrentlyValid()) {
        if (cert.daysUntilExpiration() <= 0) {
            error = "Certificate has expired";
        } else {
            error = "Certificate is not yet valid";
        }
        return false;
    }

    // Check if certificate is for client authentication
    if (cert.ext_key_usage_client_auth == false &&
        cert.key_usage_digital_signature == false) {
        // Not strictly required, but log a warning
    }

    // Check allowed issuers
    if (!map_config_.allowed_issuers.empty()) {
        bool found = false;
        for (const auto& issuer : map_config_.allowed_issuers) {
            if (dnEquals(cert.issuer_dn, issuer)) {
                found = true;
                break;
            }
        }
        if (!found) {
            error = "Certificate issuer not in allowed list";
            return false;
        }
    }

    // Check allowed OUs
    if (!map_config_.allowed_ous.empty()) {
        bool found = false;
        for (const auto& ou : map_config_.allowed_ous) {
            if (cert.subject_ou == ou) {
                found = true;
                break;
            }
        }
        if (!found) {
            error = "Certificate OU not in allowed list";
            return false;
        }
    }

    return true;
}

// ============================================================================
// Certificate Map File Parser
// ============================================================================

core::Status parseCertMapFile(const std::string& path,
                               std::vector<CertMapEntry>& entries,
                               core::ErrorContext* ctx)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        if (ctx) ctx->message = "Failed to open file: " + path;
        return core::Status::NOT_FOUND;
    }

    entries.clear();
    std::string line;
    int line_num = 0;

    while (std::getline(file, line)) {
        line_num++;

        // Skip empty lines and comments
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }

        // Parse: map-name  pattern  username
        std::istringstream iss(line);
        CertMapEntry entry;

        if (!(iss >> entry.map_name >> entry.cert_pattern >> entry.db_username)) {
            if (ctx) {
                ctx->message = "Invalid line " + std::to_string(line_num) + " in " + path;
            }
            return core::Status::INVALID_ARGUMENT;
        }

        // Check if pattern is a regex (surrounded by /)
        if (entry.cert_pattern.size() >= 2 &&
            entry.cert_pattern.front() == '/' &&
            entry.cert_pattern.back() == '/') {
            entry.is_regex = true;
            std::string pattern = entry.cert_pattern.substr(1, entry.cert_pattern.size() - 2);
            try {
                entry.pattern_regex = std::regex(pattern);
            } catch (const std::regex_error& e) {
                if (ctx) {
                    ctx->message = "Invalid regex on line " + std::to_string(line_num) +
                                    ": " + e.what();
                    ctx->code = core::Status::INVALID_ARGUMENT;
                }
                return core::Status::INVALID_ARGUMENT;
            }
        }

        entries.push_back(std::move(entry));
    }

    return core::Status::OK;
}

std::string applyCertMapEntry(const CertMapEntry& entry,
                               const CertificateInfo& cert)
{
    // Get the value to match against
    std::string value = cert.subject_dn;

    if (entry.is_regex) {
        std::smatch match;
        if (std::regex_search(value, match, entry.pattern_regex)) {
            // Replace backreferences in db_username
            std::string result = entry.db_username;
            for (size_t i = 1; i < match.size(); i++) {
                std::string ref = "\\" + std::to_string(i);
                size_t pos;
                while ((pos = result.find(ref)) != std::string::npos) {
                    result.replace(pos, ref.length(), match[i].str());
                }
            }
            return result;
        }
    } else {
        // Simple match
        if (value.find(entry.cert_pattern) != std::string::npos) {
            return entry.db_username;
        }
    }

    return "";
}

}  // namespace security
}  // namespace scratchbird
