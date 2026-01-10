# Plan 04 Prerequisites - WITH Block Infrastructure

**Version:** 1.2
**Date:** 2026-01-03 (Updated)
**Status:** ✅ COMPLETE - Plan 03B infrastructure implemented; Plan 02B alignment/testing remains

---

## Executive Summary

This document identifies the infrastructure components required for Plan 04 (Domain DDL). The WITH blocks (SECURITY, INTEGRITY, VALIDATION, QUALITY) depend on this infrastructure and are now implemented via Plan 03B.

**Previous blocker resolved (2025-12-31):** Schema/Database DDL opcodes and handlers are implemented. Remaining work is path alignment, cascade semantics, and tests (see "Resolved Blocker" section below).

**Status Update:** Plan 03B infrastructure is complete; Plan 04 can proceed with full WITH block support.

---

## ✅ RESOLVED: Schema/Database DDL Infrastructure

**Discovered:** 2025-12-26
**Severity:** BLOCKS Plan 04 AND All Emulation
**Status:** CORE IMPLEMENTATION COMPLETE (alignment/testing remaining)

### Problem

Domains are **schema-scoped**, and the required schema/database DDL infrastructure is now in place:

- ✅ EXT_CREATE/EXT_DROP/EXT_ALTER SCHEMA and DATABASE opcodes added.
- ✅ PostgreSQL parser emits CREATE/DROP/ALTER SCHEMA/DATABASE opcodes.
- ✅ MySQL parser implements CREATE/DROP/ALTER DATABASE (SCHEMA synonym).
- ✅ Firebird parser builds emulated DB paths for CREATE/DROP/ALTER DATABASE (RENAME only).
- ✅ Executor handlers implemented for schema/database DDL.
- ✅ Emulation view generator updated and wired into CREATE/DROP DATABASE.
- ✅ Canonical emulation path resolved to `remote.emulated.<dialect>.<server>.<db>` (dot-path normalized).

**Remaining gaps:**
- DROP SCHEMA/DATABASE cascade semantics (CatalogManager dropSchema is RESTRICT-only).
- Adapter/query compiler default schema path alignment (slash-path defaults remain).
- Dedicated unit/integration tests for schema/database DDL and emulated view generation.

### Impact on Plan 04

1. **Domain testing unblocked** - Schema/database DDL opcodes and handlers now exist.
2. **Emulated parser DDL unblocked** - Parsers emit valid schema/database opcodes.
3. **Path alignment still needed** - Adapters/compilers must converge on dot-path defaults.

### Dependencies

**Plan 04 now depends on:**
- ✅ Plan 03B (WITH block infrastructure) - implementation complete
- ⚠️ **Plan 02B (Schema/Database DDL)** - core complete; remaining alignment/testing
  - See `docs/archive/2026-01-04/planning/PLAN_02B_SCHEMA_DATABASE_DDL.md` for status

**Prerequisite work:** COMPLETE (Plan 03B). Remaining work is Plan 02B alignment + verification.

### Next Steps

1. Track remaining Plan 02B alignment items (cascade semantics, adapter path alignment, tests).
2. Maintain verification coverage for domain infrastructure and WITH blocks.

**Detailed Analysis:** See `/docs/archive/2026-01-09/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md`

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

**Current Status:** ✅ COMPLETE (2026-01-01)

**Implemented:**
- GlobalUniquenessIndex + DomainManager integration
- MGA-aware uniqueness checks in executor (INSERT/UPDATE/DELETE)
- Unit + integration coverage for uniqueness paths

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
EXT_CREATE_DOMAIN = 0x005C
EXT_ALTER_DOMAIN  = 0x010E
EXT_DROP_DOMAIN   = 0x010F
EXT_SHOW_DOMAIN   = 0x0064
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

Plan 03B delivered the prerequisites in parallel workstreams:

**Stream 1 - Security:** Encryption, masking, privilege integration
**Stream 2 - Data Integrity:** Global uniqueness, normalization
**Stream 3 - Validation & Quality:** Validation integration, quality pipeline
**Stream 4 - Infrastructure:** SBLR opcodes for WITH blocks

**Implementation Status:** COMPLETE (see `docs/archive/2026-01-04/planning/PLAN_03B_DOMAIN_INFRASTRUCTURE.md`)

---

## Verification Checklist

Before Plan 04 can proceed, verify:

- [x] **Encryption:** AES-256/128-GCM encryption/decryption implemented
- [x] **Key Management:** Secure key generation, storage, and rotation
- [x] **Masking:** PARTIAL/FULL masking with pattern support
- [x] **Uniqueness:** Global uniqueness index and enforcement
- [x] **Normalization:** Built-in + custom function support
- [x] **Validation:** Custom validation function integration
- [x] **Quality:** Parse→Standardize→Enrich pipeline
- [x] **SBLR Opcodes:** All 10 WITH block opcodes defined
- [ ] **Testing:** Coverage present; expand unit coverage for key mgmt/masking edge cases
- [ ] **Documentation:** Implementation guides for each component (partial)

---

## Outcome

- Option 1 (implement all prerequisites first) executed.
- WITH block infrastructure is complete; Plan 04 can proceed with full WITH support.

---

## Remaining Follow-ups

1. Track Plan 02B alignment (path defaults, cascade semantics, tests).
2. Expand unit coverage for domain infrastructure edge cases (masking/encryption/key mgmt).
3. Finish documentation guides for domain infrastructure components.
