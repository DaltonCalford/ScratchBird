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

#include <array>
#include <vector>

#include "scratchbird/protocol/wire_protocol.h"

TEST(AuthPluginRegistryNegotiationTest, AuthChallengeRoundTripsRegistryEntries) {
    std::array<uint8_t, 16> session_id{};
    for (std::size_t i = 0; i < session_id.size(); ++i) {
        session_id[i] = static_cast<uint8_t>(i + 1);
    }

    const std::vector<scratchbird::protocol::AuthMethod> allowed = {
        scratchbird::protocol::AuthMethod::SCRAM_SHA_256,
        scratchbird::protocol::AuthMethod::TOKEN,
    };
    constexpr uint8_t kTransportMaskLocal = 0x01u;
    const std::vector<uint8_t> nonce = {0xAA, 0xBB, 0xCC, 0xDD};
    const std::vector<scratchbird::protocol::AuthMethodRegistryEntry> registry = {
        {1, "scratchbird.auth.scram_sha_256", true,
         static_cast<uint32_t>(scratchbird::protocol::AuthMethod::SCRAM_SHA_256)},
        {2, "scratchbird.auth.authkey_token", true,
         static_cast<uint32_t>(scratchbird::protocol::AuthMethod::TOKEN)},
    };

    scratchbird::protocol::Message msg = scratchbird::protocol::ProtocolCodec::buildAuthChallenge(
        session_id.data(),
        "alice",
        allowed,
        true,
        scratchbird::protocol::AuthMethod::SCRAM_SHA_256,
        kTransportMaskLocal,
        nonce,
        &registry);

    uint8_t parsed_session_id[16] = {};
    std::string parsed_username;
    std::vector<scratchbird::protocol::AuthMethod> parsed_allowed;
    bool parsed_has_required = false;
    scratchbird::protocol::AuthMethod parsed_required = scratchbird::protocol::AuthMethod::PASSWORD;
    uint8_t parsed_transport_mask = 0;
    std::vector<uint8_t> parsed_nonce;
    std::vector<scratchbird::protocol::AuthMethodRegistryEntry> parsed_registry;
    scratchbird::core::ErrorContext ctx;

    ASSERT_EQ(scratchbird::protocol::ProtocolCodec::parseAuthChallenge(
                  msg,
                  parsed_session_id,
                  parsed_username,
                  parsed_allowed,
                  parsed_has_required,
                  parsed_required,
                  parsed_transport_mask,
                  parsed_nonce,
                  &ctx,
                  &parsed_registry),
              scratchbird::core::Status::OK)
        << ctx.message;

    EXPECT_EQ(parsed_username, "alice");
    EXPECT_EQ(parsed_allowed, allowed);
    EXPECT_TRUE(parsed_has_required);
    EXPECT_EQ(parsed_required, scratchbird::protocol::AuthMethod::SCRAM_SHA_256);
    EXPECT_EQ(parsed_transport_mask, kTransportMaskLocal);
    EXPECT_EQ(parsed_nonce, nonce);
    ASSERT_EQ(parsed_registry.size(), registry.size());
    for (std::size_t i = 0; i < registry.size(); ++i) {
        EXPECT_EQ(parsed_registry[i].method_slot, registry[i].method_slot);
        EXPECT_EQ(parsed_registry[i].method_id, registry[i].method_id);
        EXPECT_EQ(parsed_registry[i].has_legacy_wire_code, registry[i].has_legacy_wire_code);
        EXPECT_EQ(parsed_registry[i].legacy_wire_code, registry[i].legacy_wire_code);
    }
}
