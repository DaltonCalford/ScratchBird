# ScratchBird SQL Complete BNF/EBNF Grammar
## Version 1.0 - Comprehensive Grammar Specification

## Overview

This document provides the complete BNF/EBNF grammar for ScratchBird SQL, compiled from all technical specifications. ScratchBird uses context-aware parsing with minimal reserved words and supports multiple SQL dialect features.

## Notation Conventions

```ebnf
::=          Definition
[ ]          Optional (0 or 1)
{ }          Repetition (0 or more)
( )          Grouping
|            Alternative
< >          Non-terminal
UPPERCASE    Terminal (keyword)
'literal'    Terminal (literal)
...          Continuation
```

## 1. Top-Level Grammar

```ebnf
<scratchbird_sql> ::= 
    { <statement> [ <statement_terminator> ] }

<statement> ::=
    <ddl_statement>
  | <dml_statement>
  | <dcl_statement>
  | <tcl_statement>
  | <psql_statement>
  | <utility_statement>
  | <system_command>
  | <execute_block>
  | <empty_statement>

<statement_terminator> ::=
    ';'
  | GO                          -- MSSQL compatibility
  | '/'                         -- Oracle compatibility
  | <implicit_termination>      -- Context-aware termination

<empty_statement> ::= ';'
```

## 2. DDL - Data Definition Language

### 2.1 Database Operations

```ebnf
<ddl_statement> ::=
    <create_statement>
  | <alter_statement>
  | <drop_statement>
  | <rename_statement>
  | <truncate_statement>
  | <comment_statement>

<create_statement> ::=
    <create_database>
  | <create_schema>
  | <create_table>
  | <create_index>
  | <create_view>
  | <create_procedure>
  | <create_function>
  | <create_trigger>
  | <create_domain>
  | <create_sequence>
  | <create_type>
  | <create_role>
  | <create_user>
  | <create_tablespace>
  | <create_event>
  | <create_exception>
  | <create_generator>

<create_database> ::=
    CREATE DATABASE [ IF NOT EXISTS ] <database_name>
    [ PAGE_SIZE '=' <page_size> ]
    [ DEFAULT CHARACTER SET <charset_name> ]
    [ DEFAULT COLLATE <collation_name> ]
    [ ENCRYPTED [ WITH PASSWORD <string_literal> ] ]
    [ OWNER <user_name> ]
    [ TABLESPACE <tablespace_name> ]
  | CREATE DATABASE [ IF NOT EXISTS ] EMULATED <emulated_dialect>
    [ ON SERVER <server_name> ] <source_spec>
    [ ALIAS <alias_list> ]
    [ WITH [ OPTIONS ] '(' <db_option_list> ')' ]

<emulated_dialect> ::= <identifier> | <string_literal>
<source_spec> ::= <identifier> | <string_literal>
<alias_list> ::= <alias_name> { ',' <alias_name> }
<alias_name> ::= <identifier> | <string_literal>
<db_option_list> ::= <option_pair> { ',' <option_pair> }
<option_pair> ::= <identifier> '=' <option_value>
<option_value> ::= <identifier> | <string_literal> | <integer_literal> | <float_literal>

<page_size> ::= 
    '8K' | '16K' | '32K' | '64K' | '128K'
```

### 2.2 Schema Operations

```ebnf
<create_schema> ::=
    CREATE SCHEMA [ IF NOT EXISTS ] <schema_name>
    [ AUTHORIZATION <owner_name> ]
    [ DEFAULT CHARACTER SET <charset_name> ]
    [ PATH <schema_path> ]
    [ <schema_element> { <schema_element> } ]

<schema_element> ::=
    <create_table>
  | <create_view>
  | <create_index>
  | <create_sequence>
  | <create_domain>
  | <grant_statement>

<schema_path> ::=
    <schema_name> { ',' <schema_name> }
```

### 2.3 Table Operations

```ebnf
<create_table> ::=
    CREATE [ <table_scope> ] TABLE [ IF NOT EXISTS ] <table_name>
    (
        <table_element> { ',' <table_element> }
    )
    [ <table_options> ]
    [ AS <select_statement> [ WITH [ NO ] DATA ] ]

<table_scope> ::=
    [ GLOBAL ] TEMPORARY 
  | TEMP
  | LOCAL TEMPORARY
  | EXTERNAL
  | VIRTUAL

### 2.4 Alter Operations (stubs)

```ebnf
<alter_statement> ::=
    <alter_table_tablespace>
  | <alter_table_shard>
  | <alter_table_rls>

<alter_table_tablespace> ::=
    ALTER TABLE <table_name>
    SET TABLESPACE <tablespace_name>
    [ ONLINE [ WITH '(' <tablespace_online_option_list> ')' ] ]

<tablespace_online_option_list> ::=
    <tablespace_online_option> { ',' <tablespace_online_option> }

<tablespace_online_option> ::=
    BATCH_SIZE '=' <integer_literal>
  | MAX_LAG_MS '=' <integer_literal>
  | THROTTLE_MS '=' <integer_literal>
  | LOCK_TIMEOUT_MS '=' <integer_literal>

<alter_table_shard> ::=
    ALTER TABLE <table_name> <shard_action>

<shard_action> ::=
    SPLIT SHARD
  | MERGE SHARDS
  | MOVE SHARD <shard_id> TO <node_id>
  | REBALANCE

<shard_id> ::= <identifier> | <uuid_literal>
<node_id> ::= <identifier> | <uuid_literal>
```

Note: shard actions are cluster/Beta-only and require a sharding-capable deployment.

<table_element> ::=
    <column_definition>
  | <table_constraint>
  | <table_period_definition>
  | LIKE <table_name> [ <like_options> ]

<column_definition> ::=
    <column_name> <data_type> 
    [ <column_constraint> { <column_constraint> } ]
    [ COLLATE <collation_name> ]
    [ COMPRESS [ USING <compression_method> ] ]
    [ ENCRYPT [ USING <encryption_method> ] ]

<data_type> ::=
    <predefined_type>
  | <domain_name>
  | <array_type>
  | <multiset_type>
  | <row_type>
  | <reference_type>
  | <user_defined_type>
```

### 2.4 Data Types

