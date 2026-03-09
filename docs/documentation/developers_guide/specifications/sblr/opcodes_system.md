# Specification: SBLR System & Control Opcodes

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | sblr |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:1-5`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:1-10`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:560-590`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_container.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_opcode_identity.cpp`

## Synopsis

This specification defines the System and Control opcodes for the ScratchBird Language Runtime (SBLR) v3. These opcodes provide fundamental control flow, versioning, and bytecode container management. They form the foundation of the SBLR bytecode execution model.

## Scope

### In Scope

- Bytecode container termination and versioning
- Extended opcode dispatch mechanism
- Transaction control opcodes (COMMIT, ROLLBACK, SAVEPOINT)
- Error handling and exception propagation
- Program counter management

### Out of Scope

- Specific DDL/DML operation implementations (see opcodes_ddl.md, opcodes_dml.md)
- Expression evaluation (see opcodes_expressions.md)
- Storage engine internals

## Background

The SBLR (ScratchBird Language Runtime) is a bytecode-based execution engine that translates SQL statements into a portable intermediate representation. The System & Control opcodes provide the structural foundation for all bytecode programs, managing execution lifecycle and control flow.

### Key Concepts

- **Bytecode Container**: A self-contained unit of SBLR bytecode with header, instruction stream, and metadata
- **Program Counter (PC)**: The current execution position within the bytecode stream
- **Extended Opcodes**: A mechanism to support opcodes beyond the 16-bit range via dispatch

## Specification

### Data Structures

```cpp
// From include/scratchbird/sblr/v3_opcodes.generated.h:2-5
enum class Opcode : uint16_t {
    SBLR3_END = 0x0001,              // Bytecode termination
    SBLR3_VERSION = 0x0002,          // Version identifier
    SBLR3_EXTENDED_OPCODE = 0x0003,  // Extended opcode dispatch
};
```

#### Execution Context

```cpp
// From src/sblr/executor.cpp:420-425
const uint8_t *bytecode_;           // Raw bytecode pointer
const std::vector<uint8_t> *current_bytecode_vec_ = nullptr;
size_t bytecode_size_;              // Total bytecode size
size_t pc_;                         // Program counter
std::stack<Value> stack_;           // Evaluation stack
```

### Interface Contracts

#### Opcode: `SBLR3_END` (0x0001)

```cpp
// Source: src/sblr/executor.cpp (implied by execution loop)
// Payload: None
// Execution: Terminates the current bytecode program
```

**Semantics:**
- Marks the end of a bytecode instruction stream
- Causes the executor to return control to the caller
- No payload associated

**Execution Flow:**
```
1. Executor reads opcode 0x0001
2. Validates no payload follows (or ignores any payload)
3. Returns ExecutionResult::SUCCESS
4. Execution terminates
```

**Error Handling:**
- No error conditions for properly formed bytecode
- Premature END may leave operations incomplete

---

#### Opcode: `SBLR3_VERSION` (0x0002)

```cpp
// Source: src/sblr/v3_container.cpp
// Payload: uint16_t version_major, uint16_t version_minor
```

**Semantics:**
- Declares the SBLR bytecode version for compatibility checking
- Must appear near the start of a bytecode container
- Executor validates version compatibility before execution

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| version_major | uint16_t | Major version number |
| version_minor | uint16_t | Minor version number |

**Version Compatibility:**
| Executor Version | Bytecode Version | Result |
|------------------|------------------|--------|
| 3.x | 3.0 | ✅ Compatible |
| 3.x | 2.x | ⚠️ Deprecated (may work) |
| 3.x | 4.x | ❌ Error (unsupported) |

---

#### Opcode: `SBLR3_EXTENDED_OPCODE` (0x0003)

```cpp
// Source: src/sblr/executor.cpp:565-570
uint16_t readExtendedOpcode() {
    // Reads additional 16 bits for extended opcode space
    return readInt16();
}
```

