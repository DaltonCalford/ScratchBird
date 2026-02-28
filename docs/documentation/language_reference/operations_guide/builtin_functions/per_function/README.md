# Per-Function Reference Pages

## Coverage and Evidence Status

Status: Deferred to next pass (no current implementation proof in this revision).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_gap_contracts.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: this section is a navigation or index page; operational content is documented in child statement/system pages and is staged for a later pass.


[Built-in Functions README](../README.md) | [Operations Guide README](../../README.md) | [Language Reference README](../../../README.md) | [Documentation Workspace README](../../../../README.md)

Create one page per canonical built-in function. Use category directories below.

## Category Directories

- [Scalar and Utility Functions](scalar_general/README.md)
- [Window Functions](window/README.md)
- [Document and Path Functions](document/README.md)
- [Time-Series Functions](timeseries/README.md)
- [Search Functions](search/README.md)
- [Vector and ANN Functions](vector/README.md)
- [UDR Compile-Bridge Functions](udr_bridge/README.md)

## Starter Template

- [template_function_reference.md](template_function_reference.md)

## Required Pattern

- Keep canonical names in filenames: `f_<function_name>.md`.
- Add signature tables, type/cast rules, runtime semantics, and parser acceptance/rejection examples.
- Include source/test evidence anchors for each function page.
