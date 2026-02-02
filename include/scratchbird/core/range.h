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

#include <optional>
#include <string>
#include <stdexcept>
#include <sstream>
#include <cstdint>

namespace scratchbird::core
{

/**
 * @brief Bound inclusion type for range boundaries
 *
 * PostgreSQL-compatible bound types:
 * - INCLUSIVE: [a, b] - includes the boundary value
 * - EXCLUSIVE: (a, b) - excludes the boundary value
 */
enum class BoundType
{
    INCLUSIVE,  // [ or ]
    EXCLUSIVE   // ( or )
};

/**
 * @brief Generic range type template
 *
 * Implements PostgreSQL-compatible range types with:
 * - Inclusive/exclusive bounds
 * - Empty ranges
 * - Infinite bounds (unbounded lower/upper)
 * - Canonical forms
 *
 * @tparam T The element type (must be ordered)
 */
template <typename T>
class Range
{
private:
    std::optional<T> lower_;
    std::optional<T> upper_;
    BoundType lower_bound_type_;
    BoundType upper_bound_type_;
    bool empty_;

public:
    /**
     * @brief Construct an empty range
     */
    Range()
        : lower_(std::nullopt)
        , upper_(std::nullopt)
        , lower_bound_type_(BoundType::EXCLUSIVE)
        , upper_bound_type_(BoundType::EXCLUSIVE)
        , empty_(true)
    {}

    /**
     * @brief Construct a range with specified bounds
     *
     * @param lower Lower bound (nullopt for unbounded)
     * @param upper Upper bound (nullopt for unbounded)
     * @param lower_inc Lower bound is inclusive
     * @param upper_inc Upper bound is inclusive
     */
    Range(std::optional<T> lower, std::optional<T> upper,
          bool lower_inc = true, bool upper_inc = false)
        : lower_(lower)
        , upper_(upper)
        , lower_bound_type_(lower_inc ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE)
        , upper_bound_type_(upper_inc ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE)
        , empty_(false)
    {
        // Validate and canonicalize
        if (lower_.has_value() && upper_.has_value())
        {
            if (*lower_ > *upper_)
            {
                // Invalid range - lower > upper
                empty_ = true;
            }
            else if (*lower_ == *upper_)
            {
                // Degenerate case: [a,a] is valid, (a,a) or [a,a) or (a,a] is empty
                if (lower_bound_type_ == BoundType::EXCLUSIVE ||
                    upper_bound_type_ == BoundType::EXCLUSIVE)
                {
                    empty_ = true;
                }
            }
        }
    }

    /**
     * @brief Construct a range with explicit bound types
     */
    Range(std::optional<T> lower, std::optional<T> upper,
          BoundType lower_type, BoundType upper_type)
        : lower_(lower)
        , upper_(upper)
        , lower_bound_type_(lower_type)
        , upper_bound_type_(upper_type)
        , empty_(false)
    {
        // Validate
        if (lower_.has_value() && upper_.has_value())
        {
            if (*lower_ > *upper_)
            {
                empty_ = true;
            }
            else if (*lower_ == *upper_)
            {
                if (lower_bound_type_ == BoundType::EXCLUSIVE ||
                    upper_bound_type_ == BoundType::EXCLUSIVE)
                {
                    empty_ = true;
                }
            }
        }
    }

    // Accessors
    bool isEmpty() const { return empty_; }
    std::optional<T> lower() const { return lower_; }
    std::optional<T> upper() const { return upper_; }
    BoundType lowerBoundType() const { return lower_bound_type_; }
    BoundType upperBoundType() const { return upper_bound_type_; }
    bool isLowerInclusive() const { return lower_bound_type_ == BoundType::INCLUSIVE; }
    bool isUpperInclusive() const { return upper_bound_type_ == BoundType::INCLUSIVE; }
    bool isLowerBounded() const { return lower_.has_value(); }
    bool isUpperBounded() const { return upper_.has_value(); }

    /**
     * @brief Check if value is contained in range
     *
     * @param value Value to check
     * @return true if value is in range
     */
    bool contains(const T& value) const
    {
        if (empty_)
            return false;

        // Check lower bound
        if (lower_.has_value())
        {
            if (lower_bound_type_ == BoundType::INCLUSIVE)
            {
                if (value < *lower_)
                    return false;
            }
            else
            {
                if (value <= *lower_)
                    return false;
            }
        }

        // Check upper bound
        if (upper_.has_value())
        {
            if (upper_bound_type_ == BoundType::INCLUSIVE)
            {
                if (value > *upper_)
                    return false;
            }
            else
            {
                if (value >= *upper_)
                    return false;
            }
        }

        return true;
    }

