# Extended Numeric Types: 128-bit Integer Support

## Overview

ScratchBird provides native support for 128-bit integers (both signed and unsigned), matching and exceeding Firebird's INT128 support.

## 128-bit Integer Types

### Type Definitions

```sql
-- Signed 128-bit integer
INT128  -- Range: -170,141,183,460,469,231,731,687,303,715,884,105,728 to 
        --        170,141,183,460,469,231,731,687,303,715,884,105,727

-- Unsigned 128-bit integer  
UINT128 -- Range: 0 to 340,282,366,920,938,463,463,374,607,431,768,211,455

-- Aliases for compatibility
HUGEINT      -- Alias for INT128 (DuckDB style)
LARGINT      -- Alias for INT128
DECIMAL(39,0) -- Can store INT128 range
```

## Internal Implementation

```cpp
// Native 128-bit support using compiler intrinsics
struct Int128 {
    #ifdef __SIZEOF_INT128__
        using value_type = __int128;           // GCC/Clang native
        using unsigned_type = unsigned __int128;
    #else
        // Fallback for MSVC and others
        struct value_type {
            uint64_t low;
            int64_t high;
        };
        struct unsigned_type {
            uint64_t low;
            uint64_t high;
        };
    #endif
    
    value_type value;
    
    // Arithmetic operations
    Int128 operator+(const Int128& other) const;
    Int128 operator-(const Int128& other) const;
    Int128 operator*(const Int128& other) const;
    Int128 operator/(const Int128& other) const;
    Int128 operator%(const Int128& other) const;
    
    // Conversions
    static Int128 from_string(const string& str);
    string to_string() const;
    
    // Overflow checking
    bool add_overflow(const Int128& other, Int128& result);
    bool mul_overflow(const Int128& other, Int128& result);
};

// Unsigned variant
struct UInt128 {
    unsigned_type value;
    // Similar operations...
};
```

## SQL Usage

### Basic Operations

```sql
-- Create table with 128-bit integers
CREATE TABLE huge_numbers (
    id INT128 PRIMARY KEY,
    unsigned_val UINT128,
    signed_val INT128
);

-- Insert large values
INSERT INTO huge_numbers VALUES (
    123456789012345678901234567890,  -- INT128
    340282366920938463463374607431768211455,  -- UINT128 max
    -170141183460469231731687303715884105728  -- INT128 min
);

-- Arithmetic operations
SELECT 
    signed_val * 2 as doubled,
    unsigned_val / 1000000 as millions,
    signed_val + unsigned_val as mixed  -- Promotes to UINT128 or DECIMAL
FROM huge_numbers;
```

### Type Promotion Rules

```sql
-- Automatic type promotion
INT8 → INT16 → INT32 → INT64 → INT128 → DECIMAL
UINT8 → UINT16 → UINT32 → UINT64 → UINT128 → DECIMAL

-- Mixed operations
INT128 + UINT128 → DECIMAL(40,0)  -- To handle full range
INT128 * INT128 → INT128 (with overflow check)
INT128 / INT128 → DECIMAL(39,19)  -- Preserve precision
```

### Aggregate Functions

```sql
-- Aggregates with 128-bit integers
CREATE TABLE transactions (
    id INTEGER,
    amount INT128  -- For very large amounts
);

-- Sum can exceed INT128 range
SELECT 
    SUM(amount) as total,  -- Returns DECIMAL if overflow possible
    AVG(amount) as average,  -- Returns DECIMAL
    MIN(amount) as minimum,  -- Returns INT128
    MAX(amount) as maximum   -- Returns INT128
FROM transactions;

-- Prevent overflow with CAST
SELECT CAST(SUM(amount) AS DECIMAL(50,0)) as safe_total
FROM transactions;
```

## Use Cases

### 1. Financial Calculations

```sql
-- Cryptocurrency values (wei, satoshi, etc.)
CREATE TABLE crypto_balances (
    wallet_address VARCHAR(100),
    balance_wei UINT128,  -- Ethereum wei (10^-18 ETH)
    balance_satoshi UINT128  -- Bitcoin satoshi (10^-8 BTC)
);

-- Global financial calculations
CREATE TABLE global_economics (
    country VARCHAR(100),
    gdp_cents INT128,  -- GDP in cents to avoid decimals
    debt_cents INT128
);
```

### 2. Scientific Computing

```sql
-- Astronomical calculations
CREATE TABLE stellar_distances (
    star_id INTEGER,
    distance_nanometers UINT128,  -- Distance in nanometers
    mass_grams UINT128  -- Mass in grams
);

-- Particle physics
CREATE TABLE particle_counts (
    experiment_id INTEGER,
    particle_count UINT128,
    energy_electron_volts INT128
);
```

### 3. Unique Identifiers

```sql
-- UUID alternative with more bits
CREATE TABLE distributed_ids (
    id UINT128 PRIMARY KEY,  -- 128-bit unique identifier
    created_at TIMESTAMP
);

-- Generate 128-bit ID from multiple sources
CREATE FUNCTION generate_id_128() RETURNS UINT128
AS
BEGIN
    RETURN (CAST(EXTRACT(EPOCH FROM CURRENT_TIMESTAMP) AS UINT64) << 64) |
           (CAST(gen_random_uuid() AS UINT64));
END;
```

## Compatibility and Mapping

