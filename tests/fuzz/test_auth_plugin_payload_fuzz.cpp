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

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "scratchbird/security/providers/ident_provider.h"
#include "scratchbird/security/providers/kerberos_provider.h"
#include "scratchbird/security/providers/ldap_provider.h"
#include "scratchbird/security/providers/pam_provider.h"
#include "scratchbird/security/providers/radius_provider.h"

namespace {

template <typename ResultEnum>
bool isKnownResult(ResultEnum result);

template <>
bool isKnownResult(scratchbird::security::providers::LdapProviderResult result) {
    using Enum = scratchbird::security::providers::LdapProviderResult;
    switch (result) {
        case Enum::SUCCESS:
        case Enum::AUTH_LDAP_BIND_FAILED:
        case Enum::AUTH_LDAP_TIMEOUT:
        case Enum::AUTH_PLUGIN_POLICY_DENIED:
        case Enum::AUTH_LDAP_CONFIG_INVALID:
        case Enum::AUTH_LDAP_TRANSPORT_ERROR:
        case Enum::INTERNAL_ERROR:
            return true;
        default:
            return false;
    }
}

template <>
bool isKnownResult(scratchbird::security::providers::KerberosProviderResult result) {
    using Enum = scratchbird::security::providers::KerberosProviderResult;
    switch (result) {
        case Enum::SUCCESS:
        case Enum::AUTH_KERBEROS_TICKET_INVALID:
        case Enum::AUTH_KERBEROS_REPLAY_DETECTED:
        case Enum::AUTH_KERBEROS_SPN_MISMATCH:
        case Enum::AUTH_PLUGIN_POLICY_DENIED:
        case Enum::AUTH_KERBEROS_TIMEOUT:
        case Enum::INTERNAL_ERROR:
            return true;
        default:
            return false;
    }
}

template <>
bool isKnownResult(scratchbird::security::providers::IdentProviderResult result) {
    using Enum = scratchbird::security::providers::IdentProviderResult;
    switch (result) {
        case Enum::SUCCESS:
        case Enum::AUTH_IDENT_QUERY_FAILED:
        case Enum::AUTH_IDENT_UNTRUSTED_TRANSPORT:
        case Enum::AUTH_CREDENTIAL_INVALID:
        case Enum::AUTH_IDENT_TIMEOUT:
        case Enum::INTERNAL_ERROR:
            return true;
        default:
            return false;
    }
}

template <>
bool isKnownResult(scratchbird::security::providers::RadiusProviderResult result) {
    using Enum = scratchbird::security::providers::RadiusProviderResult;
    switch (result) {
        case Enum::SUCCESS:
        case Enum::AUTH_RADIUS_REJECTED:
        case Enum::AUTH_RADIUS_TIMEOUT:
        case Enum::AUTH_RADIUS_SHARED_SECRET_INVALID:
        case Enum::AUTH_PLUGIN_POLICY_DENIED:
        case Enum::INTERNAL_ERROR:
            return true;
        default:
            return false;
    }
}

template <>
bool isKnownResult(scratchbird::security::providers::PamProviderResult result) {
    using Enum = scratchbird::security::providers::PamProviderResult;
    switch (result) {
        case Enum::SUCCESS:
        case Enum::AUTH_PAM_DENIED:
        case Enum::AUTH_PAM_SERVICE_NOT_ALLOWED:
        case Enum::AUTH_PAM_CONVERSATION_TIMEOUT:
        case Enum::INTERNAL_ERROR:
            return true;
        default:
            return false;
    }
}

template <typename ResultEnum>
bool isFailClosed(ResultEnum result, ResultEnum success_value) {
    if (!isKnownResult(result)) {
        return true;
    }
    return result != success_value;
}

std::string randomAscii(std::mt19937_64& rng, std::size_t min_len, std::size_t max_len) {
    const std::size_t len =
        std::uniform_int_distribution<std::size_t>(min_len, max_len)(rng);
    std::string out;
    out.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
        const char c = static_cast<char>(
            std::uniform_int_distribution<int>('a', 'z')(rng));
        out.push_back(c);
    }
    return out;
}

}  // namespace

