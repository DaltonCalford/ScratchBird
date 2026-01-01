# PLAN 03B: Domain Infrastructure Implementation

**Version:** 1.1
**Date:** 2025-12-31
**Status:** IN PROGRESS

**Purpose:** Implement all infrastructure required for domain WITH blocks so that Plan 04 (Domain DDL Parsers) can proceed unimpeded.

**Prerequisite For:** PLAN_04_IMPLEMENTATION_CHECKLIST.md

---

## Current Status
- Plan 02B core implementation is complete; Plan 03B can proceed.
- Encryption key management, data encryption, masking, and domain encryption infrastructure are complete.
- Encrypted storage layer integration (Task 1.3) is complete.
- Domain encryption integration (Task 1.4) is complete.
- Remaining tasks not started beyond Section 2.

## CRITICAL RULES

1. **NO DEFERRALS** - Every item must be fully implemented
2. **NO STUBS** - No placeholder code that "will be implemented later"
3. **NO PARTIAL IMPLEMENTATIONS** - Feature either complete or not started
4. **NO TODOS** - No "TODO: implement this later" comments
5. **MGA COMPLIANCE** - All features must work with Firebird MGA transaction visibility
6. **FULL TESTING** - Every component must have comprehensive test coverage
7. **ALPHA SCOPE** - All engine functionality must be complete for Alpha

---

## Executive Summary

Plan 04 requires domain WITH blocks (SECURITY, INTEGRITY, VALIDATION, QUALITY) to be fully functional. These blocks require infrastructure that must exist BEFORE the parsers can be implemented.

**Timeline Estimate:**
- Single Developer: 17-24 working days
- Three Developers (parallel): 7.5-10.5 working days

**Total Tasks:** 35 tasks across 8 sections

---

## SECTION 1: DATA-AT-REST ENCRYPTION INFRASTRUCTURE

### Task 1.1: Encryption Key Management

**Status:** ✅ COMPLETE
**Priority:** CRITICAL
**Estimated Time:** 12 hours

**File to Create:** `include/scratchbird/core/encryption_key_manager.h`
**File to Create:** `src/core/encryption_key_manager.cpp`

**Requirements:**
1. Key generation (AES-256, AES-128)
2. Secure key storage in system catalog
3. Key rotation support
4. Per-domain key isolation
5. Master key encryption of domain keys
6. Key derivation functions (KDF)

**Implementation Details:**

```cpp
namespace scratchbird::core {

enum class EncryptionAlgorithm : uint8_t {
    NONE = 0,
    AES128_GCM = 1,
    AES256_GCM = 2
};

struct EncryptionKey {
    ID key_id;                          // Unique key identifier
    ID domain_id;                       // Domain this key belongs to
    EncryptionAlgorithm algorithm;      // Encryption algorithm
    std::vector<uint8_t> encrypted_key; // Key encrypted with master key
    std::vector<uint8_t> key_salt;      // Salt for KDF
    uint32_t key_version;               // For rotation support
    uint64_t created_at;                // Timestamp
    uint64_t rotated_at;                // Last rotation timestamp
    bool active;                        // Is this the active key?
};

class EncryptionKeyManager {
public:
    // Key lifecycle
    Status generateKey(const ID& domain_id, EncryptionAlgorithm algo,
                      ID& key_id_out, ErrorContext* ctx);
    Status rotateKey(const ID& domain_id, ErrorContext* ctx);
    Status deleteKey(const ID& key_id, ErrorContext* ctx);

    // Key retrieval
    Status getActiveKey(const ID& domain_id, EncryptionKey& key_out,
                       ErrorContext* ctx);
    Status getKeyByVersion(const ID& domain_id, uint32_t version,
                          EncryptionKey& key_out, ErrorContext* ctx);

    // Key decryption (returns plaintext key for use)
    Status decryptKey(const EncryptionKey& encrypted_key,
                     std::vector<uint8_t>& plaintext_key_out,
                     ErrorContext* ctx);

    // Master key management
    Status setMasterKey(const std::vector<uint8_t>& master_key,
                       ErrorContext* ctx);
    Status initializeMasterKey(ErrorContext* ctx); // Generate on first run

private:
    Database* db_;
    std::vector<uint8_t> master_key_; // Kept in memory, encrypted at rest
    std::mutex mutex_;
};

} // namespace scratchbird::core
```

**Acceptance Criteria:**
- [ ] Keys generated with cryptographically secure random
- [ ] Master key stored encrypted in system catalog
- [ ] Per-domain keys encrypted with master key
- [ ] Key rotation updates key_version without data re-encryption
- [ ] Old key versions retained for reading old data
- [ ] Thread-safe operation
- [ ] Full test coverage

**Test File:** `tests/unit/test_encryption_key_manager.cpp` (NEW)

---

### Task 1.2: AES Encryption/Decryption Implementation

**Status:** ✅ COMPLETE
**Priority:** CRITICAL
**Estimated Time:** 16 hours

**File to Create:** `include/scratchbird/core/data_encryption.h`
**File to Create:** `src/core/data_encryption.cpp`

**Requirements:**
1. AES-256-GCM encryption/decryption
2. AES-128-GCM encryption/decryption
3. IV (Initialization Vector) generation
4. Authentication tag handling
5. Integration with storage layer
6. Performance optimization (AES-NI support)

**Implementation Details:**

