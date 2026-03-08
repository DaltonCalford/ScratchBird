#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_runtime_selector_retires_unusable_artifact_and_requeues_rebuild)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());

        JitArtifactStore store(db_->catalog_manager());
        auto artifact = makeReadyArtifact(binding, canonical, request.compatibility);
        artifact.native_hash_sha256 = std::string(64, '0');

        core::ErrorContext ctx;
        ASSERT_EQ(store.upsertArtifact(artifact, &ctx), core::Status::OK) << ctx.message;

        const auto first = runtime.selectPath(request, &ctx);
        EXPECT_EQ(first.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(first.reason, JitReasonCode::ARTIFACT_HASH_MISMATCH);
        EXPECT_TRUE(first.compile_queued);

        std::vector<JitArtifact> retired_view;
        ASSERT_EQ(runtime.inspectArtifacts(binding.object_uuid, retired_view, &ctx), core::Status::OK)
            << ctx.message;
        EXPECT_TRUE(retired_view.empty());

        EXPECT_EQ(runtime.drainCompileQueue(&ctx), 1u);

        std::vector<JitArtifact> rebuilt;
        ASSERT_EQ(runtime.inspectArtifacts(binding.object_uuid, rebuilt, &ctx), core::Status::OK)
            << ctx.message;
        ASSERT_EQ(rebuilt.size(), 1u);

        const auto second = runtime.selectPath(request, &ctx);
        EXPECT_EQ(second.path, JitDispatchOutcome::Path::NATIVE);
        EXPECT_EQ(second.reason, JitReasonCode::NONE);

        const auto snapshot = runtime.performanceSnapshot();
        EXPECT_EQ(snapshot.retired_unusable_artifact_count, 1u);
        EXPECT_EQ(snapshot.compile_queue_enqueued_count, 1u);
        EXPECT_EQ(snapshot.queued_compile_success_count, 1u);
    }

    TEST_F(SblrJitFixture, jit_performance_snapshot_tracks_compile_latency_and_native_hit_rate)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::EXPLICIT_ONLY;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());

        core::ErrorContext ctx;
        ASSERT_EQ(runtime.compileExplicit(request, &ctx), core::Status::OK) << ctx.message;

        for (size_t i = 0; i < 16; ++i)
        {
            const auto out = runtime.selectPath(request, &ctx);
            ASSERT_EQ(out.path, JitDispatchOutcome::Path::NATIVE);
            ASSERT_EQ(out.reason, JitReasonCode::NONE);
        }

        const auto snapshot = runtime.performanceSnapshot();
        EXPECT_EQ(snapshot.explicit_compile_attempt_count, 1u);
        EXPECT_EQ(snapshot.explicit_compile_success_count, 1u);
        EXPECT_EQ(snapshot.explicit_compile_failure_count, 0u);
        EXPECT_GT(snapshot.total_compile_latency_us, 0u);
        EXPECT_EQ(snapshot.native_dispatch_count, 16u);
        EXPECT_EQ(snapshot.vm_dispatch_count, 0u);
        EXPECT_EQ(snapshot.fallback_count, 0u);

        const auto object_stats = runtime.objectPerformance(binding.object_uuid);
        EXPECT_EQ(object_stats.compile_success_count, 1u);
        EXPECT_EQ(object_stats.total_dispatch_count, 16u);
        EXPECT_EQ(object_stats.native_dispatch_count, 16u);
        EXPECT_EQ(object_stats.vm_dispatch_count, 0u);
    }
}
