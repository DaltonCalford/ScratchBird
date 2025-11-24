#pragma once

#include <cstdint>
#include <array>
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{
    // Common type alias for object IDs (UUIDv7)
    // Used across the system for users, roles, tables, etc.
    using ID = UuidV7Bytes;

} // namespace scratchbird::core
