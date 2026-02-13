# Appendix: Firebird 5 Built-in Function Semantics (Authoritative)

This appendix defines the semantics for all built-in scalar/system functions listed in `appendix_builtins_sysfunctions.md`. Each function either has a specific definition below or inherits from a category definition.

## 1. General Rules

- Unless explicitly stated otherwise, if any argument is `NULL`, the result is `NULL`.
- Argument evaluation order is left-to-right.
- For variable-argument functions, arguments are evaluated left-to-right and combined in order.
- All string comparisons and transformations are performed using the character set and collation of the input string unless explicitly stated otherwise.
- For functions that accept “any type” as input and require a string, Firebird converts the input to a string using its default conversion rules for the session.

## 2. Type Resolution Rules

### 2.1 Double-only functions (`setParamsDouble`)
- Arguments are converted to `DOUBLE` if they are of unknown type.
- Result type: `DOUBLE`.
- Applies to: `ACOS`, `ACOSH`, `ASIN`, `ASINH`, `ATAN`, `ATANH`, `ATAN2`, `COS`, `COSH`, `COT`, `SIN`, `SINH`, `TAN`, `TANH`.

### 2.2 Double-or-DECFLOAT functions (`setParamsDblDec`)
- If any argument is approximate numeric (FLOAT/DOUBLE) or no argument is DECFLOAT/INT128, arguments are treated as `DOUBLE`.
- If any argument is `DECFLOAT` or `INT128` and none are approximate numeric, arguments are treated as `DECFLOAT(34)`.
- Result type:
  - `DOUBLE` for the DOUBLE branch
  - `DECFLOAT(34)` for the DECFLOAT branch
- Applies to: `ABS`, `CEIL`, `CEILING`, `EXP`, `FLOOR`, `LN`, `LOG`, `LOG10`, `POWER`, `SIGN`, `SQRT`.

### 2.3 DECFLOAT-only functions (`setParamsDecFloat`)
- If any argument is `DECFLOAT(16)` and none are `DECFLOAT(34)`, use `DECFLOAT(16)`.
- Otherwise use `DECFLOAT(34)`.
- Result type: same DECFLOAT size as chosen above.
- Applies to: `COMPARE_DECFLOAT`, `NORMALIZE_DECFLOAT`, `QUANTIZE`, `TOTALORDER`.

### 2.4 Integer-only functions (`setParamsInteger`)
- Arguments are converted to 32-bit integer if unknown.
- Result type depends on the function.
- Applies to: `ASCII_CHAR`, `BIN_SHL`, `BIN_SHR`, `BIN_SHL_ROT`, `BIN_SHR_ROT`, `RSA_PRIVATE`, `UNICODE_CHAR`.

### 2.5 Binary integer functions (`setParamsBin`)
- Result type is the widest exact integer type among arguments: `SMALLINT` < `INTEGER` < `BIGINT` < `INT128`.
- If any argument is unknown, it is coerced to that widest type.
- Applies to: `BIN_AND`, `BIN_NOT`, `BIN_OR`, `BIN_XOR`.

### 2.6 Int64 functions (`setParamsInt64`)
- Arguments are converted to `BIGINT` if unknown.
- Applies to: `RDB$GET_TRANSACTION_CN`.

### 2.7 From-list functions (`setParamsFromList`)
The common result type is determined by the following explicit rules:

1. If any argument is a `BLOB`, result is `BLOB`. If any `BLOB` is binary, result is binary `BLOB`.
2. If any argument is `VARCHAR` or a text type mixed with non-text, result is `VARCHAR`.
3. If any argument is `CHAR`/`CSTRING` and no `VARCHAR` is required, result is `CHAR`.
4. If any argument is approximate numeric, all arguments must be numeric; result is approximate numeric (type with maximum precision and scale).
5. If all arguments are exact numeric, result is exact numeric with maximum precision and scale.
6. If any argument is date/time/timestamp, all must be date/time/timestamp or compatible time-zone variants. Mixed date/time categories are rejected.
7. If any argument is `BOOLEAN`, all must be `BOOLEAN`.
8. If types are not comparable, raise `isc_dsql_datatypes_not_comparable`.

Applies to: `GREATEST`, `LEAST`, `MAXVALUE`, `MINVALUE`, `MOD`, `REPLACE` (for type resolution only).

### 2.8 Special parameter setters
- `setParamsAsciiVal`: argument is `CHAR(1)` in ASCII; result `SMALLINT`.
- `setParamsUnicodeVal`: argument is `CHAR(4)` UTF-8; result `INTEGER`.
- `setParamsCharToUuid`: argument is `CHAR(36)` ASCII; result `UUID`.
- `setParamsDateAdd`: argument 1 is `BIGINT` (or `BIGINT` with scale for millisecond extraction), argument 3 default `TIMESTAMP`.
- `setParamsDateDiff`: argument 2 and 3 share the same date/time/timestamp type; default `TIMESTAMP` when both unknown.
- `setParamsSecondInteger`: argument 2 is `INTEGER` when unknown (used by `LEFT`, `RIGHT`, `LPAD`, `RPAD`).
- `setParamsBlobAppend`: argument 1 default is `BLOB SUB_TYPE TEXT` in connection charset.

