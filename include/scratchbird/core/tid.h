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
#include "scratchbird/core/gpid.h"

/**
 * TID (Tuple Identifier) - Addressing for heap tuples in ScratchBird
 *
 * TID = (GPID, slot) where:
 * - GPID (64-bit): Global Page ID identifying the heap page (tablespace + page_number)
 * - slot (16-bit): Item ID / slot number within the page (0-65535)
 *
 * Current encoding uses two separate fields for compatibility and clarity:
 * - 64-bit GPID
 * - 16-bit slot
 *
 * This preserves Firebird MGA's stable TID invariant:
 * - TIDs never change (tuples never move)
 * - Indexes store TIDs pointing to heap pages
 * - Multi-tablespace support via GPID component
 *
 * Related: docs/specifications/TABLESPACE_SPECIFICATION.md
 */

namespace scratchbird::core
{

/**
 * TID - Tuple Identifier structure
 *
 * Stores (GPID, slot) pair for addressing heap tuples.
 * Total size: 80 bits (10 bytes) = 64-bit GPID + 16-bit slot
 */
struct TID
{
    GPID gpid;          // Global Page ID (64-bit: tablespace_id + page_number)
    uint16_t slot;      // Slot number within page (16-bit: 0-65535)

    // Default constructor
    constexpr TID() : gpid(INVALID_GPID), slot(0) {}

    // Constructor from GPID and slot
    constexpr TID(GPID gpid_, uint16_t slot_) : gpid(gpid_), slot(slot_) {}

    // Constructor from components (tablespace_id, page_number, slot)
    constexpr TID(uint16_t tablespace_id, uint64_t page_number, uint16_t slot_)
        : gpid(makeGPID(tablespace_id, page_number)), slot(slot_) {}

    // Equality comparison
    constexpr bool operator==(const TID &other) const
    {
        return gpid == other.gpid && slot == other.slot;
    }

    constexpr bool operator!=(const TID &other) const
    {
        return !(*this == other);
    }

    // Less-than comparison (for sorting)
    constexpr bool operator<(const TID &other) const
    {
        if (gpid != other.gpid)
            return gpid < other.gpid;
        return slot < other.slot;
    }

