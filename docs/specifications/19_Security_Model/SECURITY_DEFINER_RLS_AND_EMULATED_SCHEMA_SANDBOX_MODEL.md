# Security Definer, RLS, and Emulated Schema Sandbox Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines the exact relationship between:
- `SECURITY INVOKER`
- `SECURITY DEFINER`
- `SECURITY BARRIER`
- row-level security policy application
- emulated-engine schema root sandboxing

## View and executable security modes

The canonical security modes are:
- `INVOKER`: execute with caller privileges
- `DEFINER`: execute with object-owner privileges

Current code-backed view metadata proves:
- `security_definer`
- `security_barrier`
- `check_option`
- `local_check_only`

These are canonical stored options, not parser-only decoration.

## Security barrier rule

`security_barrier` is an optimization fence.
It prevents unsafe predicate pushdown or equivalent cross-boundary rewrite that could disclose filtered data or weaken protected execution order.

A barrier surface therefore affects both:
- privilege context
- optimizer legality

## Effective-security algorithm

When an executable object is entered:
1. resolve caller identity and active role state
2. load the stored security mode and barrier options
3. if mode is `INVOKER`, retain caller identity for permission checks
4. if mode is `DEFINER`, substitute object-owner identity for protected object access checks
5. retain caller identity for audit attribution
6. preserve row-level, column-level, and domain-level checks inside the effective security context
7. if barrier is active, prohibit optimizer rewrites that would move protected predicates or protected data access outside the barrier

## Current security-stack runtime recovered from code

The current executable-object security runtime already proves:
- a thread-local view-security stack
- nested context push and pop
- effective-user resolution that walks innermost-to-outermost contexts
- `DEFINER` contexts override the effective user
- all-`INVOKER` stacks preserve caller identity
- active security barriers can be queried from the current stack

This stack behavior is current implementation authority and is not optional parser metadata.

## RLS interaction

RLS is not bypassed merely because an executable object is `SECURITY DEFINER`.

Rules:
- RLS remains part of the data-visibility model
- whether owner or superuser bypass applies is governed by RLS `forced` state and canonical RLS rules
- `SECURITY DEFINER` does not silently convert forced RLS into bypassable policy
- writes through executable objects must still obey `WITH CHECK`-style row policy validation

## Current enforcement drift recovered from code

Current code in the view-security runtime still shows deferred enforcement in some paths:
- table access checks inside the view-security manager are presently permissive placeholders
- column access checks inside the same manager are presently permissive placeholders
- `WITH CHECK OPTION` validation is still deferred in the current helper path

Canon rule:
- these permissive placeholders are not the intended model
- the stricter rules in this file remain authoritative
- any implementation relying on the placeholder behavior is non-conforming

## RLS administration

Current code-backed catalog surfaces prove:
- table `rls_enabled`
- table `rls_forced`
- policy create, lookup, list, and drop
- executor helper seams for `shouldEnforceRLS` and policy checking

Current parser/runtime drift also proves:
- catalog and executor RLS surfaces exist
- some parser-v3 SQL statement paths for policy DDL and `ALTER TABLE ... RLS` remain incomplete or gated in tests

Canon rule:
- catalog/runtime authority is canonical
- missing parser coverage is implementation drift, not absence of feature definition

## Emulated-schema sandbox rule

An emulated-engine session is anchored to a canonical schema root.

Rules:
- the canonical schema root is the name-resolution boundary
- the canonical schema root is also the security boundary
- legacy aliases must not silently become active session roots
- wrapped emulated objects execute inside the same invoker/definer and masking model as native objects
- compatibility syntax may vary by dialect, but the sandbox guarantee must not weaken

## MySQL-root code-backed anchor

Current code-backed MySQL emulation proves a canonical rooted schema form under the emulated namespace.
That anchor is sufficient to make schema-root sandboxing canonical across emulations.

## Audit consequence

At minimum the engine must be able to signal:
- barrier-protected access
- definer-boundary protected access
- RLS deny events
- RLS forced-mode application
- schema-root sandbox refusal or escape attempts

The audit trail must also preserve:
- caller identity
- effective definer identity when used
- barrier participation
- policy-driven denial versus masking outcome

## Non-guarantees

This file does not claim that every dialect parser currently exposes identical `SECURITY DEFINER`, RLS, or sandbox DDL syntax.
It defines the runtime and metadata contract those parsers must target.
