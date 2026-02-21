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

#include <cstring>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/network/control_plane.h"

using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::network::DatabaseBindingKeyRing;
using scratchbird::network::DatabaseBindingReplayCache;
using scratchbird::network::DatabaseBindingToken;
using scratchbird::network::DatabaseBindingValidationOptions;

namespace {

std::vector<uint8_t> makeKeyA() {
    std::vector<uint8_t> key(32);
    for (size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<uint8_t>(0x10 + i);
    }
    return key;
}

std::vector<uint8_t> makeKeyB() {
    std::vector<uint8_t> key(32);
    for (size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<uint8_t>(0x80 + i);
    }
    return key;
}

DatabaseBindingToken makeToken(uint32_t listener_id, uint64_t issued_at_ms, uint64_t expires_at_ms) {
    DatabaseBindingToken token;
    token.listener_id = listener_id;
    token.issued_at_ms = issued_at_ms;
    token.expires_at_ms = expires_at_ms;
    token.client_nonce = {0x01, 0x02, 0x03, 0x04};
    token.server_nonce = {0xA1, 0xA2, 0xA3, 0xA4};
    token.flags = 0x00000011u;
    for (size_t i = 0; i < token.db_uuid.size(); ++i) {
        token.db_uuid[i] = static_cast<uint8_t>(i);
        token.manager_session_id[i] = static_cast<uint8_t>(0xF0 + i);
    }
    return token;
}

}  // namespace

TEST(ControlPlaneDbbtTest, EncodeDecodeRoundTripPreservesFields) {
    DatabaseBindingToken token = makeToken(77, 1000, 9000);
    token.mac = std::vector<uint8_t>(32, 0xAA);

    std::vector<uint8_t> encoded;
    ErrorContext ctx;
    ASSERT_TRUE(scratchbird::network::encodeDatabaseBindingToken(token, encoded, &ctx))
        << ctx.message;

    DatabaseBindingToken decoded;
    ASSERT_TRUE(scratchbird::network::decodeDatabaseBindingToken(
        encoded.data(), encoded.size(), decoded, &ctx)) << ctx.message;

    EXPECT_EQ(decoded.version, token.version);
    EXPECT_EQ(decoded.listener_id, token.listener_id);
    EXPECT_EQ(decoded.issued_at_ms, token.issued_at_ms);
    EXPECT_EQ(decoded.expires_at_ms, token.expires_at_ms);
    EXPECT_EQ(decoded.db_uuid, token.db_uuid);
    EXPECT_EQ(decoded.manager_session_id, token.manager_session_id);
    EXPECT_EQ(decoded.client_nonce, token.client_nonce);
    EXPECT_EQ(decoded.server_nonce, token.server_nonce);
    EXPECT_EQ(decoded.flags, token.flags);
    EXPECT_EQ(decoded.mac, token.mac);
}

