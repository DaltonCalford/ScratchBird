# PostgreSQL SQL Extensions BNF/EBNF Grammar

## Overview
This document provides a comprehensive BNF/EBNF grammar specification for PostgreSQL SQL extensions and dialect-specific features. PostgreSQL extends the SQL standard with numerous proprietary features that need to be properly parsed.

## Notation
- `::=` defines a production rule
- `|` indicates alternatives
- `[ ]` indicates optional elements
- `{ }` indicates zero or more repetitions
- `( )` groups elements
- `< >` denotes non-terminals
- Terminal symbols are in UPPERCASE or quoted

## Core Grammar Extensions

### 1. Data Types

```ebnf
<postgresql_type> ::=
    <standard_sql_type>
  | <array_type>
  | <composite_type>
  | <domain_type>
  | <enum_type>
  | <range_type>
  | <pseudo_type>
  | <geometric_type>
  | <network_type>
  | <text_search_type>
  | <uuid_type>
  | <json_type>
  | <xml_type>

<array_type> ::= <data_type> "[" [ <integer> ] "]" { "[" [ <integer> ] "]" }
                | <data_type> ARRAY [ "[" <integer> "]" ]

<composite_type> ::= <identifier>

<domain_type> ::= <identifier>

<enum_type> ::= <identifier>

<range_type> ::= INT4RANGE | INT8RANGE | NUMRANGE | TSRANGE 
                | TSTZRANGE | DATERANGE | <identifier>

<pseudo_type> ::= ANY | ANYELEMENT | ANYARRAY | ANYNONARRAY 
                | ANYENUM | ANYRANGE | CSTRING | INTERNAL 
                | LANGUAGE_HANDLER | FDW_HANDLER | RECORD 
                | TRIGGER | VOID | OPAQUE

<geometric_type> ::= POINT | LINE | LSEG | BOX | PATH | POLYGON | CIRCLE

<network_type> ::= CIDR | INET | MACADDR | MACADDR8

<text_search_type> ::= TSVECTOR | TSQUERY

<uuid_type> ::= UUID

<json_type> ::= JSON | JSONB

<xml_type> ::= XML
```

### 2. DDL Extensions

#### CREATE TABLE Extensions

```ebnf
<create_table_statement> ::=
    CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ] TABLE 
    [ IF NOT EXISTS ] <table_name> 
    [ PARTITION OF <parent_table> [ ( <partition_bound_spec> ) ] ]
    ( <table_element_list> )
    [ INHERITS ( <parent_table> { , <parent_table> } ) ]
    [ PARTITION BY { RANGE | LIST | HASH } ( <partition_key> ) ]
    [ WITH ( <storage_parameter_list> ) | WITH OIDS | WITHOUT OIDS ]
    [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
    [ TABLESPACE <tablespace_name> ]

<table_element> ::=
    <column_definition>
  | <table_constraint>
  | <like_clause>

<column_definition> ::=
    <column_name> <data_type> [ COLLATE <collation> ]
    [ <column_constraint> { <column_constraint> } ]

<column_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    { NOT NULL
    | NULL
    | CHECK ( <expression> ) [ NO INHERIT ]
    | DEFAULT <default_expr>
    | GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY [ ( <sequence_options> ) ]
    | GENERATED ALWAYS AS ( <expression> ) STORED
    | UNIQUE [ ( <column_list> ) ] <index_parameters>
    | PRIMARY KEY <index_parameters>
    | REFERENCES <reftable> [ ( <refcolumn> ) ] 
        [ MATCH { FULL | PARTIAL | SIMPLE } ]
        [ ON DELETE <referential_action> ] 
        [ ON UPDATE <referential_action> ]
    }
    [ DEFERRABLE | NOT DEFERRABLE ] 
    [ INITIALLY { DEFERRED | IMMEDIATE } ]

<like_clause> ::=
    LIKE <source_table> 
    [ { INCLUDING | EXCLUDING } 
      { COMMENTS | CONSTRAINTS | DEFAULTS | GENERATED | IDENTITY 
      | INDEXES | STATISTICS | STORAGE | ALL } { ... } ]

<partition_bound_spec> ::=
    FOR VALUES <partition_bound_values>
  | DEFAULT

<partition_bound_values> ::=
    IN ( <value_list> )
  | FROM ( <value_list> ) TO ( <value_list> )
  | WITH ( MODULUS <integer>, REMAINDER <integer> )
```

