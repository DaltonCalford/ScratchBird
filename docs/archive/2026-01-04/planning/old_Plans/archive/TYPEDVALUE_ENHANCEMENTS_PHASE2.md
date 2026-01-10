# TypedValue Enhancements for Phase 2
# Advanced Type Support: COMPOSITE, VECTOR, and VARIANT

**Status:** Planning (Phase 2 Work)
**Priority:** Medium (Enables domain manager advanced features)
**Estimated Effort:** 40-50 hours
**Dependencies:** Domain Manager Phase 2-5 operations
**Target:** Alpha 1.5 or Alpha 2

---

## Overview

This document specifies the enhancements required for `TypedValue` to support advanced data types:
- **COMPOSITE** (RECORD types with named fields)
- **VECTOR** (Arrays/SETs with element access)
- **VARIANT** (Runtime polymorphic types)

These enhancements unblock 9 currently-stubbed operations in `DomainManager`:
- 1 RECORD operation: `extractField()`
- 5 SET operations: `setContains()`, `setsOverlap()`, `setUnion()`, `setIntersection()`, `setDifference()`
- 3 VARIANT operations: `extractDataType()`, `isOfType()`, `variantCast()`

---

## Current TypedValue Architecture

### Existing Type Support

```cpp
enum class DataType : uint8_t {
    UNKNOWN = 0,
    // Numeric types
    TINYINT, SMALLINT, INT32, INT64,
    FLOAT, DOUBLE, DECIMAL,
    // String types
    CHAR, VARCHAR, TEXT,
    // Binary types
    BINARY, VARBINARY, BLOB,
    // Temporal types
    DATE, TIME, TIMESTAMP, INTERVAL,
    // Boolean
    BOOLEAN,
    // JSON
    JSON, JSONB,
    // Spatial
    POINT, LINESTRING, POLYGON, GEOMETRY,
    // TODO: Add COMPOSITE, VECTOR, VARIANT
};
```

### Current Storage Model

```cpp
class TypedValue {
private:
    DataType type_;
    union {
        int32_t int32_val;
        int64_t int64_val;
        double double_val;
        bool bool_val;
        // ... other primitive types
    } data_;
    std::string str_data_;  // For string types
    std::vector<uint8_t> blob_data_;  // For binary types
};
```

**Limitations:**
- No support for structured types (COMPOSITE)
- No element access for arrays (VECTOR)
- No runtime type tags (VARIANT)

---

## Phase 2.1: COMPOSITE Type Support

### Goal

Enable RECORD/ROW types with named fields for structured data.

### Use Cases

```sql
-- Create RECORD domain
CREATE DOMAIN employee_record AS (
    name TEXT,
    age INTEGER,
    salary DECIMAL(10,2),
    hire_date DATE
);

-- Use in table
CREATE TABLE employees (
    id INTEGER PRIMARY KEY,
    data employee_record
);

-- Field access
SELECT (data).name, (data).salary FROM employees;
```

### Type Definition

```cpp
// Add to DataType enum
enum class DataType : uint8_t {
    // ... existing types ...
    COMPOSITE = 50,  // RECORD/ROW type
};
```

### Storage Structure

```cpp
/**
 * CompositeValue - Structured type with named fields
 */
struct CompositeValue {
    struct Field {
        std::string name;
        TypedValue value;
    };

    std::vector<Field> fields;

    // Field access
    std::optional<TypedValue> getField(const std::string& field_name) const {
        for (const auto& f : fields) {
            if (f.name == field_name) {
                return f.value;
            }
        }
        return std::nullopt;
    }

    // Set field value
    bool setField(const std::string& field_name, const TypedValue& value) {
        for (auto& f : fields) {
            if (f.name == field_name) {
                f.value = value;
                return true;
            }
        }
        return false;
    }
};
```

### TypedValue Integration

