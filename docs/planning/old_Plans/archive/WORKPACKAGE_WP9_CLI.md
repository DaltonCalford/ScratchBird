# Work Package 9: CLI Tools

**Status:** NOT STARTED
**Priority:** P0-P3 Mixed
**Estimated Hours:** 24-32
**Files:** src/cli/sb_*.cpp

---

## Overview

The four CLI tools (sb_verify, sb_backup, sb_security, sb_isql) have various incomplete features, missing implementations, and potential compilation issues.

---

## Tasks

### CLI-1: sb_verify - isValidAlphaPageSize (HIGH)
**File:** src/cli/sb_verify.cpp
**Line:** 187
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
if (!isValidAlphaPageSize(header.page_size)) {
    // Function not defined in this file
}
```

**Required Changes:**
Either:
A. Include header that defines this function
B. Define function locally:
```cpp
bool isValidAlphaPageSize(uint32_t size) {
    return size == 4096 || size == 8192 || size == 16384 || size == 32768;
}
```

**Verification:**
- [ ] sb_verify compiles
- [ ] Correctly validates page sizes

---

### CLI-2: sb_verify - validatePageChecksum (HIGH)
**File:** src/cli/sb_verify.cpp
**Line:** 277
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
if (!validatePageChecksum(page_buffer.data(), page_size)) {
    // Function not defined
}
```

**Required Changes:**
Either:
A. Include header from core (ondisk.h?)
B. Define locally using CRC32C

**Verification:**
- [ ] sb_verify compiles
- [ ] Correctly validates checksums

---

### CLI-3: sb_verify - --repair flag (HIGH)
**File:** src/cli/sb_verify.cpp
**Lines:** 435-436
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// --repair flag parsed but never used
g_config.repair = true;  // Stored but ignored
```

**Required Changes:**
1. When repair mode enabled:
   - Fix corrupted page checksums
   - Rebuild damaged B-tree nodes
   - Clear invalid references
2. Or remove the --repair option

**Verification:**
- [ ] --repair attempts to fix issues found
- [ ] Or --repair removed from help

---

### CLI-4: sb_backup - Compression (HIGH)
**File:** src/cli/sb_backup.cpp
**Lines:** 73, 250
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
bool compress = false;  // Compression disabled for simplicity
// ...
header.flags = compress ? 0x01 : 0x00;  // Flag set but nothing compressed
```

**Required Changes:**
1. Implement LZ4 or zlib compression
2. Compress pages before writing
3. Decompress on restore
4. Update header.compressed_size correctly

**Or:**
Remove --compress option from help and argument parsing.

**Verification:**
- [ ] Compressed backup is smaller than uncompressed
- [ ] Restore works correctly

---

### CLI-5: sb_security - GRANT/REVOKE commands (HIGH)
**File:** src/cli/sb_security.cpp
**Lines:** 99-102, 612-646
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
enum class Command {
    // ...
    GRANT,
    REVOKE_PERM,
    SHOW_GRANTS_USER,
    SHOW_GRANTS_OBJECT,
    // These are defined but have NO handlers
};
```

**Required Changes:**
1. Add handler functions for each command
2. Add cases to dispatch function
3. Implement SQL execution for each

**Verification:**
- [ ] sb_security grant --user=x --on=table --privilege=SELECT works
- [ ] sb_security revoke works
- [ ] sb_security show-grants works

---

### CLI-M1: sb_verify - Page limit warning (MEDIUM)
**File:** src/cli/sb_verify.cpp
**Lines:** 239-240
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Silently stops at 10,000 pages
const uint64_t max_pages = 10000;
```

**Required Changes:**
Either:
A. Remove limit
B. Add warning when limit reached
C. Add --all flag to verify all pages

**Verification:**
- [ ] Large databases fully verified or warned

---

### CLI-M2: sb_backup - Restore checksum verification (MEDIUM)
**File:** src/cli/sb_backup.cpp
**Lines:** 308-391
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Reads pages but doesn't verify checksums before writing
```

**Required Changes:**
1. Verify each page checksum before writing
2. Abort on checksum failure (or warn and continue)

**Verification:**
- [ ] Corrupted backup detected before overwriting database

---

### CLI-M3: sb_backup - Page checksums (MEDIUM)
**File:** src/cli/sb_backup.cpp
**Line:** 284
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Only header is checksummed
header.checksum = crc32(reinterpret_cast<const uint8_t*>(&header), sizeof(header) - 4);
```

