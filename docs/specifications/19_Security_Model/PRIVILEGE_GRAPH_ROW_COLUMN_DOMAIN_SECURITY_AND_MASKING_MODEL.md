# Privilege Graph, Row/Column/Domain Security, and Masking Model

## Status

Reconstructed required specification with partially implemented current enforcement.

## Purpose

This document defines ScratchBird's privilege graph, column and row policy model, domain-level security catalog model, and data masking behavior.

## Grantee Graph

The privilege graph supports these grantee classes:

- `USER`
- `ROLE`
- `GROUP`
- `PUBLIC`

The security system must compute:

- transitive effective roles for a user
- transitive effective groups for a user

This means shared-user and shared-rights behavior is graph-based rather than flat.

## Object Privilege Model

Current object and permission surfaces include at least:

- execute
- select
- insert
- update
- delete
- usage

Permissions may be granted with or without grant option.

Default privileges are schema-scoped and object-type-scoped, and may be applied when new objects are created.

## Column-Level Security

Current catalog authority includes explicit column permission records with:

- table id
- column name
- grantee id
- grantee type
- privilege bitmask
- grant option
- grantor id

Current catalog manager operations include:

- grant column permission
- revoke column permission
- test column permission
- enumerate accessible columns for a user and privilege
- list column permissions for a table

This means column security is a first-class canonical model, not merely a derived interpretation of table privileges.

## Row-Level Security Policy Model

Current catalog authority includes row-level policies with:

- policy id
- table id
- policy name
- policy type
- role list
- `USING` expression
- `WITH CHECK` expression
- enabled state

Current policy types are:

- `ALL`
- `SELECT`
- `INSERT`
- `UPDATE`
- `DELETE`

Current catalog manager operations include:

- create policy
- drop policy
- alter policy
- fetch one policy
- list table policies by type
- list applicable policies for a user
- enable or force table RLS

## Canonical RLS Requirement

Row-level security is required specification.

Any table with active row-level policy must have visibility and modification governed by:

- operation type
- current effective roles
- `USING` expression for visibility
- `WITH CHECK` expression for inserts and updates

## Current Implementation Boundary For RLS

The catalog, parser, and policy-management surfaces exist.

Full end-to-end enforcement across every execution path is not fully proven by the narrow code surfaces read in this recovery pass. Therefore:

- the specification is authoritative
- current implementation parity may still be incomplete

That gap is implementation drift, not a weakening of the model.

## Domain Security Catalog Model

Current canonical domain catalog surfaces include:

- domain constraints
- domain security
- domain validation
- domain integrity

Current domain security catalog rows include:

- `domain_id`
- security kind
- `security_expr_sblr`

Current domain security kinds include:

- `MASK_FUNCTION`
- `AUDIT_ACCESS`
- `REQUIRE_PERMISSION`
- `ENCRYPTION`

This means domain-level security is not a vague metadata note. It is a real catalog family with SBLR-backed expressions.

## Parser-Level Domain Security Surface

Current parser-v3 code accepts `WITH SECURITY (...)` options on domains, including:

- `MASKING`
- `MASK_PATTERN`
- `ENCRYPTION`
- `AUDIT_ACCESS`
- `REQUIRE_PRIVILEGE`

These parser surfaces are current code truth and must remain in canon.

## Data Masking Primitive

Current engine masking behavior supports:

- no masking
- partial masking
- full masking

The masking primitive takes:

- input value
- masking config
- privilege possession flag

If the caller has the required privilege or masking type is `NONE`, the original value is returned.

If masking applies:

- `FULL` masks all characters
- `PARTIAL` uses a pattern vocabulary

Current partial pattern tokens are:

- `#` retain source character
- `X` consume source character and emit mask token
- any other character is treated as literal punctuation or delimiter

Masking operates on valid UTF-8 by character, and falls back to byte-wise masking for invalid UTF-8.

## Required Security Semantics

The canonical security model requires:

- table/object privilege checks
- column privilege checks
- row policy checks
- domain security checks
- masking evaluation

These layers are cumulative, not mutually exclusive.

## Partial-Implementation Boundary

Current code proves:

- privilege catalog surfaces
- column permission APIs
- row-policy catalog APIs
- domain-security catalog APIs
- parser support for domain security attributes
- masking primitive implementation

Current code read in this pass does not prove that every query and DML path already enforces every one of those layers exhaustively. Therefore this document is a reconstructed required specification with partial current implementation.