```cpp
class TypedValue {
private:
    DataType type_;
    union {
        // ... existing primitive types ...
    } data_;

    // Add composite storage
    std::unique_ptr<CompositeValue> composite_data_;

public:
    // Factory method
    static TypedValue createComposite(const std::vector<std::pair<std::string, TypedValue>>& fields) {
        TypedValue tv;
        tv.type_ = DataType::COMPOSITE;
        tv.composite_data_ = std::make_unique<CompositeValue>();
        for (const auto& [name, value] : fields) {
            tv.composite_data_->fields.push_back({name, value});
        }
        return tv;
    }

    // Field access
    std::optional<TypedValue> getCompositeField(const std::string& field_name) const {
        if (type_ != DataType::COMPOSITE || !composite_data_) {
            return std::nullopt;
        }
        return composite_data_->getField(field_name);
    }

    // Get all fields
    const std::vector<CompositeValue::Field>& getCompositeFields() const {
        static const std::vector<CompositeValue::Field> empty;
        if (type_ != DataType::COMPOSITE || !composite_data_) {
            return empty;
        }
        return composite_data_->fields;
    }
};
```

### Binary Serialization Format

**COMPOSITE on-disk format:**

```
+------------------+
| Type Tag (1B)    | = DataType::COMPOSITE
+------------------+
| Field Count (2B) | Number of fields (max 65535)
+------------------+
| Field 1 Name Len | 1 byte (max 255 chars)
| Field 1 Name     | UTF-8 string
| Field 1 Type     | 1 byte DataType
| Field 1 Value    | Variable length (recursive TypedValue)
+------------------+
| Field 2 ...      |
+------------------+
| ...              |
+------------------+
```

**Size calculation:**
- Header: 3 bytes (type + count)
- Per field: 2 bytes + name_len + value_size
- Example: `employee(name TEXT, age INT32)` with name="Alice", age=30
  - Total: 3 + (1+4+1+5) + (1+3+1+4) = 23 bytes

### Implementation Tasks

1. **Data structure** (4 hours)
   - Add `CompositeValue` struct
   - Integrate into `TypedValue` with `std::unique_ptr`
   - Copy/move constructors

2. **Field access API** (2 hours)
   - `createComposite()`
   - `getCompositeField()`
   - `setCompositeField()`
   - `getCompositeFields()`

3. **Serialization** (3 hours)
   - Binary encoding/decoding
   - Recursion handling for nested COMPOSITE
   - Endianness considerations

4. **Testing** (3 hours)
   - Unit tests for field access (10+ tests)
   - Serialization round-trip tests
   - Nested COMPOSITE tests

**Total: 12 hours**

---

## Phase 2.2: VECTOR Type Support

### Goal

Enable array/set types with element iteration and access.

### Use Cases

```sql
-- Create SET domain
CREATE DOMAIN tag_set AS SET OF TEXT;

-- Use in table
CREATE TABLE documents (
    id INTEGER PRIMARY KEY,
    tags tag_set
);

-- Set operations
SELECT tags @> ARRAY['database'] FROM documents;  -- Contains
SELECT tags1 && tags2 FROM docs;                   -- Overlap
```

### Type Definition

```cpp
// VECTOR already exists as DataType, but needs element access support
enum class DataType : uint8_t {
    // ... existing types ...
    VECTOR = 40,  // Already exists, enhance storage
};
```

### Storage Structure

```cpp
/**
 * VectorValue - Array/SET type with element access
 */
struct VectorValue {
    DataType element_type;  // Homogeneous element type
    std::vector<TypedValue> elements;

    // Element access
    std::optional<TypedValue> at(size_t index) const {
        if (index >= elements.size()) return std::nullopt;
        return elements[index];
    }

    // Size
    size_t size() const { return elements.size(); }

    // Iterator support
    auto begin() { return elements.begin(); }
    auto end() { return elements.end(); }
    auto begin() const { return elements.begin(); }
    auto end() const { return elements.end(); }

    // Membership testing
    bool contains(const TypedValue& element) const {
        for (const auto& e : elements) {
            if (e == element) return true;
        }
        return false;
    }
};
```

