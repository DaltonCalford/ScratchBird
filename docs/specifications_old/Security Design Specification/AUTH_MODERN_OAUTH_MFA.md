# ScratchBird Modern Authentication (OAuth 2.0/OIDC/MFA)
## Complete Technical Specification

## Overview

This document provides comprehensive specifications for modern authentication methods including OAuth 2.0, OpenID Connect (OIDC), SAML 2.0, and Multi-Factor Authentication (MFA) with TOTP, WebAuthn/FIDO2, and biometric support.

## 1. OAuth 2.0 / OpenID Connect

### 1.1 OAuth 2.0 Configuration

```c
// OAuth 2.0 configuration
typedef struct oauth2_config {
    // Provider configuration
    char*               oc_issuer;             // Authorization server URL
    char*               oc_auth_endpoint;      // Authorization endpoint
    char*               oc_token_endpoint;     // Token endpoint
    char*               oc_userinfo_endpoint;  // UserInfo endpoint
    char*               oc_jwks_uri;           // JWKS URI
    
    // Client configuration
    char*               oc_client_id;          // Client ID
    char*               oc_client_secret;      // Client secret
    char*               oc_redirect_uri;       // Redirect URI
    
    // Grant types
    OAuth2GrantType     oc_grant_types;        // Supported grant types
    
    // Scopes
    char**              oc_scopes;             // Required scopes
    uint32_t            oc_scope_count;        // Number of scopes
    
    // PKCE
    bool                oc_require_pkce;       // Require PKCE
    PKCEMethod          oc_pkce_method;        // PKCE challenge method
    
    // Token configuration
    uint32_t            oc_access_token_ttl;   // Access token TTL
    uint32_t            oc_refresh_token_ttl;  // Refresh token TTL
    bool                oc_use_refresh_tokens; // Enable refresh tokens
    
    // JWT configuration
    JWTAlgorithm        oc_jwt_algorithm;      // JWT signing algorithm
    char*               oc_jwt_secret;         // JWT secret (for HMAC)
    EVP_PKEY*           oc_jwt_private_key;    // JWT private key (for RSA/EC)
    EVP_PKEY*           oc_jwt_public_key;     // JWT public key
    
    // Validation
    bool                oc_validate_issuer;    // Validate issuer claim
    bool                oc_validate_audience;  // Validate audience claim
    uint32_t            oc_clock_skew;         // Allowed clock skew (seconds)
} OAuth2Config;

// OAuth 2.0 grant types
typedef enum oauth2_grant_type {
    OAUTH2_GRANT_AUTHORIZATION_CODE = 1,
    OAUTH2_GRANT_IMPLICIT = 2,
    OAUTH2_GRANT_PASSWORD = 4,
    OAUTH2_GRANT_CLIENT_CREDENTIALS = 8,
    OAUTH2_GRANT_REFRESH_TOKEN = 16,
    OAUTH2_GRANT_DEVICE_CODE = 32
} OAuth2GrantType;

// OAuth 2.0 token
typedef struct oauth2_token {
    // Token values
    char*               ot_access_token;       // Access token
    char*               ot_refresh_token;      // Refresh token
    char*               ot_id_token;           // ID token (OIDC)
    
    // Token metadata
    TokenType           ot_token_type;         // Token type (Bearer)
    uint32_t            ot_expires_in;         // Expiration (seconds)
    TimestampTz         ot_issued_at;          // Issue time
    
    // Scopes
    char**              ot_scopes;             // Granted scopes
    uint32_t            ot_scope_count;        // Number of scopes
    
    // Claims (from ID token or introspection)
    JWTClaims*          ot_claims;             // Token claims
} OAuth2Token;

// JWT claims
typedef struct jwt_claims {
    // Standard claims
    char*               jc_issuer;             // iss
    char*               jc_subject;            // sub
    char**              jc_audience;           // aud
    TimestampTz         jc_expiration;         // exp
    TimestampTz         jc_not_before;         // nbf
    TimestampTz         jc_issued_at;          // iat
    char*               jc_jwt_id;             // jti
    
    // OpenID Connect claims
    char*               jc_name;               // name
    char*               jc_email;              // email
    bool                jc_email_verified;     // email_verified
    char*               jc_preferred_username; // preferred_username
    char**              jc_groups;             // groups
    uint32_t            jc_group_count;        // Number of groups
    
    // Custom claims
    HashTable*          jc_custom_claims;      // Custom claims
} JWTClaims;
```

### 1.2 OAuth 2.0 Authorization Code Flow

