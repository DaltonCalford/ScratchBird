# Numeric Functions

[Categories README](./README.md) | [Operations Guide README](../../README.md)

## Synopsis

Mathematical and statistical functions for numeric data.

## Mathematical Functions

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `ABS(x)` | Absolute value | `ABS(-5)` | `5` |
| `CEIL(x)` / `CEILING(x)` | Round up | `CEIL(4.2)` | `5` |
| `FLOOR(x)` | Round down | `FLOOR(4.8)` | `4` |
| `ROUND(x [, d])` | Round to d decimals | `ROUND(4.567, 2)` | `4.57` |
| `TRUNC(x [, d])` | Truncate | `TRUNC(4.567, 2)` | `4.56` |
| `SIGN(x)` | Sign of number | `SIGN(-10)` | `-1` |

## Power and Roots

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `POWER(x, y)` / `POW(x, y)` | x raised to y | `POWER(2, 3)` | `8` |
| `SQRT(x)` | Square root | `SQRT(16)` | `4` |
| `CBRT(x)` | Cube root | `CBRT(27)` | `3` |
| `EXP(x)` | e raised to x | `EXP(1)` | `2.718...` |
| `LN(x)` | Natural logarithm | `LN(10)` | `2.302...` |
| `LOG(x)` / `LOG10(x)` | Base 10 log | `LOG(100)` | `2` |
| `LOG(b, x)` | Log base b | `LOG(2, 8)` | `3` |

## Trigonometric Functions

| Function | Description | Example |
|----------|-------------|---------|
| `PI()` | Pi constant | `3.14159...` |
| `SIN(x)` | Sine (radians) | `SIN(PI()/2)` = `1` |
| `COS(x)` | Cosine | `COS(0)` = `1` |
| `TAN(x)` | Tangent | `TAN(PI()/4)` = `1` |
| `ASIN(x)` | Arcsine | `ASIN(1)` = `PI()/2` |
| `ACOS(x)` | Arccosine | `ACOS(1)` = `0` |
| `ATAN(x)` | Arctangent | `ATAN(1)` = `PI()/4` |
| `ATAN2(y, x)` | Arctangent of y/x | `ATAN2(1, 1)` = `PI()/4` |
| `DEGREES(x)` | Radians to degrees | `DEGREES(PI())` = `180` |
| `RADIANS(x)` | Degrees to radians | `RADIANS(180)` = `PI()` |

## Random Functions

| Function | Description | Example |
|----------|-------------|---------|
| `RANDOM()` | Random 0.0 to 1.0 | `0.742...` |
| `SETSEED(x)` | Set random seed | `SETSEED(0.5)` |

## Aggregate Functions

| Function | Description |
|----------|-------------|
| `SUM(expr)` | Sum of values |
| `AVG(expr)` | Average |
| `COUNT(*)` | Count all rows |
| `COUNT(expr)` | Count non-null |
| `MIN(expr)` | Minimum value |
| `MAX(expr)` | Maximum value |
| `STDDEV(expr)` | Standard deviation |
| `STDDEV_POP(expr)` | Population stddev |
| `VARIANCE(expr)` / `VAR_SAMP(expr)` | Variance |
| `VAR_POP(expr)` | Population variance |

## Examples

```sql
-- Calculate percentage
SELECT (part / total) * 100 AS percentage FROM stats;

-- Round to 2 decimal places
SELECT ROUND(price * 1.1, 2) AS price_with_tax FROM products;

-- Random integer 1-100
SELECT FLOOR(RANDOM() * 100 + 1)::INTEGER;

-- Statistical summary
SELECT 
    AVG(salary) as avg_salary,
    STDDEV(salary) as stddev_salary,
    MIN(salary) as min_salary,
    MAX(salary) as max_salary
FROM employees;
```
