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
 * ScratchBird OAuth 2.0 / OpenID Connect Authentication
 *
 * Alpha 3 Phase 3.5: Security Suite - Enterprise
 *
 * Implements OAuth 2.0 and OIDC authentication with:
 * - Authorization Code flow
 * - Client Credentials flow
 * - JWT token validation
 * - JWKS key rotation
 * - Token introspection
 * - Refresh token handling
 * - Multiple identity provider support
 */

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>
#include <optional>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/security/auth_method.h"

namespace scratchbird {
namespace security {

// ============================================================================
// OAuth/OIDC Types
// ============================================================================

/**
 * OAuth flow type
 */
enum class OAuthFlowType : uint8_t {
    AUTHORIZATION_CODE = 0,     // Standard web flow
    CLIENT_CREDENTIALS = 1,     // Machine-to-machine
    DEVICE_CODE = 2,            // Device authorization
    JWT_BEARER = 3              // JWT assertion
};

/**
 * Token type
 */
enum class TokenType : uint8_t {
    BEARER = 0,
    MAC = 1,
    JWT = 2
};

/**
 * JWT algorithm
 */
enum class JwtAlgorithm : uint8_t {
    HS256 = 0,      // HMAC SHA-256
    HS384 = 1,      // HMAC SHA-384
    HS512 = 2,      // HMAC SHA-512
    RS256 = 3,      // RSA SHA-256
    RS384 = 4,      // RSA SHA-384
    RS512 = 5,      // RSA SHA-512
    ES256 = 6,      // ECDSA P-256
    ES384 = 7,      // ECDSA P-384
    ES512 = 8,      // ECDSA P-521
    PS256 = 9,      // RSA-PSS SHA-256
    PS384 = 10,     // RSA-PSS SHA-384
    PS512 = 11,     // RSA-PSS SHA-512
    EDDSA = 12      // EdDSA (Ed25519)
};

/**
 * OAuth provider configuration
 */
struct OAuthProviderConfig {
    // Provider identification
    std::string provider_id;            // Unique identifier
    std::string display_name;           // Human-readable name

    // Endpoints (can be auto-discovered via .well-known)
    std::string issuer;                 // Token issuer (iss claim)
    std::string authorization_endpoint;
    std::string token_endpoint;
    std::string userinfo_endpoint;
    std::string jwks_uri;               // JSON Web Key Set URL
    std::string introspection_endpoint;
    std::string revocation_endpoint;

    // Client credentials
    std::string client_id;
    std::string client_secret;

    // Token validation
    std::vector<std::string> expected_audiences;  // Valid audience values
    std::chrono::seconds clock_skew_tolerance{60};
    bool validate_issuer = true;
    bool require_exp = true;

    // User mapping
    std::string username_claim;         // Claim for username (default: sub)
    std::string email_claim;            // Claim for email (default: email)
    std::string groups_claim;           // Claim for groups (default: groups)
    std::string roles_claim;            // Claim for roles (default: roles)

    // Scopes
    std::vector<std::string> required_scopes;
    std::vector<std::string> optional_scopes;

    // OIDC specific
    bool is_oidc = true;                // OpenID Connect provider
    bool verify_nonce = true;           // Verify nonce in ID token

    // Caching
    std::chrono::seconds jwks_cache_ttl{3600};
    std::chrono::seconds token_cache_ttl{300};

    // HTTP options
    std::chrono::seconds http_timeout{30};
    bool verify_tls = true;
    std::string ca_cert_file;
};

/**
 * JSON Web Key
 */
struct JsonWebKey {
    std::string kid;            // Key ID
    std::string kty;            // Key type (RSA, EC, oct)
    std::string use;            // Key use (sig, enc)
    std::string alg;            // Algorithm
    std::vector<std::string> key_ops;  // Key operations

    // RSA keys
    std::string n;              // Modulus (base64url)
    std::string e;              // Exponent (base64url)

    // EC keys
    std::string crv;            // Curve (P-256, P-384, P-521)
    std::string x;              // X coordinate (base64url)
    std::string y;              // Y coordinate (base64url)

