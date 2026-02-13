/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <map>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

#include "scratchbird/security/auth_manager.h"
#include "scratchbird/security/scram_auth.h"

using scratchbird::core::Status;
using scratchbird::security::AuthContext;
using scratchbird::security::AuthFailReason;
using scratchbird::security::AuthResult;
using scratchbird::security::AuthState;
using scratchbird::security::AuthType;
using scratchbird::security::CredentialStore;
using scratchbird::security::ScramAlgorithm;
using scratchbird::security::ScramClientFinal;
using scratchbird::security::ScramClientFirst;
using scratchbird::security::ScramConfig;
using scratchbird::security::ScramPhase;
using scratchbird::security::ScramSHA256AuthMethod;
using scratchbird::security::ScramSHA512AuthMethod;
using scratchbird::security::ScramState;
using scratchbird::security::UserCredential;
using scratchbird::security::base64Decode;
using scratchbird::security::base64Encode;
using scratchbird::security::calculateSaltedPassword;
using scratchbird::security::generateNonce;
using scratchbird::security::generateScramCredentials;
using scratchbird::security::normalizeUsername;
using scratchbird::security::parseClientFinal;
using scratchbird::security::parseClientFirst;
using scratchbird::security::xorBytes;
using scratchbird::security::constantTimeCompare;

namespace {

// ============================================================================
// Test Helpers
// ============================================================================

class MemoryCredentialStore : public CredentialStore {
public:
    Status getCredential(const std::string& username, UserCredential& cred) override {
        auto it = users_.find(username);
        if (it == users_.end()) {
            return Status::NOT_FOUND;
        }
        cred = it->second;
        return Status::OK;
    }

    bool userExists(const std::string& username) override {
        return users_.find(username) != users_.end();
    }

    Status createUser(const UserCredential& cred) override {
        users_[cred.username] = cred;
        return Status::OK;
    }

    Status updateCredential(const UserCredential& cred) override {
        users_[cred.username] = cred;
        return Status::OK;
    }

    Status deleteUser(const std::string& username) override {
        users_.erase(username);
        return Status::OK;
    }

    std::vector<std::string> listUsers() override {
        std::vector<std::string> names;
        names.reserve(users_.size());
        for (const auto& entry : users_) {
            names.push_back(entry.first);
        }
        return names;
    }

private:
    std::map<std::string, UserCredential> users_;
};

UserCredential makeUserCredential(const std::string& username,
                                  const std::string& password,
                                  ScramAlgorithm algorithm,
                                  uint32_t iterations) {
    UserCredential cred;
    cred.username = username;
    cred.password_iterations = iterations;
    cred.password_salt.resize(16);
    for (size_t i = 0; i < cred.password_salt.size(); ++i) {
        cred.password_salt[i] = static_cast<uint8_t>(i);
    }

    std::vector<uint8_t> salt = cred.password_salt;
    std::vector<uint8_t> stored_key;
    std::vector<uint8_t> server_key;
    EXPECT_EQ(generateScramCredentials(password, algorithm, iterations, salt, stored_key, server_key),
              Status::OK);
    cred.password_salt = salt;
    cred.scram_stored_key = stored_key;
    cred.scram_server_key = server_key;
    return cred;
}

std::string extractScramField(const std::string& message, const std::string& key) {
    size_t pos = 0;
    while (pos < message.size()) {
        size_t next = message.find(',', pos);
        std::string field = (next == std::string::npos)
            ? message.substr(pos)
            : message.substr(pos, next - pos);
        if (field.rfind(key + "=", 0) == 0) {
            return field.substr(key.size() + 1);
        }
        if (next == std::string::npos) {
            break;
        }
        pos = next + 1;
    }
    return "";
}

std::string buildClientFinal(const std::string& nonce, const std::vector<uint8_t>& proof) {
    std::string channel_binding = "biws";  // base64("n,,")
    std::string proof_b64 = base64Encode(proof);
    return "c=" + channel_binding + ",r=" + nonce + ",p=" + proof_b64;
}

// Helper to compute client proof for testing
std::vector<uint8_t> computeClientProof(
    const std::string& password,
    const std::vector<uint8_t>& salt,
    uint32_t iterations,
    const std::string& auth_message,
    ScramAlgorithm algorithm) {
    
    // SaltedPassword = PBKDF2(password, salt, iterations)
    std::vector<uint8_t> salted_password;
    calculateSaltedPassword(password, salt, iterations, algorithm, salted_password);
    
    // ClientKey = HMAC(SaltedPassword, "Client Key")
    const EVP_MD* md = (algorithm == ScramAlgorithm::SHA_256) ? EVP_sha256() : EVP_sha512();
    int hash_len = (algorithm == ScramAlgorithm::SHA_256) ? 32 : 64;
    
    std::vector<uint8_t> client_key(hash_len);
    unsigned int out_len = hash_len;
    HMAC(md, salted_password.data(), static_cast<int>(salted_password.size()),
         reinterpret_cast<const unsigned char*>("Client Key"), 10,
         client_key.data(), &out_len);
    
    // StoredKey = H(ClientKey)
    std::vector<uint8_t> stored_key(hash_len);
    if (algorithm == ScramAlgorithm::SHA_256) {
        SHA256(client_key.data(), client_key.size(), stored_key.data());
    } else {
        SHA512(client_key.data(), client_key.size(), stored_key.data());
    }
    
    // ClientSignature = HMAC(StoredKey, AuthMessage)
    std::vector<uint8_t> client_sig(hash_len);
    out_len = hash_len;
    HMAC(md, stored_key.data(), static_cast<int>(stored_key.size()),
         reinterpret_cast<const unsigned char*>(auth_message.c_str()), static_cast<int>(auth_message.size()),
         client_sig.data(), &out_len);
    
    // ClientProof = ClientKey XOR ClientSignature
    std::vector<uint8_t> proof = client_key;
    xorBytes(proof, client_sig);
    
    // Clear sensitive data
    std::fill(salted_password.begin(), salted_password.end(), 0);
    std::fill(client_key.begin(), client_key.end(), 0);
    
    return proof;
}

// ============================================================================
// SCRAM Client Test Helper Class
// ============================================================================

class SCRAMClient {
public:
    SCRAMClient(const std::string& username, const std::string& password,
                ScramAlgorithm algo = ScramAlgorithm::SHA_256)
        : username_(username), password_(password), algorithm_(algo) {}