```cpp
namespace scratchbird::core {

struct EncryptedValue {
    std::vector<uint8_t> ciphertext;    // Encrypted data
    std::vector<uint8_t> iv;            // Initialization vector (12 bytes for GCM)
    std::vector<uint8_t> auth_tag;      // Authentication tag (16 bytes)
    uint32_t key_version;               // Key version used for encryption
    EncryptionAlgorithm algorithm;      // Algorithm used
};

class DataEncryption {
public:
    /**
     * Encrypt plaintext value
     *
     * @param plaintext Data to encrypt
     * @param key Encryption key (raw bytes)
     * @param algorithm Algorithm to use
     * @param encrypted_out [out] Encrypted result
     * @param ctx Error context
     * @return Status::OK on success
     */
    static Status encrypt(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& key,
        EncryptionAlgorithm algorithm,
        EncryptedValue& encrypted_out,
        ErrorContext* ctx);

    /**
     * Decrypt ciphertext value
     *
     * @param encrypted Encrypted data
     * @param key Decryption key (raw bytes)
     * @param plaintext_out [out] Decrypted result
     * @param ctx Error context
     * @return Status::OK on success, AUTHENTICATION_FAILED if tag invalid
     */
    static Status decrypt(
        const EncryptedValue& encrypted,
        const std::vector<uint8_t>& key,
        std::vector<uint8_t>& plaintext_out,
        ErrorContext* ctx);

    /**
     * Generate random IV for GCM mode
     */
    static void generateIV(std::vector<uint8_t>& iv_out);

    /**
     * Check if AES-NI hardware acceleration available
     */
    static bool hasHardwareAcceleration();

private:
    static Status encryptAES256GCM(const std::vector<uint8_t>& plaintext,
                                   const std::vector<uint8_t>& key,
                                   EncryptedValue& encrypted_out,
                                   ErrorContext* ctx);

    static Status encryptAES128GCM(const std::vector<uint8_t>& plaintext,
                                   const std::vector<uint8_t>& key,
                                   EncryptedValue& encrypted_out,
                                   ErrorContext* ctx);

    static Status decryptAES256GCM(const EncryptedValue& encrypted,
                                   const std::vector<uint8_t>& key,
                                   std::vector<uint8_t>& plaintext_out,
                                   ErrorContext* ctx);

    static Status decryptAES128GCM(const EncryptedValue& encrypted,
                                   const std::vector<uint8_t>& key,
                                   std::vector<uint8_t>& plaintext_out,
                                   ErrorContext* ctx);
};

} // namespace scratchbird::core
```

**Crypto Library Selection:**
- Use OpenSSL (industry standard, well-audited)
- Fallback to software implementation if AES-NI unavailable
- FIPS mode support if needed

**Acceptance Criteria:**
- [x] AES-256-GCM encryption/decryption works correctly
- [x] AES-128-GCM encryption/decryption works correctly
- [x] Authentication tags validated on decrypt
- [x] Random IV generated for each encryption
- [x] Hardware acceleration used when available
- [x] Constant-time operations to prevent timing attacks
- [x] Full test coverage with test vectors from NIST
- [x] Performance test (>10,000 ops/sec for small values)

**Test File:** `tests/unit/test_data_encryption.cpp` (NEW)

---

### Task 1.3: Storage Layer Integration for Encrypted Data

**Status:** ✅ COMPLETE
**Priority:** CRITICAL
**Estimated Time:** 12 hours

**File to Modify:** `include/scratchbird/core/heap_page.h`
**File to Modify:** `src/core/heap_page.cpp`
**File to Modify:** `include/scratchbird/core/typed_value.h`
**File to Modify:** `src/core/typed_value.cpp`

**Requirements:**
1. Store EncryptedValue in heap pages
2. TypedValue support for encrypted values
3. Transparent encryption on write
4. Transparent decryption on read
5. TOAST support for large encrypted values

**Implementation Details:**

**Heap Page Storage Format:**
```cpp
// Encrypted value on-disk format
struct EncryptedValueRecord {
    uint8_t algorithm;           // EncryptionAlgorithm
    uint32_t key_version;        // Key version used
    uint16_t iv_length;          // IV length (typically 12)
    uint16_t auth_tag_length;    // Auth tag length (typically 16)
    uint32_t ciphertext_length;  // Ciphertext length
    // Followed by:
    // - IV bytes (iv_length)
    // - Auth tag bytes (auth_tag_length)
    // - Ciphertext bytes (ciphertext_length)
};
```

**TypedValue Extensions:**
```cpp
namespace scratchbird::core {

class TypedValue {
    // ... existing fields ...

    // NEW: Encryption metadata
    bool is_encrypted_ = false;
    uint32_t encryption_key_version_ = 0;
    EncryptionAlgorithm encryption_algorithm_ = EncryptionAlgorithm::NONE;

public:
    // Encryption support
    bool isEncrypted() const { return is_encrypted_; }
    void setEncrypted(bool encrypted) { is_encrypted_ = encrypted; }
    uint32_t encryptionKeyVersion() const { return encryption_key_version_; }
    EncryptionAlgorithm encryptionAlgorithm() const { return encryption_algorithm_; }

    // Encrypt this value (modifies internal storage)
    Status encrypt(const std::vector<uint8_t>& key,
                  EncryptionAlgorithm algo,
                  uint32_t key_version,
                  ErrorContext* ctx);

    // Decrypt this value (modifies internal storage)
    Status decrypt(const std::vector<uint8_t>& key,
                  ErrorContext* ctx);
};

} // namespace scratchbird::core
```

**Acceptance Criteria:**
- [x] EncryptedValueRecord format documented
- [x] TypedValue can store encrypted values
- [x] Heap page writes encrypted values correctly
- [x] Heap page reads encrypted values correctly
- [x] TOAST works with encrypted values
- [x] Transaction visibility works with encrypted values (MGA)
- [x] Full test coverage

**Test File:** `tests/unit/test_encrypted_storage.cpp` (NEW)

---

### Task 1.4: Domain Encryption Integration

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 10 hours

**File to Modify:** `include/scratchbird/core/domain_manager.h`
**File to Modify:** `src/core/domain_manager.cpp`
**File to Modify:** `src/sblr/executor.cpp`
**File to Create:** `tests/integration/test_domain_encryption.cpp`

**Requirements:**
1. Store encryption settings in DomainInfo
2. Automatic encryption on INSERT/UPDATE for encrypted domains
3. Automatic decryption on SELECT for encrypted domains
4. Integration with EncryptionKeyManager

**Implementation Details:**

**DomainSecurity Extensions (DomainInfo::security):**
```cpp
struct DomainSecurity {
    // ... existing fields ...
    bool encryption_enabled = false;
    EncryptionAlgorithm encryption_algorithm = EncryptionAlgorithm::NONE;
    ID encryption_key_id;  // Active encryption key
};
```

**DomainManager Extensions:**
```cpp
class DomainManager {
    // ... existing methods ...

    // NEW: Encryption support
    Status encryptValue(const ID& domain_id, TypedValue& value, ErrorContext* ctx);
    Status decryptValue(const ID& domain_id, TypedValue& value, ErrorContext* ctx);
};
```

