# PUBLIC_BETA_REQUIRED_GATE_EXECUTION_AND_FAILURE_MODEL

## Status

Current code-backed authority.

## Purpose

This document defines the required execution and failure model for the repository-local public-beta conformance gate.

## Scope

This specification covers the repo-local `tests/conformance/public_beta/run_required_public_beta_gate.sh` lane and the higher-order certification meaning attached to that lane.

It does not redefine the external `ScratchBird-Benchmarks` matrix.

## Gate role

The public-beta gate is a required conformance aggregator. It is not one more standalone test. It is the shell entrypoint that assembles required test families into a single release-readiness decision lane.

## Required execution model

The public-beta gate shall:

1. prepare the required runtime and result environment for the gate
2. invoke the required subordinate test families in the scripted order
3. preserve per-substep outputs needed for later audit
4. return non-zero on the first gate-failing condition unless the script explicitly enters a preserve-and-continue mode for artifact collection

## Failure semantics

The gate is fail-closed.

The gate shall be considered failed when any required subordinate condition fails, including:

1. build or prerequisite setup failure
2. missing required binaries or scripts
3. subordinate `ctest` failure
4. subordinate compatibility or conformance script failure
5. required result-root creation failure
6. required artifact copy or preservation failure when the gate contract says artifacts must be retained

## Artifact retention requirements

The public-beta gate shall retain enough information for later audit to answer:

1. which substeps were attempted
2. which substeps succeeded or failed
3. where the per-step result roots were written
4. which compatibility or conformance result directories belong to the run
5. whether the run was complete, partially failed, or aborted before required substeps executed

## Required output classes

The gate output model includes:

1. shell-visible pass/fail exit status
2. subordinate runner stdout and stderr
3. build-tree `ctest` metadata for any `ctest`-driven substeps
4. per-lane result directories for compatibility or conformance families that emit dedicated output roots

## Ordering model

The public-beta gate is authoritative for the scripted ordering of the required beta-readiness checks. Another agent shall not reorder the required substeps unless the gate specification is explicitly revised, because ordering determines:

1. prerequisite validity
2. artifact tree coherence
3. failure interpretation
4. auditability

## Relationship to clean/build/test cycle

The public-beta gate is adjacent to but not identical with a generic developer clean/build/test loop.

A generic developer loop may run unit or integration `ctest` successfully and still not satisfy public-beta release-readiness if the required conformance or compatibility lanes were not executed through the gate.

## Required implementer interpretation

Another agent shall treat the public-beta gate as:

1. a required certification aggregator
2. a preserved-artifact execution lane
3. a fail-closed readiness decision
4. a higher-order script contract, not just a directory label
