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

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

#ifdef __linux__
#include <unistd.h>
#endif

#include "scratchbird/security/providers/ident_provider.h"
#include "scratchbird/security/providers/kerberos_provider.h"
#include "scratchbird/security/providers/ldap_provider.h"
#include "scratchbird/security/providers/pam_provider.h"
#include "scratchbird/security/providers/radius_provider.h"

namespace {

volatile std::uint64_t g_baseline_sink = 0;

double percentileMicros(std::vector<double> values, double pct) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double bounded_pct = std::clamp(pct, 0.0, 1.0);
    const std::size_t idx = static_cast<std::size_t>(
        bounded_pct * static_cast<double>(values.size() - 1));
    return values[idx];
}

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

void runPhase1BaselineEquivalent(const std::string& username,
                                 const std::string& secret) {
    std::uint64_t acc = 1469598103934665603ULL;
    for (int round = 0; round < 64; ++round) {
        for (unsigned char c : username) {
            acc ^= static_cast<std::uint64_t>(c + round);
            acc *= 1099511628211ULL;
        }
        for (unsigned char c : secret) {
            acc ^= static_cast<std::uint64_t>(c + (round * 3));
            acc *= 1099511628211ULL;
        }
        acc ^= (acc >> 33);
        acc *= 0xff51afd7ed558ccdULL;
        acc ^= (acc >> 33);
    }
    g_baseline_sink ^= acc;
}

template <typename Fn>
std::vector<double> captureLatenciesMicros(std::size_t iterations,
                                           Fn&& fn,
                                           std::size_t* failures_out) {
    std::vector<double> samples;
    samples.reserve(iterations);
    std::size_t failures = 0;

    for (std::size_t i = 0; i < iterations; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const bool ok = fn();
        const auto stop = std::chrono::steady_clock::now();
        const double us =
            std::chrono::duration<double, std::micro>(stop - start).count();
        samples.push_back(us);
        if (!ok) {
            ++failures;
        }
    }

    if (failures_out) {
        *failures_out = failures;
    }
    return samples;
}

struct MethodPerfMetrics {
    std::string method;
    double connect_p50_us = 0.0;
    double connect_p95_us = 0.0;
    double auth_p50_us = 0.0;
    double auth_p95_us = 0.0;
    double auth_p95_increase_pct = 0.0;
    long long rss_delta_bytes = 0;
};

template <typename ConnectFn, typename AuthFn>
MethodPerfMetrics benchmarkMethod(const std::string& method_name,
                                  std::size_t iterations,
                                  double baseline_auth_p95_us,
                                  ConnectFn&& connect_fn,
                                  AuthFn&& auth_fn) {
    // Warmup to stabilize allocator/jit effects before measuring.
    for (std::size_t i = 0; i < 256; ++i) {
        (void)connect_fn();
        (void)auth_fn();
    }

    std::size_t connect_failures = 0;
    const auto connect_samples = captureLatenciesMicros(
        iterations, std::forward<ConnectFn>(connect_fn), &connect_failures);
    EXPECT_EQ(connect_failures, 0u) << method_name << " connect path failures";

    const std::uint64_t rss_before_auth = sampleResidentBytes();
    std::size_t auth_failures = 0;
    const auto auth_samples = captureLatenciesMicros(
        iterations, std::forward<AuthFn>(auth_fn), &auth_failures);
    const std::uint64_t rss_after_auth = sampleResidentBytes();
    EXPECT_EQ(auth_failures, 0u) << method_name << " auth path failures";

    MethodPerfMetrics metrics;
    metrics.method = method_name;
    metrics.connect_p50_us = percentileMicros(connect_samples, 0.50);
    metrics.connect_p95_us = percentileMicros(connect_samples, 0.95);
    metrics.auth_p50_us = percentileMicros(auth_samples, 0.50);
    metrics.auth_p95_us = percentileMicros(auth_samples, 0.95);
    if (baseline_auth_p95_us > 0.0) {
        metrics.auth_p95_increase_pct =
            ((metrics.auth_p95_us - baseline_auth_p95_us) / baseline_auth_p95_us) * 100.0;
    }
    if (rss_before_auth > 0 && rss_after_auth > 0) {
        metrics.rss_delta_bytes = static_cast<long long>(rss_after_auth) -
                                  static_cast<long long>(rss_before_auth);
    }

    return metrics;
}

}  // namespace

