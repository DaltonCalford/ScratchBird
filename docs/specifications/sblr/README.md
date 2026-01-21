# SBLR Bytecode Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains specifications for ScratchBird's Bytecode Language Runtime (SBLR), the low-level bytecode format used for query execution and stored procedures.

## Overview

SBLR (ScratchBird Bytecode Language Runtime) is ScratchBird's bytecode execution layer, inspired by Firebird's BLR (Binary Language Representation). All SQL queries and procedural code compile to SBLR bytecode before execution, providing a common execution format across all SQL dialects.

## Specifications in this Directory

### Core SBLR

- **[Appendix_A_SBLR_BYTECODE.md](Appendix_A_SBLR_BYTECODE.md)** (519 lines) - Complete SBLR bytecode specification
- **[SBLR_OPCODE_REGISTRY.md](SBLR_OPCODE_REGISTRY.md)** (84 lines) - Central registry of SBLR opcodes
- **[SBLR_EXECUTION_PERFORMANCE_RESEARCH.md](SBLR_EXECUTION_PERFORMANCE_RESEARCH.md)** - Performance research and options for faster SBLR execution
- **[SBLR_EXECUTION_PERFORMANCE_ALPHA.md](SBLR_EXECUTION_PERFORMANCE_ALPHA.md)** - Alpha performance spec (tier 0-2)
- **[SBLR_EXECUTION_PERFORMANCE_BETA.md](SBLR_EXECUTION_PERFORMANCE_BETA.md)** - Beta performance spec (JIT/AOT, vectorization)

### Domain Integration

- **[SBLR_DOMAIN_PAYLOADS.md](SBLR_DOMAIN_PAYLOADS.md)** (215 lines) - Domain-specific opcode payloads and operations

### Firebird Compatibility

- **[FIREBIRD_BLR_TO_SBLR_MAPPING.md](FIREBIRD_BLR_TO_SBLR_MAPPING.md)** (173 lines) - Mapping from Firebird BLR to SBLR opcodes
- **[FIREBIRD_BLR_FIXTURES.md](FIREBIRD_BLR_FIXTURES.md)** (153 lines) - Test fixtures for BLR compatibility
- **[FIREBIRD_TRANSACTION_MODEL_SPEC.md](FIREBIRD_TRANSACTION_MODEL_SPEC.md)** (1,570 lines) - Firebird transaction model reference

## Key Concepts

### Bytecode Architecture

SBLR provides a stack-based bytecode execution model:

1. **Opcodes** - Low-level operations (arithmetic, comparison, control flow)
2. **Stack machine** - Expression evaluation using operand stack
3. **Register allocation** - Efficient register usage for local variables
4. **Type safety** - Bytecode verifier ensures type correctness

### Compilation Pipeline

SQL → AST → Logical Plan → SBLR Bytecode → Execution

```
┌──────────┐    ┌─────────┐    ┌──────────┐    ┌──────────┐    ┌───────────┐
│  Parser  │───▶│ Analyzer│───▶│Optimizer │───▶│SBLR Gen  │───▶│ Executor  │
└──────────┘    └─────────┘    └──────────┘    └──────────┘    └───────────┘
   (SQL)          (AST)          (Plan)          (Bytecode)      (Results)
```

### Opcode Categories

- **Arithmetic** - ADD, SUB, MUL, DIV, MOD
- **Comparison** - EQ, NE, LT, LE, GT, GE
- **Logical** - AND, OR, NOT
- **Stack** - PUSH, POP, DUP, SWAP
- **Control Flow** - JUMP, BRANCH, CALL, RETURN
- **Data Access** - LOAD, STORE, FETCH
- **Aggregate** - SUM, COUNT, AVG, MIN, MAX
- **Domain** - Domain validation, constraint checking

### Domain-Specific Opcodes

See [SBLR_DOMAIN_PAYLOADS.md](SBLR_DOMAIN_PAYLOADS.md) for domain operations:

- **DOMAIN_CHECK** - Validate value against domain constraints
- **DOMAIN_CAST** - Cast value to domain type
- **ENUM_VALIDATE** - Validate enum value
- **VARIANT_TAG** - Get variant discriminator

## Firebird BLR Compatibility

ScratchBird maintains compatibility with Firebird's BLR format:

- **BLR import** - Read Firebird stored procedures and triggers
- **Opcode mapping** - BLR opcodes map to equivalent SBLR opcodes
- **Semantic preservation** - Identical execution semantics

See [FIREBIRD_BLR_TO_SBLR_MAPPING.md](FIREBIRD_BLR_TO_SBLR_MAPPING.md) for details.

## Related Specifications

- [Parser](../parser/) - SQL parsing to AST
- [Query Optimization](../query/) - Query plan generation
- [Transaction System](../transaction/) - MGA integration
- [Types System](../types/) - Data type operations
- [Storage Engine](../storage/) - Data access operations

## Critical Reading

Before working on SBLR implementation:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ IN ORDER:**
   - [Appendix_A_SBLR_BYTECODE.md](Appendix_A_SBLR_BYTECODE.md) - Core bytecode spec
   - [SBLR_OPCODE_REGISTRY.md](SBLR_OPCODE_REGISTRY.md) - Opcode reference
   - [SBLR_DOMAIN_PAYLOADS.md](SBLR_DOMAIN_PAYLOADS.md) - Domain operations

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