```c
// OAuth 2.0 authorization code flow
typedef struct oauth2_auth_flow {
    // State
    OAuth2FlowState     oaf_state;             // Current state
    
    // PKCE
    char                oaf_code_verifier[128]; // PKCE verifier
    char                oaf_code_challenge[128]; // PKCE challenge
    
    // State parameter
    char                oaf_state_param[64];   // State parameter
    
    // Authorization code
    char*               oaf_auth_code;         // Authorization code
    
    // Tokens
    OAuth2Token*        oaf_tokens;            // Obtained tokens
    
    // User info
    UserInfo*           oaf_user_info;         // User information
} OAuth2AuthFlow;

// Start OAuth 2.0 authorization
char* oauth2_start_authorization(
    OAuth2Config* config,
    OAuth2AuthFlow* flow)
{
    // Generate state parameter
    generate_random_string(flow->oaf_state_param, 
                          sizeof(flow->oaf_state_param));
    
    // Generate PKCE verifier and challenge if required
    if (config->oc_require_pkce) {
        // Generate code verifier (43-128 characters)
        generate_code_verifier(flow->oaf_code_verifier,
                             sizeof(flow->oaf_code_verifier));
        
        // Calculate code challenge
        if (config->oc_pkce_method == PKCE_S256) {
            // SHA256(verifier)
            uint8_t hash[SHA256_DIGEST_LENGTH];
            SHA256((uint8_t*)flow->oaf_code_verifier,
                  strlen(flow->oaf_code_verifier), hash);
            
            // Base64url encode
            base64url_encode(hash, sizeof(hash),
                           flow->oaf_code_challenge,
                           sizeof(flow->oaf_code_challenge));
        } else {
            // Plain method
            strncpy(flow->oaf_code_challenge,
                   flow->oaf_code_verifier,
                   sizeof(flow->oaf_code_challenge));
        }
    }
    
    // Build authorization URL
    char* auth_url = malloc(2048);
    char* scope_string = join_strings(config->oc_scopes,
                                     config->oc_scope_count, " ");
    
    snprintf(auth_url, 2048,
            "%s?response_type=code"
            "&client_id=%s"
            "&redirect_uri=%s"
            "&scope=%s"
            "&state=%s",
            config->oc_auth_endpoint,
            url_encode(config->oc_client_id),
            url_encode(config->oc_redirect_uri),
            url_encode(scope_string),
            flow->oaf_state_param);
    
    // Add PKCE parameters
    if (config->oc_require_pkce) {
        char pkce_params[256];
        snprintf(pkce_params, sizeof(pkce_params),
                "&code_challenge=%s"
                "&code_challenge_method=%s",
                flow->oaf_code_challenge,
                config->oc_pkce_method == PKCE_S256 ? "S256" : "plain");
        strcat(auth_url, pkce_params);
    }
    
    free(scope_string);
    flow->oaf_state = OAUTH2_STATE_AUTHORIZATION;
    
    return auth_url;
}

// Handle OAuth 2.0 callback
Status oauth2_handle_callback(
    OAuth2Config* config,
    OAuth2AuthFlow* flow,
    const char* code,
    const char* state)
{
    // Verify state parameter
    if (strcmp(state, flow->oaf_state_param) != 0) {
        log_error("OAuth2: State parameter mismatch");
        return STATUS_INVALID_STATE;
    }
    
    // Store authorization code
    flow->oaf_auth_code = strdup(code);
    
    // Exchange code for tokens
    return oauth2_exchange_code(config, flow);
}

// Exchange authorization code for tokens
Status oauth2_exchange_code(
    OAuth2Config* config,
    OAuth2AuthFlow* flow)
{
    // Build token request
    CURL* curl = curl_easy_init();
    if (!curl) {
        return STATUS_CURL_INIT_FAILED;
    }
    
    // Build POST data
    char post_data[2048];
    snprintf(post_data, sizeof(post_data),
            "grant_type=authorization_code"
            "&code=%s"
            "&redirect_uri=%s"
            "&client_id=%s"
            "&client_secret=%s",
            url_encode(flow->oaf_auth_code),
            url_encode(config->oc_redirect_uri),
            url_encode(config->oc_client_id),
            url_encode(config->oc_client_secret));
    
    // Add PKCE verifier
    if (config->oc_require_pkce) {
        char verifier_param[256];
        snprintf(verifier_param, sizeof(verifier_param),
                "&code_verifier=%s",
                flow->oaf_code_verifier);
        strcat(post_data, verifier_param);
    }
    
    // Setup request
    MemoryBuffer response = {0};
    
    curl_easy_setopt(curl, CURLOPT_URL, config->oc_token_endpoint);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        log_error("Token exchange failed: %s", curl_easy_strerror(res));
        free(response.data);
        return STATUS_TOKEN_EXCHANGE_FAILED;
    }
    
    // Parse token response
    flow->oaf_tokens = parse_token_response(response.data);
    free(response.data);
    
    if (flow->oaf_tokens == NULL) {
        return STATUS_INVALID_TOKEN_RESPONSE;
    }
    
    // Validate tokens
    Status status = validate_oauth2_tokens(config, flow->oaf_tokens);
    if (status != STATUS_OK) {
        return status;
    }
    
    flow->oaf_state = OAUTH2_STATE_AUTHENTICATED;
    return STATUS_OK;
}

// Validate OAuth 2.0 tokens
Status validate_oauth2_tokens(
    OAuth2Config* config,
    OAuth2Token* tokens)
{
    // Validate ID token if present (OIDC)
    if (tokens->ot_id_token) {
        JWTClaims* claims = NULL;
        Status status = validate_jwt(
            tokens->ot_id_token,
            config,
            &claims
        );
        
        if (status != STATUS_OK) {
            return status;
        }
        
        tokens->ot_claims = claims;
        
        // Additional OIDC validations
        if (config->oc_validate_issuer) {
            if (strcmp(claims->jc_issuer, config->oc_issuer) != 0) {
                log_error("Invalid issuer: %s", claims->jc_issuer);
                return STATUS_INVALID_ISSUER;
            }
        }
        
        if (config->oc_validate_audience) {
            bool valid_audience = false;
            for (uint32_t i = 0; i < claims->jc_audience_count; i++) {
                if (strcmp(claims->jc_audience[i], 
                          config->oc_client_id) == 0) {
                    valid_audience = true;
                    break;
                }
            }
            
            if (!valid_audience) {
                log_error("Invalid audience");
                return STATUS_INVALID_AUDIENCE;
            }
        }
    }
    
    // Validate access token expiration
    TimestampTz expires_at = tokens->ot_issued_at + tokens->ot_expires_in;
    if (expires_at < GetCurrentTimestamp()) {
        return STATUS_TOKEN_EXPIRED;
    }
    
    return STATUS_OK;
}
```

### 1.3 JWT Validation

