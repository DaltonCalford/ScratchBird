# External Agents API Audit and Remediation Plan

**Date:** 2026-02-06  
**Scope:** MySQL Parser Agent and Engine IPC Session Handler  
**Status:** API Mismatches Blocking Compilation

---

## Executive Summary

The IPC core infrastructure has been successfully implemented, but two external agent components fail to compile due to pre-existing API mismatches with other subsystems (Database, Parser, SBLR). This document provides a detailed audit of the issues and a phased remediation plan.

**Affected Components:**
1. `src/ipc/external_agents/mysql_parser_agent.cpp` - 8 blocking issues
2. `src/ipc/external_agents/engine_ipc_session_handler.cpp` - 12+ blocking issues

**Root Cause:** These files were developed against older/evolving APIs that have since changed. They require alignment with current Database, Parser V2, IPC Contract, and SBLR Executor interfaces.

---

## Part 1: MySQL Parser Agent Audit

### File: `src/ipc/external_agents/mysql_parser_agent.cpp`

#### Issue 1.1: const char* length() Calls
**Lines:** 420, 448  
**Error:** `request for member 'length' in 'username', which is of non-class type 'const char*'`

**Current Code:**
```cpp
const char* username = ...;
size_t username_len = username.length();  // ERROR: const char* has no length()
```

**Required Fix:**
```cpp
const char* username = ...;
size_t username_len = std::strlen(username);  // Use strlen for C strings
// OR if it's a std::string:
std::string username = ...;
size_t username_len = username.length();  // This works for std::string
```

**Impact:** Medium - Affects handshake response parsing  
**Dependencies:** None  
**Estimated Effort:** 15 minutes

---

#### Issue 1.2: Missing CLIENT_SECURE_CONNECTION Constant
**Line:** 430  
**Error:** `'CLIENT_SECURE_CONNECTION' is not a member of 'scratchbird::ipc::mysql'`

**Current Code:**
```cpp
if (client_capabilities & mysql::CLIENT_SECURE_CONNECTION) {
```

**Analysis:** The MySQL protocol constants need to be defined. Looking at MySQL protocol documentation:
- CLIENT_SECURE_CONNECTION = 0x00008000 (MySQL 4.1+)
- Used for authentication method negotiation

**Required Fix:**
```cpp
// In mysql_parser_agent.h or protocol header
namespace scratchbird {
namespace ipc {
namespace mysql {
    // MySQL client capability flags
    constexpr uint32_t CLIENT_LONG_PASSWORD = 0x00000001;
    constexpr uint32_t CLIENT_FOUND_ROWS = 0x00000002;
    constexpr uint32_t CLIENT_LONG_FLAG = 0x00000004;
    constexpr uint32_t CLIENT_CONNECT_WITH_DB = 0x00000008;
    constexpr uint32_t CLIENT_NO_SCHEMA = 0x00000010;
    constexpr uint32_t CLIENT_COMPRESS = 0x00000020;
    constexpr uint32_t CLIENT_ODBC = 0x00000040;
    constexpr uint32_t CLIENT_LOCAL_FILES = 0x00000080;
    constexpr uint32_t CLIENT_IGNORE_SPACE = 0x00000100;
    constexpr uint32_t CLIENT_PROTOCOL_41 = 0x00000200;
    constexpr uint32_t CLIENT_INTERACTIVE = 0x00000400;
    constexpr uint32_t CLIENT_SSL = 0x00000800;
    constexpr uint32_t CLIENT_IGNORE_SIGPIPE = 0x00001000;
    constexpr uint32_t CLIENT_TRANSACTIONS = 0x00002000;
    constexpr uint32_t CLIENT_RESERVED = 0x00004000;
    constexpr uint32_t CLIENT_SECURE_CONNECTION = 0x00008000;  // MISSING
    constexpr uint32_t CLIENT_MULTI_STATEMENTS = 0x00010000;
    constexpr uint32_t CLIENT_MULTI_RESULTS = 0x00020000;
    constexpr uint32_t CLIENT_PS_MULTI_RESULTS = 0x00040000;
    constexpr uint32_t CLIENT_PLUGIN_AUTH = 0x00080000;
    constexpr uint32_t CLIENT_CONNECT_ATTRS = 0x00100000;
    constexpr uint32_t CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = 0x00200000;
}
}
}
```

