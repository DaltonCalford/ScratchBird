# Specification: Type Coercion Rules

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | types / parser / executor |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.3.0 |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:534`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2027`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:632`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/converted/functional/gtcs/cast-datatypes.sql`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/mysql/converted/main/range_types.sql`

## Synopsis

This specification defines the type coercion, casting, and conversion rules for ScratchBird. It covers implicit coercion for operator resolution, explicit cast operations, and the conversion matrices between types. These rules apply to the native parser; emulated parsers may gate or translate according to target dialect requirements.

## Scope

### In Scope

- Explicit conversion rules (`CAST`, `::` syntax)
- Implicit coercion for operators (+, -, *, /, ||, comparison)
- Numeric promotion rules
- String/text coercion patterns
- Temporal conversion rules
- Boolean coercion
- UUID, Network type conversions
- JSON/Container type conversions
- Error handling for invalid conversions

### Out of Scope

- Emulated engine-specific conversion behaviors
- Collation/coercion interactions (see collation specs)
- Encoding conversions (see character set specs)

## Background

ScratchBird's type coercion system supports both strict and permissive modes (configurable via `types.coercion_context`). The coercion rules are designed to:
1. Preserve data integrity where possible
2. Follow SQL standard behaviors where applicable
3. Support implicit conversions for common operations
4. Provide explicit casting for all valid conversions

## Specification

### Explicit Convertibility Matrix

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:534
bool TypeSystem::isExplicitlyConvertible(DataType from, DataType to)
{
    // Same type is always convertible
    if (from == to) return true;
    
    // NULL can convert to any type
    if (from == DataType::NULL_TYPE) return true;
    
    // ... additional rules below
}
```

#### Core Conversion Rules

| From | To | Allowed | Notes |
|------|-----|---------|-------|
| Any scalar | Same type | Yes | Identity conversion |
| NULL | Any type | Yes | NULL propagation |
| Any numeric | Any numeric | Yes | May lose precision |
| BOOLEAN | Any numeric | Yes | 0/1 representation |
| Any numeric | BOOLEAN | Yes | 0 = false, non-zero = true |
| String | String | Yes | Length truncation possible |
| String | Numeric | Yes | Parse required |
| Numeric | String | Yes | Format to text |
| String | Binary | Yes | Character encoding |
| Binary | String | Yes | Hex/escape encoding |
| DATE | TIMESTAMP | Yes | Time component = midnight |
| TIMESTAMP | DATE | Yes | Truncate time component |
| TIME | TIMESTAMP | Yes | Date = current date |
| TIMESTAMP | TIME | Yes | Extract time component |
| TIMESTAMP | TIMESTAMP WITH ZONE | Yes | Add zone info |
| TIMESTAMP WITH ZONE | TIMESTAMP | Yes | Normalize to UTC |
| UUID | String | Yes | Hex with hyphens format |
| String | UUID | Yes | Parse hex format |
| UUID | Binary | Yes | Raw 16 bytes |
| Binary | UUID | Yes | Raw 16 bytes |
| INET | CIDR | Yes | Network representation |
| CIDR | INET | Yes | Host address |
| JSON | JSONB | Yes | Parse and canonicalize |
| JSONB | JSON | Yes | Serialize to text |
| JSON | String | Yes | JSON text |
| String | JSON | Yes | Parse JSON |
| Array element | Array element | Yes | Per-element conversion |

### Numeric Conversion Matrix

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:609
if (isNumericType(from) && isNumericType(to))
{
    return true;
}
```

All numeric types are explicitly convertible to each other. Conversion follows these rules:

#### Integer Conversions

| From | To | Behavior |
|------|-----|----------|
| Signed | Unsigned | Check non-negative, truncate if overflow |
| Unsigned | Signed | Check fits, truncate if overflow |
| Narrower | Wider | Zero/sign extend |
| Wider | Narrower | Truncate, check overflow |

#### Float Conversions

| From | To | Behavior |
|------|-----|----------|
| FLOAT32 | FLOAT64 | Exact representation |
| FLOAT64 | FLOAT32 | Round, check overflow |
| Integer | Float | Exact for <= 2^53, round otherwise |
| Float | Integer | Truncate toward zero |

#### Decimal Conversions

| From | To | Behavior |
|------|-----|----------|
| DECIMAL | DECIMAL | Scale adjustment, precision check |
| Integer | DECIMAL | Exact representation |
| DECIMAL | Integer | Truncate fraction |
| Float | DECIMAL | Rounding per configured mode |
| DECIMAL | Float | Possible precision loss |

### Boolean Conversion Matrix

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:615
if ((from == DataType::BOOLEAN && (to == DataType::INT8 || isNumericType(to))) ||
    (to == DataType::BOOLEAN && (from == DataType::INT8 || isNumericType(from))))
{
    return true;
}
```

