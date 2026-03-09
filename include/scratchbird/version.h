/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#ifndef SCRATCHBIRD_VERSION_H
#define SCRATCHBIRD_VERSION_H

#include <cstdint>
#include <sstream>
#include <string>

#define SCRATCHBIRD_VERSION_MAJOR 0
#define SCRATCHBIRD_VERSION_MINOR 1
#define SCRATCHBIRD_VERSION_PATCH 0
#define SCRATCHBIRD_VERSION_SUFFIX "beta.1"

#define SCRATCHBIRD_VERSION_STRING "ScratchBird v0.1.0-beta.1"

namespace scratchbird {
namespace version {

enum class ReleaseChannel : uint8_t {
    CANARY = 0,
    BETA = 1,
    STABLE = 2,
    LTS = 3,
};

enum class SupportPhase : uint8_t {
    EXPERIMENTAL = 0,
    PUBLIC_BETA = 1,
    GENERAL_AVAILABILITY = 2,
    LONG_TERM_SUPPORT = 3,
};

struct DeprecationWindow {
    uint16_t minimum_notice_days = 0;
    uint8_t minimum_minor_releases = 0;
};

struct ReleaseLifecyclePolicy {
    ReleaseChannel current_channel = ReleaseChannel::CANARY;
    SupportPhase current_phase = SupportPhase::EXPERIMENTAL;
    bool current_is_lts = false;
    uint8_t max_active_lts_lines = 0;
    uint8_t lts_support_months = 0;
    uint8_t lts_overlap_months = 0;
    DeprecationWindow beta_window{};
    DeprecationWindow stable_window{};
    DeprecationWindow lts_window{};
};

inline constexpr ReleaseLifecyclePolicy kReleaseLifecyclePolicy{
    ReleaseChannel::BETA,
    SupportPhase::PUBLIC_BETA,
    false,
    1,
    24,
    6,
    {90, 1},
    {180, 2},
    {365, 2},
};

inline constexpr const char* releaseChannelName(ReleaseChannel channel) {
    switch (channel) {
        case ReleaseChannel::CANARY:
            return "canary";
        case ReleaseChannel::BETA:
            return "beta";
        case ReleaseChannel::STABLE:
            return "stable";
        case ReleaseChannel::LTS:
            return "lts";
        default:
            return "unknown";
    }
}

inline constexpr const char* supportPhaseName(SupportPhase phase) {
    switch (phase) {
        case SupportPhase::EXPERIMENTAL:
            return "experimental";
        case SupportPhase::PUBLIC_BETA:
            return "public_beta";
        case SupportPhase::GENERAL_AVAILABILITY:
            return "general_availability";
        case SupportPhase::LONG_TERM_SUPPORT:
            return "long_term_support";
        default:
            return "unknown";
    }
}

inline constexpr bool releaseChannelMatchesPhase(ReleaseChannel channel,
                                                 SupportPhase phase) {
    switch (channel) {
        case ReleaseChannel::CANARY:
            return phase == SupportPhase::EXPERIMENTAL;
        case ReleaseChannel::BETA:
            return phase == SupportPhase::PUBLIC_BETA;
        case ReleaseChannel::STABLE:
            return phase == SupportPhase::GENERAL_AVAILABILITY;
        case ReleaseChannel::LTS:
            return phase == SupportPhase::LONG_TERM_SUPPORT;
        default:
            return false;
    }
}

inline constexpr bool validateReleaseLifecyclePolicy(
    const ReleaseLifecyclePolicy& policy) {
    if (!releaseChannelMatchesPhase(policy.current_channel, policy.current_phase)) {
        return false;
    }
    if (policy.current_is_lts != (policy.current_channel == ReleaseChannel::LTS)) {
        return false;
    }
    if (policy.max_active_lts_lines == 0 || policy.max_active_lts_lines > 2) {
        return false;
    }
    if (policy.lts_support_months < 12) {
        return false;
    }
    if (policy.lts_overlap_months == 0 ||
        policy.lts_overlap_months > policy.lts_support_months) {
        return false;
    }
    if (policy.beta_window.minimum_notice_days < 30 ||
        policy.beta_window.minimum_minor_releases == 0) {
        return false;
    }
    if (policy.stable_window.minimum_notice_days < policy.beta_window.minimum_notice_days ||
        policy.stable_window.minimum_minor_releases < policy.beta_window.minimum_minor_releases) {
        return false;
    }
    if (policy.lts_window.minimum_notice_days < policy.stable_window.minimum_notice_days ||
        policy.lts_window.minimum_minor_releases < policy.stable_window.minimum_minor_releases) {
        return false;
    }
    return true;
}

static_assert(validateReleaseLifecyclePolicy(kReleaseLifecyclePolicy),
              "ScratchBird release lifecycle policy must remain coherent");

inline std::string formatReleaseLifecycleLines() {
    const auto& policy = kReleaseLifecyclePolicy;
    std::ostringstream out;
    out << "Release channel: " << releaseChannelName(policy.current_channel) << "\n"
        << "Support phase: " << supportPhaseName(policy.current_phase) << "\n"
        << "LTS status: " << (policy.current_is_lts ? "active_lts" : "non_lts") << "\n"
        << "LTS cadence: max_active_lines=" << static_cast<unsigned>(policy.max_active_lts_lines)
        << ", support_months=" << static_cast<unsigned>(policy.lts_support_months)
        << ", overlap_months=" << static_cast<unsigned>(policy.lts_overlap_months) << "\n"
        << "Deprecation windows: beta>="
        << static_cast<unsigned>(policy.beta_window.minimum_minor_releases) << " minor/"
        << policy.beta_window.minimum_notice_days << "d, stable>="
        << static_cast<unsigned>(policy.stable_window.minimum_minor_releases) << " minor/"
        << policy.stable_window.minimum_notice_days << "d, lts>="
        << static_cast<unsigned>(policy.lts_window.minimum_minor_releases) << " minor/"
        << policy.lts_window.minimum_notice_days << "d";
    return out.str();
}

inline std::string formatProductVersionBanner() {
    std::ostringstream out;
    out << SCRATCHBIRD_VERSION_STRING << "\n" << formatReleaseLifecycleLines();
    return out.str();
}

inline std::string formatComponentVersionBanner(const std::string& component_name) {
    std::ostringstream out;
    out << component_name << " (" << SCRATCHBIRD_VERSION_STRING << ")\n"
        << formatReleaseLifecycleLines();
    return out.str();
}

}  // namespace version
}  // namespace scratchbird

#endif // SCRATCHBIRD_VERSION_H
