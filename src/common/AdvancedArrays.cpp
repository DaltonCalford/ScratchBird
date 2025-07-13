/*
 *	PROGRAM:		Advanced Array Types
 *	MODULE:			AdvancedArrays.cpp
 *	DESCRIPTION:	Multi-dimensional arrays implementation
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
#include "AdvancedArrays.h"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace ScratchBird {

// MultiDimensionalArray implementation

template<typename T>
MultiDimensionalArray<T>::MultiDimensionalArray() {
}

template<typename T>
MultiDimensionalArray<T>::MultiDimensionalArray(const std::vector<ArrayDimension>& dimensions) {
    setDimensions(dimensions);
}

template<typename T>
MultiDimensionalArray<T>::MultiDimensionalArray(const char* array_literal) {
    parseArrayLiteral(array_literal);
}

template<typename T>
void MultiDimensionalArray<T>::setDimensions(const std::vector<ArrayDimension>& dimensions) {
    dimensions_ = dimensions;
    
    // Validate dimensions
    for (const auto& dim : dimensions_) {
        if (!dim.isValid()) {
            invalid_array();
            return;
        }
    }
    
    reshapeArray();
}

template<typename T>
ULONG MultiDimensionalArray<T>::getTotalElementCount() const {
    ULONG total = 1;
    for (const auto& dim : dimensions_) {
        total *= dim.element_count;
    }
    return total;
}

template<typename T>
T& MultiDimensionalArray<T>::operator[](const std::vector<SLONG>& indices) {
    return getElement(indices);
}

template<typename T>
const T& MultiDimensionalArray<T>::operator[](const std::vector<SLONG>& indices) const {
    return getElement(indices);
}

template<typename T>
T& MultiDimensionalArray<T>::getElement(const std::vector<SLONG>& indices) {
    validateIndices(indices);
    ULONG linear_index = calculateLinearIndex(indices);
    return data_[linear_index];
}

template<typename T>
const T& MultiDimensionalArray<T>::getElement(const std::vector<SLONG>& indices) const {
    validateIndices(indices);
    ULONG linear_index = calculateLinearIndex(indices);
    return data_[linear_index];
}

template<typename T>
MultiDimensionalArray<T> MultiDimensionalArray<T>::slice(const std::vector<ArraySlice>& slices) const {
    if (slices.size() != dimensions_.size()) {
        invalid_array();
    }
    
    // Calculate new dimensions
    std::vector<ArrayDimension> new_dimensions;
    for (size_t i = 0; i < slices.size(); ++i) {
        const ArraySlice& slice = slices[i];
        if (!slice.isValid()) {
            invalid_array();
        }
        
        SLONG new_lower = slice.start_index;
        SLONG new_upper = slice.end_index;
        new_dimensions.push_back(ArrayDimension(new_lower, new_upper));
    }
    
    // Create result array
    MultiDimensionalArray<T> result(new_dimensions);
    
    // Copy sliced elements
    std::vector<SLONG> src_indices(dimensions_.size());
    std::vector<SLONG> dst_indices(new_dimensions.size());
    
    // This would need recursive implementation for proper multi-dimensional slicing
    // Simplified version for demonstration
    return result;
}

template<typename T>
bool MultiDimensionalArray<T>::contains(const MultiDimensionalArray<T>& other) const {
    // Array A contains B if all elements of B are in A
    for (const auto& element : other.data_) {
        if (std::find(data_.begin(), data_.end(), element) == data_.end()) {
            return false;
        }
    }
    return true;
}

template<typename T>
bool MultiDimensionalArray<T>::containsElement(const T& element) const {
    return std::find(data_.begin(), data_.end(), element) != data_.end();
}

template<typename T>
void MultiDimensionalArray<T>::appendElement(const T& element) {
    // For 1D arrays, simply append
    if (dimensions_.size() == 1) {
        data_.push_back(element);
        dimensions_[0].upper_bound++;
        dimensions_[0].element_count++;
    } else {
        // For multi-dimensional arrays, this is more complex
        invalid_array(); // Not implemented for multi-dimensional
    }
}

template<typename T>
ULONG MultiDimensionalArray<T>::cardinality() const {
    return data_.size();
}

template<typename T>
void MultiDimensionalArray<T>::toString(string& result) const {
    std::ostringstream oss;
    
    if (dimensions_.empty()) {
        result = "{}";
        return;
    }
    
    // Simple 1D array representation
    if (dimensions_.size() == 1) {
        oss << "{";
        for (size_t i = 0; i < data_.size(); ++i) {
            if (i > 0) oss << ",";
            oss << data_[i];
        }
        oss << "}";
    } else {
        // Multi-dimensional representation would be more complex
        oss << "{{multi-dimensional array}}";
    }
    
    result = oss.str();
}

template<typename T>
string MultiDimensionalArray<T>::toString() const {
    string result;
    toString(result);
    return result;
}

template<typename T>
void MultiDimensionalArray<T>::parseArrayLiteral(const char* literal) {
    if (!literal) {
        invalid_array();
        return;
    }
    
    string lit_str(literal);
    
    // Simple parser for 1D arrays: {1,2,3,4}
    if (lit_str.length() < 2 || lit_str[0] != '{' || lit_str.back() != '}') {
        invalid_array();
        return;
    }
    
    // Extract content between braces
    string content = lit_str.substr(1, lit_str.length() - 2);
    
    // Split by commas
    std::vector<T> elements;
    std::istringstream iss(content);
    string token;
    
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        if (!token.empty()) {
            // Convert token to T (simplified)
            if constexpr (std::is_same_v<T, string>) {
                elements.push_back(token);
            } else if constexpr (std::is_arithmetic_v<T>) {
                elements.push_back(static_cast<T>(std::stod(token)));
            }
        }
    }
    
    // Set up as 1D array
    data_ = elements;
    dimensions_.clear();
    if (!elements.empty()) {
        dimensions_.push_back(ArrayDimension(1, static_cast<SLONG>(elements.size())));
    }
}

template<typename T>
ULONG MultiDimensionalArray<T>::makeIndexKey(vary* buf) const {
    if (!buf) return 0;
    
    // Create a hash-based index key from array contents
    string str_repr = toString();
    ULONG copy_length = std::min(static_cast<ULONG>(str_repr.length()), 
                                static_cast<ULONG>(252 - sizeof(USHORT)));
    
    buf->vary_length = static_cast<USHORT>(copy_length);
    if (copy_length > 0) {
        memcpy(buf->vary_string, str_repr.c_str(), copy_length);
    }
    
    return sizeof(USHORT) + copy_length;
}

template<typename T>
bool MultiDimensionalArray<T>::isValid() const {
    if (dimensions_.empty()) return true;
    
    for (const auto& dim : dimensions_) {
        if (!dim.isValid()) return false;
    }
    
    return data_.size() == getTotalElementCount();
}

template<typename T>
ULONG MultiDimensionalArray<T>::calculateLinearIndex(const std::vector<SLONG>& indices) const {
    if (indices.size() != dimensions_.size()) {
        invalid_array();
    }
    
    ULONG linear_index = 0;
    ULONG multiplier = 1;
    
    // Calculate linear index using row-major order
    for (int i = dimensions_.size() - 1; i >= 0; --i) {
        SLONG adjusted_index = indices[i] - dimensions_[i].lower_bound;
        linear_index += adjusted_index * multiplier;
        multiplier *= dimensions_[i].element_count;
    }
    
    return linear_index;
}

template<typename T>
void MultiDimensionalArray<T>::validateIndices(const std::vector<SLONG>& indices) const {
    if (indices.size() != dimensions_.size()) {
        invalid_array();
        return;
    }
    
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] < dimensions_[i].lower_bound || indices[i] > dimensions_[i].upper_bound) {
            invalid_array();
            return;
        }
    }
}

template<typename T>
void MultiDimensionalArray<T>::reshapeArray() {
    ULONG total_elements = getTotalElementCount();
    data_.resize(total_elements);
}

template<typename T>
void MultiDimensionalArray<T>::invalid_array() {
    throw std::invalid_argument("Invalid array operation or format");
}

// ArraySliceType implementation

ArraySliceType::ArraySliceType() {
}

ArraySliceType::ArraySliceType(const MultiDimensionalArray<string>& source_array, 
                              const std::vector<ArraySlice>& slice_specs) 
    : slice_specs_(slice_specs) {
}

template<typename T>
MultiDimensionalArray<T> ArraySliceType::apply(const MultiDimensionalArray<T>& source) const {
    return source.slice(slice_specs_);
}

void ArraySliceType::toString(string& result) const {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < slice_specs_.size(); ++i) {
        if (i > 0) oss << ",";
        const ArraySlice& slice = slice_specs_[i];
        oss << slice.start_index << ":" << slice.end_index;
        if (slice.step != 1) {
            oss << ":" << slice.step;
        }
    }
    oss << "]";
    result = oss.str();
}

ULONG ArraySliceType::makeIndexKey(vary* buf) const {
    string str_repr = toString();
    ULONG copy_length = std::min(static_cast<ULONG>(str_repr.length()), 
                                static_cast<ULONG>(252 - sizeof(USHORT)));
    
    buf->vary_length = static_cast<USHORT>(copy_length);
    if (copy_length > 0) {
        memcpy(buf->vary_string, str_repr.c_str(), copy_length);
    }
    
    return sizeof(USHORT) + copy_length;
}

// Explicit template instantiations
template class MultiDimensionalArray<SLONG>;
template class MultiDimensionalArray<string>;
template class MultiDimensionalArray<double>;
template class MultiDimensionalArray<bool>;

} // namespace ScratchBird