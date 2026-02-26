#include "unit/test_sblr_jit_test_utils.h"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace scratchbird::sblr::jit::test
{
    namespace
    {
        auto p95Micros(const std::vector<uint64_t>& samples) -> uint64_t
        {
            if (samples.empty())
            {
                return 0;
            }
            std::vector<uint64_t> sorted = samples;
            std::sort(sorted.begin(), sorted.end());
            const size_t idx = (sorted.size() * 95) / 100;
            return sorted[std::min(idx, sorted.size() - 1)];
        }
    }

    TEST_F(SblrJitFixture, jit_performance_envelope)
    {
        const auto binding = createModulePlanBinding();
        std::vector<uint64_t> vm_samples;
        std::vector<uint64_t> jit_samples;
        vm_samples.reserve(20);
        jit_samples.reserve(20);

        for (size_t i = 0; i < 20; ++i)
        {
            auto vm_start = std::chrono::steady_clock::now();
            auto vm = executeSqlWithJit("SELECT 100 + 23",
                                        binding,
                                        JitCompileMode::EXPLICIT_ONLY,
                                        JitExecutionPolicy::INTERPRETED_ONLY);
            auto vm_end = std::chrono::steady_clock::now();
            ASSERT_TRUE(vm.success()) << vm.error();
            vm_samples.push_back(
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(vm_end - vm_start)
                        .count()));

            auto jit_start = std::chrono::steady_clock::now();
            auto jit = executeSqlWithJit("SELECT 100 + 23",
                                         binding,
                                         JitCompileMode::JIT_ALLOWED,
                                         JitExecutionPolicy::PREFER_NATIVE);
            auto jit_end = std::chrono::steady_clock::now();
            ASSERT_TRUE(jit.success()) << jit.error();
            jit_samples.push_back(
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(jit_end - jit_start)
                        .count()));
        }

        const uint64_t vm_p95 = p95Micros(vm_samples);
        const uint64_t jit_p95 = p95Micros(jit_samples);
        std::cout << "vm_p95_us=" << vm_p95 << "\n";
        std::cout << "jit_p95_us=" << jit_p95 << "\n";
        EXPECT_GT(vm_p95, 0U);
        EXPECT_GT(jit_p95, 0U);
        EXPECT_LE(jit_p95, vm_p95 * 3 + 1);
    }
}
