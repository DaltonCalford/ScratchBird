/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file jit_backend_llvm.cpp
 * @brief Mock LLVM JIT backend.
 *
 * This translation unit preserves the JIT backend contract for Alpha wiring
 * and test coverage, but it does not embed or invoke a real LLVM codegen
 * pipeline yet. Reviewers should treat it as a deliberate mock until the
 * full LLVM driver lands.
 */
#include "scratchbird/sblr/jit/jit_compiler.h"

#include <algorithm>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/jit/jit_artifact_store.h"

namespace scratchbird::sblr::jit
{
    namespace
    {
        // Deterministic mock used to exercise backend selection and artifact
        // plumbing before the real LLVM integration is enabled.
        class LlvmBackendMock final : public JitBackend
        {
        public:
            auto backendName() const -> const char* override
            {
                return "llvm-mock";
            }

            auto compile(const LoweredRoutine& lowered,
                         const JitCompileRequest& request) -> JitCompileResult override
            {
                JitCompileResult out;
                const std::string target_upper =
                    core::IdentifierUtils::toUpper(request.key.target_triple);
                if (target_upper.find("FAIL") != std::string::npos)
                {
                    out.success = false;
                    out.reason = JitReasonCode::BACKEND_COMPILE_FAILED;
                    out.diagnostic = "LLVM backend mock rejected target triple";
                    return out;
                }

                if (lowered.lowered_ir.empty())
                {
                    out.success = false;
                    out.reason = JitReasonCode::BACKEND_COMPILE_FAILED;
                    out.diagnostic = "lowered IR is empty";
                    return out;
                }

                out.native_blob = lowered.lowered_ir;
                std::reverse(out.native_blob.begin(), out.native_blob.end());
                out.native_blob_hash_sha256 =
                    JitArtifactStore::canonicalSblrHashHex(out.native_blob);
                out.success = true;
                out.reason = JitReasonCode::NONE;
                out.diagnostic = "LLVM backend mock compiled artifact";
                return out;
            }
        };
    }

    auto createLlvmBackend() -> std::unique_ptr<JitBackend>
    {
        return std::make_unique<LlvmBackendMock>();
    }
}