### TypedValue Integration

```cpp
class TypedValue {
private:
    // Add vector storage
    std::unique_ptr<VectorValue> vector_data_;

public:
    // Factory method
    static TypedValue createVector(DataType element_type,
                                     const std::vector<TypedValue>& elements) {
        TypedValue tv;
        tv.type_ = DataType::VECTOR;
        tv.vector_data_ = std::make_unique<VectorValue>();
        tv.vector_data_->element_type = element_type;
        tv.vector_data_->elements = elements;
        return tv;
    }

    // Convenience: infer element type from first element
    static TypedValue createVector(const std::vector<TypedValue>& elements) {
        if (elements.empty()) {
            throw std::invalid_argument("Cannot infer element type from empty vector");
        }
        return createVector(elements[0].type(), elements);
    }

    // Element access
    std::optional<TypedValue> getVectorElement(size_t index) const {
        if (type_ != DataType::VECTOR || !vector_data_) {
            return std::nullopt;
        }
        return vector_data_->at(index);
    }

    // Get all elements
    const std::vector<TypedValue>& getVectorElements() const {
        static const std::vector<TypedValue> empty;
        if (type_ != DataType::VECTOR || !vector_data_) {
            return empty;
        }
        return vector_data_->elements;
    }

    // Size
    size_t getVectorSize() const {
        if (type_ != DataType::VECTOR || !vector_data_) {
            return 0;
        }
        return vector_data_->size();
    }
};
```

### Binary Serialization Format

**VECTOR on-disk format:**

```
+-------------------+
| Type Tag (1B)     | = DataType::VECTOR
+-------------------+
| Element Type (1B) | DataType of elements
+-------------------+
| Element Count (4B)| Number of elements (max 2^32-1)
+-------------------+
| Element 1         | Variable length (recursive TypedValue)
+-------------------+
| Element 2         |
+-------------------+
| ...               |
+-------------------+
```

**Size calculation:**
- Header: 6 bytes (type + elem_type + count)
- Per element: variable (depends on element type)
- Example: `VECTOR of INT32` with [1, 2, 3]
  - Total: 6 + (4*3) = 18 bytes

### Set Operations Implementation

```cpp
// Set contains (@> operator)
bool setContains(const TypedValue& set_value, const TypedValue& element) {
    if (set_value.type() != DataType::VECTOR) return false;
    const auto& elements = set_value.getVectorElements();
    return std::find(elements.begin(), elements.end(), element) != elements.end();
}

// Set overlap (&& operator)
bool setsOverlap(const TypedValue& set1, const TypedValue& set2) {
    const auto& elems1 = set1.getVectorElements();
    const auto& elems2 = set2.getVectorElements();

    // Optimize: build hash set from smaller set
    if (elems1.size() > elems2.size()) {
        return setsOverlap(set2, set1);
    }

    std::unordered_set<TypedValue> hash_set(elems1.begin(), elems1.end());
    for (const auto& e : elems2) {
        if (hash_set.count(e) > 0) return true;
    }
    return false;
}

// Set union
TypedValue setUnion(const TypedValue& set1, const TypedValue& set2) {
    std::unordered_set<TypedValue> unique_elements;

    // Add all elements from set1
    for (const auto& e : set1.getVectorElements()) {
        unique_elements.insert(e);
    }

    // Add all elements from set2
    for (const auto& e : set2.getVectorElements()) {
        unique_elements.insert(e);
    }

    // Convert back to vector
    std::vector<TypedValue> result(unique_elements.begin(), unique_elements.end());
    return TypedValue::createVector(result);
}

// Set intersection
TypedValue setIntersection(const TypedValue& set1, const TypedValue& set2) {
    const auto& elems1 = set1.getVectorElements();
    const auto& elems2 = set2.getVectorElements();

    // Build hash set from smaller set
    std::unordered_set<TypedValue> hash_set;
    const auto& smaller = (elems1.size() < elems2.size()) ? elems1 : elems2;
    const auto& larger = (elems1.size() < elems2.size()) ? elems2 : elems1;

    for (const auto& e : smaller) {
        hash_set.insert(e);
    }

    // Find common elements
    std::vector<TypedValue> result;
    for (const auto& e : larger) {
        if (hash_set.count(e) > 0) {
            result.push_back(e);
            hash_set.erase(e);  // Prevent duplicates
        }
    }

    return TypedValue::createVector(result);
}

// Set difference (A - B)
TypedValue setDifference(const TypedValue& set1, const TypedValue& set2) {
    const auto& elems1 = set1.getVectorElements();
    const auto& elems2 = set2.getVectorElements();

    // Build hash set from set2
    std::unordered_set<TypedValue> exclude_set(elems2.begin(), elems2.end());

    // Collect elements from set1 not in set2
    std::vector<TypedValue> result;
    for (const auto& e : elems1) {
        if (exclude_set.count(e) == 0) {
            result.push_back(e);
        }
    }

    return TypedValue::createVector(result);
}
```

