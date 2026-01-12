#include "scratchbird/sblr/bytecode_validator.h"

namespace scratchbird::sblr {

core::Status validateBytecode(const std::vector<uint8_t>& bytecode,
                              core::ErrorContext* ctx) {
    if (bytecode.empty()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "Empty SBLR bytecode");
        return core::Status::INVALID_ARGUMENT;
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
