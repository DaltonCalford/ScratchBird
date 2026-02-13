# External Agents Quick Fix Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**For:** Developers fixing API mismatches  
**Companion:** See `EXTERNAL_AGENTS_API_AUDIT_AND_REMEDIATION_PLAN.md` for full details

---

## Priority 1: Syntax Errors (Can Fix Today)

### Fix 1.1: MySQL Parser - const char* length()
**File:** `src/ipc/external_agents/mysql_parser_agent.cpp`  
**Lines:** 420, 448

**Change FROM:**
```cpp
const char* username = ...;
size_t username_len = username.length();  // ERROR
```

**Change TO:**
```cpp
const char* username = ...;
size_t username_len = std::strlen(username);  // FIXED
```

---

### Fix 1.2: Engine Handler - Syntax Error Line 297
**File:** `src/ipc/external_agents/engine_ipc_session_handler.cpp`  
**Line:** 297

**Investigate:**
```bash
sed -n '290,305p' src/ipc/external_agents/engine_ipc_session_handler.cpp
```

**Likely Issue:** Extra parenthesis or missing parameter name in function signature.

---

### Fix 1.3: ErrorContext String Construction
**File:** `src/ipc/external_agents/engine_ipc_session_handler.cpp`  
**Line:** 432

**Change FROM:**
```cpp
ctx->set(core::Status::INVALID_ARGUMENT,
        "Bytecode generation error: " + errors,  // std::string
        __FILE__, __LINE__, __func__);
```

**Change TO:**
```cpp
std::string error_msg = "Bytecode generation error: " + errors;
ctx->set(core::Status::INVALID_ARGUMENT,
        error_msg.c_str(),  // const char*
        __FILE__, __LINE__, __func__);
```

---

## Priority 2: Missing Constants (1 Hour)

### Fix 2.1: MySQL CLIENT_SECURE_CONNECTION
**File:** `src/ipc/external_agents/mysql_parser_agent.cpp` (or header)  
**Line:** 430

**Add to `mysql_parser_agent.h`:**
```cpp
namespace scratchbird {
namespace ipc {
namespace mysql {
    // Add missing constants
    constexpr uint32_t CLIENT_SECURE_CONNECTION = 0x00008000;
    constexpr uint32_t CLIENT_PLUGIN_AUTH = 0x00080000;
    // ... other missing constants as needed
}
}
}
```

---

### Fix 2.2: PreparedStatement Type
**File:** `src/ipc/external_agents/mysql_parser_agent.cpp`  
**Line:** 619

**Add to `mysql_parser_agent.h`:**
```cpp
struct MySQLPreparedStatement {
    uint32_t stmt_id;
    std::string sql;
    std::vector<uint8_t> bytecode;
    std::vector<IPCFieldDesc> param_fields;
    std::vector<IPCFieldDesc> result_fields;
    bool is_valid = true;
};
```

**Add to `MySQLParserAgent` class:**
```cpp
private:
    std::unordered_map<uint32_t, std::unique_ptr<MySQLPreparedStatement>> prepared_statements_;
    std::mutex stmt_mutex_;
    uint32_t next_stmt_id_ = 1;
```

---

## Priority 3: Type Alignment (2-4 Hours)

### Fix 3.1: IPCFieldDesc Members
**Decision Required:** Extend struct or update usages?

**Option A - Extend Struct** (`include/scratchbird/ipc/ipc_contract_v1_1.h`):
```cpp
struct IPCFieldDesc {
    char name[64];
    uint32_t type_oid;
    int32_t type_mod;
    uint16_t format;
    // ADD THESE:
    uint32_t max_length;    // For MySQL/FB compatibility
    uint32_t data_type;     // Alternative to type_oid
};
```

**Option B - Update Usages** (in agent files):
```cpp
// Instead of field.data_type = ...
field.type_oid = static_cast<uint32_t>(type);

// Instead of field.max_length = ...
field.type_mod = max_length;  // Or remove if not applicable
```

---

### Fix 3.2: IPCStartupPayload Members
**Investigate current definition:**
```bash
grep -n "struct IPCStartupPayload" include/scratchbird/ipc/ipc_contract_v1_1.h -A 15
```

