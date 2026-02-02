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

#include <map>
#include <string>
#include <vector>

#include "scratchbird/security/auth_manager.h"
#include "scratchbird/security/scram_auth.h"

using scratchbird::core::Status;
using scratchbird::security::AuthContext;
using scratchbird::security::AuthFailReason;
using scratchbird::security::AuthResult;
using scratchbird::security::AuthState;
using scratchbird::security::CredentialStore;
using scratchbird::security::ScramAlgorithm;
using scratchbird::security::ScramClientFinal;
using scratchbird::security::ScramClientFirst;
using scratchbird::security::ScramSHA256AuthMethod;
using scratchbird::security::ScramSHA512AuthMethod;
using scratchbird::security::UserCredential;
using scratchbird::security::base64Encode;
using scratchbird::security::generateScramCredentials;
using scratchbird::security::parseClientFinal;
using scratchbird::security::parseClientFirst;

namespace {

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

}  // namespace

TEST(ScramParsingTest, ParseClientFirstValid) {
    ScramClientFirst parsed;
    EXPECT_TRUE(parseClientFirst("n,,n=user,r=fyko+d2lbbFgONRv9qkxdawL", parsed));
    EXPECT_EQ(parsed.gs2_flag, 'n');
    EXPECT_EQ(parsed.username, "user");
    EXPECT_EQ(parsed.client_nonce, "fyko+d2lbbFgONRv9qkxdawL");
    EXPECT_EQ(parsed.client_first_bare, "n=user,r=fyko+d2lbbFgONRv9qkxdawL");
}

TEST(ScramParsingTest, ParseClientFirstInvalid) {
    ScramClientFirst parsed;
    EXPECT_FALSE(parseClientFirst("x,,n=user,r=nonce", parsed));
    EXPECT_FALSE(parseClientFirst("n,,n=user", parsed));
    EXPECT_FALSE(parseClientFirst("n,,r=nonce", parsed));
}

TEST(ScramParsingTest, ParseClientFinalValid) {
    ScramClientFinal parsed;
    EXPECT_TRUE(parseClientFinal("c=biws,r=nonce,p=AAAA", parsed));
    EXPECT_EQ(parsed.channel_binding, "biws");
    EXPECT_EQ(parsed.nonce, "nonce");
    EXPECT_EQ(parsed.without_proof, "c=biws,r=nonce");
}

TEST(ScramParsingTest, ParseClientFinalInvalid) {
    ScramClientFinal parsed;
    EXPECT_FALSE(parseClientFinal("c=biws,r=nonce", parsed));
    EXPECT_FALSE(parseClientFinal("r=nonce,p=AAAA", parsed));
    EXPECT_FALSE(parseClientFinal("c=biws,p=AAAA", parsed));
}

TEST(ScramAuthTest, InvalidClientFirstIsProtocolError) {
    auto store = std::make_shared<MemoryCredentialStore>();
    store->createUser(makeUserCredential("user", "secret", ScramAlgorithm::SHA_256, 4096));

    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    AuthResult start = auth.start(ctx);
    ASSERT_EQ(start.state, AuthState::IN_PROGRESS);

    std::vector<uint8_t> bad_message{'n', ',', ',', 'n', '=', 'u'};
    AuthResult result = auth.continueAuth(ctx, bad_message);
    EXPECT_EQ(result.state, AuthState::FAILURE);
    EXPECT_EQ(result.failure_reason, AuthFailReason::PROTOCOL_ERROR);
}

TEST(ScramAuthTest, ChannelBindingRequiredRejectsNoBinding) {
    auto store = std::make_shared<MemoryCredentialStore>();
    store->createUser(makeUserCredential("user", "secret", ScramAlgorithm::SHA_256, 4096));

    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store);
    auth.initialize({{"channel_binding", "required"}}, nullptr);

    AuthContext ctx;
    AuthResult start = auth.start(ctx);
    ASSERT_EQ(start.state, AuthState::IN_PROGRESS);

    std::string client_first = "n,,n=user,r=nonce";
    AuthResult result = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    EXPECT_EQ(result.state, AuthState::FAILURE);
    EXPECT_EQ(result.failure_reason, AuthFailReason::NOT_ALLOWED);
}

TEST(ScramAuthTest, NonceMismatchIsProtocolError) {
    auto store = std::make_shared<MemoryCredentialStore>();
    store->createUser(makeUserCredential("user", "secret", ScramAlgorithm::SHA_256, 4096));

    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    AuthResult start = auth.start(ctx);
    ASSERT_EQ(start.state, AuthState::IN_PROGRESS);

    std::string client_first = "n,,n=user,r=clientnonce";
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);
    std::string server_first(first.response_data.begin(), first.response_data.end());

    std::vector<uint8_t> proof(32, 0);
    std::string client_final = buildClientFinal("badnonce", proof);
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    EXPECT_EQ(final.state, AuthState::FAILURE);
    EXPECT_EQ(final.failure_reason, AuthFailReason::PROTOCOL_ERROR);
    EXPECT_NE(extractScramField(server_first, "r"), "");
}

TEST(ScramAuthTest, InvalidProofFailsAuthentication) {
    auto store = std::make_shared<MemoryCredentialStore>();
    store->createUser(makeUserCredential("user", "secret", ScramAlgorithm::SHA_256, 4096));

    ScramSHA256AuthMethod auth;
    auth.setCredentialStore(store);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    AuthResult start = auth.start(ctx);
    ASSERT_EQ(start.state, AuthState::IN_PROGRESS);

    std::string client_first = "n,,n=user,r=clientnonce";
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);

    std::string server_first(first.response_data.begin(), first.response_data.end());
    std::string nonce = extractScramField(server_first, "r");
    ASSERT_FALSE(nonce.empty());

    std::vector<uint8_t> proof(32, 0);
    std::string client_final = buildClientFinal(nonce, proof);
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    EXPECT_EQ(final.state, AuthState::FAILURE);
    EXPECT_EQ(final.failure_reason, AuthFailReason::INVALID_CREDENTIALS);
}

TEST(ScramAuthTest, InvalidProofFailsAuthenticationSha512) {
    auto store = std::make_shared<MemoryCredentialStore>();
    store->createUser(makeUserCredential("user", "secret", ScramAlgorithm::SHA_512, 4096));

    ScramSHA512AuthMethod auth;
    auth.setCredentialStore(store);
    auth.initialize({}, nullptr);

    AuthContext ctx;
    AuthResult start = auth.start(ctx);
    ASSERT_EQ(start.state, AuthState::IN_PROGRESS);

    std::string client_first = "n,,n=user,r=clientnonce";
    AuthResult first = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_first.begin(), client_first.end()));
    ASSERT_EQ(first.state, AuthState::IN_PROGRESS);

    std::string server_first(first.response_data.begin(), first.response_data.end());
    std::string nonce = extractScramField(server_first, "r");
    ASSERT_FALSE(nonce.empty());

    std::vector<uint8_t> proof(64, 0);
    std::string client_final = buildClientFinal(nonce, proof);
    AuthResult final = auth.continueAuth(
        ctx, std::vector<uint8_t>(client_final.begin(), client_final.end()));
    EXPECT_EQ(final.state, AuthState::FAILURE);
    EXPECT_EQ(final.failure_reason, AuthFailReason::INVALID_CREDENTIALS);
}
