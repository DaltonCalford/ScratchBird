#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_fallback_prefer_native_falls_back_to_vm_when_artifact_missing)
    {
        const auto binding = createModulePlanBinding();
        const auto result = executeSqlWithJit("SELECT 1",
                                              binding,
                                              JitCompileMode::JIT_ALLOWED,
                                              JitExecutionPolicy::PREFER_NATIVE);
        EXPECT_TRUE(result.success()) << result.error();
    }

    TEST_F(SblrJitFixture, jit_fallback_require_native_errors_when_artifact_missing)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_execution_policy = JitExecutionPolicy::REQUIRE_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::ERROR);
        EXPECT_EQ(outcome.reason, JitReasonCode::REQUIRE_NATIVE_NOT_AVAILABLE);
    }
}
