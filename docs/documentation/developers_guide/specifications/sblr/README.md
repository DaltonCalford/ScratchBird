# SBLR (ScratchBird Language Runtime) Opcode Specifications

This directory contains comprehensive specifications for all SBLR v3 opcodes.

## Specification Files

| File | Description | Opcodes Covered |
|------|-------------|-----------------|
| [opcodes_system.md](./opcodes_system.md) | System & Control opcodes | 0x0001-0x0003, 0x0301-0x0318 |
| [opcodes_ddl.md](./opcodes_ddl.md) | Data Definition Language | 0x0101-0x017C |
| [opcodes_dml.md](./opcodes_dml.md) | Data Manipulation Language | 0x0201-0x0213, 0x061F-0x062B |
| [opcodes_query.md](./opcodes_query.md) | SELECT & Query processing | 0x0212, 0x0601-0x0660 |
| [opcodes_expressions.md](./opcodes_expressions.md) | Expression evaluation | 0x0701-0x0A0C, 0x0B00-0x0C33 |
| [opcodes_index.md](./opcodes_index.md) | Index operations | 0x0E02-0x0E24 |
| [opcodes_utility.md](./opcodes_utility.md) | Utility & Admin commands | 0x0501-0x0564 |

## Quick Reference

### Opcode Ranges

| Range | Category | Count |
|-------|----------|-------|
| 0x0001-0x00FF | System & Control | 3 |
| 0x0101-0x01FF | DDL Operations | 124 |
| 0x0201-0x02FF | DML Operations | 18 |
| 0x0301-0x03FF | Transaction Control | 24 |
| 0x0401-0x04FF | Security & Grants | 16 |
| 0x0501-0x05FF | Utility Commands | 72 |
| 0x0601-0x06FF | Query Structure | 96 |
| 0x0701-0x07FF | Expressions & Operators | 29 |
| 0x0801-0x08FF | SQL Functions | 206 |
| 0x0901-0x09FF | Aggregate Functions | 38 |
| 0x0A01-0x0AFF | Window Functions | 11 |
| 0x0B00-0x0BFF | Type Identifiers | 71 |
| 0x0C01-0x0CFF | Literal Constructors | 51 |
| 0x0D01-0x0DFF | PSQL / Stored Code | 66 |
| 0x0E01-0x0EFF | Index Operations | 17 |
| 0x0F01-0x0FFF | Array & Range Operations | 42 |
| 0x1001-0x10FF | Text Search & Regex | 13 |
| 0x1101-0x11FF | Spatial/Geometry | 37 |
| 0x1201-0x12FF | Job Scheduler | 2 |
| 0x6000-0x6FFF | Extended / vNext | 115 |

**Total: 877 opcodes**

## Source Files

- **Opcode Definitions**: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h`
- **Opcode Names**: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp`
- **Executor Implementation**: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp`
- **Payload Schemas**: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp`

## Template

Specifications follow the template defined in:
- `/home/dcalford/CliWork/ScratchBird/docs/documentation/developers_guide/specifications/TEMPLATE.md`

## Related Documentation

- [v3_container_format.md](./v3_container_format.md) - Bytecode container structure
- [v3_execution_model.md](./v3_execution_model.md) - Execution semantics
- [v3_opcode_reference.md](./v3_opcode_reference.md) - Quick opcode reference
- [v3_payload_schemas.md](./v3_payload_schemas.md) - Payload encoding details

## Version

- **Spec Version**: 1.0.0
- **Last Updated**: 2026-03-08
- **Status**: Approved