### Implementation Tasks

1. **Data structure** (3 hours)
   - Enhance `VectorValue` struct
   - Iterator support
   - Element access methods

2. **TypedValue integration** (2 hours)
   - `createVector()`
   - `getVectorElement()`
   - `getVectorElements()`
   - `getVectorSize()`

3. **Set operations** (6 hours)
   - `setContains()` (1 hour)
   - `setsOverlap()` (1 hour)
   - `setUnion()` (1.5 hours)
   - `setIntersection()` (1.5 hours)
   - `setDifference()` (1 hour)

4. **Serialization** (3 hours)
   - Binary encoding/decoding
   - Large array optimization

5. **Testing** (6 hours)
   - Element access tests (5 tests)
   - Set operation tests (15 tests)
   - Performance tests (large arrays)
   - Serialization tests (5 tests)

**Total: 20 hours**

---

## Phase 2.3: VARIANT Type Support

### Goal

Enable runtime polymorphic types with type-safe access.

### Use Cases

```sql
-- Create VARIANT domain
CREATE DOMAIN flexible_value AS VARIANT(INTEGER, TEXT, DECIMAL);

-- Use in table
CREATE TABLE dynamic_data (
    id INTEGER PRIMARY KEY,
    value flexible_value
);

-- Type checking and casting
SELECT EXTRACT(DATATYPE FROM value),
       CASE WHEN value IS OF (TYPE INTEGER) THEN CAST(value AS INTEGER)
            ELSE 0 END
FROM dynamic_data;
```

### Type Definition

```cpp
// Add to DataType enum
enum class DataType : uint8_t {
    // ... existing types ...
    VARIANT = 60,  // Runtime polymorphic type
};
```

### Storage Structure

```cpp
/**
 * VariantValue - Runtime polymorphic type
 */
struct VariantValue {
    DataType runtime_type;  // Actual type currently held
    TypedValue value;        // Actual value

    // Type checking
    bool isOfType(DataType expected_type) const {
        return runtime_type == expected_type;
    }

    // Type-safe extraction
    std::optional<TypedValue> castTo(DataType target_type) const {
        if (runtime_type != target_type) {
            return std::nullopt;
        }
        return value;
    }
};
```

### TypedValue Integration

