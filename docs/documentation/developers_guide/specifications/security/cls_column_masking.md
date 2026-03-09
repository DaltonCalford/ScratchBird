# Specification: CLS Column Masking

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/cls |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/data_masking.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:5253` (ColumnPermissionRecord)
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/data_masking.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_security_issues.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_3.cpp`

## Synopsis

This specification defines Column-Level Security (CLS) and data masking in ScratchBird, including column permission management, masking rule definitions, full and partial masking patterns, and runtime masking application. CLS enables fine-grained control over which columns users can access and how sensitive data is masked.

## Scope

### In Scope

- Column-level GRANT/REVOKE permissions
- Data masking rules and patterns
- Full masking (complete replacement)
- Partial masking (pattern-based reveal)
- UTF-8 aware masking for international data
- Masking bypass privileges (UNMASK)

### Out of Scope

- Row-Level Security (see `rls_policy_enforcement.md`)
- Authentication flows (see `authentication_flow.md`)
- Authorization model (see `authorization_model.md`)
- Encryption at rest or in transit

## Background

Column-Level Security (CLS) provides:

1. **Column Permissions**: Fine-grained SELECT/UPDATE/REFERENCES grants per column
2. **Data Masking**: Dynamic transformation of sensitive data based on privileges
3. **Masking Patterns**: Configurable reveal patterns (e.g., show last 4 digits of SSN)
4. **Privilege Bypass**: UNMASK privilege to see unmasked data

## Specification

### Data Structures

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/data_masking.h

/**
 * Masking type enumeration
 */
enum class MaskingType : uint8_t {
    NONE = 0,       // No masking - return original value
    FULL = 1,       // Full masking - replace entire value
    PARTIAL = 2     // Partial masking - pattern-based reveal
};

/**
 * Masking configuration for a column
 */
struct MaskingConfig {
    MaskingType type = MaskingType::NONE;
    
    // For FULL masking
    std::string full_mask_char = "*";
    
    // For PARTIAL masking
    std::string pattern;           // Masking pattern (e.g., "XXX-XX-####")
    std::string full_mask_char = "X";  // Character for masked positions
    
    // Pattern syntax:
    // '#' - Reveal character from original value
    // 'X' - Mask this position
    // Any other char - Literal character (e.g., '-', '(')
};
```

```cpp
// Column permission record from catalog_manager.cpp:5253-5264
struct ColumnPermissionRecord {
    ID permission_id;        // UUIDv7
    ID grantee_id;           // User/role receiving permission
    ID grantor_id;           // Who granted
    ID object_id;            // Table UUID
    uint32_t column_id;      // Column ordinal position
    uint32_t permissions;    // Bitmask: SELECT=1, UPDATE=2, REFERENCES=4
    uint8_t grant_option;    // WITH GRANT OPTION
};

// Column permission bitmasks
enum ColumnPermission : uint32_t {
    COLUMN_SELECT = 1,
    COLUMN_UPDATE = 2,
    COLUMN_REFERENCES = 4
};
```

```cpp
// Column security metadata
struct ColumnSecurityMetadata {
    ID column_id;
    ID table_id;
    std::string column_name;
    
    // Masking configuration
    MaskingConfig masking_config;
    bool masking_enabled = false;
    
    // Required privilege to see unmasked
    // If user lacks UNMASK privilege, masking is applied
    bool requires_unmask_privilege = false;
};

// Masking evaluation context
struct MaskingContext {
    ID user_id;
    std::vector<ID> role_ids;
    bool has_unmask_privilege = false;
    bool has_column_select_permission = false;
};
```

### Interface Contracts

#### Function: `DataMasking::applyMasking()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/data_masking.cpp:67
Status DataMasking::applyMasking(
    const std::string& value,
    const MaskingConfig& config,
    bool has_privilege,
    std::string& masked_out,
    ErrorContext* ctx);
```

**Preconditions:**
- Value is valid UTF-8 or binary data
- Config has valid masking type and pattern

**Postconditions:**
- If has_privilege or type == NONE: masked_out = value
- If type == FULL: masked_out is fully masked
- If type == PARTIAL: masked_out follows pattern rules

**Algorithm:**
```
1. CHECK privilege or masking type
   if has_privilege OR config.type == NONE:
       masked_out = value
       return OK

2. APPLY MASKING based on type
   switch config.type:
       case FULL:
           return applyFullMasking(value, config.full_mask_char, masked_out, ctx)
           
       case PARTIAL:
           return applyPartialMasking(value, config.pattern, 
                                      config.full_mask_char, masked_out, ctx)
```

#### Function: `DataMasking::applyPartialMasking()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/data_masking.cpp:108
Status DataMasking::applyPartialMasking(
    const std::string& value,
    const std::string& pattern,
    const std::string& mask_char,
    std::string& masked_out,
    ErrorContext* ctx);
```

**Preconditions:**
- Pattern is non-empty
- Mask character is valid UTF-8

**Postconditions:**
- Output follows pattern rules
- UTF-8 characters handled correctly

**Algorithm:**
```
Input: value, pattern, mask_char
Output: masked_out

