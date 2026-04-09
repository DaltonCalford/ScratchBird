Status: reconstructed_required

# Row UUID Alias and Shared Definition DDL Lowering Model

## Purpose

This document defines how parser libraries lower DDL that declares row-UUID alias columns or depends on cluster-shared definition objects.

## Canonical Rule

DDL lowering shall preserve UUID-backed identity intent explicitly. The parser may accept user-facing syntax, but the emitted canonical artifact shall carry:

- row UUID alias intent when declared
- shared-definition UUID dependencies for domains, events, and custom error messages

## Row UUID Alias Lowering

When a table definition declares a UUID identity column that is intended to be the logical row identity:

1. the parser shall classify that column as the row UUID alias
2. the parser shall emit canonical DDL metadata marking the alias binding
3. the parser shall preserve the rule that no second logical row UUID is to be generated for the same row

## Shared Definition Lowering

When DDL references a domain, event, or custom error message:

1. name resolution shall end in the shared-definition UUID
2. the emitted canonical artifact shall carry that UUID dependency
3. later execution or mobility checks shall use the UUID dependency rather than the user-visible name alone

## Non-Guarantees

This file does not require one exact SQL surface. It defines the canonical lowering obligations once the parser has accepted the DDL.
