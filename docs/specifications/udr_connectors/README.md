# UDR Connectors - Specification Set

This directory defines the UDR connector specifications required to plug external
resources into the ScratchBird engine without vendor-provided drivers. Each
connector is a separate UDR plugin module with a signed manifest and a clearly
scoped capability set.

## Connector UDRs

- local_files_udr: Local CSV/JSON file access (no network)
- local_scripts_udr: Local script execution for data access (no network)
- postgresql_udr: PostgreSQL remote access using the native wire protocol
- mysql_udr: MySQL/MariaDB remote access using the native wire protocol
- firebird_udr: Firebird remote access using the native wire protocol

## Shared Baseline

- UDR_CONNECTOR_BASELINE.md
  - Shared requirements: signing, permissions, admin/SQL commands, pass-through
    DDL/DML/PSQL, migration window behavior, and security constraints.

## References

- ScratchBird/docs/specifications/10-UDR-System-Specification.md
- ScratchBird/docs/specifications/09_DDL_FOREIGN_DATA.md
- ScratchBird/docs/specifications/remote_database_udr/* (core logic, pooling,
  query execution, schema introspection, migration workflows)
- ScratchBird/docs/specifications/wire_protocols/postgresql_wire_protocol.md
- ScratchBird/docs/specifications/wire_protocols/mysql_wire_protocol.md
- ScratchBird/docs/specifications/wire_protocols/firebird_wire_protocol.md
