#include <gtest/gtest.h>

#include <string>

#include "scratchbird/version.h"

TEST(ReleasePolicyVersionSurfaceTest, CurrentPolicyMatchesPublicBetaLane) {
    const auto& policy = scratchbird::version::kReleaseLifecyclePolicy;

    EXPECT_EQ(policy.current_channel, scratchbird::version::ReleaseChannel::BETA);
    EXPECT_EQ(policy.current_phase, scratchbird::version::SupportPhase::PUBLIC_BETA);
    EXPECT_FALSE(policy.current_is_lts);
    EXPECT_TRUE(scratchbird::version::validateReleaseLifecyclePolicy(policy));
}

TEST(ReleasePolicyVersionSurfaceTest, VersionStringReflectsPublishedBetaChannel) {
    const std::string version_string = SCRATCHBIRD_VERSION_STRING;

    EXPECT_NE(version_string.find("ScratchBird v"), std::string::npos);
    EXPECT_NE(version_string.find("beta.1"), std::string::npos);
}

TEST(ReleasePolicyVersionSurfaceTest, ProductBannerPublishesLifecyclePolicy) {
    const std::string banner = scratchbird::version::formatProductVersionBanner();

    EXPECT_NE(banner.find("ScratchBird v0.1.0-beta.1"), std::string::npos);
    EXPECT_NE(banner.find("Release channel: beta"), std::string::npos);
    EXPECT_NE(banner.find("Support phase: public_beta"), std::string::npos);
    EXPECT_NE(banner.find("LTS status: non_lts"), std::string::npos);
    EXPECT_NE(banner.find("Deprecation windows: beta>=1 minor/90d"), std::string::npos);
    EXPECT_NE(banner.find("stable>=2 minor/180d"), std::string::npos);
    EXPECT_NE(banner.find("lts>=2 minor/365d"), std::string::npos);
}

TEST(ReleasePolicyVersionSurfaceTest, ComponentBannerUsesSharedLifecyclePayload) {
    const std::string banner =
        scratchbird::version::formatComponentVersionBanner("sb_listener_pg");

    EXPECT_NE(banner.find("sb_listener_pg (ScratchBird v0.1.0-beta.1)"),
              std::string::npos);
    EXPECT_NE(banner.find("Release channel: beta"), std::string::npos);
    EXPECT_NE(banner.find("LTS cadence: max_active_lines=1, support_months=24, overlap_months=6"),
              std::string::npos);
}
