Status: reconstructed_required

# Domain Event and Custom Error Dependency Binding in Routines Model

## Purpose

This document defines how procedures, functions, and packages bind dependencies on domains, events, and custom error messages.

## Canonical Rule

Routine and package dependencies on domains, events, and custom error messages are UUID-bound shared-definition dependencies, not name-only references.

## Covered Routine Classes

The canonical dependent routine classes are:

- procedures
- functions
- packages
- package members

## Dependency Binding Rule

When a routine depends on a domain, event, or custom error message:

1. the dependency shall be resolved to the shared-definition UUID
2. the routine metadata shall preserve that UUID dependency
3. later remote execution or movement shall verify that dependency on the target node

## Execution Rule

At runtime, routine execution shall not reinterpret a same-named but different UUID definition as equivalent. UUID identity and canonical definition remain authoritative.

## Non-Guarantees

This file does not require all routine classes to use every shared-definition type. It defines the dependency-binding rule where they do.
