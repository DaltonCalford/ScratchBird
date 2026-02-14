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
    oss << "pc.v1";
    oss << "|op=" << input.canonical_opcode_symbol;
    oss << "|cat=" << input.catalog_epoch;
    oss << "|sec=" << input.security_epoch;
    oss << "|norm=" << input.normalization_rule_set_id;
    oss << "|obj=" << input.object_ref_digest;
    oss << "|payload=" << input.payload_hash;

    const std::string prehash = oss.str();
    const uint64_t h = stableHash64(prehash);
    oss << "|h=" << h;
    return oss.str();
}

}  // namespace scratchbird::sblr::v3

