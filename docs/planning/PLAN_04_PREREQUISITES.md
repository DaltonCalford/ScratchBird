# Plan 04 Prerequisites - WITH Block Infrastructure

**Version:** 1.0
**Date:** 2025-12-22
**Status:** ANALYSIS COMPLETE - AWAITING USER REVIEW

---

## Executive Summary

This document identifies all infrastructure components that MUST exist before Plan 04 (Domain DDL) can be implemented. The WITH blocks (SECURITY, INTEGRITY, VALIDATION, QUALITY) require significant underlying infrastructure for full Alpha implementation.

**CRITICAL:** These prerequisites must be completed BEFORE Plan 04 implementation begins, or Plan 04 must be scoped to exclude WITH blocks until prerequisites are ready.

---

## Infrastructure Status

### ✅ COMPLETE - Already Exists

#### 1. Audit Logging (`core/audit_logger.h`)
- **Status:** FULLY IMPLEMENTED
- **Used By:** WITH SECURITY (AUDIT_ACCESS)
- **Features:**
  - Sequential event IDs for tamper detection
  - Asynchronous buffered writes
  - Catalog integration for persistence
  - Query API for forensics
  - Thread-safe operation
- **Events Supported:**
  - LOGIN_SUCCESS/FAILURE, LOGOUT
  - PERMISSION_GRANTED/DENIED
  - USER_CREATED/MODIFIED/DELETED
  - DDL operations
  - DATA access events (RLS_VIOLATION, COLUMN_ACCESS_DENIED)
- **Readiness:** ✅ READY FOR USE

#### 2. Authentication & Authorization (`security/auth_manager.h`)
- **Status:** FULLY IMPLEMENTED
- **Used By:** WITH SECURITY (REQUIRE_PRIVILEGE)
- **Features:**
  - Host-Based Authentication (HBA) rules
  - Authentication method dispatch
  - Rate limiting
  - User credential management
  - Role-based access control
- **Readiness:** ✅ READY FOR USE

#### 3. Permission Caching (`core/permission_cache.h`)
- **Status:** FULLY IMPLEMENTED
- **Used By:** WITH SECURITY (privilege checking)
- **Features:**
  - LRU eviction with TTL expiration (10 seconds)
  - Thread-safe with shared_mutex
  - Cache invalidation on GRANT/REVOKE
  - VERIFIED mode for security-critical operations
- **Readiness:** ✅ READY FOR USE

#### 4. Function Execution Infrastructure (`sblr/executor.cpp`)
- **Status:** IMPLEMENTED
- **Used By:** WITH VALIDATION, WITH QUALITY, WITH INTEGRITY
- **Features:**
  - executeFunction() exists in executor (line 20073)
  - Function call mechanism in place
- **Needs Verification:**
  - Custom user-defined function support complete?
  - Function registry and resolution?
- **Readiness:** ⚠️ NEEDS VERIFICATION

---

## ❌ MISSING - Must Be Implemented

### 1. Data-at-Rest Encryption Infrastructure

**Required For:** WITH SECURITY (ENCRYPTION = AES256/AES128)

**Current Status:**
- ❌ NOT IMPLEMENTED
- Found TLS encryption (tls_context.cpp) but NOT data-at-rest encryption
- No AES encryption/decryption for column values

**Requirements:**
1. **Encryption Key Management**
   - Key generation and storage
   - Key rotation support
   - Secure key storage in system catalog
   - Per-domain or per-column key isolation

2. **Encryption/Decryption Layer**
   - AES-256-GCM encryption
   - AES-128-GCM encryption
   - Integration with storage layer
   - Transparent encryption on INSERT/UPDATE
   - Transparent decryption on SELECT

3. **Storage Format**
   - Encrypted value storage format
   - IV (Initialization Vector) storage
   - Authentication tag storage
   - Length prefix for variable-length data

4. **Performance Considerations**
   - Caching of decrypted values (with TTL)
   - Batch encryption/decryption
   - Hardware acceleration support (AES-NI)

**Estimated Effort:** 40-60 hours

