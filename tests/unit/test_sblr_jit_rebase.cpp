#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_rebase_cross_target_rebase_keeps_canonical_sblr_executable)
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

        request.compatibility.target_triple = "arm64-apple-darwin";
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::VM);

        const auto vm_result = executeSqlWithJit("SELECT 9",
                                                 binding,
                                                 JitCompileMode::EXPLICIT_ONLY,
                                                 JitExecutionPolicy::INTERPRETED_ONLY);
        EXPECT_TRUE(vm_result.success()) << vm_result.error();
        ASSERT_TRUE(vm_result.hasResultSet());
        EXPECT_EQ(vm_result.resultSet()->getValue(0, 0).toString(), "9");
    }
}
