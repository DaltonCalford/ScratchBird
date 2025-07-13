/*
 *	PROGRAM:		Range Types
 *	MODULE:			RangeTypes.h
 *	DESCRIPTION:	Range datatype support (PostgreSQL-compatible)
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by ScratchBird Development Team
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Development Team
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): _______________________________________.
 *
 */

#ifndef SB_RANGE_TYPES_H
#define SB_RANGE_TYPES_H

#include "firebird/Interface.h"
#include "sb_exception.h"
#include "classes/fb_string.h"

namespace ScratchBird {

// Range boundary flags
enum RangeBoundaryFlags {
    RANGE_EMPTY = 0x01,        // Empty range
    RANGE_LB_INC = 0x02,       // Lower bound inclusive
    RANGE_UB_INC = 0x04,       // Upper bound inclusive
    RANGE_LB_INF = 0x08,       // Lower bound is infinite
    RANGE_UB_INF = 0x10        // Upper bound is infinite
};

// Generic range template
template<typename T>
class Range {
public:
    Range();
    Range(const T& lower, const T& upper, bool lowerInc = true, bool upperInc = false);
    Range(const char* range_string);

    // Boundary access
    const T& getLower() const { return lower; }
    const T& getUpper() const { return upper; }
    bool isLowerIncluded() const { return (flags & RANGE_LB_INC) != 0; }
    bool isUpperIncluded() const { return (flags & RANGE_UB_INC) != 0; }
    bool isLowerInfinite() const { return (flags & RANGE_LB_INF) != 0; }
    bool isUpperInfinite() const { return (flags & RANGE_UB_INF) != 0; }
    bool isEmpty() const { return (flags & RANGE_EMPTY) != 0; }

    // Range operations
    bool contains(const T& value) const;
    bool contains(const Range<T>& other) const;
    bool overlaps(const Range<T>& other) const;
    bool isAdjacent(const Range<T>& other) const;
    
    Range<T> union_range(const Range<T>& other) const;
    Range<T> intersection(const Range<T>& other) const;
    Range<T> difference(const Range<T>& other) const;

    // Comparison operators
    bool operator==(const Range<T>& other) const;
    bool operator!=(const Range<T>& other) const { return !(*this == other); }
    bool operator<(const Range<T>& other) const;
    bool operator>(const Range<T>& other) const { return other < *this; }
    bool operator<=(const Range<T>& other) const { return !(other < *this); }
    bool operator>=(const Range<T>& other) const { return !(*this < other); }

    // PostgreSQL-style operators
    bool operator<<(const Range<T>& other) const { return isStrictlyLeft(other); }
    bool operator>>(const Range<T>& other) const { return isStrictlyRight(other); }
    bool operator&(const Range<T>& other) const { return overlaps(other); }

    // String representation
    void toString(string& result) const;
    string toString() const;

    // Index key generation
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return sizeof(T) * 2 + sizeof(UCHAR); }

    // Validation
    bool isValid() const;
    static bool isValidRange(const char* range_string);

private:
    T lower;
    T upper;
    UCHAR flags;

    void parseRange(const char* range_string);
    bool isStrictlyLeft(const Range<T>& other) const;
    bool isStrictlyRight(const Range<T>& other) const;
    void normalize();
    static void invalid_range();
};

// Specific range type implementations
class Int4Range : public Range<SLONG> {
public:
    Int4Range() : Range<SLONG>() {}
    Int4Range(SLONG lower, SLONG upper, bool lowerInc = true, bool upperInc = false)
        : Range<SLONG>(lower, upper, lowerInc, upperInc) {}
    Int4Range(const char* range_string) : Range<SLONG>(range_string) {}

    static ULONG getStorageSize() { return sizeof(SLONG) * 2 + sizeof(UCHAR); }
};

class Int8Range : public Range<SINT64> {
public:
    Int8Range() : Range<SINT64>() {}
    Int8Range(SINT64 lower, SINT64 upper, bool lowerInc = true, bool upperInc = false)
        : Range<SINT64>(lower, upper, lowerInc, upperInc) {}
    Int8Range(const char* range_string) : Range<SINT64>(range_string) {}

