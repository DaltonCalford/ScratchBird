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
#include <optional>
#include "scratchbird/core/types.h"

namespace scratchbird::core
{
    /**
     * DecimalValue - Fixed-precision decimal arithmetic
     *
     * Implements arbitrary-precision decimal arithmetic for DECIMAL type.
     * Uses int128_t internally for up to 38 digits of precision.
     *
     * Storage format:
     * - value: int128_t scaled integer (e.g., 12345 for 123.45 with scale=2)
     * - scale: number of decimal places (0-38)
     *
     * Example: 123.45 is stored as value=12345, scale=2
     */
    class DecimalValue
    {
    public:
        DecimalValue();
        DecimalValue(int128_t value, uint8_t scale);

        // Parse from string (e.g., "123.45", "-0.001", "1000")
        static auto fromString(const std::string& str) -> std::optional<DecimalValue>;

        // Convert to string
        auto toString() const -> std::string;

        // Accessors
        int128_t getValue() const { return value_; }
        uint8_t getScale() const { return scale_; }
        bool isZero() const { return value_ == 0; }
        bool isNegative() const { return value_ < 0; }
        bool isPositive() const { return value_ > 0; }

        // Arithmetic operations
        auto add(const DecimalValue& other) const -> DecimalValue;
        auto subtract(const DecimalValue& other) const -> DecimalValue;
        auto multiply(const DecimalValue& other) const -> DecimalValue;
        auto divide(const DecimalValue& other, uint8_t result_scale = 0) const -> std::optional<DecimalValue>;

        // Unary operations
        auto negate() const -> DecimalValue;
        auto abs() const -> DecimalValue;

        // Comparison operations
        bool operator==(const DecimalValue& other) const;
        bool operator!=(const DecimalValue& other) const;
        bool operator<(const DecimalValue& other) const;
        bool operator<=(const DecimalValue& other) const;
        bool operator>(const DecimalValue& other) const;
        bool operator>=(const DecimalValue& other) const;

        // Scale adjustment (rounds if necessary)
        auto rescale(uint8_t new_scale) const -> DecimalValue;

        // Rounding modes
        enum class RoundMode {
            ROUND_HALF_UP,      // Round 0.5 up (away from zero)
            ROUND_HALF_DOWN,    // Round 0.5 down (toward zero)
            ROUND_HALF_EVEN,    // Round to nearest even (banker's rounding)
            ROUND_DOWN,         // Always round toward zero (truncate)
            ROUND_UP,           // Always round away from zero
            ROUND_FLOOR,        // Always round toward negative infinity
            ROUND_CEILING       // Always round toward positive infinity
        };

        // Round to specified scale with rounding mode
        auto round(uint8_t target_scale, RoundMode mode = RoundMode::ROUND_HALF_UP) const -> DecimalValue;

    private:
        int128_t value_;    // Scaled integer value
        uint8_t scale_;     // Number of decimal places (0-38)

        // Helper: align scales for arithmetic
        static auto alignScales(const DecimalValue& a, const DecimalValue& b)
            -> std::pair<DecimalValue, DecimalValue>;

        // Helper: scale value by power of 10
        static auto scaleUp(int128_t value, uint8_t scale_diff) -> int128_t;

        // Helper: apply rounding when reducing scale
        static auto applyRounding(int128_t value, uint8_t scale_diff, RoundMode mode) -> int128_t;
    };

    /**
     * DecimalArithmetic - Static utility functions for DECIMAL operations
     */
    class DecimalArithmetic
    {
    public:
        // Add two DECIMAL values
        static auto add(const std::string& a, const std::string& b) -> std::optional<std::string>;

        // Subtract two DECIMAL values
        static auto subtract(const std::string& a, const std::string& b) -> std::optional<std::string>;

        // Multiply two DECIMAL values
        static auto multiply(const std::string& a, const std::string& b) -> std::optional<std::string>;

        // Divide two DECIMAL values with specified result scale
        static auto divide(const std::string& a, const std::string& b, uint8_t result_scale = 10)
            -> std::optional<std::string>;

        // Compare DECIMAL values (-1: a<b, 0: a==b, 1: a>b)
        static auto compare(const std::string& a, const std::string& b) -> std::optional<int>;

        // Negate a DECIMAL value
        static auto negate(const std::string& a) -> std::optional<std::string>;

        // Get absolute value
        static auto abs(const std::string& a) -> std::optional<std::string>;

        // Round to specified scale
        static auto round(const std::string& a, uint8_t scale,
                         DecimalValue::RoundMode mode = DecimalValue::RoundMode::ROUND_HALF_UP)
            -> std::optional<std::string>;
    };

} // namespace scratchbird::core
