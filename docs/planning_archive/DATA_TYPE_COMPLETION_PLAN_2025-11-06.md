# Data Type Completion Plan - November 6, 2025

**Created**: November 6, 2025 Evening
**Status**: READY FOR IMPLEMENTATION
**Scope**: Complete all missing data type operations
**Estimated Effort**: 110-160 hours (2.75-4 months single developer)

---

## EXECUTIVE SUMMARY

### Current Status

**Data Types**: 83/86 Complete (97%)
**Remaining Work**: 3 types + Domain support = 110-160 hours

### Missing Features

1. **COMPOSITE Type Operations** - 30-40 hours (CRITICAL)
2. **VECTOR Element Access & Operations** - 20-30 hours (CRITICAL)
3. **VARIANT Type Operations** - 40-60 hours (HIGH)
4. **Domain Constraint Enforcement** - 20-30 hours (CRITICAL)

**Total**: 110-160 hours

---

## PRIORITY ASSESSMENT

### Critical Path Analysis

Based on impact and dependencies:

**P0 (CRITICAL)**: 70-100 hours
1. Domain Constraint Enforcement (20-30h) - **BLOCKS DATA INTEGRITY**
2. COMPOSITE Type Operations (30-40h) - **BLOCKS ADVANCED QUERIES**
3. VECTOR Operations (20-30h) - **BLOCKS HNSW VECTOR QUERIES**

**P1 (HIGH)**: 40-60 hours
4. VARIANT Type Operations (40-60h) - **ENABLES POLYMORPHIC DATA**

---

## PART 1: DOMAIN CONSTRAINT ENFORCEMENT (P0)

### Current Status

**File**: `src/core/domain_manager.cpp:1364`
**Issue**: CHECK constraint evaluation not implemented
**Impact**: Domain constraints not enforced, data integrity compromised

### Implementation Plan

#### Phase 1: Expression Evaluator Integration (10-15 hours)

**File**: `src/core/domain_manager.cpp`

**Current Code** (line 1363-1369):
```cpp
auto DomainManager::validateCheckConstraint(const TypedValue& value,
                                            const DomainConstraint& constraint,
                                            ErrorContext* ctx) -> Status
{
    // TODO: Implement CHECK constraint evaluation
    // This requires integration with the expression evaluator
    // For now, just log and return OK
    LOG_DEBUG(CATALOG, "CHECK constraint validation not yet implemented");
    return Status::OK;
}
```

**Proposed Implementation**:
```cpp
auto DomainManager::validateCheckConstraint(const TypedValue& value,
                                            const DomainConstraint& constraint,
                                            ErrorContext* ctx) -> Status
{
    // Parse constraint expression if not already parsed
    if (!constraint.parsed_expression) {
        // Parse the CHECK expression
        auto expr = ExpressionParser::parse(constraint.expression);
        if (!expr) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_CONSTRAINT,
                            "Failed to parse CHECK constraint");
            return Status::INVALID_CONSTRAINT;
        }
        constraint.parsed_expression = std::move(expr);
    }

    // Create evaluation context with the value
    EvaluationContext eval_ctx;
    eval_ctx.bindValue("VALUE", value);  // Special variable for domain checks

    // Evaluate the constraint
    TypedValue result;
    Status status = ExpressionEvaluator::evaluate(
        constraint.parsed_expression.get(),
        eval_ctx,
        &result,
        ctx
    );

    if (status != Status::OK) {
        return status;
    }

    // Constraint must evaluate to boolean true
    if (result.type != TypeID::BOOLEAN) {
        SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                        "CHECK constraint must return boolean");
        return Status::TYPE_MISMATCH;
    }

    if (!result.getBool()) {
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                        "CHECK constraint violated: " + constraint.expression);
        return Status::CONSTRAINT_VIOLATION;
    }

    return Status::OK;
}
```

**Dependencies**:
- Expression parser already exists
- Expression evaluator already exists
- Need to add VALUE variable binding

#### Phase 2: Domain DDL Integration (5-10 hours)

**Tasks**:
1. Integrate constraint validation into INSERT/UPDATE paths
2. Add domain constraint violation error messages
3. Test with various CHECK expressions

**Files to Modify**:
- `src/core/domain_manager.cpp` (validateCheckConstraint)
- `src/sblr/expression_evaluator.cpp` (VALUE variable support)
- `src/core/storage_engine.cpp` (integrate validation)

#### Phase 3: Testing (5-10 hours)

**Test Cases**:
```sql
-- Test 1: Basic CHECK constraint
CREATE DOMAIN positive_int AS INTEGER CHECK (VALUE > 0);

-- Test 2: Complex CHECK constraint
CREATE DOMAIN email AS TEXT
CHECK (VALUE LIKE '%@%.%');

-- Test 3: Range CHECK
CREATE DOMAIN percentage AS NUMERIC(5,2)
CHECK (VALUE >= 0 AND VALUE <= 100);

-- Test 4: Constraint violation
INSERT INTO table VALUES (-5::positive_int);  -- Should fail
```

