#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_target_mismatch_cross_target_mismatch_falls_back_to_vm)
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

        request.compatibility.target_triple = "aarch64-unknown-linux-gnu";
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(outcome.reason, JitReasonCode::ARTIFACT_KEY_MISMATCH_TARGET_TRIPLE);
    }
}