TEST(AuthPluginPayloadFuzzTest, MalformedPayloadsAcrossP2ProvidersReturnDeterministicCodes) {
    constexpr std::size_t kIterations = 1024;
    std::mt19937_64 rng(0x5bdb00f5u);

    auto ldap = scratchbird::security::providers::createDefaultLdapProvider();
    auto kerberos = scratchbird::security::providers::createDefaultKerberosProvider();
    auto ident = scratchbird::security::providers::createDefaultIdentProvider();
    auto radius = scratchbird::security::providers::createDefaultRadiusProvider();
    auto pam = scratchbird::security::providers::createDefaultPamProvider();

    ASSERT_TRUE(ldap);
    ASSERT_TRUE(kerberos);
    ASSERT_TRUE(ident);
    ASSERT_TRUE(radius);
    ASSERT_TRUE(pam);

    for (std::size_t i = 0; i < kIterations; ++i) {
        scratchbird::security::providers::LdapAuthRequest req;
        req.username = randomAscii(rng, 0, 12);
        req.password = (i % 7 == 0) ? "__bind_fail__" : randomAscii(rng, 0, 12);
        req.ldap_uri = (i % 3 == 0) ? "" : ((i % 2 == 0) ? "ldap://blocked.local" : "ldaps://allowed.local");
        req.require_starttls = (i % 2 == 0);
        req.allowed_ldap_endpoints = {"ldaps://allowed.local"};
        const auto response = ldap->authenticate(req);
        EXPECT_NE(response.result, scratchbird::security::providers::LdapProviderResult::INTERNAL_ERROR);
        EXPECT_TRUE(isKnownResult(response.result));
    }

    for (std::size_t i = 0; i < kIterations; ++i) {
        scratchbird::security::providers::KerberosAuthRequest req;
        req.username = randomAscii(rng, 0, 10);
        req.ticket_b64 = (i % 5 == 0) ? "__invalid__" : randomAscii(rng, 0, 16);
        req.service_principal = (i % 3 == 0) ? "" : "scratchbird/db.local";
        req.keytab_path = (i % 4 == 0) ? "" : "/tmp/keytab";
        req.kdc_endpoint = (i % 2 == 0) ? "kdc.allowed.local" : "kdc.blocked.local";
        req.allowed_kdc_endpoints = {"kdc.allowed.local"};
        const auto response = kerberos->authenticate(req);
        EXPECT_NE(response.result, scratchbird::security::providers::KerberosProviderResult::INTERNAL_ERROR);
        EXPECT_TRUE(isKnownResult(response.result));
    }

    for (std::size_t i = 0; i < kIterations; ++i) {
        scratchbird::security::providers::IdentAuthRequest req;
        req.username = (i % 4 == 0) ? "" : randomAscii(rng, 1, 12);
        req.transport_remote_address =
            (i % 3 == 0) ? "203.0.113.11" : "127.0.0.1";
        req.trusted_cidrs = {"127.", "::1"};
        req.require_username_match = true;
        const auto response = ident->authenticate(req);
        EXPECT_NE(response.result, scratchbird::security::providers::IdentProviderResult::INTERNAL_ERROR);
        EXPECT_TRUE(isKnownResult(response.result));
    }

    for (std::size_t i = 0; i < kIterations; ++i) {
        scratchbird::security::providers::RadiusAuthRequest req;
        req.username = randomAscii(rng, 1, 12);
        req.password = (i % 6 == 0) ? "__reject__" : randomAscii(rng, 1, 12);
        req.radius_servers = (i % 2 == 0) ? std::vector<std::string>{"radius.allowed.local"}
                                          : std::vector<std::string>{"radius.blocked.local"};
        req.allowed_radius_endpoints = {"radius.allowed.local"};
        req.shared_secret_ref = (i % 5 == 0) ? "" : "vault://radius/shared";
        const auto response = radius->authenticate(req);
        EXPECT_NE(response.result, scratchbird::security::providers::RadiusProviderResult::INTERNAL_ERROR);
        EXPECT_TRUE(isKnownResult(response.result));
    }

    for (std::size_t i = 0; i < kIterations; ++i) {
        scratchbird::security::providers::PamAuthRequest req;
        req.username = randomAscii(rng, 1, 10);
        req.password = (i % 4 == 0) ? "__deny__" : randomAscii(rng, 1, 12);
        req.service_name = (i % 3 == 0) ? "" : ((i % 2 == 0) ? "scratchbird" : "other");
        req.allowed_modules = {"scratchbird"};
        const auto response = pam->authenticate(req);
        EXPECT_NE(response.result, scratchbird::security::providers::PamProviderResult::INTERNAL_ERROR);
        EXPECT_TRUE(isKnownResult(response.result));
    }
}

