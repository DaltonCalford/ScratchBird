#include "scratchbird/sblr/v3_plan_cache_key.h"

#include <sstream>

namespace scratchbird::sblr::v3 {

namespace {

constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

void hashField(uint64_t& h, std::string_view field) {
    for (unsigned char c : field) {
        h ^= static_cast<uint64_t>(c);
        h *= kFnvPrime;
    }
    // Field separator to avoid accidental concatenation collisions.
    h ^= static_cast<uint64_t>('|');
    h *= kFnvPrime;
}

}  // namespace

uint64_t stableHash64(std::string_view data) {
    uint64_t h = kFnvOffsetBasis;
    hashField(h, data);
    return h;
}

std::string buildPlanCacheKey(const PlanCacheKeyInput& input) {
    std::ostringstream oss;
    oss << "pc.v2";
    oss << "|profile_id=" << input.profile_id;
    oss << "|profile_ver=" << input.profile_version;
    oss << "|fmt=" << input.payload_format;
    oss << "|payload=" << input.payload_hash;
    oss << "|sess_sig=" << input.session_option_signature;
    oss << "|role_sig=" << input.role_context_signature;
    oss << "|op=" << input.canonical_opcode_symbol;
    oss << "|cat=" << input.catalog_epoch;
    oss << "|sec=" << input.security_epoch;
    oss << "|cap=" << input.capability_set_hash;
    oss << "|mod_ver=" << input.module_version;
    oss << "|rule_ver=" << input.translation_rule_version;
    oss << "|abi=" << input.host_api_abi_version;
    oss << "|triples=" << input.target_triples_hash;
    oss << "|pref=" << input.artifact_preference;
    oss << "|opt=" << input.optimization_level;
    oss << "|norm=" << input.normalization_rule_set_id;
    oss << "|obj=" << input.object_ref_digest;
    oss << "|plan_profile=" << input.plan_profile_signature;
    oss << "|stats_sig=" << input.statistics_snapshot_signature;
    oss << "|cost_profile=" << input.cost_profile_id;
    oss << "|policy_snapshot=" << input.policy_snapshot_id;

    const std::string prehash = oss.str();
    const uint64_t h = stableHash64(prehash);
    oss << "|h=" << h;
    return oss.str();
}

}  // namespace scratchbird::sblr::v3
