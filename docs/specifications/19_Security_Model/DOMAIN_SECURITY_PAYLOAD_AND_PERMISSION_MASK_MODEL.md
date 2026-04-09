# Domain Security Payload and Permission Mask Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the exact persisted domain-security payload, default omission rules, versioning, and the meaning of the domain permission mask.

## Current code-backed payload fields

`DomainSecurity` currently carries:
- `masking_config`
- `required_privilege_for_unmasked`
- `encryption_enabled`
- `encryption_algorithm`
- `encryption_key_id`
- `audit_enabled`
- `permission_mask`

The serialized payload stores, in canonical order:
1. payload version
2. masking type
3. encryption-enabled flag
4. audit-enabled flag
5. encryption algorithm
6. permission mask
7. masking pattern
8. full-mask character
9. required privilege for unmasked access
10. encryption key id

## Default omission rule

A domain-security payload may be omitted only when all security fields are at canonical defaults:
- masking type `NONE`
- empty masking pattern
- full-mask character `*`
- empty required unmask privilege
- encryption disabled
- encryption algorithm `NONE`
- zero encryption key id
- audit disabled
- permission mask `0`

Any other state requires explicit payload persistence.

## Versioning and compatibility

Current runtime supports:
- legacy payload version `1`
- current `DOMAIN_SECURITY_VERSION`

Deserialization must fail closed on:
- truncated payloads
- malformed fields
- unknown versions

Malformed domain-security payloads are corruption, not soft warnings.

## Permission-mask semantics

The permission mask is a domain-local authorization gate.
It does not replace:
- object permissions
n- column permissions
- row policy checks
- masking bypass evaluation

A domain-bound disclosure decision may therefore require multiple layers to succeed.

## Security catalog companion objects

Current code-backed catalog structures also prove a domain-security catalog family carrying:
- `security_id`
- `domain_id`
- `security_kind`
- `security_expr_sblr`
- validity and timestamp metadata

Commercial-grade canon treats these catalog entries as the extensibility lane for richer domain security expressions without weakening the base payload contract.

## Relationship to encryption and audit

A domain may simultaneously require:
- masking
- unmask privilege
- encryption-at-rest intent
- audit signaling

These controls compose. None of them may erase the others.

## Canonical failure rule

If domain security cannot be parsed, loaded, or evaluated safely, the engine must refuse the operation. It must not silently expose unmasked or unaudited values.
