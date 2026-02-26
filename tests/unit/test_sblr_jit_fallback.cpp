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
        const auto result = executeSqlWithJit("SELECT 1",
                                              binding,
                                              JitCompileMode::JIT_ALLOWED,
                                              JitExecutionPolicy::REQUIRE_NATIVE);
        EXPECT_FALSE(result.success());
        EXPECT_NE(result.error().find("SBLR_JIT_POLICY_ERROR"), std::string::npos);
    }
}