```c
// Validate JWT token
Status validate_jwt(
    const char* token,
    OAuth2Config* config,
    JWTClaims** claims_out)
{
    // Split token into parts
    char* token_copy = strdup(token);
    char* header_b64 = strtok(token_copy, ".");
    char* payload_b64 = strtok(NULL, ".");
    char* signature_b64 = strtok(NULL, ".");
    
    if (!header_b64 || !payload_b64 || !signature_b64) {
        free(token_copy);
        return STATUS_INVALID_JWT_FORMAT;
    }
    
    // Decode header
    size_t header_len;
    char* header_json = base64url_decode(header_b64, &header_len);
    
    // Parse header
    json_t* header = json_loads(header_json, 0, NULL);
    free(header_json);
    
    if (!header) {
        free(token_copy);
        return STATUS_INVALID_JWT_HEADER;
    }
    
    // Get algorithm
    const char* alg = json_string_value(json_object_get(header, "alg"));
    json_decref(header);
    
    // Verify signature
    Status status = verify_jwt_signature(
        header_b64, payload_b64, signature_b64,
        alg, config
    );
    
    if (status != STATUS_OK) {
        free(token_copy);
        return status;
    }
    
    // Decode and parse payload
    size_t payload_len;
    char* payload_json = base64url_decode(payload_b64, &payload_len);
    
    json_t* payload = json_loads(payload_json, 0, NULL);
    free(payload_json);
    free(token_copy);
    
    if (!payload) {
        return STATUS_INVALID_JWT_PAYLOAD;
    }
    
    // Extract claims
    JWTClaims* claims = calloc(1, sizeof(JWTClaims));
    
    // Standard claims
    claims->jc_issuer = json_string_value_dup(
        json_object_get(payload, "iss"));
    claims->jc_subject = json_string_value_dup(
        json_object_get(payload, "sub"));
    
    json_t* exp = json_object_get(payload, "exp");
    if (exp) {
        claims->jc_expiration = json_integer_value(exp);
    }
    
    json_t* iat = json_object_get(payload, "iat");
    if (iat) {
        claims->jc_issued_at = json_integer_value(iat);
    }
    
    // OIDC claims
    claims->jc_email = json_string_value_dup(
        json_object_get(payload, "email"));
    claims->jc_name = json_string_value_dup(
        json_object_get(payload, "name"));
    
    json_decref(payload);
    
    // Validate expiration
    if (claims->jc_expiration > 0) {
        TimestampTz now = GetCurrentTimestamp();
        if (claims->jc_expiration < now - config->oc_clock_skew) {
            free_jwt_claims(claims);
            return STATUS_JWT_EXPIRED;
        }
    }
    
    // Validate not before
    if (claims->jc_not_before > 0) {
        TimestampTz now = GetCurrentTimestamp();
        if (claims->jc_not_before > now + config->oc_clock_skew) {
            free_jwt_claims(claims);
            return STATUS_JWT_NOT_YET_VALID;
        }
    }
    
    *claims_out = claims;
    return STATUS_OK;
}

// Verify JWT signature
Status verify_jwt_signature(
    const char* header_b64,
    const char* payload_b64,
    const char* signature_b64,
    const char* algorithm,
    OAuth2Config* config)
{
    // Build signing input
    char signing_input[4096];
    snprintf(signing_input, sizeof(signing_input),
            "%s.%s", header_b64, payload_b64);
    
    // Decode signature
    size_t sig_len;
    uint8_t* signature = base64url_decode(signature_b64, &sig_len);
    
    Status status = STATUS_OK;
    
    if (strcmp(algorithm, "HS256") == 0) {
        // HMAC-SHA256
        uint8_t computed[32];
        unsigned int computed_len;
        
        HMAC(EVP_sha256(),
             config->oc_jwt_secret, strlen(config->oc_jwt_secret),
             (uint8_t*)signing_input, strlen(signing_input),
             computed, &computed_len);
        
        if (computed_len != sig_len ||
            !constant_time_compare(computed, signature, sig_len)) {
            status = STATUS_INVALID_JWT_SIGNATURE;
        }
    } else if (strcmp(algorithm, "RS256") == 0) {
        // RSA-SHA256
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        
        if (!EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL,
                                 config->oc_jwt_public_key)) {
            status = STATUS_CRYPTO_ERROR;
        } else if (!EVP_DigestVerifyUpdate(ctx, signing_input,
                                          strlen(signing_input))) {
            status = STATUS_CRYPTO_ERROR;
        } else if (EVP_DigestVerifyFinal(ctx, signature, sig_len) != 1) {
            status = STATUS_INVALID_JWT_SIGNATURE;
        }
        
        EVP_MD_CTX_free(ctx);
    } else {
        status = STATUS_UNSUPPORTED_JWT_ALGORITHM;
    }
    
    free(signature);
    return status;
}
```

## 2. Multi-Factor Authentication (MFA)

### 2.1 TOTP (Time-based One-Time Password)