**Required Changes:**
1. Calculate checksum for each page
2. Store page checksums in backup
3. Verify on restore

**Verification:**
- [ ] Page corruption detected on verify/restore

---

### CLI-M4: sb_security - Check routing (MEDIUM)
**File:** src/cli/sb_security.cpp
**Lines:** 636-640
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
case Command::CHECK_PASSWORDS:
case Command::CHECK_PERMISSIONS:
case Command::CHECK_AUDIT:
    return checkAll();  // All go to same function
```

**Required Changes:**
1. Create checkPasswords(), checkPermissions(), checkAudit()
2. Route each command to specific check
3. checkAll() calls all three

**Verification:**
- [ ] sb_security check-passwords only checks passwords
- [ ] sb_security check-all runs all checks

---

### CLI-M5: sb_security - Audit filter (MEDIUM)
**File:** src/cli/sb_security.cpp
**Lines:** 492-500
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Function ignores filter argument
// Query hardcoded to LIMIT 100
```

**Required Changes:**
1. Parse filter parameter (type, user, object, date range)
2. Build WHERE clause from filter
3. Apply to audit log query

**Verification:**
- [ ] sb_security audit --filter=type:DDL shows only DDL events

---

### CLI-M6: sb_isql - Multi-line file include (MEDIUM)
**File:** src/cli/sb_isql.cpp
**Lines:** 388-422
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Executes on each semicolon, doesn't accumulate multi-line
```

**Required Changes:**
1. Accumulate lines until complete statement
2. Handle string literals with embedded semicolons
3. Execute complete statements only

**Verification:**
- [ ] \i file.sql with multi-line statements works

---

### CLI-L1: sb_isql - Quote parsing (LOW)
**File:** src/cli/sb_isql.cpp
**Lines:** 623-640
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// This is a simplified version - a full parser would track quotes properly
```

**Required Changes:**
Handle escape sequences in strings:
- `\'` escaped single quote
- `''` doubled single quote
- `\\` escaped backslash

**Verification:**
- [ ] 'don''t' parses correctly

---

### CLI-L2: sb_isql - Unknown meta-command (LOW)
**File:** src/cli/sb_isql.cpp
**Lines:** 529-530
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Unknown command - prints error but returns true
std::cerr << "Unknown command: " << cmd << "\n";
return true;
```

**Required Changes:**
Return false to indicate failure, or don't change behavior (just document).

**Verification:**
- [ ] Unknown commands behave consistently

---

### CLI-L3: sb_isql - Password prompt (LOW)
**File:** src/cli/sb_isql.cpp
**Lines:** 787-789
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Always prompts if no password provided
```

**Required Changes:**
Add --no-password flag for non-interactive use.

**Verification:**
- [ ] sb_isql --no-password doesn't prompt

---

### CLI-L4: sb_backup - Size display (LOW)
**File:** src/cli/sb_backup.cpp
**Lines:** 448-478
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Shows same size for original and compressed (because compression not implemented)
```

**Required Changes:**
After CLI-4 (compression), this will show correct sizes.

**Verification:**
- [ ] Compressed size shown correctly after compression implemented

---

### CLI-L5: sb_verify - Report severity guidance (LOW)
**File:** src/cli/sb_verify.cpp
**Lines:** 344-381
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Shows counts but no remediation guidance
```

**Required Changes:**
Add guidance text:
- CRITICAL: Database may be corrupted, restore from backup
- ERROR: Some data may be affected, run with --repair
- WARNING: Minor issues, database usable

**Verification:**
- [ ] Report includes actionable guidance

---

## Dependencies

- CLI-1, CLI-2 may need core headers
- CLI-4 needs LZ4 or zlib library
- CLI-5 needs database connection (same as other sb_security commands)
- CLI-L4 depends on CLI-4

---

## Testing Plan

1. Compile all four tools
2. Run each tool with --help
3. Test each command/option
4. Run with sample databases
5. Test error handling

---

## Completion Checklist

- [ ] All 16 tasks implemented
- [ ] All tools compile without warnings
- [ ] All commands work as documented
- [ ] Help text accurate

---

**Last Updated:** December 2, 2025
