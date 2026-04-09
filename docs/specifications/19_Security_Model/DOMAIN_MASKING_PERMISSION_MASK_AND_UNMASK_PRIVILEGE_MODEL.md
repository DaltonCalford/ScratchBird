# Domain Masking, Permission Mask, and Unmask Privilege Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines:
- domain masking application
- permission-mask evaluation
- named unmask privilege evaluation
- precedence rules
- current runtime type limits

## Current runtime masking rule

`applyMasking()` currently follows this algorithm:
1. load the domain definition
2. if the value is `NULL`, return it unchanged
3. if the domain masking type is `NONE`, return the value unchanged
4. evaluate whether the caller holds unmask privilege
5. if privilege evaluation fails, fail the operation
6. for supported string-like types, call canonical masking logic
7. return masked or unmasked text according to privilege result

Current runtime masking is proven for string-like domain values:
- `VARCHAR`
- `TEXT`
- `CHAR`

Masking unsupported value types must fail closed as invalid arguments unless canon is later expanded.

## Permission-mask evaluation

When `permission_mask != 0`, it is authoritative.

Current code-backed evaluation order scans the canonical privilege family:
- `SELECT`
- `INSERT`
- `UPDATE`
- `DELETE`
- `TRUNCATE`
- `REFERENCES`
- `TRIGGER`
- `CREATE`
- `USAGE`
- `SEQUENCE_USAGE`
- `SEQUENCE_UPDATE`
- `EXECUTE`
- `CONNECT`
- `TEMPORARY`
- `COPY_FILE`

Algorithm:
1. iterate the canonical privilege family in engine order
2. skip privileges whose bit is not present in the mask
3. ask the catalog permission model whether the caller has that privilege on the domain object
4. if any required privilege is granted, unmask privilege succeeds
5. if none are granted, unmask privilege fails

Current code uses `any-of` semantics, not `all-of` semantics.

## Named required-privilege evaluation

When `permission_mask = 0`, the runtime evaluates `required_privilege_for_unmasked`.

Algorithm:
1. if empty, no extra unmask privilege is required
2. normalize the privilege name
3. if normalized value is `NONE`, no extra unmask privilege is required
4. otherwise parse the privilege name into a canonical privilege enum
5. reject invalid privilege names as invalid arguments
6. evaluate that privilege against the domain object in the catalog permission model

## Precedence rule

Precedence is fixed:
1. `permission_mask` if non-zero
2. named required privilege otherwise

Future implementations may not silently combine or reorder these rules.

## Audit consequence

When `audit_enabled` is set on the domain security payload, the engine shall treat at least the following as auditable security events:
- masked disclosure
- unmasked disclosure
- invalid masking privilege specification
- malformed domain-security payload
- failed masking application

## Reconstructed extension rule

Higher-level column or dialect-specific masking surfaces shall either:
- resolve into this canonical domain-masking model
- or explicitly define why they remain separate

This prevents duplicate masking semantics from diverging by dialect or parser.
