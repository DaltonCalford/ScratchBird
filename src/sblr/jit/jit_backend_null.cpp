/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_compiler.h"

namespace scratchbird::sblr::jit
{
    namespace
    {
        class NullBackend final : public JitBackend
        {
        public:
            auto backendName() const -> const char* override
            {
                return "null";
            }

            auto compile(const LoweredRoutine&,
                         const JitCompileRequest&) -> JitCompileResult override
            {
                JitCompileResult out;
                out.success = false;
                out.reason = JitReasonCode::BACKEND_UNAVAILABLE;
                out.diagnostic = "null backend forces deterministic VM fallback";
                return out;
            }
        };
    }

    auto createNullBackend() -> std::unique_ptr<JitBackend>
    {
        return std::make_unique<NullBackend>();
    }
}