TEST(AuthPluginPayloadFuzzTest, TruncatedChallengePayloadsFailClosed) {
    auto ldap = scratchbird::security::providers::createDefaultLdapProvider();
    auto kerberos = scratchbird::security::providers::createDefaultKerberosProvider();
    auto ident = scratchbird::security::providers::createDefaultIdentProvider();
    auto radius = scratchbird::security::providers::createDefaultRadiusProvider();
    auto pam = scratchbird::security::providers::createDefaultPamProvider();

    ASSERT_TRUE(ldap);
    ASSERT_TRUE(kerberos);
    ASSERT_TRUE(ident);
    ASSERT_TRUE(radius);
    ASSERT_TRUE(pam);

    const std::vector<std::string> truncated_tokens = {"", "A", "AA", "AQ", "AAAA"};

    for (const auto& token : truncated_tokens) {
        scratchbird::security::providers::LdapAuthRequest ldap_req;
        ldap_req.username = "alice";
        ldap_req.password = token;
        ldap_req.ldap_uri = "";
        ldap_req.require_starttls = true;
        const auto ldap_rsp = ldap->authenticate(ldap_req);
        EXPECT_NE(ldap_rsp.result, scratchbird::security::providers::LdapProviderResult::SUCCESS);

        scratchbird::security::providers::KerberosAuthRequest krb_req;
        krb_req.username = "alice";
        krb_req.ticket_b64 = token;
        krb_req.service_principal = "";
        krb_req.keytab_path = "";
        const auto krb_rsp = kerberos->authenticate(krb_req);
        EXPECT_NE(krb_rsp.result, scratchbird::security::providers::KerberosProviderResult::SUCCESS);

        scratchbird::security::providers::IdentAuthRequest ident_req;
        ident_req.username = token;
        ident_req.transport_remote_address = "127.0.0.1";
        ident_req.trusted_cidrs = {"127."};
        ident_req.require_username_match = true;
        const auto ident_rsp = ident->authenticate(ident_req);
        if (token.empty()) {
            EXPECT_NE(ident_rsp.result, scratchbird::security::providers::IdentProviderResult::SUCCESS);
        }

        scratchbird::security::providers::RadiusAuthRequest radius_req;
        radius_req.username = "alice";
        radius_req.password = token;
        radius_req.radius_servers = {"radius.allowed.local"};
        radius_req.allowed_radius_endpoints = {"radius.allowed.local"};
        radius_req.shared_secret_ref.clear();
        const auto radius_rsp = radius->authenticate(radius_req);
        EXPECT_NE(radius_rsp.result, scratchbird::security::providers::RadiusProviderResult::SUCCESS);

        scratchbird::security::providers::PamAuthRequest pam_req;
        pam_req.username = "alice";
        pam_req.password = token;
        pam_req.service_name.clear();
        pam_req.allowed_modules = {"scratchbird"};
        const auto pam_rsp = pam->authenticate(pam_req);
        EXPECT_NE(pam_rsp.result, scratchbird::security::providers::PamProviderResult::SUCCESS);
    }
}

TEST(AuthPluginPayloadFuzzTest, UnknownProviderResultEnumsClassifyAsFailClosed) {
    using scratchbird::security::providers::IdentProviderResult;
    using scratchbird::security::providers::KerberosProviderResult;
    using scratchbird::security::providers::LdapProviderResult;
    using scratchbird::security::providers::PamProviderResult;
    using scratchbird::security::providers::RadiusProviderResult;

    EXPECT_TRUE(isFailClosed(static_cast<LdapProviderResult>(255), LdapProviderResult::SUCCESS));
    EXPECT_TRUE(
        isFailClosed(static_cast<KerberosProviderResult>(255), KerberosProviderResult::SUCCESS));
    EXPECT_TRUE(isFailClosed(static_cast<IdentProviderResult>(255), IdentProviderResult::SUCCESS));
    EXPECT_TRUE(
        isFailClosed(static_cast<RadiusProviderResult>(255), RadiusProviderResult::SUCCESS));
    EXPECT_TRUE(isFailClosed(static_cast<PamProviderResult>(255), PamProviderResult::SUCCESS));
}
