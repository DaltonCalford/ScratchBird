# Missing Functions Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created**: November 14, 2025
**Status**: ACTIVE
**Estimated Effort**: 20-30 hours
**Goal**: Complete all missing built-in functions

---

## Overview

Implement the remaining 11 missing functions plus comprehensive bit manipulation:
1. Statistical functions (6 functions)
2. Cryptographic functions (5 functions)
3. XML functions (3-5 functions)
4. Bit manipulation operations (10+ functions)

**Total**: ~26 new functions

---

## 1. Statistical Functions (6 functions) - 4-6 hours

### Standard Aggregate Functions

**STDDEV(expr)** / **STDDEV_SAMP(expr)**
- Sample standard deviation
- Formula: sqrt(variance)
- Requires two-pass or online algorithm

**STDDEV_POP(expr)**
- Population standard deviation
- Formula: sqrt(variance_pop)

**VARIANCE(expr)** / **VAR_SAMP(expr)**
- Sample variance
- Formula: SUM((x - mean)^2) / (n - 1)

**VAR_POP(expr)**
- Population variance
- Formula: SUM((x - mean)^2) / n

**CORR(y, x)**
- Pearson correlation coefficient
- Formula: COVAR_POP(y, x) / (STDDEV_POP(y) * STDDEV_POP(x))

**COVAR_POP(y, x)**
- Population covariance
- Formula: SUM((x - mean_x) * (y - mean_y)) / n

### Implementation Strategy

Use **Welford's online algorithm** for numerically stable variance:
```cpp
struct VarianceAccumulator {
    size_t count = 0;
    double mean = 0.0;
    double M2 = 0.0;  // Sum of squared differences from mean

    void add(double value) {
        count++;
        double delta = value - mean;
        mean += delta / count;
        double delta2 = value - mean;
        M2 += delta * delta2;
    }

    double variance_pop() { return M2 / count; }
    double variance_samp() { return M2 / (count - 1); }
};
```

---

## 2. Cryptographic Functions (5 functions) - 6-8 hours

### Hash Functions

**MD5(data)**
- 128-bit hash (32 hex chars)
- Input: TEXT or BYTEA
- Output: TEXT (hex string)
- Library: OpenSSL or standalone implementation

**SHA1(data)**
- 160-bit hash (40 hex chars)
- Deprecated but still widely used
- Library: OpenSSL

**SHA256(data)**
- 256-bit hash (64 hex chars)
- Preferred for security
- Library: OpenSSL

**SHA512(data)**
- 512-bit hash (128 hex chars)
- Higher security margin
- Library: OpenSSL

### Encoding Functions

**ENCODE(data, format)**
- Encode binary data to text
- Formats: 'base64', 'hex', 'escape'
- Example: ENCODE('hello', 'base64') → 'aGVsbG8='

**DECODE(text, format)**
- Decode text to binary
- Formats: 'base64', 'hex', 'escape'
- Example: DECODE('aGVsbG8=', 'base64') → 'hello'

### Implementation Dependencies

**Option 1**: Use OpenSSL (recommended)
```cpp
#include <openssl/md5.h>
#include <openssl/sha.h>

void executeMD5() {
    Value input = stack_.top(); stack_.pop();
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input.asString().data(), input.asString().size(), hash);
    // Convert to hex string
    stack_.push(Value::makeVarchar(toHexString(hash, MD5_DIGEST_LENGTH)));
}
```

**Option 2**: Standalone implementations (no external deps)
- More code but self-contained
- Already have hex encoding/decoding

---

## 3. XML Functions (3-5 functions) - 8-10 hours

### Core XML Functions

**XMLPARSE(document_or_content, xml_text)**
- Parse XML string to XML type
- Example: XMLPARSE(DOCUMENT '<?xml version="1.0"?><root>data</root>')
- Returns: XML type

**XMLSERIALIZE(content_or_document xml AS type)**
- Serialize XML to TEXT
- Example: XMLSERIALIZE(CONTENT xml_col AS TEXT)
- Returns: TEXT

**XMLELEMENT(name, [attributes], content)**
- Create XML element
- Example: XMLELEMENT(NAME "person", XMLATTRIBUTES('John' AS name), 'Developer')
- Returns: `<person name="John">Developer</person>`

**XMLCONCAT(xml, ...)**
- Concatenate XML values
- Example: XMLCONCAT(xml1, xml2)

**XMLFOREST(expr AS name, ...)**
- Create XML forest (multiple elements)
- Example: XMLFOREST('John' AS name, 30 AS age)
- Returns: `<name>John</name><age>30</age>`

### Optional (Can Defer)

