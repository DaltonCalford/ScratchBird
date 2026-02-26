#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_soak_mixed_policy_no_semantic_divergence)
    {
        const auto binding = createModulePlanBinding();
        for (size_t i = 0; i < 200; ++i)
        {
            const auto vm = executeSqlWithJit("SELECT 3 * 7",
                                              binding,
                                              JitCompileMode::EXPLICIT_ONLY,
                                              JitExecutionPolicy::INTERPRETED_ONLY);
            ASSERT_TRUE(vm.success()) << vm.error();

            const auto pref = executeSqlWithJit("SELECT 3 * 7",
                                                binding,
                                                JitCompileMode::JIT_ALLOWED,
                                                JitExecutionPolicy::PREFER_NATIVE);
            ASSERT_TRUE(pref.success()) << pref.error();
            ASSERT_TRUE(vm.hasResultSet());
            ASSERT_TRUE(pref.hasResultSet());
            EXPECT_EQ(vm.resultSet()->getValue(0, 0).toString(),
                      pref.resultSet()->getValue(0, 0).toString());
        }
    }
}
