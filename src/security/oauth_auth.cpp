/**
 * ScratchBird OAuth 2.0 / OpenID Connect Authentication Implementation
 *
 * Alpha 3 Phase 3.5: Security Suite - Enterprise
 */

#include "scratchbird/security/oauth_auth.h"

#include <algorithm>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/bn.h>

namespace scratchbird {
namespace security {

// ============================================================================
// OAuthAuthMethod Implementation
// ============================================================================

OAuthAuthMethod::OAuthAuthMethod() = default;
OAuthAuthMethod::~OAuthAuthMethod() = default;

core::Status OAuthAuthMethod::initialize(const std::map<std::string, std::string>& config,
                                         core::ErrorContext* ctx) {
    // Parse default provider configuration
    OAuthProviderConfig provider;

    auto it = config.find("provider_id");
    if (it != config.end()) {
        provider.provider_id = it->second;
    } else {
        provider.provider_id = "default";
    }

    it = config.find("issuer");
    if (it != config.end()) {
        provider.issuer = it->second;
    }

    it = config.find("client_id");
    if (it != config.end()) {
        provider.client_id = it->second;
    }

    it = config.find("client_secret");
    if (it != config.end()) {
        provider.client_secret = it->second;
    }

    it = config.find("jwks_uri");
    if (it != config.end()) {
        provider.jwks_uri = it->second;
    }

    it = config.find("token_endpoint");
    if (it != config.end()) {
        provider.token_endpoint = it->second;
    }

    it = config.find("userinfo_endpoint");
    if (it != config.end()) {
        provider.userinfo_endpoint = it->second;
    }

    it = config.find("introspection_endpoint");
    if (it != config.end()) {
        provider.introspection_endpoint = it->second;
    }

    it = config.find("username_claim");
    if (it != config.end()) {
        provider.username_claim = it->second;
    } else {
        provider.username_claim = "sub";
    }

    it = config.find("audience");
    if (it != config.end()) {
        provider.expected_audiences.push_back(it->second);
    }

    if (!provider.issuer.empty()) {
        addProvider(provider);
    }

    return core::Status::OK;
}

AuthResult OAuthAuthMethod::start(AuthContext& ctx) {
    // OAuth auth expects a bearer token from client
    ctx.setState(AuthState::IN_PROGRESS);

    AuthResult result;
    result.state = AuthState::IN_PROGRESS;
    result.requires_response = true;
    // Signal that we expect a bearer token
    result.response_data = {'B', 'E', 'A', 'R', 'E', 'R'};

    return result;
}

AuthResult OAuthAuthMethod::continueAuth(AuthContext& ctx,
                                         const std::vector<uint8_t>& data) {
    // Data should contain the access token
    std::string token(data.begin(), data.end());

    // Remove "Bearer " prefix if present
    if (token.substr(0, 7) == "Bearer ") {
        token = token.substr(7);
    }

    JwtClaims claims;
    core::ErrorContext err_ctx;

    auto status = validateToken(token, claims, &err_ctx);
    if (status != core::Status::OK) {
        ctx.setFailure(AuthFailReason::INVALID_CREDENTIALS, err_ctx.message);
        return AuthResult::failure(AuthFailReason::INVALID_CREDENTIALS, err_ctx.message);
    }

    // Map claims to user
    std::string username = mapClaimsToUser(claims);
    if (username.empty()) {
        ctx.setFailure(AuthFailReason::USER_NOT_FOUND, "No username in token");
        return AuthResult::failure(AuthFailReason::USER_NOT_FOUND, "No username in token");
    }

    // Map claims to roles
    auto roles = mapClaimsToRoles(claims);

    ctx.setState(AuthState::SUCCESS);
    ctx.setAuthenticatedUser(username);
    for (const auto& role : roles) {
        ctx.addRole(role);
    }

    AuthResult result = AuthResult::success(username);
    result.roles = roles;
    return result;
}

void OAuthAuthMethod::abort(AuthContext& ctx) {
    ctx.setState(AuthState::FAILURE);
}

void OAuthAuthMethod::addProvider(const OAuthProviderConfig& provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    providers_[provider.provider_id] = provider;
}

const OAuthProviderConfig* OAuthAuthMethod::getProvider(const std::string& provider_id) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    auto it = providers_.find(provider_id);
    if (it != providers_.end()) {
        return &it->second;
    }
    return nullptr;
}

core::Status OAuthAuthMethod::validateToken(const std::string& token,
                                            JwtClaims& claims,
                                            core::ErrorContext* ctx) {
    // Parse JWT
    JwtToken parsed;
    auto status = parseJwt(token, parsed, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Find provider by issuer
    const OAuthProviderConfig* provider = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, config] : providers_) {
            if (config.issuer == parsed.claims.iss) {
                provider = &config;
                break;
            }
        }
    }

    if (!provider) {
        // Try first provider if only one exists
        std::lock_guard<std::mutex> lock(mutex_);
        if (providers_.size() == 1) {
            provider = &providers_.begin()->second;
        } else {
            if (ctx) ctx->message = "Unknown token issuer: " + parsed.claims.iss;
            return core::Status::PERMISSION_DENIED;
        }
    }

    // Verify signature
    status = verifyJwtSignature(parsed, *provider);
    if (status != core::Status::OK) {
        if (ctx) ctx->message = "JWT signature verification failed";
        return status;
    }

    // Validate claims
    status = validateJwtClaims(parsed.claims, *provider);
    if (status != core::Status::OK) {
        if (ctx) ctx->message = "JWT claims validation failed";
        return status;
    }

    claims = parsed.claims;
    return core::Status::OK;
}