#### CREATE INDEX Extensions

```ebnf
<create_index_statement> ::=
    CREATE [ UNIQUE ] INDEX [ CONCURRENTLY ] [ [ IF NOT EXISTS ] <index_name> ]
    ON [ ONLY ] <table_name> [ USING <method> ]
    ( <index_element> { , <index_element> } )
    [ INCLUDE ( <column_name> { , <column_name> } ) ]
    [ WITH ( <storage_parameter_list> ) ]
    [ TABLESPACE <tablespace_name> ]
    [ WHERE <predicate> ]

<index_element> ::=
    { <column_name> | ( <expression> ) } 
    [ COLLATE <collation> ] 
    [ <opclass> ] 
    [ ASC | DESC ] 
    [ NULLS { FIRST | LAST } ]

<method> ::= BTREE | HASH | GIST | SPGIST | GIN | BRIN
```

### 3. DML Extensions

#### INSERT Extensions

```ebnf
<insert_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    INSERT INTO <table_name> [ AS <alias> ] 
    [ ( <column_list> ) ]
    [ OVERRIDING { SYSTEM | USER } VALUE ]
    { DEFAULT VALUES
    | VALUES ( <value_list> ) { , ( <value_list> ) }
    | <query> }
    [ ON CONFLICT <conflict_clause> ]
    [ RETURNING <output_list> ]

<conflict_clause> ::=
    [ ( <index_column_list> ) ] [ WHERE <condition> ]
    DO { NOTHING | UPDATE SET <update_item_list> [ WHERE <condition> ] }
```

#### UPDATE Extensions

```ebnf
<update_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    UPDATE [ ONLY ] <table_name> [ * ] [ [ AS ] <alias> ]
    SET <update_item_list>
    [ FROM <from_list> ]
    [ WHERE <condition> | WHERE CURRENT OF <cursor_name> ]
    [ RETURNING <output_list> ]

<update_item> ::=
    <column_name> = { <expression> | DEFAULT }
  | ( <column_list> ) = [ ROW ] ( { <expression> | DEFAULT } { , ... } )
  | ( <column_list> ) = ( <sub_select> )
```

#### DELETE Extensions

```ebnf
<delete_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    DELETE FROM [ ONLY ] <table_name> [ * ] [ [ AS ] <alias> ]
    [ USING <from_list> ]
    [ WHERE <condition> | WHERE CURRENT OF <cursor_name> ]
    [ RETURNING <output_list> ]
```

### 4. Query Extensions

#### SELECT Extensions