| From | To | Behavior |
|------|-----|----------|
| BOOLEAN | Integer | false = 0, true = 1 |
| Integer | BOOLEAN | 0 = false, non-zero = true |
| BOOLEAN | String | "true" / "false" |
| String | BOOLEAN | "true", "t", "1" → true; "false", "f", "0" → false |

### String/Binary Conversion Matrix

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:622
if (isStringType(from) && isStringType(to))
{
    return true;
}
if (isBinaryType(from) && isBinaryType(to))
{
    return true;
}
if ((isStringType(from) && isBinaryType(to)) || 
    (isBinaryType(from) && isStringType(to)))
{
    return true;
}
```

| From | To | Format |
|------|-----|--------|
| String | Binary | UTF-8 bytes |
| Binary | String | Hex (0x...) or Escape |
| VARCHAR | CHAR | Space pad |
| CHAR | VARCHAR | Truncate trailing spaces |

### Temporal Conversion Matrix

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:636
if ((from == DataType::DATE && to == DataType::TIMESTAMP) ||
    (from == DataType::TIMESTAMP && to == DataType::DATE) ||
    (from == DataType::TIME && to == DataType::TIMESTAMP) ||
    (from == DataType::TIMESTAMP && to == DataType::TIME) ||
    (from == DataType::TIMESTAMP && to == DataType::TIMESTAMP_WITH_ZONE) ||
    (from == DataType::TIMESTAMP_WITH_ZONE && to == DataType::TIMESTAMP) ||
    (from == DataType::TIME && to == DataType::TIME_WITH_ZONE) ||
    (from == DataType::TIME_WITH_ZONE && to == DataType::TIME))
{
    return true;
}
```

| From | To | Behavior |
|------|-----|----------|
| DATE | TIMESTAMP | Add time = 00:00:00 |
| TIMESTAMP | DATE | Truncate time |
| TIME | TIMESTAMP | Add current date |
| TIMESTAMP | TIME | Extract time |
| TIMESTAMP | TIMESTAMP WITH ZONE | Apply timezone offset |
| TIMESTAMP WITH ZONE | TIMESTAMP | Normalize to UTC, strip zone |
| TIME | TIME WITH ZONE | Apply timezone offset |
| TIME WITH ZONE | TIME | Strip zone info |

### Network Type Conversions

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:662
if ((from == DataType::INET && to == DataType::CIDR) ||
    (from == DataType::CIDR && to == DataType::INET))
{
    return true;
}
if ((isNetworkType(from) && isStringType(to)) || 
    (isStringType(from) && isNetworkType(to)))
{
    return true;
}
```

| From | To | Format |
|------|-----|--------|
| INET | CIDR | Convert to network address |
| CIDR | INET | Use network address as host |
| INET/CIDR | String | IPv4: a.b.c.d, IPv6: [a:b:c:d:e:f:g:h] |
| String | INET/CIDR | Parse standard notation |
| MACADDR | MACADDR8 | Pad with 0xFFFE |
| MACADDR8 | MACADDR | Truncate (if EUI-64 compatible) |

### UUID Conversions

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:653
if ((from == DataType::UUID && isStringType(to)) ||
    (to == DataType::UUID && isStringType(from)) ||
    (from == DataType::UUID && isBinaryType(to)) ||
    (to == DataType::UUID && isBinaryType(from)))
{
    return true;
}
```

| From | To | Format |
|------|-----|--------|
| UUID | String | xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx |
| String | UUID | Parse hex with optional hyphens/braces |
| UUID | Binary | 16 raw bytes |
| Binary | UUID | 16 bytes to UUID |

### JSON Conversions

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:673
if (isJsonFamily(from) && isJsonFamily(to))
{
    return true;
}
if ((isStringType(from) && isJsonFamily(to)) || 
    (isJsonFamily(from) && isStringType(to)))
{
    return true;
}
if ((isContainerType(from) && isJsonFamily(to)) ||
    (isJsonFamily(from) && isContainerType(to)))
{
    return true;
}
```

| From | To | Behavior |
|------|-----|----------|
| JSON | JSONB | Parse and canonicalize |
| JSONB | JSON | Serialize |
| JSON | String | Direct text |
| String | JSON | Validate and store |
| ARRAY | JSON | Convert to JSON array |
| COMPOSITE | JSON | Convert to JSON object |

### Container Type Conversions

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:695
if ((isContainerType(from) && isStringType(to)) ||
    (isStringType(from) && isContainerType(to)))
{
    return true;
}
```

