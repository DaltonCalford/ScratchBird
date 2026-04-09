# View Security Definer Invoker Barrier and Sandbox Model

## Purpose

Define the current view-security execution model and its role as the foundation for emulated-engine schema sandboxing.

## Execution Modes

Supported view security modes are:

- `INVOKER`
- `DEFINER`

## Effective User Resolution

View execution uses a thread-local security stack.

When entering a view:

1. the caller identity is captured
2. the view's security options are resolved
3. the effective user is set to:
   - the view owner for `DEFINER`
   - the caller for `INVOKER`
4. the context is pushed onto the security stack

When nested views exist, effective user resolution walks from the innermost context outward and returns the first `DEFINER` owner identity. If no `DEFINER` frame exists, the caller identity remains effective.

## Security Barrier

A view may be marked as a security barrier.

Security-barrier views prevent unsafe predicate pushdown across that boundary. Optimizer and rewrite stages shall treat the barrier as a hard planning fence.

## Check Option

The view-security model carries:

- no check option
- local check option
- cascaded check option

This governs write-path validation for rows passing through secured view layers.

## Registration and Lifecycle

View security options are registered, updated, and removed through the view-security manager. The registry is a runtime control surface, not an informal side table.

## Current Code-Backed Access Boundary

Current code proves:

- thread-local security stack
- `DEFINER` versus `INVOKER` context selection
- security-barrier tracking
- check-option mode tracking

Current table and column access checks inside this subsystem remain partially stubbed. The canonical requirement is still fail-closed security enforcement once fully wired to the permission system.

## Schema Sandboxing Relationship

This view-security model is the current code-backed base for broader schema sandboxing, including the rule that a user may have rights to execute or select through a secured object without directly inheriting rights to the underlying base objects.

That broader sandboxing rule applies to:

- views
- procedures
- functions
- packages

where separately specified by canonical security surfaces.
