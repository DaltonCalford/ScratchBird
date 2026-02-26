#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_invalidation_dependency_signature_change_retires_artifacts)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        ASSERT_EQ(runtime.compileExplicit(request, &ctx), core::Status::OK) << ctx.message;

        std::vector<JitArtifact> before;
        ASSERT_EQ(runtime.inspectArtifacts(binding.object_uuid, before, &ctx), core::Status::OK);
        ASSERT_FALSE(before.empty());

        ASSERT_EQ(runtime.onDependencySignatureChange(binding.object_uuid, &ctx), core::Status::OK)
            << ctx.message;
        std::vector<JitArtifact> after;
        ASSERT_EQ(runtime.inspectArtifacts(binding.object_uuid, after, &ctx), core::Status::OK);
        EXPECT_TRUE(after.empty());
    }

    TEST_F(SblrJitFixture, jit_invalidation_policy_version_change_retires_artifacts)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;
        request.compatibility.security_policy_version = 8;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        ASSERT_EQ(runtime.compileExplicit(request, &ctx), core::Status::OK) << ctx.message;

        std::vector<JitArtifact> before;
        ASSERT_EQ(runtime.inspectArtifacts(binding.object_uuid, before, &ctx), core::Status::OK);
        ASSERT_FALSE(before.empty());

        ASSERT_EQ(runtime.onSecurityPolicyVersionChange(binding.object_uuid, &ctx), core::Status::OK)
            << ctx.message;
        std::vector<JitArtifact> after;
        ASSERT_EQ(runtime.inspectArtifacts(binding.object_uuid, after, &ctx), core::Status::OK);
        EXPECT_TRUE(after.empty());
    }
}