## 3. Function Semantics

### 3.1 Math Functions (Double)
All functions in this section accept numeric input, convert to `DOUBLE`, and return `DOUBLE`.

- `ACOS(x)`: arccosine of `x` (radians). Domain: `-1 <= x <= 1`.
- `ACOSH(x)`: inverse hyperbolic cosine. Domain: `x >= 1`.
- `ASIN(x)`: arcsine of `x` (radians). Domain: `-1 <= x <= 1`.
- `ASINH(x)`: inverse hyperbolic sine.
- `ATAN(x)`: arctangent of `x` (radians).
- `ATANH(x)`: inverse hyperbolic tangent. Domain: `-1 < x < 1`.
- `ATAN2(y, x)`: arctangent of `y/x` with quadrant determined by signs.
- `COS(x)`: cosine of `x` (radians).
- `COSH(x)`: hyperbolic cosine.
- `COT(x)`: cotangent = `1 / tan(x)`.
- `SIN(x)`: sine of `x` (radians).
- `SINH(x)`: hyperbolic sine.
- `TAN(x)`: tangent of `x` (radians).
- `TANH(x)`: hyperbolic tangent.

Domain violations raise `isc_arith_except`.

### 3.2 Math Functions (Double or DECFLOAT)
All functions in this section follow the rules in 2.2.

- `ABS(x)`: absolute value of `x`.
- `CEIL(x)`, `CEILING(x)`: smallest integer >= `x`.
- `EXP(x)`: `e^x`.
- `FLOOR(x)`: largest integer <= `x`.
- `LN(x)`: natural logarithm. Domain: `x > 0`.
- `LOG(b, x)`: logarithm of `x` with base `b`. Domain: `b > 0`, `b != 1`, `x > 0`.
- `LOG10(x)`: base-10 logarithm. Domain: `x > 0`.
- `POWER(x, y)`: `x^y`.
- `SIGN(x)`: returns `-1`, `0`, or `1` based on sign of `x`. Result type `SMALLINT`.
- `SQRT(x)`: square root. Domain: `x >= 0`.

### 3.3 DECFLOAT Functions
- `COMPARE_DECFLOAT(a, b)`: returns `-1`, `0`, or `1` comparing DECFLOAT values with IEEE rules.
- `NORMALIZE_DECFLOAT(x)`: normalizes representation (removes trailing zeros; preserves numerical value).
- `QUANTIZE(a, b)`: quantizes `a` to the exponent of `b`.
- `TOTALORDER(a, b)`: total ordering of DECFLOAT values including NaNs.

### 3.4 Bitwise Functions
- `BIN_AND(a, b, ...)`: bitwise AND of arguments.
- `BIN_OR(a, b, ...)`: bitwise OR of arguments.
- `BIN_XOR(a, b, ...)`: bitwise XOR of arguments.
- `BIN_NOT(x)`: bitwise NOT of argument.
- `BIN_SHL(x, n)`: shift left `x` by `n` bits.
- `BIN_SHR(x, n)`: shift right `x` by `n` bits.
- `BIN_SHL_ROT(x, n)`: rotate left `x` by `n` bits.
- `BIN_SHR_ROT(x, n)`: rotate right `x` by `n` bits.

Shift counts are masked to the width of the integer type. Negative shifts raise `isc_arith_except`.

### 3.5 String/Unicode Functions
- `ASCII_CHAR(n)`: returns 1‑byte ASCII character with code `n` (0–127). Out of range raises `isc_arith_except`.
- `ASCII_VAL(s)`: returns ASCII code of first character of `s` (after conversion to ASCII). Result `SMALLINT`.
- `UNICODE_CHAR(n)`: returns UTF‑8 character for Unicode codepoint `n`. Out of range raises `isc_arith_except`.
- `UNICODE_VAL(s)`: returns Unicode codepoint of first character of `s` (UTF‑8). Result `INTEGER`.
- `LEFT(s, n)`: returns leftmost `n` characters of `s`.
- `RIGHT(s, n)`: returns rightmost `n` characters of `s`.
- `LPAD(s, len, pad)`: pads `s` on the left to length `len` using `pad`. If `pad` omitted, uses space.
- `RPAD(s, len, pad)`: pads `s` on the right to length `len` using `pad`. If `pad` omitted, uses space.
- `POSITION(sub IN s [, start])`: 1‑based position of `sub` in `s`, starting at `start` (default 1). Returns 0 if not found.
- `OVERLAY(s PLACING repl FROM start [FOR length])`: replaces substring of `s` at `start` with `repl`, optionally for `length` characters.
- `REPLACE(s, find, repl)`: replaces all occurrences of `find` in `s` with `repl`.
- `REVERSE(s)`: returns `s` reversed by character.

