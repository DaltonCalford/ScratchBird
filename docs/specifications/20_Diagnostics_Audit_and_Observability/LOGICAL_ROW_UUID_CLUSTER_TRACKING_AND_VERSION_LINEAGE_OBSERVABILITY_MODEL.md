Status: reconstructed_required

# Logical Row UUID Cluster Tracking and Version Lineage Observability Model

## Purpose

This document defines the operator-visible observability contract for logical row UUID tracking and MGA version lineage.

## Canonical Rule

Operators shall be able to inspect logical row tracking state without conflating:

- logical row identity
- physical record location
- MGA version lineage

## Required Observability Fields

The observability surface shall preserve:

- logical row UUID
- current node or placement identity where cluster tracking applies
- current lineage state
- current visible version pointer or equivalent lineage marker
- alias-column exposure state if present

## Lineage Rule

Version lineage observability shall explain that:

- updates create new versions
- the logical row UUID remains stable across versions
- rollback or delete state changes lineage state but not the logical row UUID identity

## Non-Guarantees

This file does not require row-level cluster inspection to be enabled in every operator surface. It requires a canonical observability contract where such inspection is admitted.