    // Symmetric keys
    std::string k;              // Key value (base64url)

    // Certificate chain
    std::vector<std::string> x5c;  // X.509 certificate chain
    std::string x5t;            // X.509 SHA-1 thumbprint
    std::string x5t_s256;       // X.509 SHA-256 thumbprint
};

/**
 * JWT Header
 */
struct JwtHeader {
    JwtAlgorithm alg = JwtAlgorithm::RS256;
    std::string typ;            // Type (usually "JWT")
    std::string kid;            // Key ID
    std::string cty;            // Content type
};

/**
 * JWT Claims
 */
struct JwtClaims {
    // Registered claims
    std::string iss;            // Issuer
    std::string sub;            // Subject
    std::vector<std::string> aud;  // Audience
    std::chrono::system_clock::time_point exp;  // Expiration
    std::chrono::system_clock::time_point nbf;  // Not before
    std::chrono::system_clock::time_point iat;  // Issued at
    std::string jti;            // JWT ID

    // OIDC claims
    std::string nonce;
    std::string azp;            // Authorized party
    std::string at_hash;        // Access token hash
    std::string c_hash;         // Code hash
    std::string acr;            // Authentication context class reference
    std::vector<std::string> amr;  // Authentication methods references
    std::chrono::system_clock::time_point auth_time;

    // User info claims
    std::string name;
    std::string email;
    bool email_verified = false;
    std::string preferred_username;
    std::vector<std::string> groups;
    std::vector<std::string> roles;

    // Custom claims
    std::map<std::string, std::string> custom;
};

/**
 * Parsed JWT token
 */
struct JwtToken {
    std::string raw;            // Original token string
    JwtHeader header;
    JwtClaims claims;
    std::vector<uint8_t> signature;
    bool signature_verified = false;
};

/**
 * Token response from OAuth server
 */
struct TokenResponse {
    std::string access_token;
    std::string token_type;
    std::string refresh_token;
    std::string id_token;       // OIDC only
    uint32_t expires_in = 0;
    std::string scope;
    std::string error;
    std::string error_description;
};

// ============================================================================
// OAuth Authentication Method
// ============================================================================

/**
 * OAuth 2.0 Authentication Method
 *
 * Authenticates users via OAuth 2.0 access tokens.
 */
class OAuthAuthMethod : public AuthMethod {
public:
    OAuthAuthMethod();
    ~OAuthAuthMethod();

    AuthType type() const override { return AuthType::LDAP; }  // Reuse slot, should add OAUTH type
    const char* name() const override { return "oauth"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                            core::ErrorContext* ctx = nullptr) override;

    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx,
                            const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;

    /**
     * Add OAuth provider configuration
     */
    void addProvider(const OAuthProviderConfig& provider);

    /**
     * Get provider by ID
     */
    const OAuthProviderConfig* getProvider(const std::string& provider_id) const;

    /**
     * Validate access token
     */
    core::Status validateToken(const std::string& token,
                               JwtClaims& claims,
                               core::ErrorContext* ctx = nullptr);

    /**
     * Introspect token at OAuth server
     */
    core::Status introspectToken(const std::string& token,
                                 const std::string& provider_id,
                                 JwtClaims& claims,
                                 core::ErrorContext* ctx = nullptr);

    /**
     * Map token claims to database user
     */
    std::string mapClaimsToUser(const JwtClaims& claims);

    /**
     * Map token claims to database roles
     */
    std::vector<std::string> mapClaimsToRoles(const JwtClaims& claims);

private:
    /**
     * Verify JWT signature
     */
    core::Status verifyJwtSignature(const JwtToken& token,
                                    const OAuthProviderConfig& provider);

    /**
     * Validate JWT claims
     */
    core::Status validateJwtClaims(const JwtClaims& claims,
                                   const OAuthProviderConfig& provider);

    /**
     * Fetch and cache JWKS
     */
    core::Status fetchJwks(const std::string& jwks_uri,
                           std::vector<JsonWebKey>& keys);

