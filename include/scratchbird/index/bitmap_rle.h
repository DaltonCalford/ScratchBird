/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-10: Bitmap RLE Compression
//
// Run-Length Encoding for bitmap indexes, providing 50%+ space savings
// for sequential TID (tuple ID) patterns.
//
// Encoding Format:
// - Each run is encoded as (length, value)
// - Short runs use 1-byte length
// - Long runs use variable-length encoding
// - Mixed mode for alternating patterns
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <cstdint>
#include <span>
#include <optional>
#include <memory>

namespace scratchbird::index {

// ============================================================================
// RLE Run Types
// ============================================================================

// Run type indicators
enum class RLERunType : uint8_t {
    ZEROS = 0x00,         // Run of zeros (unset bits)
    ONES = 0x01,          // Run of ones (set bits)
    LITERAL = 0x02,       // Literal bits (no compression)
    MIXED = 0x03,         // Mixed pattern (alternating)
};

// A single RLE run
struct RLERun {
    RLERunType type;
    uint32_t length;      // Number of bits in run
    uint64_t literal;     // For LITERAL type: raw bits (up to 64)

    // Create zero run
    static RLERun zeros(uint32_t count) {
        return RLERun{RLERunType::ZEROS, count, 0};
    }

    // Create ones run
    static RLERun ones(uint32_t count) {
        return RLERun{RLERunType::ONES, count, 0};
    }

    // Create literal run
    static RLERun literal(uint64_t bits, uint32_t count) {
        return RLERun{RLERunType::LITERAL, count, bits};
    }
};

// ============================================================================
// Compression Statistics
// ============================================================================

struct BitmapRLEStats {
    size_t original_bytes = 0;        // Uncompressed size
    size_t compressed_bytes = 0;      // Compressed size
    uint32_t run_count = 0;           // Number of runs
    uint32_t zero_runs = 0;           // Number of zero runs
    uint32_t one_runs = 0;            // Number of one runs
    uint32_t literal_runs = 0;        // Number of literal runs
    uint32_t total_bits = 0;          // Total bits in bitmap

    double compressionRatio() const {
        return original_bytes > 0 ?
               static_cast<double>(compressed_bytes) / original_bytes : 1.0;
    }

    double savingsPercent() const {
        return (1.0 - compressionRatio()) * 100.0;
    }
};

// ============================================================================
// RLE-Compressed Bitmap
// ============================================================================

class RLEBitmap {
public:
    RLEBitmap() = default;

    // Construct from uncompressed bitmap
    explicit RLEBitmap(std::span<const uint8_t> bitmap);
    explicit RLEBitmap(std::span<const uint64_t> bitmap);

    // Construct from raw runs
    explicit RLEBitmap(std::vector<RLERun>&& runs, uint32_t total_bits);

    // ========================================================================
    // Compression/Decompression
    // ========================================================================

    // Compress an uncompressed bitmap
    static RLEBitmap compress(std::span<const uint8_t> bitmap);
    static RLEBitmap compress(std::span<const uint64_t> bitmap);

    // Decompress to uncompressed bitmap
    std::vector<uint8_t> decompress() const;
    std::vector<uint64_t> decompressWords() const;

    // ========================================================================
    // Bit Operations
    // ========================================================================

    // Check if bit is set
    bool test(uint32_t bit_index) const;

    // Set a bit (may need to split runs)
    void set(uint32_t bit_index);

    // Clear a bit
    void clear(uint32_t bit_index);

    // Toggle a bit
    void flip(uint32_t bit_index);

    // ========================================================================
    // Bitwise Operations (return new RLEBitmap)
    // ========================================================================

    RLEBitmap operator&(const RLEBitmap& other) const;  // AND
    RLEBitmap operator|(const RLEBitmap& other) const;  // OR
    RLEBitmap operator^(const RLEBitmap& other) const;  // XOR
    RLEBitmap operator~() const;                         // NOT

    // In-place operations
    RLEBitmap& operator&=(const RLEBitmap& other);
    RLEBitmap& operator|=(const RLEBitmap& other);
    RLEBitmap& operator^=(const RLEBitmap& other);

    // ========================================================================
    // Query Operations
    // ========================================================================

    // Count set bits
    uint32_t popcount() const;

    // Find first set bit (returns -1 if none)
    int32_t findFirst() const;

    // Find next set bit after index
    int32_t findNext(uint32_t after) const;

    // Check if all bits are zero
    bool empty() const;

    // Check if all bits are one
    bool full() const;

    // ========================================================================
    // Iteration
    // ========================================================================

    // Iterator over set bit positions
    class SetBitIterator {
    public:
        SetBitIterator(const RLEBitmap* bitmap, uint32_t pos);

        uint32_t operator*() const { return current_bit_; }
        SetBitIterator& operator++();
        bool operator!=(const SetBitIterator& other) const;

    private:
        const RLEBitmap* bitmap_;
        size_t run_index_;
        uint32_t bit_in_run_;
        uint32_t current_bit_;

        void advanceToNextSetBit();
    };