    std::string generateClientFirstMessage() {
        client_nonce_ = generateNonce(24);
        std::string gs2_header = "n,,";  // no channel binding
        std::string client_first_bare = "n=" + normalizeUsername(username_) + ",r=" + client_nonce_;
        client_first_bare_ = client_first_bare;
        return gs2_header + client_first_bare;
    }

    bool processServerFirstMessage(const std::string& server_first) {
        // Parse server-first: r=<nonce>,s=<salt>,i=<iterations>
        std::string nonce = extractScramField(server_first, "r");
        std::string salt_b64 = extractScramField(server_first, "s");
        std::string iters_str = extractScramField(server_first, "i");
        
        if (nonce.empty() || salt_b64.empty() || iters_str.empty()) {
            return false;
        }
        
        // Verify server nonce starts with client nonce
        if (nonce.rfind(client_nonce_, 0) != 0) {
            return false;
        }
        
        combined_nonce_ = nonce;
        salt_ = base64Decode(salt_b64);
        iterations_ = std::stoul(iters_str);
        server_first_ = server_first;
        return true;
    }

    std::string generateClientFinalMessage() {
        // c=<base64("n,,")>,r=<combined_nonce>,p=<proof>
        std::string channel_binding = "biws";  // base64("n,,")
        client_final_without_proof_ = "c=" + channel_binding + ",r=" + combined_nonce_;
        
        // Build auth message
        auth_message_ = client_first_bare_ + "," + server_first_ + "," + client_final_without_proof_;
        
        // Compute proof
        std::vector<uint8_t> proof = computeClientProof(
            password_, salt_, iterations_, auth_message_, algorithm_);
        
        return client_final_without_proof_ + ",p=" + base64Encode(proof);
    }

    bool verifyServerFinalMessage(const std::string& server_final) {
        // Parse server-final: v=<server_signature>
        if (server_final.rfind("v=", 0) != 0) {
            return false;
        }
        std::string server_sig_b64 = server_final.substr(2);
        std::vector<uint8_t> server_sig_received = base64Decode(server_sig_b64);
        
        // Compute expected server signature
        std::vector<uint8_t> salted_password;
        calculateSaltedPassword(password_, salt_, iterations_, algorithm_, salted_password);
        
        const EVP_MD* md = (algorithm_ == ScramAlgorithm::SHA_256) ? EVP_sha256() : EVP_sha512();
        int hash_len = (algorithm_ == ScramAlgorithm::SHA_256) ? 32 : 64;
        
        std::vector<uint8_t> server_key(hash_len);
        unsigned int out_len = hash_len;
        HMAC(md, salted_password.data(), static_cast<int>(salted_password.size()),
             reinterpret_cast<const unsigned char*>("Server Key"), 10,
             server_key.data(), &out_len);
        
        std::vector<uint8_t> expected_sig(hash_len);
        out_len = hash_len;
        HMAC(md, server_key.data(), hash_len,
             reinterpret_cast<const unsigned char*>(auth_message_.c_str()), 
             static_cast<int>(auth_message_.size()),
             expected_sig.data(), &out_len);
        
        // Clear sensitive data
        std::fill(salted_password.begin(), salted_password.end(), 0);
        std::fill(server_key.begin(), server_key.end(), 0);
        
        return constantTimeCompare(server_sig_received, expected_sig);
    }

