#include <gtest/gtest.h>

#include "scratchbird/sblr/v3_plan_cache_key.h"

using namespace scratchbird::sblr::v3;

TEST(SBLRV3PlanCacheKey, DeterministicForSameInput) {
    PlanCacheKeyInput in;
    in.profile_id = "native";
    in.profile_version = "3.0";
    in.payload_format = "SQL_TEXT";
    in.payload_hash = "p:def456";
    in.session_option_signature = "sess:aa";
    in.role_context_signature = "role:bb";
    in.canonical_opcode_symbol = "OP_STMT_DML_SELECT";
    in.catalog_epoch = 11;
    in.security_epoch = 21;
    in.capability_set_hash = "cap:123";
    in.module_version = 5;
    in.translation_rule_version = 7;
    in.host_api_abi_version = "abi-1";
    in.target_triples_hash = "triples:xyz";
    in.artifact_preference = "NATIVE_PREFERRED";
    in.optimization_level = "O2";
    in.normalization_rule_set_id = 0x1301;
    in.object_ref_digest = "o:abc123";

    const std::string key_a = buildPlanCacheKey(in);
    const std::string key_b = buildPlanCacheKey(in);
    EXPECT_EQ(key_a, key_b);
}

TEST(SBLRV3PlanCacheKey, ChangesWhenEpochChanges) {
    PlanCacheKeyInput base;
    base.profile_id = "native";
    base.profile_version = "3.0";
    base.payload_format = "SQL_TEXT";
    base.payload_hash = "p:def456";
    base.session_option_signature = "sess:aa";
    base.role_context_signature = "role:bb";
    base.canonical_opcode_symbol = "OP_STMT_DML_SELECT";
    base.catalog_epoch = 1;
    base.security_epoch = 2;
    base.capability_set_hash = "cap:123";
    base.module_version = 5;
    base.translation_rule_version = 7;
    base.host_api_abi_version = "abi-1";
    base.target_triples_hash = "triples:xyz";
    base.artifact_preference = "NATIVE_PREFERRED";
    base.optimization_level = "O2";
    base.normalization_rule_set_id = 0x1301;
    base.object_ref_digest = "o:abc123";

    PlanCacheKeyInput changed_catalog = base;
    changed_catalog.catalog_epoch = 3;
    EXPECT_NE(buildPlanCacheKey(base), buildPlanCacheKey(changed_catalog));

    PlanCacheKeyInput changed_security = base;
    changed_security.security_epoch = 4;
    EXPECT_NE(buildPlanCacheKey(base), buildPlanCacheKey(changed_security));
}

TEST(SBLRV3PlanCacheKey, ChangesWhenCapabilityOrRuleBindingChanges) {
    PlanCacheKeyInput base;
    base.profile_id = "native";
    base.profile_version = "3.0";
    base.payload_format = "SQL_TEXT";
    base.payload_hash = "p:def456";
    base.session_option_signature = "sess:aa";
    base.role_context_signature = "role:bb";
    base.catalog_epoch = 1;
    base.security_epoch = 2;
    base.capability_set_hash = "cap:123";
    base.module_version = 5;
    base.translation_rule_version = 7;
    base.host_api_abi_version = "abi-1";
    base.target_triples_hash = "triples:xyz";
    base.artifact_preference = "NATIVE_PREFERRED";
    base.optimization_level = "O2";
    base.canonical_opcode_symbol = "OP_STMT_DML_SELECT";
    base.normalization_rule_set_id = 0x1301;
    base.object_ref_digest = "o:abc123";

    PlanCacheKeyInput changed_capability = base;
    changed_capability.capability_set_hash = "cap:999";
    EXPECT_NE(buildPlanCacheKey(base), buildPlanCacheKey(changed_capability));

    PlanCacheKeyInput changed_module = base;
    changed_module.module_version = 6;
    EXPECT_NE(buildPlanCacheKey(base), buildPlanCacheKey(changed_module));

    PlanCacheKeyInput changed_rule = base;
    changed_rule.translation_rule_version = 8;
    EXPECT_NE(buildPlanCacheKey(base), buildPlanCacheKey(changed_rule));

    PlanCacheKeyInput changed_abi = base;
    changed_abi.host_api_abi_version = "abi-2";
    EXPECT_NE(buildPlanCacheKey(base), buildPlanCacheKey(changed_abi));
}

TEST(SBLRV3PlanCacheKey, StableHashConsistency) {
    const std::string payload = "pc.v2|op=OP_STMT_DML_SELECT|cat=1|sec=2";
    EXPECT_EQ(stableHash64(payload), stableHash64(payload));
    EXPECT_NE(stableHash64(payload), stableHash64(payload + "|x"));
}
