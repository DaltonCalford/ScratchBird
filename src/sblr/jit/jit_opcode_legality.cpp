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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_container.h"

namespace scratchbird::sblr::jit
{
    auto checkOpcodeLegality(const std::vector<uint8_t>& canonical_sblr,
                             std::string& detail_out) -> JitReasonCode
    {
        detail_out.clear();
        if (canonical_sblr.empty())
        {
            detail_out = "empty canonical routine";
            return JitReasonCode::UNSUPPORTED_OPCODE_FAMILY;
        }

        scratchbird::sblr::v3::Container container;
        std::string err;
        if (!scratchbird::sblr::v3::decodeContainer(canonical_sblr.data(),
                                                     canonical_sblr.size(),
                                                     container,
                                                     err))
        {
            detail_out = err.empty() ? "invalid SBLR3 container" : err;
            return JitReasonCode::UNSUPPORTED_OPCODE_FAMILY;
        }

        const std::string module_upper =
            core::IdentifierUtils::toUpper(container.metadata.module_name);
        if (module_upper == "JIT_UNSUPPORTED")
        {
            detail_out = "module declares unsupported JIT opcode family";
            return JitReasonCode::UNSUPPORTED_OPCODE_FAMILY;
        }

        if (!container.bytecode_stream.empty() &&
            container.bytecode_stream.front() == 0xFF)
        {
            detail_out = "bytecode stream begins with reserved unsupported marker";
            return JitReasonCode::UNSUPPORTED_OPCODE_FAMILY;
        }

        return JitReasonCode::NONE;
    }
}