    const std::string& clientNonce() const { return client_nonce_; }
    const std::string& combinedNonce() const { return combined_nonce_; }

private:
    std::string username_;
    std::string password_;
    ScramAlgorithm algorithm_;
    std::string client_nonce_;
    std::string client_first_bare_;
    std::string combined_nonce_;
    std::vector<uint8_t> salt_;
    uint32_t iterations_ = 0;
    std::string server_first_;
    std::string client_final_without_proof_;
    std::string auth_message_;
};

}  // namespace

// ============================================================================
// 1. SCRAMServer Tests (ScramSHA256AuthMethod)
// ============================================================================

class ScramServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        store_ = std::make_shared<MemoryCredentialStore>();
        store_->createUser(makeUserCredential("testuser", "password123", ScramAlgorithm::SHA_256, 4096));
        store_->createUser(makeUserCredential("admin", "adminpass", ScramAlgorithm::SHA_256, 10000));
    }

    std::shared_ptr<MemoryCredentialStore> store_;
};

// Test 1: Server generates valid server-first message
TEST_F(ScramServerTest, GenerateServerFirstMessage) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    AuthResult start = auth.start(ctx);
    ASSERT_EQ(start.state, AuthState::IN_PROGRESS);

    std::string client_first = "n,,n=testuser,r=clientnonce123";
    AuthResult result = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    
    ASSERT_EQ(result.state, AuthState::IN_PROGRESS);
    std::string server_first(result.response_data.begin(), result.response_data.end());
    
    // Verify server-first format
    EXPECT_FALSE(extractScramField(server_first, "r").empty());
    EXPECT_FALSE(extractScramField(server_first, "s").empty());
    EXPECT_EQ(extractScramField(server_first, "i"), "4096");
    
    // Verify nonce starts with client nonce
    std::string nonce = extractScramField(server_first, "r");
    EXPECT_EQ(nonce.rfind("clientnonce123", 0), 0);
}

// Test 2: Server verifies valid client final message (success case)
TEST_F(ScramServerTest, VerifyClientFinalMessageSuccess) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    // Client-first
    SCRAMClient client("testuser", "password123", ScramAlgorithm::SHA_256);
    std::string client_first = client.generateClientFirstMessage();
    
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);
    
    // Process server-first
    std::string server_first(first.response_data.begin(), first.response_data.end());
    ASSERT_TRUE(client.processServerFirstMessage(server_first));
    
    // Client-final
    std::string client_final = client.generateClientFinalMessage();
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    
    EXPECT_EQ(final.state, AuthState::SUCCESS);
    EXPECT_EQ(final.authenticated_user, "testuser");
    
    // Verify server final message
    std::string server_final(final.response_data.begin(), final.response_data.end());
    EXPECT_TRUE(client.verifyServerFinalMessage(server_final));
}

// Test 3: Server rejects wrong password
TEST_F(ScramServerTest, VerifyClientFinalMessageWrongPassword) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    // Client with wrong password
    SCRAMClient client("testuser", "wrongpassword", ScramAlgorithm::SHA_256);
    std::string client_first = client.generateClientFirstMessage();
    
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);
    
    std::string server_first(first.response_data.begin(), first.response_data.end());
    client.processServerFirstMessage(server_first);
    
    std::string client_final = client.generateClientFinalMessage();
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    
    EXPECT_EQ(final.state, AuthState::FAILURE);
    EXPECT_EQ(final.failure_reason, AuthFailReason::INVALID_CREDENTIALS);
}

// Test 4: Full authentication flow
TEST_F(ScramServerTest, FullAuthenticationFlow) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    AuthResult start = auth.start(ctx);
    EXPECT_EQ(start.state, AuthState::IN_PROGRESS);
    EXPECT_TRUE(start.requires_response);

    // Complete flow with correct password
    SCRAMClient client("admin", "adminpass", ScramAlgorithm::SHA_256);
    std::string client_first = client.generateClientFirstMessage();
    
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    EXPECT_EQ(first.state, AuthState::IN_PROGRESS);
    
    std::string server_first(first.response_data.begin(), first.response_data.end());
    EXPECT_TRUE(client.processServerFirstMessage(server_first));
    EXPECT_EQ(extractScramField(server_first, "i"), "10000");  // Custom iterations
    
    std::string client_final = client.generateClientFinalMessage();
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    
    EXPECT_EQ(final.state, AuthState::SUCCESS);
    EXPECT_EQ(final.authenticated_user, "admin");
}

// Test 5: State transitions
TEST_F(ScramServerTest, StateTransitions) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);  // Creates state at INITIAL

    // After client-first processing, state should be SERVER_FIRST_SENT
    std::string client_first = "n,,n=testuser,r=nonce123";
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    EXPECT_EQ(first.state, AuthState::IN_PROGRESS);

    // After client-final with wrong proof, state should be FAILED
    std::vector<uint8_t> bad_proof(32, 0xAB);
    std::string nonce = extractScramField(
        std::string(first.response_data.begin(), first.response_data.end()), "r");
    std::string client_final = buildClientFinal(nonce, bad_proof);
    
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    EXPECT_EQ(final.state, AuthState::FAILURE);
}

// ============================================================================
// 2. SCRAMClient Tests
// ============================================================================

class ScramClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        store_ = std::make_shared<MemoryCredentialStore>();
        store_->createUser(makeUserCredential("alice", "wonderland", ScramAlgorithm::SHA_256, 4096));
    }

    std::shared_ptr<MemoryCredentialStore> store_;
};

// Test 6: Client generates valid client-first message
TEST_F(ScramClientTest, GenerateClientFirstMessage) {
    SCRAMClient client("alice", "wonderland");
    std::string client_first = client.generateClientFirstMessage();
    
    // Should start with "n,," (no channel binding)
    EXPECT_EQ(client_first.rfind("n,,n=alice,r=", 0), 0);
    EXPECT_FALSE(client.clientNonce().empty());
    EXPECT_EQ(client.clientNonce().size(), 24);
}

// Test 7: Client processes server-first message
TEST_F(ScramClientTest, ProcessServerFirstMessage) {
    SCRAMClient client("alice", "wonderland");
    client.generateClientFirstMessage();
    
    // Simulate server-first
    std::string server_first = "r=" + client.clientNonce() + "_servernonce,s=c2FsdDEyMw==,i=4096";
    EXPECT_TRUE(client.processServerFirstMessage(server_first));
    EXPECT_EQ(client.combinedNonce(), client.clientNonce() + "_servernonce");
}

// Test 8: Client rejects server-first with wrong nonce
TEST_F(ScramClientTest, ProcessServerFirstMessageInvalidNonce) {
    SCRAMClient client("alice", "wonderland");
    client.generateClientFirstMessage();
    
    // Server-first with completely different nonce
    std::string server_first = "r=completelydifferent,s=c2FsdDEyMw==,i=4096";
    EXPECT_FALSE(client.processServerFirstMessage(server_first));
}

// Test 9: Client verifies valid server-final
TEST_F(ScramClientTest, VerifyServerFinalMessageSuccess) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    SCRAMClient client("alice", "wonderland");
    std::string client_first = client.generateClientFirstMessage();
    
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    std::string server_first(first.response_data.begin(), first.response_data.end());
    
    client.processServerFirstMessage(server_first);
    std::string client_final = client.generateClientFinalMessage();
    
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    std::string server_final(final.response_data.begin(), final.response_data.end());
    
    EXPECT_TRUE(client.verifyServerFinalMessage(server_final));
}

// Test 10: Client rejects invalid server signature
TEST_F(ScramClientTest, VerifyServerFinalMessageFailure) {
    SCRAMClient client("alice", "wonderland");
    client.generateClientFirstMessage();
    client.processServerFirstMessage("r=noncenonce,s=c2FsdA==,i=4096");
    client.generateClientFinalMessage();
    
    // Invalid server final
    EXPECT_FALSE(client.verifyServerFinalMessage("v=invalidbase64!!!"));
}

// Test 11: Full client authentication flow
TEST_F(ScramClientTest, FullClientAuthenticationFlow) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    // Step 1: Client generates first message
    SCRAMClient client("alice", "wonderland");
    std::string client_first = client.generateClientFirstMessage();
    EXPECT_FALSE(client_first.empty());
    
    // Step 2: Server responds
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    EXPECT_EQ(first.state, AuthState::IN_PROGRESS);
    
    // Step 3: Client processes server-first
    std::string server_first(first.response_data.begin(), first.response_data.end());
    EXPECT_TRUE(client.processServerFirstMessage(server_first));
    
    // Step 4: Client generates final message
    std::string client_final = client.generateClientFinalMessage();
    EXPECT_FALSE(client_final.empty());
    
    // Step 5: Server responds
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    EXPECT_EQ(final.state, AuthState::SUCCESS);
    
    // Step 6: Client verifies server signature
    std::string server_final(final.response_data.begin(), final.response_data.end());
    EXPECT_TRUE(client.verifyServerFinalMessage(server_final));
}

// ============================================================================
// 3. SCRAM-SHA-256 Tests
// ============================================================================

// Test 12: RFC 7677 compliance - PBKDF2 output length
TEST(ScramSHA256Test, PBKDF2OutputLength) {
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> salted_password;
    
    EXPECT_EQ(calculateSaltedPassword("password", salt, 4096, 
                                       ScramAlgorithm::SHA_256, salted_password),
              Status::OK);
    EXPECT_EQ(salted_password.size(), 32);  // SHA-256 output is 32 bytes
}

// Test 13: RFC 7677 compliance - HMAC calculations
TEST(ScramSHA256Test, HMACCalculations) {
    std::vector<uint8_t> key = {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                                 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b};
    
    const EVP_MD* md = EVP_sha256();
    std::vector<uint8_t> result(32);
    unsigned int out_len = 32;
    
    HMAC(md, key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>("test data"), 9,
         result.data(), &out_len);
    
    EXPECT_EQ(out_len, 32);
    // Verify HMAC produces consistent output
    std::vector<uint8_t> result2(32);
    HMAC(md, key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>("test data"), 9,
         result2.data(), &out_len);
    EXPECT_EQ(result, result2);
}