**Success Criteria**:
- ✅ CHECK constraints evaluated correctly
- ✅ Violations caught and reported
- ✅ Performance acceptable (< 1ms per check)

---

## PART 2: COMPOSITE TYPE OPERATIONS (P0)

### Current Status

**File**: `src/core/domain_manager.cpp:502`
**Issue**: COMPOSITE type operations not implemented
**Impact**: Cannot use structured types in queries

### Implementation Plan

#### Phase 1: Type System Extension (10-15 hours)

**Extend TypedValue to Support COMPOSITE**:
```cpp
// In typed_value.h
class TypedValue {
public:
    // ... existing methods ...

    // COMPOSITE support
    struct CompositeValue {
        std::vector<std::string> field_names;
        std::vector<TypedValue> field_values;
    };

    TypedValue(CompositeValue&& composite);
    const CompositeValue& getComposite() const;
    TypedValue getField(const std::string& field_name) const;
    void setField(const std::string& field_name, const TypedValue& value);

private:
    std::variant<
        // ... existing types ...
        CompositeValue
    > value_;
};
```

**Files to Modify**:
- `include/scratchbird/core/typed_value.h`
- `src/core/typed_value.cpp`

#### Phase 2: Field Access Operations (10-15 hours)

**Implement in domain_manager.cpp**:
```cpp
Status DomainManager::getCompositeField(const TypedValue& composite,
                                        const std::string& field_name,
                                        TypedValue* result_out,
                                        ErrorContext* ctx)
{
    if (composite.type != TypeID::COMPOSITE) {
        SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                        "Expected COMPOSITE type");
        return Status::TYPE_MISMATCH;
    }

    const auto& comp = composite.getComposite();

    // Find field by name
    for (size_t i = 0; i < comp.field_names.size(); ++i) {
        if (comp.field_names[i] == field_name) {
            *result_out = comp.field_values[i];
            return Status::OK;
        }
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                    "Field not found: " + field_name);
    return Status::NOT_FOUND;
}
```

#### Phase 3: SQL Integration (10-15 hours)

**Parser Integration**:
```sql
-- Test composite access
SELECT person.name, person.age FROM employees;
SELECT (ROW(1, 'Alice', 30)).name;
```

**Expression Evaluator Integration**:
- Handle dot notation: `value.field`
- Handle ROW construction: `ROW(val1, val2, ...)`

**Success Criteria**:
- ✅ COMPOSITE type created and stored
- ✅ Field access works (value.field_name)
- ✅ ROW constructor works
- ✅ Integration tests pass

---

## PART 3: VECTOR ELEMENT ACCESS & OPERATIONS (P0)

### Current Status

**File**: `src/core/domain_manager.cpp:830-931`
**Issue**: 6 vector operations stubbed with NOT_IMPLEMENTED
**Impact**: Cannot use vectors in queries despite HNSW index

### Implementation Plan

#### Phase 1: Element Access (10-15 hours)

**Implement subscript operator**:
```cpp
Status DomainManager::vectorElementAccess(const TypedValue& vector,
                                          size_t index,
                                          TypedValue* result_out,
                                          ErrorContext* ctx)
{
    if (vector.type != TypeID::VECTOR) {
        SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Expected VECTOR type");
        return Status::TYPE_MISMATCH;
    }

    const VectorValue& vec = vector.getVector();

    if (index >= vec.getDimensions()) {
        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Vector index out of range");
        return Status::OUT_OF_RANGE;
    }

    // Get element as float64
    auto elem = vec.getAsFloat64(index);
    if (!elem.has_value()) {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Failed to access vector element");
        return Status::INTERNAL_ERROR;
    }

    *result_out = TypedValue(*elem);
    return Status::OK;
}
```

**Implement slice operator**:
```cpp
Status DomainManager::vectorSlice(const TypedValue& vector,
                                  size_t start,
                                  size_t end,
                                  TypedValue* result_out,
                                  ErrorContext* ctx)
{
    if (vector.type != TypeID::VECTOR) {
        SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Expected VECTOR type");
        return Status::TYPE_MISMATCH;
    }

    const VectorValue& vec = vector.getVector();
    size_t dims = vec.getDimensions();

    if (start >= dims || end > dims || start >= end) {
        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Invalid slice range");
        return Status::OUT_OF_RANGE;
    }

    // Extract slice
    std::vector<float> slice_data;
    slice_data.reserve(end - start);

    for (size_t i = start; i < end; ++i) {
        auto elem = vec.getAsFloat64(i);
        slice_data.push_back(static_cast<float>(*elem));
    }

    *result_out = TypedValue(VectorValue(std::move(slice_data)));
    return Status::OK;
}
```

