<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# String Functions

[Categories README](./README.md) | [Operations Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

## Synopsis

String functions manipulate and query character data.

## Concatenation

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `\|\|` | Concatenation operator | `'Hello' \|\| ' ' \|\| 'World'` | `Hello World` |
| `CONCAT(str1, str2, ...)` | Concatenate strings | `CONCAT('a', 'b', 'c')` | `abc` |
| `CONCAT_WS(sep, str1, ...)` | Concatenate with separator | `CONCAT_WS(',', 'a', 'b')` | `a,b` |

## Case Conversion

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `LOWER(str)` / `LCASE(str)` | Lowercase | `LOWER('HELLO')` | `hello` |
| `UPPER(str)` / `UCASE(str)` | Uppercase | `UPPER('hello')` | `HELLO` |
| `INITCAP(str)` | Title case | `INITCAP('hello world')` | `Hello World` |

## Length and Position

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `LENGTH(str)` / `CHAR_LENGTH(str)` | String length | `LENGTH('hello')` | `5` |
| `OCTET_LENGTH(str)` | Byte length | `OCTET_LENGTH('hello')` | `5` |
| `POSITION(substr IN str)` | Find substring | `POSITION('ll' IN 'hello')` | `3` |
| `STRPOS(str, substr)` | Find substring | `STRPOS('hello', 'll')` | `3` |

## Substring Extraction

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `SUBSTRING(str FROM start [FOR len])` | Extract substring | `SUBSTRING('hello' FROM 2 FOR 3)` | `ell` |
| `SUBSTR(str, start [, len])` | Extract substring | `SUBSTR('hello', 2, 3)` | `ell` |
| `LEFT(str, n)` | Left n characters | `LEFT('hello', 3)` | `hel` |
| `RIGHT(str, n)` | Right n characters | `RIGHT('hello', 3)` | `llo` |

## Trimming

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `TRIM([LEADING/TRAILING/BOTH] [chars] FROM str)` | Remove characters | `TRIM('  hello  ')` | `hello` |
| `LTRIM(str [, chars])` | Trim left | `LTRIM('  hello')` | `hello` |
| `RTRIM(str [, chars])` | Trim right | `RTRIM('hello  ')` | `hello` |
| `BTRIM(str [, chars])` | Trim both ends | `BTRIM('xxhelloxx', 'x')` | `hello` |

## Pattern Matching

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `LIKE` | Pattern match | `'hello' LIKE 'h%'` | `true` |
| `ILIKE` | Case-insensitive LIKE | `'Hello' ILIKE 'h%'` | `true` |
| `SIMILAR TO` | Regex pattern | `'hello' SIMILAR TO 'h[a-z]+'` | `true` |
| `~` | Regex match | `'hello' ~ '^h.*o$'` | `true` |
| `~*` | Case-insensitive regex | `'Hello' ~* '^h.*o$'` | `true` |
| `!~` | Not regex match | `'hello' !~ '^x'` | `true` |

## Replacement

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `REPLACE(str, from, to)` | Replace all | `REPLACE('hello', 'l', 'x')` | `hexxo` |
| `REGEXP_REPLACE(str, pat, repl [, flags])` | Regex replace | `REGEXP_REPLACE('hello', 'l+', 'L')` | `heLo` |
| `TRANSLATE(str, from, to)` | Character translation | `TRANSLATE('hello', 'lo', '01')` | `he001` |

## Padding

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `LPAD(str, len [, fill])` | Pad left | `LPAD('hi', 5, 'x')` | `xxxhi` |
| `RPAD(str, len [, fill])` | Pad right | `RPAD('hi', 5, 'x')` | `hixxx` |

## Splitting

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `SPLIT_PART(str, delim, field)` | Extract field | `SPLIT_PART('a,b,c', ',', 2)` | `b` |
| `STRING_TO_ARRAY(str, delim)` | Split to array | `STRING_TO_ARRAY('a,b,c', ',')` | `{a,b,c}` |
| `REGEXP_SPLIT_TO_ARRAY(str, pat)` | Regex split | `REGEXP_SPLIT_TO_ARRAY('a b c', '\s+')` | `{a,b,c}` |
| `REGEXP_SPLIT_TO_TABLE(str, pat)` | Regex split to rows | - | setof text |

## Formatting

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `FORMAT(fmt, ...)` | Format string | `FORMAT('Hello %s', 'World')` | `Hello World` |
| `TO_CHAR(val, fmt)` | Convert to string | `TO_CHAR(1234, '9999')` | ` 1234` |

## Encoding

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `ENCODE(data, type)` | Encode binary | `ENCODE('hello', 'base64')` | `aGVsbG8=` |
| `DECODE(str, type)` | Decode string | `DECODE('aGVsbG8=', 'base64')` | `hello` |
| `CONVERT(str, src_enc, dest_enc)` | Convert encoding | - | - |
| `CONVERT_FROM(str, enc)` | Decode bytes | - | - |
| `CONVERT_TO(str, enc)` | Encode to bytes | - | - |

## Examples

```sql
-- Format a name
SELECT INITCAP(LOWER('JOHN DOE'));  -- 'John Doe'

-- Extract domain from email
SELECT SUBSTRING(email FROM POSITION('@' IN email) + 1) FROM users;

-- Format a message
SELECT FORMAT('User %s has %s orders', name, order_count) FROM users;

-- Clean up input
SELECT TRIM(BOTH FROM LOWER(email)) FROM signups;

-- Parse CSV-like data
SELECT SPLIT_PART(data, ',', 1) as name,
       SPLIT_PART(data, ',', 2) as value
FROM raw_data;
```

## See Also

- [Full-Text Search Functions](../../../syntax_guide/dml/01_select_core_syntax.md)
- [Pattern Matching Operators](../../../operators_and_expressions/README.md)
