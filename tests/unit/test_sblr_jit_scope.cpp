#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_scope_unknown_surface_never_enters_native_selection)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();

        auto request = makeRuntimeRequest(binding, canonical);
        request.surface = RoutineSurfaceKind::UNKNOWN;
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(outcome.reason, JitReasonCode::NATIVE_SCOPE_NOT_ELIGIBLE);
        EXPECT_FALSE(outcome.compile_queued);
    }

    TEST_F(SblrJitFixture, jit_scope_compile_explicit_rejects_non_routine_surfaces)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();

        auto request = makeRuntimeRequest(binding, canonical);
        request.surface = RoutineSurfaceKind::UNKNOWN;
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        EXPECT_EQ(runtime.compileExplicit(request, &ctx), core::Status::INVALID_ARGUMENT);
    }
}