```cpp
class TypedValue {
private:
    // Add variant storage
    std::unique_ptr<VariantValue> variant_data_;

public:
    // Factory method
    static TypedValue createVariant(DataType runtime_type,
                                     const TypedValue& value) {
        TypedValue tv;
        tv.type_ = DataType::VARIANT;
        tv.variant_data_ = std::make_unique<VariantValue>();
        tv.variant_data_->runtime_type = runtime_type;
        tv.variant_data_->value = value;
        return tv;
    }

    // Get runtime type
    DataType getVariantType() const {
        if (type_ != DataType::VARIANT || !variant_data_) {
            return DataType::UNKNOWN;
        }
        return variant_data_->runtime_type;
    }

    // Type checking
    bool variantIsOfType(DataType expected_type) const {
        if (type_ != DataType::VARIANT || !variant_data_) {
            return false;
        }
        return variant_data_->isOfType(expected_type);
    }

    // Type-safe casting
    std::optional<TypedValue> variantCast(DataType target_type) const {
        if (type_ != DataType::VARIANT || !variant_data_) {
            return std::nullopt;
        }
        return variant_data_->castTo(target_type);
    }
};
```

### Binary Serialization Format

**VARIANT on-disk format:**

```
+--------------------+
| Type Tag (1B)      | = DataType::VARIANT
+--------------------+
| Runtime Type (1B)  | Actual type currently held
+--------------------+
| Value              | Variable length (recursive TypedValue)
+--------------------+
```

**Size calculation:**
- Header: 2 bytes (type + runtime_type)
- Value: variable (depends on runtime type)
- Example: `VARIANT` holding INT32(42)
  - Total: 2 + 4 = 6 bytes

### Implementation Tasks

1. **Data structure** (2 hours)
   - Add `VariantValue` struct
   - Type tag storage

2. **TypedValue integration** (3 hours)
   - `createVariant()`
   - `getVariantType()`
   - `variantIsOfType()`
   - `variantCast()`

3. **Serialization** (2 hours)
   - Binary encoding/decoding
   - Type tag validation

4. **Testing** (4 hours)
   - Type extraction tests (5 tests)
   - Type checking tests (5 tests)
   - Casting tests (10 tests)
   - Serialization tests (3 tests)

**Total: 11 hours**

---

## Phase 2.4: Operator Overloading

### Equality Comparison

Required for set operations (hash sets, std::find).

```cpp
class TypedValue {
public:
    // Equality operator
    bool operator==(const TypedValue& other) const {
        if (type_ != other.type_) return false;

        switch (type_) {
            case DataType::INT32:
                return data_.int32_val == other.data_.int32_val;
            case DataType::TEXT:
                return str_data_ == other.str_data_;
            case DataType::COMPOSITE: {
                const auto& f1 = composite_data_->fields;
                const auto& f2 = other.composite_data_->fields;
                if (f1.size() != f2.size()) return false;
                for (size_t i = 0; i < f1.size(); ++i) {
                    if (f1[i].name != f2[i].name) return false;
                    if (f1[i].value != f2[i].value) return false;
                }
                return true;
            }
            case DataType::VECTOR: {
                const auto& e1 = vector_data_->elements;
                const auto& e2 = other.vector_data_->elements;
                return e1 == e2;  // Recursive comparison
            }
            case DataType::VARIANT:
                if (variant_data_->runtime_type != other.variant_data_->runtime_type) {
                    return false;
                }
                return variant_data_->value == other.variant_data_->value;
            // ... other types ...
        }
    }

    bool operator!=(const TypedValue& other) const {
        return !(*this == other);
    }
};
```

### Hash Function

Required for `std::unordered_set<TypedValue>`.

```cpp
namespace std {
    template<>
    struct hash<scratchbird::core::TypedValue> {
        size_t operator()(const scratchbird::core::TypedValue& tv) const {
            size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(tv.type()));

            switch (tv.type()) {
                case DataType::INT32:
                    h ^= std::hash<int32_t>{}(tv.asInt32());
                    break;
                case DataType::TEXT:
                    h ^= std::hash<std::string>{}(tv.asText());
                    break;
                case DataType::COMPOSITE: {
                    for (const auto& f : tv.getCompositeFields()) {
                        h ^= std::hash<std::string>{}(f.name);
                        h ^= std::hash<TypedValue>{}(f.value);
                    }
                    break;
                }
                case DataType::VECTOR: {
                    for (const auto& e : tv.getVectorElements()) {
                        h ^= std::hash<TypedValue>{}(e);
                    }
                    break;
                }
                case DataType::VARIANT:
                    h ^= std::hash<TypedValue>{}(tv.variantCast(tv.getVariantType()).value());
                    break;
                // ... other types ...
            }
            return h;
        }
    };
}
```