1. NORMALIZE mask character
   if mask_char empty: return error
   
2. SPLIT value into characters
   if value is valid UTF-8:
       chars = splitUTF8(value)
   else:
       chars = splitBytes(value)  // Binary fallback

3. PROCESS pattern
   output = ""
   input_idx = 0
   
   for token in pattern:
       if token == '#':
           // Reveal input character
           if input_idx < chars.size():
               output += chars[input_idx++]
           else:
               output += mask_char  // Pad with mask
               
       else if token == 'X':
           // Mask this position
           if input_idx < chars.size():
               input_idx++  // Consume input
           output += mask_char
           
       else:
           // Literal character
           output += token
           // Optional: consume matching input
           if input_idx < chars.size() AND chars[input_idx] == token:
               input_idx++

4. MASK REMAINING INPUT
   while input_idx < chars.size():
       output += mask_char
       input_idx++

5. masked_out = output
```

#### Function: `CatalogManager::grantColumnPermission()`

```cpp
// Source: catalog_manager.cpp (column permission operations)
Status CatalogManager::grantColumnPermission(
    const ID& grantor_id,
    const ID& grantee_id,
    const ID& table_id,
    uint32_t column_id,
    uint32_t permissions,  // SELECT, UPDATE, REFERENCES bitmask
    bool with_grant_option,
    ErrorContext* ctx);
```

**Preconditions:**
- Grantor has table-level GRANT OPTION or column-level GRANT OPTION
- Table and column exist
- Grantee exists

**Postconditions:**
- ColumnPermissionRecord created or updated
- Column permission cache invalidated

**Error Handling:**
- `PERMISSION_DENIED`: Grantor lacks required privilege
- `NOT_FOUND`: Table, column, or grantee doesn't exist
- `INVALID_ARGUMENT`: Invalid permission bitmask

#### Function: `Executor::applyColumnMasking()`

```cpp
// Source: sblr/executor.cpp (query result masking)
Status Executor::applyColumnMasking(
    ResultSet& results,
    const std::vector<ColumnSecurityMetadata>& column_metadata,
    const MaskingContext& ctx,
    ErrorContext* err_ctx);
```

**Preconditions:**
- ResultSet contains data from a table with masked columns
- Column metadata includes masking configs
- Masking context has user privileges

**Postconditions:**
- Sensitive columns masked based on privileges
- Original values preserved if user has UNMASK or column SELECT

**Algorithm:**
```
For each row in results:
    For each column in row:
        meta = column_metadata[column.index]
        
        if not meta.masking_enabled:
            continue  // No masking for this column
            
        if ctx.has_unmask_privilege:
            continue  // User can see unmasked data
            
        if ctx.has_column_select_permission:
            continue  // User has explicit column access
            
        // Apply masking
        Status status = DataMasking::applyMasking(
            row[column.index],
            meta.masking_config,
            false,  // no privilege
            row[column.index],
            err_ctx
        )
        
        if status != OK:
            return status
```

### Masking Patterns

```
Pattern Syntax:

Token    Meaning
------   --------
#        Reveal character from original value
X        Mask this position with mask_char
other    Literal character (passed through)

Examples:

Pattern:      XXX-XX-####
Value:        123-45-6789
Result:       XXX-XX-6789
Description:  Mask SSN except last 4

