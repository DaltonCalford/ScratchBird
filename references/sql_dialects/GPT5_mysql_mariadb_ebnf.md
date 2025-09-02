# MySQL/MariaDB SQL Variations BNF/EBNF Grammar

## Overview
This document provides a comprehensive BNF/EBNF grammar specification for MySQL and MariaDB SQL variations and dialect-specific features. While MySQL and MariaDB share a common heritage, MariaDB has diverged with additional features. This grammar covers both, with annotations for version-specific features.

## Notation
- `::=` defines a production rule
- `|` indicates alternatives
- `[ ]` indicates optional elements
- `{ }` indicates zero or more repetitions
- `( )` groups elements
- `< >` denotes non-terminals
- Terminal symbols are in UPPERCASE or quoted
- `/*MySQL*/` indicates MySQL-specific features
- `/*MariaDB*/` indicates MariaDB-specific features

## Core Grammar Extensions

### 1. Data Types

```ebnf
<mysql_data_type> ::=
    <numeric_type>
  | <string_type>
  | <date_time_type>
  | <json_type>
  | <spatial_type>
  | <binary_type>

<numeric_type> ::=
    BIT [ ( <length> ) ]
  | TINYINT [ ( <display_width> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | SMALLINT [ ( <display_width> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | MEDIUMINT [ ( <display_width> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | INT [ ( <display_width> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | INTEGER [ ( <display_width> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | BIGINT [ ( <display_width> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | DECIMAL [ ( <precision> [ , <scale> ] ) ] [ UNSIGNED ] [ ZEROFILL ]
  | DEC [ ( <precision> [ , <scale> ] ) ] [ UNSIGNED ] [ ZEROFILL ]
  | NUMERIC [ ( <precision> [ , <scale> ] ) ] [ UNSIGNED ] [ ZEROFILL ]
  | FIXED [ ( <precision> [ , <scale> ] ) ] [ UNSIGNED ] [ ZEROFILL ]
  | FLOAT [ ( <precision> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | DOUBLE [ PRECISION ] [ ( <precision> , <scale> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | REAL [ ( <precision> , <scale> ) ] [ UNSIGNED ] [ ZEROFILL ]
  | BOOL | BOOLEAN

<string_type> ::=
    CHAR [ ( <length> ) ] [ BINARY ] [ <charset_clause> ] [ <collate_clause> ]
  | VARCHAR ( <length> ) [ BINARY ] [ <charset_clause> ] [ <collate_clause> ]
  | BINARY [ ( <length> ) ]
  | VARBINARY ( <length> )
  | TINYBLOB | TINYTEXT [ <charset_clause> ] [ <collate_clause> ]
  | BLOB [ ( <length> ) ] | TEXT [ ( <length> ) ] [ <charset_clause> ] [ <collate_clause> ]
  | MEDIUMBLOB | MEDIUMTEXT [ <charset_clause> ] [ <collate_clause> ]
  | LONGBLOB | LONGTEXT [ <charset_clause> ] [ <collate_clause> ]
  | ENUM ( <value_list> ) [ <charset_clause> ] [ <collate_clause> ]
  | SET ( <value_list> ) [ <charset_clause> ] [ <collate_clause> ]

<date_time_type> ::=
    DATE
  | TIME [ ( <fsp> ) ]
  | DATETIME [ ( <fsp> ) ]
  | TIMESTAMP [ ( <fsp> ) ]
  | YEAR [ ( 4 ) ]

<json_type> ::= JSON  /*MySQL 5.7+, MariaDB 10.2+*/

<spatial_type> ::=
    GEOMETRY
  | POINT
  | LINESTRING
  | POLYGON
  | MULTIPOINT
  | MULTILINESTRING
  | MULTIPOLYGON
  | GEOMETRYCOLLECTION

<charset_clause> ::= CHARACTER SET <charset_name>
<collate_clause> ::= COLLATE <collation_name>
```

### 2. DDL Extensions

#### CREATE TABLE Extensions

