#include <gtest/gtest.h>

#include "scratchbird/sblr/jit/jit_compiler.h"
#include "scratchbird/sblr/jit/jit_llvm_toolchain.h"

namespace scratchbird::sblr::jit::test
{
    TEST(SblrJitLlvmProviderInfoTest, jit_llvm_provider_reports_build_metadata)
    {
        const LlvmToolchainInfo& info = llvmToolchainInfo();
        if (!info.available)
        {
            GTEST_SKIP() << "LLVM JIT provider not available in this build";
        }

        EXPECT_EQ(info.provider_identity, "llvm");
        EXPECT_FALSE(info.provider_version.empty());
        EXPECT_FALSE(info.host_target_triple.empty());
        EXPECT_EQ(normalizeLlvmTargetTriple(info.host_target_triple),
                  info.host_target_triple);

        auto backend = createLlvmBackend();
        ASSERT_NE(backend, nullptr);
        EXPECT_STREQ(backend->backendName(), "llvm");
    }
}
