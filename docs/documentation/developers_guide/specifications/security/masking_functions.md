# Specification: Masking Functions

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/cls/masking_functions |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/data_masking.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/data_masking.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_3.cpp`

## Synopsis

This specification defines the built-in masking functions for Column-Level Security in ScratchBird, including full masking, partial masking, and specialized masking for common data types like credit cards, SSNs, and email addresses.

## Scope

### In Scope

- Built-in masking functions
- Masking patterns and syntax
- UTF-8 aware masking
- Data type specific masking
- Custom masking functions

### Out of Scope

- Column permission management (see `authorization_model.md`)
- CLS column masking integration (see `cls_column_masking.md`)
- Encryption (see `encryption.md`)

## Background

Masking functions transform sensitive data for users without the UNMASK privilege, allowing limited data visibility while protecting sensitive information.

## Specification

### Data Structures

```cpp
// From data_masking.h

enum class MaskingType : uint8_t {
    NONE = 0,       // No masking - return original value
    FULL = 1,       // Full masking - replace entire value
    PARTIAL = 2     // Partial masking - pattern-based reveal
};

struct MaskingConfig {
    MaskingType type = MaskingType::NONE;
    
    // For FULL masking
    std::string full_mask_char = "*";
    
    // For PARTIAL masking
    std::string pattern;           // Masking pattern
    std::string full_mask_char = "X";  // Character for masked positions
    
    // Pattern syntax:
    // '#' - Reveal character from original value
    // 'X' - Mask this position
    // Any other char - Literal character (e.g., '-', '(')
};

// Built-in masking function types
enum class BuiltInMaskingFunction : uint8_t {
    NONE = 0,
    FULL = 1,
    PARTIAL = 2,
    EMAIL = 3,
    CREDIT_CARD = 4,
    SSN = 5,
    PHONE = 6,
    REGEX = 7,
    HASH = 8,
    RANDOM = 9
};
```

### Built-in Masking Functions

#### 1. FULL Masking

Replace entire value with mask character.

```cpp
Status maskFull(
    const std::string& value,
    const std::string& mask_char,
    std::string& output
);
```

**Examples:**

| Input | Mask Char | Output |
|-------|-----------|--------|
| `sensitive` | `*` | `*********` |
| `secret123` | `#` | `#########` |
| `price: $100` | `X` | `XXXXXXXXXXX` |

#### 2. PARTIAL Masking

Pattern-based masking with reveal positions.

```cpp
Status maskPartial(
    const std::string& value,
    const std::string& pattern,
    const std::string& mask_char,
    std::string& output
);
```

**Pattern Syntax:**

| Token | Meaning |
|-------|---------|
| `#` | Reveal character from original |
| `X` | Mask this position |
| Other | Literal character |

**Examples:**

| Input | Pattern | Output | Description |
|-------|---------|--------|-------------|
| `123-45-6789` | `XXX-XX-####` | `XXX-XX-6789` | SSN (show last 4) |
| `5551234567` | `(###) ###-####` | `(555) 123-4567` | Phone format |
| `4532123456789012` | `XXXX-XXXX-XXXX-####` | `XXXX-XXXX-XXXX-9012` | Credit card |
| `alice@example.com` | `###@XXX.XXX` | `ali@XXX.XXX` | Email |
| `ABCDEFGHIJ` | `XXXXXX####` | `ABCDEF####` | Account number |

**Algorithm:**
```
Input: value, pattern, mask_char
Output: masked string

1. VALIDATE mask_char is non-empty UTF-8
2. SPLIT value into UTF-8 characters[]
3. INITIALIZE output = ""
4. SET input_idx = 0

5. FOR EACH token IN pattern:
     IF token == '#':
       IF input_idx < characters.size():
         output += characters[input_idx++]
       ELSE:
         output += mask_char  // Pad if input shorter
     ELSE IF token == 'X':
       IF input_idx < characters.size():
         input_idx++  // Consume input
       output += mask_char
     ELSE:
       // Literal character
       output += token
       IF input_idx < characters.size() AND characters[input_idx] == token:
         input_idx++  // Consume matching literal from input

6. // Mask remaining input
   WHILE input_idx < characters.size():
     output += mask_char
     input_idx++

7. RETURN output
```

#### 3. EMAIL Masking

Specialized email address masking.

```cpp
Status maskEmail(
    const std::string& email,
    uint32_t reveal_local,    // Characters to show in local part
    uint32_t reveal_domain,   // Characters to show in domain
    std::string& output
);
```

**Examples:**

| Input | Reveal Local | Reveal Domain | Output |
|-------|--------------|---------------|--------|
| `john.doe@example.com` | 2 | 0 | `jo***@*******.***` |
| `alice@company.org` | 3 | 4 | `ali**@comp***.org` |
| `bob@test.io` | 1 | 2 | `b**@te**.**` |

#### 4. Credit Card Masking

PCI-DSS compliant credit card masking.

```cpp
Status maskCreditCard(
    const std::string& card_number,
    bool show_first_six,      // Show BIN (first 6)
    bool show_last_four,
    std::string& output
);
```

**Examples:**

| Input | First 6 | Last 4 | Output |
|-------|---------|--------|--------|
| `4532123456789012` | No | Yes | `XXXX-XXXX-XXXX-9012` |
| `4532123456789012` | Yes | Yes | `453212-XXXXXX-9012` |
| `371449635398431` | No | Yes | `XXXX-XXXXXX-X8431` (Amex) |

**Card Type Detection:**
```cpp
enum class CardType {
    UNKNOWN,
    VISA,       // Starts with 4
    MASTERCARD, // Starts with 51-55, 2221-2720
    AMEX,       // Starts with 34, 37
    DISCOVER    // Starts with 6011, 644-649, 65
};
```