    // Validity check
    constexpr bool isValid() const
    {
        return gpid != INVALID_GPID;
    }
};

// Invalid TID constant
constexpr TID INVALID_TID{INVALID_GPID, 0};

// Packed on-disk TID (GPID + slot) for index entry storage.
#pragma pack(push, 1)
struct OnDiskTID
{
    GPID gpid;
    uint16_t slot;
};
#pragma pack(pop)

static_assert(sizeof(OnDiskTID) == (sizeof(GPID) + sizeof(uint16_t)),
              "OnDiskTID must be packed (10 bytes)");

inline OnDiskTID toOnDiskTID(const TID &tid)
{
    return OnDiskTID{tid.gpid, tid.slot};
}

inline TID fromOnDiskTID(const OnDiskTID &tid)
{
    return TID{tid.gpid, tid.slot};
}

/**
 * makeTID - Create a TID from GPID and slot
 *
 * @param gpid Global Page ID
 * @param slot Slot number within page
 * @return TID structure
 */
inline constexpr TID makeTID(GPID gpid, uint16_t slot)
{
    return TID(gpid, slot);
}

/**
 * makeTID - Create a TID from components (tablespace_id, page_number, slot)
 *
 * @param tablespace_id Tablespace ID (16-bit: 0-65535)
 * @param page_number Page number within tablespace (48-bit)
 * @param slot Slot number within page (16-bit: 0-65535)
 * @return TID structure
 */
inline constexpr TID makeTID(uint16_t tablespace_id, uint64_t page_number, uint16_t slot)
{
    return TID(tablespace_id, page_number, slot);
}

/**
 * getGPID - Extract GPID from TID
 *
 * @param tid TID structure
 * @return GPID (64-bit)
 */
inline constexpr GPID getGPID(const TID &tid)
{
    return tid.gpid;
}

/**
 * getSlot - Extract slot number from TID
 *
 * @param tid TID structure
 * @return Slot number (16-bit: 0-65535)
 */
inline constexpr uint16_t getSlot(const TID &tid)
{
    return tid.slot;
}

/**
 * getTablespaceID - Extract tablespace ID from TID
 *
 * @param tid TID structure
 * @return Tablespace ID (16-bit: 0-65535)
 */
inline constexpr uint16_t getTablespaceID(const TID &tid)
{
    return getTablespaceID(tid.gpid);
}

/**
 * getPageNumber - Extract page number from TID
 *
 * @param tid TID structure
 * @return Page number (48-bit)
 */
inline constexpr uint64_t getPageNumber(const TID &tid)
{
    return getPageNumber(tid.gpid);
}

/**
 * isValidTID - Check if TID is valid
 *
 * @param tid TID structure
 * @return true if valid, false if INVALID_TID
 */
inline constexpr bool isValidTID(const TID &tid)
{
    return tid.isValid();
}

/**
 * tidToString - Convert TID to human-readable string
 *
 * @param tid TID structure
 * @return String like "TS1:0x000000001234:5" or "INVALID_TID"
 *
 * Format: "TS{tablespace_id}:0x{page_number:012x}:{slot}"
 */
inline std::string tidToString(const TID &tid)
{
    if (!tid.isValid())
    {
        return "INVALID_TID";
    }

    uint16_t tablespace_id = getTablespaceID(tid);
    uint64_t page_number = getPageNumber(tid);
    uint16_t slot = tid.slot;

    char buf[64];
    snprintf(buf, sizeof(buf), "TS%u:0x%012lx:%u",
             tablespace_id, page_number, slot);
    return std::string(buf);
}

/**
 * LEGACY: Convert old 64-bit TID encoding to new TID structure
 *
 * Legacy encoding: (32-bit page_id << 32) | (16-bit item_id << 16) | (16-bit tablespace_id)
 * Page number is limited to 32 bits in this encoding.
 *
 * @param legacy_tid Old 64-bit TID encoding
 * @return TID structure with tablespace_id=0
 */
inline constexpr TID convertLegacyTID(uint64_t legacy_tid)
{
    uint32_t page_id = static_cast<uint32_t>(legacy_tid >> 32);
    uint16_t item_id = static_cast<uint16_t>((legacy_tid >> 16) & 0xFFFF);
    uint16_t tablespace_id = static_cast<uint16_t>(legacy_tid & 0xFFFF);

    GPID gpid = makeGPID(tablespace_id, static_cast<uint64_t>(page_id));
    return TID(gpid, item_id);
}

/**
 * LEGACY: Convert new TID structure to old 64-bit encoding
 *
 * Encodes tablespace_id into low 16 bits.
 * Returns 0 if page number exceeds 32-bit legacy encoding.
 *
 * @param tid TID structure
 * @return Old 64-bit TID encoding, or 0 if custom tablespace
 */
inline constexpr uint64_t convertTIDtoLegacy(const TID &tid)
{
    uint64_t page_number = getPageNumber(tid);
    if (page_number > 0xFFFFFFFFULL)
    {
        return 0;
    }
    uint32_t page_id = static_cast<uint32_t>(page_number);
    uint16_t item_id = tid.slot;
    uint16_t tablespace_id = getTablespaceID(tid);

    return (static_cast<uint64_t>(page_id) << 32) |
           (static_cast<uint64_t>(item_id) << 16) |
           static_cast<uint64_t>(tablespace_id);
}

} // namespace scratchbird::core

// Hash function for TID (for std::unordered_map, std::unordered_set)
namespace std
{
    template <>
    struct hash<scratchbird::core::TID>
    {
        size_t operator()(const scratchbird::core::TID &tid) const noexcept
        {
            // Combine hash of GPID and slot
            size_t h1 = std::hash<uint64_t>{}(tid.gpid);
            size_t h2 = std::hash<uint16_t>{}(tid.slot);
            return h1 ^ (h2 << 1);
        }
    };
} // namespace std
