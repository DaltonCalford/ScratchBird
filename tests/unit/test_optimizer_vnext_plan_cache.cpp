#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/optimizer/vnext_plan_cache.h"

using scratchbird::core::generateUuidV7;
using scratchbird::optimizer::NativeArtifactMetadata;
using scratchbird::optimizer::NativeArtifactStatus;
using scratchbird::optimizer::VNextPlanCache;
using scratchbird::optimizer::VNextPlanCacheValue;
using scratchbird::sblr::v3::PlanCacheKeyInput;

namespace
{
auto makeKey() -> PlanCacheKeyInput
{
    PlanCacheKeyInput key;
    key.profile_id = "native";
    key.profile_version = "3.0";
    key.taxonomy_contract_id = "sb_runtime_plan/v12";
    key.payload_format = "SQL_TEXT";
    key.payload_hash = "hash_payload";
    key.session_option_signature = "sess_sig";
    key.role_context_signature = "role_sig";
    key.canonical_opcode_symbol = "OP_STMT_DML_SELECT";
    key.catalog_epoch = 10;
    key.security_epoch = 20;
    key.capability_set_hash = "cap_hash";
    key.module_version = 1;
    key.translation_rule_version = 2;
    key.host_api_abi_version = "abi_v1";
    key.target_triples_hash = "triples_hash";
    key.artifact_preference = "NATIVE_PREFERRED";
    key.optimization_level = "O2";
    key.normalization_rule_set_id = 0x1301;
    key.object_ref_digest = "obj_digest";
    key.plan_profile_signature = "GENERIC";
    key.index_family_signature =
        "0:1:BTREE_EQ_SCAN:2:EXACT_KEY:INDEX_NATIVE:0:QUERYABLE";
    key.family_statistics_signature = "0:BTREE_EQ_SCAN:7:HIGH:QUERYABLE";
    key.statistics_snapshot_signature = "stats:1;4";
    key.cost_profile_id = "sb_cost_formula/default@1";
    key.policy_snapshot_id = "policy:g1:t1";
    return key;
}

auto makeValue() -> VNextPlanCacheValue
{
    VNextPlanCacheValue value;
    value.native_feature_key = "feature.select";
    value.normalized_payload_hash = "norm_hash";
    value.native_ast_hash = "ast_hash";
    value.sblr_hash = "sblr_hash";
    value.sblr_payload = {0x01, 0x02, 0x03, 0x04};
    value.compile_module_id = generateUuidV7();
    value.compile_time_us = 55;
    value.native_artifact_status = NativeArtifactStatus::FALLBACK_SBLR_ONLY;
    value.fallback_reason_code = "UDR_1521";
    return value;
}

auto metricCounterValue(const std::string& metric_name,
                        const std::vector<std::string>& labels) -> double
{
    auto* metric = scratchbird::core::MetricsRegistry::getInstance().get(metric_name);
    if (metric == nullptr)
    {
        return 0.0;
    }
    auto* counter = dynamic_cast<scratchbird::core::Counter*>(metric);
    if (counter == nullptr)
    {
        return 0.0;
    }
    return counter->get(labels);
}
} // namespace

TEST(OptimizerVNextPlanCacheTest, ExactKeyMatchRequiredForHit)
{
    VNextPlanCache cache;
    PlanCacheKeyInput key = makeKey();
    VNextPlanCacheValue value = makeValue();

    auto put_result = cache.put(key, value);
    ASSERT_TRUE(put_result.ok) << put_result.error_code << ": " << put_result.error_message;

    auto hit = cache.get(key);
    ASSERT_TRUE(hit.ok);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(value.sblr_payload, hit.value.sblr_payload);
    EXPECT_EQ(value.sblr_hash, hit.value.sblr_hash);

    PlanCacheKeyInput changed_epoch = key;
    changed_epoch.catalog_epoch = 11;
    auto miss = cache.get(changed_epoch);
    ASSERT_TRUE(miss.ok);
    EXPECT_FALSE(miss.hit);

    auto stats = cache.getStats();
    EXPECT_EQ(1U, stats.hits);
    EXPECT_EQ(1U, stats.misses);
}