| From | To | Behavior |
|------|-----|----------|
| ARRAY | String | Render as text representation |
| String | ARRAY | Parse text representation |
| COMPOSITE | String | Render as (field1,field2,...) |
| String | COMPOSITE | Parse composite literal |

### vNext Extended Types

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:542
auto is_vnext_type = [](DataType type) -> bool
{
    return type == DataType::TIMESTAMP_NS ||
           type == DataType::INT256 ||
           type == DataType::UINT256 ||
           type == DataType::DECIMAL256 ||
           type == DataType::TAGGED_UNION ||
           type == DataType::DICT_ENCODED ||
           type == DataType::COMPLETION_FIELD ||
           type == DataType::PREFIX_SEARCH_FIELD ||
           type == DataType::FLAT_OBJECT;
};
```

| From | To | Notes |
|------|-----|-------|
| INT256 | DECIMAL256 | Exact conversion |
| UINT256 | DECIMAL256 | Exact conversion |
| DECIMAL256 | INT256 | Truncate fraction |
| DECIMAL256 | UINT256 | Truncate fraction, check non-negative |
| UINT256 | INT256 | Check fits |
| TIMESTAMP_NS | TIMESTAMP | Truncate nanoseconds |
| TIMESTAMP | TIMESTAMP_NS | Pad with zeros |
| TAGGED_UNION | Scalar | Extract current variant |
| Scalar | TAGGED_UNION | Wrap in union |
| DICT_ENCODED | Scalar | Decode from dictionary |
| Scalar | DICT_ENCODED | Encode to dictionary |

## Implicit Coercion Rules

### Coercion Context Mode

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2027
enum class CoercionContextMode : uint8_t
{
    STRICT = 0,
    PERMISSIVE = 1,
};

CoercionContextMode coercionContextMode()
{
    std::string configured = Config::getInstance().getString(
        "types", "coercion_context", "STRICT");
    if (configured == "PERMISSIVE")
        return CoercionContextMode::PERMISSIVE;
    return CoercionContextMode::STRICT;
}
```

### Numeric Operators (+, -, *, /, %)

1. If both operands are numeric, use numeric promotion rules
2. If one operand is text, attempt to parse as numeric
3. If parsing fails, error with `INVALID_TEXT_REPRESENTATION`

**Promotion Order:**
```
INT8 → INT16 → INT32 → INT64 → INT128
      ↓        ↓         ↓         ↓
    FLOAT32 → FLOAT64 ← DECIMAL ← DECFLOAT
```

### Text Concatenation (||)

1. Both operands coerced to text using canonical formatting
2. UUIDs render as standard hex with hyphens
3. Timestamps render in ISO-8601 format
4. Numerics render in standard decimal notation

### Comparison Operators (=, !=, <, <=, >, >=)

1. If types match, compare directly
2. If one operand is text and other is numeric/temporal:
   - Parse text to target type
   - If parsing fails, error
3. NULL comparisons return NULL (three-valued logic)

### Boolean Coercion

Accepted text values for boolean context:
- True: `"true"`, `"t"`, `"1"`, `"yes"`, `"y"` (case-insensitive)
- False: `"false"`, `"f"`, `"0"`, `"no"`, `"n"` (case-insensitive)

## Algorithms

### Numeric Promotion

```
Input:  Two numeric operands with types T1, T2
Output: Common type for operation

1. If T1 == T2, return T1
2. If either is FLOAT64, return FLOAT64
3. If either is DECIMAL, return DECIMAL (precision = max)
4. If either is FLOAT32, return FLOAT32
5. Return wider of T1, T2 (by bit width)
```

### String-to-Numeric Parsing

```
Input:  String s, target numeric type T
Output: Numeric value or error

1. Trim whitespace from s
2. Detect format (hex if 0x prefix)
3. Parse using appropriate base
4. Check range against T limits
5. Return value or NUMERIC_VALUE_OUT_OF_RANGE
```

### Temporal Parsing (Firebird-compatible)

```
Input:  String s, target temporal type T
Output: Temporal value or error

1. Trim whitespace from s
2. Match against known patterns:
   - DATE: YYYY-MM-DD
   - TIME: HH:MM:SS[.fractional]
   - TIMESTAMP: YYYY-MM-DD HH:MM:SS[.fractional]
3. Validate component ranges
4. Return value or DATETIME_VALUE_OUT_OF_RANGE
```

