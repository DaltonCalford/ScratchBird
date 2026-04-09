# Section 00 Dependencies

Status: current_authority

## Runtime dependency summary

- section 00 depends on the build graph to prove which runtime libraries are canonical
- section 00 depends on shared ID aliases and UUIDv7 definitions to prove durable identity semantics
- section 00 depends on transaction-manager horizon logic to prove MGA-style visibility and anti-WAL governance
- section 00 depends on dialect compiler entry points to prove parser front-door lowering into SBLR
- section 00 depends on catalog artifact metadata to prove lowering provenance survives past parsing

## Cross-section dependency summary

- section 02 consumes section-00 identity and ownership assumptions
- section 05 depends on section-00 anti-WAL and binary-governance rules
- section 08 depends directly on section-00 MGA and always-in-transaction invariants
- section 22 depends on section-00 parser and execution-path boundaries
- section 24 depends on section-00 artifact and subsystem-boundary rules
- section 31 depends on section-00 governance for gate and proof claims

## Primary audit entry points

- ScratchBird/src/CMakeLists.txt
- ScratchBird/include/scratchbird/core/types.h
- ScratchBird/include/scratchbird/core/uuidv7.h
- ScratchBird/src/core/transaction_manager.cpp
- ScratchBird/src/sblr/dialect_compiler_udr.cpp
- ScratchBird/include/scratchbird/core/catalog_manager.h

## Non-guarantees

- no single generated dependency graph is claimed here
- no claim is made that driver and tooling dependency closure is fully mapped in this section
- no claim is made that the historical promotion record is current dependency authority