```ebnf
<select_statement> ::=
    [ WITH [ RECURSIVE ] <cte_list> ]
    SELECT [ ALL | DISTINCT [ ON ( <expression_list> ) ] ]
    [ * | <select_list> ]
    [ FROM <from_list> ]
    [ WHERE <condition> ]
    [ GROUP BY <grouping_element_list> ]
    [ HAVING <condition> ]
    [ WINDOW <window_definition_list> ]
    [ { UNION | INTERSECT | EXCEPT } [ ALL | DISTINCT ] <select> ]
    [ ORDER BY <expression> [ ASC | DESC ] [ NULLS { FIRST | LAST } ] { , ... } ]
    [ LIMIT { <count> | ALL } ]
    [ OFFSET <start> [ ROW | ROWS ] ]
    [ FETCH { FIRST | NEXT } [ <count> ] { ROW | ROWS } { ONLY | WITH TIES } ]
    [ FOR { UPDATE | NO KEY UPDATE | SHARE | KEY SHARE } 
        [ OF <table_name> { , ... } ] [ NOWAIT | SKIP LOCKED ] { ... } ]

<grouping_element> ::=
    <expression>
  | ( )
  | ( <expression> { , <expression> } )
  | ROLLUP ( { <expression> | ( <expression_list> ) } { , ... } )
  | CUBE ( { <expression> | ( <expression_list> ) } { , ... } )
  | GROUPING SETS ( <grouping_element> { , ... } )

<from_item> ::=
    [ ONLY ] <table_name> [ * ] [ [ AS ] <alias> [ ( <column_alias_list> ) ] ]
        [ TABLESAMPLE <sampling_method> ( <argument> { , ... } ) 
            [ REPEATABLE ( <seed> ) ] ]
  | [ LATERAL ] ( <query> ) [ AS ] <alias> [ ( <column_alias_list> ) ]
  | [ LATERAL ] <function_name> ( [ <argument_list> ] ) 
        [ WITH ORDINALITY ] [ [ AS ] <alias> [ ( <column_alias_list> ) ] ]
  | [ LATERAL ] ROWS FROM( <function_name> ( [ <argument_list> ] ) 
        [ AS ( <column_definition_list> ) ] { , ... } )
        [ WITH ORDINALITY ] [ [ AS ] <alias> [ ( <column_alias_list> ) ] ]
  | <from_item> <join_type> <from_item> <join_condition>

<join_type> ::=
    [ INNER ] JOIN
  | LEFT [ OUTER ] JOIN
  | RIGHT [ OUTER ] JOIN
  | FULL [ OUTER ] JOIN
  | CROSS JOIN

<join_condition> ::=
    ON <condition>
  | USING ( <column_list> )
  | NATURAL
```

### 5. CTE and Window Functions

```ebnf
<cte> ::=
    <cte_name> [ ( <column_list> ) ] AS [ [ NOT ] MATERIALIZED ] ( <query> )

<window_definition> ::=
    <window_name> AS ( <window_specification> )

<window_specification> ::=
    [ <existing_window_name> ]
    [ PARTITION BY <expression_list> ]
    [ ORDER BY <sort_specification_list> ]
    [ <frame_clause> ]

<frame_clause> ::=
    { RANGE | ROWS | GROUPS } <frame_start> [ <frame_exclusion> ]
  | { RANGE | ROWS | GROUPS } BETWEEN <frame_start> AND <frame_end> 
        [ <frame_exclusion> ]

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

### 6. Procedural Language Extensions

#### PL/pgSQL

```ebnf
<plpgsql_function> ::=
    CREATE [ OR REPLACE ] FUNCTION <function_name> 
    ( [ <parameter_list> ] )
    RETURNS <return_type>
    [ LANGUAGE PLPGSQL ]
    [ TRANSFORM { FOR TYPE <type_name> } { , ... } ]
    [ WINDOW ] [ IMMUTABLE | STABLE | VOLATILE | [ NOT ] LEAKPROOF ]
    [ CALLED ON NULL INPUT | RETURNS NULL ON NULL INPUT | STRICT ]
    [ [ EXTERNAL ] SECURITY { INVOKER | DEFINER } ]
    [ PARALLEL { UNSAFE | RESTRICTED | SAFE } ]
    [ COST <execution_cost> ]
    [ ROWS <result_rows> ]
    [ SET <configuration_parameter> { TO <value> | = <value> | FROM CURRENT } ]
    AS <function_body>

<function_body> ::=
    $$ <plpgsql_block> $$
  | ' <plpgsql_block> '

<plpgsql_block> ::=
    [ DECLARE <declarations> ]
    BEGIN
        <statements>
    [ EXCEPTION <exception_handlers> ]
    END [ <label> ] ;

<plpgsql_statement> ::=
    <assignment>
  | <if_statement>
  | <case_statement>
  | <loop_statement>
  | <exit_statement>
  | <continue_statement>
  | <return_statement>
  | <raise_statement>
  | <assert_statement>
  | <execute_statement>
  | <sql_statement>
  | <block>

<assignment> ::=
    <variable> := <expression> ;
  | <variable> = <expression> ;

<if_statement> ::=
    IF <condition> THEN
        <statements>
    [ ELSIF <condition> THEN
        <statements> ]
    [ ELSE
        <statements> ]
    END IF ;