```c
// TOTP configuration
typedef struct totp_config {
    // Algorithm
    HashAlgorithm       tc_algorithm;          // SHA1, SHA256, SHA512
    uint32_t            tc_digits;             // Number of digits (6 or 8)
    uint32_t            tc_period;             // Time step (usually 30s)
    
    // Window
    uint32_t            tc_window;             // Acceptance window
    
    // Issuer
    char*               tc_issuer;             // Issuer name
    
    // Backup codes
    uint32_t            tc_backup_code_count;  // Number of backup codes
    uint32_t            tc_backup_code_length; // Backup code length
} TOTPConfig;

// TOTP secret
typedef struct totp_secret {
    // User
    UserId              ts_user_id;            // User ID
    char                ts_username[256];      // Username
    
    // Secret
    uint8_t             ts_secret[32];         // Secret key
    uint32_t            ts_secret_len;         // Secret length
    
    // Configuration
    HashAlgorithm       ts_algorithm;          // Hash algorithm
    uint32_t            ts_digits;             // Digits
    uint32_t            ts_period;             // Period
    
    // Metadata
    TimestampTz         ts_created_at;         // Creation time
    TimestampTz         ts_last_used;          // Last used
    uint32_t            ts_use_count;          // Use count
    
    // Backup codes
    char**              ts_backup_codes;       // Backup codes
    uint32_t            ts_backup_count;       // Number of codes
    bool*               ts_backup_used;        // Used flags
} TOTPSecret;

// Generate TOTP secret
TOTPSecret* generate_totp_secret(
    const char* username,
    TOTPConfig* config)
{
    TOTPSecret* secret = calloc(1, sizeof(TOTPSecret));
    
    strncpy(secret->ts_username, username, sizeof(secret->ts_username));
    
    // Generate random secret
    secret->ts_secret_len = 20;  // 160 bits for SHA1, can be more
    generate_random_bytes(secret->ts_secret, secret->ts_secret_len);
    
    // Set configuration
    secret->ts_algorithm = config->tc_algorithm;
    secret->ts_digits = config->tc_digits;
    secret->ts_period = config->tc_period;
    secret->ts_created_at = GetCurrentTimestamp();
    
    // Generate backup codes
    if (config->tc_backup_code_count > 0) {
        secret->ts_backup_codes = calloc(config->tc_backup_code_count,
                                        sizeof(char*));
        secret->ts_backup_used = calloc(config->tc_backup_code_count,
                                       sizeof(bool));
        secret->ts_backup_count = config->tc_backup_code_count;
        
        for (uint32_t i = 0; i < config->tc_backup_code_count; i++) {
            secret->ts_backup_codes[i] = generate_backup_code(
                config->tc_backup_code_length);
        }
    }
    
    return secret;
}

// Generate TOTP provisioning URI
char* generate_totp_uri(
    TOTPSecret* secret,
    TOTPConfig* config)
{
    // Base32 encode secret
    char secret_b32[256];
    base32_encode(secret->ts_secret, secret->ts_secret_len,
                 secret_b32, sizeof(secret_b32));
    
    // Build URI
    char* uri = malloc(1024);
    snprintf(uri, 1024,
            "otpauth://totp/%s:%s?"
            "secret=%s"
            "&issuer=%s"
            "&algorithm=%s"
            "&digits=%u"
            "&period=%u",
            url_encode(config->tc_issuer),
            url_encode(secret->ts_username),
            secret_b32,
            url_encode(config->tc_issuer),
            secret->ts_algorithm == HASH_SHA1 ? "SHA1" :
            secret->ts_algorithm == HASH_SHA256 ? "SHA256" : "SHA512",
            secret->ts_digits,
            secret->ts_period);
    
    return uri;
}

// Calculate TOTP value
uint32_t calculate_totp(
    TOTPSecret* secret,
    time_t timestamp)
{
    // Calculate time counter
    uint64_t counter = timestamp / secret->ts_period;
    
    // Convert counter to big-endian
    uint8_t counter_bytes[8];
    for (int i = 7; i >= 0; i--) {
        counter_bytes[i] = counter & 0xFF;
        counter >>= 8;
    }
    
    // Calculate HMAC
    uint8_t hmac[EVP_MAX_MD_SIZE];
    unsigned int hmac_len;
    
    const EVP_MD* md;
    switch (secret->ts_algorithm) {
        case HASH_SHA1:
            md = EVP_sha1();
            break;
        case HASH_SHA256:
            md = EVP_sha256();
            break;
        case HASH_SHA512:
            md = EVP_sha512();
            break;
        default:
            return 0;
    }
    
    HMAC(md,
         secret->ts_secret, secret->ts_secret_len,
         counter_bytes, sizeof(counter_bytes),
         hmac, &hmac_len);
    
    // Dynamic truncation
    uint32_t offset = hmac[hmac_len - 1] & 0x0F;
    uint32_t code = ((hmac[offset] & 0x7F) << 24) |
                    ((hmac[offset + 1] & 0xFF) << 16) |
                    ((hmac[offset + 2] & 0xFF) << 8) |
                    (hmac[offset + 3] & 0xFF);
    
    // Modulo to get desired number of digits
    uint32_t divisor = 1;
    for (uint32_t i = 0; i < secret->ts_digits; i++) {
        divisor *= 10;
    }
    
    return code % divisor;
}

// Verify TOTP code
bool verify_totp(
    TOTPSecret* secret,
    uint32_t code,
    uint32_t window)
{
    time_t now = time(NULL);
    
    // Check within window
    for (int i = -(int)window; i <= (int)window; i++) {
        time_t timestamp = now + (i * secret->ts_period);
        uint32_t expected = calculate_totp(secret, timestamp);
        
        if (code == expected) {
            // Update last used
            secret->ts_last_used = GetCurrentTimestamp();
            secret->ts_use_count++;
            return true;
        }
    }
    
    // Check backup codes
    for (uint32_t i = 0; i < secret->ts_backup_count; i++) {
        if (!secret->ts_backup_used[i]) {
            char code_str[16];
            snprintf(code_str, sizeof(code_str), "%06u", code);
            
            if (strcmp(secret->ts_backup_codes[i], code_str) == 0) {
                secret->ts_backup_used[i] = true;
                return true;
            }
        }
    }
    
    return false;
}
```

### 2.2 WebAuthn/FIDO2

