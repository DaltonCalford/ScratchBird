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

#include "scratchbird/server/ipc_server.h"

namespace
{

using scratchbird::server::IPCMethod;
using scratchbird::server::LocalIPCPolicy;

TEST(LocalIPCPolicyTest, DefaultPreferredMethodMatchesPlatformDefault)
{
    const LocalIPCPolicy policy = scratchbird::server::getDefaultLocalIPCPolicy();
    EXPECT_EQ(policy.preferred_method, scratchbird::server::getDefaultIPCMethod());
}

TEST(LocalIPCPolicyTest, ResolveAutoUsesPolicyPreferredMethod)
{
    const LocalIPCPolicy defaults = scratchbird::server::getDefaultLocalIPCPolicy();
    const LocalIPCPolicy resolved = scratchbird::server::resolveLocalIPCPolicy(IPCMethod::AUTO);

    EXPECT_EQ(resolved.preferred_method, defaults.preferred_method);
    EXPECT_EQ(resolved.fallback_method, defaults.fallback_method);
    EXPECT_EQ(resolved.fallback_enabled, defaults.fallback_enabled);
}

TEST(LocalIPCPolicyTest, ResolveExplicitMethodDisablesFallback)
{
    const LocalIPCPolicy resolved =
        scratchbird::server::resolveLocalIPCPolicy(IPCMethod::TCP_LOCALHOST);

    EXPECT_EQ(resolved.preferred_method, IPCMethod::TCP_LOCALHOST);
    EXPECT_EQ(resolved.fallback_method, IPCMethod::TCP_LOCALHOST);
    EXPECT_FALSE(resolved.fallback_enabled);
}

TEST(LocalIPCPolicyTest, PeerCredentialExpectationMatchesPlatformContract)
{
    const LocalIPCPolicy policy = scratchbird::server::getDefaultLocalIPCPolicy();

#ifdef _WIN32
    EXPECT_EQ(policy.preferred_method, IPCMethod::NAMED_PIPE);
    EXPECT_FALSE(policy.peer_credentials_supported);
    EXPECT_TRUE(policy.peer_credentials_required_for_peer_auth);
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    EXPECT_EQ(policy.preferred_method, IPCMethod::UNIX_SOCKET);
    EXPECT_TRUE(policy.peer_credentials_supported);
    EXPECT_TRUE(policy.peer_credentials_required_for_peer_auth);
#else
    EXPECT_EQ(policy.preferred_method, IPCMethod::TCP_LOCALHOST);
    EXPECT_FALSE(policy.peer_credentials_supported);
#endif
}

} // namespace
