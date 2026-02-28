# In-Memory-Only Tables and Views

[Prev](./01_persistent_monitoring_tables.md) | [Next](./03_runtime_metric_streams_logs_and_events.md) | [Topic README](./README.md) | [Metrics Guide README](../README.md) | [Language Reference README](../../README.md) | [Documentation Workspace README](../../../README.md)

## Coverage and Evidence Status

Status: Partial (source/test anchors are present; behavioral claims deferred for PH2).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_gap_contracts.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_native_extension_surface.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: No executable command-level examples were added in this pass.

## Purpose

Describe this metrics surface at operational depth: what it measures, where it is sourced, and how to act on it.

## What Must Be Documented

- Metric/object definition and why it exists.
- Data source location (table/view/in-memory surface/log/stream).
- Collection cadence, retention, reset semantics, and cardinality notes.
- Normal, warning, and panic operating ranges.

## Index and Workload Management Guidance

- Explain how this surface impacts query/index behavior.
- Define which index classes or workload lanes it applies to.
- Provide operational controls and mitigation levers.

## Required Tables

- Metric catalog table (metric_name, unit, source, scope, usage).
- Threshold table (metric_name, normal, warning, panic, action).
- Troubleshooting table (symptom, likely_cause, verify, mitigation).

## Evidence Anchors To Add

- Parser/engine code anchors where metrics are produced.
- Monitoring/catalog schema anchors.
- Test or runtime artifact anchors proving values and transitions.

## Completion Checklist

- [ ] Definitions and sources documented
- [ ] Thresholds documented (normal/warning/panic)
- [ ] Management actions documented
- [ ] Evidence anchors added