```c
// WebAuthn configuration
typedef struct webauthn_config {
    // Relying Party
    char*               wc_rp_id;              // RP ID (domain)
    char*               wc_rp_name;            // RP display name
    char*               wc_rp_icon;            // RP icon URL
    
    // Challenge
    uint32_t            wc_challenge_size;     // Challenge size (bytes)
    uint32_t            wc_challenge_timeout;  // Timeout (seconds)
    
    // Attestation
    AttestationConveyance wc_attestation;      // Attestation preference
    
    // User verification
    UserVerification    wc_user_verification;  // User verification
    
    // Authenticator selection
    AuthenticatorAttachment wc_attachment;     // Platform/cross-platform
    bool                wc_resident_key;       // Require resident key
    
    // Supported algorithms
    int32_t*            wc_algorithms;         // COSE algorithm IDs
    uint32_t            wc_algorithm_count;    // Number of algorithms
} WebAuthnConfig;

// WebAuthn credential
typedef struct webauthn_credential {
    // Credential ID
    uint8_t             wc_id[256];            // Credential ID
    uint32_t            wc_id_len;             // ID length
    
    // Public key
    uint8_t             wc_public_key[256];    // Public key (COSE format)
    uint32_t            wc_public_key_len;     // Key length
    
    // User
    UserId              wc_user_id;            // User ID
    char                wc_username[256];      // Username
    
    // Authenticator data
    uint8_t             wc_aaguid[16];         // Authenticator AAGUID
    uint32_t            wc_sign_count;         // Signature counter
    
    // Metadata
    TimestampTz         wc_created_at;         // Creation time
    TimestampTz         wc_last_used;          // Last used
    char                wc_name[256];          // Credential name
    
    // Flags
    bool                wc_backup_eligible;    // Backup eligible
    bool                wc_backup_state;       // Backed up
} WebAuthnCredential;

// Start WebAuthn registration
WebAuthnChallenge* webauthn_start_registration(
    const char* username,
    WebAuthnConfig* config)
{
    WebAuthnChallenge* challenge = calloc(1, sizeof(WebAuthnChallenge));
    
    // Generate challenge
    challenge->challenge_len = config->wc_challenge_size;
    generate_random_bytes(challenge->challenge, challenge->challenge_len);
    
    // Generate user ID
    generate_random_bytes(challenge->user_id, 32);
    
    // Build credential creation options
    json_t* options = json_object();
    
    // Challenge
    char challenge_b64[256];
    base64url_encode(challenge->challenge, challenge->challenge_len,
                    challenge_b64, sizeof(challenge_b64));
    json_object_set_new(options, "challenge", json_string(challenge_b64));
    
    // Relying party
    json_t* rp = json_object();
    json_object_set_new(rp, "id", json_string(config->wc_rp_id));
    json_object_set_new(rp, "name", json_string(config->wc_rp_name));
    json_object_set_new(options, "rp", rp);
    
    // User
    json_t* user = json_object();
    char user_id_b64[64];
    base64url_encode(challenge->user_id, 32, user_id_b64, sizeof(user_id_b64));
    json_object_set_new(user, "id", json_string(user_id_b64));
    json_object_set_new(user, "name", json_string(username));
    json_object_set_new(user, "displayName", json_string(username));
    json_object_set_new(options, "user", user);
    
    // Public key credential parameters
    json_t* pubkey_params = json_array();
    for (uint32_t i = 0; i < config->wc_algorithm_count; i++) {
        json_t* param = json_object();
        json_object_set_new(param, "type", json_string("public-key"));
        json_object_set_new(param, "alg", json_integer(config->wc_algorithms[i]));
        json_array_append_new(pubkey_params, param);
    }
    json_object_set_new(options, "pubKeyCredParams", pubkey_params);
    
    // Authenticator selection
    json_t* auth_selection = json_object();
    if (config->wc_attachment == AUTHENTICATOR_PLATFORM) {
        json_object_set_new(auth_selection, "authenticatorAttachment",
                          json_string("platform"));
    } else if (config->wc_attachment == AUTHENTICATOR_CROSS_PLATFORM) {
        json_object_set_new(auth_selection, "authenticatorAttachment",
                          json_string("cross-platform"));
    }
    json_object_set_new(auth_selection, "requireResidentKey",
                       json_boolean(config->wc_resident_key));
    json_object_set_new(auth_selection, "userVerification",
                       json_string(user_verification_to_string(
                           config->wc_user_verification)));
    json_object_set_new(options, "authenticatorSelection", auth_selection);
    
    // Attestation
    json_object_set_new(options, "attestation",
                       json_string(attestation_to_string(config->wc_attestation)));
    
    // Timeout
    json_object_set_new(options, "timeout",
                       json_integer(config->wc_challenge_timeout * 1000));
    
    // Store challenge for verification
    store_webauthn_challenge(challenge);
    
    challenge->options_json = json_dumps(options, JSON_COMPACT);
    json_decref(options);
    
    return challenge;
}

// Verify WebAuthn registration
Status webauthn_verify_registration(
    const char* response_json,
    WebAuthnChallenge* challenge,
    WebAuthnConfig* config,
    WebAuthnCredential** credential_out)
{
    // Parse response
    json_error_t error;
    json_t* response = json_loads(response_json, 0, &error);
    
    if (!response) {
        log_error("Invalid WebAuthn response: %s", error.text);
        return STATUS_INVALID_JSON;
    }
    
    // Get client data JSON
    const char* client_data_b64 = json_string_value(
        json_object_get(response, "clientDataJSON"));
    
    size_t client_data_len;
    char* client_data_json = base64url_decode(client_data_b64, &client_data_len);
    
    // Parse client data
    json_t* client_data = json_loads(client_data_json, 0, NULL);
    free(client_data_json);
    
    // Verify challenge
    const char* challenge_b64 = json_string_value(
        json_object_get(client_data, "challenge"));
    
    size_t decoded_challenge_len;
    uint8_t* decoded_challenge = base64url_decode(challenge_b64,
                                                 &decoded_challenge_len);
    
    if (decoded_challenge_len != challenge->challenge_len ||
        memcmp(decoded_challenge, challenge->challenge,
               challenge->challenge_len) != 0) {
        free(decoded_challenge);
        json_decref(client_data);
        json_decref(response);
        return STATUS_INVALID_CHALLENGE;
    }
    
    free(decoded_challenge);
    
    // Verify origin
    const char* origin = json_string_value(
        json_object_get(client_data, "origin"));
    
    if (!verify_origin(origin, config->wc_rp_id)) {
        json_decref(client_data);
        json_decref(response);
        return STATUS_INVALID_ORIGIN;
    }
    
    // Verify type
    const char* type = json_string_value(
        json_object_get(client_data, "type"));
    
    if (strcmp(type, "webauthn.create") != 0) {
        json_decref(client_data);
        json_decref(response);
        return STATUS_INVALID_TYPE;
    }
    
    json_decref(client_data);
    
    // Get attestation object
    const char* attestation_b64 = json_string_value(
        json_object_get(response, "attestationObject"));
    
    size_t attestation_len;
    uint8_t* attestation_data = base64url_decode(attestation_b64,
                                                &attestation_len);
    
    // Parse CBOR attestation object
    WebAuthnCredential* credential = parse_attestation_object(
        attestation_data, attestation_len);
    
    free(attestation_data);
    json_decref(response);
    
    if (!credential) {
        return STATUS_INVALID_ATTESTATION;
    }
    
    // Store credential
    store_webauthn_credential(credential);
    
    *credential_out = credential;
    return STATUS_OK;
}
```