```ebnf
<predefined_type> ::=
    <numeric_type>
  | <character_type>
  | <datetime_type>
  | <boolean_type>
  | <binary_type>
  | <uuid_type>
  | <json_type>
  | <xml_type>
  | <geometric_type>

<numeric_type> ::=
    SMALLINT | INTEGER | BIGINT | INT128
  | DECIMAL [ '(' <precision> [ ',' <scale> ] ')' ]
  | NUMERIC [ '(' <precision> [ ',' <scale> ] ')' ]
  | REAL | FLOAT [ '(' <precision> ')' ] | DOUBLE PRECISION
  | MONEY | SMALLMONEY
  | UINT8 | UINT16 | UINT32 | UINT64

<character_type> ::=
    CHAR [ '(' <length> ')' ] [ CHARACTER SET <charset_name> ]
  | VARCHAR '(' <length> ')' [ CHARACTER SET <charset_name> ]
  | TEXT [ '(' <length> ')' ]
  | NCHAR [ '(' <length> ')' ]
  | NVARCHAR '(' <length> ')'
  | CLOB [ '(' <length> ')' ]

<datetime_type> ::=
    DATE 
  | TIME [ '(' <precision> ')' ] [ WITH TIME ZONE ]
  | TIMESTAMP [ '(' <precision> ')' ] [ WITH TIME ZONE ]
  | INTERVAL <interval_fields>

<binary_type> ::=
    BINARY [ '(' <length> ')' ]
  | VARBINARY '(' <length> ')'
  | BLOB [ '(' <length> ')' ]
  | BYTEA

<array_type> ::=
    <data_type> '[' [ <array_bounds> ] ']'

<array_bounds> ::=
    <integer_literal> [ ':' <integer_literal> ]
    { ',' <integer_literal> [ ':' <integer_literal> ] }
```

### 2.5 Column Constraints

```ebnf
<column_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    <column_constraint_type>

<column_constraint_type> ::=
    NOT NULL
  | NULL
  | UNIQUE [ <index_parameters> ]
  | PRIMARY KEY [ <index_parameters> ]
  | REFERENCES <table_name> [ '(' <column_name> ')' ] 
    [ <referential_match> ] [ <referential_actions> ]
  | CHECK '(' <expression> ')'
  | DEFAULT <default_value>
  | GENERATED <generation_clause>
  | IDENTITY [ '(' <sequence_options> ')' ]
  | UUID [ <uuid_generation> ]

<generation_clause> ::=
    ALWAYS AS '(' <expression> ')' [ STORED | VIRTUAL ]
  | { ALWAYS | BY DEFAULT } AS IDENTITY [ '(' <sequence_options> ')' ]

<uuid_generation> ::=
    GENERATED ALWAYS AS UUID
  | GENERATED BY DEFAULT AS UUID
  | DEFAULT gen_uuid_v7()
```

### 2.6 Table Constraints

```ebnf
<table_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    <table_constraint_type>

<table_constraint_type> ::=
    PRIMARY KEY '(' <column_list> ')' [ <index_parameters> ]
  | UNIQUE '(' <column_list> ')' [ <index_parameters> ]
  | FOREIGN KEY '(' <column_list> ')' 
    REFERENCES <table_name> [ '(' <column_list> ')' ]
    [ <referential_match> ] [ <referential_actions> ]
  | CHECK '(' <expression> ')'
  | EXCLUDE USING <index_method> 
    '(' <exclude_element> WITH <operator> { ',' <exclude_element> WITH <operator> } ')'

<referential_match> ::=
    MATCH FULL | MATCH PARTIAL | MATCH SIMPLE

<referential_actions> ::=
    [ ON DELETE <referential_action> ]
    [ ON UPDATE <referential_action> ]

<referential_action> ::=
    CASCADE | SET NULL | SET DEFAULT | RESTRICT | NO ACTION
```

### 2.7 Index Operations

```ebnf
<create_index> ::=
    CREATE [ UNIQUE ] INDEX [ CONCURRENTLY ] [ IF NOT EXISTS ] <index_name>
    ON <table_name> [ USING <index_method> ]
    '(' <index_element> { ',' <index_element> } ')'
    [ INCLUDE '(' <column_list> ')' ]
    [ WHERE <predicate> ]
    [ <index_parameters> ]

<index_method> ::=
    BTREE | HASH | GIN | GIST | SPGIST | BRIN | BITMAP | LSM | RTREE

<index_element> ::=
    <column_name> [ <collation> ] [ <opclass> ] [ ASC | DESC ] [ NULLS { FIRST | LAST } ]
  | '(' <expression> ')' [ <collation> ] [ <opclass> ] [ ASC | DESC ] [ NULLS { FIRST | LAST } ]

<index_parameters> ::=
    [ TABLESPACE <tablespace_name> ]
    [ WITH '(' <storage_parameter> '=' <value> { ',' <storage_parameter> '=' <value> } ')' ]
    [ FILLFACTOR '=' <integer_literal> ]
```

### 2.8 Domain Operations

```ebnf
<create_domain> ::=
    CREATE DOMAIN <domain_name> [ AS ] <data_type>
    [ DEFAULT <default_value> ]
    [ <domain_constraint> { <domain_constraint> } ]

<domain_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    ( NOT NULL | NULL | CHECK '(' <expression> ')' )

<create_domain_extended> ::=
    CREATE DOMAIN <domain_name> AS (
        <record_domain>
      | <enum_domain>
      | <variant_domain>
      | <range_domain>
      | <composite_domain>
    )

<record_domain> ::=
    RECORD '(' 
        <field_name> <data_type> [ DEFAULT <default_value> ]
        { ',' <field_name> <data_type> [ DEFAULT <default_value> ] }
    ')'

<enum_domain> ::=
    ENUM '(' <string_literal> { ',' <string_literal> } ')'

<variant_domain> ::=
    VARIANT '(' 
        <variant_member> { ',' <variant_member> }
    ')'

<variant_member> ::=
    <member_name> [ '(' <data_type> ')' ]
```

## 3. DML - Data Manipulation Language

### 3.1 SELECT Statement

