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

#include <cstdint>
#include <string>
#include <cstdio>

namespace scratchbird::core
{

/**
 * GPID (Global Page ID) - 64-bit page identifier for multi-tablespace databases
 *
 * Encoding:
 * +----------------------+-------------------------+
 * | Tablespace ID (16)   | Page Number (48)        |
 * +----------------------+-------------------------+
 * Bits: 63-48             47-0
 *
 * - Tablespace ID: 0 = primary database file, 1 = reserved, 2-65535 = custom tablespaces
 * - Page Number: 0 to 2^48-1 (281,474,976,710,656 pages = 281TB with 16K pages)
 *
 * Design:
 * - Preserves Firebird MGA stable TID invariant (TIDs never change)
 * - TID format: (GPID, slot_number)
 * - Backward compatible: tablespace 0 behaves like original 32-bit page_id
 *
 * Example:
 * - Primary file page 100:  GPID = 0x0000000000000064 (tablespace 0, page 100)
 * - Tablespace 5 page 200:  GPID = 0x00050000000000C8 (tablespace 5, page 200)
 *
 * References:
 * - Specification: docs/specifications/TABLESPACE_SPECIFICATION.md (Section 3.2)
 * - Implementation Plan: docs/planning/TABLESPACE_IMPLEMENTATION_PLAN.md (Task 1.2.1)
 */

using GPID = uint64_t;

/**
 * Invalid GPID constant - represents an invalid/uninitialized page ID
 */
constexpr GPID INVALID_GPID = 0xFFFFFFFFFFFFFFFF;

/**
 * Primary tablespace ID - represents the primary database file
 */
constexpr uint16_t PRIMARY_TABLESPACE_ID = 0;

/**
 * Maximum tablespace ID (16-bit unsigned)
 */
constexpr uint16_t MAX_TABLESPACE_ID = 65535;

/**
 * Maximum page number within a tablespace (48-bit)
 */
constexpr uint64_t MAX_PAGE_NUMBER = 0x0000FFFFFFFFFFFF; // 2^48 - 1

/**
 * Bit shift for tablespace ID in GPID
 */
constexpr int GPID_TABLESPACE_SHIFT = 48;

/**
 * Mask for page number in GPID (lower 48 bits)
 */
constexpr uint64_t GPID_PAGE_NUMBER_MASK = 0x0000FFFFFFFFFFFF;

/**
 * makeGPID - Construct a GPID from tablespace ID and page number
 *
 * @param tablespace_id Tablespace ID (0-65535)
 * @param page_number Page number within tablespace (0 to 2^48-1)
 * @return GPID encoding both values
 *
 * Example:
 *   GPID gpid = makeGPID(5, 1000);  // Tablespace 5, page 1000
 */
constexpr inline GPID makeGPID(uint16_t tablespace_id, uint64_t page_number)
{
    // Ensure page_number fits in 48 bits
    page_number &= GPID_PAGE_NUMBER_MASK;

    // Shift tablespace_id to upper 16 bits and OR with page_number
    return (static_cast<uint64_t>(tablespace_id) << GPID_TABLESPACE_SHIFT) | page_number;
}

/**
 * getTablespaceID - Extract tablespace ID from GPID
 *
 * @param gpid Global Page ID
 * @return Tablespace ID (0-65535)
 *
 * Example:
 *   GPID gpid = 0x00050000000003E8;  // Tablespace 5, page 1000
 *   uint16_t ts_id = getTablespaceID(gpid);  // Returns 5
 */
constexpr inline uint16_t getTablespaceID(GPID gpid)
{
    return static_cast<uint16_t>(gpid >> GPID_TABLESPACE_SHIFT);
}

/**
 * getPageNumber - Extract page number from GPID
 *
 * @param gpid Global Page ID
 * @return Page number (0 to 2^48-1)
 *
 * Example:
 *   GPID gpid = 0x00050000000003E8;  // Tablespace 5, page 1000
 *   uint64_t page_num = getPageNumber(gpid);  // Returns 1000
 */
constexpr inline uint64_t getPageNumber(GPID gpid)
{
    return gpid & GPID_PAGE_NUMBER_MASK;
}

/**
 * isValidGPID - Check if GPID is valid
 *
 * @param gpid Global Page ID to validate
 * @return true if valid, false if INVALID_GPID or malformed
 *
 * Validation checks:
 * - Not equal to INVALID_GPID
 * - Page number within valid range (0 to 2^48-1)
 *
 * Note: Does NOT check if tablespace exists or page is allocated.
 *       This only validates the GPID encoding format.
 */
inline bool isValidGPID(GPID gpid)
{
    if (gpid == INVALID_GPID)
    {
        return false;
    }

    uint64_t page_number = getPageNumber(gpid);
    return page_number <= MAX_PAGE_NUMBER;
}

/**
 * isPrimaryTablespace - Check if GPID refers to primary database file
 *
 * @param gpid Global Page ID
 * @return true if tablespace_id == 0 (primary file), false otherwise
 *
 * Example:
 *   GPID gpid1 = makeGPID(0, 100);  // Primary file
 *   GPID gpid2 = makeGPID(5, 100);  // Custom tablespace
 *   isPrimaryTablespace(gpid1);  // Returns true
 *   isPrimaryTablespace(gpid2);  // Returns false
 */
inline bool isPrimaryTablespace(GPID gpid)
{
    return getTablespaceID(gpid) == PRIMARY_TABLESPACE_ID;
}

/**
 * convertPageIDtoGPID - Convert legacy 32-bit page_id to GPID (tablespace 0)
 *
 * @param page_id Legacy 32-bit page ID
 * @return GPID with tablespace_id=0 and page_number=page_id
 *
 * Purpose: Backward compatibility during migration to GPID addressing.
 *
 * Example:
 *   uint32_t old_page_id = 100;
 *   GPID gpid = convertPageIDtoGPID(old_page_id);  // GPID = 0x0000000000000064
 */
inline GPID convertPageIDtoGPID(uint32_t page_id)
{
    return makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id));
}

