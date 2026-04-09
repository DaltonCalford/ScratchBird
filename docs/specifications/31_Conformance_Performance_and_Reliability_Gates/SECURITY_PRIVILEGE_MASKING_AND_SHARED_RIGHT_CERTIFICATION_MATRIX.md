Status: reconstructed_required

# Security Privilege Masking and Shared Right Certification Matrix

## Purpose

This document defines the certification matrix required to prove the recovered security privilege, masking, and shared-right model.

## Required Matrix Dimensions

The certification matrix shall preserve:

- principal identity class
- privilege class under test
- wrapper or direct-object path
- row-policy state
- column-policy state
- domain-policy state
- masking state
- shared-right bundle state
- expected outcome
- actual outcome

## Required Rows

The matrix shall include rows proving:

- `VISIBLE` without `SELECT`
- `EXECUTE` through a mediated wrapper with direct-object deny preserved
- masking with no masking bypass
- masking bypass where explicitly granted
- shared-right bundle activation for wrapper objects
- shared-right bundle not granting direct underlying-object rights

## Failure Criteria

Certification fails when:

- privilege classes collapse into one another without explicit canon
- masking bypass is inferred from plain `SELECT`
- direct-object deny is lost through wrapper execution
- shared-right propagation cannot be explained by the catalog-backed model

## Non-Guarantees

This file does not require every deployment to exercise every row today. It defines the certification target for the recovered security model.