#### 5. Phone Number Masking

International phone number masking.

```cpp
Status maskPhone(
    const std::string& phone,
    bool show_country_code,
    uint32_t show_local_digits,
    std::string& output
);
```

**Examples:**

| Input | Country Code | Local Digits | Output |
|-------|--------------|--------------|--------|
| `+1-555-123-4567` | Yes | 4 | `+1-XXX-XXX-4567` |
| `+44-20-7946-0958` | Yes | 2 | `+44-XX-XXXX-XX58` |
| `+81-3-1234-5678` | No | 4 | `XX-XXXX-5678` |

#### 6. HASH Masking

Replace with cryptographic hash.

```cpp
Status maskHash(
    const std::string& value,
    HashAlgorithm algorithm,  // SHA-256, SHA-512
    uint32_t show_prefix,     // Show first N chars of hash
    std::string& output
);
```

**Example:**

| Input | Algorithm | Show Prefix | Output |
|-------|-----------|-------------|--------|
| `secret` | SHA-256 | 8 | `e3b0c442...` |

#### 7. RANDOM Masking

Replace with random data of same type/format.

```cpp
Status maskRandom(
    const std::string& value,
    DataType data_type,       // NUMBER, STRING, DATE, etc.
    std::string& output
);
```

**Example:**

| Input | Type | Output |
|-------|------|--------|
| `12345` | NUMBER | `84923` |
| `John Doe` | NAME | `Xkzj Wpl` |
| `2024-03-08` | DATE | `2019-11-23` |

#### 8. REGEX Masking

Regex-based pattern matching and replacement.

```cpp
Status maskRegex(
    const std::string& value,
    const std::string& pattern,
    const std::string& replacement,
    std::string& output
);
```

**Examples:**

| Input | Pattern | Replacement | Output |
|-------|---------|-------------|--------|
| `user@example.com` | `@.*$` | `@masked.com` | `user@masked.com` |
| `123-45-6789` | `\d(?=\d{4})` | `X` | `XXX-XX-6789` |

### SQL Functions

```sql
-- Full masking
SELECT mask_full(column_name, '*') FROM table;

-- Partial masking
SELECT mask_partial(ssn, 'XXX-XX-####', 'X') FROM employees;

-- Email masking
SELECT mask_email(email, 2, 0) FROM users;

-- Credit card
SELECT mask_credit_card(card_number, false, true) FROM payments;

-- Phone
SELECT mask_phone(phone, true, 4) FROM contacts;

-- Hash
SELECT mask_hash(sensitive_data, 'sha256', 8) FROM data;

-- Random
SELECT mask_random(date_of_birth, 'DATE') FROM patients;
```

### Masking Configuration Table

```sql
-- Define masking rules
CREATE TABLE sb_masking_policies (
    policy_id UUID PRIMARY KEY,
    table_name VARCHAR(128),
    column_name VARCHAR(128),
    masking_function VARCHAR(64),
    masking_params JSONB,
    applicable_roles VARCHAR(128)[] DEFAULT '{}',
    is_enabled BOOLEAN DEFAULT true
);

-- Example entries
INSERT INTO sb_masking_policies VALUES
    (gen_uuid(), 'customers', 'ssn', 'SSN', '{}', '{public}', true),
    (gen_uuid(), 'customers', 'credit_card', 'CREDIT_CARD', 
     '{"show_last_four": true}', '{public}', true),
    (gen_uuid(), 'users', 'email', 'EMAIL', 
     '{"reveal_local": 2, "reveal_domain": 0}', '{public}', true);
```

### UTF-8 Handling

```cpp
// UTF-8 aware character splitting
std::vector<std::string> splitUtf8OrBytes(const std::string& value) {
    // Try UTF-8 first
    if (UTF8Utils::isValidUTF8(value)) {
        chars = []
        pos = 0
        while pos < value.size():
            start = pos
            decoded = UTF8Utils::decodeChar(value, pos)
            if decoded.has_value():
                chars.append(value.substr(start, pos - start))
            else:
                // Invalid sequence - fall back to bytes
                return splitBytes(value)
        return chars
    } else {
        // Binary data - split by bytes
        return splitBytes(value)
    }
}
```

### Performance Considerations

| Function | Complexity | Notes |
|----------|------------|-------|
| FULL | O(n) | Simple character replacement |
| PARTIAL | O(n) | Single pass through pattern |
| EMAIL | O(n) | String manipulation |
| CREDIT_CARD | O(n) | With card type detection |
| PHONE | O(n) | Parsing and formatting |
| HASH | O(n) | Cryptographic operation |
| RANDOM | O(n) | Random generation |
| REGEX | O(n*m) | Pattern dependent |

## Invariants

1. **Length Preservation**: Output length approximately equals input (for most functions)
   - Verification: Implementation ensures proper sizing

2. **UTF-8 Validity**: Output is valid UTF-8 if input is valid UTF-8
   - Verification: Character-by-character handling

3. **Deterministic**: Same input produces same output (except RANDOM)
   - Verification: No randomness in most functions

## Related Specifications

- `cls_column_masking.md` - Column-level masking integration
- `authorization_model.md` - Permission system
- `encryption.md` - Encryption (alternative to masking)

## Appendix

### Masking Pattern Library

```sql
-- Common patterns
SELECT mask_partial(ssn, 'XXX-XX-####', 'X') FROM employees;
SELECT mask_partial(credit_card, 'XXXX-XXXX-XXXX-####', 'X') FROM payments;
SELECT mask_partial(phone, '(###) XXX-####', 'X') FROM contacts;
SELECT mask_partial(account, 'XXXXXX####', 'X') FROM accounts;
SELECT mask_partial(iban, 'XX## #### #### #### #### ##', 'X') FROM banks;
```

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