**Acceptance Criteria:**
- [x] DomainInfo stores encryption settings
- [x] encryptValue() encrypts using domain's key
- [x] decryptValue() decrypts using domain's key
- [x] Key rotation handled transparently (old versions still readable)
- [x] Integration with executor for INSERT/UPDATE/SELECT
- [x] Full test coverage

**Test File:** `tests/integration/test_domain_encryption.cpp` (NEW)

---

## SECTION 2: DATA MASKING INFRASTRUCTURE

### Task 2.1: Masking Engine Implementation

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 12 hours

**File to Create:** `include/scratchbird/core/data_masking.h`
**File to Create:** `src/core/data_masking.cpp`

**Requirements:**
1. PARTIAL masking with pattern support
2. FULL masking (hide all)
3. NONE masking (show all)
4. Pattern parsing (e.g., "XXX-XX-####")
5. Privilege-aware masking

**Implementation Details:**

```cpp
namespace scratchbird::core {

enum class MaskingType : uint8_t {
    NONE = 0,      // No masking
    PARTIAL = 1,   // Show subset based on pattern
    FULL = 2       // Hide all data
};

struct MaskingConfig {
    MaskingType type = MaskingType::NONE;
    std::string pattern;              // e.g., "XXX-XX-####" for SSN
    std::string full_mask_char = "*"; // Character to use for FULL masking
};

class DataMasking {
public:
    /**
     * Apply masking to value
     *
     * @param value Original value
     * @param config Masking configuration
     * @param has_privilege If true, return unmasked value
     * @param masked_out [out] Masked value
     * @return Status::OK on success
     */
    static Status applyMasking(
        const std::string& value,
        const MaskingConfig& config,
        bool has_privilege,
        std::string& masked_out,
        ErrorContext* ctx);

    /**
     * Parse masking pattern
     *
     * Pattern syntax:
     * - '#' = show original character
     * - 'X' or any other char = replace with mask character
     * - Literal chars (e.g., '-') = keep as-is
     *
     * Example: "XXX-XX-####" for SSN "123-45-6789" → "***-**-6789"
     */
    static Status parsePattern(const std::string& pattern,
                              std::vector<char>& parsed_out,
                              ErrorContext* ctx);

private:
    static Status applyPartialMasking(const std::string& value,
                                     const std::string& pattern,
                                     std::string& masked_out,
                                     ErrorContext* ctx);

    static Status applyFullMasking(const std::string& value,
                                   const std::string& mask_char,
                                   std::string& masked_out,
                                   ErrorContext* ctx);
};

} // namespace scratchbird::core
```

**Pattern Examples:**
- SSN: `"XXX-XX-####"` → "123-45-6789" becomes "***-**-6789"
- Credit Card: `"XXXX-XXXX-XXXX-####"` → "1234-5678-9012-3456" becomes "****-****-****-3456"
- Email: `"#***@***.***"` → "john@example.com" becomes "j***@***.***"

**Acceptance Criteria:**
- [ ] NONE masking returns original value
- [ ] FULL masking replaces all with mask character
- [ ] PARTIAL masking follows pattern correctly
- [ ] Pattern parsing handles various formats
- [ ] Privilege check bypasses masking
- [ ] Works with NULL values
- [ ] Full test coverage with various patterns

**Test File:** `tests/unit/test_data_masking.cpp` (NEW)

---

### Task 2.2: Domain Masking Integration

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 8 hours

**File to Modify:** `include/scratchbird/core/domain_manager.h`
**File to Modify:** `src/core/domain_manager.cpp`

**Requirements:**
1. Store masking settings in DomainInfo
2. Apply masking during SELECT based on user privileges
3. Integration with permission system

**Implementation Details:**

**DomainInfo Extensions:**
```cpp
struct DomainInfo {
    // ... existing fields ...

    // NEW: Masking settings (from WITH SECURITY)
    MaskingConfig masking_config;
    std::string required_privilege_for_unmasked;  // Privilege needed to see unmasked
};
```

**DomainManager Extensions:**
```cpp
class DomainManager {
    // ... existing methods ...

    // NEW: Masking support
    Status applyMasking(const ID& domain_id,
                       const ID& user_id,
                       TypedValue& value,
                       ErrorContext* ctx);

    Status checkMaskingPrivilege(const ID& domain_id,
                                 const ID& user_id,
                                 bool& has_privilege_out,
                                 ErrorContext* ctx);
};
```

**Acceptance Criteria:**
- [ ] DomainInfo stores masking configuration
- [ ] applyMasking() checks privilege before masking
- [ ] Integration with executor for SELECT
- [ ] NULL values handled correctly
- [ ] Full test coverage

**Test File:** Add to `tests/integration/test_domain_security.cpp`

---

## SECTION 3: GLOBAL UNIQUENESS TRACKING

### Task 3.1: Global Uniqueness Index

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 16 hours

**File to Create:** `include/scratchbird/core/global_uniqueness_index.h`
**File to Create:** `src/core/global_uniqueness_index.cpp`

**Requirements:**
1. Track unique values across ALL columns using a domain
2. Fast lookup for uniqueness checking
3. Transaction-aware (MGA visibility)
4. Concurrent insert handling

**Implementation Details:**

