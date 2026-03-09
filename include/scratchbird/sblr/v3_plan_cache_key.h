#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace scratchbird::sblr::v3 {

struct PlanCacheKeyInput {
    // Canonical cache binding fields required by vNext contracts.
    std::string profile_id;
    std::string profile_version;
    std::string payload_format;
    std::string payload_hash;
    std::string session_option_signature;
    std::string role_context_signature;
    std::string canonical_opcode_symbol;
    uint64_t catalog_epoch = 0;
    uint64_t security_epoch = 0;
    std::string capability_set_hash;
    uint64_t module_version = 0;
    uint64_t translation_rule_version = 0;
    std::string host_api_abi_version;
    std::string target_triples_hash;
    std::string artifact_preference;
    std::string optimization_level;
    uint16_t normalization_rule_set_id = 0;
    std::string object_ref_digest;
    std::string plan_profile_signature;
};

uint64_t stableHash64(std::string_view data);
std::string buildPlanCacheKey(const PlanCacheKeyInput& input);

}  // namespace scratchbird::sblr::v3
