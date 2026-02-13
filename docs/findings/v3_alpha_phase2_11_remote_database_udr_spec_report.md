# archive/alpha_phase_2/11-Remote-Database-UDR-Specification.md - Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11-Remote-Database-UDR-Specification.md`

Status notes:
- The document explicitly states it is **Non-Authoritative** (design document).

Implementation notes:
- Remote UDR connectors implemented here for PostgreSQL/MySQL/Firebird/ScratchBird in `src/udr/` (e.g., `postgresql_udr.cpp`, `mysql_udr.cpp`, `firebird_udr.cpp`, `scratchbird_udr.cpp`).
- Connection pool support for UDR connectors exists in `src/udr/connection_pool.cpp`.
- No MSSQL, ODBC, or JDBC connector implementations found in this repo.

Verification:
- Partial code-level verification (connector presence only). No protocol completeness or SQL pushdown verification performed.
