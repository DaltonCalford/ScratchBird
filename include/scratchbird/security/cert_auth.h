/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * ScratchBird Certificate Authentication
 *
 * TLS client-certificate authentication method.
 *
 * Implements inbound certificate authentication with
 * configurable certificate-to-user mapping.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <regex>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/security/auth_method.h"
#include "scratchbird/security/tls_config.h"

namespace scratchbird {
namespace security {

// ============================================================================
// Certificate Mapping
// ============================================================================

/**
 * Certificate mapping method
 *
 * Determines how the certificate is mapped to a database username.
 */
enum class CertMapMethod : uint8_t {
    CN = 0,             // Common Name
    DN = 1,             // Full Distinguished Name
    DN_COMPONENT = 2,   // Specific DN component (e.g., emailAddress)
    SAN_EMAIL = 3,      // Subject Alternative Name email
    SAN_DNS = 4,        // Subject Alternative Name DNS
    SAN_URI = 5,        // Subject Alternative Name URI
    FINGERPRINT = 6,    // Certificate fingerprint (SHA-256)
    MAP_FILE = 7,       // External mapping file
    LDAP = 8,           // LDAP lookup
    DATABASE = 9        // Database table lookup
};

/**
 * Certificate mapping rule
 */
struct CertMapRule {
    // Matching criteria
    std::string issuer_dn;          // Required issuer DN (empty = any)
    std::string subject_dn_pattern; // Subject DN regex pattern
    std::regex subject_dn_regex;
    bool subject_dn_is_regex = false;

    // Mapping
    CertMapMethod method = CertMapMethod::CN;
    std::string dn_component;       // For DN_COMPONENT method (e.g., "emailAddress")
    std::string map_file;           // For MAP_FILE method
    std::string ldap_url;           // For LDAP method
    std::string table_name;         // For DATABASE method

    // Username transformation
    std::string username_prefix;    // Prefix to add
    std::string username_suffix;    // Suffix to add
    bool lowercase = false;         // Convert to lowercase
    std::string regex_match;        // Extract username via regex
    std::string regex_replace;      // Replace pattern

    /**
     * Apply mapping to extract username from certificate
     */
    std::string mapToUsername(const CertificateInfo& cert) const;

    /**
     * Check if certificate matches this rule
     */
    bool matches(const CertificateInfo& cert) const;
};

/**
 * Certificate mapping configuration
 */
struct CertMapConfig {
    // Mapping rules (evaluated in order)
    std::vector<CertMapRule> rules;

    // Default mapping if no rule matches
    CertMapMethod default_method = CertMapMethod::CN;

    // Verification options
    bool verify_client_cert = true;         // Require valid cert
    bool allow_self_signed = false;         // Allow self-signed certs
    bool check_revocation = false;          // Check CRL/OCSP

    // Additional constraints
    std::vector<std::string> allowed_issuers;  // Only these issuers
    std::vector<std::string> allowed_ous;      // Only these OUs

    /**
     * Load mapping configuration from file
     */
    core::Status loadFromFile(const std::string& path,
                               core::ErrorContext* ctx = nullptr);

    /**
     * Add a simple CN mapping rule
     */
    void addCNMapping();

    /**
     * Add an email mapping rule
     */
    void addEmailMapping(const std::string& issuer_dn = "");
};

// ============================================================================
// Certificate Authentication Method
// ============================================================================

/**
 * Certificate Authentication Method
 *
 * Authenticates users based on TLS client certificates.
 * Requires TLS connection with client certificate verification enabled.
 */
class CertAuthMethod : public AuthMethod {
public:
    CertAuthMethod();
    ~CertAuthMethod();

    AuthType type() const override { return AuthType::CERTIFICATE; }
    const char* name() const override { return "cert"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                             core::ErrorContext* ctx = nullptr) override;

    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx,
                             const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;

    /**
     * Check if suitable for connection
     * (requires TLS with client certificate)
     */
    bool isSuitable(const ConnectionInfo& conn) const override;

    /**
     * Get mapping configuration
     */
    const CertMapConfig& mapConfig() const { return map_config_; }

    /**
     * Set mapping configuration
     */
    void setMapConfig(const CertMapConfig& config) { map_config_ = config; }

private:
    AuthResult authenticateWithCert(AuthContext& ctx, const CertificateInfo& cert);
    std::string mapCertToUsername(const CertificateInfo& cert);
    bool validateCertificate(const CertificateInfo& cert, std::string& error);

    CertMapConfig map_config_;
};

// ============================================================================
// Certificate Map File Parser
// ============================================================================

/**
 * Certificate map file format (similar to PostgreSQL pg_ident.conf):
 *
 * # Comment
 * map-name  cert-pattern  db-username
 *
 * Examples:
 *   clientcert  /CN=([^/]+)/  \1
 *   clientcert  /emailAddress=([^@]+)@example.com/  \1
 */

/**
 * Certificate map entry
 */
struct CertMapEntry {
    std::string map_name;           // Map name
    std::string cert_pattern;       // Certificate attribute pattern (regex)
    std::string db_username;        // Database username (can use backrefs)
    std::regex pattern_regex;
    bool is_regex = false;
};

/**
 * Parse certificate map file
 */
core::Status parseCertMapFile(const std::string& path,
                               std::vector<CertMapEntry>& entries,
                               core::ErrorContext* ctx = nullptr);

/**
 * Apply certificate map entry to extract username
 */
std::string applyCertMapEntry(const CertMapEntry& entry,
                               const CertificateInfo& cert);

// ============================================================================
// Certificate Utilities
// ============================================================================

/**
 * Extract DN component by OID or name
 *
 * @param dn Full distinguished name
 * @param component Component name (CN, O, OU, emailAddress, etc.)
 * @return Component value or empty string
 */
std::string extractDNComponent(const std::string& dn,
                                const std::string& component);

/**
 * Parse DN into components
 */
std::map<std::string, std::string> parseDN(const std::string& dn);

/**
 * Compare DNs for equality (handles ordering differences)
 */
bool dnEquals(const std::string& dn1, const std::string& dn2);

/**
 * Check if certificate is issued by specific CA
 */
bool isIssuedBy(const CertificateInfo& cert, const std::string& issuer_dn);

}  // namespace security
}  // namespace scratchbird
