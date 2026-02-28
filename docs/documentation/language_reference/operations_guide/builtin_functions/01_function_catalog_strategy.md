# Function Catalog Strategy

[Prev](./README.md) | [Next](./categories/README.md) | [Topic README](./README.md) | [Language Reference README](../../README.md) | [Documentation Workspace README](../../../README.md)

## Coverage and Evidence Status

Status: Partial (source/test anchors are present; behavioral claims deferred for PH2).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_gap_contracts.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_native_extension_surface.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: No executable command-level examples were added in this pass.

## Purpose

Define how to maintain complete built-in function documentation coverage.

## What Must Be Documented

- Function naming conventions and canonical signatures.
- Category taxonomy and cross-category overlap rules.
- Determinism, volatility, and execution-cost annotations.

## Required Cross-References

- Link each function to syntax guide anchors where invoked.
- Link categories to per-function pages and test evidence.
- Mark compatibility aliases separately from canonical names.

## Evidence Anchors To Add

- Parser function dispatch/validation anchors.
- Runtime implementation anchors.
- Regression tests proving accepted/rejected forms.

## Completion Checklist

- [ ] Category map complete
- [ ] Signature model documented
- [ ] Determinism/volatility guidance added
- [ ] Evidence anchors added