core::Status OAuthAuthMethod::introspectToken(const std::string& token,
                                              const std::string& provider_id,
                                              JwtClaims& claims,
                                              core::ErrorContext* ctx) {
    const OAuthProviderConfig* provider = getProvider(provider_id);
    if (!provider) {
        if (ctx) ctx->message = "Unknown provider: " + provider_id;
        return core::Status::NOT_FOUND;
    }

    if (provider->introspection_endpoint.empty()) {
        if (ctx) ctx->message = "Provider does not support introspection";
        return core::Status::NOT_SUPPORTED;
    }

    // Stub: In real implementation, would make HTTP POST to introspection endpoint
    // POST /oauth/introspect
    // token=...&token_type_hint=access_token
    // Authorization: Basic base64(client_id:client_secret)

    // For now, fall back to local validation
    return validateToken(token, claims, ctx);
}

std::string OAuthAuthMethod::mapClaimsToUser(const JwtClaims& claims) {
    // Try preferred_username first (common OIDC claim)
    if (!claims.preferred_username.empty()) {
        return claims.preferred_username;
    }

    // Then email
    if (!claims.email.empty()) {
        return claims.email;
    }

    // Finally subject
    return claims.sub;
}

std::vector<std::string> OAuthAuthMethod::mapClaimsToRoles(const JwtClaims& claims) {
    std::vector<std::string> roles;

    // Add roles from token
    for (const auto& role : claims.roles) {
        roles.push_back(role);
    }

    // Add groups as roles (with prefix)
    for (const auto& group : claims.groups) {
        roles.push_back("group:" + group);
    }

    return roles;
}