TEST(OptimizerVNextPlanCacheTest, EpochAndCapabilityInvalidationRemovesStaleEntries)
{
    VNextPlanCache cache;
    PlanCacheKeyInput base_key = makeKey();
    VNextPlanCacheValue value = makeValue();

    PlanCacheKeyInput keep = base_key;
    keep.catalog_epoch = 20;
    keep.security_epoch = 30;
    keep.capability_set_hash = "new_cap";
    keep.module_version = 5;
    keep.translation_rule_version = 9;
    keep.host_api_abi_version = "abi_v2";
    keep.target_triples_hash = "triples_new";

    PlanCacheKeyInput stale_catalog = keep;
    stale_catalog.catalog_epoch = 10;
    PlanCacheKeyInput stale_security = keep;
    stale_security.security_epoch = 21;
    PlanCacheKeyInput stale_cap = keep;
    stale_cap.capability_set_hash = "old_cap";
    PlanCacheKeyInput stale_module = keep;
    stale_module.module_version = 4;
    PlanCacheKeyInput stale_rule = keep;
    stale_rule.translation_rule_version = 8;
    PlanCacheKeyInput stale_abi = keep;
    stale_abi.host_api_abi_version = "abi_v1";
    PlanCacheKeyInput stale_triples = keep;
    stale_triples.target_triples_hash = "triples_old";

    ASSERT_TRUE(cache.put(keep, value).ok);
    ASSERT_TRUE(cache.put(stale_catalog, value).ok);
    ASSERT_TRUE(cache.put(stale_security, value).ok);
    ASSERT_TRUE(cache.put(stale_cap, value).ok);
    ASSERT_TRUE(cache.put(stale_module, value).ok);
    ASSERT_TRUE(cache.put(stale_rule, value).ok);
    ASSERT_TRUE(cache.put(stale_abi, value).ok);
    ASSERT_TRUE(cache.put(stale_triples, value).ok);

    EXPECT_EQ(1U, cache.invalidateByCatalogEpoch(20));
    EXPECT_EQ(1U, cache.invalidateBySecurityEpoch(30));
    EXPECT_EQ(1U, cache.invalidateByCapabilitySetHash("new_cap"));
    EXPECT_EQ(1U, cache.invalidateByModuleVersion(5));
    EXPECT_EQ(1U, cache.invalidateByTranslationRuleVersion(9));
    EXPECT_EQ(1U, cache.invalidateByHostApiAbiVersion("abi_v2"));
    EXPECT_EQ(1U, cache.invalidateByTargetTriplesHash("triples_new"));

    auto hit = cache.get(keep);
    ASSERT_TRUE(hit.ok);
    ASSERT_TRUE(hit.hit);

    auto stats = cache.getStats();
    EXPECT_EQ(7U, stats.invalidations);
    EXPECT_EQ(1U, stats.entries);
}

TEST(OptimizerVNextPlanCacheTest, ImmutableWriteRejectsWithUDR1511)
{
    VNextPlanCache cache;
    PlanCacheKeyInput key = makeKey();
    VNextPlanCacheValue value = makeValue();

    ASSERT_TRUE(cache.put(key, value).ok);
    auto second = cache.put(key, value);
    ASSERT_FALSE(second.ok);
    EXPECT_EQ("UDR_1511", second.error_code);
}

TEST(OptimizerVNextPlanCacheTest, InvalidKeyRejectsWithUDR1511)
{
    VNextPlanCache cache;
    PlanCacheKeyInput invalid = makeKey();
    invalid.profile_id.clear();
    auto get_result = cache.get(invalid);
    ASSERT_FALSE(get_result.ok);
    EXPECT_EQ("UDR_1511", get_result.error_code);
}

TEST(OptimizerVNextPlanCacheTest, UnsortedNativeArtifactsRejectWithUDR1511)
{
    VNextPlanCache cache;
    PlanCacheKeyInput key = makeKey();
    VNextPlanCacheValue value = makeValue();
    value.native_artifact_status = NativeArtifactStatus::GENERATED;
    value.fallback_reason_code.clear();

    NativeArtifactMetadata a;
    a.target_triple = "x86_64-pc-windows-msvc";
    a.object_format = "COFF_OBJECT";
    a.host_api_abi_version = "abi_v1";
    a.artifact_hash = "hash1";
    a.artifact_size_bytes = 123;

    NativeArtifactMetadata b;
    b.target_triple = "x86_64-apple-darwin";
    b.object_format = "MACHO_OBJECT";
    b.host_api_abi_version = "abi_v1";
    b.artifact_hash = "hash2";
    b.artifact_size_bytes = 456;

    value.native_artifacts = {a, b}; // unsorted

    auto put = cache.put(key, value);
    ASSERT_FALSE(put.ok);
    EXPECT_EQ("UDR_1511", put.error_code);
}

