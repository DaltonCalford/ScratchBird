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
// P3-5: DECIMAL Fixed-Point Implementation
//
// High-performance fixed-point decimal arithmetic using int128_t with scale tracking.
// Provides 10-100x faster performance than string-based implementation.
//
// Supports:
// - DECIMAL(precision, scale) with precision up to 38 digits
// - Full arithmetic: add, subtract, multiply, divide
// - Comparison operators
// - Rounding modes: HALF_UP, HALF_DOWN, HALF_EVEN (banker's rounding), CEILING, FLOOR, TRUNCATE
// - Overflow detection
// - String parsing and formatting
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <string>
#include <cstdint>
#include <limits>
#include <optional>
#include <array>
#include <vector>

namespace scratchbird::core {

// 128-bit integer for high precision
#ifdef __SIZEOF_INT128__
using int128_t = __int128;
using uint128_t = unsigned __int128;
#else
// Fallback for platforms without native int128
struct int128_t {
    int64_t high;
    uint64_t low;
    // ... implementation would go here
};
#endif

// Rounding modes for decimal operations
enum class DecimalRoundingMode : uint8_t {
    HALF_UP = 0,      // Round towards nearest, ties away from zero
    HALF_DOWN = 1,    // Round towards nearest, ties towards zero
    HALF_EVEN = 2,    // Round towards nearest, ties to even (banker's rounding)
    CEILING = 3,      // Round towards positive infinity
    FLOOR = 4,        // Round towards negative infinity
    TRUNCATE = 5,     // Round towards zero (truncate)
    UNNECESSARY = 6   // Assert that no rounding is needed
};

// Maximum precision supported (38 digits like Oracle/PostgreSQL)
constexpr uint8_t DECIMAL_MAX_PRECISION = 38;
constexpr uint8_t DECIMAL_MAX_SCALE = 38;

// Precomputed powers of 10
extern const std::array<int128_t, 39> POWERS_OF_10;

// Fixed-point decimal number
class Decimal {
public:
    // Constructors
    Decimal();
    Decimal(int64_t value);
    Decimal(int64_t value, uint8_t precision, uint8_t scale);
    Decimal(double value, uint8_t precision, uint8_t scale);
    Decimal(const std::string& str, uint8_t precision = 0, uint8_t scale = 0);
    Decimal(int128_t unscaled_value, uint8_t precision, uint8_t scale);

    // Copy/move
    Decimal(const Decimal& other) = default;
    Decimal(Decimal&& other) noexcept = default;
    Decimal& operator=(const Decimal& other) = default;
    Decimal& operator=(Decimal&& other) noexcept = default;

    // Accessors
    int128_t unscaledValue() const { return value_; }
    uint8_t precision() const { return precision_; }
    uint8_t scale() const { return scale_; }
    bool isZero() const { return value_ == 0; }
    bool isNegative() const { return value_ < 0; }
    bool isPositive() const { return value_ > 0; }

    // Arithmetic operations (return new Decimal)
    Decimal add(const Decimal& other, DecimalRoundingMode mode = DecimalRoundingMode::HALF_UP) const;
    Decimal subtract(const Decimal& other, DecimalRoundingMode mode = DecimalRoundingMode::HALF_UP) const;
    Decimal multiply(const Decimal& other, DecimalRoundingMode mode = DecimalRoundingMode::HALF_UP) const;
    Decimal divide(const Decimal& other, uint8_t result_scale,
                   DecimalRoundingMode mode = DecimalRoundingMode::HALF_UP) const;

    // Unary operations
    Decimal negate() const;
    Decimal abs() const;

    // Rescale to different precision/scale
    Decimal rescale(uint8_t new_precision, uint8_t new_scale,
                    DecimalRoundingMode mode = DecimalRoundingMode::HALF_UP) const;

    // Round to scale
    Decimal round(uint8_t target_scale, DecimalRoundingMode mode = DecimalRoundingMode::HALF_UP) const;