**Dependencies:**
- Cryptography library (OpenSSL/BoringSSL/libsodium)
- Storage layer modifications
- System catalog extensions for key storage

---

### 2. Data Masking Infrastructure

**Required For:** WITH SECURITY (MASKING = PARTIAL/FULL, MASK_PATTERN)

**Current Status:** ❌ NOT IMPLEMENTED

**Requirements:**
1. **Masking Types**
   - NONE: No masking (default)
   - PARTIAL: Show subset based on pattern (e.g., "XXX-XX-1234")
   - FULL: Hide all data (e.g., "***********")

2. **Pattern Processing**
   - Parse MASK_PATTERN (e.g., "XXX-XX-####")
   - Apply pattern during SELECT
   - Respect privilege-based masking (show full if privileged)

3. **Integration Points**
   - SELECT result set processing
   - Permission-aware masking
   - Client protocol integration

**Estimated Effort:** 16-24 hours

**Dependencies:**
- Permission system (already exists)
- Result set processing modifications

---

### 3. Global Uniqueness Tracking

**Required For:** WITH INTEGRITY (UNIQUENESS = TRUE)

**Current Status:** ❌ NOT IMPLEMENTED

**Requirements:**
1. **Cross-Table Uniqueness Index**
   - Global index for domain values
   - Track all columns using domain
   - Enforce uniqueness across ALL tables

2. **Index Structure**
   - Domain ID → Set of unique values
   - Fast lookup for uniqueness checking
   - Update on INSERT/UPDATE/DELETE
   - Handle concurrent transactions (MGA)

3. **Constraint Enforcement**
   - Check uniqueness before INSERT/UPDATE
   - Return appropriate error on violation
   - Transaction-aware visibility

**Estimated Effort:** 30-40 hours

**Dependencies:**
- Index infrastructure (exists)
- MGA transaction visibility
- DomainManager integration

---

### 4. Normalization Infrastructure

**Required For:** WITH INTEGRITY (NORMALIZATION, NORMALIZATION_FUNCTION)

**Current Status:** ⚠️ PARTIAL - Function infrastructure exists, normalization logic missing

**Requirements:**
1. **Built-in Normalization**
   - LOWERCASE: Auto-convert to lowercase
   - UPPERCASE: Auto-convert to uppercase
   - TRIM: Remove leading/trailing whitespace
   - TRIM_LOWERCASE: Combine TRIM + LOWERCASE
   - TRIM_UPPERCASE: Combine TRIM + UPPERCASE

2. **Custom Normalization Functions**
   - Support NORMALIZATION_FUNCTION = 'function_name'
   - Function must accept VALUE and return normalized VALUE
   - Execute during INSERT/UPDATE BEFORE constraint checking

3. **Integration**
   - Hook into INSERT/UPDATE pipeline
   - Apply normalization before CHECK constraints
   - Before uniqueness checking

**Estimated Effort:** 12-16 hours

**Dependencies:**
- Function execution infrastructure (exists)
- DomainManager modifications

---

### 5. Custom Validation Function Infrastructure

**Required For:** WITH VALIDATION (FUNCTION, ERROR_MESSAGE)

**Current Status:** ⚠️ PARTIAL - Function infrastructure exists, validation integration missing

**Requirements:**
1. **Validation Function Protocol**
   - Function signature: bool validate(value) or similar
   - Return true for valid, false for invalid
   - Integration with CHECK constraint system

2. **Custom Error Messages**
   - Store ERROR_MESSAGE in domain definition
   - Return custom message on validation failure
   - Include in transaction error context

3. **Integration**
   - Execute validation on INSERT/UPDATE
   - Before CHECK constraints
   - Proper transaction rollback on failure

**Estimated Effort:** 12-16 hours

**Dependencies:**
- Function execution infrastructure (exists)
- Error context system (exists)

---

### 6. Quality Function Pipeline

**Required For:** WITH QUALITY (PARSE_FUNCTION, STANDARDIZE_FUNCTION, ENRICH_FUNCTION)

