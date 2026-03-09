# Specification: {TITLE}

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | {parser/sblr/storage/catalog/security/ipc/etc} |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft / 🟡 Review / 🟢 Approved / ⚪ Stable / 🚫 Deprecated |
| **Last Verified** | YYYY-MM-DD |
| **Implementation Version** | ScratchBird {version or commit} |
| **Authors** | {names} |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/{path}/{file}.cpp:{line}`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/{path}/{file}.h:{line}`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_{feature}.cpp:{line}`

## Synopsis

One-paragraph summary of what this specification defines.

## Scope

### In Scope

- Specific behaviors covered by this spec
- Interface contracts defined here
- Algorithms documented here

### Out of Scope

- Related but separate concerns (link to other specs)
- Implementation details that vary by platform
- Future enhancements not yet implemented

## Background

Context needed to understand this spec. Include:
- Why this subsystem exists
- How it fits into the overall architecture
- Key concepts and terminology

## Specification

### Data Structures

```cpp
// From src/{path}/{file}.h:{line}
struct ExampleStruct {
    uint64_t id;           // Unique identifier
    uint32_t flags;        // Bitmask of EXAMPLE_FLAG_* values
    char name[64];         // Null-terminated string
};
```

### Interface Contracts

#### Function: `function_name()`

```cpp
// Source: src/{path}/{file}.cpp:{line}
ReturnType function_name(
    ParameterType param1,  // Description of param1
    ParameterType param2   // Description of param2
);
```

**Preconditions:**
- Condition that must be true before calling
- Another condition

**Postconditions:**
- Condition guaranteed after successful return
- State changes made by the function

**Error Handling:**
- What happens on invalid input
- Error codes/exceptions that may be thrown

**Thread Safety:**
- Thread safety guarantees (or lack thereof)
- Required locking protocol

### Algorithms

#### Algorithm: {Name}

```
Input:  {what goes in}
Output: {what comes out}

1. Step one description
2. Step two description
   - Sub-step detail
   - Sub-step detail
3. Step three description
```

**Complexity:**
- Time: O(n log n) - explanation
- Space: O(n) - explanation

### State Machines

```
┌─────────┐    event     ┌─────────┐
│  State  │ ───────────► │  State  │
│   A     │              │   B     │
└─────────┘              └─────────┘
     ▲                        │
     └────────────────────────┘
           another_event
```

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| IDLE | START | Initialize resources | RUNNING |
| RUNNING | PAUSE | Save checkpoint | PAUSED |
| RUNNING | STOP | Cleanup resources | IDLE |
| PAUSED | RESUME | Restore from checkpoint | RUNNING |

### Decision Trees

```
Is input valid?
├── No → Return ERROR_INVALID_INPUT
└── Yes → Is resource available?
    ├── No → Return ERROR_RESOURCE_BUSY
    └── Yes → Process request
        ├── Success → Return SUCCESS
        └── Failure → Return ERROR_PROCESSING
```

## Invariants

Properties that MUST always hold true:

1. **Invariant Name**: Description of the invariant
   - Verification: How to check this (assertion, test, etc.)
   
2. **Another Invariant**: Description
   - Verification: How to check

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `E_EXAMPLE` | Description of when this occurs | What caller should do |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_{feature}.cpp` | Basic functionality |
| `tests/unit/test_{feature}_edge_cases.cpp` | Edge cases and error paths |

## Migration Notes

If this spec represents a change from previous behavior:

- What changed
- Why it changed
- How to migrate existing code
- Deprecation timeline

## Related Specifications

- [Related Spec 1](./path/to/spec1.md) - How it relates
- [Related Spec 2](./path/to/spec2.md) - How it relates

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Term1 | Definition |
| Term2 | Definition |

### References

- External documentation links
- Academic papers
- Design documents

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | YYYY-MM-DD | Initial specification | Name |