```ebnf
<create_table_statement> ::=
    CREATE [ TEMPORARY ] TABLE [ IF NOT EXISTS ] <table_name>
    { ( <create_definition_list> ) [ <table_options> ] [ <partition_options> ]
    | LIKE <old_table_name>
    | ( LIKE <old_table_name> )
    | [ AS ] <select_statement> }

<create_definition> ::=
    <column_definition>
  | <index_definition>
  | <constraint_definition>
  | <check_constraint>  /*MySQL 8.0.16+, MariaDB 10.2+*/
  | <period_definition>  /*MariaDB 10.3+*/

<column_definition> ::=
    <column_name> <column_type> [ <column_attribute> { <column_attribute> } ]

<column_attribute> ::=
    NOT NULL | NULL
  | DEFAULT { <literal> | ( <expression> ) | <function_call> }
  | VISIBLE | INVISIBLE  /*MySQL 8.0.23+, MariaDB 10.3+*/
  | AUTO_INCREMENT
  | UNIQUE [ KEY ]
  | [ PRIMARY ] KEY
  | COMMENT '<string>'
  | COLLATE <collation_name>
  | COLUMN_FORMAT { FIXED | DYNAMIC | DEFAULT }
  | STORAGE { DISK | MEMORY | DEFAULT }
  | <reference_definition>
  | CHECK ( <expression> )  /*MySQL 8.0.16+, MariaDB 10.2+*/
  | GENERATED ALWAYS AS ( <expression> ) { VIRTUAL | STORED | PERSISTENT }
    /*MySQL 5.7+, MariaDB 5.2+ (PERSISTENT is MariaDB)*/
  | ON UPDATE <current_timestamp_function>
  | COMPRESSED [ = { ZLIB | LZ4 | LZO | SNAPPY } ]  /*MariaDB 10.1+*/
  | WITH SYSTEM VERSIONING  /*MariaDB 10.3+*/

<index_definition> ::=
    { INDEX | KEY } [ <index_name> ] [ <index_type> ] ( <key_part_list> ) [ <index_option> { <index_option> } ]
  | FULLTEXT [ INDEX | KEY ] [ <index_name> ] ( <key_part_list> ) [ <index_option> { <index_option> } ]
  | SPATIAL [ INDEX | KEY ] [ <index_name> ] ( <key_part_list> ) [ <index_option> { <index_option> } ]
  | [ CONSTRAINT [ <symbol> ] ] PRIMARY KEY [ <index_type> ] ( <key_part_list> ) [ <index_option> { <index_option> } ]
  | [ CONSTRAINT [ <symbol> ] ] UNIQUE [ INDEX | KEY ] [ <index_name> ] [ <index_type> ] ( <key_part_list> ) [ <index_option> { <index_option> } ]
  | [ CONSTRAINT [ <symbol> ] ] FOREIGN KEY [ <index_name> ] ( <key_part_list> ) <reference_definition>

<key_part> ::=
    <column_name> [ ( <length> ) ] [ ASC | DESC ]
  | ( <expression> ) [ ASC | DESC ]  /*MySQL 8.0+*/

<index_type> ::= USING { BTREE | HASH | RTREE }

<index_option> ::=
    KEY_BLOCK_SIZE [ = ] <value>
  | <index_type>
  | WITH PARSER <parser_name>
  | COMMENT '<string>'
  | VISIBLE | INVISIBLE  /*MySQL 8.0+*/
  | ENGINE_ATTRIBUTE [ = ] '<string>'  /*MySQL 8.0.21+*/
  | SECONDARY_ENGINE_ATTRIBUTE [ = ] '<string>'  /*MySQL 8.0.21+*/
  | CLUSTERING [ = ] { YES | NO }  /*MariaDB 10.3+*/
  | IGNORED  /*MariaDB 10.6+*/

<reference_definition> ::=
    REFERENCES <table_name> ( <key_part_list> )
    [ MATCH { FULL | PARTIAL | SIMPLE } ]
    [ ON DELETE <reference_option> ]
    [ ON UPDATE <reference_option> ]

<reference_option> ::=
    RESTRICT | CASCADE | SET NULL | NO ACTION | SET DEFAULT

<table_options> ::=
    <table_option> [ [ , ] <table_option> ] ...

<table_option> ::=
    [ DEFAULT ] { CHARACTER SET | CHARSET } [ = ] <charset_name>
  | [ DEFAULT ] COLLATE [ = ] <collation_name>
  | ENGINE [ = ] <engine_name>
  | AUTO_INCREMENT [ = ] <value>
  | AVG_ROW_LENGTH [ = ] <value>
  | CHECKSUM [ = ] { 0 | 1 }
  | COMMENT [ = ] '<string>'
  | COMPRESSION [ = ] { 'ZLIB' | 'LZ4' | 'NONE' }
  | CONNECTION [ = ] '<connect_string>'
  | DATA DIRECTORY [ = ] '<absolute_path>'
  | INDEX DIRECTORY [ = ] '<absolute_path>'
  | DELAY_KEY_WRITE [ = ] { 0 | 1 }
  | ENCRYPTION [ = ] { 'Y' | 'N' }  /*MySQL 5.7.11+, MariaDB 10.1+*/
  | INSERT_METHOD [ = ] { NO | FIRST | LAST }
  | KEY_BLOCK_SIZE [ = ] <value>
  | MAX_ROWS [ = ] <value>
  | MIN_ROWS [ = ] <value>
  | PACK_KEYS [ = ] { 0 | 1 | DEFAULT }
  | PASSWORD [ = ] '<string>'
  | ROW_FORMAT [ = ] { DEFAULT | DYNAMIC | FIXED | COMPRESSED | REDUNDANT | COMPACT }
  | PAGE_COMPRESSED [ = ] { 0 | 1 }  /*MariaDB 10.1+*/
  | PAGE_COMPRESSION_LEVEL [ = ] <value>  /*MariaDB 10.1+*/
  | SEQUENCE [ = ] { 0 | 1 }  /*MariaDB 10.3+*/
  | STATS_AUTO_RECALC [ = ] { DEFAULT | 0 | 1 }
  | STATS_PERSISTENT [ = ] { DEFAULT | 0 | 1 }
  | STATS_SAMPLE_PAGES [ = ] <value>
  | TABLESPACE <tablespace_name> [ STORAGE { DISK | MEMORY } ]
  | UNION [ = ] ( <table_list> )
  | WITH SYSTEM VERSIONING  /*MariaDB 10.3+*/
```

#### CREATE INDEX Extensions

```ebnf
<create_index_statement> ::=
    CREATE [ UNIQUE | FULLTEXT | SPATIAL ] INDEX <index_name>
    [ <index_type> ]
    ON <table_name> ( <key_part_list> )
    [ <index_option> { <index_option> } ]
    [ <algorithm_option> ] [ <lock_option> ]

<algorithm_option> ::= ALGORITHM [ = ] { DEFAULT | INPLACE | COPY | INSTANT }
    /*INSTANT added in MySQL 8.0.12, MariaDB 10.3.7*/

<lock_option> ::= LOCK [ = ] { DEFAULT | NONE | SHARED | EXCLUSIVE }
```

### 3. DML Extensions

#### INSERT Extensions

```ebnf
<insert_statement> ::=
    INSERT [ LOW_PRIORITY | DELAYED | HIGH_PRIORITY ] [ IGNORE ]
    [ INTO ] <table_name>
    [ PARTITION ( <partition_list> ) ]
    [ ( <column_list> ) ]
    { VALUES <value_list> { , <value_list> }
    | VALUE <value_list> { , <value_list> }
    | <select_statement>
    | SET <assignment_list>
    | TABLE <table_name>  /*MySQL 8.0.19+*/ }
    [ ON DUPLICATE KEY UPDATE <assignment_list> ]
    [ RETURNING <select_expr_list> ]  /*MariaDB 10.5+*/

<assignment> ::=
    <column_name> = { <expression> | VALUES ( <column_name> ) | DEFAULT }

<value_list> ::=
    ( { <expression> | DEFAULT } { , { <expression> | DEFAULT } } )
  | ROW ( { <expression> | DEFAULT } { , { <expression> | DEFAULT } } )
```

#### UPDATE Extensions

