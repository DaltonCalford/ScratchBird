/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/decimal_arithmetic.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// Define HAS_INT128 for GCC/Clang with __int128 support
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__aarch64__))
#define HAS_INT128 1
#else
#define HAS_INT128 0
#endif

namespace scratchbird::core
{
    // ===== DecimalValue Implementation =====

    DecimalValue::DecimalValue() : value_(0), scale_(0) {}

    DecimalValue::DecimalValue(int128_t value, uint8_t scale)
        : value_(value), scale_(scale)
    {
        if (scale > 38) {
            scale_ = 38; // Clamp to maximum supported scale
        }
    }

    auto DecimalValue::fromString(const std::string& str) -> std::optional<DecimalValue>
    {
        if (str.empty()) {
            return std::nullopt;
        }

        // Parse sign
        bool negative = false;
        size_t pos = 0;
        if (str[0] == '-') {
            negative = true;
            pos = 1;
        } else if (str[0] == '+') {
            pos = 1;
        }

        // Find decimal point
        size_t decimal_pos = str.find('.', pos);
        uint8_t scale = 0;

        std::string integer_part;
        std::string fraction_part;

        if (decimal_pos == std::string::npos) {
            // No decimal point - integer value
            integer_part = str.substr(pos);
            scale = 0;
        } else {
            // Has decimal point
            integer_part = str.substr(pos, decimal_pos - pos);
            fraction_part = str.substr(decimal_pos + 1);
            scale = static_cast<uint8_t>(fraction_part.length());
        }

        // Validate digits
        if (integer_part.empty() && fraction_part.empty()) {
            return std::nullopt;
        }

        // Build scaled value as string (without decimal point)
        std::string value_str = integer_part + fraction_part;

        // Remove leading zeros (except for "0")
        while (value_str.length() > 1 && value_str[0] == '0') {
            value_str = value_str.substr(1);
        }

        // Parse to int128_t
        int128_t value = 0;
        for (char c : value_str) {
            if (c < '0' || c > '9') {
                return std::nullopt; // Invalid character
            }
            value = value * 10 + (c - '0');
        }

        if (negative) {
            value = -value;
        }

        return DecimalValue(value, scale);
    }

    auto DecimalValue::toString() const -> std::string
    {
#if HAS_INT128
        // For native __int128
        if (value_ == 0) {
            if (scale_ == 0) {
                return "0";
            } else {
                return "0." + std::string(scale_, '0');
            }
        }

        bool negative = value_ < 0;
        int128_t abs_value = negative ? -value_ : value_;

        // Convert to string
        std::string digits;
        int128_t temp = abs_value;
        while (temp > 0) {
            digits = char('0' + (temp % 10)) + digits;
            temp /= 10;
        }

        // Pad with leading zeros if needed
        while (digits.length() <= scale_) {
            digits = "0" + digits;
        }

        // Insert decimal point
        std::string result;
        if (scale_ == 0) {
            result = digits;
        } else {
            size_t decimal_pos = digits.length() - scale_;
            result = digits.substr(0, decimal_pos) + "." + digits.substr(decimal_pos);
        }

        if (negative) {
            result = "-" + result;
        }

        return result;
#else
        // Fallback for struct-based int128_t
        std::ostringstream oss;
        oss << "DECIMAL{value=" << value_.high << ":" << value_.low << ",scale=" << static_cast<int>(scale_) << "}";
        return oss.str();
#endif
    }

    auto DecimalValue::add(const DecimalValue& other) const -> DecimalValue
    {
        auto [a, b] = alignScales(*this, other);
        return DecimalValue(a.value_ + b.value_, a.scale_);
    }

    auto DecimalValue::subtract(const DecimalValue& other) const -> DecimalValue
    {
        auto [a, b] = alignScales(*this, other);
        return DecimalValue(a.value_ - b.value_, a.scale_);
    }

    auto DecimalValue::multiply(const DecimalValue& other) const -> DecimalValue
    {
        int128_t result = value_ * other.value_;
        uint8_t result_scale = scale_ + other.scale_;

        // Clamp scale to maximum
        if (result_scale > 38) {
            // Need to reduce scale by rounding
            uint8_t scale_diff = result_scale - 38;
            result = applyRounding(result, scale_diff, RoundMode::ROUND_HALF_UP);
            result_scale = 38;
        }

        return DecimalValue(result, result_scale);
    }