**Impact:** Medium - Required for MySQL 4.1+ authentication  
**Dependencies:** None  
**Estimated Effort:** 30 minutes

---

#### Issue 1.3: PreparedStatement Type Not Declared
**Lines:** 619-620  
**Error:** `'PreparedStatement' was not declared in this scope`

**Current Code:**
```cpp
PreparedStatement* stmt = stmt_cache_.get(stmt_id);
```

**Analysis:** The MySQL parser agent references a `PreparedStatement` type that should be defined locally or included from a header. This is likely a local struct for tracking prepared statement state.

**Required Fix:**
```cpp
// In mysql_parser_agent.h or at top of .cpp file
namespace scratchbird {
namespace ipc {

struct MySQLPreparedStatement {
    uint32_t stmt_id;
    std::string sql;
    std::vector<uint8_t> bytecode;
    std::vector<IPCFieldDesc> param_fields;
    std::vector<IPCFieldDesc> result_fields;
    bool is_valid = true;
};

// In MySQLParserAgent class, add:
private:
    std::unordered_map<uint32_t, std::unique_ptr<MySQLPreparedStatement>> prepared_statements_;
    std::mutex stmt_mutex_;
    uint32_t next_stmt_id_ = 1;
};
}
}
```

**Impact:** High - Required for prepared statement support  
**Dependencies:** IPCFieldDesc definition  
**Estimated Effort:** 1 hour

---

#### Issue 1.4: IPCFieldDesc Missing Members
**Lines:** 856, 860  
**Error:** `'const struct scratchbird::ipc::IPCFieldDesc' has no member named 'max_length'` / `'data_type'`

**Current Code:**
```cpp
field.max_length = 255;  // Missing member
field.data_type = static_cast<uint32_t>(type);  // Missing member
```

**Analysis:** Need to check actual IPCFieldDesc definition in `ipc_contract_v1_1.h`

**Investigation Required:**
```bash
grep -n "struct IPCFieldDesc" include/scratchbird/ipc/ipc_contract_v1_1.h -A 20
```

**Likely Required Fix:**
```cpp
// Current definition may be missing these fields
struct IPCFieldDesc {
    char name[64];
    uint32_t type_oid;
    int32_t type_mod;
    uint16_t format;
    // MISSING: uint32_t max_length;
    // MISSING: uint32_t data_type;
};

// Either add fields to struct OR map to existing fields:
field.type_oid = static_cast<uint32_t>(type);  // Use type_oid instead of data_type
// Remove max_length assignment or add to struct
```

**Impact:** High - Required for result set metadata  
**Dependencies:** IPC Contract header alignment  
**Estimated Effort:** 2 hours (requires struct alignment decision)

---

#### Issue 1.5: Duplicate Case Values
**Lines:** 1012, 1017  
**Error:** `duplicate case value`

**Analysis:** Two case labels have the same constant value in a switch statement.

**Investigation Required:**
```bash
sed -n '1005,1025p' src/ipc/external_agents/mysql_parser_agent.cpp
```

**Likely Cause:** Copy-paste error or enum value collision.

**Impact:** Low - Syntax error  
**Dependencies:** None  
**Estimated Effort:** 15 minutes

---

## Part 2: Engine IPC Session Handler Audit

### File: `src/ipc/external_agents/engine_ipc_session_handler.cpp`

#### Issue 2.1: Syntax Error at Line 297
**Line:** 297  
**Error:** `expected unqualified-id before ')' token`

**Investigation Required:**
```bash
sed -n '290,305p' src/ipc/external_agents/engine_ipc_session_handler.cpp
```

**Likely Cause:** Malformed function signature, extra parenthesis, or missing parameter name.

**Impact:** Blocking - Prevents compilation  
**Dependencies:** None  
**Estimated Effort:** 15 minutes

---

#### Issue 2.2: SessionState Naming Conflict
**Lines:** Multiple (345, 346, 347, etc.)  
**Error:** `request for member 'session_id' in '* session', which is of non-class type 'scratchbird::ipc::SessionState'`