**XMLAGG(xml)** - Aggregate XML values
**XPATH(xpath_expr, xml)** - XPath query
**XMLEXISTS(xpath_expr, xml)** - XPath existence test

### Implementation Strategy

**Option 1**: Use libxml2 (full featured)
```cpp
#include <libxml/parser.h>
#include <libxml/tree.h>
```

**Option 2**: Minimal string-based implementation
- Store XML as TEXT internally
- Basic element creation via string concatenation
- Defer full DOM/SAX parsing to future

**Recommendation**: Start with Option 2 (minimal) to avoid dependency, upgrade to libxml2 if needed.

---

## 4. Bit Manipulation Functions (10+ functions) - 6-8 hours

### Byte Access Functions

**GET_BYTE(bytes, offset)**
- Extract byte at offset
- Example: GET_BYTE('hello', 1) → 101 (ASCII 'e')
- Input: BYTEA or TEXT, INTEGER
- Output: INTEGER (0-255)

**SET_BYTE(bytes, offset, value)**
- Set byte at offset
- Example: SET_BYTE('hello', 0, 72) → 'Hello'
- Input: BYTEA, INTEGER, INTEGER
- Output: BYTEA

**GET_BIT(bytes, offset)**
- Get bit at bit offset
- Example: GET_BIT('\x80', 0) → 1
- Output: INTEGER (0 or 1)

**SET_BIT(bytes, offset, value)**
- Set bit at bit offset
- Example: SET_BIT('\x00', 0, 1) → '\x80'

### Bitwise Operations (INTEGER)

**BIT_AND(a, b)** / **a & b**
- Bitwise AND
- Example: BIT_AND(12, 10) → 8
- Binary: 1100 & 1010 → 1000

**BIT_OR(a, b)** / **a | b**
- Bitwise OR
- Example: BIT_OR(12, 10) → 14
- Binary: 1100 | 1010 → 1110

**BIT_XOR(a, b)** / **a ^ b**
- Bitwise XOR
- Example: BIT_XOR(12, 10) → 6
- Binary: 1100 ^ 1010 → 0110

