# Security Definer, Invoker, and Emulated Schema Sandbox Model

## Status

Reconstructed required specification with partially implemented current enforcement.

## Purpose

This document defines SECURITY DEFINER versus SECURITY INVOKER behavior for views and adjacent executable schema objects, and it records the canonical sandbox rule for emulated engine schemas.

## Core Rule

Schema objects may execute with either:

- invoker security
- definer security

The effective rights of the executing object are not the same thing as the rights of the caller.

This rule is required so that:

- a user may invoke a view, procedure, function, or package
- while not holding direct rights to every underlying object referenced by that object

## View Security Modes

Current view security modes are:

- `INVOKER`
- `DEFINER`

Current view security options also include:

- security barrier
- check option
- local-vs-cascaded check option

## Current Stack Model

Current view execution uses a thread-local view security stack.

Each entered view context records:

- caller id
- effective user id
- view security options

Effective user resolution currently follows this rule:

- walk the nested view stack from innermost to outermost
- use the first definer context found
- otherwise use the caller

This gives nested view execution a deterministic privilege context.

## Security Barrier Rule

When a view has `security_barrier = true`, predicate pushdown through that view is forbidden.

This is a semantic and security boundary, not a planner hint.

## Check Option Rule

Views may specify check option behavior.

Current modes are:

- none
- local
- cascaded

The required semantic meaning is:

- local: validate only this view's visibility predicate
- cascaded: validate this view and underlying view predicates

## Current Implementation Boundary For View Security

Current code proves:

- view registration
- view option storage
- definer/invoker context stack
- security barrier tracking
- check-option mode tracking
- fail-closed denial for security-definer table-access and column-access when no
  integrated permission backend is attached
- fail-closed denial for `WITH CHECK OPTION` validation when no integrated row
  predicate backend is attached

So the accurate statement is:

- the model is canonical and required
- current isolated view-security enforcement no longer auto-allows missing
  backend paths
- the full catalog-integrated enforcement path is still only partially
  implemented today

## Emulated Engine Schema Sandbox Rule

This specification extends beyond plain view security.

For emulated engine schemas and adjacent executable schema objects, the canonical sandbox rule is:

- rights to execute or access the wrapper object do not automatically grant direct rights to the underlying objects it uses
- the wrapper object's defined security context governs what underlying objects may be reached
- the caller's direct privilege set is still relevant for the wrapper object itself, but not as a substitute for its execution context

This is the engine's required schema-sandbox model.

## Relation To Packages, Procedures, and Functions

The same sandbox principle applies to:

- views
- procedures
- functions
- packages

Where the parser and catalog expose SQL security mode, canonically the object must execute either in invoker or definer context and keep underlying-object rights sandboxed by that chosen mode.

## Partial-Implementation Boundary

Current parser and view-security code prove the security-mode model and stack machinery.

They do not fully prove end-to-end enforcement for all executable schema object
types in all execution paths from the code surfaces read in this pass. Where a
required backend is absent, the current isolated view-security path now denies
rather than silently allowing access.

Therefore this document is required specification with partial implementation drift.
