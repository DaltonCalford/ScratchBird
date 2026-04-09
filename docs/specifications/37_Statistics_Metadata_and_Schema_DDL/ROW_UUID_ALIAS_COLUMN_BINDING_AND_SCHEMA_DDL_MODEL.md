Status: reconstructed_required

# Row UUID Alias Column Binding and Schema DDL Model

## Purpose

This document defines how schema and DDL bind a user-visible UUID identity column to the system row UUID.

## Canonical Rule

If a table uses a UUID identity column as the row identifier, schema definition shall mark that column as the row UUID alias so the system row UUID and the user-visible row UUID are the same logical identity.

## Binding Rule

The schema layer shall distinguish among:

- ordinary UUID-typed columns
- UUID identity columns that are not the row UUID alias
- the one canonical row UUID alias column for the table, if present

Only the canonical row UUID alias column binds to the system row UUID.

## DDL Rule

DDL creating or altering a table shall preserve:

- whether a row UUID alias column exists
- which column is the alias
- the rule that no second competing logical row UUID is generated for the same row

## Uniqueness Rule

At most one row UUID alias column may exist per table. If multiple UUID columns exist, only one may be the row identity alias.

## Mobility Rule

Because the system row UUID is used for cluster tracking, the alias binding shall be preserved across:

- table movement between nodes
- migration and restore
- dependent object reconstruction

## Non-Guarantees

This file does not require every table to expose the system row UUID to the user. It defines the schema rule where a UUID identity column is used as the row identifier.