**Current Status:** ⚠️ PARTIAL - Function infrastructure exists, pipeline missing

**Requirements:**
1. **Function Pipeline Execution**
   - Execute in order: PARSE → STANDARDIZE → ENRICH
   - Pass output of one as input to next
   - Handle errors at each stage gracefully

2. **Function Contracts**
   - PARSE_FUNCTION: Validate and extract components
   - STANDARDIZE_FUNCTION: Format consistently
   - ENRICH_FUNCTION: Augment data with additional info

3. **Storage**
   - Store final enriched value
   - Option to store both original and enriched?
   - Metadata storage for enrichment results

**Estimated Effort:** 20-28 hours

**Dependencies:**
- Function execution infrastructure (exists)
- Storage layer modifications

---

## SBLR Opcode Requirements

### Current SBLR Opcodes

The following opcodes are already defined in `PLAN_04_IMPLEMENTATION_CHECKLIST.md`:

```cpp
// Domain DDL
EXT_CREATE_DOMAIN = 0x0200
EXT_ALTER_DOMAIN  = 0x0201
EXT_DROP_DOMAIN   = 0x0202
EXT_SHOW_DOMAIN   = 0x0203
```

### NEW Opcodes Required for WITH Blocks

**MISSING - Must be added to Section 1.3:**

```cpp
// Domain constraint enforcement
EXT_CHECK_DOMAIN_CONSTRAINT    = 0x0204  // Check domain constraint on value
EXT_APPLY_DOMAIN_MASKING       = 0x0205  // Apply masking to value (SELECT)
EXT_ENCRYPT_DOMAIN_VALUE       = 0x0206  // Encrypt value (INSERT/UPDATE)
EXT_DECRYPT_DOMAIN_VALUE       = 0x0207  // Decrypt value (SELECT)
EXT_AUDIT_DOMAIN_ACCESS        = 0x0208  // Log domain value access
EXT_CHECK_DOMAIN_PRIVILEGE     = 0x0209  // Check required privilege
EXT_NORMALIZE_DOMAIN_VALUE     = 0x020A  // Apply normalization
EXT_VALIDATE_DOMAIN_VALUE      = 0x020B  // Custom validation function
EXT_APPLY_QUALITY_PIPELINE     = 0x020C  // Execute quality function pipeline
EXT_CHECK_GLOBAL_UNIQUENESS    = 0x020D  // Check global uniqueness
```

**Estimated Effort:** 8 hours (specification + implementation)

---

## Dependency Analysis

### Critical Path

```
Prerequisites Flow:
1. Function Infrastructure (verify complete) → 0 hours if complete
2. Encryption Infrastructure → 40-60 hours
3. Masking Infrastructure → 16-24 hours
4. Global Uniqueness → 30-40 hours
5. Normalization → 12-16 hours
6. Validation Integration → 12-16 hours
7. Quality Pipeline → 20-28 hours
8. SBLR Opcodes → 8 hours

TOTAL: 138-192 hours (17-24 working days for single developer)
```

### Parallel Workstreams

These can be implemented in parallel by multiple developers:

**Stream 1 - Security (60-84 hours):**
- Encryption Infrastructure
- Masking Infrastructure
- Privilege Integration (already exists)

**Stream 2 - Data Integrity (42-56 hours):**
- Global Uniqueness
- Normalization

**Stream 3 - Validation & Quality (32-44 hours):**
- Validation Integration
- Quality Pipeline

**Stream 4 - Infrastructure (8 hours):**
- SBLR Opcodes

**Parallel Estimate:** 60-84 hours (7.5-10.5 working days with 3 developers)

---

## Verification Checklist

Before Plan 04 can proceed, verify:

- [ ] **Encryption:** AES-256/128-GCM encryption/decryption implemented
- [ ] **Key Management:** Secure key generation, storage, and rotation
- [ ] **Masking:** PARTIAL/FULL masking with pattern support
- [ ] **Uniqueness:** Global uniqueness index and enforcement
- [ ] **Normalization:** Built-in + custom function support
- [ ] **Validation:** Custom validation function integration
- [ ] **Quality:** Parse→Standardize→Enrich pipeline
- [ ] **SBLR Opcodes:** All 10 WITH block opcodes defined
- [ ] **Testing:** Unit tests for each infrastructure component
- [ ] **Documentation:** Implementation guides for each component

