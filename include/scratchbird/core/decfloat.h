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
#include <vector>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{
    enum class DecFloatRoundingMode
    {
        HALF_UP,
        HALF_EVEN,
        DOWN,
        UP,
        FLOOR,
        CEILING
    };

    struct DecFloatContext
    {
        DecFloatRoundingMode rounding = DecFloatRoundingMode::HALF_UP;
        bool trap_divide_by_zero = true;
        bool trap_invalid = true;
        bool trap_overflow = true;
        bool trap_underflow = false;
        bool trap_inexact = false;
    };

    enum class DecFloatClass
    {
        Finite,
        Infinity,
        NaN,
        SignalingNaN
    };

    struct DecFloat
    {
        uint8_t precision = 16;
        DecFloatClass klass = DecFloatClass::Finite;
        bool negative = false;
        int32_t exponent = 0;
        std::vector<uint8_t> coefficient;

        static Status parse(const std::string& text, uint8_t precision,
                            const DecFloatContext& ctx, DecFloat& out,
                            ErrorContext* err = nullptr);

        std::string toString() const;
        bool isZero() const;

        static DecFloat fromBID64(uint64_t bits);
        static DecFloat fromBID128(uint64_t high, uint64_t low);

        Status toBID64(uint64_t& out, const DecFloatContext& ctx,
                       ErrorContext* err = nullptr) const;
        Status toBID128(uint64_t& high, uint64_t& low, const DecFloatContext& ctx,
                        ErrorContext* err = nullptr) const;

        static Status add(const DecFloat& left, const DecFloat& right,
                          const DecFloatContext& ctx, DecFloat& out,
                          ErrorContext* err = nullptr);
        static Status subtract(const DecFloat& left, const DecFloat& right,
                               const DecFloatContext& ctx, DecFloat& out,
                               ErrorContext* err = nullptr);
        static Status multiply(const DecFloat& left, const DecFloat& right,
                               const DecFloatContext& ctx, DecFloat& out,
                               ErrorContext* err = nullptr);
        static Status divide(const DecFloat& left, const DecFloat& right,
                             const DecFloatContext& ctx, DecFloat& out,
                             ErrorContext* err = nullptr);

        static int compare(const DecFloat& left, const DecFloat& right,
                           ErrorContext* err = nullptr);
    };
}