### 3.6 Date/Time Functions
- `DATEADD(part, amount, value)`: adds `amount` of `part` to `value`. Result type matches `value`.
- `DATEDIFF(part, value1, value2)`: returns integer count of `part` boundaries crossed between `value1` and `value2`.
- `FIRST_DAY(unit, value)`: returns first day of the specified unit (month/year/quarter/week) containing `value`.
- `LAST_DAY(unit, value)`: returns last day of the specified unit (month/year/quarter/week) containing `value`.

`part` follows Firebird `EXTRACT` units. `DATEDIFF` result is `BIGINT`.

### 3.7 UUID Functions
- `GEN_UUID([version])`: generates a UUID. If `version` omitted, engine default version is used.
- `CHAR_TO_UUID(s)`: converts a 36‑character UUID string to binary UUID.
- `UUID_TO_CHAR(uuid)`: converts UUID to canonical 36‑character string (lowercase hex with hyphens).

### 3.8 Hash and Encoding Functions
- `BASE64_ENCODE(x)`: encodes binary/string input to Base64 string.
- `BASE64_DECODE(x)`: decodes Base64 string to binary; invalid input raises error.
- `HEX_ENCODE(x)`: encodes binary/string input to hex string.
- `HEX_DECODE(x)`: decodes hex string to binary. Type length must be even; odd length raises error.
- `CRYPT_HASH(value USING algorithm)`: returns `VARBINARY` cryptographic hash. Algorithms: `MD5`, `SHA1`, `SHA256`, `SHA512`.
- `HASH(value [USING CRC32])`: returns non-cryptographic hash. Default is 64‑bit PJW (ELF64) → `BIGINT`; `CRC32` returns `INTEGER`.

### 3.9 BLOB_APPEND
- `BLOB_APPEND(a, b, ...)`: concatenates values into a temporary `BLOB` with `BLB_close_on_read`.
- If first argument is `NULL`, a new empty text `BLOB` is created using connection charset.
- If first argument is an existing permanent `BLOB`, a new `BLOB` is created with same subtype/charset and populated with its content.
- If first argument is a temporary unclosed `BLOB` with `BLB_close_on_read`, it is reused and appended to.
- If first argument is non‑BLOB, a new text `BLOB` is created and populated with the string representation.
- For subsequent arguments:
  - `NULL` is treated as empty string.
  - `BLOB`s are converted to the target charset and appended.
  - non‑BLOBs are converted to string and appended.
- The result is an open temporary `BLOB`; it is closed automatically when read or assigned to a field.

### 3.10 Cryptographic Functions
- `ENCRYPT(input USING alg [MODE mode] KEY key [IV iv] [CTR_TYPE] [CTR_LENGTH n] [COUNTER c])`:
  - Encrypts input using specified symmetric algorithm.
  - Returns `BLOB SUB_TYPE BINARY` if input is `BLOB`, otherwise `VARBINARY`.
  - Block cipher modes require input length multiple of block size; padding must be done by caller.
  - Algorithms: AES, ANUBIS, BLOWFISH, KHAZAD, RC5, RC6, SAFER+, TWOFISH, XTEA, CHACHA20, RC4, SOBER128.
- `DECRYPT(encrypted_input USING alg [MODE mode] KEY key [IV iv] [CTR_TYPE] [CTR_LENGTH n] [COUNTER c])`:
  - Decrypts data using symmetric algorithm; output type matches ENCRYPT rules.

- `RSA_PRIVATE(key_length)`:
  - Generates RSA private key in PKCS#1 format. `key_length` in bytes (4–1024).
- `RSA_PUBLIC(private_key)`:
  - Generates RSA public key from private key (PKCS#1).
- `RSA_ENCRYPT(input KEY public_key [LPARAM tag] [HASH h] [PKCS_1_5])`:
  - Pads input using OAEP (default) or PKCS#1.5 and encrypts with public key. Returns `VARBINARY`.
- `RSA_DECRYPT(encrypted_input KEY private_key [LPARAM tag] [HASH h] [PKCS_1_5])`:
  - Decrypts with private key and removes padding. Returns `VARBINARY`.
- `RSA_SIGN_HASH(value KEY private_key [HASH h])`:
  - Signs hash of value using private key; returns `VARBINARY` signature.
- `RSA_VERIFY_HASH(value KEY public_key signature [HASH h])`:
  - Verifies signature; returns `BOOLEAN`.

### 3.11 Context and Security Functions
- `RDB$GET_CONTEXT(namespace, name)`: returns context variable value or `NULL`.
- `RDB$SET_CONTEXT(namespace, name, value)`: sets context variable; returns previous value or `NULL`.
- `RDB$GET_TRANSACTION_CN(xid)`: returns commit number for transaction id.
- `RDB$ROLE_IN_USE(role_name)`: returns `BOOLEAN` if role is active.
- `RDB$SYSTEM_PRIVILEGE(name)`: returns `BOOLEAN` if privilege exists.

## 4. Null Handling Exceptions
- `BLOB_APPEND` treats `NULL` as empty.
- `GEN_UUID` and `RAND` have no arguments; never return `NULL`.

