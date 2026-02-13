# Appendix: Firebird 5 Privilege Enforcement Map (Authoritative)

This appendix enumerates the privilege checks for DDL and DML. All checks are enforced through `SCL_check_*` / `SCL_check_access` in `scl.epp`. Failures MUST use the exact error semantics defined below.

## Privilege Flags (Authoritative)
These are the mask values used by security checks.

- `SCL_select = 1`
- `SCL_drop = 2`
- `SCL_control = 4`
- `SCL_alter = 16`
- `SCL_insert = 64`
- `SCL_delete = 128`
- `SCL_update = 256`
- `SCL_execute = 1024`
- `SCL_usage = 2048`
- `SCL_create = 4096`
- `SCL_references = 8`

Privilege name strings used in errors:
- `SCL_select -> "SELECT"`
- `SCL_insert -> "INSERT"`
- `SCL_update -> "UPDATE"`
- `SCL_delete -> "DELETE"`
- `SCL_references -> "REFERENCES"`
- `SCL_execute -> "EXECUTE"`
- `SCL_usage -> "USAGE"`
- `SCL_create -> "CREATE"`
- `SCL_alter -> "ALTER"`
- `SCL_drop -> "DROP"`
- `SCL_control -> "CONTROL"`

## Error on Failure (Authoritative)
All privilege failures raise `isc_no_priv` with the following argument sequence:

- `isc_no_priv`
- privilege string (from list above)
- object type name (from `getDdlObjectName(type)`)
- full object name (qualified; if a column is involved it is `relation.column`)
- optional: `isc_effective_user` + effective user name (invoker)

If an ACL is corrupt or unrecognized, the error is:
- `isc_no_priv`, `"(ACL unrecognized)"`, `"security_class"`, `<security class name>`, optional `isc_effective_user`.

## DDL Enforcement (Authoritative)
DDL enforcement uses the object type associated with the statement and the corresponding `SCL_check_*` call. The required privilege is the `SCL_*` mask passed to the check function.

### CREATE Statements
`CREATE` requires `SCL_create` via `SCL_check_create_access` for the object type and schema.

Object type mapping:
- `CREATE TABLE` -> `obj_relations`
- `CREATE VIEW` -> `obj_views`
- `CREATE PROCEDURE` -> `obj_procedures`
- `CREATE FUNCTION` -> `obj_functions`
- `CREATE PACKAGE` -> `obj_packages`
- `CREATE SEQUENCE` / `CREATE GENERATOR` -> `obj_generators`
- `CREATE DOMAIN` -> `obj_domains`
- `CREATE EXCEPTION` -> `obj_exceptions`
- `CREATE ROLE` -> `obj_roles`
- `CREATE FILTER` -> `obj_filters`
- `CREATE COLLATION` -> `obj_collations`
- `CREATE SCHEMA` -> `obj_schemas`

Failure error: `isc_no_priv` with privilege string `"CREATE"` and the target object type/name.

### ALTER Statements
`ALTER` requires `SCL_alter` on the target object type.

Object type mapping:
- `ALTER DATABASE` -> `SCL_check_database(..., SCL_alter)`
- `ALTER SCHEMA` -> `SCL_check_schema(..., SCL_alter)`
- `ALTER DOMAIN` -> `SCL_check_domain(..., SCL_alter)`
- `ALTER TABLE` -> `SCL_check_relation(..., SCL_alter)`
- `ALTER VIEW` -> `SCL_check_view(..., SCL_alter)`
- `ALTER PROCEDURE` -> `SCL_check_procedure(..., SCL_alter)`
- `ALTER FUNCTION` -> `SCL_check_function(..., SCL_alter)`
- `ALTER PACKAGE` -> `SCL_check_package(..., SCL_alter)`
- `ALTER EXCEPTION` -> `SCL_check_exception(..., SCL_alter)`
- `ALTER SEQUENCE` / `ALTER GENERATOR` -> `SCL_check_generator(..., SCL_alter)`
- `ALTER ROLE` -> `SCL_check_role(..., SCL_alter)`
- `ALTER FILTER` -> `SCL_check_filter(..., SCL_alter)`
- `ALTER CHARACTER SET` -> `SCL_check_charset(..., SCL_alter)`
- `ALTER COLLATION` -> `SCL_check_collation(..., SCL_alter)`

Failure error: `isc_no_priv` with privilege string `"ALTER"` and the target object type/name.

### DROP Statements
`DROP` requires `SCL_drop` on the target object type.

Object type mapping:
- `DROP TABLE` -> `SCL_check_relation(..., SCL_drop)`
- `DROP VIEW` -> `SCL_check_view(..., SCL_drop)`
- `DROP PROCEDURE` -> `SCL_check_procedure(..., SCL_drop)`
- `DROP FUNCTION` -> `SCL_check_function(..., SCL_drop)`
- `DROP PACKAGE` -> `SCL_check_package(..., SCL_drop)`
- `DROP EXCEPTION` -> `SCL_check_exception(..., SCL_drop)`
- `DROP SEQUENCE` / `DROP GENERATOR` -> `SCL_check_generator(..., SCL_drop)`
- `DROP DOMAIN` -> `SCL_check_domain(..., SCL_drop)`
- `DROP ROLE` -> `SCL_check_role(..., SCL_drop)`
- `DROP FILTER` -> `SCL_check_filter(..., SCL_drop)`
- `DROP COLLATION` -> `SCL_check_collation(..., SCL_drop)`
- `DROP SCHEMA` -> `SCL_check_schema(..., SCL_drop)`

Failure error: `isc_no_priv` with privilege string `"DROP"` and the target object type/name.

### CONTROL Statements
`CONTROL` requires `SCL_control` on the target object type. This is enforced through `SCL_check_*` with `SCL_control` as the mask (only for statements that explicitly request CONTROL).

Failure error: `isc_no_priv` with privilege string `"CONTROL"` and the target object type/name.

## DML Enforcement (Authoritative)
DML uses `SCL_check_access` against relation/view security classes and routine security classes.

### Relations and Views
- `SELECT` -> `SCL_select`
- `INSERT` -> `SCL_insert`
- `UPDATE` -> `SCL_update`
- `DELETE` -> `SCL_delete`
- `REFERENCES` (FK creation or explicit REFERENCES check) -> `SCL_references`

Failure error: `isc_no_priv` with the action name (`SELECT`, `INSERT`, `UPDATE`, `DELETE`, `REFERENCES`), object type `TABLE` or `VIEW` (from `getDdlObjectName`), and the fully qualified object name. If the check is column‑level, the name is `relation.column`.

### Procedures, Functions, Packages
- `EXECUTE PROCEDURE` -> `SCL_execute` on the procedure
- `SELECT` from a selectable stored procedure -> `SCL_execute`
- `EXECUTE FUNCTION` (UDF or stored) -> `SCL_execute`
- `EXECUTE` on package routines -> `SCL_execute`

Failure error: `isc_no_priv` with action name `EXECUTE` and the routine name.

### Other Objects (USAGE)
`USAGE` privilege applies to objects with usage semantics:
- Domains
- Character sets
- Collations
- Sequences / generators
- Schemas
- Roles

Failure error: `isc_no_priv` with action name `USAGE` and the target object name.

## SYSTEM Privileges and ANY Privileges (Authoritative)
`SCL_check_access` first tests for system privileges and "ANY" DDL privileges:
- If the user has the required system privilege mask, access is granted without object ACL checks.
- For DDL objects, the global DDL privilege class for the schema is checked via `SCL_get_object_mask`.

This behavior is mandatory and must match Firebird evaluation order.
