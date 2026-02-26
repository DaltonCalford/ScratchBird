#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_queue_stress_retains_vm_correctness)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        runtime.setHotnessThreshold(1);
        runtime.setQueueCapacity(8);

        core::ErrorContext ctx;
        for (size_t i = 0; i < 128; ++i)
        {
            const auto out = runtime.selectPath(request, &ctx);
            EXPECT_EQ(out.path, JitDispatchOutcome::Path::VM);
        }
    }
}
