#include <gtest/gtest.h>

#include "scratchbird/sblr/v3_plan_cache_key.h"

using namespace scratchbird::sblr::v3;

TEST(SBLRV3PlanCacheKey, DeterministicForSameInput) {
    PlanCacheKeyInput in;
    in.canonical_opcode_symbol = "OP_STMT_DML_SELECT";
    in.catalog_epoch = 11;
    in.security_epoch = 21;
    in.normalization_rule_set_id = 0x1301;
    in.object_ref_digest = "o:abc123";
    in.payload_hash = "p:def456";

    const std::string key_a = buildPlanCacheKey(in);
    const std::string key_b = buildPlanCacheKey(in);
    EXPECT_EQ(key_a, key_b);
}

TEST(SBLRV3PlanCacheKey, ChangesWhenEpochChanges) {
    PlanCacheKeyInput base;
    base.canonical_opcode_symbol = "OP_STMT_DML_SELECT";
    base.catalog_epoch = 1;
    base.security_epoch = 2;
    base.normalization_rule_set_id = 0x1301;
    base.object_ref_digest = "o:abc123";
    base.payload_hash = "p:def456";

    PlanCacheKeyInput changed_catalog = base;
    changed_catalog.catalog_epoch = 3;
    EXPECT_NE(buildPlanCacheKey(base), buildPlanCacheKey(changed_catalog));

    PlanCacheKeyInput changed_security = base;
    changed_security.security_epoch = 4;
    EXPECT_NE(buildPlanCacheKey(base), buildPlanCacheKey(changed_security));
}

TEST(SBLRV3PlanCacheKey, StableHashConsistency) {
    const std::string payload = "pc.v1|op=OP_STMT_DML_SELECT|cat=1|sec=2";
    EXPECT_EQ(stableHash64(payload), stableHash64(payload));
    EXPECT_NE(stableHash64(payload), stableHash64(payload + "|x"));
}

