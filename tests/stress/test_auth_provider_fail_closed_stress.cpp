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

class AuthProviderFailClosedStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        ldap_ = scratchbird::security::providers::createDefaultLdapProvider();
        kerberos_ = scratchbird::security::providers::createDefaultKerberosProvider();
        ident_ = scratchbird::security::providers::createDefaultIdentProvider();
        radius_ = scratchbird::security::providers::createDefaultRadiusProvider();
        pam_ = scratchbird::security::providers::createDefaultPamProvider();
        ASSERT_TRUE(ldap_);
        ASSERT_TRUE(kerberos_);
        ASSERT_TRUE(ident_);
        ASSERT_TRUE(radius_);
        ASSERT_TRUE(pam_);
    }

    std::unique_ptr<scratchbird::security::providers::LdapProvider> ldap_;
    std::unique_ptr<scratchbird::security::providers::KerberosProvider> kerberos_;
    std::unique_ptr<scratchbird::security::providers::IdentProvider> ident_;
    std::unique_ptr<scratchbird::security::providers::RadiusProvider> radius_;
    std::unique_ptr<scratchbird::security::providers::PamProvider> pam_;
};

}  // namespace

TEST_F(AuthProviderFailClosedStressTest, MixedNegativeBurstsRemainFailClosed) {
    constexpr std::size_t kIterations = 20000;
    std::size_t deny_count = 0;
    std::size_t unexpected_success = 0;

    for (std::size_t i = 0; i < kIterations; ++i) {
        switch (i % 6) {
            case 0: {
                scratchbird::security::providers::LdapAuthRequest req;
                req.username = "alice";
                req.password = "secret";
                req.ldap_uri = "ldaps://blocked.local";
                req.require_starttls = true;
                req.allowed_ldap_endpoints = {"ldaps://allowed.local"};
                const auto rsp = ldap_->authenticate(req);
                if (rsp.result == scratchbird::security::providers::LdapProviderResult::SUCCESS) {
                    ++unexpected_success;
                } else {
                    ++deny_count;
                }
                break;
            }
            case 1: {
                scratchbird::security::providers::KerberosAuthRequest req;
                req.username = "alice";
                req.ticket_b64 = "__replay__";
                req.service_principal = "scratchbird/db.local";
                req.keytab_path = "/tmp/keytab";
                req.kdc_endpoint = "kdc.allowed.local";
                req.allowed_kdc_endpoints = {"kdc.allowed.local"};
                const auto rsp = kerberos_->authenticate(req);
                if (rsp.result == scratchbird::security::providers::KerberosProviderResult::SUCCESS) {
                    ++unexpected_success;
                } else {
                    ++deny_count;
                }
                break;
            }
            case 2: {
                scratchbird::security::providers::IdentAuthRequest req;
                req.username = "alice";
                req.transport_remote_address = "203.0.113.20";
                req.trusted_cidrs = {"127.", "::1"};
                const auto rsp = ident_->authenticate(req);
                if (rsp.result == scratchbird::security::providers::IdentProviderResult::SUCCESS) {
                    ++unexpected_success;
                } else {
                    ++deny_count;
                }
                break;
            }
            case 3: {
                scratchbird::security::providers::RadiusAuthRequest req;
                req.username = "alice";
                req.password = "__timeout__";
                req.shared_secret_ref = "vault://radius/shared";
                req.radius_servers = {"radius.allowed.local"};
                req.allowed_radius_endpoints = {"radius.allowed.local"};
                const auto rsp = radius_->authenticate(req);
                if (rsp.result == scratchbird::security::providers::RadiusProviderResult::SUCCESS) {
                    ++unexpected_success;
                } else {
                    ++deny_count;
                }
                break;
            }
            case 4: {
                scratchbird::security::providers::PamAuthRequest req;
                req.username = "alice";
                req.password = "secret";
                req.service_name = "other";
                req.allowed_modules = {"scratchbird"};
                const auto rsp = pam_->authenticate(req);
                if (rsp.result == scratchbird::security::providers::PamProviderResult::SUCCESS) {
                    ++unexpected_success;
                } else {
                    ++deny_count;
                }
                break;
            }
            default: {
                // Simulated provider outage pattern: request has no usable endpoint/profile.
                scratchbird::security::providers::KerberosAuthRequest req;
                req.username = "alice";
                req.ticket_b64 = "ticket";
                req.service_principal = "";
                req.keytab_path = "";
                const auto rsp = kerberos_->authenticate(req);
                if (rsp.result == scratchbird::security::providers::KerberosProviderResult::SUCCESS) {
                    ++unexpected_success;
                } else {
                    ++deny_count;
                }
                break;
            }
        }
    }

    std::cout << "[Stress][AuthProviderFailClosed] iterations=" << kIterations
              << " deny_count=" << deny_count
              << " unexpected_success=" << unexpected_success
              << std::endl;

    EXPECT_EQ(unexpected_success, 0u);
    EXPECT_EQ(deny_count, kIterations);
}

TEST_F(AuthProviderFailClosedStressTest, MalformedPayloadStormHasNoInternalFallbacks) {
    constexpr std::size_t kIterations = 15000;
    std::mt19937_64 rng(0xfeedfaceULL);
    std::uniform_int_distribution<int> pick(0, 4);

    std::size_t internal_error_count = 0;

    for (std::size_t i = 0; i < kIterations; ++i) {
        const int branch = pick(rng);
        if (branch == 0) {
            scratchbird::security::providers::LdapAuthRequest req;
            req.username = "";
            req.password = "";
            req.ldap_uri = "";
            const auto rsp = ldap_->authenticate(req);
            if (rsp.result == scratchbird::security::providers::LdapProviderResult::INTERNAL_ERROR) {
                ++internal_error_count;
            }
        } else if (branch == 1) {
            scratchbird::security::providers::KerberosAuthRequest req;
            req.username = "";
            req.ticket_b64 = "";
            req.service_principal = "";
            req.keytab_path = "";
            const auto rsp = kerberos_->authenticate(req);
            if (rsp.result ==
                scratchbird::security::providers::KerberosProviderResult::INTERNAL_ERROR) {
                ++internal_error_count;
            }
        } else if (branch == 2) {
            scratchbird::security::providers::IdentAuthRequest req;
            req.username = "";
            req.transport_remote_address = "";
            req.trusted_cidrs.clear();
            req.ident_timeout_ms = 0;
            const auto rsp = ident_->authenticate(req);
            if (rsp.result == scratchbird::security::providers::IdentProviderResult::INTERNAL_ERROR) {
                ++internal_error_count;
            }
        } else if (branch == 3) {
            scratchbird::security::providers::RadiusAuthRequest req;
            req.username = "";
            req.password = "";
            req.request_timeout_ms = 0;
            req.shared_secret_ref = "";
            const auto rsp = radius_->authenticate(req);
            if (rsp.result ==
                scratchbird::security::providers::RadiusProviderResult::INTERNAL_ERROR) {
                ++internal_error_count;
            }
        } else {
            scratchbird::security::providers::PamAuthRequest req;
            req.username = "";
            req.password = "";
            req.service_name = "";
            req.conversation_timeout_ms = 0;
            const auto rsp = pam_->authenticate(req);
            if (rsp.result == scratchbird::security::providers::PamProviderResult::INTERNAL_ERROR) {
                ++internal_error_count;
            }
        }
    }

    std::cout << "[Stress][AuthProviderFailClosedMalformed] iterations=" << kIterations
              << " internal_error_count=" << internal_error_count
              << std::endl;

    EXPECT_EQ(internal_error_count, 0u);
}
