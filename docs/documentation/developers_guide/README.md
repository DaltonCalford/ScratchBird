<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Developers Guide

## Coverage and Evidence Status

Status: Partial (core parser, transaction, and architecture anchors are present; subsystem detail is in child architecture pages).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:562
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/transaction_manager.cpp:307
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/transaction_manager.cpp:407
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/transaction_manager.cpp:1703
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/permission_cache.cpp:257
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/password_policy.cpp:68

- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_transaction_manager.cpp:50
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_transaction_vnext_contract.cpp:48
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_deadlock_detection.cpp:113
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_query_compiler_v3.cpp:124
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172440Z/LINK_CHECK.txt
- Why partial: this is a top-level architecture index; detailed invariants and subsystem flows are in child pages under this guide.

The developers guide is split into major topics and then into small, task-focused documents for easy review/editing.

## Topics

- [Architecture](architecture/README.md) - High-level system topology and component relationships
- [Transactions and MGA](transactions_and_mga/README.md) - Multi-Generational Architecture internals
- [Security](security/README.md) - Authentication, authorization, and security models
- [Type System](type_system/README.md) - Built-in data types and their properties
- [System Domains](system_domains/README.md) - System-defined domains and constraints
- [System Catalog](system_catalog/README.md) - Metadata structures and catalog operations
- [Emulation and Protocol](emulation_and_protocol/README.md) - Wire protocols and dialect emulation
- **[Specifications](specifications/README.md)** - *Reverse-engineered specs with verified source anchors to actual code*

## Required Scope

This guide is expected to cover:

- Runtime architecture including embedded engine, parsers, listeners, managers, and CLI modes
- Group design (non-trusted members with per-connection auth) and cluster design (trusted channels)
- MGA vs WAL mechanics and transaction internals
- Full built-in datatype inventory (excluding user/emulation domains)
- System domain inventory (excluding emulation-support domains)
- Full system catalog coverage for base and dynamic structures

## Documentation Types

### Architecture Docs (`architecture/`, `transactions_and_mga/`, etc.)
High-level explanations of how subsystems work, design rationale, and conceptual overviews.

### Specifications (`specifications/`)
Low-level, reverse-engineered interface contracts with **verified source anchors** to actual implementation code. These are the authoritative references for:
- Function signatures and preconditions
- Data structure layouts
- State machines and algorithms
- Invariants that must be maintained

When implementing or modifying code, **consult the specs first**. When specs and implementation differ, the spec is out of date and should be updated.

## External Reference Material

Canonical specification documents now live in `docs/specifications/`. Use that tree as the authoritative specification baseline when implementation and documentation drift.

When documenting implementation details here, **reverse-engineer from the actual code** in `src/` and add verified source anchors.