```ebnf
<select_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    <select_expression>
    [ <order_by_clause> ]
    [ <limit_clause> ]
    [ <for_clause> ]
    [ <into_clause> ]

<select_expression> ::=
    <select_term> { ( UNION | INTERSECT | EXCEPT ) [ ALL | DISTINCT ] <select_term> }

<select_term> ::=
    SELECT [ ALL | DISTINCT [ ON '(' <expression_list> ')' ] ]
    [ TOP <integer_literal> [ PERCENT ] [ WITH TIES ] ]
    <select_list>
    [ FROM <from_clause> ]
    [ WHERE <where_condition> ]
    [ GROUP BY <grouping_element_list> ]
    [ HAVING <having_condition> ]
    [ WINDOW <window_definition_list> ]

<select_list> ::=
    '*' 
  | <select_item> { ',' <select_item> }

<select_item> ::=
    <expression> [ [ AS ] <alias> ]
  | <table_name> '.' '*'
  | <qualifier> '.' '*'

<from_clause> ::=
    <table_reference> { ',' <table_reference> }

<table_reference> ::=
    <table_primary>
  | <joined_table>

<table_primary> ::=
    <table_name> [ [ AS ] <alias> [ '(' <column_alias_list> ')' ] ]
  | '(' <select_statement> ')' [ AS ] <alias> [ '(' <column_alias_list> ')' ]
  | <table_function> [ [ AS ] <alias> [ '(' <column_alias_list> ')' ] ]
  | UNNEST '(' <array_expression> ')' [ WITH ORDINALITY ] [ [ AS ] <alias> ]
  | VALUES <values_list> [ [ AS ] <alias> [ '(' <column_alias_list> ')' ] ]
  | <xmltable>
  | <jsontable>

<joined_table> ::=
    <table_reference> <join_type> <table_reference> <join_specification>

<join_type> ::=
    [ INNER ] JOIN
  | LEFT [ OUTER ] JOIN
  | RIGHT [ OUTER ] JOIN
  | FULL [ OUTER ] JOIN
  | CROSS JOIN
  | NATURAL [ <join_type> ] JOIN

<join_specification> ::=
    ON <condition>
  | USING '(' <column_list> ')'
```

### 3.2 INSERT Statement

```ebnf
<insert_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    INSERT INTO <table_name> [ AS <alias> ]
    [ '(' <column_list> ')' ]
    <insert_values_clause>
    [ ON DUPLICATE KEY UPDATE <update_assignment_list> ]
    [ ON CONFLICT <conflict_clause> ]
    [ RETURNING <output_clause> ]

<insert_values_clause> ::=
    VALUES <values_list>
  | <select_statement>
  | DEFAULT VALUES

<values_list> ::=
    <value_row> { ',' <value_row> }

<value_row> ::=
    '(' <expression_list> ')'
  | ROW '(' <expression_list> ')'

<conflict_clause> ::=
    '(' <conflict_target> ')' DO 
    ( NOTHING | UPDATE SET <update_assignment_list> [ WHERE <condition> ] )

<conflict_target> ::=
    <column_list>
  | ON CONSTRAINT <constraint_name>
```

### 3.3 UPDATE Statement

```ebnf
<update_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    UPDATE [ OR <conflict_algorithm> ] <table_reference>
    SET <update_assignment_list>
    [ FROM <from_clause> ]
    [ WHERE <where_condition> ]
    [ WHERE CURRENT OF <cursor_name> ]
    [ RETURNING <output_clause> ]

<update_assignment_list> ::=
    <update_assignment> { ',' <update_assignment> }

<update_assignment> ::=
    <column_name> '=' <expression>
  | '(' <column_list> ')' '=' '(' <expression_list> ')'
  | '(' <column_list> ')' '=' <select_statement>
```

### 3.4 DELETE Statement

```ebnf
<delete_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    DELETE FROM <table_reference>
    [ USING <table_reference_list> ]
    [ WHERE <where_condition> ]
    [ WHERE CURRENT OF <cursor_name> ]
    [ RETURNING <output_clause> ]
```

### 3.5 MERGE Statement

```ebnf
<merge_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    MERGE INTO <table_name> [ [ AS ] <alias> ]
    USING <table_reference> ON <merge_condition>
    <merge_when_clause> { <merge_when_clause> }

<merge_when_clause> ::=
    WHEN MATCHED [ AND <condition> ] THEN <merge_update_delete>
  | WHEN NOT MATCHED [ BY TARGET ] [ AND <condition> ] THEN <merge_insert>
  | WHEN NOT MATCHED BY SOURCE [ AND <condition> ] THEN <merge_update_delete>

<merge_update_delete> ::=
    UPDATE SET <update_assignment_list>
  | DELETE

<merge_insert> ::=
    INSERT [ '(' <column_list> ')' ] VALUES '(' <expression_list> ')'
```

### 3.6 SELECT INTO

```ebnf
<select_into_statement> ::=
    SELECT <select_list>
    INTO <into_target>
    FROM <from_clause>
    [ WHERE <where_condition> ]
    [ GROUP BY <grouping_element_list> ]
    [ HAVING <having_condition> ]

<into_target> ::=
    <variable_list>
  | [ STRICT ] <variable_list>
  | OUTFILE <string_literal> [ <export_options> ]
  | DUMPFILE <string_literal>
  | <new_table_name>

<variable_list> ::=
    <variable> { ',' <variable> }

<variable> ::=
    '@' <identifier>
  | ':' <identifier>
  | <identifier>
```

## 4. DCL - Data Control Language

### 4.1 User and Role Management

```ebnf
<create_user> ::=
    CREATE USER [ IF NOT EXISTS ] <user_name>
    [ IDENTIFIED BY <authentication_clause> ]
    [ DEFAULT ROLE <role_list> ]
    [ <user_options> ]

<create_role> ::=
    CREATE ROLE [ IF NOT EXISTS ] <role_name>
    [ WITH ADMIN <user_name> ]
    [ <role_options> ]

<authentication_clause> ::=
    PASSWORD <string_literal>
  | EXTERNAL
  | CERTIFICATE <certificate_name>
  | KERBEROS
  | LDAP
```

### 4.2 GRANT Statement

