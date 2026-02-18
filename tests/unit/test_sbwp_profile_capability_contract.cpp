#include <gtest/gtest.h>

#include <set>

#include "scratchbird/protocol/sbwp_protocol.h"

namespace scratchbird::tests {
namespace {

namespace sbwp = scratchbird::protocol::sbwp;

TEST(SbwpProfileCapabilityContractTest, CanonicalRegistryAndMaskAreStable) {
    const auto& registry = sbwp::canonicalProfileFeatureRegistry();
    ASSERT_EQ(registry.size(), 13u);

    uint64_t computed_mask = 0;
    std::set<uint64_t> seen_bits;
    for (const auto& entry : registry) {
        ASSERT_NE(entry.first, nullptr);
        EXPECT_FALSE(std::string(entry.first).empty());
        EXPECT_NE(entry.second, 0u);
        EXPECT_TRUE((entry.second & (entry.second - 1)) == 0u) << "feature bit must be a single bit";
        EXPECT_TRUE(seen_bits.insert(entry.second).second) << "duplicate feature bit";
        computed_mask |= entry.second;
    }

    EXPECT_EQ(computed_mask, sbwp::kFeatureProfileMask);
    EXPECT_EQ(sbwp::canonicalProfileFeatureMask(), sbwp::kFeatureProfileMask);
}

TEST(SbwpProfileCapabilityContractTest, EnabledProfilesFollowCanonicalRegistryOrder) {
    const auto& registry = sbwp::canonicalProfileFeatureRegistry();
    const auto enabled = sbwp::enabledProfilesFromFeatureMask(sbwp::kFeatureProfileMask);
    ASSERT_EQ(enabled.size(), registry.size());

    for (size_t i = 0; i < registry.size(); ++i) {
        EXPECT_EQ(enabled[i], registry[i].first);
    }
}

TEST(SbwpProfileCapabilityContractTest, HasProfileFeatureSupportsCanonicalNamesAndAliases) {
    const uint64_t full_mask = sbwp::kFeatureProfileMask;
    const auto& registry = sbwp::canonicalProfileFeatureRegistry();
    for (const auto& entry : registry) {
        EXPECT_TRUE(sbwp::hasProfileFeature(full_mask, entry.first))
            << "expected enabled profile: " << entry.first;
    }

    EXPECT_TRUE(sbwp::hasProfileFeature(full_mask, "firebird"));
    EXPECT_FALSE(sbwp::hasProfileFeature(full_mask, "mssql"));

    const uint64_t without_redis = full_mask & ~sbwp::kFeatureProfileRedis;
    EXPECT_FALSE(sbwp::hasProfileFeature(without_redis, "redis"));
    EXPECT_TRUE(sbwp::hasProfileFeature(without_redis, "postgresql"));
}

} // namespace
} // namespace scratchbird::tests