TEST(AuthPluginEnterprisePerfTest, P2MethodsMeetLatencyAndLeakThresholds) {
    constexpr std::size_t kIterations = 10000;
    constexpr double kMaxAllowedP95IncreasePct = 40.0;
    constexpr long long kMaxAllowedRssDeltaBytes = 1024LL * 1024LL;

    std::size_t baseline_failures = 0;
    const auto baseline_samples = captureLatenciesMicros(
        kIterations,
        []() -> bool {
            runPhase1BaselineEquivalent("admin", "phase1_scram_equivalent");
            return true;
        },
        &baseline_failures);
    ASSERT_EQ(baseline_failures, 0u);

    const double baseline_p50 = percentileMicros(baseline_samples, 0.50);
    const double baseline_p95 = percentileMicros(baseline_samples, 0.95);
    ASSERT_GT(baseline_p95, 0.0);

    std::cout << std::fixed << std::setprecision(3)
              << "[Benchmark][AuthPluginEnterpriseBaseline]"
              << " method=phase1_equivalent_secure"
              << " auth_p50_us=" << baseline_p50
              << " auth_p95_us=" << baseline_p95
              << " iterations=" << kIterations
              << std::endl;

    std::vector<MethodPerfMetrics> all_metrics;
    all_metrics.reserve(5);

    auto ldap = scratchbird::security::providers::createDefaultLdapProvider();
    ASSERT_TRUE(ldap);
    scratchbird::security::providers::LdapAuthRequest ldap_req;
    ldap_req.username = "alice";
    ldap_req.password = "secret";
    ldap_req.ldap_uri = "ldaps://corp-ldap-1.local";
    ldap_req.require_starttls = true;
    ldap_req.allowed_ldap_endpoints = {"ldaps://corp-ldap-1.local"};
    all_metrics.push_back(benchmarkMethod(
        "scratchbird.auth.ldap_bind",
        kIterations,
        baseline_p95,
        [&]() {
            const auto rsp = ldap->authenticate(ldap_req);
            return rsp.result == scratchbird::security::providers::LdapProviderResult::SUCCESS;
        },
        [&]() {
            const auto rsp = ldap->authenticate(ldap_req);
            return rsp.result == scratchbird::security::providers::LdapProviderResult::SUCCESS;
        }));

    auto kerberos = scratchbird::security::providers::createDefaultKerberosProvider();
    ASSERT_TRUE(kerberos);
    scratchbird::security::providers::KerberosAuthRequest kerberos_req;
    kerberos_req.username = "alice";
    kerberos_req.ticket_b64 = "ticket_b64_payload";
    kerberos_req.service_principal = "scratchbird/db.local";
    kerberos_req.keytab_path = "/tmp/keytab";
    kerberos_req.kdc_endpoint = "kdc-1.local";
    kerberos_req.allowed_kdc_endpoints = {"kdc-1.local"};
    all_metrics.push_back(benchmarkMethod(
        "scratchbird.auth.kerberos_gssapi",
        kIterations,
        baseline_p95,
        [&]() {
            const auto rsp = kerberos->authenticate(kerberos_req);
            return rsp.result ==
                   scratchbird::security::providers::KerberosProviderResult::SUCCESS;
        },
        [&]() {
            const auto rsp = kerberos->authenticate(kerberos_req);
            return rsp.result ==
                   scratchbird::security::providers::KerberosProviderResult::SUCCESS;
        }));

    auto ident = scratchbird::security::providers::createDefaultIdentProvider();
    ASSERT_TRUE(ident);
    scratchbird::security::providers::IdentAuthRequest ident_req;
    ident_req.username = "alice";
    ident_req.transport_remote_address = "127.0.0.1";
    ident_req.ident_timeout_ms = 1000;
    ident_req.trusted_cidrs = {"127.", "::1"};
    ident_req.require_username_match = true;
    all_metrics.push_back(benchmarkMethod(
        "scratchbird.auth.ident_rfc1413",
        kIterations,
        baseline_p95,
        [&]() {
            const auto rsp = ident->authenticate(ident_req);
            return rsp.result == scratchbird::security::providers::IdentProviderResult::SUCCESS;
        },
        [&]() {
            const auto rsp = ident->authenticate(ident_req);
            return rsp.result == scratchbird::security::providers::IdentProviderResult::SUCCESS;
        }));

    auto radius = scratchbird::security::providers::createDefaultRadiusProvider();
    ASSERT_TRUE(radius);
    scratchbird::security::providers::RadiusAuthRequest radius_req;
    radius_req.username = "alice";
    radius_req.password = "secret";
    radius_req.radius_servers = {"radius-1.local"};
    radius_req.shared_secret_ref = "vault://radius/shared";
    radius_req.request_timeout_ms = 2000;
    radius_req.allowed_radius_endpoints = {"radius-1.local"};
    all_metrics.push_back(benchmarkMethod(
        "scratchbird.auth.radius_pap",
        kIterations,
        baseline_p95,
        [&]() {
            const auto rsp = radius->authenticate(radius_req);
            return rsp.result == scratchbird::security::providers::RadiusProviderResult::SUCCESS;
        },
        [&]() {
            const auto rsp = radius->authenticate(radius_req);
            return rsp.result == scratchbird::security::providers::RadiusProviderResult::SUCCESS;
        }));

    auto pam = scratchbird::security::providers::createDefaultPamProvider();
    ASSERT_TRUE(pam);
    scratchbird::security::providers::PamAuthRequest pam_req;
    pam_req.username = "alice";
    pam_req.password = "secret";
    pam_req.service_name = "scratchbird";
    pam_req.allowed_modules = {"scratchbird"};
    pam_req.conversation_timeout_ms = 2000;
    all_metrics.push_back(benchmarkMethod(
        "scratchbird.auth.pam_conversation",
        kIterations,
        baseline_p95,
        [&]() {
            const auto rsp = pam->authenticate(pam_req);
            return rsp.result == scratchbird::security::providers::PamProviderResult::SUCCESS;
        },
        [&]() {
            const auto rsp = pam->authenticate(pam_req);
            return rsp.result == scratchbird::security::providers::PamProviderResult::SUCCESS;
        }));

    for (const auto& metrics : all_metrics) {
        std::cout << std::fixed << std::setprecision(3)
                  << "[Benchmark][AuthPluginEnterprise]"
                  << " method=" << metrics.method
                  << " connect_p50_us=" << metrics.connect_p50_us
                  << " connect_p95_us=" << metrics.connect_p95_us
                  << " auth_p50_us=" << metrics.auth_p50_us
                  << " auth_p95_us=" << metrics.auth_p95_us
                  << " auth_p95_increase_pct=" << metrics.auth_p95_increase_pct
                  << " rss_delta_bytes=" << metrics.rss_delta_bytes
                  << " iterations=" << kIterations
                  << std::endl;

        EXPECT_LE(metrics.auth_p95_increase_pct, kMaxAllowedP95IncreasePct)
            << metrics.method << " exceeded p95 growth threshold";

        if (sampleResidentBytes() > 0) {
            EXPECT_LE(metrics.rss_delta_bytes, kMaxAllowedRssDeltaBytes)
                << metrics.method << " exceeded RSS growth threshold";
        }
    }
}
