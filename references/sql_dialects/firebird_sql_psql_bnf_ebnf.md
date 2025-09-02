# Firebird SQL/PSQL BNF/EBNF Grammar

## Overview
This document provides a comprehensive BNF/EBNF grammar specification for Firebird SQL and PSQL (Procedural SQL). Firebird is an open-source SQL relational database management system that extends the SQL standard with numerous features, particularly strong support for stored procedures, triggers, and a rich procedural language.

## Notation
- `::=` defines a production rule
- `|` indicates alternatives
- `[ ]` indicates optional elements
- `{ }` indicates zero or more repetitions
- `( )` groups elements
- `< >` denotes non-terminals
- Terminal symbols are in UPPERCASE or quoted
- `/* Version */` indicates version-specific features

## Core Grammar

### 1. Data Types

```ebnf
<firebird_data_type> ::=
    <numeric_type>
  | <character_type>
  | <date_time_type>
  | <binary_type>
  | <boolean_type>
  | <array_type>
  | <domain_type>

<numeric_type> ::=
    SMALLINT
  | INTEGER | INT
  | BIGINT
  | INT128  /* Firebird 4.0+ */
  | FLOAT [ ( <precision> ) ]
  | REAL
  | DOUBLE PRECISION
  | DECIMAL [ ( <precision> [ , <scale> ] ) ]
  | NUMERIC [ ( <precision> [ , <scale> ] ) ]
  | DECFLOAT [ ( { 16 | 34 } ) ]  /* Firebird 4.0+ */

<character_type> ::=
    CHAR [ ( <length> ) ] [ CHARACTER SET <charset_name> ]
  | CHARACTER [ ( <length> ) ] [ CHARACTER SET <charset_name> ]
  | VARCHAR ( <length> ) [ CHARACTER SET <charset_name> ]
  | CHARACTER VARYING ( <length> ) [ CHARACTER SET <charset_name> ]
  | NCHAR [ ( <length> ) ]
  | NATIONAL { CHAR | CHARACTER } [ ( <length> ) ]
  | NVARCHAR ( <length> )
  | NATIONAL { CHAR | CHARACTER } VARYING ( <length> )

<date_time_type> ::=
    DATE
  | TIME [ { WITH | WITHOUT } TIME ZONE ]  /* Firebird 4.0+ */
  | TIMESTAMP [ { WITH | WITHOUT } TIME ZONE ]  /* WITH TIME ZONE: Firebird 4.0+ */

<binary_type> ::=
    BLOB [ SUB_TYPE { <subtype_number> | <subtype_name> } ]
        [ SEGMENT SIZE <segment_size> ]
        [ CHARACTER SET <charset_name> ]

<boolean_type> ::= BOOLEAN  /* Firebird 3.0+ */

<array_type> ::=
    <base_type> [ <array_dimensions> ]

<array_dimensions> ::=
    '[' [ <lower_bound> : ] <upper_bound> { , [ <lower_bound> : ] <upper_bound> } ']'

<domain_type> ::= <domain_name>
```

### 2. DDL Statements

#### CREATE TABLE

```ebnf
<create_table_statement> ::=
    CREATE [ { GLOBAL | LOCAL } { TEMPORARY | TEMP } ] TABLE <table_name>
    [ EXTERNAL [ FILE ] '<filespec>' ]
    ( <table_element> { , <table_element> } )
    [ ON COMMIT { DELETE | PRESERVE } ROWS ]  /* For temporary tables */

<table_element> ::=
    <column_definition>
  | <table_constraint>
  | <like_clause>  /* Firebird 3.0+ */

<column_definition> ::=
    <column_name> { <data_type> | <domain_name> | COMPUTED BY ( <expression> ) }
    [ DEFAULT { <literal> | NULL | <context_variable> | <expression> } ]
    [ [ NOT ] NULL ]
    [ <column_constraint> { <column_constraint> } ]
    [ COLLATE <collation_name> ]

<column_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    { PRIMARY KEY
    | UNIQUE
    | REFERENCES <table_name> [ ( <column_name> ) ]
        [ ON DELETE { NO ACTION | CASCADE | SET NULL | SET DEFAULT } ]
        [ ON UPDATE { NO ACTION | CASCADE | SET NULL | SET DEFAULT } ]
    | CHECK ( <expression> )
    | GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY 
        [ ( <identity_options> ) ]  /* Firebird 3.0+ */ }

<identity_options> ::=
    [ START WITH <value> ]
    [ INCREMENT [ BY ] <value> ]

<table_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    { PRIMARY KEY ( <column_list> ) [ USING [ { ASC | DESC } ] INDEX <index_name> ]
    | UNIQUE ( <column_list> ) [ USING [ { ASC | DESC } ] INDEX <index_name> ]
    | FOREIGN KEY ( <column_list> ) REFERENCES <table_name> [ ( <column_list> ) ]
        [ ON DELETE { NO ACTION | CASCADE | SET NULL | SET DEFAULT } ]
        [ ON UPDATE { NO ACTION | CASCADE | SET NULL | SET DEFAULT } ]
    | CHECK ( <expression> ) }
```

#### CREATE INDEX

```ebnf
<create_index_statement> ::=
    CREATE [ UNIQUE ] [ { ASC | DESC } ] INDEX <index_name>
    ON <table_name> { ( <column_list> ) | COMPUTED BY ( <expression> ) }
    [ WHERE <search_condition> ]  /* Firebird 5.0+ partial index */
```

#### CREATE VIEW