core::Status OAuthAuthMethod::verifyJwtSignature(const JwtToken& token,
                                                 const OAuthProviderConfig& provider) {
    // Find key by kid
    const JsonWebKey* key = findKey(provider.provider_id, token.header.kid);

    // If no kid match and we have keys, try all of them
    if (!key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = jwks_cache_.find(provider.provider_id);
        if (it != jwks_cache_.end() && !it->second.empty()) {
            key = &it->second[0];  // Try first key
        }
    }

    if (!key) {
        // Try to fetch JWKS
        if (!provider.jwks_uri.empty()) {
            std::vector<JsonWebKey> keys;
            auto status = fetchJwks(provider.jwks_uri, keys);
            if (status == core::Status::OK && !keys.empty()) {
                std::lock_guard<std::mutex> lock(mutex_);
                jwks_cache_[provider.provider_id] = keys;
                jwks_cache_time_[provider.provider_id] = std::chrono::steady_clock::now();

                // Find key again
                for (const auto& k : keys) {
                    if (k.kid == token.header.kid || token.header.kid.empty()) {
                        key = &k;
                        break;
                    }
                }
            }
        }
    }

    if (!key) {
        return core::Status::PERMISSION_DENIED;
    }

    // Build message to verify (header.payload)
    std::string message_str = token.raw.substr(0, token.raw.rfind('.'));
    std::vector<uint8_t> message(message_str.begin(), message_str.end());

    // Verify based on algorithm
    bool valid = false;
    switch (token.header.alg) {
        case JwtAlgorithm::RS256:
        case JwtAlgorithm::RS384:
        case JwtAlgorithm::RS512:
        case JwtAlgorithm::PS256:
        case JwtAlgorithm::PS384:
        case JwtAlgorithm::PS512:
            valid = verifyRsaSignature(message, token.signature, *key, token.header.alg);
            break;

        case JwtAlgorithm::ES256:
        case JwtAlgorithm::ES384:
        case JwtAlgorithm::ES512:
            valid = verifyEcSignature(message, token.signature, *key, token.header.alg);
            break;

        case JwtAlgorithm::HS256:
        case JwtAlgorithm::HS384:
        case JwtAlgorithm::HS512:
            valid = verifyHmacSignature(message, token.signature,
                                        provider.client_secret, token.header.alg);
            break;

        default:
            return core::Status::NOT_SUPPORTED;
    }

    return valid ? core::Status::OK : core::Status::PERMISSION_DENIED;
}

core::Status OAuthAuthMethod::validateJwtClaims(const JwtClaims& claims,
                                                const OAuthProviderConfig& provider) {
    auto now = std::chrono::system_clock::now();

    // Check expiration
    if (provider.require_exp) {
        if (claims.exp <= now - provider.clock_skew_tolerance) {
            return core::Status::PERMISSION_DENIED;  // Token expired
        }
    }

    // Check not-before
    if (claims.nbf > now + provider.clock_skew_tolerance) {
        return core::Status::PERMISSION_DENIED;  // Token not yet valid
    }

    // Check issuer
    if (provider.validate_issuer && !provider.issuer.empty()) {
        if (claims.iss != provider.issuer) {
            return core::Status::PERMISSION_DENIED;
        }
    }

    // Check audience
    if (!provider.expected_audiences.empty()) {
        bool audience_match = false;
        for (const auto& expected : provider.expected_audiences) {
            for (const auto& actual : claims.aud) {
                if (actual == expected) {
                    audience_match = true;
                    break;
                }
            }
            if (audience_match) break;
        }
        if (!audience_match) {
            return core::Status::PERMISSION_DENIED;
        }
    }

    return core::Status::OK;
}

core::Status OAuthAuthMethod::fetchJwks(const std::string& jwks_uri,
                                        std::vector<JsonWebKey>& keys) {
    // Stub: In real implementation, would make HTTP GET request
    // For now, return empty
    keys.clear();
    return core::Status::NOT_SUPPORTED;
}

const JsonWebKey* OAuthAuthMethod::findKey(const std::string& provider_id,
                                           const std::string& kid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = jwks_cache_.find(provider_id);
    if (it == jwks_cache_.end()) {
        return nullptr;
    }

    for (const auto& key : it->second) {
        if (key.kid == kid) {
            return &key;
        }
    }

    return nullptr;
}

// ============================================================================
// OidcAuthMethod Implementation
// ============================================================================

OidcAuthMethod::OidcAuthMethod() = default;
OidcAuthMethod::~OidcAuthMethod() = default;

core::Status OidcAuthMethod::discoverProvider(const std::string& issuer,
                                              OAuthProviderConfig& config,
                                              core::ErrorContext* ctx) {
    // Stub: Would fetch .well-known/openid-configuration
    // GET {issuer}/.well-known/openid-configuration

    config.issuer = issuer;
    config.is_oidc = true;

    // In real implementation, would parse JSON response and populate:
    // - authorization_endpoint
    // - token_endpoint
    // - userinfo_endpoint
    // - jwks_uri
    // - etc.

    return core::Status::NOT_SUPPORTED;
}