    /**
     * @brief Check if this range contains another range
     *
     * @param other Range to check
     * @return true if other is completely contained
     */
    bool contains(const Range<T>& other) const
    {
        if (empty_ || other.empty_)
            return other.empty_;

        // Check lower bound
        if (other.lower_.has_value())
        {
            if (lower_.has_value())
            {
                if (*other.lower_ < *lower_)
                    return false;
                if (*other.lower_ == *lower_)
                {
                    // If other's lower is exclusive and ours is inclusive, it's contained
                    // If other's lower is inclusive and ours is exclusive, it's not
                    if (other.lower_bound_type_ == BoundType::INCLUSIVE &&
                        lower_bound_type_ == BoundType::EXCLUSIVE)
                    {
                        return false;
                    }
                }
            }
        }
        else
        {
            // Other has unbounded lower, we must too
            if (lower_.has_value())
                return false;
        }

        // Check upper bound
        if (other.upper_.has_value())
        {
            if (upper_.has_value())
            {
                if (*other.upper_ > *upper_)
                    return false;
                if (*other.upper_ == *upper_)
                {
                    if (other.upper_bound_type_ == BoundType::INCLUSIVE &&
                        upper_bound_type_ == BoundType::EXCLUSIVE)
                    {
                        return false;
                    }
                }
            }
        }
        else
        {
            // Other has unbounded upper, we must too
            if (upper_.has_value())
                return false;
        }

        return true;
    }

