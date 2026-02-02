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
#include <limits>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{
    /**
     * Safe arithmetic operations with overflow detection
     *
     * Uses compiler intrinsics for efficient overflow checking.
     * Returns false if overflow would occur, true on success.
     *
     * P0-4: Arithmetic Overflow Checking
     * Addresses CWE-190: Integer Overflow
     */

    // INT64 safe operations
    inline bool safeAddInt64(int64_t a, int64_t b, int64_t* result)
    {
        #if defined(__GNUC__) || defined(__clang__)
            return !__builtin_add_overflow(a, b, result);
        #else
            // Fallback for non-GCC/Clang compilers
            if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
                return false;
            }
            *result = a + b;
            return true;
        #endif
    }

    inline bool safeSubtractInt64(int64_t a, int64_t b, int64_t* result)
    {
        #if defined(__GNUC__) || defined(__clang__)
            return !__builtin_sub_overflow(a, b, result);
        #else
            // Fallback
            if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) {
                return false;
            }
            *result = a - b;
            return true;
        #endif
    }

    inline bool safeMultiplyInt64(int64_t a, int64_t b, int64_t* result)
    {
        #if defined(__GNUC__) || defined(__clang__)
            return !__builtin_mul_overflow(a, b, result);
        #else
            // Fallback
            if (a == 0 || b == 0) {
                *result = 0;
                return true;
            }

            // Check for overflow
            if (a > 0) {
                if (b > 0 && a > INT64_MAX / b) return false;
                if (b < 0 && b < INT64_MIN / a) return false;
            } else {
                if (b > 0 && a < INT64_MIN / b) return false;
                if (b < 0 && a < INT64_MAX / b) return false;
            }

            *result = a * b;
            return true;
        #endif
    }

    inline bool safeDivideInt64(int64_t a, int64_t b, int64_t* result, ErrorContext* ctx)
    {
        if (b == 0) {
            SET_ERROR_CONTEXT(ctx, Status::DIVISION_BY_ZERO, "Division by zero");
            return false;
        }

        // Check for INT64_MIN / -1 overflow (results in INT64_MAX + 1)
        if (a == INT64_MIN && b == -1) {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Integer overflow in division");
            return false;
        }

        *result = a / b;
        return true;
    }

    inline bool safeModuloInt64(int64_t a, int64_t b, int64_t* result, ErrorContext* ctx)
    {
        if (b == 0) {
            SET_ERROR_CONTEXT(ctx, Status::DIVISION_BY_ZERO, "Division by zero in modulo");
            return false;
        }

        // INT64_MIN % -1 = 0 (no overflow, but be explicit)
        if (a == INT64_MIN && b == -1) {
            *result = 0;
            return true;
        }

        *result = a % b;
        return true;
    }

    inline bool safeNegateInt64(int64_t a, int64_t* result, ErrorContext* ctx)
    {
        // -INT64_MIN overflows to INT64_MAX + 1
        if (a == INT64_MIN) {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Integer overflow in negation");
            return false;
        }

        *result = -a;
        return true;
    }

    // INT32 safe operations
    inline bool safeAddInt32(int32_t a, int32_t b, int32_t* result)
    {
        #if defined(__GNUC__) || defined(__clang__)
            return !__builtin_add_overflow(a, b, result);
        #else
            if ((b > 0 && a > INT32_MAX - b) || (b < 0 && a < INT32_MIN - b)) {
                return false;
            }
            *result = a + b;
            return true;
        #endif
    }

    inline bool safeSubtractInt32(int32_t a, int32_t b, int32_t* result)
    {
        #if defined(__GNUC__) || defined(__clang__)
            return !__builtin_sub_overflow(a, b, result);
        #else
            if ((b < 0 && a > INT32_MAX + b) || (b > 0 && a < INT32_MIN + b)) {
                return false;
            }
            *result = a - b;
            return true;
        #endif
    }

    inline bool safeMultiplyInt32(int32_t a, int32_t b, int32_t* result)
    {
        #if defined(__GNUC__) || defined(__clang__)
            return !__builtin_mul_overflow(a, b, result);
        #else
            if (a == 0 || b == 0) {
                *result = 0;
                return true;
            }

            if (a > 0) {
                if (b > 0 && a > INT32_MAX / b) return false;
                if (b < 0 && b < INT32_MIN / a) return false;
            } else {
                if (b > 0 && a < INT32_MIN / b) return false;
                if (b < 0 && a < INT32_MAX / b) return false;
            }

            *result = a * b;
            return true;
        #endif
    }

    inline bool safeDivideInt32(int32_t a, int32_t b, int32_t* result, ErrorContext* ctx)
    {
        if (b == 0) {
            SET_ERROR_CONTEXT(ctx, Status::DIVISION_BY_ZERO, "Division by zero");
            return false;
        }

        if (a == INT32_MIN && b == -1) {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Integer overflow in division");
            return false;
        }

        *result = a / b;
        return true;
    }

    inline bool safeModuloInt32(int32_t a, int32_t b, int32_t* result, ErrorContext* ctx)
    {
        if (b == 0) {
            SET_ERROR_CONTEXT(ctx, Status::DIVISION_BY_ZERO, "Division by zero in modulo");
            return false;
        }

        if (a == INT32_MIN && b == -1) {
            *result = 0;
            return true;
        }

        *result = a % b;
        return true;
    }

    inline bool safeNegateInt32(int32_t a, int32_t* result, ErrorContext* ctx)
    {
        if (a == INT32_MIN) {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Integer overflow in negation");
            return false;
        }

        *result = -a;
        return true;
    }

} // namespace scratchbird::core