```ebnf
<update_statement> ::=
    UPDATE [ LOW_PRIORITY ] [ IGNORE ] <table_reference_list>
    SET <assignment_list>
    [ WHERE <where_condition> ]
    [ ORDER BY <order_list> ]
    [ LIMIT <row_count> ]
    [ RETURNING <select_expr_list> ]  /*MariaDB 10.5+*/

<table_reference> ::=
    <table_name> [ [ AS ] <alias> ]
    [ USE INDEX ( <index_list> ) ]
    [ IGNORE INDEX ( <index_list> ) ]
    [ FORCE INDEX ( <index_list> ) ]
```

#### DELETE Extensions

```ebnf
<delete_statement> ::=
    DELETE [ LOW_PRIORITY ] [ QUICK ] [ IGNORE ]
    { FROM <table_name> [ [ AS ] <alias> ]
        [ PARTITION ( <partition_list> ) ]
        [ WHERE <where_condition> ]
        [ ORDER BY <order_list> ]
        [ LIMIT <row_count> ]
        [ RETURNING <select_expr_list> ]  /*MariaDB 10.5+*/
    | <table_list> FROM <table_reference_list>
        [ WHERE <where_condition> ]
    | FROM <table_list> USING <table_reference_list>
        [ WHERE <where_condition> ] }
```

#### REPLACE Statement

```ebnf
<replace_statement> ::=
    REPLACE [ LOW_PRIORITY | DELAYED ]
    [ INTO ] <table_name>
    [ PARTITION ( <partition_list> ) ]
    [ ( <column_list> ) ]
    { VALUES <value_list> { , <value_list> }
    | VALUE <value_list> { , <value_list> }
    | <select_statement>
    | SET <assignment_list>
    | TABLE <table_name> }  /*MySQL 8.0.19+*/
```

### 4. Query Extensions

#### SELECT Extensions

```ebnf
<select_statement> ::=
    [ <with_clause> ]  /*MySQL 8.0+, MariaDB 10.2.1+*/
    SELECT
    [ ALL | DISTINCT | DISTINCTROW ]
    [ HIGH_PRIORITY ]
    [ STRAIGHT_JOIN ]
    [ SQL_SMALL_RESULT ] [ SQL_BIG_RESULT ] [ SQL_BUFFER_RESULT ]
    [ SQL_NO_CACHE ] [ SQL_CALC_FOUND_ROWS ]  /*Deprecated in MySQL 8.0.17*/
    <select_expr_list>
    [ FROM <table_reference_list>
        [ WHERE <where_condition> ]
        [ GROUP BY <group_by_list> [ WITH ROLLUP ] ]
        [ HAVING <having_condition> ]
        [ WINDOW <window_definition_list> ] ]  /*MySQL 8.0+, MariaDB 10.2+*/
    [ ORDER BY <order_list> ]
    [ LIMIT { <row_count> | <offset> , <row_count> | <row_count> OFFSET <offset> } ]
    [ { FOR UPDATE | LOCK IN SHARE MODE } [ OF <table_list> ] [ NOWAIT | SKIP LOCKED ] ]
        /*NOWAIT/SKIP LOCKED: MySQL 8.0+, MariaDB 10.3+*/
    [ FOR SHARE [ OF <table_list> ] [ NOWAIT | SKIP LOCKED ] ]  /*MySQL 8.0+*/
    [ INTO { OUTFILE '<filename>' [ <export_options> ] 
           | DUMPFILE '<filename>'
           | @<var_name> { , @<var_name> } } ]

<select_expr> ::=
    *
  | <table_name> . *
  | <expression> [ [ AS ] <alias> ]

<table_reference> ::=
    <table_factor>
  | <join_table>

<table_factor> ::=
    <table_name> [ [ AS ] <alias> ] [ <index_hint_list> ]
  | ( <select_statement> ) [ AS ] <alias> [ ( <column_list> ) ]
  | ( <table_reference_list> )
  | { OJ <table_reference> LEFT OUTER JOIN <table_reference> ON <condition> }
  | VALUES <value_list> { , <value_list> } [ AS ] <alias> [ ( <column_list> ) ]
    /*VALUES as table: MySQL 8.0.19+*/
  | TABLE <table_name> [ ORDER BY <column_name> ] [ LIMIT <number> ]
    /*TABLE statement: MySQL 8.0.19+*/

<join_table> ::=
    <table_reference> [ INNER | CROSS ] JOIN <table_factor> [ <join_condition> ]
  | <table_reference> STRAIGHT_JOIN <table_factor> [ ON <condition> ]
  | <table_reference> { LEFT | RIGHT } [ OUTER ] JOIN <table_reference> <join_condition>
  | <table_reference> NATURAL [ { LEFT | RIGHT } [ OUTER ] ] JOIN <table_factor>

<join_condition> ::=
    ON <condition>
  | USING ( <column_list> )

<index_hint_list> ::=
    <index_hint> { , <index_hint> }

<index_hint> ::=
    { USE | IGNORE | FORCE } { INDEX | KEY } [ FOR { JOIN | ORDER BY | GROUP BY } ] ( <index_list> )
```

#### Common Table Expressions (CTE)

```ebnf
<with_clause> ::=
    WITH [ RECURSIVE ] <cte_list>

<cte> ::=
    <cte_name> [ ( <column_list> ) ] AS ( <select_statement> )
```

#### Window Functions

```ebnf
<window_function> ::=
    <window_function_name> ( [ <expression_list> ] ) OVER <window_spec>

<window_spec> ::=
    ( [ <partition_clause> ] [ <order_clause> ] [ <frame_clause> ] )
  | <window_name>

<partition_clause> ::= PARTITION BY <expression_list>

<order_clause> ::= ORDER BY <order_item_list>

<frame_clause> ::=
    { ROWS | RANGE } <frame_start>
  | { ROWS | RANGE } BETWEEN <frame_start> AND <frame_end>

<frame_start> ::=
    CURRENT ROW
  | UNBOUNDED PRECEDING
  | <expression> PRECEDING
  | <expression> FOLLOWING

<frame_end> ::=
    CURRENT ROW
  | UNBOUNDED FOLLOWING
  | <expression> PRECEDING
  | <expression> FOLLOWING

<window_function_name> ::=
    ROW_NUMBER | RANK | DENSE_RANK | PERCENT_RANK | CUME_DIST
  | NTILE | LAG | LEAD | FIRST_VALUE | LAST_VALUE | NTH_VALUE
```

