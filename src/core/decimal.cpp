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
// November 25, 2025

#include "scratchbird/core/decimal.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <vector>

namespace scratchbird::core {

// Precomputed powers of 10
const std::array<int128_t, 39> POWERS_OF_10 = []() {
    std::array<int128_t, 39> result{};
    result[0] = 1;
    for (size_t i = 1; i < 39; ++i) {
        result[i] = result[i - 1] * 10;
    }
    return result;
}();

// ============================================================================
// Decimal Implementation
// ============================================================================

Decimal::Decimal() : value_(0), precision_(1), scale_(0) {}

Decimal::Decimal(int64_t value) : value_(value), precision_(19), scale_(0) {}

Decimal::Decimal(int64_t value, uint8_t precision, uint8_t scale)
    : precision_(precision), scale_(scale) {
    if (scale > 0 && scale <= DECIMAL_MAX_SCALE) {
        value_ = static_cast<int128_t>(value) * POWERS_OF_10[scale];
    } else {
        value_ = value;
    }
}

Decimal::Decimal(double value, uint8_t precision, uint8_t scale)
    : precision_(precision), scale_(scale) {
    // Convert double to scaled integer
    double scaled = value * static_cast<double>(POWERS_OF_10[scale]);
    value_ = static_cast<int128_t>(std::llround(scaled));
}

Decimal::Decimal(const std::string& str, uint8_t precision, uint8_t scale) {
    auto result = parse(str, precision, scale);
    if (result) {
        *this = *result;
    } else {
        value_ = 0;
        precision_ = 1;
        scale_ = 0;
    }
}

Decimal::Decimal(int128_t unscaled_value, uint8_t precision, uint8_t scale)
    : value_(unscaled_value), precision_(precision), scale_(scale) {}

// ============================================================================
// Arithmetic Operations
// ============================================================================

void Decimal::alignScales(const Decimal& a, const Decimal& b,
                          int128_t& out_a, int128_t& out_b, uint8_t& out_scale) {
    if (a.scale_ == b.scale_) {
        out_a = a.value_;
        out_b = b.value_;
        out_scale = a.scale_;
    } else if (a.scale_ > b.scale_) {
        out_a = a.value_;
        out_b = b.value_ * POWERS_OF_10[a.scale_ - b.scale_];
        out_scale = a.scale_;
    } else {
        out_a = a.value_ * POWERS_OF_10[b.scale_ - a.scale_];
        out_b = b.value_;
        out_scale = b.scale_;
    }
}

int128_t Decimal::roundValue(int128_t value, int128_t divisor, DecimalRoundingMode mode) {
    if (divisor == 0) return value;

    int128_t quotient = value / divisor;
    int128_t remainder = value % divisor;

    if (remainder == 0) {
        return quotient;
    }

    bool is_negative = value < 0;
    int128_t abs_remainder = is_negative ? -remainder : remainder;
    int128_t half_divisor = divisor / 2;

    switch (mode) {
        case DecimalRoundingMode::HALF_UP:
            if (abs_remainder >= half_divisor) {
                quotient += is_negative ? -1 : 1;
            }
            break;

        case DecimalRoundingMode::HALF_DOWN:
            if (abs_remainder > half_divisor) {
                quotient += is_negative ? -1 : 1;
            }
            break;

        case DecimalRoundingMode::HALF_EVEN:
            if (abs_remainder > half_divisor ||
                (abs_remainder == half_divisor && (quotient & 1))) {
                quotient += is_negative ? -1 : 1;
            }
            break;

        case DecimalRoundingMode::CEILING:
            if (!is_negative && remainder != 0) {
                quotient++;
            }
            break;

        case DecimalRoundingMode::FLOOR:
            if (is_negative && remainder != 0) {
                quotient--;
            }
            break;

        case DecimalRoundingMode::TRUNCATE:
            // No adjustment needed
            break;

        case DecimalRoundingMode::UNNECESSARY:
            // Would need to throw if rounding was needed
            break;
    }

    return quotient;
}

Decimal Decimal::add(const Decimal& other, DecimalRoundingMode mode) const {
    int128_t aligned_a, aligned_b;
    uint8_t result_scale;
    alignScales(*this, other, aligned_a, aligned_b, result_scale);

    int128_t result_value = aligned_a + aligned_b;
    uint8_t result_precision = std::max(precision_, other.precision_) + 1;
    if (result_precision > DECIMAL_MAX_PRECISION) {
        result_precision = DECIMAL_MAX_PRECISION;
    }

    return Decimal(result_value, result_precision, result_scale);
}

Decimal Decimal::subtract(const Decimal& other, DecimalRoundingMode mode) const {
    int128_t aligned_a, aligned_b;
    uint8_t result_scale;
    alignScales(*this, other, aligned_a, aligned_b, result_scale);

    int128_t result_value = aligned_a - aligned_b;
    uint8_t result_precision = std::max(precision_, other.precision_) + 1;
    if (result_precision > DECIMAL_MAX_PRECISION) {
        result_precision = DECIMAL_MAX_PRECISION;
    }

    return Decimal(result_value, result_precision, result_scale);
}

Decimal Decimal::multiply(const Decimal& other, DecimalRoundingMode mode) const {
    // Multiply unscaled values and add scales
    int128_t result_value = value_ * other.value_;
    uint8_t result_scale = scale_ + other.scale_;
    uint8_t result_precision = precision_ + other.precision_;

    // Reduce scale if needed
    while (result_scale > DECIMAL_MAX_SCALE) {
        result_value = roundValue(result_value, 10, mode);
        result_scale--;
    }

    if (result_precision > DECIMAL_MAX_PRECISION) {
        result_precision = DECIMAL_MAX_PRECISION;
    }

    return Decimal(result_value, result_precision, result_scale);
}

Decimal Decimal::divide(const Decimal& other, uint8_t result_scale, DecimalRoundingMode mode) const {
    if (other.value_ == 0) {
        // Division by zero - return zero or could throw
        return Decimal(static_cast<int64_t>(0), precision_, scale_);
    }

    // Scale up dividend for precision
    uint8_t extra_scale = result_scale + 1;  // Extra digit for rounding
    int128_t scaled_dividend = value_;

    // Add extra scale to dividend
    if (scale_ < other.scale_ + result_scale + extra_scale) {
        uint8_t scale_diff = other.scale_ + result_scale + extra_scale - scale_;
        if (scale_diff <= DECIMAL_MAX_SCALE) {
            scaled_dividend *= POWERS_OF_10[scale_diff];
        }
    }

    int128_t result_value = scaled_dividend / other.value_;

    // Round to target scale
    if (extra_scale > 0) {
        result_value = roundValue(result_value, POWERS_OF_10[extra_scale], mode);
    }

    uint8_t result_precision = precision_;
    if (result_precision > DECIMAL_MAX_PRECISION) {
        result_precision = DECIMAL_MAX_PRECISION;
    }

    return Decimal(result_value, result_precision, result_scale);
}

Decimal Decimal::negate() const {
    return Decimal(-value_, precision_, scale_);
}

Decimal Decimal::abs() const {
    return Decimal(value_ < 0 ? -value_ : value_, precision_, scale_);
}

Decimal Decimal::rescale(uint8_t new_precision, uint8_t new_scale, DecimalRoundingMode mode) const {
    if (new_scale == scale_) {
        return Decimal(value_, new_precision, new_scale);
    }

    int128_t new_value;
    if (new_scale > scale_) {
        // Increase scale (multiply)
        uint8_t diff = new_scale - scale_;
        new_value = value_ * POWERS_OF_10[diff];
    } else {
        // Decrease scale (divide with rounding)
        uint8_t diff = scale_ - new_scale;
        new_value = roundValue(value_, POWERS_OF_10[diff], mode);
    }

    return Decimal(new_value, new_precision, new_scale);
}

Decimal Decimal::round(uint8_t target_scale, DecimalRoundingMode mode) const {
    if (target_scale >= scale_) {
        return *this;
    }
    return rescale(precision_, target_scale, mode);
}

// ============================================================================
// Comparison
// ============================================================================

int Decimal::compare(const Decimal& other) const {
    int128_t aligned_a, aligned_b;
    uint8_t result_scale;
    alignScales(*this, other, aligned_a, aligned_b, result_scale);

    if (aligned_a < aligned_b) return -1;
    if (aligned_a > aligned_b) return 1;
    return 0;
}

// ============================================================================
// Conversion
// ============================================================================

int64_t Decimal::toInt64() const {
    int128_t result = value_;
    if (scale_ > 0) {
        result = roundValue(result, POWERS_OF_10[scale_], DecimalRoundingMode::TRUNCATE);
    }
    return static_cast<int64_t>(result);
}

double Decimal::toDouble() const {
    // This may lose precision for large values
    if (scale_ == 0) {
        return static_cast<double>(value_);
    }
    return static_cast<double>(value_) / static_cast<double>(POWERS_OF_10[scale_]);
}

std::string Decimal::toString() const {
    return toStringWithPrecision(scale_);
}

std::string Decimal::toStringWithPrecision(uint8_t display_scale) const {
    std::string result;

    int128_t abs_value = value_ < 0 ? -value_ : value_;
    bool negative = value_ < 0;

    // Convert to string
    if (abs_value == 0) {
        result = "0";
    } else {
        while (abs_value > 0) {
            result = static_cast<char>('0' + static_cast<int>(abs_value % 10)) + result;
            abs_value /= 10;
        }
    }

    // Pad with leading zeros if needed
    while (result.length() <= scale_) {
        result = "0" + result;
    }

    // Insert decimal point
    if (scale_ > 0) {
        size_t pos = result.length() - scale_;
        result.insert(pos, ".");
    }

    // Add negative sign
    if (negative) {
        result = "-" + result;
    }

    return result;
}

// ============================================================================
// Parsing
// ============================================================================

std::optional<Decimal> Decimal::parse(const std::string& str, uint8_t precision, uint8_t scale) {
    if (str.empty()) return std::nullopt;

    bool negative = false;
    size_t pos = 0;

    // Skip whitespace
    while (pos < str.length() && std::isspace(str[pos])) pos++;

    // Check sign
    if (pos < str.length() && str[pos] == '-') {
        negative = true;
        pos++;
    } else if (pos < str.length() && str[pos] == '+') {
        pos++;
    }

    // Parse integer part
    int128_t integer_part = 0;
    size_t int_digits = 0;
    while (pos < str.length() && std::isdigit(str[pos])) {
        integer_part = integer_part * 10 + (str[pos] - '0');
        int_digits++;
        pos++;
    }

    // Parse fractional part
    int128_t fractional_part = 0;
    size_t frac_digits = 0;
    if (pos < str.length() && str[pos] == '.') {
        pos++;
        while (pos < str.length() && std::isdigit(str[pos])) {
            fractional_part = fractional_part * 10 + (str[pos] - '0');
            frac_digits++;
            pos++;
        }
    }

    // Determine scale from input if not specified
    uint8_t input_scale = static_cast<uint8_t>(frac_digits);
    if (scale == 0) {
        scale = input_scale;
    }

    // Determine precision from input if not specified
    if (precision == 0) {
        precision = static_cast<uint8_t>(int_digits + scale);
        if (precision == 0) precision = 1;
        if (precision > DECIMAL_MAX_PRECISION) precision = DECIMAL_MAX_PRECISION;
    }

    // Build the value
    int128_t value;
    if (input_scale == scale) {
        value = integer_part * POWERS_OF_10[scale] + fractional_part;
    } else if (input_scale < scale) {
        // Need more decimal places
        value = integer_part * POWERS_OF_10[scale] +
                fractional_part * POWERS_OF_10[scale - input_scale];
    } else {
        // Need fewer decimal places (round)
        value = integer_part * POWERS_OF_10[scale] +
                roundValue(fractional_part, POWERS_OF_10[input_scale - scale],
                          DecimalRoundingMode::HALF_UP);
    }

    if (negative) {
        value = -value;
    }

    return Decimal(value, precision, scale);
}

Status Decimal::parseWithError(const std::string& str, uint8_t precision, uint8_t scale,
                               Decimal* result_out, ErrorContext* ctx) {
    auto result = parse(str, precision, scale);
    if (!result) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, ("Invalid decimal format: " + str).c_str());
        return Status::INVALID_ARGUMENT;
    }
    *result_out = *result;
    return Status::OK;
}

