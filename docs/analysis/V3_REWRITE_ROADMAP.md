# V3 Rewrite Roadmap

Date: 2026-02-08

This roadmap decomposes the V3 compliance work into deliverable milestones aligned to the current codebase structure.

## Milestone 0: Scope Lock + Architecture Decisions
- Freeze authoritative V3 specs and record SHA set from `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- Decide migration strategy: `V2 -> V3` replacement or `V2+V3` coexistence with feature flags.
- Define V3 module boundaries and build targets.

## Milestone 1: V3 AST + Type/Literal System
- Implement V3 TypeSpec + Literal nodes (UUID v7 catalog IDs).
- Implement V3 AST arena and visitor interfaces.
- Update parser to build V3 AST (or add parallel V3 AST and mapping layer).
- Add resolver to bind catalog UUID v7 IDs before emission.

## Milestone 2: V3 SBLR Pipeline (Core)
- Add V3 opcode definitions and container format.
- Implement V3 payload encoders/decoders.
- Implement constant pool + symbol table.
- Implement canonicalization pass.
- Implement validation pass.

## Milestone 3: V3 SBLR Emission
- Implement V3 bytecode generator from V3 AST.
- Enforce `PARSER_TO_SBLR_EMISSION_RULES.md`.
- Add regression tests for edge cases.

## Milestone 4: V3 Executor + PSQL Runtime
- Implement V3 opcode semantics.
- Implement V3 SQL engine semantics.
- Implement V3 PSQL runtime semantics and scoping rules.

## Milestone 5: Dialect Emulation
- Update emulated parsers (MySQL/PostgreSQL) to emit canonical V3 SBLR.
- Enforce dialect gap assertions in tests.

## Milestone 6: Audit-and-Align Subsystems
- Server lifecycle and recovery
- Network/IPC contract
- Catalog schema + UUID lifecycle
- Storage formats + TOAST integration
- Transaction/MGA and lock manager
- Index architecture + GC protocols

## Milestone 7: Ops/Tools/Testing
- Implement metrics and monitoring views per V3 ops specs.
- Update tools/CLI and deployment.
- Implement full conformance test suite.

## Exit Criteria
- All V3 SBLR test vectors pass.
- Dialect conformance assertions pass.
- No-grey-areas gate passes.
- Critical benchmarks meet `PERFORMANCE_BENCHMARKS.md` targets.