### 5. Stored Programs

#### Stored Procedures

```ebnf
<create_procedure> ::=
    CREATE [ DEFINER = { <user> | CURRENT_USER } ]
    PROCEDURE [ IF NOT EXISTS ] <sp_name> ( [ <parameter_list> ] )
    [ <characteristic> { <characteristic> } ]
    <routine_body>

<parameter> ::=
    [ IN | OUT | INOUT ] <param_name> <data_type>

<characteristic> ::=
    COMMENT '<string>'
  | LANGUAGE SQL
  | [ NOT ] DETERMINISTIC
  | { CONTAINS SQL | NO SQL | READS SQL DATA | MODIFIES SQL DATA }
  | SQL SECURITY { DEFINER | INVOKER }

<routine_body> ::=
    <sql_statement>
  | <compound_statement>

<compound_statement> ::=
    [ <label> : ] BEGIN
        [ <declaration_list> ]
        <statement_list>
    END [ <label> ]

<declaration> ::=
    DECLARE <variable_list> <data_type> [ DEFAULT <expression> ]
  | DECLARE <condition_name> CONDITION FOR <condition_value>
  | DECLARE <cursor_name> CURSOR FOR <select_statement>
  | DECLARE { CONTINUE | EXIT | UNDO } HANDLER FOR <condition_value_list> <statement>

<statement> ::=
    <sql_statement>
  | <compound_statement>
  | <if_statement>
  | <case_statement>
  | <loop_statement>
  | <while_statement>
  | <repeat_statement>
  | <leave_statement>
  | <iterate_statement>
  | <return_statement>
  | <signal_statement>
  | <resignal_statement>
  | <get_diagnostics>
  | <open_cursor>
  | <fetch_cursor>
  | <close_cursor>

<if_statement> ::=
    IF <condition> THEN <statement_list>
    [ ELSEIF <condition> THEN <statement_list> ]
    [ ELSE <statement_list> ]
    END IF

<case_statement> ::=
    CASE <case_value>
        WHEN <when_value> THEN <statement_list>
        [ WHEN <when_value> THEN <statement_list> ]
        [ ELSE <statement_list> ]
    END CASE
  | CASE
        WHEN <condition> THEN <statement_list>
        [ WHEN <condition> THEN <statement_list> ]
        [ ELSE <statement_list> ]
    END CASE

<loop_statement> ::=
    [ <label> : ] LOOP
        <statement_list>
    END LOOP [ <label> ]

<while_statement> ::=
    [ <label> : ] WHILE <condition> DO
        <statement_list>
    END WHILE [ <label> ]

<repeat_statement> ::=
    [ <label> : ] REPEAT
        <statement_list>
    UNTIL <condition>
    END REPEAT [ <label> ]
```

#### Stored Functions

```ebnf
<create_function> ::=
    CREATE [ DEFINER = { <user> | CURRENT_USER } ]
    FUNCTION [ IF NOT EXISTS ] <sp_name> ( [ <parameter_list> ] )
    RETURNS <data_type>
    [ <characteristic> { <characteristic> } ]
    <routine_body>
```

#### Triggers

```ebnf
<create_trigger> ::=
    CREATE [ DEFINER = { <user> | CURRENT_USER } ]
    TRIGGER [ IF NOT EXISTS ] <trigger_name>
    { BEFORE | AFTER }
    { INSERT | UPDATE | DELETE }
    ON <table_name> FOR EACH ROW
    [ { FOLLOWS | PRECEDES } <other_trigger_name> ]  /*MySQL 5.7.2+, MariaDB 10.2.3+*/
    <trigger_body>

<trigger_body> ::=
    <statement>
  | BEGIN <statement_list> END
```

#### Events

```ebnf
<create_event> ::=
    CREATE [ DEFINER = { <user> | CURRENT_USER } ]
    EVENT [ IF NOT EXISTS ] <event_name>
    ON SCHEDULE <schedule>
    [ ON COMPLETION [ NOT ] PRESERVE ]
    [ ENABLE | DISABLE | DISABLE ON SLAVE ]
    [ COMMENT '<string>' ]
    DO <event_body>

<schedule> ::=
    AT <timestamp> [ + INTERVAL <interval> ]
  | EVERY <interval>
    [ STARTS <timestamp> [ + INTERVAL <interval> ] ]
    [ ENDS <timestamp> [ + INTERVAL <interval> ] ]

<interval> ::=
    <quantity> { MICROSECOND | SECOND | MINUTE | HOUR 
               | DAY | WEEK | MONTH | QUARTER | YEAR
               | SECOND_MICROSECOND | MINUTE_MICROSECOND
               | MINUTE_SECOND | HOUR_MICROSECOND
               | HOUR_SECOND | HOUR_MINUTE
               | DAY_MICROSECOND | DAY_SECOND
               | DAY_MINUTE | DAY_HOUR | YEAR_MONTH }
```

### 6. User-Defined Functions (UDF)

```ebnf
<create_udf> ::=
    CREATE [ AGGREGATE ] FUNCTION <function_name>
    RETURNS { STRING | INTEGER | REAL | DECIMAL }
    SONAME '<shared_library_name>'

<drop_udf> ::=
    DROP FUNCTION [ IF EXISTS ] <function_name>
```

### 7. Views

```ebnf
<create_view> ::=
    CREATE [ OR REPLACE ]
    [ ALGORITHM = { UNDEFINED | MERGE | TEMPTABLE } ]
    [ DEFINER = { <user> | CURRENT_USER } ]
    [ SQL SECURITY { DEFINER | INVOKER } ]
    VIEW <view_name> [ ( <column_list> ) ]
    AS <select_statement>
    [ WITH [ CASCADED | LOCAL ] CHECK OPTION ]
```

### 8. Prepared Statements

```ebnf
<prepare_statement> ::=
    PREPARE <stmt_name> FROM <preparable_stmt>

<execute_statement> ::=
    EXECUTE <stmt_name> [ USING @<var_name> { , @<var_name> } ]

<deallocate_statement> ::=
    { DEALLOCATE | DROP } PREPARE <stmt_name>
```

