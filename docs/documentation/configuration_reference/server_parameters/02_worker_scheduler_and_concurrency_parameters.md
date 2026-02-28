# Worker, Scheduler, and Concurrency Parameters

[Prev](./01_listener_network_and_protocol_parameters.md) | [Next](./03_storage_checkpoint_and_snapshot_parameters.md) | [Topic README](./README.md) | [Guide README](../README.md) | [Documentation Workspace README](../../README.md)

## Coverage and Evidence Status

Status: Partial (source/test anchors are present; behavioral claims deferred for PH2).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/config.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/server/config_parser.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/config.h:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/ts_config.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_config.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_state_v3.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: Behavioral option tables, defaults, and precedence are not yet fully populated from executable cases.

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