/**
 * convertGPIDtoPageID - Convert GPID to legacy 32-bit page_id (if tablespace 0)
 *
 * @param gpid Global Page ID
 * @param[out] page_id Output legacy page ID (only valid if function returns true)
 * @return true if conversion successful (tablespace == 0 and page < 2^32),
 *         false otherwise
 *
 * Purpose: Backward compatibility for code that still uses 32-bit page_id.
 *
 * Example:
 *   GPID gpid = makeGPID(0, 100);
 *   uint32_t page_id;
 *   bool success = convertGPIDtoPageID(gpid, &page_id);  // success = true, page_id = 100
 *
 *   GPID gpid2 = makeGPID(5, 100);
 *   success = convertGPIDtoPageID(gpid2, &page_id);  // success = false (tablespace != 0)
 */
inline bool convertGPIDtoPageID(GPID gpid, uint32_t *page_id)
{
    if (getTablespaceID(gpid) != PRIMARY_TABLESPACE_ID)
    {
        return false;  // Not primary tablespace
    }

    uint64_t page_number = getPageNumber(gpid);
    if (page_number > UINT32_MAX)
    {
        return false;  // Page number too large for 32-bit
    }

    if (page_id != nullptr)
    {
        *page_id = static_cast<uint32_t>(page_number);
    }

    return true;
}

/**
 * GPID Formatting Helper - Convert GPID to string for debugging/logging
 *
 * Format: "ts={tablespace_id},pg={page_number}"
 *
 * Example:
 *   GPID gpid = makeGPID(5, 1000);
 *   // String: "ts=5,pg=1000"
 *
 * Note: This is a helper for debugging. For production logging, consider
 *       using structured logging with separate tablespace_id and page_number fields.
 */
inline std::string gpidToString(GPID gpid)
{
    if (gpid == INVALID_GPID)
    {
        return "INVALID_GPID";
    }

    uint16_t ts_id = getTablespaceID(gpid);
    uint64_t pg_num = getPageNumber(gpid);

    // Use snprintf for efficient formatting
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "ts=%u,pg=%lu",
                  static_cast<unsigned>(ts_id),
                  static_cast<unsigned long>(pg_num));

    return std::string(buffer);
}

/**
 * Static assertions to validate GPID encoding assumptions
 */
static_assert(sizeof(GPID) == 8, "GPID must be 64-bit");
static_assert(GPID_TABLESPACE_SHIFT == 48, "Tablespace ID must be in upper 16 bits");
static_assert((GPID_PAGE_NUMBER_MASK >> 48) == 0, "Page number mask must be lower 48 bits");

/**
 * Example usage:
 *
 * // Create GPID for tablespace 5, page 1000
 * GPID gpid = makeGPID(5, 1000);
 *
 * // Extract components
 * uint16_t ts_id = getTablespaceID(gpid);   // Returns 5
 * uint64_t pg_num = getPageNumber(gpid);    // Returns 1000
 *
 * // Validate
 * if (isValidGPID(gpid)) {
 *     // Use GPID
 * }
 *
 * // Check if primary tablespace
 * if (isPrimaryTablespace(gpid)) {
 *     // This is the main database file
 * }
 *
 * // Backward compatibility
 * uint32_t old_page_id = 100;
 * GPID new_gpid = convertPageIDtoGPID(old_page_id);
 *
 * // Logging
 * std::cout << "Page: " << gpidToString(gpid) << std::endl;
 */

} // namespace scratchbird::core