```ebnf
<create_view_statement> ::=
    CREATE [ OR ALTER ] VIEW <view_name> [ ( <column_list> ) ]
    AS <select_statement>
    [ WITH CHECK OPTION ]
```

#### CREATE DOMAIN

```ebnf
<create_domain_statement> ::=
    CREATE DOMAIN <domain_name> [ AS ] <data_type>
    [ DEFAULT { <literal> | NULL | <context_variable> } ]
    [ [ NOT ] NULL ]
    [ CHECK ( <expression> ) ]
    [ COLLATE <collation_name> ]
```

#### CREATE SEQUENCE/GENERATOR

```ebnf
<create_sequence_statement> ::=
    CREATE { SEQUENCE | GENERATOR } <sequence_name>
    [ START WITH <value> ]
    [ INCREMENT [ BY ] <value> ]  /* SEQUENCE syntax: Firebird 3.0+ */
```

#### CREATE EXCEPTION

```ebnf
<create_exception_statement> ::=
    CREATE [ OR ALTER ] EXCEPTION <exception_name> '<message_text>'
    [ USING ( <parameter_list> ) ]  /* Firebird 5.0+ */
```

### 3. DML Statements

#### INSERT

```ebnf
<insert_statement> ::=
    INSERT INTO <table_or_view_name> [ ( <column_list> ) ]
    { DEFAULT VALUES
    | VALUES ( <value_list> )
    | <select_statement>
    | <execute_block_statement> }
    [ RETURNING <column_list> [ INTO <variable_list> ] ]

<merge_statement> ::=
    MERGE INTO <target_table> [ [ AS ] <alias> ]
    USING <source> ON <search_condition>
    [ <merge_when_clause> { <merge_when_clause> } ]
    [ PLAN <plan_clause> ]
    [ ORDER BY <order_list> ]  /* Firebird 5.0+ */
    [ RETURNING <column_list> [ INTO <variable_list> ] ]

<merge_when_clause> ::=
    WHEN MATCHED [ AND <condition> ] THEN 
        { UPDATE SET <assignment_list> | DELETE }
  | WHEN NOT MATCHED [ BY TARGET ] [ AND <condition> ] THEN
        INSERT [ ( <column_list> ) ] VALUES ( <value_list> )
  | WHEN NOT MATCHED BY SOURCE [ AND <condition> ] THEN
        { UPDATE SET <assignment_list> | DELETE }  /* Firebird 5.0+ */

<update_or_insert_statement> ::=
    UPDATE OR INSERT INTO <table_or_view_name> [ ( <column_list> ) ]
    VALUES ( <value_list> )
    [ MATCHING ( <column_list> ) ]
    [ RETURNING <column_list> [ INTO <variable_list> ] ]
```

#### UPDATE

```ebnf
<update_statement> ::=
    UPDATE <table_or_view_name> [ [ AS ] <alias> ]
    SET <assignment_list>
    [ WHERE <search_condition> | WHERE CURRENT OF <cursor_name> ]
    [ PLAN <plan_clause> ]
    [ ORDER BY <order_list> ]
    [ ROWS <value> [ TO <value> ] ]
    [ SKIP [ LOCKED ] ]  /* Firebird 5.0+ */
    [ RETURNING <column_list> [ INTO <variable_list> ] ]

<assignment> ::=
    <column_name> = { <expression> | DEFAULT }
```

#### DELETE

```ebnf
<delete_statement> ::=
    DELETE FROM <table_or_view_name> [ [ AS ] <alias> ]
    [ WHERE <search_condition> | WHERE CURRENT OF <cursor_name> ]
    [ PLAN <plan_clause> ]
    [ ORDER BY <order_list> ]
    [ ROWS <value> [ TO <value> ] ]
    [ SKIP [ LOCKED ] ]  /* Firebird 5.0+ */
    [ RETURNING <column_list> [ INTO <variable_list> ] ]
```

#### SELECT

```ebnf
<select_statement> ::=
    [ <with_clause> ]  /* Firebird 2.1+ */
    SELECT [ { ALL | DISTINCT | FIRST <value> [ SKIP <value> ] | SKIP <value> } ]
    <select_list>
    FROM <table_reference_list>
    [ WHERE <search_condition> ]
    [ GROUP BY <grouping_element_list> ]
    [ HAVING <search_condition> ]
    [ WINDOW <window_definition_list> ]  /* Firebird 3.0+ */
    [ PLAN <plan_clause> ]
    [ UNION [ { ALL | DISTINCT } ] <select_statement> ]
    [ ORDER BY <order_list> ]
    [ { ROWS <value> [ TO <value> ] | OFFSET <value> [ { ROW | ROWS } ] 
        [ FETCH { FIRST | NEXT } [ <value> ] { ROW | ROWS } ONLY ] } ]
    [ FOR { UPDATE [ OF <column_list> ] | LOCK } [ WITH LOCK ] [ SKIP LOCKED ] ]
    [ INTO <variable_list> ]  /* PSQL only */
    [ AS CURSOR <cursor_name> ]  /* PSQL only, Firebird 3.0+ */

<with_clause> ::=
    WITH [ RECURSIVE ] <cte> { , <cte> }

<cte> ::=
    <cte_name> [ ( <column_list> ) ] AS ( <select_statement> )

<select_list> ::=
    *
  | <select_item> { , <select_item> }

<select_item> ::=
    [ <table_name> . ] *
  | <expression> [ [ AS ] <alias> ]

<table_reference> ::=
    <table_primary>
  | <joined_table>

<table_primary> ::=
    <table_or_view_name> [ [ AS ] <alias> ]
  | ( <select_statement> ) [ AS ] <alias>
  | <procedure_name> ( [ <argument_list> ] ) [ AS ] <alias>
  | LATERAL <derived_table> [ AS ] <alias>  /* Firebird 4.0+ */

<joined_table> ::=
    <table_reference> [ <join_type> ] JOIN <table_primary> <join_specification>

<join_type> ::=
    INNER | { LEFT | RIGHT | FULL } [ OUTER ] | CROSS

<join_specification> ::=
    ON <search_condition>
  | USING ( <column_list> )
  | NATURAL

<grouping_element> ::=
    <expression>
  | ROLLUP ( <expression_list> )  /* Firebird 4.0+ */
  | CUBE ( <expression_list> )    /* Firebird 4.0+ */
  | GROUPING SETS ( <grouping_element_list> )  /* Firebird 4.0+ */
  | ( )
```