<loop_statement> ::=
    [ <<label>> ]
    { LOOP
    | WHILE <condition> LOOP
    | FOR <name> IN [ REVERSE ] <expression> .. <expression> [ BY <expression> ] LOOP
    | FOR <target> IN <query> LOOP
    | FOR <target> IN EXECUTE <text_expression> [ USING <expression_list> ] LOOP
    | FOREACH <target> [ SLICE <number> ] IN ARRAY <expression> LOOP }
        <statements>
    END LOOP [ <label> ] ;

<return_statement> ::=
    RETURN [ <expression> ] ;
  | RETURN NEXT <expression> ;
  | RETURN QUERY <query> ;
  | RETURN QUERY EXECUTE <text_expression> [ USING <expression_list> ] ;
```

### 7. Advanced Features

#### Arrays

```ebnf
<array_expression> ::=
    ARRAY[ <value_list> ]
  | ARRAY( <query> )
  | <expression> [ <subscript> ] { [ <subscript> ] }

<subscript> ::=
    <expression>
  | <expression> : <expression>

<array_operator> ::=
    = | <> | < | > | <= | >= | @> | <@ | && | ||
```

#### JSON/JSONB Operations

```ebnf
<json_expression> ::=
    <expression> -> <key>           -- Get JSON object field
  | <expression> -> <index>         -- Get JSON array element
  | <expression> ->> <key>          -- Get JSON object field as text
  | <expression> ->> <index>        -- Get JSON array element as text
  | <expression> #> <path>          -- Get JSON object at path
  | <expression> #>> <path>         -- Get JSON object at path as text
  | <expression> @> <expression>    -- Contains
  | <expression> <@ <expression>    -- Is contained by
  | <expression> ? <key>            -- Key exists
  | <expression> ?| <keys>          -- Any key exists
  | <expression> ?& <keys>          -- All keys exist
  | <expression> || <expression>    -- Concatenate

<json_path> ::= '{' <path_element> { , <path_element> } '}'

<json_aggregate> ::=
    JSON_AGG( <expression> [ ORDER BY <sort_specification_list> ] )
  | JSONB_AGG( <expression> [ ORDER BY <sort_specification_list> ] )
  | JSON_OBJECT_AGG( <key>, <value> )
  | JSONB_OBJECT_AGG( <key>, <value> )
```

#### Full Text Search

```ebnf
<text_search_expression> ::=
    <tsvector> @@ <tsquery>                    -- Match
  | <tsquery> @@ <tsvector>                    -- Match (commutative)
  | <tsvector> || <tsvector>                   -- Concatenate
  | TS_RANK( <tsvector>, <tsquery> )          -- Ranking
  | TS_RANK_CD( <tsvector>, <tsquery> )       -- Cover density ranking
  | TS_HEADLINE( <text>, <tsquery> )          -- Generate headline
  | TO_TSVECTOR( [ <config>, ] <text> )       -- Convert to tsvector
  | TO_TSQUERY( [ <config>, ] <text> )        -- Convert to tsquery
  | PLAINTO_TSQUERY( [ <config>, ] <text> )   -- Plain to tsquery
  | PHRASETO_TSQUERY( [ <config>, ] <text> )  -- Phrase to tsquery

<tsquery_operator> ::=
    & | | | ! | <-> | <N>
```

#### COPY Statement

```ebnf
<copy_statement> ::=
    COPY <table_name> [ ( <column_list> ) ]
    FROM { '<filename>' | PROGRAM '<command>' | STDIN }
    [ [ WITH ] ( <option> { , <option> } ) ]
    [ WHERE <condition> ]

  | COPY { <table_name> [ ( <column_list> ) ] | ( <query> ) }
    TO { '<filename>' | PROGRAM '<command>' | STDOUT }
    [ [ WITH ] ( <option> { , <option> } ) ]