```cpp
namespace scratchbird::core {

/**
 * Global uniqueness index for domains
 *
 * Tracks all unique values across all columns using a domain
 * with UNIQUENESS = TRUE constraint.
 */
class GlobalUniquenessIndex {
public:
    explicit GlobalUniquenessIndex(Database* db);
    ~GlobalUniquenessIndex();

    /**
     * Check if value is unique across all columns using this domain
     *
     * @param domain_id Domain to check
     * @param value Value to check
     * @param tx_id Current transaction ID
     * @param is_unique_out [out] True if unique, false if duplicate
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status checkUniqueness(const ID& domain_id,
                          const TypedValue& value,
                          uint64_t tx_id,
                          bool& is_unique_out,
                          ErrorContext* ctx);

    /**
     * Insert value into uniqueness index
     *
     * @param domain_id Domain ID
     * @param table_id Table ID containing the value
     * @param column_id Column ID containing the value
     * @param row_id Row ID
     * @param value Value to insert
     * @param tx_id Transaction ID
     * @param ctx Error context
     * @return Status::OK on success, UNIQUENESS_VIOLATION on duplicate
     */
    Status insertValue(const ID& domain_id,
                      const ID& table_id,
                      const ID& column_id,
                      const ID& row_id,
                      const TypedValue& value,
                      uint64_t tx_id,
                      ErrorContext* ctx);

    /**
     * Delete value from uniqueness index
     */
    Status deleteValue(const ID& domain_id,
                      const ID& table_id,
                      const ID& column_id,
                      const ID& row_id,
                      const TypedValue& value,
                      uint64_t tx_id,
                      ErrorContext* ctx);

    /**
     * Update value in uniqueness index
     */
    Status updateValue(const ID& domain_id,
                      const ID& table_id,
                      const ID& column_id,
                      const ID& row_id,
                      const TypedValue& old_value,
                      const TypedValue& new_value,
                      uint64_t tx_id,
                      ErrorContext* ctx);

    /**
     * Enable/disable uniqueness tracking for a domain
     */
    Status enableUniqueness(const ID& domain_id, ErrorContext* ctx);
    Status disableUniqueness(const ID& domain_id, ErrorContext* ctx);

private:
    Database* db_;

    // Index structure: domain_id → (value → list of (table, column, row, tx_id))
    struct ValueLocation {
        ID table_id;
        ID column_id;
        ID row_id;
        uint64_t tx_id;
        uint64_t tx_end;  // For MGA visibility
    };

    std::unordered_map<ID, std::unordered_map<TypedValue, std::vector<ValueLocation>>> index_;
    std::mutex mutex_;

    // Helper: Check if value is visible to transaction
    bool isVisible(const ValueLocation& loc, uint64_t current_tx_id);
};

} // namespace scratchbird::core
```

**MGA Considerations:**
- Each value location stores tx_id and tx_end
- Visibility determined by transaction snapshot
- Multiple versions of same value may exist (different transactions)
- Only committed, visible versions count for uniqueness

**Acceptance Criteria:**
- [ ] Index tracks values across all tables/columns
- [ ] checkUniqueness() respects MGA visibility
- [ ] Concurrent inserts handled correctly
- [ ] Updates handled (delete old, insert new)
- [ ] Deletes remove from index
- [ ] Performance acceptable (hash-based lookup)
- [ ] Full test coverage with concurrent transactions

**Test File:** `tests/unit/test_global_uniqueness_index.cpp` (NEW)

---

### Task 3.2: Domain Uniqueness Integration

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 10 hours

**File to Modify:** `include/scratchbird/core/domain_manager.h`
**File to Modify:** `src/core/domain_manager.cpp`

**Requirements:**
1. Store uniqueness settings in DomainInfo
2. Hook into INSERT/UPDATE to enforce uniqueness
3. Integration with GlobalUniquenessIndex

**Implementation Details:**

**DomainInfo Extensions:**
```cpp
struct DomainInfo {
    // ... existing fields ...

    // NEW: Uniqueness settings (from WITH INTEGRITY)
    bool enforce_global_uniqueness = false;
};
```

**DomainManager Extensions:**
```cpp
class DomainManager {
    // ... existing methods ...

    // NEW: Uniqueness support
    Status checkGlobalUniqueness(const ID& domain_id,
                                const TypedValue& value,
                                uint64_t tx_id,
                                bool& is_unique_out,
                                ErrorContext* ctx);

    Status registerUniqueValue(const ID& domain_id,
                              const ID& table_id,
                              const ID& column_id,
                              const ID& row_id,
                              const TypedValue& value,
                              uint64_t tx_id,
                              ErrorContext* ctx);

private:
    GlobalUniquenessIndex* uniqueness_index_;
};
```

**Acceptance Criteria:**
- [ ] DomainInfo stores uniqueness setting
- [ ] checkGlobalUniqueness() calls index
- [ ] registerUniqueValue() updates index
- [ ] Executor integration for INSERT/UPDATE
- [ ] Error message on uniqueness violation clear
- [ ] Full test coverage

**Test File:** Add to `tests/integration/test_domain_integrity.cpp`

---

## SECTION 4: NORMALIZATION INFRASTRUCTURE

### Task 4.1: Built-in Normalization Functions

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 8 hours

**File to Create:** `include/scratchbird/core/normalization.h`
**File to Create:** `src/core/normalization.cpp`

**Requirements:**
1. LOWERCASE normalization
2. UPPERCASE normalization
3. TRIM normalization
4. Combined normalization (TRIM_LOWERCASE, TRIM_UPPERCASE)

**Implementation Details:**

```cpp
namespace scratchbird::core {

enum class NormalizationType : uint8_t {
    NONE = 0,
    LOWERCASE = 1,
    UPPERCASE = 2,
    TRIM = 3,
    TRIM_LOWERCASE = 4,
    TRIM_UPPERCASE = 5,
    CUSTOM_FUNCTION = 99  // User-defined function
};

struct NormalizationConfig {
    NormalizationType type = NormalizationType::NONE;
    std::string custom_function_name;  // If type == CUSTOM_FUNCTION
};

class Normalization {
public:
    /**
     * Apply normalization to value
     *
     * @param value Original value
     * @param config Normalization configuration
     * @param normalized_out [out] Normalized value
     * @param ctx Error context
     * @return Status::OK on success
     */
    static Status applyNormalization(
        const TypedValue& value,
        const NormalizationConfig& config,
        TypedValue& normalized_out,
        ErrorContext* ctx);

private:
    static Status applyLowercase(const TypedValue& value,
                                TypedValue& normalized_out,
                                ErrorContext* ctx);

    static Status applyUppercase(const TypedValue& value,
                                TypedValue& normalized_out,
                                ErrorContext* ctx);

    static Status applyTrim(const TypedValue& value,
                           TypedValue& normalized_out,
                           ErrorContext* ctx);

    static Status applyTrimLowercase(const TypedValue& value,
                                    TypedValue& normalized_out,
                                    ErrorContext* ctx);

    static Status applyTrimUppercase(const TypedValue& value,
                                    TypedValue& normalized_out,
                                    ErrorContext* ctx);
};

} // namespace scratchbird::core
```

**Acceptance Criteria:**
- [ ] LOWERCASE converts correctly (Unicode-aware)
- [ ] UPPERCASE converts correctly (Unicode-aware)
- [ ] TRIM removes leading/trailing whitespace
- [ ] Combined normalization works
- [ ] NULL values handled correctly
- [ ] Full test coverage with Unicode

