/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_llvm_toolchain.h"

#if defined(SCRATCHBIRD_HAVE_LLVM_JIT) && SCRATCHBIRD_HAVE_LLVM_JIT
#include <llvm/TargetParser/Triple.h>
#endif

namespace scratchbird::sblr::jit
{
    namespace
    {
        auto buildToolchainInfo() -> LlvmToolchainInfo
        {
            LlvmToolchainInfo out;
            out.provider_identity = "llvm";
#if defined(SCRATCHBIRD_HAVE_LLVM_JIT) && SCRATCHBIRD_HAVE_LLVM_JIT
            out.available = true;
            out.provider_identity = SCRATCHBIRD_LLVM_JIT_PROVIDER_ID;
            out.provider_version = SCRATCHBIRD_LLVM_JIT_PROVIDER_VERSION;
            out.host_target_triple = llvm::Triple::normalize(
                SCRATCHBIRD_LLVM_JIT_HOST_TRIPLE);
#else
            out.available = false;
            out.provider_version = "unavailable";
            out.host_target_triple = "native";
#endif
            if (out.host_target_triple.empty())
            {
                out.host_target_triple = "native";
            }
            return out;
        }
    }

    auto llvmToolchainInfo() -> const LlvmToolchainInfo&
    {
        static const LlvmToolchainInfo info = buildToolchainInfo();
        return info;
    }

    auto llvmToolchainAvailable() -> bool
    {
        return llvmToolchainInfo().available;
    }

    auto normalizeLlvmTargetTriple(const std::string& requested) -> std::string
    {
        const LlvmToolchainInfo& info = llvmToolchainInfo();
        std::string target = requested;
        if (target.empty())
        {
            target = info.host_target_triple;
        }
#if defined(SCRATCHBIRD_HAVE_LLVM_JIT) && SCRATCHBIRD_HAVE_LLVM_JIT
        target = llvm::Triple::normalize(target);
#endif
        if (target.empty())
        {
            target = info.host_target_triple;
        }
        if (target.empty())
        {
            target = "native";
        }
        return target;
    }
}