// ============================================================================
// Special Values
// ============================================================================

Decimal Decimal::zero(uint8_t precision, uint8_t scale) {
    return Decimal(static_cast<int128_t>(0), precision, scale);
}

Decimal Decimal::one(uint8_t precision, uint8_t scale) {
    return Decimal(POWERS_OF_10[scale], precision, scale);
}

int128_t Decimal::maxForPrecision(uint8_t precision) {
    if (precision >= 39) return POWERS_OF_10[38] - 1;
    return POWERS_OF_10[precision] - 1;
}

Decimal Decimal::minValue(uint8_t precision, uint8_t scale) {
    return Decimal(-maxForPrecision(precision), precision, scale);
}

Decimal Decimal::maxValue(uint8_t precision, uint8_t scale) {
    return Decimal(maxForPrecision(precision), precision, scale);
}

// ============================================================================
// Serialization
// ============================================================================

std::vector<uint8_t> Decimal::serialize() const {
    // Format: [precision (1)] [scale (1)] [sign (1)] [length (1)] [value bytes (variable)]
    std::vector<uint8_t> result;
    result.push_back(precision_);
    result.push_back(scale_);

    int128_t abs_value = value_ < 0 ? -value_ : value_;
    result.push_back(value_ < 0 ? 1 : 0);

    // Serialize value as big-endian bytes
    std::vector<uint8_t> value_bytes;
    if (abs_value == 0) {
        value_bytes.push_back(0);
    } else {
        while (abs_value > 0) {
            value_bytes.push_back(static_cast<uint8_t>(abs_value & 0xFF));
            abs_value >>= 8;
        }
    }

    result.push_back(static_cast<uint8_t>(value_bytes.size()));
    result.insert(result.end(), value_bytes.rbegin(), value_bytes.rend());

    return result;
}

