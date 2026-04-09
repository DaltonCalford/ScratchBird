Status: reconstructed_required

# Row UUID and Shared Definition Continuity in Export Restore and Mobility Model

## Purpose

This document defines how row UUIDs and shared-definition UUID dependencies remain stable across export, restore, migration, and node mobility workflows.

## Canonical Rule

Export, restore, migration, and node-mobility workflows shall preserve:

- logical row UUID continuity
- row UUID alias-column identity where present
- shared-definition UUID dependencies for objects and rows that rely on them

These workflows shall not reassign or reinterpret those identities implicitly.

## Export Rule

When exported data or metadata carries row identity or shared-definition dependency state:

- logical row UUID remains the canonical row identity
- row UUID alias-column state remains bound to the same logical identity
- shared-definition dependencies remain UUID-bound rather than name-only

## Restore Rule

Restore shall re-establish:

- the same logical row UUIDs for restored logical rows
- the same row UUID alias-column binding where defined
- the same shared-definition UUID dependencies for restored objects

## Mobility Rule

Object or row mobility across nodes is permitted only when restored or moved state can preserve those same identities without ambiguity.

## Non-Guarantees

This file does not require every export format to expose all UUID fields directly to end users. It requires continuity of those identities through canonical workflows.