### 2.3 MFA Enrollment and Management

```c
// MFA method types
typedef enum mfa_method {
    MFA_TOTP,           // TOTP authenticator
    MFA_WEBAUTHN,       // WebAuthn/FIDO2
    MFA_SMS,            // SMS (not recommended)
    MFA_EMAIL,          // Email OTP
    MFA_BACKUP_CODES    // Backup codes
} MFAMethod;

// MFA enrollment
typedef struct mfa_enrollment {
    // User
    UserId              me_user_id;            // User ID
    char                me_username[256];      // Username
    
    // Method
    MFAMethod           me_method;             // MFA method
    void*               me_method_data;        // Method-specific data
    
    // State
    EnrollmentState     me_state;              // Enrollment state
    
    // Verification
    char                me_verification_code[32]; // Verification code
    TimestampTz         me_expires_at;         // Expiration time
} MFAEnrollment;

// Start MFA enrollment
MFAEnrollment* start_mfa_enrollment(
    const char* username,
    MFAMethod method)
{
    MFAEnrollment* enrollment = calloc(1, sizeof(MFAEnrollment));
    
    strncpy(enrollment->me_username, username, sizeof(enrollment->me_username));
    enrollment->me_method = method;
    enrollment->me_state = ENROLLMENT_STATE_PENDING;
    enrollment->me_expires_at = GetCurrentTimestamp() + 600; // 10 minutes
    
    switch (method) {
        case MFA_TOTP:
            {
                TOTPConfig config = get_totp_config();
                TOTPSecret* secret = generate_totp_secret(username, &config);
                enrollment->me_method_data = secret;
                
                // Generate QR code URI
                char* uri = generate_totp_uri(secret, &config);
                enrollment->me_qr_uri = uri;
            }
            break;
            
        case MFA_WEBAUTHN:
            {
                WebAuthnConfig config = get_webauthn_config();
                WebAuthnChallenge* challenge = webauthn_start_registration(
                    username, &config);
                enrollment->me_method_data = challenge;
            }
            break;
            
        case MFA_SMS:
        case MFA_EMAIL:
            {
                // Generate verification code
                generate_otp_code(enrollment->me_verification_code, 6);
                
                // Send code
                if (method == MFA_SMS) {
                    send_sms_code(username, enrollment->me_verification_code);
                } else {
                    send_email_code(username, enrollment->me_verification_code);
                }
            }
            break;
    }
    
    // Store enrollment
    store_mfa_enrollment(enrollment);
    
    return enrollment;
}

// Complete MFA enrollment
Status complete_mfa_enrollment(
    MFAEnrollment* enrollment,
    const char* verification_data)
{
    // Check expiration
    if (enrollment->me_expires_at < GetCurrentTimestamp()) {
        return STATUS_ENROLLMENT_EXPIRED;
    }
    
    Status status = STATUS_OK;
    
    switch (enrollment->me_method) {
        case MFA_TOTP:
            {
                // Verify TOTP code
                TOTPSecret* secret = (TOTPSecret*)enrollment->me_method_data;
                uint32_t code = strtoul(verification_data, NULL, 10);
                
                if (!verify_totp(secret, code, 1)) {
                    return STATUS_INVALID_TOTP_CODE;
                }
                
                // Store secret
                store_totp_secret(secret);
            }
            break;
            
        case MFA_WEBAUTHN:
            {
                // Verify WebAuthn registration
                WebAuthnChallenge* challenge = 
                    (WebAuthnChallenge*)enrollment->me_method_data;
                WebAuthnConfig config = get_webauthn_config();
                WebAuthnCredential* credential;
                
                status = webauthn_verify_registration(
                    verification_data, challenge, &config, &credential);
                
                if (status != STATUS_OK) {
                    return status;
                }
            }
            break;
            
        case MFA_SMS:
        case MFA_EMAIL:
            {
                // Verify OTP code
                if (strcmp(verification_data, 
                          enrollment->me_verification_code) != 0) {
                    return STATUS_INVALID_OTP_CODE;
                }
                
                // Store phone/email as verified
                if (enrollment->me_method == MFA_SMS) {
                    mark_phone_verified(enrollment->me_username);
                } else {
                    mark_email_verified(enrollment->me_username);
                }
            }
            break;
    }
    
    // Update enrollment state
    enrollment->me_state = ENROLLMENT_STATE_COMPLETE;
    update_mfa_enrollment(enrollment);
    
    // Enable MFA for user
    enable_user_mfa(enrollment->me_username, enrollment->me_method);
    
    return STATUS_OK;
}

// MFA verification during login
Status verify_mfa(
    const char* username,
    MFAMethod method,
    const char* code)
{
    switch (method) {
        case MFA_TOTP:
            {
                TOTPSecret* secret = get_user_totp_secret(username);
                if (!secret) {
                    return STATUS_MFA_NOT_ENROLLED;
                }
                
                uint32_t totp_code = strtoul(code, NULL, 10);
                if (!verify_totp(secret, totp_code, 1)) {
                    return STATUS_INVALID_MFA_CODE;
                }
            }
            break;
            
        case MFA_WEBAUTHN:
            {
                // Parse assertion response
                WebAuthnCredential* credential = get_user_webauthn_credential(
                    username, code);
                
                if (!credential) {
                    return STATUS_INVALID_CREDENTIAL;
                }
                
                // Verify assertion
                Status status = webauthn_verify_assertion(code, credential);
                if (status != STATUS_OK) {
                    return status;
                }
            }
            break;
            
        case MFA_BACKUP_CODES:
            {
                if (!verify_backup_code(username, code)) {
                    return STATUS_INVALID_BACKUP_CODE;
                }
            }
            break;
            
        default:
            return STATUS_UNSUPPORTED_MFA_METHOD;
    }
    
    // Log successful MFA
    audit_log_mfa_success(username, method);
    
    return STATUS_OK;
}
```

