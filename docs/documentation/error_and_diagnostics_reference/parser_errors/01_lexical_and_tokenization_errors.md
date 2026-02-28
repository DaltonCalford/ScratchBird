# Lexical and Tokenization Errors

[Prev](./README.md) | [Next](./02_syntax_and_context_sensitive_errors.md) | [Topic README](./README.md) | [Guide README](../README.md) | [Documentation Workspace README](../../README.md)

## Coverage and Evidence Status

Status: Partial (source/test anchors are present; behavioral claims deferred for PH2).

- Source anchor: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/vnext_error_codes.h:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/error_context.h:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/heap_toast_lob_diagnostics.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_error_paths.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_engine_error_code_harmonization.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_heap_toast_lob_diagnostics.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: Parser/engine error behavior examples and severity/impact taxonomy are still pending evidence capture.

## Purpose

Describe this area as an implementation-facing reference for operators and developers.

## What Must Be Documented

- Exact behavior and intent.
- Inputs, outputs, constraints, and side effects.
- Interactions with related subsystems.
- Failure modes and corrective actions.

## Required Content

- Source anchors in code and tests.
- Deterministic command examples.
- Validation steps and expected outcomes.
- Operational cautions and escalation triggers.

## Completion Checklist

- [ ] Behavior documented
- [ ] Inputs and constraints documented
- [ ] Source and test anchors added
- [ ] Validation and troubleshooting added
