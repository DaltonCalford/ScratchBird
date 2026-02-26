#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_policy_hint_disable_compile_suppresses_queue_promotion)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;
        request.policy.hints.disable_compile = true;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(outcome.reason, JitReasonCode::HINT_DISABLE_COMPILE);
        EXPECT_FALSE(outcome.compile_queued);
    }

    TEST_F(SblrJitFixture, jit_policy_hint_disable_execute_forces_vm_path)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;
        request.policy.hints.disable_execute = true;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(outcome.reason, JitReasonCode::HINT_DISABLE_EXECUTE);
    }

    TEST_F(SblrJitFixture, jit_policy_require_native_without_artifact_returns_deterministic_error)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();

        const auto result = executeSqlWithJit("SELECT 1",
                                              binding,
                                              JitCompileMode::EXPLICIT_ONLY,
                                              JitExecutionPolicy::REQUIRE_NATIVE);
        EXPECT_FALSE(result.success());
        EXPECT_NE(result.error().find("SBLR_JIT_POLICY_ERROR"), std::string::npos);
    }
}
