Status: reconstructed_required

# Shared Definition Object Dependency Mobility Certification Matrix

## Purpose

This document defines the certification matrix required for shared-definition dependencies and safe node mobility.

## Required Matrix Dimensions

The certification matrix shall preserve:

- shared-definition object class
- shared-definition UUID
- source node identity
- target node identity
- dependent object class
- dependency verification result
- mobility blocker state
- movement or activation outcome

## Required Rows

The matrix shall include rows proving:

- successful movement with aligned shared definition
- refusal due to missing shared-definition object
- refusal due to UUID mismatch
- refusal due to definition mismatch
- refusal due to unresolved cluster drift

## Failure Criteria

Certification fails when:

- a dependent object moves successfully with unresolved shared-definition blocker state
- UUID or definition mismatch is not detectable in the certification evidence
- parser or DDL artifacts cannot be tied back to the shared-definition UUID dependency

## Non-Guarantees

This file does not require all deployments to support mobility. It defines the certification target where mobility is admitted.
