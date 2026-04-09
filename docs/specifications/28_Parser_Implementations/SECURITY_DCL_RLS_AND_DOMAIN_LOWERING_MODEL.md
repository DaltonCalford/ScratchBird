# Security, DCL, RLS, and Domain Lowering Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines the parser contract for lowering:
- user, role, and group statements
- grant and revoke statements
- set-role and session-authorization statements
- row-level security policy statements
- table RLS state-change statements
- domain security clauses

## Parser-local lowering rule

Every parser lowers its own security syntax locally.

Rules:
- no parser may delegate security lowering to another parser
- no parser dependency may be introduced between dialect packages
- optional parser packages may be removed without breaking security lowering in any other parser package
- dialect parity is achieved by converging on canonical AST and `SBLR`, not by shared parser execution

## Current V3 AST proof

Current V3 AST already models at least:
- `CreateUserStmt`
- `AlterUserStmt`
- `CreateRoleStmt`
- `CreateGroupStmt`
- `CreatePolicyStmt`
- `AlterPolicyStmt`
- `DropPolicyStmt`
- `GrantStmt`
- `RevokeStmt`
- `SetStmt` variants for `ROLE` and `SESSION_AUTHORIZATION`
- alter-table actions for `ENABLE_RLS`, `DISABLE_RLS`, `FORCE_RLS`, and `NO_FORCE_RLS`
- domain security option capture in `CreateDomainStmt`

## Current parse proof

Current V3 parse code already proves:
- domain `WITH SECURITY (...)` option parsing for `MASKING`, `MASK_PATTERN`, `ENCRYPTION`, `AUDIT_ACCESS`, and `REQUIRE PRIVILEGE`
- `CREATE POLICY`, `ALTER POLICY`, and `DROP POLICY`
- `ALTER TABLE ... ROW LEVEL SECURITY` action parsing
- `SET ROLE`
- `SET SESSION AUTHORIZATION`
- `GRANT` and `REVOKE` privilege parsing

## Current emit proof

Current V3 emitter already lowers at least to:
- `SBLR3_CREATE_POLICY`
- `SBLR3_ALTER_POLICY`
- `SBLR3_DROP_POLICY`
- `SBLR3_GRANT`
- `SBLR3_REVOKE`
- `SBLR3_SHOW_GRANTS`
- alter-table payload action codes for RLS state changes

## Canonical lowering rules

### User, role, and group statements
- principal names are user-significant and must be preserved through canonical AST and `SBLR`
- parser may normalize quoting and case according to dialect rules before canonicalization
- parser must not invent or collapse principal identities across dialects

### Grant and revoke
- privilege families must be preserved as explicit canonical privilege sets
- object type must be preserved
- object path must be preserved until UUID binding replaces durable object names
- grantees and `PUBLIC` semantics must be preserved exactly
- `WITH GRANT OPTION` and `GRANT OPTION FOR` must remain explicit flags

### Policy DDL and table RLS actions
- policy name, target table path, policy type, permissive or restrictive state, target roles, `USING`, and `WITH CHECK` must remain explicit canonical fields
- table RLS action kind must survive lowering as explicit action codes or equivalent canonical enum values
- parser must not silently drop RLS clauses that the runtime depends on for security semantics

### Domain security clauses
- domain security options must be lowered as explicit canonical fields, not as opaque original SQL text only
- unsupported domain security options must be rejected explicitly
- parser must not erase masking, encryption, audit, or required-privilege meaning during lowering

## Dialect-local variance

Dialect-local syntax may differ, but the lowered canonical meaning must converge.

Current Firebird parser proof shows:
- grant and revoke are parser-local in the Firebird package
- some surfaces such as `RECREATE ROLE`, `RECREATE USER`, and `RECREATE MAPPING` remain explicit unsupported parser-time refusals

That is valid so long as:
- the refusal is explicit
- the unsupported surface is reflected in the dialect capability profile
- no silent fallback to another parser occurs

## Current drift rule

Where parse or emit code exists but integration coverage still marks the statement family as pending, canon shall record that as parser conformance drift.
It is not a reason to weaken the canonical lowering model.
