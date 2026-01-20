# PostgreSQL Array Type Specification
**Version:** 1.0
**Date:** November 18, 2025
**Standard:** PostgreSQL Compatible
**Purpose:** Define implementation requirements for multidimensional ARRAY types in ScratchBird

---

## Table of Contents

1. [Overview](#overview)
2. [ScratchBird Canonical Encoding (Alpha)](#scratchbird-canonical-encoding-alpha)
3. [PostgreSQL Array Fundamentals](#postgresql-array-fundamentals)
4. [Binary Storage Format](#binary-storage-format)
5. [C++ Structure Specifications](#c-structure-specifications)
6. [TypedValue Integration](#typedvalue-integration)
7. [Serialization Implementation](#serialization-implementation)
8. [SQL Syntax and Operations](#sql-syntax-and-operations)
9. [Validation and Constraints](#validation-and-constraints)

---

## Overview

PostgreSQL arrays are multidimensional collections of elements of a single type. Unlike most SQL databases, PostgreSQL allows array columns with variable dimensions and supports nested arrays.

### Key Features

- **Multidimensional:** Support 1D, 2D, 3D, ... nD arrays
- **Homogeneous:** All elements must be the same type
- **Variable bounds:** Lower bound not fixed at 0 or 1
- **NULL elements:** Individual array elements can be NULL
- **Dynamic sizing:** No fixed maximum size

### Current Implementation Status

**Existing in ScratchBird:**
- ✅ `ArrayValue` class fully implemented (array.h)
- ✅ `Array::encode()` and `Array::decode()` methods
- ✅ Element types: INT32, INT64, FLOAT32, FLOAT64, STRING, BOOL
- ✅ Multi-dimensional indexing and slicing
- ✅ `TypedValue` ARRAY encoding via canonical plain-value serialization

**Missing:**
- ❌ PostgreSQL ArrayType binary layout on disk (dimensions/lower bounds/null bitmap)
- ❌ SQL syntax support for array literals and constructors
- ❌ Subscript operators in SQL
- ❌ Element-type enforcement and dimension/bounds validation

---

## ScratchBird Canonical Encoding (Alpha)

ScratchBird currently uses a simplified, typed element list for ARRAY storage and does **not** persist the PostgreSQL ArrayType binary layout.

**Canonical encoding (see `DATA_TYPE_PERSISTENCE_AND_CASTS.md`):**
- uint32 element_count
- per element: uint8 is_null, uint16 type_code, uint32 payload_len, payload_bytes

**Notes:**
- Dimensions and lower bounds are not stored.
- Arrays are effectively 1-D in storage; multi-dimensional semantics are a future enhancement.
- Element type is not enforced at storage time; each element carries its own type code.

This document remains the **target** for PostgreSQL compatibility, but on-disk compatibility is not yet implemented.

---

## PostgreSQL Array Fundamentals

### Text Representation

```sql
-- 1D array
'{1, 2, 3}'                    -- Lower bound defaults to 1
'[2:4]={2, 3, 4}'             -- Explicit lower bound

-- 2D array
'{{1, 2}, {3, 4}}'            -- 2x2 array
'[1:2][1:2]={{1,2},{3,4}}'    -- With explicit bounds

-- Arrays with NULLs
'{1, NULL, 3}'                -- NULL element

-- String arrays (quoting required for special chars)
'{"abc", "def", "ghi"}'
'{"hello world", "with spaces"}'

-- Empty array
'{}'
```

### Type Declaration

```sql
-- Column declarations
CREATE TABLE example (
    numbers INTEGER[],              -- 1D array, any size
    matrix INTEGER[][],             -- 2D array
    fixed INTEGER[3],               -- Size hint (not enforced!)
    multi INTEGER[2][3]             -- Dimension hint
);

-- Value insertion
INSERT INTO example VALUES (
    ARRAY[1, 2, 3],                -- ARRAY constructor
    ARRAY[[1,2],[3,4]],            -- 2D array
    '{5, 6, 7}',                   -- Text literal
    '{{1,2,3},{4,5,6}}'           -- 2D text literal
);
```

### Array Operations

```sql
-- Element access (1-indexed)
SELECT numbers[1] FROM example;    -- First element
SELECT matrix[1][2] FROM example;  -- Row 1, Column 2

-- Slicing
SELECT numbers[1:3] FROM example;  -- Elements 1-3
SELECT matrix[1:2][1:1] FROM example;  -- Subarray

-- Bounds functions
SELECT array_lower(numbers, 1);    -- Lower bound of dimension 1
SELECT array_upper(numbers, 1);    -- Upper bound of dimension 1
SELECT array_length(numbers, 1);   -- Length of dimension 1

-- Array operators
SELECT numbers || 4;               -- Append element
SELECT numbers || ARRAY[4,5];      -- Concatenate arrays
SELECT 0 || numbers;               -- Prepend element

-- ANY/ALL predicates
SELECT * FROM example WHERE 2 = ANY(numbers);    -- Contains 2
SELECT * FROM example WHERE 10 > ALL(numbers);   -- All < 10
```

---

## Binary Storage Format

### PostgreSQL ArrayType Structure

From PostgreSQL source (`src/include/utils/array.h`):

```c
typedef struct ArrayType {
    int32  vl_len_;      // Varlena header (total size including header)
    int    ndim;         // Number of dimensions
    int32  dataoffset;   // Offset to data, or 0 if no nulls bitmap
    Oid    elemtype;     // Element type OID
    // Followed by:
    // int    dimensions[ndim];     // Length of each dimension
    // int    lowerBounds[ndim];    // Lower bound of each dimension
    // bits8  nullBitmap[];         // NULL bitmap (if dataoffset != 0)
    // char   data[];               // Actual element data
} ArrayType;
```

### Layout Diagram

```
┌─────────────────────────────────────────────────────────────┐
│ vl_len_ (4 bytes) - Total size including header             │
├─────────────────────────────────────────────────────────────┤
│ ndim (4 bytes) - Number of dimensions (e.g., 1, 2, 3...)   │
├─────────────────────────────────────────────────────────────┤
│ dataoffset (4 bytes) - Offset to data (0 if no NULLs)      │
├─────────────────────────────────────────────────────────────┤
│ elemtype (4 bytes) - Element type OID                       │
├─────────────────────────────────────────────────────────────┤
│ dimensions[0] (4 bytes) - Size of dimension 1               │
│ dimensions[1] (4 bytes) - Size of dimension 2               │
│ ... (ndim × 4 bytes total)                                  │
├─────────────────────────────────────────────────────────────┤
│ lowerBounds[0] (4 bytes) - Lower bound of dim 1            │
│ lowerBounds[1] (4 bytes) - Lower bound of dim 2            │
│ ... (ndim × 4 bytes total)                                  │
├─────────────────────────────────────────────────────────────┤
│ NULL bitmap (variable, present only if dataoffset != 0)    │
│ - 1 bit per array element                                   │
│ - Rounded up to byte boundary                               │
│ - 1 = NULL, 0 = not NULL                                    │
├─────────────────────────────────────────────────────────────┤
│ Element data (variable size)                                │
│ - Elements stored in row-major order                        │
│ - Fixed-size elements: packed consecutively                 │
│ - Variable-size elements: 4-byte length + data              │
└─────────────────────────────────────────────────────────────┘
```

### Example: INT32 Array [1, 2, 3]

```
Header:
  vl_len_:    52 (total size)
  ndim:       1  (1-dimensional)
  dataoffset: 0  (no NULLs)
  elemtype:   23 (INT4OID)

Dimensions:
  dimensions[0]: 3  (3 elements)

Lower Bounds:
  lowerBounds[0]: 1  (1-indexed)

Data:
  00 00 00 01  (element 0: value 1)
  00 00 00 02  (element 1: value 2)
  00 00 00 03  (element 2: value 3)
```

### Example: 2D Array {{1, 2}, {3, 4}}

```
Header:
  vl_len_:    76
  ndim:       2  (2-dimensional)
  dataoffset: 0  (no NULLs)
  elemtype:   23 (INT4OID)

Dimensions:
  dimensions[0]: 2  (2 rows)
  dimensions[1]: 2  (2 columns)

Lower Bounds:
  lowerBounds[0]: 1  (row index starts at 1)
  lowerBounds[1]: 1  (column index starts at 1)

Data (row-major order):
  00 00 00 01  (row 1, col 1: value 1)
  00 00 00 02  (row 1, col 2: value 2)
  00 00 00 03  (row 2, col 1: value 3)
  00 00 00 04  (row 2, col 2: value 4)
```

### Example: Array with NULLs [1, NULL, 3]

```
Header:
  vl_len_:    60
  ndim:       1
  dataoffset: 28  (offset to data = 16 + 4 + 4 + 4 bytes aligned)
  elemtype:   23

Dimensions:
  dimensions[0]: 3

Lower Bounds:
  lowerBounds[0]: 1

NULL Bitmap (3 bits, rounded to 1 byte):
  Bit 0: 0 (not NULL)
  Bit 1: 1 (NULL)
  Bit 2: 0 (not NULL)
  Bits 3-7: unused (padding)

  Binary: 00000010 = 0x02

Data:
  00 00 00 01  (element 0: value 1)
  (element 1 omitted - marked NULL in bitmap)
  00 00 00 03  (element 2: value 3)
```

### Variable-Length Elements

For strings, each element has its own length prefix:

```
String array ["hello", "world"]:

Data section:
  05 00 00 00  (length: 5)
  68 65 6C 6C 6F  (UTF-8: "hello")

  05 00 00 00  (length: 5)
  77 6F 72 6C 64  (UTF-8: "world")
```

---

## C++ Structure Specifications

### Existing ArrayValue Class

Located in `include/scratchbird/core/array.h`:

```cpp
namespace scratchbird::core {

enum class ArrayElementType : uint8_t {
    INT32 = 0,
    INT64 = 1,
    FLOAT32 = 2,
    FLOAT64 = 3,
    STRING = 4,
    BOOL = 5
};

class ArrayValue {
public:
    using Element = std::variant<int32_t, int64_t, float, double,
                                 std::string, bool>;

    // Constructors
    ArrayValue(ArrayElementType type, const std::vector<size_t>& dimensions);
    ArrayValue(const std::vector<int32_t>& values,
               const std::vector<size_t>& dimensions);
    // ... other type-specific constructors

    // Type queries
    auto getElementType() const -> ArrayElementType;
    auto getDimensions() const -> const std::vector<size_t>&;
    auto getRank() const -> size_t;  // Number of dimensions
    auto getTotalElements() const -> size_t;
    bool isEmpty() const;

    // Element access
    auto getElement(size_t flat_index) const -> std::optional<Element>;
    auto setElement(size_t flat_index, const Element& value) -> bool;
    auto at(const std::vector<size_t>& indices) const -> std::optional<Element>;
    auto set(const std::vector<size_t>& indices, const Element& value) -> bool;

    // Operations
    auto slice(const std::vector<std::pair<size_t, size_t>>& ranges) const
        -> std::optional<ArrayValue>;
    auto reshape(const std::vector<size_t>& new_dimensions) const
        -> std::optional<ArrayValue>;
    auto flatten() const -> ArrayValue;
    auto transpose() const -> std::optional<ArrayValue>;  // 2D only

    // Type-specific getters
    auto getInt32Vector() const -> std::optional<std::vector<int32_t>>;
    auto getInt64Vector() const -> std::optional<std::vector<int64_t>>;
    // ... other type getters

    std::string toString() const;

private:
    ArrayElementType element_type_;
    std::vector<size_t> dimensions_;

    // Storage (only one populated based on element_type_)
    std::vector<int32_t> int32_data_;
    std::vector<int64_t> int64_data_;
    std::vector<float> float32_data_;
    std::vector<double> float64_data_;
    std::vector<std::string> string_data_;
    std::vector<bool> bool_data_;
};

} // namespace scratchbird::core
```

### Existing Array Utility Class

```cpp
class Array {
public:
    // Parse from string
    static auto parse(const std::string& str, ArrayElementType type)
        -> std::optional<ArrayValue>;

    // Binary encoding/decoding (ALREADY IMPLEMENTED!)
    static auto encode(const ArrayValue& value) -> std::vector<uint8_t>;
    static auto decode(const std::vector<uint8_t>& binary)
        -> std::optional<ArrayValue>;

    // Validation
    static bool validate(const std::string& str);

    // Factory methods
    static auto fromInt32(const std::vector<int32_t>& values,
                         const std::vector<size_t>& dimensions)
        -> std::optional<ArrayValue>;
    // ... other factory methods

    // Array operations
    static auto concatenate(const ArrayValue& a, const ArrayValue& b,
                           size_t axis) -> std::optional<ArrayValue>;
    // ... other operations
};
```

### What Needs to be Added

**1. Lower Bounds Support**

PostgreSQL arrays have configurable lower bounds (not always 1). Need to extend ArrayValue:

```cpp
class ArrayValue {
    // Add:
    std::vector<int32_t> lower_bounds_;  // One per dimension

public:
    // Add methods:
    auto getLowerBound(size_t dimension) const -> int32_t;
    auto getUpperBound(size_t dimension) const -> int32_t;
    void setLowerBounds(const std::vector<int32_t>& bounds);
};
```

**2. NULL Element Support**

Current ArrayValue doesn't handle NULL elements:

```cpp
class ArrayValue {
    // Add:
    std::vector<bool> null_bitmap_;  // One bit per element

public:
    // Add methods:
    bool isNull(size_t flat_index) const;
    bool isNull(const std::vector<size_t>& indices) const;
    void setNull(size_t flat_index);
    void setNull(const std::vector<size_t>& indices);
    bool hasNulls() const;
};
```

**3. Extended Element Types**

Support all PostgreSQL element types:

```cpp
enum class ArrayElementType : uint8_t {
    // Existing
    INT32 = 0,
    INT64 = 1,
    FLOAT32 = 2,
    FLOAT64 = 3,
    STRING = 4,  // TEXT/VARCHAR
    BOOL = 5,

    // Add these for full compatibility
    INT16 = 6,
    INT8 = 7,
    UINT32 = 8,
    UINT64 = 9,
    UINT16 = 10,
    UINT8 = 11,
    DECIMAL = 12,  // As string
    DATE = 13,
    TIME = 14,
    TIMESTAMP = 15,
    INTERVAL = 16,
    UUID = 17,
    // ... more as needed
};
```

---

## TypedValue Integration

### Add to Variant

In `include/scratchbird/core/types.h`, add to `VariantType` (~line 418):

```cpp
using VariantType =
    std::variant<std::monostate,
                 // ... existing types ...
                 std::shared_ptr<ArrayValue>,  // ADD THIS
                 // ... rest of types ...
                 >;
```

### Factory Methods

Add to TypedValue class:

```cpp
// Array factory methods
static TypedValue makeArray(const ArrayValue& v);
static TypedValue makeArray(std::shared_ptr<ArrayValue> v);

// Convenience factories for common cases
static TypedValue makeArray(const std::vector<int32_t>& values);
static TypedValue makeArray(const std::vector<int32_t>& values,
                           const std::vector<size_t>& dimensions);
static TypedValue makeArray(const std::vector<int64_t>& values,
                           const std::vector<size_t>& dimensions);
static TypedValue makeArray(const std::vector<double>& values,
                           const std::vector<size_t>& dimensions);
static TypedValue makeArray(const std::vector<std::string>& values,
                           const std::vector<size_t>& dimensions);
```

### Getter Methods

```cpp
std::shared_ptr<ArrayValue> getArray() const;

// Convenience accessors
size_t getArrayRank() const;  // Number of dimensions
const std::vector<size_t>& getArrayDimensions() const;
ArrayElementType getArrayElementType() const;
```

### Array Element Access Methods

```cpp
// SQL-style array subscripting
TypedValue getArrayElement(size_t index) const;  // 1D
TypedValue getArrayElement(size_t row, size_t col) const;  // 2D
TypedValue getArrayElement(const std::vector<size_t>& indices) const;  // nD

// Slice access
TypedValue getArraySlice(size_t start, size_t end) const;  // 1D
TypedValue getArraySlice(const std::vector<std::pair<size_t, size_t>>& ranges) const;  // nD
```

---

## Serialization Implementation

### TypeSerializer::serialize() Addition

Add case to switch statement in `src/core/type_serialization.cpp`:

```cpp
case DataType::ARRAY:
{
    auto array_ptr = value.getArray();
    if (!array_ptr) {
        break;  // Return empty for NULL
    }

    // Use existing Array::encode() method
    result = Array::encode(*array_ptr);
    break;
}
```

### TypeSerializer::deserialize() Addition

```cpp
case DataType::ARRAY:
{
    if (size == 0) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty array data");
        return std::nullopt;
    }

    // Use existing Array::decode() method
    std::vector<uint8_t> binary_data(data, data + size);
    auto array_opt = Array::decode(binary_data);

    if (!array_opt) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Failed to decode array data");
        return std::nullopt;
    }

    return TypedValue::makeArray(*array_opt);
}
```

### TypeSerializer::getSerializedSize() Addition

```cpp
case DataType::ARRAY:
{
    auto array_ptr = value.getArray();
    if (!array_ptr) return 0;

    // Must actually encode to get size (variable-length)
    auto encoded = Array::encode(*array_ptr);
    return static_cast<uint32_t>(encoded.size());
}
```

### Array::encode() Format (Already Implemented)

The existing implementation should follow PostgreSQL format:

```cpp
std::vector<uint8_t> Array::encode(const ArrayValue& value) {
    std::vector<uint8_t> result;

    // Header
    int32_t ndim = value.getRank();
    int32_t dataoffset = value.hasNulls() ? calculateDataOffset(ndim) : 0;
    uint32_t elemtype = mapToOID(value.getElementType());

    // Write header
    writeInt32(result, 0);  // vl_len_ (calculate at end)
    writeInt32(result, ndim);
    writeInt32(result, dataoffset);
    writeUInt32(result, elemtype);

    // Write dimensions
    const auto& dims = value.getDimensions();
    for (size_t dim : dims) {
        writeInt32(result, static_cast<int32_t>(dim));
    }

    // Write lower bounds
    for (size_t i = 0; i < ndim; ++i) {
        writeInt32(result, value.getLowerBound(i));
    }

    // Write NULL bitmap (if any)
    if (value.hasNulls()) {
        writeNullBitmap(result, value);
    }

    // Write element data
    writeElements(result, value);

    // Update vl_len_
    int32_t total_size = static_cast<int32_t>(result.size());
    std::memcpy(result.data(), &total_size, 4);

    return result;
}
```

---

## SQL Syntax and Operations

### Array Literals

```sql
-- ARRAY constructor
SELECT ARRAY[1, 2, 3];
SELECT ARRAY[[1,2],[3,4]];

-- Text literal (PostgreSQL-compatible)
SELECT '{1, 2, 3}'::INTEGER[];
SELECT '{{1,2},{3,4}}'::INTEGER[][];

-- With explicit bounds
SELECT '[2:4]={2, 3, 4}'::INTEGER[];
```

### Subscript Operators

```sql
-- Element access (1-indexed in PostgreSQL)
SELECT arr[1];           -- First element
SELECT arr[2:4];         -- Slice (elements 2-4)
SELECT matrix[1][2];     -- 2D access

-- Update elements
UPDATE table SET arr[1] = 10 WHERE id = 1;
```

### Array Functions

Must implement these core functions:

**Constructors:**
```sql
ARRAY[val1, val2, ...]          -- Array constructor
ARRAY(subquery)                  -- Array from query result
```

**Information functions:**
```sql
array_length(arr, dimension)     -- Length of dimension
array_lower(arr, dimension)      -- Lower bound of dimension
array_upper(arr, dimension)      -- Upper bound of dimension
array_dims(arr)                  -- Text representation of dimensions
cardinality(arr)                 -- Total number of elements
```

**Search functions:**
```sql
array_position(arr, value)       -- 1-based position or NULL if not found
array_slice(arr, lower, upper)   -- Convenience wrapper for arr[lower:upper]
```

**Operators:**
```sql
arr[n]                          -- Element access
arr[n:m]                        -- Slice
arr1 || arr2                    -- Concatenate
arr || elem                     -- Append
elem || arr                     -- Prepend
arr @> arr2                     -- Contains (arr contains all of arr2)
arr <@ arr2                     -- Contained by
arr && arr2                     -- Overlaps
```

**Comparison:**
```sql
arr1 = arr2                     -- Equality
arr1 != arr2                    -- Inequality
arr1 < arr2                     -- Lexicographic comparison
```

**Predicates:**
```sql
val = ANY(arr)                  -- Element in array
val = ALL(arr)                  -- Compare with all elements
```

**Modification:**
```sql
array_append(arr, elem)         -- Append element
array_prepend(elem, arr)        -- Prepend element
array_cat(arr1, arr2)           -- Concatenate
array_remove(arr, elem)         -- Remove all occurrences
array_replace(arr, from, to)    -- Replace elements
```

**Aggregation:**
```sql
array_agg(expr)                 -- Aggregate into array
unnest(arr)                     -- Expand to rows
```

---

## Validation and Constraints

### Dimension Validation

```cpp
bool validateDimensions(const ArrayValue& arr) {
    const auto& dims = arr.getDimensions();

    // PostgreSQL supports up to 6 dimensions typically
    if (dims.size() > MAXDIM) {  // MAXDIM = 6 in PostgreSQL
        return false;
    }

    // No dimension can be zero (except for empty arrays)
    for (size_t dim : dims) {
        if (dim == 0 && !dims.empty()) {
            return false;
        }
    }

    // Lower bounds must be reasonable
    for (size_t i = 0; i < dims.size(); ++i) {
        int32_t lower = arr.getLowerBound(i);
        int32_t upper = lower + dims[i] - 1;

        // Check for overflow
        if (upper < lower) {
            return false;
        }
    }

    return true;
}
```

### Element Type Validation

```cpp
bool validateElementType(ArrayElementType type) {
    // All element types must be serializable
    switch (type) {
        case ArrayElementType::INT32:
        case ArrayElementType::INT64:
        case ArrayElementType::FLOAT32:
        case ArrayElementType::FLOAT64:
        case ArrayElementType::STRING:
        case ArrayElementType::BOOL:
            return true;
        default:
            return false;
    }
}
```

### Size Limits

```cpp
// PostgreSQL limits
constexpr size_t MAX_ARRAY_DIMENSIONS = 6;
constexpr size_t MAX_ARRAY_ELEMENTS = 1000000000;  // 1 billion

bool validateArraySize(const ArrayValue& arr) {
    return arr.getRank() <= MAX_ARRAY_DIMENSIONS &&
           arr.getTotalElements() <= MAX_ARRAY_ELEMENTS;
}
```

---

## Implementation Checklist

### Phase 1: TypedValue Integration (2-3 hours)

- [ ] Add `std::shared_ptr<ArrayValue>` to TypedValue::VariantType
- [ ] Implement makeArray() factory methods
- [ ] Implement getArray() getter method
- [ ] Implement comparison operators
- [ ] Update types.cpp with method implementations

### Phase 2: Serialization (2-3 hours)

- [ ] Add ARRAY case to TypeSerializer::serialize()
- [ ] Add ARRAY case to TypeSerializer::deserialize()
- [ ] Add ARRAY case to TypeSerializer::getSerializedSize()
- [ ] Verify Array::encode() follows PostgreSQL format
- [ ] Verify Array::decode() handles all cases

### Phase 3: Enhanced ArrayValue (4-6 hours)

- [ ] Add lower_bounds_ member
- [ ] Add null_bitmap_ member
- [ ] Implement getLowerBound/getUpperBound methods
- [ ] Implement isNull/setNull methods
- [ ] Update encode/decode to handle lower bounds
- [ ] Update encode/decode to handle NULL bitmap

### Phase 4: Testing (3-4 hours)

- [ ] Test 1D arrays (basic)
- [ ] Test 2D arrays (matrices)
- [ ] Test 3D+ arrays
- [ ] Test arrays with NULLs
- [ ] Test arrays with custom lower bounds
- [ ] Test empty arrays
- [ ] Test serialization round-trip
- [ ] Test size calculation
- [ ] Test PostgreSQL compatibility

### Phase 5: SQL Support (FUTURE - 20+ hours)

- [ ] Array literal parsing
- [ ] Subscript operator syntax
- [ ] array_length, array_dims functions
- [ ] Array constructors (ARRAY[...])
- [ ] Array operators (||, &&, @>, <@)
- [ ] ANY/ALL predicates

---

## PostgreSQL Compatibility Notes

### OID Mapping

PostgreSQL uses Object IDs (OIDs) for types:

```cpp
uint32_t mapToOID(ArrayElementType type) {
    switch (type) {
        case ArrayElementType::INT32:   return 23;   // INT4OID
        case ArrayElementType::INT64:   return 20;   // INT8OID
        case ArrayElementType::FLOAT32: return 700;  // FLOAT4OID
        case ArrayElementType::FLOAT64: return 701;  // FLOAT8OID
        case ArrayElementType::STRING:  return 25;   // TEXTOID
        case ArrayElementType::BOOL:    return 16;   // BOOLOID
        default: return 0;  // INVALID
    }
}
```

### Index Base

- **PostgreSQL default:** 1-indexed (lower bound = 1)
- **ScratchBird internal:** 0-indexed for C++ access
- **Conversion needed** when interfacing with SQL

### Array Equality

PostgreSQL array equality is element-wise AND dimension-aware:
- `{1,2,3}` ≠ `{{1,2,3}}`  (different dimensions)
- `{1,2,3}` ≠ `{1,2,3,NULL}`  (different lengths)
- `[2:4]={2,3,4}` = `[1:3]={2,3,4}`  (bounds don't affect equality)

---

## References

- **PostgreSQL Documentation - Arrays:** https://www.postgresql.org/docs/current/arrays.html
- **PostgreSQL Source - array.h:** https://github.com/postgres/postgres/blob/master/src/include/utils/array.h
- **PostgreSQL Source - arrayfuncs.c:** https://github.com/postgres/postgres/blob/master/src/backend/utils/adt/arrayfuncs.c
- **SQL Standard - Array Types:** ISO/IEC 9075-2:2016
- **libpq Binary Format:** https://www.postgresql.org/docs/current/libpq-exec.html

---

**Document Version:** 1.0
**Last Updated:** November 18, 2025
**Status:** Ready for Implementation
**Estimated Effort:** 12-15 hours for full implementation