<copy_option> ::=
    FORMAT { TEXT | CSV | BINARY }
  | FREEZE [ <boolean> ]
  | DELIMITER '<delimiter_character>'
  | NULL '<null_string>'
  | HEADER [ <boolean> ]
  | QUOTE '<quote_character>'
  | ESCAPE '<escape_character>'
  | FORCE_QUOTE { ( <column_list> ) | * }
  | FORCE_NOT_NULL ( <column_list> )
  | FORCE_NULL ( <column_list> )
  | ENCODING '<encoding_name>'
```

### 8. System Catalog and Information Schema

```ebnf
<system_catalog_query> ::=
    SELECT * FROM pg_catalog.<catalog_table>
  | SELECT * FROM information_schema.<schema_view>

<catalog_table> ::=
    pg_class | pg_attribute | pg_index | pg_constraint
  | pg_namespace | pg_type | pg_proc | pg_operator
  | pg_database | pg_tablespace | pg_roles | pg_stat_activity
  | { ... many more ... }
```

### 9. Transaction Control Extensions

```ebnf
<transaction_statement> ::=
    BEGIN [ WORK | TRANSACTION ] [ <transaction_mode> { , ... } ]
  | START TRANSACTION [ <transaction_mode> { , ... } ]
  | COMMIT [ WORK | TRANSACTION ] [ AND [ NO ] CHAIN ]
  | ROLLBACK [ WORK | TRANSACTION ] [ AND [ NO ] CHAIN ]
  | SAVEPOINT <savepoint_name>
  | RELEASE [ SAVEPOINT ] <savepoint_name>
  | ROLLBACK [ WORK | TRANSACTION ] TO [ SAVEPOINT ] <savepoint_name>
  | PREPARE TRANSACTION <transaction_id>
  | COMMIT PREPARED <transaction_id>
  | ROLLBACK PREPARED <transaction_id>

<transaction_mode> ::=
    ISOLATION LEVEL { SERIALIZABLE | REPEATABLE READ | READ COMMITTED 
                    | READ UNCOMMITTED }
  | READ WRITE | READ ONLY
  | [ NOT ] DEFERRABLE
```

### 10. PostgreSQL-Specific Operators

```ebnf
<postgresql_operator> ::=
    -- Arithmetic
    + | - | * | / | % | ^ | |/ | ||/ | ! | !! | @ | & | | | # | ~ | << | >>

    -- Comparison
    = | <> | != | < | > | <= | >= | <=> | IS DISTINCT FROM | IS NOT DISTINCT FROM

    -- String
    || | ~~ | ~~* | !~~ | !~~* | ~ | ~* | !~ | !~*

    -- Geometric
    @-@ | @@ | ## | <-> | && | &< | &> | <<| | |>> | &<| | |&> | <^ | >^

    -- Network
    << | <<= | >> | >>= | && | ~ | & | | | + | -

    -- Text Search
    @@ | || | && | !! | @> | <@

    -- JSON/JSONB
    -> | ->> | #> | #>> | @> | <@ | ? | ?| | ?& | || | - | #-

    -- Array
    @> | <@ | && | || | # | + | -

    -- Range
    @> | <@ | && | << | >> | &< | &> | -|- | + | * | -
```

### 11. Schema Management

```ebnf
<schema_statement> ::=
    CREATE SCHEMA [ IF NOT EXISTS ] <schema_name> 
        [ AUTHORIZATION <role_spec> ] [ <schema_element> { ... } ]
  | ALTER SCHEMA <schema_name> { RENAME TO <new_name> | OWNER TO <role_spec> }
  | DROP SCHEMA [ IF EXISTS ] <schema_name> { , ... } [ CASCADE | RESTRICT ]

<set_schema> ::=
    SET search_path TO <schema> { , <schema> }
  | SET search_path = <schema> { , <schema> }
```

### 12. Extensions and Foreign Data Wrappers

```ebnf
<extension_statement> ::=
    CREATE EXTENSION [ IF NOT EXISTS ] <extension_name>
        [ WITH ] [ SCHEMA <schema_name> ]
        [ VERSION <version> ]
        [ CASCADE ]
  | ALTER EXTENSION <extension_name> { UPDATE [ TO <version> ] 
                                     | SET SCHEMA <schema_name> }
  | DROP EXTENSION [ IF EXISTS ] <extension_name> { , ... } [ CASCADE | RESTRICT ]

