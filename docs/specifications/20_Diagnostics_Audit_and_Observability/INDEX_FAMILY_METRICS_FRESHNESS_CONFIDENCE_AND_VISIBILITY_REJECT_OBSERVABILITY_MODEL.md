Status: reconstructed_required

# Index Family Metrics Freshness Confidence and Visibility Reject Observability Model

## Purpose

This document defines the operator-visible observability contract for per-family metrics freshness, confidence, and visibility-reject behavior.

## Canonical Rule

Operators shall be able to inspect not only raw family metrics but also the freshness and confidence state that determine how the optimizer interprets those metrics.

## Required Observability Fields

For each implemented family and index identity, the observability surface shall expose:

- latest publication epoch
- freshness class
- confidence class
- invalidation state
- visibility reject rate or count
- maintenance debt indicator
- resident or acceleration state where applicable

## Visibility Reject Rule

Visibility reject metrics are first-class operator outputs because every index family remains subordinate to MGA truth. A family producing high reject rates must surface that state explicitly.

## Non-Guarantees

This file does not require every operator surface to show identical formatting. It requires the data fields to be inspectable.