### 9. Transaction Control

```ebnf
<transaction_statement> ::=
    START TRANSACTION [ <transaction_characteristic> { , <transaction_characteristic> } ]
  | BEGIN [ WORK ]
  | COMMIT [ WORK ] [ AND [ NO ] CHAIN ] [ [ NO ] RELEASE ]
  | ROLLBACK [ WORK ] [ AND [ NO ] CHAIN ] [ [ NO ] RELEASE ]
  | SAVEPOINT <savepoint_name>
  | RELEASE SAVEPOINT <savepoint_name>
  | ROLLBACK [ WORK ] TO [ SAVEPOINT ] <savepoint_name>
  | SET TRANSACTION <transaction_characteristic> { , <transaction_characteristic> }

<transaction_characteristic> ::=
    { WITH CONSISTENT SNAPSHOT
    | READ WRITE | READ ONLY
    | ISOLATION LEVEL <isolation_level> }

<isolation_level> ::=
    READ UNCOMMITTED | READ COMMITTED | REPEATABLE READ | SERIALIZABLE

<xa_transaction> ::=
    XA { START | BEGIN } <xid> [ JOIN | RESUME ]
  | XA END <xid> [ SUSPEND [ FOR MIGRATE ] ]
  | XA PREPARE <xid>
  | XA COMMIT <xid> [ ONE PHASE ]
  | XA ROLLBACK <xid>
  | XA RECOVER [ CONVERT XID ]

<xid> ::= '<gtrid>' [ , '<bqual>' [ , <format_id> ] ]
```

### 10. Lock Statements

```ebnf
<lock_tables> ::=
    LOCK TABLES <table_lock_list>

<table_lock> ::=
    <table_name> [ [ AS ] <alias> ] <lock_type>

<lock_type> ::=
    READ [ LOCAL ]
  | [ LOW_PRIORITY ] WRITE
  | WRITE CONCURRENT  /*MariaDB*/

<unlock_tables> ::= UNLOCK TABLES

<lock_instance> ::=
    LOCK INSTANCE FOR BACKUP  /*MySQL 8.0+*/
  | UNLOCK INSTANCE
```

### 11. Handler Statements

```ebnf
<handler_statement> ::=
    HANDLER <table_name> OPEN [ [ AS ] <alias> ]
  | HANDLER <table_name> READ <index_name> { = | <= | >= | < | > } ( <value_list> )
      [ WHERE <where_condition> ] [ LIMIT <limit> ]
  | HANDLER <table_name> READ <index_name> { FIRST | NEXT | PREV | LAST }
      [ WHERE <where_condition> ] [ LIMIT <limit> ]
  | HANDLER <table_name> READ { FIRST | NEXT }
      [ WHERE <where_condition> ] [ LIMIT <limit> ]
  | HANDLER <table_name> CLOSE
```

### 12. JSON Functions and Operators

```ebnf
<json_function> ::=
    JSON_ARRAY( [ <value> { , <value> } ] )
  | JSON_OBJECT( [ <key> , <value> { , <key> , <value> } ] )
  | JSON_QUOTE( <string> )
  | JSON_CONTAINS( <json_doc> , <candidate> [ , <path> ] )
  | JSON_CONTAINS_PATH( <json_doc> , <one_or_all> , <path> { , <path> } )
  | JSON_EXTRACT( <json_doc> , <path> { , <path> } )
  | JSON_KEYS( <json_doc> [ , <path> ] )
  | JSON_SEARCH( <json_doc> , <one_or_all> , <search_str> [ , <escape_char> [ , <path> ] ] )
  | JSON_VALUE( <json_doc> , <path> [ RETURNING <type> ] [ <on_behavior> ] )  /*MySQL 8.0.21+*/
  | JSON_SET( <json_doc> , <path> , <value> { , <path> , <value> } )
  | JSON_INSERT( <json_doc> , <path> , <value> { , <path> , <value> } )
  | JSON_REPLACE( <json_doc> , <path> , <value> { , <path> , <value> } )
  | JSON_REMOVE( <json_doc> , <path> { , <path> } )
  | JSON_MERGE( <json_doc> , <json_doc> { , <json_doc> } )  /*Deprecated*/
  | JSON_MERGE_PRESERVE( <json_doc> , <json_doc> { , <json_doc> } )  /*MySQL 5.7.22+*/
  | JSON_MERGE_PATCH( <json_doc> , <json_doc> { , <json_doc> } )  /*MySQL 5.7.22+*/
  | JSON_ARRAYAGG( <expression> [ ORDER BY <order_list> ] )  /*MySQL 5.7.22+*/
  | JSON_OBJECTAGG( <key> , <value> )  /*MySQL 5.7.22+*/
  | JSON_TABLE( <json_expr> , <path> COLUMNS ( <column_definition_list> ) )  /*MySQL 8.0+*/

<json_operator> ::=
    -> |    -- Extract JSON value
    ->> |   -- Extract JSON value and unquote
    @>      -- JSON contains (MariaDB 10.2.3+)

<json_path> ::=
    '$' [ <path_leg> { <path_leg> } ]

<path_leg> ::=
    . <member>
  | [ <index> ]
  | .* | [*]
```

### 13. Full-Text Search

```ebnf
<fulltext_search> ::=
    MATCH ( <column_list> ) AGAINST ( <search_expr> [ <search_modifier> ] )

<search_modifier> ::=
    IN NATURAL LANGUAGE MODE
  | IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION
  | IN BOOLEAN MODE
  | WITH QUERY EXPANSION
```

### 14. Regular Expressions

```ebnf
<regexp_operator> ::=
    REGEXP | RLIKE       -- Pattern matching
  | NOT REGEXP | NOT RLIKE

<regexp_function> ::=
    REGEXP_INSTR( <expr> , <pattern> [ , <position> [ , <occurrence> [ , <return_option> [ , <match_type> ] ] ] ] )
  | REGEXP_LIKE( <expr> , <pattern> [ , <match_type> ] )  /*MySQL 8.0.4+*/
  | REGEXP_REPLACE( <expr> , <pattern> , <replacement> [ , <position> [ , <occurrence> [ , <match_type> ] ] ] )
  | REGEXP_SUBSTR( <expr> , <pattern> [ , <position> [ , <occurrence> [ , <match_type> ] ] ] )

<match_type> ::= '<flags>'  -- Combination of: c i m n s u x
```