**BIT_NOT(a)** / **~a**
- Bitwise NOT (complement)
- Example: BIT_NOT(5) → -6 (two's complement)

**BIT_SHIFT_LEFT(a, n)** / **a << n**
- Left shift
- Example: BIT_SHIFT_LEFT(5, 2) → 20
- Binary: 101 << 2 → 10100

**BIT_SHIFT_RIGHT(a, n)** / **a >> n**
- Arithmetic right shift (sign-extending)
- Example: BIT_SHIFT_RIGHT(20, 2) → 5

**BIT_SHIFT_RIGHT_LOGICAL(a, n)** / **a >>> n**
- Logical right shift (zero-fill)
- Example: BIT_SHIFT_RIGHT_LOGICAL(-1, 1) → large positive

### Advanced Bitwise Operations

**BIT_COUNT(a)**
- Count set bits (population count)
- Example: BIT_COUNT(7) → 3
- Binary: 111 has 3 ones

**BIT_LENGTH(bytes)**
- Length in bits
- Example: BIT_LENGTH('hello') → 40

**OCTET_LENGTH(bytes)**
- Length in bytes (already implemented?)
- Example: OCTET_LENGTH('hello') → 5

### Mask Operations

**BIT_MASK(length)**
- Create bit mask of N ones
- Example: BIT_MASK(4) → 15
- Binary: 1111

**APPLY_MASK(value, mask)**
- Apply mask to value
- Example: APPLY_MASK(0xFF, 0x0F) → 0x0F

### Implementation

```cpp
void Executor::executeGetByte() {
    Value offset = stack_.top(); stack_.pop();
    Value bytes = stack_.top(); stack_.pop();

    if (offset.asInt32() < 0 || offset.asInt32() >= bytes.asString().size()) {
        error("Byte offset out of range");
    }

    uint8_t byte = static_cast<uint8_t>(bytes.asString()[offset.asInt32()]);
    stack_.push(Value::makeInt32(byte));
}

void Executor::executeBitAnd() {
    Value b = stack_.top(); stack_.pop();
    Value a = stack_.top(); stack_.pop();
    stack_.push(Value::makeInt64(a.asInt64() & b.asInt64()));
}

void Executor::executeBitShiftLeft() {
    Value n = stack_.top(); stack_.pop();
    Value a = stack_.top(); stack_.pop();
    stack_.push(Value::makeInt64(a.asInt64() << n.asInt64()));
}
```

---

## Implementation Order

### Session 1: Statistical Functions (4-6 hours)
1. Add opcodes (EXT_STDDEV, EXT_VARIANCE, etc.)
2. Implement Welford's algorithm accumulator
3. Add parser support (STDDEV, VARIANCE, CORR, COVAR_POP)
4. Implement executor functions
5. Write tests

### Session 2: Bit Manipulation (6-8 hours)
1. Add opcodes for byte/bit access (EXT_GET_BYTE, EXT_SET_BYTE, etc.)
2. Add opcodes for bitwise ops (EXT_BIT_AND, EXT_BIT_OR, etc.)
3. Implement executor functions
4. Add parser support for operators (&, |, ^, ~, <<, >>)
5. Write comprehensive tests

### Session 3: Cryptographic Functions (6-8 hours)
1. Evaluate OpenSSL vs standalone
2. Add opcodes (EXT_MD5, EXT_SHA256, etc.)
3. Implement hash functions
4. Implement ENCODE/DECODE
5. Add parser support
6. Write tests

### Session 4: XML Functions (8-10 hours)
1. Design minimal XML representation (TEXT-based)
2. Add opcodes (EXT_XMLPARSE, EXT_XMLELEMENT, etc.)
3. Implement basic functions (XMLPARSE, XMLSERIALIZE, XMLELEMENT)
4. Add parser support
5. Write tests
6. Optional: Add XMLCONCAT, XMLFOREST

---

## Opcodes Allocation

### Statistical (0xF3-0xF8)
```cpp
EXT_STDDEV_SAMP = 0xF3,    // STDDEV / STDDEV_SAMP
EXT_STDDEV_POP = 0xF4,     // STDDEV_POP
EXT_VAR_SAMP = 0xF5,       // VARIANCE / VAR_SAMP
EXT_VAR_POP = 0xF6,        // VAR_POP
EXT_CORR = 0xF7,           // CORR(y, x)
EXT_COVAR_POP = 0xF8,      // COVAR_POP(y, x)
```

### Cryptographic (Next available in EXTENDED_OPCODE space)
```cpp
EXT_MD5 = 0xF9,            // MD5(data)
EXT_SHA1 = 0xFA,           // SHA1(data)
EXT_SHA256 = 0xFB,         // SHA256(data)
EXT_SHA512 = 0xFC,         // SHA512(data)
EXT_ENCODE = 0xFD,         // ENCODE(data, format)
EXT_DECODE = 0xFE,         // DECODE(text, format)
```

### Bit Manipulation (Use next extended block)
Start with EXTENDED_OPCODE 0xFF, then new block starting 0x0100+

### XML Functions (After bit manipulation)
Reserve block in extended space

---

## Testing Strategy

### Statistical Functions
- Test with known datasets
- Verify against manual calculations
- Edge cases: empty set, single value, NULLs

### Cryptographic Functions
- Test against known hash values
- Verify ENCODE/DECODE round-trip
- Test with binary data

### XML Functions
- Test well-formed XML
- Test malformed XML (error handling)
- Test element creation

### Bit Manipulation
- Test all bitwise operators
- Test edge cases (negative numbers, overflow)
- Test byte/bit access boundary conditions

---

## Dependencies

### External Libraries (Optional)

**OpenSSL** (crypto functions):
```cmake
find_package(OpenSSL)
if(OpenSSL_FOUND)
    target_link_libraries(scratchbird_sblr OpenSSL::Crypto)
    add_definitions(-DHAVE_OPENSSL)
endif()
```

**libxml2** (XML functions):
```cmake
find_package(LibXml2)
if(LibXml2_FOUND)
    target_link_libraries(scratchbird_sblr LibXml2::LibXml2)
    add_definitions(-DHAVE_LIBXML2)
endif()
```

**Decision**: Start with standalone implementations, add library support as optional enhancement.

---

## Success Criteria

### Completion Checklist
- ✓ All 6 statistical functions operational
- ✓ All 5+ cryptographic functions operational
- ✓ All 3-5 XML functions operational
- ✓ All 10+ bit manipulation functions operational
- ✓ Parser support for all functions
- ✓ Opcodes allocated and documented
- ✓ Comprehensive test coverage
- ✓ Documentation updated

### Performance Targets
- Statistical: O(n) single-pass where possible
- Crypto: Comparable to OpenSSL (if using library)
- Bit ops: O(1) operations

---

## Next Steps

1. Start with **bit manipulation** (highest priority, most clear requirements)
2. Then **statistical functions** (build on existing aggregates)
3. Then **cryptographic functions** (evaluate OpenSSL)
4. Finally **XML functions** (can be minimal implementation)

**Estimated Total**: 24-32 hours for all functions

---

**Status**: Ready to begin
**Next Milestone**: Bit manipulation functions complete
**Target Completion**: November 15-16, 2025
