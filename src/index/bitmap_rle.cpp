// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-10: Bitmap RLE Compression Implementation
//
// November 25, 2025

#include "scratchbird/index/bitmap_rle.h"
#include <algorithm>
#include <cstring>
#include <bit>

namespace scratchbird::index {

// ============================================================================
// RLEBitmap Implementation
// ============================================================================

RLEBitmap::RLEBitmap(std::span<const uint8_t> bitmap)
    : RLEBitmap(compress(bitmap)) {
}

RLEBitmap::RLEBitmap(std::span<const uint64_t> bitmap)
    : RLEBitmap(compress(bitmap)) {
}

RLEBitmap::RLEBitmap(std::vector<RLERun>&& runs, uint32_t total_bits)
    : runs_(std::move(runs)), total_bits_(total_bits) {
    optimize();
}

RLEBitmap RLEBitmap::compress(std::span<const uint8_t> bitmap) {
    if (bitmap.empty()) {
        return RLEBitmap();
    }

    std::vector<RLERun> runs;
    uint32_t total_bits = bitmap.size() * 8;

    RLERunType current_type = RLERunType::ZEROS;
    uint32_t current_length = 0;

    for (size_t byte_idx = 0; byte_idx < bitmap.size(); ++byte_idx) {
        uint8_t byte = bitmap[byte_idx];

        for (int bit = 0; bit < 8; ++bit) {
            bool is_set = (byte >> bit) & 1;
            RLERunType bit_type = is_set ? RLERunType::ONES : RLERunType::ZEROS;

            if (current_length == 0) {
                current_type = bit_type;
                current_length = 1;
            } else if (bit_type == current_type) {
                current_length++;
            } else {
                // End current run, start new one
                if (current_type == RLERunType::ZEROS) {
                    runs.push_back(RLERun::zeros(current_length));
                } else {
                    runs.push_back(RLERun::ones(current_length));
                }
                current_type = bit_type;
                current_length = 1;
            }
        }
    }

    // Don't forget last run
    if (current_length > 0) {
        if (current_type == RLERunType::ZEROS) {
            runs.push_back(RLERun::zeros(current_length));
        } else {
            runs.push_back(RLERun::ones(current_length));
        }
    }

    return RLEBitmap(std::move(runs), total_bits);
}

RLEBitmap RLEBitmap::compress(std::span<const uint64_t> bitmap) {
    if (bitmap.empty()) {
        return RLEBitmap();
    }

    std::vector<RLERun> runs;
    uint32_t total_bits = bitmap.size() * 64;

    RLERunType current_type = RLERunType::ZEROS;
    uint32_t current_length = 0;

    for (uint64_t word : bitmap) {
        // Optimize: check for all-zeros or all-ones words
        if (word == 0) {
            if (current_type == RLERunType::ZEROS) {
                current_length += 64;
            } else {
                if (current_length > 0) {
                    runs.push_back(RLERun::ones(current_length));
                }
                current_type = RLERunType::ZEROS;
                current_length = 64;
            }
            continue;
        }

        if (word == UINT64_MAX) {
            if (current_type == RLERunType::ONES) {
                current_length += 64;
            } else {
                if (current_length > 0) {
                    runs.push_back(RLERun::zeros(current_length));
                }
                current_type = RLERunType::ONES;
                current_length = 64;
            }
            continue;
        }

        // Mixed word - process bit by bit
        for (int bit = 0; bit < 64; ++bit) {
            bool is_set = (word >> bit) & 1;
            RLERunType bit_type = is_set ? RLERunType::ONES : RLERunType::ZEROS;

            if (current_length == 0) {
                current_type = bit_type;
                current_length = 1;
            } else if (bit_type == current_type) {
                current_length++;
            } else {
                if (current_type == RLERunType::ZEROS) {
                    runs.push_back(RLERun::zeros(current_length));
                } else {
                    runs.push_back(RLERun::ones(current_length));
                }
                current_type = bit_type;
                current_length = 1;
            }
        }
    }

    if (current_length > 0) {
        if (current_type == RLERunType::ZEROS) {
            runs.push_back(RLERun::zeros(current_length));
        } else {
            runs.push_back(RLERun::ones(current_length));
        }
    }

    return RLEBitmap(std::move(runs), total_bits);
}

std::vector<uint8_t> RLEBitmap::decompress() const {
    size_t num_bytes = (total_bits_ + 7) / 8;
    std::vector<uint8_t> result(num_bytes, 0);

    uint32_t bit_pos = 0;
    for (const auto& run : runs_) {
        if (run.type == RLERunType::ONES) {
            // Set bits in this run
            for (uint32_t i = 0; i < run.length; ++i) {
                uint32_t byte_idx = (bit_pos + i) / 8;
                uint32_t bit_idx = (bit_pos + i) % 8;
                if (byte_idx < result.size()) {
                    result[byte_idx] |= (1 << bit_idx);
                }
            }
        } else if (run.type == RLERunType::LITERAL) {
            // Copy literal bits
            for (uint32_t i = 0; i < run.length && i < 64; ++i) {
                if ((run.literal >> i) & 1) {
                    uint32_t byte_idx = (bit_pos + i) / 8;
                    uint32_t bit_idx = (bit_pos + i) % 8;
                    if (byte_idx < result.size()) {
                        result[byte_idx] |= (1 << bit_idx);
                    }
                }
            }
        }
        // ZEROS: already initialized to 0
        bit_pos += run.length;
    }

    return result;
}

std::vector<uint64_t> RLEBitmap::decompressWords() const {
    size_t num_words = (total_bits_ + 63) / 64;
    std::vector<uint64_t> result(num_words, 0);

    uint32_t bit_pos = 0;
    for (const auto& run : runs_) {
        if (run.type == RLERunType::ONES) {
            for (uint32_t i = 0; i < run.length; ++i) {
                uint32_t word_idx = (bit_pos + i) / 64;
                uint32_t bit_idx = (bit_pos + i) % 64;
                if (word_idx < result.size()) {
                    result[word_idx] |= (1ULL << bit_idx);
                }
            }
        } else if (run.type == RLERunType::LITERAL) {
            uint32_t word_idx = bit_pos / 64;
            uint32_t bit_idx = bit_pos % 64;
            if (word_idx < result.size()) {
                result[word_idx] |= (run.literal << bit_idx);
                if (bit_idx > 0 && word_idx + 1 < result.size()) {
                    result[word_idx + 1] |= (run.literal >> (64 - bit_idx));
                }
            }
        }
        bit_pos += run.length;
    }

    return result;
}

bool RLEBitmap::test(uint32_t bit_index) const {
    if (bit_index >= total_bits_) return false;

    uint32_t bit_offset;
    size_t run_idx = findRunForBit(bit_index, &bit_offset);

    if (run_idx >= runs_.size()) return false;

    const auto& run = runs_[run_idx];
    switch (run.type) {
        case RLERunType::ZEROS:
            return false;
        case RLERunType::ONES:
            return true;
        case RLERunType::LITERAL:
            return (run.literal >> bit_offset) & 1;
        default:
            return false;
    }
}

void RLEBitmap::set(uint32_t bit_index) {
    if (bit_index >= total_bits_) return;

    // Simple implementation: decompress, set, recompress
    // A more efficient implementation would modify runs in-place
    auto decompressed = decompress();
    uint32_t byte_idx = bit_index / 8;
    uint32_t bit_idx = bit_index % 8;
    decompressed[byte_idx] |= (1 << bit_idx);

    *this = compress(decompressed);
}

void RLEBitmap::clear(uint32_t bit_index) {
    if (bit_index >= total_bits_) return;

    auto decompressed = decompress();
    uint32_t byte_idx = bit_index / 8;
    uint32_t bit_idx = bit_index % 8;
    decompressed[byte_idx] &= ~(1 << bit_idx);

    *this = compress(decompressed);
}

void RLEBitmap::flip(uint32_t bit_index) {
    if (test(bit_index)) {
        clear(bit_index);
    } else {
        set(bit_index);
    }
}

RLEBitmap RLEBitmap::operator&(const RLEBitmap& other) const {
    auto a = decompressWords();
    auto b = other.decompressWords();

    size_t min_size = std::min(a.size(), b.size());
    std::vector<uint64_t> result(min_size);

    for (size_t i = 0; i < min_size; ++i) {
        result[i] = a[i] & b[i];
    }

    return compress(result);
}

RLEBitmap RLEBitmap::operator|(const RLEBitmap& other) const {
    auto a = decompressWords();
    auto b = other.decompressWords();

    size_t max_size = std::max(a.size(), b.size());
    a.resize(max_size, 0);
    b.resize(max_size, 0);

    std::vector<uint64_t> result(max_size);
    for (size_t i = 0; i < max_size; ++i) {
        result[i] = a[i] | b[i];
    }

    return compress(result);
}

RLEBitmap RLEBitmap::operator^(const RLEBitmap& other) const {
    auto a = decompressWords();
    auto b = other.decompressWords();

    size_t max_size = std::max(a.size(), b.size());
    a.resize(max_size, 0);
    b.resize(max_size, 0);

    std::vector<uint64_t> result(max_size);
    for (size_t i = 0; i < max_size; ++i) {
        result[i] = a[i] ^ b[i];
    }

    return compress(result);
}

RLEBitmap RLEBitmap::operator~() const {
    auto words = decompressWords();
    for (auto& w : words) {
        w = ~w;
    }
    return compress(words);
}

RLEBitmap& RLEBitmap::operator&=(const RLEBitmap& other) {
    *this = *this & other;
    return *this;
}

RLEBitmap& RLEBitmap::operator|=(const RLEBitmap& other) {
    *this = *this | other;
    return *this;
}

RLEBitmap& RLEBitmap::operator^=(const RLEBitmap& other) {
    *this = *this ^ other;
    return *this;
}

uint32_t RLEBitmap::popcount() const {
    uint32_t count = 0;
    for (const auto& run : runs_) {
        switch (run.type) {
            case RLERunType::ONES:
                count += run.length;
                break;
            case RLERunType::LITERAL:
                count += std::popcount(run.literal);
                break;
            default:
                break;
        }
    }
    return count;
}

int32_t RLEBitmap::findFirst() const {
    uint32_t bit_pos = 0;
    for (const auto& run : runs_) {
        if (run.type == RLERunType::ONES) {
            return static_cast<int32_t>(bit_pos);
        } else if (run.type == RLERunType::LITERAL) {
            if (run.literal != 0) {
                return static_cast<int32_t>(bit_pos + std::countr_zero(run.literal));
            }
        }
        bit_pos += run.length;
    }
    return -1;
}

int32_t RLEBitmap::findNext(uint32_t after) const {
    if (after + 1 >= total_bits_) return -1;

    uint32_t target = after + 1;
    uint32_t bit_pos = 0;

    for (const auto& run : runs_) {
        uint32_t run_end = bit_pos + run.length;

        if (run_end <= target) {
            bit_pos = run_end;
            continue;
        }

        if (run.type == RLERunType::ONES) {
            if (bit_pos >= target) {
                return static_cast<int32_t>(bit_pos);
            } else {
                return static_cast<int32_t>(target);
            }
        } else if (run.type == RLERunType::LITERAL) {
            uint32_t start_offset = (target > bit_pos) ? (target - bit_pos) : 0;
            uint64_t mask = run.literal >> start_offset;
            if (mask != 0) {
                return static_cast<int32_t>(bit_pos + start_offset + std::countr_zero(mask));
            }
        }

        bit_pos = run_end;
    }

    return -1;
}

bool RLEBitmap::empty() const {
    for (const auto& run : runs_) {
        if (run.type == RLERunType::ONES) return false;
        if (run.type == RLERunType::LITERAL && run.literal != 0) return false;
    }
    return true;
}

bool RLEBitmap::full() const {
    for (const auto& run : runs_) {
        if (run.type == RLERunType::ZEROS) return false;
        if (run.type == RLERunType::LITERAL && run.literal != UINT64_MAX) return false;
    }
    return true;
}

BitmapRLEStats RLEBitmap::stats() const {
    BitmapRLEStats s;
    s.total_bits = total_bits_;
    s.original_bytes = (total_bits_ + 7) / 8;
    s.run_count = runs_.size();

    for (const auto& run : runs_) {
        switch (run.type) {
            case RLERunType::ZEROS:
                s.zero_runs++;
                break;
            case RLERunType::ONES:
                s.one_runs++;
                break;
            case RLERunType::LITERAL:
                s.literal_runs++;
                break;
            default:
                break;
        }
    }

    s.compressed_bytes = serializedSize();
    return s;
}

std::vector<uint8_t> RLEBitmap::serialize() const {
    std::vector<uint8_t> result;

    // Header: total_bits (4 bytes) + run_count (4 bytes)
    result.resize(8);
    std::memcpy(result.data(), &total_bits_, 4);
    uint32_t run_count = runs_.size();
    std::memcpy(result.data() + 4, &run_count, 4);

    // Each run: type (1 byte) + length (4 bytes) + literal if needed (8 bytes)
    for (const auto& run : runs_) {
        result.push_back(static_cast<uint8_t>(run.type));

        size_t offset = result.size();
        result.resize(offset + 4);
        std::memcpy(result.data() + offset, &run.length, 4);

        if (run.type == RLERunType::LITERAL) {
            offset = result.size();
            result.resize(offset + 8);
            std::memcpy(result.data() + offset, &run.literal, 8);
        }
    }

    return result;
}

RLEBitmap RLEBitmap::deserialize(std::span<const uint8_t> data) {
    if (data.size() < 8) {
        return RLEBitmap();
    }

    uint32_t total_bits;
    uint32_t run_count;
    std::memcpy(&total_bits, data.data(), 4);
    std::memcpy(&run_count, data.data() + 4, 4);

    std::vector<RLERun> runs;
    runs.reserve(run_count);

    size_t offset = 8;
    for (uint32_t i = 0; i < run_count && offset < data.size(); ++i) {
        RLERun run;
        run.type = static_cast<RLERunType>(data[offset++]);

        if (offset + 4 > data.size()) break;
        std::memcpy(&run.length, data.data() + offset, 4);
        offset += 4;

        if (run.type == RLERunType::LITERAL) {
            if (offset + 8 > data.size()) break;
            std::memcpy(&run.literal, data.data() + offset, 8);
            offset += 8;
        } else {
            run.literal = 0;
        }

        runs.push_back(run);
    }

    return RLEBitmap(std::move(runs), total_bits);
}

size_t RLEBitmap::serializedSize() const {
    size_t size = 8;  // Header
    for (const auto& run : runs_) {
        size += 5;  // type + length
        if (run.type == RLERunType::LITERAL) {
            size += 8;
        }
    }
    return size;
}

void RLEBitmap::optimize() {
    if (runs_.size() < 2) return;

    std::vector<RLERun> optimized;
    optimized.push_back(runs_[0]);

    for (size_t i = 1; i < runs_.size(); ++i) {
        auto& last = optimized.back();
        const auto& current = runs_[i];

        // Merge adjacent runs of same type
        if (last.type == current.type &&
            (last.type == RLERunType::ZEROS || last.type == RLERunType::ONES)) {
            last.length += current.length;
        } else {
            optimized.push_back(current);
        }
    }

    runs_ = std::move(optimized);
}

size_t RLEBitmap::findRunForBit(uint32_t bit_index, uint32_t* bit_offset) const {
    uint32_t pos = 0;
    for (size_t i = 0; i < runs_.size(); ++i) {
        if (pos + runs_[i].length > bit_index) {
            if (bit_offset) *bit_offset = bit_index - pos;
            return i;
        }
        pos += runs_[i].length;
    }
    return runs_.size();
}

// ============================================================================
// SetBitIterator Implementation
// ============================================================================

RLEBitmap::SetBitIterator::SetBitIterator(const RLEBitmap* bitmap, uint32_t pos)
    : bitmap_(bitmap), run_index_(0), bit_in_run_(0), current_bit_(pos) {
    if (pos < bitmap_->total_bits_) {
        advanceToNextSetBit();
    }
}

RLEBitmap::SetBitIterator& RLEBitmap::SetBitIterator::operator++() {
    current_bit_++;
    advanceToNextSetBit();
    return *this;
}

bool RLEBitmap::SetBitIterator::operator!=(const SetBitIterator& other) const {
    return current_bit_ != other.current_bit_;
}

void RLEBitmap::SetBitIterator::advanceToNextSetBit() {
    while (current_bit_ < bitmap_->total_bits_) {
        if (bitmap_->test(current_bit_)) {
            return;
        }
        current_bit_++;
    }
    current_bit_ = bitmap_->total_bits_;  // End sentinel
}

RLEBitmap::SetBitIterator RLEBitmap::begin() const {
    return SetBitIterator(this, 0);
}

RLEBitmap::SetBitIterator RLEBitmap::end() const {
    return SetBitIterator(this, total_bits_);
}

// ============================================================================
// WAHBitmap Implementation
// ============================================================================

WAHBitmap WAHBitmap::compress(std::span<const uint32_t> bitmap) {
    WAHBitmap result;
    result.original_word_count_ = bitmap.size();

    if (bitmap.empty()) return result;

    // Process 31 bits at a time (WAH word size)
    size_t bit_pos = 0;
    size_t total_bits = bitmap.size() * 32;

    while (bit_pos < total_bits) {
        // Extract 31 bits
        size_t word_idx = bit_pos / 32;
        size_t bit_in_word = bit_pos % 32;

        uint32_t bits = 0;
        if (word_idx < bitmap.size()) {
            bits = bitmap[word_idx] >> bit_in_word;
            if (bit_in_word > 1 && word_idx + 1 < bitmap.size()) {
                bits |= bitmap[word_idx + 1] << (32 - bit_in_word);
            }
        }
        bits &= 0x7FFFFFFF;  // Only 31 bits

        // Check for fill pattern
        if (bits == 0 || bits == 0x7FFFFFFF) {
            bool fill_bit = (bits != 0);
            uint32_t run_length = 1;

            // Count consecutive fill words
            bit_pos += WORD_BITS;
            while (bit_pos < total_bits && run_length < MAX_RUN_LENGTH) {
                word_idx = bit_pos / 32;
                bit_in_word = bit_pos % 32;

                uint32_t next_bits = 0;
                if (word_idx < bitmap.size()) {
                    next_bits = bitmap[word_idx] >> bit_in_word;
                    if (bit_in_word > 1 && word_idx + 1 < bitmap.size()) {
                        next_bits |= bitmap[word_idx + 1] << (32 - bit_in_word);
                    }
                }
                next_bits &= 0x7FFFFFFF;

                if ((fill_bit && next_bits == 0x7FFFFFFF) ||
                    (!fill_bit && next_bits == 0)) {
                    run_length++;
                    bit_pos += WORD_BITS;
                } else {
                    break;
                }
            }

            result.words_.push_back(makeFill(fill_bit, run_length));
        } else {
            // Literal word
            result.words_.push_back(bits);
            bit_pos += WORD_BITS;
        }
    }

    return result;
}

std::vector<uint32_t> WAHBitmap::decompress() const {
    std::vector<uint32_t> result;

    size_t bit_pos = 0;
    for (uint32_t word : words_) {
        if (isFill(word)) {
            bool fill = fillBit(word);
            uint32_t length = fillLength(word);
            uint32_t fill_bits = fill ? 0x7FFFFFFF : 0;

            for (uint32_t i = 0; i < length; ++i) {
                // Write 31 bits
                size_t word_idx = bit_pos / 32;
                size_t bit_in_word = bit_pos % 32;

                while (result.size() <= word_idx + 1) {
                    result.push_back(0);
                }

                result[word_idx] |= (fill_bits << bit_in_word);
                if (bit_in_word > 1) {
                    result[word_idx + 1] |= (fill_bits >> (32 - bit_in_word));
                }

                bit_pos += WORD_BITS;
            }
        } else {
            // Literal word
            size_t word_idx = bit_pos / 32;
            size_t bit_in_word = bit_pos % 32;

            while (result.size() <= word_idx + 1) {
                result.push_back(0);
            }

            result[word_idx] |= (word << bit_in_word);
            if (bit_in_word > 1) {
                result[word_idx + 1] |= (word >> (32 - bit_in_word));
            }

            bit_pos += WORD_BITS;
        }
    }

    // Trim to original size
    result.resize(original_word_count_);
    return result;
}

uint32_t WAHBitmap::popcount() const {
    uint32_t count = 0;
    for (uint32_t word : words_) {
        if (isFill(word)) {
            if (fillBit(word)) {
                count += WORD_BITS * fillLength(word);
            }
        } else {
            count += std::popcount(word);
        }
    }
    return count;
}

// ============================================================================
// Compression Selection
// ============================================================================

BitmapCompressionType chooseBestCompression(
    std::span<const uint8_t> bitmap,
    size_t set_bit_count) {

    size_t total_bits = bitmap.size() * 8;
    if (total_bits == 0) return BitmapCompressionType::NONE;

    double density = static_cast<double>(set_bit_count) / total_bits;

    // Very sparse or very dense: RLE works well
    if (density < 0.01 || density > 0.99) {
        return BitmapCompressionType::RLE;
    }

    // Large sparse bitmaps: Roaring is best
    if (total_bits > 1000000 && density < 0.1) {
        return BitmapCompressionType::ROARING;
    }

    // Medium density: WAH
    if (density >= 0.01 && density <= 0.5) {
        return BitmapCompressionType::WAH;
    }

    // Default to RLE
    return BitmapCompressionType::RLE;
}

} // namespace scratchbird::index