    /**
     * Find key in JWKS by kid
     */
    const JsonWebKey* findKey(const std::string& provider_id,
                              const std::string& kid);

    std::map<std::string, OAuthProviderConfig> providers_;
    std::map<std::string, std::vector<JsonWebKey>> jwks_cache_;
    std::map<std::string, std::chrono::steady_clock::time_point> jwks_cache_time_;
    std::mutex mutex_;
};

// ============================================================================
// OpenID Connect Authentication
// ============================================================================

/**
 * OpenID Connect Authentication Method
 *
 * Extension of OAuth for identity with ID tokens.
 */
class OidcAuthMethod : public OAuthAuthMethod {
public:
    OidcAuthMethod();
    ~OidcAuthMethod();

    const char* name() const override { return "oidc"; }

    /**
     * Discover OIDC provider configuration
     */
    core::Status discoverProvider(const std::string& issuer,
                                  OAuthProviderConfig& config,
                                  core::ErrorContext* ctx = nullptr);

    /**
     * Validate ID token
     */
    core::Status validateIdToken(const std::string& id_token,
                                 const std::string& nonce,
                                 JwtClaims& claims,
                                 core::ErrorContext* ctx = nullptr);

    /**
     * Fetch user info from userinfo endpoint
     */
    core::Status fetchUserInfo(const std::string& access_token,
                               const std::string& provider_id,
                               JwtClaims& claims,
                               core::ErrorContext* ctx = nullptr);
};

// ============================================================================
// JWT Utility Functions
// ============================================================================

/**
 * Parse JWT token
 */
core::Status parseJwt(const std::string& token,
                      JwtToken& parsed,
                      core::ErrorContext* ctx = nullptr);

/**
 * Decode base64url encoded string
 */
std::vector<uint8_t> base64UrlDecode(const std::string& encoded);

/**
 * Encode to base64url
 */
std::string base64UrlEncode(const std::vector<uint8_t>& data);
std::string base64UrlEncode(const uint8_t* data, size_t len);

/**
 * Parse JSON claims
 */
core::Status parseJwtClaims(const std::string& payload_json,
                            JwtClaims& claims,
                            core::ErrorContext* ctx = nullptr);

/**
 * Parse JWKS JSON
 */
core::Status parseJwks(const std::string& jwks_json,
                       std::vector<JsonWebKey>& keys,
                       core::ErrorContext* ctx = nullptr);

/**
 * Verify signature with RSA public key
 */
bool verifyRsaSignature(const std::vector<uint8_t>& message,
                        const std::vector<uint8_t>& signature,
                        const JsonWebKey& key,
                        JwtAlgorithm algorithm);

/**
 * Verify signature with EC public key
 */
bool verifyEcSignature(const std::vector<uint8_t>& message,
                       const std::vector<uint8_t>& signature,
                       const JsonWebKey& key,
                       JwtAlgorithm algorithm);

/**
 * Verify signature with HMAC
 */
bool verifyHmacSignature(const std::vector<uint8_t>& message,
                         const std::vector<uint8_t>& signature,
                         const std::string& secret,
                         JwtAlgorithm algorithm);

/**
 * Get algorithm name string
 */
const char* jwtAlgorithmToString(JwtAlgorithm alg);

/**
 * Parse algorithm string
 */
bool parseJwtAlgorithm(const std::string& str, JwtAlgorithm& alg);

/**
 * Check if token is expired
 */
bool isTokenExpired(const JwtClaims& claims,
                    std::chrono::seconds clock_skew = std::chrono::seconds{0});

/**
 * Generate random state parameter
 */
std::string generateOAuthState(size_t length = 32);

/**
 * Generate PKCE code verifier and challenge
 */
struct PkceChallenge {
    std::string code_verifier;
    std::string code_challenge;
    std::string code_challenge_method;  // "S256" or "plain"
};
PkceChallenge generatePkceChallenge();

}  // namespace security
}  // namespace scratchbird
