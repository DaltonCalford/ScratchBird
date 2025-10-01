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
            DEADLOCK = 3001,
            LOCK_TIMEOUT = 3002,
            OOM = 3003,              // Out of memory per ERROR_HANDLING.md
            PAGE_FULL = 4001,         // No space available in page
            NOT_FOUND = 4002,         // Tuple/item not found
            NOT_IMPLEMENTED = 4003,   // Feature not implemented
            COMPRESSION_ERROR = 5001, // Compression/decompression failed
        };

    } // namespace scratchbird::core

