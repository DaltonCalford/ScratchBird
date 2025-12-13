# String Functions

Text manipulation functions.

[Back to Functions Index](index.md) | [Back to Language Guide](../index.md)

---

## Case Conversion

| Function | Description | Example |
|----------|-------------|---------|
| `UPPER(s)` | Uppercase | `UPPER('hello')` → `'HELLO'` |
| `LOWER(s)` | Lowercase | `LOWER('HELLO')` → `'hello'` |
| `INITCAP(s)` | Title case | `INITCAP('hello world')` → `'Hello World'` |

---

## Length and Position

| Function | Description | Example |
|----------|-------------|---------|
| `LENGTH(s)` | Character count | `LENGTH('hello')` → `5` |
| `CHAR_LENGTH(s)` | Same as LENGTH | `CHAR_LENGTH('hello')` → `5` |
| `OCTET_LENGTH(s)` | Byte count | `OCTET_LENGTH('hello')` → `5` |
| `POSITION(sub IN s)` | Find position | `POSITION('l' IN 'hello')` → `3` |
| `STRPOS(s, sub)` | Find position | `STRPOS('hello', 'l')` → `3` |

---

## Substring

| Function | Description | Example |
|----------|-------------|---------|
| `SUBSTRING(s, start, len)` | Extract substring | `SUBSTRING('hello', 2, 3)` → `'ell'` |
| `SUBSTR(s, start, len)` | Same as SUBSTRING | `SUBSTR('hello', 2, 3)` → `'ell'` |
| `LEFT(s, n)` | First n chars | `LEFT('hello', 3)` → `'hel'` |
| `RIGHT(s, n)` | Last n chars | `RIGHT('hello', 3)` → `'llo'` |

---

## Concatenation

| Function | Description | Example |
|----------|-------------|---------|
| `s1 \|\| s2` | Concatenate | `'hello' \|\| ' world'` → `'hello world'` |
| `CONCAT(s1, s2, ...)` | Concatenate | `CONCAT('a', 'b', 'c')` → `'abc'` |
| `CONCAT_WS(sep, s1, ...)` | With separator | `CONCAT_WS('-', 'a', 'b')` → `'a-b'` |

---

## Trimming

| Function | Description | Example |
|----------|-------------|---------|
| `TRIM(s)` | Remove spaces | `TRIM('  hi  ')` → `'hi'` |
| `LTRIM(s)` | Left trim | `LTRIM('  hi')` → `'hi'` |
| `RTRIM(s)` | Right trim | `RTRIM('hi  ')` → `'hi'` |
| `TRIM(chars FROM s)` | Remove chars | `TRIM('x' FROM 'xxhixx')` → `'hi'` |

---

## Padding

| Function | Description | Example |
|----------|-------------|---------|
| `LPAD(s, len, fill)` | Left pad | `LPAD('42', 5, '0')` → `'00042'` |
| `RPAD(s, len, fill)` | Right pad | `RPAD('hi', 5, '.')` → `'hi...'` |

---

## Replacement

| Function | Description | Example |
|----------|-------------|---------|
| `REPLACE(s, from, to)` | Replace all | `REPLACE('hello', 'l', 'L')` → `'heLLo'` |
| `TRANSLATE(s, from, to)` | Character map | `TRANSLATE('hello', 'el', 'ip')` → `'hippo'` |
| `OVERLAY(s PLACING r FROM start)` | Insert | `OVERLAY('hello' PLACING 'XX' FROM 3)` → `'heXXo'` |

---

## Splitting and Joining

| Function | Description | Example |
|----------|-------------|---------|
| `SPLIT_PART(s, delim, n)` | Get part | `SPLIT_PART('a,b,c', ',', 2)` → `'b'` |
| `STRING_TO_ARRAY(s, delim)` | Split to array | `STRING_TO_ARRAY('a,b,c', ',')` → `{'a','b','c'}` |
| `ARRAY_TO_STRING(a, delim)` | Join array | `ARRAY_TO_STRING(ARRAY['a','b'], ',')` → `'a,b'` |

---

## Pattern Matching

| Function | Description | Example |
|----------|-------------|---------|
| `s LIKE pattern` | Pattern match | `'hello' LIKE 'h%'` → `TRUE` |
| `s ILIKE pattern` | Case-insensitive | `'Hello' ILIKE 'h%'` → `TRUE` |
| `s ~ pattern` | Regex match | `'hello' ~ '^h'` → `TRUE` |
| `s ~* pattern` | Regex case-insensitive | `'Hello' ~* '^h'` → `TRUE` |

---

## Regular Expressions

| Function | Description | Example |
|----------|-------------|---------|
| `REGEXP_REPLACE(s, pat, repl)` | Regex replace | `REGEXP_REPLACE('hello', 'l+', 'L')` → `'heLo'` |
| `REGEXP_MATCHES(s, pat)` | Extract matches | `REGEXP_MATCHES('abc123', '(\d+)')` → `{'123'}` |
| `REGEXP_SPLIT_TO_ARRAY(s, pat)` | Split by regex | `REGEXP_SPLIT_TO_ARRAY('a1b2c', '\d')` → `{'a','b','c'}` |
| `REGEXP_SPLIT_TO_TABLE(s, pat)` | Split to rows | Returns multiple rows |

---

## Encoding

| Function | Description | Example |
|----------|-------------|---------|
| `ASCII(s)` | ASCII code | `ASCII('A')` → `65` |
| `CHR(n)` | Char from code | `CHR(65)` → `'A'` |
| `ENCODE(data, format)` | Encode binary | `ENCODE('hi', 'base64')` |
| `DECODE(s, format)` | Decode to binary | `DECODE('aGk=', 'base64')` |

---

## Formatting

| Function | Description | Example |
|----------|-------------|---------|
| `FORMAT(fmt, args...)` | Printf-style | `FORMAT('Hello %s', 'World')` → `'Hello World'` |
| `QUOTE_LITERAL(s)` | Add quotes | `QUOTE_LITERAL('hi')` → `'\'hi\''` |
| `QUOTE_IDENT(s)` | Quote identifier | `QUOTE_IDENT('table')` → `'"table"'` |

---

## Other

| Function | Description | Example |
|----------|-------------|---------|
| `REPEAT(s, n)` | Repeat string | `REPEAT('ab', 3)` → `'ababab'` |
| `REVERSE(s)` | Reverse string | `REVERSE('hello')` → `'olleh'` |
| `MD5(s)` | MD5 hash | `MD5('hello')` → `'5d41402abc...'` |
| `COALESCE(s1, s2, ...)` | First non-null | `COALESCE(NULL, 'default')` → `'default'` |

---

## Examples

### Format Name

```sql
SELECT INITCAP(CONCAT(first_name, ' ', last_name)) AS full_name
FROM users;
```

### Clean Phone Number

```sql
SELECT REGEXP_REPLACE(phone, '[^0-9]', '', 'g') AS clean_phone
FROM contacts;
```

### Extract Domain

```sql
SELECT SPLIT_PART(email, '@', 2) AS domain
FROM users;
```

### Mask Email

```sql
SELECT
    CONCAT(
        LEFT(email, 2),
        '***@',
        SPLIT_PART(email, '@', 2)
    ) AS masked_email
FROM users;
```

---

## See Also

- [Data Types - String](../data-types/string-types.md)
- [Pattern Matching](../dml/select.md#pattern-matching)