    auto DecimalValue::divide(const DecimalValue& other, uint8_t result_scale) const -> std::optional<DecimalValue>
    {
        if (other.value_ == 0) {
            return std::nullopt; // Division by zero
        }

        // If result_scale is 0, use max of the two scales
        if (result_scale == 0) {
            result_scale = std::max(scale_, other.scale_);
        }

        // Scale up dividend to achieve desired result scale
        int128_t scaled_dividend = value_;
        uint8_t scale_to_add = result_scale + other.scale_;

        // Scale up
        for (uint8_t i = 0; i < scale_to_add; ++i) {
            scaled_dividend *= 10;
        }

        int128_t result = scaled_dividend / other.value_;

        return DecimalValue(result, result_scale);
    }

    auto DecimalValue::negate() const -> DecimalValue
    {
        return DecimalValue(-value_, scale_);
    }

    auto DecimalValue::abs() const -> DecimalValue
    {
        return DecimalValue(value_ < 0 ? -value_ : value_, scale_);
    }

    bool DecimalValue::operator==(const DecimalValue& other) const
    {
        auto [a, b] = alignScales(*this, other);
        return a.value_ == b.value_;
    }

    bool DecimalValue::operator!=(const DecimalValue& other) const
    {
        return !(*this == other);
    }

    bool DecimalValue::operator<(const DecimalValue& other) const
    {
        auto [a, b] = alignScales(*this, other);
        return a.value_ < b.value_;
    }

    bool DecimalValue::operator<=(const DecimalValue& other) const
    {
        auto [a, b] = alignScales(*this, other);
        return a.value_ <= b.value_;
    }

    bool DecimalValue::operator>(const DecimalValue& other) const
    {
        auto [a, b] = alignScales(*this, other);
        return a.value_ > b.value_;
    }

    bool DecimalValue::operator>=(const DecimalValue& other) const
    {
        auto [a, b] = alignScales(*this, other);
        return a.value_ >= b.value_;
    }

    auto DecimalValue::rescale(uint8_t new_scale) const -> DecimalValue
    {
        if (new_scale == scale_) {
            return *this;
        }

        if (new_scale > scale_) {
            // Scale up - multiply by 10^(new_scale - scale_)
            int128_t new_value = scaleUp(value_, new_scale - scale_);
            return DecimalValue(new_value, new_scale);
        } else {
            // Scale down - divide and round
            int128_t new_value = applyRounding(value_, scale_ - new_scale, RoundMode::ROUND_HALF_UP);
            return DecimalValue(new_value, new_scale);
        }
    }

    auto DecimalValue::round(uint8_t target_scale, RoundMode mode) const -> DecimalValue
    {
        if (target_scale >= scale_) {
            return rescale(target_scale);
        }

        int128_t rounded_value = applyRounding(value_, scale_ - target_scale, mode);
        return DecimalValue(rounded_value, target_scale);
    }

    // Private helpers

    auto DecimalValue::alignScales(const DecimalValue& a, const DecimalValue& b)
        -> std::pair<DecimalValue, DecimalValue>
    {
        if (a.scale_ == b.scale_) {
            return {a, b};
        }

        uint8_t max_scale = std::max(a.scale_, b.scale_);
        return {a.rescale(max_scale), b.rescale(max_scale)};
    }

    auto DecimalValue::scaleUp(int128_t value, uint8_t scale_diff) -> int128_t
    {
        for (uint8_t i = 0; i < scale_diff; ++i) {
            value *= 10;
        }
        return value;
    }