    // Comparison operators
    int compare(const Decimal& other) const;
    bool operator==(const Decimal& other) const { return compare(other) == 0; }
    bool operator!=(const Decimal& other) const { return compare(other) != 0; }
    bool operator<(const Decimal& other) const { return compare(other) < 0; }
    bool operator<=(const Decimal& other) const { return compare(other) <= 0; }
    bool operator>(const Decimal& other) const { return compare(other) > 0; }
    bool operator>=(const Decimal& other) const { return compare(other) >= 0; }

    // Arithmetic operators
    Decimal operator+(const Decimal& other) const { return add(other); }
    Decimal operator-(const Decimal& other) const { return subtract(other); }
    Decimal operator*(const Decimal& other) const { return multiply(other); }
    Decimal operator-() const { return negate(); }

    // Conversion
    int64_t toInt64() const;
    double toDouble() const;
    std::string toString() const;
    std::string toStringWithPrecision(uint8_t display_scale) const;

    // Parsing
    static std::optional<Decimal> parse(const std::string& str,
                                        uint8_t precision = 0,
                                        uint8_t scale = 0);
    static Status parseWithError(const std::string& str,
                                 uint8_t precision,
                                 uint8_t scale,
                                 Decimal* result_out,
                                 ErrorContext* ctx = nullptr);

    // Special values
    static Decimal zero(uint8_t precision = 1, uint8_t scale = 0);
    static Decimal one(uint8_t precision = 1, uint8_t scale = 0);
    static Decimal minValue(uint8_t precision, uint8_t scale);
    static Decimal maxValue(uint8_t precision, uint8_t scale);

    // Serialization (for storage)
    std::vector<uint8_t> serialize() const;
    static Decimal deserialize(const uint8_t* data, size_t len);

    // Hash for use in hash tables
    size_t hash() const;

private:
    int128_t value_;       // Unscaled integer value
    uint8_t precision_;    // Total digits (1-38)
    uint8_t scale_;        // Decimal places (0-precision)

    // Helper to align scales for arithmetic
    static void alignScales(const Decimal& a, const Decimal& b,
                           int128_t& out_a, int128_t& out_b, uint8_t& out_scale);

    // Helper to round during scale reduction
    static int128_t roundValue(int128_t value, int128_t divisor, DecimalRoundingMode mode);

    // Check for overflow
    static bool wouldOverflow(int128_t value, uint8_t precision);

    // Get maximum value for given precision
    static int128_t maxForPrecision(uint8_t precision);
};

// Decimal arithmetic context (for consistent settings across operations)
struct DecimalContext {
    uint8_t result_precision = DECIMAL_MAX_PRECISION;
    uint8_t result_scale = 6;
    DecimalRoundingMode rounding = DecimalRoundingMode::HALF_EVEN;
    bool overflow_check = true;

    static DecimalContext defaultContext() {
        return DecimalContext{};
    }

    static DecimalContext moneyContext() {
        return DecimalContext{19, 4, DecimalRoundingMode::HALF_UP, true};
    }

    static DecimalContext scientificContext() {
        return DecimalContext{38, 15, DecimalRoundingMode::HALF_EVEN, true};
    }
};

// Aggregate functions for DECIMAL
class DecimalAggregator {
public:
    DecimalAggregator(uint8_t precision = DECIMAL_MAX_PRECISION,
                      uint8_t scale = 6);

    void add(const Decimal& value);
    void reset();

    Decimal sum() const;
    Decimal avg() const;
    Decimal min() const;
    Decimal max() const;
    uint64_t count() const { return count_; }

private:
    Decimal sum_;
    Decimal min_;
    Decimal max_;
    uint64_t count_ = 0;
    bool has_value_ = false;
    uint8_t precision_;
    uint8_t scale_;
};

} // namespace scratchbird::core

// Hash specialization for std::unordered_map
namespace std {
template<>
struct hash<scratchbird::core::Decimal> {
    size_t operator()(const scratchbird::core::Decimal& d) const {
        return d.hash();
    }
};
}