| Database | Native 128-bit | Workaround |
|----------|---------------|------------|
| Firebird | INT128 | Native |
| PostgreSQL | - | NUMERIC(39,0) |
| MySQL | - | DECIMAL(39,0) |
| MSSQL | - | DECIMAL(38,0)* |
| ScratchBird | INT128/UINT128 | Native |

*MSSQL DECIMAL limited to 38 digits

### Cross-Database Compatibility

```sql
-- ScratchBird handles conversion
CREATE TABLE cross_db_compat (
    -- Stored as INT128 internally
    big_value INT128
);

-- When MySQL client connects:
-- Presented as DECIMAL(39,0)

-- When PostgreSQL client connects:
-- Presented as NUMERIC(39,0)

-- When Firebird client connects:
-- Presented as INT128 (native)
```

## Performance Optimizations

### SIMD Operations

```cpp
// Vectorized operations on INT128 arrays
class Int128Vector {
    void add_arrays(Int128* a, Int128* b, Int128* result, size_t count) {
        #ifdef __AVX512F__
        // Use AVX-512 for parallel 128-bit operations
        for (size_t i = 0; i < count; i += 4) {
            __m512i va = _mm512_loadu_si512(&a[i]);
            __m512i vb = _mm512_loadu_si512(&b[i]);
            __m512i vr = _mm512_add_epi64(va, vb);
            _mm512_storeu_si512(&result[i], vr);
        }
        #else
        // Fallback to scalar
        for (size_t i = 0; i < count; i++) {
            result[i] = a[i] + b[i];
        }
        #endif
    }
};
```

### Storage Optimization

```cpp
// Compressed storage for sparse INT128
class CompressedInt128 {
    enum StorageType {
        ZERO,     // 0 bytes (value is 0)
        INT8,     // 1 byte
        INT16,    // 2 bytes
        INT32,    // 4 bytes
        INT64,    // 8 bytes
        INT128    // 16 bytes
    };
    
    StorageType get_min_storage(Int128 value) {
        if (value == 0) return ZERO;
        if (value >= INT8_MIN && value <= INT8_MAX) return INT8;
        if (value >= INT16_MIN && value <= INT16_MAX) return INT16;
        if (value >= INT32_MIN && value <= INT32_MAX) return INT32;
        if (value >= INT64_MIN && value <= INT64_MAX) return INT64;
        return INT128;
    }
};
```

## Functions and Operators

### Mathematical Functions

```sql
-- Built-in functions for INT128
SELECT 
    ABS(int128_col),
    SIGN(int128_col),
    MOD(int128_col, 1000000),
    POWER(CAST(int128_col AS DECIMAL), 0.5),  -- Square root needs DECIMAL
    GREATEST(int128_col1, int128_col2),
    LEAST(int128_col1, int128_col2)
FROM huge_table;

-- Bitwise operations on UINT128
SELECT
    uint128_col & 0xFFFFFFFFFFFFFFFF as low_64_bits,
    uint128_col >> 64 as high_64_bits,
    uint128_col | (1 << 127) as set_high_bit,
    POPCOUNT(uint128_col) as bit_count
FROM huge_table;
```

### Conversion Functions

```sql
-- Conversion functions
CREATE FUNCTION int128_to_hex(val INT128) RETURNS VARCHAR(32)
AS BEGIN
    RETURN TO_HEX(val);
END;

CREATE FUNCTION hex_to_int128(hex VARCHAR(32)) RETURNS INT128
AS BEGIN
    RETURN FROM_HEX(hex);
END;

-- Base conversion
CREATE FUNCTION int128_to_base(val INT128, base INTEGER) RETURNS VARCHAR(130)
AS BEGIN
    -- Convert INT128 to any base (2-36)
    RETURN TO_BASE(val, base);
END;
```

## Indexing 128-bit Values

```sql
-- B-tree index on INT128
CREATE INDEX idx_huge ON huge_numbers(signed_val);

-- Hash index for equality searches
CREATE INDEX idx_huge_hash ON huge_numbers USING HASH (unsigned_val);

-- Range partitioning on INT128
CREATE TABLE partitioned_huge (
    id INT128,
    data TEXT
) PARTITION BY RANGE (id);

CREATE TABLE partitioned_huge_p1 PARTITION OF partitioned_huge
    FOR VALUES FROM (MINVALUE) TO (0);
    
CREATE TABLE partitioned_huge_p2 PARTITION OF partitioned_huge
    FOR VALUES FROM (0) TO (85070591730234615865843651857942052864);
    
CREATE TABLE partitioned_huge_p3 PARTITION OF partitioned_huge
    FOR VALUES FROM (85070591730234615865843651857942052864) TO (MAXVALUE);
```

## Configuration

```sql
-- System configuration
ALTER SYSTEM SET int128_arithmetic_mode = 'checked';  -- checked, unchecked, saturating
ALTER SYSTEM SET int128_display_format = 'decimal';   -- decimal, hex, scientific

-- Session configuration
SET int128_overflow_behavior = 'error';  -- error, wraparound, saturate
SET int128_division_by_zero = 'error';   -- error, null, infinity
```

## Benefits

1. **No Precision Loss**: Financial calculations without DECIMAL overhead
2. **Performance**: Native CPU operations vs arbitrary precision
3. **Compatibility**: Matches Firebird INT128, exceeds others
4. **Scientific Computing**: Handle massive numbers efficiently
5. **Unique IDs**: Larger space than UUID
6. **Blockchain**: Native support for crypto calculations