    SetBitIterator begin() const;
    SetBitIterator end() const;

    // ========================================================================
    // Accessors
    // ========================================================================

    // Get total number of bits
    uint32_t size() const { return total_bits_; }

    // Get number of runs
    size_t runCount() const { return runs_.size(); }

    // Get runs (for serialization)
    const std::vector<RLERun>& runs() const { return runs_; }

    // Get compression statistics
    BitmapRLEStats stats() const;

    // ========================================================================
    // Serialization
    // ========================================================================

    // Serialize to bytes
    std::vector<uint8_t> serialize() const;

    // Deserialize from bytes
    static RLEBitmap deserialize(std::span<const uint8_t> data);

    // Serialized size estimate
    size_t serializedSize() const;

private:
    std::vector<RLERun> runs_;
    uint32_t total_bits_ = 0;

    // Optimize runs by merging adjacent runs of same type
    void optimize();

    // Split a run at the given bit offset
    void splitRunAt(uint32_t bit_index);

    // Get run containing bit index
    size_t findRunForBit(uint32_t bit_index, uint32_t* bit_offset) const;
};

// ============================================================================
// Word-Aligned Hybrid (WAH) Compression
// ============================================================================

// More sophisticated compression for larger bitmaps
class WAHBitmap {
public:
    // WAH uses 32-bit words
    static constexpr uint32_t WORD_BITS = 31;  // 1 bit for type
    static constexpr uint32_t MAX_RUN_LENGTH = (1u << 30) - 1;

    WAHBitmap() = default;

    // Compress from uncompressed bitmap
    static WAHBitmap compress(std::span<const uint32_t> bitmap);

    // Decompress
    std::vector<uint32_t> decompress() const;

    // Bitwise operations
    WAHBitmap operator&(const WAHBitmap& other) const;
    WAHBitmap operator|(const WAHBitmap& other) const;

    // Query
    uint32_t popcount() const;
    bool test(uint32_t bit_index) const;

    // Size info
    size_t compressedWords() const { return words_.size(); }
    size_t originalWords() const { return original_word_count_; }

    double compressionRatio() const {
        return original_word_count_ > 0 ?
               static_cast<double>(words_.size()) / original_word_count_ : 1.0;
    }

private:
    std::vector<uint32_t> words_;
    size_t original_word_count_ = 0;

    // WAH word format:
    // Bit 31 = 0: literal word (bits 0-30 are data)
    // Bit 31 = 1: fill word
    //   Bit 30 = fill bit (0 or 1)
    //   Bits 0-29 = run length in words

    static bool isFill(uint32_t word) { return (word >> 31) != 0; }
    static bool fillBit(uint32_t word) { return ((word >> 30) & 1) != 0; }
    static uint32_t fillLength(uint32_t word) { return word & 0x3FFFFFFF; }
    static uint32_t makeFill(bool bit, uint32_t length) {
        return (1u << 31) | (static_cast<uint32_t>(bit) << 30) | length;
    }
};

// ============================================================================
// Roaring Bitmap Integration
// ============================================================================

// Simplified Roaring-style bitmap for very large sparse bitmaps
class RoaringBitmap {
public:
    // Roaring uses 16-bit chunks (65536 bits each)
    static constexpr uint32_t CHUNK_BITS = 65536;
    static constexpr uint32_t ARRAY_CONTAINER_THRESHOLD = 4096;

    RoaringBitmap() = default;

    // Add a value
    void add(uint32_t value);

    // Remove a value
    void remove(uint32_t value);

    // Check if value exists
    bool contains(uint32_t value) const;

    // Bitwise operations
    RoaringBitmap operator&(const RoaringBitmap& other) const;
    RoaringBitmap operator|(const RoaringBitmap& other) const;

    // Count
    uint32_t cardinality() const;

    // Iteration
    std::vector<uint32_t> toArray() const;

private:
    // Container types
    enum class ContainerType : uint8_t {
        ARRAY,    // Sorted array of 16-bit values
        BITMAP,   // 8KB bitmap
        RUN,      // Run-length encoded
    };

    struct Container {
        ContainerType type;
        std::vector<uint16_t> array;  // For ARRAY type
        std::vector<uint64_t> bitmap; // For BITMAP type (1024 words)
        std::vector<std::pair<uint16_t, uint16_t>> runs; // For RUN type
    };

    // Chunks indexed by high 16 bits
    std::map<uint16_t, Container> chunks_;

    Container& getOrCreateChunk(uint16_t high);
    void optimizeContainer(Container& container);
};

// ============================================================================
// Factory Functions
// ============================================================================

// Choose best compression based on bitmap characteristics
enum class BitmapCompressionType {
    RLE,      // Simple RLE - good for very sparse/dense
    WAH,      // Word-aligned hybrid - good for medium density
    ROARING,  // Roaring - good for large sparse bitmaps
    NONE,     // No compression
};

BitmapCompressionType chooseBestCompression(
    std::span<const uint8_t> bitmap,
    size_t set_bit_count);

} // namespace scratchbird::index
