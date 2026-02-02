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

/**
 * ScratchBird Parser - Shared Types
 *
 * This header contains type definitions shared across ScratchBird parsers,
 * the optimizer, and the executor. These types are independent of the
 * specific AST implementation and can be used by any component.
 *
 * This file enables the optimizer and plan nodes to function independently
 * of parser implementation details while still using compatible type definitions.
 */

#include <cstdint>

namespace scratchbird::parser {

// =============================================================================
// Join Types
// =============================================================================

/**
 * Join type for JOIN operations
 */
enum class JoinType : uint8_t {
    INNER,          // INNER JOIN
    LEFT,           // LEFT OUTER JOIN
    RIGHT,          // RIGHT OUTER JOIN
    FULL,           // FULL OUTER JOIN
    CROSS,          // CROSS JOIN
    NATURAL,        // NATURAL JOIN
    NATURAL_LEFT,   // NATURAL LEFT JOIN
    NATURAL_RIGHT,  // NATURAL RIGHT JOIN
    NATURAL_FULL    // NATURAL FULL JOIN
};

// =============================================================================
// Window Function Types
// =============================================================================

/**
 * Window function type
 */
enum class WindowFunc : uint8_t {
    ROW_NUMBER,
    RANK,
    DENSE_RANK,
    LAG,
    LEAD,
    FIRST_VALUE,
    LAST_VALUE,
    NTH_VALUE,
    CUME_DIST,
    PERCENT_RANK,
    NTILE,
    // Aggregate functions that can be used as window functions
    SUM,
    AVG,
    MIN,
    MAX,
    COUNT,
    COUNT_STAR,
    STDDEV,
    STDDEV_POP,
    STDDEV_SAMP,
    VARIANCE,
    VAR_POP,
    VAR_SAMP,
    STRING_AGG,
    ARRAY_AGG
};

/**
 * Window frame boundary type
 */
enum class FrameBoundaryType : uint8_t {
    UNBOUNDED_PRECEDING,
    PRECEDING,
    CURRENT_ROW,
    FOLLOWING,
    UNBOUNDED_FOLLOWING
};

/**
 * Window frame mode
 */
enum class FrameMode : uint8_t {
    ROWS,    // Physical row-based frames
    RANGE,   // Value range-based frames
    GROUPS   // Peer group-based frames (SQL:2011)
};

/**
 * Window frame exclusion (SQL:2011)
 */
enum class FrameExclusion : uint8_t {
    NO_OTHERS,      // EXCLUDE NO OTHERS (default)
    CURRENT_ROW,    // EXCLUDE CURRENT ROW
    GROUP,          // EXCLUDE GROUP
    TIES            // EXCLUDE TIES
};

// =============================================================================
// Subquery Types
// =============================================================================

/**
 * Subquery type for subquery expressions
 */
enum class SubqueryType : uint8_t {
    SCALAR,      // Returns single value: (SELECT col FROM ...)
    EXISTS,      // Returns boolean: EXISTS (SELECT ...)
    IN,          // Membership test: col IN (SELECT ...)
    NOT_IN,      // Membership test: col NOT IN (SELECT ...)
    ARRAY        // Returns array: ARRAY(SELECT ...)
};

// =============================================================================
// Grouping Types
// =============================================================================

/**
 * Grouping type for GROUP BY clause
 */
enum class GroupingType : uint8_t {
    STANDARD,        // Regular GROUP BY
    ROLLUP,          // GROUP BY ROLLUP(...)
    CUBE,            // GROUP BY CUBE(...)
    GROUPING_SETS    // GROUP BY GROUPING SETS(...)
};

// =============================================================================
// Sort Order Types
// =============================================================================

/**
 * Sort order for ORDER BY
 */
enum class SortOrder : uint8_t {
    ASC,
    DESC
};

/**
 * NULL ordering for ORDER BY
 */
enum class NullsOrder : uint8_t {
    DEFAULT,      // Database default
    NULLS_FIRST,
    NULLS_LAST
};

// =============================================================================
// Note: AST-level structs (FrameBoundary, OrderByItem, WindowSpec) remain
// in ast.h as they contain Expression* pointers. These shared types are
// enums only, which can be used by V2 ASTs, emulated parser ASTs, and plan nodes.
// =============================================================================

// =============================================================================
// Conversion Helpers
// =============================================================================

/**
 * Convert JoinType to string for debugging/display
 */
inline const char* joinTypeToString(JoinType type) {
    switch (type) {
        case JoinType::INNER:         return "INNER";
        case JoinType::LEFT:          return "LEFT";
        case JoinType::RIGHT:         return "RIGHT";
        case JoinType::FULL:          return "FULL";
        case JoinType::CROSS:         return "CROSS";
        case JoinType::NATURAL:       return "NATURAL";
        case JoinType::NATURAL_LEFT:  return "NATURAL LEFT";
        case JoinType::NATURAL_RIGHT: return "NATURAL RIGHT";
        case JoinType::NATURAL_FULL:  return "NATURAL FULL";
        default: return "UNKNOWN";
    }
}

/**
 * Convert WindowFunc to string for debugging/display
 */
inline const char* windowFuncToString(WindowFunc func) {
    switch (func) {
        case WindowFunc::ROW_NUMBER:   return "ROW_NUMBER";
        case WindowFunc::RANK:         return "RANK";
        case WindowFunc::DENSE_RANK:   return "DENSE_RANK";
        case WindowFunc::LAG:          return "LAG";
        case WindowFunc::LEAD:         return "LEAD";
        case WindowFunc::FIRST_VALUE:  return "FIRST_VALUE";
        case WindowFunc::LAST_VALUE:   return "LAST_VALUE";
        case WindowFunc::NTH_VALUE:    return "NTH_VALUE";
        case WindowFunc::CUME_DIST:    return "CUME_DIST";
        case WindowFunc::PERCENT_RANK: return "PERCENT_RANK";
        case WindowFunc::NTILE:        return "NTILE";
        case WindowFunc::SUM:          return "SUM";
        case WindowFunc::AVG:          return "AVG";
        case WindowFunc::MIN:          return "MIN";
        case WindowFunc::MAX:          return "MAX";
        case WindowFunc::COUNT:        return "COUNT";
        case WindowFunc::COUNT_STAR:   return "COUNT(*)";
        case WindowFunc::STDDEV:       return "STDDEV";
        case WindowFunc::STDDEV_POP:   return "STDDEV_POP";
        case WindowFunc::STDDEV_SAMP:  return "STDDEV_SAMP";
        case WindowFunc::VARIANCE:     return "VARIANCE";
        case WindowFunc::VAR_POP:      return "VAR_POP";
        case WindowFunc::VAR_SAMP:     return "VAR_SAMP";
        case WindowFunc::STRING_AGG:   return "STRING_AGG";
        case WindowFunc::ARRAY_AGG:    return "ARRAY_AGG";
        default: return "UNKNOWN";
    }
}

/**
 * Convert FrameMode to string
 */
inline const char* frameModeToString(FrameMode mode) {
    switch (mode) {
        case FrameMode::ROWS:   return "ROWS";
        case FrameMode::RANGE:  return "RANGE";
        case FrameMode::GROUPS: return "GROUPS";
        default: return "UNKNOWN";
    }
}

/**
 * Convert SubqueryType to string
 */
inline const char* subqueryTypeToString(SubqueryType type) {
    switch (type) {
        case SubqueryType::SCALAR: return "SCALAR";
        case SubqueryType::EXISTS: return "EXISTS";
        case SubqueryType::IN:     return "IN";
        case SubqueryType::NOT_IN: return "NOT IN";
        case SubqueryType::ARRAY:  return "ARRAY";
        default: return "UNKNOWN";
    }
}

/**
 * Convert GroupingType to string
 */
inline const char* groupingTypeToString(GroupingType type) {
    switch (type) {
        case GroupingType::STANDARD:      return "GROUP BY";
        case GroupingType::ROLLUP:        return "ROLLUP";
        case GroupingType::CUBE:          return "CUBE";
        case GroupingType::GROUPING_SETS: return "GROUPING SETS";
        default: return "UNKNOWN";
    }
}

}  // namespace scratchbird::parser