core::Status OidcAuthMethod::validateIdToken(const std::string& id_token,
                                             const std::string& nonce,
                                             JwtClaims& claims,
                                             core::ErrorContext* ctx) {
    // Parse and validate like regular access token
    auto status = validateToken(id_token, claims, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Verify nonce if provided
    if (!nonce.empty() && claims.nonce != nonce) {
        if (ctx) ctx->message = "Nonce mismatch";
        return core::Status::PERMISSION_DENIED;
    }

    return core::Status::OK;
}

core::Status OidcAuthMethod::fetchUserInfo(const std::string& access_token,
                                           const std::string& provider_id,
                                           JwtClaims& claims,
                                           core::ErrorContext* ctx) {
    const OAuthProviderConfig* provider = getProvider(provider_id);
    if (!provider) {
        if (ctx) ctx->message = "Unknown provider: " + provider_id;
        return core::Status::NOT_FOUND;
    }

    if (provider->userinfo_endpoint.empty()) {
        if (ctx) ctx->message = "Provider does not have userinfo endpoint";
        return core::Status::NOT_SUPPORTED;
    }

    // Stub: Would make HTTP GET to userinfo endpoint
    // GET /userinfo
    // Authorization: Bearer {access_token}

    return core::Status::NOT_SUPPORTED;
}

// ============================================================================
// JWT Utility Functions
// ============================================================================

core::Status parseJwt(const std::string& token,
                      JwtToken& parsed,
                      core::ErrorContext* ctx) {
    parsed = JwtToken{};
    parsed.raw = token;

    // Split into header.payload.signature
    std::vector<std::string> parts;
    std::istringstream iss(token);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    if (parts.size() != 3) {
        if (ctx) ctx->message = "Invalid JWT format";
        return core::Status::INVALID_ARGUMENT;
    }

    // Decode header
    std::string header_json(
        reinterpret_cast<const char*>(base64UrlDecode(parts[0]).data()),
        base64UrlDecode(parts[0]).size());

    // Parse header (simplified - would use JSON library)
    if (header_json.find("\"RS256\"") != std::string::npos) {
        parsed.header.alg = JwtAlgorithm::RS256;
    } else if (header_json.find("\"RS384\"") != std::string::npos) {
        parsed.header.alg = JwtAlgorithm::RS384;
    } else if (header_json.find("\"RS512\"") != std::string::npos) {
        parsed.header.alg = JwtAlgorithm::RS512;
    } else if (header_json.find("\"ES256\"") != std::string::npos) {
        parsed.header.alg = JwtAlgorithm::ES256;
    } else if (header_json.find("\"HS256\"") != std::string::npos) {
        parsed.header.alg = JwtAlgorithm::HS256;
    }

    // Extract kid
    size_t kid_pos = header_json.find("\"kid\"");
    if (kid_pos != std::string::npos) {
        size_t start = header_json.find(':', kid_pos);
        if (start != std::string::npos) {
            start = header_json.find('"', start + 1);
            if (start != std::string::npos) {
                size_t end = header_json.find('"', start + 1);
                if (end != std::string::npos) {
                    parsed.header.kid = header_json.substr(start + 1, end - start - 1);
                }
            }
        }
    }

    // Decode payload
    std::vector<uint8_t> payload_bytes = base64UrlDecode(parts[1]);
    std::string payload_json(
        reinterpret_cast<const char*>(payload_bytes.data()),
        payload_bytes.size());

    auto status = parseJwtClaims(payload_json, parsed.claims, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Decode signature
    parsed.signature = base64UrlDecode(parts[2]);

    return core::Status::OK;
}

std::vector<uint8_t> base64UrlDecode(const std::string& encoded) {
    // Convert base64url to base64
    std::string base64 = encoded;
    std::replace(base64.begin(), base64.end(), '-', '+');
    std::replace(base64.begin(), base64.end(), '_', '/');

    // Add padding
    while (base64.size() % 4 != 0) {
        base64 += '=';
    }

    // Decode
    std::vector<uint8_t> decoded;
    decoded.reserve(base64.size() * 3 / 4);

    static const char decode_table[] = {
        62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1,
        -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
        -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
        36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
    };

    for (size_t i = 0; i < base64.size(); i += 4) {
        uint32_t n = 0;
        int padding = 0;

        for (int j = 0; j < 4; ++j) {
            char c = base64[i + j];
            if (c == '=') {
                padding++;
                n <<= 6;
            } else if (c >= '+' && c <= 'z') {
                int idx = c - '+';
                if (idx >= 0 && idx < 80 && decode_table[idx] >= 0) {
                    n = (n << 6) | decode_table[idx];
                }
            }
        }

        if (padding < 3) decoded.push_back((n >> 16) & 0xff);
        if (padding < 2) decoded.push_back((n >> 8) & 0xff);
        if (padding < 1) decoded.push_back(n & 0xff);
    }

    return decoded;
}

std::string base64UrlEncode(const std::vector<uint8_t>& data) {
    return base64UrlEncode(data.data(), data.size());
}

std::string base64UrlEncode(const uint8_t* data, size_t len) {
    static const char encode_table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

        encoded += encode_table[(n >> 18) & 0x3f];
        encoded += encode_table[(n >> 12) & 0x3f];
        if (i + 1 < len) {
            encoded += encode_table[(n >> 6) & 0x3f];
        }
        if (i + 2 < len) {
            encoded += encode_table[n & 0x3f];
        }
    }

    // Convert to base64url (no padding)
    std::replace(encoded.begin(), encoded.end(), '+', '-');
    std::replace(encoded.begin(), encoded.end(), '/', '_');

    return encoded;
}

core::Status parseJwtClaims(const std::string& payload_json,
                            JwtClaims& claims,
                            core::ErrorContext* ctx) {
    claims = JwtClaims{};

    // Helper lambda to extract string value
    auto extractString = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = payload_json.find(search);
        if (pos == std::string::npos) return "";

        size_t colon = payload_json.find(':', pos);
        if (colon == std::string::npos) return "";

        // Skip whitespace
        size_t start = payload_json.find_first_not_of(" \t\n\r", colon + 1);
        if (start == std::string::npos) return "";

        if (payload_json[start] == '"') {
            size_t end = payload_json.find('"', start + 1);
            if (end != std::string::npos) {
                return payload_json.substr(start + 1, end - start - 1);
            }
        }
        return "";
    };

    // Helper lambda to extract number value
    auto extractNumber = [&](const std::string& key) -> int64_t {
        std::string search = "\"" + key + "\"";
        size_t pos = payload_json.find(search);
        if (pos == std::string::npos) return 0;

        size_t colon = payload_json.find(':', pos);
        if (colon == std::string::npos) return 0;

        size_t start = payload_json.find_first_not_of(" \t\n\r", colon + 1);
        if (start == std::string::npos) return 0;

        return std::strtoll(payload_json.c_str() + start, nullptr, 10);
    };

    // Extract standard claims
    claims.iss = extractString("iss");
    claims.sub = extractString("sub");
    claims.jti = extractString("jti");
    claims.nonce = extractString("nonce");
    claims.azp = extractString("azp");

    // Audience can be string or array
    std::string aud_str = extractString("aud");
    if (!aud_str.empty()) {
        claims.aud.push_back(aud_str);
    }

    // Time claims
    int64_t exp = extractNumber("exp");
    if (exp > 0) {
        claims.exp = std::chrono::system_clock::from_time_t(exp);
    }

    int64_t nbf = extractNumber("nbf");
    if (nbf > 0) {
        claims.nbf = std::chrono::system_clock::from_time_t(nbf);
    }

    int64_t iat = extractNumber("iat");
    if (iat > 0) {
        claims.iat = std::chrono::system_clock::from_time_t(iat);
    }

    // User info claims
    claims.name = extractString("name");
    claims.email = extractString("email");
    claims.preferred_username = extractString("preferred_username");

    return core::Status::OK;
}