```ebnf
<grant_statement> ::=
    <grant_privilege>
  | <grant_role>

<grant_privilege> ::=
    GRANT <privilege_list> 
    ON <privilege_object>
    TO <grantee_list>
    [ WITH GRANT OPTION ]
    [ GRANTED BY <grantor> ]

<privilege_list> ::=
    ALL [ PRIVILEGES ]
  | <privilege> { ',' <privilege> }

<privilege> ::=
    SELECT [ '(' <column_list> ')' ]
  | INSERT [ '(' <column_list> ')' ]
  | UPDATE [ '(' <column_list> ')' ]
  | DELETE
  | TRUNCATE
  | REFERENCES [ '(' <column_list> ')' ]
  | TRIGGER
  | CREATE
  | CONNECT
  | TEMPORARY
  | EXECUTE
  | USAGE
  | INDEX
  | ALTER
  | DROP

<privilege_object> ::=
    [ TABLE ] <table_name>
  | DATABASE <database_name>
  | SCHEMA <schema_name>
  | FUNCTION <function_name> [ '(' <parameter_type_list> ')' ]
  | PROCEDURE <procedure_name> [ '(' <parameter_type_list> ')' ]
  | SEQUENCE <sequence_name>
  | DOMAIN <domain_name>
  | TYPE <type_name>
  | TABLESPACE <tablespace_name>
  | ALL TABLES IN SCHEMA <schema_name>
  | ALL SEQUENCES IN SCHEMA <schema_name>
  | ALL FUNCTIONS IN SCHEMA <schema_name>

<grant_role> ::=
    GRANT <role_list> TO <grantee_list>
    [ WITH ADMIN OPTION ]
    [ GRANTED BY <grantor> ]
```

### 4.3 REVOKE Statement

```ebnf
<revoke_statement> ::=
    <revoke_privilege>
  | <revoke_role>

<revoke_privilege> ::=
    REVOKE [ GRANT OPTION FOR ] <privilege_list>
    ON <privilege_object>
    FROM <grantee_list>
    [ GRANTED BY <grantor> ]
    [ CASCADE | RESTRICT ]

<revoke_role> ::=
    REVOKE [ ADMIN OPTION FOR ] <role_list>
    FROM <grantee_list>
    [ GRANTED BY <grantor> ]
    [ CASCADE | RESTRICT ]
```

## 5. TCL - Transaction Control Language

```ebnf
<tcl_statement> ::=
    <begin_transaction>
  | <commit_transaction>
  | <rollback_transaction>
  | <savepoint_statement>
  | <release_savepoint>
  | <set_transaction>
  | <set_constraints>

<begin_transaction> ::=
    BEGIN [ WORK | TRANSACTION ] [ <transaction_mode_list> ]
  | START TRANSACTION [ <transaction_mode_list> ]

<transaction_mode_list> ::=
    <transaction_mode> { ',' <transaction_mode> }

<transaction_mode> ::=
    ISOLATION LEVEL <isolation_level>
  | READ WRITE | READ ONLY
  | [ NOT ] DEFERRABLE
  | DIAGNOSTICS SIZE <integer_literal>

<isolation_level> ::=
    READ UNCOMMITTED
  | READ COMMITTED
  | REPEATABLE READ
  | SERIALIZABLE
  | SNAPSHOT

<commit_transaction> ::=
    COMMIT [ WORK | TRANSACTION ] [ AND [ NO ] CHAIN ]
  | END [ WORK | TRANSACTION ]

<rollback_transaction> ::=
    ROLLBACK [ WORK | TRANSACTION ] [ AND [ NO ] CHAIN ]
  | ROLLBACK [ WORK | TRANSACTION ] TO [ SAVEPOINT ] <savepoint_name>

<savepoint_statement> ::=
    SAVEPOINT <savepoint_name>

<release_savepoint> ::=
    RELEASE [ SAVEPOINT ] <savepoint_name>

<set_transaction> ::=
    SET TRANSACTION <transaction_mode_list>
  | SET SESSION CHARACTERISTICS AS TRANSACTION <transaction_mode_list>
```

## 6. PSQL - Procedural SQL

### 6.1 Stored Procedures and Functions

```ebnf
<create_procedure> ::=
    CREATE [ OR REPLACE ] PROCEDURE <procedure_name>
    '(' [ <parameter_list> ] ')'
    [ RETURNS <return_clause> ]
    [ <routine_characteristics> ]
    [ AS | IS ]
    <routine_body>

<create_function> ::=
    CREATE [ OR REPLACE ] FUNCTION <function_name>
    '(' [ <parameter_list> ] ')'
    RETURNS <return_type>
    [ <routine_characteristics> ]
    [ AS | IS ]
    <routine_body>

<parameter_list> ::=
    <parameter_declaration> { ',' <parameter_declaration> }

<parameter_declaration> ::=
    [ <parameter_mode> ] <parameter_name> <data_type> [ DEFAULT <default_value> ]

<parameter_mode> ::=
    IN | OUT | INOUT | IN OUT

<return_clause> ::=
    <data_type>
  | TABLE '(' <column_definition> { ',' <column_definition> } ')'
  | SETOF <data_type>
  | REFCURSOR

<routine_characteristics> ::=
    <routine_characteristic> { <routine_characteristic> }

<routine_characteristic> ::=
    LANGUAGE <language_name>
  | [ NOT ] DETERMINISTIC
  | { CONTAINS SQL | NO SQL | READS SQL DATA | MODIFIES SQL DATA }
  | SQL SECURITY { DEFINER | INVOKER }
  | PARALLEL { UNSAFE | RESTRICTED | SAFE }
  | COST <cost_value>
  | ROWS <rows_value>
  | AUTONOMOUS TRANSACTION
  | IMMUTABLE | STABLE | VOLATILE
```

### 6.2 Routine Body

```ebnf
<routine_body> ::=
    <sql_procedure_statement>
  | <external_body_reference>
  | <plsql_block>

<plsql_block> ::=
    [ DECLARE <declaration_list> ]
    BEGIN
        <statement_list>
    [ EXCEPTION <exception_handler_list> ]
    END [ <label> ]

<declaration_list> ::=
    <declaration> { <declaration> }

<declaration> ::=
    <variable_declaration>
  | <cursor_declaration>
  | <exception_declaration>
  | <type_declaration>
  | <subprogram_declaration>

<variable_declaration> ::=
    <variable_name> <data_type> [ [ NOT ] NULL ] [ DEFAULT <default_value> ] ';'
  | <variable_name> CONSTANT <data_type> [ NOT NULL ] ':=' <expression> ';'
  | <variable_name> <table_name> '%ROWTYPE' ';'
  | <variable_name> <column_reference> '%TYPE' ';'
```

### 6.3 Control Flow Statements