### 15. Partitioning

```ebnf
<partition_options> ::=
    PARTITION BY
    { [ LINEAR ] HASH ( <expression> )
    | [ LINEAR ] KEY [ ALGORITHM = { 1 | 2 } ] ( <column_list> )
    | RANGE ( <expression> | COLUMNS ( <column_list> ) )
    | LIST ( <expression> | COLUMNS ( <column_list> ) )
    | SYSTEM_TIME [ <system_time_partition> ]  /*MariaDB 10.3+*/ }
    [ PARTITIONS <number> ]
    [ SUBPARTITION BY
        { [ LINEAR ] HASH ( <expression> )
        | [ LINEAR ] KEY [ ALGORITHM = { 1 | 2 } ] ( <column_list> ) }
        [ SUBPARTITIONS <number> ] ]
    [ ( <partition_definition> { , <partition_definition> } ) ]

<partition_definition> ::=
    PARTITION <partition_name>
    [ VALUES { LESS THAN { ( <value_list> ) | MAXVALUE }
             | IN ( <value_list> ) } ]
    [ [ STORAGE ] ENGINE [ = ] <engine_name> ]
    [ COMMENT [ = ] '<string>' ]
    [ DATA DIRECTORY [ = ] '<path>' ]
    [ INDEX DIRECTORY [ = ] '<path>' ]
    [ MAX_ROWS [ = ] <number> ]
    [ MIN_ROWS [ = ] <number> ]
    [ TABLESPACE [ = ] <tablespace_name> ]
    [ ( <subpartition_definition> { , <subpartition_definition> } ) ]

<alter_partition> ::=
    ADD PARTITION ( <partition_definition> { , <partition_definition> } )
  | DROP PARTITION <partition_names>
  | DISCARD PARTITION { <partition_names> | ALL } TABLESPACE
  | IMPORT PARTITION { <partition_names> | ALL } TABLESPACE
  | TRUNCATE PARTITION { <partition_names> | ALL }
  | COALESCE PARTITION <number>
  | REORGANIZE PARTITION <partition_names> INTO ( <partition_definition> { , <partition_definition> } )
  | EXCHANGE PARTITION <partition_name> WITH TABLE <table_name> [ { WITH | WITHOUT } VALIDATION ]
  | ANALYZE PARTITION { <partition_names> | ALL }
  | CHECK PARTITION { <partition_names> | ALL }
  | OPTIMIZE PARTITION { <partition_names> | ALL }
  | REBUILD PARTITION { <partition_names> | ALL }
  | REPAIR PARTITION { <partition_names> | ALL }
  | REMOVE PARTITIONING
```

### 16. LOAD DATA and SELECT INTO

```ebnf
<load_data> ::=
    LOAD DATA [ LOW_PRIORITY | CONCURRENT ] [ LOCAL ] INFILE '<filename>'
    [ REPLACE | IGNORE ]
    INTO TABLE <table_name>
    [ PARTITION ( <partition_list> ) ]
    [ CHARACTER SET <charset_name> ]
    [ { FIELDS | COLUMNS }
        [ TERMINATED BY '<string>' ]
        [ [ OPTIONALLY ] ENCLOSED BY '<char>' ]
        [ ESCAPED BY '<char>' ] ]
    [ LINES
        [ STARTING BY '<string>' ]
        [ TERMINATED BY '<string>' ] ]
    [ IGNORE <number> { LINES | ROWS } ]
    [ ( <column_or_user_var> { , <column_or_user_var> } ) ]
    [ SET <assignment_list> ]

<load_xml> ::=
    LOAD XML [ LOW_PRIORITY | CONCURRENT ] [ LOCAL ] INFILE '<filename>'
    [ REPLACE | IGNORE ]
    INTO TABLE <table_name>
    [ CHARACTER SET <charset_name> ]
    [ ROWS IDENTIFIED BY '<tagname>' ]
    [ IGNORE <number> { LINES | ROWS } ]
    [ ( <column_or_user_var> { , <column_or_user_var> } ) ]
    [ SET <assignment_list> ]

<export_options> ::=
    [ { FIELDS | COLUMNS }
        [ TERMINATED BY '<string>' ]
        [ [ OPTIONALLY ] ENCLOSED BY '<char>' ]
        [ ESCAPED BY '<char>' ] ]
    [ LINES
        [ STARTING BY '<string>' ]
        [ TERMINATED BY '<string>' ] ]
```

### 17. Account Management

