/*
 *	PROGRAM:		Advanced Array Types
 *	MODULE:			AdvancedArrays.h
 *	DESCRIPTION:	Multi-dimensional arrays and advanced operations
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

#ifndef SB_ADVANCED_ARRAYS_H
#define SB_ADVANCED_ARRAYS_H

#include "firebird/Interface.h"
#include "sb_exception.h"
#include "classes/fb_string.h"
#include <vector>

namespace ScratchBird {

// Array dimension structure
struct ArrayDimension {
    SLONG lower_bound;      // Lower bound index
    SLONG upper_bound;      // Upper bound index
    ULONG element_count;    // Number of elements in this dimension
    
    ArrayDimension(SLONG lower = 1, SLONG upper = 1) 
        : lower_bound(lower), upper_bound(upper) {
        element_count = (upper >= lower) ? (upper - lower + 1) : 0;
    }
    
    bool isValid() const {
        return upper_bound >= lower_bound && element_count > 0;
    }
};

// Array slice specification
struct ArraySlice {
    SLONG start_index;      // Starting index
    SLONG end_index;        // Ending index  
    SLONG step;             // Step size (default 1)
    
    ArraySlice(SLONG start = 1, SLONG end = 1, SLONG step_size = 1)
        : start_index(start), end_index(end), step(step_size) {}
        
    bool isValid() const {
        return step != 0 && 
               ((step > 0 && end_index >= start_index) || 
                (step < 0 && end_index <= start_index));
    }
    
    ULONG getElementCount() const {
        if (!isValid()) return 0;
        if (step > 0) {
            return ((end_index - start_index) / step) + 1;
        } else {
            return ((start_index - end_index) / (-step)) + 1;
        }
    }
};

// Multi-dimensional array class
template<typename T>
class MultiDimensionalArray {
public:
    MultiDimensionalArray();
    MultiDimensionalArray(const std::vector<ArrayDimension>& dimensions);
    MultiDimensionalArray(const char* array_literal);
    
    // Dimension management
    void setDimensions(const std::vector<ArrayDimension>& dimensions);
    const std::vector<ArrayDimension>& getDimensions() const { return dimensions_; }
    UCHAR getDimensionCount() const { return static_cast<UCHAR>(dimensions_.size()); }
    ULONG getTotalElementCount() const;
    
    // Element access
    T& operator[](const std::vector<SLONG>& indices);
    const T& operator[](const std::vector<SLONG>& indices) const;
    T& getElement(const std::vector<SLONG>& indices);
    const T& getElement(const std::vector<SLONG>& indices) const;
    
    // Array slicing
    MultiDimensionalArray<T> slice(const std::vector<ArraySlice>& slices) const;
    MultiDimensionalArray<T> slice(UCHAR dimension, const ArraySlice& slice_spec) const;
    
    // Array operations (PostgreSQL-compatible)
    bool contains(const MultiDimensionalArray<T>& other) const;        // @>
    bool isContainedBy(const MultiDimensionalArray<T>& other) const;   // <@
    bool overlaps(const MultiDimensionalArray<T>& other) const;        // &&
    bool equals(const MultiDimensionalArray<T>& other) const;          // =
    
    // Element operations
    bool containsElement(const T& element) const;
    void appendElement(const T& element);                               // array_append
    void prependElement(const T& element);                              // array_prepend
    void removeElement(const T& element);                               // array_remove
    
    // Concatenation
    MultiDimensionalArray<T> concatenate(const MultiDimensionalArray<T>& other) const; // ||
    
    // Aggregation functions
    ULONG cardinality() const;                                          // array_length
    std::vector<SLONG> getDimensionBounds(UCHAR dimension) const;       // array_bounds
    T getElement(SLONG linear_index) const;                            // Array subscript
    
    // String representation
    void toString(string& result) const;
    string toString() const;
    
    // Array literal parsing
    void parseArrayLiteral(const char* literal);
    
    // Storage and indexing
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return 252; }
    
    // Validation
    bool isValid() const;
    static bool isValidArrayLiteral(const char* literal);
    
private:
    std::vector<ArrayDimension> dimensions_;
    std::vector<T> data_;
    
    // Helper functions
    ULONG calculateLinearIndex(const std::vector<SLONG>& indices) const;
    std::vector<SLONG> calculateMultiIndex(ULONG linear_index) const;
    void validateIndices(const std::vector<SLONG>& indices) const;
    void reshapeArray();
    static void invalid_array();
};

// Specialized array slice type
class ArraySliceType {
public:
    ArraySliceType();
    ArraySliceType(const MultiDimensionalArray<string>& source_array, 
                   const std::vector<ArraySlice>& slice_specs);
    
    // Slice operations
    template<typename T>
    MultiDimensionalArray<T> apply(const MultiDimensionalArray<T>& source) const;
    
    void toString(string& result) const;
    string toString() const;
    
    // Storage
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getStorageSize() { return sizeof(ArraySlice) * MAX_DIMENSIONS + sizeof(UCHAR); }
    
private:
    std::vector<ArraySlice> slice_specs_;
    static constexpr UCHAR MAX_DIMENSIONS = 16;
};

// Array utility functions
class ArrayUtils {
public:
    // Array construction
    template<typename T>
    static MultiDimensionalArray<T> createArray(const std::vector<T>& elements, 
                                               const std::vector<ArrayDimension>& dimensions);
    
    // Array comparison
    template<typename T>
    static bool arraysEqual(const MultiDimensionalArray<T>& a, const MultiDimensionalArray<T>& b);
    
    // Array searching
    template<typename T>
    static SLONG findElement(const MultiDimensionalArray<T>& array, const T& element);
    
    // Array aggregation
    template<typename T>
    static T arraySum(const MultiDimensionalArray<T>& array);
    
    template<typename T>
    static T arrayAvg(const MultiDimensionalArray<T>& array);
    
    template<typename T>
    static T arrayMin(const MultiDimensionalArray<T>& array);
    
    template<typename T>
    static T arrayMax(const MultiDimensionalArray<T>& array);
    
    // Array transformation
    template<typename T>
    static MultiDimensionalArray<T> arrayReverse(const MultiDimensionalArray<T>& array);
    
    template<typename T>
    static MultiDimensionalArray<T> arraySort(const MultiDimensionalArray<T>& array, bool ascending = true);
    
    // Array statistics
    template<typename T>
    static std::vector<T> arrayToVector(const MultiDimensionalArray<T>& array);
    
    template<typename T>
    static MultiDimensionalArray<T> vectorToArray(const std::vector<T>& vector, 
                                                  const std::vector<ArrayDimension>& dimensions);
};

// Type aliases for common array types
using IntArray = MultiDimensionalArray<SLONG>;
using TextArray = MultiDimensionalArray<string>;
using NumericArray = MultiDimensionalArray<double>;
using BooleanArray = MultiDimensionalArray<bool>;

} // namespace ScratchBird

#endif // SB_ADVANCED_ARRAYS_H