## 3. SAML 2.0 Support

### 3.1 SAML Configuration

```c
// SAML 2.0 configuration
typedef struct saml_config {
    // Service Provider (SP)
    char*               sc_sp_entity_id;       // SP entity ID
    char*               sc_sp_acs_url;         // Assertion Consumer Service URL
    char*               sc_sp_sls_url;         // Single Logout Service URL
    
    // Identity Provider (IdP)
    char*               sc_idp_entity_id;      // IdP entity ID
    char*               sc_idp_sso_url;        // SSO URL
    char*               sc_idp_sls_url;        // SLS URL
    X509*               sc_idp_cert;           // IdP certificate
    
    // SP certificate and key
    X509*               sc_sp_cert;            // SP certificate
    EVP_PKEY*           sc_sp_key;             // SP private key
    
    // Binding
    SAMLBinding         sc_binding;            // HTTP-POST, HTTP-Redirect
    
    // Security
    bool                sc_sign_requests;      // Sign AuthnRequests
    bool                sc_sign_assertions;    // Require signed assertions
    bool                sc_encrypt_assertions; // Require encrypted assertions
    
    // Name ID
    SAMLNameIDFormat    sc_nameid_format;      // NameID format
    
    // Attribute mapping
    HashTable*          sc_attribute_map;      // SAML attr -> DB field
} SAMLConfig;

// Create SAML AuthnRequest
char* create_saml_authn_request(SAMLConfig* config) {
    // Generate request ID
    char request_id[64];
    generate_saml_id(request_id, sizeof(request_id));
    
    // Create timestamp
    char issue_instant[64];
    format_iso8601_timestamp(GetCurrentTimestamp(), issue_instant);
    
    // Build AuthnRequest XML
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST "AuthnRequest");
    xmlDocSetRootElement(doc, root);
    
    // Set namespaces
    xmlNewNs(root, BAD_CAST "urn:oasis:names:tc:SAML:2.0:protocol",
            BAD_CAST "samlp");
    xmlNewNs(root, BAD_CAST "urn:oasis:names:tc:SAML:2.0:assertion",
            BAD_CAST "saml");
    
    // Set attributes
    xmlNewProp(root, BAD_CAST "ID", BAD_CAST request_id);
    xmlNewProp(root, BAD_CAST "Version", BAD_CAST "2.0");
    xmlNewProp(root, BAD_CAST "IssueInstant", BAD_CAST issue_instant);
    xmlNewProp(root, BAD_CAST "Destination", BAD_CAST config->sc_idp_sso_url);
    xmlNewProp(root, BAD_CAST "AssertionConsumerServiceURL",
              BAD_CAST config->sc_sp_acs_url);
    
    // Add Issuer
    xmlNodePtr issuer = xmlNewChild(root, NULL, BAD_CAST "Issuer",
                                   BAD_CAST config->sc_sp_entity_id);
    
    // Add NameIDPolicy
    xmlNodePtr nameid_policy = xmlNewChild(root, NULL,
                                          BAD_CAST "NameIDPolicy", NULL);
    xmlNewProp(nameid_policy, BAD_CAST "Format",
              BAD_CAST nameid_format_to_string(config->sc_nameid_format));
    
    // Sign if configured
    if (config->sc_sign_requests) {
        sign_saml_request(doc, config->sc_sp_key, config->sc_sp_cert);
    }
    
    // Serialize
    xmlChar* xml_buf;
    int xml_len;
    xmlDocDumpMemory(doc, &xml_buf, &xml_len);
    
    char* result = strdup((char*)xml_buf);
    
    xmlFree(xml_buf);
    xmlFreeDoc(doc);
    
    return result;
}

// Process SAML Response
Status process_saml_response(
    const char* saml_response_b64,
    SAMLConfig* config,
    SAMLAssertion** assertion_out)
{
    // Decode response
    size_t response_len;
    char* saml_response = base64_decode(saml_response_b64, &response_len);
    
    // Parse XML
    xmlDocPtr doc = xmlReadMemory(saml_response, response_len,
                                 NULL, NULL, XML_PARSE_NONET);
    free(saml_response);
    
    if (!doc) {
        return STATUS_INVALID_SAML_RESPONSE;
    }
    
    // Verify signature if required
    if (config->sc_sign_assertions) {
        if (!verify_saml_signature(doc, config->sc_idp_cert)) {
            xmlFreeDoc(doc);
            return STATUS_INVALID_SAML_SIGNATURE;
        }
    }
    
    // Extract assertion
    SAMLAssertion* assertion = extract_saml_assertion(doc);
    
    if (!assertion) {
        xmlFreeDoc(doc);
        return STATUS_NO_SAML_ASSERTION;
    }
    
    // Validate assertion
    Status status = validate_saml_assertion(assertion, config);
    
    if (status != STATUS_OK) {
        free_saml_assertion(assertion);
        xmlFreeDoc(doc);
        return status;
    }
    
    xmlFreeDoc(doc);
    *assertion_out = assertion;
    
    return STATUS_OK;
}
```