#### Phase 2: Distance Operators (5-10 hours)

**Already implemented in VectorValue::distance()**, just need SQL integration:

```cpp
Status DomainManager::vectorDistance(const TypedValue& vec1,
                                     const TypedValue& vec2,
                                     DistanceMetric metric,
                                     TypedValue* result_out,
                                     ErrorContext* ctx)
{
    if (vec1.type != TypeID::VECTOR || vec2.type != TypeID::VECTOR) {
        SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Expected VECTOR types");
        return Status::TYPE_MISMATCH;
    }

    const VectorValue& v1 = vec1.getVector();
    const VectorValue& v2 = vec2.getVector();

    auto distance = v1.distance(v2, metric);
    if (!distance.has_value()) {
        SET_ERROR_CONTEXT(ctx, Status::DIMENSION_MISMATCH,
                        "Vector dimension mismatch");
        return Status::DIMENSION_MISMATCH;
    }

    *result_out = TypedValue(*distance);
    return Status::OK;
}
```

#### Phase 3: Vector Arithmetic (5-10 hours)

**Addition, subtraction, scalar multiplication** (already in VectorValue):
- Just need SQL operator integration
- `vector1 + vector2`
- `vector1 - vector2`
- `vector1 * scalar`

**Success Criteria**:
- ✅ vector[index] works
- ✅ vector[start:end] works
- ✅ Distance operators work (<->, <=>, <#>)
- ✅ Arithmetic operators work (+, -, *)

---

## PART 4: VARIANT TYPE OPERATIONS (P1)

### Current Status

**File**: `src/core/domain_manager.cpp:1011-1051`
**Issue**: 3 VARIANT operations stubbed
**Impact**: Cannot use polymorphic data

### Implementation Plan

#### Phase 1: Tagged Union Structure (15-20 hours)

**Extend TypedValue**:
```cpp
class TypedValue {
public:
    // VARIANT support
    struct VariantValue {
        TypeID actual_type;
        TypedValue value;
    };

    TypedValue(VariantValue&& variant);
    const VariantValue& getVariant() const;
    TypeID getVariantType() const;
    TypedValue unwrapVariant() const;
};
```

#### Phase 2: Type Operations (15-25 hours)

**IS type checking**:
```cpp
Status DomainManager::variantIsType(const TypedValue& variant,
                                    TypeID expected_type,
                                    bool* result_out,
                                    ErrorContext* ctx)
{
    if (variant.type != TypeID::VARIANT) {
        SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Expected VARIANT type");
        return Status::TYPE_MISMATCH;
    }

    *result_out = (variant.getVariantType() == expected_type);
    return Status::OK;
}
```

**Type-safe casting**:
```cpp
Status DomainManager::variantCast(const TypedValue& variant,
                                  TypeID target_type,
                                  TypedValue* result_out,
                                  ErrorContext* ctx)
{
    if (variant.type != TypeID::VARIANT) {
        SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Expected VARIANT type");
        return Status::TYPE_MISMATCH;
    }

    TypeID actual_type = variant.getVariantType();

    // Direct match
    if (actual_type == target_type) {
        *result_out = variant.unwrapVariant();
        return Status::OK;
    }

    // Try implicit coercion
    if (canImplicitlyCast(actual_type, target_type)) {
        return castValue(variant.unwrapVariant(), target_type, result_out, ctx);
    }

    SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                    "Cannot cast VARIANT to target type");
    return Status::TYPE_MISMATCH;
}
```

#### Phase 3: SQL Integration (10-15 hours)

**Parser and evaluator integration**:
```sql
-- Test VARIANT usage
CREATE TABLE mixed_data (id INT, data VARIANT);
INSERT INTO mixed_data VALUES (1, 42::VARIANT);
INSERT INTO mixed_data VALUES (2, 'text'::VARIANT);
SELECT * FROM mixed_data WHERE data IS INT;
```

**Success Criteria**:
- ✅ VARIANT can hold any type
- ✅ IS type_name works
- ✅ CAST works with proper error handling
- ✅ Type safety maintained

---

## IMPLEMENTATION TIMELINE

### Single Developer (Sequential)

**Week 1-2** (40 hours):
- Domain Constraint Enforcement (20-30h)

**Week 3-4** (40 hours):
- COMPOSITE Type Operations (30-40h)

**Week 5-6** (40 hours):
- VECTOR Operations (20-30h)
- Buffer for testing/debugging (10-20h)

**Week 7-10** (80 hours):
- VARIANT Operations (40-60h)
- Integration testing (20-30h)
- Documentation (10-20h)

**Total**: 10 weeks (2.5 months at 40 hours/week)

### Two Developers (Parallel)

**Week 1-3** (60 hours):
- **Developer A**: Domain constraints (20-30h) + COMPOSITE (30-40h)
- **Developer B**: VECTOR operations (20-30h) + Start VARIANT (30h)

**Week 4-6** (60 hours):
- **Developer A**: Complete COMPOSITE + Integration testing
- **Developer B**: Complete VARIANT (30h) + Integration testing

**Total**: 6 weeks (1.5 months with 2 developers)

---

## TESTING STRATEGY

### Unit Tests (Per Feature)

1. **Domain Constraints**: 20+ tests
   - Valid constraints pass
   - Invalid constraints fail
   - Edge cases (NULL, type mismatch)

2. **COMPOSITE**: 15+ tests
   - Field access
   - ROW construction
   - Nested composites

3. **VECTOR**: 20+ tests
   - Element access (bounds checking)
   - Slicing (various ranges)
   - Distance operators
   - Arithmetic

4. **VARIANT**: 15+ tests
   - Type storage and retrieval
   - IS operator
   - CAST operations
   - Type safety

### Integration Tests

**SQL Test Suite**:
```sql
-- Domain constraints
CREATE DOMAIN email AS TEXT CHECK (VALUE LIKE '%@%.%');
INSERT INTO users (email) VALUES ('test@example.com');  -- Pass
INSERT INTO users (email) VALUES ('invalid');  -- Fail

-- COMPOSITE
CREATE TYPE person AS (name TEXT, age INT);
SELECT (ROW('Alice', 30)::person).name;

-- VECTOR
CREATE TABLE embeddings (id INT, vec VECTOR(3));
SELECT vec[0] FROM embeddings;
SELECT * FROM embeddings ORDER BY vec <-> '[1,2,3]';

-- VARIANT
CREATE TABLE mixed (id INT, data VARIANT);
SELECT * FROM mixed WHERE data IS INT;
```

### Performance Tests

- Domain constraint evaluation: < 1ms per check
- COMPOSITE field access: < 100ns
- VECTOR operations: Same as existing VectorValue (tested)
- VARIANT operations: < 500ns overhead

---

## SUCCESS CRITERIA

### Functional Requirements

- ✅ All NOT_IMPLEMENTED blocks removed
- ✅ All data types fully operational
- ✅ SQL integration complete
- ✅ 70+ tests passing

### Quality Requirements

- ✅ MGA compliance maintained
- ✅ Type safety enforced
- ✅ Error messages clear and helpful
- ✅ Performance acceptable

### Documentation Requirements

- ✅ User documentation updated
- ✅ API documentation complete
- ✅ Test coverage documented
- ✅ Migration guide for users

---

## RISKS AND MITIGATION

### Risk 1: TypedValue Complexity

**Risk**: Adding COMPOSITE/VARIANT may complicate TypedValue
**Mitigation**: Use std::variant internally, careful API design
**Fallback**: Separate CompositeValue/VariantValue classes

### Risk 2: Expression Evaluator Integration

**Risk**: Domain constraints need full expression evaluation
**Mitigation**: Leverage existing expression evaluator
**Fallback**: Support simple constraints first, expand later

### Risk 3: Performance Impact

**Risk**: Type checking overhead for VARIANT
**Mitigation**: Benchmark and optimize hot paths
**Fallback**: Add fast-path for common types

### Risk 4: Timeline Overrun

**Risk**: 110-160 hours is substantial
**Mitigation**: Prioritize P0 items first
**Fallback**: Ship with partial VARIANT support

---

## DEPENDENCIES

### External Dependencies

- None (all features use existing codebase)

### Internal Dependencies

1. Expression evaluator (exists)
2. Parser (exists)
3. Type system (exists)
4. Storage engine (exists)

### Blocker Resolution

- No blockers identified
- All prerequisites in place
- Ready for implementation

---

## NEXT STEPS

1. **Review and Approve**: Get plan approval
2. **Set Up Branch**: Create feature/data-types-completion branch
3. **Start with P0**: Begin with Domain constraints (highest impact)
4. **Iterate**: Complete one feature at a time with tests
5. **Integrate**: Merge to main when all tests pass
6. **Document**: Update all documentation

---

## RELATED DOCUMENTATION

- `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md` - Overall plan
- `/include/scratchbird/core/typed_value.h` - Type system API
- `/src/core/domain_manager.cpp` - Implementation file
- `/src/sblr/expression_evaluator.cpp` - Expression evaluation

---

**Plan Status**: READY FOR IMPLEMENTATION
**Created**: November 6, 2025 Evening
**Estimated Completion**: February 2026 (single developer) or January 2026 (2 developers)