TEST(ControlPlaneDbbtTest, KeyRingSignAndVerifyWorksAcrossCodecBoundary) {
    DatabaseBindingKeyRing key_ring;
    ErrorContext ctx;
    ASSERT_TRUE(key_ring.addKey("k1", makeKeyA(), true, &ctx)) << ctx.message;

    DatabaseBindingToken token = makeToken(91, 2000, 8000);
    ASSERT_EQ(key_ring.sign(token, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(token.mac.empty());

    std::vector<uint8_t> encoded;
    ASSERT_TRUE(scratchbird::network::encodeDatabaseBindingToken(token, encoded, &ctx))
        << ctx.message;
    DatabaseBindingToken decoded;
    ASSERT_TRUE(scratchbird::network::decodeDatabaseBindingToken(
        encoded.data(), encoded.size(), decoded, &ctx)) << ctx.message;

    ASSERT_EQ(key_ring.verify(decoded, nullptr, &ctx), Status::OK) << ctx.message;

    DatabaseBindingKeyRing wrong_ring;
    ASSERT_TRUE(wrong_ring.addKey("k2", makeKeyB(), true, &ctx)) << ctx.message;
    EXPECT_NE(wrong_ring.verify(decoded, nullptr, &ctx), Status::OK);
}

TEST(ControlPlaneDbbtTest, ValidationRejectsReplayAndExpiry) {
    const uint64_t now_ms = 50'000;
    DatabaseBindingKeyRing key_ring;
    ErrorContext ctx;
    ASSERT_TRUE(key_ring.addKey("k1", makeKeyA(), true, &ctx)) << ctx.message;

    DatabaseBindingToken token = makeToken(501, now_ms - 1000, now_ms + 5000);
    ASSERT_EQ(key_ring.sign(token, &ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> encoded;
    ASSERT_TRUE(scratchbird::network::encodeDatabaseBindingToken(token, encoded, &ctx))
        << ctx.message;

    DatabaseBindingReplayCache replay_cache(8);
    DatabaseBindingValidationOptions opts;
    opts.expected_listener_id = 501;
    opts.now_ms = now_ms;
    opts.clock_skew_ms = 100;
    opts.enforce_replay = true;

    EXPECT_EQ(scratchbird::network::validateDatabaseBindingToken(
                  encoded, key_ring, opts, &replay_cache, nullptr, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_NE(scratchbird::network::validateDatabaseBindingToken(
                  encoded, key_ring, opts, &replay_cache, nullptr, &ctx),
              Status::OK);

    DatabaseBindingToken expired = makeToken(501, now_ms - 1000, now_ms - 500);
    ASSERT_EQ(key_ring.sign(expired, &ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(scratchbird::network::encodeDatabaseBindingToken(expired, encoded, &ctx))
        << ctx.message;
    EXPECT_NE(scratchbird::network::validateDatabaseBindingToken(
                  encoded, key_ring, opts, nullptr, nullptr, &ctx),
              Status::OK);
}

TEST(ControlPlaneDbbtTest, ValidationRejectsListenerIdMismatch) {
    const uint64_t now_ms = 90'000;
    DatabaseBindingKeyRing key_ring;
    ErrorContext ctx;
    ASSERT_TRUE(key_ring.addKey("k1", makeKeyA(), true, &ctx)) << ctx.message;

    DatabaseBindingToken token = makeToken(910, now_ms - 100, now_ms + 5000);
    ASSERT_EQ(key_ring.sign(token, &ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> encoded;
    ASSERT_TRUE(scratchbird::network::encodeDatabaseBindingToken(token, encoded, &ctx))
        << ctx.message;

    DatabaseBindingValidationOptions opts;
    opts.expected_listener_id = 911;
    opts.now_ms = now_ms;
    opts.clock_skew_ms = 100;
    opts.enforce_replay = false;

    EXPECT_NE(scratchbird::network::validateDatabaseBindingToken(
                  encoded, key_ring, opts, nullptr, nullptr, &ctx),
              Status::OK);
}

TEST(ControlPlaneDbbtTest, ValidationRejectsForgedTokenPayload) {
    const uint64_t now_ms = 95'000;
    DatabaseBindingKeyRing key_ring;
    ErrorContext ctx;
    ASSERT_TRUE(key_ring.addKey("k1", makeKeyA(), true, &ctx)) << ctx.message;

    DatabaseBindingToken token = makeToken(920, now_ms - 100, now_ms + 5000);
    ASSERT_EQ(key_ring.sign(token, &ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> encoded;
    ASSERT_TRUE(scratchbird::network::encodeDatabaseBindingToken(token, encoded, &ctx))
        << ctx.message;
    ASSERT_GT(encoded.size(), 32U);

    // Flip one byte in the signed payload while leaving MAC untouched.
    encoded[12] ^= 0x5A;

    DatabaseBindingValidationOptions opts;
    opts.expected_listener_id = 920;
    opts.now_ms = now_ms;
    opts.clock_skew_ms = 100;
    opts.enforce_replay = false;

    EXPECT_NE(scratchbird::network::validateDatabaseBindingToken(
                  encoded, key_ring, opts, nullptr, nullptr, &ctx),
              Status::OK);
}

TEST(ControlPlaneDbbtTest, KeyRingLoadsFromTextWithActiveKey) {
    const std::string keyring_text =
        "alpha=hex:00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\n"
        "*beta=hex:ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100\n";

    DatabaseBindingKeyRing key_ring;
    ErrorContext ctx;
    ASSERT_EQ(DatabaseBindingKeyRing::loadFromText(keyring_text, key_ring, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(key_ring.size(), 2U);
    EXPECT_EQ(key_ring.activeKeyId(), "beta");
}

TEST(ControlPlaneDbbtTest, ListenerPrefaceRoundTripAndValidation) {
    const uint64_t now_ms = 70'000;
    DatabaseBindingKeyRing key_ring;
    ErrorContext ctx;
    ASSERT_TRUE(key_ring.addKey("k1", makeKeyA(), true, &ctx)) << ctx.message;

    DatabaseBindingToken token = makeToken(700, now_ms - 200, now_ms + 5000);
    ASSERT_EQ(key_ring.sign(token, &ctx), Status::OK) << ctx.message;
    std::vector<uint8_t> encoded_token;
    ASSERT_TRUE(scratchbird::network::encodeDatabaseBindingToken(token, encoded_token, &ctx))
        << ctx.message;

    scratchbird::network::ListenerPrefaceV1 preface;
    preface.listener_id = 700;
    preface.dbbt = encoded_token;
    preface.db_selector = "main";
    preface.requested_profile = "native_v3";
    preface.flags = 0x10;

    std::vector<uint8_t> encoded_preface;
    ASSERT_TRUE(scratchbird::network::encodeListenerPrefaceV1(preface, encoded_preface, &ctx))
        << ctx.message;

    scratchbird::network::ListenerPrefaceV1 decoded_preface;
    ASSERT_TRUE(scratchbird::network::decodeListenerPrefaceV1(
        encoded_preface.data(), encoded_preface.size(), decoded_preface, &ctx))
        << ctx.message;
    EXPECT_EQ(decoded_preface.listener_id, preface.listener_id);
    EXPECT_EQ(decoded_preface.db_selector, preface.db_selector);
    EXPECT_EQ(decoded_preface.requested_profile, preface.requested_profile);
    EXPECT_EQ(decoded_preface.flags, preface.flags);
    EXPECT_EQ(decoded_preface.dbbt, preface.dbbt);

    DatabaseBindingReplayCache replay_cache(8);
    DatabaseBindingValidationOptions opts;
    opts.expected_listener_id = 700;
    opts.now_ms = now_ms;
    opts.clock_skew_ms = 100;
    opts.enforce_replay = true;

    scratchbird::network::ListenerPrefaceV1 validated_preface;
    DatabaseBindingToken validated_token;
    EXPECT_EQ(scratchbird::network::validateListenerPrefaceV1(
                  encoded_preface,
                  key_ring,
                  opts,
                  &replay_cache,
                  &validated_preface,
                  &validated_token,
                  &ctx),
              Status::OK) << ctx.message;
    EXPECT_NE(scratchbird::network::validateListenerPrefaceV1(
                  encoded_preface,
                  key_ring,
                  opts,
                  &replay_cache,
                  nullptr,
                  nullptr,
                  &ctx),
              Status::OK);
}

TEST(ControlPlaneDbbtTest, ListenerPrefaceAckRoundTrip) {
    scratchbird::network::ListenerPrefaceAck ack;
    ack.accepted = false;
    ack.nack_code = scratchbird::network::ListenerPrefaceNackCode::INVALID_DBBT;
    ack.message = "invalid_dbbt";

    std::vector<uint8_t> encoded;
    ErrorContext ctx;
    ASSERT_TRUE(scratchbird::network::encodeListenerPrefaceAck(ack, encoded, &ctx))
        << ctx.message;

    scratchbird::network::ListenerPrefaceAck decoded;
    ASSERT_TRUE(scratchbird::network::decodeListenerPrefaceAck(
        encoded.data(), encoded.size(), decoded, &ctx)) << ctx.message;
    EXPECT_EQ(decoded.accepted, ack.accepted);
    EXPECT_EQ(decoded.nack_code, ack.nack_code);
    EXPECT_EQ(decoded.message, ack.message);
}