**Semantics:**
- Enables opcode space beyond 16-bit range (0x0000-0xFFFF)
- First reads 0x0003, then reads the actual extended opcode
- Used for future expansion and custom opcodes

**Execution Flow:**
```
1. Read opcode 0x0003 (EXTENDED_OPCODE)
2. Read next 16 bits as extended_opcode
3. Dispatch to extended handler
4. Extended opcodes use 0x6000-0xFFFF range
```

**Payload:** None (opcode itself is the extension mechanism)

---

### Transaction Control Opcodes (0x0301-0x0318)

#### Transaction State Management

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_START_TRANSACTION | 0x0317 | Begin a new transaction |
| SBLR3_SET_TRANSACTION | 0x0316 | Configure transaction properties |
| SBLR3_COMMIT | 0x0301 | Commit current transaction |
| SBLR3_COMMIT_RETAINING | 0x0303 | Commit but retain context |
| SBLR3_COMMIT_PREPARED | 0x0302 | Commit a prepared transaction |
| SBLR3_ROLLBACK | 0x0315 | Rollback current transaction |
| SBLR3_ROLLBACK_RETAINING | 0x030B | Rollback but retain context |
| SBLR3_ROLLBACK_PREPARED | 0x030A | Rollback a prepared transaction |
| SBLR3_ROLLBACK_TO_SAVEPOINT | 0x030D | Rollback to savepoint |
| SBLR3_PREPARE_TRANSACTION | 0x0307 | Prepare for two-phase commit |

#### Savepoint Management

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_SAVEPOINT | 0x030F | Create named savepoint |
| SBLR3_SAVEPOINT_BEGIN | 0x0310 | Begin savepoint scope |
| SBLR3_SAVEPOINT_END | 0x0311 | End savepoint scope |
| SBLR3_RELEASE_SAVEPOINT | 0x0309 | Release a savepoint |

#### Autocommit Control

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_SET_AUTOCOMMIT | 0x0312 | Enable/disable autocommit |
| SBLR3_SHOW_TRANSACTION_LEVEL | 0x0314 | Query isolation level |

#### Transaction Opcode Details

**SBLR3_START_TRANSACTION (0x0317)**

```cpp
// Source: src/sblr/executor.cpp:727
void executeStartTransaction();
```

- Initiates a new transaction context
- Reads transaction characteristics from payload
- Supports isolation level specification
- Access mode (READ ONLY / READ WRITE)

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| isolation_level | uint8_t | 0=DEFAULT, 1=READ_UNCOMMITTED, 2=READ_COMMITTED, 3=REPEATABLE_READ, 4=SERIALIZABLE |
| access_mode | uint8_t | 0=DEFAULT, 1=READ_ONLY, 2=READ_WRITE |
| deferrable | uint8_t | 0=false, 1=true |

**SBLR3_COMMIT (0x0301)**

```cpp
// Source: src/sblr/executor.cpp:730
void executeCommit();
void executeCommitFlags(uint8_t flags);
```

- Commits all changes in the current transaction
- Flags support RETAINING behavior
- Triggers index maintenance and garbage collection

**SBLR3_ROLLBACK (0x0315)**

```cpp
// Source: src/sblr/executor.cpp:731
void executeRollback();
void executeRollbackFlags(uint8_t flags);
```

- Rolls back all changes in the current transaction
- Releases all transaction locks
- Restores pre-transaction state

**SBLR3_SAVEPOINT (0x030F)**

```cpp
// Source: src/sblr/executor.cpp:738
void executeSavepoint();
```

- Creates a named savepoint within the current transaction
- Enables partial rollback capabilities
- Savepoint name stored as string payload

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Length of savepoint name |
| name | char[name_len] | Savepoint identifier |

### Error Handling Opcodes

#### Exception Handling Structure

```cpp
// From src/sblr/executor.h:999-1030
struct VariableEntry {
    std::string name;
    core::DataType type;
    Value value;
    bool is_null = false;
};
```