<foreign_data_wrapper> ::=
    CREATE FOREIGN DATA WRAPPER <fdw_name>
        [ HANDLER <handler_function> ]
        [ VALIDATOR <validator_function> ]
        [ OPTIONS ( <option_list> ) ]

<foreign_server> ::=
    CREATE SERVER [ IF NOT EXISTS ] <server_name>
        [ TYPE '<server_type>' ] [ VERSION '<server_version>' ]
        FOREIGN DATA WRAPPER <fdw_name>
        [ OPTIONS ( <option_list> ) ]

<foreign_table> ::=
    CREATE FOREIGN TABLE [ IF NOT EXISTS ] <table_name> 
        ( <column_definition_list> )
        [ INHERITS ( <parent_table> { , ... } ) ]
        SERVER <server_name>
        [ OPTIONS ( <option_list> ) ]
```

### 13. Event Triggers and Rules

```ebnf
<event_trigger> ::=
    CREATE EVENT TRIGGER <trigger_name>
        ON <event>
        [ WHEN <filter_variable> IN ( <filter_value> { , ... } ) [ AND ... ] ]
        EXECUTE { FUNCTION | PROCEDURE } <function_name>()

<event> ::= DDL_COMMAND_START | DDL_COMMAND_END | TABLE_REWRITE | SQL_DROP

<rule_statement> ::=
    CREATE [ OR REPLACE ] RULE <rule_name> AS
        ON { SELECT | INSERT | UPDATE | DELETE }
        TO <table_name> [ WHERE <condition> ]
        DO [ ALSO | INSTEAD ] { NOTHING | <command> | ( <command_list> ) }
```

### 14. Tablespaces and Storage

```ebnf
<tablespace_statement> ::=
    CREATE TABLESPACE <tablespace_name>
        [ OWNER <role_name> ]
        LOCATION '<directory>'
        [ WITH ( <tablespace_option_list> ) ]

<storage_parameter> ::=
    fillfactor = <value>
  | toast_tuple_target = <value>
  | parallel_workers = <value>
  | autovacuum_enabled = <boolean>
  | autovacuum_vacuum_threshold = <value>
  | autovacuum_vacuum_scale_factor = <value>
  | { ... many more ... }
```

### 15. LISTEN/NOTIFY

```ebnf
<listen_statement> ::= LISTEN <channel_name>
<unlisten_statement> ::= UNLISTEN { <channel_name> | * }
<notify_statement> ::= NOTIFY <channel_name> [ , '<payload>' ]
```

### 16. Advisory Locks

```ebnf
<advisory_lock_function> ::=
    pg_advisory_lock( <key> )
  | pg_advisory_lock( <key1>, <key2> )
  | pg_advisory_lock_shared( <key> )
  | pg_advisory_lock_shared( <key1>, <key2> )
  | pg_try_advisory_lock( <key> )
  | pg_try_advisory_lock( <key1>, <key2> )
  | pg_try_advisory_lock_shared( <key> )
  | pg_try_advisory_lock_shared( <key1>, <key2> )
  | pg_advisory_unlock( <key> )
  | pg_advisory_unlock( <key1>, <key2> )
  | pg_advisory_unlock_shared( <key> )
  | pg_advisory_unlock_shared( <key1>, <key2> )
  | pg_advisory_unlock_all()
```

### 17. Row Level Security

```ebnf
<row_security> ::=
    ALTER TABLE <table_name> ENABLE ROW LEVEL SECURITY
  | ALTER TABLE <table_name> DISABLE ROW LEVEL SECURITY
  | ALTER TABLE <table_name> FORCE ROW LEVEL SECURITY
  | ALTER TABLE <table_name> NO FORCE ROW LEVEL SECURITY

<policy_statement> ::=
    CREATE POLICY <policy_name> ON <table_name>
        [ AS { PERMISSIVE | RESTRICTIVE } ]
        [ FOR { ALL | SELECT | INSERT | UPDATE | DELETE } ]
        [ TO { <role_name> | PUBLIC | CURRENT_USER | SESSION_USER } { , ... } ]
        [ USING ( <using_expression> ) ]
        [ WITH CHECK ( <check_expression> ) ]
