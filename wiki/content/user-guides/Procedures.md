# Procedures

**Last Updated:** 2026-01-28

Procedures are stored routines that execute with database privileges and may
perform multiple statements, including DML and transaction control. They are
invoked using CALL and can use IN, OUT, and INOUT parameters.

## CREATE PROCEDURE

```
CREATE [OR REPLACE] PROCEDURE procedure_name (
    [[parameter_mode] parameter_name data_type [DEFAULT value] [, ...]]
)
    [LANGUAGE language_name]
    [SECURITY { DEFINER | INVOKER }]
    [SET configuration_parameter = value]
AS $$
    DECLARE
        -- variables
    BEGIN
        -- statements
    END;
$$;
```

Parameter modes:
- IN (default): read-only
- OUT: output-only
- INOUT: read/write

## CALL

```
CALL procedure_name(arg1, arg2, ...);
```

Example:

```
CREATE PROCEDURE add_employee(
    first_name_in VARCHAR(50),
    last_name_in VARCHAR(50),
    hire_date_in DATE
)
AS $$
BEGIN
    INSERT INTO employees (first_name, last_name, hire_date)
    VALUES (first_name_in, last_name_in, hire_date_in);
END;
$$;

CALL add_employee('Jane', 'Doe', '2025-09-15');
```

## ALTER PROCEDURE

```
ALTER PROCEDURE procedure_name ( [parameter_types] )
    { RENAME TO new_name
    | OWNER TO new_owner
    | SET SCHEMA new_schema
    | SECURITY { DEFINER | INVOKER }
    | SET configuration_parameter = value };
```

## DROP PROCEDURE

```
DROP PROCEDURE [IF EXISTS] procedure_name ([parameter_types]) [CASCADE | RESTRICT];
```

## Security

- SECURITY DEFINER executes with the owner's privileges (default).
- SECURITY INVOKER executes with the caller's privileges.

## System procedures

ScratchBird installs internal procedures for remote database connectors:

- sys.remote_exec(server_name, sql_text, params_json, options_json)
- sys.remote_call(server_name, routine_name, params_json, options_json)

These are engine-owned and cannot be redefined.

## References

- `docs/specifications/ddl/DDL_PROCEDURES.md`
- `docs/specifications/parser/05_PSQL_PROCEDURAL_LANGUAGE.md`
- `docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