```ebnf
<create_user> ::=
    CREATE USER [ IF NOT EXISTS ]
    <user_specification> { , <user_specification> }
    [ REQUIRE { NONE | <tls_option> [ AND <tls_option> ] } ]
    [ WITH <resource_option> { <resource_option> } ]
    [ <password_option> { <password_option> } ]
    [ COMMENT '<string>' ]  /*MySQL 8.0.21+*/
    [ ATTRIBUTE '<json>' ]  /*MySQL 8.0.21+*/

<user_specification> ::=
    <user> [ <auth_option> ]

<auth_option> ::=
    IDENTIFIED BY '<password>'
  | IDENTIFIED BY RANDOM PASSWORD  /*MySQL 8.0.18+*/
  | IDENTIFIED WITH <auth_plugin> [ BY '<password>' | AS '<hash>' | INITIAL AUTHENTICATION <auth_string> ]
  | IDENTIFIED VIA <auth_plugin> [ USING '<string>' | AS '<hash>' ] [ OR <auth_plugin> ... ]  /*MariaDB*/

<grant_statement> ::=
    GRANT <privilege_list> ON <privilege_level> TO <user_list>
    [ REQUIRE { NONE | <tls_option> [ AND <tls_option> ] } ]
    [ WITH { GRANT OPTION | <resource_option> { <resource_option> } } ]

<privilege> ::=
    ALL [ PRIVILEGES ]
  | ALTER [ ROUTINE ]
  | CREATE [ ROUTINE | TEMPORARY TABLES | VIEW ]
  | DELETE | DROP | EVENT | EXECUTE | FILE | GRANT OPTION
  | INDEX | INSERT | LOCK TABLES | PROCESS | PROXY
  | REFERENCES | RELOAD | REPLICATION { CLIENT | SLAVE }
  | SELECT | SHOW DATABASES | SHOW VIEW | SHUTDOWN
  | SUPER | TRIGGER | UPDATE | USAGE
  | APPLICATION_PASSWORD_ADMIN  /*MySQL 8.0+*/
  | AUDIT_ADMIN  /*MySQL 8.0+*/
  | BACKUP_ADMIN  /*MySQL 8.0+*/
  | BINLOG_ADMIN  /*MySQL 8.0+*/
  | BINLOG_ENCRYPTION_ADMIN  /*MySQL 8.0+*/
  | CLONE_ADMIN  /*MySQL 8.0+*/
  | CONNECTION_ADMIN  /*MySQL 8.0+*/
  | ENCRYPTION_KEY_ADMIN  /*MySQL 8.0+*/
  | FLUSH_OPTIMIZER_COSTS  /*MySQL 8.0+*/
  | FLUSH_STATUS  /*MySQL 8.0+*/
  | FLUSH_TABLES  /*MySQL 8.0+*/
  | FLUSH_USER_RESOURCES  /*MySQL 8.0+*/
  | GROUP_REPLICATION_ADMIN  /*MySQL 8.0+*/
  | INNODB_REDO_LOG_ARCHIVE  /*MySQL 8.0+*/
  | INNODB_REDO_LOG_ENABLE  /*MySQL 8.0+*/
  | PASSWORDLESS_USER_ADMIN  /*MySQL 8.0+*/
  | PERSIST_RO_VARIABLES_ADMIN  /*MySQL 8.0+*/
  | REPLICATION_APPLIER  /*MySQL 8.0+*/
  | REPLICATION_SLAVE_ADMIN  /*MySQL 8.0+*/
  | RESOURCE_GROUP_ADMIN  /*MySQL 8.0+*/
  | RESOURCE_GROUP_USER  /*MySQL 8.0+*/
  | ROLE_ADMIN  /*MySQL 8.0+*/
  | SESSION_VARIABLES_ADMIN  /*MySQL 8.0+*/
  | SET_USER_ID  /*MySQL 8.0+*/
  | SHOW_ROUTINE  /*MySQL 8.0+*/
  | SYSTEM_USER  /*MySQL 8.0+*/
  | SYSTEM_VARIABLES_ADMIN  /*MySQL 8.0+*/
  | TABLE_ENCRYPTION_ADMIN  /*MySQL 8.0+*/
  | VERSION_TOKEN_ADMIN  /*MySQL 8.0+*/
  | XA_RECOVER_ADMIN  /*MySQL 8.0+*/

<privilege_level> ::=
    *
  | *.*
  | <db_name>.*
  | <db_name>.<table_name>
  | <table_name>
  | <db_name>.<routine_name>
```

### 18. Roles (MySQL 8.0+, MariaDB 10.0+)

```ebnf
<create_role> ::=
    CREATE ROLE [ IF NOT EXISTS ] <role> { , <role> }

<grant_role> ::=
    GRANT <role> { , <role> } TO <user> { , <user> }
    [ WITH ADMIN OPTION ]

<set_role> ::=
    SET ROLE { DEFAULT | NONE | ALL | ALL EXCEPT <role> { , <role> } | <role> { , <role> } }

<set_default_role> ::=
    SET DEFAULT ROLE { NONE | ALL | <role> { , <role> } } TO <user> { , <user> }
```

### 19. Resource Groups (MySQL 8.0+)

```ebnf
<create_resource_group> ::=
    CREATE RESOURCE GROUP <group_name>
    TYPE = { SYSTEM | USER }
    [ VCPU [=] <vcpu_spec> { , <vcpu_spec> } ]
    [ THREAD_PRIORITY [=] <priority> ]
    [ ENABLE | DISABLE ]

<alter_resource_group> ::=
    ALTER RESOURCE GROUP <group_name>
    [ VCPU [=] <vcpu_spec> { , <vcpu_spec> } ]
    [ THREAD_PRIORITY [=] <priority> ]
    [ ENABLE | DISABLE ]
    [ FORCE ]

<set_resource_group> ::=
    SET RESOURCE GROUP <group_name>
    [ FOR <thread_id> { , <thread_id> } ]
```

### 20. System Versioning (MariaDB 10.3+)

```ebnf
<system_versioning> ::=
    WITH SYSTEM VERSIONING

<system_time_column> ::=
    <row_start_column> <timestamp_type> GENERATED ALWAYS AS ROW START [ INVISIBLE ]
  | <row_end_column> <timestamp_type> GENERATED ALWAYS AS ROW END [ INVISIBLE ]
  | PERIOD FOR SYSTEM_TIME ( <row_start_column> , <row_end_column> )

<system_versioned_query> ::=
    SELECT ... FROM <table_name>
    FOR SYSTEM_TIME { AS OF <point_in_time>
                    | BETWEEN <point_in_time> AND <point_in_time>
                    | FROM <point_in_time> TO <point_in_time>
                    | ALL }

<system_versioning_alter> ::=
    ALTER TABLE <table_name> ADD SYSTEM VERSIONING
  | ALTER TABLE <table_name> DROP SYSTEM VERSIONING
```

### 21. Sequences (MariaDB 10.3+)

```ebnf
<create_sequence> ::=
    CREATE [ OR REPLACE ] [ TEMPORARY ] SEQUENCE [ IF NOT EXISTS ] <sequence_name>
    [ INCREMENT [ BY ] <increment> ]
    [ MINVALUE <minvalue> | NO MINVALUE | NOMINVALUE ]
    [ MAXVALUE <maxvalue> | NO MAXVALUE | NOMAXVALUE ]
    [ START [ WITH ] <start> ]
    [ CACHE <cache> | NOCACHE ]
    [ CYCLE | NOCYCLE ]
    [ ENGINE = <engine_name> ]

<sequence_operation> ::=
    NEXT VALUE FOR <sequence_name>
  | NEXTVAL( <sequence_name> )
  | PREVIOUS VALUE FOR <sequence_name>
  | LASTVAL( <sequence_name> )
  | SETVAL( <sequence_name> , <value> [ , <is_used> ] )
```

