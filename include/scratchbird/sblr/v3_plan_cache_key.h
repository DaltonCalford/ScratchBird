#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace scratchbird::sblr::v3 {

struct PlanCacheKeyInput {
    std::string canonical_opcode_symbol;
    uint64_t catalog_epoch = 0;
    uint64_t security_epoch = 0;
    uint16_t normalization_rule_set_id = 0;
    std::string object_ref_digest;
    std::string payload_hash;
};

uint64_t stableHash64(std::string_view data);
std::string buildPlanCacheKey(const PlanCacheKeyInput& input);

}  // namespace scratchbird::sblr::v3

