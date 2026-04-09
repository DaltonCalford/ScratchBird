# Remote Schema Mapping and Passthrough Policy Schema

Status: current_authority

## Purpose

This file defines the exact persisted schema for remote schema mapping and remote passthrough policy objects.

## Remote schema mapping row

A remote schema mapping row currently persists:
- schema mapping UUID
- remote connector UUID
- mapping name
- local schema UUID
- mapping mode
- remote schema pattern
- include object kinds
- optional exclude object patterns
- optional rename rule JSON
- optional last snapshot UUID
- created time
- last modified time
- validity flag

## Validation rules

The mapping row is invalid unless:
- mapping UUID is non-zero
- remote connector UUID is non-zero
- local schema UUID is non-zero
- mapping name is present
- remote schema pattern is present
- include object kinds is present
- mapping mode is valid

If a last snapshot UUID is present, it must reference a valid remote metadata snapshot for the same connector.

## Passthrough policy row

A remote passthrough policy row currently persists:
- policy UUID
- remote connector UUID
- `allow_query`
- `allow_dml`
- `allow_ddl`
- `allow_admin`
- `allow_procedural`
- `allow_join_local_txn`
- max rows
- max bytes
- timeout ms
- optional required capabilities string
- audit level
- created time
- last modified time
- validity flag

## Validation rules

A passthrough policy row is invalid unless:
- policy UUID is non-zero
- connector UUID is non-zero
- audit level is present
- at least one allow flag is true

## Non-negotiable rule

These rows are policy and mapping truth for remote connector behavior. Proxy or migration code must not invent capabilities that the catalog row does not allow.