    static ULONG getStorageSize() { return sizeof(SINT64) * 2 + sizeof(UCHAR); }
};

class NumRange : public Range<double> {
public:
    NumRange() : Range<double>() {}
    NumRange(double lower, double upper, bool lowerInc = true, bool upperInc = false)
        : Range<double>(lower, upper, lowerInc, upperInc) {}
    NumRange(const char* range_string) : Range<double>(range_string) {}

    static ULONG getStorageSize() { return sizeof(double) * 2 + sizeof(UCHAR); }
};

// Timestamp range (uses GDS_TIMESTAMP)
class TsRange : public Range<ISC_TIMESTAMP> {
public:
    TsRange() : Range<ISC_TIMESTAMP>() {}
    TsRange(const ISC_TIMESTAMP& lower, const ISC_TIMESTAMP& upper, 
            bool lowerInc = true, bool upperInc = false)
        : Range<ISC_TIMESTAMP>(lower, upper, lowerInc, upperInc) {}
    TsRange(const char* range_string) : Range<ISC_TIMESTAMP>(range_string) {}

    static ULONG getStorageSize() { return sizeof(ISC_TIMESTAMP) * 2 + sizeof(UCHAR); }
};

// Timestamp with timezone range
class TstzRange : public Range<ISC_TIMESTAMP_TZ> {
public:
    TstzRange() : Range<ISC_TIMESTAMP_TZ>() {}
    TstzRange(const ISC_TIMESTAMP_TZ& lower, const ISC_TIMESTAMP_TZ& upper,
              bool lowerInc = true, bool upperInc = false)
        : Range<ISC_TIMESTAMP_TZ>(lower, upper, lowerInc, upperInc) {}
    TstzRange(const char* range_string) : Range<ISC_TIMESTAMP_TZ>(range_string) {}

    static ULONG getStorageSize() { return sizeof(ISC_TIMESTAMP_TZ) * 2 + sizeof(UCHAR); }
};

// Date range (uses GDS_DATE)
class DateRange : public Range<ISC_DATE> {
public:
    DateRange() : Range<ISC_DATE>() {}
    DateRange(const ISC_DATE& lower, const ISC_DATE& upper,
              bool lowerInc = true, bool upperInc = false)
        : Range<ISC_DATE>(lower, upper, lowerInc, upperInc) {}
    DateRange(const char* range_string) : Range<ISC_DATE>(range_string) {}

    static ULONG getStorageSize() { return sizeof(ISC_DATE) * 2 + sizeof(UCHAR); }
};

// CITEXT implementation
class CiText {
public:
    CiText();
    CiText(const char* text);
    CiText(const string& text);

    // Assignment and conversion
    CiText& operator=(const char* text);
    CiText& operator=(const string& text);
    CiText& operator=(const CiText& other);

    // String representation
    void toString(string& result) const;
    string toString() const;
    const char* c_str() const { return text_data.c_str(); }

    // Case-insensitive comparison
    bool operator==(const CiText& other) const;
    bool operator!=(const CiText& other) const { return !(*this == other); }
    bool operator<(const CiText& other) const;
    bool operator>(const CiText& other) const { return other < *this; }
    bool operator<=(const CiText& other) const { return !(other < *this); }
    bool operator>=(const CiText& other) const { return !(*this < other); }

    // Case-insensitive operations with strings
    bool operator==(const char* str) const;
    bool operator==(const string& str) const;

    // String operations
    ULONG length() const { return text_data.length(); }
    bool empty() const { return text_data.empty(); }
    void clear() { text_data.clear(); }

    // Pattern matching (case-insensitive)
    bool like(const char* pattern) const;
    bool ilike(const char* pattern) const { return like(pattern); }  // Same as like for CITEXT

    // Index key generation
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return MAX_KEY_SIZE; }

    // Validation
    bool isValid() const { return true; }  // All strings are valid CITEXT

private:
    string text_data;
    static constexpr ULONG MAX_KEY_SIZE = 252;  // Maximum index key size

    // Case-insensitive string comparison
    static int compareIgnoreCase(const string& a, const string& b);
    static string toLower(const string& str);
};

} // namespace ScratchBird

#endif // SB_RANGE_TYPES_H