### 4. PSQL (Procedural SQL)

#### Stored Procedures

```ebnf
<create_procedure_statement> ::=
    CREATE [ OR ALTER ] PROCEDURE <procedure_name>
    [ ( <parameter_declaration_list> ) ]
    [ RETURNS ( <parameter_declaration_list> ) ]
    [ EXTERNAL NAME '<external_module_name>!<routine_name>'
        ENGINE <engine_name> [ AS <external_body> ] ]  /* Firebird 3.0+ */
    [ SQL SECURITY { DEFINER | INVOKER } ]  /* Firebird 4.0+ */
    AS
    [ <declarations> ]
    BEGIN
        <statements>
    END

<parameter_declaration> ::=
    <parameter_name> <data_type> [ { = | DEFAULT } <default_value> ]
    [ NOT NULL ] [ COLLATE <collation_name> ]

<alter_procedure_statement> ::=
    ALTER PROCEDURE <procedure_name>
    [ ( <parameter_declaration_list> ) ]
    [ RETURNS ( <parameter_declaration_list> ) ]
    AS
    [ <declarations> ]
    BEGIN
        <statements>
    END

<drop_procedure_statement> ::=
    DROP PROCEDURE [ IF EXISTS ] <procedure_name>

<execute_procedure_statement> ::=
    EXECUTE PROCEDURE <procedure_name> [ ( <argument_list> ) ]
    [ RETURNING_VALUES <variable_list> ]
```

#### Stored Functions

```ebnf
<create_function_statement> ::=
    CREATE [ OR ALTER ] FUNCTION <function_name>
    [ ( <parameter_declaration_list> ) ]
    RETURNS { <data_type> [ COLLATE <collation_name> ]
            | [ <data_type> ] TABLE ( <parameter_declaration_list> ) }  /* Firebird 5.0+ table functions */
    [ DETERMINISTIC ]
    [ EXTERNAL NAME '<external_module_name>!<routine_name>'
        ENGINE <engine_name> [ AS <external_body> ] ]  /* Firebird 3.0+ */
    [ SQL SECURITY { DEFINER | INVOKER } ]  /* Firebird 4.0+ */
    AS
    [ <declarations> ]
    BEGIN
        <statements>
    END
```

#### Triggers

```ebnf
<create_trigger_statement> ::=
    CREATE [ OR ALTER ] TRIGGER <trigger_name>
    FOR <table_or_view_name>
    [ ACTIVE | INACTIVE ]
    { BEFORE | AFTER } { INSERT | UPDATE | DELETE }
    [ OR { INSERT | UPDATE | DELETE } ]
    [ POSITION <number> ]
    [ FOLLOWS <trigger_name> { , <trigger_name> } ]  /* Firebird 4.0+ */
    [ SQL SECURITY { DEFINER | INVOKER } ]  /* Firebird 4.0+ */
    AS
    [ <declarations> ]
    BEGIN
        <statements>
    END

<database_trigger> ::=
    CREATE [ OR ALTER ] TRIGGER <trigger_name>
    [ ACTIVE | INACTIVE ]
    ON { CONNECT | DISCONNECT | TRANSACTION { START | COMMIT | ROLLBACK } }
    [ POSITION <number> ]
    [ SQL SECURITY { DEFINER | INVOKER } ]  /* Firebird 4.0+ */
    AS
    [ <declarations> ]
    BEGIN
        <statements>
    END

<ddl_trigger> ::=
    CREATE [ OR ALTER ] TRIGGER <trigger_name>
    [ ACTIVE | INACTIVE ]
    { BEFORE | AFTER } { <ddl_event> } [ OR { <ddl_event> } ]
    [ POSITION <number> ]
    [ SQL SECURITY { DEFINER | INVOKER } ]  /* Firebird 4.0+ */
    AS
    [ <declarations> ]
    BEGIN
        <statements>
    END

<ddl_event> ::=
    ANY DDL STATEMENT
  | CREATE { TABLE | PROCEDURE | FUNCTION | TRIGGER | EXCEPTION 
           | VIEW | DOMAIN | ROLE | SEQUENCE | USER | INDEX | COLLATION
           | PACKAGE | PACKAGE BODY | MAPPING }
  | ALTER { TABLE | PROCEDURE | FUNCTION | TRIGGER | EXCEPTION 
          | VIEW | DOMAIN | ROLE | SEQUENCE | USER | INDEX | COLLATION
          | CHARACTER SET | PACKAGE | MAPPING }
  | DROP { TABLE | PROCEDURE | FUNCTION | TRIGGER | EXCEPTION 
         | VIEW | DOMAIN | ROLE | SEQUENCE | USER | INDEX | COLLATION
         | PACKAGE | PACKAGE BODY | MAPPING }
```