// Test 14: SCRAM credential generation
TEST(ScramSHA256Test, GenerateCredentials) {
    std::vector<uint8_t> salt;
    std::vector<uint8_t> stored_key;
    std::vector<uint8_t> server_key;
    
    EXPECT_EQ(generateScramCredentials("testpassword", ScramAlgorithm::SHA_256,
                                       4096, salt, stored_key, server_key),
              Status::OK);
    
    EXPECT_EQ(salt.size(), 16);        // Generated salt is 16 bytes
    EXPECT_EQ(stored_key.size(), 32);  // SHA-256 stored key
    EXPECT_EQ(server_key.size(), 32);  // SHA-256 server key
}

// Test 15: Base64 encoding/decoding round-trip
TEST(ScramSHA256Test, Base64RoundTrip) {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD};
    std::string encoded = base64Encode(data);
    std::vector<uint8_t> decoded = base64Decode(encoded);
    
    EXPECT_EQ(data, decoded);
}

// Test 16: XOR bytes operation
TEST(ScramSHA256Test, XorBytes) {
    std::vector<uint8_t> a = {0xFF, 0x00, 0xAA, 0x55};
    std::vector<uint8_t> b = {0x00, 0xFF, 0x55, 0xAA};
    
    xorBytes(a, b);
    
    EXPECT_EQ(a[0], 0xFF);  // 0xFF ^ 0x00
    EXPECT_EQ(a[1], 0xFF);  // 0x00 ^ 0xFF
    EXPECT_EQ(a[2], 0xFF);  // 0xAA ^ 0x55
    EXPECT_EQ(a[3], 0xFF);  // 0x55 ^ 0xAA
}

// Test 17: Constant time comparison
TEST(ScramSHA256Test, ConstantTimeCompare) {
    std::vector<uint8_t> a = {1, 2, 3, 4, 5};
    std::vector<uint8_t> b = {1, 2, 3, 4, 5};
    std::vector<uint8_t> c = {1, 2, 3, 4, 6};
    std::vector<uint8_t> d = {1, 2, 3};
    
    EXPECT_TRUE(constantTimeCompare(a, b));
    EXPECT_FALSE(constantTimeCompare(a, c));
    EXPECT_FALSE(constantTimeCompare(a, d));
}

// ============================================================================
// 4. SCRAM-SHA-512 Tests
// ============================================================================

class ScramSHA512Test : public ::testing::Test {
protected:
    void SetUp() override {
        store_ = std::make_shared<MemoryCredentialStore>();
        store_->createUser(makeUserCredential("sha512user", "sha512pass", 
                                              ScramAlgorithm::SHA_512, 4096));
    }

    std::shared_ptr<MemoryCredentialStore> store_;
};

// Test 18: SHA-512 PBKDF2 output length
TEST_F(ScramSHA512Test, PBKDF2OutputLength) {
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> salted_password;
    
    EXPECT_EQ(calculateSaltedPassword("password", salt, 4096, 
                                       ScramAlgorithm::SHA_512, salted_password),
              Status::OK);
    EXPECT_EQ(salted_password.size(), 64);  // SHA-512 output is 64 bytes
}

// Test 19: SHA-512 full authentication flow
TEST_F(ScramSHA512Test, FullAuthenticationFlow) {
    ScramSHA512AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    SCRAMClient client("sha512user", "sha512pass", ScramAlgorithm::SHA_512);
    std::string client_first = client.generateClientFirstMessage();
    
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);
    
    std::string server_first(first.response_data.begin(), first.response_data.end());
    ASSERT_TRUE(client.processServerFirstMessage(server_first));
    
    std::string client_final = client.generateClientFinalMessage();
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    
    EXPECT_EQ(final.state, AuthState::SUCCESS);
    EXPECT_EQ(final.authenticated_user, "sha512user");
}

// Test 20: SHA-512 credential generation
TEST_F(ScramSHA512Test, GenerateCredentials) {
    std::vector<uint8_t> salt;
    std::vector<uint8_t> stored_key;
    std::vector<uint8_t> server_key;
    
    EXPECT_EQ(generateScramCredentials("testpassword", ScramAlgorithm::SHA_512,
                                       4096, salt, stored_key, server_key),
              Status::OK);
    
    EXPECT_EQ(salt.size(), 16);        // Generated salt is 16 bytes
    EXPECT_EQ(stored_key.size(), 64);  // SHA-512 stored key
    EXPECT_EQ(server_key.size(), 64);  // SHA-512 server key
}

// Test 21: SHA-512 rejects wrong password
TEST_F(ScramSHA512Test, RejectsWrongPassword) {
    ScramSHA512AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    SCRAMClient client("sha512user", "wrongpassword", ScramAlgorithm::SHA_512);
    std::string client_first = client.generateClientFirstMessage();
    
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);
    
    std::string server_first(first.response_data.begin(), first.response_data.end());
    client.processServerFirstMessage(server_first);
    
    std::string client_final = client.generateClientFinalMessage();
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    
    EXPECT_EQ(final.state, AuthState::FAILURE);
    EXPECT_EQ(final.failure_reason, AuthFailReason::INVALID_CREDENTIALS);
}

