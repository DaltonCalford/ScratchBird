#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_artifact_key_target_triple_mismatch_forces_deopt)
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
        artifact.state = core::CatalogManager::SblrArtifactState::READY;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        ArtifactCompatibilityKey mismatch = key;
        mismatch.target_triple = "aarch64-unknown-linux-gnu";
        const auto result = store.fetchVerifiedArtifact(mismatch, false, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_KEY_MISMATCH_TARGET_TRIPLE);
    }

    TEST_F(SblrJitFixture, jit_artifact_key_native_abi_mismatch_forces_deopt)
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
        artifact.state = core::CatalogManager::SblrArtifactState::READY;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        ArtifactCompatibilityKey mismatch = key;
        mismatch.native_abi_version = "sb_abi_v2";
        const auto result = store.fetchVerifiedArtifact(mismatch, false, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_KEY_MISMATCH_NATIVE_ABI);
    }

    TEST_F(SblrJitFixture, jit_artifact_hash_non_hex_forces_deopt)
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
        artifact.native_hash_sha256 = std::string(64, 'z');
        artifact.state = core::CatalogManager::SblrArtifactState::READY;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        const auto result = store.fetchVerifiedArtifact(key, false, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_HASH_INVALID);
    }

    TEST_F(SblrJitFixture, jit_artifact_key_cpu_profile_mismatch_forces_deopt)
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
        artifact.state = core::CatalogManager::SblrArtifactState::READY;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        ArtifactCompatibilityKey mismatch = key;
        mismatch.cpu_feature_profile = "avx512";
        const auto result = store.fetchVerifiedArtifact(mismatch, false, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_KEY_MISMATCH_CPU_PROFILE);
    }

    TEST_F(SblrJitFixture, jit_artifact_key_compiler_identity_mismatch_forces_deopt)
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
        artifact.state = core::CatalogManager::SblrArtifactState::READY;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        ArtifactCompatibilityKey mismatch = key;
        mismatch.compiler_identity = "other_compiler";
        const auto result = store.fetchVerifiedArtifact(mismatch, false, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_KEY_MISMATCH_COMPILER_IDENTITY);
    }

    TEST_F(SblrJitFixture, jit_artifact_key_compiler_version_mismatch_forces_deopt)
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
        artifact.state = core::CatalogManager::SblrArtifactState::READY;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        ArtifactCompatibilityKey mismatch = key;
        mismatch.compiler_version = "9.9.9";
        const auto result = store.fetchVerifiedArtifact(mismatch, false, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_KEY_MISMATCH_COMPILER_VERSION);
    }

    TEST_F(SblrJitFixture, jit_artifact_key_optimization_profile_mismatch_forces_deopt)
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
        artifact.state = core::CatalogManager::SblrArtifactState::READY;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        ArtifactCompatibilityKey mismatch = key;
        mismatch.optimization_profile = "O0";
        const auto result = store.fetchVerifiedArtifact(mismatch, false, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_KEY_MISMATCH_OPTIMIZATION_PROFILE);
    }

    TEST_F(SblrJitFixture, jit_artifact_non_ready_state_rejects_selection)
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
        artifact.state = core::CatalogManager::SblrArtifactState::COMPILING;

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        const auto result = store.fetchVerifiedArtifact(key, false, &ctx);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.reason, JitReasonCode::ARTIFACT_STATE_NOT_READY);
    }
}
