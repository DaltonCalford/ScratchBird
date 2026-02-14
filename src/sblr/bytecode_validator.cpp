/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/bytecode_validator.h"

#include <string>

#include "scratchbird/sblr/v3_validator.h"

namespace scratchbird::sblr {

core::Status validateBytecode(const std::vector<uint8_t>& bytecode,
                              core::ErrorContext* ctx) {
    if (bytecode.empty()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "Empty SBLR bytecode");
        return core::Status::INVALID_ARGUMENT;
    }

    // V3 container detection
    if (bytecode.size() >= 4 && bytecode[0] == 'S' && bytecode[1] == 'B' &&
        bytecode[2] == 'L' && bytecode[3] == '3') {
        auto detailed = scratchbird::sblr::v3::validateContainerDetailed(bytecode.data(), bytecode.size());
        if (!detailed.ok) {
            std::string err = detailed.code + ": " + detailed.message;
            if (!detailed.canonical_opcode_symbol.empty()) {
                err += " [";
                err += detailed.canonical_opcode_symbol;
                err += "]";
            }
            if (detailed.instruction_offset != 0) {
                err += " @offset=" + std::to_string(detailed.instruction_offset);
            }
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, err.c_str());
            return core::Status::INVALID_ARGUMENT;
        }
        return core::Status::OK;
    }

    if (bytecode.size() < 3) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "Truncated SBLR bytecode");
        return core::Status::INVALID_ARGUMENT;
    }

    if (bytecode[0] != static_cast<uint8_t>(Opcode::VERSION)) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "Missing SBLR VERSION header");
        return core::Status::INVALID_ARGUMENT;
    }

    if (bytecode[1] != static_cast<uint8_t>(SBLR_VERSION)) {
        SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                          "Unsupported SBLR version");
        return core::Status::NOT_SUPPORTED;
    }

    if (bytecode.back() != static_cast<uint8_t>(Opcode::END)) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "Missing SBLR END marker");
        return core::Status::INVALID_ARGUMENT;
    }

    return core::Status::OK;
}

}  // namespace scratchbird::sblr
