#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_functions_vm_native_equivalence)
    {
        const auto binding = createModulePlanBinding();

        const auto vm = executeSqlWithJit("SELECT ABS(-42)",
                                          binding,
                                          JitCompileMode::EXPLICIT_ONLY,
                                          JitExecutionPolicy::INTERPRETED_ONLY);
        ASSERT_TRUE(vm.success()) << vm.error();
        ASSERT_TRUE(vm.hasResultSet());
        ASSERT_EQ(vm.resultSet()->rowCount(), 1U);

        const auto native_pref = executeSqlWithJit("SELECT ABS(-42)",
                                                   binding,
                                                   JitCompileMode::JIT_ALLOWED,
                                                   JitExecutionPolicy::PREFER_NATIVE);
        ASSERT_TRUE(native_pref.success()) << native_pref.error();
        ASSERT_TRUE(native_pref.hasResultSet());
        ASSERT_EQ(native_pref.resultSet()->rowCount(), 1U);
        EXPECT_EQ(vm.resultSet()->getValue(0, 0).toString(),
                  native_pref.resultSet()->getValue(0, 0).toString());
    }
}
