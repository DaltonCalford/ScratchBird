#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_triggers_vm_native_equivalence)
    {
        const auto binding = createModulePlanBinding();

        ASSERT_TRUE(executeSqlWithJit("CREATE TABLE jit_trigger_t (id INT, v INT)",
                                      binding,
                                      JitCompileMode::EXPLICIT_ONLY,
                                      JitExecutionPolicy::INTERPRETED_ONLY)
                        .success());
        ASSERT_TRUE(executeSqlWithJit("INSERT INTO jit_trigger_t VALUES (1, 10)",
                                      binding,
                                      JitCompileMode::EXPLICIT_ONLY,
                                      JitExecutionPolicy::INTERPRETED_ONLY)
                        .success());

        const auto vm = executeSqlWithJit("SELECT COUNT(*) FROM jit_trigger_t",
                                          binding,
                                          JitCompileMode::EXPLICIT_ONLY,
                                          JitExecutionPolicy::INTERPRETED_ONLY);
        ASSERT_TRUE(vm.success()) << vm.error();
        ASSERT_TRUE(vm.hasResultSet());

        const auto native_pref = executeSqlWithJit("SELECT COUNT(*) FROM jit_trigger_t",
                                                   binding,
                                                   JitCompileMode::JIT_ALLOWED,
                                                   JitExecutionPolicy::PREFER_NATIVE);
        ASSERT_TRUE(native_pref.success()) << native_pref.error();
        ASSERT_TRUE(native_pref.hasResultSet());
        EXPECT_EQ(vm.resultSet()->getValue(0, 0).toString(),
                  native_pref.resultSet()->getValue(0, 0).toString());
    }
}