**Implementation: 3 hours**

---

## Testing Strategy

### Unit Tests

**COMPOSITE Tests (12 tests):**
1. Create composite with 3 fields
2. Get existing field by name
3. Get non-existent field (returns nullopt)
4. Set field value
5. Nested composite access
6. Serialization round-trip
7. Equality comparison
8. Hash function
9. NULL field values
10. Empty composite
11. Large composite (100 fields)
12. Field name case-sensitivity

**VECTOR Tests (15 tests):**
1. Create vector with 5 elements
2. Get element by index
3. Out-of-bounds access (returns nullopt)
4. Vector size
5. Iterator access
6. Contains operation (element exists)
7. Contains operation (element missing)
8. Overlap operation (true)
9. Overlap operation (false)
10. Union operation
11. Intersection operation
12. Difference operation
13. Serialization round-trip
14. Large vector (10000 elements)
15. Empty vector

**VARIANT Tests (10 tests):**
1. Create variant with INT32
2. Create variant with TEXT
3. Get runtime type
4. Type checking (true)
5. Type checking (false)
6. Cast to correct type (success)
7. Cast to wrong type (fail)
8. Serialization round-trip
9. Nested variant (VARIANT of COMPOSITE)
10. Equality comparison

### Integration Tests

**DomainManager Integration:**
```cpp
TEST(DomainManagerIntegration, RecordFieldExtraction) {
    // Create RECORD domain
    ID domain_id;
    std::vector<RecordField> fields = {
        RecordField("name", DataType::TEXT),
        RecordField("age", DataType::INT32)
    };
    ASSERT_OK(domain_mgr->createRecordDomain(schema_id, "person",
                                              fields, domain_id, &ctx));

    // Create RECORD value
    TypedValue person = TypedValue::createComposite({
        {"name", TypedValue::createText("Alice")},
        {"age", TypedValue::createInt32(30)}
    });

    // Extract field
    TypedValue name_field;
    ASSERT_OK(domain_mgr->extractField(person, "name", name_field, &ctx));
    EXPECT_EQ(name_field.asText(), "Alice");
}

TEST(DomainManagerIntegration, SetOperations) {
    // Create SET domain
    ID domain_id;
    ASSERT_OK(domain_mgr->createSetDomain(schema_id, "tags",
                                           DataType::TEXT, domain_id, &ctx));

    // Create SET values
    TypedValue set1 = TypedValue::createVector({
        TypedValue::createText("a"),
        TypedValue::createText("b")
    });

    TypedValue set2 = TypedValue::createVector({
        TypedValue::createText("b"),
        TypedValue::createText("c")
    });

    // Union
    TypedValue union_result;
    ASSERT_OK(domain_mgr->setUnion(set1, set2, union_result, &ctx));
    EXPECT_EQ(union_result.getVectorSize(), 3);  // {a, b, c}
}
```

---

## Performance Considerations

### Memory Overhead

| Type | Base Size | Overhead per Element | Example (3 elements) |
|------|-----------|----------------------|----------------------|
| COMPOSITE | 8 bytes (unique_ptr) | 32 bytes + name_len + value_size | ~150 bytes |
| VECTOR | 8 bytes (unique_ptr) + 24 bytes (vector) | value_size | ~44 bytes + 3*value |
| VARIANT | 8 bytes (unique_ptr) | 1 byte (type tag) + value_size | ~13 bytes + value |

**Optimization opportunities:**
1. **Small Buffer Optimization (SBO):** Store small COMPOSITE/VECTOR inline (no heap allocation)
2. **Copy-on-Write (COW):** Share data between copies until mutation
3. **Arena allocation:** Batch allocate COMPOSITE fields

