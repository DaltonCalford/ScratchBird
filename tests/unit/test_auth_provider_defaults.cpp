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

#include "scratchbird/security/providers/ident_provider.h"
#include "scratchbird/security/providers/kerberos_provider.h"
#include "scratchbird/security/providers/ldap_provider.h"
#include "scratchbird/security/providers/pam_provider.h"
#include "scratchbird/security/providers/radius_provider.h"

TEST(AuthProviderDefaultsTest, LdapEnforcesAllowlistAndTls) {
    auto provider = scratchbird::security::providers::createDefaultLdapProvider();
    scratchbird::security::providers::LdapAuthRequest req;
    req.username = "alice";
    req.password = "secret";
    req.ldap_uri = "ldap://example.local";
    req.require_starttls = true;
    req.allowed_ldap_endpoints = {"ldaps://example.local"};

    auto response = provider->authenticate(req);
    EXPECT_EQ(response.result,
              scratchbird::security::providers::LdapProviderResult::AUTH_PLUGIN_POLICY_DENIED);
}

TEST(AuthProviderDefaultsTest, KerberosRejectsReplayMarker) {
    auto provider = scratchbird::security::providers::createDefaultKerberosProvider();
    scratchbird::security::providers::KerberosAuthRequest req;
    req.username = "alice";
    req.ticket_b64 = "__replay__";
    req.service_principal = "scratchbird/db.local";
    req.keytab_path = "/tmp/keytab";

    auto response = provider->authenticate(req);
    EXPECT_EQ(response.result,
              scratchbird::security::providers::KerberosProviderResult::AUTH_KERBEROS_REPLAY_DETECTED);
}

TEST(AuthProviderDefaultsTest, IdentRejectsUntrustedTransport) {
    auto provider = scratchbird::security::providers::createDefaultIdentProvider();
    scratchbird::security::providers::IdentAuthRequest req;
    req.username = "alice";
    req.transport_remote_address = "10.0.0.25";
    req.trusted_cidrs = {"127.", "::1"};

    auto response = provider->authenticate(req);
    EXPECT_EQ(response.result,
              scratchbird::security::providers::IdentProviderResult::AUTH_IDENT_UNTRUSTED_TRANSPORT);
}

TEST(AuthProviderDefaultsTest, RadiusRequiresSharedSecretRef) {
    auto provider = scratchbird::security::providers::createDefaultRadiusProvider();
    scratchbird::security::providers::RadiusAuthRequest req;
    req.username = "alice";
    req.password = "secret";
    req.radius_servers = {"radius-a.local"};
    req.allowed_radius_endpoints = {"radius-a.local"};

    auto response = provider->authenticate(req);
    EXPECT_EQ(response.result,
              scratchbird::security::providers::RadiusProviderResult::AUTH_RADIUS_SHARED_SECRET_INVALID);
}

TEST(AuthProviderDefaultsTest, RadiusTimeoutFailsClosed) {
    auto provider = scratchbird::security::providers::createDefaultRadiusProvider();
    scratchbird::security::providers::RadiusAuthRequest req;
    req.username = "alice";
    req.password = "secret";
    req.shared_secret_ref = "vault://radius/shared";
    req.radius_servers = {"radius-a.local"};
    req.allowed_radius_endpoints = {"radius-a.local"};
    req.request_timeout_ms = 0;

    auto response = provider->authenticate(req);
    EXPECT_EQ(response.result,
              scratchbird::security::providers::RadiusProviderResult::AUTH_RADIUS_TIMEOUT);
}

TEST(AuthProviderDefaultsTest, PamRejectsServiceOutsideAllowlist) {
    auto provider = scratchbird::security::providers::createDefaultPamProvider();
    scratchbird::security::providers::PamAuthRequest req;
    req.username = "alice";
    req.password = "secret";
    req.service_name = "scratchbird-pam";
    req.allowed_modules = {"db-admin"};

    auto response = provider->authenticate(req);
    EXPECT_EQ(response.result,
              scratchbird::security::providers::PamProviderResult::AUTH_PAM_SERVICE_NOT_ALLOWED);
}
