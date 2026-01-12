#pragma once

#include <vector>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/opcodes.h"

namespace scratchbird::sblr {

// Validate SBLR bytecode before execution (version/header sanity).
core::Status validateBytecode(const std::vector<uint8_t>& bytecode,
                              core::ErrorContext* ctx = nullptr);

}  // namespace scratchbird::sblr