```ebnf
<control_flow_statement> ::=
    <if_statement>
  | <case_statement>
  | <loop_statement>
  | <while_statement>
  | <for_statement>
  | <repeat_statement>
  | <leave_statement>
  | <iterate_statement>
  | <return_statement>
  | <signal_statement>
  | <resignal_statement>
  | <call_statement>

<if_statement> ::=
    IF <condition> THEN
        <statement_list>
    { ELSIF <condition> THEN <statement_list> }
    [ ELSE <statement_list> ]
    END IF

<case_statement> ::=
    CASE [ <expression> ]
        { WHEN <when_value> THEN <statement_list> }
        [ ELSE <statement_list> ]
    END CASE

<loop_statement> ::=
    [ <label> ':' ] LOOP
        <statement_list>
    END LOOP [ <label> ]

<while_statement> ::=
    [ <label> ':' ] WHILE <condition> DO
        <statement_list>
    END WHILE [ <label> ]

<for_statement> ::=
    <for_loop>
  | <for_select>
  | <for_execute>

<for_loop> ::=
    [ <label> ':' ] FOR <loop_variable> IN [ REVERSE ] <range> DO
        <statement_list>
    END FOR [ <label> ]

<for_select> ::=
    [ <label> ':' ] FOR <record_variable> AS [ <cursor_name> CURSOR FOR ]
    <select_statement> DO
        <statement_list>
    END FOR [ <label> ]

<for_execute> ::=
    [ <label> ':' ] FOR <record_variable> IN EXECUTE <string_expression>
    [ USING <expression_list> ] DO
        <statement_list>
    END FOR [ <label> ]
```

### 6.4 Cursor Operations

```ebnf
<cursor_declaration> ::=
    DECLARE <cursor_name> [ [ NO ] SCROLL ] CURSOR 
    [ '(' <parameter_list> ')' ]
    FOR <select_statement>
    [ FOR { READ ONLY | UPDATE [ OF <column_list> ] } ]

<open_cursor> ::=
    OPEN <cursor_name> [ '(' <expression_list> ')' ]

<fetch_cursor> ::=
    FETCH [ <fetch_orientation> ] [ FROM ] <cursor_name>
    INTO <variable_list>

<fetch_orientation> ::=
    NEXT | PRIOR | FIRST | LAST
  | ABSOLUTE <integer_expression>
  | RELATIVE <integer_expression>
  | FORWARD [ <integer_expression> ]
  | BACKWARD [ <integer_expression> ]

<close_cursor> ::=
    CLOSE <cursor_name>

<where_current> ::=
    WHERE CURRENT OF <cursor_name>
```

### 6.5 Exception Handling

```ebnf
<exception_handler_list> ::=
    <exception_handler> { <exception_handler> }

<exception_handler> ::=
    WHEN <exception_condition> { OR <exception_condition> } THEN
        <statement_list>

<exception_condition> ::=
    <exception_name>
  | SQLSTATE [ VALUE ] <sqlstate_value>
  | SQLWARNING
  | NOT FOUND
  | SQLEXCEPTION
  | OTHERS

<raise_statement> ::=
    RAISE [ <exception_name> ] [ <raise_message> ]
  | RAISE EXCEPTION <string_expression> [ ',' <expression_list> ]
  | RAISE NOTICE <string_expression> [ ',' <expression_list> ]
  | RAISE WARNING <string_expression> [ ',' <expression_list> ]
  | RAISE INFO <string_expression> [ ',' <expression_list> ]
  | RAISE DEBUG <string_expression> [ ',' <expression_list> ]

<declare_exception> ::=
    DECLARE <exception_name> EXCEPTION [ FOR SQLSTATE <sqlstate_value> ]
```

## 7. EXECUTE BLOCK

```ebnf
<execute_block> ::=
    EXECUTE BLOCK [ '(' <parameter_list> ')' ]
    [ RETURNS '(' <output_parameter_list> ')' ]
    [ WITH AUTONOMOUS TRANSACTION ]
    [ ON <database_event> ]
    AS
    [ DECLARE <declaration_list> ]
    BEGIN
        <statement_list>
    [ EXCEPTION <exception_handler_list> ]
    END

<database_event> ::=
    CONNECT | DISCONNECT
  | TRANSACTION START | TRANSACTION COMMIT | TRANSACTION ROLLBACK
  | <trigger_event>

<output_parameter_list> ::=
    <output_parameter> { ',' <output_parameter> }

<output_parameter> ::=
    <parameter_name> <data_type>
```

## 8. Triggers

```ebnf
<create_trigger> ::=
    CREATE [ OR REPLACE ] TRIGGER <trigger_name>
    <trigger_action_time> <trigger_event>
    ON <table_name>
    [ REFERENCING <referencing_clause> ]
    [ FOR EACH { ROW | STATEMENT } ]
    [ WHEN '(' <condition> ')' ]
    [ POSITION <integer_literal> ]
    <trigger_body>

<trigger_action_time> ::=
    BEFORE | AFTER | INSTEAD OF

<trigger_event> ::=
    INSERT
  | UPDATE [ OF <column_list> ]
  | DELETE
  | INSERT OR UPDATE
  | INSERT OR DELETE
  | UPDATE OR DELETE
  | INSERT OR UPDATE OR DELETE

<referencing_clause> ::=
    { OLD [ ROW ] [ AS ] <identifier> }
    { NEW [ ROW ] [ AS ] <identifier> }
    { OLD TABLE [ AS ] <identifier> }
    { NEW TABLE [ AS ] <identifier> }

<trigger_body> ::=
    <plsql_block>
  | EXECUTE PROCEDURE <procedure_name> '(' [ <expression_list> ] ')'
  | AS <external_name>
```

## 9. Common Table Expressions (CTE)

```ebnf
<cte_list> ::=
    <cte> { ',' <cte> }

<cte> ::=
    <cte_name> [ '(' <column_list> ')' ] AS 
    [ [ NOT ] MATERIALIZED ]
    '(' <select_statement> ')'
    [ SEARCH <search_clause> ]
    [ CYCLE <cycle_clause> ]

<search_clause> ::=
    { DEPTH | BREADTH } FIRST BY <column_list> SET <column_name>

<cycle_clause> ::=
    <column_list> SET <column_name> [ TO <cycle_value> DEFAULT <non_cycle_value> ]
    [ USING <column_name> ]
```

## 10. Window Functions

