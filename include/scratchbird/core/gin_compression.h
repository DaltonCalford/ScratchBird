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
#include <cstddef>
#include "scratchbird/core/tid.h"

namespace scratchbird
{
    namespace core
    {
        /**
         * @file gin_compression.h
         * @brief Varbyte compression for GIN posting lists
         *
         * This module implements varbyte (variable-length byte) encoding with delta
         * compression for GIN posting lists. It provides significant space savings
         * (50-70% compression ratio) for sorted TupleId arrays.
         *
         * Algorithm:
         * 1. Delta Encoding: Store differences between consecutive TIDs
         * 2. Varbyte Encoding: Encode deltas using 1-5 bytes based on value
         *
         * Encoding Format (continuation bit scheme):
         * - 0-127:           1 byte:  0xxxxxxx
         * - 128-16383:       2 bytes: 10xxxxxx xxxxxxxx
         * - 16384-2097151:   3 bytes: 110xxxxx xxxxxxxx xxxxxxxx
         * - 2097152-268435455: 4 bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
         * - 268435456-2^36:  5 bytes: 11110xxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
         * - 2^36-2^44:       6 bytes: 111110xx xxxxxxxx...
         * - 2^44-2^52:       7 bytes: 1111110x xxxxxxxx...
         * - 2^52-2^60:       8 bytes: 11111110 xxxxxxxx...
         * - 2^60+:           9 bytes: 11111111 xxxxxxxx * 8 (full 64-bit)
         */

        // Constants for varbyte encoding thresholds
        constexpr uint64_t VARBYTE_1_BYTE_MAX = 127ULL;
        constexpr uint64_t VARBYTE_2_BYTE_MAX = 16383ULL;
        constexpr uint64_t VARBYTE_3_BYTE_MAX = 2097151ULL;
        constexpr uint64_t VARBYTE_4_BYTE_MAX = 268435455ULL;
        constexpr uint64_t VARBYTE_5_BYTE_MAX = 68719476735ULL;        // 2^36 - 1
        constexpr uint64_t VARBYTE_6_BYTE_MAX = 17592186044415ULL;     // 2^44 - 1
        constexpr uint64_t VARBYTE_7_BYTE_MAX = 4503599627370495ULL;   // 2^52 - 1
        constexpr uint64_t VARBYTE_8_BYTE_MAX = 1152921504606846975ULL; // 2^60 - 1

        // Masks for decoding
        constexpr uint8_t VARBYTE_1_BYTE_MASK = 0x80; // 10000000
        constexpr uint8_t VARBYTE_2_BYTE_MASK = 0xC0; // 11000000
        constexpr uint8_t VARBYTE_3_BYTE_MASK = 0xE0; // 11100000
        constexpr uint8_t VARBYTE_4_BYTE_MASK = 0xF0; // 11110000
        constexpr uint8_t VARBYTE_5_BYTE_MASK = 0xF8; // 11111000
        constexpr uint8_t VARBYTE_6_BYTE_MASK = 0xFC; // 11111100
        constexpr uint8_t VARBYTE_7_BYTE_MASK = 0xFE; // 11111110
        constexpr uint8_t VARBYTE_8_BYTE_PREFIX = 0xFE; // 11111110
        constexpr uint8_t VARBYTE_9_BYTE_PREFIX = 0xFF; // 11111111

        /**
         * Encode a single uint64_t value using varbyte encoding.
         *
         * @param value The value to encode (typically a delta)
         * @param output Buffer to write encoded bytes (must have space for 5 bytes)
         * @return Number of bytes written (1-5)
         */
        size_t encode_varbyte(uint64_t value, uint8_t *output);

        /**
         * Decode a single varbyte-encoded value.
         *
         * @param input Pointer to encoded bytes
         * @param value_out Pointer to store decoded value
         * @return Number of bytes consumed (1-5), or 0 on error
         */
        size_t decode_varbyte(const uint8_t *input, uint64_t *value_out);

        /**
         * Compress a sorted array of TupleIds using delta + varbyte encoding.
         *
         * The TIDs must be sorted in ascending order. The function applies delta
         * encoding (storing differences) followed by varbyte encoding to achieve
         * high compression ratios (typically 50-70%).
         *
         * @param tids Sorted array of TupleIds (uint64_t)
         * @param count Number of TIDs in the array
         * @param compressed_out Buffer for compressed output (should be count * 8 bytes)
         * @param max_bytes Maximum bytes available in compressed_out
         * @return Number of bytes written, or 0 if buffer too small
         */
        size_t compress_posting_list(const uint64_t *tids, uint16_t count,
                                      uint8_t *compressed_out, size_t max_bytes);

        /**
         * Decompress varbyte-encoded deltas back to TupleId array.
         *
         * Reconstructs the original sorted TID array from compressed representation.
         *
         * @param compressed Compressed data buffer
         * @param compressed_bytes Size of compressed data in bytes
         * @param tids_out Buffer for decompressed TIDs
         * @param max_tids Maximum number of TIDs that fit in tids_out
         * @return Number of TIDs decoded, or 0 on error
         */
        size_t decompress_posting_list(const uint8_t *compressed, size_t compressed_bytes,
                                        uint64_t *tids_out, uint16_t max_tids);

        /**
         * Calculate the compressed size for a sorted TID array (without actually compressing).
         *
         * Useful for deciding whether compression is beneficial.
         *
         * @param tids Sorted array of TupleIds
         * @param count Number of TIDs
         * @return Expected compressed size in bytes
         */
        size_t estimate_compressed_size(const uint64_t *tids, uint16_t count);

        /**
         * Check if compression would be beneficial for given TID array.
         *
         * Returns true if compressed size would be < 90% of uncompressed size.
         *
         * @param tids Sorted array of TupleIds
         * @param count Number of TIDs
         * @return true if compression recommended, false otherwise
         */
        bool should_compress(const uint64_t *tids, uint16_t count);

        // TID-aware compression (GPID + slot packed into 80-bit values)
        size_t compress_posting_list_tid(const TID *tids, uint16_t count,
                                         uint8_t *compressed_out, size_t max_bytes);
        size_t decompress_posting_list_tid(const uint8_t *compressed, size_t compressed_bytes,
                                           TID *tids_out, uint16_t max_tids);
        size_t estimate_compressed_size_tid(const TID *tids, uint16_t count);
        bool should_compress_tid(const TID *tids, uint16_t count);

    } // namespace core
} // namespace scratchbird