Pattern:      (###) ###-####
Value:        5551234567
Result:       (555) 123-4567
Description:  Phone number formatting (no masking)

Pattern:      XXXX-XXXX-XXXX-####
Value:        4532123456789012
Result:       XXXX-XXXX-XXXX-9012
Description:  Credit card masking (show last 4)

Pattern:      ###@XXX.XXX
Value:        alice@example.com
Result:       ali@XXX.XXX
Description:  Email masking (partial reveal)

Pattern:      XXXXXX####
Value:        1234567890
Result:       XXXXXX7890
Description:  Account number (show last 4)
```

### UTF-8 Handling

```cpp
// From /home/dcalford/CliWork/ScratchBird/src/core/data_masking.cpp:31-64
std::vector<std::string> splitUtf8OrBytes(const std::string& value) {
    // Try UTF-8 first
    if (UTF8Utils::isValidUTF8(value)):
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
    else:
        // Binary data - split by bytes
        return splitBytes(value)
}
```

### Algorithms

#### Algorithm: Full Masking

```
Input:  value, mask_char
Output: masked string

1. VALIDATE mask_char is valid UTF-8
   
2. COUNT characters in value
   if UTF8Utils::isValidUTF8(value):
       count = UTF8Utils::countCharacters(value)
   else:
       count = value.size()  // Byte count for binary

3. BUILD masked output
   masked_out = ""
   for i in 0 to count-1:
       masked_out += mask_char

4. RETURN masked_out
```

#### Algorithm: Column Access Check

```
Input:  user_id, roles[], table_id, column_id, required_perm
Output: Boolean - has access

1. CHECK UNMASK privilege
   if user_has_privilege(user_id, UNMASK):
       return true  // Full access to all columns

2. CHECK column-level permission
   perms = catalog.getColumnPermissions(user_id, table_id, column_id)
   
   // Direct grant
   if perms & required_perm:
       return true
   
   // Role grants
   for role_id in roles:
       role_perms = catalog.getColumnPermissions(role_id, table_id, column_id)
       if role_perms & required_perm:
           return true

3. CHECK table-level permission
   table_perms = catalog.getTablePermissions(user_id, table_id)
   if table_perms & required_perm:
       return true  // Table-level implies all columns

4. CHECK PUBLIC grants
   public_perms = catalog.getColumnPermissions(PUBLIC_USER_ID, table_id, column_id)
   if public_perms & required_perm:
       return true

5. RETURN false
```

### Decision Trees

```
Column Data Access
│
├─ User has UNMASK privilege? ──Yes──► Return unmasked value
│
├─ Column has masking enabled? ──No──► Return unmasked value
│
├─ User has column SELECT permission? ──Yes──► Return unmasked value
│
└─ Apply masking:
   ├─ Determine masking type (FULL or PARTIAL)
   ├─ If PARTIAL: Parse pattern
   │   ├─ '#' = reveal character
   │   ├─ 'X' = mask character
   │   └─ other = literal
   ├─ Split value into characters (UTF-8 aware)
   ├─ Process pattern tokens
   └─ Return masked value
```

```
Column Permission Check
│
├─ User is table owner? ──Yes──► Grant all permissions
│
├─ User has table-level permission? ──Yes──► Grant for all columns
│
├─ Check column-level grants:
│   ├─ Direct user grant? ──Yes──► Grant if matches
│   ├─ Role grant? ──Yes──► Grant if any role has permission
│   └─ PUBLIC grant? ──Yes──► Grant if PUBLIC has permission
│
└─ No grants found ──► Deny access
```

## Invariants

1. **UTF-8 Preservation**: Masking preserves valid UTF-8 encoding
   - Verification: `UTF8Utils::decodeChar()` used in splitting

2. **Mask Character Consistency**: Same mask character used throughout output
   - Verification: Single `mask_char` parameter for entire operation

3. **Pattern Literal Handling**: Non-pattern characters passed through
   - Verification: Default case in pattern processing loop

4. **Privilege Bypass**: UNMASK privilege always shows unmasked data
   - Verification: First check in masking decision tree

5. **Column-Level Precedence**: Column permissions override table permissions only when more restrictive
   - Verification: Check column permissions first, fall back to table

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_ARGUMENT` | Empty mask character | Provide valid mask character |
| `INVALID_ARGUMENT` | Invalid UTF-8 in mask | Use ASCII mask character |
| `PERMISSION_DENIED` | No column SELECT | Request column access |
| `INTERNAL_ERROR` | UTF-8 decode failure | Fall back to byte masking |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `test_security_phase3_3.cpp` | Column permission tests |
| `test_security_issues.cpp` | General security tests |

## Related Specifications

- `authorization_model.md` - Table and object permissions
- `privilege_types.md` - Column-level privilege definitions
- `masking_functions.md` - Built-in masking functions
- `rls_policy_enforcement.md` - Row-Level Security
- `authentication_flow.md` - User identity for permission checks
- `encryption.md` - Encryption (alternative to masking)

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| CLS | Column-Level Security - column access control |
| Masking | Dynamic data transformation for sensitive columns |
| FULL masking | Complete replacement of value |
| PARTIAL masking | Pattern-based reveal of value |
| UNMASK | Privilege to see unmasked data |
| Pattern | Template defining which characters to reveal/mask |

### Example Masking Configurations

```sql
-- Credit card masking (show last 4)
ALTER TABLE customers
    ALTER COLUMN credit_card
    SET MASKING PATTERN 'XXXX-XXXX-XXXX-####';

-- Email masking (show first 3)
ALTER TABLE users
    ALTER COLUMN email
    SET MASKING PATTERN '###@XXX.XXX';

-- Full masking for sensitive data
ALTER TABLE employees
    ALTER COLUMN salary
    SET MASKING TYPE FULL WITH CHAR '*';

-- Phone number formatting with masking
ALTER TABLE contacts
    ALTER COLUMN phone
    SET MASKING PATTERN '(###) XXX-XXXX';
```

### Masking Pattern Examples

| Data Type | Pattern | Example Input | Masked Output |
|-----------|---------|---------------|---------------|
| SSN | `XXX-XX-####` | 123-45-6789 | XXX-XX-6789 |
| Credit Card | `XXXX-XXXX-XXXX-####` | 4532123456789012 | XXXX-XXXX-XXXX-9012 |
| Phone | `(###) XXX-####` | 5551234567 | (555) XXX-4567 |
| Email | `###@XXX.XXX` | user@example.com | use@XXX.XXX |
| Bank Account | `XXXXXX####` | 1234567890 | XXXXXX7890 |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
