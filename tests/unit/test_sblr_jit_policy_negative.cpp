#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_policy_negative_interpreted_only_policy_always_bypasses_native)
    {
        const auto binding = createModulePlanBinding();
        const auto result = executeSqlWithJit("SELECT 123",
                                              binding,
                                              JitCompileMode::JIT_ALLOWED,
                                              JitExecutionPolicy::INTERPRETED_ONLY);
        EXPECT_TRUE(result.success()) << result.error();
    }
}
