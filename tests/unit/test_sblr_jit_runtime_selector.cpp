#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_runtime_selector_records_load_failure_for_exact_match_missing_blob)
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

        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::EXPLICIT_ONLY;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(outcome.reason, JitReasonCode::ARTIFACT_BLOB_LOAD_FAILED);

        core::CatalogManager::SblrArtifactStatsCatalogInfo stats{};
        EXPECT_EQ(db_->catalog_manager()->getSblrArtifactStatsCatalogEntry(artifact.artifact_id,
                                                                           stats,
                                                                           &ctx),
                  core::Status::NOT_FOUND);
    }

    TEST_F(SblrJitFixture, jit_runtime_selector_uses_loaded_artifact_and_records_execution)
    {
        const auto binding = createModulePlanBinding();
        const auto bytecode = compileSql("SELECT 40 + 2");
        ASSERT_FALSE(bytecode.empty());

        auto request = makeRuntimeRequest(binding, bytecode);
        request.policy.object_compile_mode = JitCompileMode::EXPLICIT_ONLY;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        ASSERT_EQ(runtime.compileExplicit(request, &ctx), core::Status::OK) << ctx.message;

        sblr::Executor executor(db_.get());
        executor.setConnectionContext(conn_ctx_.get());
        executor.setCurrentSchema(default_schema_id_);
        executor.setJitObjectBinding(binding.object_uuid, binding.module_id, binding.plan_id);
        executor.setJitCompatibilityProfile(request.compatibility.target_triple,
                                            request.compatibility.cpu_feature_profile,
                                            request.compatibility.native_abi_version,
                                            request.compatibility.compiler_identity,
                                            request.compatibility.compiler_version,
                                            request.compatibility.optimization_profile,
                                            request.compatibility.security_policy_version);
        executor.setJitPolicy(JitCompileMode::EXPLICIT_ONLY,
                              JitExecutionPolicy::PREFER_NATIVE);
        executor.setJitBackendLlvmEnabled(true);

        const sblr::ExecutionResult result = executor.execute(bytecode);
        ASSERT_TRUE(result.success()) << result.error();
        EXPECT_TRUE(executor.lastJitUsedNativePath());
        EXPECT_EQ(executor.lastJitReasonCode(), JitReasonCode::NONE);

        std::vector<JitArtifact> artifacts;
        ASSERT_EQ(runtime.inspectArtifacts(binding.object_uuid, artifacts, &ctx), core::Status::OK)
            << ctx.message;
        ASSERT_EQ(artifacts.size(), 1u);

        core::CatalogManager::SblrArtifactStatsCatalogInfo stats{};
        ASSERT_EQ(db_->catalog_manager()->getSblrArtifactStatsCatalogEntry(artifacts.front().artifact_id,
                                                                           stats,
                                                                           &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(stats.execution_count, 1u);
        EXPECT_EQ(stats.fallback_count, 0u);
        EXPECT_EQ(stats.load_failure_count, 0u);
    }
}