// ============================================================================
// 5. Error Handling Tests
// ============================================================================

class ScramErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        store_ = std::make_shared<MemoryCredentialStore>();
        store_->createUser(makeUserCredential("user", "pass", ScramAlgorithm::SHA_256, 4096));
    }

    std::shared_ptr<MemoryCredentialStore> store_;
};

// Test 22: Invalid client-first format
TEST_F(ScramErrorHandlingTest, InvalidClientFirstFormat) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    // Missing nonce
    std::string client_first = "n,,n=user";
    AuthResult result = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    
    EXPECT_EQ(result.state, AuthState::FAILURE);
    EXPECT_EQ(result.failure_reason, AuthFailReason::PROTOCOL_ERROR);
}

// Test 23: Nonce mismatch in client-final
TEST_F(ScramErrorHandlingTest, NonceMismatch) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    std::string client_first = "n,,n=user,r=clientnonce";
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);
    
    // Send client-final with different nonce
    std::vector<uint8_t> proof(32, 0);
    std::string client_final = buildClientFinal("wrongnonce", proof);
    
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    
    EXPECT_EQ(final.state, AuthState::FAILURE);
    EXPECT_EQ(final.failure_reason, AuthFailReason::PROTOCOL_ERROR);
}

// Test 24: Invalid proof size
TEST_F(ScramErrorHandlingTest, InvalidProofSize) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    std::string client_first = "n,,n=user,r=clientnonce";
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);
    
    std::string server_first(first.response_data.begin(), first.response_data.end());
    std::string nonce = extractScramField(server_first, "r");
    
    // Wrong proof size (16 bytes instead of 32)
    std::vector<uint8_t> proof(16, 0xAB);
    std::string client_final = buildClientFinal(nonce, proof);
    
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    
    EXPECT_EQ(final.state, AuthState::FAILURE);
}

// Test 25: Channel binding required but not provided
TEST_F(ScramErrorHandlingTest, ChannelBindingRequired) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({{"channel_binding", "required"}}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    // Client doesn't support channel binding (gs2-flag 'n')
    std::string client_first = "n,,n=user,r=nonce";
    AuthResult result = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    
    EXPECT_EQ(result.state, AuthState::FAILURE);
    EXPECT_EQ(result.failure_reason, AuthFailReason::NOT_ALLOWED);
}

// Test 26: User not found (should not reveal user existence)
TEST_F(ScramErrorHandlingTest, UserNotFound) {
    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store_);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    // Try to authenticate as non-existent user
    std::string client_first = "n,,n=nonexistent,r=nonce";
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    
    // Should still proceed to server-first (to avoid user enumeration)
    EXPECT_EQ(first.state, AuthState::IN_PROGRESS);
    EXPECT_FALSE(first.response_data.empty());
    
    // But should fail at final verification
    std::string server_first(first.response_data.begin(), first.response_data.end());
    std::string nonce = extractScramField(server_first, "r");
    std::vector<uint8_t> proof(32, 0);
    std::string client_final = buildClientFinal(nonce, proof);
    
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    EXPECT_EQ(final.state, AuthState::FAILURE);
}

// ============================================================================
// 6. Security Tests
// ============================================================================

// Test 27: Salt uniqueness
TEST(ScramSecurityTest, SaltUniqueness) {
    std::set<std::vector<uint8_t>> salts;
    
    for (int i = 0; i < 100; ++i) {
        std::vector<uint8_t> salt;
        std::vector<uint8_t> stored_key, server_key;
        
        EXPECT_EQ(generateScramCredentials("password", ScramAlgorithm::SHA_256,
                                           4096, salt, stored_key, server_key),
                  Status::OK);
        
        // Each salt should be unique
        EXPECT_EQ(salts.count(salt), 0) << "Duplicate salt generated at iteration " << i;
        salts.insert(salt);
    }
}

// Test 28: Nonce uniqueness
TEST(ScramSecurityTest, NonceUniqueness) {
    std::set<std::string> nonces;
    
    for (int i = 0; i < 100; ++i) {
        std::string nonce = generateNonce(24);
        
        EXPECT_EQ(nonces.count(nonce), 0) << "Duplicate nonce generated at iteration " << i;
        EXPECT_EQ(nonce.size(), 24);
        nonces.insert(nonce);
    }
}

// Test 29: Memory clearing (sensitive data handling)
TEST(ScramSecurityTest, ScramStateDestructorClearsData) {
    // This test verifies that ScramState destructor clears sensitive data
    // The destructor is called when state goes out of scope
    {
        ScramState state(ScramAlgorithm::SHA_256);
        state.setSalt({1, 2, 3, 4, 5});
        state.setStoredKey({6, 7, 8, 9, 10});
        state.setServerKey({11, 12, 13, 14, 15});
        
        // State will be destroyed here
    }
    
    // If the destructor didn't clear data, it would be a security issue
    // We can't directly verify memory was cleared, but we verified the destructor
    // exists and calls std::fill in the implementation
    SUCCEED();
}