Decimal Decimal::deserialize(const uint8_t* data, size_t len) {
    if (len < 4) return Decimal();

    uint8_t precision = data[0];
    uint8_t scale = data[1];
    bool negative = data[2] != 0;
    uint8_t value_len = data[3];

    if (len < 4 + value_len) return Decimal();

    int128_t value = 0;
    for (size_t i = 0; i < value_len; ++i) {
        value = (value << 8) | data[4 + i];
    }

    if (negative) {
        value = -value;
    }

    return Decimal(value, precision, scale);
}

// ============================================================================
// Hash
// ============================================================================

size_t Decimal::hash() const {
    // Normalize to common scale before hashing
    Decimal normalized = rescale(DECIMAL_MAX_PRECISION, 10, DecimalRoundingMode::HALF_UP);

    // Combine hash of value parts
    uint64_t low = static_cast<uint64_t>(normalized.value_);
    uint64_t high = static_cast<uint64_t>(normalized.value_ >> 64);

    size_t h = std::hash<uint64_t>{}(low);
    h ^= std::hash<uint64_t>{}(high) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

// ============================================================================
// DecimalAggregator Implementation
// ============================================================================

DecimalAggregator::DecimalAggregator(uint8_t precision, uint8_t scale)
    : sum_(Decimal::zero(precision, scale)),
      min_(Decimal::maxValue(precision, scale)),
      max_(Decimal::minValue(precision, scale)),
      precision_(precision), scale_(scale) {}

void DecimalAggregator::add(const Decimal& value) {
    sum_ = sum_.add(value);

    if (!has_value_ || value < min_) {
        min_ = value;
    }
    if (!has_value_ || value > max_) {
        max_ = value;
    }

    count_++;
    has_value_ = true;
}

void DecimalAggregator::reset() {
    sum_ = Decimal::zero(precision_, scale_);
    min_ = Decimal::maxValue(precision_, scale_);
    max_ = Decimal::minValue(precision_, scale_);
    count_ = 0;
    has_value_ = false;
}

Decimal DecimalAggregator::sum() const {
    return sum_;
}

Decimal DecimalAggregator::avg() const {
    if (count_ == 0) return Decimal::zero(precision_, scale_);
    return sum_.divide(Decimal(static_cast<int64_t>(count_)), scale_);
}

Decimal DecimalAggregator::min() const {
    return has_value_ ? min_ : Decimal::zero(precision_, scale_);
}

Decimal DecimalAggregator::max() const {
    return has_value_ ? max_ : Decimal::zero(precision_, scale_);
}

} // namespace scratchbird::core