**Likely Fix** (in engine handler):
```cpp
// If struct has 'user' instead of 'username':
session->username = startup.user;  // Not startup.username
```

---

### Fix 3.3: SessionState Naming
**Files:** 
- `include/scratchbird/ipc/engine_ipc_session_handler.h`
- `src/ipc/external_agents/engine_ipc_session_handler.cpp`

**Verify the map type in header:**
```cpp
// Should be:
std::unordered_map<uint32_t, std::unique_ptr<EngineSessionState>> sessions_;
// Not:
std::unordered_map<uint32_t, std::unique_ptr<SessionState>> sessions_;
```

---

## Priority 4: API Integration (1-2 Days)

### Fix 4.1: Database getCatalogManager()
**File:** `src/ipc/external_agents/engine_ipc_session_handler.cpp`

**Investigate:**
```bash
grep -n "catalog_manager" include/scratchbird/core/database.h | head -5
```

**Likely Fix:**
```cpp
// If catalog_manager is a public member:
database_->catalog_manager
// OR if there's an accessor:
database_->getCatalog()
```

---

### Fix 4.2: ParserV2 API
**File:** `src/ipc/external_agents/engine_ipc_session_handler.cpp`

**Investigate:**
```bash
ls include/scratchbird/parser/
grep -n "parse" include/scratchbird/parser/*.h | head -10
```

**Likely Fix:**
```cpp
// Try different namespaces:
parser::Parser::parse(...)
parser::v2::Parser::parse(...)
sql::Parser::parse(...)
```

---

### Fix 4.3: SBLR Transaction Opcodes
**File:** `src/ipc/external_agents/engine_ipc_session_handler.cpp`

**Stub Approach** (already partially done):
```cpp
// For now, use placeholder until opcodes implemented:
auto result = sblr::ExecutionResult(); // Placeholder

// TODO: Implement actual transaction bytecode when opcodes available
```

**Alternative:** Use BytecodeGenerator if available:
```cpp
sblr::BytecodeGenerator gen;
gen.emitBegin();  // If this method exists
// ...
```

---

## Build Verification Commands

```bash
# After each fix, verify compilation:

# MySQL parser agent
cd build && make -j4 src/CMakeFiles/scratchbird_ipc.dir/ipc/external_agents/mysql_parser_agent.cpp.o 2>&1 | tail -20

# Engine session handler
cd build && make -j4 src/CMakeFiles/scratchbird_ipc.dir/ipc/external_agents/engine_ipc_session_handler.cpp.o 2>&1 | tail -20

# Full IPC library
cd build && make -j4 scratchbird_ipc 2>&1 | tail -30
```

---

## Decision Log

Track decisions here as they are made:

| Date | Decision | Rationale | Made By |
|------|----------|-----------|---------|
| | IPCFieldDesc: Extend or Map? | | |
| | SBLR Opcodes: Stub or Implement? | | |
| | Parser API: Which namespace? | | |
| | Database: Member or Accessor? | | |

---

## Quick Reference: Common API Patterns

### IPCFieldDesc
```cpp
// Setting field info
strncpy(field.name, column_name, sizeof(field.name) - 1);
field.name[sizeof(field.name) - 1] = '\0';
field.type_oid = type_oid;
field.type_mod = -1;  // Default
field.format = 0;     // Text format
```

### IPCMessage
```cpp
// Creating message with payload
IPCMessage msg(IPCMessageType::SIMPLE_QUERY, session_id);
auto* payload = msg.getPayload<IPCSimpleQueryPayload>();
if (payload) {
    strncpy(payload->sql, sql.c_str(), sizeof(payload->sql) - 1);
}
```

### ErrorContext
```cpp
// Setting error
if (ctx) {
    ctx->set(core::Status::INVALID_ARGUMENT,
            error_msg.c_str(),  // Must be const char*
            __FILE__, __LINE__, __func__);
}
```

---

## Contact Points

For API clarifications:
- **IPC Contract:** Check `include/scratchbird/ipc/ipc_contract_v1_1.h`
- **Database:** Check `include/scratchbird/core/database.h`
- **Parser:** Check `include/scratchbird/parser/` directory
- **SBLR:** Check `include/scratchbird/sblr/executor.h`
