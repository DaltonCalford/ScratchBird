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
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/error_context.h:17
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_transaction_manager.cpp:50
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_transaction_vnext_contract.cpp:48
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_deadlock_detection.cpp:113
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_query_compiler_v3.cpp:124
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172440Z/LINK_CHECK.txt
- Why partial: this is a top-level architecture index; detailed invariants and subsystem flows are in child pages under this guide.


The developers guide is split into major topics and then into small, task-focused documents for easy review/editing.

## Topics

- [Architecture](architecture/README.md)
- [Transactions and MGA](transactions_and_mga/README.md)
- [Security](security/README.md)
- [Type System](type_system/README.md)
- [System Domains](system_domains/README.md)
- [System Catalog](system_catalog/README.md)
- [Emulation and Protocol](emulation_and_protocol/README.md)

## Required Scope

This guide is expected to cover:

- Runtime architecture including embedded engine, parsers, listeners, managers, and CLI modes
- Group design (non-trusted members with per-connection auth) and cluster design (trusted channels)
- MGA vs WAL mechanics and transaction internals
- Full built-in datatype inventory (excluding user/emulation domains)
- System domain inventory (excluding emulation-support domains)
- Full system catalog coverage for base and dynamic structures
