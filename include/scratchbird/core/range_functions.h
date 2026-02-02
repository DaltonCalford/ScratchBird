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

#include "scratchbird/core/range.h"
#include <optional>

namespace scratchbird::core
{

/**
 * @brief Range functions (PostgreSQL-compatible)
 *
 * Implements standard range functions:
 * - lower(range) - Get lower bound
 * - upper(range) - Get upper bound
 * - isempty(range) - Check if range is empty
 * - lower_inc(range) - Check if lower bound is inclusive
 * - upper_inc(range) - Check if upper bound is inclusive
 * - lower_inf(range) - Check if lower bound is infinite
 * - upper_inf(range) - Check if upper bound is infinite
 * - range_merge(range, range) - Smallest range containing both
 */

// Lower bound accessors
template <typename T>
std::optional<T> range_lower(const Range<T>& r)
{
    return r.lower();
}

template <typename T>
std::optional<T> range_upper(const Range<T>& r)
{
    return r.upper();
}

// Emptiness check
template <typename T>
bool range_isempty(const Range<T>& r)
{
    return r.isEmpty();
}

// Bound inclusion checks
template <typename T>
bool range_lower_inc(const Range<T>& r)
{
    return r.isLowerInclusive();
}

template <typename T>
bool range_upper_inc(const Range<T>& r)
{
    return r.isUpperInclusive();
}

// Infinity checks
template <typename T>
bool range_lower_inf(const Range<T>& r)
{
    return !r.isLowerBounded();
}

template <typename T>
bool range_upper_inf(const Range<T>& r)
{
    return !r.isUpperBounded();
}

// Range merge - smallest range containing both
template <typename T>
Range<T> range_merge(const Range<T>& r1, const Range<T>& r2)
{
    if (r1.isEmpty())
        return r2;
    if (r2.isEmpty())
        return r1;

    // Compute merged bounds
    std::optional<T> new_lower;
    BoundType new_lower_type;

    if (!r1.lower().has_value() || !r2.lower().has_value())
    {
        new_lower = std::nullopt;
        new_lower_type = BoundType::EXCLUSIVE;
    }
    else if (*r1.lower() < *r2.lower())
    {
        new_lower = r1.lower();
        new_lower_type = r1.lowerBoundType();
    }
    else if (*r1.lower() > *r2.lower())
    {
        new_lower = r2.lower();
        new_lower_type = r2.lowerBoundType();
    }
    else
    {
        // Equal - take inclusive if either is inclusive
        new_lower = r1.lower();
        new_lower_type = (r1.lowerBoundType() == BoundType::INCLUSIVE ||
                         r2.lowerBoundType() == BoundType::INCLUSIVE)
                            ? BoundType::INCLUSIVE
                            : BoundType::EXCLUSIVE;
    }

    std::optional<T> new_upper;
    BoundType new_upper_type;

    if (!r1.upper().has_value() || !r2.upper().has_value())
    {
        new_upper = std::nullopt;
        new_upper_type = BoundType::EXCLUSIVE;
    }
    else if (*r1.upper() > *r2.upper())
    {
        new_upper = r1.upper();
        new_upper_type = r1.upperBoundType();
    }
    else if (*r1.upper() < *r2.upper())
    {
        new_upper = r2.upper();
        new_upper_type = r2.upperBoundType();
    }
    else
    {
        new_upper = r1.upper();
        new_upper_type = (r1.upperBoundType() == BoundType::INCLUSIVE ||
                         r2.upperBoundType() == BoundType::INCLUSIVE)
                            ? BoundType::INCLUSIVE
                            : BoundType::EXCLUSIVE;
    }

    return Range<T>(new_lower, new_upper, new_lower_type, new_upper_type);
}

} // namespace scratchbird::core