**Test File:** `tests/unit/test_normalization.cpp` (NEW)

---

### Task 4.2: Custom Normalization Function Support

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 6 hours

**File to Modify:** `include/scratchbird/core/normalization.h`
**File to Modify:** `src/core/normalization.cpp`

**Requirements:**
1. Call user-defined functions for normalization
2. Function signature: `TypedValue normalize(TypedValue input)`
3. Integration with function execution infrastructure

**Implementation Details:**

**Normalization Extensions:**
```cpp
class Normalization {
    // ... existing methods ...

    /**
     * Apply custom normalization function
     *
     * @param value Original value
     * @param function_name Function to call
     * @param executor Executor for function calls
     * @param normalized_out [out] Normalized value
     * @param ctx Error context
     * @return Status::OK on success
     */
    static Status applyCustomFunction(
        const TypedValue& value,
        const std::string& function_name,
        class Executor* executor,
        TypedValue& normalized_out,
        ErrorContext* ctx);
};
```

**Acceptance Criteria:**
- [ ] Custom function called correctly
- [ ] Function signature validated
- [ ] Return value type checked
- [ ] Error handling for function failures
- [ ] Full test coverage

**Test File:** Add to `tests/unit/test_normalization.cpp`

---

### Task 4.3: Domain Normalization Integration

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 6 hours

**File to Modify:** `include/scratchbird/core/domain_manager.h`
**File to Modify:** `src/core/domain_manager.cpp`

**Requirements:**
1. Store normalization settings in DomainInfo
2. Apply normalization on INSERT/UPDATE BEFORE constraints
3. Integration with executor

**Implementation Details:**

**DomainInfo Extensions:**
```cpp
struct DomainInfo {
    // ... existing fields ...

    // NEW: Normalization settings (from WITH INTEGRITY)
    NormalizationConfig normalization_config;
};
```

**DomainManager Extensions:**
```cpp
class DomainManager {
    // ... existing methods ...

    // NEW: Normalization support
    Status applyNormalization(const ID& domain_id,
                             TypedValue& value,
                             Executor* executor,
                             ErrorContext* ctx);
};
```

**Execution Order:**
1. Normalization (FIRST)
2. Global uniqueness check
3. CHECK constraints
4. Validation function

**Acceptance Criteria:**
- [ ] DomainInfo stores normalization config
- [ ] applyNormalization() applies before constraints
- [ ] Executor integration works
- [ ] Full test coverage

**Test File:** Add to `tests/integration/test_domain_integrity.cpp`

---

## SECTION 5: VALIDATION INFRASTRUCTURE

### Task 5.1: Custom Validation Function Integration

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 10 hours

**File to Create:** `include/scratchbird/core/domain_validation.h`
**File to Create:** `src/core/domain_validation.cpp`

**Requirements:**
1. Call user-defined validation functions
2. Function signature: `bool validate(TypedValue value)`
3. Custom error messages on failure
4. Integration with CHECK constraint system

**Implementation Details:**

```cpp
namespace scratchbird::core {

struct ValidationConfig {
    std::string function_name;    // Validation function to call
    std::string error_message;    // Custom error message on failure
};

class DomainValidation {
public:
    /**
     * Validate value using custom function
     *
     * @param value Value to validate
     * @param config Validation configuration
     * @param executor Executor for function calls
     * @param is_valid_out [out] True if valid, false if invalid
     * @param ctx Error context
     * @return Status::OK on success (even if validation fails)
     *         Sets ctx with custom error message if validation fails
     */
    static Status validateValue(
        const TypedValue& value,
        const ValidationConfig& config,
        class Executor* executor,
        bool& is_valid_out,
        ErrorContext* ctx);

    /**
     * Create error context with custom message
     */
    static void setValidationError(const ValidationConfig& config,
                                   const TypedValue& value,
                                   ErrorContext* ctx);
};

} // namespace scratchbird::core
```

**Acceptance Criteria:**
- [ ] Validation function called correctly
- [ ] Return value (bool) checked
- [ ] Custom error message set on failure
- [ ] Integration with CHECK constraints
- [ ] NULL values handled (skip validation or validate?)
- [ ] Full test coverage

**Test File:** `tests/unit/test_domain_validation.cpp` (NEW)

---

### Task 5.2: Domain Validation Integration

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 6 hours

**File to Modify:** `include/scratchbird/core/domain_manager.h`
**File to Modify:** `src/core/domain_manager.cpp`

**Requirements:**
1. Store validation settings in DomainInfo
2. Execute validation on INSERT/UPDATE
3. Integration with executor

**Implementation Details:**

**DomainInfo Extensions:**
```cpp
struct DomainInfo {
    // ... existing fields ...

    // NEW: Validation settings (from WITH VALIDATION)
    ValidationConfig validation_config;
};
```

**DomainManager Extensions:**
```cpp
class DomainManager {
    // ... existing methods ...

    // NEW: Validation support
    Status validateValue(const ID& domain_id,
                        const TypedValue& value,
                        Executor* executor,
                        bool& is_valid_out,
                        ErrorContext* ctx);
};
```

**Execution Order:**
1. Normalization
2. Global uniqueness check
3. Validation function
4. CHECK constraints

**Acceptance Criteria:**
- [ ] DomainInfo stores validation config
- [ ] validateValue() calls validation function
- [ ] Custom error messages propagated
- [ ] Executor integration works
- [ ] Full test coverage

**Test File:** Add to `tests/integration/test_domain_validation.cpp`

---

## SECTION 6: QUALITY PIPELINE INFRASTRUCTURE

### Task 6.1: Quality Function Pipeline

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 14 hours

**File to Create:** `include/scratchbird/core/quality_pipeline.h`
**File to Create:** `src/core/quality_pipeline.cpp`

**Requirements:**
1. Execute PARSE → STANDARDIZE → ENRICH pipeline
2. Function chaining support
3. Error handling at each stage
4. Optional storage of enriched data

**Implementation Details:**

