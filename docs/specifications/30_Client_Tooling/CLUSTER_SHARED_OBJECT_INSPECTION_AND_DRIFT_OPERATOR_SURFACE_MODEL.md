Status: reconstructed_required

# Cluster Shared Object Inspection and Drift Operator Surface Model

## Purpose

This document defines the operator-facing inspection surface for cluster-shared definition objects, their UUIDs, and drift state.

## Canonical Rule

Operators shall be able to inspect cluster-shared definitions and their drift state directly without having to infer that state from failed movement or activation attempts.

## Required Inspection Fields

The operator surface shall preserve:

- shared-definition object class
- shared-definition UUID
- user-visible name
- canonical definition identity or digest
- per-node presence state
- per-node drift or blocker state

## Mobility Inspection Rule

For a dependent object, the operator surface shall be able to show:

- required shared-definition dependencies
- which dependency blocks movement or activation
- blocker class
- target node identity

## Non-Guarantees

This file does not require one specific CLI or SQL syntax. It requires the inspection data contract.