    auto DecimalValue::applyRounding(int128_t value, uint8_t scale_diff, RoundMode mode) -> int128_t
    {
        if (scale_diff == 0) {
            return value;
        }

        // Calculate divisor (10^scale_diff)
        int128_t divisor = 1;
        for (uint8_t i = 0; i < scale_diff; ++i) {
            divisor *= 10;
        }

        int128_t quotient = value / divisor;
        int128_t remainder = value % divisor;

        if (remainder == 0) {
            return quotient; // Exact division, no rounding needed
        }

        bool is_negative = value < 0;
        int128_t abs_remainder = is_negative ? -remainder : remainder;
        int128_t half_divisor = divisor / 2;

        switch (mode) {
            case RoundMode::ROUND_HALF_UP:
                if (abs_remainder >= half_divisor) {
                    return is_negative ? quotient - 1 : quotient + 1;
                }
                return quotient;

            case RoundMode::ROUND_HALF_DOWN:
                if (abs_remainder > half_divisor) {
                    return is_negative ? quotient - 1 : quotient + 1;
                }
                return quotient;

            case RoundMode::ROUND_HALF_EVEN:
                if (abs_remainder > half_divisor) {
                    return is_negative ? quotient - 1 : quotient + 1;
                } else if (abs_remainder == half_divisor) {
                    // Round to nearest even
                    if (quotient % 2 != 0) {
                        return is_negative ? quotient - 1 : quotient + 1;
                    }
                }
                return quotient;

            case RoundMode::ROUND_DOWN:
                return quotient;

            case RoundMode::ROUND_UP:
                return is_negative ? quotient - 1 : quotient + 1;

            case RoundMode::ROUND_FLOOR:
                return is_negative ? quotient - 1 : quotient;

            case RoundMode::ROUND_CEILING:
                return is_negative ? quotient : quotient + 1;

            default:
                return quotient;
        }
    }

    // ===== DecimalArithmetic Static Functions =====

    auto DecimalArithmetic::add(const std::string& a, const std::string& b) -> std::optional<std::string>
    {
        auto dec_a = DecimalValue::fromString(a);
        auto dec_b = DecimalValue::fromString(b);

        if (!dec_a || !dec_b) {
            return std::nullopt;
        }

        return dec_a->add(*dec_b).toString();
    }

    auto DecimalArithmetic::subtract(const std::string& a, const std::string& b) -> std::optional<std::string>
    {
        auto dec_a = DecimalValue::fromString(a);
        auto dec_b = DecimalValue::fromString(b);

        if (!dec_a || !dec_b) {
            return std::nullopt;
        }

        return dec_a->subtract(*dec_b).toString();
    }

    auto DecimalArithmetic::multiply(const std::string& a, const std::string& b) -> std::optional<std::string>
    {
        auto dec_a = DecimalValue::fromString(a);
        auto dec_b = DecimalValue::fromString(b);

        if (!dec_a || !dec_b) {
            return std::nullopt;
        }

        return dec_a->multiply(*dec_b).toString();
    }

    auto DecimalArithmetic::divide(const std::string& a, const std::string& b, uint8_t result_scale)
        -> std::optional<std::string>
    {
        auto dec_a = DecimalValue::fromString(a);
        auto dec_b = DecimalValue::fromString(b);

        if (!dec_a || !dec_b) {
            return std::nullopt;
        }

        auto result = dec_a->divide(*dec_b, result_scale);
        if (!result) {
            return std::nullopt;
        }

        return result->toString();
    }

    auto DecimalArithmetic::compare(const std::string& a, const std::string& b) -> std::optional<int>
    {
        auto dec_a = DecimalValue::fromString(a);
        auto dec_b = DecimalValue::fromString(b);

        if (!dec_a || !dec_b) {
            return std::nullopt;
        }

        if (*dec_a < *dec_b) return -1;
        if (*dec_a > *dec_b) return 1;
        return 0;
    }

    auto DecimalArithmetic::negate(const std::string& a) -> std::optional<std::string>
    {
        auto dec = DecimalValue::fromString(a);
        if (!dec) {
            return std::nullopt;
        }

        return dec->negate().toString();
    }

    auto DecimalArithmetic::abs(const std::string& a) -> std::optional<std::string>
    {
        auto dec = DecimalValue::fromString(a);
        if (!dec) {
            return std::nullopt;
        }

        return dec->abs().toString();
    }

    auto DecimalArithmetic::round(const std::string& a, uint8_t scale, DecimalValue::RoundMode mode)
        -> std::optional<std::string>
    {
        auto dec = DecimalValue::fromString(a);
        if (!dec) {
            return std::nullopt;
        }

        return dec->round(scale, mode).toString();
    }

} // namespace scratchbird::core
