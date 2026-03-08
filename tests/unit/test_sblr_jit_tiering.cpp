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

        const auto binding_two = createModulePlanBinding();
        auto request_two = makeRuntimeRequest(binding_two, canonical);
        request_two.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request_two.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        runtime.setHotnessThreshold(1);
        runtime.setQueueCapacity(1);

        core::ErrorContext ctx;
        auto first = runtime.selectPath(request, &ctx);
        EXPECT_TRUE(first.compile_queued);

        auto second = runtime.selectPath(request_two, &ctx);
        EXPECT_EQ(second.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(second.reason, JitReasonCode::QUEUE_SATURATED);
    }

    TEST_F(SblrJitFixture, jit_tiering_duplicate_hot_object_requests_do_not_duplicate_queue_entries)
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
        const auto first = runtime.selectPath(request, &ctx);
        EXPECT_TRUE(first.compile_queued);

        const auto second = runtime.selectPath(request, &ctx);
        EXPECT_EQ(second.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(second.reason, JitReasonCode::COMPILE_ALREADY_QUEUED);
        EXPECT_FALSE(second.compile_queued);

        const auto snapshot = runtime.performanceSnapshot();
        EXPECT_EQ(snapshot.compile_queue_enqueued_count, 1u);
        EXPECT_EQ(snapshot.compile_queue_duplicate_count, 1u);
        EXPECT_EQ(snapshot.compile_queue_current_depth, 1u);
        EXPECT_EQ(snapshot.compile_queue_max_depth, 1u);

        const auto object_stats = runtime.objectPerformance(binding.object_uuid);
        EXPECT_EQ(object_stats.total_dispatch_count, 2u);
        EXPECT_EQ(object_stats.compile_queue_enqueued_count, 1u);
        EXPECT_EQ(object_stats.compile_queue_duplicate_count, 1u);
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
