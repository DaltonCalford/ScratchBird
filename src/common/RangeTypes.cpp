/*
 *	PROGRAM:		Range Types
 *	MODULE:			RangeTypes.cpp
 *	DESCRIPTION:	Range datatype implementation
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

#include "firebird.h"
#include "RangeTypes.h"
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ScratchBird {

// Generic Range template implementation

template<typename T>
Range<T>::Range() : flags(RANGE_EMPTY) {
    memset(&lower, 0, sizeof(T));
    memset(&upper, 0, sizeof(T));
}

template<typename T>
Range<T>::Range(const T& lower_val, const T& upper_val, bool lowerInc, bool upperInc) 
    : lower(lower_val), upper(upper_val), flags(0) {
    
    if (lowerInc) flags |= RANGE_LB_INC;
    if (upperInc) flags |= RANGE_UB_INC;
    
    normalize();
}

template<typename T>
Range<T>::Range(const char* range_string) : flags(0) {
    parseRange(range_string);
}

template<typename T>
bool Range<T>::contains(const T& value) const {
    if (isEmpty()) return false;
    
    // Check lower bound
    if (!isLowerInfinite()) {
        if (isLowerIncluded()) {
            if (value < lower) return false;
        } else {
            if (value <= lower) return false;
        }
    }
    
    // Check upper bound
    if (!isUpperInfinite()) {
        if (isUpperIncluded()) {
            if (value > upper) return false;
        } else {
            if (value >= upper) return false;
        }
    }
    
    return true;
}

template<typename T>
bool Range<T>::contains(const Range<T>& other) const {
    if (isEmpty() || other.isEmpty()) return false;
    
    // Check if this range contains other's bounds
    if (!other.isLowerInfinite()) {
        if (!contains(other.lower)) {
            // Special case: if other's lower bound is not included,
            // we might still contain it if our bounds match exactly
            if (!(other.isLowerIncluded() == false && 
                  !isLowerInfinite() && lower == other.lower && isLowerIncluded())) {
                return false;
            }
        }
    }
    
    if (!other.isUpperInfinite()) {
        if (!contains(other.upper)) {
            // Special case for upper bound
            if (!(other.isUpperIncluded() == false && 
                  !isUpperInfinite() && upper == other.upper && isUpperIncluded())) {
                return false;
            }
        }
    }
    
    return true;
}

template<typename T>
bool Range<T>::overlaps(const Range<T>& other) const {
    if (isEmpty() || other.isEmpty()) return false;
    
    // Check if ranges are disjoint
    if (!isUpperInfinite() && !other.isLowerInfinite()) {
        if (upper < other.lower) return false;
        if (upper == other.lower && (!isUpperIncluded() || !other.isLowerIncluded())) {
            return false;
        }
    }
    
    if (!isLowerInfinite() && !other.isUpperInfinite()) {
        if (lower > other.upper) return false;
        if (lower == other.upper && (!isLowerIncluded() || !other.isUpperIncluded())) {
            return false;
        }
    }
    
    return true;
}

template<typename T>
void Range<T>::toString(string& result) const {
    if (isEmpty()) {
        result = "empty";
        return;
    }
    
    result.clear();
    
    // Opening bracket
    result += isLowerIncluded() ? "[" : "(";
    
    // Lower bound
    if (isLowerInfinite()) {
        result += "";  // Nothing for negative infinity
    } else {
        // Convert T to string - this is type-specific
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%ld", static_cast<long>(lower));
        result += buffer;
    }
    
    result += ",";
    
    // Upper bound
    if (isUpperInfinite()) {
        result += "";  // Nothing for positive infinity
    } else {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%ld", static_cast<long>(upper));
        result += buffer;
    }
    
    // Closing bracket
    result += isUpperIncluded() ? "]" : ")";
}

template<typename T>
void Range<T>::parseRange(const char* range_string) {
    if (!range_string) {
        invalid_range();
        return;
    }
    
    string range_str(range_string);
    
    // Handle empty range
    if (range_str == "empty") {
        flags = RANGE_EMPTY;
        return;
    }
    
    // Parse format: [lower,upper) or (lower,upper] etc.
    if (range_str.length() < 3) {
        invalid_range();
        return;
    }
    
    // Check opening bracket
    char open_bracket = range_str[0];
    if (open_bracket == '[') {
        flags |= RANGE_LB_INC;
    } else if (open_bracket != '(') {
        invalid_range();
        return;
    }
    
    // Check closing bracket
    char close_bracket = range_str[range_str.length() - 1];
    if (close_bracket == ']') {
        flags |= RANGE_UB_INC;
    } else if (close_bracket != ')') {
        invalid_range();
        return;
    }
    
    // Extract content between brackets
    string content = range_str.substr(1, range_str.length() - 2);
    
    // Find comma separator
    size_t comma_pos = content.find(',');
    if (comma_pos == string::npos) {
        invalid_range();
        return;
    }
    
    string lower_str = content.substr(0, comma_pos);
    string upper_str = content.substr(comma_pos + 1);
    
    // Parse bounds (simplified - would need type-specific parsing)
    if (lower_str.empty()) {
        flags |= RANGE_LB_INF;
    } else {
        // Type-specific parsing would go here
        lower = static_cast<T>(atol(lower_str.c_str()));
    }
    
    if (upper_str.empty()) {
        flags |= RANGE_UB_INF;
    } else {
        upper = static_cast<T>(atol(upper_str.c_str()));
    }
    
    normalize();
}

template<typename T>
void Range<T>::normalize() {
    // Check for empty range
    if (!isLowerInfinite() && !isUpperInfinite()) {
        if (lower > upper) {
            flags |= RANGE_EMPTY;
            return;
        }
        if (lower == upper && (!isLowerIncluded() || !isUpperIncluded())) {
            flags |= RANGE_EMPTY;
            return;
        }
    }
}

template<typename T>
void Range<T>::invalid_range() {
    throw std::invalid_argument("Invalid range format");
}

// Explicit template instantiations
template class Range<SLONG>;
template class Range<SINT64>;
template class Range<double>;
template class Range<ISC_TIMESTAMP>;
template class Range<ISC_TIMESTAMP_TZ>;
template class Range<ISC_DATE>;

// CITEXT implementation

CiText::CiText() {}

CiText::CiText(const char* text) {
    if (text) {
        text_data = text;
    }
}

CiText::CiText(const string& text) : text_data(text) {}

CiText& CiText::operator=(const char* text) {
    if (text) {
        text_data = text;
    } else {
        text_data.clear();
    }
    return *this;
}

CiText& CiText::operator=(const string& text) {
    text_data = text;
    return *this;
}

CiText& CiText::operator=(const CiText& other) {
    text_data = other.text_data;
    return *this;
}

void CiText::toString(string& result) const {
    result = text_data;
}

string CiText::toString() const {
    return text_data;
}

bool CiText::operator==(const CiText& other) const {
    return compareIgnoreCase(text_data, other.text_data) == 0;
}

bool CiText::operator<(const CiText& other) const {
    return compareIgnoreCase(text_data, other.text_data) < 0;
}

bool CiText::operator==(const char* str) const {
    if (!str) return text_data.empty();
    string other_str(str);
    return compareIgnoreCase(text_data, other_str) == 0;
}

bool CiText::operator==(const string& str) const {
    return compareIgnoreCase(text_data, str) == 0;
}

bool CiText::like(const char* pattern) const {
    if (!pattern) return false;
    
    // Simplified LIKE implementation (case-insensitive)
    string lower_text = toLower(text_data);
    string lower_pattern = toLower(string(pattern));
    
    // Simple wildcard matching
    if (lower_pattern == "%") return true;
    if (lower_pattern.find('%') == string::npos && lower_pattern.find('_') == string::npos) {
        return lower_text == lower_pattern;
    }
    
    // More complex pattern matching would go here
    return false;
}

ULONG CiText::makeIndexKey(vary* buf) const {
    if (!buf) return 0;
    
    // Convert to lowercase for case-insensitive indexing
    string lower_text = toLower(text_data);
    ULONG copy_length = std::min(static_cast<ULONG>(lower_text.length()), 
                                 static_cast<ULONG>(MAX_KEY_SIZE - sizeof(USHORT)));
    
    buf->vary_length = static_cast<USHORT>(copy_length);
    if (copy_length > 0) {
        memcpy(buf->vary_string, lower_text.c_str(), copy_length);
    }
    
    return sizeof(USHORT) + copy_length;
}

int CiText::compareIgnoreCase(const string& a, const string& b) {
    string lower_a = toLower(a);
    string lower_b = toLower(b);
    
    if (lower_a < lower_b) return -1;
    if (lower_a > lower_b) return 1;
    return 0;
}

string CiText::toLower(const string& str) {
    string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace ScratchBird