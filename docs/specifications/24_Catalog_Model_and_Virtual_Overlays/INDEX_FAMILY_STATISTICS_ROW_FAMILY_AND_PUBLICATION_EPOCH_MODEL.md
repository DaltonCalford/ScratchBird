Status: reconstructed_required

# Index Family Statistics Row Family and Publication Epoch Model

## Purpose

This document defines the canonical catalog row-family substrate for per-family index metrics and their publication epochs.

## Canonical Rule

Index-family statistics shall be represented by explicit metadata row families or equivalent catalog-backed records. Effective planner-visible metrics require committed publication state rather than ad hoc in-memory only assumptions.

## Canonical Row Families

The canonical row families are:

- family-metrics header rows
- family-metrics payload rows
- publication-epoch rows
- invalidation-event rows
- refresh-request rows where administrative refresh is supported

## Required Header Fields

Header rows shall preserve:

- index identity
- runtime family
- alias surface if any
- latest publication epoch
- latest freshness class
- latest confidence class
- current invalidation state

## Required Payload Fields

Payload rows shall preserve:

- typed metrics payload identity
- family-native metric fields
- visibility reject counts or rates
- maintenance debt indicators
- resident or acceleration state where relevant

## Commit Rule

Published metrics become authoritative for planner consumption only after committed metadata publication under ordinary MGA transaction rules. Uncommitted refresh work shall not masquerade as committed current metrics.

## Non-Guarantees

This file does not require all payloads to share one rigid physical row shape. It requires explicit catalog-backed row families and publication epochs.
