# Firebird Reference Documentation

**[← Back to Reference Documentation](../README.md)** | **[← Back to Specifications Index](../../README.md)**

This directory contains complete Firebird reference documentation used as the basis for ScratchBird's MGA implementation and Firebird dialect emulation.

## Overview

ScratchBird implements Firebird's Multi-Generational Architecture (MGA) and provides full Firebird SQL dialect compatibility. These reference documents define the target behavior.

## Reference Documents

### Core Documentation

- **[FirebirdReferenceDocument.md](FirebirdReferenceDocument.md)** (50,000+ lines) - Complete Firebird SQL reference
  - SQL syntax and semantics
  - Data types and operators
  - Built-in functions
  - Stored procedures and triggers
  - Transaction management
  - Security model

### PDF References

- **[firebird-50-language-reference.pdf](firebird-50-language-reference.pdf)** - Firebird 5.0 Language Reference (official)
- **[firebird-isql.pdf](firebird-isql.pdf)** - ISQL command-line tool reference
- **[2_newtransactionfeatures.pdf](2_newtransactionfeatures.pdf)** - New transaction features documentation

### Split Documentation

- **[firebird_docs_split/](firebird_docs_split/)** - Firebird documentation split into multiple files for easier navigation

## Usage

These documents are used for:

1. **MGA Implementation** - Define Firebird's Multi-Generational Architecture
2. **Dialect Compatibility** - Ensure SQL dialect compatibility
3. **Feature Parity** - Verify feature completeness
4. **Behavior Verification** - Test behavior matches Firebird
5. **Documentation Reference** - Cross-reference for ScratchBird documentation

## Key Firebird Features Implemented in ScratchBird

### Multi-Generational Architecture (MGA)

- **Multiple record versions** - In-place record versioning
- **No undo logs** - No separate undo/write-after log (WAL) logs
- **Non-blocking reads** - Readers never block writers
- **Snapshot isolation** - Consistent snapshots per transaction
- **Garbage collection** - Cooperative garbage collection

### Transaction Management

- **Isolation levels** - READ COMMITTED, SNAPSHOT, TABLE STABILITY
- **Lock resolution** - Wait/no-wait lock modes
- **Record versioning** - Transaction-level version visibility

### SQL Features

- **PSQL** - Procedural SQL language
- **Triggers** - BEFORE/AFTER row/statement triggers
- **Stored procedures** - Selectable and executable procedures
- **Generators (Sequences)** - Sequence generators
- **Domains** - User-defined domains with constraints
- **Blobs** - Binary large object storage

## Related Specifications

- [../../MGA_RULES.md](../../../../MGA_RULES.md) - **CRITICAL** - ScratchBird's MGA rules based on Firebird
- [../../transaction/](../../transaction/) - Transaction system implementing Firebird MGA
- [../../storage/](../../storage/) - Storage engine implementing Firebird MGA
- [../../parser/](../../parser/) - Firebird SQL dialect parser

## Critical Reading

Before implementing Firebird-compatible features:

1. **MUST READ:** [../../../../MGA_RULES.md](../../../../MGA_RULES.md) - Absolute MGA rules
2. **REFERENCE:** [FirebirdReferenceDocument.md](FirebirdReferenceDocument.md) - Firebird behavior reference
3. **VERIFY:** Behavior matches Firebird's implementation

## Navigation

- **Parent Directory:** [Reference Documentation](../README.md)
- **Specifications Index:** [Specifications Index](../../README.md)
- **Project Root:** [ScratchBird Home](../../../../README.md)

---

**Last Updated:** January 2026