### 22. CHECK Constraints (MySQL 8.0.16+, MariaDB 10.2+)

```ebnf
<check_constraint> ::=
    [ CONSTRAINT [ <symbol> ] ] CHECK ( <expression> ) [ [ NOT ] ENFORCED ]
```

### 23. Lateral Derived Tables (MySQL 8.0.14+)

```ebnf
<lateral_derived_table> ::=
    LATERAL ( <select_statement> ) [ AS ] <alias> [ ( <column_list> ) ]
```

### 24. VALUES Statement (MySQL 8.0.19+)

```ebnf
<values_statement> ::=
    VALUES <row_constructor_list> [ ORDER BY <order_list> ] [ LIMIT <limit> ]

<row_constructor> ::=
    ROW ( <value_list> )
```

### 25. EXPLAIN Extensions

```ebnf
<explain_statement> ::=
    { EXPLAIN | DESCRIBE | DESC }
    [ EXTENDED | PARTITIONS | FORMAT = { TRADITIONAL | JSON | TREE } | ANALYZE ]  /*ANALYZE: MySQL 8.0.18+*/
    { <select_statement>
    | <delete_statement>
    | <insert_statement>
    | <replace_statement>
    | <update_statement>
    | FOR CONNECTION <connection_id> }
  | { EXPLAIN | DESCRIBE | DESC } [ <table_name> [ <column_name> | <wild> ] ]
```

### 26. ANALYZE TABLE Extensions

```ebnf
<analyze_table> ::=
    ANALYZE [ NO_WRITE_TO_BINLOG | LOCAL ]
    TABLE <table_list>
    [ UPDATE HISTOGRAM ON <column_list> [ WITH <buckets> BUCKETS ] ]  /*MySQL 8.0+*/
    [ DROP HISTOGRAM ON <column_list> ]  /*MySQL 8.0+*/
```

### 27. MySQL-Specific Operators

```ebnf
<mysql_operator> ::=
    -- Arithmetic
    + | - | * | / | DIV | % | MOD

    -- Comparison
    = | <=> | != | <> | < | <= | > | >= | IS | IS NOT

    -- Logical
    AND | && | OR | || | XOR | NOT | !

    -- Bitwise
    & | | | ^ | ~ | << | >>

    -- Assignment
    := | =

    -- Member of (MySQL 8.0.17+)
    MEMBER OF( <json_array> )

    -- RegExp
    REGEXP | RLIKE | NOT REGEXP | NOT RLIKE

    -- JSON (MySQL 5.7+)
    -> | ->>
```

### 28. Comments and Hints

```ebnf
<comment> ::=
    -- <comment_text>
  | # <comment_text>
  | /* <comment_text> */
  | /*! <mysql_specific_code> */      -- MySQL-specific
  | /*!50701 <version_specific> */    -- Version-specific

<optimizer_hint> ::=
    /*+ <hint_list> */

<hint> ::=
    BKA( <table_list> ) | NO_BKA( <table_list> )
  | BNL( <table_list> ) | NO_BNL( <table_list> )
  | HASH_JOIN( <table_list> ) | NO_HASH_JOIN( <table_list> )  /*MySQL 8.0.18+*/
  | MERGE( <table_list> ) | NO_MERGE( <table_list> )
  | INDEX( <table_name> <index_list> ) | NO_INDEX( <table_name> <index_list> )
  | JOIN_ORDER( <table_list> ) | JOIN_PREFIX( <table_list> ) | JOIN_SUFFIX( <table_list> )
  | SEMIJOIN( <strategy_list> ) | NO_SEMIJOIN( <strategy_list> )
  | MAX_EXECUTION_TIME( <milliseconds> )
  | SET_VAR( <var_assignment> )
  | RESOURCE_GROUP( <group_name> )  /*MySQL 8.0+*/
```

### 29. MariaDB-Specific Features

#### MariaDB Window Functions (10.2+)

```ebnf
<mariadb_window_function> ::=
    MEDIAN( <expression> ) OVER <window_spec>  /*MariaDB 10.3.3+*/
  | PERCENTILE_CONT( <fraction> ) WITHIN GROUP ( ORDER BY <expression> ) OVER <window_spec>
  | PERCENTILE_DISC( <fraction> ) WITHIN GROUP ( ORDER BY <expression> ) OVER <window_spec>
```

#### MariaDB JSON Functions

```ebnf
<mariadb_json_function> ::=
    JSON_COMPACT( <json_doc> )
  | JSON_DETAILED( <json_doc> [ , <tab_size> ] )
  | JSON_EQUALS( <json_doc> , <json_doc> )  /*MariaDB 10.7+*/
  | JSON_NORMALIZE( <json_doc> )  /*MariaDB 10.7+*/
  | JSON_OVERLAPS( <json_doc> , <json_doc> )  /*MariaDB 10.9+*/
```

#### MariaDB Application Time Periods (10.4+)

```ebnf
<application_time_period> ::=
    PERIOD FOR <period_name> ( <start_column> , <end_column> )

<without_overlaps> ::=
    WITHOUT OVERLAPS ( <column_list> )
```

## Usage Notes

1. **Version Compatibility**: Always check MySQL/MariaDB version for feature availability
2. **Storage Engines**: Different storage engines support different features
3. **Character Sets**: MySQL/MariaDB have extensive charset/collation support
4. **SQL Modes**: SQL behavior can change based on sql_mode setting
5. **Case Sensitivity**: Table names may be case-sensitive depending on OS
6. **Identifier Quotes**: Use backticks ` for identifiers, single quotes ' for strings
7. **Optimizer Hints**: Available in MySQL 5.7.7+ and MariaDB 10.0+
8. **System Variables**: Extensive use of @@ and @ prefixed variables

This grammar provides a comprehensive foundation for implementing MySQL/MariaDB SQL parsers, covering dialect-specific extensions and variations between MySQL and MariaDB.