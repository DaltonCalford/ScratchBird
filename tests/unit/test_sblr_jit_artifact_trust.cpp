#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_artifact_trust_signature_required_without_signature_rejects_artifact)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        const auto key = makeCompatibilityKey(binding.object_uuid, canonical);

        JitArtifactStore store(db_->catalog_manager());
        JitArtifact artifact{};
        artifact.artifact_id = core::generateUuidV7();
        artifact.module_id = binding.module_id;
        artifact.plan_id = binding.plan_id;
        artifact.binary_blob_id = core::generateUuidV7();
        artifact.compatibility = key;
        artifact.has_native_hash = true;
        artifact.native_hash_sha256 = key.canonical_sblr_hash;
        artifact.has_signature_blob_id = false;
        artifact.state = core::CatalogManager::SblrArtifactState::READY;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        const auto result = store.fetchVerifiedArtifact(key, true, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_SIGNATURE_INVALID);
    }
}
