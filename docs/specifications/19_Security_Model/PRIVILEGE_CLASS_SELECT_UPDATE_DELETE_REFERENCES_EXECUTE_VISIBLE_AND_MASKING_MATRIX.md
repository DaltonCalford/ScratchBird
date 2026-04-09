Status: reconstructed_required

# Privilege Class Select Update Delete References Execute Visible and Masking Matrix

## Purpose

This document defines the canonical privilege-class matrix for object access, visibility, mutation, execution, and masking-related behavior.

## Canonical Rule

Privilege classes are distinct. Granting one privilege class does not imply another unless canon explicitly says so.

## Canonical Privilege Classes

The canonical privilege classes are:

- `SELECT`
- `INSERT`
- `UPDATE`
- `DELETE`
- `REFERENCES`
- `EXECUTE`
- `VISIBLE`
- domain use
- masking bypass where explicitly admitted

## Class Semantics

`SELECT` allows data read subject to row, column, domain, and masking policy.

`INSERT` allows creation of new rows or objects subject to domain and object policy.

`UPDATE` allows mutation of admitted target columns and rows subject to row, column, and domain policy.

`DELETE` allows row retirement or delete-marker publication subject to row eligibility and transaction rules.

`REFERENCES` allows use as a referenced target where the schema and integrity model requires it.

`EXECUTE` allows invocation of procedures, functions, packages, and other callable surfaces.

`VISIBLE` allows metadata or object visibility without implying data access or mutation rights.

Domain use allows use of a protected domain subject to domain-level rules.

Masking bypass, where policy admits it, is separate from `SELECT`.

## Separation Rules

The engine shall not infer:

- `SELECT` from `VISIBLE`
- `EXECUTE` from `SELECT`
- direct underlying-object rights from wrapper-object `EXECUTE`
- masking bypass from ordinary `SELECT`

## Interaction With Policy

Every privilege class remains subordinate to:

- row-level policy
- column-level policy
- domain-level policy
- masking policy
- security-definer and sandbox boundaries

## Non-Guarantees

This file does not define one grant syntax. It defines the canonical privilege classes and their separation.
