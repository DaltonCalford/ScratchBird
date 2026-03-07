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
 * ScratchBird SCRAM-SHA-256/512 Authentication
 *
 * Authoritative inbound SCRAM implementation for ScratchBird.
 *
 * Implements RFC 5802 (SCRAM) with SHA-256 and SHA-512 for the engine's
 * native client/server authentication paths.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/security/auth_method.h"

namespace scratchbird {
namespace security {

// Forward declaration
class CredentialStore;

// ============================================================================
// SCRAM Configuration
// ============================================================================

/**
 * SCRAM algorithm variant
 */
enum class ScramAlgorithm : uint8_t {
    SHA_256 = 0,
    SHA_512 = 1
};

/**
 * SCRAM configuration
 */
struct ScramConfig {
    // Algorithm
    ScramAlgorithm algorithm = ScramAlgorithm::SHA_256;
    uint32_t iterations = 4096;     // Minimum 4096 per RFC

    // Nonce
    uint32_t nonce_length = 24;     // Server nonce length

    // Channel binding
    bool channel_binding_required = false;

    // Security
    bool allow_empty_password = false;
};

// ============================================================================
// SCRAM State
// ============================================================================

/**
 * SCRAM authentication state machine phase
 */
enum class ScramPhase : uint8_t {
    INITIAL = 0,
    CLIENT_FIRST_RECEIVED = 1,
    SERVER_FIRST_SENT = 2,
    CLIENT_FINAL_RECEIVED = 3,
    COMPLETE = 4,
    FAILED = 5
};

/**
 * SCRAM authentication state
 *
 * Holds all state needed for the SCRAM exchange.
 */
class ScramState {
public:
    ScramState(ScramAlgorithm algo = ScramAlgorithm::SHA_256);
    ~ScramState();

    // Phase tracking
    ScramPhase phase() const { return phase_; }
    void setPhase(ScramPhase phase) { phase_ = phase; }

    // Client first message
    void setClientNonce(const std::string& nonce) { client_nonce_ = nonce; }
    const std::string& clientNonce() const { return client_nonce_; }

    void setClientFirstMessageBare(const std::string& msg) { client_first_bare_ = msg; }
    const std::string& clientFirstMessageBare() const { return client_first_bare_; }

    // Server first message
    void setServerNonce(const std::string& nonce) { server_nonce_ = nonce; }
    const std::string& serverNonce() const { return server_nonce_; }

    void setServerFirstMessage(const std::string& msg) { server_first_msg_ = msg; }
    const std::string& serverFirstMessage() const { return server_first_msg_; }

    // Salt and iterations
    void setSalt(const std::vector<uint8_t>& salt) { salt_ = salt; }
    const std::vector<uint8_t>& salt() const { return salt_; }

    void setIterations(uint32_t iters) { iterations_ = iters; }
    uint32_t iterations() const { return iterations_; }

    // Stored credentials
    void setStoredKey(const std::vector<uint8_t>& key) { stored_key_ = key; }
    const std::vector<uint8_t>& storedKey() const { return stored_key_; }

    void setServerKey(const std::vector<uint8_t>& key) { server_key_ = key; }
    const std::vector<uint8_t>& serverKey() const { return server_key_; }

    // Channel binding
    void setChannelBinding(const std::string& cb) { channel_binding_ = cb; }
    const std::string& channelBinding() const { return channel_binding_; }

    void setChannelBindingFlag(char flag) { cb_flag_ = flag; }
    char channelBindingFlag() const { return cb_flag_; }

    // Client final without proof
    void setClientFinalWithoutProof(const std::string& msg) { client_final_without_proof_ = msg; }
    const std::string& clientFinalWithoutProof() const { return client_final_without_proof_; }

    // Algorithm
    ScramAlgorithm algorithm() const { return algorithm_; }

    // Auth message (for signature verification)
    std::string getAuthMessage() const;

private:
    ScramAlgorithm algorithm_;
    ScramPhase phase_ = ScramPhase::INITIAL;

    // Client first
    std::string client_nonce_;
    std::string client_first_bare_;

    // Server first
    std::string server_nonce_;
    std::string server_first_msg_;

    // Credential data
    std::vector<uint8_t> salt_;
    uint32_t iterations_ = 0;
    std::vector<uint8_t> stored_key_;
    std::vector<uint8_t> server_key_;

    // Channel binding
    std::string channel_binding_;
    char cb_flag_ = 'n';  // 'n' = not supported, 'y' = supported, 'p' = required

    // Client final
    std::string client_final_without_proof_;
};

// ============================================================================
// SCRAM Authentication Method
// ============================================================================

/**
 * SCRAM-SHA-256 Authentication Method
 */
class ScramSHA256AuthMethod : public AuthMethod {
public:
    ScramSHA256AuthMethod();
    ~ScramSHA256AuthMethod();

    AuthType type() const override { return AuthType::SCRAM_SHA_256; }
    const char* name() const override { return "scram-sha-256"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                             core::ErrorContext* ctx = nullptr) override;

    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx,
                             const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;