```ebnf
<window_definition_list> ::=
    <window_definition> { ',' <window_definition> }

<window_definition> ::=
    <window_name> AS '(' <window_specification> ')'

<window_function> ::=
    <window_function_name> '(' [ <expression_list> ] ')' 
    [ FILTER '(' WHERE <condition> ')' ]
    OVER <window_name_or_specification>

<window_name_or_specification> ::=
    <window_name>
  | '(' <window_specification> ')'

<window_specification> ::=
    [ <window_name> ]
    [ PARTITION BY <expression_list> ]
    [ ORDER BY <sort_specification_list> ]
    [ <frame_clause> ]

<frame_clause> ::=
    { RANGE | ROWS | GROUPS } <frame_extent>
    [ <frame_exclusion> ]

<frame_extent> ::=
    <frame_start>
  | BETWEEN <frame_start> AND <frame_end>

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

<frame_exclusion> ::=
    EXCLUDE CURRENT ROW
  | EXCLUDE GROUP
  | EXCLUDE TIES
  | EXCLUDE NO OTHERS
```

## 11. Expressions

```ebnf
<expression> ::=
    <primary_expression>
  | <unary_expression>
  | <binary_expression>
  | <special_expression>

<primary_expression> ::=
    <literal>
  | <column_reference>
  | <variable_reference>
  | <function_call>
  | <aggregate_function>
  | <window_function>
  | <case_expression>
  | <cast_expression>
  | <array_expression>
  | <row_expression>
  | <subquery>
  | '(' <expression> ')'

<literal> ::=
    <numeric_literal>
  | <string_literal>
  | <date_literal>
  | <time_literal>
  | <timestamp_literal>
  | <interval_literal>
  | <boolean_literal>
  | <null_literal>
  | <uuid_literal>

<numeric_literal> ::=
    <integer_literal>
  | <decimal_literal>
  | <float_literal>

<integer_literal> ::=
    <decimal_integer_literal>
  | <hex_integer_literal>

<decimal_integer_literal> ::=
    <digit> { <digit> }

<hex_integer_literal> ::=
    '0x' <hex_digit> { <hex_digit> }
  | '0X' <hex_digit> { <hex_digit> }

<string_literal> ::=
    '\'' { <character> } '\''
  | '"' { <character> } '"'
  | '$$' { <character> } '$$'
  | N'\'' { <unicode_character> } '\''

<boolean_literal> ::=
    TRUE | FALSE | UNKNOWN

<null_literal> ::=
    NULL

<uuid_literal> ::=
    UUID '\'' <uuid_string> '\''
  | '\'' <uuid_string> '\'' '::' UUID

<hex_digit> ::=
    <digit>
  | 'a' | 'b' | 'c' | 'd' | 'e' | 'f'
  | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'
```

### 11.1 Operators

```ebnf
<binary_expression> ::=
    <expression> <binary_operator> <expression>

<binary_operator> ::=
    <arithmetic_operator>
  | <comparison_operator>
  | <logical_operator>
  | <string_operator>
  | <bitwise_operator>

<arithmetic_operator> ::=
    '+' | '-' | '*' | '/' | '%' | '^' | '**' | DIV

<comparison_operator> ::=
    '=' | '<>' | '!=' | '<' | '>' | '<=' | '>='
  | IS [ NOT ] NULL
  | IS [ NOT ] DISTINCT FROM
  | IS [ NOT ] TRUE | IS [ NOT ] FALSE | IS [ NOT ] UNKNOWN
  | [ NOT ] BETWEEN <expression> AND <expression>
  | [ NOT ] IN '(' <expression_list> ')'
  | [ NOT ] IN '(' <select_statement> ')'
  | [ NOT ] EXISTS '(' <select_statement> ')'
  | [ NOT ] LIKE <pattern> [ ESCAPE <escape_character> ]
  | [ NOT ] ILIKE <pattern> [ ESCAPE <escape_character> ]
  | [ NOT ] SIMILAR TO <pattern> [ ESCAPE <escape_character> ]
  | [ NOT ] STARTING WITH <expression>
  | [ NOT ] CONTAINING <expression>
  | [ NOT ] REGEXP <pattern>
  | <quantified_comparison>

<logical_operator> ::=
    AND | OR | NOT

<string_operator> ::=
    '||' | CONCAT

<bitwise_operator> ::=
    '&' | '|' | '#' | '~' | '<<' | '>>'

<quantified_comparison> ::=
    <comparison_operator> { ALL | ANY | SOME } '(' <select_statement> ')'
```

### 11.2 Special Expressions

```ebnf
<case_expression> ::=
    <simple_case>
  | <searched_case>

<simple_case> ::=
    CASE <expression>
        { WHEN <expression> THEN <expression> }
        [ ELSE <expression> ]
    END

<searched_case> ::=
    CASE
        { WHEN <condition> THEN <expression> }
        [ ELSE <expression> ]
    END

<cast_expression> ::=
    CAST '(' <expression> AS <data_type> [ USING <cast_format> ] ')'
  | <expression> '::' <data_type>
  | CONVERT '(' <expression> ',' <data_type> ')'
  | CONVERT '(' <expression> USING <conversion_name> ')'

<cast_format> ::=
    HEX | HEXADECIMAL | BASE64 | ESCAPE

<array_expression> ::=
    ARRAY '[' [ <expression_list> ] ']'
  | ARRAY '(' <select_statement> ')'
  | <expression> '[' <subscript> ']'
  | <expression> '[' <subscript> ':' <subscript> ']'

<row_expression> ::=
    ROW '(' <expression_list> ')'
  | '(' <expression_list> ')'

<set_expression> ::=
    SET '(' <expression_list> ')'
  | '(' <select_statement> ')' '::' SET
```

## 12. System Commands and Utilities