#### Error Propagation Flow

```
┌─────────────────┐
│  Error Detected │
└────────┬────────┘
         ▼
┌─────────────────┐
│  Build ErrorCtx │
└────────┬────────┘
         ▼
┌─────────────────┐
│  Search Handler │
└────────┬────────┘
         ▼
┌─────────────────┐     ┌─────────────────┐
│ Handler Found?  │──No──►  Propagate Up  │
└────────┬────────┘     └─────────────────┘
         │ Yes
         ▼
┌─────────────────┐
│ Execute Handler │
└─────────────────┘
```

### State Machines

#### Transaction State Machine

```
┌──────────┐    START_TRANSACTION    ┌──────────┐
│   IDLE   │ ───────────────────────►│  ACTIVE  │
└──────────┘                         └────┬─────┘
     ▲                                    │
     │                                    │ COMMIT
     │ ROLLBACK                           ▼
     │                              ┌──────────┐
     └──────────────────────────────│COMMITTED │
                                    └──────────┘
```

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| IDLE | START_TRANSACTION | Initialize transaction | ACTIVE |
| ACTIVE | COMMIT | Persist changes | COMMITTED |
| ACTIVE | ROLLBACK | Discard changes | IDLE |
| ACTIVE | SAVEPOINT | Record checkpoint | ACTIVE (with savepoint) |
| ACTIVE (with savepoint) | ROLLBACK_TO_SAVEPOINT | Restore checkpoint | ACTIVE |

### Decision Trees

#### Opcode Dispatch Decision

```
Fetch next opcode
├── 0x0001 (END) → Terminate execution
├── 0x0002 (VERSION) → Validate version
├── 0x0003 (EXTENDED) → Read extended opcode
│   └── Dispatch to extended handler table
├── 0x01xx (DDL) → DDL dispatcher
├── 0x02xx (DML) → DML dispatcher
├── 0x03xx (Transaction) → Transaction dispatcher
└── Unknown → ERROR_INVALID_OPCODE
```

### Invariants

1. **Termination Invariant**: Every valid bytecode stream MUST end with SBLR3_END
   - Verification: Executor checks PC against bytecode size

2. **Version Invariant**: SBLR3_VERSION MUST be the first opcode (after optional header)
   - Verification: Container validation on load

3. **Transaction Nesting Invariant**: Savepoints cannot exceed MAX_SAVEPOINT_DEPTH (100)
   - Verification: Counter check in executeSavepoint()

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `E_INVALID_OPCODE` | Unknown opcode encountered | Terminate execution |
| `E_VERSION_MISMATCH` | Bytecode version incompatible | Reject container |
| `E_NO_TRANSACTION` | Transaction op outside transaction | Error return |
| `E_INVALID_SAVEPOINT` | ROLLBACK to non-existent savepoint | Error return |
| `E_NESTING_TOO_DEEP` | Savepoints exceed limit | Error return |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_sblr_v3_container.cpp` | Container format and versioning |
| `tests/unit/test_sblr_v3_opcode_identity.cpp` | Opcode identity verification |
| `tests/unit/test_sblr_v3_handler_registry.cpp` | Handler dispatch |
| `tests/unit/test_sblr_jit_runtime_selector.cpp` | JIT vs interpreter selection |

## Related Specifications

- [v3_container_format.md](./v3_container_format.md) - Bytecode container structure
- [v3_execution_model.md](./v3_execution_model.md) - Execution semantics
- [opcodes_ddl.md](./opcodes_ddl.md) - Data Definition Language opcodes
- [opcodes_dml.md](./opcodes_dml.md) - Data Manipulation Language opcodes

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Bytecode | Platform-independent intermediate representation |
| Container | Self-contained bytecode unit with metadata |
| PC | Program Counter - current execution position |
| Savepoint | Transaction checkpoint for partial rollback |
| TPC | Two-Phase Commit - distributed transaction protocol |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