#### PSQL Statements

```ebnf
<psql_statement> ::=
    <assignment_statement>
  | <if_statement>
  | <case_statement>
  | <while_statement>
  | <for_statement>
  | <leave_statement>
  | <continue_statement>  /* Firebird 3.0+ */
  | <exit_statement>
  | <suspend_statement>
  | <execute_statement>
  | <execute_block_statement>
  | <exception_statement>
  | <return_statement>  /* Firebird 2.5+ */
  | <cursor_statement>
  | <sql_statement>
  | <block_statement>

<declarations> ::=
    [ DECLARE [ VARIABLE ] <variable_declaration> ; { <variable_declaration> ; } ]
    [ DECLARE <cursor_declaration> ; { <cursor_declaration> ; } ]
    [ DECLARE { PROCEDURE | FUNCTION } <subroutine_declaration> ; { <subroutine_declaration> ; } ]  /* Firebird 3.0+ */

<variable_declaration> ::=
    <variable_name> { <data_type> | TYPE OF <domain_name> | TYPE OF COLUMN <table_name>.<column_name> }
    [ NOT NULL ] [ COLLATE <collation_name> ]
    [ { = | DEFAULT } <initial_value> ]

<cursor_declaration> ::=
    <cursor_name> [ SCROLL ] CURSOR FOR ( <select_statement> )  /* SCROLL: Firebird 3.0+ */

<assignment_statement> ::=
    <variable> = <expression> ;

<if_statement> ::=
    IF ( <condition> ) THEN
        <statement_or_block>
    [ ELSE
        <statement_or_block> ]

<case_statement> ::=
    CASE <expression>
        WHEN <value> { , <value> } THEN <statement_or_block>
        { WHEN <value> { , <value> } THEN <statement_or_block> }
        [ ELSE <statement_or_block> ]
    END
  | CASE
        WHEN <condition> THEN <statement_or_block>
        { WHEN <condition> THEN <statement_or_block> }
        [ ELSE <statement_or_block> ]
    END

<while_statement> ::=
    [ <label> : ]
    WHILE ( <condition> ) DO
        <statement_or_block>

<for_statement> ::=
    [ <label> : ]
    FOR <select_statement> [ AS CURSOR <cursor_name> ]
    DO <statement_or_block>
  | [ <label> : ]
    FOR EXECUTE STATEMENT <string_expression> [ ( <parameter_list> ) ]
        [ ON EXTERNAL [ DATA SOURCE ] <connection_string> 
          [ AS USER <user> PASSWORD <password> [ ROLE <role> ] ] ]
        [ WITH { AUTONOMOUS | COMMON } TRANSACTION ]  /* Firebird 2.5+ */
        [ INTO <variable_list> ]
    DO <statement_or_block>

<leave_statement> ::=
    LEAVE [ <label> ] ;

<continue_statement> ::=
    CONTINUE [ <label> ] ;  /* Firebird 3.0+ */

<exit_statement> ::=
    EXIT ;

<suspend_statement> ::=
    SUSPEND ;

<execute_statement> ::=
    EXECUTE STATEMENT <string_expression> [ ( <parameter_list> ) ]
    [ ON EXTERNAL [ DATA SOURCE ] <connection_string> 
      [ AS USER <user> PASSWORD <password> [ ROLE <role> ] ] ]
    [ WITH { AUTONOMOUS | COMMON } TRANSACTION ]  /* Firebird 2.5+ */
    [ WITH { CALLER | DEFINER } PRIVILEGES ]  /* Firebird 4.0+ */
    [ INTO <variable_list> ] ;

<exception_statement> ::=
    EXCEPTION [ <exception_name> [ <exception_message> | USING ( <parameter_list> ) ] ] ;

<return_statement> ::=
    RETURN <expression> ;  /* Functions only, Firebird 2.5+ */

<block_statement> ::=
    BEGIN
        <statements>
    [ WHEN { <error_list> | ANY } DO
        <statement_or_block> ]
    END

<error_list> ::=
    <error> { , <error> }

<error> ::=
    { EXCEPTION <exception_name>
    | SQLCODE <sqlcode>
    | SQLSTATE <sqlstate>
    | GDSCODE <gdscode> }
```

#### EXECUTE BLOCK

```ebnf
<execute_block_statement> ::=
    EXECUTE BLOCK [ ( <parameter_declaration_list> ) ]
    [ RETURNS ( <parameter_declaration_list> ) ]
    AS
    [ <declarations> ]
    BEGIN
        <statements>
    END
```

### 5. Cursor Operations

```ebnf
<cursor_statement> ::=
    <open_cursor>
  | <fetch_cursor>
  | <close_cursor>

<open_cursor> ::=
    OPEN <cursor_name> ;

<fetch_cursor> ::=
    FETCH [ <fetch_direction> ] [ FROM ] <cursor_name>
    INTO <variable_list> ;

<fetch_direction> ::=
    NEXT | PRIOR | FIRST | LAST 
  | ABSOLUTE <value> | RELATIVE <value>  /* For SCROLL cursors */

<close_cursor> ::=
    CLOSE <cursor_name> ;
```

### 6. Packages (Firebird 3.0+)