**Analysis:** The file defines `struct EngineSessionState` but the `sessions_` map stores `SessionState` (the enum from ipc_server.h). The sed replacement was incomplete.

**Current State:**
```cpp
// In .cpp file - struct is renamed to EngineSessionState
struct EngineSessionState { ... };

// In .h file - but map still uses wrong type
std::unordered_map<uint32_t, std::unique_ptr<SessionState>> sessions_;  // WRONG
```

**Required Fix:**
```cpp
// In engine_ipc_session_handler.h:
std::unordered_map<uint32_t, std::unique_ptr<EngineSessionState>> sessions_;

// In .cpp file - ensure all references use EngineSessionState*
EngineSessionState* session = getSession(session_id);
```

**Note:** Already attempted partial fix. Need to verify complete replacement.

**Impact:** Blocking - Type mismatch  
**Dependencies:** None  
**Estimated Effort:** 30 minutes

---

#### Issue 2.3: IPCStartupPayload Missing Members
**Lines:** 347, 354  
**Error:** `'const struct scratchbird::ipc::IPCStartupPayload' has no member named 'username'`

**Investigation Required:**
```bash
grep -n "struct IPCStartupPayload" include/scratchbird/ipc/ipc_contract_v1_1.h -A 15
```

**Likely Required Fix:**
```cpp
// Current definition may be:
struct IPCStartupPayload {
    uint32_t protocol_version;
    uint32_t client_pid;
    char database[64];
    char user[32];  // Named 'user' not 'username'
    // ...
};

// Fix: Use correct member name
session->username = startup.user;  // Instead of startup.username
```

**Impact:** High - Required for session initialization  
**Dependencies:** IPC Contract header  
**Estimated Effort:** 30 minutes

---

#### Issue 2.4: Database API Mismatch
**Lines:** 354, multiple  
**Error:** `'class scratchbird::core::Database' has no member named 'getCatalogManager'`

**Current Code:**
```cpp
database_->getCatalogManager()  // Method doesn't exist
```

**Investigation Required:**
```bash
grep -n "catalog_manager" include/scratchbird/core/database.h | head -10
```

**Likely Required Fix:**
```cpp
// If catalog_manager is a public member:
database_->catalog_manager

// If there's an accessor with different name:
database_->getCatalog()  // or similar
```

**Impact:** High - Required for query parsing  
**Dependencies:** Database class definition  
**Estimated Effort:** 1 hour (requires API alignment)

---

#### Issue 2.5: Missing SBLR Opcodes
**Lines:** 790, 876  
**Error:** `'BEGIN' is not a member of 'scratchbird::sblr::Opcode'` / `'SAVEPOINT' is not a member`

**Current Code:**
```cpp
std::vector<uint8_t> begin_bytecode = {static_cast<uint8_t>(sblr::Opcode::BEGIN)};
sp_bytecode.push_back(static_cast<uint8_t>(sblr::Opcode::SAVEPOINT));
```

**Analysis:** The SBLR bytecode system may not have implemented transaction opcodes yet, or they have different names.

**Investigation Required:**
```bash
grep -n "enum.*Opcode" include/scratchbird/sblr/executor.h -A 50
# OR
grep -rn "BEGIN\|COMMIT\|ROLLBACK\|SAVEPOINT" include/scratchbird/sblr/ | head -20
```

**Likely Required Fix:**
```cpp
// Option 1: Use extended opcodes if they exist
sblr::Opcode::EXT_BEGIN
sblr::Opcode::EXT_SAVEPOINT

// Option 2: Generate bytecode using BytecodeGenerator instead of raw opcodes
sblr::BytecodeGenerator gen;
gen.emitBegin();
std::vector<uint8_t> bytecode = gen.finish();

// Option 3: Stub until SBLR implements transaction opcodes
// (Already done in partial fix)
```

**Impact:** Medium - Transaction support  
**Dependencies:** SBLR Executor/BytecodeGenerator  
**Estimated Effort:** 2-4 hours (may require SBLR changes)

---

#### Issue 2.6: ParserV2 API Issues
**Line:** 409, 535  
**Error:** `'scratchbird::parser::v2::ParserV2' has not been declared`