```ebnf
<system_command> ::=
    <show_command>
  | <describe_command>
  | <explain_command>
  | <analyze_command>
  | <sweep_command>
  | <sweep_status_command>
  | <vacuum_command>
  | <copy_command>
  | <set_command>
  | <reset_command>

<show_command> ::=
    SHOW <show_target>

<show_target> ::=
    DATABASES [ LIKE <pattern> ]
  | SCHEMAS [ LIKE <pattern> ]
  | TABLES [ FROM <schema_name> ] [ LIKE <pattern> ]
  | COLUMNS FROM <table_name>
  | INDEX FROM <table_name>
  | CREATE TABLE <table_name>
  | CREATE DATABASE <database_name>
  | PROCESSLIST
  | VARIABLES [ LIKE <pattern> ]
  | STATUS [ LIKE <pattern> ]
  | WARNINGS
  | ERRORS

<describe_command> ::=
    { DESCRIBE | DESC } <table_name> [ <column_name> ]

<explain_command> ::=
    EXPLAIN [ ANALYZE ] [ VERBOSE ] [ COSTS ] [ BUFFERS ] [ FORMAT { TEXT | XML | JSON | YAML } ]
    <statement>

<analyze_command> ::=
    ANALYZE [ VERBOSE ] [ <table_name> [ '(' <column_list> ')' ] ]

<sweep_command> ::=
    SWEEP [ VERBOSE ] [ ANALYZE ]

Note: SWEEP is the native ScratchBird/Firebird MGA maintenance command. VACUUM is a
PostgreSQL-compatibility alias that maps to sweep/GC semantics; VACUUM FULL is not
supported.

<sweep_status_command> ::=
    SWEEP STATUS [ VERBOSE ]

Note: SWEEP STATUS is a planned read-only monitoring statement. It returns sweep
statistics and transaction markers without performing a sweep.

<vacuum_command> ::=
    VACUUM [ FULL ] [ FREEZE ] [ VERBOSE ] [ ANALYZE ]
    [ <table_name> [ '(' <column_list> ')' ] ]

<copy_command> ::=
    COPY <table_name> [ '(' <column_list> ')' ]
    FROM { <filename> | STDIN }
    [ [ WITH ] '(' <copy_option_list> ')' ]
  | COPY { <table_name> [ '(' <column_list> ')' ] | '(' <select_statement> ')' }
    TO { <filename> | STDOUT }
    [ [ WITH ] '(' <copy_option_list> ')' ]

<copy_option_list> ::=
    <copy_option> { ',' <copy_option> }

<copy_option> ::=
    FORMAT { CSV | BINARY | TEXT }
  | DELIMITER <string_literal>
  | NULL <string_literal>
  | HEADER [ <boolean> ]
  | QUOTE <string_literal>
  | ESCAPE <string_literal>
  | ENCODING <string_literal>
```

## 13. SET Operations and Variables

```ebnf
<set_command> ::=
    SET [ SESSION | LOCAL ] <configuration_parameter> { TO | '=' } <value>
  | SET [ SESSION | LOCAL ] TIME ZONE <timezone_value>
  | SET [ SESSION | LOCAL ] ROLE <role_name>
  | SET [ SESSION | LOCAL ] SESSION AUTHORIZATION <user_name>
  | SET SCHEMA <schema_name>
  | SET SEARCH_PATH TO <schema_list>
  | SET CONSTRAINTS { ALL | <constraint_list> } { DEFERRED | IMMEDIATE }

<set_variable> ::=
    SET '@' <variable_name> '=' <expression>
  | SET '@' <variable_name> ':=' <expression>
  | SELECT <expression> INTO '@' <variable_name>
```

## 14. Advanced Features

### 14.1 Autonomous Transactions

```ebnf
<autonomous_transaction_block> ::=
    BEGIN AUTONOMOUS TRANSACTION
        <statement_list>
    { COMMIT | ROLLBACK }
    END

<autonomous_procedure> ::=
    CREATE PROCEDURE <procedure_name> '(' [ <parameter_list> ] ')'
    WITH AUTONOMOUS TRANSACTION
    AS
    <routine_body>
```

### 14.2 Temporal Tables

```ebnf
<temporal_table_definition> ::=
    CREATE TABLE <table_name> (
        <column_definition_list>,
        PERIOD FOR SYSTEM_TIME '(' <start_column> ',' <end_column> ')'
    )
    WITH SYSTEM VERSIONING

<temporal_query> ::=
    SELECT <select_list>
    FROM <table_name>
    FOR SYSTEM_TIME AS OF <timestamp_expression>
  | SELECT <select_list>
    FROM <table_name>
    FOR SYSTEM_TIME BETWEEN <timestamp_expression> AND <timestamp_expression>
  | SELECT <select_list>
    FROM <table_name>
    FOR SYSTEM_TIME FROM <timestamp_expression> TO <timestamp_expression>
  | SELECT <select_list>
    FROM <table_name>
    FOR SYSTEM_TIME ALL
```

### 14.3 Row-Level Security

```ebnf
<create_policy> ::=
    CREATE POLICY <policy_name> ON <table_name>
    [ AS { PERMISSIVE | RESTRICTIVE } ]
    [ FOR { ALL | SELECT | INSERT | UPDATE | DELETE } ]
    [ TO <role_list> ]
    [ USING '(' <expression> ')' ]
    [ WITH CHECK '(' <expression> ')' ]

<alter_table_rls> ::=
    ALTER TABLE <table_name> { ENABLE | DISABLE | FORCE | NO FORCE } ROW LEVEL SECURITY
```

### 14.4 Partitioning

```ebnf
<partition_by_clause> ::=
    PARTITION BY { RANGE | LIST | HASH } '(' <partition_column_list> ')'

<partition_spec> ::=
    PARTITION <partition_name> 
    { VALUES LESS THAN '(' <value_list> ')'
    | VALUES IN '(' <value_list> ')'
    | FOR VALUES FROM '(' <value_list> ')' TO '(' <value_list> ')'
    | FOR VALUES WITH '(' MODULUS <integer> ',' REMAINDER <integer> ')'
    }
    [ TABLESPACE <tablespace_name> ]
```

## 15. Comments

```ebnf
<comment> ::=
    <single_line_comment>
  | <multi_line_comment>
  | <doc_comment>

<single_line_comment> ::=
    '--' { <any_character_except_newline> } <newline>
  | '//' { <any_character_except_newline> } <newline>

<multi_line_comment> ::=
    '/*' { <any_character> } '*/'

<doc_comment> ::=
    '/**' { <any_character> } '*/'

<comment_statement> ::=
    COMMENT ON <object_type> <object_name> IS <string_literal>

<object_type> ::=
    TABLE | COLUMN | INDEX | SEQUENCE | VIEW | DATABASE | SCHEMA
  | FUNCTION | PROCEDURE | TRIGGER | TYPE | DOMAIN | CONSTRAINT
  | ROLE | TABLESPACE | EVENT | RULE | POLICY
```

## 16. Identifiers and Names