```ebnf
<create_package_header> ::=
    CREATE [ OR ALTER ] PACKAGE <package_name>
    [ SQL SECURITY { DEFINER | INVOKER } ]  /* Firebird 4.0+ */
    AS
    BEGIN
        [ <package_item> ; { <package_item> ; } ]
    END

<create_package_body> ::=
    CREATE [ OR ALTER ] PACKAGE BODY <package_name>
    AS
    BEGIN
        [ <package_body_item> ; { <package_body_item> ; } ]
        [ <initialization_block> ; ]
    END

<package_item> ::=
    <function_declaration>
  | <procedure_declaration>
  | <variable_declaration>
  | <cursor_declaration>
  | <type_declaration>

<package_body_item> ::=
    <function_implementation>
  | <procedure_implementation>
  | <variable_declaration>
  | <cursor_declaration>
  | <type_declaration>

<initialization_block> ::=
    BEGIN
        <statements>
    END
```

### 7. User-Defined Functions (UDF) - Legacy

```ebnf
<declare_external_function> ::=
    DECLARE EXTERNAL FUNCTION <function_name>
    [ <parameter_type> { , <parameter_type> } ]
    RETURNS { <parameter_type> [ BY VALUE ] | PARAMETER <position> }
    [ FREE_IT ]
    ENTRY_POINT '<entry_point>'
    MODULE_NAME '<module_name>'
```

### 8. Window Functions (Firebird 3.0+)

```ebnf
<window_function> ::=
    <window_function_name> ( [ <expression_list> ] ) OVER <window_specification>

<window_specification> ::=
    ( [ <partition_clause> ] [ <order_clause> ] [ <frame_clause> ] )
  | <window_name>

<window_definition> ::=
    <window_name> AS ( <window_specification> )

<partition_clause> ::=
    PARTITION BY <expression_list>

<order_clause> ::=
    ORDER BY <order_item_list>

<frame_clause> ::=
    { RANGE | ROWS } { <frame_start> | BETWEEN <frame_start> AND <frame_end> }
    [ EXCLUDE { CURRENT ROW | GROUP | TIES | NO OTHERS } ]  /* Firebird 4.0+ */

<frame_start> ::=
    UNBOUNDED PRECEDING
  | <expression> PRECEDING
  | CURRENT ROW
  | <expression> FOLLOWING

<frame_end> ::=
    <expression> PRECEDING
  | CURRENT ROW
  | <expression> FOLLOWING
  | UNBOUNDED FOLLOWING

<window_function_name> ::=
    ROW_NUMBER | RANK | DENSE_RANK | PERCENT_RANK | CUME_DIST
  | LAG | LEAD | FIRST_VALUE | LAST_VALUE | NTH_VALUE
  | NTILE
```

### 9. Common Table Expressions (CTE)

```ebnf
<with_clause> ::=
    WITH [ RECURSIVE ] <cte_definition> { , <cte_definition> }

<cte_definition> ::=
    <cte_name> [ ( <column_list> ) ] AS ( <select_statement> )
```

### 10. Transaction Control

```ebnf
<transaction_statement> ::=
    SET TRANSACTION [ <transaction_option> { <transaction_option> } ]
  | COMMIT [ WORK ] [ RETAIN [ SNAPSHOT ] ]
  | ROLLBACK [ WORK ] [ TO [ SAVEPOINT ] <savepoint_name> ]
    [ RETAIN [ SNAPSHOT ] ]
  | SAVEPOINT <savepoint_name>
  | RELEASE SAVEPOINT <savepoint_name> [ ONLY ]

<transaction_option> ::=
    READ { WRITE | ONLY }
  | [ ISOLATION LEVEL ] { SNAPSHOT [ TABLE STABILITY ] 
                        | READ { COMMITTED [ { NO | } RECORD_VERSION ] | UNCOMMITTED } }
  | [ NO ] WAIT
  | [ LOCK TIMEOUT <seconds> ]
  | NO AUTO UNDO  /* Firebird 3.0+ */
  | RESTART REQUESTS  /* Firebird 4.0+ */
  | IGNORE LIMBO
  | AUTOCOMMIT
  | RESERVING <table_list> FOR <reservation_specification>

<reservation_specification> ::=
    [ SHARED | PROTECTED ] { READ | WRITE }
```

### 11. Security and Access Control

