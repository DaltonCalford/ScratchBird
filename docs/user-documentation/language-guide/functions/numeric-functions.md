# Numeric Functions

Mathematical and numeric operations.

[Back to Functions Index](index.md) | [Back to Language Guide](../index.md)

---

## Basic Math

| Function | Description | Example |
|----------|-------------|---------|
| `ABS(n)` | Absolute value | `ABS(-5)` → `5` |
| `SIGN(n)` | Sign (-1, 0, 1) | `SIGN(-5)` → `-1` |
| `CEIL(n)` | Round up | `CEIL(3.2)` → `4` |
| `FLOOR(n)` | Round down | `FLOOR(3.8)` → `3` |
| `ROUND(n)` | Round to nearest | `ROUND(3.5)` → `4` |
| `ROUND(n, d)` | Round to d decimals | `ROUND(3.14159, 2)` → `3.14` |
| `TRUNC(n)` | Truncate to integer | `TRUNC(3.8)` → `3` |
| `TRUNC(n, d)` | Truncate to d decimals | `TRUNC(3.14159, 2)` → `3.14` |

---

## Arithmetic

| Function | Description | Example |
|----------|-------------|---------|
| `a + b` | Addition | `5 + 3` → `8` |
| `a - b` | Subtraction | `5 - 3` → `2` |
| `a * b` | Multiplication | `5 * 3` → `15` |
| `a / b` | Division | `15 / 3` → `5` |
| `a % b` | Modulo | `10 % 3` → `1` |
| `MOD(a, b)` | Modulo | `MOD(10, 3)` → `1` |
| `DIV(a, b)` | Integer division | `DIV(10, 3)` → `3` |
| `a ^ b` | Exponentiation | `2 ^ 3` → `8` |
| `POWER(a, b)` | Exponentiation | `POWER(2, 3)` → `8` |

---

## Square Root and Logarithms

| Function | Description | Example |
|----------|-------------|---------|
| `SQRT(n)` | Square root | `SQRT(16)` → `4` |
| `CBRT(n)` | Cube root | `CBRT(27)` → `3` |
| `EXP(n)` | e^n | `EXP(1)` → `2.718...` |
| `LN(n)` | Natural log | `LN(2.718)` → `1` |
| `LOG(n)` | Base 10 log | `LOG(100)` → `2` |
| `LOG(base, n)` | Custom base log | `LOG(2, 8)` → `3` |

---

## Trigonometry

| Function | Description | Example |
|----------|-------------|---------|
| `SIN(n)` | Sine (radians) | `SIN(0)` → `0` |
| `COS(n)` | Cosine | `COS(0)` → `1` |
| `TAN(n)` | Tangent | `TAN(0)` → `0` |
| `ASIN(n)` | Arc sine | `ASIN(1)` → `1.5707...` |
| `ACOS(n)` | Arc cosine | `ACOS(1)` → `0` |
| `ATAN(n)` | Arc tangent | `ATAN(1)` → `0.7853...` |
| `ATAN2(y, x)` | Arc tangent of y/x | `ATAN2(1, 1)` → `0.7853...` |
| `DEGREES(n)` | Radians to degrees | `DEGREES(PI())` → `180` |
| `RADIANS(n)` | Degrees to radians | `RADIANS(180)` → `3.1415...` |

---

## Constants

| Function | Description | Value |
|----------|-------------|-------|
| `PI()` | Pi | `3.14159265358979` |

---

## Random Numbers

| Function | Description | Example |
|----------|-------------|---------|
| `RANDOM()` | 0.0 to 1.0 | `RANDOM()` → `0.4536...` |
| `SETSEED(n)` | Set random seed | `SETSEED(0.5)` |

### Random Integer in Range

```sql
-- Random 1 to 100
SELECT FLOOR(RANDOM() * 100 + 1)::INTEGER;

-- Random row
SELECT * FROM users ORDER BY RANDOM() LIMIT 1;
```

---

## Comparison

| Function | Description | Example |
|----------|-------------|---------|
| `GREATEST(a, b, ...)` | Largest value | `GREATEST(1, 5, 3)` → `5` |
| `LEAST(a, b, ...)` | Smallest value | `LEAST(1, 5, 3)` → `1` |
| `NULLIF(a, b)` | NULL if a = b | `NULLIF(0, 0)` → `NULL` |
| `COALESCE(a, b, ...)` | First non-null | `COALESCE(NULL, 5)` → `5` |

---

## Bit Operations

| Function | Description | Example |
|----------|-------------|---------|
| `a & b` | Bitwise AND | `12 & 10` → `8` |
| `a \| b` | Bitwise OR | `12 \| 10` → `14` |
| `a # b` | Bitwise XOR | `12 # 10` → `6` |
| `~a` | Bitwise NOT | `~12` → `-13` |
| `a << n` | Left shift | `1 << 4` → `16` |
| `a >> n` | Right shift | `16 >> 2` → `4` |

---

## Conversion

| Function | Description | Example |
|----------|-------------|---------|
| `TO_NUMBER(s, fmt)` | String to number | `TO_NUMBER('1,234.56', '9,999.99')` |
| `n::INTEGER` | Cast to integer | `3.7::INTEGER` → `3` |
| `n::DECIMAL(10,2)` | Cast to decimal | `3::DECIMAL(10,2)` → `3.00` |

---

## Examples

### Calculate Percentage

```sql
SELECT
    category,
    COUNT(*) AS count,
    ROUND(COUNT(*) * 100.0 / SUM(COUNT(*)) OVER (), 2) AS percentage
FROM products
GROUP BY category;
```

### Distance Calculation

```sql
-- Haversine formula for distance between coordinates
SELECT
    ACOS(
        SIN(RADIANS(lat1)) * SIN(RADIANS(lat2)) +
        COS(RADIANS(lat1)) * COS(RADIANS(lat2)) *
        COS(RADIANS(lon2 - lon1))
    ) * 6371 AS distance_km
FROM locations;
```

### Compound Interest

```sql
SELECT
    principal * POWER(1 + rate, years) AS future_value
FROM investments;
```

### Price Rounding

```sql
SELECT
    name,
    price,
    CEIL(price * 0.9 * 100) / 100 AS sale_price  -- Round up to nearest cent
FROM products;
```

### Safe Division

```sql
SELECT
    revenue / NULLIF(units, 0) AS price_per_unit
FROM sales;
-- Returns NULL instead of divide-by-zero error
```

---

## See Also

- [Data Types - Numeric](../data-types/numeric-types.md)
- [Aggregate Functions](aggregate-functions.md)