    bool supportsPasswordVerification() const override { return true; }
    bool verifyPassword(const std::string& username,
                        const std::string& password) override;

    /**
     * Set credential store for user lookup
     */
    void setCredentialStore(std::shared_ptr<CredentialStore> store);

    /**
     * Get configuration
     */
    const ScramConfig& config() const { return config_; }

private:
    AuthResult processClientFirst(AuthContext& ctx, const std::string& message);
    AuthResult processClientFinal(AuthContext& ctx, const std::string& message);

    std::string generateServerFirst(ScramState& state);
    std::string generateServerFinal(ScramState& state, const std::vector<uint8_t>& server_signature);

    bool verifyClientProof(ScramState& state, const std::vector<uint8_t>& client_proof);
    std::vector<uint8_t> calculateServerSignature(ScramState& state);

    ScramConfig config_;
    std::shared_ptr<CredentialStore> credential_store_;

    // State storage (keyed by AuthContext pointer)
    std::map<AuthContext*, std::unique_ptr<ScramState>> states_;
    std::mutex states_mutex_;
};

/**
 * SCRAM-SHA-512 Authentication Method
 */
class ScramSHA512AuthMethod : public AuthMethod {
public:
    ScramSHA512AuthMethod();
    ~ScramSHA512AuthMethod();

    AuthType type() const override { return AuthType::SCRAM_SHA_512; }
    const char* name() const override { return "scram-sha-512"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                             core::ErrorContext* ctx = nullptr) override;

    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx,
                             const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;

    bool supportsPasswordVerification() const override { return true; }
    bool verifyPassword(const std::string& username,
                        const std::string& password) override;

    void setCredentialStore(std::shared_ptr<CredentialStore> store);
    const ScramConfig& config() const { return config_; }

private:
    AuthResult processClientFirst(AuthContext& ctx, const std::string& message);
    AuthResult processClientFinal(AuthContext& ctx, const std::string& message);

    std::string generateServerFirst(ScramState& state);
    std::string generateServerFinal(ScramState& state, const std::vector<uint8_t>& server_signature);

    bool verifyClientProof(ScramState& state, const std::vector<uint8_t>& client_proof);
    std::vector<uint8_t> calculateServerSignature(ScramState& state);

    ScramConfig config_;
    std::shared_ptr<CredentialStore> credential_store_;
    std::map<AuthContext*, std::unique_ptr<ScramState>> states_;
    std::mutex states_mutex_;
};

// ============================================================================
// SCRAM Utility Functions
// ============================================================================

/**
 * Generate SCRAM credentials from password
 *
 * This creates the StoredKey and ServerKey that are stored in the database.
 *
 * @param password User password
 * @param algorithm SCRAM algorithm
 * @param iterations PBKDF2 iteration count
 * @param salt Random salt (will be generated if empty)
 * @param stored_key Output: StoredKey = H(ClientKey)
 * @param server_key Output: ServerKey = HMAC(SaltedPassword, "Server Key")
 * @return Status
 */
core::Status generateScramCredentials(
    const std::string& password,
    ScramAlgorithm algorithm,
    uint32_t iterations,
    std::vector<uint8_t>& salt,
    std::vector<uint8_t>& stored_key,
    std::vector<uint8_t>& server_key);

/**
 * Calculate SaltedPassword = PBKDF2(password, salt, iterations)
 */
core::Status calculateSaltedPassword(
    const std::string& password,
    const std::vector<uint8_t>& salt,
    uint32_t iterations,
    ScramAlgorithm algorithm,
    std::vector<uint8_t>& salted_password);

/**
 * Generate random nonce
 */
std::string generateNonce(size_t length = 24);

/**
 * Normalize username per SCRAM spec (SASLprep)
 */
std::string normalizeUsername(const std::string& username);

/**
 * Parse SCRAM client-first message
 */
struct ScramClientFirst {
    char gs2_flag = 'n';            // 'n', 'y', or 'p'
    std::string authzid;            // Authorization identity (optional)
    std::string username;           // Normalized username
    std::string client_nonce;       // Client nonce (r=)
    std::string client_first_bare;  // Message without GS2 header
    std::string channel_binding;    // Channel binding data (if flag='p')
};
bool parseClientFirst(const std::string& message, ScramClientFirst& parsed);

/**
 * Parse SCRAM client-final message
 */
struct ScramClientFinal {
    std::string channel_binding;    // Base64 channel binding (c=)
    std::string nonce;              // Combined nonce (r=)
    std::string proof;              // Base64 client proof (p=)
    std::string without_proof;      // Message without proof
};
bool parseClientFinal(const std::string& message, ScramClientFinal& parsed);

/**
 * Base64 encode
 */
std::string base64Encode(const std::vector<uint8_t>& data);
std::string base64Encode(const uint8_t* data, size_t len);

/**
 * Base64 decode
 */
std::vector<uint8_t> base64Decode(const std::string& encoded);

/**
 * XOR two byte arrays
 */
void xorBytes(std::vector<uint8_t>& a, const std::vector<uint8_t>& b);

}  // namespace security
}  // namespace scratchbird