```ebnf
<create_role> ::=
    CREATE ROLE <role_name>
    [ SET SYSTEM PRIVILEGES TO <system_privilege_list> ]  /* Firebird 4.0+ */

<grant_statement> ::=
    GRANT { <privilege_list> ON [ <object_type> ] <object_name>
          | <role_list>
          | <system_privilege_list> }  /* Firebird 4.0+ */
    TO { <grantee_list> | <role_list> }
    [ WITH { GRANT | ADMIN } OPTION ]
    [ GRANTED BY { CURRENT_USER | CURRENT_ROLE } ]

<privilege> ::=
    ALL [ PRIVILEGES ]
  | SELECT | INSERT | UPDATE [ ( <column_list> ) ] | DELETE
  | REFERENCES [ ( <column_list> ) ]
  | EXECUTE | USAGE | CREATE | ALTER ANY | DROP ANY

<system_privilege> ::=
    USER_MANAGEMENT | READ_RAW_PAGES | CREATE_USER_TYPES
  | USE_NBACKUP_UTILITY | CHANGE_SHUTDOWN_MODE | TRACE_ANY_ATTACHMENT
  | MONITOR_ANY_ATTACHMENT | ACCESS_SHUTDOWN_DATABASE
  | CREATE_DATABASE | DROP_DATABASE | USE_GBAK_UTILITY
  | USE_GSTAT_UTILITY | USE_GFIX_UTILITY
  | IGNORE_DB_TRIGGERS | CHANGE_HEADER_SETTINGS
  | SELECT_ANY_OBJECT_IN_DATABASE  /* Firebird 4.0+ */
  | ACCESS_ANY_OBJECT_IN_DATABASE  /* Firebird 4.0+ */
  | MODIFY_ANY_OBJECT_IN_DATABASE  /* Firebird 4.0+ */
  | CHANGE_MAPPING_RULES | GRANT_REVOKE_ON_ANY_OBJECT
  | GRANT_REVOKE_ANY_DDL_RIGHT | CREATE_PRIVILEGED_ROLES
  
<revoke_statement> ::=
    REVOKE [ { GRANT | ADMIN } OPTION FOR ]
    { <privilege_list> ON [ <object_type> ] <object_name>
    | <role_list>
    | <system_privilege_list> }  /* Firebird 4.0+ */
    FROM { <grantee_list> | <role_list> }
    [ GRANTED BY { CURRENT_USER | CURRENT_ROLE } ]
```

### 12. Mapping (Firebird 3.0+)

```ebnf
<create_mapping> ::=
    CREATE [ OR ALTER ] [ GLOBAL ] MAPPING <mapping_name>
    USING { PLUGIN <plugin_name> [ IN <database_name> ]
          | ANY PLUGIN [ IN <database_name> | SERVERWIDE ]
          | MAPPING [ IN <database_name> ] | '*' [ IN <database_name> ] }
    FROM { ANY <object_type> | <object_type> <object_name> }
    TO { USER | ROLE } [ <target_name> ]

<alter_mapping> ::=
    ALTER [ GLOBAL ] MAPPING <mapping_name>
    USING { PLUGIN <plugin_name> [ IN <database_name> ]
          | ANY PLUGIN [ IN <database_name> | SERVERWIDE ]
          | MAPPING [ IN <database_name> ] | '*' [ IN <database_name> ] }
    FROM { ANY <object_type> | <object_type> <object_name> }
    TO { USER | ROLE } [ <target_name> ]

<drop_mapping> ::=
    DROP [ GLOBAL ] MAPPING <mapping_name>
```

### 13. Management Statements

```ebnf
<set_statistics> ::=
    SET STATISTICS INDEX <index_name>

<set_generator> ::=
    SET GENERATOR <generator_name> TO <value>

<alter_sequence> ::=
    ALTER SEQUENCE <sequence_name> { RESTART [ WITH <value> ] | INCREMENT [ BY ] <value> }

<comment_statement> ::=
    COMMENT ON { DATABASE | <object_type> <object_name> | COLUMN <table_name>.<column_name> 
               | PARAMETER <procedure_name>.<parameter_name> }
    IS { '<text>' | NULL }

<create_collation> ::=
    CREATE COLLATION <collation_name>
    FOR <charset_name>
    [ FROM { <base_collation> | EXTERNAL ( '<collation_name>' ) } ]
    [ NO PAD | PAD SPACE ]
    [ CASE { SENSITIVE | INSENSITIVE } ]
    [ ACCENT { SENSITIVE | INSENSITIVE } ]
    [ ATTRIBUTES '<attributes>' ]

<alter_character_set> ::=
    ALTER CHARACTER SET <charset_name>
    SET DEFAULT COLLATION <collation_name>
```

### 14. Context Variables

```ebnf
<context_variable> ::=
    CURRENT_CONNECTION | CURRENT_TRANSACTION
  | CURRENT_DATE | CURRENT_TIME | CURRENT_TIMESTAMP
  | LOCALTIME | LOCALTIMESTAMP  /* Firebird 2.5+ */
  | CURRENT_USER | CURRENT_ROLE
  | ROW_COUNT
  | SQLCODE | GDSCODE | SQLSTATE
  | INSERTING | UPDATING | DELETING  /* Triggers only */
  | TODAY | NOW | TOMORROW | YESTERDAY
```

### 15. Built-in Functions