```ebnf
<identifier> ::=
    <regular_identifier>
  | <delimited_identifier>

<regular_identifier> ::=
    <identifier_start> { <identifier_part> }

<identifier_start> ::=
    <letter> | '_'

<identifier_part> ::=
    <letter> | <digit> | '_' | '$'

<delimited_identifier> ::=
    '"' { <any_character_except_quote> | '""' } '"'
  | '`' { <any_character_except_backtick> | '``' } '`'
  | '[' { <any_character_except_bracket> } ']'

<qualified_name> ::=
    [ <schema_name> '.' ] <object_name>
  | [ <database_name> '.' ] [ <schema_name> '.' ] <object_name>
```

## 17. Special ScratchBird Extensions

### 17.1 UUID Support

```ebnf
<uuid_functions> ::=
    gen_uuid_v1() | gen_uuid_v4() | gen_uuid_v7()
  | uuid_to_string '(' <expression> ')'
  | string_to_uuid '(' <expression> ')'

<uuid_column> ::=
    <column_name> UUID 
    [ DEFAULT gen_uuid_v7() ]
    [ GENERATED ALWAYS AS UUID ]
```

### 17.2 INT128 Support

```ebnf
<int128_literal> ::=
    <integer_literal> '::' INT128
  | INT128 '\'' <integer_string> '\''

<int128_operations> ::=
    <expression> <arithmetic_operator> <expression>
    WHERE <expression> IS OF TYPE INT128
```

### 17.3 Domain Extensions

```ebnf
<check_domain> ::=
    CHECK '(' VALUE <comparison_operator> <expression> ')'

<domain_cast> ::=
    <expression> '::' <domain_name>
  | CAST '(' <expression> AS <domain_name> ')'
```

### 17.4 Event System

```ebnf
<create_event> ::=
    CREATE EVENT [ IF NOT EXISTS ] <event_name>
    ON SCHEDULE <schedule>
    [ ON COMPLETION [ NOT ] PRESERVE ]
    [ ENABLE | DISABLE | DISABLE ON SLAVE ]
    [ COMMENT <string_literal> ]
    DO <event_body>

<schedule> ::=
    AT <timestamp_expression> [ '+' INTERVAL <interval_expression> ]
  | EVERY <interval_expression>
    [ STARTS <timestamp_expression> ]
    [ ENDS <timestamp_expression> ]
```

### 17.5 SUSPEND and RESUME

```ebnf
<suspend_statement> ::=
    SUSPEND [ <expression_list> ]

<resume_statement> ::=
    RESUME [ <label> ]

<for_select_suspend> ::=
    FOR <select_statement> DO
        <statement_list>
        [ SUSPEND [ <expression_list> ] ]
    END FOR
```

## 18. Operator Precedence

```
Highest to Lowest:
1.  ::                      (PostgreSQL-style cast)
2.  [ ]                     (array subscript)
3.  .                       (member selection)
4.  - + ~                   (unary operators)
5.  ^                       (exponentiation)
6.  * / % DIV               (multiplication, division, modulo)
7.  + -                     (addition, subtraction)
8.  << >> & | #             (bitwise operators)
9.  = <> < > <= >= !=       (comparison)
10. BETWEEN IN LIKE ILIKE SIMILAR TO STARTING WITH CONTAINING EXISTS
11. IS NULL, IS NOT NULL, IS TRUE, IS FALSE
12. NOT                     (logical NOT)
13. AND                     (logical AND)
14. OR                      (logical OR)
```

## 19. Reserved Words

ScratchBird uses context-aware parsing, so most keywords are only reserved in specific contexts. The following are always reserved:

```
AND, AS, ASC, BETWEEN, BY, CASE, CROSS, DESC, DISTINCT, ELSE, END, 
EXISTS, FALSE, FROM, GROUP, HAVING, IN, INNER, IS, JOIN, LEFT, 
LIKE, NOT, NULL, OR, ORDER, OUTER, RIGHT, SELECT, THEN, TRUE, 
UNION, WHEN, WHERE, WITH
```

## 20. Context-Aware Features

### 20.1 Automatic Statement Termination

The parser can detect statement boundaries without explicit semicolons when:
- A new statement keyword follows a complete statement
- The context makes the boundary unambiguous
- Interactive mode with single statements

### 20.2 Intelligent Identifier Resolution

Identifiers are resolved in the following order:
1. Column names in current scope
2. Variables and parameters
3. Table/view names
4. Schema objects
5. Built-in functions
6. User-defined functions

### 20.3 Dialect Compatibility

The parser automatically recognizes and translates:
- PostgreSQL: `::` casting, `$$` quoting, RETURNING clause
- MySQL: Backtick quoting, LIMIT syntax, SHOW commands
- MSSQL: Square bracket quoting, TOP clause, `@@` variables
- Oracle: `CONNECT BY`, `ROWNUM`, dual table
- Firebird: `EXECUTE BLOCK`, `SUSPEND`, position parameters

## Appendix A: Built-in Functions

Categories include:
- Aggregate functions (COUNT, SUM, AVG, MIN, MAX, etc.)
- Window functions (ROW_NUMBER, RANK, DENSE_RANK, etc.)
- String functions (CONCAT, SUBSTRING, TRIM, etc.)
- String functions (CONCAT, SUBSTRING, TRIM, REPLACE, ENDS_WITH, etc.)
- Date/Time functions (CURRENT_DATE, EXTRACT, DATE_ADD, DATE_DIFF, TO_CHAR, TO_DATE, TO_TIMESTAMP, etc.)
- Mathematical functions (ABS, ROUND, POWER, LEAST, GREATEST, etc.)
- UUID functions (gen_uuid_v7, uuid_to_string, etc.)
- JSON functions (JSON_EXTRACT, JSON_ARRAY, JSON_EXISTS, etc.)
- Array functions (ARRAY_AGG, UNNEST, ARRAY_POSITION, ARRAY_SLICE, etc.)
- System functions (CURRENT_USER, VERSION, etc.)

## Appendix B: System Tables and Views

Standard information schema plus ScratchBird extensions:
- INFORMATION_SCHEMA.*
- SYS_TABLES, SYS_COLUMNS, SYS_INDEXES
- SYS_USERS, SYS_ROLES, SYS_PRIVILEGES
- SYS_TRIGGERS, SYS_PROCEDURES, SYS_FUNCTIONS
- SYS_EVENTS, SYS_DOMAINS, SYS_SEQUENCES

---

This BNF/EBNF grammar represents the complete SQL dialect supported by ScratchBird, incorporating all features documented in the technical specifications including context-aware parsing, multi-dialect compatibility, and advanced features like autonomous transactions, temporal tables, and the event system.