**Analysis:** The Parser V2 subsystem API has likely changed or the include is missing.

**Investigation Required:**
```bash
ls include/scratchbird/parser/
grep -rn "class.*Parser" include/scratchbird/parser/ | head -10
```

**Likely Required Fix:**
```cpp
// Check correct include path
#include "scratchbird/parser/parser_v2.h"
// OR
#include "scratchbird/sql/parser.h"

// Check correct namespace and class name
parser::v2::Parser::parse(...)  // instead of ParserV2
// OR
parser::Parser::parse(...)
```

**Impact:** High - Required for SQL parsing  
**Dependencies:** Parser subsystem  
**Estimated Effort:** 2 hours

---

#### Issue 2.7: ErrorContext String Construction
**Line:** 432  
**Error:** `cannot convert 'std::__cxx11::basic_string<char>' to 'const char*'`

**Current Code:**
```cpp
ctx->set(core::Status::INVALID_ARGUMENT,
        "Bytecode generation error: " + errors,  // std::string
        __FILE__, __LINE__, __func__);
```

**Required Fix:**
```cpp
ctx->set(core::Status::INVALID_ARGUMENT,
        ("Bytecode generation error: " + errors).c_str(),  // Convert to const char*
        __FILE__, __LINE__, __func__);
// OR construct differently
```

**Impact:** Low - Error handling only  
**Dependencies:** None  
**Estimated Effort:** 15 minutes

---

#### Issue 2.8: IPCFieldDesc Member Issues
**Lines:** 453, 458, 461, 464, 678, 679  
**Error:** Same as Issue 1.4 - missing `data_type`, `max_length` members

**Same fix applies** - need to align IPCFieldDesc struct or use existing fields.

---

## Part 3: Cross-Cutting API Alignment Issues

### 3.1 IPCFieldDesc Struct Alignment

**Problem:** The IPCFieldDesc struct definition doesn't match what agents expect.

**Required Action:**
1. Audit all usages of IPCFieldDesc across the codebase
2. Determine the "source of truth" definition
3. Update struct to include required fields OR update all usages

**Decision Needed:**
- Option A: Add `max_length` and `data_type` fields to IPCFieldDesc
- Option B: Map agent code to use existing fields (`type_oid`, `type_mod`)

**Impact:** High - Affects multiple components  
**Estimated Effort:** 4-8 hours (depending on decision)

---

### 3.2 Transaction Opcode Implementation

**Problem:** SBLR may not have transaction opcodes implemented.

**Required Action:**
1. Verify current SBLR opcode set
2. Determine if transaction support exists
3. Implement if missing OR stub for now

**Impact:** Medium - Affects transaction support  
**Estimated Effort:** 4-16 hours (if SBLR changes needed)

---

### 3.3 Parser API Stabilization

**Problem:** ParserV2 API appears to be in flux.

**Required Action:**
1. Confirm current Parser API
2. Update engine_ipc_session_handler to use correct API
3. Document the stable API

**Impact:** High - Affects SQL parsing  
**Estimated Effort:** 2-4 hours

---

## Part 4: Remediation Plan

### Phase 1: Quick Fixes (1-2 days)
**Goal:** Resolve syntax errors and simple API mismatches

1. **MySQL Parser Agent**
   - [ ] Fix const char* length() calls (Issue 1.1)
   - [ ] Add missing MySQL constants (Issue 1.2)
   - [ ] Fix duplicate case values (Issue 1.5)
   - [ ] Define PreparedStatement type (Issue 1.3)

2. **Engine Session Handler**
   - [ ] Fix syntax error at line 297 (Issue 2.1)
   - [ ] Complete SessionState renaming (Issue 2.2)
   - [ ] Fix ErrorContext string construction (Issue 2.7)
   - [ ] Map IPCStartupPayload members (Issue 2.3)

### Phase 2: API Alignment (2-3 days)
**Goal:** Align with current subsystem APIs

1. **IPCFieldDec Alignment** (Decision Required)
   - [ ] Audit all IPCFieldDesc usages
   - [ ] Decide: extend struct or update usages
   - [ ] Implement decision