```ebnf
<function_call> ::=
    <function_name> ( [ <argument_list> ] )

<aggregate_function> ::=
    COUNT ( { * | [ { ALL | DISTINCT } ] <expression> } )
  | SUM ( [ { ALL | DISTINCT } ] <expression> )
  | AVG ( [ { ALL | DISTINCT } ] <expression> )
  | MIN ( [ { ALL | DISTINCT } ] <expression> )
  | MAX ( [ { ALL | DISTINCT } ] <expression> )
  | LIST ( [ { ALL | DISTINCT } ] <expression> [ , <delimiter> ] )
  | STDDEV_POP ( <expression> )  /* Firebird 2.1+ */
  | STDDEV_SAMP ( <expression> )  /* Firebird 2.1+ */
  | VAR_POP ( <expression> )  /* Firebird 2.1+ */
  | VAR_SAMP ( <expression> )  /* Firebird 2.1+ */
  | COVAR_POP ( <expression1> , <expression2> )  /* Firebird 2.1+ */
  | COVAR_SAMP ( <expression1> , <expression2> )  /* Firebird 2.1+ */
  | CORR ( <expression1> , <expression2> )  /* Firebird 2.1+ */
  | REGR_* ( <expression1> , <expression2> )  /* Firebird 4.0+ */
  | ANY_VALUE ( <expression> )  /* Firebird 5.0+ */

<string_function> ::=
    ASCII_CHAR ( <code> )
  | ASCII_VAL ( <string> )
  | BIT_LENGTH ( <string> )
  | CHAR_LENGTH ( <string> ) | CHARACTER_LENGTH ( <string> )
  | CONTAINING ( <substring> )  /* Predicate, not function */
  | LEFT ( <string> , <length> )
  | LOWER ( <string> )
  | LPAD ( <string> , <length> [ , <pad_string> ] )
  | LTRIM ( <string> [ , <trim_string> ] )  /* Firebird 4.0+ */
  | OCTET_LENGTH ( <string> )
  | OVERLAY ( <string> PLACING <substring> FROM <position> [ FOR <length> ] )
  | POSITION ( <substring> IN <string> )
  | REPLACE ( <string> , <search> , <replace> )
  | REVERSE ( <string> )
  | RIGHT ( <string> , <length> )
  | RPAD ( <string> , <length> [ , <pad_string> ] )
  | RTRIM ( <string> [ , <trim_string> ] )  /* Firebird 4.0+ */
  | STARTING [ WITH ] ( <substring> )  /* Predicate, not function */
  | SUBSTRING ( <string> FROM <position> [ FOR <length> ] )
  | TRIM ( [ [ { LEADING | TRAILING | BOTH } ] [ <trim_string> ] FROM ] <string> )
  | UPPER ( <string> )

<datetime_function> ::=
    DATEADD ( <date_part> , <value> , <datetime> )
  | DATEDIFF ( <date_part> , <datetime1> , <datetime2> )
  | EXTRACT ( <date_part> FROM <datetime> )
  | FIRST_DAY ( OF { YEAR | QUARTER | MONTH | WEEK } FROM <date> )  /* Firebird 4.0+ */
  | LAST_DAY ( OF { YEAR | QUARTER | MONTH | WEEK } FROM <date> )  /* Firebird 4.0+ */

<date_part> ::=
    YEAR | QUARTER | MONTH | WEEK | DAY | WEEKDAY | YEARDAY
  | HOUR | MINUTE | SECOND | MILLISECOND
  | TIMEZONE_HOUR | TIMEZONE_MINUTE  /* Firebird 4.0+ */

<math_function> ::=
    ABS ( <value> )
  | ACOS ( <value> )
  | ACOSH ( <value> )  /* Firebird 3.0+ */
  | ASIN ( <value> )
  | ASINH ( <value> )  /* Firebird 3.0+ */
  | ATAN ( <value> )
  | ATAN2 ( <y> , <x> )
  | ATANH ( <value> )  /* Firebird 3.0+ */
  | CEIL ( <value> ) | CEILING ( <value> )
  | COS ( <value> )
  | COSH ( <value> )
  | COT ( <value> )
  | EXP ( <value> )
  | FLOOR ( <value> )
  | LN ( <value> )
  | LOG ( <base> , <value> )
  | LOG10 ( <value> )
  | MOD ( <value1> , <value2> )
  | PI ( )
  | POWER ( <value> , <exponent> )
  | RAND ( )
  | ROUND ( <value> [ , <scale> ] )
  | SIGN ( <value> )
  | SIN ( <value> )
  | SINH ( <value> )
  | SQRT ( <value> )
  | TAN ( <value> )
  | TANH ( <value> )
  | TRUNC ( <value> [ , <scale> ] )

<conversion_function> ::=
    CAST ( <expression> AS <data_type> )
  | DECODE ( <expression> , <search> , <result> { , <search> , <result> } [ , <default> ] )
  | IIF ( <condition> , <true_value> , <false_value> )
  | NULLIF ( <expression1> , <expression2> )
  | COALESCE ( <expression> { , <expression> } )

<blob_function> ::=
    BLOB_APPEND ( <blob> [ , <value> { , <value> } ] )  /* Firebird 3.0+ */

<uuid_function> ::=
    GEN_UUID ( )
  | UUID_TO_CHAR ( <uuid> )  /* Firebird 2.5+ */
  | CHAR_TO_UUID ( <string> )  /* Firebird 2.5+ */

<hash_function> ::=
    HASH ( <value> [ USING <algorithm> ] )  /* Firebird 2.1+ */
  | CRYPT_HASH ( <value> USING <algorithm> )  /* Firebird 4.0+ */

<algorithm> ::= MD5 | SHA1 | SHA256 | SHA512 | SHA3_224 | SHA3_256 | SHA3_384 | SHA3_512

<encrypt_decrypt> ::=
    ENCRYPT ( <value> USING <algorithm> KEY <key> [ IV <iv> ] [ <options> ] )  /* Firebird 3.0+ */
  | DECRYPT ( <value> USING <algorithm> KEY <key> [ IV <iv> ] [ <options> ] )  /* Firebird 3.0+ */
  | RSA_ENCRYPT ( <value> KEY <key> [ LPARAM <tag> ] [ HASH <hash_algorithm> ] )  /* Firebird 4.0+ */
  | RSA_DECRYPT ( <value> KEY <key> [ LPARAM <tag> ] [ HASH <hash_algorithm> ] )  /* Firebird 4.0+ */
  | RSA_SIGN_HASH ( <value> KEY <key> [ HASH <hash_algorithm> ] [ SALTLENGTH <length> ] )  /* Firebird 4.0+ */
  | RSA_VERIFY_HASH ( <value> SIGNATURE <signature> KEY <key> [ HASH <hash_algorithm> ] [ SALTLENGTH <length> ] )  /* Firebird 4.0+ */

<bit_function> ::=
    BIN_AND ( <value1> , <value2> )
  | BIN_OR ( <value1> , <value2> )
  | BIN_XOR ( <value1> , <value2> )
  | BIN_NOT ( <value> )
  | BIN_SHL ( <value> , <positions> )
  | BIN_SHR ( <value> , <positions> )

<compare_function> ::=
    MINVALUE ( <value> { , <value> } )  /* Firebird 2.1+ */
  | MAXVALUE ( <value> { , <value> } )  /* Firebird 2.1+ */
  | COMPARE_DECFLOAT ( <value1> , <value2> )  /* Firebird 4.0+ */
  | TOTALORDER ( <value1> , <value2> )  /* Firebird 4.0+ */
```