## DECFLOAT Coercion Implementation

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:632
Status coerceToDecfloat(const TypedValue& source, DataType target,
                        DecFloat& out, ErrorContext* ctx)
{
    DecFloatContext df_ctx = defaultDecfloatContext();
    
    if (source.type() == DataType::DECFLOAT16 || 
        source.type() == DataType::DECFLOAT34)
    {
        return decodeDecfloat(source.getDecfloatBytes(), source.type(), out, ctx);
    }
    
    if (source.type() == DataType::DECIMAL)
    {
        // Convert via string representation
        Decimal dec(source.getDecimalUnscaled(), 
                   source.getDecimalPrecision(), 
                   source.getDecimalScale());
        return DecFloat::parse(dec.toStringWithPrecision(dec.scale()),
                              target == DataType::DECFLOAT16 ? 16 : 34,
                              df_ctx, out, ctx);
    }
    
    if (isIntegerType(source.type()) || source.type() == DataType::BOOLEAN)
    {
        return DecFloat::parse(source.toString(),
                              target == DataType::DECFLOAT16 ? 16 : 34,
                              df_ctx, out, ctx);
    }
    
    if (isFloatType(source.type()))
    {
        // Use scientific notation with full precision
        std::ostringstream oss;
        oss.setf(std::ios::scientific);
        oss << std::setprecision(std::numeric_limits<double>::max_digits10)
            << (source.type() == DataType::FLOAT32 
                ? static_cast<double>(source.getFloat32())
                : source.getFloat64());
        return DecFloat::parse(oss.str(),
                              target == DataType::DECFLOAT16 ? 16 : 34,
                              df_ctx, out, ctx);
    }
    
    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                      "Cannot convert to DECFLOAT");
    return Status::DATATYPE_MISMATCH;
}
```

## Invariants

1. **NULL Propagation**: NULL converts to any type and remains NULL
   - Verification: NULL check before conversion

2. **Identity Conversion**: Same-type conversion always succeeds
   - Verification: Early return in conversion functions

3. **Precision Loss Warning**: Conversions that lose precision may log warnings
   - Verification: Configurable via `types.precision_loss_warning`

4. **Reversibility**: Some conversions are lossy and not reversible
   - FLOAT64 → FLOAT32 → FLOAT64 may differ
   - TIMESTAMP → DATE → TIMESTAMP loses time

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_TEXT_REPRESENTATION` | String parse failure | Return error to caller |
| `NUMERIC_VALUE_OUT_OF_RANGE` | Value exceeds target range | Return error to caller |
| `DATETIME_VALUE_OUT_OF_RANGE` | Invalid date/time value | Return error to caller |
| `DATATYPE_MISMATCH` | Incompatible types for operation | Return error to caller |
| `INVALID_ARGUMENT` | Invalid conversion parameters | Return error to caller |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/compatibility/firebird/converted/functional/gtcs/cast-datatypes.sql` | CAST operations |
| `tests/compatibility/mysql/converted/main/range_types.sql` | Type ranges |
| `tests/compatibility/mysql/converted/rpl_nogtid/rpl_typeconv.sql` | Type conversions |
| `tests/compatibility/postgresql/converted/core/create_type.sql` | Type casting |

## Related Specifications

- [scalar_types.md](./scalar_types.md) - Scalar type definitions
- [complex_types.md](./complex_types.md) - Complex type definitions
- Implicit coercion rules from `/home/dcalford/CliWork/local_work/docs/specifications/13_Operator_Model_and_Coercion/IMPLICIT_COERCION_RULES.md`

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Coercion | Implicit type conversion by the engine |
| Cast | Explicit type conversion requested by user |
| Promotion | Widening conversion to larger type |
| Narrowing | Conversion to smaller type (may lose data) |
| Canonical | Standard/internal representation |

### Configuration Keys

| Key | Default | Description |
|-----|---------|-------------|
| `types.coercion_context` | STRICT | Coercion mode (STRICT/PERMISSIVE) |
| `types.precision_loss_warning` | true | Warn on precision loss |
| `types.decimal_rounding_mode` | HALF_EVEN | Rounding mode for decimals |

### Cast Syntax

| Syntax | Description |
|--------|-------------|
| `CAST(expr AS type)` | Standard SQL cast |
| `expr::type` | PostgreSQL-style cast |
| `TYPEOF(expr)` | Get type information |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial type coercion specification | Dalton Calford |