```cpp
namespace scratchbird::core {

struct QualityConfig {
    std::string parse_function;        // Parse and validate
    std::string standardize_function;  // Format consistently
    std::string enrich_function;       // Augment data
};

struct QualityResult {
    TypedValue parsed_value;
    TypedValue standardized_value;
    TypedValue enriched_value;
    std::map<std::string, TypedValue> metadata;  // Enrichment metadata
};

class QualityPipeline {
public:
    /**
     * Execute quality pipeline on value
     *
     * Pipeline stages:
     * 1. PARSE: Validate and extract components
     * 2. STANDARDIZE: Format consistently
     * 3. ENRICH: Augment with additional data
     *
     * @param value Original value
     * @param config Quality configuration
     * @param executor Executor for function calls
     * @param result_out [out] Pipeline result
     * @param ctx Error context
     * @return Status::OK on success
     */
    static Status executePipeline(
        const TypedValue& value,
        const QualityConfig& config,
        class Executor* executor,
        QualityResult& result_out,
        ErrorContext* ctx);

private:
    static Status executeParse(const TypedValue& value,
                              const std::string& function_name,
                              Executor* executor,
                              TypedValue& parsed_out,
                              ErrorContext* ctx);

    static Status executeStandardize(const TypedValue& value,
                                    const std::string& function_name,
                                    Executor* executor,
                                    TypedValue& standardized_out,
                                    ErrorContext* ctx);

    static Status executeEnrich(const TypedValue& value,
                               const std::string& function_name,
                               Executor* executor,
                               TypedValue& enriched_out,
                               std::map<std::string, TypedValue>& metadata_out,
                               ErrorContext* ctx);
};

} // namespace scratchbird::core
```

**Function Contracts:**

**PARSE_FUNCTION:**
- Input: Original value (string, etc.)
- Output: Parsed/validated value OR error
- Example: Parse phone number "555-1234" → validate format, extract area code

**STANDARDIZE_FUNCTION:**
- Input: Parsed value
- Output: Standardized format
- Example: Phone "(555) 123-4567" → "+1-555-123-4567" (E.164)

**ENRICH_FUNCTION:**
- Input: Standardized value
- Output: Enriched value + metadata
- Example: Phone "+1-555-123-4567" → lookup carrier, timezone, location

**Acceptance Criteria:**
- [ ] Pipeline executes stages in order
- [ ] Each stage can skip if function not specified
- [ ] Errors propagated with clear stage identification
- [ ] Metadata stored alongside enriched value
- [ ] Full test coverage with realistic examples

**Test File:** `tests/unit/test_quality_pipeline.cpp` (NEW)

---

### Task 6.2: Domain Quality Integration

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 8 hours

**File to Modify:** `include/scratchbird/core/domain_manager.h`
**File to Modify:** `src/core/domain_manager.cpp`

**Requirements:**
1. Store quality settings in DomainInfo
2. Execute quality pipeline on INSERT/UPDATE
3. Store enriched value and metadata

**Implementation Details:**

**DomainInfo Extensions:**
```cpp
struct DomainInfo {
    // ... existing fields ...

    // NEW: Quality settings (from WITH QUALITY)
    QualityConfig quality_config;
};
```

**DomainManager Extensions:**
```cpp
class DomainManager {
    // ... existing methods ...

    // NEW: Quality pipeline support
    Status executeQualityPipeline(const ID& domain_id,
                                 const TypedValue& value,
                                 Executor* executor,
                                 QualityResult& result_out,
                                 ErrorContext* ctx);
};
```

**Storage Considerations:**
- Store enriched_value as primary value
- Store metadata in separate TOAST area?
- Or store both original and enriched?

**Acceptance Criteria:**
- [ ] DomainInfo stores quality config
- [ ] executeQualityPipeline() runs pipeline
- [ ] Enriched value stored correctly
- [ ] Metadata accessible
- [ ] Full test coverage

**Test File:** Add to `tests/integration/test_domain_quality.cpp`

---

## SECTION 7: SBLR OPCODE EXTENSIONS

### Task 7.1: Define WITH Block Enforcement Opcodes

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 4 hours

**File to Modify:** `include/scratchbird/sblr/opcodes.h`
**File to Modify:** `docs/specifications/Appendix_A_SBLR_BYTECODE.md`

**Requirements:**
1. Define 10 new extended opcodes for domain enforcement
2. Document payload structures
3. Integration with existing EXT_CREATE_DOMAIN opcodes

**New Opcodes:**

```cpp
namespace scratchbird::sblr {

// Domain constraint enforcement (extend from 0x0204)
constexpr uint16_t EXT_CHECK_DOMAIN_CONSTRAINT    = 0x0204;  // Check all domain constraints
constexpr uint16_t EXT_APPLY_DOMAIN_MASKING       = 0x0205;  // Apply masking (SELECT)
constexpr uint16_t EXT_ENCRYPT_DOMAIN_VALUE       = 0x0206;  // Encrypt value (INSERT/UPDATE)
constexpr uint16_t EXT_DECRYPT_DOMAIN_VALUE       = 0x0207;  // Decrypt value (SELECT)
constexpr uint16_t EXT_AUDIT_DOMAIN_ACCESS        = 0x0208;  // Log domain access
constexpr uint16_t EXT_CHECK_DOMAIN_PRIVILEGE     = 0x0209;  // Check privilege
constexpr uint16_t EXT_NORMALIZE_DOMAIN_VALUE     = 0x020A;  // Apply normalization
constexpr uint16_t EXT_VALIDATE_DOMAIN_VALUE      = 0x020B;  // Custom validation
constexpr uint16_t EXT_APPLY_QUALITY_PIPELINE     = 0x020C;  // Quality pipeline
constexpr uint16_t EXT_CHECK_GLOBAL_UNIQUENESS    = 0x020D;  // Global uniqueness

} // namespace scratchbird::sblr
```

**Payload Structures:**