### 16. Operators

```ebnf
<operator> ::=
    -- Arithmetic
    + | - | * | / | 

    -- Comparison
    = | <> | != | ~= | ^= | < | <= | > | >= 
  | IS [ NOT ] { NULL | TRUE | FALSE | UNKNOWN }
  | IS [ NOT ] DISTINCT FROM

    -- Logical
    AND | OR | NOT

    -- String
    || | COLLATE

    -- Set membership
    [ NOT ] IN
  | [ NOT ] EXISTS
  | { ALL | SOME | ANY }
  | SINGULAR

    -- Pattern matching
    [ NOT ] LIKE [ ESCAPE <escape_char> ]
  | [ NOT ] SIMILAR TO [ ESCAPE <escape_char> ]
  | [ NOT ] CONTAINING
  | [ NOT ] STARTING [ WITH ]

    -- Range
    [ NOT ] BETWEEN ... AND ...
```

### 17. Query Hints and Optimization

```ebnf
<plan_clause> ::=
    PLAN <plan_expression>

<plan_expression> ::=
    ( <plan_item> { , <plan_item> } )
  | <plan_item>

<plan_item> ::=
    [ <table_alias> ] { NATURAL 
                      | INDEX ( <index_name> { , <index_name> } ) 
                      | ORDER <index_name> }
  | JOIN ( <plan_item> , <plan_item> { , <plan_item> } )
  | [ SORT ] MERGE ( <plan_item> { , <plan_item> } )
  | HASH ( <plan_item> { , <plan_item> } )  /* Firebird 3.0+ */
  | <derived_table_alias> ( <plan_expression> )
```

### 18. External Tables

```ebnf
<external_table_clause> ::=
    EXTERNAL [ FILE ] '<file_path>'
```

### 19. Monitoring Tables

```ebnf
<monitoring_tables> ::=
    MON$ATTACHMENTS | MON$CALL_STACK | MON$CONTEXT_VARIABLES
  | MON$DATABASE | MON$IO_STATS | MON$MEMORY_USAGE
  | MON$RECORD_STATS | MON$STATEMENTS | MON$TABLE_STATS
  | MON$TRANSACTIONS | MON$COMPILED_STATEMENTS  /* Firebird 5.0+ */
  | MON$KEYWORDS  /* Firebird 5.0+ */
```

### 20. Special Syntax Elements

```ebnf
<special_values> ::=
    TRUE | FALSE | UNKNOWN  /* Boolean literals, Firebird 3.0+ */
  | NULL

<named_argument> ::=
    <parameter_name> => <expression>  /* Firebird 5.0+ */

<default_argument> ::=
    DEFAULT  /* For procedure/function calls with default parameters */

<returning_clause> ::=
    RETURNING <column_list> [ INTO <variable_list> ]

<cursor_attribute> ::=
    <cursor_name> % { ISOPEN | FOUND | NOTFOUND | ROWCOUNT }  /* PSQL */

<qualified_name> ::=
    [ <schema_name> . ] <object_name>  /* Schema support: planned future */

<delimited_identifier> ::=
    " <identifier_body> "

<string_literal> ::=
    ' <string_body> '
  | q'<delimiter> <string_body> <delimiter>'  /* Firebird 3.0+ */

<binary_literal> ::=
    x'<hex_digits>'  /* Firebird 2.5+ */
  | 0x<hex_digits>  /* Firebird 4.0+ */

<introducer> ::=
    _<charset_name>

<escape_sequence> ::=
    '' | "" | <escape_char><special_char>
```

## Usage Notes

1. **Dialect Settings**: Firebird supports dialect 1 and 3, affecting identifier case sensitivity and other behaviors
2. **Identifier Length**: Maximum 63 characters (31 in dialect 1)
3. **Case Sensitivity**: 
   - Unquoted identifiers are case-insensitive, stored in uppercase
   - Quoted identifiers preserve case
4. **String Literals**: Use single quotes; double quotes for identifiers
5. **Comments**: 
   - Single line: `--` or `//` (Firebird 5.0+)
   - Multi-line: `/* ... */`
6. **Statement Terminators**: `;` in SQL, can be changed in ISQL with `SET TERM`
7. **NULL Handling**: Firebird follows SQL standard NULL semantics
8. **Arrays**: Multi-dimensional arrays supported but with limitations
9. **Contexts**: Firebird supports multiple contexts (USER_SESSION, USER_TRANSACTION, SYSTEM)
10. **Character Sets**: Extensive support including UTF8, must be specified correctly

This comprehensive grammar covers Firebird SQL and PSQL from version 2.5 through 5.0, providing a complete foundation for implementing a Firebird SQL parser.