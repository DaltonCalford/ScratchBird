#pragma once

#include <cstdint>

namespace scratchbird::core
{

    enum class Status : uint32_t
    {
        OK = 0,
        FILE_NOT_FOUND = 1001,
        FILE_EXISTS = 1002,
        IO_ERROR = 1003,
        INVALID_PATH = 1004,
        PERMISSION_DENIED = 1005,
        INVALID_ARGUMENT = 1006,
        PAGE_CORRUPT = 2001,
        CHECKSUM_MISMATCH = 2002,
        INDEX_CORRUPTED = 2003,   // Index update failed, REINDEX needed
        DEADLOCK = 3001,
        LOCK_TIMEOUT = 3002,
        LOCK_CONFLICT = 3003,
        OOM = 3004,               // Out of memory per ERROR_HANDLING.md
        CANCELLED = 3005,         // Operation cancelled by user (Phase 4 Task 4.1.3)
        PAGE_FULL = 4001,         // No space available in page
        NOT_FOUND = 4002,         // Tuple/item not found
        NOT_IMPLEMENTED = 4003,   // Feature not implemented
        TYPE_MISMATCH = 4004,     // Type mismatch
        CONSTRAINT_VIOLATION = 4005, // Constraint violation
        OUT_OF_RANGE = 4006,      // Value out of range
        COMPRESSION_ERROR = 5001, // Compression/decompression failed
    };

} // namespace scratchbird::core