core::Status parseJwks(const std::string& jwks_json,
                       std::vector<JsonWebKey>& keys,
                       core::ErrorContext* ctx) {
    // Stub: Would parse JWKS JSON properly
    keys.clear();
    return core::Status::NOT_SUPPORTED;
}

bool verifyRsaSignature(const std::vector<uint8_t>& message,
                        const std::vector<uint8_t>& signature,
                        const JsonWebKey& key,
                        JwtAlgorithm algorithm) {
    // Need n and e from JWK
    if (key.n.empty() || key.e.empty()) {
        return false;
    }

    // Decode n and e from base64url
    std::vector<uint8_t> n_bytes = base64UrlDecode(key.n);
    std::vector<uint8_t> e_bytes = base64UrlDecode(key.e);

    // Create RSA key
    RSA* rsa = RSA_new();
    if (!rsa) return false;

    BIGNUM* bn_n = BN_bin2bn(n_bytes.data(), n_bytes.size(), nullptr);
    BIGNUM* bn_e = BN_bin2bn(e_bytes.data(), e_bytes.size(), nullptr);

    if (!bn_n || !bn_e) {
        BN_free(bn_n);
        BN_free(bn_e);
        RSA_free(rsa);
        return false;
    }

    RSA_set0_key(rsa, bn_n, bn_e, nullptr);

    // Select hash algorithm
    const EVP_MD* md = nullptr;
    switch (algorithm) {
        case JwtAlgorithm::RS256:
        case JwtAlgorithm::PS256:
            md = EVP_sha256();
            break;
        case JwtAlgorithm::RS384:
        case JwtAlgorithm::PS384:
            md = EVP_sha384();
            break;
        case JwtAlgorithm::RS512:
        case JwtAlgorithm::PS512:
            md = EVP_sha512();
            break;
        default:
            RSA_free(rsa);
            return false;
    }

    // Hash the message
    std::vector<uint8_t> hash(EVP_MD_size(md));
    unsigned int hash_len = 0;
    EVP_Digest(message.data(), message.size(), hash.data(), &hash_len, md, nullptr);

    // Verify signature
    int result = RSA_verify(EVP_MD_type(md), hash.data(), hash_len,
                            signature.data(), signature.size(), rsa);

    RSA_free(rsa);
    return result == 1;
}

