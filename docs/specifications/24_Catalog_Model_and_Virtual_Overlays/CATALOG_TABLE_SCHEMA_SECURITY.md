# Catalog Table Schema: Security

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the canonical security catalog families and the publication facts they own.

## Canonical security catalog families

Current code-backed catalog families include at least:
- users
- roles
- groups
- role and group membership edges
- object permission rows
- column permission rows
- row-level security policy rows
- table RLS enable and force state
- domain security catalog rows
- auth provider rows
- auth policy rows
- MFA policy rows
- MFA enrollment rows
- MFA recovery-code rows
- alert-target secret-reference rows
- security-policy epoch rows

## Ownership rules

These rows own:
- security principal identity
- grant and membership truth
- policy truth
- auth-provider and auth-policy truth
- MFA policy and enrollment truth
- secret-reference metadata truth
- security publication epoch truth

They do not by themselves override:
- transaction visibility rules
- MGA publication order
- parser helper visibility boundaries
- runtime permission evaluation logic in section `19`

## Security publication anchors

Current code-backed metadata proves two security publication anchors:
- global security-policy epoch
- per-table policy epoch

Current code-backed consumers include:
- connection-context security epoch state
- permission cache entries keyed by global and table policy epoch
- session metadata carrying security and table policy epochs

Canon rule:
- grant, revoke, role membership, group membership, auth policy, MFA policy, and RLS metadata changes publish through committed security catalog state and epoch advancement
- caches that depend on security decisions must validate against the relevant security epoch anchors

## Catalog row families that matter to parsers and caches

Parsers and metadata mirrors must treat these security row families as canonical durable sources:
- role and group identity rows for name resolution and diagnostics
- permission rows for discoverability-safe metadata views
- policy rows for RLS metadata and show-grants style reporting
- auth and MFA rows for administrative inspection surfaces
- domain security rows for masking and policy metadata

## Boundary

This file defines catalog row ownership and epoch anchors.
Broader runtime security guarantees remain owned by section `19`.