2. **Database API**
   - [ ] Update getCatalogManager() calls
   - [ ] Verify ConnectionContext API
   - [ ] Test database integration

3. **Parser API**
   - [ ] Confirm Parser V2 API
   - [ ] Update all parser calls
   - [ ] Verify parse result handling

### Phase 3: SBLR Integration (3-5 days)
**Goal:** Resolve SBLR opcode and execution issues

1. **Transaction Opcodes**
   - [ ] Verify current SBLR capabilities
   - [ ] Implement/stub BEGIN, COMMIT, ROLLBACK, SAVEPOINT
   - [ ] Test transaction execution

2. **Bytecode Generation**
   - [ ] Align with BytecodeGenerator V2 API
   - [ ] Verify executor integration
   - [ ] Test query execution

### Phase 4: Integration Testing (2-3 days)
**Goal:** Verify end-to-end functionality

1. **Compilation**
   - [ ] Full build without errors
   - [ ] Enable -Werror for strict checking
   - [ ] Run static analysis

2. **Unit Tests**
   - [ ] MySQL parser agent tests
   - [ ] Engine session handler tests
   - [ ] IPC message flow tests

3. **Integration Tests**
   - [ ] End-to-end query execution
   - [ ] Prepared statement flow
   - [ ] Transaction flow
   - [ ] COPY flow

---

## Part 5: Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| IPCFieldDesc changes break other components | High | High | Comprehensive testing, incremental changes |
| SBLR transaction opcodes require major work | Medium | High | Stub for now, implement later |
| Parser API changes break other components | Medium | High | Coordinate with parser team |
| Database API changes break other components | Medium | High | Verify all Database usages |
| Hidden API mismatches discovered later | Medium | Medium | Incremental compilation testing |

---

## Part 6: Dependencies and Prerequisites

### External Dependencies
- **SBLR Team:** Confirm transaction opcode status
- **Parser Team:** Confirm Parser V2 stable API
- **Database Team:** Confirm CatalogManager API

### Internal Dependencies
- IPC Contract header (`ipc_contract_v1_1.h`)
- Database header (`database.h`)
- Parser headers
- SBLR Executor headers

### Required Information
1. Current IPCFieldDesc "source of truth" definition
2. SBLR opcode list and transaction support status
3. Parser V2 API documentation
4. Database class public interface documentation

---

## Part 7: Immediate Next Steps

### This Week
1. **Decision Required:** IPCFieldDesc alignment approach
2. **Information Gathering:**
   - Verify current struct definitions
   - Confirm Parser API
   - Confirm SBLR capabilities
3. **Begin Phase 1:** Quick syntax fixes

### Next Week
1. Complete Phase 1 quick fixes
2. Begin Phase 2 API alignment
3. Implement IPCFieldDesc decision

### Following Weeks
1. Complete API alignment
2. Address SBLR integration
3. Integration testing

---

## Appendix: Investigation Commands

```bash
# IPCFieldDesc definition
grep -n "struct IPCFieldDesc" include/scratchbird/ipc/ipc_contract_v1_1.h -A 15

# IPCStartupPayload definition
grep -n "struct IPCStartupPayload" include/scratchbird/ipc/ipc_contract_v1_1.h -A 15

# Database CatalogManager
grep -n "catalog_manager" include/scratchbird/core/database.h

# SBLR Opcodes
grep -rn "enum.*Opcode" include/scratchbird/sblr/
grep -rn "BEGIN\|COMMIT\|ROLLBACK" include/scratchbird/sblr/executor.h

# Parser API
ls include/scratchbird/parser/
grep -rn "class.*Parser" include/scratchbird/parser/

# MySQL constants
grep -rn "CLIENT_" include/scratchbird/ipc/mysql_parser_agent.h
```

---

## Summary

The external agent compilation failures stem from API evolution in dependent subsystems. The issues are solvable but require:

1. **Decisions** on IPCFieldDesc alignment approach
2. **Information** from SBLR, Parser, and Database teams
3. **Phased implementation** over 1-2 weeks
4. **Comprehensive testing** to avoid regressions

The core IPC infrastructure is solid; these are integration-layer issues that can be resolved with focused effort on API alignment.
