# Language Reference

## Coverage and Evidence Status

Status: Partial (source/test anchors are present; implementation detail behavior is still documented in child pages).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:562
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/postgresql/pg_parser.cpp:328
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/postgresql/pg_parser_dml.cpp:39
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp:378
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_postgresql_parser.cpp:423
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_query_compiler_v3.cpp:124
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_nosql_emitter_contract.cpp:133
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_gap_contracts.cpp:17
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172440Z/LINK_CHECK.txt
- Why partial: this index page is fully documented as a navigation surface, while statement and subsystem specifics are captured in child guides under syntax_guide and metrics_guide.


[Documentation Workspace README](../README.md)

The language reference is split into three complementary guides:

- `operations_guide`: explains how language surfaces work together operationally.
- `syntax_guide`: defines exact parser-facing syntax by family/object/lifecycle.
- `metrics_guide`: defines monitoring surfaces, metric semantics, and operating thresholds.

## Guide Directories

- [operations_guide/](operations_guide/README.md)
- [syntax_guide/](syntax_guide/README.md)
- [metrics_guide/](metrics_guide/README.md)

## Authoring Rules

- Treat v3 native syntax as canonical.
- Treat v3 as the core parser contract; PostgreSQL/MySQL/Firebird syntax is compatibility-only.
- Keep emulation syntax isolated under explicit compatibility sections.
- Use deterministic examples tied to parser and test evidence.
- Record accepted and rejected forms for context-sensitive surfaces.