    /**
     * @brief Check if ranges overlap
     *
     * @param other Range to check
     * @return true if ranges have any common elements
     */
    bool overlaps(const Range<T>& other) const
    {
        if (empty_ || other.empty_)
            return false;

        // Check if one range is entirely before the other
        if (upper_.has_value() && other.lower_.has_value())
        {
            if (*upper_ < *other.lower_)
                return false;
            if (*upper_ == *other.lower_)
            {
                // Adjacent, not overlapping unless both bounds inclusive
                if (upper_bound_type_ == BoundType::EXCLUSIVE ||
                    other.lower_bound_type_ == BoundType::EXCLUSIVE)
                {
                    return false;
                }
            }
        }

        if (lower_.has_value() && other.upper_.has_value())
        {
            if (*lower_ > *other.upper_)
                return false;
            if (*lower_ == *other.upper_)
            {
                if (lower_bound_type_ == BoundType::EXCLUSIVE ||
                    other.upper_bound_type_ == BoundType::EXCLUSIVE)
                {
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * @brief Check if this range is strictly left of another
     *
     * @param other Range to compare
     * @return true if all elements of this range are less than all elements of other
     */
    bool isLeftOf(const Range<T>& other) const
    {
        if (empty_ || other.empty_)
            return false;

        if (!upper_.has_value() || !other.lower_.has_value())
            return false;

        // Strictly less than - no adjacency
        if (*upper_ < *other.lower_)
            return true;

        return false;
    }

    /**
     * @brief Check if this range is strictly right of another
     *
     * @param other Range to compare
     * @return true if all elements of this range are greater than all elements of other
     */
    bool isRightOf(const Range<T>& other) const
    {
        return other.isLeftOf(*this);
    }

    /**
     * @brief Check if ranges are adjacent (touching but not overlapping)
     *
     * @param other Range to check
     * @return true if ranges are adjacent
     */
    bool isAdjacentTo(const Range<T>& other) const
    {
        if (empty_ || other.empty_)
            return false;

        // Check if this range ends where other begins
        if (upper_.has_value() && other.lower_.has_value())
        {
            if (*upper_ == *other.lower_)
            {
                // Adjacent if one exclusive, one inclusive
                if ((upper_bound_type_ == BoundType::EXCLUSIVE) !=
                    (other.lower_bound_type_ == BoundType::EXCLUSIVE))
                {
                    return true;
                }
            }
        }

        // Check if other range ends where this begins
        if (lower_.has_value() && other.upper_.has_value())
        {
            if (*lower_ == *other.upper_)
            {
                if ((lower_bound_type_ == BoundType::EXCLUSIVE) !=
                    (other.upper_bound_type_ == BoundType::EXCLUSIVE))
                {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * @brief Compute union of two ranges
     *
     * @param other Range to union with
     * @return Union range, or empty if ranges don't overlap/touch
     */
    std::optional<Range<T>> rangeUnion(const Range<T>& other) const
    {
        if (empty_)
            return other;
        if (other.empty_)
            return *this;

        // Ranges must overlap or be adjacent
        if (!overlaps(other) && !isAdjacentTo(other))
            return std::nullopt;

        // Compute union bounds
        std::optional<T> new_lower;
        BoundType new_lower_type;

        if (!lower_.has_value() || !other.lower_.has_value())
        {
            // At least one unbounded lower
            new_lower = std::nullopt;
            new_lower_type = BoundType::EXCLUSIVE;
        }
        else if (*lower_ < *other.lower_)
        {
            new_lower = lower_;
            new_lower_type = lower_bound_type_;
        }
        else if (*lower_ > *other.lower_)
        {
            new_lower = other.lower_;
            new_lower_type = other.lower_bound_type_;
        }
        else
        {
            // Equal - take inclusive if either is inclusive
            new_lower = lower_;
            new_lower_type = (lower_bound_type_ == BoundType::INCLUSIVE ||
                             other.lower_bound_type_ == BoundType::INCLUSIVE)
                                ? BoundType::INCLUSIVE
                                : BoundType::EXCLUSIVE;
        }

        std::optional<T> new_upper;
        BoundType new_upper_type;

        if (!upper_.has_value() || !other.upper_.has_value())
        {
            new_upper = std::nullopt;
            new_upper_type = BoundType::EXCLUSIVE;
        }
        else if (*upper_ > *other.upper_)
        {
            new_upper = upper_;
            new_upper_type = upper_bound_type_;
        }
        else if (*upper_ < *other.upper_)
        {
            new_upper = other.upper_;
            new_upper_type = other.upper_bound_type_;
        }
        else
        {
            new_upper = upper_;
            new_upper_type = (upper_bound_type_ == BoundType::INCLUSIVE ||
                             other.upper_bound_type_ == BoundType::INCLUSIVE)
                                ? BoundType::INCLUSIVE
                                : BoundType::EXCLUSIVE;
        }

        return Range<T>(new_lower, new_upper, new_lower_type, new_upper_type);
    }

    /**
     * @brief Compute intersection of two ranges
     *
     * @param other Range to intersect with
     * @return Intersection range (may be empty)
     */
    Range<T> intersection(const Range<T>& other) const
    {
        if (empty_ || other.empty_)
            return Range<T>();  // Empty range

        if (!overlaps(other))
            return Range<T>();  // Empty range

        // Compute intersection bounds
        std::optional<T> new_lower;
        BoundType new_lower_type;

        if (!lower_.has_value())
        {
            new_lower = other.lower_;
            new_lower_type = other.lower_bound_type_;
        }
        else if (!other.lower_.has_value())
        {
            new_lower = lower_;
            new_lower_type = lower_bound_type_;
        }
        else if (*lower_ > *other.lower_)
        {
            new_lower = lower_;
            new_lower_type = lower_bound_type_;
        }
        else if (*lower_ < *other.lower_)
        {
            new_lower = other.lower_;
            new_lower_type = other.lower_bound_type_;
        }
        else
        {
            // Equal - take exclusive if either is exclusive
            new_lower = lower_;
            new_lower_type = (lower_bound_type_ == BoundType::EXCLUSIVE ||
                             other.lower_bound_type_ == BoundType::EXCLUSIVE)
                                ? BoundType::EXCLUSIVE
                                : BoundType::INCLUSIVE;
        }

        std::optional<T> new_upper;
        BoundType new_upper_type;

        if (!upper_.has_value())
        {
            new_upper = other.upper_;
            new_upper_type = other.upper_bound_type_;
        }
        else if (!other.upper_.has_value())
        {
            new_upper = upper_;
            new_upper_type = upper_bound_type_;
        }
        else if (*upper_ < *other.upper_)
        {
            new_upper = upper_;
            new_upper_type = upper_bound_type_;
        }
        else if (*upper_ > *other.upper_)
        {
            new_upper = other.upper_;
            new_upper_type = other.upper_bound_type_;
        }
        else
        {
            new_upper = upper_;
            new_upper_type = (upper_bound_type_ == BoundType::EXCLUSIVE ||
                             other.upper_bound_type_ == BoundType::EXCLUSIVE)
                                ? BoundType::EXCLUSIVE
                                : BoundType::INCLUSIVE;
        }

        return Range<T>(new_lower, new_upper, new_lower_type, new_upper_type);
    }

    /**
     * @brief Compute difference of two ranges
     *
     * @param other Range to subtract
     * @return Difference range. If the subtraction would split into two ranges,
     *         returns the lower remainder.
     */
    Range<T> difference(const Range<T>& other) const
    {
        if (empty_)
            return Range<T>();

        if (other.empty_)
            return *this;

        if (!overlaps(other))
            return *this;

        if (other.contains(*this))
            return Range<T>();

        auto compareLower = [](const std::optional<T>& a, BoundType a_type,
                               const std::optional<T>& b, BoundType b_type) -> int {
            if (!a.has_value() && !b.has_value())
                return 0;
            if (!a.has_value())
                return -1; // -inf
            if (!b.has_value())
                return 1;
            if (*a < *b)
                return -1;
            if (*a > *b)
                return 1;
            if (a_type == b_type)
                return 0;
            return (a_type == BoundType::INCLUSIVE) ? -1 : 1;
        };

        auto compareUpper = [](const std::optional<T>& a, BoundType a_type,
                               const std::optional<T>& b, BoundType b_type) -> int {
            if (!a.has_value() && !b.has_value())
                return 0;
            if (!a.has_value())
                return 1; // +inf
            if (!b.has_value())
                return -1;
            if (*a < *b)
                return -1;
            if (*a > *b)
                return 1;
            if (a_type == b_type)
                return 0;
            return (a_type == BoundType::INCLUSIVE) ? 1 : -1;
        };

        int lower_cmp = compareLower(lower_, lower_bound_type_, other.lower_, other.lower_bound_type_);
        int upper_cmp = compareUpper(upper_, upper_bound_type_, other.upper_, other.upper_bound_type_);

        // Other overlaps the lower side: trim lower bound to other's upper.
        if (lower_cmp >= 0 && upper_cmp > 0)
        {
            BoundType new_lower_type =
                (other.upper_bound_type_ == BoundType::INCLUSIVE) ? BoundType::EXCLUSIVE
                                                                 : BoundType::INCLUSIVE;
            return Range<T>(other.upper_, upper_, new_lower_type, upper_bound_type_);
        }

        // Other overlaps the upper side: trim upper bound to other's lower.
        if (lower_cmp < 0 && upper_cmp <= 0)
        {
            BoundType new_upper_type =
                (other.lower_bound_type_ == BoundType::INCLUSIVE) ? BoundType::EXCLUSIVE
                                                                 : BoundType::INCLUSIVE;
            return Range<T>(lower_, other.lower_, lower_bound_type_, new_upper_type);
        }

        // Other is strictly inside: return lower remainder.
        BoundType new_upper_type =
            (other.lower_bound_type_ == BoundType::INCLUSIVE) ? BoundType::EXCLUSIVE
                                                             : BoundType::INCLUSIVE;
        return Range<T>(lower_, other.lower_, lower_bound_type_, new_upper_type);
    }

    /**
     * @brief String representation
     *
     * Format: [lower,upper) or (lower,upper] or empty
     */
    std::string toString() const
    {
        if (empty_)
            return "empty";

        std::ostringstream oss;

        // Lower bound
        oss << (lower_bound_type_ == BoundType::INCLUSIVE ? '[' : '(');
        if (lower_.has_value())
            oss << *lower_;
        oss << ',';

        // Upper bound
        if (upper_.has_value())
            oss << *upper_;
        oss << (upper_bound_type_ == BoundType::INCLUSIVE ? ']' : ')');

        return oss.str();
    }

    // Equality operators
    bool operator==(const Range<T>& other) const
    {
        if (empty_ && other.empty_)
            return true;
        if (empty_ != other.empty_)
            return false;

        return lower_ == other.lower_ &&
               upper_ == other.upper_ &&
               lower_bound_type_ == other.lower_bound_type_ &&
               upper_bound_type_ == other.upper_bound_type_;
    }

    bool operator!=(const Range<T>& other) const
    {
        return !(*this == other);
    }

    // ============================================================================
    // PostgreSQL-Compatible Range Operators
    // ============================================================================

    /**
     * @brief && operator - Check if ranges overlap
     * @param other Range to check
     * @return true if ranges have common elements
     */
    bool operator&&(const Range<T>& other) const
    {
        return overlaps(other);
    }

    /**
     * @brief @> operator - Check if this range contains another range
     * @param other Range to check
     * @return true if this range contains other
     */
    bool containsRange(const Range<T>& other) const
    {
        return contains(other);
    }

    /**
     * @brief @> operator - Check if this range contains a value
     * @param value Value to check
     * @return true if this range contains value
     */
    bool containsElement(const T& value) const
    {
        return contains(value);
    }

    /**
     * @brief <@ operator - Check if this range is contained by another
     * @param other Range to check
     * @return true if other contains this range
     */
    bool containedBy(const Range<T>& other) const
    {
        return other.contains(*this);
    }

    /**
     * @brief << operator - Check if strictly left of
     * @param other Range to compare
     * @return true if this range is strictly left of other
     */
    bool operator<<(const Range<T>& other) const
    {
        return isLeftOf(other);
    }

    /**
     * @brief >> operator - Check if strictly right of
     * @param other Range to compare
     * @return true if this range is strictly right of other
     */
    bool operator>>(const Range<T>& other) const
    {
        return isRightOf(other);
    }

    /**
     * @brief -|- operator - Check if ranges are adjacent
     * @param other Range to check
     * @return true if ranges are adjacent (touching but not overlapping)
     */
    bool isAdjacent(const Range<T>& other) const
    {
        return isAdjacentTo(other);
    }

    /**
     * @brief & operator - Compute intersection of ranges
     * @param other Range to intersect with
     * @return Intersection range (may be empty)
     */
    Range<T> operator&(const Range<T>& other) const
    {
        return intersection(other);
    }

    /**
     * @brief + operator - Compute union of ranges
     * @param other Range to union with
     * @return Union range, or empty if ranges don't overlap/touch
     * @throws std::runtime_error if ranges are not contiguous
     */
    Range<T> operator+(const Range<T>& other) const
    {
        auto result = rangeUnion(other);
        if (!result.has_value())
        {
            throw std::runtime_error("Cannot compute union of non-contiguous ranges");
        }
        return *result;
    }

    /**
     * @brief - operator - Compute difference of ranges
     * @param other Range to subtract
     * @return Difference range (may be empty)
     * @note This is a simplified version; full implementation would return multirange
     */
    Range<T> operator-(const Range<T>& other) const
    {
        return difference(other);
    }
};

// Type aliases for common range types
using Int4Range = Range<int32_t>;
using Int8Range = Range<int64_t>;
using NumRange = Range<double>;

// Temporal range types - distinct wrapper structs to enable TypedValue variant storage
// (DATE, TIMESTAMP stored as int64_t internally but need distinct types for variant)
struct DateRange : public Range<int64_t> {
    DateRange() : Range<int64_t>() {}
    DateRange(std::optional<int64_t> lower, std::optional<int64_t> upper, bool lower_inc = true, bool upper_inc = false)
        : Range<int64_t>(lower, upper, lower_inc, upper_inc) {}
    DateRange(std::optional<int64_t> lower, std::optional<int64_t> upper, BoundType lower_type, BoundType upper_type)
        : Range<int64_t>(lower, upper, lower_type, upper_type) {}
};

struct TSRange : public Range<int64_t> {
    TSRange() : Range<int64_t>() {}
    TSRange(std::optional<int64_t> lower, std::optional<int64_t> upper, bool lower_inc = true, bool upper_inc = false)
        : Range<int64_t>(lower, upper, lower_inc, upper_inc) {}
    TSRange(std::optional<int64_t> lower, std::optional<int64_t> upper, BoundType lower_type, BoundType upper_type)
        : Range<int64_t>(lower, upper, lower_type, upper_type) {}
};

struct TSTZRange : public Range<int64_t> {
    TSTZRange() : Range<int64_t>() {}
    TSTZRange(std::optional<int64_t> lower, std::optional<int64_t> upper, bool lower_inc = true, bool upper_inc = false)
        : Range<int64_t>(lower, upper, lower_inc, upper_inc) {}
    TSTZRange(std::optional<int64_t> lower, std::optional<int64_t> upper, BoundType lower_type, BoundType upper_type)
        : Range<int64_t>(lower, upper, lower_type, upper_type) {}
};

} // namespace scratchbird::core
