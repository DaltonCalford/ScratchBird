# Metrics Guide

## Coverage and Evidence Status

Status: Deferred to next pass (no current implementation proof in this revision).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_gap_contracts.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: this section is a navigation or index page; operational content is documented in child statement/system pages and is staged for a later pass.


[Language Reference README](../README.md) | [Documentation Workspace README](../../README.md)

This guide defines operational metrics across ScratchBird language/runtime surfaces.
It explains what each metric means, where to find it, how to use it, and which values are normal, concerning, or critical.

## Scope

- Monitoring tables and catalog-backed metric surfaces
- In-memory-only metric tables/views and runtime counters
- Index-class observability (what each index type is, why it exists, and how to manage it)
- Baseline thresholds (normal/warning/panic) and response expectations
- Incident/playbook usage for tuning, repair, and escalation

## Topic Areas

- [Scope and Navigation](01_scope_and_navigation.md)
- [Monitoring Surfaces](monitoring_surfaces/README.md)
- [Metric Catalog](metric_catalog/README.md)
- [Index Observability](index_observability/README.md)
- [Baseline Thresholds and Alerting](baseline_thresholds_and_alerting/README.md)
- [Operations Playbooks](operations_playbooks/README.md)