```

### 18. Partitioning

```ebnf
<partition_statement> ::=
    CREATE TABLE <partition_name> PARTITION OF <parent_table>
        { FOR VALUES <partition_bound_spec> | DEFAULT }
        [ PARTITION BY { RANGE | LIST | HASH } ( <partition_key> ) ]
        [ <table_storage_parameters> ]

<attach_partition> ::=
    ALTER TABLE <parent_table> ATTACH PARTITION <partition_name>
        { FOR VALUES <partition_bound_spec> | DEFAULT }

<detach_partition> ::=
    ALTER TABLE <parent_table> DETACH PARTITION <partition_name>
        [ CONCURRENTLY | FINALIZE ]
```

### 19. MERGE Statement (PostgreSQL 15+)

```ebnf
<merge_statement> ::=
    [ WITH <cte_list> ]
    MERGE INTO <target_table> [ [ AS ] <target_alias> ]
    USING <data_source> ON <join_condition>
    <when_clause> { <when_clause> }

<when_clause> ::=
    WHEN MATCHED [ AND <condition> ] THEN <merge_update>
  | WHEN NOT MATCHED [ AND <condition> ] THEN <merge_insert>
  | WHEN MATCHED [ AND <condition> ] THEN DELETE

<merge_update> ::=
    UPDATE SET { <column_name> = { <expression> | DEFAULT } } { , ... }

<merge_insert> ::=
    INSERT [ ( <column_list> ) ] 
    { VALUES ( { <expression> | DEFAULT } { , ... } ) | DEFAULT VALUES }
```

### 20. SQL/JSON Path Language (PostgreSQL 12+)

```ebnf
<jsonpath> ::=
    <mode> <path_expression>

<mode> ::= strict | lax

<path_expression> ::=
    $<accessor_expression>

<accessor_expression> ::=
    .<key>
  | .<*>
  | [<index>]
  | [*]
  | [<start> to <end>]
  | ?(<filter_expression>)

<jsonpath_operator> ::=
    == | != | <> | < | <= | > | >= | =~
  | starts with | like_regex | exists

<jsonpath_function> ::=
    .type() | .size() | .double() | .ceiling() | .floor()
  | .abs() | .datetime() | .keyvalue()
```

## Usage Notes

1. **Parser Implementation**: When implementing a parser based on this grammar:
   - Handle PostgreSQL's dollar-quoted strings: `$tag$...$tag$`
   - Support E'' escape strings
   - Implement proper operator precedence
   - Handle schema-qualified names: `schema.object`

2. **Version Considerations**: 
   - This grammar covers PostgreSQL 9.5+ features
   - Some features (MERGE, SQL/JSON) require PostgreSQL 12+
   - Always check PostgreSQL version for feature availability

3. **Extensions**: PostgreSQL's extension system means additional syntax may be available depending on installed extensions (PostGIS, pg_trgm, etc.)

4. **Case Sensitivity**: 
   - Unquoted identifiers are folded to lowercase
   - Quoted identifiers preserve case: "MyTable"

5. **Comments**:
   - Single line: `-- comment`
   - Multi-line: `/* comment */`

6. **String Literals**:
   - Standard: `'string'`
   - Escape: `E'string\n'`
   - Dollar: `$$string$$` or `$tag$string$tag$`
   - Unicode: `U&'string'`

7. **Numeric Literals**:
   - Integer: `123`, `-456`
   - Decimal: `123.45`, `-0.001`
   - Scientific: `1.23E+4`, `5e-10`
   - Binary: `B'101010'`
   - Hexadecimal: `X'DEADBEEF'`

8. **Type Casting**:
   - PostgreSQL style: `expression::type`
   - SQL standard: `CAST(expression AS type)`
   - Function style: `type(expression)`

This grammar provides a comprehensive foundation for implementing a PostgreSQL SQL parser, covering the major extensions and PostgreSQL-specific features beyond standard SQL.