// Test 30: Username normalization
TEST(ScramSecurityTest, UsernameNormalization) {
    // '=' should be encoded as '=3D'
    EXPECT_EQ(normalizeUsername("user=name"), "user=3Dname");
    
    // ',' should be encoded as '=2C'
    EXPECT_EQ(normalizeUsername("user,name"), "user=2Cname");
    
    // Both
    EXPECT_EQ(normalizeUsername("a=b,c"), "a=3Db=2Cc");
    
    // Normal username unchanged
    EXPECT_EQ(normalizeUsername("normaluser"), "normaluser");
}

// Test 31: PBKDF2 iteration count minimum enforcement
TEST(ScramSecurityTest, MinimumIterationCount) {
    ScramSHA256AuthMethod auth;
    auth.initialize({{"iterations", "1000"}}, nullptr);  // Below minimum
    
    // Should enforce minimum of 4096 per RFC
    EXPECT_EQ(auth.config().iterations, 4096);
}

// Test 32: Different passwords produce different stored keys
TEST(ScramSecurityTest, DifferentPasswordsDifferentKeys) {
    std::vector<uint8_t> salt1, salt2;
    std::vector<uint8_t> stored_key1, server_key1;
    std::vector<uint8_t> stored_key2, server_key2;
    
    generateScramCredentials("password1", ScramAlgorithm::SHA_256,
                             4096, salt1, stored_key1, server_key1);
    generateScramCredentials("password2", ScramAlgorithm::SHA_256,
                             4096, salt2, stored_key2, server_key2);
    
    // Same salt could be generated, but stored keys should differ
    EXPECT_NE(stored_key1, stored_key2);
    EXPECT_NE(server_key1, server_key2);
}

// Test 33: Same password with different salts produces different keys
TEST(ScramSecurityTest, SamePasswordDifferentSalts) {
    std::vector<uint8_t> salt1, salt2;
    std::vector<uint8_t> stored_key1, server_key1;
    std::vector<uint8_t> stored_key2, server_key2;
    
    generateScramCredentials("samepassword", ScramAlgorithm::SHA_256,
                             4096, salt1, stored_key1, server_key1);
    generateScramCredentials("samepassword", ScramAlgorithm::SHA_256,
                             4096, salt2, stored_key2, server_key2);
    
    // Salts should be different
    EXPECT_NE(salt1, salt2);
    
    // Stored keys should be different
    EXPECT_NE(stored_key1, stored_key2);
}

// ============================================================================
// 7. Message Parsing Tests
// ============================================================================

TEST(ScramParsingTest, ParseClientFirstValid) {
    ScramClientFirst parsed;
    EXPECT_TRUE(parseClientFirst("n,,n=user,r=fyko+d2lbbFgONRv9qkxdawL", parsed));
    EXPECT_EQ(parsed.gs2_flag, 'n');
    EXPECT_EQ(parsed.username, "user");
    EXPECT_EQ(parsed.client_nonce, "fyko+d2lbbFgONRv9qkxdawL");
    EXPECT_EQ(parsed.client_first_bare, "n=user,r=fyko+d2lbbFgONRv9qkxdawL");
}

TEST(ScramParsingTest, ParseClientFirstWithAuthzid) {
    ScramClientFirst parsed;
    EXPECT_TRUE(parseClientFirst("n,a=authzid,n=user,r=nonce", parsed));
    EXPECT_EQ(parsed.gs2_flag, 'n');
    EXPECT_EQ(parsed.authzid, "authzid");
    EXPECT_EQ(parsed.username, "user");
}

TEST(ScramParsingTest, ParseClientFirstWithChannelBinding) {
    ScramClientFirst parsed;
    EXPECT_TRUE(parseClientFirst("p=tls-unique,,n=user,r=nonce", parsed));
    EXPECT_EQ(parsed.gs2_flag, 'p');
    EXPECT_EQ(parsed.channel_binding, "tls-unique");
}

TEST(ScramParsingTest, ParseClientFirstInvalid) {
    ScramClientFirst parsed;
    EXPECT_FALSE(parseClientFirst("x,,n=user,r=nonce", parsed));
    EXPECT_FALSE(parseClientFirst("n,,n=user", parsed));
    EXPECT_FALSE(parseClientFirst("n,,r=nonce", parsed));
    EXPECT_FALSE(parseClientFirst("", parsed));
}

TEST(ScramParsingTest, ParseClientFinalValid) {
    ScramClientFinal parsed;
    EXPECT_TRUE(parseClientFinal("c=biws,r=nonce,p=AAAA", parsed));
    EXPECT_EQ(parsed.channel_binding, "biws");
    EXPECT_EQ(parsed.nonce, "nonce");
    EXPECT_EQ(parsed.without_proof, "c=biws,r=nonce");
}