bool verifyEcSignature(const std::vector<uint8_t>& message,
                       const std::vector<uint8_t>& signature,
                       const JsonWebKey& key,
                       JwtAlgorithm algorithm) {
    // Need x and y from JWK
    if (key.x.empty() || key.y.empty()) {
        return false;
    }

    // Determine curve
    int nid;
    const EVP_MD* md;
    switch (algorithm) {
        case JwtAlgorithm::ES256:
            nid = NID_X9_62_prime256v1;
            md = EVP_sha256();
            break;
        case JwtAlgorithm::ES384:
            nid = NID_secp384r1;
            md = EVP_sha384();
            break;
        case JwtAlgorithm::ES512:
            nid = NID_secp521r1;
            md = EVP_sha512();
            break;
        default:
            return false;
    }

    // Decode x and y
    std::vector<uint8_t> x_bytes = base64UrlDecode(key.x);
    std::vector<uint8_t> y_bytes = base64UrlDecode(key.y);

    // Create EC key
    EC_KEY* ec = EC_KEY_new_by_curve_name(nid);
    if (!ec) return false;

    BIGNUM* bn_x = BN_bin2bn(x_bytes.data(), x_bytes.size(), nullptr);
    BIGNUM* bn_y = BN_bin2bn(y_bytes.data(), y_bytes.size(), nullptr);

    if (!bn_x || !bn_y) {
        BN_free(bn_x);
        BN_free(bn_y);
        EC_KEY_free(ec);
        return false;
    }

    EC_KEY_set_public_key_affine_coordinates(ec, bn_x, bn_y);
    BN_free(bn_x);
    BN_free(bn_y);

    // Hash the message
    std::vector<uint8_t> hash(EVP_MD_size(md));
    unsigned int hash_len = 0;
    EVP_Digest(message.data(), message.size(), hash.data(), &hash_len, md, nullptr);

    // ECDSA signature is r || s, need to convert to DER
    size_t coord_len = signature.size() / 2;
    BIGNUM* r = BN_bin2bn(signature.data(), coord_len, nullptr);
    BIGNUM* s = BN_bin2bn(signature.data() + coord_len, coord_len, nullptr);

    ECDSA_SIG* ecdsa_sig = ECDSA_SIG_new();
    ECDSA_SIG_set0(ecdsa_sig, r, s);

    int result = ECDSA_do_verify(hash.data(), hash_len, ecdsa_sig, ec);

    ECDSA_SIG_free(ecdsa_sig);
    EC_KEY_free(ec);

    return result == 1;
}

