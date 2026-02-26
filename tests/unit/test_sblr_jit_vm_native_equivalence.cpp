#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_vm_native_equivalence_differential_corpus)
    {
        const auto binding = createModulePlanBinding();

        const std::vector<std::string> corpus = {
            "SELECT 1 + 2",
            "SELECT ABS(-8)",
            "SELECT 10 / 2",
            "SELECT 5 * 9",
        };

        for (const auto& sql : corpus)
        {
            const auto vm = executeSqlWithJit(sql,
                                              binding,
                                              JitCompileMode::EXPLICIT_ONLY,
                                              JitExecutionPolicy::INTERPRETED_ONLY);
            ASSERT_TRUE(vm.success()) << sql << " :: " << vm.error();
            ASSERT_TRUE(vm.hasResultSet()) << sql;

            const auto native_pref = executeSqlWithJit(sql,
                                                       binding,
                                                       JitCompileMode::JIT_ALLOWED,
                                                       JitExecutionPolicy::PREFER_NATIVE);
            ASSERT_TRUE(native_pref.success()) << sql << " :: " << native_pref.error();
            ASSERT_TRUE(native_pref.hasResultSet()) << sql;
            ASSERT_EQ(vm.resultSet()->rowCount(), native_pref.resultSet()->rowCount());
            ASSERT_EQ(vm.resultSet()->columnCount(), native_pref.resultSet()->columnCount());
            EXPECT_EQ(vm.resultSet()->getValue(0, 0).toString(),
                      native_pref.resultSet()->getValue(0, 0).toString());
        }
    }
}