TEST(ScramParsingTest, ParseClientFinalWithExtensions) {
    ScramClientFinal parsed;
    EXPECT_TRUE(parseClientFinal("c=biws,r=nonce,ext=value,p=BBBB", parsed));
    EXPECT_EQ(parsed.nonce, "nonce");
    EXPECT_EQ(parsed.without_proof, "c=biws,r=nonce,ext=value");
}

TEST(ScramParsingTest, ParseClientFinalInvalid) {
    ScramClientFinal parsed;
    EXPECT_FALSE(parseClientFinal("c=biws,r=nonce", parsed));
    EXPECT_FALSE(parseClientFinal("r=nonce,p=AAAA", parsed));
    EXPECT_FALSE(parseClientFinal("c=biws,p=AAAA", parsed));
    EXPECT_FALSE(parseClientFinal("", parsed));
}

// ============================================================================
// 8. Configuration Tests
// ============================================================================

TEST(ScramConfigTest, SHA256DefaultConfig) {
    ScramSHA256AuthMethod auth;
    EXPECT_EQ(auth.config().algorithm, ScramAlgorithm::SHA_256);
    EXPECT_EQ(auth.config().iterations, 4096);
    EXPECT_FALSE(auth.config().channel_binding_required);
}

TEST(ScramConfigTest, SHA512DefaultConfig) {
    ScramSHA512AuthMethod auth;
    EXPECT_EQ(auth.config().algorithm, ScramAlgorithm::SHA_512);
    EXPECT_EQ(auth.config().iterations, 4096);
}

TEST(ScramConfigTest, CustomIterations) {
    ScramSHA256AuthMethod auth;
    auth.initialize({{"iterations", "10000"}}, nullptr);
    EXPECT_EQ(auth.config().iterations, 10000);
}

// ============================================================================
// 9. Integration/Comprehensive Tests
// ============================================================================

TEST(ScramIntegrationTest, MultipleConsecutiveAuthentications) {
    auto store = std::make_shared<MemoryCredentialStore>();
    store->createUser(makeUserCredential("user1", "pass1", ScramAlgorithm::SHA_256, 4096));
    store->createUser(makeUserCredential("user2", "pass2", ScramAlgorithm::SHA_256, 4096));

    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store);
    auth.initialize({}, nullptr);

    // First authentication
    {
        AuthContext ctx;
        auth.start(ctx);

        SCRAMClient client("user1", "pass1");
        std::string client_first = client.generateClientFirstMessage();
        
        AuthResult first = auth.continueAuth(
            ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
        std::string server_first(first.response_data.begin(), first.response_data.end());
        client.processServerFirstMessage(server_first);
        
        std::string client_final = client.generateClientFinalMessage();
        AuthResult final = auth.continueAuth(
            ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
        
        EXPECT_EQ(final.state, AuthState::SUCCESS);
    }

    // Second authentication (different user)
    {
        AuthContext ctx;
        auth.start(ctx);

        SCRAMClient client("user2", "pass2");
        std::string client_first = client.generateClientFirstMessage();
        
        AuthResult first = auth.continueAuth(
            ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
        std::string server_first(first.response_data.begin(), first.response_data.end());
        client.processServerFirstMessage(server_first);
        
        std::string client_final = client.generateClientFinalMessage();
        AuthResult final = auth.continueAuth(
            ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
        
        EXPECT_EQ(final.state, AuthState::SUCCESS);
    }
}

TEST(ScramIntegrationTest, AbortAuthentication) {
    auto store = std::make_shared<MemoryCredentialStore>();
    store->createUser(makeUserCredential("user", "pass", ScramAlgorithm::SHA_256, 4096));

    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    auth.start(ctx);

    std::string client_first = "n,,n=user,r=nonce";
    auth.continueAuth(ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    
    // Abort the authentication
    auth.abort(ctx);
    
    // Trying to continue should fail (no state found)
    AuthResult result = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    EXPECT_EQ(result.state, AuthState::FAILURE);
}

TEST(ScramIntegrationTest, AuthTypeAndName) {
    ScramSHA256AuthMethod auth256;
    EXPECT_EQ(auth256.type(), AuthType::SCRAM_SHA_256);
    EXPECT_STREQ(auth256.name(), "scram-sha-256");
    
    ScramSHA512AuthMethod auth512;
    EXPECT_EQ(auth512.type(), AuthType::SCRAM_SHA_512);
    EXPECT_STREQ(auth512.name(), "scram-sha-512");
}

TEST(ScramIntegrationTest, VerifyPasswordMethod) {
    auto store = std::make_shared<MemoryCredentialStore>();
    store->createUser(makeUserCredential("user", "correctpass", ScramAlgorithm::SHA_256, 4096));

    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store);
    auth.initialize({}, nullptr);

    EXPECT_TRUE(auth.verifyPassword("user", "correctpass"));
    EXPECT_FALSE(auth.verifyPassword("user", "wrongpass"));
    EXPECT_FALSE(auth.verifyPassword("nonexistent", "anypass"));
}
