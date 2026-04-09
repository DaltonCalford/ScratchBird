Status: reconstructed_required

# Index Family Metrics Freshness and Publication Certification Model

## Purpose

This document defines the certification evidence required for per-family metrics publication, freshness marking, invalidation, and planner consumption.

## Required Certification Classes

Certification shall cover:

- initial metrics publication
- explicit invalidation after family state change
- degraded planning with aged or stale metrics
- planner refusal with unusable metrics
- refresh leading to new publication epoch

## Required Evidence

Each certification case shall preserve:

- index identity
- family identity
- prior publication epoch
- new publication epoch or invalidation event
- freshness class before planning
- planner outcome
- candidate-bundle explanation

## Failure Criteria

Certification fails when:

- metrics appear current after known invalidation
- planner uses a family without any published freshness or confidence state
- family refusal occurs with no explicit unusable or safety reason
- refresh changes planner behavior without a traceable publication-epoch change

## Non-Guarantees

This file does not require every implementation harness to exist already. It defines the certification target for the recovered metrics-publication model.