```
EXT_CHECK_DOMAIN_CONSTRAINT:
[domain_id:16] [value_stack_offset:16]
Result: Push bool (all constraints passed)

EXT_APPLY_DOMAIN_MASKING:
[domain_id:16] [user_id:16] [value_stack_offset:16]
Result: Replace stack value with masked value

EXT_ENCRYPT_DOMAIN_VALUE:
[domain_id:16] [value_stack_offset:16]
Result: Replace stack value with encrypted value

EXT_DECRYPT_DOMAIN_VALUE:
[domain_id:16] [value_stack_offset:16]
Result: Replace stack value with decrypted value

EXT_AUDIT_DOMAIN_ACCESS:
[domain_id:16] [user_id:16] [table_id:16] [column_id:16]
Result: Log audit event

EXT_CHECK_DOMAIN_PRIVILEGE:
[domain_id:16] [user_id:16]
Result: Push bool (has privilege)

EXT_NORMALIZE_DOMAIN_VALUE:
[domain_id:16] [value_stack_offset:16]
Result: Replace stack value with normalized value

EXT_VALIDATE_DOMAIN_VALUE:
[domain_id:16] [value_stack_offset:16]
Result: Push bool (validation passed), set error context if failed

EXT_APPLY_QUALITY_PIPELINE:
[domain_id:16] [value_stack_offset:16]
Result: Replace stack value with enriched value

EXT_CHECK_GLOBAL_UNIQUENESS:
[domain_id:16] [table_id:16] [column_id:16] [row_id:16] [value_stack_offset:16]
Result: Push bool (is unique)
```

**Acceptance Criteria:**
- [ ] All 10 opcodes defined
- [ ] Payload structures documented
- [ ] No conflicts with existing opcodes
- [ ] Documentation complete

**Test File:** Documentation only, no code changes

---

### Task 7.2: Implement Opcode Handlers in Executor

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 16 hours

**File to Modify:** `src/sblr/executor.cpp`
**File to Modify:** `include/scratchbird/sblr/executor.h`

**Requirements:**
1. Implement handler for each new opcode
2. Integration with domain infrastructure
3. Proper error handling

**Implementation Details:**

Add to `Executor::executeExtendedOpcode()`:

```cpp
case EXT_CHECK_DOMAIN_CONSTRAINT: {
    ID domain_id = readID();
    uint16_t value_offset = readUInt16();
    TypedValue& value = stack_[value_offset];

    bool passed = db_->domain_manager()->checkAllConstraints(
        domain_id, value, tx_id_, &ctx);
    stack_.push_back(TypedValue::fromBool(passed));
    break;
}

case EXT_APPLY_DOMAIN_MASKING: {
    ID domain_id = readID();
    ID user_id = readID();
    uint16_t value_offset = readUInt16();
    TypedValue& value = stack_[value_offset];

    Status status = db_->domain_manager()->applyMasking(
        domain_id, user_id, value, &ctx);
    if (status != Status::OK) return ExecutionResult(ctx.message);
    break;
}

case EXT_ENCRYPT_DOMAIN_VALUE: {
    ID domain_id = readID();
    uint16_t value_offset = readUInt16();
    TypedValue& value = stack_[value_offset];

    Status status = db_->domain_manager()->encryptValue(
        domain_id, value, &ctx);
    if (status != Status::OK) return ExecutionResult(ctx.message);
    break;
}

// ... similar for all 10 opcodes ...
```

**Acceptance Criteria:**
- [ ] All 10 opcode handlers implemented
- [ ] Proper integration with domain infrastructure
- [ ] Error handling for all failure cases
- [ ] Stack manipulation correct
- [ ] Full test coverage

**Test File:** `tests/unit/test_domain_opcodes.cpp` (NEW)

---

## SECTION 8: COMPREHENSIVE TESTING

### Task 8.1: Encryption Infrastructure Tests

**Status:** IN PROGRESS
**Priority:** HIGH
**Estimated Time:** 8 hours

**Test Files:**
- `tests/unit/test_encryption_key_manager.cpp`
- `tests/unit/test_data_encryption.cpp`
- `tests/unit/test_encrypted_storage.cpp`

**Test Coverage:**
- [x] Key generation and rotation
- [x] AES-256-GCM encryption/decryption
- [x] AES-128-GCM encryption/decryption
- [x] IV randomness
- [x] Authentication tag validation
- [ ] Storage format round-trip
- [ ] TOAST integration
- [ ] Concurrent encryption operations
- [x] NIST test vectors validation

---

### Task 8.2: Masking Infrastructure Tests

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**Test File:** `tests/unit/test_data_masking.cpp`

**Test Coverage:**
- [ ] NONE masking (passthrough)
- [ ] FULL masking (all hidden)
- [ ] PARTIAL masking with various patterns
- [ ] Pattern parsing edge cases
- [ ] Privilege bypass
- [ ] NULL value handling
- [ ] Unicode support

---

### Task 8.3: Uniqueness Infrastructure Tests

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 8 hours

**Test File:** `tests/unit/test_global_uniqueness_index.cpp`

**Test Coverage:**
- [ ] Single value uniqueness
- [ ] Duplicate detection
- [ ] MGA transaction visibility
- [ ] Concurrent inserts
- [ ] Update handling
- [ ] Delete handling
- [ ] Multiple domains
- [ ] Performance stress test

---

### Task 8.4: Normalization Infrastructure Tests

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 4 hours

**Test File:** `tests/unit/test_normalization.cpp`

**Test Coverage:**
- [ ] LOWERCASE normalization
- [ ] UPPERCASE normalization
- [ ] TRIM normalization
- [ ] Combined normalization
- [ ] Unicode handling
- [ ] Custom function calls
- [ ] NULL value handling

---

### Task 8.5: Validation Infrastructure Tests

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 4 hours

**Test File:** `tests/unit/test_domain_validation.cpp`

**Test Coverage:**
- [ ] Custom validation functions
- [ ] Error message propagation
- [ ] Integration with CHECK constraints
- [ ] NULL value handling
- [ ] Function signature validation

---

### Task 8.6: Quality Pipeline Tests

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 6 hours

**Test File:** `tests/unit/test_quality_pipeline.cpp`

**Test Coverage:**
- [ ] Parse stage execution
- [ ] Standardize stage execution
- [ ] Enrich stage execution
- [ ] Full pipeline chaining
- [ ] Error handling at each stage
- [ ] Metadata storage
- [ ] Realistic examples (phone, email, address)

---

### Task 8.7: Integration Tests for Domain WITH Blocks

**Status:** ❌ NOT STARTED
**Priority:** CRITICAL
**Estimated Time:** 12 hours

**Test Files:**
- `tests/integration/test_domain_security.cpp`
- `tests/integration/test_domain_integrity.cpp`
- `tests/integration/test_domain_validation.cpp`
- `tests/integration/test_domain_quality.cpp`

**Test Coverage:**

