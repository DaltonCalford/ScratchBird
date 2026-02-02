/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
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
