Status: reconstructed_required

# Remote Management Queue Drift and Apply Certification Model

## Purpose

This document defines the certification evidence required for the reconstructed remote-management queue and dual-persistence model.

## Required Proof Classes

Certification shall preserve proof for:

- queue admission
- capability-based assessment
- local apply execution
- local-plus-cluster persistence alignment
- drift detection
- quarantine and repair classification

## Required Scenarios

The certification corpus shall include:

- admitted instruction applied successfully
- instruction refused during assessment
- local apply succeeds before cluster confirmation
- cluster record exists before local confirmation
- detected drift between intended and local state
- quarantined instruction requiring operator repair

## Evidence Record

Each certification case shall preserve:

- instruction identity
- target identity
- lifecycle trace
- local persistence state
- cluster persistence state
- resulting drift or quarantine classification
- final disposition

## Failure Criteria

Certification fails when:

- lifecycle states are missing or ambiguous
- local and cluster records cannot be compared deterministically
- drift is detected but not classifiable
- a successful apply cannot be tied back to a queued instruction identity

## Non-Guarantees

This file does not require the current codebase to have every certification harness already built. It defines the proof requirements for implementation-ready conformance.