bool verifyHmacSignature(const std::vector<uint8_t>& message,
                         const std::vector<uint8_t>& signature,
                         const std::string& secret,
                         JwtAlgorithm algorithm) {
    const EVP_MD* md = nullptr;
    switch (algorithm) {
        case JwtAlgorithm::HS256:
            md = EVP_sha256();
            break;
        case JwtAlgorithm::HS384:
            md = EVP_sha384();
            break;
        case JwtAlgorithm::HS512:
            md = EVP_sha512();
            break;
        default:
            return false;
    }

    unsigned char computed[EVP_MAX_MD_SIZE];
    unsigned int computed_len = 0;

    HMAC(md, secret.data(), secret.size(),
         message.data(), message.size(),
         computed, &computed_len);

    if (computed_len != signature.size()) {
        return false;
    }

    // Constant-time comparison
    int result = CRYPTO_memcmp(computed, signature.data(), computed_len);
    return result == 0;
}

const char* jwtAlgorithmToString(JwtAlgorithm alg) {
    switch (alg) {
        case JwtAlgorithm::HS256: return "HS256";
        case JwtAlgorithm::HS384: return "HS384";
        case JwtAlgorithm::HS512: return "HS512";
        case JwtAlgorithm::RS256: return "RS256";
        case JwtAlgorithm::RS384: return "RS384";
        case JwtAlgorithm::RS512: return "RS512";
        case JwtAlgorithm::ES256: return "ES256";
        case JwtAlgorithm::ES384: return "ES384";
        case JwtAlgorithm::ES512: return "ES512";
        case JwtAlgorithm::PS256: return "PS256";
        case JwtAlgorithm::PS384: return "PS384";
        case JwtAlgorithm::PS512: return "PS512";
        case JwtAlgorithm::EDDSA: return "EdDSA";
        default: return "unknown";
    }
}

bool parseJwtAlgorithm(const std::string& str, JwtAlgorithm& alg) {
    if (str == "HS256") { alg = JwtAlgorithm::HS256; return true; }
    if (str == "HS384") { alg = JwtAlgorithm::HS384; return true; }
    if (str == "HS512") { alg = JwtAlgorithm::HS512; return true; }
    if (str == "RS256") { alg = JwtAlgorithm::RS256; return true; }
    if (str == "RS384") { alg = JwtAlgorithm::RS384; return true; }
    if (str == "RS512") { alg = JwtAlgorithm::RS512; return true; }
    if (str == "ES256") { alg = JwtAlgorithm::ES256; return true; }
    if (str == "ES384") { alg = JwtAlgorithm::ES384; return true; }
    if (str == "ES512") { alg = JwtAlgorithm::ES512; return true; }
    if (str == "PS256") { alg = JwtAlgorithm::PS256; return true; }
    if (str == "PS384") { alg = JwtAlgorithm::PS384; return true; }
    if (str == "PS512") { alg = JwtAlgorithm::PS512; return true; }
    if (str == "EdDSA") { alg = JwtAlgorithm::EDDSA; return true; }
    return false;
}

bool isTokenExpired(const JwtClaims& claims, std::chrono::seconds clock_skew) {
    auto now = std::chrono::system_clock::now();
    return claims.exp <= now - clock_skew;
}

std::string generateOAuthState(size_t length) {
    std::vector<uint8_t> random(length);
    RAND_bytes(random.data(), random.size());
    return base64UrlEncode(random);
}

PkceChallenge generatePkceChallenge() {
    PkceChallenge challenge;

    // Generate code verifier (43-128 characters)
    std::vector<uint8_t> random(32);
    RAND_bytes(random.data(), random.size());
    challenge.code_verifier = base64UrlEncode(random);

    // Generate code challenge (SHA256 of verifier)
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(challenge.code_verifier.data()),
           challenge.code_verifier.size(), hash);

    challenge.code_challenge = base64UrlEncode(hash, SHA256_DIGEST_LENGTH);
    challenge.code_challenge_method = "S256";

    return challenge;
}

}  // namespace security
}  // namespace scratchbird
