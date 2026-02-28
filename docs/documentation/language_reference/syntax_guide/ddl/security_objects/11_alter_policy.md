# ALTER POLICY

[Prev](./10_create_policy.md) | [Next](./12_drop_policy.md) | [Topic README](./README.md) | [Language Reference README](../../../README.md) | [Documentation Workspace README](../../../../README.md)

## Coverage and Evidence Status

Status: Partial (source/test anchors are present; behavioral claims deferred for PH2).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_gap_contracts.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_native_extension_surface.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: No executable command-level examples were added in this pass.

## Intent

Define the exact parser-facing syntax contract for this statement/object surface.

## Canonical Syntax Forms To Document

- List every accepted canonical form, including required and optional clauses.
- Distinguish lifecycle actions (`CREATE`, `ALTER`, `DROP`, and control actions) where applicable.
- Include family/object boundaries and command dispatch expectations.

## Clause and Option Matrix

- Document each clause in deterministic order.
- Provide defaults, constraints, incompatibilities, and scope rules.
- Include context-sensitive rules enforced by parser/semantic layers.

## Parser Acceptance and Rejection Cases

- Add positive syntax samples that must parse.
- Add negative samples that must reject with expected error classes.
- Capture alias/deprecation behavior when compatibility paths exist.

## Examples

- Provide concise examples for common and advanced forms.
- Include at least one example showing interaction with related objects.

## Completion Checklist

- [ ] Canonical forms documented
- [ ] Clause matrix completed
- [ ] Positive and negative parser cases listed
- [ ] Examples validated against v3 parser behavior
