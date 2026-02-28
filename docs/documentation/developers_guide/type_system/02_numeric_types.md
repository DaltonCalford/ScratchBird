# Numeric Types

[Prev](./01_builtin_type_inventory.md) | [Next](./03_character_and_text_types.md) | [Topic README](./README.md) | [Developers Guide README](../README.md)

## Coverage and Evidence Status

Status: Partial (source/test anchors are present; behavioral claims deferred for PH2).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/storage_engine.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/server/scratchbird_server.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/server/sb_server_main.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_storage_engine.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_database_bootstrap.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_server.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: Architecture sections remain scaffolded and need concrete component-level flow examples.

## Purpose

Describe this subsystem at implementation depth for developers.

## What Must Be Documented

- Integer/decimal/floating type definitions
- Range, precision, and conversion rules
- Overflow/underflow and rounding behavior
- Cross-protocol representation expectations

## Diagram Description Requirements

- Provide a text-first diagram description (nodes, edges, and direction of flow).
- Identify process boundaries, trust boundaries, and ownership boundaries.
- Describe startup sequence, steady-state flow, and failure/recovery flow.

## Source Anchors To Add

- List concrete code paths and tests that prove behavior.
- Include protocol/parser/engine boundaries where applicable.

## Completion Checklist

- [ ] Scope documented with concrete behavior
- [ ] Diagram narrative added
- [ ] Code/test anchors added
- [ ] Contradictions and open questions listed
