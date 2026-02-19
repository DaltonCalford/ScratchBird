# DDL Language Guide
Last modified: 2026-02-19

Back links:
- [Language Guide README](../README.md)

This section documents SQL object lifecycle coverage by object family and object type. Every object directory contains:
- lifecycle files: create.md, alter.md, show.md, describe.md, drop.md
- object README with lifecycle matrix and links

## DDL Families
- [Management](management/README.md)
- [Data Storage](data-storage/README.md)
- [Programmability](programmability/README.md)
- [Security](security/README.md)
- [Integration](integration/README.md)
- [Cluster And Service](cluster-and-service/README.md)

## Coverage Model
- Supported: explicit parser command form exists for the lifecycle phase.
- Partial: command form exists with constrained behavior or incomplete lifecycle closure.
- Not available: no explicit command form exists in native v3 0.1.0.

## Notes
- Lifecycle coverage is evidence-based from parser/emitter/executor in beta 0.1.0.
- Runtime-partial command families are linked to [Future TODO (0.2.0)](../TODO_BETA_0_2_0.md).
- Identifier and naming behavior is documented in [DDL Management Naming Rules](management/object-naming-and-identifiers.md).
