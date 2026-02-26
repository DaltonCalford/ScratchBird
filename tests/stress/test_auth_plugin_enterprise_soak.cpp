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

#ifdef __linux__
#include <unistd.h>
#endif

#include <fstream>

#include "scratchbird/security/providers/ident_provider.h"
#include "scratchbird/security/providers/kerberos_provider.h"
#include "scratchbird/security/providers/ldap_provider.h"
#include "scratchbird/security/providers/pam_provider.h"
#include "scratchbird/security/providers/radius_provider.h"

namespace {

std::uint64_t sampleResidentBytes() {
#ifdef __linux__
    std::ifstream statm("/proc/self/statm");
    std::uint64_t pages_total = 0;
    std::uint64_t pages_resident = 0;
    if (!(statm >> pages_total >> pages_resident)) {
        return 0;
    }
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return 0;
    }
    return pages_resident * static_cast<std::uint64_t>(page_size);
#else
    return 0;
#endif
}

}  // namespace

TEST(AuthPluginEnterpriseSoakTest, MixedMethodsRandomDisconnectsRemainStable) {
    constexpr std::size_t kSimulatedSeconds = 4u * 60u * 60u;
    constexpr std::size_t kIterations = kSimulatedSeconds;
    constexpr long long kMaxAllowedRssDeltaBytes = 2LL * 1024LL * 1024LL;

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

    std::mt19937_64 rng(20260226ULL);
    std::uniform_int_distribution<int> method_pick(0, 4);
    std::bernoulli_distribution disconnect_pick(0.07);
    std::bernoulli_distribution fault_pick(0.12);

    std::size_t disconnect_events = 0;
    std::size_t reconnect_events = 0;
    std::size_t success_count = 0;
    std::size_t deny_count = 0;
    std::size_t unexpected = 0;
    bool connected = true;

    const std::uint64_t rss_start = sampleResidentBytes();

    for (std::size_t i = 0; i < kIterations; ++i) {
        if (disconnect_pick(rng)) {
            connected = false;
            ++disconnect_events;
            continue;
        }
        if (!connected) {
            connected = true;
            ++reconnect_events;
        }

        const bool inject_fault = fault_pick(rng);
        const int method = method_pick(rng);
        bool ok = false;

        switch (method) {
            case 0: {
                scratchbird::security::providers::LdapAuthRequest req;
                req.username = "alice";
                req.password = inject_fault ? "__bind_fail__" : "secret";
                req.ldap_uri = inject_fault ? "ldap://blocked.local" : "ldaps://corp-ldap-1.local";
                req.require_starttls = true;
                req.allowed_ldap_endpoints = {"ldaps://corp-ldap-1.local"};
                const auto rsp = ldap->authenticate(req);
                ok = rsp.result == scratchbird::security::providers::LdapProviderResult::SUCCESS;
                break;
            }
            case 1: {
                scratchbird::security::providers::KerberosAuthRequest req;
                req.username = "alice";
                req.ticket_b64 = inject_fault ? "__replay__" : "ticket";
                req.service_principal = "scratchbird/db.local";
                req.keytab_path = "/tmp/keytab";
                req.kdc_endpoint = "kdc-1.local";
                req.allowed_kdc_endpoints = {"kdc-1.local"};
                const auto rsp = kerberos->authenticate(req);
                ok = rsp.result ==
                     scratchbird::security::providers::KerberosProviderResult::SUCCESS;
                break;
            }
            case 2: {
                scratchbird::security::providers::IdentAuthRequest req;
                req.username = "alice";
                req.transport_remote_address = inject_fault ? "203.0.113.9" : "127.0.0.1";
                req.trusted_cidrs = {"127.", "::1"};
                req.require_username_match = true;
                const auto rsp = ident->authenticate(req);
                ok = rsp.result == scratchbird::security::providers::IdentProviderResult::SUCCESS;
                break;
            }
            case 3: {
                scratchbird::security::providers::RadiusAuthRequest req;
                req.username = "alice";
                req.password = inject_fault ? "__timeout__" : "secret";
                req.radius_servers = {"radius-1.local"};
                req.shared_secret_ref = "vault://radius/shared";
                req.request_timeout_ms = inject_fault ? 0 : 2000;
                req.allowed_radius_endpoints = {"radius-1.local"};
                const auto rsp = radius->authenticate(req);
                ok = rsp.result == scratchbird::security::providers::RadiusProviderResult::SUCCESS;
                break;
            }
            default: {
                scratchbird::security::providers::PamAuthRequest req;
                req.username = "alice";
                req.password = inject_fault ? "__deny__" : "secret";
                req.service_name = inject_fault ? "other" : "scratchbird";
                req.allowed_modules = {"scratchbird"};
                req.conversation_timeout_ms = 2000;
                const auto rsp = pam->authenticate(req);
                ok = rsp.result == scratchbird::security::providers::PamProviderResult::SUCCESS;
                break;
            }
        }

        if (inject_fault) {
            if (ok) {
                ++unexpected;
            } else {
                ++deny_count;
            }
        } else {
            if (ok) {
                ++success_count;
            } else {
                ++unexpected;
            }
        }
    }

    const std::uint64_t rss_end = sampleResidentBytes();
    const long long rss_delta =
        (rss_start > 0 && rss_end > 0)
            ? static_cast<long long>(rss_end) - static_cast<long long>(rss_start)
            : 0;

    std::cout << "[Soak][AuthPluginEnterprise]"
              << " simulated_seconds=" << kSimulatedSeconds
              << " iterations=" << kIterations
              << " disconnect_events=" << disconnect_events
              << " reconnect_events=" << reconnect_events
              << " success_count=" << success_count
              << " deny_count=" << deny_count
              << " unexpected=" << unexpected
              << " rss_delta_bytes=" << rss_delta
              << std::endl;

    EXPECT_EQ(unexpected, 0u);
    EXPECT_GT(success_count, 0u);
    EXPECT_GT(deny_count, 0u);
    if (rss_start > 0 && rss_end > 0) {
        EXPECT_LE(rss_delta, kMaxAllowedRssDeltaBytes);
    }
}