---

## Decision Points

### Option 1: Implement ALL Prerequisites First (Recommended)

**Timeline:** 17-24 days (single dev) or 7.5-10.5 days (3 devs in parallel)
**Risk:** Low - Complete infrastructure before domains
**Benefit:** Full WITH block support in Plan 04 Alpha

### Option 2: Staged Implementation

**Phase 1 - Core Domains (Now):**
- Implement Plan 04 WITHOUT WITH blocks
- Basic domains (BASIC, RECORD, ENUM, SET, VARIANT)
- ALTER/DROP/SHOW

**Phase 2 - WITH SECURITY (Later):**
- Implement encryption + masking + auditing
- Add WITH SECURITY support

**Phase 3 - WITH INTEGRITY (Later):**
- Implement uniqueness + normalization
- Add WITH INTEGRITY support

**Phase 4 - WITH VALIDATION/QUALITY (Later):**
- Implement validation + quality pipelines
- Add WITH VALIDATION and WITH QUALITY

**Timeline:** Core now, WITH blocks deferred
**Risk:** Medium - violates NO DEFERRALS rule
**Benefit:** Faster initial delivery

### Option 3: Hybrid - Essential Infrastructure Only

Implement ONLY the infrastructure needed for most common use cases:
- ✅ Auditing (already exists)
- ✅ Privilege checking (already exists)
- ⚠️ Basic masking (PARTIAL only)
- ⚠️ Built-in normalization only (no custom functions)
- ❌ Skip encryption (defer to later)
- ❌ Skip quality pipeline (defer to later)

**Timeline:** ~40-60 hours
**Risk:** High - still violates NO DEFERRALS
**Benefit:** Reduced scope

---

## Recommendations

### For User Consideration:

1. **Review Existing Function Infrastructure**
   - Is UDF (User-Defined Function) support complete?
   - Can we call custom functions from domain constraints?
   - Need to audit function execution completeness

2. **Encryption Library Selection**
   - Which crypto library? (OpenSSL, BoringSSL, libsodium)
   - Hardware acceleration requirements?
   - FIPS compliance needed?

3. **Implementation Strategy**
   - Option 1 (All prerequisites first) - safest, adheres to NO DEFERRALS
   - Option 2 (Staged) - violates NO DEFERRALS, but pragmatic
   - Option 3 (Hybrid) - middle ground, still has deferrals

4. **Resource Allocation**
   - Single developer: 17-24 days
   - Three developers (parallel): 7.5-10.5 days
   - Available development capacity?

---

## Next Steps

**DECISION REQUIRED:**

1. Does user want to proceed with Option 1 (all prerequisites)?
2. Should we defer WITH blocks and implement core domains only?
3. Should we create a separate PRE-PLAN-04 for infrastructure?

**IF PROCEEDING WITH OPTION 1:**

1. Create `PLAN_03B_WITH_BLOCK_INFRASTRUCTURE.md`
2. Break down into tasks similar to Plan 04 structure
3. Implement prerequisites first
4. Then proceed with Plan 04 with full WITH support

**IF PROCEEDING WITH OPTION 2:**

1. Update `DDL_DOMAINS_COMPREHENSIVE.md` to mark WITH blocks as "Phase 2"
2. Update `PLAN_04_IMPLEMENTATION_CHECKLIST.md` to remove Tasks 10.9-10.12
3. Reduce task count back to 76 tasks
4. Implement core domains only
5. Schedule WITH blocks for later phase

---

## Conclusion

Plan 04 as currently specified requires **138-192 hours of prerequisite infrastructure** that does not currently exist. This must be implemented BEFORE Plan 04 or the scope must be reduced to exclude WITH blocks.

**Awaiting user decision on implementation strategy.**