TEST(OptimizerVNextPlanCacheTest, InvalidateAllClearsEntries)
{
    VNextPlanCache cache;
    PlanCacheKeyInput key = makeKey();
    VNextPlanCacheValue value = makeValue();

    ASSERT_TRUE(cache.put(key, value).ok);
    EXPECT_EQ(1U, cache.invalidateAll());
    EXPECT_EQ(0U, cache.invalidateAll());

    auto miss = cache.get(key);
    ASSERT_TRUE(miss.ok);
    EXPECT_FALSE(miss.hit);

    auto stats = cache.getStats();
    EXPECT_EQ(1U, stats.invalidations);
    EXPECT_EQ(0U, stats.entries);
}

TEST(OptimizerVNextPlanCacheTest, ObjectRefInvalidationIsLocalized)
{
    VNextPlanCache cache;
    PlanCacheKeyInput kept = makeKey();
    kept.object_ref_digest = "schema_keep";
    PlanCacheKeyInput removed = makeKey();
    removed.payload_hash = "payload_removed";
    removed.object_ref_digest = "schema_removed";
    VNextPlanCacheValue value = makeValue();

    ASSERT_TRUE(cache.put(kept, value).ok);
    ASSERT_TRUE(cache.put(removed, value).ok);

    EXPECT_EQ(1U, cache.invalidateByObjectRefDigest("schema_removed"));

    auto kept_hit = cache.get(kept);
    ASSERT_TRUE(kept_hit.ok);
    EXPECT_TRUE(kept_hit.hit);

    auto removed_hit = cache.get(removed);
    ASSERT_TRUE(removed_hit.ok);
    EXPECT_FALSE(removed_hit.hit);
}

TEST(OptimizerVNextPlanCacheTest, MetricsEmissionTracksHitMissAndRejectPaths)
{
    VNextPlanCache cache;
    PlanCacheKeyInput key = makeKey();
    VNextPlanCacheValue value = makeValue();

    const std::string metric = "scratchbird_vnext_optimizer_events_total";
    const double put_ok_before = metricCounterValue(metric, {"plan_cache_put", "ok", "NONE"});
    const double get_hit_before = metricCounterValue(metric, {"plan_cache_get", "hit", "NONE"});
    const double get_miss_before = metricCounterValue(metric, {"plan_cache_get", "miss", "NONE"});
    const double get_reject_before = metricCounterValue(metric, {"plan_cache_get", "reject", "UDR_1511"});

    ASSERT_TRUE(cache.put(key, value).ok);
    auto hit = cache.get(key);
    ASSERT_TRUE(hit.ok);
    ASSERT_TRUE(hit.hit);

    PlanCacheKeyInput miss_key = key;
    miss_key.catalog_epoch = 77;
    auto miss = cache.get(miss_key);
    ASSERT_TRUE(miss.ok);
    ASSERT_FALSE(miss.hit);

    PlanCacheKeyInput invalid_key = key;
    invalid_key.profile_id.clear();
    auto reject = cache.get(invalid_key);
    ASSERT_FALSE(reject.ok);
    EXPECT_EQ("UDR_1511", reject.error_code);

    EXPECT_EQ(put_ok_before + 1.0, metricCounterValue(metric, {"plan_cache_put", "ok", "NONE"}));
    EXPECT_EQ(get_hit_before + 1.0, metricCounterValue(metric, {"plan_cache_get", "hit", "NONE"}));
    EXPECT_EQ(get_miss_before + 1.0, metricCounterValue(metric, {"plan_cache_get", "miss", "NONE"}));
    EXPECT_EQ(get_reject_before + 1.0,
              metricCounterValue(metric, {"plan_cache_get", "reject", "UDR_1511"}));
}