**WITH SECURITY:**
- [ ] Encryption on INSERT, decryption on SELECT
- [ ] Masking based on privilege
- [ ] Audit logging of domain access
- [ ] Privilege checking

**WITH INTEGRITY:**
- [ ] Global uniqueness across tables
- [ ] Auto-normalization on INSERT/UPDATE
- [ ] Custom normalization functions

**WITH VALIDATION:**
- [ ] Custom validation on INSERT/UPDATE
- [ ] Error messages
- [ ] Integration with CHECK

**WITH QUALITY:**
- [ ] Parse→Standardize→Enrich pipeline
- [ ] Metadata storage
- [ ] Realistic use cases

---

### Task 8.8: End-to-End Scenario Tests

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 8 hours

**Test File:** `tests/integration/test_domain_e2e_scenarios.cpp`

**Scenarios:**
- [ ] SSN domain with encryption + masking + audit
- [ ] Email domain with normalization + validation + quality
- [ ] Username domain with global uniqueness + normalization
- [ ] Phone domain with full quality pipeline
- [ ] Credit card with encryption + masking + validation
- [ ] Multiple domains in single table
- [ ] Domain inheritance with WITH blocks

---

## MASTER CHECKLIST SUMMARY

### Infrastructure Components (35 tasks)

**Encryption (4 tasks):**
- [x] Task 1.1: Encryption key management
- [x] Task 1.2: AES encryption/decryption
- [x] Task 1.3: Storage layer integration
- [x] Task 1.4: Domain encryption integration

**Masking (2 tasks):**
- [x] Task 2.1: Masking engine
- [x] Task 2.2: Domain masking integration

**Global Uniqueness (2 tasks):**
- [ ] Task 3.1: Global uniqueness index
- [ ] Task 3.2: Domain uniqueness integration

**Normalization (3 tasks):**
- [ ] Task 4.1: Built-in normalization
- [ ] Task 4.2: Custom normalization functions
- [ ] Task 4.3: Domain normalization integration

**Validation (2 tasks):**
- [ ] Task 5.1: Custom validation integration
- [ ] Task 5.2: Domain validation integration

**Quality Pipeline (2 tasks):**
- [ ] Task 6.1: Quality function pipeline
- [ ] Task 6.2: Domain quality integration

**SBLR Extensions (2 tasks):**
- [ ] Task 7.1: Define WITH block opcodes
- [ ] Task 7.2: Implement opcode handlers

**Testing (8 tasks):**
- [ ] Task 8.1: Encryption tests
- [x] Task 8.2: Masking tests
- [ ] Task 8.3: Uniqueness tests
- [ ] Task 8.4: Normalization tests
- [ ] Task 8.5: Validation tests
- [ ] Task 8.6: Quality pipeline tests
- [ ] Task 8.7: Integration tests
- [ ] Task 8.8: End-to-end scenarios

**Testing (10 tasks total, including subcategories):**
- Unit tests: 6 tasks
- Integration tests: 3 tasks
- End-to-end: 1 task

---

## TOTAL TASK COUNT: 35 TASKS

**Estimated Total Time:**
- Single Developer: 138-192 hours (17-24 working days)
- Three Developers (parallel): 60-84 hours (7.5-10.5 working days)

---

## CRITICAL PATH

### Week 1 (Parallel Streams):
**Stream 1 - Encryption:**
- Task 1.1: Key management
- Task 1.2: AES implementation
- Task 1.3: Storage integration
- Task 1.4: Domain integration

**Stream 2 - Data Integrity:**
- Task 3.1: Global uniqueness index
- Task 3.2: Domain uniqueness integration
- Task 4.1: Built-in normalization
- Task 4.2: Custom normalization
- Task 4.3: Domain normalization integration

**Stream 3 - Validation & Quality:**
- Task 5.1: Custom validation
- Task 5.2: Domain validation integration
- Task 6.1: Quality pipeline
- Task 6.2: Domain quality integration

### Week 2:
**Stream 1:**
- Task 2.1: Masking engine
- Task 2.2: Domain masking integration
- Task 7.1: SBLR opcodes
- Task 8.1: Encryption tests

**Stream 2:**
- Task 8.2: Masking tests
- Task 8.3: Uniqueness tests
- Task 8.4: Normalization tests

**Stream 3:**
- Task 8.5: Validation tests
- Task 8.6: Quality pipeline tests
- Task 7.2: Opcode handlers (depends on all infrastructure)

### Week 3 (Integration):
- Task 8.7: Integration tests (all streams)
- Task 8.8: End-to-end scenarios

---

## DEPENDENCIES

### Plan 04 Dependencies:
After Plan 03B completion, Plan 04 can reference:
- ✅ `EncryptionKeyManager` - for key generation
- ✅ `DataEncryption` - for encryption/decryption
- ✅ `DataMasking` - for masking
- ❌ `GlobalUniquenessIndex` - for uniqueness
- ❌ `Normalization` - for normalization
- ❌ `DomainValidation` - for validation
- ❌ `QualityPipeline` - for quality
- ❌ All 10 SBLR opcodes - for bytecode generation

### External Dependencies:
- OpenSSL library (for AES-GCM)
- Function execution infrastructure (verify completeness)

---

## VERIFICATION CHECKLIST

Before Plan 04 can proceed:
- [ ] All 35 tasks completed
- [ ] All unit tests passing
- [ ] All integration tests passing
- [ ] All end-to-end scenarios passing
- [ ] Performance benchmarks met
- [ ] Documentation complete
- [ ] Code review complete
- [ ] MGA compliance verified

---

## SUCCESS CRITERIA

Plan 03B is complete when:
1. ✅ All infrastructure components implemented
2. ✅ All tests passing (unit + integration + e2e)
3. ✅ SBLR opcodes defined and implemented
4. ✅ Documentation complete
5. ✅ Performance acceptable
6. ✅ MGA compliance verified
7. ✅ Ready for Plan 04 parser implementation

---

## NEXT STEPS

1. **User Review:** Review this specification for completeness
2. **AI Review:** Have another AI review for missing details
3. **Approval:** Get user approval to proceed
4. **Implementation:** Begin Plan 03B implementation
5. **Testing:** Comprehensive testing throughout
6. **Handoff:** Complete Plan 03B before starting Plan 04

---

**END OF SPECIFICATION**
