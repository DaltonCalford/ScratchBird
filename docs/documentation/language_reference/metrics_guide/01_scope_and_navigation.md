# Scope and Navigation

[Prev](./README.md) | [Next](./monitoring_surfaces/README.md) | [Topic README](./README.md) | [Language Reference README](../README.md) | [Documentation Workspace README](../../README.md)

## Coverage and Evidence Status

Status: Partial (source/test anchors are present; behavioral claims deferred for PH2).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_gap_contracts.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_native_extension_surface.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: No executable command-level examples were added in this pass.

## Purpose

Define how to read and use the metrics guide across engine operations, language surfaces, and index management.

## What Must Be Documented

- Metrics audience split (operators, developers, SRE, incident responders).
- How metrics guide complements syntax and operations guides.
- Rules for establishing and revising normal/warning/panic ranges.
- Cross-links to transaction, security, and cluster observability topics.

## Required Navigation Contract

- Every metrics page must include source locations and metric identifiers.
- Every threshold page must include concrete remediation actions.
- Every index page must include index-type-specific health indicators.

## Completion Checklist

- [ ] Audience and usage model documented
- [ ] Cross-guide boundaries documented
- [ ] Threshold governance documented
- [ ] Navigation contract documented
