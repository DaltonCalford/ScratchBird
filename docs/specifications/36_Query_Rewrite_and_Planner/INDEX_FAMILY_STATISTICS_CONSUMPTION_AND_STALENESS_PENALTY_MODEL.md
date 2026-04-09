Status: reconstructed_required

# Index Family Statistics Consumption and Staleness Penalty Model

## Purpose

This document defines how the optimizer consumes the published per-family statistics model and applies staleness penalties without demoting families to secondary status.

## Canonical Rule

The optimizer shall consume the published family metrics together with their freshness and confidence state. Staleness changes cost and may trigger refresh or replan, but it does not erase primary-class family participation.

## Required Consumption Inputs

The planner shall consume:

- family-metrics payload
- publication epoch
- freshness class
- confidence class
- invalidation state
- visibility reject signal

## Penalty Rule

The planner shall apply explicit penalties for:

- `AGED` metrics
- `STALE_DEGRADED` metrics
- low confidence
- high visibility reject rate
- invalidated-but-not-refreshed state where policy still allows degraded participation

## Refusal Rule

The family may be refused only when:

- metrics are `UNUSABLE`
- family-specific safety rules require refusal
- no legal degraded participation remains

The refusal shall be explicit in the candidate bundle.

## Replan Rule

Planner-visible change in publication epoch, freshness, or invalidation may trigger replan or plan-cache invalidation according to the plan-cache policy. The trigger must be explainable from these same published fields.

## Non-Guarantees

This file does not require all families to use the same penalty curve. It requires every family to participate under the same published freshness and invalidation discipline.
