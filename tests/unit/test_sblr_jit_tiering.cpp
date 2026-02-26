#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_tiering_hotness_threshold_promotion_queues_compile)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        runtime.setHotnessThreshold(2);

        core::ErrorContext ctx;
        auto first = runtime.selectPath(request, &ctx);
        EXPECT_EQ(first.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(first.reason, JitReasonCode::HOTNESS_BELOW_THRESHOLD);

        auto second = runtime.selectPath(request, &ctx);
        EXPECT_EQ(second.path, JitDispatchOutcome::Path::VM);
        EXPECT_TRUE(second.compile_queued);
        EXPECT_TRUE(second.promoted_by_hotness);
    }

    TEST_F(SblrJitFixture, jit_tiering_queue_saturation_retains_deterministic_vm_execution)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        runtime.setHotnessThreshold(1);
        runtime.setQueueCapacity(1);

        core::ErrorContext ctx;
        auto first = runtime.selectPath(request, &ctx);
        EXPECT_TRUE(first.compile_queued);

        auto second = runtime.selectPath(request, &ctx);
        EXPECT_EQ(second.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(second.reason, JitReasonCode::QUEUE_SATURATED);
    }

    TEST_F(SblrJitFixture, jit_tiering_unsupported_opcode_family_forces_vm_fallback)
    {
        const auto binding = createModulePlanBinding();
        auto canonical = compileCanonicalRoutine();
        canonical[0] = 0xFF;
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;

        const core::Status compile_status = runtime.compileExplicit(request, &ctx);
        EXPECT_EQ(compile_status, core::Status::NOT_SUPPORTED);
    }
}
