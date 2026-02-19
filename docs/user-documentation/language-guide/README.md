# ScratchBird Native V3 Language Guide
Last modified: 2026-02-19

This directory is the authoritative, lifecycle-oriented language reference for ScratchBird native parser v3 in early beta `0.1.0`.

Back links:
- [User Documentation Index](../index.md)

Sections:
- [DDL](ddl/README.md)
- [DML](dml/README.md)
- [PSQL](psql/README.md)
- [Admin And Session](admin/README.md)
- [Data Types, Domains, Casts, Operators](data-types/README.md)
- [Functions](functions/README.md)
- [Command Group Index (CREATE/ALTER/DROP/SELECT/SET/SHOW)](command-groups/README.md)
- [Future TODO (0.2.0)](TODO_BETA_0_2_0.md)
- [Consolidated Source Audit Reference](NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md)

Coverage labels used in this tree:
- `Supported`: explicit parser surface exists and command path is closed for the documented phase.
- `Partial`: command family exists but coverage is limited (syntax subset, emitter/runtime bridge partial, or lifecycle gap).
- `Not available`: no explicit native v3 command surface for that phase in `0.1.0`.

Navigation rules used consistently:
- Every directory has a `README.md` with child links and coverage summary.
- Every lifecycle/step document links back to parent readmes.
- DDL lifecycle files follow `create -> alter -> show -> describe -> drop`.
- DML/PSQL/admin/topic series files follow an explicit `next` link chain.