## 4. Configuration Examples

### 4.1 OAuth 2.0 Configuration

```yaml
# OAuth 2.0 / OpenID Connect configuration
oauth2:
  providers:
    - name: google
      issuer: https://accounts.google.com
      client_id: ${GOOGLE_CLIENT_ID}
      client_secret: ${GOOGLE_CLIENT_SECRET}
      redirect_uri: https://db.example.com/auth/callback/google
      scopes:
        - openid
        - email
        - profile
      user_mapping:
        username: email
        display_name: name
        
    - name: azure
      issuer: https://login.microsoftonline.com/{tenant}/v2.0
      client_id: ${AZURE_CLIENT_ID}
      client_secret: ${AZURE_CLIENT_SECRET}
      redirect_uri: https://db.example.com/auth/callback/azure
      scopes:
        - openid
        - email
        - profile
        - offline_access
      require_pkce: true
      
    - name: github
      auth_endpoint: https://github.com/login/oauth/authorize
      token_endpoint: https://github.com/login/oauth/access_token
      userinfo_endpoint: https://api.github.com/user
      client_id: ${GITHUB_CLIENT_ID}
      client_secret: ${GITHUB_CLIENT_SECRET}
      redirect_uri: https://db.example.com/auth/callback/github
      scopes:
        - user:email
        - read:org

# MFA configuration
mfa:
  required: false  # Make MFA mandatory
  enrollment_required: true  # Require enrollment on first login
  
  methods:
    totp:
      enabled: true
      issuer: ScratchBird
      algorithm: SHA256
      digits: 6
      period: 30
      window: 1
      backup_codes: 10
      
    webauthn:
      enabled: true
      rp_id: db.example.com
      rp_name: ScratchBird Database
      attestation: direct
      user_verification: preferred
      algorithms:
        - -7   # ES256
        - -257 # RS256
        
    sms:
      enabled: false  # Not recommended
      provider: twilio
      
    email:
      enabled: true
      from: noreply@example.com
      
# SAML configuration
saml:
  enabled: true
  sp:
    entity_id: https://db.example.com
    acs_url: https://db.example.com/saml/acs
    sls_url: https://db.example.com/saml/sls
    cert_file: /etc/scratchbird/saml-sp.crt
    key_file: /etc/scratchbird/saml-sp.key
    
  idp:
    entity_id: https://idp.example.com
    sso_url: https://idp.example.com/sso
    sls_url: https://idp.example.com/sls
    cert_file: /etc/scratchbird/saml-idp.crt
    
  security:
    sign_requests: true
    require_signed_assertions: true
    require_encrypted_assertions: false
    
  attribute_mapping:
    username: http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name
    email: http://schemas.xmlsoap.org/ws/2005/05/identity/claims/emailaddress
    groups: http://schemas.microsoft.com/ws/2008/06/identity/claims/groups
```

## 5. Testing

### 5.1 OAuth/MFA Test Suite

```c
// OAuth 2.0 tests
bool test_oauth2_flow() {
    OAuth2Config config = {
        .oc_issuer = "https://test.example.com",
        .oc_client_id = "test_client",
        .oc_client_secret = "test_secret",
        .oc_redirect_uri = "https://localhost/callback"
    };
    
    OAuth2AuthFlow flow = {0};
    
    // Start authorization
    char* auth_url = oauth2_start_authorization(&config, &flow);
    assert(auth_url != NULL);
    assert(strstr(auth_url, "response_type=code") != NULL);
    free(auth_url);
    
    // Simulate callback
    Status status = oauth2_handle_callback(&config, &flow,
                                          "test_code", flow.oaf_state_param);
    
    // Would need mock server for full test
    
    return true;
}

// TOTP tests
bool test_totp() {
    TOTPConfig config = {
        .tc_algorithm = HASH_SHA256,
        .tc_digits = 6,
        .tc_period = 30
    };
    
    TOTPSecret* secret = generate_totp_secret("testuser", &config);
    assert(secret != NULL);
    
    // Test code generation
    time_t test_time = 1234567890;
    uint32_t code = calculate_totp(secret, test_time);
    assert(code > 0 && code < 1000000);
    
    // Test verification
    assert(verify_totp(secret, code, 0) == true);
    
    // Test invalid code
    assert(verify_totp(secret, 123456, 0) == false);
    
    free_totp_secret(secret);
    return true;
}

// WebAuthn tests
bool test_webauthn() {
    WebAuthnConfig config = {
        .wc_rp_id = "localhost",
        .wc_rp_name = "Test RP",
        .wc_challenge_size = 32,
        .wc_user_verification = USER_VERIFICATION_PREFERRED
    };
    
    // Start registration
    WebAuthnChallenge* challenge = webauthn_start_registration(
        "testuser", &config);
    assert(challenge != NULL);
    assert(challenge->options_json != NULL);
    
    // Would need browser simulation for full test
    
    free_webauthn_challenge(challenge);
    return true;
}
```

## Implementation Notes

This modern authentication implementation provides:

1. **Complete OAuth 2.0/OIDC** with PKCE, JWT validation, and token management
2. **Comprehensive MFA** with TOTP, WebAuthn/FIDO2, and backup codes
3. **SAML 2.0 support** for enterprise SSO
4. **Flexible configuration** for multiple providers and methods
5. **Security best practices** including state validation and signature verification
6. **Extensible architecture** for adding new authentication methods

The system supports modern authentication standards while maintaining security and usability.