### Time Complexity

| Operation | COMPOSITE | VECTOR | VARIANT |
|-----------|-----------|--------|---------|
| Create | O(n) fields | O(n) elements | O(1) |
| Field/element access | O(n) linear search | O(1) index | O(1) |
| Equality | O(n) | O(n) | O(1) + recursive |
| Hash | O(n) | O(n) | O(1) + recursive |
| Serialization | O(n) | O(n) | O(1) + recursive |

**Optimization opportunities:**
1. **COMPOSITE field lookup:** Use `std::unordered_map<std::string, size_t>` for O(1) access (adds 16 bytes overhead)
2. **VECTOR hash sets:** Pre-compute hash for immutable vectors

---

## Migration Strategy

### Backward Compatibility

All changes are **additive** - no breaking changes to existing TypedValue API.

**Phase 1:** Add new types without changing existing code
**Phase 2:** Update DomainManager to use new types
**Phase 3:** Add SQL parser support for RECORD/SET/VARIANT syntax

### Rollout Plan

1. **Week 1-2:** Implement COMPOSITE support (12 hours)
2. **Week 3-5:** Implement VECTOR support (20 hours)
3. **Week 6-7:** Implement VARIANT support (11 hours)
4. **Week 8:** Operator overloading + integration tests (7 hours)

**Total: 50 hours over 8 weeks (part-time)**

---

## Success Criteria

### Functionality
- ✅ All 9 DomainManager operations implemented and tested
- ✅ Full CRUD support for COMPOSITE, VECTOR, VARIANT types
- ✅ Binary serialization/deserialization working
- ✅ Integration tests passing (30+ tests)

### Performance
- ✅ COMPOSITE field access: < 100ns for 10-field records
- ✅ VECTOR element access: < 50ns
- ✅ Set operations scale to 10000 elements without OOM
- ✅ Serialization: < 1μs per 100 bytes

### Quality
- ✅ Zero memory leaks (Valgrind clean)
- ✅ Thread-safe if TypedValue is const
- ✅ Code coverage > 90% for new code
- ✅ Documentation complete (function headers + examples)

---

## Future Enhancements (Post-Phase 2)

### Phase 3+

1. **Optimized COMPOSITE field lookup**
   - Hash map for O(1) field access
   - Trade-off: 16 bytes overhead per COMPOSITE

2. **VECTOR sorted sets**
   - Binary search for contains (O(log n))
   - Requires element ordering

3. **VARIANT implicit casting**
   - Widening casts (INT32 → INT64)
   - Conversion functions (TEXT → INT32)

4. **Lazy serialization**
   - Serialize only when needed (write to disk)
   - Keep in-memory representation otherwise

5. **COMPOSITE inheritance**
   - Structural subtyping
   - Field shadowing

---

## References

### Firebird MGA Compliance

**CRITICAL:** All new types must follow Firebird MGA rules:
- No PostgreSQL MVCC patterns
- Stable TIDs for COMPOSITE/VECTOR/VARIANT values in tables
- Back-versioning for updates

See: `/MGA_RULES.md`

### Related Specifications

- `/docs/specifications/DATA_TYPES_SPEC.md` - Base type system
- `/docs/specifications/DOMAIN_SPEC.md` - Domain definitions
- `/docs/archive/2026-01-04/planning/old_Plans/archive/CRUD_IMPLEMENTATION_PLAN.md` - Agent D tasks

### External Resources

- PostgreSQL COMPOSITE types: https://www.postgresql.org/docs/current/rowtypes.html
- SQL/Foundation VARIANT: ISO/IEC 9075-2:2023 Section 4.7
- C++ std::variant reference: https://en.cppreference.com/w/cpp/utility/variant

---

**Document Version:** 1.0
**Created:** November 24, 2025
**Last Updated:** November 24, 2025
**Author:** Agent D (Claude Code)
**Status:** Planning Document - Ready for Implementation
